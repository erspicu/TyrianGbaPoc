/* Deterministic mGBA regression harness, excluded from release builds. */

#ifdef AUTOTEST
enum {
    /*
     * Deterministic first-level run through the authored boss group and
     * OpenTyrian end-level flight.  These replace the old position-5400
     * POC handoff values.
     */
    SOURCE_STAGE4_EXPECTED_EVENTS = 935,
    SOURCE_STAGE4_EXPECTED_APPLIED = 926,
    SOURCE_STAGE4_EXPECTED_DEFERRED = 5,
    SOURCE_STAGE4_EXPECTED_SKIPPED = 4,
    SOURCE_STAGE4_EXPECTED_SPAWN_ATTEMPTS = 473,
    SOURCE_STAGE4_EXPECTED_SPAWN_SUCCESSES = 473,
    SOURCE_STAGE4_EXPECTED_POOL_FULL = 0,
    SOURCE_STAGE4_EXPECTED_CONTROL_WRITES = 7199,
    SOURCE_STAGE4_EXPECTED_RNG_CALLS = 2471,
    SOURCE_STAGE4_EXPECTED_MOTION_UPDATES = 78100,
    SOURCE_STAGE4_EXPECTED_RELEASES = 483,
    SOURCE_STAGE4_EXPECTED_SHOT_TRIGGERS = 428,
    SOURCE_STAGE4_EXPECTED_SHOT_SPAWNS = 428,
    SOURCE_STAGE4_EXPECTED_SHOT_MAX_ACTIVE = 32,
    SOURCE_STAGE4_EXPECTED_SHOT_UPDATES = 17466,
    SOURCE_STAGE4_EXPECTED_SHOT_RELEASES = 426,
    SOURCE_STAGE4_EXPECTED_SHOT_PLAYER_HITS = 33,
    SOURCE_STAGE4_EXPECTED_PLAYER_SHOT_HITS = 585,
    SOURCE_STAGE4_EXPECTED_PLAYER_CONTACTS = 255,
    SOURCE_STAGE4_EXPECTED_ENEMY_KILLS = 100,
    SOURCE_STAGE4_EXPECTED_DIRECT_CASH = 1406,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_SPAWNS = 3,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS = 2,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_MAX_ACTIVE = 3,
    SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS = 3,
    SOURCE_STAGE4_EXPECTED_DEATH_CONTROLS = 32,
    SOURCE_STAGE4_EXPECTED_DEATH_ASSIGNMENTS = 60,
    SOURCE_STAGE4_EXPECTED_VISIBLE_ENEMIES = 30,
    SOURCE_STAGE4_EXPECTED_UNKNOWN_VISUALS = 0,
};

static void sram_write_u32(u32 offset, u32 value)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    sram[offset] = (u8)value;
    sram[offset + 1] = (u8)(value >> 8);
    sram[offset + 2] = (u8)(value >> 16);
    sram[offset + 3] = (u8)(value >> 24);
}

static void autotest_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 pass = (
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_level_completed &&
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !game_paused &&
        level_position >= LEVEL_BOSS_TICK &&
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
        source_parity_level.data_cube_pickup_count == 1 &&
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
        telemetry_sprite2_cache_drops == 0 &&
        telemetry_sprite2_uploads ==
            telemetry_sprite2_cache_misses &&
        telemetry_sprite2_upload_bytes ==
            telemetry_sprite2_uploads * SOURCE_ENEMY_FRAME_BYTES &&
        telemetry_sprite2_max_uploads <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT &&
        telemetry_sprite2_max_visible_unique <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT &&
        telemetry_effect_cache_drops == 0 &&
        telemetry_effect_cache_uploads ==
            telemetry_effect_cache_misses &&
        telemetry_effect_cache_upload_bytes ==
            telemetry_effect_cache_uploads *
                SOURCE_EFFECT_FRAME_BYTES &&
        telemetry_effect_cache_max_uploads <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_effect_cache_max_visible_unique <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_pickup_explosion_spawns ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS &&
        telemetry_pickup_explosion_drops == 0 &&
        telemetry_pickup_explosion_max_active <=
            MAX_PICKUP_EXPLOSIONS &&
        source_parity_level.enemy_shot_active_count == 2 &&
        source_parity_level.active_enemy_count == 0 &&
        player_cash == 1456 &&
        telemetry_missed_vblanks <= 640 &&
        telemetry_source_launch_attempts == 7 &&
        telemetry_source_launch_successes == 7 &&
        telemetry_source_random_attempts == 0 &&
        telemetry_source_random_successes == 0 &&
        source_world_to_screen_x(SOURCE_PRESENTATION_X_ORIGIN) == 0 &&
        source_world_to_screen_x(
            SOURCE_PRESENTATION_X_ORIGIN + SCREEN_WIDTH - 1
        ) == SCREEN_WIDTH - 1 &&
        source_world_to_screen_y(SOURCE_PRESENTATION_Y_ORIGIN) == 0 &&
        source_world_to_screen_y(
            SOURCE_PRESENTATION_Y_ORIGIN + SCREEN_HEIGHT - 1
        ) == SCREEN_HEIGHT - 1 &&
        source_background_hofs(0) < 512 &&
        source_background_hofs(1) < 512 &&
        source_background_hofs(2) < 512 &&
        bg1_scroll_speed == 1 &&
        bg2_scroll_speed == 2 &&
        bg3_scroll_speed == 0 &&
        telemetry_layer_rule_checks == 252 &&
        telemetry_layer_rule_failures == 0
    );
    volatile u32 delay;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'B';
    sram[3] = 'A';
    sram[4] = 21;
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
    sram_write_u32(512, SOURCE_PRESENTATION_X_ORIGIN);
    sram_write_u32(516, SOURCE_PRESENTATION_Y_ORIGIN);
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

