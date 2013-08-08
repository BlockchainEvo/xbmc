#pragma once
#include <media/stagefright/OMXCodec.h>
extern "C"
{
  namespace android
  {
    sp<MediaSource> omxcodec_Create(const sp<IOMX> &omx,
            const sp<MetaData> &meta, bool createEncoder,
            const sp<MediaSource> &source,
            const char *matchComponentName = NULL,
            uint32_t flags = 0,
            const sp<ANativeWindow> &nativeWindow = NULL);
  }
}
