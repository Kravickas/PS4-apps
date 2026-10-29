#!/usr/bin/env python3
"""Generates shaders/ps_a2m.s and shaders/ps_a2m.h: the alpha-to-mask hardware test (A2M_TEST).

vs_a2m: one triangle over the whole viewport from the vertex index (no vertex buffer); z from
s0 (VS_USER_DATA_0), w 1, one parameter export.

Test pixel shaders (inputs: s[0:1] the test table, v2 / v3 POS_X / POS_Y_FLOAT): pixel
i = x + 64 y of the 64 x 64 target; A = table A[i], B = table B[i] (buffer_load through the V# at
table dwords 0..3; B at byte 0x4000). Table dwords: 4 MRT0 alpha (variants with a constant MRT0
alpha), 5 MRTZ alpha (constant), 8 the exported Z, 9 the colour RGB, 11 t_iter's alpha step per
sample, 32 / 33 t_raw's colour halves (RG pair, B).
  t_f32   MRT0 (colour, colour, colour, A), uncompressed
  t_f16   MRT0 v_cvt_pkrtz_f16_f32, compressed
  t_u16   MRT0 v_cvt_pknorm_u16_f32, compressed
  t_raw   MRT0 compressed, alpha = A[15:0] as is (any 16-bit export format)
  t_za    MRTZ (Z, -, -, A), MRT0 (colour x 3, table 4)
  t_az    MRTZ (Z, -, -, table 5), MRT0 (colour x 3, A)
  t_zab   MRTZ (Z, -, -, B), MRT0 (colour x 3, A)
  t_za0   MRTZ (Z, -, -, A) only
  t_mask  MRTZ (Z, -, B as the sample mask, -), MRT0 (colour x 3, A)
  t_kill  v_cmpx kill where B & 1, MRT0 (colour x 3, A)
  t_m1    MRT0 (colour x 3, table 4), MRT1 (colour x 3, A)
  t_m1o   MRT1 (colour x 3, A) only
  t_iter  MRT0 alpha A + sample id x table 11 (sample id: ANCILLARY [11:8], v4)
clr: MRT0 0.25 x 4 (the clear draw; the depth clear is DB_RENDER_CONTROL's).
rd: the readback: for sample s < table 10, image_load the colour (T# at 16) and the depth (T# at
24) at (x, y, s) and store them raw at the V# at 12: colour at ((y 64 + x) 8 + s) 4, depth at
0x20000 + the same; MRT0 0 into a scratch target.
vis: the visual pages (see vis_shader).

Needs llvm-mc-18 (-mcpu=bonaire). Run from the project root."""
import os, struct, subprocess, sys

PS_HEAD = [
    "s_load_dwordx4 s[4:7], s[0:1], 0x0",
    "s_load_dwordx8 s[8:15], s[0:1], 0x4",
    "s_waitcnt lgkmcnt(0)",
    "v_cvt_u32_f32 v4, v2",
    "v_cvt_u32_f32 v5, v3",
    "v_lshlrev_b32 v6, 6, v5",
    "v_add_i32 v6, vcc, v6, v4",
    "v_lshlrev_b32 v8, 2, v6",
    "v_add_i32 v18, vcc, 0x4000, v8",
    "buffer_load_dword v9, v8, s[4:7], 0 offen",
    "buffer_load_dword v17, v18, s[4:7], 0 offen",
    "s_waitcnt vmcnt(0)",
    "v_mov_b32 v10, s13",
    "v_mov_b32 v13, s12",
]

EXP_F32 = ["exp mrt0 v10, v10, v10, v9 done vm"]

# Visual composite (ps vis): background, three panel rows, the UI labels. Table: 16..23 UI T#,
# 24..27 its S#, row r at 32 + 16 r: valid, ns, microscope x, y, column / row shift (log2 of the
# sample grid), 1 / ns (float), -, result V# (8..11). Result dword per pixel: [7:0] GPU mask,
# [15:8] model mask.
VIS_Y0, VIS_PITCH, VIS_H = 120, 312, 256
VIS_X = (420, 692, 964, 1236)


