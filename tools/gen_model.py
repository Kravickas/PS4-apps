#!/usr/bin/env python3
"""Generates the ShadCube4 prop into OUT_DIR (default .): model.obj (rounded
cube, per-vertex normals, logo UVs on every face), model.bmp (the logo from
tools/logo_2048.png: the ShadPS4 badge + "ShadPS4" in URW Gothic Demi, the
logo's font), model_height.bmp (engraved logo: distance-field bevel on
the logo ink) and model_normal.bmp (from the height at relief depth D_UV, which must
equal MODEL_POM_DEPTH in src/main.c).  Needs numpy, scipy and Pillow.
    python3 gen_model.py [OUT_DIR]"""
import numpy as np, math, struct, os, re, sys
from PIL import Image
H, R, S = 0.9, 0.09, 4          # half size, edge radius, bevel segments per side
D_UV = 0.003125                  # relief depth in UV units (must equal MODEL_POM_DEPTH in main.c)
MAP = 2048                      # logo, height and normal map size
BEVEL_PX = 10                   # engraving wall width at MAP (distance-field bevel)

def grid_axis():
    inner = H - R
    neg = [-(inner + R * math.tan(math.radians(45.0 * (S - k) / S))) for k in range(S + 1)]
    pos = [inner + R * math.tan(math.radians(45.0 * k / S)) for k in range(S + 1)]
    return neg + pos            # -H .. -(H-R), (H-R) .. H

# faces: outward normal n, in-plane axes a_u (u), a_v (v) with a_u x a_v = n
FACES = [((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
         ((1, 0, 0), (0, 0, -1), (0, 1, 0)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
         ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1))]

def build():
    ax = grid_axis(); inner = H - R
    V, VT, VN, F = [], [], [], []
    for n, au, av in FACES:
        n, au, av = map(np.array, (n, au, av))
        assert np.allclose(np.cross(au, av), n)
        base = len(V); m = len(ax)
        for t in ax:
            for s in ax:
                pc = n * H + au * s + av * t
                q = np.clip(pc, -inner, inner)
                d = pc - q; nrm = d / np.linalg.norm(d)
                V.append(q + R * nrm); VN.append(nrm)
                VT.append((min(max((s + inner) / (2 * inner), 0.0), 1.0), min(max((t + inner) / (2 * inner), 0.0), 1.0)))
        for j in range(m - 1):
            for i in range(m - 1):
                a = base + j * m + i; b = a + 1; c = a + m + 1; e = a + m
                F.append((a, b, c)); F.append((a, c, e))
    return np.array(V), np.array(VT), np.array(VN), F

def write_obj(path, V, VT, VN, F):
    with open(path, 'w') as f:
        f.write("# ShadCube4 prop: rounded cube, half size %.2f, edge radius %.2f\n" % (H, R))
        f.write("# per-vertex normals (vn) and logo UVs (vt) on every face\n")
        for p in V: f.write("v %.6f %.6f %.6f\n" % tuple(p))
        for t in VT: f.write("vt %.6f %.6f\n" % tuple(t))
        for nn in VN: f.write("vn %.6f %.6f %.6f\n" % tuple(nn))
        for tri in F: f.write("f " + " ".join("%d/%d/%d" % (k + 1, k + 1, k + 1) for k in tri) + "\n")

def write_bmp24(path, rgb_top_down):
    h, w, _ = rgb_top_down.shape
    row = (w * 3 + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):              # BMP: bottom row first
        r = rgb_top_down[y][:, ::-1].tobytes()  # RGB -> BGR
        data += r + b'\0' * (row - w * 3)
    hdr = struct.pack('<2sIHHI', b'BM', 54 + len(data), 0, 0, 54)
    info = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    open(path, 'wb').write(hdr + info + data)

def maps():
    """Logo (tools/logo_2048.png), engraving height and normal map at MAP.
    Height: the logo ink (dark parts: outline, badge), binarised at its half-ink contour
    at 2*MAP, carved with a distance-field bevel (smoothstep over BEVEL_PX) and box-filtered
    to MAP; background 1 (flush with the face), ink carved in. Normals from the height at
    depth D_UV."""
    from scipy import ndimage
    img = Image.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "logo_2048.png")).convert("RGB")
    rgb = np.asarray(img.resize((MAP, MAP), Image.LANCZOS) if img.size != (MAP, MAP) else img)
    y = np.asarray(img.convert("L").resize((2 * MAP, 2 * MAP), Image.BICUBIC), np.float64) / 255.0
    x = np.clip((1.0 - y - 0.08) / (0.45 - 0.08), 0, 1)
    ink = x * x * (3 - 2 * x)                                   # dark parts of the logo
    inside = ink > 0.5
    d = ndimage.distance_transform_edt(inside)                  # px (at 2*MAP) to the ink edge
    x = np.clip(d / (2.0 * BEVEL_PX), 0, 1)
    h2 = 1.0 - x * x * (3 - 2 * x)
    h = h2.reshape(MAP, 2, MAP, 2).mean((1, 3))
    dhdu = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * (MAP / 2.0)
    dhdv = -(np.roll(h, -1, 0) - np.roll(h, 1, 0)) * (MAP / 2.0)   # rows top-down, v up
    nx, ny, nz = -D_UV * dhdu, -D_UV * dhdv, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz)
    nrm = np.stack([np.round((c / l * 0.5 + 0.5) * 255).clip(0, 255) for c in (nx, ny, nz)], -1).astype(np.uint8)
    return rgb, np.round(h * 255).astype(np.uint8), nrm


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    V, VT, VN, F = build()
    write_obj(os.path.join(out, "model.obj"), V, VT, VN, F)
    rgb, hq, nrm = maps()
    write_bmp24(os.path.join(out, "model.bmp"), rgb)
    write_bmp24(os.path.join(out, "model_height.bmp"), np.stack([hq] * 3, -1))
    write_bmp24(os.path.join(out, "model_normal.bmp"), nrm)
    print("model.obj: %d vertices, %d triangles; logo / height / normal %dx%d" % (len(V), len(F), MAP, MAP))
