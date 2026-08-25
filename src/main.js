import { initWorkspaceLayout, expandBuildPanel } from "./workspace-layout.js";
import {
  initPhase2Ux,
  refreshToolchainUi,
  maybeShowWizardOnLaunch,
  confirmBuildPreconditions,
  humanBuildFailure,
  showCopyBuildPathButton,
  hideCopyBuildPathButton,
  prependBuildOneLiner,
} from "./phase2-ux.js";
import { studioConfirm, bindStudioConfirmUi } from "./studio-dialogs.js";
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
  createFreshStarterLevels,
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
const LAST_PROJECT_KEY = "studio_last_project";

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
  double_jump_enabled: true,
  dialogue_enabled: true,
  wall_jump_enabled: true,
  dash_enabled: true,
  ground_pound_enabled: true,
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
    localStorage.setItem(LAST_PROJECT_KEY, path);
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

function clearPendingAutosave() {
  clearTimeout(saveTimer);
  saveTimer = null;
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

  try {
    const patched = await invoke("ensure_studio_integration", { projectPath });
    if (patched) {
      appendBuildLog(
        "Applied 3DS Studio hooks to main.cpp (game_config.h integration).\n"
      );
    }
    await persistProjectToDisk();
    setStatus("Project saved.");
    renderWelcomeRecent().catch(() => {});
  } catch (e) {
    const msg = e?.message || String(e);
    setStatus("Save failed.");
    alert(
      `Save failed:\n${msg}\n\nCheck that the project folder still has Makefile and source/. If OneDrive emptied it, open the project again from Projects or use Start Fresh Example.`
    );
    throw e;
  }
}

let nameProjectMode = null; // "save-as" | "new" | "rename"
let renameTargetPath = null;

function showNameProjectModal(mode, defaults = {}) {
  nameProjectMode = mode;
  renameTargetPath = defaults.path || null;
  const overlay = document.getElementById("name-project-overlay");
  const title = document.getElementById("name-project-title");
  const hint = document.getElementById("name-project-hint");
  const input = document.getElementById("name-project-input");
  const confirm = document.getElementById("name-project-confirm");
  const overwrite = document.getElementById("name-project-overwrite");
  const overwriteRow = document.getElementById("name-project-overwrite-row");
  if (!overlay || !input) return;

  if (mode === "save-as") {
    title.textContent = "Save As";
    hint.textContent =
      "Creates a named copy in Documents/3DSStudio. Use a name without spaces (e.g. MyPlatformer).";
    confirm.textContent = "Save As";
    input.value = (defaults.name || projectLabel() || "MyPlatformer").replace(/\s+/g, "");
    if (overwriteRow) overwriteRow.classList.remove("hidden");
  } else if (mode === "rename") {
    title.textContent = "Rename Project";
    hint.textContent =
      "Renames the folder under Documents/3DSStudio. Name cannot contain spaces.";
    confirm.textContent = "Rename";
    input.value = defaults.name || "MyPlatformer";
    if (overwriteRow) overwriteRow.classList.add("hidden");
  } else {
    title.textContent = "New Project";
    hint.textContent =
      "Creates a fresh starter in Documents/3DSStudio. Name cannot contain spaces.";
    confirm.textContent = "Create";
    input.value = defaults.name || "MyPlatformer";
    if (overwriteRow) overwriteRow.classList.remove("hidden");
  }
  if (overwrite) overwrite.checked = false;
  overlay.classList.remove("hidden");
  input.focus();
  input.select();
}

function hideNameProjectModal() {
  document.getElementById("name-project-overlay")?.classList.add("hidden");
  nameProjectMode = null;
  renameTargetPath = null;
}

async function confirmNameProjectModal() {
  const input = document.getElementById("name-project-input");
  const overwrite = document.getElementById("name-project-overwrite")?.checked;
  const name = input?.value?.trim() || "";
  if (!name) {
    alert("Enter a project name.");
    return;
  }

  const mode = nameProjectMode;
  const renamePath = renameTargetPath;
  hideNameProjectModal();

  if (mode === "save-as") {
    await saveProjectAs(name, overwrite);
  } else if (mode === "new") {
    await createNamedNewProject(name, overwrite);
  } else if (mode === "rename" && renamePath) {
    await applyRenameLibraryProject(renamePath, name);
  }
}

async function saveProjectAs(name, overwrite = false) {
  if (!projectPath) {
    alert("Open a project first.");
    return;
  }
  await persistProjectToDisk();
  setStatus("Saving as…");
  try {
    const result = await invoke("save_project_as", {
      sourcePath: projectPath,
      name,
      overwrite: !!overwrite,
    });
    await openProject(result.path);
    setStatus(`Saved as “${result.name}”.`);
  } catch (err) {
    const msg = err?.message || String(err);
    if (/already exists/i.test(msg) && !overwrite) {
      const ok = await studioConfirm({
        title: "Overwrite project?",
        message: `${msg}\n\nOverwrite it?`,
        confirmLabel: "Overwrite",
        danger: true,
      });
      if (ok) await saveProjectAs(name, true);
      else alert(msg);
      return;
    }
    throw err;
  }
}

