#!/usr/bin/env python3
"""Generates shaders/ps_clock.s: clock mode's UI pass (docs/clock_plan.txt). Follows the per-pixel
model docs/clock_model/gpumodel.py (clock_gpu) step by step; ends like ps_ui (clamp, sRGB, dither)."""
import struct, os
hx = lambda x: "0x%08x" % struct.unpack('<I', struct.pack('<f', x))[0]
L2E = 1.4426950408889634
IOR = (1.514, 1.517, 1.522); ABS = (0.0010, 0.0005, 0.0008)
out = []
def e(*ls): out.extend(ls)
class RA:
    def __init__(s, lo, hi): s.free = list(range(lo, hi)); s.used = set(); s.maxv = 0
    def get(s, n=1):
        for i in range(len(s.free) - n + 1):
            blk = s.free[i:i + n]
            if blk == list(range(blk[0], blk[0] + n)) and (n == 1 or blk[0] % 1 == 0):
                for r in blk: s.free.remove(r)
                s.maxv = max(s.maxv, blk[-1]); return blk[0] if n == 1 else blk
        raise RuntimeError("out of VGPRs")
    def put(s, *rs):
        for r in rs:
            if isinstance(r, list): s.put(*r)
            else: s.free.append(r); s.free.sort()
R = RA(22, 128)
S = lambda k: "s%d" % (44 + k - 40)     # table dword k (>= 40) -> SGPR
CX, CY, HW, HH, RCK, IW, IH, STR = S(40), S(41), S(42), S(43), S(44), S(45), S(46), S(47)
COL = (S(48), S(49), S(50)); FILL, L2X, L2Y, OX0, OY0, ITW, ITH = S(51), S(52), S(53), S(54), S(55), S(56), S(57)

def expneg_sq(dst, x, scale, bias):
    """dst = exp(-((x + bias) / scale)^2)"""
    e("v_add_f32 v%d, %s, v%d" % (dst, hx(bias), x), "v_mul_f32 v%d, %s, v%d" % (dst, hx(1.0 / scale), dst),
      "v_mul_f32 v%d, v%d, v%d" % (dst, dst, dst), "v_mul_f32 v%d, %s, v%d" % (dst, hx(-L2E), dst), "v_exp_f32 v%d, v%d" % (dst, dst))
def band(dst, x, bias, width):
    """dst = max(0, 1 - |x + bias| / width)"""
    e("v_add_f32 v%d, %s, v%d" % (dst, hx(bias), x), "v_and_b32 v%d, 0x7fffffff, v%d" % (dst, dst),
      "v_mul_f32 v%d, %s, v%d" % (dst, hx(1.0 / width), dst), "v_sub_f32 v%d, 1.0, v%d" % (dst, dst), "v_max_f32 v%d, 0, v%d" % (dst, dst))
def powi(dst, x, n):
    """dst = x^n for n in (5, 6, 20, 40): repeated products"""
    t = R.get()
    e("v_mul_f32 v%d, v%d, v%d" % (t, x, x))            # x^2
    if n == 6:
        e("v_mul_f32 v%d, v%d, v%d" % (dst, t, x), "v_mul_f32 v%d, v%d, v%d" % (dst, dst, dst))   # x^3 ^2
    else:
        e("v_mul_f32 v%d, v%d, v%d" % (t, t, t), "v_mul_f32 v%d, v%d, v%d" % (dst, t, x))       # x^5
        for _ in range({5: 0, 20: 2, 40: 3}[n]): e("v_mul_f32 v%d, v%d, v%d" % (dst, dst, dst))
    R.put(t)
def rrect(d, gx, gy, ox, oy):
    """rounded rect SDF d at (X - ox, Y - oy) and (gx, gy) its gradient (gx, gy may be None)"""
    qx, qy, mx, my, t = R.get(), R.get(), R.get(), R.get(), R.get()
    e("v_add_f32 v%d, %s, v2" % (qx, hx(-ox)), "v_subrev_f32 v%d, %s, v%d" % (qx, CX, qx))
    if gx is not None:
        e("v_cmp_gt_f32 vcc, 0, v%d" % qx, "v_cndmask_b32 v%d, 1.0, -1.0, vcc" % gx)
    e("v_and_b32 v%d, 0x7fffffff, v%d" % (qx, qx), "v_subrev_f32 v%d, %s, v%d" % (qx, HW, qx), "v_add_f32 v%d, %s, v%d" % (qx, RCK, qx))
    e("v_add_f32 v%d, %s, v3" % (qy, hx(-oy)), "v_subrev_f32 v%d, %s, v%d" % (qy, CY, qy))
    if gy is not None:
        e("v_cmp_gt_f32 vcc, 0, v%d" % qy, "v_cndmask_b32 v%d, 1.0, -1.0, vcc" % gy)
    e("v_and_b32 v%d, 0x7fffffff, v%d" % (qy, qy), "v_subrev_f32 v%d, %s, v%d" % (qy, HH, qy), "v_add_f32 v%d, %s, v%d" % (qy, RCK, qy))
    e("v_max_f32 v%d, 0, v%d" % (mx, qx), "v_max_f32 v%d, 0, v%d" % (my, qy),
      "v_mul_f32 v%d, v%d, v%d" % (t, mx, mx), "v_mac_f32 v%d, v%d, v%d" % (t, my, my), "v_sqrt_f32 v%d, v%d" % (t, t),
      "v_max_f32 v%d, v%d, v%d" % (d, qx, qy), "v_min_f32 v%d, 0, v%d" % (d, d), "v_add_f32 v%d, v%d, v%d" % (d, d, t),
      "v_subrev_f32 v%d, %s, v%d" % (d, RCK, d))
    if gx is not None:
        # L > 0: (mx, my) / L; else the larger of qx, qy picks the axis
        r_ = R.get()
        e("v_rcp_f32 v%d, v%d" % (r_, t), "v_mul_f32 v%d, v%d, v%d" % (mx, mx, r_), "v_mul_f32 v%d, v%d, v%d" % (my, my, r_),
          "v_cmp_gt_f32 vcc, v%d, v%d" % (qx, qy), "v_cndmask_b32 v%d, 0, 1.0, vcc" % r_, "v_cmp_lt_f32 vcc, 0, v%d" % t,
          "v_cndmask_b32 v%d, v%d, v%d, vcc" % (r_, r_, mx), "v_mul_f32 v%d, v%d, v%d" % (gx, gx, r_))
        e("v_cmp_gt_f32 vcc, v%d, v%d" % (qx, qy), "v_cndmask_b32 v%d, 1.0, 0, vcc" % r_, "v_cmp_lt_f32 vcc, 0, v%d" % t,
          "v_cndmask_b32 v%d, v%d, v%d, vcc" % (r_, r_, my), "v_mul_f32 v%d, v%d, v%d" % (gy, gy, r_))
        R.put(r_)
    R.put(qx, qy, mx, my, t)
