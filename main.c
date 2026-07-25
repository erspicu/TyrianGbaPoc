#include <gba.h>
#include <maxmod.h>

#include "res/asset_meta.h"
#include "res/soundbank.h"

#if defined(AUTOTEST_SCREENSHOT_TICK) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) || \
    defined(AUTOTEST_SCREENSHOT_REWARD)
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
#define MAX_ENEMIES 24
#define MAX_PLAYER_SHOTS 12
#define MAX_ENEMY_SHOTS 24
#define MAX_EFFECTS 48
#define MAX_REWARDS 16
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
#define REWARD_FRAME_COUNT 6
#define REWARD_TILES_PER_FRAME 4
#define REWARD_SEQUENCE_COUNT 3

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
_Static_assert(OBJ_TILE_COUNT <= 1024, "Mode 0 OBJ VRAM tile limit exceeded");

#define EVENT_MOVE 0x80
#define EVENT_ACCEL 0x81
#define EVENT_REVERSE 0x82
#define EVENT_FIRE 0x83
#define EVENT_FOREGROUND 0x84
#define EVENT_SCROLL 0x85
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
    s8 accel_x;
    s8 accel_y;
    u8 type;
    u8 hp;
    u8 phase;
    u8 link;
    u8 fire_period;
    u8 fire_timer;
    u8 reward;
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
    u8 dy;
} EnemyShot;

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

