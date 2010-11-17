INCLUDES=-I../../ -I. -I../../linux/ -I../../../lib/

SRCS=MusicAlbumInfo.cpp \
     MusicArtistInfo.cpp \
     MusicInfoScanner.cpp \
     MusicInfoScraper.cpp \

LIB=musicscanner.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
