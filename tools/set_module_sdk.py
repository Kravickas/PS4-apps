#!/usr/bin/env python3
"""Copy a module (.prx, fake SELF) with the SDK version in its module parameter changed.

The module parameter is three 64-bit words: size 0x18, magic 0x13C13F4BF, SDK version. In OpenOrbis'
libc.prx and libSceFios2.prx it is SDK 1.00 (0x1000051), stored twice: two LOAD segments of the same
address (0xc000) hold it, which the SCE_MODULE_PARAM segment points at. Every copy must hold the same
value and every copy is changed; nothing else changes. HEN does not check a fake SELF's digest.

usage: set_module_sdk.py in.prx out.prx 0x12008001
"""
import struct
import sys


def main() -> int:
    src, dst, value = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
    data = bytearray(open(src, "rb").read())
    head = struct.pack("<QQ", 0x18, 0x13C13F4BF)
    found = [i for i in range(len(data) - 24) if data[i:i + 16] == head]
    olds = {struct.unpack_from("<Q", data, i + 16)[0] for i in found}
    if not found or len(olds) != 1:
        sys.exit(f"{src}: module parameter found {len(found)} times with values {sorted(olds)}")
    for i in found:
        struct.pack_into("<Q", data, i + 16, value)
    open(dst, "wb").write(data)
    print(f"{dst}: module parameter SDK version {olds.pop():#x} -> {value:#x} ({len(found)} copies)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
