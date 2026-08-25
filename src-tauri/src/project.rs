use crate::GameConfig;
use std::path::{Path, PathBuf};

#[derive(serde::Serialize)]
pub struct ProjectInspect {
    pub valid: bool,
    pub has_editor_markers: bool,
    pub has_game_config: bool,
    pub has_makefile: bool,
    pub has_main_cpp: bool,
    pub target_name: Option<String>,
    pub message: String,
}

pub fn inspect(project_root: &Path) -> ProjectInspect {
    let makefile = project_root.join("Makefile");
    let main_cpp = project_root.join("source").join("main.cpp");
    let game_config = project_root.join("source").join("game_config.h");

    let has_makefile = makefile.exists();
    let has_main_cpp = main_cpp.exists();
    let has_game_config = game_config.exists();

    let mut has_editor_markers = false;
    if has_main_cpp {
        if let Ok(content) = std::fs::read_to_string(&main_cpp) {
            has_editor_markers = content.contains("@@EDITOR_LEVELS_START@@")
                && content.contains("@@EDITOR_LEVELS_END@@");
        }
    }

    let target_name = if has_makefile {
        std::fs::read_to_string(&makefile)
            .ok()
            .map(|c| resolve_make_target(project_root, &c))
    } else {
        None
    };

    let valid = has_makefile && has_main_cpp && has_editor_markers;
    let message = if !has_makefile {
        "Missing Makefile — open a devkitPro 3DS game folder.".into()
    } else if !has_main_cpp {
        "Missing source/main.cpp.".into()
    } else if !has_editor_markers {
        "main.cpp is missing @@EDITOR_LEVELS_START@@ / @@EDITOR_LEVELS_END@@ markers. Sync levels from the legacy editor first.".into()
    } else {
        "Ready.".into()
    };

    ProjectInspect {
        valid,
        has_editor_markers,
        has_game_config,
        has_makefile,
        has_main_cpp,
        target_name,
        message,
    }
}

pub fn load_config(project_root: &Path) -> GameConfig {
    let game_config_path = project_root.join("source").join("game_config.h");
    let mut cfg = if game_config_path.exists() {
        std::fs::read_to_string(&game_config_path)
            .ok()
            .and_then(|content| parse_game_config_h(&content))
            .unwrap_or_default()
    } else {
        GameConfig::default()
    };

    let makefile_path = project_root.join("Makefile");
    if let Ok(content) = std::fs::read_to_string(&makefile_path) {
        if let Some(title) = parse_makefile_value(&content, "APP_TITLE") {
            cfg.app_title = title;
        }
        if let Some(desc) = parse_makefile_value(&content, "APP_DESCRIPTION") {
            cfg.app_description = desc;
        }
        if let Some(author) = parse_makefile_value(&content, "APP_AUTHOR") {
            cfg.app_author = author;
        }
    }

    let main_cpp_path = project_root.join("source").join("main.cpp");
    if let Ok(content) = std::fs::read_to_string(&main_cpp_path) {
        apply_main_cpp_physics(&content, &mut cfg);
    }

    // studio_project.json config wins (written by 3DS Studio autosave / Save)
    let json_path = project_root.join("studio_project.json");
    if json_path.exists() {
        if let Ok(content) = std::fs::read_to_string(&json_path) {
            if let Ok(v) = serde_json::from_str::<serde_json::Value>(&content) {
                if let Some(studio) = v.get("config") {
                    if let Ok(merged) = serde_json::from_value::<GameConfig>(studio.clone()) {
                        cfg = merged;
                    }
                }
            }
        }
    }

    cfg
}

