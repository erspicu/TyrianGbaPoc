/*
 * Source-parity gameplay runtime orchestration.
 * Included by source_runtime.inc after the shared graphics caches.
 */

static s16 source_camera_target_offset(
    s16 player,
    s16 centre,
    s16 dead_zone_half,
    s16 slack
)
{
    s16 relative = (s16)(player - centre);
    s16 target = 0;

    if (relative > dead_zone_half) {
        target = (s16)(relative - dead_zone_half);
    } else if (relative < -dead_zone_half) {
        target = (s16)(relative + dead_zone_half);
    }
    if (target < -slack) target = (s16)-slack;
    if (target > slack) target = slack;
    return target;
}

static s16 source_camera_smooth_axis(
    s16 current_q8,
    s16 target
)
{
    s16 target_q8 = (s16)(target * 256);
    s16 delta = (s16)(target_q8 - current_q8);
    s16 step;

    if (delta > 0) {
        step = (s16)(delta >> TYRIAN_GBA_CAMERA_RESPONSE_SHIFT);
        if (step == 0) step = 1;
    } else if (delta < 0) {
        step = (s16)(
            -(
                (s16)(-delta) >>
                TYRIAN_GBA_CAMERA_RESPONSE_SHIFT
            )
        );
        if (step == 0) step = -1;
    } else {
        return current_q8;
    }
    current_q8 = (s16)(current_q8 + step);
    if (
        (delta > 0 && current_q8 > target_q8) ||
        (delta < 0 && current_q8 < target_q8)
    ) {
        current_q8 = target_q8;
    }
    return current_q8;
}

static s8 source_camera_q8_to_pixel(s16 value)
{
    if (value >= 0) {
        return (s8)((value + 128) >> 8);
    }
    return (s8)(-(((-value) + 128) >> 8));
}

static void source_camera_reset(void)
{
    source_camera_offset_x_q8 = 0;
    source_camera_offset_y_q8 = 0;
    source_camera_offset_x = 0;
    source_camera_offset_y = 0;
}

static void source_camera_update(void)
{
#if TYRIAN_GBA_SOFT_CROP_CAMERA
    s16 target_x = source_camera_target_offset(
        player_source_x,
        TYRIAN_GBA_CAMERA_SOURCE_CENTER_X,
        TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_X,
        SOURCE_VIEW_CROP_X
    );
    s16 target_y = source_camera_target_offset(
        player_source_y,
        TYRIAN_GBA_CAMERA_SOURCE_CENTER_Y,
        TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_Y,
        SOURCE_VIEW_CROP_Y
    );

    s16 next_x_q8 = source_camera_smooth_axis(
        source_camera_offset_x_q8,
        target_x
    );
    s16 next_y_q8 = source_camera_smooth_axis(
        source_camera_offset_y_q8,
        target_y
    );
    s8 next_y = source_camera_q8_to_pixel(next_y_q8);

    source_camera_offset_x_q8 = next_x_q8;
    source_camera_offset_x = source_camera_q8_to_pixel(next_x_q8);
    /*
     * Camera state is authoritative logic.  The presentation pass later
     * materializes the final complete background window and publishes VOFS
     * atomically, so skipped frames no longer build intermediate camera rows.
     */
    source_camera_offset_y_q8 = next_y_q8;
    source_camera_offset_y = next_y;
    if (
        (u32)(u16)source_camera_origin_x() <
            telemetry_camera_min_origin_x
    ) {
        telemetry_camera_min_origin_x =
            (u32)(u16)source_camera_origin_x();
    }
    if (
        (u32)(u16)source_camera_origin_x() >
            telemetry_camera_max_origin_x
    ) {
        telemetry_camera_max_origin_x =
            (u32)(u16)source_camera_origin_x();
    }
    if (
        (u32)(u16)source_camera_origin_y() <
            telemetry_camera_min_origin_y
    ) {
        telemetry_camera_min_origin_y =
            (u32)(u16)source_camera_origin_y();
    }
    if (
        (u32)(u16)source_camera_origin_y() >
            telemetry_camera_max_origin_y
    ) {
        telemetry_camera_max_origin_y =
            (u32)(u16)source_camera_origin_y();
    }
#else
    source_camera_reset();
#endif
}

static s16 source_camera_origin_x(void)
{
    return (s16)(
        SOURCE_PRESENTATION_X_ORIGIN + source_camera_offset_x
    );
}

static s16 source_camera_origin_y(void)
{
    return (s16)(
        SOURCE_PRESENTATION_Y_ORIGIN + source_camera_offset_y
    );
}

static s16 source_world_to_screen_x(s16 source_x)
{
    return (s16)(source_x - source_camera_origin_x());
}

static s16 source_world_to_screen_y(s16 source_y)
{
    return (s16)(source_y - source_camera_origin_y());
}

