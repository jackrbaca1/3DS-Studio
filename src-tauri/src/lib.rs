mod project;

use std::fs;
use std::io::{self, BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::mpsc::sync_channel;
use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_dialog::DialogExt;
use serde::{Deserialize, Serialize};

// ============================================================
// Data Types
// ============================================================

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct GameConfig {
    // App Metadata
    pub app_title: String,
    pub app_description: String,
    pub app_author: String,
    // Physics
    pub move_speed: f32,
    pub sprint_speed: f32,
    pub jump_force: f32,
    pub djump_force: f32,
    pub gravity: f32,
    pub gravity_fall: f32,
    pub dash_speed: f32,
    // Feature Toggles
    pub double_jump_enabled: bool,
    pub dialogue_enabled: bool,
    pub wall_jump_enabled: bool,
    pub dash_enabled: bool,
    pub ground_pound_enabled: bool,
}

impl Default for GameConfig {
    fn default() -> Self {
        GameConfig {
            app_title: "My 3DS Platformer".into(),
            app_description: "A 3DS platformer game".into(),
            app_author: "Game Developer".into(),
            move_speed: 3.5,
            sprint_speed: 5.5,
            jump_force: -7.8,
            djump_force: -9.0,
            gravity: 0.48,
            gravity_fall: 0.78,
            dash_speed: 12.0,
            double_jump_enabled: true,
            dialogue_enabled: true,
            wall_jump_enabled: true,
            dash_enabled: true,
            ground_pound_enabled: true,
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ProjectState {
    pub project_path: String,
    pub config: GameConfig,
}

// Global state for the open project path
struct AppState {
    project_path: Mutex<Option<String>>,
}

// ============================================================
// Tauri Commands
// ============================================================

fn dialog_path_to_string(path: tauri_plugin_dialog::FilePath) -> String {
    let fallback = path.to_string();
    match path.into_path() {
        Ok(p) => p.to_string_lossy().into_owned(),
        Err(_) => fallback,
    }
}

/// Native folder picker (non-blocking callback — safe with the Tauri event loop).
#[tauri::command]
async fn pick_directory(
    app: AppHandle,
    title: String,
    default_path: Option<String>,
) -> Result<Option<String>, String> {
    let (tx, rx) = sync_channel(1);
    let mut dialog = app.dialog().file().set_title(&title);
    if let Some(path) = default_path.filter(|p| !p.is_empty()) {
        dialog = dialog.set_directory(path);
    }
    dialog.pick_folder(move |folder| {
        let _ = tx.send(folder);
    });
    let picked = rx
        .recv()
        .map_err(|e| format!("Folder dialog failed: {}", e))?;
    Ok(picked.map(dialog_path_to_string))
}

/// Native file picker for asset import.
#[tauri::command]
async fn pick_file(
    app: AppHandle,
    title: String,
    extensions: Vec<String>,
) -> Result<Option<String>, String> {
    let (tx, rx) = sync_channel(1);
    let mut dialog = app.dialog().file().set_title(&title);
    if !extensions.is_empty() {
        let ext_refs: Vec<&str> = extensions.iter().map(|s| s.as_str()).collect();
        dialog = dialog.add_filter("Files", &ext_refs);
    }
    dialog.pick_file(move |file| {
        let _ = tx.send(file);
    });
    let picked = rx
        .recv()
        .map_err(|e| format!("File dialog failed: {}", e))?;
    Ok(picked.map(dialog_path_to_string))
}

/// Check if devkitARM toolchain is available
#[tauri::command]
fn check_toolchain() -> Result<String, String> {
    let devkitarm = Path::new(r"C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe");
    if devkitarm.exists() {
        Ok(r"C:\devkitPro".into())
    } else {
        Err("devkitARM not found at C:\\devkitPro".into())
    }
}

/// Get the MSYS2 bash path bundled with devkitPro
fn msys2_bash() -> PathBuf {
    PathBuf::from(r"C:\devkitPro\msys2\usr\bin\bash.exe")
}

/// Copy the embedded game template to a new project folder
#[tauri::command]
fn create_project(
    app: AppHandle,
    destination: String,
    config: GameConfig,
) -> Result<(), String> {
    let dest = PathBuf::from(&destination);

    // Create destination directory
    fs::create_dir_all(&dest).map_err(|e| format!("Could not create folder: {}", e))?;

    let template_src = resolve_template_dir(&app)?;
    copy_dir_all(&template_src, &dest)
        .map_err(|e| format!("Failed to copy template from {}: {}", template_src.display(), e))?;

    // Write game_config.h
    write_config_header(&dest, &config)?;

    // Patch Makefile metadata
    patch_makefile(&dest, &config)?;

    Ok(())
}

/// Save config to an existing project
#[tauri::command]
fn save_config(project_path: String, config: GameConfig) -> Result<(), String> {
    let dest = PathBuf::from(&project_path);
    write_config_header(&dest, &config)?;
    patch_makefile(&dest, &config)?;
    Ok(())
}

/// Compile the project, streaming logs back via Tauri events
#[tauri::command]
async fn compile_project(app: AppHandle, project_path: String) -> Result<(), String> {
    let bash = msys2_bash();
    if !bash.exists() {
        return Err("MSYS2 bash not found at C:\\devkitPro\\msys2".into());
    }

    // Convert Windows path to MSYS2 path
    let msys_path = windows_to_msys(&project_path);
    let build_cmd = format!(
        "export DEVKITPRO=/opt/devkitpro && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd '{}' && make -j4 2>&1",
        msys_path
    );

    let _ = app.emit("build-log", "=== Starting Build ===\n");

    let mut child = Command::new(&bash)
        .args(["-l", "-c", &build_cmd])
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn build process: {}", e))?;

    // Stream stdout
    if let Some(stdout) = child.stdout.take() {
        let reader = BufReader::new(stdout);
        for line in reader.lines() {
            if let Ok(l) = line {
                let _ = app.emit("build-log", format!("{}\n", l));
            }
        }
    }

    let status = child.wait().map_err(|e| format!("Build wait error: {}", e))?;

    if status.success() {
        let root = PathBuf::from(&project_path);
        if let Some(path) = project::find_built_3dsx(&root) {
            let _ = app.emit(
                "build-log",
                format!("\n✅ Build succeeded!\nOutput: {}\n", path.display()),
            );
        } else {
            let _ = app.emit("build-log", "\n✅ Build succeeded! (check project folder for .3dsx)\n");
        }
        Ok(())
    } else {
        let _ = app.emit("build-log", "\n❌ Build failed.\n");
        Err("Build failed".into())
    }
}

/// Compile an installable .cia package (requires Makefile cia target/tooling)
#[tauri::command]
async fn compile_project_cia(app: AppHandle, project_path: String) -> Result<(), String> {
    let project_root = PathBuf::from(&project_path);
    if !has_cia_tooling_for_project(&project_root) {
        let _ = app.emit(
            "build-log",
            "❌ CIA tooling missing.\n\
Need makerom.exe in C:\\devkitPro\\tools\\bin (or PATH).\n\
Also need bannertool.exe — OR place prebuilt cia-icon.icn and cia-banner.bnr in the project root.\n\
makerom: https://github.com/3DSGuy/Project_CTR/releases\n\
bannertool (Windows builds): https://github.com/carstene1ns/3ds-bannertool/releases\n",
        );
        return Err(
            "CIA tooling missing. Install makerom (+ bannertool, or prebuilt cia-icon.icn / cia-banner.bnr)."
                .into(),
        );
    }

    if project::ensure_default_banner_wav(&project_root)? {
        let _ = app.emit(
            "build-log",
            "Created default banner.wav (silent placeholder for CIA build).\n",
        );
    }

    let bash = msys2_bash();
    if !bash.exists() {
        return Err("MSYS2 bash not found at C:\\devkitPro\\msys2".into());
    }

    let msys_path = windows_to_msys(&project_path);
    let build_cmd = format!(
        "export DEVKITPRO=/opt/devkitpro && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd '{}' && make -j4 cia 2>&1",
        msys_path
    );

    let _ = app.emit("build-log", "=== Starting CIA Build ===\n");

    let mut child = Command::new(&bash)
        .args(["-l", "-c", &build_cmd])
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn CIA build process: {}", e))?;

    if let Some(stdout) = child.stdout.take() {
        let reader = BufReader::new(stdout);
        for line in reader.lines() {
            if let Ok(l) = line {
                let _ = app.emit("build-log", format!("{}\n", l));
            }
        }
    }

    let status = child.wait().map_err(|e| format!("CIA build wait error: {}", e))?;

    if status.success() {
        let root = PathBuf::from(&project_path);
        if let Some(path) = project::find_built_cia(&root) {
            let _ = app.emit(
                "build-log",
                format!("\n✅ CIA build succeeded!\nOutput: {}\n", path.display()),
            );
        } else {
            let _ = app.emit("build-log", "\n✅ CIA build succeeded! (check project folder for .cia)\n");
        }
        Ok(())
    } else {
        let _ = app.emit("build-log", "\n❌ CIA build failed.\n");
        Err("CIA build failed. Ensure your Makefile supports 'cia' and required tools are installed.".into())
    }
}

/// Clean build artifacts
#[tauri::command]
async fn clean_project(app: AppHandle, project_path: String) -> Result<(), String> {
    let bash = msys2_bash();
    let msys_path = windows_to_msys(&project_path);
    let cmd = format!(
        "export DEVKITPRO=/opt/devkitpro && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd '{}' && make clean 2>&1",
        msys_path
    );
    let _ = app.emit("build-log", "=== Cleaning Build ===\n");
    let output = Command::new(bash)
        .args(["-l", "-c", &cmd])
        .output()
        .map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&output.stdout).to_string()
        + &String::from_utf8_lossy(&output.stderr);
    let _ = app.emit("build-log", text + "\n✅ Clean done.\n");
    Ok(())
}

fn threedslink_exe() -> PathBuf {
    PathBuf::from(r"C:\devkitPro\tools\bin\3dslink.exe")
}

fn tool_exists_in_path(tool: &str) -> bool {
    let path_var = std::env::var_os("PATH");
    let Some(path_var) = path_var else { return false };
    for dir in std::env::split_paths(&path_var) {
        let candidate = dir.join(tool);
        if candidate.exists() {
            return true;
        }
    }
    false
}

fn has_cia_tooling_for_project(project_root: &std::path::Path) -> bool {
    let makerom_default = PathBuf::from(r"C:\devkitPro\tools\bin\makerom.exe");
    let bannertool_default = PathBuf::from(r"C:\devkitPro\tools\bin\bannertool.exe");
    let makerom_ok = makerom_default.exists() || tool_exists_in_path("makerom.exe");
    let bannertool_ok = bannertool_default.exists() || tool_exists_in_path("bannertool.exe");
    let prebuilt_ok = project_root.join("cia-icon.icn").exists()
        && project_root.join("cia-banner.bnr").exists();
    makerom_ok && (bannertool_ok || prebuilt_ok)
}

fn validate_ipv4(ip: &str) -> Result<(), String> {
    let ip = ip.trim();
    let parts: Vec<&str> = ip.split('.').collect();
    if parts.len() != 4 {
        return Err("Enter a valid IPv4 address (e.g. 192.168.1.100)".into());
    }
    for part in parts {
        let n: u16 = part
            .parse()
            .map_err(|_| format!("Invalid IP address: {}", ip))?;
        if n > 255 {
            return Err("IP address octets must be 0–255".into());
        }
    }
    Ok(())
}

/// Send the built .3dsx to a 3DS on the LAN via devkitPro 3dslink (HB Launcher → press Y first).
#[tauri::command]
async fn run_3dslink(app: AppHandle, project_path: String, ip_address: String) -> Result<(), String> {
    validate_ipv4(&ip_address)?;

    let exe = threedslink_exe();
    if !exe.exists() {
        return Err(
            "3dslink not found at C:\\devkitPro\\tools\\bin\\3dslink.exe. Install/update devkitPro tools."
                .into(),
        );
    }

    let root = PathBuf::from(&project_path);
    let threedsx = project::find_built_3dsx(&root).ok_or_else(|| {
        "No .3dsx found in the project. Build first (▶ Build), then run 3dslink.".to_string()
    })?;

    let ip = ip_address.trim();
    let _ = app.emit(
        "build-log",
        format!(
            "=== 3dslink → {} ===\nSending: {}\n\n",
            ip,
            threedsx.display()
        ),
    );

    let output = Command::new(&exe)
        .arg(&threedsx)
        .arg("-a")
        .arg(ip)
        .output()
        .map_err(|e| format!("Failed to run 3dslink: {}", e))?;

    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);
    if !stdout.is_empty() {
        let _ = app.emit("build-log", stdout.to_string());
    }
    if !stderr.is_empty() {
        let _ = app.emit("build-log", stderr.to_string());
    }

    if output.status.success() {
        let _ = app.emit("build-log", "\n✅ 3dslink finished successfully.\n");
        Ok(())
    } else {
        let _ = app.emit("build-log", "\n❌ 3dslink failed.\n");
        Err("3dslink failed".into())
    }
}

/// Open a file manager at the project folder
#[tauri::command]
fn open_project_folder(project_path: String) -> Result<(), String> {
    Command::new("explorer")
        .arg(&project_path)
        .spawn()
        .map_err(|e| e.to_string())?;
    Ok(())
}

/// Copy a custom asset file into the project
#[tauri::command]
fn import_asset(project_path: String, source_path: String, asset_type: String) -> Result<(), String> {
    let project = PathBuf::from(&project_path);
    let source = PathBuf::from(&source_path);

    let dest = match asset_type.as_str() {
        "tileset" => project.join("gfx").join("CardBoard3ds-TileSet.png"),
        "soundtrack" => project.join("romfs").join("soundtrack.mp3"),
        "background1" => project.join("gfx").join("Cavebg.png"),
        "background2" => project.join("gfx").join("Cavebg2.png"),
        "title" => project.join("gfx").join("Title.png"),
        "bottom_menu" => project.join("gfx").join("BottomMenuScreen.png"),
        "menu_load" => project.join("gfx").join("LoadGameSelected.png"),
        "menu_new" => project.join("gfx").join("NewGameSelected.png"),
        "menu_settings" => project.join("gfx").join("SettingsSelected.png"),
        "banner" => project.join("banner.png"),
        "icon" => {
            let mut icon_path = project.join("icon.png");
            if let Ok(makefile) = fs::read_to_string(project.join("Makefile")) {
                for line in makefile.lines() {
                    let trimmed = line.trim();
                    if trimmed.starts_with("TARGET") && trimmed.contains(":=") {
                        if let Some(target) = trimmed.split(":=").nth(1).map(str::trim) {
                            if !target.is_empty() {
                                icon_path = project.join(format!("{target}.png"));
                            }
                        }
                        break;
                    }
                }
            }
            icon_path
        }
        _ => return Err(format!("Unknown asset type: {}", asset_type)),
    };

    fs::copy(&source, &dest).map_err(|e| format!("Copy failed: {}", e))?;

    // Keep icon.png in sync so CIA + 3DSX builds always find the same 48x48 asset.
    if asset_type == "icon" && dest.file_name().and_then(|n| n.to_str()) != Some("icon.png") {
        let icon_copy = project.join("icon.png");
        if icon_copy != dest {
            let _ = fs::copy(&source, &icon_copy);
        }
    }

    // Force CIA icon rebuild after importing a new home menu icon.
    if asset_type == "icon" {
        let _ = fs::remove_file(project.join("cia-icon.icn"));
        let _ = fs::remove_file(project.join("build").join("cia-icon.icn"));
    }

    Ok(())
}

/// Write the levels JSON data to the project's main.cpp (via editor sync block)
#[tauri::command]
fn write_levels(project_path: String, sync_block: String) -> Result<(), String> {
    let cpp_path = PathBuf::from(&project_path).join("source").join("main.cpp");
    let content = fs::read_to_string(&cpp_path)
        .map_err(|e| format!("Could not read main.cpp: {}", e))?;

    let start_marker = "// @@EDITOR_LEVELS_START@@";
    let end_marker = "// @@EDITOR_LEVELS_END@@";

    let start_idx = content.find(start_marker)
        .ok_or("@@EDITOR_LEVELS_START@@ marker not found in main.cpp")?;
    let end_idx = content.find(end_marker)
        .ok_or("@@EDITOR_LEVELS_END@@ marker not found in main.cpp")?;

    let new_content = format!(
        "{}{}{}",
        &content[..start_idx],
        sync_block,
        &content[end_idx + end_marker.len()..]
    );
    let mut new_content = project::repair_main_cpp_level_hidden(&new_content);
    let (feat, _) = project::ensure_per_level_features(&new_content);
    new_content = feat;

    fs::write(&cpp_path, new_content).map_err(|e| format!("Write failed: {}", e))?;
    Ok(())
}

/// Save the project JSON state file for reloading
#[tauri::command]
fn save_project_json(project_path: String, json_data: String) -> Result<(), String> {
    let path = PathBuf::from(&project_path).join("studio_project.json");
    fs::write(path, json_data).map_err(|e| e.to_string())
}

/// Load the project JSON state file
#[tauri::command]
fn load_project_json(project_path: String) -> Result<String, String> {
    let path = PathBuf::from(&project_path).join("studio_project.json");
    fs::read_to_string(path).map_err(|e| e.to_string())
}

/// Check if a project JSON already exists at a path
#[tauri::command]
fn project_exists(project_path: String) -> bool {
    PathBuf::from(&project_path).join("studio_project.json").exists()
}

/// Read a text file from the project workspace (e.g. main.cpp for import)
#[tauri::command]
fn read_text_file(file_path: String) -> Result<String, String> {
    fs::read_to_string(&file_path).map_err(|e| format!("Could not read file: {}", e))
}

/// Read a binary file (e.g. tileset PNG for editor preview)
#[tauri::command]
fn read_binary_file(file_path: String) -> Result<Vec<u8>, String> {
    fs::read(&file_path).map_err(|e| format!("Could not read file: {}", e))
}

/// Validate a devkitPro platformer folder (Makefile + main.cpp + editor markers).
#[tauri::command]
fn inspect_project(project_path: String) -> project::ProjectInspect {
    project::inspect(Path::new(&project_path))
}

/// Load game settings from game_config.h, Makefile, or main.cpp.
#[tauri::command]
fn load_project_config(project_path: String) -> GameConfig {
    project::load_config(Path::new(&project_path))
}

/// One-time patch so main.cpp uses game_config.h (safe to run repeatedly).
#[tauri::command]
fn ensure_studio_integration(project_path: String) -> Result<bool, String> {
    project::ensure_studio_integration(Path::new(&project_path))
}

/// Which bundled assets exist in the project folder.
#[tauri::command]
fn get_asset_status(project_path: String) -> serde_json::Value {
    project::list_asset_status(Path::new(&project_path))
}

// ============================================================
// Helpers
// ============================================================

fn resolve_template_dir(app: &AppHandle) -> Result<PathBuf, String> {
    let resource_path = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Resource dir error: {}", e))?;
    let bundled = resource_path.join("template");
    if bundled.exists() {
        return Ok(bundled);
    }

    if let Ok(manifest) = std::env::var("CARGO_MANIFEST_DIR") {
        let dev = PathBuf::from(manifest).join("template");
        if dev.exists() {
            return Ok(dev);
        }
    }

    let exe_dir = std::env::current_exe()
        .map_err(|e| e.to_string())?
        .parent()
        .map(|p| p.to_path_buf())
        .ok_or("No exe parent")?;
    let alt = exe_dir.join("template");
    if alt.exists() {
        return Ok(alt);
    }

    Err("Game template not found. Reinstall 3DS Studio or restore src-tauri/template.".into())
}

fn windows_to_msys(win_path: &str) -> String {
    // Convert C:\Foo\Bar -> /c/Foo/Bar
    let p = win_path.replace('\\', "/");
    if let Some(rest) = p.strip_prefix("C:/").or_else(|| p.strip_prefix("c:/")) {
        format!("/c/{}", rest)
    } else if p.len() >= 2 && p.as_bytes()[1] == b':' {
        let drive = p.chars().next().unwrap().to_lowercase().to_string();
        format!("/{}/{}", drive, &p[3..])
    } else {
        p
    }
}

fn copy_dir_all(src: &Path, dst: &Path) -> io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        let dest_path = dst.join(entry.file_name());
        if ty.is_dir() {
            copy_dir_all(&entry.path(), &dest_path)?;
        } else {
            fs::copy(entry.path(), &dest_path)?;
        }
    }
    Ok(())
}

