/* ORDCNT_TEST: DS_ORDERED_COUNT on the PS4 GPU (GCN 1.1), run once before the main loop.
 *
 * Each test is one command buffer: GDS init (CP DMA from memory), one or two compute dispatches
 * (cs_ordcnt.h variant, sceGnmDispatchDirect flags 0x08 = ORDERED_APPEND_ENBL, 0x18 = + MODE),
 * CS_PARTIAL_FLUSH, GDS readback (CP DMA to memory), L2 write-back, fence. Queue 0 is the DCB
 * (graphics ring), queue 1 a compute queue mapped here (sceGnmMapComputeQueue + DingDong).
 *
 * Every wave writes a 768-byte record (src/../tools/gen_cs_ordcnt.py HEADER): TG_SIZE, TGID, wave
 * in group, M0, s_memtime around each op, a GDS ticket after each op (plain ds_add_rtn, global
 * completion order), HW_ID, and the op1 / op2 return VGPR of all 64 lanes.
 *
 * Results: ShadCube4 ordcnt.log next to the trace log (appended). A hang (fence timeout) ends the
 * run; the state file ordcnt.state makes the next launch log it and continue after it. If T01
 * (A01) hangs, the other tests of that queue are skipped and the risky ones use the other. Risky
 * tests (R*) can hang or desync the ordered wave IDs; each is followed by a probe (P*). A new
 * OC_BUILD starts over (log truncated); after the last test the run is not repeated until
 * ordcnt.state is deleted. */
#include "cs_ordcnt.h"

extern int sceGnmDispatchDirect(uint32_t* cmd, uint32_t size, uint32_t tgx, uint32_t tgy,
                                uint32_t tgz, uint32_t flags);
extern int sceGnmDispatchInitDefaultHardwareState(uint32_t* cmd, uint32_t size);
extern void sceGnmDingDong(uint32_t vqid, uint32_t next_offs_dw);

#define OC_BUILD "ordcnt-2"
#define OC_TIMEOUT_US 2000000u

#define OC_OUT_SIZE 0x400000u /* params, GDS images, records */
#define OC_GDS_INIT 0x1000u   /* GDS init image (CPU-written, copied to GDS) */
#define OC_GDS_DUMP 0x1400u   /* GDS after the dispatches */
#define OC_GDS_BYTES 0x400u
#define OC_REC_OFF 0x2000u
#define OC_REC_BYTES 0x300u
#define OC_MAX_WAVES ((OC_OUT_SIZE - OC_REC_OFF) / OC_REC_BYTES)
#define OC_TICKET 0x3F0u
#define OC_RING_DW 0x4000u
#define OC_BUF_SIZE 0x80000u /* log text, flushed per test */

/* CP_COHER_CNTL (gfx_7_2_sh_mask.h) */
#define OC_COHER_TC_WB (1u << 18)
#define OC_COHER_TCL1 (1u << 22)
#define OC_COHER_TC (1u << 23)
#define OC_COHER_KCACHE (1u << 27)
#define OC_COHER_ICACHE (1u << 29)

/* Parameter block (s16..s39 in the shader). */
enum {
    OC_P_GX,
    OC_P_WPT,
    OC_P_ADDR1,
    OC_P_ADDR2,
    OC_P_YSTRIDE1,
    OC_P_PRE_DELAY,
    OC_P_MID_DELAY,
    OC_P_VAL1,
    OC_P_LANE_MUL,
    OC_P_SLOT_MUL,
    OC_P_EXEC_LO,
    OC_P_EXEC_HI,
    OC_P_TICKET,
    OC_P_WAVES,
    OC_P_M0MODE,
    OC_P_SKIP_ODD,
    OC_P_REC_BASE,
    OC_P_VAL2,
    OC_P_YSTRIDE2,
    OC_P_ALT,
};

/* Record header dwords (gen_cs_ordcnt.py HEADER). */
enum {
    OC_H_TGSIZE,
    OC_H_TGX,
    OC_H_TGY,
    OC_H_WAVE,
    OC_H_SLOT,
    OC_H_TERM,
    OC_H_M0A,
    OC_H_M0B,
    OC_H_T0,
    OC_H_TB1 = 10,
    OC_H_TA1 = 12,
    OC_H_TB2 = 14,
    OC_H_TA2 = 16,
    OC_H_TK1 = 18,
    OC_H_TK2,
    OC_H_MARK,
    OC_H_HWID,
    OC_H_EXLO,
    OC_H_EXHI,
    OC_H_SKIP,
};

