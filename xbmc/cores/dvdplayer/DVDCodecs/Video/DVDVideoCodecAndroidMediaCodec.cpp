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

// http://developer.android.com/reference/android/media/MediaCodec.html
//
// Android MediaCodec class can be used to access low-level media codec,
// i.e. encoder/decoder components. (android.media.MediaCodec). Requires
// SDK16+ which is 4.1 Jellybean and above.
//

#include "DVDVideoCodecAndroidMediaCodec.h"
#include "DVDClock.h"
#include "threads/SystemClock.h"
#include "utils/BitstreamConverter.h"
#include "utils/fastmemcpy.h"
#include "utils/log.h"

#include "android/jni/Surface.h"
#include "android/jni/ByteBuffer.h"
#include "android/jni/MediaCodec.h"
#include "android/jni/MediaCrypto.h"
#include "android/jni/MediaFormat.h"
#include "android/jni/MediaCodecList.h"
#include "android/jni/MediaCodecInfo.h"
#include "android/activity/AndroidFeatures.h"

// Some Android devices ignore that we dequeued with
//  dequeueOutputBuffer and do not wait for releaseOutputBuffer
//  to overwrite the buffer contents. This shows up as tears like vsync.
// Do not use SINGLE_BUFFER_PICT on these devices.
//#define SINGLE_BUFFER_PICT 1

// vlc/modules/codec/omxil/utils.c
// Copyright (C) 2010 VLC authors and VideoLAN
// (LGPL v2.1)
static int IgnoreOmxDecoderPadding(const char *name)
{
    // The list of decoders that signal padding properly is not necessary,
    // since that is the default, but keep it here for reference. (This is
    // only relevant for manufacturers that are known to have decoders with
    // this kind of bug.)
    /*
    static const char *padding_decoders[] = {
      "OMX.SEC.AVC.Decoder",
      "OMX.SEC.wmv7.dec",
      "OMX.SEC.wmv8.dec",
      NULL
    };
    */
    static const char *nopadding_decoders[] = {
      "OMX.SEC.avc.dec",
      "OMX.SEC.avcdec",
      "OMX.SEC.MPEG4.Decoder",
      "OMX.SEC.mpeg4.dec",
      "OMX.SEC.vc1.dec",
      NULL
    };
    for (const char **ptr = nopadding_decoders; *ptr; ptr++)
    {
      if (!strcmp(*ptr, name))
        return 1;
    }
    return 0;
}

/*****************************************************************************/
/*****************************************************************************/
CDVDVideoCodecAndroidMediaCodec::CDVDVideoCodecAndroidMediaCodec()
{
  m_codec = NULL;
  m_formatname = "mediacodec";
  m_opened = false;
  m_bitstream = NULL;
  m_src_buffer_index = -1;
  memset(&m_videobuffer, 0x00, sizeof(DVDVideoPicture));
}

CDVDVideoCodecAndroidMediaCodec::~CDVDVideoCodecAndroidMediaCodec()
{
  Dispose();
}

