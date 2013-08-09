#pragma once
/*
 *      Copyright (C) 2013 Team XBMC
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

#include <vector>

#include "DVDVideoCodec.h"
#include "DVDStreamInfo.h"
#include "threads/Thread.h"

class CJNIMediaCodec;
class CJNIByteBuffer;
class CBitstreamConverter;

class CDVDVideoCodecAndroidMediaCodec : public CDVDVideoCodec
{
public:
  CDVDVideoCodecAndroidMediaCodec();
  virtual ~CDVDVideoCodecAndroidMediaCodec();

  // Required overrides
  virtual bool    Open(CDVDStreamInfo &hints, CDVDCodecOptions &options);
  virtual void    Dispose();
  virtual int     Decode(uint8_t *pData, int iSize, double dts, double pts);
  virtual void    Reset();
  virtual bool    GetPicture(DVDVideoPicture *pDvdVideoPicture);
  virtual bool    ClearPicture(DVDVideoPicture* pDvdVideoPicture);
  virtual void    SetSpeed(int iSpeed);
  virtual void    SetDropState(bool bDrop);
  virtual int     GetDataSize(void);
  virtual double  GetTimeSize(void);
  virtual const char* GetName(void) { return m_formatname; }

protected:
  bool            ConfigureMediaCodec(void);
  int             GetOutputPicture(void);

  CDVDStreamInfo  m_hints;
  bool            m_once;
  bool            m_opened;
  CJNIMediaCodec *m_codec;
  std::vector<CJNIByteBuffer> m_input;
  std::vector<CJNIByteBuffer> m_output;
  std::string     m_mime;
  std::string     m_codecname;
  const char     *m_formatname;
  CBitstreamConverter *m_bitstream;
  DVDVideoPicture m_videobuffer;
  bool            m_drop;
  float           m_zoom;
  int             m_speed;

private:
  ERenderFormat   m_src_format;
  int             m_src_offset[4];
  int             m_src_stride[4];
  int             m_src_buffer_index;
};
