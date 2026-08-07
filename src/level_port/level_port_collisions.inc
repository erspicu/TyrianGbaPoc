#include "gba_hotpath_layout.inc"

/*
 * The ARM kernel reads only this explicitly asserted subset of the two C
 * structures.  A field addition or ABI change must fail at compile time;
 * silently keeping stale numeric offsets would be catastrophic on hardware.
 */
_Static_assert(sizeof(OtEnemy) == OT_ASM_ENEMY_SIZE, "OtEnemy ARM size");
_Static_assert(
    __builtin_offsetof(OtEnemy, ex) == OT_ASM_ENEMY_EX_OFFSET,
    "OtEnemy::ex ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtEnemy, ey) == OT_ASM_ENEMY_EY_OFFSET,
    "OtEnemy::ey ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtEnemy, enemycycle) == OT_ASM_ENEMY_CYCLE_OFFSET,
    "OtEnemy::enemycycle ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtEnemy, mapoffset) == OT_ASM_ENEMY_MAPOFFSET_OFFSET,
    "OtEnemy::mapoffset ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtLevelPortState, enemy) ==
        OT_ASM_STATE_ENEMY_OFFSET,
    "OtLevelPortState::enemy ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtLevelPortState, enemy_avail) ==
        OT_ASM_STATE_ENEMY_AVAIL_OFFSET,
    "OtLevelPortState::enemy_avail ARM offset"
);
_Static_assert(
    __builtin_offsetof(
        OtLevelPortState,
        player_shot_collision_active_mask
    ) == OT_ASM_STATE_COLLISION_MASK_OFFSET,
    "OtLevelPortState collision mask ARM offset"
);
_Static_assert(
    __builtin_offsetof(
        OtLevelPortState,
        player_shot_collision_mask_active
    ) == OT_ASM_STATE_COLLISION_MASK_ACTIVE_OFFSET,
    "OtLevelPortState collision mask active ARM offset"
);
_Static_assert(
    __builtin_offsetof(
        OtLevelPortState,
        player_shot_collision_candidate_visit_count
    ) == OT_ASM_STATE_CANDIDATE_VISITS_OFFSET,
    "OtLevelPortState collision visits ARM offset"
);
_Static_assert(
    sizeof(OtShotCollisionResult) == OT_ASM_RESULT_SIZE,
    "OtShotCollisionResult ARM size"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, collided) ==
        OT_ASM_RESULT_COLLIDED_OFFSET,
    "OtShotCollisionResult::collided ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, consumed) ==
        OT_ASM_RESULT_CONSUMED_OFFSET,
    "OtShotCollisionResult::consumed ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, remaining_damage) ==
        OT_ASM_RESULT_REMAINING_OFFSET,
    "OtShotCollisionResult::remaining_damage ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, hit_count) ==
        OT_ASM_RESULT_HIT_COUNT_OFFSET,
    "OtShotCollisionResult::hit_count ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, kill_count) ==
        OT_ASM_RESULT_KILL_COUNT_OFFSET,
    "OtShotCollisionResult::kill_count ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, effect_count) ==
        OT_ASM_RESULT_EFFECT_COUNT_OFFSET,
    "OtShotCollisionResult::effect_count ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, superpixel_hit_count) ==
        OT_ASM_RESULT_SUPERPIXEL_COUNT_OFFSET,
    "OtShotCollisionResult::superpixel_hit_count ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, data_cubes_awarded) ==
        OT_ASM_RESULT_CUBES_OFFSET,
    "OtShotCollisionResult::data_cubes_awarded ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, cash_awarded) ==
        OT_ASM_RESULT_CASH_OFFSET,
    "OtShotCollisionResult::cash_awarded ARM offset"
);
_Static_assert(
    __builtin_offsetof(OtShotCollisionResult, effects) ==
        OT_ASM_RESULT_EFFECTS_OFFSET,
    "OtShotCollisionResult::effects ARM offset"
);

/* Runtime-readable flags keep one assembly object correct for every A/B. */
const uint32_t ot_player_shot_collision_asm_config =
    (TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK ?
        OT_ASM_CONFIG_ACTIVE_MASK : 0) |
    (TYRIAN_GBA_COLLISION_MASK_FAST_PATH ?
        OT_ASM_CONFIG_MASK_FAST_PATH : 0) |
    (TYRIAN_GBA_COLLISION_LAZY_RESULT ?
        OT_ASM_CONFIG_LAZY_RESULT : 0) |
    (TYRIAN_GBA_COLLISION_UNSIGNED_RANGE ?
        OT_ASM_CONFIG_UNSIGNED_RANGE : 0)
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    | OT_ASM_CONFIG_RECORD_VISITS
#endif
    ;

