"""Summarises flipprobe.log (FLIP_PROBE in src/main.c), complete or cut short by a crash.

usage: python tools/flipprobe_summary.py flipprobe.log [--list N]

Per submit, from the raw SUB and EV records:
  gpu_done  - the first poller event at or after the submit whose progress word shows the GPU
              reached it (EOP flips only);
  shown     - the first poller event at or after the submit whose flip status reports its flip
              arg, i.e. the flip on screen;
  lab_at_shown, cleared - its buffer's label at `shown`, and whether that label went 1 -> 0
              between gpu_done (immediate flips: the submit returning) and `shown`.
Per phase:
  shown_lab0 - flips whose buffer label was 0 when first reported on screen: the state shadPS4
               asserts cannot happen at an EOP flip's interrupt;
  cleared    - flips whose label went 1 -> 0 between the GPU finishing and the screen;
  race       - EOP submits the GPU finished before the previous flip reached the screen;
  imm_lab_set - immediate flips whose label read 1 right after sceVideoOutSubmitFlip returned;
  timing     - submit -> GPU done and GPU done -> screen, microseconds (p50 / max);
  gpu frame  - GPU time of the frame from its two GPU clock stamps, microseconds (p50 / max), using
               the GPU clock rate measured against the TSC over the run.
G is also broken down by GPU load (1 MB copies) and by max pending. Then the marker block as
libSceGnmDriver patched it, once per distinct pattern, and how the log ends.
"""
import bisect
import re
import sys
from array import array
from collections import OrderedDict, defaultdict

KV = re.compile(r"(\w+)=(\S+)")


def num(v):
    try:
        return int(v, 0)
    except ValueError:
        return v


def parse(path):
    cfg, phases, subs, tails = {}, OrderedDict(), {}, defaultdict(dict)
    ev_t, ev_l0, ev_l1, ev_p, ev_a = (array("Q"), array("Q"), array("Q"), array("q"), array("q"))
    lost, last_line, ended = [], "", False
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\r\n")
        if not line:
            continue
        last_line = line
        if line.startswith("EV "):
            d = dict(KV.findall(line))
            ev_t.append(int(d["tsc"]))
            ev_l0.append(int(d["lab0"], 16) & 0xFFFFFFFFFFFFFFFF)
            ev_l1.append(int(d["lab1"], 16) & 0xFFFFFFFFFFFFFFFF)
            ev_p.append(int(d["prog"]))
            ev_a.append(int(d["farg"]))
        elif line.startswith("SUB "):
            name = line.split(" phase=")[1].split(" eop=")[0]
            d = {k: num(v) for k, v in KV.findall(line.replace(" phase=" + name, ""))}
            d["phase"] = name
            subs[d["idx"]] = d
        elif line.startswith("TAIL_PRE_SAME") or line.startswith("TAIL_POST_SAME"):
            kind = "pre" if line.startswith("TAIL_PRE") else "post"
            idx = int(re.search(r"submit=(\d+)", line).group(1))
            ref = int(re.search(r" as=(\d+)", line).group(1))
            tails[idx][kind] = tails[ref].get(kind, [])
        elif line.startswith("TAIL_PRE") or line.startswith("TAIL_POST"):
            kind = "pre" if line.startswith("TAIL_PRE") else "post"
            idx = int(re.search(r"submit=(\d+)", line).group(1))
            parts = line.split(" : ")
            tails[idx][kind] = parts[1].split() if len(parts) > 1 else []
        elif line.startswith("CFG"):
            cfg = {k: num(v) for k, v in KV.findall(line)}
        elif line.startswith("PHASE_CFG"):
            d = dict(KV.findall(line))
            name = line.split(" name=")[1].split(" kind=")[0]
            phases[name] = {"kind": int(d["kind"]), "max_pending": int(d["max_pending"])}
        elif line.startswith("EV_LOST") or line.startswith("QEV_LOST"):
            lost.append(line)
        elif line.startswith("=== FLIP_PROBE end"):
            ended = True
    return cfg, phases, subs, tails, (ev_t, ev_l0, ev_l1, ev_p, ev_a), lost, last_line, ended


