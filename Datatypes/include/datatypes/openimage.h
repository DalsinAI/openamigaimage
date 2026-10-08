/*
 * openamigaimage datatypes: attributes of their own, for players and
 * viewers that want to say more than the standard ones do. A datatype that
 * doesn't know one answers OM_GET with FALSE, so a program asks and falls
 * back. TAG_USER with Dalsin's manufacturer number ($DA15) in the middle.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#ifndef DATATYPES_OPENIMAGE_H
#define DATATYPES_OPENIMAGE_H

#include <utility/tagitem.h>

#define OIA_Dummy      (TAG_USER + 0x0DA15000)

/* (STRPTR, G) What decodes or plays this object, in words for a status
 * line: "a cores board core, through AHI", "media.decode/1 (the Nursery),
 * through AHI", "this Amiga's CPU, through Paula (no AHI)". Valid while
 * the object lives; it may change once it plays (openmodule.datatype
 * moves to this CPU when a core fails it). */
#define OIA_DecodedBy  (OIA_Dummy + 1)

/* (STRPTR, G) How that has gone, measured: "22% of a 68040 at 27928 Hz",
 * "48 jobs on core 1, 3.1 ms each". For an info window or a log. */
#define OIA_Stats      (OIA_Dummy + 2)

/* (ULONG, G) openmodule.datatype: who mixes it, 1 this CPU, 2 a cores
 * board core, 3 a service (media.decode/1). (Numbers, not the order they
 * are tried in: a core, then the service, then this CPU.) */
#define OIA_Rung       (OIA_Dummy + 3)

#endif
