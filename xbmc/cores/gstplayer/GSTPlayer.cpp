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
#include "filesystem/File.h"
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
#include <gst/pbutils/missing-plugins.h>

// GstPlayFlags flags from playbin2. It is the policy of GStreamer to
// not publicly expose element-specific enums. That's why this
// GstPlayFlags enum has been copied here.
typedef enum {
    GST_PLAY_FLAG_VIDEO         = 0x00000001,
    GST_PLAY_FLAG_AUDIO         = 0x00000002,
    GST_PLAY_FLAG_TEXT          = 0x00000004,
    GST_PLAY_FLAG_VIS           = 0x00000008,
    GST_PLAY_FLAG_SOFT_VOLUME   = 0x00000010,
    GST_PLAY_FLAG_NATIVE_AUDIO  = 0x00000020,
    GST_PLAY_FLAG_NATIVE_VIDEO  = 0x00000040,
    GST_PLAY_FLAG_DOWNLOAD      = 0x00000080,
    GST_PLAY_FLAG_BUFFERING     = 0x000000100
} GstPlayFlags;


struct INT_GST_VARS
{
  bool        inited;
  bool        ready;
  gdouble     rate;
  GstBus      *bus;
  GMainLoop   *loop;
  GstElement  *player;
  GstElement  *videosink;
  GstElement  *audiosink;
  bool        video_uses_ismd;
  bool        audio_uses_ismd;
  
  std::string url;
  bool        use_appsrc;
  GstElement  *appsrc;
  XFILE::CFile *cfile;
};

