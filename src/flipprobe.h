#pragma once
/* ==== FLIP_PROBE: VideoOut buffer label vs EOP flips ==========================================
   Records what the display buffer labels do around EOP flips, on hardware and on shadPS4, to
   settle whether label[b] can be 0 when an EOP flip of b completes (shadPS4 asserts it is 1).

   Phases A..F run once, each followed by a drain (durations at 60 Hz, about four minutes), then
   G runs until the app is closed:
     A eop_alt_p1   30 s  EOP flips (sceGnmSubmitAndFlipCommandBuffers, plain 0x68750777
                          marker) of buffers 0,1,0,1..., the next once nothing is pending
     B eop_alt_p2   60 s  the same, up to 2 pending: frame k finishes on the GPU while k-1 waits
     C eop_alt_p8   60 s  the same, up to 8 pending
     D imm_alt_p2   30 s  immediate flips (sceVideoOutSubmitFlip) of 0,1,0,1..., up to 2 pending
     E eop_same_p2  30 s  EOP flips of buffer 0 only, up to 2 pending (re-flipping a buffer)
     F eop_imm_mix  30 s  an EOP flip of the buffer on screen, then an immediate flip of the
                          other
     G soak         ever  EOP flips of 0,1,0,1... with GPU work per frame on a random walk of
                          0..FP_LOAD_MAX 1 MB CP DMA copies, and max pending cycling 1..8, one
                          step every FP_SOAK_STEP submits

   Recorded:
     - per submit: phase, buffer, flip arg, GPU load, max pending, CPU label reads before /
       after, flip and vblank status before, every return code, TSC stamps around pacing,
       submit and SubmitDone;
     - per submit, from the GPU: the GPU clock at the start of the frame and just before the
       marker, the label as the GPU read it there (DMA copy, FP_GPU_LABEL_SNAPSHOT), and a
       progress word the poller watches;
     - the 64-dword marker block before the submit and after libSceGnmDriver patched it;
     - a poller thread: every change of label[0], label[1], the progress word, the flip status
       and the vblank count, TSC stamped;
     - every flip event from the equeue.
   Everything lives in ring buffers streamed to /user/data/ShadCube4/flipprobe.log every
   FP_FLUSH_EVERY submits, so the run can go on indefinitely and a crash loses at most that many.
   The per-submit analysis is tools/flipprobe_summary.py, from the raw records.
   Screen: both display buffers get the same picture (no flicker): a segment per phase - green
   done, white filling, grey to come - and an overall bar for A..F; in G its segment fills once
   a minute and a square per minute is added below. */

extern uint64_t sceKernelReadTsc(void);
extern uint64_t sceKernelGetTscFrequency(void);
extern int sceKernelIsNeoMode(void);

/* Submits per second of a phase: one per vblank at 60 Hz. */
#ifndef FP_PER_SECOND
#define FP_PER_SECOND 60
#endif
/* Submits after which G stops; 0 runs it until the app is closed. */
#ifndef FP_SOAK_LIMIT
#define FP_SOAK_LIMIT 0
#endif
/* Ring sizes; records are streamed out every FP_FLUSH_EVERY submits, far below these. */
#define FP_SUB_RING 1024u
#define FP_GPU_RING 4096u
#define FP_EV_RING (1u << 19)
#define FP_QEV_RING 8192u
#define FP_DCB_SLOTS 32
#define FP_DCB_DWORDS 1024
#define FP_FLUSH_EVERY 64
#define FP_PACE_TIMEOUT_US 3000000ull
#define FP_DRAIN_TIMEOUT_US 5000000ull
#define FP_TAG_PLAIN 0x68750777u
/* G: GPU work per frame, in 1 MB CP DMA copies, and the max pending schedule. */
#define FP_LOAD_MAX 48
#define FP_LOAD_CHUNK 0x100000u
#define FP_SOAK_STEP 600
/* The progress screen is redrawn every this many submits. */
#define FP_SCREEN_EVERY 15
/* 1: the GPU copies label[bi] into gpu_label_pre just before the marker (DMA_DATA memory to
   memory). Hardware only: shadPS4 keeps the labels in host memory and executes this DMA through
   its buffer cache by guest address. 0: gpu_label_pre stays 0xffffffffffffffff. */
