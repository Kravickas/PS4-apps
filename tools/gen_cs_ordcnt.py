#!/usr/bin/env python3
"""DS_ORDERED_COUNT hardware test shaders (src/ordcnt_test.h): shaders/cs_ordcnt.s and .h.

One compute shader template, one binary per variant; the variants differ only in the encoded DS
instructions (offset0 / offset1 fields, ADDR / DATA0 registers). Everything else comes from the
parameter block at the V# base (src/ordcnt_test.h, OC_P_*).

Inputs: s[0:3] V# (USER_SGPR 4), s4 TGID.x, s5 TGID.y, s6 TG_SIZE (TGID_X/Y_EN, TG_SIZE_EN),
v0 = thread id x.
DS encoding (CI): dw0 = offset0 | offset1 << 8 | gds << 17 | op << 18 | 0x36 << 26,
dw1 = addr | data0 << 8 | data1 << 16 | vdst << 24. DS_ORDERED_COUNT = op 63.
Ordered offset fields (LLVM selectDSOrderedIntrinsic): offset0 = index << 2,
offset1 = release | done << 1 | shader_type << 2 | swap << 4.

Needs llvm-mc-18 and llvm-objcopy-18. Run from the project root.
"""
import os
import struct
import subprocess
import tempfile
import textwrap

OP_ORDERED = 63
OP_ADD_RTN_U32 = 32

V_TID, V_VAL1, V_LANE, V_VAL2, V_RET1, V_RET2, V_ALT, V_ADDR1 = 0, 1, 2, 4, 5, 6, 7, 11
V_RET3, V_VAL3 = 12, 13

# name, hash, op1, op2 (or None). op = (kind, index, release, done, swap, addr_reg, data0_reg,
# stype); kind 'ord' = DS_ORDERED_COUNT, 'plain' = DS_ADD_RTN_U32 gds (no ordering).
VARIANTS = [
    ("plain", 0xC0DE0C00, ("plain", 0, 0, 0, 0, V_ADDR1, V_VAL1, 0), None),
    ("a", 0xC0DE0C01, ("ord", 0, 1, 1, 0, V_VAL1, 0, 0), None),
    ("b", 0xC0DE0C02, ("ord", 0, 1, 0, 0, V_VAL1, 0, 0), ("ord", 1, 1, 1, 0, V_VAL2, 0, 0)),
    ("c", 0xC0DE0C03, ("ord", 0, 0, 0, 0, V_VAL1, 0, 0), ("ord", 0, 1, 1, 0, V_VAL2, 0, 0)),
    ("d", 0xC0DE0C04, ("ord", 0, 1, 1, 1, V_VAL1, 0, 0), None),
    ("e1", 0xC0DE0C05, ("ord", 0, 1, 1, 0, V_ALT, V_VAL1, 0), None),
    ("e2", 0xC0DE0C06, ("ord", 0, 1, 1, 0, V_VAL1, V_ALT, 0), None),
    ("f1", 0xC0DE0C07, ("ord", 1, 1, 1, 0, V_VAL1, 0, 0), None),
    ("f2", 0xC0DE0C08, ("ord", 2, 1, 1, 0, V_VAL1, 0, 0), None),
    ("f3", 0xC0DE0C09, ("ord", 3, 1, 1, 0, V_VAL1, 0, 0), None),
    ("g", 0xC0DE0C0A, ("ord", 1, 1, 0, 0, V_VAL1, 0, 0), ("ord", 0, 1, 1, 0, V_VAL2, 0, 0)),
    ("r2", 0xC0DE0C0B, ("ord", 0, 1, 0, 0, V_VAL1, 0, 0), ("ord", 0, 1, 1, 0, V_VAL2, 0, 0)),
    ("r3", 0xC0DE0C0C, ("ord", 0, 1, 0, 0, V_VAL1, 0, 0), None),
    ("r4", 0xC0DE0C0D, ("ord", 4, 1, 1, 0, V_VAL1, 0, 0), None),
    ("r5", 0xC0DE0C0E, ("ord", 15, 1, 1, 0, V_VAL1, 0, 0), None),
    ("r6", 0xC0DE0C0F, ("ord", 0, 1, 1, 0, V_VAL1, 0, 1), None),
    ("r7", 0xC0DE0C10, ("ord", 0, 0, 0, 0, V_VAL1, 0, 0), None),
    ("h4", 0xC0DE0C11, ("ord", 0, 1, 0, 0, V_VAL1, 0, 0), ("ord", 4, 1, 1, 0, V_VAL2, 0, 0)),
    ("c1", 0xC0DE0C12, ("ord", 0, 0, 0, 0, V_VAL1, 0, 0), ("ord", 1, 1, 1, 0, V_VAL2, 0, 0)),
    ("c4", 0xC0DE0C13, ("ord", 0, 0, 0, 0, V_VAL1, 0, 0), ("ord", 4, 1, 1, 0, V_VAL2, 0, 0)),
]


