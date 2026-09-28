/* Physically based sky (atmosphere.c, compiled -O2): Bruneton's precomputed atmospheric
   scattering (2017 reference, BSD) for transmittance and single scattering, with Hillaire's
   2020 multiple-scattering approximation. Earth parameters of Bruneton's demo at 680 / 550 /
   440 nm. Everything is computed once at load; per frame the sky table for the current sun and
   moon elevations is interpolated and written for the GPU. */
#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include <stdint.h>

#define ATMO_T_W 256 /* transmittance table (Bruneton's parameterisation) */
#define ATMO_T_H 64
#define ATMO_MS_N 32   /* multiple scattering table: altitude x sun zenith cosine */
#define ATMO_SKY_W 64  /* sky table u = 0.5 (1 - cos(azimuth from the light)) */
#define ATMO_SKY_H 64  /* sky table v = 0.5 + 0.5 sign(y) sqrt(|y|), y = view direction height */
#define ATMO_SLICES 56 /* light elevation slices, -20 .. 90 degrees */
#define ATMO_SLICE_MIN_DEG -20.0f
#define ATMO_SLICE_MAX_DEG 90.0f

typedef struct {
    float* trans; /* ATMO_T_H x ATMO_T_W x 3 */
    float* ms;    /* ATMO_MS_N x ATMO_MS_N x 3 */
    float* sky; /* ATMO_SLICES x ATMO_SKY_H x ATMO_SKY_W x 9: Rayleigh, Mie, multiple (RGB each) */
    int trans_samples, sky_samples;
} Atmo;

unsigned long atmo_bytes(void);
/* mem: atmo_bytes() bytes, 16-aligned. trans_samples: transmittance integration steps
   (Bruneton: 500); sky_samples: single scattering steps (Bruneton: 50). */
void atmo_init(Atmo* a, void* mem, int trans_samples, int sky_samples);

/* Transmittance from radius r (m) along cos-zenith mu to the top of the atmosphere. */
void atmo_trans_top(const Atmo* a, float r, float mu, float out[3]);
/* Sunlight reaching the ground (per unit solar irradiance) for a sun at cos-zenith mu_s. */
void atmo_sun_ground(const Atmo* a, float mu_s, float out[3]);
/* Rayleigh / Mie single scattering (phase excluded) from the camera at radius r along the view
   cos-zenith mu, sun cos-zenith mu_s and view-sun cosine nu (Bruneton's ComputeSingleScattering).
 */
void atmo_single(const Atmo* a, float r, float mu, float mu_s, float nu, float ray[3],
                 float mie[3]);
void atmo_single_d(const Atmo* a, double r, double mu, double mu_s, double nu, double ray[3],
                   double mie[3]);
/* The sky table for a light at elevation elev_deg (interpolated between slices): W x H x 9. */
void atmo_sky_for(const Atmo* a, float elev_deg, float* out);

/* ---- run time (tables from assets/sky/atmosphere.bin, tools/make_atmosphere.c) ---- */
typedef struct {
    Atmo t; /* t.trans -> the float transmittance table of the asset */
    const uint16_t*
        img[3]; /* Rayleigh, Mie, multiple: RGBA16F, ATMO_SKY_W x (ATMO_SLICES * ATMO_SKY_H) */
} AtmoAsset;
/* head: the asset from its start (32-byte header + transmittance); atlas: the three images.
   Returns 0 if the header matches this build's table sizes. */
int atmo_asset_bind(AtmoAsset* s, const void* head, const void* atlas);
/* Sunlight reaching the eye (per unit solar irradiance) for a light at cos-zenith mu (terminator
   included) - the colour of the sun / moon as seen through the atmosphere. */
void atmo_light_ground(const AtmoAsset* s, float mu, float out[3]);
/* Sky radiance for a unit-illuminance light L (unit, world, y up) seen along V (unit): exactly the
   ps_dark lookup (table coordinates, bilinear, slice blend, Rayleigh and Cornette-Shanks phases).
 */
void atmo_sky_radiance(const AtmoAsset* s, const float L[3], const float V[3], float out[3]);
/* Light elevation in degrees (for the slice position). */
float atmo_elev_deg(float y);
/* ps_dark constants desc[164..203] (layout in ps_dark.s). F, R, U: camera basis (unit, world);
   inv_fov: 1 / tan(half vertical fov); moon_view: the sun direction in the moon's own frame
   (x right, y up, z toward the viewer). */
void atmo_sky_consts(const AtmoAsset* s, float* d, const float F[3], const float R[3],
                     const float U[3], float inv_fov, const float sun[3], float sun_scale,
                     const float moon[3], float moon_sky_scale, const float moon_view[3]);
/* Sun limb darkening I = mu^alpha (Hestroffer & Magnan 1998, eq. 5):
   alpha = -0.023 + 0.292 / lambda[um] at 680 / 550 / 440 nm. */
#define ATMO_LIMB_R 0.40641f
#define ATMO_LIMB_G 0.50791f
#define ATMO_LIMB_B 0.64064f
#define ATMO_MIE_G 0.8f
#define ATMO_BOTTOM 6360000.0f
#define ATMO_TOP 6420000.0f
#define ATMO_CAMERA_R (ATMO_BOTTOM + 2.0f) /* eye height above the ground */

#endif
