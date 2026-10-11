/*
 * PNGBench: openpng.datatype's decoder (pngdecode.c) timed in its stages on
 * one PNG file: inflate with the filters (png_unpack, one core), then the
 * colours (png_rows) three ways, which must give the same bytes:
 *   plain   every row on this CPU, as openpng.datatype 47.3 did and as any
 *           machine without openmulticore.library does;
 *   cpu0    in bands through openmulticore.library aimed at the main CPU
 *           (what the library does with no cores board);
 *   cores wait   in bands on the cores board, this CPU taking a band
 *           too and then waiting for every band (OMCBF_WAIT);
 *   cores   the same, taking back each band not finished when this CPU
 *           comes to it (omc_bands' default, openpng.datatype 47.4).
 * Times are the best of REPS runs, in milliseconds from the E clock.
 *   PNGBench FILE/A [REPS n]
 * A test program, built by png/build.sh and not installed. MIT, Copyright
 * (c) 2026 Dalsin Limited; 11 October 2026, the Team.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <openmulticore/bands.h>

#include "pngdecode.h"

struct Device *TimerBase;
static struct timerequest treq;
static ULONG efreq;

static ULONG now_us(void)
{
    struct EClockVal e;
    unsigned long long t;
    ReadEClock(&e);
    t = ((unsigned long long)e.ev_hi << 32) | e.ev_lo;
    return (ULONG)(t * 1000000ULL / efreq);
}

static APTR fast(ULONG n)
{
    APTR p = AllocVec(n, MEMF_FAST);
    return p ? p : AllocVec(n, MEMF_ANY);
}

static ULONG sum(const ULONG *p, ULONG n)
{
    ULONG h = 2166136261UL;
    while (n--) h = (h ^ *p++) * 16777619UL;
    return h;
}

static void show(const char *what, ULONG best)
{
    Printf((STRPTR)"  %-34s best %5ld.%ld ms\n", (LONG)what, best / 1000, (best % 1000) / 100);
}

int main(void)
{
    LONG argv[2] = { 0, 0 };
    struct RDArgs *ra = ReadArgs((STRPTR)"FILE/A,REPS/K/N", argv, NULL);
    static const char *const way_name[4] = { "colours, plain (47.3)", "colours, bands on cpu0", "colours, cores, waiting for all",
                                             "colours, cores (47.4)" };
    static const ULONG way_flags[4] = { OMCBF_LOCAL, OMCBF_LIBCPU0, OMCBF_WAIT, 0 };
    struct png_head *h = NULL;
    struct png_conv *c = NULL;
    struct Library *omc;
    UBYTE *file = NULL, *idat = NULL, *raw = NULL, *argb_mem = NULL;
    ULONG *argb = NULL, reps = 5, len = 0, r, w, want = 0, unpack_best = ~0UL, whole_best = ~0UL;
    APTR work = NULL;
    BPTR f;
    int rc = 20, bad = 0;
    if (!ra) {
        PutStr((STRPTR)"PNGBench FILE/A [REPS n]\n");
        return 20;
    }
    if (argv[1] && *(LONG *)argv[1] > 0) reps = *(LONG *)argv[1];
    if (OpenDevice((STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&treq, 0)) {
        PutStr((STRPTR)"PNGBench: no timer.device\n");
        FreeArgs(ra);
        return 20;
    }
    TimerBase = treq.tr_node.io_Device;
    {
        struct EClockVal e;
        efreq = ReadEClock(&e);
    }
    if ((f = Open((STRPTR)argv[0], MODE_OLDFILE))) {
        Seek(f, 0, OFFSET_END);
        len = (ULONG)Seek(f, 0, OFFSET_BEGINNING);
        if ((file = fast(len)) && Read(f, file, (LONG)len) != (LONG)len) { FreeVec(file); file = NULL; }
        Close(f);
    }
    if (!file) { Printf((STRPTR)"PNGBench: can't read %s\n", argv[0]); goto out; }
    if (!(h = fast(sizeof *h)) || !(c = fast(sizeof *c))) goto nomem;
    if (png_read_head(file, len, h)) { PutStr((STRPTR)"PNGBench: not a PNG this decoder takes\n"); goto out; }
    if (!(idat = fast(h->idat_len)) || !(raw = fast(h->raw_len)) || !(argb_mem = fast(h->width * h->height * 4 + 15))
        || !(work = fast(PNG_WORKSIZE)))
        goto nomem;
    argb = (ULONG *)(((ULONG)argb_mem + 15) & ~15UL);
    png_gather(file, len, idat);
    omc = OpenLibrary((STRPTR)OPENMULTICORE_NAME, 0);
    Printf((STRPTR)"PNGBench: %s, %ld bytes, %ldx%ld, colour type %ld, depth %ld%s; %ld cores\n", argv[0], (LONG)len,
           (LONG)h->width, (LONG)h->height, (LONG)h->ctype, (LONG)h->depth, (LONG)(h->interlace ? ", Adam7" : ""),
           omc ? (LONG)({ struct Library *OpenMulticoreBase = omc; OMC_CoreCount(); }) : 0L);
    /* the whole decode as 47.3 did it, one call */
    for (r = 0; r < reps; r++) {
        ULONG t0 = now_us(), t1;
        if (png_decode(h, idat, raw, argb, work)) { PutStr((STRPTR)"PNGBench: damaged stream\n"); goto out_lib; }
        t1 = now_us();
        if (t1 - t0 < whole_best) whole_best = t1 - t0;
    }
    want = sum(argb, h->width * h->height);
    show("whole decode, one call (47.3)", whole_best);
    /* its first stage alone */
    for (r = 0; r < reps; r++) {
        ULONG t0 = now_us(), t1;
        c->got = png_unpack(h, idat, raw, work);
        t1 = now_us();
        if (t1 - t0 < unpack_best) unpack_best = t1 - t0;
    }
    show("inflate and filters (one core)", unpack_best);
    c->h = h;
    c->raw = raw;
    c->argb = argb;
    for (w = 0; w < 4; w++) {
        ULONG best = ~0UL, got = 0;
        struct OMCBandStats st;
        if (w > 0 && !omc) { Printf((STRPTR)"  %s: no openmulticore.library\n", (LONG)way_name[w]); continue; }
        for (r = 0; r < reps; r++) {
            ULONG t0, t1, i;
            for (i = 0; i < h->width * h->height; i++) argb[i] = 0xDEADBEEFUL;   /* every pixel must be written again */
            omc__rest = 0;                  /* every run tries the board: what one call costs */
            t0 = now_us();
            omc_bands(omc, png_rows, c, h->height, argb, h->width * 4, way_flags[w], &st);
            t1 = now_us();
            if (t1 - t0 < best) best = t1 - t0;
            got = sum(argb, h->width * h->height);
            if (got != want) bad = 1;
        }
        show(way_name[w], best);
        Printf((STRPTR)"    bands %ld, on the board %ld, taken back %ld, this CPU's rows %ld; sum %08lx %s\n", st.obs_Bands, st.obs_Board,
               st.obs_Taken, st.obs_SelfRows, got, (LONG)(got == want ? "ok" : "DIFFERENT"));
        if (w == 3) {
            ULONG after = unpack_best + best;
            Printf((STRPTR)"  whole decode, 47.4 with the cores:     %5ld.%ld ms (47.3: %ld.%ld ms)\n", after / 1000, (after % 1000) / 100,
                   whole_best / 1000, (whole_best % 1000) / 100);
        }
    }
    PutStr((STRPTR)(bad ? "PNGBench: FAIL, the ways differ\n" : "PNGBench: every way gave the same picture\n"));
    rc = bad ? 10 : 0;
out_lib:
    if (omc) CloseLibrary(omc);
    goto out;
nomem:
    PutStr((STRPTR)"PNGBench: no memory\n");
out:
    if (work) FreeVec(work);
    if (argb_mem) FreeVec(argb_mem);
    if (raw) FreeVec(raw);
    if (idat) FreeVec(idat);
    if (c) FreeVec(c);
    if (h) FreeVec(h);
    if (file) FreeVec(file);
    CloseDevice((struct IORequest *)&treq);
    FreeArgs(ra);
    return rc;
}