struct OcTest {
    const char* id;
    const char* what;
    uint8_t variant, queue, flags, kind; /* kind: 0 safe, 1 risky, 2 probe */
    uint8_t gate; /* a hang here means the queue cannot run ordered dispatches: skip its others */
    uint16_t gx, gy;
    uint8_t wpt, ndisp, gds_init, lanes, m0mode, skip_odd;
    uint32_t gds_val, addr1, addr2, ystride1, pre_delay, mid_delay, val1, lane_mul, slot_mul, val2,
        alt;
    uint64_t exec;
};

#define OC_T(...)                                                                                  \
    {                                                                                              \
        .gx = 4, .gy = 2, .wpt = 2, .ndisp = 1, .flags = 0x08, .variant = CS_OC_A, .val1 = 1,      \
        .val2 = 1, .exec = 1, __VA_ARGS__                                                          \
    }
#define OC_PROBE(n) OC_T(.id = n, .what = "probe: as T02", .kind = 2)

/* OC_T defaults, then the test's own fields: later designated initializers win (C11 6.7.9). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
static const struct OcTest k_oc_tests[] = {
    OC_T(.id = "T00", .variant = CS_OC_PLAIN, .flags = 0, .exec = ~0ULL, .lane_mul = 1,
         .gds_init = 1, .addr1 = 0x40, .lanes = 1,
         .what = "plain ds_add_rtn_u32 gds, no ordered append: GDS access, CP DMA init/readback"),
    OC_T(.id = "T01", .gate = 1, .pre_delay = 40,
         .what = "1 op (idx0 rel done), later waves arrive first"),
    OC_T(.id = "T02", .what = "1 op, no delay (wave-ID continuity across submits)"),
    OC_T(.id = "T03", .gx = 16, .gy = 1, .wpt = 1, .what = "1 op, 1-wave threadgroups"),
    OC_T(.id = "T04", .gx = 2, .gy = 2, .wpt = 4, .what = "1 op, 4-wave threadgroups"),
    OC_T(.id = "T05", .variant = CS_OC_B, .mid_delay = 200,
         .what = "2 ops idx0 rel / idx1 rel done, same GDS dword, odd waves sleep between"),
    OC_T(.id = "T06", .variant = CS_OC_B, .mid_delay = 200, .addr2 = 0x20,
         .what = "2 ops idx0 / idx1, op2 M0 address 0x20"),
    OC_T(.id = "T07", .variant = CS_OC_G, .mid_delay = 200,
         .what = "2 ops idx1 rel / idx0 rel done, same dword, odd waves sleep between"),
    OC_T(.id = "T08", .variant = CS_OC_C, .mid_delay = 200,
         .what = "2 ops idx0 no-release / idx0 rel done, same dword, odd waves sleep between"),
    OC_T(.id = "T09", .exec = ~0ULL, .lane_mul = 1, .lanes = 1,
         .what = "1 op, EXEC all 64, value lane+1"),
    OC_T(.id = "T10", .exec = 1ULL << 5, .lane_mul = 1, .lanes = 1,
         .what = "1 op, EXEC lane 5 only, value lane+1"),
    OC_T(.id = "T11", .exec = 0x8000000000000001ULL, .lane_mul = 1, .lanes = 1,
         .what = "1 op, EXEC lanes 0 and 63, value lane+1"),
    OC_T(.id = "T12", .variant = CS_OC_D, .val1 = 100, .slot_mul = 1, .gds_init = 2,
         .gds_val = 0x77, .what = "1 op swap, value 100+slot, GDS init 0x77"),
    OC_T(.id = "T13", .variant = CS_OC_E1, .alt = 1000,
         .what = "ADDR field = 1000, DATA0 field = 1: which one is added"),
    OC_T(.id = "T14", .variant = CS_OC_E2, .alt = 1000,
         .what = "ADDR field = 1, DATA0 field = 1000: which one is added"),
    OC_T(.id = "T15", .variant = CS_OC_F1, .addr1 = 0x10, .what = "idx1, M0 address 0x10"),
    OC_T(.id = "T16", .variant = CS_OC_F2, .addr1 = 0x10, .what = "idx2, M0 address 0x10"),
    OC_T(.id = "T17", .variant = CS_OC_F3, .addr1 = 0x10, .what = "idx3, M0 address 0x10"),
    OC_T(.id = "T18", .addr1 = 0x10, .what = "idx0, M0 address 0x10 (bytes or dwords)"),
    OC_T(.id = "T19", .gy = 4, .wpt = 1, .addr1 = 0x20, .ystride1 = 0x10,
         .what = "M0 address 0x20 + TGID.y * 0x10"),
    OC_T(.id = "T20", .ndisp = 2, .what = "2 dispatches in one command buffer"),
    OC_T(.id = "T21", .gx = 1024, .gy = 4, .wpt = 1, .what = "4096 waves (wave-ID wrap)"),
    OC_T(.id = "T22", .m0mode = 3, .what = "M0[15:0] = TG_SIZE[17:6] (12 bits)"),
    OC_T(.id = "T23", .variant = CS_OC_B, .pre_delay = 40, .mid_delay = 200,
         .what = "2 ops idx0 / idx1, later waves arrive first, odd waves sleep between"),
    OC_T(.id = "A01", .queue = 1, .gate = 1, .pre_delay = 40, .what = "compute queue: as T01"),
    OC_T(.id = "A02", .queue = 1, .what = "compute queue: as T02"),
    OC_T(.id = "A03", .queue = 1, .variant = CS_OC_B, .mid_delay = 200,
         .what = "compute queue: as T05"),
    OC_T(.id = "A04", .queue = 1, .variant = CS_OC_C, .mid_delay = 200,
         .what = "compute queue: as T08"),
    OC_T(.id = "A05", .queue = 1, .gx = 256, .gy = 4, .wpt = 1,
         .what = "compute queue: 1024 waves"),
    OC_T(.id = "R01", .kind = 1, .flags = 0x18, .what = "ORDERED_APPEND_MODE (flags 0x18)"),
    OC_PROBE("P01"),
    OC_T(.id = "R02", .kind = 1, .flags = 0x18, .gx = 8, .gy = 1, .wpt = 1,
         .what = "flags 0x18, 1-wave threadgroups"),
    OC_PROBE("P02"),
    OC_T(.id = "R03", .kind = 1, .variant = CS_OC_R2, .mid_delay = 200,
         .what = "2 ops, both idx0 with release"),
    OC_PROBE("P03"),
    OC_T(.id = "R04", .kind = 1, .variant = CS_OC_R3, .what = "1 op idx0 release, no done"),
    OC_PROBE("P04"),
    OC_T(.id = "R05", .kind = 1, .m0mode = 1, .what = "M0[15:0] = 0"),
    OC_PROBE("P05"),
    OC_T(.id = "R06", .kind = 1, .m0mode = 2, .what = "M0[15:0] = wave ID + 1"),
    OC_PROBE("P06"),
    OC_T(.id = "R07", .kind = 1, .exec = 0, .what = "op with EXEC = 0"),
    OC_PROBE("P07"),
    OC_T(.id = "R08", .kind = 1, .skip_odd = 1, .what = "odd waves branch over the op"),
    OC_PROBE("P08"),
    OC_T(.id = "R09", .kind = 1, .variant = CS_OC_R4, .what = "idx4"),
    OC_PROBE("P09"),
    OC_T(.id = "R10", .kind = 1, .variant = CS_OC_R5, .what = "idx15"),
    OC_PROBE("P10"),
    OC_T(.id = "R11", .kind = 1, .variant = CS_OC_R6, .what = "offset1 shader type 1 (PS) in a CS"),
    OC_PROBE("P11"),
    OC_T(.id = "R12", .kind = 1, .flags = 0, .what = "ordered op without ORDERED_APPEND_ENBL"),
    OC_PROBE("P12"),
    OC_T(.id = "R13", .kind = 1, .variant = CS_OC_R7, .what = "1 op idx0 no release, no done"),
    OC_PROBE("P13"),
};
#pragma clang diagnostic pop
#define OC_NTESTS ((int)(sizeof(k_oc_tests) / sizeof(k_oc_tests[0])))

/* ---- log ---- */
static int g_oc_fd = -1;
static char g_oc_buf[OC_BUF_SIZE];
/* The log and the state file sit next to the trace log (trace_init's path). */
static char g_oc_log_path[160], g_oc_state_path[160];
static void oc_paths(void) {
    const char* t = g_trace_path ? g_trace_path : "";
    int dir = 0;
    for (int i = 0; t[i] && i < 120; i++)
        if (t[i] == '/')
            dir = i + 1;
    static const char log_name[] = "ShadCube4 ordcnt.log", state_name[] = "ordcnt.state";
    for (int i = 0; i < dir; i++)
        g_oc_log_path[i] = g_oc_state_path[i] = t[i];
    for (int i = 0; i < (int)sizeof(log_name); i++)
        g_oc_log_path[dir + i] = log_name[i];
    for (int i = 0; i < (int)sizeof(state_name); i++)
        g_oc_state_path[dir + i] = state_name[i];
}
/* trace: "ordcnt: <what> <path> ret <code>" */
static void oc_trace_ret(const char* what, const char* path, long long ret) {
    char L[256];
    int p = 0;
    for (const char* q = "ordcnt: "; *q; q++)
        L[p++] = *q;
    for (const char* q = what; *q && p < 60; q++)
        L[p++] = *q;
    L[p++] = ' ';
    for (const char* q = path; *q && p < 220; q++)
        L[p++] = *q;
    for (const char* q = " ret "; *q; q++)
        L[p++] = *q;
    p += lg_hex(L + p, (unsigned long long)(uint32_t)ret);
    L[p++] = '\n';
    trace_line(L, (unsigned long)p);
}
static unsigned g_oc_len;

