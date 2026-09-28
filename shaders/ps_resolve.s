; MSAA resolve + aerial perspective (ps_resolve.s): the scene seen through the same atmosphere as the
; sky (Rayleigh + Mie of atmosphere.c, AERIAL_M_PER_UNIT metres per unit). Per pixel: V from the pixel
; centre (x = (px - 960) / 540, y = (540 - py) / 540); the in-scattered colour C = the sky's colour at
; the horizon in V's azimuth, lit as the air over the pixel's surface point is: the nearest sample's
; point p, its height hp above the curved floor, per light y' = L height above the horizon seen from p
; (tan = a0 + p.h / R, atmo_tilted_y), slice x = asin(y') 90/pi + 10 (A&S 4.4.46; the
; sun-lit side of the globe gets dawn / day colours, the far side the planet's shadow); C = sum over
; the sun and the moon of scale (R P_R(nu) + M P_M(nu) + S), R / M / S the static horizon texture's
; rows t x 56 + k0 and + 1 blended by x - k0, u = 0.5 - 0.5 cos(phi), nu = Vh.L at the camera's
; horizon direction (tan(dip) = tilt0 + (tx Vx + tz Vz) / |Vh|). Per sample: sky (z >= 1) unchanged;
; else d from the depth, p = cam + d V, he above the floor, per species I = d (E - e^(-he/H)) / x,
; x = (he - hc) / H (d E (1 - x/2 + x^2/6) for |x| < 0.01); tau = beta_R I_R + beta_M I_M per
; channel, out = C + (colour - C) e^-tau; the 4-sample average. Table: [0] colour T#, [8] depth T#,
; [16] horizon T# (64 x 168 RGBA16F), [24] S#; [28] F, n far | R tan, far | U tan, far - n | cam, hc
; | beta_R RGB, beta_M | 1/H_R, 1/H_M, H_R, H_M | E_R, E_M, 1/(2R), tilt0 | sun hx hz, moon hx hz |
; tx, tz, sun scale, moon scale | sun L, 1/R | moon L, sqrt(2 / (R hc)).
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
; 1 / |Vh| (v17) and the camera's horizon direction Vh = (c Vx / |Vh|, -s, c Vz / |Vh|) in v44..v46
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
; the pixel's surface point: the nearest sample (min depth), p = cam + d V; hp above the floor; its dip
; a0 = sqrt(2 / (R hc)) hp up to the camera's height hc (as ps_dark), sqrt(2 hp / R) above it
s_waitcnt vmcnt(0)
v_min_f32 v50, v40, v41
v_min_f32 v50, v50, v42
v_min_f32 v50, v50, v43
v_mul_f32 v50, s43, v50
v_sub_f32 v50, s39, v50
v_rcp_f32 v50, v50
v_mul_f32 v50, s35, v50
v_mul_f32 v50, v50, v15
v_mov_b32 v92, s44
v_mac_f32 v92, v50, v9
v_mov_b32 v51, s45
v_mac_f32 v51, v50, v10
v_mov_b32 v93, s46
v_mac_f32 v93, v50, v11
v_mul_f32 v52, v92, v92
v_mac_f32 v52, v93, v93
v_mul_f32 v52, s58, v52
v_add_f32 v52, v52, v51
v_add_f32 v52, 0.5, v52
v_max_f32 v52, 0, v52
v_mul_f32 v94, s75, v52
v_mul_f32 v95, s71, v52
v_add_f32 v95, v95, v95
v_sqrt_f32 v95, v95
v_cmp_lt_f32 vcc, s47, v52
v_cndmask_b32 v94, v94, v95, vcc
; sun: y' at p -> slice (k0 v53, f v54); u; nu = Vh.L, P_R (v52), P_M (v55); C += scale (R P_R + M P_M + S)
v_mul_f32 v51, s60, v92
v_mac_f32 v51, s61, v93
v_mul_f32 v51, s71, v51
v_add_f32 v51, v94, v51
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_rsq_f32 v52, v52
v_mul_f32 v51, v51, v52
v_mov_b32 v53, s60
v_mul_f32 v53, s68, v53
v_mov_b32 v54, s61
v_mac_f32 v53, s70, v54
v_mul_f32 v53, v53, v51
v_mac_f32 v53, s69, v52
v_and_b32 v54, 0x7fffffff, v53
v_min_f32 v54, 1.0, v54
v_mov_b32 v55, 0xbaa57a2c
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3bda90c5, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbc8bfc66, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3cfd10f8, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbd4d8392, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3db63a9e, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbe5bbfca, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3fc90fda, v55
v_sub_f32 v52, 1.0, v54
v_sqrt_f32 v52, v52
v_mul_f32 v55, v55, v52
v_sub_f32 v55, 0x3fc90fdb, v55
v_cmp_gt_f32 vcc, 0, v53
v_sub_f32 v52, 0, v55
v_cndmask_b32 v55, v55, v52, vcc
v_mul_f32 v55, 0x41e52ee1, v55
v_add_f32 v55, 0x41200000, v55
v_max_f32 v55, 0, v55
v_min_f32 v55, 0x425c0000, v55
v_floor_f32 v53, v55
v_min_f32 v53, 0x42580000, v53
v_sub_f32 v54, v55, v53
v_mul_f32 v50, s60, v9
v_mac_f32 v50, s61, v11
v_mul_f32 v50, v50, v17
v_max_f32 v50, -1.0, v50
v_min_f32 v50, 1.0, v50
v_mad_f32 v50, v50, -0.5, 0.5
v_mov_b32 v71, v50
v_add_f32 v72, 0x3f000000, v53
v_mul_f32 v72, 0x3bc30c31, v72
image_sample_lz v[56:58], v[71:72], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v73, v50
v_add_f32 v74, 0x3fc00000, v53
v_mul_f32 v74, 0x3bc30c31, v74
image_sample_lz v[60:62], v[73:74], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v75, v50
v_add_f32 v76, 0x42620000, v53
v_mul_f32 v76, 0x3bc30c31, v76
image_sample_lz v[64:66], v[75:76], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v77, v50
v_add_f32 v78, 0x42660000, v53
v_mul_f32 v78, 0x3bc30c31, v78
image_sample_lz v[83:85], v[77:78], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v79, v50
v_add_f32 v80, 0x42e10000, v53
v_mul_f32 v80, 0x3bc30c31, v80
image_sample_lz v[86:88], v[79:80], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v81, v50
v_add_f32 v82, 0x42e30000, v53
v_mul_f32 v82, 0x3bc30c31, v82
image_sample_lz v[89:91], v[81:82], s[20:27], s[28:31] dmask:0x7
v_mul_f32 v51, s68, v44
v_mac_f32 v51, s69, v45
v_mac_f32 v51, s70, v46
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_mul_f32 v53, 0x3fcccccd, v51
v_sub_f32 v53, 0x3fd1eb85, v53
v_sqrt_f32 v55, v53
v_mul_f32 v55, v55, v53
v_rcp_f32 v55, v55
v_mul_f32 v55, 0x3c8557c9, v55
v_mul_f32 v55, v55, v52
v_mul_f32 v52, 0x3d747645, v52
v_mul_f32 v52, s66, v52
v_mul_f32 v55, s66, v55
s_waitcnt vmcnt(0)
v_sub_f32 v60, v60, v56
v_mac_f32 v56, v54, v60
v_sub_f32 v83, v83, v64
v_mac_f32 v64, v54, v83
v_sub_f32 v89, v89, v86
v_mac_f32 v86, v54, v89
v_mac_f32 v47, v56, v52
v_mac_f32 v47, v64, v55
v_mac_f32 v47, s66, v86
v_sub_f32 v61, v61, v57
v_mac_f32 v57, v54, v61
v_sub_f32 v84, v84, v65
v_mac_f32 v65, v54, v84
v_sub_f32 v90, v90, v87
v_mac_f32 v87, v54, v90
v_mac_f32 v48, v57, v52
v_mac_f32 v48, v65, v55
v_mac_f32 v48, s66, v87
v_sub_f32 v62, v62, v58
v_mac_f32 v58, v54, v62
v_sub_f32 v85, v85, v66
v_mac_f32 v66, v54, v85
v_sub_f32 v91, v91, v88
v_mac_f32 v88, v54, v91
v_mac_f32 v49, v58, v52
v_mac_f32 v49, v66, v55
v_mac_f32 v49, s66, v88
; moon: y' at p -> slice (k0 v53, f v54); u; nu = Vh.L, P_R (v52), P_M (v55); C += scale (R P_R + M P_M + S)
v_mul_f32 v51, s62, v92
v_mac_f32 v51, s63, v93
v_mul_f32 v51, s71, v51
v_add_f32 v51, v94, v51
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_rsq_f32 v52, v52
v_mul_f32 v51, v51, v52
v_mov_b32 v53, s62
v_mul_f32 v53, s72, v53
v_mov_b32 v54, s63
v_mac_f32 v53, s74, v54
v_mul_f32 v53, v53, v51
v_mac_f32 v53, s73, v52
v_and_b32 v54, 0x7fffffff, v53
v_min_f32 v54, 1.0, v54
v_mov_b32 v55, 0xbaa57a2c
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3bda90c5, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbc8bfc66, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3cfd10f8, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbd4d8392, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3db63a9e, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0xbe5bbfca, v55
v_mul_f32 v55, v55, v54
v_add_f32 v55, 0x3fc90fda, v55
v_sub_f32 v52, 1.0, v54
v_sqrt_f32 v52, v52
v_mul_f32 v55, v55, v52
v_sub_f32 v55, 0x3fc90fdb, v55
v_cmp_gt_f32 vcc, 0, v53
v_sub_f32 v52, 0, v55
v_cndmask_b32 v55, v55, v52, vcc
v_mul_f32 v55, 0x41e52ee1, v55
v_add_f32 v55, 0x41200000, v55
v_max_f32 v55, 0, v55
v_min_f32 v55, 0x425c0000, v55
v_floor_f32 v53, v55
v_min_f32 v53, 0x42580000, v53
v_sub_f32 v54, v55, v53
v_mul_f32 v50, s62, v9
v_mac_f32 v50, s63, v11
v_mul_f32 v50, v50, v17
v_max_f32 v50, -1.0, v50
v_min_f32 v50, 1.0, v50
v_mad_f32 v50, v50, -0.5, 0.5
v_mov_b32 v71, v50
v_add_f32 v72, 0x3f000000, v53
v_mul_f32 v72, 0x3bc30c31, v72
image_sample_lz v[56:58], v[71:72], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v73, v50
v_add_f32 v74, 0x3fc00000, v53
v_mul_f32 v74, 0x3bc30c31, v74
image_sample_lz v[60:62], v[73:74], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v75, v50
v_add_f32 v76, 0x42620000, v53
v_mul_f32 v76, 0x3bc30c31, v76
image_sample_lz v[64:66], v[75:76], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v77, v50
v_add_f32 v78, 0x42660000, v53
v_mul_f32 v78, 0x3bc30c31, v78
image_sample_lz v[83:85], v[77:78], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v79, v50
v_add_f32 v80, 0x42e10000, v53
v_mul_f32 v80, 0x3bc30c31, v80
image_sample_lz v[86:88], v[79:80], s[20:27], s[28:31] dmask:0x7
v_mov_b32 v81, v50
v_add_f32 v82, 0x42e30000, v53
v_mul_f32 v82, 0x3bc30c31, v82
image_sample_lz v[89:91], v[81:82], s[20:27], s[28:31] dmask:0x7
v_mul_f32 v51, s72, v44
v_mac_f32 v51, s73, v45
v_mac_f32 v51, s74, v46
v_mul_f32 v52, v51, v51
v_add_f32 v52, 1.0, v52
v_mul_f32 v53, 0x3fcccccd, v51
v_sub_f32 v53, 0x3fd1eb85, v53
v_sqrt_f32 v55, v53
v_mul_f32 v55, v55, v53
v_rcp_f32 v55, v55
v_mul_f32 v55, 0x3c8557c9, v55
v_mul_f32 v55, v55, v52
v_mul_f32 v52, 0x3d747645, v52
v_mul_f32 v52, s67, v52
v_mul_f32 v55, s67, v55
s_waitcnt vmcnt(0)
v_sub_f32 v60, v60, v56
v_mac_f32 v56, v54, v60
v_sub_f32 v83, v83, v64
v_mac_f32 v64, v54, v83
v_sub_f32 v89, v89, v86
v_mac_f32 v86, v54, v89
v_mac_f32 v47, v56, v52
v_mac_f32 v47, v64, v55
v_mac_f32 v47, s67, v86
v_sub_f32 v61, v61, v57
v_mac_f32 v57, v54, v61
v_sub_f32 v84, v84, v65
v_mac_f32 v65, v54, v84
v_sub_f32 v90, v90, v87
v_mac_f32 v87, v54, v90
v_mac_f32 v48, v57, v52
v_mac_f32 v48, v65, v55
v_mac_f32 v48, s67, v87
v_sub_f32 v62, v62, v58
v_mac_f32 v58, v54, v62
v_sub_f32 v85, v85, v66
v_mac_f32 v66, v54, v85
v_sub_f32 v91, v91, v88
v_mac_f32 v88, v54, v91
v_mac_f32 v49, v58, v52
v_mac_f32 v49, v66, v55
v_mac_f32 v49, s67, v88
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
