include ../Makefile.include

BUILDTOOL = yasm cmake
SUBDIRS = \
		pcre expat gettext readline sqlite3 \
		liblzo2 freetype2 fontconfig \
		openssl libssh2 curl \
		tiff jasper \
		libogg libvorbis libflac libmad fribidi libwavpack libmpeg2 \
		libass libcdio libsamplerate \
		libmicrohttpd libmodplug libsdl \
		samba python26 \

		#mysqlclient boost \

		# problem libs -> librtmp libbluray

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
	rm -rf $(XBMCPREFIX)/local
