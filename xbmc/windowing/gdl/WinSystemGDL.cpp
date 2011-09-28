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

#include "system.h"

#ifdef HAS_GDL
#include "gdl_types.h"

#define DEFAULT_OVERSCAN 0

#include "WinSystemGDL.h"
#include "utils/log.h"
#include "threads/SingleLock.h"
#include "settings/Settings.h"
#include "settings/GUISettings.h"
#include "guilib/GraphicContext.h"


CWinSystemGDL::CWinSystemGDL() : CWinSystemBase()
{
  m_eWindowSystem = WINDOW_SYSTEM_GDL;
}

CWinSystemGDL::~CWinSystemGDL()
{
  DestroyWindowSystem();
}

bool CWinSystemGDL::InitWindowSystem()
{
  if(gdl_init(0) != GDL_SUCCESS)
  {
    CLog::Log(LOGNONE, "CWinSystemGDL::InitWindowSystem gdl_init failed");
    return false;
  }
  gdl_plane_reset_all();

  if (!CWinSystemBase::InitWindowSystem())
    return false;

  return true;
}

bool CWinSystemGDL::DestroyWindowSystem()
{
  gdl_close();

  return true;
}

bool CWinSystemGDL::EnableHDCP(bool enable)
{
  gdl_boolean_t hdcpControlEnabled = enable ? GDL_TRUE : GDL_FALSE;

  if (gdl_port_set_attr(GDL_PD_ID_HDMI, GDL_PD_ATTR_ID_HDCP,
      &hdcpControlEnabled) != GDL_SUCCESS)
  {
    CLog::Log(LOGNONE, "Could not enable HDCP control");
    return false;
  }

  return true;
}

