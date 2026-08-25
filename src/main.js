import { initWorkspaceLayout, expandBuildPanel } from "./workspace-layout.js";
import {
  setAutoSaveCallback,
  setProjectData,
  getProjectData,
  initEditorCanvas,
  initLevelEditorDom,
  buildLevelTabs,
  fitViewToLevel,
  render,
  syncFromLevel,
  syncToLevel,
  loadDialogue,
  syncLevelFeaturesFromUI,
  syncLevelPhysicsFromUI,
  onCustomPhysicsToggle,
  addLevel,
  addMenuLevel,
  deleteLevel,
  renameCurrent,
  toggleHidden,
  changeCurrentWorld,
  moveCurrentLevel,
  moveCurrentLevelToWorld,
  toggleDialoguePre,
  toggleDialoguePost,
  saveDialogue,
  resizeMap,
  clearMap,
  generateSyncBlock,
  showExport,
  closeModal,
  copyExport,
  parseLevelsFromCpp,
  mergeSupplementalFromCpp,
  loadLevelsJSON,
  saveLevelsJSON,
  loadTilesetFromFile,
} from "./level-editor.js";

function waitForTauri(maxMs = 15000) {
  return new Promise((resolve, reject) => {
    const t0 = Date.now();
    const tick = () => {
      const api = window.__TAURI__;
      if (api?.core?.invoke) return resolve(api);
      if (Date.now() - t0 > maxMs) {
        return reject(
          new Error(
            "Tauri API not ready — close this window and run: cd \"3DSPlatformerDevelopmentPlatform\" then npm run dev"
          )
        );
      }
      setTimeout(tick, 50);
    };
    tick();
  });
}

function requireTauri() {
  const api = window.__TAURI__;
  if (!api?.core?.invoke) {
    throw new Error(
      "Tauri API is not available. Start the desktop app with npm run dev (not by opening index.html in a browser)."
    );
  }
  return api;
}

function invoke(cmd, args) {
  return Promise.race([
    requireTauri().core.invoke(cmd, args),
    new Promise((_, reject) => {
      setTimeout(() => reject(new Error(`Timed out waiting for ${cmd}`)), 12000);
    }),
  ]);
}
const listen = (...args) => requireTauri().event.listen(...args);

const RECENT_KEY = "studio_recent_projects";
const THREEDSLINK_IP_KEY = "studio_3dslink_ip";
const DEFAULT_PLATFORMER = "C:\\devkitPro\\examples\\3ds\\games\\platformer";

let projectPath = null;
let saveTimer = null;

const DEFAULT_CONFIG = {
  app_title: "My 3DS Platformer",
  app_description: "A 3DS platformer game",
  app_author: "Game Developer",
  move_speed: 3.5,
  sprint_speed: 5.5,
  jump_force: -7.8,
  djump_force: -9.0,
  gravity: 0.48,
  gravity_fall: 0.78,
  dash_speed: 12.0,
};

function defaultConfig() {
  return { ...DEFAULT_CONFIG };
}

/** Merge game_config.h / Makefile values with studio_project.json (JSON wins). */
function mergeProjectConfig(fromFile, fromJson) {
  return { ...defaultConfig(), ...fromFile, ...(fromJson || {}) };
}

function joinPath(base, rel) {
  const sep = base.includes("\\") ? "\\" : "/";
  return `${base.replace(/[\\/]+$/, "")}${sep}${rel.replace(/^[\\/]+/, "")}`;
}

function readConfigFromUI() {
  return {
    app_title: val("cfg-app-title", DEFAULT_CONFIG.app_title),
    app_description: val("cfg-app-desc", DEFAULT_CONFIG.app_description),
    app_author: val("cfg-app-author", DEFAULT_CONFIG.app_author),
    move_speed: num("cfg-move-speed", DEFAULT_CONFIG.move_speed),
    sprint_speed: num("cfg-sprint-speed", DEFAULT_CONFIG.sprint_speed),
    jump_force: num("cfg-jump-force", DEFAULT_CONFIG.jump_force),
    djump_force: num("cfg-djump-force", DEFAULT_CONFIG.djump_force),
    gravity: num("cfg-gravity", DEFAULT_CONFIG.gravity),
    gravity_fall: num("cfg-gravity-fall", DEFAULT_CONFIG.gravity_fall),
    dash_speed: num("cfg-dash-speed", DEFAULT_CONFIG.dash_speed),
    // Master enables in game_config.h (per-level overrides in studio_project.json)
    double_jump_enabled: true,
    dialogue_enabled: true,
    wall_jump_enabled: true,
    dash_enabled: true,
    ground_pound_enabled: true,
  };
}

