/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct translation staging area for OpenTyrian's first-level game loop.
 * The field assignments below follow JE_main() and JE_eventSystem() in
 * src/tyrian2.c at revision
 * 1c34d1bddac8c8f2de834229d04b5a729525c944.
 */
#include "opentyrian_level_port.h"

enum {
    OT_MT_M = 397,
    OT_SOURCE_PARITY_TEST_SEED = 5489,
};

_Static_assert(sizeof(uint8_t) == 1, "OpenTyrian byte width changed");
_Static_assert(sizeof(int8_t) == 1, "OpenTyrian shortint width changed");
_Static_assert(sizeof(uint16_t) == 2, "OpenTyrian word width changed");
_Static_assert(sizeof(int16_t) == 2, "OpenTyrian integer width changed");
_Static_assert(
    OT_LEVEL1_EVENT_RECORD_BYTES == 11,
    "OpenTyrian event record width changed"
);
_Static_assert(
    OT_HDT_ENEMY_RECORD_BYTES == 77,
    "OpenTyrian enemy record width changed"
);
_Static_assert(OT_ENEMY_COUNT == 100, "OpenTyrian enemy pool size changed");
_Static_assert(OT_ENEMY_POOL_SIZE == 25, "OpenTyrian pool group size changed");

bool ot_level1_event_read(uint16_t index, OtEventRecord *event)
{
    return ot_data_level1_event_read(index, event);
}

bool ot_level1_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy)
{
    return ot_data_hdt_enemy_read(enemy_id, enemy);
}

/*
 * Direct state-form translation of OpenTyrian src/mtrand.c.  The original
 * program seeds this generator from time(NULL) in opentyr.c.  GBA_PORT:
 * shadow-mode regression uses a fixed seed so the ROM test is reproducible;
 * the generator and every gameplay-side call still follow the source's
 * exact 32-bit algorithm and ordering.
 *
 * MT19937 portion:
 * Copyright (C) 1997--2004, Makoto Matsumoto, Takuji Nishimura, and
 * Eric Landry; All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of its contributors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
static void ot_mt_seed(OtMt19937 *rng, uint32_t seed)
{
    uint16_t index;

    rng->values[0] = seed;
    for (index = 1; index < OT_MT_STATE_COUNT; index++) {
        uint32_t previous = rng->values[index - 1];
        rng->values[index] =
            1812433253u * (previous ^ (previous >> 30)) + index;
    }
    rng->p0 = 0;
    rng->p1 = 1;
    rng->pm = OT_MT_M;
}

static uint32_t ot_mt_rand(OtLevelPortState *state)
{
    OtMt19937 *rng = &state->rng;
    uint32_t y;
    uint32_t combined;
    uint32_t odd_mask;

    combined = (rng->values[rng->p0] & 0x80000000u) |
               (rng->values[rng->p1] & 0x7fffffffu);
    odd_mask = 0u - (rng->values[rng->p1] & 1u);
    y = rng->values[rng->pm] ^ (combined >> 1) ^
        (odd_mask & 0x9908b0dfu);
    rng->values[rng->p0] = y;

    rng->p0 = rng->p1;
    rng->p1++;
    rng->pm++;
    if (rng->pm == OT_MT_STATE_COUNT) rng->pm = 0;
    if (rng->p1 == OT_MT_STATE_COUNT) rng->p1 = 0;

    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    state->rng_call_count++;
    return y;
}

static int8_t ot_abs_s8(int16_t value)
{
    return (int8_t)(value < 0 ? -value : value);
}

static int16_t ot_abs_s16(int16_t value)
{
    return (int16_t)(value < 0 ? -value : value);
}

static bool shape_table_is_loaded(
    const OtLevelPortState *state,
    uint8_t shape_table
)
{
    uint8_t index;

    if (shape_table == 21 || shape_table == 26) return true;
    for (index = 0; index < 4; index++) {
        if (state->shape_bank[index] == shape_table) return true;
    }
    return false;
}

/*
 * Direct fixed-Normal/single-player translation of JE_makeEnemy().
 * Difficulty scaling collapses to the original JE_EnemyDat armor/value at
 * Normal, while every slot field assignment remains in source order.
 */
