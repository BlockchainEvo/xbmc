include ../../Makefile.include
include ../intel-sdk.mk

# lib name, version
LIBNAME=libblueray
VERSION=20110213.20739ed
SOURCE=$(LIBNAME)-$(VERSION)
ARCHIVE=$(SOURCE).tgz

# configuration settings
CONFIGURE=./configure --prefix=$(PREFIX) --exec-prefix=$(PREFIX) --host=$(HOST)

LIBDYLIB=$(SOURCE)/.libs/$(LIBNAME).so

CLEAN_FILES=$(ARCHIVE) $(SOURCE)

all: $(LIBDYLIB) .installed

$(TARBALLS_LOCATION)/$(ARCHIVE):
	$(RETRIEVE_TOOL) $(RETRIEVE_TOOL_FLAGS) $(BASE_URL)/$(ARCHIVE)

$(SOURCE): $(TARBALLS_LOCATION)/$(ARCHIVE)
	-rm -rf $(SOURCE)
	mkdir $(SOURCE)
	cd $(SOURCE); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	echo $(SOURCE) > .gitignore
	#cd $(SOURCE); ./bootstrap
	cd $(SOURCE); $(CONFIGURE)

$(LIBDYLIB): $(SOURCE)
	make -C $(SOURCE)

.installed:
	make -C $(SOURCE) install
	touch $@

clean:
	make -C $(SOURCE) clean
	rm -f .installed

distclean::
	rm -rf $(SOURCE) .installed

