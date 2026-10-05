#!/bin/sh
# openamigaimage datatypes: webp, webm, openpicture, opensound, opendoc and openvideo.datatype for AmigaOS
# 3.x, built with the os32-gcc16 compiler (bebbo's amiga-gcc, GCC 16.2,
# libnix). The datatypes run on any 68020 or better, with or without an FPU.
# MIT, Copyright (c) 2026 Dalsin Limited. libwebp and libvpx keep their
# BSD licences.
#
#   OS32_GCC16   compiler root holding prefix/ (default ~/AmigaChrome/stoves/os32-gcc16)
#   PREFIX       where Classes/ and Devs/ go (default ./out)
#   TARBALLS     folder holding the upstream tarballs listed in ../SOURCES
#                (default ./tarballs, else ../tarballs); the script checks
#                their SHA-256
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
# No -m68881: a datatype must load on a 68020 without an FPU too.
CFLAGS="-O2 -m68020 -fomit-frame-pointer -DNDEBUG -DWORDS_BIGENDIAN -Wall -Wno-pointer-sign"
mkdir -p "$OUT/Classes/DataTypes" "$OUT/Devs/DataTypes" "$WORK"

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
XFLAGS="-I$HERE/common -I$W/src" compile "$HERE" "$WORK/obj-webp" common/dtstart.c common/dtlib.c webp/webpclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/webp.datatype" \
    "$WORK"/obj-webp/common_dtstart.o "$WORK"/obj-webp/common_dtlib.o "$WORK"/obj-webp/webp_webpclass.o "$WORK"/obj-libwebp/*.o -lamiga \
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
XFLAGS="-I$HERE/common -I$HERE/include -I$HERE/webm -I$V -I$WORK/libvpx-build" compile "$HERE" "$WORK/obj-webm" \
    common/dtstart.c common/dtlib.c common/dtstack.c common/dtservice.c webm/webm_demux.c webm/webmclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/webm.datatype" \
    "$WORK"/obj-webm/common_dtstart.o "$WORK"/obj-webm/common_dtlib.o "$WORK"/obj-webm/common_dtstack.o \
    "$WORK"/obj-webm/common_dtservice.o \
    "$WORK"/obj-webm/webm_webm_demux.o "$WORK"/obj-webm/webm_webmclass.o "$WORK/libvpx-build/libvpx.a" -lamiga \
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
XFLAGS="-I$HERE/common -I$HERE/include" compile "$HERE" "$WORK/obj-openpicture" \
    common/dtstart.c common/dtlib.c common/dtservice.c openpicture/pictureclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openpicture.datatype" \
    "$WORK"/obj-openpicture/common_dtstart.o "$WORK"/obj-openpicture/common_dtlib.o \
    "$WORK"/obj-openpicture/common_dtservice.o "$WORK"/obj-openpicture/openpicture_pictureclass.o -lamiga \
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
picdesc HDR "Radiance HDR" hdr "#?" $(chars "#?RADIANCE")
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
# Camera RAW is TIFF (or its own thing) inside: the name tells it, ahead of
# the TIFF datatype.
DT_PRIORITY=1 picdesc RAW "Camera RAW" raw \
    "#?.(cr2|cr3|crw|nef|nrw|arw|srf|sr2|dng|orf|rw2|raf|pef|srw|x3f|erf|kdc|dcr|mrw|3fr|iiq|rwl)"

# --- opensound.datatype -------------------------------------------------------
# FLAC, Ogg (Vorbis, Opus), AAC/M4A, ALAC, WMA and MP3, decoded by the
# media.decode/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include" compile "$HERE" "$WORK/obj-opensound" \
    common/dtstart.c common/dtlib.c common/dtservice.c opensound/soundclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/opensound.datatype" \
    "$WORK"/obj-opensound/common_dtstart.o "$WORK"/obj-opensound/common_dtlib.o \
    "$WORK"/obj-opensound/common_dtservice.o "$WORK"/obj-opensound/opensound_soundclass.o -lamiga \
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
# PC tracker modules through libopenmpt. ProTracker MODs and MED stay with
# the Amiga's own players.
# shellcheck disable=SC2046
sounddesc XM "FastTracker module" xm $(chars "Extended Module: ")
# shellcheck disable=SC2046
sounddesc IT "Impulse Tracker module" it $(chars "IMPM")
# shellcheck disable=SC2046
sounddesc S3M "Scream Tracker module" s3m $(i=0; while [ $i -lt 44 ]; do printf 'ANY '; i=$((i+1)); done) $(chars "SCRM")

