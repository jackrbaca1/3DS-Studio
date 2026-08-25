/** Phase 2: toolchain wizard, Help panel, contextual build UX. */

import { HELP_SECTIONS } from "./help-content.js";
import { studioConfirm } from "./studio-dialogs.js";

const WIZARD_AUTO_KEY = "studio-wizard-auto-shown";
const DEVKITPRO_DOCS = "https://devkitpro.org/wiki/Getting_Started";

/** @type {(cmd: string, args?: object) => Promise<any>} */
let invokeFn = null;
/** @type {(() => void) | null} */
let onReportChanged = null;

export function initPhase2Ux(opts) {
  invokeFn = opts.invoke;
  onReportChanged = opts.onReportChanged || null;
  bindWizard();
  bindHelp();
  applyToolTooltips();
  document.addEventListener("keydown", (e) => {
    if (e.key !== "Escape") return;
    if (!document.getElementById("help-overlay")?.classList.contains("hidden")) {
      hideHelp();
      return;
    }
    if (!document.getElementById("toolchain-wizard-overlay")?.classList.contains("hidden")) {
      hideWizard();
    }
  });
}

export async function openExternalUrl(url) {
  await invokeFn("open_external_url", { url });
}

export async function fetchToolchainReport() {
  return invokeFn("get_toolchain_status");
}

export function chipLabel(report) {
  if (!report) return { text: "Toolchain…", cls: "chip-muted" };
  if (report.ok) {
    const custom = report.source === "config";
    return {
      text: custom ? "Tools: custom path" : "Tools: OK",
      cls: "chip-ok",
    };
  }
  return { text: "Tools: missing", cls: "chip-fail" };
}

export function updateToolchainChips(report) {
  const { text, cls } = chipLabel(report);
  for (const id of ["welcome-toolchain-chip", "topbar-toolchain-chip"]) {
    const el = document.getElementById(id);
    if (!el) continue;
    el.textContent = text;
    el.className = `toolchain-chip ${cls}`;
    el.title = report?.message || text;
  }
  const status = document.getElementById("toolchain-status");
  if (status && report?.message) {
    status.textContent = report.message;
    status.className = report.ok ? "toolchain-status ok" : "toolchain-status fail";
  }
  if (typeof onReportChanged === "function") onReportChanged(report);
}

export async function refreshToolchainUi() {
  try {
    const report = await fetchToolchainReport();
    updateToolchainChips(report);
    return report;
  } catch (e) {
    updateToolchainChips({
      ok: false,
      message: e?.message || String(e),
      source: "",
      root: "",
      bash: false,
      gcc: false,
      makerom: false,
      bannertool: false,
      threedslink: false,
    });
    return null;
  }
}

export async function maybeShowWizardOnLaunch() {
  const report = await refreshToolchainUi();
  if (!report || report.ok) return;
  try {
    if (sessionStorage.getItem(WIZARD_AUTO_KEY) === "1") return;
    sessionStorage.setItem(WIZARD_AUTO_KEY, "1");
  } catch {
    /* ignore */
  }
  showWizard(report);
}

