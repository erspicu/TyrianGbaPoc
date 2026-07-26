/*
 * GBA presentation/input adapter for the directly translated OpenTyrian
 * first-level runtime.  Source gameplay stays in its original coordinate
 * system.  These helpers reproduce JE_starShowVGA's x=24 extraction, then
 * centre-crop its 264x184 gameplay viewport to the GBA's native 240x160.
 * No source coordinate or sprite is rescaled.
 */

typedef struct {
    u16 graphic;
    u32 last_epoch;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 valid;
} SourceEnemyCacheSlot;

typedef struct {
    u8 slot;
} SourceEnemyUpload;

static SourceEnemyCacheSlot
    source_enemy_cache[SOURCE_ENEMY_CACHE_SLOT_COUNT] EWRAM_BSS;
static SourceEnemyUpload
    source_enemy_uploads[SOURCE_ENEMY_CACHE_SLOT_COUNT] EWRAM_BSS;
static u8 source_enemy_cache_tiles
    [SOURCE_ENEMY_CACHE_SLOT_COUNT][SOURCE_ENEMY_FRAME_BYTES]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 source_enemy_decode_pixels[OT_SPRITE2_FRAME_PIXELS]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 source_enemy_palette_map[OT_PALETTE_COLOUR_COUNT]
    EWRAM_BSS;
static u32 source_enemy_cache_epoch;
static u8 source_enemy_upload_count;
static u8 source_enemy_palette_valid;

/*
 * The source level needs more than 15 simultaneous colours per enemy frame,
 * so it uses 8bpp OBJ.  Static 4bpp actors retain their authored banks; only
 * banks unused by the source-level renderer are time-shared here.
 */
static const u8 source_enemy_dynamic_palette_banks[
    SOURCE_ENEMY_DYNAMIC_PALETTE_BANK_COUNT
] = {
    1, 2, 3, 4, 5, 8, 13, 15,
};

static const u8 source_enemy_brightness_samples[
    SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT
] = {
    0, 2, 4, 6, 8, 10, 12, 15,
};

static u32 source_palette_distance(
    const u8 *left,
    const u8 *right
)
{
    s16 red = (s16)left[0] - right[0];
    s16 green = (s16)left[1] - right[1];
    s16 blue = (s16)left[2] - right[2];

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}

static u8 source_enemy_palette_destination(u8 logical_index)
{
    u8 bank = source_enemy_dynamic_palette_banks[
        logical_index / 16
    ];

    return (u8)(bank * 16 + logical_index % 16);
}

static u8 source_enemy_palette_setup(u8 palette_index)
{
    OtDataView palette;
    u8 hue;

    source_enemy_palette_valid = 0;
    if (!ot_data_palette_view(palette_index, &palette)) return 0;

    for (hue = 0; hue < 16; hue++) {
        u8 brightness;

        for (
            brightness = 0;
            brightness < SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT;
            brightness++
        ) {
            u8 logical = (u8)(
                hue * SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT +
                brightness
            );
            u8 source_index = (u8)(
                hue * 16 +
                source_enemy_brightness_samples[brightness]
            );
            u8 destination =
                source_enemy_palette_destination(logical);

            SPRITE_PALETTE[destination] =
                title_palette_colour(&palette, source_index);
        }

        /*
         * Preserve the PC hue nibble and choose the closest of eight
         * brightnesses in the actual active palette.  This is deterministic
         * and applies to every current or future Sprite2 frame.
         */
        for (brightness = 0; brightness < 16; brightness++) {
            const u8 *source_rgb =
                palette.data + (u32)(hue * 16 + brightness) * 3;
            u32 best_distance = 0xffffffffu;
            u8 best = 0;
            u8 candidate;

            for (
                candidate = 0;
                candidate < SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT;
                candidate++
            ) {
                u8 candidate_index = (u8)(
                    hue * 16 +
                    source_enemy_brightness_samples[candidate]
                );
                u32 distance = source_palette_distance(
                    source_rgb,
                    palette.data + (u32)candidate_index * 3
                );

                if (distance < best_distance) {
                    best_distance = distance;
                    best = candidate;
                }
            }
            source_enemy_palette_map[hue * 16 + brightness] =
                source_enemy_palette_destination(
                    (u8)(
                        hue * SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT +
                        best
                    )
                );
        }
    }
    source_enemy_palette_valid = 1;
    return 1;
}

static u16 source_enemy_cache_tile(u8 slot)
{
    if (slot < SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT) {
        return (u16)(
            SOURCE_ENEMY_CACHE_LOWER_TILE_BASE +
            slot * SOURCE_ENEMY_TILES_PER_SLOT
        );
    }
    return (u16)(
        SOURCE_ENEMY_CACHE_UPPER_TILE_BASE +
        (slot - SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT) *
            SOURCE_ENEMY_TILES_PER_SLOT
    );
}

