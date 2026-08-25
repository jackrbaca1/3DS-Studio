//! Fresh-project asset helpers: keep the default tileset, generate colored-block
//! placeholders for optional art, and expose import requirements.

use flate2::write::ZlibEncoder;
use flate2::Compression;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

/// Spec for each Studio asset slot (shown in UI when empty / as a hint).
#[derive(Clone, Copy)]
pub struct AssetSpec {
    pub key: &'static str,
    pub rel_path: &'static str,
    pub label: &'static str,
    pub format: &'static str,
    pub width: u32,
    pub height: u32,
    /// If true, fresh projects keep the template file as-is (default tileset).
    pub keep_from_template: bool,
}

pub const ASSET_SPECS: &[AssetSpec] = &[
    AssetSpec {
        key: "tileset",
        rel_path: "gfx/CardBoard3ds-TileSet.png",
        label: "Tileset",
        format: "PNG",
        width: 512,
        height: 512,
        keep_from_template: true,
    },
    AssetSpec {
        key: "background1",
        rel_path: "gfx/Cavebg.png",
        label: "Cave Background 1",
        format: "PNG",
        width: 256,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "background2",
        rel_path: "gfx/Cavebg2.png",
        label: "Cave Background 2",
        format: "PNG",
        width: 256,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "title",
        rel_path: "gfx/Title.png",
        label: "Title Screen",
        format: "PNG",
        width: 400,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "bottom_menu",
        rel_path: "gfx/BottomMenuScreen.png",
        label: "Menu Background",
        format: "PNG",
        width: 320,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "menu_load",
        rel_path: "gfx/LoadGameSelected.png",
        label: "Load Button (highlight)",
        format: "PNG",
        width: 400,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "menu_new",
        rel_path: "gfx/NewGameSelected.png",
        label: "New Game Button (highlight)",
        format: "PNG",
        width: 400,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "menu_settings",
        rel_path: "gfx/SettingsSelected.png",
        label: "Settings Button (highlight)",
        format: "PNG",
        width: 400,
        height: 240,
        keep_from_template: false,
    },
    AssetSpec {
        key: "soundtrack",
        rel_path: "romfs/soundtrack.mp3",
        label: "Soundtrack",
        format: "MP3",
        width: 0,
        height: 0,
        keep_from_template: false,
    },
    AssetSpec {
        key: "banner",
        rel_path: "banner.png",
        label: "CIA Banner",
        format: "PNG",
        width: 256,
        height: 128,
        keep_from_template: false,
    },
    AssetSpec {
        key: "icon",
        rel_path: "icon.png",
        label: "Home Menu Icon",
        format: "PNG",
        width: 48,
        height: 48,
        keep_from_template: false,
    },
];

pub fn spec_for(key: &str) -> Option<&'static AssetSpec> {
    ASSET_SPECS.iter().find(|s| s.key == key)
}

pub fn format_requirement(spec: &AssetSpec) -> String {
    if spec.width > 0 && spec.height > 0 {
        format!("{} {}×{}", spec.format, spec.width, spec.height)
    } else {
        spec.format.to_string()
    }
}

pub fn placeholder_marker_path(asset_path: &Path) -> PathBuf {
    let mut name = asset_path
        .file_name()
        .map(|n| n.to_os_string())
        .unwrap_or_default();
    name.push(".studio-placeholder");
    asset_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .join(name)
}

pub fn is_placeholder_asset(asset_path: &Path) -> bool {
    placeholder_marker_path(asset_path).exists()
}

pub fn clear_placeholder_marker(asset_path: &Path) {
    let _ = fs::remove_file(placeholder_marker_path(asset_path));
}

