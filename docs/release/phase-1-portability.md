# Phase 1 — Portability and runtime reliability

**Status:** complete  
**Owner:**  
**Started:** 2026-08-25  
**Finished:** 2026-08-25  

## Goal

Studio and games work on machines that are not yours: configurable toolchain, correct save/audio paths for real hardware, no colliding CIA IDs or save filenames across projects. Studio builds `.3dsx` / `.cia` for the user to copy or 3dslink — it does not depend on an emulator.

## Out of scope

- Emulator install, detect, or launch (users may use one on their own; see `docs/PLAYING.md`)
- Full setup wizard UI polish (Phase 2 builds on this)
- Installer (Phase 3)
- Marketing docs (Phase 4)

## Prerequisites

- Phase 0 Exit complete
- CFW 3DS optional for install/play smoke test (SD copy, FTP, or 3dslink)

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| DSP firmware | Never commit or redistribute `dspfirm.cdc` from a console dump. Document: real hardware requires user dump via Luma |
| Network | `3dslink` sends `.3dsx` to a user-entered IP on LAN. No cloud. Document that IP is local-only; validate input (no shell injection) |
| Path traversal | Toolchain path + project path: reject `..` tricks when spawning bash/make |
| Command injection | Build commands must not concatenate unsanitized user strings into shell beyond quoted paths |
| Unique IDs | Random/hashed per-project IDs reduce collision; do not embed PII in IDs |
| Save data | Saves are local progress only; no accounts. Document location on SD (`sdmc:/3ds/…`) |

---

## Tasks

### 1.1 Configurable toolchain

