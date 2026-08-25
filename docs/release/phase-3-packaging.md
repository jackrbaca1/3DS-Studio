# Phase 3 — Packaging and distribution artifacts

**Status:** not started  
**Owner:**  
**Started:**  
**Finished:**  

## Goal

Ship a Windows installer (and companion game artifacts) that installs Studio without requiring Node/Rust for end users. Publish versioned GitHub Release assets.

## Out of scope

- Full website (Phase 4/5)
- Auto-update server (optional note only)
- Bundling full devkitPro inside the installer

## Prerequisites

- Phase 2 Exit complete (installer should include Help/wizard)
- `npm run tauri:build` proven on a clean PATH session
- GitHub repo exists (can still be private until Phase 5)

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

- [ ] **3.1.1** Confirm `tauri.conf.json` bundle `active`, icons exist and look correct
- [ ] **3.1.2** Set version once for `package.json`, `Cargo.toml`, `tauri.conf.json` (semver)
- [ ] **3.1.3** Bundle targets: at least NSIS **or** MSI for Windows x64; record which in Decisions
- [ ] **3.1.4** Ensure `resources: ["template/"]` packs a complete buildable template (no missing gfx)
- [ ] **3.1.5** CSP: replace `csp: null` with a minimal CSP appropriate for local Tauri UI (no remote script)
- [ ] **3.1.6** Review Tauri capabilities/permissions: least privilege (dialog, fs scoped if possible)

### 3.2 Produce installer

- [ ] **3.2.1** Run `npm run tauri:build` via `scripts/tauri-dev.ps1 build`
- [ ] **3.2.2** Locate artifact under cargo target / `bundle/nsis` or `msi`
- [ ] **3.2.3** Install on a **second** Windows profile or VM with no Rust/Node
- [ ] **3.2.4** Verify New Project creates files; editor loads; Help opens
- [ ] **3.2.5** Verify Build still requires user-installed devkitPro (expected)

### 3.3 Companion artifacts

- [ ] **3.3.1** Build sample/demo `.3dsx` for graders/players
- [ ] **3.3.2** Optional: demo `.cia` (warn about unique ID / install)
- [ ] **3.3.3** Zip sample project folder (source + gfx) for “open in Studio”
- [ ] **3.3.4** Generate SHA256 for each release file; store as `SHA256SUMS.txt`

### 3.4 GitHub Release pipeline

- [ ] **3.4.1** Decide tag scheme (`v0.1.0`)
- [ ] **3.4.2** Write release notes template: changes, requirements, Citra notes, checksums
- [ ] **3.4.3** Upload: installer, `.3dsx`, project zip, `SHA256SUMS.txt`, `NOTICE`/`LICENSE`
- [ ] **3.4.4** Optional CI: workflow on tag that builds installer (document secrets: signing cert if any)
- [ ] **3.4.5** Never put signing certs or GitHub tokens in the repo

### 3.5 Installer UX copy

- [ ] **3.5.1** Start Menu shortcut name: “3DS Studio”
- [ ] **3.5.2** Uninstall entry works; removes app (app data may remain — document)
- [ ] **3.5.3** License agreement screen shows LICENSE text if NSIS supports it

### 3.6 Security pass on distributed binary

- [ ] **3.6.1** Confirm no debug assertions leaking secrets
- [ ] **3.6.2** Confirm build log does not embed your machine username in shipped template
- [ ] **3.6.3** Strings scan optional: no accidental home paths in binary resources

### 3.7 WebView2

- [ ] **3.7.1** Document WebView2 Runtime requirement (usually preinstalled on Win10/11)
- [ ] **3.7.2** If installer can bootstrap WebView2, enable; else link Evergreen bootstrapper in Help/README

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
| P3-001 | | | | open | |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| | NSIS vs MSI vs both | |
| | Code signing yes/no | |
| | CI release yes/no for v0.1 | |
| | Version number for first public tag | |

---

## Exit checklist

- [ ] Installer smoke-tested on clean machine/VM
- [ ] Draft GitHub Release prepared (can stay unpublished until Phase 5)
- [ ] Problem log: no open blockers
- [ ] Ready for Phase 4
