/* Legacy background, boss and frame update orchestration. */

static void schedule_row(
    const u8 *map,
    u16 row,
    u16 screen_block,
    u8 *pending,
    const u8 **source,
    u16 **target
)
{
    if (*pending) {
        telemetry_stream_drops++;
        return;
    }
    *source = map_row(map, row);
    *target = (u16 *)SCREEN_BASE_BLOCK(screen_block) + ((row & 31) * 32);
    *pending = 1;
    telemetry_map_rows++;
}

static void advance_backgrounds(void)
{
    u16 old_row;
    u16 new_row;
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
    old_row = bg1_scroll_pixel >> 3;
    if (bg1_scroll_pixel >= move1) {
        bg1_scroll_pixel -= move1;
        bg1_step = move1;
    } else {
        bg1_step = (u8)bg1_scroll_pixel;
        bg1_scroll_pixel = 0;
    }
    new_row = bg1_scroll_pixel >> 3;
    if (new_row != old_row) {
        schedule_row(
            bg1_map, new_row, BG0_SCREEN_BLOCK,
            &bg1_row_pending, &bg1_row_source, &bg1_row_target
        );
    }

    if (bg2_scroll_delay > 1) {
        bg2_scroll_delay--;
    } else {
        bg2_scroll_delay = bg2_scroll_delay_max;
        move2 = bg2_scroll_speed;
    }
    old_row = bg2_scroll_pixel >> 3;
    if (bg2_scroll_pixel >= move2) {
        bg2_scroll_pixel -= move2;
        bg2_step = move2;
    } else {
        bg2_step = (u8)bg2_scroll_pixel;
        bg2_scroll_pixel = 0;
    }
    new_row = bg2_scroll_pixel >> 3;
    if (
        new_row != old_row &&
        source_background2_enabled &&
        !source_background2_upload_pending
    ) {
        schedule_row(
            bg2_map, new_row, BG1_SCREEN_BLOCK,
            &bg2_row_pending, &bg2_row_source, &bg2_row_target
        );
    }

    old_row = bg3_scroll_pixel >> 3;
    if (bg3_scroll_pixel >= bg3_scroll_speed) {
        bg3_scroll_pixel -= bg3_scroll_speed;
        bg3_step = bg3_scroll_speed;
    } else {
        bg3_step = (u8)bg3_scroll_pixel;
        bg3_scroll_pixel = 0;
    }
    new_row = bg3_scroll_pixel >> 3;
    if (new_row != old_row) {
        schedule_row(
            bg3_map, new_row, BG2_SCREEN_BLOCK,
            &bg3_row_pending, &bg3_row_source, &bg3_row_target
        );
    }
    /* draw_background_3 advances backPos3 before it blits. */
    bg3_presentation_scroll_pixel = bg3_scroll_pixel;
}

static u8 source_enemy_on_screen_count(void)
{
    u8 index;
    u8 count = 0;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (
            source_parity_level.enemy_avail[index] == 0 &&
            source_parity_level.enemy_draw[index].active
        ) {
            count++;
        }
    }
    return count;
}

static u8 source_enemy_still_exploding(void)
{
    u8 index;

    for (index = 0; index < MAX_EFFECTS; index++) {
        if (effects[index].active) return 1;
    }
    return 0;
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
        source_enemy_on_screen_count() == 0 &&
        !source_enemy_still_exploding()
    ) {
        source_parity_level.ready_to_end_level = false;
        source_parity_level.end_level = true;
        source_parity_level.level_end = 40;
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

static void update_logic(void)
{
    level_tick++;
    telemetry_logic_updates++;
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
    source_apply_background_state();
    source_play_frame_weapon_sounds();
    source_sync_runtime_telemetry();
    advance_backgrounds();

    level_position += bg1_step;
    /*
     * OpenTyrian advances and collides existing player shots before
     * JE_mainGamePlayerFunctions moves the ship and creates new shots.
     * Keeping that phase boundary prevents a newly fired shot from
     * travelling and damaging an enemy in its creation tick.
     */
    update_shots();
    /*
     * Advance existing explosions before collision handling so a newly
     * spawned effect renders source frame 122 for one complete tick.
     */
    update_effects();
    /*
     * Existing fixed-position JE_setupExplosion entries lose one TTL before
     * JE_playerCollide() can append this tick's newly collected label.
     */
    source_update_fixed_pickup_explosions();
#ifdef AUTOTEST_EXPLOSION_SEAM_TEST
    if (level_tick == 2) spawn_large_effect(120, 72, 0);
#endif
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    if (level_tick == 2) spawn_reward(112, 70, 1);
#endif
    update_rewards();
    if (player_alive && !source_parity_level.end_level) {
        source_process_player_collisions();
    }
    update_player();
    if (game_state != STATE_PLAY) return;
    if (player_alive) {
        source_update_enemy_projectiles();
    }
    source_update_end_level_condition();
    source_update_boss_bar_colour();
}
