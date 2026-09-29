#!/usr/bin/env python3
"""Texture converter for ShadCube4: image -> DDS (DX10 header) that src/dds_loader.h
loads straight into GPU memory.

  make_textures.py albedo IN OUT.dds [--raw]    BC1 sRGB (--raw: RGBA8 sRGB, sharp art)
  make_textures.py normal IN OUT.dds --height H  BC5 (x, y; the shaders rebuild z)
  make_textures.py height IN OUT.dds             BC4, stretched to the full 0..255 range

Layout rules (match the PS4 LINEAR_ALIGNED mip layout, see dds_loader.h):
- power-of-two sizes >= 32; mips down to 32 px, so every level is >= 8 blocks
  wide and the tightly packed DDS chain is exactly the GPU layout;
- rows bottom-up (row 0 = image bottom = v 0), as the BMP loader did and as
  OBJ UVs expect.
Albedo mips average in linear light. Normal maps: the red / green convention is
measured against the height map (normals tilt away from rising height) and
flipped if needed, so the engine always reads +u / +v; mips average the vectors
and renormalise. Needs numpy, Pillow, quicktex."""
import struct, sys
import numpy as np
from PIL import Image
import quicktex
from quicktex.s3tc.bc1 import BC1Encoder, BC1Decoder
from quicktex.s3tc.bc4 import BC4Encoder, BC4Decoder
from quicktex.s3tc.bc5 import BC5Encoder, BC5Decoder

DXGI = {"rgba8_srgb": 29, "bc1_srgb": 72, "bc4": 80, "bc5": 83}
MIN_SIZE = 32


def load(path):
    """RGB float array in engine orientation: row 0 = image bottom."""
    a = np.asarray(Image.open(path).convert("RGB"), np.float64)
    h, w = a.shape[:2]
    if (w & (w - 1)) or (h & (h - 1)) or w < MIN_SIZE or h < MIN_SIZE:
        sys.exit("%s: %dx%d, need power-of-two sizes >= %d" % (path, w, h, MIN_SIZE))
    return a[::-1].copy()


def levels_of(w, h):
    n = 1
    while (w >> n) >= MIN_SIZE and (h >> n) >= MIN_SIZE:
        n += 1
    return n


def down2(a):
    return 0.25 * (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2])


def srgb_to_lin(c):
    c = c / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(l):
    s = np.where(l <= 0.0031308, l * 12.92, 1.055 * np.power(np.clip(l, 0, 1), 1 / 2.4) - 0.055)
    return np.clip(np.round(s * 255.0), 0, 255)


def rgba8(a):
    a = np.clip(np.round(a), 0, 255).astype(np.uint8)
    if a.ndim == 2:
        a = np.stack([a] * 3, -1)
    return np.ascontiguousarray(np.concatenate([a, np.full(a.shape[:2] + (1,), 255, np.uint8)], 2))


def encode(kind, img):
    h, w = img.shape[:2]
    raw = quicktex.RawTexture.frombytes(img.tobytes(), w, h)
    if kind == "bc1_srgb":
        enc, dec = BC1Encoder(10), BC1Decoder()
    elif kind == "bc4":
        enc, dec = BC4Encoder(0), BC4Decoder(0)
    else:
        enc, dec = BC5Encoder(0, 1), BC5Decoder(0, 1)
    t = enc.encode(raw)
    back = np.frombuffer(dec.decode(t).tobytes(), np.uint8).reshape(h, w, 4)
    return t.tobytes(), back


def write_dds(path, kind, w, h, payloads):
    compressed = kind != "rgba8_srgb"
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | (0x80000 if compressed else 0x8)
    pitch = len(payloads[0]) if compressed else w * 4
    caps = 0x1000 | ((0x400000 | 0x8) if len(payloads) > 1 else 0)
    hdr = struct.pack("<4sIIIIIII44x", b"DDS ", 124, flags, h, w, pitch, 0, len(payloads))
    hdr += struct.pack("<II4s20x", 32, 0x4, b"DX10")
    hdr += struct.pack("<IIII12x", caps, 0, 0, 0)[:20]
    hdr += struct.pack("<IIIII", DXGI[kind], 3, 0, 1, 0)
    assert len(hdr) == 4 + 124 + 20
    with open(path, "wb") as f:
        f.write(hdr)
        for p in payloads:
            f.write(p)


