#!/usr/bin/env python3
"""Which result lines change with the process SDK version.

Give the result files of one homebrew built at different SDK versions (each run prints
"SDK version 0x..." below its title). The files are ordered by that version; for every line (keyed
by its ID and how many times that ID came before it) the report names each SDK version at which the
line differs from the version before, with both texts. Lines that never change are only counted.

usage: sdk_sweep.py results1.txt results2.txt ...
"""
import collections
import re
import sys


def load(path):
    sdk = None
    lines = collections.OrderedDict()
    seen = collections.Counter()
    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.rstrip("\r\n")
        m = re.match(r"SDK version (0x[0-9A-Fa-f]+)", line)
        if m and sdk is None:
            sdk = int(m.group(1), 16)
            continue
        if not line.strip() or line.startswith("====="):
            continue
        key = line.split()[0]
        seen[key] += 1
        lines[(key, seen[key])] = line
    if sdk is None:
        sys.exit(f"{path}: no 'SDK version' line")
    return sdk, lines


def main():
    runs = sorted((load(p) + (p,) for p in sys.argv[1:]), key=lambda r: r[0])
    if len(runs) < 2:
        sys.exit("need at least two result files")
    print("runs:", ", ".join(f"{sdk:#010x} ({path})" for sdk, _, path in runs))
    keys = list(runs[0][1])
    for _, lines, _ in runs[1:]:
        keys += [k for k in lines if k not in keys]
    stable = 0
    for key in keys:
        changes = []
        prev = runs[0][1].get(key, "(missing)")
        for sdk, lines, _ in runs[1:]:
            cur = lines.get(key, "(missing)")
            if cur != prev:
                changes.append((sdk, prev, cur))
            prev = cur
        if not changes:
            stable += 1
            continue
        print(f"\n{key[0]}#{key[1]}")
        for sdk, old, new in changes:
            print(f"  from {sdk:#010x}:\n    before: {old[:220]}\n    now   : {new[:220]}")
    print(f"\n{stable} lines the same at every SDK version, {len(keys) - stable} change")


if __name__ == "__main__":
    main()
