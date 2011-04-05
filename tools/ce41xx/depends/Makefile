include ../Makefile.include

BUILDTOOLS = help2man autoconf automake libtool pkg-config yasm cmake patchelf

SUBDIRS = \
	pcre expat gettext readline sqlite3 libgpg-error libgcrypt \
	bzip2 liblzo2 freetype2 fontconfig \
	openssl libssh2 curl \
	libpng jpeg tiff jasper \
	libogg libvorbis libflac libmad fribidi libwavpack libmpeg2 \
	libass libcdio libsamplerate \
	libmicrohttpd libmodplug librtmp libxml2 libbluray \
	libsdl libsdl_mixer libsdl_image \
	samba dbus python26 mysqlclient boost xbmc \

.PHONY: $(BUILDTOOLS) $(SUBDIRS)

all: $(BUILDTOOLS) $(SUBDIRS)

$(BUILDTOOLS):
	$(MAKE) -C $@

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(BUILDTOOLS); do $(MAKE) -C $$d clean; done
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

distclean::
	for d in $(BUILDTOOLS); do $(MAKE) -C $$d distclean; done
	for d in $(SUBDIRS); do $(MAKE) -C $$d distclean; done
	rm -rf $(XBMCSTAGE)/usr/local
