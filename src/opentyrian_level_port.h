/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * First-level source-parity types derived from OpenTyrian:
 *   src/varz.h, src/episodes.h, src/tyrian2.c
 * Source revision: 1c34d1bddac8c8f2de834229d04b5a729525c944
 */
#ifndef TYRIAN_GBA_OPENTYRIAN_LEVEL_PORT_H
#define TYRIAN_GBA_OPENTYRIAN_LEVEL_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include <gba_base.h>

#include "opentyrian_data.h"

#ifndef TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
#define TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK 1
#endif
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK != 0 && \
    TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK != 1
#error TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK must be 0 or 1
#endif

#ifndef TYRIAN_GBA_ENEMY_ACTIVE_MASK
#define TYRIAN_GBA_ENEMY_ACTIVE_MASK 1
#endif
#if TYRIAN_GBA_ENEMY_ACTIVE_MASK != 0 && \
    TYRIAN_GBA_ENEMY_ACTIVE_MASK != 1
#error TYRIAN_GBA_ENEMY_ACTIVE_MASK must be 0 or 1
#endif

/* Reuse the exact enemy directory for linked-hit and player-contact scans. */
#ifndef TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY
#define TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY 1
#endif
#if TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY != 0 && \
    TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY != 1
#error TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY must be 0 or 1
#endif
#define TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY_ENABLED ( \
    TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY && \
    TYRIAN_GBA_ENEMY_ACTIVE_MASK \
)

/*
 * Measured collision-kernel experiments.  They are separate from the
 * active-mask semantic switch so the deterministic full-loadout harness can
 * compare one code-generation change at a time.
 */
#ifndef TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
#define TYRIAN_GBA_COLLISION_UNSIGNED_RANGE 0
#endif
#ifndef TYRIAN_GBA_COLLISION_MASK_FAST_PATH
#define TYRIAN_GBA_COLLISION_MASK_FAST_PATH 1
#endif
#ifndef TYRIAN_GBA_COLLISION_LAZY_RESULT
#define TYRIAN_GBA_COLLISION_LAZY_RESULT 1
#endif
#ifndef TYRIAN_GBA_COLLISION_PACKED_CALL
/*
 * Pass damage and optional radii in one ARM register.  The exact SAVARA
 * save/loadout A/B retained every route invariant, reduced linked text by
 * 72 bytes and reduced deterministic missed VBlanks from 32 to 30.
 */
#define TYRIAN_GBA_COLLISION_PACKED_CALL 1
#endif
#if TYRIAN_GBA_COLLISION_UNSIGNED_RANGE != 0 && \
    TYRIAN_GBA_COLLISION_UNSIGNED_RANGE != 1
#error TYRIAN_GBA_COLLISION_UNSIGNED_RANGE must be 0 or 1
#endif
#if TYRIAN_GBA_COLLISION_MASK_FAST_PATH != 0 && \
    TYRIAN_GBA_COLLISION_MASK_FAST_PATH != 1
#error TYRIAN_GBA_COLLISION_MASK_FAST_PATH must be 0 or 1
#endif
#if TYRIAN_GBA_COLLISION_LAZY_RESULT != 0 && \
    TYRIAN_GBA_COLLISION_LAZY_RESULT != 1
#error TYRIAN_GBA_COLLISION_LAZY_RESULT must be 0 or 1
#endif
#if TYRIAN_GBA_COLLISION_PACKED_CALL != 0 && \
    TYRIAN_GBA_COLLISION_PACKED_CALL != 1
#error TYRIAN_GBA_COLLISION_PACKED_CALL must be 0 or 1
#endif

/*
 * Build one compact, source-exact collision record per active enemy at the
 * beginning of the player-shot phase.  The ARM miss path then reads two
 * adjacent EWRAM words instead of four fields spread across a 134-byte
 * OtEnemy record.  Hit-side gameplay remains in the authoritative C path.
 */
#ifndef TYRIAN_GBA_COLLISION_SNAPSHOT
#define TYRIAN_GBA_COLLISION_SNAPSHOT ( \
    TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK && \
    TYRIAN_GBA_COLLISION_MASK_FAST_PATH \
)
#endif
#if TYRIAN_GBA_COLLISION_SNAPSHOT != 0 && \
    TYRIAN_GBA_COLLISION_SNAPSHOT != 1
