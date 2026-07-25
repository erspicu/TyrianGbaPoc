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

#include "res/asset_meta.h"

enum {
    OT_LOGICAL_SCREEN_WIDTH = 320,
    OT_LOGICAL_SCREEN_HEIGHT = 200,
    OT_GAME_VIEW_WIDTH = 264,
    OT_GAME_VIEW_HEIGHT = 184,
    OT_RIGHT_HUD_WIDTH = 56,
    OT_BOTTOM_BANNER_HEIGHT = 16,
};

typedef struct {
    uint16_t eventtime;
    uint8_t eventtype;
    int16_t eventdat;
    int16_t eventdat2;
    int8_t eventdat3;
    int8_t eventdat5;
    int8_t eventdat6;
    uint8_t eventdat4;
} OtEventRecord;

typedef struct {
    uint8_t ani;
    uint8_t tur[3];
    uint8_t freq[3];
    int8_t xmove;
    int8_t ymove;
    int8_t xaccel;
    int8_t yaccel;
    int8_t xcaccel;
    int8_t ycaccel;
    int16_t startx;
    int16_t starty;
    int8_t startxc;
    int8_t startyc;
    uint8_t armor;
    uint8_t esize;
    uint16_t egraphic[20];
    uint8_t explosiontype;
    uint8_t animate;
    uint8_t shapebank;
    int8_t xrev;
    int8_t yrev;
    uint16_t dgr;
    int8_t dlevel;
    int8_t dani;
    uint8_t elaunchfreq;
    uint16_t elaunchtype;
    int16_t value;
    uint16_t eenemydie;
} OtEnemyDefinition;

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
    bool assets_valid;

    uint32_t applied_event_count;
    uint32_t deferred_event_count;
} OtLevelPortState;

void ot_level_port_init(OtLevelPortState *state);
void ot_level_port_advance(OtLevelPortState *state, uint16_t cur_loc);
bool ot_level1_event_read(uint16_t index, OtEventRecord *event);
bool ot_level1_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy);

#endif
