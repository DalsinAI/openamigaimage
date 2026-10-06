# Datatypes

`webp.datatype`, `webm.datatype`, `openpicture.datatype`,
`opensound.datatype`, `opendoc.datatype` and `openvideo.datatype` for
AmigaOS 3.x, by Dalsin Limited. They let any datatypes program (MultiView,
OpenBrowser, a picture viewer, a player) open WebP pictures, WebM video,
AVIF, HEIC, JPEG XL, camera RAW, Photoshop and GIMP pictures, FLAC, Ogg
Vorbis, Opus, AAC, ALAC, WMA and MP3 sounds, MIDI, SID and PC tracker
tunes, office documents, PostScript, e-books, and MP4, MKV, AVI, WMV and
MPEG video. They run on a 68020 or better, with or without an FPU. WebP and
WebM need no other libraries; the others are decoded by a Cradle (below).

**Status:** Working on the bench: both decode exactly as the same libraries
do on a PC. Not yet run on real Amiga hardware; WebM playback inside MultiView
is not yet checked (see Tested).

## What they do

| Datatype | Group | Built on | What it shows |
| --- | --- | --- | --- |
| `webp.datatype` | picture | libwebp 1.6.0 | Lossy, lossless and alpha WebP, through picture.datatype's 24-bit mode (32-bit with alpha). An animated WebP shows its first frame. |
| `openpicture.datatype` | picture | the `media.decode/1` service | AVIF and HEIC/HEIF (iPhone photos), JPEG XL, camera RAW (Canon, Nikon, Sony, DNG and more), Photoshop PSD, GIMP XCF, OpenEXR, Radiance HDR, QOI, DDS, JPEG 2000, OpenRaster and Krita (their flattened picture), comic books (CBZ, the first page), SVG (drawn at its own size) and TrueType/OpenType fonts (a sample sheet), decoded on the services card or a paired Cradle and sent back as 32-bit ARGB, scaled down to fit `ENV:OpenImage/MaxSide` (default 4096). A sequence shows its first picture. |
| `opensound.datatype` | sound | the `media.decode/1` service | FLAC, Ogg (Vorbis, Opus), AAC/M4A, ALAC, WMA and MP3; MIDI (played through FluidSynth on the Cradle), C64 SID tunes (the first three minutes, through sidplayfp) and XM, IT and S3M modules (libopenmpt), sent back as 16-bit PCM at a rate Paula plays (`ENV:OpenImage/SoundRate`, default 28000 Hz: 44.1 kHz comes as 22.05 kHz). 16-bit stereo with sound.datatype V44 or newer, else 8-bit mono. |
| `opendoc.datatype` | picture | the `doc.render/1` service | DOCX, XLSX, PPTX, OpenDocument, Word/Excel/PowerPoint 97, RTF, WordPerfect, PostScript and EPS, EPUB e-books, Markdown and CSV, laid out by LibreOffice or Apache OpenOffice (Ghostscript for PostScript) on the Cradle and shown as their pages one under the other, `ENV:OpenImage/DocWidth` pixels wide (default 800), the first `ENV:OpenImage/DocPages` pages (default 8). |
| `openvideo.datatype` | animation | the `media.decode/1` service | MP4/MOV, MKV, AVI, WMV, MPEG and FLV video, and animated PNG (H.264, HEVC, AV1, VP9, MPEG-4, MPEG-1/2, WMV), kept open on the Cradle and sent a frame at a time in webm.datatype's 256 colours, scaled to fit `ENV:OpenImage/VideoWidth` x `VideoHeight` (default 640 x 480), with its sound as 8-bit mono. |
| `webm.datatype` | animation | libvpx 1.17.0 | WebM video, VP8 or VP9, decoded frame by frame as the animation plays. Frames are shown in 256 colours (a 6x6x6 colour cube with ordered dithering), so they play on any screen. Sound through `media.decode/1` when a card or Cradle offers it. |

OpenBrowser decodes the pictures in web pages through datatypes, so with
`webp.datatype` installed it shows WebP pictures too.

`openpicture.datatype`, `opensound.datatype`, `opendoc.datatype` and
`openvideo.datatype` decode nothing themselves. It sends the file to
`media.decode/1` through `openservice.device` (DalsinAI/openamigaservice):
the host does the work on AmigaChrome and our Pi appliance, and a paired
Cradle on the LAN does it for a real Amiga, which we expect to have
PiStorm-class networking. With neither, the file does not open
(`not implemented`): AV1 and HEVC are too slow on a 68k at photo sizes, and
68k decoders for FLAC, Vorbis and Opus come next.
The services are described in openamigaservice's `docs/MEDIA_DECODE.md`
and `docs/DOC_RENDER.md`.

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
| `Classes/DataTypes/openpicture.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/AVIF`, `AVIS`, `HEIC`, `HEIX`, `HEIF`, `JXL`, `JXL-ISO`, `RAW`, `PSD`, `XCF`, `EXR`, `HDR`, `HDR-Bin`, `QOI`, `DDS`, `JP2`, `J2K`, `ORA`, `KRA`, `CBZ`, `SVG`, `TTF`, `OTF`, `OTF-TT` | `DEVS:DataTypes/` |
| `Classes/DataTypes/opensound.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/FLAC`, `Ogg`, `M4A`, `M4B`, `WMA`, `MP3-ID3`, `AAC`, `MIDI`, `PSID`, `RSID`, `XM`, `IT`, `S3M` | `DEVS:DataTypes/` |
| `Classes/DataTypes/opendoc.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/DOCX`, `XLSX`, `PPTX`, `ODF`, `DOC`, `XLS`, `PPT`, `RTF`, `WPD`, `PS`, `EPS`, `EPUB`, `CSV`, `Markdown` | `DEVS:DataTypes/` |
| `Classes/DataTypes/openvideo.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/MP4`, `MOV`, `MKV`, `AVI`, `WMV`, `MPEG-PS`, `MPEG-TS`, `FLV`, `APNG`, `APNG-Name` | `DEVS:DataTypes/` |

