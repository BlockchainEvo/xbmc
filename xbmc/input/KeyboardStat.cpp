/*
 *      Copyright (C) 2007-2010 Team XBMC
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

// C++ Implementation: CKeyboard

// Comment OUT, if not really debugging!!!:
//#define DEBUG_KEYBOARD_GETCHAR

#include "KeyboardStat.h"
#include "KeyboardLayoutConfiguration.h"
#include "XBMC_events.h"
#include "utils/TimeUtils.h"

#if defined(_LINUX) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#endif

CKeyboardStat g_Keyboard;

struct XBMC_KeyMapping
{
  int   source;
  BYTE  VKey;
  char  Ascii;
  WCHAR Unicode;
};

// Convert control keypresses e.g. ctrl-A from 0x01 to 0x41
static XBMC_KeyMapping g_mapping_ctrlkeys[] =
{ {0x30, 0x60, XBMCK_0, XBMCK_0}
, {0x31, 0x61, XBMCK_1, XBMCK_1}
, {0x32, 0x62, XBMCK_2, XBMCK_2}
, {0x33, 0x63, XBMCK_3, XBMCK_3}
, {0x34, 0x64, XBMCK_4, XBMCK_4}
, {0x35, 0x65, XBMCK_5, XBMCK_5}
, {0x36, 0x66, XBMCK_6, XBMCK_6}
, {0x37, 0x67, XBMCK_7, XBMCK_7}
, {0x38, 0x68, XBMCK_8, XBMCK_8}
, {0x39, 0x69, XBMCK_9, XBMCK_9}
, {0x61, 0x41, XBMCK_a, XBMCK_a}
, {0x62, 0x42, XBMCK_b, XBMCK_b}
, {0x63, 0x43, XBMCK_c, XBMCK_c}
, {0x64, 0x44, XBMCK_d, XBMCK_d}
, {0x65, 0x45, XBMCK_e, XBMCK_e}
, {0x66, 0x46, XBMCK_f, XBMCK_f}
, {0x67, 0x47, XBMCK_g, XBMCK_g}
, {0x68, 0x48, XBMCK_h, XBMCK_h}
, {0x69, 0x49, XBMCK_i, XBMCK_i}
, {0x6a, 0x4a, XBMCK_j, XBMCK_j}
, {0x6b, 0x4b, XBMCK_k, XBMCK_k}
, {0x6c, 0x4c, XBMCK_l, XBMCK_l}
, {0x6d, 0x4d, XBMCK_m, XBMCK_m}
, {0x6e, 0x4e, XBMCK_n, XBMCK_n}
, {0x6f, 0x4f, XBMCK_o, XBMCK_o}
, {0x70, 0x50, XBMCK_p, XBMCK_p}
, {0x71, 0x51, XBMCK_q, XBMCK_q}
, {0x72, 0x52, XBMCK_r, XBMCK_r}
, {0x73, 0x53, XBMCK_s, XBMCK_s}
, {0x74, 0x54, XBMCK_t, XBMCK_t}
, {0x75, 0x55, XBMCK_u, XBMCK_u}
, {0x76, 0x56, XBMCK_v, XBMCK_v}
, {0x77, 0x57, XBMCK_w, XBMCK_w}
, {0x78, 0x58, XBMCK_x, XBMCK_x}
, {0x79, 0x59, XBMCK_y, XBMCK_y}
, {0x7a, 0x5a, XBMCK_z, XBMCK_z}
};

// based on the evdev mapped scancodes in /user/share/X11/xkb/keycodes
static XBMC_KeyMapping g_mapping_evdev[] =
{ { 121, 0xad } // Volume mute
, { 122, 0xae } // Volume down
, { 123, 0xaf } // Volume up
, { 127, 0x20 } // Pause
, { 135, 0x5d } // Right click
, { 136, 0xb2 } // Stop
, { 138, 0x49 } // Info
, { 147, 0x4d } // Menu
, { 150, 0x9f } // Sleep
, { 152, 0xb8 } // Launch file browser
, { 163, 0xb4 } // Launch Mail
, { 164, 0xab } // Browser favorites
, { 166, 0x08 } // Back
, { 167, 0xa7 } // Browser forward
, { 171, 0xb0 } // Next track
, { 172, 0xb3 } // Play_Pause
, { 173, 0xb1 } // Prev track
, { 174, 0xb2 } // Stop
, { 176, 0x52 } // Rewind
, { 179, 0xb9 } // Launch media center
, { 180, 0xac } // Browser home
, { 181, 0xa8 } // Browser refresh
, { 214, 0x1B } // Close
, { 215, 0xb3 } // Play_Pause
, { 216, 0x46 } // Forward
//, {167, 0xb3 } // Record
};

// following scancode infos are
// 1. from ubuntu keyboard shortcut (hex) -> predefined
// 2. from unix tool xev and my keyboards (decimal)
// m_VKey infos from CharProbe tool
// Can we do the same for XBoxKeyboard and DirectInputKeyboard? Can we access the scancode of them? By the way how does SDL do it? I can't find it. (Automagically? But how exactly?)
// Some pairs of scancode and virtual keys are only known half

// special "keys" above F1 till F12 on my MS natural keyboard mapped to virtual keys "F13" till "F24"):
static XBMC_KeyMapping g_mapping_ubuntu[] =
{ { 0xf5, 0x7c } // F13 Launch help browser
, { 0x87, 0x7d } // F14 undo
, { 0x8a, 0x7e } // F15 redo
, { 0x89, 0x7f } // F16 new
, { 0xbf, 0x80 } // F17 open
, { 0xaf, 0x81 } // F18 close
, { 0xe4, 0x82 } // F19 reply
, { 0x8e, 0x83 } // F20 forward
, { 0xda, 0x84 } // F21 send
//, { 0x, 0x85 } // F22 check spell (doesn't work for me with ubuntu)
, { 0xd5, 0x86 } // F23 save
, { 0xb9, 0x87 } // 0x2a?? F24 print
        // end of special keys above F1 till F12

, { 234, 0xa6 } // Browser back
, { 233, 0xa7 } // Browser forward
, { 231, 0xa8 } // Browser refresh
//, { , 0xa9 } // Browser stop
, { 122, 0xaa } // Browser search
, { 0xe5, 0xaa } // Browser search
, { 230, 0xab } // Browser favorites
, { 130, 0xac } // Browser home
, { 0xa0, 0xad } // Volume mute
, { 0xae, 0xae } // Volume down
, { 0xb0, 0xaf } // Volume up
, { 0x99, 0xb0 } // Next track
, { 0x90, 0xb1 } // Prev track
, { 0xa4, 0xb2 } // Stop
, { 0xa2, 0xb3 } // Play_Pause
, { 0xec, 0xb4 } // Launch mail
, { 129, 0xb5 } // Launch media_select
, { 198, 0xb6 } // Launch App1/PC icon
, { 0xa1, 0xb7 } // Launch App2/Calculator
, { 34, 0xba } // OEM 1: [ on us keyboard
, { 51, 0xbf } // OEM 2: additional key on european keyboards between enter and ' on us keyboards
, { 47, 0xc0 } // OEM 3: ; on us keyboards
, { 20, 0xdb } // OEM 4: - on us keyboards (between 0 and =)
, { 49, 0xdc } // OEM 5: ` on us keyboards (below ESC)
, { 21, 0xdd } // OEM 6: =??? on us keyboards (between - and backspace)
, { 48, 0xde } // OEM 7: ' on us keyboards (on right side of ;)
//, { 0, 0xdf } // OEM 8
, { 94, 0xe2 } // OEM 102: additional key on european keyboards between left shift and z on us keyboards
//, { 0xb2, 0x } // Ubuntu default setting for launch browser
//, { 0x76, 0x } // Ubuntu default setting for launch music player
//, { 0xcc, 0x } // Ubuntu default setting for eject
, { 117, 0x5d } // right click
};

// OSX defines unicode values for non-printing keys which breaks the key parser, set m_wUnicode
static XBMC_KeyMapping g_mapping_npc[] =
{ { XBMCK_BACKSPACE, 0x08 }
, { XBMCK_TAB, 0x09 }
, { XBMCK_RETURN, 0x0d }
, { XBMCK_ESCAPE, 0x1b }
, { XBMCK_SPACE, 0x20, ' ', L' ' }
, { XBMCK_MENU, 0x5d }
, { XBMCK_KP0, 0x60 }
, { XBMCK_KP1, 0x61 }
, { XBMCK_KP2, 0x62 }
, { XBMCK_KP3, 0x63 }
, { XBMCK_KP4, 0x64 }
, { XBMCK_KP5, 0x65 }
, { XBMCK_KP6, 0x66 }
, { XBMCK_KP7, 0x67 }
, { XBMCK_KP8, 0x68 }
, { XBMCK_KP9, 0x69 }
, { XBMCK_KP_ENTER, 0x6C }
, { XBMCK_UP, 0x26 }
, { XBMCK_DOWN, 0x28 }
, { XBMCK_LEFT, 0x25 }
, { XBMCK_RIGHT, 0x27 }
, { XBMCK_INSERT, 0x2D }
, { XBMCK_DELETE, 0x2E }
, { XBMCK_HOME, 0x24 }
, { XBMCK_END, 0x23 }
, { XBMCK_F1, 0x70 }
, { XBMCK_F2, 0x71 }
, { XBMCK_F3, 0x72 }
, { XBMCK_F4, 0x73 }
, { XBMCK_F5, 0x74 }
, { XBMCK_F6, 0x75 }
, { XBMCK_F7, 0x76 }
, { XBMCK_F8, 0x77 }
, { XBMCK_F9, 0x78 }
, { XBMCK_F10, 0x79 }
, { XBMCK_F11, 0x7a }
, { XBMCK_F12, 0x7b }
, { XBMCK_KP_PERIOD, 0x6e }
, { XBMCK_KP_MULTIPLY, 0x6a }
, { XBMCK_KP_MINUS, 0x6d }
, { XBMCK_KP_PLUS, 0x6b }
, { XBMCK_KP_DIVIDE, 0x6f }
, { XBMCK_PAGEUP, 0x21 }
, { XBMCK_PAGEDOWN, 0x22 }
, { XBMCK_PRINT, 0x2a }
, { XBMCK_LSHIFT, 0xa0 }
, { XBMCK_RSHIFT, 0xa1 }
};

static bool LookupKeyMapping(BYTE* VKey, char* Ascii, WCHAR* Unicode, int source, XBMC_KeyMapping* map, int count)
{
  for(int i = 0; i < count; i++)
  {
    if(source == map[i].source)
    {
      if(VKey)
        *VKey    = map[i].VKey;
      if(Ascii)
        *Ascii   = map[i].Ascii;
      if(Unicode)
        *Unicode = map[i].Unicode;
      return true;
    }
  }
  return false;
}

CKeyboardStat::CKeyboardStat()
{
  memset(&m_lastKeysym, 0, sizeof(m_lastKeysym));
  m_lastKeyTime = 0;

  // In Linux the codes (numbers) for multimedia keys differ depending on
  // what driver is used and the evdev bool switches between the two.
  m_bEvdev = true;
}

CKeyboardStat::~CKeyboardStat()
{
}

void CKeyboardStat::Initialize()
{
/* this stuff probably doesn't belong here  *
 * but in some x11 specific WinEvents file  *
 * but for some reason the code to map keys *
 * to specific xbmc vkeys is here           */