static bool ot_make_enemy(
    OtLevelPortState *state,
    OtEnemy *enemy,
    uint16_t enemy_definition_id,
    int16_t unique_shape_table,
    uint8_t *avail
)
{
    OtEnemyDefinition definition;
    uint8_t shape_table;
    uint8_t index;

    if (!ot_level1_enemy_read(enemy_definition_id, &definition)) {
        return false;
    }

    if (unique_shape_table > 0) {
        shape_table = (uint8_t)unique_shape_table;
    } else {
        shape_table = definition.shapebank;
    }

    /*
     * GBA_PORT: loaded PC Sprite2_array pointers become stable bank IDs.
     * As in JE_makeEnemy(), an unavailable bank leaves the previous slot's
     * value untouched (the APPROACH behavior).
     */
    if (shape_table_is_loaded(state, shape_table)) {
        enemy->shape_table = shape_table;
    }
    enemy->enemy_definition_id = enemy_definition_id;
    enemy->mapoffset = 0;

    for (index = 0; index < 3; index++) {
        enemy->eshotmultipos[index] = 0;
    }

    enemy->enemyground = (definition.explosiontype & 1u) == 0;
    enemy->explonum = definition.explosiontype >> 1;

    enemy->launchfreq = definition.elaunchfreq;
    enemy->launchwait = definition.elaunchfreq;
    enemy->launchtype = definition.elaunchtype % 1000u;
    enemy->launchspecial = definition.elaunchtype / 1000u;

    enemy->xaccel = (uint8_t)definition.xaccel;
    enemy->yaccel = (uint8_t)definition.yaccel;

    enemy->xminbounce = -10000;
    enemy->xmaxbounce = 10000;
    enemy->yminbounce = -10000;
    enemy->ymaxbounce = 10000;

    for (index = 0; index < 3; index++) {
        enemy->tur[index] = definition.tur[index];
    }

    enemy->ani = definition.ani;
    enemy->animin = 1;

    switch (definition.animate) {
    case 0:
        enemy->enemycycle = 1;
        enemy->aniactive = 0;
        enemy->animax = 0;
        enemy->aniwhenfire = 0;
        break;
    case 1:
        enemy->enemycycle = 0;
        enemy->aniactive = 1;
        enemy->animax = 0;
        enemy->aniwhenfire = 0;
        break;
    case 2:
        enemy->enemycycle = 1;
        enemy->aniactive = 2;
        enemy->animax = enemy->ani;
        enemy->aniwhenfire = 2;
        break;
    default:
        /*
         * The HDT format defines only 0..2.  Leave the same slot fields
         * untouched for invalid data rather than inventing behavior.
         */
        break;
    }

    if (definition.startxc != 0) {
        uint32_t range = (uint32_t)((int16_t)definition.startxc * 2);
        enemy->ex = (int16_t)(
            definition.startx +
            (int32_t)(ot_mt_rand(state) % range) -
            definition.startxc + 1
        );
    } else {
        enemy->ex = (int16_t)(definition.startx + 1);
    }

    if (definition.startyc != 0) {
        uint32_t range = (uint32_t)((int16_t)definition.startyc * 2);
        enemy->ey = (int16_t)(
            definition.starty +
            (int32_t)(ot_mt_rand(state) % range) -
            definition.startyc + 1
        );
    } else {
        enemy->ey = (int16_t)(definition.starty + 1);
    }

    enemy->exc = definition.xmove;
    enemy->eyc = definition.ymove;
    enemy->excc = definition.xcaccel;
    enemy->eycc = definition.ycaccel;
    enemy->exccw = ot_abs_s8(enemy->excc);
    enemy->exccwmax = (uint8_t)enemy->exccw;
    enemy->eyccw = ot_abs_s8(enemy->eycc);
    enemy->eyccwmax = (uint8_t)enemy->eyccw;
    enemy->exccadd = enemy->excc > 0 ? 1 : -1;
    enemy->eyccadd = enemy->eycc > 0 ? 1 : -1;
    enemy->special = false;
    enemy->iced = 0;

    if (definition.xrev == 0) {
        enemy->exrev = 100;
    } else if (definition.xrev == -99) {
        enemy->exrev = 0;
    } else {
        enemy->exrev = definition.xrev;
    }

    if (definition.yrev == 0) {
        enemy->eyrev = 100;
    } else if (definition.yrev == -99) {
        enemy->eyrev = 0;
    } else {
        enemy->eyrev = definition.yrev;
    }

    enemy->exca = enemy->xaccel > 0 ? 1 : -1;
    enemy->eyca = enemy->yaccel > 0 ? 1 : -1;
    enemy->enemytype = enemy_definition_id;

    for (index = 0; index < 3; index++) {
        if (enemy->tur[index] == 252) {
            enemy->eshotwait[index] = 1;
        } else if (enemy->tur[index] > 0) {
            enemy->eshotwait[index] = 20;
        } else {
            enemy->eshotwait[index] = 255;
        }
    }
    for (index = 0; index < 20; index++) {
        enemy->egr[index] = definition.egraphic[index];
    }
    enemy->size = definition.esize;
    enemy->linknum = 0;
    enemy->edamaged = definition.dani < 0;
    enemy->enemydie = definition.eenemydie;

    for (index = 0; index < 3; index++) {
        enemy->freq[index] = definition.freq[index];
    }

    enemy->edani = definition.dani;
    enemy->edgr = definition.dgr;
    enemy->edlevel = definition.dlevel;
    enemy->fixedmovey = 0;
    enemy->filter = 0;

    /* DIFFICULTY_NORMAL is an identity transform in the source switches. */
    enemy->evalue = definition.value;

    if (definition.armor > 0) {
        enemy->armorleft = definition.armor;
        *avail = 0;
        enemy->scoreitem = false;
    } else {
        *avail = 2;
        enemy->armorleft = 255;
        /*
         * Preserve the source's missing else assignment: a zero-value,
         * armorless definition leaves a recycled slot's scoreitem flag.
         */
        if (enemy->evalue != 0) enemy->scoreitem = true;
    }

    if (!enemy->scoreitem) state->total_enemy++;
    return true;
}

typedef enum {
    OT_SPAWN_EVENT,
    OT_SPAWN_LAUNCH,
    OT_SPAWN_RANDOM,
} OtSpawnOrigin;

static void ot_record_spawn_attempt(
    OtLevelPortState *state,
    OtSpawnOrigin origin
)
{
    switch (origin) {
    case OT_SPAWN_EVENT:
        state->spawn_attempt_count++;
        break;
    case OT_SPAWN_LAUNCH:
        state->enemy_launch_attempt_count++;
        break;
    case OT_SPAWN_RANDOM:
        state->random_spawn_attempt_count++;
        break;
    }
}

static void ot_record_spawn_pool_full(
    OtLevelPortState *state,
    OtSpawnOrigin origin
)
{
    switch (origin) {
    case OT_SPAWN_EVENT:
        state->spawn_pool_full_count++;
        break;
    case OT_SPAWN_LAUNCH:
        state->enemy_launch_pool_full_count++;
        break;
    case OT_SPAWN_RANDOM:
        state->random_spawn_pool_full_count++;
        break;
    }
}

