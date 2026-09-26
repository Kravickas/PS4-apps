============================================================
FLOOR: PARALLAX OCCLUSION MAPPING + DISTANCE FOG  (build=floor-pom+fog)
============================================================
Old floor relief was per-vertex: the grid is 6.25 units per vertex and the
height map tiles every 4 units, so the vertices only ever read 4 texels of
it - random bumps, not the texture's shape. That displacement is removed; the
mesh is the plain curve (bit-identical to the old mesh without a height map).
Relief now comes from parallax occlusion mapping in ps_floor.

ps_floor (new source shaders/ps_floor.s, hash CAFE0120, RSRC1 0x2D4):
- Tangent frame on the UV axes: T = +x projected on the surface (u), B = +z
  (v). The old frame, normalize(cross(up, N)), turned around the floor centre
  (normal-map detail rotated with position) and was NaN at the centre.
- Normal-map channel signs measured at load from the height map (correlation
  of red/green with the height slopes, 4096 points); printed in the trace.
- 16-layer POM on the height map (desc[92], sampler desc[100]), height
  normalised with the loader's R range, linear refinement between the last
  two layers, fades out by POM_FADE. Depth knob POM_DEPTH (world units).
- Fog: colour = the sky gradient at the same screen row (as ps_dark), weight
  1 - 2^(-FOG_EDGE_STOPS * (d / FLOOR_HALF)^2): clear near, 99% at the edge,
  so the floor edge melts into the sky.
- Lighting and shadow unchanged. Floor draw enables POS_Y (PS_INPUT 0x202),
  restored to 0x02 after it.
Height map: floor_displacement.bmp is now a GPU texture with mips (the CPU
copy and its in-place stretch are gone; the loader records the R range).
Fixed on the way: my_sqrt (6 Newton steps) was only right for ~0.1..1000;
now sqrtss (stars near the zenith are back on the sphere).
Checks (float32 emulation of the instruction streams):
- parallax + fog off and sign_y = -1: bit-identical to the old floor shader;
- parallax vs exact float64 ray-march: median UV error ~1e-5, 99% <= 4.7% of
  the offset (steep, oblique, grazing);
- fog: floor at the far edge within 0.0062 of the sky colour at that row;
- normal-sign estimator: 14/14 synthetic cases (smooth, tiles, bricks, all
  four sign combinations, different resolutions).
OpenOrbis build: 0 warnings, no import change, 13/13 shaders in the eboot
match; changed functions: main, bmp_load, build_static_vb, build_dcb,
my_sqrt; added normal_map_convention, build_ssharp_height; removed the now
unused cpu_alloc.
Trace line: "floor tex ... hgt=WxH levels=N range=lo..hi nrm_sign=x,y
corr_x1000=..".

============================================================
SUN AND MOON DISCS NONSTOP  (build=sun+moon-discs)
============================================================
Before: one disc = the active light (sun by day, anti-sun moon at night), so
when the sun's centre crossed the horizon the half-visible sun vanished and
the moon appeared half-risen (on the other side) - a pop.
Now ps_dark (new source shaders/ps_dark.s, hash CAFE00E4, RSRC1 0xC2) draws
both discs every frame at their true positions: sun desc[16..19] and moon
desc[20..23] = (x, y, radius^2), colours x HDR in desc[84..87] / [88..91].
The floor (drawn after the sky) hides whichever is below the horizon, so the
sun sinks out of view. Sun disc colour follows its own elevation (amber held
while it sets); moon disc stays cool blue.
Lighting and shadows unchanged: the light (desc[12], shadow light MVP) is
the body above the horizon, so a body below the horizon casts no shadow.
Checks: float32 emulation of old vs new shader bit-exact for day (sun) and
night (moon); CPU placement identical to the old disc at every angle by day
and by night; through sunset the sun disc moves continuously (max 0.056 px
per 2.5e-5 rad step) where the old one jumped to the moon. Loading screen and
startup keep both discs off-screen with radius^2 > 0 (0 * inf = NaN).
OpenOrbis build: 0 warnings, no import change, 13/13 shaders in the eboot
match; changed functions: main, loading_progress, build_dcb.

============================================================
STARS: BLEND + NO FLICKER, OUTPUT DITHER  (build=stars-blend+dither)
============================================================
Black stars during the fade: additive blending cannot darken, so the stars
were replacing the sky (colour x fade ~ 0 -> black). Blend registers matched
radeonsi exactly; the export format did not. radeonsi's table uses FP16_ABGR
as the (blend) export format for 16_16_16_16 FLOAT (and 8-bit) targets; we
exported 32_ABGR. The star draw now sets SPI_SHADER_COL_FORMAT = FP16_ABGR (4)
and ps_stars packs halves (v_cvt_pkrtz_f16_f32, compressed export, en 0xF,
done vm); restored to 32_ABGR (9) after the draw.

Flicker: hard 1.5-4 px quads cover 1..4 px depending on sub-pixel position
(4x brightness swing). Stars are now a smooth (1 - r^2)^2 splat over the quad
(corner uv and brightness via PARAM0 = uv.xy, normal.y), radius 2.0-3.0 px
(on screen 2.0-3.6): total light varies <= 2.1%. Brightness 0.35-1.0, mostly
faint; brighter = bigger. ps_stars: RSRC1 0x42, hash CAFE0201, source .s.

Banding: new ps_post_final (the up-add passes keep ps_post_comp) encodes sRGB
in the shader (exact curve, max error 1.6e-7) and adds +-0.5/255 interleaved
gradient noise before a UNORM display write (CB 0x88A8; the no-bloom fallback
keeps hardware sRGB). Dark night gradient: 17 flat bands (22 rows wide at
quarter height) -> per-row average within 0.008 of a step. RSRC1 0x105.

Checked: all 13 shaders fit their RSRC1, unique hashes; post chain passes
1-23 + all tables unchanged, last pass = ps_post_final; OpenOrbis v0.5.4
build: 0 warnings, no import/module change, 13/13 shader binaries in the
eboot match their headers; only main, build_stars, build_dcb, emit_post
changed.

============================================================
SHADERS MOVED TO shaders/  (build=hdr-bloom+fast-loader, no functional change)
============================================================
All 12 shader binaries moved from main.c into shaders/<name>.h (one per
shader, comments with them), included via shaders.h; Makefile adds -Ishaders.
main.c 4794 -> 4301 lines. Removed ps_shader_binary_WPOS_UV (unused leftover,
missed by the earlier cleanup) and a stale 'Projective shadow PS' comment.
Comment facts updated to this build (hashes, 0.0707/0.9293, shadow 0.2176,
RSRC1 0x18A / 0x2CF). Verified with clang-18: every shader section and all
70 functions (disassembly, symbolic relocations) identical before/after.

============================================================
HDR + BLOOM, sRGB OUTPUT, FAST LOADER  (build=hdr-bloom+fast-loader)
============================================================
Display buffer is A8R8G8B8Srgb (videoout 0x80000000). The colour buffer was
CB_COLOR0_INFO 0x09A8 = NUMBER_TYPE 1 (SNORM): 1.0 stored as 127 -> the whole
scene at half brightness.

Pipeline (linear light, 1.0 = display white):
  scene -> RGBA16F target (CB_INFO 0x7B0) -> 6-level bloom -> composite into
  the display buffer with NUMBER_SRGB + SWAP_ALT (0x8EA8), hardware sRGB encode.
  Bloom levels 480x270 .. 15x9 (pitch 512/256/128/64/64/64, T# tiling index 8
  needs 64-texel pitch multiples): bright-pass downsample (threshold 1.0),
  plain downsamples, blur H+V per level (9-tap binomial as 5 bilinear taps),
  up-add coarse -> fine, composite = scene + 0.25 * bloom. 24 passes, each
  starting with ACQUIRE_MEM(COHER_RT_TO_TEXTURE) like the shadow pass;
  2064 dwords per frame. One level alone spreads sigma ~8 px (the halo stayed
  inside the clipped sun - emulated); 6 levels: +0.15 at the disc edge, +0.08
  at 30 px, +0.04 at 70 px, +0.02 at 130 px.
  Shaders: shaders/post_{down,blur,comp}.s (llvm-mc -mcpu=bonaire), RSRC1
  0x89 / 0x8C / 0x103. MIMG fields decoded by hand (op 0x27 IMAGE_SAMPLE_LZ).
  Host emulation of the real tables + pass order: 4-tap == 4x4 box (7e-6),
  blur energy 0.9999998, halo centred, no change away from the sun.

Recalibration (linear):
  - colour textures (desc[0] cube/logo/model, desc[64] floor albedo) sampled
    with NUM_FORMAT SRGB; normal map and shadow map stay UNORM data
  - sky, light, star and loading-screen colours: srgb_to_linear (exact
    IEC 61966-2-1, max rel. error 5.6e-7)
  - floor + cube PS: ambient 0.3 -> 0.3^2.2 = 0.0707, diffuse 0.7 -> 0.9293;
    floor shadow factor 0.5 -> 0.5^2.2 = 0.2176 (lit part 0.7824)
  - sky PS: disc colour x desc[19] (3 v_mul_f32 after the light load): SUN_HDR
    4.0, MOON_HDR 2.0 - only the sun and moon exceed 1.0 and bloom
  Knobs: SUN_HDR, MOON_HDR, BLOOM_THRESHOLD, BLOOM_INTENSITY, EXPOSURE.
  Trace: gpo = post-processing GPU time, gtot now includes it.

