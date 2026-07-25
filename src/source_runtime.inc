/*
 * GBA presentation/input adapter for the directly translated OpenTyrian
 * first-level runtime.  Source gameplay stays in its original 320x200
 * coordinate system; only these helpers map it to the 240x160 display.
 */

#define SOURCE_VIEW_X_OFFSET 34
#define SOURCE_VIEW_Y_OFFSET (-10)

static u8 source_unknown_visual_bits[(OT_HDT_ENEMY_COUNT + 7) / 8]
    EWRAM_DATA;

static s16 source_scale_4_5(s16 value)
{
    if (value < 0) {
        return (s16)-(((-value) * 4 + 2) / 5);
    }
    return (s16)((value * 4 + 2) / 5);
}

static s16 source_unscale_4_5(s16 value)
{
    if (value < 0) {
        return (s16)-(((-value) * 5 + 2) / 4);
    }
    return (s16)((value * 5 + 2) / 4);
}

static s16 source_world_to_screen_x(s16 source_x)
{
    return (s16)(
        source_scale_4_5(source_x) + SOURCE_VIEW_X_OFFSET
    );
}

static s16 source_world_to_screen_y(s16 source_y)
{
    return (s16)(
        source_scale_4_5(source_y) + SOURCE_VIEW_Y_OFFSET
    );
}

static s16 source_player_x_from_gba(void)
{
    return (s16)(
        source_unscale_4_5(
            (s16)(player_x + 16 - SOURCE_VIEW_X_OFFSET)
        ) - 8
    );
}

static s16 source_player_y_from_gba(void)
{
    return (s16)(
        source_unscale_4_5(
            (s16)(player_y + 16 - SOURCE_VIEW_Y_OFFSET)
        ) - 8
    );
}

static s16 source_enemy_screen_x(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_x((s16)(enemy->ex + 6)) - 16
    );
}

static s16 source_enemy_screen_y(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_y((s16)(enemy->ey + 7)) - 16
    );
}

static s16 source_score_item_screen_x(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_x((s16)(enemy->ex + 6)) - 8
    );
}

static s16 source_score_item_screen_y(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_y((s16)(enemy->ey + 7)) - 8
    );
}

static s16 source_player_shot_screen_x(const PlayerShot *shot)
{
    return (s16)(source_world_to_screen_x(shot->x) - 2);
}

static s16 source_player_shot_screen_y(const PlayerShot *shot)
{
    return (s16)(source_world_to_screen_y(shot->y) - 4);
}

static s16 source_enemy_shot_screen_x(const OtEnemyShot *shot)
{
    return source_world_to_screen_x(shot->sx);
}

static s16 source_enemy_shot_screen_y(const OtEnemyShot *shot)
{
    return source_world_to_screen_y(shot->sy);
}

static u8 source_reward_sequence(const OtEnemy *enemy)
{
    switch (enemy->enemy_definition_id) {
    case 391: return 0;
    case 392: return 1;
    case 393: return 2;
    case 394: return 3;
    case 395: return 4;
    default:
        /*
         * GBA_PORT: the first-stage atlas does not yet include data cubes,
         * weapon power-ups or bonus portals.  Keep their source lifecycle
         * and use the closest existing pickup silhouette temporarily.
         */
        return enemy->evalue < 0 ? 3 : 4;
    }
}

static u8 source_reward_frame(const OtEnemy *enemy)
{
    u8 cycle = enemy->enemycycle > 0 ?
        (u8)(enemy->enemycycle - 1) : 0;

    if (
        enemy->enemy_definition_id == 394 ||
        enemy->enemy_definition_id == 395
    ) {
        u8 phase = cycle >> 1;

        if (phase > 4) phase = (u8)(8 - phase);
        if (phase >= 4) return 2;
        if (phase >= 2) return 1;
        return 0;
    }
    return (u8)((cycle >> 1) % REWARD_FRAME_COUNT);
}

static u8 source_enemy_visual_archetype(
    u16 enemy_id,
    u8 *known
)
{
    *known = 1;
    if (enemy_id >= 66 && enemy_id <= 79) return 21;
    if (enemy_id >= 121 && enemy_id <= 128) return 20;
    if (enemy_id >= 516 && enemy_id <= 527) return 22;
    switch (enemy_id) {
    case 4: return 0;
    case 3: return 1;
    case 5:
    case 38: return 2;
    case 10:
    case 12:
    case 39: return 3;
    case 15: return 4;
    case 6:
    case 13: return 5;
    case 7:
    case 14: return 6;
    case 8: return 7;
    case 9: return 8;
    case 17: return 9;
    case 24: return 10;
    case 25: return 11;
    case 26:
    case 28: return 12;
    case 27:
    case 29: return 13;
    case 30: return 14;
    case 31: return 15;
    case 32:
    case 33: return 16;
    case 34:
    case 35: return 17;
    case 36:
    case 37: return 18;
    case 40: return 19;
    case 41:
    case 42:
    case 43:
    case 44:
    case 45: return 23;
    default:
        *known = 0;
        return 0;
    }
}

