/* bmp_loader.h — Load a BMP into GPU memory as RGBA8, optionally with a mip chain */

#pragma once
#include <stdint.h>
#include "nid_resolve.h"

typedef struct {
    void *pixels;       /* RGBA8: level 0, then levels 1.. (see bmp_level_offset) */
    int width, height;  /* level 0 */
    int levels;         /* mip levels stored; 1 = level 0 only */
    unsigned long size; /* total bytes, all levels */
    int rmin, rmax;     /* R channel range over level 0 (height maps) */
} BmpTexture;

/* Row scratch buffers in ordinary cached process memory. The row buffer used to
   come from alloc_fn, i.e. GARLIC for GPU textures, and the conversion loop read
   it back byte by byte - uncached CPU reads, ~3 per pixel. Nothing here is ever
   read back from the destination. */
#define BMP_MAX_W      8192
#define BMP_MAX_LEVELS 14
static unsigned char g_bmp_row[BMP_MAX_W * 4 + 16];  /* raw file row          */
static unsigned char g_bmp_rgba[BMP_MAX_W * 4];      /* level-0 row, RGBA8    */
static unsigned char g_bmp_pend[BMP_MAX_W * 4 * 2];  /* even row per level    */
static unsigned char g_bmp_mip[BMP_MAX_W * 4 * 2];   /* produced row per level */

/* Mip layout, LINEAR_ALIGNED (tile index 8), RGBA8, SI/CI - from AMD addrlib
   (SiLib::HwlComputeMipLevel: level width = max(1, basePitch >> level);
   SiLib::HwlGetPitchAlignmentLinear: pitch align = max(8, 64 / 4) = 16 texels;
   SiLib::HwlGetSizeAdjustmentLinear: pitch*height padded to a multiple of
   max(64, pipeInterleave / 4) texels) and PAL addrMgr1 (level offset = running
   size aligned to the level's base alignment = pipe interleave; pow2Pad when
   mipLevels > 1). Levels are generated only for power-of-two sizes and only
   while both dimensions are >= 16: there pitch == width (a multiple of 16),
   width*height is a multiple of 128 texels and every level is a multiple of
   512 bytes, so no padding applies and offsets are plain running sums for a
   256- or 512-byte pipe interleave. */
static int bmp_mip_levels(int w, int h, int max_levels) {
    if (max_levels <= 1 || (w & (w - 1)) || (h & (h - 1))) return 1;
    int n = 1;
    while (n < max_levels && n < BMP_MAX_LEVELS && (w >> n) >= 16 && (h >> n) >= 16) n++;
    return n;
}
static unsigned long bmp_level_offset(int w, int h, int level) {
    unsigned long off = 0;
    for (int l = 0; l < level; l++) off += (unsigned long)(w >> l) * (unsigned long)(h >> l) * 4UL;
    return off;
}

typedef struct {
    unsigned char *base;
    int w, levels;
    unsigned long off[BMP_MAX_LEVELS];   /* byte offset of each level in base      */
    unsigned long roff[BMP_MAX_LEVELS];  /* row-buffer offset of each level (bytes) */
} BmpMipState;

static void bmp_copy_row(unsigned char *dst, const unsigned char *src, int bytes) {
    uint32_t *d = (uint32_t *)dst; const uint32_t *s = (const uint32_t *)src;
    for (int i = 0; i < bytes / 4; i++) d[i] = s[i];
}

/* Row y of `level` is available in `row`. Pair it with the previous even row and
   emit row y/2 of level+1 as a 2x2 box filter (rounded), then recurse. */
static void bmp_mip_feed(BmpMipState *m, int level, int y, const unsigned char *row) {
    if (level + 1 >= m->levels) return;
    int w = m->w >> level;
    unsigned char *pend = g_bmp_pend + m->roff[level];
    if ((y & 1) == 0) { bmp_copy_row(pend, row, w * 4); return; }
    int w2 = w >> 1;
    unsigned char *out = g_bmp_mip + m->roff[level + 1];
    for (int x = 0; x < w2; x++)
        for (int c = 0; c < 4; c++) {
            unsigned s = (unsigned)pend[(2 * x) * 4 + c] + pend[(2 * x + 1) * 4 + c] +
                         row[(2 * x) * 4 + c] + row[(2 * x + 1) * 4 + c];
            out[x * 4 + c] = (unsigned char)((s + 2) >> 2);
        }
    int y2 = y >> 1;
    bmp_copy_row(m->base + m->off[level + 1] + (unsigned long)y2 * (unsigned long)w2 * 4UL, out, w2 * 4);
    bmp_mip_feed(m, level + 1, y2, out);
}

