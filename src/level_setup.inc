/* GBA level entry/exit and resource setup; legacy v11 baseline. */

static void clear_entities(void)
{
    u16 index;
    for (index = 0; index < MAX_ENEMIES; index++) enemies[index].active = 0;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shots[index].active = 0;
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shots[index].active = 0;
    }
    for (index = 0; index < MAX_EFFECTS; index++) effects[index].active = 0;
    for (index = 0; index < MAX_REWARDS; index++) rewards[index].active = 0;
    active_effect_count = 0;
    active_reward_count = 0;
}

static const u8 *map_row(const u8 *map, u16 row)
{
    return map + (u32)row * MAP_ROW_BYTES;
}

static void load_ring(const u8 *map, u16 rows, u16 top_row, u16 screen_block)
{
    u16 row;
    u16 first = top_row > 32 ? top_row - 32 : 0;
    u16 last = first + MAP_RING_ROWS;
    u16 *ring = (u16 *)SCREEN_BASE_BLOCK(screen_block);

    if (last > rows) {
        last = rows;
        first = last - MAP_RING_ROWS;
    }
    for (row = first; row < last; row++) {
        dmaCopy(map_row(map, row), ring + ((row & 63) * 32), MAP_ROW_BYTES);
    }
}

static void enter_title(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    dmaCopy(title_bitmap, MODE3_FB, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    REG_DISPCNT = MODE_3 | BG2_ON;
    audio_load_music(MOD_TYRIAN_TITLE_FULL);
    game_paused = 0;
    game_state = STATE_TITLE;
    telemetry_state_transitions++;
}

static void enter_level(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    clear_entities();
    hide_all_sprites();

    dmaCopy(bg1_tiles, CHAR_BASE_BLOCK(0), 512 * 32);
    dmaCopy(bg2_tiles, CHAR_BASE_BLOCK(1), 512 * 32);
    dmaCopy(bg3_tiles, CHAR_BASE_BLOCK(2), 512 * 32);
    dmaCopy(bg_palette, BG_PALETTE, 512);
    dmaCopy(obj_tiles, OBJ_BASE_ADR, OBJ_TILE_COUNT * 32);
    dmaCopy(obj_palette, SPRITE_PALETTE, 512);

    REG_BG0CNT = BG_PRIORITY(3) | CHAR_BASE(0) |
                 SCREEN_BASE(BG0_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG1CNT = BG_PRIORITY(2) | CHAR_BASE(1) |
                 SCREEN_BASE(BG1_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG2CNT = BG_PRIORITY(1) | CHAR_BASE(2) |
                 SCREEN_BASE(BG2_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;

    bg1_scroll_pixel = (BG1_ROWS - 20) * 8;
    bg2_scroll_pixel = (BG2_ROWS - 20) * 8;
    bg3_scroll_pixel = (BG3_ROWS - 20) * 8;
    load_ring(bg1_map, BG1_ROWS, bg1_scroll_pixel >> 3, BG0_SCREEN_BLOCK);
    load_ring(bg2_map, BG2_ROWS, bg2_scroll_pixel >> 3, BG1_SCREEN_BLOCK);
    load_ring(bg3_map, BG3_ROWS, bg3_scroll_pixel >> 3, BG2_SCREEN_BLOCK);
    /* First-level event 0 selects the original 1:2:0 opening motion. */
    bg1_scroll_speed = 1;
    bg2_scroll_speed = 2;
    bg3_scroll_speed = 0;
    bg1_scroll_delay = 1;
    bg2_scroll_delay = 1;
    bg1_scroll_delay_max = 1;
    bg2_scroll_delay_max = 1;

    player_x = 104;
    player_y = 124;
    player_invulnerable = 0;
    fire_cooldown = 0;
    player_bank = 0;
    player_cash = 0;
    game_paused = 0;
#ifdef AUTOTEST_CASH_VISUAL_TEST
    player_cash = 12345;
#endif
    level_tick = 0;
    level_position = 0;
    event_offset = 0;
    event_time = level_events[0];
    foreground_phase = 0;
    logic_accumulator = 0;
    ot_level_port_init(&source_parity_level);

    telemetry_display_frames = 0;
    telemetry_logic_updates = 0;
    telemetry_spawn_count = 0;
    telemetry_control_count = 0;
    telemetry_collision_count = 0;
    telemetry_map_rows = 0;
    telemetry_missed_vblanks = 0;
    telemetry_stream_drops = 0;
    telemetry_max_enemies = 0;
    telemetry_max_oam = 0;
    telemetry_max_effects = 0;
    telemetry_effect_drops = 0;
    telemetry_reward_spawns = 0;
    telemetry_reward_pickups = 0;
    telemetry_max_rewards = 0;
    telemetry_reward_drops = 0;
    telemetry_enemy_shots_spawned = 0;
    telemetry_enemy_shot_drops = 0;
    telemetry_max_enemy_shots = 0;
    telemetry_enemy_replacements = 0;
    telemetry_kill_cash = 0;
    telemetry_reward_controls = 0;
    telemetry_reward_assignments = 0;
    telemetry_pause_toggles = 0;
    telemetry_paused_frames = 0;
    telemetry_source_events = 0;
    telemetry_source_events_applied = 0;
    telemetry_source_events_deferred = 0;
    last_vblank_seen = telemetry_vblank_irqs;

    REG_BG0HOFS = 8;
    REG_BG1HOFS = 8;
    REG_BG2HOFS = 8;
    REG_BG0VOFS = bg1_scroll_pixel & 511;
    REG_BG1VOFS = bg2_scroll_pixel & 511;
    REG_BG2VOFS = bg3_scroll_pixel & 511;
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    REG_DISPCNT = MODE_0 | BG0_ON | BG1_ON | BG2_ON |
                  OBJ_ON | OBJ_1D_MAP;
    audio_load_music(MOD_TYRIAN_LEVEL_FULL);
    game_state = STATE_PLAY;
    telemetry_state_transitions++;
}
