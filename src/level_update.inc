/* Legacy background, boss and frame update orchestration. */

static u8 schedule_row(
    u8 layer,
    u16 row,
    u16 screen_block,
    BackgroundPendingRows *pending
)
{
    const u8 *source;
    u16 *target;
    u8 index = pending->count;

    if (index >= BACKGROUND_PENDING_ROW_CAPACITY) {
        telemetry_stream_drops++;
        return 0;
    }
    if (!background_schedule_row(
            layer,
            row,
            screen_block,
            &source,
            &target
        )) {
        telemetry_stream_drops++;
        return 0;
    }
    pending->row[index] = row;
    pending->source[index] = source;
    pending->target[index] = target;
    pending->count = (u8)(index + 1u);
    telemetry_map_rows++;
    return 1;
}

static u16 background_presentation_scroll_pixel(
    u8 layer,
    s8 camera_offset_y
)
{
    s32 scroll;
    s32 maximum;

    if (layer == 0) {
        scroll = bg1_presentation_scroll_pixel;
    } else if (layer == 1) {
        scroll = bg2_presentation_scroll_pixel;
    } else {
        scroll = bg3_presentation_scroll_pixel;
    }
    scroll += camera_offset_y;
    maximum =
        (s32)background_canvas_rows[layer] * 8 -
        SCREEN_HEIGHT;
    if (maximum < 0) maximum = 0;
    if (scroll < 0) scroll = 0;
    if (scroll > maximum) scroll = maximum;
    return (u16)scroll;
}

static u8 background_layer_missing_presentation_rows(
    u8 layer,
    s8 camera_offset_y,
    const BackgroundPendingRows *pending,
    u16 missing[BACKGROUND_PENDING_ROW_CAPACITY],
    u16 *top_out,
    u16 *last_out
)
{
    BackgroundLayerCache *cache =
        &background_layer_cache[layer];
    u16 top = (u16)(
        background_presentation_scroll_pixel(
            layer,
            camera_offset_y
        ) >> 3
    );
    u16 last = (u16)(top + BACKGROUND_VISIBLE_TILE_ROWS);
    u16 row;
    u8 missing_count = 0;

    if (last > background_canvas_rows[layer]) {
        last = background_canvas_rows[layer];
    }
    *top_out = top;
    *last_out = last;
    for (row = top; row < last; row++) {
        u8 pending_index;
        u8 already_pending = 0;

        if (
            cache->ring_source_row[
                row & (MAP_RING_ROWS - 1u)
            ] != row ||
            cache->ring_vram_source_row[
                row & (MAP_RING_ROWS - 1u)
            ] != row
        ) {
            for (
                pending_index = 0;
                pending_index < pending->count;
                pending_index++
            ) {
                if (pending->row[pending_index] == row) {
                    already_pending = 1;
                    break;
                }
            }
            if (already_pending) continue;
            missing[missing_count++] = row;
        }
    }
    return missing_count;
}

static void background_pending_retain_window(
    BackgroundPendingRows *pending,
    u16 top,
    u16 last
)
{
    u8 read;
    u8 write = 0;

    for (read = 0; read < pending->count; read++) {
        u16 row = pending->row[read];

        if (row < top || row >= last) continue;
        if (write != read) {
            pending->row[write] = row;
            pending->source[write] = pending->source[read];
            pending->target[write] = pending->target[read];
        }
        write++;
    }
    pending->count = write;
}

/*
 * Keep the 21-row ownership window synchronized with the exact VOFS that
 * will be committed.  Camera motion and map scrolling are independent, so
 * checking only the map's raw 8-pixel boundary can leave the newly exposed
 * top/bottom row absent.  Preflight all visible layers before installing any
 * row.  Source events can move a visual layer by 20-22 pixels per tick, so a
 * complete visible-window batch is required; all rows commit before the new
 * VOFS is published.
 */
