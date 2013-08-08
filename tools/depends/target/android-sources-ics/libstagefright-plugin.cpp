/*
 *      Copyright (C) 2010-2013 Team XBMC
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
#include "MediaBuffer.h" 
#include "MetaData.h"
#include "MediaSource.h"
#include "OMXClient.h"
#include "OMXCodec.h"
#include "ProcessState.h"
#include "GraphicBuffer.h"

namespace android
{
  MediaBufferDummy* create_mediabuffer(size_t size)
  {
    return new MediaBufferDummy(size);
  }

  void mediabuffer_add_ref(MediaBufferDummy* mediabuffer)
  {
    mediabuffer->add_ref();
  }

  void mediabuffer_release(MediaBufferDummy* mediabuffer)
  {
    mediabuffer->release();
  }

  void* mediabuffer_data(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->data();
  }

  size_t mediabuffer_size(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->size();
  }

  size_t mediabuffer_range_offset(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->range_offset();
  }

  size_t mediabuffer_range_length(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->range_length();
  }

  void mediabuffer_set_range(MediaBufferDummy* mediabuffer, size_t offset, size_t length)
  { 
    mediabuffer->set_range(offset, length);
  }

  sp<GraphicBuffer> mediabuffer_graphicBuffer(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->graphicBuffer();
  }

  sp<MetaData> mediabuffer_meta_data(MediaBufferDummy* mediabuffer)
  { 
    mediabuffer->meta_data();
  }

  void mediabuffer_reset(MediaBufferDummy* mediabuffer)
  { 
    mediabuffer->reset();
  }

  void mediabuffer_setObserver(MediaBufferDummy* mediabuffer, MediaBufferObserver *group)
  { 
    mediabuffer->setObserver(group);
  }

  MediaBufferDummy* mediabuffer_clone(MediaBufferDummy* mediabuffer)
  { 
    return (MediaBufferDummy*)mediabuffer->clone();
  }

  int mediabuffer_refcount(MediaBufferDummy* mediabuffer)
  { 
    return mediabuffer->refcount();
  }

  void metadata_clear(MetaDataDummy* metadata)
  {
    metadata->clear();
  }

  bool metadata_remove(MetaDataDummy* metadata, uint32_t key)
  {
    metadata->remove(key);
  }

  bool metadata_setCString(MetaDataDummy* metadata, uint32_t key, const char *value)
  {
    metadata->setCString(key, value);
  }

  bool metadata_setInt32(MetaDataDummy* metadata, uint32_t key, int32_t value)
  {
    metadata->setInt32(key, value);
  }

  bool metadata_setInt64(MetaDataDummy* metadata, uint32_t key, int64_t value)
  {
    metadata->setInt64(key, value);
  }

  bool metadata_setFloat(MetaDataDummy* metadata, uint32_t key, float value)
  {
    metadata->setFloat(key, value);
  }

  bool metadata_setPointer(MetaDataDummy* metadata, uint32_t key, void *value)
  {
    metadata->setPointer(key, value);
  }

  bool metadata_setRect(MetaDataDummy* metadata, uint32_t key, int32_t left, int32_t top, int32_t right, int32_t bottom)
  {
    metadata->setRect(key, left, top, right, bottom);
  }

  bool metadata_findCString(MetaDataDummy* metadata, uint32_t key, const char **value)
  {
    metadata->findCString(key, value);
  }

  bool metadata_findInt32(MetaDataDummy* metadata, uint32_t key, int32_t *value)
  {
    metadata->findInt32(key, value);
  }

  bool metadata_findInt64(MetaDataDummy* metadata, uint32_t key, int64_t *value)
  {
    metadata->findInt64(key, value);
  }

  bool metadata_findFloat(MetaDataDummy* metadata, uint32_t key, float *value)
  {
    metadata->findFloat(key, value);
  }

  bool metadata_findPointer(MetaDataDummy* metadata, uint32_t key, void **value)
  {
    metadata->findPointer(key, value);
  }

  bool metadata_findRect(MetaDataDummy* metadata, uint32_t key, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom)
  {
    metadata->findRect(key, left, top, right, bottom);
  }

  bool metadata_setData(MetaDataDummy* metadata, uint32_t key, uint32_t type, const void *data, size_t size)
  {
    metadata->setData(key, type, data, size);
  }

  bool metadata_findData(MetaDataDummy* metadata, uint32_t key, uint32_t *type, const void **data, size_t *size)
  {
    metadata->findData(key, type, data, size);
  }

  XBMCMediaSource* create_mediasource(MediaSourceFunctions *functions)
  {
    return new XBMCMediaSource(functions);
  }

  void destroy_mediasource(XBMCMediaSource *mediasource)
  {
    delete mediasource;
  }

  MediaSource::ReadOptions* create_readoptions()
  {
    return new MediaSource::ReadOptions;
  }

  void destroy_readoptions(MediaSource::ReadOptions* options)
  {
    delete options;
  }

  void mediasource_readoptions_setSeekTo(MediaSource::ReadOptions* options, int64_t time_us, MediaSource::ReadOptions::SeekMode mode)
  {
    return options->setSeekTo(time_us, mode);
  }

  void mediasource_readoptions_clearSeekTo(MediaSource::ReadOptions* options)
  {
    options->clearSeekTo();
  }

  bool mediasource_readoptions_getSeekTo(MediaSource::ReadOptions* options, int64_t *time_us, MediaSource::ReadOptions::SeekMode *mode)
  {
    return options->getSeekTo(time_us, mode);
  }

  void mediasource_readoptions_setLateBy(MediaSource::ReadOptions* options, int64_t lateness_us)
  {
    options->setLateBy(lateness_us);
  }

  int64_t mediasource_readoptions_getLateBy(MediaSource::ReadOptions* options)
  {
    return options->getLateBy();
  }

  OMXClient* create_omxclient()
  {
    return new OMXClient;
  }

  void destroy_omxclient(OMXClient* client)
  {
    delete client;
  }

  status_t omxclient_connect(OMXClient* client)
  {
    return client->connect();
  }

  void omxclient_disconnect(OMXClient* client)
  {
    client->disconnect();
  }

  sp<IOMX> omxclient_interface(OMXClient* client)
  {
    return client->interface();
  }

  sp<MediaSource> omxcodec_Create(const sp<IOMX> &omx,
            const sp<MetaData> &meta, bool createEncoder,
            const sp<MediaSource> &source,
            const char *matchComponentName,
            uint32_t flags,
            const sp<ANativeWindow> &nativeWindow)
  {
    return OMXCodec::Create(omx, meta, createEncoder, source, matchComponentName, flags, nativeWindow);
  }

  sp<ProcessState> processstate_self()
  {
    return ProcessState::self();
  }

  void processstate_startThreadPool(sp<ProcessState> state)
  {
    state->startThreadPool();
  }

  ANativeWindowBuffer* graphicbuffer_getNativeBuffer(GraphicBuffer *buffer)
  {
    return buffer->getNativeBuffer();
  }

}
