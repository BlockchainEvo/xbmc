include ../../Makefile.include
include ../intel-sdk.mk

SOURCE=../../../../
# configuration settings
export PKG_CONFIG_PATH=$(PREFIX)/lib/pkgconfig
CONFIGURE=./configure --prefix=$(PREFIX) --host=$(HOST) \
  --enable-external-python --enable-gles --disable-xrandr \
  --disable-optical-drive --disable-dvdcss --enable-optimizations

all: configure

configure:
	cd $(SOURCE); ./bootstrap
	cd $(SOURCE); $(CONFIGURE)
	cd $(SOURCE); make
	cd $(SOURCE); make install

