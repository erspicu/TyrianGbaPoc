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

/*
 * Gameplay code predating the complete source sound catalog keeps these
 * descriptive aliases.  Every alias now resolves to the authoritative
 * one-based sndmast.h slot packed as source_sound_01..38.
 */
#define SFX_WEAPON_1 SFX_SOURCE_SOUND_01
#define SFX_EXPLOSION_9 SFX_SOURCE_SOUND_09
#define SFX_EXPLOSION_11 SFX_SOURCE_SOUND_11
#define SFX_SPRING SFX_SOURCE_SOUND_16
#define SFX_ITEM SFX_SOURCE_SOUND_18
#define SFX_CLINK SFX_SOURCE_SOUND_23
#define SFX_CLICK SFX_SOURCE_SOUND_24
#define SFX_CURSOR SFX_SOURCE_SOUND_28

_Static_assert(
    SFX_SOURCE_SOUND_38 == SFX_SOURCE_SOUND_01 + 37 &&
        MSL_NSAMPS == SFX_SOURCE_SOUND_38 + 1,
    "Maxmod bank must contain all 29 Tyrian SFX and nine voices"
);

#if defined(AUTOTEST_FULL_LOADOUT_STRESS) || \
    TYRIAN_GBA_DYNAMIC_FRAME_DROP
#define TYRIAN_GBA_PERF_TIMER 1
#else
#define TYRIAN_GBA_PERF_TIMER 0
#endif

#define GBA_WAITCNT (*(volatile u16 *)0x04000204)
#define GBA_WAITCNT_ROM_PREFETCH_3_1 0x4317u

/*
 * Development-only override.  It is temporarily enabled for interactive
 * campaign validation; forced-death regressions explicitly compile it out,
 * so the translated shield, armor and death path remains covered.
 */
#ifndef TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
#define TYRIAN_GBA_DEV_PLAYER_INVINCIBLE 1
#endif
#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 0 && \
    TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 1
#error TYRIAN_GBA_DEV_PLAYER_INVINCIBLE must be 0 or 1
#endif

/*
 * Diagnostic upper-bound build.  The ordinary release keeps the translated
 * Pulse-Cannon adapter; the dedicated stress target enables six stock HDT
 * weapon sources at once without changing campaign equipment/save state.
 */
#ifndef TYRIAN_GBA_STRESS_LOADOUT
#define TYRIAN_GBA_STRESS_LOADOUT 0
#endif
#if TYRIAN_GBA_STRESS_LOADOUT != 0 && TYRIAN_GBA_STRESS_LOADOUT != 1
#error TYRIAN_GBA_STRESS_LOADOUT must be 0 or 1
#endif

/*
 * Deterministic source-route tests retain their established power-11
 * workload without locking the playable ROM.  Zero means "use campaign
 * state"; non-zero values are accepted only by AUTOTEST builds.
 */
#ifndef TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER
#define TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER 0
#endif
#if TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER > 11
#error TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER must be 0..11
#endif
#if !defined(AUTOTEST) && TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER != 0
#error Fixed front-weapon power is restricted to AUTOTEST builds
#endif
#if TYRIAN_GBA_STRESS_LOADOUT
/*
 * The upper-bound build spends IWRAM on larger hot collision/render paths.
 * Its cold report-only counters can live in EWRAM, preserving the 6 KiB
 * stack/heap safety margin without slowing gameplay work.
 */
#define STRESS_EWRAM_BSS EWRAM_BSS
#define STRESS_COLD_BSS STRESS_EWRAM_BSS
#else
#define STRESS_EWRAM_BSS
#define STRESS_COLD_BSS
#endif

/*
 * Presentation-only projectile culling is safe for ordinary and stress
 * builds: collision remains in source coordinates, while work for an OBJ
 * that cannot be submitted is rejected before touching the tile cache.
 * Dedicated stress variants override these switches for A/B measurement.
 */
#ifndef TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
#define TYRIAN_GBA_PROJECTILE_PRECACHE_CULL 1
#endif
#ifndef TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
#define TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK 1
#endif
#ifndef TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION
#define TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION 0
#endif
#ifndef TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
#define TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER 0
#endif
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL != 0 && \
    TYRIAN_GBA_PROJECTILE_PRECACHE_CULL != 1
#error TYRIAN_GBA_PROJECTILE_PRECACHE_CULL must be 0 or 1
#endif
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK != 0 && \
    TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK != 1
#error TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK must be 0 or 1
#endif
#if TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION != 0 && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION != 1
#error TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION must be 0 or 1
#endif
#if TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER != 0 && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER != 1
#error TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER must be 0 or 1
#endif
#if !TYRIAN_GBA_STRESS_LOADOUT && \
    (TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION || \
        TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER)
#error Stress diagnostic skips require TYRIAN_GBA_STRESS_LOADOUT
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
#define FRONTEND_STATS_FONT_GLYPH_COUNT \
    (JUKEBOX_FONT_TILE_COUNT - 1u)
#define FRONTEND_STATS_FONT_TILES_PER_GLYPH 4u
#define FRONTEND_STATS_TILE_BASE SOURCE_ENEMY_CACHE_LOWER_TILE_BASE
#define FRONTEND_STATS_CUBE_TILE_BASE \
    ((FRONTEND_STATS_TILE_BASE + \
        FRONTEND_STATS_FONT_GLYPH_COUNT * \
            FRONTEND_STATS_FONT_TILES_PER_GLYPH + 15u) & ~15u)
#define FRONTEND_STATS_CUBE_TILE_COUNT 16u
#define FRONTEND_STATS_TILE_COUNT \
    (FRONTEND_STATS_CUBE_TILE_BASE - FRONTEND_STATS_TILE_BASE + \
        FRONTEND_STATS_CUBE_TILE_COUNT)
#define FRONTEND_STATS_TILE_BYTES \
    (FRONTEND_STATS_TILE_COUNT * 32u)
#define MAP_RING_ROWS 32
#define BG_MAP_COLUMNS 64
#define MAP_ROW_BYTES (BG_MAP_COLUMNS * sizeof(u16))
#define MAP_HALF_ROW_BYTES (32 * sizeof(u16))
#define MAP_SCREEN_BLOCK_WORDS (32 * 32)

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
enum {
    GBA_DISPLAY_FRAME_CYCLES = 280896,
    PRESENTATION_DEADLINE_GUARD_CYCLES = 8192,
    PRESENTATION_INITIAL_RENDER_ESTIMATE = 150000,
    PRESENTATION_MAX_CATCHUP_TICKS = 3,
};
#if TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
#define PRESENTATION_MAX_PENDING_LOGIC_TICKS 2
#else
#define PRESENTATION_MAX_PENDING_LOGIC_TICKS 3
#endif
#endif

/*
 * GBA has 128 hardware OBJ entries and substantially more CPU time than the
 * NES/SNES low-detail proofs.  These pools intentionally raise the first
 * level's concurrency while staying under a conservative scanline budget.
 */
/* OpenTyrian shots.h MAX_PWEAPON. */
#define MAX_PLAYER_SHOTS 81
#define MAX_ENEMY_SHOTS 60
/* varz.h MAX_EXPLOSIONS: preserve the source allocator before OAM clipping. */
#define MAX_EFFECTS 200
#define MAX_VISIBLE_EFFECTS 48
/*
 * Source rewards live in the 100-entry enemy pool.  This legacy animation
 * pool remains only for the isolated visual regression fixture.
 */
