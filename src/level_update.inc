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
    *target = (u16 *)SCREEN_BASE_BLOCK(screen_block) + ((row & 63) * 32);
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
    if (new_row != old_row) {
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
}

static void enter_boss(void)
{
    u8 index;
    for (index = 0; index < MAX_ENEMIES; index++) enemies[index].active = 0;
    foreground_phase = 0;
    boss_x = 88;
    boss_y = 8;
    boss_dx = 1;
    boss_dy = 1;
    /* PC event 79 links this bar to boss components whose armor is 254. */
    boss_hp = 254;
    boss_phase = 0;
    boss_bar_flash = 0;
    boss_bar_palette_dirty = 1;
    /*
     * PC boss records 52/54 use W59 from slot 2; record 53 uses W127
     * from slot 1. JE_makeEnemy initializes every populated turret to 20.
     */
    boss_aim_fire_wait = 20;
    boss_spread_fire_wait = 20;
    boss_aim_left_pos = 0;
    boss_aim_right_pos = 0;
    boss_spread_pos = 0;
    game_state = STATE_BOSS;
    telemetry_state_transitions++;
}

static void finish_boss(void)
{
    u8 index;
    game_state = STATE_CLEAR;
    clear_timer = 100;
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shots[index].active = 0;
    }
    spawn_large_effect(boss_x + 16, boss_y + 16, 0);
    spawn_large_effect(boss_x + 48, boss_y + 16, 0);
    spawn_large_effect(boss_x + 32, boss_y + 48, 0);
    audio_effect(SFX_EXPLOSION_9);
    telemetry_state_transitions++;
}

static void update_boss(void)
{
    u8 index;
    PlayerShot *shot;

    update_player();
    update_shots();
    update_effects();
    update_rewards();

    boss_phase++;
    if (boss_bar_flash) {
        boss_bar_flash--;
        boss_bar_palette_dirty = 1;
    }
    boss_x += boss_dx;
    if (boss_x < 4 || boss_x > 172) {
        boss_dx = -boss_dx;
        boss_x += boss_dx;
    }
    if ((boss_phase & 3) == 0) {
        boss_y += boss_dy;
        if (boss_y < 4 || boss_y > 34) {
            boss_dy = -boss_dy;
            boss_y += boss_dy;
        }
    }
    if (--boss_aim_fire_wait == 0) {
        /*
         * Original component anchors are x=24/72, y=28 inside the
         * 120x112 PC boss. The POC's 64x64 composite scales those to
         * x=13/38, y=16.
         */
        fire_enemy_weapon(
            boss_x + 13, boss_y + 16, 59, 1, &boss_aim_left_pos
        );
        fire_enemy_weapon(
            boss_x + 38, boss_y + 16, 59, 1, &boss_aim_right_pos
        );
        boss_aim_fire_wait = 10;
    }
    if (--boss_spread_fire_wait == 0) {
        fire_enemy_weapon(
            boss_x + 26, boss_y + 16, 127, 0, &boss_spread_pos
        );
        boss_spread_fire_wait = 60;
    }
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        shot = &player_shots[index];
        if (!shot->active) continue;
        if (BOX_OVERLAPS(
                shot->x + PLAYER_SHOT_HIT_X,
                shot->y + PLAYER_SHOT_HIT_Y,
                PLAYER_SHOT_HIT_WIDTH,
                PLAYER_SHOT_HIT_HEIGHT,
                boss_x + 4, boss_y + 4, 56, 56)) {
            shot->active = 0;
            if (boss_hp) {
                boss_hp--;
                boss_bar_flash = 6;
                boss_bar_palette_dirty = 1;
            }
            telemetry_collision_count++;
            audio_effect(SFX_ENEMY_HIT);
        }
    }
    if (pad_pressed & KEY_R) boss_hp = 0;
#ifdef AUTOTEST
    if (boss_phase > 240) boss_hp = 0;
#endif
    if (!boss_hp) finish_boss();
}

static void update_logic(void)
{
    level_tick++;
    telemetry_logic_updates++;
    /*
     * Stage 1 runs the exact JE_EventRecType stream in shadow mode.  Entity
     * cases remain deferred to the working v11 runtime, while translated
     * source cases and ordering are audited on every tick.
     */
    if (game_state == STATE_PLAY) {
        ot_level_port_advance(&source_parity_level, level_position);
        telemetry_source_events = source_parity_level.event_index;
        telemetry_source_events_applied =
            source_parity_level.applied_event_count;
        telemetry_source_events_deferred =
            source_parity_level.deferred_event_count;
    }
    if (game_state == STATE_PLAY) process_events();
    advance_backgrounds();

    if (game_state == STATE_PLAY) {
        level_position += bg1_step;
        update_player();
        update_shots();
        /*
         * Advance existing explosions before collision handling so a newly
         * spawned effect renders source frame 122 for one complete tick.
         */
        update_effects();
#ifdef AUTOTEST_EXPLOSION_SEAM_TEST
        if (level_tick == 2) spawn_large_effect(120, 72, 0);
#endif
#ifdef AUTOTEST_REWARD_VISUAL_TEST
        if (level_tick == 2) spawn_reward(112, 70, 1);
#endif
        update_rewards();
        update_enemies();
        collide_player_shots();
        if ((pad_pressed & (KEY_SELECT | KEY_L)) ||
            level_position >= LEVEL_BOSS_TICK) {
            enter_boss();
        }
    } else if (game_state == STATE_BOSS) {
        update_boss();
    } else if (game_state == STATE_CLEAR) {
        update_effects();
        update_rewards();
        if (clear_timer) {
            clear_timer--;
        } else {
            enter_title();
#ifdef AUTOTEST
            if (autotest_running) {
                /* The test shuts down only after the complete return path. */
                autotest_finish();
            }
#endif
        }
    }
}
