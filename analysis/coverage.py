#!/usr/bin/env python3
"""Which libkernel functions DmemTest calls, and from which functions/sections.

usage: coverage.py [src/dmemtest.cpp] [path/to/shadPS4/src/core/libraries/kernel/memory.cpp]

With the shadPS4 file, also lists every memory function shadPS4 registers that the test never
calls. Prints Markdown.
"""
import re
import sys

src = sys.argv[1] if len(sys.argv) > 1 else "src/dmemtest.cpp"
shad = sys.argv[2] if len(sys.argv) > 2 else None
text = open(src, encoding="utf-8", errors="replace").read()

# functions declared in the extern "C" block at the top
decl_block = text[text.index('extern "C" {'):text.index("\n}\n", text.index('extern "C" {'))]
declared = sorted(set(re.findall(r"\b(sceKernel\w+|mmap|munmap|mprotect|msync|mlock|munlock|__error)\s*\(", decl_block)))

# map each call to the function it sits in (definitions start at column 0, end with "}" at column 0)
calls = {}
current = None
for line in text.split("\n"):
    m = re.match(r"^(?:__attribute__\(\([^)]*\)\)\s*)?(?:static\s+)?[\w:<>\*\s&]+?\b(\w+)\s*\([^;]*\)\s*(?:const\s*)?\{\s*$", line)
    if m and not line.startswith(" "):
        current = m.group(1)
        continue
    if line == "}":
        current = None
        continue
    if current is None:
        continue
    for fn in declared:
        if re.search(r"(?<![\w.])" + re.escape(fn) + r"\s*\(", line):
            calls.setdefault(fn, set()).add(current)

print("# DmemTest coverage\n")
print(f"Generated from `{src}`.\n")
print("| function | called from |")
print("|---|---|")
for fn in declared:
    where = sorted(calls.get(fn, []))
    print(f"| `{fn}` | {', '.join(where) if where else '**never called**'} |")

if shad:
    stext = open(shad, encoding="utf-8", errors="replace").read()
    regs = re.findall(r"LIB_FUNCTION(?:_WITH_CALLER)?\(\s*\"[^\"]+\",\s*\"libkernel\",\s*\d+,\s*\"[^\"]+\",\s*(\w+)\s*\)", stext)
    names = sorted(set(re.sub(r"FromGuest$", "", r) for r in regs))
    alias = {"MapFlexibleMemory": "sceKernelMapFlexibleMemory", "MapDirectMemory": "sceKernelMapDirectMemory",
             "Mmap": "sceKernelMmap", "MemoryPoolReserve": "sceKernelMemoryPoolReserve",
             "MapNamedDirectMemory": "sceKernelMapNamedDirectMemory",
             "MapNamedFlexibleMemory": "sceKernelMapNamedFlexibleMemory",
             "ReserveVirtualRange": "sceKernelReserveVirtualRange"}
    missing = []
    for n in names:
        n2 = alias.get(n, n)
        posix = n2[6:] if n2.startswith("posix_") else None
        if n2 not in calls and (posix is None or posix not in calls):
            missing.append(n)
    print(f"\n## Registered in shadPS4 (`{shad}`) but never called by the test\n")
    for n in missing:
        print(f"- `{n}`")
    if not missing:
        print("- none")
