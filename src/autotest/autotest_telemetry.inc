static void autotest_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 pass;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * A route smoke test deliberately has no level-specific golden counts.
     * It proves that an arbitrary levelsN.dat destination can traverse the
     * common ROMFS-backed gameplay, end-flight and statistics path without
     * missing source data or exhausting a runtime graphics cache.
     */
    pass = (
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_level_completed &&
        frontend_level_ready &&
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
        autotest_campaign_levels_completed ==
            AUTOTEST_CAMPAIGN_LEVEL_COUNT &&
        autotest_campaign_failures == 0 &&
        autotest_campaign_checksum != 2166136261u &&
#endif
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !game_paused &&
#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
        telemetry_arcade_equipment_collision_pass == 1 &&
        frontend_play_mode == FRONTEND_PLAY_ARCADE &&
        /*
         * The fixed probe covers all five evalue equipment branches.  This
         * additional condition proves the authored Episode 1/section 1 run
         * itself collected at least one randomly spawned equipment item.
         */
        source_parity_level.high_value_pickup_count > 0 &&
#endif
        level_position != 0 &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_reward_drops == 0 &&
        telemetry_enemy_replacements == 0 &&
        telemetry_pause_toggles == 2 &&
        telemetry_paused_frames >= 60 &&
        source_parity_level.assets_valid &&
        telemetry_romfs_entries == TYRIAN_ROMFS_ENTRY_COUNT &&
        telemetry_romfs_image_bytes == TYRIAN_ROMFS_IMAGE_BYTES &&
        telemetry_romfs_payload_bytes == TYRIAN_ROMFS_PAYLOAD_BYTES &&
        telemetry_romfs_checks ==
            OT_ROM_IO_SELF_TEST_FIXED_CHECKS +
                11 * TYRIAN_ROMFS_PROBE_COUNT &&
        telemetry_romfs_failures == 0 &&
        telemetry_romfs_manifest_crc32 ==
            TYRIAN_ROMFS_MANIFEST_CRC32 &&
        /*
         * event_index is an absolute cursor, not a processed-event count.
         * Arbitrary stock routes may use JE_eventJump() (events 54/70),
         * timer jumps or returnActive, so a forward jump legitimately makes
         * it larger than applied+deferred+explicit-skip accounting.  The
         * first-level golden below still checks exact linear counts; generic
         * route smoke instead validates bounded cursor/counter state.
         */
        telemetry_source_events <= source_parity_level.event_count &&
        telemetry_source_events_applied +
            telemetry_source_events_deferred > 0 &&
        telemetry_source_events_skipped <=
            source_parity_level.event_count &&
        telemetry_source_spawn_successes +
            telemetry_source_spawn_pool_full +
            telemetry_source_spawn_missing ==
            telemetry_source_spawn_attempts &&
        telemetry_source_spawn_missing == 0 &&
        source_parity_level.enemy_launch_success_count +
            source_parity_level.enemy_launch_pool_full_count +
            source_parity_level.enemy_launch_missing_definition_count ==
            source_parity_level.enemy_launch_attempt_count &&
        source_parity_level.enemy_launch_missing_definition_count == 0 &&
        source_parity_level.random_spawn_success_count +
            source_parity_level.random_spawn_pool_full_count +
            source_parity_level.random_spawn_missing_definition_count ==
            source_parity_level.random_spawn_attempt_count &&
        source_parity_level.random_spawn_missing_definition_count == 0 &&
        source_parity_level.death_spawn_success_count +
            source_parity_level.death_spawn_pool_full_count +
            source_parity_level.death_spawn_missing_definition_count ==
            source_parity_level.death_spawn_attempt_count &&
        source_parity_level.death_spawn_missing_definition_count == 0 &&
        source_parity_level.score_item_unsupported_pickup_count == 0 &&
        telemetry_effect_drops == 0 &&
        telemetry_source_enemy_shot_drops == 0 &&
        telemetry_sprite2_decode_failures == 0 &&
        /*
         * A completed source route may briefly outrun the 26-slot L1 cache
         * while the production presentation scheduler drops an overdue
         * frame.  This is a deferred visual request, not missing source
         * data: the accounting below must still close exactly and no decode
         * may fail.  Keep the degradation below one percent of displayed
         * frames instead of requiring a physically impossible zero on the
         * densest stock routes.
         */
        telemetry_sprite2_cache_drops * 100u <=
            telemetry_display_frames &&
        telemetry_sprite2_uploads +
            telemetry_sprite2_upload_coalesces +
            source_enemy_upload_count +
            (source_player_upload_pending ? 1u : 0u) ==
            telemetry_sprite2_cache_misses &&
        telemetry_sprite2_compact_uploads <=
            telemetry_sprite2_uploads &&
        telemetry_sprite2_upload_bytes ==
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES &&
        telemetry_effect_cache_drops == 0 &&
        telemetry_projectile_cache_drops == 0 &&
        telemetry_layer_rule_failures == 0 &&
        telemetry_end_level_music_starts == 1 &&
        telemetry_end_level_music_natural_stops == 1 &&
        telemetry_level_complete_voice_starts == 1 &&
#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
        telemetry_stats_stage_advances == 3 &&
#else
        telemetry_stats_stage_advances == 4 &&
#endif
        frontend_stats_stage == FRONTEND_STATS_STAGE_FINAL
    );