bool CWinSystemGDL::CreateNewWindow(const CStdString& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  gdl_display_info_t di;
  gdl_pixel_format_t pixelFormat = GDL_PF_ARGB_32;
  gdl_color_space_t  colorSpace  = GDL_COLOR_SPACE_RGB;
  gdl_rectangle_t    srcRect;
  gdl_rectangle_t    dstRect;
  gdl_ret_t          rc = GDL_SUCCESS;
  gdl_boolean_t      hdcpControlEnabled = GDL_FALSE;
  gdl_boolean_t      scalineEnabled = GDL_FALSE;
  gdl_boolean_t      alphaPremult = GDL_TRUE;
  BlackLevelType     blackLevel;

  // see CWinSystemGDL::UpdateResolutions.
  if ((res.iScreenWidth > res.iWidth) && (res.iScreenHeight > res.iHeight))
    scalineEnabled = GDL_TRUE;

  CLog::Log(LOGNONE, "WinSystemGDL creating new window: Width = %d  Height = %d", res.iScreenWidth, res.iScreenHeight);

  // print overscan values
  int left   = res.Overscan.left;
  int top    = res.Overscan.top;
  int right  = res.Overscan.right;
  int bottom = res.Overscan.bottom;

  CLog::Log(LOGNONE, "WinSystemGDL overscan values left %d top %d right %d bottom %d", left, top, right, bottom);

  gdl_refresh_t refresh;
  if (res.fRefreshRate < 23.99f && res.fRefreshRate > 23.97)
    refresh = GDL_REFRESH_23_98;
  else if (res.fRefreshRate < 24.01f && res.fRefreshRate > 23.99)
    refresh = GDL_REFRESH_24;
  else if (res.fRefreshRate < 25.01f && res.fRefreshRate > 24.99)
    refresh = GDL_REFRESH_25;
  else if (res.fRefreshRate < 29.98f && res.fRefreshRate > 29.96)
    refresh = GDL_REFRESH_29_97;
  else if (res.fRefreshRate < 30.01f && res.fRefreshRate > 29.99)
    refresh = GDL_REFRESH_30;
  else if (res.fRefreshRate < 50.01f && res.fRefreshRate > 49.99)
    refresh = GDL_REFRESH_50;
  else if (res.fRefreshRate < 59.95f && res.fRefreshRate > 59.93)
    refresh = GDL_REFRESH_59_94;
  else if (res.fRefreshRate < 60.01f && res.fRefreshRate > 59.99)
    refresh = GDL_REFRESH_60;
  else if (res.fRefreshRate < 48.01f && res.fRefreshRate > 47.99)
    refresh = GDL_REFRESH_48;
  else if (res.fRefreshRate < 47.97f && res.fRefreshRate > 47.95)
    refresh = GDL_REFRESH_47_96;
  else
  {
    CLog::Log(LOGERROR, "Unsupported refresh rate: %f", res.fRefreshRate);
    return false;
  }

  gdl_plane_reset_all();

  // Default values of optional args
  memset(&di, 0, sizeof (gdl_display_info_t));
  di.id                = GDL_DISPLAY_ID_0;
  di.flags             = 0;
  di.bg_color          = 0;
  di.gamma             = GDL_GAMMA_LINEAR;
  di.tvmode.width      = res.iScreenWidth;
  di.tvmode.height     = res.iScreenHeight;
  di.tvmode.refresh    = refresh;
  di.tvmode.interlaced = (res.dwFlags & D3DPRESENTFLAG_INTERLACED ? GDL_TRUE : GDL_FALSE);
  di.tvmode.stereo_type= GDL_STEREO_NONE;
  if (di.tvmode.height > 576)
    di.color_space = GDL_COLOR_SPACE_BT709;
  else
    di.color_space = GDL_COLOR_SPACE_BT601;

  rc = gdl_set_display_info(&di);
  if ( rc != GDL_SUCCESS)
  {
    CLog::Log(LOGNONE, "Could not set display mode for display 0");
    return false;
  }

  m_nWidth  = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  dstRect.origin.x = 0;
  dstRect.origin.y = 0;
  dstRect.width  = res.iScreenWidth;
  dstRect.height = res.iScreenHeight;

  srcRect.origin.x = 0;
  srcRect.origin.y = 0;
  srcRect.width  = res.iWidth;
  srcRect.height = res.iHeight;

  //sdd  blackLevel = g_settings.GetBlackLevelAsEnum(g_guiSettings.GetString("videoscreen.blacklevel"));
  blackLevel = BLACK_LEVEL_VIDEO;
  if(blackLevel == BLACK_LEVEL_PC)
    EnableHDMIClamp(false);
  else if(blackLevel == BLACK_LEVEL_VIDEO)
    EnableHDMIClamp(true);
  else
    CLog::Log(LOGNONE, "CWinSystemGDL::CreateNewWindow Could not HDMI black levels");

  if (gdl_port_set_attr(GDL_PD_ID_HDMI, GDL_PD_ATTR_ID_HDCP, &hdcpControlEnabled) != GDL_SUCCESS)
    CLog::Log(LOGNONE, "Could not enable HDCP control");

  m_gdlPlane = GDL_GRAPHICS_PLANE;

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_config_begin(m_gdlPlane);

  rc = gdl_plane_reset(m_gdlPlane);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_config_begin(m_gdlPlane);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_SRC_COLOR_SPACE, &colorSpace);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_PIXEL_FORMAT, &pixelFormat);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &dstRect);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_SRC_RECT, &srcRect);

  if(GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_SCALE, &scalineEnabled);

  if(GDL_SUCCESS == rc)
    rc = gdl_plane_set_attr(GDL_PLANE_ALPHA_PREMULT, &alphaPremult);

  if (GDL_SUCCESS == rc)
    rc = gdl_plane_config_end(GDL_FALSE);
  else
    gdl_plane_config_end(GDL_TRUE);

  if (GDL_SUCCESS != rc)
  {
    CLog::Log(LOGNONE, "GDL configuration failed! GDL error code is 0x%x", rc);
    return false;
  }

  CLog::Log(LOGNONE, "GDL plane setup complete");

  if (!m_eglBinding.CreateWindow(EGL_DEFAULT_DISPLAY, (NativeWindowType) m_gdlPlane))
    return false;

  CLog::Log(LOGNONE, "GDL plane attach to EGL complete");

  return true;
}

bool CWinSystemGDL::DestroyWindow()
{
  if (!m_eglBinding.DestroyWindow())
    return false;

  return true;
}

bool CWinSystemGDL::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  return true;
}

