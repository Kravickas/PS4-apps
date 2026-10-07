; DS_ORDERED_COUNT test shaders (tools/gen_cs_ordcnt.py). GCN2 / Sea Islands.
; ---- variant plain ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 plain_m3
s_bfe_u32 s43, s6, 0xc0006
plain_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 plain_m2
s_add_u32 s43, s43, 1
plain_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 plain_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
plain_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 plain_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch plain_pre_loop
plain_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s37
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_add_rtn_u32 v5, v11, v1 gds
.long 0xd8820000, 0x0500010b
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
plain_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant a ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 a_m3
s_bfe_u32 s43, s6, 0xc0006
a_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 a_m2
s_add_u32 s43, s43, 1
a_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 a_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
a_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 a_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch a_pre_loop
a_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
a_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant b ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 b_m3
s_bfe_u32 s43, s6, 0xc0006
b_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 b_m2
s_add_u32 s43, s43, 1
b_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 b_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
b_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 b_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch b_pre_loop
b_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
b_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 b_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch b_mid_loop
b_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
b_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant c ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 c_m3
s_bfe_u32 s43, s6, 0xc0006
c_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 c_m2
s_add_u32 s43, s43, 1
c_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 c_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
c_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c_pre_loop
c_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
c_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c_mid_loop
c_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
c_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant d ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 d_m3
s_bfe_u32 s43, s6, 0xc0006
d_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 d_m2
s_add_u32 s43, s43, 1
d_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 d_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
d_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 d_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch d_pre_loop
d_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 swap gds
.long 0xd8fe1300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
d_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant e1 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 e1_m3
s_bfe_u32 s43, s6, 0xc0006
e1_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 e1_m2
s_add_u32 s43, s43, 1
e1_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 e1_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
e1_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 e1_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch e1_pre_loop
e1_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v7, data0 v1, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000107
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
e1_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant e2 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 e2_m3
s_bfe_u32 s43, s6, 0xc0006
e2_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 e2_m2
s_add_u32 s43, s43, 1
e2_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 e2_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
e2_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 e2_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch e2_pre_loop
e2_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v7, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000701
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
e2_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant f1 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 f1_m3
s_bfe_u32 s43, s6, 0xc0006
f1_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 f1_m2
s_add_u32 s43, s43, 1
f1_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 f1_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
f1_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 f1_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch f1_pre_loop
f1_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
f1_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant f2 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 f2_m3
s_bfe_u32 s43, s6, 0xc0006
f2_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 f2_m2
s_add_u32 s43, s43, 1
f2_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 f2_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
f2_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 f2_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch f2_pre_loop
f2_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 2 rel 1 done 1 type 0 add gds
.long 0xd8fe0308, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
f2_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant f3 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 f3_m3
s_bfe_u32 s43, s6, 0xc0006
f3_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 f3_m2
s_add_u32 s43, s43, 1
f3_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 f3_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
f3_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 f3_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch f3_pre_loop
f3_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 3 rel 1 done 1 type 0 add gds
.long 0xd8fe030c, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
f3_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant g ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 g_m3
s_bfe_u32 s43, s6, 0xc0006
g_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 g_m2
s_add_u32 s43, s43, 1
g_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 g_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
g_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 g_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch g_pre_loop
g_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 1 rel 1 done 0 type 0 add gds
.long 0xd8fe0104, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
g_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 g_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch g_mid_loop
g_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
g_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r2 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r2_m3
s_bfe_u32 s43, s6, 0xc0006
r2_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r2_m2
s_add_u32 s43, s43, 1
r2_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r2_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r2_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r2_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r2_pre_loop
r2_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
r2_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r2_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r2_mid_loop
r2_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
r2_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r3 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r3_m3
s_bfe_u32 s43, s6, 0xc0006
r3_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r3_m2
s_add_u32 s43, s43, 1
r3_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r3_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r3_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r3_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r3_pre_loop
r3_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
r3_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r4 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r4_m3
s_bfe_u32 s43, s6, 0xc0006
r4_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r4_m2
s_add_u32 s43, s43, 1
r4_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r4_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r4_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r4_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r4_pre_loop
r4_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 4 rel 1 done 1 type 0 add gds
.long 0xd8fe0310, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
r4_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r5 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r5_m3
s_bfe_u32 s43, s6, 0xc0006
r5_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r5_m2
s_add_u32 s43, s43, 1
r5_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r5_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r5_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r5_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r5_pre_loop
r5_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 15 rel 1 done 1 type 0 add gds
.long 0xd8fe033c, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
r5_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r6 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r6_m3
s_bfe_u32 s43, s6, 0xc0006
r6_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r6_m2
s_add_u32 s43, s43, 1
r6_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r6_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r6_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r6_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r6_pre_loop
r6_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 1 add gds
.long 0xd8fe0700, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
r6_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant r7 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 r7_m3
s_bfe_u32 s43, s6, 0xc0006
r7_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 r7_m2
s_add_u32 s43, s43, 1
r7_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 r7_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
r7_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 r7_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch r7_pre_loop
r7_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
r7_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant h4 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 h4_m3
s_bfe_u32 s43, s6, 0xc0006
h4_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 h4_m2
s_add_u32 s43, s43, 1
h4_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 h4_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
h4_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 h4_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch h4_pre_loop
h4_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
h4_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 h4_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch h4_mid_loop
h4_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 4 rel 1 done 1 type 0 add gds
.long 0xd8fe0310, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
h4_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant c1 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 c1_m3
s_bfe_u32 s43, s6, 0xc0006
c1_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 c1_m2
s_add_u32 s43, s43, 1
c1_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 c1_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
c1_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c1_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c1_pre_loop
c1_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
c1_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c1_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c1_mid_loop
c1_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
c1_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant c4 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 c4_m3
s_bfe_u32 s43, s6, 0xc0006
c4_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 c4_m2
s_add_u32 s43, s43, 1
c4_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 c4_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
c4_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c4_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c4_pre_loop
c4_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
c4_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 c4_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch c4_mid_loop
c4_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 4 rel 1 done 1 type 0 add gds
.long 0xd8fe0310, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
c4_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_0 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_0_m3
s_bfe_u32 s43, s6, 0xc0006
k0_0_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_0_m2
s_add_u32 s43, s43, 1
k0_0_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_0_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_0_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_0_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_0_pre_loop
k0_0_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_0_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_0_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_0_mid_loop
k0_0_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_0_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_0_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_0_mid2_loop
k0_0_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_0_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_1 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_1_m3
s_bfe_u32 s43, s6, 0xc0006
k0_1_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_1_m2
s_add_u32 s43, s43, 1
k0_1_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_1_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_1_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_1_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_1_pre_loop
k0_1_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_1_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_1_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_1_mid_loop
k0_1_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 1 rel 1 done 0 type 0 add gds
.long 0xd8fe0104, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_1_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_1_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_1_mid2_loop
k0_1_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_1_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_2 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_2_m3
s_bfe_u32 s43, s6, 0xc0006
k0_2_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_2_m2
s_add_u32 s43, s43, 1
k0_2_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_2_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_2_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_2_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_2_pre_loop
k0_2_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_2_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_2_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_2_mid_loop
k0_2_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 2 rel 1 done 0 type 0 add gds
.long 0xd8fe0108, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_2_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_2_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_2_mid2_loop
k0_2_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_2_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_3 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_3_m3
s_bfe_u32 s43, s6, 0xc0006
k0_3_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_3_m2
s_add_u32 s43, s43, 1
k0_3_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_3_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_3_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_3_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_3_pre_loop
k0_3_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_3_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_3_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_3_mid_loop
k0_3_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 3 rel 1 done 0 type 0 add gds
.long 0xd8fe010c, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_3_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_3_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_3_mid2_loop
k0_3_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_3_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_4 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_4_m3
s_bfe_u32 s43, s6, 0xc0006
k0_4_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_4_m2
s_add_u32 s43, s43, 1
k0_4_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_4_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_4_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_4_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_4_pre_loop
k0_4_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_4_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_4_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_4_mid_loop
k0_4_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 4 rel 1 done 0 type 0 add gds
.long 0xd8fe0110, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_4_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_4_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_4_mid2_loop
k0_4_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_4_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_8 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_8_m3
s_bfe_u32 s43, s6, 0xc0006
k0_8_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_8_m2
s_add_u32 s43, s43, 1
k0_8_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_8_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_8_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_8_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_8_pre_loop
k0_8_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_8_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_8_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_8_mid_loop
k0_8_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 8 rel 1 done 0 type 0 add gds
.long 0xd8fe0120, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_8_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_8_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_8_mid2_loop
k0_8_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_8_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_16 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_16_m3
s_bfe_u32 s43, s6, 0xc0006
k0_16_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_16_m2
s_add_u32 s43, s43, 1
k0_16_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_16_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_16_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_16_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_16_pre_loop
k0_16_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_16_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_16_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_16_mid_loop
k0_16_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 16 rel 1 done 0 type 0 add gds
.long 0xd8fe0140, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_16_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_16_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_16_mid2_loop
k0_16_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_16_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_32 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_32_m3
s_bfe_u32 s43, s6, 0xc0006
k0_32_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_32_m2
s_add_u32 s43, s43, 1
k0_32_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_32_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_32_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_32_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_32_pre_loop
k0_32_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_32_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_32_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_32_mid_loop
k0_32_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 32 rel 1 done 0 type 0 add gds
.long 0xd8fe0180, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_32_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_32_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_32_mid2_loop
k0_32_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_32_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k0_63 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k0_63_m3
s_bfe_u32 s43, s6, 0xc0006
k0_63_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k0_63_m2
s_add_u32 s43, s43, 1
k0_63_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k0_63_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k0_63_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_63_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_63_pre_loop
k0_63_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k0_63_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_63_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_63_mid_loop
k0_63_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 63 rel 1 done 0 type 0 add gds
.long 0xd8fe01fc, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k0_63_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k0_63_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k0_63_mid2_loop
k0_63_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k0_63_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant k1_5 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 k1_5_m3
s_bfe_u32 s43, s6, 0xc0006
k1_5_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 k1_5_m2
s_add_u32 s43, s43, 1
k1_5_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 k1_5_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
k1_5_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k1_5_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k1_5_pre_loop
k1_5_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 1 rel 0 done 0 type 0 add gds
.long 0xd8fe0004, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
k1_5_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k1_5_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k1_5_mid_loop
k1_5_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 5 rel 1 done 0 type 0 add gds
.long 0xd8fe0114, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
k1_5_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 k1_5_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch k1_5_mid2_loop
k1_5_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
k1_5_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant n3 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 n3_m3
s_bfe_u32 s43, s6, 0xc0006
n3_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 n3_m2
s_add_u32 s43, s43, 1
n3_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 n3_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
n3_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 n3_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch n3_pre_loop
n3_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
n3_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 n3_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch n3_mid_loop
n3_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
n3_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 n3_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch n3_mid2_loop
n3_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
n3_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant sp ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 sp_m3
s_bfe_u32 s43, s6, 0xc0006
sp_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 sp_m2
s_add_u32 s43, s43, 1
sp_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 sp_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
sp_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 sp_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch sp_pre_loop
sp_pre_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 sp_odd0
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_branch sp_join0
sp_odd0:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
sp_join0:
sp_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant un ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 un_m3
s_bfe_u32 s43, s6, 0xc0006
un_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 un_m2
s_add_u32 s43, s43, 1
un_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 un_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
un_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 un_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch un_pre_loop
un_pre_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 un_odd0
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_branch un_join0
un_odd0:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
un_join0:
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
un_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 un_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch un_mid_loop
un_mid_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 un_odd1
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_branch un_join1
un_odd1:
un_join1:
un_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant dn ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 dn_m3
s_bfe_u32 s43, s6, 0xc0006
dn_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 dn_m2
s_add_u32 s43, s43, 1
dn_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 dn_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
dn_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 dn_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch dn_pre_loop
dn_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 0 done 1 type 0 add gds
.long 0xd8fe0200, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
dn_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant dd ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 dd_m3
s_bfe_u32 s43, s6, 0xc0006
dd_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 dd_m2
s_add_u32 s43, s43, 1
dd_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 dd_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
dd_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 dd_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch dd_pre_loop
dd_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
dd_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 dd_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch dd_mid_loop
dd_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
dd_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant ad ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 ad_m3
s_bfe_u32 s43, s6, 0xc0006
ad_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 ad_m2
s_add_u32 s43, s43, 1
ad_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 ad_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
ad_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 ad_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch ad_pre_loop
ad_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
ad_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 ad_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch ad_mid_loop
ad_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
ad_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant rnr ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 rnr_m3
s_bfe_u32 s43, s6, 0xc0006
rnr_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 rnr_m2
s_add_u32 s43, s43, 1
rnr_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 rnr_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
rnr_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 rnr_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch rnr_pre_loop
rnr_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
rnr_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 rnr_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch rnr_mid_loop
rnr_mid_done:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 0 done 0 type 0 add gds
.long 0xd8fe0000, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
rnr_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 rnr_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch rnr_mid2_loop
rnr_mid2_done:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
rnr_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant u31 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 u31_m3
s_bfe_u32 s43, s6, 0xc0006
u31_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 u31_m2
s_add_u32 s43, s43, 1
u31_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 u31_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
u31_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u31_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u31_pre_loop
u31_pre_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u31_odd0
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_branch u31_join0
u31_odd0:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
u31_join0:
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
u31_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u31_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u31_mid_loop
u31_mid_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u31_odd1
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_branch u31_join1
u31_odd1:
u31_join1:
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
u31_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u31_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u31_mid2_loop
u31_mid2_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u31_odd2
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
s_branch u31_join2
u31_odd2:
u31_join2:
u31_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant u13 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 u13_m3
s_bfe_u32 s43, s6, 0xc0006
u13_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 u13_m2
s_add_u32 s43, s43, 1
u13_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 u13_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
u13_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u13_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u13_pre_loop
u13_pre_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u13_odd0
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_branch u13_join0
u13_odd0:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
u13_join0:
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
u13_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u13_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u13_mid_loop
u13_mid_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u13_odd1
s_branch u13_join1
u13_odd1:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
u13_join1:
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
u13_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u13_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u13_mid2_loop
u13_mid2_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u13_odd2
s_branch u13_join2
u13_odd2:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
u13_join2:
u13_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant u23 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 u23_m3
s_bfe_u32 s43, s6, 0xc0006
u23_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 u23_m2
s_add_u32 s43, s43, 1
u23_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 u23_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
u23_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u23_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u23_pre_loop
u23_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
u23_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u23_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u23_mid_loop
u23_mid_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u23_odd1
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
s_branch u23_join1
u23_odd1:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
u23_join1:
s_and_b32 s46, s39, 0xffff
s_lshr_b32 s47, s39, 16
s_and_b32 s62, s41, s46
s_cmp_eq_u32 s62, s47
s_cselect_b32 s62, s38, 0
u23_mid2_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 u23_mid2_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch u23_mid2_loop
u23_mid2_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 u23_odd2
s_branch u23_join2
u23_odd2:
s_memtime s[66:67]
s_mov_b32 m0, s71
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v12, addr v13, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe0300, 0x0c00000d
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[68:69]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s70, v8
u23_join2:
u23_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant t2 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 t2_m3
s_bfe_u32 s43, s6, 0xc0006
t2_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 t2_m2
s_add_u32 s43, s43, 1
t2_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 t2_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
t2_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 t2_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch t2_pre_loop
t2_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 2 add gds
.long 0xd8fe0b00, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
t2_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant t3 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 t3_m3
s_bfe_u32 s43, s6, 0xc0006
t3_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 t3_m2
s_add_u32 s43, s43, 1
t3_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 t3_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
t3_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 t3_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch t3_pre_loop
t3_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 3 add gds
.long 0xd8fe0f00, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
t3_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant x5 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 x5_m3
s_bfe_u32 s43, s6, 0xc0006
x5_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 x5_m2
s_add_u32 s43, s43, 1
x5_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 x5_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
x5_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 x5_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch x5_pre_loop
x5_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe2300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
x5_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant x6 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 x6_m3
s_bfe_u32 s43, s6, 0xc0006
x6_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 x6_m2
s_add_u32 s43, s43, 1
x6_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 x6_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
x6_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 x6_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch x6_pre_loop
x6_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe4300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
x6_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant x7 ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 x7_m3
s_bfe_u32 s43, s6, 0xc0006
x7_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 x7_m2
s_add_u32 s43, s43, 1
x7_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 x7_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
x7_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 x7_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch x7_pre_loop
x7_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe8300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
x7_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant lds ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 lds_m3
s_bfe_u32 s43, s6, 0xc0006
lds_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 lds_m2
s_add_u32 s43, s43, 1
lds_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 lds_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
lds_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 lds_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch lds_pre_loop
lds_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fc0300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
lds_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant x5s ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 x5s_m3
s_bfe_u32 s43, s6, 0xc0006
x5s_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 x5s_m2
s_add_u32 s43, s43, 1
x5s_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 x5s_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
x5s_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 x5s_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch x5s_pre_loop
x5s_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 1 type 0 swap gds
.long 0xd8fe3300, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
x5s_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant x5a ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 x5a_m3
s_bfe_u32 s43, s6, 0xc0006
x5a_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 x5a_m2
s_add_u32 s43, s43, 1
x5a_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 x5a_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
x5a_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 x5a_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch x5a_pre_loop
x5a_pre_done:
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v12, data0 v0, idx 0 rel 1 done 1 type 0 add gds
.long 0xd8fe2300, 0x0500000c
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
x5a_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant tgs ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18
s_memtime s[48:49]
s_getreg_b32 s61, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s40, v0
s_lshr_b32 s40, s40, 6
s_mul_i32 s41, s5, s16
s_add_u32 s41, s41, s4
s_mul_i32 s41, s41, s17
s_add_u32 s41, s41, s40
s_mul_i32 s42, s41, 0x400
s_add_u32 s42, s42, s32
s_bfe_u32 s43, s6, 0xb0006
s_cmp_eq_u32 s30, 3
s_cbranch_scc0 tgs_m3
s_bfe_u32 s43, s6, 0xc0006
tgs_m3:
s_cmp_eq_u32 s30, 1
s_cselect_b32 s43, 0, s43
s_cmp_eq_u32 s30, 2
s_cbranch_scc0 tgs_m2
s_add_u32 s43, s43, 1
tgs_m2:
s_mul_i32 s46, s5, s20
s_add_u32 s46, s46, s18
s_lshl_b32 s46, s46, 16
s_or_b32 s44, s46, s43
s_mul_i32 s46, s5, s34
s_add_u32 s46, s46, s19
s_lshl_b32 s46, s46, 16
s_or_b32 s45, s46, s43
s_lshl_b32 s46, s80, 16
s_or_b32 s71, s46, s43
v_and_b32 v2, 63, v0
v_mul_lo_u32 v3, v2, s24
s_mul_i32 s46, s41, s25
s_add_u32 s47, s46, s23
v_add_i32 v1, vcc, s47, v3
s_add_u32 s47, s46, s33
v_add_i32 v4, vcc, s47, v3
s_add_u32 s47, s46, s81
v_add_i32 v13, vcc, s47, v3
v_or_b32 v12, 0xcafe0000, v2
v_or_b32 v5, 0xdead0000, v2
v_or_b32 v6, 0xbeef0000, v2
v_mov_b32 v7, s35
v_mov_b32 v9, s28
v_mov_b32 v10, 1
v_mov_b32 v8, 0
v_mov_b32 v11, s18
s_mov_b32 s58, -1
s_mov_b32 s59, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_mov_b64 s[54:55], 0
s_mov_b64 s[56:57], 0
s_mov_b64 s[66:67], 0
s_mov_b64 s[68:69], 0
s_mov_b32 s70, -1
s_mov_b32 s64, 0x0dc0ffee
s_and_b32 s46, s31, 0xffff
s_lshr_b32 s47, s31, 16
s_and_b32 s63, s41, s46
s_cmp_lg_u32 s63, s47
s_cselect_b32 s63, 1, 0
s_cmp_eq_u32 s46, 0
s_cselect_b32 s63, 0, s63
s_cmp_lg_u32 s63, 0
s_cbranch_scc1 tgs_skip
s_sub_u32 s62, s29, 1
s_sub_u32 s62, s62, s41
s_cmp_lg_u32 s36, 0
s_cselect_b32 s62, s41, s62
s_mul_i32 s62, s62, s21
tgs_pre_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 tgs_pre_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch tgs_pre_loop
tgs_pre_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 tgs_odd0
s_memtime s[50:51]
s_mov_b32 m0, s44
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v5, addr v1, data0 v0, idx 0 rel 1 done 0 type 0 add gds
.long 0xd8fe0100, 0x05000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[52:53]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s58, v8
s_branch tgs_join0
tgs_odd0:
tgs_join0:
s_cmp_eq_u32 s82, 0
s_cselect_b32 s46, 0x10001, s82
s_and_b32 s47, s46, 0xffff
s_lshr_b32 s46, s46, 16
s_and_b32 s62, s41, s47
s_cmp_eq_u32 s62, s46
s_cselect_b32 s62, s22, 0
tgs_mid_loop:
s_cmp_eq_u32 s62, 0
s_cbranch_scc1 tgs_mid_done
s_sleep 2
s_sub_u32 s62, s62, 1
s_branch tgs_mid_loop
tgs_mid_done:
s_and_b32 s46, s41, 1
s_cmp_eq_u32 s46, 0
s_cbranch_scc0 tgs_odd1
s_branch tgs_join1
tgs_odd1:
s_memtime s[54:55]
s_mov_b32 m0, s45
s_mov_b32 exec_lo, s26
s_mov_b32 exec_hi, s27
s_nop 1
; ds_ordered_count v6, addr v4, data0 v0, idx 1 rel 1 done 1 type 0 add gds
.long 0xd8fe0304, 0x06000004
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[56:57]
s_mov_b32 m0, s37
s_mov_b64 exec, 1
s_nop 1
ds_add_rtn_u32 v8, v9, v10 gds
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
v_readfirstlane_b32 s59, v8
tgs_join1:
tgs_skip:
s_waitcnt lgkmcnt(0)
v_writelane_b32 v20, s6, 0
v_writelane_b32 v20, s4, 1
v_writelane_b32 v20, s5, 2
v_writelane_b32 v20, s40, 3
v_writelane_b32 v20, s41, 4
v_writelane_b32 v20, s43, 5
v_writelane_b32 v20, s44, 6
v_writelane_b32 v20, s45, 7
v_writelane_b32 v20, s48, 8
v_writelane_b32 v20, s49, 9
v_writelane_b32 v20, s50, 10
v_writelane_b32 v20, s51, 11
v_writelane_b32 v20, s52, 12
v_writelane_b32 v20, s53, 13
v_writelane_b32 v20, s54, 14
v_writelane_b32 v20, s55, 15
v_writelane_b32 v20, s56, 16
v_writelane_b32 v20, s57, 17
v_writelane_b32 v20, s58, 18
v_writelane_b32 v20, s59, 19
v_writelane_b32 v20, s64, 20
v_writelane_b32 v20, s61, 21
v_writelane_b32 v20, s26, 22
v_writelane_b32 v20, s27, 23
v_writelane_b32 v20, s63, 24
v_writelane_b32 v20, s66, 25
v_writelane_b32 v20, s67, 26
v_writelane_b32 v20, s68, 27
v_writelane_b32 v20, s69, 28
v_writelane_b32 v20, s70, 29
v_writelane_b32 v20, s71, 30
v_lshlrev_b32 v21, 2, v2
v_add_i32 v21, vcc, s42, v21
buffer_store_dword v20, v21, s[0:3], 0 offen
buffer_store_dword v5, v21, s[0:3], 0 offen offset:256
buffer_store_dword v6, v21, s[0:3], 0 offen offset:512
buffer_store_dword v12, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant gdump ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_waitcnt lgkmcnt(0)
v_and_b32 v2, 63, v0
v_lshlrev_b32 v1, 2, v2
v_mov_b32 v7, s23
s_lshl_b32 s46, s4, 26
s_or_b32 m0, s46, 0x400
s_nop 1
ds_read_b32 v3, v1 gds
ds_read_b32 v4, v1 offset:256 gds
ds_read_b32 v5, v1 offset:512 gds
ds_read_b32 v6, v1 offset:768 gds
s_waitcnt lgkmcnt(0)
s_lshl_b32 s42, s4, 10
s_add_u32 s42, s42, s32
v_add_i32 v21, vcc, s42, v1
buffer_store_dword v3, v21, s[0:3], 0 offen
buffer_store_dword v4, v21, s[0:3], 0 offen offset:256
buffer_store_dword v5, v21, s[0:3], 0 offen offset:512
buffer_store_dword v6, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant gfill ----
s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10
s_waitcnt lgkmcnt(0)
v_and_b32 v2, 63, v0
v_lshlrev_b32 v1, 2, v2
v_mov_b32 v7, s23
s_lshl_b32 s46, s4, 26
s_or_b32 m0, s46, 0x400
s_nop 1
ds_write_b32 v1, v7 gds
ds_write_b32 v1, v7 offset:256 gds
ds_write_b32 v1, v7 offset:512 gds
ds_write_b32 v1, v7 offset:768 gds
s_waitcnt lgkmcnt(0)
ds_read_b32 v3, v1 gds
ds_read_b32 v4, v1 offset:256 gds
ds_read_b32 v5, v1 offset:512 gds
ds_read_b32 v6, v1 offset:768 gds
s_waitcnt lgkmcnt(0)
s_lshl_b32 s42, s4, 10
s_add_u32 s42, s42, s32
v_add_i32 v21, vcc, s42, v1
buffer_store_dword v3, v21, s[0:3], 0 offen
buffer_store_dword v4, v21, s[0:3], 0 offen offset:256
buffer_store_dword v5, v21, s[0:3], 0 offen offset:512
buffer_store_dword v6, v21, s[0:3], 0 offen offset:768
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant lean ----
s_buffer_load_dwordx8 s[8:15], s[0:3], 0x0
s_memtime s[16:17]
s_getreg_b32 s18, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s19, v0
s_lshr_b32 s19, s19, 6
s_mul_i32 s20, s6, s10
s_add_u32 s20, s20, s5
s_mul_i32 s20, s20, s8
s_add_u32 s20, s20, s4
s_mul_i32 s20, s20, s9
s_add_u32 s20, s20, s19
s_lshl_b32 s21, s20, 6
s_add_u32 s21, s21, s13
s_bfe_u32 s22, s7, 0xb0006
s_sub_u32 s23, s12, 1
s_sub_u32 s23, s23, s20
s_mul_i32 s23, s23, s11
s_cmp_eq_u32 s14, 1
s_cbranch_scc0 lean_dm
s_cmp_eq_u32 s20, 0
s_cselect_b32 s23, s11, 0
lean_dm:
lean_pre_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 lean_pre_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch lean_pre_loop
lean_pre_done:
v_mov_b32 v1, 1
v_mov_b32 v2, 0xdead0000
s_memtime s[24:25]
s_mov_b32 m0, s22
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel done add gds
.long 0xd8fe0300, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[26:27]
v_readfirstlane_b32 s28, v2
s_waitcnt lgkmcnt(0)
s_mov_b32 s29, 0x0dc0ffee
v_writelane_b32 v3, s7, 0
v_writelane_b32 v3, s4, 1
v_writelane_b32 v3, s5, 2
v_writelane_b32 v3, s6, 3
v_writelane_b32 v3, s19, 4
v_writelane_b32 v3, s20, 5
v_writelane_b32 v3, s28, 6
v_writelane_b32 v3, s24, 7
v_writelane_b32 v3, s26, 8
v_writelane_b32 v3, s18, 9
v_writelane_b32 v3, s29, 10
v_writelane_b32 v3, s16, 11
v_writelane_b32 v3, s25, 12
v_writelane_b32 v3, s27, 13
v_and_b32 v4, 63, v0
v_lshlrev_b32 v4, 2, v4
v_add_i32 v4, vcc, s21, v4
s_mov_b32 exec_lo, 0xffff
s_mov_b32 exec_hi, 0
buffer_store_dword v3, v4, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant gps (ps) ----
v_writelane_b32 v3, s0, 0
v_writelane_b32 v3, s1, 1
v_writelane_b32 v3, s2, 2
v_writelane_b32 v3, s3, 3
v_writelane_b32 v3, s4, 4
v_writelane_b32 v3, s5, 5
v_writelane_b32 v3, s6, 6
v_writelane_b32 v3, s7, 7
v_writelane_b32 v3, s8, 8
v_writelane_b32 v3, s9, 9
v_writelane_b32 v3, s10, 10
v_writelane_b32 v3, s11, 11
v_writelane_b32 v3, s12, 12
v_writelane_b32 v3, s13, 13
v_writelane_b32 v3, s14, 14
v_writelane_b32 v3, s15, 15
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x0
s_memtime s[40:41]
s_getreg_b32 s42, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
s_mov_b64 s[44:45], exec
v_readfirstlane_b32 s43, v0
s_mov_b64 exec, 1
s_mov_b32 m0, 0x400
v_mov_b32 v4, 0x300
v_mov_b32 v5, 1
s_nop 1
ds_add_rtn_u32 v6, v4, v5 gds
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s46, v6
s_mov_b32 m0, s32
s_nop 1
s_movrels_b32 s47, s0
s_lshr_b32 s47, s47, s33
s_and_b32 s47, s47, s34
s_mov_b32 s48, 0
s_mov_b32 s49, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_cmp_eq_u32 s35, 0
s_cbranch_scc1 gps_skip
s_lshl_b32 s48, s36, 16
s_or_b32 s48, s48, s47
v_mov_b32 v7, 1
v_mov_b32 v8, 0xdead0000
s_memtime s[50:51]
s_mov_b32 m0, s48
s_nop 1
; ds_ordered_count v8, v7 idx0 rel done add type 1 gds
.long 0xd8fe0700, 0x08000007
s_waitcnt lgkmcnt(0)
s_memtime s[52:53]
v_readfirstlane_b32 s49, v8
s_waitcnt lgkmcnt(0)
gps_skip:
s_mov_b32 s29, 0x0dc0ffee
v_writelane_b32 v3, s46, 16
v_writelane_b32 v3, s47, 17
v_writelane_b32 v3, s48, 18
v_writelane_b32 v3, s49, 19
v_writelane_b32 v3, s50, 20
v_writelane_b32 v3, s52, 21
v_writelane_b32 v3, s42, 22
v_writelane_b32 v3, s29, 23
v_writelane_b32 v3, s40, 24
v_writelane_b32 v3, s44, 25
v_writelane_b32 v3, s45, 26
v_writelane_b32 v3, s43, 27
v_writelane_b32 v3, s51, 28
v_writelane_b32 v3, s53, 29
s_lshl_b32 s54, s46, 7
s_add_u32 s54, s54, s37
s_mov_b32 exec_lo, -1
s_mov_b32 exec_hi, 0
v_mbcnt_lo_u32_b32 v9, -1, 0
v_lshlrev_b32 v9, 2, v9
v_add_i32 v9, vcc, s54, v9
buffer_store_dword v3, v9, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_mov_b64 exec, s[44:45]
exp null off, off, off, off done vm
s_endpgm

