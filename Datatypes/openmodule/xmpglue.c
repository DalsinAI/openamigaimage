/*
 * openmodule.datatype: what libxmp needs from a C library, in a datatype
 * that has no start-up code (see ../common/dtlib.c for malloc and
 * vsnprintf).
 *
 * The module comes from memory (xmp_load_module_from_memory), so libxmp's
 * file functions are never reached: the few stdio calls it links to fail
 * here rather than pull in libnix's stdio, which needs a program's
 * start-up. (novorbis.c stands in for stb_vorbis.)
 *
 * libnix's soft floating point calls the ROM math libraries through the
 * bases below; each player process opens them for itself
 * (moduleclass.c), so only that process's FPU state is touched.
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

struct Library *MathIeeeDoubBasBase, *MathIeeeDoubTransBase, *MathIeeeSingBasBase;

FILE *fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

int fclose(FILE *f)
{
    (void)f;
    return 0;
}

size_t fread(void *buf, size_t size, size_t n, FILE *f)
{
    (void)buf;
    (void)size;
    (void)n;
    (void)f;
    return 0;
}

int fseek(FILE *f, long offset, int whence)
{
    (void)f;
    (void)offset;
    (void)whence;
    return -1;
}

long ftell(FILE *f)
{
    (void)f;
    return -1;
}

#undef getc
#undef putc
int getc(FILE *f)
{
    (void)f;
    return EOF;
}

int putc(int c, FILE *f)
{
    (void)c;
    (void)f;
    return EOF;
}

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list ap;
    int n;
    va_start(ap, format);
    n = vsnprintf(buffer, size, format, ap);
    va_end(ap);
    return n;
}

/* libxmp seeds its random numbers (random waveforms) with the time. */
time_t time(time_t *t)
{
    struct DateStamp ds;
    time_t now;
    DateStamp(&ds);
    now = (time_t)ds.ds_Days * 86400 + ds.ds_Minute * 60 + ds.ds_Tick / TICKS_PER_SECOND;
    if (t)
        *t = now;
    return now;
}