static void source_note_unknown_visual(u16 enemy_id)
{
    u16 byte_index;
    u8 mask;

    if (enemy_id >= OT_HDT_ENEMY_COUNT) return;
    byte_index = enemy_id >> 3;
    mask = (u8)(1u << (enemy_id & 7));
    if ((source_unknown_visual_bits[byte_index] & mask) == 0) {
        source_unknown_visual_bits[byte_index] |= mask;
        telemetry_source_unknown_visuals++;
    }
}

static void source_emit_effect(const OtHitEffect *effect)
{
    s16 centre_x = source_world_to_screen_x((s16)(effect->x + 6));
    s16 centre_y = source_world_to_screen_y((s16)(effect->y + 7));

    if (effect->large) {
        spawn_large_effect(centre_x, centre_y, effect->ground);
    } else {
        spawn_effect(
            (s16)(centre_x - 8),
            (s16)(centre_y - 8),
            EXPLOSION_SEQUENCE_SMALL
        );
    }
}

static void source_play_frame_weapon_sounds(void)
{
    u16 sounds = source_parity_level.frame_sound_mask;

    if (sounds & (1u << 13)) {
        audio_effect(SFX_ENEMY_SHOT_13);
    } else if (sounds & (1u << 6)) {
        audio_effect(SFX_ENEMY_SHOT_6);
    } else if (sounds & (1u << 4)) {
        audio_effect(SFX_ENEMY_SHOT_4);
    } else if (sounds & (1u << 1)) {
        audio_effect(SFX_WEAPON_1);
    }
}

static void source_sync_runtime_telemetry(void)
{
    telemetry_source_events = source_parity_level.event_index;
    telemetry_source_events_applied =
        source_parity_level.applied_event_count;
    telemetry_source_events_deferred =
        source_parity_level.deferred_event_count;
    telemetry_source_events_skipped =
        source_parity_level.skipped_event_count;
    telemetry_source_spawn_attempts =
        source_parity_level.spawn_attempt_count;
    telemetry_source_spawn_successes =
        source_parity_level.spawn_success_count;
    telemetry_source_spawn_pool_full =
        source_parity_level.spawn_pool_full_count;
    telemetry_source_spawn_missing =
        source_parity_level.spawn_missing_definition_count;
    telemetry_source_max_enemies =
        source_parity_level.max_active_enemy_count;
    telemetry_source_control_writes =
        source_parity_level.enemy_control_write_count;
    telemetry_source_rng_calls =
        source_parity_level.rng_call_count;
    telemetry_source_motion_updates =
        source_parity_level.enemy_motion_update_count;
    telemetry_source_releases =
        source_parity_level.enemy_release_count;
    telemetry_source_shot_triggers =
        source_parity_level.enemy_shot_trigger_count;
    telemetry_source_enemy_shots_spawned =
        source_parity_level.enemy_shot_spawn_count;
    telemetry_source_enemy_shot_drops =
        source_parity_level.enemy_shot_drop_count;
    telemetry_source_max_enemy_shots =
        source_parity_level.enemy_shot_max_active_count;
    telemetry_source_enemy_shot_updates =
        source_parity_level.enemy_shot_motion_update_count;
    telemetry_source_enemy_shot_releases =
        source_parity_level.enemy_shot_release_count;
    telemetry_source_enemy_shot_player_hits =
        source_parity_level.enemy_shot_player_hit_count;
    telemetry_source_player_shot_hits =
        source_parity_level.player_shot_collision_count;
    telemetry_source_enemy_kills =
        source_parity_level.enemy_kill_count;
    telemetry_source_direct_cash =
        source_parity_level.direct_cash_awarded;
    telemetry_source_score_item_spawns =
        source_parity_level.score_item_spawn_count;
    telemetry_source_score_item_pickups =
        source_parity_level.score_item_pickup_count;
    telemetry_source_score_item_max_active =
        source_parity_level.score_item_max_active_count;
    telemetry_source_death_spawn_attempts =
        source_parity_level.death_spawn_attempt_count;
    telemetry_source_death_spawn_successes =
        source_parity_level.death_spawn_success_count;
    telemetry_source_death_control_events =
        source_parity_level.death_control_event_count;
    telemetry_source_death_assignments =
        source_parity_level.death_assignment_count;
    telemetry_source_launch_attempts =
        source_parity_level.enemy_launch_attempt_count;
    telemetry_source_launch_successes =
        source_parity_level.enemy_launch_success_count;
    telemetry_source_random_attempts =
        source_parity_level.random_spawn_attempt_count;
    telemetry_source_random_successes =
        source_parity_level.random_spawn_success_count;

    /*
     * Stage 4 makes these formerly legacy-facing counters describe the
     * authoritative source runtime.  The separate source counters above
     * remain available for exact regression auditing.
     */
    telemetry_spawn_count =
        source_parity_level.spawn_success_count +
        source_parity_level.death_spawn_success_count;
    telemetry_control_count =
        source_parity_level.enemy_control_write_count;
    telemetry_max_enemies =
        source_parity_level.max_active_enemy_count;
    telemetry_enemy_shots_spawned =
        source_parity_level.enemy_shot_spawn_count;
    telemetry_enemy_shot_drops =
        source_parity_level.enemy_shot_drop_count;
    telemetry_max_enemy_shots =
        source_parity_level.enemy_shot_max_active_count;
    telemetry_kill_cash =
        source_parity_level.direct_cash_awarded;
    telemetry_reward_spawns =
        source_parity_level.score_item_spawn_count;
    telemetry_reward_pickups =
        source_parity_level.score_item_pickup_count;
    telemetry_max_rewards =
        source_parity_level.score_item_max_active_count;
    telemetry_reward_controls =
        source_parity_level.death_control_event_count;
    telemetry_reward_assignments =
        source_parity_level.death_assignment_count;
}

