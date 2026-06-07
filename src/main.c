// ============================================================================
// PS4 Spinning Cube Homebrew — VUID-06887 Depth/Stencil Test
// ============================================================================
//
// Tests the exact scenario that triggers Vulkan VUID-06887:
//   depth_write = OFF (depth aspect read-only)
//   stencil_write = ON  (stencil aspect read-write)
//
// This requires VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
// in the Vulkan backend. PS4 hardware handles this natively.
//
// Architecture:
//   - Vertex buffer built on CPU each frame (rotation applied on CPU)
//   - VS reads position + color from buffer, exports pos0 + param0
//   - PS interpolates color, exports to MRT0
//   - Depth test enabled (Less), depth write disabled
//   - Stencil test enabled (Always), stencil write on zpass (ReplaceTest)
//   - D32SfloatS8Uint depth+stencil buffer with separate planes
//   - Color buffer registered with VideoOut for display
//
// GCN shaders hand-assembled for Sea Islands (CI) ISA.
// All register offsets verified against shadPS4 regs.cpp static_asserts.
// ============================================================================

#include <stdint.h>
#include "pm4.h"

// ============================================================================
// PS4 API declarations (resolved by dynamic linker at runtime)
// ============================================================================
extern int  sceKernelAllocateDirectMemory(long searchStart, long searchEnd,
                                          unsigned long length, unsigned long alignment,
                                          int memoryType, long* physAddrOut);
extern int  sceKernelMapDirectMemory(void** addr, unsigned long length, int prot,
                                     int flags, long physAddr, unsigned long alignment);
extern int  sceKernelUsleep(unsigned int usec);
extern int  sceVideoOutOpen(int userId, int busType, int index, void* param);
extern int  sceVideoOutSetFlipRate(int handle, int rate);
extern void sceVideoOutSetBufferAttribute(void* attr, uint32_t pixelFormat,
                                          uint32_t tilingMode, uint32_t aspectRatio,
                                          uint32_t width, uint32_t height, uint32_t pitchInPixel);
extern int  sceVideoOutRegisterBuffers(int handle, int startIndex, void* const* addresses,
                                       int bufferNum, void* attr);
extern int  sceVideoOutSubmitFlip(int handle, int bufferIndex, uint32_t flipMode, int64_t flipArg);
extern int  sceGnmSubmitCommandBuffers(uint32_t count, const uint32_t* dcbGpuAddrs[],
                                       uint32_t* dcbSizesInBytes, const uint32_t* ccbGpuAddrs[],
                                       uint32_t* ccbSizesInBytes);
extern int  sceGnmSubmitDone(void);
extern int  printf(const char* fmt, ...);

// ============================================================================
// Constants
// ============================================================================
#define DISPLAY_W       1920
#define DISPLAY_H       1080
#define NUM_FRAMES      2
#define DCB_SIZE        0x10000  // 64KB per DCB
#define CUBE_VERTS      36      // 6 faces * 2 tris * 3 verts
#define VERT_STRIDE     32      // 2x vec4: position + color
#define VERT_BUF_SIZE   (CUBE_VERTS * VERT_STRIDE)

// VideoOut constants
#define SCE_VIDEO_OUT_BUS_TYPE_MAIN     0
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB    0x80000000
#define SCE_VIDEO_OUT_TILING_MODE_LINEAR             0
#define SCE_VIDEO_OUT_ASPECT_RATIO_16_9              0

// Memory protection: read + write + GPU read/write
#define PROT_CPU_RW     0x03
#define MAP_FLAGS       0x00
#define MEM_TYPE_FLEX   0x03  // CPU + GPU accessible

// ============================================================================
// GCN Shader Binaries (Sea Islands / CI ISA)
// ============================================================================
// All encodings verified against AMD GCN ISA Architecture manual.
// OrbShdr footer included for shadPS4's SearchBinaryInfo().

// --- Vertex Shader ---
// Input:  VGPR0 = vertex_id (from VGT, vgpr_comp_cnt=0)
// User:   s[0:3] = V# buffer descriptor (vertex buffer, 32-byte stride)
// Output: pos0 = position (vec4), param0 = color (vec4)
//
// Code:
//   v_lshlrev_b32 v8, 5, v0              ; v8 = vertex_id * 32
//   buffer_load_dwordx4 v[0:3], v8, s[0:3], 0 offen        ; position
//   buffer_load_dwordx4 v[4:7], v8, s[0:3], 0 offen off:16 ; color
//   s_waitcnt vmcnt(0)
//   exp pos0, v0, v1, v2, v3
//   exp param0, v4, v5, v6, v7 done
//   s_endpgm
static const uint32_t vs_shader_binary[] = {
    0xBEEB03FF, 0x00000006,     // s_mov_b32 vcc_hi, 6 (OrbShdr offset)
    0x28100085,                 // v_lshlrev_b32 v8, 5, v0
    0xE0381000, 0x80000008,     // buffer_load_dwordx4 v[0:3], v8, s[0:3], offen (soffset=0x80=inline_zero)
    0xE0381010, 0x80000408,     // buffer_load_dwordx4 v[4:7], v8, s[0:3], offen off:16 (soffset=0x80)
    0xBF8C1F70,                 // s_waitcnt vmcnt(0)
    0xF80000CF, 0x03020100,     // exp pos0, v0, v1, v2, v3
    0xF8000A0F, 0x07060504,     // exp param0, v4, v5, v6, v7 done
    0xBF810000,                 // s_endpgm
    0xBF800000,                 // s_nop (padding to even dword count)
    // OrbShdr footer (type=1=VS, length=56 bytes = 14 code dwords including header)
    0x5362724F, 0x00726468,     // "OrbShdr\0" (little-endian)
    0x00003804, 0x00000000,     // version=0, type=VS(1), length=56 (14 dwords: header+code)
    0x12345678, 0xDEADBEEF,     // shader_hash
    0x00000000,                 // crc32 (not validated)
};

