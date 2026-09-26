Post-processing pixel shaders (bloom). Assemble for PS4 (GCN2 / Sea Islands):
  llvm-mc -triple=amdgcn -mcpu=bonaire -show-encoding post_down.s
The dwords go into ps_post_*_binary in src/main.c between the SDK header token
(0xBEEB03FF, n/2-1) and the OrbShdr trailer (length = code bytes, unique hash).
Inputs: s[0:1] = descriptor table (USER_SGPR 2); v2/v3 = POS_X/POS_Y_FLOAT
(SPI_PS_INPUT_ENA 0x302, pixel centres: PA_SU_VTX_CNTL.PIX_CENTER = 1).
