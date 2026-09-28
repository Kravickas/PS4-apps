/* On-screen panels: top left, always the OPTIONS hint, camera speed and day speed, plus the
   controls list when OPTIONS is pressed; top right, the leaderboard on CIRCLE (for now this
   session's play time). The CPU lays out text (URW Gothic Demi) and DS4 icons from
   assets/ui/ui_atlas.bin (tools/make_ui_atlas.py, src/ui_atlas.h) into a 1920x1080 RGBA8 texture
   (premultiplied, sRGB-space values), only when the content changes, triple-buffered so frames in
   flight keep reading theirs. ps_post_final blurs the scene under the panels' rounded rects
   (frosted glass) and composites the texture after its sRGB encode. */
#pragma once
#include "ui_atlas.h"

#define UI_W 1920
#define UI_H 1080
#define UI_BUFS 3
#define UI_X0 10 /* panel margin from the screen edges */
#define UI_Y0 10
#define UI_PAD 22       /* inside the panels */
#define UI_LH 38        /* line pitch */
#define UI_GAP 14       /* extra gap before the controls list */
#define UI_VALUE_GAP 16 /* between the widest label and the value column */
#define UI_LABEL_DX (2 * (UI_ICON_PX + 2) + 10)
/* Icons are centred on the text's cap height; lines reserve room for both. */
#define UI_ICON_ABOVE (UI_FONT_CAP / 2 + UI_ICON_PX / 2)
#define UI_ICON_BELOW (UI_ICON_PX - UI_ICON_ABOVE)
#define UI_LINE_ABOVE (UI_ICON_ABOVE > UI_FONT_ASCENT ? UI_ICON_ABOVE : UI_FONT_ASCENT)
#define UI_LINE_BELOW (UI_ICON_BELOW > UI_FONT_DESCENT ? UI_ICON_BELOW : UI_FONT_DESCENT)
#define UI_CLEAR_W 760 /* areas cleared before each redraw (left panel / leaderboard) */
#define UI_CLEAR_H 640
#define UI_LB_GAP 20 /* leaderboard: gap to the opened controls panel */

typedef struct {
    const unsigned char* atlas; /* RGBA8, UI_ATLAS_W x UI_ATLAS_H, straight alpha */
    unsigned char* buf[UI_BUFS];
    int cur, ok;
    int controls, leaderboard;
    char key[96];  /* content of the current buffer; recompose when it changes */
    float rect[8]; /* panels: centre x, y, half w, h (px) for the left panel and the leaderboard */
    int lb[4];     /* leaderboard rect x0, y0, x1, y1 (px), fixed at init */
} Ui;
static Ui g_ui;

static int ui_load_atlas(const char* path) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0)
        return -1;
    uint32_t hd[3];
    long r = sceKernelRead(fd, hd, 12);
    unsigned long n = (unsigned long)UI_ATLAS_W * UI_ATLAS_H * 4;
    if (r != 12 || hd[0] != 0x31414955u || hd[1] != UI_ATLAS_W || hd[2] != UI_ATLAS_H) {
        sceKernelClose(fd);
        return -2;
    }
    unsigned char* p = (unsigned char*)cpu_alloc(n, 0x1000); /* ONION: the CPU reads it */
    unsigned long got = 0;
    while (p && got < n) {
        long k = sceKernelRead(fd, p + got, n - got);
        if (k <= 0)
            break;
        got += (unsigned long)k;
    }
    sceKernelClose(fd);
    if (!p || got != n)
        return -3;
    g_ui.atlas = p;
    return 0;
}

static int ui_left_panel(unsigned char* b, int controls, int cam_pct, int day_mult, int day_frozen,
                         int* bottom);

