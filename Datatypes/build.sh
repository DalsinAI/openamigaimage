#!/bin/sh
# openamigaimage datatypes: webp, webm, openpicture, opensound, openmodule, opendoc and openvideo.datatype
# for AmigaOS 3.x, built with the os32-gcc16 compiler (bebbo's amiga-gcc, GCC 16.2,
# libnix). The datatypes run on any 68020 or better, with or without an FPU.
# MIT, Copyright (c) 2026 Dalsin Limited. libwebp and libvpx keep their
# BSD licences, libxmp its MIT licence.
#
#   OS32_GCC16   compiler root holding prefix/ (default ~/AmigaChrome/stoves/os32-gcc16)
#   PREFIX       where Classes/ and Devs/ go (default ./out)
#   TARBALLS     folder holding the upstream tarballs listed in ../SOURCES
#                (default ./tarballs, else ../tarballs); the script checks
#                their SHA-256
#   AC_HELPERS   1: big copies and fills in common/dtlib.c go through
#                AC090's native helpers (../achelpers/, see ../README.md);
#                0: as before; auto (default): 1 when the pinned
#                amigachrome-guest commit is to hand, else 0, saying so
#   AMIGACHROME_GUEST  checkout of amigachrome-guest holding the commit in
#                ../AMIGACHROME_GUEST_PINNED_COMMIT, for AC_HELPERS=1
#                (default amigachrome-guest, else guest, beside this
#                repository)
#
# usage: ./build.sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
S=${OS32_GCC16:-"$HOME/AmigaChrome/stoves/os32-gcc16"}
P=$S/prefix
OUT=${PREFIX:-"$HERE/out"}
# The tarballs sit in the repository's tarballs/ folder, or in one here.
if [ -z "${TARBALLS:-}" ]; then
    if [ -d "$HERE/tarballs" ]; then TARBALLS="$HERE/tarballs"; else TARBALLS="$HERE/../tarballs"; fi
fi
WORK="$HERE/work"
CC="$P/bin/m68k-amigaos-gcc"
AR="$P/bin/m68k-amigaos-ar"
# No -m68881: a datatype must load on a 68020 without an FPU too.
CFLAGS="-O2 -m68020 -fomit-frame-pointer -DNDEBUG -DWORDS_BIGENDIAN -Wall -Wno-pointer-sign"
mkdir -p "$OUT/Classes/DataTypes" "$OUT/Devs/DataTypes" "$WORK"

# AC090's native helpers for common/dtlib.c (realloc's copy, calloc's
# clearing), built from amigachrome-guest at the pinned commit; ACLIB goes
# on each datatype's link.
AC_HELPERS=${AC_HELPERS:-auto}
case "$AC_HELPERS" in 0|1|auto) ;; *) echo "AC_HELPERS must be 0, 1 or auto, not $AC_HELPERS"; exit 2 ;; esac
if [ "$AC_HELPERS" = auto ]; then          # on when the pinned amigachrome-guest commit is to hand
    if sh "$HERE/../achelpers/achelpers.sh" --have; then AC_HELPERS=1
    else AC_HELPERS=0; echo "AC090 native helpers: off (no amigachrome-guest checkout with $(cat "$HERE/../AMIGACHROME_GUEST_PINNED_COMMIT" | cut -c1-12); set AMIGACHROME_GUEST)"; fi
fi
ACFLAGS= ACLIB=
if [ "$AC_HELPERS" = 1 ]; then
    CC="$CC" AR="$AR" CFLAGS="$CFLAGS" sh "$HERE/../achelpers/achelpers.sh" "$WORK/achelpers"
    ACFLAGS="-DOAI_AC_HELPERS -I$WORK/achelpers/src"
    ACLIB="$WORK/achelpers/libachelpers.a"
fi

unpack() {
    t="$TARBALLS/$2"
    [ -f "$t" ] || { echo "missing $t (see SOURCES)"; exit 2; }
    echo "$3  $t" | sha256sum -c - >/dev/null || { echo "SHA-256 mismatch: $t"; exit 2; }
    rm -rf "$WORK/$1"; mkdir -p "$WORK/$1"
    tar xf "$t" -C "$WORK/$1"
}

