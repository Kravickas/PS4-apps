#!/usr/bin/env python3
"""Generates shaders/ps_bfm64.s and shaders/ps_bfm64.h: the S_BFM_B64 hardware test (OPCODE_TEST 6).

One full-screen pass over the frame: every row runs its scalar sequence (inputs from the table, so
nothing is a compile-time constant), stores {lo, hi, SCC, marker} for the trace from one lane, and
the pixel picks its row's result for the bit grid (bit 63 left, white = 1; the last column is SCC).

Table (dwords): 0 X0, 1 Y0, 2 1 / cell width, 3 1 / row height, 4 first row of the page, 5 rows on
the page - 1 (floats); 6, 7 the logging pixel x, y; 8..11 the result V#; 12 the marker base;
16..23 the labels' T# (the UI buffer: premultiplied, ps_ui's format), 24..27 its point S#;
32 + 2 k: row k's inputs (s2, s3). Results: 16 bytes per row.

Needs llvm-mc-18 (-mcpu=bonaire). Run from the project root."""
import os, struct, subprocess, sys

ROWS = []  # (group, label, in0, in1, scc_in, body, log_text)


def row(group, label, in0, in1, body, text, scc_in=None):
    ROWS.append([group, label, in0 & 0xFFFFFFFF, in1 & 0xFFFFFFFF, scc_in, body, text])


BFM = ["s_bfm_b64 s[28:29], s2, s3"]
CAP = "s_cselect_b32 s30, 1, 0"

# A: width sweep, offset 0 (SGPR sources)
for w in range(64):
    row("A", "w %d, o 0" % w, w, 0, BFM, "s_bfm_b64 s[28:29], s2, s3")
# B: offset sweep, width 1
for o in range(64):
    row("B", "w 1, o %d" % o, 1, o, BFM, "s_bfm_b64 s[28:29], s2, s3")
# C: bits above [5:0] of either source
for w, o in [(64, 0), (65, 0), (0x60, 0), (0x7F, 0), (0xFFFFFFE0, 0), (0xFFFFFFFF, 0),
             (0x80000000, 0), (0x80000001, 0), (0x20, 0x40), (0x20, 0x41), (0x20, 0xFFFFFFE0),
             (1, 0xFFFFFFFF), (1, 0x80000020), (8, 0x7FFFFFE1)]:
    row("C", "w 0x%X, o 0x%X" % (w, o), w, o, BFM, "s_bfm_b64 s[28:29], s2, s3")
# D: width + offset past 64
for w, o in [(63, 1), (40, 40), (63, 63), (32, 32), (1, 63), (2, 63), (33, 31), (31, 33),
             (48, 16)]:
    row("D", "w %d, o %d" % (w, o), w, o, BFM, "s_bfm_b64 s[28:29], s2, s3")
# E: source encodings
INL = [0, 1, 31, 32, 33, 63, 64, -1, -15, -16]
for c in INL:
    op = "s_bfm_b64 s[28:29], %d, s3" % c
    row("E", "S0 inline %d, o s3 0" % c, 0, 0, [op], op)
for c in INL:
    op = "s_bfm_b64 s[28:29], s2, %d" % c
    row("E", "w s2 8, S1 inline %d" % c, 8, 0, [op], op)
FLT = ["0.5", "-0.5", "1.0", "-1.0", "2.0", "-2.0", "4.0", "-4.0"]
for f in FLT:
    op = "s_bfm_b64 s[28:29], %s, s3" % f
    row("E", "S0 inline %s, o s3 0" % f, 0, 0, [op], op)
for f in FLT:
    op = "s_bfm_b64 s[28:29], s2, %s" % f
    row("E", "w s2 4, S1 inline %s" % f, 4, 0, [op], op)
