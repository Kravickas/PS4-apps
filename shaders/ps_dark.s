s_mov_b32 m0, s2
s_load_dwordx4 s[4:7], s[0:1], 0x10
s_load_dwordx4 s[8:11], s[0:1], 0x18
s_load_dwordx4 s[12:15], s[0:1], 0x1c
s_load_dwordx4 s[16:19], s[0:1], 0x54
s_load_dwordx4 s[20:23], s[0:1], 0x14
s_load_dwordx4 s[24:27], s[0:1], 0x58
v_interp_p1_f32 v2, v0, attr0.x
v_interp_p2_f32 v2, v1, attr0.x
v_interp_p1_f32 v3, v0, attr0.y
v_interp_p2_f32 v3, v1, attr0.y
s_waitcnt lgkmcnt(0)
v_sub_f32 v4, 1.0, v3
v_mul_f32 v4, 0.5, v4
v_mov_b32 v5, s12
v_subrev_f32 v5, s8, v5
v_mul_f32 v5, v5, v4
v_add_f32 v5, s8, v5
v_mov_b32 v6, s13
v_subrev_f32 v6, s9, v6
v_mul_f32 v6, v6, v4
v_add_f32 v6, s9, v6
v_mov_b32 v7, s14
v_subrev_f32 v7, s10, v7
v_mul_f32 v7, v7, v4
v_add_f32 v7, s10, v7
v_subrev_f32 v8, s4, v2
v_subrev_f32 v9, s5, v3
v_mul_f32 v8, v8, v8
v_mac_f32 v8, v9, v9
v_rcp_f32 v9, s6
v_mul_f32 v8, v8, v9
v_sub_f32 v8, 1.0, v8
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_mul_f32 v8, v8, v8
v_sub_f32 v10, s16, v5
v_mac_f32 v5, v10, v8
v_sub_f32 v10, s17, v6
v_mac_f32 v6, v10, v8
v_sub_f32 v10, s18, v7
v_mac_f32 v7, v10, v8
; moon: flat disc (the full moon shows almost no limb darkening) with a one-pixel anti-aliased
; edge: cov = clamp((r - d) * 540 + 0.5, 0, 1), d and r in aspect-corrected NDC (1 px = 1/540);
; r = desc[23] (s23)
v_subrev_f32 v8, s20, v2
v_subrev_f32 v9, s21, v3
v_mul_f32 v8, v8, v8
v_mac_f32 v8, v9, v9
v_sqrt_f32 v8, v8
v_sub_f32 v8, s23, v8
v_mul_f32 v8, 0x44070000, v8
v_add_f32 v8, 0.5, v8
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_sub_f32 v10, s24, v5
v_mac_f32 v5, v10, v8
v_sub_f32 v10, s25, v6
v_mac_f32 v6, v10, v8
v_sub_f32 v10, s26, v7
v_mac_f32 v7, v10, v8
v_mov_b32 v11, 1.0
exp mrt0 v5, v6, v7, v11 done vm
s_endpgm
