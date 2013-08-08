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
#include "DllLibStagefrightProcessState.h"
#include "DllLibStagefright.h"
DllProcessState::DllProcessState()
{
  m_state = NULL;
  m_dll = NULL;
}

DllProcessState::DllProcessState(android::sp<android::ProcessState> state, DllLibStagefright *dll)
{
  m_state = state;
  m_dll = dll;
}

DllProcessState::~DllProcessState()
{
  delete m_dll;
}

DllProcessState DllProcessState::self()
{
  // Recursive. Fills in dll and state for future calls to startThreadPool().
  // Usually used like: ProcessState::self().startThreadPool();
  DllLibStagefright *dll = new DllLibStagefright;
  if (dll->Load())
    return DllProcessState(dll->processstate_self(), dll);
  return DllProcessState(NULL, NULL);
}

void DllProcessState::startThreadPool()
{
  if(m_state != NULL && m_dll)
    m_dll->processstate_startThreadPool(m_state);
}
