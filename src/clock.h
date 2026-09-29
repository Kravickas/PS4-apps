/* Clock mode's text: the glass pieces' signed distance, composed by the CPU
   from the glyph distance atlas (assets/ui/clock_sdf.bin, tools/make_clock_sdf.py) into an RG8
   texture over the slab (CLOCK_W x CLOCK_H): R = 128 + 8 d (d px, negative inside, +-16), G = the
   piece's group (0 none, 1 the hours and minutes, 2 the seconds, 3 the date) - its thickness in
   ps_clock. Recomposed only when the text changes; triple-buffered like the UI. */
#pragma once
#include "clock_sdf.h"

#define CLOCK_W 1152
#define CLOCK_H 648
#define CLOCK_BUFS 3
#define CLOCK_BASE_Y 409 /* the time's baseline in the slab (the screen's CY + 85) */
#define CLOCK_DATE_Y 511 /* the date's (CY + 187) */
/* The clock source's dot before the date, as in the time panel (dot 10 and gap 9 px at a 14 px cap
   height there; the date's cap is 39 px): drawn by ps_clock, dot + gap + date centred together */
#define CLOCK_DOT_R 14.0f
#define CLOCK_DOT_GAP 25.0f
#define CLOCK_SEC_GAP 8          /* between the minutes and the seconds */
#define CLOCK_RC 56.0f           /* the slab's corner radius */
#define CLOCK_LIGHT_LS 170.0f    /* internal light: scattering length (px) */
#define CLOCK_LIGHT_S0 3.0f      /* a beam's width at the rim (px) */
#define CLOCK_LIGHT_SPREAD 0.07f /* its widening per px travelled */

#define CLOCK_MAX_GLYPHS 48
typedef struct {
    const ClockGlyph* g; /* 0: none */
    short x, y;          /* the tile's top left in the texture */
    short penx, base;    /* the glyph's origin: the layout's identity */
    unsigned char gid;
} ClockPlace;
typedef struct {
    const unsigned char* atlas; /* R8, CLOCK_SDF_W x CLOCK_SDF_H */
    unsigned char* buf[CLOCK_BUFS];
    int cur, ok;
    char key[64]; /* the text of the current buffer */
    /* what each buffer holds: its glyphs where they are (partial updates diff against it) */
    ClockPlace held[CLOCK_BUFS][CLOCK_MAX_GLYPHS];
    int nheld[CLOCK_BUFS], valid[CLOCK_BUFS];
    float dot_x, dot_y; /* the dot's centre in the slab (with the current buffer's date) */
} Clock;
static Clock g_clock;

static int clock_init(void) {
    my_memset(&g_clock, 0, sizeof(g_clock));
    int fd = sceKernelOpen(ASSET_DIR "ui/clock_sdf.bin", 0, 0);
    if (fd < 0)
        return -1;
    uint32_t hd[3];
    unsigned long n = (unsigned long)CLOCK_SDF_W * CLOCK_SDF_H;
    if (sceKernelRead(fd, hd, 12) != 12 || hd[0] != 0x31445343u || hd[1] != CLOCK_SDF_W ||
        hd[2] != CLOCK_SDF_H) {
        sceKernelClose(fd);
        return -2;
    }
    unsigned char* p = (unsigned char*)cpu_alloc(n, 0x1000);
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
    g_clock.atlas = p;
    for (int b = 0; b < CLOCK_BUFS; b++) {
        g_clock.buf[b] = (unsigned char*)cpu_alloc((unsigned long)CLOCK_W * CLOCK_H * 2, 0x1000);
        if (!g_clock.buf[b])
            return -4;
    }
    g_clock.ok = 1;
    return 0;
}

