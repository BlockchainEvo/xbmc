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
#include "GSTAppsrc.h"
#include "GSTFileInfo.h"
#include "Application.h"
#include "FileItem.h"
#include "GUIInfoManager.h"
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
#include "utils/MathUtils.h"
#include "utils/TimeUtils.h"
#include "utils/URIUtils.h"
#include "utils/XMLUtils.h"

#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/missing-plugins.h>

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
// int flags = (GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO | GST_PLAY_FLAG_NATIVE_AUDIO);
// g_object_get(G_OBJECT(m_playbin), "flags", &flags, NULL);
// g_object_set(G_OBJECT(m_playbin), "flags", flags, NULL);


// ismd scale-mode, defaults to ISMD_ZOOM_TO_FIT.
typedef enum {
  ISMD_SCALE_TO_FIT,    // independent x and y scaling, ignores pixel aspect ratio
  ISMD_VPP_NO_SCALING,  // force the output to be same size as the input ignores pixel aspect ratio
  ISMD_ZOOM_TO_FILL,    // fill entire screen. use both the source and destination pixel aspect ratios
  ISMD_ZOOM_TO_FIT,     // fit at least one side of the destination rectangle, letterbox/pillarbox the remainder
  ISMD_VPP_SCP_ANAMORPHIC
} ISMD_SCALE_MODE;

static GMainLoop *gstvars_loop = NULL;
struct INT_GST_VARS
{
  bool                    inited;
  bool                    ready;
  gdouble                 rate;
  GstBus                  *bus;
  GstElement              *player;
  GstElement              *volume;
  GstElement              *textsink;
  GstElement              *videosink;

  CGSTAppsrc              *appsrc;

  std::vector<CStdString> acodec_name;
  std::vector<CStdString> vcodec_name;
  std::vector<CStdString> tcodec_name;

  std::vector<CStdString> acodec_language;
  std::vector<CStdString> vcodec_language;
  std::vector<CStdString> tcodec_language;

  unsigned int            subtitle_end;
  CStdString              subtitle_text;
  CStdString              video_title;

  int                     flags;

  bool                    is_udp;
  bool                    is_rtmp;
  GstElement              *net_vbin;
  GstElement              *net_abin;
  GstElement              *net_source;
  GstElement              *net_queue;
  GstElement              *net_clkrecover;
  GstElement              *net_typefind;
  GstElement              *net_decoder;
  bool                    net_video;
  bool                    net_audio;
  bool                    net_text;

  CCriticalSection        csection;
};

static void really_unref(GstElement *obj)
{
  while(obj && GST_OBJECT_REFCOUNT(obj))
    gst_object_unref(obj);
}
/*
static void print_caps(GstCaps *caps)
{
  for (unsigned int i = 0; i < gst_caps_get_size(caps); i++)
  {
    GstStructure *structure = gst_caps_get_structure(caps, i);
    gchar *tmp = gst_structure_to_string(structure);

    g_print("%s - %s\n", i ? "    " : "", tmp);
    g_free(tmp);
  }
}
*/

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
  else if (testName(codec, "DTS") || testName(codec, "DCA"))
    text = "dts";
  else if (testName(codec, "WMA"))
    text = "wma";
  else if (testName(codec, "WMAPRO"))
    text = "wmapro";

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

  return text;
}

