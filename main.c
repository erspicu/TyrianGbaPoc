#include <gba.h>
#include <maxmod.h>
#include <string.h>

#include "res/asset_meta.h"
#include "res/soundbank.h"
#include "res/sprite2_raw_meta.h"
#include "res/tyrian_romfs_meta.h"
#include "src/port_config.h"
#include "src/opentyrian_data.h"
#include "src/opentyrian_level_port.h"
#include "src/opentyrian_rom_io.h"
#include "src/opentyrian_sprite2.h"

#define GBA_WAITCNT (*(volatile u16 *)0x04000204)
#define GBA_WAITCNT_ROM_PREFETCH_3_1 0x4317u

/*
 * Development-validation switch.  Keep this at 1 (true) while inspecting
 * authored level flow; set it to 0 for the translated shield/armor/death
 * path.  The build can override it with
 * -DTYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0.
 */
#ifndef TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
#define TYRIAN_GBA_DEV_PLAYER_INVINCIBLE 1
#endif
#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 0 && \
    TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 1
#error TYRIAN_GBA_DEV_PLAYER_INVINCIBLE must be 0 or 1
#endif

#if defined(AUTOTEST_SCREENSHOT_TICK) || \
    defined(AUTOTEST_SCREENSHOT_POSITION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) || \
    defined(AUTOTEST_SCREENSHOT_REWARD) || \
    defined(AUTOTEST_SCREENSHOT_PAUSE)
#define AUTOTEST_SCREENSHOT_ENABLED
#endif

#define BG0_SCREEN_BLOCK 24
#define BG1_SCREEN_BLOCK 26
#define BG2_SCREEN_BLOCK 28
#define MAP_RING_ROWS 32
#define BG_MAP_COLUMNS 64
#define MAP_ROW_BYTES (BG_MAP_COLUMNS * sizeof(u16))
#define MAP_HALF_ROW_BYTES (32 * sizeof(u16))
#define MAP_SCREEN_BLOCK_WORDS (32 * 32)

/*
 * GBA has 128 hardware OBJ entries and substantially more CPU time than the
 * NES/SNES low-detail proofs.  These pools intentionally raise the first
 * level's concurrency while staying under a conservative scanline budget.
 */
#define MAX_PLAYER_SHOTS 12
#define MAX_ENEMY_SHOTS 60
/* varz.h MAX_EXPLOSIONS: preserve the source allocator before OAM clipping. */
#define MAX_EFFECTS 200
#define MAX_VISIBLE_EFFECTS 48
#define MAX_REWARDS 32
#define MAX_PICKUP_EXPLOSIONS 32
#define HARDWARE_OAM_ENTRIES 128
#define SPRITE_LIMIT HARDWARE_OAM_ENTRIES
/*
 * OpenTyrian presents the 264x184 gameplay viewport by copying game_screen
 * from x=24.  GBA keeps the centre 240x160 pixels at 1:1, so presentation
 * discards twelve pixels on every side and never rescales source geometry.
 */
#define SOURCE_GAME_SCREEN_VISIBLE_X 24
#define SOURCE_VIEW_CROP_X ((OT_GAME_VIEW_WIDTH - SCREEN_WIDTH) / 2)
#define SOURCE_VIEW_CROP_Y ((OT_GAME_VIEW_HEIGHT - SCREEN_HEIGHT) / 2)
#define SOURCE_PRESENTATION_X_ORIGIN \
    (SOURCE_GAME_SCREEN_VISIBLE_X + SOURCE_VIEW_CROP_X)
#define SOURCE_PRESENTATION_Y_ORIGIN SOURCE_VIEW_CROP_Y
#define SOURCE_MAP_CELL_WIDTH 24
#define BG1_INITIAL_SCROLL \
    ( \
        ( \
            OT_LEVEL_MAP1_ROWS - \
            OT_LEVEL_INITIAL_BOTTOM_MARGIN_ROWS - \
            OT_LEVEL_MAP1_FIRST_SOURCE_ROW \
        ) * OT_LEVEL_MAP_CELL_HEIGHT + SOURCE_VIEW_CROP_Y \
    )
#define BG23_INITIAL_SCROLL \
    ( \
        ( \
            OT_LEVEL_MAP2_ROWS - \
            OT_LEVEL_INITIAL_BOTTOM_MARGIN_ROWS - \
            OT_LEVEL_MAP23_FIRST_SOURCE_ROW \
        ) * OT_LEVEL_MAP_CELL_HEIGHT + SOURCE_VIEW_CROP_Y \
    )
#define BG2_INITIAL_SCROLL BG23_INITIAL_SCROLL
#define BG3_INITIAL_SCROLL BG23_INITIAL_SCROLL
#define BG12_INITIAL_HOFS \
    ( \
        OT_LEVEL_MAP_CELL_WIDTH + \
        SOURCE_GAME_SCREEN_VISIBLE_X + SOURCE_VIEW_CROP_X \
    )
#define BG3_INITIAL_HOFS \
    ( \
        2 * OT_LEVEL_MAP_CELL_WIDTH + \
        SOURCE_GAME_SCREEN_VISIBLE_X + SOURCE_VIEW_CROP_X \
    )
#define SOURCE_BG12_PARALLAX_BASE_X \
    (2 * SOURCE_MAP_CELL_WIDTH + SOURCE_PRESENTATION_X_ORIGIN)
#define SOURCE_BG3_PARALLAX_BASE_X \
    (3 * SOURCE_MAP_CELL_WIDTH + SOURCE_PRESENTATION_X_ORIGIN)
#define SOURCE_PLAYER_MIN_X 40
#define SOURCE_PLAYER_MAX_X 256
#define SOURCE_PLAYER_DRAW_Y_OFFSET (-7)
#define SOURCE_PLAYER_PRESENTATION_CENTRE_Y 7
#define SOURCE_PLAYER_CONTAINER_Y 2
#define SOURCE_PLAYER_ALPHA_TOP 2
#define SOURCE_PLAYER_ALPHA_BOTTOM_EXCLUSIVE 27
#define SOURCE_PLAYER_MIN_Y \
    (SOURCE_PRESENTATION_Y_ORIGIN - \
        SOURCE_PLAYER_DRAW_Y_OFFSET - SOURCE_PLAYER_ALPHA_TOP)
#define SOURCE_PLAYER_MAX_Y \
    (SOURCE_PRESENTATION_Y_ORIGIN + SCREEN_HEIGHT - \
        SOURCE_PLAYER_DRAW_Y_OFFSET - \
        SOURCE_PLAYER_ALPHA_BOTTOM_EXCLUSIVE)
