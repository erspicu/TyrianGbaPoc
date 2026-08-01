#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static uint32_t ot_round_ratio_call_count;

uint32_t ot_level_port_stress_round_ratio_call_count(void)
{
    return ot_round_ratio_call_count;
}
#endif

static int16_t ot_round_ratio(
    int32_t numerator,
    int16_t denominator
)
{
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    ot_round_ratio_call_count++;
#endif
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
    ot_player_shot_collision_mask_set(state, enemy_index, false);
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
        *control = (int8_t)-*control;
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

static void ot_queue_frame_explosion(
    OtLevelPortState *state,
    int16_t x,
    int16_t y,
    int8_t delta_y,
    uint8_t explosion_type,
    bool fixed_position
)
{
    OtFrameExplosion *request;

    if (state->frame_explosion_count >= OT_FRAME_EXPLOSION_COUNT) {
        return;
    }
    request = &state->frame_explosion[state->frame_explosion_count++];
    request->x = x;
    request->y = y;
    request->delta_y = delta_y;
    request->explosion_type = explosion_type;
    request->fixed_position = fixed_position;
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
        if (state->difficulty_level > 2) {
            enemy->eshotwait[(uint8_t)slot] = (uint8_t)(
                enemy->eshotwait[(uint8_t)slot] / 2u + 1u
            );
            if (state->difficulty_level > 7) {
                enemy->eshotwait[(uint8_t)slot] = (uint8_t)(
                    enemy->eshotwait[(uint8_t)slot] / 2u + 1u
                );
            }
        }

        /* Direct tyrian2.c JE_drawEnemy() control-opcode switch. */
        if (turret >= 251) {
            int16_t source_player_x_minus_25 =
                (int16_t)(state->player_x - 25);
            int16_t distance;
            int16_t force;

            switch (turret) {
            case 252: /* Savara Boss DualMissile */
                if (enemy->ey > 20) {
                    int16_t explosion_y = (int16_t)(
                        enemy->ey - 20 - state->back_move * 8
                    );

                    ot_queue_frame_explosion(
                        state,
                        (int16_t)(enemy->ex - 8 + enemy->mapoffset),
                        explosion_y,
                        -2,
                        6,
                        false
                    );
                    ot_queue_frame_explosion(
                        state,
                        (int16_t)(enemy->ex + 4 + enemy->mapoffset),
                        explosion_y,
                        -2,
                        6,
                        false
                    );
                }
                break;

            case 251: /* Suck-O-Magnet */
                distance = (int16_t)(
                    ot_abs_s16((int16_t)(
                        source_player_x_minus_25 - enemy->ex
                    )) +
                    ot_abs_s16((int16_t)(state->player_y - enemy->ey))
                );
                force = (int16_t)(4 - distance / 100);
                if (force > 0) {
                    state->frame_player_velocity_x_delta +=
                        source_player_x_minus_25 > enemy->ex ?
                            -force : force;
                }
                break;

            case 253: /* Rightward short-range magnet */
                if (
                    ot_abs_s16((int16_t)(
                        state->player_x - 14 - enemy->ex
                    )) < 24 &&
                    ot_abs_s16((int16_t)(
                        state->player_y - enemy->ey
                    )) < 28
                ) {
                    state->frame_player_velocity_x_delta += 2;
                }
                break;

            case 254: /* Leftward short-range magnet */
                if (
                    ot_abs_s16((int16_t)(
                        state->player_x - 14 - enemy->ex
                    )) < 24 &&
                    ot_abs_s16((int16_t)(
                        state->player_y - enemy->ey
                    )) < 28
                ) {
                    state->frame_player_velocity_x_delta -= 2;
                }
                break;

            case 255: /* Magneto RePulse */
                if (state->difficulty_level != 1) {
                    if (slot == 2) {
                        enemy->filter = 0x70;
                    } else {
                        distance = (int16_t)(
                            ot_abs_s16((int16_t)(
                                source_player_x_minus_25 - enemy->ex
                            )) +
                            ot_abs_s16((int16_t)(
                                state->player_y - enemy->ey
                            ))
                        );
                        force = (int16_t)(4 - distance / 20);
                        if (force > 0) {
                            state->frame_player_velocity_x_delta +=
                                source_player_x_minus_25 > enemy->ex ?
                                    force : -force;
                        }
                    }
                }
                break;
            }
            continue;
        }
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
                    state->frame_sound_queue[sound_slot] = weapon.sound;
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
                    uint8_t aim = weapon.aim;

                    /*
                     * JE_drawEnemy temporarily subtracts 25 from player.x;
                     * its later +25 therefore cancels.  state->player_x is
                     * kept unmodified, leaving the source expression below.
                     */
                    if (state->difficulty_level > 2) {
                        aim = (uint8_t)(
                            aim + state->difficulty_level - 2u
                        );
                    }
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
                        (int32_t)aim_x * aim,
                        magnitude
                    );
                    shot->sym = ot_round_ratio(
                        (int32_t)aim_y * aim,
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
    {
        static const uint8_t random_enemy_launch_sounds[3] = {
            13, 6, 26,
        };
        uint32_t sound_index = ot_mt_rand(state) % 3u;

        state->frame_sound_queue[sound_slot] =
            random_enemy_launch_sounds[sound_index];
    }
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
            enemy->ex <= -24 ||
            enemy->ex >= 296
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
    if (enemy_id == 2) {
        state->frame_sound_queue[3] = 7; /* S_WEAPON_7 */
    }
    ot_new_enemy(state, 0, enemy_id, 0, OT_SPAWN_RANDOM);
}

static void ot_mark_stopped_enemy_group_clear(
    OtLevelPortState *state,
    uint8_t group
)
{
    if (state->stop_background_num != group) return;
    if (
        (group == 1 && state->frame_ground_enemy_on_screen == 0) ||
        (group == 2 && state->frame_sky_enemy_on_screen == 0)
    ) {
        state->stop_background_num = 9;
    }
}

static void ot_advance_enemies_before_player(OtLevelPortState *state)
{
    /*
     * Direct JE_main() phase order before player shots/movement:
     * ground, ground2, continual, then only the sky/top layers which are
     * configured behind the player.  The over-player variants are advanced
     * later by ot_level_port_advance_over_player_enemies().
     */
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
    ot_mark_stopped_enemy_group_clear(state, 1);
    ot_spawn_continual_enemy(state);
    if (!state->sky_enemy_over_all) {
        ot_draw_enemy_pool(
            state,
            0,
            0,
            state->presentation_map_x2_offset
        );
        ot_mark_stopped_enemy_group_clear(state, 2);
    }
    if (!state->top_enemy_over) {
        ot_draw_enemy_pool(
            state,
            50,
            (int16_t)state->back_move3,
            state->presentation_map_x3_offset
        );
    }
}

static void ot_advance_enemies_over_player(OtLevelPortState *state)
{
    /* Direct JE_main() order after existing enemy-shot movement. */
    if (state->top_enemy_over) {
        ot_draw_enemy_pool(
            state,
            50,
            (int16_t)state->back_move3,
            state->presentation_map_x3_offset
        );
    }
    if (state->sky_enemy_over_all) {
        ot_draw_enemy_pool(
            state,
            0,
            0,
            state->presentation_map_x2_offset
        );
        ot_mark_stopped_enemy_group_clear(state, 2);
    }
}