# compile DIR OBJDIR FILE...: compile C files into OBJDIR ($XFLAGS added)
compile() {
    dir=$1; obj=$2; shift 2
    rm -rf "$obj"; mkdir -p "$obj"
    for f in "$@"; do
        o="$obj/$(echo "$f" | tr '/' '_' | sed 's/\.c$//').o"
        (cd "$dir" && $CC $CFLAGS ${XFLAGS:-} -c "$f" -o "$o")
    done
}

# --- webp.datatype ------------------------------------------------------------
unpack libwebp libwebp-1.6.0.tar.gz e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
W="$WORK/libwebp/libwebp-1.6.0"
# No floating point in the decoder (see the patch).
(cd "$W" && patch -p1 -s < "$HERE/patches/libwebp-1.6.0-no-float.patch")
# The decoder, the demuxer (animated and extended files) and the shared code.
# The SSE, NEON and MIPS files compile to nothing on 68k and are left out.
XFLAGS="-I$W -I$W/src" compile "$W" "$WORK/obj-libwebp" \
    src/dec/alpha_dec.c src/dec/buffer_dec.c src/dec/frame_dec.c src/dec/idec_dec.c src/dec/io_dec.c \
    src/dec/quant_dec.c src/dec/tree_dec.c src/dec/vp8_dec.c src/dec/vp8l_dec.c src/dec/webp_dec.c \
    src/demux/demux.c \
    src/dsp/alpha_processing.c src/dsp/cpu.c src/dsp/dec.c src/dsp/dec_clip_tables.c src/dsp/filters.c \
    src/dsp/lossless.c src/dsp/rescaler.c src/dsp/upsampling.c src/dsp/yuv.c \
    src/utils/bit_reader_utils.c src/utils/color_cache_utils.c src/utils/filters_utils.c \
    src/utils/huffman_utils.c src/utils/palette.c src/utils/quant_levels_dec_utils.c \
    src/utils/random_utils.c src/utils/rescaler_utils.c src/utils/thread_utils.c src/utils/utils.c
# dtstart.c is linked first, so the library's first code is a safe return.
XFLAGS="-I$HERE/common -I$W/src $ACFLAGS" compile "$HERE" "$WORK/obj-webp" common/dtstart.c common/dtlib.c webp/webpclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/webp.datatype" \
    "$WORK"/obj-webp/common_dtstart.o "$WORK"/obj-webp/common_dtlib.o "$WORK"/obj-webp/webp_webpclass.o "$WORK"/obj-libwebp/*.o $ACLIB -lamiga \
    -Wl,-Map="$WORK/webp.datatype.map"
echo "webp.datatype: $(wc -c < "$OUT/Classes/DataTypes/webp.datatype") bytes"
# RIFF, any length, WEBP.
python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/WebP" WebP webp pict webp "#?" \
    '$VER: WebP 47.1 (4.10.2026)' "'R'" "'I'" "'F'" "'F'" ANY ANY ANY ANY "'W'" "'E'" "'B'" "'P'"
echo "Devs/DataTypes/WebP: $(wc -c < "$OUT/Devs/DataTypes/WebP") bytes"

# --- webm.datatype ------------------------------------------------------------
unpack libvpx libvpx-1.17.0.tar.gz 1020f184046187baa2985dbde38e0691f49c44088bca7a1842b0236c6081dc0a
V="$WORK/libvpx/libvpx-1.17.0"
# Amiga objects align to at most 8 bytes (see the patch).
(cd "$V" && patch -p1 -s < "$HERE/patches/libvpx-1.17.0-amiga-align.patch")
# The VP8 and VP9 decoders in plain C: no encoders, threads or tools.
mkdir -p "$WORK/libvpx-build"
(cd "$WORK/libvpx-build" && CROSS="$P/bin/m68k-amigaos-" CFLAGS="-O2 -m68020 -fomit-frame-pointer" \
    "$V/configure" --target=generic-gnu --disable-vp8-encoder --disable-vp9-encoder --enable-vp8-decoder \
    --enable-vp9-decoder --disable-examples --disable-tools --disable-docs --disable-unit-tests \
    --disable-multithread --disable-runtime-cpu-detect --disable-install-docs --disable-install-bins \
    --disable-webm-io --disable-libyuv --enable-static --disable-shared --disable-postproc \
    --disable-vp9-postproc --disable-internal-stats --disable-pic --size-limit=8192x8192 > configure.log 2>&1 \
    && make -j"${JOBS:-2}" libvpx.a > build.log 2>&1) || { echo "libvpx build failed (see $WORK/libvpx-build)"; exit 1; }