static const u8 enemy_hp_table[ENEMY_ARCHETYPES] = {
    2, 1, 3, 2, 4, 2, 2, 2,
    2, 3, 3, 3, 4, 4, 3, 3,
    4, 5, 5, 4, 5, 6, 6, 5,
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

static const s8 enemy_dx_table[ENEMY_ARCHETYPES] = {
     1, -1,  1,  1,  1,  0,  2, -2,
     2,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};

static const s8 enemy_dy_table[ENEMY_ARCHETYPES] = {
    2, 2, 3, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
};

static const u8 enemy_fire_table[ENEMY_ARCHETYPES] = {
     0,  0,  0,  0, 18, 18, 10, 10,
    18, 18,  0,  0, 30,  0,  0,  0,
     0,  0,  0, 24, 30,  0,  0, 24,
};

static const u16 reward_value_table[REWARD_SEQUENCE_COUNT + 1] = {
    0, 50, 100, 1000,
};

static u8 game_state;
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
static u8 boss_fire_timer;
static u8 clear_timer;

static u16 level_tick;
static u16 event_offset;
static u16 event_time;
static u8 foreground_phase;
static u32 logic_accumulator;

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
volatile u32 telemetry_state_transitions;

#ifdef AUTOTEST
static u8 autotest_running;
static const char save_type_marker[] __attribute__((used)) = "SRAM_V113";
static void autotest_finish(void);
#ifdef AUTOTEST_SCREENSHOT_ENABLED
static u8 autotest_screenshot_delay;
#endif
#endif

static void vblank_handler(void)
{
    telemetry_vblank_irqs++;
    mmVBlank();
}

static void hide_all_sprites(void)
{
    u16 index;
    for (index = 0; index < HARDWARE_OAM_ENTRIES; index++) {
        oam_shadow[index].attr0 = ATTR0_DISABLED;
        oam_shadow[index].attr1 = 0;
        oam_shadow[index].attr2 = 0;
        oam_shadow[index].dummy = 0;
    }
    previous_oam_count = 0;
    oam_dirty = 1;
}

static void commit_vblank_work(void)
{
    if (bg1_row_pending) {
        dmaCopy(bg1_row_source, bg1_row_target, MAP_ROW_BYTES);
        bg1_row_pending = 0;
    }
    if (bg2_row_pending) {
        dmaCopy(bg2_row_source, bg2_row_target, MAP_ROW_BYTES);
        bg2_row_pending = 0;
    }
    if (bg3_row_pending) {
        dmaCopy(bg3_row_source, bg3_row_target, MAP_ROW_BYTES);
        bg3_row_pending = 0;
    }
    if (game_state != STATE_TITLE) {
        REG_BG0HOFS = 8;
        REG_BG1HOFS = 8;
        REG_BG2HOFS = 8;
        REG_BG0VOFS = bg1_scroll_pixel & 511;
        REG_BG1VOFS = bg2_scroll_pixel & 511;
        REG_BG2VOFS = bg3_scroll_pixel & 511;
        REG_BG2CNT = BG_PRIORITY(foreground_phase ? 0 : 1) |
                     CHAR_BASE(2) | SCREEN_BASE(BG2_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_2;
    }
    if (oam_dirty) {
        dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
        oam_dirty = 0;
    }
}

static void audio_load_music(u16 module)
{
    mmStop();
    mmEffectCancelAll();
    mmStart(module, MM_PLAY_LOOP);
    mmSetModuleVolume(896);
    mmSetEffectsVolume(896);
}

static void audio_effect(u16 effect)
{
    mmEffect(effect);
}

static void clear_entities(void)
{
    u16 index;
    for (index = 0; index < MAX_ENEMIES; index++) enemies[index].active = 0;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shots[index].active = 0;
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shots[index].active = 0;
    }
    for (index = 0; index < MAX_EFFECTS; index++) effects[index].active = 0;
    for (index = 0; index < MAX_REWARDS; index++) rewards[index].active = 0;
    active_effect_count = 0;
    active_reward_count = 0;
}

static const u8 *map_row(const u8 *map, u16 row)
{
    return map + (u32)row * MAP_ROW_BYTES;
}

static void load_ring(const u8 *map, u16 rows, u16 top_row, u16 screen_block)
{
    u16 row;
    u16 first = top_row > 32 ? top_row - 32 : 0;
    u16 last = first + MAP_RING_ROWS;
    u16 *ring = (u16 *)SCREEN_BASE_BLOCK(screen_block);

    if (last > rows) {
        last = rows;
        first = last - MAP_RING_ROWS;
    }
    for (row = first; row < last; row++) {
        dmaCopy(map_row(map, row), ring + ((row & 63) * 32), MAP_ROW_BYTES);
    }
}

static void enter_title(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    dmaCopy(title_bitmap, MODE3_FB, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    REG_DISPCNT = MODE_3 | BG2_ON;
    audio_load_music(MOD_TYRIAN_TITLE_FULL);
    game_state = STATE_TITLE;
    telemetry_state_transitions++;
}

static void enter_level(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    clear_entities();
    hide_all_sprites();

    dmaCopy(bg1_tiles, CHAR_BASE_BLOCK(0), 512 * 32);
    dmaCopy(bg2_tiles, CHAR_BASE_BLOCK(1), 512 * 32);
    dmaCopy(bg3_tiles, CHAR_BASE_BLOCK(2), 512 * 32);
    dmaCopy(bg_palette, BG_PALETTE, 512);
    dmaCopy(obj_tiles, OBJ_BASE_ADR, OBJ_TILE_COUNT * 32);
    dmaCopy(obj_palette, SPRITE_PALETTE, 512);

    REG_BG0CNT = BG_PRIORITY(3) | CHAR_BASE(0) |
                 SCREEN_BASE(BG0_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG1CNT = BG_PRIORITY(2) | CHAR_BASE(1) |
                 SCREEN_BASE(BG1_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG2CNT = BG_PRIORITY(1) | CHAR_BASE(2) |
                 SCREEN_BASE(BG2_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;

    bg1_scroll_pixel = (BG1_ROWS - 20) * 8;
    bg2_scroll_pixel = (BG2_ROWS - 20) * 8;
    bg3_scroll_pixel = (BG3_ROWS - 20) * 8;
    load_ring(bg1_map, BG1_ROWS, bg1_scroll_pixel >> 3, BG0_SCREEN_BLOCK);
    load_ring(bg2_map, BG2_ROWS, bg2_scroll_pixel >> 3, BG1_SCREEN_BLOCK);
    load_ring(bg3_map, BG3_ROWS, bg3_scroll_pixel >> 3, BG2_SCREEN_BLOCK);
    /* First-level event 0 selects the original 1:2:0 opening motion. */
    bg1_scroll_speed = 1;
    bg2_scroll_speed = 2;
    bg3_scroll_speed = 0;
    bg1_scroll_delay = 1;
    bg2_scroll_delay = 1;
    bg1_scroll_delay_max = 1;
    bg2_scroll_delay_max = 1;

    player_x = 104;
    player_y = 124;
    player_invulnerable = 0;
    fire_cooldown = 0;
    player_bank = 0;
    player_cash = 0;
    level_tick = 0;
    event_offset = 0;
    event_time = level_events[0];
    foreground_phase = 0;
    logic_accumulator = 0;

    telemetry_display_frames = 0;
    telemetry_logic_updates = 0;
    telemetry_spawn_count = 0;
    telemetry_control_count = 0;
    telemetry_collision_count = 0;
    telemetry_map_rows = 0;
    telemetry_missed_vblanks = 0;
    telemetry_stream_drops = 0;
    telemetry_max_enemies = 0;
    telemetry_max_oam = 0;
    telemetry_max_effects = 0;
    telemetry_effect_drops = 0;
    telemetry_reward_spawns = 0;
    telemetry_reward_pickups = 0;
    telemetry_max_rewards = 0;
    telemetry_reward_drops = 0;
    last_vblank_seen = telemetry_vblank_irqs;

    REG_BG0HOFS = 8;
    REG_BG1HOFS = 8;
    REG_BG2HOFS = 8;
    REG_BG0VOFS = bg1_scroll_pixel & 511;
    REG_BG1VOFS = bg2_scroll_pixel & 511;
    REG_BG2VOFS = bg3_scroll_pixel & 511;
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    REG_DISPCNT = MODE_0 | BG0_ON | BG1_ON | BG2_ON |
                  OBJ_ON | OBJ_1D_MAP;
    audio_load_music(MOD_TYRIAN_LEVEL_FULL);
    game_state = STATE_PLAY;
    telemetry_state_transitions++;
}

static void spawn_effect(s16 x, s16 y, u8 sequence)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) {
            effects[index].active = 1;
            effects[index].x = x;
            effects[index].y = y;
            effects[index].frame = 0;
            effects[index].sequence = sequence;
            active_effect_count++;
            if (active_effect_count > telemetry_max_effects) {
                telemetry_max_effects = active_effect_count;
            }
            return;
        }
    }
    telemetry_effect_drops++;
}

static void spawn_large_effect(s16 x, s16 y, u8 ground)
{
    u8 sequence = ground ?
        EXPLOSION_SEQUENCE_GROUND_TOP_LEFT :
        EXPLOSION_SEQUENCE_AIR_TOP_LEFT;
    spawn_effect(x - 6, y - 14, sequence);
    spawn_effect(x + 6, y - 14, sequence + 1);
    spawn_effect(x - 6, y, sequence + 2);
    spawn_effect(x + 6, y, sequence + 3);
}

static void spawn_enemy_effect(const Enemy *enemy)
{
    u8 type = enemy->type < ENEMY_ARCHETYPES ? enemy->type : 0;
    if (enemy_large_explosion[type]) {
        spawn_large_effect(
            enemy->x + 16,
            enemy->y + 16,
            enemy_ground_explosion[type]
        );
    } else {
        spawn_effect(
            enemy->x + 8,
            enemy->y + 8,
            EXPLOSION_SEQUENCE_SMALL
        );
    }
}

static void spawn_reward(s16 x, s16 y, u8 code)
{
    u8 index;
    Reward *reward;
    if (code == 0 || code > REWARD_SEQUENCE_COUNT) return;
    for (index = 0; index < MAX_REWARDS; index++) {
        if (!rewards[index].active) {
            reward = &rewards[index];
            reward->active = 1;
            reward->x = x;
            reward->y = y;
            reward->frame = 0;
            reward->sequence = code - 1;
            reward->phase = 0;
            reward->value = reward_value_table[code];
            active_reward_count++;
            telemetry_reward_spawns++;
            if (active_reward_count > telemetry_max_rewards) {
                telemetry_max_rewards = active_reward_count;
            }
            return;
        }
    }
    telemetry_reward_drops++;
}

static void spawn_enemy_reward(const Enemy *enemy)
{
    spawn_reward(enemy->x + 8, enemy->y + 8, enemy->reward);
}

static s8 decode_component(u8 value)
{
    if (value == 15) return -99;
    return (s8)value - 7;
}

static u8 event_matches(const Enemy *enemy, u8 link)
{
    return link == 0 || link == 99 || enemy->link == link;
}

static u8 active_enemy_count(void)
{
    u8 index;
    u8 count = 0;
    for (index = 0; index < MAX_ENEMIES; index++) {
        if (enemies[index].active) count++;
    }
    return count;
}

static void spawn_enemy(u8 x, u8 type, u8 motion, u8 link, u8 reward)
{
    u8 index;
    u8 replace = 0;
    s16 largest_y = -32767;
    s8 velocity;
    Enemy *enemy;

    if (type >= ENEMY_ARCHETYPES) type = 0;
    for (index = 0; index < MAX_ENEMIES; index++) {
        if (!enemies[index].active) {
            replace = index;
            break;
        }
        if (enemies[index].y > largest_y) {
            largest_y = enemies[index].y;
            replace = index;
        }
    }

    enemy = &enemies[replace];
    enemy->active = 1;
    enemy->x = x > 208 ? 208 : x;
    enemy->y = (motion & 8) ? -30 : -8;
    enemy->type = type;
    enemy->hp = enemy_hp_table[type];
    enemy->phase = (u8)(x + level_tick);
    enemy->dx = enemy_dx_table[type];
    if ((enemy->x >= 120 && enemy->dx > 0) ||
        (enemy->x < 120 && enemy->dx < 0)) {
        enemy->dx = -enemy->dx;
    }
    velocity = enemy_dy_table[type] + (s8)((motion >> 4) - 7);
    if (velocity < -3) velocity = -3;
    if (velocity > 5) velocity = 5;
    enemy->dy = velocity;
    enemy->accel_x = 0;
    enemy->accel_y = 0;
    enemy->link = link;
    enemy->fire_period = enemy_fire_table[type];
    enemy->fire_timer = enemy->fire_period;
    enemy->reward = reward;
    telemetry_spawn_count++;

    index = active_enemy_count();
    if (index > telemetry_max_enemies) telemetry_max_enemies = index;
}

static void apply_control(u8 opcode, u8 link, u8 value)
{
    u8 index;
    s8 x = decode_component(value >> 4);
    s8 y = decode_component(value & 15);
    Enemy *enemy;

    for (index = 0; index < MAX_ENEMIES; index++) {
        enemy = &enemies[index];
        if (!enemy->active || !event_matches(enemy, link)) continue;
        if (opcode == EVENT_FIRE) {
            enemy->fire_period = value ? (value + 1) >> 1 : 0;
            enemy->fire_timer = 1;
        } else if (opcode == EVENT_MOVE) {
            if (x != -99) enemy->dx = x;
            if (y != -99) enemy->dy = y;
        } else if (opcode == EVENT_ACCEL) {
            if (x != -99) enemy->accel_x = x;
            if (y != -99) enemy->accel_y = y;
        } else {
            if (x != -99 && x != 0) {
                enemy->dx = -enemy->dx;
                enemy->accel_x = -enemy->accel_x;
            }
            if (y != -99 && y != 0) {
                enemy->dy = -enemy->dy;
                enemy->accel_y = -enemy->accel_y;
            }
        }
    }
    telemetry_control_count++;
}

static void set_background_motion(
    u8 speed1,
    u8 speed2,
    u8 speed3,
    u8 delay1,
    u8 delay2
)
{
    bg1_scroll_speed = speed1;
    bg2_scroll_speed = speed2;
    bg3_scroll_speed = speed3;
    bg1_scroll_delay_max = delay1 ? delay1 : 1;
    bg2_scroll_delay_max = delay2 ? delay2 : 1;
    bg1_scroll_delay = bg1_scroll_delay_max;
    bg2_scroll_delay = bg2_scroll_delay_max;
}

static void process_events(void)
{
    u8 opcode;
    u8 length;
    while (event_offset + 1u < LEVEL_EVENT_BYTES && event_time <= level_tick) {
        opcode = level_events[event_offset + 1];
        if (opcode == EVENT_END) return;
        length = 2;
        if (opcode < ENEMY_ARCHETYPES) {
            spawn_enemy(
                level_events[event_offset + 2],
                opcode,
                level_events[event_offset + 3],
                level_events[event_offset + 4],
                level_events[event_offset + 5]
            );
            length = 6;
        } else if (
            opcode == EVENT_MOVE ||
            opcode == EVENT_ACCEL ||
            opcode == EVENT_REVERSE ||
            opcode == EVENT_FIRE
        ) {
            apply_control(
                opcode,
                level_events[event_offset + 2],
                level_events[event_offset + 3]
            );
            length = 4;
        } else if (opcode == EVENT_FOREGROUND) {
            foreground_phase = 1;
            telemetry_control_count++;
        } else if (opcode == EVENT_SCROLL) {
            set_background_motion(
                level_events[event_offset + 2],
                level_events[event_offset + 3],
                level_events[event_offset + 4],
                level_events[event_offset + 5],
                level_events[event_offset + 6]
            );
            length = 7;
        }
        event_offset += length;
        if (event_offset + 1u >= LEVEL_EVENT_BYTES) return;
        event_time += level_events[event_offset];
    }
}

static void spawn_player_shot(void)
{
    u8 index;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (!player_shots[index].active) {
            player_shots[index].active = 1;
            player_shots[index].x = player_x + PLAYER_SHOT_X_OFFSET;
            player_shots[index].y = player_y + PLAYER_SHOT_Y_OFFSET;
            audio_effect(SFX_WEAPON_1);
            return;
        }
    }
}

static void spawn_enemy_shot(s16 x, s16 y, s8 dx, u8 dy)
{
    u8 index;
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        if (!enemy_shots[index].active) {
            enemy_shots[index].active = 1;
            enemy_shots[index].x = x;
            enemy_shots[index].y = y;
            enemy_shots[index].dx = dx;
            enemy_shots[index].dy = dy;
            return;
        }
    }
}

