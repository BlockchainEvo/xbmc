include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=gst-plugins-base
VERSION=0.10.30
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

BASE_URL=http://gstreamer.freedesktop.org/src/gst-plugins-base

# configuration settings
CONFIGURE=./autogen.sh --prefix=$(PREFIX) --host=$(HOST) \
  --disable-docbook  \
  --disable-gtk-doc  \
  --without-x \
  --disable-x \
  --disable-xvideo \
  --disable-xshm \
  --disable-examples \
  --disable-gnome_vfs \
  --disable-cdparanoia

LIBDYLIB=$(SOURCE)/gst-libs/gst/video/.libs/libgstvideo-0.10.dylib

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(SOURCE): $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -rf $(SOURCE)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	cd $(SOURCE); $(CONFIGURE)

$(LIBDYLIB): $(SOURCE)
	make -j 1 -C $(SOURCE)

.installed:
	make -C $(SOURCE) install
	touch $@

clean:
	make -C $(SOURCE) clean
	rm -f .installed

distclean::
	rm -rf $(SOURCE) .installed
