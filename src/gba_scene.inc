/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_player_at_source_y(
    s16 source_y,
    u8 priority,
    u8 exit_flight
)
{
    if (player_ship_graphic > 1) {
        u16 tile = source_player_cache_acquire(
            (u16)(
                player_ship_graphic +
                (exit_flight ? 0 : (s16)player_bank * 2)
            )
        );

        if (tile != 0xffff) {
            /*
             * JE_playerMovement() ordinary ship path is
             * blit_sprite2x2(x - 5, y - 7, spriteSheet9, ship_sprite).
             * The raw Sprite2 frame retains its native (4,2) component
             * placement, matching the established 32x32 player container.
             */
            put_sprite_8bpp_32(
                source_player_screen_x(),
                (s16)(
                    source_world_to_screen_y(
                        (s16)(
                            source_y +
                            SOURCE_PLAYER_PRESENTATION_CENTRE_Y
                        )
                    ) - 16
                ),
                tile,
                priority
            );
            return;
        }
    }

    /*
     * Source ship graphics 0 and 1 are special two-body cases.  The current
     * stock title demos use ordinary graphics; retain the validated USP
     * Talon presentation as a fail-closed fallback for those rare cases or
     * a corrupt/missing Sprite2 frame.
     */
    put_sprite_with_attr1(
        source_player_screen_x(),
        (s16)(
            source_world_to_screen_y(
                (s16)(
                    source_y +
                    SOURCE_PLAYER_PRESENTATION_CENTRE_Y
                )
            ) - 16
        ),
        exit_flight || player_bank == 0 ?
            OBJ_TILE_PLAYER_0 :
            OBJ_TILE_PLAYER_1,
        OBJ_PAL_PLAYER_0, ATTR1_SIZE_32, priority,
        !exit_flight && player_bank < 0 ? OBJ_HFLIP : 0
    );
}

static bool source_detail_shadows_allowed(void)
{
    return
        source_background2_enabled &&
        !source_detail_spotlight_requested &&
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
        /* Smooth wild BG1 Alpha has priority over local OBJ-window shadows. */
        !source_detail_wild_active &&
#endif
        gameplay_filter_brightness_level() == 0;
}

static void render_player_shadow_at_source_y(
    s16 source_y,
    u8 priority
)
{
    s16 screen_x = (s16)(
        source_player_screen_x() + 30 -
        (s16)source_parity_level.presentation_map_x2_offset
    );
    s16 screen_y = (s16)(
        source_world_to_screen_y(
            (s16)(source_y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y)
        ) - 16 + 10
    );

    if (!source_detail_shadows_allowed()) return;
    if (player_ship_graphic > 1) {
        u16 tile = source_player_cache_acquire(
            (u16)(
                player_ship_graphic +
                (s16)player_bank * 2
            )
        );

        if (tile != 0xffff) {
            put_sprite_8bpp_32_windowed(
                screen_x,
                screen_y,
                tile,
                priority
            );
            return;
        }
    }
    put_sprite_windowed_with_attr1(
        screen_x,
        screen_y,
        player_bank == 0 ?
            OBJ_TILE_PLAYER_0 :
            OBJ_TILE_PLAYER_1,
        OBJ_PAL_PLAYER_0,
        ATTR1_SIZE_32,
        priority,
        player_bank < 0 ? OBJ_HFLIP : 0
    );
}

