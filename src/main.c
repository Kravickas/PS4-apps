// ============================================================================
// CUBETST00 / ShadCube4 — PS4 3D Model Viewer
// ============================================================================
//
// ARCHITECTURE OVERVIEW:
//
//   ┌─────────────┐     ┌──────────────┐     ┌─────────────┐
//   │  CPU (C)     │     │  GPU (GCN)   │     │   Screen    │
//   │              │     │              │     │             │
//   │ Build MVP    │────>│ VS: MVP*pos  │────>│  Triangles  │
//   │ matrix       │     │ PS: texture  │     │  with       │
//   │ (64 bytes    │     │    × shade   │     │  texture    │
//   │  per frame)  │     │              │     │             │
//   └─────────────┘     └──────────────┘     └─────────────┘
//
// RENDERING PIPELINE:
//   1. CPU computes a 4×4 MVP matrix (rotation + perspective projection)
//   2. CPU writes MVP to GPU buffer (64 bytes, once per frame)
//   3. CPU builds a PM4 command buffer telling the GPU what to draw
//   4. GPU vertex shader loads MVP + vertex data, transforms positions
//   5. GPU pixel shader samples texture, applies lighting, outputs color
//   6. Result goes to the framebuffer → screen
//
// FILE LOADING:
//   On startup, scans /data/CUBETST00/ for:
//     - Any .obj file → parsed as 3D model (Blender export)
//     - Any .bmp file → loaded as texture
//   Falls back to built-in spinning cube + logo texture if nothing found.
//
// BUFFER LAYOUT (single GPU buffer):
//   ┌────────────┬───────────────┬────────────┬──────────────────┐
//   │ Identity   │ BG Quad       │ MVP Matrix │ Model Vertices   │
//   │ Matrix     │ (6 verts)     │ (updated   │ (from .obj or    │
//   │ (64 bytes) │ (192 bytes)   │ per frame) │  built-in cube)  │
//   ├────────────┼───────────────┼────────────┼──────────────────┤
//   │ offset 0   │ offset 64     │ offset 256 │ offset 320       │
//   └────────────┴───────────────┴────────────┴──────────────────┘
//
//   Draw 1: V# base=offset 0   → VS reads identity MVP, draws BG quad
//   Draw 2: V# base=offset 256 → VS reads real MVP, draws 3D model
//
// BUILD: clang (FreeBSD target) → ld.lld → patch_elf.py → eboot.bin
// ============================================================================
// Vertex buffer is STATIC — model-space positions + UVs uploaded once.
// MVP matrix computed on CPU per frame (64 bytes), sent as constant buffer.
// VS does: clip_pos = MVP * model_pos  (4 dot products on GPU)
// PS does: IMAGE_SAMPLE texture, multiply by shade, export MRT0
// ============================================================================

#include <stdint.h>
#include "pm4.h"
#include "logo_texture.h"
#include "nid_resolve.h"
#include "obj_loader.h"
#include "stl_loader.h"
#include "ply_loader.h"
#include "bmp_loader.h"

// memset/memcpy declared in nid_resolve.h

#define DISPLAY_W       1920
#define DISPLAY_H       1080

/* Draw bisect: with the GPU path proven good, isolate which draw faults on
   hardware. Default = stop after the BG/sky draw. Override on the make line:
     EXTRAFLAGS=-DDRAW_STOP=2       BG + floor
     EXTRAFLAGS=-DDRAW_STOP=3       BG + floor + cube (no shadow)
     EXTRAFLAGS=-DDRAW_STOP_OFF     full scene incl. shadow */
/* BRINGUP MILESTONE: gradient confirmed the full draw pipeline works on real
   hardware (correct tiling/layout). Now step up to REAL shaders one draw at a
   time. Default = BG/sky draw with the real VS (vertex fetch + MVP) and real
   sky PS (descriptor table). Then:
     -DDRAW_STOP=2     + floor
     -DDRAW_STOP=3     + cube
     -DDRAW_STOP_OFF   full scene + shadow
     -DRT_TEST         back to the gradient/fulltri diagnostic
     -DMINIMAL_TEST    GPU-DMA magenta, no draws */
#if !defined(RT_TEST) && !defined(DRAW_STOP) && !defined(DRAW_STOP_OFF) && !defined(MINIMAL_TEST)
#define DRAW_STOP 1
#endif
/* CONFIRM the VM=0 export fix in isolation first. Default = VS_LOAD_TEST
   (fulltri+load VS + magenta PS) — the exact build that gave NOISE; with VM
   fixed it should now be SOLID MAGENTA. Opt-in:
     -DBG_PS_MAGENTA_FORCE   real VS + magenta PS
     -DREAL_BG_FORCE         real VS + real sky PS
     -DRT_TEST               gradient/fulltri (known good)
     -DMINIMAL_TEST          GPU-DMA magenta */
#if !defined(REAL_BG_FORCE) && !defined(BG_CLEAN_SKY_FORCE) && !defined(BG_CLEAN_VS_FORCE) && !defined(BG_PS_MAGENTA_FORCE) && !defined(VS_LOAD_TEST_FORCE) && !defined(RT_TEST) && !defined(MINIMAL_TEST) && defined(DRAW_STOP)
#define BG_SKY_CLEAN 1
#endif
#if defined(BG_CLEAN_SKY_FORCE) && !defined(RT_TEST) && !defined(MINIMAL_TEST) && defined(DRAW_STOP)
#define BG_CLEAN_SKY 1
#endif
#if defined(BG_CLEAN_VS_FORCE) && !defined(RT_TEST) && !defined(MINIMAL_TEST) && defined(DRAW_STOP)
#define BG_CLEAN_VS 1
#endif
#if defined(BG_PS_MAGENTA_FORCE) && !defined(RT_TEST) && !defined(MINIMAL_TEST) && defined(DRAW_STOP)
#define BG_PS_MAGENTA 1
#endif
#if defined(VS_LOAD_TEST_FORCE) && !defined(RT_TEST) && !defined(MINIMAL_TEST) && defined(DRAW_STOP)
#define VS_LOAD_TEST 1
#endif
/* Shared simple-draw config (depth off, trivial interp) for the diagnostic VS paths. */
#if defined(RT_TEST) || defined(VS_LOAD_TEST)
#define SIMPLE_DRAW 1
#endif
/* Shadow map: 4096×4096 (4K). Real PS4 games render to 4K shadow maps
   regularly (and bigger). The 32 MB Vulkan validation error in the user's
   log was NOT a GCN/PS4 limit — it was specifically shadPS4's
   `readbackLinearImages: true` config option triggering a per-frame CPU
   readback through a 32 MB StreamBuffer. Our shadow_depth is GPU-only
   (written by shadow PS, sampled by floor PS — never CPU-touched), so
   readback is unnecessary.

   Two ways to use 4K successfully:
     1. Disable `readbackLinearImages` in shadPS4's per-game settings for
        this CUBETST00 entry. We don't need it for the shadow pipeline
        and it's the setting that's triggering the 32 MB cap.
     2. (Future) Switch the shadow CB from linear (cb_info bit 7 = 1,
        tile_mode 8) to tiled (cb_info bit 7 = 0, tile_mode 13
        Display2DThin) so shadPS4 doesn't classify it as a linear image
        regardless of the readback config. This is what real games do but
        requires CB_COLOR_ATTRIB additions and matching T# tile_mode. */
#define SHADOW_W        4096
#define SHADOW_H        4096
#define NUM_FRAMES      2
#define DCB_SIZE        0x20000
#define BG_VERTS        6
#define CUBE_VERTS      36
#define FLOOR_VERTS     24576 /* 64×64 grid of quads, 2 tris each = 8192 tris = 24576 verts */
#define TOTAL_VERTS     (BG_VERTS + CUBE_VERTS + FLOOR_VERTS)
#define VERT_STRIDE     48
#define IDENT_OFF       0
#define BG_SUN_OFF      64
#define BG_DATA_OFF     80
#define MVP_OFF         (BG_DATA_OFF + BG_VERTS * VERT_STRIDE)
#define SUN_DIR_OFF     (MVP_OFF + 64)
#define CUBE_DATA_OFF   (SUN_DIR_OFF + 16)
/* Floor uses its own V# base = FLOOR_MVP_OFF. VS expects MVP at V#+0 and
   vertex data at V#+80 (same as cube). We reserve 80 bytes (MVP mirror +
   SUN_DIR mirror padding) right before floor vertex data. */
#define FLOOR_MVP_OFF   (CUBE_DATA_OFF + CUBE_VERTS * VERT_STRIDE)
#define FLOOR_DATA_OFF  (FLOOR_MVP_OFF + 80)
#define VERT_BUF_SIZE   (FLOOR_DATA_OFF + FLOOR_VERTS * VERT_STRIDE)
#define LIGHT_MVP_OFF   (VERT_BUF_SIZE + 256)  /* light-space MVP for shadow pass */
#define PROT_CPU_RW     0x03
#define PROT_GPU_RW     0x30          /* GPU_READ 0x10 | GPU_WRITE 0x20 (per OpenOrbis orbis/_types/kernel.h) */
#define MEM_TYPE_FLEX   0x03

// === VS: GPU-side MVP + dynamic lighting from sun buffer ===
// s[0:3]=V# verts. v0=vertex_id
// Buffer layout (from V# base):
//   0x00: MVP matrix (64 bytes)
//   0x40: Sun direction (16 bytes: sx, sy, sz, mode)
//   0x50: Vertex data (stride 48: pos+normal+uv)
// VS: MVP + pass {u, v, ny, nz} to PS for per-pixel sun lighting
// s[0:3]=V#. No sun in VS — PS does all lighting via desc table.
// Minimal fullscreen-triangle VS: generates 3 clip-space verts from
// vertex_id with NO vertex fetch, NO V#, NO memory load. Assembled with
// llvm-mc (hawaii). Used by RT_TEST to isolate whether the original VS's
// vertex-fetch / V# path is the hardware hang cause.
// Gradient diagnostic PS: outputs (posX/1920, posY/1080, 0, 1). A correct
// linear surface shows a smooth red->green gradient; tiling errors break it
// into blocks; pitch errors shear it. Reads pixel pos from v2,v3. llvm-mc.
static const uint32_t ps_grad_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x7E0A02FF, 0x3A005ADF,
    0x10000B02, 0x7E0A02FF, 0x3A2FF2E5, 0x10020B03,
    0x7E0C0280, 0x7E0E02F2, 0xF800080F, 0x07060100,
    0xBF810000, 0x5362724F, 0x00726468, 0x00003400,
    0x00000000, 0xDEADBEEF, 0xCAFE0E03, 0x00000000,
};

// GPU-resident copies of the RT_TEST diagnostic shaders (set in main()).
// build_dcb's RT_TEST path binds these instead of the raw .rodata arrays,
// which aren't GPU-accessible.
static void *g_vs_fulltri_gpu = 0;
static void *g_vs_bg_gpu = 0;
static void *g_vs_ftload_gpu = 0;
static void *g_ps_magenta_gpu = 0;
static void *g_ps_skyclean_gpu = 0;
static void *g_ps_grad_gpu = 0;

// Clean sky PS (llvm-mc): reads SCREEN Y from the SPI (POS_Y in v3) — no
// v_interp, no seam. t = 1 - screen_y/1080 (0x3A72B9D6 = 1/1080);
// sky = lerp(horizon, zenith, t). Reads zenith @ desc dword 24 (SMRD offset
// 0x18) and horizon @ dword 28 (0x1C) — SI/CIK SMRD offsets are in DWORDS,
// not bytes. desc ptr in s[0:1]. user_sgpr=2, 16 VGPRs.
static const uint32_t ps_skyclean_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0xC0820118, 0xC084011C,
    0xBF8C007F, 0x7E0A02FF, 0x3A72B9D6, 0x10040B03,
    0xD20A0002, 0x0001E502, 0x7E140208, 0x7E160209,
    0x7E18020A, 0x08081404, 0xD2820006, 0x042A0902,
    0x08081605, 0xD2820007, 0x042E0902, 0x08081806,
    0xD2820008, 0x04320902, 0x7E1202F2, 0xF800080F,
    0x09080706, 0xBF810000, 0x5362724F, 0x00726468,
    0x00006800, 0x00000000, 0xDEADBEEF, 0xCAFE00E3,
    0x00000000,
};

// Clean BG vertex shader (llvm-mc). BG verts are already in clip space, so no
// MVP transform: fetch the 4-float clip pos (base + 80 + vid*48) and pass it to
// POS0, plus export it as PARAM0 for the sky PS. Replaces the hand-encoded real
// VS for the BG draw to rule out a malformed-encoding hang.
static const uint32_t vs_bg_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x7E0202B0, 0x16020300,
    0x4A0202FF, 0x00000050, 0xE0381000, 0x80000401,
    0xBF8C0F70, 0xF80008CF, 0x07060504, 0xF800020F,
    0x07060504, 0xBF810000, 0x5362724F, 0x00726468,
    0x00003800, 0x00000000, 0x47505508, 0xAABBEE03,
    0x00000000,
};

// fulltri VS + one buffer_load from the V# (s[0:3]) — tests whether a VS
// buffer_load hangs on hardware. Position still vertex_id-derived (covers
// screen); loaded dword is *0 then added so it can't be dropped. llvm-mc.
static const uint32_t vs_ftload_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x34020081, 0x36020282,
    0x36040082, 0x7E020D01, 0x7E040D02, 0x7E0602F4,
    0x10020701, 0xD2080001, 0x0001E501, 0x10040702,
    0xD2080002, 0x0001E502, 0x7E140280, 0xE0301000,
    0x80000A0A, 0xBF8C0F70, 0xD210000A, 0x0001010A,
    0x06021501, 0x7E060280, 0x7E0802F2, 0xF80008CF,
    0x04030201, 0xBF810000, 0x5362724F, 0x00726468,
    0x00006800, 0x00000000, 0xDEADBEEF, 0xCAFE0E04,
    0x00000000,
};

static const uint32_t vs_fulltri_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x34020081, 0x36020282,
    0x36040082, 0x7E020D01, 0x7E040D02, 0x7E0602F4,
    0x10020701, 0xD2080001, 0x0001E501, 0x10040702,
    0xD2080002, 0x0001E502, 0x7E060280, 0x7E0802F2,
    0xF80008CF, 0x04030201, 0xBF810000, 0x5362724F,
    0x00726468, 0x00004C00, 0x00000000, 0xDEADBEEF,
    0xCAFE0E02, 0x00000000,
};

static const uint32_t vs_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000,
    0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085,
    0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601,
    0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C,
    0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13,
    0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E,
    0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80000CF,
    0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF8000A1F, 0x06040302, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0x47505508, 0xAABBEE02, 0x00000000,
};

// Dedicated shadow-pass VS. Identical bytes to vs_shader_binary but with a
// DIFFERENT hash (0xBEEFEE02CAFE5508 vs 0xAABBEE0247505508). Forces shadPS4 to
// compile and cache a distinct Vulkan pipeline for the shadow pass, so any
// pipeline-cache collision between main-pass VS and shadow-pass VS — which could
// produce double-rendering of the cube into shadow_depth — is eliminated.
static const uint32_t vs_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000,
    0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085,
    0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601,
    0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C,
    0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13,
    0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E,
    0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80000CF,
    0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF8000A1F, 0x05040302, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0xCAFE5508, 0xBEEFEE02, 0x00000000,
};

// Projective shadow PS.
// Interpolates world-space position (attr1.xyz) per-fragment, loads light_MVP from
// desc[48..63] (64 bytes at byte offset 192), transforms wpos through light_MVP to
// light-clip space, performs perspective divide, converts NDC to shadow UV,
// samples shadow_depth.r, and outputs (sample.r, 0, 0, 1) for visualization.
//
// VGPR usage: v0,v1 (barycentrics), v20,v21 (shadow UV), v24-v27 (wpos xyzw),
//             v28-v31 (light-clip xyzw), v32,v33,v34 (scratch + rcp_w), v40-v43 (RGBA out)
// SGPR usage: s0,s1 (desc ptr from user_data), s12-s15 (sampler), s16-s23 (shadow T#),
//             s32-s47 (light_MVP 4x4)
// Desc layout: desc[8..11] sampler, desc[40..47] shadow T#, desc[48..63] light_MVP
// PROJECTIVE SHADOW PS (now active): loads light_MVP from desc[48..63] via
// s_load_dwordx16, interpolates wpos.xyzw, multiplies wpos * light_MVP to get
// clip-space shadow coords, perspective-divides, computes shadow UV, samples
// shadow_depth, outputs sample.r as red intensity.
// Resource layout per shadPS4 recompiler:
//   s[0:1] = desc base, s[4:11] = shadow T# (desc[40..47]),
//   s[12:15] = sampler (desc[8..11]), s[32:47] = light_MVP (desc[48..63])
// === Cube PS (hash CAFE0114) — textured + 3D Lambert + light_color tint ===
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
//   v21      = lit factor = 0.3 + 0.7*max(0, dot)
//   v40-v43  = output RGBA = albedo * lit * light_color (alpha forced to 1)
static const uint32_t ps_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xC0C80100, 0xC0860108,
    0xC08C010C, 0xC08E0120, 0xBF8C007F, 0xC80C0000,
    0xC80D0001, 0xC8100100, 0xC8110101, 0xC8280200,
    0xC8290201, 0xC82C0300, 0xC82D0301, 0xC8300700,
    0xC8310701, 0xF0800F00, 0x00641003, 0xBF8C0F70,
    0x10281818, 0x102A1419, 0x06282B14, 0x102A161A,
    0x06282B14, 0x20282880, 0x102A28FF, 0x3F333333,
    0x062A2AFF, 0x3E99999A, 0x10502B10, 0x10522B11,
    0x10542B12, 0x1050501C, 0x1052521D, 0x1054541E,
    0x7E5602F2, 0xF800080F, 0x2B2A2928, 0xBF810000,
    /* OrbShdr footer: 40 dwords = 160 bytes = 0xA0 */
    0x5362724F, 0x00726468,
    0x0000A000, 0x00000000, 0xDEADBEEF,
    0xCAFE0114, 0x00000000,
};

// Kept for diagnostic swap-back if needed: wpos-UV shadow sample
static const uint32_t ps_shader_binary_WPOS_UV[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xC0C80128, 0xC0860108,
    0xBF8C007F, 0xC8600400, 0xC8610401, 0xC8640500,
    0xC8650501, 0x062830F0, 0x062A32F0, 0xF0800F00,
    0x00642814, 0xBF8C0F70, 0x7E5602F2, 0xF800080F,
    0x2B2A2928, 0xBF810000, 0xBF800000, 0x5362724F,
    0x00726468, 0x00004800, 0x00000000, 0xDEADBEEF,
    0xCAFE00FA, 0x00000000,
};