def bevel(nx, ny, nz, h, s, bev, gx, gy):
    """bevel_normal: u = clamp(s / bev, 0.02, 1), slope (s < bev), n = normalize(slope g, 1), h"""
    u, sl, t = R.get(), R.get(), R.get()
    e("v_mul_f32 v%d, %s, v%d" % (u, hx(1.0 / bev), s), "v_max_f32 v%d, %s, v%d" % (u, hx(0.02), u), "v_min_f32 v%d, 1.0, v%d" % (u, u),
      "v_sub_f32 v%d, 1.0, v%d" % (u, u), "v_mul_f32 v%d, v%d, v%d" % (t, u, u), "v_sub_f32 v%d, 1.0, v%d" % (t, t),
      "v_max_f32 v%d, 0, v%d" % (t, t), "v_sqrt_f32 v%d, v%d" % (t, t), "v_mul_f32 v%d, %s, v%d" % (h, hx(bev), t),
      "v_rcp_f32 v%d, v%d" % (sl, t), "v_mul_f32 v%d, v%d, v%d" % (sl, sl, u),
      "v_cmp_gt_f32 vcc, %s, v%d" % (hx(bev), s), "v_cndmask_b32 v%d, 0, v%d, vcc" % (sl, sl),
      "v_mul_f32 v%d, v%d, v%d" % (nx, sl, gx), "v_mul_f32 v%d, v%d, v%d" % (ny, sl, gy),
      "v_mul_f32 v%d, v%d, v%d" % (t, nx, nx), "v_mac_f32 v%d, v%d, v%d" % (t, ny, ny), "v_add_f32 v%d, 1.0, v%d" % (t, t),
      "v_rsq_f32 v%d, v%d" % (nz, t), "v_mul_f32 v%d, v%d, v%d" % (nx, nx, nz), "v_mul_f32 v%d, v%d, v%d" % (ny, ny, nz))
    R.put(u, sl, t)
def refract_path(nx, ny, nz, h, c, rx, ry, lp, tirm):
    """view ray (0,0,-1) into n with 1/IOR_c: r = eta (0,0,-1) + (eta nz - sqrt(k)) n; tirm = 1 if k < 0;
    Lp = h / max(|rz|, 1e-3)"""
    eta = 1.0 / IOR[c]; k, f, rz = R.get(), R.get(), R.get()
    e("v_mul_f32 v%d, v%d, v%d" % (k, nz, nz), "v_sub_f32 v%d, 1.0, v%d" % (k, k), "v_mul_f32 v%d, %s, v%d" % (k, hx(-eta * eta), k),
      "v_add_f32 v%d, 1.0, v%d" % (k, k), "v_cmp_gt_f32 vcc, 0, v%d" % k, "v_cndmask_b32 v%d, 0, 1.0, vcc" % tirm,
      "v_max_f32 v%d, 0, v%d" % (k, k), "v_sqrt_f32 v%d, v%d" % (k, k),
      "v_mul_f32 v%d, %s, v%d" % (f, hx(eta), nz), "v_sub_f32 v%d, v%d, v%d" % (f, f, k),
      "v_mul_f32 v%d, v%d, v%d" % (rx, f, nx), "v_mul_f32 v%d, v%d, v%d" % (ry, f, ny),
      "v_mul_f32 v%d, v%d, v%d" % (rz, f, nz), "v_add_f32 v%d, %s, v%d" % (rz, hx(-eta), rz),
      "v_and_b32 v%d, 0x7fffffff, v%d" % (rz, rz), "v_max_f32 v%d, %s, v%d" % (rz, hx(1e-3), rz),
      "v_rcp_f32 v%d, v%d" % (rz, rz), "v_mul_f32 v%d, v%d, v%d" % (lp, h, rz))
    R.put(k, f, rz)

