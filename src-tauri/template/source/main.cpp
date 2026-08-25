// Basic Platformer for New 3DS XL
// Uses citro2d with tileset-based rendering.
// Tileset tiles are extracted as sub-images from tileset.png.

#include <citro2d.h>
#include <3ds.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "game_config.h"

// ========================================================
// Settings (persisted across sessions)
// ========================================================
struct Settings {
	bool muteMusic;
	bool muteSfx;
	bool sprintToggle; // true = press-to-toggle, false = hold-to-sprint
	bool easyControls; // true = forgiving physics / longer coyote & jump buffer
};
static Settings g_settings = { false, false, true, false };
static const char* SETTINGS_PATH = "/3ds/platformer_settings.dat";
static constexpr u32 SETTINGS_MAGIC_V1 = 0x504C4753; // 'PLGS' — 3 fields
static constexpr u32 SETTINGS_MAGIC    = 0x504C4754; // v2 — includes easyControls

static void saveSettings() {
	FILE* f = fopen(SETTINGS_PATH, "wb");
	if (!f) return;
	fwrite(&SETTINGS_MAGIC, sizeof(u32), 1, f);
	fwrite(&g_settings, sizeof(Settings), 1, f);
	fclose(f);
}

static void loadSettings() {
	FILE* f = fopen(SETTINGS_PATH, "rb");
	if (!f) return;
	u32 magic = 0;
	fread(&magic, sizeof(u32), 1, f);
	if (magic == SETTINGS_MAGIC) {
		fread(&g_settings, sizeof(Settings), 1, f);
	} else if (magic == SETTINGS_MAGIC_V1) {
		struct { bool muteMusic, muteSfx, sprintToggle; } old = {};
		fread(&old, sizeof(old), 1, f);
		g_settings.muteMusic = old.muteMusic;
		g_settings.muteSfx = old.muteSfx;
		g_settings.sprintToggle = old.sprintToggle;
		g_settings.easyControls = false;
	}
	fclose(f);
}

// ========================================================
// Audio — procedural SFX via NDSP
// ========================================================
enum SfxId { SFX_JUMP = 0, SFX_WALL_JUMP, SFX_DASH, SFX_COIN, SFX_DEATH, SFX_STOMP, SFX_COUNT };
static constexpr int SFX_SAMPLE_RATE = 22050;
static ndspWaveBuf sfxWaveBuf[SFX_COUNT];
static s16*        sfxData[SFX_COUNT];
static int         sfxLen[SFX_COUNT]; // in samples

static void genSfxJump(s16* buf, int len) {
	// Rising square wave chirp
	for (int i = 0; i < len; i++) {
		float t = (float)i / len;
		float freq = 300.0f + 400.0f * t;
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		float vol = 1.0f - t; // fade out
		buf[i] = (s16)((phase < 0.5f ? 8000 : -8000) * vol);
	}
}

static void genSfxWallJump(s16* buf, int len) {
	// Same style as jump but higher pitch
	for (int i = 0; i < len; i++) {
		float t = (float)i / len;
		float freq = 450.0f + 500.0f * t;
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		float vol = 1.0f - t;
		buf[i] = (s16)((phase < 0.5f ? 8000 : -8000) * vol);
	}
}

static void genSfxDash(s16* buf, int len) {
	// Sharp high-pitched wind sweep
	for (int i = 0; i < len; i++) {
		float t = (float)i / len;
		float freq = 1800.0f - 1200.0f * t; // high sweep down
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		// Thin pulse wave (20% duty) for airy/sharp feel
		float wave = (phase < 0.2f) ? 1.0f : -0.3f;
		// Fast attack, smooth decay
		float env = (t < 0.05f) ? (t / 0.05f) : (1.0f - (t - 0.05f) / 0.95f);
		buf[i] = (s16)(wave * 5000.0f * env);
	}
}

static void genSfxCoin(s16* buf, int len) {
	int half = len / 2;
	for (int i = 0; i < len; i++) {
		float freq = (i < half) ? 987.0f : 1319.0f;
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		float env = 1.0f - (float)i / len;
		buf[i] = (s16)(sinf(phase * 2.0f * M_PI) * 7000.0f * env);
	}
}

static void genSfxDeath(s16* buf, int len) {
	for (int i = 0; i < len; i++) {
		float t = (float)i / len;
		float freq = 400.0f - 300.0f * t;
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		float vol = 1.0f - t * 0.7f;
		buf[i] = (s16)((phase < 0.5f ? 7000 : -7000) * vol);
	}
}

static void genSfxStomp(s16* buf, int len) {
	for (int i = 0; i < len; i++) {
		float t = (float)i / len;
		float freq = 600.0f + 200.0f * t;
		float phase = fmodf(i * freq / SFX_SAMPLE_RATE, 1.0f);
		float vol = 1.0f - t;
		buf[i] = (s16)(sinf(phase * 2.0f * M_PI) * 6000.0f * vol);
	}
}

static void initSfx() {
	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

	// Channel config
	for (int i = 0; i < SFX_COUNT; i++) {
		ndspChnReset(i);
		ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
		ndspChnSetRate(i, SFX_SAMPLE_RATE);
		ndspChnSetFormat(i, NDSP_FORMAT_MONO_PCM16);
		float mix[12] = {0};
		mix[0] = 0.8f; mix[1] = 0.8f;
		ndspChnSetMix(i, mix);
	}

	// Allocate and generate waveforms
	int lengths[SFX_COUNT] = {
		SFX_SAMPLE_RATE / 8,   // jump
		SFX_SAMPLE_RATE / 7,   // wall jump
		SFX_SAMPLE_RATE / 5,   // dash
		SFX_SAMPLE_RATE / 4,   // coin
		SFX_SAMPLE_RATE / 3,   // death
		SFX_SAMPLE_RATE / 6,   // stomp
	};
	void (*generators[SFX_COUNT])(s16*, int) = {
		genSfxJump, genSfxWallJump, genSfxDash, genSfxCoin, genSfxDeath, genSfxStomp
	};

	for (int i = 0; i < SFX_COUNT; i++) {
		sfxLen[i] = lengths[i];
		sfxData[i] = (s16*)linearAlloc(lengths[i] * sizeof(s16));
		generators[i](sfxData[i], lengths[i]);
		DSP_FlushDataCache(sfxData[i], lengths[i] * sizeof(s16));

		memset(&sfxWaveBuf[i], 0, sizeof(ndspWaveBuf));
		sfxWaveBuf[i].data_vaddr = sfxData[i];
		sfxWaveBuf[i].nsamples   = lengths[i];
		sfxWaveBuf[i].looping    = false;
	}
}

static void exitSfx() {
	ndspExit();
	for (int i = 0; i < SFX_COUNT; i++) {
		if (sfxData[i]) linearFree(sfxData[i]);
	}
}

static void playSfx(int id) {
	if (g_settings.muteSfx) return;
	ndspChnWaveBufClear(id);
	sfxWaveBuf[id].status = NDSP_WBUF_FREE;
	ndspChnWaveBufAdd(id, &sfxWaveBuf[id]);
}

// ========================================================
// Music — streaming MP3 via minimp3 + NDSP
// ========================================================
static constexpr int MUSIC_CH = 6;
static constexpr int MUSIC_NUM_BUFS = 3;
static constexpr int MUSIC_SAMPLES_PER_BUF = 1152 * 4; // ~4 MP3 frames

static mp3dec_t      mp3d;
static u8*           mp3FileData = NULL;
static size_t        mp3FileSize = 0;
static size_t        mp3Pos = 0;
static ndspWaveBuf   musicWaveBuf[MUSIC_NUM_BUFS];
static s16*          musicPcm[MUSIC_NUM_BUFS];
static bool          musicInited = false;
static int           musicRate = 44100;
static int           musicChans = 2;
static float         musicVolume = 0.7f;

static void initMusic() {
	FILE* f = fopen("romfs:/soundtrack.mp3", "rb");
	if (!f) return;
	fseek(f, 0, SEEK_END);
	mp3FileSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	mp3FileData = (u8*)malloc(mp3FileSize);
	if (!mp3FileData) { fclose(f); return; }
	fread(mp3FileData, 1, mp3FileSize, f);
	fclose(f);

	// Decode one frame to detect sample rate and channels
	mp3dec_init(&mp3d);
	mp3dec_frame_info_t info;
	s16 tempPcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	mp3dec_decode_frame(&mp3d, mp3FileData, mp3FileSize, tempPcm, &info);
	if (info.hz == 0) { free(mp3FileData); mp3FileData = NULL; return; }
	musicRate  = info.hz;
	musicChans = info.channels;

	// Reset decoder for real playback
	mp3dec_init(&mp3d);
	mp3Pos = 0;

	// Configure NDSP channel
	ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CH, (float)musicRate);
	ndspChnSetFormat(MUSIC_CH, musicChans == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	float mix[12] = {0};
	if (!g_settings.muteMusic) {
		mix[0] = musicVolume; // left
		mix[1] = musicVolume; // right
	}
	ndspChnSetMix(MUSIC_CH, mix);

	// Allocate wave buffers
	size_t bufBytes = MUSIC_SAMPLES_PER_BUF * musicChans * sizeof(s16);
	for (int i = 0; i < MUSIC_NUM_BUFS; i++) {
		musicPcm[i] = (s16*)linearAlloc(bufBytes);
		memset(&musicWaveBuf[i], 0, sizeof(ndspWaveBuf));
		musicWaveBuf[i].data_vaddr = musicPcm[i];
		musicWaveBuf[i].status = NDSP_WBUF_DONE;
	}

	musicInited = true;
}

static void musicFillBuf(int idx) {
	int totalSamples = 0;
	s16* out = musicPcm[idx];

	while (totalSamples < MUSIC_SAMPLES_PER_BUF && mp3Pos < mp3FileSize) {
		mp3dec_frame_info_t info;
		int samples = mp3dec_decode_frame(&mp3d, mp3FileData + mp3Pos,
			mp3FileSize - mp3Pos, out + totalSamples * musicChans, &info);
		if (info.frame_bytes == 0) break;
		mp3Pos += info.frame_bytes;
		totalSamples += samples;
	}

	if (totalSamples == 0) {
		// End of file — loop
		mp3dec_init(&mp3d);
		mp3Pos = 0;
		musicFillBuf(idx);
		return;
	}

	musicWaveBuf[idx].nsamples = totalSamples;
	DSP_FlushDataCache(musicPcm[idx], totalSamples * musicChans * sizeof(s16));
	ndspChnWaveBufAdd(MUSIC_CH, &musicWaveBuf[idx]);
}

static void updateMusic() {
	if (!musicInited) return;
	for (int i = 0; i < MUSIC_NUM_BUFS; i++) {
		if (musicWaveBuf[i].status == NDSP_WBUF_DONE) {
			musicFillBuf(i);
		}
	}
}

static void applyMusicMute() {
	if (!musicInited) return;
	float mix[12] = {0};
	if (!g_settings.muteMusic) {
		mix[0] = musicVolume;
		mix[1] = musicVolume;
	}
	ndspChnSetMix(MUSIC_CH, mix);
}

static void exitMusic() {
	if (!musicInited) return;
	ndspChnWaveBufClear(MUSIC_CH);
	for (int i = 0; i < MUSIC_NUM_BUFS; i++) {
		if (musicPcm[i]) linearFree(musicPcm[i]);
	}
	if (mp3FileData) free(mp3FileData);
	musicInited = false;
}

// ========================================================
// Screen dimensions
// ========================================================
static constexpr int TOP_WIDTH  = 400;
static constexpr int TOP_HEIGHT = 240;
static constexpr int BOT_WIDTH  = 320;
static constexpr int BOT_HEIGHT = 240;

// ========================================================
// Tileset configuration
//   TILESET_COLS: number of tile columns in tileset.png
//   Adjust this to match your actual tileset image layout.
// ========================================================
static constexpr int TILE_SIZE     = 32;
static constexpr int TILESET_COLS  = 16;

// ========================================================
// Tilemap dimensions (in tiles)
// ========================================================
static constexpr int MAP_W = 80;
static constexpr int MAP_H = 128; // max tiles tall — per-level height from LEVEL_INFO (up to 128)

// ========================================================
// Physics (global defaults from game_config.h; per-level overrides in initLevel)
// ========================================================
static float GRAVITY       = GC_GRAVITY;
static float GRAVITY_FALL  = GC_GRAVITY_FALL;
static float JUMP_FORCE    = GC_JUMP_FORCE;
static float DJUMP_FORCE   = GC_DJUMP_FORCE;
static constexpr float JUMP_CUT_MUL  = 0.35f;   // multiply vy when jump released early
static float MOVE_SPEED    = GC_MOVE_SPEED;
static float SPRINT_SPEED  = GC_SPRINT_SPEED;
static constexpr float ACCEL_GROUND  = 0.6f;    // ground acceleration per frame
static constexpr float ACCEL_AIR     = 0.35f;   // air acceleration per frame
static constexpr float DECEL_GROUND  = 0.70f;   // friction multiplier when no input
static constexpr float DECEL_AIR     = 0.92f;   // air friction
static constexpr float SKID_DECEL    = 0.55f;   // friction when reversing direction
static constexpr float MAX_FALL_SPD  = 10.0f;
static constexpr float WALL_SLIDE_SPD = 2.0f;   // max fall speed while wall-sliding
static constexpr float WALL_JUMP_VX  = 4.0f;    // horizontal kick from wall jump
static constexpr float WALL_JUMP_VY  = -7.8f;   // vertical force from wall jump
static constexpr int   WALL_JUMP_LOCK = 4;      // hard lock frames (short, then gradual blend)
static constexpr int   WALL_JUMP_BLEND = 10;    // frames of blended control after lock
static constexpr float WALL_JUMP_BLEND_STR = 0.35f; // how much player input overrides during blend
static constexpr int   COYOTE_FRAMES  = 5;      // frames after leaving ground where jump still works
static constexpr int   JUMP_BUFFER_FRAMES = 6;  // frames before landing where jump input is remembered
static constexpr int   EASY_COYOTE_FRAMES = 12;
static constexpr int   EASY_JUMP_BUFFER_FRAMES = 14;

static int effectiveCoyoteFrames() {
	return g_settings.easyControls ? EASY_COYOTE_FRAMES : COYOTE_FRAMES;
}
static int effectiveJumpBufferFrames() {
	return g_settings.easyControls ? EASY_JUMP_BUFFER_FRAMES : JUMP_BUFFER_FRAMES;
}
static float effectiveAccelGround() {
	return g_settings.easyControls ? 0.85f : ACCEL_GROUND;
}
static float effectiveAccelAir() {
	return g_settings.easyControls ? 0.52f : ACCEL_AIR;
}
static float effectiveDecelGround() {
	return g_settings.easyControls ? 0.55f : DECEL_GROUND;
}
static float effectiveDecelAir() {
	return g_settings.easyControls ? 0.88f : DECEL_AIR;
}
static float effectiveSkidDecel() {
	return g_settings.easyControls ? 0.35f : SKID_DECEL;
}
static float effectiveJumpCutMul() {
	return g_settings.easyControls ? 0.55f : JUMP_CUT_MUL;
}
static float effectiveJumpForce() {
	return JUMP_FORCE * (g_settings.easyControls ? 1.08f : 1.0f);
}
static float effectiveDJumpForce() {
	return DJUMP_FORCE * (g_settings.easyControls ? 1.08f : 1.0f);
}
static float effectiveGravityFall() {
	return GRAVITY_FALL * (g_settings.easyControls ? 0.85f : 1.0f);
}
static float effectiveMaxFallSpd() {
	return g_settings.easyControls ? 8.5f : MAX_FALL_SPD;
}
// Ground pound
static constexpr float GPOUND_SPEED  = 14.0f;   // downward speed during ground pound
static constexpr int   GPOUND_FREEZE = 4;       // frames of hang before pound starts
// Dash
static float DASH_SPEED    = GC_DASH_SPEED;
static constexpr int   DASH_DURATION = 6;       // frames the dash lasts
static constexpr int   DASH_COOLDOWN = 20;       // frames before dash can be used again
// Stereoscopic 3D
static constexpr float DEPTH_BG      = 4.0f;
static constexpr float DEPTH_TILES   = 2.0f;
static constexpr float DEPTH_OBJECTS = 1.0f;
static constexpr float DEPTH_PLAYER  = 2.0f;
static constexpr float BG_PARALLAX   = 0.5f;
// Polish
static constexpr int   INVULN_FRAMES  = 60;
static constexpr int   DEATH_FREEZE   = 20;
static constexpr int   DEATH_POP_TIME = 30;
static constexpr float DEATH_POP_VY   = -6.0f;
static constexpr int   FADE_FRAMES    = 15;
static constexpr float CAM_LOOK_AHEAD = 40.0f;
static constexpr float CAM_LOOK_SPEED = 0.05f;
static constexpr float CAM_DEAD_ZONE_Y = 20.0f;

// ========================================================
// Colors (ABGR format)
// ========================================================
static constexpr u32 CLR_BG     = 0xFFFFB464; // sky blue
static constexpr u32 CLR_PLAYER = 0xFF00AAFF; // orange
static constexpr u32 CLR_COIN   = 0xFF00DDFF; // gold

// ========================================================
// Sprite indices — order must match gfx/sprites.t3s
// Currently only tileset.png is listed. Add more PNGs to
// sprites.t3s and append indices here as needed.
enum SpriteIndex {
	SPR_TILESET = 0,
	SPR_BOTTOM_MENU = 1,
	SPR_MENU_LOAD = 2,
	SPR_MENU_NEW = 3,
	SPR_MENU_SETTINGS = 4,
	SPR_TITLE = 5,
	SPR_CAVE_BG = 6,
	SPR_CAVE_BG2 = 7,
	SPR_COUNT
};

// ========================================================
// Tile types for the tilemap
// ========================================================
enum TileType : u8 {
	TILE_EMPTY    = 0,
	TILE_GROUND   = 1, // 1-1 (col 1, row 1) — ground surface
	TILE_FILL     = 2, // 1-2 (col 1, row 2) — underground fill
	TILE_SPIKE    = 3, // 6-1 (col 6, row 1) — hazard
	TILE_PLATFORM = 4, // 9-1 (col 9, row 1) — default platform
	TILE_PLAT_ALT = 5, // 8-1 (col 8, row 1) — one-way platform
	TILE_BG_DECOR = 6, // 10-1 (col 10, row 1) — parallax background
	TILE_CRUMBLE  = 7, // 7-1 (col 7, row 1) — crumbling platform
	TILE_MOVING   = 8, // 5-1 (col 5, row 1) — moving platform marker
	TILE_WARP     = 9, // 4-1 (col 4, row 1) — warp portal (overlays existing tile)
	TILE_TYPE_COUNT
};

// Coordinates within tileset.png (1-indexed, X-Y i.e. col-row)
struct TileCoord { int row; int col; };
static constexpr TileCoord TILE_UV[TILE_TYPE_COUNT] = {
	{ 0, 0 }, // TILE_EMPTY — unused
	{ 1, 1 }, // TILE_GROUND   -> 1-1 (col 1, row 1)
	{ 2, 1 }, // TILE_FILL     -> 1-2 (col 1, row 2)
	{ 1, 6 }, // TILE_SPIKE    -> 6-1 (col 6, row 1)
	{ 1, 9 }, // TILE_PLATFORM -> 9-1 (col 9, row 1)
	{ 1, 8 }, // TILE_PLAT_ALT -> 8-1 (col 8, row 1)
	{ 1, 10}, // TILE_BG_DECOR -> 10-1 (col 10, row 1)
	{ 1, 7 }, // TILE_CRUMBLE  -> 7-1 (col 7, row 1)
	{ 1, 5 }, // TILE_MOVING   -> 5-1 (col 5, row 1)
	{ 1, 4 }, // TILE_WARP     -> 4-1 (col 4, row 1)
};

// ========================================================
// Tile image cache — one C2D_Image per tile type,
// each referencing a sub-region of tileset.png in the atlas
// ========================================================
static Tex3DS_SubTexture tileSubTex[TILE_TYPE_COUNT];
static C2D_Image         tileImg[TILE_TYPE_COUNT];

// VFX images (not grid tiles — extracted from tileset for dynamic use)
enum VfxImgId { VFX_WALL_JUMP = 0, VFX_DASH = 1, VFX_IMG_COUNT };
static constexpr TileCoord VFX_UV[VFX_IMG_COUNT] = {
	{ 1, 11 }, // 11-1 wall jump effect
	{ 1, 12 }, // 12-1 dash effect
};

// Cracker sprite — extracted from tileset
static constexpr TileCoord COIN_UV = { 1, 13 }; // 13-1 (col 13, row 1)
static Tex3DS_SubTexture coinSubTex;
static C2D_Image         coinImg;

// Grass overlays — drawn on top of any TILE_GROUND. Two animated frames per kind.
// Default grass renders BEHIND the player/enemies; alt grass renders IN FRONT.
enum GrassImgId {
	GRASS_DEF_A = 0, GRASS_DEF_B,
	GRASS_ALT_A,     GRASS_ALT_B,
	GRASS_IMG_COUNT
};
static constexpr TileCoord GRASS_UV[GRASS_IMG_COUNT] = {
	{ 1, 2 }, // default frame A: col 2, row 1
	{ 1, 3 }, // default frame B: col 3, row 1
	{ 2, 2 }, // alt frame A: col 2, row 2
	{ 2, 3 }, // alt frame B: col 3, row 2
};
static Tex3DS_SubTexture grassSubTex[GRASS_IMG_COUNT];
static C2D_Image         grassImg[GRASS_IMG_COUNT];
static int grassAnimTimer = 0;
static Tex3DS_SubTexture vfxSubTex[VFX_IMG_COUNT];
static C2D_Image         vfxImg[VFX_IMG_COUNT];

// Player sprites — row 3 of tileset, columns 1..11
enum PlayerSpr {
	PSPR_IDLE1 = 0, PSPR_IDLE2,
	PSPR_RUN_INIT, PSPR_RUN_STEP1, PSPR_RUN_MID, PSPR_RUN_STEP2,
	PSPR_WALL_SLIDE, PSPR_DASH, PSPR_GPOUND_FALL, PSPR_GPOUND_LAND,
	PSPR_JUMP_NEUTRAL,
	PSPR_COUNT
};
static Tex3DS_SubTexture playerSubTex[PSPR_COUNT];
static C2D_Image         playerImg[PSPR_COUNT];

// Animation state
static int  animIdleTimer = 0;
static int  animRunFrame  = 0;
static int  animRunTimer  = 0;
static int  animGpoundLandTimer = 0;
static int  animRunInitTimer = 0;
static bool animWasMoving = false;

// Enemy sprites — row 4 of tileset, columns 1..2
static constexpr int ESPR_COUNT = 2;
static Tex3DS_SubTexture enemySubTex[ESPR_COUNT];
static C2D_Image         enemyImg[ESPR_COUNT];

// Stomp combo
static int stompCombo = 0;
static constexpr float STOMP_BOUNCE      = -5.5f;  // normal stomp bounce
static constexpr float STOMP_SUPER       = -9.0f;  // hold-jump super bounce
static constexpr float STOMP_COMBO_BONUS = -1.0f;  // extra bounce per combo

// Crumbling platforms
static constexpr int MAX_CRUMBLE = 128;
struct CrumbleTile { int tx, ty; int timer; bool shaking; bool fallen; };
static CrumbleTile crumbles[MAX_CRUMBLE];
static int numCrumbles = 0;
static constexpr int CRUMBLE_SHAKE_TIME = 20;
static constexpr int CRUMBLE_RESPAWN_TIME = 180;
// Reverse index from tile coord -> crumbles[] index (-1 if none). Built in initLevel.
static short crumbleIndex[MAP_H][MAP_W];

// Minimap static-tile cache: built once per level for all non-empty, non-dynamic tiles.
// Dynamic items (crumbles, coins, movers, player) are drawn per frame on top.
struct MinimapRect { unsigned char tx, ty; unsigned int color; };
static MinimapRect minimapCache[80 * 32];
static int minimapCacheCount = 0;

// Moving platforms
static constexpr int MAX_MOVERS = 16;
struct MovingPlatform {
	float x, y;         // current position (world pixels)
	float startX;       // origin X
	float range;        // total travel distance
	float speed;        // pixels per frame
	int   dir;          // 1 or -1
	int   tileType;     // visual tile to draw
};
static MovingPlatform movers[MAX_MOVERS];
static int numMovers = 0;

static void spawnMover(float x, float y, float range, float speed) {
	if (numMovers >= MAX_MOVERS) return;
	MovingPlatform& m = movers[numMovers++];
	m.x = x; m.y = y; m.startX = x;
	m.range = range; m.speed = speed; m.dir = 1;
	m.tileType = TILE_MOVING;
}

// Enemy flatten on stomp
static constexpr int MAX_FLATTEN = 8;
struct FlattenFx { float x, y; int timer; bool active; bool flipX; };
static FlattenFx flattenFx[MAX_FLATTEN];

// Stomp combo popup
static constexpr int MAX_POPUPS = 8;
struct ComboPopup { float x, y; int value; int timer; bool active; };
static ComboPopup popups[MAX_POPUPS];

static bool levelAllowsDoubleJump = false;
static bool levelAllowsWallJump = true;
static bool levelAllowsDash = true;
static bool levelAllowsGroundPound = true;
static bool levelAllowsDialogue = true;
static bool levelAllowsMinimap = true;

// Cracker magnet
static constexpr float COIN_MAGNET_RANGE = 64.0f;
static constexpr float COIN_MAGNET_SPEED = 4.0f;

// Level counts
static constexpr int NUM_LEVELS = 10;
static constexpr int TOTAL_LEVELS = 11;
static constexpr int LAST_VISIBLE_LEVEL = 9;  // index of last non-hidden level (level 10, 0-indexed)
static constexpr int LAST_HIDDEN_LEVEL = 10;  // index of last hidden/secret level

// Level metadata struct - defined early so macros can use it
struct LevelInfo {
	const char* displayName;
	int world;
	bool isHidden;
	float parTime;
	int width;
	int height;
	bool doubleJump;
	bool dialogue;
	bool wallJump;
	bool dash;
	bool groundPound;
	bool minimap;
};

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

// Fallback LEVEL_INFO that matches old hardcoded behavior (World 1: 6 levels, World 2: 4 levels, Secret: 1)
static const LevelInfo FALLBACK_LEVEL_INFO[TOTAL_LEVELS] = {
	/* 0 */ {"1-1", 1, false, 30.0f, 80, 16, false, true, true, true, true, true},
	/* 1 */ {"1-2", 1, false, 45.0f, 80, 16, false, true, true, true, true, true},
	/* 2 */ {"1-3", 1, false, 60.0f, 80, 16, false, true, true, true, true, true},
	/* 3 */ {"1-4", 1, false, 45.0f, 80, 16, false, true, true, true, true, true},
	/* 4 */ {"1-5", 1, false, 60.0f, 80, 16, false, true, true, true, true, true},
	/* 5 */ {"1-6", 1, false, 75.0f, 80, 16, false, true, true, true, true, true},
	/* 6 */ {"2-1", 2, false, 60.0f, 80, 32, true, true, true, true, true, true},
	/* 7 */ {"2-2", 2, false, 75.0f, 80, 32, true, true, true, true, true, true},
	/* 8 */ {"2-3", 2, false, 90.0f, 80, 32, true, true, true, true, true, true},
	/* 9 */ {"2-4", 2, false, 75.0f, 80, 32, true, true, true, true, true, true},
	/* 10 */ {"???", 0, true, 120.0f, 80, 32, true, true, true, true, true, true},
};

// Forward declarations for level metadata (editor will override these macros)
#ifndef LEVEL_COUNT_MACRO
#define LEVEL_COUNT_MACRO TOTAL_LEVELS
#endif
#ifndef LEVEL_INFO_MACRO
#define LEVEL_INFO_MACRO FALLBACK_LEVEL_INFO
#endif
#define LEVEL_COUNT (LEVEL_COUNT_MACRO)
#define LEVEL_INFO (LEVEL_INFO_MACRO)

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

// Par times (seconds) and star ratings per level
static float parTimes[TOTAL_LEVELS] = {
	30.0f, 45.0f, 60.0f, 45.0f, 60.0f, 75.0f,  // world 1: levels 1-6
	60.0f, 75.0f, 90.0f, 75.0f,                  // world 2: levels 7-10
	120.0f,                                        // secret level
};
static int starRatings[TOTAL_LEVELS] = { 0 }; // 0-3 stars per level

// Dialogue system: max 4 boxes per level (before) and 4 boxes (after), 256 chars each
static constexpr int MAX_DIALOGUE_BOXES = 4;
static constexpr int MAX_DIALOGUE_CHARS = 256;
static char dialoguePre[TOTAL_LEVELS][MAX_DIALOGUE_BOXES][MAX_DIALOGUE_CHARS] = { 0 };
static char dialoguePost[TOTAL_LEVELS][MAX_DIALOGUE_BOXES][MAX_DIALOGUE_CHARS] = { 0 };
static int dialoguePreCount[TOTAL_LEVELS] = { 0 };
static int dialoguePostCount[TOTAL_LEVELS] = { 0 };

// Dialogue runtime state
static int currentDialogueBox = 0;
static int currentDialogueChar = 0;  // index into RAW text
static int dialogueTypewriterTimer = 0;
static int dialoguePauseTimer = 0;   // countdown for {p:N} pauses
static bool dialogueWaitingForInput = false;
static constexpr int TYPEWRITER_DELAY = 2; // frames between characters

static void initTileImages(C2D_SpriteSheet sheet) {
	C2D_Image tsImg = C2D_SpriteSheetGetImage(sheet, SPR_TILESET);
	const Tex3DS_SubTexture* ts = tsImg.subtex;

	float sheetW = (float)ts->width;
	float sheetH = (float)ts->height;
	float uRange = ts->right  - ts->left;
	float vRange = ts->bottom - ts->top;

	for (int i = 1; i < TILE_TYPE_COUNT; i++) {
		int col = TILE_UV[i].col - 1;
		int row = TILE_UV[i].row - 1;
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;

		tileSubTex[i].width  = TILE_SIZE;
		tileSubTex[i].height = TILE_SIZE;
		tileSubTex[i].left   = ts->left + (px / sheetW) * uRange;
		tileSubTex[i].right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		tileSubTex[i].top    = ts->top  + (py / sheetH) * vRange;
		tileSubTex[i].bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;

		tileImg[i].tex    = tsImg.tex;
		tileImg[i].subtex = &tileSubTex[i];
	}

	// Extract coin (cheezit) image
	{
		int col = COIN_UV.col - 1;
		int row = COIN_UV.row - 1;
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;
		coinSubTex.width  = TILE_SIZE;
		coinSubTex.height = TILE_SIZE;
		coinSubTex.left   = ts->left + (px / sheetW) * uRange;
		coinSubTex.right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		coinSubTex.top    = ts->top  + (py / sheetH) * vRange;
		coinSubTex.bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;
		coinImg.tex    = tsImg.tex;
		coinImg.subtex = &coinSubTex;
	}

	// Extract VFX images
	for (int i = 0; i < VFX_IMG_COUNT; i++) {
		int col = VFX_UV[i].col - 1;
		int row = VFX_UV[i].row - 1;
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;

		vfxSubTex[i].width  = TILE_SIZE;
		vfxSubTex[i].height = TILE_SIZE;
		vfxSubTex[i].left   = ts->left + (px / sheetW) * uRange;
		vfxSubTex[i].right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		vfxSubTex[i].top    = ts->top  + (py / sheetH) * vRange;
		vfxSubTex[i].bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;

		vfxImg[i].tex    = tsImg.tex;
		vfxImg[i].subtex = &vfxSubTex[i];
	}

	// Extract grass overlay images
	for (int i = 0; i < GRASS_IMG_COUNT; i++) {
		int col = GRASS_UV[i].col - 1;
		int row = GRASS_UV[i].row - 1;
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;

		grassSubTex[i].width  = TILE_SIZE;
		grassSubTex[i].height = TILE_SIZE;
		grassSubTex[i].left   = ts->left + (px / sheetW) * uRange;
		grassSubTex[i].right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		grassSubTex[i].top    = ts->top  + (py / sheetH) * vRange;
		grassSubTex[i].bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;

		grassImg[i].tex    = tsImg.tex;
		grassImg[i].subtex = &grassSubTex[i];
	}

	// Extract player sprites — row 3 (0-indexed row 2), columns 1..10
	for (int i = 0; i < PSPR_COUNT; i++) {
		int col = i;       // columns 0..9 (0-indexed)
		int row = 2;       // row 3 (0-indexed)
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;

		playerSubTex[i].width  = TILE_SIZE;
		playerSubTex[i].height = TILE_SIZE;
		playerSubTex[i].left   = ts->left + (px / sheetW) * uRange;
		playerSubTex[i].right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		playerSubTex[i].top    = ts->top  + (py / sheetH) * vRange;
		playerSubTex[i].bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;

		playerImg[i].tex    = tsImg.tex;
		playerImg[i].subtex = &playerSubTex[i];
	}

	// Extract enemy sprites — row 4 (0-indexed row 3), columns 1..2
	for (int i = 0; i < ESPR_COUNT; i++) {
		int col = i;       // 0-indexed
		int row = 3;       // row 4 (0-indexed)
		float px = col * TILE_SIZE;
		float py = row * TILE_SIZE;

		enemySubTex[i].width  = TILE_SIZE;
		enemySubTex[i].height = TILE_SIZE;
		enemySubTex[i].left   = ts->left + (px / sheetW) * uRange;
		enemySubTex[i].right  = ts->left + ((px + TILE_SIZE) / sheetW) * uRange;
		enemySubTex[i].top    = ts->top  + (py / sheetH) * vRange;
		enemySubTex[i].bottom = ts->top  + ((py + TILE_SIZE) / sheetH) * vRange;

		enemyImg[i].tex    = tsImg.tex;
		enemyImg[i].subtex = &enemySubTex[i];
	}
}

