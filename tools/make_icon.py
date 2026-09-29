#!/usr/bin/env python3
"""sce_sys/icon0.png: the in-game prop, transparent background, 512x512 RGBA.

  python3 tools/make_icon.py [OUT.png]      (default sce_sys/icon0.png)

Geometry: assets/models/cube/cube.obj (the rounded cube the game draws), scaled
to half size 1. Faces: the blue ShadPS4 badge only (tools/badge_1024.png, rebuilt
from the original logo artwork) on white, filling each face's flat part like the
previous icon - quick to recognise at icon size; no engraving. Satin sun
highlight as ps_model (GGX D, Schlick-Smith visibility, Schlick Fresnel;
MODEL_ROUGHNESS 0.45, MODEL_F0 0.04 as in main.c). View as the previous icon: yaw -28, pitch 22
degrees, camera 7 half sizes away, fitted to 90% of the canvas. Lighting in
linear light, chosen so flat white faces keep the previous icon's levels (sRGB
x 1.0 / 0.90 / 0.72 for top / front / right = linear 1.0 / 0.7874 / 0.4770):
ambient 0.20 + k max(0, N.L). Rasterised at 2048 (perspective-correct,
z-buffered, back faces culled) and box-filtered to 512. Needs numpy, Pillow."""
import os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
S = 2048
YAW, PITCH, CAM = np.radians(-28.0), np.radians(22.0), 7.0
ROUGHNESS, F0 = 0.45, 0.04
AMBIENT = 0.20


def srgb_to_linear(c):
    c = np.asarray(c, np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def linear_to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - 0.055)


def light():
    """ambient + k (N.L) = linear levels of the previous icon's white faces."""
    top, front, right = 1.0, float(srgb_to_linear(0.90)), float(srgb_to_linear(0.72))
    v = np.array([right, top, front]) - AMBIENT          # k L = (x, y, z)
    k = np.linalg.norm(v)
    return v / k, k


def rot(p):
    cy, sy, cp, sp = np.cos(YAW), np.sin(YAW), np.cos(PITCH), np.sin(PITCH)
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    x, z = x * cy + z * sy, -x * sy + z * cy
    y, z = y * cp - z * sp, y * sp + z * cp
    return np.stack([x, y, z], -1)


def load_obj(path):
    V, VT, VN, F = [], [], [], []
    for l in open(path):
        t = l.split()
        if not t:
            continue
        if t[0] == "v": V.append([float(x) for x in t[1:4]])
        elif t[0] == "vt": VT.append([float(x) for x in t[1:3]])
        elif t[0] == "vn": VN.append([float(x) for x in t[1:4]])
        elif t[0] == "f": F.append([[int(i) - 1 for i in c.split("/")] for c in t[1:4]])
    return np.array(V), np.array(VT), np.array(VN), np.array(F)


