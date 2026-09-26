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

#include "pm4.h"
#include "logo_texture.h"
#include "nid_resolve.h"
#include "obj_loader.h"
#include "stl_loader.h"
#include "ply_loader.h"
#include "bmp_loader.h"

// memset/memcpy declared in nid_resolve.h

#define DISPLAY_W       1920
#define DISPLAY_H       1080

/* Mip levels for the floor albedo/normal maps (power-of-two sizes only, down to
   16x16; see bmp_loader.h). 1 = no mips, as before. */
#define FLOOR_TEX_MIPS 9

/* Sun disc edge radius in pixels (1080p). The old disc was 59 px tall and
   105 px wide (NDC distance on a 16:9 screen); it is now round. */
#define SUN_DISC_RADIUS_PX 110.0f

/* Printed in the trace header so logs from different builds can be told apart. */
#define BUILD_TAG "cleanup-diagnostics"
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
#define GPU_TS(k) do { if (g_gpu_ts) pm4_gpu_timestamp(b, &g_gpu_ts[(k)]); } while (0)





/* Exports patched: pos0 now has DONE (was on param1). The last position export
   must carry DONE for the primitive assembler to proceed (radeonsi: done=1 on the
   last pos export, never on params). Without it frame 0 never retired on hardware. */
static const uint32_t vs_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000,
    0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085,
    0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601,
    0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C,
    0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13,
    0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E,
    0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80008CF,
    0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF800021F, 0x06040302, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0x47505508, 0xAABBEE02, 0x00000000,
};

// Dedicated shadow-pass VS. Identical bytes to vs_shader_binary but with a
// DIFFERENT hash (0xBEEFEE02CAFE5508 vs 0xAABBEE0247505508). Forces shadPS4 to
// compile and cache a distinct Vulkan pipeline for the shadow pass, so any
// pipeline-cache collision between main-pass VS and shadow-pass VS — which could
// produce double-rendering of the cube into shadow_depth — is eliminated.
/* Same export DONE patch as vs_shader_binary. */
static const uint32_t vs_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0x7E020280, 0xE0381000,
    0x80000C01, 0xE0381010, 0x80001001, 0xE0381020,
    0x80001401, 0xE0381030, 0x80001801, 0x34020085,
    0x34040084, 0x4A020501, 0x4A0202C0, 0x4A020290,
    0xE0381000, 0x80000201, 0xE0381010, 0x80000601,
    0xE0381020, 0x80002C01, 0xBF8C0070, 0x1038050C,
    0x1040070D, 0x0638411C, 0x1040090E, 0x0638411C,
    0x10400B0F, 0x0638411C, 0x103A0510, 0x10400711,
    0x063A411D, 0x10400912, 0x063A411D, 0x10400B13,
    0x063A411D, 0x103C0514, 0x10400715, 0x063C411E,
    0x10400916, 0x063C411E, 0x10400B17, 0x063C411E,
    0x103E0518, 0x10400719, 0x063E411F, 0x1040091A,
    0x063E411F, 0x10400B1B, 0x063E411F, 0xF80008CF,
    0x1F1E1D1C, 0x7E4602F2, 0xF800020F, 0x08072D2C,
    0xF800021F, 0x05040302, 0xBF810000, 0xBF800000,
    0x5362724F, 0x00726468, 0x0000F004, 0x00000000,
    0xCAFE5508, 0xBEEFEE02, 0x00000000,
};

// Projective shadow PS.
// Interpolates world-space position (attr1.xyz) per-fragment, loads light_MVP from
// desc[48..63] (64 bytes at byte offset 192), transforms wpos through light_MVP to
// light-clip space, performs perspective divide, converts NDC to shadow UV,
// samples shadow_depth.r, and outputs (sample.r, 0, 0, 1) for visualization.
//
// VGPR usage: v0,v1 (barycentrics), v20,v21 (shadow UV), v24-v27 (wpos xyzw),
//             v28-v31 (light-clip xyzw), v32,v33,v34 (scratch + rcp_w), v40-v43 (RGBA out)
// SGPR usage: s0,s1 (desc ptr from user_data), s12-s15 (sampler), s16-s23 (shadow T#),
//             s32-s47 (light_MVP 4x4)
// Desc layout: desc[8..11] sampler, desc[40..47] shadow T#, desc[48..63] light_MVP
// PROJECTIVE SHADOW PS (now active): loads light_MVP from desc[48..63] via
// s_load_dwordx16, interpolates wpos.xyzw, multiplies wpos * light_MVP to get
// clip-space shadow coords, perspective-divides, computes shadow UV, samples
// shadow_depth, outputs sample.r as red intensity.
// Resource layout per shadPS4 recompiler:
//   s[0:1] = desc base, s[4:11] = shadow T# (desc[40..47]),
//   s[12:15] = sampler (desc[8..11]), s[32:47] = light_MVP (desc[48..63])
// === Cube PS (hash CAFE0114) — textured + 3D Lambert + light_color tint ===
// (Hash bumped from CAFE0113 → CAFE0114 to defeat shadPS4 pipeline cache after
//  adding the light_color sample + tint multiply ops.)
//
// Samples the main `tex` texture (desc[0..7]) using interpolated UV from the
// VS, then modulates by full 3D Lambert lighting using all three normal
// components (x, y, z) against the sun direction. Final output is tinted by
// `light_color` from desc[32..35] — at sunset/sunrise the tint is warm
// orange so the cube takes on the sky's color; at night it's cool blue.
//
// VS exports (post-fix in vs_shader_binary):
//   PARAM0 = (uv.x, uv.y, normal.y, normal.z)
//   PARAM1 = (world_pos.x, world_pos.y, world_pos.z, normal.x)
//
// Resource bindings:
//   s12-15  sampler       @ desc[8]   (build_ssharp)
//   s16-23  tex T#        @ desc[0]   (build_tsharp — main BMP or embedded logo)
//   s24-27  sun direction @ desc[12]  (sun.x=s24, .y=s25, .z=s26)
//   s28-31  light_color   @ desc[32]  (s28=R, s29=G, s30=B)
//
// VGPR map:
//   v3, v4   = uv.x, uv.y (interpolated)
//   v10, v11 = normal.y, normal.z (from attr0.z, attr0.w)
//   v12      = normal.x (from attr1.w — packed by VS)
//   v16-v19  = albedo RGBA from IMAGE_SAMPLE
//   v20      = lambert dot accumulator
//   v21      = lit factor = 0.3 + 0.7*max(0, dot)
//   v40-v43  = output RGBA = albedo * lit * light_color (alpha forced to 1)
static const uint32_t ps_shader_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xBEFC0302, 0xBEA0047E /* s_mov_b64 s[32:33], exec: live mask */, 0xBEFE0A7E /* s_wqm_b64 exec, exec */ /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */, 0xC0C80100, 0xC0860108,
    0xC08C010C, 0xC08E0120, 0xBF8C007F, 0xC80C0000,
    0xC80D0001, 0xC8100100, 0xC8110101, 0xC8280200,
    0xC8290201, 0xC82C0300, 0xC82D0301, 0xC8300700,
    0xC8310701, 0xF0800F00, 0x00641003, 0xBF8C0F70,
    0x10281818, 0x102A1419, 0x06282B14, 0x102A161A,
    0x06282B14, 0x20282880, 0x102A28FF, 0x3F333333,
    0x062A2AFF, 0x3E99999A, 0x10502B10, 0x10522B11,
    0x10542B12, 0x1050501C, 0x1052521D, 0x1054541E,
    0x7E5602F2, 0xBEFE0420 /* s_mov_b64 exec, s[32:33]: exact mode for the export */, 0xF800180F /* exp: vm=1 */, 0x2B2A2928, 0xBF810000,
    /* OrbShdr footer: 40 dwords = 160 bytes = 0xA0 */
    0x5362724F, 0x00726468,
    0x0000B000, 0x00000000, 0xDEADBEEF,
    0xCAFE0114, 0x00000000,
};

