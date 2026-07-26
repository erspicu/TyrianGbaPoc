/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_player_sprite(u8 priority)
{
    put_sprite_with_attr1(
        source_player_screen_x(), source_player_screen_y(),
        player_bank ? OBJ_TILE_PLAYER_1 : OBJ_TILE_PLAYER_0,
        OBJ_PAL_PLAYER_0, ATTR1_SIZE_32, priority,
        player_bank < 0 ? OBJ_HFLIP : 0
    );
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

    for (index = MAX_EFFECTS - 1; index >= 0; index--) {
        u16 tile;

        if (!effects[index].active) continue;
        tile = source_effect_cache_acquire(
            effects[index].sequence,
            effects[index].frame
        );
        if (tile == 0xffff) continue;
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
        put_sprite_8bpp_32(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}

static void render_source_enemy_shots(u8 priority)
{
    s16 index;

    for (index = OT_ENEMY_SHOT_COUNT - 1; index >= 0; index--) {
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
        render_enemy_shot(&presented, priority);
    }
}

static void render_legacy_enemy_shots(u8 priority)
{
    s16 index;

    for (index = MAX_ENEMY_SHOTS - 1; index >= 0; index--) {
        if (enemy_shots[index].active) {
            render_enemy_shot(&enemy_shots[index], priority);
        }
    }
}

static void render_source_play_scene(void)
{
    u8 visible_source_enemies = 0;

    source_enemy_cache_begin_frame();

    /*
     * PC is a software renderer: the last blit is in front.  GBA resolves
     * equal-priority OBJ overlap in favour of the lower OAM index, so emit
     * the complete JE_main() sequence in reverse.
     */
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

    telemetry_source_visible_enemies = visible_source_enemies;
    if (
        visible_source_enemies >
        telemetry_source_max_visible_enemies
    ) {
        telemetry_source_max_visible_enemies =
            visible_source_enemies;
    }
}

static void render_boss_scene(void)
{
    render_boss_bar();
    render_effect_sprites(0);
    render_legacy_rewards(0);
    render_legacy_enemy_shots(0);
    render_player_sprite(0);
    render_player_shots(0);
    put_sprite(
        boss_x, boss_y, OBJ_TILE_BOSS_0, OBJ_PAL_BOSS_0,
        ATTR1_SIZE_64, 0
    );
}

static void render_clear_scene(void)
{
    render_effect_sprites(0);
    render_legacy_rewards(0);
    render_player_sprite(0);
    render_player_shots(0);
}

static void render_game(void)
{
    u8 old_count = previous_oam_count;
    u8 visible_count;

    source_effect_cache_begin_frame();
    oam_count = 0;
    if (
        game_paused
#ifdef AUTOTEST_SCREENSHOT_REWARD
        && !autotest_screenshot_delay
#endif
    ) {
        render_pause_text();
    }
    render_cash_counter();

    if (game_state == STATE_PLAY) {
        render_source_play_scene();
    } else if (game_state == STATE_BOSS) {
        render_boss_scene();
    } else if (game_state == STATE_CLEAR) {
        render_clear_scene();
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
