#!/bin/bash
#
# Functions for building MinGW port in Docker
#
# This file is part of the shadowsocks-libev.
#
# shadowsocks-libev is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# shadowsocks-libev is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with shadowsocks-libev; see the file COPYING. If not, see
# <http://www.gnu.org/licenses/>.
#

# Exit on error
set -e

. /prepare.sh

build_proj() {
    arch=$1
    host=$arch-w64-mingw32
    prefix=${DIST}/$arch
    dep=${PREFIX}/$arch

    cd "$SRC"
    git clone ${PROJ_URL} proj
    cd proj
    git checkout ${PROJ_REV}
    git submodule update --init
    ./autogen.sh
    ./configure --host=${host} --prefix=${prefix} \
      --disable-documentation \
      --with-ev="$dep" \
      --with-mbedtls="$dep" \
      --with-sodium="$dep" \
      --with-pcre="$dep" \
      --with-cares="$dep" \
      CFLAGS="-DCARES_STATICLIB -DPCRE_STATIC"
    make clean
    make LDFLAGS="-all-static -L${dep}/lib"
    make install-strip
}

dk_build() {
    for arch in i686 x86_64; do
        build_proj $arch
    done
}

dk_package() {
    rm -rf "$BASE/pack"
    mkdir -p "$BASE/pack"
    cd "$BASE/pack"
    mkdir -p ss-libev-${PROJ_REV}
    cd ss-libev-${PROJ_REV}
    for bin in local server tunnel; do
        cp ${DIST}/i686/bin/ss-${bin}.exe ss-${bin}-x86.exe
        cp ${DIST}/x86_64/bin/ss-${bin}.exe ss-${bin}-x64.exe
    done
    pushd "$SRC/proj"
    GIT_REV="$(git rev-parse --short HEAD)"
    popd
    echo "SHA1 checksum for build $(date +"%y%m%d")-${GIT_REV}" > checksum
    for f in *.exe; do
        echo "  $f:" >> checksum
        echo "    $(sha1sum $f | cut -d ' ' -f 1)" >> checksum
    done
    sed -e 's/$/\r/' checksum > checksum.txt
    rm -f checksum
    cd ..
    tar zcf /bin.tgz ss-libev-${PROJ_REV}
}