for op, lab, a, b in [("s_bfm_b64 s[28:29], 0x67, s3", "S0 literal 0x67, o s3 0", 0, 0),
                      ("s_bfm_b64 s[28:29], 0xffffffe5, s3", "S0 literal 0xFFFFFFE5, o 0", 0, 0),
                      ("s_bfm_b64 s[28:29], s2, 0x6a", "w s2 8, S1 literal 0x6A", 8, 0),
                      ("s_bfm_b64 s[28:29], s2, 0xffffff61", "w s2 8, S1 literal 0xFFFFFF61", 8, 0),
                      ("s_bfm_b64 s[28:29], 0x61, 0x61", "S0 = S1 = literal 0x61", 0, 0),
                      ("s_bfm_b64 s[28:29], s2, s2", "S0 = S1 = s2 16", 16, 0),
                      ("s_bfm_b64 s[28:29], s2, s2", "S0 = S1 = s2 40", 40, 0)]:
    row("E", lab, a, b, [op], op)
row("E", "S0 m0 40, o s3 0", 40, 0, ["s_mov_b32 m0, s2", "s_bfm_b64 s[28:29], m0, s3"],
    "s_mov_b32 m0, s2; s_bfm_b64 s[28:29], m0, s3")
row("E", "w s2 24, S1 m0 9", 24, 9, ["s_mov_b32 m0, s3", "s_bfm_b64 s[28:29], s2, m0"],
    "s_mov_b32 m0, s3; s_bfm_b64 s[28:29], s2, m0")
row("E", "S0 vcc_lo 12, S1 vcc_hi 20", 12, 20,
    ["s_mov_b64 vcc, s[2:3]", "s_bfm_b64 s[28:29], vcc_lo, vcc_hi"],
    "s_mov_b64 vcc, s[2:3]; s_bfm_b64 s[28:29], vcc_lo, vcc_hi")
row("E", "S0 vcc_hi 20, S1 vcc_lo 12", 12, 20,
    ["s_mov_b64 vcc, s[2:3]", "s_bfm_b64 s[28:29], vcc_hi, vcc_lo"],
    "s_mov_b64 vcc, s[2:3]; s_bfm_b64 s[28:29], vcc_hi, vcc_lo")
for scc in (1, 0):
    row("E", "S0 SCC %d, o s3 5" % scc, 0, 5, ["s_bfm_b64 s[28:29], src_scc, s3"],
        "s_bfm_b64 s[28:29], scc, s3", scc_in=scc)
for scc in (1, 0):
    row("E", "w s2 3, S1 SCC %d" % scc, 3, 0, ["s_bfm_b64 s[28:29], s2, src_scc"],
        "s_bfm_b64 s[28:29], s2, scc", scc_in=scc)
EXS = ["s_mov_b64 exec, s[2:3]", "s_bfm_b64 s[28:29], exec_lo, exec_hi", CAP,
       "s_mov_b64 exec, s[32:33]"]
row("E", "S0 exec_lo 0x3C, S1 exec_hi 0x7", 0x3C, 0x7, EXS,
    "s_mov_b64 exec, s[2:3]; s_bfm_b64 s[28:29], exec_lo, exec_hi")
row("E", "S0 exec_lo 0xFFFFFFE9, S1 exec_hi 0x2", 0xFFFFFFE9, 0x2, EXS,
    "s_mov_b64 exec, s[2:3]; s_bfm_b64 s[28:29], exec_lo, exec_hi")
# F: destinations, overlap, dependency
row("F", "dst vcc, w 40, o 8", 40, 8, ["s_bfm_b64 vcc, s2, s3", CAP, "s_mov_b64 s[28:29], vcc"],
    "s_bfm_b64 vcc, s2, s3")
for w, o in [(40, 8), (63, 1), (64, 0)]:
    row("F", "dst exec, w %d, o %d" % (w, o), w, o,
        ["s_bfm_b64 exec, s2, s3", CAP, "s_mov_b64 s[28:29], exec", "s_mov_b64 exec, s[32:33]"],
        "s_bfm_b64 exec, s2, s3")
row("F", "s[28:29] <- s28 20, s29 10", 20, 10,
    ["s_mov_b64 s[28:29], s[2:3]", "s_bfm_b64 s[28:29], s28, s29"],
    "s_bfm_b64 s[28:29], s28, s29")
