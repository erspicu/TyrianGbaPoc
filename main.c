#include <gba.h>
#include <maxmod.h>
#include <string.h>

#include "Configure.h"
#include "res/asset_meta.h"
#include "res/build_version.h"
#include "res/soundbank.h"
#include "res/sprite2_raw_meta.h"
#include "res/tyrian_romfs_meta.h"
#include "src/port_config.h"
#include "src/opentyrian_data.h"
#include "src/opentyrian_level_port.h"
#include "src/opentyrian_rom_io.h"
#include "src/opentyrian_season.h"
#include "src/opentyrian_sprite2.h"

/*
 * Gameplay code predating the complete source sound catalog keeps these
 * descriptive aliases.  Every alias now resolves to the authoritative
 * one-based sndmast.h slot packed as source_sound_01..38.
 */
#define SFX_WEAPON_1 SFX_SOURCE_SOUND_01
#define SFX_SELECT SFX_SOURCE_SOUND_08
#define SFX_EXPLOSION_9 SFX_SOURCE_SOUND_09
#define SFX_EXPLOSION_11 SFX_SOURCE_SOUND_11
#define SFX_SPRING SFX_SOURCE_SOUND_16
#define SFX_ITEM SFX_SOURCE_SOUND_18
#define SFX_CLINK SFX_SOURCE_SOUND_23
#define SFX_CLICK SFX_SOURCE_SOUND_24
#define SFX_CURSOR SFX_SOURCE_SOUND_28

_Static_assert(
    SFX_SOURCE_SOUND_38 == SFX_SOURCE_SOUND_01 + 37 &&
        SFX_SOURCE_XMAS_VOICE_01 == SFX_SOURCE_SOUND_38 + 1 &&
        SFX_SOURCE_XMAS_VOICE_09 ==
            SFX_SOURCE_XMAS_VOICE_01 + 8 &&
        MSL_NSAMPS == SFX_SOURCE_XMAS_VOICE_09 + 1,
    "Maxmod bank must contain 29 SFX, nine voices and nine Xmas voices"
);
_Static_assert(
    TYRIAN_GBA_LAYOUT_CASH_X >= 0 &&
        TYRIAN_GBA_LAYOUT_CASH_X < SCREEN_WIDTH &&
        TYRIAN_GBA_LAYOUT_CASH_Y >= 0 &&
        TYRIAN_GBA_LAYOUT_CASH_Y <= SCREEN_HEIGHT - 8,
    "cash HUD position must remain visible"
);
_Static_assert(
    TYRIAN_GBA_LAYOUT_SHIELD_RIGHT_X >= 18 &&
        TYRIAN_GBA_LAYOUT_SHIELD_RIGHT_X <= SCREEN_WIDTH &&
        TYRIAN_GBA_LAYOUT_ARMOR_RIGHT_X >= 18 &&
        TYRIAN_GBA_LAYOUT_ARMOR_RIGHT_X <= SCREEN_WIDTH &&
        TYRIAN_GBA_LAYOUT_GENERATOR_RIGHT_X >= 18 &&
        TYRIAN_GBA_LAYOUT_GENERATOR_RIGHT_X <= SCREEN_WIDTH,
    "energy HUD right edges must leave room for three digits"
);
_Static_assert(
    TYRIAN_GBA_LAYOUT_SHIELD_Y >= 0 &&
        TYRIAN_GBA_LAYOUT_SHIELD_Y <= SCREEN_HEIGHT - 8 &&
        TYRIAN_GBA_LAYOUT_ARMOR_Y >= 0 &&
        TYRIAN_GBA_LAYOUT_ARMOR_Y <= SCREEN_HEIGHT - 8 &&
        TYRIAN_GBA_LAYOUT_GENERATOR_Y >= 0 &&
        TYRIAN_GBA_LAYOUT_GENERATOR_Y <= SCREEN_HEIGHT - 8,
    "energy HUD rows must remain visible"
);
_Static_assert(
    TYRIAN_GBA_LAYOUT_NEXT_PANEL_X >= 0 &&
        TYRIAN_GBA_LAYOUT_NEXT_PANEL_Y >= 0 &&
        TYRIAN_GBA_LAYOUT_NEXT_PANEL_X +
            TYRIAN_GBA_LAYOUT_NEXT_PANEL_WIDTH <= SCREEN_WIDTH &&
        TYRIAN_GBA_LAYOUT_NEXT_PANEL_Y +
            TYRIAN_GBA_LAYOUT_NEXT_PANEL_HEIGHT <= SCREEN_HEIGHT,
    "Next Level text panel must fit the screen"
);
_Static_assert(
    TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y >= 4 &&
        TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y <= SCREEN_HEIGHT - 10 &&
        TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X >= 32 &&
        TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X < 192 &&
        TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X >= 32 &&
        TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X < 192,
    "quit choices must fit their fast restore cache"
);

#if defined(AUTOTEST_FULL_LOADOUT_STRESS) || \
    defined(AUTOTEST_FRONTEND_TRANSITION_STRESS) || \
    defined(AUTOTEST_ROUTE_PERF) || \
    TYRIAN_GBA_DYNAMIC_FRAME_DROP
#define TYRIAN_GBA_PERF_TIMER 1
#else
#define TYRIAN_GBA_PERF_TIMER 0
#endif

#define GBA_WAITCNT (*(volatile u16 *)0x04000204)
#define GBA_WAITCNT_ROM_PREFETCH_3_1 0x4317u

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

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
/*
 * Generic route stress stop controls.  A non-zero source event position has
 * priority over the historical fixed-duration benchmark so authored visual
 * transitions can be sampled at exact points without depending on display
 * rate or dropped presentation frames.
 */
#ifndef AUTOTEST_FULL_LOADOUT_STRESS_END_POSITION
#define AUTOTEST_FULL_LOADOUT_STRESS_END_POSITION 0
#endif
#ifndef AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS
#define AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS 3600
#endif
#if AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS < 1
#error AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS must be positive
#endif
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

#ifdef AUTOTEST_SPECIAL_WEAPON_ID
#ifndef AUTOTEST_SPECIAL_SHIELD
#define AUTOTEST_SPECIAL_SHIELD 100
#endif
#if AUTOTEST_SPECIAL_WEAPON_ID < 1 || AUTOTEST_SPECIAL_WEAPON_ID > 46
#error AUTOTEST_SPECIAL_WEAPON_ID must select an HDT Special in 1..46
#endif
#if AUTOTEST_SPECIAL_SHIELD < 0 || AUTOTEST_SPECIAL_SHIELD > 255
#error AUTOTEST_SPECIAL_SHIELD must fit the source u8 shield value
#endif
#endif

#define BG0_SCREEN_BLOCK 24
#define BG1_SCREEN_BLOCK 26
#define BG2_SCREEN_BLOCK 28
#define GAMEPLAY_OVERLAY_CHAR_BASE 2
/*
 * Gameplay BG characters occupy physical tiles 0..1477 and the three
 * 512x256 maps occupy screen blocks 24..29.  BG3 therefore stores its
 * visible 20-row map in rows 3..22 of otherwise free screen block 23.  Its
 * character bank uses the remaining 18 tiles in that block plus screen
 * blocks 30..31.  Character base 2 can address both disjoint free regions.
 * The 24-pixel VOFS below makes hardware row 3 appear at LCD row 0.
 */
#define GAMEPLAY_OVERLAY_SCREEN_BLOCK 23
#define GAMEPLAY_OVERLAY_SCREEN_ROW_OFFSET 3u
#define GAMEPLAY_OVERLAY_MAP_VOFS \
    (GAMEPLAY_OVERLAY_SCREEN_ROW_OFFSET * 8u)
#define GAMEPLAY_OVERLAY_MAP_ROWS (SCREEN_HEIGHT / 8)
#define GAMEPLAY_OVERLAY_MAX_TILES 136
#define GAMEPLAY_OVERLAY_TILE_BYTES \
    (GAMEPLAY_OVERLAY_MAX_TILES * 32u)
#define GAMEPLAY_OVERLAY_MAP_ENTRIES \
    (32u * GAMEPLAY_OVERLAY_MAP_ROWS)
#define GAMEPLAY_OVERLAY_MAP_VRAM_OFFSET \
    ( \
        GAMEPLAY_OVERLAY_SCREEN_BLOCK * 2048u + \
        GAMEPLAY_OVERLAY_SCREEN_ROW_OFFSET * 32u * sizeof(u16) \
    )
#define GAMEPLAY_OVERLAY_TILE0_VRAM_OFFSET \
    ( \
        GAMEPLAY_OVERLAY_MAP_VRAM_OFFSET + \
        GAMEPLAY_OVERLAY_MAP_ENTRIES * sizeof(u16) \
    )
#define GAMEPLAY_OVERLAY_TILE0_CAPACITY \
    ( \
        (BG0_SCREEN_BLOCK * 2048u - \
            GAMEPLAY_OVERLAY_TILE0_VRAM_OFFSET) / 32u \
    )
#define GAMEPLAY_OVERLAY_TILE1_SCREEN_BLOCK 30
#define GAMEPLAY_OVERLAY_TILE1_VRAM_OFFSET \
    (GAMEPLAY_OVERLAY_TILE1_SCREEN_BLOCK * 2048u)
#define GAMEPLAY_OVERLAY_TILE1_CAPACITY \
    ((32u * 2048u - GAMEPLAY_OVERLAY_TILE1_VRAM_OFFSET) / 32u)
#define GAMEPLAY_OVERLAY_TILE0_INDEX_BASE \
    ( \
        (GAMEPLAY_OVERLAY_TILE0_VRAM_OFFSET - \
            GAMEPLAY_OVERLAY_CHAR_BASE * 16384u) / 32u \
    )