e("; ps_clock: clock mode's UI pass (docs/clock_plan.txt, generated by tools/gen_ps_clock.py from the",
  "; per-pixel model docs/clock_model/gpumodel.py clock_gpu): the edge-lit frosted glass slab (bevel",
  "; refraction per channel, frost, grain, Fresnel, the internal light, exit glow, glints, its shadow)",
  "; and the glass text lying on it; then clamp, sRGB and dither like ps_ui. Table: [0] frame T#, [8]",
  "; bilinear clamp S#, [12] heavy frost T# (60 x 34), [20] internal light T# (480 x 270), [28] text",
  "; T# (RG8 1152 x 648), [36] point S#; [40] slab centre x, y, half w, h, corner radius, 1/1920,",
  "; 1/1080, strength | light colour RGB, fill, l2 x, y, slab origin x, y | 1/1152, 1/648 | the clock",
  "; source's dot: centre x, y, radius (off: -100), linear RGB - an antialiased disc over a shadow",
  "; (0.449 black at +2.8 px, the time panel's scaled), after everything else.",
  "s_load_dwordx8 s[4:11], s[0:1], 0x0", "s_load_dwordx4 s[12:15], s[0:1], 0x8", "s_load_dwordx8 s[16:23], s[0:1], 0xc",
  "s_load_dwordx8 s[24:31], s[0:1], 0x14", "s_load_dwordx8 s[32:39], s[0:1], 0x1c", "s_load_dwordx4 s[40:43], s[0:1], 0x24",
  "s_load_dwordx16 s[44:59], s[0:1], 0x28", "s_load_dwordx2 s[60:61], s[0:1], 0x38",
  "s_load_dwordx4 s[68:71], s[0:1], 0x3a", "s_load_dwordx2 s[72:73], s[0:1], 0x3e",
  "s_mov_b32 s62, 0x046f4f3d", "s_mov_b32 s63, 0x0127409f", "s_mov_b32 s64, 0x4bf14f21", "s_waitcnt lgkmcnt(0)")
# frame and light field at the pixel
uv = R.get(2); e("v_mul_f32 v%d, %s, v2" % (uv[0], IW), "v_mul_f32 v%d, %s, v3" % (uv[1], IH))
fr = R.get(3); lf = R.get(3); fq = R.get(3)
e("image_sample_lz v[%d:%d], v[%d:%d], s[4:11], s[12:15] dmask:0x7" % (fr[0], fr[2], uv[0], uv[1]),
  "image_sample_lz v[%d:%d], v[%d:%d], s[24:31], s[12:15] dmask:0x7" % (lf[0], lf[2], uv[0], uv[1]),
  "image_sample_lz v[%d:%d], v[%d:%d], s[16:23], s[12:15] dmask:0x7" % (fq[0], fq[2], uv[0], uv[1]))
# slab SDF, gradient, coverage, bevel
d, gx, gy = R.get(), R.get(), R.get(); rrect(d, gx, gy, 0.0, 0.0)
m = R.get(); e("v_sub_f32 v%d, 0.5, v%d" % (m, d), "v_max_f32 v%d, 0, v%d" % (m, m), "v_min_f32 v%d, 1.0, v%d" % (m, m))
s_ = R.get(); e("v_sub_f32 v%d, 0, v%d" % (s_, d), "v_max_f32 v%d, 0, v%d" % (s_, s_))
nx, ny, nz, h = R.get(), R.get(), R.get(), R.get(); bevel(nx, ny, nz, h, s_, 40.0, gx, gy); R.put(s_)
tab = R.get(3)
e("s_waitcnt vmcnt(0)")
res = R.get(3)
for c in range(3): e("v_mov_b32 v%d, v%d" % (res[c], fr[c]))
e("; waves entirely > 100 px outside the slab: the frame (the slab's shadow there is < 1e-7)",
  "v_cmp_gt_f32 vcc, %s, v%d" % (hx(100.0), d), "s_or_b32 s65, vcc_lo, vcc_hi", "s_cbranch_scc0 clock_done")
for c in range(3):
    (rx, ry), lp, tm = R.get(2), R.get(), R.get()
    refract_path(nx, ny, nz, h, c, rx, ry, lp, tm)
    ex, ey, e2, ez = R.get(), R.get(), R.get(), R.get()
    e("v_mul_f32 v%d, %s, v%d" % (ex, hx(IOR[c]), rx), "v_mul_f32 v%d, %s, v%d" % (ey, hx(IOR[c]), ry),
      "v_mul_f32 v%d, v%d, v%d" % (e2, ex, ex), "v_mac_f32 v%d, v%d, v%d" % (e2, ey, ey),
      "v_cmp_le_f32 vcc, 1.0, v%d" % e2, "v_cndmask_b32 v%d, v%d, 1.0, vcc" % (tm, tm),
      "v_sub_f32 v%d, 1.0, v%d" % (ez, e2), "v_max_f32 v%d, %s, v%d" % (ez, hx(1e-4), ez), "v_rsq_f32 v%d, v%d" % (ez, ez),
      "v_mul_f32 v%d, %s, v%d" % (ez, hx(300.0), ez),
      "v_mul_f32 v%d, v%d, v%d" % (rx, rx, lp), "v_mac_f32 v%d, v%d, v%d" % (rx, ex, ez),
      "v_mul_f32 v%d, v%d, v%d" % (ry, ry, lp), "v_mac_f32 v%d, v%d, v%d" % (ry, ey, ez),
      "v_add_f32 v%d, v%d, v2" % (rx, rx), "v_add_f32 v%d, v%d, v3" % (ry, ry),
      "v_mul_f32 v%d, %s, v%d" % (rx, IW, rx), "v_mul_f32 v%d, %s, v%d" % (ry, IH, ry))
    R.put(ex, ey, e2, ez)
    smp = R.get(3)
    e("image_sample_lz v[%d:%d], v[%d:%d], s[16:23], s[12:15] dmask:0x7" % (smp[0], smp[2], rx, ry), "s_waitcnt vmcnt(0)")
    q = R.get()
    e("v_mul_f32 v%d, %s, v%d" % (q, hx(0.35), fq[c]), "v_cmp_lt_f32 vcc, 0, v%d" % tm,
      "v_cndmask_b32 v%d, v%d, v%d, vcc" % (tab[c], smp[c], q),
      "v_mul_f32 v%d, %s, v%d" % (q, hx(-ABS[c] * L2E), lp), "v_exp_f32 v%d, v%d" % (q, q),
      "v_mul_f32 v%d, v%d, v%d" % (tab[c], tab[c], q))
    R.put(q, rx, ry, lp, tm); R.put(*smp)
