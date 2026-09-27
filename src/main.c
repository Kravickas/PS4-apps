// ============================================================================
// CUBETST00 / ShadCube4 — PS4 3D Model Viewer
// ============================================================================
//
// ARCHITECTURE OVERVIEW:
//
//   ┌─────────────┐     ┌──────────────┐     ┌─────────────┐
//   │  CPU (C)     │     │  GPU (GCN)   │     │   Screen    │
//   │              │     │              │     │             │
//   │ Build MVP    │────>│ VS: MVP*pos  │────>│  Triangles  │
//   │ matrix       │     │ PS: texture  │     │  with       │
//   │ (64 bytes    │     │    × shade   │     │  texture    │
//   │  per frame)  │     │              │     │             │
//   └─────────────┘     └──────────────┘     └─────────────┘
//
// RENDERING PIPELINE:
//   1. CPU computes a 4×4 MVP matrix (rotation + perspective projection)
//   2. CPU writes MVP to GPU buffer (64 bytes, once per frame)
//   3. CPU builds a PM4 command buffer telling the GPU what to draw
//   4. GPU vertex shader loads MVP + vertex data, transforms positions
//   5. GPU pixel shader samples texture, applies lighting, outputs color
//   6. Result goes to the framebuffer → screen
//
// FILE LOADING:
//   On startup, scans /data/ShadCube4/ for:
//     - Any .obj file → parsed as 3D model (Blender export)
//     - Any .bmp file → loaded as texture
//   Falls back to built-in spinning cube + logo texture if nothing found.
//
// BUFFER LAYOUT (single GPU buffer):
//   ┌────────────┬───────────────┬────────────┬──────────────────┐
//   │ Identity   │ BG Quad       │ MVP Matrix │ Model Vertices   │
//   │ Matrix     │ (6 verts)     │ (updated   │ (from .obj or    │
//   │ (64 bytes) │ (192 bytes)   │ per frame) │  built-in cube)  │
//   ├────────────┼───────────────┼────────────┼──────────────────┤
//   │ offset 0   │ offset 64     │ offset 256 │ offset 320       │
//   └────────────┴───────────────┴────────────┴──────────────────┘
//
//   Draw 1: V# base=offset 0   → VS reads identity MVP, draws BG quad
//   Draw 2: V# base=offset 256 → VS reads real MVP, draws 3D model
//
// BUILD: clang (FreeBSD target) → ld.lld → patch_elf.py → eboot.bin
// ============================================================================
// Vertex buffer is STATIC — model-space positions + UVs uploaded once.
// MVP matrix computed on CPU per frame (64 bytes), sent as constant buffer.
// VS does: clip_pos = MVP * model_pos  (4 dot products on GPU)
// PS does: IMAGE_SAMPLE texture, multiply by shade, export MRT0
// ============================================================================

#include <stdint.h>
/* SET_AFFINITY: declare which cores we may run on, as the game does.
   This is the ONLY change in this build that adds new imports
   (scePthreadSelf, scePthreadSetaffinity), and we currently link no
   scePthread* symbol at all - so if the OpenOrbis stubs do not carry them the
   LINK will fail. Set to 0 and the two calls disappear entirely, along with
   the need for the imports. affinity ret= in the log says what happened:
       0    the call succeeded
       <0   the call failed (harmless, default affinity kept)
       -98  SET_AFFINITY was 0
       -99  never reached */
/* MAP_COMPUTE_QUEUES: map two compute queues at init exactly as the game does,
   even though we issue no compute dispatches.

   WHY, given the corrected diagnosis: the failure is SUBMIT -> DISPATCH - the
   command processor completes a buffer and never starts the next. The DOORBELL
   is the dispatch mechanism, and it is a lock-free ring in a shared page with
   no syscall: write index wraps at 64 without checking the consumer, pending
   count saturates at 0x40 rather than erroring. Whether the kernel services
   that ring may depend on having a mapped queue. The game maps two
   (pipe 0 queue 4, pipe 1 queue 4, ring 0x1000); we map none, and I dismissed
   that rounds ago with "we issue no compute dispatches".

   NOW WITH A NAME BEHIND IT. AMD's GC documentation describes exactly this
   structure: pipes hold hardware queues (HQDs), the driver creates one MQD per
   hardware queue, and the MicroEngine Scheduler maps MQDs to HQDs. Mapping a
   queue is what gives the MES something to schedule; an application that maps
   none has no user queues at all, only the legacy kernel-queue path. The
   doorbell we ring is the standard AMD doorbell - the "submit with no ioctl
   per submission" mechanism - and switch_buffer is a pipe queue-switch
   request. pipe <= 6 and queue <= 7 are the hardware bounds, and the
   return is the flattened queue id: measured 5 and 13 for pipe0/1 queue4.
   Set to 0 to revert. mcq= in the log carries both return codes. */
/* 0 = mode 0, ioctl 0xc0108102, THE GAME'S PATH.
   1 = mode 1, ioctl 0xc020810c, the wait-free path the game never uses. */
/* HW_STATUS_POLL: periodically sample sceGnmDebugHardwareStatus.
   OFF. That ioctl was MEASURED at 477ms per call, so even once every 1024
   frames is a visible half-second hitch. The sample that actually matters -
   the one taken at the FIRST fence timeout, reported as hwstall= - is
   unconditional and unaffected, because it fires once per stall and that is
   precisely when half a second does not matter. Turn this on only if you need
   hwok= tracked during a healthy run. */
/* DISPLAY_POLL: sample sceVideoOutGetFlipStatus + GetVblankStatus each frame.
   The evidence now points at the display pipeline - the submit blocks for
   509820us, which matches the videoout service thread's 500000us equeue
   ceiling - so these are the counters that matter:
       num               total flips COMPLETED
       numFlipPending    the counter that hits the 16 limit
       numGpuFlipPending the EOP-flip counter
       currentBuffer     which buffer the display is scanning out
       vblank count      whether the display is ticking at all
   If at frame 512 the vblank keeps counting while numFlipPending climbs and
   num stops, that is the whole answer.

   ITS COST IS MEASURED AND LOGGED as dpus. The last probe I added
   (sceGnmDebugHardwareStatus) silently cost 477ms per call and became the
   bottleneck it was meant to diagnose. This one cannot do that unnoticed -
   if dpus is large, it is the new problem and we turn this off. */
#define DISPLAY_POLL 1


#define WAITFREE_SUBMIT 0

#define MAP_COMPUTE_QUEUES 1

#define SET_AFFINITY 1

#include "bgm.h"
#include "dds_loader.h"
#include "loaders.h"
#include "logo_texture.h"
#include "nid_resolve.h"
#include "pm4.h"
#include "shaders.h"

// memset/memcpy declared in nid_resolve.h

#define DISPLAY_W       1920
#define DISPLAY_H       1080

/* Sun disc edge radius in pixels (1080p). The old disc was 59 px tall and
   105 px wide (NDC distance on a 16:9 screen); it is now round. */
#define SUN_DISC_RADIUS_PX 93.5f

/* Linear HDR pipeline: the scene renders to RGBA16F, bloom runs at quarter
   resolution, the composite writes the sRGB display buffer (videoout format
   A8R8G8B8Srgb). Values are linear light; 1.0 = display white. */
#define SUN_HDR 4.0f         /* sun disc colour x this (desc[84]) */
#define MOON_HDR 2.0f        /* moon disc colour x this (desc[88]) */
#define MOON_LIGHT 0.621f    /* night light magnitude (moonlight): 0.69 - 10% */
#define BLOOM_THRESHOLD 1.0f /* only what is brighter than white blooms */
#define BLOOM_INTENSITY 0.25f
#define EXPOSURE 1.0f

/* Floor surface (ps_floor): parallax occlusion mapping from the height map
   and distance fog toward the sky gradient. */
#define POM_DEPTH 0.05f      /* relief depth in world units (a texture tile is 4 units) */
#define POM_FADE 90.0f       /* parallax fades to zero by this distance */
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
#define MOVE_SPEED_100 0.02f  /* camera speed shown as 100% (the previous default) */
#define DAY_REPEAT_DELAY 0.40f /* L1 / R1 held: first repeat after this (s) */
#define DAY_REPEAT_EVERY 0.15f /* then one step every this (s) */
/* Day and night multiples for L1 / R1 (x1 = one day in ~39 s). */
static const int k_day_mults[] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
#define DAY_MULT_COUNT ((int)(sizeof(k_day_mults) / sizeof(k_day_mults[0])))
#define FLARE_GHOSTS 1.0f     /* soft ghosts (the first flare's six) */
#define FLARE_RAYS 1.0f       /* uneven rays (glare texture, tools/make_glare.py) */
#define FLARE_GLOW 1.4f       /* glow around the sun: FLARE_GLOW / (1 + (rho / 0.08)^2) */
#define FLARE_VEIL 0.22f      /* wide warm haze: FLARE_VEIL / (1 + (rho / 0.40)^2) */
#define GLARE_STORE_MAX 4.0f  /* glare.dds stores value / this (STORE_MAX in make_glare.py) */
#define FLARE_EDGE 0.12f      /* lens flare fades out over this screen fraction at the edges */

/* Printed in the trace header so logs from different builds can be told apart. */
#define BUILD_TAG "ui-branchfix"
/* Shadow map: 4096×4096 (4K). Real PS4 games render to 4K shadow maps
   regularly (and bigger). The 32 MB Vulkan validation error in the user's
   log was NOT a GCN/PS4 limit — it was specifically shadPS4's
   `readbackLinearImages: true` config option triggering a per-frame CPU
   readback through a 32 MB StreamBuffer. Our shadow_depth is GPU-only
   (written by shadow PS, sampled by floor PS — never CPU-touched), so
   readback is unnecessary.

   Two ways to use 4K successfully:
     1. Disable `readbackLinearImages` in shadPS4's per-game settings for
        this CUBETST00 entry. We don't need it for the shadow pipeline
        and it's the setting that's triggering the 32 MB cap.
     2. (Future) Switch the shadow CB from linear (cb_info bit 7 = 1,
        tile_mode 8) to tiled (cb_info bit 7 = 0, tile_mode 13
        Display2DThin) so shadPS4 doesn't classify it as a linear image
        regardless of the readback config. This is what real games do but
        requires CB_COLOR_ATTRIB additions and matching T# tile_mode. */
#define SHADOW_W        4096
#define SHADOW_H        4096
/* BATCHING: the submit quota is per IOCTL CALL (proven on hw: 2 command
   buffers per call walled at the same CALL count, not half). BATCH_FRAMES
   frames go into ONE command buffer and cost ONE unit of quota.
     K=1  -> 513 frames free (8.6s), then 2fps      lag 16.6ms
     K=16 -> 8208 frames free (137s), then 31fps    lag 266ms
     K=32 -> 16416 frames free (274s), then 63fps   lag 531ms
   Frames-per-submit IS input latency: the whole batch is recorded before it is
   submitted. Set BATCH_FRAMES to 1 to restore exact per-frame behaviour.
   NUM_FRAMES must be >= BATCH_FRAMES so no framebuffer is reused inside one
   batch (the GPU runs the whole batch before the CPU flips any of it). */
/* HALF_RATE: run the frame loop at 30fps instead of 60 by consuming a second
   flip event each frame. Every run so far has been at 60fps, which makes
   "stalls after ~541 FRAMES" and "stalls after ~11 SECONDS" indistinguishable.
   At 30fps they separate:
       count-based -> ~541 frames = ~18 seconds
       time-based  -> ~11 seconds = ~330 frames
   Set to 0 to restore 60fps. */
#define HALF_RATE       0
/* KEEP_GPU_FED: submit the frame's work as N separate submits instead of one,
   spread across the vblank wait, so the GPU is not idle for ~99% of every
   frame. Measured: our frame is 16.68ms of which 16.06ms is a vblank wait with
   the GPU already finished; the game issues ~17 submits per frame from a job
   system and its GPU never drains. This makes the difference testable rather
   than hypothetical. 0 = one submit per frame (previous behaviour). */
/* 1 = one submit per frame (the game's shape). Set to 4 to also issue three
   no-flip keep-alive submits during the vblank wait - that was an EXPERIMENT
   to test whether GPU idleness causes the wedge. It is OFF for this build so
   that the real fixes below (CIK GS_ONCHIP hang workaround, TCL1 cache
   invalidate, window-offset scissors, viewport depth range, ONION memory,
   one-SubmitDone-per-frame) can be evaluated on their own. If the hang
   survives them, turn this back to 4. */
/* FORCE_NO_FLIP: never use the marker/flip path - build every frame with
   EVENT_WRITE_EOP and submit with plain sceGnmSubmitCommandBuffers.

   THE DECISIVE EXPERIMENT. The checkpoint proved the command processor stops
   at frame 547 having retired stage 0x1F, and the only thing after 0x1F is the
   completion packet - which on a normal frame is the 64-dword MARKER BLOCK
   that gnm's patcher rewrites. So the marker/flip path is the prime suspect.

   With this set to 1 the marker is never emitted. NOTHING WILL BE PRESENTED -
   the screen holds the first frame - but the trace tells us everything:
       fence keeps advancing, cpm keeps changing for minutes
           -> the CP is fine without the marker; the flip path is the killer
       fence still freezes around frame ~550
           -> the marker is innocent and the problem is elsewhere
   Either answer eliminates half the remaining search space.
   Set back to 0 for a normal, presenting build. */
