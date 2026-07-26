/*
 * GBA presentation/input adapter for the directly translated OpenTyrian
 * first-level runtime.  Source gameplay stays in its original coordinate
 * system.  These helpers reproduce JE_starShowVGA's x=24 extraction, then
 * centre-crop its 264x184 gameplay viewport to the GBA's native 240x160.
 * No source coordinate or sprite is rescaled.
 */

#define SOURCE_ENEMY_CATALOG_HEADER_BYTES 16

typedef struct {
    u16 frame_index;
    u32 last_epoch;
    u8 valid;
} SourceEnemyCacheSlot;

typedef struct {
    u8 slot;
    u16 frame_index;
} SourceEnemyUpload;

static SourceEnemyCacheSlot
    source_enemy_cache[OBJ_DYNAMIC_SLOT_COUNT] EWRAM_DATA;
static SourceEnemyUpload
    source_enemy_uploads[OBJ_DYNAMIC_SLOT_COUNT] EWRAM_DATA;
static u32 source_enemy_cache_epoch;
static u8 source_enemy_upload_count;
static u8 source_enemy_catalog_valid;

static u16 source_read_u16(const u8 *source)
{
    return (u16)(source[0] | ((u16)source[1] << 8));
}

static u32 source_read_u32(const u8 *source)
{
    return
        (u32)source[0] |
        ((u32)source[1] << 8) |
        ((u32)source[2] << 16) |
        ((u32)source[3] << 24);
}

static const u8 *source_enemy_catalog_record(u16 index)
{
    return enemy_frame_catalog +
        SOURCE_ENEMY_CATALOG_HEADER_BYTES +
        (u32)index * OBJ_ENEMY_FRAME_RECORD_BYTES;
}

static const u8 *source_find_enemy_frame(
    u8 shape_table,
    u16 graphic,
    u8 size
)
{
    s16 low = 0;
    s16 high = (s16)OBJ_ENEMY_FRAME_COUNT - 1;

    if (!source_enemy_catalog_valid) return 0;
    while (low <= high) {
        s16 middle = (s16)(low + (high - low) / 2);
        const u8 *record = source_enemy_catalog_record((u16)middle);
        u8 record_shape = record[0];
        u8 record_size = record[1];
        u16 record_graphic = source_read_u16(record + 2);

        if (
            record_shape < shape_table ||
            (
                record_shape == shape_table &&
                record_graphic < graphic
            ) ||
            (
                record_shape == shape_table &&
                record_graphic == graphic &&
                record_size < size
            )
        ) {
            low = (s16)(middle + 1);
        } else if (
            record_shape > shape_table ||
            (
                record_shape == shape_table &&
                record_graphic > graphic
            ) ||
            (
                record_shape == shape_table &&
                record_graphic == graphic &&
                record_size > size
            )
        ) {
            high = (s16)(middle - 1);
        } else {
            return record;
        }
    }
    return 0;
}

static void source_enemy_cache_begin_frame(void)
{
    source_enemy_cache_epoch++;
    if (source_enemy_cache_epoch == 0) {
        u8 index;

        source_enemy_cache_epoch = 1;
        for (index = 0; index < OBJ_DYNAMIC_SLOT_COUNT; index++) {
            source_enemy_cache[index].last_epoch = 0;
        }
    }
    source_enemy_upload_count = 0;
}

