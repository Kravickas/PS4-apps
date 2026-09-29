/* Loading screen, drawn by the CPU straight into two of the linear display buffers (A8R8G8B8 sRGB,
   pitch DISPLAY_W: sceVideoOutSetBufferAttribute tiling 1) and flipped - no GPU work, so it runs
   from the moment video out is registered, before any texture, shader or table exists. Its first
   frame goes up, then the system splash is hidden (the system composites it over the app until
   sceSystemServiceHideSplashScreen; called once). Progress: the asset files' real sizes
   (sceKernelLseek), advanced as each is loaded (ls_file); within the model, by its loader.
   Redrawn only when the bar moves a pixel, at most once per LS_MIN_US (flips never outrun vsync).
   ls_finish traces every step's time ("load ms: ..."). Look: blue, a white bar over a dim track. */
#ifndef LOADSCREEN_H
#define LOADSCREEN_H

#define LS_BG 0xFF14338Cu    /* sRGB (0.08, 0.20, 0.55) */
#define LS_BAR 0xFFFFFFFFu   /* white */
#define LS_TRACK 0xFF8A8FB2u /* the background 25 % of the way to white, in linear light */
#define LS_X0 288            /* NDC x -0.7 .. 0.7 */
#define LS_X1 1632
#define LS_Y0 837 /* NDC y -0.55 .. -0.65 */
#define LS_Y1 891
#define LS_MIN_US 20000u
#define LS_STEPS 24

static const char* const g_ls_files[] = {
    ASSET_DIR "sound/bgm/bgm.wav",       ASSET_DIR "images/cube/albedo.dds",
    ASSET_DIR "images/cube/normal.dds",  ASSET_DIR "images/cube/height.dds",
    ASSET_DIR "images/floor/albedo.dds", ASSET_DIR "images/flare/glare.dds",
    ASSET_DIR "ui/ui_atlas.bin",         ASSET_DIR "ui/clock_sdf.bin",
    ASSET_DIR "images/floor/normal.dds", ASSET_DIR "images/floor/height.dds",
    ASSET_DIR "images/moon/albedo.dds",  ASSET_DIR "sky/atmosphere.bin",
    ASSET_DIR "models/cube/cube.obj"};
#define LS_NFILES ((int)(sizeof(g_ls_files) / sizeof(g_ls_files[0])))

static struct {
    int on, video, idx, drawn_px, bg_done[2], splash_ret, nsteps;
    void* fb[2];
    long size[LS_NFILES];
    char counted[LS_NFILES];
    long total, done;
    uint64_t last_us, t0_us, step_us[LS_STEPS];
    const char* step[LS_STEPS];
} g_ls;

static void ls_flip(int px) {
    uint32_t* b = (uint32_t*)g_ls.fb[g_ls.idx];
    if (!g_ls.bg_done[g_ls.idx]) {
        for (long i = 0; i < (long)DISPLAY_W * DISPLAY_H; i++)
            b[i] = LS_BG;
        g_ls.bg_done[g_ls.idx] = 1;
    }
    for (int y = LS_Y0; y < LS_Y1; y++) {
        uint32_t* row = b + (long)y * DISPLAY_W;
        for (int x = LS_X0; x < LS_X1; x++)
            row[x] = x < LS_X0 + px ? LS_BAR : LS_TRACK;
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST); /* the write-combined stores land before the flip */
    sceVideoOutSubmitFlip(g_ls.video, g_ls.idx, 1, 0);
    g_ls.idx ^= 1;
    g_ls.drawn_px = px;
    g_ls.last_us = sceKernelGetProcessTime();
}

/* The bar at done + part of the next `next` bytes (part 0..1). */
static void ls_show(long next, float part, int force) {
    if (!g_ls.on)
        return;
    double f =
        g_ls.total > 0 ? ((double)g_ls.done + (double)next * part) / (double)g_ls.total : 1.0;
    int px = (int)((f > 1.0 ? 1.0 : f) * (LS_X1 - LS_X0) + 0.5);
    if (px == g_ls.drawn_px && !force)
        return;
    uint64_t now = sceKernelGetProcessTime();
    if (!force && now - g_ls.last_us < LS_MIN_US)
        return;
    if (force && now - g_ls.last_us < LS_MIN_US)
        sceKernelUsleep((unsigned int)(LS_MIN_US - (now - g_ls.last_us)));
    ls_flip(px);
}

static void ls_mark(const char* what) {
    if (g_ls.on && g_ls.nsteps < LS_STEPS) {
        g_ls.step[g_ls.nsteps] = what;
        g_ls.step_us[g_ls.nsteps++] = sceKernelGetProcessTime();
    }
}

static void ls_start(int video, void* fb0, void* fb1) {
    g_ls.video = video;
    g_ls.fb[0] = fb0;
    g_ls.fb[1] = fb1;
    g_ls.drawn_px = -1;
    for (int i = 0; i < LS_NFILES; i++) {
        int fd = sceKernelOpen(g_ls_files[i], 0, 0);
        g_ls.size[i] = 0;
        if (fd >= 0) {
            long s = sceKernelLseek(fd, 0, 2 /* SEEK_END */);
            g_ls.size[i] = s > 0 ? s : 0;
            sceKernelClose(fd);
        }
        g_ls.total += g_ls.size[i];
    }
    g_ls.on = 1;
    g_ls.t0_us = sceKernelGetProcessTime();
    ls_flip(0);
    g_ls.splash_ret = sceSystemServiceHideSplashScreen(); /* our frame is up: the splash goes */
    ls_mark("start");
}

/* One asset file loaded: its size joins the bar. */
static void ls_file(const char* path) {
    for (int i = 0; g_ls.on && i < LS_NFILES; i++) {
        const char *a = g_ls_files[i], *b = path;
        while (*a && *a == *b)
            a++, b++;
        if (*a == 0 && *b == 0) {
            if (g_ls.counted[i])
                return; /* counted once */
            g_ls.counted[i] = 1;
            g_ls.done += g_ls.size[i];
            ls_show(0, 0.0f, 0);
            ls_mark(g_ls_files[i] + sizeof(ASSET_DIR) - 1);
            return;
        }
    }
}

/* The model loader's progress (obj / stl / ply), within the model file's share. */
static void ls_model_progress(float frac, const char* msg, void* ud) {
    (void)msg;
    (void)ud;
    ls_show(g_ls.size[LS_NFILES - 1], frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac), 0);
}

/* Everything loaded: the full bar, and every step's time to the trace. */
static void ls_finish(void) {
    if (!g_ls.on)
        return;
    g_ls.done = g_ls.total;
    ls_show(0, 0.0f, 1);
    ls_mark("ready");
    char L[1024];
    int p = 0;
    for (const char* q = "load ms:"; *q; q++)
        L[p++] = *q;
    for (int i = 1; i < g_ls.nsteps && p < 960; i++) {
        L[p++] = ' ';
        for (const char* q = g_ls.step[i]; *q && p < 1000; q++)
            L[p++] = *q;
        L[p++] = '=';
        p += lg_i64(L + p, (long long)((g_ls.step_us[i] - g_ls.step_us[i - 1]) / 1000u));
    }
    for (const char* q = " total="; *q; q++)
        L[p++] = *q;
    p += lg_i64(L + p, (long long)((g_ls.step_us[g_ls.nsteps - 1] - g_ls.t0_us) / 1000u));
    for (const char* q = " before_start="; *q; q++)
        L[p++] = *q;
    p += lg_i64(L + p, (long long)(g_ls.t0_us / 1000u));
    L[p++] = '\n';
    trace_line(L, (unsigned long)p);
    g_ls.on = 0;
}

#endif