function bindWizard() {
  document.getElementById("btn-setup-tools")?.addEventListener("click", () => showWizard());
  document.getElementById("btn-welcome-setup")?.addEventListener("click", () => showWizard());
  document.getElementById("wizard-close")?.addEventListener("click", hideWizard);
  document.getElementById("wizard-cancel")?.addEventListener("click", hideWizard);
  document.getElementById("toolchain-wizard-overlay")?.addEventListener("click", (e) => {
    if (e.target.id === "toolchain-wizard-overlay") hideWizard();
  });
  document.getElementById("wizard-retest")?.addEventListener("click", async () => {
    await runWizardRetest();
  });
  document.getElementById("wizard-browse")?.addEventListener("click", async () => {
    const coreInvoke = window.__TAURI__?.core?.invoke;
    if (!coreInvoke) {
      alert("Tauri API not ready.");
      return;
    }
    setRetestButtonState("testing");
    setWizardMessageState("testing", "Saving selected folder…");
    // No 12s race — folder picker can sit open longer than invoke()'s timeout.
    const picked = await coreInvoke("pick_directory", {
      title: "Select DEVKITPRO folder (e.g. C:\\devkitPro)",
      defaultPath: "C:\\devkitPro",
    });
    if (!picked) {
      setRetestButtonState("idle");
      return;
    }
    try {
      setWizardMessageState("testing", "Testing selected folder…");
      const report = await invokeFn("set_toolchain_path", { path: picked });
      updateToolchainChips(report);
      fillWizard(report);
      setRetestButtonState(report?.ok ? "ok" : "fail");
    } catch (e) {
      setRetestButtonState("fail");
      setWizardMessageState(
        "fail",
        e?.message || "Could not use that folder. Select a DEVKITPRO directory."
      );
    }
  });
  document.getElementById("wizard-clear-path")?.addEventListener("click", async () => {
    setRetestButtonState("testing");
    setWizardMessageState("testing", "Clearing custom path and retesting…");
    try {
      const report = await invokeFn("set_toolchain_path", { path: null });
      updateToolchainChips(report);
      fillWizard(report);
      setRetestButtonState(report?.ok ? "ok" : "fail");
    } catch (e) {
      setRetestButtonState("fail");
      setWizardMessageState("fail", e?.message || String(e));
    }
  });
  document.getElementById("wizard-open-docs")?.addEventListener("click", async () => {
    try {
      await openExternalUrl(DEVKITPRO_DOCS);
    } catch (e) {
      alert(e?.message || String(e));
    }
  });
}

function setRetestButtonState(state) {
  const btn = document.getElementById("wizard-retest");
  if (!btn) return;
  btn.classList.remove("retest-idle", "retest-testing", "retest-ok", "retest-fail", "success", "danger");
  if (state === "testing") {
    btn.disabled = true;
    btn.textContent = "Testing…";
    btn.classList.add("retest-testing");
  } else if (state === "ok") {
    btn.disabled = false;
    btn.textContent = "Ready";
    btn.classList.add("retest-ok", "success");
  } else if (state === "fail") {
    btn.disabled = false;
    btn.textContent = "Retest";
    btn.classList.add("retest-fail", "danger");
  } else {
    btn.disabled = false;
    btn.textContent = "Retest";
    btn.classList.add("retest-idle");
  }
}

function setWizardMessageState(state, text) {
  const msgEl = document.getElementById("wizard-message");
  if (!msgEl) return;
  if (text != null) msgEl.textContent = text;
  msgEl.className = `wizard-message ${state === "ok" ? "ok" : state === "testing" ? "testing" : "fail"}`;
}

async function runWizardRetest() {
  setRetestButtonState("testing");
  setWizardMessageState("testing", "Testing DEVKITPRO…");
  try {
    // Brief pause so the orange "Testing…" state is visible even on fast machines.
    await new Promise((r) => setTimeout(r, 280));
    const report = await refreshToolchainUi();
    fillWizard(report);
    setRetestButtonState(report?.ok ? "ok" : "fail");
    return report;
  } catch (e) {
    setRetestButtonState("fail");
    setWizardMessageState(
      "fail",
      e?.message || "Test failed. Select a DEVKITPRO directory (usually C:\\devkitPro)."
    );
    return null;
  }
}

export async function showWizard(report) {
  const overlay = document.getElementById("toolchain-wizard-overlay");
  if (!overlay) return;
  const r = report || (await refreshToolchainUi());
  fillWizard(r);
  setRetestButtonState(r?.ok ? "ok" : r ? "fail" : "idle");
  overlay.classList.remove("hidden");
  document.getElementById("wizard-cancel")?.focus();
}

export function hideWizard() {
  document.getElementById("toolchain-wizard-overlay")?.classList.add("hidden");
}

