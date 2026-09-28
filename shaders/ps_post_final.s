s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
s_load_dwordx8 s[32:39], s[0:1], 0x1c
s_load_dwordx4 s[40:43], s[0:1], 0x24
s_load_dwordx8 s[44:51], s[0:1], 0x28
s_load_dwordx4 s[52:55], s[0:1], 0x30
s_waitcnt lgkmcnt(0)
v_mul_f32 v4, s16, v2
v_mul_f32 v5, s17, v3
image_sample_lz v[8:11], v[4:5], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[12:15], v[4:5], s[20:27], s[28:31] dmask:0xf
s_waitcnt vmcnt(0)
v_mac_f32 v8, s18, v12
v_mac_f32 v9, s18, v13
v_mac_f32 v10, s18, v14
v_mul_f32 v8, s19, v8
v_mul_f32 v9, s19, v9
v_mul_f32 v10, s19, v10
; >>> lens flare: soft ghosts + glare texture rays + glow + veil (strength 0: skipped)
s_or_b32 s56, s34, s35
s_or_b32 s56, s56, s36
s_cmp_eq_u32 s56, 0
s_cbranch_scc1 flare_done
; soft ghosts (the first flare's six): e, d, accumulate in v26..v28
v_add_f32 v24, -0.5, v4
v_mul_f32 v24, 0x3fe38e39, v24
v_add_f32 v25, -0.5, v5
v_mov_b32 v22, s32
v_add_f32 v22, -0.5, v22
v_mul_f32 v22, 0x3fe38e39, v22
v_mov_b32 v23, s33
v_add_f32 v23, -0.5, v23
v_mov_b32 v26, 0
v_mov_b32 v27, 0
v_mov_b32 v28, 0
v_mul_f32 v29, 0xbe800000, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0xbe800000, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x444c14e6, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3eb33333, v29
v_mac_f32 v27, 0x3e810625, v29
v_mac_f32 v28, 0x3e0f5c29, v29
v_mul_f32 v29, 0xbf000000, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0xbf000000, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x42f6e9e0, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3d3851ec, v29
v_mac_f32 v27, 0x3da3d70a, v29
v_mac_f32 v28, 0x3dcccccd, v29
v_mul_f32 v29, 0xbf4ccccd, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0xbf4ccccd, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x43c80000, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3dcac083, v29
v_mac_f32 v27, 0x3e3851ec, v29
v_mac_f32 v28, 0x3ddd2f1b, v29
v_mul_f32 v29, 0xbf8ccccd, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0xbf8ccccd, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x424c14e6, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3d2c0831, v29
v_mac_f32 v27, 0x3d072b02, v29
v_mac_f32 v28, 0x3d75c28f, v29
v_mul_f32 v29, 0x3ecccccd, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0x3ecccccd, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x44c80000, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3e99999a, v29
v_mac_f32 v27, 0x3e828f5c, v29
v_mac_f32 v28, 0x3e28f5c3, v29
v_mul_f32 v29, 0x3f333333, v22
v_sub_f32 v29, v24, v29
v_mul_f32 v30, 0x3f333333, v23
v_sub_f32 v30, v25, v30
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v29, 0x438ae38e, v29
v_sub_f32 v29, 1.0, v29
v_max_f32 v29, 0, v29
v_mul_f32 v29, v29, v29
v_mac_f32 v26, 0x3d75c28f, v29
v_mac_f32 v27, 0x3dac0831, v29
v_mac_f32 v28, 0x3df5c28f, v29
v_mul_f32 v26, s37, v26
v_mul_f32 v27, s37, v27
v_mul_f32 v28, s37, v28
; uneven rays: glare texture (tools/make_glare.py) at 0.5 + (e - d) / 0.6 image heights (was 1.2)
v_sub_f32 v29, v24, v22
v_sub_f32 v30, v25, v23
v_mul_f32 v31, 0x3fd55555, v29
v_add_f32 v31, 0.5, v31
v_mul_f32 v32, 0x3fd55555, v30
v_add_f32 v32, 0.5, v32
image_sample_lz v[33:35], v[31:32], s[44:51], s[52:55] dmask:0x7
; glow + veil while the sample is in flight (v31..v35 untouched): g / (1 + rho^2 / 0.04^2) + v / (1 + rho^2 / 0.20^2) (widths were 0.08, 0.40)
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v30, 0x441c4000, v29
v_add_f32 v30, 1.0, v30
v_rcp_f32 v30, v30
v_mul_f32 v30, s39, v30
v_mul_f32 v36, 0x41c80000, v29
v_add_f32 v36, 1.0, v36
v_rcp_f32 v36, v36
v_mac_f32 v30, s40, v36
v_add_f32 v26, v26, v30
v_add_f32 v27, v27, v30
v_add_f32 v28, v28, v30
s_waitcnt vmcnt(0)
v_mac_f32 v26, s38, v33
v_mac_f32 v27, s38, v34
v_mac_f32 v28, s38, v35
v_mac_f32 v8, s34, v26
v_mac_f32 v9, s35, v27
v_mac_f32 v10, s36, v28
flare_done:
; <<< lens flare
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_max_f32 v9, 0, v9
v_min_f32 v9, 1.0, v9
v_max_f32 v10, 0, v10
v_min_f32 v10, 1.0, v10
v_mov_b32 v11, 1.0
exp mrt0 v8, v9, v10, v11 done vm
s_endpgm
