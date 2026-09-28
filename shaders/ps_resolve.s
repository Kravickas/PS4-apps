; MSAA resolve (ps_resolve.s): the 4-sample scene (RGBA16F, tile index 13, no FMASK: fragment = sample)
; averaged per pixel into the single-sample HDR target. desc: MSAA T# [0] (TYPE 2D_MSAA, LAST_LEVEL =
; log2 samples = 2). v2, v3 = pixel centre (POS_X/Y_FLOAT) -> integer x, y for image_load (x, y, sample).
s_load_dwordx8 s[4:11], s[0:1], 0x0
s_waitcnt lgkmcnt(0)
v_cvt_u32_f32 v4, v2
v_cvt_u32_f32 v5, v3
v_mov_b32 v6, 0
v_mov_b32 v12, v4
v_mov_b32 v13, v5
v_mov_b32 v14, 1
v_mov_b32 v16, v4
v_mov_b32 v17, v5
v_mov_b32 v18, 2
v_mov_b32 v20, v4
v_mov_b32 v21, v5
v_mov_b32 v22, 3
image_load v[24:26], v[4:6], s[4:11] dmask:0x7 unorm
image_load v[28:30], v[12:14], s[4:11] dmask:0x7 unorm
image_load v[32:34], v[16:18], s[4:11] dmask:0x7 unorm
image_load v[36:38], v[20:22], s[4:11] dmask:0x7 unorm
s_waitcnt vmcnt(0)
v_add_f32 v24, v24, v28
v_add_f32 v25, v25, v29
v_add_f32 v26, v26, v30
v_add_f32 v32, v32, v36
v_add_f32 v33, v33, v37
v_add_f32 v34, v34, v38
v_add_f32 v24, v24, v32
v_add_f32 v25, v25, v33
v_add_f32 v26, v26, v34
v_mul_f32 v24, 0x3e800000, v24
v_mul_f32 v25, 0x3e800000, v25
v_mul_f32 v26, 0x3e800000, v26
v_mov_b32 v27, 1.0
exp mrt0 v24, v25, v26, v27 done vm
s_endpgm
