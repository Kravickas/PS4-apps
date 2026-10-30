"""python3 tools/check_ps_cvt.py - plumbing check of the OPCODE_TEST 5 binaries (shaders/ps_cvt.s) with main.c's tables: every
conversion is replaced by a tag (binary, source line), so the final results show which block of
which binary wrote each slot's class. Checks: each (row, class) with a binary is written by that
binary's block for that row's op (or row), nothing else is written, headings stay 0."""
import re, sys
ROOT = __import__("os").path.dirname(__import__("os").path.dirname(__import__("os").path.abspath(__file__)))
src = open(ROOT + "/shaders/ps_cvt.s").read()
H = open(ROOT + "/shaders/ps_cvt_set.h").read()
O5 = open(ROOT + "/src/optest5.h").read()
OPS = re.findall(r'"(V_CVT_[^"]*)"', O5.split("k_opt5_op[OPT5_OPS] = {")[1].split("};")[0])
ROWS = [(int(o), int(a, 16), int(b, 16), int(c, 16)) for o, a, b, c in
        re.findall(r"\{(\d+),\s*0x([0-9A-F]+)u,\s*0x([0-9A-F]+)u,\s*0x([0-9A-F]+)u\}", O5)]
WIDE = [int(x) for x in re.search(r"k_opt5_wide\[OPT5_OPS\] = \{([^}]*)\}", O5).group(1).split(",")]
def arr(name):
    return [int(x, 0) for x in re.search(name + r"\[[^\]]*\] = \{([^}]*)\}", H).group(1).replace("u", "").split(",") if x.strip()]
BCLASS = arr("k_opt5_bin_class"); BKIND = arr("k_opt5_bin_kind")
RB = [[int(x, 0) for x in g.split(",")] for g in re.findall(r"\{(0x[0-9A-F]+(?:,\s*0x[0-9A-F]+)*)\}", H.split("k_opt5_row_bin[][OPT5_CLASSES] = {")[1])]
assert len(RB) == len(ROWS), (len(RB), len(ROWS))
NCLS = 7; MODES = 1
# slots as ui.h: a heading (op 0xff) before each op's rows
slots = []
prev = -1
for r, (o, a, b, c) in enumerate(ROWS):
    if o != prev: slots.append(None); prev = o
    slots.append(r)
n = len(slots)
tab_slots = []
for i, r in enumerate(slots):
    if r is None: tab_slots.append([0xFF, 0, 0, 0])
    else:
        o, a, b, c = ROWS[r]
        tab_slots.append([o | (0x100 if WIDE[o] else 0) | ((r + 1) << 16), a, b, c])
