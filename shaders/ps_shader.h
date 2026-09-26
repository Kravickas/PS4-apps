#pragma once
#include <stdint.h>

// === Cube PS (hash CAFE0115) — textured + 3D Lambert + light_color tint ===
// (Hash bumped from CAFE0113 → CAFE0114 to defeat shadPS4 pipeline cache after
//  adding the light_color sample + tint multiply ops.)
//
// Samples the main `tex` texture (desc[0..7]) using interpolated UV from the
// VS, then modulates by full 3D Lambert lighting using all three normal
// components (x, y, z) against the sun direction. Final output is tinted by
// `light_color` from desc[32..35] — at sunset/sunrise the tint is warm
// orange so the cube takes on the sky's color; at night it's cool blue.
//
// VS exports (post-fix in vs_shader_binary):
//   PARAM0 = (uv.x, uv.y, normal.y, normal.z)
//   PARAM1 = (world_pos.x, world_pos.y, world_pos.z, normal.x)
//
// Resource bindings:
//   s12-15  sampler       @ desc[8]   (build_ssharp)
//   s16-23  tex T#        @ desc[0]   (build_tsharp — main BMP or embedded logo)
//   s24-27  sun direction @ desc[12]  (sun.x=s24, .y=s25, .z=s26)
//   s28-31  light_color   @ desc[32]  (s28=R, s29=G, s30=B)
//
// VGPR map:
//   v3, v4   = uv.x, uv.y (interpolated)
//   v10, v11 = normal.y, normal.z (from attr0.z, attr0.w)
//   v12      = normal.x (from attr1.w — packed by VS)
//   v16-v19  = albedo RGBA from IMAGE_SAMPLE
//   v20      = lambert dot accumulator
//   v21      = lit factor = 0.0707 + 0.9293*max(0, dot) (linear light:
//              ambient 0.3^2.2; the albedo T# decodes sRGB)
//   v40-v43  = output RGBA = albedo * lit * light_color (alpha forced to 1)
static const uint32_t ps_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF,
    0x00000015,
    0xBEFC0302,
    0xBEA0047E /* s_mov_b64 s[32:33], exec: live mask */,
    0xBEFE0A7E /* s_wqm_b64 exec, exec */ /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */,
    0xC0C80100,
    0xC0860108,
    0xC08C010C,
    0xC08E0120,
    0xBF8C007F,
    0xC80C0000,
    0xC80D0001,
    0xC8100100,
    0xC8110101,
    0xC8280200,
    0xC8290201,
    0xC82C0300,
    0xC82D0301,
    0xC8300700,
    0xC8310701,
    0xF0800F00,
    0x00641003,
    0xBF8C0F70,
    0x10281818,
    0x102A1419,
    0x06282B14,
    0x102A161A,
    0x06282B14,
    0x20282880,
    0x102A28FF,
    0x3F6DE3F7,
    0x062A2AFF,
    0x3D90E047,
    0x10502B10,
    0x10522B11,
    0x10542B12,
    0x1050501C,
    0x1052521D,
    0x1054541E,
    0x7E5602F2,
    0xBEFE0420 /* s_mov_b64 exec, s[32:33]: exact mode for the export */,
    0xF800180F /* exp: vm=1 */,
    0x2B2A2928,
    0xBF810000,
    /* OrbShdr footer: 40 dwords = 160 bytes = 0xA0 */
    0x5362724F,
    0x00726468,
    0x0000B000,
    0x00000000,
    0xDEADBEEF,
    0xCAFE0115,
    0x00000000,
};
