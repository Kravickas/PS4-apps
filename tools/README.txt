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
  make_icon.py       sce_sys/icon0.png: the ShadPS4 badge on a turned cube (transparent)

DDS rules (src/dds_loader.h): DX10 header, BC1_UNORM_SRGB / BC4_UNORM / BC5_UNORM
/ R8G8B8A8_UNORM_SRGB, power-of-two, mips down to 32 px, rows bottom-up (row 0 =
v 0). Files stay standard (linear); the loader tiles BC levels for the PS4
(Thin_1dThin) and reads RGBA8 in place (LINEAR_ALIGNED). Normal maps
are stored in the engine convention (+u, +v), measured against the height map.

The PKG also needs sce_sys/about/right.sprx (from the OpenOrbis samples; not in
this project).

logo_2048.png: the cube prop's logo (badge + wordmark in the logo's font), the
input of gen_model.py for the albedo, the engraving height and the normal map.