# Fresnel with the key light
F, kr, t = R.get(), R.get(), R.get()
e("v_sub_f32 v%d, 1.0, v%d" % (F, nz), "v_mul_f32 v%d, v%d, v%d" % (t, F, F), "v_mul_f32 v%d, v%d, v%d" % (t, t, t),
  "v_mul_f32 v%d, v%d, v%d" % (F, F, t), "v_mul_f32 v%d, %s, v%d" % (F, hx(0.96), F), "v_add_f32 v%d, %s, v%d" % (F, hx(0.04), F),
  "v_mul_f32 v%d, %s, v%d" % (kr, hx(0.45), nx), "v_mac_f32 v%d, %s, v%d" % (kr, hx(0.89), ny), "v_max_f32 v%d, 0, v%d" % (kr, kr))
powi(t, kr, 6); e("v_mul_f32 v%d, %s, v%d" % (t, hx(1.6), t), "v_add_f32 v%d, %s, v%d" % (t, hx(0.10), t), "v_mul_f32 v%d, v%d, v%d" % (t, t, F),
  "v_sub_f32 v%d, 1.0, v%d" % (F, F))
for c in range(3): e("v_mul_f32 v%d, v%d, v%d" % (tab[c], tab[c], F), "v_add_f32 v%d, v%d, v%d" % (tab[c], tab[c], t))
R.put(F, kr, t, nx, ny, nz, h)
# internal light: I, T = normalize(field.gb)
I, tx, ty = lf[0], lf[1], lf[2]; t = R.get()
e("v_mul_f32 v%d, v%d, v%d" % (t, tx, tx), "v_mac_f32 v%d, v%d, v%d" % (t, ty, ty), "v_add_f32 v%d, %s, v%d" % (t, hx(1e-18), t),
  "v_rsq_f32 v%d, v%d" % (t, t), "v_mul_f32 v%d, v%d, v%d" % (tx, tx, t), "v_mul_f32 v%d, v%d, v%d" % (ty, ty, t))
Ic = R.get(); e("v_max_f32 v%d, 0, v%d" % (Ic, I), "v_min_f32 v%d, 2.0, v%d" % (Ic, Ic), "v_mul_f32 v%d, v%d, v%d" % (Ic, Ic, m),
  "v_mul_f32 v%d, %s, v%d" % (Ic, FILL, Ic), "v_mul_f32 v%d, %s, v%d" % (Ic, STR, Ic))
for c in range(3): e("v_mac_f32 v%d, %s, v%d" % (tab[c], COL[c], Ic))
# exit glow and glints on the rim
b3, wd, ndT, ndl = R.get(), R.get(), R.get(), R.get()
band(b3, d, 3.0, 3.0); expneg_sq(wd, d, 7.0, 3.0)
e("v_mul_f32 v%d, v%d, v%d" % (ndT, gx, tx), "v_mac_f32 v%d, v%d, v%d" % (ndT, gy, ty), "v_max_f32 v%d, 0, v%d" % (ndT, ndT),
  "v_mul_f32 v%d, s%s, v%d" % (ndl, L2X[1:], gx), "v_mac_f32 v%d, s%s, v%d" % (ndl, L2Y[1:], gy), "v_sub_f32 v%d, 0, v%d" % (ndl, ndl), "v_max_f32 v%d, 0, v%d" % (ndl, ndl))
ex_ = R.get(); e("v_mul_f32 v%d, %s, v%d" % (ex_, hx(0.9), b3), "v_mac_f32 v%d, %s, v%d" % (ex_, hx(0.4), wd),
  "v_max_f32 v%d, 0, v%d" % (t, I), "v_min_f32 v%d, %s, v%d" % (t, hx(1.5), t), "v_mul_f32 v%d, v%d, v%d" % (ex_, ex_, t),
  "v_mul_f32 v%d, v%d, v%d" % (ex_, ex_, ndT), "v_mul_f32 v%d, %s, v%d" % (ex_, STR, ex_))
p40, p20, gw = R.get(), R.get(), R.get()
powi(p40, ndl, 40); powi(p20, ndl, 20); expneg_sq(gw, d, 16.0, 3.0)
e("v_mul_f32 v%d, v%d, v%d" % (p40, p40, b3), "v_mul_f32 v%d, %s, v%d" % (p40, hx(2.2), p40),
  "v_mul_f32 v%d, v%d, v%d" % (p20, p20, gw), "v_mac_f32 v%d, %s, v%d" % (p40, hx(1.2), p20), "v_mul_f32 v%d, %s, v%d" % (p40, STR, p40))
