# openamigaimage

zlib, libpng and libjpeg for AmigaOS 3.x on 68k, built as static link libraries for
GCC programs, and the project's datatypes: `webp.datatype`, `webm.datatype`, `openpicture.datatype`, `opensound.datatype`, `openmodule.datatype` (music modules: MOD, MED, Oktalyzer, DigiBooster, XM, S3M, IT, played on the Amiga itself), `opendoc.datatype` and `openvideo.datatype`
(see [Datatypes](Datatypes/README.md)). Part of the [OpenAmiga](https://github.com/DalsinAI/openamiga)
ports, made for [OpenBrowser](https://github.com/DalsinAI/openamigabrowser),
the WebKit browser for AmigaOS 3.2.

**Status:** Working: all three libraries build, and the smoke test's round trips pass on the bench. The WebP and WebM datatypes decode on the bench exactly as on a PC.

This repository holds the Amiga build, not zlib, libpng, libjpeg, libwebp, libvpx, libxmp itself: a build script,
a smoke test and the upstream licences.

## Upstream

| Library | Version | Licence | Home |
| --- | --- | --- | --- |
| zlib | 1.3.1 (Chromium's copy) | zlib licence (upstream/zlib/LICENSE) | https://zlib.net/ |
| libpng | 1.6.58 | PNG Reference Library License v2 (upstream/libpng/LICENSE) | http://www.libpng.org/ |
| libjpeg | 9f | IJG licence (upstream/libjpeg/README, "LEGAL ISSUES") | https://www.ijg.org/ |
| libwebp | 1.6.0 | BSD 3-clause, with the WebM patent grant (upstream/libwebp/COPYING, PATENTS) | https://chromium.googlesource.com/webm/libwebp |
| libvpx | 1.17.0 | BSD 3-clause, with the WebM patent grant (upstream/libvpx/LICENSE, PATENTS) | https://chromium.googlesource.com/webm/libvpx |
| libxmp | 4.7.3 | MIT (upstream/libxmp/COPYING; the code it carries from others, and their licences, in upstream/libxmp/CREDITS) | https://github.com/libxmp/libxmp |

The exact files and their SHA-256 sums are in [SOURCES](SOURCES). All credit
for the library goes to its authors; see `upstream/` for their notices.

## What the Amiga port changes

- No source changes. zlib is Chromium's copy of 1.3.1 (the one in AROS's ports cache), built with its plain C code and without Chromium's symbol renaming. libpng uses its prebuilt `pnglibconf.h`; libjpeg its `jconfig.txt`.
- AC090's native helpers (`AC_HELPERS=1`; `AC_HELPERS=0` builds the libraries as before; the default, `auto`, is 1 when the pinned amigachrome-guest commit is to hand and 0 otherwise, and says which). zlib's checksums and the big copies and fills in all three libraries go through amigachrome-guest's `common/amiga/ac_helpers`: magic functions that AmigaChrome's AC090 runs as host code, and that run as 68k code written for the 68020 and 68040 on a real Amiga. In AC090's JIT (68040, 64 KB calls, on our cloud test machine, 6 October 2026) crc32 ran at about 20 GB/s against 130 MB/s for zlib's own braided crc32 as 68k code, adler32 at about 20 GB/s against 1.1 GB/s, and a copy at about 33 GB/s against 2 (a longword loop) to 5 (MOVEM). Build flags and our own small files in `achelpers/` do it, not changes to the libraries:
  - `achelpers/zlibsums.sh` compiles zlib's own `crc32`, `crc32_z`, `adler32` and `adler32_z` under other names (`oai_zlib_*`), and `achelpers/ac_zlib.c` takes the public names: a buffer of 32 bytes or more goes to the helpers, a shorter one (or none) to zlib's own, with zlib's results for every call. So inflate's and deflate's checks, the gz functions and libpng's chunk CRCs all use them. Chromium's `copy_with_crc` (deflate's gzip path) calls crc32 inside `crc32.c`, so it moves to `ac_zlib.c` too when it is the plain copy and crc32 we expect; the script says which. zlib's API and ABI are unchanged (`crc32_combine` and the rest are zlib's own).
  - `achelpers/ac_string.h` is forced (`-include`) into every compile of the three libraries: `memcpy`, `memmove` and `memset` calls of 64 bytes or more (zlib's window, libpng's rows, libjpeg's buffers) go to the helpers, smaller ones stay GCC's own code.
  - The helper objects go into each of `libz.a`, `libpng.a` and `libjpeg.a`, so link lines stay as they were.
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

With `AC_HELPERS=1` (or `auto`, the default) the script also needs amigachrome-guest
at the commit in [AMIGACHROME_GUEST_PINNED_COMMIT](AMIGACHROME_GUEST_PINNED_COMMIT):
a checkout that has that commit, beside this repository (`../amigachrome-guest`,
else `../guest`) or named by `AMIGACHROME_GUEST`. `achelpers/achelpers.sh`
reads the helper sources from it at that commit, so other work in the
checkout does not matter. Under `auto`, a build that cannot find the commit
builds without the helpers and prints `AC090 native helpers: off`.

## Tested

`tests/imagetest.c`, run on AmigaOS 3.2.3 on AmigaChrome's AC090 emulation (68040 with FPU, 256 MB), Instance-24, 4 October 2026, as `imagetest`:

```
ZLIB 1.3.1 crc=e45c1550 packed=683 roundtrip=ok
LIBPNG 1.6.58 bytes=117 roundtrip=ok
LIBJPEG 9f bytes=695 mean_error=0.33
IMAGE_DONE
```

It has not yet been run on real Amiga hardware.

The run above predates AC090's native helpers. With them, the libraries are
not yet built with os32-gcc16 nor run on the bench. `tests/achelpers/test_achelpers.sh`
checks our side of it with an m68k Linux cross GCC and qemu-m68k, at -O0
and -O2 for the 68020 and the 68040, 6 October 2026: the helpers built from
the pinned commit; zlib's four checksums and `copy_with_crc` over
stand-ins for zlib's `crc32.c` and `adler32.c` (23690 checks against zlib's
results, each call going where it should); the forced header with zlib's,
libjpeg's and libpng's ways of calling `memcpy`, `memmove` and `memset`
(12001 checks); and the datatypes' `calloc` and `realloc`, with the helpers
and without:

