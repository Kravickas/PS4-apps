# Coverage of shadPS4's memory code by measurements

Every error return in shadPS4's `src/core/memory.cpp` and `src/core/libraries/kernel/memory.cpp`
(branch `memory-tests`), with the probe whose call reaches exactly that condition and the PS4 result,
or UNTESTED. A return counts as covered only when a probe triggers its condition and the PS4 result
is recorded; a comment in the code is not enough. Probes whose result could follow from more than one
rule (as BP13 did) are listed under "Ambiguous". Status: TO MAP (not checked yet), COVERED (probe,
PS4 result), UNTESTED (no probe reaches it: needs one), AMBIGUOUS.

Batch 1 (direct memory allocation and release) mapped: 13 returns, new probes E18, E19, RL1,
RL2 for the untested ones.
Batch 2 (VirtualQuery, ReserveVirtualRange, MapNamedDirectMemory, MapDirectMemory2) mapped: 27
returns, new probes RV7, NM5, MS6. Probes through sceKernelMapDirectMemory count for that function only:
on the PS4 the named function is a separate libkernel function, so it gets its own probes (NM5).
Batch 3 (flexible mapping, system flexible mapping, mprotect, mtypeprotect, DirectMemoryQuery,
BatchMap2, SetVirtualRangeName) mapped: 26 returns, new probes NM6, SY1, PX5, NM7.
Batch 4 (pool expand, reserve, commit, decommit, batch, moves) mapped: 36 returns, new probes PE1, PR1,
PD1, PB8, PB9. NU5 and NU10 also show two things the PS4 accepts: commit and decommit of length 0.
Batch 5 (munmap, mlock, munlock, msync, ReleaseFlexibleMemory, ConfiguredFlexibleMemorySize, PRT)
mapped: 19 returns, new probes CF1, ML3, RF1, PRT1. Three returns are host failure paths, not PS4 rules.
Batch 6 (MemoryManager: map, file map, unmap, queries, search) mapped: 37 returns, new probes MP1,
MP2, QP1, DQ2, FM9. Two DIFFER for a known reason (N6 host address space, DF devices).

Note: probes made by helper functions (a_case, a_raw, q_raw, e_alloc, ...) were first missed by the
search for probes; checking them turned three UNTESTED entries into COVERED (A12-A14, C5, C7, D1).

Total: 158 returns in 47 functions. Line numbers: memory-tests at the time of mapping.

## sceKernelAllocateDirectMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:50 | EINVAL | `if (searchStart < 0 \|\| searchEnd < 0)` | COVERED: AL1 (start and end negative), PS4 EINVAL |
| kernel/memory.cpp:54 | EINVAL | `if (len <= 0 \|\| !Common::Is16KBAligned(len))` | COVERED: E1 (len 0), E2 (len 0x1000), AL1 (len 0), PS4 EINVAL |
| kernel/memory.cpp:59 | EINVAL | `if (alignment != 0 && (!std::has_single_bit(alignment) \|\| !Common::Is16KBAligned(alignment)))` | COVERED: E3 (0x3000), E4 (0x1000), AL1 (0xC000; 0x2000 with len 0x2000), PS4 EINVAL; E5 (0) ok |
| kernel/memory.cpp:63 | EINVAL | `if (memoryType < 0 \|\| memoryType > 10)` | COVERED: H3 (-2, -1, 11, 12 EINVAL; 0-10 ok), E6, E7, AL1 |
| kernel/memory.cpp:67 | EINVAL | `if (physAddrOut == nullptr)` | PS4 (round 116): the app ended at E18 at SDK 1.00 and 12.00 (no line after E10, E18 next). From round 117 E18 runs after DONE with a "starting" line, which confirms whether the call itself ends it. shadPS4 returns EINVAL; no game can pass null here, as the PS4 ends it |
| kernel/memory.cpp:76 | EAGAIN | `if (searchEnd <= searchStart \|\| searchEnd < len \|\| !is_in_range)` | COVERED: AL2 (end<=start, end<len, end-len<start), E8, E10, PS4 EAGAIN |
| kernel/memory.cpp:82 | EAGAIN | `if (phys_addr == -1)` | COVERED: E19 (EAGAIN) |

## sceKernelCheckedReleaseDirectMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:106 | EINVAL | `if (!Common::Is16KBAligned(start) \|\| !Common::Is16KBAligned(len))` | COVERED: A13 (start +0x1000), A14 (len +0x1000), PS4 EINVAL; RL1 repeats it |
| kernel/memory.cpp:110 | EINVAL | `if (static_cast<s64>(start) < 0 \|\| static_cast<s64>(len) < 0)` | COVERED: negative start H1, D4; negative length D1 (checked, 1<<63), C5, C7; PS4 EINVAL; RL2 repeats it |

## sceKernelReleaseDirectMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:124 | EINVAL | `if (!Common::Is16KBAligned(start) \|\| !Common::Is16KBAligned(len))` | COVERED: A3 (start +0x1000), A4 (len +0x1000), A5, NU4, PS4 EINVAL |
| kernel/memory.cpp:128 | EINVAL | `if (static_cast<s64>(start) < 0 \|\| static_cast<s64>(len) < 0)` | COVERED: negative start H1, D4; negative length D1 (checked, 1<<63), C5, C7; PS4 EINVAL; RL2 repeats it |

## sceKernelAvailableDirectMemorySize

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:145 | EINVAL | `if (alignment != 0 && !std::has_single_bit(alignment))` | COVERED: AV1 (3, 0x6000 EINVAL; 1, 0x2000 ok), H4 (0x3000, 0x6000 EINVAL; 0x1000, 0x2000 ok) |
| kernel/memory.cpp:155 | ENOMEM | `if (size == 0)` | COVERED: AV2 (start>end, range outside direct memory), PS4 ENOMEM |

## VirtualQueryImpl

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:187 | EACCES | `if ((flags & 1) == 0)` | COVERED: VQ10 (vq0 at unmapped addresses: EACCES), BT24 (flag bits) |
| kernel/memory.cpp:191 | EACCES | `if (result != ORBIS_OK)` | COVERED: VQ10 (vq1 past the last mapping: EACCES) |

## ReserveVirtualRangeImpl

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:209 | EINVAL | `if (addr == nullptr)` | PS4: RV8 (null output) ends the app (round 118); shadPS4 EINVAL, which no game can see |
| kernel/memory.cpp:213 | EINVAL | `if (len == 0 \|\| !Common::Is16KBAligned(len))` | COVERED: RV2 (len 0, len 0x1000), PS4 EINVAL |
| kernel/memory.cpp:219 | EINVAL | `if (!std::has_single_bit(alignment) \|\| !Common::Is16KBAligned(alignment) \|\| alignment >= 0x8000'0000ULL)` | COVERED: RV2 (0x6000 EINVAL, 0x8000 ok), BT2 (1<<k: below 16K and from 2G EINVAL), N1 (2G EINVAL) |
| kernel/memory.cpp:232 | EINVAL | `if (True(static_cast<Core::MemoryMapFlags>(flags) & ~ValidReserveFlags))` | COVERED: BT1 (every flag bit), RV2 (flag 1), PS4 EINVAL |
| kernel/memory.cpp:239 | EINVAL | `if ((flags & static_cast<s32>(Core::MemoryMapFlags::Fixed)) != 0 && memory->ContainsVmaType(in_addr, len, Core::VMAType::Direct))` | DIFFERS: RV7, M5: on the PS4 a fixed reservation (flags 0x10) over a direct mapping succeeds and replaces it; with 0x90 ENOMEM. shadPS4 EINVAL |
| kernel/memory.cpp:245 | EINVAL | `if (Common::ElfInfo::FW_170 <= g_sdk_version)` | COVERED: FW1 (null address with Fixed, SDK 1.70+), PS4 EINVAL |

## sceKernelMapNamedDirectMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:267 | EFAULT | `if (name == nullptr)` | COVERED: NM1 (null name), PS4 EFAULT |
| kernel/memory.cpp:276 | EINVAL | `if (len == 0 \|\| !Common::Is16KBAligned(len))` | unnamed: E13 (len 0x5000), E15 (len 0) EINVAL; named: NM5, the same codes as unnamed |
| kernel/memory.cpp:281 | EINVAL | `if (!Common::Is16KBAligned(phys_addr))` | unnamed: E12 (phys +0x1000) EINVAL; named: NM5, the same codes as unnamed |
| kernel/memory.cpp:289 | EINVAL | `if (alignment != 0 && (!std::has_single_bit(alignment) \|\| !Common::Is16KBAligned(alignment) \|\| alignment >= 2_GB))` | unnamed: BT7 (1<<k), N2; named: NM5, the same codes as unnamed |
| kernel/memory.cpp:294 | ENAMETOOLONG | `if (std::strlen(name) >= ORBIS_KERNEL_MAXIMUM_NAME_LENGTH)` | COVERED: NM1 (32 characters), PS4 ENAMETOOLONG |
| kernel/memory.cpp:309 | EINVAL | `if (True(mem_prot & Core::MemoryProt::CpuExec))` | unnamed: BT6 (prot bit 2), EX2 (0x37) EINVAL; named: NM5, the same codes as unnamed |
| kernel/memory.cpp:321 | EINVAL | `if (True(mem_prot & ~ValidProt))` | unnamed: BT6 (prot bits from 0x40, SDK rules); named: NM5, the same codes as unnamed |
| kernel/memory.cpp:330 | EINVAL | `if (True(static_cast<Core::MemoryMapFlags>(flags) & ~ValidDirectFlags))` | unnamed: BT5 (every flag bit); named: DT9 (0x800000), NM5 for more |
| kernel/memory.cpp:342 | EACCES | `if (True(mem_prot & (Core::MemoryProt::CpuWrite \| Core::MemoryProt::GpuWrite)) && Core::Memory::Instance()->GetDirectMemoryType(phys_addr, &phys_type, &type_start, &type_` | COVERED: DT6 (type 10 rw, unnamed), DT9 (named and unnamed, CPU/GPU write) EACCES |
| kernel/memory.cpp:362 | EINVAL | `if (True(map_flags & Core::MemoryMapFlags::Fixed) && in_addr < SystemAreaEnd && in_addr + len >= SystemAreaStart)` | unnamed: VQ10-VQ13, LA1, LA2 (system area, SDK 2.50+); named: NM5, the same codes as unnamed |
| kernel/memory.cpp:368 | EINVAL | `if (Common::ElfInfo::FW_170 <= g_sdk_version)` | unnamed: FW1 (null + Fixed, SDK 1.70+) EINVAL; named: NM5, the same codes as unnamed |

## sceKernelMapDirectMemory2

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:396 | EINVAL | `if (len == 0 \|\| !Common::Is16KBAligned(len))` | COVERED: SF6 (len 0x2000, len 0), PS4 EINVAL |
| kernel/memory.cpp:401 | EINVAL | `if (!Common::Is16KBAligned(phys_addr))` | COVERED: SF6 (phys -0x2000), PS4 EINVAL |
| kernel/memory.cpp:410 | EINVAL | `if (alignment != 0 && (!std::has_single_bit(alignment) \|\| !Common::Is16KBAligned(alignment) \|\| alignment >= 2_GB))` | COVERED: SF6 (alignment 0xC000, 0x100, 1<<32, 0x1000), PS4 EINVAL |
| kernel/memory.cpp:414 | EINVAL | `if (!Common::Is16KBAligned(reinterpret_cast<VAddr>(*addr)))` | COVERED: SF6 (address +0x2000), PS4 EINVAL |
| kernel/memory.cpp:421 | EINVAL | `if (True(static_cast<Core::MemoryMapFlags>(flags) & ~ValidDirectFlags))` | PARTLY: SF6 (flag 1), PS4 EINVAL; other flag bits not probed for MapDirectMemory2 |
| kernel/memory.cpp:425 | EINVAL | `if (type < -1 \|\| type > 10)` | COVERED: BT8 (types 11+ EINVAL, 0-9 ok), SF7 (-2 EINVAL, -1 keeps the type) |
| kernel/memory.cpp:430 | EACCES | `if (type == 10 && True(static_cast<Core::MemoryProt>(prot) & (Core::MemoryProt::CpuWrite \| Core::MemoryProt::GpuWrite)))` | COVERED: BT8 (type 10 rw EACCES), DT6, DT9 |
| kernel/memory.cpp:436 | EINVAL | `if (True(mem_prot & Core::MemoryProt::CpuExec))` | COVERED: MS6 (prot 4, 5, 7: EINVAL) |

## sceKernelMapNamedFlexibleMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:464 | EFAULT | `if (name == nullptr)` | COVERED: NM2 (null name), PS4 EFAULT |
| kernel/memory.cpp:470 | EINVAL | `if (len == 0 \|\| !Common::Is16KBAligned(len))` | COVERED: NM6 (len 0, 0x1000: EINVAL, named and unnamed) |
| kernel/memory.cpp:475 | ENAMETOOLONG | `if (std::strlen(name) >= ORBIS_KERNEL_MAXIMUM_NAME_LENGTH)` | COVERED: NM2 (32 characters), PS4 ENAMETOOLONG |
| kernel/memory.cpp:488 | EINVAL | `if (True(mem_prot & ~ValidProt))` | unnamed: BT4 (every prot bit); named: NM6, EINVAL as unnamed (fixed 0: ok at SDK 1.00, EINVAL from 1.70) |
| kernel/memory.cpp:495 | EINVAL | `if (True(map_flags & ~ValidFlexibleFlags))` | unnamed: BT3 (every flag bit); named: NM6, EINVAL as unnamed (fixed 0: ok at SDK 1.00, EINVAL from 1.70) |
| kernel/memory.cpp:501 | EINVAL | `if (Common::ElfInfo::FW_170 <= g_sdk_version)` | unnamed: FW1 (null + Fixed, SDK 1.70+) EINVAL; named: NM6, EINVAL as unnamed (fixed 0: ok at SDK 1.00, EINVAL from 1.70) |

## sceKernelMapNamedSystemFlexibleMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:522 | EINVAL | `if (len == 0 \|\| !Common::Is16KBAligned(len))` | COVERED: SY1 (len 0, 0x1000: EINVAL) |
| kernel/memory.cpp:527 | EFAULT | `if (name == nullptr)` | COVERED: SY1 (EFAULT) |
| kernel/memory.cpp:534 | EINVAL | `if (Common::ElfInfo::FW_170 <= g_sdk_version)` | COVERED: SY1 (fixed 0: ok at SDK 1.00, EINVAL from 1.70) |
| kernel/memory.cpp:541 | ENAMETOOLONG | `if (std::strlen(name) >= ORBIS_KERNEL_MAXIMUM_NAME_LENGTH)` | PARTLY: SY1 (32 characters) fails with a code the line does not show (?) |

## sceKernelMprotect

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:585 | EACCES | `if (True(protection_flags & (Core::MemoryProt::CpuWrite \| Core::MemoryProt::GpuWrite)) && memory_manager->HasDirectMemoryType(aligned_addr, aligned_size, 10))` | COVERED: TP1 (mprotect rw on type 10), PS4 EACCES |

## posix_mprotect

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:598 | -1 | `if (result < 0)` | COVERED: PX5 (POSIX mprotect rw on type 10: -1/EACCES) |

## sceKernelMtypeprotect

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:610 | EACCES | `if (mtype == 10 && True(static_cast<Core::MemoryProt>(prot) & (Core::MemoryProt::CpuWrite \| Core::MemoryProt::GpuWrite)))` | COVERED: BT15 (type 10 rw EACCES), X1 (type 10 with write prots EACCES), W6 |
| kernel/memory.cpp:614 | EINVAL | `if (mtype < 0 \|\| mtype > 10)` | COVERED: BT15 (11+ EINVAL), TP3 (11 on flexible EINVAL), W6, J4 |
| kernel/memory.cpp:620 | EINVAL | `if ((prot & (0x8 \| 0x40 \| 0x80)) != 0)` | COVERED: BT14 (prot bits 0x8, 0x40, 0x80 EINVAL) |
| kernel/memory.cpp:624 | EACCES | `if (True(static_cast<Core::MemoryProt>(prot) & Core::MemoryProt::CpuExec))` | COVERED: BT14 (prot bit 0x4 EACCES) |
| kernel/memory.cpp:644 | EOPNOTSUPP | `if (memory_manager->ContainsFlexibleMemory(aligned_addr, aligned_size))` | COVERED: NU3, TP3 (flexible memory), PS4 EOPNOTSUPP (0x8002002D) |
| kernel/memory.cpp:654 | EINVAL | `if (!Common::Is64KBAligned(aligned_addr) \|\| !Common::Is64KBAligned(aligned_size) \|\| !memory_manager->IsWithinOnePoolReservation(aligned_addr, aligned_size))` | COVERED: TP4 (+0x4000 len 0x10000; across two reservations), PS4 EINVAL |
| kernel/memory.cpp:658 | EBUSY | `if (memory_manager->IsPhysicalMemoryAliased(aligned_addr, aligned_size))` | COVERED: EB1 (over an alias), PS4 EBUSY (0x80020010) |
| kernel/memory.cpp:662 | EACCES | `if (memory_manager->ContainsVmaType(aligned_addr, aligned_size, Core::VMAType::Reserved))` | COVERED: TP3 (plain reservation), PS4 EACCES |