Loader (why 2 GB loaded slowly: CPU-bound -O0 parsing, 4 full passes = 8 GB
of reads, reads and parsing serialized):
  - loaders.c (OBJ/STL/PLY) built with -O2 -fno-strict-aliasing -fno-builtin;
    main.o keeps its default codegen. loaders.o calls only libkernel.
  - newline scan 8 bytes/step; float parser exact (4.2M numbers == strtof);
    sqrt = sqrtss (the old 8-step Newton was ~2x off for small faces)
  - reader thread (scePthread*, names checked against NIDs) fills one 64 MB
    buffer while the other is parsed; falls back to synchronous reads
  - last line without '\n' is no longer dropped (the old loop lost the last face)
  Host: 2.67 s -> 1.04 s parse (104 MB); at 100 MB/s reads 4.50 s vs I/O 4.18 s.
  Output byte-identical threaded / synchronous / 4 KB chunks.
  Next: merge the normals pass into the emit pass (4 -> 3 file passes).
bmp_loader.h now includes what it uses (clang-format sorts includes).

============================================================
STARS, MODEL FIXES, LOADING SCREEN  (build=stars+model+loading)
============================================================
Model loaded -> cube still drawn: the model's VB puts vertices at
OBJ_DATA_OFF = 448 = CUBE_DATA_OFF, and the per-frame cube rotation kept
writing 36 cube verts there (+ into the model's shadow VB). Also the floor
MVP mirror went to vb + FLOOR_MVP_OFF (2176 = model verts 36-37) while the
floor's V# still points at the static VB, so the floor kept the load-time
camera. Fix: model_loaded skips the cube update; vb_static keeps the static
buffer and receives the floor mirror.

Loading screen:
  - frame ended with the flip MARKER but was sent with a plain
    sceGnmSubmitCommandBuffers (no patcher) -> fence never written, each
    update waited out its >1 s timeout. Now no_flip=1 (EOP fence), like the
    main loop's CPU flip.
  - first frame only after pass 1 had streamed the whole file; progress was
    pass/3. Now: open+lseek existence check (a miss still costs no frame),
    frame at 0.0 immediately, then per 64 MB chunk in all 4 passes,
    frac = (pass + bytes/size)/4. Callback is (float frac, msg, ud); STL/PLY
    use 0, 1/3, 2/3. Host test (104 MB OBJ): 0, .16, .25, .41 ... 1.0;
    vertex buffer byte-identical to the old loader.

Stars: 2500 world-fixed quads on a sphere r=400 (upper hemisphere), drawn
after the sky with the MVP VS and new ps_stars (outputs desc[36..39]; llvm-mc
bonaire; RSRC1 0x40), additive blend CB_BLEND0_CONTROL 0x40000101, depth
LESS no write (floor covers below the horizon). Fade: smoothstep over
orig_sun_y in [+0.12, -0.12] -> in from 0.87 s before moonrise to 0.87 s
after, out mirrored around moonset. Default view: ~112 stars, 1.5-3.9 px.
Changed lines clang-format clean (shadPS4 src/.clang-format, 18.1.8).

============================================================
CLEANUP: DIAGNOSTICS REMOVED  (build=cleanup-diagnostics)
============================================================
round-sun+start-of-day confirmed on hardware: 13456 frames, 0 dropped
(median 16.68 ms), GPU 5.6 ms median / 8.7 ms p95 per frame, depth clear
substitutes 1.0 (dz0 = 0x3f800000), mips active.

Removed (main.c 4812 -> ~4210 lines):
  - bring-up ladder and test modes: DRAW_STOP, RT_TEST, MINIMAL_TEST,
    VS_LOAD_TEST, BG_SKY_CLEAN / BG_CLEAN_* / BG_PS_MAGENTA (+ *_FORCE),
    SIMPLE_DRAW - stripped with unifdef for the full-scene config
  - bisection: SKY_NO_DRAW, EMPTY_FRAME, STATE_CUT / STATE_PAD_DW /
    STATE_REWRITE_N / STATE_SEC4_MASK, the g_sec4 table and STATE_CUT_POINT
  - fix switches made unconditional: WRITE_VGT_STAGES_DMA (the two VGT
    registers are never written; a comment at both sites records why),
    SCENE_NO_DEPTH (depth always on); HW_STATUS_POLL (was 0)
  - probes: drain/paced probe + wall detector, shadow-map probe (trace_smap),
    depth readback (dz0min/dz0max)
  - 8 unused shader binaries and their uploads: ps_grad, ps_skyclean, vs_bg,
    vs_ftload, vs_fulltri, ps_magenta (uploaded, never bound), ps_null,
    ps_depthonly (never uploaded)
Verified: the 8 remaining shader binaries are byte-identical; preprocessed
build_dcb / build_shadow_dcb / main() differ from before ONLY by the removed
diagnostics (build_shadow_dcb identical).

Kept on purpose:
  - CPU_FLIP (EOP fence, then sceVideoOutSubmitFlip). gnm's patcher turns the
    0x778 marker into WRITE_DATA(label=1) + NOP + WRITE_DATA(fence) - CP writes,
    no EOP (0xc0033700 / 0x500) - so the fence would land before the draws
    finish. The CPU flip is the correctly synchronized path.
  - trace log, GPU pass timestamps, CP checkpoints (cheap, still useful).
Still to do: comment rewrite + clang-format pass.

============================================================
ROUND, BIGGER SUN; START OF THE DAY  (build=round-sun+start-of-day)
============================================================
ps_dark decoded (CI opcodes from LLVM VOP1/2/3Instructions.td gfx6_gfx7):
    t = 0.5*(1 - clip_y);  sky = zenith + (horizon - zenith)*t
    d^2 = (clip_x - desc[16])^2 + (clip_y - desc[17])^2
    f = clamp(1 - d^2/desc[18], 0, 1)^2;  out = sky + (light - sky)*f
  (the old notes said w=radius^2 and *2 - both wrong; it is z, no *2)
  attr0.x (clip_x) is used ONLY for the sun distance (dword 35).
Old disc: d in NDC on 16:9 -> 105 px wide, 59 px tall (ellipse).
Now: the sky quad's clip_x and the sun's x are both multiplied by W/H, so d is
in pixel-proportional units -> round. SUN_DISC_RADIUS_PX 110 -> desc[18] =
(110/540)^2 = 0.0415 (was 0.012); moon keeps its 0.75 ratio (0.0311).
No shader change.

Start: sun_angle 0 = start of the day (first frame with sun_y >= 0, sun on
the eastern horizon). Camera unchanged (faces west), so the sun disc comes
into view later, near sunset.

Note from the depth step: near 0.01 / far 500 puts z = 0.999 at only ~10
units. If the trace shows dz0 = 0x3f7fbe77 (clear not substituted), most of
the floor would fail LESS - dz0 = 0x3f800000 means the clear works.

============================================================
DEPTH ON: 1D-TILED DEPTH SURFACE  (build=depth-1d-tiled)
============================================================
Full scene + radeonsi aniso: works.

Why depth crashed before: on CIK the DB takes its layout from DB_DEPTH_INFO
(radeonsi si_init_depth_surface, chip_class >= CIK: ARRAY_MODE / PIPE_CONFIG /
bank fields from the tile-mode entry; DB_Z_INFO.TILE_MODE_INDEX is SI-only).
We never wrote DB_DEPTH_INFO, so it stayed at its CLEAR_STATE value 0 =
ARRAY_LINEAR_GENERAL. CI addrlib only ever gives depth the depth table
entries (2D 0-4, 1D 5, PRT 6), never linear.

Now: DB_DEPTH_INFO = 0xc21 = ADDR5_SWIZZLE_MASK 1 (radeonsi: !tc_compatible
_htile) | ARRAY_MODE 2 (ARRAY_1D_TILED_THIN1) | PIPE_CONFIG 12
(P8_32x32_16x16) - the PS4 table entry Depth1DThin (5) per shadPS4
tiling.cpp. Field positions gfx_7_2_sh_mask.h, enums gfx_7_2_enum.h.
1D tiling needs pitch and height % 8 (SiLib micro-tiled alignment; base =
pipe interleave): 1920x1080x4 = 8294400 B fits the allocation (8306688 B,
64 KB aligned). DB_DEPTH_SIZE (239,134) / SLICE 32399 already matched.
Sky: DB_DEPTH_CONTROL 0x76 (Z write, ALWAYS) with DEPTH_CLEAR_ENABLE - the
game's clear minus stencil. Floor and cube: 0x16 (LESS + write).
SCENE_NO_DEPTH 0.

Diagnostic: trace fields dz0min / dz0max = min/max of depth tile (0,0)
(first 256 B = top-left 8x8 pixels, only the sky draws there):
  0x3f800000 = DB_DEPTH_CLEAR substituted (clear works without HTILE)
  0x3f7fbe77 = the sky quad's own z (0.999): the clear flag is ignored
               without HTILE; far floor beyond ~z 0.999 would then fail LESS

============================================================
ANISO = RADEONSI, FULL SCENE  (build=aniso-radeonsi+full-scene)
============================================================
mips+fence-spin result: floor looks better.

Aniso: the floor albedo/normal use the desc[8] sampler (MIMG SSAMP s12 <-
desc[8..11]); it set only MAX_ANISO_RATIO. radeonsi si_create_sampler_state
(GFX6/7, 16x = ratio 4 via si_tex_aniso_filter) also sets:
    word0 ANISO_THRESHOLD = ratio>>1 = 2 @16, ANISO_BIAS = ratio = 4 @21
    word1 PERF_MIP = ratio+6 = 10 @24
    word2 DISABLE_LSB_CEIL = 1 @29 (<= VI), FILTER_PREC_FIX = 1 @30
    (COMPAT_MODE / ANISO_OVERRIDE are VI+ only -> 0)
Positions from gfx_7_2_sh_mask.h. New aniso sampler words:
    w0 0x00820800  w1 0x0af00000  w2 0x68f00000
PCF (shadow) sampler: + DISABLE_LSB_CEIL, FILTER_PREC_FIX (radeonsi sets
them on every sampler for GFX6/7).

Ladder, final rung: DRAW_STOP_OFF (full-scene sky path, shadow pass on).
Compiled-frame diff vs DRAW_STOP 4 is exactly: sky VS g_vs_bg_gpu -> vs (MVP
VS 0xCB with the identity MVP at vb+0), interpolators set once for 2 params
(the floor's re-bind drops out), sky PS skyclean -> ps_dark (0xCA, M0 fixed,
no textures). Depth still off for floor and cube (SCENE_NO_DEPTH).

4K: not possible on this console - the original PS4 outputs at most 1080p;
4K output is PS4 Pro only.

============================================================
FLOOR MIPMAPS + FENCE SPIN  (build=mips+fence-spin)
============================================================
1) Floor albedo/normal mip chains (FLOOR_TEX_MIPS 9 -> 4096 down to 16x16).
   Layout from AMD sources, not assumed:
     addrlib (PAL releases/amd-18.40, r800/siaddrlib.cpp):
       HwlComputeMipLevel     level width = max(1, basePitch >> level)
       HwlGetPitchAlignmentLinear  pitch align = max(8, 64/4) = 16 texels
       HwlGetSizeAdjustmentLinear  pitch*height padded to max(64, PI/4) texels
     PAL addrMgr1.cpp: level offset = running size aligned to baseAlign
       (= pipe interleave); pow2Pad = (mipLevels > 1)
   Built only for power-of-two sizes, down to 16x16: there pitch == width,
   every level is a multiple of 512 B, so the layout is a plain running sum
   whether the pipe interleave is 256 or 512.
   T#: LAST_LEVEL @16, POW2_PAD @25 (gfx_7_2_sh_mask.h; radeonsi sets
   POW2_PAD(last_level > 0) on GFX6-8). word3 0x90800fac -> 0x92880fac.
   Sampler (floor albedo/normal + cube): MIP_FILTER = LINEAR (2) @26
   (SQ_TEX_Z_FILTER_LINEAR). Single-level textures are unaffected.
   Loader: mips are generated while the rows stream in (2x2 box filter,
   rounded), each level written sequentially; nothing is read back from
   GARLIC. Host test with the real floor_displacement.bmp: all 9 levels
   byte-identical to an independent numpy reference; max_levels=1 is
   byte-identical to the old loader; a 300x200 BMP stays single-level.
   Trace line: "floor tex alb=WxH levels=N nrm=WxH levels=M".