static u16 source_background_hofs(u8 layer)
{
    const OtLevelPortState *state = &source_parity_level;

    /*
     * The initial PC frame deliberately uses mapXbpPos=0.  Later frames use
     * the pointer/position pair calculated by JE_mainGamePlayerFunctions().
     * Since the GBA map contains the complete unscaled PC raster, HOFS is the
     * exact source-map pixel at game_screen x=36.
     */
    if (!state->presentation_parallax_initialized) {
        return (u16)(
            (layer == 2 ? BG3_INITIAL_HOFS : BG12_INITIAL_HOFS) +
            source_camera_offset_x
        );
    }
    if (layer == 0) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x_offset +
            source_camera_offset_x
        );
    }
    if (layer == 1) {
#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
        /*
         * backgrnd.c::draw_background_2() synchronizes water's BG2 x source
         * to BG1.  The wild blend path deliberately keeps mapX2Pos, so only
         * the authored opaque/event-48 path (and background2over==3) shares
         * BG1's offset.
         */
        if (
            state->smoothies[1] &&
            (
                state->background2_not_transparent ||
                state->background2_over == 3u
            )
        ) {
            return source_background_hofs(0);
        }
#endif
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x2_offset +
            source_camera_offset_x
        );
    }
    if (state->presentation_background3_x1) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x3_offset +
            source_camera_offset_x
        );
    }
    return (u16)(
        SOURCE_BG3_PARALLAX_BASE_X -
        state->presentation_map_x3_offset +
        source_camera_offset_x
    );
}

static u16 source_background_vofs(u8 layer)
{
    u16 scroll = background_presentation_scroll_pixel(
        layer,
        source_camera_offset_y
    );

    if (source_detail_vertical_flip) {
        /*
         * physical_y = -source_y-1 and the 160-line GBA crop is reversed:
         * VOFS = -(scroll + SCREEN_HEIGHT), modulo the 256-line text BG.
         */
        return (u16)((0u - scroll - SCREEN_HEIGHT) & 255u);
    }
    return (u16)(scroll & 255u);
}

static s16 source_player_screen_x(void)
{
    return (s16)(
        source_world_to_screen_x((s16)(player_source_x + 7)) - 16
    );
}

static s16 source_player_screen_y(void)
{
    return (s16)(
        source_world_to_screen_y(
            (s16)(
                player_source_y +
                SOURCE_PLAYER_PRESENTATION_CENTRE_Y
            )
        ) - 16
    );
}

static s16 source_enemy_draw_screen_x(
    const OtEnemyDrawCommand *draw
)
{
    return (s16)(
        source_world_to_screen_x((s16)(draw->x + 6)) - 16
    );
}

static s16 source_enemy_draw_screen_y(
    const OtEnemyDrawCommand *draw
)
{
    return (s16)(
        source_world_to_screen_y((s16)(draw->y + 7)) - 16
    );
}

#ifdef AUTOTEST
/*
 * The deterministic test pilot steers toward the live translated enemy slot,
 * after JE_drawEnemy has advanced it.  Presentation itself never uses these
 * helpers: it consumes the pre-update OtEnemyDrawCommand above.
 */
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
#endif

#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
static s16 source_player_shot_screen_x(const PlayerShot *shot)
{
    return source_world_to_screen_x((s16)(shot->x + 1));
}

static s16 source_player_shot_screen_y(const PlayerShot *shot)
{
    return source_world_to_screen_y(shot->y);
}
#endif

static s16 source_enemy_shot_screen_x(const OtEnemyShot *shot)
{
    return source_world_to_screen_x(shot->sx);
}

static s16 source_enemy_shot_screen_y(const OtEnemyShot *shot)
{
    return source_world_to_screen_y(shot->sy);
}

typedef struct {
    u16 sprite;
    u8 ttl;
} SourceExplosionDefinition;

enum {
    SOURCE_REPEATING_EXPLOSION_COUNT = 20,
};

/* Direct fixed-width counterpart of varz.h rep_explosion_type. */
typedef struct {
    u16 delay;
    u16 ttl;
    s16 x;
    s16 y;
    u8 big;
} SourceRepeatingExplosion;

static SourceRepeatingExplosion source_repeating_explosion[
    SOURCE_REPEATING_EXPLOSION_COUNT
] EWRAM_DATA;

/*
 * Direct JE_setupExplosion() table from OpenTyrian varz.c.  For a fixed
 * pickup label the PC renderer does not increment sprite; blit_sprite2()
 * receives sprite + 1 on every tick.
 */
static const SourceExplosionDefinition source_explosion_definition[53] = {
    { 144,  7 }, { 120, 12 }, { 190, 12 }, { 209, 12 },
    { 152, 12 }, { 171, 12 }, { 133,  7 }, {   1, 12 },
    {  20, 12 }, {  39, 12 }, {  58, 12 }, { 110,  3 },
    {  76,  7 }, {  91,  3 }, { 227,  3 }, { 230,  3 },
    { 233,  3 }, { 252,  3 }, { 246,  3 }, { 249,  3 },
    { 265,  3 }, { 268,  3 }, { 271,  3 }, { 236,  3 },
    { 239,  3 }, { 242,  3 }, { 261,  3 }, { 274,  3 },
    { 277,  3 }, { 280,  3 }, { 299,  3 }, { 284,  3 },
    { 287,  3 }, { 290,  3 }, { 293,  3 }, { 165,  8 },
    { 184,  8 }, { 203,  8 }, { 222,  8 }, { 168,  8 },
    { 187,  8 }, { 206,  8 }, { 225, 10 }, { 169, 10 },
    { 188, 10 }, { 207, 20 }, { 226, 14 }, { 170, 14 },
    { 189, 14 }, { 208, 14 }, { 246, 14 }, { 227, 14 },
    { 265, 14 },
};