static void ot_add_hit_effect(
    OtHitEffect *effects,
    uint8_t *effect_count,
    int16_t x,
    int16_t y,
    bool large,
    bool ground,
    uint8_t repeat_count
)
{
    OtHitEffect *effect;

    if (*effect_count >= OT_HIT_EFFECT_COUNT) return;
    effect = &effects[*effect_count];
    effect->x = x;
    effect->y = y;
    effect->large = large;
    effect->ground = ground;
    effect->repeat_count = repeat_count;
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
    if (
        state->player_super_arcade_mode != 0 &&
        definition.value == 30000
    ) {
        /* Direct tyrian2.c suppression of purple-ball death drops. */
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
    if (
        state->player_super_arcade_mode != 0 &&
        child->evalue > 30000
    ) {
        state->super_arcade_power_up++;
        if (state->super_arcade_power_up > 5) {
            state->super_arcade_power_up = 1;
        }
        child->egr[0] = (uint16_t)(
            5u + state->super_arcade_power_up * 2u
        );
        child->evalue = (int16_t)(
            30000u + state->super_arcade_power_up
        );
    }
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

static ARM_CODE __attribute__((noinline, noclone)) void
ot_kill_enemy_group(
    OtLevelPortState *state,
    uint8_t hit_index,
    OtShotCollisionResult *result
)
{
    uint8_t index;
    uint8_t target_link = state->enemy[hit_index].linknum;

    if (target_link == 0) target_link = 255;
    /*
     * Direct translation of tyrian2.c's lethal player-shot branch:
     *
     *   if ((temp == 254) && (superEnemy254Jump > 0))
     *       JE_eventJump(superEnemy254Jump);
     *
     * Several stock bosses intentionally loop their authored event segment
     * until the link-254 group dies.  Event 57 supplies the escape address;
     * omitting this jump leaves the boss dead while the segment loops forever.
     */
    if (
        target_link == 254 &&
        state->super_enemy_254_jump > 0 &&
        ot_event_jump(state, state->super_enemy_254_jump)
    ) {
        state->super_enemy_254_jump_count++;
    }
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
            enemy->enemyground,
            enemy->size == 1 ? enemy->explonum : 0
        );
        state->frame_sound_queue[6] =
            enemy->size == 1 ? 9 : 8;
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
            ot_player_shot_collision_mask_set(state, index, false);
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

static ARM_CODE __attribute__((noinline, noclone)) void
ot_apply_damaged_transition(
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
            enemy->enemyground,
            enemy->size == 1 ? (uint8_t)(enemy->explonum / 2u) : 0
        );
    }
}

ARM_CODE __attribute__((noinline, noclone)) void
ot_level_port_begin_player_shot_collision_phase(
    OtLevelPortState *state
)
{
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    uint8_t index;

    if (state == 0) return;
    state->player_shot_collision_active_mask[0] = 0;
    state->player_shot_collision_active_mask[1] = 0;
    state->player_shot_collision_active_mask[2] = 0;
    state->player_shot_collision_active_mask[3] = 0;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (state->enemy_avail[index] == 0) {
            state->player_shot_collision_active_mask[index >> 5] |=
                1u << (index & 31);
        }
    }
    state->player_shot_collision_mask_active = true;
    state->player_shot_collision_mask_rebuild_count++;
#else
    (void)state;
#endif
}

void ot_level_port_end_player_shot_collision_phase(
    OtLevelPortState *state
)
{
    if (state == 0) return;
    state->player_shot_collision_mask_active = false;
}

#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
static const uint8_t ot_collision_debruijn_index[32] = {
    0, 1, 28, 2, 29, 14, 24, 3,
    30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7,
    26, 12, 18, 6, 11, 5, 10, 9,
};

static inline uint8_t ot_collision_lowest_set_bit(uint32_t bits)
{
    uint32_t isolated = bits & (0u - bits);

    return ot_collision_debruijn_index[
        (isolated * 0x077cb531u) >> 27
    ];
}

/*
 * Return collision candidates in the same ascending slot order as the
 * original 0..99 loop.  The live mask is re-read after every candidate:
 * death-spawn children in a later slot can therefore still be hit by the
 * same penetrating shot, while children allocated below the cursor cannot,
 * exactly matching the source loop's mutation semantics.
 */
static inline bool ot_next_player_shot_collision_candidate(
    const OtLevelPortState *state,
    uint8_t *cursor,
    uint8_t *enemy_index
)
{
    uint8_t search = *cursor;

#if !TYRIAN_GBA_COLLISION_MASK_FAST_PATH
    if (!state->player_shot_collision_mask_active) {
        if (search >= OT_ENEMY_COUNT) return false;
        *enemy_index = search;
        *cursor = (uint8_t)(search + 1);
        return true;
    }
#endif

    while (search < OT_ENEMY_COUNT) {
        uint8_t word_index = (uint8_t)(search >> 5);
        uint32_t candidates =
            state->player_shot_collision_active_mask[word_index] &
            (UINT32_MAX << (search & 31));

        if (candidates != 0) {
            uint8_t bit = ot_collision_lowest_set_bit(candidates);
            uint8_t candidate =
                (uint8_t)((word_index << 5) + bit);

            if (candidate >= OT_ENEMY_COUNT) return false;
            *enemy_index = candidate;
            *cursor = (uint8_t)(candidate + 1);
            return true;
        }
        search = (uint8_t)((word_index + 1) << 5);
    }
    return false;
}

static inline void ot_record_player_shot_collision_visits(
    OtLevelPortState *state,
    uint32_t visits
)
{
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    state->player_shot_collision_candidate_visit_count += visits;
#else
    (void)state;
    (void)visits;
#endif
}
#endif

IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) bool
ot_level_port_player_shot_overlaps(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y
)
{
    uint8_t index;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    uint8_t cursor = 0;
    uint32_t visits = 0;
#endif

    if (state == 0) return false;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    while (
        ot_next_player_shot_collision_candidate(
            state,
            &cursor,
            &index
        )
    ) {
        const OtEnemy *enemy;

        visits++;
#if !TYRIAN_GBA_COLLISION_MASK_FAST_PATH
        if (state->enemy_avail[index] != 0) continue;
#endif
        enemy = &state->enemy[index];
        if (
            (
                enemy->enemycycle == 0 &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    ),
                    25
                ) &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(enemy->ey - shot_y - 12)
                    ,
                    29
                )
            ) ||
            (
                enemy->enemycycle != 0 &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    ),
                    13
                ) &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(enemy->ey - shot_y - 6)
                    ,
                    15
                )
            )
        ) {
            ot_record_player_shot_collision_visits(state, visits);
            return true;
        }
    }
    ot_record_player_shot_collision_visits(state, visits);
