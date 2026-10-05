/* ShadCube4 - PS4 homebrew renderer (OpenOrbis toolchain, hand-written GCN shaders).
 *
 * One translation unit: main.c includes the header-only modules (src/ .h files); atmosphere.c,
 * bgm.c and loaders.c are separate objects. README.txt "SOURCE MAP" lists every file.
 *
 * A frame: pad -> camera, speed, time of day, clock source -> per-frame constants (MVP, sky,
 * lights, UI / clock tables) -> one PM4 command buffer (build_dcb): shadow depth -> MSAA scene
 * (sky ps_dark + stars, floor ps_floor, cube ps_model) -> resolve + aerial perspective
 * (ps_resolve) -> bloom down / blur, lens flare (ps_post_final) -> UI (ps_ui) or clock mode
 * (frost chain, ps_clock_light, ps_clock) -> flip.
 *
 * Navigation: search "§" and the number.
 *   §1  Includes, build switches, constants, buffer layout, globals
 *   §2  Low-level utilities         §9  Colour, sky colours, textures, assets
 *   §3  Trace log                   §10 Camera frame helpers, more descriptors
 *   §4  Memory                      §11 Modules: time sources, UI, glass clock
 *   §5  Math                        §12 Post-processing: tables, MSAA state, passes
 *   §6  Static geometry, flare      §13 Main command buffer (build_dcb)
 *   §7  Camera matrix (MVP)         §14 Shadow command buffer
 *   §8  Descriptors, star field     §15 main(): setup §15.1-6, main loop §15.7-14
 */

/* ==== §1 Includes, build switches, constants, buffer layout, globals ========================== */
#include <stdint.h>
/* SET_AFFINITY: pin the threads' CPU cores (scePthreadSetaffinity), as the game does. Trace
   "affinity ret=": 0 ok, <0 failed (default affinity kept), -98 off, -99 not reached. */
/* MAP_COMPUTE_QUEUES: map two compute queues at init (pipe 0 / 1, queue 4, ring 0x1000) as the
   game does, though nothing is dispatched to them; trace mcq= has both return codes. */
/* WAITFREE_SUBMIT: 0 = submit mode 0 (ioctl 0xc0108102, the game's path); 1 = the wait-free
   mode (ioctl 0xc020810c). */
/* DISPLAY_POLL: sample sceVideoOutGetFlipStatus and GetVblankStatus each frame for the trace
   (flips done / pending, current buffer, vblank count); its cost is traced as dpus. */
#define DISPLAY_POLL 1

#define WAITFREE_SUBMIT 0 /* GNM wait-free submit mode (§15.1): off */

#define MAP_COMPUTE_QUEUES 1 /* map two compute queues at init, as the game does (doc above) */

#define SET_AFFINITY 1 /* pin the threads' cores, as the game does (doc above) */

/* OPCODE_TEST: shadPS4 conformance test drawn by ps_ui, bottom right; 0 = off.

   1 = V_BFM_B32: two panels of the cube albedo, each pixel's UV packed into one dword and unpacked
   with v_bfm_b32 masks; 8-bit fields left (control), 16-bit right (the width / offset bit 4).

   2 = V_ALIGNBIT_B32 (left) / V_ALIGNBYTE_B32 (right): the UV (16:16) sits in a 64-bit window
   {hi, lo} at bit (x + y) & 31, or byte (x + y) & 3, with junk above it, read back with the align
   op.

   3 = V_CVT_PK_U8_F32 bit grid: 28 rows x 32 cells, each row one result dword, MSB left, white = 1.
   Rows 0..19 convert k_pku8_rows (S1 0, S2 0); rows 20..27 put 171.0 into 0x11223344 at byte
   select S1 = 0, 1, 2, 3, 4, 5, 7, 0xFFFFFFFF.

   4 = bit grid of 35 rows, each one instruction on the operands in k_optest4_data:
   V_CVT_PK_U16_U32, V_CVT_PK_I16_I32, V_CVT_PKNORM_U16_F32, V_CVT_PKNORM_I16_F32, S_BITSET1_B64,
   S_BITSET0_B64, V_ASHR_I64 (high, low dword) and V_CMPX_EQ_U64 / NE_U64 / EQ_I64 (bits 0..5 EXEC
   after the compare, 8..13 VCC).

   5 = every V_CVT_* of GCN2 (src/optest5.h, tools/gen_optest5.py) over the whole screen: ps_cvt
   computes each row in 5 float modes (FLOAT_MODE 0x00, 0xC0, 0x10, 0x20, 0x30), ps_ui draws two as
   hex digits, and at frame 120 the CPU logs them as GitHub tables to the trace log.

   6 = VOP3 modifiers (src/optest6.h, shaders/ps_mod.s): omod (mul:2 / mul:4 / div:2) and clamp on
   f32, f64, f16 and integer results, neg / abs on arithmetic and conversion sources, each row in 20
   float modes (every f32 x f64/f16 denormal mode, f32 rounding +inf / -inf / zero, f64 rounding to
   zero); the same page and log as test 5 (two modes on screen, all in the log). */
#define OPCODE_TEST 6

#include "atmosphere.h"
#include "bgm.h"
#include "dds_loader.h"
#include "loaders.h"
#include "logo_texture.h"
#include "nid_resolve.h"
#include "pm4.h"
#include "shaders.h"

/* after shaders.h: the OPCODE_TEST 5 / 6 pass binaries it declares */
#include "ps_opt5_set.h"

// memset/memcpy declared in nid_resolve.h

#define DISPLAY_W 1920 /* display / render size */
#define DISPLAY_H       1080

/* Sun and moon disc edge radii in pixels (1080p): both span ~0.5 degrees. */
#define SUN_DISC_RADIUS_PX 46.75f
#define MOON_DISC_RADIUS_PX 46.75f

/* Linear HDR pipeline: the scene renders to RGBA16F, bloom runs at quarter
   resolution, the composite writes the sRGB display buffer (videoout format
   A8R8G8B8Srgb). Values are linear light; 1.0 = display white. */
/* Sun disc colour (through the atmosphere) x this (desc[84]); the disc is limb-darkened
   (Hestroffer & Magnan power law). */
#define SUN_HDR 2.19f
/* Physically based sky (atmosphere.c, assets/sky/atmosphere.bin, ps_dark): sky radiance x
   SKY_SUN_SCALE = pi (a white surface facing the sun shows radiance 1 = the light colour,
   physically E / pi). Night: the moon's sky light x MOON_SKY_SCALE (a moon 45 degrees up gives the
   zenith sRGB 0.01 0.02 0.06). Moon disc: NASA LROC albedo x Lommel-Seeliger x MOON_SCALE (full
   moon mean luminance 0.722). */
#define SKY_SUN_SCALE 3.14159265f
#define MOON_SKY_SCALE 0.1481f
#define MOON_SCALE 3.6375f
/* Aerial perspective (ps_resolve, per MSAA sample): the scene seen through the sky's own atmosphere
   (Rayleigh + Mie, their scale heights) at AERIAL_M_PER_UNIT metres per world unit; the fog colour
   is the sky's at the camera's horizon in that direction, so the far floor fades into the sky
   above it. Green extinction 1.7998e-5 / m x 114.16 = 2.0547e-3 / unit (blue 2.3x, red 0.57x);
   scale heights: Rayleigh 70 units, Mie 10.5. */
#define AERIAL_M_PER_UNIT 114.16f
/* Arena: the camera stays within +-ARENA_HALF of the centre and ARENA_EYE_MIN .. ARENA_HEIGHT above
   the floor under it, so the floor always reaches the horizon: sqrt(2 FLOOR_R ARENA_HEIGHT) = 1673
   <= FLOOR_HALF - ARENA_HALF = 1700 (the dipped horizon is FLOOR_R's, the same planet). */
#define ARENA_HALF 300.0f
#define ARENA_HEIGHT 100.0f
#define ARENA_EYE_MIN 0.1f /* never below the floor */
#define CAM_NEAR 0.01f
#define CAM_FAR 2500.0f      /* the farthest visible floor: the horizon, <= 1673 away */
#define MOON_LIGHT 0.621f    /* night light magnitude (moonlight): 0.69 - 10% */
/* D-pad left / right time-of-day scrub (sun_angle, rad/s): DAY_SCRUB_START (1 deg/s), then
   DAY_SCRUB_FAST times that after DAY_SCRUB_FAST_AFTER s held. */
#define DAY_SCRUB_START 0.017453293f
#define SUN_GLIDE_S 2.0f /* R3 / L3: the sun glides to the clock's position over this */
#define DAY_SCRUB_FAST_AFTER 2.0f /* s held: taps and shorter holds stay at DAY_SCRUB_START */
#define DAY_SCRUB_FAST 5.0f       /* then this many times faster */
/* Moonlight colour (sRGB, desaturated cool blue) x its transmittance: the night light of the cube
   (desc[32]) and of every floor point (ps_floor, desc[117..119] linear) */
static const float k_moon_light_srgb[3] = {0.52f, 0.64f, 0.84f};
/* The same colour for the moonlit sky: linear moonlight / its luminance (Rec. 709), times
   MOON_SKY_SCALE per channel (ps_dark d[30..32], ps_resolve [72..74]) - the night sky keeps its
   brightness and takes the night light's colour (the moon's own sky light, like the sun's, is
   neutral: its horizon read orange-brown next to the blue-lit floor) */
static float g_moon_tint[3] = {1.0f, 1.0f, 1.0f};
#define BLOOM_THRESHOLD 1.0f /* only what is brighter than white blooms */
/* bloom strength (sun: +0.20 at 60 px, +0.05 at 100 px) */
#define BLOOM_INTENSITY 0.1f
#define EXPOSURE 1.0f /* final scale of the post composite */

/* Floor surface (ps_floor): parallax occlusion mapping from the height map
   and distance fog toward the sky gradient. */
#define POM_DEPTH 0.025f     /* relief depth in world units (a texture tile is 4 units) */
#define POM_FADE_START 10.0f /* full parallax up to this distance */
#define POM_FADE_END 50.0f   /* ... then a linear fade to zero at this distance */
#define FOG_MIN 0.03f        /* fog at distance 0; grows as FOG_MIN * e^(d / L) */
#define FOG_FULL 290.0f      /* distance where the fog reaches 100% (floor edge is FLOOR_HALF) */
/* Midday sun disc colour (sRGB); amber at the horizon ramps to this. */
#define SUN_DAY_R 1.00f
#define SUN_DAY_G 0.97f
#define SUN_DAY_B 0.87f

/* Imported models (OBJ / STL / PLY): vs_model + ps_model, model transform on the GPU. */
#define MODEL_FIT_RADIUS                                                                           \
    0.6928203f                    /* scaled to the built-in cube's bounding radius (0.4 * sqrt 3) */
#define MODEL_POM_DEPTH 0.003125f  /* relief depth in UV units (= D_UV in tools/gen_model.py) */
#define MODEL_RELIEF 1.0f         /* relief self-shadow strength */
#define MODEL_SHADOW_OFFSET 0.02f /* shadow lookup: offset along the normal (world units) */
#define MODEL_SHADOW_BIAS                                                                          \
    (1.5f / 255.0f) /* shadow lookup: depth bias, 1.5 steps of the 8-bit map */
#define MODEL_ROUGHNESS 0.45f /* satin finish: GGX roughness (0 mirror .. 1 matte) */
#define MODEL_F0 0.04f        /* Fresnel reflectance at normal incidence (paint / plastic) */
#define FLARE_STRENGTH 1.0f   /* lens flare: 0 off; x sun colour x visibility x edge fade */
#define MOVE_SPEED_100 0.02f  /* camera speed shown as 100% */
/* D-pad up / down: the camera speed in whole SPEED_STEP_PCT steps. A press is one step; held, it
   repeats after SPEED_REPEAT_AFTER s (a slow tap stays one step), one step per SPEED_REPEAT_EVERY
   s, and SPEED_FAST times as often after SPEED_FAST_AFTER s held (as the time-of-day D-pad). */
#define SPEED_STEP_PCT 5
#define SPEED_MIN_PCT 5
#define SPEED_MAX_PCT 2500
#define SPEED_REPEAT_AFTER 0.5f
#define SPEED_REPEAT_EVERY 0.1f
#define SPEED_FAST_AFTER 2.0f
#define SPEED_FAST 5.0f
#define DAY_REPEAT_DELAY 0.40f /* L1 / R1 held: first repeat after this (s) */
#define DAY_REPEAT_EVERY 0.15f /* then one step every this (s) */
/* Day and night multiples for L1 / R1 (x1 = one day in ~39 s). */
/* Day and night speed steps in tenths (x0.1 .. x0.9, x1, x2, x4 .. x20); starts at x1. */
static const int k_day_tenths[] = {1,  2,  3,  4,  5,   6,   7,   8,   9,   10,
                                   20, 40, 60, 80, 100, 120, 140, 160, 180, 200};
#define DAY_MULT_COUNT                                                                             \
    ((int)(sizeof(k_day_tenths) / sizeof(k_day_tenths[0]))) /* entries in k_day_tenths */
#define DAY_MULT_ONE 9        /* index of x1 */
#define FLARE_GHOSTS 1.0f     /* soft ghosts (the first flare's six) */
#define FLARE_RAYS 0.0f       /* uneven rays (glare texture, tools/make_glare.py): off */
#define FLARE_GLOW 1.4f       /* glow around the sun: FLARE_GLOW / (1 + (rho / 0.08)^2) */
#define FLARE_VEIL 0.22f      /* wide warm haze: FLARE_VEIL / (1 + (rho / 0.40)^2) */
#define GLARE_STORE_MAX 4.0f  /* glare.dds stores value / this (STORE_MAX in make_glare.py) */
#define FLARE_EDGE 0.12f      /* the GHOSTS fade out over this screen fraction at the edges */

/* Printed in the trace header so logs from different builds can be told apart. */
#define BUILD_TAG "optest-6b"
/* Shadow map: 4096 x 4096, GPU-only (written by the shadow pass, sampled by the floor). In shadPS4
   turn readbackLinearImages off for this title: with it on, this linear target hits its 32 MB
   readback limit. */
#define SHADOW_W        4096
#define SHADOW_H        4096
/* BATCH_FRAMES: frames recorded into one command buffer and submitted together (1: each frame on
   its own). The system's submit quota counts calls, not buffers; a batch adds its length in
   latency. NUM_FRAMES must be >= BATCH_FRAMES. */
/* HALF_RATE: 1 = 30 fps (a second flip event consumed per frame); 0 = 60 fps. */
#define HALF_RATE       0
/* KEEP_GPU_FED: submits per frame: 1 = one; N adds N - 1 no-flip keep-alive submits during the
   vblank wait (the game submits its frame's work ~17 times). */
/* FORCE_NO_FLIP: 1 = build every frame with EVENT_WRITE_EOP, submit with
   sceGnmSubmitCommandBuffers and never flip (nothing is presented: a test switch). */
/* CPU_FLIP: present with sceVideoOutSubmitFlip from the CPU once the frame's EOP fence arrives
   (the DCB is built with no_flip=1 and submitted with sceGnmSubmitCommandBuffers) instead of the
   marker's EOP flip. */
/* FENCE_SLOTS: fence addresses rotated through, one page apart (1: one fixed address). */
#define FENCE_SLOTS     1

#define CPU_FLIP 1 /* present with sceVideoOutSubmitFlip once the fence arrives (doc above) */

#define FORCE_NO_FLIP 0 /* 1: never flip (test switch) */

/* 2 = two submits per frame but still ONE flip. A DISCRIMINATOR, not a fix:
   the failure is a COUNT (frame ~547 whether that takes 11 seconds at 60fps or
   67 seconds at 8fps), and everything consumed once per frame is still a
   suspect. Two submits per frame separates them:
       dies at frame ~273 -> the resource is per SUBMIT
       dies at frame ~547 -> per FRAME or per FLIP; submits are innocent
   Set back to 1 for one submit per frame. */
/* PACE_ON_FENCE_ONLY: 1 = pace on the fence spin alone, as the game does (its GPU-bound frame
   holds it to 60 fps), only draining the flip event queue. 0 = block on the flip event each frame:
   our frame is short, so fence pacing alone free-runs and overfills the 16-deep flip queue. */
#define PACE_ON_FENCE_ONLY 0
/* PREDRAW 1: after queuing frame N's flip, wait only until at most one flip is pending (N's),
   not until N is on screen: flip N-1 has retired, so its display buffer - frame N+1's target with
   NUM_FRAMES 3 - is free, and frame N+1 renders while N waits for vblank. Frame N+1's constants
   are still written only after frame N's fence; at most 2 flips are ever queued. */
#define PREDRAW 1

#define KEEP_GPU_FED    1
#define KA_SLOTS        4      /* dedicated keep-alive command buffers */
#define BATCH_FRAMES    1
/* display buffers (the game registers 3) */
#define NUM_FRAMES      3
#define DCB_SIZE 0x20000 /* bytes per command buffer */
/* NOP padding added to each frame DCB, in dwords (0: off) */
#define DCB_PAD_DWORDS  0
#define BG_VERTS        6
#define CUBE_VERTS 36                             /* 12 triangles */
#define FLOOR_VERTS (FLOOR_GRID * FLOOR_GRID * 6) /* 2 tris per grid quad */
#define FLOOR_GRID 128
#define FLOOR_HALF 2000.0f /* floor spans +-FLOOR_HALF in X and Z */
/* The planet: the floor is y = -0.5 - r^2 / (2 FLOOR_R), and the horizon a camera h above it sees
   dips by atan(sqrt(2 h / FLOOR_R)) (7 deg from 100 up): the sky follows it (ps_dark), so from up
   high the sky, the sun and the sunset show below eye level down to the real horizon. */
#define FLOOR_R 14000.0f
#define FLOOR_UV_MAX 1000.0f /* texture tiles across: one per 4 units */
#define VERT_STRIDE     48
#define IDENT_OFF 0                                    /* static VB: identity matrix (64 B) */
#define BG_SUN_OFF 64                                  /* sky: sun direction (16 B) */
#define BG_DATA_OFF 80                                 /* sky quad, BG_VERTS vertices */
#define MVP_OFF (BG_DATA_OFF + BG_VERTS * VERT_STRIDE) /* the camera MVP (64 B, per frame) */
#define SUN_DIR_OFF (MVP_OFF + 64)                     /* sun direction for the cube (16 B) */
#define CUBE_DATA_OFF (SUN_DIR_OFF + 16)               /* cube vertices */
/* Floor uses its own V# base = FLOOR_MVP_OFF. VS expects MVP at V#+0 and
   vertex data at V#+80 (same as cube). We reserve 80 bytes (MVP mirror +
   SUN_DIR mirror padding) right before floor vertex data. */
#define FLOOR_MVP_OFF   (CUBE_DATA_OFF + CUBE_VERTS * VERT_STRIDE)
#define FLOOR_DATA_OFF  (FLOOR_MVP_OFF + 80)
#define VERT_BUF_SIZE (FLOOR_DATA_OFF + FLOOR_VERTS * VERT_STRIDE) /* the whole static VB */
#define LIGHT_MVP_OFF   (VERT_BUF_SIZE + 256)  /* light-space MVP for shadow pass */
/* One contiguous range covering EVERY per-frame CPU write into vb:
   BG_SUN, MVP, SUN_DIR, the rotated cube vertices and the floor MVP mirror.
   The light-space MVP lives outside it and is staged separately. */
#define BATCH_VB_RANGE  (FLOOR_MVP_OFF + 64 - BG_SUN_OFF)
#define BATCH_STAGE_SZ  (BATCH_VB_RANGE + 64)
#define PROT_CPU_RW     0x03
#define PROT_GPU_RW     0x30          /* GPU_READ 0x10 | GPU_WRITE 0x20 (per OpenOrbis orbis/_types/kernel.h) */
/* PS4 direct memory types.
   ONION  = WB, CPU<->GPU coherent (command buffers, fences)
   GARLIC = WC, high GPU bandwidth, not CPU-coherent (render targets, textures) */
#define MEM_TYPE_ONION  0x00
#define MEM_TYPE_GARLIC 0x03


/* Per-pass GPU timestamps (64-bit GPU clock, EOP): [0] frame start, [1] after
   the shadow pass, [2] after the sky, [3] after the floor, [4] after the cube. */
static volatile uint64_t *g_gpu_ts = 0;
/* Stars: world-fixed quads on a sphere (see build_stars), drawn after the sky
   with additive blending; colour x fade comes from desc[36..39] per frame. */
static void* g_ps_stars_gpu = 0;
static uint32_t g_stars_v[4]; /* the stars' V# */
static int g_stars_n = 0;    /* star quads in the buffer */
static int g_stars_draw = 0; /* 0 while fully faded out (or on the loading screen) */
/* Post-processing (emit_post): HDR scene target and a 6-level bloom chain
   (quarter res down to 15x9; one level alone spreads only ~sigma 1.4 texels,
   the coarse levels give the wide halo). Pitches are 64-texel multiples: the
   T# TILING_INDEX 8 is linear aligned. A = downsample / blurred level, B = blur
   temp, then the up-sampled sum. One 32-dword descriptor table per pass. */
/* 4 bloom levels, 480 x 270 down to 60 x 34 */
#define BLOOM_LEVELS 4
static const uint16_t g_bloom_w[BLOOM_LEVELS] = {480, 240, 120, 60};
static const uint16_t g_bloom_h[BLOOM_LEVELS] = {270, 135, 68, 34};
static const uint16_t g_bloom_pitch[BLOOM_LEVELS] = {512, 256, 128, 64};
#define POST_PASSES (BLOOM_LEVELS * 4) /* down, blur H, blur V, up-add (last: composite) */
/* Post table (32-dword blocks): the bloom chain + final pass (0..POST_PASSES-1), the final
   pass's flare dwords (next block), then the frost chain (down 1920 -> 480, down 480 -> 240,
   blur H V H V at 240 x 135) and the UI pass (2 blocks). */
/* 4x MSAA on the scene (1 = off): 4-sample RGBA16F colour + 4-sample depth, resolved into g_hdr
   before the post chain (ps_resolve). */
#define MSAA_SAMPLES 4
#define MSAA_TILE_INDEX                                                                            \
    13 /* PS4 GB_TILE_MODE13 Thin1dThin: ARRAY_1D_TILED_THIN1, thin micro tiles */
#define FROST_BLOCK (POST_PASSES + 2)
#define FROST_PASSES 6                        /* the UI glass's frost chain */
#define UI_BLOCK (FROST_BLOCK + FROST_PASSES) /* ps_ui: 8 blocks (64..255: OPCODE_TEST) */
#define RESOLVE_BLOCK (UI_BLOCK + 8)          /* ps_resolve: 3 blocks */
#define CLOCK_FROST_BLOCK                                                                          \
    (RESOLVE_BLOCK + 3) /* the resolve uses three blocks; clock mode: frost                        \
                         */
#define CLOCK_LIGHT_BLOCK                                                                          \
    (CLOCK_FROST_BLOCK + 6)                 /* 240 -> 120 -> 60, H V H V at 60 x 34; light         \
                                             */
#define CLOCK_BLOCK (CLOCK_LIGHT_BLOCK + 1) /* ps_clock: 64 dwords */
#define POST_TABLE_BLOCKS (CLOCK_BLOCK + 2)
static void* g_hdr = 0;               /* the scene after the MSAA resolve (RGBA16F, ps_resolve) */
static void* g_bloom_a[BLOOM_LEVELS]; /* bloom level i: downsampled / blurred */
static void* g_bloom_b[BLOOM_LEVELS]; /* bloom level i: blur temp, then the up-sampled sum */
static uint32_t* g_post_tab = 0;      /* post pass tables, 32 dwords per block (*_BLOCK) */
static uint16_t* g_hz_table = 0; /* ps_resolve: the sky's horizon at every slice, 64 x 168 RGBA16F
                                   (atmo_horizon_texture, static) */
static uint16_t* g_trans_table =
    0; /* ps_floor: sunlight through the atmosphere, 256 x 1 RGBA16F (atmo_trans_table) */
/* Prop box for the lens flare occlusion test: model-space bounds and the world
   transform (3x4 rows) of the loaded model, or of the built-in cube (+-0.4). */
static float g_prop_lo[3] = {-0.4f, -0.4f, -0.4f};
static float g_prop_hi[3] = {0.4f, 0.4f, 0.4f};
static float g_prop_m[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.4f, 0.0f, 0.0f, 1.0f, 0.0f};
static void* g_ps_post_down_gpu = 0; /* post shaders in GPU memory (§15.5) */
static void* g_ps_post_blur_gpu = 0;
static void* g_ps_post_comp_gpu = 0;
static void* g_ps_post_final_gpu = 0;
static void* g_ps_ui_gpu = 0;
#define OPT5_PASS_MAX 20                   /* OPCODE_TEST 5 / 6: passes, one a float mode */
static void* g_o5_ps[OPT5_PASS_MAX] = {0}; /* ps_cvt (5) / ps_mod (6), one a float mode */
#define OPT5_STRIDE (8 * OPT5_MODES)       /* result bytes a slot: lo, hi a float mode */
#define OPT5_LINE (80 + 26 * OPT5_MODES)   /* log line: label <= 64, 26 a mode */
#define OPT5_RT_W 512                      /* the passes' target: one pixel per slot */
#define OPT5_MARK_A 0xC0DE5A00u /* written after the last slot by each pass */
#define OPT5_MARK_B 0xC0DE5B00u
#define OPT5_MARK(m) (OPT5_MARK_A + ((uint32_t)(m) << 8)) /* A..E: 0xC0DE5A00..0xC0DE5E00 */
static struct {
    int ok, logged;
    uint32_t *slots, *res,
        *tab;         /* slot table {op, a, b, c}, results {A lo, A hi, B lo, B hi}, pass tables */
    void *rt, *strip; /* the passes' target, the hex digit strip (256 x 16 RGBA8) */
} g_o5;
static void* g_ps_clock_gpu = 0;       /* clock mode's UI pass (ps_clock) */
static void* g_ps_clock_light_gpu = 0; /* its internal light (ps_clock_light) */
static int g_clock_mode = 0;           /* Circle: the glass clock instead of the panels */
static float* g_clock_entries = 0;     /* ps_clock_light's rim entries, 3 x CLOCK_RIM_MAX x 8 */
static int g_clock_entry_buf = 0;      /* which of the 3 entry buffers is next */
static void* g_clock_light_rt =
    0; /* the internal light field (480 x 270 RGBA16F), kept between frames */
static int g_clock_light_dirty = 0; /* this frame recomputes it (the light's direction moved) */
static int g_clock_light_valid = 0; /* it holds the field for g_clock_light_l2 */
static float g_clock_light_l2[2];   /* the light direction the field was computed for */
static void* g_ps_resolve_gpu = 0;  /* ps_resolve in GPU memory */
static void* g_msaa_color = 0; /* 4-sample scene colour (RGBA16F, tile 13), 0 = MSAA off */
static void* g_msaa_depth = 0; /* 4-sample scene depth (Z_32_FLOAT, 1D tiled) */
static void* g_frame = 0; /* the finished frame, linear RGBA16F (final pass -> frost chain, ps_ui) */
#define GPU_TS(k) do { if (g_gpu_ts) pm4_gpu_timestamp(b, &g_gpu_ts[(k)]); } while (0)

/* ==== §2 Low-level utilities: memset / memcpy, timing, GNM queries ============================ */
// === Helpers ===
static void my_memset(void *d, int v, unsigned long n) {
    unsigned char *p = (unsigned char *)d;
    for (unsigned long i = 0; i < n; i++) p[i] = (unsigned char)v;
}
static void my_memcpy(void *dst, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (unsigned long i = 0; i < n; i++) d[i] = s[i];
}

/* File trace logger. Open ONCE, keep the fd, fsync after every write so the
   log survives a GPU crash (the system kills us and snapshots a core dump;
   without fsync the buffered log never reaches disk). No libc; hand-rolled int
   formatting. */
static int g_log_fd = -1;
static int32_t g_last_event = 0;   /* latest non-zero sceSystemServiceReceiveEvent type */
static int g_event_count = 0;      /* count of non-zero system events received */
static int g_submit_count = 0;     /* total GPU command-buffer submits */
static int g_batch = 0;      /* completed batches, for crash localisation */
static int g_slow_tail = 0;  /* frames still to log at full rate after a stall */
static int g_evt_timeouts = 0;  /* flip-event waits that expired instead of firing */
static int g_keepalive = 0;     /* extra no-flip submits issued to keep the GPU fed */
static int g_ka_fail = 0;       /* SubmitDone failures (0x80d110ff) */
static int g_dcb_overflow = 0;  /* frames whose DCB overflowed and were skipped */
static int g_evt_drained = 0;   /* surplus flip events drained (should stay 0) */
static int g_flips_skipped = 0; /* flips NOT issued because the fence had stalled */
/* Set when the flip event stops arriving (the display fails before the fence does); while set,
   frames are submitted without a flip. */
static int g_display_stalled = 0;
static int g_stall_recoveries = 0;
/* GPU-side checkpoint slot. The CP writes a stage code here as it retires
   each part of the command buffer; after a hang the last value tells us
   which packet it stopped on. Set by main() once the allocation exists. */
static volatile uint32_t *g_cp_mark = 0;
/* The stage code alone is useless: it is a CONSTANT, so if the command
   processor stops, the slot still reads the last value written and looks
   identical to a live one. Pack the FRAME NUMBER into the upper bits so the
   value CHANGES every frame - then a stale cpm proves the CP has stopped. */
#define CPMARK(b, code) do { \
    if (GPU_CHECKPOINTS && g_cp_mark) \
        pm4_write_data_dword((b), g_cp_mark, ((g_cp_frame & 0xFFFFFF) << 8) | (code)); \
} while (0)
static uint32_t g_cp_frame = 0;
/* Timestamps cost a kernel round trip each, and we made FIFTEEN of them per
   frame purely to fill trace fields that are discarded on 15 frames out of 16.
   The reference title does not import sceKernelGetProcessTime at all. This
   returns a real timestamp only on frames that will actually be logged, and 0
   otherwise - so instrumentation costs nothing on the frames it is not used
   on. g_trace_this_frame is set once per frame, before the work begins. */
static int g_trace_this_frame = 0;
static int g_affinity_ret = -99;   /* result of scePthreadSetaffinity at init */
static int g_hw_ok = -1;           /* kernel's own GPU health verdict */
static int g_hw_ok_at_stall = -1;  /* same, sampled at the first fence timeout */
static int g_mcq0 = -99, g_mcq1 = -99;   /* MapComputeQueue return codes */
/* display-pipeline counters, sampled per frame under DISPLAY_POLL */
static long long g_fs_num = -1, g_fs_pend = -1, g_fs_gpu = -1, g_fs_cur = -1;
static long long g_vbl = -1, g_dp_us = 0;
static int g_cpuflip_fail = 0, g_last_cpuflip = 0;   /* CPU_FLIP diagnostics */
static volatile uint32_t *g_last_fence = 0;   /* fence slot of the last accepted submit */
static uint32_t g_last_fv = 0;                /* value that submit will write */

/* Keep an angle in [0, 2pi). The rotation accumulators grow without bound -
   after an hour at 60fps cam_yaw reaches ~4320 rad, where float32 has only
   ~0.001 rad of resolution left and the motion visibly quantises. Wrapping
   costs nothing and removes a class of long-run drift.
   (Checked: this is NOT the frame-547 hang - at 547 no sin or cos of any
   accumulator is near zero, so nothing degenerates there.) */
static float wrap_2pi(float a) {
    const float TWO_PI = 6.28318530718f;
    while (a >= TWO_PI) a -= TWO_PI;
    while (a < 0.0f)    a += TWO_PI;
    return a;
}
static inline uint64_t tstamp(void) {
    return g_trace_this_frame ? sceKernelGetProcessTime() : 0;
}
static int g_fence_timeouts = 0;    /* EOP fence waits that expired */
static unsigned int g_fence_stuck_at = 0;  /* fence value when it stopped */
static unsigned int g_fence_wanted = 0;    /* value we were waiting for */
static void trace_msg(const char *s);

/* Asset directory. /data/ShadCube4/ is the real one. The PS4 filesystem is
   case-sensitive, so every asset lookup also tries the other capitalisation as
   a fallback - it costs nothing and means a differently-cased folder still
   loads. */
#define DATA_DIR_NEW "/data/ShadCube4/"   /* primary */
#define DATA_DIR_OLD "/data/Shadcube4/"   /* alternate capitalisation, harmless */
static int lg_i64(char *o, long long v);
static void trace_line(const char *buf, unsigned long n);
/* Write a phase marker, but only for the batches around the observed crash
   (it dies during batch 34), so this costs nothing for the whole run before. */
/* Phase markers are collected and written once per frame (phase_flush: one write + one fsync)
   instead of a write + fsync per marker. */
static char g_ph_buf[1024];
static int  g_ph_len = 0;
static void phase(const char *tag){
    if (g_batch < 500 || g_batch > 580) return;
    if (g_ph_len > (int)sizeof(g_ph_buf) - 64) return;   /* never overrun */
    char *L = g_ph_buf + g_ph_len; int p=0;
    const char *m="PH "; while(*m) L[p++]=*m++;
    p+=lg_i64(L+p,(long long)g_batch);
    L[p++]=' ';
    while(*tag) L[p++]=*tag++;
    L[p++]='\n';
    g_ph_len += p;
}
static void phase_flush(void){
    if (g_ph_len <= 0) return;
    trace_line(g_ph_buf, (unsigned long)g_ph_len);
    g_ph_len = 0;
}

/* Spin body for the teardown quiesce; the game uses 16 pauses per iteration. */
/* gnm's in-flight submit counter (sceGnmAreSubmitsAllowed only reports count == 0). The function
   starts with lea rax,[rip+disp32] (48 8d 05) and mov rcx,[rax] (48 8b 08): disp32 is read from its
   own bytes; counter = **(uint32_t**)(fn + 7 + disp); -1 if the signature does not match. */
static int gnm_inflight_count(void) {
    const unsigned char *f = (const unsigned char *)&sceGnmAreSubmitsAllowed;
    if (f[0] != 0x48 || f[1] != 0x8d || f[2] != 0x05) return -1;
    int32_t disp;
    __builtin_memcpy(&disp, f + 3, 4);
    uint32_t **pp = (uint32_t **)(void *)(f + 7 + (long)disp);
    if (!pp || !*pp) return -1;
    return (int)**pp;
}

/* gnm's unnamed set / clear / get functions for the submit mode (0: ioctl 0xc0108102, the game's;
   1: the wait-free ioctl 0xc020810c - WAITFREE_SUBMIT), pushed to the kernel by the next
   sceGnmSubmitDone (ioctl 0xc004811d). They sit at fixed offsets from the exported
   sceGnmAreSubmitsAllowed and take no arguments: +0x40 set mode 1, +0x90 set mode 0, +0xe0 read
   mode. Both prologues (the anchor's lea rax,[rip+..] = 48 8d 05, the target's push rbp; mov
   rbp,rsp = 55 48 89 e5) are checked first, so a firmware with another layout gets a no-op, not a
   wild jump. */
static const unsigned char *gnm_mode_fn(int off) {
    const unsigned char *anchor = (const unsigned char *)&sceGnmAreSubmitsAllowed;
    if (anchor[0] != 0x48 || anchor[1] != 0x8d || anchor[2] != 0x05) return 0;
    const unsigned char *fn = anchor + off;
    if (fn[0] != 0x55 || fn[1] != 0x48 || fn[2] != 0x89 || fn[3] != 0xe5) return 0;
    return fn;
}
static int gnm_set_mode(int on) {
    /* The driver's own mode setter dereferences the in-flight counter pointer
       with NO null check, so calling it before gnm's lazy init would fault.
       gnm_inflight_count() validates that entire pointer chain, so use it as
       the readiness test. -2 = driver not ready yet, caller may retry. */
    if (gnm_inflight_count() < 0) return -2;
    const unsigned char *fn = gnm_mode_fn(on ? 0x40 : 0x90);
    if (!fn) return -1;
    ((void (*)(void))(void *)fn)();
    return 0;
}
static int gnm_get_mode(void) {
    const unsigned char *fn = gnm_mode_fn(0xe0);
    if (!fn) return -1;
    return ((int (*)(void))(void *)fn)();
}

static inline void cpu_pause16(void){
    for (int i=0;i<16;i++) __asm__ __volatile__("pause" ::: "memory");
}
/* ==== §3 Trace log (/user/data/ShadCube4/ShadCube4 trace.log) ================================= */
static unsigned long lg_len(const char *s){ unsigned long n=0; while(s[n]) n++; return n; }
static int lg_u64(char *o, unsigned long long v){
    char t[24]; int i=0,j=0;
    if(v==0){ o[0]='0'; return 1; }
    while(v){ t[i++]=(char)('0'+(v%10)); v/=10; }
    while(i) o[j++]=t[--i];
    return j;
}
static int lg_i64(char *o, long long v){
    int j=0; if(v<0){ o[j++]='-'; v=-v; } return j+lg_u64(o+j,(unsigned long long)v);
}
static int lg_hex(char *o, unsigned long long v){
    const char *h="0123456789abcdef"; char t[16]; int i=0,j=0;
    o[j++]='0'; o[j++]='x';
    if(v==0){ o[j++]='0'; return j; }
    while(v){ t[i++]=h[v&0xf]; v>>=4; }
    while(i) o[j++]=t[--i];
    return j;
}
/* trace_init: the log is /user/data/ShadCube4/ShadCube4 trace.log (the folder made when missing;
   an existing one only returns an error), opened once (WRONLY|CREAT|TRUNC = 0x601) and kept open;
   the other paths only if that fails. */