function applyConfigToUI(cfg) {
  setVal("cfg-app-title", cfg.app_title);
  setVal("cfg-app-desc", cfg.app_description);
  setVal("cfg-app-author", cfg.app_author);
  setSlider("cfg-move-speed", "val-move-speed", cfg.move_speed);
  setSlider("cfg-sprint-speed", "val-sprint-speed", cfg.sprint_speed);
  setSlider("cfg-jump-force", "val-jump-force", cfg.jump_force);
  setSlider("cfg-djump-force", "val-djump-force", cfg.djump_force);
  setSlider("cfg-gravity", "val-gravity", cfg.gravity);
  setSlider("cfg-gravity-fall", "val-gravity-fall", cfg.gravity_fall);
  setSlider("cfg-dash-speed", "val-dash-speed", cfg.dash_speed);
}

function val(id, fallback) {
  const el = document.getElementById(id);
  return el && el.value.trim() ? el.value.trim() : fallback;
}

function num(id, fallback) {
  const v = parseFloat(document.getElementById(id)?.value);
  return Number.isFinite(v) ? v : fallback;
}

function chk(id) {
  return !!document.getElementById(id)?.checked;
}

function setVal(id, value) {
  const el = document.getElementById(id);
  if (el) el.value = value;
}

function setChk(id, value) {
  const el = document.getElementById(id);
  if (el) el.checked = !!value;
}

function setSlider(sliderId, labelId, value) {
  const s = document.getElementById(sliderId);
  const l = document.getElementById(labelId);
  if (s) s.value = value;
  if (l) l.textContent = Number(value).toFixed(2).replace(/\.?0+$/, "");
}

function projectLabel() {
  if (!projectPath) return "";
  const parts = projectPath.replace(/\\/g, "/").split("/");
  return parts[parts.length - 1] || projectPath;
}

function rememberRecent(path) {
  try {
    const list = JSON.parse(localStorage.getItem(RECENT_KEY) || "[]").filter(
      (p) => p !== path
    );
    list.unshift(path);
    localStorage.setItem(RECENT_KEY, JSON.stringify(list.slice(0, 8)));
  } catch {
    /* ignore */
  }
}

async function persistProjectJson() {
  if (!projectPath) return;
  const payload = {
    ...getProjectData(),
    config: readConfigFromUI(),
  };
  await invoke("save_project_json", {
    projectPath,
    jsonData: JSON.stringify(payload, null, 2),
  });
}

/** Write studio_project.json, game_config.h/Makefile, and main.cpp level block. */
async function persistProjectToDisk({ syncCpp = true, saveGameConfig = true } = {}) {
  if (!projectPath) return;
  await persistProjectJson();
  if (saveGameConfig) {
    await invoke("save_config", {
      projectPath,
      config: readConfigFromUI(),
    });
  }
  if (syncCpp) {
    await syncLevelsToCpp();
  }
}

function scheduleAutoSave() {
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    persistProjectToDisk().catch((e) => console.warn("autosave:", e));
  }, 600);
}

async function syncLevelsToCpp() {
  if (!projectPath) return;
  const block = generateSyncBlock();
  await invoke("write_levels", { projectPath, syncBlock: block });
}

async function saveProject() {
  if (!projectPath) {
    alert("Open a project first.");
    return;
  }

  const patched = await invoke("ensure_studio_integration", { projectPath });
  if (patched) {
    appendBuildLog(
      "Applied 3DS Studio hooks to main.cpp (game_config.h integration).\n"
    );
  }
  await persistProjectToDisk();
  setStatus("Project saved to disk.");
}