/// Patch main.cpp so physics/dialogue/dash respect game_config.h (idempotent).
pub fn ensure_studio_integration(project_root: &Path) -> Result<bool, String> {
    let main_path = project_root.join("source").join("main.cpp");
    let mut content = std::fs::read_to_string(&main_path)
        .map_err(|e| format!("Could not read main.cpp: {}", e))?;
    let original = content.clone();
    let mut modified = false;

    if !content.contains("#include \"game_config.h\"") {
        if let Some(idx) = content.find("#include \"minimp3.h\"") {
            let line_end = content[idx..]
                .find('\n')
                .map(|i| idx + i + 1)
                .unwrap_or(idx);
            content.insert_str(line_end, "#include \"game_config.h\"\n");
            modified = true;
        }
    }

    if content.contains("static constexpr float GRAVITY       = 0.48f")
        || content.contains("static constexpr float GRAVITY       = GC_GRAVITY")
    {
        if content.contains("static constexpr float GRAVITY       = 0.48f") {
            content = content.replace(
                "// ========================================================\n// Physics constants\n// ========================================================\nstatic constexpr float GRAVITY       = 0.48f;   // rising gravity (hold jump)\nstatic constexpr float GRAVITY_FALL  = 0.78f;   // falling / jump released gravity\nstatic constexpr float JUMP_FORCE    = -7.8f;   // stronger to compensate heavier gravity\nstatic constexpr float DJUMP_FORCE   = -9.0f;   // double jump — about 1 tile higher than normal\nstatic constexpr float JUMP_CUT_MUL  = 0.35f;   // multiply vy when jump released early\nstatic constexpr float MOVE_SPEED    = 3.5f;\nstatic constexpr float SPRINT_SPEED  = 5.5f;",
                "// ========================================================\n// Physics constants (tuned in 3DS Studio via game_config.h)\n// ========================================================\nstatic constexpr float GRAVITY       = GC_GRAVITY;\nstatic constexpr float GRAVITY_FALL  = GC_GRAVITY_FALL;\nstatic constexpr float JUMP_FORCE    = GC_JUMP_FORCE;\nstatic constexpr float DJUMP_FORCE   = GC_DJUMP_FORCE;\nstatic constexpr float JUMP_CUT_MUL  = 0.35f;   // multiply vy when jump released early\nstatic constexpr float MOVE_SPEED    = GC_MOVE_SPEED;\nstatic constexpr float SPRINT_SPEED  = GC_SPRINT_SPEED;",
            );
            modified = true;
        }
        if content.contains("static constexpr float DASH_SPEED    = 12.0f") {
            content = content.replace(
                "static constexpr float DASH_SPEED    = 12.0f;   // horizontal dash speed",
                "static constexpr float DASH_SPEED    = GC_DASH_SPEED;",
            );
            modified = true;
        }
    }

    let patches: &[(&str, &str)] = &[
        (
            "if (dialoguePreCount[currentLevel] > 0 && strlen(dialoguePre[currentLevel][0]) > 0)",
            "if (DIALOGUE_ENABLED && dialoguePreCount[currentLevel] > 0 && strlen(dialoguePre[currentLevel][0]) > 0)",
        ),
        (
            "if (dialoguePostCount[currentLevel] > 0 && strlen(dialoguePost[currentLevel][0]) > 0)",
            "if (DIALOGUE_ENABLED && dialoguePostCount[currentLevel] > 0 && strlen(dialoguePost[currentLevel][0]) > 0)",
        ),
        (
            "if ((kDown & KEY_DOWN) && !player.onGround && !player.groundPound)",
            "if (GROUND_POUND_ENABLED && (kDown & KEY_DOWN) && !player.onGround && !player.groundPound)",
        ),
        (
            "if ((kDown & KEY_R) && !player.groundPound && player.dashCooldown <= 0)",
            "if (DASH_ENABLED && (kDown & KEY_R) && !player.groundPound && player.dashCooldown <= 0)",
        ),
        (
            "} else if (player.wallLeft || player.wallRight)",
            "} else if (WALL_JUMP_ENABLED && (player.wallLeft || player.wallRight))",
        ),
        (
            "} else if (worldHasDoubleJump && !player.doubleJumpUsed && !player.onGround)",
            "} else if (DOUBLE_JUMP_ENABLED && worldHasDoubleJump && !player.doubleJumpUsed && !player.onGround)",
        ),
    ];

    for (from, to) in patches {
        if content.contains(from) && !content.contains(to) {
            content = content.replace(from, to);
            modified = true;
        }
    }

    let (feat_patched, feat_modified) = ensure_per_level_features(&content);
    content = feat_patched;
    if feat_modified {
        modified = true;
    }

    let repaired = repair_main_cpp_level_hidden(&content);
    if repaired != content {
        content = repaired;
        modified = true;
    }

    let (dim_patched, dim_modified) = ensure_active_map_dimensions(&content);
    content = dim_patched;
    if dim_modified {
        modified = true;
    }

    let (phys_patched, phys_modified) = ensure_level_physics(&content);
    content = phys_patched;
    if phys_modified {
        modified = true;
    }

    if modified && content != original {
        std::fs::write(&main_path, &content)
            .map_err(|e| format!("Could not write main.cpp: {}", e))?;
    }

    Ok(modified)
}

