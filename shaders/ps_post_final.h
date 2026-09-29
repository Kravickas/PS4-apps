#pragma once
#include <stdint.h>

// Post - final composite (ps_post_final.s): (scene + desc[14] * bloom) *
// desc[15], clamped (exact IEC 61966-2-1 curve:
// 12.92 c below 0.0031308, else 1.055 c^(1/2.4) - 0.055) and dithered by
// +-0.5/255 (interleaved gradient noise on the pixel position) before the
// UNORM display write - 8-bit output of smooth gradients does not band.
// desc: scene T# [0] + point S# [8], {1/w, 1/h, bloom, exposure} [12],
// bloom T# [16] + bilinear S# [24].
// Lens flare (linear light, before the clamp), final pass table dwords: 28..29 sun (u, v)
// = pixel / size; 30..32 strength RGB (sun colour x FLARE_STRENGTH x visibility x edge
// fade; 0 = off: skipped by a scalar branch); 33..36 gains: ghosts, rays (x the texture's
// storage scale), glow, veil; 40..47 glare T#, 48..51 its S# (bilinear, clamp).
// e = (uv - 0.5) (A, 1), d = (sun - 0.5) (A, 1), A = 1920/1080. Six soft ghosts at e - t d
// (t -0.25 -0.5 -0.8 -1.1 0.4 0.7): max(0, 1 - r^2 / R^2)^2 x intensity x tint; uneven rays:
// glare texture (tools/make_glare.py: diffraction by lens scratches and dust) at
// 0.5 + (e - d) / 1.2; glow g / (1 + rho^2 / 0.08^2) + veil v / (1 + rho^2 / 0.40^2).
// Output: the finished frame, linear and clamped to [0, 1], into an RGBA16F target (no sRGB
// encode, no dither): the frost chain blurs it and ps_ui encodes it with the UI on top.
// Glow, veil and rays at half the size: glow width 0.04, veil 0.20, rays over 0.6 image heights
// (were 0.08, 0.40, 1.2); strengths unchanged.
// PGM_RSRC1 0x1C9 (40 VGPRs, 59 SGPRs incl. VCC). Hash CAFE021C.
static const uint32_t ps_post_final_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000063, 0xC0C20100, 0xC0860108, 0xC088010C, 0xC0CA0110, 0xC08E0118, 0xC0D0011C,
    0xC0940124, 0xC0D60128, 0xC09A0130, 0xBF8C007F, 0x10080410, 0x100A0611, 0xF09C0F00, 0x00610804,
    0xF09C0F00, 0x00E50C04, 0xBF8C0F70, 0x3E101812, 0x3E121A12, 0x3E141C12, 0x10101013, 0x10121213,
    0x10141413, 0x88382322, 0x88382438, 0xBF068038, 0xBF8500A0, 0x063008F1, 0x103030FF, 0x3FE38E39,
    0x06320AF1, 0x7E2C0220, 0x062C2CF1, 0x102C2CFF, 0x3FE38E39, 0x7E2E0221, 0x062E2EF1, 0x7E340280,
    0x7E360280, 0x7E380280, 0x103A2CFF, 0xBE800000, 0x083A3B18, 0x103C2EFF, 0xBE800000, 0x083C3D19,
    0x103A3B1D, 0x3E3A3D1E, 0x103A3AFF, 0x444C14E6, 0x083A3AF2, 0x203A3A80, 0x103A3B1D, 0x3E343AFF,
    0x3EB33333, 0x3E363AFF, 0x3E810625, 0x3E383AFF, 0x3E0F5C29, 0x103A2CF1, 0x083A3B18, 0x103C2EF1,
    0x083C3D19, 0x103A3B1D, 0x3E3A3D1E, 0x103A3AFF, 0x42F6E9E0, 0x083A3AF2, 0x203A3A80, 0x103A3B1D,
    0x3E343AFF, 0x3D3851EC, 0x3E363AFF, 0x3DA3D70A, 0x3E383AFF, 0x3DCCCCCD, 0x103A2CFF, 0xBF4CCCCD,
    0x083A3B18, 0x103C2EFF, 0xBF4CCCCD, 0x083C3D19, 0x103A3B1D, 0x3E3A3D1E, 0x103A3AFF, 0x43C80000,
    0x083A3AF2, 0x203A3A80, 0x103A3B1D, 0x3E343AFF, 0x3DCAC083, 0x3E363AFF, 0x3E3851EC, 0x3E383AFF,
    0x3DDD2F1B, 0x103A2CFF, 0xBF8CCCCD, 0x083A3B18, 0x103C2EFF, 0xBF8CCCCD, 0x083C3D19, 0x103A3B1D,
    0x3E3A3D1E, 0x103A3AFF, 0x424C14E6, 0x083A3AF2, 0x203A3A80, 0x103A3B1D, 0x3E343AFF, 0x3D2C0831,
    0x3E363AFF, 0x3D072B02, 0x3E383AFF, 0x3D75C28F, 0x103A2CFF, 0x3ECCCCCD, 0x083A3B18, 0x103C2EFF,
    0x3ECCCCCD, 0x083C3D19, 0x103A3B1D, 0x3E3A3D1E, 0x103A3AFF, 0x44C80000, 0x083A3AF2, 0x203A3A80,
    0x103A3B1D, 0x3E343AFF, 0x3E99999A, 0x3E363AFF, 0x3E828F5C, 0x3E383AFF, 0x3E28F5C3, 0x103A2CFF,
    0x3F333333, 0x083A3B18, 0x103C2EFF, 0x3F333333, 0x083C3D19, 0x103A3B1D, 0x3E3A3D1E, 0x103A3AFF,
    0x438AE38E, 0x083A3AF2, 0x203A3A80, 0x103A3B1D, 0x3E343AFF, 0x3D75C28F, 0x3E363AFF, 0x3DAC0831,
    0x3E383AFF, 0x3DF5C28F, 0x10343425, 0x10363625, 0x10383825, 0x083A2D18, 0x083C2F19, 0x103E3AFF,
    0x3FD55555, 0x063E3EF0, 0x10403CFF, 0x3FD55555, 0x064040F0, 0xF09C0700, 0x01AB211F, 0x103A3B1D,
    0x3E3A3D1E, 0x103C3AFF, 0x441C4000, 0x063C3CF2, 0x7E3C551E, 0x103C3C27, 0x10483AFF, 0x41C80000,
    0x064848F2, 0x7E485524, 0x3E3C4828, 0x06343D1A, 0x06363D1B, 0x06383D1C, 0xBF8C0F70, 0x3E344226,
    0x3E364426, 0x3E384626, 0x3E103422, 0x3E123623, 0x3E143824, 0x20101080, 0x1E1010F2, 0x20121280,
    0x1E1212F2, 0x20141480, 0x1E1414F2, 0x7E1602F2, 0xF800180F, 0x0B0A0908, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x00032000, 0x00000000, 0xDEADBEEF, 0xCAFE021C, 0x00000000,
};
