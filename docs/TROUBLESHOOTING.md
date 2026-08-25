# Troubleshooting

## Installer / first launch

| Symptom | What to try |
|---------|-------------|
| **Windows protected your PC** | Unsigned build. **More info → Run anyway**. |
| App won’t open / blank window | Install [WebView2 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/). The NSIS installer can also bootstrap it. |
| Help / Setup tools “do nothing” | Update to a build where modals sit above the welcome screen (v0.1.0+). |

## Build / toolchain

| Symptom | What to try |
|---------|-------------|
| Tools missing / bash not found | **Setup tools…** → browse to `C:\devkitPro` → Retest. Ignore `/opt/devkitpro` in the environment. |
| Wrong folder still shows “ready” | Fixed in v0.1: custom path is preferred even when invalid. Retest should go **red**; pick the real DEVKITPRO. |
| `No rule to make target … sprites.t3x` | Path has **spaces**. **Save As…** into `Documents\3DSStudio\NameWithoutSpaces`. |
| Looking for `.3dsx` under `build/` | Playable files are in the **project root**. Use **Folder**. |
| Cannot write `game_config.h` / missing `source/` | OneDrive “online only” or incomplete folder. Download locally or **Start Fresh Example**. |
| CIA build fails | Install `makerom` + `bannertool`, or use **Build 3dsx** only. See [TOOLCHAIN.md](TOOLCHAIN.md). |

## Audio / saves on 3DS

| Symptom | What to try |
|---------|-------------|
| Silent SFX / music | Dump `dspfirm.cdc` on the console and place at `sdmc:/3ds/dspfirm.cdc`. Do not download random firmware dumps. [PLAYING.md](PLAYING.md) |
| No music but SFX OK | Import a soundtrack MP3 in Assets and rebuild (optional). |
| Empty / missing saves | Ensure `sdmc:/3ds` exists; check save prefix from your game title. |

## Editor / projects

| Symptom | What to try |
|---------|-------------|
| New Project config error | Update Studio; defaults include all feature flags. |
| Project list empty | Projects are under `Documents\3DSStudio\` (often OneDrive Documents). |
| Build does nothing | Check Build Console; save failures appear there. Fix disk/path, then rebuild. |

## Still stuck

1. In-app **Help → Troubleshooting**
2. GitHub Issue using the **bug** or **toolchain** template
3. Attach Build Console text (no secrets needed)

Security issues: [SECURITY.md](SECURITY.md) — not public Issues.