def o(index, release, done, val, swap=0, stype=0, x1=0, gds=1):
    return ("ord", index, release, done, swap, val, 0, stype, x1, gds)


# Three-op variants: name, hash, [(op for even slots, op for odd slots or None = skip), ...].
# k<A>_<B>: op1 idx A no release, op2 idx B release, op3 idx A release done.
VARIANTS3 = []
for i, (a, b) in enumerate([(0, 0), (0, 1), (0, 2), (0, 3), (0, 4), (0, 8), (0, 16), (0, 32),
                            (0, 63), (1, 5)]):
    ops = [o(a, 0, 0, V_VAL1), o(b, 1, 0, V_VAL2), o(a, 1, 1, V_VAL3)]
    VARIANTS3.append(("k%d_%d" % (a, b), 0xC0DE0C20 + i, [(x, x) for x in ops]))
VARIANTS3 += [
    # three ops on idx0, each with release
    ("n3", 0xC0DE0C30, [(o(0, 1, 0, V_VAL1),) * 2, (o(0, 1, 0, V_VAL2),) * 2,
                        (o(0, 1, 1, V_VAL3),) * 2]),
    # even slots idx0, odd slots idx1
    ("sp", 0xC0DE0C31, [(o(0, 1, 1, V_VAL1), o(1, 1, 1, V_VAL1))]),
    # even slots two ops on idx0, odd slots one
    ("un", 0xC0DE0C32, [(o(0, 1, 0, V_VAL1), o(0, 1, 1, V_VAL1)), (o(0, 1, 1, V_VAL2), None)]),
    # done without release
    ("dn", 0xC0DE0C33, [(o(0, 0, 1, V_VAL1),) * 2]),
    # done on both ops, different indices
    ("dd", 0xC0DE0C34, [(o(0, 1, 1, V_VAL1),) * 2, (o(1, 1, 1, V_VAL2),) * 2]),
    # an op on idx0 after a done on idx0
    ("ad", 0xC0DE0C35, [(o(0, 1, 1, V_VAL1),) * 2, (o(0, 1, 1, V_VAL2),) * 2]),
    # release, no release, release done
    ("rnr", 0xC0DE0C36, [(o(0, 1, 0, V_VAL1),) * 2, (o(0, 0, 0, V_VAL2),) * 2,
                         (o(0, 1, 1, V_VAL3),) * 2]),
    # even slots three steps, odd slots one
    ("u31", 0xC0DE0C37, [(o(0, 1, 0, V_VAL1), o(0, 1, 1, V_VAL1)), (o(0, 1, 0, V_VAL2), None),
                         (o(0, 1, 1, V_VAL3), None)]),
    # even slots one step, odd slots three
    ("u13", 0xC0DE0C38, [(o(0, 1, 1, V_VAL1), o(0, 1, 0, V_VAL1)), (None, o(0, 1, 0, V_VAL2)),
                         (None, o(0, 1, 1, V_VAL3))]),
    # even slots two steps, odd slots three
    ("u23", 0xC0DE0C39, [(o(0, 1, 0, V_VAL1),) * 2, (o(0, 1, 1, V_VAL2), o(0, 1, 0, V_VAL2)),
                         (None, o(0, 1, 1, V_VAL3))]),
    # field probes: shader type 2 / 3, offset1 bits 5 / 6 / 7, GDS bit 0
    ("t2", 0xC0DE0C3A, [(o(0, 1, 1, V_VAL1, stype=2),) * 2]),
    ("t3", 0xC0DE0C3B, [(o(0, 1, 1, V_VAL1, stype=3),) * 2]),
    ("x5", 0xC0DE0C3C, [(o(0, 1, 1, V_VAL1, x1=0x20),) * 2]),
    ("x6", 0xC0DE0C3D, [(o(0, 1, 1, V_VAL1, x1=0x40),) * 2]),
    ("x7", 0xC0DE0C3E, [(o(0, 1, 1, V_VAL1, x1=0x80),) * 2]),
    ("lds", 0xC0DE0C3F, [(o(0, 1, 1, V_VAL1, gds=0),) * 2]),
]

