# Game template (bundled with 3DS Studio)

Side-scrolling platformer template for **Nintendo 3DS** homebrew, built with **devkitARM**, **libctru**, and **citro2d**. Edited and built from **3DS Studio** on Windows.

Unofficial. Not affiliated with Nintendo.

## What you get

- Tile-based levels exported from Studio into `source/main.cpp`
- Physics, crackers (collectibles), enemies, dialogue, checkpoints, warps
- Per-level feature toggles (double jump, dash, wall jump, …)
- Dual-screen layout (gameplay top; UI / minimap / menus bottom as configured)
- Optional MP3 soundtrack via romfs (needs `dspfirm.cdc` on hardware — see repo `docs/PLAYING.md`)
- Outputs: **`.3dsx`** (default) and optional **`.cia`**

## Prefer Studio

For most people: open this folder (or a copy under `Documents\3DSStudio\`) in **3DS Studio**, edit visually, then **Build 3dsx**.

Command-line build still works if `DEVKITPRO` is set:

```bash
make          # → <TARGET>.3dsx in this directory
make cia      # needs makerom + bannertool
make clean
```

**No spaces** in the project path.

## Layout

```
├── Makefile
├── source/
│   ├── main.cpp          # Game + embedded level data from Studio
│   ├── game_config.h     # Title / physics defaults from Studio
│   └── minimp3.h
├── gfx/                  # PNGs + sprites.t3s (tex3ds)
├── romfs/                # Built assets (e.g. soundtrack.mp3)
├── cia.rsf / banner.*    # CIA packaging inputs when used
└── build/                # Intermediates only — not the playable output
```

## Default controls (in-game)

| Input | Action |
|-------|--------|
| Circle Pad / D-Pad | Move |
| A / B | Jump (and other moves if enabled for the level) |
| In-game Settings | Controls NORMAL/EASY, Sprint TOGGLE/HOLD |

Exact binds depend on Easy mode and which level features are enabled in Studio.

## Playing on hardware

Copy `.3dsx` / `.cia` to a CFW 3DS (SD, FTP, or 3dslink). Studio does not launch emulators.

Full steps: [`docs/PLAYING.md`](../../docs/PLAYING.md) (audio firmware, saves, CIA).

## License

Same as the parent 3DS Studio repository (MIT). See root `LICENSE` and `THIRD_PARTY.md`. minimp3 is CC0 (see `source/minimp3.h`).
