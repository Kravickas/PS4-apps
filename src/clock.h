/* Clock mode's text (docs/clock_plan.txt): the glass pieces' signed distance, composed by the CPU
   from the glyph distance atlas (assets/ui/clock_sdf.bin, tools/make_clock_sdf.py) into an RG8
   texture over the slab (CLOCK_W x CLOCK_H): R = 128 + 8 d (d px, negative inside, +-16), G = the
   piece's group (0 none, 1 the hours and minutes, 2 the seconds, 3 the date) - its thickness in
   ps_clock. Recomposed only when the text changes; triple-buffered like the UI. */
#pragma once
#include "clock_sdf.h"

#define CLOCK_W 1152
#define CLOCK_H 648
#define CLOCK_BUFS 3
#define CLOCK_BASE_Y 409         /* the time's baseline in the slab (the screen's CY + 85) */
#define CLOCK_DATE_Y 511         /* the date's (CY + 187) */
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
        clock_layout(P, &n, 52, 0.5f * ((float)CLOCK_W - clock_width(52, dt)), CLOCK_DATE_Y, dt, 3);
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
