#pragma once
/*
 *      Copyright (C) 2012 Team XBMC
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

#include "WinEGLPlatformGeneric.h"
#include <string>
#include "guilib/Resolution.h"

class CWinEGLPlatformAmlogic : public CWinEGLPlatformGeneric
{
public:
  CWinEGLPlatformAmlogic();

  virtual EGLNativeWindowType InitWindowSystem(EGLNativeDisplayType nativeDisplay, int width, int height, int bpp);
  virtual void DestroyWindowSystem(EGLNativeWindowType native_window);
  virtual bool SetDisplayResolution(RESOLUTION_INFO &res);
  virtual bool ClampToGUIDisplayLimits(int &width, int &height);
  virtual bool ProbeDisplayResolutions(std::vector<RESOLUTION_INFO> &resolutions);
  virtual bool ShowWindow(bool show);

  // amlogic specific functions
  void ProbeHDMIAudio();
  void SetCpuMinLimit(bool limit);
  bool SetDisplayResolution(const char* resolution);
  void EnableFreeScale();
  void DisableFreeScale();

private:
  std::string m_framebuffer_name;
};
