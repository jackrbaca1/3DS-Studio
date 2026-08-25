import { studioConfirm } from "./studio-dialogs.js";

// Tile definitions — must match main.cpp
const TILE_DEFS = [
  { id: 0, name: 'Empty',    color: '#2a2a3e', key: '0', uv: null },
  { id: 1, name: 'Ground',   color: '#8B5E3C', key: '1', uv: { col: 1, row: 1 } },
  { id: 2, name: 'Fill',     color: '#5C3A1E', key: '2', uv: { col: 1, row: 2 } },
  { id: 3, name: 'Spike',    color: '#D62828', key: '3', uv: { col: 6, row: 1 } },
  { id: 4, name: 'Platform', color: '#457B9D', key: '4', uv: { col: 9, row: 1 } },
  { id: 5, name: 'One-Way',  color: '#6BAED6', key: '5', uv: { col: 8, row: 1 } },
  { id: 6, name: 'BG Decor', color: '#3a5c3a', key: '6', uv: { col: 10, row: 1 } },
  { id: 7, name: 'Crumble',  color: '#B48C3C', key: '7', uv: { col: 7, row: 1 } },
  { id: 8, name: 'Moving',   color: '#9B59B6', key: '8', uv: { col: 5, row: 1 } },
  { id: 9, name: 'Warp',     color: '#00CED1', key: '9', uv: { col: 4, row: 1 } },
];

const TILE_SIZE = 32;
/** Must match MAP_H in main.cpp (largest level height the game can store). */
const MAX_MAP_W = 80;
const MAX_MAP_H = 128;

/** 3DS runtime caps (see main.cpp static constexpr MAX_*). */
export const GAME_LIMITS = {
  movers: { max: 16, label: 'Moving platforms', bytesEach: 36 },
  coins: { max: 16, label: 'Crackers', bytesEach: 28 },
  enemies: { max: 16, label: 'Enemies', bytesEach: 64 },
  crumbles: { max: 128, label: 'Crumble tiles', bytesEach: 12 },
  mapW: { max: MAX_MAP_W, label: 'Map width (tiles)' },
  mapH: { max: MAX_MAP_H, label: 'Map height (tiles)' },
};

const TILE_MOVING_ID = 8;
const TILE_CRUMBLE_ID = 7;

function countTileType(tilemap, mapW, mapH, tileId) {
  let n = 0;
  for (let y = 0; y < mapH; y++) {
    const row = tilemap[y];
    if (!row) continue;
    for (let x = 0; x < mapW; x++) {
      if (row[x] === tileId) n++;
    }
  }
  return n;
}

export function countLevelResources(lv) {
  if (!lv || lv.isMenu) {
    return { movers: 0, coins: 0, enemies: 0, crumbles: 0, mapW: 0, mapH: 0 };
  }
  return {
    movers: countTileType(lv.tilemap, lv.mapW, lv.mapH, TILE_MOVING_ID),
    coins: (lv.coins || []).length,
    enemies: (lv.enemies || []).length,
    crumbles: countTileType(lv.tilemap, lv.mapW, lv.mapH, TILE_CRUMBLE_ID),
    mapW: lv.mapW,
    mapH: lv.mapH,
  };
}

export function estimateLevelRamBytes(lv) {
  if (!lv || lv.isMenu) return 0;
  const r = countLevelResources(lv);
  const tilemapBytes = lv.mapW * lv.mapH * 4;
  const overlayBytes = lv.mapW * lv.mapH * 3;
  const entityBytes =
    Math.min(r.movers, GAME_LIMITS.movers.max) * GAME_LIMITS.movers.bytesEach
    + Math.min(r.coins, GAME_LIMITS.coins.max) * GAME_LIMITS.coins.bytesEach
    + Math.min(r.enemies, GAME_LIMITS.enemies.max) * GAME_LIMITS.enemies.bytesEach
    + Math.min(r.crumbles, GAME_LIMITS.crumbles.max) * GAME_LIMITS.crumbles.bytesEach;
  const moversRuntime = Math.min(r.movers, GAME_LIMITS.movers.max) * 48;
  return tilemapBytes + overlayBytes + entityBytes + moversRuntime + 2048;
}

