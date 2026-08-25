# Launch checklist (v0.1.0) — GitHub Desktop / website

`gh` is optional. Use **GitHub Desktop** + browser.

## 0. Rebuild release folder (if missing)

```powershell
cd "C:\Users\JACKR\OneDrive\Documents\Personal Projects\3DSPlatformerDevelopmentPlatform"
npm run tauri:build
npm run release:package
```

Artifacts land in `dist\release\v0.1.0\` (gitignored):

- `3DS Studio_0.1.0_x64-setup.exe`
- `3DSStudio-ExamplePlatformer-v0.1.0.3dsx`
- `3DSStudio-ExamplePlatformer-v0.1.0-project.zip`
- `SHA256SUMS.txt`, `LICENSE`, `THIRD_PARTY.md`, `RELEASE_NOTES.md`

## 1. Clean tree + push

In GitHub Desktop: commit any remaining work, **Push origin**.

Confirm: https://github.com/jackrbaca1/3DSPlatformerDevelopmentPlatform

## 2. Create tag `v0.1.0`

**Desktop:** History → right-click latest release commit → **Create tag** → `v0.1.0` → push tags  
**or** website: Releases → Draft → set tag `v0.1.0` on `main`.

## 3. GitHub Release

1. Repo → **Releases** → **Draft a new release** (or edit draft)
2. Tag: `v0.1.0`
3. Title: `3DS Studio v0.1.0`
4. Description: paste from `docs/release/RELEASE_NOTES_v0.1.0.md` (+ link to `docs/PLAYING.md`)
5. Upload **all** files from `dist\release\v0.1.0\`
6. **Publish release** (not draft) when ready

## 4. Make repository public

Settings → General → Danger Zone → **Change visibility** → Public  
Only after LICENSE, PRIVACY, SECURITY, and Release assets are in place.

## 5. About box

Settings → General (scroll to About):

- Description: `Windows editor for homebrew 3DS platformers (Tauri). Builds .3dsx/.cia; needs devkitPro.`
- Topics: `3ds`, `homebrew`, `tauri`, `nintendo-3ds`, `devkitpro`, `platformer`

## 6. Smoke from the public URL

1. Download installer from the Release page (not from OneDrive).
2. Compare SHA256 to `SHA256SUMS.txt`.
3. Install → Help → New/Example project → Setup tools → Build if toolchain present.

## 7. Announce once

Paste `docs/release/ANNOUNCE_v0.1.0.md` to your chosen channel(s). One thread per venue.

## 8. First week

Watch Issues. Prefer doc fixes. Critical install/build bug → ship `v0.1.1`.