/* CPU_FLIP: flip with sceVideoOutSubmitFlip from the CPU instead of the
   marker's EOP flip.

   THE NEXT DISCRIMINATOR, and it is pointed at the one thing we know:
   fgpu=1 at the stall, so the flip that never retires is a GPU/EOP flip -
   registered by gnm's marker patcher through sceVideoOutSubmitEopFlip.
   A CPU flip does not go through that machinery at all.

       survives past 547 flips -> the limit lives in the EOP-flip path
                                  specifically, and the marker protocol is the
                                  problem
       still stops at 547      -> the limit is in the display's flip accounting
                                  regardless of how the flip was registered

   With this set we build the DCB with no_flip=1 (EOP fence, no marker), submit
   with plain sceGnmSubmitCommandBuffers, wait for the fence, then flip from the
   CPU. That is the path this app used before the marker protocol went in, so
   it is known to work. */
/* FENCE_SLOTS: how many distinct fence addresses to rotate through.

   THE NEXT TEST, and the counts justify it: the game does MORE submits, MORE
   SubmitDone and MORE flips than we reach before failing, so none of those is
   the limit. What we do differently is REUSE ONE FENCE ADDRESS for every
   frame. The game carves a FRESH fence per submit, inside the leading NOP of
   the command buffer itself (ctx->fence_addr = cur[pad+0x10], new every time).

   1   = the current behaviour, one fixed address
   256 = rotate through 256 distinct addresses, one page apart

       failure moves or disappears -> the fixed fence address is implicated,
                                      and matching the game's per-submit fence
                                      is the fix
       still exactly 547           -> the fence address is innocent and the
                                      limit is in the submit path itself

   Slots are one page apart so no two fences share a cache line - a stale line
   would otherwise confound the result. */
/* 256 ran on hardware: still exactly 547, identical to 1. Fence address innocent. */
#define FENCE_SLOTS     1


#define CPU_FLIP        1

#define FORCE_NO_FLIP   0

/* 2 = two submits per frame but still ONE flip. A DISCRIMINATOR, not a fix:
   the failure is a COUNT (frame ~547 whether that takes 11 seconds at 60fps or
   67 seconds at 8fps), and everything consumed once per frame is still a
   suspect. Two submits per frame separates them:
       dies at frame ~273 -> the resource is per SUBMIT
       dies at frame ~547 -> per FRAME or per FLIP; submits are innocent
   Set back to 1 for one submit per frame. */
/* PACE_ON_FENCE_ONLY: pace exactly the way the game does.

   Established from the binary: sceKernelWaitEqueue has ZERO call sites and
   ZERO pointer references in the whole eboot, as do sceVideoOutWaitVblank,
   GetFlipStatus and IsFlipPending. The game CREATES the equeue and REGISTERS
   the flip event (1 call each) and then NEVER TOUCHES IT AGAIN. Its entire
   per-frame CPU pacing is:
        submit; SubmitDone; spin until *fence == counter;
   The display paces it because the flip is issued with flipMode 1 (VSYNC), so
   the flip cannot retire faster than scanout and neither can the fence.

   We were additionally doing a BLOCKING sceKernelWaitEqueue every frame - a
   kernel round trip and a per-frame CONSUMABLE the reference does not have.
   "Something consumed once per frame that runs out at ~547" is exactly our bug.

   With this set we still DRAIN the queue non-blockingly (so it cannot grow if
   the display does post events) but we never BLOCK on it. Registering the
   event stays - the game does that much. Set to 0 to restore the old blocking
   wait. */
/* 0 - BLOCK ON THE FLIP EVENT. Reverted, and the trace says why:
   with this at 1 the frame loop FREE-RAN at 1240-1468 fps (dt=806us, 681us),
   filled the 16-deep flip queue within 16 frames, and every submit after that
   returned 0x80d11081 with the fence stuck.

   My reasoning for setting it to 1 was that the game never calls
   sceKernelWaitEqueue - it paces on a bare fence spin. True, but incomplete in
   the way that mattered: THE GAME'S FRAME IS GPU-BOUND. 16.8 submits of real
   work, ~16.6ms of GPU time, so its fence genuinely takes a frame to arrive
   and fence pacing holds it at 60fps for free.

   OUR frame is sub-millisecond - one pass, a cube and a floor. Our fence
   arrives in microseconds, so fence pacing paces us at 1400fps, and every one
   of those frames registers a flip the display can only retire at 60Hz.
   Copying the reference's pacing without its workload is no pacing at all.

   The blocking wait is what holds us to vsync. (The 477ms hardware-status
   probe had been standing in for it by accident, which is why the problem only
   appeared once that was removed.) */
#define PACE_ON_FENCE_ONLY 0

/* Back to 1. The submit-count discriminator would confound this run with the
   pacing change - one variable at a time. Set to 2 for that test afterwards. */
#define KEEP_GPU_FED    1
#define KA_SLOTS        4      /* dedicated keep-alive command buffers */
#define BATCH_FRAMES    1
/* Back to 3 - the reference registers 3, and the 4-buffer test is DONE:
   fnum stopped at exactly 547 with 3 buffers AND with 4. The failing flip
   targeted buffer 1 at 3 buffers and buffer 3 at 4, identical outcome. So the
   limit is GLOBAL, not per-buffer. */
#define NUM_FRAMES      3
#define DCB_SIZE        0x20000
/* NOP padding added to each frame DCB, in dwords. 0 = off.
   Used to test whether the submit wall is a BYTE budget or a SUBMIT COUNT. */
#define DCB_PAD_DWORDS  0      /* padding test ANSWERED: 20.3x bytes did not
                                  move the wall -> it is not a byte budget */
#define BG_VERTS        6
#define CUBE_VERTS      36
#define FLOOR_VERTS     24576 /* 64×64 grid of quads, 2 tris each = 8192 tris = 24576 verts */
#define FLOOR_GRID 64
#define FLOOR_HALF 300.0f   /* floor spans +-FLOOR_HALF in X and Z */
#define FLOOR_UV_MAX 150.0f /* texture tiles across: one per 4 units */
#define TOTAL_VERTS     (BG_VERTS + CUBE_VERTS + FLOOR_VERTS)
#define VERT_STRIDE     48
#define IDENT_OFF       0
#define BG_SUN_OFF      64
#define BG_DATA_OFF     80
#define MVP_OFF         (BG_DATA_OFF + BG_VERTS * VERT_STRIDE)
#define SUN_DIR_OFF     (MVP_OFF + 64)
#define CUBE_DATA_OFF   (SUN_DIR_OFF + 16)
/* Floor uses its own V# base = FLOOR_MVP_OFF. VS expects MVP at V#+0 and
   vertex data at V#+80 (same as cube). We reserve 80 bytes (MVP mirror +
   SUN_DIR mirror padding) right before floor vertex data. */
#define FLOOR_MVP_OFF   (CUBE_DATA_OFF + CUBE_VERTS * VERT_STRIDE)
#define FLOOR_DATA_OFF  (FLOOR_MVP_OFF + 80)
#define VERT_BUF_SIZE   (FLOOR_DATA_OFF + FLOOR_VERTS * VERT_STRIDE)
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
#define MEM_TYPE_FLEX   MEM_TYPE_GARLIC   /* legacy name, kept for existing uses */


/* Per-pass GPU timestamps (64-bit GPU clock, EOP): [0] frame start, [1] after
   the shadow pass, [2] after the sky, [3] after the floor, [4] after the cube. */
static volatile uint64_t *g_gpu_ts = 0;
/* Stars: world-fixed quads on a sphere (see build_stars), drawn after the sky
   with additive blending; colour x fade comes from desc[36..39] per frame. */
static void* g_ps_stars_gpu = 0;
static uint32_t g_stars_v[4];
static int g_stars_n = 0;    /* star quads in the buffer */
static int g_stars_draw = 0; /* 0 while fully faded out (or on the loading screen) */
/* Post-processing (emit_post): HDR scene target and a 6-level bloom chain
   (quarter res down to 15x9; one level alone spreads only ~sigma 1.4 texels,
   the coarse levels give the wide halo). Pitches are 64-texel multiples: the
   T# TILING_INDEX 8 is linear aligned. A = downsample / blurred level, B = blur
   temp, then the up-sampled sum. One 32-dword descriptor table per pass. */
#define BLOOM_LEVELS 6
static const uint16_t g_bloom_w[BLOOM_LEVELS] = {480, 240, 120, 60, 30, 15};
static const uint16_t g_bloom_h[BLOOM_LEVELS] = {270, 135, 68, 34, 17, 9};
static const uint16_t g_bloom_pitch[BLOOM_LEVELS] = {512, 256, 128, 64, 64, 64};
#define POST_PASSES (BLOOM_LEVELS * 4) /* down, blur H, blur V, up-add (last: composite) */
static void* g_hdr = 0;
static void* g_bloom_a[BLOOM_LEVELS];
static void* g_bloom_b[BLOOM_LEVELS];
static uint32_t* g_post_tab = 0;
/* Prop box for the lens flare occlusion test: model-space bounds and the world
   transform (3x4 rows) of the loaded model, or of the built-in cube (+-0.4). */
static float g_prop_lo[3] = {-0.4f, -0.4f, -0.4f};
static float g_prop_hi[3] = {0.4f, 0.4f, 0.4f};
static float g_prop_m[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                             0.0f, 0.4f, 0.0f, 0.0f, 1.0f, 0.0f};
static void* g_ps_post_down_gpu = 0;
static void* g_ps_post_blur_gpu = 0;
static void* g_ps_post_comp_gpu = 0;
static void* g_ps_post_final_gpu = 0;
#define GPU_TS(k) do { if (g_gpu_ts) pm4_gpu_timestamp(b, &g_gpu_ts[(k)]); } while (0)

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
/* Set when the flip event stops arriving. Measured on hardware: the FLIP EVENT
   dies FIRST (evto 0->1, flipit 0) while the fence is still healthy, and the
   fence stalls one frame LATER. So this is the display failing and taking the
   GPU with it, not the reverse. While set, we submit without a flip. */
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
static char g_path_buf[2][256];
static int lg_i64(char *o, long long v);
static void trace_line(const char *buf, unsigned long n);
/* Write a phase marker, but only for the batches around the observed crash
   (it dies during batch 34), so this costs nothing for the whole run before. */
/* Phase markers are ACCUMULATED, not written one at a time.

   trace_line does sceKernelWrite + sceKernelFsync - two syscalls, one of them a
   synchronous flush to storage. With 8 phase() calls a frame that was 16
   syscalls and 8 fsyncs EVERY FRAME inside the window, which is both a large
   per-frame cost and enough I/O to distort the timing we are trying to measure.

   Now they go into a buffer and the whole frame's worth is emitted in ONE
   write + ONE fsync by phase_flush() at the end of the frame. Same data, same
   durability, 1/8th the syscalls. */
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
/* Read gnm's in-flight submit counter.
   sceGnmAreSubmitsAllowed() only reports (count == 0). The raw count tells us
   whether the driver's outstanding-work count climbs to a ceiling and sticks -
   which is what a submit stall at a fixed count would look like.
   gnm's sceGnmAreSubmitsAllowed starts with:
       48 8d 05 <disp32>   lea rax,[rip+disp]   ; &ptr_to_counter
       48 8b 08            mov rcx,[rax]
   so we parse disp32 out of the function's own bytes instead of hardcoding an
   offset. If the signature does not match, return -1 rather than dereference
   anything. counter = **(uint32_t**)(fn + 7 + disp). */
static int gnm_inflight_count(void) {
    const unsigned char *f = (const unsigned char *)&sceGnmAreSubmitsAllowed;
    if (f[0] != 0x48 || f[1] != 0x8d || f[2] != 0x05) return -1;
    int32_t disp;
    __builtin_memcpy(&disp, f + 3, 4);
    uint32_t **pp = (uint32_t **)(void *)(f + 7 + (long)disp);
    if (!pp || !*pp) return -1;
    return (int)**pp;
}

/* gnm exports an unnamed set/clear/get triple for a boolean DRIVER MODE that
   reaches the kernel only via ioctl 0xc004811d, which sceGnmSubmitDone pushes
   when the dirty flag is set:
       if (dirty[0x1007d]) { sub_0x6200(id, mode[0x1007c]); dirty = 0; }
   Our mode flag defaults to 0 and is never marked dirty, so this process has
   NEVER issued that ioctl. The three functions sit at fixed offsets from the
   exported sceGnmAreSubmitsAllowed and all take no arguments (verified by
   disassembly):
       +0x40  set mode = 1, mark dirty
       +0x90  set mode = 0, mark dirty
       +0xe0  read mode
   Both the anchor's own prologue (lea rax,[rip+..] = 48 8d 05) and the
   target's (push rbp; mov rbp,rsp = 55 48 89 e5) are checked first, so on a
   firmware with a different layout this becomes a no-op instead of a wild
   jump. SPECULATIVE: what the mode means is unknown - the NIDs matched none of
   1600+ candidate names - but it is the one driver state we can reach and have
   never set. */
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
/* trace_init: open the log once (WRONLY|CREAT|TRUNC = 0x601), trying several
   paths. Keeps the fd open for the whole run. */