for c in range(3):
    e("v_mac_f32 v%d, %s, v%d" % (tab[c], COL[c], ex_), "v_mul_f32 v%d, 0.5, %s" % (t, COL[c]) if False else "v_mov_b32 v%d, %s" % (t, COL[c]),
      "v_mul_f32 v%d, 0.5, v%d" % (t, t), "v_add_f32 v%d, 0.5, v%d" % (t, t), "v_mac_f32 v%d, v%d, v%d" % (tab[c], p40, t))
R.put(b3, wd, ndT, ndl, ex_, p40, p20, gw, Ic)
ng = R.get(3)
for c in range(3): e("v_mov_b32 v%d, v%d" % (ng[c], tab[c]))
# grain: an integer hash of the pixel, uniform with unit variance, x 0.035 x coverage
hs, h2 = R.get(), R.get()
e("v_cvt_u32_f32 v%d, v2" % hs, "v_cvt_u32_f32 v%d, v3" % h2, "v_mul_lo_u32 v%d, v%d, s62" % (hs, hs),
  "v_mul_lo_u32 v%d, v%d, s63" % (h2, h2), "v_xor_b32 v%d, v%d, v%d" % (hs, hs, h2),
  "v_lshrrev_b32 v%d, 13, v%d" % (h2, hs), "v_xor_b32 v%d, v%d, v%d" % (hs, hs, h2), "v_mul_lo_u32 v%d, v%d, s64" % (hs, hs),
  "v_lshrrev_b32 v%d, 16, v%d" % (h2, hs), "v_xor_b32 v%d, v%d, v%d" % (hs, hs, h2), "v_cvt_f32_u32 v%d, v%d" % (hs, hs),
  "v_mul_f32 v%d, %s, v%d" % (hs, hx(2.0 / 4294967295.0), hs), "v_add_f32 v%d, -1.0, v%d" % (hs, hs),
  "v_mul_f32 v%d, %s, v%d" % (hs, hx(1.7320508 * 0.035), hs), "v_mul_f32 v%d, v%d, v%d" % (hs, hs, m), "v_add_f32 v%d, 1.0, v%d" % (hs, hs))
for c in range(3): e("v_mul_f32 v%d, v%d, v%d" % (tab[c], tab[c], hs))
R.put(hs, h2)
# the slab's shadow on the scene outside and its faint caustic
ds = R.get(); rrect(ds, None, None, 8.0, 16.0)
sh, ca = R.get(), R.get()
expneg_sq(sh, ds, 24.0, 14.0); e("v_cmp_lt_f32 vcc, %s, v%d" % (hx(-60.0), ds), "v_cndmask_b32 v%d, 0, v%d, vcc" % (sh, sh))
expneg_sq(ca, ds, 9.0, 36.0)
om = R.get(); e("v_sub_f32 v%d, 1.0, v%d" % (om, m), "v_mul_f32 v%d, v%d, v%d" % (sh, sh, om), "v_mul_f32 v%d, %s, v%d" % (sh, hx(-0.55 * 0.55), sh),
  "v_add_f32 v%d, 1.0, v%d" % (sh, sh), "v_mul_f32 v%d, v%d, v%d" % (ca, ca, om), "v_mul_f32 v%d, %s, v%d" % (ca, hx(0.10 * 0.6), ca))
for c, cc in enumerate((1.0, 0.93, 0.8)):
    e("v_mul_f32 v%d, v%d, v%d" % (res[c], fr[c], sh), "v_mac_f32 v%d, %s, v%d" % (res[c], hx(cc), ca),
      "v_sub_f32 v%d, v%d, v%d" % (t, tab[c], res[c]), "v_mac_f32 v%d, v%d, v%d" % (res[c], t, m))
R.put(ds, sh, ca, om); R.put(*fr); R.put(*tab); R.put(*fq)
# ---- text: the pieces' distance field (slab coords), groups by point loads
def tex_at(dd, gg, dxs, dys, want_g=True):
    """dd = distance at (X - ox0 + dx, Y - oy0 + dy) (bilinear R), gg = group there (point load G)"""
    c2 = R.get(2)
    e("v_subrev_f32 v%d, %s, v2" % (c2[0], OX0), "v_subrev_f32 v%d, %s, v3" % (c2[1], OY0))
    for k, (reg, dv) in enumerate(((c2[0], dxs), (c2[1], dys))):
        if isinstance(dv, str): e("v_add_f32 v%d, v%d, v%d" % (reg, reg, int(dv[1:])))
        elif dv: e("v_add_f32 v%d, %s, v%d" % (reg, hx(dv), reg))
    if want_g:
        ic = R.get(4)
        e("v_add_f32 v%d, -0.5, v%d" % (ic[0], c2[0]), "v_add_f32 v%d, -0.5, v%d" % (ic[1], c2[1]),
          "v_rndne_f32 v%d, v%d" % (ic[0], ic[0]), "v_rndne_f32 v%d, v%d" % (ic[1], ic[1]),
          "v_cvt_i32_f32 v%d, v%d" % (ic[0], ic[0]), "v_cvt_i32_f32 v%d, v%d" % (ic[1], ic[1]), "v_mov_b32 v%d, 0" % ic[2])
        g2 = R.get(2)
        e("image_load v[%d:%d], v[%d:%d], s[32:39] dmask:0x3 unorm" % (g2[0], g2[1], ic[0], ic[2]))
    e("v_mul_f32 v%d, %s, v%d" % (c2[0], ITW, c2[0]), "v_mul_f32 v%d, %s, v%d" % (c2[1], ITH, c2[1]))
    e("image_sample_lz v%d, v[%d:%d], s[32:39], s[12:15] dmask:0x1" % (dd, c2[0], c2[1]), "s_waitcnt vmcnt(0)",
      "v_mul_f32 v%d, %s, v%d" % (dd, hx(255.0 / 8.0), dd), "v_add_f32 v%d, %s, v%d" % (dd, hx(-16.0), dd))
    if want_g:
        e("v_mul_f32 v%d, %s, v%d" % (gg, hx(255.0), g2[1]), "v_rndne_f32 v%d, v%d" % (gg, gg))
        R.put(*ic); R.put(*g2)
    R.put(*c2)