bool CDVDVideoCodecAndroidMediaCodec::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  // check for 4.1 Jellybean and above.
  if (CAndroidFeatures::GetVersion() < 16)
    return false;

  m_hints = hints;
  m_drop  = false;
  m_zoom  = -1;
  m_speed = DVD_PLAYSPEED_NORMAL;

  switch(m_hints.codec)
  {
    case AV_CODEC_ID_MPEG2VIDEO:
      m_mime = "video/mpeg2";
      m_formatname = "amc-mpeg2";
      break;
    case AV_CODEC_ID_MPEG4:
      m_mime = "video/mp4v-es";
      m_formatname = "amc-mpeg4";
      break;
    case AV_CODEC_ID_H263:
      m_mime = "video/3gpp";
      m_formatname = "amc-h263";
      break;
    case AV_CODEC_ID_VP3:
    case AV_CODEC_ID_VP6:
    case AV_CODEC_ID_VP6F:
    case AV_CODEC_ID_VP8:
      //m_mime = "video/x-vp6";
      //m_mime = "video/x-vp7";
      m_mime = "video/x-vnd.on2.vp8";
      m_formatname = "amc-vpX";
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
    case AV_CODEC_ID_H264:
      m_mime = "video/avc";
      m_formatname = "amc-h264";
      m_bitstream = new CBitstreamConverter;
      if (!m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true))
      {
        delete m_bitstream, m_bitstream = NULL;
        return false;
      }
      break;
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
      m_mime = "video/wvc1";
      //m_mime = "video/wmv9";
      m_formatname = "amc-vc1";
      break;
    default:
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: Unknown hints.codec(%d)", hints.codec);
      return false;
      break;
  }

  // CJNIMediaCodec::createDecoderByXXX doesn't handle errors nicely,
  // it crashes if the codec isn't found. This is fixed in latest AOSP,
  // but not in current 4.1 devices. So 1st search for a matching codec, then create it.
  int num_codecs = CJNIMediaCodecList::getCodecCount();
  for (int i = 0; i < num_codecs; i++)
  {
    CJNIMediaCodecInfo codec_info = CJNIMediaCodecList::getCodecInfoAt(i);
    if (codec_info.isEncoder())
      continue;

    std::vector<std::string> types = codec_info.getSupportedTypes();
    // return the 1st one we find, that one is typically 'the best'
    for (size_t j = 0; j < types.size(); ++j)
    {
      if (types[j] == m_mime)
      {
        m_codecname = codec_info.getName();
        m_codec = new CJNIMediaCodec(CJNIMediaCodec::createByCodecName(m_codecname));
        // ugly cast because get_raw returns 'const jni::jhobject'
        ((jni::jhobject)m_codec->get_raw()).setGlobal();

        // clear any jni exceptions, jni gets upset if we do not.
        if (xbmc_jnienv()->ExceptionOccurred())
        {
          CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec::Open ExceptionOccurred");
          xbmc_jnienv()->ExceptionClear();
          m_codec = NULL;
          continue;
        }
        break;
      }
    }
    if (m_codec)
      break;
  }
  if (!m_codec)
  {
    CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec:: Failed to create Android MediaCodec");
    delete m_bitstream, m_bitstream = NULL;
    return false;
  }

  m_once = false;
  m_opened = ConfigureMediaCodec();
  if (!m_opened)
  {
    delete m_bitstream, m_bitstream = NULL;
    return false;
  }

  // setup a YUV420P DVDVideoPicture buffer.
  // first make sure all properties are reset.
  memset(&m_videobuffer, 0x00, sizeof(DVDVideoPicture));

  m_videobuffer.dts = DVD_NOPTS_VALUE;
  m_videobuffer.pts = DVD_NOPTS_VALUE;
  m_videobuffer.color_range  = 0;
  m_videobuffer.color_matrix = 4;
  m_videobuffer.iFlags  = DVP_FLAG_ALLOCATED;
  m_videobuffer.format  = RENDER_FMT_YUV420P;
  m_videobuffer.iWidth  = m_hints.width;
  m_videobuffer.iHeight = m_hints.height;
  m_videobuffer.iDisplayWidth  = m_hints.width;
  m_videobuffer.iDisplayHeight = m_hints.height;

  m_videobuffer.iLineSize[0] =  m_hints.width;         //Y
  m_videobuffer.iLineSize[1] = (m_hints.width + 1) /2; //U
  m_videobuffer.iLineSize[2] = (m_hints.width + 1) /2; //V
  m_videobuffer.iLineSize[3] = 0;

#if defined(SINGLE_BUFFER_PICT)
  // we fetch and pass pointers to the pict,
  // CDVDPlayerVideo::ProcessOverlays will handle
  // calling CDVDCodecUtils::CopyPicture. From
  // GetPicture until ClearPicture is called, we own the pointers.
  m_videobuffer.data[0] = NULL;
  m_videobuffer.data[1] = NULL;
  m_videobuffer.data[2] = NULL;
  m_videobuffer.data[3] = NULL;
