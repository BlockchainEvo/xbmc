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

#include "Texture.h"
#include "windowing/WindowingFactory.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/fastmemcpy.h"
#include "pictures/DllImageLib.h"
#include "DDSImage.h"
#include "filesystem/SpecialProtocol.h"
#include "jpeglib.h"
#include "filesystem/File.h"
#include "pictures/DllLibExif.h"
#include "settings/GUISettings.h"
#include "settings/Settings.h"
#include <setjmp.h>
#if defined(__APPLE__) && defined(__arm__)
#include <ImageIO/ImageIO.h>
#include "filesystem/File.h"
#include "osx/DarwinUtils.h"
#endif

/************************************************************************/
/*                                                                      */
/************************************************************************/
CBaseTexture::CBaseTexture(unsigned int width, unsigned int height, unsigned int format)
 : m_hasAlpha( true )
{
#ifndef HAS_DX 
  m_texture = 0; 
#endif
  m_pixels = NULL;
  m_loadedToGPU = false;
  Allocate(width, height, format);
}

CBaseTexture::~CBaseTexture()
{
  delete[] m_pixels;
}

void CBaseTexture::Allocate(unsigned int width, unsigned int height, unsigned int format)
{
  m_imageWidth = width;
  m_imageHeight = height;
  m_format = format;
  m_orientation = 0;

  m_textureWidth = m_imageWidth;
  m_textureHeight = m_imageHeight;

  if (m_format & XB_FMT_DXT_MASK)
    while (GetPitch() < g_Windowing.GetMinDXTPitch())
      m_textureWidth += GetBlockSize();

  if (!g_Windowing.SupportsNPOT((m_format & XB_FMT_DXT_MASK) != 0))
  {
    m_textureWidth = PadPow2(m_textureWidth);
    m_textureHeight = PadPow2(m_textureHeight);
  }
  if (m_format & XB_FMT_DXT_MASK)
  { // DXT textures must be a multiple of 4 in width and height
    m_textureWidth = ((m_textureWidth + 3) / 4) * 4;
    m_textureHeight = ((m_textureHeight + 3) / 4) * 4;
  }

  // check for max texture size
  #define CLAMP(x, y) { if (x > y) x = y; }
  CLAMP(m_textureWidth, g_Windowing.GetMaxTextureSize());
  CLAMP(m_textureHeight, g_Windowing.GetMaxTextureSize());
  CLAMP(m_imageWidth, m_textureWidth);
  CLAMP(m_imageHeight, m_textureHeight);

  delete[] m_pixels;
  m_pixels = new unsigned char[GetPitch() * GetRows()];
}

void CBaseTexture::Update(unsigned int width, unsigned int height, unsigned int pitch, unsigned int format, const unsigned char *pixels, bool loadToGPU)
{
  if (pixels == NULL)
    return;

  if (format & XB_FMT_DXT_MASK && !g_Windowing.SupportsDXT())
  { // compressed format that we don't support
    Allocate(width, height, XB_FMT_A8R8G8B8);
    CDDSImage::Decompress(m_pixels, std::min(width, m_textureWidth), std::min(height, m_textureHeight), GetPitch(m_textureWidth), pixels, format);
  }
  else
  {
    Allocate(width, height, format);

    unsigned int srcPitch = pitch ? pitch : GetPitch(width);
    unsigned int srcRows = GetRows(height);
    unsigned int dstPitch = GetPitch(m_textureWidth);
    unsigned int dstRows = GetRows(m_textureHeight);

    if (srcPitch == dstPitch)
      fast_memcpy(m_pixels, pixels, srcPitch * std::min(srcRows, dstRows));
    else
    {
      const unsigned char *src = pixels;
      unsigned char* dst = m_pixels;
      for (unsigned int y = 0; y < srcRows && y < dstRows; y++)
      {
        fast_memcpy(dst, src, std::min(srcPitch, dstPitch));
        src += srcPitch;
        dst += dstPitch;
      }
    }
  }
  ClampToEdge();

  if (loadToGPU)
    LoadToGPU();
}

