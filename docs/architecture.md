# Architecture (internal)

Windows-only Tauri app that edits a bundled 3DS platformer template and runs `make` via the user’s local **devkitPro** install.

## Components

1. **Template manager** — Copies `src-tauri/template/` to a user-chosen folder on New Project.
2. **File bridge** — Writes levels into `source/main.cpp`, regenerates `game_config.h`, imports assets into `gfx/` / `romfs/`.
3. **Build runner** — Invokes MSYS2 bash + `make` / `make cia`, streams logs to the UI. Optional 3dslink to a LAN IP.

```text
UI (src/)  →  Rust (src-tauri/)  →  user project folder  →  .3dsx / .cia
                                      ↑
                               local DEVKITPRO (not bundled)
```

## Planned vs shipped (as of Phase 0)

| Item | Status |
|------|--------|
| Level editor, physics, assets, CIA build, 3dslink | Shipped |
| Configurable toolchain path / setup wizard | Planned (Phases 1–2) |
| Launch in Citra from Studio | Planned (optional Phase 2) |
| Bundled full toolchain | Not planned for v0.1 |

## Notes

- Default documented toolchain location: `C:\devkitPro` (override work is Phase 1).
- Studio build on Windows needs MSVC; see `docs/PLATFORM.md` and `scripts/tauri-dev.ps1`.