export function updateLevelBudgetUI() {
  const list = document.getElementById('level-budget-list');
  const ramEl = document.getElementById('level-budget-ram');
  const hint = document.getElementById('level-budget-hint');
  const section = document.getElementById('level-budget-section');
  if (!list) return;

  const L = cur();
  if (L.isMenu) {
    if (hint) hint.textContent = 'Select a playable level to see limits.';
    list.innerHTML = '';
    if (ramEl) ramEl.textContent = '';
    if (section) section.classList.remove('budget-over');
    return;
  }

  const counts = countLevelResources(L);
  let anyOver = false;
  const rows = [
    ['mapW', counts.mapW],
    ['mapH', counts.mapH],
    ['movers', counts.movers],
    ['coins', counts.coins],
    ['enemies', counts.enemies],
    ['crumbles', counts.crumbles],
  ];

  list.innerHTML = '';
  for (const [key, count] of rows) {
    const lim = GAME_LIMITS[key];
    const over = count > lim.max;
    const warn = !over && count >= lim.max - 1 && lim.max <= 32;
    if (over) anyOver = true;
    const row = document.createElement('div');
    row.className = 'level-budget-row' + (over ? ' over' : warn ? ' warn' : '');
    row.innerHTML =
      '<span class="budget-label">' + lim.label + '</span>'
      + '<span class="budget-value">' + count + ' / ' + lim.max + '</span>';
    list.appendChild(row);
  }

  const ramKb = (estimateLevelRamBytes(L) / 1024).toFixed(1);
  if (ramEl) {
    ramEl.textContent = 'Est. RAM when loaded: ~' + ramKb + ' KB'
      + (anyOver ? ' — over-limit items are dropped in-game.' : '');
    ramEl.classList.toggle('over', anyOver);
  }
  if (hint) {
    hint.textContent = anyOver
      ? '"' + L.name + '" exceeds limits — trim moving tiles / enemies / crackers.'
      : 'Within limits for "' + L.name + '".';
  }
  if (section) section.classList.toggle('budget-over', anyOver);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Multi-level state
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
let nextLevelIndex = 0;

function getNextLevelIndex() {
  return nextLevelIndex++;
}

const WORLD_PRESETS = {
  1: { label: 'World 1', mapW: 80, mapH: 16 },
  2: { label: 'World 2', mapW: 80, mapH: 32 },
  0: { label: 'Secret', mapW: 80, mapH: 32, secret: true },
};

function levelWorldNum(lv) {
  if (lv.isMenu) return null;
  if (lv.isHidden || lv.world === 0) return 0;
  return typeof lv.world === 'number' ? lv.world : 1;
}

function worldPreset(world) {
  return WORLD_PRESETS[world] || WORLD_PRESETS[1];
}

/** Per-level gameplay toggles (also exported into LevelInfo in main.cpp). */
function defaultLevelFeatures(world) {
  const w = world ?? 1;
  return {
    doubleJump: w >= 2,
    dialogue: true,
    wallJump: true,
    dash: true,
    groundPound: true,
    minimap: true,
  };
}

function defaultParTime(world) {
  const w = world ?? 1;
  if (w === 0) return 60;
  return w >= 2 ? 90 : 60;
}

function normalizeLevelFeatures(lv) {
  if (lv.isMenu) return;
  if (!lv.features) {
    lv.features = defaultLevelFeatures(levelWorldNum(lv));
    return;
  }
  const d = defaultLevelFeatures(levelWorldNum(lv));
  lv.features = {
    doubleJump: lv.features.doubleJump ?? d.doubleJump,
    dialogue: lv.features.dialogue ?? d.dialogue,
    wallJump: lv.features.wallJump ?? d.wallJump,
    dash: lv.features.dash ?? d.dash,
    groundPound: lv.features.groundPound ?? d.groundPound,
    minimap: lv.features.minimap ?? d.minimap,
  };
}

function cppBool(v) {
  return v ? 'true' : 'false';
}

function cppFloat(v) {
  const n = Number(v);
  return (Number.isFinite(n) ? n : 0).toFixed(2) + 'f';
}

const DEFAULT_PHYSICS = {
  moveSpeed: 3.5,
  sprintSpeed: 5.5,
  jumpForce: -7.8,
  djumpForce: -9.0,
  gravity: 0.48,
  gravityFall: 0.78,
  dashSpeed: 12.0,
};

function readGlobalPhysicsFromUI() {
  const num = (id, fallback) => {
    const v = parseFloat(document.getElementById(id)?.value);
    return Number.isFinite(v) ? v : fallback;
  };
  return {
    moveSpeed: num('cfg-move-speed', DEFAULT_PHYSICS.moveSpeed),
    sprintSpeed: num('cfg-sprint-speed', DEFAULT_PHYSICS.sprintSpeed),
    jumpForce: num('cfg-jump-force', DEFAULT_PHYSICS.jumpForce),
    djumpForce: num('cfg-djump-force', DEFAULT_PHYSICS.djumpForce),
    gravity: num('cfg-gravity', DEFAULT_PHYSICS.gravity),
    gravityFall: num('cfg-gravity-fall', DEFAULT_PHYSICS.gravityFall),
    dashSpeed: num('cfg-dash-speed', DEFAULT_PHYSICS.dashSpeed),
  };
}

function defaultLevelPhysics() {
  return { enabled: false, ...DEFAULT_PHYSICS };
}

function normalizeLevelPhysics(lv) {
  if (lv.isMenu) return;
  const g = readGlobalPhysicsFromUI();
  if (!lv.physics) {
    lv.physics = { enabled: false, ...g };
    return;
  }
  lv.physics = {
    enabled: !!lv.physics.enabled,
    moveSpeed: lv.physics.moveSpeed ?? g.moveSpeed,
    sprintSpeed: lv.physics.sprintSpeed ?? g.sprintSpeed,
    jumpForce: lv.physics.jumpForce ?? g.jumpForce,
    djumpForce: lv.physics.djumpForce ?? g.djumpForce,
    gravity: lv.physics.gravity ?? g.gravity,
    gravityFall: lv.physics.gravityFall ?? g.gravityFall,
    dashSpeed: lv.physics.dashSpeed ?? g.dashSpeed,
  };
}

function applyLevelPhysicsFunctionCpp() {
  return [
    '#ifndef STUDIO_APPLY_LEVEL_PHYSICS_DEFINED',
    '#define STUDIO_APPLY_LEVEL_PHYSICS_DEFINED',
    'static void applyLevelPhysicsForLevel(int idx) {',
    '\tGRAVITY = GC_GRAVITY;',
    '\tGRAVITY_FALL = GC_GRAVITY_FALL;',
    '\tJUMP_FORCE = GC_JUMP_FORCE;',
    '\tDJUMP_FORCE = GC_DJUMP_FORCE;',
    '\tMOVE_SPEED = GC_MOVE_SPEED;',
    '\tSPRINT_SPEED = GC_SPRINT_SPEED;',
    '\tDASH_SPEED = GC_DASH_SPEED;',
    '\tif (idx < 0 || idx >= LEVEL_COUNT) return;',
    '\tconst LevelPhysics& p = LEVEL_PHYSICS_TABLE[idx];',
    '\tif (!p.custom) return;',
    '\tMOVE_SPEED = p.moveSpeed;',
    '\tSPRINT_SPEED = p.sprintSpeed;',
    '\tJUMP_FORCE = p.jumpForce;',
    '\tDJUMP_FORCE = p.djumpForce;',
    '\tGRAVITY = p.gravity;',
    '\tGRAVITY_FALL = p.gravityFall;',
    '\tDASH_SPEED = p.dashSpeed;',
    '}',
    '#endif',
  ].join('\n');
}

function exportPhysicsCppRow(lv, i) {
  normalizeLevelPhysics(lv);
  const p = lv.physics;
  if (!p.enabled) {
    return '\t/* ' + i + ' */ { false, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },';
  }
  return '\t/* ' + i + ' */ { true, '
    + cppFloat(p.moveSpeed) + ', ' + cppFloat(p.sprintSpeed) + ', '
    + cppFloat(p.jumpForce) + ', ' + cppFloat(p.djumpForce) + ', '
    + cppFloat(p.gravity) + ', ' + cppFloat(p.gravityFall) + ', '
    + cppFloat(p.dashSpeed) + ' },';
}

function parsePhysicsByIndex(block) {
  const out = {};
  const physRe = /\/\*\s*(\d+)\s*\*\/\s*\{\s*(true|false)\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*,\s*([\d.-]+)f?\s*\}/g;
  let m;
  while ((m = physRe.exec(block)) !== null) {
    const idx = parseInt(m[1], 10);
    out[idx] = {
      enabled: m[2] === 'true',
      moveSpeed: parseFloat(m[3]),
      sprintSpeed: parseFloat(m[4]),
      jumpForce: parseFloat(m[5]),
      djumpForce: parseFloat(m[6]),
      gravity: parseFloat(m[7]),
      gravityFall: parseFloat(m[8]),
      dashSpeed: parseFloat(m[9]),
    };
  }
  return out;
}

function createLevel(name, w, h, world, isMenu, levelIndex) {
  const tilemap = [];
  const tile3d = [];
  const warpOverlay = [];
  const winOverlay = [];
  const checkpointOverlay = [];
  for (let y = 0; y < h; y++) {
    tilemap.push(new Array(w).fill(0));
    tile3d.push(new Array(w).fill(false));
    warpOverlay.push(new Array(w).fill(false));
    winOverlay.push(new Array(w).fill(false));
    checkpointOverlay.push(new Array(w).fill(false));
  }
  const idx = levelIndex !== undefined ? levelIndex : getNextLevelIndex();
  const wNum = typeof world === 'number' ? world : 1;
  return {
    levelIndex: idx,
    name: name || 'Untitled',
    mapW: w || 80, mapH: h || 16,
    world: wNum,
    isMenu: !!isMenu,
    isHidden: false,
    tilemap,
    tile3d,
    warpOverlay,
    winOverlay,
    checkpointOverlay,
    warpTarget: -1,
    coins: [],
    enemies: [],
    spawnX: 1.5 * TILE_SIZE,
    spawnY: (h - 2) * TILE_SIZE - 28,
    undoStack: [], redoStack: [],
    dialoguePreEnabled: false,
    dialoguePostEnabled: false,
    dialoguePre: ['', '', '', ''],
    dialoguePost: ['', '', '', ''],
    features: isMenu ? null : defaultLevelFeatures(wNum),
    physics: isMenu ? null : defaultLevelPhysics(),
    parTime: isMenu ? 0 : defaultParTime(wNum),
  };
}

// Initialize with proper level indexing
nextLevelIndex = 0;
let levels = [createLevel('1-1', 80, 16, 1, false, 0)];
let currentLevelIdx = 0;

/** Minimal menu + one tutorial stage for new/example projects (not the full template campaign). */
export function createFreshStarterLevels() {
  const menu = createLevel('Menu', 80, 16, 1, true, -1);
  for (let x = 0; x < menu.mapW; x++) {
    menu.tilemap[13][x] = 1; // TILE_GROUND
    menu.tilemap[14][x] = 2; // TILE_FILL
    menu.tilemap[15][x] = 2;
  }
  menu.spawnX = 2 * TILE_SIZE;
  menu.spawnY = 13 * TILE_SIZE - 28;

  const lv = createLevel('1-1', 40, 16, 1, false, 0);
  lv.features = {
    doubleJump: false,
    dialogue: true,
    wallJump: false,
    dash: false,
    groundPound: false,
    minimap: true,
  };
  for (let x = 0; x < lv.mapW; x++) {
    lv.tilemap[14][x] = 1;
    lv.tilemap[15][x] = 2;
  }
  // Small ledge to practice jump
  for (let x = 12; x <= 16; x++) {
    lv.tilemap[11][x] = 4; // TILE_PLATFORM
  }
  lv.coins = [
    { x: 8 * TILE_SIZE + 9, y: 12 * TILE_SIZE + 9 },
    { x: 14 * TILE_SIZE + 9, y: 9 * TILE_SIZE + 9 },
    { x: 22 * TILE_SIZE + 9, y: 12 * TILE_SIZE + 9 },
  ];
  lv.winOverlay[13][36] = true;
  lv.winOverlay[13][37] = true;
  lv.dialoguePreEnabled = true;
  lv.dialoguePre = [
    'Welcome! Use Left/Right to move.',
    'Press A or B to jump.',
    'Grab the crackers and reach the goal!',
    '',
  ];
  lv.spawnX = 2 * TILE_SIZE;
  lv.spawnY = 14 * TILE_SIZE - 28;

  return {
    levels: [menu, lv],
    currentLevelIdx: 1,
    nextLevelIndex: 1,
  };
}

// Menu BG helpers
function findMenuLevelIdx() {
  return levels.findIndex(l => l.isMenu);
}
function playableLevels() {
  return levels.filter(l => !l.isMenu && !l.isHidden);
}
function allGameLevels() {
  return levels.filter(l => !l.isMenu);
}

function reindexPlayableLevels() {
  let idx = 0;
  for (const lv of levels) {
    if (!lv.isMenu) lv.levelIndex = idx++;
  }
}

/** Keep menu first; playable grouped World 1 → World 2 → other → secrets. */
function normalizePlayableOrder() {
  const menu = levels.filter(l => l.isMenu);
  const playable = levels.filter(l => !l.isMenu);
  const w1 = playable.filter(l => levelWorldNum(l) === 1);
  const w2 = playable.filter(l => levelWorldNum(l) === 2);
  const other = playable.filter(l => {
    const w = levelWorldNum(l);
    return w !== 1 && w !== 2 && w !== 0;
  });
  const secrets = playable.filter(l => levelWorldNum(l) === 0);
  levels = [...menu, ...w1, ...w2, ...other, ...secrets];
  reindexPlayableLevels();
}

/** Legacy main.cpp: all world=1 but tall maps / index split imply World 2. */
function inferWorldsFromLayout(levelList) {
  const playable = levelList.filter(l => !l.isMenu);
  if (playable.length < 2) return;
  if (!playable.every(l => l.world === 1)) return;

  let splitAt = -1;
  for (let i = 1; i < playable.length; i++) {
    if (playable[i].mapH >= 32 && playable[i - 1].mapH < 32) {
      splitAt = i;
      break;
    }
  }
  if (splitAt < 0 && playable.length >= 7) splitAt = 6;

  if (splitAt > 0) {
    for (let i = splitAt; i < playable.length; i++) {
      if (!playable[i].isHidden) playable[i].world = 2;
    }
  }
  for (const lv of playable) {
    if (lv.isHidden && lv.world === 1) lv.world = 0;
  }
}

function resizeLevelMap(L, newW, newH) {
  const oldH = L.mapH;
  const oldW = L.mapW;
  const tilemapNew = [];
  const tile3dNew = [];
  const warpNew = [];
  const winNew = [];
  const cpNew = [];
  for (let y = 0; y < newH; y++) {
    const row = new Array(newW).fill(0);
    const row3d = new Array(newW).fill(false);
    const rowWarp = new Array(newW).fill(false);
    const rowWin = new Array(newW).fill(false);
    const rowCp = new Array(newW).fill(false);
    if (y < oldH && L.tilemap[y]) {
      for (let x = 0; x < newW; x++) {
        if (x < oldW) {
          row[x] = L.tilemap[y][x];
          row3d[x] = L.tile3d?.[y]?.[x] || false;
          rowWarp[x] = L.warpOverlay?.[y]?.[x] || false;
          rowWin[x] = L.winOverlay?.[y]?.[x] || false;
          rowCp[x] = L.checkpointOverlay?.[y]?.[x] || false;
        }
      }
    }
    tilemapNew.push(row);
    tile3dNew.push(row3d);
    warpNew.push(rowWarp);
    winNew.push(rowWin);
    cpNew.push(rowCp);
  }
  L.mapW = newW;
  L.mapH = newH;
  L.tilemap = tilemapNew;
  L.tile3d = tile3dNew;
  L.warpOverlay = warpNew;
  L.winOverlay = winNew;
  L.checkpointOverlay = cpNew;
  if (L.spawnY > (newH - 2) * TILE_SIZE - 28) {
    L.spawnY = (newH - 2) * TILE_SIZE - 28;
  }
}

function applyWorldToLevel(L, world) {
  const preset = worldPreset(world);
  L.world = world;
  if (preset.secret) {
    L.isHidden = true;
    if (!L.name || /^\d+-\d+$/.test(L.name)) L.name = '???';
  }
  resizeLevelMap(L, preset.mapW, preset.mapH);
}

function levelOrganizerSection(lv) {
  if (lv.isMenu) return 'menu';
  if (lv.isHidden || lv.world === 0) return 'secret';
  if (lv.world === 2) return '2';
  if (lv.world === 1) return '1';
  return 'other';
}

function indicesInSection(sectionKey) {
  return levels
    .map((lv, i) => ({ lv, i }))
    .filter(({ lv }) => levelOrganizerSection(lv) === sectionKey)
    .map(({ i }) => i);
}

function updateLevelReorderButtons() {
  const up = document.getElementById('btn-level-up');
  const down = document.getElementById('btn-level-down');
  const moveBtn = document.getElementById('btn-level-move-world');
  const L = cur();
  if (!up || !down) return;
  if (L.isMenu) {
    up.disabled = true;
    down.disabled = true;
    if (moveBtn) moveBtn.disabled = true;
    return;
  }
  if (moveBtn) moveBtn.disabled = false;
  const section = levelOrganizerSection(L);
  const indices = indicesInSection(section);
  const pos = indices.indexOf(currentLevelIdx);
  up.disabled = pos <= 0;
  down.disabled = pos < 0 || pos >= indices.length - 1;
}

// Convenience accessors for current level
function cur() { return levels[currentLevelIdx]; }

// Expose current level fields as module-level vars for rendering/editing
let mapW, mapH, tilemap, coins, enemies, spawnX, spawnY, undoStack, redoStack;

let tile3d, warpOverlay, winOverlay, checkpointOverlay;

function syncFromLevel() {
  const L = cur();
  mapW = L.mapW; mapH = L.mapH;
  tilemap = L.tilemap; coins = L.coins;
  enemies = L.enemies || []; L.enemies = enemies;
  // Ensure overlay arrays exist (for levels loaded from older saves)
  if (!L.tile3d) { L.tile3d = []; for (let y = 0; y < mapH; y++) L.tile3d.push(new Array(mapW).fill(false)); }
  if (!L.warpOverlay) { L.warpOverlay = []; for (let y = 0; y < mapH; y++) L.warpOverlay.push(new Array(mapW).fill(false)); }
  if (!L.winOverlay) { L.winOverlay = []; for (let y = 0; y < mapH; y++) L.winOverlay.push(new Array(mapW).fill(false)); }
  if (!L.checkpointOverlay) { L.checkpointOverlay = []; for (let y = 0; y < mapH; y++) L.checkpointOverlay.push(new Array(mapW).fill(false)); }
  if (L.warpTarget === undefined) L.warpTarget = -1;
  tile3d = L.tile3d;
  warpOverlay = L.warpOverlay;
  winOverlay = L.winOverlay;
  checkpointOverlay = L.checkpointOverlay;
  spawnX = L.spawnX; spawnY = L.spawnY;
  undoStack = L.undoStack; redoStack = L.redoStack;
  const mapWEl = document.getElementById('map-w');
  const mapHEl = document.getElementById('map-h');
  const nameEl = document.getElementById('level-name');
  const hiddenEl = document.getElementById('level-hidden');
  if (mapWEl) mapWEl.value = mapW;
  if (mapHEl) mapHEl.value = mapH;
  if (nameEl) nameEl.value = L.name;
  if (hiddenEl) hiddenEl.checked = !!L.isHidden;
  const worldEl = document.getElementById('level-world');
  if (worldEl && !L.isMenu) {
    worldEl.disabled = false;
    worldEl.value = String(levelWorldNum(L));
  } else if (worldEl) {
    worldEl.disabled = true;
    worldEl.value = '1';
  }
  updateLevelReorderButtons();
  loadDialogue();
  applyLevelFeaturesToUI();
  applyLevelPhysicsToUI();
  updateLevelBudgetUI();
}

function syncToLevel() {
  const L = cur();
  L.mapW = mapW; L.mapH = mapH;
  L.tilemap = tilemap; L.coins = coins;
  L.enemies = enemies;
  L.tile3d = tile3d; L.warpOverlay = warpOverlay; L.winOverlay = winOverlay; L.checkpointOverlay = checkpointOverlay;
  L.spawnX = spawnX; L.spawnY = spawnY;
  L.undoStack = undoStack; L.redoStack = redoStack;
  syncLevelFeaturesFromUI();
  syncLevelPhysicsFromUI();
}

function applyLevelFeaturesToUI() {
  const L = cur();
  const featIds = [
    ['cfg-double-jump', 'doubleJump'],
    ['cfg-dialogue', 'dialogue'],
    ['cfg-wall-jump', 'wallJump'],
    ['cfg-dash', 'dash'],
    ['cfg-ground-pound', 'groundPound'],
    ['cfg-minimap', 'minimap'],
  ];
  const disabled = !!L.isMenu;
  normalizeLevelFeatures(L);
  const f = L.features || defaultLevelFeatures(1);
  for (const [id, key] of featIds) {
    const el = document.getElementById(id);
    if (!el) continue;
    el.disabled = disabled;
    if (!disabled) el.checked = !!f[key];
  }
  const hint = document.getElementById('level-features-hint');
  if (hint) {
    hint.textContent = disabled
      ? 'Select a playable level to edit features.'
      : `Features for "${L.name}" (saved with this level).`;
  }
  const dlgSection = document.getElementById('dialogue-section');
  if (dlgSection && !disabled) {
    dlgSection.classList.toggle('dialogue-disabled', !f.dialogue);
  }
}

function syncLevelFeaturesFromUI() {
  const L = cur();
  if (L.isMenu) return;
  L.features = {
    doubleJump: !!document.getElementById('cfg-double-jump')?.checked,
    dialogue: !!document.getElementById('cfg-dialogue')?.checked,
    wallJump: !!document.getElementById('cfg-wall-jump')?.checked,
    dash: !!document.getElementById('cfg-dash')?.checked,
    groundPound: !!document.getElementById('cfg-ground-pound')?.checked,
    minimap: !!document.getElementById('cfg-minimap')?.checked,
  };
}

function applyLevelPhysicsToUI() {
  const L = cur();
  const disabled = !!L.isMenu;
  normalizeLevelPhysics(L);
  const p = L.physics || defaultLevelPhysics();
  const toggle = document.getElementById('cfg-custom-physics');
  const sliders = document.getElementById('level-physics-sliders');
  const section = document.getElementById('level-physics-section');
  const hint = document.getElementById('level-physics-hint');

  if (toggle) {
    toggle.disabled = disabled;
    if (!disabled) toggle.checked = !!p.enabled;
  }
  if (sliders) sliders.classList.toggle('hidden', disabled || !p.enabled);
  if (section) section.classList.toggle('level-physics-disabled', disabled || !p.enabled);
  if (hint) {
    hint.textContent = disabled
      ? 'Select a playable level to override physics.'
      : p.enabled
        ? `Custom physics for "${L.name}" (saved with this level).`
        : `Using global physics for "${L.name}". Enable custom to tune this level only.`;
  }

  const rows = [
    ['lvl-move-speed', 'val-lvl-move-speed', 'moveSpeed'],
    ['lvl-sprint-speed', 'val-lvl-sprint-speed', 'sprintSpeed'],
    ['lvl-jump-force', 'val-lvl-jump-force', 'jumpForce'],
    ['lvl-djump-force', 'val-lvl-djump-force', 'djumpForce'],
    ['lvl-gravity', 'val-lvl-gravity', 'gravity'],
    ['lvl-gravity-fall', 'val-lvl-gravity-fall', 'gravityFall'],
    ['lvl-dash-speed', 'val-lvl-dash-speed', 'dashSpeed'],
  ];
  for (const [sid, lid, key] of rows) {
    const s = document.getElementById(sid);
    const lab = document.getElementById(lid);
    if (!s) continue;
    s.disabled = disabled;
    if (!disabled) {
      s.value = p[key];
      if (lab) lab.textContent = Number(p[key]).toFixed(2).replace(/\.?0+$/, '');
    }
  }
}

function syncLevelPhysicsFromUI() {
  const L = cur();
  if (L.isMenu) return;
  normalizeLevelPhysics(L);
  const enabled = !!document.getElementById('cfg-custom-physics')?.checked;
  const p = L.physics || defaultLevelPhysics();
  p.enabled = enabled;
  const rows = [
    ['lvl-move-speed', 'moveSpeed'],
    ['lvl-sprint-speed', 'sprintSpeed'],
    ['lvl-jump-force', 'jumpForce'],
    ['lvl-djump-force', 'djumpForce'],
    ['lvl-gravity', 'gravity'],
    ['lvl-gravity-fall', 'gravityFall'],
    ['lvl-dash-speed', 'dashSpeed'],
  ];
  for (const [sid, key] of rows) {
    const v = parseFloat(document.getElementById(sid)?.value);
    if (Number.isFinite(v)) p[key] = v;
  }
  L.physics = p;
}

function onCustomPhysicsToggle() {
  const L = cur();
  if (L.isMenu) return;
  const enabling = !!document.getElementById('cfg-custom-physics')?.checked;
  if (enabling) {
    if (!L.physics?.enabled) {
      L.physics = { enabled: true, ...readGlobalPhysicsFromUI() };
    } else {
      L.physics.enabled = true;
    }
  } else {
    normalizeLevelPhysics(L);
    L.physics.enabled = false;
  }
  applyLevelPhysicsToUI();
  autoSave();
}

function ensureLevelArrays(L) {
  if (!L || L.isMenu) return;
  const w = Math.min(MAX_MAP_W, Math.max(10, L.mapW || 80));
  const h = Math.min(MAX_MAP_H, Math.max(5, L.mapH || 16));
  L.mapW = w;
  L.mapH = h;

  const fixGrid = (grid, fill) => {
    const out = [];
    for (let y = 0; y < h; y++) {
      const row = grid?.[y] ? [...grid[y]] : [];
      while (row.length < w) row.push(fill);
      if (row.length > w) row.length = w;
      out.push(row);
    }
    return out;
  };

  L.tilemap = fixGrid(L.tilemap, 0);
  L.tile3d = fixGrid(L.tile3d, false);
  L.warpOverlay = fixGrid(L.warpOverlay, false);
  L.winOverlay = fixGrid(L.winOverlay, false);
  L.checkpointOverlay = fixGrid(L.checkpointOverlay, false);
}

const ZOOM_MIN = 0.0625;
const ZOOM_MAX = 5;

function mapPixelSize() {
  return { w: mapW * TILE_SIZE, h: mapH * TILE_SIZE };
}

function clampZoom(z) {
  return Math.max(ZOOM_MIN, Math.min(ZOOM_MAX, z));
}

function setZoomAtViewportCenter(newZoom) {
  if (!ensureEditorCanvas()) return;
  const mx = canvas.width / 2;
  const my = canvas.height / 2;
  const oldZoom = zoom;
  zoom = clampZoom(newZoom);
  panX = mx - (mx - panX) * (zoom / oldZoom);
  panY = my - (my - panY) * (zoom / oldZoom);
  updateUI();
  render();
}

export function fitScreen() {
  if (!ensureEditorCanvas()) return;
  const { w, h } = mapPixelSize();
  const pad = 32;
  const zx = (canvas.width - pad) / w;
  const zy = (canvas.height - pad) / h;
  zoom = clampZoom(Math.min(zx, zy));
  panX = (canvas.width - w * zoom) / 2;
  panY = (canvas.height - h * zoom) / 2;
  updateUI();
  render();
}

export function fillScreen() {
  if (!ensureEditorCanvas()) return;
  const { w, h } = mapPixelSize();
  const pad = 16;
  const zx = (canvas.width - pad) / w;
  const zy = (canvas.height - pad) / h;
  zoom = clampZoom(Math.max(zx, zy));
  panX = (canvas.width - w * zoom) / 2;
  panY = (canvas.height - h * zoom) / 2;
  updateUI();
  render();
}

function syncZoomSelectUI() {
  const label = document.getElementById('canvas-zoom-label');
  const select = document.getElementById('canvas-zoom-select');
  const pct = Math.round(zoom * 10000) / 100;
  if (label) label.textContent = pct + '%';
  if (!select) return;
  if (select.value === 'fit' || select.value === 'fill') return;
  let best = '';
  let bestDiff = Infinity;
  for (const opt of select.options) {
    const v = opt.value;
    if (!v || v === 'fit' || v === 'fill') continue;
    const diff = Math.abs(parseFloat(v) - zoom);
    if (diff < bestDiff) {
      bestDiff = diff;
      best = v;
    }
  }
  if (best && bestDiff < 0.02) select.value = best;
}

function fitViewToLevel() {
  if (!ensureEditorCanvas()) return;
  const { w, h } = mapPixelSize();
  const pad = 32;
  const zx = (canvas.width - pad) / w;
  const zy = (canvas.height - pad) / h;
  zoom = clampZoom(Math.min(zx, zy));
  const viewW = w * zoom;
  const viewH = h * zoom;
  panX = Math.max(20, (canvas.width - viewW) / 2);
  const focusY = Number.isFinite(spawnY)
    ? spawnY * zoom - canvas.height * 0.55
    : (viewH - canvas.height) / 2;
  panY = -Math.max(0, Math.min(focusY, viewH - canvas.height + 40));
  updateUI();
}

function switchLevel(idx) {
  if (idx < 0 || idx >= levels.length) return;
  syncToLevel();
  currentLevelIdx = idx;
  ensureLevelArrays(cur());
  syncFromLevel();
  buildLevelTabs();
  autoSave();
  requestAnimationFrame(() => {
    resizeCanvas();
    fitViewToLevel();
    render();
  });
}

function addLevel() {
  const sel = document.getElementById('add-level-world');
  const w = sel ? parseInt(sel.value, 10) : 1;
  addLevelToWorld(Number.isFinite(w) ? w : 1);
}

function addLevelToWorld(w) {
  syncToLevel();
  const preset = worldPreset(w);
  const gameLevels = levels.filter(l => !l.isMenu);
  const newIndex = gameLevels.length;
  let displayName;
  if (w === 0) {
    displayName = '???';
  } else {
    const worldLevels = gameLevels.filter(l => levelWorldNum(l) === w);
    displayName = w + '-' + (worldLevels.length + 1);
  }
  const lv = createLevel(displayName, preset.mapW, preset.mapH, w, false, newIndex);
  if (w === 0) lv.isHidden = true;
  levels.push(lv);
  normalizePlayableOrder();
  currentLevelIdx = levels.indexOf(lv);
  syncFromLevel();
  buildLevelTabs();
  panX = 20;
  panY = -(mapH * TILE_SIZE * zoom - canvas.height) + 40;
  autoSave();
  render();
}

function changeCurrentWorld() {
  const L = cur();
  if (L.isMenu) return;
  syncToLevel();
  const worldEl = document.getElementById('level-world');
  const w = worldEl ? parseInt(worldEl.value, 10) : 1;
  applyWorldToLevel(L, Number.isFinite(w) ? w : 1);
  if (w === 0) {
    L.isHidden = true;
    const hiddenEl = document.getElementById('level-hidden');
    if (hiddenEl) hiddenEl.checked = true;
  }
  normalizePlayableOrder();
  currentLevelIdx = levels.indexOf(L);
  syncFromLevel();
  buildLevelTabs();
  autoSave();
  render();
}

function moveCurrentLevel(delta) {
  const L = cur();
  if (L.isMenu) return;
  syncToLevel();
  const section = levelOrganizerSection(L);
  const indices = indicesInSection(section);
  const pos = indices.indexOf(currentLevelIdx);
  const newPos = pos + delta;
  if (newPos < 0 || newPos >= indices.length) return;
  const otherIdx = indices[newPos];
  const tmp = levels[currentLevelIdx];
  levels[currentLevelIdx] = levels[otherIdx];
  levels[otherIdx] = tmp;
  currentLevelIdx = otherIdx;
  reindexPlayableLevels();
  syncFromLevel();
  buildLevelTabs();
  autoSave();
  render();
}

function moveCurrentLevelToWorld() {
  const L = cur();
  if (L.isMenu) return;
  syncToLevel();
  const sel = document.getElementById('level-move-world');
  const w = sel ? parseInt(sel.value, 10) : 1;
  applyWorldToLevel(L, Number.isFinite(w) ? w : 1);
  if (w === 0) {
    L.isHidden = true;
    const hiddenEl = document.getElementById('level-hidden');
    if (hiddenEl) hiddenEl.checked = true;
  }
  normalizePlayableOrder();
  currentLevelIdx = levels.indexOf(L);
  syncFromLevel();
  buildLevelTabs();
  autoSave();
  render();
}

function addMenuLevel() {
  syncToLevel();
  let idx = findMenuLevelIdx();
  if (idx < 0) {
    const L = createLevel('Menu BG', 80, 16, 1, true);
    // Use spawn fields to store the runner's starting position
    L.spawnX = 2 * TILE_SIZE;
    L.spawnY = (16 - 3) * TILE_SIZE - 28;
    levels.push(L);
    idx = levels.length - 1;
  }
  currentLevelIdx = idx;
  syncFromLevel();
  buildLevelTabs();
  panX = 20;
  panY = 0;
  autoSave();
  render();
}

async function deleteLevel() {
  if (cur().isMenu) {
    const ok = await studioConfirm({
      title: "Delete Menu BG?",
      message: "Delete the Menu BG scene?",
      confirmLabel: "Delete",
      danger: true,
    });
    if (!ok) return;
    levels.splice(currentLevelIdx, 1);
    currentLevelIdx = 0;
    syncFromLevel();
    buildLevelTabs();
    autoSave();
    render();
    return;
  }
  if (playableLevels().length <= 1) { alert('Cannot delete the only playable level.'); return; }
  const ok = await studioConfirm({
    title: "Delete level?",
    message: `Delete "${cur().name}"?`,
    confirmLabel: "Delete",
    danger: true,
  });
  if (!ok) return;
  levels.splice(currentLevelIdx, 1);
  if (currentLevelIdx >= levels.length) currentLevelIdx = levels.length - 1;
  reindexPlayableLevels();
  syncFromLevel();
  buildLevelTabs();
  autoSave();
  render();
}

function renameCurrent() {
  cur().name = document.getElementById('level-name').value || 'Untitled';
  buildLevelTabs();
  autoSave();
}

function toggleHidden() {
  const L = cur();
  L.isHidden = document.getElementById('level-hidden').checked;
  if (L.isHidden && L.world !== 0) L.world = 0;
  if (!L.isHidden && L.world === 0) L.world = 1;
  normalizePlayableOrder();
  currentLevelIdx = levels.indexOf(L);
  buildLevelTabs();
  autoSave();
}

function setDialogueBoxesVisible() {
  const preEl = document.getElementById('dlg-pre-boxes');
  const postEl = document.getElementById('dlg-post-boxes');
  const preOn = !!document.getElementById('dlg-pre-enable')?.checked;
  const postOn = !!document.getElementById('dlg-post-enable')?.checked;
  if (preEl) preEl.classList.toggle('hidden', !preOn);
  if (postEl) postEl.classList.toggle('hidden', !postOn);
}

function updateDialogueSectionForLevel() {
  const section = document.getElementById('dialogue-section');
  const hint = document.getElementById('dialogue-hint');
  const L = cur();
  if (!section) return;
  if (L.isMenu) {
    section.classList.add('hidden');
    return;
  }
  section.classList.remove('hidden');
  if (hint) {
    hint.textContent = `Dialogue for "${L.name}". Enable Dialogue (in-game) under Level Features so lines play.`;
  }
}

function toggleDialoguePre() {
  const enabled = document.getElementById('dlg-pre-enable').checked;
  cur().dialoguePreEnabled = enabled;
  if (enabled && cur().features) cur().features.dialogue = true;
  setDialogueBoxesVisible();
  applyLevelFeaturesToUI();
  autoSave();
}

function toggleDialoguePost() {
  const enabled = document.getElementById('dlg-post-enable').checked;
  cur().dialoguePostEnabled = enabled;
  if (enabled && cur().features) cur().features.dialogue = true;
  setDialogueBoxesVisible();
  applyLevelFeaturesToUI();
  autoSave();
}

function saveDialogue() {
  if (!cur().dialoguePre) cur().dialoguePre = ['', '', '', ''];
  if (!cur().dialoguePost) cur().dialoguePost = ['', '', '', ''];
  cur().dialoguePre[0] = document.getElementById('dlg-pre-1').value;
  cur().dialoguePre[1] = document.getElementById('dlg-pre-2').value;
  cur().dialoguePre[2] = document.getElementById('dlg-pre-3').value;
  cur().dialoguePre[3] = document.getElementById('dlg-pre-4').value;
  cur().dialoguePost[0] = document.getElementById('dlg-post-1').value;
  cur().dialoguePost[1] = document.getElementById('dlg-post-2').value;
  cur().dialoguePost[2] = document.getElementById('dlg-post-3').value;
  cur().dialoguePost[3] = document.getElementById('dlg-post-4').value;
  autoSave();
}

function loadDialogue() {
  const L = cur();
  updateDialogueSectionForLevel();
  if (L.isMenu) return;

  const preEnabled = !!L.dialoguePreEnabled;
  const preEnableEl = document.getElementById('dlg-pre-enable');
  if (preEnableEl) preEnableEl.checked = preEnabled;
  const pre = L.dialoguePre || ['', '', '', ''];
  const preIds = ['dlg-pre-1', 'dlg-pre-2', 'dlg-pre-3', 'dlg-pre-4'];
  for (let i = 0; i < preIds.length; i++) {
    const el = document.getElementById(preIds[i]);
    if (el) el.value = pre[i] || '';
  }

  const postEnabled = !!L.dialoguePostEnabled;
  const postEnableEl = document.getElementById('dlg-post-enable');
  if (postEnableEl) postEnableEl.checked = postEnabled;
  const post = L.dialoguePost || ['', '', '', ''];
  const postIds = ['dlg-post-1', 'dlg-post-2', 'dlg-post-3', 'dlg-post-4'];
  for (let i = 0; i < postIds.length; i++) {
    const el = document.getElementById(postIds[i]);
    if (el) el.value = post[i] || '';
  }

  setDialogueBoxesVisible();
}

function buildLevelTabs() {
  const container = document.getElementById('level-list');
  if (!container) return;
  container.innerHTML = '';

  const sections = [
    { key: 'menu', label: 'Menu', headerClass: 'world-menu' },
    { key: '1', label: 'World 1', headerClass: 'world-1' },
    { key: '2', label: 'World 2', headerClass: 'world-2' },
    { key: 'other', label: 'Other', headerClass: 'world-2' },
    { key: 'secret', label: 'Secret', headerClass: 'world-secret' },
  ];

  for (const sec of sections) {
    const indices = indicesInSection(sec.key);
    if (!indices.length) continue;

    const header = document.createElement('div');
    header.className = 'world-section-header ' + sec.headerClass;
    header.textContent = sec.label + (sec.key !== 'menu' ? ` (${indices.length})` : '');
    container.appendChild(header);

    for (const i of indices) {
      const lv = levels[i];
      const tab = document.createElement('div');
      tab.className = 'level-item' + (i === currentLevelIdx ? ' active' : '');

      const idxSpan = !lv.isMenu && lv.levelIndex !== undefined
        ? `<span class="level-idx">${lv.levelIndex}</span>`
        : '';

      let badge = '';
      if (lv.isMenu) {
        badge = '<span class="level-badge menu">MENU</span>';
      } else if (lv.isHidden || lv.world === 0) {
        badge = '<span class="level-badge secret">SECRET</span>';
      }

      tab.innerHTML = `${idxSpan}<span class="level-name">${lv.name}</span>${badge}`;
      tab.onclick = () => switchLevel(i);
      container.appendChild(tab);
    }
  }

  updateLevelReorderButtons();
}

let selectedTile = 1;
let currentTool = 'paint';
let zoom = 1.0;
let panX = 0, panY = 0;
let isPanning = false;
let isPainting = false;
let panStartX, panStartY, panStartPX, panStartPY;
const TRACKPAD_STORAGE_KEY = 'studio_trackpad_mode';
let trackpadMode = false;
let spacePanHeld = false;
let altZoomHeld = false;
let tilesetImg = null;
let showGrid = true;
let currentStroke = null;

// Canvas (initialized on demand — avoids crashing module load before DOM is ready)
let canvas = null;
let ctx = null;
let mainDiv = null;
let editorDomReady = false;

function ensureEditorCanvas() {
  if (!canvas) {
    canvas = document.getElementById('editor-canvas');
    mainDiv = document.getElementById('center-panel');
    if (canvas) ctx = canvas.getContext('2d');
  }
  return !!(canvas && ctx && mainDiv);
}

function resizeCanvas() {
  if (!ensureEditorCanvas()) return;
  canvas.width = mainDiv.clientWidth;
  canvas.height = mainDiv.clientHeight;
  render();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Palette UI
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function buildPalette() {
  const pal = document.getElementById('tile-palette');
  if (!pal) return;
  pal.innerHTML = '';
  for (let i = 1; i < TILE_DEFS.length; i++) {
    const t = TILE_DEFS[i];
    const btn = document.createElement('div');
    btn.className = 'tile-btn' + (i === selectedTile ? ' active' : '');
    btn.dataset.tile = i;
    btn.innerHTML = `<div class="tile-swatch" style="background:${t.color}"></div>
                     <span class="tile-name">${t.key}: ${t.name}</span>`;
    btn.onclick = () => { selectedTile = i; currentTool = 'paint'; updateUI(); };
    pal.appendChild(btn);
  }
}

function updateUI() {
  document.querySelectorAll('.tile-btn').forEach(b => {
    b.classList.toggle('active', +b.dataset.tile === selectedTile);
  });
  document.querySelectorAll('.tool-btn').forEach(b => {
    b.classList.toggle('active', b.dataset.tool === currentTool);
  });
  const topbarTrackpadBtn = document.getElementById('btn-trackpad-mode');
  const trackpadToggle = document.getElementById('trackpad-mode');
  const trackpadHelp = document.getElementById('trackpad-help');
  if (topbarTrackpadBtn) topbarTrackpadBtn.textContent = trackpadMode ? 'Trackpad: On' : 'Trackpad: Off';
  if (trackpadToggle) trackpadToggle.checked = trackpadMode;
  if (trackpadHelp) {
    trackpadHelp.textContent = trackpadMode
      ? 'On: two-finger scroll pans, Alt+scroll zooms, Shift+click erases, Space+drag pans.'
      : 'Off: wheel zoom, middle mouse pan, right drag erase.';
  }
  if (mainDiv) {
    mainDiv.classList.toggle('trackpad-mode', trackpadMode);
    mainDiv.classList.toggle('space-pan', trackpadMode && spacePanHeld);
  }
  syncZoomSelectUI();
}

function bindViewportControls() {
  if (window.__viewportControlsBound) return;
  window.__viewportControlsBound = true;
  document.getElementById('btn-view-fit')?.addEventListener('click', () => {
    fitScreen();
    const sel = document.getElementById('canvas-zoom-select');
    if (sel) sel.value = 'fit';
  });
  document.getElementById('btn-view-fill')?.addEventListener('click', () => {
    fillScreen();
    const sel = document.getElementById('canvas-zoom-select');
    if (sel) sel.value = 'fill';
  });
  document.getElementById('canvas-zoom-select')?.addEventListener('change', (e) => {
    const v = e.target.value;
    if (v === 'fit') { fitScreen(); return; }
    if (v === 'fill') { fillScreen(); return; }
    const z = parseFloat(v);
    if (Number.isFinite(z)) setZoomAtViewportCenter(z);
  });
}

function bindToolButtons() {
  document.querySelectorAll('.tool-btn').forEach(b => {
    b.onclick = () => { currentTool = b.dataset.tool; updateUI(); };
  });
}

function loadTrackpadPreference() {
  try {
    return localStorage.getItem(TRACKPAD_STORAGE_KEY) === '1';
  } catch {
    return false;
  }
}

function setTrackpadMode(enabled) {
  trackpadMode = !!enabled;
  try {
    localStorage.setItem(TRACKPAD_STORAGE_KEY, trackpadMode ? '1' : '0');
  } catch {
    // ignore storage failures
  }
  if (!trackpadMode) {
    spacePanHeld = false;
    isPanning = false;
  }
  updateUI();
}

function bindTrackpadControls() {
  const toggle = document.getElementById('trackpad-mode');
  if (toggle && !toggle.dataset.bound) {
    toggle.dataset.bound = '1';
    toggle.addEventListener('change', () => setTrackpadMode(toggle.checked));
  }
  const topbarBtn = document.getElementById('btn-trackpad-mode');
  if (topbarBtn && !topbarBtn.dataset.bound) {
    topbarBtn.dataset.bound = '1';
    topbarBtn.addEventListener('click', () => setTrackpadMode(!trackpadMode));
  }
}

function shouldZoomWheelInTrackpadMode(e) {
  // Explicit gesture: Alt + two-finger scroll zooms.
  return !!(altZoomHeld || e.altKey);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Rendering
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function render() {
  if (!ensureEditorCanvas()) return;
  const w = canvas.width, h = canvas.height;
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = '#1a1a2e';
  ctx.fillRect(0, 0, w, h);

  ctx.save();
  ctx.translate(panX, panY);
  ctx.scale(zoom, zoom);

  const cellSize = TILE_SIZE;
  const vx0 = -panX / zoom, vy0 = -panY / zoom;
  const vx1 = vx0 + w / zoom, vy1 = vy0 + h / zoom;
  const col0 = Math.max(0, Math.floor(vx0 / cellSize));
  const col1 = Math.min(mapW - 1, Math.floor(vx1 / cellSize));
  const row0 = Math.max(0, Math.floor(vy0 / cellSize));
  const row1 = Math.min(mapH - 1, Math.floor(vy1 / cellSize));

  // Draw tiles
  for (let y = row0; y <= row1; y++) {
    if (!tilemap[y]) continue;
    for (let x = col0; x <= col1; x++) {
      const t = tilemap[y][x] ?? 0;
      const px = x * cellSize, py = y * cellSize;
      if (t > 0 && tilesetImg && TILE_DEFS[t].uv) {
        const uv = TILE_DEFS[t].uv;
        const sx = (uv.col - 1) * TILE_SIZE;
        const sy = (uv.row - 1) * TILE_SIZE;
        ctx.drawImage(tilesetImg, sx, sy, TILE_SIZE, TILE_SIZE, px, py, cellSize, cellSize);
      } else if (t > 0) {
        ctx.fillStyle = TILE_DEFS[t].color;
        ctx.fillRect(px, py, cellSize, cellSize);
      }
    }
  }

  // Grid lines
  if (showGrid) {
    ctx.strokeStyle = 'rgba(255,255,255,0.08)';
    ctx.lineWidth = 1 / zoom;
    for (let x = col0; x <= col1 + 1; x++) {
      ctx.beginPath(); ctx.moveTo(x * cellSize, row0 * cellSize); ctx.lineTo(x * cellSize, (row1 + 1) * cellSize); ctx.stroke();
    }
    for (let y = row0; y <= row1 + 1; y++) {
      ctx.beginPath(); ctx.moveTo(col0 * cellSize, y * cellSize); ctx.lineTo((col1 + 1) * cellSize, y * cellSize); ctx.stroke();
    }
  }

  // Map border
  ctx.strokeStyle = '#00b4d8';
  ctx.lineWidth = 2 / zoom;
  ctx.strokeRect(0, 0, mapW * cellSize, mapH * cellSize);

  // Coins
  ctx.fillStyle = '#FFD700';
  ctx.strokeStyle = '#B8860B';
  ctx.lineWidth = 2 / zoom;
  for (const c of coins) {
    ctx.beginPath();
    ctx.arc(c.x + 7, c.y + 7, 7, 0, Math.PI * 2);
    ctx.fill(); ctx.stroke();
  }

  // Enemies
  for (const en of enemies) {
    ctx.fillStyle = '#CC3333';
    ctx.strokeStyle = '#881111';
    ctx.lineWidth = 2 / zoom;
    ctx.fillRect(en.x, en.y, 24, 24);
    ctx.strokeRect(en.x, en.y, 24, 24);
    ctx.fillStyle = '#fff';
    ctx.fillRect(en.x + 4, en.y + 6, 5, 5);
    ctx.fillRect(en.x + 15, en.y + 6, 5, 5);
    ctx.fillStyle = '#000';
    ctx.fillRect(en.x + 6, en.y + 8, 2, 2);
    ctx.fillRect(en.x + 17, en.y + 8, 2, 2);
  }

  // Moving platform range visualization
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (tilemap[y][x] === 8) { // TILE_MOVING
        const px = x * cellSize, py = y * cellSize;
        const rangeW = 3 * cellSize; // 3-tile patrol range
        // Draw track line
        ctx.strokeStyle = 'rgba(155, 89, 182, 0.5)';
        ctx.lineWidth = 2 / zoom;
        ctx.setLineDash([4 / zoom, 4 / zoom]);
        const midY = py + cellSize * 0.5;
        ctx.beginPath();
        ctx.moveTo(px + cellSize * 0.5, midY);
        ctx.lineTo(px + cellSize * 0.5 + rangeW, midY);
        ctx.stroke();
        ctx.setLineDash([]);
        // Ghost at end position
        ctx.globalAlpha = 0.3;
        if (tilesetImg && TILE_DEFS[8].uv) {
          const uv = TILE_DEFS[8].uv;
          const sx = (uv.col - 1) * TILE_SIZE;
          const sy2 = (uv.row - 1) * TILE_SIZE;
          ctx.drawImage(tilesetImg, sx, sy2, TILE_SIZE, TILE_SIZE, px + rangeW, py, cellSize, cellSize);
        } else {
          ctx.fillStyle = TILE_DEFS[8].color;
          ctx.fillRect(px + rangeW, py, cellSize, cellSize);
        }
        ctx.globalAlpha = 1.0;
        // Arrow
        ctx.fillStyle = 'rgba(155, 89, 182, 0.8)';
        ctx.beginPath();
        ctx.moveTo(px + rangeW - 4 / zoom, midY - 6 / zoom);
        ctx.lineTo(px + rangeW + cellSize * 0.5, midY);
        ctx.lineTo(px + rangeW - 4 / zoom, midY + 6 / zoom);
        ctx.fill();
      }
    }
  }

  // One-way platform arrows (upward arrow overlay)
  ctx.fillStyle = 'rgba(107, 174, 214, 0.7)';
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (tilemap[y][x] === 5) { // TILE_PLAT_ALT / one-way
        const px = x * cellSize + cellSize * 0.5;
        const py2 = y * cellSize + 4 / zoom;
        const sz = 5 / zoom;
        ctx.beginPath();
        ctx.moveTo(px, py2);
        ctx.lineTo(px - sz, py2 + sz);
        ctx.lineTo(px + sz, py2 + sz);
        ctx.fill();
      }
    }
  }

  // Crumble tile warning cracks overlay
  ctx.strokeStyle = 'rgba(180, 140, 60, 0.6)';
  ctx.lineWidth = 1.5 / zoom;
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (tilemap[y][x] === 7) { // TILE_CRUMBLE
        const px = x * cellSize, py2 = y * cellSize;
        ctx.beginPath();
        ctx.moveTo(px + cellSize * 0.2, py2 + cellSize * 0.3);
        ctx.lineTo(px + cellSize * 0.5, py2 + cellSize * 0.5);
        ctx.lineTo(px + cellSize * 0.4, py2 + cellSize * 0.8);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(px + cellSize * 0.7, py2 + cellSize * 0.2);
        ctx.lineTo(px + cellSize * 0.6, py2 + cellSize * 0.6);
        ctx.stroke();
      }
    }
  }

  // 3D tile overlay (cyan border + "3D" label)
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (tile3d[y] && tile3d[y][x]) {
        const px = x * cellSize, py2 = y * cellSize;
        ctx.strokeStyle = 'rgba(0, 220, 255, 0.8)';
        ctx.lineWidth = 2.5 / zoom;
        ctx.strokeRect(px + 2/zoom, py2 + 2/zoom, cellSize - 4/zoom, cellSize - 4/zoom);
        ctx.fillStyle = 'rgba(0, 220, 255, 0.25)';
        ctx.fillRect(px, py2, cellSize, cellSize);
        ctx.fillStyle = '#00ffff';
        ctx.font = `bold ${10/zoom}px sans-serif`;
        ctx.fillText('3D', px + 2/zoom, py2 + cellSize - 3/zoom);
      }
    }
  }

  // Win/goal overlay (gold flag)
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (winOverlay[y] && winOverlay[y][x]) {
        const px = x * cellSize, py2 = y * cellSize;
        ctx.fillStyle = 'rgba(255, 215, 0, 0.45)';
        ctx.fillRect(px + 2/zoom, py2 + 2/zoom, cellSize - 4/zoom, cellSize - 4/zoom);
        ctx.strokeStyle = '#FFD700';
        ctx.lineWidth = 2 / zoom;
        ctx.strokeRect(px + 2/zoom, py2 + 2/zoom, cellSize - 4/zoom, cellSize - 4/zoom);
        ctx.fillStyle = '#fff8dc';
        ctx.fillRect(px + cellSize * 0.42, py2 + 3/zoom, cellSize * 0.12, cellSize * 0.35);
        ctx.fillStyle = '#fff';
        ctx.font = `bold ${11/zoom}px sans-serif`;
        ctx.fillText('★', px + cellSize/2 - 5/zoom, py2 + cellSize/2 + 4/zoom);
      }
    }
  }

  // Checkpoint markers (8×32 red/green poles)
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (checkpointOverlay[y] && checkpointOverlay[y][x]) {
        const px = x * cellSize + (cellSize - 8/zoom) * 0.5;
        const py2 = y * cellSize;
        ctx.fillStyle = 'rgba(220, 40, 40, 0.85)';
        ctx.fillRect(px, py2, 8/zoom, 32/zoom);
        ctx.strokeStyle = '#ff6666';
        ctx.lineWidth = 1 / zoom;
        ctx.strokeRect(px, py2, 8/zoom, 32/zoom);
      }
    }
  }

  // Warp tile overlay (teal diamond + "W" label)
  for (let y = row0; y <= row1; y++) {
    for (let x = col0; x <= col1; x++) {
      if (warpOverlay[y] && warpOverlay[y][x]) {
        const px = x * cellSize, py2 = y * cellSize;
        const cx2 = px + cellSize/2, cy2 = py2 + cellSize/2;
        ctx.fillStyle = 'rgba(0, 206, 209, 0.4)';
        ctx.beginPath();
        ctx.moveTo(cx2, py2 + 3/zoom);
        ctx.lineTo(px + cellSize - 3/zoom, cy2);
        ctx.lineTo(cx2, py2 + cellSize - 3/zoom);
        ctx.lineTo(px + 3/zoom, cy2);
        ctx.closePath();
        ctx.fill();
        ctx.strokeStyle = '#00CED1';
        ctx.lineWidth = 2 / zoom;
        ctx.stroke();
        ctx.fillStyle = '#fff';
        ctx.font = `bold ${11/zoom}px sans-serif`;
        ctx.fillText('W', px + cellSize/2 - 4/zoom, py2 + cellSize/2 + 4/zoom);
      }
    }
  }

  // Spawn
  ctx.fillStyle = '#00FF88';
  ctx.strokeStyle = '#008844';
  ctx.lineWidth = 2 / zoom;
  ctx.fillRect(spawnX, spawnY, 16, 28);
  ctx.strokeRect(spawnX, spawnY, 16, 28);
  ctx.fillStyle = '#fff';
  ctx.font = `10px sans-serif`;
  ctx.fillText('P', spawnX + 3, spawnY + 18);

  ctx.restore();
  updateLevelBudgetUI();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Mouse â†’ world coords
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function screenToWorld(sx, sy) {
  return { x: (sx - panX) / zoom, y: (sy - panY) / zoom };
}
function worldToTile(wx, wy) {
  return { tx: Math.floor(wx / TILE_SIZE), ty: Math.floor(wy / TILE_SIZE) };
}
function inBounds(tx, ty) {
  return tx >= 0 && tx < mapW && ty >= 0 && ty < mapH;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Undo/Redo
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function pushUndo(action) {
  undoStack.push(action);
  if (undoStack.length > 200) undoStack.shift();
  redoStack.length = 0;
}

function undo() {
  if (!undoStack.length) return;
  const action = undoStack.pop();
  if (action.type === 'tiles') {
    for (const c of action.changes) tilemap[c.y][c.x] = c.oldTile;
  } else if (action.type === 'coin-add') {
    coins = coins.filter(c => c !== action.coin);
    cur().coins = coins;
  } else if (action.type === 'coin-remove') {
    coins.push(action.coin);
    cur().coins = coins;
  } else if (action.type === 'enemy-add') {
    enemies = enemies.filter(e => e !== action.enemy);
    cur().enemies = enemies;
  } else if (action.type === 'enemy-remove') {
    enemies.push(action.enemy);
    cur().enemies = enemies;
  } else if (action.type === 'spawn') {
    spawnX = action.oldX; spawnY = action.oldY;
    cur().spawnX = spawnX; cur().spawnY = spawnY;
  }
  redoStack.push(action);
  autoSave();
  render();
}

function redo() {
  if (!redoStack.length) return;
  const action = redoStack.pop();
  if (action.type === 'tiles') {
    for (const c of action.changes) tilemap[c.y][c.x] = c.newTile;
  } else if (action.type === 'coin-add') {
    coins.push(action.coin);
    cur().coins = coins;
  } else if (action.type === 'coin-remove') {
    coins = coins.filter(c => c !== action.coin);
    cur().coins = coins;
  } else if (action.type === 'enemy-add') {
    enemies.push(action.enemy);
    cur().enemies = enemies;
  } else if (action.type === 'enemy-remove') {
    enemies = enemies.filter(e => e !== action.enemy);
    cur().enemies = enemies;
  } else if (action.type === 'spawn') {
    spawnX = action.newX; spawnY = action.newY;
    cur().spawnX = spawnX; cur().spawnY = spawnY;
  }
  undoStack.push(action);
  autoSave();
  render();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Painting
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function setTile(tx, ty, value) {
  if (!inBounds(tx, ty)) return;
  const old = tilemap[ty][tx];
  if (old === value) return;
  tilemap[ty][tx] = value;
  if (currentStroke) {
    currentStroke.changes.push({ x: tx, y: ty, oldTile: old, newTile: value });
  }
}

function floodFill(tx, ty, newTile) {
  if (!inBounds(tx, ty)) return;
  const oldTile = tilemap[ty][tx];
  if (oldTile === newTile) return;
  const changes = [];
  const stack = [[tx, ty]];
  const visited = new Set();
  while (stack.length) {
    const [cx, cy] = stack.pop();
    const key = cy * mapW + cx;
    if (visited.has(key)) continue;
    if (!inBounds(cx, cy)) continue;
    if (tilemap[cy][cx] !== oldTile) continue;
    visited.add(key);
    changes.push({ x: cx, y: cy, oldTile, newTile });
    tilemap[cy][cx] = newTile;
    stack.push([cx-1, cy], [cx+1, cy], [cx, cy-1], [cx, cy+1]);
  }
  if (changes.length) pushUndo({ type: 'tiles', changes });
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Mouse events
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function bindCanvasEvents() {
  if (!ensureEditorCanvas() || canvas.dataset.eventsBound) return;
  canvas.dataset.eventsBound = '1';

canvas.addEventListener('mousedown', (e) => {
  const rect = canvas.getBoundingClientRect();
  const sx = e.clientX - rect.left, sy = e.clientY - rect.top;
  const { x: wx, y: wy } = screenToWorld(sx, sy);
  const { tx, ty } = worldToTile(wx, wy);

  const wantsPan = e.button === 1 || (trackpadMode && e.button === 0 && spacePanHeld);
  if (wantsPan) {
    isPanning = true;
    panStartX = e.clientX; panStartY = e.clientY;
    panStartPX = panX; panStartPY = panY;
    e.preventDefault();
    return;
  }

  // Right-click: erase tiles only (never coins/spawn)
  if (e.button === 2 || (trackpadMode && e.button === 0 && e.shiftKey)) {
    if (inBounds(tx, ty) && tilemap[ty][tx] !== 0) {
      isPainting = true;
      currentStroke = { changes: [] };
      setTile(tx, ty, 0);
      render();
    }
    return;
  }

  if (e.button === 0) {
    if (currentTool === 'paint') {
      isPainting = true;
      currentStroke = { changes: [] };
      setTile(tx, ty, selectedTile);
      render();
    } else if (currentTool === 'erase') {
      // Erase tool: only erases tiles, ignores coins/spawn
      if (inBounds(tx, ty) && tilemap[ty][tx] !== 0) {
        isPainting = true;
        currentStroke = { changes: [] };
        setTile(tx, ty, 0);
        render();
      }
    } else if (currentTool === 'fill') {
      floodFill(tx, ty, selectedTile);
      render();
    } else if (currentTool === 'coin') {
      const cx = tx * TILE_SIZE + 9, cy = ty * TILE_SIZE + 9;
      const existing = coins.findIndex(c => Math.abs(c.x - cx) < 14 && Math.abs(c.y - cy) < 14);
      if (existing >= 0) {
        const removed = coins.splice(existing, 1)[0];
        pushUndo({ type: 'coin-remove', coin: removed });
      } else {
        const coin = { x: cx, y: cy };
        coins.push(coin);
        pushUndo({ type: 'coin-add', coin });
      }
      cur().coins = coins;
      autoSave();
      render();
    } else if (currentTool === 'enemy') {
      // Snap to tile grid, offset to feet position
      const ex = tx * TILE_SIZE + 4;
      const ey = ty * TILE_SIZE + (TILE_SIZE - 24);
      const existing = enemies.findIndex(e => Math.abs(e.x - ex) < 24 && Math.abs(e.y - ey) < 24);
      if (existing >= 0) {
        const removed = enemies.splice(existing, 1)[0];
        pushUndo({ type: 'enemy-remove', enemy: removed });
      } else {
        const enemy = { x: ex, y: ey };
        enemies.push(enemy);
        pushUndo({ type: 'enemy-add', enemy });
      }
      cur().enemies = enemies;
      autoSave();
      render();
    } else if (currentTool === 'spawn') {
      const oldX = spawnX, oldY = spawnY;
      spawnX = wx - 8; spawnY = wy - 14;
      cur().spawnX = spawnX; cur().spawnY = spawnY;
      pushUndo({ type: 'spawn', oldX, oldY, newX: spawnX, newY: spawnY });
      autoSave();
      render();
    } else if (currentTool === 'tile3d') {
      if (inBounds(tx, ty)) {
        tile3d[ty][tx] = !tile3d[ty][tx];
        autoSave();
        render();
      }
    } else if (currentTool === 'warp') {
      if (inBounds(tx, ty)) {
        warpOverlay[ty][tx] = !warpOverlay[ty][tx];
        // Prompt for warp target level when placing (not removing)
        if (warpOverlay[ty][tx]) {
          const target = prompt('Warp target level index (0-based):', cur().warpTarget >= 0 ? cur().warpTarget : '10');
          if (target !== null) cur().warpTarget = parseInt(target) || 0;
        }
        autoSave();
        render();
      }
    } else if (currentTool === 'win') {
      if (inBounds(tx, ty)) {
        winOverlay[ty][tx] = !winOverlay[ty][tx];
        autoSave();
        render();
      }
    } else if (currentTool === 'checkpoint') {
      if (inBounds(tx, ty)) {
        checkpointOverlay[ty][tx] = !checkpointOverlay[ty][tx];
        autoSave();
        render();
      }
    }
  }
});

canvas.addEventListener('mousemove', (e) => {
  const rect = canvas.getBoundingClientRect();
  const sx = e.clientX - rect.left, sy = e.clientY - rect.top;
  const { x: wx, y: wy } = screenToWorld(sx, sy);
  const { tx, ty } = worldToTile(wx, wy);

  if (inBounds(tx, ty)) {
    const tname = TILE_DEFS[tilemap[ty][tx]].name;
    document.getElementById('canvas-status').textContent =
      `Tile (${tx}, ${ty}) â€” ${tname}  |  World (${Math.round(wx)}, ${Math.round(wy)})  |  ${cur().name}`;
  }

  if (isPanning) {
    panX = panStartPX + (e.clientX - panStartX);
    panY = panStartPY + (e.clientY - panStartY);
    render();
    return;
  }

  if (isPainting) {
    const trackpadEraseDrag = trackpadMode && (e.buttons & 1) && e.shiftKey;
    if ((e.buttons & 2) || trackpadEraseDrag) {
      // Right-drag: erase tiles only
      if (inBounds(tx, ty) && tilemap[ty][tx] !== 0) {
        setTile(tx, ty, 0);
        render();
      }
    } else if (currentTool === 'erase') {
      if (inBounds(tx, ty) && tilemap[ty][tx] !== 0) {
        setTile(tx, ty, 0);
        render();
      }
    } else {
      setTile(tx, ty, selectedTile);
      render();
    }
  }
});

canvas.addEventListener('mouseup', (e) => {
  if (e.button === 1 || (trackpadMode && e.button === 0)) isPanning = false;
  if (isPainting) {
    isPainting = false;
    if (currentStroke && currentStroke.changes.length) {
      pushUndo({ type: 'tiles', ...currentStroke });
      autoSave();
    }
    currentStroke = null;
  }
});

canvas.addEventListener('mouseleave', () => {
  isPanning = false;
  if (isPainting) {
    isPainting = false;
    if (currentStroke && currentStroke.changes.length) {
      pushUndo({ type: 'tiles', ...currentStroke });
    }
    currentStroke = null;
  }
});

canvas.addEventListener('wheel', (e) => {
  e.preventDefault();
  if (!e.altKey) altZoomHeld = false;
  const rect = canvas.getBoundingClientRect();
  const mx = e.clientX - rect.left, my = e.clientY - rect.top;
  if (!trackpadMode || shouldZoomWheelInTrackpadMode(e)) {
    const oldZoom = zoom;
    const delta = e.deltaY > 0 ? 0.9 : 1.1;
    zoom = clampZoom(zoom * delta);
    panX = mx - (mx - panX) * (zoom / oldZoom);
    panY = my - (my - panY) * (zoom / oldZoom);
    updateUI();
  } else {
    panX -= e.deltaX;
    panY -= e.deltaY;
  }
  render();
}, { passive: false });

canvas.addEventListener('contextmenu', (e) => e.preventDefault());
}

function bindEditorKeyboard() {
  if (window.__editorKeydownBound) return;
  window.__editorKeydownBound = true;
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Alt') {
      altZoomHeld = true;
      // Prevent app/webview menu focus stealing after Alt gestures.
      e.preventDefault();
      return;
    }
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    if (e.code === 'Space' && trackpadMode) {
      if (!spacePanHeld) {
        spacePanHeld = true;
        updateUI();
      }
      e.preventDefault();
      return;
    }
    const panStep = e.shiftKey ? 72 : 36;
    if (e.key === 'ArrowLeft') { panX += panStep; render(); e.preventDefault(); return; }
    if (e.key === 'ArrowRight') { panX -= panStep; render(); e.preventDefault(); return; }
    if (e.key === 'ArrowUp') { panY += panStep; render(); e.preventDefault(); return; }
    if (e.key === 'ArrowDown') { panY -= panStep; render(); e.preventDefault(); return; }
    if (e.key === '+' || e.key === '=') {
      zoom = clampZoom(zoom * 1.1);
      updateUI();
      render();
      e.preventDefault();
      return;
    }
    if (e.key === '-' || e.key === '_') {
      zoom = clampZoom(zoom * 0.9);
      updateUI();
      render();
      e.preventDefault();
      return;
    }
    if (e.key === 't' || e.key === 'T') { setTrackpadMode(!trackpadMode); e.preventDefault(); return; }
    if (e.ctrlKey && e.key === 'z') { e.preventDefault(); undo(); return; }
    if (e.ctrlKey && e.key === 'y') { e.preventDefault(); redo(); return; }
    if (e.key >= '1' && e.key <= '9') { selectedTile = +e.key; currentTool = 'paint'; updateUI(); }
    if (e.key === 'e' || e.key === 'E') { currentTool = 'erase'; updateUI(); }
    if (e.key === 'f' || e.key === 'F') { currentTool = 'fill'; updateUI(); }
    if (e.key === 'c' || e.key === 'C') { currentTool = 'coin'; updateUI(); }
    if (e.key === 'n' || e.key === 'N') { currentTool = 'enemy'; updateUI(); }
    if (e.key === 's' && !e.ctrlKey) { currentTool = 'spawn'; updateUI(); }
    if (e.key === 'd' || e.key === 'D') { currentTool = 'tile3d'; updateUI(); }
    if (e.key === 'w' || e.key === 'W') { currentTool = 'warp'; updateUI(); }
    if (e.key === 'v' || e.key === 'V') { currentTool = 'win'; updateUI(); }
    if (e.key === 'h' || e.key === 'H') { currentTool = 'checkpoint'; updateUI(); }
    if (e.key === 'g' || e.key === 'G') { showGrid = !showGrid; render(); }
  });
  document.addEventListener('keyup', (e) => {
    if (e.key === 'Alt') {
      altZoomHeld = false;
      e.preventDefault();
      return;
    }
    if (e.code === 'Space' && trackpadMode && spacePanHeld) {
      spacePanHeld = false;
      isPanning = false;
      updateUI();
    }
  });
  window.addEventListener('blur', () => { altZoomHeld = false; });
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Tileset loading
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function loadTileset(event) {
  const file = event.target.files[0];
  if (!file) return;
  const img = new Image();
  img.onload = () => { tilesetImg = img; render(); };
  img.src = URL.createObjectURL(file);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Map resize / clear
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function resizeMap() {
  const newW = Math.min(MAX_MAP_W, Math.max(10, +document.getElementById('map-w').value || 80));
  const newH = Math.min(MAX_MAP_H, Math.max(5, +document.getElementById('map-h').value || 16));
  const newMap = [];
  const new3d = [];
  const newWarp = [];
  const newWin = [];
  const newCp = [];
  for (let y = 0; y < newH; y++) {
    const row = new Array(newW).fill(0);
    const row3d = new Array(newW).fill(false);
    const rowWarp = new Array(newW).fill(false);
    const rowWin = new Array(newW).fill(false);
    const rowCp = new Array(newW).fill(false);
    for (let x = 0; x < Math.min(newW, mapW); x++) {
      if (y < mapH) {
        row[x] = tilemap[y][x];
        if (tile3d[y]) row3d[x] = tile3d[y][x] || false;
        if (warpOverlay[y]) rowWarp[x] = warpOverlay[y][x] || false;
        if (winOverlay[y]) rowWin[x] = winOverlay[y][x] || false;
        if (checkpointOverlay[y]) rowCp[x] = checkpointOverlay[y][x] || false;
      }
    }
    newMap.push(row);
    new3d.push(row3d);
    newWarp.push(rowWarp);
    newWin.push(rowWin);
    newCp.push(rowCp);
  }
  tilemap = newMap;
  tile3d = new3d;
  warpOverlay = newWarp;
  winOverlay = newWin;
  checkpointOverlay = newCp;
  mapW = newW; mapH = newH;
  undoStack.length = 0; redoStack.length = 0;
  syncToLevel();
  autoSave();
  render();
}

async function clearMap() {
  const ok = await studioConfirm({
    title: "Clear level?",
    message: `Clear "${cur().name}"?`,
    confirmLabel: "Clear",
    danger: true,
  });
  if (!ok) return;
  for (let y = 0; y < mapH; y++) {
    tilemap[y].fill(0);
    if (tile3d[y]) tile3d[y].fill(false);
    if (warpOverlay[y]) warpOverlay[y].fill(false);
    if (winOverlay[y]) winOverlay[y].fill(false);
    if (checkpointOverlay[y]) checkpointOverlay[y].fill(false);
  }
  cur().warpTarget = -1;
  coins.length = 0;
  enemies.length = 0;
  spawnX = 1.5 * TILE_SIZE;
  spawnY = (mapH - 2) * TILE_SIZE - 28;
  undoStack.length = 0; redoStack.length = 0;
  syncToLevel();
  autoSave();
  render();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Export C++ (single level)
export const TILE_NAMES = ['TILE_EMPTY', 'TILE_GROUND', 'TILE_FILL', 'TILE_SPIKE', 'TILE_PLATFORM', 'TILE_PLAT_ALT', 'TILE_BG_DECOR', 'TILE_CRUMBLE', 'TILE_MOVING', 'TILE_WARP'];

export function generateSyncBlock() {
  syncToLevel();
  const parts = [];
  parts.push('// @@EDITOR_LEVELS_START@@');

  // Generate LEVEL_INFO table from level metadata
  const playable = levels.filter(l => !l.isMenu);
  if (playable.length > 0) {
    parts.push('');
    parts.push('// Level metadata â€” auto-generated from editor');
    parts.push('static const int EDITOR_LEVEL_COUNT = ' + playable.length + ';');
    parts.push('#undef LEVEL_COUNT_MACRO');
    parts.push('#define LEVEL_COUNT_MACRO EDITOR_LEVEL_COUNT');
    parts.push('static const LevelInfo EDITOR_LEVEL_INFO[EDITOR_LEVEL_COUNT] = {');
    for (let i = 0; i < playable.length; i++) {
      const lv = playable[i];
      const hidden = lv.isHidden ? 'true' : 'false';
      // Default par time based on world
      const exportWorld = lv.isHidden || lv.world === 0 ? 0 : (lv.world ?? 1);
      const parTime = (lv.parTime != null ? lv.parTime : defaultParTime(exportWorld)).toFixed(1) + 'f';
      normalizeLevelFeatures(lv);
      const f = lv.features;
      parts.push(
        '\t/* ' + i + ' */ {"' + lv.name + '", ' + exportWorld + ', ' + hidden + ', ' + parTime
        + ', ' + lv.mapW + ', ' + lv.mapH
        + ', ' + cppBool(f.doubleJump) + ', ' + cppBool(f.dialogue)
        + ', ' + cppBool(f.wallJump) + ', ' + cppBool(f.dash) + ', ' + cppBool(f.groundPound)
        + ', ' + cppBool(f.minimap) + '},'
      );
    }
    parts.push('};');
    parts.push('');
    parts.push('// Per-level physics overrides — auto-generated from editor');
    parts.push('static const LevelPhysics EDITOR_LEVEL_PHYSICS[EDITOR_LEVEL_COUNT] = {');
    for (let i = 0; i < playable.length; i++) {
      parts.push(exportPhysicsCppRow(playable[i], i));
    }
    parts.push('};');
    parts.push('#undef LEVEL_PHYSICS_TABLE');
    parts.push('#define LEVEL_PHYSICS_TABLE EDITOR_LEVEL_PHYSICS');
    parts.push('');
    parts.push('#undef LEVEL_INFO_MACRO');
    parts.push('#define LEVEL_INFO_MACRO EDITOR_LEVEL_INFO');
    parts.push('');
    parts.push('#undef LEVEL_COUNT');
    parts.push('#undef LEVEL_INFO');
    parts.push('#define LEVEL_COUNT EDITOR_LEVEL_COUNT');
    parts.push('#define LEVEL_INFO  EDITOR_LEVEL_INFO');
    parts.push('');
    parts.push('static int getActiveMapHeight() {');
    parts.push('\tif (currentLevel >= 0 && currentLevel < EDITOR_LEVEL_COUNT)');
    parts.push('\t\treturn EDITOR_LEVEL_INFO[currentLevel].height;');
    parts.push('\tif (currentLevel >= 0 && currentLevel < TOTAL_LEVELS)');
    parts.push('\t\treturn FALLBACK_LEVEL_INFO[currentLevel].height;');
    parts.push('\treturn MAP_H;');
    parts.push('}');
    parts.push('');
    parts.push('static int getActiveMapWidth() {');
    parts.push('\tif (currentLevel >= 0 && currentLevel < EDITOR_LEVEL_COUNT)');
    parts.push('\t\treturn EDITOR_LEVEL_INFO[currentLevel].width;');
    parts.push('\tif (currentLevel >= 0 && currentLevel < TOTAL_LEVELS)');
    parts.push('\t\treturn FALLBACK_LEVEL_INFO[currentLevel].width;');
    parts.push('\treturn MAP_W;');
    parts.push('}');
    parts.push('');
    parts.push(applyLevelPhysicsFunctionCpp());
    parts.push('');
    parts.push('// Secret levels: use LEVEL_INFO[i].isHidden (legacy levelHidden[] disabled by 3DS Studio)');
  }

  // Emit Menu BG (if one exists), or a default placeholder if none.
  const menuLv = levels.find(l => l.isMenu);
  {
    const lines = [];
    lines.push('// Menu scene â€” autogenerated from the editor.');
    lines.push('static void buildMenuScene() {');
    lines.push('\tmemset(menuTilemap, TILE_EMPTY, sizeof(menuTilemap));');
    if (menuLv && menuHasTiles(menuLv)) {
      const rows = Math.min(menuLv.mapH, 16);
      const cols = Math.min(menuLv.mapW, 80);
      for (let y = 0; y < rows; y++) {
        let rowTiles = [];
        let x = 0;
        while (x < cols) {
          const t = menuLv.tilemap[y][x];
          if (t === 0) { x++; continue; }
          let end = x;
          while (end + 1 < cols && menuLv.tilemap[y][end + 1] === t) end++;
          rowTiles.push({ t, x0: x, x1: end });
          x = end + 1;
        }
        if (rowTiles.length === 0) continue;
        lines.push('\t// Row ' + y);
        for (const run of rowTiles) {
          if (run.x0 === run.x1) {
            lines.push('\tmenuTilemap[' + y + '][' + run.x0 + '] = ' + TILE_NAMES[run.t] + ';');
          } else {
            lines.push('\tfor (int x = ' + run.x0 + '; x <= ' + run.x1 + '; x++)');
            lines.push('\t\tmenuTilemap[' + y + '][x] = ' + TILE_NAMES[run.t] + ';');
          }
        }
      }
      lines.push('');
      lines.push('\t// Runner spawn (uses the spawn marker placed in the editor)');
      lines.push('\tmenuRunnerStartX = ' + menuLv.spawnX.toFixed(1) + 'f;');
      lines.push('\tmenuRunnerStartY = ' + menuLv.spawnY.toFixed(1) + 'f;');
    } else if (menuLv) {
      lines.push('\tfor (int x = 0; x < MENU_MAP_W; x++) {');
      lines.push('\t\tmenuTilemap[13][x] = TILE_GROUND;');
      lines.push('\t\tmenuTilemap[14][x] = TILE_FILL;');
      lines.push('\t\tmenuTilemap[15][x] = TILE_FILL;');
      lines.push('\t}');
      lines.push('\tmenuRunnerStartX = ' + menuLv.spawnX.toFixed(1) + 'f;');
      lines.push('\tmenuRunnerStartY = ' + menuLv.spawnY.toFixed(1) + 'f;');
    } else {
      // No menu scene authored â€” provide a basic floor so the runner has ground.
      lines.push('\tfor (int x = 0; x < MENU_MAP_W; x++) {');
      lines.push('\t\tmenuTilemap[13][x] = TILE_GROUND;');
      lines.push('\t\tmenuTilemap[14][x] = TILE_FILL;');
      lines.push('\t\tmenuTilemap[15][x] = TILE_FILL;');
      lines.push('\t}');
      lines.push('\tmenuRunnerStartX = 2 * TILE_SIZE;');
      lines.push('\tmenuRunnerStartY = 13 * TILE_SIZE - 28;');
    }
    lines.push('}');
    parts.push(lines.join('\n'));
  }

  // Generate individual level functions (playable already defined above)
  for (let i = 0; i < playable.length; i++) {
    const lv = playable[i];
    const funcName = 'buildLevel_' + (i + 1);
    const hiddenFlag = lv.isHidden ? ', HIDDEN' : '';
    const wNum = levelWorldNum(lv);
    const worldLabel = wNum === 0 ? 'Secret' : ('World ' + wNum);
    const lines = [];
    lines.push('// Level ' + i + ': "' + lv.name + '", ' + worldLabel + hiddenFlag);
    lines.push('static void ' + funcName + '() {');
    lines.push('\tmemset(tilemap, TILE_EMPTY, sizeof(tilemap));');
    lines.push('\tnumCoins = 0;');
    lines.push('\tnumEnemies = 0;');
    lines.push('\tfor (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;');
    lines.push('');
    for (let y = 0; y < lv.mapH; y++) {
      let rowTiles = [];
      let x = 0;
      while (x < lv.mapW) {
        const t = lv.tilemap[y][x];
        if (t === 0) { x++; continue; }
        let end = x;
        while (end + 1 < lv.mapW && lv.tilemap[y][end + 1] === t) end++;
        rowTiles.push({ t, x0: x, x1: end });
        x = end + 1;
      }
      if (rowTiles.length === 0) continue;
      lines.push('\t// Row ' + y);
      for (const run of rowTiles) {
        if (run.x0 === run.x1) {
          lines.push('\ttilemap[' + y + '][' + run.x0 + '] = ' + TILE_NAMES[run.t] + ';');
        } else {
          lines.push('\tfor (int x = ' + run.x0 + '; x <= ' + run.x1 + '; x++)');
          lines.push('\t\ttilemap[' + y + '][x] = ' + TILE_NAMES[run.t] + ';');
        }
      }
    }
    if (lv.coins.length) {
      lines.push('');
      lines.push('\t// Crackers');
      for (let ci = 0; ci < lv.coins.length; ci++) {
        const c = lv.coins[ci];
        const bob = (ci * 0.3).toFixed(1) + 'f';
        lines.push('\tcoins[numCoins++] = { { ' + c.x.toFixed(1) + 'f, ' + c.y.toFixed(1) + 'f, 14, 14 }, true, ' + bob + ' };');
      }
    }
    if (lv.enemies && lv.enemies.length) {
      lines.push('');
      lines.push('\t// Enemies');
      for (const en of lv.enemies) {
        lines.push('\tspawnEnemy(' + en.x.toFixed(1) + 'f, ' + en.y.toFixed(1) + 'f);');
      }
    }
    // 3D tile overlay
    if (lv.tile3d) {
      let has3d = false;
      for (let y = 0; y < lv.mapH && !has3d; y++)
        for (let x = 0; x < lv.mapW && !has3d; x++)
          if (lv.tile3d[y] && lv.tile3d[y][x]) has3d = true;
      if (has3d) {
        lines.push('');
        lines.push('\t// 3D tiles');
        for (let y = 0; y < lv.mapH; y++) {
          for (let x = 0; x < lv.mapW; x++) {
            if (lv.tile3d[y] && lv.tile3d[y][x]) {
              lines.push('\ttile3dMap[' + y + '][' + x + '] = true;');
            }
          }
        }
      }
    }
    // Warp overlay
    if (lv.warpOverlay) {
      let hasWarp = false;
      for (let y = 0; y < lv.mapH && !hasWarp; y++)
        for (let x = 0; x < lv.mapW && !hasWarp; x++)
          if (lv.warpOverlay[y] && lv.warpOverlay[y][x]) hasWarp = true;
      if (hasWarp) {
        lines.push('');
        lines.push('\t// Warp tiles');
        for (let y = 0; y < lv.mapH; y++) {
          for (let x = 0; x < lv.mapW; x++) {
            if (lv.warpOverlay[y] && lv.warpOverlay[y][x]) {
              lines.push('\twarpMap[' + y + '][' + x + '] = true;');
            }
          }
        }
        if (lv.warpTarget >= 0) {
          lines.push('\twarpTargetLevel = ' + lv.warpTarget + ';');
        }
      }
    }
    // Win/goal overlay
    if (lv.winOverlay) {
      let hasWin = false;
      for (let y = 0; y < lv.mapH && !hasWin; y++)
        for (let x = 0; x < lv.mapW && !hasWin; x++)
          if (lv.winOverlay[y] && lv.winOverlay[y][x]) hasWin = true;
      if (hasWin) {
        lines.push('');
        lines.push('\t// Win / goal tiles');
        for (let y = 0; y < lv.mapH; y++) {
          for (let x = 0; x < lv.mapW; x++) {
            if (lv.winOverlay[y] && lv.winOverlay[y][x]) {
              lines.push('\twinMap[' + y + '][' + x + '] = true;');
            }
          }
        }
      }
    }
    // Checkpoint overlay
    if (lv.checkpointOverlay) {
      let hasCp = false;
      for (let y = 0; y < lv.mapH && !hasCp; y++)
        for (let x = 0; x < lv.mapW && !hasCp; x++)
          if (lv.checkpointOverlay[y] && lv.checkpointOverlay[y][x]) hasCp = true;
      if (hasCp) {
        lines.push('');
        lines.push('\t// Checkpoints');
        for (let y = 0; y < lv.mapH; y++) {
          for (let x = 0; x < lv.mapW; x++) {
            if (lv.checkpointOverlay[y] && lv.checkpointOverlay[y][x]) {
              lines.push('\tcheckpointMap[' + y + '][' + x + '] = true;');
            }
          }
        }
      }
    }
    lines.push('');
    lines.push('\t// Player spawn');
    lines.push('\tspawnX = ' + lv.spawnX.toFixed(1) + 'f;');
    lines.push('\tspawnY = ' + lv.spawnY.toFixed(1) + 'f;');

    // Dialogue
    if (lv.dialoguePreEnabled && lv.dialoguePre) {
      let hasPre = false;
      for (let i = 0; i < 4; i++) if (lv.dialoguePre[i] && lv.dialoguePre[i].trim()) hasPre = true;
      if (hasPre) {
        lines.push('');
        lines.push('\t// Pre-level dialogue');
        lines.push('\tdialoguePreCount[currentLevel] = 0;');
        for (let i = 0; i < 4; i++) {
          if (lv.dialoguePre[i] && lv.dialoguePre[i].trim()) {
            const text = lv.dialoguePre[i].replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
            lines.push('\tstrncpy(dialoguePre[currentLevel][' + i + '], "' + text + '", MAX_DIALOGUE_CHARS - 1);');
            lines.push('\tdialoguePreCount[currentLevel]++;');
          }
        }
      }
    }
    if (lv.dialoguePostEnabled && lv.dialoguePost) {
      let hasPost = false;
      for (let i = 0; i < 4; i++) if (lv.dialoguePost[i] && lv.dialoguePost[i].trim()) hasPost = true;
      if (hasPost) {
        lines.push('');
        lines.push('\t// Post-level dialogue');
        lines.push('\tdialoguePostCount[currentLevel] = 0;');
        for (let i = 0; i < 4; i++) {
          if (lv.dialoguePost[i] && lv.dialoguePost[i].trim()) {
            const text = lv.dialoguePost[i].replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
            lines.push('\tstrncpy(dialoguePost[currentLevel][' + i + '], "' + text + '", MAX_DIALOGUE_CHARS - 1);');
            lines.push('\tdialoguePostCount[currentLevel]++;');
          }
        }
      }
    }

    lines.push('}');
    parts.push(lines.join('\n'));
  }
  // Generate buildLevel() switch with mover scan
  const sw = [];
  sw.push('static void buildLevel() {');
  sw.push('\t// Reset movers and overlay maps for each level build');
  sw.push('\tnumMovers = 0;');
  sw.push('\tmemset(tile3dMap, false, sizeof(tile3dMap));');
  sw.push('\tmemset(warpMap, false, sizeof(warpMap));');
  sw.push('\tmemset(winMap, false, sizeof(winMap));');
  sw.push('\tmemset(checkpointMap, false, sizeof(checkpointMap));');
  sw.push('\twarpTargetLevel = -1;');
  sw.push('\tswitch (currentLevel) {');
  for (let i = 0; i < playable.length; i++) {
    sw.push('\t\tcase ' + i + ': buildLevel_' + (i + 1) + '(); break;');
  }
  sw.push('\t\tdefault: buildLevel_1(); break;');
  sw.push('\t}');
  sw.push('\t// Scan tilemap for TILE_MOVING markers â†’ spawn moving platforms + clear tile');
  sw.push('\tfor (int ty = 0; ty < getActiveMapHeight() && ty < MAP_H; ty++) {');
  sw.push('\t\tfor (int tx = 0; tx < getActiveMapWidth() && tx < MAP_W; tx++) {');
  sw.push('\t\t\tif (tilemap[ty][tx] == TILE_MOVING) {');
  sw.push('\t\t\t\ttilemap[ty][tx] = TILE_EMPTY;');
  sw.push('\t\t\t\tspawnMover(tx * TILE_SIZE, ty * TILE_SIZE, 3.0f * TILE_SIZE, 1.0f);');
  sw.push('\t\t\t}');
  sw.push('\t\t}');
  sw.push('\t}');
  sw.push('\trefreshLevelHasWinZone();');
  sw.push('}');
  parts.push(sw.join('\n'));
  parts.push('// @@EDITOR_LEVELS_END@@');
  return parts.join('\n\n');
}

async function syncToFile() {
  try {
    if (!fileHandle) {
      [fileHandle] = await window.showOpenFilePicker({
        types: [{ description: 'C++ source', accept: { 'text/plain': ['.cpp'] } }],
        multiple: false,
      });
      await saveFileHandle(fileHandle);
    }
    // Re-request permission if needed (e.g. restored handle after refresh)
    const perm = await fileHandle.queryPermission({ mode: 'readwrite' });
    if (perm !== 'granted') {
      const req = await fileHandle.requestPermission({ mode: 'readwrite' });
      if (req !== 'granted') { alert('File permission denied.'); return; }
    }
    // Read current file
    const file = await fileHandle.getFile();
    let content = await file.text();

    // Find markers
    const startMarker = '// @@EDITOR_LEVELS_START@@';
    const endMarker = '// @@EDITOR_LEVELS_END@@';
    const startIdx = content.indexOf(startMarker);
    const endIdx = content.indexOf(endMarker);
    if (startIdx < 0 || endIdx < 0) {
      alert('Could not find @@EDITOR_LEVELS_START@@ / @@EDITOR_LEVELS_END@@ markers in the file.\nMake sure you are selecting the correct main.cpp.');
      fileHandle = null;
      return;
    }

    // Replace level block
    const newBlock = generateSyncBlock();
    content = content.substring(0, startIdx) + newBlock + content.substring(endIdx + endMarker.length);

    // Update NUM_LEVELS (only selectable levels in level select)
    const n = playableLevels().length;
    const total = allGameLevels().length;
    content = content.replace(
      /static constexpr int NUM_LEVELS = \d+;/,
      'static constexpr int NUM_LEVELS = ' + n + ';'
    );
    content = content.replace(
      /static constexpr int TOTAL_LEVELS = \d+;/,
      'static constexpr int TOTAL_LEVELS = ' + total + ';'
    );

    // levelUnlocked[] uses TOTAL_LEVELS in game — do not shrink on sync (save slots hold unlock flags)

    // Write back
    const writable = await fileHandle.createWritable();
    await writable.write(content);
    await writable.close();

    document.getElementById('status').textContent = 'Synced ' + n + ' level(s) to main.cpp!';
    setTimeout(() => {
      document.getElementById('status').textContent = 'Ready';
    }, 3000);
  } catch (err) {
    if (err.name === 'AbortError') return; // user cancelled picker
    alert('Sync failed: ' + err.message);
    console.error(err);
  }
}

function generateCppForLevel(lv, funcName) {
  const lines = [];
  lines.push(`static void ${funcName}() {`);
  lines.push(`\tmemset(tilemap, TILE_EMPTY, sizeof(tilemap));`);
  lines.push(`\tnumCoins = 0;`);
  lines.push(`\tnumEnemies = 0;`);
  lines.push(`\tfor (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;`);
  lines.push(``);

  for (let y = 0; y < lv.mapH; y++) {
    let rowTiles = [];
    let x = 0;
    while (x < lv.mapW) {
      const t = lv.tilemap[y][x];
      if (t === 0) { x++; continue; }
      let end = x;
      while (end + 1 < lv.mapW && lv.tilemap[y][end + 1] === t) end++;
      rowTiles.push({ t, x0: x, x1: end });
      x = end + 1;
    }
    if (rowTiles.length === 0) continue;

    lines.push(`\t// Row ${y}`);
    for (const run of rowTiles) {
      if (run.x0 === run.x1) {
        lines.push(`\ttilemap[${y}][${run.x0}] = ${TILE_NAMES[run.t]};`);
      } else {
        lines.push(`\tfor (int x = ${run.x0}; x <= ${run.x1}; x++)`);
        lines.push(`\t\ttilemap[${y}][x] = ${TILE_NAMES[run.t]};`);
      }
    }
  }

  if (lv.coins.length) {
    lines.push(``);
    lines.push(`\t// Crackers`);
    for (let i = 0; i < lv.coins.length; i++) {
      const c = lv.coins[i];
      const bob = (i * 0.3).toFixed(1) + 'f';
      lines.push(`\tcoins[numCoins++] = { { ${c.x.toFixed(1)}f, ${c.y.toFixed(1)}f, 14, 14 }, true, ${bob} };`);
    }
  }

  if (lv.enemies && lv.enemies.length) {
    lines.push(``);
    lines.push(`\t// Enemies`);
    for (const en of lv.enemies) {
      lines.push(`\tspawnEnemy(${en.x.toFixed(1)}f, ${en.y.toFixed(1)}f);`);
    }
  }

  // 3D tile overlay
  if (lv.tile3d) {
    let has3d = false;
    for (let y = 0; y < lv.mapH && !has3d; y++)
      for (let x = 0; x < lv.mapW && !has3d; x++)
        if (lv.tile3d[y] && lv.tile3d[y][x]) has3d = true;
    if (has3d) {
      lines.push(``);
      lines.push(`\t// 3D tiles`);
      for (let y = 0; y < lv.mapH; y++)
        for (let x = 0; x < lv.mapW; x++)
          if (lv.tile3d[y] && lv.tile3d[y][x])
            lines.push(`\ttile3dMap[${y}][${x}] = true;`);
    }
  }

  // Warp overlay
  if (lv.warpOverlay) {
    let hasWarp = false;
    for (let y = 0; y < lv.mapH && !hasWarp; y++)
      for (let x = 0; x < lv.mapW && !hasWarp; x++)
        if (lv.warpOverlay[y] && lv.warpOverlay[y][x]) hasWarp = true;
    if (hasWarp) {
      lines.push(``);
      lines.push(`\t// Warp tiles`);
      for (let y = 0; y < lv.mapH; y++)
        for (let x = 0; x < lv.mapW; x++)
          if (lv.warpOverlay[y] && lv.warpOverlay[y][x])
            lines.push(`\twarpMap[${y}][${x}] = true;`);
      if (lv.warpTarget >= 0)
        lines.push(`\twarpTargetLevel = ${lv.warpTarget};`);
    }
  }

  if (lv.winOverlay) {
    let hasWin = false;
    for (let y = 0; y < lv.mapH && !hasWin; y++)
      for (let x = 0; x < lv.mapW && !hasWin; x++)
        if (lv.winOverlay[y] && lv.winOverlay[y][x]) hasWin = true;
    if (hasWin) {
      lines.push(``);
      lines.push(`\t// Win / goal tiles`);
      for (let y = 0; y < lv.mapH; y++)
        for (let x = 0; x < lv.mapW; x++)
          if (lv.winOverlay[y] && lv.winOverlay[y][x])
            lines.push(`\twinMap[${y}][${x}] = true;`);
    }
  }

  if (lv.checkpointOverlay) {
    let hasCp = false;
    for (let y = 0; y < lv.mapH && !hasCp; y++)
      for (let x = 0; x < lv.mapW && !hasCp; x++)
        if (lv.checkpointOverlay[y] && lv.checkpointOverlay[y][x]) hasCp = true;
    if (hasCp) {
      lines.push(``);
      lines.push(`\t// Checkpoints`);
      for (let y = 0; y < lv.mapH; y++)
        for (let x = 0; x < lv.mapW; x++)
          if (lv.checkpointOverlay[y] && lv.checkpointOverlay[y][x])
            lines.push(`\tcheckpointMap[${y}][${x}] = true;`);
    }
  }

  lines.push(``);
  lines.push(`\t// Player spawn`);
  lines.push(`\tspawnX = ${lv.spawnX.toFixed(1)}f;`);
  lines.push(`\tspawnY = ${lv.spawnY.toFixed(1)}f;`);

  // Dialogue
  if (lv.dialoguePreEnabled && lv.dialoguePre) {
    let hasPre = false;
    for (let i = 0; i < 4; i++) if (lv.dialoguePre[i] && lv.dialoguePre[i].trim()) hasPre = true;
    if (hasPre) {
      lines.push(``);
      lines.push(`\t// Pre-level dialogue`);
      lines.push(`\tdialoguePreCount[currentLevel] = 0;`);
      for (let i = 0; i < 4; i++) {
        if (lv.dialoguePre[i] && lv.dialoguePre[i].trim()) {
          const text = lv.dialoguePre[i].replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
          lines.push(`\tstrncpy(dialoguePre[currentLevel][${i}], "${text}", MAX_DIALOGUE_CHARS - 1);`);
          lines.push(`\tdialoguePreCount[currentLevel]++;`);
        }
      }
    }
  }
  if (lv.dialoguePostEnabled && lv.dialoguePost) {
    let hasPost = false;
    for (let i = 0; i < 4; i++) if (lv.dialoguePost[i] && lv.dialoguePost[i].trim()) hasPost = true;
    if (hasPost) {
      lines.push(``);
      lines.push(`\t// Post-level dialogue`);
      lines.push(`\tdialoguePostCount[currentLevel] = 0;`);
      for (let i = 0; i < 4; i++) {
        if (lv.dialoguePost[i] && lv.dialoguePost[i].trim()) {
          const text = lv.dialoguePost[i].replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n');
          lines.push(`\tstrncpy(dialoguePost[currentLevel][${i}], "${text}", MAX_DIALOGUE_CHARS - 1);`);
          lines.push(`\tdialoguePostCount[currentLevel]++;`);
        }
      }
    }
  }

  lines.push(`}`);

  let header = `// ${lv.name}\n`;
  header += `// MAP_W = ${lv.mapW}, MAP_H = ${lv.mapH}\n\n`;

  return header + lines.join('\n');
}

function showExport() {
  syncToLevel();
  const cpp = generateCppForLevel(cur(), 'buildLevel');
  document.getElementById('modal-title').textContent = 'Export â€” ' + cur().name;
  document.getElementById('export-text').value = cpp;
  document.getElementById('modal-overlay').classList.remove('hidden');
}

function showExportAll() {
  syncToLevel();
  let allCpp = '';
  for (let i = 0; i < levels.length; i++) {
    const funcName = i === 0 ? 'buildLevel' : 'buildLevel_' + (i + 1);
    allCpp += generateCppForLevel(levels[i], funcName);
    if (i < levels.length - 1) allCpp += '\n\n// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\n\n';
  }
  document.getElementById('modal-title').textContent = 'Export All Levels';
  document.getElementById('export-text').value = allCpp;
  document.getElementById('modal-overlay').classList.remove('hidden');
}

function closeModal() {
  document.getElementById('modal-overlay').classList.add('hidden');
}

function copyExport() {
  const ta = document.getElementById('export-text');
  ta.select();
  navigator.clipboard.writeText(ta.value);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Save / Load JSON (all levels)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
function saveJSON() {
  syncToLevel();
  const data = { levels: levels.map(lv => ({
    levelIndex: lv.levelIndex,
    name: lv.name, mapW: lv.mapW, mapH: lv.mapH,
    world: typeof lv.world === 'number' ? lv.world : 1,
    isMenu: !!lv.isMenu,
    isHidden: !!lv.isHidden,
    tilemap: lv.tilemap, coins: lv.coins,
    enemies: lv.enemies || [],
    tile3d: lv.tile3d || null,
    warpOverlay: lv.warpOverlay || null,
    winOverlay: lv.winOverlay || null,
    checkpointOverlay: lv.checkpointOverlay || null,
    warpTarget: lv.warpTarget !== undefined ? lv.warpTarget : -1,
    spawnX: lv.spawnX, spawnY: lv.spawnY,
    dialoguePreEnabled: lv.dialoguePreEnabled,
    dialoguePostEnabled: lv.dialoguePostEnabled,
    dialoguePre: lv.dialoguePre,
    dialoguePost: lv.dialoguePost,
    features: lv.isMenu ? null : { ...(lv.features || defaultLevelFeatures(levelWorldNum(lv))) },
    physics: lv.isMenu ? null : (lv.physics ? { ...lv.physics } : null),
    parTime: lv.isMenu ? 0 : (lv.parTime != null ? lv.parTime : defaultParTime(levelWorldNum(lv))),
  }))};
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'levels.json';
  a.click();
}

function loadJSON(event) {
  const file = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = (e) => {
    try {
      const data = JSON.parse(e.target.result);
      // Support both old single-level and new multi-level format
      if (data.levels) {
        levels = data.levels.map((lv, idx) => ({
          levelIndex: lv.levelIndex !== undefined ? lv.levelIndex : idx,
          name: lv.name || 'Untitled',
          mapW: lv.mapW, mapH: lv.mapH,
          world: typeof lv.world === 'number' ? lv.world : 1,
          isMenu: !!lv.isMenu,
          isHidden: !!lv.isHidden,
          tilemap: lv.tilemap, coins: lv.coins || [],
          enemies: lv.enemies || [],
          tile3d: lv.tile3d || null,
          warpOverlay: lv.warpOverlay || null,
          winOverlay: lv.winOverlay || null,
    checkpointOverlay: lv.checkpointOverlay || null,
          warpTarget: lv.warpTarget !== undefined ? lv.warpTarget : -1,
          spawnX: lv.spawnX || 48, spawnY: lv.spawnY || 164,
          undoStack: [], redoStack: [],
          dialoguePreEnabled: lv.dialoguePreEnabled || false,
          dialoguePostEnabled: lv.dialoguePostEnabled || false,
          dialoguePre: lv.dialoguePre || ['', '', '', ''],
          dialoguePost: lv.dialoguePost || ['', '', '', ''],
          features: lv.isMenu ? null : (lv.features ? { ...lv.features } : null),
          physics: lv.isMenu ? null : (lv.physics ? { ...lv.physics } : null),
          parTime: lv.isMenu ? 0 : (lv.parTime != null ? lv.parTime : defaultParTime(levelWorldNum(lv))),
        }));
        levels.forEach((lv) => {
          normalizeLevelFeatures(lv);
          normalizeLevelPhysics(lv);
        });
      } else {
        // Old single-level format
        levels = [{
          levelIndex: 0,
          name: '1-1',
          mapW: data.mapW, mapH: data.mapH,
          world: 1,
          isMenu: false,
          isHidden: false,
          features: defaultLevelFeatures(1),
          tilemap: data.tilemap, coins: data.coins || [],
          enemies: data.enemies || [],
          tile3d: data.tile3d || null,
          warpOverlay: data.warpOverlay || null,
          winOverlay: data.winOverlay || null,
          checkpointOverlay: data.checkpointOverlay || null,
          warpTarget: data.warpTarget !== undefined ? data.warpTarget : -1,
          spawnX: data.spawnX || 48, spawnY: data.spawnY || 164,
          undoStack: [], redoStack: [],
          dialoguePreEnabled: false,
          dialoguePostEnabled: false,
          dialoguePre: ['', '', '', ''],
          dialoguePost: ['', '', '', ''],
        }];
      }
      currentLevelIdx = Math.min(data.currentLevelIdx || 0, levels.length - 1);
      syncFromLevel();
      buildLevelTabs();
      autoSave();
      render();
    } catch (err) { alert('Invalid JSON: ' + err.message); }
  };
  reader.readAsText(file);
  event.target.value = '';
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Import levels FROM main.cpp
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
const TILE_NAME_TO_ID = {};
TILE_NAMES.forEach((name, idx) => { TILE_NAME_TO_ID[name] = idx; });

/** Extract function body between matching braces (avoids truncating on inner `}`). */
function extractCppFunctionBody(source, funcName) {
  const sig = 'static void ' + funcName + '(';
  const start = source.indexOf(sig);
  if (start < 0) return null;
  const braceStart = source.indexOf('{', start);
  if (braceStart < 0) return null;
  let depth = 0;
  for (let i = braceStart; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}') {
      depth--;
      if (depth === 0) return source.slice(braceStart + 1, i);
    }
  }
  return null;
}

function menuHasTiles(lv) {
  if (!lv?.tilemap) return false;
  for (let y = 0; y < lv.mapH; y++) {
    const row = lv.tilemap[y];
    if (!row) continue;
    for (let x = 0; x < lv.mapW; x++) {
      if (row[x]) return true;
    }
  }
  return false;
}

function levelHasDialogue(lv) {
  if (lv.dialoguePreEnabled || lv.dialoguePostEnabled) return true;
  const pre = lv.dialoguePre || [];
  const post = lv.dialoguePost || [];
  return pre.some(t => t && String(t).trim()) || post.some(t => t && String(t).trim());
}

function levelHasWinTiles(lv) {
  if (!lv?.winOverlay) return false;
  for (let y = 0; y < lv.mapH; y++) {
    const row = lv.winOverlay[y];
    if (!row) continue;
    for (let x = 0; x < lv.mapW; x++) {
      if (row[x]) return true;
    }
  }
  return false;
}

function levelHasCheckpointTiles(lv) {
  if (!lv?.checkpointOverlay) return false;
  for (let y = 0; y < lv.mapH; y++) {
    const row = lv.checkpointOverlay[y];
    if (!row) continue;
    for (let x = 0; x < lv.mapW; x++) {
      if (row[x]) return true;
    }
  }
  return false;
}

/** Pull menu tiles / dialogue from main.cpp into the open project when JSON is missing them. */
export function mergeSupplementalFromCpp(content) {
  const parsed = parseLevelsFromCpp(content);
  if (!parsed) return { menu: false, dialogue: 0 };

  let menuMerged = false;
  const parsedMenu = parsed.find(l => l.isMenu);
  if (parsedMenu && menuHasTiles(parsedMenu)) {
    let curMenu = levels.find(l => l.isMenu);
    if (!curMenu) {
      levels.unshift(parsedMenu);
      menuMerged = true;
    } else if (!menuHasTiles(curMenu)) {
      curMenu.tilemap = parsedMenu.tilemap;
      curMenu.tile3d = parsedMenu.tile3d;
      curMenu.warpOverlay = parsedMenu.warpOverlay;
      curMenu.spawnX = parsedMenu.spawnX;
      curMenu.spawnY = parsedMenu.spawnY;
      menuMerged = true;
    }
  }

  let dialogueMerged = 0;
  let winMerged = 0;
  let checkpointMerged = 0;
  for (const src of parsed) {
    if (src.isMenu || !levelHasDialogue(src)) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst || levelHasDialogue(dst)) continue;
    dst.dialoguePreEnabled = src.dialoguePreEnabled;
    dst.dialoguePostEnabled = src.dialoguePostEnabled;
    dst.dialoguePre = [...(src.dialoguePre || ['', '', '', ''])];
    dst.dialoguePost = [...(src.dialoguePost || ['', '', '', ''])];
    dialogueMerged++;
  }

  for (const src of parsed) {
    if (src.isMenu || !levelHasWinTiles(src)) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst || levelHasWinTiles(dst)) continue;
    dst.winOverlay = src.winOverlay.map(row => [...row]);
    winMerged++;
  }

  for (const src of parsed) {
    if (src.isMenu || !src.features) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst) continue;
    const def = defaultLevelFeatures(levelWorldNum(src));
    const f = src.features;
    const explicit = f.doubleJump !== def.doubleJump || f.dialogue !== def.dialogue
      || f.wallJump !== def.wallJump || f.dash !== def.dash || f.groundPound !== def.groundPound
      || f.minimap !== def.minimap;
    if (explicit) dst.features = { ...f };
  }

  let physicsMerged = 0;
  for (const src of parsed) {
    if (src.isMenu || !src.physics?.enabled) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst || dst.physics?.enabled) continue;
    dst.physics = { ...src.physics };
    physicsMerged++;
  }

  for (const src of parsed) {
    if (src.isMenu || !levelHasCheckpointTiles(src)) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst || levelHasCheckpointTiles(dst)) continue;
    dst.checkpointOverlay = src.checkpointOverlay.map(row => [...row]);
    checkpointMerged++;
  }

  for (const src of parsed) {
    if (src.isMenu || src.warpTarget < 0) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst || (dst.warpTarget !== undefined && dst.warpTarget >= 0)) continue;
    dst.warpTarget = src.warpTarget;
  }

  for (const src of parsed) {
    if (src.isMenu) continue;
    const dst = levels.find(l => !l.isMenu && l.levelIndex === src.levelIndex);
    if (!dst) continue;
    if (src.parTime != null && (dst.parTime == null || dst.parTime === defaultParTime(levelWorldNum(dst)))) {
      dst.parTime = src.parTime;
    }
  }

  if (menuMerged) normalizePlayableOrder();
  return { menu: menuMerged, dialogue: dialogueMerged, win: winMerged, physics: physicsMerged, checkpoint: checkpointMerged };
}

