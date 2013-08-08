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

#include <utils/StrongPointer.h>
#include <utils/Errors.h>
#include <stagefright/MediaSource.h>

namespace android
{
  class MetaData;
}

class DllMetaData;
class DllLibStagefright;
class CStageFrightVideoPrivate;
class XBMCMediaSource;

class DllMediaSource : MediaSourceFunctions
{
public:
  DllMediaSource(CStageFrightVideoPrivate *priv, android::sp<android::MetaData> meta);
  DllMediaSource(CStageFrightVideoPrivate *priv, DllMetaData* meta);
  ~DllMediaSource();

  virtual android::sp<android::MetaData> getFormat();
  virtual android::status_t              start(android::MetaData* metadata);
  virtual android::status_t              stop();
  virtual android::status_t              read(android::MediaBuffer **buffer, const android::MediaSource::ReadOptions *options);

  XBMCMediaSource* get();
private:
  android::sp<android::MetaData> source_meta;
  CStageFrightVideoPrivate *p;
  XBMCMediaSource* m_mediasource;
  DllLibStagefright *m_dll;
};