; ---- variant gps0 (ps) ----
v_writelane_b32 v3, s0, 0
v_writelane_b32 v3, s1, 1
v_writelane_b32 v3, s2, 2
v_writelane_b32 v3, s3, 3
v_writelane_b32 v3, s4, 4
v_writelane_b32 v3, s5, 5
v_writelane_b32 v3, s6, 6
v_writelane_b32 v3, s7, 7
v_writelane_b32 v3, s8, 8
v_writelane_b32 v3, s9, 9
v_writelane_b32 v3, s10, 10
v_writelane_b32 v3, s11, 11
v_writelane_b32 v3, s12, 12
v_writelane_b32 v3, s13, 13
v_writelane_b32 v3, s14, 14
v_writelane_b32 v3, s15, 15
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x0
s_memtime s[40:41]
s_getreg_b32 s42, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
s_mov_b64 s[44:45], exec
v_readfirstlane_b32 s43, v0
s_mov_b64 exec, 1
s_mov_b32 m0, 0x400
v_mov_b32 v4, 0x300
v_mov_b32 v5, 1
s_nop 1
ds_add_rtn_u32 v6, v4, v5 gds
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s46, v6
s_mov_b32 m0, s32
s_nop 1
s_movrels_b32 s47, s0
s_lshr_b32 s47, s47, s33
s_and_b32 s47, s47, s34
s_mov_b32 s48, 0
s_mov_b32 s49, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_cmp_eq_u32 s35, 0
s_cbranch_scc1 gps0_skip
s_lshl_b32 s48, s36, 16
s_or_b32 s48, s48, s47
v_mov_b32 v7, 1
v_mov_b32 v8, 0xdead0000
s_memtime s[50:51]
s_mov_b32 m0, s48
s_nop 1
; ds_ordered_count v8, v7 idx0 rel done add type 0 gds
.long 0xd8fe0300, 0x08000007
s_waitcnt lgkmcnt(0)
s_memtime s[52:53]
v_readfirstlane_b32 s49, v8
s_waitcnt lgkmcnt(0)
gps0_skip:
s_mov_b32 s29, 0x0dc0ffee
v_writelane_b32 v3, s46, 16
v_writelane_b32 v3, s47, 17
v_writelane_b32 v3, s48, 18
v_writelane_b32 v3, s49, 19
v_writelane_b32 v3, s50, 20
v_writelane_b32 v3, s52, 21
v_writelane_b32 v3, s42, 22
v_writelane_b32 v3, s29, 23
v_writelane_b32 v3, s40, 24
v_writelane_b32 v3, s44, 25
v_writelane_b32 v3, s45, 26
v_writelane_b32 v3, s43, 27
v_writelane_b32 v3, s51, 28
v_writelane_b32 v3, s53, 29
s_lshl_b32 s54, s46, 7
s_add_u32 s54, s54, s37
s_mov_b32 exec_lo, -1
s_mov_b32 exec_hi, 0
v_mbcnt_lo_u32_b32 v9, -1, 0
v_lshlrev_b32 v9, 2, v9
v_add_i32 v9, vcc, s54, v9
buffer_store_dword v3, v9, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_mov_b64 exec, s[44:45]
exp null off, off, off, off done vm
s_endpgm