// ========================================================
// Tilemap
// ========================================================
static u8 tilemap[MAP_H][MAP_W];

// 3D tile overlay: true = tile pops forward with 3D slider and becomes passthrough when extended
static bool tile3dMap[MAP_H][MAP_W];

// Warp tile overlay: true = stepping on this tile warps to special level
static bool warpMap[MAP_H][MAP_W];
static int  warpTargetLevel = -1; // level index to warp to (-1 = none set)

// Win/goal overlay: stepping on any painted win tile completes the level
static bool winMap[MAP_H][MAP_W];
static bool levelHasWinZone = false;

// Checkpoint overlay: editor-placed respawn markers (8×32 px red/green poles)
static bool checkpointMap[MAP_H][MAP_W];
static constexpr int MAX_CHECKPOINTS = 64;
static constexpr float CP_WIDTH = 8.0f;
static constexpr float CP_HEIGHT = 32.0f;
struct Checkpoint {
	short tx, ty;
	bool activated;
};
static Checkpoint checkpoints[MAX_CHECKPOINTS];
static int numCheckpoints = 0;
static int lastCheckpointIdx = -1;
static bool levelHasCheckpoints = false;

// Per-save-slot session: activated flags + last touched checkpoint per level
static bool sessionCpActivated[TOTAL_LEVELS][MAP_H][MAP_W];
static int sessionLastCpIdx[TOTAL_LEVELS];
static float sessionLevelTimer[TOTAL_LEVELS];
static int sessionCrackersCommitted = 0;
static bool devMode = false;

// Current 3D slider state cached per frame for collision checks
static float g_3dSlider = 0.0f;
static constexpr float SLIDER_PASSTHROUGH_THRESHOLD = 0.5f; // slider > this = 3D tiles become passthrough
static constexpr float TILE3D_FADE_START = TILE_SIZE * 4.5f; // fully opaque beyond this
static constexpr float TILE3D_FADE_END   = TILE_SIZE * 0.35f; // strongest fade when this close
static constexpr u8    TILE3D_MIN_ALPHA  = 90;

static bool tile3dIsPassable(int tx, int ty);

static int getActiveMapHeight();
static int getActiveMapWidth();

static u8 getTile(int tx, int ty) {
	int w = getActiveMapWidth();
	int h = getActiveMapHeight();
	if (tx < 0 || ty < 0 || tx >= w || ty >= h) return TILE_EMPTY;
	if (tx >= MAP_W || ty >= MAP_H) return TILE_EMPTY;
	return tilemap[ty][tx];
}

static bool is3dTile(int tx, int ty) {
	int w = getActiveMapWidth();
	int h = getActiveMapHeight();
	if (tx < 0 || ty < 0 || tx >= w || ty >= h) return false;
	if (tx >= MAP_W || ty >= MAP_H) return false;
	return tile3dMap[ty][tx];
}

static bool isWarpTile(int tx, int ty) {
	int w = getActiveMapWidth();
	int h = getActiveMapHeight();
	if (tx < 0 || ty < 0 || tx >= w || ty >= h) return false;
	if (tx >= MAP_W || ty >= MAP_H) return false;
	return warpMap[ty][tx];
}

static bool isWinTile(int tx, int ty) {
	int w = getActiveMapWidth();
	int h = getActiveMapHeight();
	if (tx < 0 || ty < 0 || tx >= w || ty >= h) return false;
	if (tx >= MAP_W || ty >= MAP_H) return false;
	return winMap[ty][tx];
}

static void refreshLevelHasWinZone() {
	levelHasWinZone = false;
	int maxH = getActiveMapHeight();
	int maxW = getActiveMapWidth();
	for (int ty = 0; ty < maxH && ty < MAP_H; ty++) {
		for (int tx = 0; tx < maxW && tx < MAP_W; tx++) {
			if (winMap[ty][tx]) {
				levelHasWinZone = true;
				return;
			}
		}
	}
}

// ========================================================
// Menu scene — scrolling main-menu background with auto-running character
// ========================================================
static constexpr int MENU_MAP_W = 80;
static constexpr int MENU_MAP_H = 16;
static u8 menuTilemap[MENU_MAP_H][MENU_MAP_W];
static float menuRunnerStartX = 64.0f;
static float menuRunnerStartY = 13 * TILE_SIZE - 28;
static float menuRunnerX = 64.0f;
static float menuRunnerY = 0.0f;
static float menuRunnerVy = 0.0f;
static bool  menuRunnerOnGround = false;
static float menuCamX = 0.0f;
static int   menuRunnerAnimTimer = 0;
static int   menuRunnerFrame = 0;

static u8 getMenuTile(int tx, int ty) {
	if (ty < 0 || ty >= MENU_MAP_H) return TILE_EMPTY;
	// Wrap horizontally so the menu world behaves as a torus.
	int wx = tx % MENU_MAP_W;
	if (wx < 0) wx += MENU_MAP_W;
	return menuTilemap[ty][wx];
}
static bool menuTileSolid(int tx, int ty) {
	u8 t = getMenuTile(tx, ty);
	return t == TILE_GROUND || t == TILE_FILL || t == TILE_PLATFORM ||
	       t == TILE_PLAT_ALT || t == TILE_CRUMBLE;
}

// ========================================================
// Grass overlays — autogenerated where TILE_GROUND is exposed.
// 0 = no grass, 1 = default (drawn behind player), 2 = alt (drawn in front).
// ========================================================
static u8 grassMap[MAP_H][MAP_W];
static u8 menuGrassMap[MENU_MAP_H][MENU_MAP_W];

static void populateGrass(int w, int h) {
	memset(grassMap, 0, sizeof(grassMap));
	for (int ty = 0; ty < h - 1 && ty < MAP_H; ty++) {
		for (int tx = 0; tx < w && tx < MAP_W; tx++) {
			if (tilemap[ty][tx] != TILE_EMPTY) continue;
			if (tilemap[ty + 1][tx] != TILE_GROUND) continue;
			grassMap[ty][tx] = 1;
		}
	}
	const int MIN_ALT_SPACING = 4;
	for (int ty = 0; ty < h - 1 && ty < MAP_H; ty++) {
		for (int tx = 0; tx < w && tx < MAP_W; tx++) {
			if (grassMap[ty][tx] != 1) continue;
			if (rand() % 10 != 0) continue;
			bool tooClose = false;
			for (int dy = -MIN_ALT_SPACING; dy <= MIN_ALT_SPACING && !tooClose; dy++) {
				for (int dx = -MIN_ALT_SPACING; dx <= MIN_ALT_SPACING; dx++) {
					int nx = tx + dx, ny = ty + dy;
					if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
					if (grassMap[ny][nx] == 2) { tooClose = true; break; }
				}
			}
			if (!tooClose) grassMap[ty][tx] = 2;
		}
	}
}

static void populateMenuGrass() {
	memset(menuGrassMap, 0, sizeof(menuGrassMap));
	for (int ty = 0; ty < MENU_MAP_H - 1; ty++) {
		for (int tx = 0; tx < MENU_MAP_W; tx++) {
			if (menuTilemap[ty][tx] != TILE_EMPTY) continue;
			if (menuTilemap[ty + 1][tx] != TILE_GROUND) continue;
			menuGrassMap[ty][tx] = 1;
		}
	}
	const int MIN_ALT_SPACING = 4;
	for (int ty = 0; ty < MENU_MAP_H - 1; ty++) {
		for (int tx = 0; tx < MENU_MAP_W; tx++) {
			if (menuGrassMap[ty][tx] != 1) continue;
			if (rand() % 10 != 0) continue;
			bool tooClose = false;
			for (int dy = -MIN_ALT_SPACING; dy <= MIN_ALT_SPACING && !tooClose; dy++) {
				for (int dx = -MIN_ALT_SPACING; dx <= MIN_ALT_SPACING; dx++) {
					int nx = tx + dx, ny = ty + dy;
					if (nx < 0 || nx >= MENU_MAP_W || ny < 0 || ny >= MENU_MAP_H) continue;
					if (menuGrassMap[ny][nx] == 2) { tooClose = true; break; }
				}
			}
			if (!tooClose) menuGrassMap[ty][tx] = 2;
		}
	}
}

static bool isOneWay(u8 t) {
	return t == TILE_PLAT_ALT;
}

static bool isSolid(u8 t) {
	return t == TILE_GROUND || t == TILE_FILL ||
	       t == TILE_PLATFORM || t == TILE_PLAT_ALT || t == TILE_CRUMBLE;
}

// Solid but NOT one-way (blocks horizontal + upward)
static bool isSolidFull(u8 t) {
	return t == TILE_GROUND || t == TILE_FILL || t == TILE_PLATFORM || t == TILE_CRUMBLE;
}

// Tile-coordinate-aware versions: 3D tiles become passthrough when slider is extended
static bool isSolidAt(int tx, int ty) {
	u8 t = getTile(tx, ty);
	if (!isSolid(t)) return false;
	if (is3dTile(tx, ty) && tile3dIsPassable(tx, ty)) return false;
	return true;
}

static bool isSolidFullAt(int tx, int ty) {
	u8 t = getTile(tx, ty);
	if (!isSolidFull(t)) return false;
	if (is3dTile(tx, ty) && tile3dIsPassable(tx, ty)) return false;
	return true;
}

static bool isHazard(u8 t) {
	return t == TILE_SPIKE;
}

// ========================================================
// AABB helper
// ========================================================
struct Rect { float x, y, w, h; };

static bool rectsOverlap(const Rect& a, const Rect& b) {
	return a.x < b.x + b.w && a.x + a.w > b.x &&
	       a.y < b.y + b.h && a.y + a.h > b.y;
}

// ========================================================
// Player
// ========================================================
struct Player {
	Rect rect;
	float vx, vy;
	bool onGround;
	bool facingRight;
	bool sprinting;
	bool jumpHeld;
	bool wallLeft, wallRight;
	int  wallJumpLock, wallJumpBlend;
	int  coyoteTimer, jumpBuffer;
	bool groundPound;
	int  gpoundFreeze;
	int  lastWallJumpSide;
	bool dashing;
	int  dashTimer, dashCooldown;
	int score, lives;
	// Polish
	float scaleX, scaleY;
	int   squashTimer;
	int   invulnTimer;
	// Double jump (unlocked in world 2)
	bool doubleJumpUsed;
};

// ========================================================
// Cracker (collectible, separate from tilemap)
// ========================================================
static constexpr int MAX_COINS = 16;
struct Coin {
	Rect rect;
	bool active;
	float bobTimer;
};

// ========================================================
// Enemy (Goomba-style patrol)
// ========================================================
static constexpr int   MAX_ENEMIES   = 16;
static constexpr float ENEMY_SPEED   = 1.2f;
static constexpr float ENEMY_GRAVITY = 0.55f;
static constexpr float ENEMY_W       = 32.0f;
static constexpr float ENEMY_H       = 12.0f;

struct Enemy {
	Rect  rect;
	float vy;
	bool  movingRight;
	bool  active;
	bool  onGround;
	bool  stationary; // true if on single-tile platform (no patrol)
	int   animTimer;
};

static Enemy enemies[MAX_ENEMIES];
static int   numEnemies = 0;

// ========================================================
// VFX particles (dynamic, non-grid effects)
// ========================================================
static constexpr int MAX_VFX = 8;
struct VfxParticle {
	float x, y;        // world position
	int   imgId;       // VFX_WALL_JUMP or VFX_DASH
	int   timer;       // frames remaining
	bool  flipX;       // mirror horizontally
	float alpha;       // fade out
	float scale;       // render scale (1.0 = normal)
	bool  active;
};
static VfxParticle vfx[MAX_VFX];

static void spawnVfx(int imgId, float x, float y, bool flipX, int duration, float scale = 1.0f) {
	int slot = -1;
	for (int i = 0; i < MAX_VFX; i++) {
		if (!vfx[i].active) { slot = i; break; }
	}
	if (slot < 0) slot = 0;
	vfx[slot] = { x, y, imgId, duration, flipX, 1.0f, scale, true };
}

static void updateVfx() {
	for (int i = 0; i < MAX_VFX; i++) {
		if (!vfx[i].active) continue;
		vfx[i].timer--;
		vfx[i].alpha = (float)vfx[i].timer / 12.0f;
		if (vfx[i].alpha < 0.0f) vfx[i].alpha = 0.0f;
		if (vfx[i].timer <= 0) vfx[i].active = false;
	}
}

// ========================================================
// Camera
// ========================================================
struct Camera { float x, y, lookAheadX, targetY; };

// ========================================================
// Game state
// ========================================================
enum GameState { STATE_PLAYING, STATE_GAMEOVER, STATE_WIN, STATE_LEVEL_SELECT, STATE_PAUSED, STATE_DYING, STATE_MAIN_MENU, STATE_SETTINGS, STATE_DEV_SETTINGS, STATE_DIALOGUE_PRE, STATE_DIALOGUE_POST };

static Player    player;

static bool tile3dIsPassable(int tx, int ty) {
	if (!is3dTile(tx, ty)) return false;
	if (g_3dSlider > SLIDER_PASSTHROUGH_THRESHOLD) return true;
	if (player.dashing) return true;
	return false;
}

static float smoothstep01(float t) {
	if (t <= 0.0f) return 0.0f;
	if (t >= 1.0f) return 1.0f;
	return t * t * (3.0f - 2.0f * t);
}

static float distToTileSurface(int tx, int ty) {
	float px = player.rect.x + player.rect.w * 0.5f;
	float py = player.rect.y + player.rect.h * 0.5f;
	float tileL = tx * (float)TILE_SIZE;
	float tileT = ty * (float)TILE_SIZE;
	float tileR = tileL + TILE_SIZE;
	float tileB = tileT + TILE_SIZE;
	float closestX = px;
	if (closestX < tileL) closestX = tileL;
	else if (closestX > tileR) closestX = tileR;
	float closestY = py;
	if (closestY < tileT) closestY = tileT;
	else if (closestY > tileB) closestY = tileB;
	float dx = px - closestX;
	float dy = py - closestY;
	return sqrtf(dx * dx + dy * dy);
}

static float tile3dProximityFade(int tx, int ty) {
	float dist = distToTileSurface(tx, ty);
	if (dist >= TILE3D_FADE_START) return 0.0f;
	if (dist <= TILE3D_FADE_END) return 1.0f;
	float t = 1.0f - (dist - TILE3D_FADE_END) / (TILE3D_FADE_START - TILE3D_FADE_END);
	return smoothstep01(t);
}

static u8 tile3dDrawAlpha(int tx, int ty) {
	if (!is3dTile(tx, ty)) return 255;

	if (tile3dIsPassable(tx, ty)) {
		if (g_3dSlider > SLIDER_PASSTHROUGH_THRESHOLD) {
			float t = (g_3dSlider - SLIDER_PASSTHROUGH_THRESHOLD)
				/ (1.0f - SLIDER_PASSTHROUGH_THRESHOLD);
			t = smoothstep01(t);
			return (u8)(255.0f - t * 120.0f);
		}
		return 185;
	}

	float fade = tile3dProximityFade(tx, ty);
	if (fade <= 0.0f) return 255;

	if (g_3dSlider > 0.05f) {
		float sliderT = smoothstep01(g_3dSlider / SLIDER_PASSTHROUGH_THRESHOLD);
		fade *= 1.0f - sliderT * 0.65f;
	}

	return (u8)(255.0f - fade * (255.0f - (float)TILE3D_MIN_ALPHA));
}

static Coin      coins[MAX_COINS];
static Camera    camera;
static int       numCoins = 0;
static GameState gameState = STATE_MAIN_MENU;
static int       currentLevel = 0;
// Implemented after EDITOR_LEVEL_INFO (see @@EDITOR_LEVELS_END@@).

// Bottom screen
static C2D_Image     bottomMenuImg;
static C2D_Image     menuBtnImg[3]; // 0=Load, 1=New, 2=Settings
static C2D_Image     titleImg;
static C2D_Image     caveBgImg;     // custom background (400x240)
static C2D_Image     caveBg2Img;    // alternate background
static C2D_Image*     activeCaveBg = nullptr; // which BG is currently in use
static C2D_TextBuf   textBuf;

static float spawnX = 48.0f;
static float spawnY = 164.0f;

static float getTimerSeconds();

static bool isCheckpointActivatedAt(int tx, int ty) {
	for (int i = 0; i < numCheckpoints; i++) {
		if (checkpoints[i].tx == tx && checkpoints[i].ty == ty)
			return checkpoints[i].activated;
	}
	return sessionCpActivated[currentLevel][ty][tx];
}

static void clearSessionCheckpoints() {
	memset(sessionCpActivated, 0, sizeof(sessionCpActivated));
	for (int i = 0; i < TOTAL_LEVELS; i++) {
		sessionLastCpIdx[i] = -1;
		sessionLevelTimer[i] = -1.0f;
	}
	sessionCrackersCommitted = 0;
}

static void buildCheckpointsFromMap() {
	numCheckpoints = 0;
	lastCheckpointIdx = -1;
	levelHasCheckpoints = false;
	int maxH = getActiveMapHeight();
	int maxW = getActiveMapWidth();
	for (int ty = 0; ty < maxH && ty < MAP_H; ty++) {
		for (int tx = 0; tx < maxW && tx < MAP_W; tx++) {
			if (!checkpointMap[ty][tx]) continue;
			if (numCheckpoints >= MAX_CHECKPOINTS) continue;
			checkpoints[numCheckpoints].tx = (short)tx;
			checkpoints[numCheckpoints].ty = (short)ty;
			checkpoints[numCheckpoints].activated =
				sessionCpActivated[currentLevel][ty][tx];
			numCheckpoints++;
		}
	}
	levelHasCheckpoints = numCheckpoints > 0;
	if (levelHasCheckpoints) {
		int saved = sessionLastCpIdx[currentLevel];
		if (saved >= 0 && saved < numCheckpoints)
			lastCheckpointIdx = saved;
	}
}

static void checkpointSpawnPos(int idx, float* outX, float* outY) {
	Checkpoint& cp = checkpoints[idx];
	*outX = cp.tx * TILE_SIZE + (TILE_SIZE - CP_WIDTH) * 0.5f + (CP_WIDTH - 16.0f) * 0.5f;
	*outY = cp.ty * TILE_SIZE + CP_HEIGHT - 28.0f;
}

static void activateCheckpoint(int idx) {
	if (idx < 0 || idx >= numCheckpoints) return;
	Checkpoint& cp = checkpoints[idx];
	if (cp.activated) return;
	float t = getTimerSeconds();
	sessionLevelTimer[currentLevel] = t;
	cp.activated = true;
	sessionCpActivated[currentLevel][cp.ty][cp.tx] = true;
	lastCheckpointIdx = idx;
	sessionLastCpIdx[currentLevel] = idx;
}

static void updateCheckpoints() {
	if (!levelHasCheckpoints) return;
	for (int i = 0; i < numCheckpoints; i++) {
		Checkpoint& cp = checkpoints[i];
		if (cp.activated) continue;
		Rect cpRect = {
			(float)cp.tx * TILE_SIZE + (TILE_SIZE - CP_WIDTH) * 0.5f,
			(float)cp.ty * TILE_SIZE,
			CP_WIDTH, CP_HEIGHT
		};
		if (rectsOverlap(player.rect, cpRect))
			activateCheckpoint(i);
	}
}

static void drawCheckpoints(float cx, float cy) {
	if (!levelHasCheckpoints) return;
	int startCol = (int)(cx / TILE_SIZE);
	int endCol   = (int)((cx + TOP_WIDTH) / TILE_SIZE) + 1;
	int startRow = (int)(cy / TILE_SIZE);
	int endRow   = (int)((cy + TOP_HEIGHT) / TILE_SIZE) + 1;
	if (startCol < 0) startCol = 0;
	if (startRow < 0) startRow = 0;
	int activeW = getActiveMapWidth();
	int activeH = getActiveMapHeight();
	if (endCol >= activeW) endCol = activeW - 1;
	if (endRow >= activeH) endRow = activeH - 1;
	if (endCol >= MAP_W) endCol = MAP_W - 1;
	if (endRow >= MAP_H) endRow = MAP_H - 1;
	for (int ty = startRow; ty <= endRow; ty++) {
		for (int tx = startCol; tx <= endCol; tx++) {
			if (!checkpointMap[ty][tx]) continue;
			float drawX = tx * TILE_SIZE + (TILE_SIZE - CP_WIDTH) * 0.5f - cx;
			float drawY = ty * TILE_SIZE - cy;
			bool active = isCheckpointActivatedAt(tx, ty);
			u32 clr = active
				? C2D_Color32(40, 200, 80, 255)
				: C2D_Color32(220, 40, 40, 255);
			C2D_DrawRectSolid(drawX, drawY, 0.15f, CP_WIDTH, CP_HEIGHT, clr);
		}
	}
}

// Screen shake
static float shakeX = 0.0f, shakeY = 0.0f;
static int   shakeTimer = 0;
static float shakeMag = 0.0f;

// Landing impact flash
static int landingFlashTimer = 0;
static float landingFlashX = 0, landingFlashY = 0;

// Light shafts (cave atmosphere)
static constexpr int MAX_LIGHT_SHAFTS = 3;
struct LightShaft {
	float worldX;     // world-space X position (center of shaft at top)
	float width;      // width at top
	float widthBot;   // width at bottom (slightly wider = diverging)
	float alpha;      // base alpha (0..1)
	float speed;      // sway speed multiplier
	float phase;      // sway phase offset
	float angle;      // lean angle in pixels (positive = leans right)
};
static LightShaft lightShafts[MAX_LIGHT_SHAFTS];
static int numLightShafts = 0;

static void generateLightShafts() {
	numLightShafts = MAX_LIGHT_SHAFTS;
	// Hand-tuned positions: left, center-right, far-right with different angles
	float positions[3] = { 80.0f, 220.0f, 340.0f };
	float angles[3]    = { -20.0f, 10.0f, -8.0f };
	float widths[3]    = { 35.0f, 45.0f, 30.0f };
	for (int i = 0; i < numLightShafts; i++) {
		LightShaft& s = lightShafts[i];
		s.worldX = positions[i] + (float)(rand() % 20 - 10);
		s.width = widths[i];
		s.widthBot = s.width * 1.3f;
		s.alpha = 0.12f + (float)(rand() % 6) / 100.0f;
		s.speed = 0.2f + (float)(rand() % 20) / 100.0f;
		s.phase = (float)(rand() % 628) / 100.0f;
		s.angle = angles[i];
	}
}

static void triggerShake(float mag, int frames) {
	shakeMag = mag;
	shakeTimer = frames;
}

static void updateShake() {
	if (shakeTimer > 0) {
		shakeTimer--;
		float t = (float)shakeTimer / 10.0f;
		shakeX = (float)((rand() % 200 - 100) / 100.0f) * shakeMag * t;
		shakeY = (float)((rand() % 200 - 100) / 100.0f) * shakeMag * t;
	} else {
		shakeX = 0; shakeY = 0;
	}
}

// Fade transition
static int  fadeTimer = 0;
static bool fadingOut = false;
static int  fadeAction = 0; // 0=none, 1=respawn, 2=next level, 3=restart

static void startFade(bool out, int action) {
	fadingOut = out;
	fadeTimer = FADE_FRAMES;
	fadeAction = action;
}

// Speedrun timer
static u64  timerStart = 0;
static u64  timerPaused = 0;
static u64  pauseBegin = 0;
static bool timerRunning = false;
static float timerFinalTime = 0.0f;

static float getTimerSeconds() {
	if (!timerRunning) return timerFinalTime;
	u64 now = svcGetSystemTick();
	u64 paused = timerPaused;
	if (gameState == STATE_PAUSED) paused += now - pauseBegin;
	u64 elapsed = now - timerStart - paused;
	return (float)elapsed / (float)SYSCLOCK_ARM11;
}

static void setTimerElapsedSeconds(float secs) {
	if (secs < 0.0f) secs = 0.0f;
	timerPaused = 0;
	u64 ticks = (u64)(secs * (float)SYSCLOCK_ARM11);
	timerStart = svcGetSystemTick() - ticks;
	timerRunning = true;
	timerFinalTime = 0.0f;
}

static void restoreCheckpointTimer() {
	if (!levelHasCheckpoints || lastCheckpointIdx < 0) return;
	float t = sessionLevelTimer[currentLevel];
	if (t >= 0.0f) setTimerElapsedSeconds(t);
}

static void stopTimer() {
	timerFinalTime = getTimerSeconds();
	timerRunning = false;
}

static void triggerLevelComplete() {
	stopTimer();
	if (!devMode)
		sessionCrackersCommitted = player.score;
	if (levelAllowsDialogue && dialoguePostCount[currentLevel] > 0 &&
	    strlen(dialoguePost[currentLevel][0]) > 0) {
		gameState = STATE_DIALOGUE_POST;
		currentDialogueBox = 0;
		currentDialogueChar = 0;
		dialogueTypewriterTimer = 0;
		dialoguePauseTimer = 0;
		dialogueWaitingForInput = false;
	} else {
		gameState = STATE_WIN;
	}
}

// Death animation
static int   deathTimer = 0;
static float deathVy = 0.0f;
static float deathX = 0.0f, deathY = 0.0f;

// Dust particles
static constexpr int MAX_DUST = 16;
struct DustParticle {
	float x, y, vx, vy, size;
	int   timer;
	bool  active;
};
static DustParticle dust[MAX_DUST];

static void spawnDust(float x, float y, float vxBase, int count) {
	for (int n = 0; n < count; n++) {
		int slot = -1;
		for (int i = 0; i < MAX_DUST; i++)
			if (!dust[i].active) { slot = i; break; }
		if (slot < 0) slot = rand() % MAX_DUST;
		dust[slot] = {
			x, y,
			vxBase + (rand() % 100 - 50) / 30.0f,
			-(rand() % 100) / 40.0f - 0.5f,
			2.0f + (rand() % 20) / 10.0f,
			8 + rand() % 6,
			true
		};
	}
}

static void updateDust() {
	for (int i = 0; i < MAX_DUST; i++) {
		if (!dust[i].active) continue;
		dust[i].x += dust[i].vx;
		dust[i].y += dust[i].vy;
		dust[i].vy += 0.1f;
		dust[i].timer--;
		if (dust[i].timer <= 0) dust[i].active = false;
	}
}

// Level select
static int levelSelectCursor = 0;
static float levelSelectScroll = 0.0f;
static bool levelUnlocked[TOTAL_LEVELS] = { true, false, false, false, false, false, false, false, false, false, false };
static float bestTimes[TOTAL_LEVELS] = { 0 };

// Main menu — cursor: 0=Load Game, 1=New Game, 2=Settings (left to right)
static int mainMenuCursor = 1;

// Settings menu
static int settingsCursor = 0; // 0=Music, 1=SFX, 2=Sprint, 3=Controls, 4=Developer, 5=Back
static GameState settingsReturnState = STATE_MAIN_MENU;
static constexpr int SETTINGS_ITEM_COUNT = 6;

// Pause menu
static int pauseCursor = 0; // 0=Resume, 1=Settings, 2=Quit
static constexpr int PAUSE_ITEM_COUNT = 3;

// Developer settings sub-menu
static int devSettingsCursor = 0; // 0=Level Select, 1=Reset Save, 2=Back
static constexpr int DEV_SETTINGS_ITEM_COUNT = 3;
static bool devResetConfirm = false; // true = showing confirmation prompt
static int slotCursor = 0;
static bool inSlotSelect = false;
static bool slotSelectIsLoad = false; // true=Load, false=New Game
static int currentSlot = 0;

// Save/Load — 3 slots
static const char* SAVE_PATHS[3] = {
	"/3ds/platformer_slot1.dat",
	"/3ds/platformer_slot2.dat",
	"/3ds/platformer_slot3.dat",
};
static constexpr u32 SAVE_MAGIC = 0x504C4157; // v3 — dynamic level count + stars
static constexpr u32 SAVE_MAGIC_V2 = 0x504C4156; // v2 — dynamic level count
static constexpr u32 SAVE_MAGIC_V1 = 0x504C4154; // old fixed format
static constexpr int MAX_SAVE_LEVELS = 32;

static bool slotHasData[3] = { false, false, false };
static int  slotLevelCount[3] = { 0, 0, 0 }; // how many levels in each slot's save
static int  slotProgress[3] = { 0, 0, 0 };   // how many levels unlocked in each slot

static void saveProgress() {
	FILE* f = fopen(SAVE_PATHS[currentSlot], "wb");
	if (!f) return;
	fwrite(&SAVE_MAGIC, sizeof(u32), 1, f);
	int n = TOTAL_LEVELS;
	fwrite(&n, sizeof(int), 1, f);
	fwrite(levelUnlocked, sizeof(bool), n, f);
	fwrite(bestTimes, sizeof(float), n, f);
	fwrite(starRatings, sizeof(int), n, f);
	fclose(f);
	slotHasData[currentSlot] = true;
	slotLevelCount[currentSlot] = n;
}

static void loadProgress() {
	FILE* f = fopen(SAVE_PATHS[currentSlot], "rb");
	if (!f) return;

	u32 magic = 0;
	fread(&magic, sizeof(u32), 1, f);

	if (magic == SAVE_MAGIC || magic == SAVE_MAGIC_V2) {
		// v2/v3 dynamic format
		int savedN = 0;
		fread(&savedN, sizeof(int), 1, f);
		if (savedN < 1 || savedN > MAX_SAVE_LEVELS) { fclose(f); return; }
		bool tmpUnlocked[MAX_SAVE_LEVELS];
		float tmpTimes[MAX_SAVE_LEVELS];
		int tmpStars[MAX_SAVE_LEVELS] = { 0 };
		fread(tmpUnlocked, sizeof(bool), savedN, f);
		fread(tmpTimes, sizeof(float), savedN, f);
		if (magic == SAVE_MAGIC) {
			fread(tmpStars, sizeof(int), savedN, f);
		}
		// Copy what fits, new levels default to locked / 0 time / 0 stars
		for (int i = 0; i < TOTAL_LEVELS; i++) {
			if (i < savedN) {
				levelUnlocked[i] = tmpUnlocked[i];
				bestTimes[i] = tmpTimes[i];
				starRatings[i] = tmpStars[i];
			} else {
				levelUnlocked[i] = false;
				bestTimes[i] = 0.0f;
				starRatings[i] = 0;
			}
		}
		levelUnlocked[0] = true; // always unlock first level
	}
	fclose(f);
}

