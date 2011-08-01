include ../../Makefile.include
include ../intel-sdk.mk

SOURCE=../../../../
# configuration settings
export CXXFLAGS+=-isystem$(SDKSTAGE)/include/linux_user
export ALSA_CFLAGS=-I$(SDKSTAGE)/usr/include/alsa
export ALSA_LIBS=-L$(SDKSTAGE)/usr/lib -lasound
CONFIGURE=./configure --prefix=$(PREFIX) --host=$(HOST) \
  --enable-external-python --enable-gles --disable-xrandr \
  --disable-optical-drive --disable-dvdcss --enable-optimizations \
  --disable-ffmpeg

all: configure

configure:
	cd $(SOURCE); ./bootstrap
	cd $(SOURCE); $(CONFIGURE)
	cd $(SOURCE); make -j $(JOBS)
	#cd $(SOURCE); make install

clean:
	cd $(SOURCE); make clean

distclean:
	cd $(SOURCE); make clean