#define MAX_REWARDS 16
#if TYRIAN_GBA_STRESS_LOADOUT
/* OpenTyrian varz.h MAX_EXPLOSIONS. */
#define MAX_PICKUP_EXPLOSIONS 200
#else
/* Fixed pickup labels; ordinary combat explosions use MAX_EFFECTS. */
#define MAX_PICKUP_EXPLOSIONS 16
#endif
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
#define CASH_COUNTER_Y 148
/*
 * Runtime Sprite2 presentation.  The original PC 256-colour indices are
 * decoded from ROMFS and presented through eight time-shared OBJ banks.
 */
#define SOURCE_LEVEL_PALETTE_INDEX 5
#define SOURCE_ENEMY_DYNAMIC_PALETTE_BANK_COUNT 8
#define SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT 8
#define SOURCE_ENEMY_FRAME_BYTES 1024
#define SOURCE_ENEMY_TILES_PER_SLOT 32
#define SOURCE_PLAYER_CACHE_TILE_BASE OBJ_TILE_PLAYER_0
#define SOURCE_SPRITE2_L2_SLOT_COUNT 64
#define SOURCE_SPRITE2_L2_FRAME_BYTES SOURCE_ENEMY_FRAME_BYTES
/*
 * The previous pre-rendered boss atlas occupied OBJ tiles 32..95, but the
 * source-parity runtime draws every boss component from ROMFS Sprite2 data.
 * Reclaim its two 32-tile 8bpp slots without overlapping the streamed
 * explosion bank which begins at tile 96.
 */
#define SOURCE_ENEMY_CACHE_RECLAIMED_TILE_BASE 32
#define SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT 2
#define SOURCE_ENEMY_CACHE_LOWER_TILE_BASE 224
#define SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT 9
#if TYRIAN_GBA_STRESS_LOADOUT
/*
 * The stress ROM trades three 32x32 enemy slots for ten additional 16x16
 * projectile slots, then reserves the final four 32x32 slots for the
 * source Super Bomb's 80x79 OPTION_SHAPES framebuffer-blend graphic.  The
 * GBA adapter centre-crops that source graphic to the hardware's largest
 * square OBJ (64x64), preserving 1:1 pixels and using hardware alpha.
 */
#define SOURCE_ENEMY_CACHE_UPPER_TILE_BASE 720
#define SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT 5
#else
#define SOURCE_ENEMY_CACHE_UPPER_TILE_BASE 640
#define SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT 12
#endif
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
#define SOURCE_PROJECTILE_TILES_PER_SLOT 8
#define SOURCE_PROJECTILE_CACHE_LOWER_TILE_BASE OBJ_TILE_REWARD
#define SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT 7
#define SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE \
    (OBJ_TILE_PLAYER_SHOT + 4)
#define SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT 1
#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE 640
#define SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT 10
#else
/*
 * Episode 4 can present nine or ten distinct authored projectile frames in
 * one scanout.  Reserve two 16x16 slots from the otherwise overprovisioned
 * explosion cache tail instead of dropping a visible shot.  Twenty-eight
 * explosion slots remain, above the measured high-detail peak of seventeen.
 */
#define SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE 208
#define SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT 2
#endif
#define SOURCE_PROJECTILE_CACHE_SLOT_COUNT \
    (SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT + \
        SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT + \
        SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT)
#define SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE \
    (SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE + \
        SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT * \
            SOURCE_PROJECTILE_TILES_PER_SLOT)
#define SOURCE_PROJECTILE_FRAME_BYTES \
    (SOURCE_PROJECTILE_TILES_PER_SLOT * 32)
#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_OPTION_PROJECTILE_SOURCE_TABLE 5
#define SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC 33
#define SOURCE_OPTION_PROJECTILE_SOURCE_WIDTH 80
#define SOURCE_OPTION_PROJECTILE_SOURCE_HEIGHT 79
#define SOURCE_OPTION_PROJECTILE_CROP_X 8
#define SOURCE_OPTION_PROJECTILE_CROP_Y 7
#define SOURCE_OPTION_PROJECTILE_SIZE 64
#define SOURCE_OPTION_PROJECTILE_TILES 128
#define SOURCE_OPTION_PROJECTILE_BYTES \
    (SOURCE_OPTION_PROJECTILE_TILES * 32)
#define SOURCE_OPTION_PROJECTILE_TILE_BASE \
    (SOURCE_ENEMY_CACHE_UPPER_TILE_BASE + \
        SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT * \
            SOURCE_ENEMY_TILES_PER_SLOT)
#else
/*
 * Upgrade option 9 (Plasma Storm) uses OPTION_SHAPES 22..24 through
 * blit_sprite_blend().  Keep all three authored frames resident so one
 * three-projectile volley never aliases its own graphics.  The middle
 * source sprite is 65 pixels wide; GBA crops its final column to the
 * hardware's 64-pixel wide-OBJ limit while retaining 1:1 pixels.
 */
#define SOURCE_OPTION_PROJECTILE_SOURCE_TABLE 5
#define SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC 22
#define SOURCE_OPTION_PROJECTILE_FRAME_COUNT 3
#define SOURCE_OPTION_PROJECTILE_SIZE_X 64
#define SOURCE_OPTION_PROJECTILE_SIZE_Y 32
#define SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME 32
#define SOURCE_OPTION_PROJECTILE_PALETTE_BANK 12
#define SOURCE_OPTION_PROJECTILE_TILES \
    (SOURCE_OPTION_PROJECTILE_FRAME_COUNT * \
        SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME)
#define SOURCE_OPTION_PROJECTILE_BYTES \
    (SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME * 32)
#define SOURCE_OPTION_PROJECTILE_TILE_BASE \
    (SOURCE_ENEMY_CACHE_UPPER_TILE_BASE + \
        (SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT - \
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT) * \
            SOURCE_ENEMY_TILES_PER_SLOT)
#endif

/*
 * Enemy 8bpp frames reclaim the middle of the old fully-resident explosion
 * atlas.  Active 16x16 explosion frames are therefore streamed into a 4bpp
 * cache at the original explosion base.  Stress builds retain all 32 slots;
 * release builds reserve the unused four-slot tail for two Episode 4
 * projectile frames.
 */