extern int sceKernelMkdir(const char*, unsigned short);
static void trace_init(void){
    sceKernelMkdir("/user/data/ShadCube4", 0777);
    const char* paths[] = {"/user/data/ShadCube4/ShadCube4 trace.log",
                           DATA_DIR_NEW "ShadCube4 trace.log",
                           DATA_DIR_OLD "ShadCube4 trace.log",
                           "ShadCube4 trace.log",
                           "/mnt/sandbox/SHAD00004/data/ShadCube4 trace.log",
                           0};
    for (int i=0; paths[i]; i++){
        int fd = sceKernelOpen(paths[i], 0x601, 0x1FF);
        if (fd >= 0){ g_log_fd = fd; return; }
    }
}
/* trace_line: write one already-built line and fsync immediately. */
static void trace_line(const char *buf, unsigned long n){
    if (g_log_fd < 0) return;
    sceKernelWrite(g_log_fd, buf, n);
    sceKernelFsync(g_log_fd);
}
/* trace_msg: write a plain string + fsync. */
static void trace_msg(const char *s){ trace_line(s, lg_len(s)); }

/* One "stat" trace line over EVERY frame of a window (the per-frame lines are sampled). */
static void trace_stat(long long frame, long long n, long long dtmax, long long slow,
                       long long miss, long long flips, long long vbl) {
    static const char* k[7] = {"stat f=", " n=", " dtmax=", " slow=", " miss=", " flips=", " vbl="};
    long long v[7] = {frame, n, dtmax, slow, miss, flips, vbl};
    char L[192];
    int p = 0;
    for (int i = 0; i < 7; i++) {
        for (const char* q = k[i]; *q; q++)
            L[p++] = *q;
        p += lg_i64(L + p, v[i]);
    }
    L[p++] = '\n';
    trace_line(L, p);
}

/* GPU-visible memory of a PS4 direct-memory type: MEM_TYPE_ONION (0: WB, CPU-GPU coherent) for
   command buffers, fences and anything the CPU polls; MEM_TYPE_GARLIC (3: WC, not CPU-coherent,
   high GPU bandwidth) for render targets, textures and vertices. */
static void* gpu_alloc_typed(unsigned long size, unsigned long align, int memtype);
/* ==== §4 Memory: CPU / GPU allocation, reports ================================================ */
/* CPU-read data (cached, coherent): the background-music PCM. */
static void* cpu_alloc(unsigned long size, unsigned long align) {
    return gpu_alloc_typed(size, align, MEM_TYPE_ONION);
}
/* Direct memory: libkernel (shadPS4 memory.cpp: sceKernelGetDirectMemorySize() -> u64;
   sceKernelAvailableDirectMemorySize(start, end, align, u64* phys, u64* size), NID C0f7TJcbfac).
   The OpenOrbis header declares the two out-params as values, so these prototypes are ours. */
extern uint64_t sceKernelGetDirectMemorySize(void);
extern int32_t sceKernelAvailableDirectMemorySize(uint64_t start, uint64_t end, uint64_t align,
                                                  uint64_t* phys_out, uint64_t* size_out);
static unsigned long g_dmem_bytes; /* direct memory allocated through gpu_alloc_typed */

static void dmem_trace(const char* what, unsigned long a, unsigned long b, int memtype, int ret) {
    char L[200];
    int p = 0;
    const char* m = what;
    while (*m)
        L[p++] = *m++;
    p += lg_hex(L + p, a);
    L[p++] = ' ';
    p += lg_hex(L + p, b);
    const char* t = " type ";
    while (*t)
        L[p++] = *t++;
    p += lg_i64(L + p, memtype);
    t = " ret ";
    while (*t)
        L[p++] = *t++;
    p += lg_hex(L + p, (unsigned long long)(unsigned)ret);
    t = " ours ";
    while (*t)
        L[p++] = *t++;
    p += lg_hex(L + p, g_dmem_bytes);
    L[p++] = '\n';
    trace_line(L, (unsigned long)p);
}

/* One line: total direct memory, largest free block, and what we allocated so far. */
static void mem_report(const char* tag) {
    uint64_t phys = 0, largest = 0;
    uint64_t total = sceKernelGetDirectMemorySize();
    int32_t r = sceKernelAvailableDirectMemorySize(0, total, 0x4000, &phys, &largest);
    char L[200];
    int p = 0;
    const char* m = "mem ";
    while (*m)
        L[p++] = *m++;
    while (*tag && p < 60)
        L[p++] = *tag++;
    m = ": total ";
    while (*m)
        L[p++] = *m++;
    p += lg_hex(L + p, total);
    m = " largest free ";
    while (*m)
        L[p++] = *m++;
    p += lg_hex(L + p, largest);
    m = " ret ";
    while (*m)
        L[p++] = *m++;
    p += lg_hex(L + p, (unsigned long long)(unsigned)r);
    m = " ours ";
    while (*m)
        L[p++] = *m++;
    p += lg_hex(L + p, g_dmem_bytes);
    L[p++] = '\n';
    trace_line(L, (unsigned long)p);
}

static void *gpu_alloc_typed(unsigned long size, unsigned long align, int memtype) {
    unsigned long phys = 0; void *addr = 0;   /* matches sceKernelAllocateDirectMemory's unsigned long* out-param */
    size = (size + 0x3FFF) & ~0x3FFFUL;
    if (align < 0x4000) align = 0x4000;
    int r = sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, memtype, &phys);
    if (r) {
        dmem_trace("ALLOC FAILED (allocate) size/align ", size, align, memtype, r);
        return 0;
    }
    r = sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW | PROT_GPU_RW, 0, phys, align);
    if (r) {
        dmem_trace("ALLOC FAILED (map) size/align ", size, align, memtype, r);
        return 0;
    }
    my_memset(addr, 0, size);
    g_dmem_bytes += size;
    return addr;
}

static void *gpu_alloc(unsigned long size, unsigned long align) {
    return gpu_alloc_typed(size, align, MEM_TYPE_GARLIC);
}

/* ==== §5 Math ================================================================================= */
/* sin: reduce to [-pi, pi], fold into [-pi/2, pi/2] with sin(pi - x) = sin(x); the series to x^11
   is then within (pi/2)^13 / 13! = 5.7e-8. */
static float my_sin(float x) {
    const float PI = 3.14159265358979f, TWO_PI = 6.28318530717959f;
    while (x > PI)
        x -= TWO_PI;
    while (x < -PI)
        x += TWO_PI;
    if (x > 0.5f * PI)
        x = PI - x;
    else if (x < -0.5f * PI)
        x = -PI - x;
    float x2 = x * x;
    return x * (1.0f -
                x2 / 6.0f *
                    (1.0f - x2 / 20.0f *
                                (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f * (1.0f - x2 / 110.0f)))));
}
static float my_cos(float x) { return my_sin(x + 1.57079632679490f); }

/* log2 for init-time constants (no libm): exponent from the float bits,
   mantissa m in [1, 2) through ln m = 2 atanh(s), s = (m - 1) / (m + 1) < 1/3,
   six odd terms. Positive normal floats only. */
static float my_log2(float x) {
    union {
        float f;
        uint32_t u;
    } v = {x};
    int e = (int)((v.u >> 23) & 0xFFu) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;
    float s = (v.f - 1.0f) / (v.f + 1.0f), s2 = s * s;
    float t = 1.0f / 11.0f;
    t = 1.0f / 9.0f + s2 * t;
    t = 1.0f / 7.0f + s2 * t;
    t = 1.0f / 5.0f + s2 * t;
    t = 1.0f / 3.0f + s2 * t;
    t = 1.0f + s2 * t;
    return (float)e + 2.0f * s * t * 1.44269504f;
}

/* Exact square root (sqrtss). */
static float my_sqrt(float x) {
    if (x <= 0.0f)
        return 0.0f;
    __asm__("sqrtss %1, %0" : "=x"(x) : "x"(x));
    return x;
}

// === Geometry (static) ===
static const float cube_pos[8][3] = {
    {-.4f,-.4f,.4f},{.4f,-.4f,.4f},{.4f,.4f,.4f},{-.4f,.4f,.4f},
    {-.4f,-.4f,-.4f},{.4f,-.4f,-.4f},{.4f,.4f,-.4f},{-.4f,.4f,-.4f},
};
static const int face_idx[12][3] = {
    {0,1,2},{0,2,3},{5,4,7},{5,7,6},{4,0,3},{4,3,7},
    {1,5,6},{1,6,2},{3,2,6},{3,6,7},{4,5,1},{4,1,0},
};
static const float uv_corners[4][2] = {{0,1},{1,1},{1,0},{0,0}};
static const int tri_uv[2][3] = {{0,1,2},{0,2,3}};

/* ==== §6 Static geometry (sky quad, cube, floor), lens flare occlusion ======================== */
// Build the static vertex buffer (sky quad, cube, floor).
static void build_static_vb(float* vb) {
    // Identity matrix at offset 0
    float *id = (float*)((char*)vb + IDENT_OFF);
    my_memset(id, 0, 64);
    id[0]=1; id[5]=1; id[10]=1; id[15]=1;

    // BG sun slot at offset 64 (VS reads this as "sun" for BG draw — ignored, zero normals)
    float *bgsun = (float*)((char*)vb + BG_SUN_OFF);
    bgsun[0]=0.20f; bgsun[1]=0.50f; bgsun[2]=0.67f; bgsun[3]=0.0f;

    // BG verts at offset 80 (after identity + bg_sun)
    float *bg = (float*)((char*)vb + BG_DATA_OFF);
    float bp[6][4]={{-1,-1,.999f,1},{1,-1,.999f,1},{1,1,.999f,1},
                     {-1,-1,.999f,1},{1,1,.999f,1},{-1,1,.999f,1}};
    for (int v=0;v<6;v++) {
        int i=v*12;
        bg[i]=bp[v][0];bg[i+1]=bp[v][1];bg[i+2]=bp[v][2];bg[i+3]=bp[v][3];
        bg[i+4]=0.0f;bg[i+5]=0.0f;bg[i+6]=0.0f;bg[i+7]=0.0f; /* zero normal = no lighting */
        /* attr0 = (clip_x * aspect, clip_y): ps_dark uses attr0.x ONLY for the
           sun distance (dword 35, v_sub_f32 v26, v2, v19), so scaling it and the
           sun's x by the aspect ratio makes the disc round in pixels. */
        bg[i+8]=bp[v][0]*((float)DISPLAY_W/(float)DISPLAY_H); bg[i+9]=bp[v][1]; bg[i+10]=0.0f; bg[i+11]=0.0f;
    }

    // Cube verts at offset 320 (after MVP slot)
    float *cb = (float*)((char*)vb + CUBE_DATA_OFF);
    static const float face_normals[6][3] = {
        {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
    };
    for (int tri=0;tri<12;tri++) {
        int face=tri/2,t=tri%2;
        for (int v=0;v<3;v++) {
            int vi=face_idx[tri][v],ui=tri_uv[t][v];
            int idx=(tri*3+v)*12;
            cb[idx]=cube_pos[vi][0]; cb[idx+1]=cube_pos[vi][1];
            cb[idx+2]=cube_pos[vi][2]; cb[idx+3]=1.0f;
            cb[idx+4]=face_normals[face][0]; cb[idx+5]=face_normals[face][1];
            cb[idx+6]=face_normals[face][2]; cb[idx+7]=0.0f;
            cb[idx+8]=uv_corners[ui][0]; cb[idx+9]=uv_corners[ui][1];
            cb[idx+10]=0.0f; cb[idx+11]=0.0f;
        }
    }

    /* Floor: FLOOR_GRID x FLOOR_GRID quads over +-FLOOR_HALF in XZ, curving
       down toward the horizon: y = Y_BASE - (x^2 + z^2) / (2R), normal =
       normalize(x/R, 1, z/R). UV = (x + HALF, z + HALF) * FLOOR_UV_MAX / (2 HALF),
       so u runs along +x and v along +z (the tangent frame ps_floor uses). Surface
       relief comes from parallax occlusion mapping in ps_floor: a vertex grid
       (2 * FLOOR_HALF / FLOOR_GRID units apart) cannot carry texture-scale height
       detail. */
    {
        float *fp = (float*)((char*)vb + FLOOR_DATA_OFF);
        const float Y_BASE = -0.5f;
        const float R = FLOOR_R;
        const float STEP = (2.0f * FLOOR_HALF) / (float)FLOOR_GRID;
        const float UV_STEP = FLOOR_UV_MAX / (float)FLOOR_GRID;
        int v = 0;
        for (int gz = 0; gz < FLOOR_GRID; gz++) {
            for (int gx = 0; gx < FLOOR_GRID; gx++) {
                float x0 = -FLOOR_HALF + (float)gx * STEP, x1 = x0 + STEP;
                float z0 = -FLOOR_HALF + (float)gz * STEP, z1 = z0 + STEP;
                /* UVs centred on the origin (0 at x = z = 0; the tiling is unchanged, HALF / 4 is
                   a whole number of tiles): full float precision where the camera looks, not 0.25
                   texel steps around u = 750 */
                float u0 = (float)(gx - FLOOR_GRID / 2) * UV_STEP, u1 = u0 + UV_STEP;
                float t0 = (float)(gz - FLOOR_GRID / 2) * UV_STEP, t1 = t0 + UV_STEP;
                /* corners A (x0,z0), B (x1,z0), C (x1,z1), D (x0,z1); tris A C B, A D C */
                const float cx[4] = {x0, x1, x1, x0}, cz[4] = {z0, z0, z1, z1};
                const float cu[4] = {u0, u1, u1, u0}, cv[4] = {t0, t0, t1, t1};
                static const int order[6] = {0, 2, 1, 0, 3, 2};
                for (int i = 0; i < 6; i++) {
                    int k = order[i];
                    float x = cx[k], z = cz[k];
                    float nx = x / R, nz = z / R;
                    float nl = my_sqrt(nx * nx + 1.0f + nz * nz);
                    float* o = fp + (unsigned long)v * 12;
                    o[0] = x;
                    o[1] = Y_BASE - (x * x + z * z) / (2.0f * R);
                    o[2] = z;
                    o[3] = 1.0f;
                    o[4] = nx / nl;
                    o[5] = 1.0f / nl;
                    o[6] = nz / nl;
                    o[7] = 0.0f;
                    o[8] = cu[k];
                    o[9] = cv[k];
                    o[10] = 0.0f;
                    o[11] = 0.0f;
                    v++;
                }
            }
        }
    }
}

/* Lens flare occlusion: does the ray o + t d (t > 0) hit the floor or the prop?
   Floor as generated: y = -0.5 - (x^2 + z^2) / (2 FLOOR_R) over +-FLOOR_HALF, so
   g(t) = c + b t + a t^2 with a = (dx^2 + dz^2) / (2 FLOOR_R), b = dy + (ox dx + oz dz) /
   FLOOR_R, c = oy + 0.5 + (ox^2 + oz^2) / (2 FLOOR_R) (a ~ 1e-5: roots in the stable form
   q = -(b + sign(b) sqrt(b^2 - 4ac)) / 2, t = q / a, c / q). Prop: the ray in
   model space (A^-1 (o - t), A^-1 d, A = the 3x3 of g_prop_m) against the bounds
   (slabs). */
static int flare_ray_blocked(const float* o, const float* d) {
    float a = (d[0] * d[0] + d[2] * d[2]) / (2.0f * FLOOR_R);
    float b = d[1] + (o[0] * d[0] + o[2] * d[2]) / FLOOR_R;
    float c = o[1] + 0.5f + (o[0] * o[0] + o[2] * o[2]) / (2.0f * FLOOR_R);
    float disc = b * b - 4.0f * a * c;
    if (disc >= 0.0f) {
        float q = -0.5f * (b + (b >= 0.0f ? my_sqrt(disc) : -my_sqrt(disc)));
        float r[2] = {a > 0.0f ? q / a : -1.0f, q != 0.0f ? c / q : -1.0f};
        for (int k = 0; k < 2; k++) {
            if (r[k] <= 0.0f)
                continue;
            float x = o[0] + r[k] * d[0], z = o[2] + r[k] * d[2];
            if (x >= -FLOOR_HALF && x <= FLOOR_HALF && z >= -FLOOR_HALF && z <= FLOOR_HALF)
                return 1;
        }
    }
    const float* m = g_prop_m;
    float c00 = m[5] * m[10] - m[6] * m[9], c01 = m[6] * m[8] - m[4] * m[10];
    float c02 = m[4] * m[9] - m[5] * m[8];
    float det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    if (det == 0.0f)
        return 0;
    float inv[9] = {c00, m[2] * m[9] - m[1] * m[10], m[1] * m[6] - m[2] * m[5],
                    c01, m[0] * m[10] - m[2] * m[8], m[2] * m[4] - m[0] * m[6],
                    c02, m[1] * m[8] - m[0] * m[9],  m[0] * m[5] - m[1] * m[4]};
    float p[3] = {o[0] - m[3], o[1] - m[7], o[2] - m[11]}, po[3], dm[3];
    for (int k = 0; k < 3; k++) {
        po[k] = (inv[k * 3] * p[0] + inv[k * 3 + 1] * p[1] + inv[k * 3 + 2] * p[2]) / det;
        dm[k] = (inv[k * 3] * d[0] + inv[k * 3 + 1] * d[1] + inv[k * 3 + 2] * d[2]) / det;
    }
    float t0 = 0.0f, t1 = 1e30f;
    for (int k = 0; k < 3; k++) {
        if (dm[k] == 0.0f) {
            if (po[k] < g_prop_lo[k] || po[k] > g_prop_hi[k])
                return 0;
            continue;
        }
        float ta = (g_prop_lo[k] - po[k]) / dm[k], tb = (g_prop_hi[k] - po[k]) / dm[k];
        if (ta > tb) {
            float tt = ta;
            ta = tb;
            tb = tt;
        }
        if (ta > t0)
            t0 = ta;
        if (tb < t1)
            t1 = tb;
        if (t0 > t1)
            return 0;
    }
    return 1;
}

/* Visible fraction of the sun disc: 29 rays from the camera through a 7 x 7 grid
   inside the disc (SUN_DISC_RADIUS_PX around NDC (nx, ny)), weighted by ps_dark's
   limb-darkened disc profile mu^alpha. Rays from build_mvp's basis: d = f + r nx aspect /
   fov + u ny / fov. */
static float flare_visibility(float yaw, float pitch, const float* cam, float nx, float ny) {
    float sy = my_sin(yaw), cy = my_cos(yaw), sp = my_sin(pitch), cp = my_cos(pitch);
    const float f[3] = {sy * cp, -sp, -cy * cp}, r[3] = {cy, 0.0f, sy},
                u[3] = {sy * sp, cp, -cy * sp};
    float aspect = (float)DISPLAY_W / (float)DISPLAY_H;
    float fov = my_cos(0.3054f) / my_sin(0.3054f);
    float rn = SUN_DISC_RADIUS_PX / ((float)DISPLAY_H * 0.5f);
    float wsum = 0.0f, vis = 0.0f;
    for (int j = -3; j <= 3; j++)
        for (int i = -3; i <= 3; i++) {
            int q = i * i + j * j;
            if (q >= 9)
                continue;
            /* ps_dark's disc profile mu^alpha (green, ATMO_LIMB_G), mu^2 = 1 - q / 9 */
            static const float kw[9] = {1.0000000f, 0.9705313f, 0.9381715f, 0.0f,      0.8613353f,
                                        0.8138821f, 0.0f,       0.0f,       0.5723548f};
            float w = kw[q];
            float px = nx + (float)i / 3.0f * rn / aspect, py = ny + (float)j / 3.0f * rn;
            float d[3];
            for (int k = 0; k < 3; k++)
                d[k] = f[k] + r[k] * px * aspect / fov + u[k] * py / fov;
            wsum += w;
            if (!flare_ray_blocked(cam, d))
                vis += w;
        }
    return vis / wsum;
}

/* Lens flare table (ps_post_final dwords 28..39) from the sun disc desc[16..19], its colour x
   SUN_HDR desc[84..86] and its visible fraction. The glow, veil and rays are the lens's response
   to the sun wherever it is in front of the camera, on or off screen (their profiles fall off on
   their own and glare.dds has a zero border): strength = visible fraction. Only the ghosts
   (reflections along the lens axis) fade out over FLARE_EDGE at the screen edges (0 off screen).
   The tint follows the sun colour at the daytime sun's luminance (Rec. 709 weights of the linear
   sRGB primaries), so the glow turns amber at sunset instead of losing half its brightness. */
static float srgb_to_linear(float c); /* defined with the texture loaders */
static void flare_consts(float* ft, const float* sd, const float* scl, float vis) {
    float nx = sd[0] * ((float)DISPLAY_H / (float)DISPLAY_W), ny = sd[1];
    float fu = 0.5f + 0.5f * nx, fv = 0.5f - 0.5f * ny;
    float e = fu < 1.0f - fu ? fu : 1.0f - fu; /* distance to the nearest edge, < 0 off screen */
    if (fv < e)
        e = fv;
    if (1.0f - fv < e)
        e = 1.0f - fv;
    float ghost = 0.0f;
    if (e > 0.0f) {
        float t = e / FLARE_EDGE;
        if (t > 1.0f)
            t = 1.0f;
        ghost = t * t * (3.0f - 2.0f * t);
    }
    float k = 0.0f;
    if (sd[0] < 90.0f) { /* 99: behind the camera */
        float ld = 0.2126f * srgb_to_linear(SUN_DAY_R) + 0.7152f * srgb_to_linear(SUN_DAY_G) +
                   0.0722f * srgb_to_linear(SUN_DAY_B);
        float ls = (0.2126f * scl[0] + 0.7152f * scl[1] + 0.0722f * scl[2]) / SUN_HDR;
        if (ls > 0.0f)
            k = FLARE_STRENGTH * vis * ld / (ls * SUN_HDR);
    }
    ft[0] = fu;
    ft[1] = fv;
    ft[2] = k * scl[0];
    ft[3] = k * scl[1];
    ft[4] = k * scl[2];
    ft[5] = FLARE_GHOSTS * ghost; /* dwords 33..36: gains; 37..39 spare; 40..51 glare T# / S# */
    ft[6] = FLARE_RAYS * GLARE_STORE_MAX;
    ft[7] = FLARE_GLOW;
    ft[8] = FLARE_VEIL;
    ft[9] = 0.0f;
    ft[10] = 0.0f;
    ft[11] = 0.0f;
}

/* ==== §7 Camera matrix (MVP) ================================================================== */
/* FPS free-fly camera: position + yaw/pitch → view-projection matrix */
static void build_mvp(float *mvp, float yaw, float pitch,
                      float cx, float cy, float cz) {
    float sy=my_sin(yaw),  cy2=my_cos(yaw);
    float sp=my_sin(pitch),cp=my_cos(pitch);

    /* Camera basis vectors */
    float fx= sy*cp, fy=-sp, fz=-cy2*cp;  /* forward (into screen) */
    float rx= cy2,   ry= 0,  rz= sy;      /* right */
    float ux= sy*sp, uy= cp, uz=-cy2*sp;  /* up */

    /* View matrix: rotate then translate */
    float v[4][4]={
        { rx, ry, rz, -(rx*cx + ry*cy + rz*cz) },
        { ux, uy, uz, -(ux*cx + uy*cy + uz*cz) },
        {-fx,-fy,-fz,  (fx*cx + fy*cy + fz*cz) },
        {  0,  0,  0,  1 }
    };

    /* Perspective projection: 35° FOV telephoto.
       Produces NDC.z in [0, 1] (DirectX/Vulkan native / ZeroToW convention) so we
       don't rely on VK_EXT_depth_clip_control. Must be paired with
       CTX_CLIPPER_CONTROL.clip_space = 1 (ZeroToW, bit 19).
       View convention: -Z is forward. For view.z = -near, NDC.z = 0. For view.z = -far, NDC.z = 1.
       Derived from:  clip.z = P22*vz + P23,  clip.w = -vz,  NDC.z = clip.z/clip.w.
         P22 = -fa / (fa - n),  P23 = -n*fa / (fa - n),  P[3][2] = -1. */
    float aspect=(float)DISPLAY_W/(float)DISPLAY_H;
    float fov=my_cos(0.3054f)/my_sin(0.3054f);
    float n = CAM_NEAR, fa = CAM_FAR;
    float p[4][4]={
        {fov/aspect, 0, 0, 0},
        {0, fov, 0, 0},
        {0, 0, -fa/(fa-n), -n*fa/(fa-n)},
        {0, 0, -1, 0}
    };

    /* MVP = P × V */
    for (int i=0;i<4;i++)
        for (int j=0;j<4;j++) {
            float s=0;
            for (int k=0;k<4;k++) s+=p[i][k]*v[k][j];
            mvp[i*4+j]=s;
        }
}

/* ==== §8 Resource descriptors (V# T# S#), star field ========================================== */
// === Descriptor builders ===
/* Build a buffer descriptor (V#).

   STRIDE IS ZERO HERE, and that matters on this hardware. From radv's vertex
   buffer setup:
       desc[1] = BASE_ADDRESS_HI(va >> 32) | STRIDE(stride);
       if (chip_class <= CIK && stride)
           desc[2] = (size - offset) / stride;   // NUM_RECORDS IN ELEMENTS
       else
           desc[2] = size - offset;              // in BYTES
   Liverpool is CIK, so on a STRIDED buffer num_records would have to be in
   units of stride, not bytes. The quirk is guarded on stride != 0, and ours is
   0 - our shaders fetch with computed byte offsets rather than a strided
   vertex-fetch descriptor - so num_records in bytes is correct as written.

   If anyone ever gives this descriptor a non-zero stride, num_records MUST be
   divided by it or every fetch runs off the end of the buffer. The assert-like
   branch below keeps that from being silent. */
static void build_vsharp_strided(uint32_t *v, void *base, uint32_t size,
                                 uint32_t stride) {
    uint64_t a = (uint64_t)(uintptr_t)base;
    uint32_t num_records = stride ? (size / stride) : size;   /* CIK: elements */
    v[0]=(uint32_t)a;
    v[1]=((uint32_t)(a>>32)&0xFFFF) | ((stride & 0x3FFF) << 16);
    v[2]=num_records;
    v[3]=(1u<<3)|(2u<<6)|(3u<<9)|(4u<<12)|(4u<<15);
}
/* Stars: STARS_N quads on a sphere of STARS_RADIUS around the origin, upper
   hemisphere only (height 0.03..1, uniform in area), each quad facing the
   origin. ps_stars draws a smooth (1 - r^2)^2 splat over the quad: radius
   2.0-3.0 px keeps a star's total light within ~2% at any sub-pixel position
   (hard 1.5 px quads varied 4x). Brighter stars are bigger. Vertex layout = the
   MVP VS's 48 bytes: pos(x,y,z,1), normal (0, brightness, 0, 0), uv (corner
   -1/+1, 0, 0) -> PARAM0 = (u, v, brightness). Fixed seed. */
#define STARS_N 2500
#define STARS_RADIUS 400.0f
#define STARS_PX 0.2335f /* world units per pixel at STARS_RADIUS: 2*400*tan(0.3054)/1080 */
/* Fade band in orig_sun_y (sun height factor): stars start fading in when the
   sun drops below +STARS_FADE (just before sunset = moonrise), are full at
   -STARS_FADE, and fade out mirrored around sunrise (= moonset). */
#define STARS_FADE 0.12f
static int build_stars(float* out) {
    uint32_t seed = 0x9E3779B9u;
#define STAR_RND() (seed = seed * 1664525u + 1013904223u, (float)(seed >> 8) * (1.0f / 16777216.0f))
    static const int tri[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < STARS_N; i++) {
        float dy = 0.03f + 0.97f * STAR_RND();
        float ph = 6.28318531f * STAR_RND();
        float rr = my_sqrt(1.0f - dy * dy);
        float dx = rr * my_cos(ph), dz = rr * my_sin(ph);
        float t1x = 1.0f, t1z = 0.0f; /* t1 = cross(up, d), t1y = 0 */
        if (dy < 0.99f) {
            float l = my_sqrt(dz * dz + dx * dx);
            t1x = dz / l;
            t1z = -dx / l;
        }
        float t2x = dy * t1z, t2y = dz * t1x - dx * t1z, t2z = -dy * t1x; /* cross(d, t1) */
        float u = STAR_RND();
        float u3 = u * u * u;                     /* mostly faint, a few bright */
        float sz = (2.0f + 1.0f * u3) * STARS_PX; /* splat radius 2.0-3.0 px */
        float bright = 0.35f + 0.65f * u3;
        float cx = dx * STARS_RADIUS, cy = dy * STARS_RADIUS, cz = dz * STARS_RADIUS;
        float c[4][3];
        for (int k = 0; k < 4; k++) {
            float a = (k == 1 || k == 2) ? sz : -sz, bb = (k >= 2) ? sz : -sz;
            c[k][0] = cx + t1x * a + t2x * bb;
            c[k][1] = cy + t2y * bb;
            c[k][2] = cz + t1z * a + t2z * bb;
        }
        for (int v = 0; v < 6; v++) {
            float* o = out + (i * 6 + v) * 12;
            o[0] = c[tri[v]][0];
            o[1] = c[tri[v]][1];
            o[2] = c[tri[v]][2];
            o[3] = 1.0f;
            for (int q = 4; q < 12; q++)
                o[q] = 0.0f;
            o[5] = bright;                                      /* normal.y */
            o[8] = (tri[v] == 1 || tri[v] == 2) ? 1.0f : -1.0f; /* uv: corner */
            o[9] = (tri[v] >= 2) ? 1.0f : -1.0f;
        }
    }
#undef STAR_RND
    return STARS_N;
}

static void build_vsharp(uint32_t *v, void *base, uint32_t size) {
    build_vsharp_strided(v, base, size, 0);
}
/* levels > 1: a mip chain in LINEAR_ALIGNED layout (see dds_loader.h). SQ_IMG_RSRC_WORD3
   (gfx_7_2_sh_mask.h): LAST_LEVEL @16, POW2_PAD @25 - radeonsi sets POW2_PAD(last_level > 0) on
   GFX6-8. */
static void build_tsharp_levels(uint32_t *t, void *tex, int w, int h, int levels) {
    uint64_t a=(uint64_t)(uintptr_t)tex;
    my_memset(t,0,32);
    t[0]=(uint32_t)(a>>8); t[1]=(uint32_t)(a>>40)|(10u<<20);
    t[2]=(uint32_t)(w-1)|((uint32_t)(h-1)<<14);
    t[3]=4u|(5u<<3)|(6u<<6)|(7u<<9)|(8u<<20)|(9u<<28);
    if (levels > 1) t[3] |= ((uint32_t)(levels - 1) << 16) | (1u << 25);
    t[4]=(uint32_t)(w-1)<<13;
}
static void build_tsharp(uint32_t *t, void *tex, int w, int h) {
    build_tsharp_levels(t, tex, w, h, 1);
}

/* Anisotropic-filtering sampler (16:1) for the floor's albedo and normal
   map. AF eliminates the blurred / smeared look at glancing angles when
   the floor stretches into the distance.

   Per AMD GCN3 ISA Vol II Sec 8.2 sampler S# layout:
     dword0 [11:9]  MAX_ANISO_RATIO (0=1:1, 1=2:1, 2=4:1, 3=8:1, 4=16:1)
     dword2 [21:20] XY_MAG_FILTER (1=Linear, 3=Aniso_Linear)
     dword2 [23:22] XY_MIN_FILTER (same enum)

   Setting MAX_ANISO_RATIO=4 + filter mode = Aniso_Linear gives the
   driver freedom to use up to 16 samples per anisotropic kernel along
   the dominant axis for best quality. */
static void build_ssharp_aniso(uint32_t *s) {
    my_memset(s, 0, 16);
    /* Field-for-field what radeonsi builds on GFX6/7 for 16x aniso
       (si_create_sampler_state; positions from gfx_7_2_sh_mask.h):
       dword0: clamp_x/y/z=Wrap(0); MAX_ANISO_RATIO=4 (16:1) @9;
               ANISO_THRESHOLD = ratio>>1 = 2 @16; ANISO_BIAS = ratio = 4 @21 */
    s[0] = (4u << 9) | (2u << 16) | (4u << 21);
    /* dword1: max_lod=0xF00 (15.0) @12; PERF_MIP = ratio+6 = 10 @24 */
    s[1] = (0xF00u << 12) | (10u << 24);
    /* dword2: xy_mag_filter=Aniso_Linear(3) at [21:20],
               xy_min_filter=Aniso_Linear(3) at [23:22],
               mip_filter=Linear(2) at [27:26] (SQ_TEX_Z_FILTER_LINEAR; radeonsi
               uses the Z_FILTER enum for MIP_FILTER). Single-level textures
               are unaffected: the T# bounds the levels. */
    /* + DISABLE_LSB_CEIL (<= VI) @29 and FILTER_PREC_FIX @30, as radeonsi. */
    s[2] = (3u << 20) | (3u << 22) | (2u << 26) | (1u << 29) | (1u << 30);
}

/* Shadow-map sampler at desc[80..83] for the floor PS (CAFE100F).

   The floor PS uses plain IMAGE_SAMPLE (not _C_LZ) so depth_compare_func
   is unused — kept at LessEqual for documentation. The compare is done
   manually in the shader via V_CMP_LT_F32 + V_CNDMASK_B32.

   Semantics that ARE used:
     - clamp_x/y/z = 6 (ClampBorder): out-of-frustum UVs hit the border
     - border_color_type = 2 (White) at raw1 bits [62..63]: samples at out-of-
       frustum UVs return 1.0 in R. Then the manual compare (stored < z_ref)
       with z_ref ∈ [0,1] is FALSE → fragment is lit. Out-of-frustum floor
       areas correctly stay lit.
     - xy_mag/min_filter = 0 (Point): the shaders read the 4 texel centres and do
       bilinear PCF themselves (see build_ssharp_pcf)
     - max_lod = 15.0 (0xF00 in u4.8): allow any mip level (shadow map
       has only level 0 anyway).

   Hardware bit layout per AMD Sea Islands ISA Table 8.12, verified against
   shadPS4/src/video_core/amdgpu/resource.h Sampler struct. */
static void build_ssharp_pcf(uint32_t *s) {
    my_memset(s,0,16);
    /* raw0 low (dword0): clamp_x/y/z = ClampBorder(6) at bits [0..8],
       depth_compare_func = LessEqual(3) at bits [12..14] (unused but harmless) */
    s[0] = 6u | (6u << 3) | (6u << 6) | (3u << 12);
    /* raw0 high (dword1): max_lod=0xF00 (15.0) at bits [12..23] */
    s[1] = (0xF00u << 12);
    /* raw1 low (dword2): xy_mag / xy_min = Point (0): ps_floor / ps_model do bilinear PCF
       themselves (4 texel-centre reads, each compared, then blended); filtering the stored depths
       first would only move the hard edge. + DISABLE_LSB_CEIL, FILTER_PREC_FIX (radeonsi, GFX6/7)
     */
    s[2] = (1u << 29) | (1u << 30);
    /* raw1 high (dword3): border_color_type = White(2) at bits [30..31] */
    s[3] = (2u << 30);
}

/* ==== §9 Colour, sky colours, textures and asset loading ====================================== */
// === DCB builder ===
/* sRGB decode (IEC 61966-2-1): colours picked as sRGB values -> linear light.
   x^2.4 = x^2 * (x^(1/5))^2, fifth root by Newton from 1 (x in (0.04, 1]). */
static float srgb_to_linear(float c) {
    if (c <= 0.04045f)
        return c / 12.92f;
    float x = (c + 0.055f) / 1.055f;
    float y = 1.0f;
    for (int i = 0; i < 8; i++)
        y -= (y * y * y * y * y - x) / (5.0f * y * y * y * y);
    return x * x * y * y;
}

/* Sky zenith / horizon colours (sRGB) from the sun's elevation y (unit
   direction, before the moon swap). Day above 0.3; twilight below, the horizon
   turning orange as the sun sinks. Under the horizon the twilight colours fade
   into the night sky with a smoothstep over -0.2..0 (value and slope continuous
   at both ends); before, the brightest orange horizon switched to night in one
   frame at -0.2. */
static void sky_colours(float y, float* zr, float* zg, float* zb, float* hr, float* hg, float* hb) {
    if (y > 0.3f) {
        *zr = 0.10f;
        *zg = 0.25f;
        *zb = 0.60f;
        *hr = 0.65f;
        *hg = 0.82f;
        *hb = 0.95f;
        return;
    }
    float k = (y + 0.2f) / 0.5f; /* 0 at the night edge, 1 at the day edge */
    if (k < 0.0f)
        k = 0.0f;
    if (k > 1.0f)
        k = 1.0f;
    float warm = 1.0f - k;
    *zr = 0.04f + 0.06f * k;
    *zg = 0.06f + 0.19f * k;
    *zb = 0.18f + 0.42f * k;
    *hr = 0.65f + 0.30f * warm;
    *hg = 0.35f + 0.47f * (1.0f - warm * warm);
    *hb = 0.25f + 0.70f * k;
    if (y < 0.0f) {
        float f = (y + 0.2f) / 0.2f;
        if (f < 0.0f)
            f = 0.0f;
        f = f * f * (3.0f - 2.0f * f);
        const float nz[3] = {0.01f, 0.02f, 0.06f}, nh[3] = {0.03f, 0.04f, 0.10f};
        *zr = nz[0] + (*zr - nz[0]) * f;
        *zg = nz[1] + (*zg - nz[1]) * f;
        *zb = nz[2] + (*zb - nz[2]) * f;
        *hr = nh[0] + (*hr - nh[0]) * f;
        *hg = nh[1] + (*hg - nh[1]) * f;
        *hb = nh[2] + (*hb - nh[2]) * f;
    }
}

/* Model draw (set once a model has loaded): vs_model / vs_model_shadow get the
   model transform M (3 rows of 4 floats) in VS user SGPRs s4..s15 after the V#;
   ps_model draws it. enabled = 0 -> the built-in cube path (vs_shader, ps_shader). */
static struct {
    int enabled;
    const void* vs;
    const void* vs_shadow;
    const void* ps;
    float m[12];
} g_model;

/* Packaged assets: the PKG's assets/ folder is mounted at /app0 (read-only). */
#define ASSET_DIR "/app0/assets/"

/* A texture for one T#: a DDS from the package (tools/make_textures.py), or a
   1x1 RGBA8 fallback texel. */
typedef struct {
    void* pixels;
    int w, h, levels;
    uint32_t dfmt, nfmt; /* SQ_IMG_RSRC_WORD1 DATA_FORMAT / NUM_FORMAT */
    uint32_t tile;       /* SQ_IMG_RSRC_WORD3 TILING_INDEX (dds_loader.h) */
    int err;             /* dds_load result, 0 = loaded */
} Tex;

#include "loadscreen.h"

static Tex load_tex(const char* path, const unsigned char fallback[4], uint32_t fallback_nfmt) {
    Tex t;
    DdsTexture d;
    t.err = dds_load(path, gpu_alloc, &d);
    if (t.err == 0) {
        t.pixels = d.pixels;
        t.w = d.width;
        t.h = d.height;
        t.levels = d.levels;
        t.dfmt = d.data_format;
        t.nfmt = d.num_format;
        t.tile = d.tile_index;
    } else {
        unsigned char* q = (unsigned char*)gpu_alloc(4, 0x1000);
        if (!q) {
            trace_msg("FATAL: texture fallback alloc failed\n");
            t.pixels = 0;
            return t;
        }
        q[0] = fallback[0];
        q[1] = fallback[1];
        q[2] = fallback[2];
        q[3] = fallback[3];
        t.pixels = q;
        t.w = 1;
        t.h = 1;
        t.levels = 1;
        t.dfmt = 0x0A; /* 8_8_8_8 */
        t.nfmt = fallback_nfmt;
        t.tile = 8; /* LINEAR_ALIGNED */
    }
    printf("texture %s: %d (%dx%d, %d levels, format 0x%x/%u)\n", path, t.err, t.w, t.h, t.levels,
           t.dfmt, t.nfmt);
    ls_file(path);
    return t;
}

/* Sky tables (assets/sky/atmosphere.bin, tools/make_atmosphere.c): the 32-byte header and the
   transmittance stay in CPU memory (colours through the air), the three RGBA16F images go to GPU
   memory for ps_dark. Missing file: three zeroed 64 x 64 images - a black sky, never an invalid
   T#. */
static AtmoAsset g_atmo;
static int g_atmo_ok = 0;
static void* g_atmo_atlas = 0;
static int g_atmo_atlas_h = 0; /* texel rows of each image */
static int read_all(int fd, void* dst, unsigned long n) {
    unsigned long got = 0;
    while (got < n) {
        long r = sceKernelRead(fd, (char*)dst + got, n - got);
        if (r <= 0)
            return -1;
        got += (unsigned long)r;
    }
    return 0;
}
static int load_atmosphere(void) {
    const unsigned long head_n = 32ul + (unsigned long)ATMO_T_W * ATMO_T_H * 12;
    const unsigned long img_n = 3ul * ATMO_SKY_W * ATMO_SLICES * ATMO_SKY_H * 8;
    int fd = sceKernelOpen(ASSET_DIR "sky/atmosphere.bin", 0, 0);
    if (fd >= 0) {
        void* head = cpu_alloc(head_n, 0x1000);
        void* atlas = gpu_alloc(img_n, 0x10000);
        void* means = cpu_alloc((unsigned long)ATMO_SLICES * 6 * 4, 0x1000);
        int ok = head && atlas && means && read_all(fd, head, head_n) == 0 &&
                 read_all(fd, atlas, img_n) == 0 &&
                 read_all(fd, means, (unsigned long)ATMO_SLICES * 6 * 4) == 0 &&
                 atmo_asset_bind(&g_atmo, head, atlas, (const float*)means) == 0;
        sceKernelClose(fd);
        if (ok) {
            g_atmo_atlas = atlas;
            g_atmo_atlas_h = ATMO_SLICES * ATMO_SKY_H;
            return 1;
        }
    }
    g_atmo_atlas = gpu_alloc(3ul * 64 * 64 * 8, 0x10000);
    if (g_atmo_atlas)
        my_memset(g_atmo_atlas, 0, 3ul * 64 * 64 * 8);
    g_atmo_atlas_h = 64;
    return 0;
}
/* ==== §10 Camera frame helpers, texture averages, more descriptors ============================ */
/* Camera basis (build_mvp's): forward, right, up. */
static void cam_basis(float yaw, float pitch, float* F, float* R, float* U) {
    float sy = my_sin(yaw), cy = my_cos(yaw), sp = my_sin(pitch), cp = my_cos(pitch);
    F[0] = sy * cp, F[1] = -sp, F[2] = -cy * cp;
    R[0] = cy, R[1] = 0.0f, R[2] = sy;
    U[0] = sy * sp, U[1] = cp, U[2] = -cy * sp;
}

/* The camera above the curved floor: its height hc and the dipped horizon's tilt (sqrt(2 hc / R),
   x / R, z / R) - ps_dark's sky rows, ps_resolve's horizon and the sun / moon disc colours use the
   same. */
static void cam_horizon(float cx, float cy, float cz, float* hc, float tilt[3]) {
    float h = cy + 0.5f + (cx * cx + cz * cz) / (2.0f * FLOOR_R);
    if (h < 0.0f)
        h = 0.0f;
    *hc = h;
    tilt[0] = my_sqrt(2.0f * h / FLOOR_R);
    tilt[1] = cx / FLOOR_R;
    tilt[2] = cz / FLOOR_R;
}
/* The sun direction in the moon's own frame (x right, y up, z toward the viewer = -moon), for
   ps_dark's Lommel-Seeliger lighting of the moon disc. */
static void moon_frame_sun(const float* moon, const float* sun, const float* R, float* out) {
    float z[3] = {-moon[0], -moon[1], -moon[2]}, rz = R[0] * z[0] + R[1] * z[1] + R[2] * z[2];
    float x[3] = {R[0] - rz * z[0], R[1] - rz * z[1], R[2] - rz * z[2]};
    float xl = my_sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    for (int c = 0; c < 3; c++)
        x[c] = xl > 1e-6f ? x[c] / xl : (c == 0 ? 1.0f : 0.0f);
    float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    out[0] = sun[0] * x[0] + sun[1] * x[1] + sun[2] * x[2];
    out[1] = sun[0] * y[0] + sun[1] * y[1] + sun[2] * y[2];
    out[2] = sun[0] * z[0] + sun[1] * z[1] + sun[2] * z[2];
}

/* Linear average of the floor albedo (set at load): the floor colour in ps_model's ground
   reflection. Default = the grey fallback texel (sRGB 128). */
static float g_floor_albedo[3] = {0.2158605f, 0.2158605f, 0.2158605f};

/* Average linear colour of a texture's smallest mip. BC1 (data format 35) as in D3D: RGB565
   endpoints widened by bit replication; c0 > c1: c2 = (2 c0 + c1) / 3, c3 = (c0 + 2 c1) / 3,
   else c2 = (c0 + c1) / 2, c3 transparent black; sRGB decoded after interpolation. Levels are
   stored consecutively (dds_loader.h), so the last level starts after the others' blocks; the
   tiled block order does not matter for an average. A single-level RGBA8 texture (the load
   fallback) averages its texels. Returns 0 if the format is not handled (out unchanged). */
static int tex_average_linear(const Tex* t, float out[3]) {
    const unsigned char* p = (const unsigned char*)t->pixels;
    double acc[3] = {0.0, 0.0, 0.0};
    long n = 0;
    if (!p)
        return 0;
    if (t->dfmt == 35u) {
        int last = t->levels - 1;
        unsigned long off = 0;
        for (int l = 0; l < last; l++) {
            int lw = t->w >> l, lh = t->h >> l;
            lw = lw < 1 ? 1 : lw;
            lh = lh < 1 ? 1 : lh;
            off += (unsigned long)((lw + 3) / 4) * (unsigned long)((lh + 3) / 4) * 8u;
        }
        int w = t->w >> last, h = t->h >> last;
        w = w < 1 ? 1 : w;
        h = h < 1 ? 1 : h;
        int blocks = ((w + 3) / 4) * ((h + 3) / 4);
        const unsigned char* b = p + off;
        for (int k = 0; k < blocks; k++, b += 8) {
            unsigned c0 = b[0] | ((unsigned)b[1] << 8), c1 = b[2] | ((unsigned)b[3] << 8);
            float pal[4][3];
            const unsigned cc[2] = {c0, c1};
            for (int e = 0; e < 2; e++) {
                unsigned r5 = (cc[e] >> 11) & 31u, g6 = (cc[e] >> 5) & 63u, b5 = cc[e] & 31u;
                pal[e][0] = (float)((r5 << 3) | (r5 >> 2));
                pal[e][1] = (float)((g6 << 2) | (g6 >> 4));
                pal[e][2] = (float)((b5 << 3) | (b5 >> 2));
            }
            for (int c = 0; c < 3; c++) {
                if (c0 > c1) {
                    pal[2][c] = (2.0f * pal[0][c] + pal[1][c]) / 3.0f;
                    pal[3][c] = (pal[0][c] + 2.0f * pal[1][c]) / 3.0f;
                } else {
                    pal[2][c] = 0.5f * (pal[0][c] + pal[1][c]);
                    pal[3][c] = 0.0f;
                }
            }
            float lin[4][3];
            for (int q = 0; q < 4; q++)
                for (int c = 0; c < 3; c++)
                    lin[q][c] = srgb_to_linear(pal[q][c] / 255.0f);
            unsigned idx =
                b[4] | ((unsigned)b[5] << 8) | ((unsigned)b[6] << 16) | ((unsigned)b[7] << 24);
            for (int i = 0; i < 16; i++, n++) {
                unsigned q = (idx >> (2 * i)) & 3u;
                for (int c = 0; c < 3; c++)
                    acc[c] += lin[q][c];
            }
        }
    } else if (t->dfmt == 0x0Au && t->levels == 1) {
        for (long i = 0; i < (long)t->w * t->h; i++, n++)
            for (int c = 0; c < 3; c++)
                acc[c] += srgb_to_linear((float)p[i * 4 + c] / 255.0f);
    }
    if (n == 0)
        return 0;
    for (int c = 0; c < 3; c++)
        out[c] = (float)(acc[c] / (double)n);
    return 1;
}

/* T# for a Tex: build_tsharp_levels with the texture's DATA_FORMAT (word1 25:20),
   NUM_FORMAT (word1 29:26) and TILING_INDEX (word3 24:20). */
static void build_tsharp_tex(uint32_t* t, const Tex* x) {
    build_tsharp_levels(t, x->pixels, x->w, x->h, x->levels);
    t[1] = (t[1] & ~((0x3Fu << 20) | (0xFu << 26))) | (x->dfmt << 20) | (x->nfmt << 26);
    t[3] = (t[3] & ~(0x1Fu << 20)) | (x->tile << 20);
}

/* Height-map sampler: wrap, bilinear, linear mips (MIP_FILTER 2 @ word2[27:26]). */
static void build_ssharp_height(uint32_t* s) {
    my_memset(s, 0, 16);
    s[1] = 0xF00u << 12; /* MAX_LOD 15.0 */
    s[2] = (1u << 20) | (1u << 22) | (2u << 26);
}

/* RGBA16F render target as a texture: 16_16_16_16 / FLOAT, linear aligned
   (TILING_INDEX 8, like the shadow map), 2D. */
static void build_tsharp_f16(uint32_t* t, void* tex, int w, int h, int pitch) {
    uint64_t a = (uint64_t)(uintptr_t)tex;
    my_memset(t, 0, 32);
    t[0] = (uint32_t)(a >> 8);
    t[1] = (uint32_t)(a >> 40) | (0xCu << 20) | (7u << 26);
    t[2] = (uint32_t)(w - 1) | ((uint32_t)(h - 1) << 14);
    t[3] = 4u | (5u << 3) | (6u << 6) | (7u << 9) | (8u << 20) | (9u << 28);
    t[4] = (uint32_t)(pitch - 1) << 13;
}
/* RG8 UNORM, linear aligned (clock mode's text distance: R = 128 + 8 d, G = the group). */
static void build_tsharp_rg8(uint32_t* t, const void* tex, int w, int h, int pitch) {
    uint64_t a = (uint64_t)(uintptr_t)tex;
    my_memset(t, 0, 32);
    t[0] = (uint32_t)(a >> 8);
    t[1] = (uint32_t)(a >> 40) | (3u << 20) | (0u << 26); /* 8_8 | UNORM */
    t[2] = (uint32_t)(w - 1) | ((uint32_t)(h - 1) << 14);
    t[3] = 4u | (5u << 3) | (4u << 6) | (5u << 9) | (8u << 20) | (9u << 28);
    t[4] = (uint32_t)(pitch - 1) << 13;
}

/* The 4-sample scene depth as a texture (ps_resolve): Z_32_FLOAT written by the DB with
   DB_DEPTH_INFO ARRAY_1D_TILED_THIN1 (depth micro tiles) = PS4 tile index 5 Depth1DThin (shadPS4
   tiling.h: ArrayMode 1D, MicroTileMode Depth); IMG_DATA_FORMAT_32 / FLOAT (gfx_7_2_enum.h), TYPE
   2D_MSAA, LAST_LEVEL log2(4) (PAL gfx6Device.cpp); no FMASK / HTILE. */
static void build_tsharp_depth_msaa(uint32_t* t, void* base, int w, int h) {
    uint64_t a = (uint64_t)(uintptr_t)base;
    my_memset(t, 0, 32);
    t[0] = (uint32_t)(a >> 8);
    t[1] = (uint32_t)(a >> 40) | (4u << 20) | (7u << 26);
    t[2] = (uint32_t)(w - 1) | ((uint32_t)(h - 1) << 14);
    t[3] = 4u | (4u << 3) | (4u << 6) | (4u << 9) | (2u << 16) | (5u << 20) | (0xEu << 28);
    t[4] = (uint32_t)(w - 1) << 13;
}

/* Clamp-to-edge sampler (CLAMP_LAST_TEXEL x/y/z), point or bilinear, LOD 0. */
static void build_ssharp_clamp(uint32_t* s, int bilinear) {
    my_memset(s, 0, 16);
    s[0] = 2u | (2u << 3) | (2u << 6);
    s[2] = bilinear ? ((1u << 20) | (1u << 22)) : 0u;
}

/* Pass tables, in emit_post order (layouts in the ps_post_* comments):
   0..5   down:   level 0 = bright-pass of the HDR scene, 1..5 = level i-1
   6..17  blur:   level i  H: A -> B, V: B -> A
   18..22 up-add: level i  B = A (point) + bilinear(level i+1: B, or A for 5)
   23     composite: scene (point) + BLOOM_INTENSITY * B0, x EXPOSURE */
static void post_consts(uint32_t* t, float a, float b, float c, float d) {
    float* f = (float*)(t + 12);
    f[0] = a;
    f[1] = b;
    f[2] = c;
    f[3] = d;
}
/* ==== §11 Modules: time sources, UI, glass clock ============================================== */
#include "timesrc.h"
#include "ui.h"
_Static_assert(OPT5_ROWS + OPT5_OPS <= OPT5_RT_W, "OPCODE_TEST 5 / 6: a pixel per slot");
_Static_assert(OPT5_MODES <= OPT5_PASS_MAX, "OPCODE_TEST 5 / 6: a pass per float mode");

/* after timesrc.h / ui.h: the clock uses g_ts */
#include "clock.h" /* glass clock: text, per-frame constants */

/* ==== §12 Post-processing: pass tables, MSAA state, passes ==================================== */
static void build_post_tables(uint32_t* tab) {
    my_memset(tab, 0, POST_TABLE_BLOCKS * 32 * 4);
    uint32_t* t = tab;
    for (int i = 0; i < BLOOM_LEVELS; i++, t += 32) {
        float w = g_bloom_w[i], h = g_bloom_h[i];
        if (i == 0) {
            build_tsharp_f16(t, g_hdr, DISPLAY_W, DISPLAY_H, DISPLAY_W);
            post_consts(t, 1.0f / w, 1.0f / h, 1.0f / DISPLAY_W, 1.0f / DISPLAY_H);
            ((float*)t)[28] = BLOOM_THRESHOLD;
        } else {
            build_tsharp_f16(t, g_bloom_a[i - 1], g_bloom_w[i - 1], g_bloom_h[i - 1],
                             g_bloom_pitch[i - 1]);
            post_consts(t, 1.0f / w, 1.0f / h, 1.0f / g_bloom_w[i - 1], 1.0f / g_bloom_h[i - 1]);
            ((float*)t)[28] = 0.0f; /* no threshold: plain downsample */
        }
        build_ssharp_clamp(t + 8, 1);
    }
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        float w = g_bloom_w[i], h = g_bloom_h[i];
        build_tsharp_f16(t, g_bloom_a[i], w, h, g_bloom_pitch[i]);
        build_ssharp_clamp(t + 8, 1);
        post_consts(t, 1.0f / w, 1.0f / h, 1.0f / w, 0.0f);
        t += 32;
        build_tsharp_f16(t, g_bloom_b[i], w, h, g_bloom_pitch[i]);
        build_ssharp_clamp(t + 8, 1);
        post_consts(t, 1.0f / w, 1.0f / h, 0.0f, 1.0f / h);
        t += 32;
    }
    for (int i = BLOOM_LEVELS - 2; i >= 0; i--, t += 32) {
        int j = i + 1;
        build_tsharp_f16(t, g_bloom_a[i], g_bloom_w[i], g_bloom_h[i], g_bloom_pitch[i]);
        build_ssharp_clamp(t + 8, 0);
        post_consts(t, 1.0f / g_bloom_w[i], 1.0f / g_bloom_h[i], 1.0f, 1.0f);
        build_tsharp_f16(t + 16, (j == BLOOM_LEVELS - 1) ? g_bloom_a[j] : g_bloom_b[j],
                         g_bloom_w[j], g_bloom_h[j], g_bloom_pitch[j]);
        build_ssharp_clamp(t + 24, 1);
    }
    build_tsharp_f16(t, g_hdr, DISPLAY_W, DISPLAY_H, DISPLAY_W);
    build_ssharp_clamp(t + 8, 0);
    post_consts(t, 1.0f / DISPLAY_W, 1.0f / DISPLAY_H, BLOOM_INTENSITY, EXPOSURE);
    build_tsharp_f16(t + 16, g_bloom_b[0], g_bloom_w[0], g_bloom_h[0], g_bloom_pitch[0]);
    build_ssharp_clamp(t + 24, 1);
    /* Frost chain (ps_post_down / ps_post_blur as in the bloom): the finished frame downsampled
       1920 -> 480 -> 240 (threshold 0) and blurred H V H V at 240 x 135 into g_bloom_a[1]; the
       bloom buffers are free once the final pass has read them. */
    uint32_t* f = tab + FROST_BLOCK * 32;
    build_tsharp_f16(f, g_frame, DISPLAY_W, DISPLAY_H, DISPLAY_W);
    build_ssharp_clamp(f + 8, 1);
    post_consts(f, 1.0f / g_bloom_w[0], 1.0f / g_bloom_h[0], 1.0f / DISPLAY_W, 1.0f / DISPLAY_H);
    ((float*)f)[28] = 0.0f;
    f += 32;
    build_tsharp_f16(f, g_bloom_a[0], g_bloom_w[0], g_bloom_h[0], g_bloom_pitch[0]);
    build_ssharp_clamp(f + 8, 1);
    post_consts(f, 1.0f / g_bloom_w[1], 1.0f / g_bloom_h[1], 1.0f / g_bloom_w[0],
                1.0f / g_bloom_h[0]);
    ((float*)f)[28] = 0.0f;
    f += 32;
    for (int k = 0; k < 2; k++) {
        float w = g_bloom_w[1], h = g_bloom_h[1];
        build_tsharp_f16(f, g_bloom_a[1], w, h, g_bloom_pitch[1]);
        build_ssharp_clamp(f + 8, 1);
        post_consts(f, 1.0f / w, 1.0f / h, 1.0f / w, 0.0f);
        f += 32;
        build_tsharp_f16(f, g_bloom_b[1], w, h, g_bloom_pitch[1]);
        build_ssharp_clamp(f + 8, 1);
        post_consts(f, 1.0f / w, 1.0f / h, 0.0f, 1.0f / h);
        f += 32;
    }
    /* MSAA resolve (ps_resolve): the 4-sample scene, PAL's MSAA SRD - TYPE 2D_MSAA (0xE),
       BASE_LEVEL 0, LAST_LEVEL = log2(samples), TILING_INDEX as the colour target. */
    if (g_msaa_color) {
        uint32_t* r = tab + RESOLVE_BLOCK * 32;
        build_tsharp_f16(r, g_msaa_color, DISPLAY_W, DISPLAY_H, DISPLAY_W);
        build_tsharp_depth_msaa(r + 8, g_msaa_depth, DISPLAY_W, DISPLAY_H); /* sample depths */
        if (g_hz_table)
            build_tsharp_f16(r + 16, g_hz_table, ATMO_SKY_W, 3 * ATMO_SLICES,
                             ATMO_SKY_W); /* the sky's horizon, every slice */
        build_ssharp_clamp(r + 24, 1);                                       /* bilinear, clamp */
        r[3] = (r[3] & ~((0x1Fu << 20) | (0xFu << 28) | (0xFu << 16) | (0xFu << 12))) |
               ((uint32_t)MSAA_TILE_INDEX << 20) | (0xEu << 28) | (2u << 16);
    }
    /* UI pass (ps_ui): frame + point S#, {1/w, 1/h}, frost (g_bloom_a[1]) + bilinear S#; the
       rects, UI T# and S# (28..47) come from ui_write_table. */
    uint32_t* u = tab + UI_BLOCK * 32;
    build_tsharp_f16(u, g_frame, DISPLAY_W, DISPLAY_H, DISPLAY_W);
    build_ssharp_clamp(u + 8, 0);
    post_consts(u, 1.0f / DISPLAY_W, 1.0f / DISPLAY_H, 0.0f, 0.0f);
    build_tsharp_f16(u + 16, g_bloom_a[1], g_bloom_w[1], g_bloom_h[1], g_bloom_pitch[1]);
    build_ssharp_clamp(u + 24, 1);
    /* Clock mode: the frost continued 240 -> 120 -> 60 wide and blurred
       H V H V at 60 x 34 into g_bloom_a[3] (~45 px), ps_clock_light's constants, ps_clock's static
       part (frame, frost, the light T# g_bloom_b[0], S#s, the slab). */
    uint32_t* c = tab + CLOCK_FROST_BLOCK * 32;
    for (int i = 2; i <= 3; i++, c += 32) {
        build_tsharp_f16(c, g_bloom_a[i - 1], g_bloom_w[i - 1], g_bloom_h[i - 1],
                         g_bloom_pitch[i - 1]);
        build_ssharp_clamp(c + 8, 1);
        post_consts(c, 1.0f / g_bloom_w[i], 1.0f / g_bloom_h[i], 1.0f / g_bloom_w[i - 1],
                    1.0f / g_bloom_h[i - 1]);
        ((float*)c)[28] = 0.0f;
    }
    for (int k = 0; k < 2; k++) {
        float w = g_bloom_w[3], h = g_bloom_h[3];
        build_tsharp_f16(c, g_bloom_a[3], (int)w, (int)h, g_bloom_pitch[3]);
        build_ssharp_clamp(c + 8, 1);
        post_consts(c, 1.0f / w, 1.0f / h, 1.0f / w, 0.0f);
        c += 32;
        build_tsharp_f16(c, g_bloom_b[3], (int)w, (int)h, g_bloom_pitch[3]);
        build_ssharp_clamp(c + 8, 1);
        post_consts(c, 1.0f / w, 1.0f / h, 0.0f, 1.0f / h);
        c += 32;
    }
    float* lt = (float*)(tab + CLOCK_LIGHT_BLOCK * 32);
    lt[4] = 1.44269504f / CLOCK_LIGHT_LS;
    lt[5] = 0.5f * 1.44269504f;
    lt[6] = CLOCK_LIGHT_S0;
    lt[7] = CLOCK_LIGHT_SPREAD;
    lt[8] = 4.0f; /* px per texel of the 480 x 270 target */
    lt[9] = 0.5f * DISPLAY_W; /* the slab, for the pass's early-out */
    lt[10] = 0.5f * DISPLAY_H;
    lt[11] = 0.5f * CLOCK_W;
    lt[12] = 0.5f * CLOCK_H;
    lt[13] = CLOCK_RC;
    uint32_t* k = tab + CLOCK_BLOCK * 32;
    build_tsharp_f16(k, g_frame, DISPLAY_W, DISPLAY_H, DISPLAY_W);
    build_ssharp_clamp(k + 8, 1);
    build_tsharp_f16(k + 12, g_bloom_a[3], g_bloom_w[3], g_bloom_h[3], g_bloom_pitch[3]);
    if (g_clock_light_rt)
        build_tsharp_f16(k + 20, g_clock_light_rt, g_bloom_w[0], g_bloom_h[0], g_bloom_pitch[0]);
    build_ssharp_clamp(k + 36, 0);
    float* kc = (float*)(k + 40);
    kc[0] = 0.5f * DISPLAY_W;
    kc[1] = 0.5f * DISPLAY_H;
    kc[2] = 0.5f * CLOCK_W;
    kc[3] = 0.5f * CLOCK_H;
    kc[4] = CLOCK_RC;
    kc[5] = 1.0f / DISPLAY_W;
    kc[6] = 1.0f / DISPLAY_H;
    kc[14] = 0.5f * (DISPLAY_W - CLOCK_W);
    kc[15] = 0.5f * (DISPLAY_H - CLOCK_H);
    kc[16] = 1.0f / CLOCK_W;
    kc[17] = 1.0f / CLOCK_H;
}