# GDS window shaders (no ordered ops): wave TGID.x covers GDS bytes [TGID.x * 0x400, +0x400) through
# M0 = {base TGID.x * 0x400, size 0x400}; lane l handles bytes l*4 + {0, 0x100, 0x200, 0x300}.
# "gdump" reads the window; "gfill" writes VAL1 there, then reads it back. Either way the record
# of wave TGID.x (REC_BASE + TGID.x * 0x400) receives the window as read.
WINDOW_SHADERS = [("gdump", 0xC0DE0C40, False), ("gfill", 0xC0DE0C41, True)]


def window_shader(name, fill):
    a = [
        "; ---- variant %s ----" % name,
        "s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0",
        "s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10",
        "s_waitcnt lgkmcnt(0)",
        "v_and_b32 v2, 63, v0",
        "v_lshlrev_b32 v1, 2, v2",
        "v_mov_b32 v7, s23",
        "s_lshl_b32 s46, s4, 26",
        "s_or_b32 m0, s46, 0x400",
        "s_nop 1",
    ]
    if fill:
        a += ["ds_write_b32 v1, v7 offset:%d gds" % off if off else "ds_write_b32 v1, v7 gds"
              for off in (0, 256, 512, 768)]
        a += ["s_waitcnt lgkmcnt(0)"]
    a += ["ds_read_b32 v%d, v1 offset:%d gds" % (3 + i, off) if off else
          "ds_read_b32 v%d, v1 gds" % (3 + i) for i, off in enumerate((0, 256, 512, 768))]
    a += [
        "s_waitcnt lgkmcnt(0)",
        "s_lshl_b32 s42, s4, 10",
        "s_add_u32 s42, s42, s32",
        "v_add_i32 v21, vcc, s42, v1",
        "buffer_store_dword v3, v21, s[0:3], 0 offen",
        "buffer_store_dword v4, v21, s[0:3], 0 offen offset:256",
        "buffer_store_dword v5, v21, s[0:3], 0 offen offset:512",
        "buffer_store_dword v6, v21, s[0:3], 0 offen offset:768",
        "s_waitcnt vmcnt(0)",
        "s_endpgm",
    ]
    return a


def op_x(op):
    """Optional op fields: extra offset1 bits (x1, bits 7:5) and the GDS bit (default 1)."""
    return (op[8] if len(op) > 8 else 0), (op[9] if len(op) > 9 else 1)


def ds_words(op, vdst):
    kind, index, release, done, swap, addr, data0, stype = op[:8]
    x1, gds = op_x(op)
    if kind == "plain":
        dw0 = (1 << 17) | (OP_ADD_RTN_U32 << 18) | (0x36 << 26)
    else:
        off0 = (index << 2) & 0xFF
        off1 = release | (done << 1) | (stype << 2) | (swap << 4) | x1
        dw0 = off0 | (off1 << 8) | (gds << 17) | (OP_ORDERED << 18) | (0x36 << 26)
    dw1 = addr | (data0 << 8) | (vdst << 24)
    return dw0, dw1


