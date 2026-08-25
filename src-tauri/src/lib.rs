mod assets;
mod project;
mod toolchain;

use std::fs;
use std::io::{self, BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::mpsc::sync_channel;
use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};
use tauri_plugin_dialog::DialogExt;
use tauri_plugin_opener::OpenerExt;
use serde::{Deserialize, Serialize};

// ============================================================
// Data Types
// ============================================================

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(default)]
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

/// Check if devkitARM toolchain is available (env → saved config → C:\devkitPro).
#[tauri::command]
fn check_toolchain() -> Result<toolchain::ToolchainReport, String> {
    let report = toolchain::report();
    if report.ok {
        Ok(report)
    } else {
        Err(report.message)
    }
}

/// Persist a custom DEVKITPRO root (empty clears to env/default).
/// Always returns the fresh report (may still be incomplete — wizard can retest).
#[tauri::command]
fn set_toolchain_path(path: Option<String>) -> Result<toolchain::ToolchainReport, String> {
    toolchain::set_devkitpro_path(path)?;
    Ok(toolchain::report())
}

/// Current resolved toolchain report without failing when incomplete.
#[tauri::command]
fn get_toolchain_status() -> toolchain::ToolchainReport {
    toolchain::report()
}

/// Open an allowlisted https URL in the system browser (install docs only).
#[tauri::command]
fn open_external_url(app: AppHandle, url: String) -> Result<(), String> {
    const ALLOWED: &[&str] = &[
        "https://devkitpro.org/",
        "https://github.com/3DSGuy/",
        "https://github.com/carstene1ns/",
    ];
    let trimmed = url.trim();
    if !ALLOWED.iter().any(|prefix| trimmed.starts_with(prefix)) {
        return Err("That URL is not allowed from Studio.".into());
    }
    app.opener()
        .open_url(trimmed, None::<&str>)
        .map_err(|e| e.to_string())
}

/// Copy the embedded game template to a new project folder
#[tauri::command]
fn create_project(
    app: AppHandle,
    destination: String,
    config: GameConfig,
) -> Result<(), String> {
    materialize_project(&app, &destination, &config)
}

/// Create (or optionally reset) the starter example under Documents/3DS Studio.
/// Returns `{ path, created }` — `created` is true when the folder was newly materialized.
#[tauri::command]
fn ensure_example_project(
    app: AppHandle,
    reset: Option<bool>,
) -> Result<serde_json::Value, String> {
    let reset = reset.unwrap_or(false);
    let dest = studio_library_dir(&app)?.join("ExamplePlatformer");
    let dest_str = dest.to_string_lossy().into_owned();

    if reset && dest.exists() {
        fs::remove_dir_all(&dest).map_err(|e| {
            format!(
                "Could not reset example project at {}:\n{}",
                dest.display(),
                e
            )
        })?;
    }

    let mut created = false;
    if !project::inspect(&dest).valid {
        if dest.exists() {
            let has_entries = fs::read_dir(&dest)
                .map(|mut d| d.next().is_some())
                .unwrap_or(false);
            if has_entries {
                return Err(format!(
                    "Folder exists but is not a Studio project:\n{}\nMove or rename it, then try again.",
                    dest.display()
                ));
            }
        }

        let config = GameConfig {
            app_title: "ExamplePlatformer".into(),
            app_description: "Starter example for 3DS Studio".into(),
            app_author: "3DS Studio".into(),
            ..GameConfig::default()
        };
        materialize_project(&app, &dest_str, &config)?;
        created = true;
    }

    Ok(serde_json::json!({
        "path": dest_str,
        "created": created,
    }))
}

