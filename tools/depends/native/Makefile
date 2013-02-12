include ../Makefile.include

ifneq ($(shell test -f $(NATIVEPREFIX)/share/config.site && echo 1),1)
  $(error Error: $(NATIVEPREFIX)/share/config.site  is missing. Please reconfigure depends to generate it)
endif

BUILDTOOLS=m4-native help2man-native gettext-native autoconf-native automake-native libtool-native pkg-config-native yasm-native cmake-native gas-preprocessor-native

NATIVE=python26-native zlib-native pcre-native swig-native rpl-native libsdl_image-native tiff-native libpng-native libjpeg-turbo-native liblzo2-native libsdl-native 


ifeq ($(OS),ios)
  NATIVE += dpkg-native
endif

.PHONY: $(NATIVE) native $(BUILDTOOLS) buildtools

all: native
	@echo "Dependencies built successfully."

# Dependency layout for parallel builds
autoconf-native: m4-native
help2man-native: autoconf-native gettext-native
automake-native: autoconf-native
libtool-native: automake-native
gettext-native: automake-native
pkg-config-native: automake-native
yasm-native: automake-native


libpng-native: zlib-native
tiff-native: libjpeg-turbo-native
swig-native: pcre-native

buildtools: $(BUILDTOOLS)
$(BUILDTOOLS):
	$(MAKE) -C $@

native: $(NATIVE)
$(NATIVE): buildtools
	$(MAKE) -C $@
clean:
	for d in $(BUILDTOOLS); do $(MAKE) -C $$d clean; done
	for d in $(NATIVE); do $(MAKE) -C $$d clean; done

distclean::
	for d in $(BUILDTOOLS); do $(MAKE) -C $$d distclean; done
	for d in $(NATIVE); do $(MAKE) -C $$d distclean; done