# --- opendoc.datatype ---------------------------------------------------------
# Office documents as pictures of their pages, laid out by LibreOffice through
# the doc.render/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include" compile "$HERE" "$WORK/obj-opendoc" \
    common/dtstart.c common/dtlib.c common/dtservice.c opendoc/docclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/opendoc.datatype" \
    "$WORK"/obj-opendoc/common_dtstart.o "$WORK"/obj-opendoc/common_dtlib.o \
    "$WORK"/obj-opendoc/common_dtservice.o "$WORK"/obj-opendoc/opendoc_docclass.o -lamiga \
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
docdesc RTF "Rich Text Format" rtf "#?" "'{'" "'\\'" "'r'" "'t'" "'f'"
docdesc WPD WordPerfect wpd "#?" 0xff "'W'" "'P'" "'C'"
# shellcheck disable=SC2046
docdesc PS PostScript ps "#?" $(chars "%!PS")
docdesc EPS "EPS with preview" eps "#?" 0xc5 0xd0 0xd3 0xc6
# EPUB is a ZIP whose first entry is its mimetype, as OpenDocument's.
# shellcheck disable=SC2046
docdesc EPUB "EPUB e-book" epub "#?" "'P'" "'K'" 3 4 $(i=0; while [ $i -lt 26 ]; do printf 'ANY '; i=$((i+1)); done) \
    $(chars "mimetypeapplication/epub+zip")
# Text with no signature: by name only.
docdesc CSV "CSV table" csv "#?.(csv|tsv)"
docdesc Markdown Markdown mdwn "#?.(md|markdown)"

# --- openvideo.datatype -------------------------------------------------------
# MP4, MOV, MKV, AVI, WMV, MPEG and FLV, decoded frame by frame by the
# media.decode/1 service on the services card or a paired Cradle.
XFLAGS="-I$HERE/common -I$HERE/include" compile "$HERE" "$WORK/obj-openvideo" \
    common/dtstart.c common/dtlib.c common/dtservice.c openvideo/videoclass.c
$CC -nostartfiles -m68020 -o "$OUT/Classes/DataTypes/openvideo.datatype" \
    "$WORK"/obj-openvideo/common_dtstart.o "$WORK"/obj-openvideo/common_dtlib.o \
    "$WORK"/obj-openvideo/common_dtservice.o "$WORK"/obj-openvideo/openvideo_videoclass.o -lamiga \
    -Wl,-Map="$WORK/openvideo.datatype.map"
echo "openvideo.datatype: $(wc -c < "$OUT/Classes/DataTypes/openvideo.datatype") bytes"
videodesc() {    # FILE NAME ID PATTERN MASK...
    f=$1; n=$2; i=$3; pat=$4; shift 4
    python3 "$HERE/common/mkdtdesc.py" "$OUT/Devs/DataTypes/$f" "$n" openvideo anim "$i" "$pat" \
        "\$VER: $f 47.1 (5.10.2026)" "$@"
    echo "Devs/DataTypes/$f: $(wc -c < "$OUT/Devs/DataTypes/$f") bytes"
}
videodesc MP4 "MPEG-4 video" mp4 "#?.(mp4|m4v|mov|3gp|3g2)" ANY ANY ANY ANY "'f'" "'t'" "'y'" "'p'"
videodesc MOV "QuickTime movie" mov "#?.mov" ANY ANY ANY ANY "'m'" "'o'" "'o'" "'v'"
# MKV shares WebM's EBML header: by name, and tried before webm.datatype.
DT_PRIORITY=1 videodesc MKV "Matroska video" mkv "#?.mkv" 0x1a 0x45 0xdf 0xa3
videodesc AVI "AVI video" avi "#?" "'R'" "'I'" "'F'" "'F'" ANY ANY ANY ANY "'A'" "'V'" "'I'" "' '"
videodesc WMV "Windows Media video" wmv "#?.(wmv|asf)" 0x30 0x26 0xb2 0x75 0x8e 0x66 0xcf 0x11
videodesc MPEG-PS "MPEG video" mpg "#?" 0x00 0x00 0x01 0xba
videodesc MPEG-TS "MPEG transport stream" mts "#?.(ts|m2ts|mts)" 0x47
videodesc FLV "Flash video" flv "#?" "'F'" "'L'" "'V'"