; ---- variant gvs (vs) ----
v_writelane_b32 v3, s0, 0
v_writelane_b32 v3, s1, 1
v_writelane_b32 v3, s2, 2
v_writelane_b32 v3, s3, 3
v_writelane_b32 v3, s4, 4
v_writelane_b32 v3, s5, 5
v_writelane_b32 v3, s6, 6
v_writelane_b32 v3, s7, 7
v_writelane_b32 v3, s8, 8
v_writelane_b32 v3, s9, 9
v_writelane_b32 v3, s10, 10
v_writelane_b32 v3, s11, 11
v_writelane_b32 v3, s12, 12
v_writelane_b32 v3, s13, 13
v_writelane_b32 v3, s14, 14
v_writelane_b32 v3, s15, 15
s_buffer_load_dwordx8 s[32:39], s[0:3], 0x0
s_memtime s[40:41]
s_getreg_b32 s42, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
s_mov_b64 s[44:45], exec
v_readfirstlane_b32 s43, v0
s_mov_b64 exec, 1
s_mov_b32 m0, 0x400
v_mov_b32 v4, 0x300
v_mov_b32 v5, 1
s_nop 1
ds_add_rtn_u32 v6, v4, v5 gds
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s46, v6
s_mov_b32 m0, s32
s_nop 1
s_movrels_b32 s47, s0
s_lshr_b32 s47, s47, s33
s_and_b32 s47, s47, s34
s_mov_b32 s48, 0
s_mov_b32 s49, -1
s_mov_b64 s[50:51], 0
s_mov_b64 s[52:53], 0
s_cmp_eq_u32 s35, 0
s_cbranch_scc1 gvs_skip
s_lshl_b32 s48, s36, 16
s_or_b32 s48, s48, s47
v_mov_b32 v7, 1
v_mov_b32 v8, 0xdead0000
s_memtime s[50:51]
s_mov_b32 m0, s48
s_nop 1
; ds_ordered_count v8, v7 idx0 rel done add type 2 gds
.long 0xd8fe0b00, 0x08000007
s_waitcnt lgkmcnt(0)
s_memtime s[52:53]
v_readfirstlane_b32 s49, v8
s_waitcnt lgkmcnt(0)
gvs_skip:
s_mov_b32 s29, 0x0dc0ffee
v_writelane_b32 v3, s46, 16
v_writelane_b32 v3, s47, 17
v_writelane_b32 v3, s48, 18
v_writelane_b32 v3, s49, 19
v_writelane_b32 v3, s50, 20
v_writelane_b32 v3, s52, 21
v_writelane_b32 v3, s42, 22
v_writelane_b32 v3, s29, 23
v_writelane_b32 v3, s40, 24
v_writelane_b32 v3, s44, 25
v_writelane_b32 v3, s45, 26
v_writelane_b32 v3, s43, 27
v_writelane_b32 v3, s51, 28
v_writelane_b32 v3, s53, 29
s_lshl_b32 s54, s46, 7
s_add_u32 s54, s54, s37
s_mov_b32 exec_lo, -1
s_mov_b32 exec_hi, 0
v_mbcnt_lo_u32_b32 v9, -1, 0
v_lshlrev_b32 v9, 2, v9
v_add_i32 v9, vcc, s54, v9
buffer_store_dword v3, v9, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_mov_b64 exec, s[44:45]
v_lshlrev_b32 v10, 3, v0
v_add_i32 v10, vcc, 0x40000, v10
buffer_load_dwordx2 v[11:12], v10, s[0:3], 0 offen
v_mov_b32 v13, 0.5
v_mov_b32 v14, 1.0
s_waitcnt vmcnt(0)
exp param0 v11, v11, v11, v11
exp pos0 v11, v12, v13, v14 done
s_endpgm

