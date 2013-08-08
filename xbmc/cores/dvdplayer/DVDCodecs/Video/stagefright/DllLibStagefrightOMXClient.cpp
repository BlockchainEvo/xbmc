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
#include "DllLibStagefrightOMXClient.h"
#include "DllLibStagefright.h"

DllOMXClient::DllOMXClient()
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_omxclient = m_dll->create_omxclient();
}

DllOMXClient::~DllOMXClient()
{
  if(m_dll && m_omxclient != NULL)
    m_dll->destroy_omxclient(m_omxclient);
  delete m_dll;
}
android::status_t DllOMXClient::connect()
{
  if(m_dll && m_omxclient != NULL)
    return m_dll->omxclient_connect(m_omxclient);
  return 0;
}

void DllOMXClient::disconnect()
{
  if(m_dll && m_omxclient != NULL)
    m_dll->omxclient_disconnect(m_omxclient);
}

android::sp<android::IOMX> DllOMXClient::interface()
{
  if(m_dll && m_omxclient != NULL)
    return m_dll->omxclient_interface(m_omxclient);
  return NULL;
}


android::OMXClient* DllOMXClient::get()
{
  return m_omxclient;
}