#define GAMEPLAY_OVERLAY_TILE1_INDEX_BASE \
    ( \
        (GAMEPLAY_OVERLAY_TILE1_VRAM_OFFSET - \
            GAMEPLAY_OVERLAY_CHAR_BASE * 16384u) / 32u \
    )
#define GAMEPLAY_OVERLAY_BLANK_TILE_INDEX \
    GAMEPLAY_OVERLAY_TILE0_INDEX_BASE

_Static_assert(
    GAMEPLAY_OVERLAY_SCREEN_ROW_OFFSET +
            GAMEPLAY_OVERLAY_MAP_ROWS <= 32u,
    "gameplay overlay map rows must fit one screen block"
);
_Static_assert(
    GAMEPLAY_OVERLAY_MAP_VRAM_OFFSET +
            GAMEPLAY_OVERLAY_MAP_ENTRIES * sizeof(u16) <=
        BG0_SCREEN_BLOCK * 2048u,
    "gameplay overlay map must stop before the first world map"
);
_Static_assert(
    GAMEPLAY_OVERLAY_TILE0_VRAM_OFFSET +
            GAMEPLAY_OVERLAY_TILE0_CAPACITY * 32u <=
        BG0_SCREEN_BLOCK * 2048u,
    "gameplay overlay lower tile segment must stop before world maps"
);
_Static_assert(
    GAMEPLAY_OVERLAY_TILE1_SCREEN_BLOCK >= BG2_SCREEN_BLOCK + 2u,
    "gameplay overlay upper tiles must not overlap world maps"
);
_Static_assert(
    GAMEPLAY_OVERLAY_MAX_TILES <=
        GAMEPLAY_OVERLAY_TILE0_CAPACITY +
            GAMEPLAY_OVERLAY_TILE1_CAPACITY,
    "gameplay overlay tile segments must retain the full source budget"
);
_Static_assert(
    GAMEPLAY_OVERLAY_TILE0_INDEX_BASE +
            GAMEPLAY_OVERLAY_TILE0_CAPACITY <= 512u &&
        GAMEPLAY_OVERLAY_TILE1_INDEX_BASE +
            GAMEPLAY_OVERLAY_TILE1_CAPACITY <= 1024u,
    "gameplay overlay tiles must fit character base 2 addressing"
);
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
#define BACKGROUND_PENDING_ROW_CAPACITY (SCREEN_HEIGHT / 8 + 1)

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP || TYRIAN_GBA_PERF_TIMER
enum {
    GBA_DISPLAY_FRAME_CYCLES = 280896,
    PRESENTATION_DEADLINE_GUARD_CYCLES = 8192,
    PRESENTATION_INITIAL_RENDER_ESTIMATE = 150000,
    PRESENTATION_MAX_CATCHUP_TICKS = 3,
};
#endif
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
#if TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
#define PRESENTATION_MAX_PENDING_LOGIC_TICKS 2
#else
#define PRESENTATION_MAX_PENDING_LOGIC_TICKS 3
#endif
#define PRESENTATION_ADAPTIVE_MEDIUM_MAX_PENDING_LOGIC_TICKS \
    TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME
#define PRESENTATION_ADAPTIVE_SEVERE_MAX_PENDING_LOGIC_TICKS \
    TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME
#define PRESENTATION_ADAPTIVE_MEDIUM_MIN_RENDER_LOGIC_TICKS \
    TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME
#define PRESENTATION_ADAPTIVE_SEVERE_MIN_RENDER_LOGIC_TICKS \
    TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME
#define PRESENTATION_ADAPTIVE_GENERAL_PRESSURE_ENTER 4
#define PRESENTATION_ADAPTIVE_GENERAL_MISSED_ENTER 2
#define PRESENTATION_ADAPTIVE_WAVE_PRESSURE_ENTER 2
#define PRESENTATION_ADAPTIVE_WAVE_MISSED_ENTER 1
#define PRESENTATION_ADAPTIVE_SEVERE_PRESSURE 8
#define PRESENTATION_ADAPTIVE_SEVERE_MISSED 4
#define PRESENTATION_ADAPTIVE_PRESSURE_MAX 12
#define PRESENTATION_ADAPTIVE_RECOVERY_LIGHT_RENDERS 16
#define PRESENTATION_ADAPTIVE_RECOVERY_RENDER_CYCLES 150000u
#endif

/*
 * GBA has 128 hardware OBJ entries and substantially more CPU time than the
 * early low-detail proofs.  These pools intentionally raise the first
 * level's concurrency while staying under a conservative scanline budget.
 */
/* OpenTyrian shots.h MAX_PWEAPON. */
#define MAX_PLAYER_SHOTS 81
#define MAX_ENEMY_SHOTS 60
/* varz.h MAX_EXPLOSIONS: preserve the source allocator before OAM clipping. */
#define MAX_EFFECTS 200
/* varz.h MAX_SUPERPIXELS: Ice Beam and other >1000 shot graphics. */
#define MAX_SUPERPIXELS 101
/*
 * Presentation-only budgets.  Gameplay allocators, collisions and authored
 * timing remain at their PC sizes; these limits decide which already-live
 * objects fit in one 128-entry GBA OAM scene.  Structural sprites are
 * reserved dynamically before these decorative/projectile groups render.
 */
#if TYRIAN_GBA_STRESS_LOADOUT
#define MAX_VISIBLE_EFFECTS 48
#define MAX_VISIBLE_ENEMY_SHOTS MAX_ENEMY_SHOTS
#define MAX_VISIBLE_PLAYER_SHOTS MAX_PLAYER_SHOTS
#else
#define MAX_VISIBLE_EFFECTS 21
#define MAX_VISIBLE_ENEMY_SHOTS 32
#define MAX_VISIBLE_PLAYER_SHOTS 36
#endif
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
/*
 * OpenTyrian JE_playerMovement() clamps ordinary single-player gameplay to
 * source Y 10..160.  The soft camera exposes enough of the original 184-line
 * viewport for the ship's opaque 24-line body to remain fully visible at
 * both exact source limits; do not replace these with GBA screen-space
 * bounds or feed camera state back into gameplay coordinates.
 */
#define SOURCE_PLAYER_MIN_Y 10
#define SOURCE_PLAYER_MAX_Y 160
#if defined(AUTOTEST) && !defined(AUTOTEST_FORCE_PLAYER_TOP)
/*
 * Preserve the established combat workload in broad route regressions.
 * AUTOTEST_FORCE_PLAYER_TOP remains available to exercise the release
 * source-space edge itself without changing thousands of authored golden
 * counts whenever a presentation boundary is adjusted.
 */
#define SOURCE_PLAYER_CLAMP_MIN_Y 17
#define SOURCE_PLAYER_CLAMP_MAX_Y 152
#else
#define SOURCE_PLAYER_CLAMP_MIN_Y SOURCE_PLAYER_MIN_Y
#define SOURCE_PLAYER_CLAMP_MAX_Y SOURCE_PLAYER_MAX_Y
#endif
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
/*
 * Runtime Sprite2 presentation.  The original PC 256-colour indices are
 * decoded from ROMFS and presented through eight time-shared OBJ banks.
 */
#define SOURCE_LEVEL_PALETTE_INDEX 5
#define SOURCE_HUD_SPECIAL_SHAPE_TABLE 26
#define SOURCE_HUD_SPECIAL_COOLDOWN_GRAPHIC 93
#define SOURCE_HUD_SPECIAL_READY_GRAPHIC 94
#define SOURCE_HUD_SUPERBOMB_GRAPHIC 304
#define SOURCE_HUD_SUPERBOMB_LIMIT 10
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
#define SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT 2
#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_ENEMY_CACHE_SPLIT_SLOT_COUNT 0
#else
#define SOURCE_ENEMY_CACHE_SPLIT_SLOT_COUNT 1
#endif
#define SOURCE_ENEMY_CACHE_SPLIT_SLOT \
    (SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT)
#define SOURCE_ENEMY_CACHE_SLOT_COUNT \
    (SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT + \
        SOURCE_ENEMY_CACHE_SPLIT_SLOT_COUNT)
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
 * Real campaign saves can combine a power-11 front/rear weapon with two
 * sidekicks and request more than ten distinct 8bpp projectile frames in one
 * scanout.  Keep the fragmented Sprite2 top half at OBJ 160..175, then use
 * the released contiguous explosion tail at OBJ 176..223 for six projectile
 * slots.  Together with the legacy lower/upper regions this yields fourteen
 * slots without changing any PC weapon or collision state.
 */
#define SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE \
    (OBJ_TILE_EXPLOSION + 16u * EXPLOSION_TILES_PER_FRAME + 16u)
#define SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT 6
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
/*
 * Special Weapon OPTION_SHAPES that do not belong to Plasma Storm:
 *   21 = Xega Ball, native 55x54, presented in one 64x64 8bpp OBJ;
 *   33 = Banana Bomb child, native 80x79, presented losslessly as six
 *        hardware-native pieces (64x64, 2x 16x32, 2x 32x16, 16x16).
 *
 * Both banks end immediately before Plasma Storm's three-frame reserve and
 * time-share the upper full-size Sprite2 cache only while the corresponding
 * equipped Special requires them.  The 8bpp path preserves every source
 * palette index; no build-time replacement graphic is introduced.
 */