; ---- variant conf ----
s_buffer_load_dwordx8 s[8:15], s[0:3], 0x0
s_getreg_b32 s18, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s19, v0
s_lshr_b32 s19, s19, 6
s_mul_i32 s20, s4, s8
s_add_u32 s20, s20, s19
s_lshl_b32 s21, s20, 8
s_add_u32 s21, s21, s11
s_bfe_u32 s22, s5, 0xb0006
s_mul_i32 s24, s9, 0x9e3779b9
s_xor_b32 s24, s24, s20
s_lshr_b32 s25, s24, 16
s_xor_b32 s24, s24, s25
s_mul_i32 s24, s24, 0x7feb352d
s_lshr_b32 s25, s24, 15
s_xor_b32 s24, s24, s25
s_mul_i32 s24, s24, 0x846ca68b
s_lshr_b32 s25, s24, 16
s_xor_b32 s24, s24, s25
s_mov_b32 s26, s24
s_lshr_b32 s25, s26, 16
s_xor_b32 s26, s26, s25
s_mul_i32 s26, s26, 0x7feb352d
s_lshr_b32 s25, s26, 15
s_xor_b32 s26, s26, s25
s_mul_i32 s26, s26, 0x846ca68b
s_lshr_b32 s25, s26, 16
s_xor_b32 s26, s26, s25
v_mov_b32 v1, 1
v_mov_b32 v2, 0xdead0000
s_mov_b32 s37, 8
s_mov_b64 s[38:39], 0
s_mov_b32 s46, 0
s_lshr_b32 s27, s24, 0
s_and_b32 s27, s27, 31
s_and_b32 s28, s27, 3
s_cmp_eq_u32 s28, 0
s_cbranch_scc1 conf_j0
s_lshr_b32 s29, s26, 0
s_and_b32 s29, s29, 63
s_mul_i32 s29, s29, s10
s_mov_b32 s23, s29
conf_d0_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 conf_d0_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch conf_d0_loop
conf_d0_done:
s_lshr_b32 s31, s27, 2
s_lshl_b32 s31, s31, 1
s_cmp_lg_u32 s28, 1
s_cselect_b32 s28, 1, 0
s_or_b32 s31, s31, s28
s_cmp_eq_u32 s31, 0
s_cbranch_scc1 conf_k0_c0
s_cmp_eq_u32 s31, 1
s_cbranch_scc1 conf_k0_c1
s_cmp_eq_u32 s31, 2
s_cbranch_scc1 conf_k0_c2
s_cmp_eq_u32 s31, 3
s_cbranch_scc1 conf_k0_c3
s_cmp_eq_u32 s31, 4
s_cbranch_scc1 conf_k0_c4
s_cmp_eq_u32 s31, 5
s_cbranch_scc1 conf_k0_c5
s_cmp_eq_u32 s31, 6
s_cbranch_scc1 conf_k0_c6
s_cmp_eq_u32 s31, 7
s_cbranch_scc1 conf_k0_c7
s_branch conf_j0
conf_k0_c0:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 add gds
.long 0xd8fe0000, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 0
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c1:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel add gds
.long 0xd8fe0100, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 1
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c2:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 add gds
.long 0xd8fe0004, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 4
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c3:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 rel add gds
.long 0xd8fe0104, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 5
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c4:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 add gds
.long 0xd8fe0008, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 8
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c5:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 rel add gds
.long 0xd8fe0108, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 9
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c6:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 add gds
.long 0xd8fe000c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 12
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_k0_c7:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 rel add gds
.long 0xd8fe010c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 13
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j0
conf_j0:
s_lshr_b32 s27, s24, 5
s_and_b32 s27, s27, 31
s_and_b32 s28, s27, 3
s_cmp_eq_u32 s28, 0
s_cbranch_scc1 conf_j1
s_lshr_b32 s29, s26, 8
s_and_b32 s29, s29, 63
s_mul_i32 s29, s29, s10
s_mov_b32 s23, s29
conf_d1_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 conf_d1_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch conf_d1_loop
conf_d1_done:
s_lshr_b32 s31, s27, 2
s_lshl_b32 s31, s31, 1
s_cmp_lg_u32 s28, 1
s_cselect_b32 s28, 1, 0
s_or_b32 s31, s31, s28
s_cmp_eq_u32 s31, 0
s_cbranch_scc1 conf_k1_c0
s_cmp_eq_u32 s31, 1
s_cbranch_scc1 conf_k1_c1
s_cmp_eq_u32 s31, 2
s_cbranch_scc1 conf_k1_c2
s_cmp_eq_u32 s31, 3
s_cbranch_scc1 conf_k1_c3
s_cmp_eq_u32 s31, 4
s_cbranch_scc1 conf_k1_c4
s_cmp_eq_u32 s31, 5
s_cbranch_scc1 conf_k1_c5
s_cmp_eq_u32 s31, 6
s_cbranch_scc1 conf_k1_c6
s_cmp_eq_u32 s31, 7
s_cbranch_scc1 conf_k1_c7
s_branch conf_j1
conf_k1_c0:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 add gds
.long 0xd8fe0000, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 0
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c1:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel add gds
.long 0xd8fe0100, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 1
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c2:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 add gds
.long 0xd8fe0004, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 4
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c3:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 rel add gds
.long 0xd8fe0104, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 5
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c4:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 add gds
.long 0xd8fe0008, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 8
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c5:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 rel add gds
.long 0xd8fe0108, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 9
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c6:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 add gds
.long 0xd8fe000c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 12
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_k1_c7:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 rel add gds
.long 0xd8fe010c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 13
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j1
conf_j1:
s_lshr_b32 s27, s24, 10
s_and_b32 s27, s27, 31
s_and_b32 s28, s27, 3
s_cmp_eq_u32 s28, 0
s_cbranch_scc1 conf_j2
s_lshr_b32 s29, s26, 16
s_and_b32 s29, s29, 63
s_mul_i32 s29, s29, s10
s_mov_b32 s23, s29
conf_d2_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 conf_d2_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch conf_d2_loop
conf_d2_done:
s_lshr_b32 s31, s27, 2
s_lshl_b32 s31, s31, 1
s_cmp_lg_u32 s28, 1
s_cselect_b32 s28, 1, 0
s_or_b32 s31, s31, s28
s_cmp_eq_u32 s31, 0
s_cbranch_scc1 conf_k2_c0
s_cmp_eq_u32 s31, 1
s_cbranch_scc1 conf_k2_c1
s_cmp_eq_u32 s31, 2
s_cbranch_scc1 conf_k2_c2
s_cmp_eq_u32 s31, 3
s_cbranch_scc1 conf_k2_c3
s_cmp_eq_u32 s31, 4
s_cbranch_scc1 conf_k2_c4
s_cmp_eq_u32 s31, 5
s_cbranch_scc1 conf_k2_c5
s_cmp_eq_u32 s31, 6
s_cbranch_scc1 conf_k2_c6
s_cmp_eq_u32 s31, 7
s_cbranch_scc1 conf_k2_c7
s_branch conf_j2
conf_k2_c0:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 add gds
.long 0xd8fe0000, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 0
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c1:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel add gds
.long 0xd8fe0100, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 1
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c2:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 add gds
.long 0xd8fe0004, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 4
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c3:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 rel add gds
.long 0xd8fe0104, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 5
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c4:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 add gds
.long 0xd8fe0008, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 8
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c5:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 rel add gds
.long 0xd8fe0108, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 9
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c6:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 add gds
.long 0xd8fe000c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 12
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_k2_c7:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 rel add gds
.long 0xd8fe010c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 13
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_j2
conf_j2:
s_lshr_b32 s29, s26, 24
s_and_b32 s29, s29, 63
s_mul_i32 s29, s29, s10
s_mov_b32 s23, s29
conf_df_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 conf_df_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch conf_df_loop
conf_df_done:
s_lshr_b32 s31, s24, 15
s_and_b32 s31, s31, 3
s_cmp_eq_u32 s31, 0
s_cbranch_scc1 conf_f0
s_cmp_eq_u32 s31, 1
s_cbranch_scc1 conf_f1
s_cmp_eq_u32 s31, 2
s_cbranch_scc1 conf_f2
s_cmp_eq_u32 s31, 3
s_cbranch_scc1 conf_f3
conf_f0:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel done add gds
.long 0xd8fe0300, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 3
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_end
conf_f1:
s_mov_b32 s30, 0x3c0000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx1 rel done add gds
.long 0xd8fe0304, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 7
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_end
conf_f2:
s_mov_b32 s30, 0x380000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx2 rel done add gds
.long 0xd8fe0308, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 11
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_end
conf_f3:
s_mov_b32 s30, 0x340000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx3 rel done add gds
.long 0xd8fe030c, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 15
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_branch conf_end
conf_end:
s_mov_b32 s49, 0x0dc0ffee
v_writelane_b32 v3, s5, 0
v_writelane_b32 v3, s20, 1
v_writelane_b32 v3, s46, 2
v_writelane_b32 v3, s49, 3
v_writelane_b32 v3, s18, 4
v_writelane_b32 v3, s24, 5
v_writelane_b32 v3, s26, 6
v_writelane_b32 v3, s38, 56
v_writelane_b32 v3, s39, 57
v_and_b32 v0, 63, v0
v_lshlrev_b32 v0, 2, v0
v_add_i32 v0, vcc, s21, v0
s_mov_b32 exec_lo, 0xff
s_mov_b32 exec_hi, 0x3000000
buffer_store_dword v3, v0, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_endpgm