static void trace_init(void){
    const char *paths[] = { "/data/trace.log",
                            DATA_DIR_NEW "trace.log", DATA_DIR_OLD "trace.log",
                            "trace.log", "/mnt/sandbox/SHAD00004/data/trace.log", 0 };
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

/* Allocate GPU-visible memory of an explicit PS4 direct-memory type.
     MEM_TYPE_ONION  (0) WB, CPU<->GPU coherent  - command buffers, fences,
                         anything the CP reads or the CPU polls.
     MEM_TYPE_GARLIC (3) WC, high GPU bandwidth, NOT CPU-coherent - render
                         targets, textures, vertex data.
   God of War allocates 467 ONION vs 163 GARLIC; we had been putting
   EVERYTHING in GARLIC, including the DCBs the command processor reads and
   the EOP fence the CPU polls. CPU write-combine stores into a buffer the CP
   reads, with no sceGnmFlushGarlic, is not a guaranteed-visible arrangement. */
static void* gpu_alloc_typed(unsigned long size, unsigned long align, int memtype);
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

static float my_sin(float x) {
    const float PI = 3.14159265358979f, TWO_PI = 6.28318530717959f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    float x2 = x * x;
    return x*(1.0f-x2/6.0f*(1.0f-x2/20.0f*(1.0f-x2/42.0f*(1.0f-x2/72.0f))));
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

/* Exact square root (sqrtss). The previous 6-step Newton from x/2 was only
   accurate for roughly 0.1..1000 (1e6 -> 7855, 1e-6 -> 0.031). */
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
static const float face_shade[6] = {1.0f,0.5f,0.7f,0.8f,0.95f,0.55f};

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
       detail; the old per-vertex sampling read only 4 texels of the height map. */
    {
        float *fp = (float*)((char*)vb + FLOOR_DATA_OFF);
        const float Y_BASE = -0.5f;
        const float R = 20000.0f;
        const float STEP = (2.0f * FLOOR_HALF) / (float)FLOOR_GRID;
        const float UV_STEP = FLOOR_UV_MAX / (float)FLOOR_GRID;
        int v = 0;
        for (int gz = 0; gz < FLOOR_GRID; gz++) {
            for (int gx = 0; gx < FLOOR_GRID; gx++) {
                float x0 = -FLOOR_HALF + (float)gx * STEP, x1 = x0 + STEP;
                float z0 = -FLOOR_HALF + (float)gz * STEP, z1 = z0 + STEP;
                float u0 = (float)gx * UV_STEP, u1 = u0 + UV_STEP;
                float t0 = (float)gz * UV_STEP, t1 = t0 + UV_STEP;
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
   Floor as generated: y = -0.5 - (x^2 + z^2) / 40000 over +-FLOOR_HALF, so
   g(t) = c + b t + a t^2 with a = (dx^2 + dz^2) / 40000, b = dy + (ox dx + oz dz) /
   20000, c = oy + 0.5 + (ox^2 + oz^2) / 40000 (a ~ 1e-5: roots in the stable form
   q = -(b + sign(b) sqrt(b^2 - 4ac)) / 2, t = q / a, c / q). Prop: the ray in
   model space (A^-1 (o - t), A^-1 d, A = the 3x3 of g_prop_m) against the bounds
   (slabs). */
static int flare_ray_blocked(const float* o, const float* d) {
    float a = (d[0] * d[0] + d[2] * d[2]) / 40000.0f;
    float b = d[1] + (o[0] * d[0] + o[2] * d[2]) / 20000.0f;
    float c = o[1] + 0.5f + (o[0] * o[0] + o[2] * o[2]) / 40000.0f;
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
   disc profile (1 - d^2 / r^2)^2. Rays from build_mvp's basis: d = f + r nx aspect /
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
            float w = 1.0f - (float)q / 9.0f;
            w *= w;
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
    float n=0.01f, fa=500.0f;
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

/* Depth T#: R32_FLOAT, Depth2DThin64 tile mode. For sampling shadow_depth
   from the floor PS as a raw depth value in .r channel.
   - data_format = 4 (Format32) in bits[25:20] of word1
   - num_format  = 7 (Float)    in bits[29:26] of word1
   - tiling_idx  = 0 (Depth2DThin64) in bits[24:20] of word3
   - type        = 9 (Color2D)  in bits[31:28] of word3
   shadPS4's ImageInfo ctor with ShaderResource.is_depth=true would promote
   this to D32Sfloat, but since we'll use a regular image_sample (not _c),
   is_depth stays false and we read the raw 32-bit float depth value directly. */
static void build_tsharp_depth(uint32_t *t, void *tex, int w, int h) {
    uint64_t a=(uint64_t)(uintptr_t)tex;
    my_memset(t,0,32);
    t[0]=(uint32_t)(a>>8);
    t[1]=(uint32_t)(a>>40) | (4u<<20) | (7u<<26);  /* Format32 | Float */
    t[2]=(uint32_t)(w-1)|((uint32_t)(h-1)<<14);
    t[3]=4u|(5u<<3)|(6u<<6)|(7u<<9)|(0u<<20)|(9u<<28); /* RGBA swizzle, Depth2DThin64, Color2D */
    t[4]=(uint32_t)(w-1)<<13;
}

static void build_ssharp(uint32_t *s) {
    my_memset(s,0,16);
    // dword0: clamp_x/y/z=Wrap(0), defaults
    // dword1: min_lod=0, max_lod=0xF00(15.0 in 4.8 fixed) at bits[23:12]
    s[1] = (0xF00u << 12);
    // dword2: xy_mag_filter=Bilinear(1) at bit20, xy_min_filter=Bilinear(1) at bit22
    s[2] = (1u << 20) | (1u << 22);
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
     - xy_mag/min_filter = 1 (Bilinear): softens the sampled edge a bit
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
    /* raw1 low (dword2): xy_mag=Bilinear(1) at [20..21], xy_min=Bilinear(1) at [22..23] */
    s[2] = (1u << 20) | (1u << 22) | (1u << 29) | (1u << 30); /* + DISABLE_LSB_CEIL, FILTER_PREC_FIX (radeonsi, GFX6/7) */
    /* raw1 high (dword3): border_color_type = White(2) at bits [30..31] */
    s[3] = (2u << 30);
}

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
    return t;
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
#include "ui.h"

static void build_post_tables(uint32_t* tab) {
    my_memset(tab, 0, (POST_PASSES + 2) * 32 * 4); /* + final pass dwords 32..95 (flare, UI) */
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
}

/* CB_COLOR0_INFO: FORMAT @2, LINEAR_GENERAL @7, NUMBER_TYPE @8, COMP_SWAP @11,
   BLEND_CLAMP @15 (gfx_7_2_sh_mask.h / enum.h). Blend clamp as Mesa: set for
   NORM/SRGB, not FLOAT. */
#define CB_INFO_RGBA16F ((0xCu << 2) | (1u << 7) | (7u << 8))
#define CB_INFO_DISPLAY_SRGB ((0xAu << 2) | (1u << 7) | (6u << 8) | (1u << 11) | (1u << 15))
/* Final composite target: ps_post_final encodes sRGB (and dithers) itself. */
#define CB_INFO_DISPLAY_UNORM ((0xAu << 2) | (1u << 7) | (0u << 8) | (1u << 11) | (1u << 15))

/* One full-screen pass into dst (w x h, pitch in pixels): the previous render
   target becomes a texture (same ACQUIRE_MEM as the shadow pass), then state
   for this size, the same way build_shadow_dcb sets up its target. */
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
#define PS_POST_FINAL_RSRC1 ((10u << 6) | 17u) /* v69, s79 + VCC (lens flare, UI panels) */

/* HDR scene -> 6-level bloom chain -> composite into the sRGB display buffer.
   Order and tables as build_post_tables. */
static void emit_post(struct PM4Builder* b, void* display, const uint32_t* bg_v) {
    const uint32_t* t = g_post_tab;
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
    post_pass(b, display, DISPLAY_W, DISPLAY_W, DISPLAY_H, CB_INFO_DISPLAY_UNORM,
              g_ps_post_final_gpu, PS_POST_FINAL_RSRC1, t, bg_v);
}

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
      /* Mesa sets this bit on BOTH the generic and window scissors
         (R_028240 and R_028204). We used to set it only on the window one, so
         the generic scissor stayed subject to PA_SC_WINDOW_OFFSET - a register
         no init function writes. */
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
    pm4_set_context_reg(b,CTX_DB_Z_INFO,3u);
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
    /* Clear-by-draw, the game's pattern (DB_RENDER_CONTROL=3 + 0x777) minus
       stencil: Z enable | Z write | ALWAYS. The clear only writes through the
       depth write path. Previously this crashed in frame 0 - with DB_DEPTH_INFO
       unset (linear general). */
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(7u<<4));

    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,0); /* no culling for BG */

    /* Colour: the RGBA16F HDR target (emit_post composites it into the display
       buffer); without it, straight into the sRGB display buffer. The old
       0x09A8 had NUMBER_TYPE 1 = SNORM: 1.0 was stored as 127 -> half bright. */
    {
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
    /* SPI_VS_OUT_CONFIG: VS_EXPORT_COUNT (bits 5:1) = param exports - 1. Two params:
       (1 << 1). The old value 1 set only the reserved bit 0 (= one export). */
    pm4_set_context_reg(b, CTX_VS_OUTPUT_CONFIG, 1u << 1);
    pm4_set_context_reg(b,CTX_PS_INPUT_ENA,0x02);
    pm4_set_context_reg(b,CTX_PS_INPUT_ADDR,0x02);
    pm4_set_context_reg(b,CTX_NUM_INTERP,2);                /* 2 attrs: {u,v,ny,nz} and {wpos.xyzw} */
    pm4_set_context_reg(b,CTX_SHADER_POS_FORMAT,4);
    pm4_set_context_reg(b,CTX_Z_EXPORT_FORMAT,0);
    pm4_set_context_reg(b,CTX_COLOR_EXPORT_FORMAT,9);
    pm4_set_context_reg(b,CTX_COLOR_CONTROL,0x00CC0010u);
    pm4_set_context_reg(b,CTX_DB_SHADER_CONTROL,0);
    /* SPI_BARYC_CNTL: gnm's PS setup always writes this and no init function
       does, so it was left at whatever CLEAR_STATE produced. 0 = PERSP_CENTER
       only, POS_FLOAT_LOCATION=0 (pixel center) - which is exactly what our
       shaders use, now stated explicitly instead of relied on implicitly. */
    pm4_set_context_reg(b,CTX_SPI_BARYC_CNTL,0);
    /* ClipperControl.clip_space = 1 (ZeroToOne / DX-convention, bit 19).
       Paired with our NDC.z∈[0,1] projection matrix. Avoids dependence on
       VK_EXT_depth_clip_control which may not be honored → produced the
       "finite render distance in front of camera" artifact. */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    pm4_set_context_reg(b,CTX_MODE_CONTROL,0);
    /* VGT_SHADER_STAGES_EN / VGT_DMA_SIZE are deliberately never written:
       neither gnm nor the game does (CLEAR_STATE default; the CP loads index
       sizes from the draw packets). Writing them every frame caused the
       512-submit stall and the frame-548 GPU wedge. */
    pm4_set_context_reg(b,CTX_AA_CONFIG,0);
    pm4_set_context_reg(b,CTX_BLEND_CONTROL0,0);
    pm4_set_uconfig_reg(b,UCFG_PRIMITIVE_TYPE,4);
    pm4_set_uconfig_reg(b,UCFG_NUM_INSTANCES,1);

    // Draw 1: BG quad with sky PS (sun disc)
    {
        uint64_t a = (uint64_t)(uintptr_t)ps_bg;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (3u << 6) | 2u, (2u << 1)};
        pm4_set_sh_regs(b, SH_PS_PGM_LO, r, 4); /* ps_dark: v0-v11, s0-s27 + VCC -> 12 / 32 */
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
       Restored to 32_ABGR (9) for the other draws. */
    if (g_stars_n > 0 && g_stars_draw && g_ps_stars_gpu) {
        {
            uint64_t a = (uint64_t)(uintptr_t)g_ps_stars_gpu;
            uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (1u << 6) | 2u, (2u << 1)};
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
        pm4_set_sh_regs(b, SH_VS_USER_DATA_0, g_stars_v, 4);
        pm4_draw_index_auto(b, (uint32_t)g_stars_n * 6u);
        CPMARK(b, 0x14); /* stars drawn */
        pm4_set_context_reg(b, CTX_BLEND_CONTROL0, 0);
        pm4_set_context_reg(b, CTX_COLOR_EXPORT_FORMAT, 9); /* SPI_SHADER_32_ABGR */
    }

    // === Floor draw: between sky and cube ===
    // Depth test Less so cube draws on top, but floor is drawn first so cube occludes it.
    // Floor uses its own V# (floor_v) pointing at vb+FLOOR_MVP_OFF where MVP is mirrored
    // and floor verts are at V#+80.
    if (ps_floor && floor_v) {
        /* ps_floor (parallax + fog): v0-v83, s0-s87 + VCC -> 84 VGPRs, 96 SGPRs.
           It reads POS_Y (v2) for the fog colour: PERSP_CENTER | POS_Y_FLOAT. */
        uint64_t a=(uint64_t)(uintptr_t)ps_floor;
        uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), (11u << 6) | 20u, (2u << 1)};
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
             52 VGPRs, 24 SGPRs = 0x8C. ps_model: v0-v97, s0-s95 + VCC -> 100
             VGPRs, 104 SGPRs = 0x318. Three params: param2 = tangent, handedness. */
          uint64_t a = (uint64_t)(uintptr_t)g_model.vs;
          uint32_t r[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0x8Cu, (16u << 1)};
          pm4_set_sh_regs(b, SH_VS_PGM_LO, r, 4);
          uint32_t ud[16];
          my_memcpy(ud, cube_v, 16);
          my_memcpy(ud + 4, g_model.m, 48);
          pm4_set_sh_regs(b, SH_VS_USER_DATA_0, ud, 16);
          a = (uint64_t)(uintptr_t)g_model.ps;
          uint32_t p[4] = {(uint32_t)(a >> 8), (uint32_t)(a >> 40), 0x318u, (2u << 1)};
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

    /* The game's two paths are MUTUALLY EXCLUSIVE (eboot 0x94edf0):
         flip     -> marker block carrying the fence addr+value, NO EOP.
                     gnm's patcher emits WRITE_DATA(label=1) + WRITE_DATA(fence)
                     and registers the flip, all inside this submit.
         no flip  -> EVENT_WRITE_EOP carrying the fence, NO marker.
       Emitting both, or emitting an EOP and then flipping separately from the
       CPU, is neither path. */
    CPMARK(b, 0x1F);   /* stage: all draws retired, about to complete */
    if (no_flip) pm4_event_write_eop(b,fence,fv);
    else         pm4_prepare_flip(b,fence,fv);   /* MUST be the last 64 dwords */
    return b->off*4;
}

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
    /* NO fence/fv parameter. The shadow pass emits no EOP - it shares one
       command buffer with the main pass and the inter-pass ACQUIRE_MEM is the
       barrier - so it never had anything to signal. Carrying an unused fence
       through the signature invited the mistake of believing it was live. */

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
      /* Mesa sets this bit on BOTH the generic and window scissors
         (R_028240 and R_028204). We used to set it only on the window one, so
         the generic scissor stayed subject to PA_SC_WINDOW_OFFSET - a register
         no init function writes. */
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
       14 consecutive context regs starting at CTX_CB_COLOR0_BASE:
         +0  BASE           = shadow_depth >> 8
         +1  PITCH          = (width/8)-1  (LSB) in pixel_tile_max encoding
         +2  SLICE          = (width*height/64)-1
         +3  VIEW           = 0 (no slice offset)
         +4  INFO           = 0x00A8 → data_format=10 (R8G8B8A8), num_type=0 (Unorm),
                              linear_general=1, COMP_SWAP=0 (RGBA, matches T# read order)
         +5..+13 = 0 (no compression, no FMask, etc.)

       BUG FIX (CAFE0107 era): previous value was 0x01A8 with comment claiming
       "num_type=0 (Unorm)". The comment was wrong — 0x01A8 decodes as SNORM
       (bit 8 set = num_type=1=SNORM per AMD CIK ISA register reference).
       That caused PS output 1.0 to be encoded as SNORM8 = 127, then T# read
       as UNORM8 = 127/255 = 0.498. The clear region of the shadow map was
       producing 0.5 instead of 1.0. Fixed by clearing bit 8 → 0x00A8 = UNORM.
       Trailing 0xC0001000 NOP carries the actual {width,height} extent for shadPS4's
       ImageInfo constructor — required by liverpool.cpp:344 assertion. */
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)shadow_depth>>8);
      uint32_t r[14]={c,(SHADOW_W/8u)-1u,(SHADOW_W*SHADOW_H/64u)-1u,0,
                      0x00A8u,0,0,0,0,0,0,0,0,0};   /* num_type=0=UNORM (was 0x01A8=SNORM) */
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
    /* Unbind the depth surface explicitly. This pass renders shadow_depth as a
       COLOR target and uses no depth buffer, but it shares one command buffer
       with the main pass, which sets DB_Z_INFO=3 (a live depth surface). With
       nothing resetting it here, the shadow pass inherits the main pass's
       depth binding from the previous frame - a bound surface with no depth
       state of its own. DEPTH_CONTROL=0 stops writes, so nothing is corrupted
       today, but leaving a live surface bound to a pass that does not use it
       is not a state we should rely on. Z_INVALID makes it explicit. */
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
    /* SPI_BARYC_CNTL: gnm's PS setup always writes this and no init function
       does, so it was left at whatever CLEAR_STATE produced. 0 = PERSP_CENTER
       only, POS_FLOAT_LOCATION=0 (pixel center) - which is exactly what our
       shaders use, now stated explicitly instead of relied on implicitly. */
    pm4_set_context_reg(b,CTX_SPI_BARYC_CNTL,0);
    /* ClipperControl = ZeroToOne (bit 19) — matches main DCB clip convention,
       matches the way light_MVP is constructed (NDC.z ∈ [0,1]). */
    pm4_set_context_reg(b,CTX_CLIPPER_CONTROL,1u<<19);
    pm4_set_context_reg(b,CTX_VIEWPORT_CONTROL,0x43F);
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONTROL,0);
    pm4_set_context_reg(b,CTX_MODE_CONTROL,0);
    /* VGT_SHADER_STAGES_EN / VGT_DMA_SIZE are deliberately never written:
       neither gnm nor the game does (CLEAR_STATE default; the CP loads index
       sizes from the draw packets). Writing them every frame caused the
       512-submit stall and the frame-548 GPU wedge. */
    pm4_set_context_reg(b,CTX_AA_CONFIG,0);
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

    /* Floor as shadow caster was producing severe shadow acne — every floor
       fragment compared against its own depth in the shadow map without bias
       gives a noisy stripe pattern across the entire floor. Disabled until
       proper depth bias / slope-scale bias is added. */
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
static void *read_file_gpu(const char *path, unsigned long *out_size) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    long sz = sceKernelLseek(fd, 0, 2);
    sceKernelLseek(fd, 0, 0);
    if (sz <= 0 || sz > 4000000000LL) { sceKernelClose(fd); return 0; }
    void *buf = gpu_alloc((unsigned long)sz, 0x1000);
    if (!buf) { sceKernelClose(fd); return 0; }
    long total = 0;
    while (total < sz) {
        long n = sceKernelRead(fd, (char*)buf + total, (unsigned long)(sz - total));
        if (n <= 0) break;
        total += n;
    }
    sceKernelClose(fd);
    *out_size = (unsigned long)total;
    return buf;
}

