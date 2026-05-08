# Building vdec2_capture.pkg from Windows

The OpenOrbis SDK is Linux-native. From Windows you have three options.

## Option A — GitHub Actions (recommended, no local setup)

1. Create a new repo on GitHub (or use a folder in your shadPS4 fork).
2. Drop these files into the repo root:
   - `vdec2_capture.cpp`
   - `Makefile`
   - `.github/workflows/build-vdec2-capture.yml` (rename `build-vdec2-capture.yml`
     and put it in the `.github/workflows/` directory)
3. Commit and push. The workflow runs automatically.
4. Go to the **Actions** tab on GitHub, click the latest run, scroll to
   **Artifacts**, download `vdec2_capture-pkg.zip`.
5. Extract `vdec2_capture.pkg`. Send to PS4 via Remote PKG Installer.

## Option B — WSL (Windows Subsystem for Linux)

If you have WSL:

```powershell
# Open WSL shell from PowerShell
wsl

# Inside WSL, navigate to the Windows folder
cd "/mnt/c/Users/joker/Desktop/vdec app"

# Install OpenOrbis (one time only)
git clone https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain.git ~/openorbis
export OO_PS4_TOOLCHAIN=$HOME/openorbis

# Build
make
```

If you don't have WSL: `wsl --install` from an admin PowerShell, then reboot.

## Option C — Native Windows (uncommon)

OpenOrbis does ship Windows binaries under `bin/windows/` but the build
scripts assume Unix. Use Git Bash or MSYS2:

```
# In Git Bash
export OO_PS4_TOOLCHAIN=/c/path/to/OpenOrbis-PS4-Toolchain
make
```

---

## After you have vdec2_capture.pkg

1. Install on your jailbroken PS4 Slim (Remote PKG Installer or USB install).
2. Run from the Library / homebrew menu.
3. The homebrew prints progress to klog and exits when done.
4. FTP `/data/vdec2_capture.json` off the console.
5. Bring the JSON back here and we extend the dataset / derive the formula.

Expected JSON size: ~2-5 MB depending on how many configs are valid.
Expected runtime on PS4 Slim: 30-90 seconds (mostly kernel calls in a tight
loop).