#error TYRIAN_GBA_COLLISION_SNAPSHOT must be 0 or 1
#endif
#if TYRIAN_GBA_COLLISION_MASK_FAST_PATH && \
    !TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
#error TYRIAN_GBA_COLLISION_MASK_FAST_PATH requires the active mask
#endif
#define TYRIAN_GBA_COLLISION_SNAPSHOT_ENABLED ( \
    TYRIAN_GBA_COLLISION_SNAPSHOT && \
    TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK && \
    TYRIAN_GBA_COLLISION_MASK_FAST_PATH && \
    TYRIAN_GBA_COLLISION_LAZY_RESULT && \
    !TYRIAN_GBA_COLLISION_UNSIGNED_RANGE \
)

enum {
    OT_LOGICAL_SCREEN_WIDTH = 320,
    OT_LOGICAL_SCREEN_HEIGHT = 200,
    OT_GAME_VIEW_WIDTH = 264,
    OT_GAME_VIEW_HEIGHT = 184,
    OT_RIGHT_HUD_WIDTH = 56,
    OT_BOTTOM_BANNER_HEIGHT = 16,
    OT_ENEMY_COUNT = 100,
    OT_ENEMY_POOL_SIZE = 25,
    OT_ENEMY_SHOT_COUNT = 60,
    OT_GLOBAL_FLAG_COUNT = 10,
    OT_NEW_PL_COUNT = 10,
    OT_MT_STATE_COUNT = 624,
    OT_STARFIELD_STAR_COUNT = 100,
    OT_HIT_EFFECT_COUNT = 16,
    OT_PICKUP_EFFECT_COUNT = 16,
    OT_FRAME_EXPLOSION_COUNT = 16,
};

/*
 * Direct fixed-width counterpart of OpenTyrian's JE_SingleEnemyType.
 *
 * GBA_PORT: sprite2s and enemydatofs are PC pointers.  The source-parity
 * runtime stores their stable shape-bank/enemy-definition identifiers
 * instead; all gameplay fields retain their original signedness and width.
 */
typedef struct {
    uint8_t fillbyte;
    int16_t ex;
    int16_t ey;
    int8_t exc;
    int8_t eyc;
    int8_t exca;
    int8_t eyca;
    int8_t excc;
    int8_t eycc;
    int8_t exccw;
    int8_t eyccw;
    uint8_t armorleft;
    uint8_t eshotwait[3];
    uint8_t eshotmultipos[3];
    uint8_t enemycycle;
    uint8_t ani;
    uint16_t egr[20];
    uint8_t size;
    uint8_t linknum;
    uint8_t aniactive;
    uint8_t animax;
    uint8_t aniwhenfire;
    uint8_t shape_table;
    /*
     * OpenTyrian stores a Sprite2_array pointer, not the authored bank ID.
     * 0..3 identify enemySpriteSheets[]; the remaining values model its two
     * fixed sheets and a cold-start NULL pointer.
     */
    uint8_t shape_slot;
    int8_t exrev;
    int8_t eyrev;
    int16_t exccadd;
    int16_t eyccadd;
    uint8_t exccwmax;
    uint8_t eyccwmax;
    uint16_t enemy_definition_id;
    bool edamaged;
    uint16_t enemytype;
    uint8_t animin;
    uint16_t edgr;
    int8_t edlevel;
    int8_t edani;
    uint8_t fill1;
    uint8_t filter;
    int16_t evalue;
    int16_t fixedmovey;
    uint8_t freq[3];
    uint8_t launchwait;
    uint16_t launchtype;
    uint8_t launchfreq;
    uint8_t xaccel;
    uint8_t yaccel;
    uint8_t tur[3];
    uint16_t enemydie;
    bool enemyground;
    uint8_t explonum;
    uint16_t mapoffset;
    bool scoreitem;
    bool special;
    uint8_t flagnum;
    bool setto;
    uint8_t iced;
    uint8_t launchspecial;
    int16_t xminbounce;
    int16_t xmaxbounce;
    int16_t yminbounce;
    int16_t ymaxbounce;
    uint8_t fill[3];
} OtEnemy;

/*
 * Fixed-width counterpart of OpenTyrian's EnemyShotType plus an explicit
 * active flag.  OpenTyrian stores availability in a parallel 60-entry
 * boolean array; keeping the flag beside the record is the only GBA_PORT
 * layout change.
 */
