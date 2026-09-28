#pragma once
#include <stdint.h>

// Sky PS (ps_dark.s): gradient + sun disc + moon disc, both drawn every frame
// (the floor, drawn after the sky, hides whichever is below the horizon).
//   s[4:7]   sun disc  desc[16]: x (aspect-scaled NDC), y (NDC), radius^2
//   s[20:23] moon disc desc[20]: same
//   s[8:11]  zenith desc[24], s[12:15] horizon desc[28]
//   s[16:19] sun colour x SUN_HDR desc[84], s[24:27] moon colour x MOON_HDR desc[88]
//   t = 0.5 * (1 - clip_y); sky = lerp(zenith, horizon, t)
// Same math as the previous single-disc shader (bit-exact per disc in float32
// emulation). radius^2 must stay > 0 (0 * inf = NaN at the disc centre).
//   sun: f = clamp(1 - d^2 / radius^2, 0, 1)^2; sky = lerp(sky, colour, f)
//   moon: flat disc (no limb darkening, like the full moon) with a one-pixel anti-aliased edge:
//   cov = clamp((r - d) * 540 + 0.5, 0, 1), r = desc[23]; sky = lerp(sky, colour, cov)
// PGM_RSRC1 0xC2 (12 VGPRs, 30 SGPRs incl. VCC). Hash CAFE00E5.
static const uint32_t ps_dark_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000020, 0xBEFC0302, 0xC0820110, 0xC0840118, 0xC086011C, 0xC0880154, 0xC08A0114,
    0xC08C0158, 0xC8080000, 0xC8090001, 0xC80C0100, 0xC80D0101, 0xBF8C007F, 0x080806F2, 0x100808F0,
    0x7E0A020C, 0x0A0A0A08, 0x100A0905, 0x060A0A08, 0x7E0C020D, 0x0A0C0C09, 0x100C0906, 0x060C0C09,
    0x7E0E020E, 0x0A0E0E0A, 0x100E0907, 0x060E0E0A, 0x0A100404, 0x0A120605, 0x10101108, 0x3E101309,
    0x7E125406, 0x10101308, 0x081010F2, 0x20101080, 0x1E1010F2, 0x10101108, 0x08140A10, 0x3E0A110A,
    0x08140C11, 0x3E0C110A, 0x08140E12, 0x3E0E110A, 0x0A100414, 0x0A120615, 0x10101108, 0x3E101309,
    0x7E106708, 0x08101017, 0x101010FF, 0x44070000, 0x061010F0, 0x20101080, 0x1E1010F2, 0x08140A18,
    0x3E0A110A, 0x08140C19, 0x3E0C110A, 0x08140E1A, 0x3E0E110A, 0x7E1602F2, 0xF800180F, 0x0B070605,
    0xBF810000, 0xBF800000, 0x5362724F, 0x00726468, 0x00010800, 0x00000000, 0xDEADBEEF, 0xCAFE00E5,
    0x00000000,
};
