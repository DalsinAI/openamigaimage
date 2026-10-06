/*
 * openvideo.datatype: MP4, MOV, MKV, AVI, WMV and MPEG video (H.264, HEVC,
 * AV1, VP9, MPEG-4, MPEG-1/2, WMV) for AmigaOS 3.x, an animation.datatype
 * subclass. The media.decode/1 service (DalsinAI/openamigaservice
 * docs/MEDIA_DECODE.md) on the services card or a paired Cradle keeps the
 * video open and sends each frame in 256 colours (webm.datatype's dithered
 * colour cube), scaled to fit ENV:OpenImage/VideoWidth x VideoHeight
 * (default 640 x 480). The sound comes the same way, as 8-bit mono handed
 * out with each frame.
 *
 * Without a card or a Cradle the video does not open: these codecs are far
 * too slow for a 68k.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <string.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/var.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <datatypes/animationclass.h>
#include <intuition/classes.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <clib/alib_protos.h>

#include "dtlib.h"
#include "dtservice.h"

const char LibName[] = "openvideo.datatype";
const char LibIdString[] = "openvideo.datatype 47.1 (5.10.2026) Dalsin Limited, decoded by media.decode/1";
const UWORD LibVersion = 47;
const UWORD LibRevision = 1;
static const char version[] __attribute__((used)) = "$VER: openvideo.datatype 47.1 (5.10.2026)";

const char dt_superclass[] = "animation.datatype";
const UWORD dt_superversion = 40;

/* media.decode/1 (docs/MEDIA_DECODE.md) */
#define MD_PROBE  1
#define MD_DECODE 2
#define MD_VOPEN  3
#define MD_VFRAME 4
#define MD_VCLOSE 5
#define MD_KIND_ANIMATION 2
#define MD_KIND_SOUND 3
#define MD_FLAG_SOUND 2
#define MD_FLAG_LOOP 4

#ifndef DTA_Repeat
#define DTA_Repeat (DTA_Dummy + 302)
#endif

/* The PAL colour clock, for periods. */
#define PAL_CLOCK 3546895UL

typedef struct {
    ULONG handle;                  /* the host's, from VOPEN; 0 when none */
    ULONG width, height, frames;
    struct BitMap *keyFrame;
    BYTE *sound;                   /* 8-bit mono for the whole video, or NULL */
    ULONG soundLength, soundPerFrame;
} VideoData;

ULONG dt_instsize = sizeof(VideoData);

BOOL dt_init(void)
{
    return TRUE;
}

void dt_cleanup(void)
{
}

static ULONG get32(const UBYTE *p)
{
    return (ULONG)p[0] << 24 | (ULONG)p[1] << 16 | (ULONG)p[2] << 8 | p[3];
}

