/*
 *      Copyright (C) 2011-2012 Team XBMC
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "system.h"
#include <EGL/egl.h>
#include "EGLNativeTypeX11.h"
#include "utils/log.h"
#include "guilib/gui3d.h"
#include "windowing/WinEvents.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
CEGLNativeTypeX11::CEGLNativeTypeX11()
{
}

CEGLNativeTypeX11::~CEGLNativeTypeX11()
{
} 

bool CEGLNativeTypeX11::CheckCompatibility()
{
  return true;
}

void CEGLNativeTypeX11::Initialize()
{
  return;
}
void CEGLNativeTypeX11::Destroy()
{
  return;
}

bool CEGLNativeTypeX11::CreateNativeDisplay()
{
  Display *display;
  display = XOpenDisplay(NULL);
  m_nativeDisplay = display;
  return true;
}

bool CEGLNativeTypeX11::CreateNativeWindow(int nativeVisual)
{
   unsigned long mask;
   Window root;
   Window win;
   XVisualInfo *visInfo, visTemplate;
   XSetWindowAttributes attr;
   int num_visuals;
   int scrnum = DefaultScreen( m_nativeDisplay );
   int width = 0;
   int height = 0;

   RESOLUTION_INFO res;
   if (!GetNativeResolution(&res))
     return false;
   width = res.iWidth;
   height = res.iHeight;

   root = RootWindow( m_nativeDisplay, scrnum );
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   visTemplate.visualid = nativeVisual;
   visInfo = XGetVisualInfo((Display*)m_nativeDisplay, VisualIDMask, &visTemplate, &num_visuals);

   if (!visInfo)
   {
     CLog::Log(LOGERROR,"EGL: Couldn't get X visual");
     return false;
   }

   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( (Display*)m_nativeDisplay, root, visInfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask;

   win = XCreateWindow( (Display*)m_nativeDisplay, root, 0, 0, width, height,
                        0, visInfo->depth, InputOutput,
                        visInfo->visual, mask, &attr );

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = 0;
      sizehints.y = 0;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints((Display*)m_nativeDisplay, win, &sizehints);
      XSetStandardProperties((Display*)m_nativeDisplay, win, "XBMC", "XBMC",
                              None, (char **)NULL, 0, &sizehints);
   }
  XMapWindow((Display*)m_nativeDisplay, win);
  m_nativeWindow = (XBNativeDisplayType)win;
  HideCursor();

  CWinEventsX11::Init((Display*)m_nativeDisplay, (Window)m_nativeWindow);

  return true;
}  

bool CEGLNativeTypeX11::GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const
{
  if (!nativeDisplay)
    return false;
  *nativeDisplay = (XBNativeDisplayType*) &m_nativeDisplay;
  return true;
}

bool CEGLNativeTypeX11::GetNativeWindow(XBNativeWindowType **nativeWindow) const
{
  if (!nativeWindow || !m_nativeWindow)
    return false;
  *nativeWindow = (XBNativeWindowType*) &m_nativeWindow;
  return true;
}

bool CEGLNativeTypeX11::DestroyNativeDisplay()
{
  return true;
}

bool CEGLNativeTypeX11::DestroyNativeWindow()
{
  m_nativeWindow = NULL;
  return true;
}

bool CEGLNativeTypeX11::GetNativeResolution(RESOLUTION_INFO *res) const
{
  int scrnum = DefaultScreen( m_nativeDisplay );
  res->iWidth = DisplayWidth(m_nativeDisplay, scrnum);
  res->iHeight = DisplayHeight(m_nativeDisplay, scrnum);
  res->iWidth  = res->iWidth;
  res->iHeight  = res->iHeight;
  res->fRefreshRate = 60;
  res->dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
  res->iScreen       = 0;
  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio   = 1.0f;
  res->iScreenWidth  = res->iWidth;
  res->iScreenHeight = res->iHeight;
  res->strMode.Format("%dx%d @ %.2f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
  res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  CLog::Log(LOGNOTICE,"Current resolution: %s\n",res->strMode.c_str());
  return true;
}

bool CEGLNativeTypeX11::SetNativeResolution(const RESOLUTION_INFO &res)
{
  return false;
}

bool CEGLNativeTypeX11::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  RESOLUTION_INFO res;
  bool ret = false;
  ret = GetNativeResolution(&res);
  if (ret && res.iWidth > 1 && res.iHeight > 1)
  {
    resolutions.push_back(res);
    return true;
  }
  return false;
}

bool CEGLNativeTypeX11::GetPreferredResolution(RESOLUTION_INFO *res) const
{
  return false;
}

bool CEGLNativeTypeX11::ShowWindow(bool show)
{
  return false;
}

bool CEGLNativeTypeX11::HideCursor()
{
  Pixmap bm_no;
  Colormap cmap;
  Cursor no_ptr;
  XColor black, dummy;
  static char bm_no_data[] = {0, 0, 0, 0, 0, 0, 0, 0};

  cmap = DefaultColormap((Display*)m_nativeDisplay, DefaultScreen((Display*)m_nativeDisplay));
  XAllocNamedColor((Display*)m_nativeDisplay, cmap, "black", &black, &dummy);
  bm_no = XCreateBitmapFromData((Display*)m_nativeDisplay, (Window)m_nativeWindow, bm_no_data, 8, 8);
  no_ptr = XCreatePixmapCursor((Display*)m_nativeDisplay, bm_no, bm_no, &black, &black, 0, 0);

  XDefineCursor((Display*)m_nativeDisplay, (Window)m_nativeWindow, no_ptr);
  XFreeCursor((Display*)m_nativeDisplay, no_ptr);
  if (bm_no != None)
    XFreePixmap((Display*)m_nativeDisplay, bm_no);
  XFreeColors((Display*)m_nativeDisplay, cmap, &black.pixel, 1, 0);
  return true;
}

