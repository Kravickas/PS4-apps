#pragma once
/* ==== FLIP_PROBE: VideoOut buffer label vs EOP flips ==========================================
   Records what the display buffer labels do around EOP flips, on hardware and on shadPS4, to
   settle whether label[b] can be 0 when an EOP flip of b completes (shadPS4 asserts it is 1).

   Phases, each followed by a drain:
     A eop_alt_p1   EOP flips (sceGnmSubmitAndFlipCommandBuffers, plain 0x68750777 marker) of
                    buffers 0,1,0,1..., next frame only once nothing is pending (control)
     B eop_alt_p2   the same, up to 2 pending: frame k finishes on the GPU while k-1 waits
     C eop_alt_p8   the same, up to 8 pending
     D imm_alt_p2   immediate flips (sceVideoOutSubmitFlip) of 0,1,0,1..., up to 2 pending
     E eop_same_p2  EOP flips of buffer 0 only, up to 2 pending (re-flipping a buffer)
     F eop_imm_mix  an EOP flip of the buffer on screen, then an immediate flip of the other

   Recorded:
     - per submit: phase, buffer, flip arg, CPU label reads before / after, flip and vblank
       status before, every return code, TSC stamps around pacing, submit and SubmitDone;
     - per submit, from the GPU: the label as the GPU read it just before the marker (DMA copy),
       the GPU clock there, and a progress word the poller watches;
     - the 64-dword marker block before the submit and after libSceGnmDriver patched it;
     - a poller thread: every change of label[0], label[1], the progress word, the flip status
       and the vblank count, TSC stamped;
     - every flip event from the equeue;
     - per submit, derived: when the GPU finished it, when it reached the screen, and the label
       values across that span.
   Output: /user/data/ShadCube4/flipprobe.log, flushed every FP_FLUSH_EVERY submits. */

extern uint64_t sceKernelReadTsc(void);
extern uint64_t sceKernelGetTscFrequency(void);
extern int sceKernelIsNeoMode(void);

#define FP_MAX_SUBMITS 2048
#define FP_MAX_EVENTS (1u << 17)
#define FP_MAX_QEVENTS 4096
#define FP_DCB_SLOTS 32
#define FP_DCB_DWORDS 1024
#define FP_FLUSH_EVERY 64
#define FP_PACE_TIMEOUT_US 3000000ull
#define FP_DRAIN_TIMEOUT_US 5000000ull
#define FP_TAG_PLAIN 0x68750777u
/* 1: the GPU copies label[bi] into gpu_label_pre just before the marker (DMA_DATA memory to
   memory). Hardware only: shadPS4 keeps the labels in host memory and executes this DMA through
   its buffer cache by guest address. 0: gpu_label_pre stays 0xffffffffffffffff. */
#define FP_GPU_LABEL_SNAPSHOT 0

enum { FP_EOP_ALT = 0, FP_IMM_ALT = 1, FP_EOP_SAME = 2, FP_EOP_IMM_MIX = 3 };

struct fp_phase {
    const char* name;
    int kind;
    int max_pending;
    int submits;
};

static const struct fp_phase g_fp_phases[] = {
    {"A eop_alt_p1", FP_EOP_ALT, 1, 180},   {"B eop_alt_p2", FP_EOP_ALT, 2, 300},
    {"C eop_alt_p8", FP_EOP_ALT, 8, 300},   {"D imm_alt_p2", FP_IMM_ALT, 2, 180},
    {"E eop_same_p2", FP_EOP_SAME, 2, 180}, {"F eop_imm_mix", FP_EOP_IMM_MIX, 2, 240},
};
#define FP_NUM_PHASES ((int)(sizeof(g_fp_phases) / sizeof(g_fp_phases[0])))