// ****************************************************************
gboolean CGSTPlayerBusCallback(GstBus *bus, GstMessage *msg, CGSTPlayer *gstplayer)
{
  INT_GST_VARS  *gstvars = gstplayer->GetGSTVars();
  GError *error    = NULL;
  gchar  *dbg_info = NULL;

  switch (GST_MESSAGE_TYPE(msg))
  {
    case GST_MESSAGE_EOS:
      gst_element_set_state(gstvars->player, GST_STATE_READY);
      g_main_loop_quit(gstvars_loop);
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
      gst_element_set_state(gstvars->player, GST_STATE_READY);
      g_main_loop_quit(gstvars_loop);
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
      }
      g_free(dbg_info);
      break;

    case GST_MESSAGE_TAG:
      {
        gchar *tag;
        GstTagList *tags;

        gst_message_parse_tag(msg, &tags);
        if (gst_tag_list_get_string(tags, GST_TAG_CONTAINER_FORMAT, &tag))
          g_print("GStreamer: Message TAG from %s, containter format:%s\n",
            GST_MESSAGE_SRC_NAME(msg), tag);
        else if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &tag))
          g_print("GStreamer: Message TAG from %s, language format:%s\n",
            GST_MESSAGE_SRC_NAME(msg), tag);
        else if (gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &tag))
          g_print("GStreamer: Message TAG from %s, video codec format:%s\n",
            GST_MESSAGE_SRC_NAME(msg), tag);
        else if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &tag))
          g_print("GStreamer: Message TAG from %s, audio codec format:%s\n",
            GST_MESSAGE_SRC_NAME(msg), tag);
        else if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &tag))
        {
          gstvars->video_title = tag;
          g_print("GStreamer: Message TAG from %s, show name:%s\n",
            GST_MESSAGE_SRC_NAME(msg), tag);
        }
        else
        {
          g_print("GStreamer: Message TAG from %s\n", GST_MESSAGE_SRC_NAME(msg));
        }
      }
      break;
    case GST_MESSAGE_BUFFERING:
      {
        gint percent = 0;
        gst_message_parse_buffering(msg, &percent);
        g_print("GStreamer: Buffering %d percent done)\n", percent);
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
#if defined(__APPLE__)
      else if (gst_structure_has_name(msg->structure, "have-ns-view"))
      {
        //NSView *nsview = NULL;
        void *nsview = NULL;
        nsview = g_value_get_pointer(gst_structure_get_value(msg->structure, "nsview"));
        // passes a GstGLView object to the callback, the object is an NSView
      }
#endif
      else if (gst_structure_has_name(msg->structure, "GstUDPSrcTimeout"))
      {
        gst_element_set_state(gstvars->player, GST_STATE_NULL);
        g_main_loop_quit(gstvars_loop);
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
      // ASYNC_DONE, we can query position, duration and other properties
      gstplayer->ProbeStreams();
      gstvars->rate  = 1.0;
      gstvars->ready = true;
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

// ****************************************************************
static void CGSTPlayerSubsOnNewBuffer(GstElement *subs_sink, CGSTPlayer *ctx)
{
  INT_GST_VARS  *gstvars = ctx->GetGSTVars();
  GstBuffer *buffer = NULL;

  // get the text buffer, note that it is not null terminated
  g_signal_emit_by_name(subs_sink, "pull-buffer", &buffer);
  if (buffer)
  {
    CSingleLock lock(gstvars->csection);

    // use timestamps instead of gst_element_query_position
    // as gstreamer sometimes does not update its internal 
    // position as rapidly as we want the values.
    gstvars->subtitle_end = XbmcThreads::SystemClockMillis() + 4000;

    guint8 *data = GST_BUFFER_DATA(buffer);
    guint   size = GST_BUFFER_SIZE(buffer);

    // sanity clamp the incoming text block
    if (data && size < 4096)
    {
      gstvars->subtitle_text = std::string((char*)data, size);
      // quirks
      gstvars->subtitle_text.Replace("&apos;","\'");
    }
    else
    {
      gstvars->subtitle_text = "";
    }
  }
}

/*
static void net_demuxer_padadded(GstElement *element, GstPad *pad, CGSTPlayer *ctx)
{
  // do this only once per pad
  if (gst_pad_is_linked(pad) == TRUE)
    return;

  // get capabilities
  GstCaps *caps = gst_pad_get_caps(pad);

  // get mime type
  const gchar *mime;
  mime = gst_structure_get_name(gst_caps_get_structure(caps, 0));

  g_print("Dynamic demuxer pad %s:%s created with mime-type %s\n",
    GST_OBJECT_NAME(element), GST_OBJECT_NAME(pad), mime);
  print_caps(caps);

  GstPad *sinkpad = NULL;
  if (g_strrstr(mime, "video"))
  {
    g_print ("Linking demuxer video...\n");
    if (g_strrstr(mime, "video/x-h264"))
    {
      // ismd_h264_viddec
    }
    else if (g_strrstr(mime, "video/mpeg"))
    {
      gint mpegversion = 0;
      GstStructure *structure = gst_caps_get_structure(caps, 0);
      gst_structure_get_int(structure, "mpegversion", &mpegversion);
      // mpegversion: 1, 2
      // ismd_mpeg2_viddec
      //
      // mpegversion: 4
      // video/x-xvid
      // video/x-divx
      // ismd_mpeg4_viddec
    }
    else if (g_strrstr(mime, "video/x-wmv"))
    {
      // ismd_vc1_viddec
    }
  }

  if (g_strrstr(mime, "audio"))
  {
    g_print ("Linking demuxer audio...\n");
  }

  if (g_strrstr(mime, "text"))
  {
    g_print ("Linking demuxer text...\n");
  }

  if (sinkpad)
  {
    if (!GST_PAD_IS_LINKED(sinkpad))
      gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
  }

  gst_caps_unref(caps);
}
*/

static void net_decoder_padadded(GstElement *element, GstPad *pad, CGSTPlayer *ctx)
{
  // do this only once per pad
  if (gst_pad_is_linked(pad) == TRUE)
    return;

  // get capabilities
  GstCaps *caps = gst_pad_get_caps(pad);

  // get mime type
  const gchar *mime;
  mime = gst_structure_get_name(gst_caps_get_structure(caps, 0));
  //g_print("Dynamic decoder pad added %s:%s created with mime-type %s\n",
  //  GST_OBJECT_NAME(element), GST_OBJECT_NAME(pad), mime);

  GstPad *sinkpad = NULL;
  INT_GST_VARS *gstvars = ctx->GetGSTVars();
  if (g_strrstr(mime, "video"))
  {
    gstvars->net_vbin  = gst_bin_new("vbin");
    GstElement *vqueue = gst_element_factory_make("queue", "vqueue");
    g_object_set(vqueue, "max-size-buffers", 3, NULL);
    gst_bin_add(GST_BIN(gstvars->net_vbin), vqueue);
    gstvars->videosink = gst_element_factory_make("ismd_vidrend_bin",  "vsink");
    g_object_set(gstvars->videosink, "qos", FALSE, NULL);
    gst_bin_add(GST_BIN(gstvars->net_vbin), gstvars->videosink);
    gst_element_link_many(vqueue, gstvars->videosink, NULL);
    //
    GstPad *vpad = gst_element_get_pad(vqueue, "sink");
    gst_element_add_pad(gstvars->net_vbin, gst_ghost_pad_new("sink", vpad));
    gst_object_unref(vpad);
    //
    gst_bin_add(GST_BIN(gstvars->player), gstvars->net_vbin);
    gst_element_sync_state_with_parent(gstvars->net_vbin);

    sinkpad = gst_element_get_static_pad(gstvars->net_vbin, "sink");
    gstvars->net_video = true;
  }

  if (g_strrstr(mime, "audio"))
  {
    gstvars->net_abin = gst_bin_new("abin");
    GstElement *aqueue = gst_element_factory_make("queue", "aqueue");
    gst_bin_add(GST_BIN(gstvars->net_abin), aqueue);
    GstElement *audiosink = gst_element_factory_make("ismd_audio_sink", NULL);
    gst_bin_add(GST_BIN(gstvars->net_abin), audiosink);
    gstvars->volume = audiosink;

    if (g_strrstr(mime, "audio/x-raw-float"))
    {
      GstElement *aconvert = gst_element_factory_make("audioconvert", NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), aconvert);
      gst_element_link_many(aqueue, aconvert, audiosink, NULL);
      gstvars->acodec_name.clear();
      gstvars->acodec_name.push_back("");
    }
    else if (g_strrstr(mime, "ac3") || g_strrstr(mime, "x-dd"))
    {
      GstElement *decoder  = gst_element_factory_make("a52dec", NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), decoder);
      GstElement *resample = gst_element_factory_make("audioresample", NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), resample);
      GstElement *convert  = gst_element_factory_make("audioconvert",  NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), convert);
      gst_element_link_many(aqueue, decoder, resample, convert, audiosink, NULL);
      
      gstvars->acodec_name.clear();
      gstvars->acodec_name.push_back("ac3");
    }
    else if (g_strrstr(mime, "dts"))
    {
      GstElement *decoder  = gst_element_factory_make("dtsdec", NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), decoder);
      GstElement *resample = gst_element_factory_make("audioresample", NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), resample);
      GstElement *convert  = gst_element_factory_make("audioconvert",  NULL);
      gst_bin_add(GST_BIN(gstvars->net_abin), convert);
      gst_element_link_many(aqueue, decoder, resample, convert, audiosink, NULL);
      gstvars->acodec_name.clear();
      gstvars->acodec_name.push_back("ac3");
    }
    else
    {
      gst_element_link(aqueue, audiosink);
      gstvars->acodec_name.clear();
      gstvars->acodec_name.push_back("aac");
    }
    //
    GstPad *apad = gst_element_get_pad(aqueue, "sink");
    gst_element_add_pad(gstvars->net_abin, gst_ghost_pad_new("sink", apad));
    gst_object_unref(apad);
    //
    gst_bin_add(GST_BIN(gstvars->player), gstvars->net_abin);
    gst_element_sync_state_with_parent(gstvars->net_abin);

    sinkpad = gst_element_get_static_pad(gstvars->net_abin, "sink");
    gstvars->net_audio = true;
  }

  if (g_strrstr(mime, "text"))
  {
    gstvars->textsink = gst_element_factory_make("appsink", "subtitle_sink");
    g_object_set(gstvars->textsink, "emit-signals", TRUE, NULL);
    g_signal_connect(gstvars->textsink, "new-buffer", G_CALLBACK(CGSTPlayerSubsOnNewBuffer), ctx);
    gst_bin_add(GST_BIN(gstvars->player), gstvars->textsink);
    //
    sinkpad = gst_element_get_static_pad(gstvars->textsink, "sink");
    gstvars->net_text = true;
  }

  if (sinkpad)
  {
    if (!GST_PAD_IS_LINKED(sinkpad))
      gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
  }

  gst_caps_unref(caps);
}

