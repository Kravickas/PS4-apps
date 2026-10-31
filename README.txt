CarveCheck - PS4 homebrew pass/fail tests for split direct memory ranges (OpenOrbis toolchain)

A mapping caches the physical range it maps. When another mapping of the same memory splits that
range afterwards, operations on the first mapping must still work on the whole range. CarveCheck
runs those operations and checks the results against what a PS4 (FW 12.02) returned for the same
calls in CarveTest. Runs at launch and keeps the results on screen; close it with the PS button.

OUTPUT
  Screen, and /data/carvecheck_results.txt (/temp0/carvecheck_results.txt if /data is not
  writable). One line per test, PASS in green or FAIL in red. The test name says which call it
  makes and how the memory was split first; a failing test lists every check it failed, with what
  the PS4 returned. RESULT ends the run: each group with what it verifies and its count, every
  test of the group under it, and below each failed test its failed checks, one per line, also for
  tests that failed in an earlier launch. After a relaunch the test results of the earlier
  launches are shown first under EARLIER RESULTS. The last line on screen names the file to
  delete for a fresh run (carvecheck.state).

TESTS
  split range (A01-A15): the range of mapping A is split by another mapping of part of the same
    memory, then:
    A01 munmap of A after an alias of pages 3-4 was mapped and unmapped
    A02 munmap of A with a MapDirectMemory2 alias of pages 3-4 still mapped; the alias keeps its
        data
    A03 the same with a MapNamedDirectMemory alias, unmapped
    A04, A05 the split at pages 0-1 and at pages 6-7
    A06 three one-page aliases of pages 5, 6 and 7, unmapped (how Red Dead Redemption 2 splits it)
    A07 mtypeprotect of all of A; A11 of pages 2-4, across the split
    A08 munmap of pages 2-4, across the split; the other pages keep their data
    A09, A10 release and checked release of the whole block; the memory is free, A is unmapped
    A12 one mtypeprotect over A and the mapping C right after it (as Red Dead Redemption 2 does)
    A13 a MapDirectMemory2 alias of pages 3-4 with type 0, then munmap of A
    A14, A15 committed pool memory, part of it released, then decommit or mtypeprotect
    On a build without the fix these end the app, which counts as a failure (see RESUME).
  region reporting (R01-R05): GetDirectMemoryType and DirectMemoryQuery for every page report the
    whole allocation, however the mapping state splits it: after an alias was unmapped (R01), a
    partial munmap (R02), a partial mtypeprotect to the same type (R03), and over two adjacent
    allocations of one type (R04). R05: two allocations of different types report two regions.
  releases (L01-L04): ReleaseDirectMemory of pool memory returns 0 and leaves it in the pool (the
    next expand does not get it back) (L01); CheckedReleaseDirectMemory of pool memory returns
    ENOENT (L02); release after decommit and munmap returns 0 (L03); an unchecked release that
    starts on a free page releases nothing past its range (L04).

RESUME
  carvecheck.state, next to the results file, holds the build, the next test, the test running
  when the app ended and every result so far. If the app ends during a test, the next launch
  appends to the results and runs that test once more; if it ends the app again it fails as
  "ended the app on two launches" and the run goes on. After the last test the run is not
  repeated until carvecheck.state is deleted; a new build starts over.

BUILD
  Needs OO_PS4_TOOLCHAIN and sce_sys/about/right.sprx (the CI stages it from the toolchain).
    make all    -> eboot.bin and IV0000-SHAD00092_00-SHADCARVECHECK00.pkg