// === ps_sky: physical sky gradient + sun disc ===
// attr0.x = clip_x, attr0.y = clip_y (screen-space)
// Sky PS: dynamic day/sunset/night gradient + warm sun disc
// Loads:
//   s[4:7]   sun_NDC at desc[16] (xyz=NDC pos, w=radius²)
//   s[8:11]  zenith RGB at desc[24]
//   s[12:15] horizon RGB at desc[28]
//   s[16:19] light_color RGB at desc[32]
// Pipeline:
//   t = 0.5*(1 - clip_y)    // gradient param: 0=top, 1=bottom
//   sky.rgb = lerp(zenith, horizon, t)
//   d² = (clip_x - sun_x)² + (clip_y - sun_y)²
//   sun_factor = (max(0, 1 - d²*radius))² * 2 → sharper falloff
//   output.rgb = lerp(sky, light_color, sun_factor)
// Hash CAFE00E2 (camera-locked but with working sun disc).
static const uint32_t ps_dark_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0xC0820110, 0xC0840118,
    0xC086011C, 0xC0880120, 0xBF8C007F, 0xC8080000,
    0xC8090001, 0xC80C0100, 0xC80D0101, 0x7E140208,
    0x7E160209, 0x7E18020A, 0x7E1A020C, 0x7E1C020D,
    0x7E1E020E, 0x7E200210, 0x7E220211, 0x7E240212,
    0x7E260204, 0x7E280205, 0x7E2A0206, 0x082C06F2,
    0x102C2CF0, 0x082E150D, 0x102E2D17, 0x062E1517,
    0x0830170E, 0x10302D18, 0x06301718, 0x0832190F,
    0x10322D19, 0x06321919, 0x08342702, 0x08362903,
    0x1034351A, 0x1036371B, 0x0634371A, 0x7E385515,
    0x1034391A, 0x083434F2, 0x20343480, 0x1E3434F2,
    0x1034351A, 0x08362F10, 0x1036351B, 0x062E2F1B,
    0x08363111, 0x1036351B, 0x0630311B, 0x08363312,
    0x1036351B, 0x0632331B, 0x7E3402F2, 0xF800080F,
    0x1A191817, 0xBF810000, 0x5362724F, 0x00726468,
    0x0000E800, 0x00000000, 0xDEADBEEF, 0xCAFE00E2,
    0x00000000,
};


// Minimal PS exporting solid magenta (1,0,1,1) to MRT0. Assembled with llvm-mc.
// Export is 0xF800080F (VM=0): a plain color export. (An earlier hand-encoded
// version set VM=1, which made the hardware read a bogus pixel-valid mask and
// produced full-screen noise — misdiagnosed as a tiling issue.)
static const uint32_t ps_magenta_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000000, 0x7E0402F2, 0x7E060280,
    0x7E1402F2, 0x7E1602F2, 0xF800080F, 0x0B0A0302,
    0xBF810000, 0x5362724F, 0x00726468, 0x00002400,
    0x00000000, 0xDEADBEEF, 0xCAFE0E01, 0x00000000,
};

// Null PS for shadow depth pass — exports zero color (depth still written by rasterizer)
static const uint32_t ps_null_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000004, 0x7E040280, 0x7E060280,
    0x7E140280, 0x7E160280, 0xF800080F, 0x0B0A0302,
    0xBF810000, 0xBF800000, 0x5362724F, 0x00726468,
    0x00002800, 0x00000000, 0xDEADBEEF, 0xCAFE00E3,
    0x00000000,
};

/* Truly minimal null PS for proper depth-only shadow pass.
   No color export, no depth export. Starts with the canonical
   s_mov_b32 vcc_hi, imm prefix (0xBEEB03FF) so shadPS4's BinaryInfo parser
   (regs_shader.h:215) locates the footer via code[1] rather than linear scan.
   With code[1]=1, footer is at (1+1)*2 = 4 dwords in, which matches our
   signature position. Fresh hash CAFE00FA forces a pipeline cache miss. */
static const uint32_t ps_depthonly_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF,   /* s_mov_b32 vcc_hi, literal (shadPS4-required prefix) */
    0x00000001,   /* literal = 1 → BinaryInfo at dword offset 4 */
    0xBF810000,   /* s_endpgm */
    0xBF800000,   /* s_nop (padding to align BinaryInfo to dword 4) */
    0x5362724F,   /* 'O','r','b','S' - signature bytes 0..3 */
    0x00726468,   /* 'h','d','r',version=0 - signature bytes 4..7 */
    0x00001000,   /* pssl=0 cached=0 type=0(PS) source=0 length=16 bytes.
                     length is in BYTES per regs_shader.h:236:
                     code = std::span{code, bininfo.length / sizeof(u32)}.
                     16 bytes = 4 dwords of executable code (prefix[2] + endpgm + nop). */
    0x00000000,   /* chunk_usage_base=0, num_input_usage_slots=0, flags=0, pad=0 */
    0xDEADBEEF,   /* shader_hash lo */
    0xCAFE00FA,   /* shader_hash hi — unique to force fresh compile */
    0x00000000,   /* crc32 (unchecked) */
};

// Shadow-pass clear PS: outputs (1, 0, 0, 1) — solid R=1.0 ("nothing occluding").
// Drawn as a fullscreen quad BEFORE the cube in the shadow pass to fill the
// entire shadow map with a "no occluder" depth value.
//
// Encoding decision: inline F_ONE (0xF2) is fine. Both inline F_ONE and literal
// 1.0 (0x3F800000) produce identical results in shadPS4 — verified empirically.
// Original encoding restored. The 1.0→0.5 bug was elsewhere (CB_COLOR_INFO
// SNORM/UNORM mismatch — see shadow CB setup).
//
// Hash CAFE0108 to ensure shadPS4 picks up the fresh shader after CB fix.
static const uint32_t ps_shadow_clear_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009,                          /* prologue */
    0x7E5002F2,                                       /* v40 = 1.0 (R) */
    0x7E520280,                                       /* v41 = 0 (G) */
    0x7E540280,                                       /* v42 = 0 (B) */
    0x7E5602F2,                                       /* v43 = 1.0 (A) */
    0xF800080F, 0x2B2A2928,                           /* EXP MRT0 v[40:43] done vm */
    0xBF810000,                                       /* s_endpgm */
    /* OrbShdr footer: 9 dwords = 36 bytes = 0x24 */
    0x5362724F, 0x00726468,
    0x00002400, 0x00000000, 0xDEADBEEF,
    0xCAFE0108, 0x00000000,                           /* fresh hash */
};

// Shadow-pass PS: exports (NDC.z, 0, 0, 1) to MRT0.r.
// Loads light_MVP from desc[48..63] into s[32:47], interpolates attr1=world_pos,
// computes clip.z and clip.w as row·world_pos dot products, perspective-divides,
// saturates NDC.z to [0,1] using V_{MAX,MIN}_LEGACY_F32 (shadPS4-safe), and
// writes to MRT0. RGBA8 output quantizes R to 8-bit (sufficient precision).
// PGM_RSRC1: 44 VGPRs, 48 SGPRs → (5<<6)|10 = 0x14A.
// Hash CAFE00FB — fresh hash to force a pipeline-cache miss.
static const uint32_t ps_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009,                          /* prefix */
    0xC1100130,                                       /* s_load_dwordx16 s32, s[0:1], 0x30 — load light_MVP */
    0xBF8C007F,                                       /* s_waitcnt lgkmcnt(0) */
    /* Interp attr1 = world_pos.xyzw → v10,v11,v12,v13 */
    0xC8280400, 0xC8290401, 0xC82C0500, 0xC82D0501,
    0xC8300600, 0xC8310601, 0xC8340700, 0xC8350701,
    /* clip.z = s40*v10 + s41*v11 + s42*v12 + s43*v13 → v16 */
    0x10201428, 0x10281629, 0x06202910, 0x1028182A,
    0x06202910, 0x10281A2B, 0x06202910,
    /* clip.w = s44*v10 + s45*v11 + s46*v12 + s47*v13 → v17 */
    0x1022142C, 0x1028162D, 0x06222911, 0x1028182E,
    0x06222911, 0x10281A2F, 0x06222911,
    /* v18 = 1/v17; v16 = v16 * v18 = NDC.z */
    0x7E245511, 0x10202510,
    /* Saturate NDC.z to [0,1] using LEGACY opcodes */
    0x1C202080, 0x1A2020F2,
    /* Output: (NDC.z, 0, 0, 1) → v40,v41,v42,v43 */
    0x7E500310, 0x7E520280, 0x7E540280, 0x7E5602F2,
    /* EXP MRT0 done=1 vm=1; s_endpgm */
    0xF800080F, 0x2B2A2928, 0xBF810000,
    /* OrbShdr footer: 37 dwords = 148 bytes = 0x94 */
    0x5362724F, 0x00726468,
    0x00009400, 0x00000000, 0xDEADBEEF,
    0xCAFE00FB, 0x00000000,
};

// === Hash CAFE0119: Floor PS with normal-map sampling ===
//
// Adds tangent-space normal map sampling (desc[72..79]) on top of v0118's
// normal-vector Lambert. Per-fragment surface detail from the bump map
// while still respecting the curved surface's vertex normals.
//
// Pipeline:
//   1. Loads: sampler, albedo, normal_map (NEW), shadow_T#, light_MVP, sun, PCF, light_color
//   2. Interps: uv, vertex_normal, world_pos
//   3. Sample albedo → v[16:19]
//   4. Sample normal map → v[60:63]; decode: tn = sample*2 - 1
//   5. Build T = normalize(cross(world_up, vertex_N))
//   6. Build B = cross(vertex_N, T)
//   7. Perturbed N = normalize(T*tn.x + B*tn.y + vertex_N*tn.z)
//   8. lit = 0.3 + 0.7 * max(0, dot(perturbed_N, sun))
//   9. Light_MVP shadow projection (unchanged)
//  10. Output = albedo * lit * shadow_factor * light_color
//
// PGM_RSRC1 = 0x28D (88 SGPRs, 56 VGPRs).
static const uint32_t ps_floor_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xC0860108, 0xC0C80140,
    0xC0CC0148, 0xC0D40128, 0xC1180130, 0xC0A0010C,
    0xC0A20150, 0xC0AA0120, 0xBF8C007F, 0xC8280000,
    0xC8290001, 0xC82C0100, 0xC82D0101, 0xC8640200,
    0xC8650201, 0xC8680300, 0xC8690301, 0xC8300400,
    0xC8310401, 0xC8340500, 0xC8350501, 0xC8380600,
    0xC8390601, 0xC8600700, 0xC8610701, 0x7E1E02F2,
    0xF0800F00, 0x0064100A, 0xF0800F00, 0x00663C0A,
    0xBF8C0070, 0x107878F4, 0x067878F3, 0x107A7AF4,
    0x067A7AF3, 0x107C7CF4, 0x067C7CF3, 0x7E40031A,
    0x7E420280, 0x08443080, 0x10464120, 0x3E464522,
    0x7E465D23, 0x10404720, 0x10444722, 0x10484519,
    0x104A411A, 0x104C4518, 0x084A4D25, 0x104C4119,
    0x084C4C80, 0x10507920, 0x3E507B24, 0x3E507D18,
    0x10527B25, 0x3E527D19, 0x10547922, 0x3E547B26,
    0x3E547D1A, 0x10565128, 0x3E565329, 0x3E56552A,
    0x7E565D2B, 0x10505728, 0x10525729, 0x1054572A,
    0x10365040, 0x3E365241, 0x3E365442, 0x203C3680,
    0x103C3CFF, 0x3F333333, 0x063C3CFF, 0x3E99999A,
    0x10581830, 0x3E581A31, 0x3E581C32, 0x06585833,
    0x105A1834, 0x3E5A1A35, 0x3E5A1C36, 0x065A5A37,
    0x105C1838, 0x3E5C1A39, 0x3E5C1C3A, 0x065C5C3B,
    0x105E183C, 0x3E5E1A3D, 0x3E5E1C3E, 0x065E5E3F,
    0x7E60552F, 0x1058612C, 0x105A612D, 0x105C612E,
    0x104258F0, 0x064242F0, 0x10445AF0, 0x064444F0,
    0x1C405C80, 0x1A4040F2, 0xF0800100, 0x022A2321,
    0xBF8C0F70, 0x7C024123, 0x7E480280, 0x004648F2,
    0x7C025E80, 0x004646F2, 0x104A46F0, 0x064A4AF0,
    0x103C3D25, 0x1064211E, 0x1066231E, 0x1068251E,
    0x10646454, 0x10666655, 0x10686856, 0x7E6A02F2,
    0xF800080F, 0x35343332, 0xBF810000, 0x5362724F,
    0x00726468, 0x0001EC00, 0x00000000, 0xDEADBEEF,
    0xCAFE0119, 0x00000000,
};

// White PS for loading bar — solid (1, 1, 1, 1) output, no texture sampling
static const uint32_t ps_blue_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000006, 0xC8080000, 0xC8090001,
    0xC80C0100, 0xC80D0101, 0x7E2802F2, 0x7E2A02F2,
    0x7E2C02F2, 0x7E2E02F2, 0xF800080F, 0x17161514,
    0xBF810000, 0xBF800000, 0x5362724F, 0x00726468,
    0x00003800, 0x00000000, 0xDEADBEEF, 0xCAFE0003,
    0x00000000,
};

// === Helpers ===
static void my_memset(void *d, int v, unsigned long n) {
    unsigned char *p = (unsigned char *)d;
    for (unsigned long i = 0; i < n; i++) p[i] = (unsigned char)v;
}
static void my_memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
}
static void *gpu_alloc(unsigned long size, unsigned long align) {
    long phys = 0; void *addr = 0;
    size = (size + 0x3FFF) & ~0x3FFFUL;
    if (align < 0x4000) align = 0x4000;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, MEM_TYPE_FLEX, &phys)) return 0;
    if (sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW | PROT_GPU_RW, 0, phys, align)) return 0;
    my_memset(addr, 0, size);
    return addr;
}

static float my_sin(float x) {
    const float PI = 3.14159265358979f, TWO_PI = 6.28318530717959f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    float x2 = x * x;
    return x*(1.0f-x2/6.0f*(1.0f-x2/20.0f*(1.0f-x2/42.0f*(1.0f-x2/72.0f))));
}
static float my_cos(float x) { return my_sin(x + 1.57079632679490f); }

static float my_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    /* Newton-Raphson iteration for sqrt */
    float g = x * 0.5f;
    for (int i = 0; i < 6; i++) g = 0.5f * (g + x / g);
    return g;
}

// === Geometry (static) ===
static const float cube_pos[8][3] = {
    {-.4f,-.4f,.4f},{.4f,-.4f,.4f},{.4f,.4f,.4f},{-.4f,.4f,.4f},
    {-.4f,-.4f,-.4f},{.4f,-.4f,-.4f},{.4f,.4f,-.4f},{-.4f,.4f,-.4f},
};
static const int face_idx[12][3] = {
    {0,1,2},{0,2,3},{5,4,7},{5,7,6},{4,0,3},{4,3,7},
    {1,5,6},{1,6,2},{3,2,6},{3,6,7},{4,5,1},{4,1,0},
};
static const float uv_corners[4][2] = {{0,1},{1,1},{1,0},{0,0}};
static const int tri_uv[2][3] = {{0,1,2},{0,2,3}};
static const float face_shade[6] = {1.0f,0.5f,0.7f,0.8f,0.95f,0.55f};

