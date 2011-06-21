/*
 *      Copyright (C) 2011 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "system.h"

#if defined (HAVE_LIBGSTREAMER)
#include "GSTPlayer.h"
#include "Application.h"
#include "FileItem.h"
#include "cores/VideoRenderers/RenderManager.h"
#include "filesystem/SpecialProtocol.h"
#include "guilib/GUIWindowManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/GUISettings.h"
#include "settings/Settings.h"
#include "threads/SingleLock.h"
#include "windowing/WindowingFactory.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"

#include <gst/gst.h>
#include <gst/video/video.h>

struct INT_GST_VARS
{
  bool        inited;
  GMainLoop   *loop;
  GstElement  *player;
  GstElement  *videosink;
  GstElement  *audiosink;
  bool        video_uses_ismd;
  bool        audio_uses_ismd;
};

void stream_foreach(gpointer data, gpointer user_data)
{
  gint streamtype;
  g_object_get(G_OBJECT(data), "type", &streamtype, NULL);
  if  (streamtype == 2)
  {
    gint width = 0;
    gint height = 0;
    GstCaps *gcaps;
    const GstStructure *gstr;

    g_object_get(G_OBJECT(data), "caps", &gcaps, NULL);
    if  (gcaps != NULL)
    {
      gstr = gst_caps_get_structure(gcaps, 0);
      gst_structure_get_int(gstr, "width", &width);
      gst_structure_get_int(gstr, "height", &height);
    }
  }
  g_object_unref(data);
}

gboolean CGSTPlayerBusCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
  CGSTPlayer    *player  = (CGSTPlayer*)data;
  INT_GST_VARS  *gstvars = player->GetGSTVars();
  gchar  *str     = NULL;
  GError *info    = NULL;
  GError *error   = NULL;
  GError *warning = NULL;

  switch (GST_MESSAGE_TYPE(msg))
  {
    case GST_MESSAGE_EOS:
      g_print ("GStreamer: End of stream\n");
      gst_element_set_state(gstvars->player, GST_STATE_READY);
      g_main_loop_quit(gstvars->loop);
      break;

    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &error, &str);
      g_free(str);
      if (error)
      {
        g_printerr("GStreamer: Error - %s %s\n", str, error->message);
        g_error_free(error);
      }
      g_main_loop_quit(gstvars->loop);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_error(msg, &warning, &str);
      g_free(str);
      if (warning)
      {
        g_printerr("GStreamer: Warning - %s %s\n", str, warning->message);
        g_error_free(warning);
      }
      break;

    case GST_MESSAGE_INFO:
      gst_message_parse_error(msg, &info, &str);
      g_free(str);
      if (info)
      {
        g_printerr("GStreamer: Info - %s %s\n", str, info->message);
        g_error_free(info);
      }
      break;

    case GST_MESSAGE_TAG:
      printf("GStreamer: Message TAG\n");
      break;
    case GST_MESSAGE_BUFFERING:
      printf("GStreamer: Message BUFFERING\n");
      break;
    case GST_MESSAGE_STATE_CHANGED:
      printf("GStreamer: Message STATE_CHANGED\n");
      GstState old_state, new_state;

      gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
      printf("GStreamer: Element %s changed state from %s to %s.\n",
          GST_OBJECT_NAME (msg->src),
          gst_element_state_get_name(old_state),
          gst_element_state_get_name(new_state));
      /*
      if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED)
      {
        // Get stream info
        GList *streaminfo = NULL;
        g_object_get(gstvars->player, "stream-info", &streaminfo, NULL);
        g_list_foreach(streaminfo, stream_foreach, NULL);
        g_list_free(streaminfo);

        GstPad* pad = gst_element_get_static_pad(gstvars->videosink, "sink");

        gint width, height;
        gst_video_get_size(pad, &width, &height);

        const GValue *framerate;
        framerate = gst_video_frame_rate(pad);
        if (framerate && GST_VALUE_HOLDS_FRACTION(framerate))
        {
          int fps_n = gst_value_get_fraction_numerator(framerate);
          int fps_d = gst_value_get_fraction_denominator(framerate);
          float fFramerate = (float)fps_n/(float)fps_d;
        }
        gst_object_unref(GST_OBJECT(pad));
      }
      */
     break;
    case GST_MESSAGE_STATE_DIRTY:
      printf("GStreamer: Message STATE_DIRTY\n");
      break;
    case GST_MESSAGE_STEP_DONE:
      printf("GStreamer: Message STEP_DONE\n");
      break;
    case GST_MESSAGE_CLOCK_PROVIDE:
      printf("GStreamer: Message CLOCK_PROVIDE\n");
      break;
    case GST_MESSAGE_CLOCK_LOST:
      printf("GStreamer: Message CLOCK_LOST\n");
      break;
    case GST_MESSAGE_NEW_CLOCK:
      printf("GStreamer: Message NEW_CLOCK\n");
      break;
    case GST_MESSAGE_STRUCTURE_CHANGE:
      printf("GStreamer: Message STRUCTURE_CHANGE\n");
      break;
    case GST_MESSAGE_STREAM_STATUS:
      printf("GStreamer: Message STREAM_STATUS\n");
      break;
    case GST_MESSAGE_APPLICATION:
      printf("GStreamer: Message APPLICATION\n");
      break;
    case GST_MESSAGE_ELEMENT:
      if (gst_structure_has_name(msg->structure, "have-ns-view"))
      {
        //NSView *nsview = NULL;
        void *nsview = NULL;
        nsview = g_value_get_pointer(gst_structure_get_value(msg->structure, "nsview"));
        // passes a GstGLView object to the callback
        // object is an NSView
        return GST_BUS_PASS;
      }
      printf("GStreamer: Message ELEMENT\n");
      break;
    case GST_MESSAGE_SEGMENT_START:
      printf("GStreamer: Message SEGMENT_START\n");
      break;
    case GST_MESSAGE_SEGMENT_DONE:
      printf("GStreamer: Message SEGMENT_DONE\n");
      break;
    case GST_MESSAGE_DURATION:
      printf("GStreamer: Message DURATION\n");
      break;
    case GST_MESSAGE_LATENCY:
      printf("GStreamer: Message LATENCY\n");
      break;
    case GST_MESSAGE_ASYNC_START:
      printf("GStreamer: Message ASYNC_START\n");
      break;
    case GST_MESSAGE_ASYNC_DONE:
      printf("GStreamer: Message ASYNC_DONE\n");
      break;
