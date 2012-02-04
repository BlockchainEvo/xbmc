INCLUDES=-I. -I../.. -I../../linux
SRCS= \
     WinSystemGDL.cpp \
     WinSystemGDLGLES.cpp

LIB=windowing_gdl.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