#else
  unsigned int iPixels = m_hints.width * m_hints.height;
  unsigned int iChromaPixels = iPixels/4;

  m_videobuffer.data[0] = (uint8_t*)malloc(16 + iPixels);
  m_videobuffer.data[1] = (uint8_t*)malloc(16 + iChromaPixels);
  m_videobuffer.data[2] = (uint8_t*)malloc(16 + iChromaPixels);
  m_videobuffer.data[3] = NULL;

  memset(m_videobuffer.data[0], 0x00, iPixels);
  memset(m_videobuffer.data[1], 128,  iChromaPixels);
  memset(m_videobuffer.data[2], 128,  iChromaPixels);
#endif

  CLog::Log(LOGINFO, "CDVDVideoCodecAndroidMediaCodec:: Open Android MediaCodec %s", m_codecname.c_str());
  return true;
}

void CDVDVideoCodecAndroidMediaCodec::Dispose()
{
  CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::Dispose");

  m_opened = false;

  if (m_codec)
    m_codec->stop(), m_codec->release(), delete m_codec, m_codec = NULL;

  delete m_bitstream, m_bitstream = NULL;

  if (m_videobuffer.iFlags)
  {
#if defined(SINGLE_BUFFER_PICT)
    m_videobuffer.data[0] = NULL;
    m_videobuffer.data[1] = NULL;
    m_videobuffer.data[2] = NULL;
#else
    free(m_videobuffer.data[0]), m_videobuffer.data[0] = NULL;
    free(m_videobuffer.data[1]), m_videobuffer.data[1] = NULL;
    free(m_videobuffer.data[2]), m_videobuffer.data[2] = NULL;
#endif
    m_videobuffer.iFlags = 0;
  }
}

int CDVDVideoCodecAndroidMediaCodec::Decode(uint8_t *pData, int iSize, double dts, double pts)
{
  // Handle input, add demuxer packet to input queue, we must accept it or
  // it will be discarded as DVDPlayerVideo has no concept of "try again".
  // we must return VC_BUFFER or VC_PICTURE, default to VC_BUFFER.
  int rtn = VC_BUFFER;

  if (!m_opened)
    return rtn;

  if (m_hints.ptsinvalid)
    pts = DVD_NOPTS_VALUE;

  // must check for an output picture 1st,
  // otherwise, mediacodec can stall on some devices.
  if (GetOutputPicture() > 0)
    rtn |= VC_PICTURE;

  if (pData)
  {
    if (m_bitstream)
    {
      m_bitstream->Convert(pData, iSize);
      iSize = m_bitstream->GetConvertSize();
      pData = m_bitstream->GetConvertBuffer();
    }

    int loop_cnt = 0;
    // loop_cnt is an oh crap exit,
    // DVDPlayerVideo will not try again but we cannot stall here.
    while (loop_cnt < 25)
    {
      loop_cnt++;

      int64_t timeout_us = 1000;
      // try to fetch an input buffer
      int index = m_codec->dequeueInputBuffer(timeout_us);
      if (index < 0)
      {
        if (((rtn & VC_PICTURE) != VC_PICTURE) && GetOutputPicture() > 0)
          rtn |= VC_PICTURE;
        continue;
      }

      // docs lie, getInputBuffers should be good after
      // m_codec->start() but the internal refs are not
      // setup until much later.
      if (m_input.empty())
        m_input = m_codec->getInputBuffers();

      // we have an input buffer, fill it.
      int size = m_input[index].capacity();
      if (!m_once)
      {
        CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::Decode, ibuffercount(%d), size(%d), iSize(%d)", m_input.size(), size, iSize);
        m_once = true;
      }
      if (iSize > size)
      {
        CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec::Decode, iSize(%d) > size(%d)", iSize, size);
        iSize = size;
      }
      void *dst_ptr = xbmc_jnienv()->GetDirectBufferAddress(m_input[index].get_raw());
      fast_memcpy(dst_ptr, pData, iSize);

      // translate from dvdplayer dts/pts to MediaCodec pts,
      // pts WILL get re-ordered by MediaCodec if needed.
      int64_t presentationTimeUs = AV_NOPTS_VALUE;
      if (pts != DVD_NOPTS_VALUE)
        presentationTimeUs = pts;
      else if (dts != DVD_NOPTS_VALUE)
        presentationTimeUs = dts;
/*
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: "
        "pts(%f), ipts(%lld), iSize(%d), GetDataSize(%d), loop_cnt(%d)",
        presentationTimeUs, pts_dtoi(presentationTimeUs), iSize, GetDataSize(), loop_cnt);
*/
      int flags = 0;
      int offset = 0;
      // do not try to pass pts as a unioned double/int64_t,
      // some android devices will diddle with presentationTimeUs
      // and you will get NaN back.
      m_codec->queueInputBuffer(index, offset, iSize, presentationTimeUs, flags);

      // clear any jni exceptions, jni gets upset if we do not.
      if (xbmc_jnienv()->ExceptionOccurred())
      {
        CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec::Decode ExceptionOccurred");
        xbmc_jnienv()->ExceptionClear();
      }
      break;
    }
  }

  return rtn;
}

