# Restart: openamigaimage (OpenImage datatypes)

_Written 6 October 2026 at about 23:55 UTC, while all work is paused on @SacredTrees's word (23:28 UTC). Read this first when work resumes; the newest capsule and the live PR list win if they disagree._

## What this repo is

zlib, libpng, libjpeg, libwebp-based and modern datatypes for AmigaOS 3.x: about 40 formats (AVIF, HEIC, JXL, FLAC, Opus, office documents, MP4/MKV video, SVG and more), most decoded on the Cradle host through the Nursery services. Ships as OpenUp's OpenImage part.

## Where it stands

Films are fixed: test.mkv plays to the end with sound at the right rate and closes in about a second (#10, with the Paula fix in amigachrome #200). zlib/libpng/libjpeg use AC090's native helpers (#11). About 40 of 49 planned formats are built. Office formats have not been tried on an instance yet.

## Merged lately

- #12 (e143fb4, 2026-10-06): Credit who made openamigaimage: CONTRIBUTORS.md
- #11 (ea58265, 2026-10-06): zlib, libpng and libjpeg use AC090's native helpers for checksums and big copies
- #10 (5795bc8, 2026-10-06): openvideo, webm: hand animation.datatype the sound's period with every frame
- #9 (c6c6b55, 2026-10-06): openvideo: take out the VideoSilence and VideoKeyFrame trial switches
- #8 (03bfd17, 2026-10-06): openvideo: keep the sound track in chip RAM and log what the host gave
- #7 (47380bd, 2026-10-06): openvideo: trial switch giving silent films a frame of silence

## Open pull requests

- None.

## Next step

1. Test the office formats on an instance.
2. Build ICO, CBR, MOBI, WOFF and animated WebP, then the 68k-native fallbacks.
3. Check the native helpers in a 68k build on the home PC (tags present, PNG timing with AC_HELPERS on and off).

## Waiting on @SacredTrees

- The TeX datatype and GPX stay parked until a go.

## Who owns it

Datatypes thread; AC090 thread for the helpers.

## Capsules

Restart capsules for this repo's workstreams, in amigachrome's `capjumps/` shelf:

- [`20261006_AmigaChrome_Datatypes_OpenPlay_Restart_Capsule.zip`](https://github.com/DalsinAI/amigachrome/tree/main/capjumps)
- [`20261006_AmigaChrome_AC090_JIT_OpenGfx_Restart_Capsule.zip`](https://github.com/DalsinAI/amigachrome/tree/main/capjumps)

Team rules that still hold: commits as SacredTrees with no co-author lines; third-party code only on "yes with review" (licence checked, commit and sha256 pinned, fetched at build, never committed); deploys with deploy_dev.py only, on a typed line.
