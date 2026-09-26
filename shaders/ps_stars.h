#pragma once
#include <stdint.h>

// Stars PS (ps_stars.s): a smooth (1 - r^2)^2 splat over the star quad, so the
// light a star adds does not depend on its sub-pixel position (hard 1.5-4 px
// quads varied up to 4x -> flicker). attr0 = (corner u, corner v, brightness)
// from the vertex uv / normal.y. Colour = desc[36..39] (colour x fade).
// Exports FP16_ABGR (packed, compr): the draw is additive, and radeonsi's
// blend format for a 16_16_16_16 FLOAT target is FP16_ABGR - with a 32-bit
// export the blend did not apply and faded stars replaced the sky (black).
// PGM_RSRC1 0x42 (12 VGPRs, s0-s7 + VCC = 16 SGPRs). Hash CAFE0201.
static const uint32_t ps_stars_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000000C, 0xBEFC0302, 0xC0820124, 0xC8080000, 0xC8090001, 0xC80C0100,
    0xC80D0101, 0xC8100200, 0xC8110201, 0x100A0502, 0x3E0A0703, 0x080A0AF2, 0x200A0A80,
    0x100A0B05, 0x100A0905, 0xBF8C007F, 0x100C0A04, 0x100E0A05, 0x10100A06, 0x7E120280,
    0x5E140F06, 0x5E161308, 0xF8001C0F, 0x00000B0A, 0xBF810000, 0x5362724F, 0x00726468,
    0x00006800, 0x00000000, 0xDEADBEEF, 0xCAFE0201, 0x00000000,
};
