// ============================================================================
//  PS4 VO flip / IRQ path test  -  standalone homebrew
//
//  Runs the same binary on real hardware and in shadPS4 and prints the same
//  lines, so the two logs can be diffed directly.
//
//  Background (from libSceGnmDriver disassembly): the reserved 64-dword block
//  at the tail of a dcb (header 0xc03e1000 = IT_NOP, 63 payload dwords, with a
//  PrepareFlip marker in payload[0]) is rewritten by PatchFlipRequest into
//  WRITE_DATA(vo_label[buf_idx] = 1) followed by NOP(PatchedFlip 0x68750776).
//  sceGnmInsertWaitFlipDone emits WAIT_REG_MEM(func=equal, ref=0) against the
//  same label, so 1 = buffer locked, 0 = released.
//
//  The dcb needs nothing else: no shaders, no draws, no register state. Each
//  buffer is filled by the CPU so the displayed colour identifies the buffer.
//
//  Output: /data/irq_results.txt (falls back to /temp0), and fd 1.
// ============================================================================

#include <stdint.h>
#include <stddef.h>

extern "C" {
    int32_t sceKernelAllocateDirectMemory(long search_start, long search_end, size_t len,
                                          size_t alignment, int memory_type, long* phys_addr_out);
    int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags,
                                     long phys_addr, size_t alignment);
    long    sceKernelGetDirectMemorySize(void);
    int     sceKernelUsleep(unsigned int usec);
    uint64_t sceKernelGetProcessTime(void);
    int     sceKernelOpen(const char* path, int flags, unsigned short mode);
    long    sceKernelWrite(int fd, const void* buf, unsigned long nbytes);
    int     sceKernelClose(int fd);
    int     sceKernelFsync(int fd);

    int32_t sceVideoOutOpen(int32_t userId, int32_t busType, int32_t index, const void* param);
    void    sceVideoOutSetBufferAttribute(void* attr, uint32_t format, uint32_t tmode,
                                          uint32_t aspect, uint32_t w, uint32_t h, uint32_t pitch);
    int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t startIndex, void* const* addrs,
                                       int32_t num, const void* attr);
    int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
    int32_t sceVideoOutGetFlipStatus(int32_t handle, void* status);
    int32_t sceVideoOutGetBufferLabelAddress(int32_t handle, uintptr_t* label_addr);

    int32_t sceGnmSubmitAndFlipCommandBuffers(uint32_t count, uint32_t* dcb_addrs[],
                                              uint32_t* dcb_sizes, uint32_t* ccb_addrs[],
                                              uint32_t* ccb_sizes, uint32_t vo_handle,
                                              uint32_t buf_idx, uint32_t flip_mode,
                                              int64_t flip_arg);
    int32_t sceUserServiceInitialize(void* params);
}

struct VideoBufAttr {
    int32_t format; int32_t tmode; int32_t aspect;
    uint32_t width; uint32_t height; uint32_t pitch; uint64_t reserved[2];
};

struct VideoFlipStatus {
    uint64_t num, ptime, stime; int64_t flipArg; uint64_t reserved[2];
    int32_t numGpuFlipPending, numFlipPending, currentBuffer; uint32_t r1;
};

// ---------------------------------------------------------------- utilities
static void* my_memset(void* d, int v, unsigned long n) {
    unsigned char* p = (unsigned char*)d;
    while (n--) *p++ = (unsigned char)v;
    return d;
}

static const char* g_log_path = "/data/irq_results.txt";

static void log_init() {
    int fd = sceKernelOpen(g_log_path, 0x0601, 0777);          // create + truncate
    if (fd < 0) {
        g_log_path = "/temp0/irq_results.txt";
        fd = sceKernelOpen(g_log_path, 0x0601, 0777);
    }
    if (fd >= 0) sceKernelClose(fd);
}

static int put_dec(char* b, int p, long long v) {
    if (v < 0) { b[p++] = '-'; v = -v; }
    char t[24]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v > 0) { t[n++] = (char)('0' + (int)(v % 10)); v /= 10; }
    while (n > 0) b[p++] = t[--n];
    return p;
}

static int put_hex(char* b, int p, unsigned long long v) {
    char t[20]; int n = 0;
    if (v == 0) t[n++] = '0';
    while (v > 0) { int d = (int)(v & 0xf); t[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10); v >>= 4; }
    while (n > 0) b[p++] = t[--n];
    return p;
}

