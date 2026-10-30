v_mov_b32 v1, 0
buffer_load_dwordx4 v[12:15], v1, s[0:3], 0 offen
buffer_load_dwordx4 v[16:19], v1, s[0:3], 0 offen offset:16
buffer_load_dwordx4 v[20:23], v1, s[0:3], 0 offen offset:32
buffer_load_dwordx4 v[24:27], v1, s[0:3], 0 offen offset:48
v_lshlrev_b32 v1, 5, v0
v_lshlrev_b32 v2, 4, v0
v_add_i32 v1, vcc, v1, v2
v_add_i32 v1, vcc, 64, v1
v_add_i32 v1, vcc, 16, v1
buffer_load_dwordx4 v[2:5], v1, s[0:3], 0 offen
buffer_load_dwordx4 v[6:9], v1, s[0:3], 0 offen offset:16
buffer_load_dwordx4 v[44:47], v1, s[0:3], 0 offen offset:32
s_waitcnt vmcnt(0) lgkmcnt(0)
v_mul_f32 v36, s4, v2
v_mac_f32 v36, s5, v3
v_mac_f32 v36, s6, v4
v_mul_f32 v37, s8, v2
v_mac_f32 v37, s9, v3
v_mac_f32 v37, s10, v4
v_mul_f32 v38, s12, v2
v_mac_f32 v38, s13, v3
v_mac_f32 v38, s14, v4
v_add_f32 v36, s7, v36
v_add_f32 v37, s11, v37
v_add_f32 v38, s15, v38
v_mul_f32 v39, s4, v6
v_mac_f32 v39, s5, v7
v_mac_f32 v39, s6, v8
v_mul_f32 v40, s8, v6
v_mac_f32 v40, s9, v7
v_mac_f32 v40, s10, v8
v_mul_f32 v41, s12, v6
v_mac_f32 v41, s13, v7
v_mac_f32 v41, s14, v8
v_mul_f32 v42, s4, v9
v_mac_f32 v42, s5, v46
v_mac_f32 v42, s6, v47
v_mul_f32 v43, s8, v9
v_mac_f32 v43, s9, v46
v_mac_f32 v43, s10, v47
v_mul_f32 v48, s12, v9
v_mac_f32 v48, s13, v46
v_mac_f32 v48, s14, v47
v_mul_f32 v28, v12, v36
v_mac_f32 v28, v13, v37
v_mac_f32 v28, v14, v38
v_add_f32 v28, v15, v28
v_mul_f32 v29, v16, v36
v_mac_f32 v29, v17, v37
v_mac_f32 v29, v18, v38
v_add_f32 v29, v19, v29
v_mul_f32 v30, v20, v36
v_mac_f32 v30, v21, v37
v_mac_f32 v30, v22, v38
v_add_f32 v30, v23, v30
v_mul_f32 v31, v24, v36
v_mac_f32 v31, v25, v37
v_mac_f32 v31, v26, v38
v_add_f32 v31, v27, v31
exp pos0 v28, v29, v30, v31 done
exp param0 v44, v45, v40, v41
exp param1 v36, v37, v38, v39
exp param2 v42, v43, v48, v5
s_endpgm