static void source_spawn_explosion(
    s16 x,
    s16 y,
    s8 delta_y,
    u8 type,
    u8 fixed_position
)
{
    u8 index;

    if (y <= -16 || y >= 190) return;
    if (type == 6) {
        y = (s16)(y + 12);
        x = (s16)(x + 2);
    } else if (type == 98) {
        type = 6;
    }
    if (
        type >=
        sizeof(source_explosion_definition) /
            sizeof(source_explosion_definition[0])
    ) {
        telemetry_pickup_explosion_drops++;
        return;
    }
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        PickupExplosion *explosion = &pickup_explosions[index];

        if (explosion->active) continue;
        explosion->active = 1;
        explosion->x = x;
        explosion->y = y;
        explosion->graphic =
            source_explosion_definition[type].sprite;
        explosion->ttl = source_explosion_definition[type].ttl;
        explosion->fixed_position = fixed_position;
        explosion->delta_y = delta_y;
        active_pickup_explosion_count++;
        telemetry_pickup_explosion_spawns++;
        if (
            active_pickup_explosion_count >
            telemetry_pickup_explosion_max_active
        ) {
            telemetry_pickup_explosion_max_active =
                active_pickup_explosion_count;
        }
        return;
    }
    telemetry_pickup_explosion_drops++;
}

static void source_spawn_fixed_pickup_explosion(
    const OtPickupEffect *effect
)
{
    source_spawn_explosion(
        effect->x,
        effect->y,
        0,
        effect->explosion_type,
        1
    );
}

static void source_update_explosions(void)
{
    u8 index;

    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        PickupExplosion *explosion = &pickup_explosions[index];

        if (!explosion->active) continue;
        if (!explosion->fixed_position) {
            s16 explode_move =
                source_parity_level.back_move2 > 0 ?
                    source_parity_level.back_move2 :
                    source_parity_level.back_move;

            explosion->graphic++;
            explosion->y = (s16)(explosion->y + explode_move);
        }
        explosion->y = (s16)(explosion->y + explosion->delta_y);
        if (explosion->y > 186) {
            explosion->ttl = 0;
        }
        if (explosion->ttl > 0) explosion->ttl--;
        if (explosion->ttl == 0) {
            explosion->active = 0;
            if (active_pickup_explosion_count) {
                active_pickup_explosion_count--;
            }
        }
    }
}

static void source_spawn_large_explosion_visual(
    bool ground,
    s16 x,
    s16 y
)
{
    s16 centre_x;
    s16 centre_y;

    if (y < 0) return;
    centre_x = source_world_to_screen_x((s16)(x + 6));
    centre_y = source_world_to_screen_y((s16)(y + 7));
    spawn_large_effect(centre_x, centre_y, ground);
}

static void source_setup_large_explosion(
    bool ground,
    u8 repeat_count,
    s16 x,
    s16 y
)
{
    u8 index;
    u8 big = 0;

    if (y < 0) return;
    source_spawn_large_explosion_visual(ground, x, y);
    if (repeat_count > 10) {
        repeat_count = (u8)(repeat_count - 10u);
        big = 1;
    }
    if (repeat_count == 0) return;
    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        SourceRepeatingExplosion *repeating =
            &source_repeating_explosion[index];

        if (repeating->ttl != 0) continue;
        repeating->ttl = repeat_count;
        repeating->delay = 2;
        repeating->x = x;
        repeating->y = y;
        repeating->big = big;
        return;
    }
}

static u8 source_repeating_explosions_active(void)
{
    u8 index;

    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        if (source_repeating_explosion[index].ttl != 0) return 1;
    }
    return 0;
}

static void source_update_repeating_explosions(void)
{
    u8 index;

    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        SourceRepeatingExplosion *repeating =
            &source_repeating_explosion[index];
        s16 x;
        s16 y;

        if (repeating->ttl == 0) continue;
        if (repeating->delay > 0) {
            repeating->delay--;
            continue;
        }
        repeating->y = (s16)(
            repeating->y + source_parity_level.back_move2 + 1
        );
        x = (s16)(
            repeating->x +
            (s16)(ot_level_port_random(&source_parity_level) % 24u) -
            12
        );
        y = (s16)(
            repeating->y +
            (s16)(ot_level_port_random(&source_parity_level) % 27u) -
            24
        );
        if (repeating->big) {
            source_setup_large_explosion(false, 2, x, y);
            if (
                repeating->ttl == 1 ||
                ot_level_port_random(&source_parity_level) % 5u == 1u
            ) {
                source_parity_level.frame_sound_queue[7] = 11;
            } else {
                source_parity_level.frame_sound_queue[6] = 9;
            }
            repeating->delay = (u16)(
                4u + ot_level_port_random(&source_parity_level) % 3u
            );
        } else {
            source_spawn_explosion(x, y, 0, 1, 0);
            source_parity_level.frame_sound_queue[5] = 4;
            repeating->delay = 3;
        }
        repeating->ttl--;
    }
}