static void render_player_sprite(u8 priority)
{
    s16 trail_positions[19];
    s16 trail_y;
    s16 delta;
    s8 render_index;
    u8 trail_count = 0;
    u8 trail_spacing;
    u8 trail_index;
    u8 num_trails;

    if (!player_alive) return;
    if (
        !source_parity_level.end_level ||
        source_parity_level.level_end == 0
    ) {
        render_player_shadow_at_source_y(player_source_y, priority);
        render_player_at_source_y(player_source_y, priority, 0);
        return;
    }

    /*
     * Direct JE_playerMovement() end-flight trail. The PC does not draw a
     * separate ordinary player sprite in endLevel; it draws up to nineteen
     * copies along this triangular-spacing history instead.
     */
    delta = (s16)(41 - source_parity_level.level_end);
    if (delta < 0) delta = (s16)-delta;
    num_trails = delta > 20 ? 20 : (u8)delta;
    trail_spacing = 1;
    trail_y = player_source_y;
    for (trail_index = 0; trail_index < num_trails; trail_index++) {
        trail_y = (s16)(trail_y + trail_spacing);
        trail_spacing++;
    }
    for (trail_index = 1; trail_index < num_trails; trail_index++) {
        trail_y = (s16)(trail_y - trail_spacing);
        trail_spacing--;
        if (trail_y > 0 && trail_y < 170) {
            trail_positions[trail_count++] = trail_y;
        }
    }
    /*
     * Later PC blits win. Lower GBA OAM indices win equal-priority overlap,
     * so emit the stored source sequence in reverse.
     */
    for (
        render_index = (s8)trail_count - 1;
        render_index >= 0;
        render_index--
    ) {
        render_player_at_source_y(
            trail_positions[(u8)render_index],
            priority,
            1
        );
    }
    if (trail_count > telemetry_end_level_trail_max) {
        telemetry_end_level_trail_max = trail_count;
    }
}

static void render_player_sidekicks(u8 priority)
{
    u8 side;

    if (!player_alive || source_parity_level.end_level) return;
#if TYRIAN_GBA_STRESS_LOADOUT
    if (!stress_loadout_valid) return;
#endif
    for (side = 0; side < 2; side++) {
#if TYRIAN_GBA_STRESS_LOADOUT
        const OtOptionDefinition *option =
            side == 0 ? &stress_left_option : &stress_right_option;
        u8 animation = stress_sidekick_animation[side];
        u8 charge = 0;
        s16 source_x = stress_sidekick_x[side];
        s16 source_y = stress_sidekick_y[side];
#else
        const SourceSidekickRuntime *runtime =
            &source_sidekick[side];
        const OtOptionDefinition *option = &runtime->option;
        u8 animation = runtime->animation_frame;
        u8 charge = runtime->charge;
        s16 source_x = runtime->x;
        s16 source_y = runtime->y;
#endif
        OtEnemyDrawCommand draw;
        u16 graphic;
        u16 tile;
        s16 screen_x;
        s16 screen_y;

        if (
#if !TYRIAN_GBA_STRESS_LOADOUT
            !runtime->valid ||
#endif
            option->option == 0 ||
            option->animation_count == 0 ||
            animation >= option->animation_count
        ) {
            continue;
        }
        graphic = (u16)(option->graphic[animation] + charge);
        draw = (OtEnemyDrawCommand){
            .active = true,
            .x = source_x,
            .y = source_y,
            .graphic = graphic,
            .shape_table =
                option->style == 1 || option->style == 2 ?
                    26 :
                    OT_COMP_SHAPE_TABLE_OPTIONS_SMALL,
            .size =
                option->style == 1 || option->style == 2 ?
                    1 :
                    0,
            .filter = 0,
            .pool = 0,
        };
        tile = source_enemy_cache_acquire(&draw);
        if (tile == 0xffff) continue;
        if (draw.size != 0) {
            /*
             * blit_sprite2x2(x - 6, y) places its first 12x14 component at
             * canvas (4,2); compensate that unchanged source layout.
             */
            screen_x = source_world_to_screen_x(
                (s16)(source_x - 10)
            );
            screen_y = source_world_to_screen_y(
                (s16)(source_y - 2)
            );
            put_sprite_8bpp_32(
                screen_x,
                screen_y,
                tile,
                priority
            );
        } else if (source_enemy_cache_tile_is_compact(tile)) {
            put_sprite_8bpp_16(
                source_world_to_screen_x(source_x),
                source_world_to_screen_y(source_y),
                tile,
                priority
            );
        } else {
            /*
             * A size-0 Sprite2 occupies (8,8)..(23,23) in a full L2 frame.
             */
            put_sprite_8bpp_32(
                source_world_to_screen_x(
                    (s16)(source_x - 8)
                ),
                source_world_to_screen_y(
                    (s16)(source_y - 8)
                ),
                tile,
                priority
            );
        }
    }
}

