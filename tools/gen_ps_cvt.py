#!/usr/bin/env python3
"""OPCODE_TEST 5 conversion passes (src/optest5.h numbering): a binary per V_CVT_* and kind, so an
emulator that cannot translate one instruction only loses that binary (main.c skips a binary that
crashed or hung in an earlier launch). Writes shaders/ps_cvt.s (every binary's source, one after the
other) and shaders/ps_cvt_set.h (the binaries; main.c gives each float mode its own hash at upload).

Kinds; class = where the result is kept (kind 0 / 1 in class 0, compared with the others in the log):
  0 VGPR        the op's e32 rows (VOP1 / VOP2 encoding), sources in VGPRs        class 0
  1 modifiers   the op's VOP3 modifier rows ("OP (mods)": e64 with the modifier bits) class 0
  2 e64         the e32 rows in the VOP3 encoding                                  class 1
  3 SGPR        the e32 rows with src0 in s[24:25] (s_mov_b32 of the row's value)  class 2
  4 literal     the e32 rows with src0 a literal dword (forced, also where an inline constant exists;
                an f64 source only when its low dword is 0: the literal is the high dword)  class 3
  5 inline      the e32 rows whose src0 has an inline constant (0..64, -1..-16, +-0.5, 1, 2, 4) class 4
  6 store       kind 0's code, results written by two buffer_store_dword        class 5
  7 store mods  kind 1's, the same way                                            class 5
  8 compute     kind 0's code as a compute shader (slot = thread group * 64 + thread)  class 6
  9 compute mods  kind 1's, the same way                                          class 6
Kinds 3..5 bake each row's value into the code (one block per row, kept when the slot's row id
matches). Leaves out V_CVT_U32_F64 and V_CVT_PKACCUM_U8_F32 (no shadPS4 translator: its assert stops
the shader) and the VOP3 forms (e64, modifiers) of V_CVT_F32_UBYTE0..3 (shadPS4 asserts decoding
them). V_CVT_OFF_F32_I4 takes immediate operands only (shadPS4 asserts on a register): one
instruction per value of its rows, kept for its op with that a; kind 3 leaves it out. Needs llvm-mc
(AMDGPU target).
python3 tools/gen_optest5.py && python3 tools/gen_ps_cvt.py"""
import os, re, shutil, struct, subprocess, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
M = 0xFFFFFFFF
H = open(os.path.join(ROOT, "src", "optest5.h")).read()
OPS = [n[len("V_CVT_"):] for n in re.findall(r'"(V_CVT_[^"]*)"', H.split("k_opt5_op[OPT5_OPS] = {")[1].split("};")[0])]
ROWS = [(int(o), int(a, 16), int(b, 16), int(c, 16)) for o, a, b, c in
        re.findall(r"\{(\d+),\s*0x([0-9A-F]+)u,\s*0x([0-9A-F]+)u,\s*0x([0-9A-F]+)u\}", H)]
MODES = int(re.search(r"#define OPT5_MODES (\d+)", H).group(1))
# base op -> (instruction, source kind: f1 / d1 / i1 one source, f2 / i2 two, pk8 three; 64-bit result)
BASE = {"I32_F32": ("v_cvt_i32_f32", "f1", 0), "U32_F32": ("v_cvt_u32_f32", "f1", 0),
        "FLR_I32_F32": ("v_cvt_flr_i32_f32", "f1", 0), "RPI_I32_F32": ("v_cvt_rpi_i32_f32", "f1", 0),
        "I32_F64": ("v_cvt_i32_f64", "d1", 0), "F32_I32": ("v_cvt_f32_i32", "i1", 0),
        "F32_U32": ("v_cvt_f32_u32", "i1", 0), "F64_I32": ("v_cvt_f64_i32", "i1", 1),
        "F64_U32": ("v_cvt_f64_u32", "i1", 1), "F64_F32": ("v_cvt_f64_f32", "f1", 1),
        "F32_F64": ("v_cvt_f32_f64", "d1", 0), "F16_F32": ("v_cvt_f16_f32", "f1", 0),
        "F32_F16": ("v_cvt_f32_f16", "i1", 0), "F32_UBYTE0": ("v_cvt_f32_ubyte0", "i1", 0),
        "F32_UBYTE1": ("v_cvt_f32_ubyte1", "i1", 0), "F32_UBYTE2": ("v_cvt_f32_ubyte2", "i1", 0),
        "F32_UBYTE3": ("v_cvt_f32_ubyte3", "i1", 0), "OFF_F32_I4": ("v_cvt_off_f32_i4", "i1", 0),
        "PKRTZ_F16_F32": ("v_cvt_pkrtz_f16_f32", "f2", 0), "PKNORM_I16_F32": ("v_cvt_pknorm_i16_f32", "f2", 0),
        "PKNORM_U16_F32": ("v_cvt_pknorm_u16_f32", "f2", 0), "PK_U16_U32": ("v_cvt_pk_u16_u32", "i2", 0),
        "PK_I16_I32": ("v_cvt_pk_i16_i32", "i2", 0), "PK_U8_F32": ("v_cvt_pk_u8_f32", "pk8", 0)}
