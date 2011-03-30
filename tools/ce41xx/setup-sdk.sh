sudo mkdir -p /opt/xbmc-ce41xx
sudo chmod 777 /opt/xbmc-ce41xx
mkdir -p /opt/xbmc-ce41xx/local/lib
mkdir -p /opt/xbmc-ce41xx/local/include
ln -sf $HOME/IntelCE-20.0.11052.243197/build_i686/staging_dir /opt/xbmc-ce41xx/IntelCE
ln -sf $HOME/IntelCE-20.0.11052.243197/build_i686/staging_dir/bin /opt/xbmc-ce41xx/toolchains
ln -sf $HOME/IntelCE-20.0.11052.243197/project_build_i686/IntelCE/root /opt/xbmc-ce41xx/targetfs
#  hide config.h in Intel's SDK
mv /opt/xbmc-ce41xx/IntelCE/include/config.h /opt/xbmc-ce41xx/IntelCE/include/config.h.orig