/* One submit (an EOP flip or an immediate flip). */
struct fp_submit {
    uint32_t idx; /* global submit index; also the flip arg */
    uint32_t phase;
    uint32_t eop; /* 1 EOP flip, 0 immediate flip */
    int32_t bi;
    int32_t ret;    /* SubmitAndFlip or SubmitFlip */
    int32_t sd_ret; /* SubmitDone after an EOP submit */
    int32_t pace_timeout;
    uint32_t dcb_slot;
    uint32_t dcb_dwords;
    uint64_t t_pace0, t_pace1, t_sub0, t_sub1, t_sd1;
    uint64_t lab_before[2], lab_after[2];
    OrbisVideoOutFlipStatus fs_before;
    int32_t fs_before_ret;
    uint64_t vbl_before;
    uint32_t tail_pre[64];
    uint32_t tail_post[64];
};

/* One change seen by the poller. */
struct fp_event {
    uint64_t tsc;
    uint64_t lab0, lab1;
    uint64_t fnum;
    int64_t farg;
    uint64_t vbl;
    uint32_t progress;
    int32_t cur, pend, gpend;
    uint32_t changed; /* bit 0 lab0, 1 lab1, 2 progress, 3 flip status, 4 vblank */
    uint32_t polls;   /* polls since the previous event */
};

/* One kevent from the flip equeue. */
struct fp_qevent {
    uint64_t tsc;
    uint64_t ident;
    int64_t data;
    int32_t filter, flags, fflags;
};

/* GPU-written per-submit data. */
struct fp_gpu {
    volatile uint64_t label_pre[FP_MAX_SUBMITS]; /* label[bi] read by the GPU before the marker */
    volatile uint64_t ts[FP_MAX_SUBMITS];        /* GPU clock at the same point */
    volatile uint32_t progress;                  /* submit index + 1, last one the GPU reached */
};

static struct {
    int video;
    volatile uint64_t* label;
    struct fp_gpu* gpu;
    uint32_t* dcb[FP_DCB_SLOTS];
    struct fp_submit* sub;
    uint32_t nsub;
    struct fp_event* ev;
    volatile uint32_t nev; /* published by the poller */
    volatile uint32_t ev_drop;
    volatile uint32_t stop;
    volatile uint64_t polls;
    volatile uint64_t max_poll_gap;
    volatile long long poll_affinity_ret;
    struct fp_qevent* qev;
    uint32_t nqev;
    OrbisKernelEqueue eq;
    int eq_ok;
    int fd;
    char* out;
    unsigned long out_n;
    uint32_t flushed_sub, flushed_ev, flushed_qev;
    uint64_t tsc_hz;
} g_fp;

/* ---- output ---- */
#define FP_OUT_CAP (1u << 20)
static void fp_flush_out(void) {
    if (g_fp.fd >= 0 && g_fp.out_n) {
        sceKernelWrite(g_fp.fd, g_fp.out, g_fp.out_n);
        sceKernelFsync(g_fp.fd);
    }
    g_fp.out_n = 0;
}
static void fp_s(const char* s) {
    while (*s) {
        if (g_fp.out_n >= FP_OUT_CAP - 64)
            fp_flush_out();
        g_fp.out[g_fp.out_n++] = *s++;
    }
}
static void fp_u(unsigned long long v) {
    char t[24];
    int n = lg_u64(t, v);
    t[n] = 0;
    fp_s(t);
}
static void fp_i(long long v) {
    char t[24];
    int n = lg_i64(t, v);
    t[n] = 0;
    fp_s(t);
}
static void fp_x(unsigned long long v) {
    char t[24];
    int n = lg_hex(t, v);
    t[n] = 0;
    fp_s(t);
}
static void fp_kv_u(const char* k, unsigned long long v) {
    fp_s(" ");
    fp_s(k);
    fp_s("=");
    fp_u(v);
}
static void fp_kv_i(const char* k, long long v) {
    fp_s(" ");
    fp_s(k);
    fp_s("=");
    fp_i(v);
}
static void fp_kv_x(const char* k, unsigned long long v) {
    fp_s(" ");
    fp_s(k);
    fp_s("=");
    fp_x(v);
}
static void fp_dwords(const char* tag, uint32_t idx, const uint32_t* d) {
    fp_s(tag);
    fp_kv_u("submit", idx);
    fp_s(" :");
    for (int i = 0; i < 64; i++) {
        fp_s(" ");
        fp_x(d[i]);
    }
    fp_s("\n");
}
static void fp_open(void) {
    g_fp.fd = -1;
    const char* paths[] = {"/user/data/ShadCube4/flipprobe.log", DATA_DIR_NEW "flipprobe.log",
                           DATA_DIR_OLD "flipprobe.log", "flipprobe.log", 0};
    for (int i = 0; paths[i]; i++) {
        int fd = sceKernelOpen(paths[i], 0x601, 0x1FF);
        if (fd >= 0) {
            g_fp.fd = fd;
            return;
        }
    }
}