gboolean CGSTPlayerBusCallback(GstBus *bus, GstMessage *msg, gpointer data)
{
  CGSTPlayer    *player  = (CGSTPlayer*)data;
  INT_GST_VARS  *gstvars = player->GetGSTVars();
  GError *error    = NULL;
  gchar  *dbg_info = NULL;

  switch (GST_MESSAGE_TYPE(msg))
  {
    case GST_MESSAGE_EOS:
      g_print ("GStreamer: End of stream\n");
      gst_element_set_state(gstvars->player, GST_STATE_READY);
      g_main_loop_quit(gstvars->loop);
      break;

    case GST_MESSAGE_ERROR:
      gst_message_parse_error(msg, &error, &dbg_info);
      if (error)
      {
        g_printerr("GStreamer: Error - %s, %s\n", 
          GST_OBJECT_NAME(msg->src), error->message);
        g_error_free(error);
      }
      g_free(dbg_info);
      g_main_loop_quit(gstvars->loop);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning(msg, &error, &dbg_info);
      if (error)
      {
        g_printerr("GStreamer: Warning - %s, %s\n",
          GST_OBJECT_NAME(msg->src), error->message);
        g_error_free(error);
      }
      g_free(dbg_info);
      break;

    case GST_MESSAGE_INFO:
      gst_message_parse_info(msg, &error, &dbg_info);
      if (error)
      {
        g_printerr("GStreamer: Info - %s, %s\n",
          GST_OBJECT_NAME(msg->src), error->message);
        g_error_free(error);
      g_free(dbg_info);
      }
      break;

    case GST_MESSAGE_TAG:
      {
        //printf("GStreamer: Message TAG\n");
        GstTagList *tag_list;
        gchar *title;

        gst_message_parse_tag(msg, &tag_list);
        if (gst_tag_list_get_string(tag_list, GST_TAG_TITLE, &title))
          g_print("GStreamer: Tag: %s\n", title);
      }
      break;
    case GST_MESSAGE_BUFFERING:
      //printf("GStreamer: Message BUFFERING\n");
      break;
    case GST_MESSAGE_STATE_CHANGED:
      //printf("GStreamer: Message STATE_CHANGED\n");
      // Ignore state changes from internal elements
      if (GST_MESSAGE_SRC(msg) == reinterpret_cast<GstObject*>(gstvars->player))
      {
        GstState old_state, new_state;

        gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
        printf("GStreamer: Element %s changed state from %s to %s.\n",
            GST_OBJECT_NAME (msg->src),
            gst_element_state_get_name(old_state),
            gst_element_state_get_name(new_state));
        /*
        if (old_state == GST_STATE_PAUSED && new_state == GST_STATE_PLAYING)
        {
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
          printf("GStreamer: Changed framerate(%f), width(%d), height(%d)\n",
            framerate, width, height);
        }
        */
      }
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
      //printf("GStreamer: Message STREAM_STATUS\n");
      break;
    case GST_MESSAGE_APPLICATION:
      printf("GStreamer: Message APPLICATION\n");
      break;
    case GST_MESSAGE_ELEMENT:
      if ( gst_is_missing_plugin_message(msg) )
      {
        gchar *description = gst_missing_plugin_message_get_description(msg);
        if (description)
        {
          printf("GStreamer: plugin %s not available!\n", description);
          g_free(description);
        }
      }
      else if (gst_structure_has_name(msg->structure, "have-ns-view"))
      {
        //NSView *nsview = NULL;
        void *nsview = NULL;
        nsview = g_value_get_pointer(gst_structure_get_value(msg->structure, "nsview"));
        // passes a GstGLView object to the callback, the object is an NSView
        return GST_BUS_PASS;
      }
      else if (gst_structure_has_name(msg->structure, "prepare-gdl-plane"))
      {
        gint bgcolor;
        g_object_get(gstvars->videosink, "bgcolor", &bgcolor, NULL);
        printf("GStreamer: Element bgcolor(%d)\n", bgcolor);
        
        gchar *rectangle = NULL;
        g_object_get(gstvars->videosink, "rectangle", &rectangle, NULL);
        printf("GStreamer: Element rectangle(%s)\n", rectangle);
        g_free(rectangle);

        gint gdl_plane;
        g_object_get(gstvars->videosink, "gdl-plane", &gdl_plane, NULL);
        printf("GStreamer: Element gdl-plane(%d)\n", gdl_plane);
        return GST_BUS_PASS;
      }
      else
      {
        g_print("GStreamer: Element %s\n", gst_structure_get_name(msg->structure));
      }
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

static void CGSTPlayerFeedData(GstElement *appsrc, guint size, CGSTPlayer *ctx)
{
  // This push method is called by the need-data signal callback,
  //  we feed data into the appsrc with an arbitrary size.
  INT_GST_VARS  *gstvars = ctx->GetGSTVars();
  unsigned int  readSize;
  GstBuffer     *buffer;
  GstFlowReturn ret;

  buffer   = gst_buffer_new_and_alloc(size);
  readSize = gstvars->cfile->Read(buffer->data, size);
  if (readSize > 0)
  {
    GST_BUFFER_SIZE(buffer) = readSize;

    g_signal_emit_by_name(gstvars->appsrc, "push-buffer", buffer, &ret);
		// this takes ownership of the buffer; don't unref
		//gst_app_src_push_buffer(appsrc, buffer);
  }
  else
  {
    // we are EOS, send end-of-stream
    g_signal_emit_by_name(gstvars->appsrc, "end-of-stream", &ret);
    //gst_app_src_end_of_stream(appsrc);
  }
  gst_buffer_unref(buffer);

  return;
}

static gboolean CGSTPlayerSeekData(GstElement *appsrc, guint64 position, CGSTPlayer *ctx)
{
  // called when appsrc wants us to return data from a new position
  // with the next call to push-buffer (FeedData).
  position = ctx->GetGSTVars()->cfile->Seek(position, SEEK_SET);
  if (position >= 0)
    return TRUE;
  else
    return FALSE;
}

static void CGSTPlayerFoundSource(GObject *object, GObject *orig, GParamSpec *pspec, CGSTPlayer *ctx)
{
  INT_GST_VARS  *gstvars = ctx->GetGSTVars();
  // called when playbin2 has constructed a source object to read
  //  from. Since we provided the appsrc:// uri to playbin2, 
  //  this will be the appsrc that we must handle. 
  // we set up some signals to push data into appsrc and one to perform a seek.

  // get a handle to the appsrc
  g_object_get(orig, pspec->name, &gstvars->appsrc, NULL);

  unsigned int flags = READ_TRUNCATED;
  gstvars->cfile = new XFILE::CFile();
  if (CFileItem(gstvars->url, false).IsInternetStream())
    flags |= READ_CACHED;
  // open file in binary mode
  if (!gstvars->cfile->Open(gstvars->url, flags))
    return;

  // we can set the length in appsrc. This allows some elements to estimate the
  // total duration of the stream. It's a good idea to set the property when you
  // can but it's not required.
  int64_t filelength = gstvars->cfile->GetLength();
  if (filelength > 0)
    g_object_set(gstvars->appsrc, "size", (gint64)filelength, NULL);
  // we are seekable in push mode, this means that the element usually pushes
  // out buffers of an undefined size and that seeks happen only occasionally
  // and only by request of the user.
  if (filelength > 0)
    gst_util_set_object_arg(G_OBJECT(gstvars->appsrc), "stream-type", "seekable");

  // configure the appsrc, we will push a buffer to appsrc when it needs more data
  g_signal_connect(gstvars->appsrc, "need-data", G_CALLBACK(CGSTPlayerFeedData), ctx);
  g_signal_connect(gstvars->appsrc, "seek-data", G_CALLBACK(CGSTPlayerSeekData), ctx);
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
  m_gstvars->ready  = false;
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

    m_gstvars->cfile = NULL;
    m_gstvars->appsrc = NULL;
    m_gstvars->use_appsrc = false;
    if (m_item.m_strPath.Left(6).Equals("udp://"))
    {
      // protocol goes to gstreamer as is
      m_gstvars->url = m_item.m_strPath;
      if (!gst_uri_is_valid(m_gstvars->url.c_str()))
        return false;
    }
    else if (m_item.m_strPath.Left(7).Equals("rtsp://"))
    {
      // protocol goes to gstreamer as is
      m_gstvars->url = m_item.m_strPath;
      if (!gst_uri_is_valid(m_gstvars->url.c_str()))
        return false;
    }
    else if (m_item.m_strPath.Left(7).Equals("http://"))
    {
      // strip user agent that we append
      m_gstvars->url = m_item.m_strPath;
      m_gstvars->url = m_gstvars->url.erase(m_gstvars->url.rfind('|'), m_gstvars->url.size());
      if (!gst_uri_is_valid(m_gstvars->url.c_str()))
        return false;
    }
    else
    {
      m_gstvars->use_appsrc = true;
      m_gstvars->url = m_item.m_strPath;
    }

    CLog::Log(LOGNOTICE, "CGSTPlayer: Opening: URL=%s", m_gstvars->url.c_str());
    
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
    m_gstvars->bus = gst_pipeline_get_bus(GST_PIPELINE(m_gstvars->player));
    gst_bus_add_watch(m_gstvars->bus, (GstBusFunc)CGSTPlayerBusCallback, this);
    
    // create video sink
    m_gstvars->videosink = gst_element_factory_make(m_videosink.c_str(), NULL);
    if (m_gstvars->videosink)
    {
      m_gstvars->video_uses_ismd = true;
      g_object_set(m_gstvars->videosink, "bgcolor", 0, NULL);
      g_object_set(m_gstvars->videosink, "rectangle", "0,0,0,0", NULL);
      g_object_set(m_gstvars->videosink, "gdl-plane", GDL_VIDEO_PLANE, NULL);
      //g_object_set(m_gstvars->videosink, "flush-repeat-frame", FALSE, NULL);
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
    if (m_gstvars->use_appsrc)
    {
      g_object_set(m_gstvars->player, "uri", "appsrc://", NULL);
      g_signal_connect(m_gstvars->player, "deep-notify::source",
        (GCallback)CGSTPlayerFoundSource, this);
    }
    else
    {
      g_object_set(m_gstvars->player, "uri", m_gstvars->url.c_str(), NULL);
    }

    // disable subtitles until we can figure out how to hook them.
    GstPlayFlags flags;
    g_object_get(m_gstvars->player, "flags", &flags, NULL);
    g_object_set(m_gstvars->player, "flags", (flags & ~GST_PLAY_FLAG_TEXT), NULL);

    m_gstvars->inited = true;
    gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);

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
    m_gstvars->ready  = false;
    m_gstvars->inited = false;

    gst_element_set_state(m_gstvars->player, GST_STATE_NULL);
    gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    g_main_loop_quit(m_gstvars->loop);
    g_main_loop_unref(m_gstvars->loop);
    gst_object_unref(m_gstvars->bus);
    gst_object_unref(m_gstvars->player);
  }

  m_bStop = true;
  m_StopPlaying = true;

  CLog::Log(LOGDEBUG, "CGSTPlayer: waiting for threads to exit");
  // wait for the main thread to finish up
  // since this main thread cleans up all other resources and threads
  // we are done after the StopThread call
  StopThread();

  if (m_gstvars->cfile)
  {
    m_gstvars->cfile->Close();
    delete m_gstvars->cfile;
    m_gstvars->cfile = NULL;
  }

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
    if (m_gstvars->ready)
    {
      gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
      gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    }
    m_callback.OnPlayBackResumed();
  }
  else
  {
    if (m_gstvars->ready)
    {
      gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
      gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    }
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
  if (m_gstvars->ready)
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
  if (m_gstvars->ready)
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
  g_print("CGSTPlayer::SeekTime(%lld)\n", seek_ms);
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->ready)
  {
    // stop the playback
		GstStateChangeReturn ret = gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			// Wait for the state change before requesting a seek
			const GstMessageType types = (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE);
      GstMessage *msg = gst_bus_timed_pop_filtered(m_gstvars->bus, GST_CLOCK_TIME_NONE, types);
			if(msg != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
					g_print("Stop Error\n");
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ASYNC_DONE)
					ret = GST_STATE_CHANGE_SUCCESS;
				gst_message_unref(msg);
			}
		}
    // gst time units are nanoseconds.
    gint64 seek_ns = seek_ms * GST_MSECOND;
    gst_element_seek(m_gstvars->player, 
			m_gstvars->rate, GST_FORMAT_TIME, 
      (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
			GST_SEEK_TYPE_SET,  seek_ns,               // start
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);  // end
    // wait for seek to complete
    gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    // restart the playback
    gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
  }
  m_callback.OnPlayBackSeek((int)seek_ms, (int)(seek_ms - m_elapsed_ms));
}