typedef struct {
    bool active;
    int16_t sx;
    int16_t sy;
    int16_t sxm;
    int16_t sym;
    int8_t sxc;
    int8_t syc;
    uint8_t tx;
    uint8_t ty;
    uint16_t sgr;
    uint8_t sdmg;
    uint8_t duration;
    uint16_t animate;
    uint16_t animax;
} OtEnemyShot;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t velocity_x;
    int16_t velocity_y;
    uint8_t damage;
} OtEnemyShotImpact;

typedef bool (*OtEnemyShotImpactHandler)(
    void *context,
    const OtEnemyShotImpact *impact
);

typedef struct {
    int16_t x;
    int16_t y;
    bool large;
    bool ground;
    uint8_t repeat_count;
} OtHitEffect;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t explosion_type;
} OtPickupEffect;

/* One-tick JE_setupExplosion() request emitted by translated enemy logic. */
typedef struct {
    int16_t x;
    int16_t y;
    int8_t delta_y;
    uint8_t explosion_type;
    bool fixed_position;
} OtFrameExplosion;

/*
 * Presentation command captured at the exact blit_enemy() point inside the
 * translated JE_drawEnemy() phase.  Gameplay may move or release the slot
 * later in the same tick; retaining this command preserves the PC draw order.
 */
typedef struct {
    bool active;
    int16_t x;
    int16_t y;
    uint16_t graphic;
    uint8_t shape_table;
    uint8_t size;
    uint8_t filter;
    uint8_t pool;
    uint8_t source_index;
    uint8_t enemy_cycle;
    uint16_t enemy_definition_id;
} OtEnemyDrawCommand;

typedef struct {
    bool collided;
    bool consumed;
    uint8_t remaining_damage;
    uint8_t hit_count;
    uint8_t kill_count;
    uint8_t effect_count;
    uint8_t superpixel_hit_count;
    uint8_t data_cubes_awarded;
    uint32_t cash_awarded;
    OtHitEffect effects[OT_HIT_EFFECT_COUNT];
} OtShotCollisionResult;

/*
 * Exact compact form of the four fields used by player-shot AABB tests.
 * y already includes the source enemycycle-dependent -12/-6 adjustment;
 * radius_x/radius_y retain the strict source bounds 25/29 or 13/15.
 */
typedef struct {
    int16_t x;
    int16_t y;
    uint16_t radius_x;
    uint16_t radius_y;
} OtPlayerShotCollisionSnapshot;

enum {
    OT_PICKUP_MESSAGE_NONE = 0,
    OT_PICKUP_MESSAGE_FRONT_POWER,
    OT_PICKUP_MESSAGE_REAR_POWER,
    OT_PICKUP_MESSAGE_WEAPON_PORT,
    OT_PICKUP_MESSAGE_SPECIAL,
    OT_PICKUP_MESSAGE_OPTION,
};

typedef struct {
    uint8_t pickup_count;
    uint8_t contact_count;
    uint16_t damage;
    /*
     * mainint.c:JE_playerCollide() applies enemy motion as ship push-back
     * before JE_playerMovement() performs its normal friction/clamp pass.
     * Keep the pre-clamp sum wide: a 255-armour authored object can produce
     * a value far outside int8_t even though movement later clamps to +/-4.
     */
    int32_t player_velocity_x_delta;
    int32_t player_velocity_y_delta;
    uint8_t effect_count;
    uint8_t data_cubes_awarded;
    uint8_t front_powerups;
    uint8_t rear_powerups;
    uint8_t orbiting_asteroids_awarded;
    uint8_t superbombs_awarded;
    uint8_t pickup_message_type;
    uint8_t pickup_message_item_id;
    bool reset_all_shot_multi_pos;
    bool front_weapon_picked_up;
    bool special_weapon_picked_up;
    bool bonus_level_triggered;
    uint16_t next_level;
    uint32_t cash_awarded;
    OtHitEffect effects[OT_HIT_EFFECT_COUNT];
    uint8_t pickup_effect_count;
    OtPickupEffect pickup_effects[OT_PICKUP_EFFECT_COUNT];
} OtPlayerCollisionResult;

/*
 * State-form MT19937 equivalent of OpenTyrian src/mtrand.c.  Keeping it in
 * the level context makes RNG ownership explicit while preserving its exact
 * 32-bit sequence and call order.
 */