static void player_hit(void)
{
    if (player_invulnerable) return;
    player_invulnerable = 45;
    telemetry_collision_count++;
    audio_effect(SFX_ENEMY_HIT);
}

static void update_player(void)
{
    if ((pad_now & KEY_LEFT) && player_x > 0) player_x -= 3;
    if ((pad_now & KEY_RIGHT) && player_x < 208) player_x += 3;
    if ((pad_now & KEY_UP) && player_y > 8) player_y -= 3;
    if ((pad_now & KEY_DOWN) && player_y < 128) player_y += 3;

    if (fire_cooldown) fire_cooldown--;
    if ((pad_now & (KEY_A | KEY_B)) && !fire_cooldown) {
        spawn_player_shot();
        fire_cooldown = PLAYER_SHOT_COOLDOWN;
    }
    if (player_invulnerable) player_invulnerable--;

    /*
     * Tyrian ship graphics 233 and 235 are neutral and right-bank poses,
     * not alternating animation frames.  Toggling them every logic update
     * made the player's silhouette flash at about 34.78 Hz.
     */
    if ((pad_now & KEY_LEFT) && !(pad_now & KEY_RIGHT)) {
        player_bank = -1;
    } else if ((pad_now & KEY_RIGHT) && !(pad_now & KEY_LEFT)) {
        player_bank = 1;
    } else {
        player_bank = 0;
    }
}

