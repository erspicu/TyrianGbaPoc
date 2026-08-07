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
            state->event_jump_count++;
            return true;
        }
    }
    state->event_index = state->event_count;
    state->event_jump_count++;
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
        ot_level_port_set_enemy_avail(state, index, 1);
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
        if (state->all_players_gone || event->eventdat == 1) {
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
                state->enemy[index].armorleft = (uint8_t)(
                    state->galaga_mode ?
                        (uint16_t)event->eventdat *
                            (uint16_t)(state->difficulty_level / 2u) :
                        (uint16_t)event->eventdat
                );
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
        uint8_t lives = state->player_front_weapon_power;

        state->death_control_event_count++;
        if (
            state->arcade_mode &&
            (event->eventdat == 512 || event->eventdat == 513)
        ) {
            return true;
        }
        /*
         * superTyrian is intentionally unsupported, but superArcadeMode and
         * the source lives alias (front-weapon power) are real campaign data.
         */
        if (state->player_super_arcade_mode != 0) {
            if (event->eventdat == 534) enemy_die = 827;
        } else if (
            event->eventdat == 533 &&
            (
                lives == 11 ||
                (ot_mt_rand(state) % 15u) < lives
            )
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
        state->music_fade_volume = 255;
        return true;

    case 35:
        state->current_song = (int16_t)(event->eventdat - 1);
        state->music_fade = false;
        state->music_fade_volume = 255;
        state->frame_music_song_request = true;
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
        uint8_t lives = state->player_front_weapon_power;

        if (
            event->eventdat == 533 &&
            (
                lives == 11 ||
                (ot_mt_rand(state) % 15u) < lives
            )
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