fn write_config_header(project_root: &Path, config: &GameConfig) -> Result<(), String> {
    let header_path = project_root.join("source").join("game_config.h");

    let wall_jump_line = if config.wall_jump_enabled {
        "static constexpr bool WALL_JUMP_ENABLED = true;"
    } else {
        "static constexpr bool WALL_JUMP_ENABLED = false;"
    };

    let dash_line = if config.dash_enabled {
        "static constexpr bool DASH_ENABLED = true;"
    } else {
        "static constexpr bool DASH_ENABLED = false;"
    };

    let gpound_line = if config.ground_pound_enabled {
        "static constexpr bool GROUND_POUND_ENABLED = true;"
    } else {
        "static constexpr bool GROUND_POUND_ENABLED = false;"
    };

    let content = format!(
        r#"// game_config.h — Auto-generated by 3DS Studio. Do not edit manually.
#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

// ── Physics ──────────────────────────────────────────────
static constexpr float GC_MOVE_SPEED   = {move_speed:.2}f;
static constexpr float GC_SPRINT_SPEED = {sprint_speed:.2}f;
static constexpr float GC_JUMP_FORCE   = {jump_force:.2}f;
static constexpr float GC_DJUMP_FORCE  = {djump_force:.2}f;
static constexpr float GC_GRAVITY      = {gravity:.2}f;
static constexpr float GC_GRAVITY_FALL = {gravity_fall:.2}f;
static constexpr float GC_DASH_SPEED   = {dash_speed:.2}f;

// ── Feature Toggles ──────────────────────────────────────
static constexpr bool DOUBLE_JUMP_ENABLED   = {double_jump};
static constexpr bool DIALOGUE_ENABLED      = {dialogue};
{wall_jump}
{dash}
{gpound}

#endif // GAME_CONFIG_H
"#,
        move_speed = config.move_speed,
        sprint_speed = config.sprint_speed,
        jump_force = config.jump_force,
        djump_force = config.djump_force,
        gravity = config.gravity,
        gravity_fall = config.gravity_fall,
        dash_speed = config.dash_speed,
        double_jump = config.double_jump_enabled,
        dialogue = config.dialogue_enabled,
        wall_jump = wall_jump_line,
        dash = dash_line,
        gpound = gpound_line,
    );

    fs::write(&header_path, content).map_err(|e| format!("Could not write game_config.h: {}", e))
}