// ============================================================================
// BMP texture loader — reads 24/32-bit uncompressed BMP, converts to RGBA
// ============================================================================
static void *load_bmp_file(const char *path, int *w, int *h) {
    int fd = sceKernelOpen(path, 0, 0);
    if (fd < 0) return 0;
    long sz = sceKernelLseek(fd, 0, 2); sceKernelLseek(fd, 0, 0);
    if (sz < 54) { sceKernelClose(fd); return 0; }
    uint8_t *d = (uint8_t*)gpu_alloc((unsigned long)sz, 0x1000);
    if (!d) { sceKernelClose(fd); return 0; }
    long t=0; while(t<sz){long n=sceKernelRead(fd,(char*)d+t,(unsigned long)(sz-t));if(n<=0)break;t+=n;}
    sceKernelClose(fd);
    if (d[0]!='B'||d[1]!='M') return 0;
    int32_t width=*(int32_t*)(d+18), height=*(int32_t*)(d+22);
    uint16_t bpp=*(uint16_t*)(d+28); uint32_t comp=*(uint32_t*)(d+30);
    uint32_t poff=*(uint32_t*)(d+10);
    int topdown=0; if(height<0){height=-height;topdown=1;}
    if(width<1||height<1||comp!=0) return 0;
    if(bpp!=24&&bpp!=32) return 0;
    int bpx=bpp/8, stride=(width*bpx+3)&~3;
    uint8_t *rgba=(uint8_t*)gpu_alloc((unsigned long)width*height*4,0x1000);
    if(!rgba) return 0;
    for(int y=0;y<height;y++){
        int sy=topdown?y:(height-1-y);
        uint8_t *sr=d+poff+(unsigned long)sy*stride, *dr=rgba+(unsigned long)y*width*4;
        for(int x=0;x<width;x++){
            dr[x*4]=sr[x*bpx+2]; dr[x*4+1]=sr[x*bpx+1];
            dr[x*4+2]=sr[x*bpx]; dr[x*4+3]=(bpp==32)?sr[x*bpx+3]:255;
        }
    }
    *w=width; *h=height; return rgba;
}




// ============================================================================
// Float/int parsers for OBJ — with end-of-buffer safety
static float pf(const char **pp, const char *end) {
    const char *p = *pp;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    float s = 1;
    if (p < end && *p == '-') {
        s = -1;
        p++;
    } else if (p < end && *p == '+') {
        p++;
    }
    float v = 0, f = 0, d = 1;
    int dot = 0;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.')) {
        if (*p == '.') {
            dot = 1;
            p++;
            continue;
        }
        if (dot) {
            f = f * 10 + (*p - '0');
            d *= 10;
        } else {
            v = v * 10 + (*p - '0');
        }
        p++;
    }
    *pp = p;
    return s * (v + f / d);
}
static int pi(const char **pp, const char *end) {
    const char *p = *pp;
    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    int s = 1, v = 0;
    if (p < end && *p == '-') {
        s = -1;
        p++;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }
    *pp = p;
    return s * v;
}

// Inline OBJ parser — minimal, no static arrays, writes directly to vb
// ============================================================================

// Parse OBJ text, write vertices to vb at CUBE_DATA_OFF.
// Allocates temp arrays via gpu_alloc. Returns vertex count.
static int parse_obj_inline(const char *data, unsigned long size, void *vb_base) {
    // Temp arrays (gpu_alloc, not BSS)
    int maxv = 16000000, maxt = 16000000;
    float *vx = (float*)gpu_alloc(maxv*4, 0x1000);
    float *vy = (float*)gpu_alloc(maxv*4, 0x1000);
    float *vz = (float*)gpu_alloc(maxv*4, 0x1000);
    float *tu = (float*)gpu_alloc(maxt*4, 0x1000);
    float *tv = (float*)gpu_alloc(maxt*4, 0x1000);
    if (!vx||!vy||!vz||!tu||!tv) return 0;

    float *out = (float*)((char*)vb_base + CUBE_DATA_OFF);
    int nv=0, nt=0, nf=0;
    const float lx=0.3f,ly=0.7f,lz=0.5f,ll=0.86f;
    const char *p=data, *end=data+size;

    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p >= end)
            break;

        // Skip comment lines
        if (*p == '#') {
            while (p < end && *p != '\n' && *p != '\r')
                p++;
            if (p < end)
                p++;
            continue;
        }

        if (*p == 'v' && p + 1 < end && *(p + 1) == ' ' && nv < maxv) {
            p += 2;
            vx[nv] = pf(&p, end);
            vy[nv] = pf(&p, end);
            vz[nv] = pf(&p, end);
            nv++;
        } else if (*p == 'v' && p + 2 < end && *(p + 1) == 't' &&
                   *(p + 2) == ' ' && nt < maxt) {
            p += 3;
            tu[nt] = pf(&p, end);
            tv[nt] = pf(&p, end);
            nt++;
        } else if (*p == 'f' && p + 1 < end && *(p + 1) == ' ') {
            p += 2;
            int fv[32], ft[32], fc = 0;
            while (p < end && *p != '\n' && *p != '\r' && fc < 32) {
                while (p < end && (*p == ' ' || *p == '\t'))
                    p++;
                if (p >= end || *p == '\n' || *p == '\r')
                    break;
                int vi2 = pi(&p, end);
                fv[fc] = (vi2 < 0) ? (nv + vi2) : (vi2 - 1);
                ft[fc] = -1;
                if (p < end && *p == '/') {
                    p++;
                    if (p < end && *p != '/') {
                        int ti2 = pi(&p, end);
                        ft[fc] = (ti2 < 0) ? (nt + ti2) : (ti2 - 1);
                    }
                    if (p < end && *p == '/') {
                        p++;
                        pi(&p, end);
                    }
                }
                fc++;
            }
            for(int i=1;i+1<fc&&nf<4000000;i++) {
                int idx[3]={0,i,i+1};
                float ax=0,ay=0,az=0,bx=0,by=0,bz=0;
                if(fv[idx[0]]>=0&&fv[idx[0]]<nv&&fv[idx[1]]>=0&&fv[idx[1]]<nv&&fv[idx[2]]>=0&&fv[idx[2]]<nv){
                    ax=vx[fv[idx[1]]]-vx[fv[idx[0]]];ay=vy[fv[idx[1]]]-vy[fv[idx[0]]];az=vz[fv[idx[1]]]-vz[fv[idx[0]]];
                    bx=vx[fv[idx[2]]]-vx[fv[idx[0]]];by=vy[fv[idx[2]]]-vy[fv[idx[0]]];bz=vz[fv[idx[2]]]-vz[fv[idx[0]]];
                }
                float nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;
                float nl=nx*nx+ny*ny+nz*nz,g=nl;
                if(nl>0.0001f){g=0.5f*(g+nl/g);g=0.5f*(g+nl/g);g=0.5f*(g+nl/g);nx/=g;ny/=g;nz/=g;}
                float sh=(nx*lx+ny*ly+nz*lz)/ll; if(sh<0)sh=-sh; if(sh<0.15f)sh=0.15f; if(sh>1)sh=1;
                for(int j=0;j<3;j++){
                    int vi=fv[idx[j]],ti=ft[idx[j]],o=nf*3*8+j*8;
                    out[o]=(vi>=0&&vi<nv)?vx[vi]:0; out[o+1]=(vi>=0&&vi<nv)?vy[vi]:0;
                    out[o+2]=(vi>=0&&vi<nv)?vz[vi]:0; out[o+3]=1;
                    out[o+4]=(ti>=0&&ti<nt)?tu[ti]:0; out[o+5]=(ti>=0&&ti<nt)?tv[ti]:0;
                    out[o+6]=sh; out[o+7]=1;
                }
                nf++;
            }
        }
        while(p<end&&*p!='\n'&&*p!='\r')p++;
        if(p<end)p++;
    }
    // Auto-scale to +-0.4
    if(nv>0){
        float mnx=vx[0],mny=vy[0],mnz=vz[0],mxx=vx[0],mxy=vy[0],mxz=vz[0];
        for(int i=1;i<nv;i++){
            if(vx[i]<mnx)mnx=vx[i];if(vx[i]>mxx)mxx=vx[i];
            if(vy[i]<mny)mny=vy[i];if(vy[i]>mxy)mxy=vy[i];
            if(vz[i]<mnz)mnz=vz[i];if(vz[i]>mxz)mxz=vz[i];
        }
        float cx=(mnx+mxx)*.5f,cy=(mny+mxy)*.5f,cz=(mnz+mxz)*.5f;
        float ext=mxx-mnx;if(mxy-mny>ext)ext=mxy-mny;if(mxz-mnz>ext)ext=mxz-mnz;
        float sc=(ext>0.0001f)?0.8f/ext:1;
        int tot=nf*3;
        for(int i=0;i<tot;i++){out[i*8]=(out[i*8]-cx)*sc;out[i*8+1]=(out[i*8+1]-cy)*sc;out[i*8+2]=(out[i*8+2]-cz)*sc;}
    }
    return nf*3;
}