static void source_update_random_explosions(void)
{
    if (
        source_parity_level.random_explosions &&
        ot_level_port_random(&source_parity_level) % 10u == 1u
    ) {
        s16 x = (s16)(
            ot_level_port_random(&source_parity_level) % 280u
        );
        s16 y = (s16)(
            ot_level_port_random(&source_parity_level) % 180u
        );

        source_setup_large_explosion(false, 20, x, y);
    }
}

static void source_apply_level_frame_outputs(void)
{
    u8 index;

    if (source_parity_level.pending_text_window != 0) {
        gameplay_overlay_show_event(
            source_parity_level.pending_text_window
        );
        source_parity_level.pending_text_window = 0;
    }
    /* JE_drawEnemy() runs before JE_playerMovement() in the source loop. */
    player_source_velocity_x +=
        source_parity_level.frame_player_velocity_x_delta;
    player_source_velocity_y +=
        source_parity_level.frame_player_velocity_y_delta;
    source_parity_level.frame_player_velocity_x_delta = 0;
    source_parity_level.frame_player_velocity_y_delta = 0;
    for (
        index = 0;
        index < source_parity_level.frame_explosion_count;
        index++
    ) {
        const OtFrameExplosion *request =
            &source_parity_level.frame_explosion[index];

        source_spawn_explosion(
            request->x,
            request->y,
            request->delta_y,
            request->explosion_type,
            request->fixed_position
        );
    }
    source_parity_level.frame_explosion_count = 0;
}

