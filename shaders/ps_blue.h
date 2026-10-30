#pragma once
#include <stdint.h>

// White PS for loading bar — solid (1, 1, 1, 1) output, no texture sampling
static const uint32_t ps_blue_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000006, 0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */,
    0xC8080000, 0xC8090001, 0xC80C0100,
    0xC80D0101, 0x7E2802F2, 0x7E2A02F2,
    0x7E2C02F2, 0x7E2E02F2, 0xF800080F,
    0x17161514, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x00003C00,
    0x00000000, 0xDEADBEEF, 0xCAFE0003,
    0x00000000,
};