2) Fence wait: the 1 ms usleep loop is now a bounded hot spin (the game's
   own wait at 0x132370 spins with no sleep), clock checked every 32 pause16
   batches; budget 250 ms, 2 ms after repeated timeouts (unchanged policy).
   Trace field fenceit (1 ms sleeps) is now fwait (microseconds).

GPU pass timestamps (gsh gsk gfl gcu gtot, gts0) are still logged, so the
effect of the mips on the floor pass is measured, not guessed.

============================================================
GPU PASS TIMESTAMPS  (build=gpu-pass-timestamps)
============================================================
shadow-pass result: works (cube shadow). Slow frames 238 -> 754 of 5737.
CPU is NOT the cost - medians, fast vs slow frames (us):
    build 11/11  submit 26/26  pad 14/18  untimed (camera, cube, copies) ~725/~740
    flip loop wB 15888 / 32561   <- all of the difference
The loop is serial: wait fence -> SubmitFlip -> wait flip event -> next frame.
A frame = ceil((~0.8 ms CPU + GPU time + fence-notice delay) / 16.7 ms) vblanks.
The shadow pass added only GPU work and tripled the slow frames.

Confirmed extra delay on the critical path: after a short spin the fence wait
sleeps in 1 ms steps (sceKernelUsleep(1000)); fenceit = 2-3 on normal frames,
so the GPU's completion is noticed up to ~1 ms late before the flip is queued.

This build measures GPU time per pass with EOP timestamps:
  EVENT_WRITE_EOP, BOTTOM_OF_PIPE_TS (40), EVENT_INDEX 5, DATA_SEL 3 = 64-bit
  GPU counter (cikd.h; shadPS4 BottomOfPipeTs=40 / GpuClock64=3), INT_SEL 0.
  Host-run encoding: c0044700 00000528 <lo> 60000020 0 0 - same packet as the
  proven fence (c0044700 00000504 <lo> 20000020 <v> 0) except event/data_sel.
  Stamps: [0] frame start [1] after shadow pass [2] after sky [3] after floor
  [4] after cube. Trace fields (GPU ticks): gsh gsk gfl gcu gtot, and gts0 raw
  to calibrate the counter frequency against ptms (not assumed).

============================================================
LADDER STEP: DRAW_STOP 4 (+shadow pass)  (build=shadow-pass)
============================================================
DRAW_STOP 3 result: good (lines gone, cube drawn). 9812 frames, no stalls or
fence timeouts; 238 frames at 33 ms (one missed vblank), clustered while the
camera moved. The trace has no GPU timestamps, so this is not proven GPU time.
Likely cost: floor textures are 4096x4096 with NO mip levels (T# LAST_LEVEL
0) sampled 3x per pixel with a 16x anisotropic sampler - looking toward the
horizon, distant pixels fetch texels scattered over 64 MB textures. Fix =
mip chains in the exact CI layout (addrlib rules): its own step.

DRAW_STOP 4 = rung 3 + the shadow pass, keeping the BG_SKY_CLEAN sky
(DRAW_STOP_OFF would ALSO switch to the full-scene sky path - two changes).
Gate changed: #if !MINIMAL_TEST && (!DRAW_STOP || DRAW_STOP >= 4).

Checked before enabling:
  - shaders: vs_shadow (pos done + 0xCB), shadow-clear PS 0x4A, shadow PS
    0x18A with M0; neither PS samples a texture (no WQM needed)
  - own interpolator state, VGT_SHADER_STAGES_EN / VGT_DMA_SIZE gated off,
    DB_Z_INFO=0 and no depth writes, 4096x4096 viewport/scissors,
    ACQUIRE_MEM RT->texture barrier at its end
  - state leak: the shadow pass shares the command buffer with the main pass
    (no CLEAR_STATE between). Compiled-code analysis: all 57 context, 4 SH and
    2 UCONFIG registers it writes are rewritten by the main pass - nothing
    leaks.
  - order: pm4_init -> build_shadow_dcb -> build_dcb appends (no reset).
Expected: the cube casts a shadow on the floor (8-bit NDC.z in RGBA8 R).
Last time this pass ran (before the VS done-bit / RSRC1 fixes) the GPU hung
in frame 0 after checkpoint 0x21.

============================================================
WQM + EXACT EXPORT, and LADDER STEP: DRAW_STOP 3 (+cube)
(build=wqm-exact-export+cube)
============================================================
ps-m0-primmask result: speckles and half-screen flicker gone, floor smooth.
Remaining: 1-2 px lines exactly on triangle edges (a full-width row at y=779
in 1920x1080 = a grid edge; faint diagonals = quad diagonals). Pixel values:
neutral grey 28-36 inside grey 48-52 floor - floor shading, not sky through a
crack.

Cause: the floor PS samples 3 textures with implicit LOD (derivatives from
the 2x2 quad, 16x aniso sampler) but never enables whole-quad mode, so at
triangle edges the helper lanes never ran v_interp and their coordinates are
stale -> wrong derivatives -> wrong filter footprint on edge pixels.
LLVM SIWholeQuadMode.cpp: "Whole quad mode is required for derivative
computations"; prolog "S_MOV_B64 LiveMask, EXEC / S_WQM_B64 EXEC, EXEC".
LLVM EXPInstructions.td: EXP has DisableWQM = 1 -> exports run in exact mode.
radeonsi: last colour export sets valid_mask=1 (vm) and done=1.