/// Upgrade main.cpp for per-level feature flags in LevelInfo (idempotent).
pub fn ensure_per_level_features(content: &str) -> (String, bool) {
    let mut s = content.to_string();
    let mut modified = false;

    if s.contains("static constexpr int MAP_H = 32;") && !s.contains("MAP_H = 128") {
        s = s.replace(
            "static constexpr int MAP_H = 32;",
            "static constexpr int MAP_H = 128; // max tiles tall — per-level height from LEVEL_INFO",
        );
        modified = true;
    }

    if !s.contains("bool doubleJump") {
        const OLD: &str = "\tint height;\r\n};";
        const NEW: &str = "\tint height;\r\n\tbool doubleJump;\r\n\tbool dialogue;\r\n\tbool wallJump;\r\n\tbool dash;\r\n\tbool groundPound;\r\n\tbool minimap;\r\n};";
        if s.contains(OLD) {
            s = s.replace(OLD, NEW);
            modified = true;
        } else if s.contains("\tint height;\n};") {
            s = s.replace(
                "\tint height;\n};",
                "\tint height;\n\tbool doubleJump;\n\tbool dialogue;\n\tbool wallJump;\n\tbool dash;\n\tbool groundPound;\n\tbool minimap;\n};",
            );
            modified = true;
        }
    }

    if s.contains("bool groundPound") && !s.contains("bool minimap") {
        if s.contains("\tbool groundPound;\r\n};") {
            s = s.replace(
                "\tbool groundPound;\r\n};",
                "\tbool groundPound;\r\n\tbool minimap;\r\n};",
            );
            modified = true;
        } else if s.contains("\tbool groundPound;\n};") {
            s = s.replace(
                "\tbool groundPound;\n};",
                "\tbool groundPound;\n\tbool minimap;\n};",
            );
            modified = true;
        }
    }

    if !s.contains("static bool levelAllowsMinimap") {
        if s.contains("static bool levelAllowsDialogue = true;\r\n") {
            s = s.replace(
                "static bool levelAllowsDialogue = true;\r\n",
                "static bool levelAllowsDialogue = true;\r\nstatic bool levelAllowsMinimap = true;\r\n",
            );
            modified = true;
        } else if s.contains("static bool levelAllowsDialogue = true;\n") {
            s = s.replace(
                "static bool levelAllowsDialogue = true;\n",
                "static bool levelAllowsDialogue = true;\nstatic bool levelAllowsMinimap = true;\n",
            );
            modified = true;
        }
    }

    if s.contains("levelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;")
        && !s.contains("levelAllowsMinimap = li.minimap")
    {
        s = s.replace(
            "\t\tlevelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;\r\n\t} else {",
            "\t\tlevelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;\r\n\t\tlevelAllowsMinimap = li.minimap;\r\n\t} else {",
        );
        s = s.replace(
            "\t\tlevelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;\n\t} else {",
            "\t\tlevelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;\n\t\tlevelAllowsMinimap = li.minimap;\n\t} else {",
        );
        s = s.replace(
            "\t\tlevelAllowsDialogue = false;\r\n\t}",
            "\t\tlevelAllowsDialogue = false;\r\n\t\tlevelAllowsMinimap = true;\r\n\t}",
        );
        s = s.replace(
            "\t\tlevelAllowsDialogue = false;\n\t}",
            "\t\tlevelAllowsDialogue = false;\n\t\tlevelAllowsMinimap = true;\n\t}",
        );
        modified = true;
    }

    if s.contains("player.rect.x < 0) { player.rect.x = 0; player.vx = 0; }")
        && !s.contains("mapRight")
    {
        s = s.replace(
            "\t// Horizontal movement + collision\r\n\tplayer.rect.x += player.vx;\r\n\tif (player.rect.x < 0) { player.rect.x = 0; player.vx = 0; }\r\n\tresolveHorizontal();",
            "\t// Horizontal movement + collision\r\n\t{\r\n\t\tfloat mapRight = (float)getActiveMapWidth() * TILE_SIZE;\r\n\t\tplayer.rect.x += player.vx;\r\n\t\tif (player.rect.x < 0.0f) { player.rect.x = 0.0f; player.vx = 0.0f; }\r\n\t\tif (player.rect.x + player.rect.w > mapRight) {\r\n\t\t\tplayer.rect.x = mapRight - player.rect.w;\r\n\t\t\tplayer.vx = 0.0f;\r\n\t\t}\r\n\t}\r\n\tresolveHorizontal();",
        );
        s = s.replace(
            "\t// Horizontal movement + collision\n\tplayer.rect.x += player.vx;\n\tif (player.rect.x < 0) { player.rect.x = 0; player.vx = 0; }\n\tresolveHorizontal();",
            "\t// Horizontal movement + collision\n\t{\n\t\tfloat mapRight = (float)getActiveMapWidth() * TILE_SIZE;\n\t\tplayer.rect.x += player.vx;\n\t\tif (player.rect.x < 0.0f) { player.rect.x = 0.0f; player.vx = 0.0f; }\n\t\tif (player.rect.x + player.rect.w > mapRight) {\n\t\t\tplayer.rect.x = mapRight - player.rect.w;\n\t\t\tplayer.vx = 0.0f;\n\t\t}\n\t}\n\tresolveHorizontal();",
        );
        modified = true;
    }

    if s.contains("drawMinimap(mapX, mapY, mapScale);")
        && !s.contains("if (levelAllowsMinimap)")
    {
        s = s.replace(
            "\t// Minimap\r\n\tfloat mapScale = (float)(BOT_WIDTH - 20) / MAP_W;",
            "\t// Minimap (per-level toggle)\r\n\tif (levelAllowsMinimap) {\r\n\t\tfloat mapScale = (float)(BOT_WIDTH - 20) / (float)getActiveMapWidth();",
        );
        s = s.replace(
            "\t// Minimap\n\tfloat mapScale = (float)(BOT_WIDTH - 20) / MAP_W;",
            "\t// Minimap (per-level toggle)\n\tif (levelAllowsMinimap) {\n\t\tfloat mapScale = (float)(BOT_WIDTH - 20) / (float)getActiveMapWidth();",
        );
        s = s.replace(
            "\tfloat mapW = MAP_W * mapScale;",
            "\t\tfloat mapW = (float)getActiveMapWidth() * mapScale;",
        );
        s = s.replace(
            "\tfloat mapH = getActiveMapHeight() * mapScale;",
            "\t\tfloat mapH = (float)getActiveMapHeight() * mapScale;",
        );
        s = s.replace(
            "\tfloat mapX = (BOT_WIDTH - mapW) * 0.5f;",
            "\t\tfloat mapX = (BOT_WIDTH - mapW) * 0.5f;",
        );
        s = s.replace(
            "\tfloat mapY = 200.0f - mapH - 5.0f;",
            "\t\tfloat mapY = 200.0f - mapH - 5.0f;",
        );
        s = s.replace(
            "\tC2D_DrawRectSolid(mapX - 1, mapY - 1, 0.1f, mapW + 2, mapH + 2, C2D_Color32(40, 40, 60, 200));\r\n\tdrawMinimap(mapX, mapY, mapScale);\r\n}",
            "\t\tC2D_DrawRectSolid(mapX - 1, mapY - 1, 0.1f, mapW + 2, mapH + 2, C2D_Color32(40, 40, 60, 200));\r\n\t\tdrawMinimap(mapX, mapY, mapScale);\r\n\t}\r\n}",
        );
        s = s.replace(
            "\tC2D_DrawRectSolid(mapX - 1, mapY - 1, 0.1f, mapW + 2, mapH + 2, C2D_Color32(40, 40, 60, 200));\n\tdrawMinimap(mapX, mapY, mapScale);\n}",
            "\t\tC2D_DrawRectSolid(mapX - 1, mapY - 1, 0.1f, mapW + 2, mapH + 2, C2D_Color32(40, 40, 60, 200));\n\t\tdrawMinimap(mapX, mapY, mapScale);\n\t}\n}",
        );
        modified = true;
    }

    if s.contains("static bool worldHasDoubleJump") {
        s = s.replace(
            "// World / double jump config\nstatic bool worldHasDoubleJump = false; // true for world 2+",
            "// Per-level feature gates (from LEVEL_INFO; set in initLevel)\nstatic bool levelAllowsDoubleJump = false;\nstatic bool levelAllowsWallJump = true;\nstatic bool levelAllowsDash = true;\nstatic bool levelAllowsGroundPound = true;\nstatic bool levelAllowsDialogue = true;",
        );
        modified = true;
    }

    if s.contains("worldHasDoubleJump = (currentLevel >= 6)") {
        s = s.replace(
            "\tworldHasDoubleJump = (currentLevel >= 6);",
            "\tif (currentLevel >= 0 && currentLevel < LEVEL_COUNT) {\n\t\tconst LevelInfo& li = LEVEL_INFO[currentLevel];\n\t\tlevelAllowsDoubleJump = DOUBLE_JUMP_ENABLED && li.doubleJump;\n\t\tlevelAllowsWallJump = WALL_JUMP_ENABLED && li.wallJump;\n\t\tlevelAllowsDash = DASH_ENABLED && li.dash;\n\t\tlevelAllowsGroundPound = GROUND_POUND_ENABLED && li.groundPound;\n\t\tlevelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;\n\t\tlevelAllowsMinimap = li.minimap;\n\t} else {\n\t\tlevelAllowsDoubleJump = false;\n\t\tlevelAllowsWallJump = false;\n\t\tlevelAllowsDash = false;\n\t\tlevelAllowsGroundPound = false;\n\t\tlevelAllowsDialogue = false;\n\t\tlevelAllowsMinimap = true;\n\t}",
        );
        modified = true;
    }

    let replacements: &[(&str, &str)] = &[
        (
            "if (DIALOGUE_ENABLED && dialoguePreCount[currentLevel] > 0 && strlen(dialoguePre[currentLevel][0]) > 0)",
            "if (levelAllowsDialogue && dialoguePreCount[currentLevel] > 0 && strlen(dialoguePre[currentLevel][0]) > 0)",
        ),
        (
            "if (DIALOGUE_ENABLED && dialoguePostCount[currentLevel] > 0 && strlen(dialoguePost[currentLevel][0]) > 0)",
            "if (levelAllowsDialogue && dialoguePostCount[currentLevel] > 0 && strlen(dialoguePost[currentLevel][0]) > 0)",
        ),
        (
            "} else if (DOUBLE_JUMP_ENABLED && worldHasDoubleJump && !player.doubleJumpUsed && !player.onGround)",
            "} else if (levelAllowsDoubleJump && !player.doubleJumpUsed && !player.onGround)",
        ),
        (
            "} else if (DOUBLE_JUMP_ENABLED && levelAllowsDoubleJump && !player.doubleJumpUsed && !player.onGround)",
            "} else if (levelAllowsDoubleJump && !player.doubleJumpUsed && !player.onGround)",
        ),
        (
            "if (GROUND_POUND_ENABLED && (kDown & KEY_DOWN) && !player.onGround && !player.groundPound)",
            "if (levelAllowsGroundPound && (kDown & KEY_DOWN) && !player.onGround && !player.groundPound)",
        ),
        (
            "if (DASH_ENABLED && (kDown & KEY_R) && !player.groundPound && player.dashCooldown <= 0)",
            "if (levelAllowsDash && (kDown & KEY_R) && !player.groundPound && player.dashCooldown <= 0)",
        ),
        (
            "} else if (WALL_JUMP_ENABLED && (player.wallLeft || player.wallRight))",
            "} else if (levelAllowsWallJump && (player.wallLeft || player.wallRight))",
        ),
    ];

    for (from, to) in replacements {
        if s.contains(from) && !s.contains(to) {
            s = s.replace(from, to);
            modified = true;
        }
    }

    (s, modified)
}

