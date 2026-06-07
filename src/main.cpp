// PS4 IMAGE_LOAD_MIP + 2x MSAA T# hardware test (graphics pipeline, two-pass)
//
// Built on the CUBETST00 GFX-ring launch (sceGnmSubmitCommandBuffers).
// The earlier single-shader version wrote and read the same MSAA UAV in one
// wave; that intra-wave read-after-write wedged the real GPU. This version
// separates them into two submits with a fence between, so the write fully
// retires before the read issues (GLC keeps the data coherent through L2):
//
//   PASS 1 (write_ps): image_store 0xAAAAAAAA -> sample 0, 0xBBBBBBBB -> sample 1
//   --- fence wait (write draw fully retired) ---
//   PASS 2 (read_ps):  image_load fragid 0/1            -> baseline [0],[1]
//                      image_load_mip (0xf0042100) v2/v3 -> tests   [2]..[5]
//
// On-disk stage markers are flushed before each submit, so if the GPU hangs
// the result file still shows which pass it died in.
//
// Verdict: [2]==AAAA && [3]==BBBB -> sample tracks v2 (Outcome A, mip slot is
// the fragid). [2]==BBBB && [3]==AAAA -> tracks v3 (Outcome B, what #4207 does).

#include <stdint.h>
#include "pm4.h"

#ifdef __cplusplus
extern "C" {
#endif
extern int  sceKernelAllocateDirectMemory(long, long, unsigned long, unsigned long, int, long*);
extern int  sceKernelMapDirectMemory(void**, unsigned long, int, int, long, unsigned long);
extern int  sceKernelUsleep(unsigned int);
extern int  sceVideoOutOpen(int, int, int, void*);
extern int  sceVideoOutSetFlipRate(int, int);
extern void sceVideoOutSetBufferAttribute(void*, uint32_t, uint32_t, uint32_t,
                                           uint32_t, uint32_t, uint32_t);
extern int  sceVideoOutRegisterBuffers(int, int, void* const*, int, void*);
extern int  sceVideoOutSubmitFlip(int, int, uint32_t, int64_t);
extern int  sceGnmSubmitCommandBuffers(uint32_t, const uint32_t**, uint32_t*, void*, void*);
extern int  sceGnmSubmitDone(void);
extern int  sceKernelOpen(const char*, int, int);
extern long sceKernelWrite(int, const void*, unsigned long);
extern int  sceKernelClose(int);
extern int  printf(const char*, ...);
#ifdef __cplusplus
}
#endif

#define DISPLAY_W   1920
#define DISPLAY_H   1080
#define NUM_FRAMES  2
#define DCB_SIZE    0x10000
#define MEM_TYPE_FLEX 0x03
#define SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB 0x80000000

// --- Vertex shader: fullscreen triangle from a 3-vertex buffer (pos + color) ---
static const uint32_t vs_shader_binary[] = {
    0xBEEB03FF, 0x00000006,
    0x28100085,
    0xE0381000, 0x80000008,
    0xE0381010, 0x80000408,
    0xBF8C1F70,
    0xF80000CF, 0x03020100,
    0xF8000A0F, 0x07060504,
    0xBF810000,
    0xBF800000,
    0x5362724F, 0x00726468, 0x00003804, 0x00000000, 0x12345678, 0xDEADBEEF, 0x00000000,
};

// --- PASS 1: write-only PS. s[0:7] = MSAA T#. No reads -> no intra-wave hazard. ---
static const uint32_t write_ps_binary[] = {
    0xBEEB03FF, 0x00000006,
    0x7E020080, 0x7E020280, 0x7E020480,   // v0=0, v1=0, v2=0
    0x7E0208FF, 0xAAAAAAAA,                // v4 = 0xAAAAAAAA
    0xF0203100, 0x00000400,                // image_store v4, v[0:2], s[0:7] glc -> sample 0
    0xBF8C1F70,
    0x7E020481, 0x7E0208FF, 0xBBBBBBBB,    // v2=1, v4=0xBBBBBBBB
    0xF0203100, 0x00000400,                // image_store -> sample 1
    0xBF8C1F70,
    0xF800180F, 0x00000000,                // exp mrt0 (dummy, v0 x4) done vm
    0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x00005800, 0x00000000, 0x00000001, 0x00005752, 0x00000000,
};

