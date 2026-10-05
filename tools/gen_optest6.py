#!/usr/bin/env python3
"""OPCODE_TEST 6: VOP3 output / input modifiers on hardware. omod (mul:2, mul:4, div:2), clamp with omod, neg / abs
on arithmetic and conversion sources, over f32 / f64 / f16 / integer results, in 20 float modes (every f32 x f64/f16
denormal mode, f32 rounding +inf / -inf / zero, f64 rounding to zero). Writes src/optest6.h (rows, labels, headings,
modes) and shaders/ps_mod.s (the pass; one pixel a slot runs every instruction and keeps the slot's).
python3 tools/gen_optest6.py"""
import os, struct, math
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
M = 0xFFFFFFFF
f32b = lambda x: struct.unpack('<I', struct.pack('<f', x))[0]
f64b = lambda x: struct.unpack('<Q', struct.pack('<d', x))[0]
def lab(x):
    if isinstance(x, tuple): return x[0]
    if math.isinf(x): return "+inf" if x > 0 else "-inf"
    if x == 0: return "-0" if math.copysign(1, x) < 0 else "0"
    if x == int(x) and abs(x) < 1e16: return "%d" % int(x)
    return np.format_float_scientific(np.float32(x), unique=True, trim='-') if abs(x) < 1e-4 or abs(x) >= 1e16 else \
           np.format_float_positional(np.float32(x), unique=True, trim='-')
def fb(x): return x[1] if isinstance(x, tuple) else f32b(x)
NAN = ("NaN 0x7FC12345", 0x7FC12345); NNAN = ("-NaN", 0xFFC00000); DEN = ("2^-127", f32b(2.0**-127)); NDEN = ("-2^-127", f32b(-2.0**-127))
TINY = ("1.5*2^-126", f32b(1.5 * 2.0**-126)); RND = ("3*2^-24", f32b(3 * 2.0**-24))
# instruction (writes v14 / v[14:15]; sources v9, v10, v11 or v[9:10]), heading, kind, 64-bit result
OPS = [
 ("v_add_f32_e64 v14, v9, v10", "V_ADD_F32", "b"), ("v_add_f32_e64 v14, v9, v10 mul:2", "V_ADD_F32 mul:2", "b"),
 ("v_add_f32_e64 v14, v9, v10 mul:4", "V_ADD_F32 mul:4", "b"), ("v_add_f32_e64 v14, v9, v10 div:2", "V_ADD_F32 div:2", "b"),
 ("v_mul_f32_e64 v14, v9, v10 mul:2", "V_MUL_F32 mul:2", "b"), ("v_max_f32_e64 v14, v9, v10 mul:2", "V_MAX_F32 mul:2", "b"),
 ("v_min_f32_e64 v14, v9, v10 div:2", "V_MIN_F32 div:2", "b"), ("v_mul_legacy_f32_e64 v14, v9, v10 mul:2", "V_MUL_LEGACY_F32 mul:2", "b"),
 ("v_mad_f32 v14, v9, v10, v11 mul:2", "V_MAD_F32 mul:2", "t"), ("v_fma_f32 v14, v9, v10, v11 div:2", "V_FMA_F32 div:2", "t"),
 ("v_rcp_f32_e64 v14, v9 mul:2", "V_RCP_F32 mul:2", "u"), ("v_sqrt_f32_e64 v14, v9 div:2", "V_SQRT_F32 div:2", "u"),
 ("v_fract_f32_e64 v14, v9 mul:4", "V_FRACT_F32 mul:4", "u"),
 ("v_cvt_f32_u32_e64 v14, v9 mul:4", "V_CVT_F32_U32 mul:4", "iu"), ("v_cvt_f32_f16_e64 v14, v9 mul:2", "V_CVT_F32_F16 mul:2", "h"),
 ("v_cvt_f32_f64_e64 v14, v[9:10] div:2", "V_CVT_F32_F64 div:2", "d"),
 ("v_mul_f32_e64 v14, v9, v10 clamp mul:2", "V_MUL_F32 clamp mul:2", "c"), ("v_add_f32_e64 v14, v9, v10 clamp div:2", "V_ADD_F32 clamp div:2", "c"),
 ("v_mad_f32 v14, v9, v10, v11 clamp", "V_MAD_F32 clamp", "c3"),
 ("v_add_f64 v[14:15], v[9:10], 1.0 mul:2", "V_ADD_F64 (+1.0) mul:2", "d64"), ("v_mul_f64 v[14:15], v[9:10], 0.5 div:2", "V_MUL_F64 (*0.5) div:2", "d64"),
 ("v_fma_f64 v[14:15], v[9:10], 1.0, 0 mul:4", "V_FMA_F64 (*1+0) mul:4", "d64"),
 ("v_mov_b32 v14, 0xdead0000\nv_cvt_f16_f32_e64 v14, v9 mul:2", "V_CVT_F16_F32 mul:2", "f16"),
 ("v_cvt_pkrtz_f16_f32_e64 v14, v9, v10 mul:2", "V_CVT_PKRTZ_F16_F32 mul:2", "pk"),
 ("v_cvt_i32_f32_e64 v14, v9 mul:2", "V_CVT_I32_F32 mul:2", "fi"), ("v_cvt_u32_f32_e64 v14, v9 div:2", "V_CVT_U32_F32 div:2", "fu"),
 ("v_add_f32_e64 v14, -v9, v10", "V_ADD_F32 (-src0)", "n1"), ("v_max_f32_e64 v14, |v9|, v10", "V_MAX_F32 (|src0|)", "n2"),
 ("v_mul_f32_e64 v14, -|v9|, v10", "V_MUL_F32 (-|src0|)", "n3"), ("v_cvt_f32_f16_e64 v14, -v9", "V_CVT_F32_F16 (-src)", "nh"),
 ("v_cvt_f32_f64_e64 v14, |v[9:10]|", "V_CVT_F32_F64 (|src|)", "nd")]