static const ClockGlyph* clock_glyph_of(int px, char ch) {
    for (unsigned i = 0; i < sizeof(clock_glyph) / sizeof(clock_glyph[0]); i++)
        if (clock_glyph[i].px == px && clock_glyph[i].ch == ch)
            return &clock_glyph[i];
    return 0;
}
static float clock_width(int px, const char* s) {
    float w = 0.0f;
    for (; *s; s++) {
        const ClockGlyph* g = clock_glyph_of(px, *s);
        w += g ? g->adv : 0.0f;
    }
    return w;
}
/* The glyphs of s at pen x (whole pixels per glyph), baseline y, appended to P (count *n). */
static void clock_layout(ClockPlace* P, int* n, int px, float x, int y, const char* s, int gid) {
    for (; *s; s++) {
        const ClockGlyph* g = clock_glyph_of(px, *s);
        if (!g)
            continue;
        if (g->w > 0 && *n < CLOCK_MAX_GLYPHS) {
            P[*n].g = g;
            P[*n].x = (short)((int)(x + 0.5f) + (int)g->xoff);
            P[*n].y = (short)(y + (int)g->yoff);
            P[*n].penx = (short)(int)(x + 0.5f);
            P[*n].base = (short)y;
            P[*n].gid = (unsigned char)gid;
            (*n)++;
        }
        x += g->adv;
    }
}
/* One placed glyph min-blended into b, only inside the rect c (x0, y0, x1, y1). */
static void clock_blit(unsigned char* b, const ClockPlace* p, const int c[4]) {
    const ClockGlyph* g = p->g;
    int x0 = p->x > c[0] ? p->x : c[0], y0 = p->y > c[1] ? p->y : c[1];
    int x1 = p->x + g->w < c[2] ? p->x + g->w : c[2], y1 = p->y + g->h < c[3] ? p->y + g->h : c[3];
    for (int yy = y0; yy < y1; yy++) {
        const unsigned char* src =
            g_clock.atlas + (unsigned long)(g->y + yy - p->y) * CLOCK_SDF_W + g->x - p->x;
        unsigned char* dst = b + (unsigned long)yy * CLOCK_W * 2;
        for (int xx = x0; xx < x1; xx++)
            if (src[xx] < dst[xx * 2]) {
                dst[xx * 2] = src[xx];
                dst[xx * 2 + 1] = p->gid;
            }
    }
}
/* Rect c cleared (+16 px: nothing near), then every glyph overlapping it re-blended inside it -
   the minimum over the tiles there, the same as composing the whole texture. */
static void clock_redraw(unsigned char* b, const ClockPlace* P, int n, const int c[4]) {
    for (int yy = c[1]; yy < c[3]; yy++) {
        unsigned char* d = b + ((unsigned long)yy * CLOCK_W + c[0]) * 2;
        for (int xx = c[0]; xx < c[2]; xx++, d += 2) {
            d[0] = 255;
            d[1] = 0;
        }
    }
    for (int i = 0; i < n; i++)
        if (P[i].x < c[2] && P[i].x + P[i].g->w > c[0] && P[i].y < c[3] &&
            P[i].y + P[i].g->h > c[1])
            clock_blit(b, &P[i], c);
}

/* Civil date from days since 0001-01-01 (H. Hinnant's days_from_civil, inverted; 0001-01-01 was
   a Monday). */