XFLAGS="-I$HERE/common -I$HERE/include -I$HERE/webm -I$V -I$WORK/libvpx-build $ACFLAGS" compile "$HERE" "$WORK/obj-webm" \
    common/dtstart.c common/dtlib.c common/dtstack.c common/dtservice.c webm/webm_demux.c webm/webmclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/webm.datatype" \
    "$WORK"/obj-webm/common_dtstart.o "$WORK"/obj-webm/common_dtlib.o "$WORK"/obj-webm/common_dtstack.o \
    "$WORK"/obj-webm/common_dtservice.o \
    "$WORK"/obj-webm/webm_webm_demux.o "$WORK"/obj-webm/webm_webmclass.o "$WORK/libvpx-build/libvpx.a" $ACLIB -lamiga \
    -Wl,-Map="$WORK/webm.datatype.map"
echo "webm.datatype: $(wc -c < "$OUT/Classes/DataTypes/webm.datatype") bytes"
# The EBML magic; datatypes.library then offers the file to webm.datatype,
# which checks the document type.
python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/WebM" WebM webm anim webm "#?" \
    '$VER: WebM 47.1 (4.10.2026)' 0x1a 0x45 0xdf 0xa3
echo "Devs/DataTypes/WebM: $(wc -c < "$OUT/Devs/DataTypes/WebM") bytes"

# --- openpicture.datatype -----------------------------------------------------
# AVIF, HEIC, JPEG XL, camera RAW, PSD, XCF, EXR, HDR, QOI, DDS and JPEG 2000,
# decoded by the media.decode/1 service (openamigaservice) on the services
# card or a paired Cradle; no codec on the 68k.
XFLAGS="-I$HERE/common -I$HERE/include $ACFLAGS" compile "$HERE" "$WORK/obj-openpicture" \
    common/dtstart.c common/dtlib.c common/dtservice.c openpicture/pictureclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openpicture.datatype" \
    "$WORK"/obj-openpicture/common_dtstart.o "$WORK"/obj-openpicture/common_dtlib.o \
    "$WORK"/obj-openpicture/common_dtservice.o "$WORK"/obj-openpicture/openpicture_pictureclass.o $ACLIB -lamiga \
    -Wl,-Map="$WORK/openpicture.datatype.map"
