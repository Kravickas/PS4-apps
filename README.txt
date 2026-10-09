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
