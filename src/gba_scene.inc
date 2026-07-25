/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_game(void)
{
    u8 index;
    u8 object_priority = foreground_phase ? 1 : 0;
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
                player_shots[index].x, player_shots[index].y,
                OBJ_TILE_PLAYER_SHOT, OBJ_PAL_PLAYER_SHOT,
                ATTR1_SIZE_16, object_priority
            );
        }
    }
    if (game_state == STATE_PLAY) {
        for (index = 0; index < MAX_ENEMIES; index++) {
            if (enemies[index].active) {
                put_sprite(
                    enemies[index].x, enemies[index].y,
                    enemy_tiles[enemies[index].type],
                    enemy_palettes[enemies[index].type],
                    ATTR1_SIZE_32, object_priority
                );
            }
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
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        if (enemy_shots[index].active) {
            render_enemy_shot(&enemy_shots[index], object_priority);
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