static void resetProgress() {
	clearSessionCheckpoints();
	for (int i = 0; i < TOTAL_LEVELS; i++) {
		levelUnlocked[i] = (i == 0);
		bestTimes[i] = 0.0f;
		starRatings[i] = 0;
	}
}

static void probeSlots() {
	for (int s = 0; s < 3; s++) {
		slotHasData[s] = false;
		slotLevelCount[s] = 0;
		slotProgress[s] = 0;
		FILE* f = fopen(SAVE_PATHS[s], "rb");
		if (!f) continue;
		u32 magic = 0;
		fread(&magic, sizeof(u32), 1, f);
		if (magic == SAVE_MAGIC || magic == SAVE_MAGIC_V2) {
			int n = 0;
			fread(&n, sizeof(int), 1, f);
			if (n >= 1 && n <= MAX_SAVE_LEVELS) {
				slotHasData[s] = true;
				slotLevelCount[s] = n;
				// Read unlocked array to count progress
				bool tmpUnlocked[MAX_SAVE_LEVELS];
				fread(tmpUnlocked, sizeof(bool), n, f);
				// Count levels beaten by checking star ratings
				int total = (n < TOTAL_LEVELS) ? n : TOTAL_LEVELS;
				// Read past bestTimes to reach starRatings in the file
				float tmpTimes2[MAX_SAVE_LEVELS];
				fread(tmpTimes2, sizeof(float), n, f);
				int tmpStars2[MAX_SAVE_LEVELS] = { 0 };
				if (magic == SAVE_MAGIC) {
					fread(tmpStars2, sizeof(int), n, f);
				}
				int beaten = 0;
				for (int i = 0; i < total; i++) {
					if (tmpStars2[i] > 0) beaten++;
				}
				slotProgress[s] = (beaten * 100) / TOTAL_LEVELS;
			}
		}
		fclose(f);
	}
}

// ========================================================
// Build level tilemaps + place crackers
// ========================================================
static void spawnEnemy(float x, float y) {
	if (numEnemies >= MAX_ENEMIES) return;
	Enemy& e = enemies[numEnemies++];
	e.rect = { x, y, ENEMY_W, ENEMY_H };
	e.vy = 0.0f;
	e.movingRight = false;
	e.active = true;
	e.onGround = false;
	e.stationary = false;
	e.animTimer = 0;
}

// @@EDITOR_LEVELS_START@@



// Level metadata — auto-generated from editor

static const int EDITOR_LEVEL_COUNT = 11;

#undef LEVEL_COUNT_MACRO

#define LEVEL_COUNT_MACRO EDITOR_LEVEL_COUNT

static const LevelInfo EDITOR_LEVEL_INFO[EDITOR_LEVEL_COUNT] = {

	/* 0 */ {"1-1", 1, false, 60.0f, 80, 16},

	/* 1 */ {"1-2", 1, false, 60.0f, 80, 16},

	/* 2 */ {"1-3", 1, false, 60.0f, 80, 16},

	/* 3 */ {"1-4", 1, false, 60.0f, 80, 16},

	/* 4 */ {"1-5", 1, false, 60.0f, 80, 16},

	/* 5 */ {"1-6", 1, false, 60.0f, 80, 16},

	/* 6 */ {"1-7", 1, false, 60.0f, 80, 32},

	/* 7 */ {"1-8", 1, false, 60.0f, 80, 32},

	/* 8 */ {"1-9", 1, false, 60.0f, 80, 32},

	/* 9 */ {"1-10", 1, false, 60.0f, 80, 32},

	/* 10 */ {"1-11", 1, true, 60.0f, 32, 120},

};

#undef LEVEL_INFO_MACRO

#define LEVEL_INFO_MACRO EDITOR_LEVEL_INFO

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

// Menu scene — autogenerated from the editor.
static void buildMenuScene() {
	memset(menuTilemap, TILE_EMPTY, sizeof(menuTilemap));
	for (int x = 0; x < MENU_MAP_W; x++) {
		menuTilemap[13][x] = TILE_GROUND;
		menuTilemap[14][x] = TILE_FILL;
		menuTilemap[15][x] = TILE_FILL;
	}
	menuRunnerStartX = 2 * TILE_SIZE;
	menuRunnerStartY = 13 * TILE_SIZE - 28;
}

// Level 0: "1-1", World 1
static void buildLevel_1() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 0
	tilemap[0][3] = TILE_FILL;
	// Row 1
	tilemap[1][3] = TILE_FILL;
	// Row 2
	tilemap[2][3] = TILE_FILL;
	// Row 3
	tilemap[3][3] = TILE_FILL;
	// Row 4
	tilemap[4][3] = TILE_FILL;
	// Row 5
	tilemap[5][3] = TILE_FILL;
	// Row 6
	tilemap[6][3] = TILE_FILL;
	for (int x = 29; x <= 30; x++)
		tilemap[6][x] = TILE_PLATFORM;
	// Row 7
	tilemap[7][3] = TILE_FILL;
	// Row 8
	tilemap[8][3] = TILE_FILL;
	for (int x = 70; x <= 71; x++)
		tilemap[8][x] = TILE_GROUND;
	for (int x = 76; x <= 79; x++)
		tilemap[8][x] = TILE_PLATFORM;
	// Row 9
	tilemap[9][3] = TILE_FILL;
	for (int x = 28; x <= 31; x++)
		tilemap[9][x] = TILE_PLATFORM;
	tilemap[9][69] = TILE_GROUND;
	for (int x = 70; x <= 71; x++)
		tilemap[9][x] = TILE_FILL;
	// Row 10
	tilemap[10][3] = TILE_FILL;
	tilemap[10][68] = TILE_GROUND;
	for (int x = 69; x <= 71; x++)
		tilemap[10][x] = TILE_FILL;
	// Row 11
	tilemap[11][3] = TILE_FILL;
	tilemap[11][67] = TILE_GROUND;
	for (int x = 68; x <= 71; x++)
		tilemap[11][x] = TILE_FILL;
	// Row 12
	for (int x = 12; x <= 14; x++)
		tilemap[12][x] = TILE_PLATFORM;
	for (int x = 25; x <= 34; x++)
		tilemap[12][x] = TILE_GROUND;
	for (int x = 51; x <= 52; x++)
		tilemap[12][x] = TILE_GROUND;
	tilemap[12][66] = TILE_GROUND;
	for (int x = 67; x <= 71; x++)
		tilemap[12][x] = TILE_FILL;
	// Row 13
	for (int x = 23; x <= 24; x++)
		tilemap[13][x] = TILE_GROUND;
	for (int x = 25; x <= 34; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 35; x <= 36; x++)
		tilemap[13][x] = TILE_GROUND;
	for (int x = 51; x <= 52; x++)
		tilemap[13][x] = TILE_FILL;
	tilemap[13][65] = TILE_GROUND;
	for (int x = 66; x <= 71; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 72; x <= 79; x++)
		tilemap[13][x] = TILE_SPIKE;
	// Row 14
	for (int x = 0; x <= 22; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 23; x <= 36; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 37; x <= 47; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 51; x <= 52; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 53; x <= 64; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 65; x <= 71; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 72; x <= 79; x++)
		tilemap[14][x] = TILE_GROUND;
	// Row 15
	for (int x = 0; x <= 47; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 48; x <= 50; x++)
		tilemap[15][x] = TILE_SPIKE;
	for (int x = 51; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 425.0f, 361.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 937.0f, 169.0f, 14, 14 }, true, 0.3f };

	// Enemies
	spawnEnemy(516.0f, 424.0f);
	spawnEnemy(964.0f, 360.0f);
	spawnEnemy(1668.0f, 360.0f);

	// Player spawn
	spawnX = 43.2f;
	spawnY = 36.2f;
}

// Level 1: "1-2", World 1
static void buildLevel_2() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 0
	tilemap[0][4] = TILE_PLATFORM;
	// Row 1
	tilemap[1][4] = TILE_PLATFORM;
	// Row 2
	tilemap[2][4] = TILE_PLATFORM;
	// Row 3
	tilemap[3][4] = TILE_PLATFORM;
	// Row 4
	tilemap[4][4] = TILE_PLATFORM;
	for (int x = 11; x <= 13; x++)
		tilemap[4][x] = TILE_PLATFORM;
	for (int x = 19; x <= 22; x++)
		tilemap[4][x] = TILE_GROUND;
	// Row 5
	tilemap[5][4] = TILE_PLATFORM;
	for (int x = 19; x <= 22; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 23; x <= 25; x++)
		tilemap[5][x] = TILE_GROUND;
	// Row 6
	tilemap[6][4] = TILE_PLATFORM;
	for (int x = 7; x <= 18; x++)
		tilemap[6][x] = TILE_PLATFORM;
	for (int x = 19; x <= 25; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 26; x <= 28; x++)
		tilemap[6][x] = TILE_GROUND;
	// Row 7
	tilemap[7][4] = TILE_PLATFORM;
	tilemap[7][7] = TILE_PLATFORM;
	for (int x = 19; x <= 28; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 29; x <= 31; x++)
		tilemap[7][x] = TILE_GROUND;
	// Row 8
	tilemap[8][4] = TILE_PLATFORM;
	for (int x = 19; x <= 31; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 32; x <= 34; x++)
		tilemap[8][x] = TILE_GROUND;
	// Row 9
	tilemap[9][4] = TILE_PLATFORM;
	for (int x = 19; x <= 34; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 35; x <= 37; x++)
		tilemap[9][x] = TILE_GROUND;
	for (int x = 64; x <= 65; x++)
		tilemap[9][x] = TILE_PLATFORM;
	// Row 10
	for (int x = 6; x <= 9; x++)
		tilemap[10][x] = TILE_PLATFORM;
	tilemap[10][19] = TILE_FILL;
	for (int x = 26; x <= 37; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 38; x <= 40; x++)
		tilemap[10][x] = TILE_GROUND;
	// Row 11
	for (int x = 12; x <= 15; x++)
		tilemap[11][x] = TILE_PLATFORM;
	tilemap[11][19] = TILE_FILL;
	for (int x = 26; x <= 40; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 41; x <= 43; x++)
		tilemap[11][x] = TILE_GROUND;
	// Row 12
	tilemap[12][19] = TILE_FILL;
	for (int x = 23; x <= 43; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 44; x <= 46; x++)
		tilemap[12][x] = TILE_GROUND;
	for (int x = 58; x <= 60; x++)
		tilemap[12][x] = TILE_PLATFORM;
	for (int x = 63; x <= 66; x++)
		tilemap[12][x] = TILE_PLATFORM;
	for (int x = 69; x <= 71; x++)
		tilemap[12][x] = TILE_PLATFORM;
	// Row 13
	tilemap[13][19] = TILE_FILL;
	// Row 14
	for (int x = 0; x <= 18; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][19] = TILE_FILL;
	for (int x = 20; x <= 57; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 72; x <= 79; x++)
		tilemap[14][x] = TILE_GROUND;
	// Row 15
	for (int x = 0; x <= 57; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 58; x <= 71; x++)
		tilemap[15][x] = TILE_SPIKE;
	for (int x = 72; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 393.0f, 105.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 777.0f, 361.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 745.0f, 329.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 777.0f, 329.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 809.0f, 329.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 809.0f, 361.0f, 14, 14 }, true, 1.5f };
	coins[numCoins++] = { { 745.0f, 361.0f, 14, 14 }, true, 1.8f };
	coins[numCoins++] = { { 2057.0f, 265.0f, 14, 14 }, true, 2.1f };

	// Enemies
	spawnEnemy(484.0f, 424.0f);
	spawnEnemy(676.0f, 424.0f);
	spawnEnemy(2084.0f, 360.0f);

	// Player spawn
	spawnX = 48.0f;
	spawnY = 420.0f;
}

// Level 2: "1-3", World 1
static void buildLevel_3() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 2
	for (int x = 29; x <= 32; x++)
		tilemap[2][x] = TILE_PLATFORM;
	// Row 3
	tilemap[3][29] = TILE_PLATFORM;
	// Row 4
	for (int x = 23; x <= 25; x++)
		tilemap[4][x] = TILE_PLATFORM;
	tilemap[4][29] = TILE_PLATFORM;
	tilemap[4][48] = TILE_PLATFORM;
	// Row 5
	tilemap[5][29] = TILE_PLATFORM;
	tilemap[5][48] = TILE_PLATFORM;
	// Row 6
	tilemap[6][29] = TILE_PLATFORM;
	for (int x = 41; x <= 44; x++)
		tilemap[6][x] = TILE_PLATFORM;
	tilemap[6][48] = TILE_PLATFORM;
	// Row 7
	tilemap[7][19] = TILE_PLATFORM;
	tilemap[7][48] = TILE_PLATFORM;
	// Row 8
	tilemap[8][19] = TILE_PLATFORM;
	for (int x = 25; x <= 27; x++)
		tilemap[8][x] = TILE_PLATFORM;
	tilemap[8][48] = TILE_PLATFORM;
	for (int x = 73; x <= 75; x++)
		tilemap[8][x] = TILE_PLATFORM;
	tilemap[8][78] = TILE_GROUND;
	// Row 9
	tilemap[9][19] = TILE_PLATFORM;
	tilemap[9][48] = TILE_PLATFORM;
	tilemap[9][78] = TILE_FILL;
	// Row 10
	for (int x = 13; x <= 16; x++)
		tilemap[10][x] = TILE_PLATFORM;
	for (int x = 21; x <= 22; x++)
		tilemap[10][x] = TILE_PLATFORM;
	tilemap[10][48] = TILE_PLATFORM;
	tilemap[10][70] = TILE_GROUND;
	tilemap[10][78] = TILE_FILL;
	// Row 11
	tilemap[11][70] = TILE_FILL;
	tilemap[11][78] = TILE_FILL;
	// Row 12
	for (int x = 7; x <= 9; x++)
		tilemap[12][x] = TILE_PLATFORM;
	tilemap[12][67] = TILE_GROUND;
	tilemap[12][70] = TILE_FILL;
	tilemap[12][78] = TILE_FILL;
	// Row 13
	tilemap[13][67] = TILE_FILL;
	tilemap[13][70] = TILE_FILL;
	tilemap[13][78] = TILE_FILL;
	// Row 14
	for (int x = 0; x <= 12; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 50; x <= 54; x++)
		tilemap[14][x] = TILE_PLATFORM;
	for (int x = 58; x <= 66; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][67] = TILE_FILL;
	for (int x = 68; x <= 69; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][70] = TILE_FILL;
	for (int x = 71; x <= 77; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][78] = TILE_FILL;
	tilemap[14][79] = TILE_GROUND;
	// Row 15
	for (int x = 0; x <= 12; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 13; x <= 57; x++)
		tilemap[15][x] = TILE_SPIKE;
	for (int x = 58; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 489.0f, 297.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 617.0f, 201.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 1513.0f, 329.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 1353.0f, 169.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 2377.0f, 233.0f, 14, 14 }, true, 1.2f };

	// Enemies
	spawnEnemy(356.0f, 424.0f);
	spawnEnemy(2020.0f, 424.0f);

	// Player spawn
	spawnX = 48.0f;
	spawnY = 420.0f;
}

// Level 3: "1-4", World 1
static void buildLevel_4() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 0
	for (int x = 0; x <= 79; x++)
		tilemap[0][x] = TILE_FILL;
	// Row 1
	for (int x = 0; x <= 79; x++)
		tilemap[1][x] = TILE_FILL;
	// Row 2
	for (int x = 0; x <= 1; x++)
		tilemap[2][x] = TILE_FILL;
	for (int x = 5; x <= 53; x++)
		tilemap[2][x] = TILE_FILL;
	// Row 3
	for (int x = 0; x <= 1; x++)
		tilemap[3][x] = TILE_FILL;
	for (int x = 22; x <= 26; x++)
		tilemap[3][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[3][x] = TILE_FILL;
	// Row 4
	for (int x = 0; x <= 10; x++)
		tilemap[4][x] = TILE_FILL;
	for (int x = 22; x <= 26; x++)
		tilemap[4][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[4][x] = TILE_FILL;
	for (int x = 57; x <= 64; x++)
		tilemap[4][x] = TILE_PLATFORM;
	// Row 5
	for (int x = 0; x <= 5; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 22; x <= 26; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 69; x <= 79; x++)
		tilemap[5][x] = TILE_GROUND;
	// Row 6
	for (int x = 0; x <= 5; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 14; x <= 15; x++)
		tilemap[6][x] = TILE_GROUND;
	for (int x = 22; x <= 26; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 30; x <= 48; x++)
		tilemap[6][x] = TILE_GROUND;
	for (int x = 52; x <= 53; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[6][x] = TILE_FILL;
	// Row 7
	for (int x = 0; x <= 5; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 14; x <= 15; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 18; x <= 20; x++)
		tilemap[7][x] = TILE_PLATFORM;
	for (int x = 22; x <= 26; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 30; x <= 48; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 71; x <= 79; x++)
		tilemap[7][x] = TILE_FILL;
	// Row 8
	for (int x = 0; x <= 5; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 9; x <= 13; x++)
		tilemap[8][x] = TILE_GROUND;
	for (int x = 14; x <= 15; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 22; x <= 26; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 30; x <= 31; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 54; x <= 56; x++)
		tilemap[8][x] = TILE_GROUND;
	for (int x = 72; x <= 79; x++)
		tilemap[8][x] = TILE_FILL;
	// Row 9
	for (int x = 0; x <= 5; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 14; x <= 15; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 22; x <= 26; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 30; x <= 31; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 58; x <= 60; x++)
		tilemap[9][x] = TILE_PLATFORM;
	for (int x = 72; x <= 79; x++)
		tilemap[9][x] = TILE_FILL;
	// Row 10
	for (int x = 0; x <= 5; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 14; x <= 15; x++)
		tilemap[10][x] = TILE_FILL;
	tilemap[10][26] = TILE_FILL;
	for (int x = 30; x <= 31; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 35; x <= 37; x++)
		tilemap[10][x] = TILE_PLATFORM;
	for (int x = 41; x <= 43; x++)
		tilemap[10][x] = TILE_PLATFORM;
	for (int x = 52; x <= 53; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 72; x <= 79; x++)
		tilemap[10][x] = TILE_FILL;
	// Row 11
	for (int x = 0; x <= 5; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 6; x <= 10; x++)
		tilemap[11][x] = TILE_GROUND;
	for (int x = 14; x <= 15; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 30; x <= 31; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 52; x <= 53; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 63; x <= 65; x++)
		tilemap[11][x] = TILE_PLATFORM;
	for (int x = 72; x <= 79; x++)
		tilemap[11][x] = TILE_FILL;
	// Row 12
	for (int x = 14; x <= 15; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 19; x <= 22; x++)
		tilemap[12][x] = TILE_PLATFORM;
	for (int x = 30; x <= 31; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 34; x <= 51; x++)
		tilemap[12][x] = TILE_GROUND;
	for (int x = 52; x <= 53; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 72; x <= 79; x++)
		tilemap[12][x] = TILE_FILL;
	// Row 13
	for (int x = 14; x <= 15; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 16; x <= 22; x++)
		tilemap[13][x] = TILE_SPIKE;
	for (int x = 30; x <= 31; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 34; x <= 53; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 58; x <= 60; x++)
		tilemap[13][x] = TILE_PLATFORM;
	for (int x = 72; x <= 79; x++)
		tilemap[13][x] = TILE_FILL;
	// Row 14
	for (int x = 0; x <= 13; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 14; x <= 15; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 16; x <= 29; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 30; x <= 31; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 72; x <= 79; x++)
		tilemap[14][x] = TILE_FILL;
	// Row 15
	for (int x = 0; x <= 31; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 32; x <= 71; x++)
		tilemap[15][x] = TILE_GROUND;
	for (int x = 72; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 105.0f, 105.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 617.0f, 201.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 873.0f, 105.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 1353.0f, 297.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 1161.0f, 297.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 2057.0f, 329.0f, 14, 14 }, true, 1.5f };
	coins[numCoins++] = { { 2249.0f, 137.0f, 14, 14 }, true, 1.8f };

	// Enemies
	spawnEnemy(1284.0f, 168.0f);
	spawnEnemy(1348.0f, 360.0f);
	spawnEnemy(2500.0f, 136.0f);

	// Player spawn
	spawnX = 48.0f;
	spawnY = 420.0f;
}

// Level 4: "1-5", World 1
static void buildLevel_5() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 0
	for (int x = 55; x <= 58; x++)
		tilemap[0][x] = TILE_FILL;
	// Row 1
	tilemap[1][44] = TILE_PLATFORM;
	for (int x = 55; x <= 58; x++)
		tilemap[1][x] = TILE_FILL;
	// Row 2
	tilemap[2][44] = TILE_PLATFORM;
	for (int x = 55; x <= 58; x++)
		tilemap[2][x] = TILE_FILL;
	// Row 3
	tilemap[3][44] = TILE_PLATFORM;
	// Row 4
	for (int x = 14; x <= 16; x++)
		tilemap[4][x] = TILE_PLATFORM;
	for (int x = 19; x <= 21; x++)
		tilemap[4][x] = TILE_PLATFORM;
	for (int x = 24; x <= 26; x++)
		tilemap[4][x] = TILE_PLATFORM;
	for (int x = 39; x <= 41; x++)
		tilemap[4][x] = TILE_PLATFORM;
	// Row 5
	tilemap[5][10] = TILE_PLATFORM;
	for (int x = 55; x <= 58; x++)
		tilemap[5][x] = TILE_FILL;
	// Row 6
	for (int x = 45; x <= 47; x++)
		tilemap[6][x] = TILE_PLATFORM;
	for (int x = 55; x <= 58; x++)
		tilemap[6][x] = TILE_FILL;
	// Row 7
	tilemap[7][36] = TILE_PLATFORM;
	tilemap[7][39] = TILE_PLATFORM;
	for (int x = 55; x <= 58; x++)
		tilemap[7][x] = TILE_FILL;
	tilemap[7][59] = TILE_GROUND;
	// Row 8
	for (int x = 9; x <= 11; x++)
		tilemap[8][x] = TILE_PLATFORM;
	tilemap[8][39] = TILE_PLATFORM;
	tilemap[8][54] = TILE_GROUND;
	for (int x = 55; x <= 59; x++)
		tilemap[8][x] = TILE_FILL;
	tilemap[8][60] = TILE_GROUND;
	// Row 9
	tilemap[9][29] = TILE_FILL;
	tilemap[9][39] = TILE_PLATFORM;
	tilemap[9][53] = TILE_GROUND;
	for (int x = 54; x <= 60; x++)
		tilemap[9][x] = TILE_FILL;
	// Row 10
	tilemap[10][29] = TILE_FILL;
	for (int x = 53; x <= 60; x++)
		tilemap[10][x] = TILE_FILL;
	tilemap[10][61] = TILE_GROUND;
	// Row 11
	for (int x = 8; x <= 12; x++)
		tilemap[11][x] = TILE_PLATFORM;
	tilemap[11][29] = TILE_FILL;
	for (int x = 35; x <= 37; x++)
		tilemap[11][x] = TILE_PLATFORM;
	for (int x = 53; x <= 61; x++)
		tilemap[11][x] = TILE_FILL;
	// Row 12
	for (int x = 4; x <= 6; x++)
		tilemap[12][x] = TILE_PLATFORM;
	for (int x = 14; x <= 16; x++)
		tilemap[12][x] = TILE_PLATFORM;
	tilemap[12][29] = TILE_FILL;
	for (int x = 50; x <= 52; x++)
		tilemap[12][x] = TILE_SPIKE;
	for (int x = 53; x <= 61; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 62; x <= 63; x++)
		tilemap[12][x] = TILE_GROUND;
	// Row 13
	tilemap[13][29] = TILE_FILL;
	for (int x = 50; x <= 52; x++)
		tilemap[13][x] = TILE_GROUND;
	for (int x = 53; x <= 63; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 64; x <= 65; x++)
		tilemap[13][x] = TILE_GROUND;
	// Row 14
	for (int x = 0; x <= 28; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][29] = TILE_FILL;
	for (int x = 30; x <= 49; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 50; x <= 65; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 66; x <= 79; x++)
		tilemap[14][x] = TILE_GROUND;
	// Row 15
	for (int x = 0; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 329.0f, 137.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 169.0f, 361.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 489.0f, 361.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 905.0f, 425.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 1801.0f, 137.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 1833.0f, 137.0f, 14, 14 }, true, 1.5f };

	// Enemies
	spawnEnemy(324.0f, 328.0f);
	spawnEnemy(836.0f, 424.0f);
	spawnEnemy(1316.0f, 424.0f);
	spawnEnemy(2340.0f, 424.0f);

	// Player spawn
	spawnX = 48.0f;
	spawnY = 420.0f;
}

// Level 5: "1-6", World 1
static void buildLevel_6() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 10
	for (int x = 19; x <= 22; x++)
		tilemap[10][x] = TILE_GROUND;
	for (int x = 52; x <= 54; x++)
		tilemap[10][x] = TILE_GROUND;
	// Row 11
	for (int x = 16; x <= 18; x++)
		tilemap[11][x] = TILE_GROUND;
	for (int x = 19; x <= 21; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 52; x <= 54; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 55; x <= 57; x++)
		tilemap[11][x] = TILE_GROUND;
	// Row 12
	for (int x = 14; x <= 15; x++)
		tilemap[12][x] = TILE_GROUND;
	for (int x = 16; x <= 21; x++)
		tilemap[12][x] = TILE_FILL;
	tilemap[12][28] = TILE_PLATFORM;
	tilemap[12][34] = TILE_PLATFORM;
	tilemap[12][40] = TILE_PLATFORM;
	tilemap[12][49] = TILE_PLATFORM;
	for (int x = 52; x <= 57; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 58; x <= 59; x++)
		tilemap[12][x] = TILE_GROUND;
	// Row 13
	for (int x = 12; x <= 13; x++)
		tilemap[13][x] = TILE_GROUND;
	for (int x = 14; x <= 21; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 52; x <= 59; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 60; x <= 61; x++)
		tilemap[13][x] = TILE_GROUND;
	// Row 14
	for (int x = 0; x <= 11; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 12; x <= 21; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 22; x <= 51; x++)
		tilemap[14][x] = TILE_SPIKE;
	for (int x = 52; x <= 61; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 62; x <= 79; x++)
		tilemap[14][x] = TILE_GROUND;
	// Row 15
	for (int x = 0; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 1001.0f, 265.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 1193.0f, 265.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 1385.0f, 265.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 2633.0f, 1705.0f, 14, 14 }, true, 0.9f };

	// Enemies
	spawnEnemy(900.0f, 360.0f);
	spawnEnemy(1092.0f, 360.0f);
	spawnEnemy(1284.0f, 360.0f);
	spawnEnemy(1572.0f, 360.0f);

	// Player spawn
	spawnX = 48.0f;
	spawnY = 420.0f;
}

// Level 6: "1-7", World 1
static void buildLevel_7() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 3
	tilemap[3][58] = TILE_PLATFORM;
	// Row 4
	tilemap[4][58] = TILE_PLATFORM;
	// Row 5
	tilemap[5][58] = TILE_PLATFORM;
	// Row 6
	tilemap[6][58] = TILE_PLATFORM;
	// Row 7
	for (int x = 34; x <= 36; x++)
		tilemap[7][x] = TILE_CRUMBLE;
	for (int x = 39; x <= 41; x++)
		tilemap[7][x] = TILE_CRUMBLE;
	for (int x = 44; x <= 46; x++)
		tilemap[7][x] = TILE_CRUMBLE;
	for (int x = 49; x <= 51; x++)
		tilemap[7][x] = TILE_CRUMBLE;
	tilemap[7][58] = TILE_PLATFORM;
	// Row 8
	for (int x = 29; x <= 31; x++)
		tilemap[8][x] = TILE_PLATFORM;
	tilemap[8][58] = TILE_PLATFORM;
	// Row 9
	tilemap[9][58] = TILE_PLATFORM;
	// Row 10
	for (int x = 25; x <= 27; x++)
		tilemap[10][x] = TILE_PLAT_ALT;
	tilemap[10][58] = TILE_PLATFORM;
	// Row 11
	for (int x = 38; x <= 45; x++)
		tilemap[11][x] = TILE_SPIKE;
	tilemap[11][58] = TILE_PLATFORM;
	// Row 12
	tilemap[12][23] = TILE_PLATFORM;
	for (int x = 36; x <= 37; x++)
		tilemap[12][x] = TILE_SPIKE;
	for (int x = 38; x <= 45; x++)
		tilemap[12][x] = TILE_GROUND;
	for (int x = 46; x <= 47; x++)
		tilemap[12][x] = TILE_SPIKE;
	tilemap[12][58] = TILE_PLATFORM;
	// Row 13
	tilemap[13][23] = TILE_PLATFORM;
	for (int x = 25; x <= 27; x++)
		tilemap[13][x] = TILE_PLAT_ALT;
	for (int x = 34; x <= 35; x++)
		tilemap[13][x] = TILE_SPIKE;
	for (int x = 36; x <= 38; x++)
		tilemap[13][x] = TILE_GROUND;
	for (int x = 39; x <= 44; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 45; x <= 47; x++)
		tilemap[13][x] = TILE_GROUND;
	tilemap[13][48] = TILE_SPIKE;
	tilemap[13][58] = TILE_PLATFORM;
	// Row 14
	tilemap[14][23] = TILE_PLATFORM;
	tilemap[14][33] = TILE_SPIKE;
	for (int x = 34; x <= 36; x++)
		tilemap[14][x] = TILE_GROUND;
	for (int x = 37; x <= 46; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 47; x <= 48; x++)
		tilemap[14][x] = TILE_GROUND;
	tilemap[14][49] = TILE_SPIKE;
	tilemap[14][58] = TILE_PLATFORM;
	// Row 15
	for (int x = 33; x <= 34; x++)
		tilemap[15][x] = TILE_GROUND;
	for (int x = 35; x <= 47; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 48; x <= 49; x++)
		tilemap[15][x] = TILE_GROUND;
	tilemap[15][50] = TILE_SPIKE;
	// Row 16
	tilemap[16][32] = TILE_SPIKE;
	tilemap[16][33] = TILE_GROUND;
	for (int x = 34; x <= 48; x++)
		tilemap[16][x] = TILE_FILL;
	for (int x = 49; x <= 50; x++)
		tilemap[16][x] = TILE_GROUND;
	tilemap[16][51] = TILE_SPIKE;
	// Row 17
	for (int x = 25; x <= 27; x++)
		tilemap[17][x] = TILE_PLATFORM;
	tilemap[17][31] = TILE_SPIKE;
	for (int x = 32; x <= 33; x++)
		tilemap[17][x] = TILE_GROUND;
	for (int x = 34; x <= 49; x++)
		tilemap[17][x] = TILE_FILL;
	for (int x = 50; x <= 51; x++)
		tilemap[17][x] = TILE_GROUND;
	tilemap[17][52] = TILE_SPIKE;
	// Row 18
	for (int x = 18; x <= 19; x++)
		tilemap[18][x] = TILE_MOVING;
	for (int x = 31; x <= 32; x++)
		tilemap[18][x] = TILE_GROUND;
	for (int x = 33; x <= 50; x++)
		tilemap[18][x] = TILE_FILL;
	for (int x = 51; x <= 52; x++)
		tilemap[18][x] = TILE_GROUND;
	for (int x = 53; x <= 54; x++)
		tilemap[18][x] = TILE_SPIKE;
	// Row 19
	tilemap[19][30] = TILE_SPIKE;
	tilemap[19][31] = TILE_GROUND;
	for (int x = 32; x <= 51; x++)
		tilemap[19][x] = TILE_FILL;
	for (int x = 52; x <= 54; x++)
		tilemap[19][x] = TILE_GROUND;
	tilemap[19][55] = TILE_SPIKE;
	for (int x = 60; x <= 63; x++)
		tilemap[19][x] = TILE_PLATFORM;
	// Row 20
	for (int x = 12; x <= 13; x++)
		tilemap[20][x] = TILE_MOVING;
	tilemap[20][29] = TILE_SPIKE;
	for (int x = 30; x <= 31; x++)
		tilemap[20][x] = TILE_GROUND;
	for (int x = 32; x <= 53; x++)
		tilemap[20][x] = TILE_FILL;
	for (int x = 54; x <= 55; x++)
		tilemap[20][x] = TILE_GROUND;
	tilemap[20][56] = TILE_SPIKE;
	// Row 21
	for (int x = 7; x <= 8; x++)
		tilemap[21][x] = TILE_PLAT_ALT;
	for (int x = 29; x <= 30; x++)
		tilemap[21][x] = TILE_GROUND;
	for (int x = 31; x <= 54; x++)
		tilemap[21][x] = TILE_FILL;
	for (int x = 55; x <= 56; x++)
		tilemap[21][x] = TILE_GROUND;
	// Row 22
	tilemap[22][28] = TILE_SPIKE;
	tilemap[22][29] = TILE_GROUND;
	for (int x = 30; x <= 55; x++)
		tilemap[22][x] = TILE_FILL;
	tilemap[22][56] = TILE_GROUND;
	tilemap[22][57] = TILE_SPIKE;
	// Row 23
	tilemap[23][27] = TILE_SPIKE;
	for (int x = 28; x <= 29; x++)
		tilemap[23][x] = TILE_GROUND;
	for (int x = 30; x <= 55; x++)
		tilemap[23][x] = TILE_FILL;
	for (int x = 56; x <= 57; x++)
		tilemap[23][x] = TILE_GROUND;
	tilemap[23][58] = TILE_SPIKE;
	// Row 24
	for (int x = 5; x <= 8; x++)
		tilemap[24][x] = TILE_PLAT_ALT;
	tilemap[24][26] = TILE_SPIKE;
	for (int x = 27; x <= 28; x++)
		tilemap[24][x] = TILE_GROUND;
	for (int x = 29; x <= 56; x++)
		tilemap[24][x] = TILE_FILL;
	for (int x = 57; x <= 58; x++)
		tilemap[24][x] = TILE_GROUND;
	// Row 25
	for (int x = 24; x <= 25; x++)
		tilemap[25][x] = TILE_SPIKE;
	for (int x = 26; x <= 27; x++)
		tilemap[25][x] = TILE_GROUND;
	for (int x = 28; x <= 57; x++)
		tilemap[25][x] = TILE_FILL;
	tilemap[25][58] = TILE_GROUND;
	for (int x = 59; x <= 60; x++)
		tilemap[25][x] = TILE_SPIKE;
	// Row 26
	tilemap[26][23] = TILE_SPIKE;
	for (int x = 24; x <= 26; x++)
		tilemap[26][x] = TILE_GROUND;
	for (int x = 27; x <= 58; x++)
		tilemap[26][x] = TILE_FILL;
	for (int x = 59; x <= 60; x++)
		tilemap[26][x] = TILE_GROUND;
	tilemap[26][61] = TILE_SPIKE;
	// Row 27
	for (int x = 5; x <= 8; x++)
		tilemap[27][x] = TILE_PLAT_ALT;
	for (int x = 19; x <= 22; x++)
		tilemap[27][x] = TILE_SPIKE;
	for (int x = 23; x <= 24; x++)
		tilemap[27][x] = TILE_GROUND;
	for (int x = 25; x <= 59; x++)
		tilemap[27][x] = TILE_FILL;
	for (int x = 60; x <= 61; x++)
		tilemap[27][x] = TILE_GROUND;
	for (int x = 62; x <= 63; x++)
		tilemap[27][x] = TILE_SPIKE;
	// Row 28
	for (int x = 16; x <= 18; x++)
		tilemap[28][x] = TILE_SPIKE;
	for (int x = 19; x <= 22; x++)
		tilemap[28][x] = TILE_GROUND;
	for (int x = 23; x <= 60; x++)
		tilemap[28][x] = TILE_FILL;
	for (int x = 61; x <= 63; x++)
		tilemap[28][x] = TILE_GROUND;
	for (int x = 64; x <= 66; x++)
		tilemap[28][x] = TILE_SPIKE;
	// Row 29
	for (int x = 16; x <= 19; x++)
		tilemap[29][x] = TILE_GROUND;
	for (int x = 20; x <= 62; x++)
		tilemap[29][x] = TILE_FILL;
	for (int x = 63; x <= 66; x++)
		tilemap[29][x] = TILE_GROUND;
	// Row 30
	for (int x = 0; x <= 15; x++)
		tilemap[30][x] = TILE_GROUND;
	for (int x = 16; x <= 65; x++)
		tilemap[30][x] = TILE_FILL;
	for (int x = 66; x <= 79; x++)
		tilemap[30][x] = TILE_GROUND;
	// Row 31
	for (int x = 0; x <= 79; x++)
		tilemap[31][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 1129.0f, 201.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 1609.0f, 201.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 1865.0f, 73.0f, 14, 14 }, true, 0.6f };

	// Player spawn
	spawnX = 48.0f;
	spawnY = 932.0f;
}

