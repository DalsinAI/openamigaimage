#!/bin/sh
# openamigaimage: zlib, libpng and libjpeg, built for AmigaOS 3.x (68020 + FPU) with the
# os32-gcc16 compiler (bebbo's amiga-gcc, GCC 16.2, libnix, libpthread).
# MIT, Copyright (c) 2026 Dalsin Limited. The library keeps its own licence.
#
#   OS32_GCC16   compiler root holding prefix/ and compat/
#                (default ~/AmigaChrome/stoves/os32-gcc16)
#   PREFIX       where include/ and lib/ go (default ./out)
#   TARBALLS     folder holding the upstream tarballs listed in SOURCES
#                (default ./tarballs); the script checks their SHA-256
#   JOBS         parallel jobs for CMake/make builds (default 2)
#   AC_HELPERS   1: zlib's checksums and the big copies and fills in zlib,
#                libpng and libjpeg go through AC090's native helpers
#                (achelpers/, see README); 0: the libraries as before;
#                auto (default): 1 when the pinned amigachrome-guest commit
#                is to hand, else 0, saying so
#   AMIGACHROME_GUEST  checkout of amigachrome-guest holding the commit in
#                AMIGACHROME_GUEST_PINNED_COMMIT, for AC_HELPERS=1
#                (default ../amigachrome-guest, else ../guest)
#
# usage: ./build.sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
S=${OS32_GCC16:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
P=$S/prefix
OUT=${PREFIX:-"$HERE/out"}
TARBALLS=${TARBALLS:-"$HERE/tarballs"}
JOBS=${JOBS:-2}
WORK="$HERE/work"
CC="$P/bin/m68k-amigaos-gcc"
CXX="$P/bin/m68k-amigaos-g++"
AR="$P/bin/m68k-amigaos-ar"
CPU=${OS32_CPU_FLAGS:-"-m68020 -m68881 -mcrt=nix20"}
# Address 0 is memory on an Amiga (exec's pointer is at 4): without
# -fno-delete-null-pointer-checks GCC drops null checks after a pointer is
# used and puts a trap (TRAP #7, Software Failure 80000027) on any path it
# proves reads or writes through a null pointer. Every compile here (C and
# C++, the helpers and zlib's checksums too) takes it.
CFLAGS="-O2 $CPU -fno-delete-null-pointer-checks -D_DEFAULT_SOURCE=1 -D_POSIX_TIMERS=1 -D_POSIX_REALTIME_SIGNALS=1 -fno-common"
AC_HELPERS=${AC_HELPERS:-auto}
case "$AC_HELPERS" in 0|1|auto) ;; *) echo "AC_HELPERS must be 0, 1 or auto, not $AC_HELPERS"; exit 2 ;; esac
if [ "$AC_HELPERS" = auto ]; then          # on when the pinned amigachrome-guest commit is to hand
    if sh "$HERE/achelpers/achelpers.sh" --have; then AC_HELPERS=1
    else AC_HELPERS=0; echo "AC090 native helpers: off (no amigachrome-guest checkout with $(cat "$HERE/AMIGACHROME_GUEST_PINNED_COMMIT" | cut -c1-12); set AMIGACHROME_GUEST)"; fi
fi
mkdir -p "$OUT/include" "$OUT/lib" "$WORK"

# AC090's native helpers, from amigachrome-guest at the pinned commit:
# ACFLAGS forces achelpers/ac_string.h into the three libraries' compiles,
# and ACOBJS go into each archive, so programs link with them as before.
ACFLAGS= ACOBJS=
if [ "$AC_HELPERS" = 1 ]; then
    CC="$CC" AR="$AR" CFLAGS="$CFLAGS" sh "$HERE/achelpers/achelpers.sh" "$WORK/achelpers"
    ACFLAGS="-I$WORK/achelpers/src -include $HERE/achelpers/ac_string.h"
    ACOBJS=$(echo "$WORK"/achelpers/obj/ac_*.o)
fi

# unpack NAME TARBALL SHA256: check the tarball and unpack it into $WORK
unpack() {
    t="$TARBALLS/$2"
    [ -f "$t" ] || { echo "missing $t (see SOURCES)"; exit 2; }
    echo "$3  $t" | sha256sum -c - >/dev/null || { echo "SHA-256 mismatch: $t"; exit 2; }
    rm -rf "$WORK/$1"; mkdir -p "$WORK/$1"
    case "$2" in
        *.zip) (cd "$WORK/$1" && unzip -q "$t") ;;
        *) tar xf "$t" -C "$WORK/$1" ;;
    esac
}