#define SOURCE_EFFECT_CACHE_TILE_BASE OBJ_TILE_EXPLOSION
#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_EFFECT_CACHE_SLOT_COUNT 32
#else
#define SOURCE_EFFECT_CACHE_SLOT_COUNT 28
#endif
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
    OBJ_SECRET_LEVEL_UNIQUE_GLYPH_COUNT == 8 &&
        OBJ_SECRET_LEVEL_TILE_COUNT ==
            OBJ_SECRET_LEVEL_UNIQUE_GLYPH_COUNT * 2,
    "SECRET LEVEL label must contain eight unique 8x16 glyphs"
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
    OBJ_TILE_SECRET_LEVEL_RUNTIME == OBJ_TILE_GAME_OVER_RUNTIME &&
        OBJ_SECRET_LEVEL_TILE_COUNT <= OBJ_GAME_OVER_TILE_COUNT,
    "SECRET LEVEL must share the transient gameplay status bank"
);
_Static_assert(
    OBJ_INSERT_COIN_UNIQUE_GLYPH_COUNT == 8 &&
        OBJ_INSERT_COIN_TILE_COUNT ==
            OBJ_INSERT_COIN_UNIQUE_GLYPH_COUNT * 2 &&
        OBJ_TILE_INSERT_COIN_RUNTIME == OBJ_TILE_GAME_OVER_RUNTIME &&
        OBJ_INSERT_COIN_TILE_COUNT <= OBJ_GAME_OVER_TILE_COUNT,
    "INSERT COIN must contain eight SMALL_FONT glyphs in the transient bank"
);
_Static_assert(
    OBJ_TILE_GAME_OVER_SOURCE ==
#if TYRIAN_GBA_STRESS_LOADOUT
        SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE,
#else
        SOURCE_ENEMY_CACHE_UPPER_TILE_BASE,
#endif
    "GAME OVER cartridge bank must use the first time-shared upper slot"
);
_Static_assert(
    JUKEBOX_MUSIC_COUNT == 41 &&
        MSL_NSONGS == JUKEBOX_MUSIC_COUNT + 3 &&
        MOD_TYRIAN_MUSIC_00 == 0 &&
        MOD_TYRIAN_MUSIC_29 == 29 &&
        MOD_TYRIAN_MUSIC_40 == 40 &&
        MOD_TYRIAN_MUSIC_09_ONCE == 41 &&
        MOD_TYRIAN_MUSIC_10_ONCE == 42 &&
        MOD_TYRIAN_MUSIC_30_ONCE == 43,
    "Maxmod catalog order or finite source-cue modules changed"
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
        OT_COMP_SHAPE_TABLE_OPTIONS_SMALL &&
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
    SOURCE_PLAYER_CACHE_TILE_BASE == 0 &&
        SOURCE_PLAYER_CACHE_TILE_BASE +
            SOURCE_ENEMY_TILES_PER_SLOT <=
            SOURCE_ENEMY_CACHE_RECLAIMED_TILE_BASE,
    "dedicated source-player frame overlaps the enemy Sprite2 cache"
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
        SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT *
            SOURCE_PROJECTILE_TILES_PER_SLOT <=
        OBJ_TILE_BOSS_BAR,
    "runtime projectile cache overlaps retained OBJ assets"
);
_Static_assert(
    SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE +
        SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT *
            SOURCE_PROJECTILE_TILES_PER_SLOT <=
#if TYRIAN_GBA_STRESS_LOADOUT
        SOURCE_ENEMY_CACHE_UPPER_TILE_BASE,
    "extra projectile cache overlaps the upper enemy cache"
#else
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE,
    "extra projectile cache overlaps the lower enemy cache"
#endif
);
_Static_assert(
    SOURCE_OPTION_PROJECTILE_TILE_BASE +
        SOURCE_OPTION_PROJECTILE_TILES <=
        OBJ_TILE_COUNT,
    "source OPTION_SHAPES projectile exceeds OBJ VRAM"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_UPPER_TILE_BASE +
        SOURCE_ENEMY_CACHE_UPPER_SLOT_COUNT *
            SOURCE_ENEMY_TILES_PER_SLOT <=
#if TYRIAN_GBA_STRESS_LOADOUT
        SOURCE_OPTION_PROJECTILE_TILE_BASE,
#else
        OBJ_TILE_COUNT,
#endif
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
#if TYRIAN_GBA_STRESS_LOADOUT
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE,
#else
        SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE,
#endif
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
    STATE_UPGRADE_MENU = 12,
    STATE_UPGRADE_SUBMENU = 13,
    STATE_QUIT_CONFIRM = 14,
};

enum {
    FRONTEND_PLAY_FULL_GAME = 0,
    FRONTEND_PLAY_ARCADE = 1,
    FRONTEND_DIFFICULTY_EASY = 1,
    FRONTEND_DIFFICULTY_NORMAL = 2,
    FRONTEND_DIFFICULTY_HARD = 3,
};

typedef struct {
    u8 episode;
    char level_name[11];
    u8 lvl_file_number;
    u8 front_weapon;
    u8 rear_weapon;
    u8 super_arcade_mode;
    u8 sidekick[2];
    u8 generator;
    u8 sidekick_level;
    u8 sidekick_series;
    u8 initial_episode;
    u8 shield;
    u8 special;
    u8 ship;
    u8 front_power;
    u8 rear_power;
    u8 source_song;
} FrontendDemoHeader;

typedef struct {
    u8 id;
    u8 power;
} FrontendWeaponItem;

/*
 * One-player subset of OpenTyrian's PlayerItems.  This remains persistent
 * while the level-port state is recreated for each ROMFS LVL section.
 */
typedef struct {
    u8 ship;
    FrontendWeaponItem weapon[2];
    u8 shield;
    u8 generator;
    u8 sidekick[2];
    u8 special;
    u8 sidekick_level;
    u8 sidekick_series;
    u8 super_arcade_mode;
    u8 weapon_mode;
} FrontendPlayerItems;

#define BOX_OVERLAPS(ax, ay, aw, ah, bx, by, bw, bh) \
    ((ax) + (aw) > (bx) && (bx) + (bw) > (ax) && \
     (ay) + (ah) > (by) && (by) + (bh) > (ay))

extern const u8 obj_tiles[];
extern const u8 obj_palette[];
extern const u8 secret_level_palettes[];
extern const u8 insert_coin_palette[];
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
    u8 ttl;
    u8 damage;
    u8 infinite;
    u8 animation;
    u8 animation_max;
    u8 trail;
    u8 reserved;
    u16 graphic;
    u16 render_graphic;
    u16 chain_weapon;
    u8 aim_enemy;
    u8 aim_delay;
    u8 aim_delay_max;
    u8 blast_filter;
    s16 x;
    s16 y;
    s16 xm;
    s16 ym;
    s8 xc;
    s8 yc;
    u8 complicated;
    u8 circle_size_x;
    u8 circle_size_y;
    s8 dev_x;
    s8 dir_x;
    s8 dev_y;
    s8 dir_y;
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
    u8 fixed_position;
    s8 delta_y;
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

enum {
    SECRET_LEVEL_TEXT_GLYPH_COUNT = 12,
};

/* Unique source bank order: S, E, C, R, T, L, V, !. */
static const u8 secret_level_glyph_map[
    SECRET_LEVEL_TEXT_GLYPH_COUNT
] = {
    0, 1, 2, 3, 1, 4,
    5, 1, 6, 1, 5, 7,
};

static const u8 secret_level_glyph_advances[
    OBJ_SECRET_LEVEL_UNIQUE_GLYPH_COUNT
] = {
    OBJ_SECRET_LEVEL_ADVANCE_0,
    OBJ_SECRET_LEVEL_ADVANCE_1,
    OBJ_SECRET_LEVEL_ADVANCE_2,
    OBJ_SECRET_LEVEL_ADVANCE_3,
    OBJ_SECRET_LEVEL_ADVANCE_4,
    OBJ_SECRET_LEVEL_ADVANCE_5,
    OBJ_SECRET_LEVEL_ADVANCE_6,
    OBJ_SECRET_LEVEL_ADVANCE_7,
};

enum {
    INSERT_COIN_TEXT_GLYPH_COUNT = 11,
    INSERT_COIN_SPACE_GLYPH = 0xff,
};

/* Unique source bank order: I, N, S, E, R, T, C, O. */
static const u8 insert_coin_glyph_map[
    INSERT_COIN_TEXT_GLYPH_COUNT
] = {
    0, 1, 2, 3, 4, 5,
    INSERT_COIN_SPACE_GLYPH,
    6, 7, 0, 1,
};

