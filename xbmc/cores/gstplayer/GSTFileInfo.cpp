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

#include "GSTFileInfo.h"
#include "GSTAppsrc.h"
#include "FileItem.h"
#include "filesystem/File.h"
#include "filesystem/StackDirectory.h"
#include "threads/SystemClock.h"
#include "pictures/Picture.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "video/VideoInfoTag.h"

#include <gst/gst.h>

// GstAutoplugSelectResult flags from gst/playback/gstplay-enum.h. 
// It is the policy of GStreamer to not publicly expose element-specific enums.
// That's why these GstAutoplugSelectResult enum has been copied here.
typedef enum
{
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

static GstAutoplugSelectResult on_autoplug_select(GstElement *decodebin, GstPad *pad,
  GstCaps *caps, GstElementFactory *factory, gpointer user_data)
{
  GstAutoplugSelectResult rtn = GST_AUTOPLUG_SELECT_TRY;

  CStdString try_factory(GST_PLUGIN_FEATURE_NAME(factory));

  if (try_factory.Left(5).Equals("ismd_"))
    rtn = GST_AUTOPLUG_SELECT_SKIP;
  /*
  if (rtn == GST_AUTOPLUG_SELECT_SKIP)
  {
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    g_print("skipping %s with caps %s\n", try_factory.c_str(), gst_structure_get_name(structure));
  }
  else
  {
    g_print("trying   %s\n", try_factory.c_str());
  }
  */
  return rtn;
}

static void on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
  GstPad *sinkpad;
  GstElement *color = (GstElement *)data;

  sinkpad = gst_element_get_static_pad(color, "sink");
  gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);
}

// ****************************************************************
bool CGSTFileInfo::GetFileDuration(const CStdString &strPath, int& duration_ms)
{
  return false;
}

