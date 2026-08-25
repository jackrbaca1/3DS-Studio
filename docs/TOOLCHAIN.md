# Toolchain (devkitPro)

3DS Studio **does not bundle** the 3DS compiler. Install [devkitPro](https://devkitpro.org/) yourself, then point Studio at it.

Official guide: [Getting Started](https://devkitpro.org/wiki/Getting_Started) (https only).

## What Studio expects (Windows)

Typical root: **`C:\devkitPro`**

| Component | Role |
|-----------|------|
| `msys2\usr\bin\bash.exe` | Runs `make` |
| `devkitARM\bin\arm-none-eabi-gcc.exe` | Compiles the game |
| `tools\bin\3dslink.exe` | Optional send-to-3DS |
| `tools\bin\makerom.exe` + `bannertool.exe` | Optional **CIA** builds |

## Setup tools (in Studio)

**Setup tools…** (welcome or top bar):

1. Shows which tools are present under the current DEVKITPRO root.
2. **Open install docs** — official Getting Started in your browser.
3. **Browse for DEVKITPRO…** — pick the folder that contains `msys2` and `devkitARM`.
4. **Retest** — orange while testing; green when ready; red if invalid.
5. **Use default path** — clear a bad custom path and fall back to env / `C:\devkitPro`.

A wrong folder (e.g. Desktop) **stays selected** so you can see the failure; browse again to `C:\devkitPro` when ready.

Unix-style env values like `/opt/devkitpro` are **ignored** on Windows so a polluted `DEVKITPRO` does not break builds.

Saved path: `%APPDATA%\3ds-studio\config.json`.

## CIA extras

For **Build CIA**:

- Install `makerom` ([Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases)) and `bannertool` ([3ds-bannertool releases](https://github.com/carstene1ns/3ds-bannertool/releases)) into `DEVKITPRO\tools\bin`, **or**
- Provide prebuilt `cia-icon.icn` / `cia-banner.bnr` in the project root, plus `banner.png` (256×128) and `banner.wav`.

## Contributors: building Studio itself

If you develop the editor from source:

```powershell
npm install
npm run dev          # or npm run tauri:build
```

Always use those scripts — not bare `npx tauri`. Reasons:

1. **MSVC vs MSYS `link.exe`** — devkitPro MSYS2 puts GNU `link.exe` on PATH; Rust needs MSVC’s linker. `scripts/tauri-dev.ps1` fixes PATH and loads Visual Studio vcvars.
2. **OneDrive** — Cargo target is redirected to `%LOCALAPPDATA%\3ds-studio-cargo-target` so synced folders do not lock builds.

See [PLATFORM.md](PLATFORM.md).

## Project path rules

- No spaces in the project path (`Documents\3DSStudio\MyGame`, not `3DS Studio\My Game`).
- Outputs (`.3dsx` / `.cia`) land next to the **Makefile**.
