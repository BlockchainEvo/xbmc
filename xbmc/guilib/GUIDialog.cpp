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

#include "GUIDialog.h"
#include "GUIWindowManager.h"
#include "GUILabelControl.h"
#include "GUIAudioManager.h"
#include "GUIInfoManager.h"
#include "threads/SingleLock.h"
#include "utils/TimeUtils.h"
#include "Application.h"

CGUIDialog::CGUIDialog(int id, const CStdString &xmlFile)
    : CGUIWindow(id, xmlFile)
{
  m_bModal = true;
  m_bRunning = false;
  m_wasRunning = false;
  m_renderOrder = 1;
  m_autoClosing = false;
  m_enableSound = true;
}

CGUIDialog::~CGUIDialog(void)
{}

void CGUIDialog::OnWindowLoaded()
{
  CGUIWindow::OnWindowLoaded();

  // Clip labels to extents
  if (m_children.size())
  {
    CGUIControl* pBase = m_children[0];

    for (iControls p = m_children.begin() + 1; p != m_children.end(); ++p)
    {
      if ((*p)->GetControlType() == CGUIControl::GUICONTROL_LABEL)
      {
        CGUILabelControl* pLabel = (CGUILabelControl*)(*p);

        if (!pLabel->GetWidth())
        {
          float spacing = (pLabel->GetXPosition() - pBase->GetXPosition()) * 2;
          pLabel->SetWidth(pBase->GetWidth() - spacing);
        }
      }
    }
  }
}

bool CGUIDialog::OnAction(const CAction &action)
{
  // keyboard or controller movement should prevent autoclosing
  if (!action.IsMouse() && m_autoClosing)
    SetAutoClose(m_showDuration);

  if (action.GetID() == ACTION_PREVIOUS_MENU || action.GetID() == ACTION_NAV_BACK)
  {
    Close();
    return true;
  }
  return CGUIWindow::OnAction(action);
}

bool CGUIDialog::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_WINDOW_DEINIT:
    {
      CGUIWindow *pWindow = g_windowManager.GetWindow(g_windowManager.GetActiveWindow());
      if (pWindow)
        g_windowManager.ShowOverlay(pWindow->GetOverlayState());

      CGUIWindow::OnMessage(message);
      // if we were running, make sure we remove ourselves from the window manager
      if (m_bRunning)
      {
        g_windowManager.RemoveDialog(GetID());
        m_bRunning = false;
        m_closing = false;
        m_autoClosing = false;
      }
      return true;
    }
  case GUI_MSG_WINDOW_INIT:
    {
      CGUIWindow::OnMessage(message);
      m_showStartTime = CTimeUtils::GetFrameTime();
      return true;
    }
  }

  return CGUIWindow::OnMessage(message);
}

void CGUIDialog::DoProcess(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
  UpdateVisibility();

  // if we were running but now we're not, mark us dirty
  if (!m_bRunning && m_wasRunning)
    dirtyregions.push_back(m_renderRegion);

  if (m_bRunning)
    CGUIWindow::DoProcess(currentTime, dirtyregions);

  m_wasRunning = m_bRunning;
}

void CGUIDialog::UpdateVisibility()
{
  if (m_visibleCondition)
  {
    if (g_infoManager.GetBool(m_visibleCondition, g_windowManager.GetActiveWindow()))
      Show();
    else
      Close();
  }
}

void CGUIDialog::DoModal_Internal(int iWindowID /*= WINDOW_INVALID */, const CStdString &param /* = "" */)
{
  //Lock graphic context here as it is sometimes called from non rendering threads
  //maybe we should have a critical section per window instead??
  CSingleLock lock(g_graphicsContext);

  if (!g_windowManager.Initialized())
    return; // don't do anything

  m_closing = false;
  m_bModal = true;
  // set running before it's added to the window manager, else the auto-show code
  // could show it as well if we are in a different thread from
  // the main rendering thread (this should really be handled via
  // a thread message though IMO)
  m_bRunning = true;
  g_windowManager.RouteToWindow(this);

  //  Play the window specific init sound
  if (m_enableSound)
    g_audioManager.PlayWindowSound(GetID(), SOUND_INIT);

  // active this window...
  CGUIMessage msg(GUI_MSG_WINDOW_INIT, 0, 0, WINDOW_INVALID, iWindowID);
  msg.SetStringParam(param);
  OnMessage(msg);

//  m_bRunning = true;

  if (!m_windowLoaded)
    Close(true);

  lock.Leave();

  while (m_bRunning && !g_application.m_bStop)
  {
    g_windowManager.ProcessRenderLoop();
  }
}

void CGUIDialog::Show_Internal()
{
  //Lock graphic context here as it is sometimes called from non rendering threads
  //maybe we should have a critical section per window instead??
  CSingleLock lock(g_graphicsContext);

  if (m_bRunning && !m_closing && !IsAnimating(ANIM_TYPE_WINDOW_CLOSE)) return;

  if (!g_windowManager.Initialized())
    return; // don't do anything

  m_bModal = false;

  // set running before it's added to the window manager, else the auto-show code
  // could show it as well if we are in a different thread from
  // the main rendering thread (this should really be handled via
  // a thread message though IMO)
  m_bRunning = true;
  m_closing = false;
  g_windowManager.AddModeless(this);

  //  Play the window specific init sound
  if (m_enableSound)
    g_audioManager.PlayWindowSound(GetID(), SOUND_INIT);

  // active this window...
  CGUIMessage msg(GUI_MSG_WINDOW_INIT, 0, 0);
  OnMessage(msg);

//  m_bRunning = true;
}

void CGUIDialog::DoModal(int iWindowID /*= WINDOW_INVALID */, const CStdString &param)
{
  if (!g_application.IsCurrentThread())
  {
    // make sure graphics lock is not held
    CSingleExit leaveIt(g_graphicsContext);
    g_application.getApplicationMessenger().DoModal(this, iWindowID, param);
  }
  else
    DoModal_Internal(iWindowID, param);
}

void CGUIDialog::Show()
{
  if (!g_application.IsCurrentThread())
  {
    // make sure graphics lock is not held
    CSingleExit leaveIt(g_graphicsContext);
    g_application.getApplicationMessenger().Show(this);
  }
  else
    Show_Internal();
}

void CGUIDialog::FrameMove()
{
  if (m_autoClosing && m_showStartTime + m_showDuration < CTimeUtils::GetFrameTime() && !m_closing)
    Close();
  CGUIWindow::FrameMove();
}

void CGUIDialog::Render()
{
  if (!m_bRunning)
    return;

  CGUIWindow::Render();
}

void CGUIDialog::SetDefaults()
{
  CGUIWindow::SetDefaults();
  m_renderOrder = 1;
}

void CGUIDialog::SetAutoClose(unsigned int timeoutMs)
{
   m_autoClosing = true;
   m_showDuration = timeoutMs;
   if (m_bRunning)
     m_showStartTime = CTimeUtils::GetFrameTime();
}