/// After copying the template: keep the default tileset; optional slots get
/// colored-block placeholders (visible in-game, marked until the user imports art).
pub fn prepare_fresh_project_assets(project_root: &Path) -> Result<(), String> {
    let gfx = project_root.join("gfx");
    fs::create_dir_all(&gfx).map_err(|e| e.to_string())?;
    fs::create_dir_all(project_root.join("romfs").join("gfx")).map_err(|e| e.to_string())?;

    let _ = fs::remove_file(gfx.join("DialogueBG.png"));
    let _ = fs::remove_file(project_root.join("romfs").join("soundtrack.mp3"));
    let _ = fs::remove_file(project_root.join("romfs").join("gfx").join("sprites.t3x"));

    for spec in ASSET_SPECS {
        if spec.keep_from_template {
            continue;
        }
        let path = project_root.join(spec.rel_path);
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        if spec.format == "MP3" {
            let _ = fs::remove_file(&path);
            clear_placeholder_marker(&path);
            continue;
        }
        if spec.width == 0 || spec.height == 0 {
            let _ = fs::remove_file(&path);
            clear_placeholder_marker(&path);
            continue;
        }
        write_block_placeholder(&path, spec)?;
    }

    let t3s = "\
--atlas -f rgba8888 -z auto
CardBoard3ds-TileSet.png
BottomMenuScreen.png
LoadGameSelected.png
NewGameSelected.png
SettingsSelected.png
Title.png
Cavebg.png
Cavebg2.png
";
    fs::write(gfx.join("sprites.t3s"), t3s).map_err(|e| e.to_string())?;

    Ok(())
}

pub fn asset_specs_json() -> serde_json::Value {
    let list: Vec<serde_json::Value> = ASSET_SPECS
        .iter()
        .map(|s| {
            serde_json::json!({
                "key": s.key,
                "path": s.rel_path,
                "label": s.label,
                "format": s.format,
                "width": s.width,
                "height": s.height,
                "requirement": format_requirement(s),
                "keep_from_template": s.keep_from_template,
            })
        })
        .collect();
    serde_json::Value::Array(list)
}

fn write_block_placeholder(path: &Path, spec: &AssetSpec) -> Result<(), String> {
    let w = spec.width;
    let h = spec.height;
    let mut px = vec![0u8; (w as usize) * (h as usize) * 4];

    let label = match spec.key {
        "background1" => {
            draw_cave_bg(&mut px, w, h, false);
            "CAVE BG 1"
        }
        "background2" => {
            draw_cave_bg(&mut px, w, h, true);
            "CAVE BG 2"
        }
        "title" => {
            draw_title(&mut px, w, h);
            "TITLE.PNG"
        }
        "bottom_menu" => {
            draw_bottom_menu(&mut px, w, h);
            "BOTTOM MENU"
        }
        "menu_load" => {
            draw_menu_highlight(&mut px, w, h, 0);
            "LOAD BUTTON"
        }
        "menu_new" => {
            draw_menu_highlight(&mut px, w, h, 1);
            "NEW GAME BTN"
        }
        "menu_settings" => {
            draw_menu_highlight(&mut px, w, h, 2);
            "SETTINGS BTN"
        }
        "banner" => {
            draw_banner(&mut px, w, h);
            "CIA BANNER"
        }
        "icon" => {
            draw_icon(&mut px, w, h);
            "ICON"
        }
        _ => {
            fill_rect(&mut px, w, h, 0, 0, w, h, [40, 44, 52, 255]);
            "PLACEHOLDER"
        }
    };

    // Filename hint — only on Studio placeholders (removed when user imports art).
    let file_hint = Path::new(spec.rel_path)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(spec.rel_path);
    draw_asset_labels(&mut px, w, h, label, file_hint);

    write_rgba_png(path, w, h, &px)?;
    fs::write(placeholder_marker_path(path), b"studio-placeholder\n")
        .map_err(|e| format!("marker: {e}"))?;
    Ok(())
}

fn idx(w: u32, x: u32, y: u32) -> usize {
    ((y as usize) * (w as usize) + (x as usize)) * 4
}

