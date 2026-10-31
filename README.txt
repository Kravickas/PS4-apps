CarveTest - PS4 direct memory physical-area test (OpenOrbis toolchain)

Maps a block of direct memory, splits its physical range with a second mapping of part of the
same memory (an alias), then runs mtypeprotect, munmap, release and checked release on the first
mapping, and decommit / mtypeprotect / munmap on committed pool memory whose physical range was
partly released. Runs at launch and keeps the results on screen; close it with the PS button.

OUTPUT
  Screen, and /data/carve_results.txt (/temp0/carve_results.txt if /data is not writable).
  Every address is a page offset from the probe's own block, so a PS4 log and a shadPS4 log diff
  line for line. Each probe allocates 10 pages and releases the first and the last, so the 8 test
  pages have free memory on both sides.
  Each probe writes two lines with the same id: "pre" after the alias steps, before the operation
  (written to the file first, so it is kept if the operation ends the app), then the operation's
  result "op=".
    ret    each call's return code in hex (0 = OK)
    phys   one char per page: A allocated, F free (DirectMemoryQuery)
    gt     GetDirectMemoryType for every page, one token per run of pages it reports alike:
           start-end:type as page offsets from the block, or page!ret for a page that fails
    dq     the same for DirectMemoryQuery; an end of z means it returned OK without an end
    A, B, C, V, F   one char per mapped page: M mapped, data intact; m mapped, data changed;
           n mapped, no CPU read; r mapped, backing released (never read); . unmapped
    Ao ..  VirtualQuery's offset for the page: = the expected physical page, # another, - none
    At ..  VirtualQuery's memory type per page (- unmapped)
    Ap ..  QueryMemoryProtection per page, two hex digits (-- unmapped)
  A is the block's mapping, B (B2, B3) the aliases, C the second mapping of the span probes,
  V the committed pool block, F the flexible mapping.

PROBES
  C0   the same memory mapped twice, before and after sceKernelEnableDmemAliasing; aliasing stays
       enabled for every later probe
  C1-C9   no split: mtypeprotect, munmap, release and checked release of all of A or of pages 2-4,
       an alias that stays mapped (C7), flexible memory unmapped in pieces (C8), and one
       mtypeprotect over A and the mapping next to it (C9)
  C10-C12 an alias of pages 3-4 mapped and unmapped, then an operation that stays inside one part
       of the split: munmap and mtypeprotect of pages 0-1, mprotect of all of A
  C20-C27 one way each to split the block, then munmap of all of A: alias of pages 3-4 unmapped
       (C20), alias given type 3 (C21) or type 0 (C22) with mtypeprotect, alias mapped with
       MapDirectMemory2 (C23), named alias unmapped (C24), alias of pages 0-1 (C25) and 6-7 (C26),
       three one-page aliases of pages 5, 6 and 7 (C27)
  C30-C35 the C20 split, then each other operation: mtypeprotect of all of A (C30), munmap of
       pages 2-4 (C31), release (C32) and checked release (C33) of the block, mtypeprotect of pages
       2-4 (C34), one mtypeprotect over A and the mapping next to it (C35)
  C40-C45 pool: 128 KiB expanded, 64 KiB committed at the start of a 2 MiB reservation; decommit
       (C40), the second page released then decommit (C41), all of it released then decommit
       (C42), the second page released then mtypeprotect (C43) or munmap (C44), munmap with
       nothing released (C45). vq= is where VirtualQuery puts the committed block, in pages from
       the expanded memory. The block's pages are released there only if that lies inside the
       expanded 128 KiB (rel1(vq)); otherwise at the expanded memory (rel1(expand)). "expand"
       shows all 8 expanded pages, "block" the committed block when VirtualQuery puts it
       elsewhere. Pool memory is not read after a decommit.
  C50-C58 the same memory mapped twice and kept mapped: mtypeprotect of A (C50), of B with
       protection 1 (C51), of A pages 0-1, which B does not map (C52), of B after all of A is
       unmapped (C53), of B to type 0 (C56); a MapDirectMemory2 alias of type 0 then munmap of A
       (C54); mprotect of B (C55); A pages 0-3 and B pages 4-7, no page mapped twice, then
       mtypeprotect of A (C57) or of B (C58)
  C60-C62 regions: mtypeprotect of A pages 2-4 to type 0 (C60); pages 4-7 released and allocated
       again on their own, of type 3 (C61, then munmap of A pages 4-7) or of type 0 (C62)

  C70-C80 pool commit of each memory type 0-10 (protection 3), C81-C88 of type 3 with protection
       0, 1, 2, 3, 10, 20, 30 and 33: per page of the 2 MiB reservation's first 8 pages,
       VirtualQuery flags (Vf), memory type (Vt) and protection (Vv), and QueryMemoryProtection
       (Vp), after the commit (pre) and after the decommit (op). Pool memory is never read in
       C70-C99.
  C90-C94 munmap of pool memory: an empty reservation, only the committed 64 KiB, the whole 2 MiB
       holding a commit, only the uncommitted part, the whole 2 MiB after a decommit
  C95-C97 release of pool memory never committed (rel, chk) and after commit, decommit and munmap;
       at= is where the next 128 KiB expand lands, in pages from the released memory
  C98  DirectMemoryQuery find-next from pool memory, before and after a commit
  C99  one mtypeprotect (type 0, protection 1) over A and an alias of A pages 3-4 mapped right
       after A in the same reserved range
  C100-C103 one line each: VirtualQuery's region for every page of A (Avq, start-end:type in pages
       from A) when pages 4-7 are a second allocation of type 0 (C100), after a MapDirectMemory2
       alias of pages 3-4 with type 0 (C101), after mtypeprotect of A pages 2-4 to type 0 (C102),
       and with an alias of pages 3-4 kept (C103)

RESUME
  carve.state, next to the results file, holds the build, the next probe and the probe running
  when the app ended. If the app ends during a probe (a crash, or closed by hand), the next launch
  appends to the results file and runs that probe once more; if it ends the app again it is
  skipped from then on ("SKIP ended two launches"). A probe that finishes clears its strike. A new
  build starts over and empties the results file. After the last probe the run is not repeated
  until carve.state is deleted.

BUILD
  Needs OO_PS4_TOOLCHAIN and sce_sys/about/right.sprx (the CI stages it from the toolchain).
    make all    -> eboot.bin and IV0000-SHAD00091_00-SHADCARVETEST000.pkg
