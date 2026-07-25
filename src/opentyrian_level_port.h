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

#include "opentyrian_data.h"

enum {
    OT_LOGICAL_SCREEN_WIDTH = 320,
    OT_LOGICAL_SCREEN_HEIGHT = 200,
    OT_GAME_VIEW_WIDTH = 264,
    OT_GAME_VIEW_HEIGHT = 184,
    OT_RIGHT_HUD_WIDTH = 56,
    OT_BOTTOM_BANNER_HEIGHT = 16,
    OT_ENEMY_COUNT = 100,
    OT_ENEMY_POOL_SIZE = 25,
    OT_GLOBAL_FLAG_COUNT = 10,
    OT_NEW_PL_COUNT = 10,
    OT_MT_STATE_COUNT = 624,
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
 * This state intentionally keeps OpenTyrian field names and integer widths.
 * It runs beside the legacy v11 loop until each deferred event/gameplay
 * function has been translated and can become authoritative.
 */
typedef struct {
    uint16_t event_index;
    uint16_t cur_loc;

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
    uint8_t stop_background_num;
    uint8_t background3_over;
    uint8_t background2_over;
    int16_t starfield_speed;
    uint16_t level_enemy_frequency;
    uint16_t super_enemy_254_jump;
    int16_t current_song;
    uint8_t level_end;
    uint16_t map_x;
    uint16_t map_x3;
    uint16_t total_enemy;
    uint8_t last_created_slot;
    int16_t player_x;
    int16_t player_y;

    bool star_active;
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
    bool random_explosions;
    bool small_enemy_adjust;
    bool assets_valid;

    OtEnemy enemy[OT_ENEMY_COUNT];
    uint8_t enemy_avail[OT_ENEMY_COUNT];
    uint8_t global_flags[OT_GLOBAL_FLAG_COUNT];
    uint8_t new_pl[OT_NEW_PL_COUNT];
    OtMt19937 rng;

    uint32_t applied_event_count;
    uint32_t deferred_event_count;
    uint32_t skipped_event_count;
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
    uint32_t enemy_launch_attempt_count;
    uint32_t enemy_launch_success_count;
    uint32_t enemy_launch_pool_full_count;
    uint32_t enemy_launch_missing_definition_count;
    uint32_t random_spawn_attempt_count;
    uint32_t random_spawn_success_count;
    uint32_t random_spawn_pool_full_count;
    uint32_t random_spawn_missing_definition_count;
} OtLevelPortState;

void ot_level_port_init(OtLevelPortState *state);
void ot_level_port_advance(
    OtLevelPortState *state,
    uint16_t cur_loc,
    int16_t player_x,
    int16_t player_y
);
bool ot_level1_event_read(uint16_t index, OtEventRecord *event);
bool ot_level1_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy);

#endif