// supports %d (int), %l (long long), %X (unsigned long long, hex), %s
static void logf(const char* fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    char buf[512];
    int p = 0;
    for (const char* c = fmt; *c && p < (int)sizeof(buf) - 24; c++) {
        if (*c != '%') { buf[p++] = *c; continue; }
        c++;
        if (*c == 'd')      p = put_dec(buf, p, __builtin_va_arg(ap, int));
        else if (*c == 'l') p = put_dec(buf, p, __builtin_va_arg(ap, long long));
        else if (*c == 'X') p = put_hex(buf, p, __builtin_va_arg(ap, unsigned long long));
        else if (*c == 's') {
            const char* s = __builtin_va_arg(ap, const char*);
            while (*s && p < (int)sizeof(buf) - 2) buf[p++] = *s++;
        } else buf[p++] = *c;
    }
    __builtin_va_end(ap);
    sceKernelWrite(1, buf, (unsigned long)p);
    int fd = sceKernelOpen(g_log_path, 0x0209, 0777);           // append
    if (fd >= 0) {
        sceKernelWrite(fd, buf, (unsigned long)p);
        sceKernelFsync(fd);
        sceKernelClose(fd);
    }
}

// ---------------------------------------------------------------- gpu memory
#define GPU_PAGE  0x10000ul
#define GPU_PAGES 64

static uint8_t* g_pool = 0;
static unsigned long g_pool_off = 0;

static bool pool_init() {
    long phys = 0;
    unsigned long total = GPU_PAGES * GPU_PAGE;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, GPU_PAGE, 3, &phys) != 0)
        return false;
    void* ptr = 0;
    if (sceKernelMapDirectMemory(&ptr, total, 0x33, 0, phys, GPU_PAGE) != 0)
        return false;
    g_pool = (uint8_t*)ptr;
    my_memset(g_pool, 0, total);
    g_pool_off = 0;
    return true;
}

static void* pool_alloc(unsigned long size) {
    unsigned long off = (g_pool_off + 255ul) & ~255ul;          // dcb: 256-byte aligned
    if (off + size > GPU_PAGES * GPU_PAGE) { off = 0; }         // wrap; earlier dcbs are done
    g_pool_off = off + size;
    return g_pool + off;
}

// ---------------------------------------------------------------- video out
#define NBUF 3

static int32_t   g_vo = -1;
static uint32_t* g_fb[NBUF];
static uintptr_t g_label_base = 0;

static inline volatile uint64_t* vo_label(int i) {
    return (volatile uint64_t*)(g_label_base + (uintptr_t)i * 8u);
}

static bool vo_init() {
    g_vo = sceVideoOutOpen(0xFF, 0, 0, 0);
    if (g_vo < 0) return false;

    const uint32_t w = 1920, h = 1080;
    unsigned long fbsz = (unsigned long)w * h * 4ul;
    unsigned long align = 0x200000ul;
    unsigned long total = (fbsz * NBUF + align - 1) / align * align;

    long off = 0;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3, &off) < 0)
        return false;
    void* base = 0;
    if (sceKernelMapDirectMemory(&base, total, 0x33, 0, off, align) < 0)
        return false;

    static const uint32_t fill[NBUF] = { 0xFF201010u, 0xFF102010u, 0xFF101020u };
    for (int b = 0; b < NBUF; b++) {
        g_fb[b] = (uint32_t*)((char*)base + fbsz * b);
        for (unsigned long i = 0; i < (unsigned long)w * h; i++) g_fb[b][i] = fill[b];
    }

    VideoBufAttr attr; my_memset(&attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1u, 0u, w, h, w);   // LINEAR
    void* bufs[NBUF] = { g_fb[0], g_fb[1], g_fb[2] };
    if (sceVideoOutRegisterBuffers(g_vo, 0, bufs, NBUF, &attr) != 0) return false;

    sceVideoOutSetFlipRate(g_vo, 0);

    if (sceVideoOutGetBufferLabelAddress(g_vo, &g_label_base) != 16) return false;
    return g_label_base != 0;
}

// ---------------------------------------------------------------- flip submit
#define IT_NOP              0x10u
#define PREPARE_FLIP_MARKER 0x68750777u   // PM4CmdNop::PrepareFlip

static inline uint32_t pm4_hdr(uint32_t opcode, uint32_t count) {
    return (3u << 30) | ((count - 1u) << 16) | ((opcode & 0xFFu) << 8);
}