#define SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC 21u
#define SOURCE_SPECIAL_OPTION_XEGA_WIDTH 55u
#define SOURCE_SPECIAL_OPTION_XEGA_HEIGHT 54u
#define SOURCE_SPECIAL_OPTION_XEGA_TILES 128u
#define SOURCE_SPECIAL_OPTION_XEGA_SLOT_COUNT 4u
#define SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC 33u
#define SOURCE_SPECIAL_OPTION_BANANA_WIDTH 80u
#define SOURCE_SPECIAL_OPTION_BANANA_HEIGHT 79u
#define SOURCE_SPECIAL_OPTION_BANANA_TILES 200u
#define SOURCE_SPECIAL_OPTION_BANANA_SLOT_COUNT 7u
#define SOURCE_SPECIAL_OPTION_BANANA_MAIN_TILE_OFFSET 0u
#define SOURCE_SPECIAL_OPTION_BANANA_RIGHT_TOP_TILE_OFFSET 128u
#define SOURCE_SPECIAL_OPTION_BANANA_RIGHT_BOTTOM_TILE_OFFSET 144u
#define SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_LEFT_TILE_OFFSET 160u
#define SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_MIDDLE_TILE_OFFSET 176u
#define SOURCE_SPECIAL_OPTION_BANANA_CORNER_TILE_OFFSET 192u
#define SOURCE_SPECIAL_OPTION_XEGA_TILE_BASE \
    (SOURCE_OPTION_PROJECTILE_TILE_BASE - \
        SOURCE_SPECIAL_OPTION_XEGA_SLOT_COUNT * \
            SOURCE_ENEMY_TILES_PER_SLOT)
#define SOURCE_SPECIAL_OPTION_BANANA_TILE_BASE \
    (SOURCE_OPTION_PROJECTILE_TILE_BASE - \
        SOURCE_SPECIAL_OPTION_BANANA_SLOT_COUNT * \
            SOURCE_ENEMY_TILES_PER_SLOT)
#endif

/*
 * Enemy 8bpp frames reclaim the middle of the old fully-resident explosion
 * atlas.  Active 16x16 explosion frames are therefore streamed into a 4bpp
 * cache at the original explosion base.  Stress builds retain all 32 slots.
 * Release telemetry reaches 27 unique frames only when every PC explosion
 * is submitted without an OAM budget.  Keep 16 in the contiguous explosion
 * bank, place one overflow frame in the four-tile
 * alignment gap between the generated static OBJ bank and the upper Sprite2
 * cache, time-share one more with the boss-bar tiles only while no boss bar
 * is active, reclaim the final frame of the legacy POC reward atlas, and
 * time-share the unused lower half of the GAME OVER / SECRET LEVEL /
 * INSERT COIN runtime bank. Those labels atomically restore their eight
 * characters when active.  The released contiguous characters expand the
 * 8bpp projectile cache; one 32x16 region remains available to the
 * fragmented 32x32 Sprite2 overflow canvas required by Episode 4/SAVARA.
 */
#define SOURCE_EFFECT_CACHE_TILE_BASE OBJ_TILE_EXPLOSION
#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT 32
#define SOURCE_EFFECT_CACHE_SLOT_COUNT 32
#else
#define SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT 16
#define SOURCE_EFFECT_CACHE_OVERFLOW_SLOT \
    SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT
#define SOURCE_EFFECT_CACHE_OVERFLOW_TILE_BASE OBJ_STATIC_TILE_COUNT
#define SOURCE_EFFECT_CACHE_BOSS_SLOT \
    (SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT + 1u)
#define SOURCE_EFFECT_CACHE_BOSS_TILE_BASE OBJ_TILE_BOSS_BAR
#define SOURCE_EFFECT_CACHE_REWARD_SLOT \
    (SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT + 2u)
#define SOURCE_EFFECT_CACHE_REWARD_TILE_BASE \
    (OBJ_TILE_REWARD + \
        (REWARD_SEQUENCE_COUNT * REWARD_FRAME_COUNT - 1u) * \
            REWARD_TILES_PER_FRAME)
#define SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT 2u
#define SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT \
    (SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT + 3u)
#define SOURCE_EFFECT_CACHE_STATUS_TILE_BASE \
    (OBJ_TILE_GAME_OVER_RUNTIME + 8u)
#define SOURCE_EFFECT_CACHE_SLOT_COUNT \
    (SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT + \
        SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT)
#endif
#define SOURCE_EFFECT_TILES_PER_SLOT EXPLOSION_TILES_PER_FRAME
#define SOURCE_EFFECT_FRAME_BYTES \
    (SOURCE_EFFECT_TILES_PER_SLOT * 32)
#if !TYRIAN_GBA_STRESS_LOADOUT
/*
 * SAVARA can request 24 full 32x32 Sprite2 canvases plus two compact frames
 * in one presentation scene.  OBJ VRAM has no free contiguous 1 KiB bank,
 * so present the 24th full canvas as two hardware-native 32x16 wide OBJs.
 * Its top half occupies the released explosion-cache tail; its bottom half
 * time-shares PAUSED plus the unused legacy player-shot characters.  Pause
 * reserves this slot and restores its glyph bank during VBlank.
 */
#define SOURCE_ENEMY_CACHE_SPLIT_HALF_TILES 16
#define SOURCE_ENEMY_CACHE_SPLIT_HALF_BYTES \
    (SOURCE_ENEMY_CACHE_SPLIT_HALF_TILES * 32)
#define SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE \
    (SOURCE_EFFECT_CACHE_TILE_BASE + \
        SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT * \
            SOURCE_EFFECT_TILES_PER_SLOT)
#define SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE OBJ_TILE_PAUSE_TEXT
#endif

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
    SOURCE_HUD_SPECIAL_SHAPE_TABLE <= SPRITE2_RAW_TABLE_COUNT &&
        SOURCE_HUD_SUPERBOMB_GRAPHIC <=
            SPRITE2_RAW_COMPONENTS_PER_TABLE,
    "stock Special Weapon or Super Bomb Sprite2 source is unavailable"
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
#if !TYRIAN_GBA_STRESS_LOADOUT
_Static_assert(
    SOURCE_SPECIAL_OPTION_BANANA_TILE_BASE >=
        SOURCE_ENEMY_CACHE_UPPER_TILE_BASE &&
        SOURCE_SPECIAL_OPTION_BANANA_TILE_BASE +
            SOURCE_SPECIAL_OPTION_BANANA_TILES <=
            SOURCE_OPTION_PROJECTILE_TILE_BASE &&
        SOURCE_SPECIAL_OPTION_XEGA_TILE_BASE +
            SOURCE_SPECIAL_OPTION_XEGA_TILES <=
            SOURCE_OPTION_PROJECTILE_TILE_BASE,
    "Special OPTION_SHAPES atlas overlaps non-shared OBJ VRAM"
);
#endif
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
        SOURCE_ENEMY_COMPACT_TILES_PER_SLOT <=
        OBJ_TILE_BOSS_BAR,
    "primary compact enemy cache overlaps the boss bar"
);
_Static_assert(
    (
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT - 1u
    ) * SOURCE_ENEMY_COMPACT_TILES_PER_SLOT <=
        OBJ_GAME_OVER_TILE_COUNT,
    "transient compact enemy cache exceeds the status-label bank"
);
_Static_assert(
    SOURCE_EFFECT_CACHE_TILE_BASE +
        SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT *
            SOURCE_EFFECT_TILES_PER_SLOT <=
#if TYRIAN_GBA_STRESS_LOADOUT
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE,
#else
        SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE,
#endif
    "explosion and enemy caches overlap"
);
#if !TYRIAN_GBA_STRESS_LOADOUT
_Static_assert(
    SOURCE_EFFECT_CACHE_OVERFLOW_TILE_BASE == OBJ_STATIC_TILE_COUNT &&
        SOURCE_EFFECT_TILES_PER_SLOT == 4 &&
        (SOURCE_EFFECT_CACHE_OVERFLOW_TILE_BASE & 3u) == 0 &&
        SOURCE_EFFECT_CACHE_OVERFLOW_TILE_BASE +
                SOURCE_EFFECT_TILES_PER_SLOT <=
            SOURCE_ENEMY_CACHE_UPPER_TILE_BASE,
    "effect overflow must fit the aligned OBJ tile 636..639 gap"
);
_Static_assert(
    SOURCE_EFFECT_CACHE_BOSS_SLOT ==
            SOURCE_EFFECT_CACHE_PRIMARY_SLOT_COUNT + 1u &&
        SOURCE_EFFECT_CACHE_BOSS_TILE_BASE == OBJ_TILE_BOSS_BAR,
    "boss overflow effect slot must time-share the boss-bar OBJ tiles"
);
_Static_assert(
    SOURCE_EFFECT_CACHE_REWARD_SLOT + 1u ==
            SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT &&
        SOURCE_EFFECT_CACHE_REWARD_TILE_BASE +
                SOURCE_EFFECT_TILES_PER_SLOT ==
            OBJ_TILE_SCORE_DIGITS,
    "reward-shared effect slot must reclaim only the last legacy reward frame"
);
_Static_assert(
    SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT +
            SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT ==
        SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        SOURCE_EFFECT_CACHE_STATUS_TILE_BASE ==
            OBJ_TILE_GAME_OVER_RUNTIME + 8u &&
        SOURCE_EFFECT_CACHE_STATUS_TILE_BASE +
                SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT *
                    SOURCE_EFFECT_TILES_PER_SLOT ==
            OBJ_TILE_REWARD,
    "status-shared effect slots must occupy only OBJ tiles 520..527"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE +
        SOURCE_ENEMY_CACHE_SPLIT_HALF_TILES ==
        SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE,
    "split Sprite2 top half must border the expanded projectile cache"
);
_Static_assert(
    SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE +
            SOURCE_PROJECTILE_CACHE_EXTRA_SLOT_COUNT *
                SOURCE_PROJECTILE_TILES_PER_SLOT ==
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE,
    "expanded projectile cache must exactly fill the released OBJ gap"
);
_Static_assert(
    SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE +
        SOURCE_ENEMY_CACHE_SPLIT_HALF_TILES <=
        SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE,
    "split Sprite2 bottom half overlaps the live projectile cache"
);
_Static_assert(
    (SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE & 1) == 0 &&
        (SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE & 1) == 0,
    "split 8bpp Sprite2 halves must use even character indices"
);
#endif
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
    SOURCE_PLAYER_MIN_Y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y -
        (SOURCE_PRESENTATION_Y_ORIGIN - SOURCE_VIEW_CROP_Y) - 16 +
        SOURCE_PLAYER_CONTAINER_Y + SOURCE_PLAYER_ALPHA_TOP >= 0,
    "source player top bound must remain visible at the upper camera edge"
);
_Static_assert(
    SOURCE_PLAYER_MAX_Y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y -
        (SOURCE_PRESENTATION_Y_ORIGIN + SOURCE_VIEW_CROP_Y) - 16 +
        SOURCE_PLAYER_CONTAINER_Y +
        SOURCE_PLAYER_ALPHA_BOTTOM_EXCLUSIVE - 1 < SCREEN_HEIGHT,
    "source player bottom bound must remain visible at the lower camera edge"
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
    STATE_DATA_CUBES = 15,
    STATE_DATA_CUBE_READER = 16,
    STATE_SHIP_SPECS = 17,
    STATE_OPTIONS_MENU = 18,
    STATE_SAVE_SLOTS = 19,
    STATE_SAVE_NAME = 20,
    STATE_EPISODE_SCENE = 21,
};

