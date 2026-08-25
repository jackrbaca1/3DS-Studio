# Phase 4 — Public polish and documentation

**Status:** not started  
**Owner:**  
**Started:**  
**Finished:**  

## Goal

Repo and Release look intentional: accurate docs, screenshots, legal/privacy pages, troubleshooting that matches the shipped installer. No filler.

## Out of scope

- Posting publicly / social threads (Phase 5)
- New major features (bugfixes from Problem logs only)

## Prerequisites

- Phase 3 Exit complete (docs must describe the real installer + wizard)

---

## Legal / privacy / safety (this phase)

Complete these as committed files, not vibes.

### Required documents

| File | Purpose |
|------|---------|
| `LICENSE` | Already from Phase 0; linked from README |
| `NOTICE` / `THIRD_PARTY.md` | Attribution |
| `docs/LEGAL.md` | Homebrew disclaimer, no Nintendo affiliation, no warranty, user responsibility for CFW/device |
| `docs/PRIVACY.md` | What data is collected (ideally: none). Local files: projects, app config, build logs on disk. No accounts. Network: optional 3dslink to user IP; optional browser open to docs URLs |
| `docs/SECURITY.md` | How to report vulnerabilities (email or private GitHub advisory). No bounty required |

### Content rules

- [ ] **4.L.1** Do not instruct piracy or commercial ROM use
- [ ] **4.L.2** Do not host or link to DSP firmware dumps
- [ ] **4.L.3** Trademark: descriptive use only; add “unofficial” / “not affiliated”
- [ ] **4.L.4** Warranty disclaimer (AS IS)
- [ ] **4.L.5** If screenshots show a face/name/Discord, scrub PII
- [ ] **4.L.6** External links https-only to official or well-known projects (devkitPro, Citra fork you actually recommend — name the fork)
- [ ] **4.L.7** Export control: not applicable to typical homebrew editor, but do not ship encryption circumvention tools

### Privacy checklist (fill answers)

| Question | Answer (fill in) |
|----------|------------------|
| Does Studio send telemetry? | |
| Does Studio check for updates automatically? | |
| What is written under AppData? | |
| What network destinations can it hit? | |
| Are crash reports uploaded? | |

---

## Tasks

### 4.1 Documentation set

- [ ] **4.1.1** Rewrite root `README.md`: hero = installer link placeholder + what it is + requirements + screenshots
- [ ] **4.1.2** `docs/USER_GUIDE.md` — new project, editor tools, assets, save/build, CIA
- [ ] **4.1.3** `docs/TOOLCHAIN.md` — install packages, PATH/`link.exe` conflict, makerom/bannertool
- [ ] **4.1.4** `docs/PLAYING.md` — controls, Easy mode, Citra setup, dspfirm, save locations
- [ ] **4.1.5** `docs/TROUBLESHOOTING.md` — build fail, silent audio, saves empty, SmartScreen, OneDrive target dir
- [ ] **4.1.6** Rewrite `src-tauri/template/README.md` to match current game (controls, features, build)
- [ ] **4.1.7** Remove contradictions (e.g. “not a CIA” if CIA is supported)
- [ ] **4.1.8** Sync in-app Help text with these docs (same facts)

### 4.2 Media

- [ ] **4.2.1** 3–6 screenshots: welcome, editor, assets, build success, in-game (Citra or hardware)
- [ ] **4.2.2** Store under `docs/images/` with compressed PNGs; no 20MB captures
- [ ] **4.2.3** Optional 30–90s demo video (YouTube/unlisted OK); link from README — no music you do not have rights to

### 4.3 Repo presentation

- [ ] **4.3.1** Topics/tags on GitHub ready (3ds, homebrew, tauri, …)
- [ ] **4.3.2** Description one-liner under 100 chars
- [ ] **4.3.3** `CHANGELOG.md` starting at v0.1.0
- [ ] **4.3.4** Issue templates optional: bug + toolchain
- [ ] **4.3.5** Delete or archive stale internal plans that confuse outsiders

### 4.4 Product truth pass

- [ ] **4.4.1** Feature list in README matches shipped UI only
- [ ] **4.4.2** Explicit “Windows only” if still true
- [ ] **4.4.3** Explicit “devkitPro required to compile”
- [ ] **4.4.4** Explicit “Citra or CFW 3DS to play”

### 4.5 Safety / abuse

- [ ] **4.5.1** CODE_OF_CONDUCT optional; if added, keep short
- [ ] **4.5.2** SECURITY.md reporting path verified (inbox you read)
- [ ] **4.5.3** Moderating plan if Issues open: no exploit-request threads for consoles beyond your tool’s scope

### 4.6 Final code hygiene from earlier logs

- [ ] **4.6.1** Close or defer every `open` item from Phases 0–3 Problem logs
- [ ] **4.6.2** No TODO comments that say “before release” left in user-facing paths
- [ ] **4.6.3** Version strings match Release tag you will use

---

## Acceptance criteria

- Stranger can go README → installer → Help/docs → Citra play without asking you
- LEGAL + PRIVACY + SECURITY exist and match actual app behavior
- Screenshots and feature list are accurate
- In-app Help and `docs/` do not contradict

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P4-001 | | | | open | |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| | Which Citra fork to recommend | |
| | Host demo video where | |
| | Public Issues open at launch? | |
| | Privacy statements finalized | |

---

## Exit checklist

- [ ] Docs reviewed once end-to-end on a clean machine
- [ ] Legal/privacy tables filled
- [ ] Problem log: no open blockers
- [ ] Ready for Phase 5
