; MSAA resolve with physically based height fog (ps_resolve.s). Per pixel: V = normalize(F + x R/fov
; + y U/fov) from the pixel centre (x = (px - 960) / 540, y = (540 - py) / 540), the in-scattered colour
; C = FS P(V.L) + FA (P: Cornette-Shanks g 0.8; FS = the scene light x pi x the haze albedo, FA = the
; mean sky and ground light, isotropic). Per sample: view depth = n far / (far - (far - n) z), d = depth
; / (V.F) (sky, z >= 1: d = 1e6), optical depth of the density sigma0 exp(-(y - y0) / H) along the ray:
; tau = sigma0 H (e^(-hc/H) - e^(-he/H)) / Vy (hc, he: heights of the ends; the exponent clamped at 80,
; never 0 x inf), or A d (1 - x/2 + x^2/6), A = sigma0 e^(-hc/H), for |x| = |d Vy / H| < 0.01; out =
; colour e^-tau + (1 - e^-tau) C; the
; average of the four samples. Table: [0] colour MSAA T#, [8] depth MSAA T# (Z_32_FLOAT, Depth1DThin),
; [16..31] F, 1/H | R/fov, A | U/fov, n far | L, far; [32..47] FS, far - n | FA, sigma0 H | e^(-hc/H),
; -hc/H, 0, 0 | 0.
s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx8 s[12:19], s[0:1], 0x8
s_load_dwordx16 s[20:35], s[0:1], 0x10
s_load_dwordx16 s[36:51], s[0:1], 0x20
s_waitcnt lgkmcnt(0)
v_cvt_u32_f32 v4, v2
v_cvt_u32_f32 v5, v3
; four (x, y, sample) address triples: colour and depth use the same ones
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
; view direction (loads in flight): x = (px - 960) / 540, y = (540 - py) / 540
v_subrev_f32 v7, 0x44700000, v2
v_mul_f32 v7, 0x3af2b9d6, v7
v_sub_f32 v8, 0x44070000, v3
v_mul_f32 v8, 0x3af2b9d6, v8
v_mov_b32 v9, s20
v_mac_f32 v9, s24, v7
v_mac_f32 v9, s28, v8
v_mov_b32 v10, s21
v_mac_f32 v10, s25, v7
v_mac_f32 v10, s29, v8
v_mov_b32 v11, s22
v_mac_f32 v11, s26, v7
v_mac_f32 v11, s30, v8
v_mul_f32 v15, v9, v9
v_mac_f32 v15, v10, v10
v_mac_f32 v15, v11, v11
v_rsq_f32 v15, v15
v_mul_f32 v9, v9, v15
v_mul_f32 v10, v10, v15
v_mul_f32 v11, v11, v15
; 1 / (V.F): distance per unit view depth
v_mul_f32 v15, s20, v9
v_mac_f32 v15, s21, v10
v_mac_f32 v15, s22, v11
v_rcp_f32 v15, v15
; C = FS P(nu) + FA, nu = V.L
v_mul_f32 v19, s32, v9
v_mac_f32 v19, s33, v10
v_mac_f32 v19, s34, v11
v_mul_f32 v23, v19, v19
v_add_f32 v23, 1.0, v23
v_mul_f32 v19, 0x3fcccccd, v19
v_sub_f32 v19, 0x3fd1eb85, v19
v_sqrt_f32 v27, v19
v_mul_f32 v27, v27, v19
v_rcp_f32 v27, v27
v_mul_f32 v23, 0x3c8557c9, v23
v_mul_f32 v23, v23, v27
v_mov_b32 v44, s40
v_mac_f32 v44, s36, v23
v_mov_b32 v45, s41
v_mac_f32 v45, s37, v23
v_mov_b32 v46, s42
v_mac_f32 v46, s38, v23
; Vy / H and 1 / Vy
v_mul_f32 v19, s23, v10
v_rcp_f32 v55, v10
v_mov_b32 v47, 0
v_mov_b32 v48, 0
v_mov_b32 v49, 0
s_waitcnt vmcnt(0)
; sample 0: d, tau, T
v_mul_f32 v50, s39, v40
v_sub_f32 v50, s35, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s31, v50
v_mul_f32 v50, v50, v15
v_cmp_le_f32 vcc, 1.0, v40
v_mov_b32 v51, 0x49742400
v_cndmask_b32 v50, v50, v51, vcc
v_mul_f32 v51, v50, v19
v_sub_f32 v52, s45, v51
v_min_f32 v52, 0x42a00000, v52
v_mul_f32 v52, 0x3fb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v52, s44, v52
v_mul_f32 v52, s43, v52
v_mul_f32 v52, v52, v55
v_mul_f32 v53, 0x3e2aaaab, v51
v_add_f32 v53, -0.5, v53
v_mul_f32 v53, v53, v51
v_add_f32 v53, 1.0, v53
v_mul_f32 v53, v53, v50
v_mul_f32 v53, s27, v53
v_and_b32 v54, 0x7fffffff, v51
v_cmp_gt_f32 vcc, 0x3c23d70a, v54
v_cndmask_b32 v52, v52, v53, vcc
v_mul_f32 v52, 0xbfb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v53, v24, v44
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v44
v_add_f32 v47, v47, v53
v_sub_f32 v53, v25, v45
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v45
v_add_f32 v48, v48, v53
v_sub_f32 v53, v26, v46
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v46
v_add_f32 v49, v49, v53
; sample 1: d, tau, T
v_mul_f32 v50, s39, v41
v_sub_f32 v50, s35, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s31, v50
v_mul_f32 v50, v50, v15
v_cmp_le_f32 vcc, 1.0, v41
v_mov_b32 v51, 0x49742400
v_cndmask_b32 v50, v50, v51, vcc
v_mul_f32 v51, v50, v19
v_sub_f32 v52, s45, v51
v_min_f32 v52, 0x42a00000, v52
v_mul_f32 v52, 0x3fb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v52, s44, v52
v_mul_f32 v52, s43, v52
v_mul_f32 v52, v52, v55
v_mul_f32 v53, 0x3e2aaaab, v51
v_add_f32 v53, -0.5, v53
v_mul_f32 v53, v53, v51
v_add_f32 v53, 1.0, v53
v_mul_f32 v53, v53, v50
v_mul_f32 v53, s27, v53
v_and_b32 v54, 0x7fffffff, v51
v_cmp_gt_f32 vcc, 0x3c23d70a, v54
v_cndmask_b32 v52, v52, v53, vcc
v_mul_f32 v52, 0xbfb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v53, v28, v44
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v44
v_add_f32 v47, v47, v53
v_sub_f32 v53, v29, v45
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v45
v_add_f32 v48, v48, v53
v_sub_f32 v53, v30, v46
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v46
v_add_f32 v49, v49, v53
; sample 2: d, tau, T
v_mul_f32 v50, s39, v42
v_sub_f32 v50, s35, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s31, v50
v_mul_f32 v50, v50, v15
v_cmp_le_f32 vcc, 1.0, v42
v_mov_b32 v51, 0x49742400
v_cndmask_b32 v50, v50, v51, vcc
v_mul_f32 v51, v50, v19
v_sub_f32 v52, s45, v51
v_min_f32 v52, 0x42a00000, v52
v_mul_f32 v52, 0x3fb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v52, s44, v52
v_mul_f32 v52, s43, v52
v_mul_f32 v52, v52, v55
v_mul_f32 v53, 0x3e2aaaab, v51
v_add_f32 v53, -0.5, v53
v_mul_f32 v53, v53, v51
v_add_f32 v53, 1.0, v53
v_mul_f32 v53, v53, v50
v_mul_f32 v53, s27, v53
v_and_b32 v54, 0x7fffffff, v51
v_cmp_gt_f32 vcc, 0x3c23d70a, v54
v_cndmask_b32 v52, v52, v53, vcc
v_mul_f32 v52, 0xbfb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v53, v32, v44
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v44
v_add_f32 v47, v47, v53
v_sub_f32 v53, v33, v45
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v45
v_add_f32 v48, v48, v53
v_sub_f32 v53, v34, v46
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v46
v_add_f32 v49, v49, v53
; sample 3: d, tau, T
v_mul_f32 v50, s39, v43
v_sub_f32 v50, s35, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s31, v50
v_mul_f32 v50, v50, v15
v_cmp_le_f32 vcc, 1.0, v43
v_mov_b32 v51, 0x49742400
v_cndmask_b32 v50, v50, v51, vcc
v_mul_f32 v51, v50, v19
v_sub_f32 v52, s45, v51
v_min_f32 v52, 0x42a00000, v52
v_mul_f32 v52, 0x3fb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v52, s44, v52
v_mul_f32 v52, s43, v52
v_mul_f32 v52, v52, v55
v_mul_f32 v53, 0x3e2aaaab, v51
v_add_f32 v53, -0.5, v53
v_mul_f32 v53, v53, v51
v_add_f32 v53, 1.0, v53
v_mul_f32 v53, v53, v50
v_mul_f32 v53, s27, v53
v_and_b32 v54, 0x7fffffff, v51
v_cmp_gt_f32 vcc, 0x3c23d70a, v54
v_cndmask_b32 v52, v52, v53, vcc
v_mul_f32 v52, 0xbfb8aa3b, v52
v_exp_f32 v52, v52
v_sub_f32 v53, v36, v44
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v44
v_add_f32 v47, v47, v53
v_sub_f32 v53, v37, v45
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v45
v_add_f32 v48, v48, v53
v_sub_f32 v53, v38, v46
v_mul_f32 v53, v53, v52
v_add_f32 v53, v53, v46
v_add_f32 v49, v49, v53
v_mul_f32 v47, 0x3e800000, v47
v_mul_f32 v48, 0x3e800000, v48
v_mul_f32 v49, 0x3e800000, v49
v_mov_b32 v50, 1.0
exp mrt0 v47, v48, v49, v50 done vm
s_endpgm
