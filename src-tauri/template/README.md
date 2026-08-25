# 3DS Platformer — devkitPro Example

A basic side-scrolling platformer for the **New Nintendo 3DS XL**, built with devkitARM, libctru, and citro2d.

## Features
- Player character with run, jump, and gravity physics
- AABB collision detection against platforms
- Collectible coins with bob animation
- Smooth horizontal camera scrolling
- Circle Pad + D-Pad support
- Debug info on bottom screen
- Ready-to-activate sprite pipeline (tex3ds)

## Hardware Target
| Spec | Value |
|---|---|
| Console | New Nintendo 3DS XL |
| CPU | ARM11 MPCore @ 804 MHz |
| Top Screen | 400×240 px |
| Bottom Screen | 320×240 px (touch) |
| Output Format | `.3dsx` (Homebrew Launcher) |

## Project Structure

```
platformer/
├── Makefile              # Build configuration
├── README.md             # This file
├── source/
│   └── main.cpp          # Game code (C++)
├── gfx/                  # Sprite PNGs + .t3s manifest
│   ├── sprites.t3s       # tex3ds sprite list (edit this)
│   ├── README_SPRITES.txt # Detailed sprite/tilemap guide
│   └── *.png             # Your sprite images go here
├── romfs/                # Created by build — embedded filesystem
│   └── gfx/
│       └── sprites.t3x   # Compiled texture atlas (auto-generated)
├── data/                 # Raw binary data files (optional)
├── include/              # Extra headers (optional)
└── build/                # Object files + intermediates (auto-generated)
```

## Controls
| Input | Action |
|---|---|
| D-Pad Left/Right or Circle Pad | Move |
| A / B | Jump |
| SELECT | Reset level |
| START | Exit to Homebrew Menu |

## Building

### Prerequisites
- [devkitPro](https://devkitpro.org/) with devkitARM installed
- `DEVKITARM` environment variable set (e.g. `C:/devkitPro/devkitARM`)

### Build Commands
```bash
# From the platformer/ directory:
make          # Build the .3dsx
make clean    # Remove build artifacts
```

This produces `platformer.3dsx` which you can run via:
1. **3dslink** (Wi-Fi): `3dslink platformer.3dsx` (console must be on same network)
2. **SD card**: Copy `platformer.3dsx` to `/3ds/` on your SD card, launch from Homebrew Launcher

## Adding Real Sprites

The game currently uses colored rectangles. To switch to real sprites:

### Step 1: Create your PNGs
Place sprite PNGs in the `gfx/` directory. Recommended sizes:
- Player: 16×24 or 32×32
- Tiles: 16×16
- Coins: 8×8 or 16×16

### Step 2: Edit `gfx/sprites.t3s`
List your PNGs (one per line). First line has tex3ds flags:
```
--atlas -f rgba5551 -z auto
player_idle.png
player_run1.png
tile_ground.png
coin.png
```

### Step 3: Enable the sprite pipeline in `Makefile`
Uncomment these three lines:
```makefile
GRAPHICS  := gfx
ROMFS     := romfs
GFXBUILD  := $(ROMFS)/gfx
```

### Step 4: Update `main.cpp`
```cpp
// At init:
romfsInit();
C2D_SpriteSheet spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");

// Drawing a sprite:
C2D_Sprite spr;
C2D_SpriteFromSheet(&spr, spriteSheet, 0); // index matches .t3s order
C2D_SpriteSetPos(&spr, x, y);
C2D_DrawSprite(&spr);

// At cleanup:
C2D_SpriteSheetFree(spriteSheet);
romfsExit();
```

See `gfx/README_SPRITES.txt` for the full sprite and tilemap guide.

## About output formats
- `.3dsx` — Homebrew Launcher / Citra (default `make` target)
- `.cia` — optional `make cia` when makerom/bannertool are installed (see project Makefile)

## License
Same as the parent 3DS Studio repository (MIT). See root `LICENSE` and `THIRD_PARTY.md`. minimp3 is CC0 (see header in `source/minimp3.h`).