function parseLevelsFromCpp(content) {
  const startMarker = '// @@EDITOR_LEVELS_START@@';
  const endMarker = '// @@EDITOR_LEVELS_END@@';
  const startIdx = content.indexOf(startMarker);
  const endIdx = content.indexOf(endMarker);
  if (startIdx < 0 || endIdx < 0) return null;
  const block = content.substring(startIdx + startMarker.length, endIdx);

  // Read MAP_W and MAP_H from the file
  const mwMatch = content.match(/static constexpr int MAP_W\s*=\s*(\d+)/);
  const mhMatch = content.match(/static constexpr int MAP_H\s*=\s*(\d+)/);
  const fileMapW = mwMatch ? parseInt(mwMatch[1]) : 80;
  const fileMapH = mhMatch ? parseInt(mhMatch[1]) : 16;

  // Parse LEVEL_INFO table if it exists (now includes width and height)
  const levelInfo = [];
  const levelInfoRe = /\/\*\s*(\d+)\s*\*\/\s*\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(true|false)\s*,\s*([\d.]+)f?\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)(?:\s*,\s*(true|false))?)?\s*\}/g;
  let infoMatch;
  while ((infoMatch = levelInfoRe.exec(block)) !== null) {
    const world = parseInt(infoMatch[3]);
    const hasFeat = infoMatch[8] !== undefined;
    levelInfo[parseInt(infoMatch[1])] = {
      name: infoMatch[2],
      world: world,
      isHidden: infoMatch[4] === 'true',
      parTime: parseFloat(infoMatch[5]),
      width: parseInt(infoMatch[6]),
      height: parseInt(infoMatch[7]),
      features: hasFeat ? {
        doubleJump: infoMatch[8] === 'true',
        dialogue: infoMatch[9] === 'true',
        wallJump: infoMatch[10] === 'true',
        dash: infoMatch[11] === 'true',
        groundPound: infoMatch[12] === 'true',
        minimap: infoMatch[13] !== undefined ? infoMatch[13] === 'true' : true,
      } : defaultLevelFeatures(world),
    };
  }

  // Parse metadata comments: // Level N: "name", World X[, HIDDEN]
  const metadataRe = /\/\/\s*Level\s+(\d+):\s*"([^"]+)"\s*,\s*(World\s+\d+|Secret)(?:\s*,\s*(HIDDEN))?/g;
  const metadata = [];
  while ((infoMatch = metadataRe.exec(block)) !== null) {
    const idx = parseInt(infoMatch[1]);
    const isSecret = infoMatch[3] === 'Secret';
    const world = isSecret ? 0 : parseInt(infoMatch[3].replace('World ', ''));
    metadata[idx] = {
      name: infoMatch[2],
      world: world,
      isHidden: !!infoMatch[4] || isSecret
    };
  }

  const physicsByIndex = parsePhysicsByIndex(block);

  const parsed = [];
  const buildLevelRe = /static void buildLevel_(\d+)\(\)/g;
  let match;
  while ((match = buildLevelRe.exec(block)) !== null) {
    const funcNum = parseInt(match[1]);
    const body = extractCppFunctionBody(block, 'buildLevel_' + funcNum);
    if (!body) continue;

    // Get metadata from LEVEL_INFO table, metadata comment, or defaults
    const info = levelInfo[funcNum - 1] || metadata[funcNum - 1] || {};
    const isSecret = info.isHidden || info.world === 0;
    let world = info.world !== undefined ? info.world : 1;
    if (isSecret && world === 1) world = 0;
    const displayName = info.name || (isSecret ? '???' : (world + '-' + funcNum));

    // Use per-level dimensions from LEVEL_INFO, or fall back to file defaults
    const levelWidth = info.width || fileMapW;
    const levelHeight = info.height || fileMapH;

    const lv = {
      levelIndex: funcNum - 1,
      name: displayName,
      mapW: levelWidth, mapH: levelHeight,
      world: world,
      features: info.features ? { ...info.features } : defaultLevelFeatures(world),
      physics: physicsByIndex[funcNum - 1]
        ? { ...physicsByIndex[funcNum - 1] }
        : defaultLevelPhysics(),
      isMenu: false,
      isHidden: info.isHidden || false,
      parTime: info.parTime != null ? info.parTime : defaultParTime(world),
      tilemap: [],
      coins: [],
      enemies: [],
      spawnX: 1.5 * TILE_SIZE,
      spawnY: (levelHeight - 2) * TILE_SIZE - 28,
      undoStack: [], redoStack: [],
      tile3d: [],
      warpOverlay: [],
      winOverlay: [],
      checkpointOverlay: [],
      warpTarget: -1,
      dialoguePreEnabled: false,
      dialoguePostEnabled: false,
      dialoguePre: ['', '', '', ''],
      dialoguePost: ['', '', '', ''],
    };
    for (let y = 0; y < levelHeight; y++) lv.tilemap.push(new Array(levelWidth).fill(0));

    // Parse single tile: tilemap[y][x] = TILE_NAME;
    const singleRe = /tilemap\[(\d+)\]\[(\d+)\]\s*=\s*(\w+);/g;
    let m;
    while ((m = singleRe.exec(body)) !== null) {
      const y = parseInt(m[1]), x = parseInt(m[2]);
      const tid = TILE_NAME_TO_ID[m[3]];
      if (tid !== undefined && y < levelHeight && x < levelWidth) lv.tilemap[y][x] = tid;
    }

    // Parse run: for (int x = x0; x <= x1; x++) tilemap[y][x] = TILE_NAME;
    // Might be across two lines or on one line
    const runRe = /for\s*\(int x\s*=\s*(\d+);\s*x\s*<=\s*(\d+);\s*x\+\+\)\s*(?:\n\s*)?tilemap\[(\d+)\]\[x\]\s*=\s*(\w+);/g;
    while ((m = runRe.exec(body)) !== null) {
      const x0 = parseInt(m[1]), x1 = parseInt(m[2]), y = parseInt(m[3]);
      const tid = TILE_NAME_TO_ID[m[4]];
      if (tid !== undefined) {
        for (let x = x0; x <= x1 && x < levelWidth; x++) {
          if (y < levelHeight) lv.tilemap[y][x] = tid;
        }
      }
    }

    // Parse coins: coins[numCoins++] = { { x, y, 14, 14 }, true, bob };
    const coinRe = /coins\[numCoins\+\+\]\s*=\s*\{\s*\{\s*([\d.]+)f?,\s*([\d.]+)f?,\s*14,\s*14\s*\},\s*true,\s*[\d.]+f?\s*\}/g;
    while ((m = coinRe.exec(body)) !== null) {
      lv.coins.push({ x: parseFloat(m[1]), y: parseFloat(m[2]) });
    }

    // Parse enemies: spawnEnemy(x, y);
    const enemyRe = /spawnEnemy\(\s*([\d.]+)f?,\s*([\d.]+)f?\s*\)/g;
    while ((m = enemyRe.exec(body)) !== null) {
      lv.enemies.push({ x: parseFloat(m[1]), y: parseFloat(m[2]) });
    }

    // Parse 3D tiles: tile3dMap[y][x] = true;
    lv.tile3d = [];
    for (let y = 0; y < levelHeight; y++) lv.tile3d.push(new Array(levelWidth).fill(false));
    const t3dRe = /tile3dMap\[(\d+)\]\[(\d+)\]\s*=\s*true;/g;
    while ((m = t3dRe.exec(body)) !== null) {
      const y = parseInt(m[1]), x = parseInt(m[2]);
      if (y < levelHeight && x < levelWidth) lv.tile3d[y][x] = true;
    }

    // Parse warp tiles: warpMap[y][x] = true;
    lv.warpOverlay = [];
    for (let y = 0; y < levelHeight; y++) lv.warpOverlay.push(new Array(levelWidth).fill(false));
    const warpRe = /warpMap\[(\d+)\]\[(\d+)\]\s*=\s*true;/g;
    while ((m = warpRe.exec(body)) !== null) {
      const y = parseInt(m[1]), x = parseInt(m[2]);
      if (y < levelHeight && x < levelWidth) lv.warpOverlay[y][x] = true;
    }
    const wtRe = /warpTargetLevel\s*=\s*(\d+);/;
    const wtMatch = body.match(wtRe);
    lv.warpTarget = wtMatch ? parseInt(wtMatch[1]) : -1;

    // Parse win/goal tiles: winMap[y][x] = true;
    lv.winOverlay = [];
    for (let y = 0; y < levelHeight; y++) lv.winOverlay.push(new Array(levelWidth).fill(false));
    const winRe = /winMap\[(\d+)\]\[(\d+)\]\s*=\s*true;/g;
    while ((m = winRe.exec(body)) !== null) {
      const y = parseInt(m[1]), x = parseInt(m[2]);
      if (y < levelHeight && x < levelWidth) lv.winOverlay[y][x] = true;
    }

    lv.checkpointOverlay = [];
    for (let y = 0; y < levelHeight; y++) lv.checkpointOverlay.push(new Array(levelWidth).fill(false));
    const cpRe = /checkpointMap\[(\d+)\]\[(\d+)\]\s*=\s*true;/g;
    while ((m = cpRe.exec(body)) !== null) {
      const y = parseInt(m[1]), x = parseInt(m[2]);
      if (y < levelHeight && x < levelWidth) lv.checkpointOverlay[y][x] = true;
    }

    // Parse spawn: spawnX = x; spawnY = y;
    const spawnXRe = /spawnX\s*=\s*([\d.]+)f?;/;
    const spawnYRe = /spawnY\s*=\s*([\d.]+)f?;/;
    const sx = body.match(spawnXRe);
    const sy = body.match(spawnYRe);
    if (sx) lv.spawnX = parseFloat(sx[1]);
    if (sy) lv.spawnY = parseFloat(sy[1]);

    const preRe = /strncpy\(dialoguePre\[currentLevel\]\[(\d+)\],\s*"((?:\\.|[^"\\])*)"/g;
    let preCount = 0;
    while ((m = preRe.exec(body)) !== null) {
      const idx = parseInt(m[1]);
      const text = m[2].replace(/\\n/g, '\n').replace(/\\"/g, '"').replace(/\\\\/g, '\\');
      lv.dialoguePre[idx] = text;
      preCount = Math.max(preCount, idx + 1);
    }
    if (preCount > 0) {
      lv.dialoguePreEnabled = true;
    }

    const postRe = /strncpy\(dialoguePost\[currentLevel\]\[(\d+)\],\s*"((?:\\.|[^"\\])*)"/g;
    let postCount = 0;
    while ((m = postRe.exec(body)) !== null) {
      const idx = parseInt(m[1]);
      const text = m[2].replace(/\\n/g, '\n').replace(/\\"/g, '"').replace(/\\\\/g, '\\');
      lv.dialoguePost[idx] = text;
      postCount = Math.max(postCount, idx + 1);
    }
    if (postCount > 0) {
      lv.dialoguePostEnabled = true;
    }

    parsed.push(lv);
  }

  const menu = parseMenuSceneFromBlock(block);
  if (menu) {
    parsed.unshift(menu);
  }

  if (parsed.length) {
    inferWorldsFromLayout(parsed);
    return parsed;
  }
  return null;
}