// --- Pixel Shader ---
// Input:  VGPR0 = I (persp center), VGPR1 = J (persp center)
// Interp: attr0 = color from VS param0 (4 components)
// Output: mrt0 = interpolated color
//
// Code:
//   v_interp_p1_f32 v2, v0, attr0.x  ;  v_interp_p2_f32 v2, v1, attr0.x
//   v_interp_p1_f32 v3, v0, attr0.y  ;  v_interp_p2_f32 v3, v1, attr0.y
//   v_interp_p1_f32 v4, v0, attr0.z  ;  v_interp_p2_f32 v4, v1, attr0.z
//   v_interp_p1_f32 v5, v0, attr0.w  ;  v_interp_p2_f32 v5, v1, attr0.w
//   exp mrt0, v2, v3, v4, v5 done vm=1
//   s_endpgm
static const uint32_t ps_shader_binary[] = {
    0xBEEB03FF, 0x00000006,     // s_mov_b32 vcc_hi, 6 (OrbShdr offset)
    0xC8080000,                 // v_interp_p1_f32 v2, v0, attr0.x
    0xC8090001,                 // v_interp_p2_f32 v2, v1, attr0.x
    0xC80C0100,                 // v_interp_p1_f32 v3, v0, attr0.y
    0xC80D0101,                 // v_interp_p2_f32 v3, v1, attr0.y
    0xC8100200,                 // v_interp_p1_f32 v4, v0, attr0.z
    0xC8110201,                 // v_interp_p2_f32 v4, v1, attr0.z
    0xC8140300,                 // v_interp_p1_f32 v5, v0, attr0.w
    0xC8150301,                 // v_interp_p2_f32 v5, v1, attr0.w
    0xF800180F, 0x05040302,     // exp mrt0, v2, v3, v4, v5 done vm=1
    0xBF810000,                 // s_endpgm
    0xBF800000,                 // s_nop (padding)
    // OrbShdr footer (type=0=PS, length=56 bytes = 14 code dwords including header)
    0x5362724F, 0x00726468,     // "OrbShdr\0" (little-endian)
    0x00003800, 0x00000000,     // version=0, type=PS(0), length=56 (14 dwords: header+code)
    0x12345678, 0xDEADBEEF,     // shader_hash
    0x00000000,                 // crc32
};

// ============================================================================
// Minimal libc replacements
// ============================================================================
static void my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d;
    for (unsigned long i = 0; i < n; i++) p[i] = (unsigned char)v;
}

static void my_memcpy(void* dst, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
}

// ============================================================================
// GPU memory allocation
// ============================================================================
static void* gpu_alloc(unsigned long size, unsigned long align) {
    long phys = 0;
    void* addr = 0;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, MEM_TYPE_FLEX, &phys))
        return 0;
    if (sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW, MAP_FLAGS, phys, align))
        return 0;
    my_memset(addr, 0, size);
    return addr;
}

// ============================================================================
// Math helpers (no libm dependency)
// ============================================================================
static float my_sin(float x) {
    // Normalize to [-pi, pi]
    const float PI = 3.14159265358979f;
    const float TWO_PI = 6.28318530717959f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    // Taylor series: sin(x) ≈ x - x³/6 + x⁵/120 - x⁷/5040 + x⁹/362880
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    float x9 = x7 * x2;
    return x - x3 / 6.0f + x5 / 120.0f - x7 / 5040.0f + x9 / 362880.0f;
}

static float my_cos(float x) {
    const float HALF_PI = 1.57079632679490f;
    return my_sin(x + HALF_PI);
}

