# Platform

## Supported for v0.1

| Piece | Platform |
|-------|----------|
| 3DS Studio editor | Windows 10/11 x64 |
| Building games from Studio | Windows + [devkitPro](https://devkitpro.org/) (devkitARM, 3ds-dev tools) |
| Playing builds | CFW 3DS (SD / FTP / 3dslink). Emulators optional and unsupported by Studio — see [PLAYING.md](PLAYING.md) |

macOS / Linux Studio builds are not supported yet.

## Versions used in development (Phase 0 snapshot)

| Tool | Version |
|------|---------|
| Node.js | 24.x |
| npm | 11.x |
| Rust (rustc) | 1.95.x (`x86_64-pc-windows-msvc`) |
| Tauri | 2.x |
| @tauri-apps/cli | 2.x (see `package-lock.json`) |

## Windows toolchain conflict

devkitPro MSYS2 ships a GNU `link.exe`. Rust needs MSVC `link.exe`. Always start Studio with:

```powershell
npm run dev
```

or `npm run tauri:build` — both use `scripts/tauri-dev.ps1`, which strips MSYS2 from PATH and loads Visual Studio vcvars.

## Build output location

If the repo lives under OneDrive (or another synced folder), Cargo may fail to write `src-tauri/target`. The helper script sets:

`CARGO_TARGET_DIR=%LOCALAPPDATA%\3ds-studio-cargo-target`

## Privacy note (Phase 0)

Studio source has **no** analytics, crash-upload, or update-check endpoints. Local file I/O and optional LAN `3dslink` only. Formal `docs/PRIVACY.md` is Phase 4.
