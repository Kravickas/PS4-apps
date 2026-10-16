s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_waitcnt lgkmcnt(0)
v_mul_f32 v12, s16, v2
v_mul_f32 v13, s17, v3
v_mov_b32 v6, s18
v_mov_b32 v7, s19
v_mul_f32 v8, 0x3faaaaab, v6
v_mul_f32 v9, 0x3faaaaab, v7
v_mul_f32 v10, 0x40471c72, v6
v_mul_f32 v11, 0x40471c72, v7
v_add_f32 v14, v12, v8
v_add_f32 v15, v13, v9
v_sub_f32 v16, v12, v8
v_sub_f32 v17, v13, v9
v_add_f32 v18, v12, v10
v_add_f32 v19, v13, v11
v_sub_f32 v20, v12, v10
v_sub_f32 v21, v13, v11
image_sample_lz v[24:27], v[12:13], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[28:31], v[14:15], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[32:35], v[16:17], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[36:39], v[18:19], s[4:11], s[12:15] dmask:0xf
image_sample_lz v[40:43], v[20:21], s[4:11], s[12:15] dmask:0xf
s_waitcnt vmcnt(0)
v_add_f32 v44, v28, v32
v_add_f32 v45, v36, v40
v_mul_f32 v48, 0x3e8c0000, v24
v_mac_f32 v48, 0x3ea80000, v44
v_mac_f32 v48, 0x3d100000, v45
v_add_f32 v44, v29, v33
v_add_f32 v45, v37, v41
v_mul_f32 v49, 0x3e8c0000, v25
v_mac_f32 v49, 0x3ea80000, v44
v_mac_f32 v49, 0x3d100000, v45
v_add_f32 v44, v30, v34
v_add_f32 v45, v38, v42
v_mul_f32 v50, 0x3e8c0000, v26
v_mac_f32 v50, 0x3ea80000, v44
v_mac_f32 v50, 0x3d100000, v45
v_mov_b32 v51, 1.0
exp mrt0 v48, v49, v50, v51 done vm
s_endpgm
