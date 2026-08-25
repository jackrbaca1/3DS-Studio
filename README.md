# 3DS Studio — Platformer Editor

Windows desktop editor (Tauri) for homebrew Nintendo 3DS platformer projects. Bundles a game template; compiling requires a separate [devkitPro](https://devkitpro.org/) install.

Unofficial. Not affiliated with Nintendo. See [docs/LEGAL.md](docs/LEGAL.md).

## Status

**v0.1.0** — Windows NSIS installer packaging complete (`docs/release/phase-3-packaging.md`). Editing, assets, `.3dsx` / CIA build, 3dslink, Help, and Setup tools work when the toolchain is present. **devkitPro is not bundled.**

## Install (end users)

1. Download the Windows **NSIS** installer from a GitHub Release (or from `dist/release/v0.1.0/` if you built locally).
2. Run the installer (per-user; no admin required).
3. If Windows SmartScreen warns (**unsigned** build): **More info → Run anyway**.
4. Launch **3DS Studio** from the Start Menu.

**WebView2:** Required. Usually already installed on Windows 10/11. The NSIS installer can download the Evergreen WebView2 bootstrapper if needed.

**Uninstall:** Use Windows Apps & features / Settings. That removes the app binaries and Start Menu shortcut. Local data may remain under `%APPDATA%\3ds-studio` (toolchain path setting) and projects under `Documents\3DSStudio`.

**Updates:** This build does not check for updates or download installers automatically.

After installing games you build, see [docs/PLAYING.md](docs/PLAYING.md) (SD / FTP / 3dslink, `dspfirm`, saves).

## Requirements

### Run the installed editor

- Windows 10/11 x64
- WebView2 Runtime (see above)
- For **Build**: [devkitPro](https://devkitpro.org/) with `devkitARM` (default `C:\devkitPro`). Use in-app **Setup tools…** if needed.

### Develop the editor (contributors)

- [Node.js](https://nodejs.org/)
- [Rust](https://rustup.rs/) (`x86_64-pc-windows-msvc`)
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) — workload **Desktop development with C++**
- WebView2

### Build games from the editor

- [devkitPro](https://devkitpro.org/) with `devkitARM` (default path `C:\devkitPro`)
- For CIA: `makerom` and `bannertool` on the toolchain PATH

devkitPro is **not** bundled. Studio only calls it when you click Build.

## Develop

```powershell
npm install
npm run dev
```

Package a release installer (after a successful build):

```powershell
npm run tauri:build
npm run release:package
```

Use `npm run dev` / `npm run tauri:build`, not plain `npx tauri` — see [docs/PLATFORM.md](docs/PLATFORM.md) (MSVC vs MSYS2 `link.exe`, OneDrive/`CARGO_TARGET_DIR`).

In the app: **Start Fresh Example**, **New Project…** (named save in `Documents/3DSStudio`, no spaces), or open from **Your projects**. Use **Save** / **Save As…** like other creative tools.

## Layout

| Path | Role |
|------|------|
| `src/` | Editor UI |
| `src-tauri/` | Rust backend + bundled `template/` |
| `scripts/tauri-dev.ps1` | Windows PATH + MSVC launcher |
| `scripts/package-release.ps1` | Copy NSIS + checksums into `dist/release/` |
| `docs/release/` | Public release phases |
| `docs/LEGAL.md` | Disclaimer |
| `docs/PLAYING.md` | Install builds on 3DS (SD, FTP, 3dslink), audio, saves |
| `THIRD_PARTY.md` | Attribution |
| `LICENSE` | MIT |

## License

MIT. Third-party notices: [THIRD_PARTY.md](THIRD_PARTY.md).