typedef struct {
    uint32_t values[OT_MT_STATE_COUNT];
    uint16_t p0;
    uint16_t p1;
    uint16_t pm;
} OtMt19937;

/*
 * This authoritative selected-level state intentionally keeps OpenTyrian
 * field names and integer widths so source-flow comparisons remain direct.
 */
typedef struct {
    uint16_t event_index;
    uint16_t event_count;
    uint16_t cur_loc;
    uint16_t return_loc;
    uint16_t level_timer_countdown;
    uint16_t level_timer_jump_to;

    uint16_t back_move;
    uint16_t back_move2;
    uint16_t back_move3;
    uint16_t explode_move;
    uint8_t map1_y_delay;
    uint8_t map1_y_delay_max;
    uint8_t map2_y_delay;
    uint8_t map2_y_delay_max;

    uint8_t shape_bank[4];
    uint8_t boss_bar_link[2];
    uint8_t boss_bar_color[2];
    uint8_t difficulty_level;
    uint8_t initial_difficulty;
    uint8_t damage_rate;
    uint8_t stop_background_num;
    uint8_t background3_over;
    uint8_t background2_over;
    uint8_t pending_text_window;
    int16_t starfield_speed;
    uint16_t armor_ship_delay;
    uint16_t warning_sound_delay;
    int16_t warning_color;
    int8_t warning_color_change;
    uint16_t level_enemy_frequency;
    uint16_t super_enemy_254_jump;
    uint16_t galaga_shot_frequency;
    uint32_t galaga_life_threshold;
    int16_t current_song;
    uint8_t music_fade_volume;
    uint8_t level_end;
    uint16_t map_x;
    uint16_t map_x3;
    uint16_t map1_pointer_offset;
    uint16_t map2_pointer_offset;
    uint16_t background2_wrap_offset;
    uint16_t background2_wrap_to_offset;
    int8_t level_filter;
    int8_t level_filter_new;
    int8_t level_brightness;
    int8_t level_brightness_change;
    uint8_t smoothies[9];
    uint8_t smoothie_data[9];
    /*
     * JE_mainGamePlayerFunctions() derives these horizontal offsets from
     * the player in PC coordinates.  The presentation fields retain the
     * values consumed by the current background/enemy draw phase.
     */
    uint16_t map_x_offset;
    uint16_t map_x2_offset;
    uint16_t map_x3_offset;
    uint16_t presentation_map_x_offset;
    uint16_t presentation_map_x2_offset;
    uint16_t presentation_map_x3_offset;
    uint16_t total_enemy;
    uint8_t last_created_slot;
    int16_t player_x;
    int16_t player_y;

    bool star_active;
    bool warning_active;
    bool enemies_active;
    bool stop_backgrounds;
    bool top_enemy_over;
    bool sky_enemy_over_all;
    bool background2_not_transparent;
    bool background3_x1;
    bool background3_x1b;
    bool enemy_continual_damage;
    bool force_events;
    bool music_fade;
    bool ready_to_end_level;
    bool end_level;
    bool really_end_level;
    /*
     * tyrian2.c calculates allPlayersGone immediately before JE_eventSystem().
     * Keep that phase result explicit so event 11 and the level-tail gates use
     * the same single-player/death semantics as the PC loop.
     */
    bool all_players_gone;
    bool random_explosions;
    bool small_enemy_adjust;
    bool arcade_mode;
    /* levelsN.dat ]e / ]g are independent of the user's Play Mode. */
    bool engage_mode;
    bool galaga_mode;
    bool level_timer;
    bool return_active;
    bool filter_active;
    bool filter_fade;
    bool filter_fade_start;
    bool map_position_override_pending;
    bool background2_wrap_pending;
    bool assets_valid;
    bool parallax_initialized;
    bool presentation_parallax_initialized;
    bool presentation_background3_x1;

    OtEnemy enemy[OT_ENEMY_COUNT];
    uint8_t enemy_avail[OT_ENEMY_COUNT];
    /*
     * Runtime-only acceleration index for the player-shot collision phase.
     * OpenTyrian's authoritative enemy_avail[] state remains unchanged.
     * The mask is rebuilt once per phase and updated immediately when a
     * death spawn or release mutates the pool, preserving source scan order.
     */
    uint32_t player_shot_collision_active_mask[4];
    bool player_shot_collision_mask_active;
    OtEnemyDrawCommand enemy_draw[OT_ENEMY_COUNT];
    OtEnemyShot enemy_shot[OT_ENEMY_SHOT_COUNT];
    uint8_t global_flags[OT_GLOBAL_FLAG_COUNT];
    uint8_t new_pl[OT_NEW_PL_COUNT];
    /*
     * Literal backgrnd.c 320x200 starfield state, including u16 wrap.
     * star_meta packs source colour 0x90..0x9f in bits 4..7 and the
     * source speed 2..4 minus two in bits 0..1.
     */
    uint16_t star_position[OT_STARFIELD_STAR_COUNT];
    uint8_t star_meta[OT_STARFIELD_STAR_COUNT];
    OtMt19937 rng;
    uint16_t frame_sound_mask;
    uint8_t frame_sound_queue[8];
    bool frame_music_song_request;
    uint16_t frame_player_damage;
    uint16_t frame_player_invulnerable_ticks;
    int32_t frame_player_velocity_x_delta;
    int32_t frame_player_velocity_y_delta;
    uint8_t frame_explosion_count;
    OtFrameExplosion frame_explosion[OT_FRAME_EXPLOSION_COUNT];
    uint8_t frame_enemy_on_screen;
    uint8_t frame_ground_enemy_on_screen;
    uint8_t frame_sky_enemy_on_screen;

    /*
     * Fixed single-player state required by the directly translated
     * JE_playerCollide()/power_up_weapon() score-item branches.
     */
    uint8_t player_front_weapon_id;
    uint8_t player_front_weapon_power;
    uint8_t player_rear_weapon_id;
    uint8_t player_rear_weapon_power;
    uint8_t player_superbombs;
    uint8_t player_armor;
    uint8_t player_shot_hit_area_x;
    uint8_t player_shot_hit_area_y;
    uint8_t player_weapon_mode;
    uint8_t player_special;
    uint8_t player_sidekick[2];
    uint8_t player_generator;
    uint8_t player_shield_item;
    uint8_t player_ship;
    uint8_t player_super_arcade_mode;
    uint8_t super_arcade_power_up;
    uint8_t player_sidekick_level;
    uint8_t player_sidekick_series;
    uint8_t player_purple_balls_needed;
    bool bonus_level;
    uint16_t next_level;
    uint16_t display_time;
    uint8_t display_flash;
    int8_t display_flash_change;

    uint32_t applied_event_count;
    uint32_t deferred_event_count;
    uint32_t skipped_event_count;
    uint32_t event_jump_count;
    uint32_t super_enemy_254_jump_count;
    uint32_t spawn_attempt_count;
    uint32_t spawn_success_count;
    uint32_t spawn_pool_full_count;
    uint32_t spawn_missing_definition_count;
    uint32_t active_enemy_count;
    uint32_t max_active_enemy_count;
    uint32_t enemy_control_write_count;
    uint32_t rng_call_count;
    uint32_t enemy_motion_update_count;
    uint32_t enemy_release_count;
    uint32_t enemy_shot_trigger_count;
    uint32_t enemy_shot_spawn_count;
    uint32_t enemy_shot_drop_count;
    uint32_t enemy_shot_active_count;
    uint32_t enemy_shot_max_active_count;
    uint32_t enemy_shot_motion_update_count;
    uint32_t enemy_shot_release_count;
    uint32_t enemy_shot_player_hit_count;
    uint32_t enemy_launch_attempt_count;
    uint32_t enemy_launch_success_count;
    uint32_t enemy_launch_pool_full_count;
    uint32_t enemy_launch_missing_definition_count;
    uint32_t random_spawn_attempt_count;
    uint32_t random_spawn_success_count;
    uint32_t random_spawn_pool_full_count;
    uint32_t random_spawn_missing_definition_count;
    uint32_t death_spawn_attempt_count;
    uint32_t death_spawn_success_count;
    uint32_t death_spawn_pool_full_count;
    uint32_t death_spawn_missing_definition_count;
    uint32_t warning_ship_spawn_attempt_count;
    uint32_t warning_ship_spawn_success_count;
    uint32_t warning_ship_spawn_pool_full_count;
    uint32_t warning_ship_spawn_missing_definition_count;
    uint32_t player_shot_collision_count;
    uint32_t player_shot_collision_mask_rebuild_count;
    uint32_t player_shot_collision_candidate_visit_count;
    uint32_t player_shot_collision_linear_slot_visit_count;
    uint32_t player_enemy_contact_count;
    uint32_t enemy_kill_count;
    uint32_t direct_cash_awarded;
    uint32_t score_item_spawn_count;
    uint32_t score_item_pickup_count;
    uint32_t score_item_max_active_count;
    uint32_t score_item_active_count;
    uint32_t score_item_unsupported_pickup_count;
    uint32_t data_cube_pickup_count;
    uint32_t front_weapon_powerup_count;
    uint32_t rear_weapon_powerup_count;
    uint32_t powerup_consolation_cash;
    uint32_t orbiting_asteroid_pickup_count;
    uint32_t superbomb_pickup_count;
    uint32_t hotdog_pickup_count;
    uint32_t armor_pickup_count;
    uint32_t bonus_portal_pickup_count;
    uint32_t high_value_pickup_count;
    uint32_t death_control_event_count;
    uint32_t death_assignment_count;
#if TYRIAN_GBA_COLLISION_SNAPSHOT_ENABLED
    /* Cold relative to OtEnemy, hot and tightly packed during shot scans. */
    OtPlayerShotCollisionSnapshot
        player_shot_collision_snapshot[OT_ENEMY_COUNT];
#endif
#if TYRIAN_GBA_ENEMY_ACTIVE_MASK
    /* Runtime-only directory; enemy_avail[] remains authoritative. */
    uint32_t enemy_active_mask[4];
#endif
    /*
     * GBA presentation identity only.  A pool slot can be released and
     * reused within one logic tick, so a renderer that merely compares
     * enemy_avail[] once per frame cannot distinguish the two instances.
     * The centralized availability setter advances this generation on each
     * free -> active transition.  It is reset at level initialization and
     * never participates in PC gameplay, collision, event or save data.
     */
    uint16_t enemy_instance_generation[OT_ENEMY_COUNT];
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    uint32_t enemy_pool_active_visits;
    uint32_t enemy_pool_linear_visits;
    uint32_t enemy_allocator_mask_word_probes;
    uint32_t enemy_allocator_slot_probes;
    uint32_t collision_hit_apply_calls;
    uint32_t collision_status_link_visits;
    uint32_t collision_kill_group_visits;
    uint32_t collision_damaged_transition_visits;
    uint32_t collision_player_contact_visits;
    uint32_t collision_zinglon_visits;
#endif
    /*
     * Build-time LVL analysis marks authored Boss component spawn events
     * before Event 79 exposes their health-bar links.  Keep this cold,
     * presentation-only state after every assembly-addressed hot field so
     * adding the manifest cannot perturb the ARM collision layout.
     */
    uint8_t selected_episode;
    uint16_t selected_lvl_file_number;
    uint32_t boss_manifest_active_mask[4];
    uint32_t boss_manifest_spawn_count;
} OtLevelPortState;