/* CB_COLOR0_INFO: FORMAT @2, LINEAR_GENERAL @7, NUMBER_TYPE @8, COMP_SWAP @11,
   BLEND_CLAMP @15 (gfx_7_2_sh_mask.h / enum.h). Blend clamp as Mesa: set for
   NORM/SRGB, not FLOAT. */
#define CB_INFO_RGBA16F ((0xCu << 2) | (1u << 7) | (7u << 8))
#define CB_INFO_DISPLAY_SRGB ((0xAu << 2) | (1u << 7) | (6u << 8) | (1u << 11) | (1u << 15))
/* Final composite target: ps_post_final encodes sRGB (and dithers) itself. */
#define CB_INFO_DISPLAY_UNORM ((0xAu << 2) | (1u << 7) | (0u << 8) | (1u << 11) | (1u << 15))

/* One full-screen pass into dst (w x h, pitch in pixels): the prior pass's target becomes a
   texture (the same ACQUIRE_MEM as the shadow pass), then state for this size, set up the way
   build_shadow_dcb sets up its target. */
/* Rasterizer / DB MSAA state (PAL gfx6MsaaState.cpp; fields from AMD gfx_7_2_sh_mask.h).
   4 samples: PA_SC_AA_CONFIG MSAA_NUM_SAMPLES 2 | MAX_SAMPLE_DIST 6 << 13 | MSAA_EXPOSED_SAMPLES 2
   << 20; PA_SC_MODE_CNTL_0 MSAA_ENABLE | VPORT_SCISSOR_ENABLE; DB_EQAA MAX_ANCHOR_SAMPLES 2 |
   MASK_EXPORT_NUM_SAMPLES 2 << 8 | ALPHA_TO_MASK_NUM_SAMPLES 2 << 12 | HIGH_QUALITY_INTERSECTIONS
   | INCOHERENT_EQAA_READS | STATIC_ANCHOR_ASSOCIATIONS (PS_ITER_SAMPLES 0: one shade per pixel);
   PAL's default 4x pattern (-2,-6) (6,-2) (-6,2) (2,6) in 1/16 px for all four quad pixels;
   centroid priorities by distance (all equal: PAL's sort keeps 0,1,2,3); DB_RENDER_OVERRIDE2
   DECOMPRESS_Z_ON_FLUSH (PAL: samples > 2). 1 sample: the PS4 driver's init values
   (PA_SC_MODE_CNTL_0 0, PA_SC_AA_CONFIG 0), PAL's 1x DB_EQAA, zero locations and priorities. */
static void emit_msaa_state(struct PM4Builder* b, int samples) {
    static const int pat[4][2] = {{-2, -6}, {6, -2}, {-6, 2}, {2, 6}};
    const int four = (samples == 4);
    uint32_t locs[16] = {0}, cp[2] = {0u, 0u}, mask[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    if (four) {
        uint32_t v = 0;
        for (int k = 0; k < 4; k++)
            v |= (((uint32_t)pat[k][0] & 0xFu) << (8 * k)) |
                 (((uint32_t)pat[k][1] & 0xFu) << (8 * k + 4));
        locs[0] = locs[4] = locs[8] = locs[12] = v; /* S0..S3 of X0Y0, X1Y0, X0Y1, X1Y1 */
        for (int i = 0; i < 8; i++)
            cp[0] |= (uint32_t)(i & 3) << (4 * i);
        cp[1] = cp[0];
    }
    pm4_set_context_reg(b, CTX_AA_CONFIG, four ? (2u | (6u << 13) | (2u << 20)) : 0u);
    pm4_set_context_reg(b, CTX_MODE_CONTROL, four ? 3u : 0u);
    pm4_set_context_reg(b, CTX_DB_EQAA,
                        four ? (2u | (2u << 8) | (2u << 12) | (1u << 16) | (1u << 17) | (1u << 20))
                             : ((1u << 16) | (1u << 17) | (1u << 20)));
    pm4_set_context_regs(b, CTX_PA_SC_AA_MASK_X0Y0_X1Y0, mask, 2);
    pm4_set_context_regs(b, CTX_PA_SC_CENTROID_PRIORITY_0, cp, 2);
    pm4_set_context_regs(b, CTX_PA_SC_AA_SAMPLE_LOCS_X0Y0_0, locs, 16);
    pm4_set_context_reg(b, CTX_DB_RENDER_OVERRIDE2, four ? (1u << 8) : 0u);
}

static void post_pass(struct PM4Builder* b, void* dst, uint32_t pitch, uint32_t w, uint32_t h,
                      uint32_t info, const void* ps, uint32_t rsrc1, const uint32_t* tab,
                      const uint32_t* bg_v) {
    pm4_acquire_mem(b, COHER_RT_TO_TEXTURE);
    {
        uint32_t s[2] = {0, (w & 0x7FFF) | ((h & 0x7FFF) << 16)};
        pm4_set_context_regs(b, CTX_SCREEN_SCISSOR, s, 2);
        pm4_set_context_regs(b, CTX_VIEWPORT_SCISSOR0, s, 2);
        s[0] = (1u << 31); /* WINDOW_OFFSET_DISABLE */
        pm4_set_context_regs(b, CTX_GENERIC_SCISSOR, s, 2);
        pm4_set_context_regs(b, CTX_WINDOW_SCISSOR, s, 2);
    }
    pm4_emit(b, pm4_type3(PM4_SET_CONTEXT_REG, 7));
    pm4_emit(b, CTX_VIEWPORT0);
    pm4_emit_f(b, (float)w * 0.5f);
    pm4_emit_f(b, (float)w * 0.5f);
    pm4_emit_f(b, (float)h * -0.5f);
    pm4_emit_f(b, (float)h * 0.5f);
    pm4_emit_f(b, 1.0f);
    pm4_emit_f(b, 0.0f);
    {
        uint32_t c = (uint32_t)((uint64_t)(uintptr_t)dst >> 8);
        uint32_t r[14] = {c, (pitch / 8) - 1, (pitch * h / 64) - 1, 0, info, 0, 0, 0, 0, 0, 0, 0, 0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, w | (h << 16));
    }
    pm4_set_context_reg(b, CTX_DEPTH_CONTROL, 0);
    pm4_set_context_reg(b, CTX_DB_Z_INFO, 0); /* Z_INVALID: no depth surface */
    pm4_set_context_reg(b, CTX_POLYGON_CONTROL, 0);
    pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);
    /* PERSP_CENTER + POS_X/Y_FLOAT: v2, v3 = pixel centre (PIX_CENTER = 1). */
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x302);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x302);
    {
        uint64_t a = (uint64_t)(uintptr_t)ps;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), rsrc1, (2u << 1)};
        pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4);
        uint64_t t = (uint64_t)(uintptr_t)tab;
        uint32_t ud[2] = {(uint32_t)t, (uint32_t)(t >> 32)};
        pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
    }
    pm4_set_sh_regs(b, SH_VS_USER_DATA_0, bg_v, 4);
    pm4_draw_index_auto(b, BG_VERTS);
}

/* PGM_RSRC1 of the post shaders: VGPRs v38 / v51 / v15, SGPRs s20 / s19 / s31,
   each + VCC (the header token writes vcc_hi). */
#define PS_POST_DOWN_RSRC1 ((2u << 6) | 9u)
#define PS_POST_BLUR_RSRC1 ((2u << 6) | 12u)
#define PS_POST_COMP_RSRC1 ((4u << 6) | 3u)
#define PS_POST_FINAL_RSRC1 ((7u << 6) | 9u) /* v36, s56 + VCC (lens flare) */
#define PS_RESOLVE_RSRC1 ((10u << 6) | 23u)  /* v0-v95, s0-s79 + VCC */
#define PS_UI_RSRC1 ((11u << 6) | 10u)       /* v43, s87 + VCC (frosted glass, UI, OPCODE_TEST) */
#define PS_CLOCK_RSRC1 ((9u << 6) | 17u)     /* v71, s76 incl. VCC */
#define PS_CLOCK_LIGHT_RSRC1 ((4u << 6) | 4u) /* v16, s34 incl. VCC */

/* HDR scene -> 6-level bloom chain -> composite into the sRGB display buffer.
   Order and tables as build_post_tables. */