static const u8 insert_coin_glyph_advances[
    OBJ_INSERT_COIN_UNIQUE_GLYPH_COUNT
] = {
    OBJ_INSERT_COIN_ADVANCE_0,
    OBJ_INSERT_COIN_ADVANCE_1,
    OBJ_INSERT_COIN_ADVANCE_2,
    OBJ_INSERT_COIN_ADVANCE_3,
    OBJ_INSERT_COIN_ADVANCE_4,
    OBJ_INSERT_COIN_ADVANCE_5,
    OBJ_INSERT_COIN_ADVANCE_6,
    OBJ_INSERT_COIN_ADVANCE_7,
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
static FrontendDemoHeader frontend_demo_header EWRAM_BSS;
static OtFile *frontend_demo_file EWRAM_BSS;
static u16 frontend_demo_keys_wait EWRAM_BSS;
static u16 frontend_title_idle_frames EWRAM_BSS;
static u8 frontend_demo_keys EWRAM_BSS;
static u8 frontend_demo_number EWRAM_BSS;
static u8 frontend_demo_active EWRAM_BSS;
static u8 frontend_demo_eof EWRAM_BSS;
static u8 frontend_demo_input_guard EWRAM_BSS;
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
static FrontendPlayerItems frontend_player_items EWRAM_BSS;
static FrontendPlayerItems frontend_upgrade_original_items EWRAM_BSS;
static u8 frontend_player_items_valid;
static u8 frontend_upgrade_category EWRAM_BSS;
static u8 frontend_upgrade_sub_count EWRAM_BSS;
static u8 frontend_upgrade_sub_item[
    OT_EPISODE_ITEM_GROUP_CAPACITY + 1
] EWRAM_BSS;
static u8 frontend_upgrade_sub_power[
    OT_EPISODE_ITEM_GROUP_CAPACITY + 1
] EWRAM_BSS;
static u8 frontend_upgrade_sub_scroll EWRAM_BSS;
static u32 frontend_upgrade_original_cash EWRAM_BSS;
static u32 frontend_upgrade_trade_cash EWRAM_BSS;
static u8 frontend_quit_yes EWRAM_BSS;
static s16 frontend_nav_x EWRAM_BSS;
static s16 frontend_nav_y EWRAM_BSS;
static s16 frontend_nav_target_x EWRAM_BSS;
static s16 frontend_nav_target_y EWRAM_BSS;
static u8 frontend_nav_planet_animation EWRAM_BSS;
static u8 frontend_nav_animation_wait EWRAM_BSS;
static u8 frontend_nav_dot_animation EWRAM_BSS;
static u8 frontend_nav_dot_wait EWRAM_BSS;
static u16 frontend_timer EWRAM_BSS;
static u8 frontend_stats_stage EWRAM_BSS;
static u8 frontend_stats_cube_visible_count EWRAM_BSS;
static u16 frontend_stats_timer EWRAM_BSS;
static u8 frontend_level_completed EWRAM_BSS;
static u8 frontend_stats_overlay_active EWRAM_BSS;
static u8 frontend_stats_tiles_pending EWRAM_BSS;
static u8 frontend_stats_palette_dirty EWRAM_BSS;
static u8 frontend_stats_font_ready EWRAM_BSS;
static u8 frontend_stats_scene_oam_count EWRAM_BSS;
static s8 frontend_stats_glow_brightness EWRAM_BSS;
static u8 frontend_stats_glyph_width[
    FRONTEND_STATS_FONT_GLYPH_COUNT
] EWRAM_BSS;
static u16 frontend_stats_obj_palette[48] EWRAM_BSS;
static u8 frontend_mode4_active;
static u8 frontend_display_page EWRAM_BSS;
static u8 frontend_frame_pending EWRAM_BSS;
static u8 frontend_pending_kind EWRAM_BSS;
static u8 frontend_patch_state EWRAM_BSS;
static u8 frontend_patch_old_selection EWRAM_BSS;
static u8 frontend_patch_new_selection EWRAM_BSS;
static u8 frontend_dirty_count EWRAM_BSS;
static u16 frontend_dirty_x[8] EWRAM_BSS;
static u16 frontend_dirty_y[8] EWRAM_BSS;
static u16 frontend_dirty_width[8] EWRAM_BSS;
static u16 frontend_dirty_height[8] EWRAM_BSS;
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
/*
 * Frontend Sprite2 decoding is only used while the first 38.4 KiB Mode-4
 * frame is live.  Keep its 2 KiB 16-bit decode canvas in the otherwise
 * unused tail of the shared 64 KiB arena instead of charging EWRAM twice.
 */
#define frontend_sprite2_decode_scratch \
    ((u16 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_FRAME_BYTES \
    ))
#define FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES (160u * 14u)
#define frontend_quit_choice_background \
    ((u8 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_FRAME_BYTES + \
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) \
    ))
_Static_assert(
    FRONTEND_FRAME_BYTES +
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) <=
        sizeof(FrontendGameplayArena),
    "frontend Sprite2 decode canvas must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_FRAME_BYTES +
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) +
        FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES <=
        sizeof(FrontendGameplayArena),
    "frontend quit-choice cache must fit the shared arena tail"
);
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
static s8 player_bank;
static u16 player_ship_graphic;
static u8 player_ship_animation;
static u8 player_generator_power;
static u32 player_cash;

#if !TYRIAN_GBA_STRESS_LOADOUT
enum {
    SOURCE_WEAPON_BAY_FRONT = 0,
    SOURCE_WEAPON_BAY_REAR = 1,
    SOURCE_WEAPON_BAY_LEFT_SIDEKICK = 2,
    SOURCE_WEAPON_BAY_RIGHT_SIDEKICK = 3,
    SOURCE_WEAPON_BAY_MISC = 4,
    SOURCE_WEAPON_BAY_COUNT = 5,
};

typedef struct {
    OtOptionDefinition option;
    OtWeaponDefinition weapon;
    u8 item_id;
    u8 weapon_charge;
    u8 valid;
    u8 weapon_valid;
    u8 ammo;
    u8 ammo_max;
    u16 ammo_refill_ticks;
    u16 ammo_refill_ticks_max;
    u8 animation_enabled;
    u8 animation_frame;
    u8 charge;
    u8 charge_ticks;
    s16 x;
    s16 y;
} SourceSidekickRuntime;

static OtWeaponDefinition source_front_weapon EWRAM_BSS;
static OtWeaponDefinition source_rear_weapon EWRAM_BSS;
static SourceSidekickRuntime source_sidekick[2] EWRAM_BSS;
static u8 source_front_weapon_valid;
static u8 source_front_weapon_bound;
static u8 source_front_weapon_port_id;
static u8 source_front_weapon_power;
static u16 source_front_weapon_hdt_id;
static u8 source_rear_weapon_valid;
static u8 source_rear_weapon_bound;
static u8 source_rear_weapon_port_id;
static u8 source_rear_weapon_power;
static u8 source_rear_weapon_mode;
static u16 source_rear_weapon_hdt_id;
static u8 source_shot_repeat[SOURCE_WEAPON_BAY_COUNT];
static u8 source_shot_multi_pos[SOURCE_WEAPON_BAY_COUNT];
static s16 source_player_old_x[20];
static s16 source_player_old_y[20];
static u8 source_sidekick_orbit_phase;
static s8 source_sidekick_attachment_move;
static u8 source_sidekick_attachment_linked;
static u8 source_sidekick_attachment_return;
static s16 source_player_delta_x;
static s16 source_player_delta_y;
#endif