#if (GST_VERSION_MICRO >= 26)
    case GST_MESSAGE_REQUEST_STATE:
      printf("GStreamer: Message REQUEST_STATE\n");
      break;
    case GST_MESSAGE_STEP_START:
      printf("GStreamer: Message STEP_START\n");
      break;
#endif
#if (GST_VERSION_MICRO >= 29)
    case GST_MESSAGE_QOS:
      printf("GStreamer: Message QOS\n");
      break;
#endif
    default:
      printf("GStreamer: Unknown message %i\n", GST_MESSAGE_TYPE(msg));
      break;
  }

  return true;
}

CGSTPlayer::CGSTPlayer(IPlayerCallback &callback) 
  : IPlayer(callback),
  CThread(),
  m_ready(true)
{
  m_speed = 1;
  m_paused = false;
  m_StopPlaying = false;

  m_gstvars = (INT_GST_VARS*)new INT_GST_VARS;
  m_gstvars->inited = false;
  m_gstvars->loop   = NULL;
  m_gstvars->player = NULL;
  m_gstvars->videosink = NULL;
  m_gstvars->audiosink = NULL;
  m_gstvars->video_uses_ismd = false;
  m_gstvars->audio_uses_ismd = false;

  gst_init(NULL, NULL);
}

CGSTPlayer::~CGSTPlayer()
{
  CloseFile();

  if (m_gstvars)
  {
    delete m_gstvars;
    m_gstvars = NULL;
  }
}

bool CGSTPlayer::Initialize(TiXmlElement* pConfig)
{
#if defined(__APPLE__)
  m_audiosink = "osxaudiosink";
  m_videosink = "osxvideosink";
#else
  m_audiosink = "ismd_audio_sink";
  m_videosink = "ismd_vidrend_bin";
#endif
  if (pConfig)
  {
    // New config specific to gstplayer
    XMLUtils::GetString(pConfig, "audiosink", m_audiosink);
    XMLUtils::GetString(pConfig, "videosink", m_videosink);
  }
  CLog::Log(LOGNOTICE, "gstplayer : audiosink (%s)", m_audiosink.c_str());
  CLog::Log(LOGNOTICE, "gstplayer : videosink (%s)", m_videosink.c_str());

  return true;
}