// === Main ===

/* ============================================================
 * STL PARSER — Binary STL format (most common from CAD/3D printing)
 * Format: 80-byte header, 4-byte triangle count, then per-triangle:
 *   12 bytes normal (3 floats), 36 bytes vertices (3×3 floats), 2 bytes attr
 * No UV coordinates — uses per-face shading only.
 * ============================================================ */
static int parse_stl_inline(const uint8_t *data, unsigned long sz,
                            float *vb_out, int max_tris) {
    if (sz < 84) return 0;
    unsigned int ntri = *(unsigned int*)(data + 80);
    if (84 + ntri * 50 > sz) ntri = (sz - 84) / 50;
    if ((int)ntri > max_tris) ntri = max_tris;
    
    /* Auto-scale: find bounding box */
    float minx=1e30f,miny=1e30f,minz=1e30f,maxx=-1e30f,maxy=-1e30f,maxz=-1e30f;
    for (unsigned int i = 0; i < ntri && i < 10000; i++) {
        const float *v = (const float*)(data + 84 + i*50 + 12);
        for (int j=0;j<3;j++) {
            float x=v[j*3],y=v[j*3+1],z=v[j*3+2];
            if(x<minx)minx=x; if(x>maxx)maxx=x;
            if(y<miny)miny=y; if(y>maxy)maxy=y;
            if(z<minz)minz=z; if(z>maxz)maxz=z;
        }
    }
    float cx=(minx+maxx)*0.5f, cy=(miny+maxy)*0.5f, cz=(minz+maxz)*0.5f;
    float range=maxx-minx; if(maxy-miny>range)range=maxy-miny; if(maxz-minz>range)range=maxz-minz;
    float scale = (range > 0.0001f) ? 0.8f / range : 1.0f;
    
    int nout = 0;
    for (unsigned int i = 0; i < ntri; i++) {
        const float *n = (const float*)(data + 84 + i*50);
        const float *v = n + 3;
        /* Per-face shade from normal */
        float shade = 0.3f + 0.7f * (n[1] > 0 ? n[1] : -n[1]);
        if (shade > 1.0f) shade = 1.0f;
        for (int j = 0; j < 3; j++) {
            float *out = vb_out + nout * 8;
            out[0] = (v[j*3+0] - cx) * scale;
            out[1] = (v[j*3+1] - cy) * scale;
            out[2] = (v[j*3+2] - cz) * scale;
            out[3] = 1.0f;
            out[4] = 0.0f; out[5] = 0.0f; /* no UVs */
            out[6] = shade;
            out[7] = 0.0f;
            nout++;
        }
    }
    return nout;
}

/* ============================================================
 * PLY PARSER — Stanford PLY format (common for 3D scans)
 * Supports ASCII and binary_little_endian with float x,y,z vertices
 * and triangular faces.
 * ============================================================ */
static int parse_ply_inline(const uint8_t *data, unsigned long sz,
                            float *vx, float *vy, float *vz,
                            float *vb_out, int maxv, int max_tris) {
    /* Parse header */
    const char *p = (const char*)data;
    const char *end = (const char*)data + (sz < 4096 ? sz : 4096);
    int nv = 0, nf = 0, is_binary = 0;
    int header_end = 0;
    
    while (p < end) {
        if (p[0]=='e' && p[1]=='l' && p[2]=='e' && p[3]=='m') {
            /* element vertex N or element face N */
            const char *q = p + 8;
            while (*q == ' ') q++;
            if (p[8]=='v') { while(*q && *q!=' ')q++; nv=0; while(*q>='0'&&*q<='9'){nv=nv*10+(*q-'0');q++;} }
            if (p[8]=='f') { while(*q && *q!=' ')q++; nf=0; while(*q>='0'&&*q<='9'){nf=nf*10+(*q-'0');q++;} }
        }
        if (p[0]=='f' && p[1]=='o' && p[2]=='r' && p[3]=='m') {
            if (p[7]=='b') is_binary = 1;
        }
        if (p[0]=='e' && p[1]=='n' && p[2]=='d' && p[3]=='_') {
            while (p < end && *p != '\n') p++;
            p++;
            header_end = (int)(p - (const char*)data);
            break;
        }
        while (p < end && *p != '\n') p++;
        p++;
    }
    
    if (nv <= 0 || nv > maxv) return 0;
    if (nf > max_tris) nf = max_tris;
    
    /* For now only support binary PLY */
    if (!is_binary) return 0;
    
    const uint8_t *bp = data + header_end;
    /* Read vertices (assume x,y,z as first 3 floats per vertex) */
    /* TODO: parse property list to find actual stride */
    int vert_stride = 12; /* minimum: 3 floats */
    for (int i = 0; i < nv && bp + 12 <= data + sz; i++) {
        const float *fv = (const float*)bp;
        vx[i] = fv[0]; vy[i] = fv[1]; vz[i] = fv[2];
        bp += vert_stride;
    }
    
    /* Auto-scale */
    float minx=vx[0],maxx=vx[0],miny=vy[0],maxy=vy[0],minz=vz[0],maxz=vz[0];
    for(int i=1;i<nv;i++){
        if(vx[i]<minx)minx=vx[i];if(vx[i]>maxx)maxx=vx[i];
        if(vy[i]<miny)miny=vy[i];if(vy[i]>maxy)maxy=vy[i];
        if(vz[i]<minz)minz=vz[i];if(vz[i]>maxz)maxz=vz[i];
    }
    float cx=(minx+maxx)*0.5f,cy=(miny+maxy)*0.5f,cz=(minz+maxz)*0.5f;
    float range=maxx-minx;if(maxy-miny>range)range=maxy-miny;if(maxz-minz>range)range=maxz-minz;
    float scale=(range>0.0001f)?0.8f/range:1.0f;
    
    /* Read faces and emit vertices */
    int nout = 0;
    for (int i = 0; i < nf && bp + 4 <= data + sz; i++) {
        int fc = *bp++;  /* face vertex count */
        if (fc < 3 || bp + fc*4 > data + sz) break;
        const int *idx = (const int*)bp;
        bp += fc * 4;
        /* Triangulate fan */
        for (int j = 1; j < fc - 1 && nout + 3 <= max_tris * 3; j++) {
            int i0=idx[0], i1=idx[j], i2=idx[j+1];
            if(i0<0||i0>=nv||i1<0||i1>=nv||i2<0||i2>=nv) continue;
            float nx=(vy[i1]-vy[i0])*(vz[i2]-vz[i0])-(vz[i1]-vz[i0])*(vy[i2]-vy[i0]);
            float ny=(vz[i1]-vz[i0])*(vx[i2]-vx[i0])-(vx[i1]-vx[i0])*(vz[i2]-vz[i0]);
            float nlen=nx*nx+ny*ny; nlen=nlen>0?1.0f/nlen:0; /* approx */
            float shade=0.3f+0.5f*(ny>0?ny:-ny)*nlen;
            if(shade>1)shade=1;
            int vi[3]={i0,i1,i2};
            for(int k=0;k<3;k++){
                float *out=vb_out+nout*8;
                out[0]=(vx[vi[k]]-cx)*scale; out[1]=(vy[vi[k]]-cy)*scale;
                out[2]=(vz[vi[k]]-cz)*scale; out[3]=1.0f;
                out[4]=0; out[5]=0; out[6]=shade; out[7]=0;
                nout++;
            }
        }
    }
    return nout;
}

/* --- Loading screen --- */
struct LoadCtx {
    const void *vs; const void *ps;
    uint32_t *vb_v; uint32_t *bg_v;
    void *vb; uint32_t *desc;
    void *fb0; void *fb1; void *depth;
    volatile uint32_t *fence;
    int video; uint32_t *pm4_buf;
    int flip_idx;
    const void *ps_blue;
    const void *ps_dark;
};

static void loading_progress(float frac, const char* msg, void* ud) {
    struct LoadCtx *c = (struct LoadCtx*)ud;
    float progress = frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : frac); /* 0..1 */

    /* Loading sky: SOLID blue (both zenith and horizon = blue) to cover behind rendering */
    {
        float *sz = (float*)(c->desc + 24); /* zenith */
        sz[0] = srgb_to_linear(0.08f);
        sz[1] = srgb_to_linear(0.20f);
        sz[2] = srgb_to_linear(0.55f);
        sz[3] = 0;
        float *sh = (float*)(c->desc + 28); /* horizon */
        sh[0] = srgb_to_linear(0.08f);
        sh[1] = srgb_to_linear(0.20f);
        sh[2] = srgb_to_linear(0.55f);
        sh[3] = 0;
        /* Sun and moon discs off-screen, radius^2 > 0 (no disc on the loading
           screen), disc colours black. */
        float *snd = (float*)(c->desc + 16);
        snd[0]=10.0f; snd[1]=10.0f; snd[2]=0.0001f; snd[3]=0;
        snd[4] = 10.0f;
        snd[5] = 10.0f;
        snd[6] = 0.0001f;
        snd[7] = 0;
        for (int q = 84; q < 92; q++)
            c->desc[q] = 0;
    }

    /* BG quad: must cover full screen — keep normals any direction, shader just reads attr0 */
    float *bg = (float*)((char*)c->vb + BG_DATA_OFF);
    for (int i = 0; i < 6; i++) {
        bg[i*12+4]=0; bg[i*12+5]=1; bg[i*12+6]=0; bg[i*12+7]=0;
    }

    /* Identity MVP so bar positions pass through as NDC directly (covers model) */
    float *mvp = (float*)((char*)c->vb + MVP_OFF);
    mvp[0]=1;mvp[1]=0;mvp[2]=0;mvp[3]=0;
    mvp[4]=0;mvp[5]=1;mvp[6]=0;mvp[7]=0;
    mvp[8]=0;mvp[9]=0;mvp[10]=1;mvp[11]=0;
    mvp[12]=0;mvp[13]=0;mvp[14]=0;mvp[15]=1;

    /* Progress bar: WHITE via ps_blue (which outputs solid color regardless of tex)
       Position near bottom center. z=0 (in front of sky BG which is also z=0),
       but bar drawn AFTER BG so it overlays */
    float *bar = (float*)((char*)c->vb + CUBE_DATA_OFF);
    float x0 = -0.7f, x1 = -0.7f + progress * 1.4f;
    float y0 = -0.65f, y1 = -0.55f;
    { float bv[6][12] = {
        {x0,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x0,y0,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x1,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0},
        {x0,y1,0,1, 0,1,0,0, 0.5f,0.5f,0,0}
      };
      for(int vi=0;vi<6;vi++) for(int fi=0;fi<12;fi++) bar[vi*12+fi]=bv[vi][fi];
    }

    struct PM4Builder pm4;
    pm4_init(&pm4, c->pm4_buf, 0x10000/4);
    int bi = c->flip_idx & 1;
    uint32_t fv = c->flip_idx + 100;
    *c->fence = 0;
    /* Model PS = ps_blue (white bar), Sky PS = ps_dark (dynamic — will render solid blue from desc) */
    uint32_t sz = build_dcb(&pm4, c->vs, c->ps_blue, c->ps_dark, 0, 0,
                            c->vb_v, c->bg_v, 0, 0,
                            c->vb, c->desc, 6, VERT_BUF_SIZE, 0, 0, 0,
                            bi ? c->fb1 : c->fb0, c->depth, 0, c->fence, fv,
                            1 /* EOP fence: a plain submit never runs gnm's marker
                                 patcher, so with the marker tail the fence was never
                                 written and every update waited out its timeout */);
    const uint32_t *a[1] = { c->pm4_buf };
    uint32_t s2[1] = { sz };
    sceGnmSubmitCommandBuffers(1, (void**)a, s2, 0, 0);
    g_submit_count++;   /* loading submits count toward any kernel-side budget */
    sceGnmSubmitDone();
    /* Bounded wait (was infinite). If the GPU faults the fence never signals;
       cap the wait so we still reach the main render loop instead of hanging
       here forever. */
    for (int w=0; w<10000 && *c->fence != fv; w++) sceKernelUsleep(100);
    sceVideoOutSubmitFlip(c->video, bi, 1, 0);
    sceKernelUsleep(16000);
    c->flip_idx++;
}

