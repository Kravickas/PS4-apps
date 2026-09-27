#!/usr/bin/env python3
"""sce_sys/icon0.png: the ShadPS4 badge on a tilted, slightly turned cube,
transparent background, 512x512 RGBA.

  python3 tools/make_icon.py BADGE.png OUT.png

BADGE: the ShadPS4 badge (shadPS4 src/resources/shadps4.png, 512x512 RGBA). Each
visible face (front, right, top) is the badge on a white face, lit top > front
> right; mild perspective; rendered at 4x and downsampled (Lanczos)."""
import sys
import numpy as np
from PIL import Image

S = 2048          # render size (downsampled to 512)
YAW, PITCH = np.radians(-28.0), np.radians(22.0)   # turned right face into view, tilted to show the top
CAM = 7.0         # camera distance (cube half size 1)
SHADE = {"front": 0.90, "right": 0.72, "top": 1.0}


def rot(p):
    cy, sy, cp, sp = np.cos(YAW), np.sin(YAW), np.cos(PITCH), np.sin(PITCH)
    x, y, z = p
    x, z = x * cy + z * sy, -x * sy + z * cy          # yaw about Y
    y, z = y * cp - z * sp, y * sp + z * cp           # pitch about X (look from above)
    return np.array([x, y, z])


def project(p):
    q = rot(p)
    f = CAM / (CAM - q[2])
    return q[0] * f, q[1] * f


def coeffs(dst, src):
    """PIL PERSPECTIVE coefficients mapping output (dst) points to input (src) points."""
    a = []
    for (x, y), (u, v) in zip(dst, src):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
    b = np.array([c for s in src for c in s], np.float64)
    return np.linalg.solve(np.array(a, np.float64), b)


def main():
    badge = Image.open(sys.argv[1]).convert("RGBA")
    face = Image.new("RGBA", badge.size, (255, 255, 255, 255))
    face.alpha_composite(badge)
    W = face.size[0]
    # face corners (object space) in texture order: top-left, top-right, bottom-right, bottom-left
    faces = {
        "front": [(-1, 1, 1), (1, 1, 1), (1, -1, 1), (-1, -1, 1)],
        "right": [(1, 1, 1), (1, 1, -1), (1, -1, -1), (1, -1, 1)],
        "top":   [(-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1)],
    }
    pts = {k: [project(np.array(c, float)) for c in v] for k, v in faces.items()}
    allp = np.array([p for v in pts.values() for p in v])
    lo, hi = allp.min(0), allp.max(0)
    scale = 0.90 * S / (hi - lo).max()
    centre = (lo + hi) / 2
    to_px = lambda p: ((p[0] - centre[0]) * scale + S / 2, S / 2 - (p[1] - centre[1]) * scale)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    src = [(0, 0), (W, 0), (W, W), (0, W)]
    for name in ("right", "top", "front"):
        dst = [to_px(p) for p in pts[name]]
        k = SHADE[name]
        shaded = Image.fromarray((np.asarray(face).astype(np.float64) * [k, k, k, 1]).astype(np.uint8), "RGBA")
        warped = shaded.transform((S, S), Image.PERSPECTIVE, tuple(coeffs(dst, src)), Image.BICUBIC)
        out.alpha_composite(warped)
    out.resize((512, 512), Image.LANCZOS).save(sys.argv[2], optimize=True)


if __name__ == "__main__":
    main()