__int64 CGSTPlayer::GetTime()
{
  //g_print("CGSTPlayer::GetTime\n");
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->ready)
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
  //g_print("CGSTPlayer::GetTotalTime\n");
  CSingleLock lock(m_gst_csection);

  if (m_gstvars->ready)
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
/* TODO: fix this crap
  if (m_gstvars->ready)
  {
    GstPad *pad = gst_element_get_pad(m_gstvars->videosink, "sink");
    //GstPad *pad = gst_element_get_static_pad(m_gstvars->videosink, "sink");
    GstPad *peerpad = gst_pad_get_peer(pad);
    GstCaps *caps = gst_pad_get_negotiated_caps(peerpad);
    if (caps)
    {
      gint width, height;
      gint rate1, rate2;

      const GstStructure *structure = gst_caps_get_structure(caps, 0);
      gst_structure_get_int(structure, "width",  &width);
      gst_structure_get_int(structure, "height", &height);
      gst_structure_get_fraction(structure, "framerate", &rate1, &rate2);
      m_video_width = width;
      gst_caps_unref(caps);
    }
    gst_object_unref(pad);
  }
  g_print("CGSTPlayer::GetPictureWidth(%d)\n", m_video_width);
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetPictureWidth(%d)", m_video_width);
*/
  return m_video_width;
}

