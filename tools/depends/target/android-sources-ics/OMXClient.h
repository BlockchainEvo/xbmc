#pragma once
#include <media/stagefright/OMXClient.h>
extern "C"
{
  namespace android
  {
    OMXClient* create_omxclient();
    void destroy_omxclient(OMXClient*);
    status_t omxclient_connect(OMXClient*);
    void omxclient_disconnect(OMXClient*);
    sp<IOMX> interface(OMXClient*);
  }
}
