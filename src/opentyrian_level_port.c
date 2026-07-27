/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct translation staging area for OpenTyrian's selected-level game loop.
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
    OT_LEVEL_EVENT_RECORD_BYTES == 11,
    "OpenTyrian event record width changed"
);
_Static_assert(
    OT_HDT_ENEMY_RECORD_BYTES == 77,
    "OpenTyrian enemy record width changed"
);
_Static_assert(OT_ENEMY_COUNT == 100, "OpenTyrian enemy pool size changed");
_Static_assert(OT_ENEMY_POOL_SIZE == 25, "OpenTyrian pool group size changed");

bool ot_level_event_read(uint16_t index, OtEventRecord *event)
{
    return ot_data_level_event_read(index, event);
}

bool ot_level_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy)
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

uint32_t ot_level_port_random(OtLevelPortState *state)
{
    /*
     * JE_playerMovement shares the process-wide MT stream with event and
     * enemy logic.  Expose that exact translated stream to the surrounding
     * GBA player adapter instead of introducing a second death-only RNG.
     */
    return ot_mt_rand(state);
}

static int8_t ot_abs_s8(int16_t value)
{
    return (int8_t)(value < 0 ? -value : value);
}

static int16_t ot_abs_s16(int16_t value)
{
    return (int16_t)(value < 0 ? -value : value);
}

enum {
    OT_ENEMY_SHAPE_SLOT_FIXED_21 = 0xfd,
    OT_ENEMY_SHAPE_SLOT_FIXED_26 = 0xfe,
    OT_ENEMY_SHAPE_SLOT_NULL = 0xff,
};

static uint8_t shape_table_slot(
    const OtLevelPortState *state,
    uint8_t shape_table
)
{
    uint8_t index;

    if (shape_table == 21) return OT_ENEMY_SHAPE_SLOT_FIXED_21;
    if (shape_table == 26) return OT_ENEMY_SHAPE_SLOT_FIXED_26;
    for (index = 0; index < 4; index++) {
        if (state->shape_bank[index] == shape_table) return index;
    }
    return OT_ENEMY_SHAPE_SLOT_NULL;
}

static uint8_t ot_enemy_resolved_shape_table(
    const OtLevelPortState *state,
    const OtEnemy *enemy
)
{
    if (enemy->shape_slot < 4) {
        return state->shape_bank[enemy->shape_slot];
    }
    if (enemy->shape_slot == OT_ENEMY_SHAPE_SLOT_FIXED_21) return 21;
    if (enemy->shape_slot == OT_ENEMY_SHAPE_SLOT_FIXED_26) return 26;
    return 0;
}

/* Direct single-player translation of JE_makeEnemy(). */
static bool ot_make_enemy(
    OtLevelPortState *state,
    OtEnemy *enemy,
    uint16_t enemy_definition_id,
    int16_t unique_shape_table,
    const OtEnemyDefinition *definition_override,
    uint8_t *avail
)
{
    OtEnemyDefinition definition;
    uint8_t shape_table;
    uint8_t shape_slot;
    uint8_t index;

    if (definition_override != 0) {
        definition = *definition_override;
    } else if (!ot_level_enemy_read(enemy_definition_id, &definition)) {
        return false;
    }

    if (unique_shape_table > 0) {
        shape_table = (uint8_t)unique_shape_table;
    } else {
        shape_table = definition.shapebank;
    }

    /*
     * Direct JE_makeEnemy() pointer semantics.  Dynamic bank IDs resolve to
     * one of enemySpriteSheets[4].  An unavailable bank leaves that pointer
     * untouched (APPROACH); a later event-5 reload of the same slot is then
     * observed automatically by every enemy retaining the slot identity.
     */
    shape_slot = shape_table_slot(state, shape_table);
    if (shape_slot != OT_ENEMY_SHAPE_SLOT_NULL) {
        enemy->shape_slot = shape_slot;
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

    if (definition.value > 1 && definition.value < 10000) {
        if (state->difficulty_level == 3) {
            enemy->evalue = (int16_t)(
                (int32_t)definition.value * 9 / 8
            );
        } else {
            /* Easy and Normal share the source identity branch. */
            enemy->evalue = definition.value;
        }
        if (enemy->evalue > 10000) enemy->evalue = 10000;
    } else {
        enemy->evalue = definition.value;
    }

    if (definition.armor > 0) {
        uint16_t armor = definition.armor;

        if (definition.armor != 255) {
            if (state->difficulty_level == 1) {
                armor = (uint16_t)(definition.armor * 3u / 4u + 1u);
            } else if (state->difficulty_level == 3) {
                armor = (uint16_t)(definition.armor * 6u / 5u);
            }
            if (armor > 254) armor = 254;
        }
        enemy->armorleft = (uint8_t)armor;
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
    OT_SPAWN_DEATH,
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
    case OT_SPAWN_DEATH:
        state->death_spawn_attempt_count++;
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
    case OT_SPAWN_DEATH:
        state->death_spawn_pool_full_count++;
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
    case OT_SPAWN_DEATH:
        state->death_spawn_missing_definition_count++;
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
    case OT_SPAWN_DEATH:
        state->death_spawn_success_count++;
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
);

static uint8_t ot_new_enemy_with_definition(
    OtLevelPortState *state,
    uint8_t enemy_offset,
    uint16_t enemy_definition_id,
    int16_t unique_shape_table,
    OtSpawnOrigin origin,
    const OtEnemyDefinition *definition_override
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
        definition_override,
        &avail
    )) {
        ot_record_spawn_missing(state, origin);
        state->assets_valid = false;
        return 0;
    }

    state->enemy_avail[slot] = avail;
    if (avail == 2 && state->enemy[slot].scoreitem) {
        state->score_item_spawn_count++;
        state->score_item_active_count++;
        if (
            state->score_item_active_count >
            state->score_item_max_active_count
        ) {
            state->score_item_max_active_count =
                state->score_item_active_count;
        }
    }
    ot_record_spawn_success(state, origin);
    state->active_enemy_count++;
    if (state->active_enemy_count > state->max_active_enemy_count) {
        state->max_active_enemy_count = state->active_enemy_count;
    }
    state->last_created_slot = (uint8_t)(slot + 1);
    return state->last_created_slot;
}

static uint8_t ot_new_enemy(
    OtLevelPortState *state,
    uint8_t enemy_offset,
    uint16_t enemy_definition_id,
    int16_t unique_shape_table,
    OtSpawnOrigin origin
)
{
    return ot_new_enemy_with_definition(
        state,
        enemy_offset,
        enemy_definition_id,
        unique_shape_table,
        origin,
        0
    );
}