static void emit_post(struct PM4Builder* b, void* display, const uint32_t* bg_v) {
    const uint32_t* t = g_post_tab;
    emit_msaa_state(b, 1); /* every post pass is single-sample */
    if (g_msaa_color)
        /* the resolve reads the scene depth: flush the DB to memory first, as PAL does for depth
           read as a texture (DB_ACTION_ENA with DB_DEST_BASE_ENA bit 14 and DEST_BASE_0_ENA bit 0,
           gfx6Barrier.cpp; gfx_7_2_sh_mask.h) */
        pm4_acquire_mem(b, COHER_RT_TO_TEXTURE | (1u << 14) | (1u << 0));
    if (g_msaa_color)
        post_pass(b, g_hdr, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_RGBA16F, g_ps_resolve_gpu,
                  PS_RESOLVE_RSRC1, g_post_tab + RESOLVE_BLOCK * 32, bg_v);
    for (int i = 0; i < BLOOM_LEVELS; i++, t += 32)
        post_pass(b, g_bloom_a[i], g_bloom_pitch[i], g_bloom_w[i], g_bloom_h[i], CB_INFO_RGBA16F,
                  g_ps_post_down_gpu, PS_POST_DOWN_RSRC1, t, bg_v);
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        post_pass(b, g_bloom_b[i], g_bloom_pitch[i], g_bloom_w[i], g_bloom_h[i], CB_INFO_RGBA16F,
                  g_ps_post_blur_gpu, PS_POST_BLUR_RSRC1, t, bg_v);
        t += 32;
        post_pass(b, g_bloom_a[i], g_bloom_pitch[i], g_bloom_w[i], g_bloom_h[i], CB_INFO_RGBA16F,
                  g_ps_post_blur_gpu, PS_POST_BLUR_RSRC1, t, bg_v);
        t += 32;
    }
    for (int i = BLOOM_LEVELS - 2; i >= 0; i--, t += 32)
        post_pass(b, g_bloom_b[i], g_bloom_pitch[i], g_bloom_w[i], g_bloom_h[i], CB_INFO_RGBA16F,
                  g_ps_post_comp_gpu, PS_POST_COMP_RSRC1, t, bg_v);
    post_pass(b, g_frame, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_RGBA16F, g_ps_post_final_gpu,
              PS_POST_FINAL_RSRC1, t, bg_v);
    /* Frost chain, then the UI pass (frame + frosted glass + UI, sRGB) to the display. */
    const uint32_t* f = g_post_tab + FROST_BLOCK * 32;
    for (int k = 0; k < 2; k++, f += 32)
        post_pass(b, g_bloom_a[k], g_bloom_pitch[k], g_bloom_w[k], g_bloom_h[k], CB_INFO_RGBA16F,
                  g_ps_post_down_gpu, PS_POST_DOWN_RSRC1, f, bg_v);
    for (int k = 0; k < 4; k++, f += 32)
        post_pass(b, (k & 1) ? g_bloom_a[1] : g_bloom_b[1], g_bloom_pitch[1], g_bloom_w[1],
                  g_bloom_h[1], CB_INFO_RGBA16F, g_ps_post_blur_gpu, PS_POST_BLUR_RSRC1, f, bg_v);
    if (g_clock_mode && g_clock.ok && g_ps_clock_gpu && g_clock_light_rt) {
        /* clock mode: the frost on to 60 x 34, the internal light, the glass clock (ps_clock) */
        const uint32_t* c = g_post_tab + CLOCK_FROST_BLOCK * 32;
        for (int i = 2; i <= 3; i++, c += 32)
            post_pass(b, g_bloom_a[i], g_bloom_pitch[i], g_bloom_w[i], g_bloom_h[i],
                      CB_INFO_RGBA16F, g_ps_post_down_gpu, PS_POST_DOWN_RSRC1, c, bg_v);
        for (int k = 0; k < 4; k++, c += 32)
            post_pass(b, (k & 1) ? g_bloom_a[3] : g_bloom_b[3], g_bloom_pitch[3], g_bloom_w[3],
                      g_bloom_h[3], CB_INFO_RGBA16F, g_ps_post_blur_gpu, PS_POST_BLUR_RSRC1, c,
                      bg_v);
        if (g_clock_light_dirty) /* only when the light's direction moved (clock_frame) */
            post_pass(b, g_clock_light_rt, g_bloom_pitch[0], g_bloom_w[0], g_bloom_h[0],
                      CB_INFO_RGBA16F, g_ps_clock_light_gpu, PS_CLOCK_LIGHT_RSRC1,
                      g_post_tab + CLOCK_LIGHT_BLOCK * 32, bg_v);
        post_pass(b, display, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_DISPLAY_UNORM,
                  g_ps_clock_gpu, PS_CLOCK_RSRC1, g_post_tab + CLOCK_BLOCK * 32, bg_v);
    } else {
        if (g_o5.ok) { /* OPCODE_TEST 5: every V_CVT row in FLOAT_MODE A and B, before ps_ui reads
                          them */
            for (int m = 0; m < OPT5_MODES; m++)
                post_pass(b, g_o5.rt, OPT5_RT_W, OPT5_RT_W, 1, CB_INFO_DISPLAY_UNORM, g_o5_ps[m],
                          OPT5_PASS_RSRC1 | ((uint32_t)k_opt5_float_mode[m] << 12),
                          g_o5.tab + 16 * m, bg_v);
        }
        post_pass(b, display, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_DISPLAY_UNORM, g_ps_ui_gpu,
                  PS_UI_RSRC1, g_post_tab + UI_BLOCK * 32, bg_v);
    }
}

/* OPCODE_TEST 5: the results as GitHub tables in the trace log, one per instruction (B bold where
   it differs from A); without both markers the CPU does not see the GPU's writes (shadPS4:
   readbacks). */
#define OPT5_STR_(x) #x
#define OPT5_STR(x) OPT5_STR_(x)
#define OPT5_TEST_STR OPT5_STR(OPCODE_TEST)
/* "0x" and two hex digits of v: a float mode in the log's header. */
static int o5_hex2(char* p, uint32_t v) {
    static const char hx[] = "0123456789ABCDEF";
    p[0] = '0';
    p[1] = 'x';
    p[2] = hx[(v >> 4) & 15];
    p[3] = hx[v & 15];
    return 4;
}

static int o5_hex(char* o, uint32_t v) {
    o[0] = '0';
    o[1] = 'x';
    for (int i = 0; i < 8; i++)
        o[2 + i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15];
    return 10;
}

static int o5_value(char* o, const volatile uint32_t* r, int wide) { /* 0xHI_LO for 64 bits */
    int n = o5_hex(o, wide ? r[1] : r[0]);
    if (!wide)
        return n;
    o[n++] = '_';
    for (int i = 0; i < 8; i++)
        o[n++] = "0123456789ABCDEF"[(r[0] >> (28 - 4 * i)) & 15];
    return n;
}

static void o5_log(void) {
    int n = g_ui.o5n;
    const volatile uint32_t* r = g_o5.res;
    char line[OPT5_LINE];
    int k = 0;
    int marks_ok = 1;
    for (int m = 0; m < OPT5_MODES; m++)
        marks_ok &= r[OPT5_STRIDE / 4 * n + m] == OPT5_MARK(m);
    if (!marks_ok) {
        k = 0;
        const char* m = "opt5: results not visible to the CPU (markers ";
        while (*m)
            line[k++] = *m++;
        for (int m = 0; m < OPT5_MODES; m++) {
            k += o5_hex(line + k, r[OPT5_STRIDE / 4 * n + m]);
            line[k++] = m + 1 < OPT5_MODES ? ' ' : ')';
        }
        k--;
        m = "); shadPS4: readbacksMode = Precise\n";
        while (*m)
            line[k++] = *m++;
        trace_line(line, (unsigned long)k);
        g_ui.o5status = 2;
        return;
    }
    trace_msg("\n## " OPT5_TITLE " (OPCODE_TEST " OPT5_TEST_STR ", build " BUILD_TAG
              ")\n\nColumns: the pass's PGM_RSRC1 FLOAT_MODE ([7:6] f64/f16 denormals, [5:4] f32 "
              "denormals, [3:2] f64 rounding, [1:0] f32 rounding); bold where it differs from the "
              "first.\n");
    for (int i = 0; i < n; i++) {
        int sl = g_ui.o5slot[i];
        const char* m;
        k = 0;
        if (sl < 0) {
            m = "\n### ";
            while (*m)
                line[k++] = *m++;
            for (m = k_opt5_op[-1 - sl]; *m;)
                line[k++] = *m++;
            m = "\n\n| Input |";
            while (*m)
                line[k++] = *m++;
            for (int md = 0; md < OPT5_MODES; md++) {
                line[k++] = ' ';
                k += o5_hex2(line + k, k_opt5_float_mode[md]);
                line[k++] = ' ';
                line[k++] = '|';
            }
            m = "\n|---|";
            while (*m)
                line[k++] = *m++;
            for (int md = 0; md < OPT5_MODES; md++) {
                m = "--:|";
                while (*m)
                    line[k++] = *m++;
            }
            line[k++] = '\n';
        } else {
            const volatile uint32_t* v = r + OPT5_STRIDE / 4 * i;
            int wide = ui_o5_wide(sl);
            m = "| ";
            while (*m)
                line[k++] = *m++;
            for (m = k_opt5_label[sl]; *m;)
                line[k++] = *m++;
            for (int md = 0; md < OPT5_MODES; md++) {
                const volatile uint32_t* w = v + 2 * md;
                int diff = md && (w[0] != v[0] || (wide && w[1] != v[1]));
                m = diff ? " | **" : " | ";
                while (*m)
                    line[k++] = *m++;
                k += o5_value(line + k, w, wide);
                if (diff) {
                    line[k++] = '*';
                    line[k++] = '*';
                }
            }
            line[k++] = ' ';
            line[k++] = '|';
            line[k++] = '\n';
        }
        trace_line(line, (unsigned long)k);
    }
    g_ui.o5status = 1;
}

/* ==== §13 Main command buffer (build_dcb) ===================================================== */
static uint32_t build_dcb(struct PM4Builder *b,
    const void *vs, const void *ps, const void *ps_bg, const void *ps_null,
    const void *ps_floor,
    const uint32_t *vb_v, const uint32_t *bg_v, const uint32_t *shadow_vb_v,
    const uint32_t *floor_v,
    void *vb_base, uint32_t *desc, int model_verts, unsigned long vb_total,
    uint32_t *ib_ptr, int num_indices, int is_indexed,
    void *color, void *depth, void *shadow_depth,
    volatile uint32_t *fence, uint32_t fv, int no_flip) {

    /* Default hardware-state init (sceGnmDrawInitDefaultHardwareState equivalent).
       Required on real PS4 — without it context registers are undefined and the
       GPU hangs. Must come first, before context_control and any draw state. */
    /* The game's leading tag NOP - first thing in every command buffer.
       See pm4_leading_tag() in pm4.h. */
    pm4_leading_tag(b, 0x5344u /* 'SD' */, g_cp_frame);
    CPMARK(b, 0x10);   /* main: entered */
    pm4_init_default_hw_state(b);
    CPMARK(b, 0x11);   /* main: hw state done */



    pm4_context_control(b);

    /* Shadow pass moved to end of DCB — see after model draw below.
       Rationale: testing if order of execution is the issue (user hint). */

    // VS: GPU-MVP, user_sgpr=4 (s[0:3]=V#). No sun in VS — PS handles lighting.
    // PGM_RSRC1 = 0xCB: vgpr_field=11 (48 VGPRs),
    // sgpr_field=3 (32 SGPRs: s0-s28 + VCC). RSRC1=4 (20 VGPRs) faults on real hardware
    // because the shader accesses v20-v45 beyond the allocation.
    { uint64_t a=(uint64_t)(uintptr_t)vs;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0xCBu,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4);
      /* VS user data set per-draw below */ }

    // PS: user_sgpr=2. ps_shader (textured + Lambert + fog): v0-v43, s0-s51 + VCC
    // -> 44 VGPRs, 56 SGPRs: PGM_RSRC1 = (6 << 6) | 10 = 0x18A.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (6u << 6) | 10u, (2u << 1)};
        pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4);
        uint32_t ud[2] = {(uint32_t)((uint64_t)(uintptr_t)desc),
                          (uint32_t)((uint64_t)(uintptr_t)desc >> 32)};
        pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
    }

    // Scissors
    CPMARK(b, 0x12);   /* stage: scissors/viewport */
    pm4_set_context_reg(b,CTX_WINDOW_OFFSET,0);   /* orphan state: nobody else sets it */
    /* CIK hardware-hang workaround, from Mesa: leaving VGT_GS_ONCHIP_CNTL at 0
       can hang the GPU even with GS unused. No gnm init function sets it. */
    pm4_set_context_reg(b,CTX_VGT_GS_ONCHIP_CNTL,VGT_GS_ONCHIP_CNTL_SAFE);
    { uint32_t s[2]={0,(DISPLAY_W&0x7FFF)|((DISPLAY_H&0x7FFF)<<16)};
      pm4_set_context_regs(b,CTX_SCREEN_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_VIEWPORT_SCISSOR0,s,2);
      s[0]=(1u<<31);   /* WINDOW_OFFSET_DISABLE */
      /* WINDOW_OFFSET_DISABLE on both the generic and the window scissor (R_028240, R_028204), as
         Mesa does: PA_SC_WINDOW_OFFSET is never written. */
      pm4_set_context_regs(b,CTX_GENERIC_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_WINDOW_SCISSOR,s,2); }

    /* Viewport depth range. The app owns this - no init function sets it.
       Our projection produces NDC z in [0,1] (CLIPPER_CONTROL bit19 selects
       the zero-to-one convention), so the range is 0.0 .. 1.0. */
#if WRITE_VIEWPORT_DEPTH_RANGE
    { uint32_t z[2]; float zmin=0.0f, zmax=1.0f;
      __builtin_memcpy(&z[0], &zmin, 4); __builtin_memcpy(&z[1], &zmax, 4);
      pm4_set_context_regs(b,CTX_VIEWPORT_ZMIN0,z,2); }
#endif

    // Viewport — zscale=1, zoffset=0 for ZeroToOne clip space
    // (shadPS4 computes minDepth = zoffset - zscale, maxDepth = zoffset + zscale
    //  for MinusWToW; for ZeroToOne: minDepth = zoffset, maxDepth = zoffset + zscale).
    // With zscale=1, zoffset=0: minDepth=0, maxDepth=1 — full range, NDC.z→depth direct.
    pm4_emit(b,pm4_type3(PM4_SET_CONTEXT_REG,7));
    pm4_emit(b,CTX_VIEWPORT0);
    pm4_emit_f(b,(float)DISPLAY_W*0.5f); pm4_emit_f(b,(float)DISPLAY_W*0.5f);
    pm4_emit_f(b,(float)DISPLAY_H*-0.5f); pm4_emit_f(b,(float)DISPLAY_H*0.5f);
    pm4_emit_f(b,1.0f); pm4_emit_f(b,0.0f);

    pm4_set_context_reg(b,CTX_INDEX_OFFSET,0);

    // Depth — clear to 1.0f via DB_RENDER_CONTROL.depth_clear_enable on the sky draw.
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,1u); // bit0 = depth_clear_enable
    pm4_set_context_reg(b,CTX_DEPTH_VIEW,0);
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_OVERRIDE,0);
    pm4_set_context_reg(b,0x00B,0x3F800000u); // CTX_DEPTH_CLEAR = 1.0f
    /* Z_32_FLOAT; NUM_SAMPLES [3:2] = log2(samples) on the 4-sample buffer when MSAA is on. */
    pm4_set_context_reg(b, CTX_DB_Z_INFO, g_msaa_color ? (3u | (2u << 2)) : 3u);
    if (g_msaa_color)
        depth = g_msaa_depth;
    /* CIK takes the depth layout from DB_DEPTH_INFO (radeonsi si_init_depth_surface,
       chip_class >= CIK: ARRAY_MODE/PIPE_CONFIG/bank fields from the tile-mode
       entry; DB_Z_INFO.TILE_MODE_INDEX is SI-only). Never written before, it was
       0 = ARRAY_LINEAR_GENERAL, which addrlib never uses for depth. PS4 table
       entry Depth1DThin (5) = ARRAY_1D_TILED_THIN1, PIPE_CONFIG P8_32x32_16x16
       (shadPS4 tiling.cpp). 1D needs pitch/height % 8 (SiLib micro-tiled
       alignment): 1920x1080x4 = 8294400 B, inside the allocation. Bank fields
       do not apply to 1D. ADDR5_SWIZZLE_MASK = !tc_compatible_htile = 1. */
    pm4_set_context_reg(b,CTX_DB_DEPTH_INFO,(1u<<0)|(2u<<4)|(12u<<8));
    pm4_set_context_reg(b,CTX_DB_STENCIL_INFO,0);
    { uint32_t z=(uint32_t)((uint64_t)(uintptr_t)depth>>8);
      uint32_t d[4]={z,0,z,0};
      pm4_set_context_regs(b,CTX_DB_Z_READ_BASE,d,4); }
    pm4_set_context_reg(b,CTX_DB_DEPTH_SIZE,((DISPLAY_W/8)-1)|(((DISPLAY_H/8)-1)<<11));
    pm4_set_context_reg(b,CTX_DB_DEPTH_SLICE,(DISPLAY_W*DISPLAY_H/64)-1);
    /* Clear-by-draw, the game's pattern (DB_RENDER_CONTROL=3 + 0x777) minus stencil: Z enable | Z
       write | ALWAYS. Needs DB_DEPTH_INFO set (it crashes with linear general). */
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(7u<<4));

    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0); /* no culling for BG */

    /* Colour: the RGBA16F HDR target (emit_post composites it into the display buffer); without it,
       straight into the sRGB display buffer (NUMBER_TYPE UNORM / SRGB: SNORM would halve it). */
    if (g_msaa_color) {
        /* 4-sample RGBA16F (PAL gfx6ColorTargetView.cpp, no FMASK / CMASK): tiled (tile index 13,
           LINEAR_GENERAL 0), ATTRIB NUM_SAMPLES = NUM_FRAGMENTS = 2; the hardware quirk without
           FMASK: FMASK_TILE_MODE_INDEX = TILE_MODE_INDEX, PITCH.FMASK_TILE_MAX = TILE_MAX,
           FMASK_SLICE = SLICE, FMASK base = colour base (CB doc); CMASK base / slice 0. */
        uint32_t c = (uint32_t)((uint64_t)(uintptr_t)g_msaa_color >> 8);
        uint32_t pt = (DISPLAY_W / 8) - 1, sl = (DISPLAY_W * DISPLAY_H / 64) - 1;
        uint32_t r[14] = {c,
                          pt | (pt << 20),
                          sl,
                          0,
                          (0xCu << 2) | (7u << 8), /* 16_16_16_16, FLOAT */
                          MSAA_TILE_INDEX | (MSAA_TILE_INDEX << 5) | (2u << 12) | (2u << 15),
                          0,
                          0,
                          0,
                          c,
                          sl,
                          0,
                          0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, DISPLAY_W | (DISPLAY_H << 16));
    } else {
        void* rt = g_hdr ? g_hdr : color;
        uint32_t c = (uint32_t)((uint64_t)(uintptr_t)rt >> 8);
        uint32_t r[14] = {c,
                          (DISPLAY_W / 8) - 1,
                          (DISPLAY_W * DISPLAY_H / 64) - 1,
                          0,
                          g_hdr ? CB_INFO_RGBA16F : CB_INFO_DISPLAY_SRGB,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0,
                          0};
        pm4_set_context_regs(b, CTX_CB_COLOR0_BASE, r, 14);
        pm4_emit(b, 0xC0001000u);
        pm4_emit(b, DISPLAY_W | (DISPLAY_H << 16));
    }

    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);           /* attr0: VS param 0 -> PS slot 0 */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0+1,1);         /* attr1: VS param 1 -> PS slot 1 */
    /* SPI_VS_OUT_CONFIG: VS_EXPORT_COUNT (bits 5:1) = param exports - 1; two params: (1 << 1). */
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 1u << 1);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,2);                /* 2 attrs: {u,v,ny,nz} and {wpos.xyzw} */
    pm4_set_context_reg(b,CTX_SHADER_POS_FORMAT,4);
    pm4_set_context_reg(b,CTX_Z_EXPORT_FORMAT,0);
    pm4_set_context_reg(b,CTX_COLOR_EXPORT_FORMAT,9);
    pm4_set_context_reg(b,CTX_COLOR_CONTROL,0x00CC0010u);
    pm4_set_context_reg(b,CTX_DB_SHADER_CONTROL,0);
    /* SPI_BARYC_CNTL, written as gnm's PS setup does (no init function does): 0 = PERSP_CENTER
       only, POS_FLOAT_LOCATION 0 (pixel centre) - what the shaders use. */
    pm4_set_context_reg(b,CTX_SPI_BARYC_CNTL,0);
    /* ClipperControl.clip_space = 1 (ZeroToOne / DX-convention, bit 19).
       Paired with our NDC.z∈[0,1] projection matrix. Avoids dependence on
       VK_EXT_depth_clip_control which may not be honored → produced the
       "finite render distance in front of camera" artifact. */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    /* VGT_SHADER_STAGES_EN / VGT_DMA_SIZE are never written, as in gnm and the game (CLEAR_STATE
       default; index sizes come from the draw packets): writing them every frame wedges the GPU. */
    emit_msaa_state(b, g_msaa_color ? MSAA_SAMPLES : 1); /* scene: 4x when MSAA is on */
    pm4_set_context_reg(b,CTX_BLEND_CONTROL0,0);
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);

    // Draw 1: BG quad with sky PS (sun disc)
    {
        uint64_t a = (uint64_t)(uintptr_t)ps_bg;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (12u << 6) | 12u, (2u << 1)};
        pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4); /* ps_dark: v0-v50, s0-s101 + VCC -> 52 / 104 */
        /* Sky PS needs desc ptr for sun position */
        uint32_t ud[2] = {(uint32_t)((uint64_t)(uintptr_t)desc),
                          (uint32_t)((uint64_t)(uintptr_t)desc >> 32)};
        pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
    }
    // VS s[0:3] = vertex/MVP V#. Sun read via s_buffer_load from V#+0x40
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b,BG_VERTS);
    GPU_TS(2);
    CPMARK(b, 0x13); /* sky drawn */

    /* Sky draw performed the one-shot depth clear. Disable clear flag so subsequent
       draws (floor, cube) render normally against the now-cleared depth buffer. */
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);

    /* Stars: after the sky, before the floor. Depth test LESS against the cleared
       1.0 without writing, so the floor drawn next covers anything below the
       horizon. Additive blend (CB_BLEND0_CONTROL: SRC ONE @0, DST ONE @8,
       COMB dst+src, ENABLE @30 - gfx_7_2_sh_mask.h / enum.h), reset to 0 after.
       The blend needs the export format radeonsi uses for blending a
       16_16_16_16 FLOAT target, FP16_ABGR (4); ps_stars exports packed halves.
       32_ABGR (9) again for the other draws. */
    if (g_stars_n > 0 && g_stars_draw && g_ps_stars_gpu) {
        {
            uint64_t a = (uint64_t)(uintptr_t)g_ps_stars_gpu;
            /* ps_stars: v0-v13, s0-s11 + VCC -> 16 VGPRs, 16 SGPRs */
            uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (1u << 6) | 3u, (2u << 1)};
            pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4);
            uint32_t ud[2] = {(uint32_t)((uint64_t)(uintptr_t)desc),
                              (uint32_t)((uint64_t)(uintptr_t)desc >> 32)};
            pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
        }
        pm4_set_context_reg(b, CTX_DEPTH_CONTROL,
                            (1u << 1) | (1u << 4));     /* Z test LESS, no write */
        pm4_set_context_reg(b, CTX_POLYGON_CONTROL, 0); /* no culling */
        pm4_set_context_reg(b, CTX_BLEND_CONTROL0, (1u << 0) | (1u << 8) | (1u << 30));
        pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 4); /* SPI_SHADER_FP16_ABGR */
        /* PERSP_CENTER | POS_X_FLOAT | POS_Y_FLOAT: v2, v3 = pixel centre (the moon hides stars) */
        pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x302);
        pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x302);
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, g_stars_v, 4);
        pm4_draw_index_auto(b, (uint32_t)g_stars_n * 6u);
        CPMARK(b, 0x14); /* stars drawn */
        pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x02);
        pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x02);
        pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);
        pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9); /* SPI_SHADER_32_ABGR */
    }

    // === Floor draw: between sky and cube ===
    // Depth test Less so cube draws on top, but floor is drawn first so cube occludes it.
    // Floor uses its own V# (floor_v) pointing at vb+FLOOR_MVP_OFF where MVP is mirrored
    // and floor verts are at V#+80.
    if (ps_floor && floor_v) {
        /* ps_floor (parallax + fog + PCF): v0-v99, s0-s87 + VCC -> 100 VGPRs, 96 SGPRs.
           It reads POS_Y (v2) for the fog colour: PERSP_CENTER | POS_Y_FLOAT. */
        uint64_t a=(uint64_t)(uintptr_t)ps_floor;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (11u << 6) | 24u, (2u << 1)};
        pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
        uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                        (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
        pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2);

        /* Depth for floor: less-than, write enabled (so cube z-tests correctly against floor) */
        pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
        pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back */

        pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x202);
        pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x202);
        pm4_set_sh_regs(b,SH_VS_USER_DATA_0,floor_v,4);
        pm4_draw_index_auto(b, FLOOR_VERTS);
        CPMARK(b, 0x15); /* floor drawn */
        pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x02);
        pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x02);
        GPU_TS(3);
    }

    // Switch back to textured PS for model. Same RSRC1 as initial: 44 VGPRs, 56 SGPRs.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (6u << 6) | 10u, (2u << 1)};
        pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4);
        uint32_t ud[2] = {(uint32_t)((uint64_t)(uintptr_t)desc),
                          (uint32_t)((uint64_t)(uintptr_t)desc >> 32)};
        pm4_set_sh_regs(b, SH_PS_USER_DATA_0, ud, 2);
    }

    // Switch to depth=Less for cube
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back, CW front */

    // Draw 2: Model with real MVP. Sun at V#+0x40 read via s_buffer_load
    { uint32_t cube_v[4];
      build_vsharp(cube_v,(char*)vb_base+MVP_OFF,(uint32_t)(vb_total > MVP_OFF ? vb_total - MVP_OFF : VERT_BUF_SIZE - MVP_OFF));
      if (g_model.enabled) {
          /* vs_model: V# + M in s[0:15] (16 user SGPRs); v0-v48, s0-s15 + VCC ->
             52 VGPRs, 24 SGPRs = 0x8C. ps_model: v0-v97, s0-s99 + VCC -> 100
             VGPRs, 104 SGPRs = 0x318. Three params: param2 = tangent, handedness. */
          uint64_t a = (uint64_t)(uintptr_t)g_model.vs;
          uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0x8Cu, (16u << 1)};
          pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
          uint32_t ud[16];
          my_memcpy(ud, cube_v, 16);
          my_memcpy(ud + 4, g_model.m, 48);
          pm4_set_sh_regs(b, SH_VS_USER_DATA_0, ud, 16);
          a = (uint64_t)(uintptr_t)g_model.ps;
          uint32_t p[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0x31Cu,
                           (2u << 1)}; /* v0-v114, s0-s101 + VCC */
          pm4_set_sh_regs(b, SH_PS_PGM_LO, p, 4);
          pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 2u << 1); /* three params */
          pm4_set_context_reg(b, CTX_PS_INPUT_CNTL_0 + 2, 2);
          pm4_set_context_reg(b, CTX_NUM_INTERP, 3);
      } else {
          pm4_set_sh_regs(b, SH_VS_USER_DATA_0, cube_v, 4);
      }
    }
    /* ps_shader reads POS_Y (v2) for the fog colour: PERSP_CENTER | POS_Y_FLOAT.
       (The loading screen's ps_blue writes v2 before reading it.) */
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x202);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x202);
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1); /* uint32 indices */
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
    }
    CPMARK(b, 0x16); /* cube / model drawn */
    pm4_set_context_reg(b, CTX_PS_INPUT_ENA, 0x02);
    pm4_set_context_reg(b, CTX_PS_INPUT_ADDR, 0x02);
    if (g_model.enabled) {
        /* Back to vs_shader and two params: the post passes draw with it. */
        uint64_t a = (uint64_t)(uintptr_t)vs;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0xCBu, (4u << 1)};
        pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
        pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 1u << 1);
        pm4_set_context_reg(b, CTX_NUM_INTERP, 2);
    }
    GPU_TS(4);
    if (g_hdr)
        emit_post(b, color, bg_v);
    GPU_TS(5);

    /* The two completion paths are mutually exclusive, as in the game: flip -> the marker block
       with the fence address and value, no EOP (gnm's patcher emits WRITE_DATA(label=1) +
       WRITE_DATA(fence) and registers the flip in this submit); no flip -> EVENT_WRITE_EOP with the
       fence, no marker. */
    CPMARK(b, 0x1F);   /* stage: all draws retired, about to complete */
    if (no_flip) pm4_event_write_eop(b,fence,fv);
    else         pm4_prepare_flip(b,fence,fv);   /* MUST be the last 64 dwords */
    return b->off*4;
}

/* ==== §14 Shadow command buffer (build_shadow_dcb) ============================================ */
/* === Standalone shadow DCB ===
   Renders model from sun POV into shadow_depth as an RGBA8 COLOR target.
   MRT0.r = NDC.z (8-bit quantized — sufficient precision for small-scene shadow).

   WHY COLOR not DEPTH: writing depth with D32_SFLOAT and sampling as R32_FLOAT
   creates two separate Vulkan images at the same GPU address (Vulkan §39.1.6
   forbids cross-format views between depth and color). shadPS4's texture cache
   would see a format mismatch and allocate a second image, silently breaking
   the write→sample chain. By making both ends RGBA8, shadPS4 reuses a single
   VkImage for write and sample, so cube depths actually become visible to the
   floor PS's compare. */
static uint32_t build_shadow_dcb(struct PM4Builder *b,
    const void *vs, const void *ps_shadow, const void *ps_clear,
    const uint32_t *shadow_vb_v, const uint32_t *shadow_floor_v,
    const uint32_t *bg_v, uint32_t *desc,
    int model_verts, int floor_verts,
    uint32_t *ib_ptr, int num_indices, int is_indexed,
    void *shadow_depth) {
    /* No fence: the shadow pass emits no EOP (it shares the command buffer with the main pass; the
       ACQUIRE_MEM between them is the barrier). */

    /* Default hardware-state init — same as main DCB. The shadow buffer is a
       separate command buffer (it runs first when present), so it also needs
       the register defaults established before any draw. Idempotent with the
       main DCB's copy when both run. */
    pm4_leading_tag(b, 0x5348u /* 'SH' */, g_cp_frame);
    CPMARK(b, 0x20);   /* shadow: entered */
    pm4_init_default_hw_state(b);
    CPMARK(b, 0x21);   /* shadow: hw state done */

    pm4_context_control(b);

    /* VS program — same as main pass. PGM_RSRC1=0x4B (46 VGPRs, 16 SGPRs);
       see main-pass note — the shader uses up to v45. */
    { uint64_t a=(uint64_t)(uintptr_t)vs;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0xCBu,(4u<<1)};
      pm4_set_sh_regs(b,SH_VS_PGM_LO,r,4); }

    /* Scissors/viewport — shadow map size */
    pm4_set_context_reg(b,CTX_WINDOW_OFFSET,0);   /* orphan state: nobody else sets it */
    /* CIK hardware-hang workaround, from Mesa: leaving VGT_GS_ONCHIP_CNTL at 0
       can hang the GPU even with GS unused. No gnm init function sets it. */
    pm4_set_context_reg(b,CTX_VGT_GS_ONCHIP_CNTL,VGT_GS_ONCHIP_CNTL_SAFE);
    { uint32_t s[2]={0,(SHADOW_W&0x7FFF)|((SHADOW_H&0x7FFF)<<16)};
      pm4_set_context_regs(b,CTX_SCREEN_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_VIEWPORT_SCISSOR0,s,2);
      s[0]=(1u<<31);   /* WINDOW_OFFSET_DISABLE */
      /* WINDOW_OFFSET_DISABLE on both the generic and the window scissor (R_028240, R_028204), as
         Mesa does: PA_SC_WINDOW_OFFSET is never written. */
      pm4_set_context_regs(b,CTX_GENERIC_SCISSOR,s,2);
      pm4_set_context_regs(b,CTX_WINDOW_SCISSOR,s,2); }

    /* Viewport depth range. The app owns this - no init function sets it.
       Our projection produces NDC z in [0,1] (CLIPPER_CONTROL bit19 selects
       the zero-to-one convention), so the range is 0.0 .. 1.0. */
#if WRITE_VIEWPORT_DEPTH_RANGE
    { uint32_t z[2]; float zmin=0.0f, zmax=1.0f;
      __builtin_memcpy(&z[0], &zmin, 4); __builtin_memcpy(&z[1], &zmax, 4);
      pm4_set_context_regs(b,CTX_VIEWPORT_ZMIN0,z,2); }
#endif
    pm4_emit(b,pm4_type3(PM4_SET_CONTEXT_REG,7));
    pm4_emit(b,CTX_VIEWPORT0);
    pm4_emit_f(b,(float)SHADOW_W*0.5f); pm4_emit_f(b,(float)SHADOW_W*0.5f);
    /* Y scale POSITIVE (no flip) — pixel row 0 at NDC y=-1, row SHADOW_H-1 at NDC y=+1.
       In Vulkan texture coords (UV.y = 0 at top-left = pixel row 0), this means:
         NDC y=-1 → UV.y=0  ;  NDC y=+1 → UV.y=1
       Floor PS computes UV.y = (NDC.y+1)*0.5, matching exactly. */
    pm4_emit_f(b,(float)SHADOW_H*0.5f); pm4_emit_f(b,(float)SHADOW_H*0.5f);
    /* Z scale=1, zoffset=0 (ZeroToOne clip-space): fragment z as stored = raw NDC.z.
       Must match the floor PS's NDC.z computation (which also does no extra scale). */
    pm4_emit_f(b,1.0f); pm4_emit_f(b,0.0f);

    pm4_set_context_reg(b,CTX_INDEX_OFFSET,0);

    /* === CB0 = RGBA8 at shadow_depth ===
       14 consecutive context regs from CTX_CB_COLOR0_BASE:
         +0  BASE  = shadow_depth >> 8
         +1  PITCH = (width / 8) - 1
         +2  SLICE = (width * height / 64) - 1
         +3  VIEW  = 0
         +4  INFO  = 0x00A8: R8G8B8A8, UNORM (bit 8 clear), linear general,
                     COMP_SWAP 0 (the T#'s order)
         +5..+13 = 0 (no compression, no FMask)
       The trailing 0xC0001000 NOP carries the {width, height} extent shadPS4's
       ImageInfo needs. */
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)shadow_depth>>8);
      uint32_t r[14]={c,(SHADOW_W/8u)-1u,(SHADOW_W*SHADOW_H/64u)-1u,0,
                      0x00A8u,0,0,0,0,0,0,0,0,0};   /* num_type 0 = UNORM */
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,SHADOW_W|(SHADOW_H<<16)); }

    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);   /* RGBA channel write enable */
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);   /* PS exports all 4 channels */

    /* No depth buffer this pass. Disable depth test entirely. Order of draws is
       the shadow map's "depth test": clear-quad first (fills with R=1.0),
       then cube draw on top (writes smaller R values where cube covers). Without
       a depth buffer, last-write-wins per pixel — fine since cube is the only
       occluder and culling handles the cube self-overlap from light's POV. */
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,0);
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);
    /* Unbind the depth surface (Z_INVALID): this pass renders shadow_depth as a colour target, and
       the main pass in the same command buffer binds a depth surface. */
    pm4_set_context_reg(b,CTX_DB_Z_INFO,0);            /* Z_INVALID */

    /* Pipeline state — color pass, RGBA8 export.
       Shadow PS reads attr1=world_pos. NUM_INTERP=2 (VS exports 2 params);
       PS_INPUT_CNTL_0 maps attr0→slot0, attr1→slot1. */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0+1,1);
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG,
                        1u << 1);                    /* two params (VS_EXPORT_COUNT, bits 5:1) */
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);    /* PERSP_CENTER_ENA */
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,2);
    pm4_set_context_reg(b,CTX_SHADER_POS_FORMAT,4);
    pm4_set_context_reg(b,CTX_Z_EXPORT_FORMAT,0);
    pm4_set_context_reg(b,CTX_COLOR_EXPORT_FORMAT,9);  /* 32_R_GR (RGBA8 pixel pipe) — same as main CB */
    pm4_set_context_reg(b,CTX_COLOR_CONTROL,0x00CC0010u);
    pm4_set_context_reg(b,CTX_DB_SHADER_CONTROL,0);
    /* SPI_BARYC_CNTL, written as gnm's PS setup does (no init function does): 0 = PERSP_CENTER
       only, POS_FLOAT_LOCATION 0 (pixel centre) - what the shaders use. */
    pm4_set_context_reg(b,CTX_SPI_BARYC_CNTL,0);
    /* ClipperControl = ZeroToOne (bit 19) — matches main DCB clip convention,
       matches the way light_MVP is constructed (NDC.z ∈ [0,1]). */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    /* VGT_SHADER_STAGES_EN / VGT_DMA_SIZE are never written, as in gnm and the game (CLEAR_STATE
       default; index sizes come from the draw packets): writing them every frame wedges the GPU. */
    emit_msaa_state(b, 1); /* the shadow map is single-sample */
    pm4_set_context_reg(b,CTX_BLEND_CONTROL0,0);
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);

    /* === Draw 1: fullscreen-clear quad with ps_clear ===
       BG verts are NDC-space fullscreen quad at z=0.999. With identity MVP in
       bg_v's referenced VB, the VS multiplies and outputs unchanged NDC. PS
       outputs (1,0,0,1) — fills shadow map with R=1.0 = "no occluder anywhere".
       This replaces the prior DMA fill, which wasn't reliably reaching shadPS4's
       VkImage cache (DMA wrote to GPU memory but the texture image kept stale
       contents from previous frames). A real GPU render pass clear works. */
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0);   /* no culling for fullscreen quad */
    { uint64_t a=(uint64_t)(uintptr_t)ps_clear;
      /* Clear PS uses v40..v43 for output — needs 44 VGPRs allocated.
         VGPR field = ceil(44/4) - 1 = 10. The binary uses s0-s9 + VCC = 12 SGPRs,
         so SGPR field = 1 (16). PGM_RSRC1 = (1<<6) | 10 = 0x4A. user_sgpr=2 in RSRC2.
         (Previous version had PGM_RSRC1=0 = 4 VGPRs — writes to v40+ went nowhere,
         so EXP read uninitialized garbage → shadow map filled with 0, not 1.0,
         making everything inside the light frustum read as "shadowed".) */
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x4Au,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b, BG_VERTS);   /* 6 verts = 2 triangles = fullscreen quad */

    /* === Draw 2: cube with shadow PS that exports NDC.z ===
       Switch PS to ps_shadow (NDC.z exporter). s0-s47 + VCC -> 56 SGPRs, 44 VGPRs (0x18A). */
    { uint64_t a=(uint64_t)(uintptr_t)ps_shadow;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x18Au,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

    /* Cull back-facing (from light POV) so only the light-facing cube surface
       is written. Without depth test, this prevents the back face overwriting
       the front face's depth value. */
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1u<<1));

    if (g_model.enabled) {
        /* vs_model_shadow: V# + M in s[0:15]; v0-v47, s0-s15 + VCC = 0x8B. */
        uint64_t a = (uint64_t)(uintptr_t)g_model.vs_shadow;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0x8Bu, (16u << 1)};
        pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
        uint32_t ud[16];
        my_memcpy(ud, shadow_vb_v, 16);
        my_memcpy(ud + 4, g_model.m, 48);
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, ud, 16);
    } else {
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, shadow_vb_v, 4);
    }
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1);
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
    }
    if (g_model.enabled) {
        uint64_t a = (uint64_t)(uintptr_t)vs;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0xCBu, (4u << 1)};
        pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
    }

    /* The floor casts no shadow (it would need depth / slope-scale bias against self-shadowing). */
    (void)shadow_floor_v; (void)floor_verts;

    /* Flush CB writes + invalidate TC so main DCB sees freshly-written shadow map.
       (1u<<25) = CB_ACTION_ENA  (flush CB pixel pipe)
       (1u<<23) = TC_ACTION_ENA  (invalidate texture cache)
       (1u<<6)  = CB_DEST_BASE_ENA */
    /* Flush the render pipes and invalidate BOTH cache levels before the main
       pass samples this surface. TCL1 (vector L1) is required alongside TC
       (L2) on CIK - see COHER_RT_TO_TEXTURE in pm4.h. */
    pm4_acquire_mem(b, COHER_RT_TO_TEXTURE);

    /* No EOP fence — shadow+main submitted as one Vulkan command buffer so the
       acquire_mem above is the barrier. */
    return b->off*4;
}

