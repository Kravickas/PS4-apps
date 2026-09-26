s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
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