def build(kind, chain, path):
    """chain: list of uint8 RGBA arrays, level 0 first."""
    h, w = chain[0].shape[:2]
    payloads, report = [], ""
    for i, img in enumerate(chain):
        if kind == "rgba8_srgb":
            payloads.append(img.tobytes())
            continue
        data, back = encode(kind, img)
        payloads.append(data)
        if i == 0:
            report = back
    write_dds(path, kind, w, h, payloads)
    size = sum(len(p) for p in payloads)
    print("%s: %s %dx%d, %d levels, %d bytes" % (path, kind, w, h, len(chain), size))
    return report


def albedo(src, out, raw=False):
    a = load(src)
    lin = srgb_to_lin(a)
    chain = [rgba8(a)]
    for _ in range(1, levels_of(a.shape[1], a.shape[0])):
        lin = down2(lin)
        chain.append(rgba8(lin_to_srgb(lin)))
    back = build("rgba8_srgb" if raw else "bc1_srgb", chain, out)
    if not raw:
        err = chain[0][..., :3].astype(float) - back[..., :3]
        print("  BC1 level 0: PSNR %.1f dB, max error %d" % (10 * np.log10(255 ** 2 / (err ** 2).mean()), np.abs(err).max()))


def slope_signs(n, hgt):
    """Correlation of the normal's x / y with -dH/du / -dH/dv (engine orientation)."""
    hh = hgt.shape[0]
    if n.shape[:2] != hgt.shape[:2]:
        img = Image.fromarray(np.clip(hgt, 0, 255).astype(np.uint8)).resize((n.shape[1], n.shape[0]), Image.BILINEAR)
        hgt = np.asarray(img, np.float64)
    du = (np.roll(hgt, -1, 1) - np.roll(hgt, 1, 1)) * 0.5
    dv = (np.roll(hgt, -1, 0) - np.roll(hgt, 1, 0)) * 0.5
    cx = (n[..., 0] * -du).sum() / np.sqrt((n[..., 0] ** 2).sum() * (du ** 2).sum() + 1e-30)
    cy = (n[..., 1] * -dv).sum() / np.sqrt((n[..., 1] ** 2).sum() * (dv ** 2).sum() + 1e-30)
    return cx, cy


def normal(src, out, height):
    n = load(src) / 127.5 - 1.0
    n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-12)
    cx, cy = slope_signs(n, load(height)[..., 0])
    sx = -1.0 if cx < -0.1 else 1.0
    sy = -1.0 if cy < -0.1 else 1.0
    n[..., 0] *= sx
    n[..., 1] *= sy
    print("  normal convention: corr x %.3f, y %.3f -> signs %+d, %+d (stored as +u / +v)" % (cx, cy, sx, sy))
    chain = []
    for i in range(levels_of(n.shape[1], n.shape[0])):
        if i:
            n = down2(n)
            n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-12)
        chain.append(rgba8(n * 127.5 + 127.5))
    back = build("bc5", chain, out)
    ref = chain[0][..., :3].astype(float) / 127.5 - 1
    x, y = back[..., 0] / 127.5 - 1, back[..., 1] / 127.5 - 1
    got = np.stack([x, y, np.sqrt(np.clip(1 - x * x - y * y, 0, 1))], -1)
    ref /= np.linalg.norm(ref, axis=2, keepdims=True)
    got /= np.linalg.norm(got, axis=2, keepdims=True)
    ang = np.degrees(np.arccos(np.clip((ref * got).sum(2), -1, 1)))
    print("  BC5 level 0 (z rebuilt): angle error mean %.2f, 99%% %.2f deg" % (ang.mean(), np.percentile(ang, 99)))


def height(src, out):
    h = load(src)[..., 0]
    lo, hi = h.min(), h.max()
    if hi <= lo:
        sys.exit("%s: flat height map" % src)
    h = (h - lo) * (255.0 / (hi - lo))
    print("  height range %d..%d stretched to 0..255" % (lo, hi))
    chain = []
    for i in range(levels_of(h.shape[1], h.shape[0])):
        if i:
            h = down2(h)
        chain.append(rgba8(h))
    back = build("bc4", chain, out)
    err = np.abs(chain[0][..., 0].astype(int) - back[..., 0])
    print("  BC4 level 0: max error %d, mean %.2f (of 255)" % (err.max(), err.mean()))


if __name__ == "__main__":
    a = sys.argv[1:]
    if len(a) >= 3 and a[0] == "albedo":
        albedo(a[1], a[2], raw="--raw" in a)
    elif len(a) >= 5 and a[0] == "normal" and a[3] == "--height":
        normal(a[1], a[2], a[4])
    elif len(a) >= 3 and a[0] == "height":
        height(a[1], a[2])
    else:
        sys.exit(__doc__)