static u8 background_schedule_presentation_window(
    s8 camera_offset_y
)
{
    u16 missing[BACKGROUND_LAYER_COUNT]
        [BACKGROUND_PENDING_ROW_CAPACITY];
    u16 top[BACKGROUND_LAYER_COUNT] = {0, 0, 0};
    u16 last[BACKGROUND_LAYER_COUNT] = {0, 0, 0};
    u8 count[BACKGROUND_LAYER_COUNT] = {0, 0, 0};
    u8 active[BACKGROUND_LAYER_COUNT] = {
        1,
        (
            source_background2_enabled &&
            !source_background2_upload_pending
        ),
        1,
    };
    u8 layer;

    if (!background_runtime_valid) return 1;
    for (layer = 0; layer < BACKGROUND_LAYER_COUNT; layer++) {
        BackgroundPendingRows *pending =
            &background_pending_rows[layer];

        if (!active[layer]) continue;
        count[layer] = background_layer_missing_presentation_rows(
            layer,
            camera_offset_y,
            pending,
            missing[layer],
            &top[layer],
            &last[layer]
        );
        background_pending_retain_window(
            pending,
            top[layer],
            last[layer]
        );
        /*
         * Presentation is now the ownership boundary.  Release rows outside
         * the newest complete viewport only when that viewport is actually
         * being materialized; source ticks skipped by drop-frame never build
         * or retain intermediate map rows.
         */
        background_retain_visible_rows(layer, top[layer]);
        if (
            pending->count + count[layer] >
            BACKGROUND_PENDING_ROW_CAPACITY
        ) {
            telemetry_stream_drops++;
            return 0;
        }
    }
    for (layer = 0; layer < BACKGROUND_LAYER_COUNT; layer++) {
        u8 row_index;

        if (!active[layer] || count[layer] == 0) continue;
        for (row_index = 0; row_index < count[layer]; row_index++) {
            u16 screen_block =
                layer == 0 ? BG0_SCREEN_BLOCK :
                layer == 1 ? BG1_SCREEN_BLOCK :
                    BG2_SCREEN_BLOCK;

            if (!schedule_row(
                    layer,
                    missing[layer][row_index],
                    screen_block,
                    &background_pending_rows[layer]
                )) {
                return 0;
            }
        }
    }
    return 1;
}

static void advance_backgrounds(void)
{
    u8 move1 = 0;
    u8 move2 = 0;

    bg1_step = 0;
    bg2_step = 0;
    bg3_step = 0;

    /*
     * draw_background_1/2 blit first and advance their backPos afterward.
     * Keep the displayed phase while preparing the next source state.
     */
    bg1_presentation_scroll_pixel = bg1_scroll_pixel;
    bg2_presentation_scroll_pixel = bg2_scroll_pixel;

    if (bg1_scroll_delay > 1) {
        bg1_scroll_delay--;
    } else {
        bg1_scroll_delay = bg1_scroll_delay_max;
        move1 = bg1_scroll_speed;
    }
    if (bg1_scroll_pixel >= move1) {
        bg1_scroll_pixel -= move1;
        bg1_step = move1;
    } else {
        bg1_step = (u8)bg1_scroll_pixel;
        bg1_scroll_pixel = 0;
    }
    if (bg2_scroll_delay > 1) {
        bg2_scroll_delay--;
    } else {
        bg2_scroll_delay = bg2_scroll_delay_max;
        move2 = bg2_scroll_speed;
    }
    if (bg2_scroll_pixel >= move2) {
        bg2_scroll_pixel -= move2;
        bg2_step = move2;
    } else {
        bg2_step = (u8)bg2_scroll_pixel;
        bg2_scroll_pixel = 0;
    }
    if (bg3_scroll_pixel >= bg3_scroll_speed) {
        bg3_scroll_pixel -= bg3_scroll_speed;
        bg3_step = bg3_scroll_speed;
    } else {
        bg3_step = (u8)bg3_scroll_pixel;
        bg3_scroll_pixel = 0;
    }
    /* draw_background_3 advances backPos3 before it blits. */
    bg3_presentation_scroll_pixel = bg3_scroll_pixel;
}

