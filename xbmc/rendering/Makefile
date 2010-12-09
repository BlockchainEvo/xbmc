INCLUDES=-I. -I../ -I../linux -I../../lib

SRCS=RenderSystem.cpp
     
LIB=rendering.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
