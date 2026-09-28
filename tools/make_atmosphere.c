/* Build-time generator for assets/sky/atmosphere.bin (host C, same model code as the game).

     cc -O2 -fno-math-errno -I../src make_atmosphere.c ../src/atmosphere.c -lm -o make_atmosphere
     ./make_atmosphere OUT

   Tables at Bruneton's reference sample counts (transmittance 500, single
   scattering 50); ~2 s on a desktop CPU, ~10 s on the PS4, which is why it is precomputed. Layout
   (little endian): 32-byte header {u32 'ATM2', trans_w, trans_h, sky_w, sky_h, slices, f32
   elev_min_deg, elev_max_deg}; transmittance f32 [trans_h][trans_w][3]; then three RGBA16F images
   (Rayleigh, Mie, multiple scattering; alpha 0) of sky_w x (slices * sky_h) texels, slice k in rows
   k * sky_h .. k * sky_h + sky_h - 1 (table row j: view height y = +-((2j+1)/sky_h - 1)^2); then,
   per slice, f32 [slices][6]: the mean sky radiance over the upper hemisphere (uniform in solid
   angle; fog in-scattering) and its cosine-weighted mean (sky light on a horizontal surface / pi),
   per unit light illuminance, with ps_dark's lookup and phases. Magic 'ATM2'. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atmosphere.h"
static unsigned short to_half(float f) { /* round to nearest even, as the GPU converts */
    unsigned int x;
    memcpy(&x, &f, 4);
    unsigned int s = (x >> 16) & 0x8000u, e = (x >> 23) & 0xFFu, m = x & 0x7FFFFFu;
    if (e == 0xFF)
        return (unsigned short)(s | 0x7C00u | (m ? 0x200u : 0u));
    int ee = (int)e - 127 + 15;
    if (ee >= 31)
        return (unsigned short)(s | 0x7C00u);
    if (ee <= 0) {
        if (ee < -10)
            return (unsigned short)s;
        m |= 0x800000u;
        unsigned int sh = (unsigned int)(14 - ee), hm = m >> sh, rem = m & ((1u << sh) - 1u),
                     half = 1u << (sh - 1);
        if (rem > half || (rem == half && (hm & 1u)))
            hm++;
        return (unsigned short)(s | hm);
    }
    unsigned int hm = m >> 13, rem = m & 0x1FFFu;
    unsigned int h = s | ((unsigned int)ee << 10) | hm;
    if (rem > 0x1000u || (rem == 0x1000u && (hm & 1u)))
        h++;
    return (unsigned short)h;
}
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: make_atmosphere OUT\n");
        return 1;
    }
    void* mem = malloc(atmo_bytes());
    Atmo a;
    atmo_init(&a, mem, 500, 50);
    FILE* f = fopen(argv[1], "wb");
    if (!f)
        return 2;
    unsigned int h[6] = {0x324D5441u, ATMO_T_W, ATMO_T_H, ATMO_SKY_W, ATMO_SKY_H, ATMO_SLICES};
    float e[2] = {ATMO_SLICE_MIN_DEG, ATMO_SLICE_MAX_DEG};
    fwrite(h, 4, 6, f);
    fwrite(e, 4, 2, f);
    fwrite(a.trans, 4, ATMO_T_W * ATMO_T_H * 3, f);
    long n = (long)ATMO_SLICES * ATMO_SKY_H * ATMO_SKY_W;
    unsigned short* img = malloc(n * 8);
    for (int t = 0; t < 3; t++) {
        for (long i = 0; i < n; i++) {
            for (int c = 0; c < 3; c++)
                img[i * 4 + c] = to_half(a.sky[i * 9 + t * 3 + c]);
            img[i * 4 + 3] = 0;
        }
        fwrite(img, 8, n, f);
    }
    { /* hemisphere means per slice: midpoint rule on a 128 x 128 (y, phi) grid, y = sin(elevation),
         dOmega = dy dphi; the table lookup of ps_dark (bilinear, clamp) on the float tables */
        const int NY = 128, NP = 128;
        float* means = malloc((size_t)ATMO_SLICES * 6 * 4);
        for (int k = 0; k < ATMO_SLICES; k++) {
            double e = (ATMO_SLICE_MIN_DEG +
                        (ATMO_SLICE_MAX_DEG - ATMO_SLICE_MIN_DEG) * k / (ATMO_SLICES - 1)) *
                       3.14159265358979323846 / 180.0;
            double Ly = sin(e), Lx = cos(e), su[3] = {0, 0, 0}, sc[3] = {0, 0, 0}, wu = 0, wc = 0;
            const float* tab = a.sky + (long)k * ATMO_SKY_H * ATMO_SKY_W * 9;
            for (int iy = 0; iy < NY; iy++) {
                double y = (iy + 0.5) / NY, hz = sqrt(1.0 - y * y);
                double v = 0.5 + 0.5 * sqrt(y), row = v * ATMO_SKY_H;
                for (int ip = 0; ip < NP; ip++) {
                    double ph = 3.14159265358979323846 * (ip + 0.5) / NP; /* 0..pi, symmetric */
                    double cphi = cos(ph), u = 0.5 - 0.5 * cphi, nu = hz * cphi * Lx + y * Ly;
                    double pr = 0.05968310365946075 * (1 + nu * nu), g = 0.8,
                           x = 1 + g * g - 2 * g * nu;
                    double pm = 0.1193662073189215 * (1 - g * g) / (2 + g * g) * (1 + nu * nu) /
                                (x * sqrt(x));
                    double fx = u * ATMO_SKY_W - 0.5, fy = row - 0.5;
                    int x0 = (int)floor(fx), y0 = (int)floor(fy);
                    double tx = fx - x0, ty = fy - y0;
                    int x1 = x0 + 1, y1 = y0 + 1;
                    x0 = x0 < 0 ? 0 : (x0 > ATMO_SKY_W - 1 ? ATMO_SKY_W - 1 : x0);
                    x1 = x1 < 0 ? 0 : (x1 > ATMO_SKY_W - 1 ? ATMO_SKY_W - 1 : x1);
                    y0 = y0 < 0 ? 0 : (y0 > ATMO_SKY_H - 1 ? ATMO_SKY_H - 1 : y0);
                    y1 = y1 < 0 ? 0 : (y1 > ATMO_SKY_H - 1 ? ATMO_SKY_H - 1 : y1);
                    for (int c = 0; c < 3; c++) {
                        double L = 0;
                        for (int t = 0; t < 3; t++) {
                            const int ch = t * 3 + c;
                            double a00 = tab[(y0 * ATMO_SKY_W + x0) * 9 + ch],
                                   a10 = tab[(y0 * ATMO_SKY_W + x1) * 9 + ch];
                            double a01 = tab[(y1 * ATMO_SKY_W + x0) * 9 + ch],
                                   a11 = tab[(y1 * ATMO_SKY_W + x1) * 9 + ch];
                            double val = (a00 * (1 - tx) + a10 * tx) * (1 - ty) +
                                         (a01 * (1 - tx) + a11 * tx) * ty;
                            L += val * (t == 0 ? pr : (t == 1 ? pm : 1.0));
                        }
                        su[c] += L;
                        sc[c] += L * y;
                    }
                    wu += 1.0;
                    wc += y;
                }
            }
            for (int c = 0; c < 3; c++) {
                means[k * 6 + c] = (float)(su[c] / wu);
                means[k * 6 + 3 + c] = (float)(sc[c] / wc);
            }
        }
        fwrite(means, 4, (size_t)ATMO_SLICES * 6, f);
    }
    fclose(f);
    return 0;
}
