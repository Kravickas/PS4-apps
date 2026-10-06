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

TRACE
  /user/data/ShadCube4/ShadCube4 trace.log (the folder is made when missing): startup, loading
  times per step ("load ms:"), time sources and zones ("tz", "tz_net", "tz_http"), and frame
  statistics every 60 frames.
  ORDCNT_TEST (src/main.c, on): ShadCube4 ordcnt.log in the trace log's folder, the
  DS_ORDERED_COUNT test, run before the main loop. After a GPU hang close the app and launch it
  again: it logs the hung test and continues (state: ordcnt.state there). Once finished it does
  not run again until ordcnt.state is deleted.

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
  src/ordcnt_test.h    DS_ORDERED_COUNT hardware test (ORDCNT_TEST), log: ShadCube4 ordcnt.log
  src/ui.h, ui_atlas.h the panels: controls list, speeds, the time pill
  src/clock.h, clock_sdf.h, clock_light_norm.h   the glass clock (text, per-frame constants)
  src/logo_texture.h   the built-in logo texture (fallback when a texture is missing)
  shaders/             GCN shaders: *.s sources -> *.h binaries (shaders/README.txt)
  tools/               asset and code generators (tools/README.txt)
