/* OPCODE_TEST 6: S_BFM_B64 on the PS4 GPU (GCN 1.1). ps_bfm64 (tools/gen_ps_bfm64.py) replaces the
   frame after the final pass: a 64-bit result grid per row (bit 63 left, white = 1; last column SCC
   after the instruction), PS_BFM64_ROWS rows on OPT_PAGES pages (touch pad: next page). Every row
   also stores {lo, hi, SCC, PS_BFM64_MARK + row} from one pixel; from frame 120 the CPU logs the
   whole table to the trace once every row's marker is visible. */
#pragma once

#define OPT_BUILD "optest-6b"
#define OPT_X0 570 /* grid: left edge, top edge (px) */
#define OPT_Y0 104
#define OPT_CW 20 /* cell width, row pitch (px) */
#define OPT_RH 20
#define OPT_PAGE_ROWS 48
#define OPT_PAGES ((PS_BFM64_ROWS + OPT_PAGE_ROWS - 1) / OPT_PAGE_ROWS)
#define OPT_LOG_FRAME 120
#define OPT_TOUCH_PAD 0x100000u         /* ORBIS_PAD_BUTTON_TOUCH_PAD */
#define OPT_TC_WB_ACTION_ENA (1u << 18) /* CP_COHER_CNTL: write back L2 (CI, cikd.h) */

static uint32_t* g_opt_tab = 0; /* table: grid, logging pixel, result V#, marker, row inputs */
static uint32_t* g_opt_res = 0; /* results: 4 dwords per row */
static void* g_ps_opt_gpu = 0;
static int g_opt_page = 0;
static int g_opt_ui_page = -1; /* the page in the UI buffer */
static int g_opt_logged = 0;
static int g_opt_tries = 0;

static void opt_set_page(int page) {
    float* f = (float*)g_opt_tab;
    int first = page * OPT_PAGE_ROWS, n = PS_BFM64_ROWS - first;
    n = n > OPT_PAGE_ROWS ? OPT_PAGE_ROWS : n;
    f[4] = (float)first;
    f[5] = (float)(n - 1);
}

static void opt_init(void) {
    g_opt_tab = (uint32_t*)gpu_alloc_typed((32 + 2 * PS_BFM64_ROWS) * 4, 0x100, MEM_TYPE_ONION);
    g_opt_res = (uint32_t*)gpu_alloc_typed(PS_BFM64_ROWS * 16, 0x100, MEM_TYPE_ONION);
    g_ps_opt_gpu = gpu_alloc_typed(sizeof(ps_bfm64_binary) + 256, 0x1000, MEM_TYPE_ONION);
    if (!g_opt_tab || !g_opt_res || !g_ps_opt_gpu) {
        g_ps_opt_gpu = 0;
        trace_msg("opt6: alloc failed\n");
        return;
    }
    my_memcpy(g_ps_opt_gpu, ps_bfm64_binary, sizeof(ps_bfm64_binary));
    my_memset(g_opt_res, 0, PS_BFM64_ROWS * 16);
    my_memset(g_opt_tab, 0, 32 * 4);
    float* f = (float*)g_opt_tab;
    f[0] = (float)OPT_X0;
    f[1] = (float)OPT_Y0;
    f[2] = 1.0f / OPT_CW;
    f[3] = 1.0f / OPT_RH;
    g_opt_tab[6] = OPT_X0 + 5; /* the logging pixel: row 0, column 0 of every page */
    g_opt_tab[7] = OPT_Y0 + 5;
    build_vsharp(g_opt_tab + 8, g_opt_res, PS_BFM64_ROWS * 16);
    g_opt_tab[12] = PS_BFM64_MARK;
    for (int k = 0; k < PS_BFM64_ROWS; k++) {
        g_opt_tab[32 + 2 * k] = ps_bfm64_rows[k].s2;
        g_opt_tab[33 + 2 * k] = ps_bfm64_rows[k].s3;
    }
    /* The labels' T# and S#: ps_ui's (the UI buffer, or the glare texture without the UI) until
       opt_ui draws a page. */
    if (g_post_tab)
        my_memcpy(g_opt_tab + 16, g_post_tab + UI_BLOCK * 32 + 36, 12 * 4);
    opt_set_page(0);
}

