#pragma once
#include <stdint.h>

// Post 3/3 - composite (shaders/ps_post_comp.s): (scene + desc[14] * bloom) * desc[15]; the sRGB
// colour buffer clamps and encodes. desc: scene T# [0] + point S# [8], {1/w, 1/h, bloom, exposure}
// [12], bloom T# [16] + bilinear S# [24].
static const uint32_t ps_post_comp_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000000C, 0xC0C20100, 0xC0860108, 0xC088010C, 0xC0CA0110, 0xC08E0118,
    0xBF8C007F, 0x10080410, 0x100A0611, 0xF09C0F00, 0x00610804, 0xF09C0F00, 0x00E50C04,
    0xBF8C0F70, 0x3E101812, 0x3E121A12, 0x3E141C12, 0x10101013, 0x10121213, 0x10141413,
    0x7E1602F2, 0xF800180F, 0x0B0A0908, 0xBF810000, 0xBF800000, 0x5362724F, 0x00726468,
    0x00006800, 0x00000000, 0xDEADBEEF, 0xCAFE0212, 0x00000000,
};
