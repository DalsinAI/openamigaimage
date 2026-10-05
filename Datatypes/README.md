# Datatypes

`webp.datatype`, `webm.datatype`, `heif.datatype` and `opensound.datatype`
for AmigaOS 3.x, by Dalsin Limited. They let any datatypes program
(MultiView, OpenBrowser, a picture viewer, a player) open WebP pictures, WebM
video, AVIF and HEIC pictures, and FLAC, Ogg Vorbis, Opus, AAC, ALAC, WMA and
MP3 sounds. They run on a 68020 or better, with or without an FPU. WebP and
WebM need no other libraries; the others are decoded by a Cradle (below).

**Status:** Working on the bench: both decode exactly as the same libraries
do on a PC. Not yet run on real Amiga hardware; WebM playback inside MultiView
is not yet checked (see Tested).

## What they do

| Datatype | Group | Built on | What it shows |
| --- | --- | --- | --- |
| `webp.datatype` | picture | libwebp 1.6.0 | Lossy, lossless and alpha WebP, through picture.datatype's 24-bit mode (32-bit with alpha). An animated WebP shows its first frame. |
| `heif.datatype` | picture | the `media.decode/1` service | AVIF and HEIC/HEIF pictures (iPhone photos), decoded on the services card or a paired Cradle and sent back as 32-bit ARGB, scaled down to fit `ENV:OpenImage/MaxSide` (default 4096). A sequence shows its first picture. |
| `opensound.datatype` | sound | the `media.decode/1` service | FLAC, Ogg (Vorbis, Opus), AAC/M4A, ALAC, WMA and MP3, sent back as 16-bit PCM at a rate Paula plays (`ENV:OpenImage/SoundRate`, default 28000 Hz: 44.1 kHz comes as 22.05 kHz). 16-bit stereo with sound.datatype V44 or newer, else 8-bit mono. |
| `webm.datatype` | animation | libvpx 1.17.0 | WebM video, VP8 or VP9, decoded frame by frame as the animation plays. Frames are shown in 256 colours (a 6x6x6 colour cube with ordered dithering), so they play on any screen. No sound yet. |

OpenBrowser decodes the pictures in web pages through datatypes, so with
`webp.datatype` installed it shows WebP pictures too.

`heif.datatype` and `opensound.datatype` decode nothing themselves. It sends the file to
`media.decode/1` through `openservice.device` (DalsinAI/openamigaservice):
the host does the work on AmigaChrome and our Pi appliance, and a paired
Cradle on the LAN does it for a real Amiga, which we expect to have
PiStorm-class networking. With neither, the file does not open
(`not implemented`): AV1 and HEVC are too slow on a 68k at photo sizes, and
68k decoders for FLAC, Vorbis and Opus come next.
The service is described in openamigaservice's `docs/MEDIA_DECODE.md`.

`webm.datatype` reads the WebM container itself (`webm/webm_demux.c`) and
decodes with libvpx. Once OpenMedia's `openmedia.library` exists
(DalsinAI/openamigamedia), the datatype will hand the frames to it for
hardware decoding where a driver offers VP8 or VP9, and keep libvpx as the
fallback.

## Installing

Copy the files from `out/` (after building) to the same places on the Amiga:

| File | Goes to |
| --- | --- |
| `Classes/DataTypes/webp.datatype` | `SYS:Classes/DataTypes/` |
| `Classes/DataTypes/webm.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/WebP` | `DEVS:DataTypes/` |
| `Devs/DataTypes/WebM` | `DEVS:DataTypes/` |
| `Classes/DataTypes/heif.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/AVIF`, `AVIS`, `HEIC`, `HEIX`, `HEIF` | `DEVS:DataTypes/` |
| `Classes/DataTypes/opensound.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/FLAC`, `Ogg`, `M4A`, `M4B`, `WMA`, `MP3-ID3`, `AAC` | `DEVS:DataTypes/` |

`heif.datatype` and `opensound.datatype` also need `openservice.device` in `DEVS:` (OpenUp's
OpenService part installs it) and a services card or a paired Cradle.

Then reboot, or run `AddDataTypes DEVS:DataTypes/WebP DEVS:DataTypes/WebM`.

