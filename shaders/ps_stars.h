#pragma once
#include <stdint.h>

// Stars PS (ps_stars.s): a smooth (1 - r^2)^2 splat over the star quad, so the
// light a star adds does not depend on its sub-pixel position (hard 1.5-4 px
// quads varied up to 4x -> flicker). attr0 = (corner u, corner v, brightness)
// from the vertex uv / normal.y. Colour = desc[36..39] (colour x fade).
// Exports FP16_ABGR (packed, compr): the draw is additive, and radeonsi's
// blend format for a 16_16_16_16 FLOAT target is FP16_ABGR - with a 32-bit
// export the blend did not apply and faded stars replaced the sky (black).
// Hidden behind the moon: x (1 - cov), the moon disc's own coverage (ps_dark) from the pixel centre
// (PS_INPUT_ENA 0x302: v2, v3 = POS_X/Y) in aspect-corrected NDC; moon desc[20..23] = (x, y, r^2,
// r). PGM_RSRC1 0x43 (16 VGPRs, 14 SGPRs incl. VCC). Hash CAFE0202.
static const uint32_t ps_stars_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000017, 0xBEFC0302, 0xC0820124, 0xC0840114, 0xC8100000, 0xC8110001, 0xC8140100,
    0xC8150101, 0xC8180200, 0xC8190201, 0x100E0904, 0x3E0E0B05, 0x080E0EF2, 0x200E0E80, 0x100E0F07,
    0x100E0D07, 0x0A1004FF, 0x44700000, 0x101010FF, 0x3AF2B9D6, 0x081206FF, 0x44070000, 0x101212FF,
    0x3AF2B9D6, 0xBF8C007F, 0x0A101008, 0x0A121209, 0x10101108, 0x3E101309, 0x7E106708, 0x0810100B,
    0x101010FF, 0x44070000, 0x061010F0, 0x20101080, 0x1E1010F2, 0x081010F2, 0x100E1107, 0x10100E04,
    0x10120E05, 0x10140E06, 0x7E160280, 0x5E181308, 0x5E1A170A, 0xF8001C0F, 0x00000D0C, 0xBF810000,
    0x5362724F, 0x00726468, 0x0000C000, 0x00000000, 0xDEADBEEF, 0xCAFE0202, 0x00000000,
};
