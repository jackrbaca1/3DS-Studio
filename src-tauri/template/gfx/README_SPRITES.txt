============================================================
  SPRITE & TILEMAP ASSET GUIDE FOR 3DS PLATFORMER
============================================================

WHERE TO PUT SPRITES
--------------------
Place all your PNG sprite files in this directory:
  platformer/gfx/

Then list them in sprites.t3s (one filename per line).
The first line of the .t3s file contains tex3ds flags.


.T3S FILE FORMAT
----------------
The .t3s file tells tex3ds how to pack your PNGs into a
texture atlas (.t3x). Format:

  --atlas -f <format> -z auto
  image1.png
  image2.png
  ...

Available pixel formats:
  rgba8888   - 32-bit, highest quality, most VRAM
  rgb888     - 24-bit, no alpha
  rgba5551   - 16-bit with 1-bit alpha
  rgb565     - 16-bit, no alpha
  rgba4444   - 16-bit with 4-bit alpha
  la88       - 16-bit luminance + alpha
  a8         - 8-bit alpha only
  la44       - 8-bit luminance + alpha
  l8         - 8-bit luminance
  a4         - 4-bit alpha
  l4         - 4-bit luminance
  etc1       - Compressed (lossy, no alpha)
  etc1a4     - Compressed with 4-bit alpha

For a platformer, rgba5551 or rgba4444 are good choices
to save VRAM while keeping transparency.


SPRITE SIZE RULES
-----------------
- Individual sprites can be any size
- tex3ds packs them into a power-of-two atlas (max 1024x1024)
- Smaller sprites = more fit in one atlas = fewer texture swaps
- Recommended sizes for a platformer:
    Player:    16x24 or 32x32
    Tiles:     16x16 or 8x8
    Coins:     8x8 or 16x16
    Enemies:   16x16 or 32x32


HOW THE BUILD PIPELINE WORKS
-----------------------------
1. You put PNGs in gfx/ and list them in gfx/sprites.t3s
2. The Makefile calls tex3ds to convert sprites.t3s -> sprites.t3x
3. sprites.t3x is placed in romfs/gfx/
4. A header file sprites.h is generated in build/ with enum indices
5. In your code, load with:
     C2D_SpriteSheet sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
6. Access individual sprites by index (order matches .t3s listing):
     C2D_SpriteFromSheet(&mySprite, sheet, 0); // first image
     C2D_SpriteFromSheet(&mySprite, sheet, 1); // second image


TILEMAP APPROACH
----------------
The 3DS does not have a built-in tilemap engine like the GBA/DS.
You must render tiles manually using citro2d. Two approaches:

APPROACH A: Sprite-based tiles (simpler)
  - Each tile is a sprite from your spritesheet
  - Loop through your 2D tile array and draw each visible tile:

    for (int ty = 0; ty < MAP_H; ty++) {
        for (int tx = 0; tx < MAP_W; tx++) {
            int tileId = tilemap[ty][tx];
            if (tileId == 0) continue; // empty
            float drawX = tx * TILE_SIZE - cameraX;
            float drawY = ty * TILE_SIZE - cameraY;
            // Only draw if on screen
            if (drawX > -TILE_SIZE && drawX < 400 &&
                drawY > -TILE_SIZE && drawY < 240) {
                C2D_DrawImageAt(
                    C2D_SpriteSheetGetImage(sheet, tileId - 1),
                    drawX, drawY, 0.0f);
            }
        }
    }

APPROACH B: Pre-rendered background (advanced)
  - Render tiles to an offscreen texture once
  - Draw the texture each frame (faster for static levels)
  - More complex to set up

For a basic platformer, Approach A works great. The New 3DS
GPU can handle hundreds of sprite draws per frame easily.


TILEMAP DATA FORMAT
-------------------
Store your level as a 2D array of tile IDs:

  static const u8 tilemap[MAP_H][MAP_W] = {
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 3, 3, 0, 0, 0 },  // 3 = platform tile
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 2, 2, 0, 0, 0, 0, 2, 2 },  // 2 = ground tile
      { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },  // 1 = solid block
  };

  Tile ID 0 = empty (don't draw)
  Tile IDs 1+ = index into your spritesheet


ENABLING SPRITES IN THE MAKEFILE
---------------------------------
When you're ready to use real sprite assets:

1. Uncomment these lines in the Makefile:
     GRAPHICS  := gfx
     ROMFS     := romfs
     GFXBUILD  := $(ROMFS)/gfx

2. Add romfsInit() at the start of main()
3. Add romfsExit() at cleanup
4. Load your spritesheet:
     spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");

5. Draw sprites instead of rectangles:
     C2D_SpriteFromSheet(&spr, spriteSheet, SPRITES_PLAYER_IDLE_IDX);
     C2D_SpriteSetPos(&spr, x, y);
     C2D_DrawSprite(&spr);


SCREEN DIMENSIONS (New 3DS XL)
-------------------------------
  Top screen:    400 x 240 pixels (gameplay)
  Bottom screen: 320 x 240 pixels (touch, HUD, map, etc.)

  Note: The XL has physically larger screens but the SAME
  pixel resolution as the regular 3DS. Your game will look
  identical on both — just bigger pixels on the XL.

  Max texture size: 1024 x 1024
  Texture dimensions must be powers of two (handled by tex3ds)


CREATING PLACEHOLDER ART
-------------------------
Use any image editor to create small PNGs:
  - player_idle.png:  16x24, orange rectangle with white eye
  - player_run1.png:  16x24, same with legs offset
  - player_jump.png:  16x24, same with arms up
  - tile_ground.png:  16x16, brown/green square
  - tile_platform.png: 16x16, grey/brown square
  - coin.png:         8x8, yellow circle
  - enemy.png:        16x16, red square

Transparency: Use PNG alpha channel. Pixels with alpha=0
are fully transparent.

============================================================