// ============================================================================
// Cube geometry
// ============================================================================
// 8 unique vertices of a unit cube centered at origin
static const float cube_positions[8][3] = {
    {-0.4f, -0.4f, +0.4f}, // 0: front-bottom-left
    {+0.4f, -0.4f, +0.4f}, // 1: front-bottom-right
    {+0.4f, +0.4f, +0.4f}, // 2: front-top-right
    {-0.4f, +0.4f, +0.4f}, // 3: front-top-left
    {-0.4f, -0.4f, -0.4f}, // 4: back-bottom-left
    {+0.4f, -0.4f, -0.4f}, // 5: back-bottom-right
    {+0.4f, +0.4f, -0.4f}, // 6: back-top-right
    {-0.4f, +0.4f, -0.4f}, // 7: back-top-left
};

// 12 triangles (6 faces * 2 tris), indices into cube_positions
static const int cube_indices[12][3] = {
    {0, 1, 2}, {0, 2, 3}, // front  (z=+0.4) — red
    {5, 4, 7}, {5, 7, 6}, // back   (z=-0.4) — green
    {4, 0, 3}, {4, 3, 7}, // left   (x=-0.4) — blue
    {1, 5, 6}, {1, 6, 2}, // right  (x=+0.4) — yellow
    {3, 2, 6}, {3, 6, 7}, // top    (y=+0.4) — cyan
    {4, 5, 1}, {4, 1, 0}, // bottom (y=-0.4) — magenta
};

// Per-face colors (RGBA float)
static const float face_colors[6][4] = {
    {1.0f, 0.2f, 0.2f, 1.0f}, // front  — red
    {0.2f, 1.0f, 0.2f, 1.0f}, // back   — green
    {0.2f, 0.2f, 1.0f, 1.0f}, // left   — blue
    {1.0f, 1.0f, 0.2f, 1.0f}, // right  — yellow
    {0.2f, 1.0f, 1.0f, 1.0f}, // top    — cyan
    {1.0f, 0.2f, 1.0f, 1.0f}, // bottom — magenta
};

// Build transformed vertex buffer for current frame
// Layout per vertex: float[4] position_clip, float[4] color
static void build_vertex_buffer(float* vb, float angle_y, float angle_x) {
    float sy = my_sin(angle_y), cy = my_cos(angle_y);
    float sx = my_sin(angle_x), cx = my_cos(angle_x);

    // Perspective projection parameters
    float aspect = (float)DISPLAY_W / (float)DISPLAY_H;
    float fov_rad = 1.0472f; // 60 degrees
    float f = 1.0f / (fov_rad * 0.5f); // simplified: ~1.91
    // Use a proper tan approximation
    float half_fov = fov_rad * 0.5f;
    float tan_half = my_sin(half_fov) / my_cos(half_fov);
    f = 1.0f / tan_half;

    float near_val = 0.1f, far_val = 100.0f;
    float range_inv = 1.0f / (near_val - far_val);

    // Camera at z=2.0, looking at origin
    float cam_z = 2.0f;

    for (int tri = 0; tri < 12; tri++) {
        int face = tri / 2;
        for (int v = 0; v < 3; v++) {
            int vi = cube_indices[tri][v];
            float px = cube_positions[vi][0];
            float py = cube_positions[vi][1];
            float pz = cube_positions[vi][2];

            // Rotate around Y axis
            float rx = px * cy + pz * sy;
            float ry = py;
            float rz = -px * sy + pz * cy;

            // Rotate around X axis
            float fx = rx;
            float fy = ry * cx - rz * sx;
            float fz = ry * sx + rz * cx;

            // Translate (camera at z=cam_z, looking at origin → move object back)
            float vz = fz - cam_z;

            // Perspective projection (produces clip-space coordinates)
            float clip_x = fx * f / aspect;
            float clip_y = fy * f;
            float clip_z = (vz * (far_val + near_val) * range_inv +
                            2.0f * far_val * near_val * range_inv);
            float clip_w = -vz;

            // Write position
            int idx = (tri * 3 + v) * 8; // 8 floats per vertex
            vb[idx + 0] = clip_x;
            vb[idx + 1] = clip_y;
            vb[idx + 2] = clip_z;
            vb[idx + 3] = clip_w;

            // Write color
            vb[idx + 4] = face_colors[face][0];
            vb[idx + 5] = face_colors[face][1];
            vb[idx + 6] = face_colors[face][2];
            vb[idx + 7] = face_colors[face][3];
        }
    }
}

// ============================================================================
// Build V# buffer descriptor (4 dwords)
// Type=0 (raw buffer), suitable for buffer_load_dwordx4 with offen
// ============================================================================
static void build_buffer_vsharp(uint32_t* vsharp, void* base, uint32_t size_bytes) {
    uint64_t addr = (uint64_t)(uintptr_t)base;
    vsharp[0] = (uint32_t)(addr & 0xFFFFFFFFu);
    vsharp[1] = (uint32_t)(addr >> 32) & 0xFFFF; // stride=0 for raw buffer
    vsharp[2] = size_bytes;
    // DST_SEL=XYZW(0,1,2,3), NUM_FORMAT=UINT(4), DATA_FORMAT=32(4), TYPE=RAW(0)
    vsharp[3] = (0u) | (1u << 3) | (2u << 6) | (3u << 9) |
                (4u << 12) | (4u << 15) | (0u << 27);
}