/* max_levels: 1 = level 0 only; >1 = mip chain up to that many levels, when
   bmp_mip_levels allows it (out->levels reports what was built). */
static int bmp_load(const char *path, void *(*alloc_fn)(unsigned long, unsigned long),
                    BmpTexture *out, int max_levels) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return -1;

    /* Read BMP header (14 bytes) + DIB header (40 bytes) */
    unsigned char hdr[54];
    if (sceKernelRead(fd, hdr, 54) < 54) { sceKernelClose(fd); return -2; }

    if (hdr[0] != 'B' || hdr[1] != 'M') { sceKernelClose(fd); return -3; }

    unsigned int data_off = *(unsigned int*)(hdr + 10);
    int w = *(int*)(hdr + 18);
    int h = *(int*)(hdr + 22);
    int bpp = *(unsigned short*)(hdr + 28);
    int compr = *(unsigned int*)(hdr + 30);
    if (h < 0) h = -h; /* top-down BMP: rows are still stored in file order */

    if (compr != 0 || (bpp != 24 && bpp != 32)) { sceKernelClose(fd); return -4; }
    if (w <= 0 || w > BMP_MAX_W || h <= 0) { sceKernelClose(fd); return -7; }

    int levels = bmp_mip_levels(w, h, max_levels);
    unsigned long out_size = bmp_level_offset(w, h, levels);
    void *pixels = alloc_fn(out_size, 0x1000);
    if (!pixels) { sceKernelClose(fd); return -5; }

    BmpMipState m;
    m.base = (unsigned char *)pixels; m.w = w; m.levels = levels;
    unsigned long ro = 0;
    for (int l = 0; l < levels; l++) {
        m.off[l] = bmp_level_offset(w, h, l);
        m.roff[l] = ro;
        ro += (unsigned long)(w >> l) * 4UL;
    }

    int stride_bmp = ((w * (bpp / 8) + 3) & ~3); /* BMP rows are 4-byte aligned */
    unsigned char *dst = (unsigned char *)pixels;
    int rmin = 255, rmax = 0;
    for (int y = 0; y < h; y++) {
        sceKernelLseek(fd, data_off + (long)y * stride_bmp, 0);
        int rd = sceKernelRead(fd, g_bmp_row, (unsigned long)stride_bmp);
        if (rd < (int)(w * (bpp / 8))) break;

        for (int x = 0; x < w; x++) {
            if (bpp == 24) {
                g_bmp_rgba[x*4+0] = g_bmp_row[x*3+2]; /* R */
                g_bmp_rgba[x*4+1] = g_bmp_row[x*3+1]; /* G */
                g_bmp_rgba[x*4+2] = g_bmp_row[x*3+0]; /* B */
                g_bmp_rgba[x*4+3] = 255;              /* A */
            } else { /* 32bpp BGRA */
                g_bmp_rgba[x*4+0] = g_bmp_row[x*4+2];
                g_bmp_rgba[x*4+1] = g_bmp_row[x*4+1];
                g_bmp_rgba[x*4+2] = g_bmp_row[x*4+0];
                g_bmp_rgba[x*4+3] = g_bmp_row[x*4+3];
            }
        }
        for (int x = 0; x < w; x++) {
            int r = g_bmp_rgba[x * 4];
            if (r < rmin)
                rmin = r;
            if (r > rmax)
                rmax = r;
        }
        bmp_copy_row(dst + (unsigned long)y * (unsigned long)w * 4UL, g_bmp_rgba, w * 4);
        bmp_mip_feed(&m, 0, y, g_bmp_rgba);
    }
    sceKernelClose(fd);

    out->pixels = pixels;
    out->width = w;
    out->height = h;
    out->levels = levels;
    out->size = out_size;
    out->rmin = rmin;
    out->rmax = rmax;
    return 0;
}