static void ot_record_spawn_missing(
    OtLevelPortState *state,
    OtSpawnOrigin origin
)
{
    switch (origin) {
    case OT_SPAWN_EVENT:
        state->spawn_missing_definition_count++;
        break;
    case OT_SPAWN_LAUNCH:
        state->enemy_launch_missing_definition_count++;
        break;
    case OT_SPAWN_RANDOM:
        state->random_spawn_missing_definition_count++;
        break;
    }
}

static void ot_record_spawn_success(
    OtLevelPortState *state,
    OtSpawnOrigin origin
)
{
    switch (origin) {
    case OT_SPAWN_EVENT:
        state->spawn_success_count++;
        break;
    case OT_SPAWN_LAUNCH:
        state->enemy_launch_success_count++;
        break;
    case OT_SPAWN_RANDOM:
        state->random_spawn_success_count++;
        break;
    }
}

/*
 * Direct translation of JE_newEnemy()'s 25-slot allocation boundary.  Event,
 * launched and continual enemies share the source pool but keep separate
 * telemetry so one path cannot hide pressure in another.
 */
static uint8_t ot_new_enemy(
    OtLevelPortState *state,
    uint8_t enemy_offset,
    uint16_t enemy_definition_id,
    int16_t unique_shape_table,
    OtSpawnOrigin origin
)
{
    uint8_t index;
    uint8_t avail;
    uint8_t slot = OT_ENEMY_COUNT;

    ot_record_spawn_attempt(state, origin);
    state->last_created_slot = 0;
    for (
        index = enemy_offset;
        index < (uint8_t)(enemy_offset + OT_ENEMY_POOL_SIZE);
        index++
    ) {
        if (state->enemy_avail[index] == 1) {
            slot = index;
            break;
        }
    }
    if (slot == OT_ENEMY_COUNT) {
        ot_record_spawn_pool_full(state, origin);
        return 0;
    }
    if (!ot_make_enemy(
        state,
        &state->enemy[slot],
        enemy_definition_id,
        unique_shape_table,
        &avail
    )) {
        ot_record_spawn_missing(state, origin);
        state->assets_valid = false;
        return 0;
    }

    state->enemy_avail[slot] = avail;
    ot_record_spawn_success(state, origin);
    state->active_enemy_count++;
    if (state->active_enemy_count > state->max_active_enemy_count) {
        state->max_active_enemy_count = state->active_enemy_count;
    }
    state->last_created_slot = (uint8_t)(slot + 1);
    return state->last_created_slot;
}

static uint8_t ot_create_new_event_enemy(
    OtLevelPortState *state,
    OtEventRecord *event,
    uint8_t enemy_type_offset,
    uint8_t enemy_offset,
    int16_t unique_shape_table
)
{
    uint8_t created;
    uint16_t enemy_definition_id;
    OtEnemy *enemy;

    enemy_definition_id =
        (uint16_t)(event->eventdat + enemy_type_offset);
    created = ot_new_enemy(
        state,
        enemy_offset,
        enemy_definition_id,
        unique_shape_table,
        OT_SPAWN_EVENT
    );
    if (created == 0) return 0;
    enemy = &state->enemy[created - 1];

    if (event->eventdat2 != -99) {
        switch (enemy_offset) {
        case 0:
            enemy->ex = (int16_t)(
                event->eventdat2 - (state->map_x - 1) * 24
            );
            enemy->ey = (int16_t)(enemy->ey - state->back_move2);
            break;
        case 25:
        case 75:
            enemy->ex = (int16_t)(
                event->eventdat2 - (state->map_x - 1) * 24 - 12
            );
            enemy->ey = (int16_t)(enemy->ey - state->back_move);
            break;
        case 50:
            if (state->background3_x1) {
                enemy->ex = (int16_t)(
                    event->eventdat2 - (state->map_x - 1) * 24 - 12
                );
            } else {
                enemy->ex = (int16_t)(
                    event->eventdat2 - state->map_x3 * 24 - 24 * 2 + 6
                );
            }
            enemy->ey = (int16_t)(enemy->ey - state->back_move3);
            if (state->background3_x1b) enemy->ex -= 6;
            break;
        default:
            break;
        }
        enemy->ey = -28;
        if (state->background3_x1b && enemy_offset == 50) enemy->ey += 4;
    }

    if (state->small_enemy_adjust && enemy->size == 0) {
        enemy->ex -= 10;
        enemy->ey -= 7;
    }

    enemy->ey = (int16_t)(enemy->ey + event->eventdat5);
    enemy->eyc = (int8_t)(enemy->eyc + event->eventdat3);
    enemy->linknum = event->eventdat4;
    enemy->fixedmovey = event->eventdat6;
    return state->last_created_slot;
}