def ds_text(op, vdst):
    kind, index, release, done, swap, addr, data0, stype = op[:8]
    if kind == "plain":
        return "ds_add_rtn_u32 v%d, v%d, v%d gds" % (vdst, addr, data0)
    return "ds_ordered_count v%d, addr v%d, data0 v%d, idx %d rel %d done %d type %d %s gds" % (
        vdst, addr, data0, index, release, done, stype, "swap" if swap else "add")


def ds_short(op):
    kind, index, release, done, swap, addr, data0, stype = op[:8]
    x1, gds = op_x(op)
    if kind == "plain":
        return "ds_add_rtn_u32 gds"
    f = ["idx%d" % index] + (["rel"] if release else []) + (["done"] if done else [])
    f += ["swap" if swap else "add", "a=v%d d0=v%d" % (addr, data0)]
    if stype:
        f.append("type%d" % stype)
    if x1:
        f.append("offset1|0x%02x" % x1)
    if not gds:
        f.append("gds=0")
    return " ".join(f)


def op_block(op, m0_sgpr, vdst, ret_sgpr, tb, ta):
    w0, w1 = ds_words(op, vdst)
    m0 = "s_mov_b32 m0, s37" if op[0] == "plain" else "s_mov_b32 m0, %s" % m0_sgpr
    return [
        "s_memtime %s" % tb,
        m0,
        "s_mov_b32 exec_lo, s26",
        "s_mov_b32 exec_hi, s27",
        "s_nop 1",
        "; " + ds_text(op, vdst),
        ".long 0x%08x, 0x%08x" % (w0, w1),
        "s_waitcnt lgkmcnt(0)",
        "s_mov_b64 exec, -1",
        "s_memtime %s" % ta,
        "s_mov_b32 m0, s37",
        "s_mov_b64 exec, 1",
        "s_nop 1",
        "ds_add_rtn_u32 v8, v9, v10 gds",
        "s_waitcnt lgkmcnt(0)",
        "s_mov_b64 exec, -1",
        "v_readfirstlane_b32 %s, v8" % ret_sgpr,
    ]


def delay(tag, count_sgpr):
    return [
        "%s_loop:" % tag,
        "s_cmp_eq_u32 %s, 0" % count_sgpr,
        "s_cbranch_scc1 %s_done" % tag,
        "s_sleep 2",
        "s_sub_u32 %s, %s, 1" % (count_sgpr, count_sgpr),
        "s_branch %s_loop" % tag,
        "%s_done:" % tag,
    ]


HEADER = [
    "s6",  # 0 TG_SIZE raw
    "s4",  # 1 TGID.x
    "s5",  # 2 TGID.y
    "s40",  # 3 wave in group
    "s41",  # 4 slot
    "s43",  # 5 M0[15:0] used
    "s44",  # 6 M0 op1
    "s45",  # 7 M0 op2
    "s48", "s49",  # 8 memtime start
    "s50", "s51",  # 10 before op1
    "s52", "s53",  # 12 after op1
    "s54", "s55",  # 14 before op2
    "s56", "s57",  # 16 after op2
    "s58",  # 18 ticket after op1
    "s59",  # 19 ticket after op2
    "s64",  # 20 marker
    "s61",  # 21 HW_ID
    "s26",  # 22 exec lo for the ops
    "s27",  # 23 exec hi
    "s63",  # 24 skipped
    "s66", "s67",  # 25 before op3
    "s68", "s69",  # 27 after op3
    "s70",  # 29 ticket after op3
    "s71",  # 30 M0 op3
]