static void source_emit_effect(const OtHitEffect *effect)
{
    s16 centre_x = source_world_to_screen_x((s16)(effect->x + 6));
    s16 centre_y = source_world_to_screen_y((s16)(effect->y + 7));

    if (effect->large) {
        source_setup_large_explosion(
            effect->ground,
            effect->repeat_count,
            effect->x,
            effect->y
        );
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
    u8 queue_index;

    /*
     * Direct tyrian2.c queue drain: every authored SFX and voice uses its
     * original one-based sndmast.h number.  Multiple occupied channels in
     * the same logic tick remain distinct Maxmod effects.
     */
    for (queue_index = 0; queue_index < 8; queue_index++) {
        source_sound_effect(
            source_parity_level.frame_sound_queue[queue_index]
        );
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
    /*
     * PC Low detail starts with background2=false.  Its special
     * background2over==3 draw path both draws layer 2 and permanently flips
     * background2=true, so a later authored event can still restore it.
     */
    if (
        !source_background2_enabled &&
        source_parity_level.background2_over == 3
    ) {
        if (background_prepare_layer(
                1,
                background_presentation_scroll_pixel(
                    1,
                    source_camera_offset_y
                ) >> 3
            )) {
            source_background2_enabled = 1;
            source_background2_upload_pending = 1;
        }
    }
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
}

static __attribute__((unused)) void source_apply_player_shot_result(
    PlayerShot *shot,
    OtShotCollisionResult *result
)
{
    u8 effect_index;
    u8 original_damage = shot != 0 ? shot->damage : 0;

    if (!result->collided) return;
    if (shot != 0) {
        if (shot->infinite) {
            shot->damage = original_damage;
            shot->active = 1;
        } else
        {
            shot->damage = result->remaining_damage;
            if (result->consumed) shot->active = 0;
        }
    }
    player_cash += result->cash_awarded;
    if (shot != 0 && result->superpixel_hit_count != 0) {
        u8 hit;

        for (hit = 0; hit < result->superpixel_hit_count; hit++) {
            source_superpixel_spawn(
                (s16)(shot->x + 6),
                (s16)(shot->y + 6),
                (u8)(original_damage / 2u + 3u),
                (u8)(original_damage / 4u + 2u),
                shot->blast_filter
            );
        }
    }
    for (
        effect_index = 0;
        effect_index < result->effect_count;
        effect_index++
    ) {
        source_emit_effect(&result->effects[effect_index]);
    }
}

static u8 source_option_projectile_collision_radii(
    u16 render_graphic,
    u8 *radius_w,
    u8 *radius_h
)
{
    if (
        render_graphic <= 60000 ||
        radius_w == 0 ||
        radius_h == 0
    ) {
        return 0;
    }
#if TYRIAN_GBA_STRESS_LOADOUT
    if (
        render_graphic !=
            60001 + SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
    ) {
        return 0;
    }
    *radius_w = SOURCE_OPTION_PROJECTILE_SOURCE_WIDTH / 2;
    *radius_h = SOURCE_OPTION_PROJECTILE_SOURCE_HEIGHT / 2;
#else
    {
        static const u8 width[
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ] = {62, 65, 62};
        static const u8 height[
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ] = {24, 28, 23};
        u16 graphic = (u16)(render_graphic - 60001u);
        u8 frame;

        if (graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC) {
            *radius_w = SOURCE_SPECIAL_OPTION_XEGA_WIDTH / 2u;
            *radius_h = SOURCE_SPECIAL_OPTION_XEGA_HEIGHT / 2u;
            return 1;
        }
        if (graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
            *radius_w = SOURCE_SPECIAL_OPTION_BANANA_WIDTH / 2u;
            *radius_h = SOURCE_SPECIAL_OPTION_BANANA_HEIGHT / 2u;
            return 1;
        }

        if (
            graphic < SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
            graphic >=
                SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC +
                    SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ) {
            return 0;
        }
        frame = (u8)(
            graphic - SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
        );
        *radius_w = (u8)(width[frame] / 2u);
        *radius_h = (u8)(height[frame] / 2u);
    }
#endif
    return 1;
}

#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
static bool source_autotest_enemy_matches_stop_pool(u8 index)
{
    switch (source_parity_level.stop_background_num) {
    case 1:
        return
            (index >= 25 && index < 50) ||
            (index >= 75 && index < 100);
    case 2:
        return index < 25;
    default:
        return true;
    }
}

/*
 * Route-smoke validates common ROMFS/event/death paths, not the temporary
 * legacy Pulse-Cannon adapter.  Some stock levels deliberately place
 * destructible control hitboxes above the visible playfield for weapons with
 * a taller PC collision radius.  If such a hitbox is the only remaining
 * member of an authored event-4 stop group, inject one source collision after
 * a grace period.  This exists only in route-smoke ROMs and is separately
 * reported, so release gameplay can never inherit or conceal the assist.
 */
static __attribute__((unused)) void
source_autotest_route_combat_assist(void)
{
    u8 index;
    u8 candidate = OT_ENEMY_COUNT;

    if (!source_parity_level.stop_backgrounds) {
        source_autotest_unreachable_stop_ticks = 0;
        return;
    }
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[index];

        if (
            !source_autotest_enemy_matches_stop_pool(index) ||
            source_parity_level.enemy_avail[index] != 0 ||
            enemy->scoreitem ||
            enemy->armorleft == 255
        ) {
            continue;
        }
        /*
         * The stock shot lifetime ends below y=-15.  A normal collision can
         * still reach an enemy centre down to y=-8; let keypad aiming handle
         * every such target without assistance.
         */
        if (enemy->ey >= -8 && enemy->ey <= 190) {
            /*
             * Keep already-earned off-screen grace.  Some authored control
             * hitboxes oscillate across this boundary for only a few ticks;
             * resetting here prevented the route input from ever reaching
             * the existing fallback even though most of each cycle remained
             * outside the stock shot's y<-15 lifetime.  A genuinely
             * reachable target is still left entirely to keypad fire while
             * it is on screen.
             */
            return;
        }
        if (candidate == OT_ENEMY_COUNT) candidate = index;
    }
    if (candidate == OT_ENEMY_COUNT) {
        source_autotest_unreachable_stop_ticks = 0;
        return;
    }
    if (++source_autotest_unreachable_stop_ticks < 60) return;
    source_autotest_unreachable_stop_ticks = 0;
    {
        const OtEnemy *enemy = &source_parity_level.enemy[candidate];
        OtShotCollisionResult result;
        uint8_t damage = enemy->armorleft;

        if (damage == 0) damage = 1;
        ot_level_port_begin_player_shot_collision_phase(
            &source_parity_level
        );
        ot_level_port_collide_player_shot(
            &source_parity_level,
            (int16_t)(enemy->ex + enemy->mapoffset),
            (int16_t)(enemy->ey - 6),
            damage,
            &result
        );
        ot_level_port_end_player_shot_collision_phase(
            &source_parity_level
        );
        if (result.collided) {
            telemetry_autotest_combat_assists++;
            source_apply_player_shot_result(0, &result);
        }
    }
}
#endif

#if TYRIAN_GBA_STRESS_LOADOUT
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
#else
static void
#endif
source_process_player_collisions(bool collide_player)
{
#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    u8 shot_index;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    bool collision_phase_started = false;
#endif
#endif
    u8 effect_index;
    OtPlayerCollisionResult player_result;
    bool player_contact_vulnerable;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    u32 collision_cycle_start = stress_cycle_counter_read();
#endif

#if !TYRIAN_GBA_STRESS_LOADOUT
    if (source_special_zinglon_damage_pending) {
        s16 width;

        /* The PC synthetic player-shot slot is consumed once per pulse. */
        source_special_zinglon_damage_pending = 0;
        width = source_special_zinglon_half_width_for_duration(
            source_special_zinglon_duration
        );
        if (
            !source_parity_level.end_level &&
            source_special_zinglon_duration > 1 &&
            width > 0
        ) {
            OtShotCollisionResult zinglon_result;

            ot_level_port_collide_zinglon_beam(
                &source_parity_level,
                (s16)(player_source_x + 7),
                (u8)width,
                &zinglon_result
            );
            source_apply_player_shot_result(
                0,
                &zinglon_result
            );
        }
    }
#endif

#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
        PlayerShot *shot = &player_shots[shot_index];
        OtShotCollisionResult result;

        if (!shot->active) continue;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
        if (!collision_phase_started) {
            ot_level_port_begin_player_shot_collision_phase(
                &source_parity_level
            );
            collision_phase_started = true;
        }
#endif
        if (shot->chain_weapon != 0) {
            if (
                ot_level_port_player_shot_overlaps(
                    &source_parity_level,
                    shot->x,
                    shot->y
                )
            ) {
                OtWeaponDefinition chain;

#if TYRIAN_GBA_STRESS_LOADOUT
                stress_shot_multi_pos[STRESS_BAY_MISC] = 0;
#else
                source_shot_multi_pos[SOURCE_WEAPON_BAY_MISC] = 0;
#endif
                if (
                    ot_data_hdt_weapon_read(
                        shot->chain_weapon,
                        &chain
                    ) &&
                    source_weapon_is_valid(&chain)
                ) {
#if TYRIAN_GBA_STRESS_LOADOUT
                    stress_spawn_weapon(
                        &chain,
                        STRESS_BAY_MISC,
                        shot->x,
                        shot->y
                    );
#else
                    source_spawn_weapon(
                        &chain,
                        SOURCE_WEAPON_BAY_MISC,
                        shot->x,
                        shot->y
                    );
#endif
                    telemetry_player_chain_volleys++;
                } else {
                    telemetry_stress_loadout_failures++;
                }
                shot->active = 0;
            }
            continue;
        }
#if TYRIAN_GBA_COLLISION_PACKED_CALL
        {
            u32 damage_and_radii = shot->damage;
            u8 radius_w;
            u8 radius_h;

            if (
                source_option_projectile_collision_radii(
                    shot->render_graphic,
                    &radius_w,
                    &radius_h
                )
            ) {
                damage_and_radii |=
                    (u32)radius_w << 8;
                damage_and_radii |=
                    (u32)radius_h << 16;
            }
            /*
             * shipblastfilter is a source palette-hue byte (0x20, 0x70,
             * 0x80, 0xd0, ...).  Packed bits 24..27 carry that high nibble;
             * bit 28 remains the independent iced flag.
             */
            damage_and_radii |=
                (u32)((shot->blast_filter >> 4) & 0x0fu) << 24;
            if (shot->iced != 0) {
                damage_and_radii |= (u32)1u << 28;
            }
            ot_level_port_collide_player_shot_packed(
                &source_parity_level,
                shot->x,
                shot->y,
                &result,
                damage_and_radii
            );
        }
#else
        {
            u8 radius_w;
            u8 radius_h;
            u32 damage_and_radii = shot->damage |
                ((u32)((shot->blast_filter >> 4) & 0x0fu) << 24);

            if (shot->iced != 0) {
                damage_and_radii |= (u32)1u << 28;
            }

            if (
                source_option_projectile_collision_radii(
                    shot->render_graphic,
                    &radius_w,
                    &radius_h
                )
            ) {
                damage_and_radii |= (u32)radius_w << 8;
                damage_and_radii |= (u32)radius_h << 16;
                ot_level_port_collide_player_shot_packed(
                    &source_parity_level,
                    shot->x,
                    shot->y,
                    &result,
                    damage_and_radii
                );
            } else {
                ot_level_port_collide_player_shot_packed(
                    &source_parity_level,
                    shot->x,
                    shot->y,
                    &result,
                    damage_and_radii
                );
            }
        }
#endif
        source_apply_player_shot_result(shot, &result);
    }
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    if (collision_phase_started) {
        ot_level_port_end_player_shot_collision_phase(
            &source_parity_level
        );
    }
#endif
#endif
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    stress_cycle_accumulate(
        collision_cycle_start,
        &telemetry_stress_collision_cycles_total,
        &telemetry_stress_collision_cycles_max
    );
#endif
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    if (collide_player) source_autotest_route_combat_assist();
#endif
#endif

    /*
     * tyrian2.c keeps advancing/colliding every existing player shot during
     * endLevel, but JE_playerCollide() itself is guarded by
     * player.is_alive && !endLevel.  Keep that boundary explicit: otherwise
     * the GBA port can leave linked enemies or event-jump targets alive while
     * the level-exit animation is already running.
     */
    if (!collide_player) return;

    source_parity_level.player_armor = player_armor;
#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE && \
    (!defined(AUTOTEST) || defined(AUTOTEST_FRONTEND_ROUTE_SECTION))
    /*
     * The development flag represents the same collision gate as a non-zero
     * PC invulnerable_ticks value.  Merely suppressing player_damage() is not
     * sufficient: JE_playerCollide() would still let the invincible ship ram
     * a low-armour link-254 boss component, clear the entire linked group via
     * its contact-death path, and bypass the player-shot-only event jump.
     * Pickups remain active because ot_level_port_collide_player() gates only
     * the damaging-contact tail with this boolean.
     * The historical first-level golden AUTOTEST deliberately keeps its own
     * deterministic contact trace; arbitrary route-smoke tests use the real
     * development-flag semantics so linked bosses cannot be bypassed.
     */
    player_contact_vulnerable = false;
#else
    player_contact_vulnerable =
        player_alive && player_invulnerable == 0;
#endif
    ot_level_port_collide_player(
        &source_parity_level,
        player_contact_vulnerable,
        &player_result
    );
    player_armor = source_parity_level.player_armor;
    player_cash += player_result.cash_awarded;
    /*
     * JE_playerCollide() runs before JE_playerMovement().  Preserve the wide
     * pre-clamp velocity so the source friction/input/clamp order remains
     * intact even for high-armour authored collision objects.
     */
    player_source_velocity_x +=
        player_result.player_velocity_x_delta;
    player_source_velocity_y +=
        player_result.player_velocity_y_delta;
    for (
        effect_index = 0;
        effect_index < player_result.effect_count;
        effect_index++
    ) {
        source_emit_effect(&player_result.effects[effect_index]);
    }
    for (
        effect_index = 0;
        effect_index < player_result.pickup_effect_count;
        effect_index++
    ) {
        source_spawn_fixed_pickup_explosion(
            &player_result.pickup_effects[effect_index]
        );
    }
    while (player_result.orbiting_asteroids_awarded != 0) {
        (void)spawn_orbiting_asteroid_shot();
        player_result.orbiting_asteroids_awarded--;
    }
    if (player_result.reset_all_shot_multi_pos) {
#if TYRIAN_GBA_STRESS_LOADOUT
        memset(stress_shot_multi_pos, 0, sizeof(stress_shot_multi_pos));
#else
        memset(source_shot_multi_pos, 0, sizeof(source_shot_multi_pos));
#endif
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (player_result.front_weapon_picked_up) {
        /* JE_playerCollide(): shotMultiPos[SHOT_FRONT]=0; repeat=10. */
        source_front_weapon_bound = 0;
        (void)source_front_weapon_sync();
        source_shot_multi_pos[SOURCE_WEAPON_BAY_FRONT] = 0;
        source_shot_repeat[SOURCE_WEAPON_BAY_FRONT] = 10;
    }
    if (player_result.special_weapon_picked_up) {
        source_special_bound_id = 0xff;
        (void)source_special_sync();
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 10;
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL2] = 0;
    }
#endif
    if (
        player_result.pickup_message_type !=
            OT_PICKUP_MESSAGE_NONE
    ) {
        gameplay_overlay_show_pickup(
            player_result.pickup_message_type,
            player_result.pickup_message_item_id
        );
    }
    if (player_result.bonus_level_triggered) {
        /*
         * mainint.c plays source song 30 ("Zanac"), then displays
         * miscText[59] ("Secret Level!") for 150 ticks.
         */
        audio_load_source_song_once_preserve_effects(30);
        secret_level_music_active = 1;
        insert_coin_tile_upload_pending = 0;
        secret_level_tile_upload_pending = 1;
        secret_level_palette_dirty = 1;
    }
    if (player_result.damage) player_damage(player_result.damage);
}