async function importLevelsFromMainCpp() {
  const cppPath = joinPath(projectPath, "source/main.cpp");
  const content = await invoke("read_text_file", { filePath: cppPath });
  const parsed = parseLevelsFromCpp(content);
  if (!parsed || !parsed.length) {
    return false;
  }
  setProjectData({
    levels: parsed,
    currentLevelIdx: 0,
    nextLevelIndex: parsed.filter((l) => !l.isMenu).length,
  });
  loadDialogue();
  return true;
}

async function supplementFromMainCppIfNeeded() {
  if (!projectPath) return;
  try {
    const cppPath = joinPath(projectPath, "source/main.cpp");
    const content = await invoke("read_text_file", { filePath: cppPath });
    const merged = mergeSupplementalFromCpp(content);
    if (merged.menu || merged.dialogue > 0 || merged.win > 0 || merged.physics > 0 || merged.checkpoint > 0) {
      syncFromLevel();
      buildLevelTabs();
      scheduleAutoSave();
      const parts = [];
      if (merged.menu) parts.push("menu BG");
      if (merged.dialogue > 0) parts.push(`${merged.dialogue} level dialogue`);
      if (merged.win > 0) parts.push(`${merged.win} win zone(s)`);
      if (merged.physics > 0) parts.push(`${merged.physics} custom physics`);
      if (merged.checkpoint > 0) parts.push(`${merged.checkpoint} checkpoint overlay(s)`);
      setStatus(`Recovered from main.cpp: ${parts.join(", ")}.`);
    }
  } catch (e) {
    console.warn("supplementFromMainCpp:", e);
  }
}

const PNG_ASSET_PATHS = {
  tileset: "gfx/CardBoard3ds-TileSet.png",
  background1: "gfx/Cavebg.png",
  background2: "gfx/Cavebg2.png",
  title: "gfx/Title.png",
  bottom_menu: "gfx/BottomMenuScreen.png",
  menu_load: "gfx/LoadGameSelected.png",
  menu_new: "gfx/NewGameSelected.png",
  menu_settings: "gfx/SettingsSelected.png",
  banner: "banner.png",
};

function clearAssetThumb(thumbEl, slot) {
  if (!thumbEl) return;
  if (thumbEl.dataset.objectUrl) {
    URL.revokeObjectURL(thumbEl.dataset.objectUrl);
    delete thumbEl.dataset.objectUrl;
  }
  thumbEl.style.backgroundImage = "";
  slot?.classList.remove("has-thumb");
}

async function resolveIconRelPath() {
  try {
    const makefile = await invoke("read_text_file", {
      filePath: joinPath(projectPath, "Makefile"),
    });
    for (const line of makefile.split("\n")) {
      const trimmed = line.trim();
      if (trimmed.startsWith("TARGET") && trimmed.includes(":=")) {
        const target = trimmed.split(":=")[1]?.trim();
        if (target) return `${target}.png`;
      }
    }
  } catch {
    /* optional */
  }
  return "icon.png";
}

async function assetRelPath(key, info) {
  if (key === "icon") return resolveIconRelPath();
  return PNG_ASSET_PATHS[key] || info.path;
}

async function setAssetThumb(slot, key, info) {
  const thumbEl = slot.querySelector(".asset-thumb");
  if (!thumbEl) return;

  clearAssetThumb(thumbEl, slot);
  if (key === "soundtrack") return;

  if (!info.exists || !PNG_ASSET_PATHS[key] && key !== "icon") return;

  try {
    const rel = await assetRelPath(key, info);
    const bytes = await invoke("read_binary_file", {
      filePath: joinPath(projectPath, rel),
    });
    const blob = new Blob([new Uint8Array(bytes)], { type: "image/png" });
    const url = URL.createObjectURL(blob);
    thumbEl.dataset.objectUrl = url;
    thumbEl.style.backgroundImage = `url("${url}")`;
    slot.classList.add("has-thumb");
  } catch {
    // leave blank thumbnail
  }
}

