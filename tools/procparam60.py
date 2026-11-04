#!/usr/bin/env python3
"""Copy OpenOrbis' crt1.o with the process parameter in the layout of newer SDKs.

OpenOrbis' .data.sce_process_param is 0x50 bytes with 3 entries; Gran Turismo 7 (SDK 11.00) has 0x60
bytes with 5 entries, the two more fields (sceProcessPreloadEnabled and one unknown) 0. This writes
the section as 0x60 bytes: size 0x60, entry count 5, the first 0x50 bytes otherwise unchanged
(relocations at +0x38, +0x40, +0x48 still apply), then 16 zero bytes. The new data is appended to the
file and the section header pointed at it; nothing else in the object changes.

usage: procparam60.py crt1.o crt1_pp60.o
"""
import struct
import sys

SECTION = b".data.sce_process_param"


def main() -> int:
    src, dst = sys.argv[1], sys.argv[2]
    data = bytearray(open(src, "rb").read())
    if data[:4] != b"\x7fELF" or data[4] != 2 or struct.unpack_from("<H", data, 0x10)[0] != 1:
        sys.exit("not an ELF64 relocatable object")
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    strtab = struct.unpack_from("<Q", data, shoff + shstrndx * shentsize + 0x18)[0]
    for i in range(shnum):
        sh = shoff + i * shentsize
        name = bytes(data[strtab + struct.unpack_from("<I", data, sh)[0]:]).split(b"\0", 1)[0]
        if name != SECTION:
            continue
        off, size = struct.unpack_from("<QQ", data, sh + 0x18)
        old = bytes(data[off:off + size])
        osize, magic, entries = struct.unpack_from("<QII", old, 0)
        if size != 0x50 or osize != 0x50 or magic != 0x4942524F or entries != 3:
            sys.exit(f"unexpected process parameter: size {size:#x}/{osize:#x} magic {magic:#x} "
                     f"entries {entries}")
        new = bytearray(old) + bytes(0x10)
        struct.pack_into("<QII", new, 0, 0x60, 0x4942524F, 5)
        while len(data) % 8:
            data.append(0)
        new_off = len(data)
        data += new
        struct.pack_into("<QQ", data, sh + 0x18, new_off, 0x60)
        open(dst, "wb").write(data)
        print(f"{dst}: process parameter 0x50/3 entries -> 0x60/5 entries")
        return 0
    sys.exit(f"section {SECTION.decode()} not found")


if __name__ == "__main__":
    sys.exit(main())
