; ps_clock_light: clock mode's internal light (docs/clock_plan.txt), 1/4 resolution (480 x 270 RGBA16F).
; Per pixel (full-res position X = 4 x, Y = 4 y) the sum over the lit rim entries the CPU lists (32 bytes
; each: px, py, tx, ty, w, 0, 0, 0; w = cos (1 - F) x 3 px / (sqrt(2 pi) x the normaliser)) of the beam
; refracted in at that rim point: w exp(-dist / Ls) exp(-q^2 / 2) / sigma, q = across / sigma, sigma =
; S0 + SPREAD max(along, 0), only ahead of it (along > 0). Out: I, I tx, I ty (the light and its mean
; direction x I). Table: [0..1] the entries' address, [2] entries x 32 (bytes, >= 32), [3] 0,
; [4] log2(e) / Ls, [5] log2(e) / 2, [6] S0, [7] SPREAD, [8] 4 (px per texel), [9..13] the slab's
; centre x, y, half w, h, corner radius. Waves whose texels all lie > 40 px outside the slab skip the
; loop (zeros): no visible pixel samples them (the bilinear footprint is 4 px).
s_load_dwordx4 s[4:7], s[0:1], 0x0
s_load_dwordx4 s[8:11], s[0:1], 0x4
s_load_dwordx8 s[24:31], s[0:1], 0x8
s_waitcnt lgkmcnt(0)
v_mul_f32 v4, s24, v2
v_mul_f32 v5, s24, v3
v_mov_b32 v6, 0
v_mov_b32 v7, 0
v_mov_b32 v8, 0
v_subrev_f32 v10, s25, v4
v_and_b32 v10, 0x7fffffff, v10
v_subrev_f32 v10, s27, v10
v_add_f32 v10, s29, v10
v_subrev_f32 v11, s26, v5
v_and_b32 v11, 0x7fffffff, v11
v_subrev_f32 v11, s28, v11
v_add_f32 v11, s29, v11
v_max_f32 v12, 0, v10
v_max_f32 v13, 0, v11
v_mul_f32 v14, v12, v12
v_mac_f32 v14, v13, v13
v_sqrt_f32 v14, v14
v_max_f32 v12, v10, v11
v_min_f32 v12, 0, v12
v_add_f32 v12, v12, v14
v_subrev_f32 v12, s29, v12
v_cmp_gt_f32 vcc, 0x42200000, v12
s_or_b32 s14, vcc_lo, vcc_hi
s_cbranch_scc0 clock_light_out
s_mov_b32 s13, 0
clock_light_loop:
s_load_dwordx8 s[16:23], s[4:5], s13
s_waitcnt lgkmcnt(0)
v_subrev_f32 v10, s16, v4
v_subrev_f32 v11, s17, v5
v_mul_f32 v12, s18, v10
v_mac_f32 v12, s19, v11
v_mul_f32 v13, s19, v10
v_mul_f32 v14, s18, v11
v_sub_f32 v13, v13, v14
v_mul_f32 v14, v10, v10
v_mac_f32 v14, v11, v11
v_sqrt_f32 v14, v14
v_max_f32 v15, 0, v12
v_mul_f32 v15, s11, v15
v_add_f32 v15, s10, v15
v_rcp_f32 v16, v15
v_mul_f32 v13, v13, v16
v_mul_f32 v13, v13, v13
v_mul_f32 v13, s9, v13
v_mac_f32 v13, s8, v14
v_sub_f32 v13, 0, v13
v_exp_f32 v13, v13
v_mul_f32 v13, v13, v16
v_mul_f32 v13, s20, v13
v_cmp_lt_f32 vcc, 0, v12
v_cndmask_b32 v13, 0, v13, vcc
v_add_f32 v6, v6, v13
v_mac_f32 v7, s18, v13
v_mac_f32 v8, s19, v13
s_add_u32 s13, s13, 32
s_cmp_lt_u32 s13, s6
s_cbranch_scc1 clock_light_loop
clock_light_out:
v_mov_b32 v9, 1.0
exp mrt0 v6, v7, v8, v9 done vm
s_endpgm
