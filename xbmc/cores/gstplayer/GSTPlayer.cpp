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
  GST_PLAY_FLAG_VIDEO         = (1 << 0),
  GST_PLAY_FLAG_AUDIO         = (1 << 1),
  GST_PLAY_FLAG_TEXT          = (1 << 2),
  GST_PLAY_FLAG_VIS           = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME   = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO  = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO  = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD      = (1 << 7),
  GST_PLAY_FLAG_BUFFERING     = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE   = (1 << 9)
} GstPlayFlags;


struct INT_GST_VARS
{
  bool        inited;
  bool        ready;
  gdouble     rate;
  GstBus      *bus;
  GMainLoop   *loop;
  GstElement  *player;
  GstElement  *textsink;
  GstElement  *videosink;
  GstElement  *audiosink;
  bool        video_uses_ismd;
  bool        audio_uses_ismd;

  std::string url;
  bool        use_appsrc;
  GstElement  *appsrc;
  XFILE::CFile *cfile;

  CStdString  acodec_name;
  CStdString  vcodec_name;
  CStdString  tcodec_name;
};

static bool testName(const CStdString codec, const char *name)
{
  if (codec.Find(name) == -1)
   return false;
  else
   return true;
}

static const char *gstTagNameToAudioName(const char *gst_name)
{
  const char *text = NULL;
  CStdString codec = gst_name;
  codec.ToUpper();

  if (testName(codec, "AAC"))
    text = "aac";
  else if (testName(codec, "AC3") || testName(codec, "AC-3"))
    text = "ac3";
  else if (testName(codec, "DCA"))
    text = "dca";
  else if (testName(codec, "DTS"))
    text = "dts";
  else if (testName(codec, "WMA"))
    text = "wma";
  else if (testName(codec, "WMAPRO"))
    text = "wmapro";

  //g_print("GStreamer: gstTagNameToAudioName, gst_name(%s), text(%s)\n", gst_name, text);
  return text;
}

static const char *gstTagNameToVideoName(const char *gst_name)
{
  const char *text = NULL;
  CStdString codec = gst_name;
  codec.ToUpper();

  if (testName(codec, "H264") || testName(codec, "H.264"))
    text = "h264";
  else if (testName(codec, "MPEG1"))
    text = "mpeg1video";
  else if (testName(codec, "MPEG2"))
    text = "mpeg2video";
  else if (testName(codec, "MPEG4"))
    text = "mpeg4";
  else if (testName(codec, "VC1") || testName(codec, "VC-1"))
    text = "mpeg4";
  else if (testName(codec, "MPEG4"))
    text = "mpeg4";

  //g_print("GStreamer: gstTagNameToVideoName, gst_name(%s), text(%s)\n", gst_name, text);
  return text;
}

static const char *gstTagNameToLanguage(const char *gst_name)
{
  const char *text = NULL;
  CStdString codec = gst_name;
  codec.ToUpper();

  if (testName(codec, "EN") || testName(codec, "ENG"))
    text = "english";
  else if (testName(codec, "RO"))
    text = "romanian";
  else if (testName(codec, "FR"))
    text = "french";
  else if (testName(codec, "NL"))
    text = "dutch";

  //g_print("GStreamer: gstTagNameToLanguage, gst_name(%s), text(%s)\n", gst_name, text);
  return text;
}

