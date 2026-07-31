/* GBA selected-level entry, exit and source-resource setup. */

static void clear_entities(void)
{
    u16 index;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shots[index].active = 0;
    }
    for (index = 0; index < MAX_EFFECTS; index++) effects[index].active = 0;
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    for (index = 0; index < MAX_REWARDS; index++) rewards[index].active = 0;
#endif
    for (
        index = 0;
        index < MAX_PICKUP_EXPLOSIONS;
        index++
    ) {
        pickup_explosions[index].active = 0;
    }
    active_effect_count = 0;
    effect_slot_high_water = 0;
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    active_reward_count = 0;
#endif
    active_pickup_explosion_count = 0;
}

#ifdef AUTOTEST
static u8 autotest_secret_level_collision(void)
{
    OtPlayerCollisionResult result;
    OtEnemy *portal = &source_parity_level.enemy[0];
    u8 portal_pass;

    source_parity_level.player_x = 80;
    source_parity_level.player_y = 96;
    source_parity_level.enemy_avail[0] = 2;
    memset(portal, 0, sizeof(*portal));
    portal->ex = 80;
    portal->ey = 96;
    portal->evalue = 10042;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    portal_pass =
        result.bonus_level_triggered &&
        result.next_level == 42 &&
        source_parity_level.bonus_level &&
        source_parity_level.next_level == 42 &&
        source_parity_level.display_time == 150 &&
        source_parity_level.enemy_avail[0] == 1;

    /*
     * JE_playerCollide() evalue==-3 must request the HDT weapon-104 misc
     * shot, not merely remove the score item and increment telemetry.
     */
    source_parity_level.enemy_avail[0] = 0;
    memset(portal, 0, sizeof(*portal));
    portal->ex = 80;
    portal->ey = 96;
    portal->evalue = -3;
    portal->scoreitem = true;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    return
        portal_pass &&
        result.orbiting_asteroids_awarded == 1 &&
        source_parity_level.orbiting_asteroid_pickup_count == 1 &&
        source_parity_level.enemy_avail[0] == 1;
}

#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
static u8 autotest_arcade_equipment_collision(void)
{
    OtPlayerCollisionResult result;
    OtEnemy *pickup = &source_parity_level.enemy[0];
    u8 pass = 1;

    if (!source_parity_level.arcade_mode) return 0;
    source_parity_level.player_x = 80;
    source_parity_level.player_y = 96;

    /*
     * evalue==30000: mainint.c always consumes the purple ball, awards
     * 100 credits, and advances the front weapon's purple-ball progression.
     */
    memset(pickup, 0, sizeof(*pickup));
    source_parity_level.enemy_avail[0] = 0;
    pickup->ex = 80;
    pickup->ey = 96;
    pickup->evalue = 30000;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    pass =
        pass &&
        result.pickup_count == 1 &&
        result.cash_awarded == 100 &&
        source_parity_level.player_front_weapon_power == 2 &&
        source_parity_level.enemy_avail[0] == 1;

    /*
     * evalue>32100: player-one special-weapon pickup branch.
     */
    memset(pickup, 0, sizeof(*pickup));
    source_parity_level.enemy_avail[0] = 0;
    pickup->ex = 80;
    pickup->ey = 96;
    pickup->evalue = 32102;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    pass =
        pass &&
        result.pickup_count == 1 &&
        result.cash_awarded == 250 &&
        source_parity_level.player_special == 2 &&
        source_parity_level.enemy_avail[0] == 1;

    memset(pickup, 0, sizeof(*pickup));
    source_parity_level.enemy_avail[0] = 0;
    pickup->ex = 80;
    pickup->ey = 96;
    pickup->evalue = 32021;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    pass =
        pass &&
        result.pickup_count == 1 &&
        result.cash_awarded == 250 &&
        source_parity_level.player_sidekick[0] == 21 &&
        source_parity_level.player_sidekick[1] == 21 &&
        source_parity_level.enemy_avail[0] == 1;

    memset(pickup, 0, sizeof(*pickup));
    source_parity_level.enemy_avail[0] = 0;
    pickup->ex = 80;
    pickup->ey = 96;
    pickup->evalue = 31029;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    pass =
        pass &&
        result.pickup_count == 1 &&
        result.cash_awarded == 250 &&
        source_parity_level.player_rear_weapon_id == 29 &&
        source_parity_level.player_rear_weapon_power == 1 &&
        source_parity_level.enemy_avail[0] == 1;

    memset(pickup, 0, sizeof(*pickup));
    source_parity_level.enemy_avail[0] = 0;
    pickup->ex = 80;
    pickup->ey = 96;
    pickup->evalue = 30007;
    ot_level_port_collide_player(
        &source_parity_level,
        true,
        &result
    );
    pass =
        pass &&
        result.pickup_count == 1 &&
        result.cash_awarded == 250 &&
        source_parity_level.player_front_weapon_id == 7 &&
        source_parity_level.player_special == 10 &&
        source_parity_level.enemy_avail[0] == 1 &&
        source_parity_level.high_value_pickup_count == 5;
    return pass;
}
#endif
#endif

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

