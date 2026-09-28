#pragma once
#include <stdint.h>

// MSAA resolve (ps_resolve.s): the 4-sample scene (RGBA16F, PS4 tile index 13 Thin1dThin, no
// FMASK / CMASK: fragment = sample) averaged per pixel into the single-sample HDR target (box
// resolve in linear light). desc: MSAA T# [0]: TYPE 2D_MSAA (0xE), BASE_LEVEL 0, LAST_LEVEL =
// log2(samples) = 2 (PAL gfx6Device.cpp), TILING_INDEX 13. image_load (x, y, sample) from the
// pixel centre (POS_X/Y_FLOAT, truncated); 0.25 is not a CI inline constant: literal 0x3E800000.
// PGM_RSRC1 0x49 (40 VGPRs, 14 SGPRs incl. VCC). Hash CAFE0400.
static const uint32_t ps_resolve_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000015, 0xC0C20100, 0xBF8C007F, 0x7E080F02, 0x7E0A0F03, 0x7E0C0280, 0x7E180304,
    0x7E1A0305, 0x7E1C0281, 0x7E200304, 0x7E220305, 0x7E240282, 0x7E280304, 0x7E2A0305, 0x7E2C0283,
    0xF0001700, 0x00011804, 0xF0001700, 0x00011C0C, 0xF0001700, 0x00012010, 0xF0001700, 0x00012414,
    0xBF8C0F70, 0x06303918, 0x06323B19, 0x06343D1A, 0x06404920, 0x06424B21, 0x06444D22, 0x06304118,
    0x06324319, 0x0634451A, 0x103030FF, 0x3E800000, 0x103232FF, 0x3E800000, 0x103434FF, 0x3E800000,
    0x7E3602F2, 0xF800180F, 0x1B1A1918, 0xBF810000, 0x5362724F, 0x00726468, 0x0000B000, 0x00000000,
    0xDEADBEEF, 0xCAFE0400, 0x00000000,
};