bool CGSTPlayer::OpenFile(const CFileItem &file, const CPlayerOptions &options)
{
  try
  {
    CLog::Log(LOGNOTICE, "CGSTPlayer: Opening: %s", file.m_strPath.c_str());
    // if playing a file close it first
    // this has to be changed so we won't have to close it.
    if(ThreadHandle())
      CloseFile();

    m_item = file;
    m_options = options;

    if (m_item.m_strPath.Left(6).Equals("udp://"))
    {
      // protocol goes to gstreamer as is
      m_url = m_item.m_strPath;
    }
    else if (m_item.m_strPath.Left(7).Equals("rtsp://"))
    {
      // protocol goes to gstreamer as is
      m_url = m_item.m_strPath;
    }
    else if (m_item.m_strPath.Left(7).Equals("http://"))
    {
      // strip user agent that we append
      m_url = m_item.m_strPath;
      m_url = m_url.erase(m_url.rfind('|'), m_url.size());
    }
    /*
    else if (m_item.m_strPath.Left(10).Equals("musicdb://"))
    {
      m_url = "file://";
      m_url.append(CFileMusicDatabase::TranslateUrl(m_item));
    }
    */
    if (m_item.m_strPath.Left(6).Equals("smb://"))
    {
      // can't handle this yet.
      m_url = m_item.m_strPath;
    }
    else
    {
      m_url = "file://";
      m_url.append(m_item.m_strPath.c_str());
    }
    CLog::Log(LOGNOTICE, "CGSTPlayer: Opening: URL=%s", m_url.c_str());
    if (!gst_uri_is_valid(m_url.c_str()))
      return false;

    m_elapsed_ms  =  0;
    m_duration_ms =  0;
    m_audio_index = -1;
    m_audio_count =  0;
    m_audio_info  = "none";
    m_video_index = -1;
    m_video_count =  0;
    m_video_info  = "none";
    m_subtitle_index = -1;
    m_subtitle_count =  0;
    m_chapter_count  =  0;

    m_video_fps      =  0.0;
    m_video_width    =  0;
    m_video_height   =  0;

    m_StopPlaying = false;

    m_gstvars->player = gst_element_factory_make( "playbin2", "player");
    gst_element_set_state(m_gstvars->player, GST_STATE_NULL);
    //
    m_gstvars->loop = g_main_loop_new(NULL, FALSE);
    if (m_gstvars->loop == NULL)
      return false;
    //
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_gstvars->player));
    gst_bus_add_watch(bus, (GstBusFunc)CGSTPlayerBusCallback, this);
    gst_object_unref(bus);
    
    // create video sink
    m_gstvars->videosink = gst_element_factory_make(m_videosink.c_str(), NULL);
    if (m_gstvars->videosink)
    {
      m_gstvars->video_uses_ismd = true;
    }
    else
    {
      CLog::Log(LOGDEBUG, "CGSTPlayer::OpenFile: using default autovideosink");
      m_gstvars->video_uses_ismd = false;
      m_gstvars->videosink = gst_element_factory_make("autovideosink", NULL);
      if (m_gstvars->videosink)
      {
        #if defined(__APPLE__)
        // When the NSView to be embedded is created an element #GstMessage with a
        //  name of 'have-ns-view' will be created and posted on the bus.
        //  The pointer to the NSView to embed will be in the 'nsview' field of that
        //  message. The application MUST handle this message and embed the view
        //  appropriately.
        g_object_set(m_gstvars->videosink, "embed", true, NULL);
        #endif
        g_object_set(m_gstvars->videosink, "message-forward", true, NULL);
      }
    }
    g_object_set(m_gstvars->player, "video-sink", m_gstvars->videosink, NULL);
    
    // create audio sink
    m_gstvars->audiosink = gst_element_factory_make(m_audiosink.c_str(), NULL);
    if (m_gstvars->audiosink)
    {
      m_gstvars->video_uses_ismd = true;
    }
    else
    {
      CLog::Log(LOGDEBUG, "CGSTPlayer::OpenFile: using default autoaudiosink");
      m_gstvars->audio_uses_ismd = false;
      m_gstvars->audiosink = gst_element_factory_make("autoaudiosink", NULL);
      if (m_gstvars->audiosink)
        g_object_set(m_gstvars->audiosink, "message-forward", true, NULL);
    }
    g_object_set(m_gstvars->player, "audio-sink", m_gstvars->audiosink, NULL);


    // set the player url and change state to paused (from null)
    g_object_set(m_gstvars->player, "uri", m_url.c_str(), NULL);
    gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
    m_gstvars->inited = true;

    m_ready.Reset();
    Create();
    if (!m_ready.WaitMSec(100))
    {
      CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
      dialog->Show();
      while(!m_ready.WaitMSec(1))
        g_windowManager.Process(false);
      dialog->Close();
    }
    // just in case process thread throws.
    m_ready.Set();

    // Playback might have been stopped due to some error
    if (m_bStop || m_StopPlaying)
      return false;

    return true;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "%s - Exception thrown on open", __FUNCTION__);
    return false;
  }
}

