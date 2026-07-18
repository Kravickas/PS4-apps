/* IB chain hardware test — Liverpool GFX7 ground truth. Standalone OpenOrbis eboot.
 *
 * Pure CP packet-parsing test: only WRITE_DATA and INDIRECT_BUFFER.
 * No shaders, no draws, no state, no EOP.
 *
 * Encodings verified against shadPS4 pm4_cmds.h / liverpool.cpp, PAL
 * si_ci_vi_merged_pm4defs.h (GFX7/CI field layout) and gfx6CmdUtil.cpp:1983
 * (chain semantics), and libSceGnmDriver.sprx 12.02 (submitted DCB is IB1).
 * The IB dword builder reproduces real RDR2 packet dwords bit-for-bit.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ---- OpenOrbis SDK (exact prototypes from include/orbis) ------------------ */
typedef int64_t off_t;

extern int32_t sceKernelAllocateDirectMemory(off_t start, off_t end, size_t len, size_t align,
                                             int32_t type, off_t* phys_out);
extern int32_t sceKernelMapDirectMemory(void** addr, size_t len, int32_t prot, int32_t flags,
                                        off_t phys, size_t align);
extern int32_t sceKernelUsleep(uint32_t us);
extern int32_t sceKernelOpen(const char* path, int32_t flags, uint16_t mode);
extern size_t sceKernelWrite(int32_t fd, const void* buf, size_t n);
extern int32_t sceKernelClose(int32_t fd);

extern int32_t sceGnmSubmitCommandBuffers(uint32_t count, void* dcbaddrs[], uint32_t* dcbbytesizes,
                                          void* ccbaddrs[], uint32_t* ccbbytesizes);
extern int32_t sceGnmSubmitDone(void);

/* verified constants */
#define PROT_CPU_RW 0x03
#define PROT_GPU_RW 0x30 /* GPU_READ 0x10 | GPU_WRITE 0x20 */
#define MEM_WB_ONION 0x0 /* CPU+GPU coherent — required for CPU readback */
#define MEM_WC_GARLIC 0x3

#define O_WRONLY 0x0001
#define O_CREAT 0x0200
#define O_APPEND 0x0008

/* ---- minimal libc (nostdlib build) --------------------------------------- */
static size_t z_strlen(const char* s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
static char* z_utoa_hex(uint32_t v, char* out, int width) {
    static const char h[] = "0123456789abcdef";
    char tmp[8];
    int i = 0;
    for (i = 0; i < 8; i++) {
        tmp[i] = h[v & 0xF];
        v >>= 4;
    }
    int n = 8;
    while (n > width && tmp[n - 1] == '0')
        n--;
    for (int k = 0; k < n; k++)
        out[k] = tmp[n - 1 - k];
    return out + n;
}
static char* z_utoa_dec(uint32_t v, char* out) {
    char tmp[10];
    int i = 0;
    if (v == 0) {
        *out++ = '0';
        return out;
    }
    while (v) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i--)
        *out++ = tmp[i];
    return out;
}

/* ---- logging to /data/ibchain_log.txt ------------------------------------ */
static int g_fd = -1;

static void log_open(void) {
    g_fd = sceKernelOpen("/data/ibchain_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
}

/* supports %s %u %08x %02u %x — enough for this test */
static void log_line(const char* fmt, ...) {
    char buf[512];
    char* o = buf;
    va_list ap;
    va_start(ap, fmt);
    for (const char* f = fmt; *f && o < buf + sizeof(buf) - 12; f++) {
        if (*f != '%') {
            *o++ = *f;
            continue;
        }
        f++;
        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }
        if (*f == 's') {
            const char* s = va_arg(ap, const char*);
            size_t n = z_strlen(s);
            for (size_t k = 0; k < n && o < buf + sizeof(buf) - 12; k++)
                *o++ = s[k];
        } else if (*f == 'u') {
            uint32_t v = va_arg(ap, uint32_t);
            char t[12];
            char* e = z_utoa_dec(v, t);
            for (char* p = t; p < e; p++)
                *o++ = *p;
        } else if (*f == 'x') {
            uint32_t v = va_arg(ap, uint32_t);
            char t[10];
            char* e = z_utoa_hex(v, t, width ? width : 1);
            for (char* p = t; p < e; p++)
                *o++ = *p;
        } else {
            *o++ = *f;
        }
    }
    va_end(ap);
    *o++ = '\n';
    if (g_fd >= 0)
        sceKernelWrite(g_fd, buf, (size_t)(o - buf));
}

/* flush a marker line and force it to disk before a potentially-hanging test */
static void log_progress(const char* stage, int idx) {
    log_line("PROGRESS %s test %u", stage, (uint32_t)idx);
    if (g_fd >= 0) {
        sceKernelClose(g_fd);
        g_fd = sceKernelOpen("/data/ibchain_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
    }
}

/* ---- GPU memory allocation ----------------------------------------------- */
static void* gpu_alloc(uint64_t size, uint64_t align) {
    size = (size + 0x3FFF) & ~0x3FFFull; /* 16 KiB page round-up */
    off_t phys = 0;
    if (sceKernelAllocateDirectMemory(0, 0x100000000ull, size, align < 0x4000 ? 0x4000 : align,
                                      MEM_WB_ONION, &phys) != 0)
        return NULL;
    void* addr = NULL;
    if (sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW | PROT_GPU_RW, 0, phys, align) != 0)
        return NULL;
    return addr;
}