static uint8_t ot_create_new_event_enemy_with_definition(
    OtLevelPortState *state,
    OtEventRecord *event,
    uint8_t enemy_type_offset,
    uint8_t enemy_offset,
    int16_t unique_shape_table,
    const OtEnemyDefinition *definition_override
)
{
    uint8_t created;
    uint16_t enemy_definition_id;
    OtEnemy *enemy;

    enemy_definition_id =
        (uint16_t)(event->eventdat + enemy_type_offset);
    created = ot_new_enemy_with_definition(
        state,
        enemy_offset,
        enemy_definition_id,
        unique_shape_table,
        OT_SPAWN_EVENT,
        definition_override
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

static uint8_t ot_create_new_event_enemy(
    OtLevelPortState *state,
    OtEventRecord *event,
    uint8_t enemy_type_offset,
    uint8_t enemy_offset,
    int16_t unique_shape_table
)
{
    return ot_create_new_event_enemy_with_definition(
        state,
        event,
        enemy_type_offset,
        enemy_offset,
        unique_shape_table,
        0
    );
}

void ot_level_port_init(
    OtLevelPortState *state,
    uint8_t difficulty_level,
    bool arcade_mode,
    bool preserve_shape_history
)
{
    const OtDataCatalog *data_catalog;
    OtEnemyDefinition first_spawn_definition;
    OtLevelInfo level_info;
    OtDataView data_view;
    uint16_t first_spawn_id;
    uint8_t inherited_shape_bank[4];
    uint8_t inherited_shape_slot[OT_ENEMY_COUNT];
    uint8_t index;

    if (preserve_shape_history) {
        for (index = 0; index < 4; index++) {
            inherited_shape_bank[index] = state->shape_bank[index];
        }
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            inherited_shape_slot[index] = state->enemy[index].shape_slot;
        }
    }
    *state = (OtLevelPortState){0};
    if (preserve_shape_history) {
        for (index = 0; index < 4; index++) {
            state->shape_bank[index] = inherited_shape_bank[index];
        }
    }

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
    state->difficulty_level =
        difficulty_level >= 1 && difficulty_level <= 3 ?
            difficulty_level :
            2;
    state->initial_difficulty = state->difficulty_level;
    state->damage_rate = 2;
    state->arcade_mode = arcade_mode;
    state->map1_pointer_offset =
        (
            OT_LEVEL_MAP1_ROWS -
            OT_LEVEL_INITIAL_BOTTOM_MARGIN_ROWS
        ) * OT_LEVEL_MAP1_COLUMNS - 1;
    state->map2_pointer_offset =
        (
            OT_LEVEL_MAP2_ROWS -
            OT_LEVEL_INITIAL_BOTTOM_MARGIN_ROWS
        ) * OT_LEVEL_MAP2_COLUMNS - 1;
    state->background2_wrap_offset = OT_LEVEL_MAP2_COLUMNS;
    state->background2_wrap_to_offset = OT_LEVEL_MAP2_COLUMNS;
    state->map_x = 1;
    state->map_x3 = 1;
    /* JE_initPlayerData(): USP Talon, Pulse-Cannon, no rear weapon. */
    state->player_front_weapon_id = 1;
    state->player_front_weapon_power = 1;
    state->player_rear_weapon_id = 0;
    state->player_rear_weapon_power = 1;
    state->player_armor = 10;
    state->player_weapon_mode = 1;
    state->player_purple_balls_needed = 1;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        state->enemy_avail[index] = 1;
        state->enemy[index].shape_slot =
            preserve_shape_history ?
                inherited_shape_slot[index] :
                OT_ENEMY_SHAPE_SLOT_NULL;
    }
    ot_mt_seed(&state->rng, OT_SOURCE_PARITY_TEST_SEED);

    state->assets_valid =
        ot_data_init() &&
        (data_catalog = ot_data_catalog()) != 0 &&
        data_catalog->initialized &&
        ot_data_level_info(&level_info) &&
        level_info.event_count > 0 &&
        level_info.enemy_count > 0 &&
        ot_data_level_enemy_pool_read(0, &first_spawn_id) &&
        ot_level_enemy_read(first_spawn_id, &first_spawn_definition) &&
        ot_data_level_map_view(0, &data_view) &&
        ot_data_level_map_view(1, &data_view) &&
        ot_data_level_map_view(2, &data_view) &&
        ot_data_background_shape_file_view(
            level_info.shape_file,
            &data_view
        );
    state->event_count =
        state->assets_valid ? level_info.event_count : 0;
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

/*
 * Direct zero-based adapter for JE_eventJump().  OpenTyrian assigns
 * eventLoc to one slot before the first event at or after curLoc because
 * JE_eventSystem() performs its unconditional eventLoc++ on return.  This
 * port owns the next zero-based record directly, so no compensating
 * decrement/increment is needed.
 */
static bool ot_event_jump(OtLevelPortState *state, uint16_t jump)
{
    OtEventRecord event;
    uint16_t index;
    uint16_t destination;

    if (jump == UINT16_MAX) {
        destination = state->return_loc;
    } else {
        state->return_loc = (uint16_t)(state->cur_loc + 1u);
        destination = jump;
    }
    state->cur_loc = destination;
    for (index = 0; index < state->event_count; index++) {
        if (!ot_level_event_read(index, &event)) {
            state->assets_valid = false;
            return false;
        }
        if (event.eventtime >= destination) {
            state->event_index = index;
            return true;
        }
    }
    state->event_index = state->event_count;
    return true;
}

static bool ot_search_for_enemy(
    const OtLevelPortState *state,
    uint8_t link_number,
    uint8_t *found_index
)
{
    int16_t found = -1;
    uint8_t index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (
            state->enemy_avail[index] == 0 &&
            state->enemy[index].linknum == link_number
        ) {
            found = index;
        }
    }
    if (found < 0) return false;
    if (found_index != 0) *found_index = (uint8_t)found;
    return true;
}

static void ot_clear_enemy_range(
    OtLevelPortState *state,
    uint8_t first,
    uint8_t count
)
{
    uint8_t index;

    for (index = first; index < (uint8_t)(first + count); index++) {
        if (state->enemy_avail[index] == 1) continue;
        if (
            state->enemy[index].scoreitem &&
            state->score_item_active_count > 0
        ) {
            state->score_item_active_count--;
        }
        state->enemy_avail[index] = 1;
        state->enemy_draw[index].active = false;
        if (state->active_enemy_count > 0) {
            state->active_enemy_count--;
        }
    }
}