int main(void) {
    printf("=== ShadCube4 ===\n");

    /* userId 0xFF = SCE_USER_SERVICE_USER_ID_SYSTEM. Decompiled from the
       game's GPU init (eboot 0x133960), which opens video out as the SYSTEM
       user, twice - once as a probe that it immediately closes, then for real:
           sceVideoOutOpen(0xff, 0, 0, 0);
           sceVideoOutClose(probe);
           video = sceVideoOutOpen(0xff, 0, 0, 0);
       We were passing 0, which is not a defined user id (INVALID is -1,
       EVERYONE 0xFE, SYSTEM 0xFF). It is accepted and the display works, but
       binding the display to an undefined user is not something to rely on -
       a full-screen title opens it as SYSTEM. */
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


    /* Enable WAIT-FREE SUBMIT as early as the driver allows.
       libSceGnmDriver.prx exports seven library namespaces; one of them is
       libSceGnmWaitFreeSubmit, and it contains exactly two functions:
           +0x40 from sceGnmAreSubmitsAllowed : mode = 1 (enable)
           +0x90                              : mode = 0 (disable)
       They set a flag the driver pushes to the kernel via ioctl 0xc004811d on
       the NEXT sceGnmSubmitDone. This process had never issued that ioctl.
       Placed here because the loading screen submits and calls SubmitDone
       before the main loop, so enabling here gets it flushed early; the old
       placement was after the loading screen and missed those submits. */
    /* MODE 0, NOT 1. Decompiling gnm's packet_builder showed the submit mode
       selects a DIFFERENT KERNEL IOCTL:
           if (6 < mode) -> "submit mode error error (mode:%d)"
           (0x2d >> mode) & 1   ->  0,2,3,5 : ioctl 0xc0108102, 16-byte arg
                                    1,4,6   : ioctl 0xc020810c, 32-byte arg
       We were calling gnm_set_mode(1), so every submit went down 0xc020810c.
       THE GAME DOES NOT IMPORT libSceGnmWaitFreeSubmit AT ALL - it is mode 0
       and uses 0xc0108102. Two different ioctls, two different kernel
       handlers. We spent the whole investigation comparing packets, state,
       flip protocol and CPU path against the game while submitting down a
       path it never touches.

       Wait-free went in to remove a 510ms stall at frame 541, and it did -
       but the note taken at the time was already right: it is a BRAKE
       REMOVAL, and the GPU wedged permanently instead. The wedge is now known
       to be a DISPATCH failure (the CP completes one buffer and never starts
       the next), which is exactly what a different submit ioctl with
       different kernel bookkeeping could produce.

       Set WAITFREE_SUBMIT to 1 to go back to mode 1. */
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

    /* Shadow map: 4096×4096 (4K) — file-scope SHADOW_W/SHADOW_H drives this.
       Previously redefined locally to 512; that local override has been removed
       so the 4K file-scope value applies consistently. ~64 MB allocation. */
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
    /* Lens flare rays (tools/make_glare.py); black fallback = no rays. */
    static const unsigned char k_black[4] = {0, 0, 0, 0};
    Tex glare_tex = load_tex(ASSET_DIR "images/flare/glare.dds", k_black, 9);
    /* On-screen panels (src/ui.h): atlas + triple-buffered UI texture. */
    int ui_err = ui_init();
    {
        char L[48];
        int p = 0;
        const char* m = "ui_init: ";
        while (*m)
            L[p++] = *m++;
        p += lg_i64(L + p, (long long)ui_err);
        L[p++] = '\n';
        trace_line(L, (unsigned long)p);
    }
    Tex floor_nrm = load_tex(ASSET_DIR "images/floor/normal.dds", k_flat, 0);
    Tex floor_hgt = load_tex(ASSET_DIR "images/floor/height.dds", k_white, 0);

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
        1024, 0x100, MEM_TYPE_ONION); /* 256 dwords: ps_model uses up to desc[151] */
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

    /* Shadow map T# descriptor at desc[40:47] (byte offset 0xA0).
       DIAGNOSTIC: RGBA8 format (matches diagnostic CB_INFO=0x1A8).
       build_tsharp sets format=10 (8_8_8_8 UNORM), which is what we want. */
    /* Shadow map T# at desc[40..47]. Shadow pass writes shadow_depth as a COLOR
       target with CB_INFO.linear_general=1. We sample it as DisplayLinearAligned
       (tile_mode=8) because tile_mode=31 (DisplayLinearGeneral) crashes shadPS4
       in image_info.cpp:200 UpdateSize — ArrayLinearGeneral isn't handled in
       that switch. Both "linear" array modes are row-major pixel arrays; the
       aligned variant just requires pitch alignment which 512 satisfies. */
    if (shadow_depth) {
        /* Shadow T# = RGBA8. Matches the shadow DCB's CB0 write format (RGBA8)
           so shadPS4's texture_cache reuses a SINGLE Vulkan image for both the
           shadow-pass write and the floor-PS read. If we used R32_FLOAT here
           while the write path is D32_SFLOAT (old approach), Vulkan's depth/
           color format incompatibility (§39.1.6) would force shadPS4 to create
           two separate images — writes to one would not be visible in the other.
           build_tsharp sets format=10 (8_8_8_8 UNORM), tile_mode=8 which matches
           build_tsharp default and the new shadow CB0 write path. */
        build_tsharp(desc + 40, shadow_depth, SHADOW_W, SHADOW_H);
    }

    /* Floor T#s: albedo desc[64], normal desc[72], height desc[92] (sampler desc[100]). */
    build_tsharp_tex(desc + 64, &floor_alb);
    build_tsharp_tex(desc + 72, &floor_nrm);
    /* ps_floor parallax + fog: camera desc[104] (xyz per frame, w = log2(FOG_MIN)),
       then (pom_scale, fog rate, h_scale, h_bias) desc[108] and (normal sign x,
       sign y, 1/POM_FADE, 1/DISPLAY_H) desc[112]. Fog (ps_floor, ps_shader,
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
        fc[3] = my_log2(FOG_MIN);
        fc[4] = floor_hgt.err ? 0.0f : POM_DEPTH * (FLOOR_UV_MAX / (2.0f * FLOOR_HALF));
        fc[5] = -my_log2(FOG_MIN) / FOG_FULL;
        fc[6] = floor_hgt.err ? 0.0f : 1.0f;
        fc[7] = floor_hgt.err ? 1.0f : 0.0f;
        fc[8] = 1.0f;
        fc[9] = 1.0f;
        fc[10] = 1.0f / POM_FADE;
        fc[11] = 1.0f / (float)DISPLAY_H;
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
        mc[14] = 0.0f;
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
    /* Post-processing targets (fully rewritten every frame) and tables. */
    g_hdr = gpu_alloc((unsigned long)DISPLAY_W * DISPLAY_H * 8, 0x10000);
    int post_ok = g_hdr != 0;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        unsigned long sz = (unsigned long)g_bloom_pitch[i] * g_bloom_h[i] * 8;
        g_bloom_a[i] = gpu_alloc(sz, 0x10000);
        g_bloom_b[i] = gpu_alloc(sz, 0x10000);
        post_ok = post_ok && g_bloom_a[i] && g_bloom_b[i];
    }
    g_post_tab = (uint32_t*)gpu_alloc_typed((POST_PASSES + 2) * 32 * 4, 0x100, MEM_TYPE_ONION);
    if (post_ok && g_post_tab) {
        build_post_tables(g_post_tab);
        /* Final pass (ps_post_final): glare T# at dwords 40..47, bilinear clamp S# at 48..51. */
        uint32_t* fin = g_post_tab + (POST_PASSES - 1) * 32;
        build_tsharp_tex(fin + 40, &glare_tex);
        build_ssharp_clamp(fin + 48, 1);
        /* UI slots (dwords 64..83) valid before the first frame (the loading screen runs this
           pass too): hidden panels and a real T# - the UI buffer, or the glare texture if the
           UI failed - so no path through the shader can ever sample a null descriptor. */
        build_tsharp_tex(fin + 72, &glare_tex);
        build_ssharp_clamp(fin + 80, 0);
        ui_write_table(fin);
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

    /* Every shader bound in a draw must live in GPU-accessible memory — the GPU
       fetches code from the PGM_LO/HI address. Binding straight from the .rodata
       arrays (CPU-only ELF memory) faults the GPU MMU on real hardware and hangs
       (shadPS4 reads guest memory via its cache, so it never faulted there). */
    #define UPLOAD_SHADER(dst, src) \
        void *dst = gpu_alloc_typed(sizeof(src)+256, 0x1000, MEM_TYPE_ONION); /* shader code: coherent, GPU executes it */ \
        my_memcpy(dst, src, sizeof(src));
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
    /* Experiment disabled: do not burn 4 x 16KB of GPU memory on buffers
       nothing will submit. */
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

    /* --- Load 3D model using obj_loader.h --- */
    {
        /* The prop shipped in the package (tools/gen_model.py); the loader is
           picked by extension (.obj / .stl / .ply). */
        static const char* obj_paths[] = {ASSET_DIR "models/cube/cube.obj", 0};
        struct LoadCtx load_ctx;
        load_ctx.vs = vs; load_ctx.ps = ps;
        load_ctx.vb_v = vb_v; load_ctx.bg_v = bg_v;
        load_ctx.vb = vb; load_ctx.desc = desc;
        load_ctx.fb0 = fb[0]; load_ctx.fb1 = fb[1];
        load_ctx.depth = depth; load_ctx.fence = fence;
        load_ctx.video = video; load_ctx.pm4_buf = dcb_mem[0];
        load_ctx.flip_idx = 0;
        load_ctx.ps_blue = ps_blue_gpu;
        load_ctx.ps_dark = ps_dark_gpu;
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
                err = obj_load_file(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
            } else if (dot[1]=='s' && dot[2]=='t' && dot[3]=='l') {
                err = stl_load_binary(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
            } else if (dot[1]=='p' && dot[2]=='l' && dot[3]=='y') {
                err = ply_load_file(obj_paths[pi], gpu_alloc, &mesh, loading_progress, &load_ctx);
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
                /* Reset BG normals to zero after loading (loading_progress sets z=0.25) */
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
            /* Restore cube verts and BG — loading_progress may have overwritten them
               during failed OBJ attempts (progress-bar geometry written at CUBE_DATA_OFF
               plus BG tweaks). Rebuild the static VB to recover the original cube. */
            build_static_vb((float*)vb);
            printf("No model found, using built-in cube.\n");
        }
    }

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
    float ground_y = -0.05f;   /* ground level */
    float eye_height = 0.15f;  /* camera height above ground */
    int day_frozen = 0;        /* Square */
    int day_step = 0;          /* index into k_day_mults (L1 / R1) */
    float day_hold = 0.0f;     /* L1 / R1 auto-repeat timer */
    float play_time_s = 0.0f;  /* this session, for the leaderboard */
    int cube_rotation_enabled = 1;  // Start button toggles this (default: spinning)
    float cube_angle_y = 0.0f;       // accumulator (advances only when enabled)
    float cube_angle_x = 0.0f;
    /* Start of the day: sun_angle 0 is the first day frame (sun_y = 0, so
       is_night = (sun_y < 0) is false) - the sun on the eastern horizon.
       The default camera faces west, so the sun itself comes into view later,
       near sunset. */
    float sun_angle = 0.0f;
    float sun_speed = 0.0027f;  // 40% slower than 0.0045 (= 76% slower than original 0.01125)
    uint32_t prev_buttons = 0;

    /* Flush the CPU write-combine buffers before the first frame.
       Textures, the framebuffers' initial contents and the model-loader
       staging arrays live in GARLIC (write-combine) and were filled by CPU
       stores that drain asynchronously. Everything the CPU rewrites per frame
       has been moved to ONION, so this only has to happen ONCE - which is
       exactly what the API is for. Without it the first frames can sample
       texels the GPU has not seen yet. */
    /* Declare which cores we may run on, as the game does. It calls
       scePthreadSetaffinity 7 times; the general worker mask it uses is 0x3f
       (cores 0-5), and it pins the thread that owns SubmitDone /
       UnmapComputeQueue / VideoOutClose to core 0 (mask 0x1).
       We set 0x3f rather than pinning to one core: it matches the reference's
       general mask and leaves the scheduler free to keep our hot fence spin off
       whichever core SceVideoOutServiceThread happens to be on. That thread
       lives in OUR process (libSceVideoOut creates it) and on a base PS4 it
       does display housekeeping every 100ms - starving it is not something to
       leave to chance.
       HONEST NOTE ON WHAT THIS DOES AND DOES NOT DO: 0x3f is cores 0-5, and
       the videoout service thread FLOATS - traced it, its prio/affinity come
       from an internal config block at module init (vo 0x580, below the export
       range) and NO public API reaches them, so it is free to use those same
       six cores. This mask therefore does NOT separate us from it. It is
       PARITY WITH THE GAME, not contention avoidance.
       A narrower mask WOULD separate us, but pinning the main thread costs
       every other thread in the process, and "our spin starves the display
       thread" is a hypothesis with no measurement behind it - the spin
       normally exits in ~30us. If hwstall= comes back 1, that hypothesis moves
       up the list and a subset mask becomes worth testing on evidence.
       Failure is non-fatal: if the call is unavailable we simply keep the
       default affinity. */
#if SET_AFFINITY
    { void *self = scePthreadSelf();
      g_affinity_ret = self ? scePthreadSetaffinity(self, 0x3fULL) : -1; }
#else
    g_affinity_ret = -98;   /* not attempted */
#endif

#if MAP_COMPUTE_QUEUES
    /* Map two compute queues exactly as the game does at init (pipe 0 queue 4,
       pipe 1 queue 4, ring 0x1000 dwords). We dispatch nothing to them - see
       the note at MAP_COMPUTE_QUEUES for why they may still matter.
       PRX guards (gnm 0x3cf0): pipe <= 6, queue <= 7, ring 4-byte aligned.
       ONION so the read-pointer the kernel writes is CPU-visible.
       Returns lea eax,[r15+rbx*8] (gnm 0x3dcf); measured 5 and 13 on hardware, so
       r15 is not the raw queue index (queue+1 fits both). Errors are 0x80d170xx, which
       are negative as int32, so >= 0 separates id from failure cleanly and
       Unmap gets exactly the id Map returned. */
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
    /* Not allocated: its only user is inside #if BATCH_FRAMES > 1, so at
       BATCH_FRAMES == 1 this was reserved GPU memory nothing could write. */
    void *batch_stage = (void*)1;   /* non-NULL so the check below passes */
#endif
    /* Was a bare `return 1`, and it sits AFTER sceVideoOutRegisterBuffers
       (line ~2350) - so it would exit with video-out still registered and the
       OS blocking on teardown. Use the unwinding path like every other fatal. */
    if (!batch_stage) FATAL_EXIT("batch staging alloc failed");

    /* Tell the system we have finished loading, so it tears down its splash
       screen. Until this is called the system keeps the splash up and keeps
       compositing it over us. Every real title calls it exactly once -
       confirmed in a God of War trace ("sceSystemServiceHideSplashScreen:
       called"). We never have.
       Why this is the prime suspect: the wall is at a fixed PROCESS TIME, not
       a submit count. Frame 0 runs at ptms=2183 and the first 500ms submit
       lands at ptms=11110, no matter how many frames fit between. That is the
       signature of a startup obligation timing out, not a GPU budget.
       And the count model is dead regardless: God of War issues ~18 GFX submits
       per frame (~535/sec) and runs for hours, so no 512-submit budget exists. */
    /* Set gnm's unnamed driver mode before rendering starts. It reaches the
       kernel only through ioctl 0xc004811d at the next SubmitDone, and this
       process has never issued that ioctl. Speculative but cheap and safe
       (byte-validated; a layout mismatch makes it a no-op). */
    /* Retry if the driver was not ready at the earlier attempt. */
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

    int splash_ret = sceSystemServiceHideSplashScreen();
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
    /* The loading screen wrote EOP stamps 2..5 too: clear them, so a stamp read on
       a stalled frame (rts= in the trace) is from that frame. */
    if (g_gpu_ts)
        for (int q = 0; q < 6; q++)
            g_gpu_ts[q] = 0;
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

        /* Section timing to locate the per-frame stall. */
        /* Decide NOW whether this frame will be logged, so the 14 timestamps
           below cost nothing on the 15 frames out of 16 that are discarded.
           Mirrors the condition used by the trace block at the end. */
        g_trace_this_frame = ((frame % 16) == 0) || (g_slow_tail > 0);

        /* REAL elapsed time for this frame, in seconds.
           Animation used to advance by a fixed amount PER FRAME, so the scene's
           speed WAS the frame rate - at 1.8fps the sun crawled. That doubled as
           a diagnostic while we were chasing the stall, but it is wrong: motion
           should be tied to time, not to how fast we happen to be rendering.
           Clamped to 100ms so a stalled frame cannot teleport the scene, and
           the first frame gets a nominal 1/60 rather than a garbage delta. */
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

        // Read gamepad
        if (pad_handle >= 0)
            scePadRead(pad_handle, &pad, 1);
        uint64_t t_pad = tstamp();

        // Button edge detection (pressed this frame, not last)
        uint32_t pressed = pad.buttons & ~prev_buttons;
        prev_buttons = pad.buttons;

        /* Controls (the on-screen list, src/ui.h, shows the same):
           Cross     freeze / unfreeze the cube      Circle  show / hide the leaderboard
           Square    freeze / unfreeze day and night Triangle reset the camera
           L1 / R1   day and night slower / faster (k_day_mults; held: repeats)
           L2 / R2   camera down / up                 D-pad up / down: camera speed
           D-pad left / right: move the sun           OPTIONS: show / hide controls
           sticks: move / look. */
        if (pressed & PAD_CROSS)
            cube_rotation_enabled = !cube_rotation_enabled;
        if (pressed & PAD_CIRCLE)
            g_ui.leaderboard = !g_ui.leaderboard;
        if (pressed & PAD_SQUARE)
            day_frozen = !day_frozen;
        if (pressed & PAD_OPTIONS)
            g_ui.controls = !g_ui.controls;
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

        /* Left stick: move forward/back + strafe (per second: x60 of the old per-frame step) */
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

        /* D-pad up / down: camera speed (x1.02 / x0.98 per frame held), 1% .. 2500% of
         * MOVE_SPEED_100 */
        if (pad.buttons & PAD_UP)
            move_speed *= 1.02f;
        if (pad.buttons & PAD_DOWN)
            move_speed *= 0.98f;
        if (move_speed < 0.01f * MOVE_SPEED_100)
            move_speed = 0.01f * MOVE_SPEED_100;
        if (move_speed > 0.5f)
            move_speed = 0.5f;

        /* Day and night: D-pad left / right held moves the sun; otherwise it runs at the
           chosen multiple unless frozen. */
        if (pad.buttons & PAD_LEFT)
            sun_angle -= 0.2592f * dt_sec;
        if (pad.buttons & PAD_RIGHT)
            sun_angle += 0.2592f * dt_sec;
        if (!day_frozen)
            sun_angle += sun_speed * (float)k_day_mults[day_step] * 60.0f * dt_sec;
        play_time_s += dt_sec;
        /* Wrap every accumulator once per frame, after all increments. */
        sun_angle    = wrap_2pi(sun_angle);
        cam_yaw      = wrap_2pi(cam_yaw);

        /* BG lighting handled by PS via light direction */

        /* === Per-frame cube rotation ===
           The cube has been static at world origin since the first build because no
           rotation was ever applied. Rotate around Y axis (vertical) so the user
           can see how the shadow shape changes with cube orientation — important
           Standard cube test rotation: classic two-axis demo spin (rotate
           around Y at one rate, around X at a slightly different rate) so
           every face of the cube becomes visible over time. Same rotation
           is applied to positions AND normals so the per-face Lambert
           lighting in the cube PS tracks the geometry. */
        {
            /* Cube rotation angles advance ONLY when cube_rotation_enabled.
               Start button (PAD_OPTIONS) toggles the flag; when disabled the
               accumulators freeze, so the cube holds its current orientation
               until rotation is re-enabled. */
            if (cube_rotation_enabled) {
                cube_angle_y += 0.78f * dt_sec;   /* was 0.013/frame */
                cube_angle_x += 0.42f * dt_sec;   /* was 0.007/frame */
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
        if (stars_vb)
            my_memcpy(stars_vb, (char*)vb + MVP_OFF, 64);

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
            /* Night: MOON_LIGHT (0.621 = the previous 0.69 - 10%) */
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
           NIGHT:                      desaturated cool blue (0.52, 0.64, 0.84) */
        float light_r, light_g, light_b;
        if (is_night) {
            /* Moon: desaturated cool blue */
            light_r = 0.52f; light_g = 0.64f; light_b = 0.84f;
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
            lc[0] = srgb_to_linear(light_r);
            lc[1] = srgb_to_linear(light_g);
            lc[2] = srgb_to_linear(light_b);
            lc[3] = 1.0f;
        }

        /* Dynamic sky colors based on REAL sun elevation (orig_sun_y, before
           the moon swap). The sun_y variable was negated for the night-light
           swap above, so it's >=0 always now and would mis-classify night as
           day. Use orig_sun_y to keep the sky correctly dark at night even
           while the moon lights up the scene. */
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
           MVP). Placement as before: sun = direction x the day magnitude ramp,
           moon = anti-sun x 0.69, both x 100; the moon disc has the sun's radius (same
           apparent size: 400x smaller, 400x closer). ps_dark: f = clamp(1 - d^2 /
           radius^2, 0, 1)^2, d in aspect-corrected NDC (x * W/H, y). */
        {
            float *mvp = (float*)((char*)vb + MVP_OFF);
            const float sun_r = SUN_DISC_RADIUS_PX / ((float)DISPLAY_H * 0.5f);
            float mag = 0.98f + 0.40f * (orig_sun_y / 0.15f);
            if (mag < 0.98f)
                mag = 0.98f;
            if (mag > 1.38f)
                mag = 1.38f;
            const float body[2][4] = {
                {sun_dx * mag, sun_dy * mag, sun_dz * mag, sun_r * sun_r},
                {-sun_dx * 0.69f, -sun_dy * 0.69f, -sun_dz * 0.69f, sun_r * sun_r}};
            for (int k = 0; k < 2; k++) {
                float bx = body[k][0] * 100, by = body[k][1] * 100, bz = body[k][2] * 100;
                float cx = mvp[0] * bx + mvp[1] * by + mvp[2] * bz + mvp[3];
                float cy = mvp[4] * bx + mvp[5] * by + mvp[6] * bz + mvp[7];
                float cw = mvp[12] * bx + mvp[13] * by + mvp[14] * bz + mvp[15];
                float* sd = (float*)(desc + 16 + 4 * k);
                if (cw > 0.01f) {
                    sd[0] = (cx / cw) * ((float)DISPLAY_W / (float)DISPLAY_H);
                    sd[1] = cy / cw;
                } else { /* behind the camera */
                    sd[0] = 99.0f;
                    sd[1] = 99.0f;
                }
                sd[2] = body[k][3];
                sd[3] = 0.0f;
            }
            /* Sun disc: amber at the horizon -> SUN_DAY_* (pale warm) at orig_sun_y
               0.15, held amber while it sets. Moon: cool blue. At SUN_HDR the core
               still clips to white; the tint shows on the rim and in the bloom. */
            float kc = orig_sun_y / 0.15f;
            if (kc < 0.0f)
                kc = 0.0f;
            if (kc > 1.0f)
                kc = 1.0f;
            float* sc = (float*)(desc + 84);
            sc[0] = srgb_to_linear(1.00f + (SUN_DAY_R - 1.00f) * kc) * SUN_HDR;
            sc[1] = srgb_to_linear(0.64f + (SUN_DAY_G - 0.64f) * kc) * SUN_HDR;
            sc[2] = srgb_to_linear(0.44f + (SUN_DAY_B - 0.44f) * kc) * SUN_HDR;
            sc[3] = 0.0f;
            float* mc = (float*)(desc + 88);
            mc[0] = srgb_to_linear(0.52f) * MOON_HDR;
            mc[1] = srgb_to_linear(0.64f) * MOON_HDR;
            mc[2] = srgb_to_linear(0.84f) * MOON_HDR;
            mc[3] = 0.0f;
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
            float nx = sd[0] * ((float)DISPLAY_H / (float)DISPLAY_W), ny = sd[1];
            float fu = 0.5f + 0.5f * nx, fv = 0.5f - 0.5f * ny, k = 0.0f;
            float e = fu < 1.0f - fu ? fu : 1.0f - fu;
            if (fv < e)
                e = fv;
            if (1.0f - fv < e)
                e = 1.0f - fv;
            if (sd[0] < 90.0f && e > 0.0f) {
                float fade = e / FLARE_EDGE;
                if (fade > 1.0f)
                    fade = 1.0f;
                const float cam[3] = {cam_x, cam_y, cam_z};
                k = FLARE_STRENGTH * fade * fade * (3.0f - 2.0f * fade) *
                    flare_visibility(cam_yaw, cam_pitch, cam, nx, ny) / SUN_HDR;
            }
            ft[0] = fu;
            ft[1] = fv;
            ft[2] = k * scl[0];
            ft[3] = k * scl[1];
            ft[4] = k * scl[2];
            ft[5] = FLARE_GHOSTS; /* dwords 33..36: gains; 37..39 spare; 40..51 glare T# / S# */
            ft[6] = FLARE_RAYS * GLARE_STORE_MAX;
            ft[7] = FLARE_GLOW;
            ft[8] = FLARE_VEIL;
            ft[9] = 0.0f;
            ft[10] = 0.0f;
            ft[11] = 0.0f;
            ui_update((int)(move_speed / MOVE_SPEED_100 * 100.0f + 0.5f), k_day_mults[day_step],
                      day_frozen, (unsigned long)play_time_s);
            ui_write_table(g_post_tab + (POST_PASSES - 1) * 32);
        }

        /* Build main DCB (samples shadow_depth but doesn't write it).
           The DCB itself requests a GPU-side depth clear via DB_RENDER_CONTROL.depth_clear_enable=1
           and DEPTH_CLEAR=1.0f on the first draw — shadPS4 translates this to a Vulkan
           loadOp=Clear on the depth attachment. A CPU linear memset won't work because
           the depth buffer is GPU-tiled. */
        uint64_t t_pre_build = tstamp();
        /* BATCHING: the submit quota is per IOCTL CALL, not per command buffer
           or per frame (proven: 2 command buffers per call walled at the same
           CALL count, not half). So we accumulate BATCH_FRAMES frames into ONE
           command buffer and submit once. Only init the builder at the start of
           a batch; sub-frames append to it. */
        if (batch_pos == 0) {
            phase("batch-start");
            pm4_init(&pm4, dcb_mem[dcb_slot], DCB_SIZE/4);
            if (g_gpu_ts) pm4_gpu_timestamp(&pm4, &g_gpu_ts[0]);
            batch_frame0 = frame;
        }

        /* Label sampled before this frame's work. gnm's marker patch injects
           WRITE_DATA(label[bi]=1); the display clears it on flip completion
           (confirmed on hw: labpre=0x0, labpost=0x1 every frame). */
        uint32_t lab_pre = label_ok ?
            ((volatile uint32_t*)flip_label_base)[bi*2] : 0xffffffffu;

        /* InsertWaitFlipDone is part of the marker/label protocol, which the
           CPU-flip path below does not use. The EOP fence wait + flip-event
           pace handle buffer-release ordering instead. Disabled for this test. */
        int wfd = -1;

        /* Shadow pass, built into the SAME command buffer, ahead of the main
           pass. gnm 0x8b0 emits ONE 16-byte IB packet per command buffer
           (0xc0023f00), so 2 buffers cost 2 IB packets per submit; merging
           halves that. Semantically identical: the CP already runs a submit's
           buffers back-to-back, and each builder emits its own hw state.
           DIAGNOSTIC: if the wall is ring SPACE it should move ~512 -> ~1024
           submits; if it is a per-submission fence array it stays at 512. */
        /* DCB SIZE EXPERIMENT: pad with TYPE3 NOPs to inflate the command
           buffer without changing what it renders (the CP skips NOP packets).
           Every earlier test held DCB size constant - merging the shadow pass
           changed the IB DESCRIPTOR count but not the total bytes - so the
           "fixed byte budget" hypothesis has never been tested.
             wall moves to ~1/4 the frames -> the limit is BYTES submitted
             wall stays at ~512 frames     -> the limit is the SUBMIT COUNT
           One TYPE3 NOP carries up to 0x3fff payload dwords; emit the header
           then skip its payload, which stays uninitialised but is never read. */
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

        /* CPU-flip path (is_eop=FALSE). Submit the DCB with NO flip marker via
           plain sceGnmSubmitCommandBuffers, wait the EOP fence so the GPU has
           finished rendering into fb[bi], then queue the flip from the CPU with
           sceVideoOutSubmitFlip. This does NOT depend on the marker EOP GfxFlip
           IRQ firing - the flip is queued by the CPU call and run by the GPU
           thread. If the 500ms wall was the marker/EOP flip ack never arriving,
           this path removes it. submit= and flip= are timed separately so the
           trace shows exactly which call (if any) still blocks. */
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
            /* The game's flip path: ONE call that submits and registers the
               flip, with the marker block at the DCB tail carrying our fence.
               eboot 0x94edf0 passes (1, &dcb, &size, 0, 0, flipMode, ...). */
            if (FORCE_NO_FLIP || CPU_FLIP || g_display_stalled) {
                /* NO-FLIP submit. The flip is registered by
                   sceGnmSubmitAndFlipCommandBuffers itself, via the marker -
                   NOT by our flip loop. So skipping the WAIT (fskip) never
                   stopped flips being queued, and the queue still filled to
                   0x80d11081. Measured: fskip climbed to 33 while the queue
                   overflowed anyway.
                   Once the display has stalled we therefore submit the
                   driver's OTHER documented path - EVENT_WRITE_EOP, no marker,
                   plain sceGnmSubmitCommandBuffers - which keeps the GPU fed
                   and the fence observable WITHOUT adding to the flip queue,
                   giving the display a chance to drain. */
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
            /* ONE SubmitDone, no spin. Measured: drn hit its 64 cap on EVERY
               frame, so sceGnmAreSubmitsAllowed never returns 1 and the spin
               never converged - SubmitDone cannot drive the counter to 0. And
               past the wall each SubmitDone blocks 512003us, so 64 of them cost
               32.77 SECONDS. sdret/sdret2 were 0x0 throughout, so SubmitDone
               always SUCCEEDS; it is blocking, not failing. */
            /* SubmitDone is DEFERRED to once per frame, after every submit
               including the keep-alives (see below). The game submits ~17
               times per frame and calls SubmitDone ONCE - 971 calls for 969
               frames in its log. SubmitDone rings the DingDong doorbell, a
               64-entry ring whose pending count saturates and is decremented
               only by the kernel, plus a drain and a ready poll; issuing it
               per submit was ringing that doorbell 4x per frame on a ring we
               otherwise never use. */
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
        /* The 2fps trace showed dt=478ms with only 139us across every existing
           timer - 478252us unaccounted, all of it inside (t_submit .. now).
           phase() returns early below batch 500 and only buffers to memory, and
           fenceit=0 means the fence wait never looped. So the cost is somewhere
           none of the current timers bracket. These three split that region
           exactly, with REAL timestamps (not gated) so they are valid on every
           frame regardless of logging. */
        uint64_t t_w0 = sceKernelGetProcessTime();
        phase("pre-flips");
        uint64_t t_flip0 = tstamp();
        uint64_t t_w1 = sceKernelGetProcessTime();
        int flip_ret = 0;
        for (int k = 0; k < BATCH_FRAMES; k++) {
            /* Reset the budget PER sub-frame. fence_iters was shared across
               all 16 waits, so once it reached the cap the later frames would
               be flipped without ever confirming the GPU had finished
               rendering them. */
            /* BOUNDED AT ~200ms, and it does NOT abort the frame.
               The old cap was 200000 x usleep(10); if the real sleep
               granularity is ~100us that is twenty seconds of apparent freeze.
               Measured: the fence normally signals in 1-2 iterations, then at
               frame ~541 it stops entirely - the GPU stops completing work.
               Timing out here and carrying on lets us see whether the GPU ever
               recovers, and what value the fence is stuck at, instead of
               hanging the loop. */
            /* SPIN FIRST, then sleep. Decompiled from the game (frame function
               at eboot 0x132370), its fence wait is a bare hot spin with no
               sleep and no timeout at all:
                   while (*(int*)(ctx+0x3a0) != *fence_ptr)
                       fence_ptr = *(int**)(ctx+0x398);
               We were calling sceKernelUsleep(100) per iteration, and the
               MEASURED granularity floor on this hardware is ~1ms regardless of
               the value asked for. With fenceit measured at 1-2 and the fence
               signalling in tens of microseconds, we were sleeping ~1ms for
               something ready in ~30us - a ~30x overshoot out of a 16.6ms frame,
               enough to miss a vblank under any extra load.
               So: spin like the game for the common case, then fall back to
               sleeping so a genuinely stalled fence cannot burn a core, and
               keep the bounded timeout the game does not have. */
            /* Bounded hot spin, like the game's own fence wait (eboot 0x132370
               spins with no sleep). The previous 1 ms usleep steps noticed GPU
               completion up to ~1 ms late, and in this serial loop that delay
               sits directly before SubmitFlip and the vblank deadline. The
               clock is read every 32 pause16 batches; fence_iters is now the
               wait in microseconds (trace field fwait).
               Once the fence has demonstrably stopped, STOP PAYING FOR IT:
               after a handful of timeouts the budget drops to 2 ms so a dead
               GPU cannot make the app miss the system's quit event (the crash
               on close), while a recovery is still detectable. */
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
                /* With PACE_ON_FENCE_ONLY the flip event is no longer waited
                   on, so the fence is our ONLY liveness signal. Drive the
                   no-flip fallback from it, otherwise a wedged GPU would keep
                   getting flips queued at it until the queue returns
                   0x80d11081. A few timeouts, not one, so a single slow frame
                   does not trip it.
                   THE MARGIN IS NOW KNOWN, not guessed: shadPS4's videoout
                   driver rejects a flip once flip_pending_num > 16, and that
                   matches the hardware exactly - our fence died at 548 and
                   0x80d11081 arrived at 563, fifteen frames of one un-retired
                   flip each. Tripping at 4 leaves twelve frames of headroom. */
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
            /* DO NOT FLIP A FRAME THE GPU NEVER FINISHED.
               Measured on hardware: once the fence stalls, flipping anyway
               queues a flip that the display can never complete, the buffer
               label stays 1, and after ~15 frames the flip queue is FULL -
               every submit then returns 0x80d11081 ("flip queue is full",
               confirmed at gnm 0xdb6). That turns one stalled fence into a
               permanent failure AND leaves teardown with a queue of flips that
               will never retire, which is what crashes the app on close.
               Skipping the flip keeps the queue drainable and the app alive. */
            /* Count it, but do NOT `continue`. The flip is issued by the
               SUBMIT (SubmitAndFlip via the marker), not here, so skipping the
               rest of this loop never stopped a flip - it only skipped the
               event wait, which is the one place a recovery can be observed.
               Stopping the flips is the no-flip submit above; this is just
               bookkeeping. */
            if (!fence_ok) g_flips_skipped++;

#if CPU_FLIP
            /* THE CPU FLIP. The DCB was built with no_flip=1 and submitted
               with plain sceGnmSubmitCommandBuffers, so no marker and no EOP
               flip was registered. The GPU has now signalled the fence, so the
               frame is complete and it is safe to present it from here.
               Skipped if the fence never arrived, for the same reason the
               marker path skips it: presenting an unfinished frame queues a
               flip that can never retire. */
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
                /* BOUNDED. This was called with a NULL timeout pointer, i.e.
                   block forever, and it is the only unbounded wait between
                   "pre-flips" and "batch-end" - which is exactly where the app
                   stops. If the flip event ever fails to arrive, the frame
                   loop freezes here and the app looks hung. 100ms lets us
                   survive a missing event and count it instead. */
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
                int wr = sceKernelWaitEqueue(flip_eq, &ev, 1, &out, &tmo);
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
        /* THE one SubmitDone for this frame, issued after EVERY submit
           (the frame DCB and all keep-alives) and after the flips. The game
           does exactly this: ~17 submits per frame, 971 SubmitDone calls for
           969 frames. Doing it per submit was ringing the DingDong doorbell
           four times a frame on a 64-entry ring we otherwise never use. */
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

        /* Per-frame trace. Discriminator: subc=(total submits) vs ptms=(wall
           clock). If the stall onset correlates with subc -> ring-fill/count
           (fixable). If with ptms only -> pure time-based kernel IRQ death. */
        /* Trace EVERY batch. With BATCH_FRAMES=16 this point is reached only
           ~3.75 times a second, so full coverage is cheap - and a long run is
           exactly what is needed to confirm the wait-free-submit result holds
           well past the ~11s mark where every earlier build stalled. The old
           windowed condition was written for per-frame submission and went
           quiet after frame 620. */
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
        /* Ask the KERNEL whether the GPU is healthy. This is a real ioctl
           (0xc0088111), so sample it only on frames we log - it is a
           diagnostic, not something to pay for 60 times a second. If hwok
           drops to 0 at the same frame cpm freezes, the kernel can see the
           wedge too and it is genuinely a hardware stall; if hwok stays 1
           while cpm is frozen, the kernel believes the GPU is fine and our
           command buffers are being dropped somewhere between the two. */
        /* MEASURED AT 477 MILLISECONDS PER CALL. This probe was tied to the
           trace condition, and at 2fps the adaptive logging turns every frame
           into a logged frame - so it ran every frame and held the app at 2fps
           by itself. The probe caused the slowdown, the slowdown enabled full
           logging, and full logging ran the probe: a closed loop, which is why
           the app was 2fps from frame ZERO rather than degrading into it.
           It is an ioctl into the graphics driver and it is evidently not cheap
           on retail. Sample it RARELY - once every 1024 frames - so hwok still
           reports the kernel's verdict without dominating the frame. The
           at-stall sample (g_fence_timeouts == 1) is untouched: it fires once
           per stall, which is exactly when its cost does not matter. */
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
            /* 3072. At 64 fields the pessimistic bound is ~1984, which left
               64 bytes in a 2048 buffer - the same margin that nearly bit us
               when this was L[896] with 47 fields. Size it for the fields we
               have plus room to add more. */
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

    /* Clean shutdown, matching the game's teardown (eboot 0xcf65c):
         SubmitDone -> pause-spin until AreSubmitsAllowed -> SubmitDone -> spin
       AreSubmitsAllowed returns 1 once the driver's in-flight counter is 0, so
       this is the driver-level quiesce. Bounded here so a wedged driver cannot
       hang the exit; the game spins unbounded. */
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
    /* TEARDOWN BUDGETS. These were bounded but at absurd limits, and the real
       sleep granularity on this hardware is ~1ms regardless of the value asked
       for (measured: sceKernelUsleep(100) sleeps ~1ms). So the old caps were:
           td_fence  1000000 x usleep(10)   = ~1000 s   (SIXTEEN MINUTES)
           td_flip    100000 x usleep(200)  = ~100 s
           td_q1/q2  2000000 x cpu_pause16  = a long spin, and
                     sceGnmAreSubmitsAllowed was MEASURED never to return 1 in
                     this app, so BOTH ran to their full count every time.
       If the fence is stuck - the exact failure we are chasing - the app would
       sit in teardown for minutes, the OS would wait, and then kill it. That
       is the "hangs on close, then crashes" behaviour.
       Now: roughly 250ms per stage, then give up and close anyway. Closing
       cleanly with work still outstanding is far better than not closing. */
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
    /* Only unregister if the flips actually drained. If the GPU stalled, the
       display still holds buffers for flips that will never complete, and
       pulling the registration out from under them is what turns a stalled
       frame into a crash on close. The game never unregisters at all - its
       teardown is SubmitDone / UnmapComputeQueue / VideoOutClose, and Close
       releases the buffers - so skipping this is the reference behaviour, not
       a shortcut. td_pend is the pending-flip count we just measured. */
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
