#pragma once
#include <binder/ProcessState.h>
extern "C"
{
  namespace android
  {
    sp<ProcessState> processstate_self();
    void processstate_startThreadPool(sp<ProcessState> state);
  }
}