static void update_shots(void)
{
    u8 index;
    PlayerShot *player_shot;
    EnemyShot *enemy_shot;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shot = &player_shots[index];
        if (!player_shot->active) continue;
        player_shot->y -= PLAYER_SHOT_SPEED;
        if (player_shot->y < -16) player_shot->active = 0;
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shot = &enemy_shots[index];
        if (!enemy_shot->active) continue;
        enemy_shot->x += enemy_shot->dx;
        enemy_shot->y += enemy_shot->dy;
        if (
            enemy_shot->x < -16 || enemy_shot->x > 240 ||
            enemy_shot->y > 168
        ) {
            enemy_shot->active = 0;
            continue;
        }
        if (BOX_OVERLAPS(
                enemy_shot->x + 4, enemy_shot->y + 4, 8, 8,
                player_x + 6, player_y + 5, 20, 23)) {
            enemy_shot->active = 0;
            player_hit();
        }
    }
}

static void update_rewards(void)
{
    u8 index;
    Reward *reward;
    for (index = 0; index < MAX_REWARDS; index++) {
        reward = &rewards[index];
        if (!reward->active) continue;
        reward->phase++;
        if ((reward->phase & 1) == 0) {
            reward->frame++;
            if (reward->frame >= REWARD_FRAME_COUNT) reward->frame = 0;
        }
        /* Original spriteSheet11 score items use ymove=1. */
        reward->y++;
        if (reward->y > 160) {
            reward->active = 0;
            if (active_reward_count) active_reward_count--;
            continue;
        }
        if (BOX_OVERLAPS(
                reward->x + 2, reward->y + 1, 12, 14,
                player_x + 6, player_y + 5, 20, 23)) {
            player_cash += reward->value;
            reward->active = 0;
            if (active_reward_count) active_reward_count--;
            telemetry_reward_pickups++;
            audio_effect(SFX_ITEM);
        }
    }
}