#define PLAYER_SHOT_SPEED 10
#define PLAYER_SHOT_COOLDOWN 4
#define PLAYER_SHOT_X_OFFSET 8
#define PLAYER_SHOT_Y_OFFSET 6
#define PLAYER_SHOT_HIT_X 3
#define PLAYER_SHOT_HIT_Y 2
#define PLAYER_SHOT_HIT_WIDTH 10
#define PLAYER_SHOT_HIT_HEIGHT 11
#define EXPLOSION_FRAME_COUNT 12
#define EXPLOSION_TILES_PER_FRAME 4
#define EXPLOSION_SEQUENCE_SMALL 0
#define EXPLOSION_SEQUENCE_AIR_TOP_LEFT 1
#define EXPLOSION_SEQUENCE_AIR_TOP_RIGHT 2
#define EXPLOSION_SEQUENCE_AIR_BOTTOM_LEFT 3
#define EXPLOSION_SEQUENCE_AIR_BOTTOM_RIGHT 4
#define EXPLOSION_SEQUENCE_GROUND_TOP_LEFT 5
#define EXPLOSION_SEQUENCE_GROUND_TOP_RIGHT 6
#define EXPLOSION_SEQUENCE_GROUND_BOTTOM_LEFT 7
#define EXPLOSION_SEQUENCE_GROUND_BOTTOM_RIGHT 8
#define REWARD_FRAME_COUNT 3
#define REWARD_TILES_PER_FRAME 4
#define REWARD_SEQUENCE_COUNT 5
#define CASH_COUNTER_X 22
#define CASH_COUNTER_Y 140
/*
 * Runtime Sprite2 presentation.  The original PC 256-colour indices are
 * decoded from ROMFS and presented through eight time-shared OBJ banks.
 */
#define SOURCE_LEVEL_PALETTE_INDEX 5
#define SOURCE_ENEMY_DYNAMIC_PALETTE_BANK_COUNT 8
#define SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT 8
#define SOURCE_ENEMY_FRAME_BYTES 1024
#define SOURCE_ENEMY_TILES_PER_SLOT 32
#define SOURCE_SPRITE2_L2_SLOT_COUNT 64
#define SOURCE_SPRITE2_L2_FRAME_BYTES SOURCE_ENEMY_FRAME_BYTES
/*
 * The old POC's pre-rendered boss atlas occupied OBJ tiles 32..95, but the
 * source-parity runtime draws every boss component from ROMFS Sprite2 data.
 * Reclaim its two 32-tile 8bpp slots without overlapping the streamed
 * explosion bank which begins at tile 96.
 */
#define SOURCE_ENEMY_CACHE_RECLAIMED_TILE_BASE 32
#define SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT 2
#define SOURCE_ENEMY_CACHE_LOWER_TILE_BASE 224
#define SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT 9
#define SOURCE_ENEMY_CACHE_UPPER_TILE_BASE 640
#define SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT 12
#define SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT \
    (SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT)
#define SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT 1
#define SOURCE_ENEMY_CACHE_SLOT_COUNT \
    (SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT)
#define SOURCE_ENEMY_COMPACT_FRAME_BYTES 256
#define SOURCE_ENEMY_COMPACT_TILES_PER_SLOT 8
#define SOURCE_PROJECTILE_CACHE_SLOT_COUNT 8
#define SOURCE_PROJECTILE_TILES_PER_SLOT 8
#define SOURCE_PROJECTILE_CACHE_LOWER_TILE_BASE OBJ_TILE_REWARD
#define SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT 7
#define SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE \
    (OBJ_TILE_PLAYER_SHOT + 4)
#define SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE \
    (SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE + \
        (SOURCE_PROJECTILE_CACHE_SLOT_COUNT - \
            SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT) * \
            SOURCE_PROJECTILE_TILES_PER_SLOT)
#define SOURCE_PROJECTILE_FRAME_BYTES \
    (SOURCE_PROJECTILE_TILES_PER_SLOT * 32)

/*
 * Enemy 8bpp frames reclaim the middle of the old fully-resident explosion
 * atlas.  Active 16x16 explosion frames are therefore streamed into a
 * 32-frame 4bpp cache at the original explosion base.
 */
#define SOURCE_EFFECT_CACHE_TILE_BASE OBJ_TILE_EXPLOSION
#define SOURCE_EFFECT_CACHE_SLOT_COUNT 32
#define SOURCE_EFFECT_TILES_PER_SLOT EXPLOSION_TILES_PER_FRAME
#define SOURCE_EFFECT_FRAME_BYTES \
    (SOURCE_EFFECT_TILES_PER_SLOT * 32)

