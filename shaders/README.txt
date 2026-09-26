GPU shaders (GCN2 / Sea Islands). Headers only - main.c includes shaders.h;
nothing here is compiled on its own (the Makefile builds src/*.c/.cpp/.s as
CPU code, which is why GCN assembly must not live in src/).

  vs_shader.h        MVP vertex shader (sky, floor, cube/model, stars, post quad)
  vs_shadow.h        same code, own hash, for the shadow pass
  ps_dark.h          sky gradient + sun/moon disc
  ps_floor.h         floor: albedo, normal map, projective shadow
  ps_shader.h        cube / model: texture + Lambert
  ps_shadow.h        shadow pass: light-space depth
  ps_shadow_clear.h  shadow map clear
  ps_stars.h         stars
  ps_blue.h          loading bar
  ps_post_*.h        bloom: down / blur / composite, sources in ps_post_*.s

Only the post shaders have .s sources; the others were hand-encoded and their
headers document the instructions. To rebuild a post shader:
  llvm-mc -triple=amdgcn -mcpu=bonaire -show-encoding ps_post_down.s
then put the dwords between the SDK header token (0xBEEB03FF, n/2-1) and the
OrbShdr trailer (length = code bytes, new unique hash) in ps_post_down.h.
Inputs: s[0:1] = descriptor table (USER_SGPR 2); v2/v3 = POS_X/POS_Y_FLOAT
(SPI_PS_INPUT_ENA 0x302, pixel centres: PA_SU_VTX_CNTL.PIX_CENTER = 1).