// Build vertex buffer per frame (CPU-side transform)
// disp_tex/disp_w/disp_h: optional displacement texture (RGBA8, R = height [0,1]).
//           NULL → no displacement (curve-only floor).
//           Caller is responsible for stretching the texture's R range to
//           [0, 255] before calling — keeps this function simple.
static void build_static_vb(float *vb,
                            const unsigned char *disp_tex, int disp_w, int disp_h) {
    // Identity matrix at offset 0
    float *id = (float*)((char*)vb + IDENT_OFF);
    my_memset(id, 0, 64);
    id[0]=1; id[5]=1; id[10]=1; id[15]=1;

    // BG sun slot at offset 64 (VS reads this as "sun" for BG draw — ignored, zero normals)
    float *bgsun = (float*)((char*)vb + BG_SUN_OFF);
    bgsun[0]=0.20f; bgsun[1]=0.50f; bgsun[2]=0.67f; bgsun[3]=0.0f;

    // BG verts at offset 80 (after identity + bg_sun)
    float *bg = (float*)((char*)vb + BG_DATA_OFF);
    float bp[6][4]={{-1,-1,.999f,1},{1,-1,.999f,1},{1,1,.999f,1},
                     {-1,-1,.999f,1},{1,1,.999f,1},{-1,1,.999f,1}};
    for (int v=0;v<6;v++) {
        int i=v*12;
        bg[i]=bp[v][0];bg[i+1]=bp[v][1];bg[i+2]=bp[v][2];bg[i+3]=bp[v][3];
        bg[i+4]=0.0f;bg[i+5]=0.0f;bg[i+6]=0.0f;bg[i+7]=0.0f; /* zero normal = no lighting */
        bg[i+8]=bp[v][0]; bg[i+9]=bp[v][1]; bg[i+10]=0.0f; bg[i+11]=0.0f; /* clip_x, clip_y */
    }

    // Cube verts at offset 320 (after MVP slot)
    float *cb = (float*)((char*)vb + CUBE_DATA_OFF);
    static const float face_normals[6][3] = {
        {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
    };
    for (int tri=0;tri<12;tri++) {
        int face=tri/2,t=tri%2;
        for (int v=0;v<3;v++) {
            int vi=face_idx[tri][v],ui=tri_uv[t][v];
            int idx=(tri*3+v)*12;
            cb[idx]=cube_pos[vi][0]; cb[idx+1]=cube_pos[vi][1];
            cb[idx+2]=cube_pos[vi][2]; cb[idx+3]=1.0f;
            cb[idx+4]=face_normals[face][0]; cb[idx+5]=face_normals[face][1];
            cb[idx+6]=face_normals[face][2]; cb[idx+7]=0.0f;
            cb[idx+8]=uv_corners[ui][0]; cb[idx+9]=uv_corners[ui][1];
            cb[idx+10]=0.0f; cb[idx+11]=0.0f;
        }
    }

    /* Floor plane: 32×32 grid of quads, ±200 world units in XZ.
       Each vertex gets:
         y = Y_BASE - (x²+z²)/(2R)         curve toward horizon
           + (disp.r - 0.5) * DISP_SCALE   per-vertex displacement
       Normals computed via finite differences of the displaced height field
       so lighting matches the actual surface (not just the curve).
       UV tiled 100× across full floor. */
    {
        float *fp = (float*)((char*)vb + FLOOR_DATA_OFF);
        const int GRID = 64;
        const float HALF = 200.0f;
        const float UV_MAX = 100.0f;
        const float Y_BASE = -0.5f;
        const float R = 20000.0f;
        const float DISP_SCALE = 1.5f;       /* ±0.75 world units of relief — clearly visible */
        const float DISP_TILES = 32.0f;      /* 32 tiles across floor = 12.5 units per tile;
                                                with 64×64 mesh = 2 verts per tile width */
        const float STEP = (2.0f * HALF) / (float)GRID;
        const float UV_STEP = UV_MAX / (float)GRID;

        /* Inline helper: y(x, z) = curve + (sample - 0.5) * DISP_SCALE.
           Displacement texture TILES UV_MAX times across the floor (same
           tile rate as albedo) so each tile in the BMP becomes a real-world
           tile on the floor. With UV_MAX=100 and 400-unit floor, each BMP
           tile covers 4 world units. Texture R values are auto-stretched
           to [0, 255] on load. */
        #define COMPUTE_Y(out_y, xx, zz) do {                                  \
            float _disp = 0.0f;                                                \
            if (disp_tex && disp_w > 0 && disp_h > 0) {                        \
                float _u = ((xx) + HALF) / (2.0f * HALF) * DISP_TILES;         \
                float _v = ((zz) + HALF) / (2.0f * HALF) * DISP_TILES;         \
                /* Wrap to [0, 1) — tile the texture */                        \
                float _uf = _u - (float)((int)_u);                             \
                float _vf = _v - (float)((int)_v);                             \
                if (_uf < 0) _uf += 1.0f;                                      \
                if (_vf < 0) _vf += 1.0f;                                      \
                int _tx = (int)(_uf * (float)disp_w);                          \
                int _tz = (int)(_vf * (float)disp_h);                          \
                if (_tx < 0) _tx = 0; if (_tx >= disp_w) _tx = disp_w - 1;     \
                if (_tz < 0) _tz = 0; if (_tz >= disp_h) _tz = disp_h - 1;    \
                int _idx = (_tz * disp_w + _tx) * 4;                           \
                _disp = ((float)disp_tex[_idx] / 255.0f - 0.5f) * DISP_SCALE;  \
            }                                                                  \
            (out_y) = Y_BASE - ((xx)*(xx) + (zz)*(zz)) / (2.0f * R) + _disp;   \
        } while (0)

        int v = 0;
        for (int gz = 0; gz < GRID; gz++) {
            for (int gx = 0; gx < GRID; gx++) {
                float x0 = -HALF + (float)gx * STEP;
                float x1 = x0 + STEP;
                float z0 = -HALF + (float)gz * STEP;
                float z1 = z0 + STEP;
                float u0 = (float)gx * UV_STEP;
                float u1 = u0 + UV_STEP;
                float vt0 = (float)gz * UV_STEP;
                float vt1 = vt0 + UV_STEP;

                /* Compute Y at each corner */
                float yA, yB, yC, yD;
                COMPUTE_Y(yA, x0, z0);
                COMPUTE_Y(yB, x1, z0);
                COMPUTE_Y(yC, x1, z1);
                COMPUTE_Y(yD, x0, z1);

                /* Per-vertex normals via finite differences. Sample y at small
                   offsets to estimate ∂y/∂x and ∂y/∂z. Normal = (-∂y/∂x, 1, -∂y/∂z)
                   normalized. Epsilon = STEP/4 for smooth gradient. */
                float eps = STEP * 0.25f;
                #define COMPUTE_N(NX, NY, NZ, xx, zz) do {                     \
                    float _yp_dx, _yn_dx, _yp_dz, _yn_dz;                      \
                    COMPUTE_Y(_yp_dx, (xx) + eps, (zz));                       \
                    COMPUTE_Y(_yn_dx, (xx) - eps, (zz));                       \
                    COMPUTE_Y(_yp_dz, (xx),       (zz) + eps);                 \
                    COMPUTE_Y(_yn_dz, (xx),       (zz) - eps);                 \
                    float _dydx = (_yp_dx - _yn_dx) / (2.0f * eps);            \
                    float _dydz = (_yp_dz - _yn_dz) / (2.0f * eps);            \
                    float _nx = -_dydx;                                        \
                    float _ny = 1.0f;                                          \
                    float _nz = -_dydz;                                        \
                    float _len = my_sqrt(_nx*_nx + _ny*_ny + _nz*_nz);         \
                    if (_len < 0.001f) _len = 1.0f;                            \
                    (NX) = _nx / _len;                                         \
                    (NY) = _ny / _len;                                         \
                    (NZ) = _nz / _len;                                         \
                } while (0)

                float NA[3], NB[3], NC[3], ND[3];
                COMPUTE_N(NA[0], NA[1], NA[2], x0, z0);
                COMPUTE_N(NB[0], NB[1], NB[2], x1, z0);
                COMPUTE_N(NC[0], NC[1], NC[2], x1, z1);
                COMPUTE_N(ND[0], ND[1], ND[2], x0, z1);

                float verts[6][8] = {
                    /* tri 1: A, C, B — pos.xyz, uv.xy, normal.xyz */
                    {x0, yA, z0,  u0, vt0,   NA[0], NA[1], NA[2]},
                    {x1, yC, z1,  u1, vt1,   NC[0], NC[1], NC[2]},
                    {x1, yB, z0,  u1, vt0,   NB[0], NB[1], NB[2]},
                    /* tri 2: A, D, C */
                    {x0, yA, z0,  u0, vt0,   NA[0], NA[1], NA[2]},
                    {x0, yD, z1,  u0, vt1,   ND[0], ND[1], ND[2]},
                    {x1, yC, z1,  u1, vt1,   NC[0], NC[1], NC[2]},
                };
                for (int i = 0; i < 6; i++) {
                    int idx = v * 12;
                    fp[idx+0] = verts[i][0];
                    fp[idx+1] = verts[i][1];
                    fp[idx+2] = verts[i][2];
                    fp[idx+3] = 1.0f;
                    fp[idx+4] = verts[i][5];
                    fp[idx+5] = verts[i][6];
                    fp[idx+6] = verts[i][7];
                    fp[idx+7] = 0.0f;
                    fp[idx+8] = verts[i][3];
                    fp[idx+9] = verts[i][4];
                    fp[idx+10] = 0.0f; fp[idx+11] = 0.0f;
                    v++;
                }

                #undef COMPUTE_N
            }
        }

        #undef COMPUTE_Y
    }
}

/* FPS free-fly camera: position + yaw/pitch → view-projection matrix */
static void build_mvp(float *mvp, float yaw, float pitch,
                      float cx, float cy, float cz) {
    float sy=my_sin(yaw),  cy2=my_cos(yaw);
    float sp=my_sin(pitch),cp=my_cos(pitch);

    /* Camera basis vectors */
    float fx= sy*cp, fy=-sp, fz=-cy2*cp;  /* forward (into screen) */
    float rx= cy2,   ry= 0,  rz= sy;      /* right */
    float ux= sy*sp, uy= cp, uz=-cy2*sp;  /* up */

    /* View matrix: rotate then translate */
    float v[4][4]={
        { rx, ry, rz, -(rx*cx + ry*cy + rz*cz) },
        { ux, uy, uz, -(ux*cx + uy*cy + uz*cz) },
        {-fx,-fy,-fz,  (fx*cx + fy*cy + fz*cz) },
        {  0,  0,  0,  1 }
    };

    /* Perspective projection: 35° FOV telephoto.
       Produces NDC.z in [0, 1] (DirectX/Vulkan native / ZeroToW convention) so we
       don't rely on VK_EXT_depth_clip_control. Must be paired with
       CTX_CLIPPER_CONTROL.clip_space = 1 (ZeroToW, bit 19).
       View convention: -Z is forward. For view.z = -near, NDC.z = 0. For view.z = -far, NDC.z = 1.
       Derived from:  clip.z = P22*vz + P23,  clip.w = -vz,  NDC.z = clip.z/clip.w.
         P22 = -fa / (fa - n),  P23 = -n*fa / (fa - n),  P[3][2] = -1. */
    float aspect=(float)DISPLAY_W/(float)DISPLAY_H;
    float fov=my_cos(0.3054f)/my_sin(0.3054f);
    float n=0.01f, fa=500.0f;
    float p[4][4]={
        {fov/aspect, 0, 0, 0},
        {0, fov, 0, 0},
        {0, 0, -fa/(fa-n), -n*fa/(fa-n)},
        {0, 0, -1, 0}
    };

    /* MVP = P × V */
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++) {
            float s=0;
            for (int k=0;k<4;k++) s+=p[i][k]*v[k][j];
            mvp[i*4+j]=s;
        }
}

// === Descriptor builders ===
static void build_vsharp(uint32_t *v, void *base, uint32_t size) {
    uint64_t a = (uint64_t)(uintptr_t)base;
    v[0]=(uint32_t)a; v[1]=(uint32_t)(a>>32)&0xFFFF; v[2]=size;
    v[3]=(1u<<3)|(2u<<6)|(3u<<9)|(4u<<12)|(4u<<15);
}
static void build_tsharp(uint32_t *t, void *tex, int w, int h) {
    uint64_t a=(uint64_t)(uintptr_t)tex;
    my_memset(t,0,32);
    t[0]=(uint32_t)(a>>8); t[1]=(uint32_t)(a>>40)|(10u<<20);
    t[2]=(uint32_t)(w-1)|((uint32_t)(h-1)<<14);
    t[3]=4u|(5u<<3)|(6u<<6)|(7u<<9)|(8u<<20)|(9u<<28);
    t[4]=(uint32_t)(w-1)<<13;
}

/* Depth T#: R32_FLOAT, Depth2DThin64 tile mode. For sampling shadow_depth
   from the floor PS as a raw depth value in .r channel.
   - data_format = 4 (Format32) in bits[25:20] of word1
   - num_format  = 7 (Float)    in bits[29:26] of word1
   - tiling_idx  = 0 (Depth2DThin64) in bits[24:20] of word3
   - type        = 9 (Color2D)  in bits[31:28] of word3
   shadPS4's ImageInfo ctor with ShaderResource.is_depth=true would promote
   this to D32Sfloat, but since we'll use a regular image_sample (not _c),
   is_depth stays false and we read the raw 32-bit float depth value directly. */
static void build_tsharp_depth(uint32_t *t, void *tex, int w, int h) {
    uint64_t a=(uint64_t)(uintptr_t)tex;
    my_memset(t,0,32);
    t[0]=(uint32_t)(a>>8);
    t[1]=(uint32_t)(a>>40) | (4u<<20) | (7u<<26);  /* Format32 | Float */
    t[2]=(uint32_t)(w-1)|((uint32_t)(h-1)<<14);
    t[3]=4u|(5u<<3)|(6u<<6)|(7u<<9)|(0u<<20)|(9u<<28); /* RGBA swizzle, Depth2DThin64, Color2D */
    t[4]=(uint32_t)(w-1)<<13;
}

static void build_ssharp(uint32_t *s) {
    my_memset(s,0,16);
    // dword0: clamp_x/y/z=Wrap(0), defaults
    // dword1: min_lod=0, max_lod=0xF00(15.0 in 4.8 fixed) at bits[23:12]
    s[1] = (0xF00u << 12);
    // dword2: xy_mag_filter=Bilinear(1) at bit20, xy_min_filter=Bilinear(1) at bit22
    s[2] = (1u << 20) | (1u << 22);
}

/* Anisotropic-filtering sampler (16:1) for the floor's albedo and normal
   map. AF eliminates the blurred / smeared look at glancing angles when
   the floor stretches into the distance.

   Per AMD GCN3 ISA Vol II Sec 8.2 sampler S# layout:
     dword0 [11:9]  MAX_ANISO_RATIO (0=1:1, 1=2:1, 2=4:1, 3=8:1, 4=16:1)
     dword2 [21:20] XY_MAG_FILTER (1=Linear, 3=Aniso_Linear)
     dword2 [23:22] XY_MIN_FILTER (same enum)

   Setting MAX_ANISO_RATIO=4 + filter mode = Aniso_Linear gives the
   driver freedom to use up to 16 samples per anisotropic kernel along
   the dominant axis for best quality. */
static void build_ssharp_aniso(uint32_t *s) {
    my_memset(s, 0, 16);
    /* dword0: clamp_x/y/z=Wrap(0); MAX_ANISO_RATIO=4 (16:1) at bits[11:9] */
    s[0] = (4u << 9);
    /* dword1: max_lod=0xF00 (15.0) at bits[23:12] */
    s[1] = (0xF00u << 12);
    /* dword2: xy_mag_filter=Aniso_Linear(3) at [21:20],
               xy_min_filter=Aniso_Linear(3) at [23:22] */
    s[2] = (3u << 20) | (3u << 22);
}

/* Shadow-map sampler at desc[80..83] for the floor PS (CAFE100F).

   The floor PS uses plain IMAGE_SAMPLE (not _C_LZ) so depth_compare_func
   is unused — kept at LessEqual for documentation. The compare is done
   manually in the shader via V_CMP_LT_F32 + V_CNDMASK_B32.

   Semantics that ARE used:
     - clamp_x/y/z = 6 (ClampBorder): out-of-frustum UVs hit the border
     - border_color_type = 2 (White) at raw1 bits [62..63]: samples at out-of-
       frustum UVs return 1.0 in R. Then the manual compare (stored < z_ref)
       with z_ref ∈ [0,1] is FALSE → fragment is lit. Out-of-frustum floor
       areas correctly stay lit.
     - xy_mag/min_filter = 1 (Bilinear): softens the sampled edge a bit
     - max_lod = 15.0 (0xF00 in u4.8): allow any mip level (shadow map
       has only level 0 anyway).

   Hardware bit layout per AMD Sea Islands ISA Table 8.12, verified against
   shadPS4/src/video_core/amdgpu/resource.h Sampler struct. */
static void build_ssharp_pcf(uint32_t *s) {
    my_memset(s,0,16);
    /* raw0 low (dword0): clamp_x/y/z = ClampBorder(6) at bits [0..8],
       depth_compare_func = LessEqual(3) at bits [12..14] (unused but harmless) */
    s[0] = 6u | (6u << 3) | (6u << 6) | (3u << 12);
    /* raw0 high (dword1): max_lod=0xF00 (15.0) at bits [12..23] */
    s[1] = (0xF00u << 12);
    /* raw1 low (dword2): xy_mag=Bilinear(1) at [20..21], xy_min=Bilinear(1) at [22..23] */
    s[2] = (1u << 20) | (1u << 22);
    /* raw1 high (dword3): border_color_type = White(2) at bits [30..31] */
    s[3] = (2u << 30);
}

// === DCB builder ===
static uint32_t build_dcb(struct PM4Builder *b,
    const void *vs, const void *ps, const void *ps_bg, const void *ps_null,
    const void *ps_floor,
    const uint32_t *vb_v, const uint32_t *bg_v, const uint32_t *shadow_vb_v,
    const uint32_t *floor_v,
    void *vb_base, uint32_t *desc, int model_verts, unsigned long vb_total,
    uint32_t *ib_ptr, int num_indices, int is_indexed,
    void *color, void *depth, void *shadow_depth,
    volatile uint32_t *fence, uint32_t fv) {

    /* Default hardware-state init (sceGnmDrawInitDefaultHardwareState equivalent).
       Required on real PS4 — without it context registers are undefined and the
       GPU hangs. Must come first, before context_control and any draw state. */
    pm4_init_default_hw_state(b);

#ifdef MINIMAL_TEST
    /* DIAGNOSTIC (opt-in: -DMINIMAL_TEST): skip ALL draws. GPU-DMA-fill the
       framebuffer with solid magenta, then signal the EOP fence. Tests
       command-processor + submit + fence + flip in isolation from the draw
       pipeline. Magenta = GPU path healthy; black = fault in init/submit. */
    {
        unsigned long fb_bytes = (unsigned long)DISPLAY_W * DISPLAY_H * 4;
        const unsigned long CHUNK = 0x100000;   /* DMA_DATA byte count is 21-bit */
        unsigned long off = 0;
        while (off < fb_bytes) {
            unsigned long n = fb_bytes - off;
            if (n > CHUNK) n = CHUNK;
            pm4_dma_fill(b, (char*)color + off, (uint32_t)n, 0xFFFF00FFu /* magenta */);
            off += n;
        }
        pm4_event_write_eop(b, fence, fv);
        return b->off * 4;
    }
#endif

    pm4_context_control(b);

    /* Shadow pass moved to end of DCB — see after model draw below.
       Rationale: testing if order of execution is the issue (user hint). */

    // VS: GPU-MVP, user_sgpr=4 (s[0:3]=V#). No sun in VS — PS handles lighting.
    // PGM_RSRC1 = 0x4B: vgpr_field=11 (46 VGPRs — shader uses up to v45),
    // sgpr_field=1 (16 SGPRs). RSRC1=4 (20 VGPRs) faults on real hardware
    // because the shader accesses v20-v45 beyond the allocation.
#ifdef RT_TEST
    /* RT_TEST: minimal fullscreen-triangle VS — no vertex fetch, no V#.
       Isolates whether the original VS's vertex-fetch path faults hardware.
       RSRC1 vgpr_field=1 (5 VGPRs, uses v0-v4); RSRC2 user_sgpr=0. */
    { uint64_t a=(uint64_t)(uintptr_t)g_vs_fulltri_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),1u,0u};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }
#elif defined(VS_LOAD_TEST)
    /* VS_LOAD_TEST: fulltri VS + one buffer_load from the V#. vgpr_field=2
       (12 VGPRs), user_sgpr=4 (V# in s[0:3]). Tests if a VS buffer_load hangs. */
    { uint64_t a=(uint64_t)(uintptr_t)g_vs_ftload_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),2u,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }
#elif defined(BG_CLEAN_VS) || defined(BG_CLEAN_SKY) || defined(BG_SKY_CLEAN)
    /* Clean llvm-mc BG VS: passthrough clip pos + param0. vgpr_field=1
       (8 VGPRs), user_sgpr=4 (V# in s[0:3]). */
    { uint64_t a=(uint64_t)(uintptr_t)g_vs_bg_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),1u,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }
#else
    { uint64_t a=(uint64_t)(uintptr_t)vs;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x4Bu,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4);
      /* VS user data set per-draw below */ }