_Static_assert(
    EXPLOSION_FRAME_COUNT == OBJ_EXPLOSION_FRAME_COUNT,
    "explosion frame count must match generated OBJ assets"
);
_Static_assert(
    EXPLOSION_TILES_PER_FRAME == OBJ_EXPLOSION_TILES_PER_FRAME,
    "explosion frame stride must match generated OBJ assets"
);
_Static_assert(
    EXPLOSION_SEQUENCE_GROUND_BOTTOM_RIGHT + 1 ==
        OBJ_EXPLOSION_SEQUENCE_COUNT,
    "explosion sequence count must match generated OBJ assets"
);
_Static_assert(
    REWARD_FRAME_COUNT == OBJ_REWARD_FRAME_COUNT,
    "reward frame count must match generated OBJ assets"
);
_Static_assert(
    REWARD_TILES_PER_FRAME == OBJ_REWARD_TILES_PER_FRAME,
    "reward frame stride must match generated OBJ assets"
);
_Static_assert(
    REWARD_SEQUENCE_COUNT == OBJ_REWARD_SEQUENCE_COUNT,
    "reward sequence count must match generated OBJ assets"
);
_Static_assert(
    OBJ_SCORE_DIGIT_COUNT == 10,
    "cash counter must contain the ten original TINY_FONT digits"
);
_Static_assert(
    OBJ_PAUSE_GLYPH_COUNT == 6,
    "pause label must contain the six original PAUSED glyphs"
);
_Static_assert(
    OBJ_GAME_OVER_GLYPH_COUNT == 8 &&
        OBJ_GAME_OVER_TILE_COUNT == OBJ_GAME_OVER_GLYPH_COUNT * 2,
    "GAME OVER label must contain eight 8x16 glyphs"
);
_Static_assert(
    OBJ_TILE_GAME_OVER_RUNTIME ==
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE +
            SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT *
                SOURCE_ENEMY_TILES_PER_SLOT &&
        OBJ_TILE_GAME_OVER_RUNTIME + OBJ_GAME_OVER_TILE_COUNT <=
            OBJ_TILE_REWARD,
    "GAME OVER runtime bank must occupy the lower-cache/reward gap"
);
_Static_assert(
    OBJ_TILE_GAME_OVER_SOURCE == SOURCE_ENEMY_CACHE_UPPER_TILE_BASE,
    "GAME OVER cartridge bank must use the first time-shared upper slot"
);
_Static_assert(
    JUKEBOX_MUSIC_COUNT == 41 &&
        MSL_NSONGS == JUKEBOX_MUSIC_COUNT &&
        MOD_TYRIAN_MUSIC_00 == 0 &&
        MOD_TYRIAN_MUSIC_29 == 29 &&
        MOD_TYRIAN_MUSIC_40 == 40,
    "Maxmod modules must preserve zero-based music.mus catalog order"
);
_Static_assert(OBJ_TILE_COUNT <= 1024, "Mode 0 OBJ VRAM tile limit exceeded");
_Static_assert(
    SOURCE_ENEMY_FRAME_BYTES ==
        OT_SPRITE2_FRAME_PIXELS,
    "one 8bpp enemy cache frame must contain a 32x32 Sprite2 canvas"
);
_Static_assert(
    SOURCE_SPRITE2_L2_SLOT_COUNT *
        SOURCE_SPRITE2_L2_FRAME_BYTES >=
        FRONTEND_FRAME_BYTES,
    "frontend/gameplay overlay must fit the Mode-4 scratch frame"
);
_Static_assert(
    SPRITE2_RAW_TABLE_COUNT ==
        OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY &&
        SPRITE2_RAW_COMPONENT_WIDTH ==
            OT_SPRITE2_COMPONENT_WIDTH &&
        SPRITE2_RAW_COMPONENT_HEIGHT ==
            OT_SPRITE2_COMPONENT_HEIGHT,
    "build-time Sprite2 raw catalog geometry changed"
);
_Static_assert(
    SOURCE_ENEMY_FRAME_BYTES ==
        SOURCE_ENEMY_TILES_PER_SLOT * 32,
    "8bpp enemy frame tile stride changed"
);
_Static_assert(
    SOURCE_ENEMY_COMPACT_FRAME_BYTES ==
        SOURCE_ENEMY_COMPACT_TILES_PER_SLOT * 32,
    "compact 8bpp enemy frame tile stride changed"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_RECLAIMED_TILE_BASE +
        SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT *
            SOURCE_ENEMY_TILES_PER_SLOT <=
        SOURCE_EFFECT_CACHE_TILE_BASE,
    "reclaimed boss cache overlaps the streamed explosion bank"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_LOWER_TILE_BASE +
        SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT *
            SOURCE_ENEMY_TILES_PER_SLOT <=
        OBJ_TILE_REWARD,
    "lower enemy frame cache overlaps retained static OBJ assets"
);
_Static_assert(
    SOURCE_PROJECTILE_CACHE_LOWER_TILE_BASE +
        SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT *
            SOURCE_PROJECTILE_TILES_PER_SLOT <=
        OBJ_TILE_SCORE_DIGITS &&
        SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE +
            (
                SOURCE_PROJECTILE_CACHE_SLOT_COUNT -
                SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT
            ) * SOURCE_PROJECTILE_TILES_PER_SLOT <=
        OBJ_TILE_BOSS_BAR,
    "runtime projectile cache overlaps retained OBJ assets"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_UPPER_TILE_BASE +
        SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT *
            SOURCE_ENEMY_TILES_PER_SLOT <=
        OBJ_TILE_COUNT,
    "upper enemy frame cache exceeds OBJ VRAM"
);
_Static_assert(
    (SOURCE_ENEMY_CACHE_LOWER_TILE_BASE & 1) == 0 &&
        (SOURCE_ENEMY_CACHE_UPPER_TILE_BASE & 1) == 0 &&
        (SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE & 1) == 0,
    "8bpp OBJ cache bases must use even character indices"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE +
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT *
            SOURCE_ENEMY_COMPACT_TILES_PER_SLOT <=
        OBJ_TILE_BOSS_BAR,
    "compact enemy cache overlaps the boss bar"
);
_Static_assert(
    SOURCE_EFFECT_CACHE_TILE_BASE +
        SOURCE_EFFECT_CACHE_SLOT_COUNT *
            SOURCE_EFFECT_TILES_PER_SLOT <=
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE,
    "explosion and enemy caches overlap"
);
_Static_assert(
    SOURCE_ENEMY_DYNAMIC_PALETTE_BANK_COUNT * 16 ==
        16 * SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT,
    "dynamic OBJ palette must provide eight shades for every PC hue"
);
_Static_assert(
    OT_GAME_VIEW_WIDTH - SCREEN_WIDTH == 24,
    "GBA viewport must crop 24 source pixels horizontally"
);
_Static_assert(
    OT_GAME_VIEW_HEIGHT - SCREEN_HEIGHT == 24,
    "GBA viewport must crop 24 source pixels vertically"
);
_Static_assert(
    SOURCE_VIEW_CROP_X == 12 && SOURCE_VIEW_CROP_Y == 12,
    "OpenTyrian gameplay viewport must be centre-cropped by 12 pixels"
);
_Static_assert(
    SOURCE_PLAYER_MIN_Y == 17 && SOURCE_PLAYER_MAX_Y == 152,
    "player Y bounds must keep the source ship alpha bbox inside the crop"
);
_Static_assert(
    SOURCE_PLAYER_MIN_Y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y -
        SOURCE_PRESENTATION_Y_ORIGIN - 16 +
        SOURCE_PLAYER_CONTAINER_Y + SOURCE_PLAYER_ALPHA_TOP == 0,
    "top player bound must place the first opaque row at GBA y=0"
);
_Static_assert(
    SOURCE_PLAYER_MAX_Y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y -
        SOURCE_PRESENTATION_Y_ORIGIN - 16 +
        SOURCE_PLAYER_CONTAINER_Y +
        SOURCE_PLAYER_ALPHA_BOTTOM_EXCLUSIVE - 1 == SCREEN_HEIGHT - 1,
    "bottom player bound must place the last opaque row at GBA y=159"
);
_Static_assert(BG_MAP_COLUMNS == 64, "background map must be 512 pixels wide");
_Static_assert(
    MAP_ROW_BYTES == 128,
    "512-pixel background rows must contain 64 GBA map entries"
);
_Static_assert(
    BG1_INITIAL_SCROLL == 8104 &&
        BG2_INITIAL_SCROLL == 16196 &&
        BG3_INITIAL_SCROLL == 16196,
    "runtime background phase no longer matches OpenTyrian mapY setup"
);
_Static_assert(
    BG12_INITIAL_HOFS == 60 && BG3_INITIAL_HOFS == 84,
    "runtime background X phase no longer matches OpenTyrian pointers"
);

enum {
    STATE_TITLE = 0,
    STATE_PLAY = 1,
    STATE_INTRO_LOGO_1 = 2,
    STATE_INTRO_LOGO_2 = 3,
    STATE_PLAY_MODE_MENU = 4,
    STATE_EPISODE_MENU = 5,
    STATE_DIFFICULTY_MENU = 6,
    STATE_GAME_MENU = 7,
    STATE_NEXT_LEVEL_MENU = 8,
    STATE_LEVEL_STATS = 9,
    STATE_GAME_OVER = 10,
    STATE_JUKEBOX = 11,
};

