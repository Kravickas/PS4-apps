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
  C  oversized lengths; C4 releases a real allocation with a 2^64-wrapping length and reports the
     allocations below and above it. C4 runs last: on hardware it may free more than its own page.

BUILD
  Needs OO_PS4_TOOLCHAIN and sce_sys/about/right.sprx (the CI stages it from the toolchain).
    make all    -> eboot.bin and IV0000-SHAD00090_00-SHADDMEMTEST0000.pkg