/* ---- poller ---- */
static void* fp_poller(void* arg) {
    (void)arg;
    g_fp.poll_affinity_ret = -98;
#if SET_AFFINITY
    {
        void* self = scePthreadSelf();
        g_fp.poll_affinity_ret = self ? scePthreadSetaffinity(self, 0x20ULL) : -1;
    }
#endif
    struct fp_event last;
    my_memset(&last, 0xff, sizeof(last));
    uint32_t since = 0;
    uint64_t prev_t = sceKernelReadTsc();
    while (!g_fp.stop) {
        struct fp_event e;
        e.lab0 = g_fp.label[0];
        e.lab1 = g_fp.label[1];
        e.progress = g_fp.gpu->progress;
        OrbisVideoOutFlipStatus fs;
        my_memset(&fs, 0, sizeof(fs));
        sceVideoOutGetFlipStatus(g_fp.video, &fs);
        OrbisVideoOutVblankStatus vs;
        my_memset(&vs, 0, sizeof(vs));
        sceVideoOutGetVblankStatus(g_fp.video, &vs);
        e.fnum = fs.num;
        e.farg = fs.flipArg;
        e.cur = fs.currentBuffer;
        e.pend = fs.numFlipPending;
        e.gpend = fs.numGpuFlipPending;
        e.vbl = vs.count;
        e.tsc = sceKernelReadTsc();
        uint64_t gap = e.tsc - prev_t;
        prev_t = e.tsc;
        if (gap > g_fp.max_poll_gap)
            g_fp.max_poll_gap = gap;
        g_fp.polls++;
        since++;
        uint32_t ch = 0;
        if (e.lab0 != last.lab0)
            ch |= 1;
        if (e.lab1 != last.lab1)
            ch |= 2;
        if (e.progress != last.progress)
            ch |= 4;
        if (e.fnum != last.fnum || e.farg != last.farg || e.cur != last.cur ||
            e.pend != last.pend || e.gpend != last.gpend)
            ch |= 8;
        if (e.vbl != last.vbl)
            ch |= 16;
        if (ch) {
            e.changed = ch;
            e.polls = since;
            since = 0;
            uint32_t n = g_fp.nev;
            if (n < FP_MAX_EVENTS) {
                g_fp.ev[n] = e;
                __atomic_store_n(&g_fp.nev, n + 1, __ATOMIC_RELEASE);
            } else {
                g_fp.ev_drop++;
            }
            last = e;
        }
    }
    return 0;
}

/* ---- equeue ---- */
static void fp_drain_queue(void) {
    if (!g_fp.eq_ok)
        return;
    for (;;) {
        struct kevent_t ev;
        int out = 0;
        unsigned int tmo = 0;
        if (sceKernelWaitEqueue(g_fp.eq, &ev, 1, &out, &tmo) != 0 || out <= 0)
            return;
        if (g_fp.nqev < FP_MAX_QEVENTS) {
            struct fp_qevent* q = &g_fp.qev[g_fp.nqev++];
            q->tsc = sceKernelReadTsc();
            q->ident = ev.ident;
            q->data = ev.data;
            q->filter = ev.filter;
            q->flags = ev.flags;
            q->fflags = (int32_t)ev.fflags;
        }
    }
}

static int fp_pending(void) {
    OrbisVideoOutFlipStatus fs;
    my_memset(&fs, 0, sizeof(fs));
    if (sceVideoOutGetFlipStatus(g_fp.video, &fs) != 0)
        return -1;
    return fs.numFlipPending + fs.numGpuFlipPending;
}

