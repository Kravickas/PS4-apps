#!/usr/bin/env python3
"""Generates FINDINGS.md: every finding comment of the OpenOrbis-Unit-Tests memory_test homebrew,
by file and line, with the DmemTest probe that retests it (COVERAGE below), or "todo".

usage: findings.py path/to/OpenOrbis-Unit-Tests/tests/code/memory_test/code > analysis/FINDINGS.md
"""
import os
import re
import sys

FILES = ["test_100.cpp", "test_170.cpp", "test_200.cpp", "test_250.cpp", "test_300.cpp", "test_350.cpp",
         "test_550.cpp"]

# (file, first line, last line, DmemTest probes) - which probes retest the findings in that range
COVERAGE = [
    ("test_100.cpp", 20, 20, "NP1"),
    ("test_100.cpp", 52, 53, "K1 (default search base)"),
    ("test_100.cpp", 54, 54, "not retested (their note: not possible to test from a homebrew)"),
    ("test_100.cpp", 55, 55, "not retested (SDK below 1.70)"),
    ("test_100.cpp", 69, 72, "FW3 (DmemCompat 0x400 on MapDirectMemory)"),
    ("test_100.cpp", 86, 87, "EX2, BT6"),
    ("test_100.cpp", 24, 27, "SF1"), ("test_100.cpp", 34, 36, "BT4"), ("test_100.cpp", 42, 43, "BT3"),
    ("test_100.cpp", 58, 59, "BT5"), ("test_100.cpp", 75, 78, "BT6, EX2"), ("test_100.cpp", 90, 98, "setup"),
    ("test_100.cpp", 99, 110, "BT7 (alignment); address/length/phys alignment: todo"),
    ("test_100.cpp", 111, 118, "BT7"), ("test_100.cpp", 119, 129, "setup"), ("test_100.cpp", 130, 141, "BT1, BT2"),
    ("test_100.cpp", 142, 151, "RV2 (length); address alignment: todo"), ("test_100.cpp", 152, 165, "BT2"),
    ("test_100.cpp", 166, 177, "BT9 (bit 21, sanitizer)"), ("test_100.cpp", 178, 205, "SF2"),
    ("test_100.cpp", 206, 256, "SF3"), ("test_100.cpp", 257, 268, "SF4"), ("test_100.cpp", 269, 286, "NU6"),
    ("test_100.cpp", 287, 418, "MG, MW, MF (file sizes and offsets); execute on a file mapping: todo"),
    ("test_100.cpp", 419, 447, "SF4"),
    ("test_100.cpp", 448, 483, "BT9 (null+fixed), K, LA (fixed, overwrite); no-overwrite over a partly mapped range: todo"),
    ("test_100.cpp", 484, 547, "K, HB, VQ10 (search base, hint, upward search, EACCES on free)"),
    ("test_100.cpp", 548, 933, "SF5"), ("test_100.cpp", 934, 950, "EX2, BT6"), ("test_100.cpp", 951, 1014, "SF6"),
    ("test_100.cpp", 1015, 1022, "SF7"), ("test_100.cpp", 1023, 1035, "SF8"), ("test_100.cpp", 1036, 1043, "SF7"),
    ("test_100.cpp", 1044, 1055, "BT9"),
    ("test_100.cpp", 1056, 1101, "DF1"), ("test_100.cpp", 1102, 1160, "DF2"),
    ("test_100.cpp", 1161, 1191, "DF3 (no writes into /dev/gc: Shared append not retested)"),
    ("test_100.cpp", 1192, 1207, "not retested (writes into /dev/gc)"), ("test_100.cpp", 1208, 1214, "DF3"),
    ("test_100.cpp", 1215, 1269, "DF4"),
    ("test_100.cpp", 1270, 1288, "setup (known GnmDriver dmem area: layout)"),
    ("test_100.cpp", 1289, 1297, "H5, T12 (null outputs)"), ("test_100.cpp", 1298, 1306, "AV1, BT28"),
    ("test_100.cpp", 1307, 1337, "AV2"), ("test_100.cpp", 1338, 1349, "AV2"), ("test_100.cpp", 1350, 1356, "AV1"),
    ("test_100.cpp", 1357, 1387, "AV3"),
    ("test_100.cpp", 1388, 1403, "AL3"), ("test_100.cpp", 1404, 1433, "AL1, BT22, BT23"),
    ("test_100.cpp", 1434, 1445, "AL2"),
    ("test_100.cpp", 1446, 1448, "not retested (null output crashes the application)"),
    ("test_100.cpp", 1449, 1513, "AL3"),
    ("test_100.cpp", 1514, 1518, "not retested (misaligned search start crashes the console)"),
    ("test_100.cpp", 1519, 1527, "NU4, C/D sections (alignment)"),
    ("test_100.cpp", 1528, 1581, "C, D sections (checked/unchecked release over free and mixed ranges)"),
    ("test_100.cpp", 1582, 1602, "B4, B8 (release unmaps)"),
    ("test_100.cpp", 1603, 1666, "FM1"), ("test_100.cpp", 1667, 1734, "FM2"),
    ("test_100.cpp", 1735, 1765, "setup (random file contents)"),
    ("test_100.cpp", 1766, 1840, "MG, MW (zero bytes past EOF), FM2 (unaligned offset)"),
    ("test_100.cpp", 1841, 1842, "FM3"),
    ("test_100.cpp", 1843, 1843, "not retested (shared writable mapping of a read-only file crashes the PS4)"),
    ("test_100.cpp", 1844, 1868, "FM3"), ("test_100.cpp", 1869, 1894, "MF9"),
    ("test_100.cpp", 1895, 1919, "DF4 (budget used up)"), ("test_100.cpp", 1920, 1935, "FX1"),
    ("test_100.cpp", 1936, 1957, "not retested (download dir needs permissions)"),
    ("test_100.cpp", 1958, 1972, "FX1"), ("test_100.cpp", 1973, 1987, "DF4"),
    ("test_100.cpp", 1988, 2030, "FX2"), ("test_100.cpp", 2031, 2067, "FX3"),
    ("test_100.cpp", 2068, 2112, "FX1, FL4, FL5"),
    ("test_100.cpp", 2113, 2137, "not retested (download dir needs permissions)"),
    ("test_100.cpp", 2138, 2148, "E11, BP9 (unallocated), DT1"), ("test_100.cpp", 2149, 2193, "DT1"),
    ("test_100.cpp", 2194, 2226, "DT2"), ("test_100.cpp", 2227, 2263, "DT3, B4"),
    ("test_100.cpp", 2264, 2294, "DT4"), ("test_100.cpp", 2295, 2369, "DT5, MS1"),
    ("test_100.cpp", 2370, 2430, "DT6"), ("test_100.cpp", 2431, 2491, "DT8, DT7 (checked release)"),
    ("test_100.cpp", 2492, 2539, "DT7"), ("test_100.cpp", 2540, 2609, "DT8"),
    ("test_100.cpp", 2610, 2674, "setup (one calling address: DmemTest's SB call site)"),
    ("test_100.cpp", 2675, 2855, "CO1"), ("test_100.cpp", 2856, 2878, "CO2"),
    ("test_100.cpp", 2879, 2879, "NP1"),
    ("test_100.cpp", 2880, 2926, "CO2"), ("test_100.cpp", 2927, 2946, "CO3"),
    ("test_100.cpp", 2947, 2947, "NP1"),
    ("test_100.cpp", 2948, 2985, "CO3"), ("test_100.cpp", 2986, 3160, "CO4"),
    ("test_100.cpp", 3161, 3342, "CO5 (a /data file instead of download0)"), ("test_100.cpp", 3343, 3391, "CO6"),
    ("test_100.cpp", 3392, 3440, "CO7"), ("test_100.cpp", 3441, 3567, "CO8"),
    ("test_100.cpp", 3568, 3588, "setup"), ("test_100.cpp", 3589, 3655, "NU2, SB (read added to write)"),
    ("test_100.cpp", 3656, 3662, "setup"), ("test_100.cpp", 3663, 3688, "PT1, RV4"),
    ("test_100.cpp", 3689, 3691, "not retested (GPU protection on reserved memory hangs the PS4)"),
    ("test_100.cpp", 3692, 3778, "PT1"), ("test_100.cpp", 3779, 3811, "EX3, RG3"),
    ("test_100.cpp", 3812, 3851, "EX1, RG1, RG2 (refused at SDK 4.508)"), ("test_100.cpp", 3852, 3898, "PT3"),
    ("test_100.cpp", 3899, 3956, "PT2"), ("test_100.cpp", 3957, 3995, "NU16, BT11, SB"),
    ("test_100.cpp", 3996, 4045, "PT4, NU8, BT11"), ("test_100.cpp", 4046, 4053, "NU2"),
    ("test_100.cpp", 4054, 4065, "BT14, BT15"), ("test_100.cpp", 4066, 4086, "setup"),
    ("test_100.cpp", 4087, 4152, "NU9, TP1, SB (type and protection read back)"),
    ("test_100.cpp", 4153, 4182, "BT14, BT15"), ("test_100.cpp", 4183, 4204, "TP1"),
    ("test_100.cpp", 4205, 4295, "TP2"), ("test_100.cpp", 4296, 4340, "TP3, NU7"),
    ("test_170.cpp", 1, 15, "setup"), ("test_170.cpp", 16, 58, "FW1 (null+fixed; pool 1 = direct)"),
    ("test_170.cpp", 59, 115, "setup"), ("test_170.cpp", 116, 189, "CO4 (direct coalescing at our SDK)"),
    ("test_170.cpp", 190, 400, "FW2"),
    ("test_200.cpp", 1, 71, "setup"), ("test_200.cpp", 72, 117, "CO4"), ("test_200.cpp", 118, 248, "FW2"),
    ("test_200.cpp", 249, 400, "FW3, DT2"),
    ("test_250.cpp", 1, 60, "FW3"), ("test_250.cpp", 61, 92, "FW4, SF3"), ("test_250.cpp", 93, 93, "BT9 (sanitizer)"),
    ("test_250.cpp", 94, 400, "FW4"),
    ("test_300.cpp", 1, 40, "SF3, FW4"), ("test_300.cpp", 41, 42, "FW4"), ("test_300.cpp", 43, 43, "BT9 (sanitizer)"),
    ("test_300.cpp", 44, 127, "FW4"), ("test_300.cpp", 128, 166, "FW5"),
    ("test_300.cpp", 167, 180, "not retested (download dir needs permissions)"), ("test_300.cpp", 181, 400, "FW5"),
    ("test_350.cpp", 1, 55, "FW5"), ("test_350.cpp", 56, 69, "not retested (download dir needs permissions)"),
    ("test_350.cpp", 70, 79, "FW5"), ("test_350.cpp", 80, 400, "FW6"),
    ("test_550.cpp", 1, 147, "FW6 (5.50 itself: needs a 5.50 build of DmemTest)"),
    ("test_550.cpp", 148, 241, "TL1 (64 MiB instead of 4 GiB)"),
    ("test_550.cpp", 242, 307, "TF1"), ("test_550.cpp", 308, 345, "TF2"), ("test_550.cpp", 346, 434, "TF3"),
    ("test_550.cpp", 435, 463, "TF4"),
    ("test_550.cpp", 464, 475, "TF4 (mprotect only: no write through a shared read-only mapping)"),
    ("test_550.cpp", 476, 600, "TF4"),
]


