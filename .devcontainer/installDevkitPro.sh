#!/usr/bin/env bash
set -euo pipefail

curl -fsSL https://apt.devkitpro.org/install-devkitpro-pacman -o install-devkitpro-pacman
chmod +x install-devkitpro-pacman
yes | ./install-devkitpro-pacman
rm -f install-devkitpro-pacman

if [ ! -e /etc/mtab ]; then
	ln -sf /proc/self/mounts /etc/mtab
fi

/opt/devkitpro/pacman/bin/pacman -Sy --noconfirm \
	wii-dev \
	ppc-libpng \
	ppc-zlib \
	ppc-libvorbis \
	ppc-libvorbisidec \
	ppc-libogg

rm -rf libwupc portlibs-ppc-libvpx
git clone --depth 1 https://github.com/SumolX/libwupc.git
cp -a libwupc/include/wupc /opt/devkitpro/portlibs/ppc/include/
cp -a libwupc/lib/* /opt/devkitpro/portlibs/ppc/lib/

git clone --depth 1 https://github.com/SumolX/portlibs-ppc-libvpx.git
cp -a portlibs-ppc-libvpx/include/* /opt/devkitpro/portlibs/ppc/include/
cp -a portlibs-ppc-libvpx/lib/* /opt/devkitpro/portlibs/ppc/lib/
