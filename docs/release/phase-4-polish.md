# Phase 4 — Public polish and documentation

**Status:** complete  
**Owner:**  
**Started:** 2026-08-25  
**Finished:** 2026-08-25  

## Goal

Repo and Release look intentional: accurate docs, screenshots, legal/privacy pages, troubleshooting that matches the shipped installer. No filler.

## Out of scope

- Posting publicly / social threads (Phase 5)
- New major features (bugfixes from Problem logs only)

## Prerequisites

- Phase 3 Exit complete (docs must describe the real installer + wizard)

---

## Legal / privacy / safety (this phase)

### Required documents

| File | Purpose |
|------|---------|
| `LICENSE` | Phase 0; linked from README |
| `THIRD_PARTY.md` | Attribution |
| `docs/LEGAL.md` | Homebrew disclaimer |
| `docs/PRIVACY.md` | No telemetry; local + LAN only |
| `docs/SECURITY.md` | GitHub Security Advisories |

### Content rules

- [x] **4.L.1**–**4.L.7** followed in shipped docs

### Privacy checklist

| Question | Answer |
|----------|--------|
| Does Studio send telemetry? | **No** |
| Does Studio check for updates automatically? | **No** |
| What is written under AppData? | `%APPDATA%\3ds-studio\config.json`; projects in `Documents\3DSStudio\` |
| What network destinations can it hit? | Optional LAN 3dslink; allowlisted https docs; installer may fetch WebView2 bootstrapper |
| Are crash reports uploaded? | **No** |

---

## Tasks

### 4.1–4.6

Documentation set, legal pages, CHANGELOG, issue templates, Help sync, architecture/PLATFORM refresh — **done**.

### 4.2 Media

- [ ] **4.2.1** User still needs to drop PNGs into `docs/images/` (see CAPTURE.md) before public launch
- [x] **4.2.2** Checklist + README gallery wired
- [x] **4.2.3** Demo video skipped

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P4-001 | 2026-08-25 | 4.2 | Screenshots need manual capture | deferred | Drop PNGs per `docs/images/CAPTURE.md` before Phase 5 public |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | No emulator recommendation | Hardware-first |
| 2026-08-25 | No demo video for v0.1 | Docs + screenshots enough |
| 2026-08-25 | Issues open with bug/toolchain templates | Private repo OK |
| 2026-08-25 | Security via GitHub Advisories | No separate email |
| 2026-08-25 | Privacy: no telemetry / auto-update / crash upload | Matches app |

### GitHub About (apply in Settings)

- **Description:** `Windows editor for homebrew 3DS platformers (Tauri). Builds .3dsx/.cia; needs devkitPro.`
- **Topics:** `3ds`, `homebrew`, `tauri`, `nintendo-3ds`, `devkitpro`, `platformer`

---

## Exit checklist

- [x] Docs written end-to-end (user should skim once on a clean install)
- [x] Legal/privacy tables filled
- [ ] Screenshots committed (deferred P4-001)
- [x] No open blockers for starting Phase 5 prep (capture screenshots first if going public)
- [x] Ready for Phase 5 (after screenshot drop recommended)