/// Runtime physics + per-level LEVEL_PHYSICS_TABLE (idempotent).
pub fn ensure_level_physics(content: &str) -> (String, bool) {
    let mut s = content.to_string();
    let mut modified = false;

    let replacements: &[(&str, &str)] = &[
        ("static constexpr float GRAVITY       = GC_GRAVITY;", "static float GRAVITY       = GC_GRAVITY;"),
        ("static constexpr float GRAVITY_FALL  = GC_GRAVITY_FALL;", "static float GRAVITY_FALL  = GC_GRAVITY_FALL;"),
        ("static constexpr float JUMP_FORCE    = GC_JUMP_FORCE;", "static float JUMP_FORCE    = GC_JUMP_FORCE;"),
        ("static constexpr float DJUMP_FORCE   = GC_DJUMP_FORCE;", "static float DJUMP_FORCE   = GC_DJUMP_FORCE;"),
        ("static constexpr float MOVE_SPEED    = GC_MOVE_SPEED;", "static float MOVE_SPEED    = GC_MOVE_SPEED;"),
        ("static constexpr float SPRINT_SPEED  = GC_SPRINT_SPEED;", "static float SPRINT_SPEED  = GC_SPRINT_SPEED;"),
        ("static constexpr float DASH_SPEED    = GC_DASH_SPEED;", "static float DASH_SPEED    = GC_DASH_SPEED;"),
    ];
    for (from, to) in replacements {
        if s.contains(from) && !s.contains(to) {
            s = s.replace(from, to);
            modified = true;
        }
    }

    if !s.contains("struct LevelPhysics") {
        const INSERT: &str = r#"
// Per-level physics overrides (custom == true uses values below instead of game_config.h)
struct LevelPhysics {
	bool custom;
	float moveSpeed;
	float sprintSpeed;
	float jumpForce;
	float djumpForce;
	float gravity;
	float gravityFall;
	float dashSpeed;
};

#ifndef LEVEL_PHYSICS_TABLE
static const LevelPhysics FALLBACK_LEVEL_PHYSICS[TOTAL_LEVELS] = {};
#define LEVEL_PHYSICS_TABLE FALLBACK_LEVEL_PHYSICS
#endif

static void applyLevelPhysicsForLevel(int idx);

"#;
        if let Some(pos) = s.find("// Fallback LEVEL_INFO") {
            s.insert_str(pos, INSERT);
            modified = true;
        }
    }

    if !s.contains("applyLevelPhysicsForLevel(currentLevel)") {
        if s.contains("\t\tlevelAllowsMinimap = true;\n\t}\n\n\tdetectStationaryEnemies();") {
            s = s.replace(
                "\t\tlevelAllowsMinimap = true;\n\t}\n\n\tdetectStationaryEnemies();",
                "\t\tlevelAllowsMinimap = true;\n\t}\n\n\tapplyLevelPhysicsForLevel(currentLevel);\n\n\tdetectStationaryEnemies();",
            );
            modified = true;
        } else if s.contains("\t\tlevelAllowsDialogue = false;\n\t}\n\n\tdetectStationaryEnemies();") {
            s = s.replace(
                "\t\tlevelAllowsDialogue = false;\n\t}\n\n\tdetectStationaryEnemies();",
                "\t\tlevelAllowsDialogue = false;\n\t}\n\n\tapplyLevelPhysicsForLevel(currentLevel);\n\n\tdetectStationaryEnemies();",
            );
            modified = true;
        }
    }

    let (repaired, repair_modified) = repair_apply_level_physics_placement(&s);
    s = repaired;
    if repair_modified {
        modified = true;
    }

    (s, modified)
}