void ot_level_port_init(OtLevelPortState *state)
{
    const OtDataCatalog *data_catalog;
    OtEnemyDefinition first_spawn_definition;
    OtWeaponDefinition first_level_weapon;
    OtLevel1Info level_info;
    OtDataView data_view;
    OtShpSprite title_logo;
    OtMusSongInfo song_info;
    uint8_t index;

    *state = (OtLevelPortState){0};

    /*
     * JE_main(), start_level_first initialization.  UI, SDL, demo, network
     * and save-game calls are outside this GBA proof's agreed scope.
     */
    state->map1_y_delay = 1;
    state->map1_y_delay_max = 1;
    state->map2_y_delay = 1;
    state->map2_y_delay_max = 1;
    state->back_move = 1;
    state->back_move2 = 2;
    state->back_move3 = 3;
    state->explode_move = 2;
    state->starfield_speed = 1;
    state->star_active = true;
    state->enemies_active = true;
    state->background2_over = 1;
    state->level_enemy_frequency = 96;
    state->current_song = -1;
    state->level_end = 255;
    state->map_x = 1;
    state->map_x3 = 1;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        state->enemy_avail[index] = 1;
    }
    ot_mt_seed(&state->rng, OT_SOURCE_PARITY_TEST_SEED);

    state->assets_valid =
        ot_data_init() &&
        (data_catalog = ot_data_catalog()) != 0 &&
        data_catalog->initialized &&
        ot_data_level1_info(&level_info) &&
        level_info.map_file == 'Z' &&
        level_info.shape_file == 'Z' &&
        level_info.enemy_count == 7 &&
        level_info.event_count == OT_LEVEL1_EXPECTED_EVENT_COUNT &&
        ot_level1_enemy_read(10, &first_spawn_definition) &&
        first_spawn_definition.ani == 8 &&
        first_spawn_definition.armor == 3 &&
        first_spawn_definition.esize == 1 &&
        first_spawn_definition.shapebank == 1 &&
        first_spawn_definition.value == 15 &&
        ot_data_hdt_weapon_read(59, &first_level_weapon) &&
        first_level_weapon.multi == 1 &&
        ot_data_pic_view(4, &data_view) &&
        ot_data_pic_palette_view(4, &data_view) &&
        ot_data_shp_sprite_read(3, 146, &title_logo) &&
        title_logo.populated &&
        ot_data_comp_shape_bank_view(1, &data_view) &&
        ot_data_comp_shape_bank_view(2, &data_view) &&
        ot_data_comp_shape_bank_view(9, &data_view) &&
        ot_data_comp_shape_bank_view(20, &data_view) &&
        ot_data_mus_song_read(17, &data_view, &song_info) &&
        song_info.patch_count > 0 &&
        song_info.position_count > 0 &&
        ot_data_mus_song_read(29, &data_view, &song_info) &&
        song_info.patch_count > 0 &&
        song_info.position_count > 0;
}

static void resolve_new_pl_link(
    OtLevelPortState *state,
    OtEventRecord *event
)
{
    if (event->eventdat3 > 79 && event->eventdat3 < 90) {
        event->eventdat4 = state->new_pl[event->eventdat3 - 80];
    }
}

