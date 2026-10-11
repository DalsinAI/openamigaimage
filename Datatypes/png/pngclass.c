/*
 * openpng.datatype: PNG pictures for AmigaOS 3.x, a picture.datatype (V43
 * mode) subclass that decodes on the Amiga itself (pngdecode.c, our own
 * inflate; no zlib or libpng). Every PNG colour type, bit depth,
 * transparency and Adam7 interlacing.
 *
 * Why it is here: on AmigaChrome's Instance-32 (11 October 2026), asking
 * AmigaOS 3.2.3's png.datatype and picture.datatype for a picture's pixels
 * as ARGB (PDTM_READPIXELARRAY, PBPAFMT_ARGB) crashed the machine inside
 * picture.datatype's 24-bit to ARGB converter. This class always hands the
 * superclass 32-bit ARGB (bmh_Depth 32), so the superclass keeps ARGB and a
 * read as ARGB is a plain copy; that converter is never reached.
 *
 * DEVS:DataTypes/OpenPNG (build.sh) matches the PNG signature and the IHDR
 * that must follow it: a longer mask than the system's PNG descriptor, which
 * AmigaOS 3.2.3's datatypes.library tries first, so this class takes PNGs
 * while the system's png.datatype stays installed and untouched. Removing
 * the descriptor gives PNGs back to it.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <intuition/classes.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <clib/alib_protos.h>

#include "dtlib.h"
#include "pngdecode.h"

const char LibName[] = "openpng.datatype";
const char LibIdString[] = "openpng.datatype 47.1 (11.10.2026) Dalsin Limited";
const UWORD LibVersion = 47;
const UWORD LibRevision = 1;
static const char version[] __attribute__((used)) = "$VER: openpng.datatype 47.1 (11.10.2026)";

const char dt_superclass[] = "picture.datatype";
const UWORD dt_superversion = 43;
ULONG dt_instsize = 0;

BOOL dt_init(void)
{
    return TRUE;
}

void dt_cleanup(void)
{
}

/* Fast RAM when there is any: the decode never needs Chip RAM (and a job on
 * another core may not touch it). */
static APTR big_alloc(ULONG n)
{
    APTR p = AllocVec(n, MEMF_FAST);
    return p ? p : AllocVec(n, MEMF_ANY);
}

static BOOL loadPNG(Class *cl, Object *o)
{
    struct BitMapHeader *bmh = NULL;
    struct png_head *h;
    STRPTR name = NULL;
    UBYTE *file, *idat = NULL, *raw = NULL;
    ULONG *argb = NULL, size = 0;
    APTR work = NULL;
    LONG err = ERROR_NO_FREE_STORE;
    BOOL ok = FALSE;

    if (GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, DTA_Name, (ULONG)&name, TAG_DONE) < 1 || !bmh) {
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }
    if (!(h = AllocVec(sizeof(*h), MEMF_ANY)))
        return FALSE;
    if (!(file = dt_read_source(o, &size))) {
        err = IoErr();
        FreeVec(h);
        SetIoErr(err);
        return FALSE;
    }
    if (png_read_head(file, size, h)) {
        FreeVec(file);
        FreeVec(h);
        SetIoErr(DTERROR_INVALID_DATA);
        return FALSE;
    }
    if ((idat = big_alloc(h->idat_len)))
        png_gather(file, size, idat);
    FreeVec(file);
    if (idat && (raw = big_alloc(h->raw_len)) && (argb = big_alloc(h->width * h->height * 4))
        && (work = AllocVec(PNG_WORKSIZE, MEMF_ANY))) {
        if (png_decode(h, idat, raw, argb, work) == 0)
            ok = TRUE;
        else
            err = DTERROR_INVALID_DATA;
    }
    if (work) FreeVec(work);
    if (raw) FreeVec(raw);
    if (idat) FreeVec(idat);

    if (ok) {
        bmh->bmh_Width = bmh->bmh_PageWidth = (UWORD)h->width;
        bmh->bmh_Height = bmh->bmh_PageHeight = (UWORD)h->height;
        bmh->bmh_Depth = 32;           /* always ARGB to the superclass (see the top) */
        bmh->bmh_Masking = h->alpha ? mskHasAlpha : mskNone;
        SetDTAttrs(o, NULL, NULL,
            DTA_ObjName, (ULONG)(name ? FilePart(name) : (STRPTR)"PNG"),
            DTA_NominalHoriz, h->width,
            DTA_NominalVert, h->height,
            PDTA_SourceMode, PMODE_V43,
            PDTA_AlphaChannel, h->alpha ? TRUE : FALSE,
            TAG_DONE);
        ok = DoSuperMethod(cl, o, PDTM_WRITEPIXELARRAY, (ULONG)argb, PBPAFMT_ARGB, h->width * 4,
            0, 0, h->width, h->height) ? TRUE : FALSE;
        if (!ok)
            err = ERROR_NO_FREE_STORE;
    }
    if (argb) FreeVec(argb);
    FreeVec(h);
    if (!ok)
        SetIoErr(err);
    return ok;
}

ULONG dt_dispatch(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID) {
    case OM_NEW: {
        Object *obj = (Object *)DoSuperMethodA(cl, o, msg);
        if (obj && !loadPNG(cl, obj)) {
            LONG err = IoErr();
            CoerceMethod(cl, obj, OM_DISPOSE);
            SetIoErr(err);
            obj = NULL;
        }
        return (ULONG)obj;
    }
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}