def bilinear(img, u, v):
    """img rows top-down (row 0 = v 1)."""
    h, w = img.shape[:2]
    x = np.clip(u * w - 0.5, 0, w - 1.001)
    y = np.clip((1.0 - v) * h - 0.5, 0, h - 1.001)
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = (x - x0)[:, None], (y - y0)[:, None]
    g = lambda a, b: img[a, b]
    return (g(y0, x0) * (1 - fx) + g(y0, x0 + 1) * fx) * (1 - fy) + (g(y0 + 1, x0) * (1 - fx) + g(y0 + 1, x0 + 1) * fx) * fy


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "sce_sys", "icon0.png")
    V, VT, VN, F = load_obj(os.path.join(ROOT, "assets", "models", "cube", "cube.obj"))
    half = np.abs(V).max()
    V = V / half                                        # half size 1, as the previous icon
    face = Image.new("RGBA", (1024, 1024), (255, 255, 255, 255))
    # 8 px white margin: the rounded edges clamp to the outermost texels, which must be white
    # (the badge's triangle tip reaches its right border and would streak along the edge)
    face.alpha_composite(Image.open(os.path.join(HERE, "badge_1024.png")).convert("RGBA").resize((1008, 1008), Image.LANCZOS), (8, 8))
    alb = srgb_to_linear(np.asarray(face.convert("RGB"), np.float64) / 255.0)

    P = rot(V)                                          # view space: camera on +z at CAM, looking at -z
    f = CAM / (CAM - P[:, 2])
    corners = rot(np.array([[x, y, z] for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)], float))
    fc = CAM / (CAM - corners[:, 2])
    cp = corners[:, :2] * fc[:, None]
    lo, hi = cp.min(0), cp.max(0)
    scale, centre = 0.90 * S / (hi - lo).max(), (lo + hi) / 2
    sx = (P[:, 0] * f - centre[0]) * scale + S / 2
    sy = S / 2 - (P[:, 1] * f - centre[1]) * scale
    iz = 1.0 / (CAM - P[:, 2])                          # 1 / view depth (perspective-correct weights)

    depth = np.full((S, S), np.inf)
    tri = np.full((S, S), -1, np.int64)
    bary = np.zeros((S, S, 3))
    for t, face in enumerate(F):
        a, b, c = face[:, 0]
        x0, y0, x1, y1, x2, y2 = sx[a], sy[a], sx[b], sy[b], sx[c], sy[c]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if area >= 0:                                   # back face (y down: front faces are negative)
            continue
        xmin, xmax = int(max(0, np.floor(min(x0, x1, x2)))), int(min(S - 1, np.ceil(max(x0, x1, x2))))
        ymin, ymax = int(max(0, np.floor(min(y0, y1, y2)))), int(min(S - 1, np.ceil(max(y0, y1, y2))))
        if xmin > xmax or ymin > ymax:
            continue
        X, Y = np.meshgrid(np.arange(xmin, xmax + 1) + 0.5, np.arange(ymin, ymax + 1) + 0.5)
        w0 = ((x1 - X) * (y2 - Y) - (x2 - X) * (y1 - Y)) / area
        w1 = ((x2 - X) * (y0 - Y) - (x0 - X) * (y2 - Y)) / area
        w2 = 1.0 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        z = -(w0 * iz[a] + w1 * iz[b] + w2 * iz[c])     # larger 1/depth = nearer
        sub = depth[ymin:ymax + 1, xmin:xmax + 1]
        upd = inside & (z < sub)
        sub[upd] = z[upd]
        tri[ymin:ymax + 1, xmin:xmax + 1][upd] = t
        bb = bary[ymin:ymax + 1, xmin:xmax + 1]
        bb[upd] = np.stack([w0[upd], w1[upd], w2[upd]], -1)

    ys, xs = np.nonzero(tri >= 0)
    T = tri[ys, xs]
    ids = F[T][:, :, 0]
    w = bary[ys, xs] * iz[ids]                          # perspective-correct
    w /= w.sum(1, keepdims=True)
    uv = (VT[F[T][:, :, 1]] * w[:, :, None]).sum(1)
    n = (VN[F[T][:, :, 2]] * w[:, :, None]).sum(1)
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    pos = (V[ids] * w[:, :, None]).sum(1)
    npert = n

    L, k = light()
    Vd = np.array([0.0, 0.0, CAM])[None, :] - rot(pos)          # to the camera, view space
    Vw = rot_inv(Vd)
    Vw /= np.linalg.norm(Vw, axis=1, keepdims=True)
    ndl = np.clip((npert * L).sum(1), 0, None)
    col = bilinear(alb, uv[:, 0], uv[:, 1]) * (AMBIENT + k * ndl)[:, None]
    a = ROUGHNESS * ROUGHNESS
    H = L[None, :] + Vw
    H /= np.linalg.norm(H, axis=1, keepdims=True)
    ndh = np.clip((npert * H).sum(1), 0, None)
    ndv = np.clip((npert * Vw).sum(1), 1e-4, None)
    vdh = np.clip((Vw * H).sum(1), 0, None)
    tt = ndh * ndh * (a * a - 1) + 1
    kk = a / 2
    spec = (a * a / (tt * tt)) * (0.25 / ((ndl * (1 - kk) + kk) * (ndv * (1 - kk) + kk))) * \
           (F0 + (1 - F0) * (1 - vdh) ** 5) * ndl * k
    col = col + spec[:, None]

    img = np.zeros((S, S, 4))
    img[ys, xs, :3] = linear_to_srgb(col)
    img[ys, xs, 3] = 1.0
    pm = img[..., :3] * img[..., 3:4]                    # premultiplied box filter to 512
    f4 = S // 512
    pm = pm.reshape(512, f4, 512, f4, 3).mean((1, 3))
    al = img[..., 3].reshape(512, f4, 512, f4).mean((1, 3))
    rgbo = np.where(al[..., None] > 0, pm / np.maximum(al[..., None], 1e-9), 0)
    out = np.concatenate([np.clip(np.round(rgbo * 255), 0, 255), np.round(al[..., None] * 255)], 2).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(out_path, optimize=True)
    print("%s: 512x512 RGBA, %d triangles, %.1f%% covered" % (out_path, len(F), 100 * al.mean()))


def rot_inv(p):
    cy, sy, cp, sp = np.cos(YAW), np.sin(YAW), np.cos(PITCH), np.sin(PITCH)
    x, y, z = p[..., 0], p[..., 1], p[..., 2]
    y, z = y * cp + z * sp, -y * sp + z * cp
    x, z = x * cy - z * sy, x * sy + z * cy
    return np.stack([x, y, z], -1)


if __name__ == "__main__":
    main()