void CBaseTexture::ClampToEdge()
{
  unsigned int imagePitch = GetPitch(m_imageWidth);
  unsigned int imageRows = GetRows(m_imageHeight);
  unsigned int texturePitch = GetPitch(m_textureWidth);
  unsigned int textureRows = GetRows(m_textureHeight);
  if (imagePitch < texturePitch)
  {
    unsigned int blockSize = GetBlockSize();
    unsigned char *src = m_pixels + imagePitch - blockSize;
    unsigned char *dst = m_pixels;
    for (unsigned int y = 0; y < imageRows; y++)
    {
      for (unsigned int x = imagePitch; x < texturePitch; x += blockSize)
        fast_memcpy(dst + x, src, blockSize);
      dst += texturePitch;
    }
  }

  if (imageRows < textureRows)
  {
    unsigned char *dst = m_pixels + imageRows * texturePitch;
    for (unsigned int y = imageRows; y < textureRows; y++)
    {
      fast_memcpy(dst, dst - texturePitch, texturePitch);
      dst += texturePitch;
    }
  }
}

bool CBaseTexture::LoadFromFile(const CStdString& texturePath, unsigned int maxWidth, unsigned int maxHeight,
                                bool autoRotate, unsigned int *originalWidth, unsigned int *originalHeight)
{
  if (URIUtils::GetExtension(texturePath).Equals(".dds"))
  { // special case for DDS images
    CDDSImage image;
    if (image.ReadFile(texturePath))
    {
      Update(image.GetWidth(), image.GetHeight(), 0, image.GetFormat(), image.GetData(), false);
      return true;
    }
    return false;
  }

#if defined(__APPLE__) && defined(__arm__)
  XFILE::CFile file;
  UInt8 *imageBuff      = NULL;
  int64_t imageBuffSize = 0;

  //open path and read data to buffer
  //this handles advancedsettings.xml pathsubstitution
  //and resulting networking
  if (file.Open(texturePath, 0))
  {
    imageBuffSize =file.GetLength();
    imageBuff = new UInt8[imageBuffSize];
    imageBuffSize = file.Read(imageBuff, imageBuffSize);
    file.Close();
  }
  else
  {
    CLog::Log(LOGERROR, "Texture manager unable to open file %s", texturePath.c_str());
    return false;
  }

  if (imageBuffSize <= 0)
  {
    CLog::Log(LOGERROR, "Texture manager read texture file failed.");
    delete [] imageBuff;
    return false;
  }

  // create the image from buffer;
  CGImageSourceRef imageSource;
  // create a CFDataRef using CFDataCreateWithBytesNoCopy and kCFAllocatorNull for deallocator.
  // this allows us to do a nocopy reference and we handle the free of imageBuff
  CFDataRef cfdata = CFDataCreateWithBytesNoCopy(NULL, imageBuff, imageBuffSize, kCFAllocatorNull);
  imageSource = CGImageSourceCreateWithData(cfdata, NULL);   
    
  if (imageSource == nil)
  {
    CLog::Log(LOGERROR, "Texture manager unable to load file: %s", CSpecialProtocol::TranslatePath(texturePath).c_str());
    CFRelease(cfdata);
    delete [] imageBuff;
    return false;
  }

  CGImageRef image = CGImageSourceCreateImageAtIndex(imageSource, 0, NULL);

  int rotate = 0;
  if (autoRotate)
  { // get the orientation of the image for displaying it correctly
    CFDictionaryRef imagePropertiesDictionary = CGImageSourceCopyPropertiesAtIndex(imageSource,0, NULL);
    if (imagePropertiesDictionary != nil)
    {
      CFNumberRef orientation = (CFNumberRef)CFDictionaryGetValue(imagePropertiesDictionary, kCGImagePropertyOrientation);
      if (orientation != nil)
      {
        int value = 0;
        CFNumberGetValue(orientation, kCFNumberIntType, &value);
        if (value)
          rotate = value - 1;
      }
      CFRelease(imagePropertiesDictionary);
    }
  }

  CFRelease(imageSource);

  unsigned int width  = CGImageGetWidth(image);
  unsigned int height = CGImageGetHeight(image);

  m_hasAlpha = (CGImageGetAlphaInfo(image) != kCGImageAlphaNone);

  if (originalWidth)
    *originalWidth = width;
  if (originalHeight)
    *originalHeight = height;

  // check texture size limits and limit to screen size - preserving aspectratio of image  
  if ( width > g_Windowing.GetMaxTextureSize() || height > g_Windowing.GetMaxTextureSize() )
  {
    float aspect;

    if ( width > height )
    {
      aspect = (float)width / (float)height;
      width  = g_Windowing.GetWidth();
      height = (float)width / (float)aspect;
    }
    else
    {
      aspect = (float)height / (float)width;
      height = g_Windowing.GetHeight();
      width  = (float)height / (float)aspect;
    }
    CLog::Log(LOGDEBUG, "Texture manager texture clamp:new texture size: %i x %i", width, height);
  }

  // use RGBA to skip swizzling
  Allocate(width, height, XB_FMT_RGBA8);
  m_orientation = rotate;
    
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

  // hw convert jmpeg to RGBA
  CGContextRef context = CGBitmapContextCreate(m_pixels,
    width, height, 8, GetPitch(), colorSpace,
    kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

  CGColorSpaceRelease(colorSpace);

  // Flip so that it isn't upside-down
  //CGContextTranslateCTM(context, 0, height);
  //CGContextScaleCTM(context, 1.0f, -1.0f);
  #if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_5
    CGContextClearRect(context, CGRectMake(0, 0, width, height));
  #else
    #if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
    // (just a way of checking whether we're running in 10.5 or later)
    if (CGContextDrawLinearGradient == 0)
      CGContextClearRect(context, CGRectMake(0, 0, width, height));
    else
    #endif
      CGContextSetBlendMode(context, kCGBlendModeCopy);
  #endif
  //CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
  CGContextRelease(context);
  CGImageRelease(image);
  CFRelease(cfdata);
  delete [] imageBuff;
#else

  if (URIUtils::GetExtension(texturePath).Equals(".jpg") || URIUtils::GetExtension(texturePath).Equals(".tbn"))
  {
    if (DecodeJPEG(texturePath, autoRotate, 0, 0))
      return true;
  }

  DllImageLib dll;
  if (!dll.Load())
    return false;

  ImageInfo image;
  memset(&image, 0, sizeof(image));

  unsigned int width = maxWidth ? std::min(maxWidth, g_Windowing.GetMaxTextureSize()) : g_Windowing.GetMaxTextureSize();
  unsigned int height = maxHeight ? std::min(maxHeight, g_Windowing.GetMaxTextureSize()) : g_Windowing.GetMaxTextureSize();

  if(!dll.LoadImage(texturePath.c_str(), width, height, &image))
  {
    CLog::Log(LOGERROR, "Texture manager unable to load file: %s", texturePath.c_str());
    return false;
  }

  m_hasAlpha = NULL != image.alpha;

  Allocate(image.width, image.height, XB_FMT_A8R8G8B8);
  if (autoRotate && image.exifInfo.Orientation)
    m_orientation = image.exifInfo.Orientation - 1;
  if (originalWidth)
    *originalWidth = image.originalwidth;
  if (originalHeight)
    *originalHeight = image.originalheight;

  unsigned int dstPitch = GetPitch();
  unsigned int srcPitch = ((image.width + 1)* 3 / 4) * 4; // bitmap row length is aligned to 4 bytes

  unsigned char *dst = m_pixels;
  unsigned char *src = image.texture + (m_imageHeight - 1) * srcPitch;

  for (unsigned int y = 0; y < m_imageHeight; y++)
  {
    unsigned char *dst2 = dst;
    unsigned char *src2 = src;
    for (unsigned int x = 0; x < m_imageWidth; x++, dst2 += 4, src2 += 3)
    {
      dst2[0] = src2[0];
      dst2[1] = src2[1];
      dst2[2] = src2[2];
      dst2[3] = 0xff;
    }
    src -= srcPitch;
    dst += dstPitch;
  }

  if(image.alpha)
  {
    dst = m_pixels + 3;
    src = image.alpha + (m_imageHeight - 1) * m_imageWidth;

    for (unsigned int y = 0; y < m_imageHeight; y++)
    {
      unsigned char *dst2 = dst;
      unsigned char *src2 = src;

      for (unsigned int x = 0; x < m_imageWidth; x++,  dst2+=4, src2++)
        *dst2 = *src2;
      src -= m_imageWidth;
      dst += dstPitch;
    }
  }
  dll.ReleaseImage(&image);
#endif

  ClampToEdge();

  return true;
}

bool CBaseTexture::LoadFromMemory(unsigned int width, unsigned int height, unsigned int pitch, unsigned int format, bool hasAlpha, unsigned char* pixels)
{
  m_imageWidth = width;
  m_imageHeight = height;
  m_format = format;
  m_hasAlpha = hasAlpha;
  Update(width, height, pitch, format, pixels, false);
  return true;
}

bool CBaseTexture::LoadPaletted(unsigned int width, unsigned int height, unsigned int pitch, unsigned int format, const unsigned char *pixels, const COLOR *palette)
{
  if (pixels == NULL || palette == NULL)
    return false;

  Allocate(width, height, format);

  for (unsigned int y = 0; y < m_imageHeight; y++)
  {
    unsigned char *dest = m_pixels + y * GetPitch();
    const unsigned char *src = pixels + y * pitch;
    for (unsigned int x = 0; x < m_imageWidth; x++)
    {
      COLOR col = palette[*src++];
      *dest++ = col.b;
      *dest++ = col.g;
      *dest++ = col.r;
      *dest++ = col.x;
    }
  }
  ClampToEdge();
  return true;
}

unsigned int CBaseTexture::PadPow2(unsigned int x)
{
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return ++x;
}

bool CBaseTexture::SwapBlueRed(unsigned char *pixels, unsigned int height, unsigned int pitch, unsigned int elements, unsigned int offset)
{
  if (!pixels) return false;
  unsigned char *dst = pixels;
  for (unsigned int y = 0; y < height; y++)
  {
    dst = pixels + (y * pitch);
    for (unsigned int x = 0; x < pitch; x+=elements)
      std::swap(dst[x+offset], dst[x+2+offset]);
  }
  return true;
}

unsigned int CBaseTexture::GetPitch(unsigned int width) const
{
  switch (m_format)
  {
  case XB_FMT_DXT1:
    return ((width + 3) / 4) * 8;
  case XB_FMT_DXT3:
  case XB_FMT_DXT5:
  case XB_FMT_DXT5_YCoCg:
    return ((width + 3) / 4) * 16;
  case XB_FMT_A8:
    return width;
  case XB_FMT_RGB8:
    return (((width + 1)* 3 / 4) * 4);
    break;
  case XB_FMT_RGBA8:
  case XB_FMT_A8R8G8B8:
  default:
    return width*4;
  }
}

unsigned int CBaseTexture::GetRows(unsigned int height) const
{
  switch (m_format)
  {
  case XB_FMT_DXT1:
    return (height + 3) / 4;
  case XB_FMT_DXT3:
  case XB_FMT_DXT5:
  case XB_FMT_DXT5_YCoCg:
    return (height + 3) / 4;
  default:
    return height;
  }
}

unsigned int CBaseTexture::GetBlockSize() const
{
  switch (m_format)
  {
  case XB_FMT_DXT1:
    return 8;
  case XB_FMT_DXT3:
  case XB_FMT_DXT5:
  case XB_FMT_DXT5_YCoCg:
    return 16;
  case XB_FMT_A8:
    return 1;
  default:
    return 4;
  }
}

bool CBaseTexture::HasAlpha() const
{
  return m_hasAlpha;
}


struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr) cinfo->err;
  longjmp(myerr->setjmp_buffer, 1);
}