function fillWizard(report) {
  if (!report) return;
  const rootEl = document.getElementById("wizard-root");
  const sourceEl = document.getElementById("wizard-source");
  const listEl = document.getElementById("wizard-missing");
  if (rootEl) rootEl.textContent = report.root || "—";
  if (sourceEl) sourceEl.textContent = report.source || "—";

  if (report.ok) {
    setWizardMessageState("ok", report.message || "devkitPro ready.");
  } else {
    setWizardMessageState(
      "fail",
      report.message ||
        "Not a valid DEVKITPRO. Select the directory that contains msys2 and devkitARM (usually C:\\devkitPro)."
    );
  }

  if (listEl) {
    const rows = [
      ["MSYS2 bash", report.bash],
      ["devkitARM gcc", report.gcc],
      ["makerom (CIA)", report.makerom],
      ["bannertool (CIA)", report.bannertool],
      ["3dslink", report.threedslink],
    ];
    listEl.innerHTML = rows
      .map(
        ([name, ok]) =>
          `<li class="${ok ? "tool-ok" : "tool-missing"}"><span class="tool-dot"></span>${name}${
            ok ? "" : " — missing"
          }</li>`
      )
      .join("");
  }
  const editNote = document.getElementById("wizard-edit-note");
  if (editNote) {
    editNote.textContent = report.ok
      ? "Build tools look ready. You can keep editing anytime."
      : "You can still open and edit projects. Choose the correct DEVKITPRO folder to enable Build.";
  }
  setRetestButtonState(report.ok ? "ok" : "fail");
}

function bindHelp() {
  document.getElementById("btn-help")?.addEventListener("click", () => showHelp());
  document.getElementById("btn-welcome-help")?.addEventListener("click", () => showHelp());
  document.getElementById("help-close")?.addEventListener("click", hideHelp);
  document.getElementById("help-overlay")?.addEventListener("click", (e) => {
    if (e.target.id === "help-overlay") hideHelp();
  });

  const nav = document.getElementById("help-nav");
  const body = document.getElementById("help-body");
  if (!nav || !body) return;

  nav.innerHTML = HELP_SECTIONS.map(
    (s, i) =>
      `<button type="button" class="help-nav-btn${i === 0 ? " active" : ""}" data-help="${s.id}">${s.title}</button>`
  ).join("");

  const showSection = (id) => {
    const section = HELP_SECTIONS.find((s) => s.id === id) || HELP_SECTIONS[0];
    body.innerHTML = `<h3>${section.title}</h3>${section.html}`;
    nav.querySelectorAll(".help-nav-btn").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.help === section.id);
    });
    body.querySelectorAll(".help-link").forEach((btn) => {
      btn.addEventListener("click", async () => {
        try {
          await openExternalUrl(btn.dataset.url);
        } catch (e) {
          alert(e?.message || String(e));
        }
      });
    });
  };

  nav.addEventListener("click", (e) => {
    const btn = e.target.closest(".help-nav-btn");
    if (btn) showSection(btn.dataset.help);
  });
  showSection(HELP_SECTIONS[0].id);
}

export function showHelp(sectionId) {
  const overlay = document.getElementById("help-overlay");
  if (!overlay) return;
  if (sectionId) {
    document.querySelector(`.help-nav-btn[data-help="${sectionId}"]`)?.click();
  }
  overlay.classList.remove("hidden");
  document.getElementById("help-close")?.focus();
}

export function hideHelp() {
  document.getElementById("help-overlay")?.classList.add("hidden");
}

const TOOL_TITLES = {
  paint: "Paint selected tile",
  erase: "Erase tiles (or right-drag when Trackpad Mode is off)",
  fill: "Flood-fill connected tiles",
  coin: "Place cracker collectibles",
  enemy: "Place enemies",
  spawn: "Set player spawn",
  tile3d: "3D tile — draws as a raised block in-game (uses more GPU budget)",
  warp: "Warp door — links to another level (set target in level list)",
  win: "Level exit / win pad",
  checkpoint: "Mid-level respawn point",
};

