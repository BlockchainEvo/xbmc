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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include "WinEGLPlatformAmlogic.h"

static int set_sysfs_str(const char *path, const char *val)
{
  int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
  if (fd >= 0)
  {
    write(fd, val, strlen(val));
    close(fd);
    return 0;
  }
  return -1;
}

static int get_sysfs_str(const char *path, char *valstr, const int size)
{
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
  {
    read(fd, valstr, size - 1);
    valstr[strlen(valstr)] = '\0';
    close(fd);
  } else {
    sprintf(valstr, "%s", "fail");
    return -1;
  }
  return 0;
}

static int set_sysfs_int(const char *path, const int val)
{
  char bcmd[16];
  int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
  if (fd >= 0)
  {
    sprintf(bcmd, "%d", val);
    write(fd, bcmd, strlen(bcmd));
    close(fd);
    return 0;
  }
  return -1;
}

static int get_sysfs_int(const char *path)
{
  int val = 0;
  char bcmd[16];
  int fd = open(path, O_RDONLY);
  if (fd >= 0)
  {
    read(fd, bcmd, sizeof(bcmd));
    val = strtol(bcmd, NULL, 16);
    close(fd);
  }
  return val;
}

////////////////////////////////////////////////////////////////////////////////////////////
CWinEGLPlatformAmlogic::CWinEGLPlatformAmlogic()
{
  const char *env_framebuffer = getenv("FRAMEBUFFER");

  // default to framebuffer 0
  m_framebuffer_name = "fb0";
  if (env_framebuffer)
  {
    std::string framebuffer(env_framebuffer);
    std::string::size_type start = framebuffer.find("fb");
    m_framebuffer_name = framebuffer.substr(start);
  }
}

EGLNativeWindowType CWinEGLPlatformAmlogic::InitWindowSystem(EGLNativeDisplayType nativeDisplay, int width, int height, int bpp)
{
  // hack the cpu min limit here, InitWindowSystem/DestroyWindowSystem
  // are only called once and we can adjust the min cpu freq and reset
  // it on quit.
  SetCpuMinLimit(true);

  fbdev_window *native_window;
  native_window = (fbdev_window*)calloc(1, sizeof(fbdev_window));
  if (width == 1920 && height == 1080)
  {
    width  = 1280;
    height = 720;
  }
  native_window->width  = width;
  native_window->height = height;

  m_nativeDisplay = nativeDisplay;
  m_nativeWindow = native_window;
  return (EGLNativeWindowType)native_window;
}

void CWinEGLPlatformAmlogic::DestroyWindowSystem(EGLNativeWindowType native_window)
{
  SetCpuMinLimit(false);

  free(native_window);
  DisableFreeScale();
}

bool CWinEGLPlatformAmlogic::SetDisplayResolution(int width, int height, float refresh, bool interlace)
{
  if (width == 1920 && height == 1080 && !interlace)
    SetDisplayResolution("1080p");
  else if (width == 1920 && height == 1080)
    SetDisplayResolution("1080i");
  else if (width == 1280 && height == 720)
    SetDisplayResolution("720p");
  else if (width == 720  && height == 480)
    SetDisplayResolution("480p");

  return true;
}

bool CWinEGLPlatformAmlogic::ClampToGUIDisplayLimits(int &width, int &height)
{
  bool rtn = false;
  if (width == 1920 && height == 1080)
  {
    // we can not render GUI fast enough in 1080p.
    // So we will render GUI at 720p and scale that to 1080p display.
    width  = 1280;
    height = 720;
    rtn = true;
  }
  return rtn;
}