static void net_decoder_padremoved(GstElement *element, GstPad *pad, CGSTPlayer *ctx)
{
  INT_GST_VARS *gstvars = ctx->GetGSTVars();

  if (gstvars->net_vbin)
  {
    //g_print ("Unlinking net_vbin...\n");
    GstPad *sinkpad = gst_element_get_static_pad(gstvars->net_vbin, "sink");
    if (sinkpad && GST_PAD_IS_LINKED(sinkpad))
    {
      gst_pad_unlink(pad, sinkpad);
      return;
    }
  }

  if (gstvars->net_abin)
  {
    //g_print ("Unlinking net_abin...\n");
    GstPad *sinkpad = gst_element_get_static_pad(gstvars->net_abin, "sink");
    if (sinkpad && GST_PAD_IS_LINKED(sinkpad))
      gst_pad_unlink(pad, sinkpad);
  }
  
  // text will take care of itself as gstvars->player owns it

}
// ****************************************************************
// ****************************************************************
CGSTPlayer::CGSTPlayer(IPlayerCallback &callback) 
  : IPlayer(callback),
  CThread("CGSTPlayer"),
  m_ready(true)
{
  m_speed = 1;
  m_paused = false;
  m_StopPlaying = false;

  m_gstvars = (INT_GST_VARS*)new INT_GST_VARS;
  m_gstvars->inited = false;
  m_gstvars->ready  = false;
  m_gstvars->rate   = 1.0;
  m_gstvars->player = NULL;
  m_gstvars->volume = NULL;
  m_gstvars->textsink  = NULL;
  m_gstvars->videosink = NULL;
  m_gstvars->subtitle_end = 0;
  m_gstvars->flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_TEXT;
  m_gstvars->is_udp = false;
  m_gstvars->is_rtmp = false;
  m_gstvars->net_vbin   = NULL;
  m_gstvars->net_abin   = NULL;
  m_gstvars->net_source = NULL;;
  m_gstvars->net_queue  = NULL;;
  m_gstvars->net_clkrecover = NULL;;
  m_gstvars->net_typefind   = NULL;;
  m_gstvars->net_decoder    = NULL;;
  m_gstvars->net_video = false;
  m_gstvars->net_audio = false;
  m_gstvars->net_text  = false;
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

    std::string url;

    m_item = file;
    m_options = options;
    m_StopPlaying = false;

    m_elapsed_ms  = 0;
    m_duration_ms = 0;

    m_audio_index = 0;
    m_audio_count = 0;
    m_audio_bits  = 0;
    m_audio_channels = 0;
    m_audio_samplerate = 0;
    m_audio_offset_ms = g_settings.m_currentVideoSettings.m_AudioDelay;

    m_video_index = 0;
    m_video_count = 0;
    m_video_fps   = 0.0;
    m_video_width = 0;
    m_video_height= 0;

    m_subtitle_index = 0;
    m_subtitle_count = 0;
    m_subtitle_show  = g_settings.m_currentVideoSettings.m_SubtitleOn;
    m_chapter_count  = 0;
    m_subtitle_offset_ms = g_settings.m_currentVideoSettings.m_SubtitleDelay;

    m_gstvars->appsrc = NULL;
    m_gstvars->volume = NULL;
    m_gstvars->textsink  = NULL;
    m_gstvars->videosink = NULL;
    m_gstvars->is_udp = false;
    m_gstvars->is_rtmp = false;
    m_gstvars->net_vbin   = NULL;
    m_gstvars->net_abin   = NULL;
    m_gstvars->net_source = NULL;;
    m_gstvars->net_queue  = NULL;;
    m_gstvars->net_clkrecover = NULL;;
    m_gstvars->net_typefind   = NULL;;
    m_gstvars->net_decoder    = NULL;;
    m_gstvars->net_video = false;
    m_gstvars->net_audio = false;
    m_gstvars->net_text  = false;

    // create a gst main loop
    if (gstvars_loop == NULL)
      gstvars_loop = g_main_loop_new(NULL, FALSE);
    if (gstvars_loop == NULL)
      return false;

    if (m_item.m_strPath.Left(6).Equals("udp://") || 
        m_item.m_strPath.Left(7).Equals("rtmp://"))
    {
      // udp playback in gstreamer requires constructing a pipeline
      // with a udpsrc, queue, ismd_clock_recovery_provider and decodebin2.
      // we hook the "pad-added" callback and create and attach the final
      // audio, video and subtitle elements.
      url = m_item.m_strPath;
      if (!gst_uri_is_valid(url.c_str()))
        return false;

      if (m_item.m_strPath.Left(6).Equals("udp://"))
      {
        m_gstvars->player       = gst_pipeline_new("gstplayer-udp");
        m_gstvars->net_source   = gst_element_factory_make("udpsrc", "source");
        g_object_set(m_gstvars->net_source, "uri", url.c_str(), NULL);
        guint64 timeout = 4 * 1e6;
        g_object_set(m_gstvars->net_source, "timeout", timeout, NULL);
        m_gstvars->is_udp = true;
      }
      else if (m_item.m_strPath.Left(7).Equals("rtmp://"))
      {
        m_gstvars->player       = gst_pipeline_new("gstplayer-rtmp");
        m_gstvars->net_source   = gst_element_factory_make("rtmpsrc", "source");
        g_object_set(m_gstvars->net_source, "location", url.c_str(), NULL);
        m_gstvars->is_rtmp = true;
      }
      //
      m_gstvars->net_queue    = gst_element_factory_make("queue", "udpqueue");
      g_object_set(m_gstvars->net_queue, "max-size-time", 0, "max-size-buffers", 0, NULL);
      //
      m_gstvars->net_clkrecover = gst_element_factory_make("ismd_clock_recovery_provider", "clkrecover");
      //
      m_gstvars->net_typefind = gst_element_factory_make("typefind", "typefind");

      m_gstvars->net_decoder  = gst_element_factory_make("decodebin2", "decoder");
      GstCaps *decoder_caps   = gst_caps_from_string("video/x-decoded-ismd;video/x-raw-yuv"
        ";audio/mpeg;audio/x-mpeg;audio/x-aac"
        ";audio/x-raw-int;audio/x-private1-lpcm"
        ";audio/x-raw-float"
        ";audio/x-ac3;audio/x-private1-ac3;audio/x-dd;audio/x-ddplus"
        ";audio/x-dts;audio/x-private1-dts"
        ";text/plain;text/x-pango-markup");
      g_object_set(m_gstvars->net_decoder, "caps", decoder_caps, NULL);
      gst_caps_unref(decoder_caps);
      g_object_set(m_gstvars->net_decoder, "max-size-bytes", 65536, NULL);

      gst_bin_add_many(GST_BIN(m_gstvars->player),
        m_gstvars->net_source,
        m_gstvars->net_queue, 
        m_gstvars->net_clkrecover, 
        m_gstvars->net_typefind, 
        m_gstvars->net_decoder, NULL);
      // link together - note that we cannot link the decoder and
      // sink yet, because the decoder uses dynamic pads. For that,
      // we set a pad-added signal handler.
      gst_element_link_many(m_gstvars->net_source,
        m_gstvars->net_queue,
        m_gstvars->net_clkrecover,
        m_gstvars->net_typefind,
        m_gstvars->net_decoder, NULL);
      g_signal_connect(m_gstvars->net_decoder, "pad-added", G_CALLBACK(net_decoder_padadded), this);
      g_signal_connect(m_gstvars->net_decoder, "pad-removed", G_CALLBACK(net_decoder_padremoved), this);
      /*
      g_print("net_source GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_source));
      g_print("net_queue  GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_queue ));
      g_print("net_clkrecover GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_clkrecover));
      g_print("net_typefind GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_typefind));
      g_print("net_decoder GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_decoder));
      */
      CLog::Log(LOGNOTICE, "CGSTPlayer: Opening: URL=%s", url.c_str());
    }
    else
    {
      if (m_item.m_strPath.Left(7).Equals("rtsp://"))
      {
        // protocol goes to gstreamer as is
        url = m_item.m_strPath;
        if (!gst_uri_is_valid(url.c_str()))
          return false;
      }
      else if (m_item.m_strPath.Left(7).Equals("http://"))
      {
      // strip user agent that we append
        url = m_item.m_strPath;
        size_t pos = url.rfind('|');
        if (pos > string::npos)
          url = url.erase(pos, url.size());
        if (!gst_uri_is_valid(url.c_str()))
          return false;
      }
      else
      {
        m_gstvars->appsrc = new CGSTAppsrc(m_item.m_strPath);
      }

      // playbin2 will figure out elements for audio automatically
      // but we need to assist with video and subtitles elements.
      m_gstvars->player = gst_element_factory_make( "playbin2", "gstplayer");

      // ---------------------------------------------------
      if (m_item.IsVideo())
      {
        // create subtitle sink
        m_gstvars->textsink = gst_element_factory_make("appsink", "subtitle_sink");
        g_object_set(m_gstvars->textsink, "emit-signals", TRUE, NULL);
        g_signal_connect(m_gstvars->textsink, "new-buffer", G_CALLBACK(CGSTPlayerSubsOnNewBuffer), this);
        GstCaps *subcaps = gst_caps_from_string("text/plain;text/x-pango-markup");
        g_object_set(m_gstvars->textsink, "caps", subcaps, NULL);
        gst_caps_unref(subcaps);
        g_object_set(m_gstvars->player, "text-sink", m_gstvars->textsink, NULL);
      }
      else
      {
        // Expected delay needed for elements to spin up to PLAYING in nanoseconds
        // Resolves 1st 0.5 seconds of audio getting cut off on music files.
        guint64 delay = 550000000;
        g_object_set(m_gstvars->player, "delay", delay, NULL);
      }

      // set the player url and change state to paused (from null)
      if (m_gstvars->appsrc)
      {
        g_object_set(m_gstvars->player, "uri", "appsrc://", NULL);
        g_signal_connect(m_gstvars->player, "deep-notify::source",
          (GCallback)m_gstvars->appsrc->FoundSource, m_gstvars->appsrc);
      }
      else
      {
        g_object_set(m_gstvars->player, "uri", url.c_str(), NULL);
        CLog::Log(LOGNOTICE, "CGSTPlayer: Opening: URL=%s", url.c_str());
      }

#if 0
      // disable the mpeg4 ISMD H/W decoder. FFMPEG plugins will need to be used instead.
      // The hardware decoder produces very choppy video that appears as though its framerate is wrong
      GstPluginFeature *hw_mpeg4_decoder = gst_registry_find_feature(gst_registry_get_default(),
        "ismd_mpeg4_viddec", GST_TYPE_ELEMENT_FACTORY);
      if (hw_mpeg4_decoder)
      {
        gst_plugin_feature_set_rank(hw_mpeg4_decoder, GST_RANK_PRIMARY-3);
        gst_object_unref(hw_mpeg4_decoder);
      }
#endif
#if 1
      // Lower the priority of the mp3 hardware decoder by instead using Fluendo's plugin if it exists.
      // The ISMD decoder produces very coarse timestamps, which manifests by sometimes showing 2-sec
      // Jumps in time.

      GstPluginFeature *sw_mp3_decoder = gst_registry_find_feature(gst_registry_get_default(),
        "flump3dec", GST_TYPE_ELEMENT_FACTORY);
      if (sw_mp3_decoder)
      {
        gst_plugin_feature_set_rank(sw_mp3_decoder, GST_RANK_PRIMARY+3);
        gst_object_unref(sw_mp3_decoder);
      }
#endif

      // setup playbin2 playback flags, generally we accept the default flags and
      // enable GST_PLAY_FLAG_NATIVE_VIDEO so we only accept formats that we can handle
      // natively (without converting). 
      g_object_get(m_gstvars->player, "flags", &m_gstvars->flags, NULL);
      m_gstvars->flags |= GST_PLAY_FLAG_NATIVE_VIDEO;
      g_object_set(G_OBJECT(m_gstvars->player), "flags", m_gstvars->flags, NULL);
    }

    // get the bus and setup the bus callback
    m_gstvars->bus = gst_pipeline_get_bus(GST_PIPELINE(m_gstvars->player));
    if (m_gstvars->bus  == NULL)
      return false;
    gst_bus_add_watch(m_gstvars->bus, (GstBusFunc)CGSTPlayerBusCallback, this);

    // sequence gst pipeline into paused
    gst_element_set_state(m_gstvars->player, GST_STATE_NULL);
    m_gstvars->inited = true;
    gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);

    m_ready.Reset();
    Create();
    if (!m_ready.WaitMSec(100))
    {
      CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
      dialog->Show();
      while(!m_ready.WaitMSec(1))
        g_windowManager.ProcessRenderLoop(false);
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

  GSTShutdown();

  m_StopPlaying = true;

  CLog::Log(LOGDEBUG, "CGSTPlayer: waiting for threads to exit");
  // wait for the main thread to finish up
  // since this main thread cleans up all other resources and threads
  // we are done after the StopThread call
  StopThread();
  
  if (m_gstvars->appsrc)
  {
    delete m_gstvars->appsrc;
    m_gstvars->appsrc = NULL;
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
  CSingleLock lock(m_csection);

  if (m_StopPlaying)
    return;

  if (m_gstvars->is_udp)
    return;

  if (m_paused == true)
  {
    if (m_gstvars->ready)
    {
      CSingleLock lock(m_gstvars->csection);
      gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
      gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    }
    m_callback.OnPlayBackResumed();
  }
  else
  {
    if (m_gstvars->ready)
    {
      CSingleLock lock(m_gstvars->csection);
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
  return (m_video_count > 0) || m_gstvars->net_video;
}

bool CGSTPlayer::HasAudio() const
{
  return (m_audio_count > 0) || m_gstvars->net_audio;
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

  // update m_elapsed_ms and m_duration_ms.
  GetTime();
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
  // update m_elapsed_ms and m_duration_ms.
  GetTime();
  GetTotalTime();

  fPercent /= 100.0f;
  fPercent += (float)m_elapsed_ms/(float)m_duration_ms;
  // convert to milliseconds
  int64_t seek_ms = m_duration_ms * fPercent;
  SeekTime(seek_ms);
}

float CGSTPlayer::GetPercentage()
{
  // update m_elapsed_ms and m_duration_ms.
  GetTime();
  GetTotalTime();
  if (m_duration_ms)
    return 100.0f * (float)m_elapsed_ms/(float)m_duration_ms;
  else
    return 0.0f;
}

float CGSTPlayer::GetCachePercentage()
{
  CSingleLock lock(m_csection);
  return std::min(100.0, (double)(GetPercentage() + GetCacheLevel()));
}

void CGSTPlayer::SetAVDelay(float fValue)
{
  // time offset in seconds of audio with respect to video
  m_audio_offset_ms = fValue * 1e3;
  if (m_gstvars->ready && !(m_gstvars->is_udp || m_gstvars->is_rtmp))
  {
    // av-offset in nanoseconds
    gint64 offset_ns = m_audio_offset_ms * 1e6;
    g_object_set(m_gstvars->player, "av-offset", offset_ns, NULL);
  }
}

float CGSTPlayer::GetAVDelay()
{
  return m_audio_offset_ms;
}

void CGSTPlayer::SetSubTitleDelay(float fValue)
{
  // time offset in seconds of subtitle with respect to playback
  m_subtitle_offset_ms = fValue * 1e3;
  if (m_gstvars->ready && m_gstvars->textsink)
  {
    // timestamp offset in nanoseconds
    gint64 offset_ns = m_subtitle_offset_ms * 1e6;
    g_object_set(m_gstvars->textsink, "ts-offset", -offset_ns, NULL);
  }
}

float CGSTPlayer::GetSubTitleDelay()
{
  return m_subtitle_offset_ms;
}

void CGSTPlayer::SetVolume(long nVolume)
{
  // nVolume is a milliBels from -6000 (-60dB or mute) to 0 (0dB or full volume)
  CSingleLock lock(m_csection);

  float volume;
  if (nVolume == -6000) {
    // We are muted
    volume = 0.0f;
  } else {
    volume = (double)nVolume / -10000.0f;
    // Convert what XBMC gives into 0.0 -> 1.0 scale playbin2 uses
    volume = ((1 - volume) - 0.4f) * 1.6666f;
  }
  if (m_gstvars->ready)
  {
    CSingleLock lock(m_gstvars->csection);
    g_object_set(m_gstvars->volume, "volume", volume, NULL);
  }
}

void CGSTPlayer::GetAudioInfo(CStdString &strAudioInfo)
{
  CSingleLock lock(m_gstvars->csection);
  m_audio_info.Format("Audio stream (%d) [%s-%s]",
    m_audio_index, m_gstvars->acodec_name[m_audio_index], m_gstvars->acodec_language[m_audio_index]);
  strAudioInfo = m_audio_info;
}

void CGSTPlayer::GetVideoInfo(CStdString &strVideoInfo)
{
  CSingleLock lock(m_gstvars->csection);
  m_video_info.Format("Video stream (%d) [%s-%s]",
    m_video_index, m_gstvars->vcodec_name[m_video_index], m_gstvars->vcodec_language[m_video_index]);
  strVideoInfo = m_video_info;
}

void CGSTPlayer::GetGeneralInfo(CStdString& strGeneralInfo)
{
  //CLog::Log(LOGDEBUG, "CGSTPlayer::GetGeneralInfo");
}

int CGSTPlayer::GetAudioStreamCount()
{
  return m_audio_count;
}

int CGSTPlayer::GetAudioStream()
{
	return m_audio_index;
}

void CGSTPlayer::GetAudioStreamName(int iStream, CStdString &strStreamName)
{
  CSingleLock lock(m_gstvars->csection);
  
  strStreamName = m_gstvars->acodec_name[iStream];
}
 
void CGSTPlayer::SetAudioStream(int SetAudioStream)
{
  if (m_gstvars->ready)
  {
    if (m_audio_count)
    {
      CSingleLock lock(m_gstvars->csection);
      m_audio_index = SetAudioStream;
      g_object_set(m_gstvars->player, "current-audio", m_audio_index, NULL);
    }
  }
}

int CGSTPlayer::GetSubtitleCount()
{
	return m_subtitle_count;
}

int CGSTPlayer::GetSubtitle()
{
	return m_subtitle_index;
}

void CGSTPlayer::GetSubtitleName(int iStream, CStdString &strStreamName)
{
  CSingleLock lock(m_gstvars->csection);
  strStreamName = m_gstvars->tcodec_language[iStream];
}

void CGSTPlayer::SetSubtitle(int iStream)
{
  if (m_gstvars->ready)
  {
    if (m_subtitle_count)
    {
      CSingleLock lock(m_gstvars->csection);
      m_subtitle_index = iStream;
      g_object_set(m_gstvars->player, "current-text", m_subtitle_index, NULL);
    }
  }
}

bool CGSTPlayer::GetSubtitleVisible()
{
  return m_subtitle_show;
}

void CGSTPlayer::SetSubtitleVisible(bool bVisible)
{
  m_subtitle_show = bVisible;
  g_settings.m_currentVideoSettings.m_SubtitleOn = bVisible;

  if (m_gstvars->ready)
  {
    if (m_subtitle_count)
    {
      CSingleLock lock(m_gstvars->csection);
      g_object_set(m_gstvars->player, "current-text", m_subtitle_index, NULL);
    }
  }
}

int CGSTPlayer::AddSubtitle(const CStdString& strSubPath)
{
  // not sure we can add a subtitle file on the fly.
  //g_print("CGSTPlayer::AddSubtitle(%s)\n", strSubPath.c_str());
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

void CGSTPlayer::SetVideoRect(const CRect &SrcRect, const CRect &DestRect)
{
  // check if destination rect or video view mode has changed
  if ((m_dst_rect != DestRect) || (m_view_mode != g_settings.m_currentVideoSettings.m_ViewMode))
  {
    m_dst_rect  = DestRect;
    m_view_mode = g_settings.m_currentVideoSettings.m_ViewMode;
  }
  else
  {
    return;
  }

  // on cex, when running 1080p display size, 
  // gui is at 720p but m_dst_rect is relative to gui.
  // need to scale up m_dst_rect to display size as video decodes
  // to separate video plane that is at display size.
  CRect gui, display, dst_rect;
  RESOLUTION res = g_graphicsContext.GetVideoResolution();
  gui.SetRect(0, 0, g_settings.m_ResInfo[res].iWidth, g_settings.m_ResInfo[res].iHeight);
  display.SetRect(0, 0, g_settings.m_ResInfo[res].iScreenWidth, g_settings.m_ResInfo[res].iScreenHeight);
  
  dst_rect = m_dst_rect;
  if (gui != display)
  {
    float xscale = display.Width()  / gui.Width();
    float yscale = display.Height() / gui.Height();
    dst_rect.x1 *= xscale;
    dst_rect.x2 *= xscale;
    dst_rect.y1 *= yscale;
    dst_rect.y2 *= yscale;
  }

  // ismd destination rectangle cannot be outside display bounds
  // or it will output black frames when updating GDL_PLANE_DST_RECT fails
  if (!display.PtInRect(CPoint(dst_rect.x1, dst_rect.y1)))
    return;
  if (!display.PtInRect(CPoint(dst_rect.x2, dst_rect.y2)))
    return;

  if (m_gstvars->videosink)
  {
    CStdString rectangle;
    rectangle.Format("%i,%i,%i,%i",
      (int)dst_rect.x1, (int)dst_rect.y1,
      (int)dst_rect.Width(), (int)dst_rect.Height());
    // setup the scaling mode (we might need to tweak these settings)
    ISMD_SCALE_MODE scale_mode;
    switch(m_view_mode)
    {
      default:
      case VIEW_MODE_NORMAL:
      case VIEW_MODE_CUSTOM:
      case VIEW_MODE_STRETCH_4x3:
      case VIEW_MODE_STRETCH_16x9:
        scale_mode = ISMD_SCALE_TO_FIT;
      break;
      case VIEW_MODE_ZOOM:
      case VIEW_MODE_WIDE_ZOOM:
        scale_mode = ISMD_ZOOM_TO_FILL;
      break;
      case VIEW_MODE_ORIGINAL:
        scale_mode = ISMD_VPP_NO_SCALING;
      break;
    }
    // update ismd_vidrend_bin with new settings.
    g_object_set(m_gstvars->videosink, "scale-mode", scale_mode, "rectangle",  rectangle.c_str(), NULL);
  }
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
  if (m_gstvars->is_udp)
    return;

  if (m_gstvars->ready)
  {
    CSingleLock lock(m_gstvars->csection);

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
    gint64 seek_ns = seek_ms * 1e6;
    // Performing a non-flushing seek in a PAUSED pipeline
    // blocks until the pipeline is set to playing again.
    gst_element_seek(m_gstvars->player,
			m_gstvars->rate, GST_FORMAT_TIME,
      //(GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_SEGMENT),
      (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
			GST_SEEK_TYPE_SET,  seek_ns,  // start
      GST_SEEK_TYPE_SET,   -1);     // end
    // wait for seek to complete
    gst_element_get_state(m_gstvars->player, NULL, NULL, 200 * GST_MSECOND);
    // restart the playback
    gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
    // remove any displaying subtitles
    m_gstvars->subtitle_end = 0;
  }

  m_callback.OnPlayBackSeek((int)seek_ms, (int)(seek_ms - m_elapsed_ms));
}

__int64 CGSTPlayer::GetTime()
{
  if (m_gstvars->ready)
  {
    CSingleLock lock(m_gstvars->csection);

    gint64 elapsed_ns;
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_position(m_gstvars->player, &fmt, &elapsed_ns))
      m_elapsed_ms = elapsed_ns / 1e6;
  }
  return m_elapsed_ms;
}

int CGSTPlayer::GetTotalTime()
{
  if (!m_duration_ms && m_gstvars->ready)
  {
    CSingleLock lock(m_gstvars->csection);

    gint64 duration_ns = 0;
    GstFormat fmt = GST_FORMAT_TIME;
    if (gst_element_query_duration(m_gstvars->player, &fmt, &duration_ns))
      m_duration_ms = duration_ns / 1e6;
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
  if (m_audio_count && m_audio_index >= 0)
  {
    CSingleLock lock(m_gstvars->csection);
    return m_gstvars->acodec_name[m_audio_index];
  }
  else
  {
    return "";
  }
}

CStdString CGSTPlayer::GetVideoCodecName()
{
  if (m_video_count && m_video_index >= 0)
  {
    CSingleLock lock(m_gstvars->csection);
    return m_gstvars->vcodec_name[m_video_index];
  }
  else
  {
    return "";
  }
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

  if (m_gstvars->is_udp)
    return;

  if (m_speed != iSpeed)
  {
    /*
    CSingleLock lock(m_gstvars->csection);

    if (m_gstvars->ready)
    {
      if (!m_paused)
      {
        gst_element_set_state(m_gstvars->player, GST_STATE_PAUSED);
        gst_element_get_state(m_gstvars->player, NULL, NULL, 200 * GST_MSECOND);
      }
    
      gint64 elapsed_ns;
      GstFormat seek_fmt = GST_FORMAT_TIME;
      if (!gst_element_query_position(m_gstvars->player, &seek_fmt, &elapsed_ns))
        elapsed_ns = GST_CLOCK_TIME_NONE;

      // gst can only do -8X to 8X
      m_gstvars->rate = iSpeed;
      if (iSpeed < 0)
      {
        if (m_gstvars->rate < -8.0)
          m_gstvars->rate = -8.0;
        gst_element_seek(m_gstvars->player, m_gstvars->rate,
          seek_fmt, GST_SEEK_FLAG_FLUSH,
          GST_SEEK_TYPE_SET, elapsed_ns,
          GST_SEEK_TYPE_SET, -1);
      }
      else
      {
        if (m_gstvars->rate > 8.0)
          m_gstvars->rate = 8.0;
        gst_element_seek(m_gstvars->player, m_gstvars->rate,
          seek_fmt, GST_SEEK_FLAG_FLUSH,
          GST_SEEK_TYPE_SET, elapsed_ns,
          GST_SEEK_TYPE_SET, -1);
      }
      
      if (!m_paused)
      {
        gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
        gst_element_get_state(m_gstvars->player, NULL, NULL, 200 * GST_MSECOND);
      }
    }
    */
    m_speed = iSpeed;
  }
}

bool CGSTPlayer::GetCurrentSubtitle(CStdString& strSubtitle)
{
  strSubtitle = "";

  if (m_gstvars->ready)
  {
    CSingleLock lock(m_gstvars->csection);

    if (m_gstvars->subtitle_end < XbmcThreads::SystemClockMillis())
      m_gstvars->subtitle_text = "";
    
    strSubtitle = m_gstvars->subtitle_text;
  }

  return !strSubtitle.IsEmpty();
}
  
CStdString CGSTPlayer::GetPlayerState()
{
  return "";
}

bool CGSTPlayer::SetPlayerState(CStdString state)
{
  return false;
}

CStdString CGSTPlayer::GetPlayingTitle()
{
  return m_gstvars->video_title;
}

int CGSTPlayer::GetCacheLevel() const
{
  if (m_gstvars->appsrc)
    return m_gstvars->appsrc->CacheLevel(m_duration_ms) * 100;
  else
    return 0;
}

void CGSTPlayer::OnStartup()
{
  g_renderManager.PreInit();
}

void CGSTPlayer::OnExit()
{
  //CLog::Log(LOGNOTICE, "CGSTPlayer::OnExit()");
  usleep(100000);
  
  GSTShutdown();

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
      gst_element_set_state(m_gstvars->player, GST_STATE_PLAYING);
    }
    else
    {
      CLog::Log(LOGDEBUG, "CGSTPlayer::Process:WaitForGSTPaused timeout");
      goto do_exit;
    }

    if (WaitForGSTPlaying(4000))
    {
      // starttime has units of seconds (SeekTime will start playback)
      if (m_options.starttime > 0)
        SeekTime(m_options.starttime * 1000);
      SetVolume(g_settings.m_nVolumeLevel);
      SetAVDelay(m_audio_offset_ms);
      SetSubTitleDelay(m_subtitle_offset_ms);
        
      // we are done initializing now, set the readyevent which will
      // drop CGUIDialogBusy, and release the hold in OpenFile
      m_ready.Set();

      if (m_video_count || m_gstvars->net_video)
      {
        unsigned int flags = 0;
        flags |= CONF_FLAGS_FORMAT_BYPASS;
        flags |= CONF_FLAGS_FULLSCREEN;
        CLog::Log(LOGDEBUG,"%s - change configuration. %dx%d. framerate: %4.2f. format: BYPASS",
          __FUNCTION__, m_video_width, m_video_height, m_video_fps);

        if(!g_renderManager.Configure(m_video_width, m_video_height,
          m_video_width, m_video_height, m_video_fps, flags))
        {
          CLog::Log(LOGERROR, "%s - failed to configure renderer", __FUNCTION__);
        }
        if (!g_renderManager.IsStarted())
        {
          CLog::Log(LOGERROR, "%s - renderer not started", __FUNCTION__);
        }
      }
    }
    else
    {
      CLog::Log(LOGERROR, "CGSTPlayer::Process: WaitForGSTPlaying() failed");
      goto do_exit;
    }

    if (m_options.identify == false)
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
    g_main_loop_run(gstvars_loop);
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "CGSTPlayer::Process: Exception thrown");
  }

do_exit:
  m_ready.Set();

}

void CGSTPlayer::ProbeStreams()
{
  if (m_gstvars->is_udp || m_gstvars->is_rtmp)
  {
    ProbeUDPStreams();
    return;
  }

  // find playbin2's video-sink and setup some handling
  if (m_item.IsVideo() && !m_gstvars->videosink)
  {
    g_object_get(m_gstvars->player, "video-sink", &m_gstvars->videosink, NULL);
    //
    gchar *elementname = gst_element_get_name(m_gstvars->videosink);
    if (g_strrstr(elementname, "ismdgstvidrendbin"))
    {
      // turn off qos for video sink, breaks seeking in mkv containers
      g_object_set(m_gstvars->videosink, "qos", FALSE, NULL);
    }
    g_free(elementname);
    #if defined(__APPLE__)
    // When the NSView to be embedded is created an element #GstMessage with a
    //  name of 'have-ns-view' will be created and posted on the bus.
    //  The pointer to the NSView to embed will be in the 'nsview' field of that
    //  message. The application MUST handle this message and embed the view
    //  appropriately.
    g_object_set(m_gstvars->videosink, "embed", true, NULL);
    #endif
  }
  // grab a copy of player element ptr for setting volume.
  m_gstvars->volume = m_gstvars->player;

  // now let's see what we have in terms of audio, video and subtitles
  m_audio_index = 0;
  m_video_index = 0;
  m_subtitle_index = 0;

  g_object_get(G_OBJECT(m_gstvars->player),
    "n-audio", &m_audio_count, "n-video", &m_video_count, "n-text", &m_subtitle_count, NULL);

  if (m_video_count)
    g_object_set(m_gstvars->player, "current-video", m_video_index, NULL);
  if (m_audio_count)
    g_object_set(m_gstvars->player, "current-audio", m_audio_index, NULL);
  if (m_subtitle_count)
    g_object_set(m_gstvars->player, "current-text", m_subtitle_index, NULL);

  // probe video
  m_gstvars->vcodec_name.clear();
  for (int i = 0; i < m_video_count; i++)
  { 
    GstPad *pad = NULL;
    g_signal_emit_by_name(m_gstvars->player, "get-video-pad", i, &pad, NULL);
    if (pad)
    {
      GstCaps *caps = gst_pad_get_negotiated_caps(pad); 
      if (caps)
      {
        GstStructure *structure = gst_caps_get_structure(caps, 0);

        gint video_fps_d, video_fps_n, video_width, video_height;
        gint video_par_d, video_par_n;
        if (!gst_structure_get_fraction(structure, "pixel-aspect-ratio", &video_par_n, &video_par_d))
        {
          video_par_n = 1;
          video_par_d = 1;
        }
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
          m_video_width  = video_width * (float)video_par_n/(float)video_par_d;
          m_video_height = video_height;
        }
        gst_caps_unref(caps);
      }
      gst_object_unref(pad);
    }

    GstTagList *tags = NULL;
    g_signal_emit_by_name(m_gstvars->player, "get-video-tags", i, &tags);
    if (tags)
    {
      gchar *language = NULL, *codec = NULL;
      gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &language);
      gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC,   &codec);
      gst_tag_list_free(tags);

      if (codec)
        m_gstvars->vcodec_name.push_back(gstTagNameToVideoName(codec));
      else
        m_gstvars->vcodec_name.push_back("");
      if (language)
        m_gstvars->vcodec_language.push_back(gst_tag_get_language_name(language));
      else
        m_gstvars->vcodec_language.push_back("");
    }
    else
    {
      m_gstvars->vcodec_name.push_back("unknown");
      m_gstvars->vcodec_language.push_back("");
    }
  }

  // probe audio
  m_gstvars->acodec_name.clear();
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
          m_audio_samplerate = samplerate;
        }
        gst_caps_unref(caps);
      }
      gst_object_unref(pad);
    }
    
    GstTagList *tags = NULL;
    g_signal_emit_by_name(m_gstvars->player, "get-audio-tags", i, &tags);
    if (tags)
    {
      gchar *language = NULL, *codec = NULL;
      gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &language);
      gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC,   &codec);
      gst_tag_list_free(tags);

      if (codec)
        m_gstvars->acodec_name.push_back(gstTagNameToAudioName(codec));
      else
        m_gstvars->acodec_name.push_back("");
      if (language)
        m_gstvars->acodec_language.push_back(gst_tag_get_language_name(language));
      else
        m_gstvars->acodec_language.push_back("");
    }
    else
    {
      m_gstvars->acodec_name.push_back("unknown");
      m_gstvars->acodec_language.push_back("");
    }
  }

  // probe subtitles
  m_gstvars->tcodec_name.clear();
  for (int i = 0; i < m_subtitle_count; i++)
  {
    GstTagList *tags = NULL;
    g_signal_emit_by_name(m_gstvars->player, "get-text-tags", i, &tags);
    if (tags)
    {
      gchar *language = NULL, *codec = NULL;

      gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE,  &language);
      gst_tag_list_get_string(tags, GST_TAG_SUBTITLE_CODEC, &codec);
      gst_tag_list_free(tags);

      m_gstvars->tcodec_name.push_back("");
      if (language)
        m_gstvars->tcodec_language.push_back(gst_tag_get_language_name(language));
      else
        m_gstvars->tcodec_language.push_back("");

    }
    else
    {
      m_gstvars->tcodec_name.push_back("unknown");
      m_gstvars->tcodec_language.push_back("");
    }
  }
}

