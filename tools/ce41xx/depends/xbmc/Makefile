include ../../Makefile.include
include ../intel-sdk.mk

SOURCE=../../../../
# configuration settings
export CXXFLAGS+=-isystem$(SDKSTAGE)/include/linux_user
CONFIGURE=./configure --prefix=$(PREFIX) --host=$(HOST) \
  --enable-external-python --enable-gles --disable-xrandr \
  --disable-optical-drive --disable-dvdcss --disable-optimizations \
  --disable-ffmpeg --disable-debug --disable-non-free --disable-sdl --disable-x11

all: configure

configure:
	cd $(SOURCE); ./bootstrap
	cd $(SOURCE); $(CONFIGURE)
	cd $(SOURCE); make -j $(JOBS)
	cd $(SOURCE); make install
	find $(PREFIX)/share/xbmc/addons/skin.confluence/media -not -name *.xbt -exec rm {} \;
clean:
	cd $(SOURCE); make clean

distclean:
	cd $(SOURCE); make clean
