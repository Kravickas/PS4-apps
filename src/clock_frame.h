/* Clock mode's per-frame CPU part (docs/clock_plan.txt): the light's screen direction, the rim
   entries for ps_clock_light (every 3 px of the slab's rim facing the light: refracted in by 2D
   Snell (IOR 1.5), weight cos (1 - F) x 3 / (sqrt(2 pi) x the normaliser)), and ps_clock's
   per-frame constants and text texture. The Python original: docs/clock_model/light_entries.py. */
#pragma once
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
                        double tod, long days) {
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
    float* e = g_clock_entries + (long)g_clock_entry_buf * CLOCK_RIM_MAX * 8;
    int n = clock_entries(e, lx, ly);
    uint32_t* lt = g_post_tab + CLOCK_LIGHT_BLOCK * 32;
    uint64_t a = (uint64_t)(uintptr_t)e;
    lt[0] = (uint32_t)a;
    lt[1] = (uint32_t)(a >> 32);
    lt[2] = (uint32_t)n * 32u;
    lt[3] = 0;
    g_clock_entry_buf = (g_clock_entry_buf + 1) % 3;
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
}