#endif

    // PS: user_sgpr=2. New projective-shadow PS uses up to v43 (need 44 VGPRs
    // → field = ceil(44/4)-1 = 10) and up to s47 (need 48 SGPRs → field = ceil(48/8)-1 = 5).
    // PGM_RSRC1 = (sgpr_field<<6) | vgpr_field = (5<<6)|10 = 0x14A.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(5u<<6)|10u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

    // Scissors
    { uint32_t s[2]={0,(DISPLAY_W&0x7FFF)|((DISPLAY_H&0x7FFF)<<16)};
      pm4_set_context_regs(b,CTX_SCREEN_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_GENERIC_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_VIEWPORT_SCISSOR0,s,2);
      s[0]=(1u<<31);
      pm4_set_context_regs(b,CTX_WINDOW_SCISSOR,s,2); }

    // Viewport — zscale=1, zoffset=0 for ZeroToOne clip space
    // (shadPS4 computes minDepth = zoffset - zscale, maxDepth = zoffset + zscale
    //  for MinusWToW; for ZeroToOne: minDepth = zoffset, maxDepth = zoffset + zscale).
    // With zscale=1, zoffset=0: minDepth=0, maxDepth=1 — full range, NDC.z→depth direct.
    pm4_emit(b,pm4_type3(PM4_SET_CONTEXT_REG,7));
    pm4_emit(b,CTX_VIEWPORT0);
    pm4_emit_f(b,(float)DISPLAY_W*0.5f); pm4_emit_f(b,(float)DISPLAY_W*0.5f);
    pm4_emit_f(b,(float)DISPLAY_H*-0.5f); pm4_emit_f(b,(float)DISPLAY_H*0.5f);
    pm4_emit_f(b,1.0f); pm4_emit_f(b,0.0f);

    pm4_set_context_reg(b,CTX_INDEX_OFFSET,0);

    // Depth — request GPU clear of depth buffer to 1.0f via DB_RENDER_CONTROL.depth_clear_enable.
    // shadPS4 translates this to Vulkan loadOp=Clear on the depth attachment.
#ifdef SIMPLE_DRAW
    /* SIMPLE_DRAW (RT_TEST / VS_LOAD_TEST): depth fully disabled. */
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);
    pm4_set_context_reg(b,CTX_DB_Z_INFO,0);            /* Z_INVALID — no depth surface */
    pm4_set_context_reg(b,CTX_DB_STENCIL_INFO,0);
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,0);        /* depth test/write OFF */
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0);      /* no culling */
#else
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,1u); // bit0 = depth_clear_enable
    pm4_set_context_reg(b,CTX_DEPTH_VIEW,0);
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_OVERRIDE,0);
    pm4_set_context_reg(b,0x00B,0x3F800000u); // CTX_DEPTH_CLEAR = 1.0f
    pm4_set_context_reg(b,CTX_DB_Z_INFO,3u);
    pm4_set_context_reg(b,CTX_DB_STENCIL_INFO,0);
    { uint32_t z=(uint32_t)((uint64_t)(uintptr_t)depth>>8);
      uint32_t d[4]={z,0,z,0};
      pm4_set_context_regs(b,CTX_DB_Z_READ_BASE,d,4); }
    pm4_set_context_reg(b,CTX_DB_DEPTH_SIZE,((DISPLAY_W/8)-1)|(((DISPLAY_H/8)-1)<<11));
    pm4_set_context_reg(b,CTX_DB_DEPTH_SLICE,(DISPLAY_W*DISPLAY_H/64)-1);
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(7u<<4)); // Always test, NO WRITE

    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0); /* no culling for BG */
#endif

    // Color — render directly to display FB (BGRA, sRGB).
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)color>>8);
      uint32_t r[14]={c,(DISPLAY_W/8)-1,(DISPLAY_W*DISPLAY_H/64)-1,0,
        0x09A8u,0,0,0,0,0,0,0,0,0};
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,DISPLAY_W|(DISPLAY_H<<16)); }

    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);
#ifdef RT_TEST
    /* RT_TEST: fulltri VS exports position only; gradient PS reads pos at v2,v3. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);         /* 1 (min-one) param export slot */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x302);         /* PERSP_CENTER(v0,v1) + POS_X(v2) + POS_Y(v3) */
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x302);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);               /* no interpolants */
#elif defined(VS_LOAD_TEST)
    /* VS_LOAD_TEST: ftload VS exports position only; magenta PS reads nothing. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);
#elif defined(BG_CLEAN_VS)
    /* BG_CLEAN_VS: VS exports pos + param0; magenta PS reads nothing (param ignored). */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);         /* 1 param export */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);
#elif defined(BG_CLEAN_SKY)
    /* BG_CLEAN_VS + hand-encoded sky PS: interpolates attr0.x/.y. 1 interpolant,
       PERSP_CENTER barycentrics in v0,v1. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);          /* VS param0 -> PS slot0 */
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);         /* 1 param export */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);          /* PERSP_CENTER -> v0,v1 */
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,1);               /* attr0 */
#elif defined(BG_SKY_CLEAN)
    /* clean sky PS reads SCREEN Y from SPI (POS_Y in v3) — no v_interp, no seam.
       PERSP_CENTER(v0,v1) + POS_X(v2) + POS_Y(v3). No interpolants. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x302);         /* POS_X(v2) + POS_Y(v3) */
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x302);
    pm4_set_context_reg(b,CTX_NUM_INTERP,0);
#else
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);           /* attr0: VS param 0 -> PS slot 0 */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0+1,1);         /* attr1: VS param 1 -> PS slot 1 */
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,1);          /* 2 param exports (export_count_min_one=1) */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,2);                /* 2 attrs: {u,v,ny,nz} and {wpos.xyzw} */
#endif
    pm4_set_context_reg(b,CTX_SHADER_POS_FORMAT,4);
    pm4_set_context_reg(b,CTX_Z_EXPORT_FORMAT,0);
    pm4_set_context_reg(b,CTX_COLOR_EXPORT_FORMAT,9);
    pm4_set_context_reg(b,CTX_COLOR_CONTROL,0x00CC0010u);
    pm4_set_context_reg(b,0x203,0);
    /* ClipperControl.clip_space = 1 (ZeroToOne / DX-convention, bit 19).
       Paired with our NDC.z∈[0,1] projection matrix. Avoids dependence on
       VK_EXT_depth_clip_control which may not be honored → produced the
       "finite render distance in front of camera" artifact. */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    pm4_set_context_reg(b,CTX_MODE_CONTROL,0);
    pm4_set_context_reg(b,CTX_STAGE_ENABLE,0);
    pm4_set_context_reg(b,CTX_AA_CONFIG,0);
    pm4_set_context_reg(b,CTX_BLEND_CONTROL0,0);
    pm4_set_context_reg(b,CTX_INDEX_SIZE,0);
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);

    // Draw 1: BG quad with sky PS (sun disc)
#ifdef RT_TEST
    /* RT_TEST: use the trivial magenta PS instead of the sky shader. Isolates
       the render-target + VS + draw pipeline from the real shaders. */
    { uint64_t a=(uint64_t)(uintptr_t)g_ps_grad_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(0u<<6)|2u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4); }
#else
#if defined(BG_PS_MAGENTA) || defined(VS_LOAD_TEST) || defined(BG_CLEAN_VS)
    /* Diagnostic: real VS, but trivial magenta PS (no descriptors). Isolates
       the VS vertex-fetch/MVP path from the sky PS descriptor table. */
    { uint64_t a=(uint64_t)(uintptr_t)g_ps_magenta_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(0u<<6)|2u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4); }
#elif defined(BG_SKY_CLEAN)
    /* Clean llvm-mc sky PS: lerp(zenith,horizon,clip_y) from the descriptor.
       RSRC1 = (sgpr_field 3)<<6 | (vgpr_field 3) = 16 VGPRs + 32 SGPRs.
       The shader uses up to v12 (horizon.b), so vgpr_field must be 3, NOT 2 —
       12 VGPRs left v12 unallocated -> read as 0 -> inverted blue channel.
       user_sgpr=2 (desc ptr in s[0:1]). */
    { uint64_t a=(uint64_t)(uintptr_t)g_ps_skyclean_gpu;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(3u<<6)|3u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
#else
    { uint64_t a=(uint64_t)(uintptr_t)ps_bg;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(1u<<6)|2u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      /* Sky PS needs desc ptr for sun position */
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
#endif
#endif
#ifdef RT_TEST
    /* fulltri VS needs no vertex data — draw 3 auto verts (one fullscreen tri). */
    pm4_draw_index_auto(b,3);
#elif defined(VS_LOAD_TEST)
    /* ftload VS: bind V# in s[0:3], draw 3 verts (fullscreen tri from vertex_id). */
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b,3);
#else
    // VS s[0:3] = vertex/MVP V#. Sun read via s_buffer_load from V#+0x40
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b,BG_VERTS);
#endif

#if defined(DRAW_STOP) && DRAW_STOP <= 1
    /* Bisect: stop after BG/sky draw. If the sky gradient renders, the
       render-target setup + VS/PS pipeline work; problem is in a later draw. */
    pm4_event_write_eop(b,fence,fv);
    return b->off*4;
#endif

    /* Sky draw performed the one-shot depth clear. Disable clear flag so subsequent
       draws (floor, cube) render normally against the now-cleared depth buffer. */
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);

    // === Floor draw: between sky and cube ===
    // Depth test Less so cube draws on top, but floor is drawn first so cube occludes it.
    // Floor uses its own V# (floor_v) pointing at vb+FLOOR_MVP_OFF where MVP is mirrored
    // and floor verts are at V#+80.
    if (ps_floor && floor_v) {
        /* Switch PS to floor PS CAFE0119. PGM_RSRC1 = 0x28D (88 SGPRs, 56 VGPRs). */
        uint64_t a=(uint64_t)(uintptr_t)ps_floor;
        uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x28Du,(2u<<1)};
        pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
        uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                        (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
        pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2);

        /* Depth for floor: less-than, write enabled (so cube z-tests correctly against floor) */
        pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
        pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back */

        pm4_set_sh_regs(b,SH_VS_USER_DATA_0,floor_v,4);
        pm4_draw_index_auto(b, FLOOR_VERTS);
    }

#if defined(DRAW_STOP) && DRAW_STOP <= 2
    /* Bisect: stop after floor draw (BG + floor, no cube). */
    pm4_event_write_eop(b,fence,fv);
    return b->off*4;
#endif

    // Switch back to textured PS for model. Same RSRC1 as initial: 44 VGPRs, 48 SGPRs.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(5u<<6)|10u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

    // Switch to depth=Less for cube
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back, CW front */

    // Draw 2: Model with real MVP. Sun at V#+0x40 read via s_buffer_load
    { uint32_t cube_v[4];
      build_vsharp(cube_v,(char*)vb_base+MVP_OFF,(uint32_t)(vb_total > MVP_OFF ? vb_total - MVP_OFF : VERT_BUF_SIZE - MVP_OFF));
      pm4_set_sh_regs(b,SH_VS_USER_DATA_0,cube_v,4); }
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1); /* uint32 indices */
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
    }

    pm4_event_write_eop(b,fence,fv);
    return b->off*4;
}

/* === Standalone shadow DCB ===
   Renders model from sun POV into shadow_depth as an RGBA8 COLOR target.
   MRT0.r = NDC.z (8-bit quantized — sufficient precision for small-scene shadow).

   WHY COLOR not DEPTH: writing depth with D32_SFLOAT and sampling as R32_FLOAT
   creates two separate Vulkan images at the same GPU address (Vulkan §39.1.6
   forbids cross-format views between depth and color). shadPS4's texture cache
   would see a format mismatch and allocate a second image, silently breaking
   the write→sample chain. By making both ends RGBA8, shadPS4 reuses a single
   VkImage for write and sample, so cube depths actually become visible to the
   floor PS's compare. */
static uint32_t build_shadow_dcb(struct PM4Builder *b,
    const void *vs, const void *ps_shadow, const void *ps_clear,
    const uint32_t *shadow_vb_v, const uint32_t *shadow_floor_v,
    const uint32_t *bg_v, uint32_t *desc,
    int model_verts, int floor_verts,
    uint32_t *ib_ptr, int num_indices, int is_indexed,
    void *shadow_depth,
    volatile uint32_t *fence, uint32_t fv) {

    /* Default hardware-state init — same as main DCB. The shadow buffer is a
       separate command buffer (it runs first when present), so it also needs
       the register defaults established before any draw. Idempotent with the
       main DCB's copy when both run. */
    pm4_init_default_hw_state(b);

    pm4_context_control(b);

    /* VS program — same as main pass. PGM_RSRC1=0x4B (46 VGPRs, 16 SGPRs);
       see main-pass note — the shader uses up to v45. */
    { uint64_t a=(uint64_t)(uintptr_t)vs;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x4Bu,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }

    /* Scissors/viewport — shadow map size */
    { uint32_t s[2]={0,(SHADOW_W&0x7FFF)|((SHADOW_H&0x7FFF)<<16)};
      pm4_set_context_regs(b,CTX_SCREEN_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_GENERIC_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_VIEWPORT_SCISSOR0,s,2);
      s[0]=(1u<<31);
      pm4_set_context_regs(b,CTX_WINDOW_SCISSOR,s,2); }
    pm4_emit(b,pm4_type3(PM4_SET_CONTEXT_REG,7));
    pm4_emit(b,CTX_VIEWPORT0);
    pm4_emit_f(b,(float)SHADOW_W*0.5f); pm4_emit_f(b,(float)SHADOW_W*0.5f);
    /* Y scale POSITIVE (no flip) — pixel row 0 at NDC y=-1, row SHADOW_H-1 at NDC y=+1.
       In Vulkan texture coords (UV.y = 0 at top-left = pixel row 0), this means:
         NDC y=-1 → UV.y=0  ;  NDC y=+1 → UV.y=1
       Floor PS computes UV.y = (NDC.y+1)*0.5, matching exactly. */
    pm4_emit_f(b,(float)SHADOW_H*0.5f); pm4_emit_f(b,(float)SHADOW_H*0.5f);
    /* Z scale=1, zoffset=0 (ZeroToOne clip-space): fragment z as stored = raw NDC.z.
       Must match the floor PS's NDC.z computation (which also does no extra scale). */
    pm4_emit_f(b,1.0f); pm4_emit_f(b,0.0f);

    pm4_set_context_reg(b,CTX_INDEX_OFFSET,0);

    /* === CB0 = RGBA8 at shadow_depth ===
       14 consecutive context regs starting at CTX_CB_COLOR0_BASE:
         +0  BASE           = shadow_depth >> 8
         +1  PITCH          = (width/8)-1  (LSB) in pixel_tile_max encoding
         +2  SLICE          = (width*height/64)-1
         +3  VIEW           = 0 (no slice offset)
         +4  INFO           = 0x00A8 → data_format=10 (R8G8B8A8), num_type=0 (Unorm),
                              linear_general=1, COMP_SWAP=0 (RGBA, matches T# read order)
         +5..+13 = 0 (no compression, no FMask, etc.)

       BUG FIX (CAFE0107 era): previous value was 0x01A8 with comment claiming
       "num_type=0 (Unorm)". The comment was wrong — 0x01A8 decodes as SNORM
       (bit 8 set = num_type=1=SNORM per AMD CIK ISA register reference).
       That caused PS output 1.0 to be encoded as SNORM8 = 127, then T# read
       as UNORM8 = 127/255 = 0.498. The clear region of the shadow map was
       producing 0.5 instead of 1.0. Fixed by clearing bit 8 → 0x00A8 = UNORM.
       Trailing 0xC0001000 NOP carries the actual {width,height} extent for shadPS4's
       ImageInfo constructor — required by liverpool.cpp:344 assertion. */
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)shadow_depth>>8);
      uint32_t r[14]={c,(SHADOW_W/8u)-1u,(SHADOW_W*SHADOW_H/64u)-1u,0,
                      0x00A8u,0,0,0,0,0,0,0,0,0};   /* num_type=0=UNORM (was 0x01A8=SNORM) */
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,SHADOW_W|(SHADOW_H<<16)); }

    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);   /* RGBA channel write enable */
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);   /* PS exports all 4 channels */

    /* No depth buffer this pass. Disable depth test entirely. Order of draws is
       the shadow map's "depth test": clear-quad first (fills with R=1.0),
       then cube draw on top (writes smaller R values where cube covers). Without
       a depth buffer, last-write-wins per pixel — fine since cube is the only
       occluder and culling handles the cube self-overlap from light's POV. */
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,0);
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);

    /* Pipeline state — color pass, RGBA8 export.
       Shadow PS reads attr1=world_pos. NUM_INTERP=2 (VS exports 2 params);
       PS_INPUT_CNTL_0 maps attr0→slot0, attr1→slot1. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0+1,1);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,1);   /* 2 param exports */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);    /* PERSP_CENTER_ENA */
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,2);
    pm4_set_context_reg(b,CTX_SHADER_POS_FORMAT,4);
    pm4_set_context_reg(b,CTX_Z_EXPORT_FORMAT,0);
    pm4_set_context_reg(b,CTX_COLOR_EXPORT_FORMAT,9);  /* 32_R_GR (RGBA8 pixel pipe) — same as main CB */
    pm4_set_context_reg(b,CTX_COLOR_CONTROL,0x00CC0010u);
    pm4_set_context_reg(b,0x203,0);
    /* ClipperControl = ZeroToOne (bit 19) — matches main DCB clip convention,
       matches the way light_MVP is constructed (NDC.z ∈ [0,1]). */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    pm4_set_context_reg(b,CTX_MODE_CONTROL,0);
    pm4_set_context_reg(b,CTX_STAGE_ENABLE,0);
    pm4_set_context_reg(b,CTX_AA_CONFIG,0);
    pm4_set_context_reg(b,CTX_BLEND_CONTROL0,0);
    pm4_set_context_reg(b,CTX_INDEX_SIZE,0);
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);

    /* === Draw 1: fullscreen-clear quad with ps_clear ===
       BG verts are NDC-space fullscreen quad at z=0.999. With identity MVP in
       bg_v's referenced VB, the VS multiplies and outputs unchanged NDC. PS
       outputs (1,0,0,1) — fills shadow map with R=1.0 = "no occluder anywhere".
       This replaces the prior DMA fill, which wasn't reliably reaching shadPS4's
       VkImage cache (DMA wrote to GPU memory but the texture image kept stale
       contents from previous frames). A real GPU render pass clear works. */
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0);   /* no culling for fullscreen quad */
    { uint64_t a=(uint64_t)(uintptr_t)ps_clear;
      /* Clear PS uses v40..v43 for output — needs 44 VGPRs allocated.
         VGPR field = ceil(44/4) - 1 = 10. SGPR field = 0 (only s0, s1 = desc ptr).
         PGM_RSRC1 = (0<<6) | 10 = 0x0A. user_sgpr=2 in RSRC2.
         (Previous version had PGM_RSRC1=0 = 4 VGPRs — writes to v40+ went nowhere,
         so EXP read uninitialized garbage → shadow map filled with 0, not 1.0,
         making everything inside the light frustum read as "shadowed".) */
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x0Au,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b, BG_VERTS);   /* 6 verts = 2 triangles = fullscreen quad */

    /* === Draw 2: cube with shadow PS that exports NDC.z ===
       Switch PS to ps_shadow (NDC.z exporter). PGM_RSRC1 = 0x14A (48 SGPR / 44 VGPR). */
    { uint64_t a=(uint64_t)(uintptr_t)ps_shadow;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x14Au,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

    /* Cull back-facing (from light POV) so only the light-facing cube surface
       is written. Without depth test, this prevents the back face overwriting
       the front face's depth value. */
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1u<<1));

    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,shadow_vb_v,4);
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1);
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
    }

    /* Floor as shadow caster was producing severe shadow acne — every floor
       fragment compared against its own depth in the shadow map without bias
       gives a noisy stripe pattern across the entire floor. Disabled until
       proper depth bias / slope-scale bias is added. */
    (void)shadow_floor_v; (void)floor_verts;

    /* Flush CB writes + invalidate TC so main DCB sees freshly-written shadow map.
       (1u<<25) = CB_ACTION_ENA  (flush CB pixel pipe)
       (1u<<23) = TC_ACTION_ENA  (invalidate texture cache)
       (1u<<6)  = CB_DEST_BASE_ENA */
    pm4_acquire_mem(b, (1u<<25)|(1u<<23)|(1u<<6));

    /* No EOP fence — shadow+main submitted as one Vulkan command buffer so the
       acquire_mem above is the barrier. */
    return b->off*4;
}