#define FP_GPU_LABEL_SNAPSHOT 0

enum { FP_EOP_ALT = 0, FP_IMM_ALT = 1, FP_EOP_SAME = 2, FP_EOP_IMM_MIX = 3, FP_SOAK = 4 };

struct fp_phase {
    const char* name;
    int kind;
    int max_pending;
    int submits; /* 0: until the app is closed */
};

static const struct fp_phase g_fp_phases[] = {
    {"A eop_alt_p1", FP_EOP_ALT, 1, 30 * FP_PER_SECOND},
    {"B eop_alt_p2", FP_EOP_ALT, 2, 60 * FP_PER_SECOND},
    {"C eop_alt_p8", FP_EOP_ALT, 8, 60 * FP_PER_SECOND},
    {"D imm_alt_p2", FP_IMM_ALT, 2, 30 * FP_PER_SECOND},
    {"E eop_same_p2", FP_EOP_SAME, 2, 30 * FP_PER_SECOND},
    {"F eop_imm_mix", FP_EOP_IMM_MIX, 2, 30 * FP_PER_SECOND},
    {"G soak", FP_SOAK, 0, FP_SOAK_LIMIT},
};
#define FP_NUM_PHASES ((int)(sizeof(g_fp_phases) / sizeof(g_fp_phases[0])))

