; MSAA resolve + aerial perspective (ps_resolve.s): the scene seen through the same atmosphere as the
; sky (Rayleigh + Mie of atmosphere.c, AERIAL_M_PER_UNIT metres per unit). Per pixel: V from the pixel
; centre (x = (px - 960) / 540, y = (540 - py) / 540); the sky's colour at the camera's local horizon in
; V's azimuth, C = sum over the sun and the moon of (R P_R(nu) + M P_M(nu) + S), R / M / S from the
; horizon table (ps_dark's rows 31/32 at the lights' slices, x the sky scale; u = 0.5 - 0.5 cos(phi) as
; ps_dark), nu = Vh.L with Vh the horizon direction: tan(dip) = tilt0 + (tx Vx + tz Vz) / |Vh|. Per
; sample: sky (z >= 1) unchanged; else d from the depth, the point p = cam + d V, its height above the
; curved floor he = py + 0.5 + (px^2 + pz^2) / (2R), and per species (H, E = e^(-hc/H)) the density
; integral I = d (E - e^(-he/H)) / x, x = (he - hc) / H (d E (1 - x/2 + x^2/6) for |x| < 0.01);
; tau = beta_R I_R + beta_M I_M per channel, out = C + (colour - C) e^-tau; the 4-sample average.
; Table: [0] colour T#, [8] depth T#, [16] horizon T# (64 x 6 RGBA16F), [24] S#; [28] F, n far | R tan,
; far | U tan, far - n | cam xyz, hc | beta_R RGB, beta_M | 1/H_R, 1/H_M, H_R, H_M | E_R, E_M, 1/(2R),
; tilt0 | sun hx hz, moon hx hz | tx, tz, 0, 0 | sun L, 0 | moon L, 0.
s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx8 s[12:19], s[0:1], 0x8
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
s_load_dwordx16 s[32:47], s[0:1], 0x1c
s_load_dwordx16 s[48:63], s[0:1], 0x2c
s_load_dwordx8 s[64:71], s[0:1], 0x3c
s_load_dwordx4 s[72:75], s[0:1], 0x44
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
image_load v40, v[4:6], s[12:19] dmask:0x1 unorm
image_load v41, v[12:14], s[12:19] dmask:0x1 unorm
image_load v42, v[16:18], s[12:19] dmask:0x1 unorm
image_load v43, v[20:22], s[12:19] dmask:0x1 unorm
; V = normalize(F + x R tan + y U tan)
v_subrev_f32 v7, 0x44700000, v2
v_mul_f32 v7, 0x3af2b9d6, v7
v_sub_f32 v8, 0x44070000, v3
v_mul_f32 v8, 0x3af2b9d6, v8
v_mov_b32 v9, s32
v_mac_f32 v9, s36, v7
v_mac_f32 v9, s40, v8
v_mov_b32 v10, s33
v_mac_f32 v10, s37, v7
v_mac_f32 v10, s41, v8
v_mov_b32 v11, s34
v_mac_f32 v11, s38, v7
v_mac_f32 v11, s42, v8
v_mul_f32 v15, v9, v9
v_mac_f32 v15, v10, v10
v_mac_f32 v15, v11, v11
v_rsq_f32 v15, v15
v_mul_f32 v9, v9, v15
v_mul_f32 v10, v10, v15
v_mul_f32 v11, v11, v15
; 1 / (V.F): distance per unit of view depth
v_mul_f32 v15, s32, v9
v_mac_f32 v15, s33, v10
v_mac_f32 v15, s34, v11
v_rcp_f32 v15, v15
; 1 / |Vh| (v17) and the local horizon direction Vh = (c Vx / |Vh|, -s, c Vz / |Vh|) in v44..v46
v_mul_f32 v16, v9, v9
v_mac_f32 v16, v11, v11
v_max_f32 v16, 0x2b8cbccc, v16
v_rsq_f32 v17, v16
v_mul_f32 v18, s64, v9
v_mac_f32 v18, s65, v11
v_mul_f32 v18, v18, v17
v_add_f32 v18, s59, v18
v_mul_f32 v19, v18, v18
v_add_f32 v19, 1.0, v19
v_rsq_f32 v19, v19
v_mul_f32 v46, v18, v19
v_sub_f32 v45, 0, v46
v_mul_f32 v19, v19, v17
v_mul_f32 v44, v9, v19
v_mul_f32 v46, v11, v19
v_mov_b32 v47, 0
v_mov_b32 v48, 0
v_mov_b32 v49, 0
; sun: u = 0.5 - 0.5 clamp(cos phi), nu = Vh.L, P_R, P_M; C += R P_R + M P_M + S (rows 0..2)
v_mul_f32 v50, s60, v9
v_mac_f32 v50, s61, v11
v_mul_f32 v50, v50, v17
v_max_f32 v50, -1.0, v50
v_min_f32 v50, 1.0, v50
v_mad_f32 v50, v50, -0.5, 0.5
v_mul_f32 v51, s68, v44
v_mac_f32 v51, s69, v45
v_mac_f32 v51, s70, v46
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_mul_f32 v53, 0x3fcccccd, v51
v_sub_f32 v53, 0x3fd1eb85, v53
v_sqrt_f32 v54, v53
v_mul_f32 v54, v54, v53
v_rcp_f32 v54, v54
v_mul_f32 v54, 0x3c8557c9, v54
v_mul_f32 v54, v54, v52
v_mul_f32 v52, 0x3d747645, v52
v_mov_b32 v51, 0x3daaaaab
image_sample_lz v[56:58], v[50:51], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v51, 0x3e800000
image_sample_lz v[60:62], v[50:51], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v51, 0x3ed55555
image_sample_lz v[64:66], v[50:51], s[20:27], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_mac_f32 v47, v56, v52
v_mac_f32 v47, v60, v54
v_add_f32 v47, v47, v64
v_mac_f32 v48, v57, v52
v_mac_f32 v48, v61, v54
v_add_f32 v48, v48, v65
v_mac_f32 v49, v58, v52
v_mac_f32 v49, v62, v54
v_add_f32 v49, v49, v66
; moon: u = 0.5 - 0.5 clamp(cos phi), nu = Vh.L, P_R, P_M; C += R P_R + M P_M + S (rows 3..5)
v_mul_f32 v50, s62, v9
v_mac_f32 v50, s63, v11
v_mul_f32 v50, v50, v17
v_max_f32 v50, -1.0, v50
v_min_f32 v50, 1.0, v50
v_mad_f32 v50, v50, -0.5, 0.5
v_mul_f32 v51, s72, v44
v_mac_f32 v51, s73, v45
v_mac_f32 v51, s74, v46
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_mul_f32 v53, 0x3fcccccd, v51
v_sub_f32 v53, 0x3fd1eb85, v53
v_sqrt_f32 v54, v53
v_mul_f32 v54, v54, v53
v_rcp_f32 v54, v54
v_mul_f32 v54, 0x3c8557c9, v54
v_mul_f32 v54, v54, v52
v_mul_f32 v52, 0x3d747645, v52
v_mov_b32 v51, 0x3f155555
image_sample_lz v[56:58], v[50:51], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v51, 0x3f400000
image_sample_lz v[60:62], v[50:51], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v51, 0x3f6aaaab
image_sample_lz v[64:66], v[50:51], s[20:27], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_mac_f32 v47, v56, v52
v_mac_f32 v47, v60, v54
v_add_f32 v47, v47, v64
v_mac_f32 v48, v57, v52
v_mac_f32 v48, v61, v54
v_add_f32 v48, v48, v65
v_mac_f32 v49, v58, v52
v_mac_f32 v49, v62, v54
v_add_f32 v49, v49, v66
; hc exponentials per species: s56 = E_R, s57 = E_M; accumulators v67..v69
v_mov_b32 v67, 0
v_mov_b32 v68, 0
v_mov_b32 v69, 0
; sample 0: d, p, he, I_R, I_M, T per channel
v_mul_f32 v50, s43, v40
v_sub_f32 v50, s39, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s35, v50
v_mul_f32 v50, v50, v15
v_mov_b32 v51, s44
v_mac_f32 v51, v50, v9
v_mov_b32 v52, s45
v_mac_f32 v52, v50, v10
v_mov_b32 v53, s46
v_mac_f32 v53, v50, v11
v_mul_f32 v51, v51, v51
v_mac_f32 v51, v53, v53
v_mul_f32 v51, s58, v51
v_add_f32 v51, v51, v52
v_add_f32 v51, 0.5, v51
v_subrev_f32 v52, s47, v51
v_mul_f32 v53, s52, v52
v_mul_f32 v56, s52, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s56, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s56, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v54, v56, v50
v_mul_f32 v53, s53, v52
v_mul_f32 v56, s53, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s57, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s57, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v55, v56, v50
v_cmp_le_f32 vcc, 1.0, v40
v_mul_f32 v56, s48, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v24, v47
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v47
v_cndmask_b32 v57, v57, v24, vcc
v_add_f32 v67, v67, v57
v_mul_f32 v56, s49, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v25, v48
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v48
v_cndmask_b32 v57, v57, v25, vcc
v_add_f32 v68, v68, v57
v_mul_f32 v56, s50, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v26, v49
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v49
v_cndmask_b32 v57, v57, v26, vcc
v_add_f32 v69, v69, v57
; sample 1: d, p, he, I_R, I_M, T per channel
v_mul_f32 v50, s43, v41
v_sub_f32 v50, s39, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s35, v50
v_mul_f32 v50, v50, v15
v_mov_b32 v51, s44
v_mac_f32 v51, v50, v9
v_mov_b32 v52, s45
v_mac_f32 v52, v50, v10
v_mov_b32 v53, s46
v_mac_f32 v53, v50, v11
v_mul_f32 v51, v51, v51
v_mac_f32 v51, v53, v53
v_mul_f32 v51, s58, v51
v_add_f32 v51, v51, v52
v_add_f32 v51, 0.5, v51
v_subrev_f32 v52, s47, v51
v_mul_f32 v53, s52, v52
v_mul_f32 v56, s52, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s56, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s56, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v54, v56, v50
v_mul_f32 v53, s53, v52
v_mul_f32 v56, s53, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s57, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s57, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v55, v56, v50
v_cmp_le_f32 vcc, 1.0, v41
v_mul_f32 v56, s48, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v28, v47
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v47
v_cndmask_b32 v57, v57, v28, vcc
v_add_f32 v67, v67, v57
v_mul_f32 v56, s49, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v29, v48
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v48
v_cndmask_b32 v57, v57, v29, vcc
v_add_f32 v68, v68, v57
v_mul_f32 v56, s50, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v30, v49
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v49
v_cndmask_b32 v57, v57, v30, vcc
v_add_f32 v69, v69, v57
; sample 2: d, p, he, I_R, I_M, T per channel
v_mul_f32 v50, s43, v42
v_sub_f32 v50, s39, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s35, v50
v_mul_f32 v50, v50, v15
v_mov_b32 v51, s44
v_mac_f32 v51, v50, v9
v_mov_b32 v52, s45
v_mac_f32 v52, v50, v10
v_mov_b32 v53, s46
v_mac_f32 v53, v50, v11
v_mul_f32 v51, v51, v51
v_mac_f32 v51, v53, v53
v_mul_f32 v51, s58, v51
v_add_f32 v51, v51, v52
v_add_f32 v51, 0.5, v51
v_subrev_f32 v52, s47, v51
v_mul_f32 v53, s52, v52
v_mul_f32 v56, s52, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s56, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s56, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v54, v56, v50
v_mul_f32 v53, s53, v52
v_mul_f32 v56, s53, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s57, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s57, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v55, v56, v50
v_cmp_le_f32 vcc, 1.0, v42
v_mul_f32 v56, s48, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v32, v47
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v47
v_cndmask_b32 v57, v57, v32, vcc
v_add_f32 v67, v67, v57
v_mul_f32 v56, s49, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v33, v48
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v48
v_cndmask_b32 v57, v57, v33, vcc
v_add_f32 v68, v68, v57
v_mul_f32 v56, s50, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v34, v49
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v49
v_cndmask_b32 v57, v57, v34, vcc
v_add_f32 v69, v69, v57
; sample 3: d, p, he, I_R, I_M, T per channel
v_mul_f32 v50, s43, v43
v_sub_f32 v50, s39, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s35, v50
v_mul_f32 v50, v50, v15
v_mov_b32 v51, s44
v_mac_f32 v51, v50, v9
v_mov_b32 v52, s45
v_mac_f32 v52, v50, v10
v_mov_b32 v53, s46
v_mac_f32 v53, v50, v11
v_mul_f32 v51, v51, v51
v_mac_f32 v51, v53, v53
v_mul_f32 v51, s58, v51
v_add_f32 v51, v51, v52
v_add_f32 v51, 0.5, v51
v_subrev_f32 v52, s47, v51
v_mul_f32 v53, s52, v52
v_mul_f32 v56, s52, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s56, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s56, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v54, v56, v50
v_mul_f32 v53, s53, v52
v_mul_f32 v56, s53, v51
v_mul_f32 v56, 0xbfb8aa3b, v56
v_min_f32 v56, 0x42e6d4ca, v56
v_exp_f32 v56, v56
v_sub_f32 v56, s57, v56
v_rcp_f32 v57, v53
v_mul_f32 v56, v56, v57
v_mul_f32 v57, 0x3e2aaaab, v53
v_add_f32 v57, -0.5, v57
v_mul_f32 v57, v57, v53
v_add_f32 v57, 1.0, v57
v_mul_f32 v57, s57, v57
v_and_b32 v58, 0x7fffffff, v53
v_cmp_gt_f32 vcc, 0x3c23d70a, v58
v_cndmask_b32 v56, v56, v57, vcc
v_mul_f32 v55, v56, v50
v_cmp_le_f32 vcc, 1.0, v43
v_mul_f32 v56, s48, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v36, v47
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v47
v_cndmask_b32 v57, v57, v36, vcc
v_add_f32 v67, v67, v57
v_mul_f32 v56, s49, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v37, v48
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v48
v_cndmask_b32 v57, v57, v37, vcc
v_add_f32 v68, v68, v57
v_mul_f32 v56, s50, v54
v_mac_f32 v56, s51, v55
v_mul_f32 v56, 0xbfb8aa3b, v56
v_exp_f32 v56, v56
v_sub_f32 v57, v38, v49
v_mul_f32 v57, v57, v56
v_add_f32 v57, v57, v49
v_cndmask_b32 v57, v57, v38, vcc
v_add_f32 v69, v69, v57
v_mul_f32 v67, 0x3e800000, v67
v_mul_f32 v68, 0x3e800000, v68
v_mul_f32 v69, 0x3e800000, v69
v_mov_b32 v70, 1.0
exp mrt0 v67, v68, v69, v70 done vm
s_endpgm