async function createNamedNewProject(name, overwrite = false) {
  const config = {
    ...defaultConfig(),
    app_title: name,
  };
  applyConfigToUI(config);
  try {
    const result = await invoke("create_named_project", {
      name,
      config,
      overwrite: !!overwrite,
    });
    await seedStarterAndOpen(result.path, config);
  } catch (err) {
    const msg = err?.message || String(err);
    if (/already exists/i.test(msg) && !overwrite) {
      const ok = await studioConfirm({
        title: "Overwrite project?",
        message: `${msg}\n\nOverwrite it?`,
        confirmLabel: "Overwrite",
        danger: true,
      });
      if (ok) await createNamedNewProject(name, true);
      else alert(msg);
      return;
    }
    throw err;
  }
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

/** Fallback specs if get_asset_specs IPC is unavailable. */
const ASSET_REQUIREMENTS = {
  tileset: "PNG 512×512",
  background1: "PNG 256×240",
  background2: "PNG 256×240",
  title: "PNG 400×240",
  bottom_menu: "PNG 320×240",
  menu_load: "PNG 400×240",
  menu_new: "PNG 400×240",
  menu_settings: "PNG 400×240",
  soundtrack: "MP3",
  icon: "PNG 48×48",
  banner: "PNG 256×128",
};

let assetSpecsByKey = { ...ASSET_REQUIREMENTS };

async function loadAssetSpecs() {
  try {
    const list = await invoke("get_asset_specs");
    if (Array.isArray(list)) {
      const map = {};
      for (const s of list) {
        map[s.key] = s.requirement || ASSET_REQUIREMENTS[s.key] || s.format;
      }
      assetSpecsByKey = { ...ASSET_REQUIREMENTS, ...map };
    }
  } catch {
    assetSpecsByKey = { ...ASSET_REQUIREMENTS };
  }
}

function assetRequirement(key) {
  return assetSpecsByKey[key] || ASSET_REQUIREMENTS[key] || "PNG";
}

function emptyAssetHint(key) {
  const req = assetRequirement(key);
  return key === "tileset" ? `Default tileset · ${req}` : `Needs ${req}`;
}

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
  // Show real art or colored placeholders; skip only when nothing is on disk.
  if (!info.on_disk && !info.exists) return;
  if (!PNG_ASSET_PATHS[key] && key !== "icon") return;

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
    const req = info.requirement || assetRequirement(key);
    if (info.exists) {
      slot.classList.add("loaded");
      if (fileEl) {
        fileEl.textContent = info.name ? `${info.name} · ${req}` : req;
      }
    } else {
      slot.classList.remove("loaded");
      if (fileEl) {
        fileEl.textContent = info.placeholder
          ? `Placeholder blocks · replace with ${req}`
          : emptyAssetHint(key);
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
  clearPendingAutosave();

  const inspect = await invoke("inspect_project", { projectPath: path });
  if (!inspect.valid) {
    alert(
      `${inspect.message}\n\nPath:\n${path}\n\nIf files are missing (OneDrive “online only”), download the folder or Start Fresh Example.`
    );
    return;
  }

  // Bind path only after validation so autosave cannot write into the wrong folder.
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
    /* no studio_project.json — load from this project's main.cpp only */
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
  renderWelcomeRecent().catch(() => {});
}

/** Write a fresh starter campaign into this folder, then open it. Levels stay in that project only. */
async function seedStarterAndOpen(path, config) {
  clearPendingAutosave();
  projectPath = path;
  if (config) applyConfigToUI(config);
  setProjectData(createFreshStarterLevels());
  await persistProjectToDisk();
  await openProject(path);
}

async function pickProjectDirectory(title) {
  let defaultPath = projectPath || "";
  try {
    defaultPath = defaultPath || localStorage.getItem(LAST_PROJECT_KEY) || "";
  } catch {
    /* ignore */
  }
  return invoke("pick_directory", {
    title,
    defaultPath: defaultPath || null,
  });
}

async function newProject() {
  showNameProjectModal("new");
}

async function openExistingProject() {
  const dest = await pickProjectDirectory(
    "Open a 3DS Studio project folder (contains Makefile and source/main.cpp)"
  );
  if (!dest) return;
  await openProject(dest);
}

/** Always rematerialize the example and seed a clean starter campaign. */
async function openFreshExampleProject() {
  setWelcomeStatus("Creating fresh example…");
  const result = await invoke("ensure_example_project", { reset: true });
  const path = result.path;
  const config = {
    ...defaultConfig(),
    app_title: "ExamplePlatformer",
    app_description: "Starter example for 3DS Studio",
    app_author: "3DS Studio",
  };
  await seedStarterAndOpen(path, config);
}

async function renderWelcomeRecent() {
  const wrap = document.getElementById("welcome-recent");
  const listEl = document.getElementById("welcome-recent-list");
  if (!wrap || !listEl) return;

  let projects = [];
  try {
    projects = await invoke("list_studio_projects");
  } catch (e) {
    console.warn("list_studio_projects:", e);
    // Fallback to browser recent paths if library scan fails
    projects = getRecentProjects().map((path) => ({
      name: projectDisplayName(path),
      path,
      modified: 0,
    }));
  }

  listEl.innerHTML = "";
  wrap.classList.remove("hidden");
  if (!projects.length) {
    listEl.innerHTML =
      '<p class="welcome-recent-empty">No projects yet. Start Fresh Example or New Project to begin.</p>';
    return;
  }

  for (const proj of projects.slice(0, 24)) {
    const row = document.createElement("div");
    row.className = "welcome-recent-row";

    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "welcome-recent-item";
    btn.title = proj.path;
    const isExample =
      proj.name === "ExamplePlatformer" || proj.name === "Example Platformer";
    btn.innerHTML = `<span class="welcome-recent-name">${escapeHtml(
      proj.name
    )}${isExample ? ' <span class="welcome-recent-tag">example</span>' : ""}</span><span class="welcome-recent-path">${escapeHtml(
      proj.path
    )}</span>`;
    btn.addEventListener("click", async () => {
      try {
        btn.disabled = true;
        setWelcomeStatus("Opening…");
        await openProject(proj.path);
      } catch (err) {
        console.error(err);
        const msg = err?.message || String(err);
        showWelcomeError(msg);
        alert(msg);
      } finally {
        btn.disabled = false;
        checkToolchain();
        renderWelcomeRecent().catch(() => {});
      }
    });

    const actions = document.createElement("div");
    actions.className = "welcome-recent-actions";

    const renameBtn = document.createElement("button");
    renameBtn.type = "button";
    renameBtn.className = "welcome-recent-action";
    renameBtn.textContent = "Rename";
    renameBtn.title = "Rename this project folder";
    renameBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      renameLibraryProject(proj).catch((err) => {
        console.error(err);
        alert(err?.message || String(err));
      });
    });

    const deleteBtn = document.createElement("button");
    deleteBtn.type = "button";
    deleteBtn.className = "welcome-recent-action danger";
    deleteBtn.textContent = "Delete";
    deleteBtn.title = "Delete this project from disk";
    deleteBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      deleteLibraryProject(proj).catch((err) => {
        console.error(err);
        alert(err?.message || String(err));
      });
    });

    actions.appendChild(renameBtn);
    actions.appendChild(deleteBtn);
    row.appendChild(btn);
    row.appendChild(actions);
    listEl.appendChild(row);
  }
}