static void clock_date(long days, int* y, int* m, int* d, int* wd) {
    *wd = (int)(days % 7); /* 0 = Monday */
    long z = days + 306;   /* days since 0000-03-01 */
    long era = z / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* The text for the time (seconds since local midnight) and the date (days since 0001-01-01):
   recomposed into the next buffer only when it changes. Returns the current buffer. */
static const unsigned char* clock_update(double sec, long days, int h12) {
    static const char* const wdn[7] = {"Monday", "Tuesday",  "Wednesday", "Thursday",
                                       "Friday", "Saturday", "Sunday"};
    static const char* const mon[12] = {"January",   "February", "March",    "April",
                                        "May",       "June",     "July",     "August",
                                        "September", "October",  "November", "December"};
    if (!g_clock.ok)
        return 0;
    char hm[8], sc[8], dt[40];
    int s = (int)sec, h = s / 3600, mi = s / 60 % 60, ss = s % 60, k = 0;
    const char* ap = 0;
    if (h12) {
        ap = h < 12 ? "AM" : "PM";
        h = h % 12 == 0 ? 12 : h % 12;
        if (h >= 10)
            hm[k++] = (char)('0' + h / 10);
    } else
        hm[k++] = (char)('0' + h / 10);
    hm[k++] = (char)('0' + h % 10);
    hm[k++] = ':';
    hm[k++] = (char)('0' + mi / 10);
    hm[k++] = (char)('0' + mi % 10);
    hm[k] = 0;
    k = 0;
    sc[k++] = ':';
    sc[k++] = (char)('0' + ss / 10);
    sc[k++] = (char)('0' + ss % 10);
    if (ap) {
        sc[k++] = ' ';
        sc[k++] = ap[0];
        sc[k++] = ap[1];
    }
    sc[k] = 0;
    int y, m, d, wd;
    clock_date(days, &y, &m, &d, &wd);
    k = 0;
    for (const char* p = wdn[wd]; *p; p++)
        dt[k++] = *p;
    dt[k++] = ' ';
    if (d >= 10)
        dt[k++] = (char)('0' + d / 10);
    dt[k++] = (char)('0' + d % 10);
    dt[k++] = ' ';
    for (const char* p = mon[m - 1]; *p; p++)
        dt[k++] = *p;
    dt[k++] = ' ';
    for (int q = 1000; q > 0; q /= 10)
        dt[k++] = (char)('0' + y / q % 10);
    dt[k] = 0;
    char key[64];
    k = 0;
    for (const char* p = hm; *p; p++)
        key[k++] = *p;
    for (const char* p = sc; *p; p++)
        key[k++] = *p;
    for (const char* p = dt; *p && k < 63; p++)
        key[k++] = *p;
    key[k] = 0;
    int same = 1;
    for (int i = 0; key[i] || g_clock.key[i]; i++)
        if (key[i] != g_clock.key[i]) {
            same = 0;
            break;
        }
    if (!same) {
        for (int i = 0; i <= k; i++)
            g_clock.key[i] = key[i];
        int nb = (g_clock.cur + 1) % CLOCK_BUFS;
        unsigned char* b = g_clock.buf[nb];
        ClockPlace P[CLOCK_MAX_GLYPHS];
        int n = 0;
        float wt = clock_width(300, hm), ws = clock_width(110, sc);
        float x0 = 0.5f * ((float)CLOCK_W - (wt + (float)CLOCK_SEC_GAP + ws));
        clock_layout(P, &n, 300, x0, CLOCK_BASE_Y, hm, 1);
        clock_layout(P, &n, 110, x0 + wt + (float)CLOCK_SEC_GAP, CLOCK_BASE_Y, sc, 2);
        float wd = clock_width(52, dt), dl = 2.0f * CLOCK_DOT_R + CLOCK_DOT_GAP;
        float xd = 0.5f * ((float)CLOCK_W - (wd + dl)) + dl;
        clock_layout(P, &n, 52, xd, CLOCK_DATE_Y, dt, 3);
        const ClockGlyph* cap = clock_glyph_of(52, 'H'); /* the dot on the date's cap centre */
        g_clock.dot_x = xd - CLOCK_DOT_GAP - CLOCK_DOT_R;
        g_clock.dot_y = (float)CLOCK_DATE_Y + (cap ? cap->yoff + 0.5f * (float)cap->h : -19.5f);
        /* Only the glyphs that differ from what this buffer holds are redrawn: the digits are
           tabular, so a changed number keeps every glyph's origin; a new layout (the count or an
           origin changed - the date, 9:59 -> 10:00 in 12 h) redraws everything. */
        const ClockPlace* H = g_clock.held[nb];
        int same_layout = g_clock.valid[nb] && g_clock.nheld[nb] == n;
        for (int i = 0; same_layout && i < n; i++)
            same_layout = H[i].penx == P[i].penx && H[i].base == P[i].base && H[i].gid == P[i].gid;
        if (same_layout) {
            for (int i = 0; i < n; i++)
                if (H[i].g != P[i].g) {
                    /* the old glyph's tile and the new one's: both fields are rewritten */
                    int c[4] = {H[i].x < P[i].x ? H[i].x : P[i].x,
                                H[i].y < P[i].y ? H[i].y : P[i].y,
                                H[i].x + H[i].g->w > P[i].x + P[i].g->w ? H[i].x + H[i].g->w
                                                                        : P[i].x + P[i].g->w,
                                H[i].y + H[i].g->h > P[i].y + P[i].g->h ? H[i].y + H[i].g->h
                                                                        : P[i].y + P[i].g->h};
                    c[0] = c[0] < 0 ? 0 : c[0];
                    c[1] = c[1] < 0 ? 0 : c[1];
                    c[2] = c[2] > CLOCK_W ? CLOCK_W : c[2];
                    c[3] = c[3] > CLOCK_H ? CLOCK_H : c[3];
                    clock_redraw(b, P, n, c);
                }
        } else {
            const int all[4] = {0, 0, CLOCK_W, CLOCK_H};
            clock_redraw(b, P, n, all);
        }
        for (int i = 0; i < n; i++)
            g_clock.held[nb][i] = P[i];
        g_clock.nheld[nb] = n;
        g_clock.valid[nb] = 1;
        g_clock.cur = nb;
    }
    return g_clock.buf[g_clock.cur];
}

/* ==== Per frame (clock_frame): the light's direction, the rim entries, ps_clock's constants === */
/* Clock mode's per-frame CPU part: the light's screen direction, the rim
   entries for ps_clock_light (every 3 px of the slab's rim facing the light: refracted in by 2D
   Snell (IOR 1.5), weight cos (1 - F) x 3 / (sqrt(2 pi) x the normaliser)), and ps_clock's
   per-frame constants and text texture. */
#include "clock_light_norm.h"

#define CLOCK_RIM_MAX 1168
static float g_clock_rim[CLOCK_RIM_MAX][4]; /* x, y, outward normal x, y */
static int g_clock_rim_n = 0;

static void clock_rim_init(void) {
    const float cx = 0.5f * DISPLAY_W, cy = 0.5f * DISPLAY_H;
    const float hx = 0.5f * CLOCK_W - CLOCK_RC, hy = 0.5f * CLOCK_H - CLOCK_RC, step = 3.0f;
    static const float corner[4][3] = {{1, -1, -90}, {1, 1, 0}, {-1, 1, 90}, {-1, -1, 180}};
    int n = 0;
    float na_f = 1.57079633f * CLOCK_RC / step;
    int na = (int)na_f;
    na += (float)na < na_f;
    for (int c = 0; c < 4; c++)
        for (int i = 0; i < na && n < CLOCK_RIM_MAX; i++, n++) {
            float a = (corner[c][2] + 90.0f * ((float)i + 0.5f) / (float)na) * 0.0174532925f;
            float ca = my_cos(a), sa = my_sin(a);
            g_clock_rim[n][0] = cx + corner[c][0] * hx + CLOCK_RC * ca;
            g_clock_rim[n][1] = cy + corner[c][1] * hy + CLOCK_RC * sa;
            g_clock_rim[n][2] = ca;
            g_clock_rim[n][3] = sa;
        }
    const float ex[4][6] = {{-hx, -0.5f * CLOCK_H, hx, -0.5f * CLOCK_H, 0, -1},
                            {0.5f * CLOCK_W, -hy, 0.5f * CLOCK_W, hy, 1, 0},
                            {hx, 0.5f * CLOCK_H, -hx, 0.5f * CLOCK_H, 0, 1},
                            {-0.5f * CLOCK_W, hy, -0.5f * CLOCK_W, -hy, -1, 0}};
    for (int e = 0; e < 4; e++) {
        float dx = ex[e][2] - ex[e][0], dy = ex[e][3] - ex[e][1];
        int ne = (int)(my_sqrt(dx * dx + dy * dy) / step);
        for (int i = 0; i < ne && n < CLOCK_RIM_MAX; i++, n++) {
            float t = ((float)i + 0.5f) / (float)ne;
            g_clock_rim[n][0] = cx + ex[e][0] + dx * t;
            g_clock_rim[n][1] = cy + ex[e][1] + dy * t;
            g_clock_rim[n][2] = ex[e][4];
            g_clock_rim[n][3] = ex[e][5];
        }
    }
    g_clock_rim_n = n;
}

/* atan2 in degrees, 0 .. 360 (a minimax polynomial for atan on [0, 1], error < 1e-5 rad). */
static float clock_atan2_deg(float y, float x) {
    float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, mn = ax < ay ? ax : ay, mx = ax < ay ? ay : ax;
    if (mx <= 0.0f)
        return 0.0f;
    float a = mn / mx, s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0)
        r = 3.14159274f - r;
    if (y < 0)
        r = -r;
    float d = r * 57.2957795f;
    return d < 0.0f ? d + 360.0f : d;
}