enum {
    FRONTEND_PLAY_FULL_GAME = 0,
    FRONTEND_PLAY_ARCADE = 1,
    FRONTEND_DIFFICULTY_EASY = 1,
    FRONTEND_DIFFICULTY_NORMAL = 2,
    FRONTEND_DIFFICULTY_HARD = 3,
};

#define BOX_OVERLAPS(ax, ay, aw, ah, bx, by, bw, bh) \
    ((ax) + (aw) > (bx) && (bx) + (bw) > (ax) && \
     (ay) + (ah) > (by) && (by) + (bh) > (ay))

extern const u8 obj_tiles[];
extern const u8 obj_palette[];
extern const u8 frontend_frames[];
extern const u8 frontend_palettes[];
extern const u8 frontend_glyphs[];
extern const u8 frontend_cube[];
extern const u8 jukebox_font_tiles[];
extern const u8 jukebox_backdrop_tiles[];
extern const u8 jukebox_backdrop_map[];
extern const u8 jukebox_bg_palette[];
extern const u8 jukebox_obj_tiles[];
extern const u8 jukebox_obj_palette[];
extern const u8 jukebox_titles[];
extern const u8 jukebox_reciprocal[];
extern const u8 jukebox_sine[];
extern const u8 soundbank[];

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    u8 damage;
} PlayerShot;

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    u8 frame;
    u8 sequence;
} Effect;

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    u8 frame;
    u8 sequence;
    u8 phase;
    u16 value;
} Reward;

/*
 * OpenTyrian stores pickup value labels in the ordinary Explosion pool with
 * fixedPosition=true.  These retain source coordinates and the exact
 * newsh6.shp Sprite2 number instead of using a GBA-authored text overlay.
 */
typedef struct {
    u8 active;
    u8 ttl;
    u16 graphic;
    s16 x;
    s16 y;
} PickupExplosion;

static PlayerShot player_shots[MAX_PLAYER_SHOTS] EWRAM_DATA;
static Effect effects[MAX_EFFECTS] EWRAM_DATA;
static Reward rewards[MAX_REWARDS] EWRAM_DATA;
static PickupExplosion pickup_explosions[MAX_PICKUP_EXPLOSIONS] EWRAM_DATA;
static u8 active_effect_count;
static u8 effect_slot_high_water;
static u8 active_reward_count;
static u8 active_pickup_explosion_count;
static OBJATTR oam_shadow[HARDWARE_OAM_ENTRIES] EWRAM_DATA;

#ifdef AUTOTEST_REWARD_VISUAL_TEST
static const u16 reward_value_table[REWARD_SEQUENCE_COUNT + 1] = {
    0, 25, 50, 75, 100, 250,
};
#endif

static const u8 reward_frame_delay_table[REWARD_SEQUENCE_COUNT] = {
    2, 2, 2, 5, 5,
};

static const u8 cash_digit_advances[OBJ_SCORE_DIGIT_COUNT] = {
    OBJ_SCORE_DIGIT_ADVANCE_0,
    OBJ_SCORE_DIGIT_ADVANCE_1,
    OBJ_SCORE_DIGIT_ADVANCE_2,
    OBJ_SCORE_DIGIT_ADVANCE_3,
    OBJ_SCORE_DIGIT_ADVANCE_4,
    OBJ_SCORE_DIGIT_ADVANCE_5,
    OBJ_SCORE_DIGIT_ADVANCE_6,
    OBJ_SCORE_DIGIT_ADVANCE_7,
    OBJ_SCORE_DIGIT_ADVANCE_8,
    OBJ_SCORE_DIGIT_ADVANCE_9,
};

static const u8 pause_glyph_advances[OBJ_PAUSE_GLYPH_COUNT] = {
    OBJ_PAUSE_ADVANCE_0,
    OBJ_PAUSE_ADVANCE_1,
    OBJ_PAUSE_ADVANCE_2,
    OBJ_PAUSE_ADVANCE_3,
    OBJ_PAUSE_ADVANCE_4,
    OBJ_PAUSE_ADVANCE_5,
};

static const u8 game_over_glyph_advances[
    OBJ_GAME_OVER_GLYPH_COUNT
] = {
    OBJ_GAME_OVER_ADVANCE_0,
    OBJ_GAME_OVER_ADVANCE_1,
    OBJ_GAME_OVER_ADVANCE_2,
    OBJ_GAME_OVER_ADVANCE_3,
    OBJ_GAME_OVER_ADVANCE_4,
    OBJ_GAME_OVER_ADVANCE_5,
    OBJ_GAME_OVER_ADVANCE_6,
    OBJ_GAME_OVER_ADVANCE_7,
};

static u8 game_state;
static u8 game_paused;
static u16 pad_now;
static u16 pad_pressed;
static u8 frontend_selection;
static u8 frontend_play_mode;
static u8 frontend_episode;
static u8 frontend_difficulty;
static u16 frontend_main_section;
/*
 * These decoded menu records are cold while gameplay is active.  Keep them
 * in EWRAM so the scarce 32 KiB IWRAM can hold collision and Sprite2 cache
 * hot paths without sacrificing the link-time stack/IRQ safety margin.
 */
static OtEpisodeMap frontend_map EWRAM_BSS;
static OtEpisodeLevel
    frontend_map_level[OT_EPISODE_MAP_CHOICE_COUNT] EWRAM_BSS;
static OtEpisodeLevel frontend_level EWRAM_BSS;
static OtFrontendText frontend_text EWRAM_BSS;
static u8 frontend_text_ready;
static u8 frontend_map_ready;
static u8 frontend_level_ready;
static u16 frontend_timer EWRAM_BSS;
static u8 frontend_stats_stage EWRAM_BSS;
static u8 frontend_stats_cube_visible_count EWRAM_BSS;
static u16 frontend_stats_timer EWRAM_BSS;
static u8 frontend_level_completed EWRAM_BSS;
static u8 frontend_mode4_active;
static u8 frontend_display_page EWRAM_BSS;
static u8 frontend_frame_pending EWRAM_BSS;
static u8 frontend_pending_kind EWRAM_BSS;
static u8 frontend_patch_state EWRAM_BSS;
static u8 frontend_patch_old_selection EWRAM_BSS;
static u8 frontend_patch_new_selection EWRAM_BSS;
static const u8 *frontend_pending_frame EWRAM_BSS;
static const u8 *frontend_pending_palette EWRAM_BSS;
/*
 * Mode-4 menus and gameplay never execute concurrently.  Share their largest
 * transient buffers so the 64 KiB Sprite2 L2 fits without reducing the
 * remaining EWRAM heap/stack margin.
 */
typedef union {
    u8 frontend_frame[FRONTEND_FRAME_BYTES];
    u8 sprite2_l2[
        SOURCE_SPRITE2_L2_SLOT_COUNT
    ][SOURCE_SPRITE2_L2_FRAME_BYTES];
} FrontendGameplayArena;

