# Phase 2 — Non-technical UX

**Status:** not started  
**Owner:**  
**Started:**  
**Finished:**  

## Goal

A person who can install apps but not debug toolchains can create a project, understand missing tools, get help in-app, and know how to play the build.

## Out of scope

- NSIS/MSI packaging (Phase 3)
- Full docs site polish (Phase 4 writes the long guides; this phase wires in-app surfaces and stubs)
- Social announce (Phase 5)

## Prerequisites

- Phase 1 Exit complete (toolchain resolution + Citra fixes exist to teach)

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| External links | Wizard “Download devkitPro / Citra” must open **https** official URLs only; no random mirrors |
| No silent downloads | Do not auto-download toolchains or emulators without explicit user action |
| No account system | Do not add login “for convenience” in this phase |
| Help content | No instructions for piracy, game dumping of commercial titles, or bypassing paid content |
| CFW | If mentioned, state risks in plain language; do not walk through exploit chains |
| Telemetry | Help/wizard must not phone home. If you add “check for updates”, make it explicit and optional (prefer Phase 3/5) |

---

## Tasks

### 2.1 First-run / toolchain wizard

- [ ] **2.1.1** On launch or first Build: if toolchain incomplete, show modal/wizard (not only console text)
- [ ] **2.1.2** Steps: Detect → What’s missing → Open install docs / link → Browse for DEVKITPRO → Retest
- [ ] **2.1.3** Persist chosen path (uses Phase 1 config)
- [ ] **2.1.4** Distinguish “Studio can run” vs “Build available”
- [ ] **2.1.5** Never block opening/editing projects when toolchain is missing

### 2.2 Welcome screen

- [ ] **2.2.1** Actions: New Project, Open Project, Open Sample (if sample path exists), Help
- [ ] **2.2.2** Remove reliance on “Open My Platformer” hardcoded examples path
- [ ] **2.2.3** Show last 3 recent projects (paths only; local)
- [ ] **2.2.4** Toolchain status chip: OK / Missing tools / Path custom

### 2.3 In-app Help

- [ ] **2.3.1** Help panel or window with sections: Editing, Assets, Build, Citra play, CIA, Controls, Troubleshooting
- [ ] **2.3.2** Content can be static HTML/Markdown shipped in app resources (same text later reused in `docs/`)
- [ ] **2.3.3** Include Citra `dspfirm.cdc` steps and save-slot notes
- [ ] **2.3.4** Include Easy Controls / Sprint toggle pointers
- [ ] **2.3.5** “Copy build output path” after successful build

### 2.4 Contextual UX (light)

- [ ] **2.4.1** Tooltips on editor tools that confuse people (3D tile, warp, checkpoint, budget limits)
- [ ] **2.4.2** Asset panel: warn if soundtrack missing before Build
- [ ] **2.4.3** CIA build: warn if bannertool/makerom missing with link to Help
- [ ] **2.4.4** Build log: human one-liner on failure (e.g. “MSYS2 bash not found”) above raw make output

### 2.5 Sample project

- [ ] **2.5.1** Decide: sample inside template defaults vs separate `samples/starter` zip
- [ ] **2.5.2** Sample must build clean on Phase 1 toolchain resolution
- [ ] **2.5.3** Sample includes short pre-level dialogue teaching move/jump only
- [ ] **2.5.4** Early levels: advanced moves disabled (use existing per-level toggles)

### 2.6 Accessibility / clarity (minimum)

- [ ] **2.6.1** Critical buttons have visible labels (not icon-only)
- [ ] **2.6.2** Error text contrast readable on dark/light UI as applicable
- [ ] **2.6.3** Do not trap focus in wizard without Esc/Cancel

### 2.7 Dead UI cleanup

- [ ] **2.7.1** Remove or hide buttons that do nothing
- [ ] **2.7.2** Remove debug leftovers from welcome/settings if any
- [ ] **2.7.3** Align menu copy with real behavior (no “Run in Citra” until implemented)

### 2.8 Optional: Citra launch

- [ ] **2.8.1** If time: detect Citra install; “Open in Citra” after build
- [ ] **2.8.2** If deferred: Problem log + Help says “File → Load ROM” manually

---

## Acceptance criteria

- Fresh user mental model: edit without toolchain; build only after wizard succeeds
- Help covers Citra audio + saves without needing Discord
- No hardcoded personal project path on welcome
- Sample or template teaches basic controls via dialogue/toggles

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P2-001 | | | | open | |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| | Wizard blocking vs advisory | |
| | Sample location | |
| | Citra launch now or defer | |
| | Help format (HTML vs MD render) | |

---

## Exit checklist

- [ ] Manual dry-run: uninstall toolchain path temporarily → wizard recovers
- [ ] Dry-run: new user opens Help and finds Citra audio steps in under one minute
- [ ] Problem log: no open blockers
- [ ] Ready for Phase 3
