INCLUDES=-I. -I.. -I../../ -I../linux -I../cores -I../posix -I../../lib/jsoncpp/jsoncpp/include -I../../lib

SRCS=AlarmClock.cpp \
     Archive.cpp \
     AutoPtrHandle.cpp \
     Builtins.cpp \
     CharsetConverter.cpp \
     Crc32.cpp \
     DownloadQueue.cpp \
     DownloadQueueManager.cpp \
     fstrcmp.c \
     fft.cpp \
     GUIInfoManager.cpp \
     HTMLTable.cpp \
     HTMLUtil.cpp \
     HttpHeader.cpp \
     IMDB.cpp \
     InfoLoader.cpp \
     log.cpp \
     RegExp.cpp \
     RssReader.cpp \
     ScraperParser.cpp \
     Splash.cpp \
     Stopwatch.cpp \
     SystemInfo.cpp \
     TuxBoxUtil.cpp \
     Weather.cpp \
     Win32Exception.cpp \
     CPUInfo.cpp \
     PCMAmplifier.cpp \
     PCMRemap.cpp \
     LabelFormatter.cpp \
     BitstreamStats.cpp \
     PerformanceStats.cpp \
     PerformanceSample.cpp \
     LCDFactory.cpp \
     LCD.cpp \
     Fanart.cpp \
     ScraperUrl.cpp \
     md5.cpp \
     ArabicShaping.cpp \
     AsyncFileCopy.cpp \
     DbusServer.cpp \
     StreamDetails.cpp \
     TimeUtils.cpp \
     JobManager.cpp \
     Teletext.cpp \
     fastmemcpy.c \
     PasswordManager.cpp \
     AliasShortcutUtils.cpp \
     AnnouncementManager.cpp \
     RingBuffer.cpp \
     FileOperationJob.cpp \
     FileUtils.cpp \
     Variant.cpp \
     XMLUtils.cpp \
     StringUtils.cpp \
     URIUtils.cpp \

LIB=utils.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