## Building

You need the os32-gcc16 compiler (bebbo's amiga-gcc on GCC 16.2 with libnix;
see DalsinAI/openamigabrowser `stove/`), Python 3 for the descriptor files,
and the libwebp and libvpx tarballs listed in `../SOURCES`, in the
repository's `tarballs/` folder. Then:

```
./build.sh
```

The datatypes and their descriptors land in `out/` (set `PREFIX` to change
that). `JOBS` sets libvpx's parallel jobs.

How they are made:

- **No start-up code.** Each datatype is a plain library with its own ROMTag
  (`common/dtlib.c`): Open, Close, Expunge and ObtainEngine(). Only libnix's
  string and setjmp functions are linked in, and memory comes from
  `AllocVec()`, so several programs can decode at once.
- **A stack of their own.** libvpx runs on a 96 KB stack (`common/dtstack.c`),
  because programs and datatypes.library's helper processes may call a
  datatype with only a few kilobytes.
- **No floating point.** libwebp's one use of it, setting up dithering, is
  patched to integer arithmetic (`patches/`), so the datatypes need neither
  an FPU nor the ROM math libraries (which change the FPU's precision in the
  calling task).
- **Descriptors** are written by `common/mkdtdesc.py`, which reproduces the
  system's own `DEVS:DataTypes/PNG` byte for byte.

## Tested

On AmigaOS 3.2.3 on AmigaChrome's AC090 emulation (68040 with FPU, 256 MB),
Instance-24, 4 October 2026, with test files made by `tests/make-media.sh`.

`tests/dtpic.c` opens each picture through datatypes.library and reads it
back as ARGB. Its checksums equal `tests/webpraw.c` decoding the same files
with libwebp on the PC:

```
DH1:OB/webp/lossy.webp 64 48 alpha=0 24b3d77f
DH1:OB/webp/lossless-alpha.webp 40 30 alpha=1 65a9a948
DH1:OB/webp/lossy-alpha.webp 50 20 alpha=1 ca376736
DH1:OB/webp/anim.webp 32 32 alpha=0 02bb8100
```

`tests/dtanim.c` opens each clip as an animation and loads every frame with
`ADTM_LOADFRAME`. All 60 frames (30 VP8, 30 VP9, 160x120 at 10 frames a
second) have the same 256-colour pixels as `tests/webmprobe.c -chunky` on the
PC:

```
DH1:OB/webm/vp8.webm 160x120 depth=8 frames=30 fps=10 key=yes
frame 0 shown=0 sum=d0a7d409
frame 1 shown=1 sum=d5d0880e
...
DH1:OB/webm/vp9.webm 160x120 depth=8 frames=30 fps=10 key=yes
```

MultiView opens the VP8 clip and shows its first frame with the animation
controls. Playback inside MultiView, and running on real Amiga hardware,
are not yet checked.

## Known issues

- picture.datatype 47.19 keeps the alpha of a picture's last pixel only when
  its depth is 32, so `webp.datatype` gives pictures with alpha a depth of 32.
- `heif.datatype`: not yet run on the bench. Rotation and mirroring stored in
  AVIF files are not applied yet (HEIC's are). No 68k decoder.
- `opensound.datatype`: not yet run on the bench. To check there: the
  sample length sound.datatype V44+ expects for 16-bit samples (frames are
  given), and that it frees the sample with FreeVec(). The Ogg and WMA
  descriptors also match Theora and WMV video, which it refuses.
- `webm.datatype`: no sound (WebM's Vorbis and Opus need decoders of their
  own); 256 colours only; VP9 decoded in software is slow on a real 68k;
  files over 4 GB and laced video blocks are not supported.

## Licence

The datatype code (`build.sh`, `common/`, `include/`, `webp/`, `webm/`, `heif/`, `opensound/`, `tests/`) is
MIT, Copyright (c) 2026 Dalsin Limited, as the rest of this repository.
libwebp and libvpx keep their BSD licences and the WebM Project's patent
grants (`../upstream/libwebp/`, `../upstream/libvpx/`); a patch to their
source stays under their licence.