row("F", "s[28:29] <- s29 10, s28 20", 20, 10,
    ["s_mov_b64 s[28:29], s[2:3]", "s_bfm_b64 s[28:29], s29, s28"],
    "s_bfm_b64 s[28:29], s29, s28")
row("F", "game: s[28:29] <- s28 37, 0", 37, 0xDEADBEEF,
    ["s_mov_b64 s[28:29], s[2:3]", "s_bfm_b64 s[28:29], s28, 0"],
    "s_bfm_b64 s[28:29], s28, 0  (s29 was 0xDEADBEEF)")
row("F", "result -> next: w hi, o lo", 36, 4,
    ["s_bfm_b64 s[34:35], s2, s3", "s_bfm_b64 s[28:29], s35, s34"],
    "s_bfm_b64 s[34:35], s2, s3; s_bfm_b64 s[28:29], s35, s34")
# G: the game's chain (Marvel's Avengers fs 0x6d179caa)
for w in (0, 1, 32, 63, 64):
    row("G", "and x,x,x  w %d" % w, w, 0,
        ["s_bfm_b64 s[34:35], s2, 0", "s_and_b64 s[34:35], s[34:35], s[34:35]", CAP,
         "s_mov_b64 s[28:29], s[34:35]"],
        "s_bfm_b64 x, s2, 0; s_and_b64 x, x, x  (SCC)")
LANE = ["s_mov_b32 s36, -1", "s_mov_b32 s37, -1", "s_mov_b64 exec, s[36:37]",
        "v_mbcnt_lo_u32_b32_e64 v19, -1, 0", "v_mbcnt_hi_u32_b32_e32 v19, -1, v19",
        "v_cmp_gt_u32_e32 vcc, s3, v19"]
for w, k in [(40, 20), (20, 40), (63, 64), (64, 64), (33, 48)]:
    row("G", "bfm w %d & vcc(lane < %d)" % (w, k), w, k,
        LANE + ["s_bfm_b64 s[34:35], s2, 0", "s_and_b64 s[28:29], s[34:35], vcc", CAP,
                "s_mov_b64 exec, s[32:33]"],
        "exec = ~0; vcc = lane < s3; s_bfm_b64 x, s2, 0; s_and_b64 r, x, vcc")
for w, o in [(1, 0), (5, 31), (5, 32), (1, 63), (64, 0)]:
    row("G", "ff1(bfm w %d, o %d)" % (w, o), w, o,
        ["s_bfm_b64 s[34:35], s2, s3", "s_ff1_i32_b64 s28, s[34:35]", CAP, "s_mov_b32 s29, 0"],
        "s_bfm_b64 x, s2, s3; s_ff1_i32_b64 r, x  (hi 0)")
for w, o in [(8, 0), (40, 32), (3, 62)]:
    row("G", "bitset0(bfm w %d o %d, ff1)" % (w, o), w, o,
        ["s_bfm_b64 s[34:35], s2, s3", "s_ff1_i32_b64 s36, s[34:35]",
         "s_bitset0_b64 s[34:35], s36", CAP, "s_mov_b64 s[28:29], s[34:35]"],
        "s_bfm_b64 x, s2, s3; s_ff1_i32_b64 i, x; s_bitset0_b64 x, i")
LOOPS = [(0, 0), (1, 0), (7, 0), (32, 0), (33, 0), (63, 0), (64, 0), (8, 56)]
for w, o in LOOPS:
    row("G", "loop SCC  w %d, o %d" % (w, o), w, o, ("LOOP_A",),
        "loop A: s_and_b64 x,x,x; s_cbranch_scc0; ff1; bitset0  (lo count, hi index sum)")
for w, o in LOOPS:
    row("G", "loop VCCZ w %d, o %d" % (w, o), w, o, ("LOOP_B",),
        "loop B: v_cmp_ne_u64 vcc, x, 0; s_cbranch_vccz; ff1; bitset0  (lo count, hi sum)")

