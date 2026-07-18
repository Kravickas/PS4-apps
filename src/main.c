/* IB chain hardware test — Liverpool GFX7 ground truth.
 *
 * Pure CP packet-parsing test: only WRITE_DATA and INDIRECT_BUFFER.
 * No shaders, no draws, no state, no EOP. Nothing that can fail for an
 * unrelated reason.
 *
 * Encodings verified against:
 *   shadPS4 pm4_cmds.h / pm4_opcodes.h / liverpool.cpp (dst_sel assert)
 *   PAL si_ci_vi_merged_pm4defs.h  (GFX7/CI IB field layout)
 *   PAL gfx6CmdUtil.cpp:1983       (chain semantics)
 *   libSceGnmDriver.sprx 12.02     (submitted DCB executes as IB1)
 */

#include <stddef.h>
#include <stdint.h>

/* ---- provided by the host project (CUBETST00) ---------------------------- */
extern void *gpu_alloc(uint64_t size, uint64_t align);
extern int sceGnmSubmitCommandBuffers(uint32_t count, void *dcb[], uint32_t dcb_sz[],
                                      void *ccb[], uint32_t ccb_sz[]);
extern int sceGnmSubmitDone(void);
extern int sceKernelUsleep(uint32_t us);
extern void log_line(const char *fmt, ...); /* writes to the result file */

/* ---- PM4 -----------------------------------------------------------------
 * type3 total dwords = count + 2.
 */
#define PM4_TYPE3(opcode, total_dw)                                                                \
    (0xC0000000u | ((((total_dw) - 2u) & 0x3FFFu) << 16) | (((opcode) & 0xFFu) << 8))

#define IT_NOP 0x10
#define IT_WRITE_DATA 0x37
#define IT_INDIRECT_BUFFER 0x3F

/* WRITE_DATA, 1 dword payload -> 5 dwords total -> count 3 -> 0xC0033700.
 * dst_sel bits 8-11 must be 2 or 5 (shadPS4 asserts); 5 = memory.
 * wr_confirm bit 20 = wait for write confirm.
 */
static uint32_t *pm4_write_dword(uint32_t *p, volatile uint32_t *dst, uint32_t val) {
    const uint64_t a = (uint64_t)(uintptr_t)dst;
    *p++ = PM4_TYPE3(IT_WRITE_DATA, 5);
    *p++ = (5u << 8) | (1u << 20); /* dst_sel=5 (memory), wr_confirm=1 */
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32);
    *p++ = val;
    return p;
}

/* INDIRECT_BUFFER -> 4 dwords total -> count 2 -> 0xC0023F00.
 * GFX7/CI dw3: ibSize:20 chain:1 offLoadPolling:1 volatile:1 valid:1
 *              vmid:4 cachePolicy:2 reserved:2
 * PAL sets CI.valid = 1 unconditionally.
 */
static uint32_t *pm4_indirect_buffer(uint32_t *p, const uint32_t *target, uint32_t size_dw,
                                     uint32_t chain, uint32_t valid, uint32_t vmid) {
    const uint64_t a = (uint64_t)(uintptr_t)target;
    *p++ = PM4_TYPE3(IT_INDIRECT_BUFFER, 4);
    *p++ = (uint32_t)(a & 0xFFFFFFFFu);
    *p++ = (uint32_t)(a >> 32) & 0xFFFFu;
    *p++ = (size_dw & 0xFFFFFu) | ((chain & 1u) << 20) | ((valid & 1u) << 23) |
           ((vmid & 0xFu) << 24);
    return p;
}

static uint32_t *pm4_nop(uint32_t *p, uint32_t n) {
    while (n--) {
        *p++ = PM4_TYPE3(IT_NOP, 2);
        *p++ = 0;
    }
    return p;
}

/* ---- result block --------------------------------------------------------- */
enum {
    R_A_MARKER = 0,  /* 0xAAAAAAAA - written in A before the IB packet   */
    R_B_MARKER = 1,  /* 0xBBBBBBBB - written in B (the chain target)     */
    R_A_TRAILING = 2,/* 0xDEAD0001 - written in A AFTER the IB  <-- KEY  */
    R_DONE_B = 3,    /* 1          - last packet in B                    */
    R_DONE_A = 4,    /* 1          - last packet in A's trailing section */
    R_CHAIN0 = 5,    /* depth markers for the deep-chain test            */
    R_SLOTS = 32
};