def vis_shader():
    a = [
        "s_load_dwordx8 s[4:11], s[0:1], 0x10",
        "s_load_dwordx4 s[12:15], s[0:1], 0x18",
        "v_cvt_u32_f32 v4, v2",
        "v_cvt_u32_f32 v5, v3",
        "v_mov_b32 v20, 0x3d75c28f",
        "v_mov_b32 v21, 0x3d75c28f",
        "v_mov_b32 v22, 0x3d75c28f",
        "v_mov_b32 v40, 0",
        "v_mov_b32 v41, 1.0",
        "v_mov_b32 v42, 0x3f0ccccd",
        "v_mov_b32 v43, 0x3df5c28f",
        "v_mov_b32 v44, 0x3e4ccccd",
        "v_mov_b32 v45, 0x3e19999a",
        "v_mov_b32 v46, 0x3ecccccd",
        "s_mov_b64 s[30:31], exec",
    ]
    for r in range(3):
        L = "r%d" % r
        a += [
            "s_load_dwordx8 s[16:23], s[0:1], 0x%x" % (32 + 16 * r),
            "s_load_dwordx4 s[24:27], s[0:1], 0x%x" % (40 + 16 * r),
            "s_waitcnt lgkmcnt(0)",
            "s_cmp_eq_u32 s16, 0",
            "s_cbranch_scc1 %s_end" % L,
            "v_subrev_i32 v6, vcc, 0x%x, v5" % (VIS_Y0 + VIS_PITCH * r),
            "v_cmp_gt_u32_e32 vcc, 0x%x, v6" % VIS_H,
            "s_and_saveexec_b64 s[32:33], vcc",
            "s_cbranch_execz %s_out" % L,
        ]
        for kind, base in enumerate(VIS_X):
            K = "%s_k%d" % (L, kind)
            width = 512 if kind == 3 else 256
            a += [
                "v_subrev_i32 v7, vcc, 0x%x, v4" % base,
                "v_cmp_gt_u32_e32 vcc, 0x%x, v7" % width,
                "s_and_saveexec_b64 s[34:35], vcc",
                "s_cbranch_execz %s_skip" % K,
            ]
            if kind < 3:
                a += [
                    "v_lshrrev_b32 v8, 2, v7",
                    "v_lshrrev_b32 v9, 2, v6",
                ]
            else:
                a += [
                    "v_lshrrev_b32 v8, 5, v7",
                    "v_lshrrev_b32 v9, 5, v6",
                    "v_add_i32 v8, vcc, s18, v8",
                    "v_add_i32 v9, vcc, s19, v9",
                    "v_and_b32 v8, 63, v8",
                    "v_and_b32 v9, 63, v9",
                ]
            a += [
                "v_lshlrev_b32 v9, 6, v9",
                "v_add_i32 v9, vcc, v9, v8",
                "v_lshlrev_b32 v9, 2, v9",
                "buffer_load_dword v10, v9, s[24:27], 0 offen",
                "s_waitcnt vmcnt(0)",
                "v_and_b32 v11, 0xff, v10",
                "v_bfe_u32 v12, v10, 8, 8",
            ]
            if kind in (0, 1):
                a += [
                    "v_bcnt_u32_b32 v13, v%d, 0" % (11 if kind == 0 else 12),
                    "v_cvt_f32_u32 v13, v13",
                    "v_mul_f32 v13, s22, v13",
                    "v_mov_b32 v20, v13",
                    "v_mov_b32 v21, v13",
                    "v_mov_b32 v22, v13",
                ]
            elif kind == 2:
                a += [
                    "v_cmp_eq_u32_e32 vcc, v11, v12",
                    "v_cndmask_b32 v20, v41, v40, vcc",
                    "v_cndmask_b32 v21, v40, v42, vcc",
                    "v_mov_b32 v22, 0",
                ]
            else:
                a += [
                    "v_and_b32 v14, 31, v7",       # sub x
                    "v_and_b32 v15, 31, v6",       # sub y
                    "s_sub_u32 s28, 5, s20",
                    "s_sub_u32 s29, 5, s21",
                    "v_lshrrev_b32 v16, s28, v14",
                    "v_lshrrev_b32 v17, s29, v15",
                    "v_lshlrev_b32 v17, s20, v17",
                    "v_add_i32 v16, vcc, v16, v17",  # sample index
                    "v_lshrrev_b32 v18, v16, v11",
                    "v_and_b32 v18, 1, v18",         # GPU bit
                    "v_lshrrev_b32 v19, v16, v12",
                    "v_and_b32 v19, 1, v19",         # model bit
                    "v_lshlrev_b32 v18, 1, v18",
                    "v_or_b32 v18, v18, v19",        # code: 3 both, 2 GPU only, 1 model only
                    "v_mov_b32 v20, v43",
                    "v_mov_b32 v21, v43",
                    "v_mov_b32 v22, v43",
                    "v_cmp_eq_u32_e32 vcc, 3, v18",
                    "v_cndmask_b32 v20, v20, v41, vcc",
                    "v_cndmask_b32 v21, v21, v41, vcc",
                    "v_cndmask_b32 v22, v22, v41, vcc",
                    "v_cmp_eq_u32_e32 vcc, 2, v18",
                    "v_cndmask_b32 v20, v20, v41, vcc",
                    "v_cndmask_b32 v21, v21, v43, vcc",
                    "v_cndmask_b32 v22, v22, v43, vcc",
                    "v_cmp_eq_u32_e32 vcc, 1, v18",
                    "v_cndmask_b32 v20, v20, v44, vcc",
                    "v_cndmask_b32 v21, v21, v46, vcc",
                    "v_cndmask_b32 v22, v22, v41, vcc",
                    # sub-cell borders: (sub & (size - 1)) == 0
                    "s_lshr_b32 s28, 32, s20",
                    "s_sub_u32 s28, s28, 1",
                    "s_lshr_b32 s29, 32, s21",
                    "s_sub_u32 s29, s29, 1",
                    "v_and_b32 v18, s28, v14",
                    "v_and_b32 v19, s29, v15",
                    "v_min_u32 v18, v18, v19",
                    "v_cmp_eq_u32_e32 vcc, 0, v18",
                    "v_cndmask_b32 v20, v20, v45, vcc",
                    "v_cndmask_b32 v21, v21, v45, vcc",
                    "v_cndmask_b32 v22, v22, v45, vcc",
                ]
            a += ["%s_skip:" % K, "s_mov_b64 exec, s[34:35]"]
        a += ["%s_out:" % L, "s_mov_b64 exec, s[32:33]", "%s_end:" % L]
    a += [
        "s_mov_b64 exec, s[30:31]",
        "v_mul_f32 v29, 0x3a088889, v2",  # x / 1920
        "v_mul_f32 v30, 0x3a72b9d6, v3",  # y / 1080
        "image_sample_lz v[29:32], v[29:30], s[4:11], s[12:15] dmask:0xf",
        "s_waitcnt vmcnt(0)",
        "v_sub_f32 v32, 1.0, v32",
        "v_mul_f32 v20, v20, v32",
        "v_add_f32 v20, v20, v29",
        "v_mul_f32 v21, v21, v32",
        "v_add_f32 v21, v21, v30",
        "v_mul_f32 v22, v22, v32",
        "v_add_f32 v22, v22, v31",
        "exp mrt0 v20, v21, v22, v41 done vm",
    ]
    return a


