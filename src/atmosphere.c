/* Physically based sky - see atmosphere.h. Transmittance and single scattering follow Eric
   Bruneton's "Precomputed Atmospheric Scattering" (2017 reference implementation, BSD licence,
   functions.glsl): same density profiles, transmittance table parameterisation, trapezoidal
   integrations and sun terminator. Multiple scattering: Sebastien Hillaire, "A Scalable and
   Production Ready Sky and Atmosphere Rendering Technique" (2020), Psi_ms = L_2nd / (1 - f_ms)
   over 64 uniform directions with an isotropic phase. Geometry in double: at 2 m above a
   6360 km radius, r^2 - R^2 is 2.5e7 against a float precision of 4e6 on r^2.
   Freestanding: exp and sqrt are local (no libc); compiled -O2 -fno-math-errno. */
#include "atmosphere.h"

/* Earth at 680 / 550 / 440 nm (Bruneton's demo.cc): Rayleigh 1.24062e-6 lambda^-4 (lambda in
   um), scale height 8 km; Mie extinction 5.328e-3 / 1200 m (Angstrom alpha 0), single scattering
   albedo 0.9, scale height 1.2 km, g 0.8; ozone 300 DU with the cross sections of demo.cc's
   table, density rising 10..25 km and falling 25..40 km; ground albedo 0.1; sun radius
   0.00935 / 2 rad. */
static const double kRay[3] = {1.24062e-6 / (0.68 * 0.68 * 0.68 * 0.68),
                               1.24062e-6 / (0.55 * 0.55 * 0.55 * 0.55),
                               1.24062e-6 / (0.44 * 0.44 * 0.44 * 0.44)};
#define MIE_EXT (5.328e-3 / 1200.0)
#define MIE_SCA (0.9 * MIE_EXT)
/* ozone: kMaxOzoneNumberDensity x kOzoneCrossSection at 680 / 550 / 440 nm (filled in by
   atmo_init from the table below) */
static double kOzone[3];
#define DOBSON_UNIT 2.687e20
#define MAX_OZONE_DENSITY (300.0 * DOBSON_UNIT / 15000.0)
/* demo.cc kOzoneCrossSection[48], 360..830 nm in 10 nm steps (m^2) */
static const double kOzoneXs[48] = {
    1.18e-27,  2.182e-28, 2.818e-28, 6.636e-28, 1.527e-27, 2.763e-27, 5.52e-27,  8.451e-27,
    1.582e-26, 2.316e-26, 3.669e-26, 4.924e-26, 7.752e-26, 9.016e-26, 1.48e-25,  1.602e-25,
    2.139e-25, 2.755e-25, 3.091e-25, 3.5e-25,   4.266e-25, 4.672e-25, 4.398e-25, 4.701e-25,
    5.019e-25, 4.305e-25, 3.74e-25,  3.215e-25, 2.662e-25, 2.238e-25, 1.852e-25, 1.473e-25,
    1.209e-25, 9.423e-26, 7.455e-26, 6.566e-26, 5.105e-26, 4.15e-26,  4.228e-26, 3.237e-26,
    2.451e-26, 2.801e-26, 2.534e-26, 1.624e-26, 1.465e-26, 2.078e-26, 1.383e-26, 7.105e-27};
#define SUN_ANGULAR_RADIUS (0.00935 / 2.0)
#define GROUND_ALBEDO 0.1
#define RB ((double)ATMO_BOTTOM)
#define RT ((double)ATMO_TOP)
#define PI 3.14159265358979323846