OFF = "OFF_F32_I4"
# no VOP3 encoding (gen_optest5.py NO_VOP3): shadPS4 asserts decoding it
NO_VOP3 = ("F32_UBYTE0", "F32_UBYTE1", "F32_UBYTE2", "F32_UBYTE3")
KINDS = ["VGPR", "modifiers", "e64", "SGPR", "literal", "inline", "store", "store mods", "compute",
         "compute mods"]
KIND_CLASS = [0, 0, 1, 2, 3, 4, 5, 5, 6, 6]
CLASSES = ["VGPR", "e64", "SGPR", "literal", "inline", "store", "compute"]
HASH = 0xCA000000  # | binary << 8 | mode, set by main.c at upload
RSRC1 = "PGM_RSRC1 0xC4 (20 VGPRs, 26 SGPRs incl. VCC)"

def split(name):
    m = re.match(r"^(\S+)(?: \((.*)\))?$", name)
    return m.group(1), m.group(2)

def mc():
    p = next((shutil.which(n) for n in ("llvm-mc", "llvm-mc-18", "llvm-mc-19", "llvm-mc-20") if shutil.which(n)), None)
    if not p: raise SystemExit("llvm-mc not found")
    return p

def encode_each(lines):
    """Encoding of each line on its own (no labels): a list of dword lists."""
    out = subprocess.run([mc(), "-triple=amdgcn", "-mcpu=bonaire", "-show-encoding"], input="\n".join(lines) + "\n",
                         capture_output=True, text=True, check=True).stdout
    enc = re.findall(r"encoding: \[([^\]]*)\]", out)
    assert len(enc) == len(lines), (len(enc), len(lines))
    res = []
    for e in enc:
        b = [int(x, 16) for x in e.split(",")]
        res.append([b[i] | b[i + 1] << 8 | b[i + 2] << 16 | b[i + 3] << 24 for i in range(0, len(b), 4)])
    return res

def operands(kind):
    return {"f1": "v14, v9", "i1": "v14, v9", "d1": "v14, v[9:10]", "f2": "v14, v9, v10", "i2": "v14, v9, v10",
            "pk8": "v14, v9, v10, v11"}[kind]

def dst(ins, wide):
    return ins.replace("v14,", "v[14:15],") if wide else ins

# encodings: e32 (VOP1 / VOP2; None for VOP3-only) and e64 of every base op, sources v9 / v[9:10], v10, v11
names = list(BASE)
e32_src = [dst("%s %s" % (BASE[n][0], operands(BASE[n][1])), BASE[n][2]) for n in names]
e64_src = [dst("%s%s %s" % (BASE[n][0], "" if BASE[n][1] == "pk8" else "_e64", operands(BASE[n][1])), BASE[n][2])
           for n in names]
E32 = dict(zip(names, encode_each(e32_src)))
E64 = dict(zip(names, encode_each(e64_src)))
for n in names:
    if len(E32[n]) != 1: E32[n] = None

