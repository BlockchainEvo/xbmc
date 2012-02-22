include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=gst-fluendo-ismd
VERSION=r2152
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.gz

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
	cd $(SOURCE); patch -p1 <../add_ts_offset.patch
	cd $(SOURCE); autoreconf -vif
	cd $(SOURCE); $(CONFIGURE)
	# configure defaults to -Werror if svn build, remove it.
	sed -i "s|-Wall -Werror|-Wall|" $(SOURCE)/Makefile $(SOURCE)/docs/Makefile $(SOURCE)/src/Makefile $(SOURCE)/examples/glplayer/Makefile

$(LIBDYLIB): $(SOURCE)
	make -j 1 -C $(SOURCE)

.installed:
	make -C $(SOURCE) install
	cp -f $(SOURCE)/src/ismd_gst_buffer.h $(PREFIX)/include/
	touch $@

clean:
	make -C $(SOURCE) clean
	rm -f .installed

distclean::
	rm -rf $(SOURCE) .installed