# Per op slot: M0 SGPR, return VGPR, ticket SGPR, memtime before / after.
SLOTS = [("s44", V_RET1, "s58", "s[50:51]", "s[52:53]"),
         ("s45", V_RET2, "s59", "s[54:55]", "s[56:57]"),
         ("s71", V_RET3, "s70", "s[66:67]", "s[68:69]")]


def slot_block(name, k, even, odd):
    m0, ret, tk, tb, ta = SLOTS[k]
    if even == odd:
        return op_block(even, m0, ret, tk, tb, ta)
    a = ["s_and_b32 s46, s41, 1",
         "s_cmp_eq_u32 s46, 0",
         "s_cbranch_scc0 %s_odd%d" % (name, k)]
    if even:
        a += op_block(even, m0, ret, tk, tb, ta)
    a += ["s_branch %s_join%d" % (name, k), "%s_odd%d:" % (name, k)]
    if odd:
        a += op_block(odd, m0, ret, tk, tb, ta)
    a += ["%s_join%d:" % (name, k)]
    return a


def shader(name, slots):
    a = [
        "; ---- variant %s ----" % name,
        "s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0",
        "s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10",
        "s_buffer_load_dwordx8 s[80:87], s[0:3], 0x18",
        "s_memtime s[48:49]",
        "s_getreg_b32 s61, hwreg(HW_REG_HW_ID)",
        "s_waitcnt lgkmcnt(0)",
        "v_readfirstlane_b32 s40, v0",
        "s_lshr_b32 s40, s40, 6",
        "s_mul_i32 s41, s5, s16",
        "s_add_u32 s41, s41, s4",
        "s_mul_i32 s41, s41, s17",
        "s_add_u32 s41, s41, s40",
        "s_mul_i32 s42, s41, 0x400",
        "s_add_u32 s42, s42, s32",
        "s_bfe_u32 s43, s6, 0xb0006",
        "s_cmp_eq_u32 s30, 3",
        "s_cbranch_scc0 %s_m3" % name,
        "s_bfe_u32 s43, s6, 0xc0006",
        "%s_m3:" % name,
        "s_cmp_eq_u32 s30, 1",
        "s_cselect_b32 s43, 0, s43",
        "s_cmp_eq_u32 s30, 2",
        "s_cbranch_scc0 %s_m2" % name,
        "s_add_u32 s43, s43, 1",
        "%s_m2:" % name,
        "s_mul_i32 s46, s5, s20",
        "s_add_u32 s46, s46, s18",
        "s_lshl_b32 s46, s46, 16",
        "s_or_b32 s44, s46, s43",
        "s_mul_i32 s46, s5, s34",
        "s_add_u32 s46, s46, s19",
        "s_lshl_b32 s46, s46, 16",
        "s_or_b32 s45, s46, s43",
        "s_lshl_b32 s46, s80, 16",
        "s_or_b32 s71, s46, s43",
        "v_and_b32 v2, 63, v0",
        "v_mul_lo_u32 v3, v2, s24",
        "s_mul_i32 s46, s41, s25",
        "s_add_u32 s47, s46, s23",
        "v_add_i32 v1, vcc, s47, v3",
        "s_add_u32 s47, s46, s33",
        "v_add_i32 v4, vcc, s47, v3",
        "s_add_u32 s47, s46, s81",
        "v_add_i32 v13, vcc, s47, v3",
        "v_or_b32 v12, 0xcafe0000, v2",
        "v_or_b32 v5, 0xdead0000, v2",
        "v_or_b32 v6, 0xbeef0000, v2",
        "v_mov_b32 v7, s35",
        "v_mov_b32 v9, s28",
        "v_mov_b32 v10, 1",
        "v_mov_b32 v8, 0",
        "v_mov_b32 v11, s18",
        "s_mov_b32 s58, -1",
        "s_mov_b32 s59, -1",
        "s_mov_b64 s[50:51], 0",
        "s_mov_b64 s[52:53], 0",
        "s_mov_b64 s[54:55], 0",
        "s_mov_b64 s[56:57], 0",
        "s_mov_b64 s[66:67], 0",
        "s_mov_b64 s[68:69], 0",
        "s_mov_b32 s70, -1",
        "s_mov_b32 s64, 0x0dc0ffee",
        # skip when SKIP[15:0] != 0 and (slot & SKIP[15:0]) != SKIP[31:16]
        "s_and_b32 s46, s31, 0xffff",
        "s_lshr_b32 s47, s31, 16",
        "s_and_b32 s63, s41, s46",
        "s_cmp_lg_u32 s63, s47",
        "s_cselect_b32 s63, 1, 0",
        "s_cmp_eq_u32 s46, 0",
        "s_cselect_b32 s63, 0, s63",
        "s_cmp_lg_u32 s63, 0",
        "s_cbranch_scc1 %s_skip" % name,
        "s_sub_u32 s62, s29, 1",
        "s_sub_u32 s62, s62, s41",
        "s_cmp_lg_u32 s36, 0",
        "s_cselect_b32 s62, s41, s62",
        "s_mul_i32 s62, s62, s21",
    ]
    a += delay(name + "_pre", "s62")
    a += slot_block(name, 0, *slots[0])
    if len(slots) > 1:
        # waves with (slot & MID_SEL[15:0]) == MID_SEL[31:16] sleep MID iterations; 0 = odd slots
        a += ["s_cmp_eq_u32 s82, 0",
              "s_cselect_b32 s46, 0x10001, s82",
              "s_and_b32 s47, s46, 0xffff",
              "s_lshr_b32 s46, s46, 16",
              "s_and_b32 s62, s41, s47",
              "s_cmp_eq_u32 s62, s46",
              "s_cselect_b32 s62, s22, 0"]
        a += delay(name + "_mid", "s62")
        a += slot_block(name, 1, *slots[1])
    if len(slots) > 2:
        # waves with (slot & MID2_SEL[15:0]) == MID2_SEL[31:16] sleep MID2 iterations
        a += ["s_and_b32 s46, s39, 0xffff",
              "s_lshr_b32 s47, s39, 16",
              "s_and_b32 s62, s41, s46",
              "s_cmp_eq_u32 s62, s47",
              "s_cselect_b32 s62, s38, 0"]
        a += delay(name + "_mid2", "s62")
        a += slot_block(name, 2, *slots[2])
    a += ["%s_skip:" % name, "s_waitcnt lgkmcnt(0)"]
    for i, s in enumerate(HEADER):
        a.append("v_writelane_b32 v20, %s, %d" % (s, i))
    a += [
        "v_lshlrev_b32 v21, 2, v2",
        "v_add_i32 v21, vcc, s42, v21",
        "buffer_store_dword v20, v21, s[0:3], 0 offen",
        "buffer_store_dword v5, v21, s[0:3], 0 offen offset:256",
        "buffer_store_dword v6, v21, s[0:3], 0 offen offset:512",
        "buffer_store_dword v12, v21, s[0:3], 0 offen offset:768",
        "s_waitcnt vmcnt(0)",
        "s_endpgm",
    ]
    return a


