# Third-party notices

3DS Studio and the bundled platformer template include or depend on the following.

## Editor (3DS Studio)

| Component | License | Notes |
|-----------|---------|--------|
| [Tauri](https://tauri.app/) 2.x and crates | MIT / Apache-2.0 | Desktop shell |
| [serde](https://serde.rs/) / serde_json | MIT OR Apache-2.0 | Serialization |
| [zip](https://crates.io/crates/zip) | MIT | Archive helper |
| tauri-plugin-dialog / tauri-plugin-opener | MIT OR Apache-2.0 | File dialogs, open URLs |
| [@tauri-apps/cli](https://www.npmjs.com/package/@tauri-apps/cli) | Apache-2.0 OR MIT | Dev/build only |

Full crate license text is in each dependency’s upstream repository. Run `cargo tree` / license tooling locally for a complete graph.

## Bundled game template

| Component | License | Notes |
|-----------|---------|--------|
| Template game code (`src-tauri/template/`) | MIT (same as this repo) | Unless a file says otherwise |
| [minimp3](https://github.com/lieff/minimp3) (`source/minimp3.h`) | CC0-1.0 | MP3 decode |
| libctru / citro2d / citro3d / devkitARM | Per [devkitPro](https://devkitpro.org/) | **Not redistributed** in this repo; required on the user’s machine to compile |

## Not included (by design)

- Nintendo 3DS system firmware, DSP firmware dumps (`dspfirm.cdc`), or commercial ROMs
- The full devkitPro toolchain installer or packages

See `docs/LEGAL.md`.