static void source_enemy_pack_8bpp(u8 slot)
{
    u8 *target = source_enemy_cache_tiles[slot];
    u8 tile_y;

    for (tile_y = 0; tile_y < 4; tile_y++) {
        u8 tile_x;

        for (tile_x = 0; tile_x < 4; tile_x++) {
            u8 row;

            for (row = 0; row < 8; row++) {
                u8 column;

                for (column = 0; column < 8; column++) {
                    u16 source_pixel = source_enemy_decode_pixels[
                        (u32)(tile_y * 8 + row) *
                            OT_SPRITE2_FRAME_WIDTH +
                        tile_x * 8 + column
                    ];
                    u32 target_offset =
                        (u32)(tile_y * 4 + tile_x) * 64 +
                        row * 8 +
                        column;

                    target[target_offset] = source_pixel == 0 ?
                        0 :
                        source_enemy_palette_map[
                            (u8)(source_pixel - 1)
                        ];
                }
            }
        }
    }
}

static void source_enemy_cache_begin_frame(void)
{
    source_enemy_cache_epoch++;
    if (source_enemy_cache_epoch == 0) {
        u8 index;

        source_enemy_cache_epoch = 1;
        for (
            index = 0;
            index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
            index++
        ) {
            source_enemy_cache[index].last_epoch = 0;
        }
    }
    source_enemy_upload_count = 0;
}

static void source_enemy_cache_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
        index++
    ) {
        if (
            source_enemy_cache[index].valid &&
            source_enemy_cache[index].last_epoch ==
                source_enemy_cache_epoch
        ) {
            visible_unique++;
        }
    }
    if (
        visible_unique >
        telemetry_sprite2_max_visible_unique
    ) {
        telemetry_sprite2_max_visible_unique = visible_unique;
    }
}

static u16 source_enemy_cache_acquire(
    const OtEnemyDrawCommand *draw
)
{
    u8 index;
    u8 selected = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 free_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u32 oldest_epoch = 0xffffffffu;

    if (!source_enemy_palette_valid) {
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        return 0xffff;
    }

    for (
        index = 0;
        index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
        index++
    ) {
        SourceEnemyCacheSlot *slot = &source_enemy_cache[index];

        if (
            slot->valid &&
            slot->shape_table == draw->shape_table &&
            slot->graphic == draw->graphic &&
            slot->size == draw->size &&
            slot->filter == draw->filter
        ) {
            slot->last_epoch = source_enemy_cache_epoch;
            telemetry_sprite2_cache_hits++;
            return source_enemy_cache_tile(index);
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
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
    selected = free_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
        telemetry_sprite2_cache_drops++;
        return 0xffff;
    }
    if (source_enemy_cache[selected].valid) {
        telemetry_sprite2_cache_evictions++;
    }
    source_enemy_cache[selected].valid = 1;
    source_enemy_cache[selected].shape_table = draw->shape_table;
    source_enemy_cache[selected].graphic = draw->graphic;
    source_enemy_cache[selected].size = draw->size;
    source_enemy_cache[selected].filter = draw->filter;
    source_enemy_cache[selected].last_epoch = source_enemy_cache_epoch;
    telemetry_sprite2_cache_misses++;
    if (
        !ot_sprite2_frame_decode(
            draw->shape_table,
            draw->graphic,
            draw->size,
            draw->filter,
            source_enemy_decode_pixels,
            OT_SPRITE2_FRAME_PIXELS
        )
    ) {
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        source_enemy_cache[selected].valid = 0;
        return 0xffff;
    }
    source_enemy_pack_8bpp(selected);

    if (
        source_enemy_upload_count >= SOURCE_ENEMY_CACHE_SLOT_COUNT
    ) {
        telemetry_sprite2_cache_drops++;
        source_enemy_cache[selected].valid = 0;
        return 0xffff;
    }
    source_enemy_uploads[source_enemy_upload_count].slot = selected;
    source_enemy_upload_count++;
    if (
        source_enemy_upload_count >
        telemetry_sprite2_max_uploads
    ) {
        telemetry_sprite2_max_uploads = source_enemy_upload_count;
    }
    return source_enemy_cache_tile(selected);
}

static void source_enemy_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_enemy_upload_count; index++) {
        const SourceEnemyUpload *upload = &source_enemy_uploads[index];
        const u8 *source = source_enemy_cache_tiles[upload->slot];
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            source_enemy_cache_tile(upload->slot) * 32;

        dmaCopy(source, target, SOURCE_ENEMY_FRAME_BYTES);
        telemetry_sprite2_uploads++;
        telemetry_sprite2_upload_bytes += SOURCE_ENEMY_FRAME_BYTES;
    }
    source_enemy_upload_count = 0;
}

typedef struct {
    u32 last_epoch;
    u8 frame_key;
    u8 valid;
} SourceEffectCacheSlot;

typedef struct {
    u8 slot;
    u8 frame_key;
} SourceEffectUpload;

static SourceEffectCacheSlot source_effect_cache[
    SOURCE_EFFECT_CACHE_SLOT_COUNT
] EWRAM_BSS;
static SourceEffectUpload source_effect_uploads[
    SOURCE_EFFECT_CACHE_SLOT_COUNT
] EWRAM_BSS;
static u32 source_effect_cache_epoch;
static u8 source_effect_upload_count;