#else
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemy *enemy;

        if (state->enemy_avail[index] != 0) continue;
        enemy = &state->enemy[index];
        if (
            (
                enemy->enemycycle == 0 &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    ),
                    25
                ) &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(enemy->ey - shot_y - 12)
                    ,
                    29
                )
            ) ||
            (
                enemy->enemycycle != 0 &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset - shot_x
                    ),
                    13
                ) &&
                ot_player_shot_axis_overlaps(
                    (int16_t)(enemy->ey - shot_y - 6)
                    ,
                    15
                )
            )
        ) {
            return true;
        }
    }
#endif
    return false;
}

static void ot_apply_player_shot_status(
    OtLevelPortState *state,
    uint8_t hit_index,
    uint8_t target_link,
    uint8_t blast_filter,
    bool iced
)
{
    OtEnemy *enemy = &state->enemy[hit_index];

    if (iced) enemy->iced = 40;
    if (enemy->enemyground) enemy->filter = blast_filter;
    if (target_link != 255) {
        uint8_t linked_index;

        for (
            linked_index = 0;
            linked_index < OT_ENEMY_COUNT;
            linked_index++
        ) {
            OtEnemy *linked = &state->enemy[linked_index];

            if (
                state->enemy_avail[linked_index] != 1 &&
                linked->enemyground &&
                linked->linknum == target_link
            ) {
                if (iced) linked->iced = 40;
                linked->filter = blast_filter;
            }
        }
    }
}

/*
 * Rare hit-side semantics stay in reviewed C: linked status propagation,
 * damaged-state transitions, death spawns, rewards and event jumps are much
 * larger than the dominant candidate/miss scan.  The hand-written ARM top
 * level calls this ABI-stable helper only after both axes overlap.
 */
ARM_CODE __attribute__((noinline, noclone)) uint32_t
ot_player_shot_collision_apply_hit_c(
    OtLevelPortState *state,
    uint32_t hit_index,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
)
{
#if TYRIAN_GBA_COLLISION_LAZY_RESULT
    uint8_t damage = (uint8_t)damage_and_radii;
#endif
    uint8_t blast_filter = (uint8_t)(
        ((damage_and_radii >> 24) & 0x0fu) << 4
    );
    bool iced = (damage_and_radii & ((uint32_t)1u << 28)) != 0;
    uint8_t index = (uint8_t)hit_index;
    OtEnemy *enemy = &state->enemy[index];
    uint16_t armor;
    uint8_t target_link;

#if TYRIAN_GBA_COLLISION_LAZY_RESULT
    if (!result->collided) {
        /* Misses promise collided=false; make every other field live here. */
        result->consumed = false;
        result->remaining_damage = damage;
        result->hit_count = 0;
        result->kill_count = 0;
        result->effect_count = 0;
        result->superpixel_hit_count = 0;
        result->data_cubes_awarded = 0;
        result->cash_awarded = 0;
        result->collided = true;
    }
#else
    result->collided = true;
#endif
    result->hit_count++;
    state->player_shot_collision_count++;
    armor = enemy->armorleft;
    target_link = enemy->linknum;
    if (target_link == 0) target_link = 255;
    ot_apply_player_shot_status(
        state,
        index,
        target_link,
        blast_filter,
        iced
    );
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

        state->frame_sound_queue[5] = 3;
        if (enemy->armorleft != 255) {
            enemy->armorleft =
                (uint8_t)(enemy->armorleft - damage_before);
            ot_add_hit_effect(
                result->effects,
                &result->effect_count,
                shot_x,
                shot_y,
                false,
                enemy->enemyground,
                0
            );
        } else if (result->superpixel_hit_count != 255u) {
            /* tyrian2.c uses JE_doSP(), not a hit explosion, here. */
            result->superpixel_hit_count++;
        }
        if (
            (int16_t)armor - damage_before <= enemy->edlevel &&
            ((!enemy->edamaged) ^ (enemy->edani < 0))
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
        return 1;
    }
    result->remaining_damage =
        (uint8_t)(result->remaining_damage - armor);
    return 0;
}

#if TYRIAN_GBA_HOTPATH_ASM
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
#define OT_PACKED_C_REFERENCE_NAME \
    ot_level_port_collide_player_shot_packed_c_reference
#define OT_PACKED_C_REFERENCE_ATTRIBUTES \
    ARM_CODE __attribute__((noinline, noclone))
#define OT_BUILD_PACKED_C_REFERENCE 1
#else
#define OT_BUILD_PACKED_C_REFERENCE 0
#endif
#else
#define OT_PACKED_C_REFERENCE_NAME \
    ot_level_port_collide_player_shot_packed
#define OT_PACKED_C_REFERENCE_ATTRIBUTES \
    IWRAM_CODE ARM_CODE __attribute__((noinline, noclone))
#define OT_BUILD_PACKED_C_REFERENCE 1
#endif

#if OT_BUILD_PACKED_C_REFERENCE
OT_PACKED_C_REFERENCE_ATTRIBUTES void
OT_PACKED_C_REFERENCE_NAME(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
)
{
    uint8_t damage = (uint8_t)damage_and_radii;
    uint8_t radius_w = (uint8_t)(damage_and_radii >> 8);
    uint8_t radius_h = (uint8_t)(damage_and_radii >> 16);
    uint8_t index;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    uint8_t cursor = 0;
    uint32_t visits = 0;
#endif

    if (result == 0) return;
    result->collided = false;
#if !TYRIAN_GBA_COLLISION_LAZY_RESULT
    result->consumed = false;
    result->remaining_damage = damage;
    result->hit_count = 0;
    result->kill_count = 0;
    result->effect_count = 0;
    result->superpixel_hit_count = 0;
    result->data_cubes_awarded = 0;
    result->cash_awarded = 0;
#else
    (void)damage;
#endif
    if (state == 0) return;

#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    while (
        ot_next_player_shot_collision_candidate(
            state,
            &cursor,
            &index
        )
    )