static double a_sqrt(double x) {
    return __builtin_sqrt(x > 0.0 ? x : 0.0);
}
/* e^x = 2^k e^r, |r| <= ln2 / 2, e^r by Taylor to r^13 (error < 1e-15) */
static double a_exp(double x) {
    if (x < -700.0)
        return 0.0;
    if (x > 700.0)
        x = 700.0;
    double kf = x * 1.4426950408889634;
    long k = (long)(kf < 0.0 ? kf - 0.5 : kf + 0.5);
    double r = x - (double)k * 0.6931471805599453;
    double t = 1.0, s = 1.0;
    for (int i = 1; i <= 13; i++) {
        t *= r / (double)i;
        s += t;
    }
    union {
        double d;
        unsigned long u;
    } p;
    p.u = (unsigned long)(k + 1023) << 52;
    return s * p.d;
}
static double clampd(double x, double a, double b) {
    return x < a ? a : (x > b ? b : x);
}
/* sin / cos: reduced to [-pi, pi], Taylor to x^21 / x^20 (error ~1e-11 there, measured in the
 * tests) */
static double a_sin(double x) {
    double k = x * (1.0 / (2.0 * 3.14159265358979323846));
    long n = (long)(k < 0.0 ? k - 0.5 : k + 0.5);
    x -= (double)n * (2.0 * 3.14159265358979323846);
    double t = x, s = x, x2 = x * x;
    for (int i = 1; i <= 10; i++) {
        t *= -x2 / (double)((2 * i) * (2 * i + 1));
        s += t;
    }
    return s;
}
static double a_cos(double x) {
    return a_sin(x + 1.57079632679489661923);
}