async function renameLibraryProject(proj) {
  showNameProjectModal("rename", {
    name: proj.name || projectDisplayName(proj.path),
    path: proj.path,
  });
}

async function applyRenameLibraryProject(oldPath, name) {
  setWelcomeStatus("Renaming…");
  try {
    const result = await invoke("rename_studio_project", {
      projectPath: oldPath,
      newName: name,
    });
    forgetRecent(oldPath);
    if (result?.path) rememberRecent(result.path);
    if (projectPath && pathsEqual(projectPath, oldPath)) {
      projectPath = result.path;
      document.getElementById("project-name-display").textContent = result.name || name;
    }
    await renderWelcomeRecent();
    setWelcomeStatus(`Renamed to ${result.name || name}`);
  } catch (err) {
    const msg = err?.message || String(err);
    showWelcomeError(msg);
    alert(msg);
  } finally {
    checkToolchain();
  }
}

async function deleteLibraryProject(proj) {
  const ok = await studioConfirm({
    title: "Delete project?",
    message: `Delete project "${proj.name}" permanently?\n\nThis removes the folder from disk:\n${proj.path}\n\nThis cannot be undone.`,
    confirmLabel: "Delete",
    danger: true,
  });
  if (!ok) return;

  setWelcomeStatus("Deleting…");
  try {
    await invoke("delete_studio_project", { projectPath: proj.path });
    forgetRecent(proj.path);
    if (projectPath && pathsEqual(projectPath, proj.path)) {
      projectPath = null;
      document.getElementById("project-name-display").textContent = "";
    }
    await renderWelcomeRecent();
    setWelcomeStatus(`Deleted ${proj.name}`);
  } catch (err) {
    const msg = err?.message || String(err);
    showWelcomeError(msg);
    alert(msg);
  } finally {
    checkToolchain();
  }
}