static void source_apply_background_state(void)
{
    bg1_scroll_speed = (u8)source_parity_level.back_move;
    bg2_scroll_speed = (u8)source_parity_level.back_move2;
    bg3_scroll_speed = (u8)source_parity_level.back_move3;
    bg1_scroll_delay_max =
        source_parity_level.map1_y_delay_max ?
        source_parity_level.map1_y_delay_max : 1;
    bg2_scroll_delay_max =
        source_parity_level.map2_y_delay_max ?
        source_parity_level.map2_y_delay_max : 1;
    if (bg1_scroll_delay > bg1_scroll_delay_max) {
        bg1_scroll_delay = bg1_scroll_delay_max;
    }
    if (bg2_scroll_delay > bg2_scroll_delay_max) {
        bg2_scroll_delay = bg2_scroll_delay_max;
    }
    foreground_phase = source_parity_level.background3_over != 0;
}

static void source_process_player_collisions(void)
{
    u8 shot_index;
    u8 effect_index;
    OtPlayerCollisionResult player_result;

    for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
        PlayerShot *shot = &player_shots[shot_index];
        OtShotCollisionResult result;

        if (!shot->active) continue;
        ot_level_port_collide_player_shot(
            &source_parity_level,
            shot->x,
            shot->y,
            shot->damage,
            &result
        );
        if (!result.collided) continue;
        shot->damage = result.remaining_damage;
        if (result.consumed) shot->active = 0;
        player_cash += result.cash_awarded;
        for (
            effect_index = 0;
            effect_index < result.effect_count;
            effect_index++
        ) {
            source_emit_effect(&result.effects[effect_index]);
        }
        if (result.kill_count) {
            audio_effect(SFX_EXPLOSION_9);
        } else {
            audio_effect(SFX_ENEMY_HIT);
        }
    }

    ot_level_port_collide_player(
        &source_parity_level,
        player_invulnerable == 0,
        &player_result
    );
    player_cash += player_result.cash_awarded;
    for (
        effect_index = 0;
        effect_index < player_result.effect_count;
        effect_index++
    ) {
        source_emit_effect(&player_result.effects[effect_index]);
    }
    if (player_result.pickup_count) audio_effect(SFX_ITEM);
    if (player_result.contact_count) player_hit();
}

static void source_update_enemy_projectiles(void)
{
    u32 old_enemy_shot_hits;

    /*
     * JE_mainGamePlayerFunctions() runs between player/enemy collision and
     * enemy-shot movement.  Reflect the just-applied GBA input at the same
     * phase boundary without changing source projectile math.
     */
    source_parity_level.player_x = source_player_x_from_gba();
    source_parity_level.player_y = source_player_y_from_gba();
    old_enemy_shot_hits =
        source_parity_level.enemy_shot_player_hit_count;
    ot_level_port_update_enemy_shots(&source_parity_level);
    if (
        source_parity_level.enemy_shot_player_hit_count >
        old_enemy_shot_hits
    ) {
        player_hit();
    }
    telemetry_collision_count =
        source_parity_level.player_shot_collision_count +
        source_parity_level.enemy_shot_player_hit_count +
        source_parity_level.player_enemy_contact_count;
    source_sync_runtime_telemetry();
}

static void source_runtime_reset(void)
{
    memset(
        source_unknown_visual_bits,
        0,
        sizeof(source_unknown_visual_bits)
    );
    telemetry_source_visible_enemies = 0;
    telemetry_source_max_visible_enemies = 0;
    telemetry_source_unknown_visuals = 0;
}
