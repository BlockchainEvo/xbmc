#pragma once
#include <media/stagefright/MediaSource.h>

namespace android
{
  class MetaData;
  class MediaBuffer;
}

class MediaSourceFunctions
{
public:
  MediaSourceFunctions(){};
  virtual android::sp<android::MetaData> getFormat()=0;
  virtual android::status_t start(android::MetaData *params)=0;
  virtual android::status_t stop()=0;
  virtual android::status_t read(android::MediaBuffer **buffer,const android::MediaSource::ReadOptions *options)=0;
};


class XBMCMediaSource : public android::MediaSource
{
public:
  XBMCMediaSource(MediaSourceFunctions *functions)
  {
    m_functions = functions;
  }

  virtual android::sp<android::MetaData> getFormat()
  {
    return m_functions->getFormat();
  }

  virtual android::status_t start(android::MetaData *params)
  {
    return m_functions->start(params);
  }

  virtual android::status_t stop()
  {
    return m_functions->stop();
  }

  virtual android::status_t read(android::MediaBuffer **buffer,
                        const android::MediaSource::ReadOptions *options)
  {
    return m_functions->read(buffer, options);
  }
private:
  MediaSourceFunctions *m_functions;
};

extern "C"
{
  namespace android
  {

    XBMCMediaSource* create_mediasource(void *priv, android::sp<android::MetaData> meta, MediaSourceFunctions* functions);
    void destroy_mediasource(XBMCMediaSource *mediasource);
    MediaSource::ReadOptions* create_readoptions();
    void destroy_readoptions(MediaSource::ReadOptions* options);
    void mediasource_readoptions_setSeekTo(MediaSource::ReadOptions* options, int64_t time_us, MediaSource::ReadOptions::SeekMode mode = MediaSource::ReadOptions::SEEK_CLOSEST_SYNC);
    void mediasource_readoptions_clearSeekTo(MediaSource::ReadOptions* options);
    bool mediasource_readoptions_getSeekTo(MediaSource::ReadOptions* options, int64_t *time_us, MediaSource::ReadOptions::SeekMode *mode);
    void mediasource_readoptions_setLateBy(MediaSource::ReadOptions* options, int64_t lateness_us);
    int64_t mediasource_readoptions_getLateBy(MediaSource::ReadOptions* options);
  }
}