static bool apply_event(
    OtLevelPortState *state,
    OtEventRecord *event,
    int16_t *skip_events,
    bool *jumped
)
{
    uint8_t index;

    *skip_events = 0;
    *jumped = false;
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

    case 4:
        state->stop_backgrounds = true;
        switch (event->eventdat) {
        case 0:
        case 1:
            state->stop_background_num = 1;
            break;
        case 2:
            state->stop_background_num = 2;
            break;
        case 3:
            state->stop_background_num = 3;
            break;
        default:
            break;
        }
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

    case 11:
        if (event->eventdat == 1) {
            state->really_end_level = true;
        } else if (!state->end_level) {
            state->ready_to_end_level = false;
            state->end_level = true;
            state->level_end = 40;
        }
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

    case 16: {
        static const uint8_t window_text_samples[9] = {
            35, 31, 32, 30, 35, 36, 38, 35, 32,
        };

        /*
         * JE_drawTextWindow() is a presentation call.  Retain the exact
         * source output index and voice queue request so the GBA frontend
         * can present it without changing event/gameplay control flow.
         */
        if (event->eventdat >= 1 && event->eventdat <= 9) {
            state->pending_text_window = (uint8_t)event->eventdat;
            state->frame_sound_queue[3] =
                window_text_samples[event->eventdat - 1];
        }
        return true;
    }

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

    case 24: /* Enemy Global Animate */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (enemy->linknum != event->eventdat4) continue;
            enemy->aniactive = 1;
            enemy->aniwhenfire = 0;
            if (event->eventdat2 > 0) {
                enemy->enemycycle = (uint8_t)event->eventdat2;
                enemy->animin = enemy->enemycycle;
            } else {
                enemy->enemycycle = 0;
            }
            if (event->eventdat > 0) {
                enemy->ani = (uint8_t)event->eventdat;
            }
            if (event->eventdat3 == 1) {
                enemy->animax = enemy->ani;
            } else if (event->eventdat3 == 2) {
                enemy->aniactive = 2;
                enemy->animax = enemy->ani;
                enemy->aniwhenfire = 2;
            }
            state->enemy_control_write_count += 4;
        }
        return true;

    case 25: /* Enemy Global Damage change */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            if (
                event->eventdat4 == 0 ||
                state->enemy[index].linknum == event->eventdat4
            ) {
                /*
                 * galagaMode is false in both supported one-player modes,
                 * matching the ordinary OpenTyrian branch.
                 */
                state->enemy[index].armorleft =
                    (uint8_t)event->eventdat;
                state->enemy_control_write_count++;
            }
        }
        return true;

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

        state->death_control_event_count++;
        if (
            state->arcade_mode &&
            (event->eventdat == 512 || event->eventdat == 513)
        ) {
            return true;
        }
        /* SA_NONE, !superTyrian, one life in both supported modes. */
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
                state->death_assignment_count++;
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

    case 38: {
        /*
         * Preserve the unusual original event-38 search exactly.  The
         * source stores the zero-based last record at/before curLoc in its
         * one-based eventLoc and then performs the common eventLoc++.
         */
        uint16_t next_index = 1;
        OtEventRecord candidate;

        state->cur_loc = (uint16_t)event->eventdat;
        for (index = 0; index < state->event_count; index++) {
            if (!ot_level_event_read(index, &candidate)) {
                state->assets_valid = false;
                return false;
            }
            if (candidate.eventtime <= state->cur_loc) {
                next_index = index;
            }
        }
        if (next_index > state->event_count) {
            next_index = state->event_count;
        }
        state->event_index = next_index;
        *jumped = true;
        return true;
    }

    case 39: /* Enemy Global Linknum Change */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            if (state->enemy[index].linknum == (uint8_t)event->eventdat) {
                state->enemy[index].linknum =
                    (uint8_t)event->eventdat2;
                state->enemy_control_write_count++;
            }
        }
        return true;

    case 40:
        state->enemy_continual_damage = true;
        return true;

    case 41:
        if (event->eventdat == 0) {
            ot_clear_enemy_range(state, 0, OT_ENEMY_COUNT);
        } else {
            ot_clear_enemy_range(state, 0, OT_ENEMY_POOL_SIZE);
        }
        return true;

    case 42:
        state->background3_over = 2;
        return true;

    case 43:
        state->background2_over = (uint8_t)event->eventdat;
        return true;

    case 44:
        state->filter_active = event->eventdat > 0;
        state->filter_fade = event->eventdat == 2;
        state->level_filter = (int8_t)event->eventdat2;
        state->level_brightness = event->eventdat3;
        state->level_filter_new = (int8_t)event->eventdat4;
        state->level_brightness_change = event->eventdat5;
        state->filter_fade_start = event->eventdat6 == 0;
        return true;

    case 45: { /* arcade-only enemy from other enemies */
        uint16_t enemy_die = (uint16_t)event->eventdat;

        if (
            event->eventdat == 533 &&
            (ot_mt_rand(state) % 15u) < 1u
        ) {
            enemy_die =
                (uint16_t)(829u + ot_mt_rand(state) % 6u);
        }
        if (state->arcade_mode) {
            for (index = 0; index < OT_ENEMY_COUNT; index++) {
                if (
                    state->enemy[index].linknum ==
                    event->eventdat4
                ) {
                    state->enemy[index].enemydie = enemy_die;
                    state->enemy_control_write_count++;
                    state->death_assignment_count++;
                }
            }
        }
        return true;
    }

    case 46: { /* change difficulty */
        int16_t difficulty;

        if (event->eventdat3 != 0) {
            state->damage_rate = (uint8_t)event->eventdat3;
        }
        if (event->eventdat2 == 0 || state->arcade_mode) {
            difficulty =
                (int16_t)state->difficulty_level + event->eventdat;
            if (difficulty < 1) difficulty = 1;
            if (difficulty > 10) difficulty = 10;
            state->difficulty_level = (uint8_t)difficulty;
        }
        return true;
    }

    case 47: /* Enemy Global armor change */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            if (
                event->eventdat4 == 0 ||
                state->enemy[index].linknum == event->eventdat4
            ) {
                state->enemy[index].armorleft =
                    (uint8_t)event->eventdat;
                state->enemy_control_write_count++;
            }
        }
        return true;

    case 48:
        state->background2_not_transparent = true;
        return true;

    case 49:
    case 50:
    case 51:
    case 52: {
        OtEnemyDefinition custom_definition;
        OtEventRecord custom_event = *event;
        int16_t unique_shape_table = event->eventdat3;
        uint8_t enemy_offset;

        if (!ot_level_enemy_read(0, &custom_definition)) {
            state->assets_valid = false;
            return false;
        }
        custom_definition.armor = (uint8_t)event->eventdat6;
        custom_definition.egraphic[0] = (uint16_t)event->eventdat;
        custom_event.eventdat = 0;
        custom_event.eventdat3 = 0;
        custom_event.eventdat6 = 0;
        switch (event->eventtype - 48u) {
        case 1:
            enemy_offset = 25;
            break;
        case 2:
            enemy_offset = 0;
            break;
        case 3:
            enemy_offset = 50;
            break;
        default:
            enemy_offset = 75;
            break;
        }
        ot_create_new_event_enemy_with_definition(
            state,
            &custom_event,
            0,
            enemy_offset,
            unique_shape_table,
            &custom_definition
        );
        return true;
    }

    case 53:
        state->force_events = event->eventdat != 99;
        return true;

    case 54:
        *jumped = true;
        return ot_event_jump(state, (uint16_t)event->eventdat);

    case 55: /* Enemy Global player-seeking acceleration */
        resolve_new_pl_link(state, event);
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (
                event->eventdat4 != 0 &&
                enemy->linknum != event->eventdat4
            ) {
                continue;
            }
            if (event->eventdat != -99) {
                enemy->xaccel = (uint8_t)event->eventdat;
                state->enemy_control_write_count++;
            }
            if (event->eventdat2 != -99) {
                enemy->yaccel = (uint8_t)event->eventdat2;
                state->enemy_control_write_count++;
            }
        }
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
            *skip_events = event->eventdat3;
        }
        return true;
    }

    case 62: /* Play sound effect in source soundQueue[3]. */
        state->frame_sound_queue[3] = (uint8_t)event->eventdat;
        return true;

    case 63: /* skip events outside two-player/one-player-action */
        if (!state->arcade_mode) {
            *skip_events = event->eventdat;
        }
        return true;

    case 64: {
        int16_t smoothie = event->eventdat;
        int16_t data_index;

        /*
         * The GBA port has no two-player exception.  Stock files address
         * only the source's one-based nine-entry arrays.
         */
        if (smoothie < 1 || smoothie > 9) return true;
        state->smoothies[smoothie - 1] =
            event->eventdat2 != 0;
        data_index = smoothie == 5 ? 3 : smoothie;
        state->smoothie_data[data_index - 1] =
            (uint8_t)event->eventdat3;
        return true;
    }

    case 65:
        state->background3_x1 = event->eventdat == 0;
        return true;

    case 66: /* difficulty-gated event skip */
        if (state->initial_difficulty <= event->eventdat) {
            *skip_events = event->eventdat2;
        }
        return true;

    case 67:
        state->level_timer = event->eventdat == 1;
        state->level_timer_countdown =
            (uint16_t)((int16_t)event->eventdat3 * 100);
        state->level_timer_jump_to = (uint16_t)event->eventdat2;
        return true;

    case 68:
        state->random_explosions = event->eventdat == 1;
        return true;

    case 69:
        state->frame_player_invulnerable_ticks =
            (uint16_t)event->eventdat;
        return true;

    case 70: {
        bool should_jump;

        if (event->eventdat2 == 0) {
            uint8_t link;

            should_jump = true;
            for (link = 1; link <= 19; link++) {
                if (ot_search_for_enemy(state, link, 0)) {
                    should_jump = false;
                    break;
                }
            }
        } else {
            should_jump =
                !ot_search_for_enemy(
                    state,
                    (uint8_t)event->eventdat2,
                    0
                ) &&
                (
                    event->eventdat3 == 0 ||
                    !ot_search_for_enemy(
                        state,
                        (uint8_t)event->eventdat3,
                        0
                    )
                ) &&
                (
                    event->eventdat4 == 0 ||
                    !ot_search_for_enemy(
                        state,
                        event->eventdat4,
                        0
                    )
                );
        }
        if (should_jump) {
            *jumped = true;
            return ot_event_jump(
                state,
                (uint16_t)event->eventdat
            );
        }
        return true;
    }

    case 71:
        if (
            (uint32_t)state->map1_pointer_offset * 2u <=
            (uint32_t)(int32_t)event->eventdat2
        ) {
            *jumped = true;
            return ot_event_jump(
                state,
                (uint16_t)event->eventdat
            );
        }
        return true;

    case 72:
        state->background3_x1b = event->eventdat == 1;
        return true;

    case 73:
        state->sky_enemy_over_all = event->eventdat == 1;
        return true;

    case 74: /* Enemy Global BounceParams */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &state->enemy[index];

            if (
                event->eventdat4 != 0 &&
                enemy->linknum != event->eventdat4
            ) {
                continue;
            }
            if (event->eventdat5 != -99) {
                enemy->xminbounce = event->eventdat5;
                state->enemy_control_write_count++;
            }
            if (event->eventdat6 != -99) {
                enemy->yminbounce = event->eventdat6;
                state->enemy_control_write_count++;
            }
            if (event->eventdat != -99) {
                enemy->xmaxbounce = event->eventdat;
                state->enemy_control_write_count++;
            }
            if (event->eventdat2 != -99) {
                enemy->ymaxbounce = event->eventdat2;
                state->enemy_control_write_count++;
            }
        }
        return true;

    case 75: {
        bool stationary_enemy_found = false;
        int16_t first_link = event->eventdat;
        int16_t last_link = event->eventdat2;
        int16_t new_pl_index = (int16_t)event->eventdat3 - 80;

        if (
            new_pl_index < 0 ||
            new_pl_index >= OT_NEW_PL_COUNT ||
            first_link > last_link
        ) {
            state->assets_valid = false;
            return false;
        }
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            const OtEnemy *enemy = &state->enemy[index];

            if (
                state->enemy_avail[index] == 0 &&
                enemy->eyc == 0 &&
                enemy->linknum >= first_link &&
                enemy->linknum <= last_link
            ) {
                stationary_enemy_found = true;
                break;
            }
        }
        if (stationary_enemy_found) {
            uint16_t range =
                (uint16_t)(last_link + 1 - first_link);
            uint8_t selected_link;
            uint8_t selected_enemy;

            do {
                selected_link = (uint8_t)(
                    ot_mt_rand(state) % range + first_link
                );
            } while (
                !ot_search_for_enemy(
                    state,
                    selected_link,
                    &selected_enemy
                ) ||
                state->enemy[selected_enemy].eyc != 0
            );
            state->new_pl[new_pl_index] = selected_link;
        } else {
            state->new_pl[new_pl_index] = 255;
            if (event->eventdat4 > 0) {
                uint32_t target =
                    (uint32_t)state->event_index +
                    event->eventdat4;
                OtEventRecord target_event;

                if (target >= state->event_count) {
                    target = state->event_count;
                } else if (
                    ot_level_event_read(
                        (uint16_t)target,
                        &target_event
                    )
                ) {
                    state->cur_loc =
                        (uint16_t)(target_event.eventtime - 1u);
                } else {
                    state->assets_valid = false;
                    return false;
                }
                state->event_index = (uint16_t)target;
                *jumped = true;
                state->skipped_event_count +=
                    event->eventdat4 > 0 ?
                        event->eventdat4 - 1u :
                        0u;
            }
        }
        return true;
    }

    case 76:
        state->return_active = true;
        return true;

    case 77:
        state->map1_pointer_offset =
            (uint16_t)(event->eventdat / 2);
        state->map2_pointer_offset = (uint16_t)(
            (event->eventdat2 > 0 ?
                event->eventdat2 :
                event->eventdat) / 2
        );
        state->map_position_override_pending = true;
        return true;

    case 78:
        if (state->galaga_shot_frequency < 10) {
            state->galaga_shot_frequency++;
        }
        return true;

    case 79:
        state->boss_bar_link[0] = (uint8_t)event->eventdat;
        state->boss_bar_link[1] = (uint8_t)event->eventdat2;
        return true;

    case 80:
        /* GBA has no two-player mode, so the source branch is a no-op. */
        return true;

    case 81:
        state->background2_wrap_offset =
            (uint16_t)(event->eventdat / 2);
        state->background2_wrap_to_offset =
            (uint16_t)(event->eventdat2 / 2);
        state->background2_wrap_pending = true;
        return true;

    case 82:
        state->player_special = (uint8_t)event->eventdat;
        return true;

    default:
        /*
         * OpenTyrian warns and ignores unknown records.  Stock LVL data has
         * a few such values in special/bonus sections; count them as
         * deferred without invalidating an otherwise valid ROMFS section.
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
    if (
        state->enemy[enemy_index].scoreitem &&
        state->score_item_active_count > 0
    ) {
        state->score_item_active_count--;
    }
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

static OtEnemyShot *ot_allocate_enemy_shot(
    OtLevelPortState *state
)
{
    uint8_t index;

    for (index = 0; index < OT_ENEMY_SHOT_COUNT; index++) {
        if (!state->enemy_shot[index].active) {
            OtEnemyShot *shot = &state->enemy_shot[index];

            *shot = (OtEnemyShot){0};
            shot->active = true;
            state->enemy_shot_spawn_count++;
            state->enemy_shot_active_count++;
            if (
                state->enemy_shot_active_count >
                state->enemy_shot_max_active_count
            ) {
                state->enemy_shot_max_active_count =
                    state->enemy_shot_active_count;
            }
            return shot;
        }
    }
    state->enemy_shot_drop_count++;
    return 0;
}

static void ot_release_enemy_shot(
    OtLevelPortState *state,
    OtEnemyShot *shot
)
{
    if (!shot->active) return;
    shot->active = false;
    if (state->enemy_shot_active_count > 0) {
        state->enemy_shot_active_count--;
    }
    state->enemy_shot_release_count++;
}

/*
 * Direct single-player/Normal translation of JE_drawEnemy()'s three firing
 * slots.  Returning false preserves the source's "goto draw_enemy_end" when
 * the 60-entry projectile pool is full, so that enemy also skips its launch
 * routine for this frame.
 */