// Read entire file into GPU memory. Returns NULL if not found.

// ============================================================================
// BMP texture loader — reads 24/32-bit uncompressed BMP, converts to RGBA
// ============================================================================




// ============================================================================
// Float/int parsers for OBJ — with end-of-buffer safety

// Inline OBJ parser — minimal, no static arrays, writes directly to vb
// ============================================================================

// Parse OBJ text, write vertices to vb at CUBE_DATA_OFF.
// Allocates temp arrays via gpu_alloc. Returns vertex count.

// === Main ===

/* ==== §15 main(): setup (§15.1-6), main loop (§15.7-14) ======================================= */
int main(void) {
    printf("=== ShadCube4 ===\n");

    /* ==== §15.1 Video out, loading screen, GNM ================================================ */
    /* Video out opens as the SYSTEM user (0xFF), as a full-screen title does. */
    int video = sceVideoOutOpen(0xFF,0,0,0);
    if (video < 0) return 1;
    sceVideoOutSetFlipRate(video,0);

    unsigned long fb_size = (unsigned long)DISPLAY_W*DISPLAY_H*4;
    void *fb[NUM_FRAMES];
    for (int i=0;i<NUM_FRAMES;i++) { fb[i]=gpu_alloc(fb_size,0x100000); if(!fb[i])return 1; }

    unsigned char buf_attr[48]; my_memset(buf_attr,0,48);
    /* tiling_mode = 1 (LINEAR) — must match the CB_COLOR0 render target, which
       writes linear (LINEAR_GENERAL set in CB_COLOR0_INFO). With tiling_mode=0
       (TILE) the display reads the linear pixels as tiled → scrambled colors.
       Verified vs shadPS4 buffer.h: TilingMode::Tile=0, Linear=1. */
    sceVideoOutSetBufferAttribute(buf_attr,0x80000000,1,0,DISPLAY_W,DISPLAY_H,DISPLAY_W);
    sceVideoOutRegisterBuffers(video,0,fb,NUM_FRAMES,buf_attr);
    ls_start(video, fb[0], fb[1]); /* the loading screen, then the splash goes */

    /* Enable WAIT-FREE SUBMIT as early as the driver allows.
       libSceGnmDriver.prx exports seven library namespaces; one of them is
       libSceGnmWaitFreeSubmit, and it contains exactly two functions:
           +0x40 from sceGnmAreSubmitsAllowed : mode = 1 (enable)
           +0x90                              : mode = 0 (disable)
       They set a flag the driver pushes to the kernel via ioctl 0xc004811d on
       the NEXT sceGnmSubmitDone. This process had never issued that ioctl.
       Off (WAITFREE_SUBMIT 0); kept for reference. */
    /* Submit mode 0 (ioctl 0xc0108102, 16-byte argument), the game's; gnm's modes 1, 4 and 6 use
       ioctl 0xc020810c (32-byte argument) - WAITFREE_SUBMIT 1 selects mode 1. */
#if WAITFREE_SUBMIT
    int mode_set = gnm_set_mode(1);
#else
    /* DO NOT CALL IT AT ALL. Both waitfree entry points (gnm 0x1a10 / 0x1a60)
       take the driver mutex, issue a DRAIN (ioctl 0xc0048117) if the counter
       is non-zero, then write the mode byte at 0x1007c and mark it dirty -
       which SubmitDone later pushes with ioctl 0xc004811d.
       THE GAME CALLS NEITHER FUNCTION, so it never drains this way and never
       issues the mode-push ioctl. Calling gnm_set_mode(0) to "select mode 0"
       would still do both. The mode byte is a zero-initialised static, so
       mode 0 is already the default and the exact match is to leave it
       untouched. */
    int mode_set = 0;
#endif
    int mode_set_early = mode_set;      /* -2 = driver not ready yet, will retry */
    int mode_now = gnm_get_mode();

    /* Flip event queue for event-driven vsync pacing. The driver posts a flip
       event (data >> 16 == flip_arg) when each flip completes at vblank; we
       block on this queue each frame instead of polling. flip rate 0 = 60Hz. */
    OrbisKernelEqueue flip_eq = 0;
    int eq_created = (sceKernelCreateEqueue(&flip_eq, "cube_flip") == 0);
    /* One registered flip event; we block on it each frame to pace. */
    int flip_ev_ok = eq_created && (sceVideoOutAddFlipEvent(flip_eq, video, 0) == 0);

    /* From here on video-out is REGISTERED and the flip event exists. Any early
       exit must tear all of it down in the same order the normal teardown uses,
       or the OS blocks when the app is closed (~1 min timeout, then a crash).
       This macro is the only sanctioned early exit. */
    #define FATAL_EXIT(msg) do { \
        trace_msg("FATAL: " msg "\n"); \
        sceGnmSubmitDone(); \
        if (flip_ev_ok) sceVideoOutDeleteFlipEvent(flip_eq, video); \
        if (eq_created) sceKernelDeleteEqueue(flip_eq); \
        sceVideoOutUnregisterBuffers(video, 0); \
        sceVideoOutClose(video); \
        return 1; \
    } while (0)

    /* Flip-done label base. RE of libSceGnmDriver shows the PS4 buffer-label
       protocol: WAIT_REG_MEM(label[bi]==0) before rendering into bi, WRITE_DATA
       (label[bi]=1) after, display clears it to 0 on flip completion. A real
       game drives this via gnm markers on the SubmitAndFlip path. We use the
       plain CPU flip, so it is unverified whether the labels move for us; read
       them each frame to find out before relying on them. */
    void *flip_label_base = 0;
    int label_ok = (sceVideoOutGetBufferLabelAddress(video, &flip_label_base) >= 0) &&
                   (flip_label_base != 0);

    trace_init();
    { char hb[96]; int p=0;
      const char *h="=== trace start flip_ev_ok=";
      for(const char*q=h;*q;q++) hb[p++]=*q;
      p+=lg_i64(hb+p, flip_ev_ok);
      const char *h2=" label_ok=";
      for(const char*q=h2;*q;q++) hb[p++]=*q;
      p+=lg_i64(hb+p, label_ok);
      const char *h3=" label_base=";
      for(const char*q=h3;*q;q++) hb[p++]=*q;
      p+=lg_hex(hb+p,(unsigned long long)(uintptr_t)flip_label_base); hb[p++]='\n';
      trace_line(hb,p); }

    void *depth=gpu_alloc(fb_size,0x10000);

    /* Shadow map: SHADOW_W x SHADOW_H (~64 MB). */
    unsigned long shadow_size = (unsigned long)SHADOW_W * SHADOW_H * 4UL;
    void *shadow_depth = gpu_alloc(shadow_size, 0x100000);
    /* Start from the shadow-clear value, not zeros: ps_shadow_clear writes
       (1,0,0,1) = R 1.0 "nothing occluding" = 0xFF0000FF per RGBA8 texel. Without
       the shadow pass a zero map made every LessEqual compare
       inside the light frustum fail - a dark rectangle with no caster. */
    if (shadow_depth) {
        uint32_t *sp = (uint32_t *)shadow_depth;
        for (unsigned long i = 0; i < shadow_size / 4; i++) sp[i] = 0xFF0000FFu;
    }
    /* GPU shadow pass writes to this buffer during rendering. */
    /* Light-space MVP will be written to vb+LIGHT_MVP_OFF each frame.
       Shadow VS reads from V# base + 0x00 (same as normal VS), but we swap
       what's at that offset for the shadow pass. */

    /* ==== §15.2 Assets: music, textures, UI and clock atlases, atmosphere ===================== */
    /* Background music: bgm.wav (tools/make_bgm.py), looped on its own thread
       from here on - through the loading screen. ONION: the thread reads it on
       the CPU. */
    {
        int r = bgm_start(ASSET_DIR "sound/bgm/bgm.wav", cpu_alloc);
        printf("bgm: %d\n", r);
        char L[64];
        int p = 0;
        const char* m = "bgm_start: ";
        while (*m)
            L[p++] = *m++;
        p += lg_i64(L + p, r);
        L[p++] = '\n';
        trace_line(L, (unsigned long)p);
        ls_file(ASSET_DIR "sound/bgm/bgm.wav");
    }
    mem_report("start");

    /* Textures: DDS from the package (tools/make_textures.py, src/dds_loader.h):
       images/floor/ and images/cube/ hold albedo (BC1 sRGB; the logo stays
       RGBA8 sRGB), normal (BC5: x, y; the shaders rebuild z; stored in the
       engine convention +u / +v) and height (BC4, stretched to 0..1). Fallbacks
       keep everything drawable: the embedded logo for the cube, mid-grey for the
       floor, a flat normal and a white height (= parallax off). */
    static const unsigned char k_grey[4] = {128, 128, 128, 255};
    static const unsigned char k_flat[4] = {128, 128, 255, 255};
    static const unsigned char k_white[4] = {255, 255, 255, 255};
    Tex cube_alb = load_tex(ASSET_DIR "images/cube/albedo.dds", k_white, 9);
    if (cube_alb.err) {
        cube_alb.pixels = gpu_alloc(LOGO_SIZE, 0x1000);
        my_memcpy(cube_alb.pixels, logo_rgba, LOGO_SIZE);
        cube_alb.w = LOGO_WIDTH;
        cube_alb.h = LOGO_HEIGHT;
        cube_alb.tile = 8;
    }
    Tex cube_nrm = load_tex(ASSET_DIR "images/cube/normal.dds", k_flat, 0);
    Tex cube_hgt = load_tex(ASSET_DIR "images/cube/height.dds", k_white, 0);
    Tex floor_alb = load_tex(ASSET_DIR "images/floor/albedo.dds", k_grey, 9);
    {
        int ok = tex_average_linear(&floor_alb, g_floor_albedo);
        char L[96];
        int p = 0;
        const char* m = "floor albedo average (linear x1000): ";
        while (*m)
            L[p++] = *m++;
        for (int c = 0; c < 3; c++) {
            p += lg_i64(L + p, (long long)(g_floor_albedo[c] * 1000.0f + 0.5f));
            L[p++] = c < 2 ? ' ' : (ok ? '\n' : '?');
        }
        if (!ok)
            L[p++] = '\n';
        trace_line(L, (unsigned long)p);
    }
    /* Lens flare rays (tools/make_glare.py); black fallback = no rays. */
    static const unsigned char k_black[4] = {0, 0, 0, 0};
    Tex glare_tex = load_tex(ASSET_DIR "images/flare/glare.dds", k_black, 9);
    /* On-screen panels (src/ui.h): atlas + triple-buffered UI texture. */
    int ui_err = ui_init();
    g_ui.topt = OPCODE_TEST >= 3 && OPCODE_TEST <= 6 ? OPCODE_TEST : 0;
    if (g_ui.topt >= 5)
        ui_o5_layout();
    else if (g_ui.topt)
        ui_optest_layout();
    ls_file(ASSET_DIR "ui/ui_atlas.bin");
    ts_init();
    clock_rim_init();
    int clock_err = clock_init();
    ls_file(ASSET_DIR "ui/clock_sdf.bin");
    {
        char L[80];
        int p = 0;
        const char* m = "ui_init: ";
        while (*m)
            L[p++] = *m++;
        p += lg_i64(L + p, (long long)ui_err);
        m = " clock_init: ";
        while (*m)
            L[p++] = *m++;
        p += lg_i64(L + p, (long long)clock_err);
        m = " rim: ";
        while (*m)
            L[p++] = *m++;
        p += lg_i64(L + p, (long long)g_clock_rim_n);
        L[p++] = '\n';
        trace_line(L, (unsigned long)p);
    }
    Tex floor_nrm = load_tex(ASSET_DIR "images/floor/normal.dds", k_flat, 0);
    Tex floor_hgt = load_tex(ASSET_DIR "images/floor/height.dds", k_white, 0);
    Tex moon_alb =
        load_tex(ASSET_DIR "images/moon/albedo.dds", k_grey, 9); /* NASA LROC, near side */
    g_atmo_ok = load_atmosphere();
    ls_file(ASSET_DIR "sky/atmosphere.bin");
    trace_msg(g_atmo_ok ? "sky: atmosphere.bin loaded\n"
                        : "sky: atmosphere.bin MISSING - black sky\n");
    if (!g_atmo_atlas)
        FATAL_EXIT("sky atlas alloc failed");

    /* ==== §15.3 GPU buffers: descriptors, vertices, tables ==================================== */
    /* desc buffer layout:
         desc[0..7]    texture T#              (byte 0..31)
         desc[8..11]   sampler S#              (byte 32..47)
         desc[12..15]  sun direction           (byte 48..63)
         desc[16..19]  sun NDC position        (byte 64..79)
         desc[20..23]  light color RGBA        (byte 80..95)
         desc[24..27]  sky zenith RGBA         (byte 96..111)
         desc[28..31]  sky horizon RGBA        (byte 112..127)
         desc[40..47]  shadow T#               (byte 160..191)
         desc[48..63]  light_MVP 4x4           (byte 192..255)
         desc[64..71]  floor albedo T#         (byte 256..287)
         desc[72..79]  floor normal T#         (byte 288..319)
       512 bytes = 128 dwords gives headroom. */
    /* Also ONION: the descriptor table is rebuilt by the CPU and read by the
       GPU in the same frame. */
    uint32_t* desc = (uint32_t*)gpu_alloc_typed(
        1024, 0x100, MEM_TYPE_ONION); /* 256 dwords: ps_model uses up to desc[163] */
    if (!desc) FATAL_EXIT("descriptor alloc failed");
    g_gpu_ts = (volatile uint64_t *)gpu_alloc_typed(0x1000, 0x100, MEM_TYPE_ONION);
    build_tsharp_tex(desc, &cube_alb); /* sRGB: colour texture -> linear on sampling */
    build_ssharp_aniso(desc+8);   /* 16× anisotropic — used by cube + floor */
    /* Second sampler at desc[80..83]: PCF depth-compare sampler for the floor
       PS (CAFE100C) projective shadow lookup. Works in tandem with:
         - Shadow T# at desc[40..47] (R32_FLOAT, shadPS4 promotes to depth view
           when the shader issues IMAGE_SAMPLE_C_LZ via is_depth inference)
         - Shadow DB cleared to 1.0 every frame (DEPTH_CLEAR=1.0f, LESS compare)
         - Floor PS computes fragment's z_ref in light-clip NDC space
       The sampler's depth_compare_func=LessEqual gives "ref <= stored ? lit".
       border_color_type=White returns 1.0 for out-of-frustum UVs so that
       unmapped areas of the floor are lit, not shadowed. */
    build_ssharp_pcf(desc+80);
    /* Sun direction at desc[12:15] — PS reads via s_load at offset 0x0C */
    { float *sun = (float*)(desc + 12);
      sun[0] = my_sin(3.14f)*0.766f; sun[1] = 0.643f; sun[2] = my_cos(3.14f)*0.766f; sun[3] = 0; }
    /* Initial sun screen pos (updated per frame in render loop) */
    { float *sd = (float*)(desc + 16);
        sd[0] = 0.5f;
        sd[1] = 0.5f;
        sd[2] = 0.006f;
        sd[3] = 0.0f;
        /* moon disc off-screen, radius^2 > 0; disc colours zero until the loop */
        sd[4] = 99.0f;
        sd[5] = 99.0f;
        sd[6] = 0.006f;
        sd[7] = 0.0f;
        for (int q = 84; q < 92; q++)
            desc[q] = 0;
    }

    /* Shadow map T# at desc[40..47]. Shadow pass writes shadow_depth as a COLOR
       target with CB_INFO.linear_general=1. We sample it as DisplayLinearAligned
       (tile_mode=8) because tile_mode=31 (DisplayLinearGeneral) crashes shadPS4
       in image_info.cpp:200 UpdateSize — ArrayLinearGeneral isn't handled in
       that switch. Both "linear" array modes are row-major pixel arrays; the
       aligned variant just requires pitch alignment which 512 satisfies. */
    if (shadow_depth) {
        /* Shadow T#: RGBA8, the shadow pass's CB0 format, so shadPS4 keeps one image for the write
           and the floor's read (a depth / colour format mismatch would split them in two).
           build_tsharp: 8_8_8_8 UNORM, tile mode 8. */
        build_tsharp(desc + 40, shadow_depth, SHADOW_W, SHADOW_H);
    }

    /* Floor T#s: albedo desc[64], normal desc[72], height desc[92] (sampler desc[100]). */
    build_tsharp_tex(desc + 64, &floor_alb);
    /* ps_dark: the sky atlases desc[204..227] + their bilinear clamp S# [228], the moon albedo
       [232] + a trilinear S# [240]; sky constants [164..203] for a default camera until the frame
       loop sets them (the loading frames draw the sky too). */
    for (int t = 0; t < 3; t++)
        build_tsharp_f16(desc + 204 + 8 * t,
                         (char*)g_atmo_atlas + (unsigned long)t * ATMO_SKY_W * g_atmo_atlas_h * 8,
                         ATMO_SKY_W, g_atmo_atlas_h, ATMO_SKY_W);
    build_ssharp_clamp(desc + 228, 1);
    build_tsharp_tex(desc + 232, &moon_alb);
    build_ssharp_height(desc + 240);
    /* ps_floor's globe lighting: the transmittance table T# desc[244] (sampler desc[228]), the
       moonlight colour (linear) and MOON_LIGHT desc[117..120], 1 / FLOOR_R desc[252]. Without the
       asset the table is white (1.0 = 0x3C00): the floor keeps a neutral light. */
    g_trans_table = (uint16_t*)gpu_alloc_typed(ATMO_TRANS_N * 8, 0x100, MEM_TYPE_ONION);
    if (!g_trans_table)
        FATAL_EXIT("transmittance table alloc failed");
    if (g_atmo_ok)
        atmo_trans_table(&g_atmo, g_trans_table);
    else
        for (int i = 0; i < ATMO_TRANS_N * 4; i++)
            g_trans_table[i] = 0x3C00;
    build_tsharp_f16(desc + 244, g_trans_table, ATMO_TRANS_N, 1, ATMO_TRANS_N);
    {
        float* ml = (float*)(desc + 117);
        for (int c = 0; c < 3; c++)
            ml[c] = srgb_to_linear(k_moon_light_srgb[c]);
        ml[3] = MOON_LIGHT;
        float y = 0.2126f * ml[0] + 0.7152f * ml[1] + 0.0722f * ml[2];
        for (int c = 0; c < 3; c++)
            g_moon_tint[c] = ml[c] / y;
        ((float*)desc)[252] = 1.0f / FLOOR_R;
    }
    {
        const float F[3] = {0.0f, 0.0f, -1.0f}, R[3] = {1.0f, 0.0f, 0.0f},
                    U[3] = {0.0f, 1.0f, 0.0f};
        const float sun[3] = {0.0f, 0.70710678f, -0.70710678f},
                    moon[3] = {0.0f, -0.70710678f, 0.70710678f};
        const float mv[3] = {0.0f, 0.0f, 1.0f};
        const float tilt[3] = {0.0f, 0.0f, 0.0f}, ground[4] = {0.0f, 0.0f, 0.0f, 1.0f / FLOOR_R};
        atmo_sky_consts(&g_atmo, (float*)(desc + 164), F, R, U, my_sin(0.3054f) / my_cos(0.3054f),
                        tilt, ground, sun, SKY_SUN_SCALE, moon, MOON_SKY_SCALE, mv);
        for (int c = 0; c < 3; c++)
            ((float*)(desc + 164))[30 + c] = MOON_SKY_SCALE * g_moon_tint[c];
    }
    build_tsharp_tex(desc + 72, &floor_nrm);
    /* ps_floor parallax + fog: camera desc[104] (xyz per frame, w = log2(FOG_MIN)),
       then (pom_scale, fog rate, h_scale, h_bias) desc[108] and (normal sign x,
       sign y, 1/(POM_FADE_END - POM_FADE_START), 1/DISPLAY_H) desc[112], and
       POM_FADE_END / (POM_FADE_END - POM_FADE_START) desc[116] (ps_floor: the fade is
       clamp(desc[116] - d * desc[114], 0, 1): 1 up to START, 0 from END). Fog (ps_floor, ps_shader,
       ps_model): weight = min(1, 2^(rate * d + log2(FOG_MIN))) = min(1, FOG_MIN *
       e^(d / L)), rate = log2(1 / FOG_MIN) / FOG_FULL. Heights arrive stretched
       to 0..1 and normals in the engine convention (tools/make_textures.py). */
    build_tsharp_tex(desc + 92, &floor_hgt);
    build_ssharp_height(desc + 100);
    {
        float* fc = (float*)(desc + 104);
        fc[0] = 0.0f;
        fc[1] = 0.0f;
        fc[2] = 0.0f;
        fc[3] =
            my_log2(FOG_MIN); /* -200 once ps_resolve does the fog (after the MSAA allocation) */
        fc[4] = floor_hgt.err ? 0.0f : POM_DEPTH * (FLOOR_UV_MAX / (2.0f * FLOOR_HALF));
        fc[5] = -my_log2(FOG_MIN) / FOG_FULL;
        fc[6] = floor_hgt.err ? 0.0f : 1.0f;
        fc[7] = floor_hgt.err ? 1.0f : 0.0f;
        fc[8] = 1.0f;
        fc[9] = 1.0f;
        fc[10] = 1.0f / (POM_FADE_END - POM_FADE_START);
        fc[11] = 1.0f / (float)DISPLAY_H;
        fc[12] = POM_FADE_END / (POM_FADE_END - POM_FADE_START); /* desc[116] */
    }
    /* Model maps: normal T# desc[128] (sampler desc[8]), height T# desc[136]
       (sampler desc[100]); constants desc[144] = (pom_scale, h_scale, h_bias,
       relief) and desc[148] = (normal sign x, sign y, shadow offset, shadow depth
       bias). Without a height map parallax and relief are off. */
    build_tsharp_tex(desc + 128, &cube_nrm);
    build_tsharp_tex(desc + 136, &cube_hgt);
    {
        float* mc = (float*)(desc + 144);
        mc[0] = cube_hgt.err ? 0.0f : MODEL_POM_DEPTH;
        mc[1] = cube_hgt.err ? 0.0f : 1.0f;
        mc[2] = cube_hgt.err ? 1.0f : 0.0f;
        mc[3] = cube_hgt.err ? 0.0f : MODEL_RELIEF;
        mc[4] = 1.0f;
        mc[5] = 1.0f;
        mc[6] = MODEL_SHADOW_OFFSET;
        mc[7] = MODEL_SHADOW_BIAS;
        /* Satin finish (ps_model desc[152..157]): a = roughness^2; a^2, k = a / 2, sun
           Fresnel F0 + (1 - F0) (1 - V.H)^5, sky Fresnel F0 + (max(1 - r, F0) - F0)
           (1 - N.V)^5. */
        float a = MODEL_ROUGHNESS * MODEL_ROUGHNESS;
        float g = 1.0f - MODEL_ROUGHNESS;
        mc[8] = a * a;
        mc[9] = 0.5f * a;
        mc[10] = MODEL_F0;
        mc[11] = 1.0f - MODEL_F0;
        mc[12] = MODEL_F0;
        mc[13] = (g > MODEL_F0 ? g : MODEL_F0) - MODEL_F0;
        mc[14] = 0.5f / a; /* desc[158]: the ground / sky blend spans R.y in [-a, a] */
        mc[15] = 0.0f;
    }

    /* VB layout:
       - Main region: [ident][BG sun][BG verts][MVP][sun dir][cube verts]
       - Shadow uses a separate VB: [light_MVP @ 0x00][pad @ 0x40][vert copy @ 0x50].
         For huge models, we skip shadow VB allocation and disable shadow entirely. */
    /* Shadow VB layout: [light_MVP @ 0x00][pad @ 0x40][cube verts @ 0x50]
       [floor verts @ cube_end]. Both cube AND floor render into shadow map
       so the curved floor self-shadows when the curve creates ridges
       blocking the sun. */
    unsigned long shadow_vb_size = 0x50
                                 + (unsigned long)CUBE_VERTS * VERT_STRIDE
                                 + (unsigned long)FLOOR_VERTS * VERT_STRIDE
                                 + 256;
    /* ONION: the CPU writes the light MVP and copies the rotated cube

       vertices into this buffer every frame, same as vb. */

    void *shadow_vb = gpu_alloc_typed(shadow_vb_size, 0x1000, MEM_TYPE_ONION);
    if (!shadow_vb) shadow_depth = 0;
    /* ONION, not GARLIC. The CPU rewrites this buffer EVERY FRAME - the MVP
       matrix, the rotated cube vertices, the sun direction and the floor MVP
       mirror - and the GPU then reads it. GARLIC is CPU write-combine: those
       stores drain asynchronously and out of order, with nothing in our frame
       guaranteeing they land before the command processor fetches the data.
       ONION is write-back and CPU<->GPU coherent, which is why the game keeps
       its per-frame data there (467 ONION allocations vs 163 GARLIC) and never
       needs sceGnmFlushGarlic. GARLIC stays correct for write-once, GPU-read
       data: render targets, textures, static geometry. */
    void *vb=gpu_alloc_typed(VERT_BUF_SIZE + 256, 0x1000, MEM_TYPE_ONION);
    /* The static layout (sky, cube, floor) lives here for the whole run. After an
       OBJ load vb switches to the model's buffer, whose vertices start at
       OBJ_DATA_OFF (448) == CUBE_DATA_OFF; the floor's V# keeps pointing here. */
    char* vb_static = (char*)vb;
    build_static_vb((float*)vb); // uploaded ONCE
    uint32_t vb_v[4], bg_v[4], shadow_vb_v[4], shadow_floor_v[4], floor_v[4];
    build_vsharp(vb_v, vb, VERT_BUF_SIZE);
    build_vsharp(bg_v, vb, VERT_BUF_SIZE);
    build_vsharp(shadow_vb_v, shadow_vb, (uint32_t)shadow_vb_size);
    /* Shadow-floor V# points at the same shadow_vb (same light_MVP at offset 0)
       but its vertex data starts AFTER the cube verts. shadPS4 VS reads the
       MVP from V#+0 and verts from V#+0x50 (constant), so we offset the V#
       base so that V#+0x50 lands on the floor vert region.
       Effective base = shadow_vb + (cube_size). MVP is at shadow_vb+0; we
       can't have MVP at offset 0 from a different base unless we duplicate it.
       Solution: copy light_MVP into a SECOND header at the start of the
       floor region (so floor base = shadow_vb + cube_size has MVP at +0). */
    unsigned long shadow_floor_base = 0x50
                                    + (unsigned long)CUBE_VERTS * VERT_STRIDE;
    /* Round up to 0x80 alignment so the V# load with offset 0x50 doesn't
       cross a cache line boundary in unexpected ways. */
    if (shadow_floor_base & 0x7F) shadow_floor_base = (shadow_floor_base + 0x7F) & ~0x7Ful;
    /* shadow_floor_v base = shadow_vb + shadow_floor_base - 0x50, so:
        - V# + 0x00 = shadow_vb + shadow_floor_base - 0x50  (MVP — needs duplicate)
        - V# + 0x50 = shadow_vb + shadow_floor_base        (floor verts)
       That means MVP must be duplicated 0x50 bytes BEFORE the floor verts.
       Easier: place MVP duplicate at offset (shadow_floor_base - 0x50) and
       point V# at (shadow_vb + shadow_floor_base - 0x50). */
    unsigned long shadow_floor_mvp_off = shadow_floor_base - 0x50;
    build_vsharp(shadow_floor_v,
                 (char*)shadow_vb + shadow_floor_mvp_off,
                 (uint32_t)(shadow_vb_size - shadow_floor_mvp_off));
    /* Floor V#: base at vb+FLOOR_MVP_OFF. Size covers MVP (64B) + padding + verts.
       Must be >= 80 + FLOOR_VERTS*VERT_STRIDE = 80 + 384*48 = 18512 bytes. */
    build_vsharp(floor_v, (char*)vb + FLOOR_MVP_OFF,
                 (uint32_t)(VERT_BUF_SIZE - FLOOR_MVP_OFF));
    /* Star buffer: MVP mirrored at +0 each frame (MVP VS convention), sun slot
       at +64 unused, vertices at +80. */
    unsigned long stars_size = 80UL + (unsigned long)STARS_N * 6UL * VERT_STRIDE;
    char* stars_vb = (char*)gpu_alloc_typed(stars_size + 256, 0x1000, MEM_TYPE_ONION);
    if (stars_vb) {
        for (int q = 0; q < 20; q++)
            ((float*)stars_vb)[q] = 0.0f;
        g_stars_n = build_stars((float*)(stars_vb + 80));
        build_vsharp(g_stars_v, stars_vb, (uint32_t)stars_size);
    }
    /* ==== §15.4 Post-processing and MSAA buffers ============================================== */
    /* Post-processing targets (fully rewritten every frame) and tables. */
    g_hdr = gpu_alloc((unsigned long)DISPLAY_W * DISPLAY_H * 8, 0x10000);
    g_frame = gpu_alloc((unsigned long)DISPLAY_W * DISPLAY_H * 8, 0x10000);
    int post_ok = g_hdr != 0 && g_frame != 0;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        unsigned long sz = (unsigned long)g_bloom_pitch[i] * g_bloom_h[i] * 8;
        g_bloom_a[i] = gpu_alloc(sz, 0x10000);
        g_bloom_b[i] = gpu_alloc(sz, 0x10000);
        post_ok = post_ok && g_bloom_a[i] && g_bloom_b[i];
    }
    g_clock_light_rt = gpu_alloc((unsigned long)g_bloom_pitch[0] * g_bloom_h[0] * 8, 0x10000);
#if MSAA_SAMPLES > 1
    if (post_ok) {
        g_msaa_color = gpu_alloc((unsigned long)DISPLAY_W * DISPLAY_H * 8 * MSAA_SAMPLES, 0x10000);
        g_msaa_depth = gpu_alloc((unsigned long)DISPLAY_W * DISPLAY_H * 4 * MSAA_SAMPLES, 0x10000);
        if (!g_msaa_color || !g_msaa_depth)
            g_msaa_color = 0; /* single-sample fallback */
    }