bool CGSTPlayer::CloseFile()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::CloseFile");

  if (m_gstvars->inited)
  {
    gst_element_set_state(m_gstvars->player, GST_STATE_READY);
    g_main_loop_quit(m_gstvars->loop);
    m_gstvars->inited = false;
  }

  m_bStop = true;
  m_StopPlaying = true;

  CLog::Log(LOGDEBUG, "CGSTPlayer: waiting for threads to exit");
  // wait for the main thread to finish up
  // since this main thread cleans up all other resources and threads
  // we are done after the StopThread call
  StopThread();

  CLog::Log(LOGDEBUG, "CGSTPlayer: finished waiting");
  g_renderManager.UnInit();

  return true;
}

bool CGSTPlayer::IsPlaying() const
{
  return !m_bStop;
}

void CGSTPlayer::Pause()
{
  CSingleLock lock(m_gst_csection);

  if (!m_StopPlaying)
    return;

  if (m_paused == true)
  {
    if (m_gstvars->inited)
      gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
    m_callback.OnPlayBackResumed();
  }
  else
  {
    if (m_gstvars->inited)
      gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
    m_callback.OnPlayBackPaused();
  }
  m_paused = !m_paused;
}

bool CGSTPlayer::IsPaused() const
{
  return m_paused;
}

bool CGSTPlayer::HasVideo() const
{
  return true;
}

bool CGSTPlayer::HasAudio() const
{
  return true;
}

void CGSTPlayer::ToggleFrameDrop()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::ToggleFrameDrop");
}

bool CGSTPlayer::CanSeek()
{
  return GetTotalTime() > 0;
}

void CGSTPlayer::Seek(bool bPlus, bool bLargeStep)
{
  int chapter_index = GetChapter();
  if (bLargeStep)
  {
    // seek to next chapter
    if (bPlus && chapter_index < m_chapter_count)
    {
      SeekChapter(chapter_index + 1);
      return;
    }
    // seek to previous chapter
    if (!bPlus && chapter_index > 1)
    {
      SeekChapter(chapter_index - 1);
      return;
    }
  }

  // force updated to m_duration_ms.
  GetTotalTime();

  int64_t seek_ms;
  if (g_advancedSettings.m_videoUseTimeSeeking &&
    (GetTotalTime() > (2 * g_advancedSettings.m_videoTimeSeekForwardBig)))
  {
    if (bLargeStep)
      seek_ms = bPlus ? g_advancedSettings.m_videoTimeSeekForwardBig : g_advancedSettings.m_videoTimeSeekBackwardBig;
    else
      seek_ms = bPlus ? g_advancedSettings.m_videoTimeSeekForward    : g_advancedSettings.m_videoTimeSeekBackward;
    // convert to milliseconds
    seek_ms *= 1000;
    seek_ms += m_elapsed_ms;
  }
  else
  {
    float percent;
    if (bLargeStep)
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForwardBig : g_advancedSettings.m_videoPercentSeekBackwardBig;
    else
      percent = bPlus ? g_advancedSettings.m_videoPercentSeekForward    : g_advancedSettings.m_videoPercentSeekBackward;
    percent /= 100.0f;
    percent += (float)m_elapsed_ms/(float)m_duration_ms;
    // convert to milliseconds
    seek_ms = m_duration_ms * percent;
  }

  // handle stacked videos, dvdplayer does it so we do it too.
  if (g_application.CurrentFileItem().IsStack() &&
    (seek_ms > m_duration_ms || seek_ms < 0))
  {
    CLog::Log(LOGDEBUG, "CGSTPlayer::Seek: In mystery code, what did I do");
    g_application.SeekTime((seek_ms - m_elapsed_ms) * 0.001 + g_application.GetTime());
    // warning, don't access any object variables here as
    // the object may have been destroyed
    return;
  }

  if (seek_ms > m_duration_ms)
    seek_ms = m_duration_ms;

  SeekTime(seek_ms);
}