// The block must be the LAST 64 dwords of the dcb: PatchFlipRequest indexes
// cmdbuf[size_dw - 64], with size_dw = dcb_size_in_bytes / 4.
static int submit_eop_flip(int buf_idx, int64_t flip_arg) {
    uint32_t* dcb = (uint32_t*)pool_alloc(64 * sizeof(uint32_t));
    if (!dcb) return -1;
    dcb[0] = pm4_hdr(IT_NOP, 63);                 // 0xc03e1000
    dcb[1] = PREPARE_FLIP_MARKER;
    for (int i = 2; i < 64; i++) dcb[i] = 0;

    uint32_t* dp[1] = { dcb };
    uint32_t  ds[1] = { 64u * sizeof(uint32_t) };
    return sceGnmSubmitAndFlipCommandBuffers(1, dp, ds, 0, 0, (uint32_t)g_vo,
                                             (uint32_t)buf_idx, 1u, flip_arg);
}

static bool wait_settled(uint32_t timeout_us) {
    while (timeout_us > 0) {
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_vo, &st);
        if (st.numFlipPending <= 0) return true;
        sceKernelUsleep(200);
        timeout_us = (timeout_us > 200u) ? timeout_us - 200u : 0u;
    }
    return false;
}

// ---------------------------------------------------------------- trace
struct Sample { uint32_t t, l0, l1, l2; int32_t arg, cur, pend, gpu; };

static Sample g_trace[64];
static int    g_ntrace = 0;

static void trace_reset() { g_ntrace = 0; }

static void trace_sample(uint64_t t0) {
    VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
    sceVideoOutGetFlipStatus(g_vo, &st);
    Sample s;
    s.t    = (uint32_t)(sceKernelGetProcessTime() - t0);
    s.l0   = (uint32_t)*vo_label(0);
    s.l1   = (uint32_t)*vo_label(1);
    s.l2   = (uint32_t)*vo_label(2);
    s.arg  = (int32_t)st.flipArg;
    s.cur  = st.currentBuffer;
    s.pend = st.numFlipPending;
    s.gpu  = st.numGpuFlipPending;
    if (g_ntrace > 0) {
        Sample& q = g_trace[g_ntrace - 1];
        if (q.l0 == s.l0 && q.l1 == s.l1 && q.l2 == s.l2 && q.arg == s.arg &&
            q.cur == s.cur && q.pend == s.pend && q.gpu == s.gpu) return;   // changes only
    }
    if (g_ntrace < (int)(sizeof(g_trace) / sizeof(g_trace[0]))) g_trace[g_ntrace++] = s;
}

static void trace_dump() {
    for (int i = 0; i < g_ntrace; i++) {
        Sample& s = g_trace[i];
        logf("    t=%d l0=%d l1=%d l2=%d arg=%d cur=%d pend=%d gpu=%d\n",
             (int)s.t, (int)s.l0, (int)s.l1, (int)s.l2, s.arg, s.cur, s.pend, s.gpu);
    }
}

// ---------------------------------------------------------------- tests
// FLIP-1: lifecycle of a single EOP flip.
static void test_flip_basic() {
    logf("[FLIP-1] single EOP flip label lifecycle\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }

    trace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    trace_sample(t0);

    int rc = submit_eop_flip(0, 901);
    logf("  submit rc=%X\n", (unsigned long long)(uint32_t)rc);
    if (rc != 0) { logf("  FAIL submit rejected\n"); return; }

    for (int i = 0; i < 10000; i++) {
        trace_sample(t0);
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_vo, &st);
        if (st.flipArg == 901 && st.numFlipPending <= 0) break;
        sceKernelUsleep(200);
    }
    trace_sample(t0);
    trace_dump();
    logf("  [DATA] label0_after_own_flip=%d (displayed buffer expected to stay locked)\n",
         (int)*vo_label(0));
}