/* One submit (an EOP flip or an immediate flip). */
struct fp_submit {
    uint32_t idx; /* submit index; also the flip arg */
    uint32_t phase;
    uint32_t eop; /* 1 EOP flip, 0 immediate flip */
    int32_t bi;
    int32_t ret;    /* SubmitAndFlip or SubmitFlip */
    int32_t sd_ret; /* SubmitDone after an EOP submit */
    int32_t pace_timeout;
    int32_t max_pending;
    uint32_t load; /* 1 MB CP DMA copies before the marker */
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

/* GPU-written per-submit data, by submit index modulo FP_GPU_RING. */
struct fp_gpu {
    volatile uint64_t label_pre[FP_GPU_RING]; /* label[bi] read by the GPU before the marker */
    volatile uint64_t ts[FP_GPU_RING];        /* GPU clock just before the marker */
    volatile uint64_t ts_start[FP_GPU_RING];  /* GPU clock at the start of the frame */
    volatile uint32_t progress;               /* submit index + 1, last one the GPU reached */
};

static struct {
    int video;
    volatile uint64_t* label;
    struct fp_gpu* gpu;
    uint32_t* dcb[FP_DCB_SLOTS];
    uint8_t* dma_src;
    uint8_t* dma_dst;
    struct fp_submit* sub; /* ring of FP_SUB_RING, by submit index */
    uint32_t nsub;
    struct fp_event* ev;   /* ring of FP_EV_RING, written by the poller */
    volatile uint64_t nev; /* events written so far, published by the poller */
    volatile uint32_t stop;
    volatile uint64_t polls;
    volatile uint64_t max_poll_gap;
    volatile long long poll_affinity_ret;
    struct fp_qevent* qev; /* ring of FP_QEV_RING */
    uint64_t nqev;
    OrbisKernelEqueue eq;
    int eq_ok;
    int fd;
    char* out;
    unsigned long out_n;
    uint32_t flushed_sub;
    uint64_t flushed_ev, flushed_qev, ev_lost, qev_lost;
    uint64_t tsc_hz;
    uint32_t* fb[2];
    uint32_t rng;
    int32_t load;
    /* The marker blocks last logged in full, and their submits. */
    uint32_t pre_full[64];
    uint32_t post_full[2][64];
    int64_t pre_full_idx;
    int64_t post_full_idx[2];
} g_fp;

/* ---- progress screen, drawn by the CPU into both display buffers alike ---- */
#define FP_BG 0xFF202428u
#define FP_TRACK 0xFF454A55u
#define FP_DONE 0xFF2FA84Fu
#define FP_FILL 0xFFFFFFFFu
#define FP_SEG_Y0 470
#define FP_SEG_Y1 560
#define FP_ALL_Y0 610
#define FP_ALL_Y1 640
#define FP_MIN_Y0 680
#define FP_MIN_Y1 700
#define FP_X0 160
#define FP_X1 1760
#define FP_GAP 16
static void fp_rect(int x0, int y0, int x1, int y1, uint32_t c) {
    for (int k = 0; k < 2; k++) {
        uint32_t* b = g_fp.fb[k];
        if (!b)
            continue;
        for (int y = y0; y < y1; y++) {
            uint32_t* row = b + (long)y * DISPLAY_W;
            for (int x = x0; x < x1; x++)
                row[x] = c;
        }
    }
}
/* Phase `phase` is `done` of `total` through (G: of the current minute, `minutes` before it). */
static void fp_screen(int phase, int done, int total, int minutes) {
    const int seg_w = (FP_X1 - FP_X0 - FP_GAP * (FP_NUM_PHASES - 1)) / FP_NUM_PHASES;
    long all_total = 0, all_done = 0;
    for (int p = 0; p < FP_NUM_PHASES; p++) {
        int x0 = FP_X0 + p * (seg_w + FP_GAP), x1 = x0 + seg_w;
        int finite = g_fp_phases[p].submits > 0;
        if (finite)
            all_total += g_fp_phases[p].submits;
        if (p < phase) {
            fp_rect(x0, FP_SEG_Y0, x1, FP_SEG_Y1, FP_DONE);
            if (finite)
                all_done += g_fp_phases[p].submits;
        } else if (p == phase) {
            int fx = x0 + (int)((long)seg_w * done / (total > 0 ? total : 1));
            fp_rect(x0, FP_SEG_Y0, fx, FP_SEG_Y1, FP_FILL);
            fp_rect(fx, FP_SEG_Y0, x1, FP_SEG_Y1, FP_TRACK);
            if (finite)
                all_done += done;
        } else {
            fp_rect(x0, FP_SEG_Y0, x1, FP_SEG_Y1, FP_TRACK);
        }
    }
    int ax = FP_X0 + (int)((long)(FP_X1 - FP_X0) * all_done / (all_total > 0 ? all_total : 1));
    fp_rect(FP_X0, FP_ALL_Y0, ax, FP_ALL_Y1, all_done >= all_total ? FP_DONE : FP_FILL);
    fp_rect(ax, FP_ALL_Y0, FP_X1, FP_ALL_Y1, FP_TRACK);
    /* G: a square per finished minute, wrapping after a full row. */
    if (minutes > 0) {
        const int sq = 20, step = 26, per_row = (FP_X1 - FP_X0) / step;
        int n = minutes % per_row;
        if (n == 0)
            n = per_row;
        fp_rect(FP_X0, FP_MIN_Y0, FP_X1, FP_MIN_Y1, FP_BG);
        for (int i = 0; i < n; i++)
            fp_rect(FP_X0 + i * step, FP_MIN_Y0, FP_X0 + i * step + sq, FP_MIN_Y1, FP_DONE);
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST); /* the write-combined stores land before a flip */
}

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
            uint64_t n = g_fp.nev;
            g_fp.ev[n % FP_EV_RING] = e;
            __atomic_store_n(&g_fp.nev, n + 1, __ATOMIC_RELEASE);
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
        struct fp_qevent* q = &g_fp.qev[g_fp.nqev++ % FP_QEV_RING];
        q->tsc = sceKernelReadTsc();
        q->ident = ev.ident;
        q->data = ev.data;
        q->filter = ev.filter;
        q->flags = ev.flags;
        q->fflags = (int32_t)ev.fflags;
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

/* G's load: a random walk over 0..FP_LOAD_MAX, steps of -4..+4. */
static uint32_t fp_next_load(void) {
    g_fp.rng ^= g_fp.rng << 13;
    g_fp.rng ^= g_fp.rng >> 17;
    g_fp.rng ^= g_fp.rng << 5;
    g_fp.load += (int32_t)(g_fp.rng % 9) - 4;
    if (g_fp.load < 0)
        g_fp.load = 0;
    if (g_fp.load > FP_LOAD_MAX)
        g_fp.load = FP_LOAD_MAX;
    return (uint32_t)g_fp.load;
}

/* ---- one submit ---- */
static uint32_t fp_build_dcb(uint32_t idx, int bi, uint32_t load, uint32_t** out) {
    uint32_t* d = g_fp.dcb[idx % FP_DCB_SLOTS];
    uint32_t g = idx % FP_GPU_RING;
    g_fp.gpu->label_pre[g] = ~0ull;
    g_fp.gpu->ts_start[g] = 0;
    g_fp.gpu->ts[g] = 0;
    struct PM4Builder bb;
    struct PM4Builder* b = &bb;
    pm4_init(b, d, FP_DCB_DWORDS);
    /* The frame's GPU work, bracketed by the GPU clock. */
    pm4_gpu_timestamp(b, &g_fp.gpu->ts_start[g]);
    for (uint32_t i = 0; i < load; i++)
        pm4_dma_copy(b, g_fp.dma_dst, g_fp.dma_src, FP_LOAD_CHUNK);
        /* The label as the GPU sees it before the marker, then the GPU clock, then the progress
           word: all ordered behind each other (cp_sync DMA, end-of-pipe timestamp, confirmed
           write). */
#if FP_GPU_LABEL_SNAPSHOT
    pm4_dma_copy(b, (void*)&g_fp.gpu->label_pre[g], (const void*)&g_fp.label[bi], 8);
#endif
    pm4_gpu_timestamp(b, &g_fp.gpu->ts[g]);
    pm4_write_data_dword(b, &g_fp.gpu->progress, idx + 1);
    /* The plain prepareFlip marker, NHL's form: 64 dwords, tag 0x68750777, the rest zero. */
    pm4_emit(b, pm4_type3(PM4_NOP, 63));
    pm4_emit(b, FP_TAG_PLAIN);
    for (int i = 0; i < 62; i++)
        pm4_emit(b, 0);
    *out = d;
    return b->overflow ? 0 : b->off;
}

static void fp_submit(uint32_t phase, int eop, int bi, int max_pending, uint32_t load) {
    struct fp_submit* s = &g_fp.sub[g_fp.nsub % FP_SUB_RING];
    my_memset(s, 0, sizeof(*s));
    s->idx = g_fp.nsub;
    s->phase = phase;
    s->eop = (uint32_t)eop;
    s->bi = bi;
    s->max_pending = max_pending;
    s->load = eop ? load : 0;
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
        uint32_t dw = fp_build_dcb(s->idx, bi, s->load, &d);
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
        uint32_t g = s->idx % FP_GPU_RING;
        /* An immediate flip has no command buffer: no GPU label read, no GPU clock. */
        g_fp.gpu->label_pre[g] = ~0ull;
        g_fp.gpu->ts_start[g] = 0;
        g_fp.gpu->ts[g] = 0;
        s->t_sub0 = sceKernelReadTsc();
        s->ret = sceVideoOutSubmitFlip(g_fp.video, bi, 1, (int64_t)s->idx);
        s->t_sub1 = sceKernelReadTsc();
    }
    s->lab_after[0] = g_fp.label[0];
    s->lab_after[1] = g_fp.label[1];
    g_fp.nsub++;
    fp_drain_queue();
}

