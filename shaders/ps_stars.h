#pragma once
#include <stdint.h>

// Stars PS: outputs desc[36..39] (star colour x fade), no interpolants, no
// textures. Assembled with llvm-mc -mcpu=bonaire; header token + OrbShdr
// trailer as the other shaders (literal 5 -> trailer at dword 12). Uses s0-s7
// + VCC (header writes vcc_hi) and v0-v3 -> PGM_RSRC1 0x40 (16 SGPR, 4 VGPR).
static const uint32_t ps_stars_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000005, /* s_mov_b32 vcc_hi, 5 (SDK header token)      */
    0xC0820124,             /* s_load_dwordx4 s[4:7], s[0:1], 0x24 (desc[36]) */
    0xBF8C007F,             /* s_waitcnt lgkmcnt(0)                          */
    0x7E000204, 0x7E020205, /* v_mov_b32 v0, s4 ; v_mov_b32 v1, s5           */
    0x7E040206, 0x7E060207, /* v_mov_b32 v2, s6 ; v_mov_b32 v3, s7           */
    0xF800180F, 0x03020100, /* exp mrt0 v0, v1, v2, v3 done vm               */
    0xBF810000,             /* s_endpgm                                      */
    0xBF800000,             /* s_nop 0 (pad: trailer at an even dword)       */
    0x5362724F, 0x00726468, 0x00003000, 0x00000000, /* OrbShdr, length 48 B   */
    0xDEADBEEF, 0xCAFE0200, 0x00000000,
};