bool CBaseTexture::DecodeJPEG(const CStdString& texturePath, bool autoRotate, int minx=0, int miny=0)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  JSAMPROW row_pointer[1];
  XFILE::CFile file;
  uint8_t *imageBuff = NULL;
  int64_t imageBuffSize = 0;

  if (file.Open(texturePath.c_str(), 0))
  {
    int64_t imgsize;
    imgsize = file.GetLength();
    imageBuff = new uint8_t[imgsize];
    imageBuffSize = file.Read(imageBuff, imgsize);
    file.Close();

    if ((imgsize != imageBuffSize) || (imageBuffSize <= 0))
    {
      delete [] imageBuff;
      return false;
    }
  }
  else
  {
    return false;
  }

  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  if (setjmp(jerr.setjmp_buffer))
  {
    jpeg_destroy_decompress(&cinfo);
    delete [] imageBuff;
    return false;
  }

  jpeg_create_decompress( &cinfo );
  jpeg_mem_src(&cinfo, imageBuff, imageBuffSize);
  jpeg_read_header( &cinfo, TRUE );

/*  jpegs don't have alpha so we only need rgb. we can use an rbg texture
  directly. It won't be 32bit aligned, but we make up for it by not having
  to set dummy alpha values.*/
  cinfo.out_color_space=JCS_RGB;
  m_hasAlpha = false;

