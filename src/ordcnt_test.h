/* ORDCNT_TEST: DS_ORDERED_COUNT on the PS4 GPU (GCN 1.1), run once before the main loop.
 *
 * Each test is one command buffer: GDS init (CP DMA from memory), one or two compute dispatches
 * (cs_ordcnt.h variant, sceGnmDispatchDirect flags 0x08 = ORDERED_APPEND_ENBL, 0x18 = + MODE),
 * CS_PARTIAL_FLUSH, GDS readback (CP DMA to memory), L2 write-back, fence. Queue 0 is the DCB
 * (graphics ring), queues 1..3 compute queues mapped here (k_oc_queue; sceGnmMapComputeQueue +
 * DingDong).
 *
 * Every wave writes a 768-byte record (tools/gen_cs_ordcnt.py HEADER): TG_SIZE, TGID, wave in
 * group, M0, s_memtime around each op, a GDS ticket after each op (plain ds_add_rtn_u32 gds with
 * M0 = the test's m0plain; valid only where the G tests show that M0 works), HW_ID, and the
 * op1 / op2 / op3 return VGPR of all 64 lanes. The window shaders (gdump / gfill) instead read or
 * fill GDS 0..0xFFFF through 64 M0 windows; register tests copy GDS registers to memory with
 * COPY_DATA.
 *
 * Results: ShadCube4 ordcnt.log next to the trace log (appended). A hang (fence timeout) ends the
 * run; the state file ordcnt.state makes the next launch log it and continue after it. If a gate
 * test hangs, the other tests of its queue are skipped and the risky ones use compute queue 1.
 * Risky tests can hang or desync the ordered wave IDs; each is followed by a probe (P*). A new
 * OC_BUILD starts over (log truncated); after the last test the run is not repeated until
 * ordcnt.state is deleted. */
#include "cs_ordcnt.h"

extern int sceGnmDispatchDirect(uint32_t* cmd, uint32_t size, uint32_t tgx, uint32_t tgy,
                                uint32_t tgz, uint32_t flags);
extern int sceGnmDispatchInitDefaultHardwareState(uint32_t* cmd, uint32_t size);
extern void sceGnmDingDong(uint32_t vqid, uint32_t next_offs_dw);

#define OC_BUILD "ordcnt-7"
#define OC_TIMEOUT_US 2000000u

#define OC_OUT_SIZE 0x400000u /* params, GDS images, records */
#define OC_GDS_INIT 0x1000u   /* GDS init image (CPU-written, copied to GDS) */
#define OC_GDS_DUMP 0x1400u   /* GDS after the dispatches */
#define OC_REG_PRE 0x1800u    /* registers (COPY_DATA) before the dispatches */
#define OC_REG_POST 0x1C00u   /* registers after them */
#define OC_GDS_BYTES 0x400u
#define OC_REC_OFF 0x2000u
#define OC_REC_BYTES 0x400u
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
    OC_P_SKIP, /* skip the ops when [15:0] != 0 and (slot & [15:0]) != [31:16] */
    OC_P_REC_BASE,
    OC_P_VAL2,
    OC_P_YSTRIDE2,
    OC_P_ALT,
    OC_P_PRE_FWD,  /* 0: pre-delay (waves - 1 - slot) * PRE_DELAY, 1: slot * PRE_DELAY */
    OC_P_M0_PLAIN, /* M0 of the plain GDS op and of the tickets */
    OC_P_MID2,     /* sleep iterations before op3 ... */
    OC_P_MID2_SEL, /* ... for waves with (slot & [15:0]) == [31:16] */
    OC_P_ADDR3,
    OC_P_VAL3,
    OC_P_MID_SEL, /* sleep before op2 when (slot & [15:0]) == [31:16]; 0 = odd slots */
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
    OC_H_TB3,
    OC_H_TA3 = 27,
    OC_H_TK3 = 29,
    OC_H_M0C,
};