/* Waits until fewer than max flips are pending; returns 1 on timeout. */
static int fp_pace(int max, uint64_t timeout_us) {
    uint64_t t0 = sceKernelGetProcessTime();
    for (;;) {
        fp_drain_queue();
        int p = fp_pending();
        if (p >= 0 && p < max)
            return 0;
        if (sceKernelGetProcessTime() - t0 > timeout_us)
            return 1;
        sceKernelUsleep(100);
    }
}

/* ---- one submit ---- */
static uint32_t fp_build_dcb(uint32_t idx, int bi, uint32_t** out) {
    uint32_t* d = g_fp.dcb[idx % FP_DCB_SLOTS];
    g_fp.gpu->label_pre[idx] = ~0ull;
    struct PM4Builder bb;
    struct PM4Builder* b = &bb;
    pm4_init(b, d, FP_DCB_DWORDS);
    /* The label as the GPU sees it before the marker, then the GPU clock, then the progress
       word: all ordered behind each other (cp_sync DMA, end-of-pipe timestamp, confirmed write). */
#if FP_GPU_LABEL_SNAPSHOT
    pm4_dma_copy(b, (void*)&g_fp.gpu->label_pre[idx], (const void*)&g_fp.label[bi], 8);
#endif
    pm4_gpu_timestamp(b, &g_fp.gpu->ts[idx]);
    pm4_write_data_dword(b, &g_fp.gpu->progress, idx + 1);
    /* The plain prepareFlip marker, NHL's form: 64 dwords, tag 0x68750777, the rest zero. */
    pm4_emit(b, pm4_type3(PM4_NOP, 63));
    pm4_emit(b, FP_TAG_PLAIN);
    for (int i = 0; i < 62; i++)
        pm4_emit(b, 0);
    *out = d;
    return b->overflow ? 0 : b->off;
}

static void fp_submit(uint32_t phase, int eop, int bi, int max_pending) {
    if (g_fp.nsub >= FP_MAX_SUBMITS)
        return;
    struct fp_submit* s = &g_fp.sub[g_fp.nsub];
    my_memset(s, 0, sizeof(*s));
    s->idx = g_fp.nsub;
    s->phase = phase;
    s->eop = (uint32_t)eop;
    s->bi = bi;
    s->t_pace0 = sceKernelReadTsc();
    s->pace_timeout = fp_pace(max_pending, FP_PACE_TIMEOUT_US);
    s->t_pace1 = sceKernelReadTsc();
    s->fs_before_ret = sceVideoOutGetFlipStatus(g_fp.video, &s->fs_before);
    {
        OrbisVideoOutVblankStatus vs;
        my_memset(&vs, 0, sizeof(vs));
        sceVideoOutGetVblankStatus(g_fp.video, &vs);
        s->vbl_before = vs.count;
    }
    s->lab_before[0] = g_fp.label[0];
    s->lab_before[1] = g_fp.label[1];
    if (eop) {
        uint32_t* d = 0;
        uint32_t dw = fp_build_dcb(s->idx, bi, &d);
        s->dcb_slot = s->idx % FP_DCB_SLOTS;
        s->dcb_dwords = dw;
        if (dw >= 64)
            for (int i = 0; i < 64; i++)
                s->tail_pre[i] = d[dw - 64 + i];
        if (dw == 0) {
            s->ret = -1;
        } else {
            void* a[1] = {d};
            uint32_t sz[1] = {dw * 4};
            s->t_sub0 = sceKernelReadTsc();
            s->ret = sceGnmSubmitAndFlipCommandBuffers(1, a, sz, 0, 0, g_fp.video, bi, 1,
                                                       (int64_t)s->idx);
            s->t_sub1 = sceKernelReadTsc();
            for (int i = 0; i < 64; i++)
                s->tail_post[i] = d[dw - 64 + i];
            s->sd_ret = sceGnmSubmitDone();
            s->t_sd1 = sceKernelReadTsc();
        }
    } else {
        s->t_sub0 = sceKernelReadTsc();
        s->ret = sceVideoOutSubmitFlip(g_fp.video, bi, 1, (int64_t)s->idx);
        s->t_sub1 = sceKernelReadTsc();
        /* An immediate flip has no command buffer: no GPU label read, no GPU clock. */
        g_fp.gpu->label_pre[s->idx] = ~0ull;
        g_fp.gpu->ts[s->idx] = 0;
    }
    s->lab_after[0] = g_fp.label[0];
    s->lab_after[1] = g_fp.label[1];
    g_fp.nsub++;
    fp_drain_queue();
}