#else
    for (index = 0; index < OT_ENEMY_COUNT; index++)
#endif
    {
        OtEnemy *enemy;
        bool collided;

#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
        visits++;
#endif
#if !TYRIAN_GBA_COLLISION_MASK_FAST_PATH
        if (state->enemy_avail[index] != 0) continue;
#endif
        enemy = &state->enemy[index];
        if (enemy->enemycycle == 0) {
            collided =
                ot_player_shot_axis_overlaps_c_inline(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset -
                            shot_x - radius_w
                    ),
                    (uint16_t)(25u + radius_w)
                ) &&
                ot_player_shot_axis_overlaps_c_inline(
                    (int16_t)(
                        enemy->ey - shot_y - 12 - radius_h
                    ),
                    (uint16_t)(29u + radius_h)
                );
        } else {
            collided =
                ot_player_shot_axis_overlaps_c_inline(
                    (int16_t)(
                        enemy->ex + enemy->mapoffset -
                            shot_x - radius_w
                    ),
                    (uint16_t)(13u + radius_w)
                ) &&
                ot_player_shot_axis_overlaps_c_inline(
                    (int16_t)(
                        enemy->ey - shot_y - 6 - radius_h
                    ),
                    (uint16_t)(15u + radius_h)
                );
        }
        if (!collided) continue;
        if (
            ot_player_shot_collision_apply_hit_c(
                state,
                index,
                shot_x,
                shot_y,
                result,
                damage_and_radii
            )
        ) {
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
            ot_record_player_shot_collision_visits(state, visits);
#endif
            return;
        }
    }
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    ot_record_player_shot_collision_visits(state, visits);
#endif
}
#endif

#undef OT_BUILD_PACKED_C_REFERENCE
#undef OT_PACKED_C_REFERENCE_ATTRIBUTES
#undef OT_PACKED_C_REFERENCE_NAME

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
#if TYRIAN_GBA_HOTPATH_ASM
extern IWRAM_CODE ARM_CODE bool ot_player_shot_axis_overlaps_asm(
    int16_t delta,
    uint16_t radius
);
extern IWRAM_CODE ARM_CODE bool
ot_player_shot_axis_overlaps_unsigned_asm(
    int16_t delta,
    uint16_t radius
);
#if TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
#define OT_COLLISION_TEST_AXIS_ASM \
    ot_player_shot_axis_overlaps_unsigned_asm
#else
#define OT_COLLISION_TEST_AXIS_ASM ot_player_shot_axis_overlaps_asm
#endif
extern IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
extern IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_instrumented_asm(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    OtShotCollisionResult *result,
    uint32_t damage_and_radii
);
extern IWRAM_CODE ARM_CODE void
ot_level_port_collide_player_shot_packed_generic_asm(
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
#define OT_COLLISION_TEST_PACKED_ASM \
    ot_level_port_collide_player_shot_packed_instrumented_asm
#else
#define OT_COLLISION_TEST_PACKED_ASM \
    ot_level_port_collide_player_shot_packed_generic_asm
#endif

static uint32_t ot_collision_test_hash(
    uint32_t hash,
    const void *data,
    uint32_t size
)
{
    const uint8_t *bytes = (const uint8_t *)data;

    while (size-- != 0) {
        hash ^= *bytes++;
        hash *= 16777619u;
    }
    return hash;
}

static void ot_collision_test_fill(
    void *data,
    uint8_t value,
    uint32_t size
)
{
    uint8_t *bytes = (uint8_t *)data;

    while (size-- != 0) *bytes++ = value;
}

static OtEnemy *ot_collision_test_enemy(
    OtLevelPortState *state,
    uint8_t index,
    int16_t x,
    int16_t y,
    uint8_t armor,
    uint8_t cycle
)
{
    OtEnemy *enemy = &state->enemy[index];

    state->enemy_avail[index] = 0;
    state->active_enemy_count++;
    enemy->ex = x;
    enemy->ey = y;
    enemy->armorleft = armor;
    enemy->enemycycle = cycle;
    enemy->edlevel = -100;
    return enemy;
}

static uint32_t ot_collision_test_prepare(
    OtLevelPortState *state,
    uint8_t test_case
)
{
    uint8_t index;
    uint32_t packed = 3u;
    OtEnemy *enemy;

    ot_collision_test_fill(state, 0, sizeof(*state));
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        state->enemy_avail[index] = 1;
    }
    state->assets_valid = true;

    switch (test_case) {
    case 0: /* dominant miss path */
        ot_collision_test_enemy(state, 7, 220, 150, 10, 0);
        break;
    case 1: /* ordinary armor subtraction + hit effect */
        ot_collision_test_enemy(state, 3, 100, 92, 10, 0);
        break;
    case 2: /* indestructible 255 armor -> superpixel feedback */
        ot_collision_test_enemy(state, 31, 100, 86, 255, 1);
        break;
    case 3: /* penetrating damage across ascending live-mask slots */
        ot_collision_test_enemy(state, 2, 100, 86, 2, 1);
        ot_collision_test_enemy(state, 8, 100, 86, 2, 1);
        ot_collision_test_enemy(state, 20, 100, 86, 5, 1);
        packed = 5u;
        break;
    case 4: /* linked group death mutates later active-mask candidates */
        enemy = ot_collision_test_enemy(state, 10, 100, 92, 1, 0);
        enemy->linknum = 42;
        enemy = ot_collision_test_enemy(state, 12, 104, 92, 1, 0);
        enemy->linknum = 42;
        packed = 2u;
        break;
    case 5: /* authored damaged-graphic transition and linked member */
        enemy = ot_collision_test_enemy(state, 1, 100, 92, 10, 0);
        enemy->linknum = 9;
        enemy->edlevel = 6;
        enemy->edgr = 77;
        enemy = ot_collision_test_enemy(state, 4, 180, 140, 12, 0);
        enemy->linknum = 9;
        enemy->edlevel = 8;
        enemy->edgr = 81;
        packed = 4u;
        break;
    case 6: /* iced/filter status propagates to a linked ground remnant */
        enemy = ot_collision_test_enemy(state, 5, 100, 92, 20, 0);
        enemy->linknum = 7;
        enemy->enemyground = true;
        enemy = &state->enemy[6];
        state->enemy_avail[6] = 2;
        enemy->linknum = 7;
        enemy->enemyground = true;
        packed = 2u | (10u << 24) | (1u << 28);
        break;
    case 7: /* exact strict-radius boundary with non-zero shot size */
        ot_collision_test_enemy(state, 63, 128, 104, 9, 0);
        packed = 3u | (4u << 8) | (3u << 16);
        break;
    case 8: /* dlevel=-1 fixed remnant, cube/cash and global flag */
        enemy = ot_collision_test_enemy(state, 15, 100, 92, 1, 0);
        enemy->linknum = 33;
        enemy->edlevel = -1;
        enemy->edgr = 123;
        enemy->evalue = 100;
        enemy->special = true;
        enemy->flagnum = 3;
        enemy->setto = true;
        packed = 1u;
        break;
    default:
        break;
    }
    ot_level_port_begin_player_shot_collision_phase(state);
    return packed;
}