const APPLY_LEVEL_PHYSICS_IMPL: &str = r#"
#ifndef STUDIO_APPLY_LEVEL_PHYSICS_DEFINED
#define STUDIO_APPLY_LEVEL_PHYSICS_DEFINED
static void applyLevelPhysicsForLevel(int idx) {
	GRAVITY = GC_GRAVITY;
	GRAVITY_FALL = GC_GRAVITY_FALL;
	JUMP_FORCE = GC_JUMP_FORCE;
	DJUMP_FORCE = GC_DJUMP_FORCE;
	MOVE_SPEED = GC_MOVE_SPEED;
	SPRINT_SPEED = GC_SPRINT_SPEED;
	DASH_SPEED = GC_DASH_SPEED;
	if (idx < 0 || idx >= LEVEL_COUNT) return;
	const LevelPhysics& p = LEVEL_PHYSICS_TABLE[idx];
	if (!p.custom) return;
	MOVE_SPEED = p.moveSpeed;
	SPRINT_SPEED = p.sprintSpeed;
	JUMP_FORCE = p.jumpForce;
	DJUMP_FORCE = p.djumpForce;
	GRAVITY = p.gravity;
	GRAVITY_FALL = p.gravityFall;
	DASH_SPEED = p.dashSpeed;
}
#endif
"#;

