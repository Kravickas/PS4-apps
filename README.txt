============================================================
TEST: SKY_NO_DRAW
============================================================
FENCE_SLOTS 256 result: still exactly 547 (cpf frozen at 547, fence timeouts
every frame). Post-stall fence= values were the CPU seed, not GPU writes.
Fence address innocent; FENCE_SLOTS back to 1.

Trace header: "scene cfg=DRAW_STOP1 BG_SKY_CLEAN" - every hardware run has
been sky-only. The frame is state setup plus ONE draw (pm4_draw_index_auto,
BG_VERTS). SKY_NO_DRAW 1 skips only that packet; all state, shaders,
checkpoints and the fence tail are unchanged. Screen will be black - expected.

    survives past 547 -> the sky draw is implicated
    still stops at 547 -> the draw is innocent; it is state/submit plumbing

Also fixed: MINIMAL_TEST did not compile (1-arg pm4_prepare_flip) and ignored
no_flip. All five configs now compile with 0 errors.

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