## sceKernelDirectMemoryQuery

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:677 | EINVAL | `if (flags != 0 && flags != 1)` | COVERED: H2 (every flag bit: only 0 and 1 ok), BT25 |
| kernel/memory.cpp:689 | EFAULT | `if (query_info == nullptr)` | COVERED: T13 (null info, size 0x18), PS4 EFAULT; size 0 ok |
| kernel/memory.cpp:699 | EFAULT | `if (copied < copy_size)` | COVERED: XC4 (info straddling into read-only / unmapped memory), PS4 EFAULT |

## sceKernelBatchMap2

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:752 | EINVAL | `if ((flags & ~ValidBatchFlags) != 0)` | COVERED: BT26 (every flag bit of one protect entry) |

## sceKernelSetVirtualRangeName

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:806 | EFAULT | `if (name == nullptr)` | COVERED: NM7 (EFAULT) |
| kernel/memory.cpp:811 | ENAMETOOLONG | `if (std::strlen(name) >= ORBIS_KERNEL_MAXIMUM_NAME_LENGTH)` | COVERED: NM3 (32 characters), PS4 ENAMETOOLONG |

## sceKernelMemoryPoolExpand

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:829 | EINVAL | `if (searchStart < 0 \|\| searchEnd <= searchStart)` | DIFFERS: PE1: start -0x10000 ok on the PS4 and expands normally (0x1650000 in round 120: the start acts as 0); end <= start ENOMEM; shadPS4 EINVAL for both |
| kernel/memory.cpp:833 | EINVAL | `if (len <= 0 \|\| !Common::Is64KBAligned(len))` | DIFFERS: PE1: len 0 ok on the PS4 and writes 0 to the output (round 120); shadPS4 EINVAL; len 0x4000 EINVAL (same) |
| kernel/memory.cpp:837 | EINVAL | `if (alignment != 0 && !Common::Is64KBAligned(alignment))` | COVERED: PE1 (alignment 0x4000: EINVAL) |
| kernel/memory.cpp:841 | EINVAL | `if (physAddrOut == nullptr)` | COVERED: PE2 (null phys out: ok on the PS4) |
| kernel/memory.cpp:850 | ENOMEM | `if (searchEnd <= searchStart \|\| searchEnd < len \|\| !is_in_range)` | COVERED: P25 (window 0x4000 < len 0x10000), PS4 ENOMEM |
| kernel/memory.cpp:856 | ENOMEM | `if (phys_addr == -1)` | COVERED: P54 (window 0x14000, no fit inside), XC1 (one page short), PS4 ENOMEM |

## MemoryPoolReserveImpl

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:877 | EINVAL | `if ((flags & ~0x90) != 0)` | COVERED: M2 (flags 1,2,4,8,20,40,100,1000 EINVAL; 80 ok), BT16 |
| kernel/memory.cpp:885 | EINVAL | `if (alignment > 1 && (!std::has_single_bit(alignment) \|\| alignment < 2_MB \|\| alignment > 1_GB))` | COVERED: M3 (0x4000, 0x10000, 1M, 3M, 2G, 4G EINVAL; 2M ok), N1, K5, BT17 |
| kernel/memory.cpp:890 | EINVAL | `if ((flags & 0x10) != 0 && (addr_in == nullptr \|\| !Common::Is2MBAligned(reinterpret_cast<VAddr>(addr_in))))` | COVERED: M2 (0x10 with a null address), XC2 (fixed at +0x4000, +0x100000), PS4 EINVAL |
| kernel/memory.cpp:894 | EINVAL | `if (len == 0 \|\| !Common::Is2MBAligned(len))` | COVERED: P4 (len 0x10000), PR1 (len 0): EINVAL |

## sceKernelMemoryPoolCommit

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:927 | EINVAL | `if (addr == nullptr \|\| !Common::Is64KBAligned(reinterpret_cast<VAddr>(addr)))` | COVERED: NU5, NU10 (address +0x4000), PS4 EINVAL |
| kernel/memory.cpp:931 | EINVAL | `if (!Common::Is64KBAligned(len))` | COVERED: NU5 (len 0x4000) EINVAL; len 0 is accepted on the PS4 (NU5) |
| kernel/memory.cpp:939 | EINVAL | `if (flags != 0)` | COVERED: BT18 (every flag bit EINVAL) |
| kernel/memory.cpp:944 | EINVAL | `if (True(mem_prot & ~(Core::MemoryProt::CpuReadWrite \| Core::MemoryProt::GpuReadWrite \| Core::MemoryProt::CpuExec)))` | COVERED: BT19 (prot bits from 0x40 EINVAL) |
| kernel/memory.cpp:948 | EINVAL | `if (True(mem_prot & Core::MemoryProt::CpuExec))` | COVERED: BT19 (prot 0x4 EINVAL), EX5 (0x7 EINVAL), NU10 |
| kernel/memory.cpp:954 | EACCES | `if (type == 10 && True(mem_prot & (Core::MemoryProt::CpuWrite \| Core::MemoryProt::GpuWrite)))` | COVERED: Y3 (type 10 prot 0x20, 0x21, 0x30 EACCES), X2 |
| kernel/memory.cpp:958 | EINVAL | `if (type != 0 && type != 3 && type != 10)` | COVERED: J3 (-1, 11, 12 EINVAL), BT20 (types 1, 2, 4-9 EINVAL) |

## sceKernelMemoryPoolDecommit

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:977 | EINVAL | `if (addr == nullptr \|\| !Common::Is64KBAligned(reinterpret_cast<VAddr>(addr)))` | COVERED: NU10 (address +0x4000), PS4 EINVAL |
| kernel/memory.cpp:982 | EINVAL | `if (flags != 0)` | COVERED: BT21 (every flag bit EINVAL) |
| kernel/memory.cpp:989 | EINVAL | `if (!Common::Is64KBAligned(len))` | COVERED: PD1 (EINVAL); len 0 is accepted on the PS4 (NU10) |

## sceKernelMemoryPoolBatch

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1003 | EINVAL | `if (entries == nullptr)` | COVERED: PB8 (EFAULT, 0 processed) |
| kernel/memory.cpp:1008 | EINVAL | `if (flags != 0)` | COVERED: BT27 (every flag bit EINVAL) |

## sceKernelConfiguredFlexibleMemorySize

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1219 | EINVAL | `if (sizeOut == nullptr)` | PARTLY: CF1 returns 0x1C000000 at SDK 1.00-3.50; the lookup fails from 5.00 (round 119 prints why), so the null output (CF2) is not measured yet (never called before: not in the OpenOrbis stub, looked up with sceKernelDlsym) |

## sceKernelMunmap

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1230 | EINVAL | `if (len == 0)` | COVERED: NU1 (len 0), PS4 EINVAL; +0x1000 and len 0x4001 are accepted (NU1) |

## posix_munmap

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1241 | -1 | `if (result < 0)` | COVERED: PX2 (len 0), PS4 -1/EINVAL |

## posix_mlock

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1249 | -1 | `if (!addr \|\| len == 0)` | DIFFERS: ML3: PS4 null -1/ENOMEM, len 0 returns 0 (shadPS4 EINVAL for both); PX2: mapped 0 |
| kernel/memory.cpp:1255 | -1 | `if (raw_addr + len < raw_addr)` | COVERED: ML3 (wrap-around: -1/EINVAL) |
| kernel/memory.cpp:1265 | -1 | `if (!Core::Memory::Instance()->IsRangeMapped(raw_addr, len) \|\| Core::Memory::Instance()->ContainsNoCpuAccess(raw_addr, len))` | COVERED: ML3 (unmapped, no CPU access: -1/ENOMEM) |

## posix_munlock

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1284 | -1 | `if (!addr \|\| len == 0)` | DIFFERS: ML3: munlock null -1/ENOMEM, len 0 returns 0 (shadPS4 EINVAL for both) |
| kernel/memory.cpp:1290 | -1 | `if (raw_addr + len < raw_addr)` | COVERED: ML3 (wrap-around: -1/EINVAL) |
| kernel/memory.cpp:1296 | -1 | `if (!Core::Memory::Instance()->IsRangeMapped(raw_addr, len))` | COVERED: ML3 (munlock unmapped: -1/ENOMEM) |
| kernel/memory.cpp:1299 | -1 | `(unconditional)` | COVERED: PX2 (munlock on mapped memory), PS4 -1/EINVAL |

## sceKernelReleaseFlexibleMemory

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1316 | EINVAL | `if (!addr \|\| ((raw_addr \| len) & 0x3fff))` | COVERED: FL2 (address +0x1000), RF1 (null address, len 0x1000): EINVAL |
| kernel/memory.cpp:1329 | EINVAL | `if (!VirtualAlloc(addr, len, MEM_RESET, PAGE_READWRITE))` | HOST FAILURE PATH (Windows VirtualAlloc MEM_RESET), not a PS4 rule: must not fail where the PS4 succeeds; FL2 (direct, hole, len 0) succeed on the PS4 |
| kernel/memory.cpp:1334 | EINVAL | `if (madvise(addr, len, MADV_FREE) != 0)` | HOST FAILURE PATH (macOS madvise), not a PS4 rule: as above, FL2 |
| kernel/memory.cpp:1339 | EINVAL | `if (madvise(addr, len, MADV_DONTNEED) != 0)` | HOST FAILURE PATH (Linux madvise), not a PS4 rule: as above, FL2 |

## posix_msync

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1350 | -1 | `if (!Core::Memory::Instance()->IsRangeMapped(reinterpret_cast<VAddr>(addr), len))` | COVERED: PX2 (msync of a hole), PS4 -1/EINVAL |

## sceKernelSetPrtAperture

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1364 | EINVAL | `if (id < 0 \|\| id >= MAX_PRT_APERTURES)` | COVERED: PRT1 (EINVAL) |
| kernel/memory.cpp:1368 | EINVAL | `if (address < PRT_AREA_START_ADDR \|\| address + size > PRT_AREA_START_ADDR + PRT_AREA_SIZE)` | COVERED: PRT1 (EINVAL) |
| kernel/memory.cpp:1372 | EINVAL | `if (address % 4096 != 0)` | COVERED: PRT1 (EINVAL) |

## sceKernelGetPrtAperture

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1388 | EINVAL | `if (id < 0 \|\| id >= MAX_PRT_APERTURES)` | COVERED: PRT1 (id 0: 0x1000000000, 0x10000; id -1, 3: EINVAL) |

## MemoryManager::PoolExpand

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:359 | -1 | `if (dmem_area == dmem_map.end() \|\| mapping_end > search_end)` | COVERED: P54, XC1 (no fit), PS4 ENOMEM |

## MemoryManager::PoolReserve

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:430 | ENOMEM | `if (size >= 16_GB && static_cast<u32>(sdk_version) < Common::ElfInfo::FW_500)` | COVERED: U10, W2, T14 and the SDK sweep (16 GiB and more: ENOMEM below SDK 5.00) |
| core/memory.cpp:439 | ENOMEM | `if (pool_avail_flushed + pool_avail_cached < blocks)` | COVERED: P75 (drained pool), J6, NU11, PS4 ENOMEM |

## MemoryManager::Allocate

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:485 | -1 | `if (dmem_area == dmem_map.end() \|\| mapping_end > search_end)` | COVERED: BT23 (alignment 1<<33 and up: no fit, EAGAIN); E19 adds a full range |

## MemoryManager::Free

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:509 | ENOENT | `if (is_checked)` | COVERED: H7 (checked release past the end of direct memory: ENOENT below 0x5000000000), H1 |
| core/memory.cpp:531 | ENOENT | `if (is_checked)` | COVERED: NU4 (checked release twice: ENOENT), P8, P28 (pool memory), T8, U6 |

## MemoryManager::PoolCommit

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:605 | EBUSY | `if (vma.type == VMAType::Pooled)` | COVERED: NU5 (commit twice: ok, then EBUSY) |
| core/memory.cpp:610 | EINVAL | `if (vma.type != VMAType::PoolReserved)` | COVERED: NU5 ("outside": commit on flexible memory), PS4 EINVAL |
| core/memory.cpp:618 | EINVAL | `if (!vma.Contains(mapped_addr, size))` | COVERED: NU5 ("2 rsv": across two reservations), PS4 EINVAL |
| core/memory.cpp:625 | ENOMEM | `if (blocks > pool_avail_flushed + pool_avail_cached)` | COVERED: P75 (drained pool), NU12, PS4 ENOMEM |

## MemoryManager::MapMemory

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:773 | EINVAL | `if (type == VMAType::Flexible && flexible_usage + size > total_flexible_size)` | COVERED: FW5 (flexible budget exceeded; EINVAL below SDK 3.50, ENOMEM from 3.50) |
| core/memory.cpp:784 | ENOMEM | `if (total_direct_size < phys_addr + size)` | DIFFERS: MP1: PS4 EACCES at and across the end of direct memory (shadPS4 ENOMEM), all SDKs |
| core/memory.cpp:795 | EACCES | `if (dmem_area->second.dma_type != PhysicalMemoryType::Allocated && dmem_area->second.dma_type != PhysicalMemoryType::Mapped)` | COVERED: E11 (free page), E14 (half free), PS4 EACCES |
| core/memory.cpp:802 | EBUSY | `if (validate_dmem && dmem_area->second.dma_type == PhysicalMemoryType::Mapped)` | COVERED: FW3, B10 (memory already mapped, from SDK 2.50), PS4 EBUSY |
| core/memory.cpp:820 | ENOMEM | `if (!IsValidMapping(virtual_addr, size))` | MP2: PS4 fixed at 0x7FFFFFFFC000 ok below SDK 3.00, EINVAL from 3.00 (area end); at 0x800000000000 EINVAL at all SDKs. shadPS4 returns ENOMEM here where this check is reached first: compare with its run |
| core/memory.cpp:826 | ENOMEM | `if (!vma.IsFree() \|\| remaining_size < size)` | COVERED: RV2 (fixed no-overwrite over a mapping), M4, M5 (0x90), PS4 ENOMEM |
| core/memory.cpp:836 | ENOMEM | `if (virtual_addr == -1 \|\| (default_search && virtual_addr + size > DEFAULT_MAPPING_END))` | COVERED: XC5, LA3 (no free space), MF2, PS4 ENOMEM |
| core/memory.cpp:849 | ENOMEM | `if (True(flags & MemoryMapFlags::Fixed) && !IsInGuestAreas(virtual_addr, size))` | DIFFERS: N6, N3 (the PS4 maps from 0x4000; the shadPS4 guest space starts at 0x400000, a host limit) |
| core/memory.cpp:860 | ENOMEM | `if (base < virtual_addr + size && virtual_addr < base + length && (base < virtual_addr \|\| base + length > virtual_addr + size))` | COVERED: N4 (fixed reservation over half of a pool reservation), W5 (fixed flexible over one), PS4 ENOMEM |

## MemoryManager::MapFile

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1026 | EBADF | `if (file == nullptr)` | COVERED: NU6 (closed fd), PS4 EBADF |
| core/memory.cpp:1032 | EINVAL | `if (file->type == Core::FileSys::FileType::Directory)` | COVERED: SF4 (directory: open ok, mmap EINVAL); FM9 repeats it |
| core/memory.cpp:1036 | EBADF | `if (file->type != Core::FileSys::FileType::Regular)` | DIFFERS: DF1-DF4 (the PS4 maps /dev/dmem1 and /dev/gc; shadPS4 EBADF; games never reach them: libkernel and the GNM driver are HLE) |
| core/memory.cpp:1063 | EACCES | `if (phys_addr < 0 \|\| static_cast<u64>(phys_addr) + size > Common::AlignUp(file_size, 16_KB))` | COVERED: MF11, MF12, MF13, MF15, MF16, MF18 (range past the last page of the file), PS4 EACCES |
| core/memory.cpp:1076 | EACCES | `if (True(prot & (MemoryProt::CpuWrite \| MemoryProt::GpuWrite)))` | COVERED: TF4 (read-only file: shared rw EACCES, private rw ok), TF5; FM9 repeats it |
| core/memory.cpp:1087 | EACCES | `if (True(prot & MemoryProt::CpuReadWrite))` | COVERED: TF3 (write-only file, shared: rw, w, r EACCES; none ok); FM9 repeats it |
| core/memory.cpp:1111 | ENOMEM | `if (!IsValidMapping(virtual_addr, size))` | FM9: PS4 EINVAL; shadPS4 ENOMEM here where reached first: compare with its run |
| core/memory.cpp:1118 | ENOMEM | `if (!vma.IsFree() \|\| remaining_size < size)` | COVERED: FM9 (ENOMEM) |
| core/memory.cpp:1126 | ENOMEM | `if (virtual_addr == -1 \|\| (default_search && virtual_addr + size > DEFAULT_MAPPING_END))` | COVERED: MF2 (default space full), PS4 ENOMEM |

## MemoryManager::PoolDecommit

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1238 | EINVAL | `if (it->second.type != VMAType::PoolReserved && it->second.type != VMAType::Pooled)` | COVERED: PD1 (EINVAL) |

## MemoryManager::PoolMove

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1336 | EINVAL | `if (it == vma_map.end())` | COVERED: PB9 (source unmapped: EINVAL, 0 processed); PB3 a valid move |
| core/memory.cpp:1342 | EINVAL | `if (it->second.type != VMAType::Pooled && it->second.type != VMAType::PoolReserved)` | COVERED: PB9 (source flexible: EINVAL) |
| core/memory.cpp:1349 | EINVAL | `if (it == vma_map.end())` | COVERED: PB9 (EINVAL) |
| core/memory.cpp:1353 | EBUSY | `if (it->second.type == VMAType::Pooled)` | COVERED: PB9 (EBUSY) |
| core/memory.cpp:1357 | EINVAL | `if (it->second.type != VMAType::PoolReserved)` | COVERED: PB9 (EINVAL) |
| core/memory.cpp:1366 | EINVAL | `if (src_reserved)` | COVERED: PB9 (ok, 1 processed; shadPS4 the same: !src_committed returns ok) |

