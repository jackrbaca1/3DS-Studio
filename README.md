# 3DS Studio

Windows editor for making homebrew **Nintendo 3DS** platformers.

Paint levels, import art and music, then build `.3dsx` or `.cia` packages for a real CFW console. Studio does **not** emulate or play games on the PC — you transfer builds to your 3DS.

**Not affiliated with Nintendo.** [Legal](docs/LEGAL.md) · [Privacy](docs/PRIVACY.md) · [Security](docs/SECURITY.md)

## Download

**[3DS Studio v0.1.0 — Windows installer](https://github.com/jackrbaca1/3DS-Studio/releases/latest)**

1. Run the `.exe` setup (per-user install).
2. If Windows SmartScreen appears: **More info → Run anyway** (the build is unsigned).
3. Open **3DS Studio** from the Start Menu.

Needs Windows 10/11 x64 and WebView2 (usually already installed).

To **compile** games inside Studio you also need [devkitPro](https://devkitpro.org/) with `devkitARM` (not bundled). Use **Setup tools…** in the app if the toolchain isn’t found.

## What it does

- Create and manage projects under `Documents\3DSStudio`
- Edit tiles, enemies, spawns, warps, dialogue, and physics
- Import graphics and an optional soundtrack
- **Build 3dsx**, optional **Build CIA**, send over LAN with **3dslink**
- Built-in **Help** and toolchain setup wizard

## Playing on a 3DS

Copy the built `.3dsx` (or install a `.cia`) to your console — SD card, FTP, or 3dslink. Details: [Playing on hardware](docs/PLAYING.md).

## Docs

| | |
|--|--|
| [User guide](docs/USER_GUIDE.md) | Projects, editor, assets, build |
| [Toolchain](docs/TOOLCHAIN.md) | Installing / pointing at devkitPro |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | SmartScreen, paths, silent audio |
| [Changelog](CHANGELOG.md) | What’s new |

## Building from source

```powershell
npm install
npm run dev
```

Release packaging: `npm run tauri:build` then `npm run release:package`. Windows notes: [PLATFORM.md](docs/PLATFORM.md).

## License

MIT — [LICENSE](LICENSE) · [THIRD_PARTY.md](THIRD_PARTY.md)
