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
#define UI_X0 40 /* left panel origin */
#define UI_Y0 40
#define UI_PAD 22
#define UI_LH 50        /* line height */
#define UI_VALUE_DX 220 /* value column of the always-visible lines */
#define UI_LABEL_DX (2 * (UI_ICON_PX + 2) + 12)
#define UI_LB_W 420    /* leaderboard width, top right */
#define UI_CLEAR_W 900 /* areas cleared before each redraw (left panel / leaderboard) */
#define UI_CLEAR_H 900

typedef struct {
    const unsigned char* atlas; /* RGBA8, UI_ATLAS_W x UI_ATLAS_H, straight alpha */
    unsigned char* buf[UI_BUFS];
    int cur, ok;
    int controls, leaderboard;
    char key[96];  /* content of the current buffer; recompose when it changes */
    float rect[8]; /* panels: centre x, y, half w, h (px) for the left panel and the leaderboard */
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
            baseline - UI_FONT_ASCENT - 10, 0, 256);
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
    ui_clear(b, UI_W - UI_LB_W - 80, 0, UI_LB_W + 80, 300);

    /* left panel: always-visible lines */
    int x = UI_X0 + UI_PAD, y = UI_Y0 + UI_PAD + UI_FONT_ASCENT + 8, wmax = 0;
    int cx = ui_text(b, x, y, "Press ", white);
    ui_icon(b, cx, y, UI_ICON_OPTIONS);
    cx = ui_text(b, cx + UI_ICON_PX + 6, y, "to show / hide controls", white);
    wmax = cx - x;
    char v[32];
    y += UI_LH;
    ui_text(b, x, y, "Camera speed", white);
    ui_fmt_int(v, cam_pct);
    int n = 0;
    while (v[n])
        n++;
    v[n] = '%';
    v[n + 1] = 0;
    ui_text(b, x + UI_VALUE_DX, y, v, white);
    y += UI_LH;
    ui_text(b, x, y, "Day speed", white);
    v[0] = (char)UI_CHAR_TIMES;
    ui_fmt_int(v + 1, day_mult);
    cx = ui_text(b, x + UI_VALUE_DX, y, v, white);
    if (day_frozen)
        ui_text(b, cx + 14, y, "(frozen)", grey);
    if (g_ui.controls) {
        static const struct {
            int icons[4];
            const char* label;
        } rows[] = {
            {{UI_ICON_CROSS, -1, -1, -1}, "Freeze / unfreeze cube"},
            {{UI_ICON_CIRCLE, -1, -1, -1}, "Show / hide leaderboard"},
            {{UI_ICON_SQUARE, -1, -1, -1}, "Freeze / unfreeze day and night"},
            {{UI_ICON_TRIANGLE, -1, -1, -1}, "Reset camera"},
            {{UI_ICON_L1, UI_ICON_R1, -1, -1}, "Day and night slower / faster"},
            {{UI_ICON_L2, UI_ICON_R2, -1, -1}, "Camera down / up"},
            {{UI_ICON_DPAD_UP, UI_ICON_DPAD_DOWN, -1, -1}, "Camera speed up / down"},
            {{UI_ICON_DPAD_LEFT, UI_ICON_DPAD_RIGHT, -1, -1}, "Move the sun (time of day)"},
            {{UI_ICON_L_2D, -1, -1, -1}, "Move"},
            {{UI_ICON_R_2D, -1, -1, -1}, "Look"},
            {{UI_ICON_OPTIONS, -1, -1, -1}, "Show / hide controls"},
            {{UI_ICON_L1, UI_ICON_R1, UI_ICON_L2, UI_ICON_R2}, "Hold all four: quit"},
        };
        y += UI_LH + 18;
        for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
            int ix = x, ni = 0;
            for (int i = 0; i < 4 && rows[r].icons[i] >= 0; i++, ni++) {
                ui_icon(b, ix, y, rows[r].icons[i]);
                ix += UI_ICON_PX + 2;
            }
            int lx = x + (ni > 2 ? ni * (UI_ICON_PX + 2) + 12 : UI_LABEL_DX);
            cx = ui_text(b, lx, y, rows[r].label, white);
            if (cx - x > wmax)
                wmax = cx - x;
            y += UI_LH;
        }
        y -= UI_LH;
    }
    float x0 = (float)UI_X0, y0 = (float)UI_Y0, x1 = (float)(x + wmax + UI_PAD),
          y1 = (float)(y + 14 + UI_PAD);
    g_ui.rect[0] = 0.5f * (x0 + x1);
    g_ui.rect[1] = 0.5f * (y0 + y1);
    g_ui.rect[2] = 0.5f * (x1 - x0);
    g_ui.rect[3] = 0.5f * (y1 - y0);

    /* leaderboard (top right): this session's play time until multiplayer scores exist */
    if (g_ui.leaderboard) {
        int lx0 = UI_W - UI_X0 - UI_LB_W, ly = UI_Y0 + UI_PAD + UI_FONT_ASCENT + 8;
        ui_text(b, lx0 + UI_PAD, ly, "Leaderboard", white);
        ly += UI_LH + 6;
        ui_text(b, lx0 + UI_PAD, ly, "You", white);
        char t[24], *q = t;
        q += ui_fmt_int(q, (long)(play_s / 3600));
        *q++ = ':';
        *q++ = (char)('0' + (play_s / 600) % 6);
        *q++ = (char)('0' + (play_s / 60) % 10);
        *q++ = ':';
        *q++ = (char)('0' + (play_s % 60) / 10);
        *q++ = (char)('0' + play_s % 10);
        *q = 0;
        ui_text(b, UI_W - UI_X0 - UI_PAD - ui_text_width(t), ly, t, white);
        ly += UI_LH;
        ui_text(b, lx0 + UI_PAD, ly, "Multiplayer scores soon", grey);
        float lx1 = (float)(UI_W - UI_X0), ly1 = (float)(ly + 14 + UI_PAD);
        g_ui.rect[4] = 0.5f * ((float)lx0 + lx1);
        g_ui.rect[5] = 0.5f * ((float)UI_Y0 + ly1);
        g_ui.rect[6] = 0.5f * (lx1 - (float)lx0);
        g_ui.rect[7] = 0.5f * (ly1 - (float)UI_Y0);
    } else {
        for (int i = 4; i < 8; i++)
            g_ui.rect[i] = -1e6f;
    }
    g_ui.cur = nb;
}

/* Final pass table dwords 64..83: rects, the current buffer's T# (RGBA8 UNORM, linear), point S#.
 */
static void ui_write_table(uint32_t* fin) {
    float* r = (float*)(fin + 64);
    for (int i = 0; i < 8; i++)
        r[i] = g_ui.ok ? g_ui.rect[i] : -1e6f;
    if (g_ui.ok) {
        build_tsharp(fin + 72, g_ui.buf[g_ui.cur], UI_W, UI_H);
        build_ssharp_clamp(fin + 80, 0);
    }
}