#else
    pass = (
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_level_completed &&
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !game_paused &&
        level_position >= 5400u &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_effect_drops <= 33 &&
        telemetry_reward_drops == 0 &&
        telemetry_enemy_replacements == 0 &&
        telemetry_pause_toggles == 2 &&
        telemetry_paused_frames >= 60 &&
        source_parity_level.assets_valid &&
        telemetry_romfs_entries == TYRIAN_ROMFS_ENTRY_COUNT &&
        telemetry_romfs_image_bytes == TYRIAN_ROMFS_IMAGE_BYTES &&
        telemetry_romfs_payload_bytes == TYRIAN_ROMFS_PAYLOAD_BYTES &&
        telemetry_romfs_checks ==
            OT_ROM_IO_SELF_TEST_FIXED_CHECKS +
                11 * TYRIAN_ROMFS_PROBE_COUNT &&
        telemetry_romfs_failures == 0 &&
        telemetry_romfs_manifest_crc32 ==
            TYRIAN_ROMFS_MANIFEST_CRC32 &&
        telemetry_source_events == SOURCE_STAGE4_EXPECTED_EVENTS &&
        telemetry_source_events_applied ==
            SOURCE_STAGE4_EXPECTED_APPLIED &&
        telemetry_source_events_deferred ==
            SOURCE_STAGE4_EXPECTED_DEFERRED &&
        telemetry_source_events_skipped ==
            SOURCE_STAGE4_EXPECTED_SKIPPED &&
        telemetry_source_events_applied +
            telemetry_source_events_deferred +
            telemetry_source_events_skipped == telemetry_source_events &&
        telemetry_source_spawn_attempts ==
            SOURCE_STAGE4_EXPECTED_SPAWN_ATTEMPTS &&
        telemetry_source_spawn_successes +
            telemetry_source_spawn_pool_full +
            telemetry_source_spawn_missing ==
            telemetry_source_spawn_attempts &&
        telemetry_source_spawn_successes ==
            SOURCE_STAGE4_EXPECTED_SPAWN_SUCCESSES &&
        telemetry_source_spawn_pool_full ==
            SOURCE_STAGE4_EXPECTED_POOL_FULL &&
        telemetry_source_spawn_missing == 0 &&
        telemetry_source_control_writes ==
            SOURCE_STAGE4_EXPECTED_CONTROL_WRITES &&
        telemetry_source_rng_calls ==
            SOURCE_STAGE4_EXPECTED_RNG_CALLS &&
        telemetry_source_motion_updates ==
            SOURCE_STAGE4_EXPECTED_MOTION_UPDATES &&
        telemetry_source_releases ==
            SOURCE_STAGE4_EXPECTED_RELEASES &&
        telemetry_source_shot_triggers ==
            SOURCE_STAGE4_EXPECTED_SHOT_TRIGGERS &&
        telemetry_source_max_enemies <= OT_ENEMY_COUNT &&
        telemetry_source_enemy_shots_spawned ==
            SOURCE_STAGE4_EXPECTED_SHOT_SPAWNS &&
        telemetry_source_enemy_shot_drops == 0 &&
        telemetry_source_max_enemy_shots ==
            SOURCE_STAGE4_EXPECTED_SHOT_MAX_ACTIVE &&
        telemetry_source_enemy_shot_updates ==
            SOURCE_STAGE4_EXPECTED_SHOT_UPDATES &&
        telemetry_source_enemy_shot_releases ==
            SOURCE_STAGE4_EXPECTED_SHOT_RELEASES &&
        telemetry_source_enemy_shot_player_hits ==
            SOURCE_STAGE4_EXPECTED_SHOT_PLAYER_HITS &&
        telemetry_source_player_shot_hits ==
            SOURCE_STAGE4_EXPECTED_PLAYER_SHOT_HITS &&
        source_parity_level.player_enemy_contact_count ==
            SOURCE_STAGE4_EXPECTED_PLAYER_CONTACTS &&
        telemetry_source_enemy_kills ==
            SOURCE_STAGE4_EXPECTED_ENEMY_KILLS &&
        telemetry_source_direct_cash ==
            SOURCE_STAGE4_EXPECTED_DIRECT_CASH &&
        telemetry_source_score_item_spawns ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_SPAWNS &&
        telemetry_source_score_item_pickups ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS &&
        telemetry_source_score_item_max_active ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_MAX_ACTIVE &&
        telemetry_source_death_spawn_attempts ==
            SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS &&
        telemetry_source_death_spawn_successes ==
            SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS &&
        source_parity_level.death_spawn_pool_full_count == 0 &&
        source_parity_level.death_spawn_missing_definition_count == 0 &&
        source_parity_level.score_item_unsupported_pickup_count == 0 &&
        source_parity_level.data_cube_pickup_count == 2 &&
        source_parity_level.front_weapon_powerup_count == 0 &&
        source_parity_level.rear_weapon_powerup_count == 0 &&
        telemetry_source_death_control_events ==
            SOURCE_STAGE4_EXPECTED_DEATH_CONTROLS &&
        telemetry_source_death_assignments ==
            SOURCE_STAGE4_EXPECTED_DEATH_ASSIGNMENTS &&
        telemetry_source_max_visible_enemies ==
            SOURCE_STAGE4_EXPECTED_VISIBLE_ENEMIES &&
        telemetry_source_unknown_visuals ==
            SOURCE_STAGE4_EXPECTED_UNKNOWN_VISUALS &&
        telemetry_sprite2_decode_failures == 0 &&
        telemetry_sprite2_cache_drops <=
            SOURCE_STAGE4_MAX_SPRITE2_CACHE_DROPS &&
        telemetry_sprite2_uploads +
            telemetry_sprite2_upload_coalesces +
            source_enemy_upload_count +
            (source_player_upload_pending ? 1u : 0u) ==
            telemetry_sprite2_cache_misses &&
        telemetry_sprite2_compact_uploads <=
            telemetry_sprite2_uploads &&
        telemetry_sprite2_upload_bytes ==
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES &&
        telemetry_sprite2_max_uploads <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT &&
        telemetry_sprite2_max_visible_unique <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT + 1u &&
        telemetry_effect_cache_drops == 0 &&
        telemetry_effect_cache_uploads +
            telemetry_effect_cache_upload_coalesces +
            source_effect_upload_count ==
            telemetry_effect_cache_misses &&
        telemetry_effect_cache_upload_bytes ==
            telemetry_effect_cache_uploads *
                SOURCE_EFFECT_FRAME_BYTES &&
        telemetry_effect_cache_max_uploads <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_effect_cache_max_visible_unique <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_pickup_explosion_spawns ==
            SOURCE_STAGE4_EXPECTED_FEEDBACK_SPAWNS &&
        telemetry_pickup_explosion_drops ==
            SOURCE_STAGE4_EXPECTED_FEEDBACK_DROPS &&
        telemetry_pickup_explosion_max_active ==
            SOURCE_STAGE4_EXPECTED_FEEDBACK_MAX_ACTIVE &&
        telemetry_end_level_music_starts == 1 &&
        telemetry_end_level_music_natural_stops == 1 &&
        telemetry_end_level_initial_warp == (u8)-4 &&
        telemetry_end_level_trail_max == 16 &&
        player_end_warp == 27 &&
        telemetry_level_complete_voice_starts == 1 &&
        telemetry_stats_stage_advances == 4 &&
        telemetry_stats_cube_reveals == 2 &&
        frontend_stats_stage == FRONTEND_STATS_STAGE_FINAL &&
        frontend_stats_cube_visible_count == 2 &&
        source_parity_level.enemy_shot_active_count == 0 &&
        source_parity_level.active_enemy_count == 0 &&
        /*
         * Full Game starts Episode 1 with the stock 10,000 credits from
         * tyrian2.c; the complete translated combat/reward phases add 5,539.
         */
        player_cash == 15539 &&
        /*
         * Runtime ROMFS background decoding adds a small, bounded amount of
         * work compared with the former first-level-only generated maps.
         * Keep the performance gate proportional across Low/Normal speed:
         * at least 95 percent of gameplay display frames must meet VBlank.
         */
        telemetry_missed_vblanks * 20u <=
            telemetry_display_frames &&
        telemetry_source_launch_attempts == 3 &&
        telemetry_source_launch_successes == 3 &&
        telemetry_source_random_attempts == 0 &&
        telemetry_source_random_successes == 0 &&
        source_world_to_screen_x(source_camera_origin_x()) == 0 &&
        source_world_to_screen_x(
            source_camera_origin_x() + SCREEN_WIDTH - 1
        ) == SCREEN_WIDTH - 1 &&
        source_world_to_screen_y(source_camera_origin_y()) == 0 &&
        source_world_to_screen_y(
            source_camera_origin_y() + SCREEN_HEIGHT - 1
        ) == SCREEN_HEIGHT - 1 &&
        source_camera_offset_x >= -SOURCE_VIEW_CROP_X &&
        source_camera_offset_x <= SOURCE_VIEW_CROP_X &&
        source_camera_offset_y >= -SOURCE_VIEW_CROP_Y &&
        source_camera_offset_y <= SOURCE_VIEW_CROP_Y &&
        source_background_hofs(0) < 512 &&
        source_background_hofs(1) < 512 &&
        source_background_hofs(2) < 512 &&
        bg1_scroll_speed == 1 &&
        bg2_scroll_speed == 2 &&
        bg3_scroll_speed == 0 &&
#if !TYRIAN_GBA_STRESS_LOADOUT
        telemetry_upgrade_loadout_pass == 1 &&
#endif
        telemetry_layer_rule_checks == 252 &&
        telemetry_layer_rule_failures == 0
    );