# H: one literal dword feeding several operands (SOPC, SOP2, VOP2 madak / madmk), and literal /
# inline constant expansion for 64-bit operands
Z = ["s_mov_b32 s28, 0", "s_mov_b32 s29, 0"]
for op in ["s_cmp_eq_u32 0x61, 0x61", "s_cmp_lg_u32 0x61, 0x61", "s_cmp_lt_u32 0x61, 0x61",
           "s_cmp_gt_u32 0x12c, 0x12c", "s_cmp_lt_i32 0xffffff00, 0xffffff00",
           "s_bitcmp1_b32 0x80000005, 0x80000005", "s_bitcmp0_b32 0x80000005, 0x80000005",
           "s_bitcmp1_b64 0x80000005, 0x80000005", "s_bitcmp1_b64 0x80000025, 0x80000025"]:
    row("H", op.split()[0] + " lit, lit", 0, 0, [op, CAP] + Z, op)
for op, scc in [("s_add_u32 s28, 0x12c, 0x12c", None), ("s_sub_u32 s28, 0x12c, 0x12c", None),
                ("s_xor_b32 s28, 0x12c, 0x12c", None), ("s_cselect_b32 s28, 0x12c, 0x12c", 0),
                ("s_cselect_b32 s28, 0x12c, 0x12c", 1), ("s_lshl_b32 s28, 0x61, 0x61", None),
                ("s_mul_i32 s28, 0x12c, 0x12c", None), ("s_min_u32 s28, 0x12c, 0x12c", None),
                ("s_max_i32 s28, 0xffffff00, 0xffffff00", None)]:
    row("H", op.split()[0] + " lit, lit", 0, 0, [op, CAP, "s_mov_b32 s29, 0"], op, scc_in=scc)
for op in ["s_and_b64 s[28:29], 0x80000005, 0x80000005",
           "s_and_b64 s[28:29], 0x12345678, 0x12345678",
           "s_lshl_b64 s[28:29], 0x80000005, 0x80000005", "s_mov_b64 s[28:29], 0x80000005",
           "s_mov_b64 s[28:29], 0x12345678", "s_not_b64 s[28:29], 0x80000005",
           "s_mov_b64 s[28:29], -1", "s_mov_b64 s[28:29], -16", "s_mov_b64 s[28:29], 1.0"]:
    row("H", " ".join(op.split()[0:1] + op.split()[2:]), 0, 0, [op, CAP], op)
for op, lab in [("v_madak_f32 v19, 0x40400000, v34, 0x40400000", "madak S0 = K = 3.0, v 2.0"),
                ("v_madmk_f32 v19, 0x40400000, 0x40400000, v34", "madmk S0 = K = 3.0, v 2.0"),
                ("v_madak_f32 v19, v35, v34, 0x40400000", "madak S0 v 3.0, K 3.0, v 2.0")]:
    row("H", lab, 0x40000000, 0x40400000,
        ["v_mov_b32 v34, s2", "v_mov_b32 v35, s3", op, "v_readfirstlane_b32 s28, v19", CAP,
         "s_mov_b32 s29, 0"],
        op + "; v_readfirstlane_b32 s28, v19")

# I: EXEC idioms. EXEC is forced to all 64 lanes and v19 = lane id, vcc = lane < s3, then the
# idiom runs a VALU write (v34) under its EXEC; with EXEC back to all lanes, the result is the
# mask of lanes that wrote (v34 == the value given). No loops: a wrong EXEC model cannot hang.
ALL = ["s_mov_b32 s36, -1", "s_mov_b32 s37, -1", "s_mov_b64 exec, s[36:37]",
       "v_mbcnt_lo_u32_b32_e64 v19, -1, 0", "v_mbcnt_hi_u32_b32_e32 v19, -1, v19",
       "v_mov_b32 v34, 0", "v_cmp_gt_u32_e32 vcc, s3, v19"]


def obs(value=None):
    cmp = "v_cmp_ne_u32_e32 vcc, 0, v34" if value is None else \
        "v_cmp_eq_u32_e32 vcc, %d, v34" % value
    return [CAP, "s_mov_b64 exec, s[36:37]", cmp, "s_mov_b64 s[28:29], vcc",
            "s_mov_b64 exec, s[32:33]"]