/// Move applyLevelPhysicsForLevel below LEVEL_COUNT / LEVEL_PHYSICS_TABLE (fixes compile error).
pub fn repair_apply_level_physics_placement(content: &str) -> (String, bool) {
    let mut s = content.to_string();
    let mut modified = false;

    const EARLY_FN: &str = "static void applyLevelPhysicsForLevel(int idx) {\n\tGRAVITY = GC_GRAVITY;\n\tGRAVITY_FALL = GC_GRAVITY_FALL;\n\tJUMP_FORCE = GC_JUMP_FORCE;\n\tDJUMP_FORCE = GC_DJUMP_FORCE;\n\tMOVE_SPEED = GC_MOVE_SPEED;\n\tSPRINT_SPEED = GC_SPRINT_SPEED;\n\tDASH_SPEED = GC_DASH_SPEED;\n\tif (idx < 0 || idx >= LEVEL_COUNT) return;\n\tconst LevelPhysics& p = LEVEL_PHYSICS_TABLE[idx];\n\tif (!p.custom) return;\n\tMOVE_SPEED = p.moveSpeed;\n\tSPRINT_SPEED = p.sprintSpeed;\n\tJUMP_FORCE = p.jumpForce;\n\tDJUMP_FORCE = p.djumpForce;\n\tGRAVITY = p.gravity;\n\tGRAVITY_FALL = p.gravityFall;\n\tDASH_SPEED = p.dashSpeed;\n}\n\n";

    if s.contains(EARLY_FN) {
        s = s.replace(EARLY_FN, "static void applyLevelPhysicsForLevel(int idx);\n\n");
        modified = true;
    }

    if !s.contains("STUDIO_APPLY_LEVEL_PHYSICS_DEFINED") {
        let anchors = [
            "\treturn MAP_W;\n}\n\n\n\n// Secret levels:",
            "\treturn MAP_W;\n}\n\n// Secret levels:",
            "\treturn MAP_W;\n}\n\n\n// Secret levels:",
            "\treturn MAP_W;\n}\n\n// Menu scene",
        ];
        for anchor in anchors {
            if let Some(pos) = s.find(anchor) {
                let insert_at = pos + anchor.find("//").unwrap_or(anchor.len());
                let line_start = s[..insert_at].rfind('\n').map(|i| i + 1).unwrap_or(insert_at);
                s.insert_str(line_start, APPLY_LEVEL_PHYSICS_IMPL);
                s.push('\n');
                modified = true;
                break;
            }
        }
    }

    if !s.contains("STUDIO_APPLY_LEVEL_PHYSICS_DEFINED")
        && s.contains("#define LEVEL_COUNT (LEVEL_COUNT_MACRO)")
        && !s.contains("EDITOR_LEVEL_COUNT")
    {
        let anchor = "#define LEVEL_INFO (LEVEL_INFO_MACRO)";
        if let Some(pos) = s.find(anchor) {
            let insert_at = pos + anchor.len();
            s.insert_str(insert_at, "\n");
            s.insert_str(insert_at + 1, APPLY_LEVEL_PHYSICS_IMPL);
            s.push('\n');
            modified = true;
        }
    }

    (s, modified)
}

const ACTIVE_MAP_DIMS_BLOCK: &str = r#"
#undef LEVEL_COUNT
#undef LEVEL_INFO
#define LEVEL_COUNT EDITOR_LEVEL_COUNT
#define LEVEL_INFO  EDITOR_LEVEL_INFO

static int getActiveMapHeight() {
	if (currentLevel >= 0 && currentLevel < EDITOR_LEVEL_COUNT)
		return EDITOR_LEVEL_INFO[currentLevel].height;
	if (currentLevel >= 0 && currentLevel < TOTAL_LEVELS)
		return FALLBACK_LEVEL_INFO[currentLevel].height;
	return MAP_H;
}

static int getActiveMapWidth() {
	if (currentLevel >= 0 && currentLevel < EDITOR_LEVEL_COUNT)
		return EDITOR_LEVEL_INFO[currentLevel].width;
	if (currentLevel >= 0 && currentLevel < TOTAL_LEVELS)
		return FALLBACK_LEVEL_INFO[currentLevel].width;
	return MAP_W;
}
"#;

/// Re-insert per-level width/height helpers after editor export (must live after EDITOR_LEVEL_INFO).
pub fn ensure_active_map_dimensions(content: &str) -> (String, bool) {
    if !content.contains("EDITOR_LEVEL_INFO") {
        return (content.to_string(), false);
    }
    if content.contains("return EDITOR_LEVEL_INFO[currentLevel].height") {
        return (content.to_string(), false);
    }

    let anchor = "#define LEVEL_INFO_MACRO EDITOR_LEVEL_INFO";
    let Some(pos) = content.find(anchor) else {
        return (content.to_string(), false);
    };
    let insert_at = pos + anchor.len();
    let mut s = String::with_capacity(content.len() + ACTIVE_MAP_DIMS_BLOCK.len() + 4);
    s.push_str(&content[..insert_at]);
    s.push_str(ACTIVE_MAP_DIMS_BLOCK);
    s.push_str(&content[insert_at..]);
    (s, true)
}

/// Fix duplicate `levelHidden[]` definitions after editor export (uses LEVEL_INFO[i].isHidden).
pub fn repair_main_cpp_level_hidden(content: &str) -> String {
    if !content.contains("EDITOR_LEVEL_INFO") {
        return content.to_string();
    }

    let mut s = remove_level_hidden_array_def(content, "EDITOR_LEVEL_COUNT");
    s = remove_level_hidden_array_def(&s, "TOTAL_LEVELS");
    s = replace_level_hidden_index_access(&s);
    s
}