function pathsEqual(a, b) {
  return String(a || "").replace(/\\/g, "/").toLowerCase() ===
    String(b || "").replace(/\\/g, "/").toLowerCase();
}

function forgetRecent(path) {
  try {
    const next = getRecentProjects().filter((p) => !pathsEqual(p, path));
    localStorage.setItem(RECENT_KEY, JSON.stringify(next));
  } catch {
    /* ignore */
  }
}

function getRecentProjects() {
  try {
    const list = JSON.parse(localStorage.getItem(RECENT_KEY) || "[]");
    return Array.isArray(list) ? list.filter((p) => typeof p === "string" && p) : [];
  } catch {
    return [];
  }
}

function projectDisplayName(path) {
  const parts = path.replace(/\\/g, "/").split("/").filter(Boolean);
  return parts[parts.length - 1] || path;
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
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

  if (!cleanFirst) {
    const ok = await confirmBuildPreconditions(buildKind, projectPath);
    if (!ok) return;
  }

  expandBuildPanel();
  const out = document.getElementById("build-output");
  if (out) out.textContent = "";
  hideCopyBuildPathButton();
  setBuildState(true);

  try {
    // Save first so levels/config are on disk; failures used to abort before compile with no build log.
    await persistProjectToDisk();
    if (cleanFirst) {
      await invoke("clean_project", { projectPath });
    } else if (buildKind === "cia") {
      await invoke("compile_project_cia", { projectPath });
    } else {
      await invoke("compile_project", { projectPath });
    }
    if (!cleanFirst) {
      appendBuildLog(
        `\nDone. Loadable .3dsx / .cia (if built) are in the project folder:\n${projectPath}\n`
      );
      showCopyBuildPathButton(projectPath);
    }
  } catch (e) {
    const msg = e?.message || String(e);
    prependBuildOneLiner(humanBuildFailure(msg));
    appendBuildLog(`\n${msg}\n`);
    alert(humanBuildFailure(msg));
  } finally {
    setBuildState(false);
  }
}

async function importAsset(assetType) {
  if (!projectPath) {
    alert("Open a project first.");
    return;
  }

  const req = assetRequirement(assetType);
  const extensions =
    assetType === "soundtrack" ? ["mp3"] : ["png"];
  const picked = await invoke("pick_file", {
    title: `Import ${assetType} (${req})`,
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
  setStatus(`Imported ${assetType} (${req}). Rebuild to bake assets into the ROM.`);
}

async function checkToolchain() {
  await refreshToolchainUi();
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
  document.getElementById("btn-projects")?.addEventListener("click", () => showWelcomeScreen());
  document.getElementById("btn-save")?.addEventListener("click", () => saveProject());
  document.getElementById("btn-save-as")?.addEventListener("click", () => {
    if (!projectPath) {
      alert("Open a project first.");
      return;
    }
    showNameProjectModal("save-as");
  });
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

  document.getElementById("name-project-confirm")?.addEventListener("click", () => {
    confirmNameProjectModal().catch((err) => {
      console.error(err);
      alert(err?.message || String(err));
    });
  });
  document.getElementById("name-project-cancel")?.addEventListener("click", hideNameProjectModal);
  document.getElementById("name-project-overlay")?.addEventListener("click", (e) => {
    if (e.target.id === "name-project-overlay") hideNameProjectModal();
  });
  document.getElementById("name-project-input")?.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      confirmNameProjectModal().catch((err) => {
        console.error(err);
        alert(err?.message || String(err));
      });
    }
    if (e.key === "Escape") hideNameProjectModal();
  });

  bindStudioConfirmUi();

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
    const ok = await studioConfirm({
      title: "Reload from C++?",
      message:
        "Replace all levels from source/main.cpp? Includes menu BG and per-level dialogue. Unsaved editor changes will be lost.",
      confirmLabel: "Reload",
      danger: true,
    });
    if (!ok) return;
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
  bindWelcomeAction("btn-open-other-folder", openExistingProject);
  bindWelcomeAction("btn-start-example", openFreshExampleProject);
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

async function showWelcomeScreen() {
  clearPendingAutosave();
  if (projectPath) {
    try {
      await persistProjectToDisk();
    } catch (e) {
      console.warn("save before welcome:", e);
    }
  }
  projectPath = null;
  document.getElementById("welcome-screen")?.classList.remove("hidden");
  document.getElementById("project-name-display").textContent = "";
  setStatus("Choose a project.");
  renderWelcomeRecent().catch(() => {});
  checkToolchain();
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
    await loadAssetSpecs();
    initPhase2Ux({ invoke });
    await checkToolchain();
    maybeShowWizardOnLaunch().catch(() => {});
    renderWelcomeRecent().catch(() => {});

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
