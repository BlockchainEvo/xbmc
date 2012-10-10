include ../Makefile.include
DEPS= ../Makefile.include Makefile

# lib name, version
LIBNAME=glib
VERSION=2.26
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).1.tar.gz
BASE_URL=http://ftp.gnome.org/pub/gnome/sources/glib/$(VERSION)

# configuration settings
CONFIGURE=cp -f $(CONFIG_SUB) $(CONFIG_GUESS) .; \
          ./configure --prefix=$(PREFIX) --host=$(HOST) --disable-shared --disable-gtk-doc-html \
          glib_cv_stack_grows=no glib_cv_uscore=no ac_cv_func_posix_getpwuid_r=no \
          ac_cv_func_posix_getgrgid_r=no ac_cv_header_sys_prctl_h=no \

LIBDYLIB=$(PLATFORM)/glib/.libs/libglib-2.0.a

CLEAN_FILES=$(ARCHIVE) $(PLATFORM)

all: .installed-$(PLATFORM)

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(PLATFORM): $(TARBALLS_LOCATION)/$(ARCHIVE) $(DEPS)
	rm -rf $(PLATFORM)/*; mkdir -p $(PLATFORM)
	$(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM); patch -p0 < ../android.patch
	cd $(PLATFORM); $(CONFIGURE)
	sed -i -e s/\-lpthreads// $(PLATFORM)/gthread/Makefile

$(LIBDYLIB): $(PLATFORM)
	$(MAKE) -C $(PLATFORM)

.installed-$(PLATFORM): $(LIBDYLIB)
	$(MAKE) -C $(PLATFORM) install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM) clean
	rm -f .installed-$(PLATFORM)

distclean::
	rm -rf $(PLATFORM) .installed-$(PLATFORM)

