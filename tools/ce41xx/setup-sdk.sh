#!/bin/bash

SCRIPT_PATH=$(cd `dirname $0` && pwd)

BUILDROOT=${INTELCE_BUILDROOT:-$HOME/IntelCE-20.0.11052.243197}
XBMCSTAGE=${INTELCE_XBMCPREFIX:-/opt/xbmc-ce41xx}
#
sudo mkdir -p $XBMCSTAGE
sudo chmod 777 $XBMCSTAGE 
mkdir -p $XBMCSTAGE/usr/local/lib
mkdir -p $XBMCSTAGE/usr/local/include
#
rm -f $XBMCSTAGE/IntelCE
rm -f $XBMCSTAGE/toolchains
rm -f $XBMCSTAGE/targetfs
ln -s $BUILDROOT/build_i686/staging_dir $XBMCSTAGE/IntelCE
ln -s $BUILDROOT/build_i686/staging_dir/bin $XBMCSTAGE/toolchains
ln -s $BUILDROOT/project_build_i686/IntelCE/root $XBMCSTAGE/targetfs
#  hide config.h in Intel's SDK
if [ -f "$XBMCSTAGE/IntelCE/include/config.h" ] ; then
  mv $XBMCSTAGE/IntelCE/include/config.h $XBMCSTAGE/IntelCE/include/config.h.orig
fi
#
echo "XBMCSTAGE=$XBMCSTAGE"                                            >  $SCRIPT_PATH/Makefile.include
echo "BASE_URL=http://mirrors.xbmc.org/build-deps/darwin-libs"         >> $SCRIPT_PATH/Makefile.include
echo "TARBALLS_LOCATION=$XBMCSTAGE/tarballs"                           >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL=/usr/bin/curl"                                     >> $SCRIPT_PATH/Makefile.include
echo "RETRIEVE_TOOL_FLAGS=-Ls --create-dirs --output \$(TARBALLS_LOCATION)/\$(ARCHIVE)" >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL=/bin/tar"                                           >> $SCRIPT_PATH/Makefile.include
echo "ARCHIVE_TOOL_FLAGS=xf"                                           >> $SCRIPT_PATH/Makefile.include
echo "JOBS=$((`grep -c processor /proc/cpuinfo` -1))"                  >> $SCRIPT_PATH/Makefile.include