## MemoryManager::CheckMapAreaEnd

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1447 | ENOMEM | `if (addr > MapAreaEnd \|\| addr - size > MapAreaEnd)` | COVERED: SF3 (stack mapping at 0xFC00000000 maps from SDK 3.00) and the SDK chain |
| core/memory.cpp:1454 | EINVAL | `if (addr >= MapAreaEnd \|\| addr + size > MapAreaEnd)` | COVERED: FW4, VQ10, BT6 (from SDK 3.00: fixed EINVAL, searched ENOMEM) |

## MemoryManager::UnmapMemory

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1541 | EINVAL | `if (virtual_addr + size < virtual_addr)` | COVERED: NU1 (wrap-around), PS4 EINVAL |
| core/memory.cpp:1549 | EINVAL | `if (!IsValidMapping(virtual_addr, size))` | MP2: PS4 munmap at 0x7FFFFFFFC000 ok, at 0x800000000000 and 0x2000 EINVAL: compare with the shadPS4 run |
| core/memory.cpp:1560 | EINVAL | `if (base < end_addr && virtual_addr < base + length && (base < virtual_addr \|\| base + length > end_addr))` | COVERED: T15, P83 (part of a pool reservation), PS4 EINVAL |

## MemoryManager::QueryProtection

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1719 | EACCES | `if (addr < min_query_addr)` | COVERED: QP1 (EACCES) |
| core/memory.cpp:1726 | EACCES | `if (vma.IsFree())` | COVERED: QP1 (EACCES) |

## MemoryManager::VirtualQuery

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:1935 | EACCES | `if (addr < query_addr && flags == 0)` | COVERED: VQ10, VQ15 (flag 0 at unmapped addresses), PS4 EACCES |
| core/memory.cpp:1946 | EACCES | `if (it == vma_map.end() \|\| it->second.type == VMAType::Free)` | COVERED: VQ10 (flag 1 past the last mapping), PS4 EACCES |

## MemoryManager::DirectMemoryQuery

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:2037 | EACCES | `if (addr >= total_direct_size)` | COVERED: DQ2 (GetDirectMemoryType ENOENT; DirectMemoryQuery EACCES) |
| core/memory.cpp:2062 | EACCES | `if (dmem_area == dmem_map.end() \|\| !is_allocated(dmem_area->second))` | COVERED: Q1, BP12, BP13, U7, PS4 EACCES |

## MemoryManager::GetDirectMemoryType

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:2328 | ENOENT | `if (addr >= total_direct_size)` | COVERED: DQ2 (GetDirectMemoryType ENOENT; DirectMemoryQuery EACCES) |
| core/memory.cpp:2336 | ENOENT | `if (dmem_area.dma_type != PhysicalMemoryType::Allocated && dmem_area.dma_type != PhysicalMemoryType::Mapped)` | COVERED: BP14 (pool memory), P94 (free page), PS4 ENOENT |

## MemoryManager::IsStack

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:2369 | EACCES | `if (vma.IsFree())` | COVERED: QP1 (EACCES) |

## MemoryManager::SearchFree

| line | error | condition | status |
|---|---|---|---|
| core/memory.cpp:2417 | -1 | `if (!IsValidMapping(virtual_addr))` | COVERED: K1 (hint 0x1000000000: ENOMEM) |
| core/memory.cpp:2464 | -1 | `(unconditional)` | COVERED: XC5, LA3, MF2 (no free space) |

## Error returns in other forms (second pass)

Phase 1 first matched only `return ORBIS_KERNEL_ERROR_...` and `return -1`; these set errno
(ErrSceToPosix, *__Error()) or return (void*)-1, and were found in a second pass over both files.

| line | error | condition | status |
|---|---|---|---|
| kernel/memory.cpp:1095 | EINVAL | `len == 0` (PosixMmapImpl) | COVERED: SF2 (len 0), NU6 (file, len 0), EINVAL |
| kernel/memory.cpp:1112 | EINVAL | `Fixed && vaddr != aligned_addr` (PosixMmapImpl) | COVERED: NU6 (fixed file mapping at +0x1000), EINVAL |
| kernel/memory.cpp:1120 | EINVAL | `Fixed && vaddr == 0, or flag 0x200000` (PosixMmapImpl) | COVERED: FW1, SF2, BT9 |
| kernel/memory.cpp:1126 | EINVAL | `anon with offset or fd; void with offset` (PosixMmapImpl) | COVERED: SF2 |
| kernel/memory.cpp:1134 | EINVAL | `stack without CPU rw, or with a file` (PosixMmapImpl) | COVERED: SF3 |
| kernel/memory.cpp:1140 | ENOMEM | `stack without address; fixed stack over a mapping` (PosixMmapImpl) | COVERED: SF3 |
| kernel/memory.cpp:1185 | result | `result != ORBIS_OK` (PosixMmapImpl) | passes on the MapMemory / MapFile result: see those |
| kernel/memory.cpp:1202 | result | (KernelMmapImpl) | passes on the PosixMmapImpl result |
| kernel/memory.cpp:597, 1240, 1248, 1254, 1264, 1283, 1289, 1295, 1298, 1349 | errno | posix_mprotect, posix_munmap, posix_mlock, posix_munlock, posix_msync | the same conditions as their phase 1 entries (the errno line, one line above the return) |

## Probe IDs used twice (fixed in round 119)

Round 119 found eight IDs used by two different probes; the result files hold both lines under one ID.
Renamed (the older probe keeps its ID): the null-entries batch PB4 -> PB8, the six impossible moves
PB5 -> PB9, BatchMap2 null cases BM1 -> BM5, file access FM3 -> FM9, mlock edges ML1 -> ML3, and two
older ones: "dmem map align 2G, 4G, 3M" M5 -> M6, "pool expand again" P8 -> P96.

## Null-pointer cases

From round 117 every case that passes a null pointer the function writes through (or reads) runs after
DONE, at the last SDK of the chain, one at a time (dmem_null.state keeps the next one, saved before the
call): E18, RV8, PE2, PB8, PB7, BM5, QP2, CF2. Reason: E18 ended the app on the PS4 (round 116). If
one ends the app, starting the app again continues with the next. Their "starting" lines show which.
Round 120: the check for a finished SDK (chain_run_done) searched only the last 512 bytes for DONE, so
the lines these cases write after DONE hid it, and the next start ran that SDK again (and emptied its
results: the SDK 12.00 null-case lines of round 118 were lost this way). It now searches the whole file.

## Round 120 facts (FW 12.02)