// --- PASS 2: read-only PS. s[0:7] = MSAA T#, s[8:11] = result V#. ---
static const uint32_t read_ps_binary[] = {
    0xBEEB03FF, 0x00000006,
    0x7E1402FF, 0xC0DE0002,                // smoke: v10 = marker
    0xE0704018, 0x80020A00,                // buffer_store -> out[6]
    0xBF8C1F70,
    0x7E020080, 0x7E020280, 0x7E020480,    // [0] image_load fragid 0
    0xF0003100, 0x00000A00,
    0xBF8C1F70,
    0xE0704000, 0x80020A00,
    0x7E020481,                            // [1] image_load fragid 1
    0xF0003100, 0x00000A00,
    0xBF8C1F70,
    0xE0704004, 0x80020A00,
    0x7E020080, 0x7E020280, 0x7E020480, 0x7E020681,  // [2] mip v2=0,v3=1
    0xF0042100, 0x00000A00,
    0xBF8C1F70,
    0xE0704008, 0x80020A00,
    0x7E020480, 0x7E020680,                // [3] mip v2=0,v3=0 (control -> sample 0)
    0xF0042100, 0x00000A00,
    0xBF8C1F70,
    0xE070400C, 0x80020A00,
    0xBF8C1F70,
    0xF800180F, 0x0A0A0A0A,                // exp mrt0 (v10 x4) done vm
    0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000A800, 0x00000000, 0x00000002, 0x00005245, 0x00000000,
};

static void my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d;
    for (unsigned long i = 0; i < n; i++) p[i] = (unsigned char)v;
}
static void my_memcpy(void* d, const void* s, unsigned long n) {
    unsigned char* dp = (unsigned char*)d; const unsigned char* sp = (const unsigned char*)s;
    for (unsigned long i = 0; i < n; i++) dp[i] = sp[i];
}
static void* gpu_alloc(unsigned long size, unsigned long align) {
    if (align < 0x4000) align = 0x4000;
    size = (size + (align - 1)) & ~(align - 1);
    long phys = 0; void* addr = 0;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, MEM_TYPE_FLEX, &phys)) return 0;
    if (sceKernelMapDirectMemory(&addr, size, 0x33, 0, phys, align)) return 0;
    my_memset(addr, 0, size);
    return addr;
}

static void build_buffer_vsharp(uint32_t* v, void* base, uint32_t size_bytes) {
    uint64_t addr = (uint64_t)(uintptr_t)base;
    v[0] = (uint32_t)(addr & 0xFFFFFFFFu);
    v[1] = (uint32_t)(addr >> 32) & 0xFFFF;
    v[2] = size_bytes;
    v[3] = (0u) | (1u << 3) | (2u << 6) | (3u << 9) | (4u << 12) | (4u << 15) | (0u << 27);
}
static void build_msaa_tsharp(uint32_t d[8], void* base) {
    my_memset(d, 0, 32);
    uint64_t addr = (uint64_t)(uintptr_t)base;
    uint64_t b = addr >> 8;
    uint64_t w01 = (b & 0x3FFFFFFFFFull) | (4ull << 52) | (4ull << 58);
    d[0] = (uint32_t)w01; d[1] = (uint32_t)(w01 >> 32);
    uint64_t w23 = 7ull | (7ull << 14) | (4ull << 32) | (5ull << 35) |
        (6ull << 38) | (7ull << 41) | (1ull << 48) | (14ull << 52) | (14ull << 60);
    d[2] = (uint32_t)w23; d[3] = (uint32_t)(w23 >> 32);
    d[4] = (uint32_t)(7ull << 13);
}

static void s_cat(char* dst, int* p, const char* s) { int i = 0; while (s[i]) dst[(*p)++] = s[i++]; }
static void s_hex(char* dst, int* p, uint32_t v) {
    const char* h = "0123456789ABCDEF";
    s_cat(dst, p, "0x");
    for (int i = 7; i >= 0; i--) dst[(*p)++] = h[(v >> (i * 4)) & 0xF];
}
static void write_file(const char* body, int n) {
    int fd = sceKernelOpen("/data/image_msaa_result.txt", 0x601, 0x1B6);
    if (fd < 0) fd = sceKernelOpen("/mnt/usb0/image_msaa_result.txt", 0x601, 0x1B6);
    if (fd >= 0) { sceKernelWrite(fd, body, (unsigned long)n); sceKernelClose(fd); }
    printf("%s", body);
}
static void write_stage(const char* msg) {
    char s[256]; int p = 0; s_cat(s, &p, "STAGE: "); s_cat(s, &p, msg); s_cat(s, &p, "\n");
    write_file(s, p);
}

