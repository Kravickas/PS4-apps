CarveCheck - PS4 homebrew pass/fail tests for split direct memory ranges (OpenOrbis toolchain)

A mapping caches the physical range it maps. When another mapping of the same memory splits that
range afterwards, operations on the first mapping must still work on the whole range. CarveCheck
runs those operations and checks the results against what a PS4 (FW 12.02) returned for the same
calls in CarveTest. Runs at launch and keeps the results on screen; close it with the PS button.

OUTPUT
  Screen, and /data/carvecheck_results.txt (/temp0/carvecheck_results.txt if /data is not
  writable). Each test prints PASS or FAIL (only the word is coloured, green or red) and its
  name, which says which call it makes and how the memory was split first; under a failed test,
  each failed check is on its own
  line with what the PS4 returned. At the end the screen is cleared and shows only RESULT, sized
  to fill the screen: each group with how many of its tests passed (and how many ended the app)
  and what it verifies, every test of the group under it, and the total. After a relaunch the
  tests already run are shown first under EARLIER RESULTS. The last line on screen names the file
  to delete for a fresh run (carvecheck.state).

TESTS
  split range: calls on a mapping whose memory another mapping split cover the whole range. The
  mapping is 8 pages; the split is a second mapping of pages 3-4 mapped and unmapped again unless
  the name says otherwise. On a build without the fix these end the app, which counts as a fail.
    munmap whole mapping; split by alias of pages 3-4, unmapped
    munmap whole mapping; MapDirectMemory2 alias of pages 3-4 kept (the alias keeps its data)
    munmap whole mapping; named alias of pages 3-4, unmapped
    munmap whole mapping; split at pages 0-1 / split at pages 6-7
    munmap whole mapping; aliases of pages 5, 6, 7 unmapped (how Red Dead Redemption 2 splits it)
    mtypeprotect whole mapping / mtypeprotect pages 2-4, across the split (data kept)
    munmap pages 2-4, across the split (the other pages keep their data)
    ReleaseDirectMemory / CheckedReleaseDirectMemory whole block (memory free, mapping gone)
    mtypeprotect over the mapping and the next one; first split (as Red Dead Redemption 2 does)
    munmap whole mapping; MapDirectMemory2 alias of pages 3-4, type 0
    PoolDecommit, and mtypeprotect, of a pool commit after a page of it was released
  region reporting: GetDirectMemoryType and DirectMemoryQuery report the whole allocation for
  every page: after an alias was unmapped, after munmap of pages 2-4, after mtypeprotect of pages
  2-4 to the same type, and over two adjacent allocations of type 3 (also after munmap of the
  second); two adjacent allocations of types 3 and 0 report one region each.
  releases: ReleaseDirectMemory of pool memory returns 0 and leaves it in the pool (the next
  expand does not get it back); CheckedReleaseDirectMemory of pool memory returns ENOENT;
  release after decommit and munmap returns 0; a release starting on a free page releases
  nothing past its range.

RESUME).
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
