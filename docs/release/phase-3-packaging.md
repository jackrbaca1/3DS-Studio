# Phase 3 — Packaging and distribution artifacts

**Status:** complete  
**Owner:**  
**Started:** 2026-08-25  
**Finished:** 2026-08-25  

## Goal

Ship a Windows installer (and companion game artifacts) that installs Studio without requiring Node/Rust for end users. Publish versioned GitHub Release assets.

## Out of scope

- Full website (Phase 4/5)
- Auto-update server (optional note only)
- Bundling full devkitPro inside the installer

## Prerequisites

- Phase 2 Exit complete (installer should include Help/wizard)
- `npm run tauri:build` proven on a clean PATH session
- GitHub repo exists (can still be private until Phase 5) — **deferred:** artifacts prepared locally; upload when `origin` exists

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| What the installer contains | Studio binaries + bundled `template/` only. No Nintendo SDK, no dspfirm dump, no commercial ROMs |
| Code signing | Unsigned Windows installs trigger SmartScreen. Prefer signing if you have a cert; if not, document “Windows protected your PC → More info → Run anyway” and expect friction |
| Installer privileges | Prefer per-user install (no admin) unless required; document UAC if admin needed |
| Telemetry in installer | Disable any third-party installer analytics. Tauri/NSIS: do not add tracking |
| Update checks | If none, say so. Do not silently download executables |
| Release integrity | Publish SHA256 checksums for installer + `.3dsx` |
| Malware false positives | Be ready to submit installer to Microsoft / VirusTotal for awareness; never pack cracked tools |
| Dependency licenses | Installer about/NOTICE accessible or in install dir |
| Privacy | Installer creates local app data only; no account |

---

## Tasks

### 3.1 Build configuration

- [x] **3.1.1** Confirm `tauri.conf.json` bundle `active`, icons exist and look correct
- [x] **3.1.2** Set version once for `package.json`, `Cargo.toml`, `tauri.conf.json` (semver) — `0.1.0`
- [x] **3.1.3** Bundle targets: NSIS only; recorded in Decisions
- [x] **3.1.4** Ensure `resources: ["template/"]` packs a complete buildable template (no missing gfx)
- [x] **3.1.5** CSP: replace `csp: null` with a minimal CSP appropriate for local Tauri UI (no remote script)
- [x] **3.1.6** Review Tauri capabilities/permissions: least privilege (dialog, fs scoped if possible)

### 3.2 Produce installer

- [x] **3.2.1** Run `npm run tauri:build` via `scripts/tauri-dev.ps1 build`
- [x] **3.2.2** Locate artifact: `%LOCALAPPDATA%\3ds-studio-cargo-target\release\bundle\nsis\3DS Studio_0.1.0_x64-setup.exe`
- [x] **3.2.3** Silent install smoke on build machine (`/S` → `%LOCALAPPDATA%\3DS Studio\`); second profile/VM still recommended before public launch
- [x] **3.2.4** Verify launch + bundled `template/gfx` present after install
- [x] **3.2.5** Verify Build still requires user-installed devkitPro (expected; documented)

### 3.3 Companion artifacts

- [x] **3.3.1** Build sample/demo `.3dsx` for graders/players
- [x] **3.3.2** Optional demo `.cia` — skipped for v0.1 (script supports `-AlsoCia`)
- [x] **3.3.3** Zip sample project folder (source + gfx) for “open in Studio”
- [x] **3.3.4** Generate SHA256 for each release file; store as `SHA256SUMS.txt`

### 3.4 GitHub Release pipeline

- [x] **3.4.1** Decide tag scheme (`v0.1.0`)
- [x] **3.4.2** Write release notes template: [RELEASE_NOTES_v0.1.0.md](RELEASE_NOTES_v0.1.0.md)
- [x] **3.4.3** Upload deferred until git remote exists; local draft assets in `dist/release/v0.1.0/`
- [x] **3.4.4** Optional CI: skipped for v0.1
- [x] **3.4.5** Never put signing certs or GitHub tokens in the repo

### 3.5 Installer UX copy

- [x] **3.5.1** Start Menu shortcut name: “3DS Studio”
- [x] **3.5.2** Uninstall works (`uninstall.exe /S`); `%APPDATA%\3ds-studio` may remain — documented in README
- [x] **3.5.3** License via `bundle.licenseFile` → `../LICENSE`

### 3.6 Security pass on distributed binary

- [x] **3.6.1** Confirm no debug assertions leaking secrets (release build)
- [x] **3.6.2** Confirm shipped template has no machine username paths
- [x] **3.6.3** Strings scan: no accidental home paths in template resources

### 3.7 WebView2

- [x] **3.7.1** Document WebView2 Runtime requirement in README
- [x] **3.7.2** `webviewInstallMode: downloadBootstrapper` enabled

---

## Acceptance criteria

- Clean PC: install Studio → edit → (with toolchain) build works
- Clean PC: no toolchain → edit + Help + wizard work; Build explains next step
- Release assets have checksums
- CSP and permissions tightened vs current `csp: null`

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P3-001 | 2026-08-25 | 3.4 | No git remote yet | deferred | Local `dist/release/v0.1.0/` ready; upload when origin exists |
| P3-002 | 2026-08-25 | 3.3 | `$USERPROFILE\Documents` missed OneDrive library | fixed | Scripts use `[Environment]::GetFolderPath('MyDocuments')` |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | NSIS only (not MSI / both) | Simpler Windows x64 path; per-user friendly |
| 2026-08-25 | Code signing: **no** for v0.1 | No cert yet; document SmartScreen |
| 2026-08-25 | CI release: **no** for v0.1 | Manual `tauri:build` + `release:package` |
| 2026-08-25 | Version / tag: **0.1.0** / `v0.1.0` | Matches package.json / Cargo / tauri.conf |
| 2026-08-25 | NSIS `installMode: currentUser` | No admin; install under LocalAppData |
| 2026-08-25 | WebView2: `downloadBootstrapper` | Small installer; fetches runtime if missing |

---

## Exit checklist

- [x] Installer smoke-tested (silent install/launch/uninstall on build machine; second profile/VM recommended before Phase 5)
- [x] Draft GitHub Release assets prepared locally (upload when remote exists; can stay unpublished until Phase 5)
- [x] Problem log: no open blockers (P3-001 deferred, not blocking Phase 4)
- [x] Ready for Phase 4

## Local release layout

`dist/release/v0.1.0/` (gitignored):

- `3DS Studio_0.1.0_x64-setup.exe`
- `3DSStudio-ExamplePlatformer-v0.1.0.3dsx`
- `3DSStudio-ExamplePlatformer-v0.1.0-project.zip`
- `LICENSE`, `THIRD_PARTY.md`, `RELEASE_NOTES.md`, `SHA256SUMS.txt`