async function refreshAssetSlots() {
  if (!projectPath) return;
  const status = await invoke("get_asset_status", { projectPath });
  for (const [key, info] of Object.entries(status)) {
    const slot = document.querySelector(`.asset-slot[data-asset="${key}"]`);
    if (!slot) continue;

    const fileEl = slot.querySelector(".asset-file");
    if (info.exists) {
      slot.classList.add("loaded");
      if (fileEl) fileEl.textContent = info.name || "Imported";
    } else {
      slot.classList.remove("loaded");
      if (fileEl) {
        fileEl.textContent =
          key === "soundtrack" ? "Click to import MP3…" : "Click to import PNG…";
      }
    }

    await setAssetThumb(slot, key, info);
  }
}

async function loadEditorTilesetPreview() {
  const tilePath = joinPath(projectPath, "gfx/CardBoard3ds-TileSet.png");
  try {
    const bytes = await invoke("read_binary_file", { filePath: tilePath });
    const blob = new Blob([new Uint8Array(bytes)], { type: "image/png" });
    loadTilesetFromFile(new File([blob], "tileset.png", { type: "image/png" }));
  } catch {
    /* preview optional */
  }
}

async function openProject(path) {
  const inspect = await invoke("inspect_project", { projectPath: path });
  if (!inspect.valid) {
    alert(inspect.message);
    return;
  }

  projectPath = path;
  rememberRecent(path);
  document.getElementById("welcome-screen").classList.add("hidden");
  document.getElementById("project-name-display").textContent = projectLabel();

  const fileConfig = await invoke("load_project_config", { projectPath: path });

  let loadedFromJson = false;
  let jsonConfig = null;
  try {
    const json = await invoke("load_project_json", { projectPath: path });
    const data = JSON.parse(json);
    jsonConfig = data.config || null;
    if (data.levels?.length) {
      setProjectData(data);
      loadDialogue();
      loadedFromJson = true;
      await supplementFromMainCppIfNeeded();
    }
  } catch {
    /* no studio_project.json */
  }

  applyConfigToUI(mergeProjectConfig(fileConfig, jsonConfig));

  if (!loadedFromJson) {
    const ok = await importLevelsFromMainCpp();
    if (!ok) {
      alert("Could not import levels from main.cpp.");
      return;
    }
  }

  buildLevelTabs();
  initEditorCanvas();
  fitViewToLevel();
  render();
  await refreshAssetSlots();
  await loadEditorTilesetPreview();

  const hint = inspect.has_game_config
    ? "Project loaded."
    : "Project loaded. Save once to add game_config.h for live physics tuning.";
  setStatus(hint);
}

async function pickProjectDirectory(title) {
  return invoke("pick_directory", {
    title,
    defaultPath: projectPath || DEFAULT_PLATFORMER,
  });
}

async function newProject() {
  const dest = await pickProjectDirectory("Choose folder for your new game project");
  if (!dest) return;

  const config = defaultConfig();
  applyConfigToUI(config);
  await invoke("create_project", { destination: dest, config });
  await openProject(dest);
}

async function openExistingProject() {
  const dest = await pickProjectDirectory(
    "Open your devkitPro platformer project folder (contains Makefile and source/main.cpp)"
  );
  if (!dest) return;
  await openProject(dest);
}

async function openDefaultPlatformer() {
  const inspect = await invoke("inspect_project", { projectPath: DEFAULT_PLATFORMER });
  if (inspect.valid) {
    await openProject(DEFAULT_PLATFORMER);
    return;
  }
  await openExistingProject();
}

function appendBuildLog(text) {
  const out = document.getElementById("build-output");
  if (!out) return;
  out.textContent += text;
  out.scrollTop = out.scrollHeight;
}

function setBuildState(running) {
  const dot = document.getElementById("build-status-dot");
  if (dot) {
    dot.classList.toggle("building", running);
    dot.classList.toggle("success", !running);
  }
}

function setStatus(msg) {
  const el = document.getElementById("canvas-status");
  if (el) el.textContent = msg;
}

function show3dslinkModal() {
  const overlay = document.getElementById("threedslink-overlay");
  const input = document.getElementById("threedslink-ip");
  if (!overlay || !input) return;
  try {
    const saved = localStorage.getItem(THREEDSLINK_IP_KEY);
    if (saved) input.value = saved;
  } catch {
    /* ignore */
  }
  overlay.classList.remove("hidden");
  input.focus();
  input.select();
}

