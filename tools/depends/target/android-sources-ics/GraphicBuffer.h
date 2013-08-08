#pragma once
#include <ui/GraphicBuffer.h>
extern "C"
{
  namespace android
  {
    ANativeWindowBuffer* graphicbuffer_getNativeBuffer(GraphicBuffer *buffer);
  }
}