function parseMenuSceneFromBlock(block) {
  const body = extractCppFunctionBody(block, 'buildMenuScene');
  if (!body) return null;
  const menuW = 80;
  const menuH = 16;
  const lv = createLevel('Menu BG', menuW, menuH, 1, true);

  const singleRe = /menuTilemap\[(\d+)\]\[(\d+)\]\s*=\s*(\w+);/g;
  let m;
  while ((m = singleRe.exec(body)) !== null) {
    const y = parseInt(m[1]);
    const x = parseInt(m[2]);
    const tid = TILE_NAME_TO_ID[m[3]];
    if (tid !== undefined && y < menuH && x < menuW) {
      lv.tilemap[y][x] = tid;
    }
  }

  const runRe = /for\s*\(int x\s*=\s*(\d+);\s*x\s*<=\s*(\d+);\s*x\+\+\)\s*(?:\n\s*)?menuTilemap\[(\d+)\]\[x\]\s*=\s*(\w+);/g;
  while ((m = runRe.exec(body)) !== null) {
    const x0 = parseInt(m[1]);
    const x1 = parseInt(m[2]);
    const y = parseInt(m[3]);
    const tid = TILE_NAME_TO_ID[m[4]];
    if (tid !== undefined) {
      for (let x = x0; x <= x1 && x < menuW; x++) {
        if (y < menuH) lv.tilemap[y][x] = tid;
      }
    }
  }

  const rx = body.match(/menuRunnerStartX\s*=\s*([\d.]+)f?;/);
  const ry = body.match(/menuRunnerStartY\s*=\s*([\d.]+)f?;/);
  if (rx) lv.spawnX = parseFloat(rx[1]);
  if (ry) lv.spawnY = parseFloat(ry[1]);

  return lv;
}