/// List valid Studio projects in Documents/3DSStudio (and legacy Documents/3DS Studio).
#[tauri::command]
fn list_studio_projects(app: AppHandle) -> Result<Vec<StudioProjectEntry>, String> {
    let primary = studio_library_dir(&app)?;
    fs::create_dir_all(&primary).map_err(|e| e.to_string())?;

    let mut roots = vec![primary];
    if let Ok(docs) = app.path().document_dir() {
        let legacy = docs.join("3DS Studio");
        if legacy.exists() && legacy != roots[0] {
            roots.push(legacy);
        }
    }

    let mut entries = Vec::new();
    let mut seen = std::collections::HashSet::new();
    for root in roots {
        let Ok(read) = fs::read_dir(&root) else {
            continue;
        };
        for entry in read {
            let entry = entry.map_err(|e| e.to_string())?;
            let path = entry.path();
            if !path.is_dir() {
                continue;
            }
            let inspect = project::inspect(&path);
            if !inspect.valid {
                continue;
            }
            let key = path.to_string_lossy().to_ascii_lowercase();
            if !seen.insert(key) {
                continue;
            }
            let name = path
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_else(|| path.display().to_string());
            let modified = entry
                .metadata()
                .ok()
                .and_then(|m| m.modified().ok())
                .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
                .map(|d| d.as_secs())
                .unwrap_or(0);
            entries.push(StudioProjectEntry {
                name,
                path: path.to_string_lossy().into_owned(),
                modified,
            });
        }
    }

    entries.sort_by(|a, b| {
        b.modified
            .cmp(&a.modified)
            .then_with(|| a.name.to_lowercase().cmp(&b.name.to_lowercase()))
    });
    Ok(entries)
}

fn ensure_path_under_studio_library(app: &AppHandle, path: &Path) -> Result<(), String> {
    let canonical = path
        .canonicalize()
        .map_err(|e| format!("Could not resolve project path: {e}"))?;

    let mut allowed: Vec<PathBuf> = Vec::new();
    let primary = studio_library_dir(app)?;
    allowed.push(
        primary
            .canonicalize()
            .unwrap_or(primary),
    );
    if let Ok(docs) = app.path().document_dir() {
        let legacy = docs.join("3DS Studio");
        if let Ok(c) = legacy.canonicalize() {
            allowed.push(c);
        }
    }
    let under = allowed.iter().any(|root| canonical.starts_with(root));
    if !under {
        return Err(
            "Only projects under Documents/3DSStudio can be renamed or deleted from Studio."
                .into(),
        );
    }
    Ok(())
}

/// Rename a library project folder (Documents/3DSStudio only).
#[tauri::command]
fn rename_studio_project(
    app: AppHandle,
    project_path: String,
    new_name: String,
) -> Result<serde_json::Value, String> {
    let src = toolchain::validate_project_path(&project_path)?;
    if !project::inspect(&src).valid {
        return Err("That folder is not a valid Studio project.".into());
    }
    ensure_path_under_studio_library(&app, &src)?;

    let project_name = sanitize_project_name(&new_name)?;
    let parent = src
        .parent()
        .ok_or_else(|| "Invalid project path.".to_string())?;
    let dest = parent.join(&project_name);
    if dest.exists() {
        return Err(format!(
            "A project named \"{project_name}\" already exists. Choose another name."
        ));
    }

    fs::rename(&src, &dest).map_err(|e| format!("Could not rename project: {e}"))?;
    Ok(serde_json::json!({
        "path": dest.to_string_lossy(),
        "name": project_name,
        "old_path": src.to_string_lossy(),
    }))
}

/// Permanently delete a library project folder (Documents/3DSStudio only).
#[tauri::command]
fn delete_studio_project(app: AppHandle, project_path: String) -> Result<(), String> {
    let src = toolchain::validate_project_path(&project_path)?;
    if !project::inspect(&src).valid {
        return Err("That folder is not a valid Studio project.".into());
    }
    ensure_path_under_studio_library(&app, &src)?;
    fs::remove_dir_all(&src).map_err(|e| format!("Could not delete project: {e}"))?;
    Ok(())
}

