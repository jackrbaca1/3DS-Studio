/**
 * Resizable / collapsible dock panels (left, right, build) + canvas relayout.
 */

const STORAGE_KEY = "studio-dock-layout";

const DEFAULTS = {
  leftWidth: 260,
  rightWidth: 290,
  buildHeight: 180,
  leftCollapsed: false,
  rightCollapsed: false,
  buildCollapsed: false,
};

const LIMITS = {
  left: { min: 200, max: 480 },
  right: { min: 220, max: 520 },
  build: { min: 72, max: 420 },
};

let layout = { ...DEFAULTS };
let drag = null;

function loadLayout() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return;
    const saved = JSON.parse(raw);
    layout = { ...DEFAULTS, ...saved };
  } catch {
    /* ignore */
  }
}

function saveLayout() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(layout));
  } catch {
    /* ignore */
  }
}

function clamp(n, min, max) {
  return Math.max(min, Math.min(max, n));
}

function notifyLayoutChange() {
  window.dispatchEvent(new Event("workspace-layout-change"));
}

function applyLayout() {
  const root = document.documentElement;
  root.style.setProperty("--dock-left-width", `${layout.leftWidth}px`);
  root.style.setProperty("--dock-right-width", `${layout.rightWidth}px`);
  root.style.setProperty("--build-panel-height", `${layout.buildHeight}px`);

  const left = document.getElementById("left-panel");
  const right = document.getElementById("right-panel");
  const build = document.getElementById("build-panel");
  const buildSplitter = document.getElementById("build-splitter");

  left?.classList.toggle("collapsed", layout.leftCollapsed);
  right?.classList.toggle("collapsed", layout.rightCollapsed);
  build?.classList.toggle("collapsed", layout.buildCollapsed);

  const leftSplitter = document.querySelector('[data-splitter="left"]');
  const rightSplitter = document.querySelector('[data-splitter="right"]');
  leftSplitter?.classList.toggle("hidden", layout.leftCollapsed);
  rightSplitter?.classList.toggle("hidden", layout.rightCollapsed);
  if (buildSplitter) {
    buildSplitter.classList.toggle("hidden", layout.buildCollapsed);
    buildSplitter.setAttribute("aria-hidden", layout.buildCollapsed ? "true" : "false");
  }

  updateCollapseButtons();
  notifyLayoutChange();
}

function updateCollapseButtons() {
  document.querySelectorAll("[data-dock-collapse]").forEach((btn) => {
    const dock = btn.dataset.dockCollapse;
    const collapsed =
      dock === "left"
        ? layout.leftCollapsed
        : dock === "right"
          ? layout.rightCollapsed
          : layout.buildCollapsed;
    btn.setAttribute("aria-expanded", collapsed ? "false" : "true");
    if (dock === "left") btn.textContent = collapsed ? "▶" : "◀";
    else if (dock === "right") btn.textContent = collapsed ? "◀" : "▶";
    else btn.textContent = collapsed ? "▲" : "▼";
  });
}

function toggleDock(dock) {
  if (dock === "left") layout.leftCollapsed = !layout.leftCollapsed;
  else if (dock === "right") layout.rightCollapsed = !layout.rightCollapsed;
  else if (dock === "build") layout.buildCollapsed = !layout.buildCollapsed;
  saveLayout();
  applyLayout();
}

function startDrag(splitter, e) {
  if (e.button !== 0) return;
  const kind = splitter.dataset.splitter;
  if (kind === "left" && layout.leftCollapsed) return;
  if (kind === "right" && layout.rightCollapsed) return;
  if (kind === "build" && layout.buildCollapsed) return;

  e.preventDefault();
  splitter.classList.add("dragging");
  document.body.classList.add("dock-dragging");
  document.body.dataset.dockDrag = kind;

  drag = {
    kind,
    startX: e.clientX,
    startY: e.clientY,
    startLeft: layout.leftWidth,
    startRight: layout.rightWidth,
    startBuild: layout.buildHeight,
  };
}

function onDragMove(e) {
  if (!drag) return;
  if (drag.kind === "left") {
    layout.leftWidth = clamp(
      drag.startLeft + (e.clientX - drag.startX),
      LIMITS.left.min,
      LIMITS.left.max
    );
  } else if (drag.kind === "right") {
    layout.rightWidth = clamp(
      drag.startRight - (e.clientX - drag.startX),
      LIMITS.right.min,
      LIMITS.right.max
    );
  } else if (drag.kind === "build") {
    layout.buildHeight = clamp(
      drag.startBuild - (e.clientY - drag.startY),
      LIMITS.build.min,
      LIMITS.build.max
    );
  }
  applyLayout();
}

function endDrag() {
  if (!drag) return;
  document.querySelectorAll(".dock-splitter.dragging").forEach((el) => {
    el.classList.remove("dragging");
  });
  document.body.classList.remove("dock-dragging");
  delete document.body.dataset.dockDrag;
  drag = null;
  saveLayout();
}

function bindSplitters() {
  document.querySelectorAll(".dock-splitter[data-splitter]").forEach((splitter) => {
    splitter.addEventListener("mousedown", (e) => startDrag(splitter, e));
  });
  window.addEventListener("mousemove", onDragMove);
  window.addEventListener("mouseup", endDrag);
}

function bindCollapseControls() {
  document.querySelectorAll("[data-dock-collapse]").forEach((btn) => {
    btn.addEventListener("click", (e) => {
      e.stopPropagation();
      toggleDock(btn.dataset.dockCollapse);
    });
  });

  document.querySelectorAll(".dock-toolbar[data-dock-toggle]").forEach((bar) => {
    bar.addEventListener("click", (e) => {
      if (e.target.closest("[data-dock-collapse]")) return;
      toggleDock(bar.dataset.dockToggle);
    });
  });
}

function observeCenterPanel() {
  const center = document.getElementById("center-panel");
  const workspace = document.getElementById("workspace");
  if (!center) return;

  const ro = new ResizeObserver(() => notifyLayoutChange());
  ro.observe(center);
  if (workspace) ro.observe(workspace);

  const build = document.getElementById("build-panel");
  if (build) ro.observe(build);
}

export function initWorkspaceLayout() {
  loadLayout();
  applyLayout();
  bindSplitters();
  bindCollapseControls();
  observeCenterPanel();
}

export function expandBuildPanel() {
  if (!layout.buildCollapsed) return;
  layout.buildCollapsed = false;
  saveLayout();
  applyLayout();
}