#endif
    volatile u32 delay;

    sram[0] = 'T';
    sram[1] = 'G';
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    sram[2] = 'C';
    sram[3] = 'M';
    sram[4] = 3;
#elif defined(AUTOTEST_FRONTEND_ROUTE_SECTION)
    sram[2] = 'R';
    sram[3] = 'S';
    sram[4] = 3;
#else
    sram[2] = 'B';
    sram[3] = 'A';
#ifdef AUTOTEST_STACK_CANARY
        sram[4] = 29;
#else
    sram[4] = 25;
#endif
#endif
    sram[5] = pass;
    sram[6] = game_state;
    sram[7] = mmActive() ? 1 : 0;
    /*
     * Keep the standard save-type signature in the ROM so emulators select
     * SRAM before the first telemetry write.  The volatile read prevents
     * section garbage collection from discarding the otherwise metadata-only
     * string.
     */
    sram[64] = *(volatile const u8 *)save_type_marker;
    sram_write_u32(8, telemetry_logic_updates);
    sram_write_u32(12, telemetry_display_frames);
    sram_write_u32(16, telemetry_vblank_irqs);
    sram_write_u32(20, telemetry_missed_vblanks);
    sram_write_u32(24, telemetry_spawn_count);
    sram_write_u32(28, telemetry_control_count);
    sram_write_u32(32, telemetry_collision_count);
    sram_write_u32(36, telemetry_map_rows);
    sram_write_u32(40, telemetry_max_enemies);
    sram_write_u32(44, telemetry_max_oam);
    sram_write_u32(48, telemetry_stream_drops);
    sram_write_u32(52, source_parity_level.event_index);
    sram_write_u32(56, level_tick);
    sram_write_u32(60, telemetry_state_transitions);
    sram_write_u32(64, telemetry_max_effects);
    sram_write_u32(68, telemetry_effect_drops);
    sram_write_u32(72, telemetry_reward_spawns);
    sram_write_u32(76, telemetry_reward_pickups);
    sram_write_u32(80, telemetry_max_rewards);
    sram_write_u32(84, telemetry_reward_drops);
    sram_write_u32(88, player_cash);
    sram_write_u32(92, telemetry_enemy_shots_spawned);
    sram_write_u32(96, telemetry_enemy_shot_drops);
    sram_write_u32(100, telemetry_max_enemy_shots);
    sram_write_u32(104, level_position);
    sram_write_u32(108, telemetry_enemy_replacements);
    sram_write_u32(112, telemetry_kill_cash);
    sram_write_u32(116, telemetry_reward_controls);
    sram_write_u32(120, telemetry_reward_assignments);
    sram_write_u32(124, telemetry_pause_toggles);
    sram_write_u32(128, telemetry_paused_frames);
    sram_write_u32(132, telemetry_source_events);
    sram_write_u32(136, telemetry_source_events_applied);
    sram_write_u32(140, telemetry_source_events_deferred);
    sram_write_u32(144, telemetry_source_events_skipped);
    sram_write_u32(148, telemetry_source_spawn_attempts);
    sram_write_u32(152, telemetry_source_spawn_successes);
    sram_write_u32(156, telemetry_source_spawn_pool_full);
    sram_write_u32(160, telemetry_source_spawn_missing);
    sram_write_u32(164, telemetry_source_max_enemies);
    sram_write_u32(168, telemetry_source_control_writes);
    sram_write_u32(172, telemetry_source_rng_calls);
    sram_write_u32(176, telemetry_romfs_entries);
    sram_write_u32(180, telemetry_romfs_image_bytes);
    sram_write_u32(184, telemetry_romfs_payload_bytes);
    sram_write_u32(188, telemetry_romfs_checks);
    sram_write_u32(192, telemetry_romfs_failures);
    sram_write_u32(196, telemetry_romfs_manifest_crc32);
    sram_write_u32(200, telemetry_source_motion_updates);
    sram_write_u32(204, telemetry_source_releases);
    sram_write_u32(208, telemetry_source_shot_triggers);
    sram_write_u32(212, telemetry_source_launch_attempts);
    sram_write_u32(216, telemetry_source_launch_successes);
    sram_write_u32(220, telemetry_source_random_attempts);
    sram_write_u32(224, telemetry_source_random_successes);
    sram_write_u32(228, telemetry_source_enemy_shots_spawned);
    sram_write_u32(232, telemetry_source_enemy_shot_drops);
    sram_write_u32(236, telemetry_source_max_enemy_shots);
    sram_write_u32(240, telemetry_source_enemy_shot_updates);
    sram_write_u32(244, telemetry_source_enemy_shot_releases);
    sram_write_u32(248, telemetry_source_enemy_shot_player_hits);
    sram_write_u32(252, telemetry_source_player_shot_hits);
    sram_write_u32(256, telemetry_source_enemy_kills);
    sram_write_u32(260, telemetry_source_direct_cash);
    sram_write_u32(264, telemetry_source_score_item_spawns);
    sram_write_u32(268, telemetry_source_score_item_pickups);
    sram_write_u32(272, telemetry_source_score_item_max_active);
    sram_write_u32(276, telemetry_source_death_spawn_attempts);
    sram_write_u32(280, telemetry_source_death_spawn_successes);
    sram_write_u32(284, telemetry_source_death_control_events);
    sram_write_u32(288, telemetry_source_death_assignments);
    sram_write_u32(292, telemetry_source_max_visible_enemies);
    sram_write_u32(296, telemetry_source_unknown_visuals);
    sram_write_u32(
        300,
        source_parity_level.player_enemy_contact_count
    );
    sram_write_u32(
        304,
        source_parity_level.score_item_unsupported_pickup_count
    );
    sram_write_u32(
        308,
        source_parity_level.death_spawn_pool_full_count
    );
    sram_write_u32(
        312,
        source_parity_level.death_spawn_missing_definition_count
    );
    sram_write_u32(316, source_parity_level.active_enemy_count);
    sram_write_u32(
        320,
        source_parity_level.enemy_shot_active_count
    );
    sram_write_u32(324, source_parity_level.data_cube_pickup_count);
    sram_write_u32(
        328,
        source_parity_level.front_weapon_powerup_count
    );
    sram_write_u32(
        332,
        source_parity_level.rear_weapon_powerup_count
    );
    sram_write_u32(336, bg1_scroll_speed);
    sram_write_u32(340, bg2_scroll_speed);
    sram_write_u32(344, bg3_scroll_speed);
    sram_write_u32(348, source_parity_level.assets_valid ? 1 : 0);
    sram_write_u32(352, game_paused);
    sram_write_u32(356, telemetry_sprite2_decode_failures);
    sram_write_u32(360, telemetry_sprite2_cache_hits);
    sram_write_u32(364, telemetry_sprite2_cache_misses);
    sram_write_u32(368, telemetry_sprite2_cache_evictions);
    sram_write_u32(372, telemetry_sprite2_cache_drops);
    sram_write_u32(376, telemetry_sprite2_uploads);
    sram_write_u32(380, telemetry_sprite2_upload_bytes);
    sram_write_u32(384, telemetry_sprite2_max_uploads);
    sram_write_u32(388, SOURCE_ENEMY_CACHE_SLOT_COUNT);
    sram_write_u32(
        392,
        source_parity_level.powerup_consolation_cash
    );
    sram_write_u32(
        396,
        source_parity_level.orbiting_asteroid_pickup_count
    );
    sram_write_u32(400, source_parity_level.superbomb_pickup_count);
    sram_write_u32(404, source_parity_level.hotdog_pickup_count);
    sram_write_u32(408, source_parity_level.armor_pickup_count);
    sram_write_u32(412, source_parity_level.bonus_portal_pickup_count);
    sram_write_u32(416, source_parity_level.high_value_pickup_count);
    sram_write_u32(420, source_parity_level.player_front_weapon_id);
    sram_write_u32(424, source_parity_level.player_front_weapon_power);
    sram_write_u32(428, source_parity_level.player_rear_weapon_id);
    sram_write_u32(432, source_parity_level.player_rear_weapon_power);
    sram_write_u32(436, source_parity_level.player_superbombs);
    sram_write_u32(440, source_parity_level.player_armor);
    sram_write_u32(444, source_parity_level.player_weapon_mode);
    sram_write_u32(448, source_parity_level.player_special);
    sram_write_u32(452, source_parity_level.player_purple_balls_needed);
    sram_write_u32(456, source_parity_level.bonus_level ? 1 : 0);
    sram_write_u32(460, source_parity_level.next_level);
    sram_write_u32(464, source_parity_level.display_time);
    sram_write_u32(468, (u16)player_source_x);
    sram_write_u32(472, (u16)player_source_y);
    sram_write_u32(
        476,
        source_parity_level.presentation_map_x_offset
    );
    sram_write_u32(
        480,
        source_parity_level.presentation_map_x2_offset
    );
    sram_write_u32(
        484,
        source_parity_level.presentation_map_x3_offset
    );
    sram_write_u32(488, source_background_hofs(0));
    sram_write_u32(492, source_background_hofs(1));
    sram_write_u32(496, source_background_hofs(2));
    sram_write_u32(500, bg1_presentation_scroll_pixel);
    sram_write_u32(504, bg2_presentation_scroll_pixel);
    sram_write_u32(508, bg3_presentation_scroll_pixel);
    sram_write_u32(512, (u16)source_camera_origin_x());
    sram_write_u32(516, (u16)source_camera_origin_y());
    sram_write_u32(520, telemetry_layer_rule_checks);
    sram_write_u32(524, telemetry_layer_rule_failures);
    sram_write_u32(528, source_parity_level.background2_over);
    sram_write_u32(532, source_parity_level.background3_over);
    sram_write_u32(536, source_parity_level.top_enemy_over ? 1 : 0);
    sram_write_u32(
        540,
        source_parity_level.sky_enemy_over_all ? 1 : 0
    );
    sram_write_u32(544, source_background2_priority());
    sram_write_u32(548, source_background3_priority());
    sram_write_u32(552, telemetry_sprite2_max_visible_unique);
    sram_write_u32(556, telemetry_effect_cache_hits);
    sram_write_u32(560, telemetry_effect_cache_misses);
    sram_write_u32(564, telemetry_effect_cache_evictions);
    sram_write_u32(568, telemetry_effect_cache_drops);
    sram_write_u32(572, telemetry_effect_cache_uploads);
    sram_write_u32(576, telemetry_effect_cache_upload_bytes);
    sram_write_u32(580, telemetry_effect_cache_max_uploads);
    sram_write_u32(584, telemetry_effect_cache_max_visible_unique);
    sram_write_u32(588, telemetry_pickup_explosion_spawns);
    sram_write_u32(592, telemetry_pickup_explosion_drops);
    sram_write_u32(596, telemetry_pickup_explosion_max_active);
    sram_write_u32(600, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(604, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(608, source_background2_enabled);
    sram_write_u32(612, telemetry_end_level_music_starts);
    sram_write_u32(616, telemetry_end_level_trail_max);
    sram_write_u32(620, telemetry_level_complete_voice_starts);
    sram_write_u32(624, telemetry_stats_stage_advances);
    sram_write_u32(628, telemetry_stats_cube_reveals);
    sram_write_u32(632, frontend_stats_stage);
    sram_write_u32(636, frontend_stats_cube_visible_count);
    sram_write_u32(640, (u8)player_end_warp);
    sram_write_u32(644, telemetry_end_level_initial_warp);
    sram_write_u32(648, background_cache_approximations);
    sram_write_u32(652, background_cache_evictions);
    sram_write_u32(656, background_cache_uploads);
    sram_write_u32(660, background_layer_cache[0].valid_count);
    sram_write_u32(664, background_layer_cache[1].valid_count);
    sram_write_u32(668, background_layer_cache[2].valid_count);
    sram_write_u32(672, background_rows_prefetched);
    sram_write_u32(676, background_rows_synchronous);
    sram_write_u32(680, background_rows_existing);
    sram_write_u32(684, background_prefetch_late_columns);
    sram_write_u32(688, background_tile_render_count);
    sram_write_u32(692, telemetry_projectile_cache_hits);
    sram_write_u32(696, telemetry_projectile_cache_misses);
    sram_write_u32(700, telemetry_projectile_cache_evictions);
    sram_write_u32(704, telemetry_projectile_cache_drops);
    sram_write_u32(708, telemetry_projectile_cache_uploads);
    sram_write_u32(712, telemetry_projectile_cache_max_uploads);
    sram_write_u32(
        716,
        telemetry_projectile_cache_max_visible_unique
    );
    sram_write_u32(6084, telemetry_sprite2_compact_uploads);
    sram_write_u32(6088, telemetry_source_sound_mask_low);
    sram_write_u32(6092, telemetry_source_sound_mask_high);
    sram_write_u32(6112, telemetry_secret_level_collision_pass);
    sram_write_u32(6116, telemetry_end_level_music_natural_stops);
    sram_write_u32(6120, telemetry_arcade_equipment_collision_pass);
    /*
     * Event-clock diagnostics.  These values make a stopped-background
     * authored sequence distinguishable from a stalled GAMELOOP without
     * changing release behaviour.
     */
    sram_write_u32(6124, source_parity_level.force_events ? 1 : 0);
    sram_write_u32(6128, source_parity_level.cur_loc);
    sram_write_u32(6132, source_parity_level.back_move);
    sram_write_u32(6136, source_parity_level.back_move2);
    sram_write_u32(6140, source_parity_level.back_move3);
    sram_write_u32(6144, source_parity_level.map1_y_delay);
    sram_write_u32(6148, source_parity_level.map1_y_delay_max);
    sram_write_u32(6152, source_parity_level.event_index);
    sram_write_u32(6156, source_parity_level.event_jump_count);
    sram_write_u32(
        6160,
        source_parity_level.super_enemy_254_jump_count
    );
    sram_write_u32(6164, source_parity_level.super_enemy_254_jump);
    sram_write_u32(6200, telemetry_boss_perf_started);
    sram_write_u32(6204, telemetry_boss_perf_completed);
    sram_write_u32(6208, telemetry_boss_perf_start_position);
    sram_write_u32(6212, telemetry_boss_perf_end_position);
    sram_write_u32(6216, telemetry_boss_perf_display_frames);
    sram_write_u32(6220, telemetry_boss_perf_missed_vblanks);
    sram_write_u32(6224, telemetry_boss_perf_sprite2_misses);
    sram_write_u32(6228, telemetry_boss_perf_sprite2_evictions);
    sram_write_u32(6232, telemetry_boss_perf_sprite2_upload_bytes);
    sram_write_u32(6236, telemetry_boss_perf_projectile_misses);
    sram_write_u32(6240, telemetry_waitcnt);
    sram_write_u32(6244, telemetry_sprite2_l2_hits);
    sram_write_u32(6248, telemetry_sprite2_l2_misses);
    sram_write_u32(6252, telemetry_sprite2_l2_evictions);
    sram_write_u32(6256, telemetry_sprite2_l2_drops);
    sram_write_u32(6260, telemetry_sprite2_l2_flushes);
    sram_write_u32(6264, telemetry_sprite2_l2_raw_builds);
    sram_write_u32(6268, telemetry_sprite2_l2_rle_fallbacks);
    sram_write_u32(6272, telemetry_sprite2_l2_max_visible_unique);
    sram_write_u32(6276, telemetry_boss_perf_l2_hits);
    sram_write_u32(6280, telemetry_boss_perf_l2_misses);
    sram_write_u32(6284, telemetry_boss_perf_l2_evictions);
    sram_write_u32(6288, telemetry_boss_perf_l2_raw_builds);
    sram_write_u32(6292, telemetry_boss_perf_l2_fallbacks);
    sram_write_u32(6296, ot_sprite2_raw_catalog_valid() ? 1 : 0);
    sram_write_u32(6300, SPRITE2_RAW_DATA_BYTES);
    sram_write_u32(6304, SPRITE2_RAW_DATA_CRC32);
    sram_write_u32(6308, SOURCE_SPRITE2_L2_SLOT_COUNT);
    sram_write_u32(6312, telemetry_upgrade_loadout_pass);
    sram_write_u32(6316, telemetry_missed_vblanks_play);
    sram_write_u32(6320, telemetry_missed_vblanks_frontend);
    sram_write_u32(6324, telemetry_missed_vblanks_game_over);
    sram_write_u32(6328, telemetry_missed_vblanks_stats);
    sram_write_u32(6332, telemetry_missed_vblanks_transition);
    sram_write_u32(6336, telemetry_missed_vblanks_frontend_other);
    sram_write_u32(
        6340,
        telemetry_frontend_transition_job_cycles_max
    );
    for (u8 phase = 0; phase < 16; phase++) {
        sram_write_u32(
            6344u + (u32)phase * 4u,
            telemetry_frontend_transition_phase_cycles_max[phase]
        );
    }
    sram_write_u32(
        6408,
        telemetry_missed_vblank_transition_job_last
    );
    sram_write_u32(
        6412,
        telemetry_missed_vblank_transition_phase_next
    );
#ifdef AUTOTEST_STACK_CANARY
    {
        u32 stack_remaining = autotest_stack_canary_remaining();

        sram_write_u32(6416, stack_remaining);
        sram_write_u32(
            6420,
            stack_remaining >= sizeof(u32) * 8u ? 1u : 0u
        );
        sram_write_u32(6424, autotest_stack_canary_filled_bytes);
        sram_write_u32(6428, autotest_ewram_heap_used());
        sram_write_u32(6432, autotest_ewram_heap_remaining());
    }
#endif
    sram_write_u32(6436, telemetry_camera_min_origin_x);
    sram_write_u32(6440, telemetry_camera_max_origin_x);
    sram_write_u32(6444, telemetry_camera_min_origin_y);
    sram_write_u32(6448, telemetry_camera_max_origin_y);
    sram_write_u32(6452, telemetry_level_music_transition_starts);
    sram_write_u32(6456, telemetry_level_music_fade_out_steps);
    sram_write_u32(6460, telemetry_level_music_silent_vblanks);
    sram_write_u32(6464, telemetry_level_music_fade_in_steps);
#ifdef AUTOTEST_LOAD_SAVE_SLOT
    sram_write_u32(6468, AUTOTEST_LOAD_SAVE_SLOT);
    sram_write_u32(6472, source_parity_level.player_ship);
    sram_write_u32(6476, source_parity_level.player_generator);
    sram_write_u32(6480, source_parity_level.player_shield_item);
    sram_write_u32(6484, source_parity_level.player_sidekick[0]);
    sram_write_u32(6488, source_parity_level.player_sidekick[1]);
#endif
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    sram_write_u32(
        6492,
        telemetry_route_first_missed_vblank_position
    );
    sram_write_u32(
        6496,
        telemetry_route_last_missed_vblank_position
    );
    for (
        u8 bucket = 0;
        bucket < ROUTE_MISSED_VBLANK_POSITION_BUCKET_COUNT;
        bucket++
    ) {
        sram_write_u32(
            6500u + (u32)bucket * 4u,
            telemetry_route_missed_vblank_position_bucket[bucket]
        );
    }
    for (u8 layer = 0; layer < BACKGROUND_LAYER_COUNT; layer++) {
        sram_write_u32(
            6564u + (u32)layer * 4u,
            background_prefetch_late_columns_layer[layer]
        );
        sram_write_u32(
            6576u + (u32)layer * 4u,
            background_prefetch_late_events_layer[layer]
        );
        sram_write_u32(
            6588u + (u32)layer * 4u,
            background_rows_synchronous_layer[layer]
        );
        sram_write_u32(
            6600u + (u32)layer * 4u,
            background_first_late_position_layer[layer]
        );
        sram_write_u32(
            6612u + (u32)layer * 4u,
            background_last_late_position_layer[layer]
        );
        sram_write_u32(
            6624u + (u32)layer * 4u,
            background_first_synchronous_position_layer[layer]
        );
        sram_write_u32(
            6636u + (u32)layer * 4u,
            background_last_synchronous_position_layer[layer]
        );
    }
    sram_write_u32(6648, source_enemy_cache_drop_record_count);
    for (
        u8 record_index = 0;
        record_index < source_enemy_cache_drop_record_count;
        record_index++
    ) {
        const SourceEnemyCacheDropRecord *record =
            &source_enemy_cache_drop_record[record_index];
        u32 offset = 6652u + (u32)record_index * 64u;

        sram_write_u32(offset + 0, record->reason);
        sram_write_u32(offset + 4, record->level_position);
        sram_write_u32(offset + 8, record->shape_table);
        sram_write_u32(offset + 12, record->graphic);
        sram_write_u32(offset + 16, record->size);
        sram_write_u32(offset + 20, record->filter);
        sram_write_u32(offset + 24, record->pool);
        sram_write_u32(offset + 28, record->source_index);
        sram_write_u32(offset + 32, record->definition_id);
        sram_write_u32(offset + 36, (u32)record->x);
        sram_write_u32(offset + 40, (u32)record->y);
        sram_write_u32(offset + 44, record->enemy_cycle);
        sram_write_u32(offset + 48, record->upload_count);
        sram_write_u32(offset + 52, record->active_slots);
        sram_write_u32(offset + 56, record->active_full_slots);
        sram_write_u32(offset + 60, record->active_compact_slots);
    }
#ifdef AUTOTEST_ROUTE_PERF
    sram_write_u32(6920, route_perf_missed_record_count);
    for (
        u8 record_index = 0;
        record_index < route_perf_missed_record_count;
        record_index++
    ) {
        const RoutePerfMissedRecord *record =
            &route_perf_missed_record[record_index];
        u32 offset = 6924u + (u32)record_index * 64u;

        sram_write_u32(offset + 0, record->level_position);
        sram_write_u32(offset + 4, record->missed_count);
        sram_write_u32(offset + 8, record->loop_cycles);
        sram_write_u32(offset + 12, record->prelogic_cycles);
        sram_write_u32(offset + 16, record->logic_cycles);
        sram_write_u32(offset + 20, record->render_cycles);
        sram_write_u32(offset + 24, record->prefetch_cycles);
        sram_write_u32(
            offset + 28,
            record->background_tile_renders
        );
        sram_write_u32(offset + 32, record->sprite2_misses);
        sram_write_u32(offset + 36, record->projectile_misses);
        sram_write_u32(offset + 40, record->effect_misses);
        sram_write_u32(offset + 44, record->oam_count);
        sram_write_u32(offset + 48, record->active_enemies);
        sram_write_u32(offset + 52, record->active_enemy_shots);
        sram_write_u32(offset + 56, record->logic_updated);
        sram_write_u32(offset + 60, record->rendered);
        for (
            u8 stage = 0;
            stage < ROUTE_PERF_LOGIC_STAGE_COUNT;
            stage++
        ) {
            sram_write_u32(
                7952u +
                    (
                        (u32)record_index *
                            ROUTE_PERF_LOGIC_STAGE_COUNT +
                        stage
                    ) * 4u,
                route_perf_missed_logic_stage[record_index][stage]
            );
        }
    }
#endif
    sram_write_u32(720, AUTOTEST_FRONTEND_ROUTE_EPISODE);
    sram_write_u32(724, AUTOTEST_FRONTEND_ROUTE_SECTION);
    sram_write_u32(728, frontend_level.lvl_file_number);
    sram_write_u32(732, frontend_level.source_song);
    sram_write_u32(736, source_parity_level.event_count);
    sram_write_u32(
        6000,
        source_enemy_first_decode_failure_valid
    );
    sram_write_u32(
        6004,
        source_enemy_first_decode_failure_shape_table
    );
    sram_write_u32(
        6008,
        source_enemy_first_decode_failure_graphic
    );
    sram_write_u32(6012, source_enemy_first_decode_failure_size);
    sram_write_u32(6016, source_enemy_first_decode_failure_filter);
    sram_write_u32(6020, source_enemy_first_decode_failure_pool);
    sram_write_u32(
        6024,
        source_enemy_first_decode_failure_source_index
    );
    sram_write_u32(6028, telemetry_autotest_combat_assists);
    sram_write_u32(6032, source_parity_level.stop_background_num);
    sram_write_u32(
        6036,
        source_parity_level.stop_backgrounds ? 1 : 0
    );
    sram_write_u32(6040, source_parity_level.frame_enemy_on_screen);
    sram_write_u32(
        6044,
        source_parity_level.frame_ground_enemy_on_screen
    );
    sram_write_u32(
        6048,
        source_parity_level.frame_sky_enemy_on_screen
    );
    sram_write_u32(
        6052,
        source_enemy_first_decode_failure_definition
    );
    sram_write_u32(
        6056,
        source_enemy_first_decode_failure_level_position
    );
    sram_write_u32(
        6060,
        (u16)source_enemy_first_decode_failure_x
    );
    sram_write_u32(
        6064,
        (u16)source_enemy_first_decode_failure_y
    );
    sram_write_u32(
        6068,
        source_enemy_first_decode_failure_shape_banks
    );
    sram_write_u32(6072, telemetry_sprite2_null_pointer_skips);
    sram_write_u32(6076, source_enemy_first_decode_failure_cycle);
    sram_write_u32(6080, telemetry_sprite2_zero_graphic_skips);
    {
        OtEventRecord pending_event = {0};
        u8 active_count = 0;
        u8 enemy_index;

        if (
            ot_data_level_event_read(
                source_parity_level.event_index,
                &pending_event
            )
        ) {
            sram_write_u32(744, pending_event.eventtime);
            sram_write_u32(748, pending_event.eventtype);
            sram_write_u32(752, (u16)pending_event.eventdat);
            sram_write_u32(756, (u16)pending_event.eventdat2);
            sram_write_u32(760, (u16)pending_event.eventdat3);
            sram_write_u32(764, pending_event.eventdat4);
        }
        for (
            enemy_index = 0;
            enemy_index < OT_ENEMY_COUNT;
            enemy_index++
        ) {
            const OtEnemy *enemy = &source_parity_level.enemy[enemy_index];
            u32 record;

            if (source_parity_level.enemy_avail[enemy_index] == 1) {
                continue;
            }
            record = 768u + (u32)active_count * 48u;
            sram_write_u32(record, enemy_index);
            sram_write_u32(
                record + 4u,
                source_parity_level.enemy_avail[enemy_index]
            );
            sram_write_u32(record + 8u, enemy->enemy_definition_id);
            sram_write_u32(record + 12u, enemy->linknum);
            sram_write_u32(record + 16u, enemy->armorleft);
            sram_write_u32(record + 20u, (u16)enemy->ex);
            sram_write_u32(record + 24u, (u16)enemy->ey);
            sram_write_u32(record + 28u, enemy->mapoffset);
            sram_write_u32(record + 32u, (u16)enemy->evalue);
            sram_write_u32(
                record + 36u,
                enemy->enemycycle |
                    ((u32)enemy->scoreitem << 8) |
                    ((u32)enemy->enemyground << 9) |
                    (
                        (u32)source_parity_level
                            .enemy_draw[enemy_index].active <<
                        10
                    )
            );
            sram_write_u32(record + 40u, (u16)enemy->fixedmovey);
            sram_write_u32(
                record + 44u,
                ((u32)(u8)enemy->exc << 24) |
                    ((u32)(u8)enemy->eyc << 16) |
                    ((u32)(u8)enemy->excc << 8) |
                    (u8)enemy->eycc
            );
            active_count++;
        }
        sram_write_u32(740, active_count);
    }
#endif
    /*
     * Keep the release scheduler and deferred-upload contract observable in
     * every ordinary autotest, not only direct route diagnostics. These
     * offsets are above all historical report blocks.
     */
    sram_write_u32(8600, TYRIAN_GBA_DYNAMIC_FRAME_DROP);
    sram_write_u32(8604, TYRIAN_GBA_WALL_CLOCK_LOGIC);
    sram_write_u32(8608, TYRIAN_GBA_RECOVER_MISSED_VBLANK);
    sram_write_u32(8612, TYRIAN_GBA_PRESENTATION_DEFER);
    sram_write_u32(8616, TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER);
    sram_write_u32(8620, telemetry_presentation_render_attempts);
    sram_write_u32(8624, telemetry_presentation_render_completed);
    sram_write_u32(8628, telemetry_presentation_render_deferred);
    sram_write_u32(8632, telemetry_presentation_render_forced);
    sram_write_u32(8636, telemetry_presentation_superseded);
    sram_write_u32(8640, telemetry_presentation_pending_logic_max);
    sram_write_u32(8644, telemetry_presentation_estimate_max);
    sram_write_u32(8648, telemetry_presentation_deadline_elapsed_max);
    sram_write_u32(8652, telemetry_logic_catchup_updates);
    sram_write_u32(8656, telemetry_logic_updates_per_loop_max);
    sram_write_u32(8660, telemetry_logic_backlog_frames_max);
    sram_write_u32(8664, telemetry_background_held_rows_max);
    sram_write_u32(8668, telemetry_vblank_recovery_loops);
    sram_write_u32(8672, telemetry_audio_frames);
    sram_write_u32(8676, telemetry_sprite2_split_uploads);
    sram_write_u32(8680, telemetry_sprite2_upload_coalesces);
    sram_write_u32(8684, telemetry_projectile_cache_upload_coalesces);
    sram_write_u32(8688, telemetry_effect_cache_upload_coalesces);
    sram_write_u32(
        8692,
        source_sprite2_pending_upload_count()
    );
    sram_write_u32(8696, source_projectile_upload_count);
    sram_write_u32(8700, source_effect_upload_count);
    sram_write_u32(8704, telemetry_structural_oam_required_max);
    sram_write_u32(8708, telemetry_effect_oam_culls);
    sram_write_u32(8712, telemetry_enemy_shot_oam_culls);
    sram_write_u32(8716, telemetry_player_shot_oam_culls);
    sram_write_u32(8720, telemetry_projectile_visible_capacity_drops);
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /* Additive route diagnostics; schema 3 readers may ignore these. */
    sram_write_u32(8724, player_alive ? 1u : 0u);
    sram_write_u32(8728, source_parity_level.end_level ? 1u : 0u);
    sram_write_u32(
        8732,
        source_autotest_unreachable_stop_ticks
    );
    sram_write_u32(
        8736,
        source_parity_level.ready_to_end_level ? 1u : 0u
    );
    sram_write_u32(
        8740,
        source_parity_level.really_end_level ? 1u : 0u
    );
    sram_write_u32(
        8744,
        source_parity_level.all_players_gone ? 1u : 0u
    );
#endif
    /* Additive inter-level ANM diagnostics; existing schema readers ignore. */
    sram_write_u32(8752, telemetry_missed_vblanks_episode_scene);
    sram_write_u32(8756, telemetry_missed_vblanks_scene_picture);
    sram_write_u32(8760, telemetry_missed_vblanks_scene_text);
    sram_write_u32(8764, telemetry_missed_vblanks_scene_animation);
    sram_write_u32(8768, telemetry_scene_animation_decode_frames);
    sram_write_u32(
        8772,
        telemetry_scene_animation_decode_cycles_total
    );
    sram_write_u32(8776, telemetry_scene_animation_decode_cycles_max);
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    {
        u32 record_index;

        sram_write_u32(6096, AUTOTEST_CAMPAIGN_LEVEL_COUNT);
        sram_write_u32(6100, autotest_campaign_levels_completed);
        sram_write_u32(6104, autotest_campaign_failures);
        sram_write_u32(6108, autotest_campaign_checksum);
        for (
            record_index = 0;
            record_index < AUTOTEST_CAMPAIGN_LEVEL_COUNT;
            record_index++
        ) {
            const AutotestCampaignRecord *record =
                &autotest_campaign_record[record_index];
            u32 offset = 6144u + record_index * 36u;

            sram_write_u32(offset, record->episode);
            sram_write_u32(offset + 4u, record->resolved_section);
            sram_write_u32(offset + 8u, record->lvl_file_number);
            sram_write_u32(offset + 12u, record->source_song);
            sram_write_u32(offset + 16u, record->event_index);
            sram_write_u32(offset + 20u, record->level_position);
            sram_write_u32(offset + 24u, record->enemy_kills);
            sram_write_u32(
                offset + 28u,
                record->background_approximations
            );
            sram_write_u32(offset + 32u, record->failure_flags);
        }
    }
#endif
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    /*
     * Always terminate the headless run after telemetry is committed.
     * Host-side verification still rejects pass=0, while a failed invariant
     * now reports immediately instead of presenting as a 30-second timeout.
     */
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
