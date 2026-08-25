# Playing your builds (3DS hardware)

3DS Studio builds **`.3dsx`** and optional **`.cia`** packages. It does not launch or recommend emulators. Copy the output to a CFW 3DS.

Unofficial homebrew. Not affiliated with Nintendo. Installing CFW has risks — your choice and responsibility. See [LEGAL.md](LEGAL.md).

## What Studio produces

| Format | How you get it | Typical use |
|--------|----------------|-------------|
| `.3dsx` | **Build 3dsx** | Homebrew Launcher, or send with **3dslink** |
| `.cia` | **Build CIA** (needs `makerom` + `bannertool`) | Install with FBI, then launch from Home Menu |

Outputs land in the **project root** (same folder as the `Makefile`) — for example `Documents/3DSStudio/MyGame/MyGame.3dsx`.  
The `build/` subfolder only holds intermediate `.o` / `.h` files; it is not where you pick up the playable ROM. Use **Folder** in Studio to open the project root.

---

## Option A — Copy to the SD card

1. Build in Studio (`.3dsx` and/or `.cia`).
2. Eject / remove the SD from the 3DS (or use an SD reader while the console is off).
3. Copy files:
   - **`.3dsx`** → `sdmc:/3ds/` (or a subfolder), then open **Homebrew Launcher** and run it.
   - **`.cia`** → anywhere convenient (e.g. `sdmc:/cias/`), then open **FBI** → select the CIA → **Install**.
4. Put the SD back and boot.

Tip: after a CIA install you can delete the `.cia` from the SD to free space; the title stays installed.

---

## Option B — FTP to the 3DS (keep the SD in the console)

Useful when you don’t want to pull the card every build.

1. On the 3DS, start an FTP server homebrew (e.g. **ftpd** / similar) and note the IP + port shown on screen.
2. On the PC, connect with any FTP client (FileZilla, WinSCP, Windows Explorer `ftp://…`, etc.) to that address on your LAN.
3. Upload:
   - `.3dsx` → `/3ds/`
   - `.cia` → e.g. `/cias/`
4. On the 3DS: run the `.3dsx` from Homebrew Launcher, or install the `.cia` with FBI.

Studio does **not** speak FTP itself — only your FTP client and the 3DS app do.

---

## Option C — 3dslink (`.3dsx` over Wi‑Fi)

Fast iterate loop for Homebrew Launcher apps (not CIA install).

1. 3DS and PC on the **same LAN**.
2. On the 3DS: open **Homebrew Launcher** → press **Y** to show the console IP.
3. In Studio: **Build 3dsx** first, then **3dslink** → enter that IP → Connect.
4. The `.3dsx` is sent and should start on the console.

Requires `3dslink` from the devkitPro 3DS tools package. IP is local-only; nothing is uploaded to the cloud.

---

## Audio on hardware (`dspfirm.cdc`)

NDSP (SFX / music) needs DSP firmware on the SD:

`sdmc:/3ds/dspfirm.cdc`

Dump it on **your** console: **Luma Rosalina** → **Miscellaneous** → **Dump DSP firmware**, then copy that file to `sdmc:/3ds/`.

Do **not** redistribute dumped firmware in this repo or releases. Do not download random “dspfirm” mirrors.

Music is optional: import an MP3 you have rights to under Assets → Soundtrack, then rebuild. If missing, the game skips music; SFX still work when `dspfirm.cdc` is present.

---

## Save slots

Saves and settings use:

`sdmc:/3ds/<save_prefix>_slot1.dat` … `_slot3.dat`  
`sdmc:/3ds/<save_prefix>_settings.dat`

`<save_prefix>` comes from the game title slug (`GC_SAVE_PREFIX` / `studio_meta.json`). Different projects use different prefixes so they do not overwrite each other.

Ensure `sdmc:/3ds` exists (the game also tries to create it).

---

## Emulators (unsupported)

You may load the same `.3dsx` / `.cia` in a PC emulator if you choose. Studio does not install, detect, recommend, or launch emulators. Prefer hardware for audio/`dspfirm` and save layout.

---

## Controls (in-game Settings)

| Setting | Options |
|---------|---------|
| Controls | NORMAL / EASY |
| Sprint Mode | TOGGLE / HOLD |

The Studio starter sample teaches move + jump in dialogue and keeps advanced moves off until you enable them per level.