/* ---- Bruneton functions.glsl ---- */
static double dist_top(double r, double mu) {
    double d = -r * mu + a_sqrt(r * r * (mu * mu - 1.0) + RT * RT);
    return d > 0.0 ? d : 0.0;
}
static double dist_bottom(double r, double mu) {
    double d = -r * mu - a_sqrt(r * r * (mu * mu - 1.0) + RB * RB);
    return d > 0.0 ? d : 0.0;
}
static int hits_ground(double r, double mu) {
    return mu < 0.0 && r * r * (mu * mu - 1.0) + RB * RB >= 0.0;
}
static double dens_ray(double h) {
    return clampd(a_exp(-h / 8000.0), 0.0, 1.0);
}
static double dens_mie(double h) {
    return clampd(a_exp(-h / 1200.0), 0.0, 1.0);
}
static double dens_ozone(double h) {
    return h < 25000.0 ? clampd(h / 15000.0 - 2.0 / 3.0, 0.0, 1.0)
                       : clampd(-h / 15000.0 + 8.0 / 3.0, 0.0, 1.0);
}
static void trans_direct(double r, double mu, int samples, double out[3]) {
    double dx = dist_top(r, mu) / (double)samples, lr = 0.0, lm = 0.0, lo = 0.0;
    for (int i = 0; i <= samples; i++) {
        double d = (double)i * dx, ri = a_sqrt(d * d + 2.0 * r * mu * d + r * r), h = ri - RB;
        double w = (i == 0 || i == samples) ? 0.5 : 1.0;
        lr += dens_ray(h) * w * dx;
        lm += dens_mie(h) * w * dx;
        lo += dens_ozone(h) * w * dx;
    }
    for (int c = 0; c < 3; c++)
        out[c] = a_exp(-(kRay[c] * lr + MIE_EXT * lm + kOzone[c] * lo));
}
static double tc_from_unit(double x, int n) {
    return 0.5 / n + x * (1.0 - 1.0 / n);
}
static double unit_from_tc(double u, int n) {
    return (u - 0.5 / n) / (1.0 - 1.0 / n);
}
static void trans_top(const Atmo* a, double r, double mu, double out[3]) {
    double H = a_sqrt(RT * RT - RB * RB), rho = a_sqrt(r * r - RB * RB);
    double d = dist_top(r, mu), dmin = RT - r, dmax = rho + H;
    double u = tc_from_unit((d - dmin) / (dmax - dmin), ATMO_T_W);
    double v = tc_from_unit(rho / H, ATMO_T_H);
    /* bilinear, texel centres at (i + 0.5) / n, clamped (GL CLAMP_TO_EDGE) */
    double x = u * ATMO_T_W - 0.5, y = v * ATMO_T_H - 0.5;
    int x0 = (int)(x < 0.0 ? -1.0 : x), y0 = (int)(y < 0.0 ? -1.0 : y);
    double fx = x - x0, fy = y - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = x0 < 0 ? 0 : (x0 > ATMO_T_W - 1 ? ATMO_T_W - 1 : x0);
    x1 = x1 < 0 ? 0 : (x1 > ATMO_T_W - 1 ? ATMO_T_W - 1 : x1);
    y0 = y0 < 0 ? 0 : (y0 > ATMO_T_H - 1 ? ATMO_T_H - 1 : y0);
    y1 = y1 < 0 ? 0 : (y1 > ATMO_T_H - 1 ? ATMO_T_H - 1 : y1);
    for (int c = 0; c < 3; c++) {
        double t00 = a->trans[(y0 * ATMO_T_W + x0) * 3 + c],
               t10 = a->trans[(y0 * ATMO_T_W + x1) * 3 + c];
        double t01 = a->trans[(y1 * ATMO_T_W + x0) * 3 + c],
               t11 = a->trans[(y1 * ATMO_T_W + x1) * 3 + c];
        out[c] = (t00 * (1 - fx) + t10 * fx) * (1 - fy) + (t01 * (1 - fx) + t11 * fx) * fy;
    }
}
static void trans_between(const Atmo* a, double r, double mu, double d, int ground, double out[3]) {
    double rd = clampd(a_sqrt(d * d + 2.0 * r * mu * d + r * r), RB, RT);
    double mud = clampd((r * mu + d) / rd, -1.0, 1.0), p[3], q[3];
    if (ground) {
        trans_top(a, rd, -mud, p);
        trans_top(a, r, -mu, q);
    } else {
        trans_top(a, r, mu, p);
        trans_top(a, rd, mud, q);
    }
    for (int c = 0; c < 3; c++) {
        double t = p[c] / q[c];
        out[c] = t < 1.0 ? t : 1.0;
    }
}
static double smoothstepd(double e0, double e1, double x) {
    double t = clampd((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
static void trans_sun(const Atmo* a, double r, double mu_s, double out[3]) {
    double sh = RB / r, ch = -a_sqrt(1.0 - sh * sh);
    double v = smoothstepd(-sh * SUN_ANGULAR_RADIUS, sh * SUN_ANGULAR_RADIUS, mu_s - ch);
    trans_top(a, r, mu_s, out);
    for (int c = 0; c < 3; c++)
        out[c] *= v;
}

/* Hillaire's multiple scattering table: Psi_ms at (altitude, sun cos-zenith). */
static void ms_lookup(const Atmo* a, double h, double mu_s, double out[3]) {
    double x = clampd((mu_s * 0.5 + 0.5), 0.0, 1.0) * (ATMO_MS_N - 1);
    double y = clampd(h / (RT - RB), 0.0, 1.0) * (ATMO_MS_N - 1);
    int x0 = (int)x, y0 = (int)y, x1 = x0 < ATMO_MS_N - 1 ? x0 + 1 : x0,
        y1 = y0 < ATMO_MS_N - 1 ? y0 + 1 : y0;
    double fx = x - x0, fy = y - y0;
    for (int c = 0; c < 3; c++) {
        double t00 = a->ms[(y0 * ATMO_MS_N + x0) * 3 + c],
               t10 = a->ms[(y0 * ATMO_MS_N + x1) * 3 + c];
        double t01 = a->ms[(y1 * ATMO_MS_N + x0) * 3 + c],
               t11 = a->ms[(y1 * ATMO_MS_N + x1) * 3 + c];
        out[c] = (t00 * (1 - fx) + t10 * fx) * (1 - fy) + (t01 * (1 - fx) + t11 * fx) * fy;
    }
}
static void ms_compute(const Atmo* a, double h, double mu_s, double out[3]) {
    const int N = 8, STEPS = 20;
    double r = RB + h, l2[3] = {0, 0, 0}, f[3] = {0, 0, 0};
    double sx = a_sqrt(1.0 - mu_s * mu_s), sy = mu_s; /* sun in the x-y plane, up = y */
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double ct = 1.0 - 2.0 * (i + 0.5) / N, st = a_sqrt(1.0 - ct * ct);
            double ph = 2.0 * PI * (j + 0.5) / N;
            /* direction w: cos-zenith ct; its x component st cos(ph) (for the sun cosine) */
            double wx = st * a_cos(ph), wy = ct;
            int g = hits_ground(r, wy);
            double dmax = g ? dist_bottom(r, wy) : dist_top(r, wy), dt = dmax / STEPS;
            double T[3] = {1, 1, 1}, L[3] = {0, 0, 0}, F[3] = {0, 0, 0};
            for (int s = 0; s < STEPS; s++) {
                double t = (s + 0.5) * dt;
                /* point x = p + t w (p = (0, r)): radius and its local up */
                double px = t * wx, py = r + t * wy, pz = t * st * a_sin(ph);
                double rx = a_sqrt(px * px + py * py + pz * pz), hx = rx - RB;
                double mus_x = (px * sx + py * sy) / rx, ts[3];
                trans_sun(a, rx, clampd(mus_x, -1.0, 1.0), ts);
                double dr = dens_ray(hx), dm = dens_mie(hx), dz = dens_ozone(hx);
                for (int c = 0; c < 3; c++) {
                    double ss = kRay[c] * dr + MIE_SCA * dm;
                    double se = kRay[c] * dr + MIE_EXT * dm + kOzone[c] * dz;
                    double tstep = a_exp(-se * dt), integ = se > 0.0 ? (1.0 - tstep) / se : dt;
                    L[c] += T[c] * ss * ts[c] * (1.0 / (4.0 * PI)) * integ;
                    F[c] += T[c] * ss * integ;
                    T[c] *= tstep;
                }
            }
            if (g) { /* Lambertian ground at the end of the ray */
                double px = dmax * wx, py = r + dmax * wy, pz = dmax * st * a_sin(ph);
                double rg = a_sqrt(px * px + py * py + pz * pz), mus_g = (px * sx + py * sy) / rg,
                       ts[3];
                trans_sun(a, rg, clampd(mus_g, -1.0, 1.0), ts);
                for (int c = 0; c < 3; c++)
                    L[c] += T[c] * ts[c] * (mus_g > 0.0 ? mus_g : 0.0) * GROUND_ALBEDO / PI;
            }
            for (int c = 0; c < 3; c++) {
                l2[c] += L[c] / (N * N);
                f[c] += F[c] / (N * N);
            }
        }
    for (int c = 0; c < 3; c++)
        out[c] = l2[c] / (1.0 - f[c]);
}

/* Sky integrals along a view ray from the camera: Rayleigh and Mie single scattering without
   the phase function (Bruneton's ComputeSingleScattering, trapezoid) and the multiple
   scattering term (the same trapezoid over T sigma_s Psi_ms). */
static void sky_integrals(const Atmo* a, double r, double mu, double mu_s, double nu, int samples,
                          double ray[3], double mie[3], double ms[3]) {
    int g = hits_ground(r, mu);
    double dx = (g ? dist_bottom(r, mu) : dist_top(r, mu)) / samples;
    double sr[3] = {0, 0, 0}, sm[3] = {0, 0, 0}, sms[3] = {0, 0, 0};
    for (int i = 0; i <= samples; i++) {
        double d = i * dx, w = (i == 0 || i == samples) ? 0.5 : 1.0;
        double rd = clampd(a_sqrt(d * d + 2.0 * r * mu * d + r * r), RB, RT);
        double musd = clampd((r * mu_s + d * nu) / rd, -1.0, 1.0), tv[3], ts[3], psi[3];
        trans_between(a, r, mu, d, g, tv);
        trans_sun(a, rd, musd, ts);
        double h = rd - RB, dr = dens_ray(h), dm = dens_mie(h);
        ms_lookup(a, h, musd, psi);
        for (int c = 0; c < 3; c++) {
            sr[c] += tv[c] * ts[c] * dr * w;
            sm[c] += tv[c] * ts[c] * dm * w;
            sms[c] += tv[c] * (kRay[c] * dr + MIE_SCA * dm) * psi[c] * w;
        }
    }
    for (int c = 0; c < 3; c++) {
        ray[c] = sr[c] * dx * kRay[c];
        mie[c] = sm[c] * dx * MIE_SCA;
        ms[c] = sms[c] * dx;
    }
}

unsigned long atmo_bytes(void) {
    return (unsigned long)(ATMO_T_W * ATMO_T_H * 3 + ATMO_MS_N * ATMO_MS_N * 3 +
                           ATMO_SLICES * ATMO_SKY_H * ATMO_SKY_W * 9) *
           sizeof(float);
}

void atmo_init(Atmo* a, void* mem, int trans_samples, int sky_samples) {
    float* m = (float*)mem;
    a->trans = m;
    a->ms = m + ATMO_T_W * ATMO_T_H * 3;
    a->sky = a->ms + ATMO_MS_N * ATMO_MS_N * 3;
    a->trans_samples = trans_samples;
    a->sky_samples = sky_samples;
    for (int c = 0; c < 3; c++) /* 680, 550, 440 nm -> table index (lambda - 360) / 10 */
        kOzone[c] = MAX_OZONE_DENSITY * kOzoneXs[(c == 0 ? 680 : c == 1 ? 550 : 440) / 10 - 36];
    double H = a_sqrt(RT * RT - RB * RB);
    for (int j = 0; j < ATMO_T_H; j++)
        for (int i = 0; i < ATMO_T_W; i++) { /* GetRMuFromTransmittanceTextureUv */
            double xm = unit_from_tc((i + 0.5) / ATMO_T_W, ATMO_T_W);
            double xr = unit_from_tc((j + 0.5) / ATMO_T_H, ATMO_T_H);
            double rho = H * xr, r = a_sqrt(rho * rho + RB * RB);
            double dmin = RT - r, dmax = rho + H, d = dmin + xm * (dmax - dmin);
            double mu =
                d == 0.0 ? 1.0 : clampd((H * H - rho * rho - d * d) / (2.0 * r * d), -1.0, 1.0);
            double t[3];
            trans_direct(r, mu, trans_samples, t);
            for (int c = 0; c < 3; c++)
                a->trans[(j * ATMO_T_W + i) * 3 + c] = (float)t[c];
        }
    for (int j = 0; j < ATMO_MS_N; j++)
        for (int i = 0; i < ATMO_MS_N; i++) {
            double o[3];
            ms_compute(a, (RT - RB) * j / (ATMO_MS_N - 1), -1.0 + 2.0 * i / (ATMO_MS_N - 1), o);
            for (int c = 0; c < 3; c++)
                a->ms[(j * ATMO_MS_N + i) * 3 + c] = (float)o[c];
        }
    for (int k = 0; k < ATMO_SLICES; k++) {
        double e = (ATMO_SLICE_MIN_DEG +
                    (ATMO_SLICE_MAX_DEG - ATMO_SLICE_MIN_DEG) * k / (ATMO_SLICES - 1)) *
                   PI / 180.0;
        double mus = a_sin(e), ce = a_cos(e);
        for (int j = 0; j < ATMO_SKY_H; j++) {
            double v = 2.0 * (j + 0.5) / ATMO_SKY_H - 1.0,
                   y = v < 0.0 ? -v * v : v * v; /* inverse of the sqrt map */
            double hz = a_sqrt(1.0 - y * y);
            for (int i = 0; i < ATMO_SKY_W; i++) {
                double cphi = 1.0 - 2.0 * (i + 0.5) / ATMO_SKY_W;
                double nu = clampd(hz * cphi * ce + y * mus, -1.0, 1.0), rr[3], mm[3], ss[3];
                sky_integrals(a, ATMO_CAMERA_R, y, mus, nu, sky_samples, rr, mm, ss);
                float* o = a->sky + (((long)k * ATMO_SKY_H + j) * ATMO_SKY_W + i) * 9;
                for (int c = 0; c < 3; c++) {
                    o[c] = (float)rr[c];
                    o[3 + c] = (float)mm[c];
                    o[6 + c] = (float)ss[c];
                }
            }
        }
    }
}

void atmo_trans_top(const Atmo* a, float r, float mu, float out[3]) {
    double o[3];
    trans_top(a, r, mu, o);
    for (int c = 0; c < 3; c++)
        out[c] = (float)o[c];
}
void atmo_sun_ground(const Atmo* a, float mu_s, float out[3]) {
    double o[3];
    trans_sun(a, ATMO_CAMERA_R, clampd(mu_s, -1.0, 1.0), o);
    for (int c = 0; c < 3; c++)
        out[c] = (float)o[c];
}
void atmo_single(const Atmo* a, float r, float mu, float mu_s, float nu, float ray[3],
                 float mie[3]) {
    double rr[3], mm[3], ss[3];
    sky_integrals(a, r, mu, mu_s, nu, a->sky_samples, rr, mm, ss);
    for (int c = 0; c < 3; c++) {
        ray[c] = (float)rr[c];
        mie[c] = (float)mm[c];
    }
}
/* Double-precision single scattering (the test harness compares it with Bruneton's reference). */
void atmo_single_d(const Atmo* a, double r, double mu, double mu_s, double nu, double ray[3],
                   double mie[3]) {
    double ss[3];
    sky_integrals(a, r, mu, mu_s, nu, a->sky_samples, ray, mie, ss);
}
void atmo_sky_for(const Atmo* a, float elev_deg, float* out) {
    float x = (elev_deg - ATMO_SLICE_MIN_DEG) / (ATMO_SLICE_MAX_DEG - ATMO_SLICE_MIN_DEG) *
              (ATMO_SLICES - 1);
    x = x < 0.0f ? 0.0f : (x > ATMO_SLICES - 1 ? (float)(ATMO_SLICES - 1) : x);
    int k0 = (int)x, k1 = k0 < ATMO_SLICES - 1 ? k0 + 1 : k0;
    float f = x - (float)k0;
    const float *s0 = a->sky + (long)k0 * ATMO_SKY_H * ATMO_SKY_W * 9,
                *s1 = a->sky + (long)k1 * ATMO_SKY_H * ATMO_SKY_W * 9;
    for (int i = 0; i < ATMO_SKY_H * ATMO_SKY_W * 9; i++)
        out[i] = s0[i] + (s1[i] - s0[i]) * f;
}

/* ---------------- run time ---------------- */
int atmo_asset_bind(AtmoAsset* s, const void* head, const void* atlas, const float* means) {
    const uint32_t* h = (const uint32_t*)head;
    if (h[0] != 0x324D5441u || h[1] != ATMO_T_W || h[2] != ATMO_T_H || h[3] != ATMO_SKY_W ||
        h[4] != ATMO_SKY_H || h[5] != ATMO_SLICES)
        return -1;
    s->t.trans = (float*)((const char*)head + 32);
    s->means = means;
    s->t.ms = 0;
    s->t.sky = 0;
    s->t.trans_samples = 0;
    s->t.sky_samples = 0;
    for (int t = 0; t < 3; t++)
        s->img[t] = (const uint16_t*)atlas + (long)t * ATMO_SKY_W * ATMO_SLICES * ATMO_SKY_H * 4;
    return 0;
}
void atmo_light_ground(const AtmoAsset* s, float mu, float out[3]) {
    double o[3];
    trans_sun(&s->t, ATMO_CAMERA_R, clampd(mu, -1.0, 1.0), o);
    for (int c = 0; c < 3; c++)
        out[c] = (float)o[c];
}
float atmo_elev_deg(float y) { /* asin by bisection on a_sin: robust to +-1 */
    double lo = -PI / 2, hi = PI / 2, t = clampd(y, -1.0, 1.0);
    for (int i = 0; i < 48; i++) {
        double m = 0.5 * (lo + hi);
        if (a_sin(m) < t)
            lo = m;
        else
            hi = m;
    }
    return (float)(0.5 * (lo + hi) * 180.0 / PI);
}
static float half_to_float(uint16_t h) {
    unsigned int s = (unsigned int)(h & 0x8000u) << 16, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu, x;
    if (e == 0) {
        if (!m)
            x = s;
        else {
            e = 1;
            while (!(m & 0x400u)) {
                m <<= 1;
                e--;
            }
            m &= 0x3FFu;
            x = s | ((e + 112u) << 23) | (m << 13);
        }
    } else if (e == 31)
        x = s | 0x7F800000u | (m << 13);
    else
        x = s | ((e + 112u) << 23) | (m << 13);
    union {
        unsigned int u;
        float f;
    } c;
    c.u = x;
    return c.f;
}
/* the slice position of a light: rows (k0 * H, k1 * H) and the blend */
static void slice_of(float y, float* r0, float* r1, float* f) {
    float x = (atmo_elev_deg(y) - ATMO_SLICE_MIN_DEG) / (ATMO_SLICE_MAX_DEG - ATMO_SLICE_MIN_DEG) *
              (ATMO_SLICES - 1);
    x = x < 0.0f ? 0.0f : (x > ATMO_SLICES - 1 ? (float)(ATMO_SLICES - 1) : x);
    int k0 = (int)x;
    if (k0 > ATMO_SLICES - 2)
        k0 = ATMO_SLICES - 2;
    *r0 = (float)(k0 * ATMO_SKY_H);
    *r1 = (float)((k0 + 1) * ATMO_SKY_H);
    *f = x - (float)k0;
}
/* bilinear fetch of image t at texture (u, v) in [0, 1], clamp to edge (the ps_dark sampler) */
static void fetch(const AtmoAsset* s, int t, float u, float v, float o[3]) {
    const int W = ATMO_SKY_W, H = ATMO_SLICES * ATMO_SKY_H;
    float x = u * W - 0.5f, y = v * H - 0.5f;
    int x0 = (int)(x < 0 ? -1 : x), y0 = (int)(y < 0 ? -1 : y);
    float fx = x - x0, fy = y - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = x0 < 0 ? 0 : (x0 > W - 1 ? W - 1 : x0);
    x1 = x1 < 0 ? 0 : (x1 > W - 1 ? W - 1 : x1);
    y0 = y0 < 0 ? 0 : (y0 > H - 1 ? H - 1 : y0);
    y1 = y1 < 0 ? 0 : (y1 > H - 1 ? H - 1 : y1);
    const uint16_t* p = s->img[t];
    for (int c = 0; c < 3; c++) {
        float a = half_to_float(p[((long)y0 * W + x0) * 4 + c]),
              b = half_to_float(p[((long)y0 * W + x1) * 4 + c]);
        float d = half_to_float(p[((long)y1 * W + x0) * 4 + c]),
              e = half_to_float(p[((long)y1 * W + x1) * 4 + c]);
        o[c] = (a * (1 - fx) + b * fx) * (1 - fy) + (d * (1 - fx) + e * fx) * fy;
    }
}
void atmo_sky_radiance(const AtmoAsset* s, const float L[3], const float V[3], float out[3]) {
    float r0, r1, f;
    slice_of(L[1], &r0, &r1, &f);
    float hl = (float)a_sqrt((double)L[0] * L[0] + (double)L[2] * L[2]);
    float hx = hl > 1e-6f ? L[0] / hl : 1.0f, hz = hl > 1e-6f ? L[2] / hl : 0.0f;
    float y = V[1], sq = (float)a_sqrt(y < 0 ? -y : y),
          vc = y >= 0.0f ? 0.5f + 0.5f * sq : 0.5f - 0.5f * sq;
    float h2 = V[0] * V[0] + V[2] * V[2];
    float cphi = (V[0] * hx + V[2] * hz) / (float)a_sqrt(h2 > 1e-12f ? h2 : 1e-12f);
    cphi = cphi < -1.0f ? -1.0f : (cphi > 1.0f ? 1.0f : cphi);
    float u = 0.5f - 0.5f * cphi, row = vc * ATMO_SKY_H;
    row = row < 0.5f ? 0.5f : (row > ATMO_SKY_H - 0.5f ? ATMO_SKY_H - 0.5f : row);
    const float inv = 1.0f / (ATMO_SLICES * ATMO_SKY_H);
    float nu = V[0] * L[0] + V[1] * L[1] + V[2] * L[2];
    float pr = 0.05968310365946075f * (1.0f + nu * nu);
    float g = ATMO_MIE_G, x = 1.0f + g * g - 2.0f * g * nu;
    float pm = 0.11936620731892150f * (1.0f - g * g) / (2.0f + g * g) * (1.0f + nu * nu) /
               (x * (float)a_sqrt(x));
    float t0[3], t1[3];
    for (int c = 0; c < 3; c++)
        out[c] = 0.0f;
    for (int t = 0; t < 3; t++) {
        fetch(s, t, u, (r0 + row) * inv, t0);
        fetch(s, t, u, (r1 + row) * inv, t1);
        float w = t == 0 ? pr : (t == 1 ? pm : 1.0f);
        for (int c = 0; c < 3; c++)
            out[c] += (t0[c] + (t1[c] - t0[c]) * f) * w;
    }
}
void atmo_sky_consts(const AtmoAsset* s, float* d, const float F[3], const float R[3],
                     const float U[3], float inv_fov, const float sun[3], float sun_scale,
                     const float moon[3], float moon_sky_scale, const float moon_view[3]) {
    (void)s;
    for (int i = 0; i < 40; i++)
        d[i] = 0.0f;
    for (int c = 0; c < 3; c++) {
        d[0 + c] = F[c];
        d[4 + c] = R[c] * inv_fov;
        d[8 + c] = U[c] * inv_fov;
        d[12 + c] = sun[c];
        d[24 + c] = moon[c];
        d[33 + c] = moon_view[c];
    }
    d[15] = sun_scale;
    d[27] = moon_sky_scale;
    const float* L[2] = {sun, moon};
    for (int k = 0; k < 2; k++) {
        float hl = (float)a_sqrt((double)L[k][0] * L[k][0] + (double)L[k][2] * L[k][2]);
        float* o = d + (k ? 28 : 16);
        o[0] = hl > 1e-6f ? L[k][0] / hl : 1.0f;
        o[1] = hl > 1e-6f ? L[k][2] / hl : 0.0f;
        slice_of(L[k][1], &o[2], &o[3], k ? &d[32] : &d[20]);
    }
    d[36] = ATMO_LIMB_R;
    d[37] = ATMO_LIMB_G;
    d[38] = ATMO_LIMB_B;
}

void atmo_sky_means(const AtmoAsset* s, float y, float up[3], float cosm[3]) {
    float r0, r1, f;
    slice_of(y, &r0, &r1, &f);
    int k0 = (int)r0 / ATMO_SKY_H, k1 = (int)r1 / ATMO_SKY_H;
    for (int c = 0; c < 3; c++) {
        up[c] = s->means[k0 * 6 + c] + (s->means[k1 * 6 + c] - s->means[k0 * 6 + c]) * f;
        cosm[c] =
            s->means[k0 * 6 + 3 + c] + (s->means[k1 * 6 + 3 + c] - s->means[k0 * 6 + 3 + c]) * f;
    }
}
float atmo_expf(float x) {
    return (float)a_exp(x);
}