def is_covered(st):
    """A finding counts as covered only when a probe retests it."""
    return not (st.startswith("todo") or st.startswith("not retested"))


def status(f, n):
    for cf, a, b, s in COVERAGE:
        if cf == f and a <= n <= b:
            return s
    return "todo"


src = sys.argv[1]
rows_by_file = []
total = covered = 0
for f in FILES:
    lines = open(os.path.join(src, f), encoding="utf-8", errors="replace").read().split("\n")
    test = None
    rows = []
    for i, l in enumerate(lines, 1):
        m = re.match(r"^TEST\(\w+,\s*(\w+)\)", l)
        if m:
            test = m.group(1)
            continue
        c = re.match(r"^\s*(?://|\*)\s?(.*)$", l)
        if not c or test is None:
            continue
        t = c.group(1).strip()
        if len(t) < 20 or t.startswith("/"):
            continue
        st = status(f, i)
        total += 1
        covered += is_covered(st)
        rows.append((test, i, t.replace("|", "\\|"), st))
    rows_by_file.append((f, rows))

print("# Findings of other homebrews, and the DmemTest probes that retest them\n")
print("Every finding comment of the OpenOrbis-Unit-Tests memory_test homebrew (tests/code/memory_test/code),")
print("by file and line, with the DmemTest probe that checks it on both machines, or \"todo\".")
print("The devs' blockpool homebrew is retested by BP, dmem_regprig by RG. Generated by analysis/findings.py.\n")
print(f"{covered} of {total} findings covered so far.\n")
print("| file | test | covered | findings |")
print("|---|---|---|---|")
for f, rows in rows_by_file:
    per_test = {}
    for t, i, txt, st in rows:
        c, n = per_test.get(t, (0, 0))
        per_test[t] = (c + is_covered(st), n + 1)
    for t, (c, n) in per_test.items():
        print(f"| {f} | {t} | {c} | {n} |")
print()
for f, rows in rows_by_file:
    if not rows:
        continue
    print(f"## {f}\n")
    print("| test | line | finding | DmemTest |")
    print("|---|---|---|---|")
    for t, i, txt, st in rows:
        print(f"| {t} | {i} | {txt} | {st} |")
    print()