/// Copy the open project into Documents/3DS Studio/<name> (creative-software Save As).
#[tauri::command]
fn save_project_as(
    app: AppHandle,
    source_path: String,
    name: String,
    overwrite: Option<bool>,
) -> Result<serde_json::Value, String> {
    let overwrite = overwrite.unwrap_or(false);
    let src = toolchain::validate_project_path(&source_path)?;
    if !project::inspect(&src).valid {
        return Err("Source folder is not a valid Studio project.".into());
    }

    let project_name = sanitize_project_name(&name)?;
    let dest = studio_library_dir(&app)?.join(&project_name);
    if dest.exists() {
        if !overwrite {
            return Err(format!(
                "A project named \"{}\" already exists. Choose another name or overwrite.",
                project_name
            ));
        }
        fs::remove_dir_all(&dest).map_err(|e| format!("Could not overwrite: {}", e))?;
    }

    fs::create_dir_all(dest.parent().unwrap_or(Path::new(".")))
        .map_err(|e| e.to_string())?;
    copy_project_tree(&src, &dest).map_err(|e| format!("Save As copy failed: {}", e))?;

    let mut config = project::load_config(&dest);
    config.app_title = project_name.clone();
    let meta = ProjectMeta::create_new(&project_name);
    meta.write(&dest)?;
    write_config_header(&dest, &config, &meta.save_prefix)?;
    patch_makefile(&dest, &config, Some(&meta.unique_id))?;

    Ok(serde_json::json!({
        "path": dest.to_string_lossy(),
        "name": project_name,
    }))
}

/// Create a new named project in the Studio library (no folder picker).
#[tauri::command]
fn create_named_project(
    app: AppHandle,
    name: String,
    config: GameConfig,
    overwrite: Option<bool>,
) -> Result<serde_json::Value, String> {
    let overwrite = overwrite.unwrap_or(false);
    let project_name = sanitize_project_name(&name)?;
    let dest = studio_library_dir(&app)?.join(&project_name);
    let dest_str = dest.to_string_lossy().into_owned();

    if dest.exists() {
        if !overwrite {
            return Err(format!(
                "A project named \"{}\" already exists. Choose another name or overwrite.",
                project_name
            ));
        }
        fs::remove_dir_all(&dest).map_err(|e| format!("Could not overwrite: {}", e))?;
    }

    let mut cfg = config;
    if cfg.app_title.trim().is_empty() || cfg.app_title == GameConfig::default().app_title {
        cfg.app_title = project_name.clone();
    }
    materialize_project(&app, &dest_str, &cfg)?;
    Ok(serde_json::json!({
        "path": dest_str,
        "name": project_name,
    }))
}

fn studio_library_dir(app: &AppHandle) -> Result<PathBuf, String> {
    let docs = app
        .path()
        .document_dir()
        .map_err(|e| format!("Could not resolve Documents folder: {}", e))?;
    // No spaces: GNU make / devkitPro break on paths like "3DS Studio/My Game".
    Ok(docs.join("3DSStudio"))
}

fn sanitize_project_name(raw: &str) -> Result<String, String> {
    let name = raw.trim();
    if name.is_empty() {
        return Err("Enter a project name.".into());
    }
    if name.len() > 64 {
        return Err("Project name is too long (max 64 characters).".into());
    }
    if name == "." || name == ".." {
        return Err("Invalid project name.".into());
    }
    if name.chars().any(|c| c.is_whitespace()) {
        return Err(
            "Project name cannot contain spaces (devkitPro make requires a space-free path). Use CamelCase or underscores."
                .into(),
        );
    }
    if name
        .chars()
        .any(|c| matches!(c, '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|') || c.is_control())
    {
        return Err("Project name cannot contain / \\ : * ? \" < > |".into());
    }
    let lower = name.to_ascii_lowercase();
    if matches!(
        lower.as_str(),
        "con" | "prn" | "aux" | "nul" | "com1" | "lpt1"
    ) {
        return Err("That name is reserved by Windows.".into());
    }
    Ok(name.to_string())
}

#[derive(Serialize)]
struct StudioProjectEntry {
    name: String,
    path: String,
    modified: u64,
}