// One DCB: bind a PS (+rsrc, +user data) and draw the fullscreen triangle.
static uint32_t build_dcb(struct PM4Builder* b, const uint32_t* vs_addr, const uint32_t* ps_addr,
                          uint32_t ps_rsrc1, uint32_t ps_rsrc2, const uint32_t* ud, uint32_t ud_n,
                          const uint32_t* vb_vsharp, void* color_base,
                          volatile uint32_t* fence_addr, uint32_t fence_value) {
    pm4_context_control(b);
    {
        uint64_t g = (uint64_t)(uintptr_t)vs_addr;
        uint32_t regs[4] = { (uint32_t)(g >> 8), (uint32_t)(g >> 40), 3u, (4u << 1) };
        pm4_set_sh_regs(b, SH_VS_PGM_LO, regs, 4);
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, vb_vsharp, 4);
    }
    {
        uint64_t g = (uint64_t)(uintptr_t)ps_addr;
        uint32_t regs[4] = { (uint32_t)(g >> 8), (uint32_t)(g >> 40), ps_rsrc1, ps_rsrc2 };
        pm4_set_sh_regs(b, SH_PS_PGM_LO, regs, 4);
        pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, ud_n);
    }
    { uint32_t sc[2] = { 0, (16u & 0x7FFF) | ((16u & 0x7FFF) << 16) };
      pm4_set_context_regs(b, CTX_SCREEN_SCISSOR, sc, 2); }
    { uint32_t sc[2] = { 0, (16u & 0x7FFF) | ((16u & 0x7FFF) << 15) };
      pm4_set_context_regs(b, CTX_GENERIC_SCISSOR, sc, 2); }
    { uint32_t sc[2] = { (1u << 30), (16u & 0x7FFF) | ((16u & 0x7FFF) << 15) };
      pm4_set_context_regs(b, CTX_WINDOW_SCISSOR, sc, 2); }
    { uint32_t sc[2] = { 0, (16u & 0x7FFF) | ((16u & 0x7FFF) << 15) };
      pm4_set_context_regs(b, CTX_VIEWPORT_SCISSOR0, sc, 2); }
    {
        pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
        pm4_emit(b, CTX_VIEWPORT0);
        pm4_emit_f(b, (float)DISPLAY_W * 0.5f); pm4_emit_f(b, (float)DISPLAY_W * 0.5f);
        pm4_emit_f(b, (float)DISPLAY_H * -0.5f); pm4_emit_f(b, (float)DISPLAY_H * 0.5f);
        pm4_emit_f(b, 0.5f); pm4_emit_f(b, 0.5f);
    }
    pm4_set_context_reg(b, CTX_INDEX_OFFSET, 0);
    pm4_set_context_reg(b, CTX_DEPTH_CONTROL, 0);
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 0);
    pm4_set_context_reg(b, CTX_DB_STENCIL_INFO, 0);
    pm4_set_context_reg(b, 0x203, 0);
    pm4_set_context_reg(b, CTX_CB_COLOR0_BASE, (uint32_t)((uint64_t)(uintptr_t)color_base >> 8));
    pm4_set_context_reg(b, CTX_CB_COLOR0_PITCH, (DISPLAY_W / 8) - 1);
    pm4_set_context_reg(b, CTX_CB_COLOR0_SLICE, (DISPLAY_W * DISPLAY_H / 64) - 1);
    pm4_set_context_reg(b, CTX_CB_COLOR0_VIEW, 0);
    pm4_set_context_reg(b, CTX_CB_COLOR0_INFO, (10u << 2) | (0u << 8)); // 8_8_8_8 UNORM
    pm4_set_context_reg(b, CTX_CB_COLOR0_ATTRIB, 8u);
    pm4_set_context_reg(b, CTX_COLOR_TARGET_MASK, 0x0000000Fu);
    pm4_set_context_reg(b, CTX_COLOR_SHADER_MASK, 0x0000000Fu);
    pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0, 0);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 0);
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x00000002u);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x00000002u);
    pm4_set_context_reg(b, CTX_NUM_INTERP, 1);
    pm4_set_context_reg(b, CTX_SHADER_POS_FORMAT, 4);
    pm4_set_context_reg(b, CTX_Z_EXPORT_FORMAT, 0);
    pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9);
    pm4_set_context_reg(b, CTX_COLOR_CONTROL, 0x00CC0010u);
    pm4_set_context_reg(b, CTX_CLIPPER_CONTROL, 0);
    pm4_set_context_reg(b, CTX_VIEWPORT_CONTROL, 0x0000043Fu);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONTROL, 0);
    pm4_set_context_reg(b, 0x205, 0x00000006u);
    pm4_set_context_reg(b, CTX_MODE_CONTROL, 0);
    pm4_set_context_reg(b, CTX_STAGE_ENABLE, 0);
    pm4_set_context_reg(b, CTX_AA_CONFIG, 0);
    pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);
    pm4_set_context_reg(b, CTX_INDEX_SIZE, 0);
    pm4_set_uconfig_reg(b, UCFG_PRIMITIVE_TYPE, 4);
    pm4_set_uconfig_reg(b, UCFG_NUM_INSTANCES, 1);
    pm4_draw_index_auto(b, 3);
    pm4_event_write_eop(b, fence_addr, fence_value);
    return b->off * 4;
}