static ULONG envNumber(const char *name, ULONG fallback, ULONG least)
{
    char buf[16];
    LONG n = GetVar((CONST_STRPTR)name, (STRPTR)buf, sizeof buf, GVF_GLOBAL_ONLY);
    ULONG v = 0;
    LONG i;

    for (i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
        v = v * 10 + (buf[i] - '0');
    return v >= least ? v : fallback;
}

/* The same 256 colours as webm.datatype: the 6x6x6 cube, then a grey ramp. */
static void setPalette(Object *o)
{
    struct ColorRegister *cmap = NULL;
    ULONG *cregs = NULL, *gregs = NULL;
    int i;

    SetDTAttrs(o, NULL, NULL, ADTA_NumColors, 256, TAG_DONE);
    GetDTAttrs(o, ADTA_ColorRegisters, (ULONG)&cmap, ADTA_CRegs, (ULONG)&cregs, ADTA_GRegs, (ULONG)&gregs, TAG_DONE);
    for (i = 0; i < 256; i++) {
        UBYTE r, g, b;
        if (i < 216) {
            r = (i / 36) * 51;
            g = ((i / 6) % 6) * 51;
            b = (i % 6) * 51;
        } else
            r = g = b = (UBYTE)((i - 216) * 255 / 39);
        if (cmap) {
            cmap[i].red = r;
            cmap[i].green = g;
            cmap[i].blue = b;
        }
        if (cregs) {
            cregs[i * 3] = r * 0x01010101UL;
            cregs[i * 3 + 1] = g * 0x01010101UL;
            cregs[i * 3 + 2] = b * 0x01010101UL;
        }
        if (gregs) {
            gregs[i * 3] = r * 0x01010101UL;
            gregs[i * 3 + 1] = g * 0x01010101UL;
            gregs[i * 3 + 2] = b * 0x01010101UL;
        }
    }
}

/* Frame index as a new 8-bit bitmap, from the host. Each call opens the
 * service itself: animation.datatype loads frames from its own process.
 * NULL with *nomem set when the memory ran out. */
static struct BitMap *fetchFrame(VideoData *d, ULONG index, BOOL *nomem)
{
    struct dt_service svc;
    struct OSBuffer buf[4];
    ULONG extra[4] = { index, 0, 0, 0 };
    struct BitMap *bm = NULL;
    struct RastPort rp;
    UBYTE *chunky;

    *nomem = FALSE;
    if (!(chunky = AllocVec(d->width * d->height, MEMF_ANY))) {
        *nomem = TRUE;
        return NULL;
    }
    if (dt_service_open(&svc, "media.decode/1")) {
        memset(buf, 0, sizeof buf);
        buf[1].ob_Data = chunky;
        buf[1].ob_Length = d->width * d->height;
        if (dt_service_call(&svc, MD_VFRAME, d->handle, 2, buf, extra, NULL, NULL) == OSERR_OK) {
            if ((bm = AllocBitMap(d->width, d->height, 8, BMF_CLEAR, NULL)) != NULL) {
                InitRastPort(&rp);
                rp.BitMap = bm;
                WriteChunkyPixels(&rp, 0, 0, d->width - 1, d->height - 1, chunky, d->width);
            } else
                *nomem = TRUE;
        }
        dt_service_close(&svc);
    }
    FreeVec(chunky);
    return bm;
}

/* The sound track as 8-bit mono at no more than 28 kHz; the rate, or 0. */
static ULONG fetchSound(VideoData *d, struct dt_service *svc, UBYTE *file, ULONG size)
{
    struct OSBuffer buf[4];
    ULONG extra[4] = { 1, 28000, 0, 0 }, frames, got = 0, i, rate = 0;
    UBYTE info[24], *pcm;

    memset(buf, 0, sizeof buf);
    buf[0].ob_Data = file;
    buf[0].ob_Length = size;
    buf[1].ob_Data = info;
    buf[1].ob_Length = sizeof info;
    if (dt_service_call(svc, MD_PROBE, 0, 2, buf, extra, NULL, NULL) != OSERR_OK || get32(info) != MD_KIND_SOUND
        || !(frames = get32(info + 12)) || frames >= 0x40000000UL || get32(info + 20) != 1
        || !(pcm = AllocVec(frames * 2, MEMF_ANY)))
        return 0;
    buf[1].ob_Data = pcm;
    buf[1].ob_Length = frames * 2;
    if (dt_service_call(svc, MD_DECODE, 0, 2, buf, extra, &got, NULL) == OSERR_OK && got) {
        for (i = 0; i < got; i++)                   /* the high byte of each 16-bit sample */
            pcm[i] = pcm[i * 2];
        d->sound = (BYTE *)pcm;
        d->soundLength = got;
        rate = get32(info + 16);
    } else
        FreeVec(pcm);
    return rate;
}

static void freeData(VideoData *d)
{
    struct dt_service svc;
    struct OSBuffer buf[4];
    ULONG extra[4] = { 0, 0, 0, 0 };

    if (d->handle && dt_service_open(&svc, "media.decode/1")) {
        memset(buf, 0, sizeof buf);
        dt_service_call(&svc, MD_VCLOSE, d->handle, 0, buf, extra, NULL, NULL);
        dt_service_close(&svc);
    }
    d->handle = 0;
    if (d->keyFrame) {
        WaitBlit();
        FreeBitMap(d->keyFrame);
    }
    d->keyFrame = NULL;
    if (d->sound)
        FreeVec(d->sound);
    d->sound = NULL;
}

static BOOL loadVideo(Class *cl, Object *o)
{
    VideoData *d = INST_DATA(cl, o);
    struct dt_service svc;
    struct OSBuffer buf[4];
    ULONG extra[4], size = 0, handle = 0, fps1000 = 0, fps, rate;
    UBYTE info[24], *data;
    STRPTR name = NULL;
    BOOL ok = FALSE, nomem;
    LONG err = DTERROR_INVALID_DATA;

    memset(d, 0, sizeof *d);
    GetDTAttrs(o, DTA_Name, (ULONG)&name, TAG_DONE);
    if (!(data = dt_read_source(o, &size)))
        return FALSE;
    if (!dt_service_open(&svc, "media.decode/1")) {
        FreeVec(data);
        SetIoErr(ERROR_NOT_IMPLEMENTED);       /* no card and no paired Cradle */
        return FALSE;
    }
    memset(buf, 0, sizeof buf);
    extra[0] = envNumber("OpenImage/VideoWidth", 640, 16);
    extra[1] = envNumber("OpenImage/VideoHeight", 480, 16);
    /* Each frame is a planar bitmap in chip RAM, and animation.datatype
     * keeps a few: ask for a smaller picture when chip RAM is short, so a
     * third or fourth video still opens. */
    while (extra[0] > 160 && AvailMem(MEMF_CHIP | MEMF_LARGEST) < 4 * extra[0] * extra[1]) {
        extra[0] /= 2;
        extra[1] /= 2;
    }
    extra[2] = extra[3] = 0;
    buf[0].ob_Data = data;
    buf[0].ob_Length = size;
    buf[1].ob_Data = info;
    buf[1].ob_Length = sizeof info;
    if (dt_service_call(&svc, MD_VOPEN, 0, 2, buf, extra, &handle, &fps1000) != OSERR_OK
        || get32(info) != MD_KIND_ANIMATION)
        goto out;
    d->handle = handle;
    d->frames = get32(info + 12);
    d->width = get32(info + 16);
    d->height = get32(info + 20);
    if (!d->frames || !d->width || !d->height || d->width > 4096 || d->height > 4096)
        goto out;
    fps = (fps1000 + 500) / 1000;
    if (!fps)
        fps = 1;
    if ((get32(info + 8) & MD_FLAG_SOUND) && (rate = fetchSound(d, &svc, data, size)) != 0) {
        d->soundPerFrame = rate * 1000 / (fps1000 ? fps1000 : 25000);
        SetDTAttrs(o, NULL, NULL, ADTA_Period, PAL_CLOCK / rate, ADTA_Volume, 64, ADTA_Cycles, 1, TAG_DONE);
    }
    if (!(d->keyFrame = fetchFrame(d, 0, &nomem))) {
        if (nomem)
            err = ERROR_NO_FREE_STORE;
        goto out;
    }

    setPalette(o);
    SetDTAttrs(o, NULL, NULL,
        DTA_ObjName, (ULONG)(name ? FilePart(name) : (STRPTR)"Video"),
        DTA_NominalHoriz, d->width,
        DTA_NominalVert, d->height,
        ADTA_Width, d->width,
        ADTA_Height, d->height,
        ADTA_Depth, 8,
        ADTA_Frames, d->frames,
        ADTA_FramesPerSecond, fps,
        ADTA_KeyFrame, (ULONG)d->keyFrame,
        /* an animated GIF or PNG that asks to play more than once */
        DTA_Repeat, (get32(info + 8) & MD_FLAG_LOOP) ? TRUE : FALSE,
        TAG_DONE);
    ok = TRUE;
out:
    dt_service_close(&svc);
    FreeVec(data);
    if (!ok) {
        freeData(d);
        SetIoErr(err);
    }
    return ok;
}

ULONG dt_dispatch(Class *cl, Object *o, Msg msg)
{
    switch (msg->MethodID) {
    case OM_NEW: {
        Object *obj = (Object *)DoSuperMethodA(cl, o, msg);
        if (obj && !loadVideo(cl, obj)) {
            LONG err = IoErr();
            CoerceMethod(cl, obj, OM_DISPOSE);
            SetIoErr(err);
            obj = NULL;
        }
        return (ULONG)obj;
    }
    case OM_DISPOSE:
        freeData(INST_DATA(cl, o));
        return DoSuperMethodA(cl, o, msg);
    case ADTM_LOADFRAME: {
        VideoData *d = INST_DATA(cl, o);
        struct adtFrame *alf = (struct adtFrame *)msg;
        ULONG index = alf->alf_TimeStamp;
        struct BitMap *bm;
        BOOL nomem;
        if (index >= d->frames)
            index = d->frames - 1;
        bm = fetchFrame(d, index, &nomem);
        if (nomem)
            SetIoErr(ERROR_NO_FREE_STORE);
        alf->alf_Frame = index;
        alf->alf_Duration = 1;
        alf->alf_BitMap = bm;
        alf->alf_CMap = NULL;
        alf->alf_Sample = NULL;
        alf->alf_SampleLength = 0;
        if (d->sound && d->soundPerFrame && index * d->soundPerFrame < d->soundLength) {
            ULONG at = index * d->soundPerFrame, n = d->soundPerFrame;
            if (at + n > d->soundLength)
                n = d->soundLength - at;
            alf->alf_Sample = d->sound + at;   /* ours: freed with the object */
            alf->alf_SampleLength = n;
        }
        alf->alf_UserData = bm;
        return bm ? 1 : 0;
    }
    case ADTM_UNLOADFRAME: {
        struct adtFrame *alf = (struct adtFrame *)msg;
        if (alf->alf_UserData) {
            WaitBlit();
            FreeBitMap((struct BitMap *)alf->alf_UserData);
        }
        alf->alf_UserData = NULL;
        alf->alf_BitMap = NULL;
        return 0;
    }
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}