// ============================================================================
// Build the PM4 draw command buffer
// ============================================================================
static uint32_t build_dcb(struct PM4Builder* b,
                          const uint32_t* vs_addr, const uint32_t* ps_addr,
                          const uint32_t* vsharp,
                          void* color_base, void* depth_base, void* stencil_base,
                          volatile uint32_t* fence_addr, uint32_t fence_value) {
    // --- Context Control ---
    pm4_context_control(b);

    // --- VS program setup (SH registers) ---
    {
        uint64_t vs_gpu = (uint64_t)(uintptr_t)vs_addr;
        uint32_t vs_lo = (uint32_t)(vs_gpu >> 8);
        uint32_t vs_hi = (uint32_t)(vs_gpu >> 40);

        // PGM_LO, PGM_HI, RSRC1, RSRC2 (4 consecutive regs)
        uint32_t vs_regs[4] = {
            vs_lo,
            vs_hi,
            // RSRC1: num_vgprs=3(16 VGPRs), num_sgprs=0(8 SGPRs),
            //        vgpr_comp_cnt=0(vertex_id only)
            3u,
            // RSRC2: scratch_en=0, num_user_regs=4(s[0:3] for V#)
            (4u << 1),
        };
        pm4_set_sh_regs(b, SH_VS_PGM_LO, vs_regs, 4);

        // VS user data: s[0:3] = V# buffer descriptor
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, vsharp, 4);
    }

    // --- PS program setup (SH registers) ---
    {
        uint64_t ps_gpu = (uint64_t)(uintptr_t)ps_addr;
        uint32_t ps_lo = (uint32_t)(ps_gpu >> 8);
        uint32_t ps_hi = (uint32_t)(ps_gpu >> 40);

        uint32_t ps_regs[4] = {
            ps_lo,
            ps_hi,
            // RSRC1: num_vgprs=2(12 VGPRs), num_sgprs=0(8 SGPRs)
            2u,
            // RSRC2: num_user_regs=0
            0u,
        };
        pm4_set_sh_regs(b, SH_PS_PGM_LO, ps_regs, 4);
    }

    // --- Screen scissor (0xA00C): full screen ---
    {
        // Top-left: (0, 0), Bottom-right: (1920, 1080)
        // Scissor format: top_left packed, bottom_right packed
        uint32_t scissor[2] = {0, 0};
        // top_left_x[14:0]=0, top_left_y[30:16]=0
        scissor[0] = 0;
        // bottom_right_x[14:0]=1920, bottom_right_y[30:16]=1080
        scissor[1] = (DISPLAY_W & 0x7FFF) | ((DISPLAY_H & 0x7FFF) << 16);
        pm4_set_context_regs(b, CTX_SCREEN_SCISSOR, scissor, 2);
    }

    // --- Generic scissor (0xA090): full screen ---
    // ViewportScissor format: x[14:0], y[29:15] — 15-bit packed (not 16-bit like screen scissor)
    {
        uint32_t scissor[2];
        scissor[0] = 0;
        scissor[1] = (DISPLAY_W & 0x7FFF) | ((DISPLAY_H & 0x7FFF) << 15);
        pm4_set_context_regs(b, CTX_GENERIC_SCISSOR, scissor, 2);
    }

    // --- Window scissor (0xA081): full screen (15-bit packed) ---
    {
        uint32_t scissor[2];
        scissor[0] = (1u << 30); // window_offset_disable = 1
        scissor[1] = (DISPLAY_W & 0x7FFF) | ((DISPLAY_H & 0x7FFF) << 15);
        pm4_set_context_regs(b, CTX_WINDOW_SCISSOR, scissor, 2);
    }

    // --- Viewport scissor 0 (0xA094): full screen (15-bit packed) ---
    {
        uint32_t scissor[2];
        scissor[0] = 0;
        scissor[1] = (DISPLAY_W & 0x7FFF) | ((DISPLAY_H & 0x7FFF) << 15);
        pm4_set_context_regs(b, CTX_VIEWPORT_SCISSOR0, scissor, 2);
    }

    // --- Viewport transform 0 (0xA10F): map NDC to screen ---
    {
        struct PM4Builder* pb = b;
        pm4_emit(pb, pm4_type3(PM4_SET_CONTEXT_REG, 7));
        pm4_emit(pb, CTX_VIEWPORT0);
        pm4_emit_f(pb, (float)DISPLAY_W * 0.5f);    // xscale
        pm4_emit_f(pb, (float)DISPLAY_W * 0.5f);    // xoffset
        pm4_emit_f(pb, (float)DISPLAY_H * -0.5f);   // yscale (negative = Y flip)
        pm4_emit_f(pb, (float)DISPLAY_H * 0.5f);    // yoffset
        pm4_emit_f(pb, 0.5f);                        // zscale
        pm4_emit_f(pb, 0.5f);                        // zoffset
    }

    // --- Index offset ---
    pm4_set_context_reg(b, CTX_INDEX_OFFSET, 0);

    // =====================================================================
    // Depth + Stencil configuration — THE VUID-06887 TEST
    // =====================================================================

    // DB_RENDER_CONTROL (0xA000): normal rendering, no clear/copy
    pm4_set_context_reg(b, CTX_DEPTH_RENDER_CONTROL, 0);

    // DB_DEPTH_VIEW (0xA002): single slice, depth READ-ONLY, stencil WRITABLE
    // z_read_only=1(bit24), stencil_read_only=0(bit25)
    pm4_set_context_reg(b, CTX_DEPTH_VIEW, 0x01000000u);

    // DB_RENDER_OVERRIDE (0xA003): defaults
    pm4_set_context_reg(b, CTX_DEPTH_RENDER_OVERRIDE, 0);

    // DB_Z_INFO (0xA010): Z32Float format, 1 sample
    // format=3(Z32Float)[1:0], num_samples=0[3:2], tile_mode_index=0[22:20]
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 3u);

    // DB_STENCIL_INFO (0xA011): Stencil8 format
    pm4_set_context_reg(b, CTX_DB_STENCIL_INFO, 1u);

    // DB_Z_READ_BASE, DB_STENCIL_READ_BASE, DB_Z_WRITE_BASE, DB_STENCIL_WRITE_BASE
    {
        uint32_t z_base = (uint32_t)((uint64_t)(uintptr_t)depth_base >> 8);
        uint32_t s_base = (uint32_t)((uint64_t)(uintptr_t)stencil_base >> 8);
        uint32_t db_regs[4] = {z_base, s_base, z_base, s_base};
        pm4_set_context_regs(b, CTX_DB_Z_READ_BASE, db_regs, 4);
    }

    // DB_DEPTH_SIZE (0xA016): pitch and height in tiles
    {
        uint32_t pitch_tile_max = (DISPLAY_W / 8) - 1;   // 239
        uint32_t height_tile_max = (DISPLAY_H / 8) - 1;  // 134
        uint32_t depth_size = (pitch_tile_max & 0x7FF) | ((height_tile_max & 0x7FF) << 11);
        pm4_set_context_reg(b, CTX_DB_DEPTH_SIZE, depth_size);
    }

    // DB_DEPTH_SLICE (0xA017): slice size
    {
        uint32_t tile_max = (DISPLAY_W * DISPLAY_H / 64) - 1; // 32399
        pm4_set_context_reg(b, CTX_DB_DEPTH_SLICE, tile_max & 0x3FFFFF);
    }

    // DB_DEPTH_CONTROL (0xA200):
    //   stencil_enable=1[0], depth_enable=1[1], depth_write_enable=0[2],
    //   depth_func=Less(1)[6:4], backface_enable=1[7],
    //   stencil_ref_func=Always(7)[10:8]
    {
        uint32_t depth_ctl = (1u << 0) |  // stencil_enable = 1
                             (1u << 1) |  // depth_enable = 1
                             (0u << 2) |  // depth_write_enable = 0 (KEY: depth read-only!)
                             (1u << 4) |  // depth_func = Less
                             (1u << 7) |  // backface_enable
                             (7u << 8) |  // stencil_ref_func = Always (front)
                             (7u << 20);  // stencil_bf_func = Always (back)
        pm4_set_context_reg(b, CTX_DEPTH_CONTROL, depth_ctl);
    }

    // DB_STENCIL_CONTROL (0xA10B):
    //   stencil_fail_front=Keep(0), stencil_zpass_front=ReplaceTest(3),
    //   stencil_zfail_front=Keep(0), back same
    {
        uint32_t stencil_ctl = (0u << 0) |   // fail_front = Keep
                               (3u << 4) |   // zpass_front = ReplaceTest (WRITES stencil!)
                               (0u << 8) |   // zfail_front = Keep
                               (0u << 12) |  // fail_back = Keep
                               (3u << 16) |  // zpass_back = ReplaceTest
                               (0u << 20);   // zfail_back = Keep
        pm4_set_context_reg(b, CTX_STENCIL_CONTROL, stencil_ctl);
    }

    // DB_STENCILREFMASK (0xA10C): stencil ref=0x42, mask=0xFF, writemask=0xFF
    {
        uint32_t ref_front = 0x42u | (0xFFu << 8) | (0xFFu << 16) | (0x42u << 24);
        pm4_set_context_reg(b, CTX_STENCIL_REF_FRONT, ref_front);
        pm4_set_context_reg(b, CTX_STENCIL_REF_BACK, ref_front);
    }

    // =====================================================================
    // Color buffer (CB) configuration
    // =====================================================================

    // CB_COLOR0_BASE (0xA318): framebuffer address >> 8
    {
        uint32_t cb_base = (uint32_t)((uint64_t)(uintptr_t)color_base >> 8);
        pm4_set_context_reg(b, CTX_CB_COLOR0_BASE, cb_base);
    }

    // CB_COLOR0_PITCH (0xA319): tile_max = (width/8)-1
    pm4_set_context_reg(b, CTX_CB_COLOR0_PITCH, (DISPLAY_W / 8) - 1);

    // CB_COLOR0_SLICE (0xA31A): tile_max = (width*height/64)-1
    pm4_set_context_reg(b, CTX_CB_COLOR0_SLICE, (DISPLAY_W * DISPLAY_H / 64) - 1);

    // CB_COLOR0_VIEW (0xA31B): single slice
    pm4_set_context_reg(b, CTX_CB_COLOR0_VIEW, 0);

    // CB_COLOR0_INFO (0xA31C):
    //   endian=0[1:0], format=10(COLOR_8_8_8_8)[6:2], linear_general=0[7],
    //   number_type=7(SRGB)[10:8], comp_swap=0[12:11]
    {
        uint32_t cb_info = (0u << 0) |    // endian = none
                           (10u << 2) |   // format = COLOR_8_8_8_8
                           (0u << 7) |    // linear_general = 0
                           (7u << 8) |    // number_type = SRGB
                           (0u << 11);    // comp_swap = standard
        pm4_set_context_reg(b, CTX_CB_COLOR0_INFO, cb_info);
    }

    // CB_COLOR0_ATTRIB (0xA31D): tile_mode_index=8(DisplayLinearAligned)
    pm4_set_context_reg(b, CTX_CB_COLOR0_ATTRIB, 8u);

    // CB_TARGET_MASK (0xA08E): enable RGBA writes for target 0
    pm4_set_context_reg(b, CTX_COLOR_TARGET_MASK, 0x0000000Fu);

    // CB_SHADER_MASK (0xA08F): enable RGBA from shader for target 0
    pm4_set_context_reg(b, CTX_COLOR_SHADER_MASK, 0x0000000Fu);

    // =====================================================================
    // Pipeline state
    // =====================================================================

    // SPI_PS_INPUT_CNTL_0 (0xA191): input_offset=0, use_default=0
    pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0, 0);

    // SPI_VS_OUT_CONFIG (0xA1B1): 1 param export (export_count_min_one=0)
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 0);

    // SPI_PS_INPUT_ENA (0xA1B3): persp_center_ena=1 (bit 1)
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x00000002u);

    // SPI_PS_INPUT_ADDR (0xA1B4): persp_center_ena=1
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x00000002u);

    // SPI_PS_IN_CONTROL (0xA1B6): num_interp=1
    pm4_set_context_reg(b, CTX_NUM_INTERP, 1);

    // SPI_SHADER_POS_FORMAT (0xA1C3): pos0=FourComp(4)
    pm4_set_context_reg(b, CTX_SHADER_POS_FORMAT, 4);

    // SPI_SHADER_Z_FORMAT (0xA1C4): no Z export from PS
    pm4_set_context_reg(b, CTX_Z_EXPORT_FORMAT, 0);

    // SPI_SHADER_COL_FORMAT (0xA1C5): MRT0=ABGR_32(9)
    pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);

    // CB_COLOR_CONTROL (0xA202): mode=Normal, rop3=0xCC(copy)
    // Value from shadPS4 init sequence (known working)
    pm4_set_context_reg(b, CTX_COLOR_CONTROL, 0x00CC0010u);

    // DB_SHADER_CONTROL (0xA203): no Z/stencil export from PS, LateZ order
    pm4_set_context_reg(b, 0x203, 0);

    // PA_CL_CLIP_CNTL (0xA204): defaults (clipping enabled)
    pm4_set_context_reg(b, CTX_CLIPPER_CONTROL, 0);

    // PA_CL_VTE_CNTL (0xA206): all viewport transforms enabled
    pm4_set_context_reg(b, CTX_VIEWPORT_CONTROL, 0x0000043Fu);

    // PA_SU_VTX_CNTL (0xA207): vs_out_control defaults
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONTROL, 0);

    // PA_SU_SC_MODE_CNTL — PolygonControl (0xA205)
    // cull_front=0[0], cull_back=1[1], front_face=CCW(1)[2]
    pm4_set_context_reg(b, 0x205, 0x00000006u);

    // PA_SC_MODE_CNTL_0 — ModeControl (0xA292)
    pm4_set_context_reg(b, CTX_MODE_CONTROL, 0);

    // VGT_SHADER_STAGES_EN (0xA2D5): VS only (raw=0)
    pm4_set_context_reg(b, CTX_STAGE_ENABLE, 0);

    // DB_EQAA — AA_CONFIG (0xA2F8): no MSAA
    pm4_set_context_reg(b, CTX_AA_CONFIG, 0);

    // CB_BLEND0_CONTROL (0xA1E0): no blending (opaque)
    pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);

    // VGT_INDEX_TYPE (0xA29D): 32-bit indices (not used for auto draw)
    pm4_set_context_reg(b, CTX_INDEX_SIZE, 0);

    // =====================================================================
    // UCONFIG: Primitive type + instances
    // =====================================================================

    // VGT_PRIMITIVE_TYPE (0xC242): TriangleList=4
    pm4_set_uconfig_reg(b, UCFG_PRIMITIVE_TYPE, 4);

    // VGT_NUM_INSTANCES (0xC24D): 1 instance
    pm4_set_uconfig_reg(b, UCFG_NUM_INSTANCES, 1);

    // =====================================================================
    // Draw!
    // =====================================================================
    pm4_draw_index_auto(b, CUBE_VERTS);

    // =====================================================================
    // Fence: signal completion via BOTTOM_OF_PIPE event
    // =====================================================================
    pm4_event_write_eop(b, fence_addr, fence_value);

    return b->off * 4; // return size in bytes
}