void CGSTPlayer::GetLastFrame()
{
  CGSTFileInfo::ExtractSnapshot();
}

void CGSTPlayer::GSTShutdown(void)
{
  if (m_gstvars->inited)
  {
    m_gstvars->rate   = 1.0;
    m_gstvars->ready  = false;
    m_gstvars->inited = false;
    m_gstvars->volume = NULL;
    // unref the videosink object that we got from async-done
    // or we hold open the hw decoder/renderer.
    if (m_gstvars->videosink && !(m_gstvars->is_udp || m_gstvars->is_rtmp))
      gst_object_unref(m_gstvars->videosink);
    m_gstvars->videosink = NULL;

    gst_element_set_state(m_gstvars->player, GST_STATE_NULL);
    gst_element_get_state(m_gstvars->player, NULL, NULL, 100 * GST_MSECOND);
    g_main_loop_quit(gstvars_loop);

    gst_object_unref(m_gstvars->bus);
    m_gstvars->bus = NULL;

    gst_object_unref(m_gstvars->player);
    m_gstvars->player = NULL;

    if (m_gstvars->is_udp || m_gstvars->is_rtmp)
    {
      /*
      if (m_gstvars->net_vbin)
        g_print("net_vbin GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_vbin));
      if (m_gstvars->net_abin)
        g_print("net_abin GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_abin));
      g_print("net_source GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_source));
      g_print("net_queue  GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_queue ));
      g_print("net_clkrecover GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_clkrecover));
      g_print("net_typefind GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_typefind));
      g_print("net_decoder GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_decoder));
      */
      // Why do I have to manually delete these elements, they
      // were gst_bin_add_many to player and should auto-delete.
      if (m_gstvars->net_vbin)
      {
        really_unref(m_gstvars->net_vbin);
        //g_print("net_vbin GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_vbin));
        m_gstvars->net_vbin = NULL;
      }
      if (m_gstvars->net_abin)
      {
        really_unref(m_gstvars->net_abin);
        //g_print("net_abin GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_abin));
        m_gstvars->net_abin = NULL;
      }
      if (m_gstvars->net_source)
        really_unref(m_gstvars->net_source);
      if (m_gstvars->net_queue)
        really_unref(m_gstvars->net_queue);
      if (m_gstvars->net_clkrecover)
        really_unref(m_gstvars->net_clkrecover);
      if (m_gstvars->net_typefind)
        really_unref(m_gstvars->net_typefind);
      if (m_gstvars->net_decoder)
        really_unref(m_gstvars->net_decoder);

      /*
      g_print("net_source GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_source));
      g_print("net_queue  GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_queue ));
      g_print("net_clkrecover GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_clkrecover));
      g_print("net_typefind GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_typefind));
      g_print("net_decoder GST_OBJECT_REFCOUNT(%d)\n", GST_OBJECT_REFCOUNT(m_gstvars->net_decoder));
      */

      m_gstvars->net_source = NULL;
      m_gstvars->net_queue = NULL;
      m_gstvars->net_clkrecover = NULL;
      m_gstvars->net_typefind = NULL;
      m_gstvars->net_decoder = NULL;
    }
  }
}