// Level 7: "1-8", World 1
static void buildLevel_8() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 8
	tilemap[8][7] = TILE_CRUMBLE;
	for (int x = 10; x <= 12; x++)
		tilemap[8][x] = TILE_CRUMBLE;
	for (int x = 13; x <= 16; x++)
		tilemap[8][x] = TILE_PLATFORM;
	// Row 9
	tilemap[9][7] = TILE_CRUMBLE;
	for (int x = 21; x <= 24; x++)
		tilemap[9][x] = TILE_PLATFORM;
	for (int x = 40; x <= 42; x++)
		tilemap[9][x] = TILE_CRUMBLE;
	for (int x = 63; x <= 65; x++)
		tilemap[9][x] = TILE_CRUMBLE;
	for (int x = 69; x <= 70; x++)
		tilemap[9][x] = TILE_CRUMBLE;
	for (int x = 74; x <= 75; x++)
		tilemap[9][x] = TILE_CRUMBLE;
	for (int x = 78; x <= 79; x++)
		tilemap[9][x] = TILE_PLATFORM;
	// Row 10
	tilemap[10][7] = TILE_CRUMBLE;
	tilemap[10][10] = TILE_CRUMBLE;
	for (int x = 28; x <= 30; x++)
		tilemap[10][x] = TILE_CRUMBLE;
	for (int x = 34; x <= 36; x++)
		tilemap[10][x] = TILE_CRUMBLE;
	for (int x = 47; x <= 49; x++)
		tilemap[10][x] = TILE_CRUMBLE;
	for (int x = 58; x <= 60; x++)
		tilemap[10][x] = TILE_CRUMBLE;
	// Row 11
	tilemap[11][10] = TILE_CRUMBLE;
	for (int x = 53; x <= 55; x++)
		tilemap[11][x] = TILE_CRUMBLE;
	// Row 12
	tilemap[12][10] = TILE_CRUMBLE;
	// Row 14
	tilemap[14][8] = TILE_PLAT_ALT;
	for (int x = 43; x <= 54; x++)
		tilemap[14][x] = TILE_PLATFORM;
	// Row 15
	for (int x = 6; x <= 7; x++)
		tilemap[15][x] = TILE_PLAT_ALT;
	tilemap[15][43] = TILE_PLATFORM;
	tilemap[15][54] = TILE_PLATFORM;
	// Row 16
	tilemap[16][43] = TILE_PLATFORM;
	tilemap[16][54] = TILE_PLATFORM;
	// Row 17
	tilemap[17][43] = TILE_PLATFORM;
	for (int x = 48; x <= 50; x++)
		tilemap[17][x] = TILE_PLAT_ALT;
	tilemap[17][54] = TILE_PLATFORM;
	// Row 18
	for (int x = 6; x <= 8; x++)
		tilemap[18][x] = TILE_PLATFORM;
	tilemap[18][43] = TILE_PLATFORM;
	tilemap[18][54] = TILE_PLATFORM;
	// Row 19
	tilemap[19][43] = TILE_PLATFORM;
	tilemap[19][54] = TILE_PLATFORM;
	// Row 20
	tilemap[20][43] = TILE_PLATFORM;
	for (int x = 46; x <= 48; x++)
		tilemap[20][x] = TILE_PLAT_ALT;
	tilemap[20][54] = TILE_PLATFORM;
	// Row 21
	for (int x = 13; x <= 16; x++)
		tilemap[21][x] = TILE_PLATFORM;
	tilemap[21][54] = TILE_PLATFORM;
	// Row 22
	tilemap[22][16] = TILE_PLATFORM;
	for (int x = 49; x <= 51; x++)
		tilemap[22][x] = TILE_PLATFORM;
	tilemap[22][54] = TILE_PLATFORM;
	// Row 23
	tilemap[23][54] = TILE_PLATFORM;
	// Row 24
	for (int x = 18; x <= 20; x++)
		tilemap[24][x] = TILE_PLATFORM;
	for (int x = 43; x <= 46; x++)
		tilemap[24][x] = TILE_PLATFORM;
	tilemap[24][54] = TILE_PLATFORM;
	// Row 25
	for (int x = 23; x <= 25; x++)
		tilemap[25][x] = TILE_PLAT_ALT;
	tilemap[25][43] = TILE_PLATFORM;
	tilemap[25][54] = TILE_PLATFORM;
	for (int x = 64; x <= 79; x++)
		tilemap[25][x] = TILE_PLATFORM;
	// Row 26
	tilemap[26][43] = TILE_PLATFORM;
	// Row 27
	tilemap[27][43] = TILE_PLATFORM;
	for (int x = 60; x <= 62; x++)
		tilemap[27][x] = TILE_PLATFORM;
	// Row 28
	for (int x = 8; x <= 10; x++)
		tilemap[28][x] = TILE_MOVING;
	for (int x = 17; x <= 19; x++)
		tilemap[28][x] = TILE_MOVING;
	tilemap[28][43] = TILE_PLATFORM;
	// Row 29
	tilemap[29][43] = TILE_PLATFORM;
	for (int x = 54; x <= 57; x++)
		tilemap[29][x] = TILE_PLATFORM;
	// Row 30
	for (int x = 0; x <= 5; x++)
		tilemap[30][x] = TILE_GROUND;
	tilemap[30][43] = TILE_PLATFORM;
	tilemap[30][54] = TILE_PLATFORM;
	// Row 31
	for (int x = 0; x <= 5; x++)
		tilemap[31][x] = TILE_FILL;
	tilemap[31][43] = TILE_PLATFORM;
	tilemap[31][54] = TILE_PLATFORM;

	// Crackers
	coins[numCoins++] = { { 617.0f, 745.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 233.0f, 233.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 713.0f, 873.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 1545.0f, 521.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 1577.0f, 521.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 1609.0f, 521.0f, 14, 14 }, true, 1.5f };
	coins[numCoins++] = { { 1129.0f, 297.0f, 14, 14 }, true, 1.8f };
	coins[numCoins++] = { { 1737.0f, 329.0f, 14, 14 }, true, 2.1f };
	coins[numCoins++] = { { 2057.0f, 265.0f, 14, 14 }, true, 2.4f };

	// Player spawn
	spawnX = 48.0f;
	spawnY = 932.0f;
}

