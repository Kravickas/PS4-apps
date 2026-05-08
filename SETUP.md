# Setup: build vdec2_capture via GitHub Actions

## One-time setup

1. **Pick a repo.** Either:
   - Create a new GitHub repo (recommended for keeping homebrew separate), or
   - Add to a folder in your existing shadPS4 fork.

2. **Drop these files into the repo root:**
   ```
   vdec2_capture.cpp
   Makefile
   .github/workflows/build-vdec2-capture.yml   <- rename build-vdec2-capture.yml to this exact path
   ```

   On Windows in your `vdec app` folder, the structure should look like:
   ```
   vdec app\
   ├─ vdec2_capture.cpp
   ├─ Makefile
   └─ .github\
      └─ workflows\
         └─ build-vdec2-capture.yml
   ```

3. **Commit and push.** The Actions tab fires off a build automatically when
   you push, or you can run it manually via "Run workflow" from the Actions
   page (the workflow includes `workflow_dispatch`).

## Each subsequent build

Either:
- Push a change (any edit to `vdec2_capture.cpp` or `Makefile` triggers it), or
- Click **Actions** → **build-vdec2-capture** → **Run workflow**.

Wait ~2-3 minutes. Click into the run, scroll to **Artifacts** at the bottom:
- **vdec2_capture-eboot** — `eboot.bin` (the actual homebrew binary)
- **vdec2_capture-elf** — unsigned ELF for inspection

## Wrapping eboot.bin into a .pkg

The CI doesn't produce a .pkg directly because PS4 PKG creation needs
graphics/icon assets and a `param.sfo`. Use one of these tools to wrap
the `eboot.bin` into a homebrew .pkg:

- **PkgTool.Core** (LibOrbisPkg) — open source, .NET-based
- **OpenOrbis-Publishing-Tools** — has GUI for this
- Or any FPKG creator you already use for your homebrew

Minimum assets you'll need alongside eboot.bin:
- `param.sfo` (title id `SHAD00001`, title `vdec2 capture`, version `01.00`)
- `icon0.png` (256×256, can be anything — even a solid color)

If you want, I can also add a PKG-creation step to the workflow using
PkgTool.Core, but it adds complexity and is easy to mess up — I'd prefer to
hand off `eboot.bin` and let you wrap it with the same tool you use for
your other homebrew (you've done this with `fp_dump_ps4`).

## Running on PS4

1. Install the .pkg via Remote PKG Installer or USB.
2. Run from the PS4 home menu.
3. Wait for it to finish (the homebrew exits when done — you'll see the menu
   reappear). Klog output goes to klog if you have it hooked up.
4. FTP `/data/vdec2_capture.json` off the console.
5. Send the JSON back here and we'll process it.

## Troubleshooting

**Build fails with `cannot find -lSceVideodec2`** — the OpenOrbis stub for
that library may be missing in this toolchain version. Edit `Makefile`,
remove `-lSceVideodec2` from `LIBS`, push again. The runtime loader will
resolve the symbols from the PS4's actual `libSceVideodec2.sprx`.

**Build fails with `crt1.o not found`** — toolchain didn't extract correctly.
Re-run the workflow (cache will refresh).

**Workflow doesn't trigger automatically** — paths filter is strict; just
use the manual "Run workflow" button.

**eboot.bin is suspiciously small (under 50 KB)** — link succeeded but stub
wasn't included. Check the workflow log for unresolved symbol warnings.