static u8 source_enemy_still_exploding(void)
{
    /* tyrian2.c enemyStillExploding tracks rep_explosions[] only. */
    return source_repeating_explosions_active();
}

static void source_resume_group_stopped_backgrounds(void)
{
    /*
     * Direct translation of the stopBackgroundNum==9 branch at the end of
     * OpenTyrian's level loop.  Group-specific event 4 stops (ground/sky)
     * resume once that pool has left the screen and its final explosion has
     * completed, even if a different enemy layer is still populated.
     */
    if (
        source_parity_level.stop_background_num == 9 &&
        source_parity_level.back_move == 0 &&
        !source_enemy_still_exploding()
    ) {
        source_parity_level.back_move = 1;
        source_parity_level.back_move2 = 2;
        source_parity_level.back_move3 = 3;
        source_parity_level.explode_move = 2;
        source_parity_level.stop_background_num = 0;
        source_parity_level.stop_backgrounds = false;
        if (source_parity_level.all_players_gone) {
            source_parity_level.really_end_level = true;
        }
    }
}

static void source_begin_end_level_audio(void)
{
    if (
        level_exit_music_started ||
        source_parity_level.all_players_gone
    ) {
        return;
    }
    level_exit_music_started = 1;
    secret_level_music_active = 0;
    /*
     * tyrian2.c selects zero-based source song 9 as soon as an alive player
     * enters endLevel.  Keep it running through JE_endLevelAni(), where the
     * "Level completed" voice and cube effects are mixed over the module.
     */
    audio_load_source_song_once_preserve_effects(9);
    telemetry_end_level_music_starts++;
    telemetry_end_level_initial_warp = (u8)player_end_warp;
}

static void source_update_end_level_condition(void)
{
    /*
     * tyrian2.c level_loop tail: event 36 arms the gate; it becomes
     * endLevel/levelEnd=40 only when no authored enemy remains visible and
     * the final death explosion has completed.  Event 11 is handled inside
     * the translated event switch and reaches the same state explicitly.
     */
    if (
        !source_parity_level.end_level &&
        source_parity_level.ready_to_end_level &&
        source_parity_level.frame_enemy_on_screen == 0 &&
        !source_enemy_still_exploding()
    ) {
        if (source_parity_level.level_timer_countdown > 0) {
            source_parity_level.level_timer = false;
        }
        source_parity_level.ready_to_end_level = false;
        source_parity_level.end_level = true;
        source_parity_level.level_end = 40;
        if (source_parity_level.all_players_gone) {
            source_parity_level.really_end_level = true;
        }
        source_begin_end_level_audio();
    }
    if (
        !source_parity_level.end_level &&
        source_parity_level.frame_enemy_on_screen == 0 &&
        source_parity_level.stop_backgrounds
    ) {
        source_parity_level.stop_backgrounds = false;
        source_parity_level.back_move = 1;
        source_parity_level.back_move2 = 2;
        source_parity_level.back_move3 = 3;
        source_parity_level.explode_move = 2;
    }
}

static void source_update_boss_bar_colour(void)
{
    u8 index;
    u8 colour = 0;

    for (index = 0; index < 2; index++) {
        if (
            source_parity_level.boss_bar_color[index] > colour
        ) {
            colour = source_parity_level.boss_bar_color[index];
        }
    }
    if (boss_bar_flash != colour) {
        boss_bar_flash = colour;
        boss_bar_palette_dirty = 1;
    }
    for (index = 0; index < 2; index++) {
        if (source_parity_level.boss_bar_color[index] > 0) {
            source_parity_level.boss_bar_color[index]--;
        }
    }
}

static void source_update_event_music_fade(void)
{
    /*
     * Literal tyrian2.c MUSIC FADE phase.  Event 34 initializes tempVolume
     * to tyrMusicVolume (255 in this calibrated adapter), then the loop
     * lowers it once per gameplay tick before processing the next event.
     */
    if (!source_parity_level.music_fade) return;
    if (source_parity_level.music_fade_volume > 10) {
        source_parity_level.music_fade_volume--;
        audio_set_source_music_volume(
            source_parity_level.music_fade_volume
        );
    } else {
        source_parity_level.music_fade = false;
    }
}