Patched (encodings from llvm-mc -mcpu=bonaire):
  ps_floor_binary   s_mov_b64 s[88:89],exec  0xBED8047E | s_wqm_b64 exec,exec
                    0xBEFE0A7E ... s_mov_b64 exec,s[88:89] 0xBEFE0458 before
                    the export; export vm=1; OrbShdr length 496 -> 508
  ps_shader_binary  (cube) same with s[32:33]: 0xBEA0047E / 0xBEFE0420;
                    length 164 -> 176
  Only these two PS sample textures. Registers fit (floor 64/91 of 64/96,
  cube 44/35 of 44/48). Verified: each new binary == original + exactly these
  dwords; all other binaries unchanged.
  NOTE vm=1: an earlier session recorded full-screen noise with vm=1 on a
  hand-encoded magenta PS and switched everything to vm=0. The rule ("set at
  least once per PS") and radeonsi say vm=1. If the floor/cube turn into
  noise, that bit is the suspect.

Ladder: DRAW_STOP 3 = sky + floor + cube (no shadow pass). FLOOR_NO_DEPTH is
now SCENE_NO_DEPTH and also covers the cube: the cube set LESS + write, which
would fail against the never-cleared depth buffer exactly as the floor did.
The cube is convex, back-face culled and drawn after the floor.

============================================================
PS M0 / PRIM_MASK FIX  (build=ps-m0-primmask)
============================================================
Probe result (shadowmap-probe): desc[40] T# base == shadow map address
(0x202400000), format 8_8_8_8, texels 0xff0000ff from init to frame 18432.
The shadow lookup is correct; the dark rectangle is gone.

New symptom: screen-space split - left half smooth, right half dark specks,
flickering. A world-space bug cannot split the screen; per-unit garbage can.

Cause: every PS that interpolates (floor, cube, full-scene sky, shadow,
loading-screen blue) ran v_interp WITHOUT setting M0.
  LLVM SIInstructions.td:  let Uses = [MODE, M0, EXEC] in { V_INTERP_P1_F32 ...
                           (int_amdgcn_interp_p1 ..., M0) }
  radeonsi si_shader.c:    PS args = user SGPRs, then PRIM_MASK (hardware-
                           provided), which the interp intrinsics take as M0.
M0 held whatever the previous wave left there, so parameter data was read
from the wrong LDS location - different per unit, per frame. shadPS4 ignores
M0 for interpolation. The sky PS on this rung uses FragCoord only.

Fix: s_mov_b32 m0, s2 (0xBEFC0302, assembled with llvm-mc -mcpu=bonaire) at
dword 2 of each interpolating PS - after the 2-dword SDK header token
0xBEEB03FF (shadPS4 checks code[0] for it). All PS bindings use 2 user SGPRs,
so PRIM_MASK is s2. No branches or PC-relative ops in these shaders. Each
OrbShdr trailer's length (bits 8-31, bytes of code) was increased by 4:
shadPS4 translates exactly length/4 dwords.

The fine grid on the floor is not vertex seams: mesh cells are 6.25 world
units (64x64 over 400); the visible squares are far smaller - the texture's
own tile pattern repeating.

============================================================
SHADOW MAP PROBE  (build=shadowmap-probe)
============================================================
shadowmap-init did not remove the dark rectangle. From the floor PS IR:
    light clip = light_MVP(desc[48..63]) x world_pos
    uv = 0.5 + 0.5*xy/w,  ref = clamp(z/w, 0, 1)
    stored = sample(desc[40..47] T#, uv).x        (plain sample, ALU compare)
    shadow = stored < ref ? 0 : 1,  1 outside w>0,  factor = 0.5 + 0.5*shadow
The IR also confirms the shadow sample's T# comes from desc[40..47] (albedo
64, normal 72). With the map filled to R=1.0, "stored < ref" cannot be true,
and the blocks inside the rectangle mean the sampled values vary per texel -
a uniform fill cannot do that. So at runtime the sampled memory does not hold
the fill (or the run was not this build: no trace was sent).

This build logs, to trace.log:
    "smap init f=-1 ..."   before the main loop
    "smap f=60 ..."        and every 1024 frames
  smap=     shadow map address (CPU)
  t40base=  the address the desc[40] T# actually points at
  t0/tmid/tlast  three texels of the map (expected 0xff0000ff)
  alb/nrm   floor albedo / normal texture addresses
Send trace.log with the screenshots.

============================================================
SHADOW MAP INIT  (build=shadowmap-init)
============================================================
vs-pos-done-fix result: floor renders in perspective, textured, 12310 frames
at 60 fps, no slow submits, no fence timeouts.

Dark rectangle with no caster: on DRAW_STOP rungs the shadow pass does not
run, so the shadow map (RGBA8, desc[40..47]) kept gpu_alloc's zero fill. The
floor PS compares with the PCF sampler (LessEqual, ClampBorder, white border):
inside the light frustum every compare against 0 failed -> shadowed; outside it
the white border (1.0) passed -> lit. A sharp moving rectangle = the light
frustum footprint, drifting with the sun (auto_spin = 0: the camera is still,
the sun is what moves each frame).
Fix: fill the map with the shadow-clear value at allocation - ps_shadow_clear
outputs (1,0,0,1) = 0xFF0000FF per texel - the same state the shadow pass's
clear produces every frame.

Known, not changed in this build:
  - Floor textures have one mip level (T# LAST_LEVEL 0): distant floor aliases
    and shimmers when the camera moves.
  - FLOOR_NO_DEPTH: the displaced floor can overdraw itself in triangle order.
    Fixed by the depth rung.
  - build_static_vb comment says the displacement tiles "UV_MAX times (same
    tile rate as albedo)", but DISP_TILES = 32 while UV_MAX = 100, on a 64x64
    grid (6.25-unit cells vs 12.5-unit relief tiles): relief creases do not
    line up with the albedo tiles.

============================================================
VS EXPORT FIX  (build=vs-pos-done-fix)
============================================================
Last run: frame 0 never retired. cpm=0x1f (the CP reached the frame's LAST
checkpoint) but fence=0 forever: the end-of-pipe event never fired, so a draw
was stuck in the shader/raster pipeline, not in command processing.

Cause: the MVP vertex shader's exports.
    vs_bg_binary (works)     exp pos0 done=1 | exp param0
    vs_shader_binary (hung)  exp pos0 done=0 | exp param0 | exp param1 done=1
    vs_shadow_binary         same as vs_shader_binary
radeonsi (si_shader.c, SI/CI): positions are exported first and done=1 is set
on the LAST POSITION export; parameter exports never set it. Without a done
position export the primitive assembler never proceeds. The floor now uses
this VS (previous build), and the full scene's shadow pass uses vs_shadow -
both hung in frame 0. shadPS4 does not model the bit, so it worked there.

Patched both binaries (DONE is bit 11 of the export's first dword):
    dword 51  0xf80000cf -> 0xf80008cf   pos0: set done
    dword 56  0xf8000a1f -> 0xf800021f   param1: clear done
Every embedded shader now follows the rule (VS: last pos done, no done on
params; PS: last colour export done). ps_depthonly_binary has no export at
all but is never uploaded or bound (dead).

Not changed: all our PS colour exports use vm=0, while radeonsi sets
valid_mask=1 on the last one. The sky PS runs on hardware with vm=0 and the
source records a deliberate "VM=0 export fix", so it is left as is.

============================================================
SHADER DUMP REVIEW  (build=floor-mvp-vs+rsrc1-fix, printed in the trace header)
============================================================
1. shadPS4 crash, explained. The dump folder has vs_..._0.spv and
   fs_0xcafe00e3_0.spv (sky) but NO fs_0xcafe0119_0.spv (floor): shadPS4 died
   emitting the floor PS's SPIR-V. That run drew the floor with the sky's state,
   NUM_INTERP=0, while the floor PS reads Param0 and Param1. shadPS4's
   input_params slots for undefined inputs stay default (id 0, component_type
   0), and EmitGetAttribute emits OpLoad(0,0) with no assert - the log just
   stops. The floor VS/interpolator fix in the previous build removes that
   state; shadPS4 could also return 0 for an undefined input instead of
   emitting invalid SPIR-V.

2. PGM_RSRC1 register allocations. LLVM 18 cannot disassemble CI, so a small
   CI decoder measured each embedded shader's max VGPR/SGPR (validated: exact
   match with shadPS4's IR for the floor PS, sky PS and BG VS). SGPR need
   includes +2 for VCC (LLVM getNumExtraSGPRs, all GFX < 10). Five were short:
     floor PS          0x28D  56 V / 88 S   uses 64 V / 90 S  -> 0x2CF
     MVP VS (sky, floor, shadow)  0x4B  48/16  uses 48/31     -> 0xCB
     shadow-clear PS   0x0A   44 / 8        uses 44/12        -> 0x4A
     full-scene sky PS (ps_dark via ps_bg)  12/16  uses 43/31 -> 0xCA
     shadow PS         0x14A  44 / 48       uses 44/50        -> 0x18A
   The floor PS WROTE v60-v63 outside its allocation. The shadow-clear PS and
   the MVP VS are the first draw after checkpoint 0x21, where the full scene
   hung in frame 0. All 15 PGM_LO bindings now fit.

3. Descriptors: every desc range the floor PS reads ([8..15], [32..35],
   [40..83]) is written. The layout comment's "light color at desc[20..23]"
   is stale - code and shaders use desc[32..35].

Header buffer grew to L[384]: the build line took the worst case to ~263 B.

============================================================
LADDER: DRAW_STOP 2 (sky + floor) - floor VS + load time
============================================================
Last run (PS4 photo): sky + a thin ragged line at ~87% screen height, full
width, 59.96 fps. That line is the floor drawn WITHOUT a camera transform.

WHY: with BG_SKY_CLEAN the sky binds g_vs_bg_gpu, a pass-through VS (shadPS4
dump vs_0xaabbee03: Position0 = raw vertex at VertexId*48+80, no MVP). The
floor draw only switched the PIXEL shader, so it inherited the pass-through
VS: world coordinates used as clip space. A floor at y~-0.5 with +-0.75
displacement lands around 75-87% down the screen, ragged by the displacement.

FIX: before the floor draw, bind the GPU-MVP VS (vs, RSRC1 0x4B, 4 user SGPRs)
and the full-scene interpolator state. Diffing the compiled state at the floor
draw, sky config vs full-scene config, gives exactly:
    SPI_PS_INPUT_CNTL_1 1, SPI_VS_OUT_CONFIG 1, SPI_PS_INPUT_ENA/ADDR 0x02,
    SPI_PS_IN_CONTROL 2   (the floor PS reads Param0 and Param1)
After the fix there are no remaining context differences at the floor draw.
The sky itself is unchanged.

SLOW LOAD (100% CPU): bmp_load allocated its row scratch buffer with the
caller's allocator - GARLIC for the albedo and normal maps - and the
conversion loop read it back byte by byte: ~100M uncached reads for two
4096x4096 files (it also leaked one direct-memory block per call). The row
buffer is now a static buffer in normal cached memory (max width 8192; wider
files return -7 and the fallback texture is used).

Still true: nothing writes depth (sky 0x72, floor 0 via FLOOR_NO_DEPTH).
Note: the build has no -O flag (clang default -O0), which slows every CPU loop.

============================================================
FIX: WRITE_VGT_STAGES_DMA 0
============================================================
Bisection result: cut 3 + {PA_SC_MODE_CNTL_0, VGT_SHADER_STAGES_EN,
VGT_DMA_SIZE} (SEC4 0x130000, 600 B) reproduces the 512 stall and the 548
wedge. The 10 value-changing writes, the other 8 same-value writes, size and
packet count were all clean.

Static check against the reference binaries:
  PA_SC_MODE_CNTL_0     gnm writes it = 0 via SET_CONTEXT_REG in its own
                        default-state tables (0x7e08, 0x7fec, 0x81dc). Sanctioned.
  VGT_SHADER_STAGES_EN  never written by gnm or the game via SET_CONTEXT_REG.
                        The game's 55 "mov edx,0x2d5" sites all call one
                        non-GPU function (0x2304c0).
  VGT_DMA_SIZE          never written by gnm or the game. The CP loads index
                        sizes from the draw packets (shadPS4 mirrors this:
                        regs.max_index_size = draw_index->max_size).
Our frame wrote both every frame because its state was modelled on shadPS4's
register struct (index_size = 0xA29D, stage_enable = 0xA2D5).

THIS BUILD: the real sky frame (SKY_NO_DRAW 0, STATE_CUT 0) without those two
writes, in the main and shadow paths. PA_SC_MODE_CNTL_0 kept, as gnm does.
Header shows "NO_VGT_STAGES_DMA".
    runs past 548 at 60fps, ioctl ~15us -> fixed
    still fails                          -> it is PA_SC_MODE_CNTL_0

Also corrected: pm4.h said 0x290 is VGT_SHADER_STAGES_EN; it is VGT_GS_MODE.
CTX_* names now carry the AMD register names.

============================================================
AUDIT FIXES (this build)
============================================================
- Header trace buffer char L[96] held 5 lines: 90 bytes with measured values,
  ~193 worst case (any negative return). Overflowed the stack on error paths;
  adding one more line overflowed it always. Now L[256].
- Teardown fence wait compared against fv, which is already one past the last
  submit, so it never passed and always burned 250ms; with FENCE_SLOTS it could
  wait on an unsubmitted slot. Now waits on g_last_fence/g_last_fv recorded at
  the accepted submit. Teardown log prints fence= and want= from those.
- MapComputeQueue comments said 4/12; hardware returned 5/13. Corrected.
- Trace header now prints "scene cfg=". With empty EXTRAFLAGS the build is
  DRAW_STOP 1 + BG_SKY_CLEAN: sky only, no floor/cube/shadow.

Compile check (gcc -fsyntax-only -Wall -Wextra, freestanding): 0 errors in
default, DRAW_STOP_OFF and DRAW_STOP=3. 92 warnings, all pre-existing:
26 unused/set-but-unused variables, 66 misleading-indentation (one clamp macro
plus loader one-liners, all semantically correct).

============================================================
*** NEXT TEST: CPU_FLIP - EOP FLIP vs CPU FLIP ***
============================================================
  fgpu=1 at the stall, so the flip that never retires is a GPU/EOP flip,
  registered by gnm's marker patcher through sceVideoOutSubmitEopFlip. A CPU
  flip does not touch that machinery at all.

  CPU_FLIP 1 builds the DCB with no_flip=1 (EOP fence, no marker), submits with
  plain sceGnmSubmitCommandBuffers, waits for the fence, and then presents with
  sceVideoOutSubmitFlip from the CPU. That is the path this app used before the
  marker protocol went in, so it is known to work.

      survives past 547 flips -> the limit is in the EOP-flip path specifically,
                                 and the marker protocol is the problem
      still stops at 547      -> the limit is in the display's flip accounting
                                 regardless of how the flip was registered

  Either answer is worth the run. New fields: cff= (CPU flip failures) and
  cfr= (last CPU flip return code), so a failing present cannot hide.

  NUM_FRAMES is back to 3.

  make      (no .py in the build)

============================================================
*** NEXT TEST: NUM_FRAMES 4 ***
============================================================
  What we know exactly: the display completes 547 flips and stops. fnum freezes
  at 547, fpend and fgpu stick at 1 forever, fcur stays on buffer 0, and the
  vblank counter keeps ticking the whole time - so the display is alive and the
  GPU did its job (label written, fence advanced, cplag 0).

  With 3 buffers, 547 flips means buffer 1 received (547-1)/3+1 = 183 of them.
  Four buffers changes the per-buffer count without changing the global flip
  count, so the two separate cleanly:

      stops at ~547 flips again   -> a GLOBAL flip limit; buffer count is
                                     irrelevant and the search moves to what
                                     the display driver counts per process
      stops at a different frame  -> a PER-BUFFER limit, and the frame number
                                     tells us how many flips one buffer survives

  NUM_FRAMES only drives fb[], dcb_mem[], bi = frame % NUM_FRAMES and the
  RegisterBuffers count, so nothing else needs changing. Costs 8MB more of
  framebuffer (33MB vs 25MB).

  Put it back to 3 afterwards - the reference registers 3.

  make      (no .py in the build)

  Send the trace either way; both outcomes are informative.

============================================================
*** THE REMAINING PROBLEM: 2 FPS, AND MY TIMERS DO NOT SEE IT ***
============================================================
    dt = 478391 us, and the measured components are:
        evt 4   pad 36   build 8   ioctl 50   submit 58   flip 25   done 16
        total 139 us
    478252 US UNACCOUNTED - 99.97% of the frame.

  wait= (now - t_submit) captures it, so it is inside that region, but every
  sub-timer in there reads microseconds. It is NOT:
      the submit ioctl      ioctl=50us  (the original 510ms stall WAS in the
                            ioctl - 244813us. This is a different problem.)
      the fence wait        fenceit=0, so the loop never ran
      the flip event wait   PACE_ON_FENCE_ONLY, we do not call WaitEqueue
      phase()               returns early below batch 500 and only buffers
      the trace write       frames were already 478ms before adaptive logging
                            engaged at all

  I am not going to guess at it. This build splits that region with THREE REAL
  timestamps - not gated, so valid on every frame:
      wA=   t_submit -> start of the flip loop
      wB=   the entire flip loop
      wC=   end of the flip loop -> end of frame
  wA + wB + wC accounts for the whole of wait=, so one of them will hold the
  478ms and the next trace localises it exactly.

  make

  Send the next trace. Read wA/wB/wC - whichever is ~478000 is where the frame
  is going.

============================================================
*** EVERY gnm EXPORT NOW NAMED AND CLASSIFIED ***
============================================================
Pulled shadPS4's LIB_FUNCTION NID table (251 entries) and joined it against the
firmware, so every export in libSceGnmDriver.sprx is now named rather than a
NID string. Full table attached as gnm_exports_classified.txt.

    TOTAL 251    REAL 93    STUB/CONST 158    REAL and reaching an ioctl 72

ALL 72 LIVE EXPORTS ARE NAMED - 72 of 72, no unknowns left. The only three that
shadPS4 has no name for are Func_4774D83BB4DDBF9A / B0A8688B679CB42D /
BADE7B4C199140DD at 0x1a10 / 0x1a60 / 0x1ab0, and those are the
libSceGnmWaitFreeSubmit enable/disable pair I identified from the submit path
weeks ago.

WHY THIS SETTLES A QUESTION
  I have been assuming sceGnmDebugHardwareStatus is the only live GPU-health
  query on retail. That was an assumption based on the handful of names I
  happened to remember. It is now CHECKED across the whole export table:
  of 251 exports, 72 are live, and none of the other 71 reports GPU state.
  The diagnostic surface really is one boolean.

  It also confirms the reset situation: DebugReset, DebugModuleReset and 155
  other entries return 0x8eee00ff or 0 without touching the driver. There is
  no way to reset or inspect a wedged GPU from userland beyond that one call.

WORTH NOTING FROM THE TABLE
  sceGnmRequestFlipAndSubmitDone (0x19a0) is LIVE - an alternative flip path
  that combines SubmitDone with the flip request. We use SubmitAndFlip. Not
  changing it without a reason, but it is the one live API in the flip area we
  have never exercised, and now it is on the record rather than buried in a NID.

  make

The build is unchanged from the last round apart from hwstall=, which samples
the kernel's GPU verdict at the exact frame the fence first times out.

============================================================
*** AND THE LIMIT OF WHAT STATIC ANALYSIS CAN REACH ***
============================================================
I traced the per-flip path to its end this round: SubmitEopFlip (vo 0x1970)
builds its 48-byte arg and calls ioctl 0xc0308203. No counter, no limit, no
queue depth anywhere in userland. The "flip queue is full" state that returns
0x80d11081 is decided KERNEL-side, and there is no 12.02 kernel dump.

Combined with the kqueue finding (the event queue coalesces and cannot
overflow), the userland CPU path no longer contains a candidate for
"something consumed once per frame that runs out at ~547". Per-frame kernel
entries are down from 21 to 5, and the five that remain are the two the game
also makes plus three that are unavoidable.

  make

============================================================
*** CPU PATH: TRACE I/O WAS 16 SYSCALLS AND 8 FSYNCS PER FRAME ***
============================================================
Enumerated every call site in our frame loop and looked at what each costs.

  trace_line() is  sceKernelWrite + sceKernelFsync  - two syscalls, one of them
  a SYNCHRONOUS FLUSH TO STORAGE. phase() called it 8 times a frame inside the
  marker window, so every frame in that window paid 16 syscalls and 8 fsyncs.
  That is both a large per-frame cost and enough I/O to distort the very timing
  we are trying to measure.

  FIXED: phase markers now accumulate into a buffer and the whole frame's worth
  goes out in ONE write + ONE fsync via phase_flush() at the end of the frame.
  Same data, same durability, 1/8th the syscalls.
      inside the window:  16 syscalls / 8 fsyncs  ->  2 syscalls / 1 fsync
  Outside the window phase() still returns immediately.

  Combined with last round's timestamp gating (15 kernel round trips per frame
  -> ~1), the instrumentation now costs a small fraction of what it did.

ALSO: THE ORDER CHECKER NOW UNDERSTANDS FORWARD DECLARATIONS
  It fired on phase()/lg_i64 and phase_flush()/trace_line. One was real -
  trace_line had no prototype and phase_flush is defined 100 lines above it -
  and one was a FALSE POSITIVE: lg_i64 has a forward declaration that the
  checker was not modelling.

  A checker that cries wolf gets ignored, which defeats the point of having it,
  so I taught it that a prototype satisfies the dependency exactly as a
  definition does. Then re-ran it against the deliberately-broken header from
  before to confirm it still catches a REAL error:
      *** pm4_leading_tag uses pm4_have_space which is defined LATER ***  exit=1
  and against all three real sources: 42 + 25 + 17 functions, clean, exit=0.

  The missing trace_line prototype is added. It is now run over main.c too,
  not just the headers - that gap is why it did not catch this one earlier.

  make

REMAINING PER-FRAME CALLS, for reference
  1 sceKernelGetProcessTime (needed - dt drives the adaptive logging)
  1 sceSystemServiceReceiveEvent, 1 scePadRead  (the game does both, 1 site each)
  1 submit ioctl, 1 SubmitDone
  1 non-blocking WaitEqueue drain (returns immediately when the queue is empty)
  sceGnmAreSubmitsAllowed x3 - NOT syscalls, it is a plain memory read of the
  in-flight counter, traced from the PRX

============================================================
*** THE GAME'S PER-DRAW PACKET STREAM, DECODED ***
============================================================
Disassembled draw_pass_A (0x9505f0) for the actual PM4 headers it emits, to see
what per-draw state the game sets that we might be missing.

    0xc0055800  ACQUIRE_MEM, coher_cntl 0x82000040   bits 31, 25(CB), 6(CB_DEST)
    0xc0004600  EVENT_WRITE, dword1 0x72e            see below
    0xc00e6900  SET_CONTEXT 0x318 count=14           CB_COLOR0_BASE + 13
    0xc0016900  SET_CONTEXT 0x31c count=1            CB_COLOR0_INFO
    0xc0016900  SET_CONTEXT single x4                registers not resolved
    0xc0026900  SET_CONTEXT count=2                  registers not resolved
    0xc0017900  SET_UCONFIG count=1                  primitive type
    0xc0044700  EVENT_WRITE_EOP dword1 0x504         identical to ours
    0xc0053c00  WAIT_REG_MEM count=5                 InsertWaitFlipDone

OUR RENDER-TARGET BLOCK IS CONFIRMED - three-way agreement:
    Mesa      R_028C60_CB_COLOR0_BASE -> (0x028C60-0x028000)/4 = 0x318
    the game  SET_CONTEXT 0x318 count=14
    ours      #define CTX_CB_COLOR0_BASE 0x318, emitted with 14 registers
              in BOTH passes
  CB_COLOR0_INFO at 0x31c is the 5th register of that block; the game writes
  the block then re-writes INFO separately, we set it as r[4] inside the block.
  Equivalent.

THE ONE THING I COULD NOT RESOLVE, stated plainly
  The per-draw EVENT_WRITE, dword1 = 0x72e:
      EVENT_TYPE  [5:0]  = 0x2e = 46
      EVENT_INDEX [11:8] = 7
  EVENT_INDEX 7 is the CACHE-FLUSH class in Mesa's scheme (0=other,
  1=ZPASS_DONE, 2/3=sample stats, 4=partial flush, 5=EOP timestamp, 6=EOS,
  7=cache flush). But EVENT_TYPE 46 is not in any table I could corroborate -
  the ones I could verify are 4=CACHE_FLUSH_TS, 20=CACHE_FLUSH_AND_INV_TS,
  37=CR_DONE_TS, 40=BOTTOM_OF_PIPE_TS.

  So: a cache-flush-class event of unknown exact type. NOT ACTED ON.
  Our render-target -> texture barrier uses ACQUIRE_MEM with the coher_cntl the
  FIRMWARE ITSELF uses in CLEAR_STATE (0x2ec47fc0 -> our 0x0ec00040: CB, DB,
  TC, TCL1, K$, CB_DEST) - a firmware-sourced barrier, not an invention.
  Emitting an extra event whose type I cannot name is exactly the speculative
  packet that has cost flashes before. Recorded with the evidence so it can be
  settled if a Sea Islands event table turns up.

  make

