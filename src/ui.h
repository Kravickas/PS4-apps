/* On-screen panel, top left: always the OPTIONS hint, camera speed and day speed, plus the
   controls list when OPTIONS is pressed. The CPU lays out text (URW Gothic Demi) and DS4 icons from
   assets/ui/ui_atlas.bin (tools/make_ui_atlas.py, src/ui_atlas.h) into a 1920x1080 RGBA8 texture
   (premultiplied, sRGB-space values), only when the content changes, triple-buffered so frames in
   flight keep reading theirs. ps_ui blurs the scene under the panel's rounded rect (frosted
   glass) and composites the texture after its sRGB encode. */
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
/* The time panel, top centre: UI_Y0 from the top like the left panel, UI_PILL_H tall; the dot (time
   source: grey in-game, yellow console, green internet) and the time. */
#define UI_PILL_H 44
#define UI_PILL_PADL 16
#define UI_PILL_PADR 18
#define UI_PILL_DOT 10
#define UI_PILL_GAP 9
#define UI_LABEL_DX (2 * (UI_ICON_PX + 2) + 10)
/* Icons are centred on the text's cap height; lines reserve room for both. */
#define UI_ICON_ABOVE (UI_FONT_CAP / 2 + UI_ICON_PX / 2)
#define UI_ICON_BELOW (UI_ICON_PX - UI_ICON_ABOVE)
#define UI_LINE_ABOVE (UI_ICON_ABOVE > UI_FONT_ASCENT ? UI_ICON_ABOVE : UI_FONT_ASCENT)
#define UI_LINE_BELOW (UI_ICON_BELOW > UI_FONT_DESCENT ? UI_ICON_BELOW : UI_FONT_DESCENT)

typedef struct {
    const unsigned char* atlas; /* RGBA8, UI_ATLAS_W x UI_ATLAS_H, straight alpha */
    unsigned char* buf[UI_BUFS];
    int cur, ok;
    int controls;
    char key[96];  /* content of the current buffer; recompose when it changes */
    float rect[4]; /* the panel: centre x, y, half w, h (px) */
    float pill[4]; /* the time panel (hidden: half sizes -1e6) */
    /* Per buffer: the dirty box x0, y0, x1, y1 (empty: x1 <= x0) and the state it shows
       (valid = 0: never drawn). */
    int dirty[UI_BUFS][4];
    struct {
        int valid, controls, cam, day, frozen;
    } st[UI_BUFS];
    int* drawing;   /* dirty box being grown by ui_blit, or 0 */
    int nclip;      /* > 0: ui_blit draws only inside clip[0 .. nclip - 1] (partial redraw) */
    int clip[2][4]; /* x0, y0, x1, y1 */
    int ycam, yday; /* baselines of the value lines (fixed by the layout) */
    int gtop, gbot; /* glyph box extent around a baseline, shadow included */
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
    for (int i = 0; i < 4; i++)
        g_ui.rect[i] = g_ui.pill[i] = -1e6f;
    if (ui_load_atlas(ASSET_DIR "ui/ui_atlas.bin") != 0)
        return -1;
    for (int b = 0; b < UI_BUFS; b++) {
        g_ui.buf[b] = (unsigned char*)cpu_alloc((unsigned long)UI_W * UI_H * 4,
                                                0x1000); /* ONION: blended by the CPU */
        if (!g_ui.buf[b])
            return -2;
        my_memset(g_ui.buf[b], 0, (unsigned long)UI_W * UI_H * 4);
    }
    /* band of a text line around its baseline: every glyph box, plus the 2 px shadow offset */
    g_ui.gtop = 1 << 20, g_ui.gbot = -(1 << 20);
    for (int i = 0; i < 96; i++) {
        int t = ui_glyph[i].yoff, e = ui_glyph[i].yoff + ui_glyph[i].h + 2;
        g_ui.gtop = t < g_ui.gtop ? t : g_ui.gtop;
        g_ui.gbot = e > g_ui.gbot ? e : g_ui.gbot;
    }
    g_ui.ok = 1;
    return 0;
}

