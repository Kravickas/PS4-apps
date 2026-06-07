#pragma once


struct CB_COLOR_PITCH {
    uint TILE_MAX : 11;
    uint RESERVED0 : 9;
    uint FMASK_TILE_MAX : 11;
    uint RESERVED1 : 1;
};

struct CB_COLOR_SLICE {
    uint TILE_MAX : 22;
    uint RESERVED0 : 10;
};

struct CB_COLOR_VIEW {
    uint SLICE_START : 11;
    uint RESERVED0 : 2;
    uint SLICE_MAX : 11;
    uint RESERVED1 : 8;
};

struct CB_COLOR_INFO {
    uint ENDIAN : 2;
    uint FORMAT : 5;
    uint LINEAR_GENERAL : 1;
    uint NUMBER_TYPE : 3;
    uint COMP_SWAP : 2;
    uint FAST_CLEAR : 1;
    uint COMPRESSION : 1;
    uint BLEND_CLAMP : 1;
    uint BLEND_BYPASS : 1;
    uint SIMPLE_FLOAT : 1;
    uint ROUND_MODE : 1;
    uint CMASK_IS_LINEAR : 1;
    uint BLEND_OPT_DONT_RD_DST : 3;
    uint BLEND_OPT_DISCARD_PIXEL : 3;
    uint FMASK_COMPRESSION_DISABLE : 1;
    uint FMASK_COMPRESS_1FRAG_ONLY : 1;
    uint DCC_ENABLE : 1;
    uint CMASK_ADDR_TYPE : 2;
    uint ALT_TILE_MODE : 1;
};

struct CB_COLOR_ATTRIB {
    uint TILE_MODE_INDEX : 5;
    uint FMASK_TILE_MODE_INDEX : 5;
    uint FMASK_BANK_HEIGHT : 2;
    uint NUM_SAMPLES : 3;
    uint NUM_FRAGMENTS : 2;
    uint FORCE_DST_ALPHA_1 : 1;
    uint RESERVED0 : 14;
};

struct CB_COLOR_DCC_CONTROL {
    uint OVERWRITE_COMBINER_DISABLE : 1;
    uint KEY_CLEAR_ENABLE : 1;
    uint MAX_UNCOMPRESSED_BLOCK_SIZE : 2;
    uint MIN_COMPRESSED_BLOCK_SIZE : 1;
    uint MAX_COMPRESSED_BLOCK_SIZE : 2;
    uint COLOR_TRANSFORM : 2;
    uint INDEPENDENT_64B_BLOCKS : 1;
    uint LOSSY_RGB_PRECISION : 4;
    uint LOSSY_ALPHA_PRECISION : 4;
    uint RESERVED0 : 4;
};

struct CB_COLOR_CMASK_SLICE {
    uint TILE_MAX : 14;
    uint RESERVED0 : 18;
};

struct CB_COLOR_FMASK_SLICE {
    uint TILE_MAX : 22;
    uint RESERVED0 : 10;
};

struct RENDER_TARGET { //mmCB_COLOR0_BASE 0xA318
    uint                 BASE;
    CB_COLOR_PITCH       PITCH;
    CB_COLOR_SLICE       SLICE;
    CB_COLOR_VIEW        VIEW;
    CB_COLOR_INFO        INFO;
    CB_COLOR_ATTRIB      ATTRIB;
    CB_COLOR_DCC_CONTROL DCC_CONTROL;
    uint                 CMASK;
    CB_COLOR_CMASK_SLICE CMASK_SLICE;
    uint                 FMASK;
    CB_COLOR_FMASK_SLICE FMASK_SLICE;
    uint64_t             CLEAR_WORD;
    uint                 DCC_BASE;
    struct {
        short width;
        short height;
    } hint;
};

// ColorFormat
#define COLOR_8              0x01
#define COLOR_16             0x02
#define COLOR_8_8            0x03
#define COLOR_32             0x04
#define COLOR_16_16          0x05
#define COLOR_10_11_11       0x06
#define COLOR_11_11_10       0x07
#define COLOR_10_10_10_2     0x08
#define COLOR_2_10_10_10     0x09
#define COLOR_8_8_8_8        0x0a
#define COLOR_32_32          0x0b
#define COLOR_16_16_16_16    0x0c
#define COLOR_32_32_32_32    0x0e
#define COLOR_5_6_5          0x10
#define COLOR_1_5_5_5        0x11
#define COLOR_5_5_5_1        0x12
#define COLOR_4_4_4_4        0x13
#define COLOR_8_24           0x14
#define COLOR_24_8           0x15
#define COLOR_X24_8_32_FLOAT 0x16
// SurfaceNumber
#define NUMBER_UNORM   0x0
#define NUMBER_SNORM   0x1
#define NUMBER_USCALED 0x2
#define NUMBER_SSCALED 0x3
#define NUMBER_UINT    0x4
#define NUMBER_SINT    0x5
#define NUMBER_SRGB    0x6
#define NUMBER_FLOAT   0x7
// SurfaceSwap 
#define SWAP_STD       0x0
#define SWAP_ALT       0x1
#define SWAP_STD_REV   0x2
#define SWAP_ALT_REV   0x3