static bool ot_collision_test_one_case(
    OtLevelPortState *state,
    uint8_t test_case
)
{
    OtShotCollisionResult result;
    uint32_t packed;
    uint32_t c_hash;
    uint32_t asm_hash;

    packed = ot_collision_test_prepare(state, test_case);
    ot_collision_test_fill(&result, 0xa5, sizeof(result));
    ot_level_port_collide_player_shot_packed_c_reference(
        state,
        100,
        80,
        &result,
        packed
    );
    c_hash = ot_collision_test_hash(
        2166136261u,
        state,
        sizeof(*state)
    );
    c_hash = ot_collision_test_hash(c_hash, &result, sizeof(result));

    packed = ot_collision_test_prepare(state, test_case);
    ot_collision_test_fill(&result, 0xa5, sizeof(result));
    OT_COLLISION_TEST_PACKED_ASM(
        state,
        100,
        80,
        &result,
        packed
    );
    asm_hash = ot_collision_test_hash(
        2166136261u,
        state,
        sizeof(*state)
    );
    asm_hash = ot_collision_test_hash(asm_hash, &result, sizeof(result));
    return c_hash == asm_hash;
}

static uint32_t ot_collision_test_random_next(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return *seed;
}

static uint32_t ot_collision_test_prepare_random(
    OtLevelPortState *state,
    uint32_t seed,
    int16_t *shot_x,
    int16_t *shot_y
)
{
    uint8_t desired;
    uint8_t created = 0;
    uint8_t index;

    ot_collision_test_fill(state, 0, sizeof(*state));
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        state->enemy_avail[index] = 1;
    }
    state->assets_valid = true;
    desired = (uint8_t)(1u + (ot_collision_test_random_next(&seed) % 30u));
    while (created < desired) {
        OtEnemy *enemy;
        uint32_t value = ot_collision_test_random_next(&seed);

        index = (uint8_t)(value % OT_ENEMY_COUNT);
        if (state->enemy_avail[index] == 0) continue;
        enemy = ot_collision_test_enemy(
            state,
            index,
            (int16_t)(20 + (ot_collision_test_random_next(&seed) % 280u)),
            (int16_t)(10 + (ot_collision_test_random_next(&seed) % 190u)),
            (uint8_t)(1u + (ot_collision_test_random_next(&seed) & 31u)),
            (uint8_t)(ot_collision_test_random_next(&seed) & 1u)
        );
        if ((value & 31u) == 0) enemy->armorleft = 255;
        enemy->mapoffset = (uint16_t)(value & 15u);
        enemy->enemyground = (value & 0x40u) != 0;
        enemy->size = (uint8_t)((value >> 7) & 1u);
        enemy->explonum = (uint8_t)((value >> 8) & 7u);
        if ((value & 3u) == 0) {
            enemy->linknum = (uint8_t)(1u + ((value >> 10) % 7u));
        }
        if ((value & 0x10000u) != 0) {
            enemy->evalue = (value & 0x20000u) != 0 ? 1 : 100;
        }
        if ((value & 0x40000u) != 0) {
            enemy->special = true;
            enemy->flagnum = (uint8_t)(1u + ((value >> 19) % 10u));
            enemy->setto = (value & 0x1000000u) != 0;
        }
        created++;
    }
    *shot_x = (int16_t)(20 + (ot_collision_test_random_next(&seed) % 280u));
    *shot_y = (int16_t)(10 + (ot_collision_test_random_next(&seed) % 190u));
    ot_level_port_begin_player_shot_collision_phase(state);
    return
        (1u + (ot_collision_test_random_next(&seed) & 63u)) |
        ((ot_collision_test_random_next(&seed) & 15u) << 8) |
        ((ot_collision_test_random_next(&seed) & 15u) << 16) |
        ((ot_collision_test_random_next(&seed) & 31u) << 24);
}