struct OcTest {
    const char* id;
    const char* what;
    uint8_t variant, queue, flags, kind; /* kind: 0 safe, 1 risky, 2 probe */
    uint8_t gate; /* a hang here means the queue cannot run ordered dispatches: skip its others */
    uint8_t pre_fwd;
    uint8_t nogds;      /* no CP DMA of GDS 0..0x3FF before / after */
    uint8_t nregs;      /* registers read with COPY_DATA before the dispatches ... */
    uint8_t regs_after; /* ... and after them */
    const uint16_t* regs;
    uint32_t m0plain, mid2, mid2_sel, addr3, val3, mid_sel, skip;
    uint16_t gx, gy, gz; /* gz > 1 only with the lean shader */
    uint8_t wpt, ndisp, gds_init, lanes, m0mode;
    uint32_t gds_val, addr1, addr2, ystride1, pre_delay, mid_delay, val1, lane_mul, slot_mul, val2,
        alt;
    uint64_t exec;
};

#define OC_T(...)                                                                                  \
    {                                                                                              \
        .gx = 4, .gy = 2, .gz = 1, .wpt = 2, .ndisp = 1, .flags = 0x08, .variant = CS_OC_A,        \
        .val1 = 1, .val2 = 1, .val3 = 1, .exec = 1, .m0plain = 0x00000400u, __VA_ARGS__            \
    }
#define OC_PROBE(n) OC_T(.id = n, .what = "probe: idx0, 16 waves, no delay", .kind = 2)

/* OC_T defaults, then the test's own fields: later designated initializers win (C11 6.7.9). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winitializer-overrides"
static const struct OcTest k_oc_tests[] = {
/* How many waves can wait at once, and in what order they run. Lean shader: 10 waves fit
   per SIMD. DELAY_MODE (.pre_fwd) 1: only wave 0 sleeps (.pre_delay iterations). */
#define OC_L(n, q, x, y, z, w, d, m, wh)                                                           \
    OC_T(.id = n, .variant = CS_OC_LEAN, .queue = q, .gx = x, .gy = y, .gz = z, .wpt = w,          \
         .pre_delay = d, .pre_fwd = m, .what = wh)
    OC_L("L01", 0, 2048, 1, 1, 1, 4000, 1, "2048 1-wave groups, wave 0 sleeps, the rest wait"),
    OC_L("L02", 0, 512, 1, 1, 4, 4000, 1, "512 4-wave groups, wave 0 sleeps, the rest wait"),
    OC_L("L03", 0, 4, 3, 2, 2, 20, 0, "grid 4x3x2 of 2-wave groups, waves arrive in reverse order"),
    OC_L("L04", 0, 8, 1, 1, 16, 2000, 1, "8 groups of 16 waves (1024 threads), wave 0 sleeps"),
    OC_L("L05", 0, 4096, 1, 1, 1, 0, 0, "4096 1-wave groups, no delays"),
    OC_L("L06", 1, 2048, 1, 1, 1, 4000, 1,
         "compute queue 1: 2048 1-wave groups, wave 0 sleeps, the rest wait"),
    OC_L("L07", 0, 6, 5, 4, 1, 0, 0, "grid 6x5x4 of 1-wave groups, no delays"),