/* ---- PM4 ----------------------------------------------------------------- */
#define PM4_TYPE3(opcode, total_dw)                                                                \
    (0xC0000000u | ((((total_dw) - 2u) & 0x3FFFu) << 16) | (((opcode) & 0xFFu) << 8))
#define IT_NOP 0x10
#define IT_WRITE_DATA 0x37
#define IT_INDIRECT_BUFFER 0x3F

static uint32_t* pm4_write_dword(uint32_t* p, volatile uint32_t* dst, uint32_t val) {
    const uint64_t a = (uint64_t)(uintptr_t)dst;
    *p++ = PM4_TYPE3(IT_WRITE_DATA, 5);
    *p++ = (5u << 8) | (1u << 20); /* dst_sel=5 (memory), wr_confirm=1 */
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32);
    *p++ = val;
    return p;
}
static uint32_t* pm4_indirect_buffer(uint32_t* p, const uint32_t* target, uint32_t size_dw,
                                     uint32_t chain, uint32_t valid, uint32_t vmid) {
    const uint64_t a = (uint64_t)(uintptr_t)target;
    *p++ = PM4_TYPE3(IT_INDIRECT_BUFFER, 4);
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32) & 0xFFFFu;
    *p++ =
        (size_dw & 0xFFFFFu) | ((chain & 1u) << 20) | ((valid & 1u) << 23) | ((vmid & 0xFu) << 24);
    return p;
}

/* ---- result block -------------------------------------------------------- */
enum {
    R_A_MARKER = 0,
    R_B_MARKER = 1,
    R_A_TRAILING = 2,
    R_DONE_B = 3,
    R_DONE_A = 4,
    R_CHAIN0 = 5,
    R_SLOTS = 32
};

static volatile uint32_t* g_res;
static uint32_t* g_bufs[16];
static uint32_t g_buf_dw[16];

static void res_clear(void) {
    for (int i = 0; i < R_SLOTS; i++)
        g_res[i] = 0;
}
static void dump_buf(const char* name, const uint32_t* b, uint32_t dw) {
    log_line("  %s (%u dw):", name, dw);
    for (uint32_t i = 0; i < dw; i += 4)
        log_line("    +%02u: %08x %08x %08x %08x", i, b[i], (i + 1 < dw) ? b[i + 1] : 0,
                 (i + 2 < dw) ? b[i + 2] : 0, (i + 3 < dw) ? b[i + 3] : 0);
}
static int submit_and_wait(uint32_t* dcb, uint32_t dcb_dw, volatile uint32_t* done_flag) {
    void* a[1] = {dcb};
    uint32_t s[1] = {dcb_dw * 4};
    sceGnmSubmitCommandBuffers(1, a, s, NULL, NULL);
    sceGnmSubmitDone();
    for (int w = 0; w < 100000; w++) {
        if (*done_flag)
            break;
        sceKernelUsleep(10);
    }
    if (!*done_flag)
        return 1;
    sceKernelUsleep(50000);
    return 0;
}
static void report(const char* name) {
    log_line("  slots: A=%08x B=%08x TRAILING=%08x done_b=%u done_a=%u", g_res[R_A_MARKER],
             g_res[R_B_MARKER], g_res[R_A_TRAILING], g_res[R_DONE_B], g_res[R_DONE_A]);
    if (g_res[R_A_TRAILING] == 0 && g_res[R_DONE_A] == 0)
        log_line("  VERDICT %s: CP did NOT return -> chain is a JUMP", name);
    else if (g_res[R_A_TRAILING] == 0xDEAD0001)
        log_line("  VERDICT %s: CP DID return -> chain behaves as a CALL", name);
    else
        log_line("  VERDICT %s: INDETERMINATE", name);
}

/* ---- tests --------------------------------------------------------------- */
static void test1_chain_no_return(void) {
    log_line("TEST 1: chain=1 does not return (IB1 -> IB1)");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t* A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 1, 1, 0);
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0001); /* unreachable if jump */
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);

    dump_buf("A", A, g_buf_dw[0]);
    dump_buf("B", B, g_buf_dw[1]);
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  TIMEOUT - CP did not reach B");
    else
        report("1");
}

static void test2_chain0_returns(void) {
    log_line("TEST 2: chain=0 launches IB2 and returns");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t* A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 0, 1, 0);
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0001); /* reachable: IB2 returns */
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);

    /* On the CALL path done_a is the last write, so poll it directly instead of
     * relying on the post-wait sleep. */
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_A]))
        log_line("  TIMEOUT - IB2 did not return (unexpected for chain=0)");
    else
        report("2");
}