int main(void) {
    int video = sceVideoOutOpen(0, 0, 0, 0);
    if (video < 0) { printf("videoout fail\n"); return 1; }
    sceVideoOutSetFlipRate(video, 0);

    unsigned long fb_size = (unsigned long)DISPLAY_W * DISPLAY_H * 4;
    void* fb[NUM_FRAMES];
    for (int i = 0; i < NUM_FRAMES; i++) {
        fb[i] = gpu_alloc(fb_size, 0x100000);
        if (!fb[i]) { printf("fb alloc fail\n"); return 1; }
    }
    unsigned char attr[48]; my_memset(attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(attr, SCE_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB, 0, 0,
                                  DISPLAY_W, DISPLAY_H, DISPLAY_W);
    sceVideoOutRegisterBuffers(video, 0, fb, NUM_FRAMES, attr);

    void* vert_buf = gpu_alloc(4096, 0x1000);
    void* img_mem  = gpu_alloc(0x10000, 0x10000);
    void* res_buf  = gpu_alloc(0x1000, 0x1000);
    void* vs_mem   = gpu_alloc(sizeof(vs_shader_binary) + 256, 0x1000);
    void* wps_mem  = gpu_alloc(sizeof(write_ps_binary) + 256, 0x1000);
    void* rps_mem  = gpu_alloc(sizeof(read_ps_binary) + 256, 0x1000);
    uint32_t* dcb  = (uint32_t*)gpu_alloc(DCB_SIZE, 0x10000);
    volatile uint32_t* fence = (volatile uint32_t*)gpu_alloc(0x1000, 0x1000);
    if (!vert_buf || !img_mem || !res_buf || !vs_mem || !wps_mem || !rps_mem || !dcb || !fence) {
        printf("alloc fail\n"); return 1;
    }
    my_memcpy(vs_mem, vs_shader_binary, sizeof(vs_shader_binary));
    my_memcpy(wps_mem, write_ps_binary, sizeof(write_ps_binary));
    my_memcpy(rps_mem, read_ps_binary, sizeof(read_ps_binary));

    {
        float* v = (float*)vert_buf;
        float tri[3][8] = {
            { -1.f, -1.f, 0.f, 1.f,  1.f, 0.f, 0.f, 1.f },
            {  3.f, -1.f, 0.f, 1.f,  0.f, 1.f, 0.f, 1.f },
            { -1.f,  3.f, 0.f, 1.f,  0.f, 0.f, 1.f, 1.f },
        };
        for (int i = 0; i < 24; i++) v[i] = ((float*)tri)[i];
    }

    uint32_t vb_vsharp[4], res_vsharp[4], img_tsharp[8];
    build_buffer_vsharp(vb_vsharp, vert_buf, 96);
    build_buffer_vsharp(res_vsharp, res_buf, 256);
    build_msaa_tsharp(img_tsharp, img_mem);
    for (int i = 0; i < 16; i++) ((volatile uint32_t*)res_buf)[i] = 0xDDDDDDDD;
    for (unsigned long i = 0; i < fb_size / 4; i++) ((uint32_t*)fb[0])[i] = 0xFF101010;

    uint32_t ud_write[8];
    for (int i = 0; i < 8; i++) ud_write[i] = img_tsharp[i];
    uint32_t ud_read[12];
    for (int i = 0; i < 8; i++) ud_read[i] = img_tsharp[i];
    for (int i = 0; i < 4; i++) ud_read[8 + i] = res_vsharp[i];

    const uint32_t* addrs[1]; uint32_t sizes[1];

    // PASS 1: write the MSAA samples
    write_stage("1 write submitted (if this is the last line, the WRITE pass hung)");
    *fence = 0;
    {
        struct PM4Builder pm4; pm4_init(&pm4, dcb, DCB_SIZE / 4);
        uint32_t n = build_dcb(&pm4, (const uint32_t*)vs_mem, (const uint32_t*)wps_mem,
                               0x2u, (8u << 1), ud_write, 8, vb_vsharp, fb[0], fence, 1);
        addrs[0] = dcb; sizes[0] = n;
        sceGnmSubmitCommandBuffers(1, addrs, sizes, 0, 0);
        sceGnmSubmitDone();
    }
    for (int w = 0; w < 2000000; w++) { if (*fence >= 1) break; sceKernelUsleep(10); }

    // PASS 2: read them back
    write_stage("2 write ok, read submitted (if last line, the READ pass hung)");
    *fence = 0;
    {
        struct PM4Builder pm4; pm4_init(&pm4, dcb, DCB_SIZE / 4);
        uint32_t n = build_dcb(&pm4, (const uint32_t*)vs_mem, (const uint32_t*)rps_mem,
                               0x42u, (12u << 1), ud_read, 12, vb_vsharp, fb[0], fence, 1);
        addrs[0] = dcb; sizes[0] = n;
        sceGnmSubmitCommandBuffers(1, addrs, sizes, 0, 0);
        sceGnmSubmitDone();
    }
    for (int w = 0; w < 2000000; w++) { if (*fence >= 1) break; sceKernelUsleep(10); }

    // Results
    volatile uint32_t* out = (volatile uint32_t*)res_buf;
    char s[2048]; int p = 0;
    s_cat(s, &p, "IMAGE_LOAD_MIP + 2x MSAA T# HW TEST (two-pass)\n");
    s_cat(s, &p, "[0] LOAD     frag0     = "); s_hex(s, &p, out[0]); s_cat(s, &p, "\n");
    s_cat(s, &p, "[1] LOAD     frag1     = "); s_hex(s, &p, out[1]); s_cat(s, &p, "\n");
    s_cat(s, &p, "[2] LOAD_MIP v2=0,v3=1 = "); s_hex(s, &p, out[2]); s_cat(s, &p, "\n");
    s_cat(s, &p, "[3] LOAD_MIP v2=0,v3=0 = "); s_hex(s, &p, out[3]); s_cat(s, &p, "\n");
    s_cat(s, &p, "smoke out[6]           = "); s_hex(s, &p, out[6]); s_cat(s, &p, "\n");
    s_cat(s, &p, "verdict: ");
    if (out[6] != 0xC0DE0002)
        s_cat(s, &p, "READ PS DID NOT RUN (smoke failed)\n");
    else if (!(out[0] == 0xAAAAAAAA && out[1] == 0xBBBBBBBB))
        s_cat(s, &p, "store/load broken (check T# tiling)\n");
    else if (out[3] != 0xAAAAAAAA)
        s_cat(s, &p, "control v2=0,v3=0 not sample0 - UNEXPECTED\n");
    else if (out[2] == 0xAAAAAAAA)
        s_cat(s, &p, "OUTCOME A - sample = v2 (mip slot=0); #4207 should use Arg(2)\n");
    else if (out[2] == 0xBBBBBBBB)
        s_cat(s, &p, "OUTCOME B - sample = v3; #4207 was correct\n");
    else
        s_cat(s, &p, "UNEXPECTED\n");
    write_file(s, p);

    sceVideoOutSubmitFlip(video, 0, 1, 0);
    for (;;) sceKernelUsleep(1000000);
    return 0;
}
