#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static u8 autotest_detail_adapter_self_test(void)
{
    u8 pass = 1;

    memset(
        source_parity_level.smoothies,
        0,
        sizeof(source_parity_level.smoothies)
    );
    memset(
        source_parity_level.smoothie_data,
        0,
        sizeof(source_parity_level.smoothie_data)
    );
    source_parity_level.filter_active = false;
    source_parity_level.level_filter = -99;
    source_parity_level.level_brightness = -99;
    source_parity_level.background2_over = 0;
    source_parity_level.background2_not_transparent = false;

#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
    source_parity_level.smoothies[0] = 1;
    source_parity_level.smoothies[2] = 1;
    source_parity_level.smoothies[3] = 1;
    source_detail_prepare_scene();
    pass = (u8)(
        !source_detail_iced_requested &&
        !source_detail_blur_requested &&
        !source_detail_wave_requested &&
        !source_detail_wild_active &&
        !source_detail_palette_mode_pending
    );
#elif TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_NORMAL
    source_parity_level.smoothies[2] = 1;
    source_parity_level.smoothies[3] = 1;
    source_detail_prepare_scene();
    pass = (u8)(
        source_detail_iced_requested &&
        source_detail_blur_requested &&
        !source_detail_wave_requested &&
        !source_detail_wild_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_GLOBAL_HUE &&
        source_detail_effect_hue_pending == 8u
    );
#elif TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_HIGH
    source_parity_level.smoothies[0] = 1;
    source_parity_level.smoothie_data[0] = 0;
    source_parity_level.smoothies[1] = 1;
    source_parity_level.smoothie_data[1] = 13;
    source_detail_prepare_scene();
    pass = (u8)(
        source_detail_wave_requested &&
        !source_detail_wild_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_WATER_HUE &&
        source_detail_effect_hue_pending == 13u
    );
#else
    source_parity_level.smoothies[1] = 1;
    source_parity_level.smoothie_data[1] = 13;
    source_parity_level.filter_active = true;
    source_parity_level.level_filter = 6;
    source_parity_level.level_brightness = -3;
    source_detail_prepare_scene();
    pass = (u8)(
        source_detail_wave_requested &&
        source_detail_wild_active &&
        !source_detail_wild_dither_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_GLOBAL_HUE &&
        source_detail_effect_hue_pending == 6u &&
        source_detail_effect_brightness_pending == -3 &&
        source_detail_effect_objects_pending &&
        source_detail_filtered_source_index(
            0x35u,
            SOURCE_DETAIL_EFFECT_WATER_HUE,
            13u,
            0
        ) == 0xd5u &&
        source_detail_filtered_source_index(
            0x85u,
            SOURCE_DETAIL_EFFECT_WATER_HUE,
            13u,
            0
        ) == 0x85u
    );
    /* Event 48 must switch already-visible BG2 tiles back to opaque. */
    source_parity_level.background2_not_transparent = true;
    source_detail_prepare_scene();
    pass = (u8)(
        pass &&
        !source_detail_wild_active &&
        !source_detail_wild_dither_active
    );
#endif
    return pass;
}