let autoSaveCallback = null;
export function setAutoSaveCallback(fn) { autoSaveCallback = fn; }

function autoSave() {
  if (autoSaveCallback) autoSaveCallback();
}

export function getProjectData() {
  syncToLevel();
  return {
    levels: levels.map(lv => ({
      levelIndex: lv.levelIndex,
      name: lv.name, mapW: lv.mapW, mapH: lv.mapH,
      world: levelWorldNum(lv) ?? lv.world ?? 1,
      isMenu: !!lv.isMenu,
      isHidden: !!lv.isHidden,
      tilemap: lv.tilemap, coins: lv.coins,
      enemies: lv.enemies || [],
      tile3d: lv.tile3d || null,
      warpOverlay: lv.warpOverlay || null,
      winOverlay: lv.winOverlay || null,
      checkpointOverlay: lv.checkpointOverlay || null,
      warpTarget: lv.warpTarget !== undefined ? lv.warpTarget : -1,
      parTime: lv.isMenu ? 0 : (lv.parTime != null ? lv.parTime : defaultParTime(levelWorldNum(lv))),
      spawnX: lv.spawnX, spawnY: lv.spawnY,
      dialoguePreEnabled: !!lv.dialoguePreEnabled,
      dialoguePostEnabled: !!lv.dialoguePostEnabled,
      dialoguePre: lv.dialoguePre || ['', '', '', ''],
      dialoguePost: lv.dialoguePost || ['', '', '', ''],
      features: lv.isMenu ? null : { ...(lv.features || defaultLevelFeatures(levelWorldNum(lv))) },
      physics: lv.isMenu ? null : (lv.physics ? { ...lv.physics } : null),
    })),
    currentLevelIdx,
    nextLevelIndex,
  };
}

