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

OP_ORDERED = 63
OP_ADD_RTN_U32 = 32

V_TID, V_VAL1, V_LANE, V_VAL2, V_RET1, V_RET2, V_ALT, V_ADDR1 = 0, 1, 2, 4, 5, 6, 7, 11

# name: (hash, op1, op2); op = (kind, index, release, done, swap, addr_reg, data0_reg, stype)
#   kind 'ord' = DS_ORDERED_COUNT, 'plain' = DS_ADD_RTN_U32 gds (no ordering)
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


def ds_words(op, vdst):
    kind, index, release, done, swap, addr, data0, stype = op
    if kind == "plain":
        dw0 = (1 << 17) | (OP_ADD_RTN_U32 << 18) | (0x36 << 26)
    else:
        off0 = (index << 2) & 0xFF
        off1 = release | (done << 1) | (stype << 2) | (swap << 4)
        dw0 = off0 | (off1 << 8) | (1 << 17) | (OP_ORDERED << 18) | (0x36 << 26)
    dw1 = addr | (data0 << 8) | (vdst << 24)
    return dw0, dw1


def ds_text(op, vdst):
    kind, index, release, done, swap, addr, data0, stype = op
    if kind == "plain":
        return "ds_add_rtn_u32 v%d, v%d, v%d gds" % (vdst, addr, data0)
    return "ds_ordered_count v%d, addr v%d, data0 v%d, idx %d rel %d done %d type %d %s gds" % (
        vdst, addr, data0, index, release, done, stype, "swap" if swap else "add")


def ds_short(op):
    kind, index, release, done, swap, addr, data0, stype = op
    if kind == "plain":
        return "ds_add_rtn_u32 gds"
    f = ["idx%d" % index] + (["rel"] if release else []) + (["done"] if done else [])
    f += ["swap" if swap else "add", "a=v%d d0=v%d" % (addr, data0)]
    if stype:
        f.append("type%d" % stype)
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
]


def shader(name, op1, op2):
    a = [
        "; ---- variant %s ----" % name,
        "s_buffer_load_dwordx16 s[16:31], s[0:3], 0x0",
        "s_buffer_load_dwordx8 s[32:39], s[0:3], 0x10",
        "s_memtime s[48:49]",
        "s_getreg_b32 s61, hwreg(HW_REG_HW_ID)",
        "s_waitcnt lgkmcnt(0)",
        "v_readfirstlane_b32 s40, v0",
        "s_lshr_b32 s40, s40, 6",
        "s_mul_i32 s41, s5, s16",
        "s_add_u32 s41, s41, s4",
        "s_mul_i32 s41, s41, s17",
        "s_add_u32 s41, s41, s40",
        "s_mul_i32 s42, s41, 0x300",
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
        "v_and_b32 v2, 63, v0",
        "v_mul_lo_u32 v3, v2, s24",
        "s_mul_i32 s46, s41, s25",
        "s_add_u32 s47, s46, s23",
        "v_add_i32 v1, vcc, s47, v3",
        "s_add_u32 s47, s46, s33",
        "v_add_i32 v4, vcc, s47, v3",
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
        "s_mov_b32 s64, 0x0dc0ffee",
        "s_and_b32 s63, s41, 1",
        "s_and_b32 s63, s63, s31",
        "s_cmp_lg_u32 s63, 0",
        "s_cbranch_scc1 %s_skip" % name,
        "s_sub_u32 s62, s29, 1",
        "s_sub_u32 s62, s62, s41",
        "s_cmp_lg_u32 s36, 0",
        "s_cselect_b32 s62, s41, s62",
        "s_mul_i32 s62, s62, s21",
    ]
    a += delay(name + "_pre", "s62")
    a += op_block(op1, "s44", V_RET1, "s58", "s[50:51]", "s[52:53]")
    if op2:
        a += ["s_and_b32 s62, s41, 1", "s_mul_i32 s62, s62, s22"]
        a += delay(name + "_mid", "s62")
        a += op_block(op2, "s45", V_RET2, "s59", "s[54:55]", "s[56:57]")
    a += ["%s_skip:" % name, "s_waitcnt lgkmcnt(0)"]
    for i, s in enumerate(HEADER):
        a.append("v_writelane_b32 v20, %s, %d" % (s, i))
    a += [
        "v_lshlrev_b32 v21, 2, v2",
        "v_add_i32 v21, vcc, s42, v21",
        "buffer_store_dword v20, v21, s[0:3], 0 offen",
        "buffer_store_dword v5, v21, s[0:3], 0 offen offset:256",
        "buffer_store_dword v6, v21, s[0:3], 0 offen offset:512",
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
        "   80 SGPRs), RSRC2 CS_OC_RSRC2 (USER_SGPR 4, TGID_X_EN, TGID_Y_EN, TG_SIZE_EN). */",
        "#define CS_OC_RSRC1 0x000C0246u",
        "#define CS_OC_RSRC2 0x00000588u",
        "",
    ]
    names = []
    for name, h, op1, op2 in VARIANTS:
        lines = shader(name, op1, op2)
        code = assemble(lines)
        if (len(code) + 2) % 2:
            code.append(0xBF800000)  # s_nop 0: header + code to an even dword count
        total = len(code) + 2
        words = [0xBEEB03FF, total // 2 - 1] + code
        words += [0x5362724F, 0x00726468, (total * 4) << 8, 0, 0xDEADBEEF, h, 0]
        out_s += lines + [""]
        out_h.append("/* %s: op1 %s%s. Hash %08X. */" % (
            name, ds_short(op1), ", op2 " + ds_short(op2) if op2 else "", h))
        out_h.append("static const uint32_t cs_oc_%s[] __attribute__((aligned(256))) = {" % name)
        for i in range(0, len(words), 8):
            out_h.append("    " + " ".join("0x%08X," % x for x in words[i:i + 8]))
        out_h.append("};")
        names.append(name)
    out_h.append("")
    out_h.append("enum {")
    for n in names:
        out_h.append("    CS_OC_%s," % n.upper())
    out_h.append("    CS_OC_COUNT")
    out_h.append("};")
    out_h.append("struct CsOcBin {")
    out_h.append("    const uint32_t* bin;")
    out_h.append("    uint32_t size;")
    out_h.append("    uint32_t has_op2;")
    out_h.append("    const char* name;")
    out_h.append("};")
    out_h.append("static const struct CsOcBin k_cs_oc[CS_OC_COUNT] = {")
    for name, h, op1, op2 in VARIANTS:
        out_h.append("    {cs_oc_%s, sizeof(cs_oc_%s), %d, \"%s\"}," % (name, name, 1 if op2 else 0,
                                                                     name))
    out_h.append("};")
    open("shaders/cs_ordcnt.s", "w").write("\n".join(out_s) + "\n")
    open("shaders/cs_ordcnt.h", "w").write("\n".join(out_h) + "\n")


if __name__ == "__main__":
    main()
