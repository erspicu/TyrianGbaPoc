/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_player_at_source_y(
    s16 source_y,
    u8 priority,
    u8 exit_flight
)
{
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
        exit_flight || !player_bank ?
            OBJ_TILE_PLAYER_0 :
            OBJ_TILE_PLAYER_1,
        OBJ_PAL_PLAYER_0, ATTR1_SIZE_32, priority,
        !exit_flight && player_bank < 0 ? OBJ_HFLIP : 0
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

static void render_player_shots(u8 priority)
{
    s16 index;

    /* PC blits ascending slots; later slots must win the GBA OBJ tie. */
    for (index = MAX_PLAYER_SHOTS - 1; index >= 0; index--) {
        if (!player_shots[index].active) continue;
        put_sprite(
            source_player_shot_screen_x(&player_shots[index]),
            source_player_shot_screen_y(&player_shots[index]),
            OBJ_TILE_PLAYER_SHOT, OBJ_PAL_PLAYER_SHOT,
            ATTR1_SIZE_16, priority
        );
    }
}

static void render_effect_sprites(u8 priority)
{
    s16 index;
    u8 submitted = 0;

    for (index = (s16)effect_slot_high_water - 1; index >= 0; index--) {
        u16 tile;

        if (!effects[index].active) continue;
        /*
         * Keep OpenTyrian's full 200-entry allocator and lifecycle, then
         * apply a presentation-only GBA budget. Newer (later PC blits)
         * survive first, while the 128-entry hardware OAM remains available
         * for enemies, projectiles, HUD and the GAME OVER overlay.
         */
        if (submitted >= MAX_VISIBLE_EFFECTS) continue;
        tile = source_effect_cache_acquire(
            effects[index].sequence,
            effects[index].frame
        );
        if (tile == 0xffff) continue;
        submitted++;
        put_sprite(
            effects[index].x, effects[index].y,
            tile,
            OBJ_PAL_EXPLOSION,
            ATTR1_SIZE_16, priority
        );
    }
}

static void render_legacy_rewards(u8 priority)
{
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
}

static void render_source_pickup_explosions(u8 priority)
{
    s16 index;

    for (index = MAX_PICKUP_EXPLOSIONS - 1; index >= 0; index--) {
        const PickupExplosion *explosion =
            &pickup_explosions[index];
        OtEnemyDrawCommand draw;
        u16 tile;

        if (!explosion->active) continue;
        draw = (OtEnemyDrawCommand){
            .active = true,
            .x = explosion->x,
            .y = explosion->y,
            .graphic = explosion->graphic,
            .shape_table = OT_COMP_SHAPE_TABLE_EXPLOSION,
            .size = 0,
            .filter = 0,
            .pool = 0,
        };
        tile = source_enemy_cache_acquire(&draw);
        if (tile == 0xffff) continue;
        if (source_enemy_cache_tile_is_compact(tile)) {
            put_sprite_8bpp_16(
                source_enemy_draw_screen_x(&draw) + 8,
                source_enemy_draw_screen_y(&draw) + 8,
                tile,
                priority
            );
        } else {
            put_sprite_8bpp_32(
                source_enemy_draw_screen_x(&draw),
                source_enemy_draw_screen_y(&draw),
                tile,
                priority
            );
        }
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
        if (source_enemy_cache_tile_is_compact(tile)) {
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
}

static void render_source_enemy_shots(u8 priority)
{
    s16 index;

    for (index = OT_ENEMY_SHOT_COUNT - 1; index >= 0; index--) {
        const OtEnemyShot *source_shot =
            &source_parity_level.enemy_shot[index];
        u8 shape_table;
        u16 graphic;
        u16 tile;

        if (!source_shot->active) continue;
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
            graphic
        );
        if (tile == 0xffff) continue;
        put_sprite_8bpp_16(
            source_enemy_shot_screen_x(source_shot),
            source_enemy_shot_screen_y(source_shot),
            tile,
            priority
        );
    }
}

static void render_source_play_scene(void)
{
    u8 visible_source_enemies = 0;

    source_sprite2_l2_begin_frame();
    source_enemy_cache_begin_frame();
    source_projectile_cache_begin_frame();
    render_source_boss_bars();

    /*
     * PC is a software renderer: the last blit is in front.  GBA resolves
     * equal-priority OBJ overlap in favour of the lower OAM index, so emit
     * the complete JE_main() sequence in reverse.
     */
    render_source_pickup_explosions(
        source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
    );
    render_effect_sprites(
        source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
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
        source_object_priority(SOURCE_DRAW_STAGE_ENEMY_SHOT)
    );
    render_player_sprite(
        source_object_priority(SOURCE_DRAW_STAGE_PLAYER)
    );
    render_player_shots(
        source_object_priority(SOURCE_DRAW_STAGE_PLAYER_SHOT)
    );
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
        game_paused
#ifdef AUTOTEST_SCREENSHOT_REWARD
        && !autotest_screenshot_delay
#endif
    ) {
        render_pause_text();
    }
    render_cash_counter();

    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        render_source_play_scene();
    }
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
    oam_dirty = 1;
}