SHADERS = [
    # name, hash, stage, body
    ("vs_a2m", 0xA2A0C101, "vs", [
        "v_and_b32 v1, 1, v0",
        "v_lshrrev_b32 v2, 1, v0",
        "v_and_b32 v2, 1, v2",
        "v_cvt_f32_u32 v1, v1",
        "v_cvt_f32_u32 v2, v2",
        "v_mul_f32 v1, 4.0, v1",
        "v_add_f32 v1, -1.0, v1",
        "v_mul_f32 v2, 4.0, v2",
        "v_add_f32 v2, -1.0, v2",
        "v_mov_b32 v3, s0",
        "v_mov_b32 v4, 1.0",
        "exp param0 v1, v1, v1, v1",
        "exp pos0 v1, v2, v3, v4 done",
    ]),
    ("t_f32", 0xA2A0C102, "ps", PS_HEAD + EXP_F32),
    ("t_f16", 0xA2A0C103, "ps", PS_HEAD + [
        "v_cvt_pkrtz_f16_f32 v11, v10, v10",
        "v_cvt_pkrtz_f16_f32 v12, v10, v9",
        "exp mrt0 v11, v11, v12, v12 done compr vm",
    ]),
    ("t_u16", 0xA2A0C104, "ps", PS_HEAD + [
        "v_cvt_pknorm_u16_f32 v11, v10, v10",
        "v_cvt_pknorm_u16_f32 v12, v10, v9",
        "exp mrt0 v11, v11, v12, v12 done compr vm",
    ]),
    ("t_raw", 0xA2A0C105, "ps", PS_HEAD + [
        "s_load_dwordx2 s[16:17], s[0:1], 0x20",
        "s_waitcnt lgkmcnt(0)",
        "v_mov_b32 v11, s16",
        "v_lshlrev_b32 v12, 16, v9",
        "v_or_b32 v12, s17, v12",
        "exp mrt0 v11, v11, v12, v12 done compr vm",
    ]),
    ("t_za", 0xA2A0C106, "ps", PS_HEAD + [
        "v_mov_b32 v14, s8",
        "exp mrtz v13, off, off, v9",
        "exp mrt0 v10, v10, v10, v14 done vm",
    ]),
    ("t_az", 0xA2A0C107, "ps", PS_HEAD + [
        "v_mov_b32 v14, s9",
        "exp mrtz v13, off, off, v14",
        "exp mrt0 v10, v10, v10, v9 done vm",
    ]),
    ("t_zab", 0xA2A0C108, "ps", PS_HEAD + [
        "exp mrtz v13, off, off, v17",
        "exp mrt0 v10, v10, v10, v9 done vm",
    ]),
    ("t_za0", 0xA2A0C109, "ps", PS_HEAD + [
        "exp mrtz v13, off, off, v9 done vm",
    ]),
    ("t_mask", 0xA2A0C10A, "ps", PS_HEAD + [
        "exp mrtz v13, off, v17, off",
        "exp mrt0 v10, v10, v10, v9 done vm",
    ]),
    ("t_kill", 0xA2A0C10B, "ps", PS_HEAD + [
        "v_and_b32 v16, 1, v17",
        "v_cmpx_eq_u32 vcc, 0, v16",
        "exp mrt0 v10, v10, v10, v9 done vm",
    ]),
    ("t_m1", 0xA2A0C10C, "ps", PS_HEAD + [
        "v_mov_b32 v14, s8",
        "exp mrt0 v10, v10, v10, v14",
        "exp mrt1 v10, v10, v10, v9 done vm",
    ]),
    ("t_m1o", 0xA2A0C10D, "ps", PS_HEAD + [
        "exp mrt1 v10, v10, v10, v9 done vm",
    ]),
    # per-sample alpha: SPI_PS_INPUT_ENA adds ANCILLARY (v4); sample id = ancillary[11:8]
    ("t_iter", 0xA2A0C10E, "ps", ["v_mov_b32 v19, v4"] + PS_HEAD + [
        "v_bfe_u32 v20, v19, 8, 4",
        "v_cvt_f32_u32 v20, v20",
        "v_mad_f32 v9, v20, s15, v9",
        "exp mrt0 v10, v10, v10, v9 done vm",
    ]),
    ("clr", 0xA2A0C10F, "ps", [
        "v_mov_b32 v4, 0.25",
        "exp mrt0 v4, v4, v4, v4 done vm",
    ]),
    ("rd", 0xA2A0C110, "ps", [
        "s_load_dwordx8 s[4:11], s[0:1], 0x10",
        "s_load_dwordx8 s[12:19], s[0:1], 0x18",
        "s_load_dwordx4 s[20:23], s[0:1], 0xc",
        "s_load_dword s24, s[0:1], 0xa",
        "s_waitcnt lgkmcnt(0)",
        "v_cvt_u32_f32 v4, v2",
        "v_cvt_u32_f32 v5, v3",
        "v_lshlrev_b32 v6, 6, v5",
        "v_add_i32 v6, vcc, v6, v4",
        "v_lshlrev_b32 v6, 5, v6",
        "s_mov_b32 s25, 0",
        "rd_loop:",
        "v_mov_b32 v7, v4",
        "v_mov_b32 v8, v5",
        "v_mov_b32 v9, s25",
        "image_load v10, v[7:9], s[4:11] dmask:0x1 unorm",
        "image_load v11, v[7:9], s[12:19] dmask:0x1 unorm",
        "s_lshl_b32 s26, s25, 2",
        "v_add_i32 v12, vcc, s26, v6",
        "v_add_i32 v13, vcc, 0x20000, v12",
        "s_waitcnt vmcnt(0)",
        "buffer_store_dword v10, v12, s[20:23], 0 offen",
        "buffer_store_dword v11, v13, s[20:23], 0 offen",
        "s_add_u32 s25, s25, 1",
        "s_cmp_lt_u32 s25, s24",
        "s_cbranch_scc1 rd_loop",
        "s_waitcnt vmcnt(0)",
        "v_mov_b32 v14, 0",
        "exp mrt0 v14, v14, v14, v14 done vm",
    ]),
    ("vis", 0xA2A0C111, "ps", vis_shader()),
]


