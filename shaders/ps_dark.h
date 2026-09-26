#pragma once
#include <stdint.h>

// === ps_sky: physical sky gradient + sun disc ===
// attr0.x = clip_x, attr0.y = clip_y (screen-space)
// Sky PS: dynamic day/sunset/night gradient + warm sun disc
// Loads:
//   s[4:7]   sun at desc[16]: x (aspect-scaled NDC), y (NDC), z = disc radius²,
//            w = disc HDR factor (SUN_HDR / MOON_HDR)
//   s[8:11]  zenith RGB at desc[24]
//   s[12:15] horizon RGB at desc[28]
//   s[16:19] light_color RGB at desc[32]
// Pipeline:
//   t = 0.5*(1 - clip_y)    // gradient param: 0=top, 1=bottom
//   sky.rgb = lerp(zenith, horizon, t)
//   d² = (clip_x - sun_x)² + (clip_y - sun_y)²   (clip_x/sun_x pre-scaled by W/H)
//   sun_factor = clamp(1 - d²/radius², 0, 1)²      (decoded: dwords 35-45)
//   output.rgb = lerp(sky, light_color * desc[19], sun_factor)  (dwords 21-23)
// Hash CAFE00E3.
static const uint32_t ps_dark_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001E, 0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */,
    0xC0820110, 0xC0840118, 0xC086011C,
    0xC0880120, 0xBF8C007F, 0xC8080000,
    0xC8090001, 0xC80C0100, 0xC80D0101,
    0x7E140208, 0x7E160209, 0x7E18020A,
    0x7E1A020C, 0x7E1C020D, 0x7E1E020E,
    0x7E200210, 0x7E220211, 0x7E240212,
    0x10202007, 0x10222207, 0x10242407 /* v16..v18 *= s7: desc[19] sun/moon HDR */,
    0x7E260204, 0x7E280205, 0x7E2A0206,
    0x082C06F2, 0x102C2CF0, 0x082E150D,
    0x102E2D17, 0x062E1517, 0x0830170E,
    0x10302D18, 0x06301718, 0x0832190F,
    0x10322D19, 0x06321919, 0x08342702,
    0x08362903, 0x1034351A, 0x1036371B,
    0x0634371A, 0x7E385515, 0x1034391A,
    0x083434F2, 0x20343480, 0x1E3434F2,
    0x1034351A, 0x08362F10, 0x1036351B,
    0x062E2F1B, 0x08363111, 0x1036351B,
    0x0630311B, 0x08363312, 0x1036351B,
    0x0632331B, 0x7E3402F2, 0xF800080F,
    0x1A191817, 0xBF810000, 0x5362724F,
    0x00726468, 0x0000F800, 0x00000000,
    0xDEADBEEF, 0xCAFE00E3, 0x00000000,
};
