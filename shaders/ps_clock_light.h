#pragma once
#include <stdint.h>

// ps_clock_light: clock mode's internal light (docs/clock_plan.txt), 1/4 resolution (480 x 270
// RGBA16F). Per pixel (full-res position X = 4 x, Y = 4 y) the sum over the lit rim entries the CPU
// lists (32 bytes each: px, py, tx, ty, w, 0, 0, 0; w = cos (1 - F) x 3 px / (sqrt(2 pi) x the
// normaliser)) of the beam refracted in at that rim point: w exp(-dist / Ls) exp(-q^2 / 2) / sigma,
// q = across / sigma, sigma = S0 + SPREAD max(along, 0), only ahead of it (along > 0). Out: I, I
// tx, I ty (the light and its mean direction x I). Table: [0..1] the entries' address, [2] entries
// x 32 (bytes, >= 32), [3] 0, [4] log2(e) / Ls, [5] log2(e) / 2, [6] S0, [7] SPREAD, [8] 4 (px per
// texel), [9..13] the slab's centre x, y, half w, h, corner radius. Waves whose texels all lie > 40
// px outside the slab skip the Emulated vs float64 over the 584-entry loop: 3e-6 of the peak; waves
// > 40 px outside the slab skip the loop (zeros). PGM_RSRC1 0x104 (20 VGPRs, 34 SGPRs incl. VCC).
// Hash CAFE0306.
static const uint32_t ps_clock_light_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000023, 0xC0820100, 0xC0840104, 0xC0CC0108, 0xBF8C007F, 0x10080418, 0x100A0618,
    0x7E0C0280, 0x7E0E0280, 0x7E100280, 0x0A140819, 0x361414FF, 0x7FFFFFFF, 0x0A14141B, 0x0614141D,
    0x0A160A1A, 0x361616FF, 0x7FFFFFFF, 0x0A16161C, 0x0616161D, 0x20181480, 0x201A1680, 0x101C190C,
    0x3E1C1B0D, 0x7E1C670E, 0x2018170A, 0x1E181880, 0x06181D0C, 0x0A18181D, 0x7C0818FF, 0x42200000,
    0x880E6B6A, 0xBF840021, 0xBE8D0380, 0xC0C8040D, 0xBF8C007F, 0x0A140810, 0x0A160A11, 0x10181412,
    0x3E181613, 0x101A1413, 0x101C1612, 0x081A1D0D, 0x101C150A, 0x3E1C170B, 0x7E1C670E, 0x201E1880,
    0x101E1E0B, 0x061E1E0A, 0x7E20550F, 0x101A210D, 0x101A1B0D, 0x101A1A09, 0x3E1A1C08, 0x081A1A80,
    0x7E1A4B0D, 0x101A210D, 0x101A1A14, 0x7C021880, 0x001A1A80, 0x060C1B06, 0x3E0E1A12, 0x3E101A13,
    0x800DA00D, 0xBF0A060D, 0xBF85FFE0, 0x7E1202F2, 0xF800180F, 0x09080706, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x00012000, 0x00000000, 0xDEADBEEF, 0xCAFE0306, 0x00000000,
};
