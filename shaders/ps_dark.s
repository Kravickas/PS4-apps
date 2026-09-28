; ps_dark: physically based sky + limb-darkened sun + lit moon.
; Sky: tables of assets/sky/atmosphere.bin (atmosphere.c: Bruneton 2017 transmittance and single
; scattering, Hillaire 2020 multiple scattering) - Rayleigh / Mie / multiple integrals without the
; phase, 56 light-elevation slices of 64 x 64 stacked in three RGBA16F atlases; per light two
; slices blended, the phases applied per pixel (sharp Mie aureole). The sun, the moon (and the
; stars, ps_stars) lie beyond the atmosphere: they are ADDED to the in-scattered sky light.
; desc[164..203] (atmo_sky_consts): F, tilt0 | R tan, tilt x | U tan, tilt z | sun L, E | shx, shz,
; row0, row1 | f |
;   moon L, E | mhx, mhz, row0, row1 | f, sun in the moon frame (x, y, z) | limb alpha RGB
; desc[204] / [212] / [220] atlas T#s, [228] their S#, [232] moon albedo T#, [240] its S#.
s_mov_b32 m0, s2
s_mov_b64 s[100:101], exec
s_wqm_b64 exec, exec
s_load_dwordx4 s[4:7], s[0:1], 0x10
s_load_dwordx4 s[8:11], s[0:1], 0x14
s_load_dwordx4 s[12:15], s[0:1], 0x54
s_load_dwordx4 s[16:19], s[0:1], 0x58
s_load_dwordx16 s[20:35], s[0:1], 0xa4
s_load_dwordx16 s[36:51], s[0:1], 0xb4
s_load_dwordx8 s[52:59], s[0:1], 0xc4
s_load_dwordx8 s[60:67], s[0:1], 0xcc
s_load_dwordx8 s[68:75], s[0:1], 0xd4
s_load_dwordx8 s[76:83], s[0:1], 0xdc
s_load_dwordx4 s[84:87], s[0:1], 0xe4
s_load_dwordx8 s[88:95], s[0:1], 0xe8
s_load_dwordx4 s[96:99], s[0:1], 0xf0
v_interp_p1_f32 v2, v0, attr0.x
v_interp_p2_f32 v2, v1, attr0.x
v_interp_p1_f32 v3, v0, attr0.y
v_interp_p2_f32 v3, v1, attr0.y
s_waitcnt lgkmcnt(0)
; view direction V = normalize(F + x R/fov + y U/fov) (x aspect-scaled NDC, y NDC)
v_mov_b32 v4, s20
v_mac_f32 v4, s24, v2
v_mac_f32 v4, s28, v3
v_mov_b32 v5, s21
v_mac_f32 v5, s25, v2
v_mac_f32 v5, s29, v3
v_mov_b32 v6, s22
v_mac_f32 v6, s26, v2
v_mac_f32 v6, s30, v3
v_mul_f32 v7, v4, v4
v_mac_f32 v7, v5, v5
v_mac_f32 v7, v6, v6
v_rsq_f32 v7, v7
v_mul_f32 v4, v4, v7
v_mul_f32 v5, v5, v7
v_mul_f32 v6, v6, v7
; 1 / |V horizontal|; the view's height above the camera's dipped horizon (the curved floor seen
; from h above it at (cx, cz)): tan(dip) = d[3] + (d[7] Vx + d[11] Vz) / |Vh| (sqrt(2h/R), cx/R, cz/R),
; y' = Vy cos(dip) + |Vh| sin(dip) - zero tilt: y' = Vy
v_mul_f32 v12, v4, v4
v_mac_f32 v12, v6, v6
v_max_f32 v12, 0x2b8cbccc, v12
v_rsq_f32 v10, v12
v_mul_f32 v12, v12, v10
v_mul_f32 v8, s27, v4
v_mac_f32 v8, s31, v6
v_mul_f32 v8, v8, v10
v_add_f32 v8, s23, v8
v_mul_f32 v9, v8, v8
v_add_f32 v9, 1.0, v9
v_rsq_f32 v9, v9
v_mul_f32 v8, v8, v9
v_mul_f32 v12, v12, v8
v_mac_f32 v12, v5, v9
; table row: clamp((0.5 + 0.5 sign(y') sqrt|y'|) x 64, 0.5, 63.5)
v_and_b32 v8, 0x7fffffff, v12
v_sqrt_f32 v8, v8
v_mul_f32 v8, 0.5, v8
v_add_f32 v9, 0.5, v8
v_sub_f32 v13, 0.5, v8
v_cmp_le_f32 vcc, 0, v12
v_cndmask_b32 v9, v13, v9, vcc
v_mul_f32 v9, 0x42800000, v9
v_max_f32 v9, 0.5, v9
v_min_f32 v9, 0x427e0000, v9
v_mov_b32 v41, 0
v_mov_b32 v42, 0
v_mov_b32 v43, 0
; ---- sun: sky light E ((R0 + f(R1-R0)) P_R + (M0 + f(M1-M0)) P_M + (S0 + f(S1-S0))) ----
; u = 0.5 - 0.5 cos(phi), cos(phi) = (Vx hx + Vz hz) / |Vh| clamped; texture v = (slice row + row) / 3584
v_mul_f32 v11, s36, v4
v_mac_f32 v11, s37, v6
v_mul_f32 v11, v11, v10
v_max_f32 v11, -1.0, v11
v_min_f32 v11, 1.0, v11
v_mul_f32 v11, 0.5, v11
v_sub_f32 v14, 0.5, v11
v_mov_b32 v16, v14
v_add_f32 v15, s38, v9
v_mul_f32 v15, 0x39924925, v15
v_add_f32 v17, s39, v9
v_mul_f32 v17, 0x39924925, v17
image_sample v[18:20], v[14:15], s[60:67], s[84:87] dmask:0x7
image_sample v[21:23], v[16:17], s[60:67], s[84:87] dmask:0x7
image_sample v[24:26], v[14:15], s[68:75], s[84:87] dmask:0x7
image_sample v[27:29], v[16:17], s[68:75], s[84:87] dmask:0x7
image_sample v[30:32], v[14:15], s[76:83], s[84:87] dmask:0x7
image_sample v[33:35], v[16:17], s[76:83], s[84:87] dmask:0x7
; phases while the samples are in flight: nu = V.L; Rayleigh 3/(16 pi)(1 + nu^2);
; Cornette-Shanks 3/(8 pi)(1 - g^2)/(2 + g^2)(1 + nu^2)/(1 + g^2 - 2 g nu)^1.5, g = 0.8
v_mul_f32 v36, s32, v4
v_mac_f32 v36, s33, v5
v_mac_f32 v36, s34, v6
v_mul_f32 v37, v36, v36
v_add_f32 v37, 1.0, v37
v_mul_f32 v38, 0x3d747645, v37
v_mul_f32 v39, 0x3fcccccd, v36
v_sub_f32 v39, 0x3fd1eb85, v39
v_sqrt_f32 v40, v39
v_mul_f32 v40, v40, v39
v_rcp_f32 v40, v40
v_mul_f32 v39, 0x3c8557c9, v37
v_mul_f32 v39, v39, v40
s_waitcnt vmcnt(0)
v_sub_f32 v21, v21, v18
v_mac_f32 v18, s40, v21
v_sub_f32 v27, v27, v24
v_mac_f32 v24, s40, v27
v_sub_f32 v33, v33, v30
v_mac_f32 v30, s40, v33
v_mul_f32 v18, v18, v38
v_mac_f32 v18, v24, v39
v_add_f32 v18, v18, v30
v_mac_f32 v41, s35, v18
v_sub_f32 v22, v22, v19
v_mac_f32 v19, s40, v22
v_sub_f32 v28, v28, v25
v_mac_f32 v25, s40, v28
v_sub_f32 v34, v34, v31
v_mac_f32 v31, s40, v34
v_mul_f32 v19, v19, v38
v_mac_f32 v19, v25, v39
v_add_f32 v19, v19, v31
v_mac_f32 v42, s35, v19
v_sub_f32 v23, v23, v20
v_mac_f32 v20, s40, v23
v_sub_f32 v29, v29, v26
v_mac_f32 v26, s40, v29
v_sub_f32 v35, v35, v32
v_mac_f32 v32, s40, v35
v_mul_f32 v20, v20, v38
v_mac_f32 v20, v26, v39
v_add_f32 v20, v20, v32
v_mac_f32 v43, s35, v20
; ---- moon: sky light E ((R0 + f(R1-R0)) P_R + (M0 + f(M1-M0)) P_M + (S0 + f(S1-S0))) ----
; u = 0.5 - 0.5 cos(phi), cos(phi) = (Vx hx + Vz hz) / |Vh| clamped; texture v = (slice row + row) / 3584
v_mul_f32 v11, s48, v4
v_mac_f32 v11, s49, v6
v_mul_f32 v11, v11, v10
v_max_f32 v11, -1.0, v11
v_min_f32 v11, 1.0, v11
v_mul_f32 v11, 0.5, v11
v_sub_f32 v14, 0.5, v11
v_mov_b32 v16, v14
v_add_f32 v15, s50, v9
v_mul_f32 v15, 0x39924925, v15
v_add_f32 v17, s51, v9
v_mul_f32 v17, 0x39924925, v17
image_sample v[18:20], v[14:15], s[60:67], s[84:87] dmask:0x7
image_sample v[21:23], v[16:17], s[60:67], s[84:87] dmask:0x7
image_sample v[24:26], v[14:15], s[68:75], s[84:87] dmask:0x7
image_sample v[27:29], v[16:17], s[68:75], s[84:87] dmask:0x7
image_sample v[30:32], v[14:15], s[76:83], s[84:87] dmask:0x7
image_sample v[33:35], v[16:17], s[76:83], s[84:87] dmask:0x7
; phases while the samples are in flight: nu = V.L; Rayleigh 3/(16 pi)(1 + nu^2);
; Cornette-Shanks 3/(8 pi)(1 - g^2)/(2 + g^2)(1 + nu^2)/(1 + g^2 - 2 g nu)^1.5, g = 0.8
v_mul_f32 v36, s44, v4
v_mac_f32 v36, s45, v5
v_mac_f32 v36, s46, v6
v_mul_f32 v37, v36, v36
v_add_f32 v37, 1.0, v37
v_mul_f32 v38, 0x3d747645, v37
v_mul_f32 v39, 0x3fcccccd, v36
v_sub_f32 v39, 0x3fd1eb85, v39
v_sqrt_f32 v40, v39
v_mul_f32 v40, v40, v39
v_rcp_f32 v40, v40
v_mul_f32 v39, 0x3c8557c9, v37
v_mul_f32 v39, v39, v40
s_waitcnt vmcnt(0)
v_sub_f32 v21, v21, v18
v_mac_f32 v18, s52, v21
v_sub_f32 v27, v27, v24
v_mac_f32 v24, s52, v27
v_sub_f32 v33, v33, v30
v_mac_f32 v30, s52, v33
v_mul_f32 v18, v18, v38
v_mac_f32 v18, v24, v39
v_add_f32 v18, v18, v30
v_mac_f32 v41, s47, v18
v_sub_f32 v22, v22, v19
v_mac_f32 v19, s52, v22
v_sub_f32 v28, v28, v25
v_mac_f32 v25, s52, v28
v_sub_f32 v34, v34, v31
v_mac_f32 v31, s52, v34
v_mul_f32 v19, v19, v38
v_mac_f32 v19, v25, v39
v_add_f32 v19, v19, v31
v_mac_f32 v42, s47, v19
v_sub_f32 v23, v23, v20
v_mac_f32 v20, s52, v23
v_sub_f32 v29, v29, v26
v_mac_f32 v26, s52, v29
v_sub_f32 v35, v35, v32
v_mac_f32 v32, s52, v35
v_mul_f32 v20, v20, v38
v_mac_f32 v20, v26, v39
v_add_f32 v20, v20, v32
v_mac_f32 v43, s47, v20
; ---- sun disc: coverage (1 px anti-aliased edge) x mu^alpha per channel (Hestroffer & Magnan
; 1998 power law, mu = sqrt(1 - d^2 / r^2)) x its colour through the air x SUN_HDR ----
v_subrev_f32 v8, s4, v2
v_subrev_f32 v11, s5, v3
v_mul_f32 v8, v8, v8
v_mac_f32 v8, v11, v11
v_sqrt_f32 v11, v8
v_sub_f32 v11, s7, v11
v_mul_f32 v11, 0x44070000, v11
v_add_f32 v11, 0.5, v11
v_max_f32 v11, 0, v11
v_min_f32 v11, 1.0, v11
v_rcp_f32 v12, s6
v_mul_f32 v8, v8, v12
v_sub_f32 v8, 1.0, v8
v_max_f32 v8, 0, v8
v_sqrt_f32 v8, v8
v_log_f32 v8, v8
v_mul_f32 v13, s56, v8
v_exp_f32 v13, v13
v_mul_f32 v13, v13, v11
v_mac_f32 v41, s12, v13
v_mul_f32 v13, s57, v8
v_exp_f32 v13, v13
v_mul_f32 v13, v13, v11
v_mac_f32 v42, s13, v13
v_mul_f32 v13, s58, v8
v_exp_f32 v13, v13
v_mul_f32 v13, v13, v11
v_mac_f32 v43, s14, v13
; ---- moon disc: coverage (as ps_stars), sphere normal n = (dx/r, dy/r, sqrt(1 - ...)), albedo of the
; orthographic near-side map at 0.5 + 0.5 (nx, ny), Lommel-Seeliger max(mu0, 0) / (mu0 + mu), mu0 = n . L
; (sun in the moon's frame), mu = nz ----
v_subrev_f32 v8, s8, v2
v_subrev_f32 v11, s9, v3
v_mul_f32 v12, v8, v8
v_mac_f32 v12, v11, v11
v_sqrt_f32 v13, v12
v_sub_f32 v13, s11, v13
v_mul_f32 v13, 0x44070000, v13
v_add_f32 v13, 0.5, v13
v_max_f32 v13, 0, v13
v_min_f32 v13, 1.0, v13
v_rcp_f32 v14, s11
v_mul_f32 v8, v8, v14
v_mul_f32 v11, v11, v14
v_mul_f32 v15, v8, v8
v_mac_f32 v15, v11, v11
v_sub_f32 v15, 1.0, v15
v_max_f32 v15, 0, v15
v_sqrt_f32 v15, v15
v_mul_f32 v16, 0.5, v8
v_add_f32 v16, 0.5, v16
v_mul_f32 v17, 0.5, v11
v_add_f32 v17, 0.5, v17
image_sample v[18:20], v[16:17], s[88:95], s[96:99] dmask:0x7
v_mul_f32 v21, s53, v8
v_mac_f32 v21, s54, v11
v_mac_f32 v21, s55, v15
v_add_f32 v22, v21, v15
v_max_f32 v22, 0x358637bd, v22
v_rcp_f32 v22, v22
v_max_f32 v21, 0, v21
v_mul_f32 v21, v21, v22
v_mul_f32 v21, v21, v13
s_waitcnt vmcnt(0)
v_mul_f32 v18, v18, v21
v_mac_f32 v41, s16, v18
v_mul_f32 v19, v19, v21
v_mac_f32 v42, s17, v19
v_mul_f32 v20, v20, v21
v_mac_f32 v43, s18, v20
v_mov_b32 v44, 1.0
s_mov_b64 exec, s[100:101]
exp mrt0 v41, v42, v43, v44 done vm
s_endpgm
