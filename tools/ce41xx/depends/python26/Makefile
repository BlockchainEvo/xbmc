include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=Python
VERSION=2.6.5
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tar.bz2

# configuration settings
export EXTRA_CFLAGS=$(CFLAGS)
NATIVE_ENV=CFLAGS="" CXXFLAGS="" LDFLAGS="" CPPFLAGS="" EXTRA_CFLAGS="" LIBS="" CPP="cpp -E" CC=gcc CXX=g++ LD=ld AR=ar RANLIB=ranlib
CONFIGURE_NATIVE=$(NATIVE_ENV) ./configure
CONFIGURE=./configure --prefix=$(PREFIX) --host=$(HOST) \
  --disable-toolbox-glue --enable-unicode=ucs4

LIBDYLIB=$(SOURCE)/libpython2.6.so

CLEAN_FILES=$(ARCHIVE) $(SOURCE)

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(LIBDYLIB): $(TARBALLS_LOCATION)/$(ARCHIVE)
	rm -rf $(SOURCE)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	cd $(SOURCE); $(CONFIGURE_NATIVE)
	cd $(SOURCE); make EXTRA_CFLAGS="" python Parser/pgen
	cd $(SOURCE); mv python hostpython
	cd $(SOURCE); mv Parser/pgen Parser/hostpgen
	cd $(SOURCE); make distclean
	cd $(SOURCE); patch -p1 < ../Python-2.6.5-xcompile.patch
	cd $(SOURCE); $(CONFIGURE)
	cd $(SOURCE); make HOSTPYTHON=./hostpython HOSTPGEN=./Parser/hostpgen BLDSHARED="i686-cm-linux-gcc -shared" CROSS_COMPILE="i686-cm-linux-" CROSS_COMPILE_TARGET=yes
	cd $(SOURCE); make HOSTPYTHON=./hostpython HOSTPGEN=./Parser/hostpgen BLDSHARED="i686-cm-linux-gcc -shared" CROSS_COMPILE="i686-cm-linux-" CROSS_COMPILE_TARGET=yes libpython2.6.so

.installed:
	cd $(SOURCE); make -n install HOSTPYTHON=./hostpython BLDSHARED="i686-cm-linux-gcc -shared" CROSS_COMPILE="i686-cm-linux-" CROSS_COMPILE_TARGET=yes prefix=$(PREFIX)
	cp $(SOURCE)/libpython2.6.so $(PREFIX)/lib
	chmod 755 $(PREFIX)/lib/libpython2.6.so
	touch $@

clean:
	rm -rf $(SOURCE) .installed

distclean::
	rm -rf $(SOURCE) .installed