static void oc_flush(void) {
    if (g_oc_fd >= 0 && g_oc_len) {
        sceKernelWrite(g_oc_fd, g_oc_buf, g_oc_len);
        sceKernelFsync(g_oc_fd);
    }
    g_oc_len = 0;
}
static void oc_s(const char* s) {
    while (*s) {
        if (g_oc_len >= OC_BUF_SIZE - 1)
            oc_flush();
        g_oc_buf[g_oc_len++] = *s++;
    }
}
static void oc_u(unsigned long long v) {
    char t[24];
    t[lg_u64(t, v)] = 0;
    oc_s(t);
}
static void oc_i(long long v) {
    char t[24];
    t[lg_i64(t, v)] = 0;
    oc_s(t);
}
static void oc_x(unsigned long long v, int digits) {
    static const char h[] = "0123456789abcdef";
    char t[20];
    int n = 0;
    t[n++] = '0';
    t[n++] = 'x';
    for (int i = digits - 1; i >= 0; i--)
        t[n++] = h[(v >> (i * 4)) & 0xf];
    t[n] = 0;
    oc_s(t);
}

/* ---- state file: next test, test in flight, queues found hung ---- */
struct OcState {
    char build[16];
    int32_t next, inflight, bad[2];
};
static void oc_state_save(const struct OcState* st) {
    int fd = sceKernelOpen(g_oc_state_path, 0x601, 0x1FF);
    if (fd < 0) {
        oc_trace_ret("state open", g_oc_state_path, fd);
        return;
    }
    sceKernelWrite(fd, st, sizeof(*st));
    sceKernelFsync(fd);
    sceKernelClose(fd);
}
static int oc_state_load(struct OcState* st) {
    int fd = sceKernelOpen(g_oc_state_path, 0, 0);
    if (fd < 0)
        return 0;
    long n = sceKernelRead(fd, st, sizeof(*st));
    sceKernelClose(fd);
    if (n != (long)sizeof(*st))
        return 0;
    for (int i = 0; OC_BUILD[i] || st->build[i]; i++)
        if (OC_BUILD[i] != st->build[i])
            return 0;
    return 1;
}