function hide3dslinkModal() {
  document.getElementById("threedslink-overlay")?.classList.add("hidden");
}

async function run3dslink() {
  if (!projectPath) {
    alert("Open a project before using 3dslink.");
    return;
  }

  const input = document.getElementById("threedslink-ip");
  const ip = input?.value?.trim();
  if (!ip) {
    alert("Enter your 3DS IP address.");
    return;
  }

  hide3dslinkModal();
  try {
    localStorage.setItem(THREEDSLINK_IP_KEY, ip);
  } catch {
    /* ignore */
  }

  expandBuildPanel();

  const out = document.getElementById("build-output");
  if (out) out.textContent = "";

  setBuildState(true);
  try {
    await invoke("run_3dslink", { projectPath, ipAddress: ip });
  } catch (e) {
    appendBuildLog(`\n${e}\n`);
  } finally {
    setBuildState(false);
  }
}

async function runBuild(cleanFirst = false, buildKind = "3dsx") {
  if (!projectPath) {
    alert("Open a project before building.");
    return;
  }

  await saveProject();

  expandBuildPanel();
  const out = document.getElementById("build-output");
  if (out) out.textContent = "";
  setBuildState(true);

  try {
    if (cleanFirst) {
      await invoke("clean_project", { projectPath });
    }
    if (buildKind === "cia") {
      await invoke("compile_project_cia", { projectPath });
    } else {
      await invoke("compile_project", { projectPath });
    }
  } catch (e) {
    appendBuildLog(`\n${e}\n`);
  } finally {
    setBuildState(false);
  }
}

async function importAsset(assetType) {
  if (!projectPath) {
    alert("Open a project first.");
    return;
  }

  const extensions =
    assetType === "soundtrack" ? ["mp3"] : ["png"];
  const picked = await invoke("pick_file", {
    title: `Import ${assetType}`,
    extensions,
  });
  if (!picked) return;

  await invoke("import_asset", {
    projectPath,
    sourcePath: picked,
    assetType,
  });

  await refreshAssetSlots();
  if (assetType === "tileset") {
    const bytes = await invoke("read_binary_file", { filePath: picked });
    const blob = new Blob([new Uint8Array(bytes)], { type: "image/png" });
    loadTilesetFromFile(new File([blob], "tileset.png", { type: "image/png" }));
  }
  setStatus(`Imported ${assetType}. Rebuild to bake assets into the ROM.`);
}

async function checkToolchain() {
  const el = document.getElementById("toolchain-status");
  if (!el) return;
  try {
    const path = await invoke("check_toolchain");
    el.textContent = `devkitPro ready — builds will produce .3dsx (${path})`;
    el.style.color = "var(--success)";
  } catch {
    el.textContent =
      "devkitARM not found at C:\\devkitPro. Install devkitPro to compile.";
    el.style.color = "var(--warning)";
  }
}

function bindConfigSliders() {
  const sliders = [
    ["cfg-move-speed", "val-move-speed"],
    ["cfg-sprint-speed", "val-sprint-speed"],
    ["cfg-jump-force", "val-jump-force"],
    ["cfg-djump-force", "val-djump-force"],
    ["cfg-gravity", "val-gravity"],
    ["cfg-gravity-fall", "val-gravity-fall"],
    ["cfg-dash-speed", "val-dash-speed"],
  ];

  for (const [sid, lid] of sliders) {
    const s = document.getElementById(sid);
    if (!s) continue;
    s.addEventListener("input", () => {
      document.getElementById(lid).textContent = s.value;
      scheduleAutoSave();
    });
  }

  for (const id of ["cfg-app-title", "cfg-app-desc", "cfg-app-author"]) {
    document.getElementById(id)?.addEventListener("change", scheduleAutoSave);
    document.getElementById(id)?.addEventListener("input", scheduleAutoSave);
  }

  for (const id of [
    "cfg-double-jump",
    "cfg-dialogue",
    "cfg-wall-jump",
    "cfg-dash",
    "cfg-ground-pound",
    "cfg-minimap",
  ]) {
    document.getElementById(id)?.addEventListener("change", () => {
      syncLevelFeaturesFromUI();
      scheduleAutoSave();
    });
  }

  document.getElementById("cfg-custom-physics")?.addEventListener("change", () => {
    onCustomPhysicsToggle();
  });

  const lvlSliders = [
    ["lvl-move-speed", "val-lvl-move-speed"],
    ["lvl-sprint-speed", "val-lvl-sprint-speed"],
    ["lvl-jump-force", "val-lvl-jump-force"],
    ["lvl-djump-force", "val-lvl-djump-force"],
    ["lvl-gravity", "val-lvl-gravity"],
    ["lvl-gravity-fall", "val-lvl-gravity-fall"],
    ["lvl-dash-speed", "val-lvl-dash-speed"],
  ];
  for (const [sid, lid] of lvlSliders) {
    const s = document.getElementById(sid);
    if (!s) continue;
    s.addEventListener("input", () => {
      const lab = document.getElementById(lid);
      if (lab) lab.textContent = s.value;
      syncLevelPhysicsFromUI();
      scheduleAutoSave();
    });
  }
}