; ---- variant steps ----
s_buffer_load_dwordx8 s[8:15], s[0:3], 0x0
s_getreg_b32 s18, hwreg(HW_REG_HW_ID)
s_waitcnt lgkmcnt(0)
v_readfirstlane_b32 s19, v0
s_lshr_b32 s19, s19, 6
s_mul_i32 s20, s4, s8
s_add_u32 s20, s20, s19
s_lshl_b32 s21, s20, 8
s_add_u32 s21, s21, s11
s_bfe_u32 s22, s5, 0xb0006
s_mul_i32 s24, s9, 0x9e3779b9
s_xor_b32 s24, s24, s20
s_lshr_b32 s25, s24, 16
s_xor_b32 s24, s24, s25
s_mul_i32 s24, s24, 0x7feb352d
s_lshr_b32 s25, s24, 15
s_xor_b32 s24, s24, s25
s_mul_i32 s24, s24, 0x846ca68b
s_lshr_b32 s25, s24, 16
s_xor_b32 s24, s24, s25
s_mov_b32 s26, s24
s_lshr_b32 s25, s26, 16
s_xor_b32 s26, s26, s25
s_mul_i32 s26, s26, 0x7feb352d
s_lshr_b32 s25, s26, 15
s_xor_b32 s26, s26, s25
s_mul_i32 s26, s26, 0x846ca68b
s_lshr_b32 s25, s26, 16
s_xor_b32 s26, s26, s25
v_mov_b32 v1, 1
v_mov_b32 v2, 0xdead0000
s_mov_b32 s37, 8
s_mov_b64 s[38:39], 0
s_mov_b32 s46, 0
s_mov_b32 s47, 0
steps_loop:
s_cmp_eq_u32 s20, s13
s_cselect_b32 s29, s15, 0
s_cmp_lg_u32 s47, s14
s_cselect_b32 s29, 0, s29
s_mov_b32 s23, s29
steps_sl_loop:
s_cmp_eq_u32 s23, 0
s_cbranch_scc1 steps_sl_done
s_sleep 2
s_sub_u32 s23, s23, 1
s_branch steps_sl_loop
steps_sl_done:
s_add_u32 s48, s47, 1
s_cmp_eq_u32 s48, s12
s_cbranch_scc1 steps_last
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel add gds
.long 0xd8fe0100, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 1
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_add_u32 s47, s47, 1
s_branch steps_loop
steps_last:
s_mov_b32 s30, 0x400000
s_or_b32 s30, s30, s22
s_memtime s[32:33]
s_mov_b32 m0, s30
s_mov_b64 exec, 1
s_nop 1
; ds_ordered_count v2, v1 idx0 rel done add gds
.long 0xd8fe0300, 0x02000001
s_waitcnt lgkmcnt(0)
s_mov_b64 exec, -1
s_memtime s[34:35]
v_readfirstlane_b32 s36, v2
s_waitcnt lgkmcnt(0)
v_mov_b32 v4, s36
v_mov_b32 v5, s32
v_mov_b32 v6, s34
s_lshl_b32 s50, s37, 2
s_add_u32 s50, s50, s21
v_mov_b32 v7, s50
s_mov_b64 exec, 1
buffer_store_dword v4, v7, s[0:3], 0 offen
buffer_store_dword v5, v7, s[0:3], 0 offen offset:4
buffer_store_dword v6, v7, s[0:3], 0 offen offset:8
s_mov_b64 exec, -1
s_add_u32 s37, s37, 3
s_mov_b32 s44, 3
s_mov_b32 s45, 0
s_lshl_b32 s40, s46, 2
s_lshl_b64 s[42:43], s[44:45], s40
s_or_b64 s[38:39], s[38:39], s[42:43]
s_add_u32 s46, s46, 1
s_mov_b32 s49, 0x0dc0ffee
v_writelane_b32 v3, s5, 0
v_writelane_b32 v3, s20, 1
v_writelane_b32 v3, s46, 2
v_writelane_b32 v3, s49, 3
v_writelane_b32 v3, s18, 4
v_writelane_b32 v3, s24, 5
v_writelane_b32 v3, s26, 6
v_writelane_b32 v3, s38, 56
v_writelane_b32 v3, s39, 57
v_and_b32 v0, 63, v0
v_lshlrev_b32 v0, 2, v0
v_add_i32 v0, vcc, s21, v0
s_mov_b32 exec_lo, 0xff
s_mov_b32 exec_hi, 0x3000000
buffer_store_dword v3, v0, s[0:3], 0 offen
s_waitcnt vmcnt(0)
s_endpgm