#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
static void render_player_shots(u8 priority, u8 oam_limit)
{
    s16 index;
    u8 submitted = 0;

    /* PC blits ascending slots; later slots must win the GBA OBJ tie. */
    for (index = MAX_PLAYER_SHOTS - 1; index >= 0; index--) {
        if (!player_shots[index].active) continue;
        if (
            submitted >= MAX_VISIBLE_PLAYER_SHOTS ||
            oam_count >= oam_limit
        ) {
            telemetry_player_shot_oam_culls++;
            break;
        }
        {
            u16 frame = player_shots[index].render_graphic;
            u8 shape_table;
            u16 graphic;
            u16 tile;
            s16 screen_x;
            s16 screen_y;

            if (frame > 60000) {
#if TYRIAN_GBA_STRESS_LOADOUT
                if (
                    frame !=
                        60001 +
                            SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
                    !source_option_projectile_valid
                ) {
                    telemetry_stress_loadout_failures++;
                    continue;
                }
                screen_x = source_player_shot_screen_x(
                    &player_shots[index]
                );
                screen_y = source_player_shot_screen_y(
                    &player_shots[index]
                );
                if (
                    screen_x <= -64 || screen_x >= SCREEN_WIDTH ||
                    screen_y <= -64 || screen_y >= SCREEN_HEIGHT
                ) {
                    continue;
                }
                put_sprite_8bpp_64_blended(
                    screen_x,
                    screen_y,
                    SOURCE_OPTION_PROJECTILE_TILE_BASE,
                    priority
                );
                telemetry_stress_option_blend_draws++;
                submitted++;
#else
                u16 option_graphic = (u16)(frame - 60001u);
                u8 option_frame;

                if (
                    option_graphic <
                        SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
                    option_graphic >=
                        SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC +
                            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
                ) {
                    continue;
                }
                option_frame = (u8)(
                    option_graphic -
                    SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
                );
                if (
                    (
                        source_option_projectile_valid &
                        (1u << option_frame)
                    ) == 0
                ) {
                    continue;
                }
                screen_x = source_player_shot_screen_x(
                    &player_shots[index]
                );
                screen_y = source_player_shot_screen_y(
                    &player_shots[index]
                );
                if (
                    screen_x <= -64 || screen_x >= SCREEN_WIDTH ||
                    screen_y <= -32 || screen_y >= SCREEN_HEIGHT
                ) {
                    continue;
                }
                put_sprite_4bpp_64x32_blended(
                    screen_x,
                    screen_y,
                    (u16)(
                        SOURCE_OPTION_PROJECTILE_TILE_BASE +
                        option_frame *
                            SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME
                    ),
                    priority
                );
                submitted++;
#endif
                continue;
            }
            if (frame > 1000) frame %= 1000;
            shape_table =
                frame > 500 ?
                    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY :
                    OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
            graphic = frame > 500 ? (u16)(frame - 500) : frame;
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
            screen_x = source_player_shot_screen_x(
                &player_shots[index]
            );
            screen_y = source_player_shot_screen_y(
                &player_shots[index]
            );
            if (
                screen_x <= -16 || screen_x >= SCREEN_WIDTH ||
                screen_y <= -16 || screen_y >= SCREEN_HEIGHT
            ) {
#if TYRIAN_GBA_STRESS_LOADOUT
                telemetry_projectile_culled_offscreen_before_cache++;
#endif
                continue;
            }
            tile = source_projectile_cache_acquire(
                shape_table,
                graphic,
                1
            );
#else
            tile = source_projectile_cache_acquire(
                shape_table,
                graphic,
                0
            );
            screen_x = source_player_shot_screen_x(
                &player_shots[index]
            );
            screen_y = source_player_shot_screen_y(
                &player_shots[index]
            );
#endif
            if (tile == 0xffff) continue;
            put_sprite_8bpp_16(
                screen_x,
                screen_y,
                tile,
                priority
            );
            if (
                source_detail_shadows_allowed() &&
                player_shots[index].y + 10 < 190 &&
                player_shots[index].xm < 100 &&
                oam_count < oam_limit
            ) {
                put_sprite_8bpp_16_windowed(
                    screen_x,
                    (s16)(screen_y + 10),
                    tile,
                    priority
                );
            }
            submitted++;
        }
    }
}
#endif

