/* Deterministic mGBA regression harness, excluded from release builds. */

#ifdef AUTOTEST
enum {
    SOURCE_STAGE2_EXPECTED_EVENTS = 878,
    SOURCE_STAGE2_EXPECTED_APPLIED = 869,
    SOURCE_STAGE2_EXPECTED_DEFERRED = 5,
    SOURCE_STAGE2_EXPECTED_SKIPPED = 4,
    SOURCE_STAGE2_EXPECTED_SPAWN_ATTEMPTS = 473,
    SOURCE_STAGE2_EXPECTED_SPAWN_SUCCESSES = 100,
    SOURCE_STAGE2_EXPECTED_POOL_FULL = 373,
    SOURCE_STAGE2_EXPECTED_CONTROL_WRITES = 3586,
    SOURCE_STAGE2_EXPECTED_RNG_CALLS = 30,
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
        game_state == STATE_TITLE &&
        !game_paused &&
        level_position >= LEVEL_BOSS_TICK &&
        telemetry_spawn_count == 414 &&
        telemetry_control_count == 380 &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_effect_drops == 0 &&
        telemetry_reward_spawns > 0 &&
        telemetry_reward_pickups > 0 &&
        telemetry_reward_drops == 0 &&
        telemetry_enemy_shots_spawned > 0 &&
        telemetry_enemy_shot_drops == 0 &&
        telemetry_enemy_replacements == 0 &&
        telemetry_max_enemy_shots <= MAX_ENEMY_SHOTS &&
        telemetry_kill_cash > 0 &&
        player_cash >= telemetry_kill_cash &&
        telemetry_reward_controls == 33 &&
        telemetry_reward_assignments > 0 &&
        telemetry_pause_toggles == 2 &&
        telemetry_paused_frames >= 60 &&
        source_parity_level.assets_valid &&
        telemetry_source_events == SOURCE_STAGE2_EXPECTED_EVENTS &&
        telemetry_source_events_applied ==
            SOURCE_STAGE2_EXPECTED_APPLIED &&
        telemetry_source_events_deferred ==
            SOURCE_STAGE2_EXPECTED_DEFERRED &&
        telemetry_source_events_skipped ==
            SOURCE_STAGE2_EXPECTED_SKIPPED &&
        telemetry_source_events_applied +
            telemetry_source_events_deferred +
            telemetry_source_events_skipped == telemetry_source_events &&
        telemetry_source_spawn_attempts ==
            SOURCE_STAGE2_EXPECTED_SPAWN_ATTEMPTS &&
        telemetry_source_spawn_successes +
            telemetry_source_spawn_pool_full +
            telemetry_source_spawn_missing ==
            telemetry_source_spawn_attempts &&
        telemetry_source_spawn_successes ==
            SOURCE_STAGE2_EXPECTED_SPAWN_SUCCESSES &&
        telemetry_source_spawn_pool_full ==
            SOURCE_STAGE2_EXPECTED_POOL_FULL &&
        telemetry_source_spawn_missing == 0 &&
        telemetry_source_max_enemies == OT_ENEMY_COUNT &&
        telemetry_source_control_writes ==
            SOURCE_STAGE2_EXPECTED_CONTROL_WRITES &&
        telemetry_source_rng_calls == SOURCE_STAGE2_EXPECTED_RNG_CALLS &&
        bg1_scroll_speed == 2 &&
        bg2_scroll_speed == 4 &&
        bg3_scroll_speed == 6
    );
    volatile u32 delay;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'B';
    sram[3] = 'A';
    sram[4] = 8;
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
    sram_write_u32(52, event_offset);
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
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    if (pass) {
        __asm__ volatile("swi 3");
    }
    for (;;) {
        VBlankIntrWait();
    }
}

static u16 autotest_input(void)
{
    u8 reward_index;
    u16 phase = (level_tick >> 5) & 3;
    u16 keys = KEY_A;
#ifdef AUTOTEST_STATIONARY_PLAYER
    /*
     * Screenshot regression mode: hold a neutral pose so two frames from
     * different level ticks can be compared without position compensation.
     */
    return 0;
#endif
    if (game_state == STATE_BOSS) {
        if (player_x + 16 < boss_x + 32) keys |= KEY_RIGHT;
        if (player_x + 16 > boss_x + 32) keys |= KEY_LEFT;
        return keys;
    }
    /*
     * Follow the first authored score item long enough to exercise the
     * physical pickup path. The release build has no attraction or assist.
     */
    for (reward_index = 0; reward_index < MAX_REWARDS; reward_index++) {
        if (!rewards[reward_index].active) continue;
        if (player_x + 16 < rewards[reward_index].x + 8) keys |= KEY_RIGHT;
        if (player_x + 16 > rewards[reward_index].x + 8) keys |= KEY_LEFT;
        if (player_y + 14 < rewards[reward_index].y + 8) keys |= KEY_DOWN;
        if (player_y + 14 > rewards[reward_index].y + 8) keys |= KEY_UP;
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
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) return 1;
    }
    return 0;
}
#endif
#endif