// Kept for diagnostic swap-back if needed: wpos-UV shadow sample
static const uint32_t ps_shader_binary_WPOS_UV[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xC0C80128, 0xC0860108,
    0xBF8C007F, 0xC8600400, 0xC8610401, 0xC8640500,
    0xC8650501, 0x062830F0, 0x062A32F0, 0xF0800F00,
    0x00642814, 0xBF8C0F70, 0x7E5602F2, 0xF800080F,
    0x2B2A2928, 0xBF810000, 0xBF800000, 0x5362724F,
    0x00726468, 0x00004800, 0x00000000, 0xDEADBEEF,
    0xCAFE00FA, 0x00000000,
};


// === ps_sky: physical sky gradient + sun disc ===
// attr0.x = clip_x, attr0.y = clip_y (screen-space)
// Sky PS: dynamic day/sunset/night gradient + warm sun disc
// Loads:
//   s[4:7]   sun at desc[16]: x (aspect-scaled NDC), y (NDC), z = disc radius²
//   s[8:11]  zenith RGB at desc[24]
//   s[12:15] horizon RGB at desc[28]
//   s[16:19] light_color RGB at desc[32]
// Pipeline:
//   t = 0.5*(1 - clip_y)    // gradient param: 0=top, 1=bottom
//   sky.rgb = lerp(zenith, horizon, t)
//   d² = (clip_x - sun_x)² + (clip_y - sun_y)²   (clip_x/sun_x pre-scaled by W/H)
//   sun_factor = clamp(1 - d²/radius², 0, 1)²      (decoded: dwords 35-45)
//   output.rgb = lerp(sky, light_color, sun_factor)
// Hash CAFE00E2 (camera-locked but with working sun disc).
static const uint32_t ps_dark_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x0000001C, 0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */, 0xC0820110, 0xC0840118,
    0xC086011C, 0xC0880120, 0xBF8C007F, 0xC8080000,
    0xC8090001, 0xC80C0100, 0xC80D0101, 0x7E140208,
    0x7E160209, 0x7E18020A, 0x7E1A020C, 0x7E1C020D,
    0x7E1E020E, 0x7E200210, 0x7E220211, 0x7E240212,
    0x7E260204, 0x7E280205, 0x7E2A0206, 0x082C06F2,
    0x102C2CF0, 0x082E150D, 0x102E2D17, 0x062E1517,
    0x0830170E, 0x10302D18, 0x06301718, 0x0832190F,
    0x10322D19, 0x06321919, 0x08342702, 0x08362903,
    0x1034351A, 0x1036371B, 0x0634371A, 0x7E385515,
    0x1034391A, 0x083434F2, 0x20343480, 0x1E3434F2,
    0x1034351A, 0x08362F10, 0x1036351B, 0x062E2F1B,
    0x08363111, 0x1036351B, 0x0630311B, 0x08363312,
    0x1036351B, 0x0632331B, 0x7E3402F2, 0xF800080F,
    0x1A191817, 0xBF810000, 0x5362724F, 0x00726468,
    0x0000EC00, 0x00000000, 0xDEADBEEF, 0xCAFE00E2,
    0x00000000,
};





// Shadow-pass clear PS: outputs (1, 0, 0, 1) — solid R=1.0 ("nothing occluding").
// Drawn as a fullscreen quad BEFORE the cube in the shadow pass to fill the
// entire shadow map with a "no occluder" depth value.
//
// Encoding decision: inline F_ONE (0xF2) is fine. Both inline F_ONE and literal
// 1.0 (0x3F800000) produce identical results in shadPS4 — verified empirically.
// Original encoding restored. The 1.0→0.5 bug was elsewhere (CB_COLOR_INFO
// SNORM/UNORM mismatch — see shadow CB setup).
//
// Hash CAFE0108 to ensure shadPS4 picks up the fresh shader after CB fix.
static const uint32_t ps_shadow_clear_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009,                          /* prologue */
    0x7E5002F2,                                       /* v40 = 1.0 (R) */
    0x7E520280,                                       /* v41 = 0 (G) */
    0x7E540280,                                       /* v42 = 0 (B) */
    0x7E5602F2,                                       /* v43 = 1.0 (A) */
    0xF800080F, 0x2B2A2928,                           /* EXP MRT0 v[40:43] done vm */
    0xBF810000,                                       /* s_endpgm */
    /* OrbShdr footer: 9 dwords = 36 bytes = 0x24 */
    0x5362724F, 0x00726468,
    0x00002400, 0x00000000, 0xDEADBEEF,
    0xCAFE0108, 0x00000000,                           /* fresh hash */
};

// Shadow-pass PS: exports (NDC.z, 0, 0, 1) to MRT0.r.
// Loads light_MVP from desc[48..63] into s[32:47], interpolates attr1=world_pos,
// computes clip.z and clip.w as row·world_pos dot products, perspective-divides,
// saturates NDC.z to [0,1] using V_{MAX,MIN}_LEGACY_F32 (shadPS4-safe), and
// writes to MRT0. RGBA8 output quantizes R to 8-bit (sufficient precision).
// PGM_RSRC1: 44 VGPRs, 48 SGPRs → (5<<6)|10 = 0x14A.
// Hash CAFE00FB — fresh hash to force a pipeline-cache miss.
static const uint32_t ps_shadow_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */,                          /* prefix */
    0xC1100130,                                       /* s_load_dwordx16 s32, s[0:1], 0x30 — load light_MVP */
    0xBF8C007F,                                       /* s_waitcnt lgkmcnt(0) */
    /* Interp attr1 = world_pos.xyzw → v10,v11,v12,v13 */
    0xC8280400, 0xC8290401, 0xC82C0500, 0xC82D0501,
    0xC8300600, 0xC8310601, 0xC8340700, 0xC8350701,
    /* clip.z = s40*v10 + s41*v11 + s42*v12 + s43*v13 → v16 */
    0x10201428, 0x10281629, 0x06202910, 0x1028182A,
    0x06202910, 0x10281A2B, 0x06202910,
    /* clip.w = s44*v10 + s45*v11 + s46*v12 + s47*v13 → v17 */
    0x1022142C, 0x1028162D, 0x06222911, 0x1028182E,
    0x06222911, 0x10281A2F, 0x06222911,
    /* v18 = 1/v17; v16 = v16 * v18 = NDC.z */
    0x7E245511, 0x10202510,
    /* Saturate NDC.z to [0,1] using LEGACY opcodes */
    0x1C202080, 0x1A2020F2,
    /* Output: (NDC.z, 0, 0, 1) → v40,v41,v42,v43 */
    0x7E500310, 0x7E520280, 0x7E540280, 0x7E5602F2,
    /* EXP MRT0 done=1 vm=1; s_endpgm */
    0xF800080F, 0x2B2A2928, 0xBF810000,
    /* OrbShdr footer: 37 dwords = 148 bytes = 0x94 */
    0x5362724F, 0x00726468,
    0x00009800, 0x00000000, 0xDEADBEEF,
    0xCAFE00FB, 0x00000000,
};