#endif
    /* With MSAA the resolve applies the physically based fog to every sample: switch off the
       per-shader fog of ps_floor / ps_model / ps_shader (their weight 2^(rate d + desc[107]) = 0).
     */
    if (g_msaa_color)
        ((float*)desc)[107] = -200.0f;
    trace_msg(g_msaa_color ? "msaa: 4x (colour 66355200 B, depth 33177600 B, tile 13)\n"
                           : "msaa: off\n");
    g_post_tab = (uint32_t*)gpu_alloc_typed(POST_TABLE_BLOCKS * 32 * 4, 0x100, MEM_TYPE_ONION);
    g_clock_entries = (float*)gpu_alloc_typed(3 * CLOCK_RIM_MAX * 32, 0x100, MEM_TYPE_ONION);
    if (!g_clock_entries)
        g_clock.ok = 0; /* no clock mode */
    g_hz_table =
        (uint16_t*)gpu_alloc_typed(ATMO_SKY_W * 3 * ATMO_SLICES * 8, 0x100, MEM_TYPE_ONION);
    if (g_hz_table) { /* static: ps_resolve picks each pixel's slices (black without the asset) */
        my_memset(g_hz_table, 0, ATMO_SKY_W * 3 * ATMO_SLICES * 8);
        if (g_atmo_ok)
            atmo_horizon_texture(g_hz_table);
    }
    if (post_ok && g_post_tab) {
        build_post_tables(g_post_tab);
        /* Final pass (ps_post_final): glare T# at dwords 40..47, bilinear clamp S# at 48..51. */
        uint32_t* fin = g_post_tab + (POST_PASSES - 1) * 32;
        build_tsharp_tex(fin + 40, &glare_tex);
        build_ssharp_clamp(fin + 48, 1);
        /* UI pass slots (28..47 of its block) valid before the first frame (the loading screen
           runs the post chain too): hidden panels and a real T# - the UI buffer, or the glare
           texture if the UI failed - so no path through ps_ui can sample a null descriptor. */
        uint32_t* uib = g_post_tab + UI_BLOCK * 32;
        build_tsharp_tex(uib + 36, &glare_tex);
        build_ssharp_clamp(uib + 44, 0);
        ui_write_table(uib);
        { /* OPCODE_TEST (ps_ui 64..95): test 1 panels, field widths, (1 << bits) - 1 and
             reciprocals; the cube albedo T# as UNORM (NUM_FORMAT word1[29:26] = 0: the sRGB bytes
             as they are) with a point S#; test 2 panels, field masks, junk word, 65535 and its
             reciprocal. The T# is always valid; x0 -1e6 hides a test. */
            float* f = (float*)(uib + 64);
            f[0] = OPCODE_TEST == 1 ? DISPLAY_W - 16.0f - 2 * 256.0f - 16.0f : -1e6f;
            f[1] = OPCODE_TEST == 1 ? DISPLAY_H - 16.0f - 256.0f : -1e6f;
            f[2] = 256.0f;
            f[3] = 256.0f + 16.0f;
            uib[68] = 8;
            uib[69] = 16;
            f[6] = 255.0f;
            f[7] = 65535.0f;
            f[8] = 1.0f / 255.0f;
            f[9] = 1.0f / 65535.0f;
            f[10] = 1.0f / 256.0f;
            f[11] = 0.0f;
            build_tsharp_tex(uib + 76, &cube_alb);
            uib[77] &= ~(0xFu << 26);
            build_ssharp_clamp(uib + 84, 0);
            f[24] = OPCODE_TEST == 2 ? DISPLAY_W - 16.0f - 2 * 256.0f - 16.0f : -1e6f;
            f[25] = OPCODE_TEST == 2 ? DISPLAY_H - 16.0f - 256.0f : -1e6f;
            uib[90] = 31;
            uib[91] = 3;
            uib[92] = 0x55555555u;
            f[29] = 65535.0f;
            f[30] = 1.0f / 65535.0f;
            f[31] = 0.0f;
            static const uint32_t k_pku8_rows[28] = {0x00000000u /* 0.0 */,
                                                     0x3F000000u /* 0.5 */,
                                                     0x3FC00000u /* 1.5 */,
                                                     0x40200000u /* 2.5 */,
                                                     0x42FECCCDu /* 127.4 */,
                                                     0x42FF0000u /* 127.5 */,
                                                     0x42FF3333u /* 127.6 */,
                                                     0x43008000u /* 128.5 */,
                                                     0x437E8000u /* 254.5 */,
                                                     0x437F0000u /* 255.0 */,
                                                     0x437F8000u /* 255.5 */,
                                                     0x43800000u /* 256.0 */,
                                                     0x43960000u /* 300.0 */,
                                                     0x447A0000u /* 1000.0 */,
                                                     0xBECCCCCDu /* -0.4 */,
                                                     0xBF800000u /* -1.0 */,
                                                     0xC3960000u /* -300.0 */,
                                                     0x7F800000u /* +inf */,
                                                     0xFF800000u /* -inf */,
                                                     0x7FC00000u /* NaN */,
                                                     0u /* S1 */,
                                                     1u /* S1 */,
                                                     2u /* S1 */,
                                                     3u /* S1 */,
                                                     4u /* S1 */,
                                                     5u /* S1 */,
                                                     7u /* S1 */,
                                                     0xFFFFFFFFu /* S1 */};
            my_memcpy(uib + 96, k_pku8_rows, sizeof(k_pku8_rows));
            f[60] = OPCODE_TEST == 3 ? (float)g_ui.tgx : -1e6f;
            f[61] = OPCODE_TEST == 3 ? (float)g_ui.tgy : -1e6f;
            f[62] = 1.0f / UI_TCELL_W;
            f[63] = 1.0f / UI_T3CELL_H;
            static const uint32_t k_optest4_data[85] = {
                0x00001234u, 0x0000ABCDu,              /* PK_U16 0x1234, 0xABCD */
                0x0000FFFFu, 0x00010000u,              /* PK_U16 0xFFFF, 0x10000 */
                0x00011170u, 0xFFFFFFFFu,              /* PK_U16 70000, 0xFFFFFFFF */
                0x80000000u, 0x00000001u,              /* PK_U16 0x80000000, 1 */
                0x00000001u, 0xFFFFFFFFu,              /* PK_I16 1, -1 */
                0x00007FFFu, 0x00008000u,              /* PK_I16 32767, 32768 */
                0xFFFF8000u, 0xFFFF7FFFu,              /* PK_I16 -32768, -32769 */
                0x7FFFFFFFu, 0x80000000u,              /* PK_I16 0x7FFFFFFF, 0x80000000 */
                0x00000000u, 0x3F800000u,              /* PKNORM_U16 0.0, 1.0 */
                0x3E800000u, 0x3F400000u,              /* PKNORM_U16 0.25, 0.75 */
                0xBF000000u, 0x3FC00000u,              /* PKNORM_U16 -0.5, 1.5 */
                0x7FC00000u, 0x7F800000u,              /* PKNORM_U16 NaN, +inf */
                0x37000080u, 0x37C000C0u,              /* PKNORM_U16 0.5/65535, 1.5/65535 */
                0x382000A0u, 0x3F000000u,              /* PKNORM_U16 2.5/65535, 32767.5/65535 */
                0x00000000u, 0x3F800000u,              /* PKNORM_I16 0.0, 1.0 */
                0xBF800000u, 0x3F000000u,              /* PKNORM_I16 -1.0, 0.5 */
                0xBFC00000u, 0x40000000u,              /* PKNORM_I16 -1.5, 2.0 */
                0x7FC00000u, 0xFF800000u,              /* PKNORM_I16 NaN, -inf */
                0x37800100u, 0x38400180u,              /* PKNORM_I16 0.5/32767, 1.5/32767 */
                0xB7800100u, 0xB8A00140u,              /* PKNORM_I16 -0.5/32767, -2.5/32767 */
                0x00000000u, 0x00000000u, 0x00000028u, /* BITSET1_B64 0, 40 */
                0x00000000u, 0x00000000u, 0x00000046u, /* BITSET1_B64 0, 70 */
                0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000021u, /* BITSET0_B64 ~0, 33 */
                0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFC0u, /* BITSET0_B64 ~0, 0xFFFFFFC0 */
                0x00000000u, 0x80000000u, 0x00000024u, /* ASHR_I64 0x80000000_00000000 >> 36 */
                0xFFFFFFFFu, 0x7FFFFFFFu, 0x00000044u, /* ASHR_I64 0x7FFFFFFF_FFFFFFFF >> 68 */
                0x12345678u, 0x87654321u, 0x00000000u, /* ASHR_I64 0x87654321_12345678 >> 0 */
                0x00000000u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u,
                0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u,
                0x00000000u, 0x00000000u, 0x00000005u, 0x00000000u, 0x00000005u,
                0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, /* CMPX cases: a lo, a hi, b lo,
                                                                       b hi x 6 */
            };
            my_memcpy(uib + 128, k_optest4_data, sizeof(k_optest4_data));
            f[188] = OPCODE_TEST == 4 ? (float)g_ui.tgx : -1e6f;
            f[189] = OPCODE_TEST == 4 ? (float)g_ui.tgy : -1e6f;
            f[190] = 1.0f / UI_TCELL_W;
            f[191] = 1.0f / UI_T4CELL_H;
            /* OPCODE_TEST 5 ([52] on; its 128..175 overlap test 4's data, unused then): the slot
               table, the results (A and B per slot, then the two markers), the passes' target and
               tables, the hex digit strip (ui_glyph_s digits centred in o5dw px cells, baseline as
               the rows'). */
            uib[52] = 0;
            if (OPCODE_TEST >= 5 && g_ui.ok) {
                int n = g_ui.o5n, dw = g_ui.o5dw;
                g_o5.slots = (uint32_t*)gpu_alloc_typed(n * 16 + 256, 256, MEM_TYPE_ONION);
                g_o5.res = (uint32_t*)gpu_alloc_typed(n * OPT5_STRIDE + 256, 256, MEM_TYPE_ONION);
                g_o5.tab = (uint32_t*)gpu_alloc_typed(16 * 4 * OPT5_MODES, 256, MEM_TYPE_ONION);
                g_o5.rt = gpu_alloc_typed(OPT5_RT_W * 4, 256, MEM_TYPE_GARLIC);
                g_o5.strip = gpu_alloc_typed(256 * 16 * 4, 256, MEM_TYPE_ONION);
                if (g_o5.slots && g_o5.res && g_o5.tab && g_o5.rt && g_o5.strip) {
                    for (int i = 0; i < n; i++) {
                        int sl = g_ui.o5slot[i];
                        for (int j = 0; j < 4; j++)
                            g_o5.slots[4 * i + j] =
                                sl < 0 ? (j ? 0u : 0xFFu)
                                : j    ? k_opt5_row[sl][j]
                                       : k_opt5_row[sl][0] |
                                          (k_opt5_wide[k_opt5_row[sl][0]] ? 0x100u : 0u);
                    }
                    my_memset(g_o5.res, 0, n * OPT5_STRIDE + 256);
                    ui_o5_strip((unsigned char*)g_o5.strip);
                    for (int h = 0; h < OPT5_MODES; h++) { /* pass h: float mode h at + 8 h */
                        uint32_t* t = g_o5.tab + 16 * h;
                        build_vsharp(t, g_o5.slots, n * 16);
                        build_vsharp(t + 4, g_o5.res, n * OPT5_STRIDE + 4 * OPT5_MODES);
                        t[8] = 8u * (uint32_t)h;
                        t[9] = (uint32_t)n;
                        t[10] = OPT5_MARK(h);
                        t[11] = OPT5_STRIDE;
                    }
                    build_vsharp(uib + 128, g_o5.slots, n * 16);
                    build_vsharp(uib + 132, g_o5.res, n * OPT5_STRIDE + 4 * OPT5_MODES);
                    uib[176] = OPT5_STRIDE; /* ps_ui: result bytes a slot */
                    build_tsharp(uib + 136, g_o5.strip, 256, 16);
                    build_ssharp_clamp(uib + 144, 0);
                    f[84] = (float)UI_O5_Y0;
                    f[85] = (float)UI_O5_PITCH;
                    f[86] = 1.0f / UI_O5_PITCH;
                    f[87] = (float)dw;
                    f[88] = 1.0f / (float)dw;
                    f[89] = 1.0f / 256.0f;
                    f[90] = 1.0f / 16.0f;
                    f[91] = (float)UI_O5_GAP_AB;
                    ui_o5_cols(f);
                    uib[52] = 1;
                    g_o5.ok = 1;
                }
            }
        }
    } else
        g_hdr = 0; /* no bloom: render straight into the sRGB display buffer */
    /* Copy cube verts into shadow VB at offset 0x50 */
    if (shadow_vb) {
        my_memcpy((char*)shadow_vb + 0x50,
                  (char*)vb + CUBE_DATA_OFF,
                  CUBE_VERTS * VERT_STRIDE);
        /* Copy floor verts into shadow VB at shadow_floor_base */
        my_memcpy((char*)shadow_vb + shadow_floor_base,
                  (char*)vb + FLOOR_DATA_OFF,
                  FLOOR_VERTS * VERT_STRIDE);
    }

    // (file loading disabled for testing)




    /* ONION: the CPU memcpys the shader code in and the GPU then FETCHES AND
       EXECUTES it. Instruction fetch from a buffer whose write-combine stores
       may not have drained is not something to leave to chance, and shader
       binaries are tiny and read once - GARLIC's bandwidth buys nothing. */
    void *vs=gpu_alloc_typed(sizeof(vs_shader_binary)+256,0x1000,MEM_TYPE_ONION);
    void *vs_shadow=gpu_alloc_typed(sizeof(vs_shadow_binary)+256,0x1000,MEM_TYPE_ONION);
    void *ps=gpu_alloc_typed(sizeof(ps_shader_binary)+256,0x1000,MEM_TYPE_ONION);
    /* A NULL here would memcpy to address 0 and then have the GPU execute from
       it. Bail cleanly instead - out of GPU memory is a legitimate failure. */
    if (!vs || !vs_shadow || !ps) FATAL_EXIT("shader alloc failed");
    my_memcpy(vs,vs_shader_binary,sizeof(vs_shader_binary));
    my_memcpy(vs_shadow,vs_shadow_binary,sizeof(vs_shadow_binary));
    my_memcpy(ps,ps_shader_binary,sizeof(ps_shader_binary));

/* ==== §15.5 Shaders to GPU memory ========================================================= */
/* Every shader bound in a draw must live in GPU-accessible memory — the GPU
   fetches code from the PGM_LO/HI address. Binding straight from the .rodata
   arrays (CPU-only ELF memory) faults the GPU MMU on real hardware and hangs
   (shadPS4 reads guest memory via its cache, so it never faulted there). */
#define UPLOAD_SHADER(dst, src)                                                                    \
    void* dst = gpu_alloc_typed(sizeof(src) + 256, 0x1000,                                         \
                                MEM_TYPE_ONION); /* shader code: coherent, GPU executes it */      \
    my_memcpy(dst, src, sizeof(src));
    ls_mark("gpu_buffers");
    UPLOAD_SHADER(ps_dark_gpu,          ps_dark_binary);
    UPLOAD_SHADER(ps_floor_gpu,         ps_floor_binary);
    UPLOAD_SHADER(ps_model_gpu, ps_model_binary);
    UPLOAD_SHADER(vs_model_gpu, vs_model_binary);
    UPLOAD_SHADER(vs_model_shadow_gpu, vs_model_shadow_binary);
    g_model.ps = ps_model_gpu;
    g_model.vs = vs_model_gpu;
    g_model.vs_shadow = vs_model_shadow_gpu;
    UPLOAD_SHADER(ps_shadow_gpu,        ps_shadow_binary);
    UPLOAD_SHADER(ps_shadow_clear_gpu,  ps_shadow_clear_binary);
    UPLOAD_SHADER(ps_blue_gpu,          ps_blue_binary);
    UPLOAD_SHADER(ps_stars_gpu, ps_stars_binary);
    g_ps_stars_gpu = ps_stars_gpu;
    UPLOAD_SHADER(ps_post_down_gpu, ps_post_down_binary);
    UPLOAD_SHADER(ps_post_blur_gpu, ps_post_blur_binary);
    UPLOAD_SHADER(ps_post_comp_gpu, ps_post_comp_binary);
    UPLOAD_SHADER(ps_post_final_gpu, ps_post_final_binary);
    g_ps_post_down_gpu = ps_post_down_gpu;
    g_ps_post_blur_gpu = ps_post_blur_gpu;
    g_ps_post_comp_gpu = ps_post_comp_gpu;
    g_ps_post_final_gpu = ps_post_final_gpu;
    UPLOAD_SHADER(ps_ui_gpu, ps_ui_binary);
    for (int m = 0; m < OPT5_MODES; m++) { /* OPCODE_TEST 5 / 6 passes */
        g_o5_ps[m] = gpu_alloc_typed(k_opt5_pass_size[m] + 256, 0x1000, MEM_TYPE_ONION);
        my_memcpy(g_o5_ps[m], k_opt5_pass[m], k_opt5_pass_size[m]);
    }
    UPLOAD_SHADER(ps_clock_gpu, ps_clock_binary);
    g_ps_clock_gpu = ps_clock_gpu;
    UPLOAD_SHADER(ps_clock_light_gpu, ps_clock_light_binary);
    g_ps_clock_light_gpu = ps_clock_light_gpu;
    UPLOAD_SHADER(ps_resolve_gpu, ps_resolve_binary);
    g_ps_resolve_gpu = ps_resolve_gpu;
#undef UPLOAD_SHADER

    uint32_t *dcb_mem[NUM_FRAMES];
    /* NOT ALLOCATED. The shadow pass shares the main pm4 builder and its
       command buffer - build_shadow_dcb writes into the same DCB, which is why
       the inter-pass ACQUIRE_MEM is the barrier. These NUM_FRAMES x DCB_SIZE
       buffers were allocated at startup and never written or submitted. */
    uint32_t *shadow_dcb_mem[NUM_FRAMES];
    /* Command buffers in ONION: the CP reads them and we rewrite them by CPU
       every frame, so they need coherency, not write-combine bandwidth. */
    for (int i=0;i<NUM_FRAMES;i++) {
        dcb_mem[i]=(uint32_t*)gpu_alloc_typed(DCB_SIZE,0x10000,MEM_TYPE_ONION);
        shadow_dcb_mem[i]=0;   /* unused - the shadow pass shares the main DCB */
    }
    /* Fences in ONION: the GPU writes them and the CPU polls them. */
    /* FENCE_SLOTS pages, one fence per page so no two share a cache line.
       fence_base is the allocation; `fence` is re-pointed each frame when
       FENCE_SLOTS > 1, mirroring the game carving a fresh fence per submit. */
    volatile uint32_t *fence_base=(volatile uint32_t*)gpu_alloc_typed(0x1000*FENCE_SLOTS,0x1000,MEM_TYPE_ONION);
    volatile uint32_t *fence = fence_base;
    /* Separate fence for the keep-alive submits so they never interfere with
       the frame fence the flip path writes. */
    volatile uint32_t *keepalive_fence=(volatile uint32_t*)gpu_alloc_typed(0x1000,0x1000,MEM_TYPE_ONION);
    uint32_t keepalive_fv = 0;
    /* The EOP packet writes through these addresses. A NULL fence would make
       the GPU write to address 0, which faults the command processor. */
    if (!fence_base) FATAL_EXIT("fence alloc failed");
    /* Checkpoint slot for the GPU-side stage markers. ONION so the CPU sees
       the CP's writes immediately. Optional: if it fails, CPMARK compiles to
       nothing at runtime and the rest of the app is unaffected. */
    g_cp_mark = (volatile uint32_t*)gpu_alloc_typed(0x1000,0x1000,MEM_TYPE_ONION);
    if (g_cp_mark) *g_cp_mark = 0;
    if (keepalive_fence) *keepalive_fence = 0;
    /* Dedicated command buffers for the keep-alive submits. They must NOT share
       dcb_mem[]: the next frame would overwrite a buffer the GPU could still be
       reading, which is exactly how a command processor gets wedged. Small,
       because a keep-alive DCB is only the default hardware state + one EOP. */
    uint32_t *ka_dcb[KA_SLOTS];
    int ka_ok = 1;
    int ka_slot = 0;
#if KEEP_GPU_FED > 1
    for (int i=0;i<KA_SLOTS;i++) {
        ka_dcb[i]=(uint32_t*)gpu_alloc_typed(0x4000,0x4000,MEM_TYPE_ONION);
        if (!ka_dcb[i]) ka_ok = 0;
    }
#else
    /* KEEP_GPU_FED 1: no keep-alive command buffers. */
    for (int i=0;i<KA_SLOTS;i++) ka_dcb[i]=0;
    ka_ok = 0;
#endif
    *fence=0;



    int model_verts = CUBE_VERTS;
    int model_loaded = 0; /* 1 once an OBJ/STL/PLY replaced the built-in cube */
    uint32_t *g_ib = 0;
    int g_num_idx = 0;
    int g_indexed = 0;
    int g_shadow_verts = CUBE_VERTS; /* always non-indexed draw count for shadow DCB */
    int g_shadow_ready = (shadow_vb != 0) ? 1 : 0;  /* cube data already copied */
    unsigned long g_vb_total = VERT_BUF_SIZE;

    /* Model AABB in world space — used to size light-space ortho for shadow pass.
       Default to built-in cube (±0.4). Updated after OBJ load. */
    float g_model_cx = 0.0f, g_model_cy = 0.0f, g_model_cz = 0.0f;
    float g_model_radius = 0.8f;  /* cube radius ~0.7, round up */
    float model_fit_radius =
        1.0f; /* loaded model: bbox half diagonal (scaled to MODEL_FIT_RADIUS) */

    /* ==== §15.6 Model (OBJ / STL / PLY; else the built-in cube) =============================== */
    {
        /* The prop shipped in the package (tools/gen_model.py); the loader is
           picked by extension (.obj / .stl / .ply). */
        static const char* obj_paths[] = {ASSET_DIR "models/cube/cube.obj", 0};
        ObjMesh mesh;
        int loaded = 0;
        for (int pi = 0; obj_paths[pi] && !loaded; pi++) {
            int err = -1;
            const char *ext = obj_paths[pi];
            while (*ext) ext++;
            /* Find extension */
            const char *dot = ext;
            while (dot > obj_paths[pi] && *dot != '.') dot--;
            if (dot[1]=='o' && dot[2]=='b' && dot[3]=='j') {
                err = obj_load_file(obj_paths[pi], gpu_alloc, &mesh, ls_model_progress, 0);
            } else if (dot[1]=='s' && dot[2]=='t' && dot[3]=='l') {
                err = stl_load_binary(obj_paths[pi], gpu_alloc, &mesh, ls_model_progress, 0);
            } else if (dot[1]=='p' && dot[2]=='l' && dot[3]=='y') {
                err = ply_load_file(obj_paths[pi], gpu_alloc, &mesh, ls_model_progress, 0);
            }
            {
                char L[192];
                int p = 0;
                const char* m = "model load ";
                while (*m)
                    L[p++] = *m++;
                for (const char* q = obj_paths[pi]; *q && p < 150; q++)
                    L[p++] = *q;
                m = " err=";
                while (*m)
                    L[p++] = *m++;
                p += lg_i64(L + p, err);
                m = " verts=";
                while (*m)
                    L[p++] = *m++;
                p += lg_i64(L + p, err == 0 ? mesh.num_verts : 0);
                L[p++] = '\n';
                trace_line(L, (unsigned long)p);
            }
            if (err == 0 && mesh.num_verts > 0) {
                printf("Loaded %s: %d verts, %d tris (decimate=%d)\n",
                       obj_paths[pi], mesh.num_verts, mesh.num_tris, mesh.indexed);
                /* Copy BG quad data into mesh.vb_base */
                my_memcpy(mesh.vb_base, vb, OBJ_DATA_OFF);
                /* BG normals to zero */
                { float *bgr = (float*)((char*)mesh.vb_base + BG_DATA_OFF);
                  for (int bi=0;bi<6;bi++) { bgr[bi*12+4]=0;bgr[bi*12+5]=0;bgr[bi*12+6]=0; } }
                vb = mesh.vb_base;
                if (mesh.indexed) {
                    model_verts = mesh.num_verts; /* unique verts for V# */
                    g_ib = mesh.ib_base;
                    g_num_idx = mesh.num_indices;
                    g_indexed = 1;
                } else {
                    model_verts = mesh.num_verts;
                }
                /* Update V# descriptors for new buffer size */
                unsigned long new_total = OBJ_DATA_OFF + (unsigned long)mesh.num_verts * VERT_STRIDE;
                g_vb_total = new_total;
                build_vsharp(vb_v, vb, (uint32_t)new_total);
                build_vsharp(bg_v, vb, (uint32_t)new_total);
                /* Shadow VB: allocate a parallel buffer with verts copied into it.
                   Layout matches cube-case: [light_MVP][pad][verts]. If alloc fails
                   (huge model), set g_shadow_ready=0 to disable shadow pass cleanly. */
                g_shadow_verts = mesh.num_verts;
                g_shadow_ready = 0;
                {
                    unsigned long new_shadow_size = 0x50 + (unsigned long)mesh.num_verts * VERT_STRIDE + 256;
                    /* Cap at 256MB — huge models skip shadow */
                    if (new_shadow_size <= 0x10000000UL) {
                        void *new_shadow_vb = gpu_alloc_typed(new_shadow_size, 0x1000, MEM_TYPE_ONION); /* must match shadow_vb's type */
                        if (new_shadow_vb) {
                            shadow_vb = new_shadow_vb;
                            my_memcpy((char*)shadow_vb + 0x50,
                                      (char*)vb + OBJ_DATA_OFF,
                                      (unsigned long)mesh.num_verts * VERT_STRIDE);
                            build_vsharp(shadow_vb_v, shadow_vb, (uint32_t)new_shadow_size);
                            g_shadow_ready = 1;
                        }
                    }
                }

                /* Compute model AABB for light-space ortho sizing. Positions at
                   offset 0,4,8 of each 48-byte vertex in vb + OBJ_DATA_OFF. */
                {
                    float *vp = (float*)((char*)vb + OBJ_DATA_OFF);
                    float minx = vp[0], miny = vp[1], minz = vp[2];
                    float maxx = minx, maxy = miny, maxz = minz;
                    for (int vi = 1; vi < mesh.num_verts; vi++) {
                        float x = vp[vi*12+0], y = vp[vi*12+1], z = vp[vi*12+2];
                        if (x < minx) minx = x; if (x > maxx) maxx = x;
                        if (y < miny) miny = y; if (y > maxy) maxy = y;
                        if (z < minz) minz = z; if (z > maxz) maxz = z;
                    }
                    g_model_cx = (minx + maxx) * 0.5f;
                    g_model_cy = (miny + maxy) * 0.5f;
                    g_model_cz = (minz + maxz) * 0.5f;
                    g_prop_lo[0] = minx;
                    g_prop_lo[1] = miny;
                    g_prop_lo[2] = minz;
                    g_prop_hi[0] = maxx;
                    g_prop_hi[1] = maxy;
                    g_prop_hi[2] = maxz;
                    float dx = maxx - minx, dy = maxy - miny, dz = maxz - minz;
                    float diag = my_sqrt(dx*dx + dy*dy + dz*dz);
                    g_model_radius = diag * 0.5f;
                    if (g_model_radius < 0.1f) g_model_radius = 0.1f;
                }
                /* The model moves like the built-in cube (vs_model transform: scaled to
                   the cube's bounding radius, lifted to its height, same rotation), so
                   the light-space framing is the cube's: centre 0, radius 0.8. */
                model_fit_radius = g_model_radius;
                g_model_cx = 0.0f;
                g_model_cy = 0.0f;
                g_model_cz = 0.0f;
                g_model_radius = 0.8f;
                g_model.enabled = 1;
                trace_msg("model draw path enabled (vs_model / ps_model)\n");
                loaded = 1;
                model_loaded = 1;
            }
        }
        if (!loaded) {
            /* No model: rebuild the static VB, the built-in cube. */
            build_static_vb((float*)vb);
            printf("No model found, using built-in cube.\n");
        }
    }

    ls_file(ASSET_DIR "models/cube/cube.obj");
    printf("VB ready: %d model verts. MVP on GPU. Running.\n", model_verts);

    // --- Gamepad init ---
    // 1:1 PS4 sequence: init userservice → get userId → init pad → open pad
    sceUserServiceInitialize(0);
    int userId = 0;
    sceUserServiceGetInitialUser(&userId);
    scePadInit();
    int pad_handle = scePadOpen(userId, 0, 0, 0);
    // pad_handle < 0 = error (PS4 error codes are negative)
    // pad_handle >= 0 = valid handle (0 is valid on real PS4)
    if (pad_handle < 0) {
        printf("scePadOpen failed: 0x%x, userId=%d\n", pad_handle, userId);
    }
    struct OrbisPadData pad;
    my_memset(&pad, 0, sizeof(pad));
    pad.lx = 128;
    pad.ly = 128;
    pad.rx = 128;
    pad.ry = 128;

    // Camera state
    /* Default view = sunset.
       Camera looks WEST (yaw = -π/2 = -1.5708), pitch level (0).
       Sun sits low on western horizon at sun_angle ≈ π (slightly before so
       the disc is clearly above the horizon — golden-hour position).
       Camera Y = original eye height (0.15) + cube_world_y offset (0.4) =
       0.55 — same vertical lift the cube got, so eye-level is at cube
       center. cam_x = 3.5 puts the camera far enough back that the whole
       tumbling cube (radius ~0.7) fits in the 35° FOV with margin. */
    float cam_yaw = -1.5708f, cam_pitch = 0.0f;
    float cam_x = 3.5f, cam_y = 0.55f, cam_z = 0.8f;
    float move_speed = 0.02f;
    int speed_pct = 100;                        /* the camera speed, % of MOVE_SPEED_100 */
    int speed_dir = 0;                          /* the D-pad direction held: +1 up, -1 down, 0 */
    float speed_held = 0.0f, speed_next = 0.0f; /* s held, s held at the next repeat */
    float ground_y = -0.05f;   /* ground level */
    float eye_height = 0.15f;  /* camera height above ground */
    int day_frozen = 0;        /* Square */
    int day_step = DAY_MULT_ONE; /* index into k_day_tenths (L1 / R1) */
    float day_hold = 0.0f;     /* L1 / R1 auto-repeat timer */
    int cube_rotation_enabled = 1;  // Start button toggles this (default: spinning)
    float cube_angle_y = 0.0f;       // accumulator (advances only when enabled)
    float cube_angle_x = 0.0f;
    /* Start of the day: sun_angle 0 is the first day frame (sun_y = 0, so
       is_night = (sun_y < 0) is false) - the sun on the eastern horizon.
       The default camera faces west, so the sun itself comes into view later,
       near sunset. */
    float sun_angle = 0.0f;
    float day_scrub_held = 0.0f; /* seconds D-pad left / right has been held (DAY_SCRUB_*) */
    int sun_follow = 0;          /* R3: the sun follows the clock (console / internet time) */
    float sun_glide = 1.0f, sun_glide_from = 0.0f; /* 0 -> 1 over SUN_GLIDE_S after a change */
    double frame_tod = 0.0;     /* this frame's clock: seconds since local midnight */
    float sun_speed = 0.0027f;  // 40% slower than 0.0045 (= 76% slower than original 0.01125)
    uint32_t prev_buttons = 0;

    /* Flush the CPU write-combine buffers before the first frame.
       Textures, the framebuffers' initial contents and the model-loader
       staging arrays live in GARLIC (write-combine) and were filled by CPU
       stores that drain asynchronously. Everything the CPU rewrites per frame
       has been moved to ONION, so this only has to happen ONCE - which is
       exactly what the API is for. Without it the first frames can sample
       texels the GPU has not seen yet. */
    /* Core affinity 0x3f (cores 0-5), the game's general worker mask; if the call fails the default
       affinity stays. */
#if SET_AFFINITY
    { void *self = scePthreadSelf();
      g_affinity_ret = self ? scePthreadSetaffinity(self, 0x3fULL) : -1; }
#else
    g_affinity_ret = -98;   /* not attempted */
#endif

#if MAP_COMPUTE_QUEUES
    /* Map two compute queues as the game does at init (pipe 0 / 1, queue 4, ring 0x1000 dwords;
       nothing is dispatched). gnm bounds: pipe <= 6, queue <= 7, ring 4-byte aligned. ONION, so the
       read pointer the kernel writes is CPU-visible. Returns the queue id (>= 0) or 0x80d170xx
       (negative); Unmap takes exactly the id Map returned. */
    {
        void *cq_ring0 = gpu_alloc_typed(0x1000 * 4, 0x1000, MEM_TYPE_ONION);
        void *cq_ring1 = gpu_alloc_typed(0x1000 * 4, 0x1000, MEM_TYPE_ONION);
        void *cq_rptr  = gpu_alloc_typed(0x1000, 0x1000, MEM_TYPE_ONION);
        if (cq_ring0 && cq_ring1 && cq_rptr) {
            my_memset(cq_ring0, 0, 0x1000 * 4);
            my_memset(cq_ring1, 0, 0x1000 * 4);
            my_memset(cq_rptr, 0, 0x1000);
            g_mcq0 = sceGnmMapComputeQueue(0, 4, cq_ring0, 0x1000, cq_rptr);
            g_mcq1 = sceGnmMapComputeQueue(1, 4, cq_ring1, 0x1000,
                                           (void*)((char*)cq_rptr + 0x40));
        } else {
            g_mcq0 = g_mcq1 = -2;   /* allocation failed */
        }
    }
#endif

    sceGnmFlushGarlic();

    uint32_t frame=0;
    /* Start ABOVE whatever the loading screen left in the fence. It shares this
       fence and leaves it high (105 in the last trace), so starting at fv=1 made
       frame 0's "*fence >= fv" pass instantly - measured fenceit=0 - and we
       flipped a buffer the GPU might still have been rendering into. */
    uint32_t fv = *fence + 1;
    int running = 1;
    int quit_reason = 0; /* 0=still running, 1=system quit event */
    /* Batch state: sub-frames accumulate into one command buffer (pm4), which
       is submitted once every BATCH_FRAMES. dcb_slot alternates so the GPU is
       never reading the buffer we are refilling. */
    struct PM4Builder pm4;
    int batch_pos = 0;
    uint32_t batch_frame0 = 0;
    int dcb_slot = 0;
    /* Per-sub-frame staging of everything the CPU rewrites in vb each frame,
       copied back by DMA at GPU execution time. */
#if BATCH_FRAMES > 1
    void *batch_stage = gpu_alloc_typed(BATCH_STAGE_SZ * BATCH_FRAMES + 0x1000,
                                        0x4000, MEM_TYPE_ONION);
#else
    /* Allocated only when BATCH_FRAMES > 1 (its only user). */
    void *batch_stage = (void*)1;   /* non-NULL so the check below passes */
#endif
    /* After sceVideoOutRegisterBuffers: exit through the unwinding path, like every fatal. */
    if (!batch_stage) FATAL_EXIT("batch staging alloc failed");

        /* The system splash went when the loading screen came up: ls_start calls
           sceSystemServiceHideSplashScreen once (until then the system composites it over the app).
         */
        /* Set gnm's unnamed driver mode before rendering starts. It reaches the
           kernel only through ioctl 0xc004811d at the next SubmitDone, and this
           process has never issued that ioctl. Speculative but cheap and safe
           (byte-validated; a layout mismatch makes it a no-op). */
        /* Retry if the driver was not ready at the first attempt. */
#if WAITFREE_SUBMIT
            if (mode_set != 0) { mode_set = gnm_set_mode(1); mode_now = gnm_get_mode(); }