MODBITS = {"clamp": (1 << 11, 0), "mul:2": (0, 1 << 27), "mul:4": (0, 2 << 27), "div:2": (0, 3 << 27)}
def mod_bits(mods):
    w0 = w1 = 0
    for t in [x.strip() for x in mods.split(",")]:
        if t in MODBITS:
            w0 |= MODBITS[t][0]; w1 |= MODBITS[t][1]; continue
        m = re.match(r"^(-?)(\|?)src(\d?)\|?$", t)
        assert m, t
        i = int(m.group(3) or 0)
        if m.group(1): w1 |= 1 << (29 + i)
        if m.group(2): w0 |= 1 << (8 + i)
    return w0, w1

def inline_code(v, d64):
    """src field for an inline constant equal to v (32-bit value, or 64-bit for an f64 source), else None."""
    if d64:
        fl = {0x3FE0000000000000: 240, 0xBFE0000000000000: 241, 0x3FF0000000000000: 242, 0xBFF0000000000000: 243,
              0x4000000000000000: 244, 0xC000000000000000: 245, 0x4010000000000000: 246, 0xC010000000000000: 247}
        if v <= 64: return 128 + v
        if v >= (1 << 64) - 16: return 192 + ((1 << 64) - v)
        return fl.get(v)
    fl = {0x3F000000: 240, 0xBF000000: 241, 0x3F800000: 242, 0xBF800000: 243, 0x40000000: 244, 0xC0000000: 245,
          0x40800000: 246, 0xC0800000: 247}
    if v <= 64: return 128 + v
    if v >= 0xFFFFFFF0: return 192 + (0x100000000 - v)
    return fl.get(v)

def longs(ws):
    return [".long " + ", ".join("0x%08x" % w for w in ws)]

def keep(cond, wide):
    L = [cond, "v_cndmask_b32 v12, v12, v14, vcc"]
    if wide: L.append("v_cndmask_b32 v13, v13, v15, vcc")
    return L

def pre(base):
    return ["v_mov_b32 v14, 0xdead0000"] if base == "F16_F32" else []

def off_values(op):
    vals = [a for o, a, b, c in ROWS if o == op]
    return sorted(set(vals), key=vals.index)

def imm_text(v):
    return str(v - (1 << 32)) if v >= 0xFFFFFFF0 else ("%d" % v if v <= 64 else "0x%x" % v)

def op_block(k, e64=False):
    """Kind 0 (e64: kind 1) code of op k on the slot's operands; [] when the kind has no such row."""
    base, mods = split(OPS[k])
    if base not in BASE: return []
    ins, kind, wide = BASE[base]
    if e64 and (mods or base in NO_VOP3): return []
    L = ["; op %d: %s%s" % (k, OPS[k], " (e64)" if e64 else "")]
    if base == OFF:
        L += ["s_movk_i32 s26, 0x%x" % k, "v_cmp_eq_u32_e64 s[22:23], s26, v8"]
        for v in off_values(k):
            if mods or e64:
                code = inline_code(v, False)
                if code is None:  # VOP3 takes no literal
                    assert e64, (OPS[k], v)
                    continue
                w0, w1 = E64[base]
                b0, b1 = mod_bits(mods) if mods else (0, 0)
                L += longs([w0 | b0, (w1 & ~0x1FF) | code | b1])
            else:
                L.append("v_cvt_off_f32_i4 v14, %s" % imm_text(v))
            L += ["v_cmp_eq_u32 vcc, 0x%x, v9" % v, "s_and_b64 vcc, vcc, s[22:23]", "v_cndmask_b32 v12, v12, v14, vcc"]
        return L
    L += pre(base)
    if mods:
        w0, w1 = E64[base]
        b0, b1 = mod_bits(mods)
        L += longs([w0 | b0, w1 | b1])
    elif e64:
        L += longs(E64[base])
    else:
        L.append(dst("%s %s" % (ins, operands(kind)), wide))
    return L + keep("v_cmp_eq_u32 vcc, %d, v8" % k, wide)