static volatile uint32_t *g_res;
static uint32_t *g_bufs[16];
static uint32_t g_buf_dw[16];

static void res_clear(void) {
    for (int i = 0; i < R_SLOTS; i++)
        g_res[i] = 0;
}

static void dump_buf(const char *name, const uint32_t *b, uint32_t dw) {
    log_line("  %s (%u dw):", name, dw);
    for (uint32_t i = 0; i < dw; i += 4)
        log_line("    +%02u: %08x %08x %08x %08x", i, b[i], (i + 1 < dw) ? b[i + 1] : 0,
                 (i + 2 < dw) ? b[i + 2] : 0, (i + 3 < dw) ? b[i + 3] : 0);
}

/* returns 0 = completed, 1 = timeout (CP likely hung) */
static int submit_and_wait(uint32_t *dcb, uint32_t dcb_dw, volatile uint32_t *done_flag) {
    void *a[1] = {dcb};
    uint32_t s[1] = {dcb_dw * 4};
    sceGnmSubmitCommandBuffers(1, a, s, NULL, NULL);
    sceGnmSubmitDone();
    for (int w = 0; w < 100000; w++) { /* ~1 s */
        if (*done_flag)
            break;
        sceKernelUsleep(10);
    }
    if (!*done_flag)
        return 1;
    sceKernelUsleep(50000); /* let any trailing packets retire */
    return 0;
}

static void report(const char *name) {
    log_line("  slots: A=%08x B=%08x TRAILING=%08x done_b=%u done_a=%u", g_res[R_A_MARKER],
             g_res[R_B_MARKER], g_res[R_A_TRAILING], g_res[R_DONE_B], g_res[R_DONE_A]);
    if (g_res[R_A_TRAILING] == 0 && g_res[R_DONE_A] == 0)
        log_line("  VERDICT %s: CP did NOT return -> chain is a JUMP", name);
    else if (g_res[R_A_TRAILING] == 0xDEAD0001)
        log_line("  VERDICT %s: CP DID return -> chain behaves as a CALL", name);
    else
        log_line("  VERDICT %s: INDETERMINATE", name);
}

/* ===== Test 1: chain=1 must NOT return ===================================== */
static void test1_chain_no_return(void) {
    log_line("TEST 1: chain=1 does not return (IB1 -> IB1)");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t *A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], /*chain=*/1, /*valid=*/1, /*vmid=*/0);
    /* unreachable if chain is a jump: */
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0001);
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);

    dump_buf("A", A, g_buf_dw[0]);
    dump_buf("B", B, g_buf_dw[1]);
    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  TIMEOUT - CP did not reach B");
    else
        report("1");
}

/* ===== Test 2: chain=0 launches an IB2 and DOES return ===================== */
static void test2_chain0_returns(void) {
    log_line("TEST 2: chain=0 launches IB2 and returns");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t *A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], /*chain=*/0, /*valid=*/1, /*vmid=*/0);
    /* reachable: an IB2 returns here */
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0001);
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  TIMEOUT");
    else
        report("2");
}

/* ===== Test 3: 10-deep chain =============================================
 * If chain were a call, depth 10 would exceed IB2 (there is no IB3) and hang.
 * RDR2 chains 65 deep on hardware without hanging.
 */
static void test3_deep_chain(void) {
    log_line("TEST 3: 10-deep chain (chain=1 each)");
    res_clear();
    const int N = 10;
    /* build last -> first so each knows its successor */
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
        log_line("  TIMEOUT - deep chain hung (would mean chain consumes a return stack)");
        return;
    }
    int ok = 1;
    for (int i = 0; i < N; i++) {
        uint32_t want = 0xC0DE0000u | (uint32_t)i;
        log_line("  depth %2d: %08x %s", i, g_res[R_CHAIN0 + i],
                 g_res[R_CHAIN0 + i] == want ? "ok" : "MISSING");
        if (g_res[R_CHAIN0 + i] != want)
            ok = 0;
    }
    log_line("  VERDICT 3: %s", ok ? "all 10 executed -> chain is a stackless JUMP"
                                   : "chain broke before depth 10");
}

/* ===== Test 4/5: trailing bytes after a chain IB are unreachable ========== */
static void test4_trailing_bytes(uint32_t filler, const char *what) {
    log_line("TEST: chain=1 followed by %s", what);
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t *A = g_bufs[0];
    p = A;
    p = pm4_write_dword(p, &g_res[R_A_MARKER], 0xAAAAAAAA);
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], 1, 1, 0);
    for (int i = 0; i < 8; i++)
        *p++ = filler; /* only parsed if the CP wrongly returns */
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  TIMEOUT / fault - CP parsed the trailing %s", what);
    else
        log_line("  OK - B ran, trailing %s never parsed (B=%08x)", what, g_res[R_B_MARKER]);
}