static void source_apply_event_music_request(int16_t previous_song)
{
    if (!source_parity_level.frame_music_song_request) return;
    if (
        source_parity_level.current_song >= 0 &&
        source_parity_level.current_song <
            (int16_t)JUKEBOX_MUSIC_COUNT
    ) {
        if (source_parity_level.current_song != previous_song) {
            audio_load_source_song_preserve_effects(
                (u8)source_parity_level.current_song
            );
        }
        audio_set_source_music_volume(255);
    }
}

static void source_update_secret_level_display(void)
{
    int16_t flash;

    if (source_parity_level.display_time == 0) return;
    source_parity_level.display_time--;
    if (
        source_parity_level.display_time == 0 &&
        secret_level_music_active
    ) {
        /*
         * Source song 30 is a finite Secret Level sting.  Maxmod's normal
         * module path is deliberately looped for level music, so explicitly
         * return to the selected level theme when the source's 150-tick
         * notice expires.
         */
        secret_level_music_active = 0;
        if (
            game_state == STATE_PLAY &&
            !source_parity_level.end_level
        ) {
            audio_load_source_song_preserve_effects(
                frontend_level.source_song
            );
        }
        if (frontend_demo_active) {
            insert_coin_tile_upload_pending = 1;
        }
    }
    flash = (int16_t)(
        source_parity_level.display_flash +
        source_parity_level.display_flash_change
    );
    source_parity_level.display_flash = (u8)flash;
    if (
        source_parity_level.display_flash > 4 ||
        source_parity_level.display_flash == 0
    ) {
        source_parity_level.display_flash_change =
            (s8)-source_parity_level.display_flash_change;
    }
    secret_level_palette_dirty = 1;
}

static void update_logic(void)
{
    u8 end_level_before = source_parity_level.end_level;
    int16_t event_song_before = source_parity_level.current_song;
    u32 event_jump_count_after_advance;
#ifdef AUTOTEST_ROUTE_PERF
    u32 route_stage_start = stress_cycle_counter_read();
#endif

    level_tick++;
    telemetry_logic_updates++;
    /* PC decrements textErase before JE_eventSystem can replace the text. */
    gameplay_overlay_logic_tick();
    if (
        frontend_demo_active &&
        !source_parity_level.end_level
    ) {
        (void)frontend_demo_apply_input();
    }
    if (!source_parity_level.end_level) {
        source_player_energy_update();
    }
    source_parity_level.player_armor = player_armor;
    source_parity_level.all_players_gone = (u8)(
        !player_alive &&
        (
            !source_parity_level.arcade_mode ||
            (
                source_parity_level.player_front_weapon_power == 1 &&
                player_exploding_ticks == 0
            )
        )
    );
    source_update_event_music_fade();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_PLAYER_ENERGY
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    /*
     * Stage 4 makes the direct OpenTyrian event/enemy runtime authoritative
     * for the first-level body.  The keypad adapter also stores the player in
     * unchanged OpenTyrian coordinates; only render_game() crops presentation.
     */
    ot_level_port_advance(
        &source_parity_level,
        level_position,
        player_source_x,
        player_source_y
    );
    event_jump_count_after_advance =
        source_parity_level.event_jump_count;
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_LEVEL_PORT
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    if (source_parity_level.cur_loc != level_position) {
        level_position = source_parity_level.cur_loc;
    }
    if (source_parity_level.frame_player_invulnerable_ticks != 0) {
        player_invulnerable =
            source_parity_level.frame_player_invulnerable_ticks;
    }
    source_apply_background_state();
    source_apply_level_frame_outputs();
    source_apply_event_music_request(event_song_before);
    if (!end_level_before && source_parity_level.end_level) {
        source_begin_end_level_audio();
    }
    /* backgrnd.c updates this immediately after background 1. */
    ot_level_port_update_starfield(
        &source_parity_level,
        source_special_astral_duration != 0
    );
    source_sync_runtime_telemetry();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_SOURCE_SYNC
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    advance_backgrounds();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_BACKGROUND
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif

    level_position += bg1_step;
    /*
     * OpenTyrian advances and collides existing player shots before
     * JE_mainGamePlayerFunctions moves the ship and creates new shots.
     * Keeping that phase boundary prevents a newly fired shot from
     * travelling and damaging an enemy in its creation tick.
     */
    update_shots();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_PLAYER_SHOTS
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
#ifdef AUTOTEST_EXPLOSION_SEAM_TEST
    if (level_tick == 2) spawn_large_effect(120, 72, 0);
#endif
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    if (level_tick == 2) spawn_reward(112, 70, 1);
#endif
    update_rewards();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_EFFECTS_REWARDS
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    source_process_player_collisions(
        player_alive && !source_parity_level.end_level
    );
    /* tyrian2.c calls JE_drawSP() after shot movement and hit handling. */
    source_superpixel_update();
    if (
        source_parity_level.event_jump_count !=
            event_jump_count_after_advance
    ) {
        /*
         * JE_eventJump() can also run from the lethal player-shot phase,
         * after this adapter has already advanced/synchronised backgrounds.
         * Preserve that source curLoc immediately so the next tick cannot
         * overwrite the jump with the pre-collision GBA level_position.
         */
        level_position = source_parity_level.cur_loc;
    }
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_PLAYER_COLLISION
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    update_player();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_PLAYER_UPDATE
    ] += stress_cycle_counter_read() - route_stage_start;
    route_stage_start = stress_cycle_counter_read();
