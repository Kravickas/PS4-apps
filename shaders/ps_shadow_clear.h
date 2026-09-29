#pragma once
#include <stdint.h>

// Shadow-pass clear PS: outputs (1, 0, 0, 1) - R 1.0, "nothing occluding". Drawn as a fullscreen
// quad before the cube in the shadow pass, filling the shadow map with the "no occluder" value.
// Hash CAFE0108.
static const uint32_t ps_shadow_clear_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, /* prologue */
    0x7E5002F2,             /* v40 = 1.0 (R) */
    0x7E520280,             /* v41 = 0 (G) */
    0x7E540280,             /* v42 = 0 (B) */
    0x7E5602F2,             /* v43 = 1.0 (A) */
    0xF800080F, 0x2B2A2928, /* EXP MRT0 v[40:43] done vm */
    0xBF810000,             /* s_endpgm */
    /* OrbShdr footer: 9 dwords = 36 bytes = 0x24 */
    0x5362724F, 0x00726468, 0x00002400, 0x00000000, 0xDEADBEEF, 0xCAFE0108,
    0x00000000, /* fresh hash */
};