bool CGSTPlayer::SeekScene(bool bPlus)
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::SeekScene");
  return false;
}

void CGSTPlayer::SeekPercentage(float fPercent)
{
  CSingleLock lock(m_gst_csection);
}

float CGSTPlayer::GetPercentage()
{
  GetTotalTime();
  if (m_duration_ms)
    return 100.0f * (float)m_elapsed_ms/(float)m_duration_ms;
  else
    return 0.0f;
}

void CGSTPlayer::SetVolume(long nVolume)
{
  // nVolume is a milliBels from -6000 (-60dB or mute) to 0 (0dB or full volume)
  CSingleLock lock(m_gst_csection);

  float volume;
  if (nVolume == -6000) {
    // We are muted
    volume = 0.0f;
  } else {
    volume = (double)nVolume / -10000.0f;
    // Convert what XBMC gives into 0.0 -> 1.0 scale playbin2 uses
    volume = ((1 - volume) - 0.4f) * 1.6666f;
  }
  g_object_set(m_gstvars->player, "volume", &volume, NULL);
}

void CGSTPlayer::GetAudioInfo(CStdString &strAudioInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetAudioInfo");
  strAudioInfo = "CGSTPlayer:GetAudioInfo";
}

void CGSTPlayer::GetVideoInfo(CStdString &strVideoInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetVideoInfo");
  strVideoInfo = "CGSTPlayer:GetVideoInfo";
}

void CGSTPlayer::GetGeneralInfo(CStdString& strGeneralInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetGeneralInfo");
  strGeneralInfo = "CGSTPlayer:GetGeneralInfo";
}

int CGSTPlayer::GetAudioStreamCount()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetAudioStreamCount");
  if (m_gstvars->inited)
  {
    gint audio_index = 0;
    gint audio_count = 0;
    g_object_get(m_gstvars->player, "n-audio", &audio_count, NULL);
    if (audio_count)
      g_object_get(m_gstvars->player, "current-audio", audio_index, NULL);
    m_audio_index = audio_index;
    m_audio_count = audio_count;
  }
  return m_audio_count;
}

int CGSTPlayer::GetAudioStream()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetAudioStream");

	return m_audio_index;
}

void CGSTPlayer::GetAudioStreamName(int iStream, CStdString &strStreamName)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetAudioStreamName");
  CSingleLock lock(m_gst_csection);
}
 
void CGSTPlayer::SetAudioStream(int SetAudioStream)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::SetAudioStream");
  CSingleLock lock(m_gst_csection);
}

int CGSTPlayer::GetSubtitleCount()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetSubtitleCount");
  if (m_gstvars->inited)
  {
    g_object_get(m_gstvars->player, "n-text", &m_subtitle_count, NULL);
    if (m_subtitle_count)
      g_object_get(m_gstvars->player, "current-text", m_subtitle_index, NULL);
  }
	return m_subtitle_count;
}

int CGSTPlayer::GetSubtitle()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetSubtitle");

	return m_subtitle_index;
}

void CGSTPlayer::GetSubtitleName(int iStream, CStdString &strStreamName)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetSubtitleName");
  CSingleLock lock(m_gst_csection);
}

void CGSTPlayer::SetSubtitle(int iStream)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::SetSubtitle");
  CSingleLock lock(m_gst_csection);
}

bool CGSTPlayer::GetSubtitleVisible()
{
  return m_subtitle_show;
}

void CGSTPlayer::SetSubtitleVisible(bool bVisible)
{
  m_subtitle_show = bVisible;
  g_settings.m_currentVideoSettings.m_SubtitleOn = bVisible;

  CSingleLock lock(m_gst_csection);
}

int CGSTPlayer::AddSubtitle(const CStdString& strSubPath)
{
  // not sure we can add a subtitle file on the fly.
  return -1;
}

void CGSTPlayer::Update(bool bPauseDrawing)
{
  g_renderManager.Update(bPauseDrawing);
}

void CGSTPlayer::GetVideoRect(CRect& SrcRect, CRect& DestRect)
{
  g_renderManager.GetVideoRect(SrcRect, DestRect);
}