fn patch_makefile(project_root: &Path, config: &GameConfig) -> Result<(), String> {
    let makefile_path = project_root.join("Makefile");
    let content = fs::read_to_string(&makefile_path)
        .map_err(|e| format!("Could not read Makefile: {}", e))?;

    // Replace APP_TITLE, APP_DESCRIPTION, APP_AUTHOR lines
    let new_content = {
        let mut result = String::new();
        for line in content.lines() {
            if line.starts_with("APP_TITLE") && line.contains(":=") {
                result.push_str(&format!("APP_TITLE\t\t:=\t{}\n", config.app_title));
            } else if line.starts_with("APP_DESCRIPTION") && line.contains(":=") {
                result.push_str(&format!("APP_DESCRIPTION\t:=\t{}\n", config.app_description));
            } else if line.starts_with("APP_AUTHOR") && line.contains(":=") {
                result.push_str(&format!("APP_AUTHOR\t\t:=\t{}\n", config.app_author));
            } else {
                result.push_str(line);
                result.push('\n');
            }
        }
        result
    };

    fs::write(&makefile_path, new_content)
        .map_err(|e| format!("Could not write Makefile: {}", e))
}

// ============================================================
// App Entry Point
// ============================================================

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .manage(AppState {
            project_path: Mutex::new(None),
        })
        .invoke_handler(tauri::generate_handler![
            pick_directory,
            pick_file,
            check_toolchain,
            create_project,
            save_config,
            compile_project,
            compile_project_cia,
            clean_project,
            run_3dslink,
            open_project_folder,
            import_asset,
            write_levels,
            save_project_json,
            load_project_json,
            project_exists,
            read_text_file,
            read_binary_file,
            inspect_project,
            load_project_config,
            ensure_studio_integration,
            get_asset_status,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
