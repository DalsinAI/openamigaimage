#!/bin/sh
# openpng.datatype and its descriptor, DEVS:DataTypes/OpenPNG, for AmigaOS 3.x
# (pngclass.c). Our own code only: no upstream tarballs. Built with the
# os32-gcc16 compiler, as the other datatypes (../build.sh runs this too).
# MIT, Copyright (c) 2026 Dalsin Limited.
#
#   OS32_GCC16   compiler root holding prefix/ (default ~/AmigaChrome/stoves/os32-gcc16)
#   PREFIX       where Classes/ and Devs/ go (default ../out)
#
# usage: sh png/build.sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
S=${OS32_GCC16:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
CC="$S/prefix/bin/m68k-amigaos-gcc"
OUT=${PREFIX:-"$HERE/out"}
WORK="$HERE/work/obj-openpng"
# Address 0 is memory on an Amiga: keep GCC from dropping null checks (../build.sh).
CFLAGS="-O2 -m68020 -fomit-frame-pointer -fno-delete-null-pointer-checks -DNDEBUG -Wall -Wno-pointer-sign"
rm -rf "$WORK"
mkdir -p "$WORK" "$OUT/Classes/DataTypes" "$OUT/Devs/DataTypes"
# include/ holds openmulticore's headers (copied in), for the colour stage's bands on the cores.
for f in common/dtstart.c common/dtlib.c png/pngdecode.c png/pngclass.c; do
    (cd "$HERE" && $CC $CFLAGS -I"$HERE/common" -I"$HERE/png" -I"$HERE/include" -c "$f" -o "$WORK/$(basename "$f" .c).o")
done
# dtstart.o first, so the library's first code is a safe return.
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openpng.datatype" \
    "$WORK/dtstart.o" "$WORK/dtlib.o" "$WORK/pngdecode.o" "$WORK/pngclass.o" -lamiga \
    -Wl,-Map="$WORK/openpng.datatype.map"
echo "openpng.datatype: $(wc -c < "$OUT/Classes/DataTypes/openpng.datatype") bytes"
# The signature and the IHDR chunk that must come next (length 13): sixteen
# bytes, longer than the system's PNG descriptor, so it is tried first.
python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/OpenPNG" OpenPNG openpng pict png "#?" \
    '$VER: OpenPNG 47.4 (11.10.2026)' 0x89 "'P'" "'N'" "'G'" 0x0d 0x0a 0x1a 0x0a 0x00 0x00 0x00 0x0d \
    "'I'" "'H'" "'D'" "'R'"
echo "Devs/DataTypes/OpenPNG: $(wc -c < "$OUT/Devs/DataTypes/OpenPNG") bytes"
# PNGBench (png/pngbench.c): the decoder timed in its stages, the colours on
# this CPU and on the cores; a test program, not installed.
$CC $CFLAGS -noixemul -I"$HERE/png" -I"$HERE/include" -o "$WORK/PNGBench" "$HERE/png/pngbench.c" "$HERE/png/pngdecode.c"
echo "PNGBench: $(wc -c < "$WORK/PNGBench") bytes (in $WORK)"