static FrontendGameplayArena frontend_gameplay_arena
    EWRAM_BSS __attribute__((aligned(4)));
#define frontend_frame_scratch \
    (frontend_gameplay_arena.frontend_frame)
#define source_sprite2_l2_tiles \
    (frontend_gameplay_arena.sprite2_l2)
/* Authoritative OpenTyrian gameplay coordinates, never GBA screen pixels. */
static s16 player_source_x;
static s16 player_source_y;
static s8 player_source_velocity_x;
static s8 player_source_velocity_y;
static u8 player_source_x_friction_ticks;
static u8 player_source_y_friction_ticks;
static u16 player_invulnerable;
static u8 player_alive;
static u8 player_exploding_ticks;
static u8 player_death_fx_wait;
static u8 player_death_music_volume;
static u8 player_death_music_fade_active;
static u8 player_armor;
static u8 player_shield;
static u8 player_shield_max;
static u8 player_lives;
static s8 player_end_warp;
static u8 fire_cooldown;
static s8 player_bank;
static u32 player_cash;

static u8 boss_bar_flash;
static u8 boss_bar_palette_dirty;
static u8 boss_obj_palette_restore_pending;

static u16 level_tick;
static u16 level_position;
static u32 logic_accumulator;
static u8 level_exit_music_started;
static OtLevelPortState source_parity_level EWRAM_BSS;
static u8 source_enemy_shape_history_valid;

static u16 bg1_scroll_pixel;
static u16 bg2_scroll_pixel;
static u16 bg3_scroll_pixel;
static u16 bg1_presentation_scroll_pixel;
static u16 bg2_presentation_scroll_pixel;
static u16 bg3_presentation_scroll_pixel;
static u8 bg1_scroll_speed;
static u8 bg2_scroll_speed;
static u8 bg3_scroll_speed;
static u8 bg1_scroll_delay;
static u8 bg2_scroll_delay;
static u8 bg1_scroll_delay_max;
static u8 bg2_scroll_delay_max;
static u8 bg1_step;
static u8 bg2_step;
static u8 bg3_step;
static u8 bg1_row_pending;
static u8 bg2_row_pending;
static u8 bg3_row_pending;
static u8 source_background2_enabled;
static u8 source_background2_upload_pending;
static const u8 *bg1_row_source;
static const u8 *bg2_row_source;
static const u8 *bg3_row_source;
static u16 *bg1_row_target;
static u16 *bg2_row_target;
static u16 *bg3_row_target;

static u8 oam_count;
static u8 previous_oam_count;
static u8 oam_dirty;
static u8 game_over_tile_upload_pending;
static u32 last_vblank_seen;

volatile u32 telemetry_vblank_irqs;
volatile u32 telemetry_display_frames;
volatile u32 telemetry_logic_updates;
volatile u32 telemetry_spawn_count;
volatile u32 telemetry_control_count;
volatile u32 telemetry_collision_count;
volatile u32 telemetry_map_rows;
volatile u32 telemetry_missed_vblanks;
volatile u32 telemetry_stream_drops;
volatile u32 telemetry_max_enemies;
volatile u32 telemetry_max_oam;
volatile u32 telemetry_max_effects;
volatile u32 telemetry_effect_drops;
volatile u32 telemetry_reward_spawns;
volatile u32 telemetry_reward_pickups;
volatile u32 telemetry_max_rewards;
volatile u32 telemetry_reward_drops;
volatile u32 telemetry_enemy_shots_spawned;
volatile u32 telemetry_enemy_shot_drops;
volatile u32 telemetry_max_enemy_shots;
volatile u32 telemetry_enemy_replacements;
volatile u32 telemetry_kill_cash;
volatile u32 telemetry_reward_controls;
volatile u32 telemetry_reward_assignments;
volatile u32 telemetry_pause_toggles;
volatile u32 telemetry_paused_frames;
volatile u32 telemetry_source_events;
volatile u32 telemetry_source_events_applied;
volatile u32 telemetry_source_events_deferred;
volatile u32 telemetry_source_events_skipped;
volatile u32 telemetry_source_spawn_attempts;
volatile u32 telemetry_source_spawn_successes;
volatile u32 telemetry_source_spawn_pool_full;
volatile u32 telemetry_source_spawn_missing;
volatile u32 telemetry_source_max_enemies;
volatile u32 telemetry_source_control_writes;
volatile u32 telemetry_source_rng_calls;
volatile u32 telemetry_source_motion_updates;
volatile u32 telemetry_source_releases;
volatile u32 telemetry_source_shot_triggers;
volatile u32 telemetry_source_enemy_shots_spawned;
volatile u32 telemetry_source_enemy_shot_drops;
volatile u32 telemetry_source_max_enemy_shots;
volatile u32 telemetry_source_enemy_shot_updates;
volatile u32 telemetry_source_enemy_shot_releases;
volatile u32 telemetry_source_enemy_shot_player_hits;
volatile u32 telemetry_source_player_shot_hits;
volatile u32 telemetry_source_enemy_kills;
volatile u32 telemetry_source_direct_cash;
volatile u32 telemetry_autotest_combat_assists;
volatile u32 telemetry_source_score_item_spawns;
volatile u32 telemetry_source_score_item_pickups;
volatile u32 telemetry_source_score_item_max_active;
volatile u32 telemetry_source_death_spawn_attempts;
volatile u32 telemetry_source_death_spawn_successes;
volatile u32 telemetry_source_death_control_events;
volatile u32 telemetry_source_death_assignments;
volatile u32 telemetry_source_visible_enemies;
volatile u32 telemetry_source_max_visible_enemies;
volatile u32 telemetry_source_unknown_visuals;
volatile u32 telemetry_source_launch_attempts;
volatile u32 telemetry_source_launch_successes;
volatile u32 telemetry_source_random_attempts;
volatile u32 telemetry_source_random_successes;
volatile u32 telemetry_sprite2_decode_failures;
volatile u32 telemetry_sprite2_null_pointer_skips;
volatile u32 telemetry_sprite2_zero_graphic_skips;
volatile u32 telemetry_sprite2_cache_hits;
volatile u32 telemetry_sprite2_cache_misses;
volatile u32 telemetry_sprite2_cache_evictions;
volatile u32 telemetry_sprite2_cache_drops;
volatile u32 telemetry_sprite2_uploads;
volatile u32 telemetry_sprite2_upload_bytes;
volatile u32 telemetry_sprite2_compact_uploads;
volatile u32 telemetry_sprite2_max_uploads;
volatile u32 telemetry_sprite2_max_visible_unique;
volatile u32 telemetry_projectile_cache_hits;
volatile u32 telemetry_projectile_cache_misses;
volatile u32 telemetry_projectile_cache_evictions;
volatile u32 telemetry_projectile_cache_drops;
volatile u32 telemetry_projectile_cache_uploads;
volatile u32 telemetry_projectile_cache_max_uploads;
volatile u32 telemetry_projectile_cache_max_visible_unique;
volatile u32 telemetry_effect_cache_hits;
volatile u32 telemetry_effect_cache_misses;
volatile u32 telemetry_effect_cache_evictions;
volatile u32 telemetry_effect_cache_drops;
volatile u32 telemetry_effect_cache_uploads;
volatile u32 telemetry_effect_cache_upload_bytes;
volatile u32 telemetry_effect_cache_max_uploads;
volatile u32 telemetry_effect_cache_max_visible_unique;
volatile u32 telemetry_state_transitions;
volatile u32 telemetry_romfs_entries;
volatile u32 telemetry_romfs_image_bytes;
volatile u32 telemetry_romfs_payload_bytes;
volatile u32 telemetry_romfs_checks;
volatile u32 telemetry_romfs_failures;
volatile u32 telemetry_romfs_manifest_crc32;
volatile u32 telemetry_layer_rule_checks;
volatile u32 telemetry_layer_rule_failures;
volatile u32 telemetry_pickup_explosion_spawns;
volatile u32 telemetry_pickup_explosion_drops;
volatile u32 telemetry_pickup_explosion_max_active;
volatile u32 telemetry_end_level_music_starts;
volatile u32 telemetry_end_level_initial_warp;
volatile u32 telemetry_end_level_trail_max;
volatile u32 telemetry_level_complete_voice_starts;
volatile u32 telemetry_stats_stage_advances;
volatile u32 telemetry_stats_cube_reveals;
volatile u32 telemetry_player_death_large_explosions;
volatile u32 telemetry_player_death_sfx_9;
volatile u32 telemetry_player_death_sfx_11;
volatile u32 telemetry_player_death_sfx_22;
volatile u32 telemetry_player_death_music_fade_steps;
volatile u32 telemetry_game_over_music_starts;
volatile u32 telemetry_game_over_overlay_frames;
volatile u32 telemetry_game_over_exits;
volatile u32 telemetry_boss_perf_started;
volatile u32 telemetry_boss_perf_completed;
volatile u32 telemetry_boss_perf_start_position;
volatile u32 telemetry_boss_perf_end_position;
volatile u32 telemetry_boss_perf_display_frames;
volatile u32 telemetry_boss_perf_missed_vblanks;
volatile u32 telemetry_boss_perf_sprite2_misses;
volatile u32 telemetry_boss_perf_sprite2_evictions;
volatile u32 telemetry_boss_perf_sprite2_upload_bytes;
volatile u32 telemetry_boss_perf_projectile_misses;
volatile u32 telemetry_sprite2_l2_hits;
volatile u32 telemetry_sprite2_l2_misses;
volatile u32 telemetry_sprite2_l2_evictions;
volatile u32 telemetry_sprite2_l2_drops;
volatile u32 telemetry_sprite2_l2_flushes;
volatile u32 telemetry_sprite2_l2_raw_builds;
volatile u32 telemetry_sprite2_l2_rle_fallbacks;
volatile u32 telemetry_sprite2_l2_max_visible_unique;
volatile u32 telemetry_boss_perf_l2_hits;
volatile u32 telemetry_boss_perf_l2_misses;
volatile u32 telemetry_boss_perf_l2_evictions;
volatile u32 telemetry_boss_perf_l2_raw_builds;
volatile u32 telemetry_boss_perf_l2_fallbacks;
volatile u32 telemetry_waitcnt;