function applyToolTooltips() {
  document.querySelectorAll(".tool-btn[data-tool]").forEach((btn) => {
    const tip = TOOL_TITLES[btn.dataset.tool];
    if (tip) btn.title = tip;
  });
  const budget = document.getElementById("level-budget-hint");
  if (budget) {
    budget.title =
      "3DS memory/entity caps. Stay under the limits or the game may fail to load the level.";
  }
}

export function humanBuildFailure(raw) {
  const t = String(raw || "");
  const lower = t.toLowerCase();
  if (lower.includes("bash") && (lower.includes("not found") || lower.includes("msys"))) {
    return "MSYS2 bash not found — open Setup tools… and point at C:\\devkitPro.";
  }
  if (lower.includes("devkitpro") || lower.includes("devkitarm") || lower.includes("gcc")) {
    return "Build tools incomplete — open Setup tools… or install from the official Getting Started guide.";
  }
  if (lower.includes("makerom") || lower.includes("bannertool") || lower.includes("cia tooling")) {
    return "CIA tools missing — see Help → CIA tools, or use Build 3dsx instead.";
  }
  if (lower.includes("no spaces") || lower.includes("space")) {
    return "Project path has spaces — Save As into Documents/3DSStudio with a name that has no spaces.";
  }
  if (lower.includes("game_config") || lower.includes("cannot write") || lower.includes("source")) {
    return "Project files incomplete (missing source/) — try Start Fresh Example or re-open the folder.";
  }
  const first = t.split(/\r?\n/).map((l) => l.trim()).find(Boolean);
  if (first && first.length < 160) return first;
  return "Build failed — see the log below for details.";
}

export async function confirmBuildPreconditions(buildKind, projectPath) {
  let report = null;
  try {
    report = await fetchToolchainReport();
    updateToolchainChips(report);
  } catch {
    /* continue */
  }

  if (report && !report.ok) {
    const openSetup = await studioConfirm({
      title: "Build tools missing",
      message: `${report.message}\n\nOpen Setup tools…?\n(Cancel = try build anyway)`,
      confirmLabel: "Setup tools…",
    });
    if (openSetup) {
      await showWizard(report);
      return false;
    }
  }

  if (buildKind === "cia" && report && (!report.makerom || !report.bannertool)) {
    const go = await studioConfirm({
      title: "CIA tools missing?",
      message:
        "makerom and/or bannertool look missing. CIA build will likely fail.\n\nOK = try anyway · Cancel = open Help",
      confirmLabel: "Try anyway",
    });
    if (!go) {
      showHelp("cia");
      return false;
    }
  }

  if (projectPath && buildKind !== "clean") {
    try {
      const status = await invokeFn("get_asset_status", { projectPath });
      const track = status?.soundtrack;
      if (track && !track.exists) {
        const go = await studioConfirm({
          title: "No soundtrack",
          message:
            "No soundtrack MP3 imported yet. Build will continue without music.\n\nOK = build anyway · Cancel = stay here",
          confirmLabel: "Build anyway",
        });
        if (!go) return false;
      }
    } catch {
      /* optional */
    }
  }

  return true;
}

export function showCopyBuildPathButton(projectPath) {
  const bar = document.getElementById("build-path-actions");
  if (!bar || !projectPath) return;
  bar.classList.remove("hidden");
  const pathEl = document.getElementById("build-path-label");
  if (pathEl) pathEl.textContent = projectPath;
  const btn = document.getElementById("btn-copy-build-path");
  if (!btn || btn.dataset.bound === "1") return;
  btn.dataset.bound = "1";
  btn.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(projectPath);
      btn.textContent = "Copied!";
      setTimeout(() => {
        btn.textContent = "Copy project path";
      }, 1500);
    } catch {
      alert(projectPath);
    }
  });
}

export function hideCopyBuildPathButton() {
  document.getElementById("build-path-actions")?.classList.add("hidden");
}

export function prependBuildOneLiner(text) {
  const out = document.getElementById("build-output");
  if (!out) return;
  const line = document.createElement("div");
  line.className = "build-one-liner";
  line.textContent = text;
  out.prepend(line);
}
