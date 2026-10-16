#!/usr/bin/env python3
"""Writes the GP4 project PkgTool.Core packs (replaces OpenOrbis create-gp4).

  python3 tools/make_gp4.py --out pkg.gp4 --content-id ID FILE...

Same project as create-gp4 (volume, package, one chunk / scenario, one <file>
per input with targ_path = orig_path), except <rootdir>: create-gp4 always
writes the fixed tree sce_sys/about, sce_module, assets/{audio,fonts,images,
misc,videos}, and PkgTool.Core's BuildFSTree fails ("Sequence contains no
elements" in FindDir) for a file in any directory not declared there, e.g.
assets/images/cube/. Here every directory that holds a file is declared."""
import argparse, datetime
from xml.sax.saxutils import quoteattr


def tree(paths):
    root = {}
    for p in paths:
        node = root
        for part in p.split("/")[:-1]:
            node = node.setdefault(part, {})
    return root


def emit(node, depth, out):
    for name in sorted(node):
        pad = "\t" * depth
        if node[name]:
            out.append("%s<dir targ_name=%s>" % (pad, quoteattr(name)))
            emit(node[name], depth + 1, out)
            out.append("%s</dir>" % pad)
        else:
            out.append("%s<dir targ_name=%s />" % (pad, quoteattr(name)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--content-id", required=True)
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    files = [f.replace("\\", "/") for f in a.files]
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    x = ['<?xml version="1.0"?>',
         '<psproject xmlns:xsd="http://www.w3.org/2001/XMLSchema" '
         'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" fmt="gp4" version="1000">',
         "\t<volume>",
         "\t\t<volume_type>pkg_ps4_app</volume_type>",
         "\t\t<volume_id>PS4VOLUME</volume_id>",
         "\t\t<volume_ts>%s</volume_ts>" % ts,
         '\t\t<package content_id=%s passcode="00000000000000000000000000000000"' % quoteattr(a.content_id),
         '\t\t\tstorage_type="digital50" app_type="full" />',
         '\t\t<chunk_info chunk_count="1" scenario_count="1">',
         "\t\t\t<chunks>",
         '\t\t\t\t<chunk id="0" layer_no="0" label="Chunk #0" />',
         "\t\t\t</chunks>",
         '\t\t\t<scenarios default_id="0">',
         '\t\t\t\t<scenario id="0" type="sp" initial_chunk_count="1" label="Scenario #0">0</scenario>',
         "\t\t\t</scenarios>",
         "\t\t</chunk_info>",
         "\t</volume>",
         '\t<files img_no="0">']
    for f in files:
        x.append("\t\t<file targ_path=%s orig_path=%s />" % (quoteattr(f), quoteattr(f)))
    x.append("\t</files>")
    x.append("\t<rootdir>")
    emit(tree(files), 2, x)
    x.append("\t</rootdir>")
    x.append("</psproject>")
    with open(a.out, "w", newline="\n") as fh:
        fh.write("\n".join(x) + "\n")


if __name__ == "__main__":
    main()