#endif
    if (
        game_state != STATE_PLAY &&
        game_state != STATE_GAME_OVER
    ) {
        source_play_frame_weapon_sounds();
#ifdef AUTOTEST_ROUTE_PERF
        route_perf_current_logic_stage[
            ROUTE_PERF_LOGIC_STAGE_ENEMY_SHOTS_TAIL
        ] += stress_cycle_counter_read() - route_stage_start;
#endif
        return;
    }
    if (!source_parity_level.end_level) {
        source_update_enemy_projectiles();
    }
    /*
     * Only the enemy-shot block is inside tyrian2.c's !endLevel guard.
     * background3over/topEnemyOver/skyEnemyOverAll and their JE_drawEnemy()
     * updates remain live throughout the ship's exit flight.
     */
    ot_level_port_advance_over_player_enemies(
        &source_parity_level
    );
    source_apply_level_frame_outputs();
    source_sync_runtime_telemetry();
    source_update_repeating_explosions();
    /*
     * tyrian2.c advances the complete explosion pool only after player
     * collisions/movement, enemy shots, over-player enemies and sequenced
     * explosions have all had their chance to append new entries.
     */
    update_effects();
    source_update_explosions();
    source_parity_level.player_armor = player_armor;
    ot_level_port_update_low_armor_warning(
        &source_parity_level,
        player_alive != 0
    );
    source_update_random_explosions();
    /*
     * Preserve tyrian2.c's tail phase: returnActive, secret notice, timer,
     * filter fade, then the end-level gate.  Keeping these out of the enemy
     * advance prevents an event return from changing the current draw phase.
     */
    ot_level_port_update_return_active(&source_parity_level);
    if (
        source_parity_level.event_jump_count !=
            event_jump_count_after_advance
    ) {
        level_position = source_parity_level.cur_loc;
    }
    source_update_secret_level_display();
    ot_level_port_update_level_timer(&source_parity_level);
    ot_level_port_update_filter_fade(&source_parity_level);
    source_update_boss_bar_colour();
    source_play_frame_weapon_sounds();
    source_resume_group_stopped_backgrounds();
    source_update_end_level_condition();
#ifdef AUTOTEST_ROUTE_PERF
    route_perf_current_logic_stage[
        ROUTE_PERF_LOGIC_STAGE_ENEMY_SHOTS_TAIL
    ] += stress_cycle_counter_read() - route_stage_start;
#endif
}
