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
#include "DllLibStagefrightReadOptions.h"
#include "DllLibStagefright.h"

DllReadOptions::DllReadOptions()
:m_options(NULL)
,m_inherited(false)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_options = m_dll->create_readoptions();
}

DllReadOptions::DllReadOptions(android::MediaSource::ReadOptions* options)
:m_inherited(true)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
  {
    m_options = options;
  }
}

DllReadOptions::~DllReadOptions()
{
  if(m_dll && m_options && !m_inherited)
    m_dll->destroy_readoptions(m_options);
  delete m_dll;
}

void DllReadOptions::setSeekTo(int64_t time_us, android::MediaSource::ReadOptions::SeekMode mode)
{
  if (m_dll && m_options)
    m_dll->mediasource_readoptions_setSeekTo(m_options, time_us, mode);
}

void DllReadOptions::clearSeekTo()
{
  if (m_dll && m_options)
    m_dll->mediasource_readoptions_clearSeekTo(m_options);
}

bool DllReadOptions::getSeekTo(int64_t *time_us, android::MediaSource::ReadOptions::SeekMode *mode)
{
  if (m_dll && m_options)
    return m_dll->mediasource_readoptions_getSeekTo(m_options, time_us, mode);
  return false;
}

void DllReadOptions::setLateBy(int64_t lateness_us)
{
  if (m_dll && m_options)
    m_dll->mediasource_readoptions_setLateBy(m_options, lateness_us);
}

int64_t DllReadOptions::getLateBy()
{
  if (m_dll && m_options)
    return m_dll->mediasource_readoptions_getLateBy(m_options);
  return 0;
}

android::MediaSource::ReadOptions* DllReadOptions::get()
{
  return m_options;
}
