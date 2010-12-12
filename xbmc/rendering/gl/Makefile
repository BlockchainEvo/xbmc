INCLUDES=-I. -I../.. -I../../linux

SRCS=GUIWindowTestPatternGL.cpp \
     RenderSystemGL.cpp \
     
LIB=rendering_gl.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
