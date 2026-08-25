# Phase 1 — Portability and runtime reliability

**Status:** not started  
**Owner:**  
**Started:**  
**Finished:**  

## Goal

Studio and games work on machines that are not yours: configurable toolchain, Citra-compatible saves/audio setup, no colliding CIA IDs or save filenames across projects.

## Out of scope

- Full setup wizard UI polish (Phase 2 builds on this)
- Installer (Phase 3)
- Marketing docs (Phase 4)

## Prerequisites

- Phase 0 Exit complete
- Access to Citra (or fork) for save/audio verification
- Real 3DS optional but useful for CIA/save smoke test

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| DSP firmware | Never commit or redistribute `dspfirm.cdc` from a console dump. Document: Citra may use empty stub for HLE; real hardware requires user dump via Luma |
| Network | `3dslink` sends `.3dsx` to a user-entered IP on LAN. No cloud. Document that IP is local-only; validate input (no shell injection) |
| Path traversal | Toolchain path + project path: reject `..` tricks when spawning bash/make |
| Command injection | Build commands must not concatenate unsanitized user strings into shell beyond quoted paths |
| Unique IDs | Random/hashed per-project IDs reduce collision; do not embed PII in IDs |
| Save data | Saves are local progress only; no accounts. Document location on SD/Citra sdmc |

---

## Tasks

### 1.1 Configurable toolchain

- [ ] **1.1.1** Define resolution order: env `DEVKITPRO` → saved Studio setting → `C:\devkitPro`
- [ ] **1.1.2** Replace hardcoded paths in `src-tauri/src/lib.rs` (`msys2_bash`, `arm-none-eabi-gcc`, `3dslink`, `makerom`, `bannertool`)
- [ ] **1.1.3** Persist toolchain root in a small config file under `%APPDATA%\3ds-studio\` (or Tauri app config dir) — not in the game project
- [ ] **1.1.4** Update `check_toolchain` to report which path was used and what is missing (gcc, bash, makerom, bannertool, 3dslink)
- [ ] **1.1.5** Remove or replace hardcoded `DEFAULT_PLATFORMER` in `src/main.js`; use last-opened path or empty
- [ ] **1.1.6** Security: normalize paths; refuse to run make if project path looks unsafe

### 1.2 Save / settings paths (game)

- [ ] **1.2.1** Change settings + slot paths to `sdmc:/3ds/...` form
- [ ] **1.2.2** On first save, create `sdmc:/3ds` if missing (`mkdir` / equivalent)
- [ ] **1.2.3** Derive filename prefix from app title or project id (e.g. `sdmc:/3ds/<slug>_slot1.dat`) so games do not overwrite each other
- [ ] **1.2.4** Migrate or document: old `/3ds/platformer_slot*.dat` ignored on Citra; users may start fresh
- [ ] **1.2.5** Apply same changes to live platformer project **and** `src-tauri/template`
- [ ] **1.2.6** Verify New Game → quit → Load on Citra

### 1.3 CIA unique ID

- [ ] **1.3.1** Stop shipping one shared `APP_UNIQUE_ID` for every user game
- [ ] **1.3.2** Generate stable per-project ID on New Project (store in project metadata / Makefile var)
- [ ] **1.3.3** Document range used for homebrew title IDs and collision risk if users fork templates blindly
- [ ] **1.3.4** Ensure Studio rewrite of Makefile / rsf keeps ID on rebuild

### 1.4 Audio reliability

- [ ] **1.4.1** Document Citra requirement: `sdmc:/3ds/dspfirm.cdc` (empty file OK for Citra HLE)
- [ ] **1.4.2** Template: either ship a short placeholder `romfs/soundtrack.mp3` **you have rights to**, or make music optional with clear Studio warning when missing
- [ ] **1.4.3** Confirm SFX still play with dspfirm present and soundtrack absent
- [ ] **1.4.4** Check mute settings do not leave NDSP in a bad state

### 1.5 Build tool discovery

- [ ] **1.5.1** Clear errors when `makerom` / `bannertool` missing (CIA path)
- [ ] **1.5.2** Detect `banner.wav` / prebuilt `.icn`/`.bnr` paths; keep auto-silent-wav behavior if already present
- [ ] **1.5.3** Log resolved DEVKITPRO at start of each build

### 1.6 Regression matrix

| Test | Real 3DS | Citra | Pass? |
|------|----------|-------|-------|
| New Game save slot | | | |
| Load Game slot | | | |
| Settings persist (sprint/easy) | | | |
| SFX | | | |
| Music (if file present) | | | |
| Build 3dsx | | | |
| Build CIA | | | |
| 3dslink (optional) | | | |

---

## Acceptance criteria

- Studio builds against a non-default toolchain path you configure once
- Citra: save slots round-trip; audio works with documented dspfirm step
- Two projects can install CIA / write saves without clobbering each other
- No shell injection via project path or IP field (basic review)

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P1-001 | | | | open | |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| | Save prefix scheme (slug vs uuid) | |
| | Ship placeholder MP3 vs warn-only | |
| | Title ID generation method | |
| | Config file location for toolchain | |

---

## Exit checklist

- [ ] Regression matrix filled (Citra required; real hardware if available)
- [ ] Template and any canonical example project both updated
- [ ] Problem log: no open blockers
- [ ] Ready for Phase 2
