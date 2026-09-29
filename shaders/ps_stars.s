s_mov_b32 m0, s2
s_load_dwordx4 s[4:7], s[0:1], 0x24
s_load_dwordx4 s[8:11], s[0:1], 0x14
v_interp_p1_f32 v4, v0, attr0.x
v_interp_p2_f32 v4, v1, attr0.x
v_interp_p1_f32 v5, v0, attr0.y
v_interp_p2_f32 v5, v1, attr0.y
v_interp_p1_f32 v6, v0, attr0.z
v_interp_p2_f32 v6, v1, attr0.z
v_mul_f32 v7, v4, v4
v_mac_f32 v7, v5, v5
v_sub_f32 v7, 1.0, v7
v_max_f32 v7, 0, v7
v_mul_f32 v7, v7, v7
v_mul_f32 v7, v7, v6
; hidden behind the moon: x (1 - cov), cov = clamp((r - d) * 540 + 0.5, 0, 1) - the moon disc's
; own coverage (ps_dark); pixel centre (v2, v3 = POS_X/Y_FLOAT) in aspect-corrected NDC:
; ((x - 960) / 540, (540 - y) / 540); moon desc[20..23] = (x, y, r^2, r)
v_subrev_f32 v8, 0x44700000, v2
v_mul_f32 v8, 0x3af2b9d6, v8
v_sub_f32 v9, 0x44070000, v3
v_mul_f32 v9, 0x3af2b9d6, v9
s_waitcnt lgkmcnt(0)
v_subrev_f32 v8, s8, v8
v_subrev_f32 v9, s9, v9
v_mul_f32 v8, v8, v8
v_mac_f32 v8, v9, v9
v_sqrt_f32 v8, v8
v_sub_f32 v8, s11, v8
v_mul_f32 v8, 0x44070000, v8
v_add_f32 v8, 0.5, v8
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_sub_f32 v8, 1.0, v8
v_mul_f32 v7, v7, v8
v_mul_f32 v8, s4, v7
v_mul_f32 v9, s5, v7
v_mul_f32 v10, s6, v7
v_mov_b32 v11, 0
v_cvt_pkrtz_f16_f32 v12, v8, v9
v_cvt_pkrtz_f16_f32 v13, v10, v11
exp mrt0 v12, v12, v13, v13 done compr vm
s_endpgm
