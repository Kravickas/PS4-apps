// ============================================================================
// PS4 GPU Compute — v_cmp mask readback + v_cmp_lg_u64 test
//
// Built on the proven raw-PM4 dispatch path (sceGnmSubmitCommandBuffers) that
// was verified to run all 64 lanes and land buffer_store output on shadPS4.
//
// Two dispatches, each hand-assembled GCN, each storing 4 uniform dwords:
//
//   Test 1  mask_readback (predicate lane<48):
//     v_cmp_lt_u32 s[8:9],v0,48 ; v_cmp_lt_u32 vcc,v0,48
//     store [ s8, s9, vcc_lo, vcc_hi ]   <- reads a v_cmp mask back AS A VALUE
//
//   Test 2  v_cmp_lg_u64 (A=lane<48, B=lane<40)  -- the RE3/RE4 instruction:
//     maskA->s[8:9], maskB->v[2:3], v_cmp_lg_u64 vcc,s[8:9],v[2:3]
//     store [ vcc?1:0, maskA_hi, maskB_hi, 0 ]
//
// dst is pre-filled 0xCDCDCDCD so a slot the shader never wrote is told apart
// from a slot it wrote 0 into. V# stride=16 => each lane's dwordx4 fills its
// own 16-byte record (in-bounds; num_records=64).
//
// Expected on PS4 hardware (64 lanes):
//   mask_readback = [0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0x0000FFFF]
//   lg_u64        = [1, 0x0000FFFF, 0x000000FF, 0]
// shadPS4 is 32-lane, so the hi words are 0 and lg's compare is 0 (both
// predicates are all-true across 32 lanes) -- explainable, not garbage. The
// decisive observable is whether the lo words read back as the real mask
// (0xFFFFFFFF) or as garbage (the per-lane-U1 readback bug).
//
// Results -> /temp0/results.txt (fallback /data/results.txt).
// ============================================================================

#include <stdint.h>

extern "C" {
    int  sceKernelAllocateDirectMemory(long searchStart, long searchEnd,
                                        unsigned long len, unsigned long alignment,
                                        int memoryType, long* physAddrOut);
    int  sceKernelMapDirectMemory(void** addr, unsigned long len, int prot,
                                  int flags, long physAddr, unsigned long alignment);
    int  sceKernelUsleep(unsigned int usec);
    int  sceKernelOpen(const char* path, int flags, unsigned short mode);
    long sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int  sceKernelClose(int fd);
    int  sceKernelFsync(int fd);
    int  sceGnmSubmitCommandBuffers(uint32_t count, const uint32_t* dcb_addrs[],
                                    uint32_t* dcb_sizes, const uint32_t* ccb_addrs[],
                                    uint32_t* ccb_sizes);
    int  sceGnmSubmitDone();
}

// ---- Minimal logging: only %d and %x are used in this program -------------
static int g_log_fd = -1;

static void log_init() {
    g_log_fd = sceKernelOpen("/temp0/results.txt", 0x0601, 0777);
    if (g_log_fd < 0)
        g_log_fd = sceKernelOpen("/data/results.txt", 0x0601, 0777);
}

static int append_dec(char* b, int p, int v) {
    if (v < 0) { b[p++] = '-'; v = -v; }
    if (v == 0) { b[p++] = '0'; return p; }
    char tmp[16]; int n = 0;
    while (v > 0) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) b[p++] = tmp[--n];
    return p;
}

static int append_hex(char* b, int p, uint32_t v) {
    b[p++] = '0'; b[p++] = 'x';
    if (v == 0) { b[p++] = '0'; return p; }
    char tmp[8]; int n = 0;
    while (v > 0) { int d = v & 0xF; tmp[n++] = d < 10 ? (char)('0'+d) : (char)('A'+d-10); v >>= 4; }
    while (n > 0) b[p++] = tmp[--n];
    return p;
}

static void logf(const char* fmt, ...) {
    char buf[256];
    int p = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    for (const char* f = fmt; *f && p < 240; f++) {
        if (*f != '%') { buf[p++] = *f; continue; }
        f++;
        if (*f == 'd')      p = append_dec(buf, p, __builtin_va_arg(ap, int));
        else if (*f == 'x') p = append_hex(buf, p, __builtin_va_arg(ap, uint32_t));
        else                buf[p++] = *f;
    }
    __builtin_va_end(ap);
    if (g_log_fd >= 0) sceKernelWrite(g_log_fd, buf, (unsigned long)p);
}

static void log_close() {
    if (g_log_fd >= 0) { sceKernelFsync(g_log_fd); sceKernelClose(g_log_fd); }
}

// ---- GPU memory ------------------------------------------------------------
static void* gpu_alloc(unsigned long size) {
    const unsigned long align = 0x10000;
    size = (size + 0xFFFF) & ~0xFFFFUL;
    long phys = 0;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, 3, &phys) != 0)
        return nullptr;
    void* ptr = nullptr;
    if (sceKernelMapDirectMemory(&ptr, size, 0x33, 0, phys, align) != 0)
        return nullptr;
    return ptr;
}