// === Hash CAFE0119: Floor PS with normal-map sampling ===
//
// Adds tangent-space normal map sampling (desc[72..79]) on top of v0118's
// normal-vector Lambert. Per-fragment surface detail from the bump map
// while still respecting the curved surface's vertex normals.
//
// Pipeline:
//   1. Loads: sampler, albedo, normal_map (NEW), shadow_T#, light_MVP, sun, PCF, light_color
//   2. Interps: uv, vertex_normal, world_pos
//   3. Sample albedo → v[16:19]
//   4. Sample normal map → v[60:63]; decode: tn = sample*2 - 1
//   5. Build T = normalize(cross(world_up, vertex_N))
//   6. Build B = cross(vertex_N, T)
//   7. Perturbed N = normalize(T*tn.x + B*tn.y + vertex_N*tn.z)
//   8. lit = 0.3 + 0.7 * max(0, dot(perturbed_N, sun))
//   9. Light_MVP shadow projection (unchanged)
//  10. Output = albedo * lit * shadow_factor * light_color
//
// PGM_RSRC1 = 0x28D (88 SGPRs, 56 VGPRs).
static const uint32_t ps_floor_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000009, 0xBEFC0302, 0xBED8047E /* s_mov_b64 s[88:89], exec: live mask */, 0xBEFE0A7E /* s_wqm_b64 exec, exec */ /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */, 0xC0860108, 0xC0C80140,
    0xC0CC0148, 0xC0D40128, 0xC1180130, 0xC0A0010C,
    0xC0A20150, 0xC0AA0120, 0xBF8C007F, 0xC8280000,
    0xC8290001, 0xC82C0100, 0xC82D0101, 0xC8640200,
    0xC8650201, 0xC8680300, 0xC8690301, 0xC8300400,
    0xC8310401, 0xC8340500, 0xC8350501, 0xC8380600,
    0xC8390601, 0xC8600700, 0xC8610701, 0x7E1E02F2,
    0xF0800F00, 0x0064100A, 0xF0800F00, 0x00663C0A,
    0xBF8C0070, 0x107878F4, 0x067878F3, 0x107A7AF4,
    0x067A7AF3, 0x107C7CF4, 0x067C7CF3, 0x7E40031A,
    0x7E420280, 0x08443080, 0x10464120, 0x3E464522,
    0x7E465D23, 0x10404720, 0x10444722, 0x10484519,
    0x104A411A, 0x104C4518, 0x084A4D25, 0x104C4119,
    0x084C4C80, 0x10507920, 0x3E507B24, 0x3E507D18,
    0x10527B25, 0x3E527D19, 0x10547922, 0x3E547B26,
    0x3E547D1A, 0x10565128, 0x3E565329, 0x3E56552A,
    0x7E565D2B, 0x10505728, 0x10525729, 0x1054572A,
    0x10365040, 0x3E365241, 0x3E365442, 0x203C3680,
    0x103C3CFF, 0x3F333333, 0x063C3CFF, 0x3E99999A,
    0x10581830, 0x3E581A31, 0x3E581C32, 0x06585833,
    0x105A1834, 0x3E5A1A35, 0x3E5A1C36, 0x065A5A37,
    0x105C1838, 0x3E5C1A39, 0x3E5C1C3A, 0x065C5C3B,
    0x105E183C, 0x3E5E1A3D, 0x3E5E1C3E, 0x065E5E3F,
    0x7E60552F, 0x1058612C, 0x105A612D, 0x105C612E,
    0x104258F0, 0x064242F0, 0x10445AF0, 0x064444F0,
    0x1C405C80, 0x1A4040F2, 0xF0800100, 0x022A2321,
    0xBF8C0F70, 0x7C024123, 0x7E480280, 0x004648F2,
    0x7C025E80, 0x004646F2, 0x104A46F0, 0x064A4AF0,
    0x103C3D25, 0x1064211E, 0x1066231E, 0x1068251E,
    0x10646454, 0x10666655, 0x10686856, 0x7E6A02F2,
    0xBEFE0458 /* s_mov_b64 exec, s[88:89]: exact mode for the export */, 0xF800180F /* exp: vm=1 */, 0x35343332, 0xBF810000, 0x5362724F,
    0x00726468, 0x0001FC00, 0x00000000, 0xDEADBEEF,
    0xCAFE0119, 0x00000000,
};

// White PS for loading bar — solid (1, 1, 1, 1) output, no texture sampling
static const uint32_t ps_blue_binary[] __attribute__((aligned(256))) = {
    0xBEEB03FF, 0x00000006, 0xBEFC0302 /* s_mov_b32 m0, s2: PRIM_MASK for v_interp */, 0xC8080000, 0xC8090001,
    0xC80C0100, 0xC80D0101, 0x7E2802F2, 0x7E2A02F2,
    0x7E2C02F2, 0x7E2E02F2, 0xF800080F, 0x17161514,
    0xBF810000, 0xBF800000, 0x5362724F, 0x00726468,
    0x00003C00, 0x00000000, 0xDEADBEEF, 0xCAFE0003,
    0x00000000,
};

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
static const char *asset_path(int slot, const char *name) {
    char *d = g_path_buf[slot & 1]; int i = 0;
    const char *p = (slot & 2) ? DATA_DIR_OLD : DATA_DIR_NEW;
    while (*p && i < 200) d[i++] = *p++;
    while (*name && i < 250) d[i++] = *name++;
    d[i] = 0;
    return d;
}
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
static void *gpu_alloc_typed(unsigned long size, unsigned long align, int memtype) {
    unsigned long phys = 0; void *addr = 0;   /* matches sceKernelAllocateDirectMemory's unsigned long* out-param */
    size = (size + 0x3FFF) & ~0x3FFFUL;
    if (align < 0x4000) align = 0x4000;
    if (sceKernelAllocateDirectMemory(0, 0x600000000ULL, size, align, memtype, &phys)) return 0;
    if (sceKernelMapDirectMemory(&addr, size, PROT_CPU_RW | PROT_GPU_RW, 0, phys, align)) return 0;
    my_memset(addr, 0, size);
    return addr;
}

static void *gpu_alloc(unsigned long size, unsigned long align) {
    return gpu_alloc_typed(size, align, MEM_TYPE_GARLIC);
}
/* Cached memory for data the CPU reads back (GARLIC is write-combined: CPU reads
   are uncached and very slow). */