bool CWinSystemGDL::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  CLog::Log(LOGNONE, "CWinSystemGDL::SetFullScreen");
  m_nWidth  = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  m_eglBinding.ReleaseSurface();
  CreateNewWindow("", fullScreen, res, NULL);

  return true;
}

bool CWinSystemGDL::EnableHDMIClamp(bool bEnable)
{
  gdl_boolean_t hdmiClamp = bEnable ? GDL_TRUE : GDL_FALSE;

  if(gdl_port_set_attr(GDL_PD_ID_HDMI, GDL_PD_ATTR_ID_OUTPUT_CLAMP, &hdmiClamp) != GDL_SUCCESS)
  {
    CLog::Log(LOGNONE, "Could not set gdl output clamp");
    return false;
  }

  gdl_pd_attribute_t test;
  gdl_port_get_attr(GDL_PD_ID_HDMI, GDL_PD_ATTR_ID_OUTPUT_CLAMP, &test);

  return true;
}

void CWinSystemGDL::UpdateResolutions()
{
  gdl_display_id_t   display_id = GDL_DISPLAY_ID_0;
  gdl_pd_id_t        pd_id = GDL_PD_MIN_SUPPORTED_DRIVERS;
  gdl_pd_attribute_t attr;
  gdl_ret_t          rc;
  gdl_tvmode_t       m;
  int                i = 0;
  RESOLUTION         Res720p = RES_INVALID;
  RESOLUTION         res_index = RES_DESKTOP;

  CLog::Log(LOGNONE, "Display <%d> supports the following modes:", display_id);

  // Traverse through all ports
  while (pd_id < GDL_PD_MAX_SUPPORTED_DRIVERS)
  {
    // Find any port on given display
    rc = gdl_port_get_attr(pd_id, GDL_PD_ATTR_ID_DISPLAY_PIPE, &attr);
    if ((rc == GDL_SUCCESS) && (attr.content._uint.value == display_id))
    {
      // Get modes supported by given port, 1 by 1
      while (gdl_get_port_tvmode_by_index(pd_id, i++, &m) == GDL_SUCCESS)
      {
        bool ignored = true;

        // Check if given mode is supported on given display
        if (gdl_check_tvmode(display_id, &m) == GDL_SUCCESS)
        {
          // We only care about progressive 59.94 or 50 Hz resolutions
          if (!m.interlaced && (m.refresh == GDL_REFRESH_59_94 || m.refresh == GDL_REFRESH_50))
          {
            ignored = false;
          }

          CLog::Log(LOGNONE, "Resolution: %dx%d%c%s - %s\n",
              m.width,
              m.height,
              m.interlaced ? 'i' : 'p',
              gdl_dbg_string_refresh(m.refresh),
              (ignored ? "ignored" : "added"));

          if (ignored)
            continue;

          float refresh;
          switch (m.refresh)
          {
            case GDL_REFRESH_23_98: refresh = 23.98; break;
            case GDL_REFRESH_24:    refresh = 24.00; break;
            case GDL_REFRESH_25:    refresh = 25.00; break;
            case GDL_REFRESH_29_97: refresh = 29.97; break;
            case GDL_REFRESH_30:    refresh = 30.00; break;
            case GDL_REFRESH_50:    refresh = 50.00; break;
            case GDL_REFRESH_59_94: refresh = 59.94; break;
            case GDL_REFRESH_60:    refresh = 60.00; break;
            case GDL_REFRESH_48:    refresh = 48.00; break;
            case GDL_REFRESH_47_96: refresh = 47.96; break;
            default:                refresh = 0.0;   break;
          }

          // if this is a new setting,
          // create a new empty setting to fill in.
          if (g_settings.m_ResInfo.size() <= res_index)
          {
            RESOLUTION_INFO res;
            g_settings.m_ResInfo.push_back(res);
          }

          int gui_width  = m.width;
          int gui_height = m.height;
          if (!m.interlaced && m.width == 1920 && m.height == 1080)
          {
            // CE41xx can not render GUI fast enough in 1080p.
            // So we will use hw scaler to render GUI at 720p
            // and hw scale that to 1080p display.
            gui_width  = 1280;
            gui_height = 720;
          }
          
          if (DEFAULT_OVERSCAN > 0)
          {
            g_settings.m_ResInfo[res_index].Overscan.left = (int) ((float) gui_width  * DEFAULT_OVERSCAN);
            g_settings.m_ResInfo[res_index].Overscan.top  = (int) ((float) gui_height * DEFAULT_OVERSCAN);
            g_settings.m_ResInfo[res_index].Overscan.right  = gui_width  - (int) ((float) gui_width  * DEFAULT_OVERSCAN);
            g_settings.m_ResInfo[res_index].Overscan.bottom = gui_height - (int) ((float) gui_height * DEFAULT_OVERSCAN);
          }
          else
          {
            g_settings.m_ResInfo[res_index].Overscan.left = 0;
            g_settings.m_ResInfo[res_index].Overscan.top  = 0;
            g_settings.m_ResInfo[res_index].Overscan.right  = gui_width;
            g_settings.m_ResInfo[res_index].Overscan.bottom = gui_height;
          }
          g_settings.m_ResInfo[res_index].iScreen     = 0;
          g_settings.m_ResInfo[res_index].bFullScreen = true;
          g_settings.m_ResInfo[res_index].iSubtitles  = (int)(0.965 * gui_height);
          if (m.interlaced)
            g_settings.m_ResInfo[res_index].dwFlags = D3DPRESENTFLAG_INTERLACED;
          else
            g_settings.m_ResInfo[res_index].dwFlags = D3DPRESENTFLAG_PROGRESSIVE;
          g_settings.m_ResInfo[res_index].fRefreshRate = refresh;
          g_settings.m_ResInfo[res_index].fPixelRatio  = 1.0f;
          g_settings.m_ResInfo[res_index].iWidth  = gui_width;
          g_settings.m_ResInfo[res_index].iHeight = gui_height;
          g_settings.m_ResInfo[res_index].iScreenWidth  = m.width;
          g_settings.m_ResInfo[res_index].iScreenHeight = m.height;
          g_settings.m_ResInfo[res_index].strMode.Format("%dx%d @ %.2f%s - Full Screen",
            m.width, m.height, refresh, m.interlaced ? "i" : "");

          if (!m.interlaced && (m.width == 1280) && (m.height == 720) && (m.refresh == GDL_REFRESH_59_94))
            Res720p = res_index;

          res_index = (RESOLUTION) (((int) res_index) + 1);
        }
      }

      // We are done
      break;
    }

    pd_id = (gdl_pd_id_t) (((int) pd_id) + 1);
  }

  // swap desktop index for 720p if available
  if (Res720p != RES_INVALID)
  {
    CLog::Log(LOGNONE, "Found 720p at %d, setting to RES_DESKTOP at %d", (int)Res720p, (int)RES_DESKTOP);

    RESOLUTION_INFO desktop = g_settings.m_ResInfo[RES_DESKTOP];
    g_settings.m_ResInfo[RES_DESKTOP] = g_settings.m_ResInfo[Res720p];
    g_settings.m_ResInfo[Res720p] = desktop;
  }
  else
  {
    CLog::Log(LOGNONE, "Warning: 720p not supported, desktop not changed");
  }
}

void CWinSystemGDL::NotifyAppActiveChange(bool bActivated)
{
}

bool CWinSystemGDL::Minimize()
{
  return true;
}

bool CWinSystemGDL::Restore()
{
  return true;
}

bool CWinSystemGDL::Hide()
{
  gdl_boolean_t mute = GDL_TRUE;

  gdl_plane_config_begin(GDL_GRAPHICS_PLANE);
  gdl_plane_set_attr(GDL_PLANE_VID_MUTE, &mute);
  gdl_plane_config_end(GDL_FALSE);
  return true;
}

bool CWinSystemGDL::Show(bool raise)
{
  gdl_boolean_t mute = GDL_FALSE;

  gdl_plane_config_begin(GDL_GRAPHICS_PLANE);
  gdl_plane_set_attr(GDL_PLANE_VID_MUTE, &mute);
  gdl_plane_config_end(GDL_FALSE);
  return true;
}

#endif
