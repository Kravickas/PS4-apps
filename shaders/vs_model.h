#pragma once
#include <stdint.h>

// Model VS (vs_model.s): model transform on the GPU.
//   s[0:3] = V# (MVP at +0, vertices at +80, 48 bytes each), s[4:15] = M rows
//   (m00 m01 m02 tx / m10 m11 m12 ty / m20 m21 m22 tz).
//   Vertex: (x, y, z, handedness), (nx, ny, nz, tx), (u, v, ty, tz).
//   world = M (x, y, z, 1); normal, tangent = M3x3 * n, t; pos0 = MVP * (world, 1).
//   param0 = (u, v, n.y, n.z), param1 = (world, n.x), param2 = (tangent, handedness).
// With M = identity pos0 / param0 / param1 are bit-identical to vs_shader.
// PGM_RSRC1 0x8c (49 VGPRs, 18 SGPRs incl. VCC). Hash CAFE0131.
static const uint32_t vs_model_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000026, 0x7E020280, 0xE0381000, 0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085, 0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601, 0xE0381020, 0x80002C01, 0xBF8C0070, 0x10480404,
    0x3E480605, 0x3E480806, 0x104A0408, 0x3E4A0609, 0x3E4A080A, 0x104C040C, 0x3E4C060D, 0x3E4C080E,
    0x06484807, 0x064A4A0B, 0x064C4C0F, 0x104E0C04, 0x3E4E0E05, 0x3E4E1006, 0x10500C08, 0x3E500E09,
    0x3E50100A, 0x10520C0C, 0x3E520E0D, 0x3E52100E, 0x10541204, 0x3E545C05, 0x3E545E06, 0x10561208,
    0x3E565C09, 0x3E565E0A, 0x1060120C, 0x3E605C0D, 0x3E605E0E, 0x1038490C, 0x3E384B0D, 0x3E384D0E,
    0x0638390F, 0x103A4910, 0x3E3A4B11, 0x3E3A4D12, 0x063A3B13, 0x103C4914, 0x3E3C4B15, 0x3E3C4D16,
    0x063C3D17, 0x103E4918, 0x3E3E4B19, 0x3E3E4D1A, 0x063E3F1B, 0xF80008CF, 0x1F1E1D1C, 0xF800020F,
    0x29282D2C, 0xF800021F, 0x27262524, 0xF800022F, 0x05302B2A, 0xBF810000, 0x5362724F, 0x00726468,
    0x00013800, 0x00000000, 0xDEADBEEF, 0xCAFE0131, 0x00000000,
};
