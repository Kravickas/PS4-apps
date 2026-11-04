# Cross-verification of the memory rules

Each rule implemented in shadPS4 (branch memory-tests) against independent sources:

- **DmemTest**: measured on the PS4 (FW 12.02 console), IDs as in the results. Every result up to
  round 71 is at process SDK version 0x01000051 (SDK 1.00, create-fself's default; shadPS4 reads the
  same value). Rules behind an SDK threshold (2.00, 2.50, 3.00, 3.50, 5.00, 5.50, 10.00) are measured
  below it only, until the SDK builds (README, SDK VERSION) have run.
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
| mtypeprotect on pool memory: 64 KiB aligned, within one entry, else EINVAL | TP4 (`I`, `I`, `o`) | `vm_map_type_protect`: OBJT_BLOCKPOOL branch | confirmed |
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
| file without read access: maximum protection write and GPU write only | TF3, TF6 (`mprotect 0x33` → `0x22`) | `sys_mmap` maxprot | confirmed |
| shared mapping, file without write access: write or GPU write → EACCES; private with GPU write OK | TF4, TF6 (`A`, `o`, `A`) | `sys_mmap` maxprot | confirmed |
| private file mapping: writable copy | FM1, TF2, TF4, TF5 | `sys_mmap`: private → maxprot has write | confirmed |
| VirtualQuery: entry entirely in 0x7F0000000-0xFF0000000 refused (EACCES) | VQ9-VQ15 | kern_dmem.pas `sys_virtual_query` (`ET_DYN_LOAD_ADDR_SYS`) | confirmed |
| ... find-next skips those entries and reports the next one | VQ15 (nothing above), VQ16 | `sys_virtual_query` | confirmed |
| ... an entry reaching below the area is reported | VQ17 | `sys_virtual_query`: `(start shr 28) < 127` | confirmed |
| ... callers in that area (system libraries) are answered | VQ14 (modules there) | `sys_virtual_query`: `is_libsys_call` | from kern (not measurable from a homebrew) |
| QueryMemoryProtection on pool memory: 0x33 below SDK 10.00 (whole entry) | CV40-CV42, CV70-CV94 | kern_dmem.pas `sys_query_memory_protection`: sdk < 0x10000000 → entry prot | confirmed |
| ... start and end: the whole reservation below SDK 10.00 | PG4 (`0-200000`) | same | confirmed |
| QueryMemoryProtection on direct memory hides execute | XR2, XR3 | `sys_query_memory_protection`: DMEM → `and (GPU_ALL or RW)` | confirmed |
| anonymous mappings made writable never merge (budget credential, kept after mprotect), any SDK | CO1, CO9 (prot step), FW6 | vm_map.pas `vm_map_simplify_entry`: `if entry^.cred then Exit`; the credential is set at insert | confirmed |
| anonymous mappings made with any access never merge (read-only too) | CO15 | kern: credential only for writable ones | measured wins |
| anonymous mappings without access merge like void reservations without MAP_SHARED (no end-of-range rule); mprotect does not merge them | CO16 | — | measured |
| mlock over memory without CPU access → ENOMEM | CO2, CO3, CO8, CO16; ML1 (new: direct p0) | `vm_map_wire` faults pages in (FreeBSD) | confirmed (mechanism) / probe |
| direct memory never merges below SDK 2.00; pool entries never merge | (all results at SDK 1.00) | `vm_map_simplify_entry` | confirmed below 2.00 |
| merging needs the same name and, below SDK 5.50, the same caller | VQ6-VQ8, FW6 | `vm_map_simplify_entry`: name and `anon_addr` unless sdk_55 | confirmed |
| MAP_NO_COALESCE keeps entries apart | CO8 | `vm_map_simplify_entry`: NO_COALESCE / entry_id | confirmed |
| new anonymous/void entry extends its left neighbour, then simplify | CO11, CO12 | vm_map.pas `vm_map_insert` (vm_object_coalesce, simplify) | confirmed (left merge) |
| void reservation: right merge only with MAP_SHARED or without a left merge; end-of-range piece | CO2, CO3, CO11-CO14 (simulated against all six before implementing) | not explained by kern's code alone | measured |
| void reservation without MAP_SHARED: mprotect does not merge it | CO3 (prot step) | — | measured |
| MapDirectMemory2 keeps an area apart only when it changes the type | DT5 (change), CV23 (same type) | — | measured |
| VirtualQuery on direct memory: region = run of pages with the same physical type, offset at its start | CV100, CV101 | — | measured |
| direct mapping split by mprotect/mtypeprotect does not re-merge below SDK 2.00, does from 2.00 | FW2, TP2 at SDK 1.00, 1.50, 2.00 | kern: no direct merging below 2.00, simplify after type protect | confirmed |
| GetDirectMemoryType region over neighbouring same-type areas | CV3-CV35, CV61, GT1 | (dmem query not in kern's syscalls read here) | measured |
| mtypeprotect EBUSY: only when the requested range holds an aliased page | EB1 (`EBUSY`, `o`, `o`), CV52 | kern tests the whole entry | measured wins |
| pool batch move | PB3-PB5 | not implemented in kern | measured |

## Measured SDK differences

Every released firmware version from 1.00 to 9.50 run on FW 12.02 (SDK chain): rules change at
exactly these versions and at no other.

| from SDK | what changes | probes |
|---|---|---|
| 1.70 | address 0 with MAP_FIXED for MapFlexibleMemory, MapDirectMemory, ReserveVirtualRange: accepted below, EINVAL from | FW1, BT1, BT3, BT5 bit 4 |
| 2.00 | direct mappings merge with neighbours, also after mprotect / mtypeprotect back; flexible memory available at start 0x1B800000 -> 0x1BB00000; layout at start | CO4, CO10, FW2, TP2; FL1-FL6, DF4; Y, K, MF, HB, J6, LA3, XC5, VQ9 |
| 2.50 | MapDirectMemory goes through MapDirectMemory2 (not with MAP_STACK): memory already mapped without aliasing EBUSY; execute EINVAL (was EACCES); protection bits 0x100+ accepted; a fixed mapping reaching 0x7F0000000-0xFF0000000 EINVAL (a page ending at 0x7F0000000 included) | B10, EX2, BT6, VQ10-VQ13, VQ15, VQ17, LA1, LA2 |
| 2.50 | fixed direct mapping over pool memory holding a committed block: ENOMEM below, does not return from 2.50 (the PS4 soft-locks; those probes are skipped from 2.50) | T19 |
| 3.00 | a mapping at or reaching past 0xFC00000000: fixed EINVAL, search ENOMEM (was mapped); protection bits 0x100+ EINVAL again | FW4, VQ10, BT6 |
| 3.50 | flexible budget exceeded: EINVAL -> ENOMEM | FW5 |
| 5.00 | pool reservations of 16 GiB and more allowed (largest 0x3FFE00000 -> 0x5ED400000); the pool counters after it follow from that | U10; Y4, Z1, M1, NU5 |

The chain now runs SDK 1.00, these versions, and 10.00, written as real SDK version numbers:
major.minor.patch with three-digit minor and patch, digit for digit, the released SDK x.y0 being
x.y08.001 (0x01008001 ... 0x05008001, 0x10008001; Gran Turismo 7's executable carries 0x11008001,
Unreal Engine's PS4 notes list 9.508.001, 10.008.011, 12.508.001). These are the values the runs used,
except 1.00, which was create-fself's default 0x01000051 (the same band). param.sfo declares the
highest (PUBTOOLINFO sdk_ver=10008001, SYSTEM_VER 0x10008000).

10.00 is refused by FW 12.02 as corrupted: started from an older SDK by LoadExec and as eboot.bin from
the menu; with the firmware version field unset and set; with the SDK written 0x10008001 and
0x10000001. Not the cause either: HEN's generic authentication info, which Gran Turismo 7's
fake-signed executable (SDK 11.00, no embedded authentication info) also runs with. Open: Gran
Turismo 7's process parameter has 5 entries with _sceLibcParam, _sceKernelMemParam and
_sceKernelFsParam (0x60 bytes); OpenOrbis' crt1.o writes 0x50 bytes with 3 entries, its pointers to
the same three parameters filled in by relocations at load (they read 0 in the file). The 10.00
executable is now linked with a 0x60, 5-entry process parameter (README).
Refused with that too. param.sfo next (red_prig: "perhaps more data is needed in param.sfo"): Gran
Turismo 7's has SYSTEM_VER 0x11008000 (our derivation), no PUBTOOLINFO, and FORMAT "obs" and
PUBTOOLMINVER 0x02990000, which ours lacked; ours now has those two and no PUBTOOLINFO.
Refused with that too (SDK 10.00, 10.50, 11.00, 11.50, 12.00). Next: the bundled OpenOrbis modules
(libc.prx, libSceFios2.prx), module parameter SDK 1.00, now packaged with it set to 10.00.
Refused with that too. Now the app finds the reason itself: execve from SDK 10.00 (the kernel's
answer, without the system launcher) and the kernel's message buffer at every launch, in
dmem_exec.txt (README).

## GPU protection on file mappings

shadPS4's MapFile asserted, noting that on real hardware GPU file mappings crash the system. Round 69
ran TF6 on the PS4 (FW 12.02) without any crash: a private mapping with GPU write succeeds, a shared
one of a read-only file is EACCES, and the run reached DONE. TF6 never lets the GPU use such memory;
what the GPU does with it is not measured.

## Open, by design of the sources

- `is_libsys_call` for VirtualQuery: a homebrew cannot run code in the system area, so this comes
  from kern alone.
- fpPS4 kern has per-title exceptions in `sys_virtual_query` (CUSA lists); those are not rules and
  are not copied.