#undef OC_L
    /* Risky: offset1 bit 5. */
    OC_T(.id = "B01", .kind = 1, .variant = CS_OC_X5,
         .what = "bit 5, value 1 in lane 0 (repeat of Z03)"),
    OC_PROBE("P01"),
    OC_T(.id = "B02", .kind = 1, .variant = CS_OC_X5, .exec = ~0ULL, .lane_mul = 1, .lanes = 1,
         .what = "bit 5, EXEC all, value lane+1"),
    OC_PROBE("P02"),
    OC_T(.id = "B03", .kind = 1, .variant = CS_OC_X5, .val1 = 100, .slot_mul = 1, .gds_init = 1,
         .what = "bit 5, value 100+slot, GDS pattern 0x1000+dword"),
    OC_PROBE("P03"),
    OC_T(.id = "B04", .kind = 1, .variant = CS_OC_X5S, .val1 = 100, .slot_mul = 1, .gds_init = 2,
         .gds_val = 0x77, .what = "bit 5 with swap, value 100+slot, GDS 0x77"),
    OC_PROBE("P04"),
    OC_T(.id = "B05", .kind = 1, .variant = CS_OC_X5A, .val3 = 0x100, .slot_mul = 1,
         .what = "bit 5, ADDR v12 = 0xcafe0000|lane, next register v13 = 0x100+slot"),
    OC_PROBE("P05"),
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
    int32_t next, inflight, bad[4];
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
/* COPY_DATA (gfx_v8_0 ring_emit_rreg): src_sel 0 register, dst_sel 5 memory, write confirm. */
static void oc_copy_reg(struct PM4Builder* b, uint32_t reg, void* dst) {
    uint64_t a = (uint64_t)(uintptr_t)dst;
    pm4_emit(b, pm4_type3(0x40, 5));
    pm4_emit(b, (5u << 8) | (1u << 20));
    pm4_emit(b, reg);
    pm4_emit(b, 0);
    pm4_emit(b, (uint32_t)a);
    pm4_emit(b, (uint32_t)(a >> 32));
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

/* Queue 0: the DCB (graphics ring). Queues 1..3: compute queues mapped here (pipe, queue). */
static const uint32_t k_oc_queue[3][2] = {{0, 5}, {0, 6}, {1, 5}};
static const char* const k_oc_qname[4] = {"DCB", "compute queue 1 (pipe 0 queue 5)",
                                          "compute queue 2 (pipe 0 queue 6)",
                                          "compute queue 3 (pipe 1 queue 5)"};

/* Lean shader (cs_ordcnt.h "lean"): 64-byte records, its own parameter block. */
enum {
    OC_L_GX,
    OC_L_WPT,
    OC_L_GY,
    OC_L_DELAY,
    OC_L_WAVES,
    OC_L_REC_BASE,
    OC_L_DELAY_MODE, /* 0: (waves - 1 - slot) * DELAY, 1: slot 0 sleeps DELAY */
};
enum {
    OC_LR_TGSIZE,
    OC_LR_X,
    OC_LR_Y,
    OC_LR_Z,
    OC_LR_WAVE,
    OC_LR_SLOT,
    OC_LR_RET,
    OC_LR_TISSUE,
    OC_LR_TRET,
    OC_LR_HWID,
    OC_LR_MARK,
    OC_LR_TSTART,
    OC_LR_TISSUE_HI,
    OC_LR_TRET_HI,
};
#define OC_LEAN_REC 64u
static uint32_t oc_waves(const struct OcTest* t) {
    return (uint32_t)t->gx * t->gy * t->gz * t->wpt * t->ndisp;
}
static uint32_t oc_rec_bytes(const struct OcTest* t) {
    return t->variant == CS_OC_LEAN ? OC_LEAN_REC : OC_REC_BYTES;
}

struct OcCtx {
    uint8_t* out;
    void* sh[CS_OC_COUNT];
    uint32_t* dcb;
    volatile uint32_t* fence;
    uint32_t fv;
    struct {
        uint32_t* ring;
        volatile uint32_t* rptr;
        int vqid;
        uint32_t wptr;
    } cq[3]; /* queue 1..3 = cq[0..2] */
};

/* Builds one test's packets into b. */
static void oc_build(struct PM4Builder* b, struct OcCtx* c, const struct OcTest* t, int q) {
    uint32_t avail = b->cap - b->off;
    int w = q ? sceGnmDispatchInitDefaultHardwareState(b->buf + b->off, avail)
              : sceGnmDrawInitDefaultHardwareState350(b->buf + b->off, avail);
    if (w > 0)
        b->off += (uint32_t)w;
    oc_acquire(b, OC_COHER_ICACHE | OC_COHER_KCACHE | OC_COHER_TCL1 | OC_COHER_TC);
    if (!t->nogds)
        oc_dma(b, 0, (uint64_t)(uintptr_t)(c->out + OC_GDS_INIT), 1, 0, OC_GDS_BYTES);
    for (int i = 0; i < t->nregs; i++)
        oc_copy_reg(b, t->regs[i], c->out + OC_REG_PRE + i * 4);
    uint64_t sa = (uint64_t)(uintptr_t)c->sh[t->variant];
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
        uint32_t rsrc[2] = {k_cs_oc[t->variant].rsrc1, k_cs_oc[t->variant].rsrc2};
        oc_sh(b, 0x212, rsrc, 2);
        uint32_t tmp = 0;
        oc_sh(b, 0x218, &tmp, 1);
        oc_sh(b, 0x240, v, 4);
        if (pm4_have_space(b, 9)) {
            sceGnmDispatchDirect(b->buf + b->off, 9, t->gx, t->gy, t->gz, t->flags);
            b->off += 9;
        } else {
            b->overflow++;
        }
    }
    pm4_emit(b, pm4_type3(0x46, 1));
    pm4_emit(b, 0x07u | (4u << 8)); /* EVENT_WRITE CS_PARTIAL_FLUSH */
    for (int i = 0; t->regs_after && i < t->nregs; i++)
        oc_copy_reg(b, t->regs[i], c->out + OC_REG_POST + i * 4);
    if (!t->nogds)
        oc_dma(b, 1, 0, 0, (uint64_t)(uintptr_t)(c->out + OC_GDS_DUMP), OC_GDS_BYTES);
    oc_acquire(b, OC_COHER_TC_WB | OC_COHER_TC | OC_COHER_TCL1);
    c->fv++;
    if (q)
        oc_release_mem(b, c->fence, c->fv);
    else
        pm4_event_write_eop(b, c->fence, c->fv);
}