void CDVDVideoCodecAndroidMediaCodec::Reset()
{
  CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::Reset");

  if (!m_opened)
    return;

  if (m_codec)
  {
    m_codec->flush();
    m_videobuffer.pts = DVD_NOPTS_VALUE;

    if (xbmc_jnienv()->ExceptionOccurred())
    {
      CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec::Reset ExceptionOccurred");
      xbmc_jnienv()->ExceptionClear();
    }
  }
}

bool CDVDVideoCodecAndroidMediaCodec::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (!m_opened)
    return false;

  *pDvdVideoPicture = m_videobuffer;

  return true;
}

bool CDVDVideoCodecAndroidMediaCodec::ClearPicture(DVDVideoPicture* pDvdVideoPicture)
{
#if defined(SINGLE_BUFFER_PICT)
  if (m_src_buffer_index >= 0)
  {
    m_codec->releaseOutputBuffer(m_src_buffer_index, false);
    m_src_buffer_index = -1;
  }
#endif

  memset(pDvdVideoPicture, 0, sizeof(DVDVideoPicture));
  return true;
}

void CDVDVideoCodecAndroidMediaCodec::SetDropState(bool bDrop)
{
  m_drop = bDrop;
  if (m_drop)
    m_videobuffer.iFlags |=  DVP_FLAG_DROPPED;
  else
    m_videobuffer.iFlags &= ~DVP_FLAG_DROPPED;
}

void CDVDVideoCodecAndroidMediaCodec::SetSpeed(int iSpeed)
{
  //CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::SetSpeed, speed(%d)", iSpeed);
  // right now, we just track speed

  // update internal vars regardless if we are open or not.
  m_speed = iSpeed;

  if (!m_opened)
    return;

  switch(iSpeed)
  {
    case DVD_PLAYSPEED_PAUSE:
      break;
    case DVD_PLAYSPEED_NORMAL:
      break;
    default:
      break;
  }
}

int CDVDVideoCodecAndroidMediaCodec::GetDataSize(void)
{
  // mediacode cannot buffer much (5-10 frames),
  // just ignore internal buffering contribution.
  return 0;
}

double CDVDVideoCodecAndroidMediaCodec::GetTimeSize(void)
{
  // mediacode cannot buffer much (5-10 frames),
  // just ignore internal buffering contribution.
  return 0.0;
}

