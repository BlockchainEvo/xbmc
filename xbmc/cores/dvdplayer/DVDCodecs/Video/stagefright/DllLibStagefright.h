#pragma once

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

#if (defined HAVE_CONFIG_H) && (!defined TARGET_WINDOWS)
  #include "config.h"
#endif
#define DLL_PATH_LIBSTAGEFRIGHT "foo"
#include "DynamicDll.h"
#include "utils/log.h"
#include <utils/StrongPointer.h>

//#include <media/stagefright/MetaData.h>
#include <stagefright/MetaData.h>
#include <stagefright/GraphicBuffer.h>
#include <stagefright/OMXClient.h>
#include <stagefright/ProcessState.h>
#include <stagefright/MediaSource.h>

class XBMCMediaSource;
class MediaSourceFunctions;

namespace android
{
  class MediaBuffer;
  class MetaData;
  class MediaBufferObserver;
  class OMXClient;
  class ProcessState;
  class MediaSource;
}

class DllLibStagefrightMediaSourceInterface
{
public:
  virtual ~DllLibStagefrightMediaSourceInterface(){};
  virtual XBMCMediaSource* create_mediasource(MediaSourceFunctions *functions)=0;
  virtual void destroy_mediasource(XBMCMediaSource *mediasource)=0;
};

class DllLibStagefrightReadOptionsInterface
{
public:
    virtual android::MediaSource::ReadOptions* create_readoptions();
    virtual void destroy_readoptions(android::MediaSource::ReadOptions* options);
    virtual void mediasource_readoptions_setSeekTo(android::MediaSource::ReadOptions* options, int64_t time_us, android::MediaSource::ReadOptions::SeekMode mode);
    virtual void mediasource_readoptions_clearSeekTo(android::MediaSource::ReadOptions* options);
    virtual bool mediasource_readoptions_getSeekTo(android::MediaSource::ReadOptions* options, int64_t *time_us, android::MediaSource::ReadOptions::SeekMode *mode);
    virtual void mediasource_readoptions_setLateBy(android::MediaSource::ReadOptions* options, int64_t lateness_us);
    virtual int64_t mediasource_readoptions_getLateBy(android::MediaSource::ReadOptions* options);
};

class DllLibStagefrightMediaBufferInterface
{
public:
  virtual ~DllLibStagefrightMediaBufferInterface() {}
  virtual android::MediaBuffer* create_mediabuffer(size_t size)=0;
  virtual void destroy_mediabuffer(android::MediaBuffer* buffer)=0;
  virtual void mediabuffer_release (android::MediaBuffer* buffer)=0;
  virtual void mediabuffer_add_ref(android::MediaBuffer* buffer)=0;
  virtual void *mediabuffer_data(android::MediaBuffer* buffer)=0;
  virtual size_t mediabuffer_size(android::MediaBuffer* buffer)=0;
  virtual size_t mediabuffer_range_offset(android::MediaBuffer* buffer)=0;
  virtual size_t mediabuffer_range_length(android::MediaBuffer* buffer)=0;
  virtual void mediabuffer_set_range(android::MediaBuffer* buffer, size_t offset, size_t length)=0;
  virtual android::sp<android::GraphicBuffer> mediabuffer_graphicBuffer(android::MediaBuffer* buffer)=0;
  virtual android::sp<android::MetaData> mediabuffer_meta_data(android::MediaBuffer* buffer)=0;
  virtual void mediabuffer_reset(android::MediaBuffer* buffer)=0;
  virtual void mediabuffer_setObserver(android::MediaBuffer* buffer, android::MediaBufferObserver *group)=0;
  virtual android::MediaBuffer *mediabuffer_clone(android::MediaBuffer* buffer)=0;
  virtual int mediabuffer_refcount(android::MediaBuffer* buffer)=0;
};