static u32 boss_perf_start_display_frames;
static u32 boss_perf_start_missed_vblanks;
static u32 boss_perf_start_sprite2_misses;
static u32 boss_perf_start_sprite2_evictions;
static u32 boss_perf_start_sprite2_upload_bytes;
static u32 boss_perf_start_projectile_misses;
static u32 boss_perf_start_l2_hits;
static u32 boss_perf_start_l2_misses;
static u32 boss_perf_start_l2_evictions;
static u32 boss_perf_start_l2_raw_builds;
static u32 boss_perf_start_l2_fallbacks;

static const u16 boss_bar_fill_colours[7][3] = {
    {
        BOSS_BAR_FLASH_0_BOTTOM,
        BOSS_BAR_FLASH_0_MIDDLE,
        BOSS_BAR_FLASH_0_TOP,
    },
    {
        BOSS_BAR_FLASH_1_BOTTOM,
        BOSS_BAR_FLASH_1_MIDDLE,
        BOSS_BAR_FLASH_1_TOP,
    },
    {
        BOSS_BAR_FLASH_2_BOTTOM,
        BOSS_BAR_FLASH_2_MIDDLE,
        BOSS_BAR_FLASH_2_TOP,
    },
    {
        BOSS_BAR_FLASH_3_BOTTOM,
        BOSS_BAR_FLASH_3_MIDDLE,
        BOSS_BAR_FLASH_3_TOP,
    },
    {
        BOSS_BAR_FLASH_4_BOTTOM,
        BOSS_BAR_FLASH_4_MIDDLE,
        BOSS_BAR_FLASH_4_TOP,
    },
    {
        BOSS_BAR_FLASH_5_BOTTOM,
        BOSS_BAR_FLASH_5_MIDDLE,
        BOSS_BAR_FLASH_5_TOP,
    },
    {
        BOSS_BAR_FLASH_6_BOTTOM,
        BOSS_BAR_FLASH_6_MIDDLE,
        BOSS_BAR_FLASH_6_TOP,
    },
};

#ifdef AUTOTEST
static u8 autotest_running;
static u8 autotest_frontend_finish_pending;
static const char save_type_marker[] __attribute__((used)) = "SRAM_V121";
static void autotest_finish(void);
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
#if AUTOTEST_CAMPAIGN_LEVEL_COUNT < 1 || AUTOTEST_CAMPAIGN_LEVEL_COUNT > 16
#error AUTOTEST_CAMPAIGN_LEVEL_COUNT must be between 1 and 16
#endif
typedef struct {
    u32 episode;
    u32 resolved_section;
    u32 lvl_file_number;
    u32 source_song;
    u32 event_index;
    u32 level_position;
    u32 enemy_kills;
    u32 background_approximations;
    u32 failure_flags;
} AutotestCampaignRecord;