#endif
    { char L[384];  /* build + scene + 5 lines, worst ~263 bytes */ int p=0;
      g_hw_ok = sceGnmDebugHardwareStatus(0);   /* baseline while healthy */
      { const char *bt = "build=" BUILD_TAG "\n"; while (*bt) L[p++] = *bt++; }
      const char *mc = "scene cfg=full scene+shadow";
      while (*mc) L[p++] = *mc++;
      L[p++] = '\n';
      const char *mq = "mapcomputequeue=";
      while (*mq) L[p++] = *mq++;
      p += lg_i64(L+p, (long long)g_mcq0); L[p++]=' ';
      p += lg_i64(L+p, (long long)g_mcq1); L[p++]='\n';
      const char *mh = "hwstatus at start=";
      while (*mh) L[p++] = *mh++;
      p += lg_i64(L+p, (long long)g_hw_ok);
      L[p++] = '\n';
      const char *m0 = "affinity ret=";
      while (*m0) L[p++] = *m0++;
      p += lg_i64(L+p, (long long)g_affinity_ret);
      L[p++] = '\n';
      const char *m = "gnm waitfree early=";
      while (*m) L[p++] = *m++;
      p += lg_i64(L+p, (long long)mode_set_early);
      const char *m1 = " set=";
      while (*m1) L[p++] = *m1++;
      p += lg_i64(L+p, (long long)mode_set);
      const char *m2 = " now=";
      while (*m2) L[p++] = *m2++;
      p += lg_i64(L+p, (long long)mode_now);
      L[p++]='\n'; L[p]=0;
      trace_msg(L);
    }

    ls_mark("final_setup");
    ls_finish(); /* the full bar; the splash went when the loading screen came up */
    int splash_ret = g_ls.splash_ret;
    { char L[96]; int p=0;
      const char *m = "HideSplashScreen ret=";
      while (*m) L[p++] = *m++;
      p += lg_i64(L+p, (long long)splash_ret);
      L[p++]='\n'; L[p]=0;
      trace_msg(L);
    }

    /* MEASURE the pre-loop submit count - never assume it. */
    { char L[96]; int p=0;
      const char *m = "submits before main loop: ";
      while (*m) L[p++] = *m++;
      p += lg_i64(L+p, (long long)g_submit_count);
      L[p++]='\n'; L[p]=0;
      trace_msg(L);
    }

    /* Event buffer for sceSystemServiceReceiveEvent. SDK struct is large; this
       is a safe over-allocation. We only read the first int32 (eventType). */
    static unsigned char sysevent[8192];
    {
        char T[256];
        int p = 0;
#define TP(x)                                                                                      \
    do {                                                                                           \
        const char* _q = (x);                                                                      \
        while (*_q)                                                                                \
            T[p++] = *_q++;                                                                        \
    } while (0)
        const Tex* tl[6] = {&floor_alb, &floor_nrm, &floor_hgt, &cube_alb, &cube_nrm, &cube_hgt};
        static const char* tn[6] = {"floor alb=", " nrm=", " hgt=", " cube alb=", " nrm=", " hgt="};
        TP("textures (dds_load, size, levels, format): ");
        for (int k = 0; k < 6; k++) {
            TP(tn[k]);
            p += lg_i64(T + p, tl[k]->err);
            TP(",");
            p += lg_i64(T + p, tl[k]->w);
            TP("x");
            p += lg_i64(T + p, tl[k]->h);
            TP(",");
            p += lg_i64(T + p, tl[k]->levels);
            TP(",");
            p += lg_i64(T + p, tl[k]->dfmt);
        }
        TP("\n");
#undef TP
        trace_line(T, (unsigned long)p);
    }
    /* Clear EOP stamps 2..5, so a stamp read on a stalled frame (rts= in the trace) is from that
       frame. */
    if (g_gpu_ts)
        for (int q = 0; q < 6; q++)
            g_gpu_ts[q] = 0;
    /* ==== §15.7 Main loop ===================================================================== */
    while (running) {
        int bi=frame%NUM_FRAMES;
#if FENCE_SLOTS > 1
        /* A FRESH FENCE ADDRESS THIS FRAME, as the game does per submit.
           Each slot is its own page. The slot must be seeded BELOW the value
           we are about to wait for, or the wait would pass instantly on a
           stale value from 256 frames ago. */
        fence = (volatile uint32_t*)((char*)fence_base
                                     + (unsigned long)(frame % FENCE_SLOTS) * 0x1000);
        *fence = fv - 1;
#endif

        /* Section timing (trace) */
        /* Decide NOW whether this frame will be logged, so the 14 timestamps
           below cost nothing on the 15 frames out of 16 that are discarded.
           Mirrors the condition used by the trace block at the end. */
        g_trace_this_frame = ((frame % 16) == 0) || (g_slow_tail > 0);

        /* Real elapsed time for this frame (s): motion is tied to time, not the frame rate. Clamped
           to 100 ms so a stalled frame cannot jump the scene; the first frame gets 1/60. */
        uint64_t t_now_us = sceKernelGetProcessTime();
        static uint64_t t_prev_us = 0;
        float dt_sec = t_prev_us ? (float)(t_now_us - t_prev_us) * 1e-6f
                                 : (1.0f / 60.0f);
        t_prev_us = t_now_us;
        if (dt_sec > 0.1f)   dt_sec = 0.1f;
        if (dt_sec < 0.0f)   dt_sec = 0.0f;
        uint64_t t_loop0 = tstamp();

        /* Poll for system events. Log EVERY event type (not just quit) so we can
           catch what the system posts at the ~8s mark where the flip vsync wait
           degrades. et==0x10000000 is the terminate/quit request. */
        int32_t et_now = 0;
        if (sceSystemServiceReceiveEvent(sysevent) == 0) {
            et_now = *(int32_t*)sysevent;
            if (et_now == 0x10000000) { running = 0; quit_reason = 1; }
            if (et_now != 0) { g_last_event = et_now; g_event_count++; }
        }
        uint64_t t_evt = tstamp();

        /* ==== §15.8 Input: pad, buttons, camera, speed, time of day =========================== */
        // Read gamepad
        if (pad_handle >= 0)
            scePadRead(pad_handle, &pad, 1);
        uint64_t t_pad = tstamp();

        // Button edge detection (pressed this frame, not last)
        uint32_t pressed = pad.buttons & ~prev_buttons;
        prev_buttons = pad.buttons;

        /* Controls (the on-screen list, src/ui.h, shows the same):
           Cross     freeze / unfreeze the cube
           Square    freeze / unfreeze day and night Triangle reset the camera
           L1 / R1   day and night slower / faster (k_day_tenths; held: repeats)
           L2 / R2   camera down / up                 D-pad up / down: camera speed
           D-pad left / right: move the sun           OPTIONS: show / hide controls
           sticks: move / look. */
        if (pressed & PAD_CIRCLE) {
            g_clock_mode = !g_clock_mode;
            g_clock_light_valid = 0; /* recompute its light on entering */
        }
        if (pressed & PAD_CROSS)
            cube_rotation_enabled = !cube_rotation_enabled;
        if (pressed & PAD_SQUARE)
            day_frozen = !day_frozen;
        if (pressed & PAD_OPTIONS)
            g_ui.controls = !g_ui.controls;
        if ((pressed & PAD_TOUCHPAD) && g_o5.ok)
            g_ui.o5page = (g_ui.o5page + 1) % g_ui.o5pages;
        if (pressed & PAD_TRI) {
            cam_yaw = -1.5708f;
            cam_pitch = 0;
            cam_x = 3.5f;
            cam_y = 0.55f;
            cam_z = 0.8f;
        }
        if (pad.buttons & PAD_R2)
            cam_y += move_speed * 60.0f * dt_sec;
        if (pad.buttons & PAD_L2)
            cam_y -= move_speed * 60.0f * dt_sec;
        /* L1 / R1: one step per press; held, repeats after DAY_REPEAT_DELAY every DAY_REPEAT_EVERY
         */
        {
            int dir = (pad.buttons & PAD_R1) ? 1 : (pad.buttons & PAD_L1) ? -1 : 0;
            if (pressed & (PAD_L1 | PAD_R1)) {
                day_step += dir;
                day_hold = -DAY_REPEAT_DELAY;
            } else if (dir) {
                day_hold += dt_sec;
                if (day_hold >= DAY_REPEAT_EVERY) {
                    day_step += dir;
                    day_hold = 0.0f;
                }
            }
            if (day_step < 0)
                day_step = 0;
            if (day_step >= DAY_MULT_COUNT)
                day_step = DAY_MULT_COUNT - 1;
        }

        /* Left stick: move forward / back + strafe (per second) */
        float lx = ((float)pad.lx - 128.0f) / 128.0f;
        float ly = ((float)pad.ly - 128.0f) / 128.0f;
        float spd = move_speed * 60.0f * dt_sec;
        if (ly > 0.15f || ly < -0.15f) {
            cam_x += my_sin(cam_yaw) * (-ly) * spd;
            cam_z -= my_cos(cam_yaw) * (-ly) * spd;
        }
        if (lx > 0.15f || lx < -0.15f) {
            cam_x += my_cos(cam_yaw) * lx * spd;
            cam_z += my_sin(cam_yaw) * lx * spd;
        }
        /* Arena (ARENA_*): over the middle of the floor, ARENA_EYE_MIN .. ARENA_HEIGHT above it */
        cam_x = cam_x > ARENA_HALF ? ARENA_HALF : (cam_x < -ARENA_HALF ? -ARENA_HALF : cam_x);
        cam_z = cam_z > ARENA_HALF ? ARENA_HALF : (cam_z < -ARENA_HALF ? -ARENA_HALF : cam_z);
        {
            float fy = -0.5f - (cam_x * cam_x + cam_z * cam_z) / (2.0f * FLOOR_R);
            cam_y = cam_y > fy + ARENA_HEIGHT ? fy + ARENA_HEIGHT : cam_y;
            cam_y = cam_y < fy + ARENA_EYE_MIN ? fy + ARENA_EYE_MIN : cam_y;
        }

        /* Right stick: look around */
        float rx = ((float)pad.rx - 128.0f) / 128.0f;
        float ry = ((float)pad.ry - 128.0f) / 128.0f;
        if (rx > 0.15f || rx < -0.15f)
            cam_yaw += rx * 2.4f * dt_sec;
        if (ry > 0.15f || ry < -0.15f)
            cam_pitch += ry * 1.8f * dt_sec;
        if (cam_pitch > 1.5f)
            cam_pitch = 1.5f;
        if (cam_pitch < -1.5f)
            cam_pitch = -1.5f;

        /* D-pad up / down: camera speed, SPEED_STEP_PCT per step (SPEED_* above) */
        {
            int up = (pad.buttons & PAD_UP) != 0, dn = (pad.buttons & PAD_DOWN) != 0;
            int dir = up == dn ? 0 : (up ? 1 : -1);
            if (dir != speed_dir) { /* a press (or the other direction): one step now */
                speed_dir = dir;
                speed_held = 0.0f;
                speed_next = SPEED_REPEAT_AFTER;
                speed_pct += dir * SPEED_STEP_PCT;
            } else if (dir != 0) {
                speed_held += dt_sec;
                while (speed_held >= speed_next) {
                    speed_pct += dir * SPEED_STEP_PCT;
                    speed_next += speed_held >= SPEED_FAST_AFTER ? SPEED_REPEAT_EVERY / SPEED_FAST
                                                                 : SPEED_REPEAT_EVERY;
                }
            }
            if (speed_pct < SPEED_MIN_PCT)
                speed_pct = SPEED_MIN_PCT;
            if (speed_pct > SPEED_MAX_PCT)
                speed_pct = SPEED_MAX_PCT;
            move_speed = (float)speed_pct * 0.01f * MOVE_SPEED_100;
        }

        /* Day and night: D-pad left / right held moves the sun; otherwise it runs at the
           chosen multiple unless frozen. Taps and holds up to DAY_SCRUB_FAST_AFTER seconds move
           it at DAY_SCRUB_START, longer holds DAY_SCRUB_FAST times faster; the count restarts
           when both are released. */
        if (pad.buttons & (PAD_LEFT | PAD_RIGHT)) {
            float rate = day_scrub_held < DAY_SCRUB_FAST_AFTER ? DAY_SCRUB_START
                                                               : DAY_SCRUB_START * DAY_SCRUB_FAST;
            if (pad.buttons & PAD_LEFT)
                sun_angle -= rate * dt_sec;
            if (pad.buttons & PAD_RIGHT)
                sun_angle += rate * dt_sec;
            if (day_scrub_held < 60.0f)
                day_scrub_held += dt_sec;
        } else
            day_scrub_held = 0.0f;
        if (!day_frozen)
            sun_angle +=
                sun_speed * (float)k_day_tenths[day_step] * 6.0f * dt_sec; /* x tenths / 10 x 60 */
        /* ==== §15.9 Time sources (L3 / R3), the sun =========================================== */
        /* Time (timesrc.h): L3 cycles the clock's source (in-game -> console -> internet; in-game
           at every start), R3 makes the sun follow the clock - nonstop, from the microsecond
           clock every frame, gliding SUN_GLIDE_S (smoothstep, the shorter way round) when it
           starts or the source changes. While it follows, the speed / freeze / D-pad settings
           wait (they apply again when R3 lets go). sun_angle 0 = 06:00, pi / 2 = 12:00. */
        if (pressed & PAD_L3) {
            ts_set_source((g_ts.source + 1) % 3);
            sun_glide = 0.0f;
            sun_glide_from = sun_angle;
        }
        if (pressed & PAD_R3) {
            sun_follow = !sun_follow;
            sun_glide = 0.0f;
            sun_glide_from = sun_angle;
        }
        if (sun_follow && g_ts.source != TS_INGAME) {
            frame_tod = ts_local_seconds(sun_angle);
            float target = (float)((frame_tod - 21600.0) * (6.283185307179586 / 86400.0));
            if (sun_glide < 1.0f) {
                sun_glide += dt_sec / SUN_GLIDE_S;
                sun_glide = sun_glide > 1.0f ? 1.0f : sun_glide;
                float d = wrap_2pi(target - sun_glide_from);
                d = d > 3.14159265f ? d - 6.28318531f : d;
                sun_angle = sun_glide_from + d * sun_glide * sun_glide * (3.0f - 2.0f * sun_glide);
            } else
                sun_angle = target;
        }
        /* Wrap every accumulator once per frame, after all increments. */
        sun_angle    = wrap_2pi(sun_angle);
        if (!(sun_follow && g_ts.source != TS_INGAME))
            frame_tod = ts_local_seconds(sun_angle); /* in-game: from the final sun_angle */
        { /* trace: a "time" line whenever the source, the network state or following changes */
            static int last_st = -1;
            int st =
                g_ts.source | (g_ts.net_started << 2) | (ts_net_valid() << 3) | (sun_follow << 4);
            if (st != last_st) {
                static const char* const kk[4] = {
                    "time src=", " net_started=", " net_ok=", " follow="};
                long long vv[4] = {g_ts.source, g_ts.net_started, ts_net_valid(), sun_follow};
                char L[96];
                int p = 0;
                for (int i = 0; i < 4; i++) {
                    for (const char* q = kk[i]; *q; q++)
                        L[p++] = *q;
                    p += lg_i64(L + p, vv[i]);
                }
                L[p++] = '\n';
                trace_line(L, p);
                last_st = st;
            }
        }
        cam_yaw      = wrap_2pi(cam_yaw);

        /* BG lighting handled by PS via light direction */

        /* Per-frame cube rotation: around Y and X at different rates, so every face comes into
           view; the same rotation applies to positions and normals. */
        {
            /* Cube rotation angles advance ONLY when cube_rotation_enabled.
               Start button (PAD_OPTIONS) toggles the flag; when disabled the
               accumulators freeze, so the cube holds its current orientation
               until rotation is re-enabled. */
            if (cube_rotation_enabled) {
                cube_angle_y += 0.78f * dt_sec;
                cube_angle_x += 0.42f * dt_sec;
                cube_angle_y = wrap_2pi(cube_angle_y);
                cube_angle_x = wrap_2pi(cube_angle_x);
            }
            float angle_y = cube_angle_y;
            float angle_x = cube_angle_x;
            float cy_ = my_cos(angle_y), sy_ = my_sin(angle_y);
            float cx_ = my_cos(angle_x), sx_ = my_sin(angle_x);

            /* Rotate (vx,vy,vz) around Y first, then X.
                 Y: (x,y,z) → (x*cy + z*sy, y, -x*sy + z*cy)
                 X: (x,y,z) → (x, y*cx - z*sx, y*sx + z*cx)
               Combined into the R##() macro. */
            #define ROTATE_XY(rx, ry, rz, vx, vy, vz) do { \
                float _yx = (vx)*cy_ + (vz)*sy_;            \
                float _yy = (vy);                           \
                float _yz = -(vx)*sy_ + (vz)*cy_;           \
                rx = _yx;                                   \
                ry = _yy*cx_ - _yz*sx_;                     \
                rz = _yy*sx_ + _yz*cx_;                     \
            } while(0)

            /* Walk the same triangle list as build_static_vb. Update positions
               AND normals — normals must rotate with the geometry or per-face
               lambert lighting in the cube PS gets stuck (e.g. the +Z face's
               normal.z=+1 stays at +1 even after rotation, so that face stays
               lit relative to sun.z regardless of where it has rotated to). */
            static const float face_normals_local[6][3] = {
                {0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}
            };
            /* World-space Y translation applied AFTER rotation so the cube
               spins around its own center but sits ABOVE the floor (Y=-0.5).
               cube_pos[] spans Y±0.4 about its local origin; with full 2-axis
               tumble, the lowest diagonal corner reaches ~0.69 below center.
               Lifting the center to Y=0.4 puts the lowest corner at ~Y=-0.29,
               clear of the floor. */
            const float cube_world_y = 0.4f;
            if (model_loaded) {
                /* Model transform for vs_model / vs_model_shadow: the cube's motion
                   (ROTATE_XY below: yaw, then pitch) at the cube's size and height,
                   M = T(0, cube_world_y, 0) * R * s. */
                float sc = MODEL_FIT_RADIUS / model_fit_radius;
                const float R[3][3] = {
                    {cy_, 0.0f, sy_}, {sx_ * sy_, cx_, -sx_ * cy_}, {-cx_ * sy_, sx_, cx_ * cy_}};
                for (int row = 0; row < 3; row++) {
                    for (int col = 0; col < 3; col++)
                        g_model.m[row * 4 + col] = R[row][col] * sc;
                    g_model.m[row * 4 + 3] = (row == 1) ? cube_world_y : 0.0f;
                }
            }
            {
                /* The prop's transform for the lens flare occlusion test (model: M above;
                   built-in cube: the same motion at scale 1). */
                float sc = model_loaded ? MODEL_FIT_RADIUS / model_fit_radius : 1.0f;
                const float R[3][3] = {
                    {cy_, 0.0f, sy_}, {sx_ * sy_, cx_, -sx_ * cy_}, {-cx_ * sy_, sx_, cx_ * cy_}};
                for (int row = 0; row < 3; row++) {
                    for (int col = 0; col < 3; col++)
                        g_prop_m[row * 4 + col] = R[row][col] * sc;
                    g_prop_m[row * 4 + 3] = (row == 1) ? cube_world_y : 0.0f;
                }
            }
            float *cb = (float*)((char*)vb + CUBE_DATA_OFF);
            /* Only the built-in cube: with a model loaded, vb + CUBE_DATA_OFF is the
               model's first 36 vertices, and rewriting them drew the cube inside
               the model every frame. */
            for (int tri = 0; tri < 12 && !model_loaded; tri++) {
                int face = tri / 2;
                int t = tri % 2;
                /* Pre-rotate this face's normal once for all 3 verts of the tri */
                float rnx, rny, rnz;
                ROTATE_XY(rnx, rny, rnz,
                          face_normals_local[face][0],
                          face_normals_local[face][1],
                          face_normals_local[face][2]);
                for (int v = 0; v < 3; v++) {
                    int vi = face_idx[tri][v];
                    int idx = (tri*3 + v) * 12;
                    /* Position: rotate about origin, then translate up. */
                    float rx, ry, rz;
                    ROTATE_XY(rx, ry, rz,
                              cube_pos[vi][0], cube_pos[vi][1], cube_pos[vi][2]);
                    cb[idx]   = rx;
                    cb[idx+1] = ry + cube_world_y;
                    cb[idx+2] = rz;
                    cb[idx+3] = 1.0f;
                    /* Normal (unaffected by translation) */
                    cb[idx+4] = rnx;
                    cb[idx+5] = rny;
                    cb[idx+6] = rnz;
                    cb[idx+7] = 0.0f;
                    /* UVs untouched (already set by build_static_vb) */
                    (void)t;
                }
            }

            /* Mirror the rotated cube data into the shadow VB so the shadow pass
               draws the SAME geometry. shadow_vb layout: [light_MVP @ 0][pad][cube verts @ 0x50]. */
            if (shadow_vb && g_shadow_ready && !model_loaded) {
                my_memcpy((char*)shadow_vb + 0x50,
                          (char*)vb + CUBE_DATA_OFF,
                          CUBE_VERTS * VERT_STRIDE);
            }

#undef ROTATE_XY
        }

        /* ==== §15.10 Camera, sky and light constants for this frame =========================== */
        build_mvp((float*)((char*)vb+MVP_OFF), cam_yaw, cam_pitch, cam_x, cam_y, cam_z);
        {
            float* cp = (float*)(desc + 104); /* ps_floor: view vector for parallax + fog */
            cp[0] = cam_x;
            cp[1] = cam_y;
            cp[2] = cam_z;
        }

        /* Mirror MVP to FLOOR_MVP_OFF so the floor draw (V#-base = vb+FLOOR_MVP_OFF)
           can read MVP at V#+0 and floor vertex data at V#+80, matching VS convention.
           Copy 64 bytes (16 floats = 4x4 matrix). */
        /* Into the STATIC buffer the floor's V# points at. Writing vb +
           FLOOR_MVP_OFF after a model load put matrix floats into model vertices
           36-37 and left the floor with the camera from load time. */
        my_memcpy(vb_static + FLOOR_MVP_OFF, (char*)vb + MVP_OFF, 64);
        if (stars_vb) /* at infinity: the camera's rotation only (the stars sit on a sphere around
                         the origin; with the translation they drifted as the camera moved, and
                         the camera can now reach past STARS_RADIUS) */
            build_mvp((float*)stars_vb, cam_yaw, cam_pitch, 0.0f, 0.0f, 0.0f);

        /* Build light-space MVP at LIGHT_MVP_OFF for shadow pass.
           Orthographic projection from sun looking at origin.
           sun_x, sun_y, sun_z are the computed sun direction below.
           Build it after sun direction is known. */

        /* Compute sun direction.

           REAL-WORLD SUN ARC (mid-latitude northern observer):
              sunrise (h=0):    sun in EAST  = +X, on horizon
              noon    (h=π/2):  sun in SOUTH = -Z (into scene), high in sky
              sunset  (h=π):    sun in WEST  = -X, on horizon
              midnight(h=3π/2): sun BELOW horizon, opposite side

           World axis convention (this codebase):
              +X = east, -X = west
              +Y = up, -Y = down
              +Z = behind camera (north), -Z = in front of camera (south)
              Camera starts at (0, 0.15, +3.5) looking down -Z toward origin.
              Cube sits in front of camera at -Z, which is "south".

           Construction: sun moves on a great circle whose plane is tilted
           from vertical by `tilt` radians (the observer's latitude offset
           from the equator). At hour-angle h:
              raw_x = cos(h)         east-west sweep  (full ±1 range)
              raw_y_up = sin(h)      vertical (raw)
           Then rotate around the X axis so the plane tilts SOUTH (toward -Z):
              sun_y =  raw_y_up * cos(tilt)
              sun_z = -raw_y_up * sin(tilt)   (negative — toward -Z = south)

           Result: sunrise on east horizon, noon high south, sunset on west
           horizon — visibly OPPOSITE sides of the sky. Peak elevation is
           cos(tilt), so the sun never reaches zenith. */
        float sun_y, sun_x, sun_z;
        {
            const float tilt = 0.55f;                /* ~31° latitude tilt */
            const float ct = my_cos(tilt);
            const float st = my_sin(tilt);
            float raw_x = my_cos(sun_angle);         /* east-west: +1 dawn, -1 dusk */
            float raw_y_up = my_sin(sun_angle);      /* vertical raw: +1 noon, -1 midnight */
            sun_x = raw_x;                            /* east-west position */
            sun_y = raw_y_up * ct;                    /* vertical (attenuated by tilt) */
            sun_z = -raw_y_up * st;                   /* southward (-Z) when sun is up */
        }

        /* Swap to moon (anti-sun) at night so the active light is always
           ABOVE the horizon. Reuse the sun_x/y/z variables so the rest of
           the pipeline (light_MVP construction, shadow DCB) just works.
           Save the ORIGINAL sun elevation before flipping — the sky color
           block below still needs to know whether it's actually day or night
           (sun_y becomes >=0 always after the swap).

           Sun magnitude ramp scales the Lambert dot product in cube/floor PS:
             night                   → MOON_LIGHT 0.621 (moon)
             horizon (sun_y=0)       → 0.98
             rising 0 → sun_y 0.15   → linear ramp 0.98 → 1.38
             above sun_y 0.15        → 1.38  (peak day)

           Peak from sun_y 0.15 (8.6°) so the sun stays at full brightness
           for nearly all of the day. Darkening only kicks in during the
           final ~1-2 hours before sunset (or first 1-2 after sunrise). */
        float orig_sun_y = sun_y;
        const float sun_dx = sun_x, sun_dy = sun_y, sun_dz = sun_z; /* unswapped, unit */
        int is_night = (sun_y < 0.0f);
        if (is_night) {
            sun_x = -sun_x;
            sun_y = -sun_y;
            sun_z = -sun_z;
            /* Night: MOON_LIGHT */
            sun_x *= MOON_LIGHT;
            sun_y *= MOON_LIGHT;
            sun_z *= MOON_LIGHT;
        } else {
            /* Day/twilight: ramp 0.98 at horizon → 1.38 at sun_y >= sin(8.6°) = 0.15.
               Boosted 15% from 0.85→1.20 to 0.98→1.38. Dimming only kicks in at
               the LAST ~8° of sun elevation. */
            float mag = 0.98f + 0.40f * (sun_y / 0.15f);
            if (mag > 1.38f) mag = 1.38f;
            sun_x *= mag; sun_y *= mag; sun_z *= mag;
        }
        {
            float *sun = (float*)(desc + 12);
            sun[0] = sun_x; sun[1] = sun_y; sun[2] = sun_z; sun[3] = 0;
        }

        /* Build light-space MVP (orthographic projection looking from sun toward origin).
           We use view direction = -sun_dir, target = origin, scene radius ~5 units.
           Since we just need a working matrix, use a simple axis-aligned ortho that
           projects along sun direction. Scene extends ~±5 in each axis → ortho size 10.

           Light-space matrix layout (row-major, same as camera MVP):
           - X axis: perpendicular to sun, world up cross sun
           - Y axis: world up reprojected
           - Z axis: sun direction (view direction)
           - Then orthographic scale to [-1,1] NDC */
        {
            float *lm = (float*)shadow_vb;
            /* Zero out */
            for (int i = 0; i < 16; i++) lm[i] = 0.0f;

            /* Light forward = -sun_dir (looking FROM sun TO origin) */
            float lfx = -sun_x, lfy = -sun_y, lfz = -sun_z;
            float flen = my_sqrt(lfx*lfx + lfy*lfy + lfz*lfz);
            if (flen < 0.001f) flen = 1.0f;
            lfx /= flen; lfy /= flen; lfz /= flen;

            /* Light right = normalize(cross(world_up, light_forward))
               world_up = (0, 1, 0) */
            float lrx = 1.0f * lfz - 0.0f * lfy;  /* up.y*lf.z - up.z*lf.y = lfz */
            float lry = 0.0f * lfx - 0.0f * lfz;  /* up.z*lf.x - up.x*lf.z = 0 */
            float lrz = 0.0f * lfy - 1.0f * lfx;  /* up.x*lf.y - up.y*lf.x = -lfx */
            float rlen = my_sqrt(lrx*lrx + lry*lry + lrz*lrz);
            if (rlen < 0.001f) { lrx = 1; lry = 0; lrz = 0; rlen = 1; }
            lrx /= rlen; lry /= rlen; lrz /= rlen;

            /* Light up = cross(light_forward, light_right) */
            float lux = lfy * lrz - lfz * lry;
            float luy = lfz * lrx - lfx * lrz;
            float luz = lfx * lry - lfy * lrx;

            /* Orthographic bounds. Frustum sized for cube shadow. */
            float frustum_radius = 8.0f * g_model_radius;
            float sxy = 1.0f / frustum_radius;
            /* Z range tight around the cube for precision. */
            float sz_ = 1.0f / (16.0f * g_model_radius);

            /* Place light back to keep cube inside ortho Z range. */
            float dist = 8.0f * g_model_radius;
            float lpx = g_model_cx - lfx * dist;
            float lpy = g_model_cy - lfy * dist;
            float lpz = g_model_cz - lfz * dist;

            /* View matrix rows: [right | up | forward | translation] */
            /* MVP row-major, position translation = -dot(axis, light_pos) */
            lm[0] = lrx * sxy;  lm[1] = lry * sxy;  lm[2] = lrz * sxy;
            lm[3] = -(lrx*lpx + lry*lpy + lrz*lpz) * sxy;
            lm[4] = lux * sxy;  lm[5] = luy * sxy;  lm[6] = luz * sxy;
            lm[7] = -(lux*lpx + luy*lpy + luz*lpz) * sxy;
            lm[8] = lfx * sz_;  lm[9] = lfy * sz_;  lm[10] = lfz * sz_;
            lm[11] = -(lfx*lpx + lfy*lpy + lfz*lpz) * sz_;  /* NO +0.5 — translation already centers depth */
            lm[12] = 0; lm[13] = 0; lm[14] = 0; lm[15] = 1;  /* ortho: w=1 */

            /* Mirror the same matrix into desc[48..63] (byte offset 192 = 0xC0).
               Main PS loads this via s_load_dwordx16 s[32:47], s[0:1], 0x30
               and uses it to project each fragment's world-space position into
               light-clip space for projective shadow sampling. */
            {
                float *dm = (float*)(desc + 48);
                for (int i = 0; i < 16; i++) dm[i] = lm[i];
            }
            /* Mirror light_MVP into the shadow_vb's floor-draw header so the
               floor shadow draw reads the same matrix at V#+0. */
            if (shadow_vb && !model_loaded) {
                /* A loaded model has its own shadow_vb without the floor region:
                   shadow_floor_mvp_off there lands on model vertices. */
                float *lm2 = (float*)((char*)shadow_vb + shadow_floor_mvp_off);
                for (int i = 0; i < 16; i++) lm2[i] = lm[i];
            }
        }

        /* Dynamic light color based on elevation and day/night state.

           This light_color ACTIVELY tints cube and floor output (both PS
           shaders multiply final RGB by light_color from desc[32]). Sky PS
           also reads it for moon/sun disc color.

           Threshold uses orig_sun_y (the RAW unscaled vertical component,
           ranges 0..cos(tilt) ≈ 0..0.852 over a full day). Matches the
           magnitude ramp's sin(8.6°)=0.15 threshold so warm tinting and
           dimming both kick in only at the last ~8° of sun elevation —
           the actual sunset/sunrise window.

           DAY (orig_sun_y > 0.15):   neutral white  (1.00, 1.00, 1.00)
           SUNSET/SUNRISE ramp:        white → warm amber (1.00, 0.64, 0.44)
           NIGHT:                      desaturated cool blue (0.52, 0.64, 0.84) x the moon's
                                       transmittance (atmosphere asset)
           This is the light at the arena centre (the cube); ps_floor evaluates the same model with
           each floor point's own light height on the curved floor. */
        float light_r, light_g, light_b;
        int light_linear = 0; /* 1: light_* are already linear (the physical day colour) */
        if (is_night) {
            /* Moon: desaturated cool blue, through the same air as the sun (ps_floor does the same
               at every floor point: continuous at the terminator) */
            light_r = k_moon_light_srgb[0];
            light_g = k_moon_light_srgb[1];
            light_b = k_moon_light_srgb[2];
            if (g_atmo_ok) {
                float t[3], t1[3];
                atmo_light_ground(&g_atmo, -orig_sun_y, t);
                atmo_light_ground(&g_atmo, 1.0f, t1);
                light_r = srgb_to_linear(light_r) * t[0] / t1[0];
                light_g = srgb_to_linear(light_g) * t[1] / t1[1];
                light_b = srgb_to_linear(light_b) * t[2] / t1[2];
                light_linear = 1;
            }
        } else if (g_atmo_ok) {
            /* Sunlight through the atmosphere (atmosphere.c), white-balanced to the sun at the
               zenith: white by day, amber-red at the horizon - the same air as the sky. */
            float t[3], t1[3];
            atmo_light_ground(&g_atmo, orig_sun_y, t);
            atmo_light_ground(&g_atmo, 1.0f, t1);
            light_r = t[0] / t1[0];
            light_g = t[1] / t1[1];
            light_b = t[2] / t1[2];
            light_linear = 1;
        } else if (orig_sun_y > 0.15f) {
            /* Full daylight: neutral white */
            light_r = 1.00f; light_g = 1.00f; light_b = 1.00f;
        } else {
            /* Sunrise/sunset ramp: amber at horizon → white at orig_sun_y >= 0.15.
               k = 0 at horizon, 1 at plateau edge */
            float k = orig_sun_y / 0.15f;
            /* Horizon (k=0): amber (1.00, 0.64, 0.44)
               Plateau (k=1): white (1.00, 1.00, 1.00) */
            light_r = 1.00f;
            light_g = 0.64f + 0.36f * k;
            light_b = 0.44f + 0.56f * k;
        }
        {
            float *lc = (float*)(desc + 32);
            lc[0] = light_linear ? light_r : srgb_to_linear(light_r);
            lc[1] = light_linear ? light_g : srgb_to_linear(light_g);
            lc[2] = light_linear ? light_b : srgb_to_linear(light_b);
            lc[3] = 1.0f;
            /* desc[160..162]: the floor as ps_model's downward reflections see it - ps_floor's
               lighting for an unshadowed flat floor, (0.070740275 + 0.929259717 max(0, L.y)) x
               average albedo x light colour, L = desc[12..14] (same constants and inputs). */
            const float* ld = (const float*)(desc + 12);
            float e = 0.070740275f + 0.929259717f * (ld[1] > 0.0f ? ld[1] : 0.0f);
            float* gr = (float*)(desc + 160);
            for (int c = 0; c < 3; c++)
                gr[c] = g_floor_albedo[c] * lc[c] * e;
            gr[3] = 0.0f;
        }

        /* Sky colours from the real sun elevation (orig_sun_y: the moon swap above made sun_y >=
           0), so the sky stays dark at night while the moon lights the scene. */
        float zr, zg, zb, hr, hg, hb;
        sky_colours(orig_sun_y, &zr, &zg, &zb, &hr, &hg, &hb);
        {
            float *sz = (float*)(desc + 24);
            sz[0] = srgb_to_linear(zr);
            sz[1] = srgb_to_linear(zg);
            sz[2] = srgb_to_linear(zb);
            sz[3] = 0;
            float *sh = (float*)(desc + 28);
            sh[0] = srgb_to_linear(hr);
            sh[1] = srgb_to_linear(hg);
            sh[2] = srgb_to_linear(hb);
            sh[3] = 0;
            if (g_atmo_ok) {
                /* The fog (ps_floor / ps_model) blends toward these: the physical sky at the zenith
                   and at the horizon in the camera's direction (sun + moon light, ps_dark's
                   scales). */
                float F[3], R[3], U[3], hz[3], a[3], m[3];
                cam_basis(cam_yaw, cam_pitch, F, R, U);
                const float sun[3] = {sun_dx, sun_dy, sun_dz},
                            moon[3] = {-sun_dx, -sun_dy, -sun_dz};
                const float up[3] = {0.0f, 1.0f, 0.0f};
                float hl = my_sqrt(F[0] * F[0] + F[2] * F[2]);
                hz[0] = hl > 1e-6f ? F[0] / hl : 1.0f, hz[1] = 0.0f,
                hz[2] = hl > 1e-6f ? F[2] / hl : 0.0f;
                const float* vv[2] = {up, hz};
                float* dst[2] = {sz, sh};
                for (int k = 0; k < 2; k++) {
                    atmo_sky_radiance(&g_atmo, sun, vv[k], a);
                    atmo_sky_radiance(&g_atmo, moon, vv[k], m);
                    for (int c = 0; c < 3; c++)
                        dst[k][c] = SKY_SUN_SCALE * a[c] + MOON_SKY_SCALE * g_moon_tint[c] * m[c];
                }
            }
        }
        { /* Star fade (smoothstep over the STARS_FADE band) -> desc[36..39].
             Alpha 0: additive blend leaves the destination alpha alone. */
            float fa = (STARS_FADE - orig_sun_y) / (2.0f * STARS_FADE);
            if (fa < 0.0f)
                fa = 0.0f;
            if (fa > 1.0f)
                fa = 1.0f;
            fa = fa * fa * (3.0f - 2.0f * fa);
            float* st = (float*)(desc + 36);
            st[0] = srgb_to_linear(0.85f) * fa;
            st[1] = srgb_to_linear(0.88f) * fa;
            st[2] = 1.0f * fa;
            st[3] = 0.0f;
            g_stars_draw = fa > 0.0f;
        }

        /* Sun and moon discs: desc[16..19] / desc[20..23] = (x, y, radius^2) and
           colour x HDR in desc[84..87] / desc[88..91]. Both are drawn every
           frame at their true positions; the floor (drawn after the sky) hides
           whichever is below the horizon, so a setting sun sinks out of view
           instead of vanishing when its centre crosses the horizon. Lighting
           and shadows stay with the body above the horizon (desc[12], light
           MVP). Placement: at infinity - the camera rotation only, x = cot (R.d) / (F.d), y = cot
           (U.d) / (F.d) (build_mvp's projection of a direction), the same directions ps_dark's
           sky and ps_resolve's fog reconstruct (a point 100 units from the origin drifted from its
           own sky glow as the camera moved away); radii SUN_DISC_RADIUS_PX and
           MOON_DISC_RADIUS_PX. ps_dark: f = clamp(1 - d^2 / radius^2, 0, 1)^2, d in
           aspect-corrected NDC (x * W/H, y). */
        {
            const float sun_r = SUN_DISC_RADIUS_PX / ((float)DISPLAY_H * 0.5f);
            const float moon_r = MOON_DISC_RADIUS_PX / ((float)DISPLAY_H * 0.5f);
            float cF[3], cR[3], cU[3];
            cam_basis(cam_yaw, cam_pitch, cF, cR, cU);
            const float cot = my_cos(0.3054f) / my_sin(0.3054f);
            const float dirs[2][3] = {{sun_dx, sun_dy, sun_dz}, {-sun_dx, -sun_dy, -sun_dz}};
            for (int k = 0; k < 2; k++) {
                const float* dv = dirs[k];
                float fz = dv[0] * cF[0] + dv[1] * cF[1] + dv[2] * cF[2];
                float* sd = (float*)(desc + 16 + 4 * k);
                if (fz > 0.01f) {
                    sd[0] = cot * (dv[0] * cR[0] + dv[1] * cR[1] + dv[2] * cR[2]) / fz;
                    sd[1] = cot * (dv[0] * cU[0] + dv[1] * cU[1] + dv[2] * cU[2]) / fz;
                } else { /* behind the camera */
                    sd[0] = 99.0f;
                    sd[1] = 99.0f;
                }
                sd[2] = k ? moon_r * moon_r : sun_r * sun_r;
                sd[3] = k ? moon_r : sun_r; /* radius: ps_dark's crisp edges */
            }
            /* Sun disc: amber at the horizon -> SUN_DAY_* (pale warm) at orig_sun_y
               0.15, held amber while it sets. Moon: cool blue. At SUN_HDR the core
               still clips to white; the tint shows on the rim and in the bloom. */
            /* Sun and moon as seen through the atmosphere (atmosphere.c transmittance, terminator
               included): white at noon, amber-red at the horizon. The moon's colour comes from its
               NASA albedo map in ps_dark; MOON_SCALE sets its brightness. Each light's height is
               taken above the horizon the camera sees (atmo_tilted_y with the camera's dip, the
               same height ps_dark's sky uses around the disc): from up high the sun stays visible
               until it sets behind the floor, not at the eye-level line of the arena centre. */
            float F[3], R[3], U[3], mv[3], hc, tilt[3];
            cam_basis(cam_yaw, cam_pitch, F, R, U);
            cam_horizon(cam_x, cam_y, cam_z, &hc, tilt);
            const float sun[3] = {sun_dx, sun_dy, sun_dz}, moon[3] = {-sun_dx, -sun_dy, -sun_dz};
            float ts[3] = {1.0f, 1.0f, 1.0f}, tm[3] = {1.0f, 1.0f, 1.0f};
            if (g_atmo_ok) {
                atmo_light_ground(&g_atmo, atmo_tilted_y(sun, tilt), ts);
                atmo_light_ground(&g_atmo, atmo_tilted_y(moon, tilt), tm);
            }
            float* sc = (float*)(desc + 84);
            float* mc = (float*)(desc + 88);
            for (int c = 0; c < 3; c++) {
                sc[c] = ts[c] * SUN_HDR;
                mc[c] = tm[c] * MOON_SCALE;
            }
            sc[3] = 0.0f;
            mc[3] = 0.0f;
            {
                const float ground[4] = {cam_x, cam_z, cam_y + 0.5f, 1.0f / FLOOR_R};
                moon_frame_sun(moon, sun, R, mv);
                atmo_sky_consts(&g_atmo, (float*)(desc + 164), F, R, U,
                                my_sin(0.3054f) / my_cos(0.3054f), tilt, ground, sun, SKY_SUN_SCALE,
                                moon, MOON_SKY_SCALE, mv);
                for (int c = 0; c < 3; c++)
                    ((float*)(desc + 164))[30 + c] = MOON_SKY_SCALE * g_moon_tint[c];
            }
        }
        /* Lens flare (ps_post_final, final pass table dwords 28..39): sun position
           (u, v: pixel / size, v down), strength = sun colour x FLARE_STRENGTH x visible
           fraction of the disc x edge fade, and the gains FLARE_GHOSTS / RAYS / GLOW /
           VEIL. desc[16..17] = sun NDC x aspect, y (99 behind the camera); desc[84..86] =
           sun colour x SUN_HDR. */
        if (g_post_tab) {
            float* ft = (float*)(g_post_tab + (POST_PASSES - 1) * 32 + 28);
            const float* sd = (const float*)(desc + 16);
            const float* scl = (const float*)(desc + 84);
            float vis = 0.0f;
            if (sd[0] < 90.0f) { /* in front of the camera, on or off screen */
                const float cam[3] = {cam_x, cam_y, cam_z};
                vis = flare_visibility(cam_yaw, cam_pitch, cam,
                                       sd[0] * ((float)DISPLAY_H / (float)DISPLAY_W), sd[1]);
            }
            flare_consts(ft, sd, scl, vis);
            if (g_msaa_color) { /* aerial perspective: ps_resolve table dwords 28..71 + horizon */
                float* rt = (float*)(g_post_tab + RESOLVE_BLOCK * 32);
                float F[3], R[3], U[3], hc, tilt[3], br[3], bm, hr, hm;
                cam_basis(cam_yaw, cam_pitch, F, R, U);
                cam_horizon(cam_x, cam_y, cam_z, &hc, tilt);
                atmo_aerial_coeffs(AERIAL_M_PER_UNIT, br, &bm, &hr, &hm);
                /* the view direction of the aspect-scaled NDC point (x, y): F + x tan R + y tan U,
                   tan = tan(half fov) - build_mvp's projection (scale cot) inverted */
                const float tan_half = my_sin(0.3054f) / my_cos(0.3054f);
                const float* sk = (const float*)(desc + 164); /* this frame's sky constants */
                for (int c = 0; c < 3; c++) {
                    rt[28 + c] = F[c];
                    rt[32 + c] = R[c] * tan_half;
                    rt[36 + c] = U[c] * tan_half;
                    rt[44 + c] = br[c];
                    rt[64 + c] = sk[12 + c]; /* sun L */
                    rt[68 + c] = sk[24 + c]; /* moon L */
                }
                rt[31] = CAM_NEAR * CAM_FAR;
                rt[35] = CAM_FAR;
                rt[39] = CAM_FAR - CAM_NEAR;
                rt[40] = cam_x;
                rt[41] = cam_y;
                rt[42] = cam_z;
                rt[43] = hc;
                rt[47] = bm;
                rt[48] = 1.0f / hr;
                rt[49] = 1.0f / hm;
                rt[50] = hr;
                rt[51] = hm;
                rt[52] = atmo_expf(-hc / hr);
                rt[53] = atmo_expf(-hc / hm);
                rt[54] = 1.0f / (2.0f * FLOOR_R);
                rt[55] = tilt[0];
                rt[56] = sk[16]; /* the lights' horizontal units */
                rt[57] = sk[17];
                rt[58] = sk[28];
                rt[59] = sk[29];
                rt[60] = tilt[1];
                rt[61] = tilt[2];
                rt[62] = sk[15]; /* the lights' scales (the horizon texture is unscaled) */
                rt[63] = sk[27];
                rt[67] = 1.0f / FLOOR_R;
                rt[71] = sk[18]; /* sqrt(2 / (R hc)): the dip rule below the camera, as ps_dark */
                rt[72] = sk[30]; /* the moon's sky scale per channel (g_moon_tint) */
                rt[73] = sk[31];
                rt[74] = sk[32];
                rt[75] = 0.0f;
            }
            /* ==== §15.11 UI panels and the glass clock (per-frame tables) ===================== */
            char tod_txt[16];
            ts_format(tod_txt, frame_tod);
            int ts_dot = g_ts.source == TS_INGAME    ? 1
                         : g_ts.source == TS_CONSOLE ? 2
                         : ts_net_valid()            ? 3
                                                     : 2;
            if (g_o5.ok && !g_o5.logged && frame >= 120) {
                o5_log();
                g_o5.logged = 1;
            }
            ui_update(speed_pct, k_day_tenths[day_step], day_frozen, tod_txt, ts_dot);
            ui_write_table(g_post_tab + UI_BLOCK * 32);
            if (g_o5.ok)
                ui_o5_cols((float*)(g_post_tab + UI_BLOCK * 32 + 64));
            if (g_clock_mode) {
                const float lsun[3] = {sun_dx, sun_dy, sun_dz},
                            lmoon[3] = {-sun_dx, -sun_dy, -sun_dz};
                clock_frame(cam_yaw, cam_pitch, is_night ? lmoon : lsun, is_night,
                            (const float*)(desc + 32), frame_tod, ts_local_days(), ts_dot);
            }
        }

        /* ==== §15.12 Command buffers: shadow + main, submit =================================== */
        /* Build main DCB (samples shadow_depth but doesn't write it).
           The DCB itself requests a GPU-side depth clear via DB_RENDER_CONTROL.depth_clear_enable=1
           and DEPTH_CLEAR=1.0f on the first draw — shadPS4 translates this to a Vulkan
           loadOp=Clear on the depth attachment. A CPU linear memset won't work because
           the depth buffer is GPU-tiled. */
        uint64_t t_pre_build = tstamp();
        /* BATCH_FRAMES frames go into one command buffer: the builder starts at a batch's first
           frame; the batch's other frames append to it. */
        if (batch_pos == 0) {
            phase("batch-start");
            pm4_init(&pm4, dcb_mem[dcb_slot], DCB_SIZE/4);
            if (g_gpu_ts) pm4_gpu_timestamp(&pm4, &g_gpu_ts[0]);
            batch_frame0 = frame;
        }

        /* Label sampled before this frame's work: gnm's marker patch writes label[bi] = 1 and the
           display clears it when the flip completes (trace labpre / labpost). */
        uint32_t lab_pre = label_ok ?
            ((volatile uint32_t*)flip_label_base)[bi*2] : 0xffffffffu;

        /* InsertWaitFlipDone is part of the marker/label protocol, which the
           CPU-flip path below does not use. The EOP fence wait + flip-event
           pace handle buffer-release ordering instead. Disabled for this test. */
        int wfd = -1;

        /* The shadow pass goes into the same command buffer, ahead of the main pass (one IB packet
           per submit instead of two; each builder emits its own hardware state). */
        /* DCB_PAD_DWORDS: TYPE3 NOPs pad the command buffer without changing what it renders; one
           NOP carries up to 0x3fff payload dwords (the header is emitted, the payload skipped,
           never read). */
        {
            uint32_t pad_dw = DCB_PAD_DWORDS;
            while (pad_dw > 2 && pm4.off + pad_dw <= pm4.cap) {
                uint32_t chunk = pad_dw > 0x3000 ? 0x3000 : pad_dw;
                pm4_emit(&pm4, pm4_type3(PM4_NOP, chunk - 1)); /* hdr + (chunk-1) payload */
                pm4.off += (chunk - 1);
                pad_dw -= chunk;
            }
        }

        /* Stage this sub-frame's data, then emit GPU-side copies that restore
           it at execution time. The CPU holds only ONE version of the shared
           vertex buffer, so sub-frame k+1's animation would otherwise clobber
           sub-frame k's. vb+64 .. vb+2240 is one contiguous range covering
           every per-frame CPU write (BG_SUN, MVP, SUN_DIR, the rotated cube
           vertices, the floor MVP mirror); the light-space MVP sits outside it. */