/*  libjpeg can scale the image for us if it is too big. It must be in the format
  num/denom, where (for our purposes) that is [1-8]/8.
  The only way to know how big a resulting image will be is to try a ratio and
  test its resulting size.
  If the res is greater than the one desired, use that one since there's no need
  to decode a bigger one just to squish it back down. If the res is greater than
  the gpu can hold, use the previous one.*/
  if (minx <= 0 || miny <= 0)
  {
    minx = g_settings.m_ResInfo[g_guiSettings.m_LookAndFeelResolution].iScreenWidth;
    miny = g_settings.m_ResInfo[g_guiSettings.m_LookAndFeelResolution].iScreenHeight;
  }
  cinfo.scale_denom=8;
  unsigned int maxtexsize = g_Windowing.GetMaxTextureSize();
  for (cinfo.scale_num = 1;cinfo.scale_num <=8 ;cinfo.scale_num++)
  {
    jpeg_calc_output_dimensions (&cinfo);
    if ((cinfo.output_width * cinfo.output_height) > (maxtexsize * maxtexsize))
    {
      cinfo.scale_num--;
      break;
    }
    if ( cinfo.output_width >= minx && cinfo.output_height >= miny)
      break;
  }
  jpeg_calc_output_dimensions (&cinfo);

  unsigned int srcPitch = (((cinfo.output_width + 1)* 3 / 4) * 4); // bitmap row length is aligned to 4 bytes
  Allocate(cinfo.output_width, cinfo.output_height, XB_FMT_RGB8);
  unsigned int dstPitch = GetPitch();
  if (srcPitch > dstPitch)
     return false;

  unsigned char *dst = m_pixels;
  jpeg_start_decompress( &cinfo );
  while( cinfo.output_scanline < cinfo.output_height )
    {
      jpeg_read_scanlines( &cinfo, &dst, 1 );
      dst+=dstPitch;
    }

  jpeg_finish_decompress( &cinfo );
  jpeg_destroy_decompress( &cinfo );
  delete [] imageBuff;

  //Now that we've decoded the image, we need to check the EXIF tag for possible rotation
  ExifInfo_t m_exifInfo;
  IPTCInfo_t m_iptcInfo;
  DllLibExif exifDll;
  if (!exifDll.Load())
    {
      m_orientation=0;
      return true;
    }
  exifDll.process_jpeg(texturePath.c_str(), &m_exifInfo, &m_iptcInfo);
  if (autoRotate && m_exifInfo.Orientation)
    m_orientation = m_exifInfo.Orientation - 1;


return true;
}
