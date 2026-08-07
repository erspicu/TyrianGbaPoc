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

extern uint32_t ot_mt_rand_core_asm(OtMt19937 *rng);

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) uint32_t
ot_mt_rand(OtLevelPortState *state)
{
#if TYRIAN_GBA_HOTPATH_ASM
    uint32_t y = ot_mt_rand_core_asm(&state->rng);

    state->rng_call_count++;
    return y;
#else
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
#endif
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

static inline bool ot_player_shot_axis_overlaps_c_inline(
    int16_t delta,
    uint16_t radius
)
{
#if TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
    /*
     * Strict -radius < delta < radius written as one unsigned interval.
     * The caller performs the source's explicit int16_t truncation before
     * entering here; promotion to int32_t then prevents intermediate wrap.
     * Authored gameplay coordinates never reach INT16_MIN, the sole value
     * for which the legacy int16-returning abs helper is not representable.
     */
    return
        (uint32_t)((int32_t)delta + radius - 1) <
        (uint32_t)(radius * 2u - 1u);
#else
    return ot_abs_s16(delta) < (int16_t)radius;
#endif
}

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) bool
ot_player_shot_axis_overlaps_c_reference(
    int16_t delta,
    uint16_t radius
)
{
    return ot_player_shot_axis_overlaps_c_inline(delta, radius);
}
#endif

#if !TYRIAN_GBA_HOTPATH_ASM
static inline bool ot_player_shot_axis_overlaps(
    int16_t delta,
    uint16_t radius
)
{
    return ot_player_shot_axis_overlaps_c_inline(delta, radius);
}
#endif

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

    /* JE_makeEnemy() substitutes the ordinary arcade pickup definition. */
    if (
        state->player_super_arcade_mode != 0 &&
        enemy_definition_id == 534
    ) {
        enemy_definition_id = 533;
    }
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
        int32_t value = definition.value;

        /* Literal JE_makeEnemy() reward scaling, including event-46 levels. */
        switch (state->difficulty_level) {
        case 0:
            value = value * 3 / 4;
            break;
        case 1:
        case 2:
            break;
        case 3:
            value = value * 9 / 8;
            break;
        case 4:
            value = value * 3 / 2;
            break;
        case 5:
            value *= 2;
            break;
        case 6:
            value = value * 5 / 2;
            break;
        case 7:
        case 8:
            value *= 4;
            break;
        default:
            value *= 8;
            break;
        }
        enemy->evalue = (int16_t)value;
        if (enemy->evalue > 10000) enemy->evalue = 10000;
    } else {
        enemy->evalue = definition.value;
    }

    if (definition.armor > 0) {
        uint16_t armor = definition.armor;

        if (definition.armor != 255) {
            switch (state->difficulty_level) {
            case 0:
                armor = (uint16_t)(definition.armor / 2u + 1u);
                break;
            case 1:
                armor = (uint16_t)(definition.armor * 3u / 4u + 1u);
                break;
            case 2:
                break;
            case 3:
                armor = (uint16_t)(definition.armor * 6u / 5u);
                break;
            case 4:
                armor = (uint16_t)(definition.armor * 3u / 2u);
                break;
            case 5:
                armor = (uint16_t)(definition.armor * 9u / 5u);
                break;
            case 6:
                armor = (uint16_t)(definition.armor * 2u);
                break;
            case 7:
                armor = (uint16_t)(definition.armor * 3u);
                break;
            case 8:
                armor = (uint16_t)(definition.armor * 4u);
                break;
            default:
                armor = (uint16_t)(definition.armor * 8u);
                break;
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
    OT_SPAWN_WARNING_SHIP,
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
    case OT_SPAWN_WARNING_SHIP:
        state->warning_ship_spawn_attempt_count++;
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
    case OT_SPAWN_WARNING_SHIP:
        state->warning_ship_spawn_pool_full_count++;
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
    case OT_SPAWN_WARNING_SHIP:
        state->warning_ship_spawn_missing_definition_count++;
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
    case OT_SPAWN_WARNING_SHIP:
        state->warning_ship_spawn_success_count++;
        break;
    }
}

static inline void ot_player_shot_collision_mask_set(
    OtLevelPortState *state,
    uint8_t enemy_index,
    bool active
)
{
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    uint32_t bit;
    uint8_t word;

    if (!state->player_shot_collision_mask_active) return;
    word = (uint8_t)(enemy_index >> 5);
    bit = 1u << (enemy_index & 31);
    if (active) {
        state->player_shot_collision_active_mask[word] |= bit;
    } else {
        state->player_shot_collision_active_mask[word] &= ~bit;
    }
#else
    (void)state;
    (void)enemy_index;
    (void)active;
#endif
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
    ot_player_shot_collision_mask_set(state, slot, avail == 0);
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
     * JE_main(), start_level_first initialization. UI, audio, demo, storage
     * and presentation are supplied by the surrounding GBA platform adapter.
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
    state->warning_color = 120;
    state->warning_color_change = 1;
    state->warning_sound_delay = 0;
    state->armor_ship_delay = 50;
    state->enemies_active = true;
    state->background2_over = 1;
    state->level_enemy_frequency = 96;
    state->current_song = -1;
    state->music_fade_volume = 255;
    state->level_end = 255;
    /* tyrian2.c's initial JE_filterScreen fade-in state. */
    state->filter_active = true;
    state->filter_fade = true;
    state->filter_fade_start = false;
    state->level_filter = -99;
    state->level_filter_new = 0;
    state->level_brightness = -14;
    state->level_brightness_change = 1;
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
    state->player_generator = 2;
    state->player_shield_item = 4;
    state->player_ship = 1;
    state->super_arcade_power_up = 1;
    state->galaga_life_threshold = 10000;
    state->player_armor = 10;
    state->player_shot_hit_area_x = 11;
    state->player_shot_hit_area_y = 14;
    state->player_weapon_mode = 1;
    state->player_purple_balls_needed = 1;
    state->display_flash = 0;
    state->display_flash_change = 1;
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
