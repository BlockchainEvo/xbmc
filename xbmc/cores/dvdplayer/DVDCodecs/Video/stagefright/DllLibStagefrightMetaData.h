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
#include <MetaData.h>

namespace android
{
  class MetaData;
}

class DllLibStagefright;

class DllMetaData
{
public:
  DllMetaData();
  ~DllMetaData();
  DllMetaData(const android::sp<android::MetaData> &metadata);
  DllMetaData(DllMetaData *metadata);
  operator android::sp<android::MetaData>();
  void clear();
  bool remove(uint32_t key);
  bool setCString(uint32_t key, const char *value);
  bool setInt32(uint32_t key, int32_t value);
  bool setInt64(uint32_t key, int64_t value);
  bool setFloat(uint32_t key, float value);
  bool setPointer(uint32_t key, void *value);
  bool setRect(uint32_t key, int32_t left, int32_t top, int32_t right, int32_t bottom);
  bool findCString(uint32_t key, const char **value);
  bool findInt32(uint32_t key, int32_t *value);
  bool findInt64(uint32_t key, int64_t *value);
  bool findFloat(uint32_t key, float *value);
  bool findPointer(uint32_t key, void **value);
  bool findRect(uint32_t key,int32_t *left, int32_t *top,int32_t *right, int32_t *bottom);
  bool setData(uint32_t key, uint32_t type, const void *data, size_t size);
  bool findData(uint32_t key, uint32_t *type,const void **data, size_t *size);

//  void set(android::sp<android::MetaData> metadata);

  android::sp<android::MetaData> get() const;
private:
  DllLibStagefright *m_dll;
  android::sp<android::MetaData> m_metadata;
};