static bool ot_enemy_fire_slots(
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
                OtEnemyShot *shot;
                uint8_t position;
                int16_t aim_x;
                int16_t aim_y;
                int16_t magnitude;

                shot = ot_allocate_enemy_shot(state);
                if (shot == 0) return false;
                if (weapon.sound > 0) {
                    uint32_t sound_slot;

                    do {
                        sound_slot = ot_mt_rand(state) % 8u;
                    } while (sound_slot == 3);
                    if (weapon.sound < 16) {
                        state->frame_sound_mask |=
                            (uint16_t)(1u << weapon.sound);
                    }
                }
                if (enemy->aniactive == 2) enemy->aniactive = 1;
                enemy->eshotmultipos[(uint8_t)slot]++;
                if (
                    enemy->eshotmultipos[(uint8_t)slot] >
                    weapon.max
                ) {
                    enemy->eshotmultipos[(uint8_t)slot] = 1;
                }
                position = (uint8_t)(
                    enemy->eshotmultipos[(uint8_t)slot] - 1
                );
                if (position >= 8) {
                    state->assets_valid = false;
                    ot_release_enemy_shot(state, shot);
                    continue;
                }

                shot->sx = (int16_t)(
                    enemy->ex + weapon.bx[position] + enemy->mapoffset
                );
                shot->sy = (int16_t)(enemy->ey + weapon.by[position]);
                shot->sdmg = weapon.attack[position];
                shot->tx = weapon.tx;
                shot->ty = weapon.ty;
                shot->duration = weapon.delay[position];
                shot->animate = 0;
                shot->animax = weapon.weapani;
                shot->sgr = weapon.sg[position];

                switch (slot) {
                case 0:
                    shot->syc = weapon.acceleration;
                    shot->sxc = weapon.accelerationx;
                    shot->sxm = weapon.sx[position];
                    shot->sym = weapon.sy[position];
                    break;
                case 2:
                    shot->sxc = (int8_t)-weapon.acceleration;
                    shot->syc = weapon.accelerationx;
                    shot->sxm = (int16_t)-weapon.sy[position];
                    shot->sym = (int16_t)-weapon.sx[position];
                    break;
                default:
                    /*
                     * Keep OpenTyrian's slot-2 assignment exactly, including
                     * its use of acceleration for both axes.
                     */
                    shot->sxc = weapon.acceleration;
                    shot->syc = (int8_t)-weapon.acceleration;
                    shot->sxm = weapon.sy[position];
                    shot->sym = (int16_t)-weapon.sx[position];
                    break;
                }

                if (weapon.aim > 0) {
                    /*
                     * JE_drawEnemy temporarily subtracts 25 from player.x;
                     * its later +25 therefore cancels.  state->player_x is
                     * kept unmodified, leaving the source expression below.
                     */
                    aim_x = (int16_t)(
                        state->player_x - enemy->ex -
                        enemy->mapoffset - 4
                    );
                    aim_y = (int16_t)(state->player_y - enemy->ey);
                    if (aim_x == 0) aim_x = 1;
                    if (aim_y == 0) aim_y = 1;
                    magnitude =
                        ot_abs_s16(aim_x) > ot_abs_s16(aim_y) ?
                        ot_abs_s16(aim_x) : ot_abs_s16(aim_y);
                    shot->sxm = ot_round_ratio(
                        (int32_t)aim_x * weapon.aim,
                        magnitude
                    );
                    shot->sym = ot_round_ratio(
                        (int32_t)aim_y * weapon.aim,
                        magnitude
                    );
                }
                state->enemy_shot_trigger_count++;
            }
        }
    }
    return true;
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
 * bounce, off-screen release, fire cadence, concrete 60-shot allocation and
 * launch lifecycle.  GBA OAM conversion remains a presentation-only adapter.
 */
