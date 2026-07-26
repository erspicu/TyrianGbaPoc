/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_game(void)
{
    u8 index;
    /*
     * Player, projectiles, pickups and explosions must remain above every
     * background.  Source enemy pools choose their own inter-layer priority
     * below; applying the old global foreground flag hid the player behind
     * BG2 as soon as source background3_over started at 1.
     */
    u8 object_priority = 0;
    u8 old_count = previous_oam_count;
    u8 visible_count;
    oam_count = 0;

    if (
        game_paused
#ifdef AUTOTEST_SCREENSHOT_REWARD
        && !autotest_screenshot_delay
#endif
    ) {
        render_pause_text();
    }
    put_sprite_with_attr1(
        player_x, player_y,
        player_bank ? OBJ_TILE_PLAYER_1 : OBJ_TILE_PLAYER_0,
        OBJ_PAL_PLAYER_0, ATTR1_SIZE_32, object_priority,
        player_bank < 0 ? OBJ_HFLIP : 0
    );
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) {
            put_sprite(
                source_player_shot_screen_x(&player_shots[index]),
                source_player_shot_screen_y(&player_shots[index]),
                OBJ_TILE_PLAYER_SHOT, OBJ_PAL_PLAYER_SHOT,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    if (game_state == STATE_PLAY) {
        /*
         * PC software blits later objects over earlier ones.  GBA resolves
         * equal-priority OBJ overlap in favour of the lower OAM index, so
         * emit the translated draw sequence in reverse:
         * top, sky, ground2, ground; and descending slot order per pool.
         */
        static const u8 source_pool_oam_order[4] = {
            50, 0, 75, 25
        };
        u8 visible_source_enemies = 0;
        u8 pool_order;

        source_enemy_cache_begin_frame();
        for (pool_order = 0; pool_order < 4; pool_order++) {
            s16 slot;

            for (slot = OT_ENEMY_POOL_SIZE - 1; slot >= 0; slot--) {
                const OtEnemyDrawCommand *draw;
                s16 screen_x;
                s16 screen_y;
                u16 tile;
                u8 palette;
                u8 priority;

                index = (u8)(source_pool_oam_order[pool_order] + slot);
                draw = &source_parity_level.enemy_draw[index];
                if (!draw->active) continue;
                screen_x = source_enemy_draw_screen_x(draw);
                screen_y = source_enemy_draw_screen_y(draw);
                if (
                    screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
                    screen_y <= -32 || screen_y >= SCREEN_HEIGHT
                ) {
                    continue;
                }
                visible_source_enemies++;
                tile = source_enemy_cache_acquire(draw, &palette);
                if (tile == 0xffff) continue;
                priority =
                    draw->pool >= 50 && draw->pool < 75 ? 0 :
                    (draw->pool < 25 ? 1 : 2);
                put_sprite(
                    screen_x,
                    screen_y,
                    tile,
                    palette,
                    ATTR1_SIZE_32,
                    priority
                );
            }
        }
        telemetry_source_visible_enemies = visible_source_enemies;
        if (
            visible_source_enemies >
            telemetry_source_max_visible_enemies
        ) {
            telemetry_source_max_visible_enemies =
                visible_source_enemies;
        }
    } else if (game_state == STATE_BOSS) {
        render_boss_bar();
        put_sprite(
            boss_x, boss_y, OBJ_TILE_BOSS_0, OBJ_PAL_BOSS_0,
            ATTR1_SIZE_64, object_priority
        );
    }
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) {
            put_sprite(
                rewards[index].x, rewards[index].y,
                OBJ_TILE_REWARD +
                    (
                        rewards[index].sequence * REWARD_FRAME_COUNT +
                        rewards[index].frame
                    ) * REWARD_TILES_PER_FRAME,
                OBJ_PAL_REWARD, ATTR1_SIZE_16, object_priority
            );
        }
    }
    if (game_state == STATE_PLAY) {
        for (index = 0; index < OT_ENEMY_SHOT_COUNT; index++) {
            const OtEnemyShot *source_shot =
                &source_parity_level.enemy_shot[index];
            EnemyShot presented;
            u16 graphic;

            if (!source_shot->active) continue;
            graphic = source_shot->sgr >= 500 ?
                (u16)(source_shot->sgr - 500) :
                source_shot->sgr;
            if (graphic > 255) continue;
            presented = (EnemyShot){
                .active = 1,
                .x = source_enemy_shot_screen_x(source_shot),
                .y = source_enemy_shot_screen_y(source_shot),
                .graphic = (u8)graphic,
                .animate = (u8)source_shot->animate,
            };
            render_enemy_shot(&presented, object_priority);
        }
    } else {
        for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
            if (enemy_shots[index].active) {
                render_enemy_shot(&enemy_shots[index], object_priority);
            }
        }
    }
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (effects[index].active) {
            put_sprite(
                effects[index].x, effects[index].y,
                OBJ_TILE_EXPLOSION +
                    (
                        effects[index].sequence * EXPLOSION_FRAME_COUNT +
                        effects[index].frame
                    ) * EXPLOSION_TILES_PER_FRAME,
                OBJ_PAL_EXPLOSION,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    render_cash_counter();
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