function bindEditorControls() {
  document.getElementById("btn-add-level")?.addEventListener("click", addLevel);
  document.getElementById("btn-add-menu")?.addEventListener("click", addMenuLevel);
  document.getElementById("btn-del-level")?.addEventListener("click", deleteLevel);
  document.getElementById("level-name")?.addEventListener("input", renameCurrent);
  document.getElementById("level-hidden")?.addEventListener("change", toggleHidden);
  document.getElementById("level-world")?.addEventListener("change", changeCurrentWorld);
  document.getElementById("btn-level-up")?.addEventListener("click", () => moveCurrentLevel(-1));
  document.getElementById("btn-level-down")?.addEventListener("click", () => moveCurrentLevel(1));
  document.getElementById("btn-level-move-world")?.addEventListener("click", moveCurrentLevelToWorld);
  document.getElementById("btn-resize-map")?.addEventListener("click", resizeMap);

  document.getElementById("dlg-pre-enable")?.addEventListener("change", toggleDialoguePre);
  document.getElementById("dlg-post-enable")?.addEventListener("change", toggleDialoguePost);
  for (const id of [
    "dlg-pre-1",
    "dlg-pre-2",
    "dlg-pre-3",
    "dlg-pre-4",
    "dlg-post-1",
    "dlg-post-2",
    "dlg-post-3",
    "dlg-post-4",
  ]) {
    const el = document.getElementById(id);
    el?.addEventListener("change", () => {
      saveDialogue();
      scheduleAutoSave();
    });
    el?.addEventListener("input", () => {
      saveDialogue();
      scheduleAutoSave();
    });
  }

  document.getElementById("tileset-upload")?.addEventListener("change", (e) => {
    loadTilesetFromFile(e.target.files[0]);
  });
}

