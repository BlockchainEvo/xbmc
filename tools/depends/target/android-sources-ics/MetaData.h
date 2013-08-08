#pragma once

#include <media/stagefright/MetaData.h>
class MetaDataDummy : public android::MetaData
{
public:
  MetaDataDummy() : android::MetaData(){};
};

class MetaDataDummy;
extern "C"
{
  namespace android
  {

    void metadata_clear(MetaDataDummy* metadata);
    bool metadata_remove(MetaDataDummy* metadata, uint32_t key);

    bool metadata_setCString(MetaDataDummy* metadata, uint32_t key, const char *value);
    bool metadata_setInt32(MetaDataDummy* metadata, uint32_t key, int32_t value);
    bool metadata_setInt64(MetaDataDummy* metadata, uint32_t key, int64_t value);
    bool metadata_setFloat(MetaDataDummy* metadata, uint32_t key, float value);
    bool metadata_setPointer(MetaDataDummy* metadata, uint32_t key, void *value);

    bool metadata_setRect(MetaDataDummy* metadata, uint32_t key, int32_t left, int32_t top, int32_t right, int32_t bottom);

    bool metadata_findCString(MetaDataDummy* metadata, uint32_t key, const char **value);
    bool metadata_findInt32(MetaDataDummy* metadata, uint32_t key, int32_t *value);
    bool metadata_findInt64(MetaDataDummy* metadata, uint32_t key, int64_t *value);
    bool metadata_findFloat(MetaDataDummy* metadata, uint32_t key, float *value);
    bool metadata_findPointer(MetaDataDummy* metadata, uint32_t key, void **value);

    bool metadata_findRect(MetaDataDummy* metadata, uint32_t key, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom);

    bool metadata_setData(MetaDataDummy* metadata, uint32_t key, uint32_t type, const void *data, size_t size);

    bool metadata_findData(MetaDataDummy* metadata, uint32_t key, uint32_t *type, const void **data, size_t *size);
  }
}