def row_block(r, kind_ix):
    """Kinds 2..4: row r's op on its own value baked in; [] when the kind has no encoding for it."""
    k, a, b, c = ROWS[r]
    base, mods = split(OPS[k])
    if mods or base not in BASE: return []
    ins, kind, wide = BASE[base]
    d64 = kind == "d1"
    v = a | b << 32 if d64 else a
    L = ["; row %d: %s" % (r, OPS[k])]
    if kind_ix == 2:
        if base == OFF: return []
        L += ["s_mov_b32 s24, 0x%x" % a] + (["s_mov_b32 s25, 0x%x" % b] if d64 else []) + pre(base)
        src = "s[24:25]" if d64 else "s24"
        L.append(dst("%s %s" % (ins, operands(kind).replace("v[9:10]", src).replace("v9", src, 1)), wide))
    elif kind_ix == 3:
        if E32[base] is None or (d64 and a != 0): return []
        lit = b if d64 else a
        L += pre(base) + longs([(E32[base][0] & ~0x1FF) | 0xFF, lit])
    else:
        code = inline_code(v, d64)
        if code is None: return []
        L += pre(base)
        if E32[base] is None:
            w0, w1 = E64[base]
            L += longs([w0, (w1 & ~0x1FF) | code])
        else:
            L += longs([(E32[base][0] & ~0x1FF) | code])
    return L + keep("v_cmp_eq_u32 vcc, 0x%x, v17" % (r + 1), wide)

HEAD = ["; binary %d: V_CVT_%s, kind %s (tools/gen_ps_cvt.py): one %s per table slot. Table (s[0:1]):",
        "; [0..3] V# of the slots {op | 64-bit result << 8 | row + 1 << 16, a, b, c} (op 0xff: a heading), [4..7] V#",
        "; of the results ([11] bytes a slot: lo, hi a class and float mode), [8] this pass's byte offset in a",
        "; result, [9] slot count, [10] marker. Runs the op on the slot's operands and keeps the slot's (a, b = an",
        "; f64 source; c = PK_U8's S2); FLOAT_MODE from PGM_RSRC1. Slot 0 stores the marker at [slot count * [11] +",
        "; [8] / 2]."]

def base_of(k):
    return split(OPS[k])[0]

def body(base, kind):
    """The binary's op code: [] when the kind has nothing of this op."""
    L = []
    if kind in (0, 6, 8, 2):
        for k in range(len(OPS)):
            if base_of(k) == base and not split(OPS[k])[1]: L += op_block(k, kind == 2)
    elif kind in (1, 7, 9):
        for k in range(len(OPS)):
            if base_of(k) == base and split(OPS[k])[1]: L += op_block(k)
    else:
        for r in range(len(ROWS)):
            if base_of(ROWS[r][0]) == base: L += row_block(r, kind - 1)
    return L

def source(b, base, kind, code):
    cs = kind in (8, 9)
    lab = "_%d" % b
    L = [l % (b, base, KINDS[kind], "thread" if cs else "pixel (x + 512 y)") if i == 0 else l
         for i, l in enumerate(HEAD)]
    L += ["s_load_dwordx8 s[4:11], s[0:1], 0x0", "s_load_dwordx4 s[12:15], s[0:1], 0x8"]
    if cs:
        L += ["s_lshl_b32 s3, s2, 6", "v_add_i32 v4, vcc, s3, v0"]
    else:
        L += ["v_cvt_u32_f32 v4, v2", "v_cvt_u32_f32 v5, v3", "v_lshlrev_b32 v5, 9, v5", "v_add_i32 v4, vcc, v4, v5"]
    L += ["s_waitcnt lgkmcnt(0)", "v_cmp_gt_u32 vcc, s13, v4", "s_and_saveexec_b64 s[16:17], vcc",
          "s_cbranch_execz cvt_done" + lab, "v_lshlrev_b32 v5, 4, v4",
          "buffer_load_dwordx4 v[8:11], v5, s[4:7], 0 offen", "s_waitcnt vmcnt(0)", "v_lshrrev_b32 v17, 16, v8",
          "v_and_b32 v8, 0xff, v8", "v_mov_b32 v12, 0", "v_mov_b32 v13, 0"]
    L += code
    L += ["v_mul_u32_u24 v6, s15, v4", "v_add_i32 v6, vcc, s12, v6"]
    if kind in (6, 7):
        L += ["buffer_store_dword v12, v6, s[8:11], 0 offen", "buffer_store_dword v13, v6, s[8:11], 0 offen offset:4"]
    else:
        L.append("buffer_store_dwordx2 v[12:13], v6, s[8:11], 0 offen")
    L += ["v_cmp_eq_u32 vcc, 0, v4", "s_and_saveexec_b64 s[18:19], vcc", "s_cbranch_execz cvt_nomark" + lab,
          "s_mul_i32 s20, s13, s15", "s_lshr_b32 s21, s12, 1", "s_add_u32 s20, s20, s21", "v_mov_b32 v7, s20",
          "v_mov_b32 v16, s14", "buffer_store_dword v16, v7, s[8:11], 0 offen", "cvt_nomark%s:" % lab,
          "s_mov_b64 exec, s[18:19]", "cvt_done%s:" % lab, "s_mov_b64 exec, s[16:17]"]
    if not cs: L += ["v_mov_b32 v0, 0", "exp mrt0 v0, v0, v0, v0 done vm"]
    L.append("s_endpgm")
    return "\n".join(L) + "\n"