int CGSTPlayer::GetPictureHeight()
{
/* TODO: fix this crap
  if (m_gstvars->ready)
  {
    GstPad *pad = gst_element_get_pad(m_gstvars->videosink, "sink");
    GstPad *peerpad = gst_pad_get_peer(pad);
    GstCaps *caps = gst_pad_get_negotiated_caps(peerpad);
    if (caps)
    {
      gint width, height;

      const GstStructure *structure = gst_caps_get_structure(caps, 0);
      gst_structure_get_int(structure, "width",  &width);
      gst_structure_get_int(structure, "height", &height);
      m_video_height = height;
      gst_caps_unref(caps);
    }
    gst_object_unref(pad);
  }
  g_print("CGSTPlayer::GetPictureHeight(%d)\n", m_video_height);
  CLog::Log(LOGDEBUG, "CGSTPlayer::GetPictureHeight(%d)", m_video_height);
*/
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

  CSingleLock lock(m_gst_csection);
  if (m_speed != iSpeed)
  {
    // recover power of two value
    int ipower = 0;
    int ispeed = abs(iSpeed);
    while (ispeed >>= 1) ipower++;

    GstSeekFlags flags;
    switch(ipower)
    {
      // regular playback
      case  0:
        m_gstvars->rate = 1.0;
        flags = GST_SEEK_FLAG_NONE;
      break;
      default:
        flags = (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_SKIP);
        // N x fast forward/rewind (I-frames)
        if (iSpeed > 0)
          m_gstvars->rate = ipower *  1.0;
        else
          m_gstvars->rate = ipower * -1.0;
      break;
    }

    gst_element_seek(m_gstvars->player, 
			m_gstvars->rate, GST_FORMAT_TIME, flags,
			GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,     // start
      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);    // end
    // wait for rate change to complete
    gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);

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
    // hide the gui layer so we can get stream info first
    // without having video playback blended into it.
    g_Windowing.Hide();

    if (WaitForGSTPaused(2000))
    {
      m_gstvars->rate  = 1.0;
      m_gstvars->ready = true;

      // starttime has units of seconds (SeekTime will start playback)
      /*
      if (m_options.starttime > 0)
        SeekTime(m_options.starttime * 1000);
      else
      */
        gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
    }
    else
    {
      CLog::Log(LOGDEBUG, "CGSTPlayer::Process:WaitForGSTPaused timeout");
      throw;
    }

    if (WaitForGSTPlaying(2000))
    {
      // we are done initializing now, set the readyevent which will
      // drop CGUIDialogBusy, and release the hold in OpenFile
      m_ready.Set();

      m_video_index = 0;
      g_object_get(m_gstvars->player, "n-video", &m_video_count, NULL);
      if (m_video_count)
        g_object_set(m_gstvars->player, "current-video", m_video_index, NULL);

      m_audio_index = 0;
      g_object_get(m_gstvars->player, "n-audio", &m_audio_count, NULL);
      if (m_audio_count)
        g_object_set(m_gstvars->player, "current-audio", m_audio_index, NULL);

      // turn on/off subs
      SetSubtitleVisible(g_settings.m_currentVideoSettings.m_SubtitleOn);

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
    }
    else
    {
      m_ready.Set();
      m_StopPlaying = true;
      CLog::Log(LOGERROR, "CGSTPlayer::Process: WaitForGSTPlaying() failed");
      throw;
    }

    m_callback.OnPlayBackStarted();
    WaitForWindowFullScreenVideo(2000);
    // show gui layer again.
    g_Windowing.Show();

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
  catch(...)
  {
    m_ready.Set();
    m_StopPlaying = true;
    g_Windowing.Show();
    CLog::Log(LOGERROR, "CGSTPlayer::Process: Exception thrown");
    return;
  }

}