`openpicture.datatype`, `opensound.datatype`, `opendoc.datatype` and
`openvideo.datatype` also need `openservice.device` in `DEVS:` (OpenUp's
OpenService part installs it) and a services card or a paired Cradle.

Then reboot, or run `AddDataTypes DEVS:DataTypes/WebP DEVS:DataTypes/WebM`.

## Building

You need the os32-gcc16 compiler (bebbo's amiga-gcc on GCC 16.2 with libnix;
see DalsinAI/openamigabrowser `stove/`), Python 3 for the descriptor files,
and the libwebp and libvpx tarballs listed in `../SOURCES`, in the
repository's `tarballs/` folder; with `AC_HELPERS=1`, also amigachrome-guest
at the commit in `../AMIGACHROME_GUEST_PINNED_COMMIT` (see the main README's
Building). Then:

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
- **AC090's native helpers.** `common/dtlib.c`'s `realloc` copies, and its
  `calloc` clears, through amigachrome-guest's helpers (`../achelpers/`,
  `AC_HELPERS=1`, or `auto`, the default, when the pinned commit is to hand): host code on AmigaChrome, 68k code written
  for the 68020 and 68040 elsewhere, and for under 64 bytes libnix's
  `memcpy` and `memset` as before. `calloc` then asks `AllocVec()` for
  uncleared memory, since exec's clearing is 68k code. `AC_HELPERS=0`
  builds them as before.
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
- `openpicture.datatype`: not yet run on the bench. Rotation and mirroring
  stored in AVIF files are not applied yet (HEIC's are), nor EXIF rotation.
  No 68k decoder. Camera RAW is chosen by its name, ahead of a TIFF
  datatype, and SVG by its name, ahead of the text datatype. TGA, PCX, ICO
  and the other classic formats are left to the Amiga's own datatypes, which
  work without a Cradle (`media.decode/1` still reads ICO, for OpenBrowser's
  site icons). An APNG is told by an acTL chunk straight after IHDR, or by
  the name `.apng`; others show as a still PNG. An APNG's frames come
  composited on black, without alpha (animation.datatype has none), at a
  steady rate that keeps its uneven frame delays; it repeats when its
  play count is not one. Animated WebP stays a still in `webp.datatype` until the
  Cradle's FFmpeg decodes WebP animation. An OpenType font with TrueType
  outlines is told by its `.otf` name.
- Text formats (SVG, Radiance HDR's header, PostScript, RTF, CSV and
  Markdown) have text descriptors (DTF_ASCII): datatypes.library only tries
  those on a file that reads as text.
- The ProTracker MOD and MED descriptors are left out on purpose: the
  Amiga plays those itself. Pictures in JPEG, PNG and GIF also stay with the
  system's datatypes; `media.decode/1` decodes them for OpenBrowser.
- `opensound.datatype`: not yet run on the bench. To check there: the
  sample length sound.datatype V44+ expects for 16-bit samples (frames are
  given), and that it frees the sample with FreeVec(). The Ogg and WMA
  descriptors also match Theora and WMV video, which it refuses.
- `opendoc.datatype`: not yet run on the bench. Every page is drawn at the
  first page's size. Office Open XML and Office 97 files are told apart by
  their names (`.docx`, `.xls`...), as they share a ZIP or OLE2 header with
  other files. CSV and Markdown are chosen by their names. No text view
  without a Cradle yet.
- `openvideo.datatype`: not yet run on the bench. The whole file is read
  into memory and sent once, so a video needs that much free RAM. Each
  frame opens the service afresh, because animation.datatype loads frames
  from its own process. MKV is chosen by its name and tried before
  webm.datatype (both are EBML). Each frame is a planar bitmap in chip RAM:
  when the largest free chip block holds fewer than four frames, the video
  is asked for at half the size (down to 160 wide), and running out says
  "not enough memory" rather than nothing.
- `webm.datatype`: sound (Vorbis or Opus) only through a services card or a
  paired Cradle, as 8-bit mono, for files up to 64 MB; 256 colours only
  (VP9 in 4:2:0, 4:4:4 or RGB; 8-bit only); VP9 decoded in software is slow on a real 68k;
  files over 4 GB and laced video blocks are not supported.

## Licence

The datatype code (`build.sh`, `common/`, `include/`, `webp/`, `webm/`, `openpicture/`, `opensound/`, `opendoc/`, `openvideo/`, `tests/`) is
MIT, Copyright (c) 2026 Dalsin Limited, as the rest of this repository.
libwebp and libvpx keep their BSD licences and the WebM Project's patent
grants (`../upstream/libwebp/`, `../upstream/libvpx/`); a patch to their
source stays under their licence.