def rows_kind(base, kind):
    """Rows the binary keeps a result for."""
    out = []
    for r, (k, a, b_, c) in enumerate(ROWS):
        if base_of(k) != base: continue
        mods = split(OPS[k])[1]
        if kind in (0, 6, 8) and not mods: out.append(r)
        elif kind in (1, 7, 9) and mods: out.append(r)
        elif kind == 2 and not mods and base not in NO_VOP3 and (base != OFF or inline_code(a, False) is not None):
            out.append(r)
        elif kind in (3, 4, 5) and row_block(r, kind - 1): out.append(r)
    return out

def assemble(src):
    with tempfile.TemporaryDirectory() as d:
        obj = os.path.join(d, "p.o")
        subprocess.run([mc(), "-triple=amdgcn", "-mcpu=bonaire", "-filetype=obj", "-o", obj], input=src, text=True,
                       check=True)
        e = open(obj, "rb").read()
    # ELF64: .text by name (branches resolved, unlike -show-encoding)
    shoff, shentsize, shnum, shstrndx = struct.unpack_from("<Q", e, 0x28)[0], *struct.unpack_from("<HHH", e, 0x3A)
    sh = [struct.unpack_from("<IIQQQQ", e, shoff + i * shentsize) for i in range(shnum)]
    names_off = sh[shstrndx][4]
    text = next(x for x in sh if e[names_off + x[0]:e.index(b"\0", names_off + x[0])] == b".text")
    b = e[text[4]:text[4] + text[5]]
    return list(struct.unpack("<%dI" % (len(b) // 4), b))

def binary(name, code, h):
    if len(code) % 2: code = code + [0xBF800000]  # s_nop: even dword count after the SDK token
    code = [0xBEEB03FF, len(code) // 2] + code    # token, n / 2 - 1 with n = dwords incl. the token
    L = ["static const uint32_t %s[] __attribute__((aligned(256))) = {" % name]
    for i in range(0, len(code), 6): L.append("    " + " ".join("0x%08X," % w for w in code[i:i + 6]))
    L.append("    " + " ".join("0x%08X," % w for w in (0x5362724F, 0x00726468, len(code) * 4 << 8, 0, 0xDEADBEEF, h, 0)))
    L.append("};")
    return L

# modifier ops: the patched e64 must equal llvm-mc's own encoding where it takes the modifiers
chk = [(k, OPS[k]) for k in range(len(OPS)) if split(OPS[k])[1] and split(OPS[k])[0] in BASE and split(OPS[k])[0] != OFF]
ok_llvm = []
for k, n in chk:
    base, mods = split(n)
    ins, kind, wide = BASE[base]
    srcs = operands(kind).split(", ")[1:]
    toks = [t.strip() for t in mods.split(",")]
    post = ""
    for t in toks:
        if t in MODBITS: post += " " + t; continue
        m = re.match(r"^(-?)(\|?)src(\d?)\|?$", t)
        i = int(m.group(3) or 0)
        s = srcs[i]
        if m.group(2): s = "|%s|" % s
        if m.group(1): s = "-" + s
        srcs[i] = s
    line = dst("%s%s v14, %s%s" % (ins, "" if kind == "pk8" else "_e64", ", ".join(srcs), post), wide)
    try:
        e = encode_each([line])[0]
    except subprocess.CalledProcessError:
        continue  # llvm-mc refuses the modifier on this operand type: only the patched encoding exists
    w0, w1 = E64[base]
    b0, b1 = mod_bits(mods)
    assert e == [w0 | b0, w1 | b1], (n, line, ["%08x" % x for x in e], "%08x %08x" % (w0 | b0, w1 | b1))
    ok_llvm.append(n)

bins = []  # (base, kind, rows)
for base in BASE:
    for kind in range(len(KINDS)):
        code = body(base, kind)
        rows = rows_kind(base, kind)
        if code:
            assert rows, (base, kind)
            bins.append((base, kind, code, rows))
srcs, arrays = [], []
row_bin = [[0xFFFF] * len(CLASSES) for _ in ROWS]
for b, (base, kind, code, rows) in enumerate(bins):
    src = source(b, base, kind, code)
    srcs.append(src)
    arrays += binary("ps_cvt_b%d" % b, assemble(src), HASH | b << 8)
    for r in rows:
        assert row_bin[r][KIND_CLASS[kind]] == 0xFFFF, (r, kind)
        row_bin[r][KIND_CLASS[kind]] = b
open(os.path.join(ROOT, "shaders", "ps_cvt.s"), "w").write(
    "; OPCODE_TEST 5 passes, one binary after the other (shaders/ps_cvt_set.h). Generated by tools/gen_ps_cvt.py.\n"
    + "\n".join(srcs))
OPI = {n: i for i, n in enumerate(OPS)}
S = ["/* Generated by tools/gen_ps_cvt.py - do not edit. The OPCODE_TEST 5 passes: %d binaries (shaders/ps_cvt.s), "
     "each run in every float mode; %s. Hash %08X | binary << 8 | float mode (main.c, at upload). k_opt5_row_bin: "
     "the binary that keeps a row's result in each class (0xFFFF: none). */" % (len(bins), RSRC1, HASH),
     "#pragma once", "#define OPT5_BINS %d" % len(bins), "#define OPT5_CLASSES %d" % len(CLASSES),
     "#define OPT5_HASH 0x%08Xu" % HASH, "#define OPT5_KIND_COMPUTE 8 /* and 9: compute shaders */"]
S += arrays
S += ["static const char* const k_opt5_kind[%d] = {%s};" % (len(KINDS), ", ".join('"%s"' % k for k in KINDS)),
      "static const char* const k_opt5_class[OPT5_CLASSES] = {%s};" % ", ".join('"%s"' % c for c in CLASSES),
      "static const uint32_t* const k_opt5_bin[OPT5_BINS] = {%s};" % ", ".join(
          "ps_cvt_b%d" % b for b in range(len(bins))),
      "static const uint32_t k_opt5_bin_size[OPT5_BINS] = {%s};" % ", ".join(
          "sizeof(ps_cvt_b%d)" % b for b in range(len(bins))),
      "static const uint8_t k_opt5_bin_op[OPT5_BINS] = {%s}; /* the op's e32 form in src/optest5.h */" % ", ".join(
          str(OPI[base]) for base, kind, code, rows in bins),
      "static const uint8_t k_opt5_bin_kind[OPT5_BINS] = {%s};" % ", ".join(str(kind) for base, kind, c, r in bins),
      "static const uint8_t k_opt5_bin_class[OPT5_BINS] = {%s};" % ", ".join(
          str(KIND_CLASS[kind]) for base, kind, c, r in bins),
      "static const uint16_t k_opt5_row_bin[][OPT5_CLASSES] = {%s}; /* a row of src/optest5.h */" % ", ".join(
          "{%s}" % ", ".join("0x%X" % x for x in rb) for rb in row_bin)]
open(os.path.join(ROOT, "shaders", "ps_cvt_set.h"), "w").write("\n".join(S) + "\n")
print("shaders/ps_cvt.s, shaders/ps_cvt_set.h: %d binaries, %d bytes; %d rows; modifier encodings checked against "
      "llvm-mc: %d of %d" % (len(bins), 4 * sum(1 for l in arrays for w in l.split() if w.startswith("0x")), len(ROWS),
                             len(ok_llvm), len(chk)))
for c in range(len(CLASSES)):
    print("  class %d %-8s %4d rows" % (c, CLASSES[c], sum(1 for rb in row_bin if rb[c] != 0xFFFF)))