class DllLibStagefrightMetaDataInterface
{
public:
    virtual ~DllLibStagefrightMetaDataInterface() {};
    virtual android::MetaData* create_metadata()=0;
    virtual void destroy_metadata(android::MetaData* metadata)=0;
    virtual void metadata_clear()=0;
    virtual bool metadata_remove(uint32_t key)=0;
    virtual bool metadata_setCString(uint32_t key, const char *value)=0;
    virtual bool metadata_setInt32(uint32_t key, int32_t value)=0;
    virtual bool metadata_setInt64(uint32_t key, int64_t value)=0;
    virtual bool metadata_setFloat(uint32_t key, float value)=0;
    virtual bool metadata_setPointer(uint32_t key, void *value)=0;
    virtual bool metadata_setRect(uint32_t key,int32_t left, int32_t top,int32_t right, int32_t bottom)=0;
    virtual bool metadata_findCString(uint32_t key, const char **value)=0;
    virtual bool metadata_findInt32(uint32_t key, int32_t *value)=0;
    virtual bool metadata_findInt64(uint32_t key, int64_t *value)=0;
    virtual bool metadata_findFloat(uint32_t key, float *value)=0;
    virtual bool metadata_findPointer(uint32_t key, void **value)=0;
    virtual bool metadata_findRect(uint32_t key,int32_t *left, int32_t *top,int32_t *right, int32_t *bottom)=0;
    virtual bool metadata_setData(uint32_t key, uint32_t type, const void *data, size_t size)=0;
    virtual bool metadata_findData(uint32_t key, uint32_t *type,const void **data, size_t *size)=0;
};

class DllLibStagefrightOMXClientInterface
{
public:
    virtual android::OMXClient* create_omxclient()=0;
    virtual void destroy_omxclient(android::OMXClient*)=0;
    virtual android::status_t omxclient_connect(android::OMXClient*)=0;
    virtual void omxclient_disconnect(android::OMXClient*)=0;
    virtual android::sp<android::IOMX> omxclient_interface(android::OMXClient*)=0;
};

class DllLibStagefrightOMXCodecInterface
{
public:
    virtual android::sp<android::MediaSource> omxcodec_Create(const android::sp<android::IOMX> &omx,
            const android::sp<android::MetaData> &meta, bool createEncoder,
            const android::sp<android::MediaSource> &source,
            const char *matchComponentName = NULL,
            uint32_t flags = 0,
            const android::sp<ANativeWindow> &nativeWindow = NULL);
};
class DllLibStagefrightProcessStateInterface
{
public:
    virtual android::sp<android::ProcessState> processstate_self()=0;
    virtual void processstate_startThreadPool(android::sp<android::ProcessState> state)=0;
};

class DllLibStagefrightGraphicBufferInterface
{
public:
    virtual ANativeWindowBuffer* graphicbuffer_getNativeBuffer(android::GraphicBuffer *buffer)=0;
};

class DllLibStagefright : public DllDynamic, DllLibStagefrightMediaBufferInterface
{
public:
  DECLARE_DLL_WRAPPER(DllLibStagefright, DLL_PATH_LIBSTAGEFRIGHT)
  DEFINE_METHOD1(android::MediaBuffer*, create_mediabuffer, (size_t p1))
  DEFINE_METHOD1(void, destroy_mediabuffer, (android::MediaBuffer* p1))
  DEFINE_METHOD1(void, mediabuffer_release, (android::MediaBuffer* p1))
  DEFINE_METHOD1(void, mediabuffer_add_ref, (android::MediaBuffer* p1))
  DEFINE_METHOD1(void*, mediabuffer_data, (android::MediaBuffer* p1))
  DEFINE_METHOD1(size_t, mediabuffer_size, (android::MediaBuffer* p1))
  DEFINE_METHOD1(size_t, mediabuffer_range_offset, (android::MediaBuffer* p1))
  DEFINE_METHOD1(size_t, mediabuffer_range_length, (android::MediaBuffer* p1))
  DEFINE_METHOD3(void, mediabuffer_set_range, (android::MediaBuffer* p1, size_t p2, size_t p3))
  DEFINE_METHOD1(android::sp<android::GraphicBuffer>, mediabuffer_graphicBuffer, (android::MediaBuffer* p1))
  DEFINE_METHOD1(android::sp<android::MetaData>, mediabuffer_meta_data, (android::MediaBuffer* p1))
  DEFINE_METHOD1(void, mediabuffer_reset, (android::MediaBuffer* p1))
  DEFINE_METHOD2(void, mediabuffer_setObserver, (android::MediaBuffer* p1, android::MediaBufferObserver* p2))
  DEFINE_METHOD1(android::MediaBuffer*, mediabuffer_clone, (android::MediaBuffer* p1))
  DEFINE_METHOD1(int, mediabuffer_refcount, (android::MediaBuffer* p1))

