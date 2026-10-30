#!/usr/bin/env python3
"""OPCODE_TEST 5 conversion passes (src/optest5.h numbering). Writes shaders/ps_cvt*.s, shaders/cs_cvt.s, one header
per kind (shaders/ps_cvt*.h, shaders/cs_cvt.h; main.c gives each float mode its own hash at upload) and
shaders/ps_cvt_set.h (the kinds, their binaries and which rows each one runs).

Kinds, each a pass per float mode; every row's result is kept per kind and compared with kind 0 in the log:
  0 VGPR      e32 encoding, sources in VGPRs; modifier ops (name "OP (mods)") as e64 with the modifier bits
  1 e64       the e32 rows (ops without modifiers) in the VOP3 encoding, sources in VGPRs
  2 SGPR      the e32 rows with src0 in s[24:25] (s_mov_b32 of the row's value)
  3 literal   the e32 rows with src0 a literal dword (forced, also where an inline constant exists; an f64 source
              only when its low dword is 0: the literal is the high dword)
  4 inline    the e32 rows whose src0 has an inline constant (integer 0..64, -1..-16 or +-0.5, 1, 2, 4)
  5 store     kind 0's code, results written by two buffer_store_dword instead of buffer_store_dwordx2
  6 compute   kind 0's code as a compute shader (slot = thread group * 64 + thread)
Kinds 2..4 bake each row's value into the code (one block per row, kept when the slot's row id matches).
Leaves out V_CVT_U32_F64 and V_CVT_PKACCUM_U8_F32 (no shadPS4 translator: its assert stops the shader).
V_CVT_OFF_F32_I4 takes immediate operands only (shadPS4 asserts on a register): one instruction per value of its
rows, kept for its op with that a; kind 2 leaves it out. Needs llvm-mc (AMDGPU target).
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
KINDS = ["VGPR", "e64", "SGPR", "literal", "inline", "store", "compute"]
KIND_FILE = ["ps_cvt", "ps_cvt_e64", "ps_cvt_sgpr", "ps_cvt_lit", "ps_cvt_inl", "ps_cvt_st2", "cs_cvt"]
PS_KINDS = 6
HASH = 0xCAFE7000  # | kind << 8 | mode, set by main.c at upload
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
    if e64 and mods: return []
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

HEAD = ["; OPCODE_TEST 5 conversion pass, kind %s (tools/gen_ps_cvt.py): one %s per table slot. Table (s[0:1]):",
        "; [0..3] V# of the slots {op | 64-bit result << 8 | row + 1 << 16, a, b, c} (op 0xff: a heading), [4..7] V# of",
        "; the results ([11] bytes a slot: lo, hi a pass), [8] this pass's byte offset in a result, [9] slot count,",
        "; [10] marker. Runs the V_CVT_* on the slot's operands and keeps the slot's (a, b = an f64 source; c = PK_U8's",
        "; S2); FLOAT_MODE from PGM_RSRC1. Slot 0 stores the marker at [slot count * [11] + [8] / 2]."]

def source(kind_ix):
    cs = kind_ix == 6
    L = [l % (KINDS[kind_ix], "thread" if cs else "pixel (x + 512 y)") if i == 0 else l for i, l in enumerate(HEAD)]
    L += ["; Generated by tools/gen_ps_cvt.py.",
          "s_load_dwordx8 s[4:11], s[0:1], 0x0", "s_load_dwordx4 s[12:15], s[0:1], 0x8"]
    if cs:
        L += ["s_lshl_b32 s3, s2, 6", "v_add_i32 v4, vcc, s3, v0"]
    else:
        L += ["v_cvt_u32_f32 v4, v2", "v_cvt_u32_f32 v5, v3", "v_lshlrev_b32 v5, 9, v5", "v_add_i32 v4, vcc, v4, v5"]
    L += ["s_waitcnt lgkmcnt(0)", "v_cmp_gt_u32 vcc, s13, v4", "s_and_saveexec_b64 s[16:17], vcc",
          "s_cbranch_execz cvt_done", "v_lshlrev_b32 v5, 4, v4", "buffer_load_dwordx4 v[8:11], v5, s[4:7], 0 offen",
          "s_waitcnt vmcnt(0)", "v_lshrrev_b32 v17, 16, v8", "v_and_b32 v8, 0xff, v8", "v_mov_b32 v12, 0",
          "v_mov_b32 v13, 0"]
    if kind_ix in (2, 3, 4):
        for r in range(len(ROWS)): L += row_block(r, kind_ix)
    else:
        for k in range(len(OPS)): L += op_block(k, kind_ix == 1)
    L += ["v_mul_u32_u24 v6, s15, v4", "v_add_i32 v6, vcc, s12, v6"]
    if kind_ix == 5:
        L += ["buffer_store_dword v12, v6, s[8:11], 0 offen", "buffer_store_dword v13, v6, s[8:11], 0 offen offset:4"]
    else:
        L.append("buffer_store_dwordx2 v[12:13], v6, s[8:11], 0 offen")
    L += ["v_cmp_eq_u32 vcc, 0, v4", "s_and_saveexec_b64 s[18:19], vcc", "s_cbranch_execz cvt_nomark",
          "s_mul_i32 s20, s13, s15", "s_lshr_b32 s21, s12, 1", "s_add_u32 s20, s20, s21", "v_mov_b32 v7, s20",
          "v_mov_b32 v16, s14", "buffer_store_dword v16, v7, s[8:11], 0 offen", "cvt_nomark:",
          "s_mov_b64 exec, s[18:19]", "cvt_done:", "s_mov_b64 exec, s[16:17]"]
    if not cs: L += ["v_mov_b32 v0, 0", "exp mrt0 v0, v0, v0, v0 done vm"]
    L.append("s_endpgm")
    return "\n".join(L) + "\n"

def rows_of(kind_ix):
    """Rows the kind runs (bit kind_ix of k_opt5_row_kinds)."""
    if kind_ix in (0, 5, 6): return set(range(len(ROWS)))
    if kind_ix == 1:
        return {r for r, (k, a, b, c) in enumerate(ROWS) if split(OPS[k])[0] in BASE and not split(OPS[k])[1] and
                (split(OPS[k])[0] != OFF or inline_code(a, False) is not None)}
    return {r for r in range(len(ROWS)) if row_block(r, kind_ix)}

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

def header(name, src, code, h):
    if len(code) % 2: code = code + [0xBF800000]  # s_nop: even dword count after the SDK token
    code = [0xBEEB03FF, len(code) // 2] + code    # token, n / 2 - 1 with n = dwords incl. the token
    L = ["#pragma once", "#include <stdint.h>", ""]
    for l in src.split("\n"):
        if not l.startswith(";"): break
        if not l.startswith("; Generated"): L.append("//" + l[1:])
    L += ["// %s. Hash %08X (main.c adds the float mode)." % (RSRC1, h),
          "static const uint32_t %s_binary[] __attribute__((aligned(256))) = {" % name]
    for i in range(0, len(code), 6): L.append("    " + " ".join("0x%08X," % w for w in code[i:i + 6]))
    L.append("    " + " ".join("0x%08X," % w for w in (0x5362724F, 0x00726468, len(code) * 4 << 8, 0, 0xDEADBEEF, h, 0)))
    L.append("};")
    return "\n".join(L) + "\n"

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

files = []
mask = [0] * len(ROWS)
for ki in range(len(KINDS)):
    src = source(ki)
    code = assemble(src)
    open(os.path.join(ROOT, "shaders", KIND_FILE[ki] + ".s"), "w").write(src)
    open(os.path.join(ROOT, "shaders", KIND_FILE[ki] + ".h"), "w").write(
        header(KIND_FILE[ki], src, code, HASH | ki << 8))
    for r in rows_of(ki): mask[r] |= 1 << ki
    files.append((KIND_FILE[ki], len(code)))
S = ["/* Generated by tools/gen_ps_cvt.py - do not edit. OPCODE_TEST 5 passes: kinds 0..%d pixel shaders (ps_cvt*.h), "
     "kind %d the compute shader (cs_cvt.h); k_opt5_row_kinds: bit k = kind k runs the row. */" % (PS_KINDS - 1, PS_KINDS),
     "#pragma once", "#define OPT5_KINDS %d" % len(KINDS), "#define OPT5_PS_KINDS %d" % PS_KINDS,
     "#define OPT5_HASH 0x%08Xu" % HASH,
     "static const char* const k_opt5_kind[OPT5_KINDS] = {%s};" % ", ".join('"%s"' % k for k in KINDS),
     "static const uint32_t* const k_opt5_kind_bin[OPT5_KINDS] = {%s};" % ", ".join(
         "%s_binary" % f for f in KIND_FILE),
     "static const uint32_t k_opt5_kind_size[OPT5_KINDS] = {%s};" % ", ".join(
         "sizeof(%s_binary)" % f for f in KIND_FILE),
     "static const uint8_t k_opt5_row_kinds[] = {%s}; /* a row of src/optest5.h */" % ", ".join(
         "0x%02X" % m for m in mask)]
open(os.path.join(ROOT, "shaders", "ps_cvt_set.h"), "w").write("\n".join(S) + "\n")
for f, n in files: print("shaders/%s.s / .h: %d dwords" % (f, n))
print("shaders/ps_cvt_set.h: %d rows; modifier encodings checked against llvm-mc: %d of %d" % (
    len(ROWS), len(ok_llvm), len(chk)))
for ki in range(len(KINDS)): print("  kind %d %-8s %4d rows" % (ki, KINDS[ki], sum(1 for m in mask if m >> ki & 1)))
