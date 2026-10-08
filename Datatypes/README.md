# Datatypes

`webp.datatype`, `webm.datatype`, `openpicture.datatype`,
`opensound.datatype`, `openmodule.datatype`, `opendoc.datatype` and
`openvideo.datatype` for AmigaOS 3.x, by Dalsin Limited. They let any
datatypes program (MultiView, OpenBrowser, OpenPlay, a picture viewer) open
WebP pictures, WebM video, AVIF, HEIC, JPEG XL, camera RAW, Photoshop and
GIMP pictures, FLAC, Ogg Vorbis, Opus, AAC, ALAC, WMA and MP3 sounds, MIDI
and SID tunes, music modules (ProTracker MOD, MED and OctaMED, Oktalyzer,
DigiBooster, FastTracker 2 XM, Scream Tracker 3 S3M, Impulse Tracker IT and
more), office documents, PostScript, e-books, and MP4, MKV, AVI, WMV and
MPEG video. They run on a 68020 or better, with or without an FPU. WebP,
WebM and music modules need no other libraries and play on the Amiga
itself; the others are decoded by a Cradle (below).

**Status:** Working on the bench: both decode exactly as the same libraries
do on x86 cores. Not yet run on real Amiga hardware; WebM playback inside MultiView
is not yet checked (see Tested).

## What they do

