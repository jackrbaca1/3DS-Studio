//! Resolve and persist the local devkitPro install path.

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

const DEFAULT_DEVKITPRO: &str = r"C:\devkitPro";
const CONFIG_DIR_NAME: &str = "3ds-studio";
const CONFIG_FILE_NAME: &str = "config.json";

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct StudioAppConfig {
    /// User-chosen DEVKITPRO root (Windows path). Empty = not set.
    #[serde(default)]
    pub devkitpro_path: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
pub struct ToolchainReport {
    pub ok: bool,
    pub root: String,
    pub source: String,
    pub bash: bool,
    pub gcc: bool,
    pub makerom: bool,
    pub bannertool: bool,
    pub threedslink: bool,
    pub message: String,
}

fn app_config_dir() -> Result<PathBuf, String> {
    let base = std::env::var_os("APPDATA")
        .map(PathBuf::from)
        .ok_or_else(|| "APPDATA not set".to_string())?;
    Ok(base.join(CONFIG_DIR_NAME))
}

pub fn load_app_config() -> StudioAppConfig {
    let Ok(dir) = app_config_dir() else {
        return StudioAppConfig::default();
    };
    let path = dir.join(CONFIG_FILE_NAME);
    let Ok(text) = fs::read_to_string(path) else {
        return StudioAppConfig::default();
    };
    serde_json::from_str(&text).unwrap_or_default()
}

pub fn save_app_config(cfg: &StudioAppConfig) -> Result<(), String> {
    let dir = app_config_dir()?;
    fs::create_dir_all(&dir).map_err(|e| format!("Could not create config dir: {e}"))?;
    let path = dir.join(CONFIG_FILE_NAME);
    let text = serde_json::to_string_pretty(cfg).map_err(|e| e.to_string())?;
    fs::write(path, text).map_err(|e| e.to_string())
}

/// Map env/config strings to a Windows-native path Studio can use.
/// Skips pure Unix roots like `/opt/devkitpro` (common when a shell polluted DEVKITPRO).
fn normalize_devkitpro_candidate(raw: &str) -> Option<PathBuf> {
    let t = raw.trim().trim_end_matches(['/', '\\']);
    if t.is_empty() {
        return None;
    }

    // Already a Windows path (C:\... or C:/...)
    if t.len() >= 2 && t.as_bytes()[1] == b':' {
        return Some(PathBuf::from(t.replace('/', "\\")));
    }

    // MSYS/Git-Bash style: /c/devkitPro → C:\devkitPro
    if let Some(rest) = t
        .strip_prefix("/c/")
        .or_else(|| t.strip_prefix("/C/"))
        .or_else(|| t.strip_prefix("/d/"))
        .or_else(|| t.strip_prefix("/D/"))
    {
        let drive = t.chars().nth(1).unwrap_or('c').to_ascii_uppercase();
        return Some(PathBuf::from(format!(
            "{}:\\{}",
            drive,
            rest.replace('/', "\\")
        )));
    }

    // pacman/Linux default — not usable by the Win32 Studio process
    if t.eq_ignore_ascii_case("/opt/devkitpro") || t.eq_ignore_ascii_case("/opt/devkitPro") {
        return None;
    }

    // Other absolute Unix paths: ignore for Windows native builds
    if t.starts_with('/') {
        return None;
    }

    Some(PathBuf::from(t))
}

fn toolchain_looks_usable(root: &Path) -> bool {
    msys2_bash(root).exists() || arm_gcc(root).exists()
}

/// Resolution order:
/// 1. Saved Studio path (always, when set — even if incomplete, so the wizard can show the bad pick)
/// 2. Usable env `DEVKITPRO`
/// 3. Usable `C:\devkitPro`
/// 4. Fallback to default path string for messaging
///
/// Ignores Unix-only env values (e.g. `/opt/devkitpro`).
pub fn resolve_devkitpro() -> (PathBuf, &'static str) {
    let cfg = load_app_config();
    if let Some(saved) = cfg.devkitpro_path.as_ref() {
        if let Some(p) = normalize_devkitpro_candidate(saved) {
            return (p, "config");
        }
    }

    if let Ok(env) = std::env::var("DEVKITPRO") {
        if let Some(p) = normalize_devkitpro_candidate(&env) {
            if toolchain_looks_usable(&p) {
                return (p, "env");
            }
        }
    }

    let default = PathBuf::from(DEFAULT_DEVKITPRO);
    if toolchain_looks_usable(&default) {
        return (default, "default");
    }

    (PathBuf::from(DEFAULT_DEVKITPRO), "default")
}

pub fn set_devkitpro_path(path: Option<String>) -> Result<(), String> {
    let mut cfg = load_app_config();
    cfg.devkitpro_path = path.and_then(|s| {
        let normalized = normalize_devkitpro_candidate(&s)?;
        Some(normalized.to_string_lossy().into_owned())
    });
    save_app_config(&cfg)
}

pub fn msys2_bash(root: &Path) -> PathBuf {
    root.join("msys2").join("usr").join("bin").join("bash.exe")
}

pub fn arm_gcc(root: &Path) -> PathBuf {
    root.join("devkitARM")
        .join("bin")
        .join("arm-none-eabi-gcc.exe")
}

pub fn tools_bin(root: &Path, name: &str) -> PathBuf {
    root.join("tools").join("bin").join(name)
}

pub fn tool_exists_in_path(tool: &str) -> bool {
    let Some(path_var) = std::env::var_os("PATH") else {
        return false;
    };
    for dir in std::env::split_paths(&path_var) {
        if dir.join(tool).exists() {
            return true;
        }
    }
    false
}

pub fn report() -> ToolchainReport {
    let (root, source) = resolve_devkitpro();
    let root_s = root.display().to_string();
    let bash = msys2_bash(&root).exists();
    let gcc = arm_gcc(&root).exists();
    let makerom =
        tools_bin(&root, "makerom.exe").exists() || tool_exists_in_path("makerom.exe");
    let bannertool =
        tools_bin(&root, "bannertool.exe").exists() || tool_exists_in_path("bannertool.exe");
    let threedslink = tools_bin(&root, "3dslink.exe").exists();

    let env_hint = std::env::var("DEVKITPRO")
        .ok()
        .filter(|v| normalize_devkitpro_candidate(v).is_none())
        .map(|v| {
            format!(
                " (ignored DEVKITPRO={v} — Unix path; using Windows install instead)"
            )
        })
        .unwrap_or_default();

    let ok = bash && gcc;
    let message = if ok {
        format!("devkitPro ready ({source}): {root_s}{env_hint}")
    } else {
        format!(
            "Not a valid DEVKITPRO at {root_s}. Select the folder that contains msys2 and devkitARM (usually C:\\devkitPro).{env_hint}"
        )
    };

    ToolchainReport {
        ok,
        root: root_s,
        source: source.into(),
        bash,
        gcc,
        makerom,
        bannertool,
        threedslink,
        message,
    }
}

pub fn has_cia_tooling(root: &Path, project_root: &Path) -> bool {
    let makerom_ok =
        tools_bin(root, "makerom.exe").exists() || tool_exists_in_path("makerom.exe");
    let bannertool_ok =
        tools_bin(root, "bannertool.exe").exists() || tool_exists_in_path("bannertool.exe");
    let prebuilt_ok = project_root.join("cia-icon.icn").exists()
        && project_root.join("cia-banner.bnr").exists();
    makerom_ok && (bannertool_ok || prebuilt_ok)
}

/// Reject empty paths, NUL, and obvious traversal tricks before shelling out.
pub fn validate_project_path(project_path: &str) -> Result<PathBuf, String> {
    let trimmed = project_path.trim();
    if trimmed.is_empty() {
        return Err("Project path is empty".into());
    }
    if trimmed.contains('\0') {
        return Err("Invalid project path".into());
    }
    let path = PathBuf::from(trimmed);
    let canon = path
        .canonicalize()
        .map_err(|e| format!("Invalid project path: {e}"))?;
    if !canon.is_dir() {
        return Err("Project path is not a directory".into());
    }
    Ok(canon)
}

/// Escape a path for embedding inside single-quoted bash `-c` strings.
pub fn shell_single_quote(s: &str) -> String {
    format!("'{}'", s.replace('\'', "'\\''"))
}
