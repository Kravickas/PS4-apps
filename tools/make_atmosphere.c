/* Build-time generator for assets/sky/atmosphere.bin (host C, same model code as the game):
     cc -O2 -fno-math-errno -I../src make_atmosphere.c ../src/atmosphere.c -o make_atmosphere && ./make_atmosphere OUT
   Tables at Bruneton's reference sample counts (transmittance 500, single scattering 50);
   ~2 s on a desktop CPU, ~10 s on the PS4, which is why it is precomputed.
   Layout (little endian): 32-byte header {u32 'ATM1', trans_w, trans_h, sky_w, sky_h, slices,
   f32 elev_min_deg, elev_max_deg}; transmittance f32 [trans_h][trans_w][3]; then three RGBA16F
   images (Rayleigh, Mie, multiple scattering; alpha 0) of sky_w x (slices * sky_h) texels, slice k
   in rows k * sky_h .. k * sky_h + sky_h - 1 (table row j: view height y = +-((2j+1)/sky_h - 1)^2). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atmosphere.h"
static unsigned short to_half(float f) { /* round to nearest even, as the GPU converts */
    unsigned int x; memcpy(&x, &f, 4);
    unsigned int s = (x >> 16) & 0x8000u, e = (x >> 23) & 0xFFu, m = x & 0x7FFFFFu;
    if (e == 0xFF) return (unsigned short)(s | 0x7C00u | (m ? 0x200u : 0u));
    int ee = (int)e - 127 + 15;
    if (ee >= 31) return (unsigned short)(s | 0x7C00u);
    if (ee <= 0) {
        if (ee < -10) return (unsigned short)s;
        m |= 0x800000u; unsigned int sh = (unsigned int)(14 - ee), hm = m >> sh, rem = m & ((1u << sh) - 1u), half = 1u << (sh - 1);
        if (rem > half || (rem == half && (hm & 1u))) hm++;
        return (unsigned short)(s | hm);
    }
    unsigned int hm = m >> 13, rem = m & 0x1FFFu;
    unsigned int h = s | ((unsigned int)ee << 10) | hm;
    if (rem > 0x1000u || (rem == 0x1000u && (hm & 1u))) h++;
    return (unsigned short)h;
}
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: make_atmosphere OUT\n"); return 1; }
    void* mem = malloc(atmo_bytes()); Atmo a; atmo_init(&a, mem, 500, 50);
    FILE* f = fopen(argv[1], "wb"); if (!f) return 2;
    unsigned int h[6] = {0x314D5441u, ATMO_T_W, ATMO_T_H, ATMO_SKY_W, ATMO_SKY_H, ATMO_SLICES};
    float e[2] = {ATMO_SLICE_MIN_DEG, ATMO_SLICE_MAX_DEG};
    fwrite(h, 4, 6, f); fwrite(e, 4, 2, f);
    fwrite(a.trans, 4, ATMO_T_W * ATMO_T_H * 3, f);
    long n = (long)ATMO_SLICES * ATMO_SKY_H * ATMO_SKY_W;
    unsigned short* img = malloc(n * 8);
    for (int t = 0; t < 3; t++) {
        for (long i = 0; i < n; i++) {
            for (int c = 0; c < 3; c++) img[i * 4 + c] = to_half(a.sky[i * 9 + t * 3 + c]);
            img[i * 4 + 3] = 0;
        }
        fwrite(img, 8, n, f);
    }
    fclose(f); return 0;
}