export function setProjectData(data) {
  if (!data || !data.levels || !data.levels.length) return false;
  nextLevelIndex = data.nextLevelIndex || data.levels.length;
  levels = data.levels.map((lv, idx) => ({
    levelIndex: lv.levelIndex !== undefined ? lv.levelIndex : idx,
    name: lv.name || 'Untitled',
    mapW: lv.mapW, mapH: lv.mapH,
    world: typeof lv.world === 'number' ? lv.world : 1,
    isMenu: !!lv.isMenu,
    isHidden: !!lv.isHidden,
    tilemap: lv.tilemap, coins: lv.coins || [],
    enemies: lv.enemies || [],
    tile3d: lv.tile3d || null,
    warpOverlay: lv.warpOverlay || null,
    winOverlay: lv.winOverlay || null,
    checkpointOverlay: lv.checkpointOverlay || null,
    warpTarget: lv.warpTarget !== undefined ? lv.warpTarget : -1,
    spawnX: lv.spawnX || 48, spawnY: lv.spawnY || 164,
    undoStack: [], redoStack: [],
    dialoguePreEnabled: lv.dialoguePreEnabled || false,
    dialoguePostEnabled: lv.dialoguePostEnabled || false,
    dialoguePre: lv.dialoguePre || ['', '', '', ''],
    dialoguePost: lv.dialoguePost || ['', '', '', ''],
    features: lv.isMenu ? null : (lv.features ? { ...lv.features } : null),
    physics: lv.isMenu ? null : (lv.physics ? { ...lv.physics } : null),
    parTime: lv.isMenu ? 0 : (lv.parTime != null ? lv.parTime : defaultParTime(levelWorldNum(lv))),
  }));
  levels.forEach((lv) => {
    normalizeLevelFeatures(lv);
    normalizeLevelPhysics(lv);
    ensureLevelArrays(lv);
  });
  inferWorldsFromLayout(levels);
  normalizePlayableOrder();
  currentLevelIdx = Math.min(data.currentLevelIdx || 0, levels.length - 1);
  syncFromLevel();
  buildLevelTabs();
  return true;
}