  DEFINE_METHOD0(android::MetaData*, create_metadata)
  DEFINE_METHOD1(void, destroy_metadata,     (android::MetaData* p1))
  DEFINE_METHOD1(void, metadata_clear,       (android::sp<android::MetaData> p1))
  DEFINE_METHOD2(bool, metadata_remove,      (android::sp<android::MetaData> p1, uint32_t p2))
  DEFINE_METHOD3(bool, metadata_setCString,  (android::sp<android::MetaData> p1, uint32_t p2, const char* p3))
  DEFINE_METHOD3(bool, metadata_setInt32,    (android::sp<android::MetaData> p1, uint32_t p2, int32_t p3))
  DEFINE_METHOD3(bool, metadata_setInt64,    (android::sp<android::MetaData> p1, uint32_t p2, int64_t p3))
  DEFINE_METHOD3(bool, metadata_setFloat,    (android::sp<android::MetaData> p1, uint32_t p2, float p3))
  DEFINE_METHOD3(bool, metadata_setPointer,  (android::sp<android::MetaData> p1, uint32_t p2, void* p3))
  DEFINE_METHOD6(bool, metadata_setRect,     (android::sp<android::MetaData> p1, uint32_t p2, uint32_t p3, int32_t p4, int32_t p5, int32_t p6))
  DEFINE_METHOD3(bool, metadata_findCString, (android::sp<android::MetaData> p1, uint32_t p2, const char** p3))
  DEFINE_METHOD3(bool, metadata_findInt32,   (android::sp<android::MetaData> p1, uint32_t p2, int32_t *p3))
  DEFINE_METHOD3(bool, metadata_findInt64,   (android::sp<android::MetaData> p1, uint32_t p2, int64_t *p3))
  DEFINE_METHOD3(bool, metadata_findFloat,   (android::sp<android::MetaData> p1, uint32_t p2, float *p3))
  DEFINE_METHOD3(bool, metadata_findPointer, (android::sp<android::MetaData> p1, uint32_t p2, void** p3))
  DEFINE_METHOD6(bool, metadata_findRect,    (android::sp<android::MetaData> p1, uint32_t p2, int32_t *p3, int32_t *p4, int32_t *p5, int32_t *p6))
  DEFINE_METHOD5(bool, metadata_setData,     (android::sp<android::MetaData> p1, uint32_t p2, uint32_t p3, const void *p4, size_t p5))
  DEFINE_METHOD5(bool, metadata_findData,    (android::sp<android::MetaData> p1, uint32_t p2, uint32_t* p3, const void **p4, size_t *p5))

  DEFINE_METHOD1(XBMCMediaSource*, create_mediasource, (MediaSourceFunctions* p1))
  DEFINE_METHOD1(void, destroy_mediasource, (XBMCMediaSource* p1))

  DEFINE_METHOD0(android::MediaSource::ReadOptions*, create_readoptions)
  DEFINE_METHOD1(void, destroy_readoptions, (android::MediaSource::ReadOptions* p1))
  DEFINE_METHOD3(void, mediasource_readoptions_setSeekTo, (android::MediaSource::ReadOptions* p1, int64_t p2, android::MediaSource::ReadOptions::SeekMode p3))
  DEFINE_METHOD1(void, mediasource_readoptions_clearSeekTo, (android::MediaSource::ReadOptions* p1))
  DEFINE_METHOD3(bool, mediasource_readoptions_getSeekTo, (android::MediaSource::ReadOptions* p1, int64_t *p2, android::MediaSource::ReadOptions::SeekMode *p3))
  DEFINE_METHOD2(void, mediasource_readoptions_setLateBy, (android::MediaSource::ReadOptions* p1, int64_t p2))
  DEFINE_METHOD1(int64_t, mediasource_readoptions_getLateBy, (android::MediaSource::ReadOptions* p1))

  DEFINE_METHOD0(android::OMXClient*, create_omxclient)
  DEFINE_METHOD1(void, destroy_omxclient, (android::OMXClient* p1))
  DEFINE_METHOD1(android::status_t, omxclient_connect, (android::OMXClient* p1))
  DEFINE_METHOD1(void, omxclient_disconnect, (android::OMXClient* p1))
  DEFINE_METHOD1(android::sp<android::IOMX>, omxclient_interface, (android::OMXClient* p1))

  DEFINE_METHOD7(android::sp<android::MediaSource>, omxcodec_Create, (const android::sp<android::IOMX> &p1,
                      const android::sp<android::MetaData> &p2, bool p3, const android::sp<android::MediaSource> &p4,
                      const char *p5, uint32_t p6, const android::sp<ANativeWindow> &p7))

