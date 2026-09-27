s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
s_load_dwordx8 s[32:39], s[0:1], 0x1c
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
v_sub_f32 v29, v24, v22
v_sub_f32 v30, v25, v23
v_mul_f32 v31, v30, v30
v_mul_f32 v31, 0x46d9038e, v31
v_max_f32 v32, v29, -v29
v_mac_f32 v31, 4.0, v32
v_exp_f32 v31, -v31
v_mac_f32 v26, 0x3e0ccccd, v31
v_mac_f32 v27, 0x3e400000, v31
v_mac_f32 v28, 0x3e800000, v31
v_mul_f32 v31, v29, v29
v_mac_f32 v31, v30, v30
v_sqrt_f32 v31, v31
v_add_f32 v31, 0xbe99999a, v31
v_mul_f32 v31, 0x42055555, v31
v_mul_f32 v31, v31, v31
v_sub_f32 v31, 1.0, v31
v_max_f32 v31, 0, v31
v_mul_f32 v31, v31, v31
v_mac_f32 v26, 0x3d4ccccd, v31
v_mac_f32 v27, 0x3d2e147b, v31
v_mac_f32 v28, 0x3d0f5c29, v31
v_mac_f32 v8, s34, v26
v_mac_f32 v9, s35, v27
v_mac_f32 v10, s36, v28
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
v_add_f32 v16, v16, v20
v_add_f32 v17, v17, v20
v_add_f32 v18, v18, v20
v_mov_b32 v19, 1.0
exp mrt0 v16, v17, v18, v19 done vm
s_endpgm
