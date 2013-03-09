include Makefile.include

NATIVE=native/.installed-$(NATIVEPLATFORM)
DEPENDS=depends/.installed-$(PLATFORM)
all: $(DEPENDS)

$(NATIVE):
	$(MAKE) -C native

$(DEPENDS): $(NATIVE)
	$(MAKE) -C depends

clean:
	$(MAKE) -C native clean
	$(MAKE) -C depends clean

test-dependencies:
	$(MAKE) -C native test-dependencies
	$(MAKE) -C native
	$(MAKE) -C depends test-dependencies

distclean::
	$(MAKE) -C native distclean
	$(MAKE) -C depends distclean