enum {
    FRONTEND_SCENE_RETURN_GAME_MENU = 0,
    FRONTEND_SCENE_RETURN_LEVEL = 1,
};

enum {
    FRONTEND_TRANSITION_JOB_NONE = 0,
    FRONTEND_TRANSITION_JOB_NEXT_LEVEL,
    FRONTEND_TRANSITION_JOB_UPGRADE_SUBMENU,
    FRONTEND_TRANSITION_JOB_GAME_MENU,
    FRONTEND_TRANSITION_JOB_UPGRADE_MENU,
    FRONTEND_TRANSITION_JOB_QUIT,
    FRONTEND_TRANSITION_JOB_LEVEL_ENTRY,
    FRONTEND_TRANSITION_JOB_DATA_CUBES,
    FRONTEND_TRANSITION_JOB_DATA_READER,
    FRONTEND_TRANSITION_JOB_SHIP_SPECS,
};

enum {
    FRONTEND_GAME_MENU_JOB_INITIALIZE_CAMPAIGN = 1u << 0,
    FRONTEND_GAME_MENU_JOB_PREPARE_MAP = 1u << 1,
    FRONTEND_GAME_MENU_JOB_LOAD_SONG = 1u << 2,
};

enum {
    FRONTEND_QUIT_JOB_ENTER = 1u << 0,
};

enum {
    FRONTEND_DATA_JOB_RELOAD = 1u << 0,
    FRONTEND_DATA_JOB_FULL_FRAME = 1u << 1,
    FRONTEND_DATA_JOB_SELECTION = 1u << 2,
};

enum {
    FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT = 16,
    FRONTEND_SHIP_SPECS_SCALE_STEPS = 24,
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

/*
 * JE_loadMap()'s ]s / ]b "LAST LEVEL" checkpoint.  The PC stores this in
 * its reserved backup slot before entering ENGAGE/GALAGA bonus games.  The
 * GBA front end keeps the equivalent campaign snapshot separate from the
 * eleven user-visible save slots so a scripted survival game can restore
 * the exact pre-game route, equipment, cash and cube state on final death.
 */
typedef struct {
    FrontendPlayerItems items;
    u32 cash;
    u16 main_section;
    u8 play_mode;
    u8 episode;
    u8 difficulty;
    u8 armor;
    u8 shield;
    u8 shield_max;
    u8 cube_count;
    u8 cube_list[OT_EPISODE_CUBE_CAPACITY];
    u8 secret_hint;
} FrontendScriptCheckpoint;

#define BOX_OVERLAPS(ax, ay, aw, ah, bx, by, bw, bh) \
    ((ax) + (aw) > (bx) && (bx) + (bw) > (ax) && \
     (ay) + (ah) > (by) && (by) + (bh) > (ay))

extern const u8 obj_tiles[];
extern const u8 obj_palette[];
extern const u8 secret_level_palettes[];
extern const u8 insert_coin_palette[];
extern const u8 background_gba_palette[];
extern const u8 background_palette_nearest_asset[];
extern const u8 background_palette_mask_bank[];
extern const u8 frontend_frames[];
extern const u8 frontend_palettes[];
extern const u8 frontend_glyphs[];
extern const u8 frontend_stats_tiles[];
extern const u8 frontend_stats_widths[];
extern const u8 frontend_native_font[];
extern const u8 frontend_pregame_font[];
extern const u8 frontend_static_menu_panels[];
extern const u8 frontend_static_pre_game_frames[];
extern const u8 frontend_static_save_name_overlay[];
extern const u8 frontend_static_quit_overlay[];
extern const u8 frontend_static_quit_choices[];
extern const u8 frontend_static_quit_shade[];
extern const u8 frontend_static_help_strips[];
extern const u8 frontend_nav_obj_tiles[];
extern const u8 frontend_nav_obj_meta[];
extern const u8 frontend_nav_obj_palette[];
extern const u8 frontend_nav_bitmap_blocks[];
extern const u16 frontend_nav_bitmap_indices[];
extern const u8 frontend_source_stamp_offsets[];
extern const u8 frontend_source_stamp_data[];
extern const u8 jukebox_font_tiles[];
extern const u8 jukebox_backdrop_tiles[];
extern const u8 jukebox_backdrop_map[];
extern const u8 jukebox_bg_palette[];
extern const u8 jukebox_obj_tiles[];
extern const u8 jukebox_obj_palette[];
extern const u8 jukebox_titles[];
extern const u8 jukebox_reciprocal[];
extern const u8 jukebox_sine[];
extern const u8 tyrend_gba_frames[];
extern const u8 tyrend_gba_frames_end[];
extern const u8 tyrend_gba_palette[];
extern const u8 tyrend_gba_palette_end[];
extern const u8 soundbank[];

_Static_assert(
    TYREND_GBA_FRAME_WIDTH == SCREEN_WIDTH &&
        TYREND_GBA_FRAME_HEIGHT == SCREEN_HEIGHT &&
        TYREND_GBA_FRAME_BYTES == FRONTEND_FRAME_BYTES,
    "predecoded ending frames must match the GBA Mode-4 surface"
);
_Static_assert(
    TYREND_GBA_DATA_BYTES ==
        TYREND_GBA_FRAME_COUNT * TYREND_GBA_FRAME_BYTES &&
        TYREND_GBA_PALETTE_BYTES ==
            OT_PALETTE_COLOUR_COUNT * sizeof(u16),
    "predecoded ending animation metadata is inconsistent"
);

typedef struct {
    u8 active;
    u8 ttl;
    u8 damage;
    u8 infinite;
    u8 animation;
    u8 animation_max;
    u8 trail;
    u8 iced;
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

/*
 * Literal JE_doSP()/JE_drawSP() state, narrowed to the ranges authored by
 * Tyrian.  These particles are rendered through BG3 rather than consuming
 * the 128-entry OBJ pool, but retain the PC 101-slot overwrite ring.
 */
typedef struct {
    s16 x;
    s16 y;
    s8 delta_x;
    s8 delta_y;
    u8 color;
    u8 z;
} SourceSuperpixel;

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    u8 frame;
    u8 sequence;
} Effect;

#ifdef AUTOTEST_REWARD_VISUAL_TEST
typedef struct {
    u8 active;
    s16 x;
    s16 y;
    u8 frame;
    u8 sequence;
    u8 phase;
    u16 value;
} Reward;
#endif

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
#define PLAYER_SHOT_FREE_MASK_WORDS ((MAX_PLAYER_SHOTS + 31u) / 32u)
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
static u32 player_shot_active_mask[PLAYER_SHOT_FREE_MASK_WORDS] EWRAM_DATA;
static u8 active_player_shot_count;
#endif
static SourceSuperpixel source_superpixels[MAX_SUPERPIXELS] EWRAM_DATA;
static u8 source_last_superpixel;
static u8 source_active_superpixels;
static Effect effects[MAX_EFFECTS] EWRAM_DATA;
#define EFFECT_ACTIVE_MASK_WORDS ((MAX_EFFECTS + 31u) / 32u)
#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
static u32 effect_active_mask[EFFECT_ACTIVE_MASK_WORDS] EWRAM_DATA;
#endif
#ifdef AUTOTEST_REWARD_VISUAL_TEST
static Reward rewards[MAX_REWARDS] EWRAM_DATA;
#endif
static PickupExplosion pickup_explosions[MAX_PICKUP_EXPLOSIONS] EWRAM_DATA;
#define PICKUP_EXPLOSION_ACTIVE_MASK_WORDS \
    ((MAX_PICKUP_EXPLOSIONS + 31u) / 32u)
#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
static u32 pickup_explosion_active_mask[
    PICKUP_EXPLOSION_ACTIVE_MASK_WORDS
] EWRAM_DATA;
#endif
static u8 active_effect_count;
static u8 effect_slot_high_water;
#ifdef AUTOTEST_REWARD_VISUAL_TEST
static u8 active_reward_count;
#endif
static u8 active_pickup_explosion_count;
static OBJATTR oam_shadow[HARDWARE_OAM_ENTRIES] EWRAM_DATA;

#ifdef AUTOTEST_REWARD_VISUAL_TEST
static const u16 reward_value_table[REWARD_SEQUENCE_COUNT + 1] = {
    0, 25, 50, 75, 100, 250,
};
static const u8 reward_frame_delay_table[REWARD_SEQUENCE_COUNT] = {
    2, 2, 2, 5, 5,
};
#endif

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
enum {
    FRONTEND_DEMO_KEY_UP = 1u << 0,
    FRONTEND_DEMO_KEY_DOWN = 1u << 1,
    FRONTEND_DEMO_KEY_LEFT = 1u << 2,
    FRONTEND_DEMO_KEY_RIGHT = 1u << 3,
    FRONTEND_DEMO_KEY_MAIN_FIRE = 1u << 4,
    FRONTEND_DEMO_KEY_REAR_MODE = 1u << 5,
    FRONTEND_DEMO_KEY_LEFT_SIDEKICK = 1u << 6,
    FRONTEND_DEMO_KEY_RIGHT_SIDEKICK = 1u << 7,
};
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
static u8 frontend_demo_keys_previous EWRAM_BSS;
static u8 frontend_demo_keys_pressed EWRAM_BSS;
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
static OtEpisodeRouteState frontend_route_state EWRAM_BSS;
static OtEpisodeSceneReader frontend_scene_reader EWRAM_BSS;
static OtEpisodeSceneAction frontend_scene_action EWRAM_BSS;
static u16 frontend_scene_palette[OT_PALETTE_COLOUR_COUNT] EWRAM_BSS;
static u16 frontend_scene_animation_frame EWRAM_BSS;
static u16 frontend_scene_target_section EWRAM_BSS;
static u8 frontend_scene_return_mode EWRAM_BSS;
static u8 frontend_scene_waiting_input EWRAM_BSS;
static u8 frontend_scene_finish_pending EWRAM_BSS;
static u8 frontend_scene_end_episode EWRAM_BSS;
static u8 frontend_scene_animation_active EWRAM_BSS;
static u8 frontend_scene_episode_announcement EWRAM_BSS;
static u8 frontend_scene_animation_wait EWRAM_BSS;
static u8 frontend_scene_warning_delay EWRAM_BSS;
static u8 frontend_scene_warning_bar_delay EWRAM_BSS;
static u8 frontend_scene_warning_colour EWRAM_BSS;
static s8 frontend_scene_warning_direction EWRAM_BSS;
static u8 frontend_scene_secret_hint EWRAM_BSS;
static u8 frontend_secret_hint EWRAM_BSS;
static OtFrontendText frontend_text EWRAM_BSS;
static u8 frontend_text_ready;
static u8 frontend_map_ready;
static u8 frontend_level_ready;
static FrontendPlayerItems frontend_player_items EWRAM_BSS;
static u8 frontend_script_game_active EWRAM_BSS;
/* OpenTyrian game_menu.c old_items[0]: entry loadout, then the accepted
 * preview after the first confirm while the cursor advances to Done. */
static FrontendPlayerItems frontend_upgrade_accepted_items EWRAM_BSS;
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
static u32 frontend_upgrade_trade_cash EWRAM_BSS;
static u8 frontend_quit_yes EWRAM_BSS;
static u8 frontend_quit_dialog_cache_valid EWRAM_BSS;
static u8 frontend_quit_dialog_cache_selection EWRAM_BSS;
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
static u8 frontend_title_brand_pending EWRAM_BSS;
static u8 frontend_pending_kind EWRAM_BSS;
static u8 frontend_palette_pending EWRAM_BSS;
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
static u8 frontend_transition_job EWRAM_BSS;
static u8 frontend_transition_phase EWRAM_BSS;
static u8 frontend_transition_flags EWRAM_BSS;
static u8 frontend_transition_failed EWRAM_BSS;
static u8 frontend_transition_work_index EWRAM_BSS;
static u8 frontend_transition_work_mode EWRAM_BSS;
static u16 frontend_level_entry_section EWRAM_BSS;
#define FRONTEND_SHIP_PANEL_CACHE_WIDTH 120u
#define FRONTEND_SHIP_PANEL_CACHE_HEIGHT SCREEN_HEIGHT
#define FRONTEND_SHIP_PANEL_CACHE_BYTES \
    ( \
        FRONTEND_SHIP_PANEL_CACHE_WIDTH * \
        FRONTEND_SHIP_PANEL_CACHE_HEIGHT \
    )
/*
 * Next Level and Upgrade submenus overwrite the shared Mode-4 frame.  Keep
 * the configuration-dependent left ship panel in a compact packed cache so
 * returning to Game Menu never replays source art.  This costs 19.2 KiB of
 * the measured EWRAM margin and remains inactive during gameplay.
 */
static u8 frontend_ship_panel_cache[
    FRONTEND_SHIP_PANEL_CACHE_BYTES
] EWRAM_BSS __attribute__((aligned(4)));
static FrontendPlayerItems
    frontend_ship_panel_cache_items EWRAM_BSS;
static u32 frontend_ship_panel_cache_cash EWRAM_BSS;
static u8 frontend_ship_panel_cache_armor EWRAM_BSS;
static u8 frontend_ship_panel_cache_valid EWRAM_BSS;
static u8 frontend_data_cube_count EWRAM_BSS;
static u8 frontend_data_cube_list[
    OT_EPISODE_CUBE_CAPACITY
] EWRAM_BSS;
static u8 frontend_level_cube_start_count EWRAM_BSS;
static u8 frontend_data_page_count EWRAM_BSS;
static u8 frontend_data_cache_valid EWRAM_BSS;
static u8 frontend_data_reader_index EWRAM_BSS;
static u16 frontend_data_scroll_pixel EWRAM_BSS;
static u16 frontend_data_scroll_target EWRAM_BSS;
static s8 frontend_data_glow EWRAM_BSS;
static s8 frontend_data_glow_delta EWRAM_BSS;
static u8 frontend_data_glow_wait EWRAM_BSS;
static OtShipDefinition frontend_ship_specs_ship EWRAM_BSS;
static u8 frontend_ship_specs_scale_active EWRAM_BSS;
static u8 frontend_ship_specs_scale_step EWRAM_BSS;
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
#define FRONTEND_COLD_PAGE_CACHE_OFFSET \
    ( \
        FRONTEND_FRAME_BYTES + \
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) \
    )
