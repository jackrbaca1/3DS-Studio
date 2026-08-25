# Privacy

3DS Studio is designed to keep work **on your PC**. This document matches the v0.1.0 behavior.

Unofficial homebrew tooling. Not affiliated with Nintendo. See [LEGAL.md](LEGAL.md).

## Summary

| Topic | Behavior |
|-------|----------|
| Accounts / login | None |
| Telemetry / analytics | None |
| Automatic update checks | None |
| Crash report uploads | None |
| Cloud sync by Studio | None (OneDrive may sync folders if you put projects there — that is Windows/OneDrive, not Studio) |

## Data stored on disk

| Location | Contents |
|----------|----------|
| `Documents\3DSStudio\<ProjectName>\` | Your game projects (levels, assets, Makefile, builds) |
| `%APPDATA%\3ds-studio\config.json` | Optional Studio setting: custom DEVKITPRO path |
| Project `build/` | Local compiler intermediates when you Build |
| Install dir (`%LOCALAPPDATA%\3DS Studio\`) | App binaries + bundled template (installer) |

Uninstalling Studio removes the app folder and Start Menu shortcut. **AppData config and Documents projects may remain.**

## Network

Studio may use the network only when **you** trigger it:

1. **3dslink** — sends a built `.3dsx` to a **LAN IP you enter** (your 3DS). Same network only; not a cloud service.
2. **Open install docs / tool links** — opens the system browser to allowlisted **https** URLs (devkitPro Getting Started; makerom / bannertool GitHub releases).
3. **Installer (NSIS)** — if WebView2 Runtime is missing, the installer may download Microsoft’s Evergreen WebView2 bootstrapper.

There is no Studio “phone home,” account API, or silent download of game toolchains.

## Build logs

Build output is shown in the local Build Console. It is not uploaded by Studio.

## Changes

If privacy behavior changes in a future version, this file and the release notes will be updated.