def analyse(subs, ev):
    ev_t, ev_l0, ev_l1, ev_p, ev_a = ev
    n = len(ev_t)
    out = {}
    for idx in sorted(subs):
        s = subs[idx]
        if s.get("t_sub0", 0) == 0:
            continue
        lab = ev_l1 if s["bi"] == 1 else ev_l0
        lo = bisect.bisect_left(ev_t, s["t_sub0"])
        gpu_i = shown_i = -1
        i = lo
        while i < n and (shown_i < 0 or (s["eop"] and gpu_i < 0)):
            if s["eop"] and gpu_i < 0 and ev_p[i] >= idx + 1:
                gpu_i = i
            if shown_i < 0 and ev_a[i] == idx:
                shown_i = i
            i += 1
            if i - lo > 200000:
                break
        a = {"gpu_i": gpu_i, "shown_i": shown_i}
        if shown_i >= 0:
            a["lab_at_shown"] = lab[shown_i]
            a["shown_tsc"] = ev_t[shown_i]
        if gpu_i >= 0:
            a["gpu_tsc"] = ev_t[gpu_i]
        frm = gpu_i if gpu_i >= 0 else bisect.bisect_left(ev_t, s.get("t_sub1", s["t_sub0"]))
        cleared = 0
        if shown_i >= 0 and frm >= 0:
            for j in range(frm + 1, shown_i + 1):
                if lab[j - 1] != 0 and lab[j] == 0:
                    cleared += 1
        a["cleared"] = cleared
        out[idx] = a
    return out