/* Premultiplied "over": dst = src * a + dst * (1 - a); tint replaces the atlas colour. */
static void ui_blend(unsigned char* dst, int sx, int sy, int dx, int dy, int x0, int y0, int x1,
                     int y1, const unsigned char* tint, int alpha256) {
    if (x1 <= x0 || y1 <= y0)
        return;
    if (g_ui.drawing) { /* grow the dirty box by the drawn rect */
        int* r = g_ui.drawing;
        if (r[2] <= r[0]) {
            r[0] = x0, r[1] = y0, r[2] = x1, r[3] = y1;
        } else {
            r[0] = x0 < r[0] ? x0 : r[0], r[1] = y0 < r[1] ? y0 : r[1];
            r[2] = x1 > r[2] ? x1 : r[2], r[3] = y1 > r[3] ? y1 : r[3];
        }
    }
    for (int y = y0; y < y1; y++) {
        const unsigned char* s =
            g_ui.atlas + ((unsigned long)(sy + y - dy) * UI_ATLAS_W + sx + (x0 - dx)) * 4;
        unsigned char* d = dst + ((unsigned long)y * UI_W + x0) * 4;
        for (int x = x0; x < x1; x++, s += 4, d += 4) {
            int a = (s[3] * alpha256) >> 8;
            if (a == 0)
                continue;
            const unsigned char* c = tint ? tint : s;
            /* (x + 127) / 255 without a division: t = x + 128, (t + (t >> 8)) >> 8 - equal for
               every x in 0 .. 255 * 255 (checked exhaustively) */
            for (int k = 0; k < 3; k++) {
                int t = c[k] * a + d[k] * (255 - a) + 128;
                d[k] = (unsigned char)((t + (t >> 8)) >> 8);
            }
            int t = d[3] * (255 - a) + 128;
            d[3] = (unsigned char)(a + ((t + (t >> 8)) >> 8));
        }
    }
}

/* Atlas rect (sx, sy, w, h) blended at (dx, dy), clipped to the buffer and, during a partial
   redraw, to each clip rect (the clip rects never overlap). */