/* ---- log sections ---- */
static void fp_log_submit(const struct fp_submit* s) {
    fp_s("SUB");
    fp_kv_u("idx", s->idx);
    fp_s(" phase=");
    fp_s(g_fp_phases[s->phase].name);
    fp_kv_u("eop", s->eop);
    fp_kv_i("bi", s->bi);
    fp_kv_x("ret", (uint32_t)s->ret);
    fp_kv_x("sd_ret", (uint32_t)s->sd_ret);
    fp_kv_i("pace_timeout", s->pace_timeout);
    fp_kv_u("t_pace0", s->t_pace0);
    fp_kv_u("t_pace1", s->t_pace1);
    fp_kv_u("t_sub0", s->t_sub0);
    fp_kv_u("t_sub1", s->t_sub1);
    fp_kv_u("t_sd1", s->t_sd1);
    fp_kv_x("lab0_before", s->lab_before[0]);
    fp_kv_x("lab1_before", s->lab_before[1]);
    fp_kv_x("lab0_after", s->lab_after[0]);
    fp_kv_x("lab1_after", s->lab_after[1]);
    fp_kv_x("gpu_label_pre", g_fp.gpu->label_pre[s->idx]);
    fp_kv_u("gpu_ts", g_fp.gpu->ts[s->idx]);
    fp_kv_x("fs_ret", (uint32_t)s->fs_before_ret);
    fp_kv_u("fs_num", s->fs_before.num);
    fp_kv_i("fs_arg", s->fs_before.flipArg);
    fp_kv_i("fs_cur", s->fs_before.currentBuffer);
    fp_kv_i("fs_pend", s->fs_before.numFlipPending);
    fp_kv_i("fs_gpend", s->fs_before.numGpuFlipPending);
    fp_kv_u("fs_ptime", s->fs_before.ptime);
    fp_kv_u("fs_stime", s->fs_before.stime);
    fp_kv_u("vbl", s->vbl_before);
    fp_kv_u("dcb_slot", s->dcb_slot);
    fp_kv_u("dcb_dwords", s->dcb_dwords);
    fp_s("\n");
    if (s->eop) {
        fp_dwords("TAIL_PRE ", s->idx, s->tail_pre);
        fp_dwords("TAIL_POST", s->idx, s->tail_post);
    }
}

static void fp_log_event(uint32_t i, const struct fp_event* e) {
    fp_s("EV");
    fp_kv_u("i", i);
    fp_kv_u("tsc", e->tsc);
    fp_kv_x("ch", e->changed);
    fp_kv_x("lab0", e->lab0);
    fp_kv_x("lab1", e->lab1);
    fp_kv_u("prog", e->progress);
    fp_kv_u("fnum", e->fnum);
    fp_kv_i("farg", e->farg);
    fp_kv_i("cur", e->cur);
    fp_kv_i("pend", e->pend);
    fp_kv_i("gpend", e->gpend);
    fp_kv_u("vbl", e->vbl);
    fp_kv_u("polls", e->polls);
    fp_s("\n");
}

static void fp_log_qevent(uint32_t i, const struct fp_qevent* q) {
    fp_s("QEV");
    fp_kv_u("i", i);
    fp_kv_u("tsc", q->tsc);
    fp_kv_x("ident", q->ident);
    fp_kv_i("filter", q->filter);
    fp_kv_x("flags", (uint32_t)q->flags);
    fp_kv_x("fflags", (uint32_t)q->fflags);
    fp_kv_x("data", (uint64_t)q->data);
    fp_s("\n");
}