dt, gt = R.get(), R.get(); tex_at(dt, gt, 0.0, 0.0); R.put(gt)
e("; waves with no lane within 15 px of the text: nothing of it reaches them (shadows need d < 5.6",
  "; within 7 px, caustics d < 0 within 9.1 px, the contact line d < 2.4, the letters d < 0.5)",
  "v_cmp_gt_f32 vcc, %s, v%d" % (hx(15.0), dt), "s_or_b32 s65, vcc_lo, vcc_hi", "s_cbranch_scc0 clock_text_done")
shade, caus = R.get(), R.get(); e("v_mov_b32 v%d, 0" % shade, "v_mov_b32 v%d, 0" % caus)
offx, offy = R.get(), R.get()
for g, th in ((1, 1.0), (2, 0.55), (3, 0.33)):
    for kind in ("shade", "caus"):
        off = 7.0 * th * (1.3 if kind == "caus" else 1.0)
        e("v_mov_b32 v%d, s%s" % (offx, L2X[1:]), "v_mul_f32 v%d, %s, v%d" % (offx, hx(-off), offx),
          "v_mov_b32 v%d, s%s" % (offy, L2Y[1:]), "v_mul_f32 v%d, %s, v%d" % (offy, hx(-off), offy))
        dd, gg = R.get(), R.get(); tex_at(dd, gg, "v%d" % offx, "v%d" % offy)
        v = R.get()
        if kind == "shade":
            sg = 1.5 + 3.0 * th
            e("v_mul_f32 v%d, %s, v%d" % (v, hx(-1.0 / (2.5 * sg)), dd), "v_add_f32 v%d, 0.5, v%d" % (v, v),
              "v_max_f32 v%d, 0, v%d" % (v, v), "v_min_f32 v%d, 1.0, v%d" % (v, v),
              "v_cmp_eq_f32 vcc, %s, v%d" % (hx(float(g)), gg), "v_cndmask_b32 v%d, 0, v%d, vcc" % (v, v), "v_max_f32 v%d, v%d, v%d" % (shade, shade, v))
        else:
            wr = 3.0 * th + 1.0 + 2.5 * th
            expneg_sq(v, dd, wr, 3.0 * th)
            e("v_cmp_gt_f32 vcc, 0, v%d" % dd, "v_cndmask_b32 v%d, 0, v%d, vcc" % (v, v),
              "v_cmp_eq_f32 vcc, %s, v%d" % (hx(float(g)), gg), "v_cndmask_b32 v%d, 0, v%d, vcc" % (v, v), "v_add_f32 v%d, v%d, v%d" % (caus, caus, v))
        R.put(dd, gg, v)
R.put(offx, offy)
e("v_mul_f32 v%d, %s, v%d" % (shade, hx(-0.40), shade), "v_mul_f32 v%d, %s, v%d" % (caus, hx(0.30), caus), "v_mul_f32 v%d, %s, v%d" % (caus, STR, caus))
# the letters' normal: the text distance's gradient by central differences over +-1.5 px (smooth
# around the glyphs' corners - rounded glass)
a1, a2, b1, b2, junk = R.get(), R.get(), R.get(), R.get(), R.get()
tex_at(a1, junk, 1.5, 0.0, want_g=False); tex_at(a2, junk, -1.5, 0.0, want_g=False)
tex_at(b1, junk, 0.0, 1.5, want_g=False); tex_at(b2, junk, 0.0, -1.5, want_g=False); R.put(junk)
grx, gry = a1, b1
e("v_sub_f32 v%d, v%d, v%d" % (grx, a1, a2), "v_mul_f32 v%d, %s, v%d" % (grx, hx(1.0 / 3.0), grx),
  "v_sub_f32 v%d, v%d, v%d" % (gry, b1, b2), "v_mul_f32 v%d, %s, v%d" % (gry, hx(1.0 / 3.0), gry))
R.put(a2, b2)
lgx, lgy = R.get(), R.get()
e("v_mul_f32 v%d, v%d, v%d" % (t, grx, grx), "v_mac_f32 v%d, v%d, v%d" % (t, gry, gry), "v_sqrt_f32 v%d, v%d" % (t, t),
  "v_add_f32 v%d, %s, v%d" % (t, hx(1e-6), t), "v_rcp_f32 v%d, v%d" % (t, t), "v_mul_f32 v%d, v%d, v%d" % (lgx, grx, t), "v_mul_f32 v%d, v%d, v%d" % (lgy, gry, t))