static void ot_draw_enemy_pool(
    OtLevelPortState *state,
    uint8_t pool,
    int16_t temp_back_move,
    uint16_t temp_map_x_offset
)
{
    uint8_t index;

    for (index = pool; index < pool + OT_ENEMY_POOL_SIZE; index++) {
        OtEnemy *enemy;

        if (state->enemy_avail[index] == 1) continue;
        enemy = &state->enemy[index];
        state->enemy_motion_update_count++;
        enemy->mapoffset = temp_map_x_offset;

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

        if (
            enemy->ex + enemy->mapoffset > -29 &&
            enemy->ex + enemy->mapoffset < 300
        ) {
            OtEnemyDrawCommand *draw = &state->enemy_draw[index];

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
            /*
             * This is the exact source blit point.  JE_drawEnemy() draws
             * before movement, firing and the later player-shot collision
             * phase, so the GBA renderer consumes this immutable command
             * instead of re-reading a potentially moved/released slot.
             */
            if (
                (enemy->size == 1 && enemy->ey > -26) ||
                (enemy->size != 1 && enemy->ey > -13)
            ) {
                draw->active = true;
                draw->x = (int16_t)(enemy->ex + enemy->mapoffset);
                draw->y = enemy->ey;
                draw->graphic = enemy->egr[enemy->enemycycle - 1];
                draw->shape_table =
                    ot_enemy_resolved_shape_table(state, enemy);
                draw->size = enemy->size;
                draw->filter = enemy->filter;
                draw->pool = pool;
                draw->source_index = index;
                draw->enemy_cycle = enemy->enemycycle;
                draw->enemy_definition_id =
                    enemy->enemy_definition_id;
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
        if (
            enemy->ex + enemy->mapoffset <= -24 ||
            enemy->ex + enemy->mapoffset >= 296
        ) {
            continue;
        }
        if (enemy->edamaged) continue;
        state->frame_enemy_on_screen++;
        if (pool == 25 || pool == 75) {
            state->frame_ground_enemy_on_screen++;
        } else if (pool == 0) {
            state->frame_sky_enemy_on_screen++;
        }
        if (enemy->iced != 0) {
            enemy->iced--;
            if (enemy->enemyground) enemy->filter = 0x09;
            continue;
        }

        if (!ot_enemy_fire_slots(state, enemy)) continue;
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
    if (ot_data_catalog()->level_enemy_count == 0) {
        state->assets_valid = false;
        return;
    }
    enemy_index = (uint16_t)(
        ot_mt_rand(state) % ot_data_catalog()->level_enemy_count
    );
    if (!ot_data_level_enemy_pool_read(enemy_index, &enemy_id)) {
        state->assets_valid = false;
        return;
    }
    ot_new_enemy(state, 0, enemy_id, 0, OT_SPAWN_RANDOM);
}

static void ot_advance_enemies(OtLevelPortState *state)
{
    /* JE_main() draw/update order: ground, ground2, continual, sky, top. */
    ot_draw_enemy_pool(
        state,
        25,
        (int16_t)state->back_move,
        state->presentation_map_x_offset
    );
    ot_draw_enemy_pool(
        state,
        75,
        (int16_t)state->back_move,
        state->presentation_map_x_offset
    );
    ot_spawn_continual_enemy(state);
    ot_draw_enemy_pool(
        state,
        0,
        0,
        state->presentation_map_x2_offset
    );
    ot_draw_enemy_pool(
        state,
        50,
        (int16_t)state->back_move3,
        state->presentation_map_x3_offset
    );
}

static void ot_add_hit_effect(
    OtHitEffect *effects,
    uint8_t *effect_count,
    int16_t x,
    int16_t y,
    bool large,
    bool ground
)
{
    OtHitEffect *effect;

    if (*effect_count >= OT_HIT_EFFECT_COUNT) return;
    effect = &effects[*effect_count];
    effect->x = x;
    effect->y = y;
    effect->large = large;
    effect->ground = ground;
    (*effect_count)++;
}

static void ot_add_pickup_effect(
    OtPlayerCollisionResult *result,
    int16_t x,
    int16_t y,
    uint8_t explosion_type
)
{
    OtPickupEffect *effect;

    if (
        result->pickup_effect_count >= OT_PICKUP_EFFECT_COUNT
    ) {
        return;
    }
    effect = &result->pickup_effects[result->pickup_effect_count];
    effect->x = x;
    effect->y = y;
    effect->explosion_type = explosion_type;
    result->pickup_effect_count++;
}

static void ot_spawn_death_enemy(
    OtLevelPortState *state,
    uint8_t dead_index,
    uint16_t enemy_definition_id
)
{
    OtEnemyDefinition definition;
    uint8_t pool;
    uint8_t created;
    OtEnemy *child;
    const OtEnemy *dead = &state->enemy[dead_index];

    if (
        enemy_definition_id == 0 ||
        !ot_level_enemy_read(enemy_definition_id, &definition)
    ) {
        if (enemy_definition_id != 0) {
            state->death_spawn_attempt_count++;
            state->death_spawn_missing_definition_count++;
            state->assets_valid = false;
        }
        return;
    }
    pool = (uint8_t)(dead_index - dead_index % OT_ENEMY_POOL_SIZE);
    if (definition.value > 30000) pool = 0;
    created = ot_new_enemy(
        state,
        pool,
        enemy_definition_id,
        0,
        OT_SPAWN_DEATH
    );
    if (created == 0) return;
    child = &state->enemy[created - 1];
    child->scoreitem = child->evalue != 0;
    child->ex = dead->ex;
    child->ey = dead->ey;
}

static void ot_credit_destroyed_enemy(
    OtLevelPortState *state,
    const OtEnemy *enemy,
    OtShotCollisionResult *result
)
{
    if (enemy->evalue == 1) {
        /*
         * Direct translation of the cubeMax++ branch in the player-shot
         * death path.  A data cube can be credited by destruction without
         * becoming a separate score-item object.
         */
        state->data_cube_pickup_count++;
        result->data_cubes_awarded++;
    } else if (enemy->evalue > 1 && enemy->evalue < 10000) {
        uint32_t value = (uint16_t)enemy->evalue;

        state->direct_cash_awarded += value;
        result->cash_awarded += value;
    }
}

static bool ot_death_link_matches(
    const OtEnemy *candidate,
    uint8_t target_link
)
{
    uint8_t candidate_link = candidate->linknum;

    return
        target_link == 254 ||
        (
            target_link != 255 &&
            (
                target_link == candidate_link ||
                (int16_t)target_link - 100 == candidate_link ||
                (
                    candidate_link > 40 &&
                    candidate_link / 20 == target_link / 20 &&
                    candidate_link <= target_link
                )
            )
        );
}

static void ot_kill_enemy_group(
    OtLevelPortState *state,
    uint8_t hit_index,
    OtShotCollisionResult *result
)
{
    uint8_t index;
    uint8_t target_link = state->enemy[hit_index].linknum;

    if (target_link == 0) target_link = 255;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        OtEnemy *enemy;

        if (state->enemy_avail[index] == 1) continue;
        enemy = &state->enemy[index];
        if (
            index != hit_index &&
            !ot_death_link_matches(enemy, target_link)
        ) {
            continue;
        }
        if (enemy->special) {
            if (
                enemy->flagnum == 0 ||
                enemy->flagnum > OT_GLOBAL_FLAG_COUNT
            ) {
                state->assets_valid = false;
            } else {
                state->global_flags[enemy->flagnum - 1] = enemy->setto;
            }
        }
        ot_spawn_death_enemy(state, index, enemy->enemydie);
        ot_credit_destroyed_enemy(state, enemy, result);
        ot_add_hit_effect(
            result->effects,
            &result->effect_count,
            (int16_t)(enemy->ex + enemy->mapoffset),
            enemy->ey,
            enemy->size == 1,
            enemy->enemyground
        );
        /*
         * OpenTyrian keeps dlevel=-1 components in availability state 2 and
         * swaps them to their authored damaged graphic.  This is how a
         * destructible foreground reveals the underlying background without
         * discarding its fixed remnant.
         */
        if (
            enemy->edlevel == -1 &&
            target_link == enemy->linknum
        ) {
            enemy->edlevel = 0;
            state->enemy_avail[index] = 2;
            enemy->egr[0] = enemy->edgr;
            enemy->ani = 1;
            enemy->aniactive = 0;
            enemy->animax = 0;
            enemy->animin = 1;
            enemy->edamaged = true;
            enemy->enemycycle = 1;
        } else {
            ot_release_enemy(state, index);
            state->enemy_kill_count++;
        }
        result->kill_count++;
    }
}

static void ot_apply_damaged_transition(
    OtLevelPortState *state,
    uint8_t hit_index,
    uint8_t target_link,
    OtShotCollisionResult *result
)
{
    uint8_t index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        OtEnemy *enemy;
        uint8_t link;
        bool matches;

        if (state->enemy_avail[index] == 1) continue;
        enemy = &state->enemy[index];
        link = enemy->linknum;
        matches =
            index == hit_index ||
            (
                target_link != 255 &&
                (
                    (enemy->edlevel > 0 && link == target_link) ||
                    (
                        state->enemy_continual_damage &&
                        (int16_t)target_link - 100 == link
                    ) ||
                    (
                        link > 40 &&
                        link / 20 == target_link / 20 &&
                        link <= target_link
                    )
                )
            );
        if (!matches) continue;

        enemy->enemycycle = 1;
        enemy->edamaged = !enemy->edamaged;
        if (enemy->edani != 0) {
            enemy->ani = (uint8_t)ot_abs_s8(enemy->edani);
            enemy->aniactive = 1;
            enemy->animax = 0;
            enemy->animin = (uint8_t)enemy->edgr;
            enemy->enemycycle = (uint8_t)(enemy->animin - 1);
        } else if (enemy->edgr > 0) {
            enemy->egr[0] = enemy->edgr;
            enemy->ani = 1;
            enemy->aniactive = 0;
            enemy->animax = 0;
            enemy->animin = 1;
        } else {
            ot_release_enemy(state, index);
            state->enemy_kill_count++;
            result->kill_count++;
        }
        enemy->aniwhenfire = 0;
        if (enemy->armorleft > (uint8_t)enemy->edlevel) {
            enemy->armorleft = (uint8_t)enemy->edlevel;
        }
        ot_add_hit_effect(
            result->effects,
            &result->effect_count,
            (int16_t)(enemy->ex + enemy->mapoffset),
            enemy->ey,
            enemy->size == 1,
            enemy->enemyground
        );
    }
}

IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
ot_level_port_collide_player_shot(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    uint8_t damage,
    OtShotCollisionResult *result
)
{
    uint8_t index;

    if (result == 0) return;
    *result = (OtShotCollisionResult){0};
    result->remaining_damage = damage;
    if (state == 0 || damage == 0) return;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        OtEnemy *enemy;
        bool collided;
        uint16_t armor;
        uint8_t target_link;

        if (state->enemy_avail[index] != 0) continue;
        enemy = &state->enemy[index];
        if (enemy->enemycycle == 0) {
            collided =
                ot_abs_s16(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    )
                ) < 25 &&
                ot_abs_s16(
                    (int16_t)(enemy->ey - shot_y - 12)
                ) < 29;
        } else {
            collided =
                ot_abs_s16(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    )
                ) < 13 &&
                ot_abs_s16(
                    (int16_t)(enemy->ey - shot_y - 6)
                ) < 15;
        }
        if (!collided) continue;

        result->collided = true;
        result->hit_count++;
        state->player_shot_collision_count++;
        armor = enemy->armorleft;
        target_link = enemy->linknum;
        if (target_link == 0) target_link = 255;
        for (uint8_t bar = 0; bar < 2; bar++) {
            if (
                target_link != 255 &&
                target_link == state->boss_bar_link[bar]
            ) {
                state->boss_bar_color[bar] = 6;
            }
        }

        if (armor > result->remaining_damage) {
            uint8_t damage_before = result->remaining_damage;

            if (enemy->armorleft != 255) {
                enemy->armorleft =
                    (uint8_t)(enemy->armorleft - damage_before);
            }
            ot_add_hit_effect(
                result->effects,
                &result->effect_count,
                shot_x,
                shot_y,
                false,
                enemy->enemyground
            );
            if (
                (int16_t)armor - damage_before <= enemy->edlevel &&
                (
                    (!enemy->edamaged) ^
                    (enemy->edani < 0)
                )
            ) {
                ot_apply_damaged_transition(
                    state,
                    index,
                    target_link,
                    result
                );
            }
        } else {
            ot_kill_enemy_group(state, index, result);
        }

        if (result->remaining_damage <= armor) {
            result->remaining_damage = 0;
            result->consumed = true;
            return;
        }
        result->remaining_damage =
            (uint8_t)(result->remaining_damage - armor);
    }
}

