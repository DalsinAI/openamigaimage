# openamigaimage

zlib, libpng and libjpeg for AmigaOS 3.x on 68k, built as static link libraries for
GCC programs. Part of the [OpenAmiga](https://github.com/DalsinAI/openamiga)
ports, made for [OpenBrowser](https://github.com/DalsinAI/openamigabrowser),
the WebKit browser for AmigaOS 3.2.

**Status:** Working: all three build, and the smoke test's round trips pass on the bench.

This repository holds the Amiga build, not zlib, libpng, libjpeg itself: a build script,
a smoke test and the upstream licences.

## Upstream

| Library | Version | Licence | Home |
| --- | --- | --- | --- |
| zlib | 1.3.1 (Chromium's copy) | zlib licence (upstream/zlib/LICENSE) | https://zlib.net/ |
| libpng | 1.6.58 | PNG Reference Library License v2 (upstream/libpng/LICENSE) | http://www.libpng.org/ |
| libjpeg | 9f | IJG licence (upstream/libjpeg/README, "LEGAL ISSUES") | https://www.ijg.org/ |

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

## Known issues

- None known.

## Licence

Dalsin Limited's Amiga changes (the build script, patches, configuration
headers and tests) are MIT, Copyright (c) 2026 Dalsin Limited: see
[LICENSE](LICENSE). zlib, libpng, libjpeg keep their own licences, in
[upstream/](upstream/); a patch to their source stays under that licence.
