/** In-app Help — keep aligned with docs/USER_GUIDE.md, PLAYING.md, TROUBLESHOOTING.md. */

export const HELP_SECTIONS = [
  {
    id: "editing",
    title: "Editing",
    html: `
      <p>Open a project from the welcome screen, then paint tiles, place crackers/enemies, and tune physics in the right panel.</p>
      <ul>
        <li><strong>Save</strong> writes levels to <code>studio_project.json</code> and syncs into <code>source/main.cpp</code>.</li>
        <li><strong>Save As…</strong> copies the whole project under <code>Documents/3DSStudio/</code> (no spaces in the name).</li>
        <li><strong>Projects</strong> list: open a project, or use <strong>Rename</strong> / <strong>Delete</strong> on that row (library folders only).</li>
        <li><strong>Projects</strong> (top bar) returns to the project list without quitting Studio.</li>
        <li><strong>Folder</strong> opens the project root in Explorer (where <code>.3dsx</code> / <code>.cia</code> appear after Build).</li>
      </ul>
    `,
  },
  {
    id: "assets",
    title: "Assets",
    html: `
      <p>Click an asset slot to import art. Slots show the required format (e.g. <code>PNG 256×240</code>).</p>
      <ul>
        <li>Fresh projects ship <strong>colored placeholder</strong> art with labels — replace them with your own PNGs when ready.</li>
        <li>The default <strong>tileset</strong> is kept so levels look playable immediately.</li>
        <li><strong>Soundtrack</strong> is optional MP3. Missing music is fine; SFX still work on hardware if <code>dspfirm.cdc</code> is present.</li>
      </ul>
    `,
  },
  {
    id: "build",
    title: "Build",
    html: `
      <p>Studio can always edit. Building needs <strong>devkitPro</strong> on Windows (not bundled).</p>
      <ul>
        <li><strong>Build 3dsx</strong> → playable Homebrew Launcher app in the project root (next to the Makefile, not inside <code>build/</code>).</li>
        <li><strong>Build CIA</strong> → installable package (needs <code>makerom</code> + <code>bannertool</code>).</li>
        <li>If tools are missing, open <strong>Setup tools…</strong> to detect, browse for <code>C:\\devkitPro</code>, or open install docs.</li>
      </ul>
      <p>Official install guide: <button type="button" class="help-link" data-url="https://devkitpro.org/wiki/Getting_Started">devkitPro Getting Started</button></p>
    `,
  },
  {
    id: "playing",
    title: "Playing on 3DS",
    html: `
      <p>Studio only builds packages. You copy them to a CFW 3DS. Installing CFW has risks (bricking, warranty) — your choice. Studio does not launch emulators.</p>
      <h4>SD card</h4>
      <ol>
        <li>Build <code>.3dsx</code> and/or <code>.cia</code>.</li>
        <li>Copy <code>.3dsx</code> to <code>sdmc:/3ds/</code> → run from Homebrew Launcher.</li>
        <li>Copy <code>.cia</code> anywhere → install with <strong>FBI</strong>.</li>
      </ol>
      <h4>FTP</h4>
      <p>Run an FTP server on the 3DS, connect from the PC, upload the same files. Studio does not speak FTP itself.</p>
      <h4>3dslink</h4>
      <p>Build first. On the 3DS: Homebrew Launcher → <strong>Y</strong> for IP. In Studio: <strong>3dslink</strong> → enter IP. Same LAN only; nothing goes to the cloud.</p>
    `,
  },
  {
    id: "audio",
    title: "Audio (dspfirm)",
    html: `
      <p>SFX and music need DSP firmware on the SD:</p>
      <p><code>sdmc:/3ds/dspfirm.cdc</code></p>
      <p>Dump it on <strong>your</strong> console: Luma Rosalina → Miscellaneous → Dump DSP firmware, then copy to that path.</p>
      <p><strong>Do not</strong> redistribute dumped firmware or download random mirrors. Without it, audio is usually silent.</p>
    `,
  },
  {
    id: "saves",
    title: "Saves",
    html: `
      <p>Slots and settings use:</p>
      <ul>
        <li><code>sdmc:/3ds/&lt;prefix&gt;_slot1.dat</code> … <code>_slot3.dat</code></li>
        <li><code>sdmc:/3ds/&lt;prefix&gt;_settings.dat</code></li>
      </ul>
      <p><code>&lt;prefix&gt;</code> comes from your game title so different projects do not overwrite each other.</p>
    `,
  },
  {
    id: "controls",
    title: "Controls",
    html: `
      <p>In-game Settings (on the console):</p>
      <ul>
        <li><strong>Controls</strong> NORMAL / EASY</li>
        <li><strong>Sprint Mode</strong> TOGGLE / HOLD</li>
      </ul>
      <p>The starter sample level teaches move + jump in dialogue and keeps advanced moves off until you enable them per level.</p>
    `,
  },
  {
    id: "cia",
    title: "CIA tools",
    html: `
      <p>CIA builds need extra tools under your DEVKITPRO <code>tools\\bin</code>:</p>
      <ul>
        <li><button type="button" class="help-link" data-url="https://github.com/3DSGuy/Project_CTR/releases">makerom (Project_CTR)</button></li>
        <li><button type="button" class="help-link" data-url="https://github.com/carstene1ns/3ds-bannertool/releases">bannertool</button></li>
      </ul>
      <p>Or place prebuilt <code>cia-icon.icn</code> and <code>cia-banner.bnr</code> in the project root. You also need <code>banner.png</code> (256×128) and <code>banner.wav</code>.</p>
    `,
  },
  {
    id: "troubleshoot",
    title: "Troubleshooting",
    html: `
      <ul>
        <li><strong>SmartScreen on install</strong> — unsigned build: More info → Run anyway.</li>
        <li><strong>Build says bash / DEVKITPRO missing</strong> — Setup tools… and point at <code>C:\\devkitPro</code>. Ignore Unix env values like <code>/opt/devkitpro</code>.</li>
        <li><strong>No rule for sprites.t3x</strong> — project path must have <strong>no spaces</strong> (<code>Documents/3DSStudio/MyGame</code>).</li>
        <li><strong>Cannot write game_config.h</strong> — folder missing <code>source/</code> (OneDrive emptied it). Re-open or Start Fresh Example.</li>
        <li><strong>Looking for .3dsx in build/</strong> — use <strong>Folder</strong>; outputs sit next to the Makefile.</li>
        <li><strong>Silent audio on 3DS</strong> — dump <code>dspfirm.cdc</code> to <code>sdmc:/3ds/</code> (do not download random dumps).</li>
      </ul>
    `,
  },
];
