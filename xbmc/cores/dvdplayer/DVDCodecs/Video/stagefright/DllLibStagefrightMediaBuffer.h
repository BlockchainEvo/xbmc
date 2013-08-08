#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <utils/StrongPointer.h>
#include "DllLibStagefrightMetaData.h"
#include "DllLibStagefrightGraphicBuffer.h"

namespace android
{
  class MetaData;
  class GraphicBuffer;
  class MediaBuffer;
  class MediaBufferObserver;
}

class DllMediaBuffer
{
public:
  DllMediaBuffer(size_t size);
  ~DllMediaBuffer();
  void release();
  void add_ref();
  void *data();
  size_t size();
  size_t range_offset();
  size_t range_length();
  void set_range(size_t offset, size_t length);
  DllGraphicBuffer graphicBuffer();
  DllMetaData meta_data();
  void setObserver(android::MediaBufferObserver *group);
  void reset();
  android::MediaBuffer* clone();
  int refcount();

  android::MediaBuffer* get() const;
  android::MediaBuffer** getaddr() { return &m_buffer; } ;
  void set(android::MediaBuffer* buffer) { m_buffer = buffer; }

private:
  android::MediaBuffer *m_buffer;
  DllLibStagefright *m_dll;
};

