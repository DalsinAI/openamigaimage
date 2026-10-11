/*
 * openpng.datatype: PNG pictures for AmigaOS 3.x, a picture.datatype (V43
 * mode) subclass that decodes on the Amiga itself (pngdecode.c, our own
 * inflate; no zlib or libpng). Every PNG colour type, bit depth,
 * transparency and Adam7 interlacing. The pixels go to picture.datatype as
 * 32-bit ARGB (bmh_Depth 32), so a program reading them back as ARGB gets
 * them as they were decoded, alpha included.
 *
 * A read or write of the pixels with no pixel array (pbpa_PixelData NULL)
 * is refused here rather than passed on. That is what a program gets when it
 * sends PDTM_READPIXELARRAY with DoDTMethodA() instead of DoMethodA():
 * DoDTMethodA puts a GadgetInfo (NULL with no window) in the message's
 * second longword, which for this method is the pixel array, and
 * picture.datatype then writes the picture from address 0 up through Chip
 * RAM. OpenBench 0.1 and a test tool did that on AmigaChrome's Instance-32
 * (10 and 11 October 2026); this class answers FALSE instead.
 *
 * DEVS:DataTypes/OpenPNG (build.sh) matches the PNG signature and the IHDR
 * that must follow it: a longer mask than the system's PNG descriptor, which
 * AmigaOS 3.2.3's datatypes.library tries first, so this class takes PNGs
 * while the system's png.datatype stays installed and untouched. Removing
 * the descriptor gives PNGs back to it.
 *
 * The colours (png_rows: each row to ARGB, and Adam7's placing) are shared
 * between the cores of a cores board in bands of rows, with omc_bands()
 * from openmulticore (include/openmulticore/bands.h, copied in), when the
 * picture is big enough to gain (OPENPNG_BANDS_MIN: 8 megapixels, while
 * a job costs 1 to 3 ms to send on Instance-32's board). With no
 * openmulticore.library or no board, or after any error on a core, the rows
 * are done here: the same bytes either way. Inflate and the filters stay on
 * one core, as each row needs the one before (11 October 2026, the Team).
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
#ifndef OPENPNG_NO_OMC
#include <openmulticore/bands.h>
#endif

/* Pictures with fewer pixels than this keep their colours on this CPU.
 * Measured on Instance-32 (PNGBench, 11 October 2026): the colours of a
 * 1920 x 1080 picture take 8 to 12 ms on the main CPU, while sending eight
 * bands to the cores board and collecting them costs 15 to 20 ms, so only
 * very big pictures gain until a job costs less to send. */
#define OPENPNG_BANDS_MIN (4096UL * 2048UL)

const char LibName[] = "openpng.datatype";
const char LibIdString[] = "openpng.datatype 47.4 (11.10.2026) Dalsin Limited";
const UWORD LibVersion = 47;
const UWORD LibRevision = 4;
static const char version[] __attribute__((used)) = "$VER: openpng.datatype 47.4 (11.10.2026)";

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

/* The colours from raw into argb: in bands on the cores when there are
 * some and the picture is big, else here. */
static void colours(struct png_conv *c)
{
#ifndef OPENPNG_NO_OMC
    if (c->h->width * c->h->height >= OPENPNG_BANDS_MIN) {
        omc_bands(NULL, png_rows, c, c->h->height, c->argb, c->h->width * 4, 0, NULL);
        return;
    }
#endif
    png_rows(c, 0, c->h->height);
}

/* The header and the colour stage's arguments, in Fast RAM when there is
 * any: a band on a core reads them. */
struct png_state {
    struct png_head h;
    struct png_conv c;
};

static BOOL loadPNG(Class *cl, Object *o)
{
    struct BitMapHeader *bmh = NULL;
    struct png_state *st;
    struct png_head *h;
    STRPTR name = NULL;
    UBYTE *file, *idat = NULL, *raw = NULL, *argb_mem = NULL;
    ULONG *argb = NULL, size = 0;
    APTR work = NULL;
    LONG err = ERROR_NO_FREE_STORE;
    BOOL ok = FALSE;

    if (GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, DTA_Name, (ULONG)&name, TAG_DONE) < 1 || !bmh) {
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }
    if (!(st = big_alloc(sizeof(*st))))
        return FALSE;
    h = &st->h;
    if (!(file = dt_read_source(o, &size))) {
        err = IoErr();
        FreeVec(st);
        SetIoErr(err);
        return FALSE;
    }
    if (png_read_head(file, size, h)) {
        FreeVec(file);
        FreeVec(st);
        SetIoErr(DTERROR_INVALID_DATA);
        return FALSE;
    }
    if ((idat = big_alloc(h->idat_len)))
        png_gather(file, size, idat);
    FreeVec(file);
    /* argb starts on a 16-byte line of its own, so the cores can take bands of it (bands.h) */
    if (idat && (raw = big_alloc(h->raw_len)) && (argb_mem = big_alloc(h->width * h->height * 4 + 15))
        && (work = AllocVec(PNG_WORKSIZE, MEMF_ANY))) {
        argb = (ULONG *)(((ULONG)argb_mem + 15) & ~15UL);
        if ((st->c.got = png_unpack(h, idat, raw, work))) {
            st->c.h = h;
            st->c.raw = raw;
            st->c.argb = argb;
            colours(&st->c);
            ok = TRUE;
        } else
            err = DTERROR_INVALID_DATA;
    }
    if (work) FreeVec(work);
    if (raw) FreeVec(raw);
    if (idat) FreeVec(idat);

    if (ok) {
        bmh->bmh_Width = bmh->bmh_PageWidth = (UWORD)h->width;
        bmh->bmh_Height = bmh->bmh_PageHeight = (UWORD)h->height;
        bmh->bmh_Depth = 32;           /* ARGB, alpha or not */
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
    if (argb_mem) FreeVec(argb_mem);
    FreeVec(st);
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
    case PDTM_READPIXELARRAY:
    case PDTM_WRITEPIXELARRAY:
        if (!((struct pdtBlitPixelArray *)msg)->pbpa_PixelData)
            return FALSE;              /* no pixel array (sent with DoDTMethodA?): see the top */
        return DoSuperMethodA(cl, o, msg);
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}
