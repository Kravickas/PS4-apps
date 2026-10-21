#!/usr/bin/env python3
"""Compare a PS4 and a shadPS4 dmem_results.txt.

usage: compare_results.py "ps4 dmem_results.txt" "shadps4 dmem_results.txt"

Reports where each run ended, per-section matching/differing line counts, every differing line,
every SB (silent behaviour) line found on one side only, and the first TR difference per section.
"""
import collections
import difflib
import sys


def load(path):
    return [l.rstrip("\r\n") for l in open(path, encoding="utf-8", errors="replace")]


ps4, shad = load(sys.argv[1]), load(sys.argv[2])


def sections(lines):
    out = collections.OrderedDict()
    cur = "(start)"
    for l in lines:
        if l.startswith("== "):
            cur = l[3:].split()[0]
        out.setdefault(cur, []).append(l)
    return out


print("== run end")
print(f"  PS4    : {len(ps4)} lines, last: {ps4[-1] if ps4 else ''}")
print(f"  shadPS4: {len(shad)} lines, last: {shad[-1] if shad else ''}")
if shad and shad[-1] != "===== DONE =====":
    print("  shadPS4 did not finish: last line above is where it stopped")

P, S = sections(ps4), sections(shad)
print("\n== per section (lines starting with an ID; SB/TR/PS listed separately)")
for sec in P:
    pl = [l for l in P[sec] if not l.startswith(("SB ", "TR ", "PS ", "== "))]
    sl = [l for l in S.get(sec, []) if not l.startswith(("SB ", "TR ", "PS ", "== "))]
    same = sum(1 for a, b in zip(pl, sl) if a == b)
    diff = [(a, b) for a, b in zip(pl, sl) if a != b]
    missing = len(pl) - len(sl) if len(pl) > len(sl) else 0
    flag = "" if not diff and not missing else "  <--"
    print(f"  {sec:10s} same {same:4d}  differ {len(diff):3d}  not reached {missing:3d}{flag}")
    for a, b in diff:
        print(f"      PS4 : {a}")
        print(f"      shad: {b}")

for tag in ("SB ", "PS "):
    pc = collections.Counter(l for l in ps4 if l.startswith(tag))
    sc = collections.Counter(l for l in shad if l.startswith(tag))
    only_p, only_s = pc - sc, sc - pc
    print(f"\n== {tag.strip()} lines on one side only: PS4 {sum(only_p.values())}, shadPS4 {sum(only_s.values())}")
    for l in only_p:
        print(f"  PS4 only : {l}")
    for l in only_s:
        print(f"  shad only: {l}")

pt = [l for l in ps4 if l.startswith("TR ")]
st = [l for l in shad if l.startswith("TR ")]
print(f"\n== TR (pool trace): PS4 {len(pt)} lines, shadPS4 {len(st)} lines")
for op, i1, i2, j1, j2 in difflib.SequenceMatcher(a=pt, b=st, autojunk=False).get_opcodes():
    if op == "equal":
        continue
    for l in pt[i1:i2][:4]:
        print(f"  PS4 : {l}")
    for l in st[j1:j2][:4]:
        print(f"  shad: {l}")
    print("  ...")