def assemble(lines):
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, "a.s")
        obj = os.path.join(d, "a.o")
        raw = os.path.join(d, "a.bin")
        open(src, "w").write("\n".join(lines) + "\n")
        subprocess.check_call(["llvm-mc-18", "-triple=amdgcn", "-mcpu=bonaire", "-filetype=obj",
                               src, "-o", obj])
        subprocess.check_call(["llvm-objcopy-18", "-O", "binary", "--only-section=.text", obj,
                               raw])
        b = open(raw, "rb").read()
    assert len(b) % 4 == 0
    return list(struct.unpack("<%dI" % (len(b) // 4), b))


def comment(text):
    """A C block comment wrapped at 100 columns the way clang-format reflows one."""
    lines = textwrap.wrap(text, width=97, break_long_words=False, break_on_hyphens=False)
    out = ["/* " + lines[0]] + [" * " + x for x in lines[1:]]
    if len(out[-1]) + 3 <= 100:
        out[-1] += " */"
    else:
        out.append(" */")
    return out


def main():
    out_s = ["; DS_ORDERED_COUNT test shaders (tools/gen_cs_ordcnt.py). GCN2 / Sea Islands."]
    out_h = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        "/* DS_ORDERED_COUNT test compute shaders (src/ordcnt_test.h), generated by",
        "   tools/gen_cs_ordcnt.py from one template (cs_ordcnt.s): s[0:3] V# of the parameter "
        "block,",
        "   s4 / s5 TGID.x / y, s6 TG_SIZE, v0 thread id. COMPUTE_PGM_RSRC1 CS_OC_RSRC1 (28 VGPRs,",
        "   96 SGPRs), RSRC2 CS_OC_RSRC2 (USER_SGPR 4, TGID_X_EN, TGID_Y_EN, TG_SIZE_EN). */",
        "#define CS_OC_RSRC1 0x000C02C6u",
        "#define CS_OC_RSRC2 0x00000588u",
        "",
    ]
    allv = [(n, h, [(x, x) for x in (op1, op2) if x]) for n, h, op1, op2 in VARIANTS]
    allv += VARIANTS3
    allv += [(n, h, fill) for n, h, fill in WINDOW_SHADERS]
    for name, h, slots in allv:
        lines = window_shader(name, slots) if isinstance(slots, bool) else shader(name, slots)
        code = assemble(lines)
        if (len(code) + 2) % 2:
            code.append(0xBF800000)  # s_nop 0: header + code to an even dword count
        if (len(code) + 2 + 7) % 8 == 1:
            code += [0xBF800000] * 2  # no 1-element last row: clang-format would repack the array
        total = len(code) + 2
        words = [0xBEEB03FF, total // 2 - 1] + code
        words += [0x5362724F, 0x00726468, (total * 4) << 8, 0, 0xDEADBEEF, h, 0]
        out_s += lines + [""]
        desc = []
        if isinstance(slots, bool):
            desc.append("GDS window %s" % ("fill + read" if slots else "read"))
            slots = []
        for k, (even, odd) in enumerate(slots):
            if even == odd:
                desc.append("op%d %s" % (k + 1, ds_short(even)))
            else:
                desc.append("op%d even %s, odd %s" % (k + 1, ds_short(even) if even else "none",
                                                        ds_short(odd) if odd else "none"))
        out_h += comment("%s: %s. Hash %08X." % (name, "; ".join(desc), h))
        out_h.append("static const uint32_t cs_oc_%s[] __attribute__((aligned(256))) = {" % name)
        for i in range(0, len(words), 8):
            out_h.append("    " + " ".join("0x%08X," % x for x in words[i:i + 8]))
        out_h.append("};")
    out_h.append("")
    out_h.append("enum {")
    for name, h, slots in allv:
        out_h.append("    CS_OC_%s," % name.upper())
    out_h.append("    CS_OC_COUNT")
    out_h.append("};")
    out_h.append("struct CsOcBin {")
    out_h.append("    const uint32_t* bin;")
    out_h.append("    uint32_t size;")
    out_h.append("    uint32_t nops; /* op slots */")
    out_h.append("    const char* name;")
    out_h.append("};")
    out_h.append("static const struct CsOcBin k_cs_oc[CS_OC_COUNT] = {")
    for name, h, slots in allv:
        n = 0 if isinstance(slots, bool) else len(slots)
        out_h.append("    {cs_oc_%s, sizeof(cs_oc_%s), %d, \"%s\"}," % (name, name, n, name))
    out_h.append("};")
    open("shaders/cs_ordcnt.s", "w").write("\n".join(out_s) + "\n")
    open("shaders/cs_ordcnt.h", "w").write("\n".join(out_h) + "\n")


if __name__ == "__main__":
    main()