fn remove_level_hidden_array_def(content: &str, size_name: &str) -> String {
    let needle = format!("static bool levelHidden[{size_name}]");
    let Some(mut start) = content.find(&needle) else {
        return content.to_string();
    };

    if let Some(comment_start) = content[..start].rfind("\n// Which levels are hidden") {
        start = comment_start + 1;
    }

    let Some(rel_end) = content[start..].find("};") else {
        return content.to_string();
    };
    let mut end = start + rel_end + 2;
    while end < content.len() {
        let ch = content.as_bytes()[end];
        if ch == b'\n' || ch == b'\r' {
            end += 1;
        } else {
            break;
        }
    }

    let mut out = String::with_capacity(content.len());
    out.push_str(&content[..start]);
    out.push_str(&content[end..]);
    out
}

fn replace_level_hidden_index_access(content: &str) -> String {
    const PREFIX: &str = "levelHidden[";
    let mut out = String::with_capacity(content.len());
    let mut rest = content;
    while let Some(pos) = rest.find(PREFIX) {
        out.push_str(&rest[..pos]);
        let after = &rest[pos + PREFIX.len()..];
        let Some(bracket) = after.find(']') else {
            out.push_str(PREFIX);
            rest = after;
            continue;
        };
        let index = &after[..bracket];
        out.push_str("LEVEL_INFO[");
        out.push_str(index);
        out.push_str("].isHidden");
        rest = &after[bracket + 1..];
    }
    out.push_str(rest);
    out
}

pub fn find_built_3dsx(project_root: &Path) -> Option<PathBuf> {
    let makefile = std::fs::read_to_string(project_root.join("Makefile")).ok()?;
    let target = resolve_make_target(project_root, &makefile);

    for path in [
        project_root.join(format!("{target}.3dsx")),
        project_root.join("build").join(format!("{target}.3dsx")),
    ] {
        if path.exists() {
            return Some(path);
        }
    }

    find_newest_3dsx_in_dirs(&[project_root, &project_root.join("build")])
}

/// Silent 0.1s stereo WAV used when `banner.wav` is missing (CIA `makebanner` requires `-a`).
const DEFAULT_BANNER_WAV: &[u8] = include_bytes!("../template/banner.wav");

pub fn ensure_default_banner_wav(project_root: &Path) -> Result<bool, String> {
    let wav = project_root.join("banner.wav");
    if wav.exists() {
        return Ok(false);
    }
    std::fs::write(&wav, DEFAULT_BANNER_WAV)
        .map_err(|e| format!("Could not create banner.wav: {}", e))?;
    Ok(true)
}

pub fn find_built_cia(project_root: &Path) -> Option<PathBuf> {
    let makefile = std::fs::read_to_string(project_root.join("Makefile")).ok()?;
    let target = resolve_make_target(project_root, &makefile);

    for path in [
        project_root.join(format!("{target}.cia")),
        project_root.join("build").join(format!("{target}.cia")),
    ] {
        if path.exists() {
            return Some(path);
        }
    }

    find_newest_with_ext_in_dirs(&[project_root, &project_root.join("build")], "cia")
}

fn folder_target_name(project_root: &Path) -> String {
    project_root
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("game")
        .to_string()
}

/// devkitPro templates often set `TARGET := $(notdir $(CURDIR))` — resolve to the folder name.
fn resolve_make_target(project_root: &Path, makefile: &str) -> String {
    match parse_makefile_value(makefile, "TARGET") {
        Some(value) if is_unresolved_makefile_value(&value) => folder_target_name(project_root),
        Some(value) => value,
        None => folder_target_name(project_root),
    }
}

fn is_unresolved_makefile_value(value: &str) -> bool {
    value.contains('$') || value.contains('(') || value.contains(')')
}

fn find_newest_3dsx_in_dirs(dirs: &[&Path]) -> Option<PathBuf> {
    find_newest_with_ext_in_dirs(dirs, "3dsx")
}

fn find_newest_with_ext_in_dirs(dirs: &[&Path], ext: &str) -> Option<PathBuf> {
    let mut newest: Option<(PathBuf, std::time::SystemTime)> = None;
    for dir in dirs {
        let Ok(entries) = std::fs::read_dir(dir) else {
            continue;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().and_then(|s| s.to_str()) != Some(ext) {
                continue;
            }
            let Ok(modified) = entry.metadata().and_then(|m| m.modified()) else {
                continue;
            };
            if newest.as_ref().is_none_or(|(_, t)| modified > *t) {
                newest = Some((path, modified));
            }
        }
    }
    newest.map(|(path, _)| path)
}