void ot_level_port_set_enemy_avail(
    OtLevelPortState *state,
    uint8_t enemy_index,
    uint8_t avail
);

void ot_level_port_set_boss_manifest_identity(
    OtLevelPortState *state,
    uint8_t episode,
    uint16_t lvl_file_number
);

bool ot_level_port_boss_manifest_member(
    const OtLevelPortState *state,
    uint8_t enemy_index
);

uint32_t ot_level_port_boss_manifest_spawn_count(
    const OtLevelPortState *state
);

uint32_t ot_level_port_boss_manifest_active_count(
    const OtLevelPortState *state
);

uint8_t ot_level_port_boss_manifest_episode(
    const OtLevelPortState *state
);

uint16_t ot_level_port_boss_manifest_lvl_file_number(
    const OtLevelPortState *state
);

void ot_level_port_init(
    OtLevelPortState *state,
    uint8_t difficulty_level,
    bool arcade_mode,
    bool preserve_shape_history
);
void ot_level_port_advance(
    OtLevelPortState *state,
    uint16_t cur_loc,
    int16_t player_x,
    int16_t player_y
);
void ot_level_port_advance_over_player_enemies(OtLevelPortState *state);
void ot_level_port_update_parallax(
    OtLevelPortState *state,
    int16_t player_x
);
void ot_level_port_initialize_starfield(OtLevelPortState *state);
void ot_level_port_update_starfield(
    OtLevelPortState *state,
    bool force_active
);
void ot_level_port_update_low_armor_warning(
    OtLevelPortState *state,
    bool player_alive
);
void ot_level_port_update_return_active(OtLevelPortState *state);
void ot_level_port_update_filter_fade(OtLevelPortState *state);
void ot_level_port_update_level_timer(OtLevelPortState *state);
void ot_level_port_recalculate_player_power_progress(
    OtLevelPortState *state
);
void ot_level_port_update_enemy_shots(
    OtLevelPortState *state,
    OtEnemyShotImpactHandler impact_handler,
    void *impact_context
);
uint32_t ot_level_port_random(OtLevelPortState *state);
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
uint32_t ot_level_port_stress_round_ratio_call_count(void);
#endif
ARM_CODE void ot_level_port_begin_player_shot_collision_phase(
    OtLevelPortState *state
);
void ot_level_port_end_player_shot_collision_phase(
    OtLevelPortState *state
);
/*
 * Strict source-parity axis test used by the ARM collision kernels.  It is
 * public only so the GBA differential harness can exercise every int16_t
 * input; gameplay callers should normally use the packed collision API.
 */
