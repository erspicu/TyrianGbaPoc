#include <gba.h>
#include <maxmod.h>

#include "res/asset_meta.h"
#include "res/soundbank.h"
#include "src/opentyrian_level_port.h"

#if defined(AUTOTEST_SCREENSHOT_TICK) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) || \
    defined(AUTOTEST_SCREENSHOT_REWARD) || \
    defined(AUTOTEST_SCREENSHOT_PAUSE)
#define AUTOTEST_SCREENSHOT_ENABLED
#endif

#define BG0_SCREEN_BLOCK 24
#define BG1_SCREEN_BLOCK 26
#define BG2_SCREEN_BLOCK 28
#define MAP_RING_ROWS 64
#define MAP_ROW_BYTES 64

/*
 * GBA has 128 hardware OBJ entries and substantially more CPU time than the
 * NES/SNES low-detail proofs.  These pools intentionally raise the first
 * level's concurrency while staying under a conservative scanline budget.
 */
#define MAX_ENEMIES 48
#define MAX_PLAYER_SHOTS 12
#define MAX_ENEMY_SHOTS 60
#define MAX_EFFECTS 48
#define MAX_REWARDS 32
#define ENEMY_ARCHETYPES 24
#define HARDWARE_OAM_ENTRIES 128
#define SPRITE_LIMIT HARDWARE_OAM_ENTRIES
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

#define PC_SHOT_GRAPHIC_DART 58
#define PC_SHOT_GRAPHIC_RED 112
#define PC_SHOT_GRAPHIC_LASER_LEFT 145
#define PC_SHOT_GRAPHIC_LASER_DOWN 146
#define PC_SHOT_GRAPHIC_LASER_RIGHT 147
#define PC_SHOT_GRAPHIC_DART_LEFT 201
#define PC_SHOT_GRAPHIC_DART_RIGHT 202

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
    OBJ_PROJECTILE_SOURCE_COUNT == 8,
    "enemy projectile source count must match the PC level-1 set"
);
_Static_assert(
    OBJ_PROJECTILE_TILE_COUNT == 18,
    "enemy projectile shape packing must remain within OBJ VRAM"
);
_Static_assert(
    OBJ_TILE_PROJECTILE_113 == OBJ_TILE_PROJECTILE_112 + 1,
    "PC red projectile animation frames must be adjacent"
);
_Static_assert(
    OBJ_PAL_PROJECTILE_112 == OBJ_PAL_PROJECTILE_113,
    "PC red projectile animation frames must share a palette"
);
_Static_assert(OBJ_TILE_COUNT <= 1024, "Mode 0 OBJ VRAM tile limit exceeded");

#define EVENT_MOVE 0x80
#define EVENT_ACCEL 0x81
#define EVENT_REVERSE 0x82
#define EVENT_FIRE 0x83
#define EVENT_FOREGROUND 0x84
#define EVENT_SCROLL 0x85
#define EVENT_REWARD 0x86
#define EVENT_END 0xFF

#define STATE_TITLE 0
#define STATE_PLAY 1
#define STATE_BOSS 2
#define STATE_CLEAR 3

#define BOX_OVERLAPS(ax, ay, aw, ah, bx, by, bw, bh) \
    ((ax) + (aw) > (bx) && (bx) + (bw) > (ax) && \
     (ay) + (ah) > (by) && (by) + (bh) > (ay))

extern const u8 title_bitmap[];
extern const u8 bg1_tiles[];
extern const u8 bg2_tiles[];
extern const u8 bg3_tiles[];
extern const u8 bg_palette[];
extern const u8 bg1_map[];
extern const u8 bg2_map[];
extern const u8 bg3_map[];
extern const u8 obj_tiles[];
extern const u8 obj_palette[];
extern const u8 level_events[];
extern const u8 soundbank[];

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    s8 dx;
    s8 dy;
    s8 fixed_dy;
    s8 accel_x;
    s8 accel_y;
    u8 pool;
    u8 type;
    u8 hp;
    u8 phase;
    u8 link;
    u8 reward;
    u16 kill_value;
    u8 turret[3];
    u8 frequency[3];
    u8 fire_wait[3];
    u8 multi_pos[3];
} Enemy;

typedef struct {
    u8 active;
    s16 x;
    s16 y;
} PlayerShot;

typedef struct {
    u8 active;
    s16 x;
    s16 y;
    s8 dx;
    s8 dy;
    u8 duration;
    u8 graphic;
    u8 animate;
    u8 animax;
} EnemyShot;

typedef struct {
    u8 graphic;
    s8 bx;
    s8 by;
    s8 sx;
    s8 sy;
} WeaponShot;

typedef struct {
    u8 id;
    u8 first;
    u8 multi;
    u8 maximum;
    u8 aim;
    u8 animax;
    u16 sound;
} WeaponDef;

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

