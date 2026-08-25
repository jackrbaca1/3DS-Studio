# Phase 2 — Non-technical UX

**Status:** complete  
**Owner:**  
**Started:** 2026-08-25  
**Finished:** 2026-08-25  

## Goal

A person who can install apps but not debug toolchains can create a project, understand missing tools, get help in-app, and know how to play the build.

## Out of scope

- NSIS/MSI packaging (Phase 3)
- Full docs site polish (Phase 4 writes the long guides; this phase wires in-app surfaces and stubs)
- Social announce (Phase 5)

## Prerequisites

- Phase 1 Exit complete (toolchain resolution + save/audio path fixes exist to teach)

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| External links | Wizard “Download devkitPro” must open **https** official URLs only; no random mirrors |
| No silent downloads | Do not auto-download toolchains or emulators without explicit user action |
| No account system | Do not add login “for convenience” in this phase |
| Help content | No instructions for piracy, game dumping of commercial titles, or bypassing paid content |
| CFW | If mentioned, state risks in plain language; do not walk through exploit chains |
| Telemetry | Help/wizard must not phone home. If you add “check for updates”, make it explicit and optional (prefer Phase 3/5) |

---

## Tasks

### 2.1 First-run / toolchain wizard

- [x] **2.1.1** On launch or first Build: if toolchain incomplete, show modal/wizard (not only console text)
- [x] **2.1.2** Steps: Detect → What’s missing → Open install docs / link → Browse for DEVKITPRO → Retest
- [x] **2.1.3** Persist chosen path (uses Phase 1 config)
- [x] **2.1.4** Distinguish “Studio can run” vs “Build available”
- [x] **2.1.5** Never block opening/editing projects when toolchain is missing

### 2.2 Welcome screen

- [x] **2.2.1** Actions: New Project, Open Folder, Start Fresh Example, Help (+ project list)
- [x] **2.2.2** Remove reliance on “Open My Platformer” hardcoded examples path
- [x] **2.2.3** Show recent / library projects (paths only; local)
- [x] **2.2.4** Toolchain status chip: OK / Missing tools / Path custom

### 2.3 In-app Help

- [x] **2.3.1** Help panel or window with sections: Editing, Assets, Build, Playing on 3DS (SD / FTP / 3dslink), CIA, Controls, Troubleshooting
- [x] **2.3.2** Content can be static HTML/Markdown shipped in app resources (same text later reused in `docs/`)
- [x] **2.3.3** Include `dspfirm.cdc` dump steps and save-slot notes from `docs/PLAYING.md`
- [x] **2.3.4** Include Easy Controls / Sprint toggle pointers
- [x] **2.3.5** “Copy build output path” after successful build

### 2.4 Contextual UX (light)

- [x] **2.4.1** Tooltips on editor tools that confuse people (3D tile, warp, checkpoint, budget limits)
- [x] **2.4.2** Asset panel: warn if soundtrack missing before Build
- [x] **2.4.3** CIA build: warn if bannertool/makerom missing with link to Help
- [x] **2.4.4** Build log: human one-liner on failure (e.g. “MSYS2 bash not found”) above raw make output

### 2.5 Sample project

- [x] **2.5.1** Sample = fresh template defaults + starter levels (not a separate zip)
- [x] **2.5.2** Sample must build clean on Phase 1 toolchain resolution
- [x] **2.5.3** Sample includes short pre-level dialogue teaching move/jump only
- [x] **2.5.4** Early levels: advanced moves disabled (use existing per-level toggles)

### 2.6 Accessibility / clarity (minimum)

- [x] **2.6.1** Critical buttons have visible labels (not icon-only)
- [x] **2.6.2** Error text contrast readable on dark/light UI as applicable
- [x] **2.6.3** Do not trap focus in wizard without Esc/Cancel

### 2.7 Dead UI cleanup

- [x] **2.7.1** Remove or hide buttons that do nothing
- [x] **2.7.2** Remove debug leftovers from welcome/settings if any
- [x] **2.7.3** Align menu copy with real behavior (build + 3dslink only; no emulator launch)

### 2.8 Playing on hardware (docs in Help)

- [x] **2.8.1** Help links to SD copy, FTP, and 3dslink steps (`docs/PLAYING.md`)
- [x] **2.8.2** Do **not** add emulator detect/launch features

---

## Acceptance criteria

- Fresh user mental model: edit without toolchain; build only after wizard succeeds
- Help covers hardware install (SD / FTP / 3dslink) + `dspfirm` / saves without needing Discord
- No hardcoded personal project path on welcome
- Sample or template teaches basic controls via dialogue/toggles

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P2-001 | 2026-08-25 | 2.1 | Browse path ignored when `C:\devkitPro` still usable | closed | Config path always preferred when set |
| P2-002 | 2026-08-25 | 2.2 | Help/Setup overlays behind welcome (z-index) | closed | Modal z-index above welcome |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | Wizard is **advisory** (never blocks edit) | Matches 2.1.5; build can still prompt |
| 2026-08-25 | Sample = template + starter levels in library | Already shipped in Phase 1 |
| 2026-08-25 | No emulator launch in Studio | Builds + install docs only |
| 2026-08-25 | Help = in-app HTML panel (static sections) | No MD renderer dependency yet |
| 2026-08-25 | External links only via allowlisted https opener | Phase 2 legal table |
| 2026-08-25 | Saved DEVKITPRO path wins even if invalid | Wizard can show bad picks; Retest turns red |

---

## Exit checklist

- [x] Manual dry-run: uninstall toolchain path temporarily → wizard recovers
- [x] Dry-run: new user opens Help and finds 3DS install / audio steps in under one minute
- [x] Problem log: no open blockers
- [x] Ready for Phase 3