static bool source_apply_enemy_shot_impact(
    void *context,
    const OtEnemyShotImpact *impact
)
{
    u16 damage_through_shield;

    (void)context;
    /* tyrian2.c creates this type-0 impact even during invulnerability. */
    source_spawn_explosion(
        impact->x,
        impact->y,
        0,
        0,
        0
    );
    damage_through_shield = player_damage(impact->damage);
    if (damage_through_shield > 0) {
        player_source_velocity_x +=
            ((s32)impact->velocity_x * damage_through_shield) / 2;
        player_source_velocity_y +=
            ((s32)impact->velocity_y * damage_through_shield) / 2;
    }
    return player_alive != 0;
}

static void source_update_enemy_projectiles(void)
{
    u8 hit_area_x = source_parity_level.player_shot_hit_area_x;
    u8 hit_area_y = source_parity_level.player_shot_hit_area_y;

    /*
     * JE_mainGamePlayerFunctions() runs between player/enemy collision and
     * enemy-shot movement.  Reflect the just-applied GBA keypad input at the
     * same phase boundary, directly in unchanged OpenTyrian coordinates.
     */
    source_parity_level.player_x = player_source_x;
    source_parity_level.player_y = player_source_y;
    if (!player_alive) {
        /* PC keeps moving/drawing shots but skips its alive-player loop. */
        source_parity_level.player_shot_hit_area_x = 0;
        source_parity_level.player_shot_hit_area_y = 0;
    }
    ot_level_port_update_enemy_shots(
        &source_parity_level,
        source_apply_enemy_shot_impact,
        0
    );
    source_parity_level.player_shot_hit_area_x = hit_area_x;
    source_parity_level.player_shot_hit_area_y = hit_area_y;
    telemetry_collision_count =
        source_parity_level.player_shot_collision_count +
        source_parity_level.enemy_shot_player_hit_count +
        source_parity_level.player_enemy_contact_count;
    source_sync_runtime_telemetry();
}