static void ot_recalculate_purple_balls(OtLevelPortState *state)
{
    static const uint8_t required[12] = {
        1, 1, 2, 4, 8, 12, 16, 20, 25, 30, 40, 50
    };
    uint8_t power = state->player_front_weapon_power;

    if (power > 11) power = 11;
    state->player_purple_balls_needed = required[power];
}

static bool ot_power_up_weapon(
    OtLevelPortState *state,
    bool front,
    OtPlayerCollisionResult *result
)
{
    uint8_t *weapon_id = front ?
        &state->player_front_weapon_id :
        &state->player_rear_weapon_id;
    uint8_t *weapon_power = front ?
        &state->player_front_weapon_power :
        &state->player_rear_weapon_power;

    /*
     * player.c:power_up_weapon(), specialized only by replacing the PC
     * shotMultiPos reset with source-state ownership in this GBA proof.
     */
    if (*weapon_id != 0 && *weapon_power < 11) {
        (*weapon_power)++;
        ot_recalculate_purple_balls(state);
        return true;
    }
    result->cash_awarded += 1000;
    state->powerup_consolation_cash += 1000;
    return false;
}

static void ot_handle_purple_ball(
    OtLevelPortState *state,
    OtPlayerCollisionResult *result
)
{
    if (state->player_purple_balls_needed > 1) {
        state->player_purple_balls_needed--;
    } else {
        (void)ot_power_up_weapon(state, true, result);
    }
}