static bool apply_event(
    OtLevelPortState *state,
    OtEventRecord *event,
    uint16_t *skip_events
)
{
    uint8_t index;

    *skip_events = 0;
    switch (event->eventtype) {
    case 1:
        state->starfield_speed = event->eventdat;
        return true;

    case 2:
        state->map1_y_delay = 1;
        state->map1_y_delay_max = 1;
        state->map2_y_delay = 1;
        state->map2_y_delay_max = 1;
        state->back_move = (uint16_t)event->eventdat;
        state->back_move2 = (uint16_t)event->eventdat2;
        state->explode_move = state->back_move2 > 0 ?
            state->back_move2 : state->back_move;
        state->back_move3 = (uint16_t)(int16_t)event->eventdat3;
        if (state->back_move > 0) state->stop_background_num = 0;
        return true;

    case 3:
        state->back_move = 1;
        state->map1_y_delay = 3;
        state->map1_y_delay_max = 3;
        state->back_move2 = 1;
        state->map2_y_delay = 2;
        state->map2_y_delay_max = 2;
        state->back_move3 = 1;
        return true;

    case 5:
        state->shape_bank[0] =
            event->eventdat > 0 ? (uint8_t)event->eventdat : 0;
        state->shape_bank[1] =
            event->eventdat2 > 0 ? (uint8_t)event->eventdat2 : 0;
        state->shape_bank[2] =
            event->eventdat3 > 0 ? (uint8_t)event->eventdat3 : 0;
        state->shape_bank[3] =
            event->eventdat4 > 0 ? event->eventdat4 : 0;
        return true;

    case 6: /* Ground Enemy */
        ot_create_new_event_enemy(state, event, 0, 25, 0);
        return true;

    case 7: /* Top Enemy */
        ot_create_new_event_enemy(state, event, 0, 50, 0);
        return true;

    case 8:
        state->star_active = false;
        return true;

    case 9:
        state->star_active = true;
        return true;

    case 10: /* Ground Enemy 2 */
        ot_create_new_event_enemy(state, event, 0, 75, 0);
        return true;

    case 12: { /* Custom 4x4 Ground Enemy */
        uint8_t enemy_offset = 0;
        uint8_t created;

        switch (event->eventdat6) {
        case 0:
        case 1:
            enemy_offset = 25;
            break;
        case 2:
            enemy_offset = 0;
            break;
        case 3:
            enemy_offset = 50;
            break;
        case 4:
            enemy_offset = 75;
            break;
        default:
            break;
        }
        event->eventdat6 = 0;
        ot_create_new_event_enemy(state, event, 0, enemy_offset, 0);
        created = ot_create_new_event_enemy(
            state, event, 1, enemy_offset, 0
        );
        if (created > 0) state->enemy[created - 1].ex += 24;
        created = ot_create_new_event_enemy(
            state, event, 2, enemy_offset, 0
        );
        if (created > 0) state->enemy[created - 1].ey -= 28;
        created = ot_create_new_event_enemy(
            state, event, 3, enemy_offset, 0
        );
        if (created > 0) {
            state->enemy[created - 1].ex += 24;
            state->enemy[created - 1].ey -= 28;
        }
        return true;
    }

    case 13:
        state->enemies_active = false;
        return true;

    case 14:
        state->enemies_active = true;
        return true;

    case 15: /* Sky Enemy */
        ot_create_new_event_enemy(state, event, 0, 0, 0);
        return true;

    case 17: { /* Ground Bottom */
        uint8_t created =
            ot_create_new_event_enemy(state, event, 0, 25, 0);
        if (created > 0) {
            state->enemy[created - 1].ey =
                (int16_t)(190 + event->eventdat5);
        }
        return true;
    }

    case 18: { /* Sky Enemy on Bottom */
        uint8_t created =
            ot_create_new_event_enemy(state, event, 0, 0, 0);
        if (created > 0) {
            state->enemy[created - 1].ey =
                (int16_t)(190 + event->eventdat5);
        }
        return true;
    }

    case 19: { /* Enemy Global Move */
        uint8_t initial_index = 0;
        uint8_t maximum_index = 0;
        bool all_enemies = false;

        if (event->eventdat3 > 79 && event->eventdat3 < 90) {
            initial_index = 0;
            maximum_index = OT_ENEMY_COUNT;
            all_enemies = false;
            resolve_new_pl_link(state, event);
        } else {
            switch (event->eventdat3) {
            case 0:
                initial_index = 0;
                maximum_index = OT_ENEMY_COUNT;
                all_enemies = false;
                break;
            case 2:
                initial_index = 0;
                maximum_index = 25;
                all_enemies = true;
                break;
            case 1:
                initial_index = 25;
                maximum_index = 50;
                all_enemies = true;
                break;
            case 3:
                initial_index = 50;
                maximum_index = 75;
                all_enemies = true;
                break;
            case 99:
                initial_index = 0;
                maximum_index = OT_ENEMY_COUNT;
                all_enemies = true;
                break;
            default:
                break;
            }
        }

        for (index = initial_index; index < maximum_index; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (all_enemies || enemy->linknum == event->eventdat4) {
                if (event->eventdat != -99) {
                    enemy->exc = (int8_t)event->eventdat;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat2 != -99) {
                    enemy->eyc = (int8_t)event->eventdat2;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat6 != 0) {
                    enemy->fixedmovey = event->eventdat6;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat6 == -99) {
                    enemy->fixedmovey = 0;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat5 > 0) {
                    enemy->enemycycle = (uint8_t)event->eventdat5;
                    state->enemy_control_write_count++;
                }
            }
        }
        return true;
    }

    case 20: /* Enemy Global Accel */
        resolve_new_pl_link(state, event);
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (
                state->enemy_avail[index] != 1 &&
                (
                    enemy->linknum == event->eventdat4 ||
                    event->eventdat4 == 0
                )
            ) {
                if (event->eventdat != -99) {
                    enemy->excc = (int8_t)event->eventdat;
                    enemy->exccw = ot_abs_s8(event->eventdat);
                    enemy->exccwmax = (uint8_t)ot_abs_s8(event->eventdat);
                    enemy->exccadd = event->eventdat > 0 ? 1 : -1;
                    state->enemy_control_write_count += 4;
                }
                if (event->eventdat2 != -99) {
                    enemy->eycc = (int8_t)event->eventdat2;
                    enemy->eyccw = ot_abs_s8(event->eventdat2);
                    enemy->eyccwmax =
                        (uint8_t)ot_abs_s8(event->eventdat2);
                    enemy->eyccadd = event->eventdat2 > 0 ? 1 : -1;
                    state->enemy_control_write_count += 4;
                }
                if (event->eventdat5 > 0) {
                    enemy->enemycycle = (uint8_t)event->eventdat5;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat6 > 0) {
                    enemy->ani = (uint8_t)event->eventdat6;
                    enemy->animin = (uint8_t)event->eventdat5;
                    enemy->animax = 0;
                    enemy->aniactive = 1;
                    state->enemy_control_write_count += 4;
                }
            }
        }
        return true;

    case 21:
        state->background3_over = 1;
        return true;

    case 22:
        state->background3_over = 0;
        return true;

    case 23: { /* Sky Enemy on Bottom */
        uint8_t created =
            ot_create_new_event_enemy(state, event, 0, 50, 0);
        if (created > 0) {
            state->enemy[created - 1].ey =
                (int16_t)(180 + event->eventdat5);
        }
        return true;
    }

    case 26:
        state->small_enemy_adjust = event->eventdat != 0;
        return true;

    case 27: /* Enemy Global AccelRev */
        resolve_new_pl_link(state, event);
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (
                event->eventdat4 == 0 ||
                enemy->linknum == event->eventdat4
            ) {
                if (event->eventdat != -99) {
                    enemy->exrev = (int8_t)event->eventdat;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat2 != -99) {
                    enemy->eyrev = (int8_t)event->eventdat2;
                    state->enemy_control_write_count++;
                }
                if (event->eventdat3 != 0 && event->eventdat3 < 17) {
                    enemy->filter = (uint8_t)event->eventdat3;
                    state->enemy_control_write_count++;
                }
            }
        }
        return true;

    case 28:
        state->top_enemy_over = false;
        return true;

    case 29:
        state->top_enemy_over = true;
        return true;

    case 30:
        state->map1_y_delay = 1;
        state->map1_y_delay_max = 1;
        state->map2_y_delay = 1;
        state->map2_y_delay_max = 1;
        state->back_move = (uint16_t)event->eventdat;
        state->back_move2 = (uint16_t)event->eventdat2;
        state->explode_move = state->back_move2;
        state->back_move3 = (uint16_t)(int16_t)event->eventdat3;
        return true;

    case 31: /* Enemy Fire Override */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];
            uint8_t turret_index;

            if (
                event->eventdat4 == 99 ||
                enemy->linknum == event->eventdat4
            ) {
                enemy->freq[0] = (uint8_t)event->eventdat;
                enemy->freq[1] = (uint8_t)event->eventdat2;
                enemy->freq[2] = (uint8_t)event->eventdat3;
                state->enemy_control_write_count += 3;
                for (turret_index = 0; turret_index < 3; turret_index++) {
                    enemy->eshotwait[turret_index] = 1;
                    state->enemy_control_write_count++;
                }
                if (enemy->launchtype > 0) {
                    enemy->launchfreq = (uint8_t)event->eventdat5;
                    enemy->launchwait = 1;
                    state->enemy_control_write_count += 2;
                }
            }
        }
        return true;

    case 32: { /* create enemy */
        uint8_t created =
            ot_create_new_event_enemy(state, event, 0, 50, 0);
        if (created > 0) state->enemy[created - 1].ey = 190;
        return true;
    }

    case 33: { /* Enemy From other Enemies */
        uint16_t enemy_die = (uint16_t)event->eventdat;

        /*
         * Fixed scope: single-player Normal, SA_NONE, !superTyrian.
         * player[0].lives aliases the initial front-weapon power (1).
         */
        if (
            event->eventdat == 533 &&
            (ot_mt_rand(state) % 15u) < 1u
        ) {
            enemy_die = (uint16_t)(829u + ot_mt_rand(state) % 6u);
        }
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            if (state->enemy[index].linknum == event->eventdat4) {
                state->enemy[index].enemydie = enemy_die;
                state->enemy_control_write_count++;
            }
        }
        return true;
    }

    case 34:
        state->music_fade = true;
        return true;

    case 35:
        state->current_song = (int16_t)(event->eventdat - 1);
        state->music_fade = false;
        return true;

    case 36:
        state->ready_to_end_level = true;
        return true;

    case 37:
        state->level_enemy_frequency = (uint16_t)event->eventdat;
        return true;

    case 40:
        state->enemy_continual_damage = true;
        return true;

    case 42:
        state->background3_over = 2;
        return true;

    case 43:
        state->background2_over = (uint8_t)event->eventdat;
        return true;

    case 45: /* arcade-only enemy from other enemies */
        /*
         * The single-player, non-action mode performs no enemy assignment,
         * but event 533 still consumes/mutates RNG before that mode check.
         * First-level records use 602..628, so this branch is normally idle.
         */
        if (
            event->eventdat == 533 &&
            (ot_mt_rand(state) % 15u) < 1u
        ) {
            event->eventdat =
                (int16_t)(829u + ot_mt_rand(state) % 6u);
        }
        return true;

    case 48:
        state->background2_not_transparent = true;
        return true;

    case 53:
        state->force_events = event->eventdat != 99;
        return true;

    case 56: { /* Ground2 Bottom */
        uint8_t created =
            ot_create_new_event_enemy(state, event, 0, 75, 0);
        if (created > 0) state->enemy[created - 1].ey = 190;
        return true;
    }

    case 57:
        state->super_enemy_254_jump = (uint16_t)event->eventdat;
        return true;

    case 60: /* Assign Special Enemy */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            if (state->enemy[index].linknum == event->eventdat4) {
                state->enemy[index].special = true;
                state->enemy[index].flagnum = (uint8_t)event->eventdat;
                state->enemy[index].setto = event->eventdat2 == 1;
                state->enemy_control_write_count += 3;
            }
        }
        return true;

    case 61: { /* flag conditionally skips following events */
        int16_t flag_index = (int16_t)(event->eventdat - 1);

        if (flag_index < 0 || flag_index >= OT_GLOBAL_FLAG_COUNT) {
            state->assets_valid = false;
            return false;
        }
        if (
            state->global_flags[flag_index] ==
            (uint8_t)event->eventdat2
        ) {
            /*
             * First-level source records only contain positive skip counts.
             * A negative value would require the not-yet-used backward event
             * control-flow adapter and is deliberately rejected.
             */
            if (event->eventdat3 < 0) {
                state->assets_valid = false;
                return false;
            }
            *skip_events = (uint8_t)event->eventdat3;
        }
        return true;
    }

    case 65:
        state->background3_x1 = event->eventdat == 0;
        return true;

    case 68:
        state->random_explosions = event->eventdat == 1;
        return true;

    case 72:
        state->background3_x1b = event->eventdat == 1;
        return true;

    case 73:
        state->sky_enemy_over_all = event->eventdat == 1;
        return true;

    case 79:
        state->boss_bar_link[0] = (uint8_t)event->eventdat;
        state->boss_bar_link[1] = (uint8_t)event->eventdat2;
        return true;

    default:
        /*
         * Player/collision/render/audio cases remain owned by the legacy
         * runtime in stage 2.  Their exact records are counted as deferred;
         * no approximate source-parity behavior is introduced.
         */
        return false;
    }
}