// Read entire file into GPU memory. Returns NULL if not found.
static void *read_file_gpu(const char *path, unsigned long *out_size) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    long sz = sceKernelLseek(fd, 0, 2);
    sceKernelLseek(fd, 0, 0);
    if (sz <= 0 || sz > 4000000000LL) { sceKernelClose(fd); return 0; }
    void *buf = gpu_alloc((unsigned long)sz, 0x1000);
    if (!buf) { sceKernelClose(fd); return 0; }
    long total = 0;
    while (total < sz) {
        long n = sceKernelRead(fd, (char*)buf + total, (unsigned long)(sz - total));
        if (n <= 0) break;
        total += n;
    }
    sceKernelClose(fd);
    *out_size = (unsigned long)total;
    return buf;
}

// ============================================================================
// BMP texture loader — reads 24/32-bit uncompressed BMP, converts to RGBA
// ============================================================================
static void *load_bmp_file(const char *path, int *w, int *h) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    long sz = sceKernelLseek(fd, 0, 2); sceKernelLseek(fd, 0, 0);
    if (sz < 54) { sceKernelClose(fd); return 0; }
    uint8_t *d = (uint8_t*)gpu_alloc((unsigned long)sz, 0x1000);
    if (!d) { sceKernelClose(fd); return 0; }
    long t=0; while(t<sz){long n=sceKernelRead(fd,(char*)d+t,(unsigned long)(sz-t));if(n<=0)break;t+=n;}
    sceKernelClose(fd);
    if (d[0]!='B'||d[1]!='M') return 0;
    int32_t width=*(int32_t*)(d+18), height=*(int32_t*)(d+22);
    uint16_t bpp=*(uint16_t*)(d+28); uint32_t comp=*(uint32_t*)(d+30);
    uint32_t poff=*(uint32_t*)(d+10);
    int topdown=0; if(height<0){height=-height;topdown=1;}
    if(width<1||height<1||comp!=0) return 0;
    if(bpp!=24&&bpp!=32) return 0;
    int bpx=bpp/8, stride=(width*bpx+3)&~3;
    uint8_t *rgba=(uint8_t*)gpu_alloc((unsigned long)width*height*4,0x1000);
    if(!rgba) return 0;
    for(int y=0;y<height;y++){
        int sy=topdown?y:(height-1-y);
        uint8_t *sr=d+poff+(unsigned long)sy*stride, *dr=rgba+(unsigned long)y*width*4;
        for(int x=0;x<width;x++){
            dr[x*4]=sr[x*bpx+2]; dr[x*4+1]=sr[x*bpx+1];
            dr[x*4+2]=sr[x*bpx]; dr[x*4+3]=(bpp==32)?sr[x*bpx+3]:255;
        }
    }
    *w=width; *h=height; return rgba;
}




// ============================================================================
// Float/int parsers for OBJ — with end-of-buffer safety
static float pf(const char **pp, const char *end) {
    const char *p = *pp;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    float s = 1;
    if (p < end && *p == '-') {
        s = -1;
        p++;
    } else if (p < end && *p == '+') {
        p++;
    }
    float v = 0, f = 0, d = 1;
    int dot = 0;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.')) {
        if (*p == '.') {
            dot = 1;
            p++;
            continue;
        }
        if (dot) {
            f = f * 10 + (*p - '0');
            d *= 10;
        } else {
            v = v * 10 + (*p - '0');
        }
        p++;
    }
    *pp = p;
    return s * (v + f / d);
}
static int pi(const char **pp, const char *end) {
    const char *p = *pp;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    int s = 1, v = 0;
    if (p < end && *p == '-') {
        s = -1;
        p++;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    *pp = p;
    return s * v;
}

// Inline OBJ parser — minimal, no static arrays, writes directly to vb
// ============================================================================

// Parse OBJ text, write vertices to vb at CUBE_DATA_OFF.
// Allocates temp arrays via gpu_alloc. Returns vertex count.
static int parse_obj_inline(const char *data, unsigned long size, void *vb_base) {
    // Temp arrays (gpu_alloc, not BSS)
    int maxv = 16000000, maxt = 16000000;
    float *vx = (float*)gpu_alloc(maxv*4, 0x1000);
    float *vy = (float*)gpu_alloc(maxv*4, 0x1000);
    float *vz = (float*)gpu_alloc(maxv*4, 0x1000);
    float *tu = (float*)gpu_alloc(maxt*4, 0x1000);
    float *tv = (float*)gpu_alloc(maxt*4, 0x1000);
    if (!vx||!vy||!vz||!tu||!tv) return 0;

    float *out = (float*)((char*)vb_base + CUBE_DATA_OFF);
    int nv=0, nt=0, nf=0;
    const float lx=0.3f,ly=0.7f,lz=0.5f,ll=0.86f;
    const char *p=data, *end=data+size;

    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p >= end)
            break;

        // Skip comment lines
        if (*p == '#') {
            while (p < end && *p != '\n' && *p != '\r')
                p++;
            if (p < end)
                p++;
            continue;
        }

        if (*p == 'v' && p + 1 < end && *(p + 1) == ' ' && nv < maxv) {
            p += 2;
            vx[nv] = pf(&p, end);
            vy[nv] = pf(&p, end);
            vz[nv] = pf(&p, end);
            nv++;
        } else if (*p == 'v' && p + 2 < end && *(p + 1) == 't' &&
                   *(p + 2) == ' ' && nt < maxt) {
            p += 3;
            tu[nt] = pf(&p, end);
            tv[nt] = pf(&p, end);
            nt++;
        } else if (*p == 'f' && p + 1 < end && *(p + 1) == ' ') {
            p += 2;
            int fv[32], ft[32], fc = 0;
            while (p < end && *p != '\n' && *p != '\r' && fc < 32) {
                while (p < end && (*p == ' ' || *p == '\t'))
                    p++;
                if (p >= end || *p == '\n' || *p == '\r')
                    break;
                int vi2 = pi(&p, end);
                fv[fc] = (vi2 < 0) ? (nv + vi2) : (vi2 - 1);
                ft[fc] = -1;
                if (p < end && *p == '/') {
                    p++;
                    if (p < end && *p != '/') {
                        int ti2 = pi(&p, end);
                        ft[fc] = (ti2 < 0) ? (nt + ti2) : (ti2 - 1);
                    }
                    if (p < end && *p == '/') {
                        p++;
                        pi(&p, end);
                    }
                }
                fc++;
            }
            for(int i=1;i+1<fc&&nf<4000000;i++) {
                int idx[3]={0,i,i+1};
                float ax=0,ay=0,az=0,bx=0,by=0,bz=0;
                if(fv[idx[0]]>=0&&fv[idx[0]]<nv&&fv[idx[1]]>=0&&fv[idx[1]]<nv&&fv[idx[2]]>=0&&fv[idx[2]]<nv){
                    ax=vx[fv[idx[1]]]-vx[fv[idx[0]]];ay=vy[fv[idx[1]]]-vy[fv[idx[0]]];az=vz[fv[idx[1]]]-vz[fv[idx[0]]];
                    bx=vx[fv[idx[2]]]-vx[fv[idx[0]]];by=vy[fv[idx[2]]]-vy[fv[idx[0]]];bz=vz[fv[idx[2]]]-vz[fv[idx[0]]];
                }
                float nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;
                float nl=nx*nx+ny*ny+nz*nz,g=nl;
                if(nl>0.0001f){g=0.5f*(g+nl/g);g=0.5f*(g+nl/g);g=0.5f*(g+nl/g);nx/=g;ny/=g;nz/=g;}
                float sh=(nx*lx+ny*ly+nz*lz)/ll; if(sh<0)sh=-sh; if(sh<0.15f)sh=0.15f; if(sh>1)sh=1;
                for(int j=0;j<3;j++){
                    int vi=fv[idx[j]],ti=ft[idx[j]],o=nf*3*8+j*8;
                    out[o]=(vi>=0&&vi<nv)?vx[vi]:0; out[o+1]=(vi>=0&&vi<nv)?vy[vi]:0;
                    out[o+2]=(vi>=0&&vi<nv)?vz[vi]:0; out[o+3]=1;
                    out[o+4]=(ti>=0&&ti<nt)?tu[ti]:0; out[o+5]=(ti>=0&&ti<nt)?tv[ti]:0;
                    out[o+6]=sh; out[o+7]=1;
                }
                nf++;
            }
        }
        while(p<end&&*p!='\n'&&*p!='\r')p++;
        if(p<end)p++;
    }
    // Auto-scale to +-0.4
    if(nv>0){
        float mnx=vx[0],mny=vy[0],mnz=vz[0],mxx=vx[0],mxy=vy[0],mxz=vz[0];
        for(int i=1;i<nv;i++){
            if(vx[i]<mnx)mnx=vx[i];if(vx[i]>mxx)mxx=vx[i];
            if(vy[i]<mny)mny=vy[i];if(vy[i]>mxy)mxy=vy[i];
            if(vz[i]<mnz)mnz=vz[i];if(vz[i]>mxz)mxz=vz[i];
        }
        float cx=(mnx+mxx)*.5f,cy=(mny+mxy)*.5f,cz=(mnz+mxz)*.5f;
        float ext=mxx-mnx;if(mxy-mny>ext)ext=mxy-mny;if(mxz-mnz>ext)ext=mxz-mnz;
        float sc=(ext>0.0001f)?0.8f/ext:1;
        int tot=nf*3;
        for(int i=0;i<tot;i++){out[i*8]=(out[i*8]-cx)*sc;out[i*8+1]=(out[i*8+1]-cy)*sc;out[i*8+2]=(out[i*8+2]-cz)*sc;}
    }
    return nf*3;
}

// === Main ===

/* ============================================================
 * STL PARSER — Binary STL format (most common from CAD/3D printing)
 * Format: 80-byte header, 4-byte triangle count, then per-triangle:
 *   12 bytes normal (3 floats), 36 bytes vertices (3×3 floats), 2 bytes attr
 * No UV coordinates — uses per-face shading only.
 * ============================================================ */
static int parse_stl_inline(const uint8_t *data, unsigned long sz,
                            float *vb_out, int max_tris) {
    if (sz < 84) return 0;
    unsigned int ntri = *(unsigned int*)(data + 80);
    if (84 + ntri * 50 > sz) ntri = (sz - 84) / 50;
    if ((int)ntri > max_tris) ntri = max_tris;
    
    /* Auto-scale: find bounding box */
    float minx=1e30f,miny=1e30f,minz=1e30f,maxx=-1e30f,maxy=-1e30f,maxz=-1e30f;
    for (unsigned int i = 0; i < ntri && i < 10000; i++) {
        const float *v = (const float*)(data + 84 + i*50 + 12);
        for (int j=0;j<3;j++) {
            float x=v[j*3],y=v[j*3+1],z=v[j*3+2];
            if(x<minx)minx=x; if(x>maxx)maxx=x;
            if(y<miny)miny=y; if(y>maxy)maxy=y;
            if(z<minz)minz=z; if(z>maxz)maxz=z;
        }
    }
    float cx=(minx+maxx)*0.5f, cy=(miny+maxy)*0.5f, cz=(minz+maxz)*0.5f;
    float range=maxx-minx; if(maxy-miny>range)range=maxy-miny; if(maxz-minz>range)range=maxz-minz;
    float scale = (range > 0.0001f) ? 0.8f / range : 1.0f;
    
    int nout = 0;
    for (unsigned int i = 0; i < ntri; i++) {
        const float *n = (const float*)(data + 84 + i*50);
        const float *v = n + 3;
        /* Per-face shade from normal */
        float shade = 0.3f + 0.7f * (n[1] > 0 ? n[1] : -n[1]);
        if (shade > 1.0f) shade = 1.0f;
        for (int j = 0; j < 3; j++) {
            float *out = vb_out + nout * 8;
            out[0] = (v[j*3+0] - cx) * scale;
            out[1] = (v[j*3+1] - cy) * scale;
            out[2] = (v[j*3+2] - cz) * scale;
            out[3] = 1.0f;
            out[4] = 0.0f; out[5] = 0.0f; /* no UVs */
            out[6] = shade;
            out[7] = 0.0f;
            nout++;
        }
    }
    return nout;
}

/* ============================================================
 * PLY PARSER — Stanford PLY format (common for 3D scans)
 * Supports ASCII and binary_little_endian with float x,y,z vertices
 * and triangular faces.
 * ============================================================ */
static int parse_ply_inline(const uint8_t *data, unsigned long sz,
                            float *vx, float *vy, float *vz,
                            float *vb_out, int maxv, int max_tris) {
    /* Parse header */
    const char *p = (const char*)data;
    const char *end = (const char*)data + (sz < 4096 ? sz : 4096);
    int nv = 0, nf = 0, is_binary = 0;
    int header_end = 0;
    
    while (p < end) {
        if (p[0]=='e' && p[1]=='l' && p[2]=='e' && p[3]=='m') {
            /* element vertex N or element face N */
            const char *q = p + 8;
            while (*q == ' ') q++;
            if (p[8]=='v') { while(*q && *q!=' ')q++; nv=0; while(*q>='0'&&*q<='9'){nv=nv*10+(*q-'0');q++;} }
            if (p[8]=='f') { while(*q && *q!=' ')q++; nf=0; while(*q>='0'&&*q<='9'){nf=nf*10+(*q-'0');q++;} }
        }
        if (p[0]=='f' && p[1]=='o' && p[2]=='r' && p[3]=='m') {
            if (p[7]=='b') is_binary = 1;
        }
        if (p[0]=='e' && p[1]=='n' && p[2]=='d' && p[3]=='_') {
            while (p < end && *p != '\n') p++;
            p++;
            header_end = (int)(p - (const char*)data);
            break;
        }
        while (p < end && *p != '\n') p++;
        p++;
    }
    
    if (nv <= 0 || nv > maxv) return 0;
    if (nf > max_tris) nf = max_tris;
    
    /* For now only support binary PLY */
    if (!is_binary) return 0;
    
    const uint8_t *bp = data + header_end;
    /* Read vertices (assume x,y,z as first 3 floats per vertex) */
    /* TODO: parse property list to find actual stride */
    int vert_stride = 12; /* minimum: 3 floats */
    for (int i = 0; i < nv && bp + 12 <= data + sz; i++) {
        const float *fv = (const float*)bp;
        vx[i] = fv[0]; vy[i] = fv[1]; vz[i] = fv[2];
        bp += vert_stride;
    }
    
    /* Auto-scale */
    float minx=vx[0],maxx=vx[0],miny=vy[0],maxy=vy[0],minz=vz[0],maxz=vz[0];
    for(int i=1;i<nv;i++){
        if(vx[i]<minx)minx=vx[i];if(vx[i]>maxx)maxx=vx[i];
        if(vy[i]<miny)miny=vy[i];if(vy[i]>maxy)maxy=vy[i];
        if(vz[i]<minz)minz=vz[i];if(vz[i]>maxz)maxz=vz[i];
    }
    float cx=(minx+maxx)*0.5f,cy=(miny+maxy)*0.5f,cz=(minz+maxz)*0.5f;
    float range=maxx-minx;if(maxy-miny>range)range=maxy-miny;if(maxz-minz>range)range=maxz-minz;
    float scale=(range>0.0001f)?0.8f/range:1.0f;
    
    /* Read faces and emit vertices */
    int nout = 0;
    for (int i = 0; i < nf && bp + 4 <= data + sz; i++) {
        int fc = *bp++;  /* face vertex count */
        if (fc < 3 || bp + fc*4 > data + sz) break;
        const int *idx = (const int*)bp;
        bp += fc * 4;
        /* Triangulate fan */
        for (int j = 1; j < fc - 1 && nout + 3 <= max_tris * 3; j++) {
            int i0=idx[0], i1=idx[j], i2=idx[j+1];
            if(i0<0||i0>=nv||i1<0||i1>=nv||i2<0||i2>=nv) continue;
            float nx=(vy[i1]-vy[i0])*(vz[i2]-vz[i0])-(vz[i1]-vz[i0])*(vy[i2]-vy[i0]);
            float ny=(vz[i1]-vz[i0])*(vx[i2]-vx[i0])-(vx[i1]-vx[i0])*(vz[i2]-vz[i0]);
            float nlen=nx*nx+ny*ny; nlen=nlen>0?1.0f/nlen:0; /* approx */
            float shade=0.3f+0.5f*(ny>0?ny:-ny)*nlen;
            if(shade>1)shade=1;
            int vi[3]={i0,i1,i2};
            for(int k=0;k<3;k++){
                float *out=vb_out+nout*8;
                out[0]=(vx[vi[k]]-cx)*scale; out[1]=(vy[vi[k]]-cy)*scale;
                out[2]=(vz[vi[k]]-cz)*scale; out[3]=1.0f;
                out[4]=0; out[5]=0; out[6]=shade; out[7]=0;
                nout++;
            }
        }
    }
    return nout;
}

/* --- Loading screen --- */
struct LoadCtx {
    const void *vs; const void *ps;
    uint32_t *vb_v; uint32_t *bg_v;
    void *vb; uint32_t *desc;
    void *fb0; void *fb1; void *depth;
    volatile uint32_t *fence;
    int video; uint32_t *pm4_buf;
    int flip_idx;
    const void *ps_blue;
    const void *ps_dark;
};