```
ok   23690 of 23690 checks; 1396 calls to zlib's own, 10446 to the helpers (zlib's checksums, -O2 -m68040)
ok   12001 of 12001 checks (memcpy, memmove and memset, -O2 -m68040)
ok   12001 of 12001 checks (with the helpers; 0 of 6000 AllocVecs with MEMF_CLEAR, 2315 calls of 64 bytes and more) (dtlib.c, -O2 -m68040)
ok   9001 of 9001 checks (without the helpers; 3000 of 6000 AllocVecs with MEMF_CLEAR, 2315 calls of 64 bytes and more) (dtlib.c, -O2 -m68040)
ok   zlibsums.sh leaves deflate's gzip crc to zlib when crc32.c has no plain copy_with_crc
```

## Datatypes

The `Datatypes` drawer holds the datatypes the project makes, so any datatypes
program can open more formats. They are built by `Datatypes/build.sh`, apart
from the libraries above:

- `webp.datatype` (libwebp): WebP pictures, lossy, lossless and with alpha.
- `webm.datatype` (libvpx): WebM video, VP8 and VP9, in 256 colours.
- `openpicture.datatype`: AVIF, HEIC, JPEG XL, camera RAW, PSD, XCF, EXR,
  HDR, QOI, DDS and JPEG 2000 pictures, decoded by a Cradle through the
  `media.decode/1` service (DalsinAI/openamigaservice).
- `opensound.datatype`: FLAC, Ogg Vorbis, Opus, AAC, ALAC, WMA and MP3
  sounds, MIDI, SID tunes and XM, IT and S3M modules, decoded the same way.
- `opendoc.datatype`: office documents (DOCX, XLSX, PPTX, OpenDocument,
  Office 97, RTF, WordPerfect), PostScript, EPUB, Markdown and CSV as their
  pages, laid out by LibreOffice on a
  Cradle through `doc.render/1`.
- `openvideo.datatype`: MP4, MKV, AVI, WMV and MPEG video, sent a frame at a
  time by a Cradle through `media.decode/1`.

Both decode on the bench exactly as their libraries do on a PC. Installing,
building and the test results are in [Datatypes/README.md](Datatypes/README.md).

## Known issues

- AC090's native helpers: not yet built with os32-gcc16 against the real
  tarballs, so whether Chromium's `crc32.c` has the plain `copy_with_crc`
  (deflate's gzip path through the helpers) is still to be seen in the
  build's output; the gain on AmigaChrome and the speed on a real 68040 are
  not yet measured on the bench.

## Licence

Dalsin Limited's Amiga changes (the build script, patches, configuration
headers and tests) are MIT, Copyright (c) 2026 Dalsin Limited: see
[LICENSE](LICENSE). zlib, libpng, libjpeg, libwebp, libvpx keep their own licences, in
[upstream/](upstream/); a patch to their source stays under that licence.

## Contributors

openamigaimage is created and maintained by [SacredTrees](https://github.com/SacredTrees) with the AmigaChrome agent team, copyright Dalsin Limited. Everyone whose work it includes is credited in [`CONTRIBUTORS.md`](CONTRIBUTORS.md).
