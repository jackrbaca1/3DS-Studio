# Release notes — 3DS Studio v0.1.0

## What's included

- Windows **NSIS** installer (per-user, x64) — **3DS Studio**
- Bundled game **template** (no Nintendo SDK, no `dspfirm`)
- In-app **Help** and **Setup tools…** wizard
- Optional companion files (when packaged): demo `.3dsx`, sample project zip, `SHA256SUMS.txt`, `LICENSE`, `THIRD_PARTY.md`

## Requirements

- Windows 10/11 x64
- **WebView2** Runtime (usually preinstalled; installer can download the Evergreen bootstrapper if missing)
- To **Build** games inside Studio: [devkitPro](https://devkitpro.org/) with `devkitARM` (typically `C:\devkitPro`). Not bundled.

## Install

1. Run the `.exe` NSIS installer.
2. If **Windows protected your PC** (unsigned build): **More info → Run anyway**.
3. Start Menu → **3DS Studio**.

Uninstall via Windows Apps & features. App settings under `%APPDATA%\3ds-studio` and projects under `Documents\3DSStudio` may remain.

This release **does not** auto-update or download installers in the background.

## Playing builds on a 3DS

See [docs/PLAYING.md](../PLAYING.md): SD card copy, FTP, 3dslink, `dspfirm.cdc`, save paths. CFW and homebrew are your responsibility.

## Checksums

Verify every download against `SHA256SUMS.txt` in the same release folder (rebuilt by `npm run release:package`).

## Known limitations

- Unsigned installer → SmartScreen friction
- No MSI; NSIS only
- CIA tools (`makerom` / `bannertool`) are optional extras for CIA builds
- GitHub Release upload requires a configured remote (artifacts can be built locally first)