static u16 autotest_input(void)
{
    u8 reward_index;
    u8 source_index;
    u16 phase = (level_tick >> 5) & 3;
    u16 keys = KEY_A;
    s16 player_screen_x = source_player_screen_x();
    s16 player_screen_y = source_player_screen_y();
#ifdef AUTOTEST_FORCE_PLAYER_TOP
    /*
     * Visual boundary regression mode: hold UP until the PC-space clamp
     * reaches the GBA crop-safe upper limit.
     */
    return KEY_UP;
#endif
#ifdef AUTOTEST_STATIONARY_PLAYER
    /*
     * Screenshot regression mode: hold a neutral pose so two frames from
     * different level ticks can be compared without position compensation.
     */
    return 0;
#endif
    if (source_parity_level.boss_bar_link[0] != 0) {
        for (
            source_index = 0;
            source_index < OT_ENEMY_COUNT;
            source_index++
        ) {
            const OtEnemy *enemy =
                &source_parity_level.enemy[source_index];

            if (
                source_parity_level.enemy_avail[source_index] == 1 ||
                enemy->linknum !=
                    source_parity_level.boss_bar_link[0]
            ) {
                continue;
            }
            if (player_source_x < enemy->ex - 2) keys |= KEY_RIGHT;
            if (player_source_x > enemy->ex + 2) keys |= KEY_LEFT;
            if (player_source_y < 140) keys |= KEY_DOWN;
            return keys;
        }
    }
    /*
     * Event-33 data cubes are physical score-item enemies (evalue==1), not
     * destruction awards.  Prioritize them over cash objects so the exact
     * JE_playerCollide() cube branch remains covered with PC inertia.
     */
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 target_x;
        s16 target_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem ||
            enemy->evalue != 1
        ) {
            continue;
        }
        target_x = (s16)(enemy->ex + enemy->mapoffset);
        target_y = enemy->ey;
        if (player_source_x < target_x - 2) keys |= KEY_RIGHT;
        if (player_source_x > target_x + 2) keys |= KEY_LEFT;
        if (player_source_y < target_y - 2) keys |= KEY_DOWN;
        if (player_source_y > target_y + 2) keys |= KEY_UP;
        return keys;
    }
    /*
     * Follow the first authored score item long enough to exercise the
     * physical pickup path. The release build has no attraction or assist.
     */
    for (reward_index = 0; reward_index < MAX_REWARDS; reward_index++) {
        if (!rewards[reward_index].active) continue;
        if (player_screen_x + 16 < rewards[reward_index].x + 8) {
            keys |= KEY_RIGHT;
        }
        if (player_screen_x + 16 > rewards[reward_index].x + 8) {
            keys |= KEY_LEFT;
        }
        if (player_screen_y + 14 < rewards[reward_index].y + 8) {
            keys |= KEY_DOWN;
        }
        if (player_screen_y + 14 > rewards[reward_index].y + 8) {
            keys |= KEY_UP;
        }
        return keys;
    }
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 reward_x;
        s16 reward_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem
        ) {
            continue;
        }
        reward_x = source_score_item_screen_x(enemy);
        reward_y = source_score_item_screen_y(enemy);
        if (player_screen_x + 16 < reward_x + 8) keys |= KEY_RIGHT;
        if (player_screen_x + 16 > reward_x + 8) keys |= KEY_LEFT;
        if (player_screen_y + 14 < reward_y + 8) keys |= KEY_DOWN;
        if (player_screen_y + 14 > reward_y + 8) keys |= KEY_UP;
        return keys;
    }
    if (phase == 0) keys |= KEY_RIGHT;
    if (phase == 1) keys |= KEY_UP;
    if (phase == 2) keys |= KEY_LEFT;
    if (phase == 3) keys |= KEY_DOWN;
    return keys;
}

#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
static u8 autotest_explosion_visible(void)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) continue;
#ifdef AUTOTEST_SCREENSHOT_EXPLOSION_FRAME
        if (effects[index].frame == AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) {
            return 1;
        }
#else
        return 1;
#endif
    }
    return 0;
}
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
static u8 autotest_reward_visible(void)
{
    u8 index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (
            source_parity_level.enemy_draw[index].active &&
            source_parity_level.enemy[index].scoreitem
        ) {
            return 1;
        }
    }
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) return 1;
    }
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        if (pickup_explosions[index].active) return 1;
    }
    return 0;
}
#endif
#endif