echo "openpicture.datatype: $(wc -c < "$OUT/Classes/DataTypes/openpicture.datatype") bytes"
picdesc() {      # FILE NAME ID PATTERN MASK...
    f=$1; n=$2; i=$3; pat=$4; shift 4
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" openpicture pict "$i" "$pat" \
        "\$VER: $f 47.1 (5.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
chars() {        # each byte of $1 as a mask item, in hex (spaces and all)
    printf %s "$1" | od -An -v -tx1 | sed 's/[0-9a-f][0-9a-f]/0x&/g'
}
# An ISO BMFF ftyp box (any size) with the brand: one descriptor per brand.
# shellcheck disable=SC2046
for d in "AVIF:AVIF:avif" "AVIS:AVIF sequence:avis" "HEIC:HEIC:heic" "HEIX:HEIC (heix):heix" "HEIF:HEIF:mif1"; do
    file=${d%%:*}; rest=${d#*:}; title=${rest%%:*}; brand=${rest#*:}
    picdesc "$file" "$title" "$brand" "#?" ANY ANY ANY ANY $(chars "ftyp$brand")
done
# shellcheck disable=SC2046
picdesc JXL "JPEG XL" jxl "#?" 0xff 0x0a
# shellcheck disable=SC2046
picdesc JXL-ISO "JPEG XL (container)" jxl "#?" 0 0 0 0x0c $(chars "JXL ")
picdesc EXR OpenEXR exr "#?" 0x76 0x2f 0x31 0x01
# shellcheck disable=SC2046
# Its header is text: a file whose first block reads as text needs the text
# descriptor, one with pixels early on the binary one.
DT_TEXT=1 picdesc HDR "Radiance HDR" hdr "#?" $(chars "#?RADIANCE")
# shellcheck disable=SC2046
picdesc HDR-Bin "Radiance HDR (binary)" hdr "#?" $(chars "#?RADIANCE")
# shellcheck disable=SC2046
picdesc PSD "Photoshop" psd "#?" $(chars "8BPS")
# shellcheck disable=SC2046
picdesc XCF "GIMP picture" xcf "#?" $(chars "gimp xcf")
# shellcheck disable=SC2046
picdesc QOI QOI qoi "#?" $(chars "qoif")
# shellcheck disable=SC2046
picdesc DDS "DirectDraw surface" dds "#?" $(chars "DDS ")
# shellcheck disable=SC2046
picdesc JP2 "JPEG 2000" jp2 "#?" 0 0 0 0x0c $(chars "jP  ")
picdesc J2K "JPEG 2000 codestream" j2k "#?" 0xff 0x4f 0xff 0x51
# OpenRaster and Krita: a ZIP whose first entry is the mimetype; comics by name.
# shellcheck disable=SC2046
picdesc ORA OpenRaster ora "#?" $(chars "PK") 3 4 $(i=0; while [ $i -lt 26 ]; do printf 'ANY '; i=$((i+1)); done) \
    $(chars "mimetypeimage/openraster")
# shellcheck disable=SC2046
picdesc KRA "Krita picture" kra "#?" $(chars "PK") 3 4 $(i=0; while [ $i -lt 26 ]; do printf 'ANY '; i=$((i+1)); done) \
    $(chars "mimetypeapplication/x-krita")
# shellcheck disable=SC2046
picdesc CBZ "Comic book" cbz "#?.cbz" $(chars "PK") 3 4
# SVG is drawn on the host at the picture's own size (media_svg.c); it is
# text (DT_TEXT), so the name tells it, ahead of the ascii datatype.
DT_TEXT=1 DT_PRIORITY=1 picdesc SVG "SVG drawing" svg "#?.(svg|svgz)"
# Fonts as ImageMagick's sample sheet.
picdesc TTF "TrueType font" ttf "#?.(ttf|ttc)"
# shellcheck disable=SC2046
picdesc OTF "OpenType font" otf "#?" $(chars "OTTO")
# OpenType with TrueType outlines starts as a TrueType font does.
picdesc OTF-TT "OpenType font (TrueType outlines)" otf "#?.otf" 0 1 0 0
# Camera RAW is TIFF (or its own thing) inside: the name tells it, ahead of
# the TIFF datatype.
DT_PRIORITY=1 picdesc RAW "Camera RAW" raw \
    "#?.(cr2|cr3|crw|nef|nrw|arw|srf|sr2|dng|orf|rw2|raf|pef|srw|x3f|erf|kdc|dcr|mrw|3fr|iiq|rwl)"

# --- opensound.datatype -------------------------------------------------------
# FLAC, Ogg (Vorbis, Opus), AAC/M4A, ALAC, WMA and MP3, decoded by the
# media.decode/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include $ACFLAGS" compile "$HERE" "$WORK/obj-opensound" \
    common/dtstart.c common/dtlib.c common/dtservice.c opensound/soundclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/opensound.datatype" \
    "$WORK"/obj-opensound/common_dtstart.o "$WORK"/obj-opensound/common_dtlib.o \
    "$WORK"/obj-opensound/common_dtservice.o "$WORK"/obj-opensound/opensound_soundclass.o $ACLIB -lamiga \
    -Wl,-Map="$WORK/opensound.datatype.map"
echo "opensound.datatype: $(wc -c < "$OUT/Classes/DataTypes/opensound.datatype") bytes"
sounddesc() {    # FILE NAME ID MASK...
    f=$1; n=$2; i=$3; shift 3
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" opensound soun "$i" "#?" \
        "\$VER: $f 47.1 (5.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
sounddesc FLAC FLAC flac "'f'" "'L'" "'a'" "'C'"
sounddesc Ogg "Ogg sound" ogg "'O'" "'g'" "'g'" "'S'"
sounddesc M4A "MPEG-4 audio" m4a ANY ANY ANY ANY "'f'" "'t'" "'y'" "'p'" "'M'" "'4'" "'A'" "' '"
sounddesc M4B "MPEG-4 audiobook" m4b ANY ANY ANY ANY "'f'" "'t'" "'y'" "'p'" "'M'" "'4'" "'B'" "' '"
# ASF holds both: Windows Media audio by its name here, video in openvideo.
python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/WMA" "Windows Media audio" opensound soun wma "#?.wma" \
    '$VER: WMA 47.1 (5.10.2026)' 0x30 0x26 0xb2 0x75 0x8e 0x66 0xcf 0x11
echo "Devs/DataTypes/WMA: $(wc -c < "$OUT/Devs/DataTypes/WMA") bytes"
sounddesc MP3-ID3 "MP3 (ID3)" mp3 "'I'" "'D'" "'3'"
sounddesc AAC "AAC (ADTS)" aac 0xff 0xf1
# Tunes the host plays: MIDI through FluidSynth, SID through sidplayfp.
# shellcheck disable=SC2046
sounddesc MIDI MIDI midi $(chars "MThd") 0 0 0 6
# shellcheck disable=SC2046
sounddesc PSID "C64 SID tune" psid $(chars "PSID")
# shellcheck disable=SC2046
sounddesc RSID "C64 SID tune (RSID)" rsid $(chars "RSID")
# PC tracker modules through libopenmpt, when openmodule.datatype (below,
# played on the Amiga itself, tried first) is not installed.
# shellcheck disable=SC2046
sounddesc XM "FastTracker module" xm $(chars "Extended Module: ")
# shellcheck disable=SC2046
sounddesc IT "Impulse Tracker module" it $(chars "IMPM")
# shellcheck disable=SC2046
sounddesc S3M "Scream Tracker module" s3m $(i=0; while [ $i -lt 44 ]; do printf 'ANY '; i=$((i+1)); done) $(chars "SCRM")

# --- openmodule.datatype -----------------------------------------------------
# Music modules played on the Amiga itself: libxmp mixes them a buffer at a
# time in a player process of the object's own, onto two Paula channels
# through audio.device (openmodule/DESIGN.md); a module too heavy for this
# CPU goes to a cores board core (openmulticore.library, headers copied in
# include/) or to media.decode/1 (dtservice). libxmp's loaders, all of
# them; no depackers or ProWizard (they write temporary files) and no Ogg
# Vorbis samples (openmodule/xmpglue.c).
unpack libxmp libxmp-4.7.3.tar.gz b6a98797e4fb9c9a705f5d53112aa5214561857e929a644928b9e658930d9440
X="$WORK/libxmp/libxmp-4.7.3"
XMPFLAGS="-DWORDS_BIGENDIAN -DLIBXMP_NO_DEPACKERS -DLIBXMP_NO_PROWIZARD -DLIBXMP_STATIC -I$X/include"
# shellcheck disable=SC2046
XFLAGS="$XMPFLAGS -w" compile "$X" "$WORK/obj-libxmp" \
    $(cd "$X" && ls src/*.c | grep -v -e win32.c -e mkstemp.c -e tempfile.c) \
    $(cd "$X" && ls src/loaders/*.c | grep -v -e pw_load.c -e vorbis.c)
rm -f "$WORK/libxmp.a"
"$AR" rcs "$WORK/libxmp.a" "$WORK"/obj-libxmp/*.o
# dtlib without its malloc (DT_OWN_MALLOC): openmodule's own keeps libxmp's
# memory in one arena when a cores board may mix (openmodule/xmpglue.h);
# dtservice for media.decode/1, the third rung.
XFLAGS="-I$HERE/common -I$HERE/include -I$X/include -DLIBXMP_STATIC -DDT_OWN_MALLOC $ACFLAGS" compile "$HERE" "$WORK/obj-openmodule" \
    common/dtstart.c common/dtlib.c common/dtservice.c openmodule/moduleclass.c openmodule/xmpglue.c openmodule/novorbis.c
# libm: libnix's soft floating point, which calls the ROM math libraries
# (opened in the player process; see moduleclass.c).
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openmodule.datatype" \
    "$WORK"/obj-openmodule/common_dtstart.o "$WORK"/obj-openmodule/common_dtlib.o \
    "$WORK"/obj-openmodule/common_dtservice.o "$WORK"/obj-openmodule/openmodule_moduleclass.o \
    "$WORK"/obj-openmodule/openmodule_xmpglue.o \
    "$WORK"/obj-openmodule/openmodule_novorbis.o \
    "$WORK/libxmp.a" $ACLIB -lamiga -lm \
    -Wl,-Map="$WORK/openmodule.datatype.map"
echo "openmodule.datatype: $(wc -c < "$OUT/Classes/DataTypes/openmodule.datatype") bytes"
# modbench (tests/modbench.c): libxmp's mixing time on this Amiga, a plain
# Shell program, left in work/.
$CC -O2 -m68020 -fomit-frame-pointer -I"$X/include" -DLIBXMP_STATIC -o "$WORK/modbench" \
    "$HERE/tests/modbench.c" "$WORK"/obj-openmodule/openmodule_novorbis.o "$WORK/libxmp.a" -lm
moddesc() {      # FILE NAME ID PATTERN MASK...
    f=$1; n=$2; i=$3; pat=$4; shift 4
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" openmodule soun "$i" "$pat" \
        "\$VER: $f 47.1 (8.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
anys() {         # $1 ANY mask items
    i=0; while [ "$i" -lt "$1" ]; do printf 'ANY '; i=$((i+1)); done
}
# 31-instrument MODs carry their kind at byte 1080: ProTracker (M.K., and
# M!K! past 64 patterns), StarTrekker (FLT4, FLT8), FastTracker (2CHN to
# 9CHN) and TakeTracker (10CH to 32CH). datatypes.library reads as much of
# a file as its longest mask, so these masks run to byte 1083.
# shellcheck disable=SC2046
moddesc ProTracker "ProTracker module" mod "#?" $(anys 1080) $(chars "M.K.")
# shellcheck disable=SC2046
moddesc ProTracker-100 "ProTracker module (over 64 patterns)" mod "#?" $(anys 1080) $(chars "M!K!")
# shellcheck disable=SC2046
moddesc StarTrekker "StarTrekker module" mod "#?" $(anys 1080) $(chars "FLT4")
# shellcheck disable=SC2046
moddesc StarTrekker-8 "StarTrekker module (8 channels)" mod "#?" $(anys 1080) $(chars "FLT8")
# shellcheck disable=SC2046
moddesc MOD-xCHN "FastTracker MOD" mod "#?" $(anys 1081) $(chars "CHN")
# shellcheck disable=SC2046
moddesc MOD-xxCH "TakeTracker MOD" mod "#?" $(anys 1082) $(chars "CH")
# The 15-instrument SoundTracker MOD has no signature: by its name only
# (mod.name, as on the Amiga, or name.mod), and libxmp checks the header.
# With no mask it is tried after every descriptor that has one.
moddesc SoundTracker "SoundTracker module" mod "(mod.#?|#?.mod)"
# shellcheck disable=SC2046
moddesc OctaMED "MED module" mmd "#?" $(chars "MMD") ANY
# shellcheck disable=SC2046
moddesc MED-2 "MED 2 module" med "#?" $(chars "MED") 2
# shellcheck disable=SC2046
moddesc MED-3 "MED 3 module" med "#?" $(chars "MED") 3
# shellcheck disable=SC2046
moddesc MED-4 "MED 4 module" med "#?" $(chars "MED") 4
# shellcheck disable=SC2046
moddesc Oktalyzer "Oktalyzer module" okt "#?" $(chars "OKTASONG")
# shellcheck disable=SC2046
moddesc DigiBooster "DigiBooster module" digi "#?" $(chars "DIGI Booster module")
# shellcheck disable=SC2046
moddesc DigiBooster-Pro "DigiBooster Pro module" dbm "#?" $(chars "DBM0")
# FastTracker 2, Scream Tracker 3 and Impulse Tracker: opensound.datatype
# has descriptors for these too (played by libopenmpt on the Cradle).
# AmigaOS 3.2's datatypes.library tries longer masks first and does not go
# by priority (openmodule/DESIGN.md), so these masks are one byte longer
# than opensound's (ANY: an XM, S3M or IT is never that short), and these
# play here when both are installed.
# shellcheck disable=SC2046
moddesc Module-XM "FastTracker 2 module" xm "#?" $(chars "Extended Module: ") ANY
# shellcheck disable=SC2046
moddesc Module-S3M "Scream Tracker 3 module" s3m "#?" $(anys 44) $(chars "SCRM") ANY
# shellcheck disable=SC2046
moddesc Module-IT "Impulse Tracker 2 module" it "#?" $(chars "IMPM") ANY

# --- opendoc.datatype ---------------------------------------------------------
# Office documents as pictures of their pages, laid out by LibreOffice through
# the doc.render/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include $ACFLAGS" compile "$HERE" "$WORK/obj-opendoc" \
    common/dtstart.c common/dtlib.c common/dtservice.c opendoc/docclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/opendoc.datatype" \
    "$WORK"/obj-opendoc/common_dtstart.o "$WORK"/obj-opendoc/common_dtlib.o \
    "$WORK"/obj-opendoc/common_dtservice.o "$WORK"/obj-opendoc/opendoc_docclass.o $ACLIB -lamiga \
    -Wl,-Map="$WORK/opendoc.datatype.map"
echo "opendoc.datatype: $(wc -c < "$OUT/Classes/DataTypes/opendoc.datatype") bytes"
docdesc() {      # FILE NAME ID PATTERN MASK...
    f=$1; n=$2; i=$3; pat=$4; shift 4
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" opendoc pict "$i" "$pat" \
        "\$VER: $f 47.1 (5.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
# Office Open XML files are ZIP files: the name tells them apart.
docdesc DOCX "Word document" docx "#?.(docx|docm|dotx)" "'P'" "'K'" 3 4
docdesc XLSX "Excel workbook" xlsx "#?.(xlsx|xlsm|xltx)" "'P'" "'K'" 3 4
docdesc PPTX "PowerPoint presentation" pptx "#?.(pptx|pptm|ppsx|potx)" "'P'" "'K'" 3 4
# OpenDocument: its first entry is the mimetype, stored.
# shellcheck disable=SC2046
docdesc ODF OpenDocument odf "#?" "'P'" "'K'" 3 4 $(i=0; while [ $i -lt 26 ]; do printf 'ANY '; i=$((i+1)); done) \
    $(chars "mimetypeapplication/vnd.oasis.opendocument.")
# Office 97-2003 files are OLE2 compound files: the name tells them apart.
docdesc DOC "Word 97 document" doc "#?.(doc|dot)" 0xd0 0xcf 0x11 0xe0 0xa1 0xb1 0x1a 0xe1
docdesc XLS "Excel 97 workbook" xls "#?.(xls|xlt)" 0xd0 0xcf 0x11 0xe0 0xa1 0xb1 0x1a 0xe1
docdesc PPT "PowerPoint 97 presentation" ppt "#?.(ppt|pps|pot)" 0xd0 0xcf 0x11 0xe0 0xa1 0xb1 0x1a 0xe1
DT_TEXT=1 docdesc RTF "Rich Text Format" rtf "#?" "'{'" "'\\'" "'r'" "'t'" "'f'"
docdesc WPD WordPerfect wpd "#?" 0xff "'W'" "'P'" "'C'"
# shellcheck disable=SC2046
DT_TEXT=1 docdesc PS PostScript ps "#?" $(chars "%!PS")
docdesc EPS "EPS with preview" eps "#?" 0xc5 0xd0 0xd3 0xc6
# EPUB is a ZIP whose first entry is its mimetype, as OpenDocument's.
# shellcheck disable=SC2046
docdesc EPUB "EPUB e-book" epub "#?" "'P'" "'K'" 3 4 $(i=0; while [ $i -lt 26 ]; do printf 'ANY '; i=$((i+1)); done) \
    $(chars "mimetypeapplication/epub+zip")
# Text with no signature: by name only, ahead of the ascii datatype.
DT_TEXT=1 DT_PRIORITY=1 docdesc CSV "CSV table" csv "#?.(csv|tsv)"
DT_TEXT=1 DT_PRIORITY=1 docdesc Markdown Markdown mdwn "#?.(md|markdown)"

# --- openvideo.datatype -------------------------------------------------------
# MP4, MOV, MKV, AVI, WMV, MPEG and FLV, decoded frame by frame by the
# media.decode/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include $ACFLAGS" compile "$HERE" "$WORK/obj-openvideo" \
    common/dtstart.c common/dtlib.c common/dtservice.c openvideo/videoclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openvideo.datatype" \
    "$WORK"/obj-openvideo/common_dtstart.o "$WORK"/obj-openvideo/common_dtlib.o \
    "$WORK"/obj-openvideo/common_dtservice.o "$WORK"/obj-openvideo/openvideo_videoclass.o $ACLIB -lamiga \
    -Wl,-Map="$WORK/openvideo.datatype.map"
echo "openvideo.datatype: $(wc -c < "$OUT/Classes/DataTypes/openvideo.datatype") bytes"
videodesc() {    # FILE NAME ID PATTERN MASK...
    f=$1; n=$2; i=$3; pat=$4; shift 4
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" openvideo anim "$i" "$pat" \
        "\$VER: $f 47.1 (5.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
# APNG, played by the host like a video without sound, ahead of the PNG
# datatype: a PNG whose acTL chunk follows IHDR straight away (most
# encoders), or any PNG named .apng (acTL may come later).
# shellcheck disable=SC2046
DT_PRIORITY=1 videodesc APNG "Animated PNG" apng "#?" 0x89 $(chars "PNG") 0x0d 0x0a 0x1a 0x0a \
    $(i=0; while [ $i -lt 29 ]; do printf 'ANY '; i=$((i+1)); done) $(chars "acTL")
# shellcheck disable=SC2046
DT_PRIORITY=1 videodesc APNG-Name "Animated PNG (by name)" apng "#?.apng" 0x89 $(chars "PNG")
videodesc MP4 "MPEG-4 video" mp4 "#?.(mp4|m4v|mov|3gp|3g2)" ANY ANY ANY ANY "'f'" "'t'" "'y'" "'p'"
videodesc MOV "QuickTime movie" mov "#?.mov" ANY ANY ANY ANY "'m'" "'o'" "'o'" "'v'"
# MKV shares WebM's EBML header: by name, and tried before webm.datatype.
DT_PRIORITY=1 videodesc MKV "Matroska video" mkv "#?.mkv" 0x1a 0x45 0xdf 0xa3
videodesc AVI "AVI video" avi "#?" "'R'" "'I'" "'F'" "'F'" ANY ANY ANY ANY "'A'" "'V'" "'I'" "' '"
videodesc WMV "Windows Media video" wmv "#?.(wmv|asf)" 0x30 0x26 0xb2 0x75 0x8e 0x66 0xcf 0x11
videodesc MPEG-PS "MPEG video" mpg "#?" 0x00 0x00 0x01 0xba
videodesc MPEG-TS "MPEG transport stream" mts "#?.(ts|m2ts|mts)" 0x47
videodesc FLV "Flash video" flv "#?" "'F'" "'L'" "'V'"
