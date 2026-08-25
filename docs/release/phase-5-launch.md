# Phase 5 — Launch and post-launch

**Status:** in progress  
**Owner:**  
**Started:** 2026-08-25  
**Finished:**  

## Goal

Make the project public cleanly: publish Release, open repo, announce once with accurate links, then monitor early fallout.

## Out of scope

- Major new features
- Rewriting architecture

## Prerequisites

- Phase 4 Exit complete (docs/LEGAL/PRIVACY/SECURITY in repo; screenshots optional deferred)
- Installer + `.3dsx` + checksums — rebuild with `npm run tauri:build` + `npm run release:package` if `dist/release/v0.1.0/` is missing
- LEGAL / PRIVACY / SECURITY published in repo

---

## Legal / privacy / safety (this phase)

| Item | Action |
|------|--------|
| Public repo | Visibility public only after LICENSE + PRIVACY + no secrets |
| Announce copy | No affiliation claims; no “bypass Nintendo” framing; no links to warez |
| Personal safety | Doxxing: strip personal phone/address from git history and bio if needed |
| Account security | 2FA on GitHub; protect `main`; no force-push after release tag |
| Supply chain | Tag Release from a clean tree; prefer signed tag if you use GPG/SSH signing |
| User data | Do not ask users to send full SD card dumps or dspfirm in Issues |
| Incident response | If a bad build ships: unpublish asset, yank tag notes, post fixed build; document in CHANGELOG |
| Take-down | Have a plan if Nintendo or a rightsholder objects (remove trademarked assets you do not own; keep code if clean) |

### Internet safety (announce channels)

- [x] **5.S.1** Prefer links to **your** GitHub Release, not random file hosts
- [x] **5.S.2** Warn users: only download installer from your Releases page; verify SHA256
- [x] **5.S.3** Do not hotlink unsigned binaries from Discord CDN as the canonical source
- [x] **5.S.4** If posting on Reddit/forums, use official account; expect SmartScreen questions — answer with checksums
- [x] **5.S.5** Do not collect emails unless you have a real privacy policy for that list

---

## Tasks

### 5.1 Pre-flight (same day as launch)

- [ ] **5.1.1** `git status` clean on release commit
- [ ] **5.1.2** Re-download installer from draft/public Release; verify SHA256
- [ ] **5.1.3** Smoke: install → new project → help → (toolchain) build
- [x] **5.1.4** Confirm PRIVACY answers still true
- [ ] **5.1.5** Branch protection on default branch (optional solo; enable if collaborating)

### 5.2 Publish

- [ ] **5.2.1** Push tag `v0.1.0`
- [ ] **5.2.2** Publish GitHub Release (installer, `.3dsx`, project zip, SHA256SUMS, LICENSE, THIRD_PARTY)
- [ ] **5.2.3** Set repo **public**
- [ ] **5.2.4** README Releases link works; pin About description/topics
- [x] **5.2.5** No separate secret-laden fork known

### 5.3 Announce (one clear message)

Draft: [ANNOUNCE_v0.1.0.md](ANNOUNCE_v0.1.0.md)

Channels:

- [ ] **5.3.1** Class / school submission portal (if applicable)
- [ ] **5.3.2** Optional: homebrew Discord/forum
- [ ] **5.3.3** Optional: portfolio site
- [x] **5.3.4** Do **not** spam multiple identical threads

### 5.4 First-week monitoring

- [ ] **5.4.1** Watch Issues for SmartScreen, toolchain, install/audio
- [ ] **5.4.2** Patch docs for repeated questions
- [ ] **5.4.3** Critical bug → `v0.1.1`
- [ ] **5.4.4** Log user-facing bugs in Problem log

### 5.5 Post-launch backlog (do not block launch)

- [ ] Auto-update policy
- [ ] macOS/Linux
- [ ] Code signing cert
- [ ] Cloud builds
- [ ] QR CIA install helper
- [ ] README screenshot PNGs (`docs/images/CAPTURE.md`)

---

## Acceptance criteria

- Public repo + published Release with installer and demo
- Announce text accurate and non-infringing
- Checksums published; 2FA on; no known secrets in history
- Owner knows how to yank/replace a bad build

---

## Problem log

| ID | Date | Task | Problem | Status | Resolution |
|----|------|------|---------|--------|------------|
| P5-001 | 2026-08-25 | 5.2 | `gh` CLI not installed; use GitHub Desktop / web UI | open | Steps in LAUNCH_CHECKLIST.md |
| P5-002 | 2026-08-25 | 5.2 | `dist/release/v0.1.0/` may be missing locally | open | Re-run `npm run tauri:build` + `npm run release:package` |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| 2026-08-25 | Public when Release assets uploaded | Avoid empty public repo |
| 2026-08-25 | Announce: GitHub Release URL only as binary source | Supply-chain / SmartScreen trust |
| 2026-08-25 | Issues stay enabled | Already have bug/toolchain templates |
| 2026-08-25 | Patch policy: `v0.1.1` for critical install/build breaks | Semver patch |

### GitHub About (Settings → General)

- **Description:** `Windows editor for homebrew 3DS platformers (Tauri). Builds .3dsx/.cia; needs devkitPro.`
- **Topics:** `3ds`, `homebrew`, `tauri`, `nintendo-3ds`, `devkitpro`, `platformer`
- **Website:** leave blank or link portfolio later

---

## Exit checklist

- [ ] Release public and smoke-tested from public URL
- [ ] At least one external person followed README **or** failure logged + fixed
- [ ] Phase 0–5 Problem logs reviewed; deferred items in post-launch backlog
- [ ] Launch complete

## How to yank a bad build

1. GitHub Release → edit → delete bad asset (or mark pre-release).
2. Ship fixed installer as `v0.1.1` (new tag) with updated `SHA256SUMS.txt`.
3. Note the yank in `CHANGELOG.md`.
4. Do **not** force-push `main` after others may have cloned.