bool CWinEGLPlatformAmlogic::ProbeDisplayResolutions(std::vector<CStdString> &resolutions)
{
  int fd = open("/sys/class/amhdmitx/amhdmitx0/disp_cap", O_RDONLY);
  if (fd >= 0)
  {
    char valstr[256] = {0};

    read(fd, valstr, sizeof(valstr) - 1);
    valstr[strlen(valstr)] = '\0';
    close(fd);
    
    std::vector<CStdString> probe_str;
    StringUtils::SplitString(valstr, "\n", probe_str);

    resolutions.clear();
    for (size_t i = 0; i < probe_str.size(); i++)
    {
      // strips, for example, 720p* to 720p
      if (probe_str[i].Right(1) == "*")
        probe_str[i] = probe_str[i].Left(std::max(0, (int)probe_str[i].size() - 1));
      
      if (probe_str[i].Equals("480p"))            resolutions.push_back("720x480p60Hzp");
      else if (probe_str[i].Equals("720p"))       resolutions.push_back("1280x720p60Hz");
      else if (probe_str[i].Equals("720p50hz"))   resolutions.push_back("1280x720p50Hz");
      else if (probe_str[i].Equals("1080i"))      resolutions.push_back("1920x1080i60Hz");
      else if (probe_str[i].Equals("1080i50hz"))  resolutions.push_back("1920x1080i50Hz");
      else if (probe_str[i].Equals("1080p"))      resolutions.push_back("1920x1080p60Hz");
      else if (probe_str[i].Equals("1080p50hz"))  resolutions.push_back("1920x1080p50Hz");
    }
    if (resolutions.size() == 0)
      resolutions.push_back("1280x720p60Hz");
  }
  
  return false;
}

bool CWinEGLPlatformAmlogic::ShowWindow(bool show)
{
  std::string blank_framebuffer = "/sys/class/graphics/" + m_framebuffer_name + "/blank";
  if (show)
    set_sysfs_int(blank_framebuffer.c_str(), 0);
  else
    set_sysfs_int(blank_framebuffer.c_str(), 1);
  return true;
}

void CWinEGLPlatformAmlogic::SetCpuMinLimit(bool limit)
{
  // when playing hw decoded audio, we cannot drop below 600MHz
  // or risk hw audio issues. AML code does a 2X scaling based off
  // /sys/class/audiodsp/codec_mips but tests show that this is
  // seems risky so we just clamp to 600Mhz to be safe.

  // only adjust if we are running "ondemand"
  char scaling_governor[256] = {0};
  get_sysfs_str("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", scaling_governor, 255);
  if (strncmp(scaling_governor, "ondemand", 255))
    return;

  int freq;
  if (limit)
    freq = 600000;
  else
    freq = get_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
  set_sysfs_int("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", freq);
}

void CWinEGLPlatformAmlogic::ProbeHDMIAudio()
{
  std::vector<CStdString> audio_formats;
  // Audio {format, channel, freq, cce}
  // {1, 7, 7f, 7}
  // {7, 5, 1e, 0}
  // {2, 5, 7, 0}
  // {11, 7, 7e, 1}
  // {10, 7, 6, 0}
  // {12, 7, 7e, 0}

  int fd = open("/sys/class/amhdmitx/amhdmitx0/edid", O_RDONLY);
  if (fd >= 0)
  {
    char valstr[1024] = {0};

    read(fd, valstr, sizeof(valstr) - 1);
    valstr[strlen(valstr)] = '\0';
    close(fd);

    std::vector<CStdString> probe_str;
    StringUtils::SplitString(valstr, "\n", probe_str);

    for (size_t i = 0; i < probe_str.size(); i++)
    {
      if (probe_str[i].find("Audio") == std::string::npos)
      {
        for (size_t j = i+1; j < probe_str.size(); j++)
        {
          if      (probe_str[i].find("{1,")  != std::string::npos)
            printf(" PCM found {1,\n");
          else if (probe_str[i].find("{2,")  != std::string::npos)
            printf(" AC3 found {2,\n");
          else if (probe_str[i].find("{3,")  != std::string::npos)
            printf(" MPEG1 found {3,\n");
          else if (probe_str[i].find("{4,")  != std::string::npos)
            printf(" MP3 found {4,\n");
          else if (probe_str[i].find("{5,")  != std::string::npos)
            printf(" MPEG2 found {5,\n");
          else if (probe_str[i].find("{6,")  != std::string::npos)
            printf(" AAC found {6,\n");
          else if (probe_str[i].find("{7,")  != std::string::npos)
            printf(" DTS found {7,\n");
          else if (probe_str[i].find("{8,")  != std::string::npos)
            printf(" ATRAC found {8,\n");
          else if (probe_str[i].find("{9,")  != std::string::npos)
            printf(" One_Bit_Audio found {9,\n");
          else if (probe_str[i].find("{10,") != std::string::npos)
            printf(" Dolby found {10,\n");
          else if (probe_str[i].find("{11,") != std::string::npos)
            printf(" DTS_HD found {11,\n");
          else if (probe_str[i].find("{12,") != std::string::npos)
            printf(" MAT found {12,\n");
          else if (probe_str[i].find("{13,") != std::string::npos)
            printf(" ATRAC found {13,\n");
          else if (probe_str[i].find("{14,") != std::string::npos)
            printf(" WMA found {14,\n");
          else
            break;
        }
        break;
      }
    }
  }
}

