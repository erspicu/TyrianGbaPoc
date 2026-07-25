/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct translation staging area for OpenTyrian's first-level game loop.
 * The field assignments below follow JE_main() and JE_eventSystem() in
 * src/tyrian2.c at revision
 * 1c34d1bddac8c8f2de834229d04b5a729525c944.
 */
#include "opentyrian_level_port.h"

extern const uint8_t opentyrian_level1_events[];
extern const uint8_t opentyrian_level1_enemies[];

enum {
    OT_SOURCE_HEADER_BYTES = 8,
    OT_EVENT_RECORD_BYTES = 11,
    OT_ENEMY_RAW_RECORD_BYTES = 77,
    OT_ENEMY_RECORD_BYTES = 79,
    OT_MT_M = 397,
    OT_SOURCE_PARITY_TEST_SEED = 5489,
};

_Static_assert(sizeof(uint8_t) == 1, "OpenTyrian byte width changed");
_Static_assert(sizeof(int8_t) == 1, "OpenTyrian shortint width changed");
_Static_assert(sizeof(uint16_t) == 2, "OpenTyrian word width changed");
_Static_assert(sizeof(int16_t) == 2, "OpenTyrian integer width changed");
_Static_assert(
    OPENTYRIAN_LEVEL1_EVENT_RECORD_BYTES == OT_EVENT_RECORD_BYTES,
    "generated OpenTyrian event record width changed"
);
_Static_assert(
    OPENTYRIAN_LEVEL1_ENEMY_RECORD_BYTES == OT_ENEMY_RECORD_BYTES,
    "generated OpenTyrian enemy record width changed"
);
_Static_assert(OT_ENEMY_COUNT == 100, "OpenTyrian enemy pool size changed");
_Static_assert(OT_ENEMY_POOL_SIZE == 25, "OpenTyrian pool group size changed");

static uint16_t read_u16(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static int16_t read_s16(const uint8_t *source)
{
    return (int16_t)read_u16(source);
}

static bool header_matches(
    const uint8_t *source,
    char a,
    char b,
    char c,
    char d,
    uint16_t count,
    uint8_t record_bytes
)
{
    return source[0] == (uint8_t)a &&
           source[1] == (uint8_t)b &&
           source[2] == (uint8_t)c &&
           source[3] == (uint8_t)d &&
           read_u16(source + 4) == count &&
           source[6] == record_bytes;
}

bool ot_level1_event_read(uint16_t index, OtEventRecord *event)
{
    const uint8_t *source;

    if (event == 0 || index >= OPENTYRIAN_LEVEL1_EVENT_COUNT) {
        return false;
    }
    source = opentyrian_level1_events + OT_SOURCE_HEADER_BYTES +
             (uint32_t)index * OT_EVENT_RECORD_BYTES;
    event->eventtime = read_u16(source);
    event->eventtype = source[2];
    event->eventdat = read_s16(source + 3);
    event->eventdat2 = read_s16(source + 5);
    event->eventdat3 = (int8_t)source[7];
    event->eventdat5 = (int8_t)source[8];
    event->eventdat6 = (int8_t)source[9];
    event->eventdat4 = source[10];
    return true;
}

bool ot_level1_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy)
{
    uint16_t low = 0;
    uint16_t high = OPENTYRIAN_LEVEL1_ENEMY_COUNT;

    if (enemy == 0) return false;
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2);
        const uint8_t *record =
            opentyrian_level1_enemies + OT_SOURCE_HEADER_BYTES +
            (uint32_t)middle * OT_ENEMY_RECORD_BYTES;
        uint16_t record_id = read_u16(record);
        const uint8_t *source;
        uint8_t index;

        if (record_id < enemy_id) {
            low = (uint16_t)(middle + 1);
            continue;
        }
        if (record_id > enemy_id) {
            high = middle;
            continue;
        }

        source = record + 2;
        enemy->ani = source[0];
        for (index = 0; index < 3; index++) {
            enemy->tur[index] = source[1 + index];
            enemy->freq[index] = source[4 + index];
        }
        enemy->xmove = (int8_t)source[7];
        enemy->ymove = (int8_t)source[8];
        enemy->xaccel = (int8_t)source[9];
        enemy->yaccel = (int8_t)source[10];
        enemy->xcaccel = (int8_t)source[11];
        enemy->ycaccel = (int8_t)source[12];
        enemy->startx = read_s16(source + 13);
        enemy->starty = read_s16(source + 15);
        enemy->startxc = (int8_t)source[17];
        enemy->startyc = (int8_t)source[18];
        enemy->armor = source[19];
        enemy->esize = source[20];
        for (index = 0; index < 20; index++) {
            enemy->egraphic[index] = read_u16(source + 21 + index * 2);
        }
        enemy->explosiontype = source[61];
        enemy->animate = source[62];
        enemy->shapebank = source[63];
        enemy->xrev = (int8_t)source[64];
        enemy->yrev = (int8_t)source[65];
        enemy->dgr = read_u16(source + 66);
        enemy->dlevel = (int8_t)source[68];
        enemy->dani = (int8_t)source[69];
        enemy->elaunchfreq = source[70];
        enemy->elaunchtype = read_u16(source + 71);
        enemy->value = read_s16(source + 73);
        enemy->eenemydie = read_u16(source + 75);
        return true;
    }
    return false;
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