static void loading_progress(int pass, const char *msg, void *ud) {
    struct LoadCtx *c = (struct LoadCtx*)ud;
    float progress = (float)pass / 3.0f; /* 0.0 → 1.0 */

    /* Loading sky: SOLID blue (both zenith and horizon = blue) to cover behind rendering */
    {
        float *sz = (float*)(c->desc + 24); /* zenith */
        sz[0]=0.08f; sz[1]=0.20f; sz[2]=0.55f; sz[3]=0;
        float *sh = (float*)(c->desc + 28); /* horizon */
        sh[0]=0.08f; sh[1]=0.20f; sh[2]=0.55f; sh[3]=0;
        /* Sun NDC off-screen (no disc on loading screen) */
        float *snd = (float*)(c->desc + 16);
        snd[0]=10.0f; snd[1]=10.0f; snd[2]=0.0001f; snd[3]=0;
        /* Sun color black so no bleed if anything samples it */
        float *sc = (float*)(c->desc + 20);
        sc[0]=0; sc[1]=0; sc[2]=0; sc[3]=0;
    }

    /* BG quad: must cover full screen — keep normals any direction, shader just reads attr0 */
    float *bg = (float*)((char*)c->vb + BG_DATA_OFF);
    for (int i = 0; i < 6; i++) {
        bg[i*12+4]=0; bg[i*12+5]=1; bg[i*12+6]=0; bg[i*12+7]=0;
    }

    /* Identity MVP so bar positions pass through as NDC directly (covers model) */
    float *mvp = (float*)((char*)c->vb + MVP_OFF);
    mvp[0]=1;mvp[1]=0;mvp[2]=0;mvp[3]=0;
    mvp[4]=0;mvp[5]=1;mvp[6]=0;mvp[7]=0;
    mvp[8]=0;mvp[9]=0;mvp[10]=1;mvp[11]=0;
    mvp[12]=0;mvp[13]=0;mvp[14]=0;mvp[15]=1;

    /* Progress bar: WHITE via ps_blue (which outputs solid color regardless of tex)
       Position near bottom center. z=0 (in front of sky BG which is also z=0),
       but bar drawn AFTER BG so it overlays */
    float *bar = (float*)((char*)c->vb + CUBE_DATA_OFF);
    float x0 = -0.7f, x1 = -0.7f + progress * 1.4f;
    float y0 = -0.65f, y1 = -0.55f;
    { float bv[6][12] = {
        {x0,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x0,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x0,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0}
      };
      for(int vi=0;vi<6;vi++) for(int fi=0;fi<12;fi++) bar[vi*12+fi]=bv[vi][fi];
    }

    struct PM4Builder pm4;
    pm4_init(&pm4, c->pm4_buf, 0x10000/4);
    int bi = c->flip_idx & 1;
    uint32_t fv = c->flip_idx + 100;
    *c->fence = 0;
    /* Model PS = ps_blue (white bar), Sky PS = ps_dark (dynamic — will render solid blue from desc) */
    uint32_t sz = build_dcb(&pm4, c->vs, c->ps_blue, c->ps_dark, 0, 0,
                            c->vb_v, c->bg_v, 0, 0,
                            c->vb, c->desc, 6, VERT_BUF_SIZE, 0, 0, 0,
                            bi ? c->fb1 : c->fb0, c->depth, 0, c->fence, fv);
    const uint32_t *a[1] = { c->pm4_buf };
    uint32_t s2[1] = { sz };
    sceGnmSubmitCommandBuffers(1, (void**)a, s2, 0, 0);
    sceGnmSubmitDone();
    /* Bounded wait (was infinite). If the GPU faults the fence never signals;
       cap the wait so we still reach the main render loop instead of hanging
       here forever. */
    for (int w=0; w<10000 && *c->fence != fv; w++) sceKernelUsleep(100);
    sceVideoOutSubmitFlip(c->video, bi, 1, 0);
    sceKernelUsleep(16000);
    c->flip_idx++;
}

