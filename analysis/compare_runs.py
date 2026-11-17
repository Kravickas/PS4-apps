#!/usr/bin/env python3
"""Compare DmemTest results of two systems (round 128 and later), line by line.

usage: compare_runs.py <PS4 file or folder> <other file or folder> [--layout]

A folder is compared file by file (dmem_results_<SDK>.txt in both).

Kinds of line in a results file:
  result  starts with a probe ID. Behaviour only: it must be the same on both systems.
  LY      layout notes of the probe before it (addresses, sizes, module lists). Left out of the
          verdict; shown with --layout.
  HF      a call whose result the probe does not print failed: "HF <code> <call>". Both runs must
          have the same ones; an HF line on one side only means a set-up or clean-up step went
          differently there, and the result lines after it may not be the same case.
  SB TR PS  read-back, pool trace and pool state lines. Compared as sets (as compare_results.py).
  ==      section headers.

Nothing is interpreted: for every result line that is not identical, both lines are printed and
the blank-separated fields that differ are marked. "@f", "@u", "@s" in a line is what the probe
saw at its address before it ran (free, used, in the system library area); when those differ the
two systems did not run the same case, and the line is listed as "different case".

Checks made before any comparison (a failed check is reported first and the verdict is not "same"):
  - both files have the same ROUND line
  - both files have the DONE line
  - the COUNT line of each file agrees with the lines in that file
  - both files have the same probe IDs, the same number of times
"""
import collections
import os
import re
import sys

OTHER = ("SB ", "TR ", "PS ", "LY ", "HF ")