/*
 * Datacube pages and the quit dialog are mutually exclusive.  Reuse the
 * shared arena tail for the four decrypted stock cube records and their
 * temporary face palette rather than reserving another ~14 KiB of EWRAM.
 * Ship Specs aliases the first cube slot because it likewise cannot be open
 * at the same time as Data.
 */
#define frontend_data_cube_cache \
    ((OtDataCube *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET \
    ))
#define frontend_data_face_palette \
    ((u16 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET + \
        sizeof(OtDataCube) * OT_EPISODE_CUBE_CAPACITY \
    ))
#define frontend_ship_info_cache \
    ((OtShipInfo *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET \
    ))
#define FRONTEND_SHIP_SPRITE_WIDTH 96u
#define FRONTEND_SHIP_SPRITE_HEIGHT 112u
#define FRONTEND_SHIP_SPRITE_BYTES \
    (FRONTEND_SHIP_SPRITE_WIDTH * FRONTEND_SHIP_SPRITE_HEIGHT)
#define frontend_ship_sprite_scratch \
    ((u8 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET + \
        sizeof(OtShipInfo) \
    ))
#define FRONTEND_QUIT_CHOICE_CACHE_X 32u
#define FRONTEND_QUIT_CHOICE_CACHE_Y \
    (TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y - 4u)
#define FRONTEND_QUIT_CHOICE_CACHE_WIDTH 160u
#define FRONTEND_QUIT_CHOICE_CACHE_HEIGHT 14u
#define FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES \
    ( \
        FRONTEND_QUIT_CHOICE_CACHE_WIDTH * \
        FRONTEND_QUIT_CHOICE_CACHE_HEIGHT \
    )
#define frontend_quit_choice_background \
    ((u8 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_FRAME_BYTES + \
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) \
    ))
#define FRONTEND_QUIT_DIALOG_CACHE_X 0u
#define FRONTEND_QUIT_DIALOG_CACHE_Y 36u
#define FRONTEND_QUIT_DIALOG_CACHE_WIDTH SCREEN_WIDTH
#define FRONTEND_QUIT_DIALOG_CACHE_HEIGHT 92u
#define FRONTEND_QUIT_DIALOG_CACHE_BYTES \
    ( \
        FRONTEND_QUIT_DIALOG_CACHE_WIDTH * \
        FRONTEND_QUIT_DIALOG_CACHE_HEIGHT \
    )
#define frontend_quit_dialog_background \
    ((u8 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_FRAME_BYTES + \
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) + \
        FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES \
    ))
_Static_assert(
    FRONTEND_FRAME_BYTES +
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) <=
        sizeof(FrontendGameplayArena),
    "frontend Sprite2 decode canvas must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_COLD_PAGE_CACHE_OFFSET +
        sizeof(OtDataCube) * OT_EPISODE_CUBE_CAPACITY +
        OT_PALETTE_COLOUR_COUNT * sizeof(u16) <=
        sizeof(FrontendGameplayArena),
    "datacube records and face palette must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_COLD_PAGE_CACHE_OFFSET + sizeof(OtShipInfo) <=
        sizeof(FrontendGameplayArena),
    "Ship Specs text must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_COLD_PAGE_CACHE_OFFSET +
        sizeof(OtShipInfo) +
        FRONTEND_SHIP_SPRITE_BYTES <=
        sizeof(FrontendGameplayArena),
    "Ship Specs source sprite must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_FRAME_BYTES +
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) +
        FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES <=
        sizeof(FrontendGameplayArena),
    "frontend quit-choice cache must fit the shared arena tail"
);
_Static_assert(
    FRONTEND_FRAME_BYTES +
        OT_SPRITE2_FRAME_PIXELS * sizeof(u16) +
        FRONTEND_QUIT_CHOICE_BACKGROUND_BYTES +
        FRONTEND_QUIT_DIALOG_CACHE_BYTES <=
        sizeof(FrontendGameplayArena),
    "frontend quit-dialog cache must fit the shared arena tail"
);
/* Authoritative OpenTyrian gameplay coordinates, never GBA screen pixels. */
static s16 player_source_x;
static s16 player_source_y;
static s16 source_camera_offset_x_q8;
static s16 source_camera_offset_y_q8;
static s8 source_camera_offset_x;
static s8 source_camera_offset_y;
/* OpenTyrian Player.x_velocity/y_velocity are int, then clamp to +/-4. */
static s32 player_source_velocity_x;
static s32 player_source_velocity_y;
static u8 player_source_x_friction_ticks;
static u8 player_source_y_friction_ticks;
static u16 player_invulnerable;
static u8 player_alive;
static u8 player_exploding_ticks;
static u8 player_death_fx_wait;
static u8 player_death_music_volume;
static u8 player_death_music_fade_active;
static u8 audio_level_music_fade_in_active;
static u8 audio_level_music_fade_in_step;
static u8 player_armor;
static u8 player_initial_armor;
static u8 player_shield;
static u8 player_shield_max;
static u8 player_lives;
static s8 player_end_warp;
static s8 player_bank;
static u16 player_ship_graphic;
static u8 player_ship_animation;
static u8 player_generator_power;
static u16 player_weapon_energy;
static u16 player_shield_recharge_cost;
static u8 player_shield_wait;
static u32 player_cash;
/* Shared by the production and full-loadout HUD renderers. */
static u8 source_special_ready_indicator;