static int ui_init(void) {
    my_memset(&g_ui, 0, sizeof(g_ui));
    for (int i = 0; i < 8; i++)
        g_ui.rect[i] = -1e6f;
    if (ui_load_atlas(ASSET_DIR "ui/ui_atlas.bin") != 0)
        return -1;
    for (int b = 0; b < UI_BUFS; b++) {
        g_ui.buf[b] = (unsigned char*)cpu_alloc((unsigned long)UI_W * UI_H * 4,
                                                0x1000); /* ONION: blended by the CPU */
        if (!g_ui.buf[b])
            return -2;
        my_memset(g_ui.buf[b], 0, (unsigned long)UI_W * UI_H * 4);
    }
    /* Leaderboard: centred, 10 % of the height above and below; left edge UI_LB_GAP right of
       the opened controls panel (measured in its widest state), right edge mirrored. */
    int bottom = 0, right = ui_left_panel(0, 1, 2500, 20, 1, &bottom);
    g_ui.lb[0] = right + UI_LB_GAP;
    g_ui.lb[1] = UI_H / 10;
    g_ui.lb[2] = UI_W - g_ui.lb[0];
    g_ui.lb[3] = UI_H - UI_H / 10;
    g_ui.ok = 1;
    return 0;
}

/* Premultiplied "over": dst = src * a + dst * (1 - a); tint replaces the atlas colour. */
static void ui_blit(unsigned char* dst, int sx, int sy, int w, int h, int dx, int dy,
                    const unsigned char* tint, int alpha256) {
    for (int j = 0; j < h; j++) {
        int y = dy + j;
        if (y < 0 || y >= UI_H)
            continue;
        const unsigned char* s = g_ui.atlas + ((unsigned long)(sy + j) * UI_ATLAS_W + sx) * 4;
        unsigned char* d = dst + ((unsigned long)y * UI_W + dx) * 4;
        for (int i = 0; i < w; i++, s += 4, d += 4) {
            int x = dx + i;
            int a = (s[3] * alpha256) >> 8;
            if (a == 0 || x < 0 || x >= UI_W)
                continue;
            const unsigned char* c = tint ? tint : s;
            for (int k = 0; k < 3; k++)
                d[k] = (unsigned char)((c[k] * a + d[k] * (255 - a) + 127) / 255);
            d[3] = (unsigned char)(a + (d[3] * (255 - a) + 127) / 255);
        }
    }
}

static int ui_glyph_index(char ch) {
    unsigned char c = (unsigned char)ch;
    return (c >= 32 && c <= 127) ? c - 32 : '?' - 32; /* 127 = the multiplication sign */
}

static int ui_text_width(const char* s) {
    int w = 0;
    while (*s)
        w += ui_glyph[ui_glyph_index(*s++)].adv;
    return w;
}

/* Text at baseline y with a soft shadow; returns the pen position after it. */
static int ui_text(unsigned char* dst, int x, int y, const char* s, const unsigned char col[3]) {
    static const unsigned char black[3] = {0, 0, 0};
    for (int pass = 0; pass < 2; pass++) {
        int cx = x;
        for (const char* p = s; *p; p++) {
            const UiGlyph* g = &ui_glyph[ui_glyph_index(*p)];
            if (pass == 0)
                ui_blit(dst, g->x, g->y, g->w, g->h, cx + g->xoff + 2, y + g->yoff + 2, black, 115);
            else
                ui_blit(dst, g->x, g->y, g->w, g->h, cx + g->xoff, y + g->yoff, col, 256);
            cx += g->adv;
        }
        if (pass == 1)
            return cx;
    }
    return x;
}

static void ui_icon(unsigned char* dst, int x, int baseline, int id) {
    ui_blit(dst, ui_icon_rect[id][0], ui_icon_rect[id][1], UI_ICON_PX, UI_ICON_PX, x,
            baseline - UI_ICON_ABOVE, 0, 256);
}

