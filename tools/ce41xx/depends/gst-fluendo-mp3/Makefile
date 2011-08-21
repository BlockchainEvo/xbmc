include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=gst-fluendo-mp3
VERSION=0.10.15
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

BASE_URL=http://core.fluendo.com/gstreamer/src/gst-fluendo-mp3

# configuration settings
CONFIGURE=./autogen.sh --prefix=$(PREFIX) --host=$(HOST) \
  --disable-maintainer-mode --enable-cpu-tune-atom

LIBDYLIB=$(SOURCE)/src/.libs/libgstfluimp3.so

CLEAN_FILES=$(ARCHIVE) $(SOURCE)

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(SOURCE): $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -rf $(SOURCE)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	cd $(SOURCE); $(CONFIGURE)
	# configure defaults to -Werror if svn build, remove it.
	sed -i "s|-Wall -Werror|-Wall|" $(SOURCE)/Makefile $(SOURCE)/src/Makefile $(SOURCE)/common/Makefile

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
