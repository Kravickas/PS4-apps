#!/usr/bin/env python3
"""Generates shaders/ps_triangle.s and shaders/ps_triangle.h: the shaders of TRIANGLE_TEST
(src/triangle_test.h).

tri_vs_fetch  VS: calls the fetch shader (s[0:1], V# table at s[2:3]), then loads the same V#
              with its own buffer_load_format_xyzw and stores both results, 32 bytes per vertex:
              fetch v[4:7] at +0, direct v[8:11] at +16, through the store V# at table dword 4.
              Position outside the clip volume (points are culled).
tri_fetch     the fetch shader (no header / trailer): s_load_dwordx4 s[8:11], s[2:3], 0,
              buffer_load_format_xyzw v[4:7], v0, s[8:11], 0 idxen, s_setpc_b64 s[0:1].
tri_vs_full   VS: one triangle over the whole viewport from the vertex index, z 0, w 1.
tri_vs_index  VS: the vertex index (the index buffer value) marks its dword in the flag buffer
              (V# at table dword 0, user data s[0:1]); position outside the clip volume.
tri_vs_raw    VS: raw loads of the data V# (table dword 0) at byte offset 5 * vertex index: dword,
              ushort, ubyte, dwordx2, and s_buffer_load_dword at dword offset 1 and at the SGPR
              byte offset 6; stores dword, ushort, ubyte, s_buffer imm, dwordx2 (2), s_buffer
              sgpr, 0 - 32 bytes per vertex through the store V# at table dword 4. User data
              s[0:1] the table. Position outside the clip volume.
tri_ps_cov    PS: marks the pixel in the coverage map (V# at table dword 0; dword 4 the map
              pitch in pixels, dword 5 the value) and exports table dwords 6..9 as MRT0 (32_ABGR).
              Inputs: PERSP_CENTER, POS_X_FLOAT (v2), POS_Y_FLOAT (v3).

Needs llvm-mc-18 (-mcpu=bonaire). Run from the project root."""
import os, re, struct, subprocess, sys

SHADERS = [
    ("tri_vs_fetch", 0xA2A0D201, "vs", [
        "s_swappc_b64 s[0:1], s[0:1]",
        "s_load_dwordx4 s[8:11], s[2:3], 0x0",
        "s_load_dwordx4 s[12:15], s[2:3], 0x4",
        "s_waitcnt lgkmcnt(0)",
        "buffer_load_format_xyzw v[8:11], v0, s[8:11], 0 idxen",
        "s_waitcnt vmcnt(0)",
        "v_lshlrev_b32 v12, 5, v0",
        "buffer_store_dwordx4 v[4:7], v12, s[12:15], 0 offen",
        "buffer_store_dwordx4 v[8:11], v12, s[12:15], 0 offen offset:16",
        "s_waitcnt vmcnt(0) expcnt(0)",
        "v_mov_b32 v1, -2.0",
        "v_mov_b32 v2, 0",
        "v_mov_b32 v3, 1.0",
        "exp param0 v2, v2, v2, v2",
        "exp pos0 v1, v1, v2, v3 done",
    ]),
    ("tri_vs_full", 0xA2A0D202, "vs", [
        "v_and_b32 v1, 1, v0",
        "v_lshrrev_b32 v2, 1, v0",
        "v_and_b32 v2, 1, v2",
        "v_cvt_f32_u32 v1, v1",
        "v_cvt_f32_u32 v2, v2",
        "v_mul_f32 v1, 4.0, v1",
        "v_add_f32 v1, -1.0, v1",
        "v_mul_f32 v2, 4.0, v2",
        "v_add_f32 v2, -1.0, v2",
        "v_mov_b32 v3, 0",
        "v_mov_b32 v4, 1.0",
        "exp param0 v3, v3, v3, v3",
        "exp pos0 v1, v2, v3, v4 done",
    ]),
    ("tri_vs_index", 0xA2A0D203, "vs", [
        "s_load_dwordx4 s[4:7], s[0:1], 0x0",
        "s_waitcnt lgkmcnt(0)",
        "v_lshlrev_b32 v1, 2, v0",
        "v_mov_b32 v2, 1",
        "buffer_store_dword v2, v1, s[4:7], 0 offen",
        "s_waitcnt vmcnt(0) expcnt(0)",
        "v_mov_b32 v3, -2.0",
        "v_mov_b32 v4, 0",
        "v_mov_b32 v5, 1.0",
        "exp param0 v4, v4, v4, v4",
        "exp pos0 v3, v3, v4, v5 done",
    ]),
    ("tri_vs_raw", 0xA2A0D205, "vs", [
        "s_load_dwordx4 s[8:11], s[0:1], 0x0",
        "s_load_dwordx4 s[12:15], s[0:1], 0x4",
        "s_mov_b32 s18, 6",
        "s_waitcnt lgkmcnt(0)",
        "v_mul_u32_u24 v1, 5, v0",
        "buffer_load_dword v4, v1, s[8:11], 0 offen",
        "buffer_load_ushort v5, v1, s[8:11], 0 offen",
        "buffer_load_ubyte v6, v1, s[8:11], 0 offen",
        "buffer_load_dwordx2 v[8:9], v1, s[8:11], 0 offen",
        "s_buffer_load_dword s16, s[8:11], 0x1",
        "s_buffer_load_dword s17, s[8:11], s18",
        "s_waitcnt vmcnt(0) lgkmcnt(0)",
        "v_mov_b32 v7, s16",
        "v_mov_b32 v10, s17",
        "v_mov_b32 v11, 0",
        "v_lshlrev_b32 v12, 5, v0",
        "buffer_store_dwordx4 v[4:7], v12, s[12:15], 0 offen",
        "buffer_store_dwordx4 v[8:11], v12, s[12:15], 0 offen offset:16",
        "s_waitcnt vmcnt(0) expcnt(0)",
        "v_mov_b32 v13, -2.0",
        "v_mov_b32 v14, 0",
        "v_mov_b32 v15, 1.0",
        "exp param0 v14, v14, v14, v14",
        "exp pos0 v13, v13, v14, v15 done",
    ]),
    ("tri_ps_cov", 0xA2A0D204, "ps", [
        "s_load_dwordx4 s[4:7], s[0:1], 0x0",
        "s_load_dwordx8 s[8:15], s[0:1], 0x4",
        "s_waitcnt lgkmcnt(0)",
        "v_cvt_u32_f32 v4, v2",
        "v_cvt_u32_f32 v5, v3",
        "v_mul_lo_u32 v6, v5, s8",
        "v_add_i32 v6, vcc, v6, v4",
        "v_lshlrev_b32 v6, 2, v6",
        "v_mov_b32 v7, s9",
        "buffer_store_dword v7, v6, s[4:7], 0 offen",
        "v_mov_b32 v8, s10",
        "v_mov_b32 v9, s11",
        "v_mov_b32 v10, s12",
        "v_mov_b32 v11, s13",
        "s_waitcnt vmcnt(0) expcnt(0)",
        "exp mrt0 v8, v9, v10, v11 done vm",
    ]),
]