static u16 source_enemy_cache_acquire(
    const OtEnemyDrawCommand *draw,
    u8 *palette
)
{
    const u8 *record = source_find_enemy_frame(
        draw->shape_table,
        draw->graphic,
        draw->size
    );
    u16 frame_index;
    u8 index;
    u8 selected = OBJ_DYNAMIC_SLOT_COUNT;
    u8 free_slot = OBJ_DYNAMIC_SLOT_COUNT;
    u8 eviction_slot = OBJ_DYNAMIC_SLOT_COUNT;
    u32 oldest_epoch = 0xffffffffu;

    if (record == 0) {
        telemetry_source_unknown_visuals++;
        telemetry_enemy_frame_catalog_misses++;
        return 0xffff;
    }
    frame_index = source_read_u16(record + 6);
    *palette = draw->filter ?
        OBJ_PAL_ENEMY_FILTER :
        record[4];

    for (index = 0; index < OBJ_DYNAMIC_SLOT_COUNT; index++) {
        SourceEnemyCacheSlot *slot = &source_enemy_cache[index];

        if (slot->valid && slot->frame_index == frame_index) {
            slot->last_epoch = source_enemy_cache_epoch;
            telemetry_enemy_frame_cache_hits++;
            return (u16)(
                OBJ_DYNAMIC_TILE_BASE +
                index * OBJ_DYNAMIC_TILES_PER_SLOT
            );
        }
        if (!slot->valid) {
            if (free_slot == OBJ_DYNAMIC_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_enemy_cache_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != OBJ_DYNAMIC_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == OBJ_DYNAMIC_SLOT_COUNT) {
        telemetry_enemy_frame_cache_drops++;
        return 0xffff;
    }
    if (source_enemy_cache[selected].valid) {
        telemetry_enemy_frame_cache_evictions++;
    }
    source_enemy_cache[selected].valid = 1;
    source_enemy_cache[selected].frame_index = frame_index;
    source_enemy_cache[selected].last_epoch = source_enemy_cache_epoch;
    telemetry_enemy_frame_cache_misses++;
    if (source_enemy_upload_count >= OBJ_DYNAMIC_SLOT_COUNT) {
        telemetry_enemy_frame_cache_drops++;
        source_enemy_cache[selected].valid = 0;
        return 0xffff;
    }
    source_enemy_uploads[source_enemy_upload_count].slot = selected;
    source_enemy_uploads[source_enemy_upload_count].frame_index =
        frame_index;
    source_enemy_upload_count++;
    if (
        source_enemy_upload_count >
        telemetry_enemy_frame_max_uploads
    ) {
        telemetry_enemy_frame_max_uploads = source_enemy_upload_count;
    }
    return (u16)(
        OBJ_DYNAMIC_TILE_BASE +
        selected * OBJ_DYNAMIC_TILES_PER_SLOT
    );
}

static void source_enemy_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_enemy_upload_count; index++) {
        const SourceEnemyUpload *upload = &source_enemy_uploads[index];
        const u8 *source =
            enemy_frame_tiles +
            (u32)upload->frame_index * OBJ_ENEMY_FRAME_BYTES;
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            (
                OBJ_DYNAMIC_TILE_BASE +
                upload->slot * OBJ_DYNAMIC_TILES_PER_SLOT
            ) * 32;

        dmaCopy(source, target, OBJ_ENEMY_FRAME_BYTES);
        telemetry_enemy_frame_uploads++;
        telemetry_enemy_frame_upload_bytes += OBJ_ENEMY_FRAME_BYTES;
    }
    source_enemy_upload_count = 0;
}

static s16 source_world_to_screen_x(s16 source_x)
{
    return (s16)(source_x - SOURCE_PRESENTATION_X_ORIGIN);
}

static s16 source_world_to_screen_y(s16 source_y)
{
    return (s16)(source_y - SOURCE_PRESENTATION_Y_ORIGIN);
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
        return layer == 2 ? BG3_INITIAL_HOFS : BG12_INITIAL_HOFS;
    }
    if (layer == 0) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x_offset
        );
    }
    if (layer == 1) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x2_offset
        );
    }
    if (state->presentation_background3_x1) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x3_offset
        );
    }
    return (u16)(
        SOURCE_BG3_PARALLAX_BASE_X -
        state->presentation_map_x3_offset
    );
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
     * enemy-shot movement.  Reflect the just-applied GBA keypad input at the
     * same phase boundary, directly in unchanged OpenTyrian coordinates.
     */
    source_parity_level.player_x = player_source_x;
    source_parity_level.player_y = player_source_y;
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
    memset(source_enemy_cache, 0, sizeof(source_enemy_cache));
    memset(source_enemy_uploads, 0, sizeof(source_enemy_uploads));
    source_enemy_cache_epoch = 0;
    source_enemy_upload_count = 0;
    source_enemy_catalog_valid =
        enemy_frame_catalog[0] == 'O' &&
        enemy_frame_catalog[1] == 'T' &&
        enemy_frame_catalog[2] == 'E' &&
        enemy_frame_catalog[3] == 'F' &&
        source_read_u16(enemy_frame_catalog + 4) == 1 &&
        source_read_u16(enemy_frame_catalog + 6) ==
            OBJ_ENEMY_FRAME_COUNT &&
        source_read_u16(enemy_frame_catalog + 8) ==
            OBJ_ENEMY_FRAME_RECORD_BYTES &&
        source_read_u16(enemy_frame_catalog + 10) ==
            OBJ_ENEMY_FRAME_TILES &&
        source_read_u32(enemy_frame_catalog + 12) ==
            OBJ_ENEMY_FRAME_TILE_BYTES;
    if (!source_enemy_catalog_valid) {
        source_parity_level.assets_valid = false;
    }
    telemetry_source_visible_enemies = 0;
    telemetry_source_max_visible_enemies = 0;
    telemetry_source_unknown_visuals = 0;
    telemetry_enemy_frame_catalog_misses = 0;
    telemetry_enemy_frame_cache_hits = 0;
    telemetry_enemy_frame_cache_misses = 0;
    telemetry_enemy_frame_cache_evictions = 0;
    telemetry_enemy_frame_cache_drops = 0;
    telemetry_enemy_frame_uploads = 0;
    telemetry_enemy_frame_upload_bytes = 0;
    telemetry_enemy_frame_max_uploads = 0;
}