// ****************************************************************
bool CGSTFileInfo::ExtractThumb(const CStdString &strPath, const CStdString &strTarget, CStreamDetails *pStreamDetails)
{
  // TODO: figure out why everything but mov/mkv's will hang us.
  CStdString extension;
  extension = URIUtils::GetExtension(strPath);
  if (!extension.Equals(".mkv") && !extension.Equals(".mov"))
    return false;

  bool rtn = false;
  int  bgn_time = XbmcThreads::SystemClockMillis();

  // setup desired width, gstreamer will calc a height with 1:1 pixel aspect ratio
  gint width, height;
  width  = g_advancedSettings.m_thumbSize;
  height = 0;

  // may be called multiple times in an app, subsequent calls are no-op
  gst_init(NULL, NULL);

  GstElement *pipeline = gst_pipeline_new("thumb-extractor");

  GstElement *decoder  = gst_element_factory_make("uridecodebin",  "uridecodebin");
  g_object_set(decoder, "uri", "appsrc://", NULL);

  GstElement *ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", NULL);
  GstElement *videoscale = gst_element_factory_make("videoscale", NULL);

  GstElement *thumbsink  = gst_element_factory_make("appsink", "sink");
	GstCaps *thumbcaps = gst_caps_new_simple("video/x-raw-rgb",
    "width",      G_TYPE_INT, width,
    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
    "bpp",        G_TYPE_INT, 32,
    "depth",      G_TYPE_INT, 32,
    "endianness", G_TYPE_INT, 4321,
    "red_mask",   G_TYPE_INT, 0x0000FF00,
    "green_mask", G_TYPE_INT, 0x00FF0000,
    "blue_mask",  G_TYPE_INT, 0xFF000000,
    NULL);
  g_object_set(thumbsink, "caps", thumbcaps, NULL);
  gst_caps_unref(thumbcaps);

  // add all elements to the pipeline.
  gst_bin_add_many(GST_BIN(pipeline), decoder, ffmpegcolorspace, videoscale, thumbsink, NULL);

  // connect uridecodebin signals. we use this to;
  // 1) filter out ismd hardware decoders
  // 2) auto connect to ffmpegcolorspace.
  g_signal_connect(decoder, "autoplug-select", G_CALLBACK(on_autoplug_select), NULL);
  g_signal_connect(decoder, "pad-added",       G_CALLBACK(on_pad_added),       ffmpegcolorspace);

  // create our vfs appsrc and connect it to the pipeline
  CGSTAppsrc appsrc(strPath.c_str());
  g_signal_connect(pipeline, "deep-notify::source", (GCallback)appsrc.FoundSource, &appsrc);

  // link the color convert, scaler and thumbsink elements together.
  // we will dynamically connect uridecodebin to ffmpegcolorspace using "pad-added" signal.
  if (!gst_element_link_many(ffmpegcolorspace, videoscale, thumbsink, NULL))
  {
    CLog::Log(LOGDEBUG, "%s - gst_element_link_many failed", __FUNCTION__);
    rtn = false;
    goto do_exit;
  }

  // set to PAUSED to make the first frame arrive in the appsink callback
  GstStateChangeReturn gst_state_rtn;
  gst_state_rtn = gst_element_set_state(pipeline, GST_STATE_PAUSED);
  switch (gst_state_rtn)
  {
    case GST_STATE_CHANGE_FAILURE:
      CLog::Log(LOGDEBUG, "%s - failed to play the file", __FUNCTION__);
      rtn = false;
      goto do_exit;
    case GST_STATE_CHANGE_NO_PREROLL:
      // for live sources, we need to set the pipeline to PLAYING before we can
      // receive a buffer. We don't do that yet
      CLog::Log(LOGDEBUG, "%s - live sources not supported yet", __FUNCTION__);
      rtn = false;
      goto do_exit;
    default:
      break;
  }
  // This can block for up to 5 seconds. If your machine is really overloaded,
  // it might time out before the pipeline prerolled and we generate an error. A
  // better way is to run a mainloop and catch errors there.
  gst_state_rtn = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
  if (gst_state_rtn == GST_STATE_CHANGE_FAILURE)
  {
    CLog::Log(LOGDEBUG, "%s - failed to play the file", __FUNCTION__);
    rtn = false;
    goto do_exit;
  }

  // get the duration
  gint64    duration, position;
  GstFormat format;
  format = GST_FORMAT_TIME;
  gst_element_query_duration(pipeline, &format, &duration);

  if (duration != -1)
    // we have a duration, seek to 5%
    position = duration * 5 / 100;
  else
    // no duration, seek to 1 second, this could EOS
    position = 1 * GST_SECOND;

  // seek to the a position in the file. Most files have a black first frame so
  // by seeking to somewhere else we have a bigger chance of getting something
  // more interesting. An optimisation would be to detect black images and then
  // seek a little more
  gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
      (GstSeekFlags)(GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH), position);

  // get the preroll buffer from appsink, this block untils appsink really prerolls
  GstElement *sink;
  GstBuffer  *buffer;
  sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
  g_signal_emit_by_name(sink, "pull-preroll", &buffer, NULL);
  // possible that we don't have a buffer because 
  // we went EOS right away or had an error so check.
  if (buffer)
  {
    GstCaps *caps;
    GstStructure *struture;

    // get the snapshot buffer format now. We set the caps on the appsink so
    // that it can only be an rgb buffer. The only thing we have not specified
    // on the caps is the height, which is dependant on the pixel-aspect-ratio
    // of the source material
    caps = gst_buffer_get_caps(buffer);
    if (!caps)
    {
      CLog::Log(LOGDEBUG, "%s - could not get thumbnail format", __FUNCTION__);
      rtn = false;
      goto do_exit;
    }
    struture = gst_caps_get_structure(caps, 0);

    gboolean gst_bool;
    // we need to get the final caps on the buffer to get the size
    gst_bool  = gst_structure_get_int(struture, "width",  &width);
    gst_bool |= gst_structure_get_int(struture, "height", &height);
    if (!gst_bool)
    {
      CLog::Log(LOGDEBUG, "%s - could not get thumbnail dimensions, res(%d)", __FUNCTION__, gst_bool);
      rtn = false;
      goto do_exit;
    }

    // save the thumbnail
    CPicture::CreateThumbnailFromSurface(GST_BUFFER_DATA(buffer), width, height, width * 4, strTarget);
    rtn = true;
  }
  else
  {
    CLog::Log(LOGDEBUG, "%s - could not make thumbnail\n", __FUNCTION__);
  }

do_exit:
  // cleanup and exit
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);


  if(!rtn)
  {
    XFILE::CFile file;
    if(file.OpenForWrite(strTarget))
      file.Close();
  }

  if(rtn)
  {
    CLog::Log(LOGDEBUG, "%s - measured %d ms to extract %d x %d thumb from file <%s> ",
      __FUNCTION__, (XbmcThreads::SystemClockMillis() - bgn_time), width, height, strPath.c_str());
  }

  return rtn;
}

// ****************************************************************
bool CGSTFileInfo::GetFileStreamDetails(CFileItem *pItem)
{
  return false;
}