| Datatype | Group | Built on | What it shows |
| --- | --- | --- | --- |
| `webp.datatype` | picture | libwebp 1.6.0 | Lossy, lossless and alpha WebP, through picture.datatype's 24-bit mode (32-bit with alpha). An animated WebP shows its first frame. |
| `openpicture.datatype` | picture | the `media.decode/1` service | AVIF and HEIC/HEIF (iPhone photos), JPEG XL, camera RAW (Canon, Nikon, Sony, DNG and more), Photoshop PSD, GIMP XCF, OpenEXR, Radiance HDR, QOI, DDS, JPEG 2000, OpenRaster and Krita (their flattened picture), comic books (CBZ, the first page), SVG (drawn at its own size) and TrueType/OpenType fonts (a sample sheet), decoded on the services card or a paired Cradle and sent back as 32-bit ARGB, scaled down to fit `ENV:OpenImage/MaxSide` (default 4096). A sequence shows its first picture. |
| `opensound.datatype` | sound | the `media.decode/1` service | FLAC, Ogg (Vorbis, Opus), AAC/M4A, ALAC, WMA and MP3; MIDI (played through FluidSynth on the Cradle), C64 SID tunes (the first three minutes, through sidplayfp) and XM, IT and S3M modules (libopenmpt), sent back as 16-bit PCM at a rate Paula plays (`ENV:OpenImage/SoundRate`, default 28000 Hz: 44.1 kHz comes as 22.05 kHz). 16-bit stereo with sound.datatype V44 or newer, else 8-bit mono. |
| `openmodule.datatype` | sound | libxmp 4.7.3 | Music modules, played on the Amiga itself: ProTracker, NoiseTracker and SoundTracker MODs (M.K., M!K!, FLT4, FLT8, 2CHN to 32CH, and 15-instrument SoundTracker by its name), MED and OctaMED (MMD0 to MMD3, MED 2 to 4), Oktalyzer, DigiBooster and DigiBooster Pro, FastTracker 2 XM, Scream Tracker 3 S3M and Impulse Tracker IT (libxmp reads some 60 module formats in all; a descriptor for another is all it takes). Streamed by a player process of its own, so a song starts at once and never sits in memory whole: 16-bit stereo through AHI (`ahi.device` unit 0, at its own rate), else 8-bit stereo on two Paula channels. Mixed on a cores board core (AmigaChrome's, through `openmulticore.library`), else by `media.decode/1` on a services card or a paired Cradle, else on this CPU, and the datatype says which (`OIA_DecodedBy`, `include/datatypes/openimage.h`; `openmodule/DESIGN.md` sections 2 and 7). The module's title and length show; play, pause, stop, volume and repeat work as for a sound. |
| `opendoc.datatype` | picture | the `doc.render/1` service | DOCX, XLSX, PPTX, OpenDocument, Word/Excel/PowerPoint 97, RTF, WordPerfect, PostScript and EPS, EPUB e-books, Markdown and CSV, laid out by LibreOffice or Apache OpenOffice (Ghostscript for PostScript) on the Cradle and shown as their pages one under the other, `ENV:OpenImage/DocWidth` pixels wide (default 800), the first `ENV:OpenImage/DocPages` pages (default 8). |
| `openvideo.datatype` | animation | the `media.decode/1` service | MP4/MOV, MKV, AVI, WMV, MPEG and FLV video, and animated PNG (H.264, HEVC, AV1, VP9, MPEG-4, MPEG-1/2, WMV), kept open on the Cradle and sent a frame at a time in webm.datatype's 256 colours, scaled to fit `ENV:OpenImage/VideoWidth` x `VideoHeight` (default 640 x 480), with its sound as 8-bit mono. |
| `webm.datatype` | animation | libvpx 1.17.0 | WebM video, VP8 or VP9, decoded frame by frame as the animation plays. Frames are shown in 256 colours (a 6x6x6 colour cube with ordered dithering), so they play on any screen. Sound through `media.decode/1` when a card or Cradle offers it. |

OpenBrowser decodes the pictures in web pages through datatypes, so with
`webp.datatype` installed it shows WebP pictures too.

`openpicture.datatype`, `opensound.datatype`, `opendoc.datatype` and
`openvideo.datatype` decode nothing themselves. It sends the file to
`media.decode/1` through `openservice.device` (DalsinAI/openamigaservice):
the x86 or ARM64 cores do the work on AmigaChrome and the Pi appliance, and a paired
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
| `Classes/DataTypes/openmodule.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/ProTracker`, `ProTracker-100`, `StarTrekker`, `StarTrekker-8`, `MOD-xCHN`, `MOD-xxCH`, `SoundTracker`, `OctaMED`, `MED-2`, `MED-3`, `MED-4`, `Oktalyzer`, `DigiBooster`, `DigiBooster-Pro`, `Module-XM`, `Module-S3M`, `Module-IT` | `DEVS:DataTypes/` |
| `Classes/DataTypes/opendoc.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/DOCX`, `XLSX`, `PPTX`, `ODF`, `DOC`, `XLS`, `PPT`, `RTF`, `WPD`, `PS`, `EPS`, `EPUB`, `CSV`, `Markdown` | `DEVS:DataTypes/` |
| `Classes/DataTypes/openvideo.datatype` | `SYS:Classes/DataTypes/` |
| `Devs/DataTypes/MP4`, `MOV`, `MKV`, `AVI`, `WMV`, `MPEG-PS`, `MPEG-TS`, `FLV`, `APNG`, `APNG-Name` | `DEVS:DataTypes/` |

`openpicture.datatype`, `opensound.datatype`, `opendoc.datatype` and
`openvideo.datatype` also need `openservice.device` in `DEVS:` (OpenUp's
OpenService part installs it) and a services card or a paired Cradle.

`openmodule.datatype` needs nothing else: no card and no Cradle (it then
mixes on this CPU, and plays through Paula without AHI). It plays through
AHI 6 when `ahi.device` is installed (on AmigaChrome with ACAHI's driver,
`acaudio.audio`). To hand the mixing to AmigaChrome's cores board it needs
`openmulticore.library` in `LIBS:` (DalsinAI/openamigamulticore, built
with its own `library/build.sh` and the os32 stove, not os32-gcc16; it is
not in OpenUp yet), and for `media.decode/1`, `openservice.device`.

Then reboot, or run `AddDataTypes REFRESH`.

## Building

You need the os32-gcc16 compiler (bebbo's amiga-gcc on GCC 16.2 with libnix;
see DalsinAI/openamigabrowser `stove/`), Python 3 for the descriptor files,
and the libwebp, libvpx and libxmp tarballs listed in `../SOURCES`, in the
repository's `tarballs/` folder; with `AC_HELPERS=1`, also amigachrome-guest
at the commit in `../AMIGACHROME_GUEST_PINNED_COMMIT` (see the main README's
Building). Then:

```
./build.sh
```

The datatypes and their descriptors land in `out/` (set `PREFIX` to change
that). `JOBS` sets libvpx's parallel jobs. The build also leaves
`work/modbench` (`tests/modbench.c`), which times libxmp's mixing on an
Amiga.

How they are made:

- **No start-up code.** Each datatype is a plain library with its own ROMTag
  (`common/dtlib.c`): Open, Close, Expunge and ObtainEngine(). Only libnix's
  string and setjmp functions are linked in, and memory comes from
  `AllocVec()`, so several programs can decode at once.
- **AC090's native helpers.** `common/dtlib.c`'s `realloc` copies, and its
  `calloc` clears, through amigachrome-guest's helpers (`../achelpers/`,
  `AC_HELPERS=1`, or `auto`, the default, when the pinned commit is to hand): x86 or ARM64 code on AmigaChrome, 68k code written
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
  calling task). The one exception is `openmodule.datatype`: libxmp works
  out periods in floating point, through libnix's soft floating point and
  the ROM math libraries, and does all its work in the datatype's own
  player process, which opens those libraries for itself, so the calling
  program's FPU is never touched.