/* Everything recorded since the previous flush. */
static void fp_flush_records(void) {
    while (g_fp.flushed_sub < g_fp.nsub)
        fp_log_submit(&g_fp.sub[g_fp.flushed_sub++]);
    uint32_t n = __atomic_load_n(&g_fp.nev, __ATOMIC_ACQUIRE);
    while (g_fp.flushed_ev < n) {
        fp_log_event(g_fp.flushed_ev, &g_fp.ev[g_fp.flushed_ev]);
        g_fp.flushed_ev++;
    }
    while (g_fp.flushed_qev < g_fp.nqev) {
        fp_log_qevent(g_fp.flushed_qev, &g_fp.qev[g_fp.flushed_qev]);
        g_fp.flushed_qev++;
    }
    fp_flush_out();
}

/* Per submit: when the GPU finished it, when it was on screen, and its buffer's label in between.
   "shown" is the first poller event whose flip status reports this submit's flip arg. */
static void fp_analyse(void) {
    uint32_t n = __atomic_load_n(&g_fp.nev, __ATOMIC_ACQUIRE);
    fp_s("=== ANALYSIS (per submit)\n");
    for (uint32_t k = 0; k < g_fp.nsub; k++) {
        const struct fp_submit* s = &g_fp.sub[k];
        long long gpu_i = -1, shown_i = -1;
        for (uint32_t i = 0; i < n; i++) {
            if (gpu_i < 0 && s->eop && g_fp.ev[i].progress >= s->idx + 1 &&
                g_fp.ev[i].tsc >= s->t_sub0)
                gpu_i = i;
            if (shown_i < 0 && g_fp.ev[i].farg == (int64_t)s->idx && g_fp.ev[i].tsc >= s->t_sub0)
                shown_i = i;
            if ((gpu_i >= 0 || !s->eop) && shown_i >= 0)
                break;
        }
        uint64_t lab_at_gpu = ~0ull, lab_at_shown = ~0ull;
        long long ones_to_zero = 0, zero_to_ones = 0;
        if (s->bi >= 0 && s->bi <= 1) {
            if (gpu_i >= 0)
                lab_at_gpu = s->bi ? g_fp.ev[gpu_i].lab1 : g_fp.ev[gpu_i].lab0;
            if (shown_i >= 0)
                lab_at_shown = s->bi ? g_fp.ev[shown_i].lab1 : g_fp.ev[shown_i].lab0;
            long long from = gpu_i >= 0 ? gpu_i : -1;
            if (from < 0) {
                for (uint32_t i = 0; i < n; i++)
                    if (g_fp.ev[i].tsc >= s->t_sub1) {
                        from = i;
                        break;
                    }
            }
            if (from >= 0 && shown_i >= 0) {
                for (long long i = from + 1; i <= shown_i; i++) {
                    uint64_t a = s->bi ? g_fp.ev[i - 1].lab1 : g_fp.ev[i - 1].lab0;
                    uint64_t c = s->bi ? g_fp.ev[i].lab1 : g_fp.ev[i].lab0;
                    if (a != 0 && c == 0)
                        ones_to_zero++;
                    if (a == 0 && c != 0)
                        zero_to_ones++;
                }
            }
        }
        fp_s("AN");
        fp_kv_u("idx", s->idx);
        fp_s(" phase=");
        fp_s(g_fp_phases[s->phase].name);
        fp_kv_u("eop", s->eop);
        fp_kv_i("bi", s->bi);
        fp_kv_i("gpu_done_ev", gpu_i);
        fp_kv_u("gpu_done_tsc", gpu_i >= 0 ? g_fp.ev[gpu_i].tsc : 0);
        fp_kv_i("shown_ev", shown_i);
        fp_kv_u("shown_tsc", shown_i >= 0 ? g_fp.ev[shown_i].tsc : 0);
        fp_kv_x("gpu_label_pre", g_fp.gpu->label_pre[s->idx]);
        fp_kv_x("lab_at_gpu_done", lab_at_gpu);
        fp_kv_x("lab_at_shown", lab_at_shown);
        fp_kv_i("lab_1to0_between", ones_to_zero);
        fp_kv_i("lab_0to1_between", zero_to_ones);
        fp_s("\n");
    }
}

