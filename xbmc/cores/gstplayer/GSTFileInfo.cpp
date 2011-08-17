/*
 *      Copyright (C) 2005-2008 Team XBMC
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
#include "threads/Event.h"
#include "threads/SystemClock.h"
#include "pictures/Picture.h"
#include "settings/AdvancedSettings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "video/VideoInfoTag.h"


#include <gst/gst.h>

#if defined(HAS_INTEL_SMD)
// ismd hw decoded thumbnail generation using a 2 stage gstreamer pipeline.
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include "ismd_gst_buffer.h"
#include "ismd_core_protected.h"

// ****************************************************************
static void on_pad_added(GstElement *element, GstPad *pad, gpointer user_data)
{
  GstElement *pproc = (GstElement*)user_data;
  GstPad *sinkpad = gst_element_get_static_pad(pproc, "sink");
  gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
}

static GstFlowReturn on_buffer(GstAppSink *sink, gpointer user_data)
{
  if (user_data)
    ((CEvent*)user_data)->Set();

  return GST_FLOW_OK;
}

// ****************************************************************
bool CGSTFileInfo::ExtractThumb(const CStdString &strPath, const CStdString &strTarget, CStreamDetails *pStreamDetails)
{
  // do not try avi containers
  CStdString extension = URIUtils::GetExtension(strPath);
  if (extension.Equals(".avi"))
    return false;

  bool rtn = false;
  int  bgn_time = XbmcThreads::SystemClockMillis();

  CEvent prerolled;
  prerolled.Reset();
  GstElement *pipeline  = gst_pipeline_new("thumb-extractor");
  GstElement *decoder   = gst_element_factory_make("uridecodebin",  "uridecodebin");
  GstElement *pproc     = gst_element_factory_make("ismd_vidpproc", "vidpproc");
  GstElement *sink      = gst_element_factory_make("appsink",       "video-output");
  //
  // setup pipeline
  // configure elements
  GstCaps *ismd_caps = gst_caps_from_string("video/x-decoded-ismd");
  g_object_set(decoder, "uri", "appsrc://", "caps", ismd_caps, NULL);
  gst_caps_unref(ismd_caps);
  // AppSink Callbacks are faster than emmiting signals
  GstAppSinkCallbacks appsink_cbs = { NULL, on_buffer, on_buffer, NULL };
  gst_app_sink_set_callbacks(GST_APP_SINK(sink), &appsink_cbs, &prerolled, NULL);
  // add elements to the pipeline.
	gst_bin_add_many(GST_BIN(pipeline), decoder, pproc, sink, NULL);
  // setup to auto connect decoder src to pproc sink
  g_signal_connect(decoder, "pad-added", G_CALLBACK(on_pad_added), pproc);
  // create our vfs appsrc and connect it to the pipeline
  CGSTAppsrc appsrc(strPath.c_str());
  g_signal_connect(pipeline, "deep-notify::source", (GCallback)appsrc.FoundSource, &appsrc);

  // link the elements together.
  if (!gst_element_link_many(pproc, sink, NULL))
  {
    CLog::Log(LOGDEBUG, "%s - gst_element_link_many failed", __FUNCTION__);
    goto do_exit;
  }

  // set to PAUSED to make the first frame arrive in the appsink callback
  GstStateChangeReturn ret;
  ret = gst_element_set_state(pipeline, GST_STATE_PAUSED);
  switch (ret)
  {
    case GST_STATE_CHANGE_FAILURE:
      CLog::Log(LOGDEBUG, "%s - failed to play the file", __FUNCTION__);
      goto do_exit;
    case GST_STATE_CHANGE_NO_PREROLL:
      // for live sources, we need to set the pipeline to PLAYING before we can
      // receive a buffer. We don't do that yet
      CLog::Log(LOGDEBUG, "%s - live sources not supported yet", __FUNCTION__);
      goto do_exit;
    default:
      break;
  }
  // This can block for up to 5 seconds. If your machine is really overloaded,
  // it might time out before the pipeline prerolled and we generate an error. A
  // better way is to run a mainloop and catch errors there.
  ret = gst_element_get_state(pipeline, NULL, NULL, 5 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_FAILURE)
  {
    CLog::Log(LOGDEBUG, "%s - failed to play the file", __FUNCTION__);
    goto do_exit;
  }

  // since we will get a preroll before we can seek, use this to
  // setup the hw decoder to output into thumbsized pictures. 
  if (prerolled.WaitMSec(2000))
  {
    GstBuffer *buffer;
    if ((buffer = gst_app_sink_pull_preroll(GST_APP_SINK(sink))))
    {
      ismd_buffer_handle_t handle = ((ISmdGstBuffer*)buffer)->ismd_buffer_id;
      if (handle)
        ismd_buffer_add_reference(handle); 
      // got a handle to a buffer, now get the descriptor
      ismd_buffer_descriptor_t desc;
      if (ismd_buffer_read_desc(handle, &desc) == ISMD_SUCCESS)
      {
        // got the buffer descriptor
        // we only understand certain types of buffers
        if (desc.buffer_type == ISMD_BUFFER_TYPE_PHYS ||
            desc.buffer_type == ISMD_BUFFER_TYPE_VIDEO_FRAME ||
            desc.buffer_type == ISMD_BUFFER_TYPE_VIDEO_FRAME_REGION)
        {
          ismd_frame_attributes_t *attr = (ismd_frame_attributes_t*)desc.attributes;
          // scale output down to thumbsize size
          int width = g_advancedSettings.m_thumbSize;
          double w_scaler = (float) width / attr->dest_size.width;
          int height = attr->dest_size.height * w_scaler;
          
          CStdString rectangle;
          rectangle.Format("0,0,%d,%d", width, height);
          g_object_set(pproc, "rectangle", rectangle.c_str(), NULL);
        }
      }
      if (handle)
        ismd_buffer_dereference(handle);
      gst_buffer_unref(buffer);
    }
  }
  else
  {
    CLog::Log(LOGDEBUG, "%s - failed to get 1st preroll", __FUNCTION__);
    goto do_exit;
  }

  // get the duration
  gint64 duration, position;
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
  prerolled.Reset();
  gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
      (GstSeekFlags)(GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH), position);

  // now we fetch real thumbnail at seek position.
  GstBuffer *buffer;
  buffer = NULL;
  if (prerolled.WaitMSec(2000))
    buffer = gst_app_sink_pull_preroll(GST_APP_SINK(sink));
  
  if (buffer)
  {
    ismd_buffer_handle_t handle;
    ismd_buffer_descriptor_t desc;
    ismd_result_t rc = ISMD_SUCCESS;

    handle = ((ISmdGstBuffer*)buffer)->ismd_buffer_id;
    if (handle)
      ismd_buffer_add_reference(handle);
    // got a handle to a buffer, now get the descriptor
    rc = ismd_buffer_read_desc(handle, &desc);
    if (rc == ISMD_SUCCESS)
    {
      // got the buffer descriptor
      // we only understand certain types of buffers
      if (desc.buffer_type == ISMD_BUFFER_TYPE_PHYS ||
          desc.buffer_type == ISMD_BUFFER_TYPE_VIDEO_FRAME ||
          desc.buffer_type == ISMD_BUFFER_TYPE_VIDEO_FRAME_REGION)
      {
        ismd_frame_attributes_t *attr = (ismd_frame_attributes_t*)desc.attributes;
        // do image extract
        // 2048 is a magic constant from ISMD_CORE_INT_PROPERTY("frame_buffer_properties.stride")
        int virt_y_stride  = ISMD_CORE_INT_PROPERTY("frame_buffer_properties.stride");
        int virt_uv_stride = virt_y_stride;
        int dest_y_width   = attr->cont_size.width;
        int dest_y_height  = attr->cont_size.height;
        int dest_y_stride  = GST_ROUND_UP_4(attr->cont_size.width);
        int dest_uv_width  = GST_ROUND_UP_2(attr->cont_size.width  / 2);
        int dest_uv_height = GST_ROUND_UP_2(attr->cont_size.height / 2);
        int dest_uv_stride = GST_ROUND_UP_4(attr->cont_size.width);

        int buffer_size = (dest_y_stride * dest_y_height) + (dest_uv_stride * dest_uv_height);
        GstBuffer *out_buffer = gst_buffer_new_and_alloc(buffer_size);
        GST_BUFFER_SIZE(out_buffer) = buffer_size;

        GstCaps *caps = gst_caps_new_simple("video/x-raw-yuv",
          "format",     GST_TYPE_FOURCC, GST_MAKE_FOURCC ('N', 'V', '1', '2'),
          "framerate",  GST_TYPE_FRACTION, 30, 1,
          "width",      G_TYPE_INT, (int)dest_y_width,
          "height",     G_TYPE_INT, (int)dest_y_height, NULL);
        gst_buffer_set_caps(out_buffer, caps);
        gst_caps_unref(caps);
        // get PTS and put as time stamp
        if (attr->original_pts != ISMD_NO_PTS)
          GST_BUFFER_TIMESTAMP(out_buffer) = gst_util_uint64_scale_round(attr->original_pts, 100000, 9);

        // map ismd buffer into virtual memory
        //int mem_map_size = virt_y_stride * (dest_y_height * 2);
        int mem_map_size = desc.phys.size;
        guint8 *virt_y  = (guint8*)ISMD_BUFFER_MAP_BY_TYPE(desc.buffer_type, desc.phys.base, mem_map_size);
        guint8 *virt_uv = virt_y + attr->u;
        //guint8 *virt_uv = &virt_y[virt_y_stride * dest_y_height];

        guint8 *dest_y  = GST_BUFFER_DATA(out_buffer);
        guint8 *dest_uv = dest_y + (dest_y_stride * dest_y_height);

        // copy y data
        for (int line = 0; line < dest_y_height; line++)
        {
          memcpy(dest_y + (line * dest_y_stride),
                 virt_y + (line * virt_y_stride),
                 dest_y_width);
        }
        // copy uv data
        // NV12  1/2 width, 1/2 height
        // NV16, 1/2 width, 1/1 height 
        if (attr->pixel_format != ISMD_PF_NV12)
          virt_uv_stride *= 2;
        for (int line = 0; line < dest_uv_height; line++)
        {
          memcpy(dest_uv + (line * dest_uv_stride),
                 virt_uv + (line * virt_uv_stride),
                 dest_uv_width * 2);
        }
        // unmap ismd buffer
        OS_UNMAP_IO_FROM_MEM(virt_y, mem_map_size);
        // release ismd buffer handle
        ismd_buffer_dereference(handle);
        handle = 0;
        // done with this pipeline.
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_element_get_state(pipeline, NULL, NULL, 1 * GST_SECOND);
        gst_object_unref(pipeline);
        pipeline = NULL;

        GError *err;
        GstCaps *rgba_caps = gst_caps_new_simple("video/x-raw-rgb",
          "width",      G_TYPE_INT, dest_y_width,
          "height",     G_TYPE_INT, dest_y_height,
          "bpp",        G_TYPE_INT, 32,
          "depth",      G_TYPE_INT, 32,
          "endianness", G_TYPE_INT, 4321,
          "red_mask",   G_TYPE_INT, 0x0000FF00,
          "green_mask", G_TYPE_INT, 0x00FF0000,
          "blue_mask",  G_TYPE_INT, 0xFF000000, NULL);
        GstBuffer *final_buffer = gst_video_convert_frame(out_buffer, rgba_caps, 1 * GST_SECOND, &err);
        if (final_buffer)
        {
          GstCaps *caps;
          GstStructure *struture;
          // get the thumbnail buffer format now. We set the caps on the appsink so
          // that it can only be an rgba buffer. The only thing we have not specified
          // on the caps is the height, which is dependant on the pixel-aspect-ratio
          // of the source material. This is done in ExtractThumbColorConvert.
          caps = gst_buffer_get_caps(final_buffer);
          if (!caps)
          {
            CLog::Log(LOGDEBUG, "%s - could not get thumbnail format", __FUNCTION__);
            goto do_exit;
          }
          struture = gst_caps_get_structure(caps, 0);

          gboolean gst_bool;
          int width, height;
          // we need to get the final caps on the buffer to get the size
          gst_bool  = gst_structure_get_int(struture, "width",  &width);
          gst_bool |= gst_structure_get_int(struture, "height", &height);
          if (!gst_bool)
          {
            CLog::Log(LOGDEBUG, "%s - could not get thumbnail dimensions, res(%d)", __FUNCTION__, gst_bool);
            goto do_exit;
          }
          CPicture::CreateThumbnailFromSurface(GST_BUFFER_DATA(final_buffer), 
            width, height, width * 4, strTarget);
          gst_buffer_unref(final_buffer);
          rtn = true;
        }
        else
        {
          CLog::Log(LOGDEBUG, "%s - gst_video_convert_frame failed\n", __FUNCTION__);
        }
      }
    }
    if (handle)
      ismd_buffer_dereference(handle);
    gst_buffer_unref(buffer);
  }

do_exit:
  // cleanup and exit
  if (pipeline)
  {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = NULL;
  }

  if(!rtn)
  {
    XFILE::CFile file;
    if (file.OpenForWrite(strTarget))
      file.Close();
  }

  CLog::Log(LOGDEBUG,"%s - measured %d ms to extract thumb from file <%s> ",
    __FUNCTION__, (XbmcThreads::SystemClockMillis() - bgn_time), strPath.c_str());
  return rtn;
}

bool CGSTFileInfo::ExtractSnapshot(void)
{
  return false;
}

#else

// thumbnail generation using a 1 stage gstreamer pipeline.
static void on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
  GstPad *sinkpad;

  GstCaps *caps = gst_pad_get_caps(pad);
  GstStructure *structure = gst_caps_get_structure(caps, 0);
  CStdString check_cap(gst_structure_get_name(structure));

  // ignore anything that is not video
  if (check_cap.Left(11).Equals("video/x-raw"))
  {
    GstElement *ffmpegcolorspace = (GstElement *)data;
    sinkpad = gst_element_get_static_pad(ffmpegcolorspace, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
  }
  gst_caps_unref(caps);
}

static void new_preroll(GstElement *appsink, gpointer user_data)
{
  if (user_data)
    ((CEvent*)user_data)->Set();
}

// ****************************************************************
bool CGSTFileInfo::GSTExtractThumb(const CStdString &strPath, const CStdString &strTarget, CStreamDetails *pStreamDetails)
{
  // do not try avi containers
  CStdString extension = URIUtils::GetExtension(strPath);
  if (extension.Equals(".avi"))
    return false;

  bool rtn = false;
  int  bgn_time = XbmcThreads::SystemClockMillis();

  // setup desired width, gstreamer will calc a height with 1:1 pixel aspect ratio
  gint width, height;
  width  = g_advancedSettings.m_thumbSize;
  height = 0;

  // may be called multiple times in an app, subsequent calls are no-op
  gst_init(NULL, NULL);

  CEvent got_preroll;
  got_preroll.Reset();
  GstElement *pipeline         = gst_pipeline_new("thumb-extractor");
  GstElement *decoder          = gst_element_factory_make("uridecodebin", "uridecodebin");
  GstElement *ffmpegcolorspace = gst_element_factory_make("ffmpegcolorspace", NULL);
  GstElement *videoscale       = gst_element_factory_make("videoscale", NULL);
  // add all elements to the pipeline.
  gst_bin_add_many(GST_BIN(pipeline), decoder, ffmpegcolorspace, videoscale, thumbsink, NULL);
  // configure elements
  g_object_set(decoder, "uri", "appsrc://", NULL);
  g_object_set(decoder, "use-buffering", TRUE, NULL);
  //Add black borders if necessary to keep the DAR
  g_object_set(videoscale, "add-borders", TRUE, NULL);
  // setup thumbnail scaling
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
  g_object_set(thumbsink, "caps", thumbcaps, "emit-signals", TRUE, NULL);
  g_signal_connect(thumbsink, "new-preroll", (GCallback)new_preroll, &got_preroll);
  gst_caps_unref(thumbcaps);

  // connect uridecodebin signal to ffmpegcolorspace;
  g_signal_connect(decoder, "pad-added", G_CALLBACK(on_pad_added), ffmpegcolorspace);

  // create our vfs appsrc and connect it to the pipeline
  CGSTAppsrc appsrc(strPath.c_str());
  g_signal_connect(pipeline, "deep-notify::source", (GCallback)appsrc.FoundSource, &appsrc);

  // link the color convert, scaler and thumbsink elements together.
  // we will dynamically connect uridecodebin to ffmpegcolorspace using "pad-added" signal.
  if (!gst_element_link_many(ffmpegcolorspace, videoscale, thumbsink, NULL))
  {
    CLog::Log(LOGDEBUG, "%s - gst_element_link_many failed", __FUNCTION__);
    goto do_exit;
  }

  // set to PAUSED to make the first frame arrive in the appsink callback
  GstStateChangeReturn gst_state_rtn;
  gst_state_rtn = gst_element_set_state(pipeline, GST_STATE_PAUSED);
  switch (gst_state_rtn)
  {
    case GST_STATE_CHANGE_FAILURE:
      CLog::Log(LOGDEBUG, "%s - failed to play the file", __FUNCTION__);
      goto do_exit;
    case GST_STATE_CHANGE_NO_PREROLL:
      // for live sources, we need to set the pipeline to PLAYING before we can
      // receive a buffer. We don't do that yet
      CLog::Log(LOGDEBUG, "%s - live sources not supported yet", __FUNCTION__);
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
    goto do_exit;
  }

  // get the duration
  gint64    duration, position;
  GstFormat format;
  format = GST_FORMAT_TIME;
  duration = -1;

  if (duration != -1)
    // we have a duration, seek to 33%
    position = duration * (33 / 100);
  else
    // no duration, seek to 30 second, this could EOS
    position = 60 * GST_SECOND;

  // seek to the a position in the file. Most files have a black first frame so
  // by seeking to somewhere else we have a bigger chance of getting something
  // more interesting. An optimisation would be to detect black images and then
  // seek a little more
  gst_element_seek_simple(pipeline, GST_FORMAT_TIME,
      (GstSeekFlags)(GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH), position);

  GstElement *sink;
  sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
  if (!sink)
  {
    CLog::Log(LOGDEBUG, "%s - failed to get sink element", __FUNCTION__);
    goto do_exit;
  }

  // wait for preroll buffer from appsink, this block untils appsink really prerolls
  if (!got_preroll.WaitMSec(2000))
  {
    CLog::Log(LOGDEBUG, "%s - failed to get pull-preroll buffer", __FUNCTION__);
    goto do_exit;
  }

  GstBuffer  *buffer;
  g_signal_emit_by_name(sink, "pull-preroll", &buffer, NULL);
  //g_signal_emit_by_name(sink, "pull-buffer", &buffer, NULL);
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
#endif

// ****************************************************************
bool CGSTFileInfo::GetFileDuration(const CStdString &path, int& duration)
{
  return false;
}

// ****************************************************************
bool CGSTFileInfo::GetFileStreamDetails(CFileItem *pItem)
{
  return false;
}