- **openmodule.datatype streams.** sound.datatype 47 can only play a
  sample that is all in memory, so openmodule is what the V44 autodoc
  calls a streaming subclass: it answers `DTM_TRIGGER` itself and plays
  through audio.device, four buffers ahead, from a process of its own
  (`openmodule/DESIGN.md`). libxmp is built unchanged, with all its
  loaders; its depackers, ProWizard and Ogg Vorbis samples are left out
  (`openmodule/xmpglue.c`, `openmodule/novorbis.c`).
- **Descriptors** are written by `common/mkdtdesc.py`, which reproduces the
  system's own `DEVS:DataTypes/PNG` byte for byte.

## Tested

On AmigaOS 3.2.3 on AmigaChrome's AC090 emulation (68040 with FPU, 256 MB),
Instance-24, 4 October 2026, with test files made by `tests/make-media.sh`.

`tests/dtpic.c` opens each picture through datatypes.library and reads it
back as ARGB. Its checksums equal `tests/webpraw.c` decoding the same files
with libwebp on x86 cores:

```
DH1:OB/webp/lossy.webp 64 48 alpha=0 24b3d77f
DH1:OB/webp/lossless-alpha.webp 40 30 alpha=1 65a9a948
DH1:OB/webp/lossy-alpha.webp 50 20 alpha=1 ca376736
DH1:OB/webp/anim.webp 32 32 alpha=0 02bb8100
```

`tests/dtanim.c` opens each clip as an animation and loads every frame with
`ADTM_LOADFRAME`. All 60 frames (30 VP8, 30 VP9, 160x120 at 10 frames a
second) have the same 256-colour pixels as `tests/webmprobe.c -chunky` on
x86 cores:

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

### openmodule.datatype

On AmigaOS 3.2.3 on AC090 (68040 with FPU), the modlab scratch copy of an
OpenUp install, 8 October 2026, with modules made by
`tests/make-modules.py` (synthesised samples, a 30-second tune; checked on
x86 cores first by `tests/modbench.c` built against libxmp for the host,
which renders each to a WAV file). `tests/dtsound.c` opens each through
datatypes.library and plays it as OpenPlay does (`DTM_TRIGGER`: play,
pause, play, stop), while the x86 cores record the instance's Paula output:

```
mods/mod.ot-4ch: datatype=openmodule descriptor="ProTracker module" name="openmodule 4ch test" kind="Protracker M.K." frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/mod.ot-8ch: datatype=openmodule descriptor="FastTracker MOD" name="openmodule 8ch test" kind="Scream Tracker 8CHN" frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/mod.ot-st15: datatype=openmodule descriptor="SoundTracker module" name="openmodule st15" kind="unknown tracker 15 instrument" frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/med.ot-mmd0: datatype=openmodule descriptor="MED module" name="openmodule MMD0 test" kind="OctaMED 3.00 MMD0" frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/ot-okt.okta: datatype=openmodule descriptor="Oktalyzer module" name="ot-okt.okta" kind="Oktalyzer" frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/ot-xm.xm: datatype=openmodule descriptor="FastTracker 2 module" name="openmodule XM test" kind="openmodule tests XM 1.04" frames=857948 rate=27928 period=127 sample=NULL seconds=30
mods/mod.random: NEWDTOBJECT_FAIL ioerr=2008
```

(`ot-xm.xm` with opensound.datatype installed too; `mod.random` is 3000
random bytes, refused as invalid data.) Each recording has sound where it
should, silence through the second's pause, and the same spectrum as the
x86 cores' render of the same module at the same rate (correlation of the log
band energies 1.00 for all six; left and right in the same balance; the
loudness over time correlates 0.6 to 0.9 with the x86 cores'). Setting
`SDTA_Volume` to 16 while it plays brings the level to 0.25 of what it
was at once; a song left to play to its end signals `SDTA_SignalTask`
after its 30.7 seconds. In OpenPlay the module plays as soon as it opens,
the status line names `openmodule.datatype - 27928 Hz - ProTracker
module` and the bar shows 0:30; in MultiView it shows the sound icon and
plays when clicked.