============================================================
*** BATCHING IMPLEMENTED — 16 FRAMES PER SUBMIT ***
============================================================
THE LEVER, proven by our own earlier data: when we submitted TWO command
buffers per call, the wall came at the same CALL count (~508), not half. So the
quota is per IOCTL CALL, not per command buffer or per frame. K frames can go
in ONE call for ONE unit of quota.

    K=1   (before)   513 frames =   8.6s @60fps, then  2.0 fps   lag  16.6ms
    K=16 (this build) 8208 frames = 136.8s @60fps, then 31.4 fps  lag 265.6ms
    K=32              16416 frames = 273.6s @60fps, then 62.7 fps lag 531.2ms

HOW IT IS BUILT:
  BATCH_FRAMES 16, NUM_FRAMES 16 (main.c). Set BATCH_FRAMES to 1 to restore the
  exact previous per-frame behaviour - everything else still works.

  Sub-frames accumulate into ONE command buffer; only every 16th iteration
  submits. Each sub-frame ends with its OWN EOP fence value (fv+k), so the CPU
  can wait for frame k individually, flip it, and pace on the flip event. The
  GPU renders all 16 back-to-back; the CPU paces the flips at vblank, so the
  display rate is unchanged - we just spend ONE submit for 16 frames.

  THE HARD PART, and why this needed the DMA helper: the CPU can only hold ONE
  version of the shared vertex buffer before a submit, and the cube rotation
  rewrites vertex DATA every frame (not just the matrix). So each sub-frame's
  data is staged to its own slot and copied BACK into vb by a DMA_DATA packet
  at the head of that sub-frame's block, at GPU execution time.
  One contiguous range covers every per-frame CPU write:
      vb+BG_SUN_OFF(64) .. vb+FLOOR_MVP_OFF+64 = 2176 bytes
      (BG_SUN, MVP, SUN_DIR, rotated cube verts, floor MVP mirror)
  plus the light-space MVP (64 bytes) which lives outside that range.
  2 DMA copies per sub-frame, 2240 bytes staged per frame, 35KB total.

  ORDERING (this was a bug I caught and fixed before shipping): the DMA copies
  must be emitted BEFORE the shadow pass, because the shadow pass reads the
  light MVP and the rotated cube vertices. Order per sub-frame is now
      DMA restore -> shadow pass -> main pass -> EOP(fv+k)

  BUDGET CHECK: 16 x (318 + 14) = 5312 dwords against a 32768-dword DCB cap.
  16 framebuffers = 128MB, DCBs 4MB, staging 35KB. All comfortable.

  NUM_FRAMES is 16 so no framebuffer is reused inside a batch - the GPU runs
  the whole batch before the CPU flips any of it, so reuse would race. That is
  also why this needs no GPU-side wait packets and cannot deadlock.

