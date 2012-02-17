include ../Makefile.include

BUILDTOOLS = help2man autoconf automake libtool pkg-config yasm cmake

SUBDIRS = \
	pcre expat gettext readline sqlite3 libgpg-error libgcrypt \
	bzip2 liblzo2 freetype2 fontconfig \
	openssl libssh2 curl \
	libjpeg-turbo tiff jasper \
	libogg libvorbis libflac libid3tag libmad fribidi libmpeg2 \
	enca libass libcdio libsamplerate \
	libmicrohttpd libmodplug librtmp libxml2 yajl libvpx \
	liboil libxml2 glib-2.0 liba52 libdca libsoup gstreamer \
	gstreamer-plugins-base gstreamer-plugins-good \
	gstreamer-plugins-bad gstreamer-plugins-ugly  \
	gst-fluendo-mp3 gst-fluendo-mpegdemux gst-fluendo-ismd \
	samba dbus python26 mysqlclient boost libusb xbmc \

IMAGENAME = xbmc-selectv-`date +'%y%m%d'`

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
	rm -rf $(XBMCPREFIX)/*

image:
	make -C $(BUILDROOT) tarroot IMAGE=$(CURDIR)/$(IMAGENAME) TAR_COMPRESSOR=
	echo "S111xbmc S102init_pic_uart" | cat $(TARGETFS)/etc/init.d/rc.deps - > $(CURDIR)/packaging/etc/init.d/rc.deps
	cd packaging; tar -uf $(CURDIR)/$(IMAGENAME).tar *
	tar -X excludes.txt -uf $(CURDIR)/$(IMAGENAME).tar $(XBMCPREFIX)
	gzip $(IMAGENAME).tar