static void oc_params(struct OcCtx* c, const struct OcTest* t) {
    uint32_t waves = oc_waves(t) / t->ndisp;
    my_memset(c->out, 0, OC_REC_OFF + (unsigned long)oc_waves(t) * oc_rec_bytes(t));
    if (t->variant == CS_OC_LEAN) {
        uint32_t* p = (uint32_t*)c->out;
        p[OC_L_GX] = t->gx;
        p[OC_L_WPT] = t->wpt;
        p[OC_L_GY] = t->gy;
        p[OC_L_DELAY] = t->pre_delay;
        p[OC_L_WAVES] = waves;
        p[OC_L_REC_BASE] = OC_REC_OFF;
        p[OC_L_DELAY_MODE] = t->pre_fwd;
        return;
    }
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
        p[OC_P_SKIP] = t->skip;
        p[OC_P_REC_BASE] = OC_REC_OFF - d * 0x100 + (uint32_t)d * waves * OC_REC_BYTES;
        p[OC_P_VAL2] = t->val2;
        p[OC_P_YSTRIDE2] = 0;
        p[OC_P_ALT] = t->alt;
        p[OC_P_PRE_FWD] = t->pre_fwd;
        p[OC_P_M0_PLAIN] = t->m0plain;
        p[OC_P_MID2] = t->mid2;
        p[OC_P_MID2_SEL] = t->mid2_sel;
        p[OC_P_ADDR3] = t->addr3;
        p[OC_P_VAL3] = t->val3;
        p[OC_P_MID_SEL] = t->mid_sel;
    }
}

