// ============================================================================
// PS4 GPU Compute Dispatch Debug — minimal shader writes its lane id to dst[lane]
// Tests: does every lane run + store? Post-marker is OUTSIDE the output range so
// it cannot mask a missing lane. Distinguishes a real lane drop from a marker
// collision at dst[63].
// Results written to /temp0/results.txt (fallback /data/results.txt).
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

static void zero(void* s, unsigned long n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) *p++ = 0;
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

#define NUM_ELEMENTS 64

// Shader: writes its own lane id (v0) to dst[lane]. No buffer_load, no src buffer.
// Lane id (not a constant) so each dst[i] should equal i -- lets us see exactly
// which lanes wrote, instead of a constant that hides per-lane coverage.
//   v_mov_b32 v1, v0                            ; v1 = lane id (thread_id_x, TIDIG_COMP_CNT=0)
//   buffer_store_dword v1, v0, s[0:3], 0 idxen  ; dst[v0] = v1
//   s_waitcnt vmcnt(0)
//   s_endpgm
//   [OrbShdr BinaryInfo: type=5(CS), length=20 bytes]
static const uint32_t g_shader[] = {
    0x7E020300,                        // v_mov_b32 v1, v0   (v1 = lane id)
    0xE0702000, 0x80000100,            // buffer_store_dword v1, v0, s[0:3], 0 idxen
    0xBF8C0F70,                        // s_waitcnt vmcnt(0)
    0xBF810000,                        // s_endpgm
    0x5362724F, 0x00726468,            // "OrbShdr\0"
    0x00001414,                        // type=5(CS), length=20 bytes
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

int main(void) {
    log_init();
    logf("=== Compute Dispatch Debug ===\n");

    uint32_t* shader = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* dst    = (uint32_t*)gpu_alloc(0x10000);
    uint32_t* dcb    = (uint32_t*)gpu_alloc(0x10000);
    if (!shader || !dst || !dcb) { logf("alloc fail\n"); log_close(); return 1; }

    zero(shader, 0x10000);
    zero(dst, 0x10000);
    zero(dcb, 0x10000);
    for (uint32_t i = 0; i < sizeof(g_shader) / 4; i++) shader[i] = g_shader[i];

    // V# for dst: stride=4, num_records=64, XYZW / UINT / 32-bit
    uint64_t dst_addr = (uint64_t)(uintptr_t)dst;
    uint32_t vdesc[4] = {
        (uint32_t)(dst_addr & 0xFFFFFFFF),
        ((uint32_t)(dst_addr >> 32) & 0xFFF) | (4 << 16),
        NUM_ELEMENTS,
        (4<<0)|(5<<3)|(6<<6)|(7<<9)|(4<<12)|(4<<15),
    };

    uint32_t off = 0;
    #define EMIT(v) dcb[off++] = (uint32_t)(v)

    EMIT(PM4_HDR(IT_CONTEXT_CONTROL, 2)); EMIT(0x80000000); EMIT(0x80000000);

    // Pre-dispatch marker: dst[0] = 0x11111111
    uint64_t m0 = (uint64_t)(uintptr_t)&dst[0];
    EMIT(PM4_HDR(IT_WRITE_DATA, 4)); EMIT((5<<8)|(1<<20));
    EMIT(m0 & 0xFFFFFFFF); EMIT(m0 >> 32); EMIT(0x11111111);

    // Shader address
    uint64_t sa = (uint64_t)(uintptr_t)shader;
    EMIT(PM4_HDR(IT_SET_SH_REG, 3)); EMIT(CS_ADDRESS_LO);
    EMIT((sa >> 8) & 0xFFFFFFFF); EMIT((sa >> 40) & 0xFF);

    // Thread group 64x1x1
    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_X); EMIT((64<<16)|64);
    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_Y); EMIT((1<<16)|1);
    EMIT(PM4_HDR(IT_SET_SH_REG, 2)); EMIT(CS_NUM_THREAD_Z); EMIT((1<<16)|1);

    // RSRC1=0 (vgprs/sgprs minimal), RSRC2 user_sgpr=4
    EMIT(PM4_HDR(IT_SET_SH_REG, 3)); EMIT(CS_SETTINGS_LO); EMIT(0x00000000); EMIT(0x00000008);

    // User data: V# → s[0:3]
    EMIT(PM4_HDR(IT_SET_SH_REG, 5)); EMIT(CS_USER_DATA_0);
    EMIT(vdesc[0]); EMIT(vdesc[1]); EMIT(vdesc[2]); EMIT(vdesc[3]);

    // Dispatch 1 group
    EMIT(PM4_HDR(IT_DISPATCH_DIRECT, 4)); EMIT(1); EMIT(1); EMIT(1); EMIT(1);

    // Post-dispatch marker: dst[64] = 0x22222222  (OUTSIDE the 0..63 shader range,
    // so it cannot collide with the shader's own write to dst[63])
    uint64_t m1 = (uint64_t)(uintptr_t)&dst[64];
    EMIT(PM4_HDR(IT_WRITE_DATA, 4)); EMIT((5<<8)|(1<<20));
    EMIT(m1 & 0xFFFFFFFF); EMIT(m1 >> 32); EMIT(0x22222222);

    EMIT(PM4_HDR(IT_NOP, 1)); EMIT(0);
    #undef EMIT

    logf("DCB: %d dwords\n", off);

    const uint32_t* dp[1] = { dcb };
    uint32_t ds[1] = { off * 4 };
    sceGnmSubmitCommandBuffers(1, dp, ds, nullptr, nullptr);
    sceGnmSubmitDone();
    sceKernelUsleep(2000000);

    logf("dst[0]  = %x", dst[0]);
    if      (dst[0] == 0x11111111) logf(" (pre-marker survived: lane 0 did NOT write)\n");
    else if (dst[0] == 0x00000000) logf(" (lane 0 wrote its id 0)\n");
    else                           logf(" (unexpected)\n");

    logf("dst[63] = %x", dst[63]);
    if      (dst[63] == 63) logf(" (lane 63 wrote its id)\n");
    else if (dst[63] == 0)  logf(" (lane 63 did NOT write: still zero)\n");
    else                    logf(" (unexpected)\n");

    logf("dst[64] = %x", dst[64]);
    if      (dst[64] == 0x22222222) logf(" (post-marker: PM4 continued past dispatch)\n");
    else                            logf(" (post-marker missing!)\n");

    // dst[i] should equal i. For i>0 a non-written slot stays 0 (zeroed);
    // for i==0 a non-written slot stays the pre-marker 0x11111111.
    int hits = 0;
    int wrong = 0;
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        uint32_t unwritten = (i == 0) ? 0x11111111u : 0u;
        if (dst[i] == (uint32_t)i)        hits++;
        else if (dst[i] != unwritten)     wrong++;
    }
    logf("\nLanes that wrote their id: %d/%d", hits, NUM_ELEMENTS);
    if (wrong) logf("  (%d slots hold a wrong value)", wrong);
    logf("\n");

    if (hits == NUM_ELEMENTS)
        logf("=== ALL 64 LANES WROTE - dispatch fully correct ===\n");
    else if (hits == NUM_ELEMENTS - 1 && dst[63] == 0)
        logf("=== 63/64: lane 63 genuinely did NOT write (real lane drop) ===\n");
    else
        logf("=== %d/64 lanes wrote - investigate the gap ===\n", hits);

    log_close();
    sceKernelUsleep(3000000);
    return 0;
}
