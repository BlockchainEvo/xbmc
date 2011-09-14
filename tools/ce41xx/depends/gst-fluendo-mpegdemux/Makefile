include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=gst-fluendo-mpegdemux
VERSION=0.10.68
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.bz2
BASE_URL=http://core.fluendo.com/gstreamer/src/gst-fluendo-mpegdemux/

# configuration settings
export CFLAGS+=-isystem$(SDKSTAGE)/include/linux_user
CONFIGURE=./configure --prefix=$(PREFIX) --host=$(HOST) \
  --disable-maintainer-mode --enable-cpu-tune-atom

LIBDYLIB=$(SOURCE)/src/.libs/libgstfluismd.so

CLEAN_FILES=$(ARCHIVE) $(SOURCE)

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(SOURCE): $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -rf $(SOURCE)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	cd $(SOURCE); patch -p0 <../lpcm.patch
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