static int ui_fmt_int(char* p, long v) {
    char t[24];
    int n = 0;
    if (v < 0) {
        *p++ = '-';
        v = -v;
        n = 1;
    }
    int k = 0;
    do {
        t[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    for (int i = 0; i < k; i++)
        p[i] = t[k - 1 - i];
    p[k] = 0;
    return n + k;
}

static void ui_clear(unsigned char* b, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h && y < UI_H; y++)
        my_memset(b + ((unsigned long)y * UI_W + x0) * 4, 0, (unsigned long)w * 4);
}

/* Text or, when b is NULL, only its extent: returns the pen position after it. */
static int ui_put(unsigned char* b, int x, int y, const char* s, const unsigned char col[3]) {
    return b ? ui_text(b, x, y, s, col) : x + ui_text_width(s);
}

/* The left panel: the OPTIONS hint, camera speed, time of day speed and, if controls, the
   controls list. Draws into b, or only measures when b is NULL. Returns the panel's right edge
   (the widest laid-out line + padding) and sets *bottom. */
static int ui_left_panel(unsigned char* b, int controls, int cam_pct, int day_mult, int day_frozen,
                         int* bottom) {
    static const unsigned char white[3] = {255, 255, 255}, grey[3] = {205, 205, 210};
    int x = UI_X0 + UI_PAD, y = UI_Y0 + UI_PAD + UI_LINE_ABOVE, xr = x;
    int w1 = ui_text_width("Camera speed"), w2 = ui_text_width("Time of day speed");
    int value_dx = (w1 > w2 ? w1 : w2) + UI_VALUE_GAP, cx;
    if (b)
        ui_icon(b, x, y, UI_ICON_OPTIONS);
    cx = ui_put(b, x + UI_ICON_PX + 6, y, "to show / hide controls", white);
    xr = cx > xr ? cx : xr;
    char v[32];
    y += UI_LH;
    ui_put(b, x, y, "Camera speed", white);
    ui_fmt_int(v, cam_pct);
    int n = 0;
    while (v[n])
        n++;
    v[n] = '%';
    v[n + 1] = 0;
    cx = ui_put(b, x + value_dx, y, v, white);
    xr = cx > xr ? cx : xr;
    y += UI_LH;
    ui_put(b, x, y, "Time of day speed", white);
    v[0] = (char)UI_CHAR_TIMES; /* day_mult in tenths: x0.1 .. x0.9, then whole multiples */
    {
        int n = ui_fmt_int(v + 1, day_mult / 10) + 1;
        if (day_mult % 10) {
            v[n] = '.';
            v[n + 1] = (char)('0' + day_mult % 10);
            v[n + 2] = 0;
        }
    }
    cx = ui_put(b, x + value_dx, y, v, white);
    if (day_frozen)
        cx = ui_put(b, cx + 14, y, "(frozen)", grey);
    xr = cx > xr ? cx : xr;
    if (controls) {
        static const struct {
            int icons[2];
            const char* label;
        } rows[] = {
            {{UI_ICON_CROSS, -1}, "Freeze / unfreeze cube"},
            {{UI_ICON_CIRCLE, -1}, "Show / hide leaderboard"},
            {{UI_ICON_SQUARE, -1}, "Freeze / unfreeze day and night"},
            {{UI_ICON_TRIANGLE, -1}, "Reset camera"},
            {{UI_ICON_L1, UI_ICON_R1}, "Day and night slower / faster"},
            {{UI_ICON_L2, UI_ICON_R2}, "Camera down / up"},
            {{UI_ICON_DPAD_UP, UI_ICON_DPAD_DOWN}, "Camera speed up / down"},
            {{UI_ICON_DPAD_LEFT, UI_ICON_DPAD_RIGHT}, "Move the sun (time of day)"},
            {{UI_ICON_L_2D, -1}, "Move"},
            {{UI_ICON_R_2D, -1}, "Look"},
        };
        y += UI_LH + UI_GAP;
        for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
            int ix = x;
            for (int i = 0; i < 2 && rows[r].icons[i] >= 0; i++) {
                if (b)
                    ui_icon(b, ix, y, rows[r].icons[i]);
                ix += UI_ICON_PX + 2;
            }
            xr = ix > xr ? ix : xr;
            cx = ui_put(b, x + UI_LABEL_DX, y, rows[r].label, white);
            xr = cx > xr ? cx : xr;
            y += UI_LH;
        }
        y -= UI_LH;
    }
    *bottom = y + UI_LINE_BELOW + UI_PAD;
    return xr + UI_PAD;
}