static bool ot_collision_test_random_case(
    OtLevelPortState *state,
    uint32_t seed
)
{
    OtShotCollisionResult result;
    int16_t shot_x;
    int16_t shot_y;
    uint32_t packed;
    uint32_t c_hash;
    uint32_t asm_hash;

    packed = ot_collision_test_prepare_random(
        state,
        seed,
        &shot_x,
        &shot_y
    );
    ot_collision_test_fill(&result, 0x3c, sizeof(result));
    ot_level_port_collide_player_shot_packed_c_reference(
        state,
        shot_x,
        shot_y,
        &result,
        packed
    );
    c_hash = ot_collision_test_hash(
        2166136261u,
        state,
        sizeof(*state)
    );
    c_hash = ot_collision_test_hash(c_hash, &result, sizeof(result));

    packed = ot_collision_test_prepare_random(
        state,
        seed,
        &shot_x,
        &shot_y
    );
    ot_collision_test_fill(&result, 0x3c, sizeof(result));
    OT_COLLISION_TEST_PACKED_ASM(
        state,
        shot_x,
        shot_y,
        &result,
        packed
    );
    asm_hash = ot_collision_test_hash(
        2166136261u,
        state,
        sizeof(*state)
    );
    asm_hash = ot_collision_test_hash(asm_hash, &result, sizeof(result));
    return c_hash == asm_hash;
}
#endif

uint32_t ot_level_port_hotpath_asm_differential_test(
    OtLevelPortState *state
)
{
#if TYRIAN_GBA_HOTPATH_ASM
    static const uint16_t radius[] = {
        0, 1, 2, 13, 15, 25, 29, 255, 280,
        32767, 32768u, 65535u,
    };
    uint32_t result = 0;
    uint32_t radius_index;
    uint32_t random_case;
    int32_t delta;
    uint8_t test_case;

    for (radius_index = 0;
         radius_index < sizeof(radius) / sizeof(radius[0]);
         radius_index++) {
        for (delta = -32768; delta <= 32767; delta++) {
            bool c_value = ot_player_shot_axis_overlaps_c_reference(
                (int16_t)delta,
                radius[radius_index]
            );
            bool asm_value = OT_COLLISION_TEST_AXIS_ASM(
                (int16_t)delta,
                radius[radius_index]
            );

            if (c_value != asm_value) return result;
        }
    }
    result |= 1u;
    if (state == 0) return result;
    for (test_case = 0; test_case < 9; test_case++) {
        if (!ot_collision_test_one_case(state, test_case)) {
            return 0x100u | test_case;
        }
    }
    for (random_case = 0; random_case < 128u; random_case++) {
        if (
            !ot_collision_test_random_case(
                state,
                0xc001d00du ^ (random_case * 0x9e3779b9u)
            )
        ) {
            return 0x200u | random_case;
        }
    }
    result |= 2u;
    return result;
#else
    (void)state;
    return 3u;
#endif
}
#if TYRIAN_GBA_HOTPATH_ASM
#undef OT_COLLISION_TEST_AXIS_ASM
#endif
#endif

