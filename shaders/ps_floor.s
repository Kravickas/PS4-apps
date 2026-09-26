s_mov_b32 m0, s2
s_mov_b64 s[84:85], exec
s_wqm_b64 exec, exec
s_load_dwordx4 s[4:7], s[0:1], 0x8
s_load_dwordx8 s[8:15], s[0:1], 0x40
s_load_dwordx8 s[16:23], s[0:1], 0x48
s_load_dwordx8 s[24:31], s[0:1], 0x5c
s_load_dwordx4 s[32:35], s[0:1], 0x64
s_load_dwordx4 s[36:39], s[0:1], 0x68
s_load_dwordx4 s[40:43], s[0:1], 0x6c
s_load_dwordx4 s[44:47], s[0:1], 0x70
s_load_dwordx4 s[48:51], s[0:1], 0xc
s_load_dwordx4 s[52:55], s[0:1], 0x20
s_load_dwordx8 s[56:63], s[0:1], 0x28
s_load_dwordx4 s[64:67], s[0:1], 0x50
s_load_dwordx16 s[68:83], s[0:1], 0x30
v_interp_p1_f32 v10, v0, attr0.x
v_interp_p2_f32 v10, v1, attr0.x
v_interp_p1_f32 v11, v0, attr0.y
v_interp_p2_f32 v11, v1, attr0.y
v_interp_p1_f32 v25, v0, attr0.z
v_interp_p2_f32 v25, v1, attr0.z
v_interp_p1_f32 v26, v0, attr0.w
v_interp_p2_f32 v26, v1, attr0.w
v_interp_p1_f32 v12, v0, attr1.x
v_interp_p2_f32 v12, v1, attr1.x
v_interp_p1_f32 v13, v0, attr1.y
v_interp_p2_f32 v13, v1, attr1.y
v_interp_p1_f32 v14, v0, attr1.z
v_interp_p2_f32 v14, v1, attr1.z
v_interp_p1_f32 v24, v0, attr1.w
v_interp_p2_f32 v24, v1, attr1.w
v_mov_b32 v15, 1.0
s_waitcnt lgkmcnt(0)
v_mul_f32 v3, v24, v24
v_mac_f32 v3, v25, v25
v_mac_f32 v3, v26, v26
v_rsq_f32 v3, v3
v_mul_f32 v24, v24, v3
v_mul_f32 v25, v25, v3
v_mul_f32 v26, v26, v3
v_mul_f32 v27, v24, v24
v_sub_f32 v27, 1.0, v27
v_mul_f32 v28, v24, v25
v_sub_f32 v28, 0, v28
v_mul_f32 v29, v24, v26
v_sub_f32 v29, 0, v29
v_mul_f32 v3, v27, v27
v_mac_f32 v3, v28, v28
v_mac_f32 v3, v29, v29
v_rsq_f32 v3, v3
v_mul_f32 v27, v27, v3
v_mul_f32 v28, v28, v3
v_mul_f32 v29, v29, v3
v_mul_f32 v30, v28, v26
v_mul_f32 v3, v29, v25
v_sub_f32 v30, v30, v3
v_mul_f32 v31, v29, v24
v_mul_f32 v3, v27, v26
v_sub_f32 v31, v31, v3
v_mul_f32 v32, v27, v25
v_mul_f32 v3, v28, v24
v_sub_f32 v32, v32, v3
v_sub_f32 v33, s36, v12
v_sub_f32 v34, s37, v13
v_sub_f32 v35, s38, v14
v_mul_f32 v36, v33, v33
v_mac_f32 v36, v34, v34
v_mac_f32 v36, v35, v35
v_rsq_f32 v3, v36
v_mul_f32 v37, v36, v3
v_mul_f32 v33, v33, v3
v_mul_f32 v34, v34, v3
v_mul_f32 v35, v35, v3
v_mul_f32 v4, v33, v27
v_mac_f32 v4, v34, v28
v_mac_f32 v4, v35, v29
v_mul_f32 v5, v33, v30
v_mac_f32 v5, v34, v31
v_mac_f32 v5, v35, v32
v_mul_f32 v6, v33, v24
v_mac_f32 v6, v34, v25
v_mac_f32 v6, v35, v26
v_mul_f32 v7, s46, v37
v_sub_f32 v7, 1.0, v7
v_max_f32 v7, 0, v7
v_mul_f32 v7, s40, v7
v_max_f32 v8, 0x3dcccccd, v6
v_rcp_f32 v8, v8
v_mul_f32 v8, v8, v7
v_mul_f32 v8, 0x3d800000, v8
v_mul_f32 v40, v4, v8
v_mul_f32 v41, v5, v8
v_mov_b32 v44, v10
v_mov_b32 v45, v11
v_mov_b32 v46, v10
v_mov_b32 v47, v11
v_mov_b32 v43, 0
v_mov_b32 v48, 0
v_mov_b32 v49, 0
s_mov_b64 s[86:87], exec
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
image_sample v50, v[44:45], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_mul_f32 v50, s42, v50
v_add_f32 v50, s43, v50
v_sub_f32 v50, 1.0, v50
v_sub_f32 v51, v50, v43
v_cmp_lt_f32 vcc, 0, v51
s_and_b64 vcc, s[86:87], vcc
s_mov_b64 s[86:87], vcc
v_cndmask_b32 v49, v51, v49, vcc
v_cndmask_b32 v46, v46, v44, vcc
v_cndmask_b32 v47, v47, v45, vcc
v_cndmask_b32 v48, v48, v51, vcc
v_sub_f32 v52, v44, v40
v_cndmask_b32 v44, v44, v52, vcc
v_sub_f32 v52, v45, v41
v_cndmask_b32 v45, v45, v52, vcc
v_add_f32 v52, 0x3d800000, v43
v_cndmask_b32 v43, v43, v52, vcc
v_sub_f32 v52, v49, v48
v_min_f32 v52, 0xb58637bd, v52
v_rcp_f32 v52, v52
v_mul_f32 v52, v49, v52
v_max_f32 v52, 0, v52
v_min_f32 v52, 1.0, v52
v_sub_f32 v53, v46, v44
v_mac_f32 v44, v53, v52
v_sub_f32 v53, v47, v45
v_mac_f32 v45, v53, v52
image_sample v[16:19], v[44:45], s[8:15], s[4:7] dmask:0xf
image_sample v[60:63], v[44:45], s[16:23], s[4:7] dmask:0xf
s_load_dwordx4 s[24:27], s[0:1], 0x18
s_load_dwordx4 s[28:31], s[0:1], 0x1c
s_waitcnt vmcnt(0) lgkmcnt(0)
v_mul_f32 v60, 2.0, v60
v_add_f32 v60, -1.0, v60
v_mul_f32 v61, 2.0, v61
v_add_f32 v61, -1.0, v61
v_mul_f32 v62, 2.0, v62
v_add_f32 v62, -1.0, v62
v_mul_f32 v60, s44, v60
v_mul_f32 v61, s45, v61
v_mul_f32 v20, v27, v60
v_mac_f32 v20, v30, v61
v_mac_f32 v20, v24, v62
v_mul_f32 v21, v28, v60
v_mac_f32 v21, v31, v61
v_mac_f32 v21, v25, v62
v_mul_f32 v22, v29, v60
v_mac_f32 v22, v32, v61
v_mac_f32 v22, v26, v62
v_mul_f32 v3, v20, v20
v_mac_f32 v3, v21, v21
v_mac_f32 v3, v22, v22
v_rsq_f32 v3, v3
v_mul_f32 v20, v20, v3
v_mul_f32 v21, v21, v3
v_mul_f32 v22, v22, v3
v_mul_f32 v23, s48, v20
v_mac_f32 v23, s49, v21
v_mac_f32 v23, s50, v22
v_max_f32 v23, 0, v23
v_mul_f32 v23, 0x3f6de3f7, v23
v_add_f32 v23, 0x3d90e047, v23
v_mul_f32 v64, s68, v12
v_mac_f32 v64, s69, v13
v_mac_f32 v64, s70, v14
v_add_f32 v64, s71, v64
v_mul_f32 v65, s72, v12
v_mac_f32 v65, s73, v13
v_mac_f32 v65, s74, v14
v_add_f32 v65, s75, v65
v_mul_f32 v66, s76, v12
v_mac_f32 v66, s77, v13
v_mac_f32 v66, s78, v14
v_add_f32 v66, s79, v66
v_mul_f32 v67, s80, v12
v_mac_f32 v67, s81, v13
v_mac_f32 v67, s82, v14
v_add_f32 v67, s83, v67
v_rcp_f32 v68, v67
v_mul_f32 v64, v64, v68
v_mul_f32 v65, v65, v68
v_mul_f32 v66, v66, v68
v_mul_f32 v69, 0.5, v64
v_add_f32 v69, 0.5, v69
v_mul_f32 v70, 0.5, v65
v_add_f32 v70, 0.5, v70
v_max_legacy_f32 v71, 0, v66
v_min_legacy_f32 v71, 1.0, v71
image_sample v72, v[69:70], s[56:63], s[64:67] dmask:0x1
s_waitcnt vmcnt(0)
v_cmp_lt_f32 vcc, v72, v71
v_mov_b32 v73, 0
v_cndmask_b32 v72, 1.0, v73, vcc
v_cmp_lt_f32 vcc, 0, v67
v_cndmask_b32 v72, 1.0, v72, vcc
v_mul_f32 v73, 0x3f4848e6, v72
v_add_f32 v73, 0x3e5edc67, v73
v_mul_f32 v23, v73, v23
v_mul_f32 v74, v23, v16
v_mul_f32 v75, v23, v17
v_mul_f32 v76, v23, v18
v_mul_f32 v74, s52, v74
v_mul_f32 v75, s53, v75
v_mul_f32 v76, s54, v76
v_mul_f32 v77, s47, v2
v_mov_b32 v78, s28
v_subrev_f32 v78, s24, v78
v_mul_f32 v78, v78, v77
v_add_f32 v78, s24, v78
v_mov_b32 v79, s29
v_subrev_f32 v79, s25, v79
v_mul_f32 v79, v79, v77
v_add_f32 v79, s25, v79
v_mov_b32 v80, s30
v_subrev_f32 v80, s26, v80
v_mul_f32 v80, v80, v77
v_add_f32 v80, s26, v80
v_mul_f32 v81, s41, v37
v_add_f32 v81, s39, v81
v_exp_f32 v81, v81
v_min_f32 v81, 1.0, v81
v_sub_f32 v81, 1.0, v81
v_sub_f32 v82, v74, v78
v_mac_f32 v78, v82, v81
v_sub_f32 v82, v75, v79
v_mac_f32 v79, v82, v81
v_sub_f32 v82, v76, v80
v_mac_f32 v80, v82, v81
v_mov_b32 v83, 1.0
s_mov_b64 exec, s[84:85]
exp mrt0 v78, v79, v80, v83 done vm
s_endpgm
