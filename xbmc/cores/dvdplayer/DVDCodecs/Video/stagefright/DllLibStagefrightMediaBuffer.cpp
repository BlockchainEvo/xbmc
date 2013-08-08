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
#include "DllLibStagefrightMediaBuffer.h"
#include "DllLibStagefright.h"

DllMediaBuffer::DllMediaBuffer(size_t size)
:m_buffer(NULL)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_buffer = m_dll->create_mediabuffer(size);
}

DllMediaBuffer::~DllMediaBuffer()
{
//  if(m_dll && m_buffer)
//    m_dll->destroy_mediabuffer(m_buffer);
  delete m_dll;
}

void DllMediaBuffer::release()
{
  if (m_buffer)
    m_dll->mediabuffer_release(m_buffer);
}

void DllMediaBuffer::add_ref()
{
  if (m_buffer)
    m_dll->mediabuffer_add_ref(m_buffer);
}

void* DllMediaBuffer::data()
{
  if (m_buffer)
    return m_dll->mediabuffer_data(m_buffer);
  return NULL;
}

size_t DllMediaBuffer::size()
{
  if (m_buffer)
    return m_dll->mediabuffer_size(m_buffer);
  return -1;
}

size_t DllMediaBuffer::range_offset()
{
  if (m_buffer)
    return m_dll->mediabuffer_range_offset(m_buffer);
  return -1;
}

size_t DllMediaBuffer::range_length()
{
  if (m_buffer)
    return m_dll->mediabuffer_range_length(m_buffer);
  return -1;
}

void DllMediaBuffer::set_range(size_t offset, size_t length)
{
  if (m_buffer)
    return m_dll->mediabuffer_set_range(m_buffer, offset, length);
}

DllGraphicBuffer DllMediaBuffer::graphicBuffer()
{
  if (m_buffer)
    return m_dll->mediabuffer_graphicBuffer(m_buffer);
  return DllGraphicBuffer(NULL);
}

DllMetaData DllMediaBuffer::meta_data()
{
  if (m_buffer)
    return m_dll->mediabuffer_meta_data(m_buffer);
  return DllMetaData();
}

void DllMediaBuffer::reset()
{
  if (m_buffer)
    return m_dll->mediabuffer_reset(m_buffer);
}

void DllMediaBuffer::setObserver(android::MediaBufferObserver *group)
{
  if (m_buffer)
    m_dll->mediabuffer_setObserver(m_buffer, group);
}

int DllMediaBuffer::refcount()
{
  if (m_buffer)
    return m_dll->mediabuffer_refcount(m_buffer);
  return 0;
}

android::MediaBuffer* DllMediaBuffer::clone()
{
  if (m_buffer)
    return m_dll->mediabuffer_clone(m_buffer);
  return NULL;
}

android::MediaBuffer* DllMediaBuffer::get() const
{
  return m_buffer;
}