static uint8_t ot_create_new_event_enemy(
    OtLevelPortState *state,
    OtEventRecord *event,
    uint8_t enemy_type_offset,
    uint8_t enemy_offset,
    int16_t unique_shape_table
)
{
    uint8_t index;
    uint8_t slot = OT_ENEMY_COUNT;
    uint8_t avail;
    uint16_t enemy_definition_id;
    OtEnemy *enemy;

    state->spawn_attempt_count++;
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
        state->spawn_pool_full_count++;
        return 0;
    }

    enemy_definition_id =
        (uint16_t)(event->eventdat + enemy_type_offset);
    enemy = &state->enemy[slot];
    if (!ot_make_enemy(
        state,
        enemy,
        enemy_definition_id,
        unique_shape_table,
        &avail
    )) {
        state->spawn_missing_definition_count++;
        state->assets_valid = false;
        return 0;
    }

    state->enemy_avail[slot] = avail;
    state->spawn_success_count++;
    state->active_enemy_count++;
    if (state->active_enemy_count > state->max_active_enemy_count) {
        state->max_active_enemy_count = state->active_enemy_count;
    }
    state->last_created_slot = (uint8_t)(slot + 1);

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
    OtEnemyDefinition first_spawn_definition;
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

    state->assets_valid = header_matches(
        opentyrian_level1_events,
        'O', 'T', 'L', '1',
        OPENTYRIAN_LEVEL1_EVENT_COUNT,
        OT_EVENT_RECORD_BYTES
    ) && header_matches(
        opentyrian_level1_enemies,
        'O', 'T', 'E', '1',
        OPENTYRIAN_LEVEL1_ENEMY_COUNT,
        OT_ENEMY_RECORD_BYTES
    ) && opentyrian_level1_enemies[7] == OT_ENEMY_RAW_RECORD_BYTES &&
        ot_level1_enemy_read(10, &first_spawn_definition) &&
        first_spawn_definition.ani == 8 &&
        first_spawn_definition.armor == 3 &&
        first_spawn_definition.esize == 1 &&
        first_spawn_definition.shapebank == 1 &&
        first_spawn_definition.value == 15;
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

void ot_level_port_advance(OtLevelPortState *state, uint16_t cur_loc)
{
    OtEventRecord event;

    state->cur_loc = cur_loc;
    while (
        state->event_index < OPENTYRIAN_LEVEL1_EVENT_COUNT &&
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
            (uint16_t)(OPENTYRIAN_LEVEL1_EVENT_COUNT - state->event_index);
        if (skip_events > remaining) skip_events = remaining;
        state->event_index = (uint16_t)(state->event_index + skip_events);
        state->skipped_event_count += skip_events;
    }
}
