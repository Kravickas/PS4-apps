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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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
s_and_b32 s63, s41, 1
s_and_b32 s63, s63, s31
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
s_and_b32 s62, s41, 1
s_mul_i32 s62, s62, s22
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