fn materialize_project(
    app: &AppHandle,
    destination: &str,
    config: &GameConfig,
) -> Result<(), String> {
    let dest = PathBuf::from(destination.trim());
    if destination.trim().is_empty() || destination.contains('\0') {
        return Err("Invalid destination path".into());
    }

    fs::create_dir_all(&dest).map_err(|e| format!("Could not create folder: {}", e))?;

    let template_src = resolve_template_dir(app)?;
    copy_dir_all(&template_src, &dest)
        .map_err(|e| format!("Failed to copy template from {}: {}", template_src.display(), e))?;

    assets::prepare_fresh_project_assets(&dest)?;

    let meta = ProjectMeta::create_new(&config.app_title);
    meta.write(&dest)?;

    write_config_header(&dest, config, &meta.save_prefix)?;
    patch_makefile(&dest, config, Some(&meta.unique_id))?;

    Ok(())
}

/// Save config to an existing project
#[tauri::command]
fn save_config(project_path: String, config: GameConfig) -> Result<(), String> {
    let dest = toolchain::validate_project_path(&project_path)?;
    let meta = ProjectMeta::load_or_create(&dest, &config.app_title)?;
    write_config_header(&dest, &config, &meta.save_prefix)?;
    patch_makefile(&dest, &config, Some(&meta.unique_id))?;
    Ok(())
}