void CGSTPlayer::GetVideoAspectRatio(float &fAR)
{
  fAR = g_renderManager.GetAspectRatio();
}

int CGSTPlayer::GetChapterCount()
{
  // check for avi/mkv chapters.
  if (m_chapter_count == 0)
  {
  }
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetChapterCount:m_chapter_count(%d)", m_chapter_count);
  return m_chapter_count;
}

int CGSTPlayer::GetChapter()
{
  // returns a one based value.
  // if we have a chapter list, we need to figure out which chapter we are in.
  int chapter_index = 0;

  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetChapter:chapter_index(%d)", chapter_index);
  return chapter_index + 1;
}

void CGSTPlayer::GetChapterName(CStdString& strChapterName)
{
  if (m_chapter_count)
    strChapterName = m_chapters[GetChapter() - 1].name;
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetChapterName:strChapterName(%s)", strChapterName.c_str());
}

int CGSTPlayer::SeekChapter(int chapter_index)
{
  // chapter_index is a one based value.
  CLog::Log(LOGDEBUG, "CGSTPlayer::SeekChapter:chapter_index(%d)", chapter_index);
  /*
  {
    if (chapter_index < 0)
      chapter_index = 0;
    if (chapter_index > m_chapter_count)
      return 0;

    // Seek to the chapter.
    SeekTime(m_chapters[chapter_index - 1].seekto_ms);
  }
  else
  {
    // we do not have a chapter list so do a regular big jump.
    if (chapter_index > 0)
      Seek(true,  true);
    else
      Seek(false, true);
  }
  */
  return 0;
}

float CGSTPlayer::GetActualFPS()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetActualFPS:m_video_fps(%f)", m_video_fps);
  return m_video_fps;
}

void CGSTPlayer::SeekTime(__int64 seek_ms)
{
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->inited)
  {
    // gst time units are nanoseconds.
    gint64 seek_ns = seek_ms * GST_MSECOND;
    gst_element_seek_simple(m_gstvars->player,
      GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, seek_ns);
  }
  m_callback.OnPlayBackSeek((int)seek_ms, (int)(seek_ms - m_elapsed_ms));
}

__int64 CGSTPlayer::GetTime()
{
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->inited)
  {
    gint64 elapsed_ns = 0;
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_position(m_gstvars->player, &fmt, &elapsed_ns))
      m_elapsed_ms = elapsed_ns / GST_MSECOND;
  }
  return m_elapsed_ms;
}

int CGSTPlayer::GetTotalTime()
{
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->inited)
  {
    gint64 duration_ns = 0;
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_duration(m_gstvars->player, &fmt, &duration_ns))
      m_duration_ms = duration_ns / GST_MSECOND;
  }
	return m_duration_ms / 1000;
}

int CGSTPlayer::GetAudioBitrate()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetAudioBitrate");
  return 0;
}
int CGSTPlayer::GetVideoBitrate()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetVideoBitrate");
  return 0;
}

int CGSTPlayer::GetSourceBitrate()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetSourceBitrate");
  return 0;
}

int CGSTPlayer::GetChannels()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetActualFPS");
  // returns number of audio channels (ie 5.1 = 6)

  return m_audio_channels;
}

int CGSTPlayer::GetBitsPerSample()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetBitsPerSample");
  return 0;
}

int CGSTPlayer::GetSampleRate()
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetSampleRate");
  return 0;
}

CStdString CGSTPlayer::GetAudioCodecName()
{
  CStdString strAudioCodec;

  return strAudioCodec;
}

CStdString CGSTPlayer::GetVideoCodecName()
{
  CStdString strVideoCodec;

  return strVideoCodec;
}

int CGSTPlayer::GetPictureWidth()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetPictureWidth");
  if (m_gstvars->inited)
  {
    GstPad *pad = gst_element_get_static_pad(m_gstvars->videosink, "src");
    GstCaps *caps = gst_pad_get_negotiated_caps(pad);
    if (caps)
    {
      int width, height;
      const GstStructure *structure = gst_caps_get_structure(caps, 0);
      gst_structure_get_int(structure, "width",  &width);
      gst_structure_get_int(structure, "height", &height);
      m_video_width = width;
    }
  }
  return m_video_width;
}