/* Returns 0 done, -1 fence timeout, -2 could not submit. */
static int oc_submit(struct OcCtx* c, const struct OcTest* t, int q, uint64_t* us) {
    struct PM4Builder b;
    if (q) {
        if (c->cq[q - 1].vqid <= 0 || c->cq[q - 1].wptr + 0x400 > OC_RING_DW)
            return -2;
        pm4_init(&b, c->cq[q - 1].ring + c->cq[q - 1].wptr, OC_RING_DW - c->cq[q - 1].wptr);
    } else {
        pm4_init(&b, c->dcb, 0x4000 / 4);
    }
    oc_build(&b, c, t, q);
    if (b.overflow)
        return -2;
    uint64_t t0 = sceKernelGetProcessTime();
    if (q) {
        c->cq[q - 1].wptr += b.off;
        sceGnmDingDong((uint32_t)c->cq[q - 1].vqid, c->cq[q - 1].wptr);
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
/* GDS as read by the window shaders (64 windows of 0x400 bytes): nonzero dwords, and the dwords
   that differ from the previous window read in this launch. */
static uint32_t g_oc_snap[0x4000];
static int g_oc_snap_ok;
static void oc_windows(struct OcCtx* c) {
    const uint32_t* g = (const uint32_t*)(c->out + OC_REC_OFF);
    uint32_t nz = 0, nd = 0;
    oc_s("GDS 0x0000..0xFFFF as read (byte offset: value), nonzero:");
    for (uint32_t i = 0; i < 0x4000; i++)
        if (g[i]) {
            if (nz < 256) {
                oc_s(nz % 6 ? "  " : "\n");
                oc_x(i * 4, 4);
                oc_s(": ");
                oc_x(g[i], 8);
            }
            nz++;
        }
    oc_s("\nnonzero dwords: ");
    oc_u(nz);
    if (g_oc_snap_ok) {
        oc_s("\nchanged since the previous read (byte offset: before -> after):");
        for (uint32_t i = 0; i < 0x4000; i++)
            if (g[i] != g_oc_snap[i]) {
                if (nd < 256) {
                    oc_s(nd % 4 ? "  " : "\n");
                    oc_x(i * 4, 4);
                    oc_s(": ");
                    oc_x(g_oc_snap[i], 8);
                    oc_s(" -> ");
                    oc_x(g[i], 8);
                }
                nd++;
            }
        oc_s("\nchanged dwords: ");
        oc_u(nd);
    } else {
        oc_s("\n(no previous read in this launch)");
    }
    oc_s("\n\n");
    my_memcpy(g_oc_snap, g, sizeof(g_oc_snap));
    g_oc_snap_ok = 1;
}

/* Lean tests: launch order (slot = x, then y, then z, then wave in group) against the order the
   ops ran in (the returned value), wave-ID steps, how many waves waited at once, and where they
   ran (HW_ID: shader engine, compute unit, SIMD, wave slot). */
static uint32_t g_oc_by_ret[0x10000];
static void oc_lean_report(struct OcCtx* c, const struct OcTest* t, uint64_t us) {
    uint32_t n = oc_waves(t);
    const uint32_t* r0 = (const uint32_t*)(c->out + OC_REC_OFF);
    uint32_t marks = 0, badret = 0;
    for (uint32_t i = 0; i < n && i < 0x10000; i++)
        g_oc_by_ret[i] = 0xFFFFFFFFu;
    for (uint32_t s = 0; s < n; s++) {
        const uint32_t* r = r0 + s * (OC_LEAN_REC / 4);
        if (r[OC_LR_MARK] != 0x0DC0FFEEu)
            continue;
        marks++;
        if (r[OC_LR_RET] < n && g_oc_by_ret[r[OC_LR_RET]] == 0xFFFFFFFFu)
            g_oc_by_ret[r[OC_LR_RET]] = s;
        else
            badret++;
    }
    oc_s("done in ");
    oc_u(us);
    oc_s(" us\nrecords written: ");
    oc_u(marks);
    oc_s(" / ");
    oc_u(n);
    oc_s(", returned values outside 0..waves-1 or repeated: ");
    oc_u(badret);
    /* launch order vs run order */
    uint32_t slot_steps = 0, slot_other = 0, id_steps = 0, id_other = 0, prev = 0xFFFFFFFFu;
    oc_s("\nrun order (by returned value) where the slot does not advance by 1:");
    for (uint32_t v = 0; v < n; v++) {
        uint32_t s = g_oc_by_ret[v];
        if (s == 0xFFFFFFFFu)
            continue;
        if (prev != 0xFFFFFFFFu) {
            const uint32_t* a = r0 + prev * (OC_LEAN_REC / 4);
            const uint32_t* b = r0 + s * (OC_LEAN_REC / 4);
            uint32_t ia = (a[OC_LR_TGSIZE] >> 6) & 0x7FF, ib = (b[OC_LR_TGSIZE] >> 6) & 0x7FF;
            if (s == prev + 1) {
                slot_steps++;
            } else if (slot_other++ < 16) {
                oc_s(" [");
                oc_u(v);
                oc_s("] ");
                oc_u(prev);
                oc_s("->");
                oc_u(s);
            }
            if (ib == ia + 1) {
                id_steps++;
            } else if (id_other++ < 16) {
                oc_s(" {id ");
                oc_x(ia, 3);
                oc_s("->");
                oc_x(ib, 3);
                oc_s(" at ");
                oc_u(v);
                oc_s("}");
            }
        }
        prev = s;
    }
    oc_s("\nslot +1 steps ");
    oc_u(slot_steps);
    oc_s(", other ");
    oc_u(slot_other);
    oc_s("; wave-ID +1 steps ");
    oc_u(id_steps);
    oc_s(", other ");
    oc_u(id_other);
    oc_s("\nfirst 48 in run order (value: slot x,y,z wave id):");
    for (uint32_t v = 0; v < n && v < 48; v++) {
        uint32_t s = g_oc_by_ret[v];
        if (s == 0xFFFFFFFFu)
            continue;
        const uint32_t* r = r0 + s * (OC_LEAN_REC / 4);
        oc_s(v % 4 ? "   " : "\n");
        oc_u(v);
        oc_s(": ");
        oc_u(s);
        oc_s(" ");
        oc_u(r[OC_LR_X]);
        oc_s(",");
        oc_u(r[OC_LR_Y]);
        oc_s(",");
        oc_u(r[OC_LR_Z]);
        oc_s(" w");
        oc_u(r[OC_LR_WAVE]);
        oc_s(" ");
        oc_x((r[OC_LR_TGSIZE] >> 6) & 0x7FF, 3);
    }
    /* waiting at the same time; where the waves ran */
    uint64_t t0 = 0;
    const uint32_t* w0 = r0;
    if (w0[OC_LR_MARK] == 0x0DC0FFEEu)
        t0 = ((uint64_t)w0[OC_LR_TISSUE_HI] << 32) | w0[OC_LR_TISSUE];
    uint32_t before0 = 0, peak = 0, maxslot = 0;
    static uint8_t cu_seen[64];
    my_memset(cu_seen, 0, sizeof(cu_seen));
    for (uint32_t s = 0; s < n; s++) {
        const uint32_t* r = r0 + s * (OC_LEAN_REC / 4);
        if (r[OC_LR_MARK] != 0x0DC0FFEEu)
            continue;
        uint64_t is = ((uint64_t)r[OC_LR_TISSUE_HI] << 32) | r[OC_LR_TISSUE];
        uint64_t rt = ((uint64_t)r[OC_LR_TRET_HI] << 32) | r[OC_LR_TRET];
        if (s && t0 && is < t0)
            before0++;
        uint32_t live = 0;
        for (uint32_t k = 0; k < n; k++) {
            const uint32_t* q = r0 + k * (OC_LEAN_REC / 4);
            if (q[OC_LR_MARK] != 0x0DC0FFEEu)
                continue;
            uint64_t qi = ((uint64_t)q[OC_LR_TISSUE_HI] << 32) | q[OC_LR_TISSUE];
            uint64_t qr = ((uint64_t)q[OC_LR_TRET_HI] << 32) | q[OC_LR_TRET];
            if (qi <= is && is < qr)
                live++;
        }
        if (live > peak)
            peak = live;
        (void)rt;
        uint32_t h = r[OC_LR_HWID];
        uint32_t cu = ((h >> 13) & 3) * 32 + ((h >> 12) & 1) * 16 + ((h >> 8) & 15);
        cu_seen[cu & 63] = 1;
        if ((h & 15) > maxslot)
            maxslot = h & 15;
    }
    uint32_t ncu = 0;
    for (int i = 0; i < 64; i++)
        ncu += cu_seen[i];
    oc_s("\nwaves that issued their op before wave 0 did: ");
    oc_u(before0);
    oc_s("\nmost ops in flight at one time (issued, not yet returned): ");
    oc_u(peak);
    oc_s("\ncompute units used: ");
    oc_u(ncu);
    oc_s(", highest wave slot in a SIMD: ");
    oc_u(maxslot);
    oc_s("\nfirst 8 records (TG_SIZE, HW_ID):");
    for (uint32_t s = 0; s < n && s < 8; s++) {
        const uint32_t* r = r0 + s * (OC_LEAN_REC / 4);
        oc_s(" ");
        oc_x(r[OC_LR_TGSIZE], 8);
        oc_s("/");
        oc_x(r[OC_LR_HWID], 8);
    }
    oc_s("\n\n");
}

static void oc_report(struct OcCtx* c, const struct OcTest* t, int q, uint64_t us) {
    uint32_t waves = oc_waves(t);
    if (t->variant == CS_OC_LEAN) {
        oc_lean_report(c, t, us);
        return;
    }
    if (t->nregs) {
        const uint32_t* r0 = (const uint32_t*)(c->out + OC_REG_PRE);
        const uint32_t* r1 = (const uint32_t*)(c->out + OC_REG_POST);
        oc_s("registers (dword offset: before");
        oc_s(t->regs_after ? " / after the dispatch):" : "):");
        for (int i = 0; i < t->nregs; i++) {
            oc_s(i % 4 ? "  " : "\n");
            oc_x(t->regs[i], 4);
            oc_s(": ");
            oc_x(r0[i], 8);
            if (t->regs_after) {
                oc_s(" / ");
                oc_x(r1[i], 8);
            }
        }
        oc_s("\n");
    }
    if (t->variant == CS_OC_GDUMP || t->variant == CS_OC_GFILL) {
        oc_s("done in ");
        oc_u(us);
        oc_s(" us\n");
        oc_windows(c);
        return;
    }
    if (!t->ndisp) {
        oc_s("done in ");
        oc_u(us);
        oc_s(" us\n\n");
        return;
    }
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
         "| t ret1 | t op2 | t ret2 | HW_ID | M0 op3 | ret3 | tk3 | t op3 | t ret3 |\n");
    oc_s("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    uint32_t rows = waves > 128 ? 128 : waves;
    for (uint32_t s = 0; s < rows; s++) {
        const uint32_t* h = (const uint32_t*)(c->out + OC_REC_OFF + s * OC_REC_BYTES);
        const uint32_t* r1 = h + 64;
        const uint32_t* r2 = h + 128;
        const uint32_t* r3 = h + 192;
        oc_s("| ");
        oc_u(s);
        if (h[OC_H_MARK] != 0x0DC0FFEEu) {
            oc_s(" | not written ||||||||||||||||||\n");
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
        oc_s(" | ");
        oc_x(h[OC_H_M0C], 8);
        oc_s(" | ");
        oc_x(r3[rl], 8);
        oc_s(" | ");
        oc_i((int32_t)h[OC_H_TK3]);
        for (int k = 0; k < 2; k++) {
            uint32_t v = h[OC_H_TB3 + 2 * k];
            oc_s(" | ");
            if (v)
                oc_u(v - t0);
            else
                oc_s("-");
        }
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
    if (k_cs_oc[t->variant].nops > 1)
        oc_order(c, waves, 128 + rl, "ret2");
    if (k_cs_oc[t->variant].nops > 2)
        oc_order(c, waves, 192 + rl, "ret3");
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
    int ok = c.out && c.dcb && c.fence;
    for (int i = 0; i < 3; i++) {
        c.cq[i].ring = (uint32_t*)gpu_alloc_typed(OC_RING_DW * 4, 0x4000, MEM_TYPE_ONION);
        c.cq[i].rptr = (volatile uint32_t*)gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
        ok = ok && c.cq[i].ring && c.cq[i].rptr;
    }
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
    for (int i = 0; i < 3; i++) {
        c.cq[i].vqid = sceGnmMapComputeQueue(k_oc_queue[i][0], k_oc_queue[i][1], c.cq[i].ring,
                                             OC_RING_DW, (void*)c.cq[i].rptr);
        oc_s("compute queue ");
        oc_u(i + 1);
        oc_s(": sceGnmMapComputeQueue(pipe ");
        oc_u(k_oc_queue[i][0]);
        oc_s(", queue ");
        oc_u(k_oc_queue[i][1]);
        oc_s(") = ");
        oc_x((uint32_t)c.cq[i].vqid, 8);
        oc_s("\n");
    }
    oc_s("\n");
    oc_flush();
    for (int k = st.next; k < OC_NTESTS; k++) {
        const struct OcTest* t = &k_oc_tests[k];
        int q = t->queue;
        if (t->kind && st.bad[0] && !st.bad[1])
            q = 1;
        uint32_t waves = oc_waves(t);
        oc_s("### ");
        oc_s(t->id);
        oc_s(": ");
        oc_s(t->what);
        oc_s("\nvariant ");
        oc_s(k_cs_oc[t->variant].name);
        oc_s(", ");
        oc_s(k_oc_qname[q]);
        oc_s(", flags ");
        oc_x(t->flags, 2);
        oc_s(", grid ");
        oc_u(t->gx);
        oc_s("x");
        oc_u(t->gy);
        oc_s("x");
        oc_u(t->gz);
        oc_s(" x ");
        oc_u(t->wpt);
        oc_s(" waves x ");
        oc_u(t->ndisp);
        oc_s(" dispatch, EXEC ");
        oc_x(t->exec, 16);
        oc_s(", M0 mode ");
        oc_u(t->m0mode);
        oc_s("\n");
        if (st.bad[q] || waves > (OC_OUT_SIZE - OC_REC_OFF) / oc_rec_bytes(t)) {
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
