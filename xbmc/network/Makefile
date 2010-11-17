INCLUDES=-I../../ -I. -I.. -I../linux -I../../lib -I../../lib/jsoncpp/jsoncpp/include

INCLUDES+=-I../lib/libUPnP/Platinum/Source/Core \
          -I../lib/libUPnP/Platinum/Source/Platinum \
          -I../lib/libUPnP/Platinum/Source/Devices/MediaServer \
          -I../lib/libUPnP/Platinum/Source/Devices/MediaConnect \
          -I../lib/libUPnP/Platinum/Source/Devices/MediaRenderer \
          -I../lib/libUPnP/Neptune/Source/System/Posix \
          -I../lib/libUPnP/Neptune/Source/Core

SRCS=cddb.cpp \
     DNSNameCache.cpp \
     EventClient.cpp \
     EventPacket.cpp \
     EventServer.cpp \
     GUIDialogAccessPoints.cpp \
     GUIDialogNetworkSetup.cpp \
     Network.cpp \
     Socket.cpp \
     TCPServer.cpp \
     UdpClient.cpp \
     UPnP.cpp \
     WebServer.cpp \
     ZeroconfBrowser.cpp \
     Zeroconf.cpp \

LIB=network.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
