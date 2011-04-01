#!/bin/bash

BUILDROOT=${INTELCE_BUILDROOT:-$HOME/IntelCE-20.0.11052.243197}
XBMCPREFIX=${INTELCE_XBMCPREFIX:-/opt/xbmc-ce41xx}

sudo mkdir -p $XBMCPREFIX
sudo chmod 777 $XBMCPREFIX
mkdir -p $XBMCPREFIX/local/lib
mkdir -p $XBMCPREFIX/local/include
ln -sf $BUILDROOT/build_i686/staging_dir $XBMCPREFIX/IntelCE
ln -sf $BUILDROOT/build_i686/staging_dir/bin $XBMCPREFIX/toolchains
ln -sf $BUILDROOT/project_build_i686/IntelCE/root $XBMCPREFIX/targetfs
#  hide config.h in Intel's SDK
mv $XBMCPREFIX/IntelCE/include/config.h $XBMCPREFIX/IntelCE/include/config.h.orig

echo "XBMCPREFIX=$XBMCPREFIX"                                          >  Makefile.include
echo "BASE_URL=http://mirrors.xbmc.org/build-deps/darwin-libs"         >> Makefile.include
echo "TARBALLS_LOCATION=$XBMCPREFIX/tarballs"                          >> Makefile.include
echo "RETRIEVE_TOOL=/usr/bin/curl"                                     >> Makefile.include
echo "RETRIEVE_TOOL_FLAGS=-Ls --create-dirs --output \$(TARBALLS_LOCATION)/\$(ARCHIVE)" >> Makefile.include
echo "ARCHIVE_TOOL=/bin/tar"                                           >> Makefile.include
echo "ARCHIVE_TOOL_FLAGS=xf"                                           >> Makefile.include
