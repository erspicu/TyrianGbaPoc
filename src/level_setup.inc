/* GBA level entry/exit and resource setup; legacy v11 baseline. */

static void clear_entities(void)
{
    u16 index;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shots[index].active = 0;
    }
    for (index = 0; index < MAX_EFFECTS; index++) effects[index].active = 0;
    for (index = 0; index < MAX_REWARDS; index++) rewards[index].active = 0;
    for (
        index = 0;
        index < MAX_PICKUP_EXPLOSIONS;
        index++
    ) {
        pickup_explosions[index].active = 0;
    }
    active_effect_count = 0;
    effect_slot_high_water = 0;
    active_reward_count = 0;
    active_pickup_explosion_count = 0;
}

static u16 title_palette_colour(const OtDataView *palette, u8 index)
{
    const u8 *rgb = palette->data + (u32)index * 3;

    /* Stock palette.dat stores VGA 6-bit RGB; GBA uses 5 bits/channel. */
    return (u16)(
        (rgb[0] >> 1) |
        ((rgb[1] >> 1) << 5) |
        ((rgb[2] >> 1) << 10)
    );
}

static u8 enter_level(u16 selected_section)
{
    OtEpisodeLevel selected_level;

    if (
        selected_section == 0 ||
        !ot_data_episode_level_resolve(
            (u8)(frontend_episode + 1u),
            selected_section,
            frontend_play_mode,
            frontend_difficulty,
            &selected_level
        ) ||
        selected_level.episode_complete ||
        !ot_data_level_select(
            selected_level.episode,
            selected_level.lvl_file_number
        )
    ) {
        frontend_level_ready = 0;
        return 0;
    }
    frontend_level = selected_level;
    frontend_level_ready = 1;
    if (!background_bind_selected_level()) {
        frontend_level_ready = 0;
        return 0;
    }

    REG_DISPCNT = LCDC_OFF;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = 0;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    source_background2_enabled =
        TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW;
    source_background2_upload_pending = 0;
    game_over_tile_upload_pending = 0;
    clear_entities();
    hide_all_sprites();

    dmaCopy(obj_tiles, OBJ_BASE_ADR, OBJ_TILE_COUNT * 32);
    dmaCopy(obj_palette, SPRITE_PALETTE, 512);
    /*
     * source_runtime_reset() replaces only its documented time-shared banks
     * with a projection of the PC level palette. enter_boss() restores this
     * complete static palette before the legacy POC boss is presented.
     */
    boss_obj_palette_restore_pending = 0;
    boss_bar_flash = 0;
    boss_bar_palette_dirty = 1;

    REG_BG0CNT = BG_PRIORITY(3) |
                 CHAR_BASE(background_cache_char_base[0]) |
                 SCREEN_BASE(BG0_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_1;
    REG_BG1CNT = BG_PRIORITY(2) |
                 CHAR_BASE(background_cache_char_base[1]) |
                 SCREEN_BASE(BG1_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_1;
    REG_BG2CNT = BG_PRIORITY(1) |
                 CHAR_BASE(background_cache_char_base[2]) |
                 SCREEN_BASE(BG2_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_1;

    bg1_scroll_pixel = BG1_INITIAL_SCROLL;
    bg2_scroll_pixel = BG2_INITIAL_SCROLL;
    bg3_scroll_pixel = BG3_INITIAL_SCROLL;
    bg1_presentation_scroll_pixel = bg1_scroll_pixel;
    bg2_presentation_scroll_pixel = bg2_scroll_pixel;
    bg3_presentation_scroll_pixel = bg3_scroll_pixel;
    if (
        !background_prepare_layer(0, bg1_scroll_pixel >> 3) ||
        !background_prepare_layer(2, bg3_scroll_pixel >> 3) ||
        (
            source_background2_enabled &&
            !background_prepare_layer(1, bg2_scroll_pixel >> 3)
        )
    ) {
        frontend_level_ready = 0;
        return 0;
    }
    background_commit_full_layer(
        0,
        bg1_scroll_pixel >> 3,
        BG0_SCREEN_BLOCK
    );
    if (source_background2_enabled) {
        background_commit_full_layer(
            1,
            bg2_scroll_pixel >> 3,
            BG1_SCREEN_BLOCK
        );
    }
    background_commit_full_layer(
        2,
        bg3_scroll_pixel >> 3,
        BG2_SCREEN_BLOCK
    );
    /* First-level event 0 selects the original 1:2:0 opening motion. */
    bg1_scroll_speed = 1;
    bg2_scroll_speed = 2;
    bg3_scroll_speed = 0;
    bg1_scroll_delay = 1;
    bg2_scroll_delay = 1;
    bg1_scroll_delay_max = 1;
    bg2_scroll_delay_max = 1;

    /* OpenTyrian JE_main() launch position before its first movement clamp. */
    player_source_x = 100;
    player_source_y = 180;
    player_source_velocity_x = 0;
    player_source_velocity_y = 0;
    player_source_x_friction_ticks = 0;
    player_source_y_friction_ticks = 0;
    player_invulnerable = 0;
    /*
     * JE_initPlayerData() + JE_main(): USP Talon armor=10 and shield item 4
     * (Gencore High Energy Shield mpwr=5, shield_max=10).
     */
    player_alive = 1;
    player_exploding_ticks = 0;
    player_death_fx_wait = 0;
    player_death_music_volume = 255;
    player_death_music_fade_active = 0;
    player_armor = 10;
    player_shield = 5;
    player_shield_max = 10;
    player_lives = 1;
    player_end_warp = -4;
    fire_cooldown = 0;
    player_bank = 0;
    player_cash = 0;
    game_paused = 0;
#ifdef AUTOTEST_CASH_VISUAL_TEST
    player_cash = 12345;
#endif
    level_tick = 0;
    level_position = 0;
    logic_accumulator = 0;
    level_exit_music_started = 0;
    ot_level_port_init(
        &source_parity_level,
        frontend_difficulty,
        frontend_play_mode == FRONTEND_PLAY_ARCADE,
        source_enemy_shape_history_valid != 0
    );
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_loadout_init();
#endif
    source_enemy_shape_history_valid = 1;
    source_runtime_reset();

    telemetry_display_frames = 0;
    telemetry_logic_updates = 0;
    telemetry_spawn_count = 0;
    telemetry_control_count = 0;
    telemetry_collision_count = 0;
    telemetry_map_rows = 0;
    telemetry_missed_vblanks = 0;
    telemetry_stream_drops = 0;
    telemetry_max_enemies = 0;
    telemetry_max_oam = 0;
    telemetry_max_effects = 0;
    telemetry_effect_drops = 0;
    telemetry_reward_spawns = 0;
    telemetry_reward_pickups = 0;
    telemetry_max_rewards = 0;
    telemetry_reward_drops = 0;
    telemetry_enemy_shots_spawned = 0;
    telemetry_enemy_shot_drops = 0;
    telemetry_max_enemy_shots = 0;
    telemetry_enemy_replacements = 0;
    telemetry_kill_cash = 0;
    telemetry_reward_controls = 0;
    telemetry_reward_assignments = 0;
    telemetry_pause_toggles = 0;
    telemetry_paused_frames = 0;
    telemetry_source_events = 0;
    telemetry_source_events_applied = 0;
    telemetry_source_events_deferred = 0;
    telemetry_source_events_skipped = 0;
    telemetry_source_spawn_attempts = 0;
    telemetry_source_spawn_successes = 0;
    telemetry_source_spawn_pool_full = 0;
    telemetry_source_spawn_missing = 0;
    telemetry_source_max_enemies = 0;
    telemetry_source_control_writes = 0;
    telemetry_source_rng_calls = 0;
    telemetry_source_motion_updates = 0;
    telemetry_source_releases = 0;
    telemetry_source_shot_triggers = 0;
    telemetry_source_enemy_shots_spawned = 0;
    telemetry_source_enemy_shot_drops = 0;
    telemetry_source_max_enemy_shots = 0;
    telemetry_source_enemy_shot_updates = 0;
    telemetry_source_enemy_shot_releases = 0;
    telemetry_source_enemy_shot_player_hits = 0;
    telemetry_source_player_shot_hits = 0;
    telemetry_source_enemy_kills = 0;
    telemetry_source_direct_cash = 0;
    telemetry_autotest_combat_assists = 0;
    telemetry_source_score_item_spawns = 0;
    telemetry_source_score_item_pickups = 0;
    telemetry_source_score_item_max_active = 0;
    telemetry_source_death_spawn_attempts = 0;
    telemetry_source_death_spawn_successes = 0;
    telemetry_source_death_control_events = 0;
    telemetry_source_death_assignments = 0;
    telemetry_source_visible_enemies = 0;
    telemetry_source_max_visible_enemies = 0;
    telemetry_source_unknown_visuals = 0;
    telemetry_sprite2_null_pointer_skips = 0;
    telemetry_sprite2_zero_graphic_skips = 0;
    telemetry_projectile_cache_hits = 0;
    telemetry_projectile_cache_misses = 0;
    telemetry_projectile_cache_evictions = 0;
    telemetry_projectile_cache_drops = 0;
    telemetry_projectile_cache_uploads = 0;
    telemetry_projectile_cache_max_uploads = 0;
    telemetry_projectile_cache_max_visible_unique = 0;
    telemetry_player_shot_spawns = 0;
    telemetry_player_shot_drops = 0;
    telemetry_player_shot_max_active = 0;
    telemetry_player_chain_volleys = 0;
    telemetry_stress_option_blend_draws = 0;
    telemetry_stress_psg_triggers = 0;
    telemetry_wall_vblanks = 0;
    telemetry_presentation_render_attempts = 0;
    telemetry_presentation_render_completed = 0;
    telemetry_presentation_render_deferred = 0;
    telemetry_presentation_render_forced = 0;
    telemetry_presentation_superseded = 0;
    telemetry_presentation_pending_logic_max = 0;
    telemetry_presentation_estimate_max = 0;
    telemetry_presentation_deadline_elapsed_max = 0;
    telemetry_logic_catchup_updates = 0;
    telemetry_logic_updates_per_loop_max = 0;
    telemetry_logic_backlog_frames_max = 0;
    telemetry_background_held_rows_max = 0;
    telemetry_vblank_recovery_loops = 0;
    telemetry_audio_frames = 0;
#if TYRIAN_GBA_STRESS_LOADOUT
    telemetry_projectile_culled_offscreen_before_cache = 0;
    telemetry_projectile_culled_oam_full_before_cache = 0;
    telemetry_projectile_post_visibility_acquires = 0;
    telemetry_projectile_visible_capacity_drops = 0;
#endif
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    telemetry_stress_logic_cycles_total = 0;
    telemetry_stress_logic_cycles_max = 0;
    telemetry_stress_render_cycles_total = 0;
    telemetry_stress_render_cycles_max = 0;
    telemetry_stress_collision_cycles_total = 0;
    telemetry_stress_collision_cycles_max = 0;
#endif
#if TYRIAN_GBA_PERF_TIMER
    telemetry_perf_vblank_irq_cycles_last = 0;
    telemetry_perf_vblank_irq_cycles_total = 0;
    telemetry_perf_vblank_irq_cycles_max = 0;
    telemetry_perf_commit_cycles_total = 0;
    telemetry_perf_commit_cycles_max = 0;
    telemetry_perf_audio_input_cycles_total = 0;
    telemetry_perf_audio_input_cycles_max = 0;
    telemetry_perf_prelogic_cycles_total = 0;
    telemetry_perf_prelogic_cycles_max = 0;
#endif
    telemetry_detail_lava_frames = 0;
    telemetry_detail_water_frames = 0;
    telemetry_detail_iced_frames = 0;
    telemetry_detail_blur_frames = 0;
    telemetry_detail_wild_frames = 0;
    telemetry_stress_loadout_failures =
#if TYRIAN_GBA_STRESS_LOADOUT
        stress_loadout_valid ? 0 : 1;
#else
        0;
#endif
    telemetry_source_launch_attempts = 0;
    telemetry_source_launch_successes = 0;
    telemetry_source_random_attempts = 0;
    telemetry_source_random_successes = 0;
    telemetry_pickup_explosion_spawns = 0;
    telemetry_pickup_explosion_drops = 0;
    telemetry_pickup_explosion_max_active = 0;
    telemetry_end_level_music_starts = 0;
    telemetry_end_level_initial_warp = 0;
    telemetry_end_level_trail_max = 0;
    telemetry_level_complete_voice_starts = 0;
    telemetry_stats_stage_advances = 0;
    telemetry_stats_cube_reveals = 0;
    telemetry_player_death_large_explosions = 0;
    telemetry_player_death_sfx_9 = 0;
    telemetry_player_death_sfx_11 = 0;
    telemetry_player_death_sfx_22 = 0;
    telemetry_player_death_music_fade_steps = 0;
    telemetry_game_over_music_starts = 0;
    telemetry_game_over_overlay_frames = 0;
    telemetry_game_over_exits = 0;
    last_vblank_seen = telemetry_vblank_irqs;

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    presentation_render_ewma = 133630;
    presentation_render_deviation = 8192;
    presentation_render_estimate =
        PRESENTATION_INITIAL_RENDER_ESTIMATE;
    telemetry_presentation_estimate_max =
        presentation_render_estimate;
    presentation_render_pending = 0;
    presentation_pending_logic_ticks = 0;
    presentation_registers_valid = 0;
    presentation_release_held_window = 0;
    gameplay_presentation_capture_registers();
#endif
    REG_BG0HOFS = source_background_hofs(0);
    REG_BG1HOFS = source_background_hofs(1);
    REG_BG2HOFS = source_background_hofs(2);
    REG_BG0VOFS = bg1_scroll_pixel & 255;
    REG_BG1VOFS = bg2_scroll_pixel & 255;
    REG_BG2VOFS = bg3_scroll_pixel & 255;
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    REG_DISPCNT = MODE_0 | BG0_ON | BG2_ON | OBJ_ON | OBJ_1D_MAP |
        (source_background2_present() ? BG1_ON : 0);
    audio_load_source_song(frontend_level.source_song);
    game_state = STATE_PLAY;
    telemetry_state_transitions++;
    return 1;
}
