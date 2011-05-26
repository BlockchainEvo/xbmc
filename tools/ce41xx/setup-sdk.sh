#!/bin/bash

SCRIPT_PATH=$(cd `dirname $0` && pwd)

#Edit these two
BUILDROOT=${INTELCE_BUILDROOT:-$HOME/IntelCE-21.0.11124.264601}
TARBALLS=/opt/xbmc-tarballs

XBMCPREFIX=${INTELCE_XBMCPREFIX:-/opt/xbmc-cex}
SDKSTAGE=$BUILDROOT/build_i686/staging_dir
TARGETFS=$BUILDROOT/project_build_i686/IntelCE/root
TOOLCHAIN=$BUILDROOT/build_i686/staging_dir/bin
#
sudo mkdir -p $XBMCPREFIX
sudo chmod 777 $XBMCPREFIX
mkdir -p $XBMCPREFIX/lib
mkdir -p $XBMCPREFIX/include
#
#  hide config.h in Intel's SDK
if [ -f "$SDKSTAGE/IntelCE/include/config.h" ] ; then
  mv $SDKSTAGE/include/config.h $SDKSTAGE/include/config.h.orig
fi
#
echo "SDKSTAGE=$SDKSTAGE"                                              >  $SCRIPT_PATH/Makefile.include
echo "XBMCPREFIX=$XBMCPREFIX"                                          >> $SCRIPT_PATH/Makefile.include
echo "TARGETFS=$TARGETFS"                                              >> $SCRIPT_PATH/Makefile.include
echo "TOOLCHAIN=$TOOLCHAIN"                                            >> $SCRIPT_PATH/Makefile.include
echo "BASE_URL=http://mirrors.xbmc.org/build-deps/darwin-libs"         >> $SCRIPT_PATH/Makefile.include
echo "TARBALLS_LOCATION=$TARBALLS"                                     >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL=/usr/bin/curl"                                     >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL_FLAGS=-Ls --create-dirs --output \$(TARBALLS_LOCATION)/\$(ARCHIVE)" >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL=/bin/tar"                                           >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL_FLAGS=xf"                                           >> $SCRIPT_PATH/Makefile.include
echo "JOBS=$((`grep -c processor /proc/cpuinfo` -1))"                  >> $SCRIPT_PATH/Makefile.include
