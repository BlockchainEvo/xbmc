#!/bin/bash

SCRIPT_PATH=$(cd `dirname $0` && pwd)

BUILDROOT=${INTELCE_BUILDROOT:-$HOME/IntelCE-20.0.11052.243197}
XBMCPREFIX=${INTELCE_XBMCPREFIX:-/opt}
#
sudo mkdir -p $XBMCPREFIX
sudo chmod 777 $XBMCPREFIX
mkdir -p $XBMCPREFIX/local/lib
mkdir -p $XBMCPREFIX/local/include
#
rm -f $XBMCPREFIX/IntelCE
rm -f $XBMCPREFIX/toolchains
rm -f $XBMCPREFIX/targetfs
ln -s $BUILDROOT/build_i686/staging_dir $XBMCPREFIX/IntelCE
ln -s $BUILDROOT/build_i686/staging_dir/bin $XBMCPREFIX/toolchains
ln -s $BUILDROOT/project_build_i686/IntelCE/root $XBMCPREFIX/targetfs
#  hide config.h in Intel's SDK
if [ -f "$XBMCPREFIX/IntelCE/include/config.h" ] ; then
  mv $XBMCPREFIX/IntelCE/include/config.h $XBMCPREFIX/IntelCE/include/config.h.orig
fi
#
echo "XBMCPREFIX=$XBMCPREFIX"                                          >  $SCRIPT_PATH/Makefile.include
echo "BASE_URL=http://mirrors.xbmc.org/build-deps/darwin-libs"         >> $SCRIPT_PATH/Makefile.include
echo "TARBALLS_LOCATION=$XBMCPREFIX/tarballs"                          >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL=/usr/bin/curl"                                     >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL_FLAGS=-Ls --create-dirs --output \$(TARBALLS_LOCATION)/\$(ARCHIVE)" >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL=/bin/tar"                                           >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL_FLAGS=xf"                                           >> $SCRIPT_PATH/Makefile.include
