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
    state->frame_player_velocity_x_delta = 0;
    state->frame_player_velocity_y_delta = 0;
    state->frame_explosion_count = 0;
    state->pending_text_window = 0;
    state->frame_music_song_request = false;
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
     * tyrian2.c advances curLoc by one when event 53 has enabled
     * forceEvents and event 2 has stopped the first background.  Several
     * authored set pieces depend on this separate event clock: Episode 4
     * HARVEST stops at 9400, runs its mechanical gate sequence, and only
     * later spawns/arms the real boss.  Merely storing force_events leaves
     * the timeline parked forever because the normal background-derived
     * increment is zero.
     *
     * Keep this immediately after JE_eventSystem(), before the background
     * delay/update phase, which is the exact source ordering.
     */
    if (state->force_events && state->back_move == 0) {
        state->cur_loc++;
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
    ot_advance_enemies_before_player(state);
}

void ot_level_port_advance_over_player_enemies(OtLevelPortState *state)
{
    if (state != 0) ot_advance_enemies_over_player(state);
}

void ot_level_port_update_filter_fade(OtLevelPortState *state)
{
    if (state != 0) ot_update_filter_fade(state);
}

void ot_level_port_update_level_timer(OtLevelPortState *state)
{
    if (state != 0) ot_update_level_timer(state);
}

void ot_level_port_initialize_starfield(OtLevelPortState *state)
{
    int16_t index;

    if (state == 0) return;
    /*
     * Literal initialize_starfield() call order.  Besides presentation this
     * is gameplay-significant: the 400 MT19937 draws precede every authored
     * random enemy, launch and explosion decision in the PC level loop.
     */
    for (index = OT_STARFIELD_STAR_COUNT - 1; index >= 0; index--) {
        uint32_t x = ot_level_port_random(state) % 320u;
        uint32_t y = ot_level_port_random(state) % 200u;
        uint8_t speed = (uint8_t)(
            ot_level_port_random(state) % 3u
        );
        uint8_t color = (uint8_t)(
            ot_level_port_random(state) % 16u
        );

        state->star_position[index] = (uint16_t)(x + y * 320u);
        state->star_meta[index] = (uint8_t)(
            speed | (color << 4)
        );
    }
}

void ot_level_port_update_starfield(OtLevelPortState *state)
{
    int16_t index;

    if (state == 0 || !state->star_active) return;
    for (index = OT_STARFIELD_STAR_COUNT - 1; index >= 0; index--) {
        /* OpenTyrian deliberately relies on JE_word/u16 wrap-around. */
        state->star_position[index] = (uint16_t)(
            state->star_position[index] +
            (
                (state->star_meta[index] & 3u) + 2 +
                state->starfield_speed
            ) * 320
        );
    }
}

void ot_level_port_update_low_armor_warning(
    OtLevelPortState *state,
    bool player_alive
)
{
    uint16_t sound_delay;
    int16_t warning_max;

    if (state == 0) return;
    state->warning_active = false;
    if (!player_alive || state->player_armor >= 6) return;
    state->warning_active = true;

    if (state->armor_ship_delay > 0) {
        state->armor_ship_delay--;
    } else {
        uint8_t created = ot_new_enemy(
            state,
            50,
            560,
            0,
            OT_SPAWN_WARNING_SHIP
        );

        if (created > 0) {
            OtEnemy *enemy = &state->enemy[created - 1u];

            enemy->enemydie = (uint16_t)(
                561u + ot_level_port_random(state) % 3u
            );
            enemy->eyc = (int8_t)(
                (int16_t)enemy->eyc - (int16_t)state->back_move3
            );
            enemy->armorleft = 4;
        }
        state->armor_ship_delay = 500;
    }

    sound_delay = (uint16_t)(state->player_armor * 4u + 8u);
    if (state->warning_sound_delay > sound_delay) {
        state->warning_sound_delay = sound_delay;
    }
    if (state->warning_sound_delay > 1) {
        state->warning_sound_delay--;
    } else {
        state->frame_sound_queue[7] = 17; /* S_WARNING */
        state->warning_sound_delay = sound_delay;
    }

    state->warning_color = (int16_t)(
        state->warning_color + state->warning_color_change
    );
    warning_max = (int16_t)(
        113 + (14 - state->player_armor * 2)
    );
    if (state->warning_color > warning_max) {
        state->warning_color_change =
            (int8_t)-state->warning_color_change;
        state->warning_color = warning_max;
    } else if (state->warning_color < 113) {
        state->warning_color_change =
            (int8_t)-state->warning_color_change;
    }
}

void ot_level_port_update_return_active(OtLevelPortState *state)
{
    if (
        state != 0 &&
        state->return_active &&
        state->frame_enemy_on_screen == 0
    ) {
        ot_event_jump(state, UINT16_MAX);
        state->return_active = false;
    }
}
