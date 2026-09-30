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
v_add_f32 v16, v16, v20
v_add_f32 v17, v17, v20
v_add_f32 v18, v18, v20
v_mov_b32 v19, 1.0
exp mrt0 v16, v17, v18, v19 done vm
s_endpgm
