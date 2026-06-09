#!/usr/bin/env bash
set -eu

ln -sf /proc/self/mounts /etc/mtab 2>/dev/null || true

curl -fsSL -A "dkp-apt" https://apt.devkitpro.org/install-devkitpro-pacman -o install-devkitpro-pacman
chmod +x install-devkitpro-pacman
./install-devkitpro-pacman
rm -f install-devkitpro-pacman

dkp-pacman -Sy --noconfirm \
	wii-dev \
	ppc-libpng \
	ppc-zlib \
	ppc-libvorbis \
	ppc-libvorbisidec \
	ppc-libogg

rm -rf /tmp/libwupc /tmp/portlibs-ppc-libvpx
git clone --depth 1 https://github.com/SumolX/libwupc.git /tmp/libwupc
cp -a /tmp/libwupc/include/wupc /opt/devkitpro/portlibs/ppc/include/
cp -a /tmp/libwupc/lib/* /opt/devkitpro/portlibs/ppc/lib/

git clone --depth 1 https://github.com/SumolX/portlibs-ppc-libvpx.git /tmp/portlibs-ppc-libvpx
cp -a /tmp/portlibs-ppc-libvpx/include/* /opt/devkitpro/portlibs/ppc/include/
cp -a /tmp/portlibs-ppc-libvpx/lib/* /opt/devkitpro/portlibs/ppc/lib/