gboolean CGSTPlayerBusCallback(GstBus *bus, GstMessage *msg, CGSTPlayer *gstplayer)
{
  INT_GST_VARS  *gstvars = gstplayer->GetGSTVars();
  GError *error    = NULL;
  gchar  *dbg_info = NULL;

  switch (GST_MESSAGE_TYPE(msg))
  {
    case GST_MESSAGE_EOS:
      g_print("GStreamer: End of stream\n");
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
        if (!strcmp(GST_OBJECT_NAME(msg->src), "suboverlay"))
        {
        }
        else
        {
          g_printerr("GStreamer: Warning - %s, %s\n",
            GST_OBJECT_NAME(msg->src), error->message);
        }
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
        g_print("GStreamer: Message TAG from %s\n",
          GST_MESSAGE_SRC_NAME(msg));
        gchar *title;
        GstTagList *tags;

        gst_message_parse_tag(msg, &tags);
        if (gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &title))
        {
          gstvars->vcodec_name = gstTagNameToVideoName(title);
          g_print("GStreamer:  video codec:%s\n", title);
        }
        if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &title))
        {
          gstvars->acodec_name = gstTagNameToAudioName(title);
          g_print("GStreamer:  audio codec:%s\n", title);
          if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &title))
          {
            gstvars->acodec_name = gstTagNameToLanguage(title);
            g_print("GStreamer:  language code:%s\n", title);
          }
        }
        if (gst_tag_list_get_string(tags, GST_TAG_SUBTITLE_CODEC, &title))
        {
          gstvars->tcodec_name = gstTagNameToLanguage(title);
          g_print("GStreamer:  subtitle codec:%s\n", title);
        }
        if (gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &title))
          g_print("GStreamer:  containter format:%s\n", title);
        if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &title))
          g_print("GStreamer:  show name:%s\n", title);
      }
      break;
    case GST_MESSAGE_BUFFERING:
      {
        gint percent = 0;
        gst_message_parse_buffering(msg, &percent);
        g_print("GStreamer: Buffering %d percent done)", percent);
      }
      break;
    case GST_MESSAGE_STATE_CHANGED:
      // Ignore state changes from internal elements
      if (GST_MESSAGE_SRC(msg) == reinterpret_cast<GstObject*>(gstvars->player))
      {
        GstState old_state, new_state;

        gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
        g_print("GStreamer: Element %s changed state from %s to %s.\n",
          GST_OBJECT_NAME (msg->src),
          gst_element_state_get_name(old_state),
          gst_element_state_get_name(new_state));
      }
     break;
    case GST_MESSAGE_STATE_DIRTY:
      g_print("GStreamer: Message STATE_DIRTY\n");
      break;
    case GST_MESSAGE_STEP_DONE:
      g_print("GStreamer: Message STEP_DONE\n");
      break;
    case GST_MESSAGE_CLOCK_PROVIDE:
      g_print("GStreamer: Message CLOCK_PROVIDE\n");
      break;
    case GST_MESSAGE_CLOCK_LOST:
      g_print("GStreamer: Message CLOCK_LOST\n");
      break;
    case GST_MESSAGE_NEW_CLOCK:
      g_print("GStreamer: Message NEW_CLOCK\n");
      break;
    case GST_MESSAGE_STRUCTURE_CHANGE:
      g_print("GStreamer: Message STRUCTURE_CHANGE\n");
      break;
    case GST_MESSAGE_STREAM_STATUS:
      //g_print("GStreamer: Message STREAM_STATUS\n");
      break;
    case GST_MESSAGE_APPLICATION:
      g_print("GStreamer: Message APPLICATION\n");
      break;
    case GST_MESSAGE_ELEMENT:
      if ( gst_is_missing_plugin_message(msg) )
      {
        gchar *description = gst_missing_plugin_message_get_description(msg);
        if (description)
        {
          g_print("GStreamer: plugin %s not available!\n", description);
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
        gint gdl_plane;
        gchar *rectangle = NULL;
        g_object_get(gstvars->videosink, "rectangle", &rectangle, NULL);
        g_object_get(gstvars->videosink, "gdl-plane", &gdl_plane, NULL);

        g_print("GStreamer: Element prepare-gdl-plane: rectangle(%s), gdl-plane(%d)\n", 
          rectangle, gdl_plane);

        g_free(rectangle);

        return GST_BUS_PASS;
      }
      else if (gst_structure_has_name(msg->structure, "playbin2-stream-changed"))
      {
        g_print("GStreamer: Element %s\n", gst_structure_get_name(msg->structure));
      }
      else
      {
        g_print("GStreamer: Element %s\n", gst_structure_get_name(msg->structure));
      }
      break;
    case GST_MESSAGE_SEGMENT_START:
      g_print("GStreamer: Message SEGMENT_START\n");
      break;
    case GST_MESSAGE_SEGMENT_DONE:
      g_print("GStreamer: Message SEGMENT_DONE\n");
      break;
    case GST_MESSAGE_DURATION:
      g_print("GStreamer: Message DURATION\n");
      break;
    case GST_MESSAGE_LATENCY:
      g_print("GStreamer: Message LATENCY\n");
      break;
    case GST_MESSAGE_ASYNC_START:
      g_print("GStreamer: Message ASYNC_START\n");
      break;
    case GST_MESSAGE_ASYNC_DONE:
      g_print("GStreamer: Message ASYNC_DONE\n");
      break;
#if (GST_VERSION_MICRO >= 26)
    case GST_MESSAGE_REQUEST_STATE:
      g_print("GStreamer: Message REQUEST_STATE\n");
      break;
    case GST_MESSAGE_STEP_START:
      g_print("GStreamer: Message STEP_START\n");
      break;
#endif
#if (GST_VERSION_MICRO >= 29)
    case GST_MESSAGE_QOS:
      g_print("GStreamer: Message QOS\n");
      break;
#endif
    default:
      g_print("GStreamer: Unknown message %i\n", GST_MESSAGE_TYPE(msg));
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
  m_gstvars->textsink  = NULL;
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
  m_audiosink_name = "osxaudiosink";
  m_videosink_name = "osxvideosink";
#else
  m_audiosink_name = "ismd_audio_sink";
  m_videosink_name = "ismd_vidrend_bin";
#endif
  m_subtitlesink_name = "textoverlay";

  if (pConfig)
  {
    // New config specific to gstplayer
    XMLUtils::GetString(pConfig, "audiosink", m_audiosink_name);
    XMLUtils::GetString(pConfig, "videosink", m_videosink_name);
  }
  CLog::Log(LOGNOTICE, "CGSTPlayer : audiosink (%s)", m_audiosink_name.c_str());
  CLog::Log(LOGNOTICE, "CGSTPlayer : videosink (%s)", m_videosink_name.c_str());

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
    m_audio_info  =  "none";
    m_audio_bits  =  0;
    m_audio_channels = 0;
    m_audio_samplerate = 0;

    m_video_index = -1;
    m_video_count =  0;
    m_video_info  =  "none";
    m_video_fps   =  0.0;
    m_video_width =  0;
    m_video_height=  0;

    m_subtitle_index = -1;
    m_subtitle_count =  0;
    m_chapter_count  =  0;

    m_StopPlaying = false;

    m_gstvars->player = gst_element_factory_make( "playbin2", "player");
    gst_element_set_state(m_gstvars->player, GST_STATE_NULL);
    //
    m_gstvars->loop = g_main_loop_new(NULL, FALSE);
    if (m_gstvars->loop == NULL)
      return false;
    //
    m_gstvars->bus = gst_pipeline_get_bus(GST_PIPELINE(m_gstvars->player));
    if (m_gstvars->bus  == NULL)
      return false;
    gst_bus_add_watch(m_gstvars->bus, (GstBusFunc)CGSTPlayerBusCallback, this);
    
    // create video sink
    m_gstvars->videosink = gst_element_factory_make(m_videosink_name.c_str(), NULL);
    if (m_gstvars->videosink)
    {
      m_gstvars->video_uses_ismd = true;

      gint gdl_plane;
      g_object_get(m_gstvars->videosink, "gdl-plane", &gdl_plane, NULL);
      if (gdl_plane != GDL_VIDEO_PLANE)
        g_object_set(m_gstvars->videosink, "gdl-plane", GDL_VIDEO_PLANE, NULL);
      g_object_set(m_gstvars->videosink, "rectangle", "0,0,0,0", NULL);
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
    m_gstvars->audiosink = gst_element_factory_make(m_audiosink_name.c_str(), NULL);
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

    // create subtitle sink
    // flugdlsink "text-sink"
    // disable subtitles until we can figure out how to hook them.
    GstPlayFlags flags;
    g_object_get(m_gstvars->player, "flags", &flags, NULL);
    g_object_set(m_gstvars->player, "flags", (flags & ~GST_PLAY_FLAG_TEXT), NULL);
    g_printerr("GStreamer: Disabling subtitles\n");

    // video time offset (to audio)
    // Specifies an offset in ns to apply on clock synchronization.
    // "stream-time-offset"
    // "av-offset

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

  if (m_StopPlaying)
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
  return m_video_count > 0;
}

bool CGSTPlayer::HasAudio() const
{
  return m_audio_count > 0;
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
  m_audio_info.Format("Audio stream (%d) [%s]",
    m_audio_index, m_gstvars->acodec_name);
  strAudioInfo = m_audio_info;
}

void CGSTPlayer::GetVideoInfo(CStdString &strVideoInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetVideoInfo");
  m_video_info.Format("Video stream (%d) [%s]",
    m_video_index, m_gstvars->vcodec_name);
  strVideoInfo = m_video_info;
}

void CGSTPlayer::GetGeneralInfo(CStdString& strGeneralInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetGeneralInfo");
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

  if (m_gstvars->ready)
  {
    CSingleLock lock(m_gst_csection);

    GstState state;
    gst_element_get_state(m_gstvars->player, &state, NULL, 100 * GST_MSECOND);
    if (state != GST_STATE_PAUSED)
    {
      // stop the playback
      GstStateChangeReturn ret = gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
      if (ret == GST_STATE_CHANGE_ASYNC)
      {
        // Wait for the state change before requesting a seek
        const GstMessageType types = (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE);
        GstMessage *msg = gst_bus_timed_pop_filtered(m_gstvars->bus, GST_CLOCK_TIME_NONE, types);
        if (msg != NULL)
        {
          if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
            g_print("Stop Error\n");
          else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ASYNC_DONE)
            ret = GST_STATE_CHANGE_SUCCESS;
          gst_message_unref(msg);
        }
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
  // returns number of audio channels (ie 5.1 = 6)
  return m_audio_channels;
}

int CGSTPlayer::GetBitsPerSample()
{
  return m_audio_bits;
}

int CGSTPlayer::GetSampleRate()
{
  return m_audio_samplerate;
}

CStdString CGSTPlayer::GetAudioCodecName()
{
  return m_gstvars->acodec_name;
}

CStdString CGSTPlayer::GetVideoCodecName()
{
  return m_gstvars->vcodec_name;
}

int CGSTPlayer::GetPictureWidth()
{
  return m_video_width;
}

int CGSTPlayer::GetPictureHeight()
{
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
/*
    // TODO: figure out why FF/RW are not working.
    g_print("CGSTPlayer::ToFFRW: iSpeed(%d)\n", iSpeed);

    if (m_gstvars->ready)
    {
      gint64 elapsed_ns = 0;
      GstFormat fmt = GST_FORMAT_TIME;
      if (gst_element_query_position(m_gstvars->player, &fmt, &elapsed_ns))
        m_elapsed_ms = elapsed_ns / GST_MSECOND;

      GstSeekFlags flags;
      if (iSpeed == 1)
        flags = GST_SEEK_FLAG_NONE;
      else
        flags = GST_SEEK_FLAG_SKIP;
      
      m_gstvars->rate = iSpeed;
      gst_element_seek(m_gstvars->player,
        m_gstvars->rate, GST_FORMAT_TIME, flags,
        GST_SEEK_TYPE_SET,  elapsed_ns,           // start
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE); // end
      // wait for rate change to complete
      gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    }
*/
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
    if (WaitForGSTPaused(10000))
    {
      m_gstvars->rate  = 1.0;
      m_gstvars->ready = true;

      ProbeStreams();

      // starttime has units of seconds (SeekTime will start playback)
      if (m_options.starttime > 0)
        SeekTime(m_options.starttime * 1000);
      else
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

void CGSTPlayer::ProbeStreams()
{
  m_audio_index = 0;
  m_video_index = 0;
  m_subtitle_index = 0;

  g_object_get(G_OBJECT(m_gstvars->player),
    "n-audio", &m_audio_count, "n-video", &m_video_count, "n-text", &m_subtitle_count, NULL);
  g_print("CGSTPlayer::Process: n-audio(%d), n-video(%d), n-text(%d)\n",
    m_audio_count, m_video_count, m_subtitle_count);

  if (m_video_count)
    g_object_set(m_gstvars->player, "current-video", m_video_index, NULL);
  if (m_audio_count)
    g_object_set(m_gstvars->player, "current-audio", m_audio_index, NULL);
  if (m_subtitle_count)
    g_object_set(m_gstvars->player, "current-text", m_subtitle_index, NULL);

  if (m_gstvars->ready)
  {
    // probe audio
    for (int i = 0; i < m_audio_count; i++)
    {
      GstPad *pad = NULL;
      g_signal_emit_by_name(m_gstvars->player, "get-audio-pad", i, &pad, NULL);
      if (pad)
      {
        GstCaps *caps = gst_pad_get_negotiated_caps(pad); 
        if (caps)
        {
          gint width, channels, samplerate;
          CStdString caps_str = gst_caps_to_string(caps);
          GstStructure *structure = gst_caps_get_structure(caps, 0);

          if (!gst_structure_get_int(structure, "width", &width))
            width = 0;
          if (!gst_structure_get_int(structure, "channels", &channels))
            channels = 2;
          if (!gst_structure_get_int(structure, "rate", &samplerate))
            samplerate = 48000;

          if (i == m_audio_index)
          {
            m_audio_bits = width;
            m_audio_channels = channels;
            m_audio_samplerate = channels;
          }

          g_print("CGSTPlayer::Process: caps(%s), width(%d), channels(%d), samplerate(%d)\n",
            caps_str.c_str(), width, channels, samplerate);

          gst_caps_unref(caps);
        }
        gst_object_unref(pad);
      }
    }

    // probe video
    for (int i = 0; i < m_video_count; i++)
    { 
      GstPad *pad = NULL;
      g_signal_emit_by_name(m_gstvars->player, "get-video-pad", i, &pad, NULL);
      if (pad)
      {
        GstCaps *caps = gst_pad_get_negotiated_caps(pad); 
        if (caps)
        {
          gchar *caps_str = gst_caps_to_string(caps);
          GstStructure *structure = gst_caps_get_structure(caps, 0);

          gint video_fps_d, video_fps_n, video_width, video_height;
          if (!gst_structure_get_fraction(structure, "framerate", &video_fps_n, &video_fps_d))
          {
            video_fps_n = 0;
            video_fps_d = 1;
          }
          if (!gst_structure_get_int(structure, "width",  &video_width))
            video_width = 0;
          if (!gst_structure_get_int(structure, "height",  &video_height))
            video_height = 0;

          if (i == m_video_index)
          {
            m_video_fps    = (float)video_fps_n/(float)video_fps_d;
            m_video_width  = video_width;
            m_video_height = video_height;
          }

          g_print("CGSTPlayer::Process: caps(%s), framerate(%f), width(%d), height(%d)\n",
            caps_str, m_video_fps, video_width, m_video_height);

          gst_caps_unref(caps);
        }
        gst_object_unref(pad);
      }
    }

    // probe subtitles
    for (int i = 0; i < m_subtitle_count; i++)
    {
      GstPad *pad = NULL;
      g_signal_emit_by_name(m_gstvars->player, "get-text-pad", i, &pad, NULL);
      if (pad)
      {
        GstCaps *caps = gst_pad_get_negotiated_caps(pad); 
        if (caps)
        {
          gchar *caps_str = gst_caps_to_string(caps);
          //GstStructure *structure = gst_caps_get_structure(caps, 0);
          // TODO: retreve subtitle info

          g_print("CGSTPlayer::Process: caps(%s)\n", caps_str);

          gst_caps_unref(caps);
        }
        gst_object_unref(pad);
      }
    }
  }
}

bool CGSTPlayer::WaitForGSTPaused(int timeout_ms)
{
  bool rtn = false;
  GstState state;

  while (!m_bStop && (timeout_ms > 0))
  {
    // pump the gst message bus while we wait
    while (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, false);
    // check for paused
    gst_element_get_state(m_gstvars->player, &state, NULL, 100 * GST_MSECOND);
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
  GstState state;

  while (!m_bStop && (timeout_ms > 0))
  {
    // pump the gst message bus while we wait
    while (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, false);
    // check for playing
    gst_element_get_state(m_gstvars->player, &state, NULL, 100 * GST_MSECOND);
    if (state == GST_STATE_PLAYING)
    {
      rtn = true;
      break;
    }
    timeout_ms -= 100;
  }

  return rtn;
}

#endif