def pct(v):
    v = sorted(v)
    return ("%.0f/%.0f" % (v[len(v) // 2], v[-1])) if v else "-"


def row(label, ids, subs, an, us, gpu_us):
    eop = [i for i in ids if subs[i]["eop"] == 1]
    imm = [i for i in ids if subs[i]["eop"] == 0]
    unseen = [i for i in ids if an.get(i, {}).get("shown_i", -1) < 0]
    no_gpu = [i for i in eop if an.get(i, {}).get("gpu_i", -1) < 0]
    shown0 = [i for i in ids if an.get(i, {}).get("shown_i", -1) >= 0 and an[i]["lab_at_shown"] == 0]
    cleared = [i for i in ids if an.get(i, {}).get("cleared", 0) > 0]
    race = [i for i in eop if i - 1 in an and an[i].get("gpu_i", -1) >= 0
            and an[i - 1].get("shown_i", -1) >= 0 and an[i]["gpu_tsc"] < an[i - 1]["shown_tsc"]]
    imm_set = [i for i in imm if subs[i].get("lab%d_after" % subs[i]["bi"], 0) == 1]
    s2g = [us(an[i]["gpu_tsc"] - subs[i]["t_sub1"]) for i in eop if an.get(i, {}).get("gpu_i", -1) >= 0]
    g2s = [us(an[i]["shown_tsc"] - an[i]["gpu_tsc"]) for i in eop
           if an.get(i, {}).get("gpu_i", -1) >= 0 and an[i].get("shown_i", -1) >= 0]
    gms = [gpu_us(subs[i]) for i in eop if subs[i].get("gpu_ts_start", 0) and subs[i].get("gpu_ts", 0)]
    print("%-16s %6d %6d %5d %6d %6d %10d %8d %6d %11s  %s | %s | %s" % (
        label[:16], len(ids), len(eop), len(imm), len(unseen), len(no_gpu), len(shown0), len(cleared),
        len(race), "%d/%d" % (len(imm_set), len(imm)), pct(s2g), pct(g2s), pct(gms)))
    return shown0, cleared


def main():
    path = sys.argv[1]
    nlist = int(sys.argv[sys.argv.index("--list") + 1]) if "--list" in sys.argv else 3
    cfg, phases, subs, tails, ev, lost, last_line, ended = parse(path)
    hz = cfg.get("tsc_hz", 1) or 1
    us = lambda t: t * 1e6 / hz
    an = analyse(subs, ev)
    # The GPU clock's rate, measured: GPU stamps against the TSC of the poller seeing the same
    # submits finish, between the first and the last EOP submit that has both.
    timed = [i for i in sorted(subs) if subs[i]["eop"] == 1 and subs[i].get("gpu_ts", 0)
             and an.get(i, {}).get("gpu_i", -1) >= 0]
    gpu_hz = 0.0
    if len(timed) >= 2:
        a, b = timed[0], timed[-1]
        dt = an[b]["gpu_tsc"] - an[a]["gpu_tsc"]
        if dt > 0:
            gpu_hz = (subs[b]["gpu_ts"] - subs[a]["gpu_ts"]) * hz / dt
    gpu_us = lambda s: (s["gpu_ts"] - s["gpu_ts_start"]) * 1e6 / gpu_hz if gpu_hz else 0.0
    print("tsc_hz=%s gpu_clock_hz(measured)=%.0f neo=%s label_base=%s submits=%d events=%d ended=%s" % (
        cfg.get("tsc_hz"), gpu_hz, cfg.get("neo"), hex(cfg.get("label_base", 0)), len(subs), len(ev[0]),
        ended))
    if not ended:
        print("log ends without '=== FLIP_PROBE end' (closed or crashed); last line:")
        print("  " + last_line[:200])
    for l in lost:
        print("RING LOSS: " + l)
    print()
    hdr = "%-16s %6s %6s %5s %6s %6s %10s %8s %6s %11s  %s"
    print(hdr % ("phase", "subs", "eop", "imm", "unseen", "no_gpu", "shown_lab0", "cleared", "race",
                 "imm_lab_set", "us: submit->gpu p50/max | gpu->screen p50/max | gpu frame us p50/max"))
    by_phase = defaultdict(list)
    for idx in sorted(subs):
        by_phase[subs[idx]["phase"]].append(idx)
    flagged = {}
    for name in phases:
        ids = by_phase.get(name, [])
        if ids:
            flagged[name] = row(name, ids, subs, an, us, gpu_us)
    g = [n for n in phases if phases[n]["kind"] == 4]
    if g and by_phase.get(g[0]):
        ids = by_phase[g[0]]
        print()
        print("G by GPU load (1 MB CP DMA copies):")
        for lo, hi in ((0, 7), (8, 15), (16, 23), (24, 31), (32, 39), (40, 48)):
            sel = [i for i in ids if lo <= subs[i].get("load", 0) <= hi]
            if sel:
                row("  load %2d-%2d" % (lo, hi), sel, subs, an, us, gpu_us)
        print("G by max pending:")
        for mp in range(1, 9):
            sel = [i for i in ids if subs[i].get("max_pending", 0) == mp]
            if sel:
                row("  pending %d" % mp, sel, subs, an, us, gpu_us)
    print()
    print("examples (first %d per phase) of shown_lab0 / cleared:" % nlist)
    for name, (shown0, cleared) in flagged.items():
        for i in sorted(set(shown0) | set(cleared))[:nlist]:
            a = an[i]
            print("  %-16s idx=%d eop=%d bi=%d load=%s lab_at_shown=%s cleared=%d" % (
                name[:16], i, subs[i]["eop"], subs[i]["bi"], subs[i].get("load", 0),
                hex(a.get("lab_at_shown", -1) & 0xFFFFFFFFFFFFFFFF), a["cleared"]))
    print()
    print("marker block after libSceGnmDriver's patch (distinct patterns):")
    seen = OrderedDict()
    for idx in sorted(tails):
        post = tuple(tails[idx].get("post", []))
        if post and post not in seen:
            seen[post] = idx
    for post, idx in list(seen.items())[:8]:
        words = list(post)
        while words and words[-1] == "0x0":
            words.pop()
        print("  first at submit %d (bi=%s): %s" % (idx, subs[idx]["bi"] if idx in subs else "?", " ".join(words)))
    print("  distinct patterns: %d over %d EOP submits" % (len(seen), sum(1 for i in subs if subs[i]["eop"] == 1)))


if __name__ == "__main__":
    main()
