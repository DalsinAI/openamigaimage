/*
 * openpng.datatype's decoder: PNG pictures read on the Amiga itself, written
 * from the PNG specification (ISO/IEC 15948, W3C PNG 3rd edition) and RFC
 * 1950/1951 (zlib's wrapper and inflate). No other library.
 *
 * Every colour type and bit depth the specification allows (greyscale 1, 2,
 * 4, 8, 16; RGB 8, 16; palette 1, 2, 4, 8; grey with alpha 8, 16; RGBA 8,
 * 16), transparency (tRNS: a palette's alphas, or the one transparent grey
 * or RGB colour) and Adam7 interlacing. The picture comes out as 0xAARRGGBB
 * words, one row after the other; 16-bit samples are taken to 8.
 *
 * Built to run as a job on another core (openmulticore.library, the cores
 * board): png_decode() makes no OS calls, writes no static data and touches
 * only the memory it is handed, which the caller allocates from the sizes
 * png_head() works out. Its colour stage, png_rows(), is shared between the
 * cores in bands of rows (11 October 2026).
 *
 * MIT, Copyright (c) 2026 Dalsin Limited.
 */
#ifndef PNGDECODE_H
#define PNGDECODE_H

#include <exec/types.h>

#define PNG_MAXSIDE   32768UL
#define PNG_MAXPIXELS (64UL << 20)       /* 256 MB of ARGB */

struct png_head {
    ULONG width, height;
    UBYTE depth;                         /* bits per sample */
    UBYTE ctype;                         /* 0 grey, 2 RGB, 3 palette, 4 grey+alpha, 6 RGBA */
    UBYTE interlace;                     /* 0 none, 1 Adam7 */
    UBYTE alpha;                         /* 1: colour type 4 or 6, or a tRNS chunk */
    UBYTE has_key;                       /* a tRNS colour for grey or RGB */
    UBYTE pad[3];
    UWORD key[3];                        /* that colour: grey, or R, G, B (at the picture's depth) */
    ULONG idat_len;                      /* all the IDAT chunks' bytes together */
    ULONG raw_len;                       /* the inflated rows of every pass, with their filter bytes */
    ULONG palette[256];                  /* ARGB; tRNS alphas applied */
};

/* What png_decode() needs to work in, besides the picture: the inflate state
 * and Huffman tables. */
#define PNG_WORKSIZE 4096UL

/* Reads the chunks before the pixels. 0, or -1 when this isn't a PNG it can
 * read (bad signature, IHDR, sizes or a missing IDAT). */
LONG png_read_head(const UBYTE *file, ULONG len, struct png_head *h);

/* Copies the IDAT chunks' data together into idat (h->idat_len bytes). */
void png_gather(const UBYTE *file, ULONG len, UBYTE *idat);

/* Inflates idat into raw (h->raw_len bytes), undoes the filters and writes
 * the picture to argb (width * height words). work is PNG_WORKSIZE bytes,
 * longword aligned. 0, or -1 on a damaged stream. No OS calls. */
LONG png_decode(const struct png_head *h, const UBYTE *idat, UBYTE *raw, ULONG *argb, APTR work);

/* png_decode() in its two parts, so the second can be shared between cores (11 October 2026):
 * png_unpack() inflates and undoes the filters, row after row (one core); it returns how many bytes of
 * raw came, or 0 on a damaged stream. png_rows() then writes rows y0 to y1 - 1 of argb from raw; rows are
 * independent, so it can run in bands on several cores at once (openmulticore's omc_bands), and it makes
 * no OS calls, writes nothing but those rows, and reads only *conv, the header and raw. */
struct png_conv {
    const struct png_head *h;
    const UBYTE *raw;
    ULONG got;                           /* png_unpack()'s answer */
    ULONG *argb;                         /* width * height words */
};
ULONG png_unpack(const struct png_head *h, const UBYTE *idat, UBYTE *raw, APTR work);
void png_rows(APTR conv, ULONG y0, ULONG y1);

#endif