/* The lit rim entries for direction (l2x, l2y) (the way the light travels on the screen). */
static int clock_entries(float* out, float l2x, float l2y) {
    float x = clock_atan2_deg(l2y, l2x) * 0.5f;
    int i0 = (int)x;
    float f = x - (float)i0;
    i0 %= 180;
    float norm = clock_light_norm[i0] * (1.0f - f) + clock_light_norm[(i0 + 1) % 180] * f;
    const float eta = 1.0f / 1.5f, wk = 3.0f / (2.50662827f * norm);
    int n = 0;
    for (int i = 0; i < g_clock_rim_n; i++) {
        const float* r = g_clock_rim[i];
        float c = -(r[2] * l2x + r[3] * l2y);
        if (c <= 0.0f)
            continue;
        float k = 1.0f - eta * eta * (1.0f - c * c), g = eta * c - my_sqrt(k);
        float tx = eta * l2x + g * r[2], ty = eta * l2y + g * r[3], tl = my_sqrt(tx * tx + ty * ty);
        float om = 1.0f - c, fr = 0.04f + 0.96f * om * om * om * om * om;
        float* o = out + n * 8;
        o[0] = r[0];
        o[1] = r[1];
        o[2] = tx / tl;
        o[3] = ty / tl;
        o[4] = c * (1.0f - fr) * wk;
        o[5] = o[6] = o[7] = 0.0f;
        n++;
    }
    if (n == 0) { /* the pass always reads one entry: a weightless one */
        for (int j = 0; j < 8; j++)
            out[j] = 0.0f;
        n = 1;
    }
    return n;
}

