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
#include "DllLibStagefrightMediaSource.h"
#include "DllLibStagefright.h"
#include "DllLibStagefrightMediaBuffer.h"
#include "DllLibStagefrightMetaData.h"
#include "DllLibStagefrightReadOptions.h"
#include "StageFrightVideo.h"
#include "StageFrightVideoPrivate.h"

DllMediaSource::DllMediaSource(CStageFrightVideoPrivate *priv, android::sp<android::MetaData> meta)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_mediasource = m_dll->create_mediasource(this);
  source_meta = meta;
  p = priv;
}

DllMediaSource::DllMediaSource(CStageFrightVideoPrivate *priv, DllMetaData* meta)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_mediasource = m_dll->create_mediasource(this);
  source_meta = meta->get();
  p = priv;
}

DllMediaSource::~DllMediaSource()
{
  if (m_dll && m_mediasource)
    m_dll->destroy_mediasource(m_mediasource);
  delete m_dll;
}

android::sp<android::MetaData> DllMediaSource::getFormat()
{
  return source_meta;
}

android::status_t DllMediaSource::start(android::MetaData* metadata)
{
  return android::OK;
}

android::status_t DllMediaSource::stop()
{
  return android::OK;
}

android::status_t DllMediaSource::read(android::MediaBuffer **buffer, const android::MediaSource::ReadOptions *options)
{
    Frame *frame;
    status_t ret;
    *buffer = NULL;
    int64_t time_us = -1;
    android::MediaSource::ReadOptions::SeekMode mode;
    if (options)
    {
      DllReadOptions readoptions((android::MediaSource::ReadOptions *)options);
      if (readoptions.getSeekTo(&time_us, &mode))
      {
#if defined(DEBUG_VERBOSE)
        CLog::Log(LOGDEBUG, "%s: reading source(%d): seek:%llu\n", CLASSNAME,p->in_queue.size(), time_us);
#endif
      }
      else
      {
#if defined(DEBUG_VERBOSE)
        CLog::Log(LOGDEBUG, "%s: reading source(%d)\n", CLASSNAME,p->in_queue.size());
#endif
      }
    }
    else
    {
#if defined(DEBUG_VERBOSE)
      CLog::Log(LOGDEBUG, "%s: reading source(%d)\n", CLASSNAME,p->in_queue.size());
#endif
    }

    p->in_mutex.lock();
    while (p->in_queue.empty() && p->decode_thread)
      p->in_condition.wait(p->in_mutex);

    if (p->in_queue.empty())
    {
      p->in_mutex.unlock();
      return VC_ERROR;
    }
    
    std::map<int64_t,Frame*>::iterator it = p->in_queue.begin();
    frame = it->second;
    ret = frame->status;
    *buffer = frame->medbuf->get();

    p->in_queue.erase(it);
    p->in_mutex.unlock();

#if defined(DEBUG_VERBOSE)
    CLog::Log(LOGDEBUG, ">>> exiting reading source(%d); pts:%llu\n", p->in_queue.size(),frame->pts);
#endif

    free(frame);

    return ret;
}

XBMCMediaSource* DllMediaSource::get()
{
  return m_mediasource;
}