# per pixel: the light escaping the table into the letters, and the rims' direction terms
ie, lgl, lge = R.get(), R.get(), R.get()
e("v_max_f32 v%d, %s, v%d" % (ie, hx(0.35), I), "v_min_f32 v%d, %s, v%d" % (ie, hx(1.6), ie), "v_mul_f32 v%d, %s, v%d" % (ie, hx(0.55), ie), "v_mul_f32 v%d, %s, v%d" % (ie, STR, ie),
  "v_mul_f32 v%d, s%s, v%d" % (lge, L2X[1:], lgx), "v_mac_f32 v%d, s%s, v%d" % (lge, L2Y[1:], lgy),
  "v_sub_f32 v%d, 0, v%d" % (lgl, lge), "v_max_f32 v%d, 0, v%d" % (lgl, lgl), "v_max_f32 v%d, 0, v%d" % (lge, lge))
p6 = R.get(); powi(p6, lgl, 6)
# 4 rotated-grid sub-samples: d_k = d + g . o_k; each composited (shadow, caustic, contact line,
# the letter) and clamped to 0..1, then averaged (the display's view of the pixel)
acc = R.get(3)
for c in range(3): e("v_mov_b32 v%d, 0" % acc[c])
for (ox, oy) in ((-0.125, -0.375), (0.375, -0.125), (0.125, 0.375), (-0.375, 0.125)):
    dk, mi, omi = R.get(), R.get(), R.get()
    e("v_mul_f32 v%d, %s, v%d" % (dk, hx(ox), grx), "v_mac_f32 v%d, %s, v%d" % (dk, hx(oy), gry), "v_add_f32 v%d, v%d, v%d" % (dk, dk, dt),
      "v_sub_f32 v%d, 0.5, v%d" % (mi, dk), "v_max_f32 v%d, 0, v%d" % (mi, mi), "v_min_f32 v%d, 1.0, v%d" % (mi, mi), "v_sub_f32 v%d, 1.0, v%d" % (omi, mi))
    sm, cm, cn = R.get(), R.get(), R.get()
    e("v_mul_f32 v%d, v%d, v%d" % (sm, shade, omi), "v_add_f32 v%d, 1.0, v%d" % (sm, sm), "v_mul_f32 v%d, v%d, v%d" % (cm, caus, omi))
    band(cn, dk, -1.2, 1.2)
    e("v_mul_f32 v%d, v%d, v%d" % (cn, cn, omi), "v_mul_f32 v%d, %s, v%d" % (cn, hx(-0.40), cn), "v_add_f32 v%d, 1.0, v%d" % (cn, cn))
    r = R.get(3)
    for c in range(3):
        e("v_mul_f32 v%d, v%d, v%d" % (r[c], res[c], sm), "v_mac_f32 v%d, %s, v%d" % (r[c], COL[c], cm), "v_mul_f32 v%d, v%d, v%d" % (r[c], r[c], cn))
    R.put(sm, cm, cn)
    ls_ = R.get(); e("v_sub_f32 v%d, 0, v%d" % (ls_, dk), "v_max_f32 v%d, 0, v%d" % (ls_, ls_))
    lnx, lny, lnz, lh = R.get(), R.get(), R.get(), R.get(); bevel(lnx, lny, lnz, lh, ls_, 9.0, lgx, lgy); R.put(ls_)
    let = R.get(3)
    for c in range(3):
        rx, ry, lp, tm = R.get(), R.get(), R.get(), R.get()
        refract_path(lnx, lny, lnz, lh, c, rx, ry, lp, tm)   # entering glass from air: no total reflection
        e("v_mul_f32 v%d, %s, v%d" % (lp, hx(-ABS[c] * L2E), lp), "v_exp_f32 v%d, v%d" % (lp, lp), "v_mul_f32 v%d, v%d, v%d" % (let[c], ng[c], lp))
        R.put(rx, ry, lp, tm)
    F, kr = R.get(), R.get()
    e("v_sub_f32 v%d, 1.0, v%d" % (F, lnz), "v_mul_f32 v%d, v%d, v%d" % (t, F, F), "v_mul_f32 v%d, v%d, v%d" % (t, t, t),
      "v_mul_f32 v%d, v%d, v%d" % (F, F, t), "v_mul_f32 v%d, %s, v%d" % (F, hx(0.96), F), "v_add_f32 v%d, %s, v%d" % (F, hx(0.04), F),
      "v_mul_f32 v%d, %s, v%d" % (kr, hx(0.45), lnx), "v_mac_f32 v%d, %s, v%d" % (kr, hx(0.89), lny), "v_max_f32 v%d, 0, v%d" % (kr, kr))
    powi(t, kr, 6); e("v_mul_f32 v%d, %s, v%d" % (t, hx(1.6), t), "v_add_f32 v%d, %s, v%d" % (t, hx(0.10), t), "v_mul_f32 v%d, v%d, v%d" % (t, t, F), "v_sub_f32 v%d, 1.0, v%d" % (F, F))
    for c in range(3): e("v_mul_f32 v%d, v%d, v%d" % (let[c], let[c], F), "v_add_f32 v%d, v%d, v%d" % (let[c], let[c], t))
    R.put(F, kr, lnx, lny, lnz, lh)
    lb, gw, rm = R.get(), R.get(), R.get()
    band(lb, dk, 1.5, 1.5); expneg_sq(gw, dk, 3.0, 1.5)
    e("v_mul_f32 v%d, v%d, v%d" % (rm, lb, p6), "v_mul_f32 v%d, %s, v%d" % (rm, hx(1.1), rm),
      "v_mul_f32 v%d, v%d, v%d" % (lb, lb, lge), "v_mac_f32 v%d, %s, v%d" % (rm, hx(0.55), lb),
      "v_mul_f32 v%d, v%d, v%d" % (gw, gw, p6), "v_mac_f32 v%d, 0.5, v%d" % (rm, gw), "v_mul_f32 v%d, %s, v%d" % (rm, STR, rm),
      "v_add_f32 v%d, v%d, v%d" % (rm, rm, ie), "v_mul_f32 v%d, v%d, v%d" % (mi, mi, m))
    for c in range(3):
        e("v_mac_f32 v%d, %s, v%d" % (let[c], COL[c], rm), "v_sub_f32 v%d, v%d, v%d" % (t, let[c], r[c]), "v_mac_f32 v%d, v%d, v%d" % (r[c], t, mi),
          "v_max_f32 v%d, 0, v%d" % (r[c], r[c]), "v_min_f32 v%d, 1.0, v%d" % (r[c], r[c]), "v_add_f32 v%d, v%d, v%d" % (acc[c], acc[c], r[c]))
    R.put(lb, gw, rm, dk, mi, omi); R.put(*let); R.put(*r)