static void opt_input(uint32_t pressed) {
    if (pressed & OPT_TOUCH_PAD) {
        g_opt_page = (g_opt_page + 1) % OPT_PAGES;
        if (g_opt_tab)
            opt_set_page(g_opt_page);
    }
}

/* After the final pass: the test grid over the whole frame, then the results written back from
   L2 for the CPU (TC_WB_ACTION_ENA, cikd.h). */
static void opt_pass(struct PM4Builder* b, const uint32_t* bg_v) {
    if (!g_ps_opt_gpu)
        return;
    post_pass(b, g_frame, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_RGBA16F, g_ps_opt_gpu,
              PS_BFM64_RSRC1, g_opt_tab, bg_v);
    pm4_acquire_mem(b, COHER_TC_ACTION_ENA | OPT_TC_WB_ACTION_ENA);
}

static int opt_cat(char* o, const char* s) {
    int n = 0;
    while (s[n]) {
        o[n] = s[n];
        n++;
    }
    return n;
}

static int opt_hex8(char* o, uint32_t v) {
    static const char h[] = "0123456789ABCDEF";
    o[0] = '0', o[1] = 'x';
    for (int i = 0; i < 8; i++)
        o[2 + i] = h[(v >> (28 - 4 * i)) & 15];
    return 10;
}

/* The page's text in a UI buffer (only when the page changes): header, column ticks, row labels.
   The panels stay hidden; clock mode is off. */
static void opt_ui(void) {
    static const unsigned char white[3] = {255, 255, 255}, grey[3] = {190, 190, 196};
    g_clock_mode = 0;
    if (!g_ui.ok || !g_opt_tab || g_opt_ui_page == g_opt_page)
        return;
    int nb = (g_ui.cur + 1) % UI_BUFS;
    unsigned char* b = g_ui.buf[nb];
    g_ui.drawing = 0;
    g_ui.nclip = 0;
    ui_clear(b, 0, 0, UI_W, UI_H);
    g_ui.st[nb].valid = 0;
    g_ui.dirty[nb][0] = g_ui.dirty[nb][1] = g_ui.dirty[nb][2] = g_ui.dirty[nb][3] = 0;
    char t[160], *p = t;
    p += opt_cat(p,
                 "S_BFM_B64 on GCN 1.1 (Liverpool), OPCODE_TEST 6, build " OPT_BUILD "     page ");
    p += ui_fmt_int(p, g_opt_page + 1);
    *p++ = '/';
    p += ui_fmt_int(p, OPT_PAGES);
    p += opt_cat(p, " (touch pad)");
    *p = 0;
    ui_text(b, 40, 34, t, white);
    ui_text(b, 40, 62,
            "s[28:29] bits: bit 63 left, white = 1. Last column: SCC after. Inputs s2, s3 from the "
            "table. All rows: ShadCube4 trace.log",
            grey);
    static const int tick_col[6] = {0, 15, 31, 47, 63, 65};
    static const char* tick_txt[6] = {"63", "48", "32", "16", "0", "SCC"};
    for (int i = 0; i < 6; i++) {
        int x = OPT_X0 + tick_col[i] * OPT_CW + OPT_CW / 2 - ui_text_width(tick_txt[i]) / 2;
        ui_text(b, x, OPT_Y0 - 8, tick_txt[i], grey);
    }
    int first = g_opt_page * OPT_PAGE_ROWS;
    for (int r = 0; r < OPT_PAGE_ROWS && first + r < PS_BFM64_ROWS; r++) {
        const Bfm64Row* w = &ps_bfm64_rows[first + r];
        p = t;
        p += ui_fmt_int(p, first + r);
        *p++ = ' ', *p++ = w->group, *p++ = ' ', *p++ = ' ';
        p += opt_cat(p, w->label);
        p += opt_cat(p, "  [SCC ");
        *p++ = (char)('0' + w->scc_in);
        *p++ = ']';
        *p = 0;
        ui_text(b, 40, OPT_Y0 + r * OPT_RH + 14, t, white);
    }
    for (int i = 0; i < 4; i++)
        g_ui.rect[i] = g_ui.pill[i] = -1e6f;
    g_ui.cur = nb;
    g_opt_ui_page = g_opt_page;
    /* ps_ui composites the UI buffer inside the panels only: ps_bfm64 composites the labels */
    build_tsharp(g_opt_tab + 16, g_ui.buf[nb], UI_W, UI_H);
    build_ssharp_clamp(g_opt_tab + 24, 0);
}

