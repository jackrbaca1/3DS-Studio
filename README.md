# 3DS Studio

**A Windows editor for homebrew Nintendo 3DS platformers — paint levels, import art, build real** `.3dsx` **/** `.cia` **packages for your console.**

Plan worlds on the PC, tune physics with sliders, swap in your own PNGs and MP3s, then compile with devkitPro and run the result on hardware you own. No Nintendo SDK, no emulator bundled, no subscription.

`Windows` · `MIT` · `Tauri` · `devkitPro` · `No tracking`

**[Download v0.1.0 (Windows installer)](https://github.com/jackrbaca1/3DS-Studio/releases/latest)**

Unofficial homebrew tooling. **Not affiliated with Nintendo.** [Legal](docs/LEGAL.md) · [Privacy](docs/PRIVACY.md) · [Security](docs/SECURITY.md)

---



## Why

Most game tools either never leave the PC or hide the console pipeline. 3DS Studio sits in the middle: **a visual editor for creators**, **a normal devkitPro project for developers**. You get a level painter and asset importer; the template still builds with `make` when you're ready to ship.

---



## Features

**Projects** — example game, new project wizard, library under `Documents\3DSStudio`, rename/delete, save & save-as

**Level editor** — tiles, crackers, enemies, spawns, checkpoints, warps, 3D props, moving platforms, crumble tiles, level budget, worlds & secrets, per-level moves (double jump, wall jump, dash, ground pound), global & level physics, dialogue

**Assets** — guided PNG/MP3 import with size hints; starter tileset + labeled placeholders

**Build** — Setup tools wizard, Build 3dsx, Build CIA, 3dslink over LAN, streamed build console, clean

**Help** — in-app guide plus [user docs](docs/USER_GUIDE.md) for playing on SD/FTP, audio (`dspfirm`), saves, CIA tools

---



## Quick start

1. Download the installer from **[Releases](https://github.com/jackrbaca1/3DS-Studio/releases/latest)**.
2. Run setup. If SmartScreen warns: **More info → Run anyway** (unsigned build).
3. Open **3DS Studio** → **Start Fresh Example**.
4. Edit a level. When ready for hardware: install [devkitPro](https://devkitpro.org/), use **Setup tools…**, then **Build 3dsx**.
5. Copy to your 3DS SD card or use **3dslink**. [Playing guide](docs/PLAYING.md)

---



## Requirements


| Task       | Need                                                          |
| ---------- | ------------------------------------------------------------- |
| Run Studio | Windows 10/11 x64, WebView2                                   |
| Compile    | [devkitPro](https://devkitpro.org/) + devkitARM (not bundled) |
| Play       | CFW 3DS, SD / FTP / 3dslink                                   |


---



## Docs


|                                            |                                  |
| ------------------------------------------ | -------------------------------- |
| [User guide](docs/USER_GUIDE.md)           | Projects, editor, assets, build  |
| [Toolchain](docs/TOOLCHAIN.md)             | devkitPro, CIA tools             |
| [Playing on 3DS](docs/PLAYING.md)          | SD, FTP, 3dslink, audio, saves   |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | SmartScreen, paths, build errors |
| [Changelog](CHANGELOG.md)                  | Version history                  |


---



## Build from source

```powershell
npm install
npm run dev
```

Release packaging: `npm run tauri:build` then `npm run release:package`. See [PLATFORM.md](docs/PLATFORM.md).

---



## AI assistance disclaimer

**Most of the application was written with AI coding tools.** I am not claiming to have authored this codebase line-by-line by hand, and I do not personally understand the implementation in detail.

What I *did* own and drive:

- Product direction — what 3DS Studio should do for creators and for hardware
- Pipelines — projects → editor → assets → `make` / devkitPro → `.3dsx` / `.cia`
- Tooling & packaging — Windows install flow, Setup tools, docs, release artifacts
- Hardware path — building for a real CFW 3DS (SD / FTP / 3dslink), not an in-app emulator

Treat this as an **AI-assisted project** that I designed, steered, tested, and shipped. If you need a maintainer who can explain every Rust/JS/C++ detail from memory, that is not me. Open an issue for bugs or questions, and **expect honesty over gatekeeping.**

---



## About

Created by **[Jack Baca](https://github.com/jackrbaca1)**. MIT — [LICENSE](LICENSE) · [THIRD_PARTY.md](THIRD_PARTY.md)

Issues and feedback: [GitHub Issues](https://github.com/jackrbaca1/3DS-Studio/issues)