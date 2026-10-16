#!/usr/bin/env python3
"""assets/images/flare/glare.dds: the uneven sun rays of the lens flare (ps_post_final).

  python3 tools/make_glare.py [OUT.dds]

Photographic streaks come from diffraction by the fine hairline scratches and dust on a real front
element (the same model Ritschel et al., "Temporal Glare", Eurographics 2009, use for the eye's
ciliary corona): each thin straight scratch throws a streak perpendicular to itself, many random
ones give the uneven fan of photos. Pupil image: aperture disc minus SCRATCHES random hairlines
(1-3 px) and DUST specks (seeded); far field |FFT|^2 at a reference wavelength, rescaled radially by
lambda / 550 nm for 450..660 nm (longer wavelengths reach further: warm-tinted tips) and summed to
RGB; unit total energy. Then, in 1080p screen units (1 pattern px = PATTERN_PX screen px): times the
sun's flux (sun / sunlit-white luminance 5.6e4 x its disc, radius SUN_PX) x GLARE (how dirty the
lens is), convolved with the sun disc and softened (SOFT_PX), faded to zero over the outer quarter,
cropped to SIZE_H image heights and resampled to TEX px. Stored as value / STORE_MAX, RGBA8 sRGB
(tools/make_textures.py --raw), rows bottom-up. ps_post_final multiplies back by STORE_MAX and the
per-frame strength (sun colour x visibility x edge fade x FLARE_RAYS). Needs numpy, scipy, Pillow."""
import math, os, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from scipy.signal import fftconvolve

N, R_PUPIL, SCRATCHES, DUST, SEED, WIDTHS = 2048, 260, 180, 160, 7, (1, 1, 1, 2, 2, 3)
PATTERN_PX = 1.1          # 1080p screen px per pattern px
SUN_PX = 8.0              # the real sun's image radius at 1080p (0.533 deg across 35 deg of FOV)
K_SUN = 5.6e4             # sun luminance / sunlit white
GLARE = 0.09              # lens dirtiness: keeps the rays subtle (approved preview)
SOFT_PX = 6.0             # extra softening, 1080p px
SIZE_H = 1.2              # texture covers this many image heights, centred on the sun
TEX = 512
STORE_MAX = 4.0           # stored value = glare (x white) / STORE_MAX, clamped


def pupil():
    rng = np.random.default_rng(SEED)
    y, x = np.mgrid[0:N, 0:N] - N / 2.0
    ap = (np.hypot(x, y) <= R_PUPIL).astype(np.float64)
    occ = Image.new("L", (N, N), 0); d = ImageDraw.Draw(occ)
    for _ in range(SCRATCHES):
        r = R_PUPIL * math.sqrt(rng.random()); a = rng.uniform(0, 2 * math.pi)
        cx, cy = N / 2 + r * math.cos(a), N / 2 + r * math.sin(a)
        ln = rng.uniform(0.15, 0.9) * R_PUPIL; ang = rng.uniform(0, math.pi)
        dx, dy = 0.5 * ln * math.cos(ang), 0.5 * ln * math.sin(ang)
        d.line([(cx - dx, cy - dy), (cx + dx, cy + dy)], fill=int(rng.uniform(90, 255)), width=int(rng.choice(WIDTHS)))
    for _ in range(DUST):
        r = R_PUPIL * math.sqrt(rng.random()); a = rng.uniform(0, 2 * math.pi); s = rng.uniform(0.6, 2.2)
        cx, cy = N / 2 + r * math.cos(a), N / 2 + r * math.sin(a)
        d.ellipse([cx - s, cy - s, cx + s, cy + s], fill=255)
    return ap * (1.0 - np.asarray(occ, np.float64) / 255.0)


def spectral(I, lams=(450, 480, 510, 540, 570, 600, 630, 660), ref=550.0):
    c = N / 2.0; yy, xx = np.mgrid[0:N, 0:N] - c
    w = lambda l: np.array([math.exp(-((l - 600) / 40) ** 2), math.exp(-((l - 545) / 40) ** 2), math.exp(-((l - 460) / 35) ** 2)])
    rgb = np.zeros((N, N, 3)); tot = np.zeros(3)
    for l in lams:
        s = l / ref
        Il = ndimage.map_coordinates(I, [yy / s + c, xx / s + c], order=1, mode="constant") / (s * s)
        rgb += Il[..., None] * w(l); tot += w(l)
    return rgb / tot


def build():
    F = np.fft.fftshift(np.fft.fft2(pupil())); I = np.abs(F) ** 2; I /= I.sum()
    rgb = spectral(I)
    flux = K_SUN * math.pi * SUN_PX ** 2                      # white x px^2
    rgb *= flux / PATTERN_PX ** 2 * GLARE                    # x white, per 1080p px
    rs = SUN_PX / PATTERN_PX; k = int(math.ceil(rs)) + 1
    yy, xx = np.mgrid[-k:k + 1, -k:k + 1]; disc = (np.hypot(xx, yy) <= rs).astype(float); disc /= disc.sum()
    half = int(round(SIZE_H * 1080 / PATTERN_PX / 2))
    crop = rgb[N // 2 - half:N // 2 + half, N // 2 - half:N // 2 + half]
    crop = np.stack([ndimage.gaussian_filter(fftconvolve(crop[..., c], disc, mode="same"), SOFT_PX / PATTERN_PX) for c in range(3)], -1)
    n = crop.shape[0]; q = np.hypot(*(np.mgrid[0:n, 0:n] - n / 2 + 0.5)) / (n / 2)
    f = np.clip((1.0 - q) / 0.25, 0, 1); crop *= (f * f * (3 - 2 * f))[..., None]
    tex = np.stack([np.asarray(Image.fromarray(crop[..., c].astype(np.float32), "F").resize((TEX, TEX), Image.BOX)) for c in range(3)], -1)
    return np.clip(tex / STORE_MAX, 0, 1)


def to_srgb8(c):
    c = np.where(c <= 0.0031308, 12.92 * c, 1.055 * np.power(c, 1 / 2.4) - 0.055)
    return np.clip(np.round(c * 255), 0, 255).astype(np.uint8)


if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "assets", "images", "flare", "glare.dds")
    tex = build()
    sys.path.insert(0, here); import make_textures
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "glare.png")
        Image.fromarray(to_srgb8(tex[::-1])).save(png)          # rows bottom-up for the loader
        os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
        make_textures.albedo(png, out, raw=True)
    print("%s: %dx%d RGBA8 sRGB, covers %.1f image heights, stored x%.1f; max %.3f, rays at 0.2 H: %.4f" % (
        out, TEX, TEX, SIZE_H, STORE_MAX, tex.max() * STORE_MAX, tex[TEX // 2 + int(0.2 * TEX / SIZE_H), TEX // 2 - 40:TEX // 2 + 40, 1].mean() * STORE_MAX))