// Level 8: "1-9", World 1
static void buildLevel_9() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 0
	for (int x = 0; x <= 79; x++)
		tilemap[0][x] = TILE_FILL;
	// Row 1
	for (int x = 0; x <= 79; x++)
		tilemap[1][x] = TILE_FILL;
	// Row 2
	for (int x = 0; x <= 36; x++)
		tilemap[2][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[2][x] = TILE_FILL;
	tilemap[2][52] = TILE_FILL;
	tilemap[2][56] = TILE_FILL;
	tilemap[2][60] = TILE_FILL;
	tilemap[2][64] = TILE_FILL;
	tilemap[2][68] = TILE_FILL;
	// Row 3
	for (int x = 0; x <= 36; x++)
		tilemap[3][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[3][x] = TILE_FILL;
	tilemap[3][50] = TILE_FILL;
	tilemap[3][52] = TILE_FILL;
	tilemap[3][54] = TILE_FILL;
	tilemap[3][56] = TILE_FILL;
	tilemap[3][58] = TILE_FILL;
	tilemap[3][60] = TILE_FILL;
	tilemap[3][62] = TILE_FILL;
	tilemap[3][64] = TILE_FILL;
	tilemap[3][66] = TILE_FILL;
	tilemap[3][68] = TILE_FILL;
	// Row 4
	for (int x = 0; x <= 36; x++)
		tilemap[4][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[4][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[4][x] = TILE_FILL;
	tilemap[4][50] = TILE_FILL;
	tilemap[4][52] = TILE_FILL;
	tilemap[4][54] = TILE_FILL;
	tilemap[4][56] = TILE_FILL;
	tilemap[4][58] = TILE_FILL;
	tilemap[4][60] = TILE_FILL;
	tilemap[4][62] = TILE_FILL;
	tilemap[4][64] = TILE_FILL;
	tilemap[4][66] = TILE_FILL;
	tilemap[4][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[4][x] = TILE_FILL;
	// Row 5
	for (int x = 0; x <= 7; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 30; x <= 36; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[5][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[5][x] = TILE_FILL;
	tilemap[5][50] = TILE_FILL;
	tilemap[5][52] = TILE_FILL;
	tilemap[5][54] = TILE_FILL;
	tilemap[5][56] = TILE_FILL;
	tilemap[5][58] = TILE_FILL;
	tilemap[5][60] = TILE_FILL;
	tilemap[5][62] = TILE_FILL;
	tilemap[5][64] = TILE_FILL;
	tilemap[5][66] = TILE_FILL;
	tilemap[5][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[5][x] = TILE_FILL;
	// Row 6
	for (int x = 0; x <= 7; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 30; x <= 36; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[6][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[6][x] = TILE_FILL;
	tilemap[6][50] = TILE_FILL;
	tilemap[6][52] = TILE_FILL;
	tilemap[6][54] = TILE_FILL;
	tilemap[6][56] = TILE_FILL;
	tilemap[6][58] = TILE_FILL;
	tilemap[6][60] = TILE_FILL;
	tilemap[6][62] = TILE_FILL;
	tilemap[6][64] = TILE_FILL;
	tilemap[6][66] = TILE_FILL;
	tilemap[6][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[6][x] = TILE_FILL;
	// Row 7
	for (int x = 0; x <= 7; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 30; x <= 36; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[7][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[7][x] = TILE_FILL;
	tilemap[7][50] = TILE_FILL;
	tilemap[7][52] = TILE_FILL;
	tilemap[7][54] = TILE_FILL;
	tilemap[7][56] = TILE_FILL;
	tilemap[7][58] = TILE_FILL;
	tilemap[7][60] = TILE_FILL;
	tilemap[7][62] = TILE_FILL;
	tilemap[7][64] = TILE_FILL;
	tilemap[7][66] = TILE_FILL;
	tilemap[7][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[7][x] = TILE_FILL;
	// Row 8
	for (int x = 0; x <= 7; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[8][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[8][x] = TILE_FILL;
	tilemap[8][50] = TILE_FILL;
	tilemap[8][52] = TILE_FILL;
	tilemap[8][54] = TILE_FILL;
	tilemap[8][56] = TILE_FILL;
	tilemap[8][58] = TILE_FILL;
	tilemap[8][60] = TILE_FILL;
	tilemap[8][62] = TILE_FILL;
	tilemap[8][64] = TILE_FILL;
	tilemap[8][66] = TILE_FILL;
	tilemap[8][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[8][x] = TILE_FILL;
	// Row 9
	for (int x = 0; x <= 7; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 40; x <= 43; x++)
		tilemap[9][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[9][x] = TILE_FILL;
	tilemap[9][50] = TILE_FILL;
	tilemap[9][52] = TILE_FILL;
	tilemap[9][54] = TILE_FILL;
	tilemap[9][56] = TILE_FILL;
	tilemap[9][58] = TILE_FILL;
	tilemap[9][60] = TILE_FILL;
	tilemap[9][62] = TILE_FILL;
	tilemap[9][64] = TILE_FILL;
	tilemap[9][66] = TILE_FILL;
	tilemap[9][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[9][x] = TILE_FILL;
	// Row 10
	for (int x = 0; x <= 7; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 12; x <= 13; x++)
		tilemap[10][x] = TILE_PLAT_ALT;
	for (int x = 14; x <= 25; x++)
		tilemap[10][x] = TILE_GROUND;
	for (int x = 40; x <= 43; x++)
		tilemap[10][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[10][x] = TILE_FILL;
	tilemap[10][50] = TILE_FILL;
	tilemap[10][52] = TILE_FILL;
	tilemap[10][54] = TILE_FILL;
	tilemap[10][56] = TILE_FILL;
	tilemap[10][58] = TILE_FILL;
	tilemap[10][60] = TILE_FILL;
	tilemap[10][62] = TILE_FILL;
	tilemap[10][64] = TILE_FILL;
	tilemap[10][66] = TILE_FILL;
	tilemap[10][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[10][x] = TILE_FILL;
	// Row 11
	for (int x = 0; x <= 7; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 30; x <= 43; x++)
		tilemap[11][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[11][x] = TILE_FILL;
	tilemap[11][50] = TILE_FILL;
	tilemap[11][52] = TILE_FILL;
	tilemap[11][54] = TILE_FILL;
	tilemap[11][56] = TILE_FILL;
	tilemap[11][58] = TILE_FILL;
	tilemap[11][60] = TILE_FILL;
	tilemap[11][62] = TILE_FILL;
	tilemap[11][64] = TILE_FILL;
	tilemap[11][66] = TILE_FILL;
	tilemap[11][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[11][x] = TILE_FILL;
	// Row 12
	for (int x = 0; x <= 7; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 30; x <= 32; x++)
		tilemap[12][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[12][x] = TILE_FILL;
	tilemap[12][50] = TILE_FILL;
	tilemap[12][52] = TILE_FILL;
	tilemap[12][54] = TILE_FILL;
	tilemap[12][56] = TILE_FILL;
	tilemap[12][58] = TILE_FILL;
	tilemap[12][60] = TILE_FILL;
	tilemap[12][62] = TILE_FILL;
	tilemap[12][64] = TILE_FILL;
	tilemap[12][66] = TILE_FILL;
	tilemap[12][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[12][x] = TILE_FILL;
	// Row 13
	for (int x = 0; x <= 7; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 8; x <= 9; x++)
		tilemap[13][x] = TILE_PLAT_ALT;
	for (int x = 14; x <= 25; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 30; x <= 32; x++)
		tilemap[13][x] = TILE_FILL;
	for (int x = 46; x <= 48; x++)
		tilemap[13][x] = TILE_FILL;
	tilemap[13][50] = TILE_FILL;
	tilemap[13][52] = TILE_FILL;
	tilemap[13][54] = TILE_FILL;
	tilemap[13][56] = TILE_FILL;
	tilemap[13][58] = TILE_FILL;
	tilemap[13][60] = TILE_FILL;
	tilemap[13][62] = TILE_FILL;
	tilemap[13][64] = TILE_FILL;
	tilemap[13][66] = TILE_FILL;
	tilemap[13][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[13][x] = TILE_FILL;
	// Row 14
	for (int x = 0; x <= 7; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 30; x <= 32; x++)
		tilemap[14][x] = TILE_FILL;
	for (int x = 35; x <= 48; x++)
		tilemap[14][x] = TILE_FILL;
	tilemap[14][50] = TILE_FILL;
	tilemap[14][52] = TILE_FILL;
	tilemap[14][54] = TILE_FILL;
	tilemap[14][56] = TILE_FILL;
	tilemap[14][58] = TILE_FILL;
	tilemap[14][60] = TILE_FILL;
	tilemap[14][62] = TILE_FILL;
	tilemap[14][64] = TILE_FILL;
	tilemap[14][66] = TILE_FILL;
	tilemap[14][68] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[14][x] = TILE_FILL;
	// Row 15
	for (int x = 0; x <= 7; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[15][x] = TILE_FILL;
	for (int x = 30; x <= 32; x++)
		tilemap[15][x] = TILE_FILL;
	tilemap[15][50] = TILE_FILL;
	tilemap[15][54] = TILE_FILL;
	tilemap[15][58] = TILE_FILL;
	tilemap[15][62] = TILE_FILL;
	tilemap[15][66] = TILE_FILL;
	for (int x = 70; x <= 79; x++)
		tilemap[15][x] = TILE_FILL;
	// Row 16
	for (int x = 0; x <= 7; x++)
		tilemap[16][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[16][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[16][x] = TILE_FILL;
	// Row 17
	for (int x = 0; x <= 7; x++)
		tilemap[17][x] = TILE_FILL;
	for (int x = 12; x <= 13; x++)
		tilemap[17][x] = TILE_PLAT_ALT;
	for (int x = 14; x <= 25; x++)
		tilemap[17][x] = TILE_FILL;
	// Row 18
	for (int x = 0; x <= 7; x++)
		tilemap[18][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[18][x] = TILE_FILL;
	// Row 19
	for (int x = 0; x <= 7; x++)
		tilemap[19][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[19][x] = TILE_FILL;
	for (int x = 34; x <= 36; x++)
		tilemap[19][x] = TILE_SPIKE;
	for (int x = 41; x <= 43; x++)
		tilemap[19][x] = TILE_SPIKE;
	for (int x = 48; x <= 50; x++)
		tilemap[19][x] = TILE_SPIKE;
	for (int x = 60; x <= 62; x++)
		tilemap[19][x] = TILE_SPIKE;
	for (int x = 67; x <= 69; x++)
		tilemap[19][x] = TILE_SPIKE;
	for (int x = 74; x <= 76; x++)
		tilemap[19][x] = TILE_SPIKE;
	// Row 20
	for (int x = 0; x <= 7; x++)
		tilemap[20][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[20][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[20][x] = TILE_FILL;
	// Row 21
	for (int x = 0; x <= 7; x++)
		tilemap[21][x] = TILE_FILL;
	for (int x = 8; x <= 9; x++)
		tilemap[21][x] = TILE_PLAT_ALT;
	for (int x = 14; x <= 25; x++)
		tilemap[21][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[21][x] = TILE_FILL;
	// Row 22
	for (int x = 0; x <= 7; x++)
		tilemap[22][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[22][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[22][x] = TILE_FILL;
	// Row 23
	for (int x = 0; x <= 7; x++)
		tilemap[23][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[23][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[23][x] = TILE_FILL;
	// Row 24
	for (int x = 0; x <= 7; x++)
		tilemap[24][x] = TILE_FILL;
	for (int x = 14; x <= 25; x++)
		tilemap[24][x] = TILE_FILL;
	for (int x = 30; x <= 34; x++)
		tilemap[24][x] = TILE_FILL;
	for (int x = 78; x <= 79; x++)
		tilemap[24][x] = TILE_FILL;
	// Row 25
	for (int x = 0; x <= 7; x++)
		tilemap[25][x] = TILE_FILL;
	for (int x = 12; x <= 13; x++)
		tilemap[25][x] = TILE_PLAT_ALT;
	for (int x = 14; x <= 25; x++)
		tilemap[25][x] = TILE_FILL;
	for (int x = 30; x <= 34; x++)
		tilemap[25][x] = TILE_FILL;
	for (int x = 78; x <= 79; x++)
		tilemap[25][x] = TILE_FILL;
	// Row 26
	for (int x = 14; x <= 25; x++)
		tilemap[26][x] = TILE_FILL;
	for (int x = 30; x <= 34; x++)
		tilemap[26][x] = TILE_FILL;
	for (int x = 78; x <= 79; x++)
		tilemap[26][x] = TILE_FILL;
	// Row 27
	for (int x = 14; x <= 25; x++)
		tilemap[27][x] = TILE_FILL;
	for (int x = 30; x <= 34; x++)
		tilemap[27][x] = TILE_FILL;
	for (int x = 78; x <= 79; x++)
		tilemap[27][x] = TILE_FILL;
	// Row 28
	for (int x = 14; x <= 25; x++)
		tilemap[28][x] = TILE_FILL;
	for (int x = 39; x <= 79; x++)
		tilemap[28][x] = TILE_FILL;
	// Row 29
	for (int x = 14; x <= 25; x++)
		tilemap[29][x] = TILE_FILL;
	for (int x = 39; x <= 79; x++)
		tilemap[29][x] = TILE_FILL;
	// Row 30
	for (int x = 0; x <= 13; x++)
		tilemap[30][x] = TILE_GROUND;
	for (int x = 14; x <= 25; x++)
		tilemap[30][x] = TILE_FILL;
	for (int x = 39; x <= 79; x++)
		tilemap[30][x] = TILE_FILL;
	// Row 31
	for (int x = 0; x <= 25; x++)
		tilemap[31][x] = TILE_FILL;
	for (int x = 30; x <= 79; x++)
		tilemap[31][x] = TILE_FILL;

	// Crackers
	coins[numCoins++] = { { 425.0f, 777.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 265.0f, 393.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 1257.0f, 873.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 1673.0f, 873.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 1993.0f, 873.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 2441.0f, 873.0f, 14, 14 }, true, 1.5f };

	// Enemies
	spawnEnemy(1764.0f, 616.0f);

	// 3D tiles
	tile3dMap[24][78] = true;
	tile3dMap[24][79] = true;
	tile3dMap[25][78] = true;
	tile3dMap[25][79] = true;
	tile3dMap[26][78] = true;
	tile3dMap[26][79] = true;
	tile3dMap[27][78] = true;
	tile3dMap[27][79] = true;

	// Warp tiles
	warpMap[24][79] = true;
	warpMap[25][79] = true;
	warpMap[26][79] = true;
	warpMap[27][79] = true;
	warpTargetLevel = 10;

	// Player spawn
	spawnX = 48.0f;
	spawnY = 932.0f;
}

// Level 9: "1-10", World 1
static void buildLevel_10() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 4
	for (int x = 53; x <= 56; x++)
		tilemap[4][x] = TILE_CRUMBLE;
	// Row 6
	for (int x = 7; x <= 9; x++)
		tilemap[6][x] = TILE_CRUMBLE;
	for (int x = 17; x <= 19; x++)
		tilemap[6][x] = TILE_CRUMBLE;
	for (int x = 27; x <= 29; x++)
		tilemap[6][x] = TILE_CRUMBLE;
	for (int x = 37; x <= 39; x++)
		tilemap[6][x] = TILE_CRUMBLE;
	for (int x = 47; x <= 49; x++)
		tilemap[6][x] = TILE_CRUMBLE;
	// Row 9
	for (int x = 0; x <= 2; x++)
		tilemap[9][x] = TILE_CRUMBLE;
	tilemap[9][3] = TILE_PLATFORM;
	// Row 10
	tilemap[10][3] = TILE_PLATFORM;
	tilemap[10][38] = TILE_PLATFORM;
	for (int x = 41; x <= 46; x++)
		tilemap[10][x] = TILE_PLATFORM;
	// Row 11
	tilemap[11][3] = TILE_PLATFORM;
	tilemap[11][38] = TILE_PLATFORM;
	tilemap[11][41] = TILE_PLATFORM;
	// Row 12
	tilemap[12][3] = TILE_PLATFORM;
	tilemap[12][22] = TILE_PLATFORM;
	tilemap[12][38] = TILE_PLATFORM;
	tilemap[12][41] = TILE_PLATFORM;
	// Row 13
	tilemap[13][3] = TILE_PLATFORM;
	tilemap[13][22] = TILE_PLATFORM;
	tilemap[13][38] = TILE_PLATFORM;
	tilemap[13][41] = TILE_PLATFORM;
	// Row 14
	tilemap[14][3] = TILE_PLATFORM;
	tilemap[14][38] = TILE_PLATFORM;
	tilemap[14][41] = TILE_PLATFORM;
	// Row 15
	tilemap[15][3] = TILE_PLATFORM;
	for (int x = 17; x <= 19; x++)
		tilemap[15][x] = TILE_PLATFORM;
	for (int x = 25; x <= 27; x++)
		tilemap[15][x] = TILE_PLATFORM;
	for (int x = 31; x <= 33; x++)
		tilemap[15][x] = TILE_PLATFORM;
	tilemap[15][38] = TILE_PLATFORM;
	tilemap[15][41] = TILE_PLATFORM;
	// Row 16
	tilemap[16][3] = TILE_PLATFORM;
	tilemap[16][38] = TILE_PLATFORM;
	tilemap[16][41] = TILE_PLATFORM;
	for (int x = 63; x <= 79; x++)
		tilemap[16][x] = TILE_CRUMBLE;
	// Row 17
	tilemap[17][3] = TILE_PLATFORM;
	tilemap[17][38] = TILE_PLATFORM;
	tilemap[17][41] = TILE_PLATFORM;
	// Row 18
	tilemap[18][3] = TILE_PLATFORM;
	for (int x = 12; x <= 14; x++)
		tilemap[18][x] = TILE_PLAT_ALT;
	tilemap[18][17] = TILE_PLATFORM;
	tilemap[18][38] = TILE_PLATFORM;
	tilemap[18][41] = TILE_PLATFORM;
	// Row 19
	for (int x = 0; x <= 2; x++)
		tilemap[19][x] = TILE_CRUMBLE;
	tilemap[19][3] = TILE_PLATFORM;
	tilemap[19][17] = TILE_PLATFORM;
	tilemap[19][38] = TILE_PLATFORM;
	for (int x = 39; x <= 40; x++)
		tilemap[19][x] = TILE_PLAT_ALT;
	tilemap[19][41] = TILE_PLATFORM;
	// Row 20
	tilemap[20][17] = TILE_PLATFORM;
	tilemap[20][41] = TILE_PLATFORM;
	// Row 21
	tilemap[21][17] = TILE_PLATFORM;
	tilemap[21][41] = TILE_PLATFORM;
	// Row 22
	for (int x = 36; x <= 40; x++)
		tilemap[22][x] = TILE_CRUMBLE;
	tilemap[22][41] = TILE_PLATFORM;
	// Row 23
	for (int x = 0; x <= 2; x++)
		tilemap[23][x] = TILE_MOVING;
	for (int x = 11; x <= 14; x++)
		tilemap[23][x] = TILE_PLATFORM;

	// Crackers
	coins[numCoins++] = { { 265.0f, 169.0f, 14, 14 }, true, 0.0f };
	coins[numCoins++] = { { 585.0f, 169.0f, 14, 14 }, true, 0.3f };
	coins[numCoins++] = { { 905.0f, 169.0f, 14, 14 }, true, 0.6f };
	coins[numCoins++] = { { 1225.0f, 169.0f, 14, 14 }, true, 0.9f };
	coins[numCoins++] = { { 1545.0f, 169.0f, 14, 14 }, true, 1.2f };
	coins[numCoins++] = { { 2121.0f, 489.0f, 14, 14 }, true, 1.5f };
	coins[numCoins++] = { { 2473.0f, 489.0f, 14, 14 }, true, 1.8f };

	// Player spawn
	spawnX = 40.0f;
	spawnY = 258.4f;
}

// Level 10: "1-11", World 1, HIDDEN
static void buildLevel_11() {
	memset(tilemap, TILE_EMPTY, sizeof(tilemap));
	numCoins = 0;
	numEnemies = 0;
	for (int i = 0; i < MAX_VFX; i++) vfx[i].active = false;

	// Row 118
	for (int x = 0; x <= 31; x++)
		tilemap[118][x] = TILE_GROUND;
	// Row 119
	for (int x = 0; x <= 31; x++)
		tilemap[119][x] = TILE_FILL;

	// Player spawn (feet on row 118 ground)
	spawnX = 53.5f;
	spawnY = 118.0f * TILE_SIZE - 28.0f;
}

static void buildLevel() {
	// Reset movers and overlay maps for each level build
	numMovers = 0;
	memset(tile3dMap, false, sizeof(tile3dMap));
	memset(warpMap, false, sizeof(warpMap));
	memset(winMap, false, sizeof(winMap));
	memset(checkpointMap, false, sizeof(checkpointMap));
	warpTargetLevel = -1;
	switch (currentLevel) {
		case 0: buildLevel_1(); break;
		case 1: buildLevel_2(); break;
		case 2: buildLevel_3(); break;
		case 3: buildLevel_4(); break;
		case 4: buildLevel_5(); break;
		case 5: buildLevel_6(); break;
		case 6: buildLevel_7(); break;
		case 7: buildLevel_8(); break;
		case 8: buildLevel_9(); break;
		case 9: buildLevel_10(); break;
		case 10: buildLevel_11(); break;
		default: buildLevel_1(); break;
	}
	// Scan tilemap for TILE_MOVING markers → spawn moving platforms + clear tile
	for (int ty = 0; ty < getActiveMapHeight() && ty < MAP_H; ty++) {
		for (int tx = 0; tx < getActiveMapWidth() && tx < MAP_W; tx++) {
			if (tilemap[ty][tx] == TILE_MOVING) {
				tilemap[ty][tx] = TILE_EMPTY;
				spawnMover(tx * TILE_SIZE, ty * TILE_SIZE, 3.0f * TILE_SIZE, 1.0f);
			}
		}
	}
	refreshLevelHasWinZone();
}

// @@EDITOR_LEVELS_END@@

static void detectStationaryEnemies();

static void snapCameraToPlayer() {
	float maxCamX = (float)getActiveMapWidth() * TILE_SIZE - TOP_WIDTH;
	if (maxCamX < 0.0f) maxCamX = 0.0f;
	float maxCamY = (float)getActiveMapHeight() * TILE_SIZE - TOP_HEIGHT;
	if (maxCamY < 0.0f) maxCamY = 0.0f;
	float focusY = player.rect.y + player.rect.h * 0.5f - TOP_HEIGHT * 0.4f;
	if (focusY < 0.0f) focusY = 0.0f;
	if (focusY > maxCamY) focusY = maxCamY;
	camera.y = focusY;
	camera.x = player.rect.x + player.rect.w * 0.5f - TOP_WIDTH * 0.5f;
	if (camera.x < 0.0f) camera.x = 0.0f;
	if (camera.x > maxCamX) camera.x = maxCamX;
}

static void snapPlayerOntoSolidBelow() {
	int leftTile = (int)(player.rect.x / TILE_SIZE);
	int rightTile = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
	int footTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);
	if (footTile < 0) footTile = 0;
	int maxH = getActiveMapHeight();
	for (int ty = footTile; ty < maxH; ty++) {
		for (int tx = leftTile; tx <= rightTile; tx++) {
			if (!isSolidAt(tx, ty)) continue;
			player.rect.y = (float)ty * TILE_SIZE - player.rect.h;
			player.vy = 0.0f;
			player.onGround = true;
			return;
		}
	}
}

static void initLevel() {
	buildLevel();
	buildCheckpointsFromMap();
	populateGrass(getActiveMapWidth(), getActiveMapHeight());
	generateLightShafts();
	// BG selection is now done per-tile in drawGame (4:1 mix)

	if (levelHasCheckpoints && lastCheckpointIdx >= 0) {
		float cpx, cpy;
		checkpointSpawnPos(lastCheckpointIdx, &cpx, &cpy);
		player.rect = { cpx, cpy, 16.0f, 28.0f };
	} else {
		player.rect = { spawnX, spawnY, 16.0f, 28.0f };
	}
	player.vx = 0.0f;
	player.vy = 0.0f;
	player.onGround = false;
	player.facingRight = true;
	// Note: player.sprinting intentionally NOT reset — sprint toggle persists across levels.
	player.jumpHeld = false;
	player.wallLeft = false;
	player.wallRight = false;
	player.wallJumpLock = 0;
	player.wallJumpBlend = 0;
	player.coyoteTimer = 0;
	player.jumpBuffer = 0;
	player.groundPound = false;
	player.gpoundFreeze = 0;
	player.lastWallJumpSide = 0;
	player.dashing = false;
	player.dashTimer = 0;
	player.dashCooldown = 0;
	player.score = sessionCrackersCommitted;
	player.lives = 3;
	player.scaleX = 1.0f;
	player.scaleY = 1.0f;
	player.squashTimer = 0;
	player.invulnTimer = 0;

	snapPlayerOntoSolidBelow();
	camera.lookAheadX = 0.0f;
	camera.targetY = 0.0f;
	snapCameraToPlayer();

	animIdleTimer = 0; animRunFrame = 0; animRunTimer = 0;
	animGpoundLandTimer = 0; animRunInitTimer = 0; animWasMoving = false;
	stompCombo = 0;

	for (int i = 0; i < MAX_DUST; i++) dust[i].active = false;
	for (int i = 0; i < MAX_FLATTEN; i++) flattenFx[i].active = false;
	for (int i = 0; i < MAX_POPUPS; i++) popups[i].active = false;
	shakeTimer = 0; shakeX = 0; shakeY = 0;
	fadeTimer = 0;
	deathTimer = 0;
	player.doubleJumpUsed = false;

	// World 2 (levels 6+) has double jump and taller maps
	if (currentLevel >= 0 && currentLevel < LEVEL_COUNT) {
		const LevelInfo& li = LEVEL_INFO[currentLevel];
		levelAllowsDoubleJump = DOUBLE_JUMP_ENABLED && li.doubleJump;
		levelAllowsWallJump = WALL_JUMP_ENABLED && li.wallJump;
		levelAllowsDash = DASH_ENABLED && li.dash;
		levelAllowsGroundPound = GROUND_POUND_ENABLED && li.groundPound;
		levelAllowsDialogue = DIALOGUE_ENABLED && li.dialogue;
		levelAllowsMinimap = li.minimap;
	} else {
		levelAllowsDoubleJump = false;
		levelAllowsWallJump = false;
		levelAllowsDash = false;
		levelAllowsGroundPound = false;
		levelAllowsDialogue = false;
		levelAllowsMinimap = true;
	}

	applyLevelPhysicsForLevel(currentLevel);

	detectStationaryEnemies();

	// Register crumble tiles from tilemap
	numCrumbles = 0;
	for (int ty = 0; ty < getActiveMapHeight() && ty < MAP_H; ty++)
		for (int tx = 0; tx < getActiveMapWidth() && tx < MAP_W; tx++)
			if (tilemap[ty][tx] == TILE_CRUMBLE && numCrumbles < MAX_CRUMBLE)
				crumbles[numCrumbles++] = { tx, ty, 0, false, false };

	// Build crumble reverse index for O(1) lookup during rendering / collision.
	// All cells default to -1 (no crumble). 0xFFFF for short = -1 when interpreted signed,
	// so memset with 0xFF works.
	memset(crumbleIndex, 0xFF, sizeof(crumbleIndex));
	for (int i = 0; i < numCrumbles; i++)
		crumbleIndex[crumbles[i].ty][crumbles[i].tx] = (short)i;

	// Build minimap static cache. Skip empty + registered crumbles (those are drawn dynamically).
	// Crumble tiles NOT in crumbles[] (would-be overflow if MAX_CRUMBLE were exceeded) are
	// still drawn statically so the player can see them on the minimap as a fallback.
	minimapCacheCount = 0;
	u32 crumbleMmClr = C2D_Color32(180, 140, 60, 200);
	for (int ty = 0; ty < getActiveMapHeight() && ty < MAP_H; ty++) {
		for (int tx = 0; tx < getActiveMapWidth() && tx < MAP_W; tx++) {
			u8 t = tilemap[ty][tx];
			if (t == TILE_EMPTY) continue;
			if (t == TILE_CRUMBLE) {
				// Only orphans (not registered) need to be cached statically.
				if (crumbleIndex[ty][tx] >= 0) continue;
				minimapCache[minimapCacheCount++] = { (unsigned char)tx, (unsigned char)ty, crumbleMmClr };
				continue;
			}
			u32 clr;
			if (t == TILE_SPIKE) clr = C2D_Color32(210, 40, 40, 200);
			else if (t == TILE_PLATFORM || t == TILE_PLAT_ALT) clr = C2D_Color32(70, 120, 160, 200);
			else clr = C2D_Color32(140, 94, 60, 200);
			minimapCache[minimapCacheCount++] = { (unsigned char)tx, (unsigned char)ty, clr };
		}
	}

	// Start speedrun timer (restore elapsed time if returning to a saved checkpoint)
	timerPaused = 0;
	if (levelHasCheckpoints && lastCheckpointIdx >= 0 && sessionLevelTimer[currentLevel] >= 0.0f)
		setTimerElapsedSeconds(sessionLevelTimer[currentLevel]);
	else {
		timerStart = svcGetSystemTick();
		timerRunning = true;
	}

	// Check for pre-level dialogue
	if (levelAllowsDialogue && dialoguePreCount[currentLevel] > 0 && strlen(dialoguePre[currentLevel][0]) > 0) {
		gameState = STATE_DIALOGUE_PRE;
		currentDialogueBox = 0;
		currentDialogueChar = 0;
		dialogueTypewriterTimer = 0;
		dialoguePauseTimer = 0;
		dialogueWaitingForInput = false;
	} else {
		gameState = STATE_PLAYING;
	}
}

// ========================================================
// Input
// ========================================================
static void handleInput(u32 kHeld, u32 kDown, u32 kUp) {
	// --- Dash cooldown tick ---
	if (player.dashCooldown > 0) player.dashCooldown--;

	// --- Active dash: override movement ---
	if (player.dashing) {
		player.dashTimer--;
		player.vx = player.facingRight ? DASH_SPEED : -DASH_SPEED;
		player.vy = 0; // freeze vertical during dash
		if (player.dashTimer <= 0) {
			player.dashing = false;
			player.vx *= 0.4f; // bleed off speed after dash
		}
		return; // skip all other input during dash
	}

	// --- Sprint — toggle or hold based on g_settings.sprintToggle ---
	if (g_settings.sprintToggle) {
		if (kDown & (KEY_Y | KEY_X)) player.sprinting = !player.sprinting;
	} else {
		player.sprinting = (kHeld & (KEY_Y | KEY_X)) != 0;
	}
	float targetSpeed = player.sprinting ? SPRINT_SPEED : MOVE_SPEED;
	float accel = player.onGround ? effectiveAccelGround() : effectiveAccelAir();
	float decel = player.onGround ? effectiveDecelGround() : effectiveDecelAir();

	// --- Ground pound / splat freezes X movement ---
	if (player.groundPound || animGpoundLandTimer > 0) {
		player.vx = 0;
	}
	// --- Wall jump: short hard lock, then gradual blend back to player control ---
	else if (player.wallJumpLock > 0) {
		player.wallJumpLock--;
		// During hard lock: no directional input at all
		if (player.wallJumpLock == 0)
			player.wallJumpBlend = WALL_JUMP_BLEND;
	} else if (player.wallJumpBlend > 0) {
		player.wallJumpBlend--;
		// Blend: player input partially overrides wall jump velocity
		float blend = WALL_JUMP_BLEND_STR + (1.0f - WALL_JUMP_BLEND_STR) *
			(1.0f - (float)player.wallJumpBlend / WALL_JUMP_BLEND);
		if (kHeld & KEY_LEFT) {
			player.vx += (-targetSpeed - player.vx) * blend * 0.3f;
			player.facingRight = false;
		} else if (kHeld & KEY_RIGHT) {
			player.vx += (targetSpeed - player.vx) * blend * 0.3f;
			player.facingRight = true;
		}
	} else {
		// --- Normal momentum-based horizontal movement with turnaround skid ---
		if (kHeld & KEY_LEFT) {
			if (player.vx > 0 && player.onGround)
				player.vx *= effectiveSkidDecel();
			if (player.vx > -targetSpeed)
				player.vx -= accel;
			if (player.vx < -targetSpeed)
				player.vx = -targetSpeed;
			player.facingRight = false;
		} else if (kHeld & KEY_RIGHT) {
			if (player.vx < 0 && player.onGround)
				player.vx *= effectiveSkidDecel();
			if (player.vx < targetSpeed)
				player.vx += accel;
			if (player.vx > targetSpeed)
				player.vx = targetSpeed;
			player.facingRight = true;
		} else {
			player.vx *= decel;
			if (fabsf(player.vx) < 0.1f) player.vx = 0.0f;
		}
	}

	// --- Ground pound: press DOWN while airborne (not on ground, not already pounding) ---
	if (levelAllowsGroundPound && (kDown & KEY_DOWN) && !player.onGround && !player.groundPound) {
		player.groundPound = true;
		player.gpoundFreeze = GPOUND_FREEZE;
		player.vx = 0;
		player.vy = 0; // brief hang
		player.jumpHeld = false;
		player.wallJumpLock = 0;
		player.wallJumpBlend = 0;
	}

	// --- Dash: R bumper, not during ground pound ---
	if (levelAllowsDash && (kDown & KEY_R) && !player.groundPound && player.dashCooldown <= 0) {
		player.dashing = true;
		player.dashTimer = DASH_DURATION;
		player.dashCooldown = DASH_COOLDOWN;
		player.vy = 0;
		player.groundPound = false;
		player.gpoundFreeze = 0;
		player.wallJumpLock = 0;
		player.wallJumpBlend = 0;
		playSfx(SFX_DASH);
		// VFX: dash effect behind the player
		float vfxX = player.facingRight
			? (player.rect.x - TILE_SIZE + 4)
			: (player.rect.x + player.rect.w - 4);
		spawnVfx(VFX_DASH, vfxX, player.rect.y - 2, !player.facingRight, 10);
		return; // dash starts immediately
	}

	// --- Jump buffer: remember jump press for a few frames ---
	if (kDown & (KEY_A | KEY_B))
		player.jumpBuffer = effectiveJumpBufferFrames();
	else if (player.jumpBuffer > 0)
		player.jumpBuffer--;

	// --- Jump logic (with coyote time + jump buffer) ---
	bool jumpBtn = (kHeld & (KEY_A | KEY_B)) != 0;
	bool wantJump = player.jumpBuffer > 0;
	bool canCoyote = player.coyoteTimer > 0;

	if (wantJump) {
		// Cancel ground pound with jump
		if (player.groundPound) {
			player.groundPound = false;
			player.gpoundFreeze = 0;
			animGpoundLandTimer = 0;
			player.vy = effectiveJumpForce() * 0.75f; // shorter bounce out of pound
			player.onGround = false;
			player.jumpHeld = true;
			player.jumpBuffer = 0;
		} else if (animGpoundLandTimer > 0) {
			// Jump out of splat
			animGpoundLandTimer = 0;
			player.vy = effectiveJumpForce();
			player.onGround = false;
			player.jumpHeld = true;
			player.jumpBuffer = 0;
			playSfx(SFX_JUMP);
		} else if (player.onGround || canCoyote) {
			player.vy = effectiveJumpForce();
			player.onGround = false;
			player.jumpHeld = true;
			player.jumpBuffer = 0;
			player.coyoteTimer = 0;
			player.lastWallJumpSide = 0;
			playSfx(SFX_JUMP);
			player.scaleX = 0.8f;
			player.scaleY = 1.2f;
			player.squashTimer = 6;
			spawnDust(player.rect.x + player.rect.w * 0.5f, player.rect.y + player.rect.h, 0.0f, 2);
		} else if (levelAllowsWallJump && (player.wallLeft || player.wallRight)) {
			// Wall jump — only if not the same wall as last time
			int side = player.wallLeft ? -1 : 1;
			if (side != player.lastWallJumpSide) {
				player.vy = WALL_JUMP_VY;
				player.vx = player.wallLeft ? WALL_JUMP_VX : -WALL_JUMP_VX;
				player.facingRight = player.wallLeft;
				player.wallJumpLock = WALL_JUMP_LOCK;
				player.wallJumpBlend = 0;
				player.jumpHeld = true;
				player.jumpBuffer = 0;
				player.lastWallJumpSide = side;
				playSfx(SFX_WALL_JUMP);
				// VFX: wall jump effect at the wall
				// Effect is in top-left corner of sprite, so:
				//   Right wall (wallRight): place at player's right edge, no flip
				//   Left wall (wallLeft): place at player's left edge, flip X
				{
					float feetY = player.rect.y + player.rect.h;
					float py = feetY - TILE_SIZE + 16; // align effect at feet
					if (player.wallLeft) {
						spawnVfx(VFX_WALL_JUMP, player.rect.x - TILE_SIZE * 4.0f + 8, py, true, 12, 2.0f);
					} else {
						spawnVfx(VFX_WALL_JUMP, player.rect.x + player.rect.w - 8, py, false, 12, 2.0f);
					}
				}
				player.wallLeft = false;
				player.wallRight = false;
			}
		} else if (levelAllowsDoubleJump && !player.doubleJumpUsed && !player.onGround) {
			// Double jump (world 2+)
			player.vy = effectiveDJumpForce();
			player.jumpHeld = true;
			player.jumpBuffer = 0;
			player.doubleJumpUsed = true;
			playSfx(SFX_JUMP);
			player.scaleX = 0.8f;
			player.scaleY = 1.2f;
			player.squashTimer = 6;
			spawnDust(player.rect.x + player.rect.w * 0.5f, player.rect.y + player.rect.h, 0.0f, 2);
		}
	}

	// --- Variable jump height: cut velocity when button released ---
	if (player.jumpHeld && !jumpBtn) {
		player.jumpHeld = false;
		if (player.vy < 0)
			player.vy *= effectiveJumpCutMul();
	}
}

// ========================================================
// Tile-based collision helpers
// ========================================================
static void resolveHorizontal() {
	int leftTile   = (int)(player.rect.x / TILE_SIZE);
	int rightTile  = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
	int topTile    = (int)(player.rect.y / TILE_SIZE);
	int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);

	for (int ty = topTile; ty <= bottomTile; ty++) {
		for (int tx = leftTile; tx <= rightTile; tx++) {
			if (!isSolidFullAt(tx, ty)) continue; // one-way tiles don't block horizontally
			float tileX = tx * TILE_SIZE;
			if (player.vx > 0) {
				player.rect.x = tileX - player.rect.w;
			} else if (player.vx < 0) {
				player.rect.x = tileX + TILE_SIZE;
			}
			player.vx = 0;
			return;
		}
	}
}

// Detect wall contact (for wall jump) — call AFTER resolveHorizontal
static void detectWalls() {
	player.wallLeft = false;
	player.wallRight = false;
	if (player.onGround) return; // only wall-slide/jump while airborne

	int topTile    = (int)(player.rect.y / TILE_SIZE);
	int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);

	// Check one pixel to the left (skip one-way tiles)
	int leftProbe = (int)((player.rect.x - 1.0f) / TILE_SIZE);
	for (int ty = topTile; ty <= bottomTile; ty++) {
		if (isSolidFullAt(leftProbe, ty)) { player.wallLeft = true; break; }
	}

	// Check one pixel to the right (skip one-way tiles)
	int rightProbe = (int)((player.rect.x + player.rect.w + 1.0f) / TILE_SIZE);
	for (int ty = topTile; ty <= bottomTile; ty++) {
		if (isSolidFullAt(rightProbe, ty)) { player.wallRight = true; break; }
	}

	// Moving platforms act as solid walls too.
	float pTop = player.rect.y;
	float pBot = player.rect.y + player.rect.h;
	float pLeftEdge  = player.rect.x - 1.0f;
	float pRightEdge = player.rect.x + player.rect.w + 1.0f;
	for (int i = 0; i < numMovers; i++) {
		const MovingPlatform& m = movers[i];
		float mT = m.y, mB = m.y + TILE_SIZE;
		if (pBot <= mT || pTop >= mB) continue;
		if (!player.wallLeft  && pLeftEdge  >= m.x && pLeftEdge  <= m.x + TILE_SIZE) player.wallLeft  = true;
		if (!player.wallRight && pRightEdge >= m.x && pRightEdge <= m.x + TILE_SIZE) player.wallRight = true;
	}
}

static void resolveVertical() {
	int leftTile   = (int)(player.rect.x / TILE_SIZE);
	int rightTile  = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
	int topTile    = (int)(player.rect.y / TILE_SIZE);
	int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);

	for (int ty = topTile; ty <= bottomTile; ty++) {
		for (int tx = leftTile; tx <= rightTile; tx++) {
			if (!isSolidAt(tx, ty)) continue;
			u8 t = getTile(tx, ty);
			float tileY = ty * TILE_SIZE;
			if (player.vy > 0) {
				// One-way: only land if feet were above tile top before this frame
				if (isOneWay(t)) {
					float prevFeet = player.rect.y + player.rect.h - player.vy;
					if (prevFeet > tileY + 2.0f) continue; // was already inside, pass through
				}
				player.rect.y = tileY - player.rect.h;
				player.vy = 0;
				player.onGround = true;
				// Trigger crumble (O(1) reverse-index lookup)
				if (t == TILE_CRUMBLE) {
					int ci = crumbleIndex[ty][tx];
					if (ci >= 0 && !crumbles[ci].shaking && !crumbles[ci].fallen) {
						crumbles[ci].shaking = true;
						crumbles[ci].timer = CRUMBLE_SHAKE_TIME;
					}
				}
			} else if (player.vy < 0) {
				if (isOneWay(t)) continue; // pass through one-way from below
				player.rect.y = tileY + TILE_SIZE;
				player.vy = 0;
			}
			return;
		}
	}
}

static bool checkHazards() {
	int leftTile   = (int)(player.rect.x / TILE_SIZE);
	int rightTile  = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
	int topTile    = (int)(player.rect.y / TILE_SIZE);
	int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);

	float px = player.rect.x, py = player.rect.y;
	float pw = player.rect.w, ph = player.rect.h;

	for (int ty = topTile; ty <= bottomTile; ty++)
		for (int tx = leftTile; tx <= rightTile; tx++) {
			if (!isHazard(getTile(tx, ty))) continue;
			if (is3dTile(tx, ty) && tile3dIsPassable(tx, ty)) continue;
			// Spike hitbox: only bottom 12px of the tile
			float spikeTop = ty * TILE_SIZE + (TILE_SIZE - 12);
			float spikeBot = (ty + 1) * TILE_SIZE;
			float spikeL   = tx * TILE_SIZE;
			float spikeR   = (tx + 1) * TILE_SIZE;
			if (px + pw > spikeL && px < spikeR &&
			    py + ph > spikeTop && py < spikeBot)
				return true;
		}
	return false;
}

// ========================================================
// Update
// ========================================================
static void detectStationaryEnemies() {
	for (int i = 0; i < numEnemies; i++) {
		Enemy& e = enemies[i];
		if (!e.active) continue;
		int cx = (int)((e.rect.x + e.rect.w * 0.5f) / TILE_SIZE);
		int cy = (int)((e.rect.y + e.rect.h) / TILE_SIZE);
		int maxH = getActiveMapHeight();
		while (cy < maxH && !isSolid(getTile(cx, cy))) cy++;
		if (cy < maxH && isSolid(getTile(cx, cy))) {
			bool groundLeft  = isSolid(getTile(cx - 1, cy));
			bool groundRight = isSolid(getTile(cx + 1, cy));
			if (!groundLeft && !groundRight) {
				e.stationary = true;
				e.rect.x = cx * TILE_SIZE + (TILE_SIZE - e.rect.w) * 0.5f;
				e.rect.y = cy * TILE_SIZE - e.rect.h;
				e.vy = 0;
				e.onGround = true;
			}
		}
	}
}

static void doRespawn() {
	buildLevel(); // reset enemies, coins, and tilemap
	buildCheckpointsFromMap();
	detectStationaryEnemies();
	if (levelHasCheckpoints && lastCheckpointIdx >= 0) {
		checkpointSpawnPos(lastCheckpointIdx, &player.rect.x, &player.rect.y);
	} else {
		player.rect.x = spawnX;
		player.rect.y = spawnY;
	}
	player.onGround = false;
	snapPlayerOntoSolidBelow();
	snapCameraToPlayer();
	player.vx = 0;
	player.vy = 0;
	player.jumpHeld = false;
	player.wallLeft = false;
	player.wallRight = false;
	player.wallJumpLock = 0;
	player.wallJumpBlend = 0;
	player.coyoteTimer = 0;
	player.jumpBuffer = 0;
	player.groundPound = false;
	player.gpoundFreeze = 0;
	player.lastWallJumpSide = 0;
	player.dashing = false;
	player.dashTimer = 0;
	player.dashCooldown = 0;
	player.invulnTimer = INVULN_FRAMES;
	player.scaleX = 1.0f;
	player.scaleY = 1.0f;
	player.squashTimer = 0;
	stompCombo = 0;
	player.score = sessionCrackersCommitted;
	gameState = STATE_PLAYING;
	restoreCheckpointTimer();
	startFade(false, 0); // fade in
}

static void loseLife() {
	player.lives--;
	if (player.lives <= 0) {
		clearSessionCheckpoints();
		gameState = STATE_GAMEOVER;
		playSfx(SFX_DEATH);
		triggerShake(4.0f, 12);
		return;
	}
	// Start death animation
	playSfx(SFX_DEATH);
	triggerShake(4.0f, 12);
	deathX = player.rect.x;
	deathY = player.rect.y;
	deathVy = DEATH_POP_VY;
	deathTimer = DEATH_FREEZE + DEATH_POP_TIME;
	gameState = STATE_DYING;
}

// ========================================================
// Crumbling platforms update
// ========================================================
static void updateCrumbles() {
	for (int i = 0; i < numCrumbles; i++) {
		CrumbleTile& c = crumbles[i];
		if (c.shaking) {
			c.timer--;
			if (c.timer <= 0) {
				c.shaking = false;
				c.fallen = true;
				c.timer = CRUMBLE_RESPAWN_TIME;
				tilemap[c.ty][c.tx] = TILE_EMPTY; // remove tile
			}
		} else if (c.fallen) {
			c.timer--;
			if (c.timer <= 0) {
				c.fallen = false;
				tilemap[c.ty][c.tx] = TILE_CRUMBLE; // respawn
			}
		}
	}
}

// ========================================================
// Moving platforms update
// ========================================================
static void updateMovers() {
	for (int i = 0; i < numMovers; i++) {
		MovingPlatform& m = movers[i];
		float prevX = m.x;
		m.x += m.speed * m.dir;
		if (m.x > m.startX + m.range) { m.x = m.startX + m.range; m.dir = -1; }
		if (m.x < m.startX) { m.x = m.startX; m.dir = 1; }
		float dx = m.x - prevX;

		// Check if player is standing on this platform
		Rect platRect = { m.x, m.y, (float)TILE_SIZE, (float)TILE_SIZE };
		float playerFeet = player.rect.y + player.rect.h;
		bool onPlat = player.onGround &&
			player.rect.x + player.rect.w > platRect.x &&
			player.rect.x < platRect.x + platRect.w &&
			fabsf(playerFeet - platRect.y) < 2.0f;
		if (onPlat) {
			player.rect.x += dx;
		}
	}
}

// Resolve player collision with moving platforms (call after resolveVertical).
// Full 4-sided: land on top, bonk from below, push horizontally.
static void resolveMovers() {
	for (int i = 0; i < numMovers; i++) {
		MovingPlatform& m = movers[i];
		Rect platRect = { m.x, m.y, (float)TILE_SIZE, (float)TILE_SIZE };
		if (!rectsOverlap(player.rect, platRect)) continue;

		// Compute overlap on each axis to pick the smallest penetration.
		float pLeft  = player.rect.x;
		float pRight = player.rect.x + player.rect.w;
		float pTop   = player.rect.y;
		float pBot   = player.rect.y + player.rect.h;
		float mLeft  = platRect.x;
		float mRight = platRect.x + platRect.w;
		float mTop   = platRect.y;
		float mBot   = platRect.y + platRect.h;

		float overlapL = pRight - mLeft;   // push player left by this
		float overlapR = mRight - pLeft;   // push player right by this
		float overlapT = pBot   - mTop;    // push player up by this (land)
		float overlapB = mBot   - pTop;    // push player down by this (head bonk)

		// Prefer top landing if player is falling and was above the platform last frame.
		float prevFeet = pBot - player.vy;
		if (player.vy >= 0 && prevFeet <= mTop + 2.0f) {
			player.rect.y = mTop - player.rect.h;
			player.vy = 0;
			player.onGround = true;
			continue;
		}
		// Otherwise resolve on the axis of smallest overlap.
		float minH = overlapL < overlapR ? overlapL : overlapR;
		float minV = overlapT < overlapB ? overlapT : overlapB;
		if (minH < minV) {
			if (overlapL < overlapR) { player.rect.x -= overlapL; }
			else                     { player.rect.x += overlapR; }
			if (player.vx != 0) player.vx = 0;
		} else {
			if (overlapT < overlapB) {
				player.rect.y -= overlapT;
				if (player.vy > 0) player.vy = 0;
				player.onGround = true;
			} else {
				player.rect.y += overlapB;
				if (player.vy < 0) player.vy = 0;
			}
		}
	}
}

// ========================================================
// Enemy flatten FX + combo popup helpers
// ========================================================
static void spawnFlatten(float x, float y, bool flipX) {
	for (int i = 0; i < MAX_FLATTEN; i++) {
		if (!flattenFx[i].active) {
			flattenFx[i] = { x, y, 12, true, flipX };
			return;
		}
	}
	flattenFx[0] = { x, y, 12, true, flipX };
}

static void updateFlattenFx() {
	for (int i = 0; i < MAX_FLATTEN; i++) {
		if (!flattenFx[i].active) continue;
		flattenFx[i].timer--;
		if (flattenFx[i].timer <= 0) flattenFx[i].active = false;
	}
}

static void spawnPopup(float x, float y, int value) {
	for (int i = 0; i < MAX_POPUPS; i++) {
		if (!popups[i].active) {
			popups[i] = { x, y, value, 40, true };
			return;
		}
	}
	popups[0] = { x, y, value, 40, true };
}

static void updatePopups() {
	for (int i = 0; i < MAX_POPUPS; i++) {
		if (!popups[i].active) continue;
		popups[i].y -= 0.8f;
		popups[i].timer--;
		if (popups[i].timer <= 0) popups[i].active = false;
	}
}

static void updatePlayer() {
	bool wasOnGround = player.onGround;

	// --- Ground pound logic ---
	if (player.groundPound) {
		if (player.gpoundFreeze > 0) {
			// Freeze in the air briefly before slamming down
			player.gpoundFreeze--;
			player.vy = 0;
			player.vx = 0;
			return; // skip all physics during freeze
		}
		// Slamming down
		player.vy = GPOUND_SPEED;
		player.vx = 0;
	} else if (player.dashing) {
		// No gravity during dash — vy stays 0
		player.vy = 0;
	} else {
		// --- Asymmetric gravity ---
		// Rising and holding jump: lighter gravity (slight hang at apex)
		// Falling or jump released: heavier gravity (snappy descent)
		if (player.vy >= 0 || !player.jumpHeld) {
			player.vy += effectiveGravityFall();
		} else {
			player.vy += GRAVITY;
		}
	}

	// Wall slide: cap fall speed when touching a wall (not during ground pound)
	if (!player.groundPound && !player.onGround &&
		(player.wallLeft || player.wallRight)) {
		if (player.vy > WALL_SLIDE_SPD)
			player.vy = WALL_SLIDE_SPD;
	}

	// Cap fall speed (ground pound has its own fixed speed)
	if (!player.groundPound && player.vy > effectiveMaxFallSpd())
		player.vy = effectiveMaxFallSpd();

	// Horizontal movement + collision
	{
		float mapRight = (float)getActiveMapWidth() * TILE_SIZE;
		player.rect.x += player.vx;
		if (player.rect.x < 0.0f) { player.rect.x = 0.0f; player.vx = 0.0f; }
		if (player.rect.x + player.rect.w > mapRight) {
			player.rect.x = mapRight - player.rect.w;
			player.vx = 0.0f;
		}
	}
	resolveHorizontal();

	// Detect wall contact (must be after horizontal resolve)
	detectWalls();

	// Vertical movement + collision
	player.onGround = false;
	player.rect.y += player.vy;
	resolveVertical();
	resolveMovers(); // moving platform landing

	// --- Coyote time: start timer when leaving ground ---
	if (wasOnGround && !player.onGround && player.vy >= 0 && !player.groundPound) {
		player.coyoteTimer = effectiveCoyoteFrames();
	} else if (player.onGround) {
		player.coyoteTimer = 0;
	} else if (player.coyoteTimer > 0) {
		player.coyoteTimer--;
	}

	// --- On landing ---
	if (player.onGround) {
		if (!wasOnGround) {
			// Just landed — squash + dust + landing flash
			player.scaleX = 1.3f;
			player.scaleY = 0.7f;
			player.squashTimer = 6;
			float feetX = player.rect.x + player.rect.w * 0.5f;
			float feetY2 = player.rect.y + player.rect.h;
			spawnDust(feetX, feetY2, 0.0f, 3);
			landingFlashTimer = 4;
			landingFlashX = feetX;
			landingFlashY = feetY2;
			if (player.groundPound) { triggerShake(3.0f, 8); animGpoundLandTimer = 12; }
		}
		player.jumpHeld = false;
		player.groundPound = false;
		player.gpoundFreeze = 0;
		player.wallJumpLock = 0;
		player.wallJumpBlend = 0;
		player.lastWallJumpSide = 0;
		player.doubleJumpUsed = false;
		stompCombo = 0;
		if (player.dashing) {
			player.dashing = false;
			player.vx *= 0.4f;
		}
	}

	// Squash/stretch decay
	if (player.squashTimer > 0) {
		player.squashTimer--;
		float t = (float)player.squashTimer / 6.0f;
		player.scaleX = 1.0f + (player.scaleX > 1.0f ? 0.3f : -0.15f) * t;
		player.scaleY = 1.0f + (player.scaleY < 1.0f ? -0.3f : 0.15f) * t;
	} else {
		player.scaleX = 1.0f;
		player.scaleY = 1.0f;
	}

	// Invulnerability tick
	if (player.invulnTimer > 0) player.invulnTimer--;
	if (landingFlashTimer > 0) landingFlashTimer--;

	// Running dust (every 8 frames while grounded and moving fast)
	if (player.onGround && fabsf(player.vx) > 2.5f) {
		static int dustCd = 0;
		if (++dustCd >= 8) {
			dustCd = 0;
			float fx = player.facingRight ? player.rect.x : (player.rect.x + player.rect.w);
			spawnDust(fx, player.rect.y + player.rect.h, player.facingRight ? -1.0f : 1.0f, 1);
		}
	}

	// Hazard check (spikes) — lose a life
	if (player.invulnTimer > 0) { /* immune */ }
	else if (checkHazards()) { loseLife(); return; }

	// Coin magnet: pull nearby coins toward player (only if line-of-sight is clear)
	{
		float pcx = player.rect.x + player.rect.w * 0.5f;
		float pcy = player.rect.y + player.rect.h * 0.5f;
		for (int i = 0; i < numCoins; i++) {
			if (!coins[i].active) continue;
			float ccx = coins[i].rect.x + coins[i].rect.w * 0.5f;
			float ccy = coins[i].rect.y + coins[i].rect.h * 0.5f;
			float dx = pcx - ccx, dy = pcy - ccy;
			float dist = sqrtf(dx * dx + dy * dy);
			if (dist < COIN_MAGNET_RANGE && dist > 1.0f) {
				// Raycast from coin to player; skip if any solid tile blocks the path
				bool blocked = false;
				int steps = (int)(dist / 4.0f) + 1; // check every 4px for precision
				for (int s = 1; s < steps; s++) {
					float t = (float)s / steps;
					float sx = ccx + dx * t;
					float sy = ccy + dy * t;
					int tx = (int)(sx / TILE_SIZE);
					int ty = (int)(sy / TILE_SIZE);
					if (isSolid(getTile(tx, ty))) { blocked = true; break; }
				}
				if (!blocked) {
					float pull = COIN_MAGNET_SPEED * (1.0f - dist / COIN_MAGNET_RANGE);
					coins[i].rect.x += (dx / dist) * pull;
					coins[i].rect.y += (dy / dist) * pull;
				}
			}
		}
	}

	// Collect coins
	for (int i = 0; i < numCoins; i++) {
		if (!coins[i].active) continue;
		if (rectsOverlap(player.rect, coins[i].rect)) {
			coins[i].active = false;
			player.score++;
			playSfx(SFX_COIN);
		}
	}

	// Fell off the map
	if (player.rect.y > getActiveMapHeight() * TILE_SIZE + 64) { loseLife(); return; }

	updateCheckpoints();

	// Level complete: painted win zone, or legacy right-edge fallback if no win tiles
	{
		bool won = false;
		if (levelHasWinZone) {
			int leftTile   = (int)(player.rect.x / TILE_SIZE);
			int rightTile  = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
			int topTile    = (int)(player.rect.y / TILE_SIZE);
			int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);
			for (int ty = topTile; ty <= bottomTile && !won; ty++) {
				for (int tx = leftTile; tx <= rightTile; tx++) {
					if (isWinTile(tx, ty)) { won = true; break; }
				}
			}
		} else if (player.rect.x + player.rect.w >= getActiveMapWidth() * TILE_SIZE) {
			won = true;
		}
		if (won) {
			triggerLevelComplete();
			return;
		}
	}

	// Warp tile check — player touching a warp tile triggers level warp
	if (warpTargetLevel >= 0) {
		int leftTile   = (int)(player.rect.x / TILE_SIZE);
		int rightTile  = (int)((player.rect.x + player.rect.w - 0.01f) / TILE_SIZE);
		int topTile    = (int)(player.rect.y / TILE_SIZE);
		int bottomTile = (int)((player.rect.y + player.rect.h - 0.01f) / TILE_SIZE);
		for (int ty = topTile; ty <= bottomTile; ty++) {
			for (int tx = leftTile; tx <= rightTile; tx++) {
				if (isWarpTile(tx, ty)) {
					if (!devMode && warpTargetLevel >= 0 && warpTargetLevel < TOTAL_LEVELS) {
						levelUnlocked[warpTargetLevel] = true;
						saveProgress();
					}
					currentLevel = warpTargetLevel;
					initLevel();
					startFade(false, 0);
					return;
				}
			}
		}
	}
}

static void updateCoins() {
	for (int i = 0; i < numCoins; i++) {
		if (!coins[i].active) continue;
		coins[i].bobTimer += 0.05f;
	}
}

static void enemySpriteBounds(const Enemy& e, float& spriteTop, float& spriteBottom) {
	spriteBottom = e.rect.y + e.rect.h;
	spriteTop = spriteBottom - TILE_SIZE;
}

static bool playerDescendingOntoEnemy() {
	if (player.groundPound || animGpoundLandTimer > 0) return true;
	if (player.vy > 0.05f) return true;
	float prevFeet = player.rect.y + player.rect.h - player.vy;
	float feet = player.rect.y + player.rect.h;
	return prevFeet < feet - 0.01f;
}

static bool playerCanStompEnemy(const Enemy& e) {
	float spriteTop, spriteBottom;
	enemySpriteBounds(e, spriteTop, spriteBottom);

	float prevFeet = player.rect.y + player.rect.h - player.vy;
	float feet = player.rect.y + player.rect.h;
	float head = player.rect.y;

	float stompBottom = spriteTop + TILE_SIZE * 0.55f;
	float marginX = e.stationary ? 6.0f : 4.0f;
	Rect stompRect = {
		e.rect.x + marginX,
		spriteTop - 2.0f,
		e.rect.w - marginX * 2.0f,
		stompBottom - spriteTop + 10.0f
	};
	if (!rectsOverlap(player.rect, stompRect)) return false;
	if (!playerDescendingOntoEnemy()) return false;

	bool cameFromAbove = prevFeet <= stompBottom + 12.0f;
	bool feetNearTop = feet >= spriteTop - 6.0f && feet <= spriteBottom + 10.0f;
	float enemyMid = spriteTop + TILE_SIZE * 0.45f;
	bool mostlyAbove = head + 8.0f < enemyMid + 14.0f;

	return cameFromAbove && feetNearTop && mostlyAbove;
}

static bool playerSideHitEnemy(const Enemy& e) {
	if (playerCanStompEnemy(e)) return false;

	float spriteTop, spriteBottom;
	enemySpriteBounds(e, spriteTop, spriteBottom);

	float hurtTop = spriteTop + TILE_SIZE * 0.3f;
	Rect hurtRect = {
		e.rect.x + 10.0f,
		hurtTop,
		e.rect.w - 20.0f,
		spriteBottom - hurtTop + 2.0f
	};
	return rectsOverlap(player.rect, hurtRect);
}

static void updateEnemies() {
	for (int i = 0; i < numEnemies; i++) {
		Enemy& e = enemies[i];
		if (!e.active) continue;

		// --- Gravity ---
		e.vy += ENEMY_GRAVITY;
		if (e.vy > 10.0f) e.vy = 10.0f;

		// --- Animation ---
		e.animTimer++;

		// --- Horizontal patrol (skip for stationary enemies) ---
		bool reversed = false;
		if (!e.stationary) {
			float speed = e.movingRight ? ENEMY_SPEED : -ENEMY_SPEED;
			e.rect.x += speed;

			// --- Horizontal tile collision: reverse on wall ---
			{
				int tx = e.movingRight
					? (int)((e.rect.x + e.rect.w + 1.0f) / TILE_SIZE)
					: (int)((e.rect.x - 1.0f) / TILE_SIZE);
				int topRow    = (int)(e.rect.y / TILE_SIZE);
				int bottomRow = (int)((e.rect.y + e.rect.h - 0.01f) / TILE_SIZE);
				for (int ty = topRow; ty <= bottomRow; ty++) {
					if (isSolid(getTile(tx, ty))) {
						e.movingRight = !e.movingRight;
						e.rect.x -= speed;
						reversed = true;
						break;
					}
				}
			}

			// --- Edge detection: reverse if no ground ahead (skip if already reversed) ---
			if (!reversed && e.onGround) {
				// Probe from the leading half of the enemy (not past its edge)
				float halfW = e.rect.w * 0.5f;
				float centerX = e.rect.x + halfW;
				float probeX = e.movingRight ? (centerX + halfW * 0.5f) : (centerX - halfW * 0.5f);
				int   probeCol = (int)(probeX / TILE_SIZE);
				int   groundRow = (int)((e.rect.y + e.rect.h + 1.0f) / TILE_SIZE);
				if (!isSolid(getTile(probeCol, groundRow))) {
					e.movingRight = !e.movingRight;
				}
			}
		}

		// --- Vertical movement + ground collision ---
		e.rect.y += e.vy;
		e.onGround = false;
		{
			int leftCol   = (int)(e.rect.x / TILE_SIZE);
			int rightCol  = (int)((e.rect.x + e.rect.w - 0.01f) / TILE_SIZE);
			int topRow    = (int)(e.rect.y / TILE_SIZE);
			int bottomRow = (int)((e.rect.y + e.rect.h - 0.01f) / TILE_SIZE);
			for (int ty = topRow; ty <= bottomRow; ty++) {
				for (int tx = leftCol; tx <= rightCol; tx++) {
					if (!isSolid(getTile(tx, ty))) continue;
					float tileY = ty * TILE_SIZE;
					if (e.vy > 0) {
						e.rect.y = tileY - e.rect.h;
						e.vy = 0;
						e.onGround = true;
					} else if (e.vy < 0) {
						e.rect.y = tileY + TILE_SIZE;
						e.vy = 0;
					}
				}
			}
		}

		// --- Kill if fell off map ---
		if (e.rect.y > getActiveMapHeight() * TILE_SIZE + 64) {
			e.active = false;
			continue;
		}

		// --- Player interaction (stomp vs side hit) ---
		if (playerCanStompEnemy(e)) {
			float spriteTop, spriteBottom;
			enemySpriteBounds(e, spriteTop, spriteBottom);
			e.active = false;
			stompCombo++;
			player.score += stompCombo;
			playSfx(SFX_STOMP);
			spawnDust(e.rect.x + e.rect.w * 0.5f, e.rect.y, 0.0f, 2 + stompCombo);
			spawnFlatten(e.rect.x + e.rect.w * 0.5f - TILE_SIZE * 0.5f,
				e.rect.y + e.rect.h - TILE_SIZE * 0.3f, !e.movingRight);
			spawnPopup(e.rect.x + e.rect.w * 0.5f, e.rect.y - 16.0f, stompCombo);

			if (player.groundPound) {
				triggerShake(2.0f + stompCombo * 0.5f, 6 + stompCombo);
			} else {
				bool holdingJump = ((hidKeysHeld() & (KEY_A | KEY_B)) != 0) || player.jumpBuffer > 0;
				float bounce = holdingJump ? STOMP_SUPER : STOMP_BOUNCE;
				float comboBonus = STOMP_COMBO_BONUS * fminf(stompCombo - 1, 4);
				player.vy = bounce + comboBonus;
				player.jumpHeld = holdingJump;
				player.onGround = false;
				player.rect.y = spriteTop - player.rect.h;
				triggerShake(1.5f + stompCombo * 0.5f, 4 + stompCombo);
				player.scaleX = 0.75f;
				player.scaleY = 1.3f;
				player.squashTimer = 6;
			}
		} else if (playerSideHitEnemy(e) && player.invulnTimer <= 0) {
			loseLife();
			return;
		}
	}
}

static void updateCamera() {
	// Lookahead: bias toward facing direction
	float lookTarget = player.facingRight ? CAM_LOOK_AHEAD : -CAM_LOOK_AHEAD;
	camera.lookAheadX += (lookTarget - camera.lookAheadX) * CAM_LOOK_SPEED;

	float targetX = player.rect.x - TOP_WIDTH / 2.0f + player.rect.w / 2.0f + camera.lookAheadX;
	camera.x += (targetX - camera.x) * 0.1f;
	if (camera.x < 0.0f) camera.x = 0.0f;
	float maxCamX = (float)getActiveMapWidth() * TILE_SIZE - TOP_WIDTH;
	if (maxCamX > 0 && camera.x > maxCamX) camera.x = maxCamX;

	// Vertical: dead zone — only move if player exits the zone
	float playerCenterY = player.rect.y + player.rect.h * 0.5f;
	float camCenterY = camera.y + TOP_HEIGHT * 0.4f;
	float diff = playerCenterY - camCenterY;
	if (fabsf(diff) > CAM_DEAD_ZONE_Y) {
		float sign = diff > 0 ? 1.0f : -1.0f;
		float targetY = playerCenterY - TOP_HEIGHT * 0.4f - sign * CAM_DEAD_ZONE_Y;
		camera.y += (targetY - camera.y) * 0.1f;
	}
	if (camera.y < 0.0f) camera.y = 0.0f;
	float maxCamY = getActiveMapHeight() * TILE_SIZE - TOP_HEIGHT;
	if (maxCamY > 0 && camera.y > maxCamY) camera.y = maxCamY;
}

// ========================================================
// Player animation
// ========================================================
static void updateAnimation() {
	bool moving = fabsf(player.vx) > 0.5f;

	// Ground pound landing: start timer when landing from gpound
	// (detected by squashTimer starting + was falling fast)
	// We piggyback on the squash timer for timing the splat sprite

	// Idle animation: cycle between idle1 and idle2
	if (!moving && player.onGround) {
		animIdleTimer++;
		animRunInitTimer = 0;
		animRunTimer = 0;
		animWasMoving = false;
	} else {
		animIdleTimer = 0;
	}

	// Run init: brief "turning head" frame when starting to move
	if (moving && player.onGround && !animWasMoving) {
		animRunInitTimer = 8; // show init frame for 8 frames
		animRunFrame = 0;
		animRunTimer = 0;
	}
	if (animRunInitTimer > 0) animRunInitTimer--;
	animWasMoving = moving;

	// Run cycle: step1 -> mid -> step2 -> mid (4-frame loop)
	if (moving && player.onGround && animRunInitTimer <= 0) {
		animRunTimer++;
		int spd = (fabsf(player.vx) > 4.0f) ? 4 : 6;
		if (animRunTimer >= spd) {
			animRunTimer = 0;
			animRunFrame = (animRunFrame + 1) % 4;
		}
	}

	// Ground pound land timer — hold DOWN to keep the splat
	if (animGpoundLandTimer > 0) {
		if (hidKeysHeld() & KEY_DDOWN)
			animGpoundLandTimer = 12; // keep it active
		else
			animGpoundLandTimer--;
	}
}

static int getPlayerSprite() {
	// Priority order: death > gpound land > gpound fall > dash > wall slide > run > idle

	// Ground pound landing splat
	if (animGpoundLandTimer > 0)
		return PSPR_GPOUND_LAND;

	// Ground pound falling
	if (player.groundPound)
		return PSPR_GPOUND_FALL;

	// Dashing
	if (player.dashing)
		return PSPR_DASH;

	// Wall sliding
	if (!player.onGround && (player.wallLeft || player.wallRight) && player.vy > 0)
		return PSPR_WALL_SLIDE;

	// Airborne (not special)
	if (!player.onGround) {
		bool dirHeld = (hidKeysHeld() & (KEY_LEFT | KEY_RIGHT)) != 0;
		if (!dirHeld) {
			// Neutral: jump sprite going up, fall sprite going down
			return (player.vy < 0) ? PSPR_JUMP_NEUTRAL : PSPR_GPOUND_FALL;
		}
		return PSPR_RUN_MID; // directional — legs together
	}

	// Run init
	if (animRunInitTimer > 0)
		return PSPR_RUN_INIT;

	// Running
	if (fabsf(player.vx) > 0.5f && player.onGround) {
		// 4-frame cycle: step1, mid, step2, mid
		static const int runFrames[4] = { PSPR_RUN_STEP1, PSPR_RUN_MID, PSPR_RUN_STEP2, PSPR_RUN_MID };
		return runFrames[animRunFrame % 4];
	}

	// Idle
	int idleCycle = (animIdleTimer / 30) % 2; // swap every 30 frames (~0.5s)
	return idleCycle == 0 ? PSPR_IDLE1 : PSPR_IDLE2;
}

// ========================================================
// Rendering
// ========================================================

// Screen-bounds check: is a world-space rect visible after camera offset?
static inline bool onScreen(float wx, float wy, float w, float h, float cx, float cy) {
	float sx = wx - cx, sy = wy - cy;
	return sx + w > 0 && sx < TOP_WIDTH && sy + h > 0 && sy < TOP_HEIGHT;
}

// Frame time profiler (dev mode)
static u64 frameStartTick = 0;
static float frameTimeMs = 0.0f;

// Player shadow: raycast down to find ground distance
static float getShadowDist() {
	float footY = player.rect.y + player.rect.h;
	int col = (int)((player.rect.x + player.rect.w * 0.5f) / TILE_SIZE);
	int startRow = (int)(footY / TILE_SIZE);
	for (int ty = startRow; ty < getActiveMapHeight() && ty < startRow + 8; ty++) {
		u8 t = getTile(col, ty);
		if (isSolid(t)) return ty * TILE_SIZE - footY;
	}
	return 128.0f; // no ground found nearby
}

static void drawBgPattern(float cx, float cy) {
	// Tile the BG_DECOR image across the entire visible screen with parallax
	float bgCx = cx * BG_PARALLAX;
	float bgCy = cy * BG_PARALLAX;
	// Compute pixel offset within a tile for seamless wrapping
	float offX = fmodf(bgCx, (float)TILE_SIZE);
	float offY = fmodf(bgCy, (float)TILE_SIZE);
	if (offX < 0) offX += TILE_SIZE;
	if (offY < 0) offY += TILE_SIZE;
	// Snap to integer to avoid sub-pixel gaps between tiles
	offX = floorf(offX);
	offY = floorf(offY);

	int cols = (TOP_WIDTH + TILE_SIZE - 1) / TILE_SIZE + 1;
	int rows = (TOP_HEIGHT + TILE_SIZE - 1) / TILE_SIZE + 1;

	for (int r = 0; r < rows; r++) {
		for (int c = 0; c < cols; c++) {
			float drawX = c * TILE_SIZE - offX;
			float drawY = r * TILE_SIZE - offY;
			C2D_DrawImageAt(tileImg[TILE_BG_DECOR], drawX, drawY, 0.0f, NULL, 1.0f, 1.0f);
		}
	}
}

// Draw grass overlay tiles for one layer (alt=false → default grass; alt=true → alt grass).
// `gmap` is row-major with stride `mapW`. `altDepth` lets callers control where alt grass
// sits in the depth stack — game uses 1.0 (above player), menu uses 0.6 (below UI).
// If `wrapX` is true, columns are looked up modulo `mapW` (used by the menu torus).
static void drawGrassLayer(const u8* gmap, int mapW, int mapH, float cx, float cy, bool alt,
                           float altDepth = 1.0f, bool wrapX = false, int rowStride = 0) {
	if (rowStride <= 0) rowStride = mapW;
	int startCol = (int)floorf(cx / TILE_SIZE);
	int endCol   = (int)floorf((cx + TOP_WIDTH) / TILE_SIZE) + 1;
	int startRow = (int)floorf(cy / TILE_SIZE);
	int endRow   = (int)floorf((cy + TOP_HEIGHT) / TILE_SIZE) + 1;
	if (!wrapX) {
		if (startCol < 0) startCol = 0;
		if (endCol >= mapW) endCol = mapW - 1;
	}
	if (startRow < 0) startRow = 0;
	if (endRow >= mapH) endRow = mapH - 1;

	int frame = (grassAnimTimer / 20) & 1;
	C2D_Image defImg = grassImg[frame == 0 ? GRASS_DEF_A : GRASS_DEF_B];
	C2D_Image altImg = grassImg[frame == 0 ? GRASS_ALT_A : GRASS_ALT_B];
	float depth = alt ? altDepth : 0.05f;
	u8 wantKind = alt ? 2 : 1;

	for (int ty = startRow; ty <= endRow; ty++) {
		for (int tx = startCol; tx <= endCol; tx++) {
			int wx = tx;
			if (wrapX) {
				wx = tx % mapW;
				if (wx < 0) wx += mapW;
			}
			u8 g = gmap[ty * rowStride + wx];
			if (g != wantKind) continue;
			float drawX = tx * TILE_SIZE - cx;
			float drawY = ty * TILE_SIZE - cy;
			C2D_DrawImageAt(alt ? altImg : defImg, drawX, drawY, depth, NULL, 1.0f, 1.0f);
		}
	}
}

// mode: 0 = normal tiles only, 1 = 3D tiles only
static void drawTilemap(float cx, float cy, float eyeOff, int mode) {
	// Foreground tiles only (skip BG decoration)
	// Use wider culling range to account for 3D tile offset
	float maxOff = fabsf(eyeOff * DEPTH_TILES);
	int startCol = (int)((cx - maxOff) / TILE_SIZE);
	int endCol   = (int)((cx + TOP_WIDTH + maxOff) / TILE_SIZE) + 1;
	int startRow = (int)(cy / TILE_SIZE);
	int endRow   = (int)((cy + TOP_HEIGHT) / TILE_SIZE) + 1;

	if (startCol < 0) startCol = 0;
	if (startRow < 0) startRow = 0;
	int activeW = getActiveMapWidth();
	int activeH = getActiveMapHeight();
	if (endCol >= activeW) endCol = activeW - 1;
	if (endRow >= activeH) endRow = activeH - 1;
	if (endCol >= MAP_W) endCol = MAP_W - 1;
	if (endRow >= MAP_H) endRow = MAP_H - 1;

	for (int ty = startRow; ty <= endRow; ty++) {
		for (int tx = startCol; tx <= endCol; tx++) {
			u8 t = tilemap[ty][tx];
			if (t == TILE_EMPTY) continue;
			bool is3d = tile3dMap[ty][tx];
			if (mode == 0 && is3d) continue;   // normal pass: skip 3D tiles
			if (mode == 1 && !is3d) continue;  // 3D pass: skip normal tiles

			float drawX = tx * TILE_SIZE - cx;
			float drawY = ty * TILE_SIZE - cy;

			// 3D tiles get eye offset (pop forward)
			if (is3d) drawX += eyeOff * DEPTH_TILES;

			// Crumble shake offset (O(1) reverse-index lookup)
			if (t == TILE_CRUMBLE) {
				int ci = crumbleIndex[ty][tx];
				if (ci >= 0 && crumbles[ci].shaking) {
					drawX += (rand() % 5 - 2);
					drawY += (rand() % 3 - 1);
				}
			}

			float depth = is3d ? 0.7f : 0.0f;
			if (is3d) {
				u8 alpha = tile3dDrawAlpha(tx, ty);
				if (alpha < 255) {
					C2D_ImageTint tint;
					C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, alpha), 0.0f);
					C2D_DrawImageAt(tileImg[t], drawX, drawY, depth, &tint, 1.0f, 1.0f);
				} else {
					C2D_DrawImageAt(tileImg[t], drawX, drawY, depth, NULL, 1.0f, 1.0f);
				}
			} else {
				C2D_DrawImageAt(tileImg[t], drawX, drawY, depth, NULL, 1.0f, 1.0f);
			}

			// Draw warp overlay on top of the tile if this position has a warp
			if (warpMap[ty][tx]) {
				u8 warpAlpha = is3d ? tile3dDrawAlpha(tx, ty) : 255;
				if (warpAlpha < 255) {
					C2D_ImageTint tint;
					C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, warpAlpha), 0.0f);
					C2D_DrawImageAt(tileImg[TILE_WARP], drawX, drawY, depth + 0.01f, &tint, 1.0f, 1.0f);
				} else {
					C2D_DrawImageAt(tileImg[TILE_WARP], drawX, drawY, depth + 0.01f, NULL, 1.0f, 1.0f);
				}
			}
			if (winMap[ty][tx]) {
				C2D_DrawRectSolid(drawX + 4.0f, drawY + 4.0f, depth + 0.02f,
					TILE_SIZE - 8.0f, TILE_SIZE - 8.0f, C2D_Color32(255, 215, 0, 220));
				C2D_DrawRectSolid(drawX + 10.0f, drawY + 2.0f, depth + 0.03f,
					4.0f, 10.0f, C2D_Color32(255, 240, 120, 255));
			}
		}
	}
}

static void drawGame(C3D_RenderTarget* target, float eyeOff) {
	u32 clearClr = activeCaveBg ? C2D_Color32(0, 0, 0, 255) : CLR_BG;
	C2D_TargetClear(target, clearClr);
	C2D_SceneBegin(target);

	float cx = camera.x + shakeX;
	float cy = camera.y + shakeY;

	// Background: custom image tiled with horizontal parallax and slight vertical parallax
	if (caveBgImg.tex && caveBgImg.subtex) {
		float bgW = (float)caveBgImg.subtex->width;
		float bgH = (float)caveBgImg.subtex->height;
		float parallaxX = cx * BG_PARALLAX;
		float parallaxY = cy * 0.1f; // Slight vertical parallax (10% of camera movement)
		float offX = fmodf(parallaxX, bgW);
		if (offX < 0) offX += bgW;
		float offY = fmodf(parallaxY, bgH);
		if (offY < 0) offY += bgH;
		// Snap to integer to avoid sub-pixel gaps between tiles
		offX = floorf(offX);
		offY = floorf(offY);
		// Draw enough copies to fill screen (both horizontally and vertically)
		for (float dx = -offX; dx < TOP_WIDTH; dx += bgW) {
			for (float dy = -offY; dy < TOP_HEIGHT; dy += bgH) {
				C2D_DrawImageAt(caveBgImg, dx, dy, 0.0f, NULL, 1.0f, 1.0f);
			}
		}
	} else {
		C2D_DrawRectSolid(-eyeOff * DEPTH_BG, 0.0f, 0.0f,
			(float)TOP_WIDTH + fabsf(eyeOff * DEPTH_BG) * 2.0f, (float)TOP_HEIGHT,
			CLR_BG);
		drawBgPattern(cx - eyeOff * DEPTH_BG, cy);
	}

	// Foreground tilemap — normal tiles first (player draws on top of these)
	drawTilemap(cx, cy, eyeOff, 0);
	drawCheckpoints(cx, cy);

	// Default grass overlay — sits above the ground tile but behind player/enemies
	drawGrassLayer(&grassMap[0][0], getActiveMapWidth(), getActiveMapHeight(), cx, cy, false, 1.0f, false, MAP_W);

	// Moving platforms
	for (int i = 0; i < numMovers; i++) {
		const MovingPlatform& m = movers[i];
		if (!onScreen(m.x, m.y, TILE_SIZE, TILE_SIZE, cx, cy)) continue;
		C2D_DrawImageAt(tileImg[m.tileType], m.x - cx, m.y - cy, 0.0f, NULL, 1.0f, 1.0f);
	}

	// Coins / cheezits (sprite with glow pulse)
	{
		float pulse = (sinf(grassAnimTimer * 0.08f) + 1.0f) * 0.5f; // 0..1
		for (int i = 0; i < numCoins; i++) {
			if (!coins[i].active) continue;
			const Rect& r = coins[i].rect;
			if (!onScreen(r.x, r.y - 3.0f, r.w, r.h + 6.0f, cx, cy)) continue;
			float bobY = sinf(coins[i].bobTimer) * 3.0f;
			// Draw cheezit sprite centered on hitbox
			float drawX = r.x + r.w * 0.5f - TILE_SIZE * 0.5f - cx;
			float drawY = r.y + r.h * 0.5f - TILE_SIZE * 0.5f - cy + bobY;
			// Glow: tint the sprite brighter on pulse
			C2D_ImageTint coinTint;
			u8 tintVal = (u8)(220 + 35 * pulse);
			C2D_PlainImageTint(&coinTint, C2D_Color32(255, tintVal, tintVal, 255), 0.15f * pulse);
			C2D_DrawImageAt(coinImg, drawX, drawY, 0.5f, &coinTint, 1.0f, 1.0f);
		}
	}

	// Enemies (sprite-based)
	for (int i = 0; i < numEnemies; i++) {
		const Enemy& e = enemies[i];
		if (!e.active) continue;
		const Rect& r = e.rect;
		if (!onScreen(r.x - 8, r.y - 4, r.w + 16, r.h + 4, cx, cy)) continue;

		// Pick walk frame: alternate every 10 frames
		int frame = (e.animTimer / 10) % ESPR_COUNT;

		// Center 32x32 sprite on the hitbox, bottom-aligned
		float hitCenterX = r.x + r.w * 0.5f;
		float hitBottom  = r.y + r.h;
		float drawX = hitCenterX - TILE_SIZE * 0.5f - cx;
		float drawY = hitBottom - TILE_SIZE - cy;

		// Flip via UV swap (sprite faces right by default)
		C2D_Image img = enemyImg[frame];
		Tex3DS_SubTexture flipped;
		if (!e.movingRight) {
			flipped = *img.subtex;
			float tmp = flipped.left;
			flipped.left = flipped.right;
			flipped.right = tmp;
			img.subtex = &flipped;
		}

		C2D_DrawImageAt(img, drawX, drawY, 0.8f, NULL, 1.0f, 1.0f);
	}

	// VFX particles
	for (int i = 0; i < MAX_VFX; i++) {
		const VfxParticle& p = vfx[i];
		if (!p.active) continue;
		if (!onScreen(p.x, p.y, TILE_SIZE * p.scale, TILE_SIZE * p.scale, cx, cy)) continue;
		float drawX = p.x - cx;
		float drawY = p.y - cy;
		float s = p.scale;
		float scaleX = p.flipX ? -s : s;
		if (p.flipX) drawX += TILE_SIZE * s;
		C2D_ImageTint tint;
		C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, (u8)(p.alpha * 255)), 0.0f);
		C2D_DrawImageAt(vfxImg[p.imgId], drawX, drawY, 0.9f, &tint, scaleX, s);
	}

	// Dust particles
	for (int i = 0; i < MAX_DUST; i++) {
		if (!dust[i].active) continue;
		if (!onScreen(dust[i].x, dust[i].y, dust[i].size, dust[i].size, cx, cy)) continue;
		float alpha = (float)dust[i].timer / 14.0f;
		if (alpha > 1.0f) alpha = 1.0f;
		u8 a = (u8)(alpha * 180);
		C2D_DrawRectSolid(dust[i].x - cx, dust[i].y - cy, 0.85f,
			dust[i].size, dust[i].size, C2D_Color32(220, 210, 190, a));
	}

	// Enemy flatten FX (squished enemy sprite)
	for (int i = 0; i < MAX_FLATTEN; i++) {
		if (!flattenFx[i].active) continue;
		if (!onScreen(flattenFx[i].x, flattenFx[i].y, TILE_SIZE, TILE_SIZE, cx, cy)) continue;
		float fAlpha = (float)flattenFx[i].timer / 12.0f;
		C2D_Image fimg = enemyImg[0];
		Tex3DS_SubTexture fflip;
		if (flattenFx[i].flipX) {
			fflip = *fimg.subtex;
			float tmp = fflip.left;
			fflip.left = fflip.right;
			fflip.right = tmp;
			fimg.subtex = &fflip;
		}
		C2D_ImageTint ftint;
		C2D_PlainImageTint(&ftint, C2D_Color32(255, 255, 255, (u8)(fAlpha * 255)), 0.0f);
		C2D_DrawImageAt(fimg, flattenFx[i].x - cx, flattenFx[i].y - cy, 0.85f,
			&ftint, 1.0f, 0.3f); // squished vertically
	}

	// Stomp combo popups
	C2D_TextBufClear(textBuf);
	for (int i = 0; i < MAX_POPUPS; i++) {
		if (!popups[i].active) continue;
		if (!onScreen(popups[i].x - 16, popups[i].y - 8, 32, 16, cx, cy)) continue;
		float pAlpha = (float)popups[i].timer / 40.0f;
		u8 pa = (u8)(pAlpha * 255);
		char pbuf[16];
		snprintf(pbuf, sizeof(pbuf), "+%d", popups[i].value);
		u32 pclr = C2D_Color32(255, 255, 100, pa);
		C2D_Text ptext;
		C2D_TextParse(&ptext, textBuf, pbuf);
		C2D_TextOptimize(&ptext);
		C2D_DrawText(&ptext, C2D_WithColor, popups[i].x - cx - 8.0f, popups[i].y - cy, 0.95f,
			0.6f, 0.6f, pclr);
	}

	// Player shadow (dark ellipse projected on ground below)
	if (gameState != STATE_DYING || deathTimer > DEATH_POP_TIME) {
		float shadowDist = getShadowDist();
		if (shadowDist < 96.0f) {
			float t = 1.0f - shadowDist / 96.0f; // 1 when on ground, 0 when far
			float shadowW = player.rect.w * (0.6f + 0.4f * t);
			float shadowH = 4.0f * t;
			float shadowX = player.rect.x + player.rect.w * 0.5f - shadowW * 0.5f - cx;
			float shadowY = player.rect.y + player.rect.h + shadowDist - shadowH * 0.5f - cy;
			u8 shadowA = (u8)(80 * t);
			C2D_DrawRectSolid(shadowX, shadowY, 0.05f, shadowW, shadowH, C2D_Color32(0, 0, 0, shadowA));
		}
	}

	// Landing impact flash
	if (landingFlashTimer > 0) {
		float flashAlpha = (float)landingFlashTimer / 4.0f;
		float flashW = 20.0f + (4 - landingFlashTimer) * 4.0f;
		float flashH = 4.0f;
		C2D_DrawRectSolid(landingFlashX - flashW * 0.5f - cx, landingFlashY - flashH - cy, 0.88f,
			flashW, flashH, C2D_Color32(255, 255, 255, (u8)(flashAlpha * 120)));
	}

	// Player (sprite-based with squash/stretch and invuln flash)
	if (gameState != STATE_DYING || deathTimer > DEATH_POP_TIME) {
		// Hide player during pop phase of death (we draw death ghost instead)
		bool visible = true;
		if (player.invulnTimer > 0 && (player.invulnTimer / 3) % 2 == 0)
			visible = false; // flash
		if (visible) {
			const Rect& r = player.rect;
			float sx = player.scaleX;
			float sy = player.scaleY;

			int sprIdx = getPlayerSprite();

			// Determine facing: wall slide faces toward the wall
			bool faceRight = player.facingRight;
			if (sprIdx == PSPR_WALL_SLIDE) {
				faceRight = player.wallRight; // wallRight = wall to right = face right into it
			}

			// Sprite is 32x32, hitbox is 16x28
			// Center sprite horizontally on hitbox, align bottom of sprite with bottom of hitbox
			float sprW = TILE_SIZE * sx;
			float sprH = TILE_SIZE * sy;
			float hitCenterX = r.x + r.w * 0.5f;
			float hitBottom  = r.y + r.h;

			float drawX2 = hitCenterX - sprW * 0.5f - cx;
			float drawY2 = hitBottom - sprH - cy;

			// Wall slide: align sprite legs (10px from edge) against the wall
			// Sprite facing right: legs at pixel (32-10)=22 from left edge
			// Sprite facing left (flipped): legs at pixel 10 from left edge
			if (sprIdx == PSPR_WALL_SLIDE) {
				if (player.wallRight) {
					// Wall to right, face right: legs touch r.x + r.w
					drawX2 = (r.x + r.w) - (TILE_SIZE - 10.0f) * sx - cx;
				} else {
					// Wall to left, face left (flipped): legs touch r.x
					drawX2 = r.x - 10.0f * sx - cx;
				}
			}

			// Flip via UV swap instead of negative scale — guarantees centering
			C2D_Image img = playerImg[sprIdx];
			Tex3DS_SubTexture flipped;
			if (!faceRight) {
				flipped = *img.subtex;
				float tmp = flipped.left;
				flipped.left = flipped.right;
				flipped.right = tmp;
				img.subtex = &flipped;
			}

			C2D_DrawImageAt(img, drawX2, drawY2, 0.6f, NULL, sx, sy);
		}
	}

	// Death animation ghost (tinted sprite)
	if (gameState == STATE_DYING && deathTimer <= DEATH_POP_TIME) {
		u8 a = (u8)((float)deathTimer / DEATH_POP_TIME * 255);
		float hitCenterX = deathX + player.rect.w * 0.5f;
		float sprDrawX = hitCenterX - TILE_SIZE * 0.5f - cx;
		float sprDrawY = deathY + player.rect.h - TILE_SIZE - cy;
		C2D_Image dimg = playerImg[PSPR_RUN_MID];
		Tex3DS_SubTexture dflip;
		if (!player.facingRight) {
			dflip = *dimg.subtex;
			float tmp = dflip.left;
			dflip.left = dflip.right;
			dflip.right = tmp;
			dimg.subtex = &dflip;
		}
		C2D_ImageTint deathTint;
		C2D_PlainImageTint(&deathTint, C2D_Color32(255, 170, 0, a), 1.0f);
		C2D_DrawImageAt(dimg, sprDrawX, sprDrawY, 1.0f, &deathTint, 1.0f, 1.0f);
	}

	// Flush before 3D tiles to ensure correct alpha blending with player
	C2D_Flush();

	// 3D tiles — drawn after player so they appear in front
	drawTilemap(cx, cy, eyeOff, 1);

	// Flush before alt grass to ensure correct alpha blending with enemies
	C2D_Flush();

	// Alt grass overlay — drawn in front of player/enemies
	drawGrassLayer(&grassMap[0][0], getActiveMapWidth(), getActiveMapHeight(), cx, cy, true, 0.85f, false, MAP_W);


	// Fade overlay
	if (fadeTimer > 0) {
		float t = (float)fadeTimer / FADE_FRAMES;
		float alpha = fadingOut ? (1.0f - t) : t;
		u8 a = (u8)(alpha * 255);
		C2D_DrawRectSolid(0, 0, 1.5f, TOP_WIDTH, TOP_HEIGHT, C2D_Color32(0, 0, 0, a));
	}
}

// ========================================================
// Bottom screen rendering helpers
// ========================================================
static void drawText(float x, float y, float scaleX, float scaleY, u32 color, const char* str) {
	C2D_Text text;
	C2D_TextParse(&text, textBuf, str);
	C2D_TextOptimize(&text);
	C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, scaleX, scaleY, color);
}

static void drawTextZ(float x, float y, float z, float scaleX, float scaleY, u32 color, const char* str) {
	C2D_Text text;
	C2D_TextParse(&text, textBuf, str);
	C2D_TextOptimize(&text);
	C2D_DrawText(&text, C2D_WithColor, x, y, z, scaleX, scaleY, color);
}

static void drawMinimap(float ox, float oy, float scale) {
	// Static cache (prebuilt in initLevel): all non-empty, non-crumble tiles.
	for (int i = 0; i < minimapCacheCount; i++) {
		const MinimapRect& r = minimapCache[i];
		C2D_DrawRectSolid(ox + r.tx * scale, oy + r.ty * scale, 0.3f, scale, scale, r.color);
	}
	// Crumbles (dynamic: skip fallen ones).
	u32 crumbleClr = C2D_Color32(180, 140, 60, 200);
	for (int i = 0; i < numCrumbles; i++) {
		const CrumbleTile& c = crumbles[i];
		if (c.fallen) continue;
		C2D_DrawRectSolid(ox + c.tx * scale, oy + c.ty * scale, 0.3f, scale, scale, crumbleClr);
	}
	// Coins
	for (int i = 0; i < numCoins; i++) {
		if (!coins[i].active) continue;
		float cx2 = ox + (coins[i].rect.x / TILE_SIZE) * scale;
		float cy2 = oy + (coins[i].rect.y / TILE_SIZE) * scale;
		C2D_DrawRectSolid(cx2, cy2, 0.4f, scale, scale, C2D_Color32(0, 220, 255, 255));
	}
	// Moving platforms
	for (int i = 0; i < numMovers; i++) {
		const MovingPlatform& m = movers[i];
		float mx = ox + (m.x / TILE_SIZE) * scale;
		float my = oy + (m.y / TILE_SIZE) * scale;
		C2D_DrawRectSolid(mx, my, 0.4f, scale, scale, C2D_Color32(155, 89, 182, 200));
	}
	// Player dot
	float px = ox + (player.rect.x / TILE_SIZE) * scale;
	float py = oy + (player.rect.y / TILE_SIZE) * scale;
	C2D_DrawRectSolid(px, py, 0.5f, scale * 1.5f, scale * 1.5f, C2D_Color32(0, 170, 255, 255));
}

static void drawBottomScreen(C3D_RenderTarget* bot) {
	C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(bot);

	C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);

	C2D_TextBufClear(textBuf);

	char buf[64];
	snprintf(buf, sizeof(buf), "Lives: %d", player.lives);
	drawText(20.0f, 200.0f, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255), buf);

	snprintf(buf, sizeof(buf), "Crackers: %d", player.score);
	drawText(110.0f, 200.0f, 0.6f, 0.6f, C2D_Color32(255, 255, 200, 255), buf);

	// Speedrun timer
	float secs = getTimerSeconds();
	int mins = (int)(secs / 60.0f);
	float remSecs = secs - mins * 60.0f;
	snprintf(buf, sizeof(buf), "%d:%05.2f", mins, remSecs);
	drawText(200.0f, 200.0f, 0.6f, 0.6f, C2D_Color32(180, 255, 180, 255), buf);

	// Frame time (dev mode only)
	if (devMode) {
		snprintf(buf, sizeof(buf), "%.1f ms (%.0f fps)", frameTimeMs, frameTimeMs > 0 ? 1000.0f / frameTimeMs : 0);
		drawText(20.0f, 8.0f, 0.5f, 0.5f, C2D_Color32(0, 255, 0, 200), buf);
	}

	// Minimap (per-level toggle)
	if (levelAllowsMinimap) {
		float mapScale = (float)(BOT_WIDTH - 20) / (float)getActiveMapWidth();
		if (mapScale > 10.0f) mapScale = 10.0f;
		float mapW = (float)getActiveMapWidth() * mapScale;
		float mapH = (float)getActiveMapHeight() * mapScale;
		float mapX = (BOT_WIDTH - mapW) * 0.5f;
		float mapY = 200.0f - mapH - 5.0f;
		C2D_DrawRectSolid(mapX - 1, mapY - 1, 0.1f, mapW + 2, mapH + 2, C2D_Color32(40, 40, 60, 200));
		drawMinimap(mapX, mapY, mapScale);
	}
}

static void drawGameOverScreen(C3D_RenderTarget* bot) {
	C2D_TargetClear(bot, C2D_Color32(20, 10, 10, 255));
	C2D_SceneBegin(bot);

	C2D_TextBufClear(textBuf);

	drawText(95.0f,  50.0f, 1.0f, 1.0f, C2D_Color32(255, 60, 60, 255), "GAME OVER");
	drawText(65.0f,  90.0f, 0.6f, 0.6f, C2D_Color32(200, 200, 200, 255), "You ran out of lives!");

	char buf[64];
	snprintf(buf, sizeof(buf), "Final Score: %d", player.score);
	drawText(90.0f, 120.0f, 0.7f, 0.7f, C2D_Color32(255, 255, 200, 255), buf);

	drawText(80.0f, 170.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press A to restart");
	drawText(80.0f, 190.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press START to exit");
}

static void drawWinScreen(C3D_RenderTarget* bot) {
	C2D_TargetClear(bot, C2D_Color32(10, 20, 10, 255));
	C2D_SceneBegin(bot);

	C2D_TextBufClear(textBuf);

	bool isLastHidden = (currentLevel == LAST_HIDDEN_LEVEL);
	bool isLastVisible = (currentLevel == LAST_VISIBLE_LEVEL);

	if (isLastHidden) {
		drawText(65.0f, 40.0f, 1.0f, 1.0f, C2D_Color32(255, 215, 0, 255), "YOU WIN!");
		drawText(55.0f, 75.0f, 0.6f, 0.6f, C2D_Color32(200, 200, 200, 255), "All levels complete!");
	} else if (isLastVisible) {
		drawText(65.0f, 40.0f, 1.0f, 1.0f, C2D_Color32(200, 200, 255, 255), "You win?");
		drawText(55.0f, 75.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Or is there more..?");
	} else {
		drawText(75.0f, 40.0f, 1.0f, 1.0f, C2D_Color32(80, 255, 80, 255), "LEVEL COMPLETE!");
	}

	char buf[64];
	snprintf(buf, sizeof(buf), "Score: %d", player.score);
	drawText(105.0f, 100.0f, 0.7f, 0.7f, C2D_Color32(255, 255, 200, 255), buf);

	snprintf(buf, sizeof(buf), "Lives remaining: %d", player.lives);
	drawText(80.0f, 125.0f, 0.7f, 0.7f, C2D_Color32(200, 200, 200, 255), buf);

	// Time + star rating
	float t2 = getTimerSeconds();
	int tm = (int)(t2 / 60.0f);
	float ts = t2 - tm * 60.0f;
	float par = parTimes[currentLevel];
	int stars = 1;
	if (t2 <= par) stars = 3;
	else if (t2 <= par * 1.5f) stars = 2;
	char starStr[4] = "   ";
	for (int si = 0; si < stars; si++) starStr[si] = '*';
	starStr[stars] = '\0';
	snprintf(buf, sizeof(buf), "Time: %d:%05.2f  %s", tm, ts, starStr);
	drawText(70.0f, 148.0f, 0.6f, 0.6f, C2D_Color32(255, 215, 0, 255), buf);

	if (isLastHidden) {
		drawText(70.0f, 175.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press A to play again");
	} else if (isLastVisible) {
		drawText(70.0f, 175.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press A to continue");
	} else {
		drawText(70.0f, 175.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press A for next level");
	}
	drawText(70.0f, 195.0f, 0.6f, 0.6f, C2D_Color32(180, 180, 180, 255), "Press START to exit");
}

static bool isLevelListedInSelect(int i) {
	if (i < 0 || i >= LEVEL_COUNT) return false;
	if (!LEVEL_INFO[i].isHidden) return true;
	return levelUnlocked[i];
}

static void advanceLevelSelectCursor(int delta) {
	if (LEVEL_COUNT <= 0) return;
	int tries = 0;
	do {
		levelSelectCursor += delta;
		if (levelSelectCursor >= LEVEL_COUNT) levelSelectCursor = 0;
		if (levelSelectCursor < 0) levelSelectCursor = LEVEL_COUNT - 1;
		if (isLevelListedInSelect(levelSelectCursor)) return;
		tries++;
	} while (tries < LEVEL_COUNT);
}

static void drawLevelSelectScreen(C3D_RenderTarget* top, C3D_RenderTarget* bot) {
	C2D_TargetClear(top, C2D_Color32(20, 20, 40, 255));
	C2D_SceneBegin(top);

	C2D_TextBufClear(textBuf);
	drawText(110.0f, 20.0f, 1.0f, 1.0f, C2D_Color32(255, 215, 0, 255), "SELECT LEVEL");

	// Build dynamic world layout from LEVEL_INFO if available, or use defaults
	struct WorldInfo { int startIdx; int count; const char* name; u32 color; };
	WorldInfo worlds[4]; // Max 4 worlds
	int numWorlds = 0;

	// Check if LEVEL_INFO exists and use it for dynamic world layout
	// Otherwise fall back to hardcoded 6+4 split
	if (LEVEL_COUNT > 0) {
		int currentWorld = -1;
		for (int i = 0; i < LEVEL_COUNT && numWorlds < 4; i++) {
			if (!isLevelListedInSelect(i)) continue;
			int lvlWorld = LEVEL_INFO[i].world;
			if (lvlWorld != currentWorld) {
				// Start new world
				worlds[numWorlds].startIdx = i;
				worlds[numWorlds].count = 1;
				currentWorld = lvlWorld;
				if (lvlWorld == 0) {
					worlds[numWorlds].name = "???";
					worlds[numWorlds].color = C2D_Color32(255, 100, 200, 255);
				} else {
					static char wbuf[4][16];
					snprintf(wbuf[numWorlds], 16, "World %d", lvlWorld);
					worlds[numWorlds].name = wbuf[numWorlds];
					// Alternate colors for worlds
					worlds[numWorlds].color = (numWorlds % 2 == 0)
						? C2D_Color32(100, 200, 255, 255)
						: C2D_Color32(255, 150, 100, 255);
				}
				numWorlds++;
			} else {
				// Continue current world
				worlds[numWorlds - 1].count++;
			}
		}
	}

	// Fallback to hardcoded layout if no worlds detected
	if (numWorlds == 0) {
		static constexpr int W1_COUNT = 6;
		int w2Count = NUM_LEVELS - W1_COUNT;
		if (w2Count < 0) w2Count = 0;
		worlds[0] = { 0, W1_COUNT, "World 1", C2D_Color32(100, 200, 255, 255) };
		if (w2Count > 0) {
			worlds[1] = { W1_COUNT, w2Count, "World 2", C2D_Color32(255, 150, 100, 255) };
			numWorlds = 2;
		} else {
			numWorlds = 1;
		}
	}

	// Build row mapping: figure out which row the cursor is on
	float itemH = 50.0f;
	float listTop = 70.0f;
	float listBot = 200.0f;

	// Calculate actual row for selected level
	int cursorRow = 0;
	for (int w = 0; w < numWorlds; w++) {
		if (levelSelectCursor >= worlds[w].startIdx && levelSelectCursor < worlds[w].startIdx + worlds[w].count) {
			// +1 for world header for each preceding world + header + position within world
			cursorRow = w + 1; // headers
			for (int pw = 0; pw < w; pw++) cursorRow += worlds[pw].count;
			cursorRow += (levelSelectCursor - worlds[w].startIdx);
			break;
		}
	}

	float cursorY = listTop + cursorRow * itemH;
	float targetScroll = levelSelectScroll;
	if (cursorY - targetScroll > listBot) targetScroll = cursorY - listBot;
	if (cursorY - targetScroll < listTop) targetScroll = cursorY - listTop;
	if (targetScroll < 0.0f) targetScroll = 0.0f;
	if (fabsf(targetScroll - levelSelectScroll) < 1.0f)
		levelSelectScroll = targetScroll;
	else
		levelSelectScroll += (targetScroll - levelSelectScroll) * 0.25f;

	int row = 0;
	// Draw each world
	for (int w = 0; w < numWorlds; w++) {
		float wy = listTop + row * itemH - levelSelectScroll;
		if (wy >= 45.0f && wy <= 215.0f) {
			char header[32];
			snprintf(header, sizeof(header), "--- %s ---", worlds[w].name);
			drawText(110.0f, wy, 0.6f, 0.6f, worlds[w].color, header);
		}
		row++;
		for (int l = 0; l < worlds[w].count; l++) {
			int i = worlds[w].startIdx + l;
			float y = listTop + row * itemH - levelSelectScroll;
			row++;
			if (y < 45.0f || y > 215.0f) continue;
			bool sel = (i == levelSelectCursor);
			u32 clr = levelUnlocked[i]
				? (sel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(200, 200, 200, 255))
				: (sel ? C2D_Color32(150, 150, 150, 255) : C2D_Color32(100, 100, 100, 255));
			char buf[80]; char stars[8] = "";
			if (starRatings[i] > 0) { for (int s2 = 0; s2 < starRatings[i] && s2 < 3; s2++) stars[s2] = '*'; stars[starRatings[i]] = '\0'; }

			// Use LEVEL_INFO display name if available, otherwise generate
			const char* displayName = (i < LEVEL_COUNT) ? LEVEL_INFO[i].displayName : "???";

			if (levelUnlocked[i]) {
				if (bestTimes[i] > 0.01f) { int m=(int)(bestTimes[i]/60.0f); float s=bestTimes[i]-m*60.0f; snprintf(buf,sizeof(buf),"%s%s  [%d:%05.2f] %s",sel?"> ":"  ",displayName,m,s,stars); }
				else snprintf(buf,sizeof(buf),"%s%s",sel?"> ":"  ",displayName);
			} else snprintf(buf,sizeof(buf),"%s%s  [LOCKED]",sel?"> ":"  ",displayName);
			drawText(80.0f, y, 0.7f, 0.7f, clr, buf);
		}
	}

	drawText(60.0f, 218.0f, 0.5f, 0.5f, C2D_Color32(150, 150, 150, 255), "D-Pad to select, A to play, B to go back");

	C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(bot);
	C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
}

// ========================================================
// Menu scene update + render
// ========================================================
static void initMenuScene() {
	buildMenuScene();
	populateMenuGrass();
	menuRunnerX = menuRunnerStartX;
	menuRunnerY = menuRunnerStartY;
	menuRunnerVy = 0.0f;
	menuRunnerOnGround = false;
	menuCamX = 0.0f;
	menuRunnerAnimTimer = 0;
	menuRunnerFrame = 0;
}

static void updateMenuScene() {
	const float RUNNER_W = 16.0f;
	const float RUNNER_H = 28.0f;
	const float RUNNER_SPEED = 2.2f;

	// --- Vertical: gravity + tile collision ---
	menuRunnerVy += GRAVITY_FALL;
	if (menuRunnerVy > MAX_FALL_SPD) menuRunnerVy = MAX_FALL_SPD;
	menuRunnerY += menuRunnerVy;

	bool grounded = false;
	{
		int tL = (int)(menuRunnerX / TILE_SIZE);
		int tR = (int)((menuRunnerX + RUNNER_W - 0.01f) / TILE_SIZE);
		int tT = (int)(menuRunnerY / TILE_SIZE);
		int tB = (int)((menuRunnerY + RUNNER_H - 0.01f) / TILE_SIZE);
		for (int ty = tT; ty <= tB && !grounded; ty++) {
			for (int tx = tL; tx <= tR; tx++) {
				if (!menuTileSolid(tx, ty)) continue;
				float tileY = ty * TILE_SIZE;
				if (menuRunnerVy > 0) {
					menuRunnerY = tileY - RUNNER_H;
					menuRunnerVy = 0;
					grounded = true;
				} else if (menuRunnerVy < 0) {
					menuRunnerY = tileY + TILE_SIZE;
					menuRunnerVy = 0;
				}
				break;
			}
		}
	}
	menuRunnerOnGround = grounded;

	// --- Horizontal: always run right; jump if blocked or about to fall in a pit ---
	menuRunnerX += RUNNER_SPEED;

	// Wall ahead?
	bool blocked = false;
	{
		int tL = (int)(menuRunnerX / TILE_SIZE);
		int tR = (int)((menuRunnerX + RUNNER_W - 0.01f) / TILE_SIZE);
		int tT = (int)(menuRunnerY / TILE_SIZE);
		int tB = (int)((menuRunnerY + RUNNER_H - 0.01f) / TILE_SIZE);
		for (int ty = tT; ty <= tB && !blocked; ty++) {
			for (int tx = tL; tx <= tR; tx++) {
				u8 t = getMenuTile(tx, ty);
				if (t == TILE_GROUND || t == TILE_FILL || t == TILE_PLATFORM || t == TILE_CRUMBLE) {
					menuRunnerX = tx * TILE_SIZE - RUNNER_W;
					blocked = true;
					break;
				}
			}
		}
	}

	// Auto-jump when grounded and either a wall or a gap is up ahead.
	// Lookahead is generous so the jump triggers early — feels more natural.
	if (menuRunnerOnGround && menuRunnerVy >= 0) {
		const float LOOKAHEAD = 28.0f;
		int aheadX = (int)((menuRunnerX + RUNNER_W + LOOKAHEAD) / TILE_SIZE);
		int feetY  = (int)((menuRunnerY + RUNNER_H + 2.0f) / TILE_SIZE);
		bool gapAhead = true;
		for (int dy = 0; dy < 3; dy++) {
			if (menuTileSolid(aheadX, feetY + dy)) { gapAhead = false; break; }
		}
		// Wall ahead within lookahead range (any solid intersecting the runner's vertical band)?
		bool wallAhead = false;
		int tT = (int)(menuRunnerY / TILE_SIZE);
		int tB = (int)((menuRunnerY + RUNNER_H - 0.01f) / TILE_SIZE);
		for (int ty = tT; ty <= tB && !wallAhead; ty++) {
			if (menuTileSolid(aheadX, ty)) wallAhead = true;
		}
		if (blocked || wallAhead || gapAhead) {
			// Strong enough to clear up to a 3-tile-tall wall (~96 px) under GRAVITY_FALL.
			menuRunnerVy = -12.5f;
			menuRunnerOnGround = false;
		}
	}

	// Seamless horizontal wrap: subtract one world width when the runner crosses the right edge.
	// Tile lookups and rendering both wrap modulo MENU_MAP_W so the visual is continuous.
	const float MENU_WORLD_W = (float)(MENU_MAP_W * TILE_SIZE);
	if (menuRunnerX >= MENU_WORLD_W) {
		menuRunnerX -= MENU_WORLD_W;
	}
	// Fell off the bottom — respawn at start position.
	if (menuRunnerY > MENU_MAP_H * TILE_SIZE + 64) {
		menuRunnerX = menuRunnerStartX;
		menuRunnerY = menuRunnerStartY;
		menuRunnerVy = 0.0f;
	}

	// Camera follows runner, centered on the runner's body. No clamping (torus world).
	const float RUNNER_W_CENTER = 16.0f;
	menuCamX = menuRunnerX + RUNNER_W_CENTER * 0.5f - TOP_WIDTH * 0.5f;

	// Run animation cycle
	menuRunnerAnimTimer++;
	if (menuRunnerAnimTimer >= 5) {
		menuRunnerAnimTimer = 0;
		menuRunnerFrame = (menuRunnerFrame + 1) % 4;
	}
}

// Vertical offset so the bottom of the 16-row menu world aligns with the bottom of the screen.
static constexpr float MENU_CAM_Y = (float)(MENU_MAP_H * TILE_SIZE - TOP_HEIGHT);

static void drawMenuTilemap(float cx) {
	// Camera can be any value (torus world); use floored cx so columns wrap correctly.
	int startCol = (int)floorf(cx / TILE_SIZE);
	int endCol   = (int)floorf((cx + TOP_WIDTH) / TILE_SIZE) + 1;
	for (int ty = 0; ty < MENU_MAP_H; ty++) {
		for (int tx = startCol; tx <= endCol; tx++) {
			int wx = tx % MENU_MAP_W;
			if (wx < 0) wx += MENU_MAP_W;
			u8 t = menuTilemap[ty][wx];
			if (t == TILE_EMPTY) continue;
			float drawX = tx * TILE_SIZE - cx;
			float drawY = ty * TILE_SIZE - MENU_CAM_Y;
			C2D_DrawImageAt(tileImg[t], drawX, drawY, 0.0f, NULL, 1.0f, 1.0f);
		}
	}
}

static void drawMenuScene(C3D_RenderTarget* target) {
	C2D_TargetClear(target, CLR_BG);
	C2D_SceneBegin(target);

	float cx = menuCamX;

	// Sky background
	C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, (float)TOP_WIDTH, (float)TOP_HEIGHT, CLR_BG);

	// BG decoration parallax
	drawBgPattern(cx, 0.0f);

	// Foreground tilemap
	drawMenuTilemap(cx);

	// Default grass (behind runner)
	drawGrassLayer(&menuGrassMap[0][0], MENU_MAP_W, MENU_MAP_H, cx, MENU_CAM_Y, false, 1.0f, true);

	// Runner sprite — pick run-cycle frame; flip not needed (always faces right)
	static const int runFrames[4] = { PSPR_RUN_STEP1, PSPR_RUN_MID, PSPR_RUN_STEP2, PSPR_RUN_MID };
	int sprIdx = menuRunnerOnGround ? runFrames[menuRunnerFrame] :
	             (menuRunnerVy < 0 ? PSPR_JUMP_NEUTRAL : PSPR_GPOUND_FALL);
	float drawX = menuRunnerX - cx - 8.0f; // center 32px sprite on 16px hitbox
	float drawY = menuRunnerY + 28.0f - TILE_SIZE - MENU_CAM_Y; // align bottom
	C2D_DrawImageAt(playerImg[sprIdx], drawX, drawY, 0.4f, NULL, 1.0f, 1.0f);

	// Alt grass (in front of runner; below the menu UI which sits at depth >= 0.8)
	drawGrassLayer(&menuGrassMap[0][0], MENU_MAP_W, MENU_MAP_H, cx, MENU_CAM_Y, true, 0.6f, true);
}

static void drawMainMenu(C3D_RenderTarget* top, C3D_RenderTarget* bot) {
	// Top screen: scene already drawn by drawMenuScene; overlay UI on top.
	C2D_SceneBegin(top);
	C2D_TextBufClear(textBuf);

	if (!inSlotSelect) {
		// Title + button image matching the current selection
		// mainMenuCursor: 0=Load, 1=New, 2=Settings
		C2D_DrawImageAt(titleImg, 0.0f, 0.0f, 0.93f, NULL, 1.0f, 1.0f);
		C2D_DrawImageAt(menuBtnImg[mainMenuCursor], 0.0f, 0.0f, 0.95f, NULL, 1.0f, 1.0f);
	} else {
		// Slot selection overlay on top of scene
		// Dim overlay
		C2D_DrawRectSolid(0, 0, 0.9f, TOP_WIDTH, TOP_HEIGHT, C2D_Color32(0, 0, 0, 150));
		const char* title = slotSelectIsLoad ? "LOAD GAME" : "NEW GAME";
		drawTextZ(130.0f, 75.0f, 0.95f, 0.8f, 0.8f, C2D_Color32(200, 200, 200, 255), title);

		for (int s = 0; s < 3; s++) {
			bool sel = (slotCursor == s);
			u32 clr = sel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(180, 180, 180, 255);
			if (slotSelectIsLoad && !slotHasData[s])
				clr = C2D_Color32(80, 80, 80, 255);
			char buf[48];
			if (slotHasData[s]) {
				snprintf(buf, sizeof(buf), "%sSlot %d  [%d%%]", sel ? "> " : "  ", s + 1, slotProgress[s]);
			} else {
				snprintf(buf, sizeof(buf), "%sSlot %d  [Empty]", sel ? "> " : "  ", s + 1);
			}
			drawTextZ(110.0f, 110.0f + s * 35.0f, 0.95f, 0.7f, 0.7f, clr, buf);
		}
		drawTextZ(105.0f, 218.0f, 0.95f, 0.5f, 0.5f, C2D_Color32(200, 200, 200, 255), "A to confirm, B to go back");
	}

	C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
	C2D_SceneBegin(bot);
	C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
}

static void drawPauseOverlay(C3D_RenderTarget* target) {
	// Draw on top of the game scene (called after drawGame)
	C2D_SceneBegin(target);
	C2D_TextBufClear(textBuf);
	// Dim overlay (above player/enemies/alt grass which are at 1.0f)
	C2D_DrawRectSolid(0, 0, 0.95f, TOP_WIDTH, TOP_HEIGHT, C2D_Color32(0, 0, 0, 150));
	drawTextZ(145.0f, 60.0f, 0.99f, 1.0f, 1.0f, C2D_Color32(255, 215, 0, 255), "PAUSED");

	const char* labels[PAUSE_ITEM_COUNT] = { "Resume", "Settings", "Quit" };
	for (int i = 0; i < PAUSE_ITEM_COUNT; i++) {
		bool sel = (pauseCursor == i);
		u32 clr = sel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(180, 180, 180, 255);
		char buf[32];
		snprintf(buf, sizeof(buf), "%s%s", sel ? "> " : "  ", labels[i]);
		drawTextZ(130.0f, 110.0f + i * 35.0f, 0.99f, 0.7f, 0.7f, clr, buf);
	}
}

static void drawSettings(C3D_RenderTarget* target) {
	C2D_SceneBegin(target);
	C2D_TextBufClear(textBuf);
	// Dim backdrop (above player/enemies/alt grass)
	C2D_DrawRectSolid(0, 0, 0.95f, TOP_WIDTH, TOP_HEIGHT, C2D_Color32(0, 0, 0, 180));

	drawTextZ(135.0f, 18.0f, 0.99f, 1.0f, 1.0f, C2D_Color32(255, 215, 0, 255), "SETTINGS");

	const char* labels[SETTINGS_ITEM_COUNT] = {
		"Music",
		"Sound Effects",
		"Sprint Mode",
		"Controls",
		"Developer",
		"Back",
	};
	char valueBufs[4][16];
	snprintf(valueBufs[0], sizeof(valueBufs[0]), "%s", g_settings.muteMusic ? "OFF" : "ON");
	snprintf(valueBufs[1], sizeof(valueBufs[1]), "%s", g_settings.muteSfx   ? "OFF" : "ON");
	snprintf(valueBufs[2], sizeof(valueBufs[2]), "%s", g_settings.sprintToggle ? "TOGGLE" : "HOLD");
	snprintf(valueBufs[3], sizeof(valueBufs[3]), "%s", g_settings.easyControls ? "EASY" : "NORMAL");

	for (int i = 0; i < SETTINGS_ITEM_COUNT; i++) {
		bool sel = (settingsCursor == i);
		u32 clr = sel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(200, 200, 200, 255);
		char prefix[4]; snprintf(prefix, sizeof(prefix), "%s", sel ? "> " : "  ");
		char line[48];
		if (i < 4) {
			snprintf(line, sizeof(line), "%s%s", prefix, labels[i]);
			drawTextZ(60.0f, 55.0f + i * 28.0f, 0.99f, 0.7f, 0.7f, clr, line);
			// Value column on the right
			drawTextZ(260.0f, 55.0f + i * 28.0f, 0.99f, 0.7f, 0.7f, clr, valueBufs[i]);
		} else {
			snprintf(line, sizeof(line), "%s%s", prefix, labels[i]);
			drawTextZ(60.0f, 55.0f + i * 28.0f, 0.99f, 0.7f, 0.7f, clr, line);
		}
	}

	drawTextZ(60.0f, 218.0f, 0.99f, 0.45f, 0.45f, C2D_Color32(180, 180, 180, 255),
		"D-Pad navigate, A/LEFT/RIGHT change, B back");
}

static void drawDevSettings(C3D_RenderTarget* target) {
	C2D_SceneBegin(target);
	C2D_TextBufClear(textBuf);
	// Dim backdrop (above player/enemies/alt grass)
	C2D_DrawRectSolid(0, 0, 0.95f, TOP_WIDTH, TOP_HEIGHT, C2D_Color32(0, 0, 0, 180));

	drawTextZ(120.0f, 18.0f, 0.99f, 1.0f, 1.0f, C2D_Color32(255, 100, 100, 255), "DEVELOPER");

	const char* labels[DEV_SETTINGS_ITEM_COUNT] = {
		"Level Select",
		"Reset Save Progress",
		"Back",
	};

	for (int i = 0; i < DEV_SETTINGS_ITEM_COUNT; i++) {
		bool sel = (devSettingsCursor == i);
		u32 clr = sel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(200, 200, 200, 255);
		const char* prefix = sel ? "> " : "  ";
		char line[48];
		snprintf(line, sizeof(line), "%s%s", prefix, labels[i]);
		drawTextZ(60.0f, 65.0f + i * 32.0f, 0.99f, 0.7f, 0.7f, clr, line);
	}

	if (devResetConfirm) {
		drawTextZ(60.0f, 160.0f, 0.99f, 0.6f, 0.6f, C2D_Color32(255, 80, 80, 255),
			"Are you sure? A = Yes, B = No");
	}

	drawTextZ(60.0f, 218.0f, 0.99f, 0.45f, 0.45f, C2D_Color32(180, 180, 180, 255),
		"A to select, B to go back");
}

// Parse raw dialogue text up to rawLimit chars, producing display text (no markup).
// If yellowFlags is non-null, sets each byte to 1 for chars inside ** markers (yellow).
// Returns number of display chars written.
static int parseDialogueUpTo(const char* src, int rawLimit, char* dst, int dstSize, u8* yellowFlags = nullptr) {
	int si = 0, di = 0;
	bool inBold = false;
	while (src[si] && si < rawLimit && di < dstSize - 1) {
		if (src[si] == '*' && src[si+1] == '*') {
			si += 2; // skip **
			inBold = !inBold;
		}
		else if (src[si] == '{' && src[si+1] == 'p' && src[si+2] == ':') {
			while (src[si] && src[si] != '}') si++;
			if (src[si] == '}') si++;
		}
		else {
			if (yellowFlags) yellowFlags[di] = inBold ? 1 : 0;
			dst[di++] = src[si++];
		}
	}
	dst[di] = '\0';
	return di;
}

// Check if raw text at position si is a {p:N} pause. Returns N if yes, 0 if not.
static int checkPause(const char* src, int si) {
	if (src[si] == '{' && src[si+1] == 'p' && src[si+2] == ':') {
		int val = 0;
		int i = si + 3;
		while (src[i] >= '0' && src[i] <= '9') { val = val * 10 + (src[i] - '0'); i++; }
		if (src[i] == '}') return val > 0 ? val : 1;
	}
	return 0;
}

// Draw dialogue scene: gray spotlight BG, idle ant sprite, text box with typewriter
static void drawDialogue(C3D_RenderTarget* target, float eyeOff, const char* text) {
	C2D_TargetClear(target, C2D_Color32(20, 20, 20, 255));
	C2D_SceneBegin(target);
	C2D_TextBufClear(textBuf);

	// Gray spotlight gradient background
	float spotX = TOP_WIDTH / 2.0f;
	float spotY = TOP_HEIGHT - 50.0f;
	// Draw a soft gray circle (layered rects for gradient)
	for (int ring = 5; ring >= 0; ring--) {
		float radius = 40.0f + ring * 20.0f;
		u8 brightness = (u8)(60 - ring * 8);
		C2D_DrawRectSolid(spotX - radius, spotY - radius * 0.7f, 0.05f,
			radius * 2, radius * 1.4f, C2D_Color32(brightness, brightness, brightness, 255));
	}

	// Draw idle ant sprite at bottom center
	int idleCycle = ((int)(osGetTime() / 500)) % 2;
	int sprIdx = idleCycle == 0 ? PSPR_IDLE1 : PSPR_IDLE2;
	float sprScale = 2.0f;
	float sprW = TILE_SIZE * sprScale;
	float sprH = TILE_SIZE * sprScale;
	float sprDrawX = (TOP_WIDTH - sprW) / 2.0f;
	float sprDrawY = TOP_HEIGHT - sprH - 16.0f;
	C2D_DrawImageAt(playerImg[sprIdx], sprDrawX, sprDrawY, 0.3f, NULL, sprScale, sprScale);

	// Parse raw text up to current typewriter position, stripping markup
	char visibleText[MAX_DIALOGUE_CHARS];
	u8 yellowFlags[MAX_DIALOGUE_CHARS];
	memset(yellowFlags, 0, sizeof(yellowFlags));
	int visLen = parseDialogueUpTo(text, currentDialogueChar, visibleText, sizeof(visibleText), yellowFlags);

	// Text box background
	float boxW = 360.0f;
	float boxH = 80.0f;
	float boxX = (TOP_WIDTH - boxW) / 2.0f;
	float boxY = sprDrawY - boxH - 8.0f;
	C2D_DrawRectSolid(boxX, boxY, 0.4f, boxW, boxH, C2D_Color32(10, 10, 30, 230));
	C2D_DrawRectSolid(boxX + 2, boxY + 2, 0.41f, boxW - 4, boxH - 4, C2D_Color32(30, 30, 50, 220));

	// Word-wrap visible text into lines, then center and draw with color segments
	float textScale = 0.5f;
	float maxLineW = boxW - 24.0f;
	float lineH = 14.0f;
	u32 whiteClr = C2D_Color32(255, 255, 255, 255);
	u32 yellowClr = C2D_Color32(255, 255, 80, 255);

	// Step 1: break visibleText into lines via word wrap
	struct Line { int start; int len; };
	Line lines[16]; int numLines = 0;
	int lineStart = 0;
	int lastSpace = -1;
	float lineW = 0;
	for (int ci = 0; ci < visLen && numLines < 16; ci++) {
		if (visibleText[ci] == '\n') {
			lines[numLines++] = { lineStart, ci - lineStart };
			lineStart = ci + 1;
			lastSpace = -1;
			lineW = 0;
			continue;
		}
		if (visibleText[ci] == ' ') lastSpace = ci;
		// Measure char width
		char ch[2] = { visibleText[ci], '\0' };
		C2D_Text chText;
		C2D_TextParse(&chText, textBuf, ch);
		C2D_TextOptimize(&chText);
		float cw = 0, ch2 = 0;
		C2D_TextGetDimensions(&chText, textScale, textScale, &cw, &ch2);
		lineW += cw;
		if (lineW > maxLineW && ci > lineStart) {
			if (lastSpace > lineStart) {
				lines[numLines++] = { lineStart, lastSpace - lineStart };
				lineStart = lastSpace + 1;
			} else {
				lines[numLines++] = { lineStart, ci - lineStart };
				lineStart = ci;
			}
			lastSpace = -1;
			lineW = 0;
			// Re-measure from new lineStart to ci
			for (int ri = lineStart; ri <= ci; ri++) {
				char rc[2] = { visibleText[ri], '\0' };
				C2D_Text rt;
				C2D_TextParse(&rt, textBuf, rc);
				C2D_TextOptimize(&rt);
				float rw = 0, rh = 0;
				C2D_TextGetDimensions(&rt, textScale, textScale, &rw, &rh);
				lineW += rw;
			}
		}
	}
	if (lineStart < visLen && numLines < 16)
		lines[numLines++] = { lineStart, visLen - lineStart };

	// Step 2: calculate total text height and vertical centering
	float totalTextH = numLines * lineH;
	float textStartY = boxY + (boxH - totalTextH) * 0.5f;

	// Step 3: draw each line centered, with color segments
	for (int li = 0; li < numLines; li++) {
		int lStart = lines[li].start;
		int lLen = lines[li].len;
		float curY = textStartY + li * lineH;

		// Measure full line width for centering
		char lineBuf[MAX_DIALOGUE_CHARS];
		memcpy(lineBuf, visibleText + lStart, lLen);
		lineBuf[lLen] = '\0';
		C2D_Text fullLineText;
		C2D_TextParse(&fullLineText, textBuf, lineBuf);
		C2D_TextOptimize(&fullLineText);
		float fullW = 0, fullH = 0;
		C2D_TextGetDimensions(&fullLineText, textScale, textScale, &fullW, &fullH);

		float lineX = boxX + (boxW - fullW) * 0.5f;

		// Draw color segments within this line
		int si = 0;
		float curX = lineX;
		while (si < lLen) {
			int se = si;
			u8 segClr = yellowFlags[lStart + si];
			while (se < lLen && yellowFlags[lStart + se] == segClr) se++;
			char seg[MAX_DIALOGUE_CHARS];
			int sLen = se - si;
			memcpy(seg, visibleText + lStart + si, sLen);
			seg[sLen] = '\0';
			C2D_Text segText;
			C2D_TextParse(&segText, textBuf, seg);
			C2D_TextOptimize(&segText);
			float sw = 0, sh = 0;
			C2D_TextGetDimensions(&segText, textScale, textScale, &sw, &sh);
			u32 clr = segClr ? yellowClr : whiteClr;
			C2D_DrawText(&segText, C2D_WithColor, curX, curY, 0.9f, textScale, textScale, clr);
			curX += sw;
			si = se;
		}
	}

	// Draw "Press A" indicator when waiting for input
	if (dialogueWaitingForInput) {
		C2D_Text aText;
		C2D_TextParse(&aText, textBuf, "A >");
		C2D_TextOptimize(&aText);
		C2D_DrawText(&aText, C2D_WithColor, boxX + boxW - 40.0f, boxY + boxH - 18.0f, 0.9f, 0.45f, 0.45f,
			C2D_Color32(255, 255, 100, 255));
	}
}

// ========================================================
// Main
// ========================================================
int main(int argc, char* argv[]) {
	gfxInitDefault();
	gfxSet3D(true); // enable stereoscopic 3D on top screen
	loadSettings();
	initSfx();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	romfsInit();

	C3D_RenderTarget* top     = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	C3D_RenderTarget* bot     = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	// Load spritesheet (includes tileset + bottom menu)
	C2D_SpriteSheet spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	// Extract individual tile images from the tileset
	initTileImages(spriteSheet);
	initMusic();

	// Get bottom menu image
	bottomMenuImg = C2D_SpriteSheetGetImage(spriteSheet, SPR_BOTTOM_MENU);
	menuBtnImg[0] = C2D_SpriteSheetGetImage(spriteSheet, SPR_MENU_LOAD);
	menuBtnImg[1] = C2D_SpriteSheetGetImage(spriteSheet, SPR_MENU_NEW);
	menuBtnImg[2] = C2D_SpriteSheetGetImage(spriteSheet, SPR_MENU_SETTINGS);
	titleImg = C2D_SpriteSheetGetImage(spriteSheet, SPR_TITLE);
	caveBgImg = C2D_SpriteSheetGetImage(spriteSheet, SPR_CAVE_BG);
	caveBg2Img = C2D_SpriteSheetGetImage(spriteSheet, SPR_CAVE_BG2);

	textBuf = C2D_TextBufNew(512);

	probeSlots();
	clearSessionCheckpoints();
	initMenuScene();

	while (aptMainLoop()) {
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		u32 kUp   = hidKeysUp();

		// Frame time measurement
		u64 now = svcGetSystemTick();
		if (frameStartTick > 0)
			frameTimeMs = (float)(now - frameStartTick) / (float)(SYSCLOCK_ARM11 / 1000);
		frameStartTick = now;

		float slider = osGet3DSliderState();
		g_3dSlider = slider; // cache for collision checks
		// Threshold: below this, skip the right-eye render entirely. Saves ~half the top-screen work.
		const float STEREO_THRESHOLD = 0.05f;
		bool stereoActive = slider > STEREO_THRESHOLD;
		float eyeOff = stereoActive ? slider * 1.0f : 0.0f;

		// Grass animation tick (state-independent)
		grassAnimTimer++;

        // ---- Main Menu ----
        if (gameState == STATE_MAIN_MENU) {
            if (kDown & KEY_START) break; // exit game

            if (!inSlotSelect) {
                // mainMenuCursor: 0=Load, 1=New, 2=Settings (left to right)
                if (kDown & KEY_RIGHT) { mainMenuCursor++; if (mainMenuCursor > 2) mainMenuCursor = 0; }
                if (kDown & KEY_LEFT) { mainMenuCursor--; if (mainMenuCursor < 0) mainMenuCursor = 2; }
                if (kDown & KEY_A) {
                    if (mainMenuCursor == 2) {
                        // Settings
                        settingsReturnState = STATE_MAIN_MENU;
                        settingsCursor = 0;
                        gameState = STATE_SETTINGS;
                    } else {
                        devMode = false;
                        inSlotSelect = true;
                        slotSelectIsLoad = (mainMenuCursor == 0); // 0=Load
                        slotCursor = 0;
                        probeSlots();
                    }
                }
            } else {
                if (kDown & KEY_DOWN) { slotCursor++; if (slotCursor > 2) slotCursor = 0; }
                if (kDown & KEY_UP) { slotCursor--; if (slotCursor < 0) slotCursor = 2; }
                if (kDown & KEY_B) { inSlotSelect = false; }
                if (kDown & KEY_A) {
                    if (slotSelectIsLoad) {
                        // Load: only if slot has data
                        if (slotHasData[slotCursor]) {
                            currentSlot = slotCursor;
                            resetProgress();
                            loadProgress();
                            levelSelectCursor = 0;
                            levelSelectScroll = 0.0f;
                            inSlotSelect = false;
                            gameState = STATE_LEVEL_SELECT;
                        }
                    } else {
                        // New Game: pick slot, reset, save fresh data
                        currentSlot = slotCursor;
                        resetProgress();
                        saveProgress();
                        levelSelectCursor = 0;
                        levelSelectScroll = 0.0f;
                        inSlotSelect = false;
                        gameState = STATE_LEVEL_SELECT;
                    }
                }
            }

			updateMusic();
			updateMenuScene();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawMenuScene(top);
			drawMainMenu(top, bot);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Level Select ----
		if (gameState == STATE_LEVEL_SELECT) {
			if (kDown & KEY_START) break; // exit game
			if (kDown & KEY_B) {
				gameState = STATE_MAIN_MENU;
				mainMenuCursor = 0;
				inSlotSelect = false;
				continue;
			}
			if (kDown & KEY_DOWN) advanceLevelSelectCursor(1);
			if (kDown & KEY_UP) advanceLevelSelectCursor(-1);
			if ((kDown & KEY_A) && levelUnlocked[levelSelectCursor]) {
				currentLevel = levelSelectCursor;
				initLevel();
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawLevelSelectScreen(top, bot);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Game Over ----
		if (gameState == STATE_GAMEOVER) {
			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawGame(top, eyeOff);
			if (stereoActive) drawGame(topRight, -eyeOff);
			drawGameOverScreen(bot);
			C3D_FrameEnd(0);

			if (kDown & (KEY_A | KEY_B)) {
				if (devMode) { player.lives = 99; }
				gameState = STATE_LEVEL_SELECT;
			}
			continue;
		}

		// ---- Win ----
		if (gameState == STATE_WIN) {
			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawGame(top, eyeOff);
			if (stereoActive) drawGame(topRight, -eyeOff);
			drawWinScreen(bot);
			C3D_FrameEnd(0);

			if (kDown & (KEY_A | KEY_B)) {
				// Save best time + star rating
				float t = getTimerSeconds();
				timerRunning = false;
				if (!devMode && currentLevel < TOTAL_LEVELS && (bestTimes[currentLevel] < 0.01f || t < bestTimes[currentLevel]))
					bestTimes[currentLevel] = t;
				// Star rating: 3 = under par, 2 = under 1.5x par, 1 = completed
				if (!devMode && currentLevel < TOTAL_LEVELS && bestTimes[currentLevel] > 0.01f) {
					float par = parTimes[currentLevel];
					if (bestTimes[currentLevel] <= par) starRatings[currentLevel] = 3;
					else if (bestTimes[currentLevel] <= par * 1.5f) starRatings[currentLevel] = 2;
					else starRatings[currentLevel] = 1;
				}

				if (currentLevel < LAST_VISIBLE_LEVEL) {
					levelUnlocked[currentLevel + 1] = true;
					if (!devMode) saveProgress();
					int savedLives = player.lives;
					int savedScore = player.score;
					currentLevel++;
					initLevel();
					player.lives = savedLives;
					player.score = savedScore;
					startFade(false, 0);
				} else if (currentLevel == LAST_VISIBLE_LEVEL) {
					if (LAST_HIDDEN_LEVEL < TOTAL_LEVELS)
						levelUnlocked[LAST_HIDDEN_LEVEL] = true;
					if (!devMode) saveProgress();
					gameState = STATE_LEVEL_SELECT;
				} else {
					if (!devMode) saveProgress();
					gameState = STATE_LEVEL_SELECT;
				}
			}
			continue;
		}

		// ---- Dying (death animation) ----
		if (gameState == STATE_DYING) {
			deathTimer--;
			if (deathTimer > DEATH_POP_TIME) {
				// Freeze phase — do nothing
			} else {
				// Pop up phase
				deathVy += 0.3f;
				deathY += deathVy;
			}
			updateShake();

			if (deathTimer <= 0) {
				startFade(true, 1); // fade out, then respawn
				gameState = STATE_PLAYING; // temporary, will respawn on fade end
				doRespawn();
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawGame(top, eyeOff);
			if (stereoActive) drawGame(topRight, -eyeOff);
			drawBottomScreen(bot);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Paused ----
		if (gameState == STATE_PAUSED) {
			if (kDown & KEY_DOWN) { pauseCursor++; if (pauseCursor >= PAUSE_ITEM_COUNT) pauseCursor = 0; }
			if (kDown & KEY_UP) { pauseCursor--; if (pauseCursor < 0) pauseCursor = PAUSE_ITEM_COUNT - 1; }
			if (kDown & (KEY_A | KEY_START)) {
				if (pauseCursor == 0) {
					// Resume
					gameState = STATE_PLAYING;
					timerPaused += svcGetSystemTick() - pauseBegin;
				} else if (pauseCursor == 1) {
					// Settings
					settingsReturnState = STATE_PAUSED;
					settingsCursor = 0;
					gameState = STATE_SETTINGS;
				} else if (pauseCursor == 2) {
					// Quit
					if (!devMode) saveProgress();
					gameState = STATE_LEVEL_SELECT;
					timerRunning = false;
				}
			}
			if (kDown & KEY_B) {
				// B also resumes
				gameState = STATE_PLAYING;
				timerPaused += svcGetSystemTick() - pauseBegin;
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawGame(top, eyeOff);
			drawPauseOverlay(top);
			if (stereoActive) {
				drawGame(topRight, -eyeOff);
				drawPauseOverlay(topRight);
			}
			drawBottomScreen(bot);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Settings ----
		if (gameState == STATE_SETTINGS) {
			if (kDown & KEY_B) {
				saveSettings();
				gameState = settingsReturnState;
			} else {
				if (kDown & KEY_UP)   { settingsCursor--; if (settingsCursor < 0) settingsCursor = SETTINGS_ITEM_COUNT - 1; }
				if (kDown & KEY_DOWN) { settingsCursor++; if (settingsCursor >= SETTINGS_ITEM_COUNT) settingsCursor = 0; }

				bool change = (kDown & (KEY_A | KEY_LEFT | KEY_RIGHT)) != 0;
				if (change) {
					switch (settingsCursor) {
						case 0:
							g_settings.muteMusic = !g_settings.muteMusic;
							applyMusicMute();
							break;
						case 1:
							g_settings.muteSfx = !g_settings.muteSfx;
							break;
						case 2:
							g_settings.sprintToggle = !g_settings.sprintToggle;
							// When switching to hold mode, clear any leftover toggled-on state.
							if (!g_settings.sprintToggle) player.sprinting = false;
							break;
						case 3:
							g_settings.easyControls = !g_settings.easyControls;
							break;
						case 4:
							// Developer sub-menu — only triggered by A.
							if (kDown & KEY_A) {
								devSettingsCursor = 0;
								devResetConfirm = false;
								saveSettings();
								gameState = STATE_DEV_SETTINGS;
							}
							break;
						case 5:
							if (kDown & KEY_A) {
								saveSettings();
								gameState = settingsReturnState;
							}
							break;
					}
				}
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			if (settingsReturnState == STATE_MAIN_MENU) {
				updateMenuScene();
				drawMenuScene(top);
				drawSettings(top);
				C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
				C2D_SceneBegin(bot);
				C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
			} else {
				drawGame(top, eyeOff);
				drawSettings(top);
				if (stereoActive) {
					drawGame(topRight, -eyeOff);
					drawSettings(topRight);
				}
				drawBottomScreen(bot);
			}
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Developer Settings ----
		if (gameState == STATE_DEV_SETTINGS) {
			if (devResetConfirm) {
				// Confirmation mode for reset
				if (kDown & KEY_A) {
					// Confirmed: delete all save files
					for (int s = 0; s < 3; s++) {
						remove(SAVE_PATHS[s]);
					}
					probeSlots();
					resetProgress();
					devResetConfirm = false;
				} else if (kDown & KEY_B) {
					devResetConfirm = false;
				}
			} else {
				if (kDown & KEY_B) {
					gameState = STATE_SETTINGS;
				} else {
					if (kDown & KEY_UP) { devSettingsCursor--; if (devSettingsCursor < 0) devSettingsCursor = DEV_SETTINGS_ITEM_COUNT - 1; }
					if (kDown & KEY_DOWN) { devSettingsCursor++; if (devSettingsCursor >= DEV_SETTINGS_ITEM_COUNT) devSettingsCursor = 0; }
					if (kDown & KEY_A) {
						switch (devSettingsCursor) {
							case 0:
								// Dev Level Select
								devMode = true;
								for (int i = 0; i < NUM_LEVELS; i++) levelUnlocked[i] = true;
								player.lives = 99;
								sessionCrackersCommitted = 0;
								player.score = 0;
								levelSelectCursor = 0;
								levelSelectScroll = 0.0f;
								gameState = STATE_LEVEL_SELECT;
								break;
							case 1:
								// Reset Save Progress — ask for confirmation
								devResetConfirm = true;
								break;
							case 2:
								// Back
								gameState = STATE_SETTINGS;
								break;
						}
					}
				}
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			if (settingsReturnState == STATE_MAIN_MENU) {
				updateMenuScene();
				drawMenuScene(top);
				drawDevSettings(top);
				C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
				C2D_SceneBegin(bot);
				C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
			} else {
				drawGame(top, eyeOff);
				drawDevSettings(top);
				if (stereoActive) {
					drawGame(topRight, -eyeOff);
					drawDevSettings(topRight);
				}
				drawBottomScreen(bot);
			}
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Dialogue Pre-Level ----
		if (gameState == STATE_DIALOGUE_PRE) {
			// Update typewriter effect with pause support
			const char* text = dialoguePre[currentLevel][currentDialogueBox];
			int rawLen = (int)strlen(text);
			if (currentDialogueChar < rawLen && !dialogueWaitingForInput) {
				if (dialoguePauseTimer > 0) {
					dialoguePauseTimer--;
				} else {
					dialogueTypewriterTimer++;
					if (dialogueTypewriterTimer >= TYPEWRITER_DELAY) {
						dialogueTypewriterTimer = 0;
						currentDialogueChar++;
						// Skip over any markup at new position
						while (currentDialogueChar < rawLen) {
							int pause = checkPause(text, currentDialogueChar);
							if (pause > 0) {
								dialoguePauseTimer = pause;
								int si = currentDialogueChar;
								while (text[si] && text[si] != '}') si++;
								if (text[si] == '}') si++;
								currentDialogueChar = si;
								break;
							} else if (text[currentDialogueChar] == '*' && text[currentDialogueChar+1] == '*') {
								currentDialogueChar += 2;
							} else {
								break;
							}
						}
					}
				}
			} else if (currentDialogueChar >= rawLen) {
				dialogueWaitingForInput = true;
			}

			if (kDown & KEY_A) {
				if (!dialogueWaitingForInput) {
					// Fast-forward to end
					currentDialogueChar = strlen(text);
					dialogueWaitingForInput = true;
				} else {
					// Advance to next box
					currentDialogueBox++;
					if (currentDialogueBox >= dialoguePreCount[currentLevel] || strlen(dialoguePre[currentLevel][currentDialogueBox]) == 0) {
						// Done with pre-level dialogue, start level
						gameState = STATE_PLAYING;
						snapPlayerOntoSolidBelow();
						snapCameraToPlayer();
						currentDialogueBox = 0;
						currentDialogueChar = 0;
						dialoguePauseTimer = 0;
						dialogueWaitingForInput = false;
					} else {
						// Next box
						currentDialogueChar = 0;
						dialoguePauseTimer = 0;
						dialogueWaitingForInput = false;
					}
				}
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawDialogue(top, eyeOff, dialoguePre[currentLevel][currentDialogueBox]);
			if (stereoActive) drawDialogue(topRight, -eyeOff, dialoguePre[currentLevel][currentDialogueBox]);
			C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
			C2D_SceneBegin(bot);
			C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Dialogue Post-Level ----
		if (gameState == STATE_DIALOGUE_POST) {
			// Update typewriter effect with pause support
			const char* text = dialoguePost[currentLevel][currentDialogueBox];
			int rawLen = (int)strlen(text);
			if (currentDialogueChar < rawLen && !dialogueWaitingForInput) {
				if (dialoguePauseTimer > 0) {
					dialoguePauseTimer--;
				} else {
					dialogueTypewriterTimer++;
					if (dialogueTypewriterTimer >= TYPEWRITER_DELAY) {
						dialogueTypewriterTimer = 0;
						currentDialogueChar++;
						// Skip over any markup at new position
						while (currentDialogueChar < rawLen) {
							int pause = checkPause(text, currentDialogueChar);
							if (pause > 0) {
								dialoguePauseTimer = pause;
								int si = currentDialogueChar;
								while (text[si] && text[si] != '}') si++;
								if (text[si] == '}') si++;
								currentDialogueChar = si;
								break;
							} else if (text[currentDialogueChar] == '*' && text[currentDialogueChar+1] == '*') {
								currentDialogueChar += 2;
							} else {
								break;
							}
						}
					}
				}
			} else if (currentDialogueChar >= rawLen) {
				dialogueWaitingForInput = true;
			}

			if (kDown & KEY_A) {
				if (!dialogueWaitingForInput) {
					// Fast-forward to end
					currentDialogueChar = strlen(text);
					dialogueWaitingForInput = true;
				} else {
					// Advance to next box
					currentDialogueBox++;
					if (currentDialogueBox >= dialoguePostCount[currentLevel] || strlen(dialoguePost[currentLevel][currentDialogueBox]) == 0) {
						// Done with post-level dialogue
						gameState = STATE_WIN;
						currentDialogueBox = 0;
						currentDialogueChar = 0;
						dialoguePauseTimer = 0;
						dialogueWaitingForInput = false;
					} else {
						// Next box
						currentDialogueChar = 0;
						dialoguePauseTimer = 0;
						dialogueWaitingForInput = false;
					}
				}
			}

			updateMusic();
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			drawDialogue(top, eyeOff, dialoguePost[currentLevel][currentDialogueBox]);
			if (stereoActive) drawDialogue(topRight, -eyeOff, dialoguePost[currentLevel][currentDialogueBox]);
			C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
			C2D_SceneBegin(bot);
			C2D_DrawImageAt(bottomMenuImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
			C3D_FrameEnd(0);
			continue;
		}

		// ---- Playing ----
		if (kDown & KEY_START) {
			gameState = STATE_PAUSED;
			pauseCursor = 0;
			pauseBegin = svcGetSystemTick();
			continue;
		}

		// Fade update
		if (fadeTimer > 0) {
			fadeTimer--;
		}

		circlePosition pos;
		hidCircleRead(&pos);
		if (pos.dx < -30) kHeld |= KEY_LEFT;
		if (pos.dx >  30) kHeld |= KEY_RIGHT;

		handleInput(kHeld, kDown, kUp);
		updatePlayer();
		updateCoins();
		updateEnemies();
		updateCrumbles();
		updateMovers();
		updateFlattenFx();
		updatePopups();
		updateVfx();
		updateDust();
		updateShake();
		updateCamera();
		updateAnimation();
		updateMusic();

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		drawGame(top, eyeOff);
		if (stereoActive) drawGame(topRight, -eyeOff);
		drawBottomScreen(bot);
		C3D_FrameEnd(0);
	}

	exitMusic();
	exitSfx();
	C2D_TextBufDelete(textBuf);
	C2D_SpriteSheetFree(spriteSheet);
	C2D_Fini();
	C3D_Fini();
	romfsExit();
	gfxExit();
	return 0;
}