/* ===== BAD PATHS — each may hang. Run in isolation. ======================= */

/* Test 7: valid=0. PAL always sets valid=1 on CI. Undocumented otherwise. */
static void test7_valid_zero(void) {
    log_line("TEST 7 [BAD PATH]: IB with valid=0");
    res_clear();
    uint32_t *B = g_bufs[1], *p = B;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xBBBBBBBB);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t *A = g_bufs[0];
    p = A;
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], /*chain=*/1, /*valid=*/0, /*vmid=*/0);
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  RESULT: valid=0 -> IB NOT executed (or CP hung)");
    else
        log_line("  RESULT: valid=0 -> IB executed anyway (B=%08x)", g_res[R_B_MARKER]);
}

/* Test 8: ib_size = 0. PAL asserts size != 0. */
static void test8_size_zero(void) {
    log_line("TEST 8 [BAD PATH]: IB with ib_size=0");
    res_clear();
    uint32_t *A = g_bufs[0], *p = A;
    p = pm4_indirect_buffer(p, g_bufs[1], /*size_dw=*/0, 1, 1, 0);
    p = pm4_write_dword(p, &g_res[R_A_TRAILING], 0xDEAD0008);
    p = pm4_write_dword(p, &g_res[R_DONE_A], 1);
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_A]))
        log_line("  RESULT: ib_size=0 -> CP hung or stalled");
    else
        log_line("  RESULT: ib_size=0 -> survived, trailing=%08x", g_res[R_A_TRAILING]);
}

/* Test 9: IB3. Submitted DCB is IB1 -> chain=0 gives IB2 -> chain=0 gives IB3.
 * PAL: "will cause a hang because the CP does not support an IB3."
 * DESTRUCTIVE. Own eboot. Expect to reboot the console.
 */
static void test9_ib3_hang(void) {
    log_line("TEST 9 [DESTRUCTIVE]: IB3 attempt - expected to HANG the CP");
    res_clear();
    uint32_t *C = g_bufs[2], *p = C;
    p = pm4_write_dword(p, &g_res[R_B_MARKER], 0xCCCCCCCC);
    p = pm4_write_dword(p, &g_res[R_DONE_B], 1);
    g_buf_dw[2] = (uint32_t)(p - C);

    uint32_t *B = g_bufs[1];
    p = B;
    p = pm4_indirect_buffer(p, C, g_buf_dw[2], /*chain=*/0, 1, 0); /* IB2 -> IB3 */
    g_buf_dw[1] = (uint32_t)(p - B);

    uint32_t *A = g_bufs[0];
    p = A;
    p = pm4_indirect_buffer(p, B, g_buf_dw[1], /*chain=*/0, 1, 0); /* IB1 -> IB2 */
    g_buf_dw[0] = (uint32_t)(p - A);

    if (submit_and_wait(A, g_buf_dw[0], &g_res[R_DONE_B]))
        log_line("  RESULT: HUNG as documented - CP has no IB3");
    else
        log_line("  RESULT: IB3 executed (C=%08x) - CP DOES support IB3", g_res[R_B_MARKER]);
}

/* ---- entry --------------------------------------------------------------- */
void ibchain_run_safe_tests(void) {
    g_res = (volatile uint32_t *)gpu_alloc(R_SLOTS * 4, 256);
    for (int i = 0; i < 16; i++)
        g_bufs[i] = (uint32_t *)gpu_alloc(256 * 4, 256); /* separate allocations */

    log_line("=== IB chain tests: submitted DCB executes as IB1 (verified by disasm) ===");
    test1_chain_no_return();
    test2_chain0_returns();
    test3_deep_chain();
    test4_trailing_bytes(0x00000000u, "zero padding");
    test4_trailing_bytes(0xDEADBEEFu, "garbage");
    log_line("=== safe tests done ===");
}

void ibchain_run_bad_paths(void) {
    /* Each of these may hang. The harness must record progress to disk BEFORE
     * calling, so a hang identifies the culprit after reboot. */
    test7_valid_zero();
    test8_size_zero();
}

void ibchain_run_destructive(void) {
    test9_ib3_hang(); /* own eboot */
}
