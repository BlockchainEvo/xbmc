INCLUDES=-I. -I../../ -I../../linux -I../../../lib

SRCS=scrobbler.cpp lastfmscrobbler.cpp librefmscrobbler.cpp

LIB=scrobbler.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
