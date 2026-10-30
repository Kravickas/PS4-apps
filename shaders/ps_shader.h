#pragma once
#include <stdint.h>

// Cube / model PS (ps_shader.s): textured + Lambert + light colour + fog.
//   albedo (T# desc[0], S# desc[8]) x (0.0707 + 0.9293 * max(0, N.L)) (sun
//   desc[12]) x light colour desc[32] - unchanged; then the fog of ps_floor:
//   colour = sky gradient at this row (zenith desc[24], horizon desc[28],
//   t = POS_Y * desc[115]), weight min(1, 2^(desc[109] * d + desc[107])) =
//   min(1, FOG_MIN * e^(d / L)), d = |camera desc[104] - world position|.
// VS exports: PARAM0 = (uv, normal.y, normal.z), PARAM1 = (world xyz, normal.x).
// Inputs: PERSP_CENTER + POS_Y_FLOAT (v2). With the fog off the output is
// bit-identical to the original shader (CAFE0115) in float32 emulation, and
// fog weight / colour are bit-identical to ps_floor's for the same point.
// PGM_RSRC1 0x18A (44 VGPRs, s0-s51 + VCC = 56 SGPRs). Hash CAFE0117.
static const uint32_t ps_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000002B, 0xBEFC0302, 0xBEA0047E, 0xBEFE0A7E, 0xC0C80100, 0xC0860108, 0xC08C010C,
    0xC08E0120, 0xC0920168, 0xC014016D, 0xC0148173, 0xC0960118, 0xC098011C, 0xBF8C007F, 0xC80C0000,
    0xC80D0001, 0xC8100100, 0xC8110101, 0xC8280200, 0xC8290201, 0xC82C0300, 0xC82D0301, 0xC8300700,
    0xC8310701, 0xC8140400, 0xC8150401, 0xC8180500, 0xC8190501, 0xC81C0600, 0xC81D0601, 0xF0800F00,
    0x00641003, 0xBF8C0F70, 0x10281818, 0x102A1419, 0x06282B14, 0x102A161A, 0x06282B14, 0x20282880,
    0x102A28FF, 0x3F6DE3F7, 0x062A2AFF, 0x3D90E047, 0x10502B10, 0x10522B11, 0x10542B12, 0x1050501C,
    0x1052521D, 0x1054541E, 0x080A0A24, 0x080C0C25, 0x080E0E26, 0x10100B05, 0x3E100D06, 0x3E100F07,
    0x7E125D08, 0x10101308, 0x10101028, 0x06101027, 0x7E104B08, 0x1E1010F2, 0x081010F2, 0x10120429,
    0x7E300230, 0x0A30302C, 0x10301318, 0x0630302C, 0x7E320231, 0x0A32322D, 0x10321319, 0x0632322D,
    0x7E340232, 0x0A34342E, 0x1034131A, 0x0634342E, 0x08363128, 0x3E30111B, 0x08363329, 0x3E32111B,
    0x0836352A, 0x3E34111B, 0x7E5602F2, 0xBEFE0420, 0xF800180F, 0x2B1A1918, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x00016000, 0x00000000, 0xDEADBEEF, 0xCAFE0117, 0x00000000,
};