fn fill_rect(
    px: &mut [u8],
    w: u32,
    h: u32,
    x0: u32,
    y0: u32,
    rw: u32,
    rh: u32,
    rgba: [u8; 4],
) {
    let x1 = (x0 + rw).min(w);
    let y1 = (y0 + rh).min(h);
    for y in y0..y1 {
        for x in x0..x1 {
            let i = idx(w, x, y);
            px[i..i + 4].copy_from_slice(&rgba);
        }
    }
}

/// Tiny 5×7 glyphs for A–Z, 0–9, space, `.`, `-`, `_`.
fn glyph(ch: char) -> Option<[u8; 7]> {
    // Each byte is a row; low 5 bits are pixels left→right.
    let g: [u8; 7] = match ch.to_ascii_uppercase() {
        'A' => [0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
        'B' => [0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110],
        'C' => [0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110],
        'D' => [0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110],
        'E' => [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],
        'F' => [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000],
        'G' => [0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110],
        'H' => [0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
        'I' => [0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
        'J' => [0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100],
        'K' => [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
        'L' => [0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111],
        'M' => [0b10001, 0b11011, 0b10101, 0b10001, 0b10001, 0b10001, 0b10001],
        'N' => [0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001],
        'O' => [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
        'P' => [0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000],
        'Q' => [0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101],
        'R' => [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],
        'S' => [0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110],
        'T' => [0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100],
        'U' => [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
        'V' => [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100],
        'W' => [0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010],
        'X' => [0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001],
        'Y' => [0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100],
        'Z' => [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111],
        '0' => [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],
        '1' => [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
        '2' => [0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111],
        '3' => [0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110],
        '4' => [0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010],
        '5' => [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
        '6' => [0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
        '7' => [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
        '8' => [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
        '9' => [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110],
        ' ' => [0, 0, 0, 0, 0, 0, 0],
        '.' => [0, 0, 0, 0, 0, 0b00100, 0b00100],
        '-' => [0, 0, 0, 0b11111, 0, 0, 0],
        '_' => [0, 0, 0, 0, 0, 0, 0b11111],
        _ => return None,
    };
    Some(g)
}

fn draw_char(
    px: &mut [u8],
    w: u32,
    h: u32,
    x: u32,
    y: u32,
    ch: char,
    scale: u32,
    rgba: [u8; 4],
) {
    let Some(rows) = glyph(ch) else {
        return;
    };
    for (row_i, bits) in rows.iter().enumerate() {
        for col in 0..5u32 {
            if (bits >> (4 - col)) & 1 == 1 {
                fill_rect(
                    px,
                    w,
                    h,
                    x + col * scale,
                    y + (row_i as u32) * scale,
                    scale,
                    scale,
                    rgba,
                );
            }
        }
    }
}

fn text_width(text: &str, scale: u32) -> u32 {
    let n = text.chars().count() as u32;
    if n == 0 {
        return 0;
    }
    n * (5 * scale + scale) - scale
}

fn draw_text(
    px: &mut [u8],
    w: u32,
    h: u32,
    mut x: u32,
    y: u32,
    text: &str,
    scale: u32,
    rgba: [u8; 4],
) {
    for ch in text.chars() {
        draw_char(px, w, h, x, y, ch, scale, rgba);
        x += 5 * scale + scale; // glyph + 1px gap
    }
}

fn draw_asset_labels(px: &mut [u8], w: u32, h: u32, title: &str, filename: &str) {
    let scale = if w >= 200 && h >= 120 {
        2
    } else if w >= 64 {
        1
    } else {
        1
    };
    let line_h = 7 * scale + scale;
    let pad = 4u32;

    // Opaque label plate so text stays readable on any art.
    let tw = text_width(title, scale).max(text_width(filename, scale));
    let box_w = (tw + pad * 2).min(w.saturating_sub(4));
    let box_h = line_h * 2 + pad * 2;
    let box_x = 2;
    let box_y = 2;
    fill_rect(px, w, h, box_x, box_y, box_w, box_h, [0, 0, 0, 210]);
    draw_text(
        px,
        w,
        h,
        box_x + pad,
        box_y + pad,
        title,
        scale,
        [255, 255, 255, 255],
    );
    draw_text(
        px,
        w,
        h,
        box_x + pad,
        box_y + pad + line_h,
        filename,
        scale,
        [180, 220, 255, 255],
    );
}

fn draw_cave_bg(px: &mut [u8], w: u32, h: u32, alt: bool) {
    let sky = if alt {
        [48, 72, 120, 255]
    } else {
        [36, 52, 88, 255]
    };
    let rock = if alt {
        [96, 78, 64, 255]
    } else {
        [72, 60, 52, 255]
    };
    let accent = if alt {
        [220, 160, 64, 255]
    } else {
        [80, 180, 120, 255]
    };
    fill_rect(px, w, h, 0, 0, w, h, sky);
    // Ground band
    fill_rect(px, w, h, 0, h * 2 / 3, w, h / 3, rock);
    // Block platforms
    fill_rect(px, w, h, 16, h / 2, 48, 16, accent);
    fill_rect(px, w, h, w / 2 - 24, h / 2 + 20, 64, 16, accent);
    fill_rect(px, w, h, w - 72, h / 2 - 10, 48, 16, accent);
    // Simple “cave teeth”
    for i in 0..6 {
        let x = 20 + i * (w / 6);
        fill_rect(px, w, h, x, 0, 18, 28 + (i % 3) * 10, rock);
    }
}

fn draw_title(px: &mut [u8], w: u32, h: u32) {
    // Transparent so menu runner scene shows through; big title bar + blocks.
    fill_rect(px, w, h, 0, 0, w, h, [0, 0, 0, 0]);
    fill_rect(px, w, h, 40, 28, w - 80, 56, [20, 24, 40, 220]);
    // “TITLE” as five block columns
    let colors = [
        [255, 90, 90, 255],
        [255, 200, 80, 255],
        [90, 220, 120, 255],
        [90, 160, 255, 255],
        [200, 120, 255, 255],
    ];
    let bar_y = 40u32;
    let block = 28u32;
    let gap = 12u32;
    let total = 5 * block + 4 * gap;
    let start = (w.saturating_sub(total)) / 2;
    for (i, c) in colors.iter().enumerate() {
        let x = start + (i as u32) * (block + gap);
        fill_rect(px, w, h, x, bar_y, block, block, *c);
    }
    // Subtitle strip
    fill_rect(px, w, h, 80, 100, w - 160, 14, [230, 230, 240, 200]);
}

fn draw_bottom_menu(px: &mut [u8], w: u32, h: u32) {
    fill_rect(px, w, h, 0, 0, w, h, [24, 28, 40, 255]);
    // Top HUD strip
    fill_rect(px, w, h, 0, 0, w, 28, [40, 48, 72, 255]);
    fill_rect(px, w, h, 12, 8, 60, 14, [90, 160, 255, 255]);
    fill_rect(px, w, h, 86, 8, 80, 14, [255, 200, 80, 255]);
    // Center panel
    fill_rect(px, w, h, 24, 50, w - 48, h - 100, [36, 42, 60, 255]);
    // Fake map blocks
    for row in 0..4u32 {
        for col in 0..8u32 {
            if (row + col) % 2 == 0 {
                fill_rect(
                    px,
                    w,
                    h,
                    40 + col * 30,
                    70 + row * 24,
                    24,
                    18,
                    [70, 110, 90, 255],
                );
            }
        }
    }
    // Bottom bar
    fill_rect(px, w, h, 0, h - 36, w, 36, [32, 36, 52, 255]);
}

fn draw_menu_highlight(px: &mut [u8], w: u32, h: u32, which: u32) {
    // Mostly transparent overlay; highlight one of three button slots.
    fill_rect(px, w, h, 0, 0, w, h, [0, 0, 0, 0]);
    let colors = [
        [80, 160, 255, 230],
        [80, 220, 120, 230],
        [255, 180, 70, 230],
    ];
    let labels_w = [100u32, 120, 110];
    let slot_w = w / 3;
    let x = which * slot_w + 20;
    let bw = labels_w[which as usize].min(slot_w - 30);
    fill_rect(px, w, h, x, 150, bw, 36, colors[which as usize]);
    // Corner ticks
    fill_rect(px, w, h, x, 150, 8, 8, [255, 255, 255, 255]);
    fill_rect(px, w, h, x + bw - 8, 150 + 28, 8, 8, [255, 255, 255, 255]);
}

fn draw_banner(px: &mut [u8], w: u32, h: u32) {
    fill_rect(px, w, h, 0, 0, w, h, [30, 34, 50, 255]);
    fill_rect(px, w, h, 8, 8, w - 16, h - 16, [50, 60, 90, 255]);
    let cols = [
        [255, 90, 90, 255],
        [255, 200, 80, 255],
        [90, 220, 120, 255],
        [90, 160, 255, 255],
    ];
    for (i, c) in cols.iter().enumerate() {
        fill_rect(px, w, h, 24 + (i as u32) * 52, 40, 40, 40, *c);
    }
}

fn draw_icon(px: &mut [u8], w: u32, h: u32) {
    fill_rect(px, w, h, 0, 0, w, h, [20, 24, 36, 255]);
    fill_rect(px, w, h, 6, 6, w - 12, h - 12, [60, 140, 220, 255]);
    fill_rect(px, w, h, 14, 14, 12, 12, [255, 220, 80, 255]);
    fill_rect(px, w, h, 28, 28, 12, 12, [90, 220, 120, 255]);
}

fn write_rgba_png(path: &Path, width: u32, height: u32, rgba: &[u8]) -> Result<(), String> {
    let expected = (width as usize) * (height as usize) * 4;
    if rgba.len() != expected {
        return Err(format!(
            "RGBA buffer size mismatch: got {}, expected {}",
            rgba.len(),
            expected
        ));
    }

    let mut raw = Vec::with_capacity((width as usize + 1) * height as usize * 4);
    for y in 0..height as usize {
        raw.push(0); // filter: None
        let row = y * (width as usize) * 4;
        raw.extend_from_slice(&rgba[row..row + (width as usize) * 4]);
    }

    let mut encoder = ZlibEncoder::new(Vec::new(), Compression::fast());
    encoder
        .write_all(&raw)
        .map_err(|e| format!("PNG compress: {}", e))?;
    let compressed = encoder
        .finish()
        .map_err(|e| format!("PNG compress finish: {}", e))?;

    let mut out = Vec::new();
    out.extend_from_slice(&[137, 80, 78, 71, 13, 10, 26, 10]);
    write_chunk(&mut out, b"IHDR", &{
        let mut data = Vec::with_capacity(13);
        data.extend_from_slice(&width.to_be_bytes());
        data.extend_from_slice(&height.to_be_bytes());
        data.extend_from_slice(&[8, 6, 0, 0, 0]);
        data
    });
    write_chunk(&mut out, b"IDAT", &compressed);
    write_chunk(&mut out, b"IEND", &[]);

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    fs::write(path, out).map_err(|e| format!("Write {}: {}", path.display(), e))
}

fn write_chunk(out: &mut Vec<u8>, chunk_type: &[u8; 4], data: &[u8]) {
    out.extend_from_slice(&(data.len() as u32).to_be_bytes());
    out.extend_from_slice(chunk_type);
    out.extend_from_slice(data);
    let mut crc = 0xffff_ffff;
    crc = crc32_update(crc, chunk_type);
    crc = crc32_update(crc, data);
    crc ^= 0xffff_ffff;
    out.extend_from_slice(&crc.to_be_bytes());
}

fn crc32_update(mut crc: u32, data: &[u8]) -> u32 {
    for &b in data {
        crc ^= u32::from(b);
        for _ in 0..8 {
            let mask = if crc & 1 != 0 { 0xedb8_8320u32 } else { 0 };
            crc = (crc >> 1) ^ mask;
        }
    }
    crc
}