def load(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return [l.rstrip("\r\n") for l in f]


def is_result(l):
    return bool(l) and "A" <= l[0] <= "Z" and not l.startswith(OTHER)


def parse(lines):
    """Lines up to DONE: results keyed by (ID, occurrence), layout notes by ID, checks."""
    info = {"round": None, "done": False, "count": None, "n_result": 0, "n_layout": 0}
    results = collections.OrderedDict()
    layout = collections.defaultdict(list)
    seen = collections.Counter()
    for l in lines:
        if l == "===== DONE =====":
            info["done"] = True
            break
        if l.startswith("ROUND "):
            info["round"] = l
        m = re.match(r"COUNT result lines=(\d+) layout lines=(\d+)$", l)
        if m:
            info["count"] = (int(m.group(1)), int(m.group(2)))
            continue
        if l.startswith("LY "):
            info["n_layout"] += 1
            parts = l.split(None, 2)
            layout[parts[1] if len(parts) > 1 else "?"].append(parts[2] if len(parts) > 2 else "")
            continue
        if not is_result(l):
            continue
        info["n_result"] += 1
        key = l.split()[0]
        # "PX1 starting" and the like: markers, counted but not compared as results
        seen[key] += 1
        results[(key, seen[key])] = l
    return info, results, layout


def mark(a, b):
    """The fields of a and b, and a line with ^ under every field of a that differs."""
    fa, fb = a.split(), b.split()
    out = []
    pos = 0
    for i, tok in enumerate(fa):
        start = a.index(tok, pos)
        pos = start + len(tok)
        differs = i >= len(fb) or fb[i] != tok
        out.append((start, len(tok), differs))
    line = [" "] * len(a)
    for start, n, differs in out:
        if differs:
            for k in range(start, start + n):
                line[k] = "^"
    return "".join(line).rstrip()


def different_case(a, b):
    """True when what the probes saw at their addresses before they ran (the @ letters) differs."""
    return re.findall(r"@[fus]", a) != re.findall(r"@[fus]", b)


def counters_only(a, b):
    """True when every differing field is a pool counter field (stats=..., a/b/c/d)."""
    fa, fb = a.split(), b.split()
    if len(fa) != len(fb):
        return False
    d = [(x, y) for x, y in zip(fa, fb) if x != y]
    pat = re.compile(r"^(stats=)?-?[0-9A-F]+(/-?[0-9A-F]+){3}$")
    return bool(d) and all(pat.match(x) and pat.match(y) for x, y in d)


def compare(pa, pb, show_layout):
    A, B = load(pa), load(pb)
    ia, ra, la = parse(A)
    ib, rb, lb = parse(B)
    problems = []
    if ia["round"] != ib["round"] or ia["round"] is None:
        problems.append(f"ROUND differs or is missing: {ia['round']!r} | {ib['round']!r}")
    for name, i in (("first", ia), ("second", ib)):
        if not i["done"]:
            problems.append(f"{name} file has no DONE line: the run did not finish")
        if i["count"] is None:
            problems.append(f"{name} file has no COUNT line")
        elif i["count"] != (i["n_result"], i["n_layout"]):
            problems.append(f"{name} file: COUNT says {i['count']}, the file has "
                            f"{(i['n_result'], i['n_layout'])}: lines are missing or were added")
    only_a = [k for k in ra if k not in rb]
    only_b = [k for k in rb if k not in ra]
    for k in only_a:
        problems.append(f"only in the first file: {ra[k]}")
    for k in only_b:
        problems.append(f"only in the second file: {rb[k]}")

    same = 0
    diffs, cases = [], []
    for k, a in ra.items():
        if k not in rb:
            continue
        b = rb[k]
        if a == b:
            same += 1
        elif different_case(a, b):
            cases.append((k, a, b))
        else:
            diffs.append((k, a, b))

    print(f"== {os.path.basename(pa)}  |  {os.path.basename(pb)}")
    for p in problems:
        print(f"  CHECK FAILED: {p}")
    print(f"  result lines: {same} same, {len(diffs)} differ, {len(cases)} ran a different case")
    for title, items in (("differ, in the order they ran", diffs),
                         ("different case (what was at the address differs)", cases)):
        if items:
            print(f"\n  -- {title}")
        for k, a, b in items:
            print(f"  1: {a}")
            print(f"  2: {b}")
            print(f"     {mark(a, b)}")
            if counters_only(a, b):
                print("     (only pool counters differ: they are running totals, so this can "
                      "follow from an earlier difference; look at the first such line)")
            if show_layout:
                for l in la.get(k[0], []):
                    print(f"     LY 1: {l}")
                for l in lb.get(k[0], []):
                    print(f"     LY 2: {l}")
    hf_a = collections.Counter(l for l in A if l.startswith("HF "))
    hf_b = collections.Counter(l for l in B if l.startswith("HF "))
    hf_differs = hf_a != hf_b
    print(f"\n  -- HF lines (failed calls the probes do not print): {sum(hf_a.values())} | "
          f"{sum(hf_b.values())}{'  DIFFERENT' if hf_differs else '  same'}")
    for l in (hf_a - hf_b):
        print(f"  1 only: {l}")
    for l in (hf_b - hf_a):
        print(f"  2 only: {l}")
    for tag in ("SB ", "PS ", "TR "):
        ca = collections.Counter(l for l in A if l.startswith(tag))
        cb = collections.Counter(l for l in B if l.startswith(tag))
        oa, ob = ca - cb, cb - ca
        if oa or ob:
            print(f"\n  -- {tag.strip()} lines on one side only: {sum(oa.values())} | "
                  f"{sum(ob.values())}")
            for l in list(oa)[:40]:
                print(f"  1 only: {l}")
            for l in list(ob)[:40]:
                print(f"  2 only: {l}")
    if show_layout:
        print("\n  -- layout notes that differ (not part of the verdict)")
        for key in la:
            if la[key] != lb.get(key):
                for l in la[key]:
                    print(f"  LY 1 {key}: {l}")
                for l in lb.get(key, []):
                    print(f"  LY 2 {key}: {l}")
    ok = not problems and not diffs and not cases and not hf_differs
    print(f"\n  VERDICT: {'same behaviour' if ok else 'NOT the same'}")
    return ok, {k[0] for k, _, _ in diffs}, {k[0] for k, _, _ in cases}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_layout = "--layout" in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    a, b = args
    if os.path.isdir(a) and os.path.isdir(b):
        names = sorted(n for n in os.listdir(a) if re.match(r"dmem_results_.*\.txt$", n))
        table = collections.defaultdict(list)
        all_ok = True
        for n in names:
            if not os.path.exists(os.path.join(b, n)):
                print(f"== {n}: missing in the second folder")
                all_ok = False
                continue
            ok, d, c = compare(os.path.join(a, n), os.path.join(b, n), show_layout)
            all_ok = all_ok and ok
            sdk = n[len("dmem_results_"):-4]
            for i in d:
                table[i].append(sdk)
            for i in c:
                table[i].append(sdk + "(case)")
            print()
        print("== probe IDs that differ, by SDK version")
        for i in sorted(table):
            print(f"  {i:6s} {' '.join(table[i])}")
        print(f"\n== OVERALL: {'same behaviour at every SDK version' if all_ok else 'NOT the same'}")
    else:
        compare(a, b, show_layout)


if __name__ == "__main__":
    main()