/* ---- PM4 ---- */
static void oc_sh(struct PM4Builder* b, uint32_t reg, const uint32_t* v, uint32_t n) {
    pm4_emit(b, pm4_type3(PM4_SET_SH_REG, n + 1) | 2u); /* shader type: compute */
    pm4_emit(b, reg);
    for (uint32_t i = 0; i < n; i++)
        pm4_emit(b, v[i]);
}
/* DMA_DATA (cikd.h): src_sel 0 memory / 1 GDS, dst_sel 0 memory / 1 GDS, CP_SYNC. */
static void oc_dma(struct PM4Builder* b, uint32_t src_sel, uint64_t src, uint32_t dst_sel,
                   uint64_t dst, uint32_t bytes) {
    pm4_emit(b, pm4_type3(0x50, 6));
    pm4_emit(b, (1u << 31) | (src_sel << 29) | (dst_sel << 20));
    pm4_emit(b, (uint32_t)src);
    pm4_emit(b, (uint32_t)(src >> 32));
    pm4_emit(b, (uint32_t)dst);
    pm4_emit(b, (uint32_t)(dst >> 32));
    pm4_emit(b, bytes & 0x1FFFFFu);
}
static void oc_acquire(struct PM4Builder* b, uint32_t cntl) {
    pm4_emit(b, pm4_type3(PM4_ACQUIRE_MEM, 6));
    pm4_emit(b, cntl);
    pm4_emit(b, 0xFFFFFFFFu);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 0);
    pm4_emit(b, 10);
}
/* RELEASE_MEM (gfx_v7_0_ring_emit_fence_compute): TCL1 + TC action, CACHE_FLUSH_AND_INV_TS
   (0x14), event index 5, DATA_SEL 1 (32-bit), no interrupt. */
static void oc_release_mem(struct PM4Builder* b, volatile uint32_t* addr, uint32_t value) {
    uint64_t a = (uint64_t)(uintptr_t)addr;
    pm4_emit(b, pm4_type3(0x49, 6));
    pm4_emit(b, (1u << 16) | (1u << 17) | 0x14u | (5u << 8));
    pm4_emit(b, 1u << 29);
    pm4_emit(b, (uint32_t)a & 0xFFFFFFFCu);
    pm4_emit(b, (uint32_t)(a >> 32));
    pm4_emit(b, value);
    pm4_emit(b, 0);
}