static void update_enemies(void)
{
    u8 index;
    Enemy *enemy;

    for (index = 0; index < MAX_ENEMIES; index++) {
        enemy = &enemies[index];
        if (!enemy->active) continue;
        enemy->phase++;
        if ((enemy->phase & 7) == 0) {
            if (enemy->accel_x > 0 && enemy->dx < 3) enemy->dx++;
            if (enemy->accel_x < 0 && enemy->dx > -3) enemy->dx--;
            if (enemy->accel_y > 0 && enemy->dy < 5) enemy->dy++;
            if (enemy->accel_y < 0 && enemy->dy > -3) enemy->dy--;
        }
        enemy->x += enemy->dx;
        enemy->y += enemy->dy;
        if (enemy->x < 0) {
            enemy->x = 0;
            enemy->dx = -enemy->dx;
        } else if (enemy->x > 208) {
            enemy->x = 208;
            enemy->dx = -enemy->dx;
        }
        if (enemy->y < -40 || enemy->y > 168) {
            enemy->active = 0;
            continue;
        }
        if (enemy->fire_period) {
            if (enemy->fire_timer) {
                enemy->fire_timer--;
            } else {
                spawn_enemy_shot(enemy->x + 8, enemy->y + 25, 0, 3);
                enemy->fire_timer = enemy->fire_period;
            }
        }
        if (BOX_OVERLAPS(
                enemy->x + 4, enemy->y + 4, 24, 24,
                player_x + 6, player_y + 5, 20, 23)) {
            enemy->active = 0;
            spawn_enemy_effect(enemy);
            player_hit();
        }
    }
}

static void collide_player_shots(void)
{
    u8 enemy_index;
    u8 shot_index;
    Enemy *enemy;
    PlayerShot *shot;

    for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
        shot = &player_shots[shot_index];
        if (!shot->active) continue;
        for (enemy_index = 0; enemy_index < MAX_ENEMIES; enemy_index++) {
            enemy = &enemies[enemy_index];
            if (!enemy->active) continue;
            if (BOX_OVERLAPS(
                    shot->x + PLAYER_SHOT_HIT_X,
                    shot->y + PLAYER_SHOT_HIT_Y,
                    PLAYER_SHOT_HIT_WIDTH,
                    PLAYER_SHOT_HIT_HEIGHT,
                    enemy->x + 3, enemy->y + 3, 26, 26)) {
                shot->active = 0;
                telemetry_collision_count++;
                audio_effect(SFX_ENEMY_HIT);
                if (enemy->hp) enemy->hp--;
                if (!enemy->hp) {
                    spawn_enemy_reward(enemy);
                    enemy->active = 0;
                    spawn_enemy_effect(enemy);
                    audio_effect(SFX_EXPLOSION_9);
                }
                break;
            }
        }
    }
}

static void update_effects(void)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) continue;
        effects[index].y += (
            bg2_scroll_speed ? bg2_scroll_speed : bg1_scroll_speed
        );
        if (effects[index].frame + 1 < EXPLOSION_FRAME_COUNT) {
            effects[index].frame++;
        } else {
            effects[index].active = 0;
            if (active_effect_count) active_effect_count--;
        }
    }
}

static void schedule_row(
    const u8 *map,
    u16 row,
    u16 screen_block,
    u8 *pending,
    const u8 **source,
    u16 **target
)
{
    if (*pending) {
        telemetry_stream_drops++;
        return;
    }
    *source = map_row(map, row);
    *target = (u16 *)SCREEN_BASE_BLOCK(screen_block) + ((row & 63) * 32);
    *pending = 1;
    telemetry_map_rows++;
}

