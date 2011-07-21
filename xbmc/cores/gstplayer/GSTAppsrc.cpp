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

#include "GSTAppsrc.h"
#include "FileItem.h"
#include "utils/log.h"

#include <gst/gst.h>

CGSTAppsrc::CGSTAppsrc(const std::string url)
{
  m_url = url;
  m_cfile = NULL;
  m_appsrc = NULL;
}
CGSTAppsrc::~CGSTAppsrc()
{
  if (m_cfile)
  {
    m_cfile->Close();
    delete m_cfile;
    m_cfile = NULL;
  }
}

void CGSTAppsrc::FeedData(GstElement *appsrc, guint size, CGSTAppsrc *ctx)
{
  // This push method is called by the need-data signal callback,
  //  we feed data into the appsrc with an arbitrary size.
  unsigned int  readSize;
  GstBuffer     *buffer;
  GstFlowReturn ret;

  buffer   = gst_buffer_new_and_alloc(size);
  readSize = ctx->m_cfile->Read(buffer->data, size);
  if (readSize > 0)
  {
    GST_BUFFER_SIZE(buffer) = readSize;

    g_signal_emit_by_name(ctx->m_appsrc, "push-buffer", buffer, &ret);
    // this takes ownership of the buffer; don't unref
    //gst_app_src_push_buffer(appsrc, buffer);
  }
  else
  {
    // we are EOS, send end-of-stream
    g_signal_emit_by_name(ctx->m_appsrc, "end-of-stream", &ret);
    //gst_app_src_end_of_stream(appsrc);
  }
  gst_buffer_unref(buffer);

  return;
}

gboolean CGSTAppsrc::SeekData(GstElement *appsrc, guint64 position, CGSTAppsrc *ctx)
{
  // called when appsrc wants us to return data from a new position
  // with the next call to push-buffer (FeedData).
  position = ctx->m_cfile->Seek(position, SEEK_SET);
  if (position >= 0)
    return TRUE;
  else
    return FALSE;
}

void CGSTAppsrc::FoundSource(GObject *object, GObject *orig, GParamSpec *pspec, CGSTAppsrc *ctx)
{
  // called when playbin2 has constructed a source object to read
  //  from. Since we provided the appsrc:// uri to playbin2, 
  //  this will be the appsrc that we must handle. 
  // we set up some signals to push data into appsrc and one to perform a seek.

  // get a handle to the appsrc
  g_object_get(orig, pspec->name, &ctx->m_appsrc, NULL);

  unsigned int flags = READ_TRUNCATED;
  ctx->m_cfile = new XFILE::CFile();
  if (CFileItem(ctx->m_url, false).IsInternetStream())
    flags |= READ_CACHED;
  // open file in binary mode
  if (!ctx->m_cfile->Open(ctx->m_url, flags))
    return;

  // we can set the length in appsrc. This allows some elements to estimate the
  // total duration of the stream. It's a good idea to set the property when you
  // can but it's not required.
  int64_t filelength = ctx->m_cfile->GetLength();
  if (filelength > 0)
    g_object_set(ctx->m_appsrc, "size", (gint64)filelength, NULL);
  // we are seekable in push mode, this means that the element usually pushes
  // out buffers of an undefined size and that seeks happen only occasionally
  // and only by request of the user.
  if (filelength > 0)
    gst_util_set_object_arg(G_OBJECT(ctx->m_appsrc), "stream-type", "seekable");

  // configure the appsrc, we will push a buffer to appsrc when it needs more data
  g_signal_connect(ctx->m_appsrc, "need-data", G_CALLBACK(FeedData), ctx);
  g_signal_connect(ctx->m_appsrc, "seek-data", G_CALLBACK(SeekData), ctx);
}