/// Compile the project, streaming logs back via Tauri events
#[tauri::command]
async fn compile_project(app: AppHandle, project_path: String) -> Result<(), String> {
    let _project = toolchain::validate_project_path(&project_path)?;
    let (root, source) = toolchain::resolve_devkitpro();
    let bash = toolchain::msys2_bash(&root);
    if !bash.exists() {
        return Err(format!(
            "MSYS2 bash not found under {}\\msys2 (resolved via {source})",
            root.display()
        ));
    }

    let msys_path = windows_to_msys(&project_path);
    let dk_msys = windows_to_msys(&root.display().to_string());
    let quoted = toolchain::shell_single_quote(&msys_path);
    let build_cmd = format!(
        "export DEVKITPRO={dk} && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd {quoted} && make -j4 2>&1",
        dk = toolchain::shell_single_quote(&dk_msys),
        quoted = quoted
    );

    let _ = app.emit(
        "build-log",
        format!(
            "=== Starting Build ===\nDEVKITPRO={} ({source})\n",
            root.display()
        ),
    );

    let mut child = Command::new(&bash)
        .args(["-l", "-c", &build_cmd])
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn build process: {}", e))?;

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
        let root_proj = PathBuf::from(&project_path);
        if let Some(path) = project::find_built_3dsx(&root_proj) {
            let _ = app.emit(
                "build-log",
                format!(
                    "\n✅ Build succeeded!\nLoadable file (project root, not build/):\n  {}\n",
                    path.display()
                ),
            );
        } else {
            let _ = app.emit(
                "build-log",
                "\n✅ Build succeeded, but no .3dsx found in the project root.\nLook next to Makefile (go up one level from build/).\n",
            );
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
    let project_root = toolchain::validate_project_path(&project_path)?;
    let (dk_root, source) = toolchain::resolve_devkitpro();
    if !toolchain::has_cia_tooling(&dk_root, &project_root) {
        let tools = dk_root.join("tools").join("bin");
        let _ = app.emit(
            "build-log",
            format!(
                "❌ CIA tooling missing.\n\
Need makerom.exe in {tools} (or PATH).\n\
Also need bannertool.exe — OR place prebuilt cia-icon.icn and cia-banner.bnr in the project root.\n\
makerom: https://github.com/3DSGuy/Project_CTR/releases\n\
bannertool (Windows builds): https://github.com/carstene1ns/3ds-bannertool/releases\n\
Resolved DEVKITPRO: {} ({source})\n",
                dk_root.display(),
                tools = tools.display()
            ),
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

    let bash = toolchain::msys2_bash(&dk_root);
    if !bash.exists() {
        return Err(format!(
            "MSYS2 bash not found under {}\\msys2",
            dk_root.display()
        ));
    }

    let msys_path = windows_to_msys(&project_path);
    let dk_msys = windows_to_msys(&dk_root.display().to_string());
    let quoted = toolchain::shell_single_quote(&msys_path);
    let build_cmd = format!(
        "export DEVKITPRO={dk} && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd {quoted} && make -j4 cia 2>&1",
        dk = toolchain::shell_single_quote(&dk_msys),
        quoted = quoted
    );

    let _ = app.emit(
        "build-log",
        format!(
            "=== Starting CIA Build ===\nDEVKITPRO={} ({source})\n",
            dk_root.display()
        ),
    );

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
        if let Some(path) = project::find_built_cia(&project_root) {
            let _ = app.emit(
                "build-log",
                format!(
                    "\n✅ CIA build succeeded!\nLoadable file (project root, not build/):\n  {}\n",
                    path.display()
                ),
            );
        } else {
            let _ = app.emit(
                "build-log",
                "\n✅ CIA build succeeded, but no .cia found in the project root.\nLook next to Makefile (go up one level from build/).\n",
            );
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
    let _project = toolchain::validate_project_path(&project_path)?;
    let (root, source) = toolchain::resolve_devkitpro();
    let bash = toolchain::msys2_bash(&root);
    if !bash.exists() {
        return Err(format!("MSYS2 bash not found under {}", root.display()));
    }
    let msys_path = windows_to_msys(&project_path);
    let dk_msys = windows_to_msys(&root.display().to_string());
    let quoted = toolchain::shell_single_quote(&msys_path);
    let cmd = format!(
        "export DEVKITPRO={dk} && export DEVKITARM=${{DEVKITPRO}}/devkitARM && export PATH=${{DEVKITPRO}}/tools/bin:${{DEVKITPRO}}/devkitARM/bin:$PATH && cd {quoted} && make clean 2>&1",
        dk = toolchain::shell_single_quote(&dk_msys),
        quoted = quoted
    );
    let _ = app.emit(
        "build-log",
        format!(
            "=== Cleaning Build ===\nDEVKITPRO={} ({source})\n",
            root.display()
        ),
    );
    let output = Command::new(bash)
        .args(["-l", "-c", &cmd])
        .output()
        .map_err(|e| e.to_string())?;
    let text = String::from_utf8_lossy(&output.stdout).to_string()
        + &String::from_utf8_lossy(&output.stderr);
    let _ = app.emit("build-log", text + "\n✅ Clean done.\n");
    Ok(())
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
    let root_proj = toolchain::validate_project_path(&project_path)?;
    let (dk_root, _) = toolchain::resolve_devkitpro();
    let exe = toolchain::tools_bin(&dk_root, "3dslink.exe");
    if !exe.exists() {
        return Err(format!(
            "3dslink not found at {}. Install/update the 3ds-dev tools package.",
            exe.display()
        ));
    }

    let threedsx = project::find_built_3dsx(&root_proj).ok_or_else(|| {
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

    if let Some(parent) = dest.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("Could not create asset folder: {}", e))?;
    }

    fs::copy(&source, &dest).map_err(|e| format!("Copy failed: {}", e))?;
    assets::clear_placeholder_marker(&dest);

    // Keep icon.png in sync so CIA + 3DSX builds always find the same 48x48 asset.
    if asset_type == "icon" && dest.file_name().and_then(|n| n.to_str()) != Some("icon.png") {
        let icon_copy = project.join("icon.png");
        if icon_copy != dest {
            let _ = fs::copy(&source, &icon_copy);
            assets::clear_placeholder_marker(&icon_copy);
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
    let project = PathBuf::from(&project_path);
    let source_dir = project.join("source");
    fs::create_dir_all(&source_dir).map_err(|e| {
        format!(
            "Could not create source/ under {}:\n{}",
            project.display(),
            e
        )
    })?;
    let cpp_path = source_dir.join("main.cpp");
    let content = fs::read_to_string(&cpp_path).map_err(|e| {
        format!(
            "Could not read main.cpp at {}:\n{}\nRe-open the project from Projects, or Start Fresh Example.",
            cpp_path.display(),
            e
        )
    })?;

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

/// Static import requirements (format / pixel size) for each asset slot.
#[tauri::command]
fn get_asset_specs() -> serde_json::Value {
    assets::asset_specs_json()
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
        let name = entry.file_name();
        let name_str = name.to_string_lossy();
        // Never copy another project's editor save into a new folder.
        if name_str == "studio_project.json" || name_str == "studio_meta.json" {
            continue;
        }
        if name_str.ends_with(".studio-placeholder") {
            continue;
        }
        let ty = entry.file_type()?;
        let dest_path = dst.join(&name);
        if ty.is_dir() {
            copy_dir_all(&entry.path(), &dest_path)?;
        } else {
            fs::copy(entry.path(), &dest_path)?;
        }
    }
    Ok(())
}

/// Copy a live project for Save As — keeps levels JSON, skips build junk.
fn copy_project_tree(src: &Path, dst: &Path) -> io::Result<()> {
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let name = entry.file_name();
        let name_str = name.to_string_lossy();
        if matches!(
            name_str.as_ref(),
            "build" | "target" | ".git" | "studio_meta.json"
        ) {
            continue;
        }
        let ty = entry.file_type()?;
        let dest_path = dst.join(&name);
        if ty.is_dir() {
            copy_project_tree(&entry.path(), &dest_path)?;
        } else {
            fs::copy(entry.path(), &dest_path)?;
        }
    }
    Ok(())
}

fn write_config_header(
    project_root: &Path,
    config: &GameConfig,
    save_prefix: &str,
) -> Result<(), String> {
    let source_dir = project_root.join("source");
    fs::create_dir_all(&source_dir).map_err(|e| {
        format!(
            "Could not create source/ under {}:\n{}",
            project_root.display(),
            e
        )
    })?;
    let header_path = source_dir.join("game_config.h");

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

    let prefix = sanitize_save_prefix(save_prefix);

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

// ── Saves (sdmc:/3ds/<prefix>_*.dat) ─────────────────────
static constexpr const char* GC_SAVE_PREFIX = "{prefix}";

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
        prefix = prefix,
        double_jump = config.double_jump_enabled,
        dialogue = config.dialogue_enabled,
        wall_jump = wall_jump_line,
        dash = dash_line,
        gpound = gpound_line,
    );

    fs::write(&header_path, content).map_err(|e| {
        format!(
            "Could not write game_config.h at {}:\n{}",
            header_path.display(),
            e
        )
    })
}

fn patch_makefile(
    project_root: &Path,
    config: &GameConfig,
    unique_id: Option<&str>,
) -> Result<(), String> {
    let makefile_path = project_root.join("Makefile");
    let content = fs::read_to_string(&makefile_path)
        .map_err(|e| format!("Could not read Makefile: {}", e))?;

    let mut has_unique_var = false;
    let mut result = String::new();
    for line in content.lines() {
        if line.starts_with("APP_TITLE") && line.contains(":=") {
            result.push_str(&format!("APP_TITLE\t\t:=\t{}\n", config.app_title));
        } else if line.starts_with("APP_DESCRIPTION") && line.contains(":=") {
            result.push_str(&format!("APP_DESCRIPTION\t:=\t{}\n", config.app_description));
        } else if line.starts_with("APP_AUTHOR") && line.contains(":=") {
            result.push_str(&format!("APP_AUTHOR\t\t:=\t{}\n", config.app_author));
        } else if line.starts_with("APP_UNIQUE_ID") && line.contains(":=") {
            has_unique_var = true;
            if let Some(id) = unique_id {
                result.push_str(&format!("APP_UNIQUE_ID\t:=\t{}\n", id));
            } else {
                result.push_str(line);
                result.push('\n');
            }
        } else if line.contains("-DAPP_UNIQUE_ID=") {
            // Prefer Makefile variable form so UniqueId can differ per project.
            result.push_str("\t\t-DAPP_UNIQUE_ID=$(APP_UNIQUE_ID)\n");
        } else {
            result.push_str(line);
            result.push('\n');
        }
    }

    if !has_unique_var {
        if let Some(id) = unique_id {
            // Insert after APP_AUTHOR block
            let insert = format!("APP_UNIQUE_ID\t:=\t{}\n", id);
            if let Some(pos) = result.find("APP_AUTHOR") {
                if let Some(nl) = result[pos..].find('\n') {
                    let at = pos + nl + 1;
                    result.insert_str(at, &insert);
                } else {
                    result.push_str(&insert);
                }
            } else {
                result.insert_str(0, &insert);
            }
        }
    }

    fs::write(&makefile_path, result).map_err(|e| format!("Could not write Makefile: {}", e))
}

fn extract_unique_id_arg(line: &str) -> Option<String> {
    let key = "-DAPP_UNIQUE_ID=";
    let idx = line.find(key)?;
    let rest = &line[idx + key.len()..];
    let end = rest
        .find(|c: char| c.is_whitespace() || c == '"')
        .unwrap_or(rest.len());
    Some(rest[..end].to_string())
}

fn sanitize_save_prefix(raw: &str) -> String {
    let mut out: String = raw
        .chars()
        .filter(|c| c.is_ascii_alphanumeric())
        .map(|c| c.to_ascii_lowercase())
        .take(12)
        .collect();
    if out.is_empty() {
        out = "game".into();
    }
    out
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct ProjectMeta {
    unique_id: String,
    save_prefix: String,
}

impl ProjectMeta {
    fn path(root: &Path) -> PathBuf {
        root.join("studio_meta.json")
    }

    fn create_new(app_title: &str) -> Self {
        Self {
            unique_id: generate_unique_id(),
            save_prefix: slug_from_title(app_title),
        }
    }

    fn write(&self, root: &Path) -> Result<(), String> {
        let text = serde_json::to_string_pretty(self).map_err(|e| e.to_string())?;
        fs::write(Self::path(root), text).map_err(|e| e.to_string())
    }

    fn load_or_create(root: &Path, app_title: &str) -> Result<Self, String> {
        let path = Self::path(root);
        if path.exists() {
            let text = fs::read_to_string(&path).map_err(|e| e.to_string())?;
            if let Ok(meta) = serde_json::from_str::<ProjectMeta>(&text) {
                if !meta.unique_id.is_empty() && !meta.save_prefix.is_empty() {
                    return Ok(meta);
                }
            }
        }
        // Recover unique id from Makefile if present
        let mut meta = Self::create_new(app_title);
        if let Ok(makefile) = fs::read_to_string(root.join("Makefile")) {
            for line in makefile.lines() {
                if line.starts_with("APP_UNIQUE_ID") && line.contains(":=") {
                    if let Some(id) = line.split(":=").nth(1).map(str::trim) {
                        if id.starts_with("0x") || id.starts_with("0X") {
                            meta.unique_id = id.to_string();
                        }
                    }
                }
                if let Some(arg) = extract_unique_id_arg(line) {
                    if arg.starts_with("0x") || arg.starts_with("0X") {
                        meta.unique_id = arg;
                    }
                }
            }
        }
        meta.write(root)?;
        Ok(meta)
    }
}

fn slug_from_title(title: &str) -> String {
    sanitize_save_prefix(title)
}

fn generate_unique_id() -> String {
    use std::time::{SystemTime, UNIX_EPOCH};
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_nanos() as u32)
        .unwrap_or(0x1234);
    // Homebrew-style 20-bit id in 0xF2000..0xFEFFF (avoid stock template 0xF1234).
    let id = 0xF2000 | (nanos & 0x0CFFF);
    format!("0x{id:X}")
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
            get_toolchain_status,
            set_toolchain_path,
            open_external_url,
            create_project,
            ensure_example_project,
            list_studio_projects,
            rename_studio_project,
            delete_studio_project,
            save_project_as,
            create_named_project,
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
            get_asset_specs,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
