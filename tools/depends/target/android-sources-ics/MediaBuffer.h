#pragma once
#include <media/stagefright/MediaBuffer.h>
#include <ui/GraphicBuffer.h>
#include <utils/StrongPointer.h>
namespace android
{
  class GraphicBuffer;
  class MetaData;
  class MediaBufferObserver;
}

class MediaBufferDummy : public android::MediaBuffer
{
public:
  MediaBufferDummy(size_t size) : android::MediaBuffer(size){};
};

extern "C"
{
  namespace android
  {
    MediaBufferDummy* create_mediabuffer(size_t size);
    void destroy_mediabuffer(MediaBufferDummy* mediabuffer);
    void mediabuffer_add_ref(MediaBufferDummy* mediabuffer);
    void mediabuffer_release(MediaBufferDummy* mediabuffer);
    void* mediabuffer_data(MediaBufferDummy* mediabuffer);
    size_t mediabuffer_size(MediaBufferDummy* mediabuffer);
    size_t mediabuffer_range_offset(MediaBufferDummy* mediabuffer);
    size_t mediabuffer_range_length(MediaBufferDummy* mediabuffer);
    void mediabuffer_set_range(MediaBufferDummy* mediabuffer, size_t offset, size_t length);
    sp<GraphicBuffer> mediabuffer_graphicBuffer(MediaBufferDummy* mediabuffer);
    sp<MetaData> mediabuffer_meta_data(MediaBufferDummy* mediabuffer);
    void mediabuffer_reset(MediaBufferDummy* mediabuffer);
    void mediabuffer_setObserver(MediaBufferDummy* mediabuffer, MediaBufferObserver *group);
    MediaBufferDummy* mediabuffer_clone(MediaBufferDummy* mediabuffer);
    int mediabuffer_refcount(MediaBufferDummy* mediabuffer);
  }
}