THE TRADEOFF, stated plainly: frames-per-submit IS input latency, because the
whole batch is recorded before it is submitted. At K=16 the camera responds
265ms late. For a walkable demo that is noticeable. For the GPU conformance
harness this is aimed at, it costs nothing - nothing there is interactive.

  make

READ THE LOG:
  subc= now counts BATCHES, not frames. 16 frames per subc.
  dt=    is now per BATCH (~266ms at 60fps), not per frame.
  ioctl= should stay 15-35us for the first ~513 BATCHES = 8208 frames.
  If the picture animates smoothly for over two minutes before any slowdown,
  batching works. After that it should degrade to ~31fps, not 2fps.
  If it renders wrong (stale/duplicated frames), the DMA restore is not
  covering something the CPU writes per frame - that is the thing to check
  first, and BATCH_FRAMES 1 immediately isolates it.

============================================================
THE MODEL, CONFIRMED (frames 503-540 of the last trace)
============================================================
    f=503-506  ioctl=15us       60fps
    f=507      ioctl=495301us   WALL
    f=508-511  ioctl=510053+    slow, back-to-back
    f=512      ioctl=12us       after 10s idle -> FAST
    f=513-523  ioctl=12-13us    600ms pacing -> ALL FAST
    f=524+     ioctl=509099+    pacing stopped -> SLOW again
  build stays 6-8us and flip 12-14us THROUGHOUT. Only the submit ioctl moves.

  RULE: ~512 free submits, then a HARD MINIMUM ~510ms between consecutive
  submit ioctls, measured from the last submit (so idling satisfies it).