function bindTopBar() {
  document.getElementById("btn-save")?.addEventListener("click", () => saveProject());
  document.getElementById("btn-export-cpp")?.addEventListener("click", showExport);
  document.getElementById("btn-compile")?.addEventListener("click", () => runBuild(false));
  document.getElementById("btn-build-cia")?.addEventListener("click", () => runBuild(false, "cia"));
  document.getElementById("btn-3dslink")?.addEventListener("click", show3dslinkModal);
  document.getElementById("btn-clean")?.addEventListener("click", () => runBuild(true));

  document.getElementById("threedslink-connect")?.addEventListener("click", () => run3dslink());
  document.getElementById("threedslink-cancel")?.addEventListener("click", hide3dslinkModal);
  document.getElementById("threedslink-overlay")?.addEventListener("click", (e) => {
    if (e.target.id === "threedslink-overlay") hide3dslinkModal();
  });
  document.getElementById("threedslink-ip")?.addEventListener("keydown", (e) => {
    if (e.key === "Enter") run3dslink();
    if (e.key === "Escape") hide3dslinkModal();
  });
  document.getElementById("btn-open-folder")?.addEventListener("click", () => {
    if (projectPath) invoke("open_project_folder", { projectPath });
  });

  document.getElementById("btn-save-json")?.addEventListener("click", () => {
    syncToLevel();
    const blob = new Blob([saveLevelsJSON()], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "levels.json";
    a.click();
    URL.revokeObjectURL(a.href);
  });

  document.getElementById("btn-import-json")?.addEventListener("click", () => {
    document.getElementById("json-file-input")?.click();
  });

  document.getElementById("btn-reimport-cpp")?.addEventListener("click", async () => {
    if (!projectPath) return;
    if (
      !confirm(
        "Replace all levels from source/main.cpp? Includes menu BG and per-level dialogue. Unsaved editor changes will be lost."
      )
    ) {
      return;
    }
    await importLevelsFromMainCpp();
    buildLevelTabs();
    initEditorCanvas();
    scheduleAutoSave();
    setStatus("Re-imported levels from main.cpp.");
  });

  document.getElementById("btn-copy-export")?.addEventListener("click", copyExport);
  document.getElementById("btn-close-modal")?.addEventListener("click", closeModal);
  document.getElementById("modal-close")?.addEventListener("click", closeModal);
}

function setWelcomeStatus(message) {
  const el = document.getElementById("toolchain-status");
  if (el) el.textContent = message;
}

function bindWelcomeAction(id, handler) {
  const btn = document.getElementById(id);
  if (!btn) return;
  btn.addEventListener("click", async () => {
    try {
      btn.disabled = true;
      setWelcomeStatus("Working…");
      await handler();
    } catch (err) {
      console.error(err);
      const msg = err?.message || String(err);
      showWelcomeError(msg);
      alert(msg);
    } finally {
      btn.disabled = false;
      checkToolchain();
    }
  });
}

function bindWelcome() {
  bindWelcomeAction("btn-new-project", newProject);
  bindWelcomeAction("btn-open-project", openExistingProject);
  bindWelcomeAction("btn-open-platformer", openDefaultPlatformer);
}

function bindAssets() {
  document.querySelectorAll(".asset-slot").forEach((slot) => {
    slot.addEventListener("click", () => importAsset(slot.dataset.asset));
  });
}

function bindBuildPanel() {
  listen("build-log", (event) => {
    appendBuildLog(event.payload);
  }).catch(console.error);
}

function ensureJsonFileInput() {
  if (document.getElementById("json-file-input")) return;
  const input = document.createElement("input");
  input.type = "file";
  input.id = "json-file-input";
  input.accept = ".json";
  input.hidden = true;
  input.addEventListener("change", async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const text = await file.text();
    if (loadLevelsJSON(text)) {
      buildLevelTabs();
      loadDialogue();
      initEditorCanvas();
      scheduleAutoSave();
      setStatus("Levels loaded from JSON.");
    } else {
      alert("Could not parse levels JSON.");
    }
    e.target.value = "";
  });
  document.body.appendChild(input);
}

function showWelcomeError(message) {
  const el = document.getElementById("welcome-error");
  if (!el) return;
  el.textContent = message;
  el.classList.remove("hidden");
}

window.addEventListener("DOMContentLoaded", async () => {
  try {
    await waitForTauri();
    bindWelcome();

    try {
      initLevelEditorDom();
    } catch (editorErr) {
      console.warn("Level editor init:", editorErr);
    }

    applyConfigToUI(defaultConfig());
    bindConfigSliders();
    bindEditorControls();
    bindTopBar();
    bindAssets();
    bindBuildPanel();
    initWorkspaceLayout();
    ensureJsonFileInput();

    try {
      const savedIp = localStorage.getItem(THREEDSLINK_IP_KEY);
      if (savedIp) {
        const ipInput = document.getElementById("threedslink-ip");
        if (ipInput) ipInput.value = savedIp;
      }
    } catch {
      /* ignore */
    }

    setAutoSaveCallback(scheduleAutoSave);
    checkToolchain();

    document.addEventListener("keydown", (e) => {
      if (e.ctrlKey && e.key === "s") {
        e.preventDefault();
        saveProject();
      }
    });

  } catch (err) {
    console.error(err);
    showWelcomeError(err?.message || String(err));
  }
});

window.addEventListener("error", (e) => {
  showWelcomeError(e.message || "Script error");
});
window.addEventListener("unhandledrejection", (e) => {
  showWelcomeError(e.reason?.message || String(e.reason || "Unhandled error"));
});