enum {
    AUTOTEST_CAMPAIGN_FAIL_DUPLICATE_ROUTE = 1u << 0,
    AUTOTEST_CAMPAIGN_FAIL_COMPLETION = 1u << 1,
    AUTOTEST_CAMPAIGN_FAIL_ASSETS = 1u << 2,
    AUTOTEST_CAMPAIGN_FAIL_EVENT_ACCOUNTING = 1u << 3,
    AUTOTEST_CAMPAIGN_FAIL_EVENT_SPAWN = 1u << 4,
    AUTOTEST_CAMPAIGN_FAIL_LAUNCH_SPAWN = 1u << 5,
    AUTOTEST_CAMPAIGN_FAIL_RANDOM_SPAWN = 1u << 6,
    AUTOTEST_CAMPAIGN_FAIL_DEATH_SPAWN = 1u << 7,
    AUTOTEST_CAMPAIGN_FAIL_ROMFS = 1u << 8,
    AUTOTEST_CAMPAIGN_FAIL_SPRITE_DECODE = 1u << 9,
    AUTOTEST_CAMPAIGN_FAIL_SPRITE_CACHE = 1u << 10,
    AUTOTEST_CAMPAIGN_FAIL_EFFECT_CACHE = 1u << 11,
    AUTOTEST_CAMPAIGN_FAIL_PROJECTILE_CACHE = 1u << 12,
    AUTOTEST_CAMPAIGN_FAIL_LAYER_RULE = 1u << 13,
    AUTOTEST_CAMPAIGN_FAIL_END_MUSIC = 1u << 14,
};

static u32 autotest_campaign_levels_completed;
static u32 autotest_campaign_failures;
static u32 autotest_campaign_checksum;
static AutotestCampaignRecord
    autotest_campaign_record[AUTOTEST_CAMPAIGN_LEVEL_COUNT] EWRAM_BSS;
#endif
#ifdef AUTOTEST_SCREENSHOT_ENABLED
static u8 autotest_screenshot_delay;
#endif
#endif