struct OcCtx {
    uint8_t* out;
    void* sh[CS_OC_COUNT];
    uint32_t* dcb;
    volatile uint32_t* fence;
    uint32_t fv;
    uint32_t* ring;
    volatile uint32_t* rptr;
    int vqid;
    uint32_t wptr;
};

/* Builds one test's packets into b. */
static void oc_build(struct PM4Builder* b, struct OcCtx* c, const struct OcTest* t, int q) {
    uint32_t avail = b->cap - b->off;
    int w = q ? sceGnmDispatchInitDefaultHardwareState(b->buf + b->off, avail)
              : sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, avail);
    if (w > 0)
        b->off += (uint32_t)w;
    oc_acquire(b, OC_COHER_ICACHE | OC_COHER_KCACHE | OC_COHER_TCL1 | OC_COHER_TC);
    oc_dma(b, 0, (uint64_t)(uintptr_t)(c->out + OC_GDS_INIT), 1, 0, OC_GDS_BYTES);
    uint64_t sa = (uint64_t)(uintptr_t)c->sh[t->variant];
    uint32_t waves = (uint32_t)t->gx * t->gy * t->wpt;
    for (int d = 0; d < t->ndisp; d++) {
        uint8_t* base = c->out + d * 0x100;
        uint32_t v[4];
        build_vsharp(v, base, OC_OUT_SIZE - d * 0x100);
        static const uint32_t start[6] = {0, 0, 0, 0, 1, 1};
        uint32_t st[6];
        for (int i = 0; i < 6; i++)
            st[i] = start[i];
        st[3] = 64u * t->wpt;
        oc_sh(b, 0x204, st, 6);
        uint32_t pgm[2] = {(uint32_t)(sa >> 8), (uint32_t)(sa >> 40)};
        oc_sh(b, 0x20c, pgm, 2);
        uint32_t rsrc[2] = {CS_OC_RSRC1, CS_OC_RSRC2};
        oc_sh(b, 0x212, rsrc, 2);
        uint32_t tmp = 0;
        oc_sh(b, 0x218, &tmp, 1);
        oc_sh(b, 0x240, v, 4);
        (void)waves;
        if (pm4_have_space(b, 9)) {
            sceGnmDispatchDirect(b->buf + b->off, 9, t->gx, t->gy, 1, t->flags);
            b->off += 9;
        } else {
            b->overflow++;
        }
    }
    pm4_emit(b, pm4_type3(0x46, 1));
    pm4_emit(b, 0x07u | (4u << 8)); /* EVENT_WRITE CS_PARTIAL_FLUSH */
    oc_dma(b, 1, 0, 0, (uint64_t)(uintptr_t)(c->out + OC_GDS_DUMP), OC_GDS_BYTES);
    oc_acquire(b, OC_COHER_TC_WB | OC_COHER_TC | OC_COHER_TCL1);
    c->fv++;
    if (q)
        oc_release_mem(b, c->fence, c->fv);
    else
        pm4_event_write_eop(b, c->fence, c->fv);
}

static void oc_params(struct OcCtx* c, const struct OcTest* t) {
    uint32_t waves = (uint32_t)t->gx * t->gy * t->wpt;
    my_memset(c->out, 0, OC_REC_OFF + (unsigned long)waves * t->ndisp * OC_REC_BYTES);
    uint32_t* g = (uint32_t*)(c->out + OC_GDS_INIT);
    for (uint32_t i = 0; i < OC_GDS_BYTES / 4; i++)
        g[i] = t->gds_init == 1 ? 0x1000u + i : t->gds_init == 2 ? t->gds_val : 0;
    for (int d = 0; d < t->ndisp; d++) {
        uint32_t* p = (uint32_t*)(c->out + d * 0x100);
        p[OC_P_GX] = t->gx;
        p[OC_P_WPT] = t->wpt;
        p[OC_P_ADDR1] = t->addr1;
        p[OC_P_ADDR2] = t->addr2;
        p[OC_P_YSTRIDE1] = t->ystride1;
        p[OC_P_PRE_DELAY] = t->pre_delay;
        p[OC_P_MID_DELAY] = t->mid_delay;
        p[OC_P_VAL1] = t->val1;
        p[OC_P_LANE_MUL] = t->lane_mul;
        p[OC_P_SLOT_MUL] = t->slot_mul;
        p[OC_P_EXEC_LO] = (uint32_t)t->exec;
        p[OC_P_EXEC_HI] = (uint32_t)(t->exec >> 32);
        p[OC_P_TICKET] = OC_TICKET;
        p[OC_P_WAVES] = waves;
        p[OC_P_M0MODE] = t->m0mode;
        p[OC_P_SKIP_ODD] = t->skip_odd;
        p[OC_P_REC_BASE] = OC_REC_OFF - d * 0x100 + (uint32_t)d * waves * OC_REC_BYTES;
        p[OC_P_VAL2] = t->val2;
        p[OC_P_YSTRIDE2] = 0;
        p[OC_P_ALT] = t->alt;
    }
}