static void advance_backgrounds(void)
{
    u16 old_row;
    u16 new_row;
    u8 move1 = 0;
    u8 move2 = 0;

    if (bg1_scroll_delay > 1) {
        bg1_scroll_delay--;
    } else {
        bg1_scroll_delay = bg1_scroll_delay_max;
        move1 = bg1_scroll_speed;
    }
    old_row = bg1_scroll_pixel >> 3;
    if (bg1_scroll_pixel >= move1) {
        bg1_scroll_pixel -= move1;
    } else {
        bg1_scroll_pixel = 0;
    }
    new_row = bg1_scroll_pixel >> 3;
    if (new_row != old_row) {
        schedule_row(
            bg1_map, new_row, BG0_SCREEN_BLOCK,
            &bg1_row_pending, &bg1_row_source, &bg1_row_target
        );
    }

    if (bg2_scroll_delay > 1) {
        bg2_scroll_delay--;
    } else {
        bg2_scroll_delay = bg2_scroll_delay_max;
        move2 = bg2_scroll_speed;
    }
    old_row = bg2_scroll_pixel >> 3;
    if (bg2_scroll_pixel >= move2) {
        bg2_scroll_pixel -= move2;
    } else {
        bg2_scroll_pixel = 0;
    }
    new_row = bg2_scroll_pixel >> 3;
    if (new_row != old_row) {
        schedule_row(
            bg2_map, new_row, BG1_SCREEN_BLOCK,
            &bg2_row_pending, &bg2_row_source, &bg2_row_target
        );
    }

    old_row = bg3_scroll_pixel >> 3;
    if (bg3_scroll_pixel >= bg3_scroll_speed) {
        bg3_scroll_pixel -= bg3_scroll_speed;
    } else {
        bg3_scroll_pixel = 0;
    }
    new_row = bg3_scroll_pixel >> 3;
    if (new_row != old_row) {
        schedule_row(
            bg3_map, new_row, BG2_SCREEN_BLOCK,
            &bg3_row_pending, &bg3_row_source, &bg3_row_target
        );
    }
}

static void enter_boss(void)
{
    u8 index;
    for (index = 0; index < MAX_ENEMIES; index++) enemies[index].active = 0;
    foreground_phase = 0;
    boss_x = 88;
    boss_y = 8;
    boss_dx = 1;
    boss_dy = 1;
    boss_hp = 96;
    boss_phase = 0;
    boss_fire_timer = 12;
    game_state = STATE_BOSS;
    telemetry_state_transitions++;
}

static void finish_boss(void)
{
    u8 index;
    game_state = STATE_CLEAR;
    clear_timer = 100;
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shots[index].active = 0;
    }
    spawn_large_effect(boss_x + 16, boss_y + 16, 0);
    spawn_large_effect(boss_x + 48, boss_y + 16, 0);
    spawn_large_effect(boss_x + 32, boss_y + 48, 0);
    audio_effect(SFX_EXPLOSION_9);
    telemetry_state_transitions++;
}

static void update_boss(void)
{
    u8 index;
    s8 aim;
    PlayerShot *shot;

    update_player();
    update_shots();
    update_effects();
    update_rewards();

    boss_phase++;
    boss_x += boss_dx;
    if (boss_x < 4 || boss_x > 172) {
        boss_dx = -boss_dx;
        boss_x += boss_dx;
    }
    if ((boss_phase & 3) == 0) {
        boss_y += boss_dy;
        if (boss_y < 4 || boss_y > 34) {
            boss_dy = -boss_dy;
            boss_y += boss_dy;
        }
    }
    if (boss_fire_timer) {
        boss_fire_timer--;
    } else {
        aim = player_x + 16 < boss_x + 32 ? -1 :
              (player_x > boss_x + 32 ? 1 : 0);
        spawn_enemy_shot(boss_x + 8, boss_y + 54, -1, 3);
        spawn_enemy_shot(boss_x + 28, boss_y + 58, aim, 4);
        spawn_enemy_shot(boss_x + 48, boss_y + 54, 1, 3);
        boss_fire_timer = boss_hp < 48 ? 8 : 14;
    }
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        shot = &player_shots[index];
        if (!shot->active) continue;
        if (BOX_OVERLAPS(
                shot->x + PLAYER_SHOT_HIT_X,
                shot->y + PLAYER_SHOT_HIT_Y,
                PLAYER_SHOT_HIT_WIDTH,
                PLAYER_SHOT_HIT_HEIGHT,
                boss_x + 4, boss_y + 4, 56, 56)) {
            shot->active = 0;
            if (boss_hp) boss_hp--;
            telemetry_collision_count++;
            audio_effect(SFX_ENEMY_HIT);
        }
    }
    if (pad_pressed & KEY_R) boss_hp = 0;
#ifdef AUTOTEST
    if (boss_phase > 240) boss_hp = 0;
#endif
    if (!boss_hp) finish_boss();
}