static int16_t ot_round_ratio(
    int32_t numerator,
    int16_t denominator
)
{
    if (numerator < 0) {
        return (int16_t)(
            -((-numerator + denominator / 2) / denominator)
        );
    }
    return (int16_t)((numerator + denominator / 2) / denominator);
}

static void ot_release_enemy(
    OtLevelPortState *state,
    uint8_t enemy_index
)
{
    if (state->enemy_avail[enemy_index] == 1) return;
    state->enemy_avail[enemy_index] = 1;
    if (state->active_enemy_count > 0) state->active_enemy_count--;
    state->enemy_release_count++;
}

static void ot_advance_fixed_acceleration(
    int8_t *current_speed,
    int8_t *control,
    int8_t *wait,
    uint8_t wait_max,
    int8_t *reverse_speed,
    int16_t *add
)
{
    if (--(*wait) > 0) return;
    if (*current_speed == *reverse_speed) {
        *control = (int8_t)-*control;
        *reverse_speed = (int8_t)-*reverse_speed;
        *add = (int16_t)-*add;
        return;
    }

    *current_speed = (int8_t)(*current_speed + *add);
    *wait = (int8_t)wait_max;
    if (*current_speed == *reverse_speed) {
        *wait = (int8_t)-*wait;
        *reverse_speed = (int8_t)-*reverse_speed;
        *add = (int16_t)-*add;
    }
}