int main(void) {
    printf("=== ShadCube4 ===\n");

    int video = sceVideoOutOpen(0,0,0,0);
    if (video < 0) return 1;
    sceVideoOutSetFlipRate(video,0);

    unsigned long fb_size = (unsigned long)DISPLAY_W*DISPLAY_H*4;
    void *fb[NUM_FRAMES];
    for (int i=0;i<NUM_FRAMES;i++) { fb[i]=gpu_alloc(fb_size,0x100000); if(!fb[i])return 1; }

    unsigned char buf_attr[48]; my_memset(buf_attr,0,48);
    /* tiling_mode = 1 (LINEAR) — must match the CB_COLOR0 render target, which
       writes linear (LINEAR_GENERAL set in CB_COLOR0_INFO). With tiling_mode=0
       (TILE) the display reads the linear pixels as tiled → scrambled colors.
       Verified vs shadPS4 buffer.h: TilingMode::Tile=0, Linear=1. */
    sceVideoOutSetBufferAttribute(buf_attr,0x80000000,1,0,DISPLAY_W,DISPLAY_H,DISPLAY_W);
    sceVideoOutRegisterBuffers(video,0,fb,NUM_FRAMES,buf_attr);

    void *depth=gpu_alloc(fb_size,0x10000);

    /* Shadow map: 4096×4096 (4K) — file-scope SHADOW_W/SHADOW_H drives this.
       Previously redefined locally to 512; that local override has been removed
       so the 4K file-scope value applies consistently. ~64 MB allocation. */
    unsigned long shadow_size = (unsigned long)SHADOW_W * SHADOW_H * 4UL;
    void *shadow_depth = gpu_alloc(shadow_size, 0x100000);
    /* GPU shadow pass writes to this buffer during rendering. */
    /* Light-space MVP will be written to vb+LIGHT_MVP_OFF each frame.
       Shadow VS reads from V# base + 0x00 (same as normal VS), but we swap
       what's at that offset for the shadow pass. */

    /* Try loading BMP texture, fallback to logo */
    void *tex = 0; int tex_w = LOGO_WIDTH, tex_h = LOGO_HEIGHT;
    {
        BmpTexture bmp;
        const char *bmp_paths[] = { "/data/CUBETST00/texture.bmp", "/data/CUBETST00/model.bmp", 0 };
        for (int bi = 0; bmp_paths[bi]; bi++) {
            if (bmp_load(bmp_paths[bi], gpu_alloc, &bmp) == 0) {
                tex = bmp.pixels; tex_w = bmp.width; tex_h = bmp.height;
                break;
            }
        }
    }
    if (!tex) {
        /* No texture file on disk — fall back to the embedded shadPS4 logo
           (256×256 RGBA8 from logo_texture.h). Previous code created a 1×1
           white pixel here, which is why the cube rendered solid white when
           texture.bmp was missing. */
        tex = gpu_alloc(LOGO_SIZE, 0x1000);
        my_memcpy(tex, logo_rgba, LOGO_SIZE);
        tex_w = LOGO_WIDTH; tex_h = LOGO_HEIGHT;
    }

    /* Floor albedo texture. */
    void *floor_albedo_tex = 0;
    int floor_tex_w = 1, floor_tex_h = 1;
    {
        BmpTexture bmp;
        if (bmp_load("/data/CUBETST00/floor_albedo.bmp", gpu_alloc, &bmp) == 0) {
            floor_albedo_tex = bmp.pixels;
            floor_tex_w = bmp.width; floor_tex_h = bmp.height;
        }
    }
    if (!floor_albedo_tex) {
        /* Fallback to mid-grey 1×1 so floor still renders something visible. */
        floor_albedo_tex = gpu_alloc(4, 0x1000);
        unsigned char *p = (unsigned char*)floor_albedo_tex;
        p[0]=128; p[1]=128; p[2]=128; p[3]=255;
        floor_tex_w = 1; floor_tex_h = 1;
    }

    /* Floor normal map texture from BMP. Tangent-space normal map: each
       pixel encodes (n.x*0.5+0.5, n.y*0.5+0.5, n.z*0.5+0.5) in RGB.
       Floor PS samples this at desc[72..79] and uses it for per-fragment
       Lambert lighting.

       Falls back to a 1×1 "no-bump" texel encoding the unit Z normal
       (0.5, 0.5, 1.0 → byte (128, 128, 255)) so floor still renders if
       the BMP file is missing. */
    void *floor_normal_tex = 0;
    int floor_nrm_w = 1, floor_nrm_h = 1;
    {
        BmpTexture bmp;
        if (bmp_load("/data/CUBETST00/floor_normal.bmp", gpu_alloc, &bmp) == 0) {
            floor_normal_tex = bmp.pixels;
            floor_nrm_w = bmp.width; floor_nrm_h = bmp.height;
        }
    }
    if (!floor_normal_tex) {
        floor_normal_tex = gpu_alloc(4, 0x1000);
        unsigned char *p = (unsigned char*)floor_normal_tex;
        p[0]=128; p[1]=128; p[2]=255; p[3]=255;   /* flat tangent normal */
        floor_nrm_w = 1; floor_nrm_h = 1;
    }

    /* Floor displacement texture (height map) — sampled per-vertex during
       floor mesh generation in build_static_vb. R channel is the height
       value [0,1]; final Y offset = (sample - 0.5) * DISP_SCALE so it
       displaces both up and down from the curve. NULL if missing → no
       displacement applied (curve-only floor). */
    void *floor_disp_tex = 0;
    int floor_disp_w = 0, floor_disp_h = 0;
    {
        BmpTexture bmp;
        if (bmp_load("/data/CUBETST00/floor_displacement.bmp", gpu_alloc, &bmp) == 0) {
            floor_disp_tex = bmp.pixels;
            floor_disp_w = bmp.width; floor_disp_h = bmp.height;
            /* Auto-stretch the displacement range to span [0,255]. Many
               displacement BMPs use a compressed value range (e.g. only
               100-130 out of 0-255), which produces imperceptible
               displacement. Scan ALL pixels for the actual R range, then
               remap each pixel in-place. After this, the texture has full
               dynamic range and build_static_vb sees the proper variation. */
            unsigned char *px = (unsigned char*)bmp.pixels;
            unsigned long total = (unsigned long)bmp.width * (unsigned long)bmp.height;
            int min_r = 255, max_r = 0;
            for (unsigned long i = 0; i < total; i++) {
                int r = px[i * 4];   /* RGBA8 after bmp_load conversion */
                if (r < min_r) min_r = r;
                if (r > max_r) max_r = r;
            }
            int range = max_r - min_r;
            if (range > 0 && range < 240) {
                /* Compressed range — stretch to full [0,255]. */
                for (unsigned long i = 0; i < total; i++) {
                    int v = ((int)px[i * 4] - min_r) * 255 / range;
                    if (v < 0) v = 0; else if (v > 255) v = 255;
                    px[i * 4] = (unsigned char)v;
                }
            }
        }
    }

    /* desc buffer layout:
         desc[0..7]    texture T#              (byte 0..31)
         desc[8..11]   sampler S#              (byte 32..47)
         desc[12..15]  sun direction           (byte 48..63)
         desc[16..19]  sun NDC position        (byte 64..79)
         desc[20..23]  light color RGBA        (byte 80..95)
         desc[24..27]  sky zenith RGBA         (byte 96..111)
         desc[28..31]  sky horizon RGBA        (byte 112..127)
         desc[40..47]  shadow T#               (byte 160..191)
         desc[48..63]  light_MVP 4x4           (byte 192..255)
         desc[64..71]  floor albedo T#         (byte 256..287)
         desc[72..79]  floor normal T#         (byte 288..319)
       512 bytes = 128 dwords gives headroom. */
    uint32_t *desc=(uint32_t*)gpu_alloc(512,0x100);
    build_tsharp(desc,tex,tex_w,tex_h);
    build_ssharp_aniso(desc+8);   /* 16× anisotropic — used by cube + floor */
    /* Second sampler at desc[80..83]: PCF depth-compare sampler for the floor
       PS (CAFE100C) projective shadow lookup. Works in tandem with:
         - Shadow T# at desc[40..47] (R32_FLOAT, shadPS4 promotes to depth view
           when the shader issues IMAGE_SAMPLE_C_LZ via is_depth inference)
         - Shadow DB cleared to 1.0 every frame (DEPTH_CLEAR=1.0f, LESS compare)
         - Floor PS computes fragment's z_ref in light-clip NDC space
       The sampler's depth_compare_func=LessEqual gives "ref <= stored ? lit".
       border_color_type=White returns 1.0 for out-of-frustum UVs so that
       unmapped areas of the floor are lit, not shadowed. */
    build_ssharp_pcf(desc+80);
    /* Sun direction at desc[12:15] — PS reads via s_load at offset 0x0C */
    { float *sun = (float*)(desc + 12);
      sun[0] = my_sin(3.14f)*0.766f; sun[1] = 0.643f; sun[2] = my_cos(3.14f)*0.766f; sun[3] = 0; }
    /* Initial sun screen pos (updated per frame in render loop) */
    { float *sd = (float*)(desc + 16);
      sd[0] = 0.5f; sd[1] = 0.5f; sd[2] = 0.006f; sd[3] = 1.0f; }

    /* Shadow map T# descriptor at desc[40:47] (byte offset 0xA0).
       DIAGNOSTIC: RGBA8 format (matches diagnostic CB_INFO=0x1A8).
       build_tsharp sets format=10 (8_8_8_8 UNORM), which is what we want. */
    /* Shadow map T# at desc[40..47]. Shadow pass writes shadow_depth as a COLOR
       target with CB_INFO.linear_general=1. We sample it as DisplayLinearAligned
       (tile_mode=8) because tile_mode=31 (DisplayLinearGeneral) crashes shadPS4
       in image_info.cpp:200 UpdateSize — ArrayLinearGeneral isn't handled in
       that switch. Both "linear" array modes are row-major pixel arrays; the
       aligned variant just requires pitch alignment which 512 satisfies. */
    if (shadow_depth) {
        /* Shadow T# = RGBA8. Matches the shadow DCB's CB0 write format (RGBA8)
           so shadPS4's texture_cache reuses a SINGLE Vulkan image for both the
           shadow-pass write and the floor-PS read. If we used R32_FLOAT here
           while the write path is D32_SFLOAT (old approach), Vulkan's depth/
           color format incompatibility (§39.1.6) would force shadPS4 to create
           two separate images — writes to one would not be visible in the other.
           build_tsharp sets format=10 (8_8_8_8 UNORM), tile_mode=8 which matches
           build_tsharp default and the new shadow CB0 write path. */
        build_tsharp(desc + 40, shadow_depth, SHADOW_W, SHADOW_H);
    }

    /* Floor PBR T#s at desc[64..71] (albedo) and desc[72..79] (normal).
       BMP files are LINEAR pixel arrays. Use tile_mode=8 (DisplayLinearAligned)
       per shadPS4's TileMode enum in tiling.h. Earlier I used 13 (Thin1DThin, a
       TILED layout) which caused shadPS4 to de-tile nonexistent tiles — that's
       why the texture looked streaked/washed.
       build_tsharp already sets 8 so no patching needed. */
    build_tsharp(desc + 64, floor_albedo_tex, floor_tex_w, floor_tex_h);
    /* Floor normal map at desc[72..79] — procedural 64x64 tangent-space normals */
    build_tsharp(desc + 72, floor_normal_tex, floor_nrm_w, floor_nrm_h);

    /* VB layout:
       - Main region: [ident][BG sun][BG verts][MVP][sun dir][cube verts]
       - Shadow uses a separate VB: [light_MVP @ 0x00][pad @ 0x40][vert copy @ 0x50].
         For huge models, we skip shadow VB allocation and disable shadow entirely. */
    /* Shadow VB layout: [light_MVP @ 0x00][pad @ 0x40][cube verts @ 0x50]
       [floor verts @ cube_end]. Both cube AND floor render into shadow map
       so the curved floor self-shadows when the curve creates ridges
       blocking the sun. */
    unsigned long shadow_vb_size = 0x50
                                 + (unsigned long)CUBE_VERTS * VERT_STRIDE
                                 + (unsigned long)FLOOR_VERTS * VERT_STRIDE
                                 + 256;
    void *shadow_vb = gpu_alloc(shadow_vb_size, 0x1000);
    if (!shadow_vb) shadow_depth = 0;
    void *vb=gpu_alloc(VERT_BUF_SIZE + 256, 0x1000);
    build_static_vb((float*)vb,
                    (const unsigned char*)floor_disp_tex,
                    floor_disp_w, floor_disp_h);  // uploaded ONCE
    uint32_t vb_v[4], bg_v[4], shadow_vb_v[4], shadow_floor_v[4], floor_v[4];
    build_vsharp(vb_v, vb, VERT_BUF_SIZE);
    build_vsharp(bg_v, vb, VERT_BUF_SIZE);
    build_vsharp(shadow_vb_v, shadow_vb, (uint32_t)shadow_vb_size);
    /* Shadow-floor V# points at the same shadow_vb (same light_MVP at offset 0)
       but its vertex data starts AFTER the cube verts. shadPS4 VS reads the
       MVP from V#+0 and verts from V#+0x50 (constant), so we offset the V#
       base so that V#+0x50 lands on the floor vert region.
       Effective base = shadow_vb + (cube_size). MVP is at shadow_vb+0; we
       can't have MVP at offset 0 from a different base unless we duplicate it.
       Solution: copy light_MVP into a SECOND header at the start of the
       floor region (so floor base = shadow_vb + cube_size has MVP at +0). */
    unsigned long shadow_floor_base = 0x50
                                    + (unsigned long)CUBE_VERTS * VERT_STRIDE;
    /* Round up to 0x80 alignment so the V# load with offset 0x50 doesn't
       cross a cache line boundary in unexpected ways. */
    if (shadow_floor_base & 0x7F) shadow_floor_base = (shadow_floor_base + 0x7F) & ~0x7Ful;
    /* shadow_floor_v base = shadow_vb + shadow_floor_base - 0x50, so:
        - V# + 0x00 = shadow_vb + shadow_floor_base - 0x50  (MVP — needs duplicate)
        - V# + 0x50 = shadow_vb + shadow_floor_base        (floor verts)
       That means MVP must be duplicated 0x50 bytes BEFORE the floor verts.
       Easier: place MVP duplicate at offset (shadow_floor_base - 0x50) and
       point V# at (shadow_vb + shadow_floor_base - 0x50). */
    unsigned long shadow_floor_mvp_off = shadow_floor_base - 0x50;
    build_vsharp(shadow_floor_v,
                 (char*)shadow_vb + shadow_floor_mvp_off,
                 (uint32_t)(shadow_vb_size - shadow_floor_mvp_off));
    /* Floor V#: base at vb+FLOOR_MVP_OFF. Size covers MVP (64B) + padding + verts.
       Must be >= 80 + FLOOR_VERTS*VERT_STRIDE = 80 + 384*48 = 18512 bytes. */
    build_vsharp(floor_v, (char*)vb + FLOOR_MVP_OFF,
                 (uint32_t)(VERT_BUF_SIZE - FLOOR_MVP_OFF));
    /* Copy cube verts into shadow VB at offset 0x50 */
    if (shadow_vb) {
        my_memcpy((char*)shadow_vb + 0x50,
                  (char*)vb + CUBE_DATA_OFF,
                  CUBE_VERTS * VERT_STRIDE);
        /* Copy floor verts into shadow VB at shadow_floor_base */
        my_memcpy((char*)shadow_vb + shadow_floor_base,
                  (char*)vb + FLOOR_DATA_OFF,
                  FLOOR_VERTS * VERT_STRIDE);
    }

    // (file loading disabled for testing)




    void *vs=gpu_alloc(sizeof(vs_shader_binary)+256,0x1000);
    void *vs_shadow=gpu_alloc(sizeof(vs_shadow_binary)+256,0x1000);
    void *ps=gpu_alloc(sizeof(ps_shader_binary)+256,0x1000);
    my_memcpy(vs,vs_shader_binary,sizeof(vs_shader_binary));
    my_memcpy(vs_shadow,vs_shadow_binary,sizeof(vs_shadow_binary));
    my_memcpy(ps,ps_shader_binary,sizeof(ps_shader_binary));

    /* Every shader bound in a draw must live in GPU-accessible memory — the GPU
       fetches code from the PGM_LO/HI address. Binding straight from the .rodata
       arrays (CPU-only ELF memory) faults the GPU MMU on real hardware and hangs
       (shadPS4 reads guest memory via its cache, so it never faulted there). */
    #define UPLOAD_SHADER(dst, src) \
        void *dst = gpu_alloc(sizeof(src)+256, 0x1000); \
        my_memcpy(dst, src, sizeof(src));
    UPLOAD_SHADER(ps_dark_gpu,          ps_dark_binary);
    UPLOAD_SHADER(ps_floor_gpu,         ps_floor_binary);
    UPLOAD_SHADER(ps_shadow_gpu,        ps_shadow_binary);
    UPLOAD_SHADER(ps_shadow_clear_gpu,  ps_shadow_clear_binary);
    UPLOAD_SHADER(vs_fulltri_gpu,       vs_fulltri_binary);
    UPLOAD_SHADER(vs_bg_gpu,            vs_bg_binary);
    UPLOAD_SHADER(vs_ftload_gpu,        vs_ftload_binary);
    UPLOAD_SHADER(ps_magenta_gpu,       ps_magenta_binary);
    UPLOAD_SHADER(ps_skyclean_gpu,      ps_skyclean_binary);
    UPLOAD_SHADER(ps_blue_gpu,          ps_blue_binary);
    UPLOAD_SHADER(ps_grad_gpu,          ps_grad_binary);
    #undef UPLOAD_SHADER
    g_vs_fulltri_gpu = vs_fulltri_gpu;
    g_vs_bg_gpu = vs_bg_gpu;
    g_vs_ftload_gpu = vs_ftload_gpu;
    g_ps_magenta_gpu = ps_magenta_gpu;
    g_ps_skyclean_gpu = ps_skyclean_gpu;
    g_ps_grad_gpu = ps_grad_gpu;

    uint32_t *dcb_mem[NUM_FRAMES];
    uint32_t *shadow_dcb_mem[NUM_FRAMES];
    for (int i=0;i<NUM_FRAMES;i++) {
        dcb_mem[i]=(uint32_t*)gpu_alloc(DCB_SIZE,0x10000);
        shadow_dcb_mem[i]=(uint32_t*)gpu_alloc(DCB_SIZE,0x10000);
    }
    volatile uint32_t *fence=(volatile uint32_t*)gpu_alloc(0x1000,0x1000);
    volatile uint32_t *shadow_fence=(volatile uint32_t*)gpu_alloc(0x1000,0x1000);
    *fence=0;
    *shadow_fence=0;



    int model_verts = CUBE_VERTS;
    uint32_t *g_ib = 0;
    int g_num_idx = 0;
    int g_indexed = 0;
    int g_shadow_verts = CUBE_VERTS; /* always non-indexed draw count for shadow DCB */
    int g_shadow_ready = (shadow_vb != 0) ? 1 : 0;  /* cube data already copied */
    unsigned long g_vb_total = VERT_BUF_SIZE;

    /* Model AABB in world space — used to size light-space ortho for shadow pass.
       Default to built-in cube (±0.4). Updated after OBJ load. */
    float g_model_cx = 0.0f, g_model_cy = 0.0f, g_model_cz = 0.0f;
    float g_model_radius = 0.8f;  /* cube radius ~0.7, round up */

    /* --- Load 3D model using obj_loader.h --- */
    {
        static const char *obj_paths[] = {
            "/data/CUBETST00/model.obj",
            "/data/CUBETST00/mesh.obj",
            "/data/CUBETST00/object.obj",
            "/data/CUBETST00/scene.obj",
            "/data/CUBETST00/bugatti.obj",
            "/data/CUBETST00/car.obj",
            0
        };
        struct LoadCtx load_ctx;
        load_ctx.vs = vs; load_ctx.ps = ps;
        load_ctx.vb_v = vb_v; load_ctx.bg_v = bg_v;
        load_ctx.vb = vb; load_ctx.desc = desc;
        load_ctx.fb0 = fb[0]; load_ctx.fb1 = fb[1];
        load_ctx.depth = depth; load_ctx.fence = fence;
        load_ctx.video = video; load_ctx.pm4_buf = dcb_mem[0];
        load_ctx.flip_idx = 0;
        load_ctx.ps_blue = ps_blue_gpu;
        load_ctx.ps_dark = ps_dark_gpu;
        ObjMesh mesh;
        int loaded = 0;
        for (int pi = 0; obj_paths[pi] && !loaded; pi++) {
            int err = -1;
            const char *ext = obj_paths[pi];
            while (*ext) ext++;
            /* Find extension */
            const char *dot = ext;
            while (dot > obj_paths[pi] && *dot != '.') dot--;
            if (dot[1]=='o' && dot[2]=='b' && dot[3]=='j') {
                err = obj_load_file(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
            } else if (dot[1]=='s' && dot[2]=='t' && dot[3]=='l') {
                err = stl_load_binary(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
            } else if (dot[1]=='p' && dot[2]=='l' && dot[3]=='y') {
                err = ply_load_file(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
            }
            if (err == 0 && mesh.num_verts > 0) {
                printf("Loaded %s: %d verts, %d tris (decimate=%d)\n",
                       obj_paths[pi], mesh.num_verts, mesh.num_tris, mesh.indexed);
                /* Copy BG quad data into mesh.vb_base */
                my_memcpy(mesh.vb_base, vb, OBJ_DATA_OFF);
                /* Reset BG normals to zero after loading (loading_progress sets z=0.25) */
                { float *bgr = (float*)((char*)mesh.vb_base + BG_DATA_OFF);
                  for (int bi=0;bi<6;bi++) { bgr[bi*12+4]=0;bgr[bi*12+5]=0;bgr[bi*12+6]=0; } }
                vb = mesh.vb_base;
                if (mesh.indexed) {
                    model_verts = mesh.num_verts; /* unique verts for V# */
                    g_ib = mesh.ib_base;
                    g_num_idx = mesh.num_indices;
                    g_indexed = 1;
                } else {
                    model_verts = mesh.num_verts;
                }
                /* Update V# descriptors for new buffer size */
                unsigned long new_total = OBJ_DATA_OFF + (unsigned long)mesh.num_verts * VERT_STRIDE;
                g_vb_total = new_total;
                build_vsharp(vb_v, vb, (uint32_t)new_total);
                build_vsharp(bg_v, vb, (uint32_t)new_total);
                /* Shadow VB: allocate a parallel buffer with verts copied into it.
                   Layout matches cube-case: [light_MVP][pad][verts]. If alloc fails
                   (huge model), set g_shadow_ready=0 to disable shadow pass cleanly. */
                g_shadow_verts = mesh.num_verts;
                g_shadow_ready = 0;
                {
                    unsigned long new_shadow_size = 0x50 + (unsigned long)mesh.num_verts * VERT_STRIDE + 256;
                    /* Cap at 256MB — huge models skip shadow */
                    if (new_shadow_size <= 0x10000000UL) {
                        void *new_shadow_vb = gpu_alloc(new_shadow_size, 0x1000);
                        if (new_shadow_vb) {
                            shadow_vb = new_shadow_vb;
                            my_memcpy((char*)shadow_vb + 0x50,
                                      (char*)vb + OBJ_DATA_OFF,
                                      (unsigned long)mesh.num_verts * VERT_STRIDE);
                            build_vsharp(shadow_vb_v, shadow_vb, (uint32_t)new_shadow_size);
                            g_shadow_ready = 1;
                        }
                    }
                }

                /* Compute model AABB for light-space ortho sizing. Positions at
                   offset 0,4,8 of each 48-byte vertex in vb + OBJ_DATA_OFF. */
                {
                    float *vp = (float*)((char*)vb + OBJ_DATA_OFF);
                    float minx = vp[0], miny = vp[1], minz = vp[2];
                    float maxx = minx, maxy = miny, maxz = minz;
                    for (int vi = 1; vi < mesh.num_verts; vi++) {
                        float x = vp[vi*12+0], y = vp[vi*12+1], z = vp[vi*12+2];
                        if (x < minx) minx = x; if (x > maxx) maxx = x;
                        if (y < miny) miny = y; if (y > maxy) maxy = y;
                        if (z < minz) minz = z; if (z > maxz) maxz = z;
                    }
                    g_model_cx = (minx + maxx) * 0.5f;
                    g_model_cy = (miny + maxy) * 0.5f;
                    g_model_cz = (minz + maxz) * 0.5f;
                    float dx = maxx - minx, dy = maxy - miny, dz = maxz - minz;
                    float diag = my_sqrt(dx*dx + dy*dy + dz*dz);
                    g_model_radius = diag * 0.5f;
                    if (g_model_radius < 0.1f) g_model_radius = 0.1f;
                }
                loaded = 1;
            }
        }
        if (!loaded) {
            /* Restore cube verts and BG — loading_progress may have overwritten them
               during failed OBJ attempts (progress-bar geometry written at CUBE_DATA_OFF
               plus BG tweaks). Rebuild the static VB to recover the original cube. */
            build_static_vb((float*)vb,
                            (const unsigned char*)floor_disp_tex,
                            floor_disp_w, floor_disp_h);
            printf("No model found, using built-in cube.\n");
        }
    }

    printf("VB ready: %d model verts. MVP on GPU. Running.\n", model_verts);

    // --- Gamepad init ---
    // 1:1 PS4 sequence: init userservice → get userId → init pad → open pad
    sceUserServiceInitialize(0);
    int userId = 0;
    sceUserServiceGetInitialUser(&userId);
    scePadInit();
    int pad_handle = scePadOpen(userId, 0, 0, 0);
    // pad_handle < 0 = error (PS4 error codes are negative)
    // pad_handle >= 0 = valid handle (0 is valid on real PS4)
    if (pad_handle < 0) {
        printf("scePadOpen failed: 0x%x, userId=%d\n", pad_handle, userId);
    }
    struct OrbisPadData pad;
    my_memset(&pad, 0, sizeof(pad));
    pad.lx = 128;
    pad.ly = 128;
    pad.rx = 128;
    pad.ry = 128;

    // Camera state
    /* Default view = sunset.
       Camera looks WEST (yaw = -π/2 = -1.5708), pitch level (0).
       Sun sits low on western horizon at sun_angle ≈ π (slightly before so
       the disc is clearly above the horizon — golden-hour position).
       Camera Y = original eye height (0.15) + cube_world_y offset (0.4) =
       0.55 — same vertical lift the cube got, so eye-level is at cube
       center. cam_x = 3.5 puts the camera far enough back that the whole
       tumbling cube (radius ~0.7) fits in the 35° FOV with margin. */
    float cam_yaw = -1.5708f, cam_pitch = 0.0f;
    float cam_x = 3.5f, cam_y = 0.55f, cam_z = 0.8f;
    float move_speed = 0.02f;
    float vel_y = 0;           /* vertical velocity (gravity/jump) */
    float ground_y = -0.05f;   /* ground level */
    float eye_height = 0.15f;  /* camera height above ground */
    int on_ground = 1;
    int sprint = 0;
    float gravity = -0.004f;
    float jump_vel = 0.06f;
    int auto_spin = 0;  // 1=spinning camera (no longer Start-toggleable; reserved)
    int cube_rotation_enabled = 1;  // Start button toggles this (default: spinning)
    float cube_angle_y = 0.0f;       // accumulator (advances only when enabled)
    float cube_angle_x = 0.0f;
    float sun_angle = 3.0f;  // start at sunset: sun low on western horizon
    float sun_speed = 0.0027f;  // 40% slower than 0.0045 (= 76% slower than original 0.01125)
    uint32_t prev_buttons = 0;

    uint32_t frame=0,fv=1;
    for (;;) {
        int bi=frame%NUM_FRAMES;

        // Read gamepad
        if (pad_handle >= 0)
            scePadRead(pad_handle, &pad, 1);

        // Button edge detection (pressed this frame, not last)
        uint32_t pressed = pad.buttons & ~prev_buttons;
        prev_buttons = pad.buttons;

        // Cross: toggle auto-spin
        /* Cross = jump */
        if ((pressed & PAD_CROSS) && on_ground) {
            vel_y = jump_vel;
            on_ground = 0;
        }
        /* Square = sprint toggle */
        if (pressed & 0x8000) sprint = !sprint;
        /* Options/Start = toggle cube rotation pause/resume */
        if (pressed & PAD_OPTIONS) cube_rotation_enabled = !cube_rotation_enabled;
        // Triangle: reset camera to default sunset view
        if (pressed & PAD_TRI) { cam_yaw=-1.5708f; cam_pitch=0; cam_x=3.5f; cam_y=0.55f; cam_z=0.8f; vel_y=0; on_ground=1; sprint=0; }

        // L1/R1: zoom
        if (pad.buttons & PAD_R1) cam_y += move_speed;
        if (pad.buttons & PAD_L1) cam_y -= move_speed;

        // Left stick: orbit camera
        float lx = ((float)pad.lx - 128.0f) / 128.0f;
        float ly = ((float)pad.ly - 128.0f) / 128.0f;
        /* Left stick: move forward/back + strafe */
        float spd = sprint ? move_speed * 3.0f : move_speed;
        if (ly > 0.15f || ly < -0.15f) {
            cam_x += my_sin(cam_yaw) * (-ly) * spd;
            cam_z -= my_cos(cam_yaw) * (-ly) * spd;
        }
        if (lx > 0.15f || lx < -0.15f) {
            cam_x += my_cos(cam_yaw) * lx * spd;
            cam_z += my_sin(cam_yaw) * lx * spd;
        }

        // Right stick: pan camera
        float rx = ((float)pad.rx - 128.0f) / 128.0f;
        float ry = ((float)pad.ry - 128.0f) / 128.0f;
        /* Right stick: look around */
        if (rx > 0.15f || rx < -0.15f) cam_yaw += rx * 0.04f;
        if (ry > 0.15f || ry < -0.15f) cam_pitch += ry * 0.03f;
        if (cam_pitch > 1.5f) cam_pitch = 1.5f;
        if (cam_pitch < -1.5f) cam_pitch = -1.5f;

        // D-pad: fine rotation
        /* D-pad: speed control + precise movement */
        if (pad.buttons & PAD_UP)    move_speed *= 1.02f;
        if (pad.buttons & PAD_DOWN)  move_speed *= 0.98f;
        if (move_speed < 0.001f) move_speed = 0.001f;
        if (move_speed > 0.5f) move_speed = 0.5f;
        if (pad.buttons & PAD_LEFT)  cam_yaw -= 0.02f;
        if (pad.buttons & PAD_RIGHT) cam_yaw += 0.02f;

        // Auto-spin
        if (auto_spin) cam_yaw += 0.02f;

        /* Sun controls: L3 toggles auto-orbit, L2/R2 manual rotation */
        if (pressed & 0x0002) sun_speed = (sun_speed > 0.001f) ? 0.0f : 0.0027f; /* L3 toggle */
        if (pad.buttons & 0x0100) sun_angle -= 0.00432f; /* L2 held = sun left, 40% slower */
        if (pad.buttons & 0x0200) sun_angle += 0.00432f; /* R2 held = sun right */
        sun_angle += sun_speed;

        /* BG lighting handled by PS via light direction */

        /* === Per-frame cube rotation ===
           The cube has been static at world origin since the first build because no
           rotation was ever applied. Rotate around Y axis (vertical) so the user
           can see how the shadow shape changes with cube orientation — important
           Standard cube test rotation: classic two-axis demo spin (rotate
           around Y at one rate, around X at a slightly different rate) so
           every face of the cube becomes visible over time. Same rotation
           is applied to positions AND normals so the per-face Lambert
           lighting in the cube PS tracks the geometry. */
        {
            /* Cube rotation angles advance ONLY when cube_rotation_enabled.
               Start button (PAD_OPTIONS) toggles the flag; when disabled the
               accumulators freeze, so the cube holds its current orientation
               until rotation is re-enabled. */
            if (cube_rotation_enabled) {
                cube_angle_y += 0.013f;  /* primary Y spin rate */
                cube_angle_x += 0.007f;  /* slower X spin rate */
            }
            float angle_y = cube_angle_y;
            float angle_x = cube_angle_x;
            float cy_ = my_cos(angle_y), sy_ = my_sin(angle_y);
            float cx_ = my_cos(angle_x), sx_ = my_sin(angle_x);

            /* Rotate (vx,vy,vz) around Y first, then X.
                 Y: (x,y,z) → (x*cy + z*sy, y, -x*sy + z*cy)
                 X: (x,y,z) → (x, y*cx - z*sx, y*sx + z*cx)
               Combined into the R##() macro. */
            #define ROTATE_XY(rx, ry, rz, vx, vy, vz) do { \
                float _yx = (vx)*cy_ + (vz)*sy_;            \
                float _yy = (vy);                           \
                float _yz = -(vx)*sy_ + (vz)*cy_;           \
                rx = _yx;                                   \
                ry = _yy*cx_ - _yz*sx_;                     \
                rz = _yy*sx_ + _yz*cx_;                     \
            } while(0)

            /* Walk the same triangle list as build_static_vb. Update positions
               AND normals — normals must rotate with the geometry or per-face
               lambert lighting in the cube PS gets stuck (e.g. the +Z face's
               normal.z=+1 stays at +1 even after rotation, so that face stays
               lit relative to sun.z regardless of where it has rotated to). */
            static const float face_normals_local[6][3] = {
                {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
            };
            /* World-space Y translation applied AFTER rotation so the cube
               spins around its own center but sits ABOVE the floor (Y=-0.5).
               cube_pos[] spans Y±0.4 about its local origin; with full 2-axis
               tumble, the lowest diagonal corner reaches ~0.69 below center.
               Lifting the center to Y=0.4 puts the lowest corner at ~Y=-0.29,
               clear of the floor. */
            const float cube_world_y = 0.4f;
            float *cb = (float*)((char*)vb + CUBE_DATA_OFF);
            for (int tri = 0; tri < 12; tri++) {
                int face = tri / 2;
                int t = tri % 2;
                /* Pre-rotate this face's normal once for all 3 verts of the tri */
                float rnx, rny, rnz;
                ROTATE_XY(rnx, rny, rnz,
                          face_normals_local[face][0],
                          face_normals_local[face][1],
                          face_normals_local[face][2]);
                for (int v = 0; v < 3; v++) {
                    int vi = face_idx[tri][v];
                    int idx = (tri*3 + v) * 12;
                    /* Position: rotate about origin, then translate up. */
                    float rx, ry, rz;
                    ROTATE_XY(rx, ry, rz,
                              cube_pos[vi][0], cube_pos[vi][1], cube_pos[vi][2]);
                    cb[idx]   = rx;
                    cb[idx+1] = ry + cube_world_y;
                    cb[idx+2] = rz;
                    cb[idx+3] = 1.0f;
                    /* Normal (unaffected by translation) */
                    cb[idx+4] = rnx;
                    cb[idx+5] = rny;
                    cb[idx+6] = rnz;
                    cb[idx+7] = 0.0f;
                    /* UVs untouched (already set by build_static_vb) */
                    (void)t;
                }
            }

            /* Mirror the rotated cube data into the shadow VB so the shadow pass
               draws the SAME geometry. shadow_vb layout: [light_MVP @ 0][pad][cube verts @ 0x50]. */
            if (shadow_vb && g_shadow_ready) {
                my_memcpy((char*)shadow_vb + 0x50,
                          (char*)vb + CUBE_DATA_OFF,
                          CUBE_VERTS * VERT_STRIDE);
            }

            #undef ROTATE_XY
        }

        build_mvp((float*)((char*)vb+MVP_OFF), cam_yaw, cam_pitch, cam_x, cam_y, cam_z);

        /* Mirror MVP to FLOOR_MVP_OFF so the floor draw (V#-base = vb+FLOOR_MVP_OFF)
           can read MVP at V#+0 and floor vertex data at V#+80, matching VS convention.
           Copy 64 bytes (16 floats = 4x4 matrix). */
        my_memcpy((char*)vb + FLOOR_MVP_OFF,
                  (char*)vb + MVP_OFF, 64);

        /* Build light-space MVP at LIGHT_MVP_OFF for shadow pass.
           Orthographic projection from sun looking at origin.
           sun_x, sun_y, sun_z are the computed sun direction below.
           Build it after sun direction is known. */

        /* Compute sun direction.

           REAL-WORLD SUN ARC (mid-latitude northern observer):
              sunrise (h=0):    sun in EAST  = +X, on horizon
              noon    (h=π/2):  sun in SOUTH = -Z (into scene), high in sky
              sunset  (h=π):    sun in WEST  = -X, on horizon
              midnight(h=3π/2): sun BELOW horizon, opposite side

           World axis convention (this codebase):
              +X = east, -X = west
              +Y = up, -Y = down
              +Z = behind camera (north), -Z = in front of camera (south)
              Camera starts at (0, 0.15, +3.5) looking down -Z toward origin.
              Cube sits in front of camera at -Z, which is "south".

           Construction: sun moves on a great circle whose plane is tilted
           from vertical by `tilt` radians (the observer's latitude offset
           from the equator). At hour-angle h:
              raw_x = cos(h)         east-west sweep  (full ±1 range)
              raw_y_up = sin(h)      vertical (raw)
           Then rotate around the X axis so the plane tilts SOUTH (toward -Z):
              sun_y =  raw_y_up * cos(tilt)
              sun_z = -raw_y_up * sin(tilt)   (negative — toward -Z = south)

           Result: sunrise on east horizon, noon high south, sunset on west
           horizon — visibly OPPOSITE sides of the sky. Peak elevation is
           cos(tilt), so the sun never reaches zenith. */
        float sun_y, sun_x, sun_z;
        {
            const float tilt = 0.55f;                /* ~31° latitude tilt */
            const float ct = my_cos(tilt);
            const float st = my_sin(tilt);
            float raw_x = my_cos(sun_angle);         /* east-west: +1 dawn, -1 dusk */
            float raw_y_up = my_sin(sun_angle);      /* vertical raw: +1 noon, -1 midnight */
            sun_x = raw_x;                            /* east-west position */
            sun_y = raw_y_up * ct;                    /* vertical (attenuated by tilt) */
            sun_z = -raw_y_up * st;                   /* southward (-Z) when sun is up */
        }

        /* Swap to moon (anti-sun) at night so the active light is always
           ABOVE the horizon. Reuse the sun_x/y/z variables so the rest of
           the pipeline (light_MVP construction, shadow DCB) just works.
           Save the ORIGINAL sun elevation before flipping — the sky color
           block below still needs to know whether it's actually day or night
           (sun_y becomes >=0 always after the swap).

           Sun magnitude ramp scales the Lambert dot product in cube/floor PS:
             night                   → 0.60  (moon, 20% brighter than old 0.50)
             horizon (sun_y=0)       → 0.60  (matches night for seamless transition)
             rising 0 → sin(20°)=0.342 → linear ramp 0.60 → 1.20
             above 20° elevation     → 1.20  (peak day, +20% boost over old 1.00)

           Threshold lowered to 20° so the sun stays at full peak brightness
           for nearly all of the day. Darkening only kicks in during the
           final ~1-2 hours before sunset (or first 1-2 after sunrise). */
        float orig_sun_y = sun_y;
        int is_night = (sun_y < 0.0f);
        if (is_night) {
            sun_x = -sun_x;
            sun_y = -sun_y;
            sun_z = -sun_z;
            /* Night: 0.69 (was 0.60, +15% brighter as requested) */
            sun_x *= 0.69f; sun_y *= 0.69f; sun_z *= 0.69f;
        } else {
            /* Day/twilight: ramp 0.98 at horizon → 1.38 at sun_y >= sin(8.6°) = 0.15.
               Boosted 15% from 0.85→1.20 to 0.98→1.38. Dimming only kicks in at
               the LAST ~8° of sun elevation. */
            float mag = 0.98f + 0.40f * (sun_y / 0.15f);
            if (mag > 1.38f) mag = 1.38f;
            sun_x *= mag; sun_y *= mag; sun_z *= mag;
        }
        {
            float *sun = (float*)(desc + 12);
            sun[0] = sun_x; sun[1] = sun_y; sun[2] = sun_z; sun[3] = 0;
        }

        /* Build light-space MVP (orthographic projection looking from sun toward origin).
           We use view direction = -sun_dir, target = origin, scene radius ~5 units.
           Since we just need a working matrix, use a simple axis-aligned ortho that
           projects along sun direction. Scene extends ~±5 in each axis → ortho size 10.

           Light-space matrix layout (row-major, same as camera MVP):
           - X axis: perpendicular to sun, world up cross sun
           - Y axis: world up reprojected
           - Z axis: sun direction (view direction)
           - Then orthographic scale to [-1,1] NDC */
        {
            float *lm = (float*)shadow_vb;
            /* Zero out */
            for (int i = 0; i < 16; i++) lm[i] = 0.0f;

            /* Light forward = -sun_dir (looking FROM sun TO origin) */
            float lfx = -sun_x, lfy = -sun_y, lfz = -sun_z;
            float flen = my_sqrt(lfx*lfx + lfy*lfy + lfz*lfz);
            if (flen < 0.001f) flen = 1.0f;
            lfx /= flen; lfy /= flen; lfz /= flen;

            /* Light right = normalize(cross(world_up, light_forward))
               world_up = (0, 1, 0) */
            float lrx = 1.0f * lfz - 0.0f * lfy;  /* up.y*lf.z - up.z*lf.y = lfz */
            float lry = 0.0f * lfx - 0.0f * lfz;  /* up.z*lf.x - up.x*lf.z = 0 */
            float lrz = 0.0f * lfy - 1.0f * lfx;  /* up.x*lf.y - up.y*lf.x = -lfx */
            float rlen = my_sqrt(lrx*lrx + lry*lry + lrz*lrz);
            if (rlen < 0.001f) { lrx = 1; lry = 0; lrz = 0; rlen = 1; }
            lrx /= rlen; lry /= rlen; lrz /= rlen;

            /* Light up = cross(light_forward, light_right) */
            float lux = lfy * lrz - lfz * lry;
            float luy = lfz * lrx - lfx * lrz;
            float luz = lfx * lry - lfy * lrx;

            /* Orthographic bounds. Frustum sized for cube shadow. */
            float frustum_radius = 8.0f * g_model_radius;
            float sxy = 1.0f / frustum_radius;
            /* Z range tight around the cube for precision. */
            float sz_ = 1.0f / (16.0f * g_model_radius);

            /* Place light back to keep cube inside ortho Z range. */
            float dist = 8.0f * g_model_radius;
            float lpx = g_model_cx - lfx * dist;
            float lpy = g_model_cy - lfy * dist;
            float lpz = g_model_cz - lfz * dist;

            /* View matrix rows: [right | up | forward | translation] */
            /* MVP row-major, position translation = -dot(axis, light_pos) */
            lm[0] = lrx * sxy;  lm[1] = lry * sxy;  lm[2] = lrz * sxy;
            lm[3] = -(lrx*lpx + lry*lpy + lrz*lpz) * sxy;
            lm[4] = lux * sxy;  lm[5] = luy * sxy;  lm[6] = luz * sxy;
            lm[7] = -(lux*lpx + luy*lpy + luz*lpz) * sxy;
            lm[8] = lfx * sz_;  lm[9] = lfy * sz_;  lm[10] = lfz * sz_;
            lm[11] = -(lfx*lpx + lfy*lpy + lfz*lpz) * sz_;  /* NO +0.5 — translation already centers depth */
            lm[12] = 0; lm[13] = 0; lm[14] = 0; lm[15] = 1;  /* ortho: w=1 */

            /* Mirror the same matrix into desc[48..63] (byte offset 192 = 0xC0).
               Main PS loads this via s_load_dwordx16 s[32:47], s[0:1], 0x30
               and uses it to project each fragment's world-space position into
               light-clip space for projective shadow sampling. */
            {
                float *dm = (float*)(desc + 48);
                for (int i = 0; i < 16; i++) dm[i] = lm[i];
            }
            /* Mirror light_MVP into the shadow_vb's floor-draw header so the
               floor shadow draw reads the same matrix at V#+0. */
            if (shadow_vb) {
                float *lm2 = (float*)((char*)shadow_vb + shadow_floor_mvp_off);
                for (int i = 0; i < 16; i++) lm2[i] = lm[i];
            }
        }

        /* Dynamic light color based on elevation and day/night state.

           This light_color ACTIVELY tints cube and floor output (both PS
           shaders multiply final RGB by light_color from desc[32]). Sky PS
           also reads it for moon/sun disc color.

           Threshold uses orig_sun_y (the RAW unscaled vertical component,
           ranges 0..cos(tilt) ≈ 0..0.852 over a full day). Matches the
           magnitude ramp's sin(8.6°)=0.15 threshold so warm tinting and
           dimming both kick in only at the last ~8° of sun elevation —
           the actual sunset/sunrise window.

           DAY (orig_sun_y > 0.15):   neutral white  (1.00, 1.00, 1.00)
           SUNSET/SUNRISE ramp:        white → warm amber (1.00, 0.64, 0.44)
           NIGHT:                      desaturated cool blue (0.52, 0.64, 0.84) */
        float light_r, light_g, light_b;
        if (is_night) {
            /* Moon: desaturated cool blue */
            light_r = 0.52f; light_g = 0.64f; light_b = 0.84f;
        } else if (orig_sun_y > 0.15f) {
            /* Full daylight: neutral white */
            light_r = 1.00f; light_g = 1.00f; light_b = 1.00f;
        } else {
            /* Sunrise/sunset ramp: amber at horizon → white at orig_sun_y >= 0.15.
               k = 0 at horizon, 1 at plateau edge */
            float k = orig_sun_y / 0.15f;
            /* Horizon (k=0): amber (1.00, 0.64, 0.44)
               Plateau (k=1): white (1.00, 1.00, 1.00) */
            light_r = 1.00f;
            light_g = 0.64f + 0.36f * k;
            light_b = 0.44f + 0.56f * k;
        }
        {
            float *lc = (float*)(desc + 32);
            lc[0] = light_r; lc[1] = light_g; lc[2] = light_b; lc[3] = 1.0f;
        }

        /* Dynamic sky colors based on REAL sun elevation (orig_sun_y, before
           the moon swap). The sun_y variable was negated for the night-light
           swap above, so it's >=0 always now and would mis-classify night as
           day. Use orig_sun_y to keep the sky correctly dark at night even
           while the moon lights up the scene. */
        float zr, zg, zb, hr, hg, hb;
        if (orig_sun_y > 0.3f) {
            /* Full day */
            zr = 0.10f; zg = 0.25f; zb = 0.60f;
            hr = 0.65f; hg = 0.82f; hb = 0.95f;
        } else if (orig_sun_y > -0.2f) {
            /* Twilight/sunset: fade from day to night with orange horizon */
            float k = (orig_sun_y + 0.2f) / 0.5f;    /* 0 at night-edge, 1 at full-day edge */
            if (k < 0) k = 0; if (k > 1) k = 1;
            /* Zenith darkens */
            zr = 0.04f + 0.06f * k;
            zg = 0.06f + 0.19f * k;
            zb = 0.18f + 0.42f * k;
            /* Horizon warm orange near sunset */
            float warm = 1.0f - k;                   /* 1 at night-edge */
            hr = 0.65f + 0.30f * warm;               /* peaks orange */
            hg = 0.35f + 0.47f * (1.0f - warm*warm);
            hb = 0.25f + 0.70f * k;
        } else {
            /* Night sky: deep blue, slightly brighter at horizon */
            zr = 0.01f; zg = 0.02f; zb = 0.06f;
            hr = 0.03f; hg = 0.04f; hb = 0.10f;
        }
        {
            float *sz = (float*)(desc + 24);
            sz[0] = zr; sz[1] = zg; sz[2] = zb; sz[3] = 0;
            float *sh = (float*)(desc + 28);
            sh[0] = hr; sh[1] = hg; sh[2] = hb; sh[3] = 0;
        }

        /* Project sun to NDC for sky disc rendering → desc[16:19] */
        {
            float *mvp = (float*)((char*)vb + MVP_OFF);
            float *sun = (float*)(desc + 12);
            float sx = sun[0]*100, sy2 = sun[1]*100, sz = sun[2]*100;
            float cx = mvp[0]*sx + mvp[1]*sy2 + mvp[2]*sz + mvp[3];
            float cy = mvp[4]*sx + mvp[5]*sy2 + mvp[6]*sz + mvp[7];
            float cw = mvp[12]*sx + mvp[13]*sy2 + mvp[14]*sz + mvp[15];
            float *sd = (float*)(desc + 16);
            /* Disc radius: SUN doubled (0.006 → 0.012 = 200% of original).
               MOON at 75% of new sun size = 0.009.
               sd[2] is the radius value sampled by the sky PS for the bright
               disc blend. */
            float disc_radius = is_night ? 0.009f : 0.012f;
            if (cw > 0.01f) {
                sd[0] = cx / cw; sd[1] = cy / cw;
                sd[2] = disc_radius; sd[3] = 1.0f;
            } else {
                sd[0] = 99.0f; sd[1] = 99.0f;
                sd[2] = disc_radius; sd[3] = 0.0f;
            }
        }

        /* Build main DCB (samples shadow_depth but doesn't write it).
           The DCB itself requests a GPU-side depth clear via DB_RENDER_CONTROL.depth_clear_enable=1
           and DEPTH_CLEAR=1.0f on the first draw — shadPS4 translates this to a Vulkan
           loadOp=Clear on the depth attachment. A CPU linear memset won't work because
           the depth buffer is GPU-tiled. */
        struct PM4Builder pm4; pm4_init(&pm4,dcb_mem[bi],DCB_SIZE/4);
        uint32_t sz=build_dcb(&pm4,vs,ps,ps_dark_gpu,0,ps_floor_gpu,
                              vb_v,bg_v,0,floor_v,
                              vb,desc,model_verts,g_vb_total,g_ib,g_num_idx,g_indexed,
                              fb[bi],depth,0,fence,fv);

        /* shadow_depth clear moved to GPU-side DMA_DATA at start of shadow DCB
           (pm4_dma_fill). CPU memset was bypassing shadPS4's Vulkan image cache —
           stale cached pixels from previous frames leaked into the main pass
           sample, producing ghost silhouettes. */

        /* Build shadow DCB. Shadow VS uses its OWN V# pointing at shadow_vb which
           contains [light_MVP @ 0][pad][verts copy @ 0x50 = 80]. No MVP swap needed.
           For huge meshes we skip shadow (shadow_vb alloc fails/skipped). */
        uint32_t shadow_sz = 0;
#if !defined(MINIMAL_TEST) && !defined(DRAW_STOP)
        if (shadow_depth && g_shadow_ready) {
            struct PM4Builder shadow_pm4;
            pm4_init(&shadow_pm4, shadow_dcb_mem[bi], DCB_SIZE/4);
            shadow_sz = build_shadow_dcb(&shadow_pm4,
                                         vs_shadow, ps_shadow_gpu, ps_shadow_clear_gpu,
                                         shadow_vb_v, shadow_floor_v, bg_v, desc,
                                         g_shadow_verts, FLOOR_VERTS,
                                         0, 0, 0,
                                         shadow_depth, shadow_fence, fv);
        }
#endif

        if (shadow_sz > 0) {
            /* Submit shadow + main in a SINGLE sceGnmSubmitCommandBuffers call.
               shadPS4 processes each pointer as a distinct DCB but concatenates
               them in the Vulkan queue so their execution is strictly sequential.
               Previously two separate submits produced two separate Vulkan command
               buffer batches that could overlap in execution — main DCB sampled
               shadow_depth while shadow draw was still in-flight, producing
               two silhouettes (current + previous frame) in the sampled texture.
               The acquire_mem at end of shadow DCB becomes a Vulkan barrier that
               correctly serializes within the single submission. */
            {
                const uint32_t *a[2] = { shadow_dcb_mem[bi], dcb_mem[bi] };
                uint32_t s[2] = { shadow_sz, sz };
                sceGnmSubmitCommandBuffers(2, (void**)a, s, 0, 0);
            }
            sceGnmSubmitDone();
        } else {
            const uint32_t *a[1] = { dcb_mem[bi] };
            uint32_t s[1] = { sz };
            sceGnmSubmitCommandBuffers(1, (void**)a, s, 0, 0);
            sceGnmSubmitDone();
        }
        for (int w=0;w<1000000&&*fence<fv;w++) sceKernelUsleep(10);
        fv++;
        /* Throttle flips: wait until the flip queue has a free slot before
           submitting, so we never reuse a buffer the display is still scanning.
           With NUM_FRAMES buffers, keep at most NUM_FRAMES-1 flips pending.
           Without this the flip FIFO overflows after a few seconds and the
           display freezes. */
        { OrbisVideoOutFlipStatus fs;
          for (int w=0; w<100000; w++) {
              if (sceVideoOutGetFlipStatus(video,&fs) != 0) break;
              if (fs.flipPendingNum < NUM_FRAMES) break;
              sceKernelUsleep(200);
          } }
        sceVideoOutSubmitFlip(video,bi,1,(int64_t)frame);
        frame++;
    }
    return 0;
}