static void update_logic(void)
{
    level_tick++;
    telemetry_logic_updates++;
    advance_backgrounds();

    if (game_state == STATE_PLAY) {
        update_player();
        update_shots();
        /*
         * Advance existing explosions before collision handling so a newly
         * spawned effect renders source frame 122 for one complete tick.
         */
        update_effects();
#ifdef AUTOTEST_EXPLOSION_SEAM_TEST
        if (level_tick == 2) spawn_large_effect(120, 72, 0);
#endif
#ifdef AUTOTEST_REWARD_VISUAL_TEST
        if (level_tick == 2) spawn_reward(112, 70, 1);
#endif
        update_rewards();
        update_enemies();
        collide_player_shots();
        process_events();
        if ((pad_pressed & (KEY_SELECT | KEY_L)) ||
            level_tick >= LEVEL_BOSS_TICK) {
            enter_boss();
        }
    } else if (game_state == STATE_BOSS) {
        update_boss();
    } else if (game_state == STATE_CLEAR) {
        update_effects();
        update_rewards();
        if (clear_timer) {
            clear_timer--;
        } else {
            enter_title();
#ifdef AUTOTEST
            if (autotest_running) {
                /* The test shuts down only after the complete return path. */
                autotest_finish();
            }
#endif
        }
    }
}

static void put_sprite_with_attr1(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    u8 priority,
    u16 attr1_flags
)
{
    OBJATTR *object;
    s16 dimension = size == ATTR1_SIZE_64 ? 64 :
                    (size == ATTR1_SIZE_32 ? 32 :
                    (size == ATTR1_SIZE_16 ? 16 : 8));
    if (oam_count >= SPRITE_LIMIT) return;
    if (x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00FF) | ATTR0_NORMAL |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01FF) | size | attr1_flags;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(priority) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static void put_sprite(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    u8 priority
)
{
    put_sprite_with_attr1(x, y, tile, palette, size, priority, 0);
}

static void render_cash_counter(void)
{
    u8 index;
    u8 digit;
    u8 started = 0;
    u8 frame = (level_tick >> 1) % REWARD_FRAME_COUNT;
    u32 value = player_cash > 99999 ? 99999 : player_cash;
    u32 divisor = 10000;

    put_sprite(
        176, 0,
        OBJ_TILE_REWARD +
            (
                (REWARD_SEQUENCE_COUNT - 1) * REWARD_FRAME_COUNT + frame
            ) * REWARD_TILES_PER_FRAME,
        OBJ_PAL_REWARD, ATTR1_SIZE_16, 0
    );
    for (index = 0; index < 5; index++) {
        digit = (value / divisor) % 10;
        if (digit || started || index == 4) {
            put_sprite(
                192 + ((u16)index << 3), 4,
                OBJ_TILE_SCORE_DIGITS + digit,
                OBJ_PAL_SCORE_DIGITS, ATTR1_SIZE_8, 0
            );
            started = 1;
        }
        divisor /= 10;
    }
}

