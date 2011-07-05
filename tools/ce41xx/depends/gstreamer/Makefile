include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=gstreamer
#VERSION=0.10.30
VERSION=0.10.35
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

BASE_URL=http://gstreamer.freedesktop.org/src/gstreamer

# configuration settings
CONFIGURE=./autogen.sh --prefix=$(PREFIX) --host=$(HOST) \
  --disable-docbook  \
  --disable-gtk-doc  \
  --disable-failing-tests \
  --disable-examples \
  --disable-check \
  --disable-tests \
  --disable-net

LIBDYLIB=$(SOURCE)/gst/.libs/libgstreamer-0.10.0.dylib

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