for c in range(3): e("v_mul_f32 v%d, %s, v%d" % (res[c], hx(0.25), acc[c]))
e("clock_text_done:")
e("clock_done:")
# the clock source's dot: shadow, then the disc (coverage clamp(0.5 - (|p - c| - r)), as ui_disc)
ddx, ddy, dd, cv = R.get(), R.get(), R.get(), R.get()
e("v_subrev_f32 v%d, s68, v2" % ddx, "v_subrev_f32 v%d, s69, v3" % ddy)
e("v_subrev_f32 v%d, %s, v%d" % (dd, hx(2.8), ddx), "v_mul_f32 v%d, v%d, v%d" % (dd, dd, dd),
  "v_subrev_f32 v%d, %s, v%d" % (cv, hx(2.8), ddy), "v_mac_f32 v%d, v%d, v%d" % (dd, cv, cv),
  "v_sqrt_f32 v%d, v%d" % (dd, dd), "v_sub_f32 v%d, s70, v%d" % (cv, dd), "v_add_f32 v%d, 0.5, v%d" % (cv, cv),
  "v_max_f32 v%d, 0, v%d" % (cv, cv), "v_min_f32 v%d, 1.0, v%d" % (cv, cv),
  "v_mul_f32 v%d, %s, v%d" % (cv, hx(-115.0 / 256.0), cv), "v_add_f32 v%d, 1.0, v%d" % (cv, cv))
for c in range(3): e("v_mul_f32 v%d, v%d, v%d" % (res[c], res[c], cv))
e("v_mul_f32 v%d, v%d, v%d" % (dd, ddx, ddx), "v_mac_f32 v%d, v%d, v%d" % (dd, ddy, ddy), "v_sqrt_f32 v%d, v%d" % (dd, dd),
  "v_sub_f32 v%d, s70, v%d" % (cv, dd), "v_add_f32 v%d, 0.5, v%d" % (cv, cv),
  "v_max_f32 v%d, 0, v%d" % (cv, cv), "v_min_f32 v%d, 1.0, v%d" % (cv, cv))
for c in range(3):
    e("v_sub_f32 v%d, s%d, v%d" % (dd, 71 + c, res[c]), "v_mac_f32 v%d, v%d, v%d" % (res[c], dd, cv))
R.put(ddx, ddy, dd, cv)
# ---- ps_ui's end: clamp, sRGB, dither
e("v_mov_b32 v8, v%d" % res[0], "v_mov_b32 v9, v%d" % res[1], "v_mov_b32 v10, v%d" % res[2],
  "v_mul_f32 v20, 0x3d897143, v2", "v_mac_f32 v20, 0x3bbf4590, v3", "v_fract_f32 v20, v20", "v_mul_f32 v20, 0x4253ee82, v20",
  "v_fract_f32 v20, v20", "v_subrev_f32 v20, 0.5, v20", "v_mul_f32 v20, 0x3b808081, v20")
for a, o in ((8, 16), (9, 17), (10, 18)):
    e("v_max_f32 v%d, 0, v%d" % (a, a), "v_min_f32 v%d, 1.0, v%d" % (a, a), "v_log_f32 v%d, v%d" % (o, a), "v_mul_f32 v%d, 0x3ed55555, v%d" % (o, o),
      "v_exp_f32 v%d, v%d" % (o, o), "v_mul_f32 v%d, 0x3f870a3d, v%d" % (o, o), "v_subrev_f32 v%d, 0x3d6147ae, v%d" % (o, o),
      "v_mul_f32 v21, 0x414eb852, v%d" % a, "v_cmp_ge_f32 vcc, 0x3b4d2e1c, v%d" % a, "v_cndmask_b32 v%d, v%d, v21, vcc" % (o, o))
e("v_add_f32 v16, v16, v20", "v_add_f32 v17, v17, v20", "v_add_f32 v18, v18, v20", "v_mov_b32 v19, 1.0",
  "exp mrt0 v16, v17, v18, v19 done vm", "s_endpgm")
p = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "shaders", "ps_clock.s")
open(p, "w").write("\n".join(out) + "\n"); print("ps_clock.s: %d lines, VGPRs up to v%d" % (len(out), R.maxv))
