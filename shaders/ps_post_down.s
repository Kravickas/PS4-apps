s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dword s20, s[0:1], 0x1c
s_waitcnt lgkmcnt(0)
v_mul_f32 v4, s16, v2
v_mul_f32 v5, s17, v3
v_subrev_f32 v10, s18, v4
v_subrev_f32 v11, s19, v5
v_add_f32 v12, s18, v4
v_mov_b32 v13, v11
v_mov_b32 v14, v10
v_add_f32 v15, s19, v5
v_mov_b32 v16, v12
v_mov_b32 v17, v15
image_sample_lz v[20:23], v[10:11], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[24:27], v[12:13], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[28:31], v[14:15], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[32:35], v[16:17], s[4:11], s[12:15] dmask:0xf
s_waitcnt vmcnt(0)
v_add_f32 v20, v20, v24
v_add_f32 v20, v20, v28
v_add_f32 v20, v20, v32
v_add_f32 v21, v21, v25
v_add_f32 v21, v21, v29
v_add_f32 v21, v21, v33
v_add_f32 v22, v22, v26
v_add_f32 v22, v22, v30
v_add_f32 v22, v22, v34
v_mul_f32 v20, 0.25, v20
v_mul_f32 v21, 0.25, v21
v_mul_f32 v22, 0.25, v22
v_max_f32 v36, v20, v21
v_max_f32 v36, v22, v36
v_subrev_f32 v37, s20, v36
v_max_f32 v37, 0, v37
v_max_f32 v38, 0x38d1b717, v36
v_rcp_f32 v38, v38
v_mul_f32 v37, v37, v38
v_mul_f32 v20, v20, v37
v_mul_f32 v21, v21, v37
v_mul_f32 v22, v22, v37
v_mov_b32 v23, 1.0
exp mrt0 v20, v21, v22, v23 done vm
s_endpgm