#if TYRIAN_GBA_STRESS_LOADOUT
enum {
    STRESS_BAY_FRONT = 0,
    STRESS_BAY_REAR = 1,
    STRESS_BAY_LEFT_SIDEKICK = 2,
    STRESS_BAY_RIGHT_SIDEKICK = 3,
    STRESS_BAY_SPECIAL = 4,
    STRESS_BAY_SUPERBOMB = 5,
    STRESS_BAY_EQUIPMENT_COUNT = 6,
    STRESS_BAY_MISC = 6,
    STRESS_BAY_COUNT = 7,
};

static OtWeaponDefinition
    stress_weapon[STRESS_BAY_EQUIPMENT_COUNT] EWRAM_BSS;
static OtOptionDefinition stress_left_option EWRAM_BSS;
static OtOptionDefinition stress_right_option EWRAM_BSS;
static OtSpecialDefinition stress_special EWRAM_BSS;
static u8 stress_loadout_valid;
static u8 stress_shot_repeat[STRESS_BAY_COUNT];
static u8 stress_shot_multi_pos[STRESS_BAY_COUNT];
static u8 stress_sidekick_animation[2];
static s16 stress_sidekick_x[2];
static s16 stress_sidekick_y[2];
static s16 stress_player_delta_x;
static s16 stress_player_delta_y;
#endif
static u8 source_option_projectile_valid;

static u8 boss_bar_flash;
static u8 boss_bar_palette_dirty;
static u8 boss_obj_palette_restore_pending;

static u16 level_tick;
static u16 level_position;
static u32 logic_accumulator;
static u8 level_exit_music_started;
static OtLevelPortState source_parity_level EWRAM_BSS;

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

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
typedef struct {
    u16 hofs[3];
    u16 vofs[3];
    u16 bg1cnt;
    u16 bg2cnt;
    u16 dispcnt;
    u16 bldcnt;
    u16 bldalpha;
    u16 full_scroll_pixel[3];
} GameplayPresentationRegisters;

static GameplayPresentationRegisters gameplay_presentation;
static u32 presentation_render_estimate;
static u32 presentation_render_ewma;
static u32 presentation_render_deviation;
static u8 presentation_render_pending;
static u8 presentation_pending_logic_ticks;
static u8 presentation_registers_valid;
static u8 presentation_release_held_window;
#endif

static u8 oam_count;
static u8 previous_oam_count;
static u8 oam_dirty;
static u8 game_over_tile_upload_pending;
static u8 secret_level_tile_upload_pending;
static u8 insert_coin_tile_upload_pending;
static u8 secret_level_palette_dirty;
static u8 secret_level_music_active;
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
volatile u32 telemetry_player_shot_spawns;
volatile u32 telemetry_player_shot_drops;
volatile u32 telemetry_player_shot_max_active;
volatile u32 telemetry_player_chain_volleys;
volatile u32 telemetry_upgrade_loadout_pass;
volatile u32 telemetry_stress_loadout_failures;
volatile u32 telemetry_stress_option_blend_draws;
volatile u32 telemetry_stress_psg_triggers;
#if TYRIAN_GBA_STRESS_LOADOUT
volatile u32 telemetry_projectile_culled_offscreen_before_cache;
volatile u32 telemetry_projectile_culled_oam_full_before_cache;
volatile u32 telemetry_projectile_post_visibility_acquires;
volatile u32 telemetry_projectile_visible_capacity_drops;
#endif
volatile u32 telemetry_detail_lava_frames;
volatile u32 telemetry_detail_water_frames;
volatile u32 telemetry_detail_iced_frames;
volatile u32 telemetry_detail_blur_frames;
volatile u32 telemetry_detail_wild_frames;
volatile u32 telemetry_effect_cache_hits;
volatile u32 telemetry_effect_cache_misses;
volatile u32 telemetry_effect_cache_evictions;
volatile u32 telemetry_effect_cache_drops;
volatile u32 telemetry_effect_cache_uploads;
volatile u32 telemetry_effect_cache_upload_bytes;
volatile u32 telemetry_effect_cache_max_uploads;
volatile u32 telemetry_effect_cache_max_visible_unique;
volatile u32 telemetry_state_transitions;
volatile u32 telemetry_frontend_full_redraws;
volatile u32 telemetry_frontend_dirty_commits;
volatile u32 telemetry_frontend_dirty_bytes;
volatile u32 telemetry_frontend_runtime_shp_decodes;
volatile u32 telemetry_frontend_runtime_sprite2_decodes;
volatile u32 telemetry_romfs_entries STRESS_COLD_BSS;
volatile u32 telemetry_romfs_image_bytes STRESS_COLD_BSS;
volatile u32 telemetry_romfs_payload_bytes STRESS_COLD_BSS;
volatile u32 telemetry_romfs_checks STRESS_COLD_BSS;
volatile u32 telemetry_romfs_failures STRESS_COLD_BSS;
volatile u32 telemetry_romfs_manifest_crc32 STRESS_COLD_BSS;
volatile u32 telemetry_layer_rule_checks;
volatile u32 telemetry_layer_rule_failures;
volatile u32 telemetry_pickup_explosion_spawns;
volatile u32 telemetry_pickup_explosion_drops;
volatile u32 telemetry_pickup_explosion_max_active;
volatile u32 telemetry_end_level_music_starts;
volatile u32 telemetry_end_level_music_natural_stops;
volatile u32 telemetry_end_level_initial_warp;
volatile u32 telemetry_end_level_trail_max;
volatile u32 telemetry_level_complete_voice_starts;
#ifdef AUTOTEST
volatile u32 telemetry_source_sound_mask_low;
volatile u32 telemetry_source_sound_mask_high;
volatile u32 telemetry_secret_level_collision_pass;
volatile u32 telemetry_arcade_equipment_collision_pass;
#endif
volatile u32 telemetry_demo_starts;
volatile u32 telemetry_demo_idle_starts;
volatile u32 telemetry_demo_finishes;
volatile u32 telemetry_demo_aborts;
volatile u32 telemetry_demo_parse_failures;
volatile u32 telemetry_stats_stage_advances;
volatile u32 telemetry_stats_cube_reveals;
volatile u32 telemetry_player_death_large_explosions;
volatile u32 telemetry_player_death_sfx_9;
volatile u32 telemetry_player_death_sfx_11;
volatile u32 telemetry_player_death_sfx_22;
volatile u32 telemetry_player_death_music_fade_steps;
volatile u32 telemetry_game_over_music_starts;
volatile u32 telemetry_game_over_music_natural_stops;
volatile u32 telemetry_game_over_overlay_frames;
volatile u32 telemetry_game_over_exits;
volatile u32 telemetry_boss_perf_started STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_completed STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_start_position STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_end_position STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_display_frames STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_missed_vblanks STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_sprite2_misses STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_sprite2_evictions STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_sprite2_upload_bytes STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_projectile_misses STRESS_COLD_BSS;
volatile u32 telemetry_sprite2_l2_hits;
volatile u32 telemetry_sprite2_l2_misses;
volatile u32 telemetry_sprite2_l2_evictions;
volatile u32 telemetry_sprite2_l2_drops;
volatile u32 telemetry_sprite2_l2_flushes;
volatile u32 telemetry_sprite2_l2_raw_builds;
volatile u32 telemetry_sprite2_l2_rle_fallbacks;
volatile u32 telemetry_sprite2_l2_max_visible_unique;
volatile u32 telemetry_boss_perf_l2_hits STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_l2_misses STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_l2_evictions STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_l2_raw_builds STRESS_COLD_BSS;
volatile u32 telemetry_boss_perf_l2_fallbacks STRESS_COLD_BSS;
volatile u32 telemetry_waitcnt;
volatile u32 telemetry_wall_vblanks;
volatile u32 telemetry_presentation_render_attempts;
volatile u32 telemetry_presentation_render_completed;
volatile u32 telemetry_presentation_render_deferred;
volatile u32 telemetry_presentation_render_forced;
volatile u32 telemetry_presentation_superseded;
volatile u32 telemetry_presentation_pending_logic_max;
volatile u32 telemetry_presentation_estimate_max;
volatile u32 telemetry_presentation_deadline_elapsed_max;
volatile u32 telemetry_logic_catchup_updates;
volatile u32 telemetry_logic_updates_per_loop_max;
volatile u32 telemetry_logic_backlog_frames_max;
volatile u32 telemetry_background_held_rows_max;
volatile u32 telemetry_vblank_recovery_loops;
volatile u32 telemetry_audio_frames;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
volatile u32 telemetry_stress_logic_cycles_total;
volatile u32 telemetry_stress_logic_cycles_max;
volatile u32 telemetry_stress_render_cycles_total;
volatile u32 telemetry_stress_render_cycles_max;
volatile u32 telemetry_stress_collision_cycles_total;
volatile u32 telemetry_stress_collision_cycles_max;
#endif
#if TYRIAN_GBA_PERF_TIMER
static volatile u8 perf_timer_ready;
static volatile u32 perf_vblank_cycle_stamp;
volatile u32 telemetry_perf_vblank_irq_cycles_last;
volatile u32 telemetry_perf_vblank_irq_cycles_total;
volatile u32 telemetry_perf_vblank_irq_cycles_max;
volatile u32 telemetry_perf_commit_cycles_total;
volatile u32 telemetry_perf_commit_cycles_max;
volatile u32 telemetry_perf_audio_input_cycles_total;
volatile u32 telemetry_perf_audio_input_cycles_max;
volatile u32 telemetry_perf_prelogic_cycles_total;
volatile u32 telemetry_perf_prelogic_cycles_max;
#endif