bool CDVDVideoCodecAndroidMediaCodec::ConfigureMediaCodec(void)
{
  // setup a MediaFormat to match the video content,
  // used by codec during configure
  CJNIMediaFormat mediaformat = CJNIMediaFormat::createVideoFormat(m_mime.c_str(), m_hints.width, m_hints.height);
  // some android devices forget to default the demux input max size
  mediaformat.setInteger(CJNIMediaFormat::KEY_MAX_INPUT_SIZE, 0);

  // handle codec extradata
  if (m_hints.extrasize)
  {
    size_t size = m_hints.extrasize;
    void  *src_ptr = m_hints.extradata;
    if (m_bitstream)
    {
      size = m_bitstream->GetExtraSize();
      src_ptr = m_bitstream->GetExtraData();
    }
    // Allocate a byte buffer via allocateDirect in java instead of NewDirectByteBuffer,
    // since the latter doesn't allocate storage of its own, and we don't know how long
    // the codec uses the buffer.
    CJNIByteBuffer bytebuffer = CJNIByteBuffer::allocateDirect(size);
    void *dts_ptr = xbmc_jnienv()->GetDirectBufferAddress(bytebuffer.get_raw());
    fast_memcpy(dts_ptr, src_ptr, size);
    // codec will automatically handle buffers as extradata
    // using entries with keys "csd-0", "csd-1", etc.
    mediaformat.setByteBuffer("csd-0", bytebuffer);
  }

  // configure and start the codec.
  // use the MediaFormat that we have setup.
  // use a null MediaCrypto, our content is not encrypted.
  // use a null Surface, we will extract the video picture data manually.
  int flags = 0;
  CJNISurface surface = jni::jhobject(NULL);
  CJNIMediaCrypto crypto = jni::jhobject(NULL);
  m_codec->configure(mediaformat, surface, crypto, flags);
  m_codec->start();

  // always, check/clear jni exceptions.
  if (xbmc_jnienv()->ExceptionOccurred())
    xbmc_jnienv()->ExceptionClear();

  return true;
}