static void test3_deep_chain(void) {
    log_line("TEST 3: 10-deep chain (chain=1 each)");
    res_clear();
    const int N = 10;
    for (int i = N - 1; i >= 0; i--) {
        uint32_t *b = g_bufs[i], *p = b;
        p = pm4_write_dword(p, &g_res[R_CHAIN0 + i], 0xC0DE0000u | (uint32_t)i);
        if (i == N - 1)
            p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
        else
            p = pm4_indirect_buffer(p, g_bufs[i + 1], g_buf_dw[i + 1], 1, 1, 0);
        g_buf_dw[i] = (uint32_t)(p - b);
    }
    if (submit_and_wait(g_bufs[0], g_buf_dw[0], &g_res[R_DONE_B])) {
        log_line("  TIMEOUT - deep chain hung");
        return;
    }
    int ok = 1;
    for (int i = 0; i < N; i++) {
        uint32_t want = 0xC0DE0000u | (uint32_t)i;
        log_line("  depth %2u: %08x %s", (uint32_t)i, g_res[R_CHAIN0 + i],
                 g_res[R_CHAIN0 + i] == want ? "ok" : "MISSING");
        if (g_res[R_CHAIN0 + i] != want)
            ok = 0;
    }
    log_line("  VERDICT 3: %s",
             ok ? "all 10 executed -> stackless JUMP" : "chain broke before depth 10");
}

static void test4_trailing_bytes(uint32_t filler, const char* what) {
    log_line("TEST: chain=1 followed by %s", what);
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t* A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 1, 1, 0);
    for (int i = 0; i < 8; i++)
        *p++ = filler;
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  TIMEOUT / fault - CP parsed the trailing %s", what);
    else
        log_line("  OK - B ran (B=%08x), trailing %s never parsed", g_res[R_B_MARKER], what);
}

/* bad paths — may hang; progress is flushed before each */
static void test7_valid_zero(void) {
    log_line("TEST 7 [BAD PATH]: IB valid=0");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);
    uint32_t* A = g_bufs[0];
    p = A;
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 1, 0, 0);
    g_buf_dw[0] = (uint32_t)(p - A);
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  RESULT: valid=0 -> IB NOT executed (or hung)");
    else
        log_line("  RESULT: valid=0 -> executed anyway (B=%08x)", g_res[R_B_MARKER]);
}
static void test8_size_zero(void) {
    log_line("TEST 8 [BAD PATH]: IB ib_size=0");
    res_clear();
    uint32_t *A = g_bufs[0], *p = A;
    p = pm4_indirect_buffer(p, g_bufs[1], 0, 1, 1, 0);
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0008);
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_A]))
        log_line("  RESULT: ib_size=0 -> hung or stalled");
    else
        log_line("  RESULT: ib_size=0 -> survived, trailing=%08x", g_res[R_A_TRAILING]);
}
__attribute__((unused)) static void test9_ib3_hang(void) {
    log_line("TEST 9 [DESTRUCTIVE]: IB3 attempt - expected HANG");
    res_clear();
    uint32_t *C = g_bufs[2], *p = C;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xCCCCCCCC);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[2] = (uint32_t)(p - C);
    uint32_t* B = g_bufs[1];
    p = B;
    p = pm4_indirect_buffer(p, C, g_buf_dw[2], 0, 1, 0);
    g_buf_dw[1] = (uint32_t)(p - B);
    uint32_t* A = g_bufs[0];
    p = A;
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 0, 1, 0);
    g_buf_dw[0] = (uint32_t)(p - A);
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  RESULT: HUNG as documented - CP has no IB3");
    else
        log_line("  RESULT: IB3 executed (C=%08x) - CP DOES support IB3", g_res[R_B_MARKER]);
}

/* ---- entry --------------------------------------------------------------- */
/* Set to 1 only for the destructive IB3 run (expect to reboot the console). */
#ifndef RUN_DESTRUCTIVE
#define RUN_DESTRUCTIVE 0
#endif

int main(void) {
    log_open();
    log_line("=== IB chain tests: submitted DCB executes as IB1 (verified by disasm) ===");

    g_res = (volatile uint32_t*)gpu_alloc(R_SLOTS * 4, 256);
    int alloc_ok = (g_res != NULL);
    for (int i = 0; i < 16; i++) {
        g_bufs[i] = (uint32_t*)gpu_alloc(256 * 4, 256);
        if (!g_bufs[i])
            alloc_ok = 0;
    }
    if (!alloc_ok) {
        log_line("FATAL: gpu_alloc failed");
        if (g_fd >= 0)
            sceKernelClose(g_fd);
        return 1;
    }

    log_progress("start", 1);
    test1_chain_no_return();
    log_progress("start", 2);
    test2_chain0_returns();
    log_progress("start", 3);
    test3_deep_chain();
    log_progress("start", 4);
    test4_trailing_bytes(0x00000000u, "zero padding");
    log_progress("start", 5);
    test4_trailing_bytes(0xDEADBEEFu, "garbage");

    log_progress("start", 7);
    test7_valid_zero();
    log_progress("start", 8);
    test8_size_zero();

#if RUN_DESTRUCTIVE
    log_progress("start", 9);
    test9_ib3_hang();
#endif

    log_line("=== all tests done ===");
    if (g_fd >= 0)
        sceKernelClose(g_fd);
    return 0;
}