static void ot_enemy_fire_slots(
    OtLevelPortState *state,
    OtEnemy *enemy
)
{
    int8_t slot;

    for (slot = 2; slot >= 0; slot--) {
        uint16_t turret;

        if (enemy->freq[(uint8_t)slot] == 0) continue;
        turret = enemy->tur[(uint8_t)slot];
        enemy->eshotwait[(uint8_t)slot]--;
        if (
            enemy->eshotwait[(uint8_t)slot] != 0 ||
            turret == 0
        ) {
            continue;
        }
        enemy->eshotwait[(uint8_t)slot] =
            enemy->freq[(uint8_t)slot];

        /*
         * 251..255 are magnet/explosion control opcodes, not HDT weapons.
         * Their gameplay-side player forces/render effects are deferred, but
         * the source wait/animation state above is already authoritative.
         */
        if (turret >= 251) continue;
        {
            OtWeaponDefinition weapon;
            uint8_t projectile;

            if (!ot_data_hdt_weapon_read(turret, &weapon)) {
                state->assets_valid = false;
                continue;
            }
            for (projectile = 0; projectile < weapon.multi; projectile++) {
                if (weapon.sound > 0) {
                    uint32_t sound_slot;

                    do {
                        sound_slot = ot_mt_rand(state) % 8u;
                    } while (sound_slot == 3);
                }
                if (enemy->aniactive == 2) enemy->aniactive = 1;
                enemy->eshotmultipos[(uint8_t)slot]++;
                if (
                    enemy->eshotmultipos[(uint8_t)slot] >
                    weapon.max
                ) {
                    enemy->eshotmultipos[(uint8_t)slot] = 1;
                }
                state->enemy_shot_trigger_count++;
            }
        }
    }
}

static void ot_enemy_launch(
    OtLevelPortState *state,
    OtEnemy *parent,
    uint8_t parent_pool
)
{
    uint8_t launch_pool;
    uint8_t created;
    OtEnemy *child;
    OtEnemyDefinition child_definition;
    int16_t source_x;
    int16_t source_y;
    uint32_t sound_slot;

    if (parent->launchfreq == 0) return;
    parent->launchwait--;
    if (parent->launchwait != 0) return;
    parent->launchwait = parent->launchfreq;
    if (
        parent->launchspecial != 0 &&
        (
            parent->ey > state->player_y + 5 ||
            parent->ey < state->player_y - 5
        )
    ) {
        return;
    }
    if (parent->aniactive == 2) parent->aniactive = 1;
    if (parent->launchtype == 0) return;

    source_x = parent->ex;
    source_y = parent->ey;
    launch_pool =
        parent_pool == 25 || parent_pool == 75 ? 75 : parent_pool;
    created = ot_new_enemy(
        state,
        launch_pool,
        parent->launchtype,
        0,
        OT_SPAWN_LAUNCH
    );
    if (created == 0) return;
    child = &state->enemy[created - 1];
    if (!ot_data_hdt_enemy_read(child->enemytype, &child_definition)) {
        state->assets_valid = false;
        return;
    }

    child->ex = source_x;
    child->ey = (int16_t)(source_y + child_definition.startyc);
    if (child->size == 0) child->ey -= 7;
    if (child->launchtype > 0 && child->launchfreq == 0) {
        if (child->launchtype > 90) {
            uint16_t radius = (uint16_t)(child->launchtype - 90);
            child->ex = (int16_t)(
                child->ex +
                (int32_t)(ot_mt_rand(state) % (radius * 4u)) -
                radius * 2
            );
        } else {
            int16_t aim_x =
                (int16_t)(state->player_x - source_x - 4);
            int16_t aim_y =
                (int16_t)(state->player_y - source_y);
            int16_t magnitude;

            if (aim_x == 0) aim_x = 1;
            if (aim_y == 0) aim_y = 1;
            magnitude = ot_abs_s16(aim_x) > ot_abs_s16(aim_y) ?
                ot_abs_s16(aim_x) : ot_abs_s16(aim_y);
            child->exc = (int8_t)ot_round_ratio(
                (int32_t)aim_x * child->launchtype,
                magnitude
            );
            child->eyc = (int8_t)ot_round_ratio(
                (int32_t)aim_y * child->launchtype,
                magnitude
            );
        }
    }

    do {
        sound_slot = ot_mt_rand(state) % 8u;
    } while (sound_slot == 3);
    (void)(ot_mt_rand(state) % 3u);
    if (parent->launchspecial == 1 && parent->linknum < 100) {
        child->linknum = parent->linknum;
    }
}

/*
 * First translated JE_drawEnemy() slice.  It keeps the source's four
 * 25-entry pools, update order, animation, random/fixed acceleration,
 * bounce, off-screen release, fire cadence and launch lifecycle.  Blitting,
 * collision damage and the concrete 60-shot objects still remain in the
 * presentation runtime.
 */