int CDVDVideoCodecAndroidMediaCodec::GetOutputPicture(void)
{
  //CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::GetOutputPicture");
  int rtn = 0;

  int64_t timeout_us = 300;
  CJNIMediaCodecBufferInfo bufferInfo;
  int index = m_codec->dequeueOutputBuffer(bufferInfo, timeout_us);
  if (index >= 0)
  {
    if (m_drop)
    {
      m_codec->releaseOutputBuffer(index, false);

      // always, check/clear jni exceptions.
      if (xbmc_jnienv()->ExceptionOccurred())
        xbmc_jnienv()->ExceptionClear();
      return 0;
    }

    // some devices will return a valid index
    // before signaling INFO_OUTPUT_BUFFERS_CHANGED which
    // is used to setup m_output, D'uh. setup m_output here.
    if (m_output.empty())
      m_output = m_codec->getOutputBuffers();

    int size   = bufferInfo.size();
    int flags  = bufferInfo.flags();
    int offset = bufferInfo.offset();
    int64_t pts= bufferInfo.presentationTimeUs();

    if (flags & CJNIMediaCodec::BUFFER_FLAG_SYNC_FRAME)
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: BUFFER_FLAG_SYNC_FRAME");

    if (flags & CJNIMediaCodec::BUFFER_FLAG_CODEC_CONFIG)
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: BUFFER_FLAG_CODEC_CONFIG");

    if (flags & CJNIMediaCodec::BUFFER_FLAG_END_OF_STREAM)
    {
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: BUFFER_FLAG_END_OF_STREAM");
      m_codec->releaseOutputBuffer(index, false);
      // always, check/clear jni exceptions.
      if (xbmc_jnienv()->ExceptionOccurred())
        xbmc_jnienv()->ExceptionClear();
      return 0;
    }

    if (!m_output[index].isDirect())
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: m_output[index].isDirect == false");

    if (size && m_output[index].capacity())
    {
/*
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: "
        "pts(%f), GetDataSize(%d), GetTimeSize(%f)",
         pts,     GetDataSize(),   GetTimeSize());
*/
      uint8_t *src_ptr = (uint8_t*)xbmc_jnienv()->GetDirectBufferAddress(m_output[index].get_raw());
      src_ptr += offset;

      if (m_src_format == RENDER_FMT_YUV420P)
      {
        for (int i = 0; i < 3; i++)
        {
#if defined(SINGLE_BUFFER_PICT)
          m_videobuffer.data[i] = src_ptr + m_src_offset[i];
          m_videobuffer.iLineSize[i] = m_src_stride[i];
#else
          uint8_t *src   = src_ptr + m_src_offset[i];
          int src_stride = m_src_stride[i];
          uint8_t *dst   = m_videobuffer.data[i];
          int dst_stride = m_videobuffer.iLineSize[i];

          int height = m_videobuffer.iHeight;
          if (i > 0)
            height = (m_videobuffer.iHeight + 1) / 2;

          for (int j = 0; j < height; j++, src += src_stride, dst += dst_stride)
            fast_memcpy(dst, src, dst_stride);
#endif
        }
      }
      else if (m_src_format == RENDER_FMT_NV12)
      {
        for (int i = 0; i < 2; i++)
        {
#if defined(SINGLE_BUFFER_PICT)
          m_videobuffer.data[i] = src_ptr + m_src_offset[i];
          m_videobuffer.iLineSize[i] = m_src_stride[i];
#else
          uint8_t *src   = src_ptr + m_src_offset[i];
          int src_stride = m_src_stride[i];
          uint8_t *dst   = m_videobuffer.data[i];
          int dst_stride = m_videobuffer.iLineSize[i];

          int height = m_videobuffer.iHeight;

          for (int j = 0; j < height; j++, src += src_stride, dst += dst_stride)
            fast_memcpy(dst, src, dst_stride);
#endif
        }
      }
      m_videobuffer.format = m_src_format;
      m_videobuffer.dts = DVD_NOPTS_VALUE;
      m_videobuffer.pts = DVD_NOPTS_VALUE;
      if (pts != AV_NOPTS_VALUE)
        m_videobuffer.pts = pts;

      rtn = 1;
    }

#if defined(SINGLE_BUFFER_PICT)
    if (rtn == 1)
      m_src_buffer_index = index;
    else
#else
      m_codec->releaseOutputBuffer(index, false);
#endif

    // always, check/clear jni exceptions.
    if (xbmc_jnienv()->ExceptionOccurred())
      xbmc_jnienv()->ExceptionClear();
  }
  else if (index == CJNIMediaCodec::INFO_OUTPUT_BUFFERS_CHANGED)
  {
    CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: INFO_OUTPUT_BUFFERS_CHANGED");
    m_output = m_codec->getOutputBuffers();
    CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: OutputBuffer buffercount(%d)", m_output.size());
  }
  else if (index == CJNIMediaCodec::INFO_OUTPUT_FORMAT_CHANGED)
  {
    CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: INFO_OUTPUT_FORMAT_CHANGED");
    CJNIMediaFormat mediaformat = m_codec->getOutputFormat();

    int width       = mediaformat.getInteger("width");
    int height      = mediaformat.getInteger("height");
    int stride      = mediaformat.getInteger("stride");
    int slice_height= mediaformat.getInteger("slice-height");
    int color_format= mediaformat.getInteger("color-format");
    int crop_left   = mediaformat.getInteger("crop-left");
    int crop_top    = mediaformat.getInteger("crop-top");
    int crop_right  = mediaformat.getInteger("crop-right");
    int crop_bottom = mediaformat.getInteger("crop-bottom");

    CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: "
      "width(%d), height(%d), stride(%d), slice-height(%d), color-format(%d)",
      width, height, stride, slice_height, color_format);
    CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: "
      "crop-left(%d), crop-top(%d), crop-right(%d), crop-bottom(%d)",
      crop_left, crop_top, crop_right, crop_bottom);

    // Android device quirks and fixes
    if (stride <= 0)
        stride = width;
    if (slice_height <= 0)
    {
      slice_height = height;
      if (color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_FormatYUV420Planar)
      {
        // NVidia Tegra 3 on Nexus 7 does not set slice_heights
        if (strstr(m_codecname.c_str(), "OMX.Nvidia.") != NULL)
        {
          slice_height = ( ((height)+31) & ~31 );
          CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: NVidia Tegra 3 quirk, slice_height(%d)", slice_height);
        }
      }
    }
    if (color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_TI_FormatYUV420PackedSemiPlanar)
    {
      slice_height -= crop_top / 2;
      // set crop top/left here, since the offset parameter already includes this.
      // if we would ignore the offset parameter in the BufferInfo, we could just keep
      // the original slice height and apply the top/left cropping instead.
      crop_top = 0;
      crop_left = 0;
    }
    if (IgnoreOmxDecoderPadding(m_codecname.c_str()))
    {
      slice_height = 0;
      stride = crop_right  + 1 - crop_left;
    }

    // picture display width/height include the cropping.
    m_videobuffer.iDisplayWidth  = crop_right  + 1 - crop_left;
    m_videobuffer.iDisplayHeight = crop_bottom + 1 - crop_top;

    // default picture format to none
    for (int i = 0; i < 4; i++)
    {
      m_src_offset[i] = 0;
      m_src_stride[i] = 0;
    }
    m_src_format = RENDER_FMT_NONE;

    // setup picture format and data offset vectors
    if (color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_FormatYUV420Planar)
    {
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: COLOR_FormatYUV420Planar");
      m_src_format = RENDER_FMT_YUV420P;

      // Y plane
      m_src_stride[0] = stride;
      m_src_offset[0] = crop_top * stride;
      m_src_offset[0]+= crop_left;

      // U plane
      m_src_stride[1] = (stride + 1) / 2;
      //  skip over the Y plane
      m_src_offset[1] = slice_height * stride;
      //  crop_top/crop_left divided by two
      //  because one byte of the U/V planes
      //  corresponds to two pixels horizontally/vertically
      m_src_offset[1]+= crop_top  / 2 * m_src_stride[1];
      m_src_offset[1]+= crop_left / 2;

      // V plane
      m_src_stride[2] = (stride + 1) / 2;
      //  skip over the Y plane
      m_src_offset[2] = slice_height * stride;
      //  skip over the U plane
      m_src_offset[2]+= ((slice_height + 1) / 2) * ((stride + 1) / 2);
      //  crop_top/crop_left divided by two
      //  because one byte of the U/V planes
      //  corresponds to two pixels horizontally/vertically
      m_src_offset[2]+= crop_top  / 2 * m_src_stride[2];
      m_src_offset[2]+= crop_left / 2;
    }
    else if (color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_FormatYUV420SemiPlanar
          || color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_QCOM_FormatYUV420SemiPlanar
          || color_format == CJNIMediaCodecInfoCodecCapabilities::COLOR_TI_FormatYUV420PackedSemiPlanar)
    {
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec:: COLOR_FormatYUV420SemiPlanar");
      m_src_format = RENDER_FMT_NV12;

      // Y plane
      m_src_stride[0] = stride;
      m_src_offset[0] = crop_top * stride;
      m_src_offset[0]+= crop_left;

      // UV plane
      m_src_stride[1] = stride;
      //  skip over the Y plane
      m_src_offset[1] = slice_height * stride;
      m_src_offset[1]+= crop_top * stride;
      m_src_offset[1]+= crop_left;
    }
    else
    {
      CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec:: Fixme unknown color_format(%d)", color_format);
    }

    // clear any jni exceptions
    if (xbmc_jnienv()->ExceptionOccurred())
      xbmc_jnienv()->ExceptionClear();
  }
  else if (index == CJNIMediaCodec::INFO_TRY_AGAIN_LATER)
  {
    //CLog::Log(LOGDEBUG, "CDVDVideoCodecAndroidMediaCodec::GetOutputPicture INFO_TRY_AGAIN_LATER");
    // dequeueOutputBuffer timeout
    rtn = -1;
  }
  else
  {
    CLog::Log(LOGERROR, "CDVDVideoCodecAndroidMediaCodec::GetOutputPicture unknown index(%d)", index);
  }

  return rtn;
}