WIDE = {"d64"}
IN = {
 "b": [(3.0, 0.5), (TINY, 0.0), (3e38, 3e38), (NAN, 1.0), (-0.0, -0.0), (float('inf'), 1.0), (DEN, 0.0), (1.0, RND)],
 "t": [(3.0, 0.5, 1.0), (TINY, 1.0, 0.0), (3e38, 2.0, 0.0), (NAN, 1.0, 0.0), (-0.0, 1.0, -0.0), (1.0, 1.0, RND), (DEN, 1.0, 0.0)],
 "u": [(3.0,), (0.25,), (1.3,), (TINY,), (DEN,), (NAN,), (-0.0,), (float('inf'),), (3e38,)],
 "iu": [(("3", 3),), (("0xFFFFFFFF", M),), (("0x7FFFFFFF", 0x7FFFFFFF),)],
 "h": [(("0x3C00", 0x3C00),), (("0x0001", 0x0001),), (("0x7E00", 0x7E00),), (("0x7BFF", 0x7BFF),), (("0x8000", 0x8000),)],
 "c": [(0.6, 1.0), (-0.5, 1.0), (NAN, 1.0), (3.0, 0.5)], "c3": [(0.6, 1.0, 0.5), (-0.5, 1.0, 0.0), (NAN, 1.0, 0.0), (3.0, 0.5, 0.0)],
 "f16": [(1.0,), (40000.0,), (3e-5,), (NAN,)], "pk": [(1.0, 40000.0), (3e-5, NAN)],
 "fi": [(3.0,), (-2.5,), (1e10,)], "fu": [(7.0,), (1.0,)],
 "n1": [(NAN, 1.0), (DEN, 0.0), (1.0, 2.0)], "n2": [(NNAN, 1.0), (NDEN, 0.0), (-3.0, 1.0)], "n3": [(NAN, 1.0), (2.0, 3.0), (NDEN, 1.0)],
 "nh": [(("0x3C00", 0x3C00),), (("0x7E00", 0x7E00),), (("0x0001", 0x0001),), (("0xFFFF3C00", 0xFFFF3C00),)]}
D64 = {"d": [1.0, 1e-38, ("NaN", 0x7FF8000000000000), 3e38], "d64": [3.0, 1e-308, ("NaN 0x7FF8000012345678", 0x7FF8000012345678), -0.0],
       "nd": [("-NaN", 0xFFF8000000000000), -1.0, ("-dmin64", 0x8000000000000001)]}
MODES = [0x00, 0x20, 0x10, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01, 0x02, 0x03, 0x0C]
rows = []
for k, (ins, name, kind) in enumerate(OPS):
    if kind in D64:
        for v in D64[kind]:
            q = v[1] if isinstance(v, tuple) else f64b(v); l = v[0] if isinstance(v, tuple) else repr(v)
            rows.append((k, q & M, q >> 32, 0, l))
    else:
        for t in IN[kind]:
            w = [fb(x) for x in t] + [0] * (3 - len(t))
            rows.append((k, w[0], w[1], w[2], ", ".join(lab(x) for x in t)))
