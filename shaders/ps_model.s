s_mov_b32 m0, s2
s_mov_b64 s[84:85], exec
s_wqm_b64 exec, exec
s_load_dwordx4 s[4:7], s[0:1], 0x8
s_load_dwordx8 s[8:15], s[0:1], 0x0
s_load_dwordx8 s[16:23], s[0:1], 0x80
s_load_dwordx8 s[24:31], s[0:1], 0x88
s_load_dwordx4 s[32:35], s[0:1], 0x64
s_load_dwordx4 s[36:39], s[0:1], 0x68
s_load_dwordx4 s[40:43], s[0:1], 0x90
s_load_dwordx4 s[44:47], s[0:1], 0x94
s_load_dwordx4 s[48:51], s[0:1], 0xc
s_load_dwordx4 s[52:55], s[0:1], 0x20
s_load_dwordx8 s[56:63], s[0:1], 0x28
s_load_dwordx4 s[64:67], s[0:1], 0x50
s_load_dwordx16 s[68:83], s[0:1], 0x30
s_load_dwordx8 s[88:95], s[0:1], 0x98
s_load_dwordx4 s[96:99], s[0:1], 0xa0
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
v_interp_p1_f32 v27, v0, attr2.x
v_interp_p2_f32 v27, v1, attr2.x
v_interp_p1_f32 v28, v0, attr2.y
v_interp_p2_f32 v28, v1, attr2.y
v_interp_p1_f32 v29, v0, attr2.z
v_interp_p2_f32 v29, v1, attr2.z
v_interp_p1_f32 v30, v0, attr2.w
v_interp_p2_f32 v30, v1, attr2.w
s_waitcnt lgkmcnt(0)
v_mul_f32 v3, v24, v24
v_mac_f32 v3, v25, v25
v_mac_f32 v3, v26, v26
v_rsq_f32 v3, v3
v_mul_f32 v24, v24, v3
v_mul_f32 v25, v25, v3
v_mul_f32 v26, v26, v3
v_mul_f32 v3, v24, v27
v_mac_f32 v3, v25, v28
v_mac_f32 v3, v26, v29
v_mul_f32 v4, v24, v3
v_sub_f32 v27, v27, v4
v_mul_f32 v4, v25, v3
v_sub_f32 v28, v28, v4
v_mul_f32 v4, v26, v3
v_sub_f32 v29, v29, v4
v_mul_f32 v3, v27, v27
v_mac_f32 v3, v28, v28
v_mac_f32 v3, v29, v29
v_rsq_f32 v3, v3
v_mul_f32 v27, v27, v3
v_mul_f32 v28, v28, v3
v_mul_f32 v29, v29, v3
v_mul_f32 v31, v25, v29
v_mul_f32 v3, v26, v28
v_sub_f32 v31, v31, v3
v_mul_f32 v32, v26, v27
v_mul_f32 v3, v24, v29
v_sub_f32 v32, v32, v3
v_mul_f32 v33, v24, v28
v_mul_f32 v3, v25, v27
v_sub_f32 v33, v33, v3
v_mul_f32 v31, v31, v30
v_mul_f32 v32, v32, v30
v_mul_f32 v33, v33, v30
v_sub_f32 v34, s36, v12
v_sub_f32 v35, s37, v13
v_sub_f32 v36, s38, v14
v_mul_f32 v37, v34, v34
v_mac_f32 v37, v35, v35
v_mac_f32 v37, v36, v36
v_rsq_f32 v3, v37
v_mul_f32 v38, v37, v3
v_mul_f32 v34, v34, v3
v_mul_f32 v35, v35, v3
v_mul_f32 v36, v36, v3
v_mul_f32 v4, v34, v27
v_mac_f32 v4, v35, v28
v_mac_f32 v4, v36, v29
v_mul_f32 v5, v34, v31
v_mac_f32 v5, v35, v32
v_mac_f32 v5, v36, v33
v_mul_f32 v6, v34, v24
v_mac_f32 v6, v35, v25
v_mac_f32 v6, v36, v26
; POM LOD fade (Tatarchuk, DX SDK ParallaxOcclusionMapping): height-map mip at the base UV (isotropic sampler)
image_get_lod v98, v[10:11], s[24:31], s[32:35] dmask:0x1
s_waitcnt vmcnt(0)
v_max_f32 v98, 0, v98
v_sub_f32 v98, 0x40400000, v98
v_max_f32 v98, 0, v98
v_min_f32 v98, 1.0, v98
v_mul_f32 v8, s40, v98
v_max_f32 v3, 0x3dcccccd, v6
v_rcp_f32 v3, v3
v_mul_f32 v8, v8, v3
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v50, s41, v50
v_add_f32 v50, s42, v50
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
v_mul_f32 v53, 0x3d800000, v52
v_sub_f32 v53, v43, v53
v_sub_f32 v53, 1.0, v53
v_mov_b32 v55, s48
v_mov_b32 v56, s49
v_mov_b32 v57, s50
v_mul_f32 v3, v55, v55
v_mac_f32 v3, v56, v56
v_mac_f32 v3, v57, v57
v_rsq_f32 v3, v3
v_mul_f32 v55, v55, v3
v_mul_f32 v56, v56, v3
v_mul_f32 v57, v57, v3
v_mul_f32 v58, v55, v27
v_mac_f32 v58, v56, v28
v_mac_f32 v58, v57, v29
v_mul_f32 v59, v55, v31
v_mac_f32 v59, v56, v32
v_mac_f32 v59, v57, v33
v_mul_f32 v60, v55, v24
v_mac_f32 v60, v56, v25
v_mac_f32 v60, v57, v26
v_max_f32 v61, 0x3d4ccccd, v60
v_rcp_f32 v61, v61
v_sub_f32 v62, 1.0, v53
v_mul_f32 v62, 0x3e000000, v62
v_mul_f32 v63, v61, v62
v_mul_f32 v63, s40, v63
v_mul_f32 v64, v58, v63
v_mul_f32 v65, v59, v63
v_mov_b32 v74, v44
v_mov_b32 v75, v45
v_mac_f32 v74, 1.0, v64
v_mac_f32 v75, 1.0, v65
v_mov_b32 v76, v44
v_mov_b32 v77, v45
v_mac_f32 v76, 2.0, v64
v_mac_f32 v77, 2.0, v65
v_mov_b32 v78, v44
v_mov_b32 v79, v45
v_mac_f32 v78, 0x40400000, v64
v_mac_f32 v79, 0x40400000, v65
v_mov_b32 v80, v44
v_mov_b32 v81, v45
v_mac_f32 v80, 4.0, v64
v_mac_f32 v81, 4.0, v65
v_mov_b32 v82, v44
v_mov_b32 v83, v45
v_mac_f32 v82, 0x40a00000, v64
v_mac_f32 v83, 0x40a00000, v65
v_mov_b32 v84, v44
v_mov_b32 v85, v45
v_mac_f32 v84, 0x40c00000, v64
v_mac_f32 v85, 0x40c00000, v65
v_mov_b32 v86, v44
v_mov_b32 v87, v45
v_mac_f32 v86, 0x40e00000, v64
v_mac_f32 v87, 0x40e00000, v65
v_mov_b32 v88, v44
v_mov_b32 v89, v45
v_mac_f32 v88, 0x41000000, v64
v_mac_f32 v89, 0x41000000, v65
image_sample v[16:19], v[44:45], s[8:15], s[4:7] dmask:0xf
image_sample v[20:23], v[44:45], s[16:23], s[4:7] dmask:0xf
image_sample v90, v[74:75], s[24:31], s[32:35] dmask:0x1
image_sample v91, v[76:77], s[24:31], s[32:35] dmask:0x1
image_sample v92, v[78:79], s[24:31], s[32:35] dmask:0x1
image_sample v93, v[80:81], s[24:31], s[32:35] dmask:0x1
image_sample v94, v[82:83], s[24:31], s[32:35] dmask:0x1
image_sample v95, v[84:85], s[24:31], s[32:35] dmask:0x1
image_sample v96, v[86:87], s[24:31], s[32:35] dmask:0x1
image_sample v97, v[88:89], s[24:31], s[32:35] dmask:0x1
s_load_dwordx4 s[24:27], s[0:1], 0x18
s_load_dwordx4 s[28:31], s[0:1], 0x1c
s_waitcnt vmcnt(0)
s_load_dword s32, s[0:1], 0x6d
s_load_dword s33, s[0:1], 0x73
v_mov_b32 v66, 0
v_mul_f32 v90, s41, v90
v_add_f32 v90, s42, v90
v_mov_b32 v67, v53
v_mac_f32 v67, 1.0, v62
v_sub_f32 v90, v90, v67
v_max_f32 v66, v66, v90
v_mul_f32 v91, s41, v91
v_add_f32 v91, s42, v91
v_mov_b32 v67, v53
v_mac_f32 v67, 2.0, v62
v_sub_f32 v91, v91, v67
v_max_f32 v66, v66, v91
v_mul_f32 v92, s41, v92
v_add_f32 v92, s42, v92
v_mov_b32 v67, v53
v_mac_f32 v67, 0x40400000, v62
v_sub_f32 v92, v92, v67
v_max_f32 v66, v66, v92
v_mul_f32 v93, s41, v93
v_add_f32 v93, s42, v93
v_mov_b32 v67, v53
v_mac_f32 v67, 4.0, v62
v_sub_f32 v93, v93, v67
v_max_f32 v66, v66, v93
v_mul_f32 v94, s41, v94
v_add_f32 v94, s42, v94
v_mov_b32 v67, v53
v_mac_f32 v67, 0x40a00000, v62
v_sub_f32 v94, v94, v67
v_max_f32 v66, v66, v94
v_mul_f32 v95, s41, v95
v_add_f32 v95, s42, v95
v_mov_b32 v67, v53
v_mac_f32 v67, 0x40c00000, v62
v_sub_f32 v95, v95, v67
v_max_f32 v66, v66, v95
v_mul_f32 v96, s41, v96
v_add_f32 v96, s42, v96
v_mov_b32 v67, v53
v_mac_f32 v67, 0x40e00000, v62
v_sub_f32 v96, v96, v67
v_max_f32 v66, v66, v96
v_mul_f32 v97, s41, v97
v_add_f32 v97, s42, v97
v_mov_b32 v67, v53
v_mac_f32 v67, 0x41000000, v62
v_sub_f32 v97, v97, v67
v_max_f32 v66, v66, v97
v_mul_f32 v66, 4.0, v66
v_min_f32 v66, 1.0, v66
v_mul_f32 v66, s43, v66
v_mul_f32 v66, v66, v98
v_sub_f32 v66, 1.0, v66
v_mul_f32 v20, 2.0, v20
v_add_f32 v20, -1.0, v20
v_mul_f32 v21, 2.0, v21
v_add_f32 v21, -1.0, v21
v_mul_f32 v22, v20, v20
v_mac_f32 v22, v21, v21
v_sub_f32 v22, 1.0, v22
v_max_f32 v22, 0, v22
v_sqrt_f32 v22, v22
v_mul_f32 v20, s44, v20
v_mul_f32 v21, s45, v21
v_mul_f32 v68, v27, v20
v_mac_f32 v68, v31, v21
v_mac_f32 v68, v24, v22
v_mul_f32 v69, v28, v20
v_mac_f32 v69, v32, v21
v_mac_f32 v69, v25, v22
v_mul_f32 v70, v29, v20
v_mac_f32 v70, v33, v21
v_mac_f32 v70, v26, v22
v_mul_f32 v3, v68, v68
v_mac_f32 v3, v69, v69
v_mac_f32 v3, v70, v70
v_rsq_f32 v3, v3
v_mul_f32 v68, v68, v3
v_mul_f32 v69, v69, v3
v_mul_f32 v70, v70, v3
v_mul_f32 v71, s48, v68
v_mul_f32 v72, s49, v69
v_add_f32 v71, v71, v72
v_mul_f32 v72, s50, v70
v_add_f32 v71, v71, v72
v_max_f32 v71, 0, v71
v_mov_b32 v72, v12
v_mac_f32 v72, s46, v24
v_mov_b32 v73, v13
v_mac_f32 v73, s46, v25
v_mov_b32 v74, v14
v_mac_f32 v74, s46, v26
v_mul_f32 v76, s68, v72
v_mac_f32 v76, s69, v73
v_mac_f32 v76, s70, v74
v_add_f32 v76, s71, v76
v_mul_f32 v77, s72, v72
v_mac_f32 v77, s73, v73
v_mac_f32 v77, s74, v74
v_add_f32 v77, s75, v77
v_mul_f32 v78, s76, v72
v_mac_f32 v78, s77, v73
v_mac_f32 v78, s78, v74
v_add_f32 v78, s79, v78
v_mul_f32 v79, s80, v72
v_mac_f32 v79, s81, v73
v_mac_f32 v79, s82, v74
v_add_f32 v79, s83, v79
v_rcp_f32 v80, v79
v_mul_f32 v76, v76, v80
v_mul_f32 v77, v77, v80
v_mul_f32 v78, v78, v80
v_mul_f32 v81, 0.5, v76
v_add_f32 v81, 0.5, v81
v_mul_f32 v82, 0.5, v77
v_add_f32 v82, 0.5, v82
v_max_legacy_f32 v83, 0, v78
v_min_legacy_f32 v83, 1.0, v83
v_subrev_f32 v83, s47, v83
; bilinear PCF: the 2 x 2 texels around the sample point (point sampler at texel centres, 4096^2 map),
; each compared with the reference (lit unless stored < ref), blended by the fractional position
v_mul_f32 v99, 0x45800000, v81
v_add_f32 v99, -0.5, v99
v_floor_f32 v100, v99
v_sub_f32 v99, v99, v100
v_mul_f32 v101, 0x45800000, v82
v_add_f32 v101, -0.5, v101
v_floor_f32 v102, v101
v_sub_f32 v101, v101, v102
v_add_f32 v103, 0.5, v100
v_mul_f32 v103, 0x39800000, v103
v_add_f32 v104, 0.5, v102
v_mul_f32 v104, 0x39800000, v104
v_add_f32 v105, 0x39800000, v103
v_mov_b32 v106, v104
v_mov_b32 v107, v103
v_add_f32 v108, 0x39800000, v104
v_mov_b32 v109, v105
v_mov_b32 v110, v108
image_sample_lz v111, v[103:104], s[56:63], s[64:67] dmask:0x1
image_sample_lz v112, v[105:106], s[56:63], s[64:67] dmask:0x1
image_sample_lz v113, v[107:108], s[56:63], s[64:67] dmask:0x1
image_sample_lz v114, v[109:110], s[56:63], s[64:67] dmask:0x1
s_waitcnt vmcnt(0) lgkmcnt(0)
v_mov_b32 v100, 0
v_cmp_lt_f32 vcc, v111, v83
v_cndmask_b32 v111, 1.0, v100, vcc
v_cmp_lt_f32 vcc, v112, v83
v_cndmask_b32 v112, 1.0, v100, vcc
v_cmp_lt_f32 vcc, v113, v83
v_cndmask_b32 v113, 1.0, v100, vcc
v_cmp_lt_f32 vcc, v114, v83
v_cndmask_b32 v114, 1.0, v100, vcc
v_sub_f32 v112, v112, v111
v_mac_f32 v111, v112, v99
v_sub_f32 v114, v114, v113
v_mac_f32 v113, v114, v99
v_sub_f32 v113, v113, v111
v_mac_f32 v111, v113, v101
v_mov_b32 v84, v111
v_cmp_lt_f32 vcc, 0, v79
v_cndmask_b32 v84, 1.0, v84, vcc
v_mul_f32 v85, 0x3f4848e6, v84
v_add_f32 v85, 0x3e5edc67, v85
v_mul_f32 v71, v71, v85
v_mul_f32 v71, v71, v66
v_mul_f32 v86, 0x3f6de3f7, v71
v_add_f32 v86, 0x3d90e047, v86
v_mul_f32 v40, v16, v86
v_mul_f32 v41, v17, v86
v_mul_f32 v42, v18, v86
v_mul_f32 v40, s52, v40
v_mul_f32 v41, s53, v41
v_mul_f32 v42, s54, v42
v_mov_b32 v58, s48
v_mul_f32 v58, v58, v58
v_mov_b32 v59, s49
v_mac_f32 v58, v59, v59
v_mov_b32 v59, s50
v_mac_f32 v58, v59, v59
v_sqrt_f32 v58, v58
v_add_f32 v90, v55, v34
v_add_f32 v91, v56, v35
v_add_f32 v92, v57, v36
v_mul_f32 v3, v90, v90
v_mac_f32 v3, v91, v91
v_mac_f32 v3, v92, v92
v_rsq_f32 v3, v3
v_mul_f32 v90, v90, v3
v_mul_f32 v91, v91, v3
v_mul_f32 v92, v92, v3
v_mul_f32 v93, v68, v90
v_mac_f32 v93, v69, v91
v_mac_f32 v93, v70, v92
v_max_f32 v93, 0, v93
v_mul_f32 v94, v68, v55
v_mac_f32 v94, v69, v56
v_mac_f32 v94, v70, v57
v_max_f32 v94, 0, v94
v_mul_f32 v95, v68, v34
v_mac_f32 v95, v69, v35
v_mac_f32 v95, v70, v36
v_max_f32 v95, 0x38d1b717, v95
v_mul_f32 v96, v34, v90
v_mac_f32 v96, v35, v91
v_mac_f32 v96, v36, v92
v_max_f32 v96, 0, v96
; geometric specular AA (Tokuyoshi & Kaplanyan, JCGT 10(2) 2021, Listing 5): alpha^2' =
; saturate(alpha^2 + min(2 SIGMA2 (|dN/dx|^2 + |dN/dy|^2), KAPPA)), SIGMA2 = 0.15915494, KAPPA = 0.18;
; coarse quad derivatives of N (v68..v70) by ds_swizzle (lane bit 0 = x, bit 1 = y); M0 = -1 for DS
; on CI (LLVM ldsRequiresM0Init, < GFX9); the shader is in WQM here (helper lanes valid).
s_mov_b32 m0, -1
ds_swizzle_b32 v99, v68 offset:swizzle(QUAD_PERM,0,0,0,0)
ds_swizzle_b32 v100, v68 offset:swizzle(QUAD_PERM,1,1,1,1)
ds_swizzle_b32 v101, v68 offset:swizzle(QUAD_PERM,2,2,2,2)
ds_swizzle_b32 v102, v69 offset:swizzle(QUAD_PERM,0,0,0,0)
ds_swizzle_b32 v103, v69 offset:swizzle(QUAD_PERM,1,1,1,1)
ds_swizzle_b32 v104, v69 offset:swizzle(QUAD_PERM,2,2,2,2)
ds_swizzle_b32 v105, v70 offset:swizzle(QUAD_PERM,0,0,0,0)
ds_swizzle_b32 v106, v70 offset:swizzle(QUAD_PERM,1,1,1,1)
ds_swizzle_b32 v107, v70 offset:swizzle(QUAD_PERM,2,2,2,2)
s_waitcnt lgkmcnt(0)
v_sub_f32 v100, v100, v99
v_sub_f32 v101, v101, v99
v_sub_f32 v103, v103, v102
v_sub_f32 v104, v104, v102
v_sub_f32 v106, v106, v105
v_sub_f32 v107, v107, v105
v_mul_f32 v108, v100, v100
v_mac_f32 v108, v101, v101
v_mac_f32 v108, v103, v103
v_mac_f32 v108, v104, v104
v_mac_f32 v108, v106, v106
v_mac_f32 v108, v107, v107
v_mul_f32 v108, 0x3ea2f983, v108
v_min_f32 v108, 0x3e3851ec, v108
v_add_f32 v109, s88, v108
v_min_f32 v109, 1.0, v109
v_sqrt_f32 v110, v109
v_mul_f32 v110, 0.5, v110
v_mul_f32 v97, v93, v93
v_mov_b32 v59, v109
v_add_f32 v59, -1.0, v59
v_mul_f32 v97, v97, v59
v_add_f32 v97, 1.0, v97
v_mul_f32 v97, v97, v97
v_rcp_f32 v97, v97
v_mul_f32 v97, v109, v97
v_mov_b32 v59, v110
v_sub_f32 v60, 1.0, v59
v_mul_f32 v61, v94, v60
v_add_f32 v61, v110, v61
v_mul_f32 v62, v95, v60
v_add_f32 v62, v110, v62
v_mul_f32 v61, v61, v62
v_rcp_f32 v61, v61
v_mul_f32 v61, 0x3e800000, v61
v_sub_f32 v62, 1.0, v96
v_mul_f32 v63, v62, v62
v_mul_f32 v63, v63, v63
v_mul_f32 v63, v63, v62
v_mul_f32 v63, s91, v63
v_add_f32 v63, s90, v63
v_mul_f32 v97, v97, v61
v_mul_f32 v97, v97, v63
v_mul_f32 v97, v97, v94
v_mul_f32 v97, v97, v58
v_mul_f32 v97, v97, v85
v_mul_f32 v97, v97, v66
; energy balance: the diffuse light x (1 - F), F = desc[156] + desc[157] (1 - N.V)^5 (the sky reflection's weight)
v_sub_f32 v63, 1.0, v95
v_mul_f32 v64, v63, v63
v_mul_f32 v64, v64, v64
v_mul_f32 v64, v64, v63
v_mul_f32 v64, s93, v64
v_add_f32 v64, s92, v64
v_sub_f32 v65, 1.0, v64
v_mul_f32 v40, v40, v65
v_mul_f32 v41, v41, v65
v_mul_f32 v42, v42, v65
v_mac_f32 v40, s52, v97
v_mac_f32 v41, s53, v97
v_mac_f32 v42, s54, v97
; sky reflection: R.y = 2 (N.V) N.y - V.y; sky = horizon + (zenith - horizon) clamp(R.y, 0, 1);
; below the horizon the floor (desc[160..162]): env = ground + (sky - ground) b,
; b = smoothstep(clamp(R.y desc[158] + 0.5, 0, 1)), desc[158] = 1 / (2 a): the blend spans R.y in [-a, a]
v_mul_f32 v62, v95, v69
v_add_f32 v62, v62, v62
v_sub_f32 v62, v62, v35
v_max_f32 v61, 0, v62
v_min_f32 v61, 1.0, v61
v_mul_f32 v60, s94, v62
v_add_f32 v60, 0.5, v60
v_max_f32 v60, 0, v60
v_min_f32 v60, 1.0, v60
v_mul_f32 v63, v60, v60
v_mul_f32 v59, -2.0, v60
v_add_f32 v59, 0x40400000, v59
v_mul_f32 v60, v63, v59
v_mov_b32 v65, s24
v_subrev_f32 v65, s28, v65
v_mul_f32 v65, v65, v61
v_add_f32 v65, s28, v65
v_subrev_f32 v65, s96, v65
v_mul_f32 v65, v65, v60
v_add_f32 v65, s96, v65
v_mac_f32 v40, v65, v64
v_mov_b32 v65, s25
v_subrev_f32 v65, s29, v65
v_mul_f32 v65, v65, v61
v_add_f32 v65, s29, v65
v_subrev_f32 v65, s97, v65
v_mul_f32 v65, v65, v60
v_add_f32 v65, s97, v65
v_mac_f32 v41, v65, v64
v_mov_b32 v65, s26
v_subrev_f32 v65, s30, v65
v_mul_f32 v65, v65, v61
v_add_f32 v65, s30, v65
v_subrev_f32 v65, s98, v65
v_mul_f32 v65, v65, v60
v_add_f32 v65, s98, v65
v_mac_f32 v42, v65, v64
v_mul_f32 v87, s32, v38
v_add_f32 v87, s39, v87
v_exp_f32 v87, v87
v_min_f32 v87, 1.0, v87
v_sub_f32 v87, 1.0, v87
v_mul_f32 v88, s33, v2
v_mov_b32 v56, s28
v_subrev_f32 v56, s24, v56
v_mul_f32 v56, v56, v88
v_add_f32 v56, s24, v56
v_mov_b32 v57, s29
v_subrev_f32 v57, s25, v57
v_mul_f32 v57, v57, v88
v_add_f32 v57, s25, v57
v_mov_b32 v58, s30
v_subrev_f32 v58, s26, v58
v_mul_f32 v58, v58, v88
v_add_f32 v58, s26, v58
v_sub_f32 v89, v40, v56
v_mac_f32 v56, v89, v87
v_sub_f32 v89, v41, v57
v_mac_f32 v57, v89, v87
v_sub_f32 v89, v42, v58
v_mac_f32 v58, v89, v87
v_mov_b32 v43, 1.0
s_mov_b64 exec, s[84:85]
exp mrt0 v56, v57, v58, v43 done vm
s_endpgm