static void render_game(void)
{
    u8 index;
    u8 bars;
    u8 object_priority = foreground_phase ? 1 : 0;
    u8 old_count = previous_oam_count;
    u8 visible_count;
    oam_count = 0;

    put_sprite_with_attr1(
        player_x, player_y,
        player_bank ? OBJ_TILE_PLAYER_1 : OBJ_TILE_PLAYER_0,
        OBJ_PAL_PLAYER_0, ATTR1_SIZE_32, object_priority,
        player_bank < 0 ? OBJ_HFLIP : 0
    );
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) {
            put_sprite(
                player_shots[index].x, player_shots[index].y,
                OBJ_TILE_PLAYER_SHOT, OBJ_PAL_PLAYER_SHOT,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    if (game_state == STATE_PLAY) {
        for (index = 0; index < MAX_ENEMIES; index++) {
            if (enemies[index].active) {
                put_sprite(
                    enemies[index].x, enemies[index].y,
                    enemy_tiles[enemies[index].type],
                    enemy_palettes[enemies[index].type],
                    ATTR1_SIZE_32, object_priority
                );
            }
        }
    } else if (game_state == STATE_BOSS) {
        put_sprite(
            boss_x, boss_y, OBJ_TILE_BOSS_0, OBJ_PAL_BOSS_0,
            ATTR1_SIZE_64, object_priority
        );
        bars = (boss_hp + 11) / 12;
        for (index = 0; index < bars; index++) {
            put_sprite(
                4 + ((u16)index << 4), 2,
                OBJ_TILE_BOSS_BAR, OBJ_PAL_BOSS_BAR,
                ATTR1_SIZE_16, 0
            );
        }
    }
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) {
            put_sprite(
                rewards[index].x, rewards[index].y,
                OBJ_TILE_REWARD +
                    (
                        rewards[index].sequence * REWARD_FRAME_COUNT +
                        rewards[index].frame
                    ) * REWARD_TILES_PER_FRAME,
                OBJ_PAL_REWARD, ATTR1_SIZE_16, object_priority
            );
        }
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        if (enemy_shots[index].active) {
            put_sprite(
                enemy_shots[index].x, enemy_shots[index].y,
                OBJ_TILE_ENEMY_SHOT, OBJ_PAL_ENEMY_SHOT,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (effects[index].active) {
            put_sprite(
                effects[index].x, effects[index].y,
                OBJ_TILE_EXPLOSION +
                    (
                        effects[index].sequence * EXPLOSION_FRAME_COUNT +
                        effects[index].frame
                    ) * EXPLOSION_TILES_PER_FRAME,
                OBJ_PAL_EXPLOSION,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    render_cash_counter();
    visible_count = oam_count;
    while (oam_count < old_count) {
        oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
        oam_count++;
    }
    previous_oam_count = visible_count;
    if (visible_count > telemetry_max_oam) {
        telemetry_max_oam = visible_count;
    }
    oam_dirty = 1;
}

#ifdef AUTOTEST
static void sram_write_u32(u32 offset, u32 value)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    sram[offset] = (u8)value;
    sram[offset + 1] = (u8)(value >> 8);
    sram[offset + 2] = (u8)(value >> 16);
    sram[offset + 3] = (u8)(value >> 24);
}

static void autotest_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 pass = (
        game_state == STATE_TITLE &&
        telemetry_logic_updates >= LEVEL_BOSS_TICK &&
        telemetry_spawn_count == 414 &&
        telemetry_control_count == 347 &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_effect_drops == 0 &&
        telemetry_reward_spawns > 0 &&
        telemetry_reward_pickups > 0 &&
        telemetry_reward_drops == 0 &&
        bg1_scroll_speed == 2 &&
        bg2_scroll_speed == 4 &&
        bg3_scroll_speed == 6
    );
    volatile u32 delay;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'B';
    sram[3] = 'A';
    sram[4] = 3;
    sram[5] = pass;
    sram[6] = game_state;
    sram[7] = mmActive() ? 1 : 0;
    /*
     * Keep the standard save-type signature in the ROM so emulators select
     * SRAM before the first telemetry write.  The volatile read prevents
     * section garbage collection from discarding the otherwise metadata-only
     * string.
     */
    sram[64] = *(volatile const u8 *)save_type_marker;
    sram_write_u32(8, telemetry_logic_updates);
    sram_write_u32(12, telemetry_display_frames);
    sram_write_u32(16, telemetry_vblank_irqs);
    sram_write_u32(20, telemetry_missed_vblanks);
    sram_write_u32(24, telemetry_spawn_count);
    sram_write_u32(28, telemetry_control_count);
    sram_write_u32(32, telemetry_collision_count);
    sram_write_u32(36, telemetry_map_rows);
    sram_write_u32(40, telemetry_max_enemies);
    sram_write_u32(44, telemetry_max_oam);
    sram_write_u32(48, telemetry_stream_drops);
    sram_write_u32(52, event_offset);
    sram_write_u32(56, level_tick);
    sram_write_u32(60, telemetry_state_transitions);
    sram_write_u32(64, telemetry_max_effects);
    sram_write_u32(68, telemetry_effect_drops);
    sram_write_u32(72, telemetry_reward_spawns);
    sram_write_u32(76, telemetry_reward_pickups);
    sram_write_u32(80, telemetry_max_rewards);
    sram_write_u32(84, telemetry_reward_drops);
    sram_write_u32(88, player_cash);
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    if (pass) {
        __asm__ volatile("swi 3");
    }
    for (;;) {
        VBlankIntrWait();
    }
}

static u16 autotest_input(void)
{
    u16 phase = (level_tick >> 5) & 3;
    u16 keys = KEY_A;
#ifdef AUTOTEST_STATIONARY_PLAYER
    /*
     * Screenshot regression mode: hold a neutral pose so two frames from
     * different level ticks can be compared without position compensation.
     */
    return 0;
#endif
    if (game_state == STATE_BOSS) {
        if (player_x + 16 < boss_x + 32) keys |= KEY_RIGHT;
        if (player_x + 16 > boss_x + 32) keys |= KEY_LEFT;
        return keys;
    }
    if (phase == 0) keys |= KEY_RIGHT;
    if (phase == 1) keys |= KEY_UP;
    if (phase == 2) keys |= KEY_LEFT;
    if (phase == 3) keys |= KEY_DOWN;
    return keys;
}

#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
static u8 autotest_explosion_visible(void)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) continue;
#ifdef AUTOTEST_SCREENSHOT_EXPLOSION_FRAME
        if (effects[index].frame == AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) {
            return 1;
        }
#else
        return 1;
#endif
    }
    return 0;
}
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
static u8 autotest_reward_visible(void)
{
    u8 index;
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) return 1;
    }
    return 0;
}
#endif
#endif

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
                         * The first VBlank commits OAM; the second lets mGBA
                         * finish one rasterized frame before taking the
                         * final framebuffer screenshot.
                         */
                        autotest_screenshot_delay = 2;
                    }
#endif
                }
            }
            telemetry_display_frames++;
        }
    }
}