// FLIP-2: the decisive probe.
// A->buf0, B->buf1, C->buf0 back to back with no WaitFlipDone, so buf0 is
// re-armed (label driven back to 1 by C) while it is still displayed. When B is
// presented buf0 stops being displayed: an unconditional label clear reads 0
// even though C is pending; a conditional clear leaves it at 1.
static void test_flip_rearm() {
    logf("[FLIP-2] re-armed buffer replaced\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }

    trace_reset();
    uint64_t t0 = sceKernelGetProcessTime();
    trace_sample(t0);

    int rA = submit_eop_flip(0, 1001); trace_sample(t0);
    int rB = submit_eop_flip(1, 1002); trace_sample(t0);
    int rC = submit_eop_flip(0, 1003); trace_sample(t0);
    logf("  submit rc: A=%X B=%X C=%X\n", (unsigned long long)(uint32_t)rA,
         (unsigned long long)(uint32_t)rB, (unsigned long long)(uint32_t)rC);
    if (rA != 0 || rB != 0 || rC != 0) { logf("  FAIL submit rejected\n"); return; }

    // captured live so the answer never depends on trace-buffer capacity
    int l0_at_B = -1, l0_late_B = -1;
    uint64_t t_B = 0;
    for (int i = 0; i < 15000; i++) {
        trace_sample(t0);
        VideoFlipStatus st; my_memset(&st, 0, sizeof(st));
        sceVideoOutGetFlipStatus(g_vo, &st);
        if (l0_at_B < 0 && st.flipArg == 1002) {
            l0_at_B = (int)*vo_label(0);
            t_B = sceKernelGetProcessTime();
        }
        if (l0_at_B >= 0 && l0_late_B < 0 && (sceKernelGetProcessTime() - t_B) >= 3000u)
            l0_late_B = (int)*vo_label(0);
        if (st.flipArg == 1003 && l0_late_B >= 0) break;
        sceKernelUsleep(200);
    }
    trace_sample(t0);
    trace_dump();
    logf("  [DATA] label0_at_B=%d label0_3ms_after_B=%d\n", l0_at_B, l0_late_B);
    logf("  [DATA] 1=re-arm survives (conditional clear), 0=unconditional clear\n");
    wait_settled(2000000);
}

// FLIP-3: how deep the flip queue goes and what the full condition returns.
// PatchFlipRequest maps sceVideoOutSubmitEopFlip's 0x80290012 to 0x80D11081.
static void test_flip_queue_full() {
    logf("[FLIP-3] flip queue depth and full return code\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }

    int n_ok = 0, first_err = 0;
    for (int i = 0; i < 32; i++) {
        int rc = submit_eop_flip(i % NBUF, 2000 + i);
        if (rc == 0) { n_ok++; continue; }
        first_err = rc;
        break;
    }
    logf("  [DATA] accepted=%d first_err=%X (0x80D11081 = queue full)\n",
         n_ok, (unsigned long long)(uint32_t)first_err);
    wait_settled(4000000);
}

// FLIP-4: sustained rapid reuse. This is the shape that makes shadPS4's
// "Out of order flip IRQ" assert fire: the label of a re-armed buffer is
// cleared by the presenter between the GPU write and the flip marker. On
// hardware it is simply a throughput test.
static void test_flip_stress() {
    logf("[FLIP-4] sustained rapid buffer reuse\n");
    if (!wait_settled(2000000)) { logf("  SKIP prior flips never settled\n"); return; }

    const int TARGET = 600;
    int sent = 0, full = 0, err = 0, last_err = 0;
    uint64_t t0 = sceKernelGetProcessTime();

    for (int i = 0; i < TARGET; i++) {
        int rc = submit_eop_flip(i % NBUF, 3000 + i);
        if (rc == 0) { sent++; continue; }
        if ((uint32_t)rc == 0x80D11081u) {                    // queue full: back off
            full++;
            sceKernelUsleep(2000);
            i--;
            continue;
        }
        err++; last_err = rc;
        break;
    }
    uint64_t dt = sceKernelGetProcessTime() - t0;
    wait_settled(5000000);

    logf("  [DATA] sent=%d queue_full_retries=%d errors=%d last_err=%X elapsed_us=%l\n",
         sent, full, err, (unsigned long long)(uint32_t)last_err, (long long)dt);
    logf("  [DATA] labels l0=%d l1=%d l2=%d\n",
         (int)*vo_label(0), (int)*vo_label(1), (int)*vo_label(2));
    logf("  [DATA] reached this line = no flip IRQ assert\n");
}

// ---------------------------------------------------------------- entry
int main(void) {
    sceUserServiceInitialize(0);
    log_init();

    logf("=== PS4 VO flip / IRQ test ===\n");

    if (!pool_init()) { logf("FATAL gpu pool alloc failed\n"); goto idle; }
    if (!vo_init())   { logf("FATAL video out init failed\n"); goto idle; }

    logf("vo_handle=%d label_base=%X buffers=%d\n",
         (int)g_vo, (unsigned long long)g_label_base, NBUF);

    // establish a displayed buffer before any test reads a label
    submit_eop_flip(0, 1);
    wait_settled(2000000);

    test_flip_basic();
    test_flip_rearm();
    test_flip_queue_full();
    test_flip_stress();

    logf("=== done, results in %s ===\n", g_log_path);

idle:
    for (;;) sceKernelUsleep(1000000);
    return 0;
}
