ShadCube4 - PS4 homebrew renderer (OpenOrbis toolchain, hand-written GCN shaders)

A physically based sky over a curved floor, a textured cube with shadows and parallax, bloom and
a lens flare, day and night with a lit moon, and a glass clock mode with internet time.

CONTROLS (OPTIONS shows / hides this list in the app)
  Left stick / right stick   move / look
  L2 / R2                    camera down / up
  D-pad up / down            camera speed, 5 % steps (a press is one step; held: repeats
                             after 0.5 s, 5x as often after 2 s)
  D-pad left / right         move the sun (time of day); held over 2 s: 5x faster
  L1 / R1                    day and night slower / faster
  Square                     freeze / unfreeze day and night
  Cross                      freeze / unfreeze the cube
  Triangle                   reset camera
  L3                         clock source: in-game (grey dot) / console (yellow) / internet (green)
  R3                         the sun follows the clock
  Circle                     clock mode (the glass clock)

TIME SOURCES (src/timesrc.h)
  In-game   the scene's own sun (06:00 at sunrise).
  Console   the PS4 clock with its Time Zone and Daylight Saving settings.
  Internet  SNTP (pool.ntp.org, time.google.com, time.cloudflare.com) every 30 min, slewed in
            between; its own time zone by the connection's IP from ip-api.com (then
            worldtimeapi.org), independent of the console's settings.

BUILD
  Needs the OpenOrbis PS4 toolchain (OO_PS4_TOOLCHAIN) and sce_sys/about/right.sprx (from the
  OpenOrbis samples; not in this project).
    make all    -> eboot.bin and IV0000-SHAD00004_00-SHADCUBE40000000.pkg
  Assets live in assets/ (shipped at /app0/assets/); tools/README.txt rebuilds them.

SHADPS4 OPCODE TEST (main.c OPCODE_TEST; 0 = off)
  1 = V_BFM_B32: two panels bottom right show the cube albedo, each pixel's UV packed into one dword
  and unpacked with v_bfm_b32 masks - 8-bit fields left (control), 16-bit fields right. On the PS4
  both show the texture; with a 4-bit field extract (shadPS4) the right panel is one flat colour.
  2 = V_ALIGNBIT_B32 (left) / V_ALIGNBYTE_B32 (right): each pixel's UV sits in a 64-bit window at bit
  (x + y) & 31 / byte (x + y) & 3 with junk above it, read back with the align op. On the PS4 both show
  the texture; where the shift is 0, an undefined shift by 32 can leak the junk (diagonal stripes).
  3 = V_CVT_PK_U8_F32: a bit grid bottom right, 28 rows x 32 cells, each row one result dword (MSB
  left, white = 1). Rows 0..19 convert 0, 0.5, 1.5, 2.5, 127.4, 127.5, 127.6, 128.5, 254.5, 255, 255.5,
  256, 300, 1000, -0.4, -1, -300, +inf, -inf, NaN; rows 20..27 put 171.0 into 0x11223344 at byte
  select 0, 1, 2, 3, 4, 5, 7, 0xFFFFFFFF, in a labelled panel. Take a PNG screenshot to read it exactly.

TRACE
  /user/data/ShadCube4/ShadCube4 trace.log (the folder is made when missing): startup, loading
  times per step ("load ms:"), time sources and zones ("tz", "tz_net", "tz_http"), and frame
  statistics every 60 frames.

SOURCE MAP
  src/main.c           the renderer: setup, main loop, command buffers - "§" index at the top
  src/pm4.h            PM4 command packets
  src/nid_resolve.h    PS4 system function prototypes
  src/atmosphere.c/.h  the sky model (Bruneton, Hillaire); an -O2 object
  src/loaders.c/.h     model loaders (obj_loader.h, stl_loader.h, ply_loader.h, tangent.h); -O2
  src/dds_loader.h     DDS textures into GPU memory
  src/bgm.c/.h         the looping music thread
  src/loadscreen.h     the loading screen (CPU-drawn)
  src/timesrc.h        time sources: in-game, console, SNTP and the time zone lookup
  src/ui.h, ui_atlas.h the panels: controls list, speeds, the time pill
  src/clock.h, clock_sdf.h, clock_light_norm.h   the glass clock (text, per-frame constants)
  src/logo_texture.h   the built-in logo texture (fallback when a texture is missing)
  shaders/             GCN shaders: *.s sources -> *.h binaries (shaders/README.txt)
  tools/               asset and code generators (tools/README.txt)