static void fill32(void* s, unsigned long dwords, uint32_t v) {
    uint32_t* p = (uint32_t*)s;
    for (unsigned long i = 0; i < dwords; i++) p[i] = v;
}

// ---- PM4 -------------------------------------------------------------------
#define PM4_HDR(op, count) ((3u << 30) | ((uint32_t)((count)-1) << 16) | (((uint32_t)(op) & 0xFF) << 8))
#define IT_NOP             0x10
#define IT_DISPATCH_DIRECT 0x15
#define IT_SET_SH_REG      0x76
#define IT_CONTEXT_CONTROL 0x28
#define IT_WRITE_DATA      0x37

#define CS_NUM_THREAD_X  0x207
#define CS_NUM_THREAD_Y  0x208
#define CS_NUM_THREAD_Z  0x209
#define CS_ADDRESS_LO    0x20C
#define CS_SETTINGS_LO   0x212
#define CS_USER_DATA_0   0x240

#define CD 0xCDCDCDCDu   // pre-fill sentinel: "shader never wrote here"

// ---- Shader 1: mask_readback, predicate lane<48 ----------------------------
// RSRC1 = 0x41 : VGPRS=1 (v0..v7), SGPRS=1 (s0..s15)
static const uint32_t g_shader_mask[] = {
    0xD1820008, 0x00016100,            // v_cmp_lt_u32 s[8:9], v0, 48   maskA->s[8:9]
    0xD182006A, 0x00016100,            // v_cmp_lt_u32 vcc,    v0, 48   maskA->vcc
    0x7E080208,                        // v_mov_b32 v4, s8
    0x7E0A0209,                        // v_mov_b32 v5, s9
    0x7E0C026A,                        // v_mov_b32 v6, vcc_lo
    0x7E0E026B,                        // v_mov_b32 v7, vcc_hi
    0xE0782000, 0x80000400,            // buffer_store_dwordx4 v[4:7], v0, s[0:3] idxen
    0xBF8C0F70,                        // s_waitcnt vmcnt(0)
    0xBF810000,                        // s_endpgm
    0x5362724F, 0x00726468,            // "OrbShdr\0"
    0x00003014,                        // type=5(CS), length=48 bytes
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

// ---- Shader 2: v_cmp_lg_u64, A=lane<48, B=lane<40 --------------------------
// RSRC1 = 0x42 : VGPRS=2 (v0..v11), SGPRS=1 (s0..s15)
static const uint32_t g_shader_lg[] = {
    0xD1820008, 0x00016100,            // v_cmp_lt_u32 s[8:9], v0, 48   maskA->s[8:9]
    0xD182006A, 0x00015100,            // v_cmp_lt_u32 vcc,    v0, 40   maskB->vcc
    0x7E04026A,                        // v_mov_b32 v2, vcc_lo          maskB->v[2:3]
    0x7E06026B,                        // v_mov_b32 v3, vcc_hi
    0xD1CA006A, 0x00020408,            // v_cmp_lg_u64 vcc, s[8:9], v[2:3]
    0x7E100281,                        // v_mov_b32 v8, 1
    0x00081080,                        // v_cndmask_b32 v4, 0, v8, vcc  v4 = vcc?1:0
    0x7E0A0209,                        // v_mov_b32 v5, s9              maskA_hi
    0x7E0C0303,                        // v_mov_b32 v6, v3              maskB_hi
    0x7E0E0280,                        // v_mov_b32 v7, 0
    0xE0782000, 0x80000400,            // buffer_store_dwordx4 v[4:7], v0, s[0:3] idxen
    0xBF8C0F70,                        // s_waitcnt vmcnt(0)
    0xBF810000,                        // s_endpgm
    0x5362724F, 0x00726468,            // "OrbShdr\0"
    0x00004414,                        // type=5(CS), length=68 bytes
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

// One dispatch: program the shader, V# (stride=16 -> per-lane 16-byte record),
// user data s[0:3]=V#, 64x1x1, dispatch 1 group; post-marker at dst[256]
// (just past the 64 records) confirms the PM4 ran to completion.
static void run_dispatch(uint32_t* dcb, const uint32_t* shader, uint32_t* dst,
                         uint32_t rsrc1) {
    uint64_t dst_addr = (uint64_t)(uintptr_t)dst;
    uint32_t vdesc[4] = {
        (uint32_t)(dst_addr & 0xFFFFFFFF),
        ((uint32_t)(dst_addr >> 32) & 0xFFF) | (16 << 16),   // stride = 16 bytes
        64,                                                   // num_records
        (4<<0)|(5<<3)|(6<<6)|(7<<9)|(4<<12)|(4<<15),
    };

    uint32_t off = 0;
    #define EMIT(v) dcb[off++] = (uint32_t)(v)

    EMIT(PM4_HDR(IT_CONTEXT_CONTROL, 2)); EMIT(0x80000000); EMIT(0x80000000);

    uint64_t sa = (uint64_t)(uintptr_t)shader;
    EMIT(PM4_HDR(IT_SET_SH_REG, 3)); EMIT(CS_ADDRESS_LO);
    EMIT((sa >> 8) & 0xFFFFFFFF); EMIT((sa >> 40) & 0xFF);

    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_X); EMIT((64<<16)|64);
    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_Y); EMIT((1<<16)|1);
    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_Z); EMIT((1<<16)|1);

    // RSRC1 = per-shader register count, RSRC2 user_sgpr=4 (s[0:3]=V#)
    EMIT(PM4_HDR(IT_SET_SH_REG, 3)); EMIT(CS_SETTINGS_LO); EMIT(rsrc1); EMIT(0x00000008);

    EMIT(PM4_HDR(IT_SET_SH_REG, 5)); EMIT(CS_USER_DATA_0);
    EMIT(vdesc[0]); EMIT(vdesc[1]); EMIT(vdesc[2]); EMIT(vdesc[3]);

    EMIT(PM4_HDR(IT_DISPATCH_DIRECT, 4)); EMIT(1); EMIT(1); EMIT(1); EMIT(1);

    uint64_t m = (uint64_t)(uintptr_t)&dst[256];
    EMIT(PM4_HDR(IT_WRITE_DATA, 4)); EMIT((5<<8)|(1<<20));
    EMIT(m & 0xFFFFFFFF); EMIT(m >> 32); EMIT(0x22222222);

    EMIT(PM4_HDR(IT_NOP, 1)); EMIT(0);
    #undef EMIT

    const uint32_t* dp[1] = { dcb };
    uint32_t ds[1] = { off * 4 };
    sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
    sceGnmSubmitDone();
    sceKernelUsleep(2000000);
}

