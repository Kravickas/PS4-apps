#pragma once
/* DDS textures from tools/make_textures.py, read straight into GPU memory.

   Accepted: DX10 header, 2D, one layer, DXGI BC1_UNORM_SRGB (72), BC4_UNORM (80),
   BC5_UNORM (83) or R8G8B8A8_UNORM_SRGB (29); power-of-two sizes >= 32; mip levels
   only while both dimensions stay >= 32 px.

   GPU layout LINEAR_ALIGNED (AMD addrlib SiLib, shadPS4 ImageSizeLinearAligned):
   an element is a 4x4 block (8 or 16 bytes) or a texel (4 bytes); pitch aligned
   to max(8, 64 / element bytes) elements, each level to max(64, 256 / element
   bytes) elements, levels back to back. At >= 32 px a level is >= 8 blocks /
   32 texels wide (a multiple of 8 / 16) with >= 64 elements, so no padding
   applies: the DDS chain, tightly packed, is the GPU layout and is read in
   place. Rows are stored bottom-up (row 0 = v 0, as the OBJ UVs expect). */
#include <stdint.h>
#include "nid_resolve.h"

typedef struct {
    void* pixels;
    int width, height, levels;
    uint32_t data_format; /* SQ_IMG_RSRC_WORD1 DATA_FORMAT (gfx_7_2_enum.h) */
    uint32_t num_format;  /* SQ_IMG_RSRC_WORD1 NUM_FORMAT: 0 UNORM, 9 SRGB */
    unsigned long size;
} DdsTexture;

#define DDS_HEADER_BYTES 148 /* "DDS " + DDS_HEADER (124) + DDS_HEADER_DXT10 (20) */
#define DDS_MIN_SIZE 32

static uint32_t dds_u32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 0 on success; -1 open, -2 short header, -3 not a supported DX10 DDS, -4 size,
   -5 mip chain, -6 file size, -7 alloc, -8 short read. */
static int dds_load(const char* path, void* (*alloc_fn)(unsigned long, unsigned long),
                    DdsTexture* out) {
    unsigned char h[DDS_HEADER_BYTES];
    int fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0);
    if (fd < 0)
        return -1;
    long got = 0;
    while (got < DDS_HEADER_BYTES) {
        long r = sceKernelRead(fd, h + got, (unsigned long)(DDS_HEADER_BYTES - got));
        if (r <= 0)
            break;
        got += r;
    }
    if (got != DDS_HEADER_BYTES) {
        sceKernelClose(fd);
        return -2;
    }
    uint32_t dxgi = dds_u32(h + 128), bytes_per = 0, block = 1;
    if (h[0] != 'D' || h[1] != 'D' || h[2] != 'S' || h[3] != ' ' || dds_u32(h + 4) != 124 ||
        dds_u32(h + 76) != 32 || !(dds_u32(h + 80) & 0x4) || h[84] != 'D' || h[85] != 'X' ||
        h[86] != '1' || h[87] != '0' || dds_u32(h + 132) != 3 /* TEXTURE2D */ ||
        dds_u32(h + 140) != 1 /* array size */) {
        sceKernelClose(fd);
        return -3;
    }
    if (dxgi == 72) { /* BC1_UNORM_SRGB */
        out->data_format = 0x23;
        out->num_format = 9;
        bytes_per = 8;
        block = 4;
    } else if (dxgi == 80) { /* BC4_UNORM */
        out->data_format = 0x26;
        out->num_format = 0;
        bytes_per = 8;
        block = 4;
    } else if (dxgi == 83) { /* BC5_UNORM */
        out->data_format = 0x27;
        out->num_format = 0;
        bytes_per = 16;
        block = 4;
    } else if (dxgi == 29) { /* R8G8B8A8_UNORM_SRGB */
        out->data_format = 0x0A;
        out->num_format = 9;
        bytes_per = 4;
        block = 1;
    } else {
        sceKernelClose(fd);
        return -3;
    }
    uint32_t w = dds_u32(h + 16), hh = dds_u32(h + 12), levels = dds_u32(h + 28);
    if (levels == 0)
        levels = 1;
    if (w < DDS_MIN_SIZE || hh < DDS_MIN_SIZE || (w & (w - 1)) || (hh & (hh - 1)) || w > 16384 ||
        hh > 16384) {
        sceKernelClose(fd);
        return -4;
    }
    if (levels > 15 || (w >> (levels - 1)) < DDS_MIN_SIZE || (hh >> (levels - 1)) < DDS_MIN_SIZE) {
        sceKernelClose(fd);
        return -5;
    }
    unsigned long size = 0;
    for (uint32_t l = 0; l < levels; l++)
        size += (unsigned long)((w >> l) / block) * (unsigned long)((hh >> l) / block) * bytes_per;
    long file_size = sceKernelLseek(fd, 0, 2 /* SEEK_END */);
    if (file_size != (long)(DDS_HEADER_BYTES + size)) {
        sceKernelClose(fd);
        return -6;
    }
    sceKernelLseek(fd, DDS_HEADER_BYTES, 0 /* SEEK_SET */);
    unsigned char* dst = (unsigned char*)alloc_fn(size, 0x1000);
    if (!dst) {
        sceKernelClose(fd);
        return -7;
    }
    unsigned long done = 0;
    while (done < size) {
        unsigned long chunk = size - done;
        if (chunk > (8UL << 20))
            chunk = 8UL << 20;
        long r = sceKernelRead(fd, dst + done, chunk);
        if (r <= 0)
            break;
        done += (unsigned long)r;
    }
    sceKernelClose(fd);
    if (done != size)
        return -8;
    out->pixels = dst;
    out->width = (int)w;
    out->height = (int)hh;
    out->levels = (int)levels;
    out->size = size;
    return 0;
}