static void render_effect_sprites(u8 priority, u8 oam_limit)
{
    s16 index;
    u8 submitted = 0;

    for (index = (s16)effect_slot_high_water - 1; index >= 0; index--) {
        u16 tile;

        if (!effects[index].active) continue;
        if (
            submitted >= MAX_VISIBLE_EFFECTS ||
            oam_count >= oam_limit
        ) {
            telemetry_effect_oam_culls++;
            break;
        }
        if (
            effects[index].x <= -16 ||
            effects[index].x >= SCREEN_WIDTH ||
            effects[index].y <= -16 ||
            effects[index].y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        /*
         * Keep OpenTyrian's full 200-entry allocator and lifecycle, then
         * apply a presentation-only GBA budget. Newer (later PC blits)
         * survive first, while the 128-entry hardware OAM remains available
         * for enemies, projectiles, HUD and the GAME OVER overlay.
         */
        tile = source_effect_cache_acquire(
            effects[index].sequence,
            effects[index].frame
        );
        if (tile == 0xffff) continue;
        submitted++;
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
        put_sprite(
            effects[index].x, effects[index].y,
            tile,
            OBJ_PAL_EXPLOSION,
            ATTR1_SIZE_16, priority
        );
#else
        put_sprite_blended(
            effects[index].x, effects[index].y,
            tile,
            OBJ_PAL_EXPLOSION,
            ATTR1_SIZE_16, priority
        );
#endif
    }
}

static void render_legacy_rewards(u8 priority)
{
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    s16 index;

    for (index = MAX_REWARDS - 1; index >= 0; index--) {
        if (!rewards[index].active) continue;
        put_sprite(
            rewards[index].x, rewards[index].y,
            OBJ_TILE_REWARD +
                (
                    rewards[index].sequence * REWARD_FRAME_COUNT +
                    rewards[index].frame
                ) * REWARD_TILES_PER_FRAME,
            OBJ_PAL_REWARD, ATTR1_SIZE_16, priority
        );
    }
#else
    (void)priority;
#endif
}

static void render_source_enemy_cached_sprite(
    s16 screen_x,
    s16 screen_y,
    u16 tile,
    u8 priority
)
{
    if (source_enemy_cache_tile_is_split(tile)) {
#if !TYRIAN_GBA_STRESS_LOADOUT
        /* A fragmented canvas is one scene object: submit both halves or 0. */
        if (oam_count + 2u > SPRITE_LIMIT) return;
        put_sprite_8bpp_32x16(
            screen_x,
            screen_y,
            SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE,
            priority
        );
        put_sprite_8bpp_32x16(
            screen_x,
            (s16)(screen_y + 16),
            SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE,
            priority
        );
#endif
    } else if (source_enemy_cache_tile_is_compact(tile)) {
        put_sprite_8bpp_16(
            screen_x + 8,
            screen_y + 8,
            tile,
            priority
        );
    } else {
        put_sprite_8bpp_32(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW
static void render_source_enemy_cached_sprite_blended(
    s16 screen_x,
    s16 screen_y,
    u16 tile,
    u8 priority
)
{
    if (source_enemy_cache_tile_is_split(tile)) {
#if !TYRIAN_GBA_STRESS_LOADOUT
        if (oam_count + 2u > SPRITE_LIMIT) return;
        put_sprite_8bpp_32x16_blended(
            screen_x,
            screen_y,
            SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE,
            priority
        );
        put_sprite_8bpp_32x16_blended(
            screen_x,
            (s16)(screen_y + 16),
            SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE,
            priority
        );
#endif
    } else if (source_enemy_cache_tile_is_compact(tile)) {
        put_sprite_8bpp_16_blended(
            screen_x + 8,
            screen_y + 8,
            tile,
            priority
        );
    } else {
        put_sprite_8bpp_32_blended(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}
#endif

static void render_source_pickup_explosions(u8 priority, u8 oam_limit)
{
    s16 index;

    for (index = MAX_PICKUP_EXPLOSIONS - 1; index >= 0; index--) {
        const PickupExplosion *explosion =
            &pickup_explosions[index];
        OtEnemyDrawCommand draw;
        u16 tile;
        s16 screen_x;
        s16 screen_y;
        u8 oam_required;

        if (!explosion->active) continue;
        if (oam_count >= oam_limit) {
            telemetry_effect_oam_culls++;
            break;
        }
        draw = (OtEnemyDrawCommand){
            .active = true,
            .x = explosion->x,
            .y = explosion->y,
            .graphic = (u16)(explosion->graphic + 1),
            .shape_table = OT_COMP_SHAPE_TABLE_EXPLOSION,
            .size = 0,
            .filter = 0,
            .pool = 0,
        };
        screen_x = source_enemy_draw_screen_x(&draw);
        screen_y = source_enemy_draw_screen_y(&draw);
        if (
            screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
            screen_y <= -32 || screen_y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        tile = source_enemy_cache_acquire(&draw);
        if (tile == 0xffff) continue;
        oam_required = source_enemy_cache_tile_is_split(tile) ? 2u : 1u;
        if (oam_count + oam_required > oam_limit) {
            telemetry_effect_oam_culls++;
            continue;
        }
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
        render_source_enemy_cached_sprite(
            screen_x,
            screen_y,
            tile,
            priority
        );
#else
        render_source_enemy_cached_sprite_blended(
            screen_x,
            screen_y,
            tile,
            priority
        );
#endif
    }
}

static void render_source_enemy_pool(
    u8 pool,
    u8 draw_stage,
    u8 *visible_source_enemies
)
{
    s16 slot;
    u8 priority = source_object_priority(draw_stage);

    /*
     * JE_drawEnemy() blits ascending slots.  Lower GBA OAM indices win an
     * equal-priority OBJ tie, so consume the source pool in reverse.
     */
    for (slot = OT_ENEMY_POOL_SIZE - 1; slot >= 0; slot--) {
        u8 index = (u8)(pool + slot);
        const OtEnemyDrawCommand *draw =
            &source_parity_level.enemy_draw[index];
        s16 screen_x;
        s16 screen_y;
        u16 tile;

        if (!draw->active) continue;
        screen_x = source_enemy_draw_screen_x(draw);
        screen_y = source_enemy_draw_screen_y(draw);
        if (
            screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
            screen_y <= -32 || screen_y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        (*visible_source_enemies)++;
        tile = source_enemy_cache_acquire(draw);
        if (tile == 0xffff) continue;
        render_source_enemy_cached_sprite(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}

static void render_source_enemy_shots(u8 priority, u8 oam_limit)
{
    s16 index;
    u8 submitted = 0;

    for (index = OT_ENEMY_SHOT_COUNT - 1; index >= 0; index--) {
        const OtEnemyShot *source_shot =
            &source_parity_level.enemy_shot[index];
        u8 shape_table;
        u16 graphic;
        u16 tile;
        s16 screen_x;
        s16 screen_y;

        if (!source_shot->active) continue;
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
        if (
            submitted >= MAX_VISIBLE_ENEMY_SHOTS ||
            oam_count >= oam_limit
        ) {
            telemetry_enemy_shot_oam_culls++;
#if TYRIAN_GBA_STRESS_LOADOUT
            telemetry_projectile_culled_oam_full_before_cache++;
#endif
            break;
        }
        screen_x = source_enemy_shot_screen_x(source_shot);
        screen_y = source_enemy_shot_screen_y(source_shot);
        if (
            screen_x <= -16 || screen_x >= SCREEN_WIDTH ||
            screen_y <= -16 || screen_y >= SCREEN_HEIGHT
        ) {
#if TYRIAN_GBA_STRESS_LOADOUT
            telemetry_projectile_culled_offscreen_before_cache++;
#endif
            continue;
        }
#endif
        shape_table =
            source_shot->sgr >= 500 ?
                OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY :
                OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
        graphic = (u16)(
            (
                source_shot->sgr >= 500 ?
                    source_shot->sgr - 500 :
                    source_shot->sgr
            ) + source_shot->animate
        );
        tile = source_projectile_cache_acquire(
            shape_table,
            graphic,
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
            1
#else
            0
#endif
        );
        if (tile == 0xffff) continue;
#if !TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
        screen_x = source_enemy_shot_screen_x(source_shot);
        screen_y = source_enemy_shot_screen_y(source_shot);
#endif
        put_sprite_8bpp_16(
            screen_x,
            screen_y,
            tile,
            priority
        );
        submitted++;
    }
}

/*
 * Count the OAM entries that must survive presentation pressure.  The PC
 * renderer has no object-count ceiling, while GBA has exactly 128 entries.
 * A streamed Sprite2 frame can occupy two 32x16 entries when it lands in the
 * fragmented cache slot, so reserve two entries per visible authored object.
 * This is deliberately conservative: unused reserve becomes headroom, never
 * a missing player, enemy, or Boss component.
 */
static u8 source_structural_oam_required(void)
{
    u16 required = 0;
    u8 index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemyDrawCommand *draw =
            &source_parity_level.enemy_draw[index];
        s16 screen_x;
        s16 screen_y;

        if (!draw->active) continue;
        screen_x = source_enemy_draw_screen_x(draw);
        screen_y = source_enemy_draw_screen_y(draw);
        if (
            screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
            screen_y <= -32 || screen_y >= SCREEN_HEIGHT
        ) {
            continue;
        }
#if TYRIAN_GBA_STRESS_LOADOUT
        required++;
#else
        required += 2u;
#endif
    }

    if (player_alive) {
        if (
            source_parity_level.end_level &&
            source_parity_level.level_end != 0
        ) {
            /* JE_playerMovement() can emit at most nineteen exit copies. */
            required += 19u;
        } else {
            required++;
            if (source_detail_shadows_allowed()) required++;
#if TYRIAN_GBA_STRESS_LOADOUT
            if (stress_loadout_valid) required += 2u;
#else
            for (index = 0; index < 2; index++) {
                const SourceSidekickRuntime *runtime =
                    &source_sidekick[index];

                if (
                    runtime->valid &&
                    runtime->option.option != 0 &&
                    runtime->option.animation_count != 0 &&
                    runtime->animation_frame <
                        runtime->option.animation_count
                ) {
                    required += 2u;
                }
            }
#endif
        }
    }

    if (required > SPRITE_LIMIT) required = SPRITE_LIMIT;
    if (required > telemetry_structural_oam_required_max) {
        telemetry_structural_oam_required_max = required;
    }
    return (u8)required;
}

static u8 source_detail_oam_height(u16 attr0, u16 attr1)
{
    static const u8 heights[3][4] = {
        { 8, 16, 32, 64 },
        { 8,  8, 16, 32 },
        {16, 32, 32, 64 },
    };
    u8 shape = (u8)((attr0 >> 14) & 3u);
    u8 size = (u8)((attr1 >> 14) & 3u);

    return shape < 3 ? heights[shape][size] : 8;
}

static void source_detail_transform_world_oam(u8 first, u8 last)
{
    u8 index;

    if (!source_detail_vertical_flip) return;
    for (index = first; index < last; index++) {
        OBJATTR *object = &oam_shadow[index];
        s16 y = (s16)(object->attr0 & 0xffu);
        u8 height = source_detail_oam_height(
            object->attr0,
            object->attr1
        );

        if (y >= SCREEN_HEIGHT) y = (s16)(y - 256);
        y = (s16)(SCREEN_HEIGHT - y - height);
        object->attr0 = (u16)(
            (object->attr0 & ~0xffu) | (y & 0xffu)
        );
        object->attr1 ^= OBJ_VFLIP;
    }
}

static void render_source_play_scene(void)
{
    u8 visible_source_enemies = 0;
    u8 nonstructural_oam_limit = SPRITE_LIMIT;
    u8 effect_oam_limit;

    source_sprite2_l2_begin_frame();
    source_enemy_cache_begin_frame();
    source_projectile_cache_begin_frame();
    render_source_boss_bars();
#if !TYRIAN_GBA_STRESS_LOADOUT
    {
        u8 structural_oam = source_structural_oam_required();

        nonstructural_oam_limit =
            structural_oam >= SPRITE_LIMIT - oam_count ?
                oam_count :
                (u8)(SPRITE_LIMIT - structural_oam);
    }
#else
    (void)source_structural_oam_required();
#endif

    /*
     * PC is a software renderer: the last blit is in front.  GBA resolves
     * equal-priority OBJ overlap in favour of the lower OAM index, so emit
     * the complete JE_main() sequence in reverse.
     */
    render_source_pickup_explosions(
        source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
        nonstructural_oam_limit
    );
    effect_oam_limit = nonstructural_oam_limit;
#if !TYRIAN_GBA_STRESS_LOADOUT
    {
        u8 available = source_effect_cache_available_slots();
        u8 budget = available < MAX_VISIBLE_EFFECTS ?
            available : MAX_VISIBLE_EFFECTS;

        if (budget < effect_oam_limit - oam_count) {
            effect_oam_limit = (u8)(oam_count + budget);
        }
    }
#endif
    render_effect_sprites(
        source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
        effect_oam_limit
    );
    render_legacy_rewards(
        source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
    );
    if (source_parity_level.sky_enemy_over_all) {
        render_source_enemy_pool(
            0,
            SOURCE_DRAW_STAGE_SKY_ENEMY_OVER,
            &visible_source_enemies
        );
    }
    if (source_parity_level.top_enemy_over) {
        render_source_enemy_pool(
            50,
            SOURCE_DRAW_STAGE_TOP_ENEMY_OVER,
            &visible_source_enemies
        );
    }
    render_source_enemy_shots(
        source_object_priority(SOURCE_DRAW_STAGE_ENEMY_SHOT),
        nonstructural_oam_limit
    );
    render_player_sidekicks(
        source_object_priority(SOURCE_DRAW_STAGE_PLAYER)
    );
    render_player_sprite(
        source_object_priority(SOURCE_DRAW_STAGE_PLAYER)
    );
#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
    render_player_shots(
        source_object_priority(SOURCE_DRAW_STAGE_PLAYER_SHOT),
        nonstructural_oam_limit
    );
#endif
    if (!source_parity_level.top_enemy_over) {
        render_source_enemy_pool(
            50,
            SOURCE_DRAW_STAGE_TOP_ENEMY,
            &visible_source_enemies
        );
    }
    if (!source_parity_level.sky_enemy_over_all) {
        render_source_enemy_pool(
            0,
            SOURCE_DRAW_STAGE_SKY_ENEMY,
            &visible_source_enemies
        );
    }
    /* PC draws ground2 after ground, hence reverse OAM order is 75 then 25. */
    render_source_enemy_pool(
        75,
        SOURCE_DRAW_STAGE_GROUND_ENEMY,
        &visible_source_enemies
    );
    render_source_enemy_pool(
        25,
        SOURCE_DRAW_STAGE_GROUND_ENEMY,
        &visible_source_enemies
    );
    source_enemy_cache_end_frame();
    source_projectile_cache_end_frame();
    source_sprite2_l2_end_frame();

    telemetry_source_visible_enemies = visible_source_enemies;
    if (
        visible_source_enemies >
        telemetry_source_max_visible_enemies
    ) {
        telemetry_source_max_visible_enemies =
            visible_source_enemies;
    }
}

static void render_game(void)
{
    u8 old_count = previous_oam_count;
    u8 visible_count;
    u8 world_oam_start;

    source_detail_prepare_scene();

    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        gameplay_overlay_prepare_frame();
    } else {
        gameplay_overlay_active = 0;
        gameplay_overlay_upload_pending = 0;
    }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
    /*
     * This render will replace the held scene at the next VBlank.  Its row
     * DMA and VOFS commit together, so old display-only cache references may
     * be released while constructing the newest coalesced window.
     */
    presentation_release_held_window = 1;
#endif
    (void)background_schedule_presentation_window(
        source_camera_offset_y
    );
    source_effect_cache_begin_frame();
    oam_count = 0;
    if (game_state == STATE_GAME_OVER) {
        /*
         * PC draws GAME OVER after the complete gameplay scene. Lower GBA
         * OAM indices win equal-priority ties, so reserve its front-most
         * entries before emitting the translated world in reverse order.
         */
        render_game_over_text();
    }
    if (
        game_state == STATE_PLAY &&
        source_parity_level.display_time > 0
    ) {
        render_secret_level_text();
    }
    if (
        game_state == STATE_PLAY &&
        frontend_demo_active &&
        source_parity_level.display_time == 0
    ) {
        render_insert_coin_text();
    }
    if (
        game_paused
#ifdef AUTOTEST_SCREENSHOT_REWARD
        && !autotest_screenshot_delay
#endif
    ) {
        render_pause_text();
    }
    render_cash_counter();
    render_player_energy_status();
    world_oam_start = oam_count;

    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        render_source_play_scene();
    }
    source_detail_transform_world_oam(world_oam_start, oam_count);
    source_effect_cache_end_frame();

    visible_count = oam_count;
    while (oam_count < old_count) {
        oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
        oam_count++;
    }
    previous_oam_count = visible_count;
    if (visible_count > telemetry_max_oam) {
        telemetry_max_oam = visible_count;
    }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    gameplay_presentation_capture_registers();
#endif
    oam_dirty = 1;
}
