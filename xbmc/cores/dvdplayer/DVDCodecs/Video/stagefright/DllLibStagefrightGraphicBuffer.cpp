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
#include "DllLibStagefrightGraphicBuffer.h"
#include "DllLibStagefright.h"

DllGraphicBuffer::DllGraphicBuffer(android::sp<android::GraphicBuffer> graphicbuffer)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_graphicbuffer = graphicbuffer;
}

ANativeWindowBuffer* DllGraphicBuffer::getNativeBuffer()
{
  if (m_dll && m_graphicbuffer != NULL)
    return m_dll->graphicbuffer_getNativeBuffer(m_graphicbuffer.get());
  return NULL;
}

android::GraphicBuffer* DllGraphicBuffer::get()
{
  return m_graphicbuffer.get();
}
