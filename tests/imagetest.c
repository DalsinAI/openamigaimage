/* openamigaimage smoke test: zlib round trip, PNG write and read back,
 * JPEG compress and decompress. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <png.h>
#include <jpeglib.h>
int main(void)
{
    unsigned char src[4096], zbuf[8192], back[4096]; uLongf zlen = sizeof zbuf, blen = sizeof back; int i, ok;
    for (i = 0; i < 4096; i++) src[i] = (unsigned char)(i * 7 + (i >> 5));
    compress2(zbuf, &zlen, src, sizeof src, 9); uncompress(back, &blen, zbuf, zlen);
    printf("ZLIB %s crc=%08lx packed=%lu roundtrip=%s\n", zlibVersion(), crc32(0, src, sizeof src), (unsigned long)zlen, blen == 4096 && !memcmp(src, back, 4096) ? "ok" : "BAD");
    {   /* PNG: 16x8 RGBA gradient through libpng's simplified API */
        png_image img; unsigned char pix[16 * 8 * 4], rd[16 * 8 * 4]; png_alloc_size_t sz = 0; void *mem;
        for (i = 0; i < 16 * 8 * 4; i++) pix[i] = (unsigned char)(i * 13);
        memset(&img, 0, sizeof img); img.version = PNG_IMAGE_VERSION; img.width = 16; img.height = 8; img.format = PNG_FORMAT_RGBA;
        png_image_write_get_memory_size(img, sz, 0, pix, 0, NULL);
        mem = malloc(sz); ok = png_image_write_to_memory(&img, mem, &sz, 0, pix, 0, NULL);
        png_image_free(&img); memset(&img, 0, sizeof img); img.version = PNG_IMAGE_VERSION;
        ok = ok && png_image_begin_read_from_memory(&img, mem, sz); img.format = PNG_FORMAT_RGBA;
        ok = ok && png_image_finish_read(&img, NULL, rd, 0, NULL);
        printf("LIBPNG %s bytes=%lu roundtrip=%s\n", png_get_libpng_ver(NULL), (unsigned long)sz, ok && !memcmp(pix, rd, sizeof pix) ? "ok" : "BAD");
        free(mem);
    }
    {   /* JPEG: 32x16 RGB, compress then decompress */
        struct jpeg_compress_struct c; struct jpeg_decompress_struct d; struct jpeg_error_mgr e1, e2;
        unsigned char rgb[32 * 16 * 3], *out = NULL; unsigned long olen = 0; JSAMPROW row; long diff = 0; unsigned char line[32 * 3]; int y;
        for (i = 0; i < 32 * 16 * 3; i++) rgb[i] = (unsigned char)((i % 96) * 2);
        c.err = jpeg_std_error(&e1); jpeg_create_compress(&c); jpeg_mem_dest(&c, &out, &olen);
        c.image_width = 32; c.image_height = 16; c.input_components = 3; c.in_color_space = JCS_RGB;
        jpeg_set_defaults(&c); jpeg_set_quality(&c, 95, TRUE); jpeg_start_compress(&c, TRUE);
        while (c.next_scanline < 16) { row = rgb + c.next_scanline * 96; jpeg_write_scanlines(&c, &row, 1); }
        jpeg_finish_compress(&c); jpeg_destroy_compress(&c);
        d.err = jpeg_std_error(&e2); jpeg_create_decompress(&d); jpeg_mem_src(&d, out, olen); jpeg_read_header(&d, TRUE); jpeg_start_decompress(&d);
        for (y = 0; y < 16; y++) { row = line; jpeg_read_scanlines(&d, &row, 1); for (i = 0; i < 96; i++) diff += abs(line[i] - rgb[y * 96 + i]); }
        jpeg_finish_decompress(&d); jpeg_destroy_decompress(&d);
        printf("LIBJPEG %d%c bytes=%lu mean_error=%ld.%02ld\n", JPEG_LIB_VERSION_MAJOR, 'a' + JPEG_LIB_VERSION_MINOR - 1, olen, diff / 1536, (diff * 100 / 1536) % 100);
    }
    printf("IMAGE_DONE\n");
    return 0;
}
