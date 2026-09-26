#pragma once
#include <stdint.h>

// Dedicated shadow-pass VS. Identical bytes to vs_shader_binary but with a
// DIFFERENT hash (0xBEEFEE02CAFE5508 vs 0xAABBEE0247505508). Forces shadPS4 to
// compile and cache a distinct Vulkan pipeline for the shadow pass, so any
// pipeline-cache collision between main-pass VS and shadow-pass VS — which could
// produce double-rendering of the cube into shadow_depth — is eliminated.
/* Same export DONE patch as vs_shader_binary. */
static const uint32_t vs_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000, 0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085, 0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601, 0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C, 0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13, 0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E, 0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80008CF, 0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF800021F, 0x05040302, 0xBF810000, 0xBF800000, 0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0xCAFE5508, 0xBEEFEE02, 0x00000000,
};