static u32 boss_perf_start_display_frames STRESS_COLD_BSS;
static u32 boss_perf_start_missed_vblanks STRESS_COLD_BSS;
static u32 boss_perf_start_sprite2_misses STRESS_COLD_BSS;
static u32 boss_perf_start_sprite2_evictions STRESS_COLD_BSS;
static u32 boss_perf_start_sprite2_upload_bytes STRESS_COLD_BSS;
static u32 boss_perf_start_projectile_misses STRESS_COLD_BSS;
static u32 boss_perf_start_l2_hits STRESS_COLD_BSS;
static u32 boss_perf_start_l2_misses STRESS_COLD_BSS;
static u32 boss_perf_start_l2_evictions STRESS_COLD_BSS;
static u32 boss_perf_start_l2_raw_builds STRESS_COLD_BSS;
static u32 boss_perf_start_l2_fallbacks STRESS_COLD_BSS;

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
static s16 source_world_to_screen_y(s16 source_y);
static u16 source_background_hofs(u8 layer);
static void source_runtime_reset(void);
static void source_player_cache_commit(void);
static void source_enemy_cache_commit(void);
static void source_projectile_cache_commit(void);
static void source_effect_cache_commit(void);
static u8 source_option_projectile_sync(void);
static void source_spawn_explosion(
    s16 x,
    s16 y,
    s8 delta_y,
    u8 type,
    u8 fixed_position
);
static void source_begin_end_level_audio(void);
#if !TYRIAN_GBA_STRESS_LOADOUT
static void source_front_weapon_init(void);
static u8 source_front_weapon_sync(void);
#ifdef AUTOTEST
static u8 autotest_source_upgrade_loadout(void);
#endif
#endif
#if TYRIAN_GBA_STRESS_LOADOUT
static void stress_loadout_init(void);
static void stress_spawn_weapon(
    const OtWeaponDefinition *weapon,
    u8 bay,
    s16 origin_x,
    s16 origin_y
);
#endif
#if TYRIAN_GBA_PERF_TIMER
static inline u32 stress_cycle_counter_read(void)
{
    u16 high_before;
    u16 low;
    u16 high_after;

    /*
     * Timer 2 runs at the 16.78 MHz system clock and Timer 3 cascades.
     * Repeat if the low word rolled over between the two high reads.
     */
    do {
        high_before = REG_TM3CNT_L;
        low = REG_TM2CNT_L;
        high_after = REG_TM3CNT_L;
    } while (high_before != high_after);
    return ((u32)high_before << 16) | low;
}

static inline void stress_cycle_accumulate(
    u32 start,
    volatile u32 *total,
    volatile u32 *maximum
)
{
    u32 elapsed = stress_cycle_counter_read() - start;

    *total += elapsed;
    if (elapsed > *maximum) *maximum = elapsed;
}
#endif
static void frontend_commit_vblank(void);
static void frontend_campaign_apply_to_level(void);
static void frontend_campaign_sync_from_level(void);
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
    u32 vblank_steps;
    u8 logic_updated;
    u8 commit_vblank_now;
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
    u32 observed_vblank;
    u32 vblank_irq_latch_consumed = 0;
#endif
#if TYRIAN_GBA_PERF_TIMER
    u32 stress_cycle_start;
    u32 perf_frame_cycle_start;
#endif
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

#if TYRIAN_GBA_PERF_TIMER
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TIMER_COUNT | TIMER_START;
    REG_TM2CNT_H = TIMER_START;
    perf_timer_ready = 1;
#endif
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
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
        /*
         * A heavy logic/render pass can finish after one or more VBlank IRQs
         * have already run.  Do not call VBlankIntrWait() in that case: SWI 5
         * discards the latched IRQ and would sleep through one extra frame.
         * Consume one counted LCD period per loop so mmFrame(), input and the
         * wall-clock logic accumulator all recover at the correct 59.73 Hz.
         *
         * Active-display recovery loops must not DMA pending rows/OAM.  The
         * next genuinely waited VBlank commits the newest complete scene.
         */
        observed_vblank = telemetry_vblank_irqs;
        if (
            last_vblank_seen &&
            observed_vblank > last_vblank_seen
        ) {
            /*
             * BIOS keeps only a bit for an already-serviced VBlank.  Consume
             * that latch exactly once per newest IRQ count; ReturnFlag=0 is
             * guaranteed to return immediately here because the dispatcher
             * set it before invoking vblank_handler().  Without this step a
             * later wait would mistake the stale latch for a new LCD period.
             */
            if (observed_vblank > vblank_irq_latch_consumed) {
                IntrWait(0, IRQ_VBLANK);
                observed_vblank = telemetry_vblank_irqs;
                vblank_irq_latch_consumed = observed_vblank;
            }
            current_vblank = last_vblank_seen + 1;
            vblank_steps = 1;
            commit_vblank_now = 0;
            telemetry_missed_vblanks++;
            telemetry_vblank_recovery_loops++;
        } else {
            commit_vblank_now = 1;
            /*
             * ReturnFlag=0 also closes the small race between the counter
             * check above and entering BIOS: an IRQ arriving there is
             * consumed immediately instead of being discarded.
             */
            IntrWait(0, IRQ_VBLANK);
            observed_vblank = telemetry_vblank_irqs;
            vblank_irq_latch_consumed = observed_vblank;
            if (
                last_vblank_seen &&
                observed_vblank > last_vblank_seen
            ) {
                /*
                 * Normally this is exactly +1.  Consuming only one keeps the
                 * audio/logic accounting correct even if an emulator or an
                 * unusually long IRQ reports more than one at once.
                 */
                current_vblank = last_vblank_seen + 1;
            } else {
                current_vblank = observed_vblank;
            }
            vblank_steps = 1;
        }