- sceKernelGetModuleList: SDK 1.00-3.50 list 30-31 modules (libkernel.sprx is handle 0x2001); from SDK
  5.00 only 1 (the application's), at 5.00, 5.01, 5.03, 5.05, 5.50 and 12.00. shadPS4 listed 5 at 12.00.
- sceKernelDlsym on libkernel.sprx: sceKernelConfiguredFlexibleMemorySize found (returns 0x1C000000,
  448 MiB); sceKernelClearVirtualRangeName not found (0x80020003, ESRCH), though its NID is the hash of
  that name (as shadPS4 registers it). Round 121 tries handle 0x2001 directly from SDK 5.00.
- Null-pointer cases at 5.50 and 12.00 alike: E18, RV8 end the app; PE2 ok; PB8 EFAULT 0; PB7 ok;
  BM5 EFAULT 0 | ok; QP2 ok.
- P22 (EACCES) and Q20 (next allocation) disagree at every SDK for the hole right after a pool block.
  Round 121: Q22-Q25 (before the pool block free, t3, t0 or pool) all give the next allocation, so that
  is not it. Left: P22 expands the pool block before allocating after it, build() allocates first; Q26
  (round 122) uses P22's order.
- Round 121: sceKernelDlsym with handle 0x2001 works from SDK 5.00 (the module list does not show
  libkernel): sceKernelConfiguredFlexibleMemorySize returns 0x1C000000 at 5.50 and 12.00 too;
  sceKernelClearVirtualRangeName is ESRCH that way as well (not exported by libkernel on FW 12.02).

## Exported functions never called before

All are probed now: sceKernelConfiguredFlexibleMemorySize (CF1), sceKernelMapNamedSystemFlexibleMemory
(SY1), sceKernelSetPrtAperture and sceKernelGetPrtAperture (PRT1), sceKernelClearVirtualRangeName (CL1,
CL2; shadPS4 sets the name to "" and always returns 0, nothing measured before). The two not in
OpenOrbis' libkernel stub are looked up in libkernel by name. Not probed: the trivial
sceKernelIsAddressSanitizerEnabled and _sceKernelRtldSetApplicationHeapAPI.

## Ambiguous probes

BP13, CV98: find-next from pool memory returns EACCES, as it would if pool memory ended the search
or if it were skipped with nothing allocated after it: BQ1 separates the two. M4 "dmem"/"part": the
address was not 2 MiB aligned on the PS4, so they measured the alignment check: M5 measures the overlap.

## What comes after the error returns

Phase 1 (this file): every error return mapped. Done when every UNTESTED entry has its run.
Phase 2 (mapped; its UNTESTED entries wait for their runs): every rule on the success paths (what a call does when it succeeds: merging, splitting,
reported regions and fields, protections, layout, counters), mapped the same way: each rule in the code
to the probe that measures it, or UNTESTED.
Phase 3 (done, round 116): every probe checked for a setup that lets its result follow from more than
one rule (as BP13 and M4 did): 604 probes, 585 clear, 13 ambiguous (each fixed by guards or separated
by another probe), 6 dead (never run; their questions measured by other probes).
Next: the PS4 run of everything added since round 99, the same packages in shadPS4, and fixes where
shadPS4 differs.

# Phase 2: success-path rules

Every decision in the same code that changes what a successful call does or reports: each if / else
branch on a path that does not end in an error. Status: TO MAP; COVERED (probe, PS4 result); UNTESTED;
INTERNAL (bookkeeping or host mechanics, not a PS4 rule: the entry names the probes that measure its
effect, or says why it has none). The "cited" column only says a note sits near the branch.

Total: 219 decision points in 64 functions.

## P2 MemoryManager::MemoryManager

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:33 | `if (extra_dmem != 0) {` |  | INTERNAL: emulator setting (extra direct memory) |
| memory.cpp:37 | `if (extra_fmem != 0) {` |  | INTERNAL: emulator setting (extra flexible memory) |

## P2 MemoryManager::SetupMemoryRegions

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:60 | `if (EmulatorSettings.IsDevKit()) {` |  | INTERNAL: emulator setting (devkit sizes) |
| memory.cpp:64 | `if (extra_dmem != 0) {` |  | INTERNAL: emulator setting |
| memory.cpp:71 | `if (extra_fmem != 0) {` |  | INTERNAL: emulator setting |
| memory.cpp:78 | `if (!use_extended_mem1 && is_neo) {` |  | UNTESTED: extended memory mode (needs a package with the extended memory attribute; base console only) |
| memory.cpp:81 | `if (!use_extended_mem2 && !is_neo) {` |  | UNTESTED: as 78 |

## P2 MemoryManager::ClampRangeSize

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:105 | `if (size < MinSizeToClamp) {` |  | INTERNAL: host range clamping |
| memory.cpp:125 | `if (size != clamped_size) {` |  | INTERNAL: as 105 |

## P2 MemoryManager::SetPrtArea

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:134 | `if (area.mapped) {` |  | INTERNAL: PRT area bookkeeping; the API in PRT1 |

## P2 MemoryManager::CopySparseMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:153 | `if (auto* entry = backing_pages.find(page); entry && *entry) {` |  | INTERNAL: host copy |

## P2 MemoryManager::SaveFileTail

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:178 | `if (file == nullptr \|\| !file->is_opened \|\| !file->handle) {` |  | INTERNAL: guard (file gone); effect MF24 |
| memory.cpp:182 | `if (host_file == nullptr) {` |  | INTERNAL: guard (no host file) |
| memory.cpp:191 | `if (from >= to) {` |  | INTERNAL: guard (nothing past the end) |
| memory.cpp:195 | `if (tail.file_size != file_size \|\| tail.bytes.size() != page_end - tail_start) {` |  | COVERED: MFT1 (file grown by a write: the bytes read 0) |
| memory.cpp:200 | `if (False(vma.prot & MemoryProt::CpuRead)) {` |  | INTERNAL: host read access to copy |

## P2 MemoryManager::RestoreFileTail

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:217 | `if (it == file_tails.end() \|\| it->second.file_size != file_size) {` |  | DIFFERS: MFT1: on the PS4 the bytes past the end survive close and reopen (0x66 read back, size 0x2800); a write that grows the file over them zeroes them. shadPS4 drops them in close() (DropFileTail) |
| memory.cpp:223 | `if (from >= to) {` |  | INTERNAL: guard |

## P2 MemoryManager::GuestWritablePrefix

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:275 | `if (next == vma_map.begin()) {` |  | INTERNAL: host helper for kernel writes into guest memory |
| memory.cpp:280 | `if (addr >= vma.base + vma.size) {` |  | INTERNAL: as 275 |
| memory.cpp:284 | `if (vma.IsFree() \|\| False(vma.prot & MemoryProt::CpuWrite)) {` |  | INTERNAL: as 275 |

## P2 MemoryManager::TryWriteBacking

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:301 | `if (!HasPhysicalBacking(current_vma->second)) {` |  | INTERNAL: host helper |
| memory.cpp:316 | `if (!size) {` |  | INTERNAL: host helper |

## P2 MemoryManager::PoolExpand

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:347 | `if (dmem_area == dmem_map.end()) {` |  | INTERNAL: search loop guard |

## P2 MemoryManager::SetCommittedPoolBlockType

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:402 | `if (memory_type < 0 \|\| memory_type > 10) {` |  | COVERED via counters: U8, W1, X3, Y4, P71 |
| memory.cpp:407 | `if (pool_flushed_blocks.contains(block) == flushed) {` |  | COVERED via counters: U8, W1 |
| memory.cpp:410 | `if (flushed) {` |  | COVERED via counters: U8, W1, X3 |

## P2 MemoryManager::Allocate

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:473 | `if (dmem_area == dmem_map.end()) {` |  | INTERNAL: search loop guard |

## P2 MemoryManager::Free

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:501 | `if (is_checked && phys_addr >= CheckedReleaseSuccessStart) {` | yes | COVERED: H7, H8, XC3 (checked release from 0x5000000000 returns 0) |
| memory.cpp:524 | `if (area_end <= current_phys_addr) {` |  | INTERNAL: empty slice |
| memory.cpp:541 | `if (mapping.type != VMAType::Direct) {` |  | COVERED: F1-F9 (released memory: its mappings go) |
| memory.cpp:545 | `if (release_end > phys_mapping.base && phys_addr < phys_mapping.base + phys_mapping.size) {` |  | COVERED: F1-F9, D12 |
| memory.cpp:562 | `if (IsValidGpuMapping(addr, unmap_size)) {` |  | INTERNAL: GPU (rasterizer) |

## P2 MemoryManager::PoolCommit

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:636 | `if (flushed_short) {` | yes | COVERED: M1, Z1, Y4, NU12 (flushed / cached blocks when flushed ones run out) |
| memory.cpp:641 | `if (True(prot & MemoryProt::CpuWrite)) {` | yes | COVERED: PC1 (prot 2 reads back 3 from SDK 10.00; 0x33 below, the reservation rule) |
| memory.cpp:660 | `if (handle->second.dma_type != PhysicalMemoryType::Pooled) {` | yes | INTERNAL: search guard |
| memory.cpp:674 | `if (mtype == 3) {` |  | COVERED: M1, Z1 (type 3 block accounting) |
| memory.cpp:703 | `if (IsValidGpuMapping(mapped_addr, size)) {` |  | INTERNAL: GPU |

## P2 MemoryManager::CreateArea

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:715 | `if (True(flags & MemoryMapFlags::Fixed)) {` |  | COVERED: RV3, M4, N5 (a fixed mapping replaces what it covers) |
| memory.cpp:737 | `if (True(prot & MemoryProt::CpuWrite)) {` |  | COVERED: NU2 (write implies read) |
| memory.cpp:754 | `if (type == VMAType::Direct && VirtualMemoryArea::IsCallerName(new_vma.name)) {` |  | COVERED: FW7, FW8 (caller of a direct mapping) |

## P2 MemoryManager::MapMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:766 | `if (type == VMAType::Flexible && flexible_usage + size > total_flexible_size) {` |  | see phase 1 (flexible budget, FW5) |
| memory.cpp:780 | `if (phys_addr != -1) {` | yes | see phase 1 (direct memory validation: E11, E14, FW3, MP1) |
| memory.cpp:810 | `if (True(flags & MemoryMapFlags::Fixed)) {` |  | control flow: fixed mapping (see 811, 815, 873) |
| memory.cpp:811 | `if (const s32 r = CheckMapAreaEnd(virtual_addr, size, true, stack); r != ORBIS_OK) {` |  | see phase 1 (CheckMapAreaEnd) |
| memory.cpp:815 | `if (True(flags & MemoryMapFlags::Fixed) && True(flags & MemoryMapFlags::NoOverwrite)) {` |  | see phase 1 (fixed no-overwrite: RV2) |
| memory.cpp:828 | `} else if (False(flags & MemoryMapFlags::Fixed)) {` |  | non-fixed: placement by SearchFree (see P2 SearchFree) |
| memory.cpp:840 | `if (False(flags & MemoryMapFlags::Fixed)) {` |  | control flow: non-fixed (see 841) |
| memory.cpp:841 | `if (const s32 r = CheckMapAreaEnd(virtual_addr, size, false, stack); r != ORBIS_OK) {` |  | see phase 1 (CheckMapAreaEnd) |
| memory.cpp:852 | `if (True(flags & MemoryMapFlags::Fixed)) {` | yes | see phase 1 (partial pool reservation: N4, W5) |
| memory.cpp:866 | `if (IsValidGpuMapping(virtual_addr, size)) {` |  | INTERNAL: GPU unmap before mapping (rasterizer), no PS4 rule |
| memory.cpp:873 | `if (True(flags & MemoryMapFlags::Fixed)) {` |  | COVERED: M4 (fixed reservation 0x10 over a whole pool reservation: ok, counters unchanged, blocks given back and taken again) |
| memory.cpp:885 | `if (type == VMAType::Reserved && True(flags & MemoryMapFlags::Void) && True(flags & MemoryMapFlags::Fixed)) {` |  | COVERED: CO8 (reserve 400111, middle 111), CO2, CO3 (end of the reserved range kept for no_merge_from_left) |
| memory.cpp:895 | `if (type == VMAType::Reserved && False(flags & MemoryMapFlags::Void)) {` |  | COVERED: CO8, CO2, CO3 (as 885) |
| memory.cpp:902 | `if (type == VMAType::Flexible) {` |  | INTERNAL: host memory for flexible mappings; its effects are the budget (FL1-FL6) and contents |
| memory.cpp:908 | `if (handle->second.dma_type != PhysicalMemoryType::Free) {` |  | INTERNAL: as 902 |
| memory.cpp:943 | `} else if (type == VMAType::Direct) {` |  | COVERED via its effects: FW3, B10 (EBUSY from 2.50), CV probes (region reporting) |
| memory.cpp:975 | `if ((type == VMAType::Reserved && True(flags & MemoryMapFlags::Void)) \|\| (type == VMAType::Flexible && prot == MemoryProt::NoAccess)) {` |  | COVERED: CO2, CO3, CO11-CO14, CO16 (merging of void reservations and no-access mappings) |
| memory.cpp:987 | `if (!merged_left && !merged_right && at_reserved_end) {` | yes | COVERED: CO8 (no_merge_from_left) |
| memory.cpp:990 | `} else if (new_vma.type != VMAType::Direct \|\| sdk_version >= Common::ElfInfo::FW_200) {` |  | COVERED: FW2, CO4, TP2, FW8 and the SDK chain (direct mappings merge from SDK 2.00) |
| memory.cpp:997 | `if (type != VMAType::Reserved && type != VMAType::PoolReserved) {` |  | INTERNAL: host address-space mapping |
| memory.cpp:999 | `if (type != VMAType::Flexible) {` |  | INTERNAL: as 997 |
| memory.cpp:1009 | `if (IsValidGpuMapping(mapped_addr, size)) {` |  | INTERNAL: GPU mapping (rasterizer) |

## P2 MemoryManager::MapFile

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1039 | `if (True(prot & MemoryProt::CpuWrite)) {` | yes | COVERED: TF1 (rw file, shared rw), NU2 (write implies read) |
| memory.cpp:1051 | `if (non_host_backed) {` |  | INTERNAL: non-host file back ends (read-only); no PS4 rule of its own |
| memory.cpp:1066 | `if (False(host_file->GetAccessMode() & Common::FS::FileAccessMode::Write) && False(host_file->GetAccessMode() & Common::FS::FileAccessMode::Append)) {` | yes | COVERED: TF4, TF5 (file without write access: shared write EACCES, private copy) |
| memory.cpp:1095 | `if (prot >= MemoryProt::GpuRead) {` |  | COVERED: TF6 (GPU protection on a file mapping succeeds) |
| memory.cpp:1102 | `if (True(prot & MemoryProt::CpuExec)) {` | yes | COVERED: PT3, XR6 (execute dropped: p=1, does not run) |
| memory.cpp:1107 | `if (True(flags & MemoryMapFlags::Fixed) && True(flags & MemoryMapFlags::NoOverwrite)) {` | yes | see phase 1 (fixed no-overwrite) |
| memory.cpp:1120 | `} else if (False(flags & MemoryMapFlags::Fixed)) {` |  | see phase 1 (search: MF2) |
| memory.cpp:1129 | `if (const s32 r = CheckMapAreaEnd(virtual_addr, size, True(flags & MemoryMapFlags::Fixed), false); r != ORBIS_OK) {` |  | see phase 1 (CheckMapAreaEnd) |
| memory.cpp:1136 | `if (IsValidGpuMapping(virtual_addr, size)) {` |  | INTERNAL: GPU unmap (rasterizer) |
| memory.cpp:1173 | `if (True(mp & MemoryProt::CpuRead)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1176 | `if (True(mp & MemoryProt::CpuReadWrite)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1179 | `if (True(mp & MemoryProt::CpuExec)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1189 | `if (write_only) {` | yes | COVERED: TF3, TF6 (write-only file) |
| memory.cpp:1195 | `} else if (host_file != nullptr && False(flags & MemoryMapFlags::Shared)) {` | yes | COVERED: TF2, TF5 (private mapping is a copy) |
| memory.cpp:1200 | `if (to_read != 0) {` | yes | COVERED via its effect: MF22 (bytes past the end of the file read 0), TF2 |
| memory.cpp:1210 | `if (host_file != nullptr) {` |  | INTERNAL: host file mapping |
| memory.cpp:1215 | `if (prot != Core::MemoryProt::CpuRead) {` |  | INTERNAL: host protection after the copy |

## P2 MemoryManager::PoolDecommit

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1244 | `if (IsValidGpuMapping(virtual_addr, size)) {` |  | INTERNAL: GPU |
| memory.cpp:1259 | `if (vma_base.type == VMAType::Pooled) {` |  | COVERED: NU5 (decommit of committed memory; twice is ok) |

## P2 MemoryManager::PoolMove

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1361 | `if (!src_committed) {` |  | COVERED: PB9 (ok, 1 processed; shadPS4 the same: !src_committed returns ok) |
| memory.cpp:1368 | `if (IsValidGpuMapping(src_addr, size)) {` |  | INTERNAL: GPU |
| memory.cpp:1427 | `if (IsValidGpuMapping(dest_addr, size)) {` |  | INTERNAL: GPU |

## P2 MemoryManager::CheckMapAreaEnd

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1440 | `if (static_cast<u32>(sdk_version) < Common::ElfInfo::FW_300 \|\| (addr >> 47) != 0) {` | yes | COVERED: SDK chain (no check below SDK 3.00) |

## P2 MemoryManager::ContainsNoCpuAccess

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1463 | `if (vma.IsFree()) {` |  | COVERED: ML3 (ENOMEM) |
| memory.cpp:1466 | `if (vma.type == VMAType::Reserved \|\| vma.type == VMAType::PoolReserved \|\| False(vma.prot & (MemoryProt::CpuRead \| MemoryProt::CpuExec))) {` |  | COVERED: ML3 (no CPU access: ENOMEM) |

## P2 MemoryManager::IsPhysicalMemoryAliased

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1502 | `if (vma.type != VMAType::Direct) {` |  | COVERED: EB1 (mtypeprotect over an alias: EBUSY) |
| memory.cpp:1508 | `if (start < end) {` |  | COVERED: EB1 |
| memory.cpp:1515 | `if (vma.type != VMAType::Direct) {` |  | COVERED: EB1 |
| memory.cpp:1522 | `if (start < end && vma.base + offset + (start - area.base) != piece.va + (start - piece.pa)) {` |  | COVERED: EB1 ("alias gone": ok) |

## P2 MemoryManager::UnmapMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1533 | `if (size == 0) {` |  | see phase 1 (munmap len 0: NU1, PX2) |
| memory.cpp:1566 | `if (IsValidGpuMapping(virtual_addr, size)) {` |  | INTERNAL: GPU |

## P2 MemoryManager::UnmapBytesFromEntry

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1585 | `if (vma_base.type == VMAType::Free) {` |  | COVERED: NU1 (+hole: ok) |
| memory.cpp:1588 | `if (vma_type == VMAType::File && vma_base.file_shared) {` |  | COVERED: MF24 (bytes past the end of the file kept) |
| memory.cpp:1591 | `if (vma_type == VMAType::Pooled) {` |  | COVERED: P52, P53, T15 (committed pool memory unmapped: blocks back to the pool) |
| memory.cpp:1597 | `if (vma_base.phys_areas.size() > 0) {` |  | INTERNAL: walk over the physical areas |
| memory.cpp:1607 | `if (vma_type == VMAType::Direct) {` |  | COVERED: F3, CV probes (direct memory stays allocated after munmap) |
| memory.cpp:1623 | `} else if (vma_type == VMAType::Pooled) {` |  | COVERED: P52, P53, T15 |
| memory.cpp:1636 | `} else if (vma_type == VMAType::Flexible) {` |  | COVERED: FL4 (budget), FZ1 (contents 0) |
| memory.cpp:1688 | `if (vma_type != VMAType::Reserved && vma_type != VMAType::PoolReserved) {` |  | INTERNAL: host unmap |

## P2 MemoryManager::QueryProtection

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1733 | `if ((vma.type == VMAType::Pooled \|\| vma.type == VMAType::PoolReserved) && static_cast<u32>(sdk_version) < 0x10000000) {` | yes | COVERED: PG4, PG1, CV40-CV94 (pool memory below SDK 10.00: reservation and 0x33) |
| memory.cpp:1736 | `if (reservation != pool_reservations.end()) {` | yes | COVERED: PG4 |
| memory.cpp:1741 | `if (start != nullptr) {` | yes | COVERED: QP2 (ok) |
| memory.cpp:1744 | `if (end != nullptr) {` |  | COVERED: QP2 (ok) |
| memory.cpp:1747 | `if (prot != nullptr) {` |  | COVERED: QP2 (ok) |

## P2 MemoryManager::ProtectBytes

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1772 | `if (vma_base.type == VMAType::Pooled) {` | yes | COVERED: EX5 (pool commit 0x3, mprotect 0x7: ok, prot 0x33, does not run) |
| memory.cpp:1777 | `if (vma_base.type == VMAType::File) {` | yes | COVERED: PT3, XR6 (file mapping given execute does not run it) |
| memory.cpp:1783 | `if (vma_base.type == VMAType::File && vma_base.file_read_only) {` | yes | COVERED: TF4 (read-only file, shared: mprotect rw succeeds, prot stays 1) |
| memory.cpp:1791 | `if (vma_base.type == VMAType::Free \|\| vma_base.type == VMAType::PoolReserved) {` | yes | PARTLY: free memory PX2 (hole), PT2 (gap): ok, nothing changes; reserved pool memory MPR1 (ok; reads back 0x33 below SDK 10.00, 0 from 10.00) |
| memory.cpp:1796 | `if (True(prot & MemoryProt::CpuWrite)) {` | yes | COVERED: NU2 (mprotect write-only reads back 0x3) |
| memory.cpp:1804 | `if (vma_base.type == VMAType::File && vma_base.file_write_only) {` | yes | COVERED: TF3, TF6 (write-only file: no read) |
| memory.cpp:1811 | `if (True(prot & MemoryProt::CpuRead)) {` | yes | INTERNAL: host permission bits |
| memory.cpp:1814 | `if (True(prot & MemoryProt::CpuReadWrite)) {` | yes | INTERNAL: host permission bits |
| memory.cpp:1817 | `if (True(prot & MemoryProt::CpuExec)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1820 | `if (True(prot & MemoryProt::GpuRead)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1823 | `if (True(prot & MemoryProt::GpuWrite)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1826 | `if (True(prot & MemoryProt::GpuReadWrite)) {` |  | INTERNAL: host permission bits |
| memory.cpp:1830 | `if (vma_base.type == VMAType::Direct \|\| vma_base.type == VMAType::Pooled \|\| vma_base.type == VMAType::File) {` |  | COVERED: XR2, XR3, EX1, PT3 (execute runs or not, hidden from the queries) |
| memory.cpp:1846 | `if ((vma_base.type != VMAType::Direct \|\| static_cast<u32>(sdk_version) >= Common::ElfInfo::FW_200) && !vma_base.void_private) {` | yes | COVERED: FW2, TP2 (direct, SDK 2.00), CO3 (void without shared) |
| memory.cpp:1852 | `if (vma_base.type == VMAType::Reserved) {` | yes | COVERED: RV4, PT1 (reserved range: split and merged by prot, stays inaccessible) |
| memory.cpp:1859 | `if (new_prot != old_prot) {` |  | INTERNAL: host protection change |

## P2 MemoryManager::Protect

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1868 | `if (size == 0) {` |  | COVERED: NU2 (len 0: ok) |
| memory.cpp:1879 | `if (addr < vbase) {` |  | COVERED: MPR1 (mprotect at 0x2000: ok) |
| memory.cpp:1881 | `if (skip >= size) {` |  | COVERED: MPR1 |
| memory.cpp:1887 | `if (addr >= vend) {` |  | COVERED: MPR1 (at 0x800000000000: ok) |
| memory.cpp:1890 | `if (addr + size > vend) {` |  | COVERED: MPR1 (across 0x800000000000: ok) |
| memory.cpp:1903 | `if (it == vma_map.end()) {` |  | COVERED: PX2 (hole), PT2 (gap), NU2 (+hole) |
| memory.cpp:1907 | `if (vma_base.base > addr + protected_bytes) {` |  | COVERED: PT2 (gap inside the range) |
| memory.cpp:1910 | `if (protected_bytes >= size) {` |  | COVERED: PT2 |
| memory.cpp:1915 | `if (result < 0) {` |  | COVERED: TP1 (type 10 rw: EACCES) |

## P2 MemoryManager::VirtualQuery

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1961 | `if (vma.type == VMAType::Direct) {` | yes | COVERED: DT1 (offset of a direct mapping relative to its phys) |
| memory.cpp:1969 | `if (phys != vma.phys_areas.begin()) {` | yes | COVERED: DT1, CV probes |
| memory.cpp:1990 | `if (!runs.empty() && runs.back().end == start && runs.back().type == dmem.memory_type) {` |  | COVERED: CV probes, VQ4 (runs of memory type) |
| memory.cpp:2000 | `if (query_offset >= run.start && query_offset < run.end) {` |  | COVERED: CV probes |
| memory.cpp:2008 | `if (vma.type == VMAType::PoolReserved \|\| vma.type == VMAType::Pooled) {` |  | COVERED: VQ1, VQ2, VQ3 (pool flags 8 / 0x18) |
| memory.cpp:2012 | `if (vma.type == VMAType::Pooled && !vma.phys_areas.empty()) {` | yes | COVERED: VQ1, VQ2, VQ5 (committed pool: offset, type) |
| memory.cpp:2017 | `if (phys != vma.phys_areas.begin()) {` | yes | COVERED: VQ2 |
| memory.cpp:2023 | `if (vma.type == VMAType::Reserved \|\| vma.type == VMAType::PoolReserved) {` |  | COVERED: RV4, VQ3 (reserved: prot 0, flags 0) |

## P2 MemoryManager::DirectMemoryQuery

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2046 | `if (find_next) {` |  | COVERED: Q1, Q3, Q12-Q16, CV98 |
| memory.cpp:2049 | `if (dmem_area->second.dma_type == PhysicalMemoryType::Free && dmem_area->second.base == addr && dmem_area != dmem_map.begin() && std::prev(dmem_area)-` |  | COVERED: Q12 (area ending at the address) |
| memory.cpp:2075 | `if (in_region(dmem_area->second)) {` |  | COVERED: Q4, Q5, CV probes (region in both directions) |

## P2 MemoryManager::DirectQueryAvailable

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2098 | `if (dmem_area->second.dma_type != PhysicalMemoryType::Free) {` |  | COVERED: AV1-AV3, H4, SF8 |
| memory.cpp:2110 | `if (remaining_size > max_size) {` | yes | COVERED: AV3 (largest area) |

## P2 MemoryManager::ContainsVmaType

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2138 | `if (it->second.type == type) {` |  | helper: callers mapped (RV7 reservation over direct; TP3 mtypeprotect on a reservation) |

## P2 MemoryManager::HasDirectMemoryType

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2149 | `if (vma.type != VMAType::Direct) {` |  | COVERED: TP1 (mprotect rw on type 10) |
| memory.cpp:2154 | `if (start < addr + size && start + area.size > addr && area.memory_type == memory_type) {` |  | COVERED: TP1 |

## P2 MemoryManager::ContainsFlexibleMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2166 | `if (it->second.type == VMAType::Flexible) {` |  | COVERED: NU3, TP3 (mtypeprotect on flexible memory) |

## P2 MemoryManager::SetDirectMemoryType

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2193 | `if (vma_handle->second.type == VMAType::PoolReserved && memory_type != 3) {` | yes | COVERED: NU14 (mtypeprotect on reserved pool memory: counters) |
| memory.cpp:2199 | `if (vma_handle->second.type == VMAType::PoolReserved) {` | yes | COVERED: NU14 (reserved pool memory reads back rw afterwards) |
| memory.cpp:2204 | `if (vma_handle->second.type == VMAType::Direct \|\| vma_handle->second.type == VMAType::Pooled) {` | yes | COVERED: U8, U9, T17, W1 (types reported) |
| memory.cpp:2210 | `if (vma_handle->second.type == VMAType::Pooled) {` | yes | COVERED: U8, X3, Y4 (pool block type, counters) |
| memory.cpp:2243 | `if (from_map_direct2) {` | yes | COVERED: DT5, DT7, DT8, CV23 (type set by MapDirectMemory2 keeps the area apart) |
| memory.cpp:2245 | `} else if (vma_handle->second.type == VMAType::Direct) {` | yes | COVERED: TP2 (type set by mtypeprotect lets areas merge) |
| memory.cpp:2262 | `if (vma_handle->second.type != VMAType::Direct \|\| static_cast<u32>(sdk_version) >= Common::ElfInfo::FW_200) {` | yes | COVERED: TP2, FW2 (below SDK 2.00 no merge again) |
| memory.cpp:2266 | `if (vma_handle->second.base + vma_handle->second.size <= current_addr) {` | yes | INTERNAL: next area |

## P2 MemoryManager::NameVirtualRange

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2292 | `if (!it->second.IsFree()) {` |  | COVERED: NM3 (name over a hole: skipped) |
| memory.cpp:2293 | `if (size_in_vma < it->second.size) {` |  | COVERED: NM3 (half of a mapping: split) |
| memory.cpp:2305 | `if (it->second.base + it->second.size <= current_addr) {` |  | INTERNAL: next area |
| memory.cpp:2317 | `if (!it->second.IsFree()) {` | yes | COVERED: FW9 (rename merges) |

## P2 MemoryManager::IsStack

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2374 | `if (vma.type == VMAType::Stack) {` |  | COVERED: MS3 (stack: yes, flexible: no) |
| memory.cpp:2379 | `if (start != nullptr) {` |  | COVERED: QP2 (ok) |
| memory.cpp:2383 | `if (end != nullptr) {` |  | COVERED: QP2 (ok) |

## P2 MemoryManager::InvalidateMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2399 | `if (rasterizer) {` |  | INTERNAL: GPU |

## P2 MemoryManager::SearchFree

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2410 | `if (virtual_addr < min_search_address) {` |  | DIFFERS (host limit, as N6): SH1: the PS4 places a search at the hints 0x4000 and 0x100000 exactly |
| memory.cpp:2425 | `if (it->second.IsFree() && it->second.Contains(virtual_addr, size)) {` |  | COVERED: K1 (hints 0x10000000, 0x300000000, 0x7F0000000, 0x900000000: exactly there), MF4 (rel hint 0) |
| memory.cpp:2434 | `if (!it->second.IsFree()) {` |  | COVERED: MF3 (hint inside a reservation: placed after it), HB2, HB4 |
| memory.cpp:2442 | `if (virtual_addr > vma.base + vma.size) {` |  | COVERED: N2, K5, AL3 (alignment larger than a free area) |
| memory.cpp:2448 | `if (virtual_addr >= max_search_address) {` |  | COVERED: XC5, LA3 (nothing free below the limit) |
| memory.cpp:2455 | `if (remaining_size >= size) {` |  | COVERED: MF3, K1 (first free area after the hint that fits) |

## P2 MemoryManager::MergeWithNext

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2479 | `if (next_vma->second.reserve_end != 0) {` |  | COVERED: CO8 (end of a reserved range carried over) |

## P2 MemoryManager::MergeWithPrev

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2488 | `if (iter == handle_map.begin()) {` |  | INTERNAL: first area |
| memory.cpp:2492 | `if (!prev_vma->second.CanMergeWith(iter->second, ignore_callers)) {` |  | COVERED: CO1-CO16, FW6-FW9 (merge conditions) |
| memory.cpp:2501 | `if (iter->second.reserve_end != 0) {` |  | COVERED: CO8 |

## P2 MemoryManager::MergeAdjacent

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2515 | `if (next_vma != handle_map.end() && iter->second.CanMergeWith(next_vma->second)) {` |  | COVERED: CO probes, FW2, TP2 |
| memory.cpp:2520 | `if (iter != handle_map.begin()) {` |  | INTERNAL: first area |
| memory.cpp:2522 | `if (prev_vma->second.CanMergeWith(iter->second)) {` |  | COVERED: CO probes |

## P2 MemoryManager::CarveVMA

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2541 | `if (start_in_vma == 0 && size == vma.size) {` |  | INTERNAL: splitting; effect in the CO and CV areas |
| memory.cpp:2548 | `if (end_in_vma != vma.size) {` |  | INTERNAL: as 2541 |
| memory.cpp:2552 | `if (start_in_vma != 0) {` |  | INTERNAL: as 2541 |

## P2 MemoryManager::CarvePhysArea

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2572 | `if (end_in_vma != area.size) {` |  | INTERNAL: splitting; effect in the CV regions |
| memory.cpp:2576 | `if (start_in_area != 0) {` |  | INTERNAL: as 2572 |

## P2 MemoryManager::Split

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:2593 | `if (HasPhysicalBacking(new_vma)) {` |  | INTERNAL: splitting physical areas with a mapping |
| memory.cpp:2600 | `if (offset + region.size <= offset_in_vma) {` |  | INTERNAL: as 2593 |
| memory.cpp:2604 | `if (offset < offset_in_vma && offset + region.size > offset_in_vma) {` |  | INTERNAL: as 2593 |
| memory.cpp:2616 | `if (offset >= offset_in_vma) {` |  | INTERNAL: as 2593 |

## P2 ?

| line | condition | cited | status |
|---|---|---|---|
| memory.h:242 | `if (size == 0) {` | yes | INTERNAL: size 0 helper |
| memory.h:307 | `if (size == 0) {` |  | INTERNAL: size 0 helper |

## P2 sceKernelCheckedReleaseDirectMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:112 | `if (len == 0) {` |  | COVERED: A12 (checked release, len 0: ok, still allocated); RL3 repeats it |

## P2 sceKernelReleaseDirectMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:130 | `if (len == 0) {` |  | COVERED: D14 (len 0: ok, page stays allocated) |

## P2 sceKernelAvailableDirectMemorySize

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:158 | `if (physAddrOut != nullptr) {` |  | COVERED: H5 (both null), T12 (each null) |
| memory.cpp:161 | `if (sizeOut != nullptr) {` |  | COVERED: H5, T12 |

## P2 VirtualQueryImpl

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:182 | `if (result != ORBIS_OK \|\| system_caller) {` |  | COVERED: VQ15 (query of the system area from the application: EACCES) |

## P2 sceKernelMapNamedDirectMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:311 | `if (via_map2 && sdk < Common::ElfInfo::FW_300) {` |  | COVERED: BT6, VQ10-VQ17 and the SDK chain (MapDirectMemory2 below 3.00: prot bits 0x100 and up) |
| memory.cpp:351 | `if (via_map2) {` |  | COVERED: LA1, LA2, VQ11-VQ13 (system area, MapDirectMemory2 from 2.50) |

## P2 sceKernelMapDirectMemory2

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:451 | `if (type != -1) {` |  | COVERED: SF7, DT5, MS1 (type set unless -1) |

## P2 sceKernelMapNamedSystemFlexibleMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:544 | `if (in_addr == 0) {` |  | COVERED: SY1 (valid mapping: ok, prot 3, name sy1) |

## P2 sceKernelMprotect

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:568 | `if (aligned_size == 0) {` |  | COVERED: NU2 (len 0: ok) |

## P2 sceKernelMtypeprotect

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:632 | `if (aligned_size == 0) {` |  | COVERED: MT0 (size 0: ok) |

## P2 sceKernelDirectMemoryQuery

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:685 | `if (result != ORBIS_OK \|\| copy_size == 0) {` |  | COVERED: Q probes, BP12, U7 |

## P2 sceKernelBatchMap2

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:757 | `if (entries == nullptr \|\| entries[i].length == 0 \|\| entries[i].operation > 4) {` | yes | COVERED: BM5 (EFAULT, 0) |
| memory.cpp:797 | `if (numEntriesOut != NULL) { // can be zero. do not return an error code.` |  | COVERED: BM5 (ok) |

## P2 sceKernelMemoryPoolCommit

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:961 | `if (len == 0) {` | yes | COVERED: NU5 (len 0: ok) |

## P2 sceKernelMemoryPoolDecommit

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:984 | `if (len == 0) {` | yes | COVERED: NU10 (len 0: ok) |

## P2 sceKernelMemoryPoolBatch

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1043 | `if (!Common::Is64KBAligned(dest) \|\| !Common::Is64KBAligned(src) \|\| !Common::Is64KBAligned(len)) {` |  | COVERED: PB6 (EINVAL) |
| memory.cpp:1063 | `if (num_processed != nullptr) {` | yes | COVERED: PB7 (ok) |

## P2 PosixMmapImpl

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1119 | `if ((True(mem_flags & Core::MemoryMapFlags::Fixed) && vaddr == 0) \|\| (flags & 0x200000) != 0) {` | yes | COVERED: FW1, SF2, BT9 (null with Fixed; flag 0x200000), EINVAL |
| memory.cpp:1124 | `if ((True(mem_flags & Core::MemoryMapFlags::Anon) && (phys_addr != 0 \|\| fd != -1)) \|\| (True(mem_flags & Core::MemoryMapFlags::Void) && phys_addr != 0)` | yes | COVERED: SF2 (anon with offset, anon with fd, void with offset: EINVAL; void ok) |
| memory.cpp:1129 | `if (True(mem_flags & Core::MemoryMapFlags::Stack)) {` | yes | control flow: stack mapping (1132, 1137) |
| memory.cpp:1132 | `if ((mem_prot & Core::MemoryProt::CpuReadWrite) != Core::MemoryProt::CpuReadWrite \|\| fd != -1) {` |  | COVERED: SF3 (stack read-only, stack with fd: EINVAL) |
| memory.cpp:1137 | `if ((False(mem_flags & Core::MemoryMapFlags::Fixed) && vaddr == 0) \|\| (True(mem_flags & Core::MemoryMapFlags::Fixed) && memory->IsRangeMapped(aligned_` |  | COVERED: SF3 (stack without address: ENOMEM; fixed stack over a mapping: ENOMEM) |
| memory.cpp:1147 | `if (True(mem_flags & Core::MemoryMapFlags::Anon) && False(mem_flags & Core::MemoryMapFlags::Void)) {` | yes | COVERED: BT10 (anonymous: CPU rwx and GPU rw kept, other bits dropped), BT9 (System flag) |
| memory.cpp:1160 | `} else if (True(mem_flags & Core::MemoryMapFlags::Stack)) {` | yes | COVERED: SF3 (stack mapping at 0xFC00000000) |
| memory.cpp:1164 | `} else if (True(mem_flags & Core::MemoryMapFlags::Void)) {` |  | COVERED: BT9, CO2, CO3, SF2 (void reservation) |

## P2 KernelMmapImpl

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1202 | `if (addr_out == reinterpret_cast<void*>(-1)) {` |  | passes on the PosixMmapImpl result |

## P2 sceKernelReleaseFlexibleMemory

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1320 | `if (len == 0 \|\| !Core::Memory::Instance()->ContainsFlexibleMemory(raw_addr, len)) {` | yes | COVERED: FL2 (len 0: ok) |

## P2 MapNamedDirectMemoryFromGuest

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1415 | `if (name != nullptr && name[0] == '\0') {` | yes | COVERED: NM1 (empty name: the caller name, 17 characters) |

## P2 MapNamedFlexibleMemoryFromGuest

| line | condition | cited | status |
|---|---|---|---|
| memory.cpp:1423 | `if (name != nullptr && name[0] == '\0') {` | yes | COVERED: NM2 (empty name: the caller name) |

# Phase 3: ambiguity review

Every probe, checked for a result that could follow from more than one rule: what lies around the test
memory (queries, placement, merging), an earlier check firing first (several unusual arguments in one
call), or a result that does not depend on the rule (only "ok", no effect checked). Status: TO REVIEW;
CLEAR (the setup isolates the rule; says why); AMBIGUOUS (what else could explain it, and the probe that
separates it); DEAD (never runs: says why and which probe measures its question). Risk categories come from the label, the calls and the result: a starting point only.

Total: 600 probes, including those made by helper functions (q_raw, e_alloc, bt_bits, ...).

| probe | label | risk to check | status |
|---|---|---|---|
| A1 | rel  whole block | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| A2 | rel  len 0 | earlier check | CLEAR: one argument changed from a valid baseline |
| A3 | rel  start +1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| A4 | rel  len +1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| A5 | rel  start +1000, len 0 | earlier check | CLEAR: combines start +1000 and len 0 on purpose, to show which check comes first (alignment: EINVAL) |
| A6 | rel  page 1 only | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| A7 | rel  whole block twice | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| A8 | rel  pages 2-3 already free | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| A9 | rel  free page | earlier check | CLEAR: one argument changed from a valid baseline |
| A10 | rel  start = dmem size | effect checked? | CLEAR: release / checked release at addresses past direct memory, where nothing exists: the code is the rule |
| A11 | chk  whole block | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| A12 | chk  len 0 | earlier check | CLEAR: one argument changed from a valid baseline |
| A13 | chk  start +1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| A14 | chk  len +1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| A15 | chk  whole block twice | earlier check | CLEAR: one argument changed from a valid baseline |
| A16 | chk  pages 2-3 already free | earlier check | CLEAR: one argument changed from a valid baseline |
| A17 | chk  free page | earlier check | CLEAR: one argument changed from a valid baseline |
| A18 | chk  start = dmem size | earlier check | CLEAR: one argument changed from a valid baseline |
| AL1 | :1404 start<0 end<0 / :1411 len0 / :1415 al c000 / :1420 al 2000 len 2 | earlier check | CLEAR: each call changes one argument (the label lists alignment 2000 and length 2000, two calls) |
| AL2 | :1434 end<=start / :1438 end<len / :1442 end-len<start | earlier check | CLEAR: each call changes one argument from a valid one |
| AL3 | :1458 align 20000 from R+4000 / :1487 start R+8000 / :1496 R taken, al | surroundings: query, surroundings: placement, earlier check | CLEAR: search bounded by the window the probe passes |
| AV1 | :1298 align 1 / 3 / 6000 / :1350 2000 (window R..R+40000) | surroundings: placement, earlier check | CLEAR: search bounded by the window the probe passes |
| AV2 | :1307 start>end / :1311 outside dmem / :1338 start+6000 / :1344 end+60 | earlier check | CLEAR: each call changes one argument from a valid one |
| AV3 | :1357 align 20000 from R+4000 / :1363 largest area after a middle allo | surroundings: query, surroundings: placement, earlier check | CLEAR: search bounded by the window the probe passes |
| B1 | rel  all 4, mapped all 4 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B2 | rel  pages 1-2, mapped all 4 | earlier check | CLEAR: one argument changed from a valid baseline |
| B3 | rel  page 0, mapped all 4 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B4 | rel  2-5, mapped 4-7 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B5 | rel  2-5, mapped 0-3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B6 | rel  1-6, mapped 2-5 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B7 | chk  all 4, mapped all 4 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B8 | rel  2-5, 1 map, types 0+3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B9 | rel  2-5, 1 map, types 3+3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| B10 | map twice, rel | earlier check | CLEAR: one argument changed from a valid baseline |
| B11 | enable dmem aliasing | surroundings: query | CLEAR: fields of its own mapping or reservation |
| B12 | map twice after enable, rel | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BM1 | map direct, protect r, typeprotect 3, unmap: code n / state | earlier check | CLEAR: designed in this review, one argument per call |
| BM2 | map direct, op 7, unmap: code n / state | earlier check | CLEAR: each call changes one argument from a valid one |
| BM3 | map flexible op / BatchMap2 flags 0, flags 10: code n | earlier check | CLEAR: each call changes one argument from a valid one |
| BP0 | counters before | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP1 | expand 20000 from 20000 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP2 | reserve 2M | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP3 | commit bp+0 t0 rw | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP4 | commit bp+10000 t0 rw | earlier check | CLEAR: each call changes one argument from a valid one |
| BP5 | expand 20000 again | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP6 | reserve 2M again | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP7 | commit bp2+0 t3 r | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP8 | commit bp2+10000 t3 r | earlier check | CLEAR: each call changes one argument from a valid one |
| BP9 | map direct the pool block | earlier check | CLEAR: each call changes one argument from a valid one |
| BP10 | allocate 64K (args not in the log) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP11 | checked release the pool block | earlier check | CLEAR: each call changes one argument from a valid one |
| BP12 | dmem query 0 the pool block | surroundings: query, earlier check | CLEAR: exact query of the own pool block (EACCES / ENOENT whatever is around) |
| BP13 | dmem query 1 the pool block | surroundings: query, earlier check | AMBIGUOUS on its own; answered by T10, P22 (pool memory ends the search: EACCES) |
| BP14 | get type the pool block | surroundings: query, earlier check | CLEAR: exact query of the own pool block (EACCES / ENOENT whatever is around) |
| BP15 | decommit bp+0 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP16 | munmap bp | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP17 | commit bp2+10000 t3 r | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP18 | mtypeprotect bp2 2M t3 rw | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP19 | mprotect bp2 2M rw+gpu rw | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| BP20 | munmap bp2-10000 2M+10000 | surroundings: query | CLEAR: fields of its own mapping or reservation |
| BQ1 | find-next from pool memory, D allocated right after it: P / P last / D | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| BT1 | ReserveVirtualRange flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT2 | ReserveVirtualRange alignment 1<<k | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| BT3 | MapFlexibleMemory flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT4 | MapFlexibleMemory prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT5 | MapDirectMemory flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT6 | MapDirectMemory prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT7 | MapDirectMemory alignment 1<<k | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| BT8 | MapDirectMemory2 type value | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT9 | mmap anon: flags 1002/bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT10 | mmap anon: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT11 | mprotect flexible: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT12 | mprotect direct: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT13 | mprotect pool committed: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT14 | mtypeprotect direct t0: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT15 | mtypeprotect direct rw: type value | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT16 | PoolReserve flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT17 | PoolReserve alignment 1<<k | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| BT18 | PoolCommit flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT19 | PoolCommit t0: prot bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT20 | PoolCommit read: type value | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT21 | PoolDecommit flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT22 | AllocateDirectMemory: type value | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT23 | AllocateDirectMemory: alignment 1<<k | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| BT24 | VirtualQuery flags bit | surroundings: query, earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT25 | DirectMemoryQuery flags bit | surroundings: query, earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT26 | BatchMap2 (one protect entry) flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT27 | MemoryPoolBatch (one protect entry) flags bit | earlier check | CLEAR: one bit at a time on a call that succeeds with the bit clear |
| BT28 | AvailableDirectMemorySize alignment 1<<k | surroundings: query, surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| BT29 | POSIX mmap anon: flags 1002/bit (errno) | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| C1 | rel  last page, len BIG | earlier check | CLEAR: one argument changed from a valid baseline |
| C2 | chk  last page, len BIG | earlier check | CLEAR: one argument changed from a valid baseline |
| C3 | rel  last page, len 1<<32 | effect checked? | CLEAR for the length rule (the code); what a large release frees is measured on own pages by C4 and R1 (the last page here is not the probe's) |
| C4 | rel  own page, len BIG | earlier check | CLEAR: each call changes one argument from a valid one |
| C5 | rel  last page, len 1<<63 | earlier check | CLEAR: one argument changed from a valid baseline |
| C6 | rel  last page, end = 2^64-4000 | earlier check | CLEAR: one argument changed from a valid baseline |
| C7 | rel  last page, end = 2^64 (wraps to 0) | earlier check | CLEAR: one argument changed from a valid baseline |
| C8 | rel  last page, len 7FFFFFFFFFFFC000 | effect checked? | CLEAR for the length rule (the code); what a large release frees is measured on own pages by C4 and R1 (the last page here is not the probe's) |
| C9 | rel  last page, end = 2^63-4000 | effect checked? | CLEAR for the length rule (the code); what a large release frees is measured on own pages by C4 and R1 (the last page here is not the probe's) |
| C10 | rel  last page, end = 2^63 | effect checked? | CLEAR for the length rule (the code); what a large release frees is measured on own pages by C4 and R1 (the last page here is not the probe's) |
| CF1 | ConfiguredFlexibleMemorySize: dlsym / null / size | earlier check | CLEAR: designed in this review, one argument per call |
| CG1 | size 18, 0C writable (unmapped) | earlier check | CLEAR: one argument changed from a valid baseline |
| CG2 | size 14, 12 writable (unmapped) | earlier check | CLEAR: one argument changed from a valid baseline |
| CG3 | size 14, 10 writable (unmapped) | earlier check | CLEAR: one argument changed from a valid baseline |
| CG4 | size 14, 11 writable (unmapped) | earlier check | CLEAR: one argument changed from a valid baseline |
| CG5 | size 18, 10 writable (unmapped) | earlier check | CLEAR: one argument changed from a valid baseline |
| CG6 | size 14, 12 writable (read-only) | earlier check | CLEAR: one argument changed from a valid baseline |
| CL1 | clear name: named map (code, name) / free / len0 / +1000 | surroundings: query, earlier check | CLEAR: designed in this review (rounds 102-112) for one rule |
| CL2 | two no-access pages named x, y: areas / both cleared: areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO1 | :2683 anon 1010 p33 | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO2 | :2860 reserve 111 (void+shared) | surroundings: merging, earlier check | CLEAR: inside its own reservation / allocation |
| CO3 | :2929 reserve 110 (void, no shared) | surroundings: merging, earlier check | CLEAR: inside its own reservation / allocation |
| CO4 | :2986 direct 10 p33 (neighbouring phys) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO5 | :3169 file 10 rw (no shared) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO6 | :3344 file 11 rw (shared) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO7 | :3392 file 10 r (no shared) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO8 | :3441 reserve 400111, middle 111 | surroundings: merging, earlier check | CLEAR: inside its own reservation / allocation |
| CO9 | CO1 with p3: anon 1010 | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO10 | CO4 with p3: direct 10 (neighbouring phys) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO15 | anon 1010 p1 (read-only) | surroundings: merging | CLEAR: inside its own reservation / allocation |
| CO16 | anon 1010 p0 | surroundings: merging, earlier check | CLEAR: inside its own reservation / allocation |
| CV0 | alias before / after EnableDmemAliasing | effect checked? | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV1 | no alias; mtype A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV2 | no alias; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV3 | no alias; munmap A 2-4 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV4 | no alias; rel 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV5 | no alias; chk 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV6 | no alias; mtype A 2-4 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV7 | alias 3-4 kept; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV8 | flexible 4 pages, munmap 1, then 0+2-3 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV9 | no alias; mtype A+C (A 0-7, C 4p) | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV10 | alias 3-4 unmapped; munmap A 0-1 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV11 | alias 3-4 unmapped; mtype A 0-1 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV12 | alias 3-4 unmapped; mprotect A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV20 | alias 3-4 unmapped; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV21 | alias 3-4 mtype 3; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV22 | alias 3-4 mtype 0; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV23 | map2 alias 3-4 kept; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV24 | named alias 3-4 unmapped; munmap A | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV25 | alias 0-1 unmapped; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV26 | alias 6-7 unmapped; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV27 | aliases 5,6,7 unmapped; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV30 | alias 3-4 unmapped; mtype A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV31 | alias 3-4 unmapped; munmap A 2-4 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV32 | alias 3-4 unmapped; rel 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV33 | alias 3-4 unmapped; chk 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV34 | alias 3-4 unmapped; mtype A 2-4 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV35 | alias 3-4 unmapped; mtype A+C | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV40 | pool commit 64K of 128K; decommit | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV41 | pool commit, rel page 1; decommit | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV42 | pool commit, rel all; decommit | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV43 | pool commit, rel page 1; mtype | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV44 | pool commit, rel page 1; munmap | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV45 | pool commit; munmap | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV50 | alias 3-4 kept; mtype A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV51 | alias 3-4 kept; mtype B prot 1 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV52 | alias 3-4 kept; mtype A 0-1 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV53 | alias 3-4 kept, A unmapped; mtype B | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV54 | map2 alias 3-4 type 0; munmap A 0-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV55 | alias 3-4 kept; mprotect B 1 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV56 | alias 3-4 kept; mtype B type 0 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV57 | A 0-3 + B 4-7, no page shared; mtype A | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV58 | A 0-3 + B 4-7, no page shared; mtype B | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV60 | no alias; mtype A 2-4 type 0 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV61 | two allocs 0-3 + 4-7 type 3; munmap A 4-7 | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV62 | two allocs 0-3 type 3 + 4-7 type 0; none | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV70 | pool commit type 0, prot 3 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV71 | pool commit type 1, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV72 | pool commit type 2, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV73 | pool commit type 3, prot 3 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV74 | pool commit type 4, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV75 | pool commit type 5, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV76 | pool commit type 6, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV77 | pool commit type 7, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV78 | pool commit type 8, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV79 | pool commit type 9, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV80 | pool commit type 10, prot 3 | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV81 | pool commit type 3, prot 0 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV82 | pool commit type 3, prot 1 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV83 | pool commit type 3, prot 2 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV84 | pool commit type 3, prot 3 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV85 | pool commit type 3, prot 10 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV86 | pool commit type 3, prot 20 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV87 | pool commit type 3, prot 30 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV88 | pool commit type 3, prot 33 | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV90 | pool munmap empty reservation | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV91 | pool munmap committed 64K | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV92 | pool munmap 2M with 64K committed | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV93 | pool munmap uncommitted part | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV94 | pool munmap 2M after decommit | effect checked? | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV95 | pool rel never committed | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV96 | pool chk never committed | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV97 | pool rel after decommit+munmap | earlier check | CLEAR: own 2 fresh pool blocks and own 2M reservation; VirtualQuery per page inside it; the phys checks are type queries of pool memory (ENOENT / EACCES whatever is around) |
| CV98 | pool find-next query | surroundings: query, earlier check | AMBIGUOUS on its own (unknown area after the block); answered by T10 (an allocation right after the pool block: EACCES), P22; BQ1, Q20, Q21 repeat it |
| CV99 | A + alias in one mtype call | earlier check | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV100 | VQ: two allocs type 3 + 0 | surroundings: query | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV101 | VQ: map2 alias 3-4 type 0 | surroundings: query | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV102 | VQ: mtype A 2-4 type 0 | surroundings: query | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| CV103 | VQ: alias 3-4 kept, same type | surroundings: query | CLEAR: block_new keeps a free page on both sides (phys); a direct mapping only merges with physically contiguous ones, so no VA neighbour can join |
| D1 | chk  last page, len 1<<63 | earlier check | CLEAR: one argument changed from a valid baseline |
| D2 | chk  last page, len 7FFF..C000 | earlier check | CLEAR: one argument changed from a valid baseline |
| D3 | rel  start 1<<63, len 4000 | earlier check | CLEAR: one argument changed from a valid baseline |
| D4 | chk  start 1<<63, len 4000 | earlier check | CLEAR: one argument changed from a valid baseline |
| D5 | rel  start FFFF..C000, len 4000 | earlier check | CLEAR: one argument changed from a valid baseline |
| D6 | chk  start FFFF..C000, len 4000 | earlier check | CLEAR: one argument changed from a valid baseline |
| D7 | rel  0-3 of 6, hole at 2-3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| D8 | chk  0-3 of 6, hole at 2-3 | earlier check | CLEAR: one argument changed from a valid baseline |
| D9 | rel  0-3 of 6, hole at 0-1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| D10 | rel  0-3 of 8, holes at 1,3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| D11 | rel  1-4 of 6, hole at 2 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| D12 | rel  0-3, hole 2-3, 4-5 mapped | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| D13 | chk  own page, len 1<<62 | earlier check | CLEAR: one argument changed from a valid baseline |
| D14 | rel  own page, len 0 | earlier check | CLEAR: one argument changed from a valid baseline |
| DF1 | :1061 /dev/dmem1 t10 r: open / map / vq / :1098 release, vq | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| DF2 | :1102 wb garlic 800001, prot 1 (vq) / 2 / 3 / 10 / 20 / 30 | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| DF3 | :1165 /dev/gc rw: open / map / vq / :1208 private | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| DF4 | :1231 budget used: avail / :1237 /dev/gc map / :1247 system libc.sprx  | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| DQ2 | at dmem end: get type / query 0 / query 1 | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| DT1 | :2149 MapDirectMemory2 unallocated / :2184 vq of a direct map (off rel | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| DT2 | :2194 same phys twice: map2 / mirror / :2208 chk release: both u / :22 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| DT3 | :2227 release middle page: pages / :2252 remap all: map / map2 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| DT4 | :2264 backing for types 0-10: write, unmap, remap, same data | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| DT5 | :2323 map2 t3 on t0 page next to t3 page: areas / :2342 after munmap | surroundings: merging | CLEAR: inside its own reservation / allocation |
| DT6 | :2370 type 10: map rw / r / gpu w / :2413 map2 to t10 with rw | earlier check | CLEAR: each call changes one argument from a valid one |
| DT7 | :2492 areas after filling the gap: R / R+4000 / R+8000 / R+C000 | surroundings: placement, surroundings: merging | CLEAR: inside its own reservation / allocation |
| DT8 | :2569 two unmerged t3 areas, one map2 -1 / :2587 one map2 t3: areas | surroundings: query, surroundings: merging | CLEAR: fields of its own mapping or reservation |
| DT9 | type 10, prot 2 20 30 1, flags 0/800000: map / named / map2 | earlier check | CLEAR: type 10 with write and flag 0x800000 together on purpose, to show the order (flags first) |
| E1 | alloc len 0 | earlier check | CLEAR: one argument changed from a valid baseline |
| E2 | alloc len 1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| E3 | alloc align 3000 | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| E4 | alloc align 1000 | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| E5 | alloc align 0 | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| E6 | alloc type 11 | earlier check | CLEAR: one argument changed from a valid baseline |
| E7 | alloc type -1 | earlier check | CLEAR: one argument changed from a valid baseline |
| E8 | alloc range smaller than len | earlier check | CLEAR: one argument changed from a valid baseline |
| E9 | alloc search start +1000 | surroundings: placement | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| E10 | alloc end before start | earlier check | CLEAR: one argument changed from a valid baseline |
| E12 | map phys +1000 | earlier check | CLEAR: one argument changed from a valid baseline |
| E13 | map len 5000 | earlier check | CLEAR: one argument changed from a valid baseline |
| E14 | map half free (2 alloc + 2 free) | earlier check | CLEAR: one argument changed from a valid baseline |
| E15 | map len 0 | earlier check | CLEAR: one argument changed from a valid baseline |
| E16 | map prot 0 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| E17 | map prot 1 (read only) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| E18 | alloc, null phys out | earlier check | CLEAR: one argument; ended the app on the PS4 (round 116), runs after DONE from round 117 |
| E19 | alloc in the range of an allocated page | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| EB1 | A 8 pages, alias of 4-5: mtype A 2-4 / A 0-3 / alias gone: A 2-4 | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| EX1 | dmem 0x33 then mprotect 0x37 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| EX2 | dmem mapped 0x37 | earlier check | CLEAR with BT6: 0x37 is refused for the execute bit (BT6 bit 2) |
| EX3 | flexible 0x3 then mprotect 0x7 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| EX4 | flexible mapped 0x7 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| EX5 | pool: commit 0x7 / commit 0x3 then mprotect 0x7 | earlier check | CLEAR: each call changes one argument from a valid one |
| EX6 | mmap anon 0x7 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F1 | mtype 2-3 -> 0, rel 1-2 | earlier check | CLEAR: each call changes one argument from a valid one |
| F2 | mprotect 1-2 ro, rel all | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F3 | munmap page 1, rel all | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F4 | two maps 0-1 / 2-3, rel 1-2 | earlier check | CLEAR: each call changes one argument from a valid one |
| F5 | aliased twice, rel 1-2 | earlier check | CLEAR: each call changes one argument from a valid one |
| F6 | rel all, re-alloc same phys | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F7 | rel 1-2, munmap whole range | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F8 | rel 1-2, mprotect released 1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| F9 | mtype released page 1 | earlier check | CLEAR: each call changes one argument from a valid one |
| FL1 | avail: -map64K / +unmap / release 16K of 32K: code, avail, pages | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FL2 | release: direct / hole / len0 / +1000 | earlier check | CLEAR: each call changes one argument from a valid one |
| FL3 | 64K flexible touched: avail -map, -touch / release 16K: avail, value | surroundings: query | AMBIGUOUS values: the budget deltas depend on the flexible usage before the probe (1 MB steps); FL1, FL5, FL6 together pin the rule |
| FL4 | avail flexible: before / after 16M map + touch / after unmap | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FL5 | avail drop: 64K 1M 2M 2M+16K 4M 4M+16K (hex) | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FL6 | avail drop: 16 maps of 64K / one 1M / 17 maps of 64K | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FM1 | :1616 flags 0 rw: map / mem write / :1644 file unchanged | earlier check | CLEAR: each call changes one argument from a valid one |
| FM2 | :1667 off 5000 len C000: page off / vq range / vq off / byte0 = file[5 | surroundings: query | CLEAR: fields of its own mapping or reservation |
| FM3 | file: ro shared rw / ro private rw / wo read / dir / fixed noovw / 800 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| FW1 | 170:21 null+fixed: mmap / flex / direct / pool reserve / reserve range | earlier check | CLEAR: each call changes one argument from a valid one |
| FW2 | 200:118 one dmem map of 4 pages: areas / mprotect p1 r: areas, qp rang | surroundings: query, surroundings: merging | CLEAR: fields of its own mapping or reservation |
| FW3 | 250:24 page mapped, again: direct / named / direct+stack / named+stack | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FW4 | 300:41 anon fixed over FC00000000 / :69 anon hint above / :94 direct f | surroundings: placement | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| FW5 | 350:19 budget used: flex 16K / anon 16K / /data file 16K | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| FW6 | 550:82 five anon maps, five call sites: areas / renamed: areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| FW7 | two direct maps, two call sites: areas / renamed: areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| FW8 | two direct maps, one call site: areas / renamed: areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| FW9 | two no-access anon maps, two call sites: areas / renamed: areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| FX1 | budget drop for 1M: flex / anon / stack / void / 64K /data file | earlier check | CLEAR: each call changes one argument from a valid one |
| FX2 | :1993 flexible 16K rw: vq | surroundings: query | CLEAR: fields of its own mapping or reservation |
| FX3 | :2031 unmap middle: rest kept / :2047 fixed remap has old / :2056 over | surroundings: query | CLEAR: fields of its own mapping or reservation |
| FZ1 | flex: write 5A, unmap, map again at the same address: byte0, last byte | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| G1 | rel  16M, prot 3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| G2 | rel  16M, prot 33 (GPU) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| GT1 | t3/t0 areas / t3/t3 areas / t3 x3, middle released: first, last | surroundings: merging | AMBIGUOUS values until round 113: its first area began at R, right after whatever ended that free area; from round 114 one free page first |
| H1 | start: rel/chk (1<<33,40,62,7F..,63) | effect checked? | CLEAR: release / checked release at addresses past direct memory, where nothing exists: the code is the rule |
| H2 | query flags 1<<31 .. 1<<0 | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| H3 | alloc memory type -2 .. 12 | earlier check | CLEAR: each call changes one argument from a valid one |
| H4 | available align 0,1000..1<<40 | surroundings: query, surroundings: placement, earlier check | CLEAR: alignment results (ok / EINVAL / ENOMEM at 1<<40) do not depend on where free memory lies |
| H5 | available, null outputs | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| H6 | bytes written by failing query | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| H7 | 16 samples 1<<33..1<<40 chk/rel | effect checked? | CLEAR: release / checked release at addresses past direct memory, where nothing exists: the code is the rule |
| H8 | boundary X | earlier check | CLEAR: each call changes one argument from a valid one |
| HB1 | hint = an existing reservation | surroundings: placement | CLEAR: placement relative to its own reservation or a free hint |
| HB2 | hint 7EEA00000, 4M (crosses 7EEC00000) | surroundings: placement | AMBIGUOUS: the skipped range (7EEC00000-7EF000000; 800000000-800A00000) is either a rule or occupied; HB5 (round 114) lists what is mapped there |
| HB3 | hint 800000000 (system region) | surroundings: query, surroundings: placement | AMBIGUOUS: the skipped range (7EEC00000-7EF000000; 800000000-800A00000) is either a rule or occupied; HB5 (round 114) lists what is mapped there |
| HB4 | hint 7EEC00000 exactly, 2M | surroundings: placement | AMBIGUOUS: the skipped range (7EEC00000-7EF000000; 800000000-800A00000) is either a rule or occupied; HB5 (round 114) lists what is mapped there |
| HB5 | areas from 7EEA00000 / from 800000000 | surroundings: placement | CLEAR: added in round 114 to separate HB2-HB4 |
| J1 | query info inside a pool reservation | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| J2 | info straddles writable->unmapped | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| J3 | pool commit type -1, 11, 12 | earlier check | CLEAR: each call changes one argument from a valid one |
| J4 | mtype size 0: type 11 / type 10 p3 | earlier check | AMBIGUOUS: size 0 with type 11 / type 10 prot 3 fails on the type and prot checks first; MT0 (round 110) has size 0 with valid arguments |
| J5 | fixed+no-overwrite: rsv start / whole | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| J6 | reserve that does not fit: delta | earlier check | CLEAR: each call changes one argument from a valid one |
| K1 | rsv hint 0,1000,3000,7F00,9000,10000(M) | surroundings: placement, earlier check | CLEAR: placement relative to its own reservation or a free hint |
| K2 | fill with rsv: at/size ... then d/f | surroundings: placement | CLEAR: the rule is the code once space runs out; counts and addresses are layout |
| K3 | fixed+no-ovr dmem 7F0,800,1000,FC00 M | earlier check | CLEAR: each call changes one argument from a valid one |
| K5 | rsv align 64M, 1G, 4G: addr | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| LA1 | free at 7EEC0,7EF0,7F00,7F80,7FC0,..,10000 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| LA2 | band edges (16K) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| LA3 | default search after filling | surroundings: placement, earlier check | CLEAR: the rule is the code once space runs out; counts and addresses are layout |
| M1 | t10 commit with af=0, ac>0: delta | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| M2 | reserve flags 1,2,4,8,20,40,100,1000,80,10 | earlier check | CLEAR: each call changes one argument from a valid one |
| M3 | reserve align 4000,10000,1M,2M,3M,2G,4G | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| M4 | fixed rsv 10/90: free/part/whole/dmem | earlier check | CLEAR with N4 and XC2: "part" is EINVAL from the unaligned address (XC2); an aligned overlapping one is ENOMEM (N4) |
| M5 | fixed rsv 10/90 over a direct map at a 2M aligned address | surroundings: placement, earlier check | CLEAR: inside its own reservation / allocation |
| MF1 | file mmap, no hint | surroundings: query, surroundings: placement | CLEAR: the probe checks success; the address is layout |
| MF2 | file mmap, default space full | surroundings: placement, earlier check | CLEAR: the rule is the code once space runs out; counts and addresses are layout |
| MF3 | file mmap, hint = a reservation | surroundings: placement | CLEAR: placement relative to its own reservation or a free hint |
| MF4 | file mmap, hint 900000000 | surroundings: placement | CLEAR: placement relative to its own reservation or a free hint |
| MF5 | rw fd, read-only map: ret/content | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF6 | ro fd, read-only map: ret/content | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF7 | rw-fd map sees later write: byte0 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| MF8 | ro-fd map sees later write: byte0 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| MF9 | close fd while mapped: ret/byte 8001 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF10 | map/close/reopen/map: rets, content | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF11 | empty file, ro fd, read-only map | earlier check | CLEAR: each call changes one argument from a valid one |
| MF12 | ro fd, map 64K of a 16K file | earlier check | CLEAR: one argument changed from a valid baseline |
| MF13 | rw fd, map 64K of a 16K file | earlier check | CLEAR: one argument changed from a valid baseline |
| MF14 | ro fd, map 16K of a 16K file | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF15 | ro fd, offset 32K past 16K EOF | earlier check | CLEAR: one argument changed from a valid baseline |
| MF16 | rw fd, empty file, map 64K | earlier check | CLEAR: one argument changed from a valid baseline |
| MF17 | ro fd, 10K file, map 16K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF18 | ro fd, 10K file, map 32K | earlier check | CLEAR: one argument changed from a valid baseline |
| MF19 | ro fd, 20K file, map 32K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF20 | ro fd, 20K file, offset 16K, map 16K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF21 | rw fd, 10K file, map 16K read-write | surroundings: query | CLEAR: fields of its own mapping or reservation |
| MF22 | 10K file, map 16K: bytes 0, 27FF, 2800, 3FFF | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| MF23 | fd position after that mmap (set 123) | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| MF24 | rw tail map: mprotect, size, byte0, 3000 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MF25 | ro 16K map of 10K file, later write: byte0 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| MFT1 | 10K file, 66 at 3000 past EOF: reopened: byte size / grown to 4000: by | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| ML1 | mlock: null len0 wrap unmapped noaccess / munlock: null len0 wrap unma | earlier check | CLEAR: designed in this review, one argument per call |
| MP1 | map direct: phys = dmem end / dmem end - 4000, len 8000 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| MP2 | fixed flex at 7FFFFFFFC000 / 800000000000 / munmap: those / 2000 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| MPR1 | mprotect: pool reserved (code qprot) / at 2000 / 800000000000 / across | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| MS1 | MapDirectMemory2 type 3 on type 0: code / type / vq type | surroundings: query | CLEAR: fields of its own mapping or reservation |
| MS2 | AllocateMainDirectMemory 16K: code, then release | effect checked? | CLEAR: existence and codes of the call; nothing else to check |
| MS3 | IsStack: local var (code, inside) / flexible | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| MS4 | Mlock, Munlock: mapping / hole | effect checked? | CLEAR: existence and codes of the call; nothing else to check |
| MS5 | GetPrtAperture 0..2: code size | effect checked? | CLEAR: existence and codes of the call; nothing else to check |
| MS6 | MapDirectMemory2 prot 4 / 5 / 7 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| N1 | reserve align 2M,4M,..,2G | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| N2 | dmem align 4K,16K,32K,64K,1M,2M,16M,256M,1G | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| N3 | fixed rsv at 200000, 40000000 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| N4 | fixed rsv over 2nd half of a 4M rsv | earlier check | CLEAR: each call changes one argument from a valid one |
| N5 | fixed rsv over a 2M dmem map 10/90 | earlier check | CLEAR: each call changes one argument from a valid one |
| N6 | fixed 16K rsv at 4000 10000 100000 1FC000 200000 3FC000 400000 | earlier check | CLEAR: each call changes one argument from a valid one |
| NM1 | named direct: abc / 31 / 32 / empty / null | earlier check | CLEAR: each call changes one argument from a valid one |
| NM2 | named flexible: abc / 31 / 32 / empty / null | earlier check | CLEAR: each call changes one argument from a valid one |
| NM3 | set name: flex / half (both pages) / 32 / len0 / hole | earlier check | CLEAR: each call changes one argument from a valid one |
| NM4 | set name: direct / pool reserved (it, next block) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| NM5 | named: len0 5000 / phys+1000 / al 6000 1000 1<<31 / prot 4 7 100 /  | earlier check | CLEAR: designed in this review, one argument per call |
| NM6 | flex len0 1000 / named: len0 1000 / prot 8 100 / fl 1 2000 / fixed 0 | earlier check | CLEAR: designed in this review, one argument per call |
| NM7 | set name: null | earlier check | CLEAR: designed in this review, one argument per call |
| NP1 | 100:20 getpagesize / 100:2879 qprot on reserve range / :2947 on void m | surroundings: query | CLEAR: fields of its own mapping or reservation |
| NU1 | munmap: 0 / +1000 / 4001 / +hole / twice / wrap | earlier check | CLEAR: each call changes one argument from a valid one |
| NU2 | mprotect: 0 / +1000 / 4001 / +hole / flex+dmem / wo / 40,100 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| NU3 | mtypeprotect: +1000 / +hole / flexible / pool reserved | earlier check | CLEAR: each call changes one argument from a valid one |
| NU4 | release: chk twice / rel twice / +1000 / 4001 | earlier check | CLEAR: each call changes one argument from a valid one |
| NU5 | pool: commit twice / decommit twice / half / 0 / +4000 / 4000 / outsid | earlier check | CLEAR: each call changes one argument from a valid one |
| NU6 | file mmap: len 0 / off 1000 / closed fd / fixed +1000 | earlier check | CLEAR: each call changes one argument from a valid one |
| NU7 | query mid-area: dmem +1001 / vq +1234 / vq hole | surroundings: query, earlier check | AMBIGUOUS values until round 112 (unguarded allocation, as Q6); from round 112 alloc_guarded |
| NU8 | mprotect 1/bit read back: 8 40 80 100 200 400 | earlier check | CLEAR: each call changes one argument from a valid one |
| NU9 | +1000 len 4000: mprotect pages / mtypeprotect types | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| NU10 | commit +4000 len0 / len0 t7 / len0 exec / decommit len0 / +4000 | earlier check | CLEAR with NU5: length 0 alone is accepted (NU5), so each EINVAL comes from the other argument (+4000, type 7, execute) |
| NU11 | 15G reservation with fewer than 15 blocks: code, stats | earlier check | CLEAR: each call changes one argument from a valid one |
| NU12 | no flushed left: t3 1blk / t10 2blk / 1 flushed left: t10 2blk | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| NU13 | no flushed left, ac=T, one commit | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| NU14 | mtypeprotect on reserved: t0 / t3 / t10 / t0 2blk / t0 half | surroundings: query | CLEAR: fields of its own mapping or reservation |
| NU15 | reserved block: t0 then t3 / t0 then t0 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| NU16 | mprotect 1/bit read back: 800 1000 2000 4000 10000 | earlier check | CLEAR: each call changes one argument from a valid one |
| P1 | pool expand 64K | effect checked? | CLEAR: setup (expand); its effect shows in P2-P9 |
| P2 | query pooled block | surroundings: query, earlier check | CLEAR: exact query of pool memory, EACCES whatever is around it |
| P3 | find-next from pooled block | surroundings: query, earlier check | CLEAR: find-next from pool memory: EACCES (as T10, where an allocation follows directly) |
| P4 | reserve + commit 64K | earlier check | CLEAR: each call changes one argument from a valid one |
| P5 | decommit 64K | effect checked? | DEAD: runs only if P4's 64K reservation succeeds, which the PS4 refuses (EINVAL); decommit 64K is measured by P30 |
| P6 | rel  pooled block | effect checked? | CLEAR with P7: after the release the block is still pool memory (P7: EACCES) |
| P7 | query pooled block after rel | surroundings: query, earlier check | CLEAR: exact query of pool memory, EACCES whatever is around it |
| P8 | chk  pooled block | earlier check | CLEAR: each call changes one argument from a valid one |
| P9 | query after chk | surroundings: query, earlier check | CLEAR: exact query of pool memory, EACCES whatever is around it |
| P20 | stats before | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P21 | expand at base, B at +20000 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P22 | find-next at pool end (hole start) | surroundings: query, earlier check | CLEAR: find-next at the hole right after a pool block: EACCES (the area before is pool memory) |
| P23 | find-next inside hole | surroundings: query | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P24 | map pooled block | earlier check | CLEAR: one argument changed from a valid baseline |
| P25 | expand, window 4000 < len 10000 | earlier check | CLEAR: each call changes one argument from a valid one |
| P26 | reserve 2M, commit 64K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P27 | exact query of pool block | surroundings: query, earlier check | CLEAR: exact query of pool memory, EACCES whatever is around it |
| P28 | chk  pool block, committed | earlier check | CLEAR: each call changes one argument from a valid one |
| P29 | rel  pool block, committed | earlier check | CLEAR: each call changes one argument from a valid one |
| P30 | decommit 64K | earlier check | CLEAR: each call changes one argument from a valid one |
| P31 | rel  pool block, decommitted | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P40 | stats before | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P41 | expand 4 blocks | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P42 | reserve R1, R2 (2M each) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P43 | commit R1+0 64K t3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P44 | commit R1+10000 64K t3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P45 | commit R2+0 64K t0 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P46 | commit R1+20000 128K t3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P47 | decommit R1+10000 64K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P48 | decommit R1+0 64K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P49 | decommit R1+20000 128K | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P50 | munmap R1 (all decommitted) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P51 | reserve R3, commit 64K t3 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P52 | munmap R2, 64K still committed | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P53 | munmap R3, 64K still committed | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P54 | expand, window 14000, no fit inside | earlier check | CLEAR: each call changes one argument from a valid one |
| P70 | expand 16 blocks | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P71 | commit 64K type 0..10: move | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| P72 | decommit each: move | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| P73 | reserve 2,4,6,8M / munmap: delta | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P74 | draw order (needs af>0 and ac>0) | earlier check | CLEAR: each call changes one argument from a valid one |
| P75 | drained pool: reserve / commit | earlier check | CLEAR: each call changes one argument from a valid one |
| P80 | stats now / after 1s | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| P81 | reserve 16,32,64M / munmap | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| P82 | reserve 128,256,512M / munmap | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| P83 | 4M: reserve / unmap 1st 2M / 2nd | earlier check | CLEAR: each call changes one argument from a valid one |
| P84 | 128K t3 with af=1, ac>=1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P85 | 16x64K t3, munmap: series | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P86 | 1M t3 commit, decommit: series | surroundings: query | CLEAR: fields of its own mapping or reservation |
| P90 | commit t3, mtype->0, decommit | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P91 | commit t0, mtype->3, decommit | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P92 | type of an uncommitted pool block | surroundings: query, earlier check | CLEAR: type of one page (pool / free / committed): ENOENT whatever is around it |
| P93 | type of a plain allocation | surroundings: query | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| P94 | type of a free page | surroundings: query, earlier check | CLEAR: type of one page (pool / free / committed): ENOENT whatever is around it |
| PB1 | commit, protect r, typeprotect 3, decommit | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| PB2 | commit, opcode 9, decommit: code n / state | earlier check | CLEAR: each call changes one argument from a valid one |
| PB3 | move committed block: code n / dest prot, value / src prot | surroundings: query | CLEAR: fields of its own mapping or reservation |
| PB4 | batch: null entries | earlier check | CLEAR: designed in this review, one argument per call |
| PB5 | move src unmapped/flex/reserved, dest unmapped/committed/flex: code n | earlier check | CLEAR: designed in this review, one argument per call |
| PB6 | batch: move to dest +4000 / commit entry, null processed | earlier check | CLEAR: designed in this review, one argument per call |
| PC1 | pool commit prot 2: code, qprot | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| PD1 | decommit: len 4000 / flexible / direct | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| PE1 | expand: start<0 / end<=start / len0 / len4000 / al4000 / null out | earlier check | CLEAR: designed in this review, one argument per call |
| PG1 | pool: commit 1 / commit 3 / mprotect 1 / 13 / 30 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| PG3 | pool vq: commit 1 / commit 3, mprotect 1 (no writes) | surroundings: query | CLEAR: fields of its own mapping or reservation |
| PG4 | qprot on a committed block (2nd of 2M): start-end rel reservation, pro | surroundings: query | CLEAR: fields of its own mapping or reservation |
| PR1 | pool reserve len 0 | earlier check | CLEAR: designed in this review, one argument per call |
| PRT1 | set: id-1 id3 below past +800 valid / get: id0 addr size / id-1 id3 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| PT1 | :3663 reserved: mprotect piece 2 rw / pieces 1,3 rw / whole 0 (areas,  | surroundings: merging | CLEAR: inside its own reservation / allocation |
| PT2 | :3899 mprotect r over flex, direct, gap, anon: prot of each / areas | surroundings: merging | CLEAR: inside its own reservation / allocation |
| PT3 | :3861 file mmap r+x: prot, ran / :3873 mprotect r+x: prot, ran | surroundings: query | CLEAR: fields of its own mapping or reservation |
| PT4 | :4011 mprotect r +3000 len 10000: pages 0-5 / :4033 +1000 len F000 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| PX1 | mmap anon: result, name rel / munmap | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| PX2 | munmap len0 / mprotect hole / msync / msync hole / mlock / munlock | earlier check | CLEAR: each call changes one argument from a valid one |
| PX3 | POSIX mmap name - return address of the function calling mmap | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| PX5 | POSIX mprotect rw on type 10 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| Q1 | find-next from hole before A/B | surroundings: query, earlier check | AMBIGUOUS until round 111: the result came from the area before the layout, which the probe did not set (EACCES). From round 112 build() puts free memory there (Q1: preceded by free memory); Q19 (a type 0 allocation) and Q20 (a pool block) cover the other cases |
| Q2 | exact query of the hole | surroundings: query, earlier check | CLEAR: exact query of a hole, EACCES whatever is around it |
| Q3 | find-next from inside B | surroundings: query | CLEAR: a type 0 wall ends the region of B (and guards from round 112) |
| Q4 | type 0 run: query Y | surroundings: query | CLEAR: type 3 walls on both sides of the type 0 run |
| Q5 | type 0 run: query Y, X mapped | surroundings: query | CLEAR: as Q4 |
| Q7 | info size 10 | surroundings: query | AMBIGUOUS values until round 111 (as Q6); the copied size was clear |
| Q8 | info size 0 | surroundings: query | AMBIGUOUS values until round 111 (as Q6); the copied size was clear |
| Q9 | offset = dmem size | surroundings: query, earlier check | CLEAR: offset = end of direct memory |
| Q10 | find-next from last page | surroundings: query, earlier check | CLEAR: last page of direct memory |
| Q11 | flags 2 | surroundings: query, earlier check | CLEAR: flag check |
| Q12 | find-next at hole start (A end) | surroundings: query, earlier check | CLEAR from round 112 (guards); until round 111 an adjacent type 3 allocation could have widened A (it did not on the PS4 run) |
| Q13 | find-next at hole +4000 | surroundings: query | CLEAR from round 112 (as Q12) |
| Q14 | find-next at hole +10000 | surroundings: query | CLEAR from round 112 (as Q12) |
| Q15 | find-next at last hole page | surroundings: query | CLEAR from round 112 (as Q12) |
| Q16 | find-next at A end - 4000 | surroundings: query, earlier check | CLEAR from round 112 (as Q12) |
| Q17 | flags 3 | surroundings: query, earlier check | CLEAR: flag check |
| Q18 | flags -1 | surroundings: query, earlier check | CLEAR: flag check |
| Q19 | find-next at hole start after a type 0 alloc | surroundings: query | CLEAR: added in round 112, guarded layout |
| Q20 | find-next at hole start after a pool block | surroundings: query | CLEAR: added in round 112, guarded layout; repeats T10 / P22 (which already answer it) |
| Q21 | find-next from the pool block's last page | surroundings: query | CLEAR: added in round 112, guarded layout |
| QP1 | qprot: free / 4000 / IsStack: free | surroundings: query | CLEAR: designed in this review (rounds 102-112) for one rule |
| QP2 | qprot null start/end/prot / IsStack null start/end | surroundings: query, earlier check | CLEAR: designed in this review (rounds 102-112) for one rule |
| R1 | rel  own page, len 1<<62 | earlier check | CLEAR: each call changes one argument from a valid one |
| RF1 | ReleaseFlexibleMemory: null / len 1000 | earlier check | CLEAR: designed in this review, one argument per call |
| RG1 | sceKernelMprotect 0x37: code / vq / stub | surroundings: query | CLEAR: fields of its own mapping or reservation |
| RG2 | POSIX mprotect 0x37 (the commented-out path): ret / stub | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| RG3 | the same on flexible memory: code / vq / stub | surroundings: query | CLEAR: fields of its own mapping or reservation |
| RL1 | chk release: start +1000 / len 4001 | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| RL2 | rel / chk release len 1<<63 at dmem end | effect checked? | CLEAR by design (added in this review); result pending the next PS4 run |
| RL3 | chk release len 0: code, still allocated / MT0 mtypeprotect size 0 t0  | earlier check | CLEAR: designed in this review, one argument per call |
| RV1 | reserve 2M: code / query / prot / name | surroundings: query | CLEAR: fields of its own mapping or reservation |
| RV2 | len0/len1000/al6000/al8000/fl1/fixed/fixed+4000/fixed noovw | earlier check | CLEAR: each call changes one argument from a valid one |
| RV3 | direct fixed in middle: code / before / it / after / unmap it / it | earlier check | CLEAR: each call changes one argument from a valid one |
| RV4 | munmap first 16K: code / there / rest / mprotect / flex fixed | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| RV5 | reserve flags: 2 4 8 20 40 100 400 1000 2000 | earlier check | CLEAR: each call changes one argument from a valid one |
| RV6 | reserve flags: 800 4000 8000 10000 20000 40000 80000 100000 200000 400 | earlier check | CLEAR: each call changes one argument from a valid one |
| RV7 | reserve null out / fixed rsv over a direct map: 10 / 90 | earlier check | CLEAR: designed in this review, one argument per call |
| S1 | query start / mid of alloc | surroundings: query | AMBIGUOUS values until round 112: the region ran into a neighbouring allocation (start=-10000); from round 112 alloc_guarded |
| S2 | map 2pg, query prot | surroundings: query | CLEAR: fields of its own mapping or reservation |
| S4 | query B, A mapped | surroundings: query, earlier check | CLEAR: [wall t0][A][B][D][wall t0]: the region A..D is bounded by the walls |
| S5 | query B, A and D mapped | surroundings: query, earlier check | CLEAR: [wall t0][A][B][D][wall t0]: the region A..D is bounded by the walls |
| S6 | query A (mapped) | surroundings: query, earlier check | CLEAR: [wall t0][A][B][D][wall t0]: the region A..D is bounded by the walls |
| S7 | query D (mapped) | surroundings: query | CLEAR: [wall t0][A][B][D][wall t0]: the region A..D is bounded by the walls |
| SF1 | :27 flexible size 2000 / 6000 | earlier check | CLEAR: each call changes one argument from a valid one |
| SF2 | :178 len0 / :183 anon+off / anon+fd / void+off / void | earlier check | CLEAR: each call changes one argument from a valid one |
| SF3 | :206 stack r / stack+fd / stack rw / :226 >FC00000000 / :240 over | earlier check | CLEAR: each call changes one argument from a valid one |
| SF4 | :257 fixed file, addr offset != file offset / :419 directory | earlier check | CLEAR: each call changes one argument from a valid one |
| SF5 | :548 anon flag: code p/f a(2M aligned) w(write ok) | surroundings: placement | CLEAR: inside its own reservation / allocation |
| SF6 | MapDirectMemory2: al c000/100/100000000/1000 addr+2000 phys-2000 len20 | earlier check | CLEAR: each call changes one argument from a valid one |
| SF7 | :1015 MapDirectMemory2 type -1 (type kept) / -2 / 10 / :1019 overlap | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| SF8 | :1023 EnableDmemAliasing / :1027 overlap now | surroundings: query | CLEAR: fields of its own mapping or reservation |
| SH1 | rsv hint 4000 / 100000 (not fixed): = or address | surroundings: placement | CLEAR: designed in this review |
| SY1 | system flex: null / 32 / len0 / 1000 / fixed 0 / valid: code prot flag | surroundings: query, earlier check | CLEAR: designed in this review (rounds 102-112) for one rule |
| T1 | VirtualQuery of committed VA | surroundings: query | CLEAR: fields of its own mapping or reservation |
| T2 | exact query of committed block | surroundings: query | DEAD: runs only if VirtualQuery reveals the committed block's phys (T1: located=0 on the PS4 and on shadPS4); the same questions are measured by U2-U6 |
| T3 | find-next from committed block | surroundings: query | DEAD: runs only if VirtualQuery reveals the committed block's phys (T1: located=0 on the PS4 and on shadPS4); the same questions are measured by U2-U6 |
| T4 | type of committed block | surroundings: query | DEAD: runs only if VirtualQuery reveals the committed block's phys (T1: located=0 on the PS4 and on shadPS4); the same questions are measured by U2-U6 |
| T5 | map committed block | effect checked? | DEAD: runs only if VirtualQuery reveals the committed block's phys (T1: located=0 on the PS4 and on shadPS4); the same questions are measured by U2-U6 |
| T6 | chk / rel committed block | effect checked? | DEAD: runs only if VirtualQuery reveals the committed block's phys (T1: located=0 on the PS4 and on shadPS4); the same questions are measured by U2-U6 |
| T8 | [A 4pg][pool blk]: chk / rel all | earlier check | CLEAR: each call changes one argument from a valid one |
| T9 | [pool][A][B]: query B, rel to B | surroundings: query, earlier check | CLEAR: [pool][A][B]: the region of B runs back to A and stops at the pool block |
| T10 | [pool][A][B]: find-next at pool | surroundings: query, earlier check | CLEAR: [pool][A][B], A right after the pool block: find-next at the pool block is EACCES on the PS4, so pool memory ends the search (this answers the BQ1 question: memory-tests behaviour, not PR #5495) |
| T11 | type of mapped memory | surroundings: query | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| T12 | available: null phys / null size | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| T13 | query, null info: size 0 / 18 | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| T14 | reserve 1T | earlier check | CLEAR: each call changes one argument from a valid one |
| T15 | munmap committed 64K of a reserve | earlier check | CLEAR: each call changes one argument from a valid one |
| T16 | 128K t3: decommit 1st 64K / 2nd | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| T17 | t3 commit, mtype->1,2,10: mt/dc | earlier check | CLEAR: each call changes one argument from a valid one |
| T18 | munmap over two reservations | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| T19 | MAP_FIXED dmem over reserve / munmap | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| T20 | block stats: bytes written per size | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| TF1 | :273 rw file, shared rw: map / mem 2 -> file 2 / file 1 -> mem 1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| TF2 | :308 rw file, private rw: map / file 2 -> mem still 1 / mem 2 -> file  | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| TF3 | :346 write-only file, shared: rw / w / r / none / mprotect rw, mem 1 / | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| TF4 | :435 ro file: rw sh/pr/none, w sh, r sh/pr/none / sh r, mprotect rw (n | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| TF5 | ro file, private rw: map / kernel write into it / file still 1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| TF6 | wo file sh p0, mprotect 33: vprot / ro file: sh 21 / pr 20 / sh 20 | earlier check | CLEAR: each call changes one argument from a valid one |
| TL1 | 550:148 big dmem map, move 16 1M pieces / move 4 back / checked releas | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| TP1 | :4183 mtypeprotect t10 r / mprotect rw / :4196 mtypeprotect t0 rw / ty | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| TP2 | :4213 middle t3: vm areas, dmem areas / :4261 all t1: vm areas, dmem a | surroundings: merging | CLEAR: inside its own reservation / allocation |
| TP3 | :4301 flex t11 / flex t0 / :4316 reserved t0 / :4323 free t0 | earlier check | CLEAR: flexible memory with type 11 shows the order (type checked first); each alone measured (TP3 flex t0, BT15) |
| TP4 | pool mtype: +4000 len 10000 / two reservations / one, aligned | surroundings: placement, earlier check | CLEAR: alignment and range rules show in the codes; the layout could only turn ok into ENOMEM, and the ok results fit |
| U1 | drain, expand X, commit X | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| U2 | exact query of committed X | surroundings: query, earlier check | CLEAR: exact query of pool memory, EACCES whatever is around it |
| U3 | find-next from committed X | surroundings: query, earlier check | CLEAR: find-next from pool memory: EACCES (as T10, where an allocation follows directly) |
| U4 | type of committed X | surroundings: query, earlier check | CLEAR: type of one page (pool / free / committed): ENOENT whatever is around it |
| U5 | map committed X | earlier check | CLEAR: one argument changed from a valid baseline |
| U6 | chk / rel committed X | earlier check | CLEAR: each call changes one argument from a valid one |
| U7 | null info, free page: size 18 / 0 | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| U8 | pool t3 commit, mtype -1..12: move | earlier check | CLEAR: each call changes one argument from a valid one |
| U9 | dmem mtype -1..12: ret/query type | surroundings: query | CLEAR: fields of its own mapping or reservation |
| U10 | largest pool reserve (2M steps) | surroundings: placement, earlier check | CLEAR: the rule is the code once space runs out; counts and addresses are layout |
| U11 | MAP_FIXED over: empty rsv/dmem/free | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| U12 | query info = unmapped VA | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| UL1 | scratch files gone? / unlink missing | earlier check | CLEAR: each call changes one argument from a valid one |
| V1 | whole layout, align 4000 | surroundings: query, surroundings: placement, earlier check | CLEAR: the available-size search is bounded by the window the probe passes |
| V2 | whole layout, align 20000 | surroundings: query, surroundings: placement, earlier check | CLEAR: the available-size search is bounded by the window the probe passes |
| V3 | only the hole 4-7 | surroundings: query | CLEAR: the available-size search is bounded by the window the probe passes |
| V4 | only allocated A | surroundings: query, earlier check | CLEAR: the available-size search is bounded by the window the probe passes |
| V5 | end before start | surroundings: query, earlier check | CLEAR: the available-size search is bounded by the window the probe passes |
| V6 | align 3000 | surroundings: query, surroundings: placement, earlier check | CLEAR: the available-size search is bounded by the window the probe passes |
| VQ1 | commit at start: committed / rest | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ2 | type-3 commit in the middle: gap / it / rest | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ3 | first commit decommitted: start | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ4 | same call site, adjacent: 2nd / 1st | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ5 | middle commit mtypeprotect type 0: it | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ6 | names: fn, 1st, 2nd, name-fn | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| VQ7 | names: flexible / direct / mmap anon / mmap file | surroundings: query | CLEAR: fields of its own mapping or reservation |
| VQ8 | names rel: decommitted / reserved rest / committed | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| VQ9 | fixed dmem at: map / vq0 / vq1 / qprot | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| VQ10 | addr(M): map / vq0 vq1 mapped / vq0 vq1 free | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| VQ11 | fixed dmem, vq0 when mapped: FC00 FD00 FE00 FF00 FF80 FFC0 FFF0 FFFFC  | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| VQ12 | fixed dmem, map+vq0: FD40 FD80 FDC0 FDF0 FDFF FE00 FE80 FEFF FF00 (x1M | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| VQ13 | fixed dmem, map+vq0: 7EF04 7EF40 7EF80 7EFC0 7EFFC 7F000 (x64K) | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| VQ14 | module first segment, TLS, stack, our code, FE0000000: addr:vq | surroundings: query, earlier check | CLEAR: the rule is in the codes (system area hidden); the "next" addresses are layout |
| VQ15 | vq1 from 7EFFFC000: vq0 there / a) next=module / b) own page at 7F0000 | surroundings: query, earlier check | CLEAR: the rule is in the codes (system area hidden); the "next" addresses are layout |
| VQ16 | page at 1000000000; vq1 from 7EFFFC000 / from FD0000000 (free) | surroundings: query | CLEAR: the rule is in the codes (system area hidden); the "next" addresses are layout |
| VQ17 | 4 pages at 7EFFF8000 (straddling 7F0000000): map / vq0 at 7F0000000 /  | surroundings: query, earlier check | CLEAR: the rule is in the codes (system area hidden); the "next" addresses are layout |
| W1 | pool t3 commit, mtype 4..9: delta | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| W2 | two reserves of 3FFE00000 at once | earlier check | CLEAR: each call changes one argument from a valid one |
| W3 | query info = read-only page | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| W4 | fixed dmem: whole rsv / rsv tail | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| W5 | fixed flexible over rsv / free VA | earlier check | CLEAR: each call changes one argument from a valid one |
| W6 | mtype 11 prot 1 / 10 prot 1: prot | surroundings: query, earlier check | CLEAR: fields of its own mapping or reservation |
| X1 | dmem mtype 10, prot 0,1,2,3,10..33 | surroundings: query | CLEAR: fields of its own mapping or reservation |
| X2 | pool commit t10, prot 0,1,2,3: move | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| X3 | pool t3 commit, mtype->10 prot 1 | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| X4 | fixed over rsv tail (last page) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| X5 | fixed over rsv middle | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| X6 | fixed whole rsv holding a commit | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| X7 | fixed rsv + 2M past it | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| XC1 | expand win: fit@c, fit@end, 1 page short | earlier check | CLEAR: each call changes one argument from a valid one |
| XC2 | fixed rsv at +4000, +100000, +200000 | earlier check | CLEAR: each call changes one argument from a valid one |
| XC3 | chk/rel: X+1M, X+32G, X+4000, 6000.., X-4000+1M | effect checked? | CLEAR: release / checked release at addresses past direct memory, where nothing exists: the code is the rule |
| XC4 | straddle into RO (8) / unmapped (4) | surroundings: query, earlier check | CLEAR: codes and buffer handling, independent of the layout |
| XC5 | default space full: flex / pool rsv | surroundings: placement, earlier check | CLEAR: the rule is the code once space runs out; counts and addresses are layout |
| XR1 | flexible rw, mprotect rwx | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| XR2 | direct rw+gpu rw, mprotect +x (0x37, dmem_regprig) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| XR3 | direct rw, mprotect rwx (0x7) | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| Y1 | VA of pool rsv / dmem map / flex | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| Y2 | max rsv / again holding it / w 8G | effect checked? | CLEAR: the result reports the effect (state, counters, contents or areas), not only the code |
| Y3 | pool commit t10, prot 10,11,20,21,30 | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| Y4 | t10 prot 1 commit with ac>0: move | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
| Z1 | t10 commit with af=0, ac>0: move | effect checked? | CLEAR: the result reports the effect (budget, bytes, counters, protections, names or areas) |