static void report(const char* name, uint32_t* dst,
                   uint32_t e0, uint32_t e1, uint32_t e2, uint32_t e3) {
    logf("\n");
    logf(name);
    logf("\n");
    logf("  dst[256] marker = %x", dst[256]);
    logf(dst[256] == 0x22222222 ? "  (PM4 ran to completion)\n" : "  (marker MISSING!)\n");

    uint32_t v0 = dst[0], v1 = dst[1], v2 = dst[2], v3 = dst[3];
    logf("  got      = [ %x", v0); logf(", %x", v1); logf(", %x", v2); logf(", %x", v3); logf(" ]\n");
    logf("  PS4-64   = [ %x", e0); logf(", %x", e1); logf(", %x", e2); logf(", %x", e3); logf(" ]\n");

    int unwritten = (v0==CD) + (v1==CD) + (v2==CD) + (v3==CD);
    if (unwritten) logf("  %d/4 slots still 0xCDCDCDCD -- store did NOT fully land\n", unwritten);
    else           logf("  all 4 slots written (store landed)\n");

    if (v0==e0 && v1==e1 && v2==e2 && v3==e3)
        logf("  == matches PS4-64 exactly ==\n");
    else
        logf("  != PS4-64 (on 32-lane shadPS4 the hi words are 0; compare narrows)\n");
}

int main(void) {
    log_init();
    logf("=== v_cmp mask readback + v_cmp_lg_u64 ===\n");

    uint32_t* shaderA = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* shaderB = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* dstA    = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* dstB    = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* dcb     = (uint32_t*)gpu_alloc(0x10000);
    if (!shaderA || !shaderB || !dstA || !dstB || !dcb) {
        logf("alloc fail\n"); log_close(); return 1;
    }

    fill32(shaderA, 0x10000/4, 0);
    fill32(shaderB, 0x10000/4, 0);
    for (uint32_t i = 0; i < sizeof(g_shader_mask)/4; i++) shaderA[i] = g_shader_mask[i];
    for (uint32_t i = 0; i < sizeof(g_shader_lg)/4;   i++) shaderB[i] = g_shader_lg[i];

    fill32(dstA, 0x10000/4, CD);
    fill32(dstB, 0x10000/4, CD);

    run_dispatch(dcb, shaderA, dstA, 0x41);
    run_dispatch(dcb, shaderB, dstB, 0x42);

    report("Test 1  mask_readback  (lane<48)", dstA,
           0xFFFFFFFF, 0x0000FFFF, 0xFFFFFFFF, 0x0000FFFF);
    report("Test 2  v_cmp_lg_u64   (A=lane<48, B=lane<40)", dstB,
           0x00000001, 0x0000FFFF, 0x000000FF, 0x00000000);

    log_close();
    sceKernelUsleep(3000000);
    return 0;
}
