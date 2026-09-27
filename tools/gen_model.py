#!/usr/bin/env python3
"""Generates the ShadCube4 prop into OUT_DIR (default .): model.obj (rounded
cube, per-vertex normals, logo UVs on every face), model.bmp (the logo from
src/logo_texture.h), model_height.bmp (embossed logo: ink coverage, smoothed)
and model_normal.bmp (from the height at relief depth D_UV, which must equal
MODEL_POM_DEPTH in src/main.c).  Needs numpy and Pillow.
    python3 gen_model.py [OUT_DIR]"""
import numpy as np, math, struct, os, re, sys
from PIL import Image
H, R, S = 0.9, 0.09, 4          # half size, edge radius, bevel segments per side
D_UV = 0.025                    # relief depth in UV units (must equal MODEL_POM_DEPTH in main.c)
MAP = 512                       # height / normal map size

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

def maps(logo):
    rgb = logo[:, :, :3].astype(np.float64) / 255.0
    big = np.asarray(Image.fromarray(logo[:, :, :3]).resize((MAP, MAP), Image.BICUBIC), np.float64) / 255.0
    lum = 0.2126 * big[..., 0] + 0.7152 * big[..., 1] + 0.0722 * big[..., 2]
    x = np.clip((1.0 - lum - 0.08) / (0.45 - 0.08), 0, 1); m = x * x * (3 - 2 * x)      # ink coverage
    k = np.arange(-6, 7); g = np.exp(-k * k / (2 * 1.5 ** 2)); g /= g.sum()           # Gaussian sigma 1.5 px
    for axis in (0, 1):
        m = sum(g[i] * np.roll(m, k[i], axis=axis) for i in range(len(k)))
    h = m                                                     # 0 background, 1 logo top
    # rows top-down in the image; texture v = up, so d/dv = -(d/drow)
    dhdu = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * (MAP / 2.0)
    dhdv = -(np.roll(h, -1, 0) - np.roll(h, 1, 0)) * (MAP / 2.0)
    nx, ny, nz = -D_UV * dhdu, -D_UV * dhdv, np.ones_like(h)
    l = np.sqrt(nx * nx + ny * ny + nz * nz); nx, ny, nz = nx / l, ny / l, nz / l
    hq = np.round(h * 255).astype(np.uint8)
    nrm = np.stack([np.round((c * 0.5 + 0.5) * 255).clip(0, 255) for c in (nx, ny, nz)], -1).astype(np.uint8)
    return hq, nrm, h

def load_logo(path):
    txt = open(path).read()
    w = int(re.search(r"#define LOGO_WIDTH (\d+)", txt).group(1))
    h = int(re.search(r"#define LOGO_HEIGHT (\d+)", txt).group(1))
    body = txt[txt.index("{") + 1:txt.rindex("}")]
    v = np.array([int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", body)], np.uint8)
    return v.reshape(h, w, 4)            # rows top-down, RGBA

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    here = os.path.dirname(os.path.abspath(__file__))
    logo = load_logo(os.path.join(here, "..", "src", "logo_texture.h"))
    V, VT, VN, F = build()
    write_obj(os.path.join(out, "model.obj"), V, VT, VN, F)
    write_bmp24(os.path.join(out, "model.bmp"), logo[:, :, :3])
    hq, nrm, h = maps(logo)
    write_bmp24(os.path.join(out, "model_height.bmp"), np.stack([hq] * 3, -1))
    write_bmp24(os.path.join(out, "model_normal.bmp"), nrm)
    print("model.obj: %d vertices, %d triangles; maps %dx%d" % (len(V), len(F), MAP, MAP))