pub fn list_asset_status(project_root: &Path) -> serde_json::Value {
    let makefile = std::fs::read_to_string(project_root.join("Makefile")).unwrap_or_default();
    let target = resolve_make_target(project_root, &makefile);
    let icon_rel = if project_root.join(format!("{target}.png")).exists() {
        format!("{target}.png")
    } else {
        "icon.png".to_string()
    };

    let assets = [
        ("tileset", "gfx/CardBoard3ds-TileSet.png"),
        ("background1", "gfx/Cavebg.png"),
        ("background2", "gfx/Cavebg2.png"),
        ("title", "gfx/Title.png"),
        ("bottom_menu", "gfx/BottomMenuScreen.png"),
        ("menu_load", "gfx/LoadGameSelected.png"),
        ("menu_new", "gfx/NewGameSelected.png"),
        ("menu_settings", "gfx/SettingsSelected.png"),
        ("soundtrack", "romfs/soundtrack.mp3"),
        ("banner", "banner.png"),
    ];
    let mut out = serde_json::Map::new();
    for (key, rel) in assets {
        let path = project_root.join(rel);
        let file_name = path
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or(rel)
            .to_string();
        out.insert(
            key.to_string(),
            serde_json::json!({
                "exists": path.exists(),
                "path": rel,
                "name": if path.exists() { file_name } else { String::new() }
            }),
        );
    }

    let icon_path = project_root.join(&icon_rel);
    let icon_name = icon_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("icon.png")
        .to_string();
    out.insert(
        "icon".to_string(),
        serde_json::json!({
            "exists": icon_path.exists(),
            "path": icon_rel,
            "name": if icon_path.exists() { icon_name } else { String::new() }
        }),
    );

    serde_json::Value::Object(out)
}

fn parse_makefile_value(content: &str, key: &str) -> Option<String> {
    for line in content.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with(key) && trimmed.contains(":=") {
            let value = trimmed.split(":=").nth(1)?.trim().to_string();
            if !value.is_empty() {
                return Some(value);
            }
        }
    }
    None
}

fn parse_game_config_h(content: &str) -> Option<GameConfig> {
    let mut cfg = GameConfig::default();
    let mut found = false;

    macro_rules! float {
        ($field:ident, $key:expr) => {
            if let Some(v) = parse_gc_float(content, $key) {
                cfg.$field = v;
                found = true;
            }
        };
    }
    macro_rules! bool_key {
        ($field:ident, $key:expr) => {
            if let Some(v) = parse_gc_bool(content, $key) {
                cfg.$field = v;
                found = true;
            }
        };
    }

    float!(move_speed, "GC_MOVE_SPEED");
    float!(sprint_speed, "GC_SPRINT_SPEED");
    float!(jump_force, "GC_JUMP_FORCE");
    float!(djump_force, "GC_DJUMP_FORCE");
    float!(gravity, "GC_GRAVITY");
    float!(gravity_fall, "GC_GRAVITY_FALL");
    float!(dash_speed, "GC_DASH_SPEED");
    bool_key!(double_jump_enabled, "DOUBLE_JUMP_ENABLED");
    bool_key!(dialogue_enabled, "DIALOGUE_ENABLED");
    bool_key!(wall_jump_enabled, "WALL_JUMP_ENABLED");
    bool_key!(dash_enabled, "DASH_ENABLED");
    bool_key!(ground_pound_enabled, "GROUND_POUND_ENABLED");

    if found { Some(cfg) } else { None }
}

fn parse_gc_float(content: &str, key: &str) -> Option<f32> {
    let pattern = format!("{key}");
    for line in content.lines() {
        if line.contains(&pattern) && line.contains('=') {
            let rhs = line.split('=').nth(1)?.trim().trim_end_matches(';');
            let num = rhs.trim_end_matches('f').trim();
            return num.parse().ok();
        }
    }
    None
}

fn parse_gc_bool(content: &str, key: &str) -> Option<bool> {
    for line in content.lines() {
        if line.contains(key) && line.contains('=') {
            let rhs = line.split('=').nth(1)?.trim().trim_end_matches(';');
            return match rhs {
                "true" => Some(true),
                "false" => Some(false),
                _ => None,
            };
        }
    }
    None
}

fn apply_main_cpp_physics(content: &str, cfg: &mut GameConfig) {
    macro_rules! set_float {
        ($field:ident, $name:expr) => {
            if let Some(v) = parse_cpp_float(content, $name) {
                cfg.$field = v;
            }
        };
    }
    set_float!(move_speed, "MOVE_SPEED");
    set_float!(sprint_speed, "SPRINT_SPEED");
    set_float!(jump_force, "JUMP_FORCE");
    set_float!(djump_force, "DJUMP_FORCE");
    set_float!(gravity, "GRAVITY");
    set_float!(gravity_fall, "GRAVITY_FALL");
    set_float!(dash_speed, "DASH_SPEED");

    if content.contains("DOUBLE_JUMP_ENABLED") {
        if let Some(v) = parse_cpp_bool_const(content, "DOUBLE_JUMP_ENABLED") {
            cfg.double_jump_enabled = v;
        }
    }
    if content.contains("DIALOGUE_ENABLED") {
        if let Some(v) = parse_cpp_bool_const(content, "DIALOGUE_ENABLED") {
            cfg.dialogue_enabled = v;
        }
    }
}

fn parse_cpp_float(content: &str, name: &str) -> Option<f32> {
    for line in content.lines() {
        if !line.contains("constexpr float") || !line.contains(name) {
            continue;
        }
        let rhs = line.split('=').nth(1)?.trim().trim_end_matches(';');
        let num = rhs.trim_end_matches('f').trim();
        if num.parse::<f32>().is_ok() {
            return num.parse().ok();
        }
    }
    None
}

fn parse_cpp_bool_const(content: &str, name: &str) -> Option<bool> {
    for line in content.lines() {
        if line.contains(name) && line.contains('=') {
            let rhs = line.split('=').nth(1)?.trim().trim_end_matches(';');
            return match rhs {
                "true" => Some(true),
                "false" => Some(false),
                _ => None,
            };
        }
    }
    None
}
