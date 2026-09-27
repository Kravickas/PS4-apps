#!/usr/bin/env python3
"""Builds assets/ (shipped in the PKG at /app0/assets/) from sources:

  python3 tools/build_assets.py --floor DIR --music FILE [--out assets]

  DIR   holds floor_albedo, floor_normal and floor_displacement images (any
        format Pillow reads, power-of-two sizes)
  FILE  the background music (any format ffmpeg reads)

Output layout:
  models/cube/cube.obj                       gen_model.py
  images/cube/albedo.dds  normal.dds  height.dds   (logo: RGBA8, BC5, BC4)
  images/floor/albedo.dds normal.dds  height.dds   (BC1 sRGB, BC5, BC4)
  sound/bgm/bgm.wav                          make_bgm.py
Needs numpy, Pillow, quicktex and ffmpeg."""
import argparse, glob, os, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def run(*args):
    subprocess.run([sys.executable] + list(args), check=True)


def find(folder, stem):
    hits = sorted(glob.glob(os.path.join(folder, stem + ".*")))
    if not hits:
        sys.exit("missing %s.* in %s" % (stem, folder))
    return hits[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--floor", required=True)
    ap.add_argument("--music", required=True)
    ap.add_argument("--out", default=os.path.join(HERE, "..", "assets"))
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    for d in ("models/cube", "images/cube", "images/floor", "sound/bgm"):
        os.makedirs(os.path.join(out, d), exist_ok=True)
    tex = os.path.join(HERE, "make_textures.py")
    with tempfile.TemporaryDirectory() as tmp:
        run(os.path.join(HERE, "gen_model.py"), tmp)
        shutil.copyfile(os.path.join(tmp, "model.obj"), os.path.join(out, "models/cube/cube.obj"))
        cube = os.path.join(out, "images/cube")
        run(tex, "albedo", os.path.join(tmp, "model.bmp"), os.path.join(cube, "albedo.dds"), "--raw")
        run(tex, "normal", os.path.join(tmp, "model_normal.bmp"), os.path.join(cube, "normal.dds"),
            "--height", os.path.join(tmp, "model_height.bmp"))
        run(tex, "height", os.path.join(tmp, "model_height.bmp"), os.path.join(cube, "height.dds"))
    floor = os.path.join(out, "images/floor")
    run(tex, "albedo", find(a.floor, "floor_albedo"), os.path.join(floor, "albedo.dds"))
    run(tex, "normal", find(a.floor, "floor_normal"), os.path.join(floor, "normal.dds"),
        "--height", find(a.floor, "floor_displacement"))
    run(tex, "height", find(a.floor, "floor_displacement"), os.path.join(floor, "height.dds"))
    run(os.path.join(HERE, "make_bgm.py"), a.music, os.path.join(out, "sound/bgm/bgm.wav"))


if __name__ == "__main__":
    main()
