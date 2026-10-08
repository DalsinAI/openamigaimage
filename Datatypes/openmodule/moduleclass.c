/*
 * openmodule.datatype: music modules for AmigaOS 3.x, played on the Amiga
 * itself: ProTracker, NoiseTracker and SoundTracker MODs, MED and OctaMED,
 * Oktalyzer, DigiBooster, FastTracker 2, Scream Tracker 3 and Impulse
 * Tracker, and the other formats libxmp reads. A sound.datatype subclass.
 *
 * Streamed, not decoded whole: libxmp (MIT) mixes the module an eighth of
 * a second at a time into 8-bit stereo, and a player process of the
 * object's own keeps four such buffers queued on a left and a right Paula
 * channel through audio.device. sound.datatype V44 and newer describe this
 * as a streaming subclass: SDTA_Sample stays NULL, SDTA_SampleLength and
 * SDTA_SamplesPerSec give the length, and DTM_TRIGGER plays, pauses and
 * stops. See DESIGN.md beside this file for why it is done this way.
 *
 * All of libxmp's work (loading, and mixing, which uses some floating
 * point) happens in the player process, never in the caller's task, so
 * the ROM math libraries it calls on a 68k without an FPU only ever change
 * that process's FPU state.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited. libxmp keeps its MIT licence.
 */
#include <string.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <devices/audio.h>
#include <devices/timer.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <intuition/classes.h>
#include <intuition/gadgetclass.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/datatypes.h>
#include <clib/alib_protos.h>

#include <xmp.h>

#include "dtlib.h"

const char LibName[] = "openmodule.datatype";
const char LibIdString[] = "openmodule.datatype 47.1 (8.10.2026) Dalsin Limited, played by libxmp 4.7.3";
const UWORD LibVersion = 47;
const UWORD LibRevision = 1;
static const char version[] __attribute__((used)) = "$VER: openmodule.datatype 47.1 (8.10.2026)";

const char dt_superclass[] = "sound.datatype";
const UWORD dt_superversion = 39;

#ifdef OM_DEBUG
#include <stdarg.h>
int vsnprintf(char *buffer, unsigned long size, const char *format, va_list ap);
/* A trace (build with -DOM_DEBUG, and -DOM_DEBUG_LOG=\"file\" for another
 * place than T:openmodule.log); from processes only. */
#ifndef OM_DEBUG_LOG
#define OM_DEBUG_LOG "T:openmodule.log"
#endif
static void dlog(const char *fmt, ...)
{
    char line[160];
    va_list ap;
    BPTR f;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line - 1, fmt, ap);
    va_end(ap);
    strcat(line, "\n");
    if ((f = Open((CONST_STRPTR)OM_DEBUG_LOG, MODE_READWRITE))) {
        Seek(f, 0, OFFSET_END);
        Write(f, line, strlen(line));
        Close(f);
    }
}
#else
#define dlog(...) ((void)0)
#endif

/* Buffers queued on each channel, and how long each one plays. */
#define NBUF 4
#define CHUNK_DIV 8                    /* an eighth of a second */
#define PLAYER_STACK 65536
#define PLAYER_PRI 2

/* Player commands: PC_QUIT by message, the others by mi->want and
 * SIGBREAKF_CTRL_F, so they can be given from any task (a click on the
 * gadget comes from input.device's) without waiting; the latest wins. */
enum { PC_NONE, PC_PLAY, PC_PAUSE, PC_STOP, PC_REWIND, PC_QUIT };

/* Player states */
enum { PS_STOPPED, PS_PLAYING, PS_PAUSED, PS_DRAINING };

struct ModInst {
    struct Process *player;
    struct MsgPort *port;          /* the player's commands */
    UBYTE *data;                   /* the module file, kept for DTM_WRITE */
    ULONG size;
    ULONG rate, period, frames, ms;
    LONG interp;
    volatile UWORD volume;
    volatile UWORD repeat;
    volatile UWORD state;
    volatile UWORD want;
    UWORD immediate, started;
    struct Task *sigTask;
    ULONG sigMask;
    char title[XMP_NAME_SIZE + 1];
    char type[XMP_NAME_SIZE + 1];
};

