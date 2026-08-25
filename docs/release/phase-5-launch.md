# Phase 5 — Launch and post-launch

**Status:** not started  
**Owner:**  
**Started:**  
**Finished:**  

## Goal

Make the project public cleanly: publish Release, open repo, announce once with accurate links, then monitor early fallout.

## Out of scope

- Major new features
- Rewriting architecture

## Prerequisites

- Phase 4 Exit complete
- Installer + `.3dsx` + checksums ready
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

- [ ] **5.S.1** Prefer links to **your** GitHub Release, not random file hosts
- [ ] **5.S.2** Warn users: only download installer from your Releases page; verify SHA256
- [ ] **5.S.3** Do not hotlink unsigned binaries from Discord CDN as the canonical source
- [ ] **5.S.4** If posting on Reddit/forums, use official account; expect SmartScreen questions — answer with checksums
- [ ] **5.S.5** Do not collect emails unless you have a real privacy policy for that list

---

## Tasks

### 5.1 Pre-flight (same day as launch)

- [ ] **5.1.1** `git status` clean on release commit
- [ ] **5.1.2** Re-download installer from draft Release on a clean PC; verify SHA256
- [ ] **5.1.3** Smoke: install → new project → help → (toolchain) build → Citra load
- [ ] **5.1.4** Confirm PRIVACY answers still true
- [ ] **5.1.5** Branch protection on default branch (require PR if working with others)

### 5.2 Publish

- [ ] **5.2.1** Push tags `v0.1.0` (or chosen version)
- [ ] **5.2.2** Publish GitHub Release (no longer draft)
- [ ] **5.2.3** Set repo public
- [ ] **5.2.4** Pin README; Releases link at top works
- [ ] **5.2.5** Archive or private any personal fork that still has secrets/paths

### 5.3 Announce (one clear message)

Include only:

1. What it is (one sentence)
2. Windows installer link
3. Demo `.3dsx` + Citra note
4. “devkitPro required to compile”
5. Not affiliated with Nintendo
6. SHA256 or link to checksums file

Channels (check what you will use):

- [ ] **5.3.1** Class / school submission portal
- [ ] **5.3.2** Optional: relevant homebrew Discord/forum (follow their rules)
- [ ] **5.3.3** Optional: portfolio site
- [ ] **5.3.4** Do **not** spam multiple identical threads

Draft announce (edit before send):

```text
3DS Studio v0.1.0 — Windows editor for homebrew 3DS platformers.
Installer + demo .3dsx: <Release URL>
Compile requires free devkitPro. Play demo in Citra (see PLAYING.md for dspfirm).
Unofficial; not affiliated with Nintendo.
Checksums: SHA256SUMS.txt on the Release.
```

### 5.4 First-week monitoring

- [ ] **5.4.1** Watch Issues for SmartScreen, toolchain path, Citra audio
- [ ] **5.4.2** Patch docs fast for repeated questions (prefer doc fix over code if possible)
- [ ] **5.4.3** If critical bug: `v0.1.1` patch Release
- [ ] **5.4.4** Log every user-facing bug in Problem log below (even if fixed same day)

### 5.5 Post-launch backlog (do not block launch)

Record only; schedule later:

- [ ] Auto-update policy
- [ ] macOS/Linux
- [ ] Code signing cert
- [ ] Cloud builds
- [ ] QR CIA install helper

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
| P5-001 | | | | open | |

---

## Decisions

| Date | Decision | Why |
|------|----------|-----|
| | Public date | |
| | Announce channels | |
| | Issues enabled? | |
| | v0.1.x patch policy | |

---

## Exit checklist

- [ ] Release public and smoke-tested from public URL
- [ ] At least one external person (classmate/friend) followed README successfully **or** documented failure in Problem log with fix shipped
- [ ] Phase 0–5 Problem logs reviewed; deferred items listed in post-launch backlog
- [ ] Launch complete
