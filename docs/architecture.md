# Architecture (internal)

Windows-only Tauri app that edits a bundled 3DS platformer template and runs `make` via the user’s local **devkitPro** install.

## Components

1. **Template manager** — Copies `src-tauri/template/` into `Documents\3DSStudio\<Name>\` (or a chosen folder).
2. **File bridge** — Writes levels into `source/main.cpp`, regenerates `game_config.h`, imports assets into `gfx/` / `romfs/`.
3. **Toolchain** — Resolves DEVKITPRO (config / env / `C:\devkitPro`), Setup tools wizard, allowlisted https opener.
4. **Build runner** — Invokes MSYS2 bash + `make` / `make cia`, streams logs to the UI. Optional 3dslink to a LAN IP.

```text
UI (src/)  →  Rust (src-tauri/)  →  user project folder  →  .3dsx / .cia
                                      ↑
                               local DEVKITPRO (not bundled)
```

## Shipped (v0.1)

| Item | Status |
|------|--------|
| Level editor, physics, assets, CIA build, 3dslink | Shipped |
| Configurable toolchain path / Setup tools wizard / Help | Shipped |
| NSIS installer | Shipped |
| Emulator launch from Studio | Out of scope — see `docs/PLAYING.md` |
| Bundled full toolchain | Not planned |

## Notes

- Default toolchain location: `C:\devkitPro` (override via Setup tools).
- Studio build on Windows needs MSVC; see `docs/PLATFORM.md` and `scripts/tauri-dev.ps1`.
- Privacy: `docs/PRIVACY.md`. No telemetry.
