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
s_waitcnt vmcnt(0)
v_mac_f32 v8, s18, v12
v_mac_f32 v9, s18, v13
v_mac_f32 v10, s18, v14
v_mul_f32 v8, s19, v8
v_mul_f32 v9, s19, v9
v_mul_f32 v10, s19, v10
v_mov_b32 v11, 1.0
exp mrt0 v8, v9, v10, v11 done vm
s_endpgm
