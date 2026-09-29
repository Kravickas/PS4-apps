#pragma once
/* DDS textures from tools/make_textures.py, read straight into GPU memory.

   Accepted: DX10 header, 2D, one layer, DXGI BC1_UNORM_SRGB (72), BC4_UNORM (80),
   BC5_UNORM (83) or R8G8B8A8_UNORM_SRGB (29); power-of-two sizes >= 32; mip levels
   only while both dimensions stay >= 32 px.

   GPU layout. The GFX7 hardware (PS4) samples block-compressed formats only
   from tiled surfaces: AMD PAL's GFX7 format table lists BC1 / BC4 / BC5 as
   (linear: Copy, optimal: IrXsIfl) - linear BC cannot be read by a shader, and
   the PS4 hung on the first floor draw that did. So:
   - BC1 / BC4 / BC5 -> tile index 13 (Thin_1dThin: ARRAY_1D_TILED_THIN1, micro
     tile mode THIN = addrlib ADDR_NON_DISPLAYABLE). addrlib (EgBasedLib::
     ComputeSurfaceAddrFromCoordMicroTiled, Lib::ComputePixelIndexWithinMicroTile):
     an element (4x4 block) at (x, y) of a level lives at
       ((y / 8) * (pitch / 8) + x / 8) * 64 * B + index(x % 8, y % 8) * B,
       index bits (low to high) = x0, y0, x1, y1, x2, y2,
     B = 8 (BC1, BC4) or 16 (BC5). SiLib: pitch aligned to 8 elements, height to
     8, level size padded to the 256-byte pipe interleave. Levels >= 32 px are
     >= 8 blocks each way, so nothing pads: level sizes and offsets are those of
     the packed DDS chain, only the element order inside a level changes.
   - RGBA8 -> tile index 8 (LINEAR_ALIGNED, readable by shaders): pitch aligned
     to 16 texels, levels to 64 texels (addrlib SiLib linear); no padding at
     >= 32 px, so the DDS chain is read in place.
   Rows are stored bottom-up (row 0 = v 0, as the OBJ UVs expect). */
#include <stdint.h>
#include "nid_resolve.h"

typedef struct {
    void* pixels;
    int width, height, levels;
    uint32_t data_format; /* SQ_IMG_RSRC_WORD1 DATA_FORMAT (gfx_7_2_enum.h) */
    uint32_t num_format;  /* SQ_IMG_RSRC_WORD1 NUM_FORMAT: 0 UNORM, 9 SRGB */
    uint32_t tile_index;  /* SQ_IMG_RSRC_WORD3 TILING_INDEX: 13 (1D thin) for BC, 8 linear */
    unsigned long size;
} DdsTexture;

#define DDS_HEADER_BYTES 148 /* "DDS " + DDS_HEADER (124) + DDS_HEADER_DXT10 (20) */
#define DDS_MIN_SIZE 32
#define DDS_MAX_SIZE 16384

/* One row of micro tiles (8 block rows) of the widest BC level: 4096 blocks x 8 x 16 B. */
static unsigned char g_dds_rows[(DDS_MAX_SIZE / 4) * 8 * 16];

/* Reads one level of bw x bh blocks (B bytes each) from fd and writes it tiled
   (Thin_1dThin) to dst. Streams one row of micro tiles at a time; dst is written
   sequentially (write-combined GPU memory, never read back). 0 on success. */
static int dds_read_level_tiled(int fd, unsigned char* dst, uint32_t bw, uint32_t bh, uint32_t B) {
    unsigned long row_bytes = (unsigned long)bw * 8 * B;
    for (uint32_t ty = 0; ty < bh / 8; ty++) {
        unsigned long got = 0;
        while (got < row_bytes) {
            long r = sceKernelRead(fd, g_dds_rows + got, row_bytes - got);
            if (r <= 0)
                return -1;
            got += (unsigned long)r;
        }
        for (uint32_t tx = 0; tx < bw / 8; tx++) {
            for (uint32_t e = 0; e < 64; e++) {
                /* element e of a THIN micro tile: index bits (low to high) x0, y0,
                   x1, y1, x2, y2 */
                uint32_t x = (e & 1) | ((e >> 1) & 2) | ((e >> 2) & 4);
                uint32_t y = ((e >> 1) & 1) | ((e >> 2) & 2) | ((e >> 3) & 4);
                const uint64_t* src =
                    (const uint64_t*)(g_dds_rows + ((unsigned long)y * bw + tx * 8 + x) * B);
                uint64_t* d = (uint64_t*)dst;
                d[0] = src[0];
                if (B == 16)
                    d[1] = src[1];
                dst += B;
            }
        }
    }
    return 0;
}

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
    if (w < DDS_MIN_SIZE || hh < DDS_MIN_SIZE || (w & (w - 1)) || (hh & (hh - 1)) ||
        w > DDS_MAX_SIZE || hh > DDS_MAX_SIZE) {
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
    if (block == 4) {
        unsigned char* p = dst;
        for (uint32_t l = 0; l < levels; l++) {
            uint32_t bw = (w >> l) / 4, bh = (hh >> l) / 4;
            if (dds_read_level_tiled(fd, p, bw, bh, bytes_per) != 0) {
                sceKernelClose(fd);
                return -8;
            }
            p += (unsigned long)bw * bh * bytes_per;
        }
        out->tile_index = 13;
    } else {
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
        if (done != size) {
            sceKernelClose(fd);
            return -8;
        }
        out->tile_index = 8;
    }
    sceKernelClose(fd);
    out->pixels = dst;
    out->width = (int)w;
    out->height = (int)hh;
    out->levels = (int)levels;
    out->size = size;
    return 0;
}