export function loadTilesetFromFile(file) {
  if (!file) return;
  const img = new Image();
  img.onload = () => { tilesetImg = img; render(); };
  img.src = URL.createObjectURL(file);
}

export function saveLevelsJSON() {
  return JSON.stringify(getProjectData(), null, 2);
}

export function loadLevelsJSON(text) {
  try {
    const data = JSON.parse(text);
    if (data.levels) return setProjectData(data);
    return false;
  } catch { return false; }
}

export function initLevelEditorDom() {
  if (editorDomReady) return;
  editorDomReady = true;
  trackpadMode = loadTrackpadPreference();
  syncFromLevel();
  buildLevelTabs();
  buildPalette();
  bindToolButtons();
  bindTrackpadControls();
  bindViewportControls();
  bindCanvasEvents();
  bindEditorKeyboard();
  if (!window.__editorResizeBound) {
    window.__editorResizeBound = true;
    window.addEventListener('resize', resizeCanvas);
    window.addEventListener('workspace-layout-change', resizeCanvas);
  }
  resizeCanvas();
  updateUI();
}

export function initEditorCanvas() {
  if (!ensureEditorCanvas()) return;
  resizeCanvas();
  panX = 20;
  panY = -(mapH * TILE_SIZE * zoom - canvas.height) + 40;
  render();
}

export {
  TILE_DEFS, TILE_SIZE,
  levels, currentLevelIdx,
  cur, syncFromLevel, syncToLevel, switchLevel,
  addLevel, addLevelToWorld, addMenuLevel, deleteLevel, renameCurrent, toggleHidden,
  changeCurrentWorld, moveCurrentLevel, moveCurrentLevelToWorld,
  applyLevelFeaturesToUI, syncLevelFeaturesFromUI,
  applyLevelPhysicsToUI, syncLevelPhysicsFromUI, onCustomPhysicsToggle,
  loadDialogue, fitViewToLevel,
  toggleDialoguePre, toggleDialoguePost, saveDialogue,
  buildLevelTabs, resizeCanvas, buildPalette, updateUI, render,
  resizeMap, clearMap,
  generateCppForLevel, showExport, showExportAll,
  closeModal, copyExport, parseLevelsFromCpp,
};