/* Lays out both panels into the next buffer when the content key changes; sets the rects. */
static void ui_update(int cam_pct, int day_mult, int day_frozen, unsigned long play_s) {
    if (!g_ui.ok)
        return;
    char key[96], *k = key;
    k += ui_fmt_int(k, g_ui.controls);
    *k++ = ',';
    k += ui_fmt_int(k, g_ui.leaderboard);
    *k++ = ',';
    k += ui_fmt_int(k, cam_pct);
    *k++ = ',';
    k += ui_fmt_int(k, day_mult);
    *k++ = ',';
    k += ui_fmt_int(k, day_frozen);
    *k++ = ',';
    k += ui_fmt_int(k, g_ui.leaderboard ? (long)play_s : 0);
    int same = 1;
    for (int i = 0; key[i] || g_ui.key[i]; i++)
        if (key[i] != g_ui.key[i]) {
            same = 0;
            break;
        }
    if (same)
        return;
    for (int i = 0; i < (int)sizeof(key); i++)
        g_ui.key[i] = key[i];

    static const unsigned char white[3] = {255, 255, 255}, grey[3] = {205, 205, 210};
    int nb = (g_ui.cur + 1) % UI_BUFS;
    unsigned char* b = g_ui.buf[nb];
    ui_clear(b, 0, 0, UI_CLEAR_W, UI_CLEAR_H);
    ui_clear(b, g_ui.lb[0], g_ui.lb[1], g_ui.lb[2] - g_ui.lb[0], g_ui.lb[3] - g_ui.lb[1]);

    /* left panel: rect = the bounding box of what was laid out */
    int bottom = 0, right = ui_left_panel(b, g_ui.controls, cam_pct, day_mult, day_frozen, &bottom);
    float x0 = (float)UI_X0, y0 = (float)UI_Y0, x1 = (float)right, y1 = (float)bottom;
    g_ui.rect[0] = 0.5f * (x0 + x1);
    g_ui.rect[1] = 0.5f * (y0 + y1);
    g_ui.rect[2] = 0.5f * (x1 - x0);
    g_ui.rect[3] = 0.5f * (y1 - y0);

    /* leaderboard (fixed rect, centred; set by ui_init): this session's play time until
       multiplayer scores exist */
    if (g_ui.leaderboard) {
        char t[24], *q = t;
        q += ui_fmt_int(q, (long)(play_s / 3600));
        *q++ = ':';
        *q++ = (char)('0' + (play_s / 600) % 6);
        *q++ = (char)('0' + (play_s / 60) % 10);
        *q++ = ':';
        *q++ = (char)('0' + (play_s % 60) / 10);
        *q++ = (char)('0' + play_s % 10);
        *q = 0;
        int lx0 = g_ui.lb[0], lx1 = g_ui.lb[2], ly = g_ui.lb[1] + UI_PAD + UI_LINE_ABOVE;
        ui_text(b, lx0 + UI_PAD, ly, "Leaderboard", white);
        ly += UI_LH;
        ui_text(b, lx0 + UI_PAD, ly, "You", white);
        ui_text(b, lx1 - UI_PAD - ui_text_width(t), ly, t, white);
        ly += UI_LH;
        ui_text(b, lx0 + UI_PAD, ly, "Multiplayer scores soon", grey);
        g_ui.rect[4] = 0.5f * (float)(g_ui.lb[0] + g_ui.lb[2]);
        g_ui.rect[5] = 0.5f * (float)(g_ui.lb[1] + g_ui.lb[3]);
        g_ui.rect[6] = 0.5f * (float)(g_ui.lb[2] - g_ui.lb[0]);
        g_ui.rect[7] = 0.5f * (float)(g_ui.lb[3] - g_ui.lb[1]);
    } else {
        for (int i = 4; i < 8; i++)
            g_ui.rect[i] = -1e6f;
    }
    g_ui.cur = nb;
}

/* UI pass table (ps_ui) dwords 28..47: rects, the current buffer's T# (RGBA8 UNORM, linear) and a
   point S#. */
static void ui_write_table(uint32_t* blk) {
    float* r = (float*)(blk + 28);
    for (int i = 0; i < 8; i++)
        r[i] = g_ui.ok ? g_ui.rect[i] : -1e6f;
    if (g_ui.ok) {
        build_tsharp(blk + 36, g_ui.buf[g_ui.cur], UI_W, UI_H);
        build_ssharp_clamp(blk + 44, 0);
    }
}