/*
 * Direct JE_getShipInfo() and start_level_first shield setup for the fixed
 * single-player GBA path.  IDs remain the original HDT item indices; this
 * routine deliberately reads the currently selected episode database after
 * ot_data_level_select(), including Episode 4's embedded item block.
 */
static u8 source_player_equipment_apply(void)
{
    OtPowerDefinition power;
    OtShipDefinition ship;
    OtShieldDefinition shield;

    if (
        !ot_data_hdt_power_read(
            source_parity_level.player_generator,
            &power
        ) ||
        !ot_data_hdt_ship_read(
            source_parity_level.player_ship,
            &ship
        ) ||
        !ot_data_hdt_shield_read(
            source_parity_level.player_shield_item,
            &shield
        )
    ) {
        return 0;
    }

    player_generator_power = power.power;
    player_ship_graphic = ship.shipgraphic;
    player_ship_animation = ship.animation;
    player_armor = ship.damage;
    source_parity_level.player_armor = player_armor;
    player_shield = shield.max_power;
    player_shield_max = (u8)(shield.max_power * 2u);
    player_shield_recharge_cost =
        (u16)shield.recharge_power * 20u;
    return 1;
}

static u8 enter_selected_level(const OtEpisodeLevel *selected_level)
{
    if (
        selected_level == 0 ||
        selected_level->episode == 0 ||
        selected_level->episode > OT_EPISODE_COUNT ||
        selected_level->lvl_file_number == 0 ||
        selected_level->source_song >= JUKEBOX_MUSIC_COUNT ||
        selected_level->episode_complete ||
        !ot_data_level_select(
            selected_level->episode,
            selected_level->lvl_file_number
        )
    ) {
        frontend_level_ready = 0;
        return 0;
    }
    frontend_level = *selected_level;
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
    secret_level_tile_upload_pending = 0;
    insert_coin_tile_upload_pending = frontend_demo_active ? 1 : 0;
    secret_level_palette_dirty = 0;
    secret_level_music_active = 0;
    clear_entities();
    hide_all_sprites();

    dmaCopy(obj_tiles, OBJ_BASE_ADR, OBJ_TILE_COUNT * 32);
    dmaCopy(obj_palette, SPRITE_PALETTE, 512);
    /*
     * source_runtime_reset() replaces only its documented time-shared banks
     * with a projection of the PC level palette. Keep the complete static
     * palette as the authoritative backing for later UI/palette restoration.
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
    source_camera_reset();
    player_source_velocity_x = 0;
    player_source_velocity_y = 0;
    player_source_x_friction_ticks = 0;
    player_source_y_friction_ticks = 0;
    player_invulnerable = 100;
    player_alive = 1;
    player_exploding_ticks = 0;
    player_death_fx_wait = 0;
    player_death_music_volume = 255;
    player_death_music_fade_active = 0;
    player_armor = 0;
    player_shield = 0;
    player_shield_max = 0;
    player_lives = 1;
    player_end_warp = -4;
    player_bank = 0;
    player_ship_graphic = 0;
    player_ship_animation = 0;
    player_generator_power = 0;
    player_weapon_energy = 0;
    player_shield_recharge_cost = 0;
    player_shield_wait = 1;
    if (frontend_demo_active || !frontend_player_items_valid) {
        player_cash = 0;
    }
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
        false
    );
#ifdef AUTOTEST
    /*
     * Focused translation regression for mainint.c's evalue>10000 portal.
     * Reinitialize immediately so the probe cannot affect authored state,
     * event order or RNG.
     */
    telemetry_secret_level_collision_pass =
        autotest_secret_level_collision();
    ot_level_port_init(
        &source_parity_level,
        frontend_difficulty,
        frontend_play_mode == FRONTEND_PLAY_ARCADE,
        false
    );
#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
    telemetry_arcade_equipment_collision_pass =
        autotest_arcade_equipment_collision();
    ot_level_port_init(
        &source_parity_level,
        frontend_difficulty,
        true,
        false
    );
#endif
#endif
    if (frontend_demo_active) {
        source_parity_level.player_front_weapon_id =
            frontend_demo_header.front_weapon;
        source_parity_level.player_front_weapon_power =
            frontend_demo_header.front_power;
        source_parity_level.player_rear_weapon_id =
            frontend_demo_header.rear_weapon;
        source_parity_level.player_rear_weapon_power =
            frontend_demo_header.rear_power;
        source_parity_level.player_special =
            frontend_demo_header.special;
        source_parity_level.player_sidekick[0] =
            frontend_demo_header.sidekick[0];
        source_parity_level.player_sidekick[1] =
            frontend_demo_header.sidekick[1];
        source_parity_level.player_generator =
            frontend_demo_header.generator;
        source_parity_level.player_shield_item =
            frontend_demo_header.shield;
        source_parity_level.player_ship =
            frontend_demo_header.ship;
        source_parity_level.player_super_arcade_mode =
            frontend_demo_header.super_arcade_mode;
        source_parity_level.player_sidekick_level =
            frontend_demo_header.sidekick_level;
        source_parity_level.player_sidekick_series =
            frontend_demo_header.sidekick_series;
    } else {
        frontend_campaign_apply_to_level();
    }
    if (!source_player_equipment_apply()) {
        frontend_level_ready = 0;
        return 0;
    }
    /*
     * Direct levelsN.dat ]?, ]! and ]+ handling. Apply the ordered source
     * operations only after entry resources/equipment succeeded, then retain
     * this baseline so cubes picked up in the level accumulate across maps.
     */
    frontend_campaign_apply_level_script(&frontend_level);
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_loadout_init();
#else
    source_front_weapon_init();
#endif
    source_runtime_reset();
#if defined(AUTOTEST) && !TYRIAN_GBA_STRESS_LOADOUT
    telemetry_upgrade_loadout_pass =
        autotest_source_upgrade_loadout();
#endif

    telemetry_display_frames = 0;
    telemetry_logic_updates = 0;
    telemetry_spawn_count = 0;
    telemetry_control_count = 0;
    telemetry_collision_count = 0;
    telemetry_map_rows = 0;
    telemetry_missed_vblanks = 0;
    telemetry_missed_vblanks_play = 0;
    telemetry_missed_vblanks_frontend = 0;
    telemetry_missed_vblanks_game_over = 0;
    telemetry_missed_vblanks_stats = 0;
    telemetry_missed_vblanks_transition = 0;
    telemetry_missed_vblanks_frontend_other = 0;
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
    telemetry_projectile_cache_upload_coalesces = 0;
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
    telemetry_end_level_music_natural_stops = 0;
    telemetry_end_level_initial_warp = 0;
    telemetry_end_level_trail_max = 0;
    telemetry_level_complete_voice_starts = 0;
#ifdef AUTOTEST
    telemetry_source_sound_mask_low = 0;
    telemetry_source_sound_mask_high = 0;
#endif
    telemetry_stats_stage_advances = 0;
    telemetry_stats_cube_reveals = 0;
    telemetry_player_death_large_explosions = 0;
    telemetry_player_death_sfx_9 = 0;
    telemetry_player_death_sfx_11 = 0;
    telemetry_player_death_sfx_22 = 0;
    telemetry_player_death_music_fade_steps = 0;
    telemetry_game_over_music_starts = 0;
    telemetry_game_over_music_natural_stops = 0;
    telemetry_game_over_overlay_frames = 0;
    telemetry_game_over_exits = 0;
    telemetry_camera_min_origin_x = (u32)(u16)source_camera_origin_x();
    telemetry_camera_max_origin_x = (u32)(u16)source_camera_origin_x();
    telemetry_camera_min_origin_y = (u32)(u16)source_camera_origin_y();
    telemetry_camera_max_origin_y = (u32)(u16)source_camera_origin_y();
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
    REG_BG0VOFS = source_background_vofs(0);
    REG_BG1VOFS = source_background_vofs(1);
    REG_BG2VOFS = source_background_vofs(2);
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    REG_DISPCNT = MODE_0 | BG0_ON | BG2_ON | OBJ_ON | OBJ_1D_MAP |
        (source_background2_present() ? BG1_ON : 0);
    audio_load_level_song_fade_in(frontend_level.source_song);
    /* tyrian2.c start_level_first: soundQueue[3] = V_GOOD_LUCK. */
    source_sound_effect(33);
    game_state = STATE_PLAY;
    telemetry_state_transitions++;
    return 1;
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
        selected_level.episode_complete
    ) {
        frontend_level_ready = 0;
        return 0;
    }
    return enter_selected_level(&selected_level);
}
