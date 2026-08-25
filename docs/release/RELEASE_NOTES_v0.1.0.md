# 3DS Studio v0.1.0

**A Windows editor for homebrew Nintendo 3DS platformers — paint levels, import art, build real `.3dsx` / `.cia` packages for your console.**

No Nintendo SDK. No emulator bundled. No subscription, no telemetry, no cloud account. Install Studio, create a project, and ship a game you can run on hardware you own.

`Windows 10/11` · `MIT` · `Tauri` · `devkitPro` · `No tracking` · `Hardware-first`

---

## Why 3DS Studio

Most “make a game” tools either stay on the PC or hide the console behind a black box. 3DS Studio is built for people who want a **real homebrew title on a real 3DS**: a visual level editor on Windows, a proven platformer template under the hood, and one-click builds when [devkitPro](https://devkitpro.org/) is installed.

**For creators:** you don't write C++ to place tiles or tune jump height — the editor writes the code for you.  
**For developers:** every project is a normal devkitPro Makefile tree you can open, extend, and version-control like any other homebrew repo.

Unofficial homebrew tooling. **Not affiliated with Nintendo.**

---

## Features

### Projects & library

- **Start Fresh Example** — playable starter game with tutorial dialogue and multiple worlds
- **New Project…** — blank project from the bundled template
- **Your projects** — library under `Documents\3DSStudio` with open, **Rename**, and **Delete**
- **Save / Save As…** — write levels to disk and sync into `source/main.cpp` + `game_config.h`
- **Folder** — jump straight to the project root in Explorer (where builds land)

### Level editor

- **Tile painting** — paint, erase, and fill from the palette
- **Crackers, enemies, spawns, checkpoints, win flags, warps, 3D props** — placement tools with on-canvas tooltips
- **Moving platforms & crumble tiles** — with a live **Level Budget** so you stay within 3DS memory limits
- **Worlds & secrets** — menu level plus World 1, World 2, and hidden levels
- **Per-level features** — double jump, wall jump, dash, ground pound, minimap, dialogue (toggle per level)
- **Global & per-level physics** — gravity, jump force, move speed, dash speed, and more via sliders
- **Dialogue** — pre- and post-level lines tied to the selected level
- **Autosave** while you edit (use **Save** before Build if you want to be sure)

### Assets

- **Guided import slots** — each slot shows required format (e.g. `PNG 256×240`)
- **Placeholder art** on fresh projects so levels look playable before you draw anything
- **Default tileset** included — replace slots with your own PNGs when ready
- **Optional MP3 soundtrack** — import from the Assets panel; rebuild after changes

### Build & deploy

- **Build 3dsx** — Homebrew Launcher package in the project root
- **Build CIA** — installable title (needs `makerom` + `bannertool`; see [Toolchain guide](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/TOOLCHAIN.md))
- **3dslink** — send a fresh `.3dsx` to a CFW 3DS on your LAN (no cloud upload)
- **Clean** — `make clean` from the UI
- **Build console** — streamed compiler output with clear success/failure paths
- **Setup tools… wizard** — detect, browse, and retest your `DEVKITPRO` path (`C:\devkitPro` or custom)

### Help & docs

- **In-app Help** — editing, assets, build, playing on hardware, audio (`dspfirm`), saves, CIA tools, troubleshooting
- **Markdown docs** in the repo — [User guide](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/USER_GUIDE.md), [Playing on 3DS](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/PLAYING.md), [Troubleshooting](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/TROUBLESHOOTING.md)

### Privacy & packaging

- **No telemetry, no auto-update, no background downloads** (except WebView2 bootstrapper if missing)
- **Per-user NSIS installer** — no admin required
- **MIT licensed** — fork-friendly; see [LICENSE](https://github.com/jackrbaca1/3DS-Studio/blob/main/LICENSE) and [THIRD_PARTY.md](https://github.com/jackrbaca1/3DS-Studio/blob/main/THIRD_PARTY.md)

---

## Quick start (creators)

1. Download **`3DS Studio_0.1.0_x64-setup.exe`** below.
2. Run the installer. If Windows SmartScreen appears: **More info → Run anyway** (unsigned build).
3. Open **3DS Studio** from the Start Menu.
4. Click **Start Fresh Example** (or **New Project…**).
5. Paint a level, tweak physics, import art if you like.
6. When you're ready to play on hardware: install [devkitPro](https://devkitpro.org/), open **Setup tools…**, then **Build 3dsx**.
7. Copy the `.3dsx` to your 3DS SD card — or use **3dslink** on the same Wi‑Fi network.

Full walkthrough: [User guide](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/USER_GUIDE.md) · Playing on console: [PLAYING.md](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/PLAYING.md)

---

## For developers

```text
UI (src/)  →  Rust IPC (src-tauri/)  →  user project folder  →  make / devkitPro  →  .3dsx / .cia
```

- **Stack:** Tauri 2 · Rust · vanilla JS level editor · bundled libctru platformer template
- **Build from source:** `npm install` → `npm run dev` (Windows; see [PLATFORM.md](https://github.com/jackrbaca1/3DS-Studio/blob/main/docs/PLATFORM.md))
- **Release packaging:** `npm run tauri:build` then `npm run release:package`
- **Outputs:** `.3dsx` / `.cia` next to the project `Makefile`, not inside `build/`
- **Project paths:** no spaces in folder names (GNU make / devkitPro requirement)

Contributions and issues welcome on [GitHub](https://github.com/jackrbaca1/3DS-Studio).

---

## Download this release

| Asset | Purpose |
|-------|---------|
| **3DS Studio_0.1.0_x64-setup.exe** | Windows installer (required) |
| **3DSStudio-ExamplePlatformer-v0.1.0.3dsx** | Pre-built demo you can run on a 3DS without compiling |
| **3DSStudio-ExamplePlatformer-v0.1.0-project.zip** | Example project folder to open in Studio |
| **SHA256SUMS.txt** | Verify downloads (recommended) |
| **LICENSE** · **THIRD_PARTY.md** · **RELEASE_NOTES.md** | Legal and attribution |

Verify the installer hash against `SHA256SUMS.txt` before running.

---

## Requirements

| Task | What you need |
|------|----------------|
| **Run Studio** | Windows 10/11 x64, [WebView2](https://developer.microsoft.com/microsoft-edge/webview2/) (usually preinstalled) |
| **Compile games** | [devkitPro](https://devkitpro.org/) with `devkitARM` — **not bundled** |
| **Play on 3DS** | Your CFW console, SD card / FTP / 3dslink; optional `dspfirm.cdc` on SD for audio |

---

## Known limitations (v0.1.0)

- Unsigned installer → SmartScreen “Windows protected your PC” (expected; verify SHA256)
- NSIS per-user installer only (no MSI)
- CIA builds need extra tools (`makerom`, `bannertool`) beyond base devkitPro
- Studio does **not** emulate or play games on the PC — hardware-first by design
- No automatic updates; watch [Releases](https://github.com/jackrbaca1/3DS-Studio/releases) for new versions

---

## About

**3DS Studio** is an independent portfolio project by **[Jack Baca](https://github.com/jackrbaca1)** — a visual toolchain for 3DS homebrew platformers, released under MIT so others can learn from and build on it.

Questions, bugs, or ideas: [open an issue](https://github.com/jackrbaca1/3DS-Studio/issues).