bool CGSTPlayer::WaitForGSTPaused(int timeout_ms)
{
  bool rtn = false;
  GstState state, pending;

  while (!m_bStop && (timeout_ms > 0))
  {
    gst_element_get_state(m_gstvars->player, &state, &pending, 100 * GST_MSECOND);
    if (state == GST_STATE_PAUSED)
    {
      rtn = true;
      break;
    }
    timeout_ms -= 100;
  }

  return rtn;
}

bool CGSTPlayer::WaitForGSTPlaying(int timeout_ms)
{
  bool rtn = false;
  GstState state, pending;

  while (!m_bStop && (timeout_ms > 0))
  {
    gst_element_get_state(m_gstvars->player, &state, &pending, 100 * GST_MSECOND);
    if (state == GST_STATE_PLAYING)
    {
      rtn = true;
      break;
    }
    timeout_ms -= 100;
  }

  return rtn;
}

bool CGSTPlayer::WaitForWindowFullScreenVideo(int timeout_ms)
{
  bool rtn = false;

  double present_time;
  // we do a two step check.
  // 1st, wait for switch to fullscreen video rendering in gui
  while (!m_bStop && (timeout_ms > 0))
  {
    if (g_graphicsContext.IsFullScreenVideo())
      break;

    timeout_ms -= 100;
    Sleep(100);
  }
  // 2nd, wait for renderer to flip at least once.
  present_time = g_renderManager.GetPresentTime();
  while (!m_bStop && (timeout_ms > 0))
  {
    if (present_time < g_renderManager.GetPresentTime())
    {
      rtn = true;
      break;
    }
    timeout_ms -= 100;
    Sleep(100);
  }
  // BUGFIX: why don't we know when gui has truly transitioned?
  // sleep another 1/2 seconds to be sure.
  Sleep(500);

  return rtn;
}

#endif
