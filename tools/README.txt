Asset pipeline (Python 3 with numpy, Pillow, quicktex; ffmpeg for the music):
  pip install numpy pillow quicktex

  python3 tools/build_assets.py --floor <DIR> --music <FILE>
      Rebuilds assets/ (shipped in the PKG at /app0/assets/):
        models/cube/cube.obj                   gen_model.py (rounded logo cube)
        images/cube/albedo|normal|height.dds   logo RGBA8 sRGB, BC5, BC4
        images/floor/albedo|normal|height.dds  BC1 sRGB, BC5, BC4
        sound/bgm/bgm.wav                      make_bgm.py
      DIR holds floor_albedo.*, floor_normal.*, floor_displacement.* (any image
      format Pillow reads, power-of-two sizes >= 32); FILE any audio ffmpeg reads.

  make_textures.py   one texture -> DDS (albedo / normal / height); prints quality
  gen_model.py       the prop (OBJ + logo albedo / height / normal sources)
  make_bgm.py        audio -> 16-bit stereo 48 kHz WAV with a crossfaded loop seam
  make_gp4.py        PKG project for PkgTool.Core; called by the Makefile
  make_icon.py       sce_sys/icon0.png: the in-game rounded cube (cube.obj) with the blue
                     badge (badge_1024.png) on each face, satin; no arguments needed
  make_glare.py      assets/images/flare/glare.dds: the lens flare's rays - unused (FLARE_RAYS 0,
                     the texture is not shipped; without it the flare samples black)
  make_moon.py       assets/images/moon/albedo.dds: NASA's LROC colour map warped to the
                     near side, for ps_dark's moon disc
  make_atmosphere.c  assets/sky/atmosphere.bin: the sky tables (host C with src/atmosphere.c)
  make_clock_sdf.py FONT     assets/ui/clock_sdf.bin + src/clock_sdf.h: the clock's glyph
                     distance atlas (URW Gothic Demi; not shipped)
  make_clock_light_norm.py   src/clock_light_norm.h: the clock's internal-light normaliser
  gen_ps_clock.py    shaders/ps_clock.s (clock mode's glass)

DDS rules (src/dds_loader.h): DX10 header, BC1_UNORM_SRGB / BC4_UNORM / BC5_UNORM
/ R8G8B8A8_UNORM_SRGB, power-of-two, mips down to 32 px, rows bottom-up (row 0 =
v 0). Files stay standard (linear); the loader tiles BC levels for the PS4
(Thin_1dThin) and reads RGBA8 in place (LINEAR_ALIGNED). Normal maps
are stored in the engine convention (+u, +v), measured against the height map.

The PKG also needs sce_sys/about/right.sprx (from the OpenOrbis samples; not in
this project).

logo_2048.png: the cube prop's logo (badge + wordmark in the logo's font), the
input of gen_model.py for the albedo, the engraving height and the normal map.
badge_1024.png: the ShadPS4 badge rebuilt at 1024 px from the original logo artwork
(shad.png), used by make_icon.py.

make_ui_atlas.py FONT: assets/ui/ui_atlas.bin + src/ui_atlas.h - glyphs of the logo's font
(URW Gothic Demi, 28 px) and the DS4 button icons in ui_icons/ (from a DS4 icon
pack), used by the on-screen panels (src/ui.h). The font is not shipped; pass its path.