STRIDE = 8 * NCLS * MODES
res = {}  # byte offset -> dword
progs = re.split(r"(?m)^; binary (\d+): ", src)[1:]
progs = [(int(progs[i]), progs[i + 1]) for i in range(0, len(progs), 2)]
assert len(progs) == len(BCLASS)
M32 = 0xFFFFFFFF
def run(b, text, cls):
    lines = [l.strip() for l in text.split("\n")]
    labels = {l[:-1]: i for i, l in enumerate(lines) if l.endswith(":")}
    s = {}; t8 = 8 * (cls * MODES); t9 = n; t10 = 0xC0DE5A00 + ((cls * MODES) << 8); t11 = STRIDE
    s[12], s[13], s[14], s[15] = t8, t9, t10, t11
    cs = BKIND[b] >= 8
    lanes = list(range(n))
    v = {i: {} for i in lanes}
    for i in lanes:
        v[i][0] = i % 64 if cs else 0
        v[i][2] = (i % 512) + 0.5; v[i][3] = (i // 512) + 0.5
    s[2] = 0  # group: computed per lane below for cs (emulate one big group: slot = s2*64+v0)
    exec_ = set(lanes); vcc = set(); sp = {}
    pc = 0
    def val(x, i):
        x = x.strip()
        if x.startswith("v") and x[1:].isdigit(): return v[i].get(int(x[1:]), 0)
        if x.startswith("s") and x[1:].isdigit(): return s.get(int(x[1:]), 0)
        return int(x, 0) & M32
    while pc < len(lines):
        l = lines[pc]; pc += 1
        if not l or l.startswith(";") or l.endswith(":"): continue
        op, _, args = l.partition(" ")
        a = [x.strip() for x in args.split(",")] if args else []
        if op in ("s_load_dwordx8", "s_load_dwordx4", "s_waitcnt", "exp", "s_nop"): continue
        if op == "s_endpgm": break
        if op == "s_lshl_b32": s[int(a[0][1:])] = 0; continue  # s3 = group*64: lanes use own group below
        if op == "v_add_i32" and cs and a[2] == "s3":
            for i in exec_: v[i][4] = i  # slot = group * 64 + thread
            continue
        if op == "v_cvt_u32_f32" and a[1] in ("v2", "v3"):
            for i in exec_: v[i][int(a[0][1:])] = int(v[i][int(a[1][1:])])
            continue
        if op == "v_lshlrev_b32":
            for i in exec_: v[i][int(a[0][1:])] = (val(a[2], i) << val(a[1], i)) & M32
            continue
        if op == "v_lshrrev_b32":
            for i in exec_: v[i][int(a[0][1:])] = val(a[2], i) >> val(a[1], i)
            continue
        if op == "v_add_i32":
            for i in exec_: v[i][int(a[0][1:])] = (val(a[2], i) + val(a[3], i)) & M32
            continue
        if op == "v_and_b32":
            for i in exec_: v[i][int(a[0][1:])] = val(a[1], i) & val(a[2], i)
            continue
        if op == "v_mov_b32":
            for i in exec_: v[i][int(a[0][1:])] = val(a[1], i)
            continue
        if op == "v_mul_u32_u24":
            for i in exec_: v[i][int(a[0][1:])] = (val(a[1], i) & 0xFFFFFF) * (val(a[2], i) & 0xFFFFFF) & M32
            continue
        if op in ("v_cmp_gt_u32", "v_cmp_eq_u32", "v_cmp_ne_u32", "v_cmp_eq_u32_e64"):
            f = {"gt": lambda x, y: x > y, "eq": lambda x, y: x == y, "ne": lambda x, y: x != y}[op.split("_")[2]]
            res_ = {i for i in exec_ if f(val(a[1], i), val(a[2], i))}
            if a[0] == "vcc": vcc = res_
            else: sp[a[0]] = res_
            continue
        if op == "s_and_saveexec_b64":
            sp[a[0]] = set(exec_); exec_ = exec_ & vcc; continue
        if op == "s_and_b64":
            vcc = vcc & sp[a[2]]; continue
        if op == "s_mov_b64":
            exec_ = set(sp[a[1]]); continue
        if op == "s_cbranch_execz":
            if not exec_: pc = labels[a[0]] + 1
            continue
        if op in ("s_mov_b32", "s_movk_i32"):
            s[int(a[0][1:])] = int(a[1], 0) & M32; continue
        if op in ("s_mul_i32", "s_lshr_b32", "s_add_u32"):
            x, y = val(a[1], 0), val(a[2], 0)
            s[int(a[0][1:])] = {"s_mul_i32": x * y, "s_lshr_b32": x >> y, "s_add_u32": x + y}[op] & M32
            continue
        if op == "buffer_load_dwordx4":
            for i in exec_:
                sl = val(a[1], i) // 16
                for j in range(4): v[i][8 + j] = tab_slots[sl][j]
            continue
        if op in ("buffer_store_dwordx2", "buffer_store_dword"):
            off = 4 if "offset:4" in l else 0
            for i in exec_:
                base = val(a[1], i) + off
                if op == "buffer_store_dwordx2":
                    res[base] = v[i].get(12, 0); res[base + 4] = v[i].get(13, 0)
                else:
                    res[base] = v[i].get(int(a[0][1:]), 0)
            continue
        if op == "v_cndmask_b32":
            for i in exec_ & vcc: v[i][int(a[0][1:])] = val(a[2], i)
            continue
        if op.startswith("v_cvt") or op == ".long":
            tag = (b << 16) | pc  # this block of this binary
            for i in exec_:
                v[i][14] = tag; v[i][15] = tag ^ 0x80000000
            continue
        raise SystemExit("unhandled: " + l)
for b, text in progs: run(b, text, BCLASS[b])
# checks
bad = 0
for i, r in enumerate(slots):
    for c in range(NCLS):
        lo = res.get(i * STRIDE + 8 * c, 0)
        if r is None:
            if lo: bad += 1; print("heading written", i, c)
            continue
        want = RB[r][c]
        if want == 0xFFFF:
            if lo: bad += 1; print("written without binary", r, c, hex(lo))
            continue
        if lo >> 16 != want:
            bad += 1
            if bad < 20: print("row", r, OPS[ROWS[r][0]], "class", c, "written by", lo >> 16, "want", want)
print("slots", n, "checks done, mismatches", bad)
