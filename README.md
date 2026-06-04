# PS4 homebrew build template (OpenOrbis)

Reusable build. Put your code in `src/`, push, and GitHub Actions produces the `.pkg`.
Package identity (name / title id / icon) is fixed and reused across builds.

## Use it
1. Edit the identity block at the top of `Makefile` once:
   - `TITLE` (menu name), `TITLE_ID` (e.g. `BREW00001`), `CONTENT_ID`, `VERSION`.
   - Replace `sce_sys/icon0.png` (512x512) with your icon.
2. Drop sources into `src/` — every `.c`, `.cpp`, `.s` there is compiled and linked
   automatically. To add libraries, append to `LIBS` in the Makefile.
3. Commit + push. The `build` workflow runs, and the `.pkg` + `eboot.bin` are in the
   run's **Artifacts**.

That's it — to make a new homebrew, swap the files in `src/`; name/icon stay the same.

## libc + FIOS2
Both are linked by default (`-lc -lSceFios2`, plus the usual `-lkernel -lc++
-lSceLibcInternal`). The PS4 resolves the real `libc.prx` / `libSceFios2.prx` from the
system at load. `crt1.o` (OpenOrbis) does the libc init, so `malloc`/`printf`/etc. work.

## Bundling the two PRX (optional)
The jailbroken PS4 already provides `libc.prx` and `libSceFios2.prx`, so you normally
don't ship them. If you specifically want them inside the package, drop the files into
`sce_module/` — anything there is added to the pkg automatically.

## Local build (Linux/WSL with the toolchain)
```
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis-PS4-Toolchain
make            # -> <CONTENT_ID>.pkg + eboot.bin
make clean
```
CI pins toolchain **v0.5.4 (LLVM 18)**; bump the version in `.github/workflows/build.yml`
if needed.

## Notes
- `sce_sys/about/right.sprx` is staged from the toolchain by CI. For local builds, copy it
  in yourself (it ships with the OpenOrbis toolchain).
- `PkgTool.Core` needs `libicu` (CI installs it).
