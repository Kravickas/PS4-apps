s_mov_b32 m0, s2
s_load_dwordx4 s[4:7], s[0:1], 0x24
v_interp_p1_f32 v2, v0, attr0.x
v_interp_p2_f32 v2, v1, attr0.x
v_interp_p1_f32 v3, v0, attr0.y
v_interp_p2_f32 v3, v1, attr0.y
v_interp_p1_f32 v4, v0, attr0.z
v_interp_p2_f32 v4, v1, attr0.z
v_mul_f32 v5, v2, v2
v_mac_f32 v5, v3, v3
v_sub_f32 v5, 1.0, v5
v_max_f32 v5, 0, v5
v_mul_f32 v5, v5, v5
v_mul_f32 v5, v5, v4
s_waitcnt lgkmcnt(0)
v_mul_f32 v6, s4, v5
v_mul_f32 v7, s5, v5
v_mul_f32 v8, s6, v5
v_mov_b32 v9, 0
v_cvt_pkrtz_f16_f32 v10, v6, v7
v_cvt_pkrtz_f16_f32 v11, v8, v9
exp mrt0 v10, v10, v11, v11 done compr vm
s_endpgm