W = "v_mov_b32 v34, 1"
for lab, k, k2, idiom, value in [
        ("exec all, write", 12, 0, [W], None),
        ("s_and_saveexec_b64 vcc", 12, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", W, "s_mov_b64 exec, s[38:39]"], None),
        ("s_and_saveexec_b64 vcc, none", 0, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", W, "s_mov_b64 exec, s[38:39]"], None),
        ("s_and_saveexec_b64 vcc, lane < 48", 48, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", W, "s_mov_b64 exec, s[38:39]"], None),
        ("s_mov_b64 exec, vcc", 12, 0, ["s_mov_b64 exec, vcc", W], None),
        ("s_and_b64 exec, exec, vcc", 12, 0, ["s_and_b64 exec, exec, vcc", W], None),
        ("s_andn2_b64 exec, exec, vcc", 12, 0, ["s_andn2_b64 exec, exec, vcc", W], None),
        ("s_mov_b64 exec 0; s_or_b64 exec, vcc", 12, 0,
         ["s_mov_b64 exec, 0", "s_or_b64 exec, exec, vcc", W], None),
        ("s_xor_b64 exec, exec, vcc", 12, 0, ["s_xor_b64 exec, exec, vcc", W], None),
        ("s_not_b64 exec, vcc", 12, 0, ["s_not_b64 exec, vcc", W], None),
        ("v_cmpx_gt_u32 (lane < 12)", 12, 0, ["v_cmpx_gt_u32_e32 vcc, s3, v19", W], None),
        ("and s, exec, vcc; scc0; mov exec", 12, 0,
         ["s_and_b64 s[38:39], exec, vcc", "s_cbranch_scc0 skip_{K}",
          "s_mov_b64 exec, s[38:39]", W, "skip_{K}:"], None),
        ("and s, exec, vcc; scc0; none", 0, 0,
         ["s_and_b64 s[38:39], exec, vcc", "s_cbranch_scc0 skip_{K}",
          "s_mov_b64 exec, s[38:39]", W, "skip_{K}:"], None),
        ("mov exec, vcc; execz", 12, 0,
         ["s_mov_b64 exec, vcc", "s_cbranch_execz skip_{K}", W, "skip_{K}:"], None),
        ("mov exec, vcc; execz; none", 0, 0,
         ["s_mov_b64 exec, vcc", "s_cbranch_execz skip_{K}", W, "skip_{K}:"], None),
        ("vccz branch, all lanes", 12, 0, ["s_cbranch_vccz skip_{K}", W, "skip_{K}:"], None),
        ("vccnz branch, all lanes", 12, 0, ["s_cbranch_vccnz skip_{K}", W, "skip_{K}:"], None),
        ("if/else: then lanes", 12, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", "s_xor_b64 s[38:39], exec, s[38:39]", W,
          "s_mov_b64 exec, s[38:39]", "v_mov_b32 v34, 2"], 1),
        ("if/else: else lanes", 12, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", "s_xor_b64 s[38:39], exec, s[38:39]", W,
          "s_mov_b64 exec, s[38:39]", "v_mov_b32 v34, 2"], 2),
        ("nested saveexec 20, 8", 20, 8,
         ["s_and_saveexec_b64 s[38:39], vcc", "v_cmp_gt_u32_e32 vcc, s2, v19",
          "s_and_saveexec_b64 s[40:41], vcc", W, "s_mov_b64 exec, s[40:41]",
          "s_mov_b64 exec, s[38:39]"], None),
        ("nested saveexec 8, 20", 8, 20,
         ["s_and_saveexec_b64 s[38:39], vcc", "v_cmp_gt_u32_e32 vcc, s2, v19",
          "s_and_saveexec_b64 s[40:41], vcc", W, "s_mov_b64 exec, s[40:41]",
          "s_mov_b64 exec, s[38:39]"], None),
        ("write after scope restore", 12, 0,
         ["s_and_saveexec_b64 s[38:39], vcc", W, "s_mov_b64 exec, s[38:39]",
          "v_mov_b32 v34, 2"], 2)]:
    row("I", lab, k2, k, ALL + idiom + obs(value),
        "exec ~0, vcc = lane < s3; " + "; ".join(i for i in idiom if not i.endswith(":")))
row("I", "vcc from v_cmp under exec lane < 12", 0, 12,
    ALL + ["s_and_saveexec_b64 s[38:39], vcc", "v_cmp_gt_u32_e32 vcc, 64, v19",
           "s_mov_b64 s[28:29], vcc", CAP, "s_mov_b64 exec, s[32:33]"],
    "exec = lane < s3 (saveexec); v_cmp_gt_u32 vcc, 64, lane  (vcc only active lanes)")

N = len(ROWS)
MARK = 0xB6400000  # result marker base: dword 3 of row k = MARK + k

asm = []
e = asm.append
e("s_mov_b32 vcc_hi, HDRLEN")
e("s_load_dwordx8 s[4:11], s[0:1], 0x0")
e("s_load_dwordx4 s[12:15], s[0:1], 0x8")
e("s_load_dword s16, s[0:1], 0xc")
e("s_load_dwordx8 s[40:47], s[0:1], 0x10")
e("s_load_dwordx4 s[48:51], s[0:1], 0x18")
e("s_waitcnt lgkmcnt(0)")
e("s_mov_b64 s[32:33], exec")
e("v_subrev_f32 v4, s4, v2")
e("v_subrev_f32 v5, s5, v3")
e("v_mul_f32 v6, s6, v4")
e("v_mul_f32 v7, s7, v5")
e("v_floor_f32 v8, v6")
e("v_floor_f32 v9, v7")
e("v_sub_f32 v10, v6, v8")
e("v_sub_f32 v11, v7, v9")
e("v_sub_f32 v13, 0x42820000, v8")  # 65 - col
e("v_min_f32 v13, v13, v8")
e("v_min_f32 v13, v13, v9")
e("v_sub_f32 v14, s9, v9")          # rows-1 - row
e("v_min_f32 v13, v13, v14")
e("v_cmp_le_f32_e64 s[20:21], 0, v13")
e("v_cvt_u32_f32 v15, v2")
e("v_cvt_u32_f32 v16, v3")
e("v_cmp_eq_u32_e64 s[22:23], s10, v15")
e("v_cmp_eq_u32_e64 s[24:25], s11, v16")
e("s_and_b64 s[22:23], s[22:23], s[24:25]")
e("v_add_f32 v17, s8, v9")
e("v_cvt_u32_f32 v17, v17")
e("v_cvt_u32_f32 v18, v8")
e("v_mov_b32 v20, 0")
e("v_mov_b32 v21, 0")
e("v_mov_b32 v22, 0")

for k, (g, lab, a, b, scc_in, body, text) in enumerate(ROWS):
    e("; row %d (%s) %s" % (k, g, lab))
    e("s_load_dwordx2 s[2:3], s[0:1], 0x%x" % (32 + 2 * k))
    e("s_waitcnt lgkmcnt(0)")
    if scc_in is None:
        scc_in = 1 if k % 2 == 0 else 0
    ROWS[k][4] = scc_in
    e("s_cmp_eq_u32 0, %d" % (0 if scc_in else 1))
    if body == ("LOOP_A",) or body == ("LOOP_B",):
        e("s_bfm_b64 s[34:35], s2, s3")
        e("s_mov_b32 s28, 0")
        e("s_mov_b32 s29, 0")
        e("loop_%d:" % k)
        if body == ("LOOP_A",):
            e("s_and_b64 s[34:35], s[34:35], s[34:35]")
            e("s_cbranch_scc0 done_%d" % k)
        else:
            e("v_cmp_ne_u64_e64 vcc, s[34:35], 0")
            e("s_cbranch_vccz done_%d" % k)
        e("s_ff1_i32_b64 s36, s[34:35]")
        e("s_add_u32 s28, s28, 1")
        e("s_add_u32 s29, s29, s36")
        e("s_bitset0_b64 s[34:35], s36")
        e("s_branch loop_%d" % k)
        e("done_%d:" % k)
        e(CAP)
    else:
        for ins in body:
            e(ins.replace("{K}", str(k)))
        if CAP not in body:
            e(CAP)
    e("v_cmp_ne_u32_e32 vcc, %d, v17" % k if k <= 64 else "v_cmp_ne_u32_e32 vcc, 0x%x, v17" % k)
    e("v_mov_b32 v19, s28")
    e("v_cndmask_b32 v20, v19, v20, vcc")
    e("v_mov_b32 v19, s29")
    e("v_cndmask_b32 v21, v19, v21, vcc")
    e("v_mov_b32 v19, s30")
    e("v_cndmask_b32 v22, v19, v22, vcc")
    e("s_add_u32 s31, s16, 0x%x" % k if k > 64 else "s_add_u32 s31, s16, %d" % k)
    e("v_mov_b32 v24, s28")
    e("v_mov_b32 v25, s29")
    e("v_mov_b32 v26, s30")
    e("v_mov_b32 v27, s31")
    e("v_mov_b32 v28, 0x%x" % (16 * k))
    e("s_mov_b64 exec, s[22:23]")
    e("buffer_store_dwordx4 v[24:27], v28, s[12:15], 0 offen")
    e("s_mov_b64 exec, s[32:33]")

# the grid cell: bit (63 - col) of {hi, lo}, col 64 a gap, col 65 SCC
e("s_mov_b64 exec, s[32:33]")
e("v_sub_i32 v23, vcc, 63, v18")
e("v_and_b32 v23, 31, v23")
e("v_cmp_gt_u32_e32 vcc, 32, v18")
e("v_cndmask_b32 v24, v20, v21, vcc")
e("v_lshrrev_b32 v24, v23, v24")
e("v_and_b32 v24, 1, v24")
e("v_cmp_eq_u32_e32 vcc, 0x41, v18")
e("v_cndmask_b32 v24, v24, v22, vcc")
e("v_cmp_ne_u32_e32 vcc, 64, v18")
e("v_cndmask_b32 v24, 2, v24, vcc")
# zero bits: 0.10 / 0.20 by alternating byte; one bits: 1.0; gaps, gutters, outside: 0.02
e("v_lshrrev_b32 v25, 3, v18")
e("v_and_b32 v25, 1, v25")
e("v_cvt_f32_u32 v25, v25")
e("v_mul_f32 v25, 0x3dcccccd, v25")
e("v_add_f32 v25, 0x3dcccccd, v25")
e("v_mov_b32 v26, 1.0")
e("v_cmp_eq_u32_e32 vcc, 1, v24")
e("v_cndmask_b32 v25, v25, v26, vcc")
e("v_mov_b32 v26, 0x3ca3d70a")
e("v_cmp_eq_u32_e32 vcc, 2, v24")
e("v_cndmask_b32 v25, v25, v26, vcc")
e("v_cmp_lt_f32_e32 vcc, 0x3f59999a, v10")
e("v_cndmask_b32 v25, v25, v26, vcc")
e("v_cmp_lt_f32_e32 vcc, 0x3f400000, v11")
e("v_cndmask_b32 v25, v25, v26, vcc")
e("v_cndmask_b32_e64 v25, v26, v25, s[20:21]")
# the labels: the UI buffer over the grid, premultiplied (as ps_ui)
e("v_mul_f32 v29, 0x3a088889, v2")  # x / 1920
e("v_mul_f32 v30, 0x3a72b9d6, v3")  # y / 1080
e("image_sample_lz v[29:32], v[29:30], s[40:47], s[48:51] dmask:0xf")
e("s_waitcnt vmcnt(0)")
e("v_sub_f32 v32, 1.0, v32")
e("v_mul_f32 v25, v25, v32")
e("v_add_f32 v25, v25, v29")
e("v_mov_b32 v27, 1.0")
e("exp mrt0, v25, v25, v25, v27 done vm")
e("s_endpgm")

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sh = os.path.join(root, "shaders")
src_path = os.path.join(sh, "ps_bfm64.s")


def assemble(text):
    p = subprocess.run(["llvm-mc-18", "-triple=amdgcn", "-mcpu=bonaire", "-filetype=obj", "-o",
                        "/tmp/ps_bfm64.o", "-"], input=text, capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stderr)
    subprocess.run(["llvm-objcopy-18", "-O", "binary", "--only-section=.text", "/tmp/ps_bfm64.o",
                    "/tmp/ps_bfm64.bin"], check=True)
    d = open("/tmp/ps_bfm64.bin", "rb").read()
    return list(struct.unpack("<%dI" % (len(d) // 4), d))


hdr = "; S_BFM_B64 hardware test (OPCODE_TEST 6), generated by tools/gen_ps_bfm64.py\n"
words = assemble("\n".join(asm).replace("HDRLEN", "0x7fff") + "\n")
if len(words) % 2:
    asm.append("s_nop 0")
    words = assemble("\n".join(asm).replace("HDRLEN", "0x7fff") + "\n")
hdrlen = len(words) // 2 - 1
text = hdr + "\n".join(asm).replace("HDRLEN", "0x%x" % hdrlen) + "\n"
words = assemble(text)
open(src_path, "w").write(text)

HASH = 0xCAFE0601
trailer = [0x5362724F, 0x00726468, (len(words) * 4) << 8, 0, 0xDEADBEEF, HASH, 0]
allw = words + trailer
vg = 35 + 1  # v0..v35
sg = 51 + 1 + 2  # s0..s51 + VCC


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


out = []
o = out.append
o("#pragma once")
o("#include <stdint.h>")
o("")
o("// S_BFM_B64 hardware test (OPCODE_TEST 6): shaders/ps_bfm64.s, generated by")
o("// tools/gen_ps_bfm64.py. Table: dwords 0..5 grid (x0, y0, 1 / cell w, 1 / row h, first row,")
o("// rows - 1), 6 / 7 logging pixel, 8..11 result V#, 12 marker base, 16..23 labels T#, 24..27")
o("// its S#, 32 + 2 k row k's s2, s3.")
o("// Result row k at 16 k: lo, hi, SCC, marker base + k.")
o("#define PS_BFM64_ROWS %d" % N)
o("#define PS_BFM64_MARK 0x%08Xu" % MARK)
o("#define PS_BFM64_RSRC1 ((%du << 6) | %du) /* v0-v%d, s0-s51 + VCC */" %
  ((sg + 7) // 8 - 1, (vg + 3) // 4 - 1, vg - 1))
o("static const uint32_t ps_bfm64_binary[] __attribute__((aligned(256))) = {")
for i in range(0, len(allw), 8):
    o("    " + " ".join("0x%08X," % x for x in allw[i:i + 8]))
o("};")
o("")
o("typedef struct {")
o("    char group;")
o("    uint8_t scc_in;")
o("    uint32_t s2, s3;")
o("    const char* label;")
o("    const char* ins;")
o("} Bfm64Row;")
o("static const Bfm64Row ps_bfm64_rows[PS_BFM64_ROWS] = {")
for g, lab, a, b, scc_in, body, t in ROWS:
    ln = "    {'%s', %d, 0x%08Xu, 0x%08Xu, %s, %s}," % (g, scc_in, a, b, cstr(lab), cstr(t))
    if len(ln) > 100:
        o("    {'%s', %d, 0x%08Xu, 0x%08Xu, %s," % (g, scc_in, a, b, cstr(lab)))
        parts, cur = [], ""
        for word in t.split(" "):
            nxt = word if not cur else cur + " " + word
            if len(cstr(nxt)) + 8 > 100:
                parts.append(cur + " ")
                cur = word
            else:
                cur = nxt
        parts.append(cur)
        for i, part in enumerate(parts):
            o("     %s%s" % (cstr(part), "}," if i == len(parts) - 1 else ""))
        continue
    o(ln)
o("};")
open(os.path.join(sh, "ps_bfm64.h"), "w").write("\n".join(out) + "\n")
print("rows %d, code %d dwords" % (N, len(words)))
