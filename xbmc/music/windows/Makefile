INCLUDES=-I../../ -I. -I../../linux/ -I../../../lib/

SRCS=GUIWindowMusicBase.cpp \
     GUIWindowMusicNav.cpp \
     GUIWindowMusicOverlay.cpp \
     GUIWindowMusicPlaylist.cpp \
     GUIWindowMusicPlaylistEditor.cpp \
     GUIWindowMusicSongs.cpp \

LIB=musicwindows.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
