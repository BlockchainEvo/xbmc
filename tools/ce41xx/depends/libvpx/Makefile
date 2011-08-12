include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=libvpx
VERSION=v0.9.7
BASE_URL=http://webm.googlecode.com/files/
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.bz2
export CROSS=$(HOST)-
export LD=$(CC)

# configuration settings
#gstreamer fails to build without the encoder
CONFIGURE=./configure --prefix=$(PREFIX) --target=x86-linux-gcc --cpu=atom \
  --disable-examples  --disable-install-bins --enable-vp8-encoder \
  --enable-vp8-decoder --disable-install-docs --enable-shared --disable-static \

LIBDYLIB=$(SOURCE)/$(LIBNAME)/$(LIBNAME).so

CLEAN_FILES=$(ARCHIVE) $(SOURCE)

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(SOURCE): $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -rf $(SOURCE)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	cd $(SOURCE); sed -i 's|add_cflags  -m$${bits}||' build/make/configure.sh
	cd $(SOURCE); sed -i 's|add_ldflags -m$${bits}||' build/make/configure.sh
	cd $(SOURCE); $(CONFIGURE)

$(LIBDYLIB): $(SOURCE)
	make -j $(JOBS) -C $(SOURCE)

.installed:
	make -C $(SOURCE) install
	touch $@

clean:
	make -C $(SOURCE) clean
	rm -f .installed

distclean::
	rm -rf $(SOURCE) .installed

