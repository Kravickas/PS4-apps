#pragma once
#include <stdint.h>

// Shadow-pass PS: exports (NDC.z, 0, 0, 1) to MRT0.r.
// Loads light_MVP from desc[48..63] into s[32:47], interpolates attr1=world_pos,
// computes clip.z and clip.w as row·world_pos dot products, perspective-divides,
// saturates NDC.z to [0,1] using V_{MAX,MIN}_LEGACY_F32 (shadPS4-safe), and
// writes to MRT0. RGBA8 output quantizes R to 8-bit (sufficient precision).
// PGM_RSRC1: 44 VGPRs, s0-s47 + VCC = 56 SGPRs → (6<<6)|10 = 0x18A.
// Hash CAFE00FB — fresh hash to force a pipeline-cache miss.
static const uint32_t ps_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF,
    0x00000009,
    0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */, /* prefix */
    0xC1100130, /* s_load_dwordx16 s32, s[0:1], 0x30 — load light_MVP */
    0xBF8C007F, /* s_waitcnt lgkmcnt(0) */
    /* Interp attr1 = world_pos.xyzw → v10,v11,v12,v13 */
    0xC8280400,
    0xC8290401,
    0xC82C0500,
    0xC82D0501,
    0xC8300600,
    0xC8310601,
    0xC8340700,
    0xC8350701,
    /* clip.z = s40*v10 + s41*v11 + s42*v12 + s43*v13 → v16 */
    0x10201428,
    0x10281629,
    0x06202910,
    0x1028182A,
    0x06202910,
    0x10281A2B,
    0x06202910,
    /* clip.w = s44*v10 + s45*v11 + s46*v12 + s47*v13 → v17 */
    0x1022142C,
    0x1028162D,
    0x06222911,
    0x1028182E,
    0x06222911,
    0x10281A2F,
    0x06222911,
    /* v18 = 1/v17; v16 = v16 * v18 = NDC.z */
    0x7E245511,
    0x10202510,
    /* Saturate NDC.z to [0,1] using LEGACY opcodes */
    0x1C202080,
    0x1A2020F2,
    /* Output: (NDC.z, 0, 0, 1) → v40,v41,v42,v43 */
    0x7E500310,
    0x7E520280,
    0x7E540280,
    0x7E5602F2,
    /* EXP MRT0 done=1 vm=1; s_endpgm */
    0xF800080F,
    0x2B2A2928,
    0xBF810000,
    /* OrbShdr footer: 37 dwords = 148 bytes = 0x94 */
    0x5362724F,
    0x00726468,
    0x00009800,
    0x00000000,
    0xDEADBEEF,
    0xCAFE00FB,
    0x00000000,
};
