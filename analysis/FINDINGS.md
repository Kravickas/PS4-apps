# Findings of other homebrews, and the DmemTest probes that retest them

Every finding comment of the OpenOrbis-Unit-Tests memory_test homebrew (tests/code/memory_test/code),
by file and line, with the DmemTest probe that checks it on both machines, or "todo".
The devs' blockpool homebrew is retested by BP, dmem_regprig by RG.
203 of 848 findings covered so far.


## test_100.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| MapMemoryTest | 20 | These tests assume a 16KB page size, so check getpagesize to validate this. | todo (page size) |
| MapMemoryTest | 24 | Most memory functions in libkernel rely on sceKernelMmap, | SF1 |
| MapMemoryTest | 25 | leading to overlap in some edge cases. | SF1 |
| MapMemoryTest | 26 | Start with testing libkernel's error returns for sceKernelMapFlexibleMemory | SF1 |
| MapMemoryTest | 27 | If size is less than page size, or size is not page aligned, return EINVAL | SF1 |
| MapMemoryTest | 34 | Protection should be a bitwise-or'ed combination of flags. It can be zero, | BT4 |
| MapMemoryTest | 35 | or include Read (1) \| Write (2) \| Execute (4) \| GpuRead (0x10) \| GpuWrite (0x20). | BT4 |
| MapMemoryTest | 36 | If a value is inputted for protection that doesn't match those flags, it returns EINVAL. | BT4 |
| MapMemoryTest | 42 | The only permitted flags are: Fixed (0x10) \| NoOverwrite (0x80) \| NoCoalesce (0x400000) | BT3 |
| MapMemoryTest | 43 | If flags is non-zero, and includes bits not contained in that list, then it returns EINVAL. | BT3 |
| MapMemoryTest | 52 | Other sceKernelMapFlexibleMemory Notes: | todo |
| MapMemoryTest | 53 | If address is not specified, it defaults to 0x200000000 | todo |
| MapMemoryTest | 54 | Not possible to test here, but there's a condition it follows that instead defaults to 0x880000000 | todo |
| MapMemoryTest | 55 | On SDK version < 1.70, flag Fixed with unspecified address would remove the Fixed flag. | todo |
| MapMemoryTest | 58 | Now for edge cases in sceKernelMapDirectMemory. This function allows flags: | BT5 |
| MapMemoryTest | 59 | Fixed (0x10) \| NoOverwrite (0x80) \| DmemCompat (0x400) \| Sanitizer (0x200000) \| NoCoalesce (0x400000) | BT5 |
| MapMemoryTest | 69 | Flag DmemCompat is exclusive to libkernel code, mmap flag 0x400 is Stack. | todo |
| MapMemoryTest | 70 | For games compiled with sdk version < 2.50, DmemCompat does nothing. | todo |
| MapMemoryTest | 71 | For newer titles, it will redirect sceKernelMapDirectMemory to sceKernelMapDirectMemory2. | todo |
| MapMemoryTest | 72 | We can check for edge cases specific to the latter case when testing sys_mmap_dmem later. | todo |
| MapMemoryTest | 75 | Protection should be a bitwise-or'ed combination of flags. It can be zero, | BT6, EX2 |
| MapMemoryTest | 76 | or include Read (1) \| Write (2) \| Execute (4) \| GpuRead (0x10) \| GpuWrite (0x20). | BT6, EX2 |
| MapMemoryTest | 77 | If a value is inputted for protection that doesn't match those flags, it returns EINVAL. | BT6, EX2 |
| MapMemoryTest | 78 | Note that the execute flag causes mmap to fail, that will come up during later edge case tests. | BT6, EX2 |
| MapMemoryTest | 86 | The execute protection causes sceKernelMapDirectMemory calls to fail. | todo |
| MapMemoryTest | 87 | This will likely come up in sys_mmap tests. | todo |
| MapMemoryTest | 90 | Allocate some direct memory to test with | setup |
| MapMemoryTest | 91 | Technically this isn't necessary at all, since the error returns should occur before any file mapping logic (and thus, before the dmem checks), | setup |
| MapMemoryTest | 92 | but this should make the tests less confusing to work with. | setup |
| MapMemoryTest | 99 | Input address, length, phys_addr, and alignment all must be page-aligned. | BT7 (alignment); address/length/phys alignment: todo |
| MapMemoryTest | 111 | Additionally, alignment must be a power of two. | BT7 |
| MapMemoryTest | 115 | Finally, alignment must be less than 0x100000000 | BT7 |
| MapMemoryTest | 119 | Release allocated direct memory | setup |
| MapMemoryTest | 124 | Other sceKernelMapDirectMemory Notes: | setup |
| MapMemoryTest | 125 | If address is not specified, it defaults to 0x200000000 | setup |
| MapMemoryTest | 126 | Not possible to test here, but there's a condition it follows that instead defaults to 0x880000000 | setup |
| MapMemoryTest | 127 | On SDK version < 1.70, flag Fixed with unspecified address would remove the Fixed flag. | setup |
| MapMemoryTest | 130 | Now for sceKernelReserveVirtualRange edge cases. | BT1, BT2 |
| MapMemoryTest | 131 | Alignment must be power of two | BT1, BT2 |
| MapMemoryTest | 136 | Flags can only be Fixed, NoOverwrite, and NoCoalesce | BT1, BT2 |
| MapMemoryTest | 142 | Address input must be aligned. | RV2 (length); address alignment: todo |
| MapMemoryTest | 147 | length input must be aligned. | RV2 (length); address alignment: todo |
| MapMemoryTest | 152 | Alignment input must be aligned. | BT2 |
| MapMemoryTest | 156 | Alignment input must be less than 0x100000000 | BT2 |
| MapMemoryTest | 161 | Other notes for sceKernelReserveVirtualRange: | BT2 |
| MapMemoryTest | 162 | If Fixed flag is specified and address is null, sdk ver 1.70 and below removes flag fixed. | BT2 |
| MapMemoryTest | 163 | Games compiled for newer firmwares will instead return EINVAL. | BT2 |
| MapMemoryTest | 166 | Now for sys_mmap edge cases. | BT9 (bit 21, sanitizer) |
| MapMemoryTest | 167 | To test each of these, I'll use sceKernelMmap (which is just a wrapper for the syscall that converts errors to ORBIS errors) | BT9 (bit 21, sanitizer) |
| MapMemoryTest | 168 | If sys_mmap is called with the Sanitizer flag, then non-devkit consoles return EINVAL. | BT9 (bit 21, sanitizer) |
| MapMemoryTest | 172 | For this test call, use flags Sanitizer (0x200000) \| System (0x2000) \| Anon (0x1000) \| NoOverwrite (0x80) \| Fixed (0x10) | BT9 (bit 21, sanitizer) |
| MapMemoryTest | 173 | This combination of flags is what sceKernelMapSanitizerShadowMemory uses. | BT9 (bit 21, sanitizer) |
| MapMemoryTest | 178 | If length == 0, return EINVAL | SF2 |
| MapMemoryTest | 183 | If flags include Void (0x100) or Anon (0x1000), and phys_addr is non-zero or fd is not -1, return EINVAL. | SF2 |
| MapMemoryTest | 184 | Note: Void flag means this is reserving memory, sceKernelReserveVirtualRange uses it internally. | SF2 |
| MapMemoryTest | 194 | Successful call with anon flag for comparison. | SF2 |
| MapMemoryTest | 200 | Successful call with void flag for comparison. | SF2 |
| MapMemoryTest | 206 | If flags include Stack (0x400) and prot does not include CpuReadWrite or fd is not -1, return EINVAL. | SF3 |
| MapMemoryTest | 213 | Successful stack mappings. | SF3 |
| MapMemoryTest | 226 | Compiled SDK version 3.00 or above prohibit stack mappings above 0xfc00000000. | SF3 |
| MapMemoryTest | 227 | Since this test homebrew has SDK version 1.00, this should succeed. | SF3 |
| MapMemoryTest | 240 | Stack flag does not allow memory overwriting. | SF3 |
| MapMemoryTest | 251 | Stack and Void flags append Anon flag internally. | SF3 |
| MapMemoryTest | 252 | Stack, Anon, and Void flags skip fget_mmap call (which gets the file object for the fd input) | SF3 |
| MapMemoryTest | 253 | If Fixed is not specified and address is 0, then it's set to 0x200000000 | SF3 |
| MapMemoryTest | 254 | Length parameter is aligned up to the nearest page. | SF3 |
| MapMemoryTest | 257 | If flag Fixed (0x10) is specified, address must have the same offset as phys_addr. | SF4 |
| MapMemoryTest | 264 | This is where sys_mmap calls fget_mmap to get a file for mmap. | SF4 |
| MapMemoryTest | 265 | I still need to decompile everything here, especially the switch case for file type. | SF4 |
| MapMemoryTest | 266 | Insert error cases here if there are any errors that I can expose through homebrew. | SF4 |
| MapMemoryTest | 269 | If an unopened file descriptor is specified, then returns EBADF. | NU6 |
| MapMemoryTest | 287 | Offset is where in the file we want to map. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 288 | The file is ~500KB large, so offset 0x10000 should be valid. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 297 | Try file mmap with read-write file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 301 | Write empty data to the file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 307 | Read only file mmap for read-write file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 313 | Read-write file mmap on read-write file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 319 | Read-write-execute file mmap on read-write file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 325 | file mmap with offset == file size. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 329 | file mmap with offset + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 333 | file mmap with offset 0 + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 343 | Read-only file mmap on read-only file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 349 | Read-write file mmap on read-only file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 355 | Read-write-execute file mmap on read-only file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 361 | file mmap with valid offset. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 367 | file mmap with offset == file size. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 371 | file mmap with offset + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 375 | file mmap with offset 0 + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 382 | Try file mmap with read-write file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 386 | Write empty data to the file | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 390 | Checks for file size in both size and offset parameters are aligned up. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 391 | Here, we shouldn't see errors unless I try size + offset > 0x10000 | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 392 | file mmap with offset == file size. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 396 | file mmap with offset + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 400 | file mmap with offset 0 + size greater than file size | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 404 | file mmap with size + offset greater than file size, but less than page-aligned file size. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 410 | file mmap with size greater than file size, but less than page-aligned file size. | MG, MW, MF (file sizes and offsets); execute on a file mapping: todo |
| MapMemoryTest | 419 | file mmap on directories fails with EINVAL | SF4 |
| MapMemoryTest | 431 | File mmaps to GPU cause the PS4 to crash, this should get investigated. | SF4 |
| MapMemoryTest | 433 | Based on decompilation, Sanitizer flag with a valid address (below a hardcoded 0x800000000000) restricts prot here. | SF4 |
| MapMemoryTest | 434 | Specifically, if address input > 0xfc00000000, prot is restricted to GpuReadWrite. | SF4 |
| MapMemoryTest | 435 | If address input is still zero here (Fixed flag with null address input?), then address defaults to 0xfc00000000. | SF4 |
| MapMemoryTest | 436 | sys_mmap calls vm_mmap2 with offset aligned down to the nearest page. | SF4 |
| MapMemoryTest | 441 | Starts with a check for size == 0, returning EINVAL. This can't be tested because sys_mmap had an earlier return for this case. | SF4 |
| MapMemoryTest | 442 | Then checks if our mapping will fit in the virtual memory limit? (if not return ENOMEM) | SF4 |
| MapMemoryTest | 443 | Then checks if phys_addr is aligned, returning EINVAL if it isn't. sys_mmap aligned phys_addr for us though. | SF4 |
| MapMemoryTest | 444 | If flags Fixed is specified, and address input is misaligned, return EINVAL. Again, sys_mmap checked this for us. | SF4 |
| MapMemoryTest | 445 | A bunch of checks specific to file mmaps, most of which are un-important for homebrew testing (mostly internal stuff like devices) | SF4 |
| MapMemoryTest | 448 | Run some behavior tests on mmap here. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 449 | Start with expected flag behaviors. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 450 | If address is null and fixed is specified, returns EINVAL. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 456 | When fixed is specified, address will match the input address exactly. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 463 | If a mapping already exists at the address, and no overwrite is not specified, then it will overwrite the mapping. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 464 | This is tested more thoroughly in the flexible memory tests. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 469 | If no overwrite is specified and the area contains a mapping, mmap returns an error. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 473 | Make sure this check is properly applied if the range is partially mapped. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 477 | Make sure this check is properly applied if the range is partially mapped. | BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo |
| MapMemoryTest | 484 | When fixed is not specified, address will search for a free area, starting at a base of 0x200000000. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 485 | libc mappings follow this behavior, so we can't hardcode an address here, instead search manually for a free address to check against. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 504 | VirtualQuery with flags 0 means there must be a mapping here. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 507 | VirtualQuery returns EACCES when failing to find a mapping. Abuse this to find the first free address mmap should use. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 508 | Since we're only mapping one page, we can safely assume any gap is large enough to map. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 513 | If VirtualQuery works properly, addr should be our first free address. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 518 | If this succeeds, sceKernelMmap is using the correct base address for addr == nullptr. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 524 | If fixed is not specified, but address is free, then the mapping will use the specified address. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 525 | Use a hardcoded, but presumably free address for this. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 532 | When fixed is not specified, mmap will search to higher addresses, not lower ones. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 543 | Run mmap with various remaining valid flags to ensure all are at least usable when they should be. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 544 | Many of these are either related to multi-process, dmem, or other forms of memory, and won't have a noticeable impact on anon mappings. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 545 | MAP_2MB_ALIGN and MAP_OPTIMAL_SPACE are removed during sys_mmap, so they have no impact here. | K, HB, VQ10 (search base, hint, upward search, EACCES on free) |
| MapMemoryTest | 548 | Test anon unbacked mmap with shared (1) | SF5 |
| MapMemoryTest | 553 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 567 | Test writing data to memory | SF5 |
| MapMemoryTest | 573 | Unmap after testing. | SF5 |
| MapMemoryTest | 577 | While the shared flag would make the data persist for a file mapping, it does not do anything for anon mappings. | SF5 |
| MapMemoryTest | 586 | Test anon unbacked mmap with private (2) | SF5 |
| MapMemoryTest | 591 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 605 | Test writing data to memory | SF5 |
| MapMemoryTest | 611 | Unmap after testing. | SF5 |
| MapMemoryTest | 615 | Test anon unbacked mmap with void (0x100) | SF5 |
| MapMemoryTest | 620 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 626 | void flag forces prot = 0 | SF5 |
| MapMemoryTest | 635 | Unmap after testing. | SF5 |
| MapMemoryTest | 639 | Test anon mmap with has semaphore (0x200) | SF5 |
| MapMemoryTest | 644 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 658 | Test writing data to memory | SF5 |
| MapMemoryTest | 664 | Unmap after testing. | SF5 |
| MapMemoryTest | 668 | Test anon mmap with no sync (0x800) | SF5 |
| MapMemoryTest | 673 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 687 | Test writing data to memory | SF5 |
| MapMemoryTest | 693 | Unmap after testing. | SF5 |
| MapMemoryTest | 697 | Test anon mmap with system (0x2000) | SF5 |
| MapMemoryTest | 702 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 716 | Test writing data to memory | SF5 |
| MapMemoryTest | 722 | MAP_SYSTEM would normally place this memory under a different budget, test that in FlexibleTest | SF5 |
| MapMemoryTest | 724 | Unmap after testing. | SF5 |
| MapMemoryTest | 728 | Test anon mmap with all available (0x4000) | SF5 |
| MapMemoryTest | 733 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 747 | Test writing data to memory | SF5 |
| MapMemoryTest | 753 | Unmap after testing. | SF5 |
| MapMemoryTest | 757 | Test anon mmap with 2mb align (0x10000) | SF5 |
| MapMemoryTest | 762 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 776 | The 2mb align flag is removed in mmap, so this has no impact on address alignment. | SF5 |
| MapMemoryTest | 779 | Test writing data to memory | SF5 |
| MapMemoryTest | 785 | Unmap after testing. | SF5 |
| MapMemoryTest | 789 | Test anon mmap with no core (0x20000) | SF5 |
| MapMemoryTest | 794 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 808 | Test writing data to memory | SF5 |
| MapMemoryTest | 814 | Unmap after testing. | SF5 |
| MapMemoryTest | 818 | Test anon mmap with prefault read (0x40000) | SF5 |
| MapMemoryTest | 823 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 837 | Test writing data to memory | SF5 |
| MapMemoryTest | 843 | Unmap after testing. | SF5 |
| MapMemoryTest | 847 | Test anon mmap with self (0x80000) | SF5 |
| MapMemoryTest | 852 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 866 | Test writing data to memory | SF5 |
| MapMemoryTest | 872 | Unmap after testing. | SF5 |
| MapMemoryTest | 876 | Test anon mmap with optimal space (0x100000) | SF5 |
| MapMemoryTest | 881 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 895 | Test writing data to memory | SF5 |
| MapMemoryTest | 901 | Unmap after testing. | SF5 |
| MapMemoryTest | 905 | Test anon mmap with writable wb garlic (0x800000) | SF5 |
| MapMemoryTest | 910 | Run VirtualQuery to see if mapping behaved as expected | SF5 |
| MapMemoryTest | 924 | Test writing data to memory | SF5 |
| MapMemoryTest | 930 | Unmap after testing. | SF5 |
| MapMemoryTest | 934 | Executable protections cause dmem file mmaps to fail. | EX2, BT6 |
| MapMemoryTest | 940 | vm_map_find causes ENOMEM return if there are no valid memory areas to map. | EX2, BT6 |
| MapMemoryTest | 941 | If vm_map_find returns any address between 0x80000000 and 0x200000000, ENOMEM is returned. | EX2, BT6 |
| MapMemoryTest | 945 | Notes for sceKernelMapDirectMemory2: | EX2, BT6 |
| MapMemoryTest | 946 | Alignment must be a power of 2, page aligned, and less than 0x100000000 | EX2, BT6 |
| MapMemoryTest | 947 | If alignment is less than page size, it's increased to the page size. | EX2, BT6 |
| MapMemoryTest | 948 | Remaining behavior seems to rely on checks in sys_mmap_dmem. | EX2, BT6 |
| MapMemoryTest | 951 | To run sys_mmap_dmem without worries from phys addresses, allocate some dmem first. | SF6 |
| MapMemoryTest | 968 | Notes for sys_mmap_dmem: | SF6 |
| MapMemoryTest | 969 | Starts with parameter checks. | SF6 |
| MapMemoryTest | 970 | Address, phys_addr, and length must be page aligned. Length = 0 is not allowed. | SF6 |
| MapMemoryTest | 971 | Flags must only contain Fixed, NoOverwrite, Sanitizer, NoCoalesce, and the alignment bits. | SF6 |
| MapMemoryTest | 972 | Prot contains only CpuRead, CpuWrite, GpuRead, or GpuWrite. | SF6 |
| MapMemoryTest | 973 | mtype must be less than 10 | SF6 |
| MapMemoryTest | 974 | If any of these error checks fail, the function returns EINVAL. | SF6 |
| MapMemoryTest | 976 | sys_mmap_dmem also follows the same address input logic as sys_mmap. | SF6 |
| MapMemoryTest | 977 | If address is null and map Fixed is specified, returns EINVAL. | SF6 |
| MapMemoryTest | 979 | sys_mmap_dmem has an extra check for physical addresses overlapping. | SF6 |
| MapMemoryTest | 980 | This check is removed if sceKernelEnableDmemAliasing is called. | SF6 |
| MapMemoryTest | 988 | Misaligned phys_addr | SF6 |
| MapMemoryTest | 1015 | -1 mtype skips assigning type. | SF7 |
| MapMemoryTest | 1019 | Overlapping phys addr causes EBUSY | SF7 |
| MapMemoryTest | 1023 | Try enabling dmem aliasing | SF8 |
| MapMemoryTest | 1027 | Overlapping phys addr works now. | SF8 |
| MapMemoryTest | 1036 | Lower negatives are invalid. | SF7 |
| MapMemoryTest | 1044 | null address with map fixed | BT9 |
| MapMemoryTest | 1052 | Might continue down this rabbit hole at some point, but I don't see too much of a point? | BT9 |
| MapMemoryTest | 1053 | I've gleamed the error cases they start with, everything else will just come from testing, as always. | BT9 |
| DeviceFileTest | 1057 | This function is for testing any lower-level behavior through directly opening device files. | todo |
| DeviceFileTest | 1058 | This is kept as a separate test since, no matter how messy a game is, it generally wont be manipulating low level hardware behavior. | todo |
| DeviceFileTest | 1060 | Test for the writable wb garlic flag (0x800000), this flag is used by a libkernel function that OpenOrbis doesn't have. | todo |
| DeviceFileTest | 1061 | To get around this, we can instead open /dev/dmem1 ourselves to get a file descriptor for mmap. | todo |
| DeviceFileTest | 1065 | Allocate some direct memory for direct memory mapping. | todo |
| DeviceFileTest | 1070 | Start with a typical mmap to verify we're mapping to the dmem file. | todo |
| DeviceFileTest | 1071 | Like libkernel, specify MAP_SHARED flag, though this is theoretically not required. | todo |
| DeviceFileTest | 1076 | To confirm this actually mapped the expected direct memory, check if sceKernelReleaseDirectMemory can unmap it. | todo |
| DeviceFileTest | 1080 | Define OrbisKernelVirtualQueryInfo struct | todo |
| DeviceFileTest | 1098 | If the memory unmapped properly, VirtualQuery will return EACCES. | todo |
| DeviceFileTest | 1102 | Now test the MAP_WRITABLE_WB_GARLIC flag. | todo |
| DeviceFileTest | 1106 | Read-only CPU mapping normally works. | todo |
| DeviceFileTest | 1111 | Run VirtualQuery on this mapping to make sure it has the correct types. | todo |
| DeviceFileTest | 1128 | Despite the flag, write prot on CPU still returns EACCESS for this mtype. | todo |
| DeviceFileTest | 1135 | Read-only GPU mapping normally works. | todo |
| DeviceFileTest | 1141 | write-only GPU should work due to the flag, normally this fails. | todo |
| DeviceFileTest | 1147 | read-write GPU should work due to the flag, normally this fails. | todo |
| DeviceFileTest | 1153 | Release direct memory. | todo |
| DeviceFileTest | 1161 | Some devices can be mmapped, these can follow different rules. | todo |
| DeviceFileTest | 1165 | Perform device file mmap. | todo |
| DeviceFileTest | 1170 | Run sceKernelVirtualQuery to make sure memory area is as expected. | todo |
| DeviceFileTest | 1185 | Device file mmaps are supposed to automatically append MAP_SHARED. | todo |
| DeviceFileTest | 1188 | Read contents of memory into a buffer. | todo |
| DeviceFileTest | 1192 | Since we can't sceKernelRead our way through this, we can instead check for MAP_SHARED by unmapping, then remapping the file. | todo |
| DeviceFileTest | 1193 | If changes persist, this memory is likely backed in some form. If not, we can assume MAP_SHARED was not appended. | todo |
| DeviceFileTest | 1197 | Perform device file mmap. | todo |
| DeviceFileTest | 1201 | Compare the device memory before and after. If the changes from before are still present, the mapping must have MAP_SHARED. | todo |
| DeviceFileTest | 1208 | Device file mmaps fail with EINVAL if you map with flags Private (2) | todo |
| DeviceFileTest | 1215 | Test budget behaviors that wouldn't come up in normal games. | todo |
| DeviceFileTest | 1224 | Mapped flex mem successfully. | todo |
| DeviceFileTest | 1226 | Add the mapping address to addresses, need to unmap later to clean up. | todo |
| DeviceFileTest | 1231 | After all these mappings, available flex size should be 0. | todo |
| DeviceFileTest | 1237 | Device mmaps do not use the flexible budget. | todo |
| DeviceFileTest | 1247 | mmaps from system folders do not use the flexible budget. | todo |
| DeviceFileTest | 1262 | Unmap all of the flexible memory used here. | todo |
| AvailableDirectMemoryTest | 1271 | Not exactly a way for this one to fail. Just need it to test other functions. | todo |
| AvailableDirectMemoryTest | 1275 | Test sceKernelAvailableDirectMemorySize. | todo |
| AvailableDirectMemoryTest | 1282 | Check for known quirks on success. | todo |
| AvailableDirectMemoryTest | 1283 | Specifically, the dmem map already has a mapping (which comes from libSceGnmDriver). | todo |
| AvailableDirectMemoryTest | 1284 | Many games are built with this assumption in mind, though most don't need it emulated. | todo |
| AvailableDirectMemoryTest | 1285 | This mapping will be from physical addresses 0 to 0x10000 under these circumstances. | todo |
| AvailableDirectMemoryTest | 1289 | Now test for potential edge cases. | todo |
| AvailableDirectMemoryTest | 1290 | Based on decompilation, this function should accept both phys_addr and size of nullptr. | todo |
| AvailableDirectMemoryTest | 1294 | If there is no size to output, this function returns ENOMEM | todo |
| AvailableDirectMemoryTest | 1298 | The alignment check is very lenient, they run (align - 1) & align to check. | todo |
| AvailableDirectMemoryTest | 1299 | While this alignment value is far lower than the usually acceptable amount, this still works. | todo |
| AvailableDirectMemoryTest | 1303 | If you use a value that triggers the check, it returns EINVAL | todo |
| AvailableDirectMemoryTest | 1307 | There is no specific error for start > end, it still returns ENOMEM. | todo |
| AvailableDirectMemoryTest | 1311 | There's also no specific error for the range being outside the dmem map, it returns ENOMEM. | todo |
| AvailableDirectMemoryTest | 1318 | Start and end phys addresses are limited internally, with no error return for them. | todo |
| AvailableDirectMemoryTest | 1319 | Make sure size is appropriately restricted. | todo |
| AvailableDirectMemoryTest | 1324 | phys_addr and size are restricted based on the start and end. | todo |
| AvailableDirectMemoryTest | 1325 | So in a case like this, where start is after the start of the free dmem area, size and phys_addr are restricted. | todo |
| AvailableDirectMemoryTest | 1331 | phys_addr and size are restricted based on the start and end. | todo |
| AvailableDirectMemoryTest | 1332 | So in a case like this, where end is before the end of the free dmem area, size and phys_addr are restricted. | todo |
| AvailableDirectMemoryTest | 1338 | start is rounded up to alignment. | todo |
| AvailableDirectMemoryTest | 1344 | end ignores alignment | todo |
| AvailableDirectMemoryTest | 1350 | Alignments below the system requirements are rounded up to the system-wide page size of 0x4000. | todo |
| AvailableDirectMemoryTest | 1351 | This can be tested with the search start, as that is rounded up to alignment. | todo |
| AvailableDirectMemoryTest | 1357 | phys_addr is aligned up to alignment, size is aligned down to compensate. | todo |
| AvailableDirectMemoryTest | 1363 | returned statistics are always from the largest area of free space. | todo |
| AvailableDirectMemoryTest | 1364 | Do an allocation in the middle of the dmem map to demonstrate this. | todo |
| AvailableDirectMemoryTest | 1368 | Nothing else in that area of the dmem map, so this should be the returned phys address. | todo |
| AvailableDirectMemoryTest | 1373 | The larger area should be from 0x10000 to dmem_size / 2 here. | todo |
| AvailableDirectMemoryTest | 1377 | Release the direct memory used for testing this, we want a clean dmem_map. | todo |
| AvailableDirectMemoryTest | 1381 | Free mappings should coalesce, make sure sceKernelAvailableDirectMemorySize still returns the expected values. | todo |
| AllocateDirectMemoryTest | 1389 | Test sceKernelAllocateDirectMemory | todo |
| AllocateDirectMemoryTest | 1390 | Start with the typical working case. | todo |
| AllocateDirectMemoryTest | 1397 | When there is no direct memory at the start, phys_addr equals start. | todo |
| AllocateDirectMemoryTest | 1400 | Keep dmem_map clean. | todo |
| AllocateDirectMemoryTest | 1404 | If either start or end is less than zero, returns EINVAL | todo |
| AllocateDirectMemoryTest | 1411 | If length is zero, returns EINVAL | todo |
| AllocateDirectMemoryTest | 1415 | If there is more than one active bit in align, returns EINVAL | todo |
| AllocateDirectMemoryTest | 1416 | 0xc000 is page-aligned, but should fail this. | todo |
| AllocateDirectMemoryTest | 1420 | If alignment or length is not page aligned, returns EINVAL | todo |
| AllocateDirectMemoryTest | 1426 | If type > 10, return EINVAL | todo |
| AllocateDirectMemoryTest | 1430 | If type < 0, return EINVAL | todo |
| AllocateDirectMemoryTest | 1434 | If limited end <= start, return EAGAIN | todo |
| AllocateDirectMemoryTest | 1438 | If limited end < length, return EAGAIN | todo |
| AllocateDirectMemoryTest | 1442 | If limited end - length < start, return EAGAIN | todo |
| AllocateDirectMemoryTest | 1446 | There is no check for if phys_addr is null in firmware. This call WILL crash the application. | todo |
| AllocateDirectMemoryTest | 1447 | result = sceKernelAllocateDirectMemory(0, dmem_size, 0x20000, 0, 0, nullptr); | todo |
| AllocateDirectMemoryTest | 1449 | Now on to edge cases that don't cause immediate returns. | todo |
| AllocateDirectMemoryTest | 1451 | This call fails with EAGAIN, since there's no direct memory in that area. | todo |
| AllocateDirectMemoryTest | 1458 | phys_addr will be aligned up to alignment. | todo |
| AllocateDirectMemoryTest | 1464 | Keep dmem_map clean. | todo |
| AllocateDirectMemoryTest | 1468 | size is not affected by alignment | todo |
| AllocateDirectMemoryTest | 1474 | Run sceKernelGetDirectMemoryType to confirm mapping dimensions. | todo |
| AllocateDirectMemoryTest | 1483 | Keep dmem_map clean. | todo |
| AllocateDirectMemoryTest | 1487 | If start is later than the start of the free pages, phys_addr = start | todo |
| AllocateDirectMemoryTest | 1492 | Keep dmem_map clean. | todo |
| AllocateDirectMemoryTest | 1496 | If align would force phys_addr up to a mapped page, then that area is unsuitable. | todo |
| AllocateDirectMemoryTest | 1505 | Since dmem_start through dmem_start + 0x4000 is mapped, and alignment is 0x20000, the returned phys_addr is aligned up to dmem_start + 0x20000. | todo |
| AllocateDirectMemoryTest | 1508 | Keep dmem_map clean. | todo |
| AllocateDirectMemoryTest | 1514 | Firmware doesn't align search start, which allows this call to create a misaligned dmem page. | todo |
| AllocateDirectMemoryTest | 1515 | A notable side effect of this behavior is crashing the entire console. | todo |
| AllocateDirectMemoryTest | 1516 | result = sceKernelAllocateDirectMemory(0x10001, dmem_size, 0x4000, 0, 0, &phys_addr); | todo |
| ReleaseDirectMemoryTest | 1520 | Both checked and unchecked return an error if addr or length aren't aligned to page size. | todo |
| ReleaseDirectMemoryTest | 1528 | Releasing free dmem causes error ENOENT on checked release | todo |
| ReleaseDirectMemoryTest | 1532 | Releasing free dmem "succeeds" on unchecked release | todo |
| ReleaseDirectMemoryTest | 1536 | Allocate a little dmem to test with | todo |
| ReleaseDirectMemoryTest | 1546 | Now we've got 0x100000-0x120000 and 0x130000-0x140000 allocated. | todo |
| ReleaseDirectMemoryTest | 1547 | Checked fails if anything in the range is free. | todo |
| ReleaseDirectMemoryTest | 1548 | Make sure you're checking the end of the range. | todo |
| ReleaseDirectMemoryTest | 1551 | Check for the start of the range. | todo |
| ReleaseDirectMemoryTest | 1554 | Check for the middle of the range, first 0x10000 and last 0x10000 are allocated, middle 0x10000 is free. | todo |
| ReleaseDirectMemoryTest | 1558 | Because of how checked fails, the allocations are safe. | todo |
| ReleaseDirectMemoryTest | 1572 | Unchecked will deallocate both free areas | todo |
| ReleaseDirectMemoryTest | 1576 | As a result of the unchecked release, this sceKernelGetDirectMemoryType call fails. | todo |
| ReleaseDirectMemoryTest | 1582 | Release should unmap any mappings that reference the physical block. | todo |
| ReleaseDirectMemoryTest | 1583 | Allocate a little dmem to test with | todo |
| ReleaseDirectMemoryTest | 1598 | Since the mapping was unmapped through ReleaseDirectMemory, QueryMemoryProtection should return EACCES. | todo |
| ReleaseDirectMemoryTest | 1603 | Test for various edge cases related to file mmaps | todo |
| FileMappingTest | 1605 | Start with behaviors of read-only mmaps | todo |
| FileMappingTest | 1610 | Run a sceKernelRead to read from the file directly. | todo |
| FileMappingTest | 1616 | Using flags 0, prot read-write. | todo |
| FileMappingTest | 1623 | With this mmap, we should be able to read from the file. | todo |
| FileMappingTest | 1624 | Run a memcpy to copy from this memory to a buffer. | todo |
| FileMappingTest | 1628 | Compare memory buffer to file contents. | todo |
| FileMappingTest | 1632 | Write zeros to the memory area, then read back the new data. | todo |
| FileMappingTest | 1636 | Re-read file contents | todo |
| FileMappingTest | 1642 | Compare memory buffer to file contents. | todo |
| FileMappingTest | 1644 | File contents should not be modified by the memory write. | todo |
| FileMappingTest | 1656 | Run a memcpy to copy from this memory to a buffer. | todo |
| FileMappingTest | 1659 | Compare memory buffer to file contents. | todo |
| FileMappingTest | 1667 | Check for alignment behavior. | todo |
| FileMappingTest | 1672 | Size should round up to 0x10000, offset should round down to 0x4000. | todo |
| FileMappingTest | 1678 | mmap's address output is increased by page offset. | todo |
| FileMappingTest | 1679 | This is not reflected in the actual mapping. | todo |
| FileMappingTest | 1684 | Run virtual query to check details | todo |
| FileMappingTest | 1685 | Need to define struct first. | todo |
| FileMappingTest | 1706 | VirtualQuery does not report file mmap offsets. | todo |
| FileMappingTest | 1716 | Run a memcpy to copy from this memory to a buffer. | todo |
| FileMappingTest | 1719 | Compare memory buffer to file contents. | todo |
| FileMappingTest | 1735 | Run a sceKernelRead on /dev/urandom to get random numbers | todo |
| FileMappingTest | 1741 | Use sceKernelWrite to write the random data to the file. | todo |
| FileMappingTest | 1745 | Seek back to the beginning of the file. | todo |
| FileMappingTest | 1750 | Using flags 0, prot read-write. | todo |
| FileMappingTest | 1757 | Run a memcpy to copy from this memory to a buffer, then compare it to the file data. | todo |
| FileMappingTest | 1766 | Test misaligned file size behavior. | todo |
| FileMappingTest | 1767 | This should leave the file contents intact. | todo |
| FileMappingTest | 1771 | Using flags 0, prot read-write. | todo |
| FileMappingTest | 1775 | Run sceKernelVirtualQuery to make sure memory area is as expected. | todo |
| FileMappingTest | 1790 | Run a memcpy to copy from this memory to a buffer, then compare it to the file data. | todo |
| FileMappingTest | 1792 | Only compare the parts that are in the file. Outside the file, memory contents differ from old file contents. | todo |
| FileMappingTest | 1796 | Bytes outside the file contents are zeroed. | todo |
| FileMappingTest | 1806 | Alignment behavior allows this call to succeed too, since offset aligns down and size aligns up. | todo |
| FileMappingTest | 1807 | Remember that output_addr is misaligned here due to the offset. | todo |
| FileMappingTest | 1811 | Run sceKernelVirtualQuery to make sure memory area is as expected. | todo |
| FileMappingTest | 1826 | Run a memcpy to copy from this memory to a buffer, then compare it to the file data. | todo |
| FileMappingTest | 1828 | Only compare the parts that are in the file. Outside the file, memory contents differ from old file contents. | todo |
| FileMappingTest | 1832 | Bytes outside the file contents are zeroed. | todo |
| FileMappingTest | 1841 | Now test MAP_SHARED behavior (safely). | todo |
| FileMappingTest | 1842 | This flag copies changes from memory to the file. | todo |
| FileMappingTest | 1843 | Note that using this flag with read-only files is dangerous, instantly crashes PS4s and causes data corruption. | todo |
| FileMappingTest | 1847 | Right now the data we get will be the same random values. | todo |
| FileMappingTest | 1848 | Do a memory write to write 0s to the start of the file. | todo |
| FileMappingTest | 1851 | Lseek to the start of the file, then read the first 0x8000 bytes. | todo |
| FileMappingTest | 1857 | Compare the memory to the raw file contents. | todo |
| FileMappingTest | 1869 | Test closing a file while it's mmapped | todo |
| FileMappingTest | 1886 | Compare the memory to the raw file contents. | todo |
| FlexibleTest | 1896 | This test is to validate memory behaviors unique to flexible memory mappings. | todo |
| FlexibleTest | 1897 | Start by testing available flexible memory size. | todo |
| FlexibleTest | 1907 | Mapped flex mem successfully. | todo |
| FlexibleTest | 1909 | Add the mapping address to addresses, need to unmap later to clean up. | todo |
| FlexibleTest | 1914 | After all these mappings, available flex size should be 0. | todo |
| FlexibleTest | 1920 | sceKernelMmap with MAP_ANON uses the flexible budget. | todo |
| FlexibleTest | 1925 | Test budget behavior with MAP_STACK | todo |
| FlexibleTest | 1930 | sceKernelMmap with MAP_VOID | todo |
| FlexibleTest | 1936 | sceKernelMmap with files also uses the flexible budget. | todo |
| FlexibleTest | 1944 | Make sure download dir file has enough space to mmap before trying. | todo |
| FlexibleTest | 1952 | mmap download dir file. | todo |
| FlexibleTest | 1958 | Files in system folders like /data don't count towards the budget. | todo |
| FlexibleTest | 1973 | We can't directly correlate flex size to mapped memory, | todo |
| FlexibleTest | 1974 | since my list of addresses will also cut into the flex mem budget. | todo |
| FlexibleTest | 1976 | Unmap all of the flexible memory used here. | todo |
| FlexibleTest | 1983 | Available flex size should be greater than 0 now | todo |
| FlexibleTest | 1988 | Flexible memory is not backed in any way, so the contents are garbage before initializing. | todo |
| FlexibleTest | 1993 | While we're here, make sure stored memory info matches expectations for flexible memory. | todo |
| FlexibleTest | 2023 | Write 1's to the full memory range. | todo |
| FlexibleTest | 2026 | Ensure the full range has 1's | todo |
| FlexibleTest | 2031 | Unmap the middle of the range | todo |
| FlexibleTest | 2035 | Ensure the remaining mapped areas are still filled with 1's | todo |
| FlexibleTest | 2041 | Remap the unmapped area, use flag fixed to ensure correct placement. | todo |
| FlexibleTest | 2047 | The new memory area will not have the contents of the old memory. | todo |
| FlexibleTest | 2051 | Re-fill the area with 1's | todo |
| FlexibleTest | 2056 | Overwrite the whole memory area, this should destroy the contents within. | todo |
| FlexibleTest | 2068 | Get a base budget value to calculate with. | todo |
| FlexibleTest | 2074 | Note: Seems like allocations are not directly consuming the budget. | todo |
| FlexibleTest | 2075 | Using these larger sized mappings allows me to validate budget behavior, but I will need to test this issue. | todo |
| FlexibleTest | 2077 | Test sceKernelMapFlexibleMemory to see how it impacts the budget. | todo |
| FlexibleTest | 2081 | This should reduce available size by 0x100000. | todo |
| FlexibleTest | 2090 | This should bring available back to normal. | todo |
| FlexibleTest | 2095 | Test mmap with MAP_ANON to see how it impacts the budget. | todo |
| FlexibleTest | 2099 | This should reduce available size by 0x100000. | todo |
| FlexibleTest | 2108 | This should bring available back to normal. | todo |
| FlexibleTest | 2113 | Test file mmap from download dir to see how it impacts the budget. | todo |
| FlexibleTest | 2119 | This should reduce available size by 0x10000. | todo |
| FlexibleTest | 2132 | This should bring available back to normal. | todo |
| DirectTest | 2139 | This test is to validate memory behaviors somewhat unique to direct memory mappings. | todo |
| DirectTest | 2140 | This will focus on behavioral edge cases, since all the error checks involved are checked in MapMemoryTest. | todo |
| DirectTest | 2142 | First, sceKernelMapDirectMemory on physical addresses that are not mapped. | todo |
| DirectTest | 2143 | Once again, due to the SceGnmDriver mapping, 0 through 0x10000 is allocated. | todo |
| DirectTest | 2149 | Since it's a different syscall, check sceKernelMapDirectMemory2 as well. | todo |
| DirectTest | 2154 | Allocate some direct memory to test with. | todo |
| DirectTest | 2159 | Perform a direct memory mapping. | todo |
| DirectTest | 2163 | While we're here, make sure stored memory info matches expectations for direct memory. | todo |
| DirectTest | 2184 | Offset is set to physical address. | todo |
| DirectTest | 2194 | Perform a second mapping to the same physical address, this should mirror the first one. | todo |
| DirectTest | 2199 | Both calls should succeed, and writes to the first address will be mirrored to the second. | todo |
| DirectTest | 2204 | Store the current state of this memory | todo |
| DirectTest | 2208 | Both addresses should be unmapped by sceKernelCheckedReleaseDirectMemory | todo |
| DirectTest | 2212 | Ensure physical memory is properly released | todo |
| DirectTest | 2223 | Backing contents should remain the same despite the direct memory release. | todo |
| DirectTest | 2227 | Partially release page. In theory, this should only unmap the part of mapped to these physical addresses. | todo |
| DirectTest | 2231 | Use sceKernelQueryMemoryProtection to validate expected addresses. | todo |
| DirectTest | 2241 | We unmapped from 0x4000 to 0x8000 in the mapping. Query past that gap | todo |
| DirectTest | 2248 | Unmap the full direct memory area. | todo |
| DirectTest | 2252 | Remap the area, this should fail due to the missing physical address chunk. | todo |
| DirectTest | 2256 | Since it's a different syscall, check sceKernelMapDirectMemory2 as well. | todo |
| DirectTest | 2260 | Release any remaining physical memory from before. Use sceKernelReleaseDirectMemory to skip the unallocated addresses. | todo |
| DirectTest | 2264 | Loop through all possible mtypes, ensure all are backed appropriately | todo |
| DirectTest | 2268 | Allocate direct memory using specified mtype | todo |
| DirectTest | 2272 | Map direct memory to this physical address space | todo |
| DirectTest | 2277 | Check for proper backing behavior. As per usual, copy bytes, unmap, remap, then check for bytes. | todo |
| DirectTest | 2287 | If this succeeds, this memory is backed. | todo |
| DirectTest | 2291 | Release direct memory used for this test. | todo |
| DirectTest | 2295 | While we're here, we can test sceKernelMapDirectMemory2's type-setting. | todo |
| DirectTest | 2296 | The following code was written with the SceGnmDriver dmem allocation in mind, | todo |
| DirectTest | 2297 | instead of rewriting the code to remove that assumption, perform an extra dmem allocation with the same type and size. | todo |
| DirectTest | 2303 | Allocate direct memory with mtype | todo |
| DirectTest | 2307 | Use sceKernelGetDirectMemoryType to ensure type is stored correctly | todo |
| DirectTest | 2314 | This is the mtype of our extra allocation. | todo |
| DirectTest | 2315 | Allocations with this type will coalesce with it. | todo |
| DirectTest | 2323 | Use sceKernelMapDirectMemory2 to change memory type. | todo |
| DirectTest | 2328 | Use sceKernelGetDirectMemoryType to ensure type is updated correctly | todo |
| DirectTest | 2331 | sceKernelMapDirectMemory2 doesn't coalesce dmem areas after changing type. | todo |
| DirectTest | 2333 | This is the mtype of our extra allocation. | todo |
| DirectTest | 2334 | Allocations with this type will coalesce with it. | todo |
| DirectTest | 2342 | Use sceKernelMunmap to unmap memory | todo |
| DirectTest | 2346 | Use sceKernelGetDirectMemoryType to ensure type remains the same | todo |
| DirectTest | 2349 | sceKernelMapDirectMemory2 doesn't coalesce dmem areas after changing type. | todo |
| DirectTest | 2351 | This is the mtype of our extra allocation. | todo |
| DirectTest | 2352 | Allocations with this type will coalesce with it. | todo |
| DirectTest | 2360 | Use sceKernelCheckedReleaseDirectMemory to erase direct memory | todo |
| DirectTest | 2365 | Release the extra allocation as well. | todo |
| DirectTest | 2370 | memory type 10 does not allow write permissions of any form. | todo |
| DirectTest | 2413 | Make sure permissions are checked appropriately when changing type through sceKernelMapDirectMemory2 | todo |
| DirectTest | 2431 | Test dmem coalescing edge cases | todo |
| DirectTest | 2432 | We can get two "mergable" direct memory areas by utilizing sceKernelMapDirectMemory2. | todo |
| DirectTest | 2435 | This will produce two separate direct memory areas. | todo |
| DirectTest | 2441 | Use sceKernelMapDirectMemory2 to set the mtype of the first allocation to match the second. | todo |
| DirectTest | 2442 | Despite doing this, the two areas remain separate. | todo |
| DirectTest | 2446 | We don't need the dmem mapped for this test, so unmap it. | todo |
| DirectTest | 2450 | Verify dmem map state | todo |
| DirectTest | 2465 | Try to map both areas in one sceKernelMapDirectMemory call. | todo |
| DirectTest | 2471 | That mapping will not coalesce the dmem areas. | todo |
| DirectTest | 2483 | Ensure checked release works properly under these conditions. | todo |
| DirectTest | 2492 | Set up four separate allocations, unmergable due to memory type. | todo |
| DirectTest | 2497 | Leave a gap here, this will be where we allocate the third area. | todo |
| DirectTest | 2503 | Use sceKernelMapDirectMemory2 to set the mtypes of the first and third allocations to match the second and fourth. | todo |
| DirectTest | 2515 | Now allocate a fifth area that is mergable with all nearby areas. This will merge 2, 3, and 4, but leave 1 and 5 separate. | todo |
| DirectTest | 2519 | Use sceKernelGetDirectMemoryType to check. | todo |
| DirectTest | 2536 | This should release all allocations. | todo |
| DirectTest | 2540 | Set up two unmerged and unmapped allocations to test sceKernelMapDirectMemory2 with. | todo |
| DirectTest | 2541 | Release our allocation, then set the unmerged dmem areas back up. | todo |
| DirectTest | 2557 | Now we have two 0x10000 dmem pages with the same type, but unmerged. Validate this. | todo |
| DirectTest | 2569 | Run sceKernelMapDirectMemory2 to map both unallocated pages. Use -1 for mtype param to skip changing mtype. | todo |
| DirectTest | 2575 | This will not merge the areas. | todo |
| DirectTest | 2587 | Run sceKernelMapDirectMemory2 to map and change type for both areas. | todo |
| DirectTest | 2593 | This does not merge the areas. | todo |
| DirectTest | 2605 | This should release both allocations. | todo |
| CoalescingTest | 2619 | Define lambdas for memory calls. This (along with the optnone attribute) is needed to ensure a static calling address | todo |
| CoalescingTest | 2620 | (which is one of the things checked when merging vmem areas). | todo |
| CoalescingTest | 2622 | This function is effectively an mmap wrapper, except for the case of mapping direct memory. | todo |
| CoalescingTest | 2624 | Used for a direct memory mapping, use sceKernelAllocateDirectMemory and sceKernelMapDirectMemory. | todo |
| CoalescingTest | 2637 | Can't GPU map files. | todo |
| CoalescingTest | 2641 | Reserved memory has no protection, mmap just ignores prot. | todo |
| CoalescingTest | 2648 | We can use a VirtualQuery to see if this is direct or not | todo |
| CoalescingTest | 2666 | Just release the direct memory, which will unmap for us. | todo |
| CoalescingTest | 2675 | Test memory coalescing behaviors. | todo |
| CoalescingTest | 2676 | For safety, start by reserving a 0xa0000 sized chunk of virtual memory | todo |
| CoalescingTest | 2681 | To test coalescing in it's fullest, we'll map outside-in | todo |
| CoalescingTest | 2700 | None of these mappings should combine. | todo |
| CoalescingTest | 2723 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 2730 | Nothing combines, we now have 5 separate areas. | todo |
| CoalescingTest | 2756 | Test using sceKernelSetVirtualRangeName, see if that merges anything. | todo |
| CoalescingTest | 2762 | Nothing combines, all areas are named separately. | todo |
| CoalescingTest | 2788 | Test using sceKernelMprotect, see if anything merges. | todo |
| CoalescingTest | 2794 | Nothing combines, all areas now have reduced prot. | todo |
| CoalescingTest | 2820 | Test using sceKernelMlock, see if anything merges. | todo |
| CoalescingTest | 2852 | Unmap testing memory. | todo |
| CoalescingTest | 2858 | Perform memory reservations instead. | todo |
| CoalescingTest | 2877 | Check the state of the vmem areas. | todo |
| CoalescingTest | 2878 | Reserved areas coalesce normally. | todo |
| CoalescingTest | 2879 | sceKernelQueryMemoryProtection returns errors with reserved memory, use sceKernelVirtualQuery instead. | todo |
| CoalescingTest | 2907 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 2914 | All reserved areas should coalesce together here. | todo |
| CoalescingTest | 2921 | Unmap testing memory. | todo |
| CoalescingTest | 2927 | Perform memory reservations without MAP_SHARED. This appears to skip some coalescing logic? | todo |
| CoalescingTest | 2946 | Check the state of the vmem areas. | todo |
| CoalescingTest | 2947 | sceKernelQueryMemoryProtection returns errors with reserved memory, use sceKernelVirtualQuery instead. | todo |
| CoalescingTest | 2960 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 2967 | Without MAP_SHARED, the new mapping only merges with the previous mapping, the later mapping remains unmerged. | todo |
| CoalescingTest | 2980 | Unmap testing memory. | todo |
| CoalescingTest | 2986 | Now test with direct memory. | todo |
| CoalescingTest | 2987 | With SDK version 1.00, this will not coalesce. | todo |
| CoalescingTest | 3026 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 3033 | Mappings remain separate. | todo |
| CoalescingTest | 3059 | Test using sceKernelSetVirtualRangeName, see if that merges anything. | todo |
| CoalescingTest | 3065 | Mappings remain separate, name is applied to all. | todo |
| CoalescingTest | 3091 | Test using sceKernelMprotect, see if anything merges. | todo |
| CoalescingTest | 3097 | Mappings remain separate, protection is applied to all. | todo |
| CoalescingTest | 3123 | Test using sceKernelMlock, see if anything merges. | todo |
| CoalescingTest | 3129 | Mappings remain separate. | todo |
| CoalescingTest | 3155 | Unmap testing memory. | todo |
| CoalescingTest | 3161 | Test coalescing with a file mmap. | todo |
| CoalescingTest | 3168 | file mmaps coalesce so long as MAP_SHARED is provided. Otherwise, they remain separate. | todo |
| CoalescingTest | 3169 | Start by testing without MAP_SHARED. | todo |
| CoalescingTest | 3208 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 3215 | Mappings remain separate. | todo |
| CoalescingTest | 3241 | Test using sceKernelSetVirtualRangeName, see if that merges anything. | todo |
| CoalescingTest | 3247 | Mappings remain separate. | todo |
| CoalescingTest | 3273 | Test using sceKernelMprotect, see if anything merges. | todo |
| CoalescingTest | 3279 | Mappings remain separate, protection is applied to all. | todo |
| CoalescingTest | 3305 | Test using sceKernelMlock, see if anything merges. | todo |
| CoalescingTest | 3311 | Mappings remain separate. | todo |
| CoalescingTest | 3337 | Unmap testing memory. | todo |
| CoalescingTest | 3343 | file mmaps coalesce so long as MAP_SHARED is provided. Otherwise, they remain separate. | todo |
| CoalescingTest | 3344 | Test with MAP_SHARED here. | todo |
| CoalescingTest | 3373 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 3380 | Mappings all combine together. | todo |
| CoalescingTest | 3386 | Unmap testing memory. | todo |
| CoalescingTest | 3392 | Check behavior for read-only file mmaps without MAP_SHARED | todo |
| CoalescingTest | 3393 | This case also merges, like how MAP_SHARED behaves. | todo |
| CoalescingTest | 3422 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 3429 | Mappings all combine together. | todo |
| CoalescingTest | 3435 | Unmap testing memory. | todo |
| CoalescingTest | 3441 | Now test the NoCoalesce (0x400000) flag | todo |
| CoalescingTest | 3442 | With this, even reserved memory will not coalesce. | todo |
| CoalescingTest | 3461 | Check the state of the vmem areas. | todo |
| CoalescingTest | 3462 | Due to the NoCoalesce flag, these won't combine | todo |
| CoalescingTest | 3487 | Test making a mapping in between these other mappings. | todo |
| CoalescingTest | 3488 | Omit the NoCoalesce flag, this still shouldn't coalesce due to the surrounding areas. | todo |
| CoalescingTest | 3495 | The mappings remain separate. | todo |
| CoalescingTest | 3526 | Test using sceKernelSetVirtualRangeName, see if that merges anything. | todo |
| CoalescingTest | 3532 | Even there, mappings shouldn't coalesce. | todo |
| CoalescingTest | 3563 | Unmap testing memory. | todo |
| ProtectTest | 3569 | Set up some basic memory this test can use. | todo |
| ProtectTest | 3570 | Allocate a decent chunk of direct memory, this area should be free, and isn't touched by other tests. | todo |
| ProtectTest | 3576 | The elevated dmem_start should ensure this area is free, so dmem_phys_addr should equal dmem_start. | todo |
| ProtectTest | 3579 | Reserve a chunk of memory for testing with. | todo |
| ProtectTest | 3580 | Use a fixed base address that should be free. | todo |
| ProtectTest | 3586 | MAP_FIXED should ensure this is the same. | todo |
| ProtectTest | 3589 | Start with some "normal" behavior to ensure edge case tests are based on valid calls. | todo |
| ProtectTest | 3590 | Map some direct memory, give it write permissions, write some test data, then protect with read and read that data back. | todo |
| ProtectTest | 3591 | This will test basic functionality of mprotect. | todo |
| ProtectTest | 3597 | For simplicity, use sceKernelVirtualQuery to validate prot. | todo |
| ProtectTest | 3598 | While not necessary for most mappings, some tests will need it. | todo |
| ProtectTest | 3613 | Verify the lack of protection. | todo |
| ProtectTest | 3619 | Use mprotect to set the full memory area to read-write. | todo |
| ProtectTest | 3620 | A future edge case to test is that read is forcibly appended to write mappings (which is why I'm using read-write prot here). | todo |
| ProtectTest | 3629 | Now write to the new memory area. | todo |
| ProtectTest | 3633 | Now protect with no prot | todo |
| ProtectTest | 3642 | Now protect with read prot. | todo |
| ProtectTest | 3643 | If protection is handled properly, the string I copied should still be there. | todo |
| ProtectTest | 3652 | Now read from the memory | todo |
| ProtectTest | 3656 | Replace mapped memory with a reserved area before proceeding. | todo |
| ProtectTest | 3657 | Since this compiles with SDK ver 1.00, the only way to ensure all mappings coalesce into one is to re-reserve the whole area. | todo |
| ProtectTest | 3663 | Memory reserved through sceKernelReserveVirtualRange (or mmap with MAP_VOID) restricts maximum prot to 0. | todo |
| ProtectTest | 3664 | That said, this call still splits the vmem area. | todo |
| ProtectTest | 3689 | Note: Attempting to give GPU protections to a reserved memory area causes a full system crash. | todo |
| ProtectTest | 3690 | Only uncomment this if you're willing to unplug your PS4 just to get it to shut off | todo |
| ProtectTest | 3691 | result = sceKernelMprotect(addr, 0x20000, 0x10); | todo |
| ProtectTest | 3693 | This should split the memory area up further. | todo |
| ProtectTest | 3732 | This should re-merge the inner mappings | todo |
| ProtectTest | 3757 | This mprotect will re-coalesce the full reservation. | todo |
| ProtectTest | 3768 | This mprotect should do absolutely nothing. | todo |
| ProtectTest | 3779 | Now test executable mappings. These are rarely seen in retail games, but utilized by some homebrew. | todo |
| ProtectTest | 3792 | Write a basic function into memory that returns 256. | todo |
| ProtectTest | 3798 | Now use mprotect to turn this flexible mapping into an executable memory area. | todo |
| ProtectTest | 3802 | Now run the function from memory, make sure it returns the expected value. | todo |
| ProtectTest | 3806 | Replace mapped memory with a reserved area before proceeding. | todo |
| ProtectTest | 3812 | Now test direct memory behavior. | todo |
| ProtectTest | 3813 | As MapMemoryTest shows, direct memory mappings with executable protections aren't allowed. | todo |
| ProtectTest | 3814 | However, you can protect the direct memory, and it will be executable. | todo |
| ProtectTest | 3827 | Write a function into this memory. | todo |
| ProtectTest | 3831 | Now use mprotect to turn this direct mapping into an executable area. | todo |
| ProtectTest | 3842 | Now run the function from memory, make sure it returns the expected value. | todo |
| ProtectTest | 3846 | Replace mapped memory with a reserved area before proceeding. | todo |
| ProtectTest | 3852 | In addition, we should be able to execute from files too. | todo |
| ProtectTest | 3853 | use sceKernelWrite to write our function to the file data, then mmap the file with execute protections and run the function. | todo |
| ProtectTest | 3860 | The function should be at the start of the file. | todo |
| ProtectTest | 3861 | While the file mmap succeeds, the execute protection isn't applied. | todo |
| ProtectTest | 3873 | Use mprotect to apply execute protection. | todo |
| ProtectTest | 3884 | Run the function from memory. Since it's at the start of the file, it should be at the start of the memory area. | todo |
| ProtectTest | 3889 | Replace mapped memory with a reserved area before proceeding. | todo |
| ProtectTest | 3895 | Close the test file. | todo |
| ProtectTest | 3899 | If an mprotect call covers multiple memory areas, it will protect all of them. | todo |
| ProtectTest | 3901 | Start by splitting the reserved area up again. | todo |
| ProtectTest | 3904 | Do some weird arithmetic to ensure we never accidentally duplicate prots | todo |
| ProtectTest | 3905 | Since PS4 appends read to all write-only protections. | todo |
| ProtectTest | 3911 | Verify the splitting occurred as expected. | todo |
| ProtectTest | 3921 | Perform an mprotect that will merge the whole area together. | todo |
| ProtectTest | 3922 | Include extra space in the call to ensure this behavior still happens if free areas are encountered. | todo |
| ProtectTest | 3932 | Finally, perform a flexible mapping, then protect the whole map and check it. | todo |
| ProtectTest | 3933 | The full map mprotect will include this mapping. | todo |
| ProtectTest | 3945 | This does mark various mappings in the memory map with RWX prot. | todo |
| ProtectTest | 3946 | Not all mappings are marked though, so this needs further investigation. | todo |
| ProtectTest | 3957 | Replace mapped memory with a reserved area before proceeding. | todo |
| ProtectTest | 3963 | The PS4 appends read to all write only mappings. | todo |
| ProtectTest | 3974 | Write-only protection. | todo |
| ProtectTest | 3985 | Write \| Execute protection. | todo |
| ProtectTest | 3996 | Finally, test parameters. | todo |
| ProtectTest | 3997 | Start with the parameter alignment. | todo |
| ProtectTest | 4001 | Alignment should align size up to 0x10000 | todo |
| ProtectTest | 4011 | Alignment should align addr + 0x3000 down to addr, and that offset means size is aligned up to 0x14000 | todo |
| ProtectTest | 4018 | Because size rounding follows the formula (addr & page_mask) + page_mask + size & ~page_mask | todo |
| ProtectTest | 4019 | addr + 0x3000 is rounded down to addr, and size gets rounded up to 0x14000 here. | todo |
| ProtectTest | 4029 | mprotect simply ignores invalid protection flags. | todo |
| ProtectTest | 4033 | Alignment should align addr down to 0x1000, size up to 0x10000 | todo |
| ProtectTest | 4040 | Clean up memory used. | todo |
| ProtectTest | 4046 | At this point, the memory designated by vmem_start is free. | todo |
| ProtectTest | 4047 | mprotect succeeds on free memory, but does nothing to it (not that you can actually check for that) | todo |
| TypeProtectTest | 4056 | This function uses completely separate code from mprotect. | todo |
| TypeProtectTest | 4059 | If mtype is greater than 10, returns EINVAL | todo |
| TypeProtectTest | 4060 | If prot & 0xc8 != 0, returns EINVAL (invalid prot check? 0xc8 = 0x8 \| 0x40 \| 0x80) | todo |
| TypeProtectTest | 4061 | Bits higher than that are simply ignored, so prot 0x100 results in protecting with prot 0. | todo |
| TypeProtectTest | 4062 | If mtype == 10, and prot contains execute or write permissions, then returns EPERM. | todo |
| TypeProtectTest | 4063 | Blockpool logic, like in most places, diverges part-way through here. | todo |
| TypeProtectTest | 4066 | Like with the mprotect tests, allocate a chunk of direct memory, and a chunk of virtual memory space for testing. | todo |
| TypeProtectTest | 4067 | Set up some basic memory this test can use. | todo |
| TypeProtectTest | 4068 | Allocate a decent chunk of direct memory, this area should be free, and isn't touched by other tests. | todo |
| TypeProtectTest | 4074 | The elevated dmem_start should ensure this area is free, so dmem_phys_addr should equal dmem_start. | todo |
| TypeProtectTest | 4077 | Reserve a chunk of memory for testing with. | todo |
| TypeProtectTest | 4078 | Use a fixed base address that should be free. | todo |
| TypeProtectTest | 4084 | MAP_FIXED should ensure this is the same. | todo |
| TypeProtectTest | 4087 | As a basic test case, perform a direct memory mapping, then mtypeprotect it. | todo |
| TypeProtectTest | 4107 | Verify the lack of protection. | todo |
| TypeProtectTest | 4114 | Perform both type-setting and prot setting here to ensure it fully functions. | todo |
| TypeProtectTest | 4118 | Both prot and type can be validated through sceKernelVirtualQuery calls. | todo |
| TypeProtectTest | 4125 | Use sceKernelGetDirectMemoryType to confirm the type changing applied properly. | todo |
| TypeProtectTest | 4140 | If the memory is now read-write, we can both write and read from it. Test this. | todo |
| TypeProtectTest | 4144 | Reduce prot to read-only, change mtype again. | todo |
| TypeProtectTest | 4151 | Basic sanity check complete, now we can test the basic edge cases. | todo |
| TypeProtectTest | 4153 | For basic parameter testing, use the memory mapped in the prior test. | todo |
| TypeProtectTest | 4154 | Test invalid type parameters. | todo |
| TypeProtectTest | 4160 | Test type 10 with write prots | todo |
| TypeProtectTest | 4166 | Test invalid prot params? | todo |
| TypeProtectTest | 4174 | Higher bits are ignored, protect still succeeds. | todo |
| TypeProtectTest | 4183 | Try mtypeprotect to mtype 10, then mprotect to prot write. | todo |
| TypeProtectTest | 4192 | Mprotect fails since the physical memory has type 10, which can't be mapped with write perms. | todo |
| TypeProtectTest | 4196 | Type changing occurs before prot, so this succeeds. | todo |
| TypeProtectTest | 4205 | Now for the "fun" stuff, try splitting direct memory by setting types. | todo |
| TypeProtectTest | 4206 | We can use the direct memory area we already have for this. | todo |
| TypeProtectTest | 4210 | mem_scan to help visualize | todo |
| TypeProtectTest | 4213 | With this, our 1 direct memory mapping should split into three. | todo |
| TypeProtectTest | 4236 | Additionally, there should be 3 dmem areas within this test's dmem range. | todo |
| TypeProtectTest | 4247 | physical areas coalesce when type-changing through mtypeprotect. | todo |
| TypeProtectTest | 4254 | Now try mtypeprotect over the whole area with type = 1 | todo |
| TypeProtectTest | 4258 | mem_scan to help visualize | todo |
| TypeProtectTest | 4261 | With this, the areas should all receive the updated type, but won't remerge. | todo |
| TypeProtectTest | 4284 | All physical areas should coalese. | todo |
| TypeProtectTest | 4296 | Test Mtypeprotect with memory that is not direct. | todo |
| TypeProtectTest | 4297 | Overwrite the direct memory mapping with flexible memory | todo |
| TypeProtectTest | 4301 | mtype check occurs first, so this fails. | todo |
| TypeProtectTest | 4305 | Mtypeprotect fails on memory that isn't direct. | todo |
| TypeProtectTest | 4309 | Mtypeprotect fails on memory that isn't direct. | todo |
| TypeProtectTest | 4316 | Mtypeprotect on reserved memory fails | todo |
| TypeProtectTest | 4323 | Mtypeprotect on free memory fails | todo |
| TypeProtectTest | 4327 | Clean up memory used. | todo |

## test_170.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW170Test | 21 | Null address with MAP_FIXED is now prohibited from the libkernel side. | todo |
| FW170Test | 41 | Note: Pool 1 should match sceKernelMapDirectMemory behavior. | todo |
| FW170Test | 48 | mmap and sceKernelMemoryPoolReserve both still reach the mmap syscall, but error there like fw 1.00 does. | todo |
| FW170Test | 59 | Test behavior that changed in firmware 2.00, these differences should not be present here. | todo |
| FW200Test | 62 | This function is effectively an mmap wrapper, except for the case of mapping direct memory. | todo |
| FW200Test | 64 | Used for a direct memory mapping, use sceKernelAllocateDirectMemory and sceKernelMapDirectMemory. | todo |
| FW200Test | 78 | Can't GPU map files. | todo |
| FW200Test | 82 | Reserved memory has no protection, mmap just ignores prot. | todo |
| FW200Test | 89 | We can use a VirtualQuery to see if this is direct or not | todo |
| FW200Test | 107 | Just release the direct memory, which will unmap for us. | todo |
| FW200Test | 116 | In firmware 2.00, direct memory mappings coalesce. | todo |
| FW200Test | 136 | On firmware 2.00, there will be 2 mappings. Here, there should be 4. | todo |
| FW200Test | 159 | Test making a mapping in between these other mappings. | todo |
| FW200Test | 164 | There should now be 5 mappings. | todo |
| FW200Test | 190 | Mappings split through mprotect and mtypeprotect will not re-coalesce on older SDK versions. | todo |
| FW200Test | 214 | Mappings remain separate. | todo |
| FW200Test | 228 | Areas split due to changed mtype | todo |
| FW200Test | 247 | Areas do not recombine. | todo |
| FW200Test | 266 | Areas split due to changed prot | todo |
| FW200Test | 285 | Areas do not recombine. | todo |
| FW200Test | 304 | Unmap testing memory. | todo |

## test_200.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW200Test | 19 | This function is effectively an mmap wrapper, except for the case of mapping direct memory. | todo |
| FW200Test | 21 | Used for a direct memory mapping, use sceKernelAllocateDirectMemory and sceKernelMapDirectMemory. | todo |
| FW200Test | 35 | Can't GPU map files. | todo |
| FW200Test | 39 | Reserved memory has no protection, mmap just ignores prot. | todo |
| FW200Test | 46 | We can use a VirtualQuery to see if this is direct or not | todo |
| FW200Test | 63 | Just release the direct memory, which will unmap for us. | todo |
| FW200Test | 72 | In this firmware, direct memory mappings now coalesce. | todo |
| FW200Test | 92 | There should be two observable mappings. | todo |
| FW200Test | 105 | Test making a mapping in between these other mappings. | todo |
| FW200Test | 112 | Mappings all merge together. | todo |
| FW200Test | 118 | Mappings split through mprotect and mtypeprotect can now re-coalesce. | todo |
| FW200Test | 142 | Mappings all merge together. | todo |
| FW200Test | 148 | Areas split from changed type too. | todo |
| FW200Test | 154 | Query memory protection doesn't report the splitting. | todo |
| FW200Test | 160 | sceKernelVirtualQuery does expose the split though. | todo |
| FW200Test | 214 | Areas split due to changed prot | todo |
| FW200Test | 244 | Unmap testing memory. | todo |
| FW200Test | 249 | Test behavior that changed in firmware 2.50, to verify the changes aren't visible here. | todo |
| FW250Test | 257 | Starting with firmware 2.50, several direct memory functions that previously called sys_mmap will now use sceKernelMapDirectMemory2 instead. | todo |
| FW250Test | 258 | This behavior will not be present here, which can be identified through overlapping dmem use. | todo |
| FW250Test | 260 | All of these mappings would fail on firmware 2.50, but succeed on anything below that. | todo |
| FW250Test | 278 | This will unmap all memory tied to this physical area. | todo |

## test_250.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW250Test | 24 | Starting with firmware 2.50, several direct memory functions that previously called sys_mmap will now use sceKernelMapDirectMemory2 instead. | todo |
| FW250Test | 25 | This includes sceKernelMapDirectMemory, sceKernelMapNamedDirectMemory, sceKernelInternalMapDirectMemory, and sceKernelInternalMapNamedDirectMemory. | todo |
| FW250Test | 26 | This behavior is reverted if you use flag MAP_STACK (0x400). | todo |
| FW250Test | 28 | We can abuse the overlapping dmem error case sceKernelMapDirectMemory2 has to identify this. | todo |
| FW250Test | 38 | Then check the flag behavior. If MAP_STACK behaves appropriately, these mappings will all succeed instead. | todo |
| FW250Test | 56 | This will unmap all memory tied to this physical area. | todo |
| FW250Test | 61 | Test behavior that changed in firmware 3.00 | todo |
| FW300Test | 63 | Starting with firmware 3.00, mmap calls with MAP_STACK and address greater than 0xfc00000000 should fail with ENOMEM | todo |
| FW300Test | 64 | Since this homebrew compiles with SDK version 2.50, these differences should not be present here. | todo |
| FW300Test | 90 | mmap calls with MAP_FIXED have a similar error condition, | todo |
| FW300Test | 91 | if addr + size > 0xfc00000000 and SDK version is at or above 3.00, then return EINVAL. | todo |
| FW300Test | 92 | Since this homebrew is compiled with SDK version set to 2.50, these should all succeed. | todo |
| FW300Test | 93 | Note: MAP_SANITIZER does not hit these error returns, that behavior cannot be tested on retail hardware. | todo |
| FW300Test | 125 | mmap calls without MAP_FIXED also have a similar edge case, but with ENOMEM returns. | todo |
| FW300Test | 156 | sys_mmap_dmem also has an equivalent check. | todo |
| FW300Test | 157 | To test this, we need to allocate some direct memory. | todo |

## test_300.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW300Test | 19 | Starting with firmware 3.00, mmap calls with MAP_STACK and address greater than 0xfc00000000 should fail with ENOMEM | todo |
| FW300Test | 41 | mmap calls with MAP_FIXED have a similar error condition, | todo |
| FW300Test | 42 | if addr + size > 0xfc00000000 and SDK version is at or above 3.00, then return EINVAL. | todo |
| FW300Test | 43 | Note: MAP_SANITIZER does not hit these error returns, that behavior cannot be tested on retail hardware. | todo |
| FW300Test | 69 | mmap calls without MAP_FIXED also have a similar edge case, but with ENOMEM returns. | todo |
| FW300Test | 94 | sys_mmap_dmem also has an equivalent check. | todo |
| FW300Test | 95 | To test this, we need to allocate some direct memory. | todo |
| FW300Test | 128 | Test behavior that changed in firmware 3.50 | todo |
| FW350Test | 130 | Starting with firmware 3.50, out of memory errors return ORBIS_KERNEL_ERROR_ENOMEM instead of ORBIS_KERNEL_ERROR_EINVAL. | todo |
| FW350Test | 131 | This should not be visible here, since this homebrew compiles with SDK version 3.00 | todo |
| FW350Test | 141 | Mapped flex mem successfully. | todo |
| FW350Test | 143 | Add the mapping address to addresses, need to unmap later to clean up. | todo |
| FW350Test | 148 | After all these mappings, available flex size should be 0. | todo |
| FW350Test | 154 | sceKernelMmap with MAP_ANON uses the flexible budget. | todo |
| FW350Test | 159 | sceKernelMmap with files also uses the flexible budget. | todo |
| FW350Test | 167 | Make sure download dir file has enough space to mmap before trying. | todo |
| FW350Test | 175 | mmap download dir file. | todo |
| FW350Test | 181 | Unmap memory used by this test | todo |
| FW350Test | 187 | Clear list of addresses | todo |

## test_350.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW350Test | 19 | Starting with firmware 3.50, out of memory errors return ORBIS_KERNEL_ERROR_ENOMEM instead of ORBIS_KERNEL_ERROR_EINVAL. | todo |
| FW350Test | 20 | Start by using the full flexible budget. | todo |
| FW350Test | 30 | Mapped flex mem successfully. | todo |
| FW350Test | 32 | Add the mapping address to addresses, need to unmap later to clean up. | todo |
| FW350Test | 37 | After all these mappings, available flex size should be 0. | todo |
| FW350Test | 43 | sceKernelMmap with MAP_ANON uses the flexible budget. | todo |
| FW350Test | 48 | sceKernelMmap with files also uses the flexible budget. | todo |
| FW350Test | 56 | Make sure download dir file has enough space to mmap before trying. | todo |
| FW350Test | 64 | mmap download dir file. | todo |
| FW350Test | 70 | Unmap memory used by this test | todo |
| FW350Test | 76 | Clear list of addresses | todo |
| FW350Test | 80 | Test behavior that changed in firmware 5.50, the differences will not be present here. | todo |
| FW550Test | 82 | Starting with firmware 5.50, memory mappings performed with different call addresses will now merge. | todo |
| FW550Test | 110 | There should be five observable mappings. | todo |
| FW550Test | 138 | Now call sceKernelSetVirtualRangeName | todo |
| FW550Test | 144 | Because of the older SDK version, the mappings remain separate. | todo |
| FW550Test | 170 | Unmap testing memory. | todo |

## test_550.cpp

| test | line | finding | DmemTest |
|---|---|---|---|
| FW550Test | 18 | Starting with firmware 5.50, memory mappings performed with different call addresses will now merge. | todo |
| FW550Test | 19 | This ignores the anon names, but not other memory names. | todo |
| FW550Test | 61 | There should be five observable mappings. | todo |
| FW550Test | 89 | Now call sceKernelSetVirtualRangeName | todo |
| FW550Test | 95 | Mappings all merge together. | todo |
| FW550Test | 101 | Unmap testing memory. | todo |
| FW550Test | 105 | Additionally, mappings with different anon names also end up merging. | todo |
| FW550Test | 106 | The memory name preserved will always be the name of the newest mapping. | todo |
| FW550Test | 143 | Unmap testing memory. | todo |
| TLOU2Test | 149 | Check for something that came up in fpPS4. | todo |
| TLOU2Test | 150 | I'll probably rewrite this test at some point. | todo |
| TestFileMappings | 259 | Open and prepare file | todo |
| TestFileMappings | 263 | File data starts zeroed after ftruncate | todo |
| TestFileMappings | 266 | Write ones for testing purposes | todo |
| TestFileMappings | 269 | Close file and begin tests. | todo |
| TestFileMappings | 272 | A couple combinations of file perms and mmap prots need testing here. | todo |
| TestFileMappings | 273 | First, we want the basic case. Open file as read-write, mmap as read-write with MAP_SHARED specified. | todo |
| TestFileMappings | 277 | What we should have here is a shared mapping. | todo |
| TestFileMappings | 278 | All writes to the memory will be visible in the file, and all writes to the file will appear in memory. | todo |
| TestFileMappings | 279 | As-is, both memory and file should be full of ones. | todo |
| TestFileMappings | 283 | Now write to the memory. | todo |
| TestFileMappings | 286 | Now reads should show 2s | todo |
| TestFileMappings | 289 | Memory should (naturally) also contain twos | todo |
| TestFileMappings | 292 | Writing directly to the file should update the memory | todo |
| TestFileMappings | 295 | Now reads should (naturally) show 1s | todo |
| TestFileMappings | 298 | Memory should also contain ones | todo |
| TestFileMappings | 301 | Unmap and close file | todo |
| TestFileMappings | 308 | What we should have here is a private mapping, where file contents and memory contents aren't synchronized | todo |
| TestFileMappings | 309 | As-is, both memory and file should be full of ones. | todo |
| TestFileMappings | 313 | Writing directly to the file should not update the memory | todo |
| TestFileMappings | 316 | Now reads should show twos | todo |
| TestFileMappings | 319 | Memory should still contain ones | todo |
| TestFileMappings | 327 | Now memory should contain twos | todo |
| TestFileMappings | 330 | Writes to memory won't update the file either | todo |
| TestFileMappings | 333 | Memory should contain ones | todo |
| TestFileMappings | 336 | File should still contain twos | todo |
| TestFileMappings | 342 | Unmap and close file | todo |
| TestFileMappings | 346 | Open file as write-only. | todo |
| TestFileMappings | 349 | Read-write mmap is impossible. | todo |
| TestFileMappings | 357 | Write-only mmap is impossible, as write-only is treated as read-write. | todo |
| TestFileMappings | 365 | Read-only mmap is impossible | todo |
| TestFileMappings | 373 | No-access mmap is possible. | todo |
| TestFileMappings | 376 | We can protect this to get access to data | todo |
| TestFileMappings | 379 | Now memory should contain ones if it is properly backed | todo |
| TestFileMappings | 382 | Since this is a shared mapping, we can write twos to the file and see the change | todo |
| TestFileMappings | 386 | While we can't see it through file reads, we can still write through memory too | todo |
| TestFileMappings | 396 | The memory should contain the ones written earlier. | todo |
| TestFileMappings | 403 | Re-open and setup the same mapping, this time as private. | todo |
| TestFileMappings | 408 | Now memory should contain ones if it is properly backed | todo |
| TestFileMappings | 411 | Under these circumstances, this private mapping behaves as a copy-on-write mapping. | todo |
| TestFileMappings | 412 | The contents of the memory update appropriately. | todo |
| TestFileMappings | 416 | Writes to memory trigger a copy | todo |
| TestFileMappings | 420 | Verify that the memory is no longer identical | todo |
| TestFileMappings | 424 | Remap to confirm write occurred properly. | todo |
| TestFileMappings | 430 | Write ones and close. | todo |
| TestFileMappings | 435 | Open file as read-only. | todo |
| TestFileMappings | 438 | Read-write mmap is impossible with MAP_SHARED. | todo |
| TestFileMappings | 442 | You can, however, perform a read-write mmap with MAP_PRIVATE or no flags. | todo |
| TestFileMappings | 448 | Same applies to write-only, as write-only is treated as read-write. | todo |
| TestFileMappings | 456 | Read-only mmap is possible with all flag combinations | todo |
| TestFileMappings | 464 | We can take advantage of mprotect again | todo |
| TestFileMappings | 468 | Now we have read-write access to a read-only file. | todo |
| TestFileMappings | 471 | Write to our memory, this will update the file contents. | todo |
| TestFileMappings | 476 | Unmap and close file. | todo |
| TestFileMappings | 480 | Open file as read-only. | todo |
| TestFileMappings | 483 | Make sure MAP_PRIVATE behaves as expected. | todo |
| TestFileMappings | 487 | Now we have read-write access to a read-only file. | todo |
| TestFileMappings | 490 | Write to our memory, this will not update the file contents. | todo |
| TestFileMappings | 494 | Unmap and close file. | todo |