#if TYRIAN_GBA_HOTPATH_ASM
#if TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
IWRAM_CODE ARM_CODE bool ot_player_shot_axis_overlaps_unsigned_asm(
    int16_t delta,
    uint16_t radius
);
#define ot_player_shot_axis_overlaps \
    ot_player_shot_axis_overlaps_unsigned_asm
#else
IWRAM_CODE ARM_CODE bool ot_player_shot_axis_overlaps(
    int16_t delta,
    uint16_t radius
);
#endif
#endif
IWRAM_CODE ARM_CODE void ot_level_port_collide_player_shot(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    uint8_t damage,
    OtShotCollisionResult *result
);
void ot_level_port_collide_zinglon_beam(
    OtLevelPortState *state,
    int16_t beam_x,
    uint8_t half_width,
    OtShotCollisionResult *result
);
IWRAM_CODE ARM_CODE void ot_level_port_collide_player_shot_sized(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    uint8_t damage,
    uint8_t radius_w,
    uint8_t radius_h,
    OtShotCollisionResult *result
);
#if TYRIAN_GBA_HOTPATH_ASM
IWRAM_CODE ARM_CODE void ot_level_port_collide_player_shot_packed_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_instrumented_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_generic_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_snapshot_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
#if \
    TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK && \
    TYRIAN_GBA_COLLISION_MASK_FAST_PATH && \
    TYRIAN_GBA_COLLISION_LAZY_RESULT && \
    !TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
