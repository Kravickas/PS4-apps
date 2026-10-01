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
s_load_dwordx4 s[56:59], s[0:1], 0x30
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
; >>> panels: coverage m (v37) and edge e (v38) of the rounded rects (dwords 28..31 the options panel, 32..35 the time panel, 48..51 the OPCODE_TEST 3 panel: centre x, y, half w, h in px; hidden = half -1e6), radius 16
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
; the OPCODE_TEST 3 panel (dwords 48..51): the same
v_subrev_f32 v39, s56, v2
v_max_f32 v39, v39, -v39
v_subrev_f32 v39, s58, v39
v_add_f32 v39, 0x41800000, v39
v_max_f32 v39, 0, v39
v_subrev_f32 v40, s57, v3
v_max_f32 v40, v40, -v40
v_subrev_f32 v40, s59, v40
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
; >>> opcode test 2 (main.c OPCODE_TEST 2): [88] x0, [89] y0 (x0 -1e6: off), size / stride / 1 / size as test 1.
;     Each pixel's UV (16:16) sits in a 64-bit window {hi, lo} at bit sb with [92] junk above it, read
;     back with v_alignbit_b32 (left: s = (x + y) & [90], sb = s) or v_alignbyte_b32 (right: s = (x + y) &
;     [91], sb = 8 s). ISA: {S0, S1} >> S2[4:0] or >> 8 S2[1:0]. [93] 65535, [94] 1 / 65535.
s_load_dwordx8 s[80:87], s[0:1], 0x58
s_waitcnt lgkmcnt(0)
v_subrev_f32 v21, s80, v2
v_subrev_f32 v22, s81, v3
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
s_cbranch_scc0 optest2_done
v_mov_b32 v28, 1.0
v_cndmask_b32 v28, 0, v28, vcc
v_cmp_le_f32 vcc, v23, v21
v_mov_b32 v29, s82
v_mov_b32 v30, s83
v_cndmask_b32 v29, v29, v30, vcc
v_mov_b32 v30, 0
v_mov_b32 v31, 3
v_cndmask_b32 v30, v30, v31, vcc
v_add_f32 v31, v25, v22
v_cvt_u32_f32 v31, v31
v_and_b32 v31, v31, v29
v_lshlrev_b32 v32, v30, v31
v_mul_f32 v33, s66, v25
v_mul_f32 v33, s85, v33
v_cvt_u32_f32 v33, v33
v_mul_f32 v34, s66, v22
v_mul_f32 v34, s85, v34
v_cvt_u32_f32 v34, v34
v_lshlrev_b32 v33, 16, v33
v_or_b32 v33, v33, v34
; lo = packed << sb; hi = (packed >> 1) >> (31 - sb) | junk << sb (every shift in 0..31)
v_lshlrev_b32 v35, v32, v33
v_lshrrev_b32 v36, 1, v33
v_xor_b32 v37, 31, v32
v_lshrrev_b32 v36, v37, v36
v_mov_b32 v37, s84
v_lshlrev_b32 v37, v32, v37
v_or_b32 v36, v36, v37
v_alignbit_b32 v38, v36, v35, v31
v_alignbyte_b32 v39, v36, v35, v31
v_cndmask_b32 v38, v38, v39, vcc
v_and_b32 v39, 0xffff, v38
v_lshrrev_b32 v38, 16, v38
v_cvt_f32_u32 v38, v38
v_cvt_f32_u32 v39, v39
v_mul_f32 v38, s86, v38
v_mul_f32 v39, s86, v39
image_sample_lz v[40:42], v[38:39], s[68:75], s[76:79] dmask:0x7
s_waitcnt vmcnt(0)
v_cmp_lt_f32 vcc, 0, v28
v_cndmask_b32 v16, v16, v40, vcc
v_cndmask_b32 v17, v17, v41, vcc
v_cndmask_b32 v18, v18, v42, vcc
optest2_done:
; <<< opcode test 2
; >>> opcode test 3 (main.c OPCODE_TEST 3): V_CVT_PK_U8_F32 bit grid - [124] x0, [125] y0 (x0 -1e6: off),
;     [126] 1 / cell width, [127] 1 / cell height. 28 rows x 32 cells, the result's bits MSB left: white 1, grey 0
;     (alternate bytes darker; blue-grey in rows 20..27). Rows 0..19: S0 = [96 + row] (f32), S1 = 0,
;     S2 = 0; rows 20..27: S0 = 171.0, S1 = [96 + row], S2 = 0x11223344.
s_load_dwordx4 s[84:87], s[0:1], 0x7c
s_waitcnt lgkmcnt(0)
v_subrev_f32 v21, s84, v2
v_subrev_f32 v22, s85, v3
v_mul_f32 v23, s86, v21
v_mul_f32 v24, s87, v22
v_floor_f32 v25, v23
v_floor_f32 v26, v24
v_sub_f32 v27, 0x41f80000, v25
v_min_f32 v27, v27, v25
v_min_f32 v27, v27, v26
v_sub_f32 v28, 0x41d80000, v26
v_min_f32 v27, v27, v28
v_cmp_le_f32 vcc, 0, v27
; SCC = any lane inside the grid
s_or_b32 s53, vcc_lo, vcc_hi
s_cbranch_scc0 optest3_done
v_mov_b32 v28, 1.0
v_cndmask_b32 v28, 0, v28, vcc
s_load_dwordx16 s[56:71], s[0:1], 0x60
s_load_dwordx8 s[72:79], s[0:1], 0x70
s_load_dwordx4 s[80:83], s[0:1], 0x78
s_waitcnt lgkmcnt(0)
v_cvt_u32_f32 v29, v26
v_cvt_u32_f32 v30, v25
v_mov_b32 v31, 0
; v31 = [96 + row]
v_mov_b32 v32, s56
v_cmp_ne_u32 vcc, 0, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s57
v_cmp_ne_u32 vcc, 1, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s58
v_cmp_ne_u32 vcc, 2, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s59
v_cmp_ne_u32 vcc, 3, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s60
v_cmp_ne_u32 vcc, 4, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s61
v_cmp_ne_u32 vcc, 5, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s62
v_cmp_ne_u32 vcc, 6, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s63
v_cmp_ne_u32 vcc, 7, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s64
v_cmp_ne_u32 vcc, 8, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s65
v_cmp_ne_u32 vcc, 9, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s66
v_cmp_ne_u32 vcc, 10, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s67
v_cmp_ne_u32 vcc, 11, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s68
v_cmp_ne_u32 vcc, 12, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s69
v_cmp_ne_u32 vcc, 13, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s70
v_cmp_ne_u32 vcc, 14, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s71
v_cmp_ne_u32 vcc, 15, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s72
v_cmp_ne_u32 vcc, 16, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s73
v_cmp_ne_u32 vcc, 17, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s74
v_cmp_ne_u32 vcc, 18, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s75
v_cmp_ne_u32 vcc, 19, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s76
v_cmp_ne_u32 vcc, 20, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s77
v_cmp_ne_u32 vcc, 21, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s78
v_cmp_ne_u32 vcc, 22, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s79
v_cmp_ne_u32 vcc, 23, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s80
v_cmp_ne_u32 vcc, 24, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s81
v_cmp_ne_u32 vcc, 25, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s82
v_cmp_ne_u32 vcc, 26, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v32, s83
v_cmp_ne_u32 vcc, 27, v29
v_cndmask_b32 v31, v32, v31, vcc
v_cmp_gt_u32 vcc, 20, v29
v_mov_b32 v32, 0x432b0000
v_cndmask_b32 v32, v32, v31, vcc
v_mov_b32 v33, 0
v_cndmask_b32 v33, v31, v33, vcc
v_mov_b32 v34, 0x11223344
v_cndmask_b32 v34, v34, v33, vcc
v_cvt_pk_u8_f32 v35, v32, v33, v34
v_xor_b32 v36, 31, v30
v_lshrrev_b32 v36, v36, v35
v_and_b32 v36, 1, v36
; grey: 1 -> 0.93, 0 -> 0.32 / 0.22 (odd bytes); blue +0.18 for 0 bits in rows 20..27; cell border 0.05
v_lshrrev_b32 v37, 3, v30
v_and_b32 v37, 1, v37
v_cvt_f32_u32 v37, v37
v_mul_f32 v37, 0xbdcccccd, v37
v_add_f32 v37, 0x3ea3d70a, v37
v_mov_b32 v38, 0x3f6e147b
v_cmp_ne_u32 vcc, 0, v36
v_cndmask_b32 v39, v37, v38, vcc
v_mov_b32 v40, 0x3e3851ec
v_mov_b32 v42, 0
v_cmp_gt_u32 vcc, 20, v29
v_cndmask_b32 v41, v40, v42, vcc
v_cmp_ne_u32 vcc, 0, v36
v_cndmask_b32 v41, v41, v42, vcc
v_add_f32 v40, v39, v41
v_sub_f32 v37, v23, v25
v_sub_f32 v38, 1.0, v37
v_min_f32 v37, v37, v38
v_sub_f32 v38, v24, v26
v_min_f32 v37, v37, v38
v_sub_f32 v38, 1.0, v38
v_min_f32 v37, v37, v38
v_cmp_gt_f32 vcc, 0x3dcccccd, v37
v_mov_b32 v38, 0x3d4ccccd
v_cndmask_b32 v39, v39, v38, vcc
v_cndmask_b32 v40, v40, v38, vcc
v_cmp_lt_f32 vcc, 0, v28
v_cndmask_b32 v16, v16, v39, vcc
v_cndmask_b32 v17, v17, v39, vcc
v_cndmask_b32 v18, v18, v40, vcc
optest3_done:
; <<< opcode test 3
; >>> opcode test 4 (main.c OPCODE_TEST 4): the bit grid of test 3 for 35 rows - [252] x0, [253] y0 (x0 -1e6:
;     off), [254] 1 / cell width, [255] 1 / cell height. Each row runs one instruction on operands from
;     [128..212]: V_CVT_PK_U16_U32, V_CVT_PK_I16_I32, V_CVT_PKNORM_U16_F32, V_CVT_PKNORM_I16_F32 (rows 0..19),
;     S_BITSET1_B64 / S_BITSET0_B64, V_ASHR_I64 (high, low dword), V_CMPX_EQ_U64 / NE_U64 / EQ_I64 (bits 0..5
;     EXEC after, 8..13 VCC).
s_load_dwordx4 s[84:87], s[0:1], 0xfc
s_waitcnt lgkmcnt(0)
v_subrev_f32 v21, s84, v2
v_subrev_f32 v22, s85, v3
v_mul_f32 v23, s86, v21
v_mul_f32 v24, s87, v22
v_floor_f32 v25, v23
v_floor_f32 v26, v24
v_sub_f32 v27, 0x41f80000, v25
v_min_f32 v27, v27, v25
v_min_f32 v27, v27, v26
v_sub_f32 v28, 0x42080000, v26
v_min_f32 v27, v27, v28
v_cmp_le_f32 vcc, 0, v27
; SCC = any lane inside the grid
s_or_b32 s53, vcc_lo, vcc_hi
s_cbranch_scc0 optest4_done
v_mov_b32 v28, 1.0
v_cndmask_b32 v28, 0, v28, vcc
v_cvt_u32_f32 v29, v26
v_cvt_u32_f32 v30, v25
v_mov_b32 v31, 0
; v31 = the result of the row's instruction on its operands ([128..212])
; row 0: PK_U16 0x1234, 0xABCD
s_load_dwordx4 s[56:59], s[0:1], 0x80
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_u16_u32 v32, s56, v34
v_cmp_ne_u32 vcc, 0, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 1: PK_U16 0xFFFF, 0x10000
s_load_dwordx4 s[56:59], s[0:1], 0x82
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_u16_u32 v32, s56, v34
v_cmp_ne_u32 vcc, 1, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 2: PK_U16 70000, 0xFFFFFFFF
s_load_dwordx4 s[56:59], s[0:1], 0x84
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_u16_u32 v32, s56, v34
v_cmp_ne_u32 vcc, 2, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 3: PK_U16 0x80000000, 1
s_load_dwordx4 s[56:59], s[0:1], 0x86
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_u16_u32 v32, s56, v34
v_cmp_ne_u32 vcc, 3, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 4: PK_I16 1, -1
s_load_dwordx4 s[56:59], s[0:1], 0x88
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_i16_i32 v32, s56, v34
v_cmp_ne_u32 vcc, 4, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 5: PK_I16 32767, 32768
s_load_dwordx4 s[56:59], s[0:1], 0x8a
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_i16_i32 v32, s56, v34
v_cmp_ne_u32 vcc, 5, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 6: PK_I16 -32768, -32769
s_load_dwordx4 s[56:59], s[0:1], 0x8c
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_i16_i32 v32, s56, v34
v_cmp_ne_u32 vcc, 6, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 7: PK_I16 0x7FFFFFFF, 0x80000000
s_load_dwordx4 s[56:59], s[0:1], 0x8e
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pk_i16_i32 v32, s56, v34
v_cmp_ne_u32 vcc, 7, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 8: PKNORM_U16 0.0, 1.0
s_load_dwordx4 s[56:59], s[0:1], 0x90
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 8, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 9: PKNORM_U16 0.25, 0.75
s_load_dwordx4 s[56:59], s[0:1], 0x92
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 9, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 10: PKNORM_U16 -0.5, 1.5
s_load_dwordx4 s[56:59], s[0:1], 0x94
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 10, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 11: PKNORM_U16 NaN, +inf
s_load_dwordx4 s[56:59], s[0:1], 0x96
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 11, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 12: PKNORM_U16 0.5/65535, 1.5/65535
s_load_dwordx4 s[56:59], s[0:1], 0x98
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 12, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 13: PKNORM_U16 2.5/65535, 32767.5/65535
s_load_dwordx4 s[56:59], s[0:1], 0x9a
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_u16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 13, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 14: PKNORM_I16 0.0, 1.0
s_load_dwordx4 s[56:59], s[0:1], 0x9c
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 14, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 15: PKNORM_I16 -1.0, 0.5
s_load_dwordx4 s[56:59], s[0:1], 0x9e
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 15, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 16: PKNORM_I16 -1.5, 2.0
s_load_dwordx4 s[56:59], s[0:1], 0xa0
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 16, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 17: PKNORM_I16 NaN, -inf
s_load_dwordx4 s[56:59], s[0:1], 0xa2
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 17, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 18: PKNORM_I16 0.5/32767, 1.5/32767
s_load_dwordx4 s[56:59], s[0:1], 0xa4
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 18, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 19: PKNORM_I16 -0.5/32767, -2.5/32767
s_load_dwordx4 s[56:59], s[0:1], 0xa6
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s57
v_cvt_pknorm_i16_f32 v32, s56, v34
v_cmp_ne_u32 vcc, 19, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 20: BITSET1_B64 0, 40  hi
s_load_dwordx4 s[56:59], s[0:1], 0xa8
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset1_b64 s[66:67], s58
v_mov_b32 v32, s67
v_cmp_ne_u32 vcc, 20, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 21: BITSET1_B64 0, 40  lo
s_load_dwordx4 s[56:59], s[0:1], 0xa8
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset1_b64 s[66:67], s58
v_mov_b32 v32, s66
v_cmp_ne_u32 vcc, 21, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 22: BITSET1_B64 0, 70  hi
s_load_dwordx4 s[56:59], s[0:1], 0xab
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset1_b64 s[66:67], s58
v_mov_b32 v32, s67
v_cmp_ne_u32 vcc, 22, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 23: BITSET1_B64 0, 70  lo
s_load_dwordx4 s[56:59], s[0:1], 0xab
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset1_b64 s[66:67], s58
v_mov_b32 v32, s66
v_cmp_ne_u32 vcc, 23, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 24: BITSET0_B64 ~0, 33  hi
s_load_dwordx4 s[56:59], s[0:1], 0xae
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset0_b64 s[66:67], s58
v_mov_b32 v32, s67
v_cmp_ne_u32 vcc, 24, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 25: BITSET0_B64 ~0, 33  lo
s_load_dwordx4 s[56:59], s[0:1], 0xae
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset0_b64 s[66:67], s58
v_mov_b32 v32, s66
v_cmp_ne_u32 vcc, 25, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 26: BITSET0_B64 ~0, 0xFFFFFFC0  hi
s_load_dwordx4 s[56:59], s[0:1], 0xb1
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset0_b64 s[66:67], s58
v_mov_b32 v32, s67
v_cmp_ne_u32 vcc, 26, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 27: BITSET0_B64 ~0, 0xFFFFFFC0  lo
s_load_dwordx4 s[56:59], s[0:1], 0xb1
s_waitcnt lgkmcnt(0)
s_mov_b64 s[66:67], s[56:57]
s_bitset0_b64 s[66:67], s58
v_mov_b32 v32, s66
v_cmp_ne_u32 vcc, 27, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 28: ASHR_I64 0x80000000_00000000 >> 36  hi
s_load_dwordx4 s[56:59], s[0:1], 0xb4
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_mov_b32 v32, v33
v_cmp_ne_u32 vcc, 28, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 29: ASHR_I64 0x80000000_00000000 >> 36  lo
s_load_dwordx4 s[56:59], s[0:1], 0xb4
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_cmp_ne_u32 vcc, 29, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 30: ASHR_I64 0x7FFFFFFF_FFFFFFFF >> 68  hi
s_load_dwordx4 s[56:59], s[0:1], 0xb7
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_mov_b32 v32, v33
v_cmp_ne_u32 vcc, 30, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 31: ASHR_I64 0x7FFFFFFF_FFFFFFFF >> 68  lo
s_load_dwordx4 s[56:59], s[0:1], 0xb7
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_cmp_ne_u32 vcc, 31, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 32: ASHR_I64 0x87654321_12345678 >> 0  hi
s_load_dwordx4 s[56:59], s[0:1], 0xba
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_mov_b32 v32, v33
v_cmp_ne_u32 vcc, 32, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 33: ASHR_I64 0x87654321_12345678 >> 0  lo
s_load_dwordx4 s[56:59], s[0:1], 0xba
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_ashr_i64 v[32:33], v[34:35], v36
v_cmp_ne_u32 vcc, 33, v29
v_cndmask_b32 v31, v32, v31, vcc
; row 34: CMPX_*_64: bits 0-5 EXEC, 8-13 VCC
v_mov_b32 v32, 0
s_load_dwordx4 s[56:59], s[0:1], 0xbd
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_eq_u64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 0, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 8, v33
v_or_b32 v32, v32, v33
s_load_dwordx4 s[56:59], s[0:1], 0xc1
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_eq_u64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 1, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 9, v33
v_or_b32 v32, v32, v33
s_load_dwordx4 s[56:59], s[0:1], 0xc5
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_ne_u64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 2, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 10, v33
v_or_b32 v32, v32, v33
s_load_dwordx4 s[56:59], s[0:1], 0xc9
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_ne_u64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 3, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 11, v33
v_or_b32 v32, v32, v33
s_load_dwordx4 s[56:59], s[0:1], 0xcd
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_eq_i64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 4, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 12, v33
v_or_b32 v32, v32, v33
s_load_dwordx4 s[56:59], s[0:1], 0xd1
s_waitcnt lgkmcnt(0)
v_mov_b32 v34, s56
v_mov_b32 v35, s57
v_mov_b32 v36, s58
v_mov_b32 v37, s59
s_mov_b64 s[60:61], exec
v_cmpx_eq_i64 vcc, v[34:35], v[36:37]
s_mov_b64 s[62:63], exec
s_mov_b64 s[64:65], vcc
s_mov_b64 exec, s[60:61]
v_cndmask_b32_e64 v33, 0, 1, s[62:63]
v_lshlrev_b32 v33, 5, v33
v_or_b32 v32, v32, v33
v_cndmask_b32_e64 v33, 0, 1, s[64:65]
v_lshlrev_b32 v33, 13, v33
v_or_b32 v32, v32, v33
v_cmp_ne_u32 vcc, 34, v29
v_cndmask_b32 v31, v32, v31, vcc
v_mov_b32 v35, v31
v_xor_b32 v36, 31, v30
v_lshrrev_b32 v36, v36, v35
v_and_b32 v36, 1, v36
; grey: 1 -> 0.93, 0 -> 0.32 / 0.22 (odd bytes); blue +0.18 for 0 bits in rows 20..27; cell border 0.05
v_lshrrev_b32 v37, 3, v30
v_and_b32 v37, 1, v37
v_cvt_f32_u32 v37, v37
v_mul_f32 v37, 0xbdcccccd, v37
v_add_f32 v37, 0x3ea3d70a, v37
v_mov_b32 v38, 0x3f6e147b
v_cmp_ne_u32 vcc, 0, v36
v_cndmask_b32 v39, v37, v38, vcc
v_mov_b32 v40, 0x3e3851ec
v_mov_b32 v42, 0
v_cmp_gt_u32 vcc, 20, v29
v_cndmask_b32 v41, v40, v42, vcc
v_cmp_ne_u32 vcc, 0, v36
v_cndmask_b32 v41, v41, v42, vcc
v_add_f32 v40, v39, v41
v_sub_f32 v37, v23, v25
v_sub_f32 v38, 1.0, v37
v_min_f32 v37, v37, v38
v_sub_f32 v38, v24, v26
v_min_f32 v37, v37, v38
v_sub_f32 v38, 1.0, v38
v_min_f32 v37, v37, v38
v_cmp_gt_f32 vcc, 0x3dcccccd, v37
v_mov_b32 v38, 0x3d4ccccd
v_cndmask_b32 v39, v39, v38, vcc
v_cndmask_b32 v40, v40, v38, vcc
v_cmp_lt_f32 vcc, 0, v28
v_cndmask_b32 v16, v16, v39, vcc
v_cndmask_b32 v17, v17, v39, vcc
v_cndmask_b32 v18, v18, v40, vcc
optest4_done:
; <<< opcode test 4
; >>> opcode test 5 (main.c OPCODE_TEST 5): the V_CVT table - [52] on (0: off). The results of ps_cvt_a / ps_cvt_b
;     ([132] V#: 48 bytes a slot, A lo, A hi, B lo, B hi first) as hex digits from the [136] T# strip ([144] S#; 16 cells of [151] px,
;     white with coverage in alpha); slot ops from [128] V# (0xff: a heading, bit 8: a 64-bit result, 16 digits).
;     Rows from y [148], pitch [149] ([150] = 1 / pitch); [152] 1 / [151]; strip scale [153] u, [154] v; [155] gap
;     between the A and B fields; columns [156 + 4k] = A field x, B field x, first slot, slots (k = 0..4).
s_load_dword s56, s[0:1], 0x34
s_waitcnt lgkmcnt(0)
s_cmp_eq_u32 s56, 0
s_cbranch_scc1 optest5_done
s_load_dwordx16 s[56:71], s[0:1], 0x94
s_load_dwordx8 s[72:79], s[0:1], 0xa4
s_load_dwordx4 s[80:83], s[0:1], 0xac
s_waitcnt lgkmcnt(0)
; v21 = y in the table, v22 = row slot in a column, v23 = y in the row
v_subrev_f32 v21, s56, v3
v_mul_f32 v22, s58, v21
v_floor_f32 v22, v22
v_mul_f32 v23, s57, v22
v_sub_f32 v23, v21, v23
v_add_f32 v27, 0.5, v22
; hit (v35): x in a field (v29) of A (v30 = 0) or B (1.0), slot (v34)
v_mov_b32 v31, 0
v_mov_b32 v32, 1.0
v_mov_b32 v29, 0
v_mov_b32 v30, 0
v_mov_b32 v34, 0
v_mov_b32 v35, 0
; column 0
v_mov_b32 v25, s65
v_subrev_f32 v25, s64, v25
v_subrev_f32 v25, s63, v25
v_mov_b32 v28, s67
v_subrev_f32 v28, v22, v28
v_add_f32 v28, -0.5, v28
v_min_f32 v28, v28, v27
v_mov_b32 v33, s66
v_add_f32 v33, v33, v22
v_subrev_f32 v24, s64, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v31, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_subrev_f32 v24, s65, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v32, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
; column 1
v_mov_b32 v25, s69
v_subrev_f32 v25, s68, v25
v_subrev_f32 v25, s63, v25
v_mov_b32 v28, s71
v_subrev_f32 v28, v22, v28
v_add_f32 v28, -0.5, v28
v_min_f32 v28, v28, v27
v_mov_b32 v33, s70
v_add_f32 v33, v33, v22
v_subrev_f32 v24, s68, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v31, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_subrev_f32 v24, s69, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v32, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
; column 2
v_mov_b32 v25, s73
v_subrev_f32 v25, s72, v25
v_subrev_f32 v25, s63, v25
v_mov_b32 v28, s75
v_subrev_f32 v28, v22, v28
v_add_f32 v28, -0.5, v28
v_min_f32 v28, v28, v27
v_mov_b32 v33, s74
v_add_f32 v33, v33, v22
v_subrev_f32 v24, s72, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v31, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_subrev_f32 v24, s73, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v32, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
; column 3
v_mov_b32 v25, s77
v_subrev_f32 v25, s76, v25
v_subrev_f32 v25, s63, v25
v_mov_b32 v28, s79
v_subrev_f32 v28, v22, v28
v_add_f32 v28, -0.5, v28
v_min_f32 v28, v28, v27
v_mov_b32 v33, s78
v_add_f32 v33, v33, v22
v_subrev_f32 v24, s76, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v31, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_subrev_f32 v24, s77, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v32, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
; column 4
v_mov_b32 v25, s81
v_subrev_f32 v25, s80, v25
v_subrev_f32 v25, s63, v25
v_mov_b32 v28, s83
v_subrev_f32 v28, v22, v28
v_add_f32 v28, -0.5, v28
v_min_f32 v28, v28, v27
v_mov_b32 v33, s82
v_add_f32 v33, v33, v22
v_subrev_f32 v24, s80, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v31, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_subrev_f32 v24, s81, v2
v_sub_f32 v26, v25, v24
v_min_f32 v26, v26, v24
v_min_f32 v26, v26, v28
v_cmp_lt_f32 vcc, 0, v26
v_cndmask_b32 v29, v29, v24, vcc
v_cndmask_b32 v30, v30, v32, vcc
v_cndmask_b32 v34, v34, v33, vcc
v_cndmask_b32 v35, v35, v32, vcc
v_cmp_lt_f32 vcc, 0, v35
; SCC = any lane in a field
s_or_b32 s53, vcc_lo, vcc_hi
s_cbranch_scc0 optest5_done
s_mov_b32 s84, s59
s_mov_b32 s85, s60
s_mov_b32 s86, s61
s_mov_b32 s87, s62
s_load_dwordx16 s[56:71], s[0:1], 0x80
s_load_dwordx4 s[72:75], s[0:1], 0x90
s_waitcnt lgkmcnt(0)
v_cvt_u32_f32 v36, v34
v_lshlrev_b32 v36, 4, v36
v_lshlrev_b32 v24, 1, v36
v_add_i32 v24, vcc, v24, v36
buffer_load_dword v37, v36, s[56:59], 0 offen
buffer_load_dwordx4 v[38:41], v24, s[60:63], 0 offen
s_waitcnt vmcnt(0)
; a heading has no digits; op bit 8 (a 64-bit result) gives 16 digits (v42 = digit count), others 8
v_cmp_eq_u32 vcc, 0xff, v37
v_cndmask_b32 v35, v35, v31, vcc
v_and_b32 v42, 0x100, v37
v_cmp_ne_u32 vcc, 0, v42
v_mov_b32 v42, 0x41000000
v_mov_b32 v43, 0x41800000
v_cndmask_b32 v42, v42, v43, vcc
; v43 = digit, past the count: no digit
v_mul_f32 v43, s85, v29
v_floor_f32 v43, v43
v_cmp_le_f32 vcc, v42, v43
v_cndmask_b32 v35, v35, v31, vcc
; the field's value (A or B), the digit's dword (v38) and nibble
v_cmp_lt_f32 vcc, 0, v30
v_cndmask_b32 v38, v38, v40, vcc
v_cndmask_b32 v39, v39, v41, vcc
v_sub_f32 v40, 0x41800000, v42
v_add_f32 v40, v40, v43
v_cmp_gt_f32 vcc, 0x41000000, v40
v_cndmask_b32 v38, v38, v39, vcc
v_cvt_u32_f32 v41, v40
v_and_b32 v41, 7, v41
v_xor_b32 v41, 7, v41
v_lshlrev_b32 v41, 2, v41
v_lshrrev_b32 v38, v41, v38
v_and_b32 v38, 15, v38
; strip coordinates: u = (nibble * cell + x in the cell) * [153], v = y in the row * [154]
v_mul_f32 v41, s84, v43
v_sub_f32 v41, v29, v41
v_cvt_f32_u32 v38, v38
v_mul_f32 v38, s84, v38
v_add_f32 v38, v38, v41
v_mul_f32 v38, s86, v38
v_mul_f32 v39, s87, v23
image_sample_lz v[40:43], v[38:39], s[64:71], s[72:75] dmask:0xf
s_waitcnt vmcnt(0)
v_cmp_lt_f32 vcc, 0, v35
v_cndmask_b32 v43, v31, v43, vcc
; white over the panel by the glyph's coverage
v_sub_f32 v40, 0x3f733333, v16
v_mul_f32 v40, v40, v43
v_add_f32 v16, v16, v40
v_sub_f32 v40, 0x3f733333, v17
v_mul_f32 v40, v40, v43
v_add_f32 v17, v17, v40
v_sub_f32 v40, 0x3f733333, v18
v_mul_f32 v40, v40, v43
v_add_f32 v18, v18, v40
optest5_done:
; <<< opcode test 5
v_add_f32 v16, v16, v20
v_add_f32 v17, v17, v20
v_add_f32 v18, v18, v20
v_mov_b32 v19, 1.0
exp mrt0 v16, v17, v18, v19 done vm
s_endpgm
