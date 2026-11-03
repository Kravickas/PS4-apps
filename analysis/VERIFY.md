# Cross-verification of the memory rules

Each rule implemented in shadPS4 (branch memory-tests) against independent sources:

- **DmemTest**: measured on the PS4 (FW 12.02 console, SDK 4.508 eboot), IDs as in the results.
- **fpPS4 kern**: red-prig/fpPS4, branch `kern` (2026-10-22), whose kernel side is reconstructed
  from the PS4 kernel. File and function given; "kern" = that branch.
- **FreeBSD 9**: the PS4 kernel's base, for the generic VM parts (fpPS4 kern follows it there).

Verdict: **confirmed** (sources agree), **measured wins** (fpPS4 differs, the PS4 measurement
decides), **from kern, probe** (rule taken from fpPS4, a DmemTest probe checks it), **open**.

| rule | DmemTest | fpPS4 kern | verdict |
|---|---|---|---|
| mprotect keeps CPU r/w/x and GPU r/w (`& 0x37`), write implies read, other bits dropped | BT11-BT13, NU8, NU16 | vm_mmap.pas `sys_mprotect`: `prot := (w→r) or (prot and $37)` | confirmed |
| mtypeprotect: type > 10 or `prot & 0xC8` → EINVAL; write implies read | BT14, BT15 | vm_mmap.pas `sys_mtypeprotect` | confirmed |
| mtypeprotect: type 10 with write, execute or GPU write → EACCES | DT6, TP1, BT15 | vm_map.pas `vm_map_type_protect`: WB_GARLIC and `prot & $EE` | confirmed |
| mtypeprotect: reserved range → EACCES (max protection 0) | TP3 | `vm_map_type_protect` first pass, max_protection check | confirmed |
| mtypeprotect: flexible memory → EOPNOTSUPP (no dmem object) | NU7, TP3 | `vm_map_type_protect`: not OBJ_DMEM_EXT → KERN_FAILURE → EOPNOTSUPP | confirmed |
| mtypeprotect: physical memory mapped twice → EBUSY | CV21, CV22, CV50, CV51, CV56, CV99 | `vm_map_type_protect`: `rmem_map_test(rt_continuity)` → KERN_BUSY | confirmed |
| ... checked only over the requested range | CV52 (op=0) | kern tests the whole map entry (would be EBUSY) | measured wins |
| mtypeprotect sets the type of the physical memory (every mapping sees it) | CV54, CV62 | `dmem_map_set_mtype` | confirmed |
| mtypeprotect on pool memory: 64 KiB aligned, within one entry, else EINVAL | TP4 (new) | `vm_map_type_protect`: OBJT_BLOCKPOOL branch | from kern, probe |
| pool block class: type 3 (WC_GARLIC) flushed, every other type cached | TR trace (576 = 576) | kern_blockpool.pas `get_mflags` | confirmed |
| pool commit over a committed block → EBUSY | PB5, NU | kern_blockpool.pas `kern_blockpool_map` | confirmed |
| mmap: sanitizer flag → EINVAL; length 0 → EINVAL | BT9 bit 21, SF2 | vm_mmap.pas `sys_mmap` | confirmed |
| mmap: MAP_VOID or MAP_ANON with an offset or a file → EINVAL | SF2 | `sys_mmap` | confirmed |
| mmap: MAP_VOID wins over MAP_ANON (max protection 0) | BT9 bit 8 (SB) | `sys_mmap`: VOID sets cap_maxprot 0 and adds ANON | confirmed |
| mmap: MAP_STACK needs CPU read and write and no file | SF3 | `sys_mmap` | confirmed |
| mmap: MAP_FIXED at 0 → EINVAL | FW1, BT9 bit 4 | `sys_mmap` (appinfo mmap_flags) | confirmed |
| mmap: no address → search from the user heap | K1, SF5 | `sys_mmap`: `SCE_USR_HEAP_START` | confirmed |
| file mapping never gets execute | XR6, PT3 | `sys_mmap` DTYPE_VNODE: "no VM_PROT_EXECUTE at all" | confirmed |
| file without read access: any read, write or GPU read → EACCES; no access OK | TF3 | `sys_mmap` DTYPE_VNODE | confirmed |
| file without read access: maximum protection write and GPU write only | TF3 (CPU read), TF6 (new: GPU read) | `sys_mmap` maxprot | confirmed / from kern, probe |
| shared mapping, file without write access: write → EACCES | TF4 (CPU write), TF6 (new: GPU write) | `sys_mmap` maxprot | confirmed / from kern, probe |
| private file mapping: writable copy | FM1, TF2, TF4, TF5 | `sys_mmap`: private → maxprot has write | confirmed |
| VirtualQuery: entry entirely in 0x7F0000000-0xFF0000000 refused (EACCES) | VQ9-VQ15 | kern_dmem.pas `sys_virtual_query` (`ET_DYN_LOAD_ADDR_SYS`) | confirmed |
| ... find-next skips those entries and reports the next one | VQ15 (nothing above), VQ16 (new) | `sys_virtual_query` | from kern, probe |
| ... an entry reaching below the area is reported | VQ17 (new) | `sys_virtual_query`: `(start shr 28) < 127` | from kern, probe |
| ... callers in that area (system libraries) are answered | VQ14 (modules there) | `sys_virtual_query`: `is_libsys_call` | from kern (not measurable from a homebrew) |
| QueryMemoryProtection on pool memory: 0x33 below SDK 10.00 (whole entry) | CV40-CV42, CV70-CV94 | kern_dmem.pas `sys_query_memory_protection`: sdk < 0x10000000 → entry prot | confirmed |
| ... start and end: the whole reservation below SDK 10.00 | PG4 (new) | same | from kern, probe |
| QueryMemoryProtection on direct memory hides execute | XR2, XR3 | `sys_query_memory_protection`: DMEM → `and (GPU_ALL or RW)` | confirmed |
| writable anonymous mappings never merge (budget credential), any SDK | CO1, CO9, FW6 | vm_map.pas `vm_map_simplify_entry`: `if entry^.cred then Exit` | confirmed |
| read-only / no-access anonymous mappings follow the normal rules | CO15, CO16 (new) | same | from kern, probe |
| direct memory never merges below SDK 2.00; pool entries never merge | (SDK 4.508) | `vm_map_simplify_entry` | confirmed |
| merging needs the same name and, below SDK 5.50, the same caller | VQ6-VQ8, FW6 | `vm_map_simplify_entry`: name and `anon_addr` unless sdk_55 | confirmed |
| MAP_NO_COALESCE keeps entries apart | CO8 | `vm_map_simplify_entry`: NO_COALESCE / entry_id | confirmed |
| new anonymous/void entry extends its left neighbour, then simplify | CO11, CO12 | vm_map.pas `vm_map_insert` (vm_object_coalesce, simplify) | confirmed (left merge) |
| void reservation: right merge only with MAP_SHARED or without a left merge; end-of-range piece | CO2, CO3, CO12-CO14 | not explained by kern's code alone | measured |
| direct mapping split by mprotect/mtypeprotect does not re-merge | FW2, TP2 | kern calls `vm_map_simplify_entry` after type protect | measured wins |
| GetDirectMemoryType region over neighbouring same-type areas | CV3-CV35, CV61, GT1 (new) | (dmem query not in kern's syscalls read here) | measured |
| pool batch move | PB3-PB5 | not implemented in kern | measured |

## Open, by design of the sources

- `is_libsys_call` for VirtualQuery: a homebrew cannot run code in the system area, so this comes
  from kern alone.
- fpPS4 kern has per-title exceptions in `sys_virtual_query` (CUSA lists); those are not rules and
  are not copied.