bool CWinEGLPlatformAmlogic::SetDisplayResolution(const char *resolution)
{
  CStdString modestr;
  modestr = resolution;

  // switch display resolution
  set_sysfs_str("/sys/class/display/mode", modestr.c_str());
  usleep(250 * 1000);

  // setup gui freescale depending on display resolution
  DisableFreeScale();
  if (modestr.Equals("1080i") || modestr.Equals("1080p"))
    EnableFreeScale();

  return true;
}

void CWinEGLPlatformAmlogic::EnableFreeScale()
{
  // remove default OSD and video path (default_osd default)
  set_sysfs_str("/sys/class/vfm/map", "rm all");
  usleep(60 * 1000);

  // add OSD path
  set_sysfs_str("/sys/class/vfm/map", "add osdpath osd amvideo");
  // enable OSD free scale using frame buffer size of 720p
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb0/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb0/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb1/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb1/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 1);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 1);
  usleep(60 * 1000);
  // remove OSD path
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_str("/sys/class/vfm/map", "rm osdpath");
  usleep(60 * 1000);
  // add video path
  set_sysfs_str("/sys/class/vfm/map", "add videopath decoder ppmgr amvideo");
  // enable video free scale (scaling to 1920x1080 with frame buffer size 1280x720)
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 0);
  set_sysfs_int("/sys/class/video/disable_video", 1);
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 1);
  set_sysfs_str("/sys/class/ppmgr/ppscaler_rect", "0 0 1919 1079 0");
  set_sysfs_str("/sys/class/ppmgr/disp", "1280 720");
  usleep(60 * 1000);
  //
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb0/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb0/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb1/scale_width",  1280);
  set_sysfs_int("/sys/class/graphics/fb1/scale_height", 720);
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 1);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 1);
  usleep(60 * 1000);
  //
  set_sysfs_int("/sys/class/video/disable_video", 2);
  set_sysfs_str("/sys/class/display/axis", "0 0 1279 719 0 0 0 0");
  set_sysfs_str("/sys/class/ppmgr/ppscaler_rect", "0 0 1279 719 1");
}

void CWinEGLPlatformAmlogic::DisableFreeScale()
{
  // turn off frame buffer freescale
  set_sysfs_int("/sys/class/graphics/fb0/free_scale", 0);
  set_sysfs_int("/sys/class/graphics/fb1/free_scale", 0);
  // revert to default video paths
  set_sysfs_str("/sys/class/vfm/map", "rm all");
  set_sysfs_str("/sys/class/vfm/map", "add default_osd osd amvideo");
  set_sysfs_str("/sys/class/vfm/map", "add default decoder ppmgr amvideo");
  // disable post processing scaler and disable_video special mode
  set_sysfs_int("/sys/class/ppmgr/ppscaler", 0);
  set_sysfs_int("/sys/class/video/disable_video", 0);

  // revert display axis
  int fd0;
  std::string framebuffer = "/dev/" + m_framebuffer_name;

  if ((fd0 = open(framebuffer.c_str(), O_RDWR)) >= 0)
  {
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd0, FBIOGET_VSCREENINFO, &vinfo) == 0)
    {
      char daxis_str[255] = {0};
      sprintf(daxis_str, "%d %d %d %d 0 0 0 0", 0, 0, vinfo.xres, vinfo.yres);
      set_sysfs_str("/sys/class/display/axis", daxis_str);
    }
    close(fd0);
  }
}
