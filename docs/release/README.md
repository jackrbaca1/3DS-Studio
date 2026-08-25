# Release plans

Work phases in order. Do not start a phase until the previous phase **Exit checklist** is complete.

| Phase | File | Focus | Status |
|-------|------|--------|--------|
| 0 | [phase-0-hygiene.md](phase-0-hygiene.md) | Repo clean, license, identity | **complete** |
| 1 | [phase-1-portability.md](phase-1-portability.md) | Toolchain paths, saves/audio IDs, build outputs | **complete** |
| 2 | [phase-2-ux.md](phase-2-ux.md) | Setup wizard, help, non-technical flow | **complete** |
| 3 | [phase-3-packaging.md](phase-3-packaging.md) | Installer, Releases, signing | **complete** |
| 4 | [phase-4-polish.md](phase-4-polish.md) | Docs, screenshots, legal/privacy pages | not started |
| 5 | [phase-5-launch.md](phase-5-launch.md) | Public post, announce, post-launch | not started |

## How to track work

In each phase file:

1. Mark tasks `[x]` when done.
2. Fill **Problem log** as soon as something blocks or surprises you (do not wait until the end).
3. Fill **Decisions** when you choose between options (so later phases stay consistent).
4. Only then complete the **Exit checklist**.

Status values for the problem log: `open` | `fixed` | `wontfix` | `deferred`.

## Rules

- One phase at a time.
- Prefer small commits per completed task group.
- No release tagging until Phase 3 Exit is done.
- No public announce until Phase 5 Exit is done.