int CGSTPlayer::GetPictureHeight()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetPictureHeight");
  if (m_gstvars->inited)
  {
    GstPad *pad = gst_element_get_static_pad(m_gstvars->videosink, "src");
    GstCaps *caps = gst_pad_get_negotiated_caps(pad);
    if (caps)
    {
      int width, height;
      const GstStructure *structure = gst_caps_get_structure(caps, 0);
      gst_structure_get_int(structure, "width",  &width);
      gst_structure_get_int(structure, "height", &height);
      m_video_height = height;
    }
  }
  return m_video_height;
}

bool CGSTPlayer::GetStreamDetails(CStreamDetails &details)
{
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetStreamDetails");
  return false;
}

void CGSTPlayer::ToFFRW(int iSpeed)
{
  if (m_StopPlaying)
    return;

  if (m_speed != iSpeed)
  {
    // recover power of two value
    int ipower = 0;
    int ispeed = abs(iSpeed);
    while (ispeed >>= 1) ipower++;

    switch(ipower)
    {
      // regular playback
      case  0:
        //cmd.cmd = LPBCmd_PLAY;
        //cmd.param2.speed = 1 * 1024;
      break;
      default:
        // N x fast forward/rewind (I-frames)
        /*
        if (iSpeed > 0)
          cmd.cmd = LPBCmd_FAST_FORWARD;
        else
          cmd.cmd = LPBCmd_SCAN_BACKWARD;
        cmd.param2.speed = ipower * 1024;
        */
      break;
    }

    CSingleLock lock(m_gst_csection);

    m_speed = iSpeed;
  }
}

void CGSTPlayer::OnStartup()
{
  CThread::SetName("CGSTPlayer");

  g_renderManager.PreInit();
}

void CGSTPlayer::OnExit()
{
  //CLog::Log(LOGNOTICE, "CGSTPlayer::OnExit()");
  m_bStop = true;
  // if we didn't stop playing, advance to the next item in xbmc's playlist
  if(m_options.identify == false)
  {
    if (m_StopPlaying)
      m_callback.OnPlayBackStopped();
    else
      m_callback.OnPlayBackEnded();
  }
}

void CGSTPlayer::Process()
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer: Thread started");
  try
  {
    // big fake out here, we do not know the video width, height yet
    // so setup renderer to full display size and tell it we are doing
    // bypass. This tell it to get out of the way as amp will be doing
    // the actual video rendering in a video plane that is under the GUI
    // layer.
    int width = g_graphicsContext.GetWidth();
    int height= g_graphicsContext.GetHeight();
    int displayWidth  = width;
    int displayHeight = height;
    double fFrameRate = 24;
    unsigned int flags = 0;

    flags |= CONF_FLAGS_FORMAT_BYPASS;
    flags |= CONF_FLAGS_FULLSCREEN;
    CStdString formatstr = "BYPASS";
    CLog::Log(LOGDEBUG,"%s - change configuration. %dx%d. framerate: %4.2f. format: %s",
      __FUNCTION__, width, height, fFrameRate, formatstr.c_str());
    g_renderManager.IsConfigured();
    if(!g_renderManager.Configure(width, height, displayWidth, displayHeight, fFrameRate, flags))
      CLog::Log(LOGERROR, "%s - failed to configure renderer", __FUNCTION__);
    if (!g_renderManager.IsStarted())
      CLog::Log(LOGERROR, "%s - renderer not started", __FUNCTION__);

    // start the playback
    gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "CGSTPlayer::Process: Exception thrown");
    return;
  }

  m_video_index = 0;
  g_object_get(m_gstvars->player, "n-video", &m_video_count, NULL);
  if (m_video_count)
    g_object_set(m_gstvars->player, "current-video", m_video_index, NULL);

  m_audio_index = 0;
  g_object_get(m_gstvars->player, "n-audio", &m_audio_count, NULL);
  if (m_audio_count)
    g_object_set(m_gstvars->player, "current-audio", m_audio_index, NULL);

  // we are done initializing now, set the readyevent which will
  // drop CGUIDialogBusy, and release the hold in OpenFile
  m_ready.Set();

  // wait for playback to start with 2 second timeout
  // get our initial status.

  // once playback starts, we can turn on/off subs
  SetSubtitleVisible(g_settings.m_currentVideoSettings.m_SubtitleOn);

  m_callback.OnPlayBackStarted();
  /*
  while (!m_bStop && !m_StopPlaying)
  {
    if (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, false);
    else
      usleep(50*1000);
  }
  */
  g_main_loop_run(m_gstvars->loop);
  m_callback.OnPlayBackEnded();

}
#endif
