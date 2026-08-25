# 3DS Studio — Platformer Editor

**Windows** desktop editor for homebrew **Nintendo 3DS** platformers. Paint levels, import assets, and build `.3dsx` / `.cia` packages. Studio does **not** play games or launch emulators — you copy builds to a CFW 3DS.

Unofficial. **Not affiliated with Nintendo.** [Legal](docs/LEGAL.md) · [Privacy](docs/PRIVACY.md) · [Security](docs/SECURITY.md)

## Install

1. Download **`3DS Studio_0.1.0_x64-setup.exe`** from [GitHub Releases](https://github.com/jackrbaca1/3DSPlatformerDevelopmentPlatform/releases).
2. Run the installer (per-user). If SmartScreen warns: **More info → Run anyway** (unsigned).
3. Start Menu → **3DS Studio**.

Requires **WebView2** (usually preinstalled on Windows 10/11). **devkitPro is not bundled** — install it separately to compile games ([Toolchain](docs/TOOLCHAIN.md)).

Uninstall via Windows Apps. `%APPDATA%\3ds-studio` and `Documents\3DSStudio` projects may remain. No automatic updates.

## Screenshots

Add PNGs under [`docs/images/`](docs/images/CAPTURE.md) (`welcome.png`, `editor.png`, `assets.png`, `build-success.png`, `setup-tools.png`). Until those files exist, the gallery below will not render on GitHub.

| Welcome | Editor |
|---------|--------|
| ![Welcome](docs/images/welcome.png) | ![Editor](docs/images/editor.png) |

| Assets | Build success |
|--------|----------------|
| ![Assets](docs/images/assets.png) | ![Build](docs/images/build-success.png) |

| Setup tools |
|-------------|
| ![Setup tools](docs/images/setup-tools.png) |

## What you can do

- Create / open projects under `Documents\3DSStudio` (no spaces in names)
- Edit tiles, crackers, enemies, spawns, warps, dialogue, physics
- Import art and optional soundtrack
- **Build 3dsx**, optional **Build CIA**, **3dslink** to a LAN 3DS
- In-app **Help** and **Setup tools…** wizard

## Requirements

| Task | Need |
|------|------|
| Run Studio | Windows 10/11 x64, WebView2 |
| Compile games | [devkitPro](https://devkitpro.org/) + devkitARM (e.g. `C:\devkitPro`) |
| Play builds | Your CFW 3DS (SD / FTP / 3dslink) — [Playing](docs/PLAYING.md) |

## Docs

| Doc | Topic |
|-----|--------|
| [User guide](docs/USER_GUIDE.md) | Projects, editor, assets, build |
| [Toolchain](docs/TOOLCHAIN.md) | devkitPro, CIA tools, contributor builds |
| [Playing](docs/PLAYING.md) | SD / FTP / 3dslink, audio, saves |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | SmartScreen, paths, silent audio |
| [Changelog](CHANGELOG.md) | v0.1.0+ |

## Develop (contributors)

```powershell
npm install
npm run dev
```

Package installer: `npm run tauri:build` then `npm run release:package`.  
Always use those scripts on Windows — see [PLATFORM.md](docs/PLATFORM.md).

## License

MIT. [LICENSE](LICENSE) · [THIRD_PARTY.md](THIRD_PARTY.md)