/* ---- log records ---- */
static void fp_log_submit(const struct fp_submit* s) {
    uint32_t g = s->idx % FP_GPU_RING;
    fp_s("SUB");
    fp_kv_u("idx", s->idx);
    fp_s(" phase=");
    fp_s(g_fp_phases[s->phase].name);
    fp_kv_u("eop", s->eop);
    fp_kv_i("bi", s->bi);
    fp_kv_u("load", s->load);
    fp_kv_i("max_pending", s->max_pending);
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
    fp_kv_x("gpu_label_pre", g_fp.gpu->label_pre[g]);
    fp_kv_u("gpu_ts_start", g_fp.gpu->ts_start[g]);
    fp_kv_u("gpu_ts", g_fp.gpu->ts[g]);
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
    if (!s->eop)
        return;
    /* A marker block in full when it differs from the last one logged in full (per buffer for
       the patched block); otherwise the submit it is identical to. */
    int same_pre = g_fp.pre_full_idx >= 0;
    for (int i = 0; same_pre && i < 64; i++)
        same_pre = g_fp.pre_full[i] == s->tail_pre[i];
    if (same_pre) {
        fp_s("TAIL_PRE_SAME");
        fp_kv_u("submit", s->idx);
        fp_kv_u("as", (uint64_t)g_fp.pre_full_idx);
        fp_s("\n");
    } else {
        fp_dwords("TAIL_PRE ", s->idx, s->tail_pre);
        for (int i = 0; i < 64; i++)
            g_fp.pre_full[i] = s->tail_pre[i];
        g_fp.pre_full_idx = s->idx;
    }
    int b = s->bi & 1;
    int same_post = g_fp.post_full_idx[b] >= 0;
    for (int i = 0; same_post && i < 64; i++)
        same_post = g_fp.post_full[b][i] == s->tail_post[i];
    if (same_post) {
        fp_s("TAIL_POST_SAME");
        fp_kv_u("submit", s->idx);
        fp_kv_u("as", (uint64_t)g_fp.post_full_idx[b]);
        fp_s("\n");
    } else {
        fp_dwords("TAIL_POST", s->idx, s->tail_post);
        for (int i = 0; i < 64; i++)
            g_fp.post_full[b][i] = s->tail_post[i];
        g_fp.post_full_idx[b] = s->idx;
    }
}