/* Returns 0 done, -1 fence timeout, -2 could not submit. */
static int oc_submit(struct OcCtx* c, const struct OcTest* t, int q, uint64_t* us) {
    struct PM4Builder b;
    if (q) {
        if (c->vqid <= 0 || c->wptr + 0x400 > OC_RING_DW)
            return -2;
        pm4_init(&b, c->ring + c->wptr, OC_RING_DW - c->wptr);
    } else {
        pm4_init(&b, c->dcb, 0x4000 / 4);
    }
    oc_build(&b, c, t, q);
    if (b.overflow)
        return -2;
    uint64_t t0 = sceKernelGetProcessTime();
    if (q) {
        c->wptr += b.off;
        sceGnmDingDong((uint32_t)c->vqid, c->wptr);
    } else {
        uint32_t sz = b.off * 4;
        void* a[1] = {c->dcb};
        if (sceGnmSubmitCommandBuffers(1, a, &sz, 0, 0) != 0)
            return -2;
        sceGnmSubmitDone();
    }
    while (*c->fence < c->fv) {
        if (sceKernelGetProcessTime() - t0 > OC_TIMEOUT_US)
            return -1;
        sceKernelUsleep(100);
    }
    *us = sceKernelGetProcessTime() - t0;
    return 0;
}

static uint32_t oc_ret_lane(uint64_t exec) {
    for (uint32_t i = 0; i < 64; i++)
        if (exec >> i & 1)
            return i;
    return 0;
}

/* Slots sorted by one return dword (col: record dword index), and the M0[15:0] step between
   neighbours in that order. */
static void oc_order(struct OcCtx* c, uint32_t waves, uint32_t col, const char* name) {
    static uint32_t ord[OC_MAX_WAVES];
    uint32_t n = 0;
    for (uint32_t s = 0; s < waves; s++) {
        const uint32_t* h = (const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES);
        if (h[OC_H_MARK] == 0x0DC0FFEEu && !h[OC_H_SKIP])
            ord[n++] = s;
    }
    for (uint32_t i = 1; i < n; i++) {
        uint32_t x = ord[i];
        uint32_t kx = ((const uint32_t*)(c->out + OC_REC_OFF + x * OC_REC_BYTES))[col];
        uint32_t j = i;
        while (j > 0) {
            uint32_t y = ord[j - 1];
            uint32_t ky = ((const uint32_t*)(c->out + OC_REC_OFF + y * OC_REC_BYTES))[col];
            if (ky <= kx)
                break;
            ord[j] = y;
            j--;
        }
        ord[j] = x;
    }
    oc_s("\nslots by ");
    oc_s(name);
    oc_s(":");
    for (uint32_t i = 0; i < n && i < 128; i++) {
        oc_s(" ");
        oc_u(ord[i]);
    }
    uint32_t step1 = 0, other = 0, dup = 0;
    oc_s("\nM0[15:0] steps along that order (other than +1):");
    for (uint32_t i = 1; i < n; i++) {
        const uint32_t* a = (const uint32_t*)(c->out + OC_REC_OFF + ord[i - 1] * OC_REC_BYTES);
        const uint32_t* b = (const uint32_t*)(c->out + OC_REC_OFF + ord[i] * OC_REC_BYTES);
        if (a[col] == b[col])
            dup++;
        if (b[OC_H_TERM] == a[OC_H_TERM] + 1) {
            step1++;
        } else {
            other++;
            if (other <= 32) {
                oc_s(" [");
                oc_u(i);
                oc_s("] ");
                oc_x(a[OC_H_TERM], 3);
                oc_s("->");
                oc_x(b[OC_H_TERM], 3);
            }
        }
    }
    oc_s("\n+1 steps ");
    oc_u(step1);
    oc_s(", other ");
    oc_u(other);
    oc_s(", equal neighbours ");
    oc_u(dup);
    oc_s("\n");
}