void CGSTPlayer::ProbeUDPStreams(void)
{
  if (!m_gstvars->videosink)
    return;

  bool done = false;
  gpointer elementdata = NULL;
  GstElement *child = GST_ELEMENT_PARENT(m_gstvars->videosink);
  GstElement *parent = GST_ELEMENT_PARENT(child);
  GstIterator *element_iter = gst_bin_iterate_elements(GST_BIN_CAST(parent));
  while (!done && element_iter)
  {
    switch (gst_iterator_next(element_iter, &elementdata))
    {
      case GST_ITERATOR_DONE:
      case GST_ITERATOR_ERROR:
        done = true;
      break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync(element_iter);
      break;
      case GST_ITERATOR_OK:
        GstElement *element = GST_ELEMENT_CAST(elementdata);
        gchar *elementname  = gst_element_get_name(element);
        if (g_strrstr(elementname, "abin") || g_strrstr(elementname, "vbin"))
        {
          gpointer paddata = NULL;
          GstIterator *pad_iter = gst_element_iterate_sink_pads(element);
          while (gst_iterator_next(pad_iter, &paddata) == GST_ITERATOR_OK)
          {
            GstPad  *pad  = GST_PAD(paddata);
            GstCaps *caps = gst_pad_get_negotiated_caps(pad);
            gchar   *str  = gst_caps_to_string(caps);
            if (g_str_has_prefix(str, "video/"))
            {
              GstStructure *structure = gst_caps_get_structure(caps, 0);

              gint video_fps_d, video_fps_n, video_width, video_height;
              gint video_par_d, video_par_n;
              if (!gst_structure_get_fraction(structure, "pixel-aspect-ratio", &video_par_n, &video_par_d))
              {
                video_par_n = 1;
                video_par_d = 1;
              }
              if (!gst_structure_get_fraction(structure, "framerate", &video_fps_n, &video_fps_d))
              {
                video_fps_n = 0;
                video_fps_d = 1;
              }
              if (!gst_structure_get_int(structure, "width",  &video_width))
                video_width = 0;
              if (!gst_structure_get_int(structure, "height",  &video_height))
                video_height = 0;

              m_video_fps    = (float)video_fps_n/(float)video_fps_d;
              m_video_width  = video_width * (float)video_par_n/(float)video_par_d;
              m_video_height = video_height;
            }
            else if (g_str_has_prefix(str, "audio/"))
            {
              gint width, channels, samplerate;
              GstStructure *structure = gst_caps_get_structure(caps, 0);

              if (!gst_structure_get_int(structure, "width", &width))
                width = 0;
              if (!gst_structure_get_int(structure, "channels", &channels))
                channels = 2;
              if (!gst_structure_get_int(structure, "rate", &samplerate))
                samplerate = 48000;

              m_audio_bits = width;
              m_audio_channels = channels;
              m_audio_samplerate = samplerate;
            }
            else if (g_str_has_prefix(str, "text/"))
            {
            }
          }
          gst_iterator_free(pad_iter);
        }
        g_free(elementname);
      break;
    }
  }
  gst_iterator_free(element_iter);
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