static void *cpu_alloc(unsigned long size, unsigned long align) {
    return gpu_alloc_typed(size, align, MEM_TYPE_ONION);
}

static float my_sin(float x) {
    const float PI = 3.14159265358979f, TWO_PI = 6.28318530717959f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    float x2 = x * x;
    return x*(1.0f-x2/6.0f*(1.0f-x2/20.0f*(1.0f-x2/42.0f*(1.0f-x2/72.0f))));
}
static float my_cos(float x) { return my_sin(x + 1.57079632679490f); }

static float my_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    /* Newton-Raphson iteration for sqrt */
    float g = x * 0.5f;
    for (int i = 0; i < 6; i++) g = 0.5f * (g + x / g);
    return g;
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

// Build vertex buffer per frame (CPU-side transform)
// disp_tex/disp_w/disp_h: optional displacement texture (RGBA8, R = height [0,1]).
//           NULL → no displacement (curve-only floor).
//           Caller is responsible for stretching the texture's R range to
//           [0, 255] before calling — keeps this function simple.
static void build_static_vb(float *vb,
                            const unsigned char *disp_tex, int disp_w, int disp_h) {
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

    /* Floor plane: 32×32 grid of quads, ±200 world units in XZ.
       Each vertex gets:
         y = Y_BASE - (x²+z²)/(2R)         curve toward horizon
           + (disp.r - 0.5) * DISP_SCALE   per-vertex displacement
       Normals computed via finite differences of the displaced height field
       so lighting matches the actual surface (not just the curve).
       UV tiled 100× across full floor. */
    {
        float *fp = (float*)((char*)vb + FLOOR_DATA_OFF);
        const int GRID = 64;
        const float HALF = 200.0f;
        const float UV_MAX = 100.0f;
        const float Y_BASE = -0.5f;
        const float R = 20000.0f;
        const float DISP_SCALE = 1.5f;       /* ±0.75 world units of relief — clearly visible */
        const float DISP_TILES = 32.0f;      /* 32 tiles across floor = 12.5 units per tile;
                                                with 64×64 mesh = 2 verts per tile width */
        const float STEP = (2.0f * HALF) / (float)GRID;
        const float UV_STEP = UV_MAX / (float)GRID;

        /* Inline helper: y(x, z) = curve + (sample - 0.5) * DISP_SCALE.
           Displacement texture TILES UV_MAX times across the floor (same
           tile rate as albedo) so each tile in the BMP becomes a real-world
           tile on the floor. With UV_MAX=100 and 400-unit floor, each BMP
           tile covers 4 world units. Texture R values are auto-stretched
           to [0, 255] on load. */
        #define COMPUTE_Y(out_y, xx, zz) do {                                  \
            float _disp = 0.0f;                                                \
            if (disp_tex && disp_w > 0 && disp_h > 0) {                        \
                float _u = ((xx) + HALF) / (2.0f * HALF) * DISP_TILES;         \
                float _v = ((zz) + HALF) / (2.0f * HALF) * DISP_TILES;         \
                /* Wrap to [0, 1) — tile the texture */                        \
                float _uf = _u - (float)((int)_u);                             \
                float _vf = _v - (float)((int)_v);                             \
                if (_uf < 0) _uf += 1.0f;                                      \
                if (_vf < 0) _vf += 1.0f;                                      \
                int _tx = (int)(_uf * (float)disp_w);                          \
                int _tz = (int)(_vf * (float)disp_h);                          \
                if (_tx < 0) _tx = 0; if (_tx >= disp_w) _tx = disp_w - 1;     \
                if (_tz < 0) _tz = 0; if (_tz >= disp_h) _tz = disp_h - 1;    \
                int _idx = (_tz * disp_w + _tx) * 4;                           \
                _disp = ((float)disp_tex[_idx] / 255.0f - 0.5f) * DISP_SCALE;  \
            }                                                                  \
            (out_y) = Y_BASE - ((xx)*(xx) + (zz)*(zz)) / (2.0f * R) + _disp;   \
        } while (0)

        int v = 0;
        for (int gz = 0; gz < GRID; gz++) {
            for (int gx = 0; gx < GRID; gx++) {
                float x0 = -HALF + (float)gx * STEP;
                float x1 = x0 + STEP;
                float z0 = -HALF + (float)gz * STEP;
                float z1 = z0 + STEP;
                float u0 = (float)gx * UV_STEP;
                float u1 = u0 + UV_STEP;
                float vt0 = (float)gz * UV_STEP;
                float vt1 = vt0 + UV_STEP;

                /* Compute Y at each corner */
                float yA, yB, yC, yD;
                COMPUTE_Y(yA, x0, z0);
                COMPUTE_Y(yB, x1, z0);
                COMPUTE_Y(yC, x1, z1);
                COMPUTE_Y(yD, x0, z1);

                /* Per-vertex normals via finite differences. Sample y at small
                   offsets to estimate ∂y/∂x and ∂y/∂z. Normal = (-∂y/∂x, 1, -∂y/∂z)
                   normalized. Epsilon = STEP/4 for smooth gradient. */
                float eps = STEP * 0.25f;
                #define COMPUTE_N(NX, NY, NZ, xx, zz) do {                     \
                    float _yp_dx, _yn_dx, _yp_dz, _yn_dz;                      \
                    COMPUTE_Y(_yp_dx, (xx) + eps, (zz));                       \
                    COMPUTE_Y(_yn_dx, (xx) - eps, (zz));                       \
                    COMPUTE_Y(_yp_dz, (xx),       (zz) + eps);                 \
                    COMPUTE_Y(_yn_dz, (xx),       (zz) - eps);                 \
                    float _dydx = (_yp_dx - _yn_dx) / (2.0f * eps);            \
                    float _dydz = (_yp_dz - _yn_dz) / (2.0f * eps);            \
                    float _nx = -_dydx;                                        \
                    float _ny = 1.0f;                                          \
                    float _nz = -_dydz;                                        \
                    float _len = my_sqrt(_nx*_nx + _ny*_ny + _nz*_nz);         \
                    if (_len < 0.001f) _len = 1.0f;                            \
                    (NX) = _nx / _len;                                         \
                    (NY) = _ny / _len;                                         \
                    (NZ) = _nz / _len;                                         \
                } while (0)

                float NA[3], NB[3], NC[3], ND[3];
                COMPUTE_N(NA[0], NA[1], NA[2], x0, z0);
                COMPUTE_N(NB[0], NB[1], NB[2], x1, z0);
                COMPUTE_N(NC[0], NC[1], NC[2], x1, z1);
                COMPUTE_N(ND[0], ND[1], ND[2], x0, z1);

                float verts[6][8] = {
                    /* tri 1: A, C, B — pos.xyz, uv.xy, normal.xyz */
                    {x0, yA, z0,  u0, vt0,   NA[0], NA[1], NA[2]},
                    {x1, yC, z1,  u1, vt1,   NC[0], NC[1], NC[2]},
                    {x1, yB, z0,  u1, vt0,   NB[0], NB[1], NB[2]},
                    /* tri 2: A, D, C */
                    {x0, yA, z0,  u0, vt0,   NA[0], NA[1], NA[2]},
                    {x0, yD, z1,  u0, vt1,   ND[0], ND[1], ND[2]},
                    {x1, yC, z1,  u1, vt1,   NC[0], NC[1], NC[2]},
                };
                for (int i = 0; i < 6; i++) {
                    int idx = v * 12;
                    fp[idx+0] = verts[i][0];
                    fp[idx+1] = verts[i][1];
                    fp[idx+2] = verts[i][2];
                    fp[idx+3] = 1.0f;
                    fp[idx+4] = verts[i][5];
                    fp[idx+5] = verts[i][6];
                    fp[idx+6] = verts[i][7];
                    fp[idx+7] = 0.0f;
                    fp[idx+8] = verts[i][3];
                    fp[idx+9] = verts[i][4];
                    fp[idx+10] = 0.0f; fp[idx+11] = 0.0f;
                    v++;
                }

                #undef COMPUTE_N
            }
        }

        #undef COMPUTE_Y
    }
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
static void build_vsharp(uint32_t *v, void *base, uint32_t size) {
    build_vsharp_strided(v, base, size, 0);
}
/* levels > 1: a mip chain laid out by bmp_load (LINEAR_ALIGNED, see
   bmp_loader.h). SQ_IMG_RSRC_WORD3 (gfx_7_2_sh_mask.h): LAST_LEVEL @16,
   POW2_PAD @25 - radeonsi sets POW2_PAD(last_level > 0) on GFX6-8. */
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

    // PS: user_sgpr=2. New projective-shadow PS uses up to v43 (need 44 VGPRs
    // → field = ceil(44/4)-1 = 10) and up to s47 (need 48 SGPRs → field = ceil(48/8)-1 = 5).
    // PGM_RSRC1 = (sgpr_field<<6) | vgpr_field = (5<<6)|10 = 0x14A.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(5u<<6)|10u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

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

    // Color — render directly to display FB (BGRA, sRGB).
    { uint32_t c=(uint32_t)((uint64_t)(uintptr_t)color>>8);
      uint32_t r[14]={c,(DISPLAY_W/8)-1,(DISPLAY_W*DISPLAY_H/64)-1,0,
        0x09A8u,0,0,0,0,0,0,0,0,0};
      pm4_set_context_regs(b,CTX_CB_COLOR0_BASE,r,14);
      pm4_emit(b,0xC0001000u); pm4_emit(b,DISPLAY_W|(DISPLAY_H<<16)); }

    pm4_set_context_reg(b,CTX_COLOR_TARGET_MASK,0xF);
    pm4_set_context_reg(b,CTX_COLOR_SHADER_MASK,0xF);
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0,0);           /* attr0: VS param 0 -> PS slot 0 */
    pm4_set_context_reg(b,CTX_PS_INPUT_CNTL_0+1,1);         /* attr1: VS param 1 -> PS slot 1 */
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,1);          /* 2 param exports (export_count_min_one=1) */
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
    { uint64_t a=(uint64_t)(uintptr_t)ps_bg;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(3u<<6)|10u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4); /* ps_dark: 43 VGPRs, s0-s28 + VCC -> 44 / 32 */
      /* Sky PS needs desc ptr for sun position */
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }
    // VS s[0:3] = vertex/MVP V#. Sun read via s_buffer_load from V#+0x40
    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,bg_v,4);
    pm4_draw_index_auto(b,BG_VERTS);
    GPU_TS(2);


    /* Sky draw performed the one-shot depth clear. Disable clear flag so subsequent
       draws (floor, cube) render normally against the now-cleared depth buffer. */
    pm4_set_context_reg(b,CTX_DEPTH_RENDER_CONTROL,0);

    // === Floor draw: between sky and cube ===
    // Depth test Less so cube draws on top, but floor is drawn first so cube occludes it.
    // Floor uses its own V# (floor_v) pointing at vb+FLOOR_MVP_OFF where MVP is mirrored
    // and floor verts are at V#+80.
    if (ps_floor && floor_v) {
        /* Floor PS CAFE0119: uses v0-v63, s0-s87 + VCC -> 64 VGPRs, 96 SGPRs (0x2CF). */
        uint64_t a=(uint64_t)(uintptr_t)ps_floor;
        uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),0x2CFu,(2u<<1)};
        pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
        uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                        (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
        pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2);

        /* Depth for floor: less-than, write enabled (so cube z-tests correctly against floor) */
        pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
        pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back */

        pm4_set_sh_regs(b,SH_VS_USER_DATA_0,floor_v,4);
        pm4_draw_index_auto(b, FLOOR_VERTS);
        GPU_TS(3);
    }


    // Switch back to textured PS for model. Same RSRC1 as initial: 44 VGPRs, 48 SGPRs.
    { uint64_t a=(uint64_t)(uintptr_t)ps;
      uint32_t r[4]={(uint32_t)(a>>8),(uint32_t)(a>>40),(5u<<6)|10u,(2u<<1)};
      pm4_set_sh_regs(b,SH_PS_PGM_LO,r,4);
      uint32_t ud[2]={(uint32_t)((uint64_t)(uintptr_t)desc),
                      (uint32_t)((uint64_t)(uintptr_t)desc>>32)};
      pm4_set_sh_regs(b,SH_PS_USER_DATA_0,ud,2); }

    // Switch to depth=Less for cube
    pm4_set_context_reg(b,CTX_DEPTH_CONTROL,(1u<<1)|(1u<<2)|(1u<<4));
    pm4_set_context_reg(b,CTX_POLYGON_CONTROL,(1<<1)); /* cull back, CW front */

    // Draw 2: Model with real MVP. Sun at V#+0x40 read via s_buffer_load
    { uint32_t cube_v[4];
      build_vsharp(cube_v,(char*)vb_base+MVP_OFF,(uint32_t)(vb_total > MVP_OFF ? vb_total - MVP_OFF : VERT_BUF_SIZE - MVP_OFF));
      pm4_set_sh_regs(b,SH_VS_USER_DATA_0,cube_v,4); }
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1); /* uint32 indices */
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
    }
    GPU_TS(4);

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
    pm4_set_context_reg(b,CTX_VS_OUTPUT_CONFIG,1);   /* 2 param exports */
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

    pm4_set_sh_regs(b,SH_VS_USER_DATA_0,shadow_vb_v,4);
    if (is_indexed && ib_ptr && num_indices > 0) {
        pm4_index_type(b, 1);
        pm4_draw_index_2(b, (uint32_t)num_indices,
                         (uint64_t)(uintptr_t)ib_ptr, (uint32_t)num_indices);
    } else {
        pm4_draw_index_auto(b, model_verts);
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

static void loading_progress(int pass, const char *msg, void *ud) {
    struct LoadCtx *c = (struct LoadCtx*)ud;
    float progress = (float)pass / 3.0f; /* 0.0 → 1.0 */

    /* Loading sky: SOLID blue (both zenith and horizon = blue) to cover behind rendering */
    {
        float *sz = (float*)(c->desc + 24); /* zenith */
        sz[0]=0.08f; sz[1]=0.20f; sz[2]=0.55f; sz[3]=0;
        float *sh = (float*)(c->desc + 28); /* horizon */
        sh[0]=0.08f; sh[1]=0.20f; sh[2]=0.55f; sh[3]=0;
        /* Sun NDC off-screen (no disc on loading screen) */
        float *snd = (float*)(c->desc + 16);
        snd[0]=10.0f; snd[1]=10.0f; snd[2]=0.0001f; snd[3]=0;
        /* Sun color black so no bleed if anything samples it */
        float *sc = (float*)(c->desc + 20);
        sc[0]=0; sc[1]=0; sc[2]=0; sc[3]=0;
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
                            bi ? c->fb1 : c->fb0, c->depth, 0, c->fence, fv, 0);
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

    /* Try loading BMP texture, fallback to logo */
    void *tex = 0; int tex_w = LOGO_WIDTH, tex_h = LOGO_HEIGHT;
    {
        BmpTexture bmp;
        const char *bmp_paths[] = { DATA_DIR_NEW "texture.bmp", DATA_DIR_NEW "model.bmp",
                                    DATA_DIR_OLD "texture.bmp", DATA_DIR_OLD "model.bmp", 0 };
        for (int bi = 0; bmp_paths[bi]; bi++) {
            if (bmp_load(bmp_paths[bi], gpu_alloc, &bmp, 1) == 0) {
                tex = bmp.pixels; tex_w = bmp.width; tex_h = bmp.height;
                break;
            }
        }
    }
    if (!tex) {
        /* No texture file on disk — fall back to the embedded shadPS4 logo
           (256×256 RGBA8 from logo_texture.h). Previous code created a 1×1
           white pixel here, which is why the cube rendered solid white when
           texture.bmp was missing. */
        tex = gpu_alloc(LOGO_SIZE, 0x1000);
        my_memcpy(tex, logo_rgba, LOGO_SIZE);
        tex_w = LOGO_WIDTH; tex_h = LOGO_HEIGHT;
    }

    /* Floor albedo texture. */
    void *floor_albedo_tex = 0;
    int floor_tex_w = 1, floor_tex_h = 1, floor_tex_levels = 1;
    {
        BmpTexture bmp;
        if ((bmp_load(asset_path(0,"floor_albedo.bmp"), gpu_alloc, &bmp, FLOOR_TEX_MIPS) == 0 ||
            bmp_load(asset_path(2,"floor_albedo.bmp"), gpu_alloc, &bmp, FLOOR_TEX_MIPS) == 0)) {
            floor_albedo_tex = bmp.pixels;
            floor_tex_w = bmp.width; floor_tex_h = bmp.height; floor_tex_levels = bmp.levels;
        }
    }
    if (!floor_albedo_tex) {
        /* Fallback to mid-grey 1×1 so floor still renders something visible. */
        floor_albedo_tex = gpu_alloc(4, 0x1000);
        unsigned char *p = (unsigned char*)floor_albedo_tex;
        p[0]=128; p[1]=128; p[2]=128; p[3]=255;
        floor_tex_w = 1; floor_tex_h = 1;
    }

    /* Floor normal map texture from BMP. Tangent-space normal map: each
       pixel encodes (n.x*0.5+0.5, n.y*0.5+0.5, n.z*0.5+0.5) in RGB.
       Floor PS samples this at desc[72..79] and uses it for per-fragment
       Lambert lighting.

       Falls back to a 1×1 "no-bump" texel encoding the unit Z normal
       (0.5, 0.5, 1.0 → byte (128, 128, 255)) so floor still renders if
       the BMP file is missing. */
    void *floor_normal_tex = 0;
    int floor_nrm_w = 1, floor_nrm_h = 1, floor_nrm_levels = 1;
    {
        BmpTexture bmp;
        if ((bmp_load(asset_path(0,"floor_normal.bmp"), gpu_alloc, &bmp, FLOOR_TEX_MIPS) == 0 ||
            bmp_load(asset_path(2,"floor_normal.bmp"), gpu_alloc, &bmp, FLOOR_TEX_MIPS) == 0)) {
            floor_normal_tex = bmp.pixels;
            floor_nrm_w = bmp.width; floor_nrm_h = bmp.height; floor_nrm_levels = bmp.levels;
        }
    }
    if (!floor_normal_tex) {
        floor_normal_tex = gpu_alloc(4, 0x1000);
        unsigned char *p = (unsigned char*)floor_normal_tex;
        p[0]=128; p[1]=128; p[2]=255; p[3]=255;   /* flat tangent normal */
        floor_nrm_w = 1; floor_nrm_h = 1;
    }

    /* Floor displacement texture (height map) — sampled per-vertex during
       floor mesh generation in build_static_vb. R channel is the height
       value [0,1]; final Y offset = (sample - 0.5) * DISP_SCALE so it
       displaces both up and down from the curve. NULL if missing → no
       displacement applied (curve-only floor). */
    void *floor_disp_tex = 0;
    int floor_disp_w = 0, floor_disp_h = 0;
    {
        BmpTexture bmp;
        if ((bmp_load(asset_path(0,"floor_displacement.bmp"), cpu_alloc, &bmp, 1) == 0 ||
            bmp_load(asset_path(2,"floor_displacement.bmp"), cpu_alloc, &bmp, 1) == 0)) {
            floor_disp_tex = bmp.pixels;
            floor_disp_w = bmp.width; floor_disp_h = bmp.height;
            /* Auto-stretch the displacement range to span [0,255]. Many
               displacement BMPs use a compressed value range (e.g. only
               100-130 out of 0-255), which produces imperceptible
               displacement. Scan ALL pixels for the actual R range, then
               remap each pixel in-place. After this, the texture has full
               dynamic range and build_static_vb sees the proper variation. */
            unsigned char *px = (unsigned char*)bmp.pixels;
            unsigned long total = (unsigned long)bmp.width * (unsigned long)bmp.height;
            int min_r = 255, max_r = 0;
            for (unsigned long i = 0; i < total; i++) {
                int r = px[i * 4];   /* RGBA8 after bmp_load conversion */
                if (r < min_r) min_r = r;
                if (r > max_r) max_r = r;
            }
            int range = max_r - min_r;
            if (range > 0 && range < 240) {
                /* Compressed range — stretch to full [0,255]. */
                for (unsigned long i = 0; i < total; i++) {
                    int v = ((int)px[i * 4] - min_r) * 255 / range;
                    if (v < 0) v = 0; else if (v > 255) v = 255;
                    px[i * 4] = (unsigned char)v;
                }
            }
        }
    }

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
    uint32_t *desc=(uint32_t*)gpu_alloc_typed(512,0x100,MEM_TYPE_ONION);
    if (!desc) FATAL_EXIT("descriptor alloc failed");
    g_gpu_ts = (volatile uint64_t *)gpu_alloc_typed(0x1000, 0x100, MEM_TYPE_ONION);
    build_tsharp(desc,tex,tex_w,tex_h);
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
      sd[0] = 0.5f; sd[1] = 0.5f; sd[2] = 0.006f; sd[3] = 1.0f; }

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

    /* Floor PBR T#s at desc[64..71] (albedo) and desc[72..79] (normal).
       BMP files are LINEAR pixel arrays. Use tile_mode=8 (DisplayLinearAligned)
       per shadPS4's TileMode enum in tiling.h. Earlier I used 13 (Thin1DThin, a
       TILED layout) which caused shadPS4 to de-tile nonexistent tiles — that's
       why the texture looked streaked/washed.
       build_tsharp already sets 8 so no patching needed. */
    build_tsharp_levels(desc + 64, floor_albedo_tex, floor_tex_w, floor_tex_h, floor_tex_levels);
    /* Floor normal map at desc[72..79] — procedural 64x64 tangent-space normals */
    build_tsharp_levels(desc + 72, floor_normal_tex, floor_nrm_w, floor_nrm_h, floor_nrm_levels);

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
    build_static_vb((float*)vb,
                    (const unsigned char*)floor_disp_tex,
                    floor_disp_w, floor_disp_h);  // uploaded ONCE
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
    UPLOAD_SHADER(ps_shadow_gpu,        ps_shadow_binary);
    UPLOAD_SHADER(ps_shadow_clear_gpu,  ps_shadow_clear_binary);
    UPLOAD_SHADER(ps_blue_gpu,          ps_blue_binary);
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

    /* --- Load 3D model using obj_loader.h --- */
    {
        static const char *obj_paths[] = {
            DATA_DIR_NEW "model.obj",   DATA_DIR_OLD "model.obj",
            DATA_DIR_NEW "mesh.obj",    DATA_DIR_OLD "mesh.obj",
            DATA_DIR_NEW "object.obj",  DATA_DIR_OLD "object.obj",
            DATA_DIR_NEW "scene.obj",   DATA_DIR_OLD "scene.obj",
            DATA_DIR_NEW "bugatti.obj", DATA_DIR_OLD "bugatti.obj",
            DATA_DIR_NEW "car.obj",     DATA_DIR_OLD "car.obj",
            0
        };
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
                    float dx = maxx - minx, dy = maxy - miny, dz = maxz - minz;
                    float diag = my_sqrt(dx*dx + dy*dy + dz*dz);
                    g_model_radius = diag * 0.5f;
                    if (g_model_radius < 0.1f) g_model_radius = 0.1f;
                }
                loaded = 1;
            }
        }
        if (!loaded) {
            /* Restore cube verts and BG — loading_progress may have overwritten them
               during failed OBJ attempts (progress-bar geometry written at CUBE_DATA_OFF
               plus BG tweaks). Rebuild the static VB to recover the original cube. */
            build_static_vb((float*)vb,
                            (const unsigned char*)floor_disp_tex,
                            floor_disp_w, floor_disp_h);
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
    int pad_ok = 0;          /* only trust pad.buttons after a successful read */
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
    float vel_y = 0;           /* vertical velocity (gravity/jump) */
    float ground_y = -0.05f;   /* ground level */
    float eye_height = 0.15f;  /* camera height above ground */
    int on_ground = 1;
    int sprint = 0;
    float gravity = -0.004f;
    float jump_vel = 0.06f;
    int auto_spin = 0;  // 1=spinning camera (no longer Start-toggleable; reserved)
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
    int quit_reason = 0;   /* 0=still running, 1=system quit event, 2=pad combo */
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
    { char T[128]; int p = 0;
      #define TP(x) do { const char *_q = (x); while (*_q) T[p++] = *_q++; } while (0)
      TP("floor tex alb="); p += lg_i64(T + p, floor_tex_w); TP("x"); p += lg_i64(T + p, floor_tex_h);
      TP(" levels="); p += lg_i64(T + p, floor_tex_levels);
      TP(" nrm="); p += lg_i64(T + p, floor_nrm_w); TP("x"); p += lg_i64(T + p, floor_nrm_h);
      TP(" levels="); p += lg_i64(T + p, floor_nrm_levels); TP("\n");
      #undef TP
      trace_line(T, (unsigned long)p); }
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
            pad_ok = (scePadRead(pad_handle, &pad, 1) >= 0);
        uint64_t t_pad = tstamp();

        /* Manual quit fallback: hold all four triggers (L1+R1+L2+R2) together.
           Guarantees a clean exit + teardown regardless of the system-event
           path, so the app never hangs the OS on close. */
        if (pad_ok && (pad.buttons & 0x0F00) == 0x0F00) { running = 0; quit_reason = 2; }

        // Button edge detection (pressed this frame, not last)
        uint32_t pressed = pad.buttons & ~prev_buttons;
        prev_buttons = pad.buttons;

        // Cross: toggle auto-spin
        /* Cross = jump */
        if ((pressed & PAD_CROSS) && on_ground) {
            vel_y = jump_vel;
            on_ground = 0;
        }
        /* Square = sprint toggle */
        if (pressed & 0x8000) sprint = !sprint;
        /* Options/Start = toggle cube rotation pause/resume */
        if (pressed & PAD_OPTIONS) cube_rotation_enabled = !cube_rotation_enabled;
        // Triangle: reset camera to default sunset view
        if (pressed & PAD_TRI) { cam_yaw=-1.5708f; cam_pitch=0; cam_x=3.5f; cam_y=0.55f; cam_z=0.8f; vel_y=0; on_ground=1; sprint=0; }

        // L1/R1: zoom
        if (pad.buttons & PAD_R1) cam_y += move_speed * 60.0f * dt_sec;
        if (pad.buttons & PAD_L1) cam_y -= move_speed * 60.0f * dt_sec;

        // Left stick: orbit camera
        float lx = ((float)pad.lx - 128.0f) / 128.0f;
        float ly = ((float)pad.ly - 128.0f) / 128.0f;
        /* Left stick: move forward/back + strafe */
        /* All movement is per-SECOND now. move_speed was a per-frame step, so
           x60 keeps the original feel at 60fps while making it frame-rate
           independent. */
        float spd = (sprint ? move_speed * 3.0f : move_speed) * 60.0f * dt_sec;
        if (ly > 0.15f || ly < -0.15f) {
            cam_x += my_sin(cam_yaw) * (-ly) * spd;
            cam_z -= my_cos(cam_yaw) * (-ly) * spd;
        }
        if (lx > 0.15f || lx < -0.15f) {
            cam_x += my_cos(cam_yaw) * lx * spd;
            cam_z += my_sin(cam_yaw) * lx * spd;
        }

        // Right stick: pan camera
        float rx = ((float)pad.rx - 128.0f) / 128.0f;
        float ry = ((float)pad.ry - 128.0f) / 128.0f;
        /* Right stick: look around */
        if (rx > 0.15f || rx < -0.15f) cam_yaw   += rx * 2.4f * dt_sec;  /* was 0.04/frame */
        if (ry > 0.15f || ry < -0.15f) cam_pitch += ry * 1.8f * dt_sec;  /* was 0.03/frame */
        if (cam_pitch > 1.5f) cam_pitch = 1.5f;
        if (cam_pitch < -1.5f) cam_pitch = -1.5f;

        // D-pad: fine rotation
        /* D-pad: speed control + precise movement */
        if (pad.buttons & PAD_UP)    move_speed *= 1.02f;
        if (pad.buttons & PAD_DOWN)  move_speed *= 0.98f;
        if (move_speed < 0.001f) move_speed = 0.001f;
        if (move_speed > 0.5f) move_speed = 0.5f;
        if (pad.buttons & PAD_LEFT)  cam_yaw -= 1.2f * dt_sec;   /* was 0.02/frame */
        if (pad.buttons & PAD_RIGHT) cam_yaw += 1.2f * dt_sec;   /* was 0.02/frame */

        // Auto-spin
        if (auto_spin) cam_yaw += 1.2f * dt_sec;                 /* was 0.02/frame */

        /* Sun controls: L3 toggles auto-orbit, L2/R2 manual rotation */
        if (pressed & 0x0002) sun_speed = (sun_speed > 0.001f) ? 0.0f : 0.0027f; /* L3 toggle */
        if (pad.buttons & 0x0100) sun_angle -= 0.2592f * dt_sec; /* L2 held = sun left, 40% slower */
        if (pad.buttons & 0x0200) sun_angle += 0.2592f * dt_sec; /* R2 held = sun right */
        sun_angle += sun_speed * 60.0f * dt_sec;   /* sun_speed was per-frame */
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
            float *cb = (float*)((char*)vb + CUBE_DATA_OFF);
            for (int tri = 0; tri < 12; tri++) {
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
            if (shadow_vb && g_shadow_ready) {
                my_memcpy((char*)shadow_vb + 0x50,
                          (char*)vb + CUBE_DATA_OFF,
                          CUBE_VERTS * VERT_STRIDE);
            }

            #undef ROTATE_XY
        }

        build_mvp((float*)((char*)vb+MVP_OFF), cam_yaw, cam_pitch, cam_x, cam_y, cam_z);

        /* Mirror MVP to FLOOR_MVP_OFF so the floor draw (V#-base = vb+FLOOR_MVP_OFF)
           can read MVP at V#+0 and floor vertex data at V#+80, matching VS convention.
           Copy 64 bytes (16 floats = 4x4 matrix). */
        my_memcpy((char*)vb + FLOOR_MVP_OFF,
                  (char*)vb + MVP_OFF, 64);

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
             night                   → 0.60  (moon, 20% brighter than old 0.50)
             horizon (sun_y=0)       → 0.60  (matches night for seamless transition)
             rising 0 → sin(20°)=0.342 → linear ramp 0.60 → 1.20
             above 20° elevation     → 1.20  (peak day, +20% boost over old 1.00)

           Threshold lowered to 20° so the sun stays at full peak brightness
           for nearly all of the day. Darkening only kicks in during the
           final ~1-2 hours before sunset (or first 1-2 after sunrise). */
        float orig_sun_y = sun_y;
        int is_night = (sun_y < 0.0f);
        if (is_night) {
            sun_x = -sun_x;
            sun_y = -sun_y;
            sun_z = -sun_z;
            /* Night: 0.69 (was 0.60, +15% brighter as requested) */
            sun_x *= 0.69f; sun_y *= 0.69f; sun_z *= 0.69f;
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
            if (shadow_vb) {
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
            lc[0] = light_r; lc[1] = light_g; lc[2] = light_b; lc[3] = 1.0f;
        }

        /* Dynamic sky colors based on REAL sun elevation (orig_sun_y, before
           the moon swap). The sun_y variable was negated for the night-light
           swap above, so it's >=0 always now and would mis-classify night as
           day. Use orig_sun_y to keep the sky correctly dark at night even
           while the moon lights up the scene. */
        float zr, zg, zb, hr, hg, hb;
        if (orig_sun_y > 0.3f) {
            /* Full day */
            zr = 0.10f; zg = 0.25f; zb = 0.60f;
            hr = 0.65f; hg = 0.82f; hb = 0.95f;
        } else if (orig_sun_y > -0.2f) {
            /* Twilight/sunset: fade from day to night with orange horizon */
            float k = (orig_sun_y + 0.2f) / 0.5f;    /* 0 at night-edge, 1 at full-day edge */
            if (k < 0) k = 0; if (k > 1) k = 1;
            /* Zenith darkens */
            zr = 0.04f + 0.06f * k;
            zg = 0.06f + 0.19f * k;
            zb = 0.18f + 0.42f * k;
            /* Horizon warm orange near sunset */
            float warm = 1.0f - k;                   /* 1 at night-edge */
            hr = 0.65f + 0.30f * warm;               /* peaks orange */
            hg = 0.35f + 0.47f * (1.0f - warm*warm);
            hb = 0.25f + 0.70f * k;
        } else {
            /* Night sky: deep blue, slightly brighter at horizon */
            zr = 0.01f; zg = 0.02f; zb = 0.06f;
            hr = 0.03f; hg = 0.04f; hb = 0.10f;
        }
        {
            float *sz = (float*)(desc + 24);
            sz[0] = zr; sz[1] = zg; sz[2] = zb; sz[3] = 0;
            float *sh = (float*)(desc + 28);
            sh[0] = hr; sh[1] = hg; sh[2] = hb; sh[3] = 0;
        }

        /* Project sun to NDC for sky disc rendering → desc[16:19] */
        {
            float *mvp = (float*)((char*)vb + MVP_OFF);
            float *sun = (float*)(desc + 12);
            float sx = sun[0]*100, sy2 = sun[1]*100, sz = sun[2]*100;
            float cx = mvp[0]*sx + mvp[1]*sy2 + mvp[2]*sz + mvp[3];
            float cy = mvp[4]*sx + mvp[5]*sy2 + mvp[6]*sz + mvp[7];
            float cw = mvp[12]*sx + mvp[13]*sy2 + mvp[14]*sz + mvp[15];
            float *sd = (float*)(desc + 16);
            /* ps_dark: f = clamp(1 - d^2 / sd[2], 0, 1)^2 with d measured in
               aspect-corrected NDC (x * W/H, y) - so sd[2] is the disc edge
               radius SQUARED, in units of half the screen height. Moon keeps
               the previous moon/sun ratio of this value (0.75). */
            const float sun_r = SUN_DISC_RADIUS_PX / ((float)DISPLAY_H * 0.5f);
            float disc_radius = sun_r * sun_r * (is_night ? 0.75f : 1.0f);
            if (cw > 0.01f) {
                sd[0] = (cx / cw) * ((float)DISPLAY_W / (float)DISPLAY_H); sd[1] = cy / cw;
                sd[2] = disc_radius; sd[3] = 1.0f;
            } else {
                sd[0] = 99.0f; sd[1] = 99.0f;
                sd[2] = disc_radius; sd[3] = 0.0f;
            }
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
        uint64_t gts[5] = {0, 0, 0, 0, 0};
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
                for (int q = 0; q < 5; q++) { gts[q] = g_gpu_ts[q]; g_gpu_ts[q] = 0; }
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
            LP(" gtot="); p+=lg_i64(L+p,GTD(0,4));
            #undef GTD
            LP(" fv="); p+=lg_u64(L+p,(unsigned long long)fv);
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
