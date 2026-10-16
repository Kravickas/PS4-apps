"""Summarises flipprobe.log (FLIP_PROBE in src/main.c).

usage: python tools/flipprobe_summary.py flipprobe.log

Per phase:
  submits, EOP / immediate, submits never seen on screen, EOP submits whose GPU completion
  was not seen;
  shown_lab0   - flips whose buffer label was 0 at the moment the flip was first reported on
                 screen (the state shadPS4 asserts cannot happen at an EOP flip's interrupt);
  cleared      - flips whose buffer label went 1 -> 0 between the GPU finishing the submit
                 (immediate flips: the submit returning) and the flip reaching the screen;
  race         - EOP submits the GPU finished before the previous flip reached the screen;
  imm_lab_set  - immediate flips whose label read 1 right after sceVideoOutSubmitFlip returned;
  timing       - submit -> GPU done and GPU done -> on screen, in microseconds.
Then the marker block as libSceGnmDriver patched it, once per distinct pattern.
"""
import re
import sys
from collections import OrderedDict, defaultdict

KV = re.compile(r"(\w+)=(\S+)")


def num(v):
    try:
        return int(v, 0)
    except ValueError:
        return v


def parse(path):
    subs, ans, tails, cfg = {}, {}, defaultdict(dict), {}
    phases = OrderedDict()
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.rstrip("\n")
        if line.startswith("CFG"):
            cfg = {k: num(v) for k, v in KV.findall(line)}
        elif line.startswith("PHASE_CFG"):
            d = dict(KV.findall(line))
            name = line.split(" name=")[1].split(" kind=")[0]
            phases[name] = {"kind": int(d["kind"]), "max_pending": int(d["max_pending"])}
        elif line.startswith("SUB "):
            name = line.split(" phase=")[1].split(" eop=")[0]
            d = {k: num(v) for k, v in KV.findall(line.replace(" phase=" + name, ""))}
            d["phase"] = name
            subs[d["idx"]] = d
        elif line.startswith("TAIL_PRE_SAME") or line.startswith("TAIL_POST_SAME"):
            # Identical to an earlier submit's block, which was logged in full.
            kind = "pre" if line.startswith("TAIL_PRE") else "post"
            idx = int(re.search(r"submit=(\d+)", line).group(1))
            ref = int(re.search(r" as=(\d+)", line).group(1))
            tails[idx][kind] = tails[ref][kind]
        elif line.startswith("TAIL_PRE") or line.startswith("TAIL_POST"):
            kind = "pre" if line.startswith("TAIL_PRE") else "post"
            idx = int(re.search(r"submit=(\d+)", line).group(1))
            parts = line.split(" : ")
            tails[idx][kind] = parts[1].split() if len(parts) > 1 else []
        elif line.startswith("AN "):
            name = line.split(" phase=")[1].split(" eop=")[0]
            d = {k: num(v) for k, v in KV.findall(line.replace(" phase=" + name, ""))}
            d["phase"] = name
            ans[d["idx"]] = d
    return cfg, phases, subs, ans, tails


def main():
    cfg, phases, subs, ans, tails = parse(sys.argv[1])
    hz = cfg.get("tsc_hz", 1) or 1
    us = lambda t: t * 1e6 / hz
    print("tsc_hz=%s neo=%s label_base=%s" % (cfg.get("tsc_hz"), cfg.get("neo"), hex(cfg.get("label_base", 0))))
    print()
    hdr = "%-15s %4s %4s %4s %7s %8s %10s %7s %5s %11s  %s"
    print(hdr % ("phase", "subs", "eop", "imm", "unseen", "no_gpu", "shown_lab0", "cleared", "race",
                 "imm_lab_set", "us: submit->gpu p50/max | gpu->screen p50/max"))
    by_phase = defaultdict(list)
    for idx in sorted(subs):
        by_phase[subs[idx]["phase"]].append(idx)
    for name in phases:
        ids = by_phase.get(name, [])
        eop = [i for i in ids if subs[i]["eop"] == 1]
        imm = [i for i in ids if subs[i]["eop"] == 0]
        unseen = [i for i in ids if ans.get(i, {}).get("shown_ev", -1) < 0]
        no_gpu = [i for i in eop if ans.get(i, {}).get("gpu_done_ev", -1) < 0]
        shown0 = [i for i in ids if ans.get(i, {}).get("shown_ev", -1) >= 0 and ans[i]["lab_at_shown"] == 0]
        cleared = [i for i in ids if ans.get(i, {}).get("lab_1to0_between", 0) > 0]
        race = []
        for i in eop:
            p = i - 1
            if p in ans and ans[i].get("gpu_done_ev", -1) >= 0 and ans[p].get("shown_ev", -1) >= 0:
                if ans[i]["gpu_done_tsc"] < ans[p]["shown_tsc"]:
                    race.append(i)
        imm_set = [i for i in imm if (subs[i]["lab%d_after" % subs[i]["bi"]] if subs[i]["bi"] in (0, 1) else 0) == 1]
        s2g = sorted(us(ans[i]["gpu_done_tsc"] - subs[i]["t_sub1"]) for i in eop if ans.get(i, {}).get("gpu_done_ev", -1) >= 0)
        g2s = sorted(us(ans[i]["shown_tsc"] - ans[i]["gpu_done_tsc"]) for i in eop
                     if ans.get(i, {}).get("gpu_done_ev", -1) >= 0 and ans[i].get("shown_ev", -1) >= 0)
        pct = lambda a: ("%.0f/%.0f" % (a[len(a) // 2], a[-1])) if a else "-"
        print(hdr % (name[:15], len(ids), len(eop), len(imm), len(unseen), len(no_gpu), len(shown0),
                     len(cleared), len(race), "%d/%d" % (len(imm_set), len(imm)), pct(s2g) + " | " + pct(g2s)))
    print()
    print("examples (first 3 per phase) of shown_lab0 / cleared:")
    for name in phases:
        ids = [i for i in by_phase.get(name, []) if ans.get(i, {}).get("lab_1to0_between", 0) > 0
               or (ans.get(i, {}).get("shown_ev", -1) >= 0 and ans[i]["lab_at_shown"] == 0)]
        for i in ids[:3]:
            a = ans[i]
            print("  %-15s idx=%d eop=%d bi=%d lab_at_gpu_done=%s lab_at_shown=%s 1to0=%d 0to1=%d" % (
                name[:15], i, subs[i]["eop"], subs[i]["bi"], hex(a["lab_at_gpu_done"]), hex(a["lab_at_shown"]),
                a["lab_1to0_between"], a["lab_0to1_between"]))
    print()
    print("marker block after libSceGnmDriver's patch (distinct patterns, label/arg words included):")
    seen = OrderedDict()
    for idx in sorted(tails):
        post = tuple(tails[idx].get("post", []))
        if post and post not in seen:
            seen[post] = idx
    for post, idx in list(seen.items())[:8]:
        words = list(post)
        while words and words[-1] == "0x0":
            words.pop()
        print("  first at submit %d (bi=%s): %s" % (idx, subs[idx]["bi"], " ".join(words)))
    print("  distinct patterns: %d over %d EOP submits" % (len(seen), sum(1 for i in subs if subs[i]["eop"] == 1)))


if __name__ == "__main__":
    main()