static void autotest_full_loadout_stress_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u32 rng_benchmark_start;
    u32 rng_benchmark_elapsed;
    u32 rng_benchmark_sink = 0;
    u16 rng_benchmark_ime;
    u16 rng_benchmark_index;
    u8 active_shots = 0;
    u8 index;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active_shots++;
    }
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'W';
    sram[3] = '8';
    sram[4] = 1;
    sram[5] =
        stress_loadout_valid &&
        telemetry_stress_loadout_failures == 0;
    sram_write_u32(8, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(12, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(16, telemetry_display_frames);
    sram_write_u32(20, telemetry_logic_updates);
    sram_write_u32(24, telemetry_missed_vblanks);
    sram_write_u32(28, telemetry_max_oam);
    sram_write_u32(32, telemetry_player_shot_spawns);
    sram_write_u32(36, telemetry_player_shot_drops);
    sram_write_u32(40, telemetry_player_shot_max_active);
    sram_write_u32(44, telemetry_player_chain_volleys);
    sram_write_u32(48, telemetry_projectile_cache_drops);
    sram_write_u32(
        52,
        telemetry_projectile_cache_max_visible_unique
    );
    sram_write_u32(56, telemetry_sprite2_cache_drops);
    sram_write_u32(60, telemetry_sprite2_max_visible_unique);
    sram_write_u32(64, telemetry_sprite2_l2_drops);
    sram_write_u32(68, telemetry_effect_cache_drops);
    sram_write_u32(72, telemetry_pickup_explosion_drops);
    sram_write_u32(76, telemetry_pickup_explosion_max_active);
    sram_write_u32(80, telemetry_source_max_visible_enemies);
    sram_write_u32(84, telemetry_source_unknown_visuals);
    sram_write_u32(88, background_cache_approximations);
    sram_write_u32(92, telemetry_stress_loadout_failures);
    sram_write_u32(96, telemetry_vblank_irqs);
    sram_write_u32(100, level_position);
    sram_write_u32(104, active_shots);
    sram_write_u32(108, SOURCE_PROJECTILE_CACHE_SLOT_COUNT);
    sram_write_u32(112, SOURCE_ENEMY_CACHE_SLOT_COUNT);
    sram_write_u32(116, MAX_PLAYER_SHOTS);
    sram_write_u32(120, source_parity_level.assets_valid);
    sram_write_u32(128, telemetry_stress_option_blend_draws);
    sram_write_u32(132, source_option_projectile_valid);
    sram_write_u32(136, telemetry_detail_lava_frames);
    sram_write_u32(140, telemetry_detail_water_frames);
    sram_write_u32(144, telemetry_detail_iced_frames);
    sram_write_u32(148, telemetry_detail_blur_frames);
    sram_write_u32(152, telemetry_detail_wild_frames);
    sram_write_u32(156, telemetry_stress_psg_triggers);
    sram_write_u32(
        160,
        telemetry_projectile_culled_offscreen_before_cache
    );
    sram_write_u32(
        164,
        telemetry_projectile_culled_oam_full_before_cache
    );
    sram_write_u32(
        168,
        telemetry_projectile_post_visibility_acquires
    );
    sram_write_u32(
        172,
        telemetry_projectile_visible_capacity_drops
    );
    sram_write_u32(176, telemetry_stress_logic_cycles_total);
    sram_write_u32(180, telemetry_stress_logic_cycles_max);
    sram_write_u32(184, telemetry_stress_render_cycles_total);
    sram_write_u32(188, telemetry_stress_render_cycles_max);
    sram_write_u32(192, telemetry_stress_collision_cycles_total);
    sram_write_u32(196, telemetry_stress_collision_cycles_max);
    sram_write_u32(
        200,
        TYRIAN_GBA_PROJECTILE_PRECACHE_CULL |
            (TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION << 1) |
            (
                TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER <<
                2
            ) |
            (TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK << 3) |
            (TYRIAN_GBA_DYNAMIC_FRAME_DROP << 4) |
            (TYRIAN_GBA_WALL_CLOCK_LOGIC << 5) |
            (TYRIAN_GBA_COLLISION_UNSIGNED_RANGE << 6) |
            (TYRIAN_GBA_COLLISION_MASK_FAST_PATH << 7) |
            (TYRIAN_GBA_PRESENTATION_DEFER << 8) |
            (TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER << 9) |
            (TYRIAN_GBA_COLLISION_LAZY_RESULT << 10) |
            (TYRIAN_GBA_COLLISION_PACKED_CALL << 11) |
            (TYRIAN_GBA_RECOVER_MISSED_VBLANK << 12)
    );
    sram_write_u32(204, telemetry_projectile_cache_hits);
    sram_write_u32(208, telemetry_projectile_cache_misses);
    sram_write_u32(212, telemetry_projectile_cache_evictions);
    sram_write_u32(216, telemetry_projectile_cache_uploads);
    sram_write_u32(
        220,
        source_parity_level.player_shot_collision_mask_rebuild_count
    );
    sram_write_u32(
        224,
        source_parity_level.player_shot_collision_candidate_visit_count
    );
    sram_write_u32(
        228,
        source_parity_level.player_shot_collision_linear_slot_visit_count
    );
    sram_write_u32(
        232,
        source_parity_level.player_shot_collision_mask_active
    );
    sram_write_u32(236, telemetry_perf_vblank_irq_cycles_total);
    sram_write_u32(240, telemetry_perf_vblank_irq_cycles_max);
    sram_write_u32(244, telemetry_perf_commit_cycles_total);
    sram_write_u32(248, telemetry_perf_commit_cycles_max);
    sram_write_u32(252, telemetry_perf_audio_input_cycles_total);
    sram_write_u32(256, telemetry_perf_audio_input_cycles_max);
    sram_write_u32(260, telemetry_perf_prelogic_cycles_total);
    sram_write_u32(264, telemetry_perf_prelogic_cycles_max);
    sram_write_u32(268, telemetry_wall_vblanks);
    sram_write_u32(
        272,
        telemetry_presentation_render_attempts
    );
    sram_write_u32(
        276,
        telemetry_presentation_render_completed
    );
    sram_write_u32(
        280,
        telemetry_presentation_render_deferred
    );
    sram_write_u32(
        284,
        telemetry_presentation_render_forced
    );
    sram_write_u32(288, telemetry_presentation_superseded);
    sram_write_u32(
        292,
        telemetry_presentation_pending_logic_max
    );
    sram_write_u32(
        296,
        telemetry_presentation_estimate_max
    );
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    sram_write_u32(300, presentation_render_estimate);
    sram_write_u32(324, presentation_render_pending);
#else
    sram_write_u32(300, 0);
    sram_write_u32(324, 0);
#endif
    sram_write_u32(
        304,
        telemetry_presentation_deadline_elapsed_max
    );
    sram_write_u32(308, telemetry_logic_catchup_updates);
    sram_write_u32(
        312,
        telemetry_logic_updates_per_loop_max
    );
    sram_write_u32(316, telemetry_logic_backlog_frames_max);
    sram_write_u32(320, telemetry_background_held_rows_max);
    sram_write_u32(328, telemetry_vblank_recovery_loops);
    sram_write_u32(332, telemetry_audio_frames);
    sram_write_u32(336, source_parity_level.rng_call_count);
    sram_write_u32(
        340,
        source_parity_level.enemy_motion_update_count
    );
    sram_write_u32(
        344,
        source_parity_level.enemy_shot_motion_update_count
    );
    sram_write_u32(
        348,
        ot_level_port_stress_round_ratio_call_count()
    );
    sram_write_u32(
        352,
        source_parity_level.enemy_shot_trigger_count
    );
    sram_write_u32(
        356,
        source_parity_level.enemy_launch_success_count
    );
    /*
     * Measure the generator itself after all gameplay counters have been
     * captured.  IRQs are masked so an audio/VBlank interrupt cannot turn
     * this into an interrupt-latency benchmark; Timer 2/3 keeps counting.
     */
    rng_benchmark_ime = REG_IME;
    REG_IME = 0;
    rng_benchmark_start = stress_cycle_counter_read();
    for (rng_benchmark_index = 0;
         rng_benchmark_index < 10000;
         rng_benchmark_index++) {
        rng_benchmark_sink ^=
            ot_level_port_random(&source_parity_level);
    }
    rng_benchmark_elapsed =
        stress_cycle_counter_read() - rng_benchmark_start;
    REG_IME = rng_benchmark_ime;
    sram_write_u32(360, rng_benchmark_elapsed);
    sram_write_u32(364, rng_benchmark_sink);
    sram_write_u32(368, 10000);
    sram_write_u32(372, telemetry_detail_filter_hue_frames);
    sram_write_u32(376, telemetry_detail_palette_rebuilds);
    sram_write_u32(380, telemetry_detail_wave_frames);
    sram_write_u32(384, telemetry_detail_wild_dither_frames);
    sram_write_u32(388, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(392, TYRIAN_GBA_STRESS_LOADOUT);
    /* Additive TGW8 route/transition diagnostics. */
    sram_write_u32(400, AUTOTEST_FRONTEND_ROUTE_EPISODE);
    sram_write_u32(404, AUTOTEST_FRONTEND_ROUTE_SECTION);
    sram_write_u32(408, frontend_level.lvl_file_number);
    sram_write_u32(
        412,
        AUTOTEST_FULL_LOADOUT_STRESS_END_POSITION
    );
    sram_write_u32(
        416,
        AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS
    );
    sram_write_u32(420, source_parity_level.smoothies[0] != 0);
    sram_write_u32(424, source_parity_level.smoothies[1] != 0);
    sram_write_u32(428, source_parity_level.smoothie_data[0]);
    sram_write_u32(432, source_parity_level.smoothie_data[1]);
    /* The self-test mutates smoothie state, so run it after the snapshot. */
    sram_write_u32(396, autotest_detail_adapter_self_test());
    sram[124] = *(volatile const u8 *)save_type_marker;
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
#endif

#ifdef AUTOTEST_EPISODE_WRAP_FLOW
static u8 autotest_episode_wrap_selected;
static u8 autotest_episode_wrap_source_pass;
static u8 autotest_episode_wrap_story_seen;
static u8 autotest_episode_wrap_announcement_seen;

static u16 autotest_episode_wrap_input(void)
{
    if (frontend_transition_busy() || frontend_frame_pending) return 0;
    if (
        game_state == STATE_NEXT_LEVEL_MENU &&
        !autotest_episode_wrap_selected
    ) {
        autotest_episode_wrap_source_pass = (u8)(
            frontend_episode == 3 &&
            frontend_main_section == 44 &&
            frontend_map_ready &&
            frontend_map.requested_section == 44 &&
            frontend_map.choice_count == 2 &&
            frontend_map.map_planet[1] == 21 &&
            frontend_map.map_section[1] == 42 &&
            frontend_map_level[1].episode_complete
        );
        frontend_selection = 1;
        autotest_episode_wrap_selected = 1;
        return KEY_A;
    }
    if (game_state == STATE_EPISODE_SCENE) {
        if (frontend_scene_episode_announcement) {
            autotest_episode_wrap_announcement_seen = 1;
            return KEY_A;
        }
        if (frontend_scene_waiting_input) {
            autotest_episode_wrap_story_seen = 1;
            return KEY_A;
        }
        if (frontend_scene_animation_active) return KEY_A;
    }
    return 0;
}

static void autotest_episode_wrap_finish_update(void)
{
    const OtDataCatalog *catalog;
    volatile u8 *sram;
    u8 passed;

    if (
        !autotest_episode_wrap_selected ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }

    catalog = ot_data_catalog();
    passed = (u8)(
        autotest_episode_wrap_source_pass &&
        autotest_episode_wrap_story_seen &&
        autotest_episode_wrap_announcement_seen &&
        frontend_episode == 0 &&
        frontend_main_section == 1 &&
        frontend_map_ready &&
        frontend_map.requested_section == 1 &&
        !frontend_map.episode_complete &&
        frontend_selection == 4 &&
        game_state == STATE_GAME_MENU &&
        frontend_mode4_active &&
        catalog != 0 &&
        mmActive()
    );

    sram = (volatile u8 *)0x0e000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'S';
    sram[3] = 'I';
    sram[4] = 1;
    sram[5] = passed;
    sram_write_u32(8, autotest_episode_wrap_source_pass);
    sram_write_u32(12, frontend_episode);
    sram_write_u32(16, frontend_main_section);
    sram_write_u32(20, frontend_map.requested_section);
    sram_write_u32(24, game_state);
    sram_write_u32(28, frontend_selection);
    sram_write_u32(32, frontend_map.choice_count);
    sram_write_u32(36, frontend_map.resolved_section);
    sram_write_u32(40, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(44, mmActive());
    sram_write_u32(48, autotest_episode_wrap_story_seen);
    sram_write_u32(52, autotest_episode_wrap_announcement_seen);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_SCRIPTED_SURVIVAL_FLOW
static u8 autotest_scripted_survival_exit_pending;
static u8 autotest_scripted_survival_game_over_pass;

static void autotest_scripted_survival_update(void)
{
    static u8 settled_frames;
    FrontendScriptCheckpoint checkpoint;
    u8 checkpoint_valid;

    if (autotest_scripted_survival_exit_pending) return;
    if (++settled_frames < 4) return;
    checkpoint_valid = frontend_script_checkpoint_read(&checkpoint);

    autotest_scripted_survival_game_over_pass = (u8)(
        game_state == STATE_GAME_OVER &&
        frontend_script_game_active &&
        checkpoint_valid &&
        checkpoint.main_section == 44 &&
        source_parity_level.arcade_mode &&
        source_parity_level.engage_mode &&
        source_parity_level.galaga_mode &&
        source_parity_level.player_ship == 13 &&
        source_parity_level.player_front_weapon_id == 39 &&
        source_parity_level.player_front_weapon_power == 1 &&
        player_lives == 1 &&
        !player_alive &&
        telemetry_game_over_music_starts == 1
    );

    /* A real player may dismiss GAME OVER immediately. */
    pad_pressed = KEY_A;
    game_over_update();
    autotest_scripted_survival_exit_pending = 1;
}

static void autotest_scripted_survival_finish_update(void)
{
    volatile u8 *sram;
    FrontendScriptCheckpoint checkpoint;
    u8 checkpoint_valid;
    u8 restored;

    if (
        !autotest_scripted_survival_exit_pending ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }
    checkpoint_valid = frontend_script_checkpoint_read(&checkpoint);

    restored = (u8)(
        autotest_scripted_survival_game_over_pass &&
        !frontend_script_game_active &&
        checkpoint_valid &&
        frontend_main_section == 44 &&
        frontend_map.requested_section == 44 &&
        frontend_map_ready &&
        frontend_player_items_valid &&
        player_cash == checkpoint.cash &&
        memcmp(
            &frontend_player_items,
            &checkpoint.items,
            sizeof(frontend_player_items)
        ) == 0 &&
        frontend_selection == 4 &&
        frontend_mode4_active &&
        telemetry_game_over_exits == 1
    );

    sram = (volatile u8 *)0x0e000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'S';
    sram[3] = 'X';
    sram[4] = 1;
    sram[5] = restored;
    sram_write_u32(8, checkpoint.main_section);
    sram_write_u32(12, frontend_main_section);
    sram_write_u32(16, frontend_map.requested_section);
    sram_write_u32(20, game_state);
    sram_write_u32(24, frontend_selection);
    sram_write_u32(28, source_parity_level.player_front_weapon_power);
    sram_write_u32(32, player_lives);
    sram_write_u32(36, telemetry_game_over_exits);
    sram_write_u32(40, checkpoint.cash);
    sram_write_u32(44, player_cash);
    sram_write_u32(48, source_parity_level.engage_mode);
    sram_write_u32(52, source_parity_level.galaga_mode);
    sram_write_u32(56, frontend_map.choice_count);
    sram_write_u32(60, frontend_map.resolved_section);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_DEATH_FLOW
static u8 autotest_death_exit_pending;
static u8 autotest_death_game_over_pass;
static u8 autotest_death_normal_weapon_pass;

static void autotest_death_update(void)
{
    static u16 settled_frames;
    static u8 observed_game_over_music_active;
    const OtDataCatalog *catalog;
    volatile u8 *sram;

    if (autotest_death_exit_pending) return;

    settled_frames++;
    if (mmActive()) observed_game_over_music_active = 1;
    if (settled_frames < 4) return;
    /*
     * The source GAME OVER cue is finite.  Do not synthesize the exit key
     * until Maxmod reaches the stripped order-list boundary; this turns the
     * old four-frame "song started" smoke test into an actual no-loop
     * regression.  The bounded fallback records pass=0 instead of hanging.
     */
    if (mmActive() && settled_frames < 1200) return;

    catalog = ot_data_catalog();
    sram = (volatile u8 *)0x0E000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'D';
    sram[3] = '2';
    sram_write_u32(
        4,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(8, mmActive());
    sram_write_u32(12, game_state);
    sram_write_u32(16, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(20, player_alive);
    sram_write_u32(24, player_exploding_ticks);
    sram_write_u32(28, telemetry_player_death_large_explosions);
    sram_write_u32(32, telemetry_player_death_sfx_9);
    sram_write_u32(36, telemetry_player_death_sfx_11);
    sram_write_u32(40, telemetry_player_death_sfx_22);
    sram_write_u32(44, telemetry_player_death_music_fade_steps);
    sram_write_u32(48, telemetry_game_over_music_starts);
    sram_write_u32(52, telemetry_game_over_overlay_frames);
    sram_write_u32(56, source_parity_level.rng_call_count);
    sram_write_u32(60, game_over_tile_upload_pending);
    sram_write_u32(64, source_parity_level.level_end);
    sram_write_u32(68, frontend_mode4_active);
    sram_write_u32(72, active_effect_count);
    sram_write_u32(76, telemetry_effect_drops);
    sram_write_u32(80, telemetry_display_frames);
    sram_write_u32(84, oam_count);
    autotest_death_normal_weapon_pass = (
        TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER == 0 &&
        source_parity_level.player_front_weapon_id == 1 &&
        source_parity_level.player_front_weapon_power == 1 &&
        source_front_weapon_bound &&
        source_front_weapon_port_id == 1 &&
        source_front_weapon_power == 1 &&
        source_front_weapon_hdt_id == 155 &&
        source_front_weapon_valid
    );
    sram_write_u32(120, TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER);
    sram_write_u32(
        124,
        source_parity_level.player_front_weapon_id
    );
    sram_write_u32(
        128,
        source_parity_level.player_front_weapon_power
    );
    sram_write_u32(132, source_front_weapon_hdt_id);
    sram_write_u32(136, source_front_weapon_valid);
    sram_write_u32(140, autotest_death_normal_weapon_pass);
    sram_write_u32(144, observed_game_over_music_active);
    sram_write_u32(
        148,
        telemetry_game_over_music_natural_stops
    );
    sram_write_u32(152, settled_frames);
    autotest_death_game_over_pass = (
        game_state == STATE_GAME_OVER &&
        catalog != 0 &&
        catalog->selected_mus_song == 10 &&
        observed_game_over_music_active &&
        !mmActive() &&
        telemetry_game_over_music_natural_stops == 1 &&
        !TYRIAN_GBA_DEV_PLAYER_INVINCIBLE &&
        !player_alive &&
        player_exploding_ticks == 0 &&
        telemetry_player_death_large_explosions == 120 &&
        telemetry_player_death_sfx_22 == 1 &&
        telemetry_player_death_sfx_9 +
            telemetry_player_death_sfx_11 > 0 &&
        telemetry_player_death_music_fade_steps == 59 &&
        telemetry_game_over_music_starts == 1 &&
        telemetry_game_over_overlay_frames >= 4 &&
        !game_over_tile_upload_pending &&
        source_parity_level.level_end == 0 &&
        !frontend_mode4_active
    );
    sram_write_u32(88, autotest_death_game_over_pass);

#ifdef AUTOTEST_DEATH_CAPTURE
    /*
     * Optional visual harness: stop before the synthetic return key so
     * mGBA -O captures the complete live gameplay/GAME OVER composition.
     */
    __asm__ volatile("swi 3");
#endif

    /*
     * Exercise the source "any input" exit after recording the live Mode-0
     * overlay, then verify that Game Menu restores the title module.
     */
    pad_pressed = KEY_A;
    game_over_update();
    autotest_death_exit_pending = 1;
}

static void autotest_death_finish_update(void)
{
    const OtDataCatalog *catalog;

    if (
        !autotest_death_exit_pending ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }
    catalog = ot_data_catalog();
    sram_write_u32(
        92,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(96, game_state);
    sram_write_u32(100, frontend_selection);
    sram_write_u32(104, frontend_mode4_active);
    sram_write_u32(108, telemetry_game_over_exits);
    sram_write_u32(112, mmActive());
    sram_write_u32(
        116,
        autotest_death_game_over_pass &&
        autotest_death_normal_weapon_pass &&
        catalog != 0 &&
        catalog->selected_mus_song == 2 &&
        frontend_selection == 4 &&
        frontend_mode4_active &&
        telemetry_game_over_exits == 1 &&
        mmActive()
    );
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_JUKEBOX_FLOW
static u8 autotest_jukebox_entered;

static u16 autotest_jukebox_input(void)
{
    if (
        game_state == STATE_INTRO_LOGO_1 ||
        game_state == STATE_INTRO_LOGO_2
    ) {
        return KEY_A;
    }
    if (game_state == STATE_TITLE) {
        if (autotest_jukebox_entered) return 0;
        return frontend_selection < 3 ? KEY_DOWN : KEY_A;
    }
    if (game_state != STATE_JUKEBOX) return 0;

    autotest_jukebox_entered = 1;
    switch (telemetry_jukebox_frames + 1u) {
    case 24:
        return KEY_LEFT;
    case 48:
        return KEY_RIGHT;
    case 72:
        return KEY_A;
    case 96:
    case 120:
        return KEY_SELECT;
    case 160:
        return KEY_B;
    default:
        return 0;
    }
}

static void autotest_jukebox_maybe_finish(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 passed;

#ifdef AUTOTEST_JUKEBOX_CAPTURE
    if (
        game_state == STATE_JUKEBOX &&
        telemetry_jukebox_frames >= 140
    ) {
        __asm__ volatile("swi 3");
    }
#endif
    if (
        !autotest_jukebox_entered ||
        game_state != STATE_TITLE
    ) {
        return;
    }

    passed = (
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        mmActive() &&
        frontend_mode4_active &&
        telemetry_jukebox_frames >= 176 &&
        telemetry_jukebox_track_changes == 3 &&
        telemetry_jukebox_previous_wraps == 1 &&
        telemetry_jukebox_next_wraps == 1 &&
        telemetry_jukebox_text_toggles == 2 &&
        telemetry_jukebox_text_map_commits >= 5 &&
        telemetry_jukebox_palette_commits >= 30 &&
        telemetry_jukebox_max_oam <= HARDWARE_OAM_ENTRIES &&
        telemetry_jukebox_max_oam >= 80 &&
        telemetry_jukebox_exits == 1 &&
        jukebox_song == 1 &&
        !jukebox_hide_text &&
        jukebox_fade_level == 0
    );

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'J';
    sram[3] = '1';
    sram_write_u32(4, passed);
    sram_write_u32(
        8,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(12, game_state);
    sram_write_u32(16, frontend_mode4_active);
    sram_write_u32(20, telemetry_jukebox_frames);
    sram_write_u32(24, telemetry_jukebox_track_changes);
    sram_write_u32(28, telemetry_jukebox_previous_wraps);
    sram_write_u32(32, telemetry_jukebox_next_wraps);
    sram_write_u32(36, telemetry_jukebox_text_toggles);
    sram_write_u32(40, telemetry_jukebox_text_map_commits);
    sram_write_u32(44, telemetry_jukebox_palette_commits);
    sram_write_u32(48, telemetry_jukebox_max_oam);
    sram_write_u32(52, telemetry_jukebox_exits);
    sram_write_u32(56, mmActive());
    sram_write_u32(60, jukebox_song);
    sram_write_u32(64, jukebox_hide_text);
    sram_write_u32(68, jukebox_quitting);
    sram_write_u32(72, jukebox_fade_level);
    sram_write_u32(76, MSL_NSONGS);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_DEMO_FLOW
static u8 autotest_demo_phase;
static u8 autotest_demo_checks = 1;
static u16 autotest_demo_play_frames;
static u8 autotest_demo_nonzero_keys_seen;
static u8 autotest_demo_completed;
static u8 autotest_demo_input_streams;

static u16 autotest_demo_input(void)
{
    if (
        game_state == STATE_TITLE &&
        autotest_demo_phase == 1
    ) {
        return frontend_selection < 2 ? KEY_DOWN : KEY_A;
    }
    return 0;
}

static void autotest_demo_validate_header(u8 demo_number)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    static const char *const expected_name[FRONTEND_DEMO_COUNT] = {
        "TYRIAN   ",
        "SAVARA   ",
        "SOH JIN  ",
        "MINES    ",
        "DELIANI  "
    };
    static const u8 expected_level[FRONTEND_DEMO_COUNT] = {
        9, 12, 13, 7, 5
    };
    static const u8 expected_front[FRONTEND_DEMO_COUNT] = {
        7, 5, 19, 19, 19
    };
    static const u8 expected_rear[FRONTEND_DEMO_COUNT] = {
        29, 29, 0, 0, 0
    };
    static const u8 expected_left[FRONTEND_DEMO_COUNT] = {
        21, 21, 21, 19, 19
    };
    static const u8 expected_right[FRONTEND_DEMO_COUNT] = {
        20, 21, 23, 19, 19
    };
    static const u8 expected_initial_episode[FRONTEND_DEMO_COUNT] = {
        1, 1, 2, 1, 1
    };
    static const u8 expected_shield[FRONTEND_DEMO_COUNT] = {
        6, 8, 6, 9, 9
    };
    static const u8 expected_special[FRONTEND_DEMO_COUNT] = {
        0, 2, 0, 0, 0
    };
    static const u8 expected_ship[FRONTEND_DEMO_COUNT] = {
        1, 6, 4, 4, 3
    };
    static const u8 expected_front_power[FRONTEND_DEMO_COUNT] = {
        1, 10, 3, 10, 10
    };
    static const u8 expected_rear_power[FRONTEND_DEMO_COUNT] = {
        1, 11, 1, 1, 1
    };
    static const u8 expected_song[FRONTEND_DEMO_COUNT] = {
        17, 32, 15, 15, 13
    };
    static const u16 expected_ship_graphic[FRONTEND_DEMO_COUNT] = {
        233, 195, 157, 157, 157
    };
    static const u8 expected_armor[FRONTEND_DEMO_COUNT] = {
        10, 27, 20, 20, 17
    };
    static const u8 expected_shield_power[FRONTEND_DEMO_COUNT] = {
        7, 10, 7, 12, 12
    };
    u8 index;

    if (demo_number == 0 || demo_number > FRONTEND_DEMO_COUNT) {
        autotest_demo_checks = 0;
        return;
    }
    index = (u8)(demo_number - 1u);
    autotest_demo_checks =
        autotest_demo_checks &&
        frontend_demo_active &&
        frontend_demo_file != 0 &&
        game_state == STATE_PLAY &&
        frontend_demo_number == demo_number &&
        telemetry_demo_starts == demo_number &&
        telemetry_demo_idle_starts == 1 &&
        frontend_play_mode == FRONTEND_PLAY_FULL_GAME &&
        frontend_difficulty == FRONTEND_DIFFICULTY_NORMAL &&
        frontend_level_ready &&
        source_parity_level.assets_valid &&
        catalog != 0 &&
        frontend_demo_header.episode == 1 &&
        strcmp(
            frontend_demo_header.level_name,
            expected_name[index]
        ) == 0 &&
        frontend_demo_header.lvl_file_number ==
            expected_level[index] &&
        frontend_demo_header.front_weapon ==
            expected_front[index] &&
        frontend_demo_header.rear_weapon ==
            expected_rear[index] &&
        frontend_demo_header.super_arcade_mode == 0 &&
        frontend_demo_header.sidekick[0] ==
            expected_left[index] &&
        frontend_demo_header.sidekick[1] ==
            expected_right[index] &&
        frontend_demo_header.generator == 5 &&
        frontend_demo_header.sidekick_level == 2 &&
        frontend_demo_header.sidekick_series == 1 &&
        frontend_demo_header.initial_episode ==
            expected_initial_episode[index] &&
        frontend_demo_header.shield ==
            expected_shield[index] &&
        frontend_demo_header.special ==
            expected_special[index] &&
        frontend_demo_header.ship == expected_ship[index] &&
        frontend_demo_header.front_power ==
            expected_front_power[index] &&
        frontend_demo_header.rear_power ==
            expected_rear_power[index] &&
        frontend_demo_header.source_song ==
            expected_song[index] &&
        frontend_level.lvl_file_number ==
            expected_level[index] &&
        frontend_level.source_song == expected_song[index] &&
        source_parity_level.player_front_weapon_id ==
            expected_front[index] &&
        source_parity_level.player_front_weapon_power ==
            expected_front_power[index] &&
        source_parity_level.player_rear_weapon_id ==
            expected_rear[index] &&
        source_parity_level.player_rear_weapon_power ==
            expected_rear_power[index] &&
        source_parity_level.player_special ==
            expected_special[index] &&
        source_parity_level.player_sidekick[0] ==
            expected_left[index] &&
        source_parity_level.player_sidekick[1] ==
            expected_right[index] &&
        source_parity_level.player_generator == 5 &&
        source_parity_level.player_shield_item ==
            expected_shield[index] &&
        source_parity_level.player_ship == expected_ship[index] &&
        player_generator_power == 30 &&
        player_ship_graphic == expected_ship_graphic[index] &&
        player_ship_animation == 2 &&
        player_armor == expected_armor[index] &&
        player_shield == expected_shield_power[index] &&
        player_shield_max ==
            (u8)(expected_shield_power[index] * 2u) &&
        catalog->selected_mus_song == expected_song[index];
}

static void autotest_demo_finish_to_sram(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 passed =
        autotest_demo_checks &&
        autotest_demo_completed == FRONTEND_DEMO_COUNT &&
        autotest_demo_input_streams == FRONTEND_DEMO_COUNT &&
        game_state == STATE_TITLE &&
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !frontend_demo_active &&
        frontend_demo_file == 0 &&
        frontend_demo_number == FRONTEND_DEMO_COUNT &&
        telemetry_demo_starts == FRONTEND_DEMO_COUNT &&
        telemetry_demo_idle_starts == 1 &&
        telemetry_demo_aborts == FRONTEND_DEMO_COUNT &&
        telemetry_demo_finishes == 0 &&
        telemetry_demo_parse_failures == 0 &&
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        mmActive();

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'D';
    sram[3] = 'M';
    sram_write_u32(4, passed);
    sram_write_u32(8, telemetry_demo_starts);
    sram_write_u32(12, telemetry_demo_idle_starts);
    sram_write_u32(16, telemetry_demo_aborts);
    sram_write_u32(20, telemetry_demo_finishes);
    sram_write_u32(24, telemetry_demo_parse_failures);
    sram_write_u32(28, frontend_demo_number);
    sram_write_u32(32, game_state);
    sram_write_u32(36, frontend_mode4_active);
    sram_write_u32(40, frontend_frame_pending);
    sram_write_u32(
        44,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(48, mmActive());
    sram_write_u32(52, autotest_demo_input_streams);
    sram_write_u32(56, FRONTEND_DEMO_IDLE_VBLANKS);
    sram_write_u32(60, MSL_NSONGS);
    __asm__ volatile("swi 3");
}

static void autotest_demo_maybe_finish(void)
{
    if (frontend_demo_active) {
        if (
            (autotest_demo_phase == 0 &&
             telemetry_demo_idle_starts == 1) ||
            autotest_demo_phase == 1
        ) {
            u8 demo_number = (u8)(autotest_demo_completed + 1u);

            autotest_demo_validate_header(demo_number);
            autotest_demo_phase = 2;
            autotest_demo_play_frames = 0;
            autotest_demo_nonzero_keys_seen = 0;
        }
        if (autotest_demo_phase == 2) {
            if (frontend_demo_keys != 0) {
                autotest_demo_nonzero_keys_seen = 1;
            }
            autotest_demo_play_frames++;
            if (autotest_demo_play_frames >= 120) {
                autotest_demo_checks =
                    autotest_demo_checks &&
                    !insert_coin_tile_upload_pending &&
                    autotest_demo_nonzero_keys_seen;
                if (autotest_demo_nonzero_keys_seen) {
                    autotest_demo_input_streams++;
                }
                autotest_demo_completed++;
                frontend_demo_finish(1);
                autotest_demo_phase =
                    autotest_demo_completed >=
                        FRONTEND_DEMO_COUNT ?
                            3 :
                            1;
            }
        }
        return;
    }
    if (
        autotest_demo_phase == 3 &&
        game_state == STATE_TITLE &&
        !frontend_frame_pending
    ) {
        autotest_demo_finish_to_sram();
    }
}
#endif
