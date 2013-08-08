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
#include <stdint.h>
//#include <sys/types.h>
#include <OMXCodec.h>
#include "DllLibStagefrightOMXCodec.h"
#include "DllLibStagefrightMediaSource.h"
#include "DllLibStagefright.h"
android::sp<android::MediaSource> DllOMXCodec::Create(const android::sp<android::IOMX> &omx,
            const android::sp<android::MetaData> &meta, bool createEncoder,
            const android::sp<android::MediaSource> &source,
            const char *matchComponentName,
            uint32_t flags,
            const android::sp<ANativeWindow> &nativeWindow)
{
  DllLibStagefright dll;
  if (dll.Load())
    return dll.omxcodec_Create(omx, meta, createEncoder, source, matchComponentName, flags, nativeWindow);
  return NULL;
}
