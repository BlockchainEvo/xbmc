SUBDIRS = \
        yasm cmake \
        pcre expat gettext readline sqlite3 \
        liblzo2 freetype2 fontconfig \
        openssl libssh2 curl \
        libogg libvorbis libflac libmad fribidi libwavpack libmpeg2 \
        libass libcdio libsamplerate \
        libmicrohttpd libmodplug \

        #samba python26 mysqlclient boost \

# problem libs -> tiff librtmp libbluray

.PHONY: $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

distclean::
	for d in $(SUBDIRS); do $(MAKE) -C $$d distclean; done
        #rm -rf $(BUILDDIR)