static void source_runtime_reset(void)
{
    u32 layer_passed_checks;
    u32 layer_failed_checks;

    memset(source_enemy_cache, 0, sizeof(source_enemy_cache));
    memset(
        source_repeating_explosion,
        0,
        sizeof(source_repeating_explosion)
    );
    memset(source_enemy_uploads, 0, sizeof(source_enemy_uploads));
    source_player_cache_graphic = 0;
    source_player_cache_valid = 0;
    source_player_cache_visible = 0;
    source_player_upload_pending = 0;
    source_player_upload_l2_slot = 0;
    memset(
        source_projectile_cache,
        0,
        sizeof(source_projectile_cache)
    );
    memset(
        source_projectile_uploads,
        0,
        sizeof(source_projectile_uploads)
    );
    memset(
        source_effect_cache_last_epoch,
        0,
        sizeof(source_effect_cache_last_epoch)
    );
    memset(
        source_effect_cache_frame_key,
        SOURCE_EFFECT_FRAME_KEY_INVALID,
        sizeof(source_effect_cache_frame_key)
    );
    memset(source_effect_uploads, 0, sizeof(source_effect_uploads));
    memset(source_effect_frame_slot, 0, sizeof(source_effect_frame_slot));
    source_enemy_cache_epoch = 0;
    source_enemy_upload_count = 0;
    source_projectile_cache_epoch = 0;
    source_projectile_upload_count = 0;
    source_effect_cache_epoch = 0;
    source_effect_upload_count = 0;
    source_effect_boss_bar_tiles_dirty = 0;
    source_effect_reward_tiles_dirty = 0;
    source_effect_reward_tiles_required = 0;
    source_effect_status_tiles_dirty = 0;
    source_effect_status_tiles_required = 0;
    telemetry_sprite2_l2_hits = 0;
    telemetry_sprite2_l2_misses = 0;
    telemetry_sprite2_l2_evictions = 0;
    telemetry_sprite2_l2_drops = 0;
    telemetry_sprite2_l2_flushes = 0;
    telemetry_sprite2_l2_raw_builds = 0;
    telemetry_sprite2_l2_rle_fallbacks = 0;
    telemetry_sprite2_l2_max_visible_unique = 0;
    if (!source_enemy_palette_setup(SOURCE_LEVEL_PALETTE_INDEX)) {
        source_parity_level.assets_valid = false;
    }
    if (!source_option_projectile_sync()) {
        source_parity_level.assets_valid = false;
#if TYRIAN_GBA_STRESS_LOADOUT
        stress_loadout_valid = 0;
#endif
    }
    /*
     * Capture the authoritative post-Sprite2/post-projectile OBJ palette once
     * per level.  Pentium filtration then derives reversible palette states
     * without ever filtering an already-filtered result.
     */
    source_detail_capture_obj_palette();
    if (
        !source_layer_priority_self_test(
            &layer_passed_checks,
            &layer_failed_checks
        )
    ) {
        source_parity_level.assets_valid = false;
    }
    telemetry_layer_rule_checks = layer_passed_checks;
    telemetry_layer_rule_failures = layer_failed_checks;
    telemetry_source_visible_enemies = 0;
    telemetry_source_max_visible_enemies = 0;
    telemetry_source_unknown_visuals = 0;
    telemetry_sprite2_decode_failures = 0;
    telemetry_sprite2_cache_hits = 0;
    telemetry_sprite2_cache_misses = 0;
    telemetry_sprite2_cache_evictions = 0;
    telemetry_sprite2_cache_drops = 0;
    source_enemy_first_decode_failure_valid = 0;
    source_enemy_first_decode_failure_shape_table = 0;
    source_enemy_first_decode_failure_graphic = 0;
    source_enemy_first_decode_failure_size = 0;
    source_enemy_first_decode_failure_filter = 0;
    source_enemy_first_decode_failure_pool = 0;
    source_enemy_first_decode_failure_source_index = 0;
    source_enemy_first_decode_failure_definition = 0;
    source_enemy_first_decode_failure_level_position = 0;
    source_enemy_first_decode_failure_x = 0;
    source_enemy_first_decode_failure_y = 0;
    source_enemy_first_decode_failure_cycle = 0;
    source_enemy_first_decode_failure_shape_banks = 0;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    source_autotest_unreachable_stop_ticks = 0;
#endif
    telemetry_sprite2_uploads = 0;
    telemetry_sprite2_upload_bytes = 0;
    telemetry_sprite2_compact_uploads = 0;
    telemetry_sprite2_split_uploads = 0;
    telemetry_sprite2_upload_coalesces = 0;
    telemetry_sprite2_max_uploads = 0;
    telemetry_sprite2_max_visible_unique = 0;
    telemetry_boss_perf_started = 0;
    telemetry_boss_perf_completed = 0;
    telemetry_boss_perf_start_position = 0;
    telemetry_boss_perf_end_position = 0;
    telemetry_boss_perf_display_frames = 0;
    telemetry_boss_perf_missed_vblanks = 0;
    telemetry_boss_perf_sprite2_misses = 0;
    telemetry_boss_perf_sprite2_evictions = 0;
    telemetry_boss_perf_sprite2_upload_bytes = 0;
    telemetry_boss_perf_projectile_misses = 0;
    telemetry_boss_perf_l2_hits = 0;
    telemetry_boss_perf_l2_misses = 0;
    telemetry_boss_perf_l2_evictions = 0;
    telemetry_boss_perf_l2_raw_builds = 0;
    telemetry_boss_perf_l2_fallbacks = 0;
    boss_perf_start_display_frames = 0;
    boss_perf_start_missed_vblanks = 0;
    boss_perf_start_sprite2_misses = 0;
    boss_perf_start_sprite2_evictions = 0;
    boss_perf_start_sprite2_upload_bytes = 0;
    boss_perf_start_projectile_misses = 0;
    boss_perf_start_l2_hits = 0;
    boss_perf_start_l2_misses = 0;
    boss_perf_start_l2_evictions = 0;
    boss_perf_start_l2_raw_builds = 0;
    boss_perf_start_l2_fallbacks = 0;
    telemetry_effect_cache_hits = 0;
    telemetry_effect_cache_misses = 0;
    telemetry_effect_cache_evictions = 0;
    telemetry_effect_cache_drops = 0;
    telemetry_effect_cache_uploads = 0;
    telemetry_effect_cache_upload_bytes = 0;
    telemetry_effect_cache_upload_coalesces = 0;
    telemetry_effect_cache_max_uploads = 0;
    telemetry_effect_cache_max_visible_unique = 0;
}