#if BATCH_FRAMES > 1
        {
            unsigned char *stg = (unsigned char*)batch_stage + batch_pos * BATCH_STAGE_SZ;
            my_memcpy(stg,               (char*)vb + BG_SUN_OFF, BATCH_VB_RANGE);
            my_memcpy(stg + BATCH_VB_RANGE, (char*)vb + LIGHT_MVP_OFF, 64);
            pm4_dma_copy(&pm4, (char*)vb + BG_SUN_OFF,    stg,               BATCH_VB_RANGE);
            pm4_dma_copy(&pm4, (char*)vb + LIGHT_MVP_OFF, stg + BATCH_VB_RANGE, 64);
        }
#endif  /* with BATCH_FRAMES==1 the CPU's own write to vb is the one the GPU
           reads, so no staging or GPU-side restore is needed at all */

        /* Set BEFORE either builder, and OUTSIDE the shadow conditional: both
           leading tags and every CPMARK embed this, and if the shadow pass is
           skipped it would otherwise never update and the checkpoint would look
           frozen for the wrong reason. */
        g_cp_frame = frame;

        uint32_t shadow_sz = 0;
        if (shadow_depth && g_shadow_ready) {
            shadow_sz = build_shadow_dcb(&pm4,
                                         vs_shadow, ps_shadow_gpu, ps_shadow_clear_gpu,
                                         shadow_vb_v, shadow_floor_v, bg_v, desc,
                                         g_shadow_verts, FLOOR_VERTS,
                                         0, 0, 0,
                                         shadow_depth);
        }
        if (g_gpu_ts) pm4_gpu_timestamp(&pm4, &g_gpu_ts[1]);

        uint32_t sz=build_dcb(&pm4,vs,ps,ps_dark_gpu,0,ps_floor_gpu,
                              vb_v,bg_v,0,floor_v,
                              vb,desc,model_verts,g_vb_total,g_ib,g_num_idx,g_indexed,
                              fb[bi],depth,0,fence,fv+batch_pos,
                              (FORCE_NO_FLIP || CPU_FLIP || g_display_stalled) /* 1 = no marker, EOP only */);
        batch_pos++;
        if (batch_pos < BATCH_FRAMES) { frame++; continue; }  /* keep accumulating */
        batch_pos = 0;

        /* CPU-flip path (is_eop FALSE): submit the DCB without a flip marker
           (sceGnmSubmitCommandBuffers), wait for its EOP fence (the GPU has finished fb[bi]), then
           queue the flip from the CPU (sceVideoOutSubmitFlip). submit= and flip= are timed
           separately in the trace. */
        int saf_ret; uint64_t t_saf; uint64_t t_submit; uint64_t t_ioctl0;
        int fence_iters = 0; int flip_iters = 0;
        uint64_t gts[6] = {0, 0, 0, 0, 0, 0};
        int asb, asa, asd;   /* AreSubmitsAllowed: before / after submit / after done */
        int ifb, ifa, ifd;   /* raw gnm in-flight submit count at the same points */
        int drn = 0;         /* drain-spin iterations needed to reach submits-allowed */
        int sdret = 0, sdret2 = 0;  /* sceGnmSubmitDone return: 0 ok, 0x80d110ff fail */
        uint64_t t_done0;
        {
            const uint32_t *a[1] = { dcb_mem[dcb_slot] };
            uint32_t s[1] = { sz };
            asb = sceGnmAreSubmitsAllowed();        /* driver in-flight counter == 0 ? */
            ifb = gnm_inflight_count();             /* raw in-flight count before */
            phase("pre-submit");
            /* Refuse to submit an overflowed command buffer. The flip marker
               must be the last 64 dwords; if pm4_emit dropped anything the
               marker is not where gnm's patcher will look and it would rewrite
               unrelated dwords. Skipping the frame is recoverable; a corrupted
               command stream is not. */
            t_ioctl0 = tstamp();   /* AFTER build_dcb, before ioctl */
            if (pm4.overflow) {
                /* Do NOT submit. The flip marker must be the last 64 dwords
                   because gnm's patcher reads dcb[size_dw - 0x40]; if pm4_emit
                   dropped anything the marker is not there and the patcher
                   would rewrite unrelated dwords. Skipping a frame is
                   recoverable, a corrupted command stream is not. */
                g_dcb_overflow++;
                saf_ret = -1;
            } else {
            /* The marker flip path: one call submits and registers the flip, the marker block
               at the DCB tail carrying the fence - (1, &dcb, &size, 0, 0, flipMode, ...), as
               the game calls it. */
            if (FORCE_NO_FLIP || CPU_FLIP || g_display_stalled) {
                /* No-flip submit once the display has stalled: EVENT_WRITE_EOP, no marker, plain
                   sceGnmSubmitCommandBuffers - the GPU stays fed and the fence observable without
                   adding to the flip queue, so the display can drain. */
                saf_ret = sceGnmSubmitCommandBuffers(1, (void**)a, s, 0, 0);
            } else {
                saf_ret = sceGnmSubmitAndFlipCommandBuffers(1, (void**)a, s, 0, 0,
                                                            video, bi, 1, (int64_t)frame);
            }
            }
            if (saf_ret == 0) {
                g_last_fence = fence;
                g_last_fv = fv + BATCH_FRAMES - 1;
            }
            t_saf = tstamp();
            asa = sceGnmAreSubmitsAllowed();
            ifa = gnm_inflight_count();             /* raw in-flight count after submit */
            t_done0 = tstamp();
            /* One SubmitDone, no spin (sceGnmAreSubmitsAllowed does not reach 1 in this app). */
            /* SubmitDone once per frame, after every submit (below), as the game does. */
            sdret = 0; sdret2 = 0;
            drn = 0;
            asd = sceGnmAreSubmitsAllowed();
            ifd = gnm_inflight_count();             /* raw in-flight count after done */
        }
        t_submit = tstamp();
        long long d_ioctl = (long long)(t_saf - t_ioctl0);   /* JUST the kernel submit */
        long long d_build = (long long)(t_ioctl0 - t_pre_build); /* JUST build_dcb (CPU) */
        long long d_done  = 0;   /* set below, after the single SubmitDone */

        /* Flip out the whole batch. The GPU renders all BATCH_FRAMES frames
           back-to-back; each ends with its own EOP fence value (fv+k), so the
           CPU can wait for frame k individually, flip it, and pace on vblank.
           The display rate is unchanged - we just spent ONE submit for K
           frames instead of K submits. */
        /* Three real timestamps split the submit-to-now region (valid on every frame, logged or
         * not). */
        uint64_t t_w0 = sceKernelGetProcessTime();
        phase("pre-flips");
        uint64_t t_flip0 = tstamp();
        uint64_t t_w1 = sceKernelGetProcessTime();
        int flip_ret = 0;
        for (int k = 0; k < BATCH_FRAMES; k++) {
            /* The fence budget resets per sub-frame, so every frame's completion is confirmed. */
            /* Bounded (~200 ms); a timeout does not abort the frame, so the trace shows whether the
               GPU recovers and where the fence stopped. */
            /* Bounded hot spin, like the game's own fence wait (the sleep granularity is ~1 ms, far
               longer than the fence takes). The clock is read every 32 pause16 batches; fence_iters
               is the wait in microseconds (trace fwait). After a few timeouts the budget drops to 2
               ms, so a dead GPU cannot make the app miss the system's quit event. */
            {
                uint64_t fw0 = sceKernelGetProcessTime();
                uint64_t budget_us = (g_fence_timeouts > 5) ? 2000 : 250000;
                while (*fence < fv+k) {
                    for (int sp = 0; sp < 32 && *fence < fv+k; sp++) cpu_pause16();
                    if (sceKernelGetProcessTime() - fw0 >= budget_us) break;
                }
                fence_iters = (int)(sceKernelGetProcessTime() - fw0);
            }
            int fence_ok = (*fence >= fv+k);
            if (!fence_ok) {
                g_fence_timeouts++;
                /* The fence drives the no-flip fallback (with PACE_ON_FENCE_ONLY it is the only
                   liveness signal); a few timeouts, not one, trip it. The flip queue takes 16
                   pending flips (0x80d11081 beyond), so tripping at 4 leaves twelve frames of
                   headroom. */
                if (g_fence_timeouts > 3) g_display_stalled = 1;
                /* Sample the kernel's GPU verdict at the MOMENT of the first
                   timeout, not just on logged frames - this is the one frame
                   whose answer actually matters. */
                if (g_fence_timeouts == 1) g_hw_ok_at_stall = sceGnmDebugHardwareStatus(0);
                g_fence_stuck_at = *fence;
                g_fence_wanted   = fv+k;
                phase("FENCE-TIMEOUT");
            }
            else if (g_display_stalled) {
                /* fence is answering again - resume normal flips */
                g_display_stalled = 0; g_stall_recoveries++;
            }
            phase("fence-ok");
            if (g_gpu_ts && fence_ok)
                for (int q = 0; q < 6; q++) {
                    gts[q] = g_gpu_ts[q];
                    g_gpu_ts[q] = 0;
                }
            /* Never flip a frame the GPU did not finish: its flip could never complete, and ~15 of
               them fill the flip queue (every submit then returns 0x80d11081) and crash the app on
               close. */
            /* Count it, but do NOT `continue`. The flip is issued by the
               SUBMIT (SubmitAndFlip via the marker), not here, so skipping the
               rest of this loop never stopped a flip - it only skipped the
               event wait, which is the one place a recovery can be observed.
               Stopping the flips is the no-flip submit above; this is just
               bookkeeping. */
            if (!fence_ok) g_flips_skipped++;

#if CPU_FLIP
            /* ==== §15.13 Flip ================================================================= */
            /* The CPU flip: the fence has arrived, so the frame is complete; skipped if it never
               did (an unfinished frame's flip can never retire). */
            if (fence_ok && !FORCE_NO_FLIP) {
                int cf = sceVideoOutSubmitFlip(video, bi, 1, (int64_t)frame);
                if (cf != 0) { g_cpuflip_fail++; g_last_cpuflip = cf; }
            }
#endif
            /* No CPU flip. sceGnmSubmitAndFlipCommandBuffers already registered
               it via the marker, so calling sceVideoOutSubmitFlip here would
               queue a SECOND flip for the same frame. */
            phase("flip-done");
#if KEEP_GPU_FED > 1
            /* Keep the GPU fed across the vblank wait instead of leaving it
               idle. Measured: the frame is 16.68ms of which 16.06ms is this
               wait, with the GPU long finished - it is idle ~99% of the time.
               The game issues ~17 submits per frame from a job system and its
               GPU never drains. Each filler is a NO-FLIP submit (hardware
               state + its own EOP fence), which is the driver's documented
               second path and touches no render state. */
            for (int fill = 1; ka_ok && keepalive_fence && fill < KEEP_GPU_FED; fill++) {
                /* Wait for this slot's previous keep-alive to retire before
                   rewriting it - KA_SLOTS deep, so this normally never waits. */
                uint32_t need = (keepalive_fv >= KA_SLOTS) ? (keepalive_fv - KA_SLOTS + 1) : 0;
                for (int w=0; w<2000 && *keepalive_fence < need; w++) sceKernelUsleep(100);
                if (*keepalive_fence < need) break;   /* GPU behind: skip filling */
                uint32_t *buf = ka_dcb[ka_slot];
                struct PM4Builder fp;
                pm4_init(&fp, buf, 0x4000/4);
                pm4_init_default_hw_state(&fp);
                pm4_event_write_eop(&fp, keepalive_fence, ++keepalive_fv);
                uint32_t fsz = fp.off * 4;
                const uint32_t *fa[1] = { buf };
                uint32_t fs[1] = { fsz };
                if (sceGnmSubmitCommandBuffers(1, (void**)fa, fs, 0, 0) == 0) {
                    /* NO SubmitDone here. The game submits ~17 times per frame
                       and calls SubmitDone ONCE (971 calls / 969 frames in its
                       log). SubmitDone is not free: it rings the DingDong
                       doorbell - a 64-entry ring whose pending count saturates
                       and is decremented only by the kernel - plus a drain and
                       a ready poll. One per frame, issued after every submit
                       below, matches the reference implementation. */
                    g_keepalive++;
                    ka_slot = (ka_slot + 1) % KA_SLOTS;
                }
            }
#endif
            if (flip_ev_ok && !FORCE_NO_FLIP) {
                /* Bounded wait (100 ms): a missing flip event is counted instead of hanging the
                 * loop. */
                struct kevent_t ev; int out=0;
                /* 100ms while healthy; 2ms once we know the display has
                   stopped answering - enough to still spot a recovery, not
                   enough to make the frame loop unresponsive. */
                unsigned int tmo = g_display_stalled ? 2000u : 100000u;
#if PACE_ON_FENCE_ONLY
                /* NO BLOCKING WAIT. The game never calls sceKernelWaitEqueue
                   at all; it paces on the fence and lets flipMode 1 (VSYNC)
                   hold the rate. We keep only the non-blocking drain below so
                   the queue cannot grow. */
                (void)tmo; (void)ev; out = 0;
#else
                /* Block for the flip event that paces this frame... */
#if PREDRAW
                /* Re-checked after every event: a leftover event from a prior flip must not release
                   the wait early. If the status cannot be read, fall back to one blocking wait per
                   frame (never free-run). */
                int wr = 0;
                out = 1;
                for (;;) {
                    OrbisVideoOutFlipStatus pfs;
                    if (sceVideoOutGetFlipStatus(video, &pfs) != 0) {
                        wr = sceKernelWaitEqueue(flip_eq, &ev, 1, &out, &tmo);
                        break;
                    }
                    if (pfs.numFlipPending <= 1)
                        break;
                    wr = sceKernelWaitEqueue(flip_eq, &ev, 1, &out, &tmo);
                    if (wr != 0 || out <= 0)
                        break;
                }
#else
                int wr = sceKernelWaitEqueue(flip_eq, &ev, 1, &out, &tmo);
#endif
                if (wr != 0 || out <= 0) {
                    g_evt_timeouts++;
                    /* Under FORCE_NO_FLIP no flip is ever queued, so no flip
                       event can ever arrive - do NOT read that as a stall. */
                    if (!FORCE_NO_FLIP) g_display_stalled = 1;
                } else if (g_display_stalled) {
                    /* the display is answering again - resume normal flips */
                    g_display_stalled = 0; g_stall_recoveries++;
                }
#endif
                flip_iters = out;
                /* PERIODIC, not per-frame. The game calls sceKernelWaitEqueue
                   ZERO times in the entire binary - it registers the flip event
                   and never touches the queue again. We keep a drain purely as
                   insurance against unbounded growth, but paying a syscall
                   every frame for a queue that measured evd=0 is exactly the
                   per-frame cost we are trying to eliminate. Once every 64
                   frames still bounds the queue at 64 entries while costing
                   1/64th of the syscalls. */
                if ((frame & 63) == 0) {
                    for (int dr = 0; dr < 64; dr++) {
                        struct kevent_t evd; int outd = 0;
                        unsigned int tmo0 = 0;
                        if (sceKernelWaitEqueue(flip_eq, &evd, 1, &outd, &tmo0) != 0) break;
                        if (outd <= 0) break;
                        g_evt_drained++;
                    }
                }
            }
#if HALF_RATE
            if (flip_ev_ok) {
                struct kevent_t ev2; int out2=0;
                unsigned int tmo2 = 100000;
                sceKernelWaitEqueue(flip_eq, &ev2, 1, &out2, &tmo2);
            }
#endif
            phase("evt-done");
        }
        /* The frame's one SubmitDone, after every submit (the frame DCB and all keep-alives) and
           the flips, as the game does. */
        /* The default hardware state has now been emitted and executed once.
           From here the per-frame DCBs only set what changes, exactly as the
           game does. */
        if (saf_ret == 0) g_hw_state_done = 1;

        { uint64_t t_sd0 = tstamp();
          sdret = sceGnmSubmitDone();
          sdret2 = sdret;
          /* SubmitDone returns 0x80d110ff when its ready-poll (ioctl 0x8116)
             does not return 1. Counting it here means a persistent driver
             failure shows up as a number instead of staying invisible. */
          if (sdret != 0) g_ka_fail++;
          d_done = (long long)(tstamp() - t_sd0); }

        fv += BATCH_FRAMES;
        dcb_slot = (dcb_slot + 1) % NUM_FRAMES;
        g_batch++;
        phase("batch-end");
        phase_flush();   /* one write + one fsync for the whole frame */
        uint64_t t_w2 = sceKernelGetProcessTime();
        uint64_t t_flip1 = tstamp();
        long long d_flip = (long long)(t_flip1 - t_flip0);

        g_submit_count++;

        /* Per-frame trace line: subc (total submits), ptms (process time). */
        /* Traced every batch. */
        /* ADAPTIVE trace rate.
           Sampling every 16th frame is fine at 60fps (one line per 266ms), but
           if the app chokes to ~2fps then 16 frames takes EIGHT SECONDS and the
           log goes silent exactly when something interesting is happening. That
           blind spot made a choke look indistinguishable from a crash.
           So: sample every 16th frame while fast, but log EVERY frame as soon
           as one takes longer than 30ms, and keep logging for 200 frames after
           things recover so the entry and exit of a choke are both captured. */
        /* REAL timestamp every frame, not gated: dt is computed from it and
           dt is what decides whether this frame gets logged at all. Gating it
           would make dt garbage and break the adaptive logging. */
        /* Kernel GPU health (sceGnmDebugHardwareStatus, an ioctl costing ~0.5 s): sampled once
           every 1024 frames and at the first fence timeout; hwok in the trace. */
#if DISPLAY_POLL
        {   uint64_t dp0 = sceKernelGetProcessTime();
            OrbisVideoOutFlipStatus fsp;
            if (sceVideoOutGetFlipStatus(video, &fsp) == 0) {
                g_fs_num  = (long long)fsp.num;
                g_fs_pend = (long long)fsp.numFlipPending;
                g_fs_gpu  = (long long)fsp.numGpuFlipPending;
                g_fs_cur  = (long long)fsp.currentBuffer;
            }
            OrbisVideoOutVblankStatus vbs;
            if (sceVideoOutGetVblankStatus(video, &vbs) == 0)
                g_vbl = (long long)vbs.count;
            g_dp_us = (long long)(sceKernelGetProcessTime() - dp0);
        }
#endif

        uint64_t t_w3 = sceKernelGetProcessTime();
        uint64_t now = sceKernelGetProcessTime();
        static uint64_t prev_t = 0;
        uint64_t dt = prev_t ? (now - prev_t) : 0; prev_t = now;
        /* ==== §15.14 Statistics: per-second trace lines ======================================= */
        /* Frame statistics over EVERY frame: per 60 frames the worst dt, frames longer than one
           refresh (> 17.5 ms) and than 1.5 refreshes (> 25 ms), and flips completed vs vblanks
           elapsed in the window (equal = a new image on every refresh). */
        {
            static long long st_n = 0, st_max = 0, st_slow = 0, st_miss = 0, st_fl0 = -1,
                             st_vb0 = -1;
            if (dt) {
                st_n++;
                st_max = (long long)dt > st_max ? (long long)dt : st_max;
                st_slow += dt > 17500;
                st_miss += dt > 25000;
            }
            if (st_n >= 60) {
                OrbisVideoOutFlipStatus sfs;
                OrbisVideoOutVblankStatus svb;
                long long fl = sceVideoOutGetFlipStatus(video, &sfs) == 0 ? (long long)sfs.num : -1;
                long long vb =
                    sceVideoOutGetVblankStatus(video, &svb) == 0 ? (long long)svb.count : -1;
                trace_stat((long long)frame, st_n, st_max, st_slow, st_miss,
                           (st_fl0 >= 0 && fl >= 0) ? fl - st_fl0 : -1,
                           (st_vb0 >= 0 && vb >= 0) ? vb - st_vb0 : -1);
                st_fl0 = fl;
                st_vb0 = vb;
                st_n = st_max = st_slow = st_miss = 0;
            }
        }
        if (dt > 30000 || g_fence_timeouts > 0) g_slow_tail = 400;
        else if (g_slow_tail > 0) g_slow_tail--;
        if ((frame % 16) == 0 || g_slow_tail > 0) {
            long long d_evt   = (long long)(t_evt    - t_loop0);
            long long d_pad   = (long long)(t_pad    - t_evt);
            long long d_saft  = (long long)(t_saf    - t_pre_build);
            long long d_wait  = (long long)(now      - t_submit);
            /* 1280, not 896. The 47 trace fields worst-case to ~892 bytes,
               which left FOUR bytes of headroom - and the estimate assumes 12
               chars per value, which a negative 64-bit number exceeds. This
               buffer is on the stack and pm4-style bounds checking does not
               apply to it; overflowing it corrupts the frame's locals. */
            /* 3072 bytes: every field (~1984 at 64 fields) and room for more. */
            char L[3072]; int p=0;
            #define LP(s) do{ const char*_q=(s); while(*_q) L[p++]=*_q++; }while(0)
            LP("f="); p+=lg_i64(L+p,(long long)frame);
            LP(" dt="); p+=lg_i64(L+p,(long long)dt);
            LP(" evt="); p+=lg_i64(L+p,d_evt);
            LP(" pad="); p+=lg_i64(L+p,d_pad);
            LP(" submit="); p+=lg_i64(L+p,d_saft);
            LP(" build="); p+=lg_i64(L+p,d_build);
            LP(" dcbsz="); p+=lg_i64(L+p,(long long)sz);
            LP(" ioctl="); p+=lg_i64(L+p,d_ioctl);
            LP(" sret="); p+=lg_hex(L+p,(unsigned long long)(unsigned int)saf_ret);
            LP(" asb="); p+=lg_i64(L+p,asb);
            LP(" asa="); p+=lg_i64(L+p,asa);
            LP(" asd="); p+=lg_i64(L+p,asd);
            LP(" ifb="); p+=lg_i64(L+p,ifb);
            LP(" ifa="); p+=lg_i64(L+p,ifa);
            LP(" ifd="); p+=lg_i64(L+p,ifd);
            LP(" drn="); p+=lg_i64(L+p,drn);
            LP(" sdret="); p+=lg_hex(L+p,(unsigned long long)(unsigned int)sdret);
            LP(" sdret2="); p+=lg_hex(L+p,(unsigned long long)(unsigned int)sdret2);
            LP(" flip="); p+=lg_i64(L+p,d_flip);
            LP(" fret="); p+=lg_hex(L+p,(unsigned long long)(unsigned int)flip_ret);
            LP(" evto="); p+=lg_i64(L+p,g_evt_timeouts);
            LP(" ka="); p+=lg_i64(L+p,g_keepalive);
            LP(" kaf="); p+=lg_i64(L+p,g_ka_fail);
            LP(" cpm="); p+=lg_hex(L+p,(unsigned long long)(g_cp_mark?*g_cp_mark:0));
            /* cpf = the frame the CP last touched; cplag = how far behind the
               CPU it is. These two make the distinction that cost me several
               rounds: cpm alone cannot separate "completed frame N" from
               "died at the end of frame N", because 0x1F is the LAST
               checkpoint either way. cplag says it directly -
                   cplag 0 or 1  the CP is keeping up
                   cplag growing the CP has stopped STARTING new buffers,
                                 which is a dispatch failure, not an execution
                                 failure. */
            { unsigned long long m = g_cp_mark ? *g_cp_mark : 0;
              unsigned long long cpf = m >> 8;
              LP(" cpf="); p+=lg_i64(L+p,(long long)cpf);
              LP(" cplag="); p+=lg_i64(L+p,(long long)((unsigned long long)frame - cpf)); }
            LP(" ovf="); p+=lg_i64(L+p,g_dcb_overflow);
            LP(" evd="); p+=lg_i64(L+p,g_evt_drained);
            LP(" fskip="); p+=lg_i64(L+p,g_flips_skipped);
            LP(" hwok="); p+=lg_i64(L+p,g_hw_ok);
            LP(" hwstall="); p+=lg_i64(L+p,g_hw_ok_at_stall);
            LP(" dstall="); p+=lg_i64(L+p,g_display_stalled);
            LP(" drec="); p+=lg_i64(L+p,g_stall_recoveries);
            LP(" fnto="); p+=lg_i64(L+p,g_fence_timeouts);
            LP(" fstuck="); p+=lg_i64(L+p,(long long)g_fence_stuck_at);
            LP(" fwant="); p+=lg_i64(L+p,(long long)g_fence_wanted);
            LP(" done="); p+=lg_i64(L+p,d_done);
            LP(" wait="); p+=lg_i64(L+p,d_wait);
            /* the three splits of the unaccounted region */
            LP(" wA="); p+=lg_i64(L+p,(long long)(t_w1 - t_w0));   /* t_submit -> flip start */
            LP(" wB="); p+=lg_i64(L+p,(long long)(t_w2 - t_w1));   /* the whole flip loop  */
            LP(" wC="); p+=lg_i64(L+p,(long long)(t_w3 - t_w2));   /* flip end -> frame end */
            /* the display pipeline, which is where the evidence points */
            LP(" fnum=");  p+=lg_i64(L+p,g_fs_num);    /* flips COMPLETED   */
            LP(" fpend="); p+=lg_i64(L+p,g_fs_pend);   /* pending, 16 = full*/
            LP(" fgpu=");  p+=lg_i64(L+p,g_fs_gpu);    /* EOP flips pending */
            LP(" fcur=");  p+=lg_i64(L+p,g_fs_cur);    /* buffer on screen  */
            LP(" vbl=");   p+=lg_i64(L+p,g_vbl);       /* vblanks since open*/
            LP(" dpus=");  p+=lg_i64(L+p,g_dp_us);     /* COST of this poll */
            LP(" cff="); p+=lg_i64(L+p,g_cpuflip_fail);
            LP(" cfr="); p+=lg_hex(L+p,(unsigned long long)(unsigned)g_last_cpuflip);
            LP(" subc="); p+=lg_i64(L+p,g_submit_count);
            LP(" ptms="); p+=lg_i64(L+p,(long long)(now/1000));
            LP(" evc="); p+=lg_i64(L+p,g_event_count);
            LP(" fwait="); p+=lg_i64(L+p,fence_iters);
            LP(" flipit="); p+=lg_i64(L+p,flip_iters);
            LP(" wfd="); p+=lg_i64(L+p,wfd);
            LP(" labpre="); p+=lg_hex(L+p,(unsigned long long)lab_pre);
            LP(" labpost="); p+=lg_hex(L+p,(unsigned long long)(label_ok ?
                ((volatile uint32_t*)flip_label_base)[bi*2] : 0xffffffffu));
            LP(" saf="); p+=lg_hex(L+p,(unsigned long long)(unsigned int)saf_ret);
            LP(" fence="); p+=lg_u64(L+p,(unsigned long long)*fence);
            /* GPU pass times in GPU-clock ticks; -1 = stamp missing */
            #define GTD(a,b_) ((gts[a] && gts[b_] && gts[b_] >= gts[a]) ? (long long)(gts[b_] - gts[a]) : -1LL)
            LP(" gts0="); p+=lg_u64(L+p,(unsigned long long)gts[0]);
            LP(" gsh="); p+=lg_i64(L+p,GTD(0,1));
            LP(" gsk="); p+=lg_i64(L+p,GTD(1,2));
            LP(" gfl="); p+=lg_i64(L+p,GTD(2,3));
            LP(" gcu="); p+=lg_i64(L+p,GTD(3,4));
            LP(" gpo=");
            p += lg_i64(L + p, GTD(4, 5));
            LP(" gtot=");
            p += lg_i64(L + p, GTD(0, 5));
#undef GTD
            LP(" fv="); p+=lg_u64(L+p,(unsigned long long)fv);
            /* rts: which EOP stamps are written right now - batch start, after
               shadow, sky, floor, cube/model, post (1 = written). On a stalled
               frame the first 0 after a 1 brackets the draw that hangs. */
            LP(" rts=");
            for (int q = 0; q < 6; q++)
                L[p++] = (g_gpu_ts && g_gpu_ts[q]) ? '1' : '0';
            L[p++]='\n';
            #undef LP
            trace_line(L,p);
        }
        frame++;
    }

    /* Clean shutdown, like the game's teardown: SubmitDone, spin until AreSubmitsAllowed (the
       driver's in-flight counter at 0), SubmitDone, spin - bounded here, so a wedged driver cannot
       hang the exit. */
    { char L[128]; int p=0;
      const char *m = "loop exit: reason=";
      while (*m) L[p++] = *m++;
      p += lg_i64(L+p,(long long)quit_reason);
      const char *m2 = " lastev=";
      while (*m2) L[p++] = *m2++;
      p += lg_hex(L+p,(unsigned long long)(unsigned int)g_last_event);
      const char *m3 = " frame=";
      while (*m3) L[p++] = *m3++;
      p += lg_i64(L+p,(long long)frame);
      L[p++]='\n'; L[p]=0; trace_msg(L); }
    trace_msg("teardown: begin\n");
    /* Teardown budgets: ~250 ms per stage, then close anyway (the sleep granularity is ~1 ms
       whatever the value asked for). */
    /* IF THE GPU IS ALREADY DEAD, DO NOT WAIT FOR IT.
       We KNOW it is dead: the checkpoint stopped changing and the fence stopped
       advancing. Draining, fence-waiting and flip-waiting against a wedged
       command processor just burns a second of the OS's patience while it is
       trying to close us - and the app gets killed instead of exiting. When
       the GPU is known-bad we skip straight to releasing the handles. */
    int gpu_dead = (g_fence_timeouts > 0) || g_display_stalled;
    int td_q1=0, td_q2=0;
    if (!gpu_dead) {
        sceGnmSubmitDone();
        for (; td_q1<20000 && !sceGnmAreSubmitsAllowed(); td_q1++) cpu_pause16();
        sceGnmSubmitDone();
        for (; td_q2<20000 && !sceGnmAreSubmitsAllowed(); td_q2++) cpu_pause16();
    }

    /* Our own EOP fence: the last submitted frame has actually retired.
       ~250 waits x ~1ms = ~250ms, then proceed regardless. */
    /* Wait on the slot and value actually submitted last: fv is already one past
       it, and with FENCE_SLOTS > 1 `fence` may point at an unsubmitted slot. */
    int td_fence=0;
    if (!gpu_dead && g_last_fence)
        for (; td_fence<250 && *g_last_fence < g_last_fv; td_fence++) sceKernelUsleep(1000);

    /* Let queued flips drain before pulling the buffers out from under them.
       ~250ms, then proceed. */
    int td_flip=0; int td_pend=-1;
    { OrbisVideoOutFlipStatus fs;
      for (; !gpu_dead && td_flip<250; td_flip++) {
          if (sceVideoOutGetFlipStatus(video,&fs) != 0) break;
          td_pend = fs.numFlipPending;
          if (fs.numFlipPending == 0) break;
          sceKernelUsleep(1000);
      } }
    { char L[200]; int p=0;
      #define LP(s) do{ const char*_q=(s); while(*_q) L[p++]=*_q++; }while(0)
      LP("teardown: q1="); p+=lg_i64(L+p,td_q1);
      LP(" q2="); p+=lg_i64(L+p,td_q2);
      LP(" fencewait="); p+=lg_i64(L+p,td_fence);
      LP(" flipwait="); p+=lg_i64(L+p,td_flip);
      LP(" gpudead="); p+=lg_i64(L+p,gpu_dead);
      LP(" pend="); p+=lg_i64(L+p,td_pend);
      LP(" fence="); p+=lg_u64(L+p,(unsigned long long)(g_last_fence ? *g_last_fence : 0));
      LP(" want="); p+=lg_u64(L+p,(unsigned long long)g_last_fv); L[p++]='\n';
      #undef LP
      trace_line(L,p); }
    /* Order matters: delete the flip event BEFORE unregistering the buffers it
       refers to, then drop the equeue, then the buffers, then the handle.
       Unregistering first leaves an event pointing at freed display state. */
    if (flip_ev_ok) sceVideoOutDeleteFlipEvent(flip_eq, video);
    if (eq_created) sceKernelDeleteEqueue(flip_eq);
    /* Unregister only if the flips drained: pulling the registration from under flips that never
       complete crashes on close. The game never unregisters (VideoOutClose releases the buffers).
     */
    if (td_pend == 0) {
        sceVideoOutUnregisterBuffers(video, 0);
    } else {
        trace_msg("teardown: flips still pending, skipping UnregisterBuffers\n");
    }
    sceVideoOutClose(video);
    if (pad_handle >= 0) scePadClose(pad_handle);   /* scePadOpen had no match */
#if MAP_COMPUTE_QUEUES
    /* The game's teardown (fn 0xcd670) unmaps its compute queues before
       closing video-out. Ours returns a vqueue id in g_mcq*, so unmap only
       what actually mapped. */
    if (g_mcq0 >= 0) sceGnmUnmapComputeQueue((uint32_t)g_mcq0);
    if (g_mcq1 >= 0) sceGnmUnmapComputeQueue((uint32_t)g_mcq1);
#endif
    trace_msg("teardown: closed\n");
    return 0;
}