static void source_effect_cache_begin_frame(void)
{
    source_effect_cache_epoch++;
    if (source_effect_cache_epoch == 0) {
        u8 index;

        source_effect_cache_epoch = 1;
        for (
            index = 0;
            index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
            index++
        ) {
            source_effect_cache[index].last_epoch = 0;
        }
    }
    source_effect_upload_count = 0;
}

static void source_effect_cache_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
        index++
    ) {
        if (
            source_effect_cache[index].valid &&
            source_effect_cache[index].last_epoch ==
                source_effect_cache_epoch
        ) {
            visible_unique++;
        }
    }
    if (
        visible_unique >
        telemetry_effect_cache_max_visible_unique
    ) {
        telemetry_effect_cache_max_visible_unique = visible_unique;
    }
}

static u16 source_effect_cache_acquire(u8 sequence, u8 frame)
{
    u8 frame_key;
    u8 index;
    u8 free_slot = SOURCE_EFFECT_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_EFFECT_CACHE_SLOT_COUNT;
    u32 oldest_epoch = 0xffffffffu;
    u8 selected;

    if (
        sequence >= OBJ_EXPLOSION_SEQUENCE_COUNT ||
        frame >= OBJ_EXPLOSION_FRAME_COUNT
    ) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    frame_key = (u8)(
        sequence * OBJ_EXPLOSION_FRAME_COUNT + frame
    );
    for (
        index = 0;
        index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
        index++
    ) {
        SourceEffectCacheSlot *slot = &source_effect_cache[index];

        if (slot->valid && slot->frame_key == frame_key) {
            slot->last_epoch = source_effect_cache_epoch;
            telemetry_effect_cache_hits++;
            return (u16)(
                SOURCE_EFFECT_CACHE_TILE_BASE +
                index * SOURCE_EFFECT_TILES_PER_SLOT
            );
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_EFFECT_CACHE_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_effect_cache_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != SOURCE_EFFECT_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == SOURCE_EFFECT_CACHE_SLOT_COUNT) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    if (source_effect_cache[selected].valid) {
        telemetry_effect_cache_evictions++;
    }
    if (
        source_effect_upload_count >= SOURCE_EFFECT_CACHE_SLOT_COUNT
    ) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    source_effect_cache[selected].valid = 1;
    source_effect_cache[selected].frame_key = frame_key;
    source_effect_cache[selected].last_epoch =
        source_effect_cache_epoch;
    source_effect_uploads[source_effect_upload_count].slot = selected;
    source_effect_uploads[source_effect_upload_count].frame_key =
        frame_key;
    source_effect_upload_count++;
    telemetry_effect_cache_misses++;
    if (
        source_effect_upload_count >
        telemetry_effect_cache_max_uploads
    ) {
        telemetry_effect_cache_max_uploads =
            source_effect_upload_count;
    }
    return (u16)(
        SOURCE_EFFECT_CACHE_TILE_BASE +
        selected * SOURCE_EFFECT_TILES_PER_SLOT
    );
}

static void source_effect_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_effect_upload_count; index++) {
        const SourceEffectUpload *upload =
            &source_effect_uploads[index];
        const u8 *source =
            obj_tiles +
            (
                OBJ_TILE_EXPLOSION +
                upload->frame_key *
                    OBJ_EXPLOSION_TILES_PER_FRAME
            ) * 32;
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            (
                SOURCE_EFFECT_CACHE_TILE_BASE +
                upload->slot * SOURCE_EFFECT_TILES_PER_SLOT
            ) * 32;

        dmaCopy(source, target, SOURCE_EFFECT_FRAME_BYTES);
        telemetry_effect_cache_uploads++;
        telemetry_effect_cache_upload_bytes +=
            SOURCE_EFFECT_FRAME_BYTES;
    }
    source_effect_upload_count = 0;
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
    u32 layer_passed_checks;
    u32 layer_failed_checks;

    memset(source_enemy_cache, 0, sizeof(source_enemy_cache));
    memset(source_enemy_uploads, 0, sizeof(source_enemy_uploads));
    memset(source_effect_cache, 0, sizeof(source_effect_cache));
    memset(source_effect_uploads, 0, sizeof(source_effect_uploads));
    source_enemy_cache_epoch = 0;
    source_enemy_upload_count = 0;
    source_effect_cache_epoch = 0;
    source_effect_upload_count = 0;
    if (!source_enemy_palette_setup(SOURCE_LEVEL_PALETTE_INDEX)) {
        source_parity_level.assets_valid = false;
    }
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
    telemetry_sprite2_uploads = 0;
    telemetry_sprite2_upload_bytes = 0;
    telemetry_sprite2_max_uploads = 0;
    telemetry_sprite2_max_visible_unique = 0;
    telemetry_effect_cache_hits = 0;
    telemetry_effect_cache_misses = 0;
    telemetry_effect_cache_evictions = 0;
    telemetry_effect_cache_drops = 0;
    telemetry_effect_cache_uploads = 0;
    telemetry_effect_cache_upload_bytes = 0;
    telemetry_effect_cache_max_uploads = 0;
    telemetry_effect_cache_max_visible_unique = 0;
}