def assemble(text):
    p = subprocess.run(["llvm-mc-18", "-triple=amdgcn", "-mcpu=bonaire", "-filetype=obj", "-o",
                        "/tmp/ps_a2m.o", "-"], input=text, capture_output=True, text=True)
    if p.returncode:
        sys.exit(p.stderr)
    subprocess.run(["llvm-objcopy-18", "-O", "binary", "--only-section=.text", "/tmp/ps_a2m.o",
                    "/tmp/ps_a2m.bin"], check=True)
    d = open("/tmp/ps_a2m.bin", "rb").read()
    return list(struct.unpack("<%dI" % (len(d) // 4), d))


def regs_used(lines):
    """Highest VGPR / SGPR index named in the code."""
    import re
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


def build(name, body):
    lines = ["s_mov_b32 vcc_hi, HDRLEN"] + body + ["s_endpgm"]
    words = assemble("\n".join(lines).replace("HDRLEN", "0x7fff") + "\n")
    if len(words) % 2:
        lines.insert(len(lines) - 1, "s_nop 0")
        words = assemble("\n".join(lines).replace("HDRLEN", "0x7fff") + "\n")
    hdrlen = len(words) // 2 - 1
    text = "\n".join(lines).replace("HDRLEN", "0x%x" % hdrlen) + "\n"
    return text, assemble(text)


root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sh = os.path.join(root, "shaders")
src = ["; Alpha-to-mask hardware test (A2M_TEST), generated by tools/gen_ps_a2m.py"]
out = ["#pragma once", "#include <stdint.h>", "",
       "// Alpha-to-mask hardware test (A2M_TEST): shaders/ps_a2m.s, generated by",
       "// tools/gen_ps_a2m.py (its docstring has the table layout).", ""]
names = []
for name, h, stage, body in SHADERS:
    text, words = build(name, body)
    src.append("; ---- %s (%s) ----" % (name, stage))
    src.append(text.rstrip("\n"))
    vmax, smax = regs_used(text.splitlines())
    vg = vmax + 1
    sg = smax + 1 + 2  # + VCC (the header token writes vcc_hi)
    rsrc1 = (((sg + 7) // 8 - 1) << 6) | ((vg + 3) // 4 - 1)
    trailer = [0x5362724F, 0x00726468, (len(words) * 4) << 8, 0, 0xDEADBEEF, h, 0]
    allw = words + trailer
    up = name.upper()
    out.append("#define A2M_%s_RSRC1 0x%08Xu /* v0-v%d, s0-s%d + VCC */" % (up, rsrc1, vg - 1,
                                                                            smax))
    out.append("static const uint32_t a2m_%s_bin[] __attribute__((aligned(256))) = {" % name)
    for i in range(0, len(allw), 8):
        out.append("    " + " ".join("0x%08X," % x for x in allw[i:i + 8]))
    out.append("};")
    out.append("")
    names.append(name)
open(os.path.join(sh, "ps_a2m.s"), "w").write("\n".join(src) + "\n")
open(os.path.join(sh, "ps_a2m.h"), "w").write("\n".join(out))
print("shaders: %s" % ", ".join(names))