static s16 source_player_screen_x(void);
static s16 source_player_screen_y(void);
static u16 source_background_hofs(u8 layer);
static void source_runtime_reset(void);
static void source_enemy_cache_commit(void);
static void source_projectile_cache_commit(void);
static void source_effect_cache_commit(void);
static void frontend_commit_vblank(void);
static void jukebox_commit_vblank(void);
static void jukebox_enter(void);
static void jukebox_update(void);
static void jukebox_render(void);
#include "src/background_runtime.inc"
#include "src/layer_runtime.inc"
#include "src/gba_platform.inc"
#include "src/level_setup.inc"
#include "src/frontend_runtime.inc"
#include "src/entity_runtime.inc"
#include "src/combat_runtime.inc"
#include "src/source_runtime.inc"
#include "src/level_update.inc"
#include "src/gba_oam.inc"
#include "src/jukebox_runtime.inc"
#include "src/gba_hud.inc"
#include "src/gba_scene.inc"
#include "src/autotest.inc"
int main(void)
{
    u32 current_vblank;
    u8 logic_updated;
    uint32_t romfs_passed_checks = 0;
    uint32_t romfs_failed_checks = 0;
    const OtRomFs *mounted_romfs;

    /*
     * WS0 3/1-cycle reads plus Game Pak prefetch.  Sprite2 raw data, ROMFS
     * and executable code all live in the Game Pak address space.
     */
    GBA_WAITCNT = GBA_WAITCNT_ROM_PREFETCH_3_1;
    telemetry_waitcnt = GBA_WAITCNT;
    irqInit();
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
    ot_rom_io_self_test(&romfs_passed_checks, &romfs_failed_checks);
    mounted_romfs = ot_rom_io_filesystem();
    telemetry_romfs_entries =
        mounted_romfs != 0 ? mounted_romfs->entry_count : 0;
    telemetry_romfs_image_bytes =
        mounted_romfs != 0 ? mounted_romfs->image_size : 0;
    telemetry_romfs_payload_bytes =
        mounted_romfs != 0 ? mounted_romfs->payload_bytes : 0;
    telemetry_romfs_checks =
        romfs_passed_checks + romfs_failed_checks;
    telemetry_romfs_failures = romfs_failed_checks;
    telemetry_romfs_manifest_crc32 =
        mounted_romfs != 0 ? mounted_romfs->manifest_crc32 : 0;
#ifdef AUTOTEST_ROMFS_LEVEL_MATRIX
    /*
     * Exercise every stock tyrianN.lvl section and every levelsN.dat route
     * on the actual GBA runtime.  This path deliberately runs before audio
     * or front-end setup: its only inputs are the original ROMFS files.
     */
    autotest_romfs_level_matrix();
#endif
    /*
     * Maxmod's GBA initializer waits across two display frames while its
     * VBlank routine primes the double-buffer write position.  The IRQ must
     * therefore be live before mmInitDefault(), or the first mmFrame() writes
     * through an uninitialized output pointer.
     */
    mmInitDefault((mm_addr)soundbank, 16);

    telemetry_vblank_irqs = 0;
    telemetry_state_transitions = 0;
    hide_all_sprites();
    frontend_begin();
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * Visual/test-only direct route entry.  Release builds always traverse
     * the original logo and menu sequence.
     */
#ifdef AUTOTEST_FRONTEND_ROUTE_EPISODE
    frontend_episode = AUTOTEST_FRONTEND_ROUTE_EPISODE - 1;
#else
    frontend_episode = 0;
#endif
    frontend_main_section = AUTOTEST_FRONTEND_ROUTE_SECTION;
#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
    frontend_play_mode = FRONTEND_PLAY_ARCADE;
#endif
    if (frontend_prepare_map()) {
        frontend_enter_state(STATE_NEXT_LEVEL_MENU, 0);
    }
#endif

    for (;;) {
        logic_updated = 0;
        VBlankIntrWait();
        current_vblank = telemetry_vblank_irqs;
        if (last_vblank_seen && current_vblank > last_vblank_seen + 1) {
            telemetry_missed_vblanks += current_vblank - last_vblank_seen - 1;
        }
        last_vblank_seen = current_vblank;
        commit_vblank_work();
#ifdef AUTOTEST_SCREENSHOT_ENABLED
        if (autotest_screenshot_delay &&
            --autotest_screenshot_delay == 0) {
            __asm__ volatile("swi 3");
        }
#endif
        mmFrame();

        scanKeys();
        pad_now = keysHeld();
        pad_pressed = keysDown();
#ifdef AUTOTEST
        if (game_state == STATE_GAME_OVER) {
            pad_now = 0;
            pad_pressed = 0;
        } else if (game_state != STATE_PLAY) {
            if (!autotest_running) autotest_running = 1;
            pad_now = 0;
#ifdef AUTOTEST_JUKEBOX_FLOW
            pad_pressed = autotest_jukebox_input();
#elif defined(AUTOTEST_FRONTEND_STRESS)
            pad_pressed =
                game_state == STATE_TITLE ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_CAPTURE_STATE)
            pad_pressed =
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE ?
                    0 :
                    KEY_A;
#else
            pad_pressed =
                game_state == STATE_LEVEL_STATS ?
                    (
                        frontend_stats_stage >=
                            FRONTEND_STATS_STAGE_FINAL ?
                                KEY_A :
                                0
                    ) :
                    (
                        autotest_frontend_finish_pending ?
                            0 :
                            KEY_A
                    );
#endif
        } else {
            pad_now = autotest_input();
            pad_pressed = 0;
            if (
                telemetry_display_frames == 120 ||
                telemetry_display_frames == 180
            ) {
                pad_pressed = KEY_START;
            }
        }
#endif

        if (game_state == STATE_GAME_OVER) {
            game_over_update();
            if (game_state == STATE_GAME_OVER) {
                render_game();
                telemetry_display_frames++;
#ifdef AUTOTEST_DEATH_FLOW
                autotest_death_update();
#endif
            }
        } else if (game_state == STATE_JUKEBOX) {
            jukebox_update();
            if (game_state == STATE_JUKEBOX) {
                jukebox_render();
                telemetry_display_frames++;
            }
        } else if (game_state != STATE_PLAY) {
#ifdef AUTOTEST_BOOT_ONLY
            u8 old_state = game_state;
#endif
            frontend_update();
#ifdef AUTOTEST_FRONTEND_CAPTURE_STATE
            static u8 frontend_capture_armed;

            if (
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE
            ) {
                if (!frontend_capture_armed) {
#ifdef AUTOTEST_FRONTEND_CAPTURE_SELECTION
                    frontend_selection =
                        AUTOTEST_FRONTEND_CAPTURE_SELECTION;
                    frontend_redraw();
#endif
                    frontend_capture_armed = 1;
                } else if (
                    frontend_capture_armed == 1 &&
                    !frontend_frame_pending
                ) {
                    /*
                     * The page-select write happens during this VBlank.
                     * Let the PPU rasterize one complete frame before the
                     * headless screenshot hook stops execution.
                     */
                    frontend_capture_armed = 2;
                } else if (frontend_capture_armed == 2) {
#ifdef AUTOTEST_DEATH_MUSIC_CHECK
                    const OtDataCatalog *catalog = ot_data_catalog();
                    volatile u8 *sram = (volatile u8 *)0x0E000000;

                    sram[0] = 'T';
                    sram[1] = 'G';
                    sram[2] = 'D';
                    sram[3] = 'M';
                    sram_write_u32(
                        4,
                        catalog ? catalog->selected_mus_song : 0xffffu
                    );
                    sram_write_u32(8, mmActive());
                    sram_write_u32(12, game_state);
                    sram_write_u32(
                        16,
                        TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
                    );
#endif
                    __asm__ volatile("swi 3");
                }
            }
#endif
#ifdef AUTOTEST_FRONTEND_STRESS
            {
                static u8 frontend_stress_started;
                static u16 frontend_stress_frames;

                if (game_state == STATE_TITLE) {
                    if (!frontend_stress_started) {
                        frontend_stress_started = 1;
                        telemetry_missed_vblanks = 0;
                        last_vblank_seen = current_vblank;
                    }
                    {
                        u8 old_selection = frontend_selection;

#ifndef AUTOTEST_FRONTEND_STRESS_NO_REDRAW
                        frontend_selection ^= 1u;
                        frontend_redraw_selection(old_selection);
#else
                        (void)old_selection;
#endif
                    }
                    frontend_stress_frames++;
                    if (frontend_stress_frames == 600) {
                        volatile u8 *sram =
                            (volatile u8 *)0x0E000000;

                        sram[0] = 'T';
                        sram[1] = 'G';
                        sram[2] = 'F';
                        sram[3] = '4';
                        sram_write_u32(4, frontend_stress_frames);
                        sram_write_u32(8, telemetry_missed_vblanks);
                        sram_write_u32(12, telemetry_vblank_irqs);
                        sram_write_u32(
                            16,
                            frontend_frame_pending
                        );
                        __asm__ volatile("swi 3");
                    }
                }
            }
#endif
#ifdef AUTOTEST
            if (
                autotest_frontend_finish_pending &&
                !frontend_frame_pending
            ) {
                autotest_finish();
            }
#endif
#ifdef AUTOTEST_BOOT_ONLY
            if (old_state == STATE_NEXT_LEVEL_MENU &&
                game_state == STATE_PLAY) {
                __asm__ volatile("swi 3");
            }
#endif
        } else {
            if (
                game_state == STATE_PLAY &&
                (pad_pressed & KEY_START)
            ) {
                toggle_pause();
            }
            if (game_paused) {
                telemetry_paused_frames++;
                render_game();
#ifdef AUTOTEST_SCREENSHOT_PAUSE
                if (!autotest_screenshot_delay) {
                    autotest_screenshot_delay = 2;
                }
#endif
            } else {
                logic_accumulator += TYRIAN_GBA_LOGIC_NUMERATOR;
                if (logic_accumulator >= TYRIAN_GBA_LOGIC_DENOMINATOR) {
                    logic_accumulator -= TYRIAN_GBA_LOGIC_DENOMINATOR;
                    logic_updated = 1;
                    update_logic();
                    if (
                        game_state == STATE_PLAY ||
                        game_state == STATE_GAME_OVER
                    ) {
                        render_game();
                    }
                    if (game_state == STATE_PLAY) {
#ifdef AUTOTEST_SCREENSHOT_TICK
                        if (
                            !autotest_screenshot_delay &&
                            game_state == STATE_PLAY &&
                            level_tick >= AUTOTEST_SCREENSHOT_TICK
                        ) {
                            autotest_screenshot_delay = 3;
                        }
#endif
#ifdef AUTOTEST_SCREENSHOT_POSITION
                        if (
                            !autotest_screenshot_delay &&
                            game_state == STATE_PLAY &&
                            level_position >= AUTOTEST_SCREENSHOT_POSITION
                        ) {
                            /*
                             * Position-based capture makes the PC event
                             * sequence reproducible even when presentation
                             * frame counts change.
                             */
                            autotest_screenshot_delay = 3;
                        }
#endif
#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
                        if (
                            !autotest_screenshot_delay &&
                            autotest_explosion_visible()
                        ) {
                            autotest_screenshot_delay = 3;
                        }
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
                        if (
                            !autotest_screenshot_delay &&
                            autotest_reward_visible()
                        ) {
                            /*
                             * The first VBlank commits OAM; the second lets
                             * mGBA finish a rasterized frame before capture.
                             */
                            game_paused = 1;
                            autotest_screenshot_delay = 2;
                        }
#endif
                    }
                }
            }
            if (game_state == STATE_PLAY) {
                background_prefetch_step(
                    logic_updated ? 0 : BACKGROUND_PREFETCH_IDLE_MISSES
                );
            }
            telemetry_display_frames++;
#ifdef AUTOTEST
            if (
                autotest_running &&
                telemetry_display_frames >= 20000 &&
                game_state == STATE_PLAY
            ) {
                /*
                 * Deterministic deadlock watchdog: commit partial telemetry
                 * instead of leaving the headless verifier waiting forever.
                 */
                autotest_finish();
            }
#endif
        }
#ifdef AUTOTEST_JUKEBOX_FLOW
        autotest_jukebox_maybe_finish();
#endif
    }
}
