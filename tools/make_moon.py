#!/usr/bin/env python3
"""Moon albedo for ps_dark: NASA's CGI Moon Kit LROC colour map (equirectangular, 0 deg longitude at
the centre; https://svs.gsfc.nasa.gov/4720, NASA's Scientific Visualization Studio) warped into an
orthographic view of the near side (the Moon is tidally locked: we always see this face), north
up, disc radius = half the image. The shader samples it at the disc coordinates (u, v) = 0.5 +
0.5 (x, y) - no trigonometry per pixel - and lights it from the sphere normal. Outside the disc the
limb colour is extended outward so bilinear filtering and mips never pull black into the rim.
Warped and averaged in linear light (4x4 supersampling per output pixel).
  make_moon.py lroc_color_2k.jpg OUT.png [SIZE]   then   make_textures.py albedo OUT.png OUT.dds"""
import sys
import numpy as np
from PIL import Image

def main():
    src, out = sys.argv[1], sys.argv[2]
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 512
    t = np.asarray(Image.open(src).convert("RGB")).astype(np.float64) / 255.0
    t = np.where(t <= 0.04045, t / 12.92, ((t + 0.055) / 1.055) ** 2.4)
    H, W, _ = t.shape
    ss = 4
    acc = np.zeros((n, n, 3))
    for sy in range(ss):
        for sx in range(ss):
            y, x = np.mgrid[0:n, 0:n].astype(np.float64)
            px = (x + (sx + 0.5) / ss) / n * 2 - 1
            py = 1 - (y + (sy + 0.5) / ss) / n * 2
            r = np.sqrt(px * px + py * py)
            k = np.where(r > 0.999, 0.999 / np.maximum(r, 1e-9), 1.0)   # outside: the limb point radially
            px, py = px * k, py * k
            pz = np.sqrt(np.clip(1 - px * px - py * py, 0, 1))
            lon = np.arctan2(px, pz); lat = np.arcsin(np.clip(py, -1, 1))
            u = (0.5 + lon / (2 * np.pi)) * W - 0.5; v = (0.5 - lat / np.pi) * H - 0.5
            u0 = np.floor(u).astype(int); v0 = np.floor(v).astype(int); fu = (u - u0)[..., None]; fv = (v - v0)[..., None]
            u1 = (u0 + 1) % W; u0 %= W; v1 = np.clip(v0 + 1, 0, H - 1); v0 = np.clip(v0, 0, H - 1)
            acc += (t[v0, u0] * (1 - fu) + t[v0, u1] * fu) * (1 - fv) + (t[v1, u0] * (1 - fu) + t[v1, u1] * fu) * fv
    lin = acc / (ss * ss)
    s = np.where(lin <= 0.0031308, 12.92 * lin, 1.055 * lin ** (1 / 2.4) - 0.055)
    Image.fromarray(np.clip(np.round(s * 255), 0, 255).astype(np.uint8)).save(out)

if __name__ == "__main__":
    main()
