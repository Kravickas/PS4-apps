s_mov_b32 m0, s2
s_mov_b64 s[32:33], exec
s_wqm_b64 exec, exec
s_load_dwordx8 s[16:23], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[24:27], s[0:1], 0xc
s_load_dwordx4 s[28:31], s[0:1], 0x20
s_load_dwordx4 s[36:39], s[0:1], 0x68
s_load_dword s40, s[0:1], 0x6d
s_load_dword s41, s[0:1], 0x73
s_load_dwordx4 s[44:47], s[0:1], 0x18
s_load_dwordx4 s[48:51], s[0:1], 0x1c
s_waitcnt lgkmcnt(0)
v_interp_p1_f32 v3, v0, attr0.x
v_interp_p2_f32 v3, v1, attr0.x
v_interp_p1_f32 v4, v0, attr0.y
v_interp_p2_f32 v4, v1, attr0.y
v_interp_p1_f32 v10, v0, attr0.z
v_interp_p2_f32 v10, v1, attr0.z
v_interp_p1_f32 v11, v0, attr0.w
v_interp_p2_f32 v11, v1, attr0.w
v_interp_p1_f32 v12, v0, attr1.w
v_interp_p2_f32 v12, v1, attr1.w
v_interp_p1_f32 v5, v0, attr1.x
v_interp_p2_f32 v5, v1, attr1.x
v_interp_p1_f32 v6, v0, attr1.y
v_interp_p2_f32 v6, v1, attr1.y
v_interp_p1_f32 v7, v0, attr1.z
v_interp_p2_f32 v7, v1, attr1.z
image_sample v[16:19], v[3:4], s[16:23], s[12:15] dmask:0xf
s_waitcnt vmcnt(0)
v_mul_f32 v20, s24, v12
v_mul_f32 v21, s25, v10
v_add_f32 v20, v20, v21
v_mul_f32 v21, s26, v11
v_add_f32 v20, v20, v21
v_max_f32 v20, 0, v20
v_mul_f32 v21, 0x3f6de3f7, v20
v_add_f32 v21, 0x3d90e047, v21
v_mul_f32 v40, v16, v21
v_mul_f32 v41, v17, v21
v_mul_f32 v42, v18, v21
v_mul_f32 v40, s28, v40
v_mul_f32 v41, s29, v41
v_mul_f32 v42, s30, v42
v_sub_f32 v5, s36, v5
v_sub_f32 v6, s37, v6
v_sub_f32 v7, s38, v7
v_mul_f32 v8, v5, v5
v_mac_f32 v8, v6, v6
v_mac_f32 v8, v7, v7
v_rsq_f32 v9, v8
v_mul_f32 v8, v8, v9
v_mul_f32 v8, s40, v8
v_add_f32 v8, s39, v8
v_exp_f32 v8, v8
v_min_f32 v8, 1.0, v8
v_sub_f32 v8, 1.0, v8
v_mul_f32 v9, s41, v2
v_mov_b32 v24, s48
v_subrev_f32 v24, s44, v24
v_mul_f32 v24, v24, v9
v_add_f32 v24, s44, v24
v_mov_b32 v25, s49
v_subrev_f32 v25, s45, v25
v_mul_f32 v25, v25, v9
v_add_f32 v25, s45, v25
v_mov_b32 v26, s50
v_subrev_f32 v26, s46, v26
v_mul_f32 v26, v26, v9
v_add_f32 v26, s46, v26
v_sub_f32 v27, v40, v24
v_mac_f32 v24, v27, v8
v_sub_f32 v27, v41, v25
v_mac_f32 v25, v27, v8
v_sub_f32 v27, v42, v26
v_mac_f32 v26, v27, v8
v_mov_b32 v43, 1.0
s_mov_b64 exec, s[32:33]
exp mrt0 v24, v25, v26, v43 done vm
s_endpgm