/* Logs one finished test: GDS changes, the wave table, lane dumps, order summary. */
static void oc_report(struct OcCtx* c, const struct OcTest* t, int q, uint64_t us) {
    uint32_t waves = (uint32_t)t->gx * t->gy * t->wpt * t->ndisp;
    const uint32_t* gi = (const uint32_t*)(c->out + OC_GDS_INIT);
    const uint32_t* gd = (const uint32_t*)(c->out + OC_GDS_DUMP);
    oc_s("done in ");
    oc_u(us);
    oc_s(" us\n\nGDS changed (byte offset: before -> after):");
    int nch = 0;
    for (uint32_t i = 0; i < OC_GDS_BYTES / 4; i++)
        if (gi[i] != gd[i]) {
            oc_s(nch++ % 4 ? "  " : "\n");
            oc_x(i * 4, 3);
            oc_s(": ");
            oc_x(gi[i], 8);
            oc_s(" -> ");
            oc_x(gd[i], 8);
        }
    if (!nch)
        oc_s(" none");
    oc_s("\n\n");
    uint32_t rl = oc_ret_lane(t->exec);
    uint32_t marks = 0;
    for (uint32_t s = 0; s < waves; s++)
        marks +=
            ((const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES))[OC_H_MARK] == 0x0DC0FFEEu;
    oc_s("records written: ");
    oc_u(marks);
    oc_s(" / ");
    oc_u(waves);
    oc_s(". ret = op return VGPR, lane ");
    oc_u(rl);
    oc_s("; tk = GDS ticket after the op; times: shader clocks from the earliest wave start\n\n");
    uint32_t t0 = 0xFFFFFFFFu;
    for (uint32_t s = 0; s < waves; s++) {
        const uint32_t* h = (const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES);
        if (h[OC_H_MARK] == 0x0DC0FFEEu && h[OC_H_T0] < t0)
            t0 = h[OC_H_T0];
    }
    oc_s("| slot | tgid x,y | wave | TG_SIZE | M0 op1 | M0 op2 | ret1 | ret2 | tk1 | tk2 | t op1 "
         "| t ret1 | t op2 | t ret2 | HW_ID |\n");
    oc_s("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    uint32_t rows = waves > 128 ? 128 : waves;
    for (uint32_t s = 0; s < rows; s++) {
        const uint32_t* h = (const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES);
        const uint32_t* r1 = h + 64;
        const uint32_t* r2 = h + 128;
        oc_s("| ");
        oc_u(s);
        if (h[OC_H_MARK] != 0x0DC0FFEEu) {
            oc_s(" | not written |||||||||||||\n");
            continue;
        }
        oc_s(" | ");
        oc_u(h[OC_H_TGX]);
        oc_s(",");
        oc_u(h[OC_H_TGY]);
        oc_s(" | ");
        oc_u(h[OC_H_WAVE]);
        oc_s(" | ");
        oc_x(h[OC_H_TGSIZE], 8);
        oc_s(" | ");
        oc_x(h[OC_H_M0A], 8);
        oc_s(" | ");
        oc_x(h[OC_H_M0B], 8);
        oc_s(" | ");
        oc_x(r1[rl], 8);
        oc_s(" | ");
        oc_x(r2[rl], 8);
        oc_s(" | ");
        oc_i((int32_t)h[OC_H_TK1]);
        oc_s(" | ");
        oc_i((int32_t)h[OC_H_TK2]);
        for (int k = 0; k < 4; k++) {
            uint32_t v = h[OC_H_TB1 + 2 * k];
            oc_s(" | ");
            if (v)
                oc_u(v - t0);
            else
                oc_s("-");
        }
        oc_s(" | ");
        oc_x(h[OC_H_HWID], 8);
        oc_s(" |\n");
    }
    if (rows < waves) {
        oc_s("(rows ");
        oc_u(rows);
        oc_s(".. omitted)\n");
    }
    if (t->lanes) {
        oc_s("\nop1 return, all 64 lanes (lane 0 first):\n");
        for (uint32_t s = 0; s < waves && s < 16; s++) {
            const uint32_t* r1 = (const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES) + 64;
            oc_s("slot ");
            oc_u(s);
            oc_s(":");
            for (int l = 0; l < 64; l++) {
                oc_s(" ");
                oc_x(r1[l], 8);
            }
            oc_s("\n");
        }
    }
    oc_order(c, waves, 64 + rl, "ret1");
    if (k_cs_oc[t->variant].has_op2)
        oc_order(c, waves, 128 + rl, "ret2");
    oc_s("\n");
}

/* Runs the remaining tests. Returns 0, or -1 after a fence timeout (the GPU is hung). */
static int ordcnt_run(void) {
    oc_paths();
    struct OcState st;
    int resumed = oc_state_load(&st);
    if (resumed && st.next >= OC_NTESTS && st.inflight < 0) {
        trace_msg("ordcnt: all tests done earlier; delete ordcnt.state to run them again\n");
        return 0;
    }
    if (!resumed) {
        my_memset(&st, 0, sizeof(st));
        for (int i = 0; OC_BUILD[i]; i++)
            st.build[i] = OC_BUILD[i];
        st.inflight = -1;
        resumed = 0;
    }
    g_oc_fd = sceKernelOpen(g_oc_log_path, resumed ? 0x209 : 0x601, 0x1FF);
    oc_trace_ret("open", g_oc_log_path, g_oc_fd);
    if (g_oc_fd < 0)
        return 0;
    oc_s(resumed ? "\n## resumed at "
                 : "# DS_ORDERED_COUNT on PS4 hardware, build " OC_BUILD "\n\n## start at ");
    oc_s(k_oc_tests[st.next].id);
    oc_s("\n\n");
    if (st.inflight >= 0) {
        const struct OcTest* t = &k_oc_tests[st.inflight];
        oc_s("### ");
        oc_s(t->id);
        oc_s(": HUNG - the app ended before its fence (no record)\n\n");
        if (t->gate)
            st.bad[t->queue] = 1;
        st.next = st.inflight + 1;
        st.inflight = -1;
    }
    struct OcCtx c;
    my_memset(&c, 0, sizeof(c));
    c.out = (uint8_t*)gpu_alloc_typed(OC_OUT_SIZE, 0x10000, MEM_TYPE_ONION);
    c.dcb = (uint32_t*)gpu_alloc_typed(0x4000, 0x4000, MEM_TYPE_ONION);
    c.fence = (volatile uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    c.ring = (uint32_t*)gpu_alloc_typed(OC_RING_DW * 4, 0x4000, MEM_TYPE_ONION);
    c.rptr = (volatile uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
    int ok = c.out && c.dcb && c.fence && c.ring && c.rptr;
    for (int i = 0; ok && i < CS_OC_COUNT; i++) {
        c.sh[i] = gpu_alloc_typed(k_cs_oc[i].size + 256, 0x1000, MEM_TYPE_ONION);
        if (!c.sh[i] || ((uint64_t)(uintptr_t)c.sh[i] >> 40))
            ok = 0;
        else
            my_memcpy(c.sh[i], k_cs_oc[i].bin, k_cs_oc[i].size);
    }
    if (!ok) {
        oc_s("allocation failed\n");
        oc_flush();
        return 0;
    }
    *c.fence = 0;
    c.vqid = sceGnmMapComputeQueue(0, 5, c.ring, OC_RING_DW, (void*)c.rptr);
    oc_s("compute queue: sceGnmMapComputeQueue(pipe 0, queue 5) = ");
    oc_x((uint32_t)c.vqid, 8);
    oc_s("\n\n");
    oc_flush();
    for (int k = st.next; k < OC_NTESTS; k++) {
        const struct OcTest* t = &k_oc_tests[k];
        int q = t->queue;
        if (t->kind && st.bad[0] && !st.bad[1])
            q = 1;
        uint32_t waves = (uint32_t)t->gx * t->gy * t->wpt * t->ndisp;
        oc_s("### ");
        oc_s(t->id);
        oc_s(": ");
        oc_s(t->what);
        oc_s("\nvariant ");
        oc_s(k_cs_oc[t->variant].name);
        oc_s(", ");
        oc_s(q ? "compute queue" : "DCB");
        oc_s(", flags ");
        oc_x(t->flags, 2);
        oc_s(", grid ");
        oc_u(t->gx);
        oc_s("x");
        oc_u(t->gy);
        oc_s(" x ");
        oc_u(t->wpt);
        oc_s(" waves x ");
        oc_u(t->ndisp);
        oc_s(" dispatch, EXEC ");
        oc_x(t->exec, 16);
        oc_s(", M0 mode ");
        oc_u(t->m0mode);
        oc_s("\n");
        if (st.bad[q] || waves > OC_MAX_WAVES) {
            oc_s("SKIPPED (this queue hung on its first ordered test, or too many waves)\n\n");
            oc_flush();
            st.next = k + 1;
            oc_state_save(&st);
            continue;
        }
        oc_params(&c, t);
        st.inflight = k;
        st.next = k;
        oc_state_save(&st);
        oc_flush();
        uint64_t us = 0;
        int r = oc_submit(&c, t, q, &us);
        if (r == -1) {
            oc_s("TIMEOUT: no fence after 2 s - GPU hung; close the app, relaunch to continue\n\n");
            if (t->gate)
                st.bad[q] = 1;
            st.inflight = -1;
            st.next = k + 1;
            oc_state_save(&st);
            oc_flush();
            return -1;
        }
        if (r == -2)
            oc_s("NOT SUBMITTED (no queue / ring space / submit error)\n\n");
        else
            oc_report(&c, t, q, us);
        st.inflight = -1;
        st.next = k + 1;
        oc_state_save(&st);
        oc_flush();
    }
    oc_s("## all tests done\n");
    oc_flush();
    return 0;
}
