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

distclean::
	$(MAKE) -C native distclean
	$(MAKE) -C depends distclean