  DEFINE_METHOD0(android::sp<android::ProcessState>, processstate_self)
  DEFINE_METHOD1(void, processstate_startThreadPool, (android::sp<android::ProcessState> p1))

  DEFINE_METHOD1(ANativeWindowBuffer*, graphicbuffer_getNativeBuffer, (android::GraphicBuffer *p1))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(create_mediabuffer)
    RESOLVE_METHOD(destroy_mediabuffer)
    RESOLVE_METHOD(mediabuffer_release)
    RESOLVE_METHOD(mediabuffer_add_ref)
    RESOLVE_METHOD(mediabuffer_data)
    RESOLVE_METHOD(mediabuffer_size)
    RESOLVE_METHOD(mediabuffer_range_offset)
    RESOLVE_METHOD(mediabuffer_range_length)
    RESOLVE_METHOD(mediabuffer_set_range)
    RESOLVE_METHOD(mediabuffer_graphicBuffer)
    RESOLVE_METHOD(mediabuffer_meta_data)
    RESOLVE_METHOD(mediabuffer_setObserver)
    RESOLVE_METHOD(mediabuffer_reset)
    RESOLVE_METHOD(mediabuffer_clone)
    RESOLVE_METHOD(mediabuffer_refcount)

    RESOLVE_METHOD(create_metadata)
    RESOLVE_METHOD(destroy_metadata)
    RESOLVE_METHOD(metadata_clear)
    RESOLVE_METHOD(metadata_remove)
    RESOLVE_METHOD(metadata_setCString)
    RESOLVE_METHOD(metadata_setInt32)
    RESOLVE_METHOD(metadata_setInt64)
    RESOLVE_METHOD(metadata_setFloat)
    RESOLVE_METHOD(metadata_setPointer)
    RESOLVE_METHOD(metadata_setRect)
    RESOLVE_METHOD(metadata_findCString)
    RESOLVE_METHOD(metadata_findInt32)
    RESOLVE_METHOD(metadata_findInt64)
    RESOLVE_METHOD(metadata_findFloat)
    RESOLVE_METHOD(metadata_findPointer)
    RESOLVE_METHOD(metadata_findRect)
    RESOLVE_METHOD(metadata_setData)
    RESOLVE_METHOD(metadata_findData)

    RESOLVE_METHOD(create_mediasource)
    RESOLVE_METHOD(destroy_mediasource)

    RESOLVE_METHOD(create_readoptions)
    RESOLVE_METHOD(destroy_readoptions)
    RESOLVE_METHOD(mediasource_readoptions_setSeekTo)
    RESOLVE_METHOD(mediasource_readoptions_clearSeekTo)
    RESOLVE_METHOD(mediasource_readoptions_getSeekTo)
    RESOLVE_METHOD(mediasource_readoptions_setLateBy)
    RESOLVE_METHOD(mediasource_readoptions_getLateBy)

    RESOLVE_METHOD(create_omxclient)
    RESOLVE_METHOD(destroy_omxclient)
    RESOLVE_METHOD(omxclient_connect)
    RESOLVE_METHOD(omxclient_disconnect)
    RESOLVE_METHOD(omxclient_interface)

    RESOLVE_METHOD(omxcodec_Create)

    RESOLVE_METHOD(processstate_self)
    RESOLVE_METHOD(processstate_startThreadPool)

    RESOLVE_METHOD(graphicbuffer_getNativeBuffer)
  END_METHOD_RESOLVE()
};


/*
class DllLibStagefrightLoader
{
public:
  DllLibStagefrightLoader() { m_refcount = 0; m_stagefright_dll = NULL; } ;
  
  bool LoadStagefright(DllLibStagefright *dll)
  {
    if (m_stagefright_dll)
    {
      m_refcount++;
      dll = m_stagefright_dll;
      return true;
    }
    m_stagefright_dll = new DllLibStagefright;
    if (m_stagefright_dll && m_stagefright_dll->Load())
    {
      m_refcount++;
      dll = m_stagefright_dll;
      return true;
    }
    dll = NULL;
    return false;
  }
  void UnloadStagefright()
  {
    m_refcount--;
    if (!m_refcount)
    {
      m_stagefright_dll->Unload();
      delete m_stagefright_dll;
      m_stagefright_dll = NULL;
    }
  }
private:
  DllLibStagefright *m_stagefright_dll;
  int m_refcount;
};
*/