RULED OUT (checked against the real GoW trace and psdevwiki, not assumed):
  CATEGORY 'gd' -> SCE_LNC_APP_TYPE_BIG_APP, the same class as a game. Correct.
  APP_TYPE 1 = "Paid Standalone Full App", what real games use. Correct.
  ATTRIBUTE - all 32 documented flags are CPU-mode(6/7)/NEO/HDR/VR/HDCP/button
    assignment. NONE grants GPU submission rate.
  sceKernelSetGpuCu - GoW calls it ZERO times.
  PAID - already tested on hardware: boots, wall unchanged.
  The /dev/gc ioctls are UNDOCUMENTED publicly (psdevwiki lists every other
  device and has no gc section), which is why searching never finds this.

============================================================
IF THE WALL IS REAL: THE BATCHING DESIGN
============================================================
  measured: SUBMITS are capped. FLIPS are not (13us, always). build_dcb is 6us
  and we use ~2k dwords of a 32768-dword buffer. The constraint is SUBMITS PER
  SECOND - not work, not bytes, not flips.

      for i in 0..K-1:
          InsertWaitFlipDone(buffer[i % N])   // GPU waits for that buffer's
                                              // previous flip to complete
          render frame i into buffer[i % N]
          EOP: fence[i] = i+1
      submit ONCE
      CPU flips buffer[i % N] as fence[i] signals, paced at vblank

  The GPU self-paces INSIDE the command buffer, so it cannot outrun the screen.
  K=60 gives 60fps sustained at 1 submit/sec, inside the measured 2/sec cap.
  Cost: each frame slot needs its own [MVP + vertex data] because the VS fetches
  verts at V#+80, so K slots x VERT_BUF_SIZE (~1.2MB) = ~71MB at K=60, with the
  vertex data copied ONCE at init and only the 64-byte MVP rewritten per batch.
  DCB_SIZE must grow (K x ~2k dwords).
  This is what sceGnmInsertWaitFlipDone exists for; batched pre-recorded command
  buffers are standard practice, not a hack.

============================================================
ShadCube4 — package identity
============================================================
  TITLE       = ShadCube4
  TITLE_ID    = SHAD00004
  CONTENT_ID  = IV0000-SHAD00004_00-SHADCUBE40000000   (exactly 36 chars,
                embeds TITLE_ID at 7:16, 16-char free suffix - all valid)
  PAID        = 0x3100000000000001  (game authority, matches the God of War
                eboot; was the OpenOrbis default 0x3800000000000011 = a system
                app / NPXS20103)

  Asset folder on USB is now /data/ShadCube4/ (was /data/CUBETST00/). Drop
  optional model.obj / textures there. Trace still writes /data/trace.log first
  (sandbox-independent); the sandbox fallback is /mnt/sandbox/SHAD00004/.

  Every rule checked against the create-fself docs + ConsoleMods pkg spec:
  CONTENT_ID is 36 chars, its embedded title id equals the TITLE_ID field, and
  the PAID is a valid program-authority value read from a real retail eboot.

CUBETST00 — Makefile-ready source

Drop these into your OpenOrbis Makefile project:

  src/           → copy all files into your project's src/
  sce_sys/icon0.png → your package icon (replace with your own if you want)