static Enemy enemies[MAX_ENEMIES] EWRAM_DATA;
static PlayerShot player_shots[MAX_PLAYER_SHOTS] EWRAM_DATA;
static EnemyShot enemy_shots[MAX_ENEMY_SHOTS] EWRAM_DATA;
static Effect effects[MAX_EFFECTS] EWRAM_DATA;
static Reward rewards[MAX_REWARDS] EWRAM_DATA;
static u8 active_effect_count;
static u8 active_reward_count;
static OBJATTR oam_shadow[HARDWARE_OAM_ENTRIES] EWRAM_DATA;

static const u16 enemy_tiles[ENEMY_ARCHETYPES] = {
    OBJ_TILE_ENEMY_0, OBJ_TILE_ENEMY_1,
    OBJ_TILE_ENEMY_2, OBJ_TILE_ENEMY_3,
    OBJ_TILE_ENEMY_4, OBJ_TILE_ENEMY_5,
    OBJ_TILE_ENEMY_6, OBJ_TILE_ENEMY_7,
    OBJ_TILE_ENEMY_8, OBJ_TILE_ENEMY_9,
    OBJ_TILE_ENEMY_10, OBJ_TILE_ENEMY_11,
    OBJ_TILE_ENEMY_12, OBJ_TILE_ENEMY_13,
    OBJ_TILE_ENEMY_14, OBJ_TILE_ENEMY_15,
    OBJ_TILE_ENEMY_16, OBJ_TILE_ENEMY_17,
    OBJ_TILE_ENEMY_18, OBJ_TILE_ENEMY_19,
    OBJ_TILE_ENEMY_20, OBJ_TILE_ENEMY_21,
    OBJ_TILE_ENEMY_22, OBJ_TILE_ENEMY_23,
};

static const u8 enemy_palettes[ENEMY_ARCHETYPES] = {
    OBJ_PAL_ENEMY_0, OBJ_PAL_ENEMY_1,
    OBJ_PAL_ENEMY_2, OBJ_PAL_ENEMY_3,
    OBJ_PAL_ENEMY_4, OBJ_PAL_ENEMY_5,
    OBJ_PAL_ENEMY_6, OBJ_PAL_ENEMY_7,
    OBJ_PAL_ENEMY_8, OBJ_PAL_ENEMY_9,
    OBJ_PAL_ENEMY_10, OBJ_PAL_ENEMY_11,
    OBJ_PAL_ENEMY_12, OBJ_PAL_ENEMY_13,
    OBJ_PAL_ENEMY_14, OBJ_PAL_ENEMY_15,
    OBJ_PAL_ENEMY_16, OBJ_PAL_ENEMY_17,
    OBJ_PAL_ENEMY_18, OBJ_PAL_ENEMY_19,
    OBJ_PAL_ENEMY_20, OBJ_PAL_ENEMY_21,
    OBJ_PAL_ENEMY_22, OBJ_PAL_ENEMY_23,
};

/*
 * HDT esize/explosiontype values audited against the representative enemy
 * behind each visual archetype.  Archetype 1 is the only 1x1 source; every
 * other representative uses Tyrian's four-part large explosion.
 */
