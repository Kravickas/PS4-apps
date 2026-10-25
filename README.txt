DmemTest - PS4 direct memory release test (OpenOrbis toolchain)

Measures what sceKernelReleaseDirectMemory and sceKernelCheckedReleaseDirectMemory return, and what
they do to virtual mappings of the released memory. Runs once at launch and keeps the results on
screen; close it with the PS button.

OUTPUT
  Screen, and /data/dmem_results.txt (/temp0/dmem_results.txt if /data is not writable).
  Every address is relative to the test's own block, so a PS4 log and a shadPS4 log diff line for
  line: run it on both and compare.
    phys  one char per page: A allocated, F free
    va    one char per mapped page: M mapped, data intact; m mapped, data changed;
          n mapped, no CPU read; r mapped, backing released (never read); . unmapped
    ret   the call's return code in hex (0 = OK)

SECTIONS
  S  the probes themselves (query of an allocated / mapped page); nothing else runs if they fail
  A  return codes on unmapped memory: alignment, length 0, double release, partly free, out of range
  B  release of mapped memory: which mappings remain; B4 and B8 release a range that starts below a
     mapped area and ends inside it; B8 maps two adjacent allocations of different types at once
  D  release edges: oversized checked lengths, start with bit 63 set, and ranges containing a free
     hole; pages of the block outside the released range must stay allocated
  Q  query edges: find-next around a hole, a type-0 run, unaligned offset, flags, out-of-range
     offsets, and QS: bytes written per info size
  V  sceKernelAvailableDirectMemorySize over a known layout
  E  allocate / map argument validation
  F  release after mtypeprotect, mprotect, partial munmap, split mappings, aliases, re-allocation
  P  memory pool expand / commit / decommit, and releasing pooled memory
  H  ladders: release start values, every single-bit query flag, memory types -2..12,
     available-size alignments, null outputs, bytes written by a failing query
  H7 checked release past the end of dmem: 16 samples, then H8 bisects on the console for the
     first start that returns OK, with checks either side of it
  P4 pool block counters through expand, two reservations, commits of type 3 and 0, decommits,
     munmap of an empty reservation and of reservations that still hold committed memory
  P7 pool model, as counter moves per operation: commit and decommit of each memory type 0..10,
     reservation cost for 2/4/6/8 MiB, which available pool each kind of allocation draws from,
     and reserve / commit against a fully drained pool
  P8 pool: block return timing after unmap and decommit, reservation cost from 16 to 512 MiB,
     partial unmap of a reservation, and a two-block commit split across both available pools
  P9 memory type changed with mtypeprotect between commit and decommit, and
     sceKernelGetDirectMemoryType on a pool block, a plain allocation and a free page
  T  every path the shadPS4 changes touch that no other section isolates: the committed pool block
     itself (located through VirtualQuery) under query, find-next, type, map, chk and rel; one
     release over an allocation plus a pool block; query next to a pool block; type of mapped
     memory; one null output; a null query info; a reserve that cannot be placed; munmap of only
     the committed part of a reservation; partial decommit; mtypeprotect of committed pool memory
     to types 1, 2 and 10; munmap over two reservations; a fixed direct map over a reservation;
     block stats with sizes other than 16
  U  the committed pool block isolated by draining the pool and committing one known block, then
     queried, typed, mapped and released; a null query info on a failing lookup; mtypeprotect type
     ladders on committed pool memory and on direct memory; the largest pool reservation found by
     bisection; MAP_FIXED direct maps over an empty reservation, a direct mapping and free VA.
     U12 runs last: a query whose info pointer is unmapped
  W  exact counter deltas for mtypeprotect of committed pool memory to types 4..9; whether the
     pool reservation limit is a size limit; query info on a read-only page; fixed direct maps
     over a whole reservation and its tail; a fixed flexible map over a reservation and free VA;
     whether a rejected mtypeprotect leaves the protection unchanged
  X  memory type 10 by protection (mtypeprotect and pool commit), and fixed direct maps over the
     tail, the middle, a whole committed reservation, and a reservation plus space past it
  Y  pool reservation addresses next to a direct and a flexible mapping; whether the reservation
     limit is a shared total; type 10 pool commits with GPU protection and with cached blocks free
  Z  a type 10 pool commit when no flushed block is available
  J  the remaining code paths: query info inside a reservation and straddling into unmapped
     memory, pool commit types -1/11/12, size-0 mtypeprotect, fixed+no-overwrite over a
     reservation, and a reservation that does not fit giving its block back
  M  pool reserve flags and alignment ladders, pool reserve with MAP_FIXED over free space, part of
     a reservation, a whole reservation and a direct mapping, direct map alignments of 2G/4G/3M, and
     the type 10 fallback deltas
  N  alignment ladders for pool reservations (2M..2G) and direct maps (4K..1G), fixed reservations at
     low aligned addresses, over the second half of a reservation, and over a 2 MiB direct mapping
  LA the occupied band above the thread stacks, found with fixed no-overwrite probes and bisected
     to 16 KiB, and where the default search goes once everything below it is full
  HB non-fixed reservations whose hint is occupied, crosses the default search end, or lies in
     the system region
  XC cross-checks from a second angle for rules that rested on one probe: pool expand windows,
     fixed reservation alignment, checked release past 0x5000000000, partial copy into read-only
     and unmapped memory, and the default search end for flexible maps and pool reservations
  MF file mappings: placement without a hint, with the default search space full, with an
     occupied hint, and with a hint above the default search end
  CG how many bytes a query writes before a fault, for sizes that are and are not multiples of 8
     and for unmapped and read-only memory after the writable part
  MFC (near the end) file mapping contents through a read-write and a read-only fd, and whether
     each mapping shows a write made to the file afterwards; closing the fd while mapped;
     map/close/reopen/map; mapping an empty file through a read-only fd
  MFE mapping past the end of a file: beyond EOF through read-only and read-write fds, exactly the
     file, an offset past EOF, files that do not end on a page boundary, the bytes after the end of the file in its last page,
     the fd position across a mapping; MF25 whether a read-only mapping into a partly filled last page shows a later write; MF16 an
     empty file through a read-write fd, MF21 a writable mapping into a partly filled last page, and
     MF24 writes through such a mapping (inside the file and past its end), mprotect, the file size
     afterwards and what a new mapping shows, run last. UL1 checks the scratch files are gone and what unlinking a missing file returns. The scratch files are deleted after the last test
  MG end-of-file grid: 14 file sizes around the 4 KiB and 16 KiB boundaries (1 byte to 64 KiB),
     each mapped read-only at offsets 0/16K/32K with lengths 16K/32K/48K/64K, contents checked
  MW (at the end) the MG grid mapped read-write through a read-write fd
  PS the four pool counters before each section from P2 on, and again after one idle second
  VQ what VirtualQuery reports for pool memory: offsets inside a reservation, memory type of
     committed parts, adjacent reservations from one call site, and names next to the call site
     (name minus the creating function), also for flexible, direct, anonymous and file mappings,
     and for a reservation's parts after a commit and a decommit (VQ8)
  NU (at the end) edge cases of every call: length 0, unaligned address or length, ranges over
     a hole or over two kinds of mapping, repeated calls, a closed fd; codes and the state after
  TR in sections U, W, Z, J, L and all of NU, every pool call that changes the block counters, with
     its result and the change of each counter
  NU8-NU10 each undefined mprotect bit read back, rounding of mprotect and mtypeprotect with an
     unaligned start, pool commit and decommit edge orderings; NU11 a reservation needing more blocks
     than are available; NU12 commits once the flushed blocks run out; NU13 the cached count at which
     such a commit converts (type 3 and type 10); NU14 mtypeprotect on reserved pool memory; NU15 a
     second type change on the same reserved block; NU lines list codes without a letter
  RV ReserveVirtualRange: query, validation, mapping into it, partial unmap, mprotect, name
  NM named direct/flexible mappings (name lengths, empty, null) and SetVirtualRangeName
  FL flexible memory accounting and ReleaseFlexibleMemory
  BM BatchMap/BatchMap2: ops, entries processed; BM2 (an invalid op mid-batch) runs after EX
  PB MemoryPoolBatch: ops, an invalid opcode; PB3 (move) runs after EX
  PX POSIX mmap/munmap/mprotect/msync/mlock/munlock: result or -1/errno, POSIX mmap name
  MS MapDirectMemory2, AllocateMainDirectMemory, IsStack, Mlock/Munlock, GetPrtAperture
  PG/PG3/RV5/FL3/PX3 pool protection read back (PG3: VirtualQuery's, without writing); VQ9 VirtualQuery
     and QueryMemoryProtection at fixed direct mappings across the address space; FL4 absolute
     available flexible size around a 16 MiB map; VQ10 VirtualQuery codes across 0x7F0000000-
     0x10000000000, mapped and free; FL5 the available-size drop for flexible maps of several sizes; RV6 reserve flags 0x800 to 0x400000 one
     by one; NU16 the mprotect bits 0x800 to 0x10000 read back; reserve flags one by one, flexible accounting with
     touched pages and the value of a released page, POSIX mmap name against its caller's frame
  RG the dmem_regprig homebrew step by step (WB_ONION direct memory, map RW+GPU RW, sceKernelMprotect
     and POSIX mprotect to +execute, every VirtualQuery field, its call stub only if execute reads
     back), and the same on flexible memory
  PT3, XR (last of all) does code really run: flexible and direct (0x37 as dmem_regprig, and 0x7),
     the cases known to run. Measured: direct runs code while hiding execute; pool memory and a
     file mapping given execute do not (the process is ended), so they are not run again
  VQ11/VQ12/VQ13 where VirtualQuery refuses (EACCES) even mapped addresses; VQ14 what sits there:
     each module's first segment, the main thread's TLS, a stack variable, our code, 0xFE0000000;
     VQ15 VirtualQuery for the next mapping from just below the range when that mapping is inside it
  EX (very last) execute permission on direct, flexible, pool and anonymous memory: the result,
     the protection read back, and, only when both allow it, the result of running a small stub
  BP (first) the devs' blockpool homebrew call by call on an empty pool, each result and the pool
     counters compared with their hardware log (blog.txt): "= blog" or "DIFFERS from blog"
  BT (before the cleanup) every bit of every flags/protection parameter, every value of every type
     parameter and every power-of-two alignment, one call each, one result code per bit or value
  SF findings of the OpenOrbis-Unit-Tests memory_test homebrew (MapMemoryTest), retested: flexible
     sizes, mmap edge cases (length 0, anon/void with offset or fd, stack rules, fixed offsets, a
     directory), every named anon flag, MapDirectMemory2 checks; SF8 (after EX) dmem aliasing
  AV/AL memory_test AvailableDirectMemorySize / AllocateDirectMemory findings inside a free 4 MiB area
  DF (after EX) memory_test device file findings: /dev/dmem1, /dev/gc, flexible budget (no writes)
  FM/FX memory_test file mapping (private/shared writes, unaligned offsets) and flexible findings
     (budget for 1 MiB maps of each kind, VirtualQuery, contents after unmap/remap/overwrite)
  DT memory_test DirectTest findings: double mappings, contents after release, partial release,
     backing per type, type changes and which direct memory areas merge
  CO memory_test coalescing findings: for each kind of mapping, the areas after outside-in mapping,
     a middle mapping, a rename, an mprotect and an mlock; CO9/CO10 = CO1/CO4 without GPU protection;
     CO11/CO12 reservations mapped left to right, right to left and in the middle of a range
  PT/TP memory_test protect / type protect findings: reserved splits, one mprotect over several
     kinds, execute on a file mapping, mprotect alignment, mtypeprotect with type 10, splits and
     merges by type, and where mtypeprotect fails (never GPU protection on reserved memory)
  FW/TL/TF memory_test per-SDK rules at our SDK version (1.70 null+fixed, 2.00 re-merging, 2.50
     overlap and MAP_STACK, 3.00 0xFC00000000 limits, 3.50 ENOMEM, 5.50 call sites), the TLOU2
     pattern at 64 MiB, and every file open mode with every mapping kind (no write through a shared
     mapping of a read-only file); TF5 a read-only file mapped private and read-write right away
  K  address space layout: placement with hints, filling the space below the ceiling and then
     mapping direct and flexible memory, fixed maps above it, every region above 0x700000000 and the whole map from 0,
     placement with large alignments
  P54 pool expand with a window long enough for the length but with no aligned fit inside it
  P2 pool with a 2 MiB reservation: block stats around expand, commit, checked and unchecked
     release of committed and decommitted pool memory, find-next and map around a pool block,
     expand with a search window smaller than the length
  C  oversized lengths from the last page, and C4 on a real allocation (rejected length)
  G  releases a 16 MiB, 2 MiB-aligned mapped block, CPU-only then GPU-visible
  R  R1 releases a page above the framebuffer with len 1<<62, which the length check accepts, and
     reports the pages below and above it. Runs last: it frees everything above that page.

BUILD
  Needs OO_PS4_TOOLCHAIN and sce_sys/about/right.sprx (the CI stages it from the toolchain).
    make all    -> eboot.bin and IV0000-SHAD00090_00-SHADDMEMTEST0000.pkg


Cross-checks (SB lines too), for every call in every section
  access(read)/access(write)  VirtualQuery says the CPU may read/write a page, but the kernel cannot
                              (checked with a one-byte sceKernelWrite from / sceKernelRead into the
                              page through a scratch file: EFAULT instead of a fault; the byte written
                              back is the byte read, and file/device mappings are never written)
  failed but ... changed      a call that failed changed protection, VirtualQuery protection or name
  type: vq vs dmem            VirtualQuery's memory type differs from GetDirectMemoryType's

Result lines that do not fit end in [TRUNCATED] (4000 characters per line).

Silent behaviour (SB lines)
  Every successful mprotect, direct/flexible/named mapping, mmap, MapDirectMemory2, pool commit,
  mtypeprotect and SetVirtualRangeName reads back what it set (protection of the first and last
  page from QueryMemoryProtection, and VirtualQuery's protection as vprot; memory type, name). When that differs from what was asked for, an SB line names the section,
  the call, and the requested and actual value. Comparing the SB lines of both logs shows every
  adjustment one machine makes and the other does not.

analysis/
  coverage.py         which libkernel functions the test calls and from where; with shadPS4's
                      src/core/libraries/kernel/memory.cpp also the registered ones never called
  COVERAGE.md         its output for this version of the test
  FINDINGS.md         every finding of other homebrews (memory_test, blockpool, dmem_regprig) with
                      the DmemTest probe that retests it, or todo
  findings.py         generates FINDINGS.md from the memory_test sources and its coverage table
  compare_results.py  compares the PS4 and the shadPS4 results file: where each run ended, matching
                      and differing lines per section, SB/PS lines on one side only, TR differences