#if !TYRIAN_GBA_STRESS_LOADOUT
enum {
    SOURCE_WEAPON_BAY_FRONT = 0,
    SOURCE_WEAPON_BAY_REAR = 1,
    SOURCE_WEAPON_BAY_LEFT_SIDEKICK = 2,
    SOURCE_WEAPON_BAY_RIGHT_SIDEKICK = 3,
    SOURCE_WEAPON_BAY_MISC = 4,
    SOURCE_WEAPON_BAY_P2_CHARGE = 5,
    SOURCE_WEAPON_BAY_P1_SUPERBOMB = 6,
    SOURCE_WEAPON_BAY_P2_SUPERBOMB = 7,
    SOURCE_WEAPON_BAY_SPECIAL = 8,
    SOURCE_WEAPON_BAY_NORTSPARKS = 9,
    SOURCE_WEAPON_BAY_SPECIAL2 = 10,
    SOURCE_WEAPON_BAY_COUNT = 11,
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
    u16 poweruse;
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
static u16 source_front_weapon_poweruse;
static u8 source_rear_weapon_valid;
static u8 source_rear_weapon_bound;
static u8 source_rear_weapon_port_id;
static u8 source_rear_weapon_power;
static u8 source_rear_weapon_mode;
static u16 source_rear_weapon_hdt_id;
static u16 source_rear_weapon_poweruse;
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
static u8 source_special_bound_id;
static u8 source_special_valid;
static u8 source_special_power;
static u8 source_special_type;
static u16 source_special_definition_weapon;
static u8 source_special_wait;
static u8 source_special_next_wait;
static u8 source_special_fire_held;
static u8 source_special_flare_start;
static u8 source_special_spray;
static u8 source_special_link_to_player;
static u8 source_special_weapon_frequency;
static u8 source_special_zinglon_duration;
static u8 source_special_zinglon_visual_half_width;
static u8 source_special_zinglon_visual_active;
static u8 source_special_zinglon_damage_pending;
static u16 source_special_flare_duration;
static u16 source_special_weapon_id;
static s8 source_special_weapon_filter;
static u8 source_last_spawned_shot_index;
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
/* Stress builds do not activate Astral, but share the BG3/star adapter. */
static u8 source_player_invulnerable_visual_active;
static u8 source_special_astral_duration;
static u8 source_special_astral_visual_active;
static u8 source_option_projectile_valid;
#if !TYRIAN_GBA_STRESS_LOADOUT
static u16 source_special_option_projectile_graphic;
#endif

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
static u8 source_background2_enabled;
static u8 source_background2_upload_pending;
/*
 * Source detail presentation state.  The PC writes its final 264x184
 * framebuffer after all gameplay blits; Mode 0 needs explicit hardware
 * adapters for the same profile gates.
 */
static u8 source_detail_vertical_flip;
static u8 source_detail_spotlight_requested;
static u8 source_detail_iced_requested;
static u8 source_detail_blur_requested;
static u8 source_detail_wave_requested;
static u8 source_detail_shadow_windows_active;
static u8 source_detail_wild_active;
/*
 * Pentium normally maps the PC wild BG2 pass to the GBA colour-effects
 * unit.  This second flag is deliberately separate: it records the rare
 * frames where another full-screen effect owns that unit and the BG1 tile
 * cache must temporarily use the checkerboard compatibility fallback.
 */
static u8 source_detail_wild_dither_active;
static u8 source_detail_palette_mode_active;
static u8 source_detail_palette_mode_pending;
static u8 source_detail_palette_dirty;
static u8 source_detail_palette_indices_valid;
static u8 source_detail_obj_palette_base_valid;
static u8 source_detail_effect_kind_pending;
static u8 source_detail_effect_hue_pending;
static s8 source_detail_effect_brightness_pending;
static u8 source_detail_effect_objects_pending;
static u8 source_detail_bg_source_index[256] EWRAM_BSS;
static u8 source_detail_obj_source_index[256] EWRAM_BSS;
static u16 source_detail_effect_bg_palette[256]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 source_detail_effect_obj_palette[256]
    EWRAM_BSS __attribute__((aligned(4)));
/* ARM palette builder: filtered source index -> final GBA 15-bit colour. */
static u16 source_detail_effect_colour_lut[256]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 source_detail_obj_palette_base[256]
    EWRAM_BSS __attribute__((aligned(4)));
/*
 * Double-buffered WIN0H scanline tables.  DMA0 streams one halfword at each
 * HBlank, avoiding 160 CPU IRQ entries per display frame.  Rendering always
 * writes the inactive table, so a dropped/pending scene cannot tear the
 * spotlight that the LCD is currently consuming.
 */
#define SOURCE_DETAIL_SPOTLIGHT_SCANLINES (SCREEN_HEIGHT + 1u)
static u16 source_detail_spotlight_table[2]
    [SOURCE_DETAIL_SPOTLIGHT_SCANLINES]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 source_detail_spotlight_table_active;
static u8 source_detail_spotlight_table_pending;
/*
 * Pentium/High lava and water are source framebuffer filters.  Mode 0 cannot
 * rewrite a complete 240x160 framebuffer, so DMA0 streams the closest native
 * text-BG equivalent: per-scanline BG0/BG1 scroll pairs.  The inactive table
 * is prepared while the LCD consumes the active one, exactly like WIN0H.
 */
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
#define SOURCE_DETAIL_WAVE_SCANLINES (SCREEN_HEIGHT + 1u)
static u16 source_detail_wave_table[2]
    [SOURCE_DETAIL_WAVE_SCANLINES][4]
    EWRAM_BSS __attribute__((aligned(4)));
/*
 * Mode 0 can only apply one horizontal offset to an entire scanline, while
 * OpenTyrian's lava/water filter varies its sample offset every eight pixels.
 * Keep a spatially low-pass, zero-DC profile for each source filter so that
 * the hardware approximation does not turn those local samples into harsh
 * full-width bands.  These values depend only on the fixed 240x160 crop and
 * OpenTyrian's filter formula, so store the verified result in Game Pak ROM
 * instead of spending EWRAM and boot-time software division on regeneration.
 */
static const s8 source_detail_wave_profile[2]
    [SOURCE_DETAIL_WAVE_SCANLINES] = {
    {
        -3, -3, -3, -3, -2, -2, -1,  0,  0,  1,  2,  2,  3,  3,  3,  3,
         3,  2,  1,  1,  0,  0, -1, -2, -2, -3, -3, -3, -3, -3, -2, -1,
        -1,  0,  1,  1,  2,  2,  3,  3,  3,  3,  2,  2,  1,  1,  0, -1,
        -1, -2, -3, -3, -3, -3, -3, -2, -2, -1,  0,  0,  1,  1,  2,  3,
         3,  3,  3,  3,  2,  2,  1,  0,  0, -1, -2, -2, -3, -3, -3, -3,
        -3, -2, -1, -1,  0,  0,  1,  2,  2,  3,  3,  3,  3,  3,  2,  1,
         1,  0, -1, -1, -2, -2, -3, -3, -3, -3, -2, -2, -1, -1,  0,  1,
         1,  2,  3,  3,  3,  3,  3,  2,  2,  1,  0,  0, -1, -1, -2, -3,
        -3, -3, -3, -3, -2, -2, -1,  0,  0,  1,  2,  2,  3,  3,  3,  3,
         3,  2,  1,  1,  0,  0, -1, -2, -2, -3, -3, -3, -3, -3, -2, -2,
        -1,
    },
    {
        -1, -2, -2, -2, -1, -1, -1,  0,  0,  0,  1,  1,  1,  1,  2,  2,
         2,  1,  1,  1,  0,  0,  0, -1, -1, -1, -2, -2, -2, -2, -1, -1,
        -1,  0,  0,  0,  1,  1,  1,  2,  2,  2,  1,  1,  1,  1,  0,  0,
         0, -1, -1, -1, -2, -2, -2, -1, -1, -1,  0,  0,  0,  0,  1,  1,
         1,  2,  2,  2,  1,  1,  1,  0,  0,  0, -1, -1, -1, -1, -2, -2,
        -2, -1, -1, -1,  0,  0,  0,  1,  1,  1,  2,  2,  2,  2,  1,  1,
         1,  0,  0,  0, -1, -1, -1, -2, -2, -2, -1, -1, -1, -1,  0,  0,
         0,  1,  1,  1,  2,  2,  2,  1,  1,  1,  0,  0,  0,  0, -1, -1,
        -1, -2, -2, -2, -1, -1, -1,  0,  0,  0,  1,  1,  1,  1,  2,  2,
         2,  1,  1,  1,  0,  0,  0, -1, -1, -1, -2, -2, -2, -2, -1, -1,
        -1,
    },
};
/* Sustained presentation pressure attenuates only the visual wave adapter. */
static u16 source_detail_wave_strength_q8 EWRAM_BSS;
static u8 source_detail_wave_pressure_score EWRAM_BSS;
static u8 source_detail_wave_pressure_active EWRAM_BSS;
#endif
static u8 source_detail_wave_table_active;
static u8 source_detail_wave_table_pending;
/* Transparent hardware BG3: source starfield and in-level text/warnings. */
static u8 gameplay_overlay_tiles[
    GAMEPLAY_OVERLAY_TILE_BYTES
] EWRAM_BSS __attribute__((aligned(4)));
static u16 gameplay_overlay_map[
    GAMEPLAY_OVERLAY_MAP_ENTRIES
] EWRAM_BSS;
static u8 gameplay_overlay_star_nibble[16];
static u8 gameplay_overlay_warning_colour[16];
static u8 gameplay_overlay_superpixel_bank[16];
static u8 gameplay_overlay_superpixel_nibble[16][16];
static u8 gameplay_overlay_star_bank;
static u8 gameplay_overlay_event_bank;
static u8 gameplay_overlay_event_nibble[2];
static u8 gameplay_overlay_tile_count;
static u8 gameplay_overlay_active;
static u8 gameplay_overlay_upload_pending;
static u8 gameplay_overlay_message_index;
static u8 gameplay_overlay_message_ticks;
static u8 gameplay_overlay_pickup_message_type;
static u8 gameplay_overlay_pickup_message_item_id;

/*
 * A source-authored parallax event may expose more than one 8-pixel map row
 * in a single logic tick (UNDERDELI reaches 22 pixels).  Keep every row that
 * must become visible at the next VBlank instead of letting a scalar pending
 * slot silently turn the 32-row hardware ring into stale data.
 */
typedef struct {
    u8 count;
    u16 row[BACKGROUND_PENDING_ROW_CAPACITY];
    const u8 *source[BACKGROUND_PENDING_ROW_CAPACITY];
    u16 *target[BACKGROUND_PENDING_ROW_CAPACITY];
} BackgroundPendingRows;

static BackgroundPendingRows
    background_pending_rows[3] EWRAM_BSS;

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
typedef struct {
    u16 hofs[3];
    u16 vofs[3];
    u16 bg0cnt;
    u16 bg1cnt;
    u16 bg2cnt;
    u16 dispcnt;
    u16 bldcnt;
    u16 bldalpha;
    u16 bldy;
    u16 mosaic;
    u16 win0h;
    u16 win0v;
    u16 winin;
    u16 winout;
    u16 full_scroll_pixel[3];
    u8 spotlight_active;
    u8 spotlight_table_index;
    u8 wave_active;
    u8 wave_table_index;
    u8 effect_palette_active;
} GameplayPresentationRegisters;

static GameplayPresentationRegisters gameplay_presentation;
static u32 presentation_render_estimate;
static u32 presentation_render_ewma;
static u32 presentation_render_deviation;
static u8 presentation_render_pending;
static u8 presentation_pending_logic_ticks;
static u8 presentation_registers_valid;
static u8 presentation_release_held_window;
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
static u8 presentation_adaptive_dispatch_active;
static u8 presentation_adaptive_dispatch_severe;
static u8 presentation_adaptive_dispatch_pressure;
static u8 presentation_adaptive_dispatch_recovery_ticks;
static u8 presentation_adaptive_dispatch_idle_render;
static u32 presentation_adaptive_dispatch_missed_baseline;
#endif
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
volatile u32 telemetry_sprite2_filter_admission_denials EWRAM_BSS;
volatile u32 telemetry_sprite2_filter_fallback_hits EWRAM_BSS;
volatile u32 telemetry_sprite2_filter_fallback_builds EWRAM_BSS;
volatile u32 telemetry_sprite2_filter_evictions EWRAM_BSS;
volatile u32 telemetry_sprite2_exact_lookup_probes EWRAM_BSS;
volatile u32 telemetry_sprite2_exact_lookup_hits EWRAM_BSS;
volatile u32 telemetry_sprite2_uploads;
volatile u32 telemetry_sprite2_upload_bytes;
volatile u32 telemetry_sprite2_compact_uploads;
volatile u32 telemetry_sprite2_split_uploads;
volatile u32 telemetry_sprite2_upload_coalesces EWRAM_BSS;
volatile u32 telemetry_sprite2_max_uploads;
volatile u32 telemetry_sprite2_max_visible_unique;
volatile u32 telemetry_sprite2_generation_mismatches EWRAM_BSS;
volatile u32 telemetry_sprite2_l2_pin_exhaustions EWRAM_BSS;
volatile u32 telemetry_sprite2_pending_cancellations EWRAM_BSS;
volatile u32 telemetry_sprite2_scratch_deferrals EWRAM_BSS;
volatile u32 telemetry_projectile_cache_hits;
volatile u32 telemetry_projectile_cache_misses;
volatile u32 telemetry_projectile_cache_evictions;
volatile u32 telemetry_projectile_cache_drops;
volatile u32 telemetry_projectile_cache_uploads;
volatile u32 telemetry_projectile_cache_upload_coalesces EWRAM_BSS;
volatile u32 telemetry_projectile_cache_max_uploads;
volatile u32 telemetry_projectile_cache_max_visible_unique;
volatile u32 telemetry_projectile_hint_probes EWRAM_BSS;
volatile u32 telemetry_projectile_hint_hits EWRAM_BSS;
volatile u32 telemetry_projectile_hint_fallback_scans EWRAM_BSS;
volatile u32 telemetry_projectile_visible_capacity_drops;
volatile u32 telemetry_structural_oam_required_max;
volatile u32 telemetry_effect_oam_culls;
volatile u32 telemetry_enemy_shot_oam_culls;
volatile u32 telemetry_player_shot_oam_culls;
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
#endif
volatile u32 telemetry_detail_lava_frames;
volatile u32 telemetry_detail_water_frames;
volatile u32 telemetry_detail_iced_frames;
volatile u32 telemetry_detail_blur_frames;
volatile u32 telemetry_detail_wild_frames;
volatile u32 telemetry_detail_filter_hue_frames;
volatile u32 telemetry_detail_palette_rebuilds;
volatile u32 telemetry_detail_wave_frames;
volatile u32 telemetry_detail_wild_dither_frames;
volatile u32 telemetry_detail_wave_attenuated_frames EWRAM_BSS;
volatile u32 telemetry_detail_wave_pressure_score_max EWRAM_BSS;
volatile u32 telemetry_detail_wave_strength_min_q8 EWRAM_BSS;
volatile u32 telemetry_effect_cache_hits;
volatile u32 telemetry_effect_cache_misses;
volatile u32 telemetry_effect_cache_evictions;
volatile u32 telemetry_effect_cache_drops;
volatile u32 telemetry_effect_cache_uploads;
volatile u32 telemetry_effect_cache_upload_bytes;
volatile u32 telemetry_effect_cache_upload_coalesces EWRAM_BSS;
volatile u32 telemetry_effect_cache_max_uploads;
volatile u32 telemetry_effect_cache_max_visible_unique;
volatile u32 telemetry_state_transitions;
volatile u32 telemetry_frontend_full_redraws;
volatile u32 telemetry_frontend_dirty_commits;
volatile u32 telemetry_frontend_dirty_bytes;
volatile u32 telemetry_frontend_runtime_shp_decodes;
volatile u32 telemetry_frontend_runtime_sprite2_decodes;
volatile u32 telemetry_frontend_transition_job_cycles_max;
volatile u32 telemetry_frontend_transition_phase_cycles_max[
    FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT
];
volatile u32 telemetry_missed_vblanks_play;
volatile u32 telemetry_missed_vblanks_frontend;
volatile u32 telemetry_missed_vblanks_game_over;
volatile u32 telemetry_missed_vblanks_stats;
volatile u32 telemetry_missed_vblanks_transition;
volatile u32 telemetry_missed_vblanks_frontend_other;
volatile u32 telemetry_missed_vblanks_episode_scene;
volatile u32 telemetry_missed_vblanks_scene_picture;
volatile u32 telemetry_missed_vblanks_scene_picture_decode;
volatile u32 telemetry_missed_vblanks_scene_picture_scale;
volatile u32 telemetry_missed_vblanks_scene_picture_present;
volatile u32 telemetry_missed_vblanks_scene_text;
volatile u32 telemetry_missed_vblanks_scene_animation;
volatile u32 telemetry_scene_animation_decode_frames;
volatile u32 telemetry_scene_animation_decode_cycles_total;
volatile u32 telemetry_scene_animation_decode_cycles_max;
volatile u32 telemetry_missed_vblank_transition_job_last;
volatile u32 telemetry_missed_vblank_transition_phase_next;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
enum {
    ROUTE_MISSED_VBLANK_POSITION_BUCKET_SHIFT = 9,
    ROUTE_MISSED_VBLANK_POSITION_BUCKET_COUNT = 16,
};
volatile u32 telemetry_route_first_missed_vblank_position;
volatile u32 telemetry_route_last_missed_vblank_position;
volatile u32 telemetry_route_missed_vblank_position_bucket[
    ROUTE_MISSED_VBLANK_POSITION_BUCKET_COUNT
];
#ifdef AUTOTEST_ROUTE_PERF
enum {
    ROUTE_PERF_MISSED_RECORD_COUNT = 16,
    ROUTE_PERF_LOGIC_STAGE_PLAYER_ENERGY = 0,
    ROUTE_PERF_LOGIC_STAGE_LEVEL_PORT = 1,
    ROUTE_PERF_LOGIC_STAGE_SOURCE_SYNC = 2,
    ROUTE_PERF_LOGIC_STAGE_BACKGROUND = 3,
    ROUTE_PERF_LOGIC_STAGE_PLAYER_SHOTS = 4,
    ROUTE_PERF_LOGIC_STAGE_EFFECTS_REWARDS = 5,
    ROUTE_PERF_LOGIC_STAGE_PLAYER_COLLISION = 6,
    ROUTE_PERF_LOGIC_STAGE_PLAYER_UPDATE = 7,
    ROUTE_PERF_LOGIC_STAGE_ENEMY_SHOTS_TAIL = 8,
    ROUTE_PERF_LOGIC_STAGE_COUNT = 9,
};

typedef struct {
    u32 level_position;
    u32 missed_count;
    u32 loop_cycles;
    u32 prelogic_cycles;
    u32 logic_cycles;
    u32 render_cycles;
    u32 prefetch_cycles;
    u32 background_tile_renders;
    u32 sprite2_misses;
    u32 projectile_misses;
    u32 effect_misses;
    u32 oam_count;
    u32 active_enemies;
    u32 active_enemy_shots;
    u32 logic_updated;
    u32 rendered;
} RoutePerfMissedRecord;

static RoutePerfMissedRecord route_perf_missed_record[
    ROUTE_PERF_MISSED_RECORD_COUNT
] EWRAM_BSS;
static u8 route_perf_missed_record_count;
static u32 route_perf_previous_loop_cycles;
static u32 route_perf_previous_prelogic_cycles;
static u32 route_perf_previous_logic_cycles;
static u32 route_perf_previous_render_cycles;
static u32 route_perf_previous_prefetch_cycles;
static u32 route_perf_previous_background_tile_renders;
static u32 route_perf_previous_sprite2_misses;
static u32 route_perf_previous_projectile_misses;
static u32 route_perf_previous_effect_misses;
static u32 route_perf_previous_oam_count;
static u32 route_perf_previous_active_enemies;
static u32 route_perf_previous_active_enemy_shots;
static u32 route_perf_previous_logic_updated;
static u32 route_perf_previous_rendered;
static u32 route_perf_current_logic_stage[
    ROUTE_PERF_LOGIC_STAGE_COUNT
];
static u32 route_perf_previous_logic_stage[
    ROUTE_PERF_LOGIC_STAGE_COUNT
];
static u32 route_perf_missed_logic_stage[
    ROUTE_PERF_MISSED_RECORD_COUNT
][ROUTE_PERF_LOGIC_STAGE_COUNT] EWRAM_BSS;
#endif
#endif
volatile u32 telemetry_frontend_nav_bitmap_redraws;
volatile u32 telemetry_frontend_nav_obj_updates;
volatile u32 telemetry_frontend_nav_obj_uploads;
volatile u32 telemetry_frontend_nav_obj_upload_bytes;
volatile u32 telemetry_frontend_nav_obj_overflows;
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
/*
 * Emulator and flash-cart save-type scanners look for this exact marker in
 * the ROM.  It belongs in release builds as well as telemetry builds.
 */
static const char save_type_marker[] __attribute__((used)) = "SRAM_V121";

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
volatile u32 telemetry_level_music_transition_starts;
volatile u32 telemetry_level_music_fade_out_steps;
volatile u32 telemetry_level_music_silent_vblanks;
volatile u32 telemetry_level_music_fade_in_steps;
volatile u32 telemetry_camera_min_origin_x;
volatile u32 telemetry_camera_max_origin_x;
volatile u32 telemetry_camera_min_origin_y;
volatile u32 telemetry_camera_max_origin_y;
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
volatile u32 telemetry_adaptive_dispatch_attempts;
volatile u32 telemetry_adaptive_dispatch_entries;
volatile u32 telemetry_adaptive_dispatch_severe_entries;
volatile u32 telemetry_adaptive_dispatch_exits;
volatile u32 telemetry_adaptive_dispatch_logic_busy_deferred;
volatile u32 telemetry_adaptive_dispatch_idle_renders;
volatile u32 telemetry_adaptive_dispatch_safety_forced;
volatile u32 telemetry_wave_dispatch_scope_attempts;
volatile u32 telemetry_wave_dispatch_entries;
volatile u32 telemetry_wave_dispatch_exits;
volatile u32 telemetry_wave_dispatch_logic_busy_deferred;
volatile u32 telemetry_wave_dispatch_idle_renders;
volatile u32 telemetry_wave_dispatch_safety_forced;
volatile u32 telemetry_logic_catchup_updates;
volatile u32 telemetry_logic_updates_per_loop_max;
volatile u32 telemetry_logic_backlog_frames_max;
volatile u32 telemetry_background_held_rows_max;
volatile u32 telemetry_vblank_recovery_loops;
volatile u32 telemetry_audio_frames;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
volatile u32 telemetry_player_shot_allocator_calls;
volatile u32 telemetry_player_shot_allocator_slot_probes;
volatile u32 telemetry_player_shot_allocator_mask_word_probes;
volatile u32 telemetry_player_shot_update_cycles_total;
volatile u32 telemetry_player_shot_update_cycles_max;
volatile u32 telemetry_player_shot_update_active_visits;
volatile u32 telemetry_player_shot_update_linear_visits;
volatile u32 telemetry_player_shot_update_complicated_visits;
volatile u32 telemetry_player_shot_update_trail_visits;
volatile u32 telemetry_player_shot_update_aimed_visits;
volatile u32 telemetry_player_shot_update_superpixel_visits;
#endif

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
volatile u32 telemetry_perf_prefetch_cycles_total;
volatile u32 telemetry_perf_prefetch_cycles_max;
volatile u32 telemetry_perf_loop_work_cycles_total;
volatile u32 telemetry_perf_loop_work_cycles_max;
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
volatile u32 telemetry_frontend_quit_capture_cycles_max;
volatile u32 telemetry_frontend_quit_shade_cycles_max;
volatile u32 telemetry_frontend_quit_overlay_cycles_max;
volatile u32 telemetry_frontend_quit_choices_cycles_max;
#endif
#endif

/* Cold boss-interval snapshots buy back 24 bytes of user stack in IWRAM. */
static u32 boss_perf_start_display_frames EWRAM_BSS;
static u32 boss_perf_start_missed_vblanks EWRAM_BSS;
static u32 boss_perf_start_sprite2_misses EWRAM_BSS;
static u32 boss_perf_start_sprite2_evictions EWRAM_BSS;
static u32 boss_perf_start_sprite2_upload_bytes EWRAM_BSS;
static u32 boss_perf_start_projectile_misses EWRAM_BSS;
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
/*
 * Route/golden tests historically exercise authored level timing with a
 * permanently firing power-11 cannon.  Keep that test-only load generator
 * independent from the newly restored stock generator/power-bar economy.
 * Focused energy tests temporarily clear this flag and therefore still
 * validate the exact OpenTyrian consumption and recharge rules.
 */
#ifndef AUTOTEST_UNLIMITED_WEAPON_ENERGY
#define AUTOTEST_UNLIMITED_WEAPON_ENERGY 1
#endif
#if AUTOTEST_UNLIMITED_WEAPON_ENERGY != 0 && \
    AUTOTEST_UNLIMITED_WEAPON_ENERGY != 1
#error AUTOTEST_UNLIMITED_WEAPON_ENERGY must be 0 or 1
#endif
static u8 autotest_unlimited_weapon_energy =
    AUTOTEST_UNLIMITED_WEAPON_ENERGY;
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
static u8 autotest_frontend_transition_ready;
#endif
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
static s16 source_camera_origin_x(void);
static s16 source_camera_origin_y(void);
static void source_camera_reset(void);
static void source_camera_update(void);
static u16 background_presentation_scroll_pixel(
    u8 layer,
    s8 camera_offset_y
);
static u8 background_schedule_presentation_window(
    s8 camera_offset_y
);
static u16 source_background_vofs(u8 layer);
static s16 source_world_to_screen_x(s16 source_x);
static s16 source_world_to_screen_y(s16 source_y);
static u16 source_background_hofs(u8 layer);
static void source_runtime_reset(void);
static void gameplay_overlay_reset(void);
static void gameplay_overlay_prepare_palette(void);
static void gameplay_overlay_logic_tick(void);
static void gameplay_overlay_show_event(u8 event_index);
static void gameplay_overlay_show_initial_message(void);
static void gameplay_overlay_show_pickup(u8 message_type, u8 item_id);
static void gameplay_overlay_prepare_frame(void);
static void source_player_cache_commit(void);
static void source_enemy_cache_commit(void);
static void source_projectile_cache_commit(void);
static void source_static_atlas_upload_commit(void);
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
static void frontend_campaign_apply_level_script(
    const OtEpisodeLevel *level
);
static void frontend_campaign_sync_from_level(void);
static u8 frontend_episode_scene_begin(
    u16 section,
    u8 return_mode,
    u16 target_section
);
static void frontend_episode_announcement_begin(void);
static void frontend_episode_scene_update(void);
static void frontend_script_checkpoint_apply_map(
    const OtEpisodeMap *map
);
static void frontend_script_checkpoint_clear(void);
static u8 frontend_script_checkpoint_read(
    FrontendScriptCheckpoint *checkpoint
);
static u8 frontend_script_checkpoint_restore(void);
static void jukebox_commit_vblank(void);
static void jukebox_enter(void);
static void jukebox_update(void);
static void jukebox_render(void);
#ifdef AUTOTEST
static u32 source_sprite2_pending_upload_count(void);
#endif
static void source_effect_restore_shared_tiles_if_needed(void);
#include "src/background_runtime.inc"
#include "src/layer_runtime.inc"
#include "src/gba_platform.inc"
#include "src/effect_pool_mask.inc"
#include "src/player_shot_pool.inc"
#include "src/level_setup.inc"
#include "src/frontend_runtime.inc"
#include "src/gameplay_overlay.inc"
#include "src/entity_runtime.inc"
#include "src/combat_runtime.inc"
#include "src/source_runtime.inc"
#include "src/level_update.inc"
#include "src/gba_oam.inc"
#include "src/jukebox_runtime.inc"
#include "src/gba_hud.inc"
#include "src/gba_scene.inc"
#include "src/autotest.inc"
#include "src/main_loop.inc"