// ============================================================================
// Main entry point
// ============================================================================
int main(void) {
    printf("=== PS4 Spinning Cube — VUID-06887 Depth/Stencil Test ===\n");
    printf("  depth_write = OFF, stencil_write = ON\n");
    printf("  Format: D32SfloatS8Uint (separate depth/stencil planes)\n\n");

    // --- VideoOut setup ---
    int video = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, 0);
    if (video < 0) {
        printf("ERROR: sceVideoOutOpen failed: 0x%08x\n", video);
        return 1;
    }
    sceVideoOutSetFlipRate(video, 0);

    // --- Allocate framebuffers ---
    unsigned long fb_size = (unsigned long)DISPLAY_W * DISPLAY_H * 4;
    void* fb[NUM_FRAMES];
    for (int i = 0; i < NUM_FRAMES; i++) {
        fb[i] = gpu_alloc(fb_size, 0x100000); // 1MB aligned
        if (!fb[i]) {
            printf("ERROR: framebuffer alloc failed\n");
            return 1;
        }
    }

    // Register with VideoOut
    unsigned char buf_attr[48];
    my_memset(buf_attr, 0, sizeof(buf_attr));
    sceVideoOutSetBufferAttribute(buf_attr,
        SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB,
        SCE_VIDEO_OUT_TILING_MODE_LINEAR,
        SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
        DISPLAY_W, DISPLAY_H, DISPLAY_W);
    sceVideoOutRegisterBuffers(video, 0, fb, NUM_FRAMES, buf_attr);

    // --- Allocate depth buffer (D32 plane) ---
    unsigned long depth_size = (unsigned long)DISPLAY_W * DISPLAY_H * 4;
    void* depth_buf = gpu_alloc(depth_size, 0x10000);
    if (!depth_buf) {
        printf("ERROR: depth buffer alloc failed\n");
        return 1;
    }

    // --- Allocate stencil buffer (S8 plane, separate from depth) ---
    unsigned long stencil_size = (unsigned long)DISPLAY_W * DISPLAY_H;
    // Align up to 64KB for GPU
    stencil_size = (stencil_size + 0xFFFF) & ~0xFFFFUL;
    void* stencil_buf = gpu_alloc(stencil_size, 0x10000);
    if (!stencil_buf) {
        printf("ERROR: stencil buffer alloc failed\n");
        return 1;
    }

    // --- Allocate vertex buffer ---
    void* vert_buf = gpu_alloc(VERT_BUF_SIZE + 256, 0x1000); // extra alignment padding
    if (!vert_buf) {
        printf("ERROR: vertex buffer alloc failed\n");
        return 1;
    }

    // --- Copy shader binaries to GPU-visible memory ---
    void* vs_mem = gpu_alloc(sizeof(vs_shader_binary) + 256, 0x1000);
    void* ps_mem = gpu_alloc(sizeof(ps_shader_binary) + 256, 0x1000);
    if (!vs_mem || !ps_mem) {
        printf("ERROR: shader alloc failed\n");
        return 1;
    }
    my_memcpy(vs_mem, vs_shader_binary, sizeof(vs_shader_binary));
    my_memcpy(ps_mem, ps_shader_binary, sizeof(ps_shader_binary));

    // --- Allocate DCBs ---
    uint32_t* dcb_mem[NUM_FRAMES];
    for (int i = 0; i < NUM_FRAMES; i++) {
        dcb_mem[i] = (uint32_t*)gpu_alloc(DCB_SIZE, 0x10000);
        if (!dcb_mem[i]) {
            printf("ERROR: DCB alloc failed\n");
            return 1;
        }
    }

    // --- Allocate fence ---
    volatile uint32_t* fence = (volatile uint32_t*)gpu_alloc(0x1000, 0x1000);
    if (!fence) {
        printf("ERROR: fence alloc failed\n");
        return 1;
    }
    *fence = 0;

    printf("Allocations:\n");
    printf("  FB[0]:    %p\n", fb[0]);
    printf("  FB[1]:    %p\n", fb[1]);
    printf("  Depth:    %p (%lu bytes)\n", depth_buf, depth_size);
    printf("  Stencil:  %p (%lu bytes)\n", stencil_buf, stencil_size);
    printf("  VertBuf:  %p (%d bytes)\n", vert_buf, VERT_BUF_SIZE);
    printf("  VS code:  %p\n", vs_mem);
    printf("  PS code:  %p\n", ps_mem);
    printf("  Fence:    %p\n", (void*)fence);

    // Build V# descriptor for vertex buffer
    uint32_t vsharp[4];
    build_buffer_vsharp(vsharp, vert_buf, VERT_BUF_SIZE);

    printf("\nV# descriptor: %08x %08x %08x %08x\n",
           vsharp[0], vsharp[1], vsharp[2], vsharp[3]);
    printf("\nStarting render loop...\n");

    // =====================================================================
    // Render loop
    // =====================================================================
    uint32_t frame = 0;
    uint32_t fence_val = 1;

    // Clear depth buffer to 1.0f (far plane)
    {
        uint32_t* dp = (uint32_t*)depth_buf;
        uint32_t one_f = 0x3F800000; // 1.0f as uint32
        for (unsigned long i = 0; i < (unsigned long)DISPLAY_W * DISPLAY_H; i++) {
            dp[i] = one_f;
        }
    }

    for (;;) {
        int buf_idx = frame % NUM_FRAMES;

        // --- Update rotation ---
        float angle_y = (float)frame * 0.02f;
        float angle_x = (float)frame * 0.013f;

        // --- Build vertex buffer with current rotation ---
        build_vertex_buffer((float*)vert_buf, angle_y, angle_x);

        // --- Clear framebuffer to dark gray ---
        {
            uint32_t* fb_pixels = (uint32_t*)fb[buf_idx];
            uint32_t clear_color = 0xFF1A1A1A; // ABGR: dark gray, alpha=1
            for (unsigned long i = 0; i < (unsigned long)DISPLAY_W * DISPLAY_H; i++) {
                fb_pixels[i] = clear_color;
            }
        }

        // --- Clear depth to 1.0f each frame ---
        {
            uint32_t* dp = (uint32_t*)depth_buf;
            uint32_t one_f = 0x3F800000;
            for (unsigned long i = 0; i < (unsigned long)DISPLAY_W * DISPLAY_H; i++) {
                dp[i] = one_f;
            }
        }

        // --- Clear stencil to 0 ---
        my_memset(stencil_buf, 0, stencil_size);

        // --- Build DCB ---
        struct PM4Builder pm4;
        pm4_init(&pm4, dcb_mem[buf_idx], DCB_SIZE / 4);

        uint32_t dcb_bytes = build_dcb(&pm4,
            (const uint32_t*)vs_mem, (const uint32_t*)ps_mem,
            vsharp,
            fb[buf_idx], depth_buf, stencil_buf,
            fence, fence_val);

        // --- Submit DCB ---
        const uint32_t* dcb_addrs[1] = {dcb_mem[buf_idx]};
        uint32_t dcb_sizes[1] = {dcb_bytes};
        int ret = sceGnmSubmitCommandBuffers(1, dcb_addrs, dcb_sizes, 0, 0);
        if (ret != 0) {
            printf("Frame %u: SubmitCommandBuffers failed: 0x%08x\n", frame, ret);
        }

        sceGnmSubmitDone();

        // --- Wait for GPU ---
        for (int wait = 0; wait < 1000000; wait++) {
            if (*fence >= fence_val) break;
            sceKernelUsleep(10);
        }

        if (*fence < fence_val) {
            printf("Frame %u: GPU fence timeout! fence=%u expected=%u\n",
                   frame, *fence, fence_val);
        }
        fence_val++;

        // --- Flip ---
        sceVideoOutSubmitFlip(video, buf_idx, 1, 0);

        // --- Verify stencil writes (first frame only) ---
        if (frame == 5) {
            uint8_t* sp = (uint8_t*)stencil_buf;
            int stencil_written = 0;
            for (unsigned long i = 0; i < (unsigned long)DISPLAY_W * DISPLAY_H; i++) {
                if (sp[i] == 0x42) stencil_written++;
            }
            printf("\n=== VUID-06887 Test Results (frame %u) ===\n", frame);
            printf("  Stencil pixels with ref 0x42: %d / %d\n",
                   stencil_written, DISPLAY_W * DISPLAY_H);
            if (stencil_written > 0) {
                printf("  PASS: Stencil writes work with depth_write=OFF\n");
                printf("  (depth read-only + stencil read-write = correct layout)\n");
            } else {
                printf("  FAIL: No stencil writes detected!\n");
                printf("  (Vulkan layout may be rejecting stencil writes)\n");
            }
            printf("==========================================\n\n");
        }

        // --- Frame pacing ---
        sceKernelUsleep(16000); // ~60fps

        frame++;

        // Log periodically
        if ((frame % 300) == 0) {
            printf("Frame %u: running (fence=%u)\n", frame, *fence);
        }
    }

    return 0;
}