void ot_level_port_collide_player(
    OtLevelPortState *state,
    bool player_vulnerable,
    OtPlayerCollisionResult *result
)
{
    uint8_t index;

    if (result == 0) return;
    *result = (OtPlayerCollisionResult){0};
    if (state == 0) return;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        OtEnemy *enemy;
        uint8_t availability;
        int16_t value;
        bool was_score_item;
        bool consumed = false;
        bool suppress_contact = false;
        bool show_fixed_pickup_effect = false;

        if (state->enemy_avail[index] == 1) continue;
        enemy = &state->enemy[index];
        if (
            ot_abs_s16(
                (int16_t)(
                    state->player_x -
                    (enemy->ex + enemy->mapoffset)
                )
            ) >= 12 ||
            ot_abs_s16(
                (int16_t)(state->player_y - enemy->ey)
            ) >= 14
        ) {
            continue;
        }
        availability = state->enemy_avail[index];
        value = enemy->evalue;
        was_score_item = enemy->scoreitem;

        /*
         * Direct fixed-single-player translation of JE_playerCollide().
         * Keep the source branch order: high-value equipment, armor, bonus
         * portals, ordinary score items, then damaging enemy contact.
         */
        if (value > 29999) {
            suppress_contact = true;
            if (value == 30000) {
                result->cash_awarded += 100;
                ot_handle_purple_ball(state, result);
                state->high_value_pickup_count++;
                consumed = true;
            } else if (value > 32100) {
                result->cash_awarded += 250;
                state->player_special = (uint8_t)(value - 32100);
                state->high_value_pickup_count++;
                consumed = true;
            } else if (value > 32000) {
                /*
                 * Standard one-player mode does not own player-2 sidekicks;
                 * OpenTyrian deliberately leaves this object in the world.
                 */
            } else if (value > 31000) {
                /*
                 * Preserve the source's unconditional 250-credit award
                 * before its player-2/onePlayerAction ownership checks.
                 */
                result->cash_awarded += 250;
            } else {
                /*
                 * Front-weapon items above 30000 are only consumed by
                 * two-player or onePlayerAction modes, both outside this
                 * fixed single-player first-level translation.
                 */
            }
        } else if (value > 20000) {
            suppress_contact = true;
            uint16_t armor = (uint16_t)(
                state->player_armor + value - 20000
            );

            state->player_armor = armor > 28 ? 28 : (uint8_t)armor;
            state->armor_pickup_count++;
            consumed = true;
        } else if (value > 10000 && availability == 2) {
            suppress_contact = true;
            if (!state->bonus_level) {
                state->bonus_level = true;
                state->next_level = (uint16_t)(value - 10000);
                state->display_time = 150;
                state->bonus_portal_pickup_count++;
                result->bonus_level_triggered = true;
                result->next_level = state->next_level;
                consumed = true;
            }
        } else if (enemy->scoreitem) {
            suppress_contact = true;
            consumed = true;
            show_fixed_pickup_effect = true;
            if (value == 1) {
                state->data_cube_pickup_count++;
                result->data_cubes_awarded++;
            } else if (value == -1) {
                state->front_weapon_powerup_count++;
                result->front_powerups++;
                (void)ot_power_up_weapon(state, true, result);
            } else if (value == -2) {
                state->rear_weapon_powerup_count++;
                result->rear_powerups++;
                (void)ot_power_up_weapon(state, false, result);
            } else if (value == -3) {
                /*
                 * The source creates special player shot 104.  The current
                 * fixed Pulse-Cannon adapter has no misc-shot pool, so retain
                 * the exact acquired gameplay state for the forthcoming
                 * player-weapon translation.
                 */
                state->orbiting_asteroid_pickup_count++;
            } else if (value == -4) {
                if (state->player_superbombs < 10) {
                    state->player_superbombs++;
                    result->superbombs_awarded++;
                }
                state->superbomb_pickup_count++;
            } else if (value == -5) {
                state->player_front_weapon_id = 25;
                state->player_rear_weapon_id = 26;
                state->player_front_weapon_power = 1;
                state->player_rear_weapon_power = 1;
                state->player_weapon_mode = 1;
                state->hotdog_pickup_count++;
            } else {
                /*
                 * Literal source fallback: this_player->cash += evalue.
                 * Known special negative values have already branched.
                 */
                result->cash_awarded += (uint32_t)(int32_t)value;
            }
        }

        if (consumed) {
            if (show_fixed_pickup_effect) {
                /*
                 * Literal JE_playerCollide() tail:
                 * JE_setupExplosion(..., explosiontype, true, false).
                 * JE_makeEnemy split the source byte into enemyground and
                 * explonum, so reconstruct it without another HDT read.
                 */
                ot_add_pickup_effect(
                    result,
                    (int16_t)(enemy->ex + enemy->mapoffset),
                    enemy->ey,
                    (uint8_t)(
                        (enemy->explonum << 1) |
                        (enemy->enemyground ? 0u : 1u)
                    )
                );
            }
            ot_release_enemy(state, index);
            if (was_score_item) {
                state->score_item_pickup_count++;
            }
            result->pickup_count++;
            continue;
        }
        if (
            !suppress_contact &&
            player_vulnerable &&
            availability == 0 &&
            enemy->enemyground
        ) {
            uint8_t damage = enemy->armorleft;

            if (damage > 2) damage = 2;
            state->player_enemy_contact_count++;
            result->contact_count++;
            result->damage = (uint16_t)(result->damage + damage);
        }
    }
}