#if defined(_LINUX) && !defined(__APPLE__)
  Display* dpy = XOpenDisplay(NULL);
  if (!dpy)
    return;

  XkbDescPtr desc;
  char* symbols;

  desc = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
  if(!desc)
  {
    XCloseDisplay(dpy);
    return;
  }

  symbols = XGetAtomName(dpy, desc->names->symbols);
  if(symbols)
  {
    CLog::Log(LOGDEBUG, "CKeyboardStat::Initialize - XKb symbols %s", symbols);
    if(strstr(symbols, "(evdev)"))
      m_bEvdev = true;
    else
      m_bEvdev = false;
  }

  XFree(symbols);
  XkbFreeKeyboard(desc, XkbAllComponentsMask, True);
  XCloseDisplay(dpy);
#endif
}

const CKey CKeyboardStat::ProcessKeyDown(XBMC_keysym& keysym)
{ uint8_t vkey;
  wchar_t unicode;
  char ascii;
  uint32_t modifiers;
  unsigned int held;

  ascii = 0;
  vkey = 0;
  unicode = keysym.unicode;
  held = 0;

  modifiers = 0;
  if (keysym.mod & XBMCKMOD_CTRL)
    modifiers |= CKey::MODIFIER_CTRL;
  if (keysym.mod & XBMCKMOD_SHIFT)
    modifiers |= CKey::MODIFIER_SHIFT;
  if (keysym.mod & XBMCKMOD_ALT)
    modifiers |= CKey::MODIFIER_ALT;
  if (keysym.mod & XBMCKMOD_RALT)
    modifiers |= CKey::MODIFIER_RALT;
  if (keysym.mod & XBMCKMOD_SUPER)
    modifiers |= CKey::MODIFIER_SUPER;

  CLog::Log(LOGDEBUG, "SDLKeyboard: scancode: %d, sym: %d, unicode: %d, modifier: %x", keysym.scancode, keysym.sym, keysym.unicode, keysym.mod);

  if ((keysym.unicode >= 'A' && keysym.unicode <= 'Z') ||
    (keysym.unicode >= 'a' && keysym.unicode <= 'z'))
  {
    ascii = (char)keysym.unicode;
    vkey = toupper(ascii);
  }
  else if (keysym.unicode >= '0' && keysym.unicode <= '9')
  {
    ascii = (char)keysym.unicode;
    vkey = 0x60 + ascii - '0'; // xbox keyboard routine appears to return 0x60->69 (unverified). Ideally this "fixing"
    // should be done in xbox routine, not in the sdl/directinput routines.
    // we should just be using the unicode/ascii value in all routines (perhaps with some
    // headroom for modifier keys?)
  }
  else
  {
    // see comment above about the weird use of vkey here...
    if (keysym.unicode == ')') { vkey = 0x60; ascii = ')'; }
    else if (keysym.unicode == '!') { vkey = 0x61; ascii = '!'; }
    else if (keysym.unicode == '@') { vkey = 0x62; ascii = '@'; }
    else if (keysym.unicode == '#') { vkey = 0x63; ascii = '#'; }
    else if (keysym.unicode == '$') { vkey = 0x64; ascii = '$'; }
    else if (keysym.unicode == '%') { vkey = 0x65; ascii = '%'; }
    else if (keysym.unicode == '^') { vkey = 0x66; ascii = '^'; }
    else if (keysym.unicode == '&') { vkey = 0x67; ascii = '&'; }
    else if (keysym.unicode == '*') { vkey = 0x68; ascii = '*'; }
    else if (keysym.unicode == '(') { vkey = 0x69; ascii = '('; }
    else if (keysym.unicode == ':') { vkey = 0xba; ascii = ':'; }
    else if (keysym.unicode == ';') { vkey = 0xba; ascii = ';'; }
    else if (keysym.unicode == '=') { vkey = 0xbb; ascii = '='; }
    else if (keysym.unicode == '+') { vkey = 0xbb; ascii = '+'; }
    else if (keysym.unicode == '<') { vkey = 0xbc; ascii = '<'; }
    else if (keysym.unicode == ',') { vkey = 0xbc; ascii = ','; }
    else if (keysym.unicode == '-') { vkey = 0xbd; ascii = '-'; }
    else if (keysym.unicode == '_') { vkey = 0xbd; ascii = '_'; }
    else if (keysym.unicode == '>') { vkey = 0xbe; ascii = '>'; }
    else if (keysym.unicode == '.') { vkey = 0xbe; ascii = '.'; }
    else if (keysym.unicode == '?') { vkey = 0xbf; ascii = '?'; } // 0xbf is OEM 2 Why is it assigned here?
    else if (keysym.unicode == '/') { vkey = 0xbf; ascii = '/'; }
    else if (keysym.unicode == '~') { vkey = 0xc0; ascii = '~'; }
    else if (keysym.unicode == '`') { vkey = 0xc0; ascii = '`'; }
    else if (keysym.unicode == '{') { vkey = 0xeb; ascii = '{'; }
    else if (keysym.unicode == '[') { vkey = 0xeb; ascii = '['; } // 0xeb is not defined by MS. Why is it assigned here?
    else if (keysym.unicode == '|') { vkey = 0xec; ascii = '|'; }
    else if (keysym.unicode == '\\') { vkey = 0xec; ascii = '\\'; }
    else if (keysym.unicode == '}') { vkey = 0xed; ascii = '}'; }
    else if (keysym.unicode == ']') { vkey = 0xed; ascii = ']'; } // 0xed is not defined by MS. Why is it assigned here?
    else if (keysym.unicode == '"') { vkey = 0xee; ascii = '"'; }
    else if (keysym.unicode == '\'') { vkey = 0xee; ascii = '\''; }

    // For control key combinations, e.g. ctrl-P, the UNICODE gets set
    // to 1 for ctrl-A, 2 for ctrl-B etc. This mapping sets the UNICODE
    // back to 'a', 'b', etc.
    // It isn't clear to me if this applies to Linux and Mac as well as
    // Windows.
    if (modifiers & CKey::MODIFIER_CTRL)
    {
      if (!vkey && !ascii)
        LookupKeyMapping(&vkey, NULL, &unicode
                       , keysym.sym
                       , g_mapping_ctrlkeys
                       , sizeof(g_mapping_ctrlkeys)/sizeof(g_mapping_ctrlkeys[0]));
    }

    /* Check for standard non printable keys */
    if (!vkey && !ascii)
      LookupKeyMapping(&vkey, NULL, &unicode
                     , keysym.sym
                     , g_mapping_npc
                     , sizeof(g_mapping_npc)/sizeof(g_mapping_npc[0]));


    if (!vkey && !ascii)
    {
      /* Check for linux defined non printable keys */
        if(m_bEvdev)
          LookupKeyMapping(&vkey, NULL, NULL
                         , keysym.scancode
                         , g_mapping_evdev
                         , sizeof(g_mapping_evdev)/sizeof(g_mapping_evdev[0]));
        else
          LookupKeyMapping(&vkey, NULL, NULL
                         , keysym.scancode
                         , g_mapping_ubuntu
                         , sizeof(g_mapping_ubuntu)/sizeof(g_mapping_ubuntu[0]));
    }

    if (!vkey && !ascii)
    {
      if (keysym.mod & XBMCKMOD_LSHIFT) vkey = 0xa0;
      else if (keysym.mod & XBMCKMOD_RSHIFT) vkey = 0xa1;
      else if (keysym.mod & XBMCKMOD_LALT) vkey = 0xa4;
      else if (keysym.mod & XBMCKMOD_RALT) vkey = 0xa5;
      else if (keysym.mod & XBMCKMOD_LCTRL) vkey = 0xa2;
      else if (keysym.mod & XBMCKMOD_RCTRL) vkey = 0xa3;
      else if (keysym.unicode > 32 && keysym.unicode < 128)
        // only TRUE ASCII! (Otherwise XBMC crashes! No unicode not even latin 1!)
        ascii = (char)(keysym.unicode & 0xff);
    }
  }

  // At this point update the key hold time
  // If XBMC_keysym was a class we could use == but memcmp it is :-(
  if (memcmp(&keysym, &m_lastKeysym, sizeof(XBMC_keysym)) == 0)
  {
    held = CTimeUtils::GetFrameTime() - m_lastKeyTime;
  }
  else
  {
    m_lastKeysym = keysym;
    m_lastKeyTime = CTimeUtils::GetFrameTime();
    held = 0;
  }

  // Create and return a CKey

  CKey key(vkey, unicode, ascii, modifiers, held);
    
  return key;
}

void CKeyboardStat::ProcessKeyUp(void)
{
  memset(&m_lastKeysym, 0, sizeof(m_lastKeysym));
  m_lastKeyTime = 0;
}