static const u8 enemy_large_explosion[ENEMY_ARCHETYPES] = {
    1, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

static const u8 enemy_ground_explosion[ENEMY_ARCHETYPES] = {
    1, 1, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 1, 0, 0,
};

/*
 * Exact tyrian.hdt WeaponType entries referenced by first-level enemies and
 * the 46..65 boss component grid. Field order is graphic, bx, by, sx, sy.
 * Slot rotation and aim are applied below exactly as JE_drawEnemy does. Every
 * used position has del=255; acceleration/accelerationx and tx/ty are zero.
 */
static const WeaponShot weapon_shots[] = {
    {PC_SHOT_GRAPHIC_LASER_DOWN,   0,  0,  0, 4}, /* W2 */
    {PC_SHOT_GRAPHIC_LASER_RIGHT,  0,  0, -3, 3}, /* W3 */
    {PC_SHOT_GRAPHIC_LASER_LEFT,   0,  0, -3, 3}, /* W4 */
    {PC_SHOT_GRAPHIC_RED,          0,  0,  0, 0}, /* W59 */
    {PC_SHOT_GRAPHIC_RED,          0,  0,  0, 0}, /* W62 */
    {PC_SHOT_GRAPHIC_DART,         0,  0,  0, 8}, /* W78 pos 1 */
    {PC_SHOT_GRAPHIC_DART_LEFT,    0,  0, -2, 7}, /* W78 pos 2 */
    {PC_SHOT_GRAPHIC_DART_RIGHT,   0,  0,  2, 7}, /* W78 pos 3 */
    {PC_SHOT_GRAPHIC_RED,         -8, -2,  0, 0}, /* W115 */
    {PC_SHOT_GRAPHIC_RED,          8, -2,  0, 0}, /* W116 */
    {PC_SHOT_GRAPHIC_DART,        -8,  0,  0, 6}, /* W125 */
    {PC_SHOT_GRAPHIC_DART,         8,  0,  0, 6}, /* W126 */
    {PC_SHOT_GRAPHIC_RED,          0,  0,  0, 3}, /* W127 pos 1 */
    {PC_SHOT_GRAPHIC_RED,          0,  0, -1, 2}, /* W127 pos 2 */
    {PC_SHOT_GRAPHIC_RED,          0,  0,  1, 2}, /* W127 pos 3 */
    {PC_SHOT_GRAPHIC_RED,          0,  0, -2, 1}, /* W127 pos 4 */
    {PC_SHOT_GRAPHIC_RED,          0,  0,  2, 1}, /* W127 pos 5 */
};

static const WeaponDef weapon_defs[] = {
    {  2,  0, 1, 1, 0, 0, SFX_ENEMY_SHOT_4},
    {  3,  1, 1, 1, 0, 0, SFX_ENEMY_SHOT_4},
    {  4,  2, 1, 1, 0, 0, SFX_ENEMY_SHOT_4},
    { 59,  3, 1, 1, 2, 2, SFX_ENEMY_SHOT_13},
    { 62,  4, 1, 1, 2, 2, SFX_ENEMY_SHOT_13},
    { 78,  5, 3, 3, 0, 0, SFX_ENEMY_SHOT_6},
    {115,  8, 1, 1, 3, 2, SFX_ENEMY_SHOT_13},
    {116,  9, 1, 1, 3, 2, SFX_ENEMY_SHOT_13},
    {125, 10, 1, 1, 0, 0, SFX_WEAPON_1},
    {126, 11, 1, 1, 0, 0, SFX_WEAPON_1},
    {127, 12, 5, 5, 0, 2, SFX_ENEMY_SHOT_6},
};

static const u16 reward_value_table[REWARD_SEQUENCE_COUNT + 1] = {
    0, 25, 50, 75, 100, 250,
};

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

static u8 game_state;
static u8 game_paused;
static u16 pad_now;
static u16 pad_pressed;
static s16 player_x;
static s16 player_y;
static u8 player_invulnerable;
static u8 fire_cooldown;
static s8 player_bank;
static u32 player_cash;

static s16 boss_x;
static s16 boss_y;
static s8 boss_dx;
static s8 boss_dy;
static u8 boss_hp;
static u8 boss_phase;
static u8 boss_bar_flash;
static u8 boss_bar_palette_dirty;
static u8 boss_aim_fire_wait;
static u8 boss_spread_fire_wait;
static u8 boss_aim_left_pos;
static u8 boss_aim_right_pos;
static u8 boss_spread_pos;
static u8 clear_timer;

static u16 level_tick;
static u16 level_position;
static u16 event_offset;
static u16 event_time;
static u8 foreground_phase;
static u32 logic_accumulator;
static OtLevelPortState source_parity_level;

static u16 bg1_scroll_pixel;
static u16 bg2_scroll_pixel;
static u16 bg3_scroll_pixel;
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
static const u8 *bg1_row_source;
static const u8 *bg2_row_source;
static const u8 *bg3_row_source;
static u16 *bg1_row_target;
static u16 *bg2_row_target;
static u16 *bg3_row_target;

static u8 oam_count;
static u8 previous_oam_count;
static u8 oam_dirty;
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
volatile u32 telemetry_state_transitions;

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
static const char save_type_marker[] __attribute__((used)) = "SRAM_V114";
static void autotest_finish(void);
#ifdef AUTOTEST_SCREENSHOT_ENABLED
static u8 autotest_screenshot_delay;
#endif
#endif

#include "src/gba_platform.inc"
#include "src/level_setup.inc"
#include "src/entity_runtime.inc"
#include "src/event_runtime.inc"
#include "src/combat_runtime.inc"
#include "src/level_update.inc"
#include "src/gba_oam.inc"
#include "src/gba_hud.inc"
#include "src/gba_scene.inc"
#include "src/autotest.inc"
int main(void)
{
    u32 current_vblank;

    irqInit();
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
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
    enter_title();

    for (;;) {
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
        if (game_state == STATE_TITLE && !autotest_running) {
            autotest_running = 1;
            pad_pressed = KEY_START;
        } else if (game_state != STATE_TITLE) {
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

        if (game_state == STATE_TITLE) {
            if (pad_pressed & KEY_START) {
                enter_level();
#ifdef AUTOTEST_BOOT_ONLY
                __asm__ volatile("swi 3");
#endif
            }
        } else {
            if (
                (game_state == STATE_PLAY || game_state == STATE_BOSS) &&
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
                logic_accumulator += ORIGINAL_LOGIC_NUMERATOR;
                if (logic_accumulator >= ORIGINAL_LOGIC_DENOMINATOR) {
                    logic_accumulator -= ORIGINAL_LOGIC_DENOMINATOR;
                    update_logic();
                    if (game_state != STATE_TITLE) {
                        render_game();
#ifdef AUTOTEST_SCREENSHOT_TICK
                        if (
                            !autotest_screenshot_delay &&
                            game_state != STATE_TITLE &&
                            level_tick >= AUTOTEST_SCREENSHOT_TICK
                        ) {
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
            telemetry_display_frames++;
        }
    }
}