void ot_level_port_update_enemy_shots(OtLevelPortState *state)
{
    uint8_t index;

    if (state == 0) return;
    for (index = 0; index < OT_ENEMY_SHOT_COUNT; index++) {
        OtEnemyShot *shot = &state->enemy_shot[index];
        uint8_t old_duration;

        if (!shot->active) continue;
        state->enemy_shot_motion_update_count++;
        shot->sxm = (int16_t)(shot->sxm + shot->sxc);
        shot->sx = (int16_t)(shot->sx + shot->sxm);
        if (shot->tx != 0) {
            if (shot->sx > state->player_x) {
                if (shot->sxm > -(int16_t)shot->tx) shot->sxm--;
            } else if (shot->sxm < shot->tx) {
                shot->sxm++;
            }
        }
        shot->sym = (int16_t)(shot->sym + shot->syc);
        shot->sy = (int16_t)(shot->sy + shot->sym);
        if (shot->ty != 0) {
            if (shot->sy > state->player_y) {
                if (shot->sym > -(int16_t)shot->ty) shot->sym--;
            } else if (shot->sym < shot->ty) {
                shot->sym++;
            }
        }

        old_duration = shot->duration;
        shot->duration--;
        if (
            old_duration == 0 ||
            shot->sy > 190 ||
            shot->sy <= -14 ||
            shot->sx > 275 ||
            shot->sx <= 0
        ) {
            ot_release_enemy_shot(state, shot);
            continue;
        }
        if (
            shot->sx > state->player_x - 11 &&
            shot->sx < state->player_x + 11 &&
            shot->sy > state->player_y - 14 &&
            shot->sy < state->player_y + 14
        ) {
            state->frame_player_damage =
                (uint16_t)(state->frame_player_damage + shot->sdmg);
            ot_release_enemy_shot(state, shot);
            state->enemy_shot_player_hit_count++;
            continue;
        }
        if (shot->animax != 0) {
            shot->animate++;
            if (shot->animate >= shot->animax) shot->animate = 0;
        }
    }
}

void ot_level_port_clear_projectiles(OtLevelPortState *state)
{
    uint8_t index;

    if (state == 0) return;
    for (index = 0; index < OT_ENEMY_SHOT_COUNT; index++) {
        ot_release_enemy_shot(state, &state->enemy_shot[index]);
    }
}

void ot_level_port_update_parallax(
    OtLevelPortState *state,
    int16_t player_x
)
{
    uint16_t temp_w;

    if (state == 0) return;
    if (player_x < 40) player_x = 40;
    if (player_x > 256) player_x = 256;

    /*
     * Integer form of the positive-range OpenTyrian expression:
     * floor((260 - (player.x - 36)) / (260 - 36) * (24 * 3) - 1).
     */
    temp_w = (uint16_t)(
        ((uint32_t)(296 - player_x) * 72u) / 224u - 1u
    );
    state->map_x3_offset = temp_w;
    state->map_x2_offset = (uint16_t)((temp_w * 2u) / 3u);
    state->map_x_offset = (uint16_t)(state->map_x2_offset / 2u);
    state->parallax_initialized = true;
}

static void ot_apply_relative_event_offset(
    OtLevelPortState *state,
    int16_t skip_events
)
{
    int32_t next_index =
        (int32_t)state->event_index + 1 + skip_events;

    if (next_index < 0) next_index = 0;
    if (next_index > state->event_count) {
        next_index = state->event_count;
    }
    state->event_index = (uint16_t)next_index;
    if (skip_events > 0) {
        state->skipped_event_count += (uint16_t)skip_events;
    }
}

static void ot_update_filter_fade(OtLevelPortState *state)
{
    if (!state->filter_active || !state->filter_fade) return;

    state->level_brightness = (int8_t)(
        state->level_brightness +
        state->level_brightness_change
    );
    if (
        (
            state->filter_fade_start &&
            state->level_brightness < -14
        ) ||
        state->level_brightness > 14
    ) {
        state->level_brightness_change =
            (int8_t)-state->level_brightness_change;
        state->filter_fade_start = false;
        state->level_filter = state->level_filter_new;
    }
    if (
        !state->filter_fade_start &&
        state->level_brightness == 0
    ) {
        state->filter_fade = false;
        state->level_brightness = -99;
    }
}

static void ot_update_level_timer(OtLevelPortState *state)
{
    if (!state->level_timer || state->level_timer_countdown == 0) {
        return;
    }
    state->level_timer_countdown--;
    if (state->level_timer_countdown == 0) {
        ot_event_jump(state, state->level_timer_jump_to);
        return;
    }
    if (state->level_timer_countdown > 200) {
        if (state->level_timer_countdown % 100u == 0) {
            state->frame_sound_queue[7] = 17; /* S_WARNING */
        }
        if (state->level_timer_countdown % 10u == 0) {
            state->frame_sound_queue[6] = 24; /* S_CLICK */
        }
    } else if (state->level_timer_countdown % 20u == 0) {
        state->frame_sound_queue[7] = 17; /* S_WARNING */
    }
}

void ot_level_port_advance(
    OtLevelPortState *state,
    uint16_t cur_loc,
    int16_t player_x,
    int16_t player_y
)
{
    OtEventRecord event;
    uint32_t event_guard = 0;
    uint32_t event_guard_limit;
    uint8_t index;

    state->frame_sound_mask = 0;
    for (index = 0; index < 8; index++) {
        state->frame_sound_queue[index] = 0;
    }
    state->frame_player_damage = 0;
    state->frame_player_invulnerable_ticks = 0;
    state->pending_text_window = 0;
    state->map_position_override_pending = false;
    state->background2_wrap_pending = false;
    state->frame_enemy_on_screen = 0;
    state->frame_ground_enemy_on_screen = 0;
    state->frame_sky_enemy_on_screen = 0;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        state->enemy_draw[index].active = false;
    }
    state->cur_loc = cur_loc;
    state->player_x = player_x;
    state->player_y = player_y;
    event_guard_limit = (uint32_t)state->event_count * 4u + 32u;
    while (state->event_index < state->event_count) {
        int16_t skip_events;
        bool jumped;

        if (!ot_level_event_read(state->event_index, &event)) {
            state->assets_valid = false;
            break;
        }
        if (event.eventtime > state->cur_loc) break;
        if (apply_event(state, &event, &skip_events, &jumped)) {
            state->applied_event_count++;
        } else {
            state->deferred_event_count++;
        }
        if (!jumped) {
            ot_apply_relative_event_offset(state, skip_events);
        }
        event_guard++;
        if (event_guard > event_guard_limit) {
            state->assets_valid = false;
            break;
        }
    }
    /*
     * Backgrounds and enemies use the offsets calculated after the preceding
     * player phase.  Snapshot them before the keypad adapter calculates the
     * next frame's offsets.
     */
    state->presentation_map_x_offset = state->map_x_offset;
    state->presentation_map_x2_offset = state->map_x2_offset;
    state->presentation_map_x3_offset = state->background3_x1 ?
        state->map_x_offset : state->map_x3_offset;
    state->presentation_parallax_initialized =
        state->parallax_initialized;
    state->presentation_background3_x1 = state->background3_x1;
    ot_advance_enemies(state);
    if (
        state->stop_background_num == 1 &&
        state->frame_ground_enemy_on_screen == 0
    ) {
        state->stop_background_num = 9;
    }
    if (
        state->stop_background_num == 2 &&
        state->frame_sky_enemy_on_screen == 0
    ) {
        state->stop_background_num = 9;
    }
    if (
        !state->end_level &&
        state->frame_enemy_on_screen == 0 &&
        state->stop_backgrounds
    ) {
        state->stop_backgrounds = false;
        state->back_move = 1;
        state->back_move2 = 2;
        state->back_move3 = 3;
        state->explode_move = 2;
    }
    if (
        state->return_active &&
        state->frame_enemy_on_screen == 0
    ) {
        ot_event_jump(state, UINT16_MAX);
        state->return_active = false;
    }
    ot_update_filter_fade(state);
    ot_update_level_timer(state);
}
