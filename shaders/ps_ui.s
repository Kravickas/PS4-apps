; UI pass: the finished frame (linear, clamped; final pass) + frosted glass under the panel (the frame
; blurred: 240 x 135, binomial H V H V) + the UI texture, sRGB-encoded and dithered, to the display.
s_load_dwordx8 s[4:11], s[0:1], 0x0
s_load_dwordx4 s[12:15], s[0:1], 0x8
s_load_dwordx4 s[16:19], s[0:1], 0xc
s_load_dwordx8 s[20:27], s[0:1], 0x10
s_load_dwordx4 s[28:31], s[0:1], 0x18
s_load_dwordx4 s[32:35], s[0:1], 0x1c
s_load_dwordx4 s[36:39], s[0:1], 0x20
s_load_dwordx8 s[40:47], s[0:1], 0x24
s_load_dwordx4 s[48:51], s[0:1], 0x2c
s_waitcnt lgkmcnt(0)
v_mul_f32 v4, s16, v2
v_mul_f32 v5, s17, v3
image_sample_lz v[8:10], v[4:5], s[4:11], s[12:15] dmask:0x7
v_mul_f32 v20, 0x3d897143, v2
v_mac_f32 v20, 0x3bbf4590, v3
v_fract_f32 v20, v20
v_mul_f32 v20, 0x4253ee82, v20
v_fract_f32 v20, v20
v_subrev_f32 v20, 0.5, v20
v_mul_f32 v20, 0x3b808081, v20
; >>> panels: coverage m (v37) and edge e (v38) of the rounded rects (dwords 28..31 the options panel, 32..35 the time panel: centre x, y, half w, h in px; hidden = half -1e6), radius 16
v_mov_b32 v37, 0
v_mov_b32 v38, 0
v_subrev_f32 v39, s32, v2
v_max_f32 v39, v39, -v39
v_subrev_f32 v39, s34, v39
v_add_f32 v39, 0x41800000, v39
v_max_f32 v39, 0, v39
v_subrev_f32 v40, s33, v3
v_max_f32 v40, v40, -v40
v_subrev_f32 v40, s35, v40
v_add_f32 v40, 0x41800000, v40
v_max_f32 v40, 0, v40
v_mul_f32 v39, v39, v39
v_mac_f32 v39, v40, v40
v_sqrt_f32 v39, v39
v_add_f32 v39, 0xc1800000, v39
v_sub_f32 v40, 0.5, v39
v_max_f32 v40, 0, v40
v_min_f32 v40, 1.0, v40
v_max_f32 v37, v37, v40
v_add_f32 v40, 0x3f400000, v39
v_max_f32 v40, v40, -v40
v_sub_f32 v40, 1.0, v40
v_max_f32 v40, 0, v40
v_max_f32 v38, v38, v40
; the time panel (dwords 32..35): the same rounded rect, coverage and edge joined by max
v_subrev_f32 v39, s36, v2
v_max_f32 v39, v39, -v39
v_subrev_f32 v39, s38, v39
v_add_f32 v39, 0x41800000, v39
v_max_f32 v39, 0, v39
v_subrev_f32 v40, s37, v3
v_max_f32 v40, v40, -v40
v_subrev_f32 v40, s39, v40
v_add_f32 v40, 0x41800000, v40
v_max_f32 v40, 0, v40
v_mul_f32 v39, v39, v39
v_mac_f32 v39, v40, v40
v_sqrt_f32 v39, v39
v_add_f32 v39, 0xc1800000, v39
v_sub_f32 v40, 0.5, v39
v_max_f32 v40, 0, v40
v_min_f32 v40, 1.0, v40
v_max_f32 v37, v37, v40
v_add_f32 v40, 0x3f400000, v39
v_max_f32 v40, v40, -v40
v_sub_f32 v40, 1.0, v40
v_max_f32 v40, 0, v40
v_max_f32 v38, v38, v40
s_waitcnt vmcnt(0)
v_cmp_lt_f32 vcc, 0, v37
; SCC = any lane (no VCCZ: its status bit is unreliable on SI/CI)
s_or_b32 s52, vcc_lo, vcc_hi
s_cbranch_scc0 ui_frost_done
; glass = blurred frame x 0.62 + tint; frame += m (glass - frame) + 0.08 e
image_sample_lz v[40:42], v[4:5], s[20:27], s[28:31] dmask:0x7
s_waitcnt vmcnt(0)
v_mul_f32 v40, 0x3f1eb852, v40
v_add_f32 v40, 0x3c449ba6, v40
v_sub_f32 v40, v40, v8
v_mac_f32 v8, v40, v37
v_mac_f32 v8, 0x3da3d70a, v38
v_mul_f32 v41, 0x3f1eb852, v41
v_add_f32 v41, 0x3c449ba6, v41
v_sub_f32 v41, v41, v9
v_mac_f32 v9, v41, v37
v_mac_f32 v9, 0x3da3d70a, v38
v_mul_f32 v42, 0x3f1eb852, v42
v_add_f32 v42, 0x3c83126f, v42
v_sub_f32 v42, v42, v10
v_mac_f32 v10, v42, v37
v_mac_f32 v10, 0x3da3d70a, v38
ui_frost_done:
; <<< panel
v_max_f32 v8, 0, v8
v_min_f32 v8, 1.0, v8
v_max_f32 v9, 0, v9
v_min_f32 v9, 1.0, v9
v_max_f32 v10, 0, v10
v_min_f32 v10, 1.0, v10
v_log_f32 v16, v8
v_mul_f32 v16, 0x3ed55555, v16
v_exp_f32 v16, v16
v_mul_f32 v16, 0x3f870a3d, v16
v_subrev_f32 v16, 0x3d6147ae, v16
v_mul_f32 v21, 0x414eb852, v8
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v8
v_cndmask_b32 v16, v16, v21, vcc
v_log_f32 v17, v9
v_mul_f32 v17, 0x3ed55555, v17
v_exp_f32 v17, v17
v_mul_f32 v17, 0x3f870a3d, v17
v_subrev_f32 v17, 0x3d6147ae, v17
v_mul_f32 v21, 0x414eb852, v9
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v9
v_cndmask_b32 v17, v17, v21, vcc
v_log_f32 v18, v10
v_mul_f32 v18, 0x3ed55555, v18
v_exp_f32 v18, v18
v_mul_f32 v18, 0x3f870a3d, v18
v_subrev_f32 v18, 0x3d6147ae, v18
v_mul_f32 v21, 0x414eb852, v10
v_cmp_ge_f32 vcc, 0x3b4d2e1c, v10
v_cndmask_b32 v18, v18, v21, vcc
; >>> ui content: premultiplied sRGB-space texture (dwords 36..43 T#, 44..47 point S#), panels only
v_cmp_lt_f32 vcc, 0, v37
; SCC = any lane (no VCCZ: its status bit is unreliable on SI/CI)
s_or_b32 s52, vcc_lo, vcc_hi
s_cbranch_scc0 ui_content_done
image_sample_lz v[40:43], v[4:5], s[40:47], s[48:51] dmask:0xf
s_waitcnt vmcnt(0)
v_sub_f32 v39, 1.0, v43
v_mul_f32 v16, v16, v39
v_add_f32 v16, v16, v40
v_mul_f32 v17, v17, v39
v_add_f32 v17, v17, v41
v_mul_f32 v18, v18, v39
v_add_f32 v18, v18, v42
ui_content_done:
; <<< ui content
; >>> opcode test (main.c OPCODE_TEST 1): two panels - [64] x0, [65] y0, [66] size, [67] stride (px; x0 -1e6:
;     off). Each pixel packs its UV into one dword ([68] / [69] bits a field: 8 left, 16 right) and unpacks
;     it through v_bfm_b32 masks, then samples [76] T# (a UNORM view: sRGB bytes as they are) with [84] S#.
;     [70..71] (1 << bits) - 1, [72..73] their reciprocals, [74] 1 / size. ISA: width / offset = S[4:0].
s_load_dwordx16 s[56:71], s[0:1], 0x40
s_load_dwordx8 s[72:79], s[0:1], 0x50
s_waitcnt lgkmcnt(0)
v_subrev_f32 v21, s56, v2
v_subrev_f32 v22, s57, v3
v_mov_b32 v23, s59
v_cmp_le_f32 vcc, v23, v21
v_cndmask_b32 v24, 0, v23, vcc
v_sub_f32 v25, v21, v24
v_sub_f32 v26, s58, v25
v_min_f32 v26, v26, v25
v_min_f32 v26, v26, v22
v_sub_f32 v27, s58, v22
v_min_f32 v26, v26, v27
v_min_f32 v26, v26, v21
v_cmp_lt_f32 vcc, 0, v26
; SCC = any lane inside a panel
s_or_b32 s53, vcc_lo, vcc_hi
s_cbranch_scc0 optest_done
v_mov_b32 v28, 1.0
v_cndmask_b32 v28, 0, v28, vcc
v_cmp_le_f32 vcc, v23, v21
v_mov_b32 v29, s60
v_mov_b32 v30, s61
v_cndmask_b32 v29, v29, v30, vcc
v_mov_b32 v30, s62
v_mov_b32 v31, s63
v_cndmask_b32 v30, v30, v31, vcc
v_mov_b32 v31, s64
v_mov_b32 v32, s65
v_cndmask_b32 v31, v31, v32, vcc
v_mul_f32 v33, s66, v25
v_mul_f32 v33, v33, v30
v_cvt_u32_f32 v33, v33
v_mul_f32 v34, s66, v22
v_mul_f32 v34, v34, v30
v_cvt_u32_f32 v34, v34
; packed = U << bits | V; unpacked through the masks ((1 << bits) - 1) << bits and (1 << bits) - 1
v_lshlrev_b32 v35, v29, v33
v_or_b32 v35, v35, v34
v_bfm_b32 v36, v29, v29
v_mov_b32 v37, 0
v_bfm_b32 v37, v29, v37
v_and_b32 v38, v35, v36
v_lshrrev_b32 v38, v29, v38
v_and_b32 v39, v35, v37
v_cvt_f32_u32 v38, v38
v_cvt_f32_u32 v39, v39
v_mul_f32 v38, v38, v31
v_mul_f32 v39, v39, v31
image_sample_lz v[40:42], v[38:39], s[68:75], s[76:79] dmask:0x7
s_waitcnt vmcnt(0)
v_cmp_lt_f32 vcc, 0, v28
v_cndmask_b32 v16, v16, v40, vcc
v_cndmask_b32 v17, v17, v41, vcc
v_cndmask_b32 v18, v18, v42, vcc
optest_done:
; <<< opcode test
v_add_f32 v16, v16, v20
v_add_f32 v17, v17, v20
v_add_f32 v18, v18, v20
v_mov_b32 v19, 1.0
exp mrt0 v16, v17, v18, v19 done vm
s_endpgm
