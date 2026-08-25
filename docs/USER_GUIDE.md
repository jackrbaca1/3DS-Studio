# User guide

Windows-only visual editor for homebrew **Nintendo 3DS** platformers. Studio edits projects and can build `.3dsx` / `.cia` when [devkitPro](https://devkitpro.org/) is installed. It does **not** play games or launch emulators — see [PLAYING.md](PLAYING.md).

Unofficial. Not affiliated with Nintendo. [LEGAL.md](LEGAL.md) · [PRIVACY.md](PRIVACY.md)

## Install Studio

1. Download the NSIS installer from the GitHub **Releases** page (`3DS Studio_0.1.0_x64-setup.exe`).
2. Run it (per-user; no admin). If SmartScreen appears: **More info → Run anyway** (unsigned build).
3. Start Menu → **3DS Studio**.

Needs **WebView2** (usually already on Windows 10/11). Details: root [README.md](../README.md).

## Projects

Projects live under **`Documents\3DSStudio\`** with **no spaces** in the folder name (GNU make / devkitPro requirement).

| Action | What it does |
|--------|----------------|
| **Start Fresh Example** | Creates/resets `ExamplePlatformer` with starter levels |
| **New Project…** | Named empty project from the template |
| **Open Other Folder…** | Open any existing project folder |
| **Your projects** list | Open a library project; **Rename** / **Delete** on the row |
| **Save** | Write levels + config into the current folder |
| **Save As…** | Copy the whole project under a new name |
| **Projects** | Return to the welcome screen |

## Editor basics

- **Tiles** — paint, erase, fill from the palette.
- **Crackers / enemies / spawn / win / checkpoint / warp / 3D** — placement tools (see tooltips).
- **Level Budget** — stay under entity/memory caps for the 3DS runtime.
- **Level Features** — per-level double jump, wall jump, dash, dialogue, etc.
- **Global / level physics** — sliders in the right panel.
- **Dialogue** — pre/post lines for the selected level.

Autosave can run while you edit; use **Save** before Build if unsure.

## Assets

Open the **Assets** section in the right panel. Click a slot to import.

- Fresh projects keep a default **tileset**; other art may start as labeled **placeholder** blocks until you import real PNGs.
- Each slot shows the required size/format (e.g. `PNG 256×240`).
- **Soundtrack** is optional MP3. Rebuild after importing.

## Build

Editing never requires the toolchain. Building does.

1. **Setup tools…** if status shows tools missing — point at `C:\devkitPro` (or your install).
2. **Build 3dsx** — produces `<name>.3dsx` in the **project root** (next to the Makefile), not inside `build/`.
3. **Build CIA** — needs `makerom` + `bannertool` (see [TOOLCHAIN.md](TOOLCHAIN.md)).
4. **3dslink** — after a successful `.3dsx` build, send to a CFW 3DS on your LAN.
5. **Folder** — opens the project root in Explorer.
6. **Clean** — `make clean`.

More: [TOOLCHAIN.md](TOOLCHAIN.md), [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## Help

In-app **Help** covers editing, assets, build, playing on 3DS, audio (`dspfirm`), saves, controls, CIA tools, and troubleshooting. Prefer Help for a quick lookup; these markdown docs for longer reading.

## Next steps after a successful build

[PLAYING.md](PLAYING.md) — SD card, FTP, 3dslink, audio firmware, save paths.