#else
        commit_vblank_now = 1;
        VBlankIntrWait();
        current_vblank = telemetry_vblank_irqs;
        vblank_steps = 1;
        if (
            last_vblank_seen &&
            current_vblank > last_vblank_seen
        ) {
            vblank_steps = current_vblank - last_vblank_seen;
        }
        if (last_vblank_seen && current_vblank > last_vblank_seen + 1) {
            telemetry_missed_vblanks += current_vblank - last_vblank_seen - 1;
        }
#endif
        last_vblank_seen = current_vblank;
#if TYRIAN_GBA_PERF_TIMER
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
        perf_frame_cycle_start = perf_vblank_cycle_stamp;
        if (!perf_frame_cycle_start) {
            perf_frame_cycle_start = stress_cycle_counter_read();
        }
#else
        perf_frame_cycle_start = stress_cycle_counter_read();
#endif
        stress_cycle_start = stress_cycle_counter_read();
#endif
        if (commit_vblank_now) {
            commit_vblank_work();
        }
#if TYRIAN_GBA_PERF_TIMER
        if (commit_vblank_now) {
            stress_cycle_accumulate(
                stress_cycle_start,
                &telemetry_perf_commit_cycles_total,
                &telemetry_perf_commit_cycles_max
            );
        }
#endif
#ifdef AUTOTEST_SCREENSHOT_ENABLED
        if (autotest_screenshot_delay &&
            --autotest_screenshot_delay == 0) {
            __asm__ volatile("swi 3");
        }
#endif
#if TYRIAN_GBA_PERF_TIMER
        stress_cycle_start = stress_cycle_counter_read();
#endif
        mmFrame();
        if (
            game_state == STATE_PLAY ||
            game_state == STATE_GAME_OVER
        ) {
            telemetry_audio_frames++;
        }

        scanKeys();
        pad_now = keysHeld();
        pad_pressed = keysDown();
        if (frontend_demo_active) {
            if (frontend_demo_input_guard > 0) {
                frontend_demo_input_guard--;
            } else if (pad_now != 0 || pad_pressed != 0) {
                frontend_demo_finish(1);
                pad_now = 0;
                pad_pressed = 0;
            }
        }
#if TYRIAN_GBA_PERF_TIMER
        stress_cycle_accumulate(
            stress_cycle_start,
            &telemetry_perf_audio_input_cycles_total,
            &telemetry_perf_audio_input_cycles_max
        );
        {
            u32 prelogic_elapsed =
                stress_cycle_counter_read() - perf_frame_cycle_start;

            telemetry_perf_prelogic_cycles_total += prelogic_elapsed;
            if (
                prelogic_elapsed >
                telemetry_perf_prelogic_cycles_max
            ) {
                telemetry_perf_prelogic_cycles_max = prelogic_elapsed;
            }
        }
#endif
#ifdef AUTOTEST
        if (game_state == STATE_GAME_OVER) {
            pad_now = 0;
            pad_pressed = 0;
        } else if (game_state != STATE_PLAY) {
            if (!autotest_running) autotest_running = 1;
            pad_now = 0;
#ifdef AUTOTEST_JUKEBOX_FLOW
            pad_pressed = autotest_jukebox_input();
#elif defined(AUTOTEST_DEMO_FLOW)
            pad_pressed = autotest_demo_input();
#elif defined(AUTOTEST_FRONTEND_STRESS)
            pad_pressed =
                game_state == STATE_GAME_MENU ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_CAPTURE_STATE)
            /*
             * Deterministically route to the requested front-end page.
             * The normal Game Menu cursor starts on "Play Next Level", so
             * captures of Upgrade Ship and Quit Game need the same explicit
             * menu choice a player would make before pressing A.
             */
            if (
                game_state == STATE_GAME_MENU &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_UPGRADE_MENU ||
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_UPGRADE_SUBMENU
                )
            ) {
                frontend_selection = 2;
            } else if (
                game_state == STATE_UPGRADE_MENU &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_UPGRADE_SUBMENU
            ) {
                /*
                 * The front-weapon inventory has the longest labels and is
                 * the deterministic visual regression page for state 13.
                 */
                frontend_selection = 1;
            } else if (
                game_state == STATE_GAME_MENU &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_QUIT_CONFIRM
            ) {
                frontend_selection = 5;
            }
            pad_pressed =
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE ?
                    0 :
                    KEY_A;
#else
            pad_pressed =
                game_state == STATE_LEVEL_STATS ?
                    (
                        frontend_stats_stage >=
                            FRONTEND_STATS_STAGE_FINAL &&
                        telemetry_end_level_music_natural_stops != 0 ?
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
#ifndef AUTOTEST_FULL_LOADOUT_STRESS
            if (
                telemetry_display_frames == 120 ||
                telemetry_display_frames == 180
            ) {
                pad_pressed = KEY_START;
            }
#endif
        }