static void ot_draw_enemy_pool(
    OtLevelPortState *state,
    uint8_t pool,
    int16_t temp_back_move
)
{
    uint8_t index;

    for (index = pool; index < pool + OT_ENEMY_POOL_SIZE; index++) {
        OtEnemy *enemy;

        if (state->enemy_avail[index] == 1) continue;
        enemy = &state->enemy[index];
        state->enemy_motion_update_count++;
        enemy->mapoffset = 0;

        if (
            enemy->xaccel != 0 &&
            (uint32_t)enemy->xaccel - 89u >
                ot_mt_rand(state) % 11u
        ) {
            int16_t player_x = (int16_t)(state->player_x - 25);

            if (player_x > enemy->ex) {
                if (enemy->exc < (int16_t)enemy->xaccel - 89) {
                    enemy->exc++;
                }
            } else if (
                enemy->exc >= 0 ||
                -enemy->exc < (int16_t)enemy->xaccel - 89
            ) {
                enemy->exc--;
            }
        }
        if (
            enemy->yaccel != 0 &&
            (uint32_t)enemy->yaccel - 89u >
                ot_mt_rand(state) % 11u
        ) {
            if (state->player_y > enemy->ey) {
                if (enemy->eyc < (int16_t)enemy->yaccel - 89) {
                    enemy->eyc++;
                }
            } else if (
                enemy->eyc >= 0 ||
                -enemy->eyc < (int16_t)enemy->yaccel - 89
            ) {
                enemy->eyc--;
            }
        }

        if (enemy->ex > -29 && enemy->ex < 300) {
            if (enemy->aniactive == 1) {
                enemy->enemycycle++;
                if (enemy->enemycycle == enemy->animax) {
                    enemy->aniactive = enemy->aniwhenfire;
                } else if (enemy->enemycycle > enemy->ani) {
                    enemy->enemycycle = enemy->animin;
                }
            }
            if (
                enemy->enemycycle == 0 ||
                enemy->enemycycle > 20
            ) {
                state->assets_valid = false;
                ot_release_enemy(state, index);
                continue;
            }
            if (enemy->egr[enemy->enemycycle - 1] == 999) {
                ot_release_enemy(state, index);
                continue;
            }
            enemy->filter = 0;
        }

        if (enemy->excc != 0) {
            ot_advance_fixed_acceleration(
                &enemy->exc,
                &enemy->excc,
                &enemy->exccw,
                enemy->exccwmax,
                &enemy->exrev,
                &enemy->exccadd
            );
        }
        if (enemy->eycc != 0) {
            ot_advance_fixed_acceleration(
                &enemy->eyc,
                &enemy->eycc,
                &enemy->eyccw,
                enemy->eyccwmax,
                &enemy->eyrev,
                &enemy->eyccadd
            );
        }

        enemy->ey = (int16_t)(enemy->ey + enemy->fixedmovey);
        enemy->ex = (int16_t)(enemy->ex + enemy->exc);
        if (enemy->ex < -80 || enemy->ex > 340) {
            ot_release_enemy(state, index);
            continue;
        }
        enemy->ey = (int16_t)(enemy->ey + enemy->eyc);
        if (enemy->ey < -112 || enemy->ey > 190) {
            ot_release_enemy(state, index);
            continue;
        }

        if (
            enemy->ex <= enemy->xminbounce ||
            enemy->ex >= enemy->xmaxbounce
        ) {
            enemy->exc = (int8_t)-enemy->exc;
        }
        if (
            enemy->ey <= enemy->yminbounce ||
            enemy->ey >= enemy->ymaxbounce
        ) {
            enemy->eyc = (int8_t)-enemy->eyc;
        }
        if (enemy->scoreitem) {
            if (enemy->ex < -5) enemy->ex++;
            if (enemy->ex > 245) enemy->ex--;
        }
        enemy->ey = (int16_t)(enemy->ey + temp_back_move);
        if (enemy->ex <= -24 || enemy->ex >= 296) continue;
        if (enemy->edamaged) continue;
        if (enemy->iced != 0) {
            enemy->iced--;
            if (enemy->enemyground) enemy->filter = 0x09;
            continue;
        }

        ot_enemy_fire_slots(state, enemy);
        ot_enemy_launch(state, enemy, pool);
    }
}

static void ot_spawn_continual_enemy(OtLevelPortState *state)
{
    uint16_t enemy_id;
    uint16_t enemy_index;

    if (!state->enemies_active) return;
    if (
        ot_mt_rand(state) % 100u <= state->level_enemy_frequency
    ) {
        return;
    }
    if (ot_data_catalog()->level1_enemy_count == 0) {
        state->assets_valid = false;
        return;
    }
    enemy_index = (uint16_t)(
        ot_mt_rand(state) % ot_data_catalog()->level1_enemy_count
    );
    if (!ot_data_level1_enemy_pool_read(enemy_index, &enemy_id)) {
        state->assets_valid = false;
        return;
    }
    ot_new_enemy(state, 0, enemy_id, 0, OT_SPAWN_RANDOM);
}

static void ot_advance_enemies(OtLevelPortState *state)
{
    /* JE_main() draw/update order: ground, ground2, continual, sky, top. */
    ot_draw_enemy_pool(state, 25, (int16_t)state->back_move);
    ot_draw_enemy_pool(state, 75, (int16_t)state->back_move);
    ot_spawn_continual_enemy(state);
    ot_draw_enemy_pool(state, 0, 0);
    ot_draw_enemy_pool(state, 50, (int16_t)state->back_move3);
}

void ot_level_port_advance(
    OtLevelPortState *state,
    uint16_t cur_loc,
    int16_t player_x,
    int16_t player_y
)
{
    OtEventRecord event;

    state->cur_loc = cur_loc;
    state->player_x = player_x;
    state->player_y = player_y;
    while (
        state->event_index < OT_LEVEL1_EXPECTED_EVENT_COUNT &&
        ot_level1_event_read(state->event_index, &event) &&
        event.eventtime <= state->cur_loc
    ) {
        uint16_t skip_events;
        uint16_t remaining;

        if (apply_event(state, &event, &skip_events)) {
            state->applied_event_count++;
        } else {
            state->deferred_event_count++;
        }
        state->event_index++;

        remaining =
            (uint16_t)(
                OT_LEVEL1_EXPECTED_EVENT_COUNT - state->event_index
            );
        if (skip_events > remaining) skip_events = remaining;
        state->event_index = (uint16_t)(state->event_index + skip_events);
        state->skipped_event_count += skip_events;
    }
    ot_advance_enemies(state);
}
