INCLUDES=-I. -I../ -I../linux -I../../lib

SRCS=AutorunMediaJob.cpp \
     cdioSupport.cpp \
     DetectDVDType.cpp \
     IoSupport.cpp \
     MediaManager.cpp \
     
LIB=storage.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