# archive NAME FILE...: compile into $OUT/lib/libNAME.a ($XFLAGS added),
# with the objects in $EXTRA_OBJS
archive() {
    name=$1; shift
    obj="$WORK/obj-$name"
    rm -rf "$obj"; mkdir -p "$obj"
    for f in "$@"; do
        o="$obj/$(echo "$f" | tr '/' '_' | sed 's/\.[a-z]*$//').o"
        case "$f" in
            *.cc|*.cpp) $CXX $CFLAGS ${XFLAGS:-} -c "$f" -o "$o" ;;
            *) $CC $CFLAGS ${XFLAGS:-} -c "$f" -o "$o" ;;
        esac
    done
    rm -f "$OUT/lib/lib$name.a"
    # shellcheck disable=SC2086
    $AR rcs "$OUT/lib/lib$name.a" "$obj"/*.o ${EXTRA_OBJS:-}
    echo "lib$name.a: $(wc -c < "$OUT/lib/lib$name.a") bytes"
}

unpack zlib zlib.tar.gz 9298ad1c8498a60cd10f97b5724955fcac4f0343ecdc1ec80ceecfe43cd92cc5
cd "$WORK/zlib"
# Chromium's zlib 1.3.1, without its symbol renaming (chromeconf.h).
ZFLAGS="-DCHROMIUM_ZLIB_NO_CHROMECONF -I."
ZSRC="compress.c cpu_features.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c
    inftrees.c trees.c uncompr.c zutil.c"
if [ "$AC_HELPERS" = 1 ]; then
    # zlib's own crc32, crc32_z, adler32 and adler32_z under other names, and
    # achelpers/ac_zlib.c in front of them (see achelpers/zlibsums.sh).
    rm -rf "$WORK/obj-zsum"
    CC="$CC" CFLAGS="$CFLAGS $ZFLAGS $ACFLAGS" sh "$HERE/achelpers/zlibsums.sh" "$WORK/obj-zsum"
    # shellcheck disable=SC2086
    EXTRA_OBJS="$(echo "$WORK"/obj-zsum/*.o) $ACOBJS" XFLAGS="$ZFLAGS $ACFLAGS" archive z $ZSRC
else
    # shellcheck disable=SC2086
    EXTRA_OBJS= XFLAGS="$ZFLAGS" archive z adler32.c crc32.c $ZSRC
fi
sed 's/^#if !defined(CHROMIUM_ZLIB_NO_CHROMECONF)/#if 0 \/* no Chromium symbol renaming *\//' zconf.h > "$OUT/include/zconf.h"
cp zlib.h "$OUT/include/"

unpack libpng libpng-1.6.58.tar.gz 8c9b05b675ca7301a458df2c2e46f26e1d41ff36b8863f8c33530bc58c2e6225
cd "$WORK/libpng/libpng-1.6.58"
cp scripts/pnglibconf.h.prebuilt pnglibconf.h
EXTRA_OBJS="$ACOBJS" XFLAGS="-I. -I$OUT/include -DPNG_ARM_NEON_OPT=0 -DPNG_POWERPC_VSX_OPT=0 $ACFLAGS" archive png png.c pngerror.c pngget.c \
    pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c \
    pngwtran.c pngwutil.c
cp png.h pngconf.h pnglibconf.h "$OUT/include/"

unpack libjpeg jpegsrc.v9f.tar.gz 04705c110cb2469caa79fb71fba3d7bf834914706e9641a4589485c1f832565b
cd "$WORK/libjpeg/jpeg-9f"
cp jconfig.txt jconfig.h
EXTRA_OBJS="$ACOBJS" XFLAGS="-I. $ACFLAGS" archive jpeg jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c \
    jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcprepct.c jcsample.c jctrans.c \
    jdapimin.c jdapistd.c jdarith.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c \
    jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c \
    jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c jquant1.c jquant2.c jutils.c \
    jmemmgr.c jmemnobs.c
cp jpeglib.h jconfig.h jmorecfg.h jerror.h "$OUT/include/"
