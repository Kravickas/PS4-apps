# DmemTest coverage

Generated from `src/dmemtest.cpp`.

| function | called from |
|---|---|
| `__error` | px_ret, run_bt |
| `mlock` | run_px |
| `mmap` | px3_mk, px_mk, run_bt, run_mf3 |
| `mprotect` | run_px, run_rg |
| `msync` | run_px |
| `munlock` | run_px |
| `munmap` | run_bt, run_followups, run_px |
| `sceKernelAllocateDirectMemory` | alloc_in, run_bp, run_bt, run_h |
| `sceKernelAllocateMainDirectMemory` | run_ms |
| `sceKernelAvailableDirectMemorySize` | run_bt, run_h, run_t, v_line |
| `sceKernelAvailableFlexibleMemorySize` | run_fl, run_followups |
| `sceKernelBatchMap` | run_bm, run_last |
| `sceKernelBatchMap2` | run_bm, run_bt |
| `sceKernelCheckedReleaseDirectMemory` | chk, nu_release, release, run_bp, run_h, run_p, run_p2, run_t, run_u, run_xc |
| `sceKernelClose` | emit, log_init, mf_grid_line, mf_make_10k, nu_file, remove_scratch_files, run_mf2, run_mf21, run_mf24, run_mf25, run_mf3, run_sf, run_vq, run_xc |
| `sceKernelDirectMemoryQuery` | allocated, cg_case, nu_query, q_line, run_bp, run_bt, run_f, run_h, run_j, run_q, run_t, run_u, run_u12, run_w, run_x, run_xc, sanity |
| `sceKernelEnableDmemAliasing` | run_b, run_sf8 |
| `sceKernelFsync` | emit |
| `sceKernelGetDirectMemorySize` | main |
| `sceKernelGetDirectMemoryType` | gdt_line, run_bp, run_ms, run_sf |
| `sceKernelGetPrtAperture` | run_ms |
| `sceKernelIsStack` | run_ms |
| `sceKernelLseek` | run_mf24, run_mf3 |
| `sceKernelMapDirectMemory` | e_map, fill_default, fixed_probe, g_case, map, nu_mprotect, run_bp, run_bt, run_ex, run_followups, run_j, run_k, run_l, run_m, run_n, run_rg, run_rv, run_sf8, run_t, run_u, run_w, vq_mk_direct |
| `sceKernelMapDirectMemory2` | run_bt, run_ms, run_sf |
| `sceKernelMapFlexibleMemory` | nu_flex2, run_bt, run_ex, run_fl, run_followups, run_k, run_rg, run_rv, run_sf, run_w, run_xc, run_y, sb_map_flex, vq_mk_flex |
| `sceKernelMapNamedDirectMemory` | run_nm |
| `sceKernelMapNamedFlexibleMemory` | run_nm |
| `sceKernelMemoryPoolBatch` | run_bt, run_last, run_pb |
| `sceKernelMemoryPoolCommit` | nu_mtype_reserved, nu_pool, nu_poolorder, p9_case, run_bp, run_bt, run_ex, run_followups, run_j, run_last, run_m, run_p, run_p2, run_p4, run_p7, run_p8, run_t, run_u, run_vq, run_w, run_x, run_y, run_z, tr_commit |
| `sceKernelMemoryPoolDecommit` | nu_pool, nu_poolorder, p9_case, run_bp, run_bt, run_j, run_m, run_p, run_p2, run_p4, run_p7, run_p8, run_pb, run_t, run_u, run_vq, run_w, run_x, run_y, run_z, tr_decommit |
| `sceKernelMemoryPoolExpand` | nu_mtype_reserved, nu_mtypeprotect, nu_pool, nu_poolorder, run_bp, run_bt, run_ex, run_followups, run_last, run_nm, run_p, run_p2, run_p4, run_p54, run_p7, run_p8, run_p9, run_pb, run_t, run_u, run_vq, run_xc, tr_expand |
| `sceKernelMemoryPoolGetBlockStats` | ps, run_t, stats, tr_read |
| `sceKernelMemoryPoolReserve` | fill_default, hint_line, max_reserve, nu_fewblocks, nu_mtype_reserved, nu_mtype_reserved_twice, nu_mtypeprotect, nu_pool, nu_poolorder, p9_case, reserve_sizes, run_bp, run_bt, run_ex, run_followups, run_hb, run_j, run_k, run_l, run_last, run_m, run_n, run_nm, run_p, run_p2, run_p4, run_p7, run_p8, run_pb, run_t, run_u, run_vq, run_w, run_x, run_xc, run_y, run_z, tr_reserve, vq_reserve |
| `sceKernelMlock` | run_ms |
| `sceKernelMmap` | mf_grid_line, mf_line, nu_file, run_bt, run_ex, run_mf2, run_mf21, run_mf24, run_mf25, run_mf3, run_sf, vq_mk_anon, vq_mk_file |
| `sceKernelMprotect` | cg_case, nu_mprotect, nu_protbits, nu_rounding, run_bp, run_bt, run_ex, run_f, run_followups, run_mf24, run_rg, run_rv, run_xc, sb_mprotect |
| `sceKernelMtypeprotect` | nu_mtype_reserved, nu_mtype_reserved_twice, nu_mtypeprotect, nu_rounding, p9_case, run_bp, run_bt, run_f, run_j, run_t, run_u, run_vq, run_w, run_x, tr_mtypeprotect |
| `sceKernelMunlock` | run_ms |
| `sceKernelMunmap` | bt_unmap_ret, cg_case, e_map, fixed_probe, hint_line, max_reserve, mf_grid_line, mf_line, nu_fewblocks, nu_file, nu_mprotect, nu_mtype_reserved, nu_mtype_reserved_twice, nu_mtypeprotect, nu_munmap, nu_pool, nu_poolorder, nu_protbits, nu_query, nu_rounding, p9_case, reserve_sizes, run_bm, run_bp, run_bt, run_ex, run_f, run_fl, run_followups, run_hb, run_j, run_k, run_l, run_last, run_m, run_mf2, run_mf21, run_mf24, run_mf25, run_mf3, run_ms, run_n, run_nm, run_p4, run_p7, run_p8, run_pb, run_px, run_rg, run_rv, run_sf, run_sf8, run_t, run_u, run_vq, run_w, run_x, run_xc, run_y, run_z, rv_try, tr_munmap, unfill_default, unmap_all |
| `sceKernelOpen` | emit, log_init, mf_grid_line, mf_make_10k, mf_open, nu_file, remove_scratch_files, run_mf2, run_mf21, run_mf24, run_mf25, run_mf3, run_sf, run_vq |
| `sceKernelQueryMemoryProtection` | ex_try, mapped, nu_page, run_followups, run_last, rv_prot, sb_read_prot |
| `sceKernelReleaseDirectMemory` | a_free, b_alias, b_split, build, d_outside_mapped, g_case, neighbours, nu_release, release, release_all, run_bp, run_bt, run_c, run_e, run_f, run_h, run_h7, run_ms, run_p, run_p2, run_p54, run_r, run_t, run_u, run_xc |
| `sceKernelReleaseFlexibleMemory` | run_fl, run_followups |
| `sceKernelReserveVirtualRange` | run_bm, run_bt, run_followups, run_last, run_rv, run_sf, rv_mk, rv_try |
| `sceKernelSetVirtualRangeName` | run_nm, sb_set_name |
| `sceKernelUnlink` | remove_scratch_files |
| `sceKernelUsleep` | main, pool_checkpoint, run_p8, series |
| `sceKernelVirtualQuery` | nm_show, nu_query, nu_vtype, run_bt, run_followups, run_t, sb_read_name, sb_read_type, sb_read_vprot, sf_vq, vq_field, vq_name, vq_rel, vq_walk |
| `sceKernelWrite` | emit, mf_grid_line, mf_make_10k, mf_open, run_mf2, run_mf21, run_mf25, run_mf3 |

## Registered in shadPS4 (`/home/claude/mc/src/core/libraries/kernel/memory.cpp`) but never called by the test

- `_sceKernelRtldSetApplicationHeapAPI`
- `sceKernelClearVirtualRangeName`
- `sceKernelConfiguredFlexibleMemorySize`
- `sceKernelIsAddressSanitizerEnabled`
- `sceKernelMapNamedSystemFlexibleMemory`
- `sceKernelSetPrtAperture`