static void ui_blit(unsigned char* dst, int sx, int sy, int w, int h, int dx, int dy,
                    const unsigned char* tint, int alpha256) {
    int x0 = dx < 0 ? 0 : dx, y0 = dy < 0 ? 0 : dy;
    int x1 = dx + w > UI_W ? UI_W : dx + w, y1 = dy + h > UI_H ? UI_H : dy + h;
    if (!g_ui.nclip) {
        ui_blend(dst, sx, sy, dx, dy, x0, y0, x1, y1, tint, alpha256);
        return;
    }
    for (int i = 0; i < g_ui.nclip; i++) {
        const int* c = g_ui.clip[i];
        ui_blend(dst, sx, sy, dx, dy, x0 > c[0] ? x0 : c[0], y0 > c[1] ? y0 : c[1],
                 x1 < c[2] ? x1 : c[2], y1 < c[3] ? y1 : c[3], tint, alpha256);
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
    for (int y = y0; y < y0 + h && y < UI_H; y++) {
        uint32_t* p = (uint32_t*)(b + ((unsigned long)y * UI_W + x0) * 4); /* 4-byte aligned */
        for (int x = 0; x < w; x++)
            p[x] = 0;
    }
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
    g_ui.ycam = y;
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
    g_ui.yday = y;
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
            {{UI_ICON_SQUARE, -1}, "Freeze / unfreeze day and night"},
            {{UI_ICON_TRIANGLE, -1}, "Reset camera"},
            {{UI_ICON_L1, UI_ICON_R1}, "Day and night slower / faster"},
            {{UI_ICON_L2, UI_ICON_R2}, "Camera down / up"},
            {{UI_ICON_DPAD_UP, UI_ICON_DPAD_DOWN}, "Camera speed up / down"},
            {{UI_ICON_DPAD_LEFT, UI_ICON_DPAD_RIGHT}, "Move the sun (time of day)"},
            {{UI_ICON_L_2D, -1}, "Move"},
            {{UI_ICON_R_2D, -1}, "Look"},
            {{UI_ICON_L_2D, -1}, "Press: clock source (in-game / console / internet)"},
            {{UI_ICON_R_2D, -1}, "Press: the sun follows the clock"},
            {{UI_ICON_CIRCLE, -1}, "Clock"},
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

/* Lays out the panel into the next buffer when the content key changes; sets the rect. */
/* An antialiased disc (the time panel's dot), straight colour, premultiplied "over". */
static void ui_disc(unsigned char* dst, float cx, float cy, float r, const unsigned char col[3],
                    int alpha256) {
    int x0 = (int)(cx - r) - 1, x1 = (int)(cx + r) + 2, y0 = (int)(cy - r) - 1,
        y1 = (int)(cy + r) + 2;
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < UI_H; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < UI_W; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float c = 0.5f - (my_sqrt(dx * dx + dy * dy) - r);
            c = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c;
            int a = ((int)(c * 255.0f + 0.5f) * alpha256) >> 8;
            if (a == 0)
                continue;
            unsigned char* d = dst + ((unsigned long)y * UI_W + x) * 4;
            for (int k = 0; k < 3; k++) {
                int t = col[k] * a + d[k] * (255 - a) + 128;
                d[k] = (unsigned char)((t + (t >> 8)) >> 8);
            }
            int t = d[3] * (255 - a) + 128;
            d[3] = (unsigned char)(a + ((t + (t >> 8)) >> 8));
        }
}
/* The time panel's width for this text: every digit as the widest one, A / P as the wider - the
   same for every time of the format, so the glass never changes size. */
static int ui_pill_width(const char* s) {
    int dw = 0, apw = 0;
    for (char c = '0'; c <= '9'; c++) {
        int a = ui_glyph[ui_glyph_index(c)].adv;
        dw = a > dw ? a : dw;
    }
    apw = ui_glyph[ui_glyph_index('A')].adv > ui_glyph[ui_glyph_index('P')].adv
              ? ui_glyph[ui_glyph_index('A')].adv
              : ui_glyph[ui_glyph_index('P')].adv;
    int w = 0;
    for (; *s; s++)
        w += (*s >= '0' && *s <= '9')   ? dw
             : (*s == 'A' || *s == 'P') ? apw
                                        : ui_glyph[ui_glyph_index(*s)].adv;
    return UI_PILL_PADL + UI_PILL_DOT + UI_PILL_GAP + w + UI_PILL_PADR;
}

/* dot: 0 = no time panel, 1 grey (in-game), 2 yellow (console), 3 green (internet) */
static void ui_update(int cam_pct, int day_mult, int day_frozen, const char* time_txt, int dot) {
    if (!g_ui.ok)
        return;
    char key[96], *k = key;
    k += ui_fmt_int(k, g_ui.controls);
    *k++ = ',';
    k += ui_fmt_int(k, cam_pct);
    *k++ = ',';
    k += ui_fmt_int(k, day_mult);
    *k++ = ',';
    k += ui_fmt_int(k, day_frozen);
    *k++ = ',';
    k += ui_fmt_int(k, dot);
    *k++ = ',';
    for (const char* t = time_txt; *t && k < key + sizeof(key) - 1; t++)
        *k++ = *t;
    *k = 0;
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

    int nb = (g_ui.cur + 1) % UI_BUFS;
    unsigned char* b = g_ui.buf[nb];
    int* dp = g_ui.dirty[nb];

    /* Partial redraw when this buffer shows the same layout and only values differ: clear the
       bands of the changed value lines and redraw everything clipped to them - identical to a
       full redraw (same draw order inside the bands, nothing differs outside). */
    int partial = g_ui.st[nb].valid && g_ui.st[nb].controls == g_ui.controls &&
                  g_ui.st[nb].frozen == day_frozen;
    g_ui.nclip = 0;
    if (partial) {
        int ys[2], m = 0;
        if (g_ui.st[nb].cam != cam_pct)
            ys[m++] = g_ui.ycam;
        if (g_ui.st[nb].day != day_mult)
            ys[m++] = g_ui.yday;
        for (int i = 0; i < m; i++) { /* bands in y order (insertion sort), overlaps merged */
            int y0 = ys[i] + g_ui.gtop, y1 = ys[i] + g_ui.gbot;
            y0 = y0 < 0 ? 0 : y0;
            y1 = y1 > UI_H ? UI_H : y1;
            int j = g_ui.nclip;
            while (j > 0 && g_ui.clip[j - 1][1] > y0) {
                for (int k = 0; k < 4; k++)
                    g_ui.clip[j][k] = g_ui.clip[j - 1][k];
                j--;
            }
            g_ui.clip[j][0] = 0, g_ui.clip[j][1] = y0, g_ui.clip[j][2] = UI_W, g_ui.clip[j][3] = y1;
            g_ui.nclip++;
        }
        int n = 0;
        for (int i = 0; i < g_ui.nclip; i++) {
            if (n > 0 && g_ui.clip[i][1] <= g_ui.clip[n - 1][3]) {
                if (g_ui.clip[i][3] > g_ui.clip[n - 1][3])
                    g_ui.clip[n - 1][3] = g_ui.clip[i][3];
            } else {
                for (int k = 0; k < 4; k++)
                    g_ui.clip[n][k] = g_ui.clip[i][k];
                n++;
            }
        }
        g_ui.nclip = n;
        for (int i = 0; i < n; i++)
            ui_clear(b, 0, g_ui.clip[i][1], UI_W, g_ui.clip[i][3] - g_ui.clip[i][1]);
    } else { /* full redraw: clear only what was drawn into this buffer last time */
        if (dp[2] > dp[0])
            ui_clear(b, dp[0], dp[1], dp[2] - dp[0], dp[3] - dp[1]);
        dp[0] = dp[1] = dp[2] = dp[3] = 0;
    }
    unsigned char* db = (partial && g_ui.nclip == 0) ? 0 : b; /* 0: already current, measure */

    /* the panel's rect = the bounding box of what was laid out */
    g_ui.drawing = dp;
    int bottom = 0,
        right = ui_left_panel(db, g_ui.controls, cam_pct, day_mult, day_frozen, &bottom);
    float x0 = (float)UI_X0, y0 = (float)UI_Y0, x1 = (float)right, y1 = (float)bottom;
    g_ui.rect[0] = 0.5f * (x0 + x1);
    g_ui.rect[1] = 0.5f * (y0 + y1);
    g_ui.rect[2] = 0.5f * (x1 - x0);
    g_ui.rect[3] = 0.5f * (y1 - y0);

    g_ui.drawing = 0;
    g_ui.nclip = 0;
    /* the time panel: its box cleared and redrawn in every new buffer (a dozen glyphs) */
    {
        static const unsigned char dots[4][3] = {
            {0, 0, 0}, {150, 150, 158}, {240, 205, 70}, {110, 215, 130}};
        static const unsigned char white[3] = {255, 255, 255}, black[3] = {0, 0, 0};
        static int px0 = 0, pw = 0;
        if (pw > 0)
            ui_clear(b, px0 - 2, UI_Y0 - 2, pw + 8, UI_PILL_H + 8);
        if (dot > 0) {
            pw = ui_pill_width(time_txt);
            px0 = UI_W / 2 - pw / 2;
            float cy = (float)UI_Y0 + 0.5f * UI_PILL_H, r = 0.5f * UI_PILL_DOT;
            float cx = (float)(px0 + UI_PILL_PADL) + r;
            ui_disc(b, cx + 1.0f, cy + 1.0f, r, black, 115);
            ui_disc(b, cx, cy, r, dots[dot], 256);
            ui_text(b, px0 + UI_PILL_PADL + UI_PILL_DOT + UI_PILL_GAP,
                    UI_Y0 + UI_PILL_H / 2 + UI_FONT_CAP / 2, time_txt, white);
            g_ui.pill[0] = (float)px0 + 0.5f * (float)pw;
            g_ui.pill[1] = (float)UI_Y0 + 0.5f * UI_PILL_H;
            g_ui.pill[2] = 0.5f * (float)pw;
            g_ui.pill[3] = 0.5f * UI_PILL_H;
        } else
            g_ui.pill[0] = g_ui.pill[1] = g_ui.pill[2] = g_ui.pill[3] = -1e6f;
    }
    g_ui.st[nb].valid = 1;
    g_ui.st[nb].controls = g_ui.controls;
    g_ui.st[nb].cam = cam_pct;
    g_ui.st[nb].day = day_mult;
    g_ui.st[nb].frozen = day_frozen;
    g_ui.cur = nb;
}

/* UI pass table (ps_ui): dwords 28..31 the panel's rect, 32..35 the time panel's, 36..47 the
   current buffer's T# (RGBA8 UNORM, linear) and a point S#. */
static void ui_write_table(uint32_t* blk) {
    float* r = (float*)(blk + 28);
    for (int i = 0; i < 4; i++) {
        r[i] = g_ui.ok ? g_ui.rect[i] : -1e6f;
        r[4 + i] = g_ui.ok ? g_ui.pill[i] : -1e6f;
    }
    if (g_ui.ok) {
        build_tsharp(blk + 36, g_ui.buf[g_ui.cur], UI_W, UI_H);
        build_ssharp_clamp(blk + 44, 0);
    }
}