- [x] **1.1.1** Define resolution order: usable env `DEVKITPRO` → saved Studio setting → `C:\devkitPro` (ignore Unix `/opt/devkitpro` on Windows)
- [x] **1.1.2** Replace hardcoded paths in `src-tauri/src/lib.rs` (`msys2_bash`, `arm-none-eabi-gcc`, `3dslink`, `makerom`, `bannertool`)
- [x] **1.1.3** Persist toolchain root in a small config file under `%APPDATA%\3ds-studio\` (or Tauri app config dir) — not in the game project
- [x] **1.1.4** Update `check_toolchain` to report which path was used and what is missing (gcc, bash, makerom, bannertool, 3dslink)
- [x] **1.1.5** Remove or replace hardcoded `DEFAULT_PLATFORMER` in `src/main.js`; use last-opened path or empty
- [x] **1.1.6** Security: normalize paths; refuse to run make if project path looks unsafe

### 1.2 Save / settings paths (game)

- [x] **1.2.1** Change settings + slot paths to `sdmc:/3ds/...` form
- [x] **1.2.2** On first save, create `sdmc:/3ds` if missing (`mkdir` / equivalent)
- [x] **1.2.3** Derive filename prefix from app title or project id (e.g. `sdmc:/3ds/<slug>_slot1.dat`) so games do not overwrite each other
- [x] **1.2.4** Document: old `/3ds/platformer_slot*.dat` paths are obsolete; users may start fresh
- [x] **1.2.5** Apply same changes to live platformer project **and** `src-tauri/template`
- [x] **1.2.6** Play verification is **optional hardware** — document install paths in `docs/PLAYING.md`

### 1.3 CIA unique ID

- [x] **1.3.1** Stop shipping one shared `APP_UNIQUE_ID` for every user game
- [x] **1.3.2** Generate stable per-project ID on New Project (store in project metadata / Makefile var)
- [x] **1.3.3** Document range used for homebrew title IDs and collision risk if users fork templates blindly
- [x] **1.3.4** Ensure Studio rewrite of Makefile / rsf keeps ID on rebuild

### 1.4 Audio reliability

- [x] **1.4.1** Document hardware requirement: `sdmc:/3ds/dspfirm.cdc` (user dump via Luma)
- [x] **1.4.2** Soundtrack optional; Studio warns / skips when missing; do not ship uncleared music in releases
- [x] **1.4.3** SFX-without-music behavior documented (engine no-ops missing MP3); confirm on hardware when convenient
- [x] **1.4.4** Check mute settings do not leave NDSP in a bad state (unchanged; mute only adjusts mix)

### 1.5 Build tool discovery

- [x] **1.5.1** Clear errors when `makerom` / `bannertool` missing (CIA path)
- [x] **1.5.2** Detect `banner.wav` / prebuilt `.icn`/`.bnr` paths; keep auto-silent-wav behavior if already present
- [x] **1.5.3** Log resolved DEVKITPRO at start of each build

### 1.6 Regression matrix

Studio **must** verify builds. Hardware play is recommended but not a Phase 1 blocker. Emulators are out of scope for Studio development.

Library path: `Documents/3DSStudio/<Name>` — **no spaces** (GNU make / devkitPro requirement).

| Test | Studio / PC | Hardware (optional) | Pass? |
|------|-------------|---------------------|-------|
| Build `.3dsx` | required | | **pass** (2026-08-25) |
| Build CIA (or clear error if tools missing) | required | | **pass** (2026-08-25) |
| Two projects → different `APP_UNIQUE_ID` + save prefix | required | | **pass** (space-free named saves) |
| New Game save slot | | optional | |
| Load Game slot | | optional | |
| Settings persist (sprint/easy) | | optional | |
| SFX (with `dspfirm.cdc`) | | optional | |
| Music (if MP3 imported) | | optional | |
| 3dslink | | optional | |
| CIA via SD or FTP + FBI | | optional | |

---

## Acceptance criteria

- Studio builds against a non-default toolchain path you configure once
- `.3dsx` / CIA (or clear missing-tool errors) work from Studio
- Two projects can produce CIA / write saves without clobbering each other
- Install/play docs cover SD copy, FTP, and 3dslink (`docs/PLAYING.md`)
- No shell injection via project path or IP field (basic review)

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P1-001 | 2026-08-25 | 1.6 | Emulator smoke test was planned as a gate | wontfix | Dropped; Studio ships builds + `docs/PLAYING.md` only |
| P1-002 | 2026-08-25 | 1.4.2 | Template soundtrack rights not independently verified | deferred | Optional import only; no uncleared music in public Release |
| P1-003 | 2026-08-25 | 1.1 | UI to browse/set toolchain path deferred to Phase 2 wizard; `set_toolchain_path` IPC ready | deferred | Phase 2 |
| P1-004 | 2026-08-25 | 1.1 / build | `DEVKITPRO=/opt/devkitpro` broke Windows builds | fixed | Ignore Unix env; use `C:\devkitPro` |
| P1-005 | 2026-08-25 | 1.6 | Spaces in `Documents/3DS Studio/...` broke make (`sprites.t3x`) | fixed | Library = `Documents/3DSStudio`; ban spaces in project names |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | Save prefix = slug of title (stable in `studio_meta.json`) | Readable on SD; unique per project without PII |
| 2026-08-25 | Soundtrack optional; music skip if missing | Engine already no-ops; document in `docs/PLAYING.md` |
| 2026-08-25 | Title ID = `0xF2000`–`0xFEFFF` random, stored in Makefile + `studio_meta.json` | Avoid stock `0xF1234` collisions |
| 2026-08-25 | Toolchain config = `%APPDATA%\3ds-studio\config.json` | Per-user, not in game projects |
| 2026-08-25 | No emulator dependency for Studio development or Phase 1 exit | App builds artifacts; users install via SD / FTP / 3dslink |
| 2026-08-25 | Project library under `Documents/3DSStudio` without spaces | GNU make cannot handle spaces in project paths |

---

## Exit checklist

- [x] Regression matrix: Studio **required** rows filled (Build 3dsx, CIA, two-project path/names)
- [x] Play/install docs: `docs/PLAYING.md` (SD, FTP, 3dslink, dspfirm, saves)
- [x] Template and any canonical example project both updated
- [x] Problem log: no open blockers (P1-001 wontfix; P1-002/003 deferred; P1-004/005 fixed)
- [x] Ready for Phase 2 on code (wizard can use `set_toolchain_path`)