#endif

        if (game_state == STATE_GAME_OVER) {
            u8 game_over_logic_updates = 0;

            game_over_update();
            if (game_state == STATE_GAME_OVER) {
                /*
                 * OpenTyrian keeps executing level_loop while GAME OVER is
                 * overlaid; only the player is gone.  Advance the translated
                 * event/enemy/background loop at the same configured source
                 * cadence instead of freezing the final death frame.
                 */
                telemetry_wall_vblanks += vblank_steps;
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                logic_accumulator +=
                    vblank_steps * TYRIAN_GBA_LOGIC_NUMERATOR;
#else
                logic_accumulator += TYRIAN_GBA_LOGIC_NUMERATOR;
#endif
                while (
                    logic_accumulator >=
                        TYRIAN_GBA_LOGIC_DENOMINATOR
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                    &&
                    game_over_logic_updates <
                        PRESENTATION_MAX_CATCHUP_TICKS
#else
                    &&
                    game_over_logic_updates == 0
#endif
                ) {
                    logic_accumulator -=
                        TYRIAN_GBA_LOGIC_DENOMINATOR;
                    update_logic();
                    game_over_logic_updates++;
                }
                render_game();
                background_prefetch_step(
                    game_over_logic_updates != 0 ?
                        0 :
                        BACKGROUND_PREFETCH_IDLE_MISSES
                );
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
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE !=
                        STATE_LEVEL_STATS ||
                    (
                        frontend_stats_stage >=
                            FRONTEND_STATS_STAGE_FINAL &&
                        frontend_stats_timer == 0
                    )
                )
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
                static u16 frontend_stress_updates;
                static u32 frontend_stress_vblank_start;

                if (game_state == STATE_GAME_MENU) {
                    if (!frontend_stress_started) {
                        if (!frontend_frame_pending) {
                            frontend_stress_started = 1;
                            telemetry_missed_vblanks = 0;
                            telemetry_frontend_full_redraws = 0;
                            telemetry_frontend_dirty_commits = 0;
                            telemetry_frontend_dirty_bytes = 0;
                            telemetry_frontend_runtime_shp_decodes = 0;
                            telemetry_frontend_runtime_sprite2_decodes = 0;
                            frontend_stress_vblank_start =
                                telemetry_vblank_irqs;
                            last_vblank_seen = current_vblank;
                        }
                    } else if (
                        frontend_stress_updates < 600 &&
                        !frontend_frame_pending
                    ) {
                        u8 old_selection = frontend_selection;

                        frontend_selection =
                            frontend_selection == 4 ? 5 : 4;
                        frontend_redraw_selection(old_selection);
                        frontend_stress_updates++;
                    } else if (
                        frontend_stress_updates == 600 &&
                        !frontend_frame_pending
                    ) {
                        volatile u8 *sram =
                            (volatile u8 *)0x0E000000;

                        sram[0] = 'T';
                        sram[1] = 'G';
                        sram[2] = 'F';
                        sram[3] = '5';
                        sram_write_u32(4, frontend_stress_updates);
                        sram_write_u32(8, telemetry_missed_vblanks);
                        sram_write_u32(
                            12,
                            telemetry_vblank_irqs -
                                frontend_stress_vblank_start
                        );
                        sram_write_u32(
                            16,
                            frontend_frame_pending
                        );
                        sram_write_u32(
                            20,
                            telemetry_frontend_full_redraws
                        );
                        sram_write_u32(
                            24,
                            telemetry_frontend_dirty_commits
                        );
                        sram_write_u32(
                            28,
                            telemetry_frontend_dirty_bytes
                        );
                        sram_write_u32(
                            32,
                            telemetry_frontend_runtime_shp_decodes
                        );
                        sram_write_u32(
                            36,
                            telemetry_frontend_runtime_sprite2_decodes
                        );
                        sram_write_u32(40, game_state);
                        sram_write_u32(44, frontend_selection);
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
            u8 logic_updates_this_loop = 0;
            u8 rendered_this_loop = 0;
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
            u8 game_state_before_logic = game_state;
#endif

            telemetry_wall_vblanks += vblank_steps;
            if (
                game_state == STATE_PLAY &&
                (pad_pressed & KEY_START)
            ) {
                toggle_pause();
            }
            if (game_paused) {
                telemetry_paused_frames++;
                render_game();
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                presentation_render_pending = 0;
                presentation_pending_logic_ticks = 0;
#endif
                rendered_this_loop = 1;
#ifdef AUTOTEST_SCREENSHOT_PAUSE
                if (!autotest_screenshot_delay) {
                    autotest_screenshot_delay = 2;
                }
#endif
            } else {
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                logic_accumulator +=
                    vblank_steps * TYRIAN_GBA_LOGIC_NUMERATOR;
#else
                logic_accumulator += TYRIAN_GBA_LOGIC_NUMERATOR;
#endif
                while (
                    logic_accumulator >=
                        TYRIAN_GBA_LOGIC_DENOMINATOR
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                    &&
                    logic_updates_this_loop <
                        PRESENTATION_MAX_CATCHUP_TICKS
#else
                    &&
                    logic_updates_this_loop == 0
#endif
                ) {
                    logic_accumulator -= TYRIAN_GBA_LOGIC_DENOMINATOR;
                    logic_updated = 1;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    /*
                     * Wall-clock catch-up may execute more than one source
                     * tick after a delayed VBlank.  Regenerate the scripted
                     * input from the current authoritative state for each
                     * tick so the stress workload remains identical to the
                     * one-tick-per-loop baseline.
                     */
                    pad_now = autotest_input();
                    stress_cycle_start = stress_cycle_counter_read();
#endif
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
                    if (presentation_render_pending) {
                        /*
                         * This update supersedes a scene already waiting for
                         * presentation, so the scheduler will force a render
                         * afterward.  Let background streaming release the
                         * old display-only row before it allocates the new
                         * one; both VRAM and register changes commit together
                         * at the next VBlank.
                         */
                        presentation_release_held_window = 1;
                    }
#endif
                    update_logic();
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    stress_cycle_accumulate(
                        stress_cycle_start,
                        &telemetry_stress_logic_cycles_total,
                        &telemetry_stress_logic_cycles_max
                    );
#endif
                    logic_updates_this_loop++;
                    /*
                     * keysDown() describes one physical sample, not every
                     * source tick recovered after an overrun.  Held input
                     * remains in pad_now; consume edge-triggered input only
                     * on the first catch-up update.
                     */
                    pad_pressed = 0;
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                    gameplay_presentation_mark_logic_update();
#endif
                    if (
                        game_state != STATE_PLAY &&
                        game_state != STATE_GAME_OVER
                    ) {
                        break;
                    }
#if !TYRIAN_GBA_DYNAMIC_FRAME_DROP
                    if (game_state == STATE_PLAY ||
                        game_state == STATE_GAME_OVER) {
                        u32 render_elapsed;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                        stress_cycle_start = stress_cycle_counter_read();
#endif
                        render_game();
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                        render_elapsed =
                            stress_cycle_counter_read() -
                            stress_cycle_start;
                        telemetry_stress_render_cycles_total +=
                            render_elapsed;
                        if (
                            render_elapsed >
                            telemetry_stress_render_cycles_max
                        ) {
                            telemetry_stress_render_cycles_max =
                                render_elapsed;
                        }
#else
                        render_elapsed = 0;
#endif
                        (void)render_elapsed;
                        telemetry_presentation_render_attempts++;
                        telemetry_presentation_render_completed++;
                        rendered_this_loop = 1;
                    }
#endif
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
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                if (logic_updates_this_loop > 1) {
                    telemetry_logic_catchup_updates +=
                        logic_updates_this_loop - 1;
                }
                if (
                    logic_accumulator >=
                    TYRIAN_GBA_LOGIC_DENOMINATOR
                ) {
                    u32 backlog =
                        logic_accumulator /
                        TYRIAN_GBA_LOGIC_DENOMINATOR;

                    if (
                        backlog >
                        telemetry_logic_backlog_frames_max
                    ) {
                        telemetry_logic_backlog_frames_max =
                            backlog;
                    }
                }
#endif
                if (
                    logic_updates_this_loop >
                    telemetry_logic_updates_per_loop_max
                ) {
                    telemetry_logic_updates_per_loop_max =
                        logic_updates_this_loop;
                }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                if (
                    presentation_render_pending &&
                    (
                        game_state == STATE_PLAY ||
                        game_state == STATE_GAME_OVER
                    )
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
                    &&
                    current_vblank >= telemetry_vblank_irqs
#endif
                    &&
                    gameplay_presentation_should_render(
                        perf_frame_cycle_start,
                        game_state != game_state_before_logic
                    )
                ) {
                    u32 render_start =
                        stress_cycle_counter_read();
                    u32 render_elapsed;

                    render_game();
                    render_elapsed =
                        stress_cycle_counter_read() - render_start;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    telemetry_stress_render_cycles_total +=
                        render_elapsed;
                    if (
                        render_elapsed >
                        telemetry_stress_render_cycles_max
                    ) {
                        telemetry_stress_render_cycles_max =
                            render_elapsed;
                    }
#endif
                    gameplay_presentation_complete(render_elapsed);
                    rendered_this_loop = 1;
                }
#endif
            }
            if (game_state == STATE_PLAY) {
                background_prefetch_step(
                    logic_updated || rendered_this_loop ?
                        0 :
                        BACKGROUND_PREFETCH_IDLE_MISSES
                );
            }
            telemetry_display_frames++;
#ifdef AUTOTEST
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            if (
                game_state == STATE_PLAY &&
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                telemetry_wall_vblanks >= 3600
#else
                telemetry_display_frames >= 3600
#endif
            ) {
                autotest_full_loadout_stress_finish();
            }
#endif
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
#ifdef AUTOTEST_DEMO_FLOW
        autotest_demo_maybe_finish();
#endif
    }
}
