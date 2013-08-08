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
#include "DllLibStagefrightMetaData.h"
#include "DllLibStagefright.h"
#include <media/stagefright/MetaData.h>

namespace android{

void RefBase::decStrong(void const* foo) const
{
  return;
}
void RefBase::incStrong(void const* foo) const
{
  return;
}
}
int32_t android_atomic_dec(volatile int32_t* addr) {
  return 0;
}
int32_t android_atomic_inc(volatile int32_t* addr) {
  return 0;
}

DllMetaData::DllMetaData()
:m_metadata(NULL)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_metadata = m_dll->create_metadata();
}

DllMetaData::DllMetaData(const android::sp<android::MetaData> &metadata)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_metadata = metadata;
}

DllMetaData::DllMetaData(DllMetaData *metadata)
{
  m_dll = new DllLibStagefright;
  if (m_dll && m_dll->Load())
    m_metadata = metadata->get();
}

DllMetaData::~DllMetaData()
{
  if(m_dll && m_metadata != NULL)
    m_dll->destroy_metadata(m_metadata.get());
  delete m_dll;
}

DllMetaData::operator android::sp<android::MetaData>()
{
  return get();
}

void DllMetaData::clear()
{
  if (m_metadata != NULL)
    m_dll->metadata_clear(m_metadata);
  return;
}

bool DllMetaData::remove(uint32_t key)
{
  if (m_metadata != NULL)
    return m_dll->metadata_remove(m_metadata, key);
  return false;
}

bool DllMetaData::setCString(uint32_t key, const char *value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setCString(m_metadata, key, value);
  return false;
}

bool DllMetaData::setInt32(uint32_t key, int32_t value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setInt32(m_metadata, key, value);
  return false;
}

bool DllMetaData::setInt64(uint32_t key, int64_t value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setInt64(m_metadata, key, value);
  return false;
}

bool DllMetaData::setFloat(uint32_t key, float value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setFloat(m_metadata, key, value);
  return false;
}

bool DllMetaData::setPointer(uint32_t key, void *value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setPointer(m_metadata, key, value);
  return false;
}

bool DllMetaData::setRect(uint32_t key, int32_t left, int32_t top, int32_t right, int32_t bottom)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setRect(m_metadata, key, left, top, right, bottom);
  return false;
}

bool DllMetaData::findCString(uint32_t key, const char **value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findCString(m_metadata, key, value);
  return false;
}

bool DllMetaData::findInt32(uint32_t key, int32_t *value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findInt32(m_metadata, key, value);
  return false;
}

bool DllMetaData::findInt64(uint32_t key, int64_t *value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findInt64(m_metadata, key, value);
  return false;
}

bool DllMetaData::findFloat(uint32_t key, float *value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findFloat(m_metadata, key, value);
  return false;
}

bool DllMetaData::findPointer(uint32_t key, void **value)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findPointer(m_metadata, key, value);
  return false;
}

bool DllMetaData::findRect(uint32_t key,int32_t *left, int32_t *top,int32_t *right, int32_t *bottom)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findRect(m_metadata, key, left, top, right, bottom);
  return false;
}

bool DllMetaData::setData(uint32_t key, uint32_t type, const void *data, size_t size)
{
  if (m_metadata != NULL)
    return m_dll->metadata_setData(m_metadata, key, type, data, size);
  return false;
}

bool DllMetaData::findData(uint32_t key, uint32_t *type,const void **data, size_t *size)
{
  if (m_metadata != NULL)
    return m_dll->metadata_findData(m_metadata, key, type, data, size);
  return false;
}

android::sp<android::MetaData> DllMetaData::get() const
{
  return m_metadata;
}
/*
void DllMetaData::set(android::sp<android::MetaData> metadata)
{
  m_metadata = metadata;
}
*/