void ot_level_port_collide_zinglon_beam(
    OtLevelPortState *state,
    int16_t beam_x,
    uint8_t half_width,
    OtShotCollisionResult *result
)
{
    uint8_t index;

    if (result == 0) return;
    result->collided = false;
    result->consumed = false;
    result->remaining_damage = 10;
    result->hit_count = 0;
    result->kill_count = 0;
    result->effect_count = 0;
    result->superpixel_hit_count = 0;
    result->data_cubes_awarded = 0;
    result->cash_awarded = 0;
    if (state == 0 || half_width == 0) return;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        OtEnemy *enemy;
        int16_t delta_x;
        uint16_t armor;
        uint8_t target_link;

        if (state->enemy_avail[index] != 0) continue;
        enemy = &state->enemy[index];
        delta_x = (int16_t)(
            enemy->ex + enemy->mapoffset - beam_x
        );
        if (delta_x < 0) delta_x = (int16_t)-delta_x;
        if ((uint16_t)delta_x >= half_width) continue;

        result->collided = true;
        result->hit_count++;
        state->player_shot_collision_count++;
        armor = enemy->armorleft;
        target_link = enemy->linknum;
        if (target_link == 0) target_link = 255;
        ot_apply_player_shot_status(
            state,
            index,
            target_link,
            9,
            false
        );
        for (uint8_t bar = 0; bar < 2; bar++) {
            if (
                target_link != 255 &&
                target_link == state->boss_bar_link[bar]
            ) {
                state->boss_bar_color[bar] = 6;
            }
        }
        if (armor > 10) {
            state->frame_sound_queue[5] = 3;
            /*
             * tyrian2.c deliberately skips armor subtraction for the
             * synthetic MAX_PWEAPON-1 Zinglon slot.  It destroys targets at
             * ten armor or below and may trigger an authored damaged-state
             * threshold, but it does not chip stronger armor each pulse.
             */
            if (
                (int16_t)armor - 10 <= enemy->edlevel &&
                ((!enemy->edamaged) ^ (enemy->edani < 0))
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
    }
}

IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
ot_level_port_collide_player_shot_sized(
    OtLevelPortState *state,
    int16_t shot_x,
    int16_t shot_y,
    uint8_t damage,
    uint8_t radius_w,
    uint8_t radius_h,
    OtShotCollisionResult *result
)
{
    ot_level_port_collide_player_shot_packed(
        state,
        shot_x,
        shot_y,
        result,
        (uint32_t)damage |
            ((uint32_t)radius_w << 8) |
            ((uint32_t)radius_h << 16)
    );
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
    ot_level_port_collide_player_shot_packed(
        state,
        shot_x,
        shot_y,
        result,
        damage
    );
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

void ot_level_port_recalculate_player_power_progress(
    OtLevelPortState *state
)
{
    if (state != 0) ot_recalculate_purple_balls(state);
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
     * shotMultiPos reset with source-state ownership in this GBA port.
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

/*
 * varz.c specialArcadeWeapon[PORT_NUM], indexed by the one-based front-port
 * pickup ID minus one.  This is source gameplay data, kept here only because
 * the table is compiled into OpenTyrian rather than stored in tyrian.hdt.
 */
static const uint8_t ot_special_arcade_weapon[OT_HDT_PORT_COUNT - 1] = {
    17, 17, 18, 0, 0, 0, 10, 0, 0, 0, 0, 0, 44, 0,
    10, 0, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 45, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * varz.c SAWeapon[SA][5].  Unlike ordinary Arcade mode, the seven hidden
 * Super Arcade ships translate the five coloured evalue pickups through
 * this ship-specific table before assigning the front weapon.
 */
static const uint8_t ot_super_arcade_weapon[7][5] = {
    {  9, 31, 32, 33, 34 },
    { 19,  8, 22, 41, 34 },
    { 27,  5, 20, 42, 31 },
    { 15,  3, 28, 22, 12 },
    { 23, 35, 25, 14,  6 },
    {  2,  5, 21,  4,  7 },
    { 40, 38, 37, 41, 36 },
};

void ot_level_port_collide_player(
    OtLevelPortState *state,
    bool player_vulnerable,
    OtPlayerCollisionResult *result
)
{
    uint8_t index;

    if (result == 0) return;
    /*
     * As above, the effect arrays are length-delimited.  Initialize only
     * their counts and the scalar result fields instead of clearing both
     * arrays every tick.
     */
    result->pickup_count = 0;
    result->contact_count = 0;
    result->damage = 0;
    result->player_velocity_x_delta = 0;
    result->player_velocity_y_delta = 0;
    result->effect_count = 0;
    result->data_cubes_awarded = 0;
    result->front_powerups = 0;
    result->rear_powerups = 0;
    result->orbiting_asteroids_awarded = 0;
    result->superbombs_awarded = 0;
    result->pickup_message_type = OT_PICKUP_MESSAGE_NONE;
    result->pickup_message_item_id = 0;
    result->reset_all_shot_multi_pos = false;
    result->front_weapon_picked_up = false;
    result->special_weapon_picked_up = false;
    result->bonus_level_triggered = false;
    result->next_level = 0;
    result->cash_awarded = 0;
    result->pickup_effect_count = 0;
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
                state->frame_sound_queue[7] = 29;
                consumed = true;
            } else if (
                state->player_super_arcade_mode >= 1 &&
                state->player_super_arcade_mode <= 7 &&
                value >= 30001 &&
                value <= 30005
            ) {
                uint8_t weapon_id = ot_super_arcade_weapon[
                    state->player_super_arcade_mode - 1u
                ][value - 30001];

                /* Direct mainint.c superArcadeMode pickup branch. */
                result->front_weapon_picked_up = true;
                if (weapon_id == state->player_front_weapon_id) {
                    result->cash_awarded += 1000;
                    (void)ot_power_up_weapon(state, true, result);
                } else {
                    ot_handle_purple_ball(state, result);
                }
                state->player_front_weapon_id = weapon_id;
                result->cash_awarded += 200;
                state->high_value_pickup_count++;
                state->frame_sound_queue[7] = 29;
                consumed = true;
            } else if (value > 32100) {
                result->cash_awarded += 250;
                state->player_special = (uint8_t)(value - 32100);
                result->pickup_message_type =
                    OT_PICKUP_MESSAGE_SPECIAL;
                result->pickup_message_item_id =
                    state->player_special;
                result->special_weapon_picked_up = true;
                state->high_value_pickup_count++;
                state->frame_sound_queue[7] = 29;
                consumed = true;
            } else if (value > 32000) {
                if (state->arcade_mode) {
                    uint8_t option_id = (uint8_t)(value - 32000);

                    /*
                     * mainint.c onePlayerAction branch assigns the same
                     * option to LEFT_SIDEKICK and RIGHT_SIDEKICK.
                     */
                    state->player_sidekick[0] = option_id;
                    state->player_sidekick[1] = option_id;
                    result->pickup_message_type =
                        OT_PICKUP_MESSAGE_OPTION;
                    result->pickup_message_item_id = option_id;
                    result->cash_awarded += 250;
                    state->high_value_pickup_count++;
                    state->frame_sound_queue[7] = 29;
                    consumed = true;
                }
            } else if (value > 31000) {
                /*
                 * Preserve the source's unconditional 250-credit award
                 * before its player-2/onePlayerAction ownership checks.
                 */
                result->cash_awarded += 250;
                if (state->arcade_mode) {
                    state->player_rear_weapon_id =
                        (uint8_t)(value - 31000);
                    result->pickup_message_type =
                        OT_PICKUP_MESSAGE_WEAPON_PORT;
                    result->pickup_message_item_id =
                        state->player_rear_weapon_id;
                    if (state->player_rear_weapon_power == 0) {
                        state->player_rear_weapon_power = 1;
                    }
                    state->high_value_pickup_count++;
                    state->frame_sound_queue[7] = 29;
                    consumed = true;
                }
            } else {
                if (state->arcade_mode) {
                    uint8_t port_id = (uint8_t)(value - 30000);

                    state->player_front_weapon_id = port_id;
                    result->pickup_message_type =
                        OT_PICKUP_MESSAGE_WEAPON_PORT;
                    result->pickup_message_item_id = port_id;
                    if (
                        port_id > 0 &&
                        port_id <=
                            sizeof(ot_special_arcade_weapon)
                    ) {
                        state->player_special =
                            ot_special_arcade_weapon[port_id - 1u];
                    } else {
                        state->player_special = 0;
                    }
                    result->cash_awarded += 250;
                    state->high_value_pickup_count++;
                    state->frame_sound_queue[7] = 29;
                    consumed = true;
                }
            }
        } else if (value > 20000) {
            suppress_contact = true;
            uint16_t armor = (uint16_t)(
                state->player_armor + value - 20000
            );

            state->player_armor = armor > 28 ? 28 : (uint8_t)armor;
            state->armor_pickup_count++;
            state->frame_sound_queue[7] = 29;
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
            state->frame_sound_queue[7] = 18;
            if (value == 1) {
                state->data_cube_pickup_count++;
                result->data_cubes_awarded++;
                state->frame_sound_queue[3] = 37;
            } else if (value == -1) {
                state->front_weapon_powerup_count++;
                result->front_powerups++;
                result->pickup_message_type =
                    OT_PICKUP_MESSAGE_FRONT_POWER;
                (void)ot_power_up_weapon(state, true, result);
                state->frame_sound_queue[7] = 29;
            } else if (value == -2) {
                state->rear_weapon_powerup_count++;
                result->rear_powerups++;
                result->pickup_message_type =
                    OT_PICKUP_MESSAGE_REAR_POWER;
                (void)ot_power_up_weapon(state, false, result);
                state->frame_sound_queue[7] = 29;
            } else if (value == -3) {
                state->orbiting_asteroid_pickup_count++;
                result->orbiting_asteroids_awarded++;
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
                result->reset_all_shot_multi_pos = true;
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
            uint16_t collision_armor = enemy->armorleft;
            uint8_t target_link = enemy->linknum;

            /* Direct mainint.c:JE_playerCollide() contact-damage cap. */
            if (damage > state->damage_rate) {
                damage = state->damage_rate;
            }
            state->player_enemy_contact_count++;
            result->contact_count++;
            result->damage = (uint16_t)(result->damage + damage);

            /*
             * The source applies this before its movement/friction clamp and
             * uses the original armour, including the authored 255 sentinel.
             */
            if (enemy->armorleft > 0) {
                result->player_velocity_x_delta +=
                    ((int32_t)enemy->exc * enemy->armorleft) / 2;
                result->player_velocity_y_delta +=
                    ((int32_t)enemy->eyc * enemy->armorleft) / 2;
            }

            if (collision_armor == 255) collision_armor = 30000;
            if (target_link == 0) target_link = 255;
            if (collision_armor > damage) {
                if (enemy->armorleft != 255) {
                    enemy->armorleft =
                        (uint8_t)(enemy->armorleft - damage);
                }
                state->frame_sound_queue[5] = 3; /* S_ENEMY_HIT */
            } else {
                uint8_t linked_index;

                /*
                 * Literal single-player contact-death group predicate from
                 * mainint.c.  This intentionally differs from the player-shot
                 * path: contact deaths do not award cash, spawn enemydie, set
                 * special flags, or invoke the link-254 event jump.
                 */
                for (
                    linked_index = 0;
                    linked_index < OT_ENEMY_COUNT;
                    linked_index++
                ) {
                    OtEnemy *linked;
                    uint8_t linked_link;
                    bool matches;

                    if (state->enemy_avail[linked_index] == 1) continue;
                    linked = &state->enemy[linked_index];
                    linked_link = linked->linknum;
                    matches =
                        linked_index == index ||
                        (
                            target_link != 255 &&
                            (
                                target_link == linked_link ||
                                (int16_t)target_link - 100 == linked_link ||
                                (
                                    linked_link > 40 &&
                                    linked_link / 20 == target_link / 20 &&
                                    linked_link <= target_link
                                )
                            )
                        );
                    if (!matches) continue;

                    ot_add_hit_effect(
                        result->effects,
                        &result->effect_count,
                        (int16_t)(linked->ex + linked->mapoffset),
                        linked->ey,
                        linked->size == 1,
                        linked->enemyground,
                        linked->size == 1 ? linked->explonum : 0
                    );
                    linked->linknum = 0;
                    if (linked->size == 1) {
                        state->frame_sound_queue[6] = 9;
                    } else {
                        state->frame_sound_queue[5] = 4;
                    }
                    ot_release_enemy(state, linked_index);
                }
            }
        }
    }
}

/*
 * This scan runs every logic tick, but its callback-heavy ARM form consumed
 * 1.25 KiB of the 32 KiB IWRAM while the EP2 stress trace updated only 74
 * active enemy shots.  Keep it as compact Thumb ROM code: the measured gain
 * from placement is smaller than the IWRAM needed by the source-parity stack,
 * Sprite2 cache path, background decoder and Maxmod mixer.
 */
__attribute__((noinline, noclone)) void
ot_level_port_update_enemy_shots(
    OtLevelPortState *state,
    OtEnemyShotImpactHandler impact_handler,
    void *impact_context
)
{
    uint8_t index;
    bool player_can_collide = true;

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
            player_can_collide &&
            shot->sx >
                state->player_x - state->player_shot_hit_area_x &&
            shot->sx <
                state->player_x + state->player_shot_hit_area_x &&
            shot->sy >
                state->player_y - state->player_shot_hit_area_y &&
            shot->sy <
                state->player_y + state->player_shot_hit_area_y
        ) {
            OtEnemyShotImpact impact;

            state->frame_player_damage =
                (uint16_t)(state->frame_player_damage + shot->sdmg);
            impact.x = shot->sx;
            impact.y = shot->sy;
            impact.velocity_x = shot->sxm;
            impact.velocity_y = shot->sym;
            impact.damage = shot->sdmg;
            ot_release_enemy_shot(state, shot);
            state->enemy_shot_player_hit_count++;
            if (impact_handler != 0) {
                player_can_collide = impact_handler(
                    impact_context,
                    &impact
                );
            }
            continue;
        }
        if (shot->animax != 0) {
            shot->animate++;
            if (shot->animate >= shot->animax) shot->animate = 0;
        }
    }
}
