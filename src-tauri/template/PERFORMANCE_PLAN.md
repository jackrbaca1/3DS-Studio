# Performance Optimization Plan

## Problem
Levels with many tiles drawn experience frame drops on New 3DS XL. The root cause is the high number of individual `C2D_DrawImageAt` calls per frame, especially with stereoscopic 3D enabled (doubling draw calls for the top screen).

## Current Draw Call Budget (per eye)
| Layer | Approx Calls (worst case) |
|-------|--------------------------|
| Background pattern | ~154 (14×11 grid) |
| Foreground tilemap | ~143 (13×11 visible tiles) |
| Default grass overlay | ~13 tiles (surface only) |
| Coins | up to 16 |
| Enemies | up to 8 |
| VFX particles | up to 16 |
| Dust particles | up to 8 |
| Player | 1 |
| Alt grass overlay | ~6 tiles |
| **Total per eye** | **~365** |
| **With stereo 3D** | **~730 on top screen** |

Add bottom screen minimap draws and the budget easily exceeds 800+ objects/frame.

---

## Optimization Strategies (Priority Order)

### 1. Skip Off-Screen Draws (Quick Win) ✅ DONE
- **Background pattern**: Already view-culled. ✓
- **Foreground tilemap**: Already view-culled. ✓
- **Enemies/coins/VFX/dust/flatten/movers/popups**: `onScreen()` frustum check added. ✓

### 2. Reduce Background Pattern Draws ✅ DONE (partial)
- Grid reduced from +2 to +1 padding (~416→390 calls on left eye).
- BG pattern **skipped entirely on right eye** (saves ~390 calls per stereo frame).
- Future: Pre-render to texture for single-quad draw (not yet implemented).

### 3. Batch Adjacent Same-Type Tiles (Medium Effort)
Horizontal runs of the same tile type can be drawn as a single stretched image or use a row-based atlas slice. This could cut foreground tilemap calls significantly in levels with large solid regions.

### 4. Dirty-Rect Tilemap Caching (Higher Effort)
- Render the visible tilemap region into an off-screen texture.
- Only re-render when camera moves more than 1 tile.
- Draw the cached texture as 1-2 quads per eye.
- This is the biggest win (~143 calls → 1-2 calls) but requires managing render targets.

### 5. Reduce Stereo Overhead ✅ DONE
- Skip right-eye render when 3D slider < threshold. ✓
- Right eye skips: BG pattern, default grass, alt grass, VFX, dust, flatten FX, stomp popups. ✓
- Right eye keeps: tilemap, coins, enemies, player, movers, 3D tiles, fade (for correct stereo). ✓

### 6. Grass Overlay Optimization
- Grass only exists on the surface row of ground. Currently iterates the full visible grid.
- Maintain a list of grass tile positions instead of scanning the map each frame.
- Or skip grass draw entirely if more than N tiles are visible (adaptive quality).

### 7. Object Pool Culling ✅ DONE
- Coins, enemies, VFX, dust, flatten FX, movers, popups: `onScreen()` check added. ✓
- Cost: one inline comparison per entity.

### 8. Increase C2D Max Objects
- `C2D_Init(C2D_DEFAULT_MAX_OBJECTS)` defaults to 4096. If hitting this limit, increase it.
- Unlikely to be the bottleneck but worth verifying.

---

## Implementation Order
1. ~~**Object culling** for enemies/coins/VFX~~ ✅ Done
2. ~~**Background pattern** reduction~~ ✅ Done (partial — skipped on right eye, grid tightened)
3. ~~**Stereo detail reduction**~~ ✅ Done
4. **Tilemap texture caching** (largest remaining improvement, needs render target management)
5. **Adaptive grass** (skip if frame budget tight)
6. **BG texture caching** (pre-render to single quad)

---

## Metrics ✅ IMPLEMENTED
- Frame time measured via `svcGetSystemTick()` at loop start, displayed on bottom screen in dev mode.
- Shows `X.X ms (Y fps)` in green text when Developer mode is enabled.
- Enable via: Settings → Developer → ON, then play a level to see metrics.