#if TYRIAN_GBA_COLLISION_SNAPSHOT_ENABLED
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
#define ot_level_port_collide_player_shot_packed \
    ot_level_port_collide_player_shot_packed_snapshot_instrumented_asm
#else
#define ot_level_port_collide_player_shot_packed \
    ot_level_port_collide_player_shot_packed_snapshot_asm
#endif
#else
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
#define ot_level_port_collide_player_shot_packed \
    ot_level_port_collide_player_shot_packed_instrumented_asm
#else
#define ot_level_port_collide_player_shot_packed \
    ot_level_port_collide_player_shot_packed_asm
#endif
#endif
#else
#define ot_level_port_collide_player_shot_packed \
    ot_level_port_collide_player_shot_packed_generic_asm
#endif
#else
IWRAM_CODE ARM_CODE void ot_level_port_collide_player_shot_packed(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
#endif
IWRAM_CODE ARM_CODE bool ot_level_port_player_shot_overlaps(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y
);
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
uint32_t ot_level_port_hotpath_asm_differential_test(
    OtLevelPortState *state
);
IWRAM_CODE ARM_CODE bool
ot_player_shot_axis_overlaps_c_reference(
    int16_t delta,
    uint16_t radius
);
#endif
void ot_level_port_collide_player(
    OtLevelPortState *state,
    bool player_vulnerable,
    OtPlayerCollisionResult *result
);
void ot_level_port_clear_projectiles(OtLevelPortState *state);
bool ot_level_event_read(uint16_t index, OtEventRecord *event);
bool ot_level_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy);

#endif
