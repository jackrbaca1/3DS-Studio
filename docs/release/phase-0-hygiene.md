# Phase 0 — Hygiene

**Status:** complete  
**Owner:**  
**Started:** 2026-08-25  
**Finished:** 2026-08-25  

## Goal

Make the repository safe and honest to publish: correct identity, license, no junk files, no personal paths, no accidental secrets. Nothing user-facing yet.

## Out of scope

- Toolchain path UI (Phase 1)
- Installer (Phase 3)
- Public announce (Phase 5)

## Prerequisites

- Local clone builds with `npm run dev`
- List of third-party code already in-tree (`minimp3`, Tauri, citro/devkit examples lineage)

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| Copyright ownership | Confirm you own Studio code + template game code you wrote |
| Third-party code | Inventory licenses (MIT/Apache for Tauri deps; minimp3 license file; any copied snippets) |
| Nintendo IP | Do not claim affiliation. Do not ship Nintendo ROMs, firmware, or dumped DSP firmware. Trademarks: “Nintendo” / “3DS” only in factual descriptive use |
| Homebrew legality | Document that building/running homebrew may require user-owned hardware and user-installed CFW; you do not distribute exploits or piracy tools |
| Secrets | Scan for API keys, tokens, private keys, absolute personal paths, OneDrive usernames in committed files |
| Privacy baseline | Confirm app currently does **no** analytics/telemetry/crash upload (verify in code). If true, state that in LICENSE/README later; if false, stop and redesign before Phase 4 |
| Git history | If secrets were ever committed, plan history rewrite or key rotation before public |

---

## Tasks

### 0.1 Identity

- [x] **0.1.1** Rename npm package from `tauri-app` to something stable (e.g. `3ds-studio`) in `package.json`
- [x] **0.1.2** Align `src-tauri/Cargo.toml` package name/description with product name “3DS Studio”
- [x] **0.1.3** Confirm `tauri.conf.json` `productName`, `identifier`, `version` are intentional for v0.1.0
- [x] **0.1.4** Grep for leftover “tauri-app”, placeholder author strings, fake emails

### 0.2 License and attribution

- [x] **0.2.1** Add root `LICENSE` (choose one; MIT is typical for this stack — record choice in Decisions)
- [x] **0.2.2** Add `NOTICE` or `THIRD_PARTY.md` listing: Tauri, Rust crates (cargo license summary), minimp3, any other vendored files
- [x] **0.2.3** Clarify in NOTICE: template game vs editor vs toolchain (toolchain is **not** redistributed)
- [x] **0.2.4** Remove or rewrite template README “public domain” claim if it conflicts with root LICENSE

### 0.3 Delete / quarantine junk

- [x] **0.3.1** Delete `src-tauri/template/source/main.cpp.crswap` (and any other `*.crswap`)
- [x] **0.3.2** Search for editor swap/backup files (`*~`, `*.bak`, `*.orig`)
- [x] **0.3.3** Decide fate of `implementation_plan.md`: delete, or move to `docs/architecture.md` after scrubbing `file:///c:/...` personal links
- [x] **0.3.4** Confirm `src-tauri/target/` and `node_modules/` are not tracked

### 0.4 Gitignore and ignore hygiene

- [x] **0.4.1** Root `.gitignore`: add `*.crswap`, `*.bak`, `.DS_Store`, OS junk already partial
- [x] **0.4.2** Ignore local env files if any (`*.env`, `.env.*`)
- [x] **0.4.3** Ignore installer/output dirs if build ever writes into repo (`src-tauri/target`, cargo target outside tree is fine)
- [x] **0.4.4** Run `git status` / `git check-ignore -v` on suspicious paths

### 0.5 Path and secret scrub

- [x] **0.5.1** Grep repo for `C:\\Users\\`, `OneDrive`, `JACKR`, other personal usernames
- [x] **0.5.2** Replace personal absolute paths in docs with placeholders (`C:\\devkitPro\\...` as documented default is OK; home directories are not)
- [x] **0.5.3** Grep for `password`, `api_key`, `secret`, `token`, private key headers
- [x] **0.5.4** Confirm no `.cia` / large binaries that you did not intend to version (optional LFS later)

### 0.6 README honesty pass (minimal)

- [x] **0.6.1** Root README: accurate requirements; no “zero install” claims
- [x] **0.6.2** Mark incomplete features clearly or remove promises (Citra launch, custom toolchain UI)
- [x] **0.6.3** Do **not** write marketing fluff yet (Phase 4)

### 0.7 Dependency snapshot

- [x] **0.7.1** Record Node, Rust, Tauri major versions used
- [x] **0.7.2** Note known Windows-only constraints in a short `docs/PLATFORM.md` (one page)

---

## Acceptance criteria

- Clean `git status` with no swap files or personal home paths in tracked content
- Root `LICENSE` + third-party attribution present
- Package/product names consistent
- Verified: no telemetry endpoints in Studio source (or documented if any exist)
- Nintendo/homebrew disclaimer draft exists (can live in NOTICE or short LEGAL.md)

---

## Problem log

Record issues while doing Phase 0. Do not delete rows.

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P0-001 | 2026-08-25 | 0.3 / exit | No `.git` directory existed | fixed | `git init`, branch renamed to `main`. No initial commit yet (wait for explicit ask). |
| P0-002 | 2026-08-25 | 0.5 | `DEFAULT_PLATFORMER` still hardcoded to `C:\devkitPro\examples\...` in `src/main.js` | deferred | Phase 1 (last-opened / no hard default) |
| P0-003 | 2026-08-25 | 0.6 | `tauri.conf.json` still has `"csp": null` | deferred | Phase 3 security pass |
| P0-004 | 2026-08-25 | 0.2.4 | Template README still describes an old “basic” feature set | deferred | Phase 4 full rewrite; license + CIA lines fixed now |
| P0-005 | 2026-08-25 | 0.5 | `src-tauri/target/.rustc_info.json` contains local rustup path with username | fixed | Ignored via `/target/`; not for commit |
| P0-006 | 2026-08-25 | 0.1 | Renaming Cargo package requires lockfile regen | fixed | `cargo generate-lockfile`; `cargo check` OK |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | License: MIT | Matches Tauri ecosystem; simple for a student/OSS tool |
| 2026-08-25 | Replace `implementation_plan.md` with short `docs/architecture.md` | Remove personal `file:///` links and outdated UI mockups |
| 2026-08-25 | Keep identifier `com.studio3ds.platformer` | Already set in `tauri.conf.json` |
| 2026-08-25 | npm name `3ds-studio`; Cargo package `studio3ds` / lib `studio3ds_lib` | Clear product id; valid Rust crate name |
| 2026-08-25 | Attribution file named `THIRD_PARTY.md` | Clearer than NOTICE for this repo |

---

## Exit checklist

- [x] All tasks above done or explicitly deferred with a Problem log row
- [x] Problem log has no `open` blockers for Phase 1
- [ ] Commit(s) pushed to a private or draft branch (public optional) — **git init done; commit when you ask**
- [x] Ready for Phase 1