Your Makefile compiles src/*.c (just main.c here) and links the real
OpenOrbis SDK. The SDK's crt1.o handles entry + stack alignment, so the
hardware crash from the hand-rolled framework is fixed by using the SDK
proper.

RUNTIME DATA (not part of the package — put on USB/HDD at /data/CUBETST00/):
  texture.bmp or model.bmp     (cube albedo; falls back to built-in logo)
  floor_albedo.bmp             (floor texture)
  floor_normal.bmp             (tangent-space normal map)
  floor_displacement.bmp       (R-channel height; auto-stretched)
  optional: model.obj / mesh.obj / *.stl / *.ply (3D model to view)

All data files are optional — the scene renders with built-in fallbacks.

NID_RESOLVE.H NOTE:
  Cleaned up — removed the uint32_t/uint8_t/int64_t typedefs that would
  conflict with the SDK's <stdint.h>. Now includes <stdint.h> instead.

Verified: main.c compiles clean with FreeBSD target + only defines main()
(no _start conflict with SDK crt1.o).

============================================================
GPU MEMORY PROTECTION FIX (critical for real hardware)
============================================================
gpu_alloc now maps with PROT_CPU_RW | PROT_GPU_RW (0x33) instead of
PROT_CPU_RW (0x03).

Verified against OpenOrbis SDK header orbis/_types/kernel.h:
  VM_PROT_READ            = 0x01
  VM_PROT_WRITE           = 0x02   → CPU_RW = 0x03
  ORBIS_KERNEL_PROT_GPU_READ  = 0x10
  ORBIS_KERNEL_PROT_GPU_WRITE = 0x20  → GPU_RW = 0x30
  combined                = 0x33

Why this was the silent-crash cause:
Every GPU resource (command buffers, shaders, vertex/index buffers,
textures, framebuffers, depth, shadow map) is allocated through this
single gpu_alloc. With CPU-only 0x03, none of them carried GPU page
permissions. On real PS4 the GPU MMU faults the instant the command
processor fetches the command buffer at the first submit -> the app
dies before any flip -> black screen / "crash without showing
anything." shadPS4 reads guest memory through its buffer cache and
ignores GPU page permissions, so it ran there. Classic
emulator-vs-hardware gap.

============================================================
DEFAULT HARDWARE STATE FIX (critical for real hardware)
============================================================
Both DCBs now emit pm4_init_default_hw_state() at the start, before
context_control. This is the sceGnmDrawInitDefaultHardwareState
sequence: ClearContextState (CLEAR_STATE preamble) + the base
InitSequence register-defaults block.

Copied verbatim from shadPS4 source:
  - gnmdriver.cpp        ClearStateSequence (12 dwords)
  - gnmdriver_init.h     InitSequence (base/non-Neo, 0x73+2 dwords)

Why this was a second silent-crash cause:
Every real PS4 draw begins with sceGnmDrawInitDefaultHardwareState,
which puts the scan converter, viewport, clip, VGT, and color-buffer
context registers into a known-good state. The cube-derived draw
skipped it entirely — emitting only CONTEXT_CONTROL then jumping to
draw-specific register writes. On shadPS4 this is fine (the emulator
self-initializes its register tracking). On real PS4, dozens of
context registers hold undefined values → GPU hangs → black screen.

Adds 127 dwords (508 bytes) per DCB — trivial against the 64 KB
command buffer.

Note: the sequence is selected by SDK version on real hardware
(base / 1.75 / 2.00 / 3.50, each with Neo variants). The base
InitSequence embedded here is correct for a base FAT PS4. If you
later target a Neo (PS4 Pro) or a newer SDK version, the matching
InitSequence variant from gnmdriver_init.h would apply.

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

============================================================
FREEZE + CLEAN EXIT + canonical OpenOrbis flip sync
============================================================
Checked the freeze/exit against the OpenOrbis homebrew templates
(samples/_common/graphics.cpp). Findings + fixes:

1) FREEZE (few seconds) — EOP needs int_sel=2 (completion IRQ).
   EVENT_WRITE_EOP had int_sel=0: it wrote the fence (frames rendered) but
   never raised the GPU completion IRQ that retires submissions + drives
   the flip pipeline. Work backed up -> stall -> freeze. shadPS4 retires
   via Vulkan fences so it never showed.
   Fix: EOP data_control 0x20000000 -> 0x22000000 (int_sel=2). Verified vs
   shadPS4 gnmdriver.cpp + pm4_cmds.h.

2) FLIP SYNC — now matches the canonical OpenOrbis FrameWait.
   The OpenOrbis graphics sample submits a flip tagged with frameID, then
   waits until flipStatus.flipArg == frameID (that flip was shown) before
   continuing. Replaced my flipPendingNum poll with this exact pattern:
   SubmitFlip(...,frame) then wait flipArg >= frame. Paces to vsync, no
   buffer-reuse races.

3) CLOSE HANG (~1 min then crash) — clean shutdown added.
   NOTE: the OpenOrbis samples themselves use for(;;) with NO quit handling
   (they rely on the system force-killing them), so there's no canonical
   clean-exit to copy. Ours is better:
     - loop is while(running)
     - poll sceSystemServiceReceiveEvent; eventType==0x10000000 -> exit
     - MANUAL QUIT: hold L1+R1+L2+R2 together -> exit (guaranteed path)
     - on exit: SubmitDone + wait last fence (GPU idle) + drain flips
       (flipPendingNum==0) + sceVideoOutUnregisterBuffers + sceVideoOutClose
   Releases what the OS waits on -> immediate close.

The quit eventType 0x10000000 could not be fully verified from a primary
source; the L1+R1+L2+R2 combo is the guaranteed clean-exit path.

    make

  - No freeze + clean close (menu or L1+R1+L2+R2) -> all good.
  - Freezes -> report seconds-to-freeze.
  - Menu close hangs but combo works -> event constant needs correcting.

All fixes: GPU prot 0x33, hw-state init, 1080p, GPU-resident shaders,
linear tiling, VM=0 PS exports, clean BG VS + sky PS, EOP completion IRQ,
canonical OpenOrbis flip sync, clean shutdown/teardown.

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

===================================================

============================================================
*** THE WALL IS A TIME (~10.6s), NOT A SUBMIT COUNT ***
============================================================
FROM OUR OWN TRACE — ptms is process time in ms, we have logged it all along:
     f=506  ptms=10613  submit=34
     f=507  ptms=11110  submit=495085   <-- WALL
     frame 0 ran at ptms=2183 (2.2s of loading)
  The wall lands at ~10.6s of PROCESS TIME. Every run.
  It also explains the 482 "outlier" with NO invented numbers: a longer load
  means the main loop starts later, so fewer frames fit before the SAME
  wall-clock moment. 482 frames = 8.03s vs 508 = 8.47s; 0.44s apart = load
  variance. I had ptms in every trace since the start and never looked at it.

THE SUBMIT-COUNT MODEL IS ARITHMETICALLY IMPOSSIBLE (God of War, 969 frames):
     sceGnmSubmitAndFlipCommandBuffersForWorkload    969   = 1 flip/frame
     sceGnmSubmitCommandBuffersForWorkload         16296
     sceGnmDingDong                                32593
  -> ~18 GFX submits per frame, ~535/sec at 30fps.
  -> would burn a "512 budget" in 0.96 SECONDS. It runs for hours.
  Our cube does ONE submit/frame (60/sec) and walls at 508.
  A real game submits 9x FASTER and never walls. THERE IS NO SUBMIT BUDGET.

WORKLOAD API — A RED HERRING FROM THE EMULATOR'S OWN LOGGING:
     The eboot we reverse-engineered IS God of War - the same binary the log
     came from. The log shows sceGnmSubmitCommandBuffersForWorkload x16296 and
     ZERO plain calls, which looked like "the game uses Workload and we don't".
     It does not. shadPS4's source (gnmdriver.cpp:2306):
         s32 sceGnmSubmitCommandBuffers(count, ...) {
             return sceGnmSubmitCommandBuffersForWorkload(count, count, ...); }
         s32 sceGnmSubmitAndFlipCommandBuffers(...) {          // :2172
             return sceGnmSubmitAndFlipCommandBuffersForWorkload(count,count,...); }
     The LOG_DEBUG lives in the ForWorkload function, so the log records the
     EMULATOR's internal forwarding, not the game's choice. Real gnm.sprx does
     the same thing (0x11b0 shifts args and tail-jumps into 0xf80), and the
     workload id is never read: every rdi use in 0xf80 is "lea rdi,[rip+..]"
     loading an error string. sceGnmBeginWorkload @0x860 is a stub:
     *out = (id < 0x10); return id >= 0x10.
     God of War calls the PLAIN functions - exactly like we do.

     STATIC CENSUS vs RUNTIME LOG, same binary - every row agrees:
        SubmitCommandBuffers        static 1  runtime 0 (->16296 fwd)
        SubmitAndFlip               static 1  runtime 0 (->969 fwd)
        DingDong                    static 2  runtime 32593 (loops)
        SubmitDone                  static 5  runtime 971
        InsertWaitFlipDone          static 1  runtime 0 (path not taken)
        sceKernelWaitEqueue         static 0  runtime 0
        HideSplashScreen            static 1  runtime 1
        MapComputeQueue             static 2  runtime 2
     The census was right. It was briefly discarded on a misread of an
     emulator's internal call.

THE MISSING CALL — sceSystemServiceHideSplashScreen
     God of War:  line 1233 sceVideoOutOpen
                  line 1242 RegisterBuffers
                  line 1263 FIRST Gnm submit     <- already RENDERING
                  line 3101 HideSplashScreen     <- and only THEN reports ready
     Our cube:    never called. Not once. Not anywhere.

  It is NOT a visual gate - GoW is drawing before it calls it. It is a READY
  SIGNAL: "my first real frame is up, stop treating me as still-launching".
  So "but we can SEE the cube" does not refute it - GoW sees its frames too.
  The splash state is a LIFECYCLE FLAG. We never clear it, and ~10s later
  something acts on a process that never reported ready.

  A startup obligation timing out ~10s after launch is exactly the shape of a
  fixed-process-time wall. Nothing in the GPU path has that shape.

THIS BUILD:
  - calls sceSystemServiceHideSplashScreen() once before the main loop, after
    init + asset load (NID Vo5V8KAwCmk, libSceSystemService - already linked,
    we call sceSystemServiceReceiveEvent from it)
  - traces "HideSplashScreen ret=N"
  - traces "submits before main loop: N" (loading submits now actually
    increment g_submit_count - they never did, so that probe would have read 0)

  make

READ THE LOG:
  HideSplashScreen ret=0   -> accepted. If nonzero the error code itself says
                              what the system wanted.
  then the wall:
     GONE                  -> ROOT CAUSE. We never reported ready.
     still at ptms~10600   -> splash was not it, but the wall is STILL a fixed
                              process time, so the next suspects are the other
                              lifecycle gaps below - not the GPU.

OTHER REAL DIFFERENCES FOUND, NOT YET CHASED:
  sceSystemServiceReceiveEvent   we poll it EVERY frame; GoW never calls it once
                                 (a difference in the OPPOSITE direction)
  sceKernelGetGPI                GoW 970 = per frame; we never call it
  sceSystemServiceGetDisplaySafeAreaInfo  GoW 971 = per frame; we never call it
  scePlayGoGetToDoList/LanguageMask/InstallSpeed  GoW 971 each = per frame
  sceKernelSetPrtAperture        GoW 1 at startup (GPU aperture); we never call
  sceSysmodulePreloadModuleForLibkernel   GoW 1; we never call
  sceCoredumpRegisterCoredumpHandler      GoW 1; we never call
  sceUserServiceGetInitialUser   GoW 7; we only call UserServiceInitialize
  A real game pokes the SYSTEM every single frame. We poke nothing.

CORRECTIONS TO EARLIER CLAIMS IN THIS FILE:
  - "512 total submits" was CIRCULAR: measured 508/507/482, then invented
    loading addends (4/5/30) to reach 512.
  - "Bloodborne count=6676 proves games exceed 512" - that was a shadPS4
    (EMULATOR) log. No PS4 kernel, no limit. It proved nothing.
  - "authority 0x38 = system = GPU throttled" is FALSE: GnmCompositor.elf is
    PAID 0x3800000000000009, same authority, and composites the entire console
    every frame forever.
  - "Workload API ruled out, games never call it" - generalised from one eboot.
  The PAID change (Makefile line 69) is still present but is now a distant
  secondary suspect. The lifecycle/splash gap is the primary one.