`work/modbench` (10 seconds of each module, 8-bit stereo, as the datatype
mixes; share of one AC090 CPU):

| Module | Voices | 28 kHz linear | 28 kHz nearest | 16 kHz nearest |
| --- | --- | --- | --- | --- |
| ProTracker M.K. | 4 | 22.6 % | 12.4 % | 8.9 % |
| FastTracker 8CHN MOD | 8 | 33.9 % | 14.8 % | 12.2 % |
| OctaMED MMD0 | 4 | 21.7 % | | 8.4 % |
| Oktalyzer | 4 | 24.6 % | | 8.7 % |
| FastTracker 2 XM | 6 | 29.4 % | | 10.6 % |

(The table above is libxmp alone, mixing 8-bit as for Paula.)

Through AHI, with the ladder (`openmodule/DESIGN.md` sections 2 and 7),
on the same copy with the cores board fitted and AHI 6.6 with ACAHI's
Host mix on unit 0 at 44100 Hz, the test modules and a 32-channel XM,
`tests/dtsound.c LOAD=1`, two runs: chosen without being told, every
module plays on a core (4-74 % of the sound's time a job) and playing
takes 0-5 % of the main CPU. Forced onto `media.decode/1` (2 s pieces,
83-469 ms a call) 0-6 %; forced onto this CPU, 19-38 % for 4 to 8
voices and 87 % for the 32-channel XM, which breaks up. Recorded from
ACAHI's ring, every other case has no gaps but the pause the test gives,
and the spectrum and stereo balance of libxmp's render on x86 cores.
Without AHI (`ENV:OpenImage/ModuleOutput` `paula`) it plays through Paula
as before, and says so. OpenPlay's status line and Info window name the
rung, the output and the numbers.

Not yet checked: real Amiga hardware (a stock 68020 above all, where the
16 kHz default is a guess), DigiBooster, S3M, IT and the MED 2 to 4 and
StarTrekker descriptors with real files (their masks are libxmp's own
tests), modules from the wider world, `DTA_Repeat` and `DTA_Immediate`,
the ladder over a paired Cradle on the LAN, AHI modes other than ACAHI's
Host mix, and AHI on real hardware.

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
- Pictures in JPEG, PNG and GIF stay with the system's datatypes;
  `media.decode/1` decodes them for OpenBrowser.
- AmigaOS 3.2.3's datatypes.library tries a longer mask first and, between
  equal masks, priority 0 before higher ones (`openmodule/DESIGN.md`), so
  the priority 1 given to APNG, MKV, Camera RAW, SVG, CSV and Markdown
  above does not put them first there; not yet re-checked for those.
- `openmodule.datatype`: PowerPacker and XPK packed modules don't open
  (libxmp's depackers are left out), nor XM files with Ogg Vorbis samples.
  A file named `mod.something` or `something.mod` that no other datatype
  knows is offered to it and refused as invalid data. Through AHI, a
  volume change is heard from the next half-second request, and a pause
  skips what was queued (up to a second). Through Paula (no AHI) it is
  8-bit, and each module playing takes two of Paula's four channels, so two
  can play at once and a third stays silent. Saving writes the module as
  it came; there is no copy to the clipboard. libxmp's floating point goes through the ROM math libraries, in
  the player process only, and in a cores board job too (library code read
  where it is: it works, but OpenMulticore's rules say no library calls,
  and a strict job over the LAN could not do it). The rung is chosen when a
  module opens. openmulticore.library 0.1 built with os32-gcc16
  passes jobs the wrong arguments (OMCTest's first job ran for minutes,
  even on the main CPU); built as its `build.sh` says, with the os32 stove
  (GCC 6.5), OMCTest passes and openmodule's jobs run.
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

The datatype code (`build.sh`, `common/`, `include/`, `webp/`, `webm/`, `openpicture/`, `opensound/`, `openmodule/`, `opendoc/`, `openvideo/`, `tests/`) is
MIT, Copyright (c) 2026 Dalsin Limited, as the rest of this repository.
libwebp and libvpx keep their BSD licences and the WebM Project's patent
grants (`../upstream/libwebp/`, `../upstream/libvpx/`); a patch to their
source stays under their licence. libxmp, built unchanged into
`openmodule.datatype`, keeps its MIT licence (Copyright (C) 1996-2026
Claudio Matsuoka and Hipolito Carraro Jr; `../upstream/libxmp/COPYING`,
and `CREDITS` for the code it carries from others).