FETCH = [
    "s_load_dwordx4 s[8:11], s[2:3], 0x0",
    "s_waitcnt lgkmcnt(0)",
    "buffer_load_format_xyzw v[4:7], v0, s[8:11], 0 idxen",
    "s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)",
    "s_setpc_b64 s[0:1]",
]


def assemble(text):
    p = subprocess.run(["llvm-mc-18", "-triple=amdgcn", "-mcpu=bonaire", "-filetype=obj", "-o",
                        "/tmp/ps_triangle.o", "-"], input=text, capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stderr)
    subprocess.run(["llvm-objcopy-18", "-O", "binary", "--only-section=.text", "/tmp/ps_triangle.o",
                    "/tmp/ps_triangle.bin"], check=True)
    d = open("/tmp/ps_triangle.bin", "rb").read()
    return list(struct.unpack("<%dI" % (len(d) // 4), d))


def regs_used(lines):
    v = s = 0
    for ln in lines:
        for a, b in re.findall(r"\bv\[(\d+):(\d+)\]", ln):
            v = max(v, int(b))
        for a in re.findall(r"\bv(\d+)\b", ln):
            v = max(v, int(a))
        for a, b in re.findall(r"\bs\[(\d+):(\d+)\]", ln):
            s = max(s, int(b))
        for a in re.findall(r"\bs(\d+)\b", ln):
            s = max(s, int(a))
    return v, s


def build(body):
    # The header token s_mov_b32 vcc_hi, <literal> as raw words: llvm-mc would encode a small value
    # as an inline constant (0xBEEB03xx), the SDK token is always the literal form 0xBEEB03FF.
    lines = [".long 0xbeeb03ff", ".long HDRLEN"] + body + ["s_endpgm"]
    words = assemble("\n".join(lines).replace("HDRLEN", "0x7fff") + "\n")
    if len(words) % 2:
        lines.insert(len(lines) - 1, "s_nop 0")
        words = assemble("\n".join(lines).replace("HDRLEN", "0x7fff") + "\n")
    hdrlen = len(words) // 2 - 1
    text = "\n".join(lines).replace("HDRLEN", "0x%x" % hdrlen) + "\n"
    return text, assemble(text)


def emit(out, name, words):
    out.append("static const uint32_t %s_bin[] __attribute__((aligned(256))) = {" % name)
    for i in range(0, len(words), 8):
        out.append("    " + " ".join("0x%08X," % x for x in words[i:i + 8]))
    out.append("};")
    out.append("")


root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sh = os.path.join(root, "shaders")
src = ["; TRIANGLE_TEST shaders, generated by tools/gen_ps_triangle.py"]
out = ["#pragma once", "#include <stdint.h>", "",
       "// TRIANGLE_TEST shaders: shaders/ps_triangle.s, generated by tools/gen_ps_triangle.py",
       "// (its docstring has the inputs and table layouts).", ""]
for name, h, stage, body in SHADERS:
    text, words = build(body)
    src.append("; ---- %s (%s) ----" % (name, stage))
    src.append(text.rstrip("\n"))
    vmax, smax = regs_used(text.splitlines())
    vg = vmax + 1
    sg = smax + 1 + 2  # + VCC (the header token writes vcc_hi)
    rsrc1 = (((sg + 7) // 8 - 1) << 6) | ((vg + 3) // 4 - 1)
    trailer = [0x5362724F, 0x00726468, (len(words) * 4) << 8, 0, 0xDEADBEEF, h, 0]
    out.append("#define %s_RSRC1 0x%08Xu /* v0-v%d, s0-s%d + VCC */" %
               (name.upper(), rsrc1, vg - 1, smax))
    emit(out, name, words + trailer)
text = "\n".join(FETCH) + "\n"
src.append("; ---- tri_fetch (fetch shader, no header / trailer) ----")
src.append(text.rstrip("\n"))
emit(out, "tri_fetch", assemble(text))
open(os.path.join(sh, "ps_triangle.s"), "w").write("\n".join(src) + "\n")
open(os.path.join(sh, "ps_triangle.h"), "w").write("\n".join(out))
print("shaders: %s, tri_fetch" % ", ".join(n for n, _, _, _ in SHADERS))