/* From OPT_LOG_FRAME, every 60 frames until it succeeds (at most 10 tries): the whole table as a
   GitHub table once every row's marker is visible to the CPU. */
static void opt_log(long long frame) {
    if (!g_ps_opt_gpu || g_opt_logged || frame < OPT_LOG_FRAME ||
        (frame - OPT_LOG_FRAME) % 60 != 0 || g_opt_tries >= 10)
        return;
    g_opt_tries++;
    int seen = 0;
    for (int k = 0; k < PS_BFM64_ROWS; k++)
        seen += ((volatile uint32_t*)g_opt_res)[4 * k + 3] == PS_BFM64_MARK + (uint32_t)k;
    char t[320], *p = t;
    if (seen != PS_BFM64_ROWS) {
        p += opt_cat(p, "opt6: results not visible to the CPU (markers ");
        p += lg_u64(p, (unsigned long long)seen);
        *p++ = '/';
        p += lg_u64(p, PS_BFM64_ROWS);
        p += opt_cat(p, ", frame ");
        p += lg_u64(p, (unsigned long long)frame);
        p += opt_cat(p, "); shadPS4: readbacksMode = Precise\n");
        trace_line(t, (unsigned long)(p - t));
        return;
    }
    g_opt_logged = 1;
    trace_msg("\n## S_BFM_B64 on GCN 1.1 (OPCODE_TEST 6, build " OPT_BUILD ")\n\n"
              "s2, s3: the row's inputs from the table. SCC in: set by s_cmp before the "
              "instruction. Result: s[28:29] (loops: lo count, hi index sum).\n\n"
              "| # | grp | test | instruction | s2 | s3 | SCC in | result hi | result lo | SCC |\n"
              "|---|---|---|---|---|---|---|---|---|---|\n");
    for (int k = 0; k < PS_BFM64_ROWS; k++) {
        const Bfm64Row* w = &ps_bfm64_rows[k];
        const volatile uint32_t* r = (const volatile uint32_t*)g_opt_res + 4 * k;
        p = t;
        p += opt_cat(p, "| ");
        p += lg_u64(p, (unsigned long long)k);
        p += opt_cat(p, " | ");
        *p++ = w->group;
        p += opt_cat(p, " | ");
        p += opt_cat(p, w->label);
        p += opt_cat(p, " | `");
        p += opt_cat(p, w->ins);
        p += opt_cat(p, "` | ");
        p += opt_hex8(p, w->s2);
        p += opt_cat(p, " | ");
        p += opt_hex8(p, w->s3);
        p += opt_cat(p, " | ");
        *p++ = (char)('0' + w->scc_in);
        p += opt_cat(p, " | ");
        p += opt_hex8(p, r[1]);
        p += opt_cat(p, " | ");
        p += opt_hex8(p, r[0]);
        p += opt_cat(p, " | ");
        p += opt_hex8(p, r[2]);
        p += opt_cat(p, " |\n");
        trace_line(t, (unsigned long)(p - t));
    }
    trace_msg("\n");
}