static void fp_log_event(uint64_t i, const struct fp_event* e) {
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

static void fp_log_qevent(uint64_t i, const struct fp_qevent* q) {
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

/* Everything recorded since the previous flush. A ring that was lapped reports what it lost. */
static void fp_flush_records(void) {
    while (g_fp.flushed_sub < g_fp.nsub)
        fp_log_submit(&g_fp.sub[g_fp.flushed_sub++ % FP_SUB_RING]);
    uint64_t n = __atomic_load_n(&g_fp.nev, __ATOMIC_ACQUIRE);
    if (n - g_fp.flushed_ev > FP_EV_RING) {
        uint64_t lost = n - g_fp.flushed_ev - FP_EV_RING;
        g_fp.ev_lost += lost;
        fp_s("EV_LOST");
        fp_kv_u("from", g_fp.flushed_ev);
        fp_kv_u("count", lost);
        fp_s("\n");
        g_fp.flushed_ev += lost;
    }
    while (g_fp.flushed_ev < n) {
        fp_log_event(g_fp.flushed_ev, &g_fp.ev[g_fp.flushed_ev % FP_EV_RING]);
        g_fp.flushed_ev++;
    }
    if (g_fp.nqev - g_fp.flushed_qev > FP_QEV_RING) {
        uint64_t lost = g_fp.nqev - g_fp.flushed_qev - FP_QEV_RING;
        g_fp.qev_lost += lost;
        fp_s("QEV_LOST");
        fp_kv_u("from", g_fp.flushed_qev);
        fp_kv_u("count", lost);
        fp_s("\n");
        g_fp.flushed_qev += lost;
    }
    while (g_fp.flushed_qev < g_fp.nqev) {
        fp_log_qevent(g_fp.flushed_qev, &g_fp.qev[g_fp.flushed_qev % FP_QEV_RING]);
        g_fp.flushed_qev++;
    }
    fp_flush_out();
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

static void fp_phase_line(const char* what, int p) {
    fp_s(what);
    fp_kv_i("n", p);
    fp_s(" name=");
    fp_s(g_fp_phases[p].name);
    fp_kv_u("tsc", sceKernelReadTsc());
    fp_kv_u("submit", g_fp.nsub);
    fp_kv_x("lab0", g_fp.label[0]);
    fp_kv_x("lab1", g_fp.label[1]);
}

/* Runs A..F, then G until the app is closed (or FP_SOAK_LIMIT submits). Uses the registered
   buffers 0 and 1; teardown stays with the caller. */
static int flip_probe_run(int video, void* const* fb, int nfb, void* label_base,
                          OrbisKernelEqueue eq, int eq_ok) {
    my_memset(&g_fp, 0, sizeof(g_fp));
    g_fp.video = video;
    g_fp.label = (volatile uint64_t*)label_base;
    g_fp.eq = eq;
    g_fp.eq_ok = eq_ok;
    g_fp.tsc_hz = sceKernelGetTscFrequency();
    g_fp.fb[0] = nfb > 0 ? (uint32_t*)fb[0] : 0;
    g_fp.fb[1] = nfb > 1 ? (uint32_t*)fb[1] : 0;
    g_fp.pre_full_idx = -1;
    g_fp.post_full_idx[0] = g_fp.post_full_idx[1] = -1;
    g_fp.rng = 0x9E3779B9u;
    g_fp.load = FP_LOAD_MAX / 2;
    fp_open();
    g_fp.out = (char*)gpu_alloc(FP_OUT_CAP, 0x4000);
    g_fp.gpu = (struct fp_gpu*)gpu_alloc(sizeof(struct fp_gpu), 0x10000);
    g_fp.sub = (struct fp_submit*)gpu_alloc(sizeof(struct fp_submit) * FP_SUB_RING, 0x4000);
    g_fp.ev = (struct fp_event*)gpu_alloc(sizeof(struct fp_event) * FP_EV_RING, 0x4000);
    g_fp.qev = (struct fp_qevent*)gpu_alloc(sizeof(struct fp_qevent) * FP_QEV_RING, 0x4000);
    g_fp.dma_src = (uint8_t*)gpu_alloc(FP_LOAD_CHUNK, 0x10000);
    g_fp.dma_dst = (uint8_t*)gpu_alloc(FP_LOAD_CHUNK, 0x10000);
    for (int i = 0; i < FP_DCB_SLOTS; i++)
        g_fp.dcb[i] = (uint32_t*)gpu_alloc(FP_DCB_DWORDS * 4, 0x4000);
    int alloc_ok =
        g_fp.out && g_fp.gpu && g_fp.sub && g_fp.ev && g_fp.qev && g_fp.dma_src && g_fp.dma_dst;
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
    fp_kv_i("gpu_label_snapshot", FP_GPU_LABEL_SNAPSHOT);
    fp_kv_u("load_max", FP_LOAD_MAX);
    fp_kv_x("load_chunk", FP_LOAD_CHUNK);
    fp_kv_u("soak_step", FP_SOAK_STEP);
    fp_kv_u("soak_limit", FP_SOAK_LIMIT);
    fp_kv_u("per_second", FP_PER_SECOND);
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

    /* Whatever the loading screen queued finishes first; then both buffers get the same picture,
       so flipping between them shows no change but the progress. */
    fp_s("PRE_DRAIN");
    fp_kv_i("timeout", fp_drain());
    fp_kv_x("lab0", g_fp.label[0]);
    fp_kv_x("lab1", g_fp.label[1]);
    fp_s("\n");
    fp_flush_out();
    fp_rect(0, 0, DISPLAY_W, DISPLAY_H, FP_BG);
    fp_screen(0, 0, g_fp_phases[0].submits, 0);

    void* thr = 0;
    int thr_ret = scePthreadCreate(&thr, 0, fp_poller, 0, "flipprobe_poll");
    fp_s("POLLER");
    fp_kv_x("ret", (uint32_t)thr_ret);
    fp_s("\n");
    fp_flush_out();
    sceKernelUsleep(50000);

    for (int p = 0; p < FP_NUM_PHASES; p++) {
        const struct fp_phase* ph = &g_fp_phases[p];
        fp_phase_line("PHASE_BEGIN", p);
        fp_s("\n");
        for (uint32_t k = 0; ph->submits == 0 || k < (uint32_t)ph->submits; k++) {
            switch (ph->kind) {
            case FP_EOP_ALT:
                fp_submit((uint32_t)p, 1, k & 1, ph->max_pending, 0);
                break;
            case FP_IMM_ALT:
                fp_submit((uint32_t)p, 0, k & 1, ph->max_pending, 0);
                break;
            case FP_EOP_SAME:
                fp_submit((uint32_t)p, 1, 0, ph->max_pending, 0);
                break;
            case FP_EOP_IMM_MIX: {
                OrbisVideoOutFlipStatus fs;
                my_memset(&fs, 0, sizeof(fs));
                sceVideoOutGetFlipStatus(video, &fs);
                int on = (fs.currentBuffer == 1) ? 1 : 0;
                if ((k & 1) == 0)
                    fp_submit((uint32_t)p, 1, on, ph->max_pending + 1, 0);
                else
                    fp_submit((uint32_t)p, 0, on ^ 1, ph->max_pending + 1, 0);
                break;
            }
            case FP_SOAK:
                fp_submit((uint32_t)p, 1, k & 1, 1 + (int)((k / FP_SOAK_STEP) % 8), fp_next_load());
                break;
            }
            if ((k % FP_SCREEN_EVERY) == 0) {
                if (ph->submits > 0) {
                    fp_screen(p, (int)k, ph->submits, 0);
                } else {
                    const uint32_t minute = 60 * FP_PER_SECOND;
                    fp_screen(p, (int)(k % minute), (int)minute, (int)(k / minute));
                }
            }
            if ((g_fp.nsub % FP_FLUSH_EVERY) == 0)
                fp_flush_records();
        }
        int dt = fp_drain();
        fp_phase_line("PHASE_END", p);
        fp_kv_i("drain_timeout", dt);
        fp_s("\n");
        fp_flush_records();
    }

    /* Both buffers carry the picture, so the one on screen shows it without another flip. */
    fp_screen(FP_NUM_PHASES, 0, 0, 0);
    g_fp.stop = 1;
    void* rv = 0;
    int join_ret = thr_ret == 0 ? scePthreadJoin(thr, &rv) : -1;
    fp_flush_records();
    fp_s("=== FLIP_PROBE end");
    fp_kv_u("submits", g_fp.nsub);
    fp_kv_u("events", g_fp.nev);
    fp_kv_u("events_lost", g_fp.ev_lost);
    fp_kv_u("qevents", g_fp.nqev);
    fp_kv_u("qevents_lost", g_fp.qev_lost);
    fp_kv_u("polls", g_fp.polls);
    fp_kv_u("max_poll_gap_tsc", g_fp.max_poll_gap);
    fp_kv_x("join_ret", (uint32_t)join_ret);
    fp_kv_i("poll_affinity_ret", g_fp.poll_affinity_ret);
    fp_s("\n");
    fp_flush_out();
    if (g_fp.fd >= 0)
        sceKernelClose(g_fp.fd);
    sceKernelUsleep(2000000);
    return 0;
}
