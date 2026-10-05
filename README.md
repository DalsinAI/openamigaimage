# openamigaimage

zlib, libpng and libjpeg for AmigaOS 3.x on 68k, built as static link libraries for
GCC programs, and the project's datatypes: `webp.datatype`, `webm.datatype`, `heif.datatype`, `opensound.datatype`, `opendoc.datatype` and `openvideo.datatype`
(see [Datatypes](Datatypes/README.md)). Part of the [OpenAmiga](https://github.com/DalsinAI/openamiga)
ports, made for [OpenBrowser](https://github.com/DalsinAI/openamigabrowser),
the WebKit browser for AmigaOS 3.2.

**Status:** Working: all three libraries build, and the smoke test's round trips pass on the bench. The WebP and WebM datatypes decode on the bench exactly as on a PC.

This repository holds the Amiga build, not zlib, libpng, libjpeg, libwebp, libvpx itself: a build script,
a smoke test and the upstream licences.

## Upstream

| Library | Version | Licence | Home |
| --- | --- | --- | --- |
| zlib | 1.3.1 (Chromium's copy) | zlib licence (upstream/zlib/LICENSE) | https://zlib.net/ |
| libpng | 1.6.58 | PNG Reference Library License v2 (upstream/libpng/LICENSE) | http://www.libpng.org/ |
| libjpeg | 9f | IJG licence (upstream/libjpeg/README, "LEGAL ISSUES") | https://www.ijg.org/ |
| libwebp | 1.6.0 | BSD 3-clause, with the WebM patent grant (upstream/libwebp/COPYING, PATENTS) | https://chromium.googlesource.com/webm/libwebp |
| libvpx | 1.17.0 | BSD 3-clause, with the WebM patent grant (upstream/libvpx/LICENSE, PATENTS) | https://chromium.googlesource.com/webm/libvpx |

The exact files and their SHA-256 sums are in [SOURCES](SOURCES). All credit
for the library goes to its authors; see `upstream/` for their notices.

## What the Amiga port changes

- No source changes. zlib is Chromium's copy of 1.3.1 (the one in AROS's ports cache), built with its plain C code and without Chromium's symbol renaming. libpng uses its prebuilt `pnglibconf.h`; libjpeg its `jconfig.txt`.
- Note that OpenBrowser decodes page images through the system's datatypes, not these libraries; libpng is used to write PNG files and by FreeType.

## Building

You need the os32-gcc16 compiler (bebbo's amiga-gcc on GCC 16.2 with libnix
and libpthread; see DalsinAI/openamigabrowser `stove/`) and the upstream
tarballs from [SOURCES](SOURCES) in `tarballs/`. Then:

```
./build.sh
```

The libraries and headers land in `out/` (set `PREFIX` to change that). The
script prints which other settings it needs, if any. Target: 68020 or better
with an FPU (`-m68020 -m68881`), libnix (`-mcrt=nix20`).

Link with: `-lpng -ljpeg -lz -lm`

## Tested

`tests/imagetest.c`, run on AmigaOS 3.2.3 on AmigaChrome's AC090 emulation (68040 with FPU, 256 MB), Instance-24, 4 October 2026, as `imagetest`:

```
ZLIB 1.3.1 crc=e45c1550 packed=683 roundtrip=ok
LIBPNG 1.6.58 bytes=117 roundtrip=ok
LIBJPEG 9f bytes=695 mean_error=0.33
IMAGE_DONE
```

It has not yet been run on real Amiga hardware.

## Datatypes

The `Datatypes` drawer holds the datatypes the project makes, so any datatypes
program can open more formats. They are built by `Datatypes/build.sh`, apart
from the libraries above:

- `webp.datatype` (libwebp): WebP pictures, lossy, lossless and with alpha.
- `webm.datatype` (libvpx): WebM video, VP8 and VP9, in 256 colours.
- `heif.datatype`: AVIF and HEIC pictures, decoded by a Cradle through the
  `media.decode/1` service (DalsinAI/openamigaservice).
- `opensound.datatype`: FLAC, Ogg Vorbis, Opus, AAC, ALAC, WMA and MP3
  sounds, decoded the same way.
- `opendoc.datatype`: office documents (DOCX, XLSX, PPTX, OpenDocument,
  Office 97, RTF, WordPerfect) as their pages, laid out by LibreOffice on a
  Cradle through `doc.render/1`.
- `openvideo.datatype`: MP4, MKV, AVI, WMV and MPEG video, sent a frame at a
  time by a Cradle through `media.decode/1`.

Both decode on the bench exactly as their libraries do on a PC. Installing,
building and the test results are in [Datatypes/README.md](Datatypes/README.md).

## Known issues

- None known.

## Licence

Dalsin Limited's Amiga changes (the build script, patches, configuration
headers and tests) are MIT, Copyright (c) 2026 Dalsin Limited: see
[LICENSE](LICENSE). zlib, libpng, libjpeg, libwebp, libvpx keep their own licences, in
[upstream/](upstream/); a patch to their source stays under that licence.