ULONG dt_instsize = sizeof(struct ModInst);

struct PlayerMsg {
    struct Message pm_Msg;
    ULONG pm_Cmd;
    struct ModInst *pm_Inst;
    LONG pm_Result;
};

/* The CPU decides the defaults: 68040 and 68060 mix at 28000 Hz with
 * linear interpolation, the 68020 and 68030 at 16000 Hz without. */
static BOOL fastCpu;

static const struct DTMethod triggers[] = {
    {(STRPTR)"Play", (STRPTR)"PLAY", STM_PLAY},
    {(STRPTR)"Pause", (STRPTR)"PAUSE", STM_PAUSE},
    {(STRPTR)"Stop", (STRPTR)"STOP", STM_STOP},
    {NULL, NULL, 0}
};

BOOL dt_init(void)
{
    fastCpu = (SysBase->AttnFlags & (AFF_68040 | (1 << 7))) != 0;   /* bit 7: 68060 */
    return TRUE;
}

void dt_cleanup(void)
{
}

static ULONG envNumber(const char *name)
{
    char buf[16];
    LONG n = GetVar((CONST_STRPTR)name, (STRPTR)buf, sizeof buf, GVF_GLOBAL_ONLY);
    ULONG v = 0;
    LONG i;

    for (i = 0; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
        v = v * 10 + (buf[i] - '0');
    return v;
}

/* ENV:OpenImage/ModuleMix: nearest, linear or spline. */
static LONG envInterp(void)
{
    char buf[16];
    LONG n = GetVar((CONST_STRPTR)"OpenImage/ModuleMix", (STRPTR)buf, sizeof buf, GVF_GLOBAL_ONLY);
    if (n > 0) {
        if (buf[0] == 'n' || buf[0] == 'N' || buf[0] == '0')
            return XMP_INTERP_NEAREST;
        if (buf[0] == 'l' || buf[0] == 'L' || buf[0] == '1')
            return XMP_INTERP_LINEAR;
        if (buf[0] == 's' || buf[0] == 'S' || buf[0] == '2')
            return XMP_INTERP_SPLINE;
    }
    return fastCpu ? XMP_INTERP_LINEAR : XMP_INTERP_NEAREST;
}

/* --- the player process ------------------------------------------------------- */

/* The ROM math libraries libnix's soft floating point calls (xmpglue.c):
 * opened by each player process for itself. */
extern struct Library *MathIeeeDoubBasBase, *MathIeeeDoubTransBase, *MathIeeeSingBasBase;

struct Audio {
    struct MsgPort *port;
    struct IOAudio *ctl;
    struct IOAudio *req[NBUF][2];
    UBYTE *chip[NBUF][2];
    UBYTE busy[NBUF];
    UBYTE *mix;
    ULONG chunk;                   /* frames a buffer */
    UBYTE open, ended;
    UBYTE pervol;                  /* the next write sets period and volume */
};

/* Left and right: Paula's channels 0 and 3 play left, 1 and 2 right. */
static UBYTE allocMap[] = {3, 5, 10, 12};

static BOOL audioOpen(struct ModInst *mi, struct Audio *a)
{
    ULONG unit;
    int i, s;

    a->ctl->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    a->ctl->ioa_Request.io_Command = ADCMD_ALLOCATE;
    a->ctl->ioa_Request.io_Flags = ADIOF_NOWAIT;
    a->ctl->ioa_Data = allocMap;
    a->ctl->ioa_Length = sizeof allocMap;
    if (OpenDevice((CONST_STRPTR)AUDIONAME, 0, (struct IORequest *)a->ctl, 0))
        return FALSE;
    unit = (ULONG)a->ctl->ioa_Request.io_Unit;
    for (i = 0; i < NBUF; i++) {
        for (s = 0; s < 2; s++) {
            struct IOAudio *r = a->req[i][s];
            *r = *a->ctl;
            r->ioa_Request.io_Message.mn_ReplyPort = a->port;
            /* s 0: the left channel (0 or 3), s 1: the right (1 or 2) */
            r->ioa_Request.io_Unit = (struct Unit *)(unit & (s == 0 ? 9 : 6));
        }
        a->busy[i] = 0;
    }
    a->open = TRUE;
    a->ended = FALSE;
    (void)mi;
    return TRUE;
}

static void audioCommand(struct Audio *a, UWORD cmd)
{
    a->ctl->ioa_Request.io_Command = cmd;
    a->ctl->ioa_Request.io_Flags = 0;
    DoIO((struct IORequest *)a->ctl);
}

static void audioClose(struct Audio *a)
{
    int i, s;

    if (!a->open)
        return;
    /* CMD_FLUSH returns every write on both channels, playing or queued,
     * in one go inside audio.device; then each is collected. */
    dlog("close: flush");
    audioCommand(a, CMD_FLUSH);
    for (i = 0; i < NBUF; i++) {
        if (!a->busy[i])
            continue;
        for (s = 0; s < 2; s++)
            WaitIO((struct IORequest *)a->req[i][s]);
        a->busy[i] = 0;
    }
    dlog("close: reset");
    audioCommand(a, CMD_RESET);
    dlog("close: closedevice");
    CloseDevice((struct IORequest *)a->ctl);
    dlog("close: done");
    a->open = FALSE;
}

/* Mixes the next buffer into slot i and queues it. FALSE when the song is
 * over (nothing queued). */
static BOOL audioQueue(struct ModInst *mi, xmp_context ctx, struct Audio *a, int i)
{
    UBYTE *src = a->mix, *l = a->chip[i][0], *r = a->chip[i][1];
    ULONG n = a->chunk;
    int s;

    if (a->ended || xmp_play_buffer(ctx, a->mix, n * 2, mi->repeat ? 0 : 1) < 0) {
        a->ended = TRUE;
        return FALSE;
    }
    while (n--) {
        *l++ = *src++;
        *r++ = *src++;
    }
    for (s = 0; s < 2; s++) {
        struct IOAudio *q = a->req[i][s];
        /* Only the first write sets the period and volume: later ones
         * keep the channel's, so a change of volume (ADCMD_PERVOL) is
         * heard at once rather than after the queued buffers. */
        q->ioa_Request.io_Command = CMD_WRITE;
        q->ioa_Request.io_Flags = a->pervol ? ADIOF_PERVOL : 0;
        q->ioa_Data = a->chip[i][s];
        q->ioa_Length = a->chunk;
        q->ioa_Period = mi->period;
        q->ioa_Volume = mi->volume;
        q->ioa_Cycles = 1;
        BeginIO((struct IORequest *)q);
    }
    a->busy[i] = 1;
    a->pervol = FALSE;
    return TRUE;
}

/* Starts from where the song is: every buffer mixed and queued on stopped
 * channels, then both started together so left and right stay in step. */
static BOOL audioStart(struct ModInst *mi, xmp_context ctx, struct Audio *a)
{
    int i;

    if (!audioOpen(mi, a))
        return FALSE;
    audioCommand(a, CMD_STOP);
    a->pervol = TRUE;
    for (i = 0; i < NBUF; i++)
        if (!audioQueue(mi, ctx, a, i))
            break;
    audioCommand(a, CMD_START);
    return TRUE;
}

static void rewindSong(struct ModInst *mi, xmp_context ctx)
{
    xmp_end_player(ctx);
    xmp_start_player(ctx, mi->rate, XMP_FORMAT_8BIT);
    xmp_set_player(ctx, XMP_PLAYER_INTERP, mi->interp);
    xmp_play_buffer(ctx, NULL, 0, 0);
}

static void signalEnd(struct ModInst *mi)
{
    if (mi->sigTask && mi->sigMask)
        Signal(mi->sigTask, mi->sigMask);
}

static void playerMain(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct PlayerMsg *start, *pm;
    struct ModInst *mi;
    struct xmp_module_info info;
    struct Audio a;
    xmp_context ctx = NULL;
    LONG err = DTERROR_INVALID_DATA;
    UWORD lastVolume;
    int i, s;

    WaitPort(&me->pr_MsgPort);
    start = (struct PlayerMsg *)GetMsg(&me->pr_MsgPort);
    mi = start->pm_Inst;

    memset(&a, 0, sizeof a);
    MathIeeeDoubBasBase = OpenLibrary((CONST_STRPTR)"mathieeedoubbas.library", 34);
    MathIeeeDoubTransBase = OpenLibrary((CONST_STRPTR)"mathieeedoubtrans.library", 34);
    MathIeeeSingBasBase = OpenLibrary((CONST_STRPTR)"mathieeesingbas.library", 34);
    if (!MathIeeeDoubBasBase || !MathIeeeDoubTransBase || !MathIeeeSingBasBase) {
        err = ERROR_INVALID_RESIDENT_LIBRARY;
        goto fail;
    }
    if (!(mi->port = CreateMsgPort()) || !(a.port = CreateMsgPort())) {
        err = ERROR_NO_FREE_STORE;
        goto fail;
    }
    if (!(ctx = xmp_create_context())) {
        err = ERROR_NO_FREE_STORE;
        goto fail;
    }
    i = xmp_load_module_from_memory(ctx, mi->data, mi->size);
    if (i < 0) {
        err = i == -XMP_ERROR_SYSTEM ? ERROR_NO_FREE_STORE : DTERROR_INVALID_DATA;
        xmp_free_context(ctx);
        ctx = NULL;
        goto fail;
    }
    xmp_get_module_info(ctx, &info);
    strncpy(mi->title, info.mod->name, XMP_NAME_SIZE);
    strncpy(mi->type, info.mod->type, XMP_NAME_SIZE);
    mi->ms = info.seq_data[0].duration;
    if (xmp_start_player(ctx, mi->rate, XMP_FORMAT_8BIT) < 0)
        goto fail;
    xmp_set_player(ctx, XMP_PLAYER_INTERP, mi->interp);

    /* Buffers: chip RAM for Paula, an even number of bytes each. */
    a.chunk = (mi->rate / CHUNK_DIV) & ~1UL;
    a.ctl = (struct IOAudio *)CreateIORequest(a.port, sizeof(struct IOAudio));
    a.mix = AllocVec(a.chunk * 2, MEMF_ANY);
    if (!a.ctl || !a.mix) {
        err = ERROR_NO_FREE_STORE;
        goto fail;
    }
    for (i = 0; i < NBUF; i++)
        for (s = 0; s < 2; s++)
            if (!(a.req[i][s] = AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR)) ||
                !(a.chip[i][s] = AllocVec(a.chunk, MEMF_CHIP))) {
                err = ERROR_NO_FREE_STORE;
                goto fail;
            }

    start->pm_Result = 0;
    ReplyMsg(&start->pm_Msg);
    start = NULL;
    lastVolume = mi->volume;

    for (;;) {
        ULONG sigs = Wait(1UL << mi->port->mp_SigBit | 1UL << a.port->mp_SigBit | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
        BOOL kick = FALSE, quit = FALSE;
        UWORD cmd;
        /* The volume, at once on the playing channels. */
        if (mi->volume != lastVolume && a.open) {
            lastVolume = mi->volume;
            a.ctl->ioa_Request.io_Command = ADCMD_PERVOL;
            a.ctl->ioa_Request.io_Flags = 0;
            a.ctl->ioa_Period = mi->period;
            a.ctl->ioa_Volume = lastVolume;
            DoIO((struct IORequest *)a.ctl);
        }

        if (sigs & SIGBREAKF_CTRL_F) {
            Forbid();
            cmd = mi->want;
            mi->want = PC_NONE;
            Permit();
            dlog("player: command %ld in state %ld", (long)cmd, (long)mi->state);
            if (cmd == PC_REWIND || cmd == PC_STOP) {
                if (mi->state != PS_STOPPED) {
                    audioClose(&a);
                    rewindSong(mi, ctx);
                    mi->state = PS_STOPPED;
                }
                if (cmd == PC_REWIND)
                    cmd = PC_PLAY;
            }
            if (cmd == PC_PLAY) {
                if (mi->state == PS_PAUSED) {
                    audioCommand(&a, CMD_START);
                    mi->state = a.ended ? PS_DRAINING : PS_PLAYING;
                } else if (mi->state == PS_STOPPED)
                    kick = TRUE;
            } else if (cmd == PC_PAUSE) {
                if (mi->state == PS_PLAYING || mi->state == PS_DRAINING) {
                    audioCommand(&a, CMD_STOP);
                    mi->state = PS_PAUSED;
                }
            }
        }

        while ((pm = (struct PlayerMsg *)GetMsg(mi->port))) {
            dlog("player: message %ld", (long)pm->pm_Cmd);
            if (pm->pm_Cmd == PC_QUIT) {
                quit = TRUE;
                start = pm;                       /* replied on the way out */
                break;
            }
            pm->pm_Result = 0;
            ReplyMsg(&pm->pm_Msg);
        }
        if (quit)
            break;

        if (kick) {
            lastVolume = mi->volume;
            if (audioStart(mi, ctx, &a))
                mi->state = PS_PLAYING;
        }

        /* Buffers played: mix the next ones into them. */
        if (a.open && mi->state != PS_PAUSED) {
            BOOL any = FALSE;
            for (i = 0; i < NBUF; i++) {
                if (a.busy[i] && CheckIO((struct IORequest *)a.req[i][0]) && CheckIO((struct IORequest *)a.req[i][1])) {
                    BYTE e0 = WaitIO((struct IORequest *)a.req[i][0]);
                    BYTE e1 = WaitIO((struct IORequest *)a.req[i][1]);
                    a.busy[i] = 0;
                    if (e0 || e1) {
                        dlog("player: write error %ld %ld", (long)e0, (long)e1);
                        a.ended = TRUE;           /* channels taken away */
                    }
                    else
                        audioQueue(mi, ctx, &a, i);
                }
                if (a.busy[i])
                    any = TRUE;
            }
            if (a.ended && mi->state == PS_PLAYING)
                mi->state = PS_DRAINING;
            if (!any) {                           /* the song is over */
                dlog("player: song over");
                audioClose(&a);
                rewindSong(mi, ctx);
                mi->state = PS_STOPPED;
                signalEnd(mi);
            }
        }
    }

    dlog("player: quitting");
    audioClose(&a);
    err = 0;
fail:
    dlog("player: cleanup, err %ld", (long)err);
    if (ctx) {
        xmp_end_player(ctx);
        xmp_release_module(ctx);
        xmp_free_context(ctx);
    }
    for (i = 0; i < NBUF; i++)
        for (s = 0; s < 2; s++) {
            if (a.req[i][s])
                FreeVec(a.req[i][s]);
            if (a.chip[i][s])
                FreeVec(a.chip[i][s]);
        }
    if (a.mix)
        FreeVec(a.mix);
    if (a.ctl)
        DeleteIORequest((struct IORequest *)a.ctl);
    if (a.port)
        DeleteMsgPort(a.port);
    if (mi->port) {
        DeleteMsgPort(mi->port);
        mi->port = NULL;
    }
    dlog("player: freed");
    CloseLibrary(MathIeeeSingBasBase);
    CloseLibrary(MathIeeeDoubTransBase);
    CloseLibrary(MathIeeeDoubBasBase);
    dlog("player: gone");
    /* Forbid() until the process is gone: the code it runs is the
     * datatype's, which may be unloaded as soon as the reply is seen. */
    Forbid();
    if (start) {
        start->pm_Result = err;
        ReplyMsg(&start->pm_Msg);
    }
}

/* --- talking to the player ---------------------------------------------------- */

/* Asks the player to play, pause, stop or rewind, from any task. */
static void want(struct ModInst *mi, UWORD cmd)
{
    if (!mi->player)
        return;
    mi->want = cmd;
    Signal(&mi->player->pr_Task, SIGBREAKF_CTRL_F);
}

/* A message to the player, waiting for its answer (PC_QUIT). */
static LONG sendCommand(struct ModInst *mi, ULONG cmd)
{
    struct PlayerMsg pm;
    struct MsgPort *reply;

    if (!mi->port)
        return -1;
    if (!(reply = CreateMsgPort()))
        return -1;
    memset(&pm, 0, sizeof pm);
    pm.pm_Msg.mn_ReplyPort = reply;
    pm.pm_Msg.mn_Length = sizeof pm;
    pm.pm_Cmd = cmd;
    pm.pm_Inst = mi;
    dlog("send %ld", (long)cmd);
    PutMsg(mi->port, &pm.pm_Msg);
    WaitPort(reply);
    dlog("send %ld: answered", (long)cmd);
    GetMsg(reply);
    DeleteMsgPort(reply);
    return pm.pm_Result;
}

/* Reads the file, starts the player, which loads the module; FALSE (with
 * the reason in IoErr()) when libxmp can't. */
static BOOL loadModule(Class *cl, Object *o)
{
    struct ModInst *mi = INST_DATA(cl, o);
    struct PlayerMsg pm;
    struct MsgPort *reply;
    STRPTR name = NULL;
    ULONG rate;

    mi->interp = envInterp();
    rate = envNumber("OpenImage/SoundRate");
    if (rate < 4000)
        rate = fastCpu ? 28000 : 16000;
    /* Paula: the period is whole colour clocks, 124 at the least. */
    mi->period = (SysBase->ex_EClockFrequency * 5 + rate / 2) / rate;
    if (mi->period < 124)
        mi->period = 124;
    mi->rate = (SysBase->ex_EClockFrequency * 5 + mi->period / 2) / mi->period;

    GetDTAttrs(o, DTA_Name, (ULONG)&name, TAG_DONE);
    if (!(mi->data = dt_read_source(o, &mi->size)))
        return FALSE;
    if (!(reply = CreateMsgPort())) {
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }
    mi->player = CreateNewProcTags(NP_Entry, (ULONG)playerMain, NP_Name, (ULONG)"openmodule player",
                                   NP_StackSize, PLAYER_STACK, NP_Priority, PLAYER_PRI,
                                   NP_Input, 0, NP_Output, 0, NP_CloseInput, FALSE, NP_CloseOutput, FALSE,
                                   TAG_DONE);
    if (!mi->player) {
        DeleteMsgPort(reply);
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }
    memset(&pm, 0, sizeof pm);
    pm.pm_Msg.mn_ReplyPort = reply;
    pm.pm_Msg.mn_Length = sizeof pm;
    pm.pm_Inst = mi;
    PutMsg(&mi->player->pr_MsgPort, &pm.pm_Msg);
    WaitPort(reply);
    GetMsg(reply);
    DeleteMsgPort(reply);
    if (pm.pm_Result) {
        mi->player = NULL;                    /* it has gone */
        SetIoErr(pm.pm_Result);
        return FALSE;
    }

    mi->frames = mi->ms / 1000 * mi->rate + mi->ms % 1000 * mi->rate / 1000;
    SetDTAttrs(o, NULL, NULL,
        DTA_ObjName, (ULONG)(mi->title[0] ? mi->title : (name ? (char *)FilePart(name) : "Module")),
        DTA_ObjAnnotation, (ULONG)mi->type,
        TAG_DONE);
    return TRUE;
}

static void disposeModule(struct ModInst *mi)
{
    if (mi->player && mi->port)
        sendCommand(mi, PC_QUIT);
    mi->player = NULL;
    if (mi->data) {
        FreeVec(mi->data);
        mi->data = NULL;
    }
}

/* The attributes the object keeps for itself, from OM_NEW and OM_SET. */
static void takeAttrs(struct ModInst *mi, struct TagItem *tags, BOOL isNew)
{
    struct TagItem *ti, *state = tags;
    UWORD volume = mi->volume;

    while ((ti = NextTagItem(&state))) {
        switch (ti->ti_Tag) {
        case SDTA_Volume:
            volume = ti->ti_Data > 64 ? 64 : (UWORD)ti->ti_Data;
            break;
        case DTA_Repeat:
            mi->repeat = ti->ti_Data ? 1 : 0;
            break;
        case DTA_Immediate:
            mi->immediate = ti->ti_Data ? 1 : 0;
            break;
        case SDTA_SignalTask:
            mi->sigTask = (struct Task *)ti->ti_Data;
            break;
        case SDTA_SignalBit:
            mi->sigMask = ti->ti_Data;
            break;
        case SDTA_SignalBitNumber:
            mi->sigMask = (LONG)ti->ti_Data >= 0 && ti->ti_Data < 32 ? 1UL << ti->ti_Data : 0;
            break;
        }
    }
    if (volume != mi->volume) {
        mi->volume = volume;
        if (!isNew && mi->player)
            Signal(&mi->player->pr_Task, SIGBREAKF_CTRL_E);
    }
}

static void trigger(struct ModInst *mi, ULONG function)
{
    switch (function & STMF_METHOD_MASK) {
    case STM_PLAY:
    case STM_RESUME:
        want(mi, PC_PLAY);
        break;
    case STM_PAUSE:
        want(mi, PC_PAUSE);
        break;
    case STM_STOP:
        want(mi, PC_STOP);
        break;
    case STM_REWIND:
        want(mi, PC_REWIND);
        break;
    }
}

ULONG dt_dispatch(Class *cl, Object *o, Msg msg)
{
    struct ModInst *mi;

    switch (msg->MethodID) {
    case OM_NEW: {
        Object *obj = (Object *)DoSuperMethodA(cl, o, msg);
        if (obj) {
            mi = INST_DATA(cl, obj);
            memset(mi, 0, sizeof *mi);
            mi->volume = 64;
            takeAttrs(mi, ((struct opSet *)msg)->ops_AttrList, TRUE);
            if (!loadModule(cl, obj)) {
                LONG err = IoErr();
                CoerceMethod(cl, obj, OM_DISPOSE);
                SetIoErr(err);
                obj = NULL;
            }
        }
        return (ULONG)obj;
    }
    case OM_DISPOSE:
        disposeModule(INST_DATA(cl, o));
        return DoSuperMethodA(cl, o, msg);
    case OM_SET:
    case OM_UPDATE:
        takeAttrs(INST_DATA(cl, o), ((struct opSet *)msg)->ops_AttrList, FALSE);
        return DoSuperMethodA(cl, o, msg);
    case OM_GET: {
        struct opGet *g = (struct opGet *)msg;
        mi = INST_DATA(cl, o);
        switch (g->opg_AttrID) {
        case SDTA_SampleLength:
            *g->opg_Storage = mi->frames;
            return TRUE;
        case SDTA_SamplesPerSec:
            *g->opg_Storage = mi->rate;
            return TRUE;
        case SDTA_Period:
            *g->opg_Storage = mi->period;
            return TRUE;
        case SDTA_Volume:
            *g->opg_Storage = mi->volume;
            return TRUE;
        case DTA_Repeat:
            *g->opg_Storage = mi->repeat;
            return TRUE;
        case SDTA_ReplayPeriod: {
            struct timeval *tv = (struct timeval *)*g->opg_Storage;
            if (tv) {
                tv->tv_secs = mi->repeat ? 0xFFFFFFFFUL : mi->ms / 1000;
                tv->tv_micro = mi->repeat ? 0xFFFFFFFFUL : mi->ms % 1000 * 1000;
            }
            return TRUE;
        }
        case DTA_TriggerMethods:
            *g->opg_Storage = (ULONG)triggers;
            return TRUE;
        }
        return DoSuperMethodA(cl, o, msg);
    }
    case DTM_TRIGGER:
        trigger(INST_DATA(cl, o), ((struct dtTrigger *)msg)->dtt_Function);
        return TRUE;
    case GM_GOACTIVE: {
        /* A click plays, as it does a sound. */
        mi = INST_DATA(cl, o);
        if (mi->state != PS_PLAYING && mi->state != PS_DRAINING)
            want(mi, PC_PLAY);
        return GMR_NOREUSE;
    }
    case GM_LAYOUT:
    case DTM_PROCLAYOUT: {
        ULONG r = DoSuperMethodA(cl, o, msg);
        mi = INST_DATA(cl, o);
        if (mi->immediate && !mi->started) {
            mi->started = 1;
            want(mi, PC_PLAY);
        }
        return r;
    }
    case DTM_WRITE: {
        /* Saved as it came: there is no 8SVX of a module to write. */
        struct dtWrite *w = (struct dtWrite *)msg;
        mi = INST_DATA(cl, o);
        if (w->dtw_Mode == DTWM_RAW && w->dtw_FileHandle && mi->data)
            return Write(w->dtw_FileHandle, mi->data, mi->size) == (LONG)mi->size;
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        return FALSE;
    }
    case DTM_COPY:
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        return FALSE;
    default:
        return DoSuperMethodA(cl, o, msg);
    }
}