/* Waits for every flip to finish; returns 1 on timeout. */
static int fp_drain(void) {
    uint64_t t0 = sceKernelGetProcessTime();
    while (sceKernelGetProcessTime() - t0 < FP_DRAIN_TIMEOUT_US) {
        fp_drain_queue();
        if (fp_pending() == 0) {
            sceKernelUsleep(100000);
            fp_drain_queue();
            return 0;
        }
        sceKernelUsleep(500);
    }
    return 1;
}

/* Runs every phase on the registered buffers 0 and 1, then logs everything. Teardown stays with
   the caller. */
static int flip_probe_run(int video, void* const* fb, int nfb, void* label_base,
                          OrbisKernelEqueue eq, int eq_ok) {
    my_memset(&g_fp, 0, sizeof(g_fp));
    g_fp.video = video;
    g_fp.label = (volatile uint64_t*)label_base;
    g_fp.eq = eq;
    g_fp.eq_ok = eq_ok;
    g_fp.tsc_hz = sceKernelGetTscFrequency();
    fp_open();
    g_fp.out = (char*)gpu_alloc(FP_OUT_CAP, 0x4000);
    g_fp.gpu = (struct fp_gpu*)gpu_alloc(sizeof(struct fp_gpu), 0x10000);
    g_fp.sub = (struct fp_submit*)gpu_alloc(sizeof(struct fp_submit) * FP_MAX_SUBMITS, 0x4000);
    g_fp.ev = (struct fp_event*)gpu_alloc(sizeof(struct fp_event) * FP_MAX_EVENTS, 0x4000);
    g_fp.qev = (struct fp_qevent*)gpu_alloc(sizeof(struct fp_qevent) * FP_MAX_QEVENTS, 0x4000);
    for (int i = 0; i < FP_DCB_SLOTS; i++)
        g_fp.dcb[i] = (uint32_t*)gpu_alloc(FP_DCB_DWORDS * 4, 0x4000);
    int alloc_ok = g_fp.out && g_fp.gpu && g_fp.sub && g_fp.ev && g_fp.qev;
    for (int i = 0; i < FP_DCB_SLOTS; i++)
        alloc_ok = alloc_ok && g_fp.dcb[i];
    if (!g_fp.out)
        return 2;

    fp_s("=== FLIP_PROBE start\n");
    fp_s("CFG");
    fp_kv_i("video", video);
    fp_kv_x("label_base", (uint64_t)(uintptr_t)label_base);
    fp_kv_i("nfb", nfb);
    for (int i = 0; i < nfb && i < 4; i++) {
        fp_s(" fb");
        fp_i(i);
        fp_s("=");
        fp_x((uint64_t)(uintptr_t)fb[i]);
    }
    fp_kv_u("tsc_hz", g_fp.tsc_hz);
    fp_kv_i("neo", sceKernelIsNeoMode());
    fp_kv_i("eq_ok", eq_ok);
    fp_kv_i("alloc_ok", alloc_ok);
    fp_kv_x("gpu_mem", (uint64_t)(uintptr_t)g_fp.gpu);
    fp_kv_x("progress_addr", (uint64_t)(uintptr_t)&g_fp.gpu->progress);
    fp_kv_u("submit_size", sizeof(struct fp_submit));
    fp_s("\n");
    for (int p = 0; p < FP_NUM_PHASES; p++) {
        fp_s("PHASE_CFG");
        fp_kv_i("n", p);
        fp_s(" name=");
        fp_s(g_fp_phases[p].name);
        fp_kv_i("kind", g_fp_phases[p].kind);
        fp_kv_i("max_pending", g_fp_phases[p].max_pending);
        fp_kv_i("submits", g_fp_phases[p].submits);
        fp_s("\n");
    }
    fp_flush_out();
    if (!alloc_ok)
        return 3;

    /* Whatever the loading screen queued finishes first. */
    fp_s("PRE_DRAIN");
    fp_kv_i("timeout", fp_drain());
    fp_kv_x("lab0", g_fp.label[0]);
    fp_kv_x("lab1", g_fp.label[1]);
    fp_s("\n");
    fp_flush_out();

    void* thr = 0;
    int thr_ret = scePthreadCreate(&thr, 0, fp_poller, 0, "flipprobe_poll");
    fp_s("POLLER");
    fp_kv_x("ret", (uint32_t)thr_ret);
    fp_s("\n");
    fp_flush_out();
    sceKernelUsleep(50000);

    for (int p = 0; p < FP_NUM_PHASES; p++) {
        const struct fp_phase* ph = &g_fp_phases[p];
        fp_s("PHASE_BEGIN");
        fp_kv_i("n", p);
        fp_s(" name=");
        fp_s(ph->name);
        fp_kv_u("tsc", sceKernelReadTsc());
        fp_kv_u("first_submit", g_fp.nsub);
        fp_kv_x("lab0", g_fp.label[0]);
        fp_kv_x("lab1", g_fp.label[1]);
        fp_s("\n");
        for (int k = 0; k < ph->submits && g_fp.nsub < FP_MAX_SUBMITS; k++) {
            switch (ph->kind) {
            case FP_EOP_ALT:
                fp_submit((uint32_t)p, 1, k & 1, ph->max_pending);
                break;
            case FP_IMM_ALT:
                fp_submit((uint32_t)p, 0, k & 1, ph->max_pending);
                break;
            case FP_EOP_SAME:
                fp_submit((uint32_t)p, 1, 0, ph->max_pending);
                break;
            case FP_EOP_IMM_MIX: {
                OrbisVideoOutFlipStatus fs;
                my_memset(&fs, 0, sizeof(fs));
                sceVideoOutGetFlipStatus(video, &fs);
                int on = (fs.currentBuffer == 1) ? 1 : 0;
                if ((k & 1) == 0)
                    fp_submit((uint32_t)p, 1, on, ph->max_pending + 1);
                else
                    fp_submit((uint32_t)p, 0, on ^ 1, ph->max_pending + 1);
                break;
            }
            }
            if ((g_fp.nsub % FP_FLUSH_EVERY) == 0)
                fp_flush_records();
        }
        int dt = fp_drain();
        fp_s("PHASE_END");
        fp_kv_i("n", p);
        fp_s(" name=");
        fp_s(ph->name);
        fp_kv_u("tsc", sceKernelReadTsc());
        fp_kv_u("last_submit", g_fp.nsub);
        fp_kv_i("drain_timeout", dt);
        fp_kv_x("lab0", g_fp.label[0]);
        fp_kv_x("lab1", g_fp.label[1]);
        fp_s("\n");
        fp_flush_records();
    }

    g_fp.stop = 1;
    void* rv = 0;
    int join_ret = thr_ret == 0 ? scePthreadJoin(thr, &rv) : -1;
    fp_flush_records();
    fp_analyse();
    fp_s("=== FLIP_PROBE end");
    fp_kv_u("submits", g_fp.nsub);
    fp_kv_u("events", g_fp.nev);
    fp_kv_u("event_drops", g_fp.ev_drop);
    fp_kv_u("qevents", g_fp.nqev);
    fp_kv_u("polls", g_fp.polls);
    fp_kv_u("max_poll_gap_tsc", g_fp.max_poll_gap);
    fp_kv_x("join_ret", (uint32_t)join_ret);
    fp_kv_i("poll_affinity_ret", g_fp.poll_affinity_ret);
    fp_kv_i("gpu_label_snapshot", FP_GPU_LABEL_SNAPSHOT);
    fp_s("\n");
    fp_flush_out();
    if (g_fp.fd >= 0)
        sceKernelClose(g_fp.fd);
    return 0;
}