# main.c OPT5_LINE: labels and headings up to 64 characters
assert max(len(r["label"] if isinstance(r, dict) else r[4]) for r in rows) <= 64
def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
L = ["/* Generated by tools/gen_optest6.py - do not edit. OPCODE_TEST 6: VOP3 modifiers, %d rows {op, a, b, c} over %d"
     % (len(rows), len(OPS)),
     "   instructions (shaders/ps_mod.s; a, b = the f64 source), each in %d float modes. */" % len(MODES),
     "#pragma once", "#include <stdint.h>", "#define OPT5_ROWS %d" % len(rows), "#define OPT5_OPS %d" % len(OPS),
     "#define OPT5_MODES %d" % len(MODES), '#define OPT5_TITLE "VOP3 modifiers (GCN2)"',
     "static const char* const k_opt5_op[OPT5_OPS] = {" + ", ".join(cstr(n) for _, n, _ in OPS) + "};",
     "static const uint8_t k_opt5_wide[OPT5_OPS] = {" + ", ".join("1" if kd in WIDE else "0" for _, _, kd in OPS) + "};",
     "/* PGM_RSRC1 FLOAT_MODE per pass: [7:6] f64/f16 denormals, [5:4] f32 denormals, [3:2] f64 rounding, [1:0] f32 */",
     "static const uint8_t k_opt5_float_mode[OPT5_MODES] = {" + ", ".join("0x%02X" % m for m in MODES) + "};",
     "static const uint32_t k_opt5_row[OPT5_ROWS][4] = {"]
for r in rows: L.append("    {%d, 0x%08Xu, 0x%08Xu, 0x%08Xu}, /* %s %s */" % (r[0], r[1], r[2], r[3], OPS[r[0]][1], r[4]))
L.append("};")
L.append("static const char* const k_opt5_label[OPT5_ROWS] = {")
for r in rows: L.append("    %s," % cstr(r[4]))
L.append("};")
open(os.path.join(ROOT, "src", "optest6.h"), "w").write("\n".join(L) + "\n")
S = ["; OPCODE_TEST 6 modifier pass: one pixel per table slot (x = slot). Table (s[0:1]): [0..3] V# of the slots",
     "; {op, a, b, c} (op bits 7..0; 0xff: a heading), [4..7] V# of the results ([11] bytes a slot: lo, hi a float mode),",
     "; [8] this pass's byte offset in a result (mode * 8), [9] slot count, [10] marker. Runs the instructions of",
     "; tools/gen_optest6.py on the slot's operands, keeps the slot's; FLOAT_MODE from PGM_RSRC1. Slot 0 stores the",
     "; marker at [slot count * [11] + mode * 4]. [12] instruction limit: op k runs only if k < [12] (main.c's",
     "; start-up bisection). Generated by tools/gen_optest6.py.",
     "s_load_dwordx8 s[4:11], s[0:1], 0x0", "s_load_dwordx4 s[12:15], s[0:1], 0x8", "s_load_dword s22, s[0:1], 0xc", "s_waitcnt lgkmcnt(0)",
     "v_cvt_u32_f32 v4, v2", "v_cmp_gt_u32 vcc, s13, v4", "s_and_saveexec_b64 s[16:17], vcc", "s_cbranch_execz mod_done",
     "v_lshlrev_b32 v5, 4, v4", "buffer_load_dwordx4 v[8:11], v5, s[4:7], 0 offen", "s_waitcnt vmcnt(0)",
     "v_and_b32 v8, 0xff, v8", "v_mov_b32 v12, 0", "v_mov_b32 v13, 0"]
for k, (ins, name, kind) in enumerate(OPS):
    S += ["; op %d: %s" % (k, name), "s_cmp_le_u32 s22, %d" % k, "s_cbranch_scc1 op%d_skip" % k] + ins.split("\n")
    S += ["v_cmp_eq_u32 vcc, %d, v8" % k, "v_cndmask_b32 v12, v12, v14, vcc"]
    if kind in WIDE: S.append("v_cndmask_b32 v13, v13, v15, vcc")
    S.append("op%d_skip:" % k)
S += ["v_mul_u32_u24 v6, s15, v4", "v_add_i32 v6, vcc, s12, v6", "buffer_store_dwordx2 v[12:13], v6, s[8:11], 0 offen",
      "v_cmp_eq_u32 vcc, 0, v4", "s_and_saveexec_b64 s[18:19], vcc", "s_cbranch_execz mod_nomark",
      "s_mul_i32 s20, s13, s15", "s_lshr_b32 s21, s12, 1", "s_add_u32 s20, s20, s21", "v_mov_b32 v7, s20", "v_mov_b32 v16, s14",
      "buffer_store_dword v16, v7, s[8:11], 0 offen", "mod_nomark:", "s_mov_b64 exec, s[18:19]", "mod_done:",
      "s_mov_b64 exec, s[16:17]", "v_mov_b32 v0, 0", "exp mrt0 v0, v0, v0, v0 done vm", "s_endpgm"]
open(os.path.join(ROOT, "shaders", "ps_mod.s"), "w").write("\n".join(S) + "\n")
print("src/optest6.h: %d rows, %d instructions, %d float modes; shaders/ps_mod.s" % (len(rows), len(OPS), len(MODES)))
