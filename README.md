# 3DS Studio — Platformer Editor

Windows desktop editor (Tauri) for homebrew Nintendo 3DS platformer projects. Bundles a game template; compiling requires a separate [devkitPro](https://devkitpro.org/) install.

Unofficial. Not affiliated with Nintendo. See [docs/LEGAL.md](docs/LEGAL.md).

## Status

**v0.1 development.** Editing, assets, `.3dsx` / CIA build, and 3dslink work when the toolchain is present. Not yet packaged for casual installers (see `docs/release/`). Configurable toolchain path and in-app setup wizard are planned.

## Requirements

### Run / develop the editor

- [Node.js](https://nodejs.org/)
- [Rust](https://rustup.rs/) (`x86_64-pc-windows-msvc`)
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) — workload **Desktop development with C++**
- WebView2 (usually already on Windows 10/11)

### Build games from the editor

- [devkitPro](https://devkitpro.org/) with `devkitARM` (default path `C:\devkitPro`)
- For CIA: `makerom` and `bannertool` on the toolchain PATH

devkitPro is **not** bundled. Studio only calls it when you click Build.

## Develop

```powershell
npm install
npm run dev
```

Use `npm run dev` / `npm run tauri:build`, not plain `npx tauri` — see [docs/PLATFORM.md](docs/PLATFORM.md) (MSVC vs MSYS2 `link.exe`, OneDrive/`CARGO_TARGET_DIR`).

In the app: New Project or Open → edit → Save → Build → `.3dsx` in the project folder.

## Layout

| Path | Role |
|------|------|
| `src/` | Editor UI |
| `src-tauri/` | Rust backend + bundled `template/` |
| `scripts/tauri-dev.ps1` | Windows PATH + MSVC launcher |
| `docs/release/` | Public release phases |
| `docs/LEGAL.md` | Disclaimer |
| `THIRD_PARTY.md` | Attribution |
| `LICENSE` | MIT |

## License

MIT. Third-party notices: [THIRD_PARTY.md](THIRD_PARTY.md).
