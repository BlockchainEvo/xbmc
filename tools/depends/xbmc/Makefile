-include ../Makefile.include

SOURCE=../../../

export CXXFLAGS+=-O3
export CFLAGS+=-O3

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) build-aux/ ;\
  ./configure --prefix=$(PREFIX) --host=$(HOST) \
  --enable-neon --enable-gles --enable-debug \
  --disable-sdl --disable-x11 --disable-xrandr \
  --disable-optical-drive --disable-joystick \
  --enable-shared-lib --disable-alsa


all: $(SOURCE)/libxbmc.so


$(SOURCE)/libxbmc.so:
	cd $(SOURCE); ./bootstrap
	cd $(SOURCE); $(CONFIGURE)

../Makefile.include:
	$(error Please run edit and run setup-sdk.sh)

clean:
	cd $(SOURCE); $(MAKE) clean

distclean:
	cd $(SOURCE); $(MAKE) clean
