s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
s_load_dwordx8 s[32:39], s[0:1], 0x1c
s_load_dwordx4 s[40:43], s[0:1], 0x24
s_load_dwordx8 s[44:51], s[0:1], 0x28
s_load_dwordx4 s[52:55], s[0:1], 0x30
s_load_dwordx8 s[60:67], s[0:1], 0x40
s_load_dwordx8 s[68:75], s[0:1], 0x48
s_load_dwordx4 s[76:79], s[0:1], 0x50
s_waitcnt lgkmcnt(0)
v_mul_f32 v4, s16, v2
v_mul_f32 v5, s17, v3
image_sample_lz v[8:11], v[4:5], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[12:15], v[4:5], s[20:27], s[28:31] dmask:0xf
v_mul_f32 v20, 0x3d897143, v2
v_mac_f32 v20, 0x3bbf4590, v3
v_fract_f32 v20, v20
v_mul_f32 v20, 0x4253ee82, v20
v_fract_f32 v20, v20
v_subrev_f32 v20, 0.5, v20
v_mul_f32 v20, 0x3b808081, v20
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
; uneven rays: glare texture (tools/make_glare.py) at 0.5 + (e - d) / 1.2 image heights
v_sub_f32 v29, v24, v22
v_sub_f32 v30, v25, v23
v_mul_f32 v31, 0x3f555555, v29
v_add_f32 v31, 0.5, v31
v_mul_f32 v32, 0x3f555555, v30
v_add_f32 v32, 0.5, v32
image_sample_lz v[33:35], v[31:32], s[44:51], s[52:55] dmask:0x7
; glow + veil while the sample is in flight (v31..v35 untouched): g / (1 + rho^2 / 0.08^2) + v / (1 + rho^2 / 0.40^2)
v_mul_f32 v29, v29, v29
v_mac_f32 v29, v30, v30
v_mul_f32 v30, 0x431c4000, v29
v_add_f32 v30, 1.0, v30
v_rcp_f32 v30, v30
v_mul_f32 v30, s39, v30
v_mul_f32 v36, 0x40c80000, v29
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
; >>> ui panels: frosted glass under the rounded panel rects (dwords 64..71: centre x, y, half w, h in px
;     per panel; hidden = half -1e6), m = coverage, e = edge; blur only in waves that touch a panel
v_mov_b32 v37, 0
v_mov_b32 v38, 0
v_subrev_f32 v39, s60, v2
v_max_f32 v39, v39, -v39
v_subrev_f32 v39, s62, v39
v_add_f32 v39, 0x41800000, v39
v_max_f32 v39, 0, v39
v_subrev_f32 v40, s61, v3
v_max_f32 v40, v40, -v40
v_subrev_f32 v40, s63, v40
v_add_f32 v40, 0x41800000, v40
v_max_f32 v40, 0, v40
v_mul_f32 v39, v39, v39
v_mac_f32 v39, v40, v40
v_sqrt_f32 v39, v39
v_add_f32 v39, 0xc1800000, v39
v_sub_f32 v40, 0.5, v39
v_max_f32 v40, 0, v40
v_min_f32 v40, 1.0, v40
v_max_f32 v37, v37, v40
v_add_f32 v40, 0x3f400000, v39
v_max_f32 v40, v40, -v40
v_sub_f32 v40, 1.0, v40
v_max_f32 v40, 0, v40
v_max_f32 v38, v38, v40
v_subrev_f32 v39, s64, v2
v_max_f32 v39, v39, -v39
v_subrev_f32 v39, s66, v39
v_add_f32 v39, 0x41800000, v39
v_max_f32 v39, 0, v39
v_subrev_f32 v40, s65, v3
v_max_f32 v40, v40, -v40
v_subrev_f32 v40, s67, v40
v_add_f32 v40, 0x41800000, v40
v_max_f32 v40, 0, v40
v_mul_f32 v39, v39, v39
v_mac_f32 v39, v40, v40
v_sqrt_f32 v39, v39
v_add_f32 v39, 0xc1800000, v39
v_sub_f32 v40, 0.5, v39
v_max_f32 v40, 0, v40
v_min_f32 v40, 1.0, v40
v_max_f32 v37, v37, v40
v_add_f32 v40, 0x3f400000, v39
v_max_f32 v40, v40, -v40
v_sub_f32 v40, 1.0, v40
v_max_f32 v40, 0, v40
v_max_f32 v38, v38, v40
v_cmp_lt_f32 vcc, 0, v37
s_cbranch_vccz ui_frost_done
; 5 x 5 bilinear taps, 8 px apart (scene T# s[4:11], bilinear S# s[28:31])
v_mov_b32 v57, 0
v_mov_b32 v58, 0
v_mov_b32 v59, 0
v_add_f32 v60, 0xbc088889, v4
v_add_f32 v61, 0xbc72b9d6, v5
image_sample_lz v[42:44], v[60:61], s[4:11], s[28:31] dmask:0x7
v_add_f32 v62, 0xbb888889, v4
v_add_f32 v63, 0xbc72b9d6, v5
image_sample_lz v[45:47], v[62:63], s[4:11], s[28:31] dmask:0x7
v_add_f32 v64, 0x00000000, v4
v_add_f32 v65, 0xbc72b9d6, v5
image_sample_lz v[48:50], v[64:65], s[4:11], s[28:31] dmask:0x7
v_add_f32 v66, 0x3b888889, v4
v_add_f32 v67, 0xbc72b9d6, v5
image_sample_lz v[51:53], v[66:67], s[4:11], s[28:31] dmask:0x7
v_add_f32 v68, 0x3c088889, v4
v_add_f32 v69, 0xbc72b9d6, v5
image_sample_lz v[54:56], v[68:69], s[4:11], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_add_f32 v57, v57, v42
v_add_f32 v58, v58, v43
v_add_f32 v59, v59, v44
v_add_f32 v57, v57, v45
v_add_f32 v58, v58, v46
v_add_f32 v59, v59, v47
v_add_f32 v57, v57, v48
v_add_f32 v58, v58, v49
v_add_f32 v59, v59, v50
v_add_f32 v57, v57, v51
v_add_f32 v58, v58, v52
v_add_f32 v59, v59, v53
v_add_f32 v57, v57, v54
v_add_f32 v58, v58, v55
v_add_f32 v59, v59, v56
v_add_f32 v60, 0xbc088889, v4
v_add_f32 v61, 0xbbf2b9d6, v5
image_sample_lz v[42:44], v[60:61], s[4:11], s[28:31] dmask:0x7
v_add_f32 v62, 0xbb888889, v4
v_add_f32 v63, 0xbbf2b9d6, v5
image_sample_lz v[45:47], v[62:63], s[4:11], s[28:31] dmask:0x7
v_add_f32 v64, 0x00000000, v4
v_add_f32 v65, 0xbbf2b9d6, v5
image_sample_lz v[48:50], v[64:65], s[4:11], s[28:31] dmask:0x7
v_add_f32 v66, 0x3b888889, v4
v_add_f32 v67, 0xbbf2b9d6, v5
image_sample_lz v[51:53], v[66:67], s[4:11], s[28:31] dmask:0x7
v_add_f32 v68, 0x3c088889, v4
v_add_f32 v69, 0xbbf2b9d6, v5
image_sample_lz v[54:56], v[68:69], s[4:11], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_add_f32 v57, v57, v42
v_add_f32 v58, v58, v43
v_add_f32 v59, v59, v44
v_add_f32 v57, v57, v45
v_add_f32 v58, v58, v46
v_add_f32 v59, v59, v47
v_add_f32 v57, v57, v48
v_add_f32 v58, v58, v49
v_add_f32 v59, v59, v50
v_add_f32 v57, v57, v51
v_add_f32 v58, v58, v52
v_add_f32 v59, v59, v53
v_add_f32 v57, v57, v54
v_add_f32 v58, v58, v55
v_add_f32 v59, v59, v56
v_add_f32 v60, 0xbc088889, v4
v_add_f32 v61, 0x00000000, v5
image_sample_lz v[42:44], v[60:61], s[4:11], s[28:31] dmask:0x7
v_add_f32 v62, 0xbb888889, v4
v_add_f32 v63, 0x00000000, v5
image_sample_lz v[45:47], v[62:63], s[4:11], s[28:31] dmask:0x7
v_add_f32 v64, 0x00000000, v4
v_add_f32 v65, 0x00000000, v5
image_sample_lz v[48:50], v[64:65], s[4:11], s[28:31] dmask:0x7
v_add_f32 v66, 0x3b888889, v4
v_add_f32 v67, 0x00000000, v5
image_sample_lz v[51:53], v[66:67], s[4:11], s[28:31] dmask:0x7
v_add_f32 v68, 0x3c088889, v4
v_add_f32 v69, 0x00000000, v5
image_sample_lz v[54:56], v[68:69], s[4:11], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_add_f32 v57, v57, v42
v_add_f32 v58, v58, v43
v_add_f32 v59, v59, v44
v_add_f32 v57, v57, v45
v_add_f32 v58, v58, v46
v_add_f32 v59, v59, v47
v_add_f32 v57, v57, v48
v_add_f32 v58, v58, v49
v_add_f32 v59, v59, v50
v_add_f32 v57, v57, v51
v_add_f32 v58, v58, v52
v_add_f32 v59, v59, v53
v_add_f32 v57, v57, v54
v_add_f32 v58, v58, v55
v_add_f32 v59, v59, v56
v_add_f32 v60, 0xbc088889, v4
v_add_f32 v61, 0x3bf2b9d6, v5
image_sample_lz v[42:44], v[60:61], s[4:11], s[28:31] dmask:0x7
v_add_f32 v62, 0xbb888889, v4
v_add_f32 v63, 0x3bf2b9d6, v5
image_sample_lz v[45:47], v[62:63], s[4:11], s[28:31] dmask:0x7
v_add_f32 v64, 0x00000000, v4
v_add_f32 v65, 0x3bf2b9d6, v5
image_sample_lz v[48:50], v[64:65], s[4:11], s[28:31] dmask:0x7
v_add_f32 v66, 0x3b888889, v4
v_add_f32 v67, 0x3bf2b9d6, v5
image_sample_lz v[51:53], v[66:67], s[4:11], s[28:31] dmask:0x7
v_add_f32 v68, 0x3c088889, v4
v_add_f32 v69, 0x3bf2b9d6, v5
image_sample_lz v[54:56], v[68:69], s[4:11], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_add_f32 v57, v57, v42
v_add_f32 v58, v58, v43
v_add_f32 v59, v59, v44
v_add_f32 v57, v57, v45
v_add_f32 v58, v58, v46
v_add_f32 v59, v59, v47
v_add_f32 v57, v57, v48
v_add_f32 v58, v58, v49
v_add_f32 v59, v59, v50
v_add_f32 v57, v57, v51
v_add_f32 v58, v58, v52
v_add_f32 v59, v59, v53
v_add_f32 v57, v57, v54
v_add_f32 v58, v58, v55
v_add_f32 v59, v59, v56
v_add_f32 v60, 0xbc088889, v4
v_add_f32 v61, 0x3c72b9d6, v5
image_sample_lz v[42:44], v[60:61], s[4:11], s[28:31] dmask:0x7
v_add_f32 v62, 0xbb888889, v4
v_add_f32 v63, 0x3c72b9d6, v5
image_sample_lz v[45:47], v[62:63], s[4:11], s[28:31] dmask:0x7
v_add_f32 v64, 0x00000000, v4
v_add_f32 v65, 0x3c72b9d6, v5
image_sample_lz v[48:50], v[64:65], s[4:11], s[28:31] dmask:0x7
v_add_f32 v66, 0x3b888889, v4
v_add_f32 v67, 0x3c72b9d6, v5
image_sample_lz v[51:53], v[66:67], s[4:11], s[28:31] dmask:0x7
v_add_f32 v68, 0x3c088889, v4
v_add_f32 v69, 0x3c72b9d6, v5
image_sample_lz v[54:56], v[68:69], s[4:11], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_add_f32 v57, v57, v42
v_add_f32 v58, v58, v43
v_add_f32 v59, v59, v44
v_add_f32 v57, v57, v45
v_add_f32 v58, v58, v46
v_add_f32 v59, v59, v47
v_add_f32 v57, v57, v48
v_add_f32 v58, v58, v49
v_add_f32 v59, v59, v50
v_add_f32 v57, v57, v51
v_add_f32 v58, v58, v52
v_add_f32 v59, v59, v53
v_add_f32 v57, v57, v54
v_add_f32 v58, v58, v55
v_add_f32 v59, v59, v56
image_sample_lz v[42:44], v[4:5], s[4:11], s[12:15] dmask:0x7
s_waitcnt vmcnt(0)
; glass = (colour with the sharp scene swapped for its blur) x 0.62 + tint; v8.. += m (glass - v8) + 0.08 e
v_mul_f32 v57, 0x3d23d70a, v57
v_sub_f32 v57, v57, v42
v_mul_f32 v57, s19, v57
v_add_f32 v57, v8, v57
v_mul_f32 v57, 0x3f1eb852, v57
v_add_f32 v57, 0x3c449ba6, v57
v_sub_f32 v57, v57, v8
v_mac_f32 v8, v57, v37
v_mac_f32 v8, 0x3da3d70a, v38
v_mul_f32 v58, 0x3d23d70a, v58
v_sub_f32 v58, v58, v43
v_mul_f32 v58, s19, v58
v_add_f32 v58, v9, v58
v_mul_f32 v58, 0x3f1eb852, v58
v_add_f32 v58, 0x3c449ba6, v58
v_sub_f32 v58, v58, v9
v_mac_f32 v9, v58, v37
v_mac_f32 v9, 0x3da3d70a, v38
v_mul_f32 v59, 0x3d23d70a, v59
v_sub_f32 v59, v59, v44
v_mul_f32 v59, s19, v59
v_add_f32 v59, v10, v59
v_mul_f32 v59, 0x3f1eb852, v59
v_add_f32 v59, 0x3c83126f, v59
v_sub_f32 v59, v59, v10
v_mac_f32 v10, v59, v37
v_mac_f32 v10, 0x3da3d70a, v38
ui_frost_done:
; <<< ui panels (frost)
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_max_f32 v9, 0, v9
v_min_f32 v9, 1.0, v9
v_max_f32 v10, 0, v10
v_min_f32 v10, 1.0, v10
v_log_f32 v16, v8
v_mul_f32 v16, 0x3ed55555, v16
v_exp_f32 v16, v16
v_mul_f32 v16, 0x3f870a3d, v16
v_subrev_f32 v16, 0x3d6147ae, v16
v_mul_f32 v21, 0x414eb852, v8
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v8
v_cndmask_b32 v16, v16, v21, vcc
v_log_f32 v17, v9
v_mul_f32 v17, 0x3ed55555, v17
v_exp_f32 v17, v17
v_mul_f32 v17, 0x3f870a3d, v17
v_subrev_f32 v17, 0x3d6147ae, v17
v_mul_f32 v21, 0x414eb852, v9
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v9
v_cndmask_b32 v17, v17, v21, vcc
v_log_f32 v18, v10
v_mul_f32 v18, 0x3ed55555, v18
v_exp_f32 v18, v18
v_mul_f32 v18, 0x3f870a3d, v18
v_subrev_f32 v18, 0x3d6147ae, v18
v_mul_f32 v21, 0x414eb852, v10
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v10
v_cndmask_b32 v18, v18, v21, vcc
; >>> ui content: premultiplied sRGB-space texture (dwords 72..79 T#, 80..83 point S#), panels only
v_cmp_lt_f32 vcc, 0, v37
s_cbranch_vccz ui_content_done
image_sample_lz v[40:43], v[4:5], s[68:75], s[76:79] dmask:0xf
s_waitcnt vmcnt(0)
v_sub_f32 v39, 1.0, v43
v_mul_f32 v16, v16, v39
v_add_f32 v16, v16, v40
v_mul_f32 v17, v17, v39
v_add_f32 v17, v17, v41
v_mul_f32 v18, v18, v39
v_add_f32 v18, v18, v42
ui_content_done:
; <<< ui content
v_add_f32 v16, v16, v20
v_add_f32 v17, v17, v20
v_add_f32 v18, v18, v20
v_mov_b32 v19, 1.0
exp mrt0 v16, v17, v18, v19 done vm
s_endpgm