/* This frame's clock: L the scene's light (unit, world: the sun by day, the moon by night), lc the
   scene's light colour (desc[32]), tod the clock's seconds since midnight, days its local date. */
static void clock_frame(float yaw, float pitch, const float L[3], int night, const float lc[3],
                        double tod, long days, int dot) {
    float F[3], R[3], U[3];
    cam_basis(yaw, pitch, F, R, U);
    float fz = L[0] * F[0] + L[1] * F[1] + L[2] * F[2];
    float lr = L[0] * R[0] + L[1] * R[1] + L[2] * R[2],
          lu = L[0] * U[0] + L[1] * U[1] + L[2] * U[2];
    float lx, ly;
    if (fz > 0.01f) { /* on the screen side: from its screen position toward the slab's centre */
        float cot = my_cos(0.3054f) / my_sin(0.3054f);
        lx = -540.0f * cot * lr / fz;
        ly = 540.0f * cot * lu / fz;
    } else { /* behind the camera: its projected direction */
        lx = -lr;
        ly = lu;
    }
    float ln = my_sqrt(lx * lx + ly * ly);
    if (ln < 1e-3f) {
        lx = 0.0f;
        ly = 1.0f;
    } else {
        lx /= ln;
        ly /= ln;
    }
    /* the light field depends only on (lx, ly): recomputed when it moved more than 0.02 degrees
       (|d l2|^2 > (3.5e-4)^2), else the kept one serves */
    float dx = lx - g_clock_light_l2[0], dy = ly - g_clock_light_l2[1];
    g_clock_light_dirty = !g_clock_light_valid || dx * dx + dy * dy > 1.2e-7f;
    if (g_clock_light_dirty) {
        float* e = g_clock_entries + (long)g_clock_entry_buf * CLOCK_RIM_MAX * 8;
        int n = clock_entries(e, lx, ly);
        uint32_t* lt = g_post_tab + CLOCK_LIGHT_BLOCK * 32;
        uint64_t a = (uint64_t)(uintptr_t)e;
        lt[0] = (uint32_t)a;
        lt[1] = (uint32_t)(a >> 32);
        lt[2] = (uint32_t)n * 32u;
        lt[3] = 0;
        g_clock_entry_buf = (g_clock_entry_buf + 1) % 3;
        g_clock_light_l2[0] = lx;
        g_clock_light_l2[1] = ly;
        g_clock_light_valid = 1;
    }
    uint32_t* k = g_post_tab + CLOCK_BLOCK * 32;
    float* kc = (float*)(k + 40);
    float m = lc[0] > lc[1] ? lc[0] : lc[1];
    m = m > lc[2] ? m : lc[2];
    m = m > 1e-9f ? m : 1.0f;
    kc[7] = night ? 0.85f : 1.35f; /* strength */
    kc[8] = lc[0] / m;
    kc[9] = lc[1] / m;
    kc[10] = lc[2] / m;
    kc[11] = night ? 0.14f : 0.18f; /* fill */
    kc[12] = lx;
    kc[13] = ly;
    const unsigned char* tex = clock_update(tod, days, g_ts.h12);
    if (tex)
        build_tsharp_rg8(k + 28, tex, CLOCK_W, CLOCK_H, CLOCK_W);
    /* the clock source's dot (ps_clock [58..63]): centre on screen, radius (off: -100), linear
       colour */
    kc[18] = 0.5f * (float)(DISPLAY_W - CLOCK_W) + g_clock.dot_x;
    kc[19] = 0.5f * (float)(DISPLAY_H - CLOCK_H) + g_clock.dot_y;
    kc[20] = dot > 0 && dot < 4 ? CLOCK_DOT_R : -100.0f;
    for (int c = 0; c < 3; c++)
        kc[21 + c] = srgb_to_linear((float)k_ts_dot_srgb[dot > 0 && dot < 4 ? dot : 0][c] / 255.0f);
}
