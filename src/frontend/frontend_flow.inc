static const char *frontend_stats_text(u8 index, const char *fallback)
{
    if (
        frontend_text_ready &&
        index < 68 &&
        frontend_text.misc_text[index][0] != '\0'
    ) {
        return frontend_text.misc_text[index];
    }
    return fallback;
}

static u8 frontend_stats_adjust_brightness(u8 raw, s8 brightness)
{
    s16 value = (s16)raw + brightness;

    if (value < 0) return 0;
    if (value > 15) return value >= 31 ? 0 : 15;
    return (u8)value;
}

static void frontend_stats_fill_palette(u16 *target, s8 brightness)
{
    OtDataView palette;
    u8 raw;

    memset(target, 0, 16u * sizeof(u16));
    if (!ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        return;
    }
    for (raw = 3; raw <= 15; raw++) {
        u8 value = frontend_stats_adjust_brightness(
            raw,
            brightness
        );
        u8 source_index = (u8)(0xf0u | value);

        /*
         * JE_endLevelAni calls set_colors(white, 254, 254) before any glow.
         */
        target[raw - 2u] =
            source_index == 254 ?
                0x7fffu :
                title_palette_colour(&palette, source_index);
    }
    target[14] = title_palette_colour(&palette, 3);
    target[15] = 0x7fffu;
}

static void frontend_stats_refresh_palettes(void)
{
    OtDataView palette;
    u8 value;

    memset(frontend_stats_obj_palette, 0, 16u * sizeof(u16));
    if (ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        /*
         * JE_drawCube uses hue 9.  OBJ colour zero remains transparent;
         * entry 1 doubles as the two dark shadow passes and the visually
         * indistinguishable lowest foreground value in the 4bpp adapter.
         */
        frontend_stats_obj_palette[1] =
            title_palette_colour(&palette, 0);
        for (value = 2; value <= 15; value++) {
            frontend_stats_obj_palette[value] =
                title_palette_colour(
                    &palette,
                    (u8)(0x90u | value)
                );
        }
    }
    frontend_stats_fill_palette(
        frontend_stats_obj_palette + 16,
        frontend_stats_glow_brightness
    );
    frontend_stats_fill_palette(
        frontend_stats_obj_palette + 32,
        -4
    );
    frontend_stats_palette_dirty = 1;
}

static void frontend_stats_restart_glow(void)
{
    frontend_stats_glow_brightness = 2;
    frontend_stats_refresh_palettes();
}

static void frontend_stats_advance_glow(void)
{
    if (
        frontend_stats_timer % 5u == 0 &&
        frontend_stats_glow_brightness > -4
    ) {
        frontend_stats_glow_brightness--;
        frontend_stats_refresh_palettes();
    }
}

static u8 frontend_stats_prepare_tiles(void)
{
    u8 *tiles = frontend_stats_tile_scratch();

    /*
     * Glyph RLE, the source-equivalent +/-1 outline and cube packing are
     * immutable.  Copy the verified build-time OBJ catalog; only the glow
     * palettes remain dynamic for JE_outTextGlow parity.
     */
    memcpy(
        tiles,
        frontend_stats_tiles,
        FRONTEND_STATS_TILE_BYTES
    );
    memcpy(
        frontend_stats_glyph_width,
        frontend_stats_widths,
        FRONTEND_STATS_FONT_GLYPH_COUNT
    );
    frontend_stats_restart_glow();
    frontend_stats_tiles_pending = 1;
    frontend_stats_font_ready = 1;
    return 1;
}

static void frontend_stats_put_sprite(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    s16 dimension
)
{
    OBJATTR *object;

    if (oam_count >= HARDWARE_OAM_ENTRIES) return;
    if (
        x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(0) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static s16 frontend_stats_draw_text(
    s16 x,
    s16 y,
    const char *text,
    u8 palette
)
{
    while (*text != '\0') {
        u8 tile = frontend_runtime_font_tile(*text++);

        if (tile == 0) {
            x = (s16)(x + 6);
            continue;
        }
        tile--;
        frontend_stats_put_sprite(
            (s16)(x - 1),
            (s16)(y - 1),
            (u16)(
                FRONTEND_STATS_TILE_BASE +
                (u16)tile * FRONTEND_STATS_FONT_TILES_PER_GLYPH
            ),
            palette,
            ATTR1_SIZE_16,
            16
        );
        x = (s16)(x + frontend_stats_glyph_width[tile] + 1);
    }
    return x;
}

static s16 frontend_stats_draw_u32(
    s16 x,
    s16 y,
    u32 value,
    u8 palette
)
{
    char digit[10];
    u8 count = 0;

    do {
        digit[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digit));
    while (count != 0) {
        char text[2] = {digit[--count], '\0'};

        x = frontend_stats_draw_text(x, y, text, palette);
    }
    return x;
}

static void frontend_render_stats(void)
{
    u32 killed_percent =
        source_parity_level.total_enemy == 0 ?
            0 :
            (
                source_parity_level.enemy_kill_count * 100u +
                source_parity_level.total_enemy / 2u
            ) /
            source_parity_level.total_enemy;
    s16 x;
    u8 old_oam_count = previous_oam_count;
    u8 settled_palette = 15;
    u8 active_palette = 14;
    u8 palette;

    if (!frontend_stats_font_ready) return;
    oam_count = frontend_stats_scene_oam_count;
    if (source_parity_level.bonus_level) {
        frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_COMPLETED_X,
            TYRIAN_GBA_LAYOUT_STATS_COMPLETED_Y,
            frontend_stats_text(16, "BONUS LEVEL"),
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
    } else {
        x = frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_COMPLETED_X,
            TYRIAN_GBA_LAYOUT_STATS_COMPLETED_Y,
            frontend_stats_text(
                frontend_level_completed ? 26 : 61,
                frontend_level_completed ? "COMPLETED" : "EXITING"
            ),
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
        x = (s16)(x + 6);
        frontend_stats_draw_text(
            x,
            TYRIAN_GBA_LAYOUT_STATS_COMPLETED_Y,
            frontend_level.level_name,
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_CASH) {
        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_CASH ?
                active_palette :
                settled_palette;
        x = frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_CASH_X,
            TYRIAN_GBA_LAYOUT_STATS_CASH_Y,
            frontend_stats_text(27, "CASH"),
            palette
        );
        frontend_stats_draw_u32(
            (s16)(x + 6),
            TYRIAN_GBA_LAYOUT_STATS_CASH_Y,
            player_cash,
            palette
        );
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_ENEMIES) {
        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_ENEMIES ?
                active_palette :
                settled_palette;
        x = frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_ENEMIES_X,
            TYRIAN_GBA_LAYOUT_STATS_ENEMIES_Y,
            frontend_stats_text(62, "ENEMIES DESTROYED"),
            palette
        );
        x = frontend_stats_draw_u32(
            (s16)(x + 6),
            TYRIAN_GBA_LAYOUT_STATS_ENEMIES_Y,
            killed_percent,
            palette
        );
        frontend_stats_draw_text(
            x,
            TYRIAN_GBA_LAYOUT_STATS_ENEMIES_Y,
            "%",
            palette
        );
    }
    if (
        frontend_play_mode != FRONTEND_PLAY_ARCADE &&
        frontend_stats_stage >= FRONTEND_STATS_STAGE_CUBES
    ) {
        u8 cube_total = frontend_stats_cube_total();
        u8 cube_count = frontend_stats_cube_visible_count;
        u8 cube_index;

        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_CUBES ?
                active_palette :
                settled_palette;
        frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_X,
            TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_Y,
            frontend_stats_text(3, "CUBES"),
            palette
        );
        if (cube_count > cube_total) cube_count = cube_total;
        for (cube_index = 0; cube_index < cube_count; cube_index++) {
            frontend_stats_put_sprite(
                (s16)(
                    TYRIAN_GBA_LAYOUT_STATS_CUBE_FIRST_X +
                    (u16)cube_index *
                        TYRIAN_GBA_LAYOUT_STATS_CUBE_X_STEP
                ),
                TYRIAN_GBA_LAYOUT_STATS_CUBE_Y,
                FRONTEND_STATS_CUBE_TILE_BASE,
                13,
                ATTR1_SIZE_32,
                32
            );
        }
        if (
            cube_total == 0 &&
            frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL
        ) {
            frontend_stats_draw_text(
                TYRIAN_GBA_LAYOUT_STATS_CUBE_FIRST_X,
                TYRIAN_GBA_LAYOUT_STATS_CUBE_Y,
                frontend_stats_text(14, "NONE"),
                settled_palette
            );
        }
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL) {
        frontend_stats_draw_text(
            TYRIAN_GBA_LAYOUT_STATS_PROMPT_X,
            TYRIAN_GBA_LAYOUT_STATS_PROMPT_Y,
            frontend_stats_text(4, "PRESS A KEY"),
            frontend_stats_timer == 0 ?
                settled_palette :
                active_palette
        );
    }
    {
        u8 new_oam_count = oam_count;

        while (oam_count < old_oam_count) {
            oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
            oam_count++;
        }
        previous_oam_count = new_oam_count;
        oam_count = new_oam_count;
    }
    oam_dirty = 1;
}

static u8 frontend_static_frame_for_state(
    u8 state,
    u8 selection
)
{
    switch (state) {
    case STATE_INTRO_LOGO_1:
        return FRONTEND_FRAME_INTRO_LOGO_1;
    case STATE_INTRO_LOGO_2:
        return FRONTEND_FRAME_INTRO_LOGO_2;
    case STATE_TITLE:
        return (u8)(FRONTEND_FRAME_TITLE_BASE + selection);
    case STATE_PLAY_MODE_MENU:
        return (u8)(FRONTEND_FRAME_PLAY_MODE_BASE + selection);
    case STATE_EPISODE_MENU:
        return (u8)(FRONTEND_FRAME_EPISODE_BASE + selection);
    case STATE_DIFFICULTY_MENU:
        return (u8)(FRONTEND_FRAME_DIFFICULTY_BASE + selection);
    case STATE_GAME_MENU:
        return (u8)(FRONTEND_FRAME_GAME_MENU_BASE + selection);
    case STATE_NEXT_LEVEL_MENU:
        return FRONTEND_FRAME_NEXT_LEVEL_0;
    case STATE_GAME_OVER:
        return FRONTEND_FRAME_GAME_OVER;
    default:
        return FRONTEND_FRAME_COUNT;
    }
}

static void frontend_redraw(void)
{
    u8 frame;

    if (
        game_state == STATE_TITLE ||
        game_state == STATE_PLAY_MODE_MENU ||
        game_state == STATE_EPISODE_MENU ||
        game_state == STATE_DIFFICULTY_MENU
    ) {
        frontend_render_pre_game_menu();
        return;
    }
    if (game_state == STATE_LEVEL_STATS) {
        frontend_render_stats();
        return;
    }
    if (game_state == STATE_GAME_MENU) {
        frontend_render_game_menu();
        return;
    }
    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level();
        return;
    }
    if (game_state == STATE_UPGRADE_MENU) {
        frontend_render_upgrade_menu();
        return;
    }
    if (game_state == STATE_UPGRADE_SUBMENU) {
        frontend_render_upgrade_submenu();
        return;
    }
    if (game_state == STATE_QUIT_CONFIRM) {
        frontend_render_quit_confirm();
        return;
    }
    if (game_state == STATE_DATA_CUBES) {
        frontend_render_data_cubes();
        return;
    }
    if (game_state == STATE_DATA_CUBE_READER) {
        frontend_render_data_reader();
        return;
    }
    if (game_state == STATE_SHIP_SPECS) {
        frontend_render_ship_specs();
        return;
    }
    if (game_state == STATE_OPTIONS_MENU) {
        frontend_render_options_menu();
        return;
    }
    if (game_state == STATE_SAVE_SLOTS) {
        frontend_render_save_slots();
        return;
    }
    if (game_state == STATE_SAVE_NAME) {
        frontend_render_save_name();
        return;
    }
    frame = frontend_static_frame_for_state(
        game_state,
        frontend_selection
    );
    frontend_present_index(frame);
}

static void frontend_redraw_selection(u8 old_selection)
{
    u8 frame = frontend_static_frame_for_state(
        game_state,
        frontend_selection
    );

    if (
        game_state == STATE_TITLE ||
        game_state == STATE_PLAY_MODE_MENU ||
        game_state == STATE_EPISODE_MENU ||
        game_state == STATE_DIFFICULTY_MENU
    ) {
        frontend_render_pre_game_selection(old_selection);
        return;
    }
    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level_selection(old_selection);
        return;
    }
    if (game_state == STATE_GAME_MENU) {
        frontend_render_game_menu_selection(old_selection);
        return;
    }
    if (game_state == STATE_UPGRADE_MENU) {
        frontend_render_upgrade_menu_selection(old_selection);
        return;
    }
    if (
        game_state == STATE_UPGRADE_SUBMENU ||
        game_state == STATE_QUIT_CONFIRM
    ) {
        frontend_redraw();
        return;
    }
    if (game_state == STATE_DATA_CUBES) {
        frontend_render_data_selection(old_selection);
        return;
    }
    if (
        frame >= FRONTEND_FRAME_COUNT ||
        !frontend_mode4_active ||
        frontend_pending_kind == FRONTEND_PENDING_FULL_FRAME
    ) {
        frontend_present_index(frame);
        return;
    }
    frontend_pending_frame = frontend_frame_address(frame);
    frontend_patch_state = game_state;
    frontend_patch_old_selection = old_selection;
    frontend_patch_new_selection = frontend_selection;
    frontend_pending_kind = FRONTEND_PENDING_SELECTION_PATCH;
    frontend_frame_pending = 1;
}

static void frontend_enter_state(u8 state, u8 selection)
{
    u8 old_state = game_state;

    if (state != STATE_TITLE) {
        frontend_title_brand_pending = 0;
    }
    if (
        old_state == STATE_NEXT_LEVEL_MENU &&
        state != STATE_NEXT_LEVEL_MENU
    ) {
        frontend_nav_obj_disable();
    }
    if (
        old_state == STATE_UPGRADE_SUBMENU &&
        state != STATE_UPGRADE_SUBMENU
    ) {
        frontend_upgrade_sim_disable();
    }
    if (state != STATE_LEVEL_STATS) {
        frontend_stats_overlay_active = 0;
        frontend_stats_tiles_pending = 0;
        frontend_stats_palette_dirty = 0;
        frontend_stats_font_ready = 0;
    }
    game_state = state;
    frontend_selection = selection;
    if (!frontend_render_shared_menu_transition(old_state, state)) {
        frontend_redraw();
    }
    telemetry_state_transitions++;
}

static void frontend_demo_close(void)
{
    if (frontend_demo_file != 0) {
        ot_fclose(frontend_demo_file);
        frontend_demo_file = 0;
    }
    frontend_demo_active = 0;
    frontend_demo_eof = 0;
    frontend_demo_keys = 0;
    frontend_demo_keys_wait = 0;
    frontend_demo_input_guard = 0;
    insert_coin_tile_upload_pending = 0;
}

static void enter_title(void)
{
    frontend_transition_job = FRONTEND_TRANSITION_JOB_NONE;
    frontend_transition_phase = 0;
    frontend_transition_flags = 0;
    frontend_transition_work_index = 0;
    frontend_transition_work_mode = 0;
    frontend_demo_close();
    frontend_player_items_valid = 0;
    player_cash = 0;
    frontend_title_idle_frames = 0;
    frontend_enter_state(STATE_TITLE, 0);
    audio_load_source_song(29);
}

static void frontend_demo_finish(u8 aborted)
{
    if (!frontend_demo_active) return;
    if (aborted) {
        telemetry_demo_aborts++;
    } else {
        telemetry_demo_finishes++;
    }
    frontend_demo_close();
    enter_title();
}

static u8 frontend_demo_read_header(void)
{
    u8 wait_bytes[2];
    u8 unused[3];
    u8 one_based_song;
    char filename[7] = "demo.1";

    if (++frontend_demo_number > FRONTEND_DEMO_COUNT) {
        frontend_demo_number = 1;
    }
    filename[5] = (char)('0' + frontend_demo_number);
    frontend_demo_file = ot_dir_fopen(
        ot_data_dir(),
        filename,
        "rb"
    );
    if (frontend_demo_file == 0) return 0;
    memset(&frontend_demo_header, 0, sizeof(frontend_demo_header));
    if (
        !ot_fread_u8_exact(
            &frontend_demo_header.episode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            (u8 *)frontend_demo_header.level_name,
            10,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.lvl_file_number,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.front_weapon,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.rear_weapon,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.super_arcade_mode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            frontend_demo_header.sidekick,
            2,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.generator,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.sidekick_level,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.sidekick_series,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.initial_episode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.shield,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.special,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.ship,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.front_power,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.rear_power,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(unused, 3, frontend_demo_file) ||
        !ot_fread_u8_exact(
            &one_based_song,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(wait_bytes, 2, frontend_demo_file)
    ) {
        return 0;
    }
    frontend_demo_header.level_name[10] = '\0';
    if (
        frontend_demo_header.episode == 0 ||
        frontend_demo_header.episode > OT_EPISODE_COUNT ||
        frontend_demo_header.lvl_file_number == 0 ||
        one_based_song == 0 ||
        one_based_song > JUKEBOX_MUSIC_COUNT ||
        frontend_demo_header.front_weapon >= OT_HDT_PORT_COUNT ||
        frontend_demo_header.rear_weapon >= OT_HDT_PORT_COUNT ||
        frontend_demo_header.generator >= OT_HDT_POWER_COUNT ||
        frontend_demo_header.sidekick[0] >= OT_HDT_OPTION_COUNT ||
        frontend_demo_header.sidekick[1] >= OT_HDT_OPTION_COUNT ||
        frontend_demo_header.shield >= OT_HDT_SHIELD_COUNT ||
        frontend_demo_header.special >= OT_HDT_SPECIAL_COUNT ||
        frontend_demo_header.ship >= OT_HDT_SHIP_COUNT ||
        frontend_demo_header.front_power == 0 ||
        frontend_demo_header.front_power > 11 ||
        frontend_demo_header.rear_power == 0 ||
        frontend_demo_header.rear_power > 11
    ) {
        return 0;
    }
    /*
     * demo.? stores OpenTyrian's levelSong value.  It is one-based and the
     * PC path deliberately calls play_song(levelSong - 1).  Keep every
     * FrontendDemoHeader consumer zero-based, like OtEpisodeLevel.
     */
    frontend_demo_header.source_song = (u8)(one_based_song - 1u);
    frontend_demo_keys = 0;
    frontend_demo_keys_wait =
        (u16)(((u16)wait_bytes[0] << 8) | wait_bytes[1]);
    return 1;
}

static u8 frontend_demo_start(u8 idle_start)
{
    OtEpisodeLevel demo_level;

    frontend_demo_close();
    if (!frontend_demo_read_header()) {
        telemetry_demo_parse_failures++;
        frontend_demo_close();
        enter_title();
        return 0;
    }
    memset(&demo_level, 0, sizeof(demo_level));
    demo_level.episode = frontend_demo_header.episode;
    demo_level.lvl_file_number =
        frontend_demo_header.lvl_file_number;
    demo_level.source_song = frontend_demo_header.source_song;
    memcpy(
        demo_level.level_name,
        frontend_demo_header.level_name,
        sizeof(demo_level.level_name)
    );
    demo_level.level_name[
        sizeof(demo_level.level_name) - 1u
    ] = '\0';
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = (u8)(demo_level.episode - 1u);
    frontend_difficulty = FRONTEND_DIFFICULTY_NORMAL;
    frontend_main_section = 0;
    frontend_map = (OtEpisodeMap){0};
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_demo_active = 1;
    frontend_demo_eof = 0;
    frontend_demo_input_guard = 12;
    if (!enter_selected_level(&demo_level)) {
        telemetry_demo_parse_failures++;
        frontend_demo_close();
        enter_title();
        return 0;
    }
    telemetry_demo_starts++;
    if (idle_start) telemetry_demo_idle_starts++;
    return 1;
}

static u8 frontend_demo_apply_input(void)
{
    u8 wait_bytes[2];

    if (!frontend_demo_active || frontend_demo_file == 0) return 1;
    while (frontend_demo_keys_wait == 0) {
        if (
            !ot_fread_u8_exact(
                &frontend_demo_keys,
                1,
                frontend_demo_file
            ) ||
            !ot_fread_u8_exact(
                wait_bytes,
                2,
                frontend_demo_file
            )
        ) {
            frontend_demo_eof = 1;
            frontend_demo_keys = 0;
            source_parity_level.end_level = true;
            source_parity_level.level_end = 40;
            source_begin_end_level_audio();
            break;
        }
        frontend_demo_keys_wait =
            (u16)(((u16)wait_bytes[0] << 8) | wait_bytes[1]);
    }
    if (frontend_demo_keys_wait > 0) {
        frontend_demo_keys_wait--;
    }
    pad_now = 0;
    if (frontend_demo_keys & (1u << 0)) pad_now |= KEY_UP;
    if (frontend_demo_keys & (1u << 1)) pad_now |= KEY_DOWN;
    if (frontend_demo_keys & (1u << 2)) pad_now |= KEY_LEFT;
    if (frontend_demo_keys & (1u << 3)) pad_now |= KEY_RIGHT;
    if (frontend_demo_keys & (1u << 4)) pad_now |= KEY_A;
    if (frontend_demo_keys & (1u << 5)) pad_now |= KEY_L;
    if (frontend_demo_keys & (1u << 6)) pad_now |= KEY_B;
    if (frontend_demo_keys & (1u << 7)) pad_now |= KEY_R;
    return 1;
}

static void frontend_advance_episode(void)
{
    if (frontend_episode + 1u >= OT_EPISODE_COUNT) {
        enter_title();
        return;
    }
    frontend_episode++;
    frontend_main_section = 1;
    frontend_map = (OtEpisodeMap){0};
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_start_game_menu_transition(
        FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
        FRONTEND_GAME_MENU_JOB_LOAD_SONG
    );
}

static void frontend_begin(void)
{
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = 0;
    frontend_difficulty = FRONTEND_DIFFICULTY_NORMAL;
    frontend_main_section = 1;
    frontend_map = (OtEpisodeMap){0};
    memset(
        frontend_map_level,
        0,
        sizeof(frontend_map_level)
    );
    frontend_level = (OtEpisodeLevel){0};
    frontend_text_ready =
        ot_data_frontend_text_load(&frontend_text);
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_level_completed = 0;
    frontend_player_items_valid = 0;
    memset(
        &frontend_player_items,
        0,
        sizeof(frontend_player_items)
    );
    player_cash = 0;
    frontend_demo_file = 0;
    frontend_demo_keys_wait = 0;
    frontend_title_idle_frames = 0;
    frontend_demo_keys = 0;
    frontend_demo_number = 0;
    frontend_demo_active = 0;
    frontend_demo_eof = 0;
    frontend_demo_input_guard = 0;
    frontend_save_initialize();
    frontend_timer = 200;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = 0;
    frontend_stats_overlay_active = 0;
    frontend_stats_tiles_pending = 0;
    frontend_stats_palette_dirty = 0;
    frontend_stats_font_ready = 0;
    frontend_stats_scene_oam_count = 0;
    frontend_stats_glow_brightness = -4;
    frontend_nav_obj_active = 0;
    frontend_nav_obj_slot_count = 0;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_palette_pending = 0;
    frontend_nav_obj_dot_tiles_pending = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_title_brand_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_transition_job = FRONTEND_TRANSITION_JOB_NONE;
    frontend_transition_phase = 0;
    frontend_transition_flags = 0;
    frontend_transition_failed = 0;
    frontend_transition_work_index = 0;
    frontend_transition_work_mode = 0;
    telemetry_level_music_transition_starts = 0;
    telemetry_level_music_fade_out_steps = 0;
    telemetry_level_music_silent_vblanks = 0;
    telemetry_level_music_fade_in_steps = 0;
    game_state = STATE_INTRO_LOGO_1;
    frontend_selection = 0;
    mmStop();
    frontend_redraw();
    telemetry_state_transitions++;
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    autotest_campaign_levels_completed = 0;
    autotest_campaign_failures = 0;
    autotest_campaign_checksum = 2166136261u;
    memset(
        autotest_campaign_record,
        0,
        sizeof(autotest_campaign_record)
    );
#endif
}

static void frontend_next_intro(void)
{
    if (game_state == STATE_INTRO_LOGO_1) {
        frontend_timer = 200;
        frontend_enter_state(STATE_INTRO_LOGO_2, 0);
    } else {
        enter_title();
    }
}

static void frontend_move_selection(u8 item_count, s8 direction)
{
    u8 old_selection = frontend_selection;

    if (direction < 0) {
        frontend_selection =
            frontend_selection == 0 ?
                (u8)(item_count - 1) :
                (u8)(frontend_selection - 1);
    } else {
        frontend_selection =
            frontend_selection + 1 >= item_count ?
                0 :
                (u8)(frontend_selection + 1);
    }
    frontend_redraw_selection(old_selection);
    source_sound_effect(28); /* OpenTyrian S_CURSOR. */
}

/* Incremental static-menu transition jobs and their state machine. */
#include "frontend_transition_flow.inc"

static void frontend_update(void)
{
    u16 confirm = pad_pressed & (KEY_A | KEY_START);

    if (
        game_state == STATE_INTRO_LOGO_1 ||
        game_state == STATE_INTRO_LOGO_2
    ) {
        if (pad_pressed != 0 || frontend_timer == 0) {
            frontend_next_intro();
        } else {
            frontend_timer--;
        }
        return;
    }

    if (
        game_state == STATE_TITLE &&
        frontend_title_brand_pending &&
        !frontend_frame_pending
    ) {
        u16 first_row = TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y > 0 ?
            TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y - 1u :
            0u;

        frontend_dirty_begin();
        frontend_dirty_add(
            0,
            first_row,
            SCREEN_WIDTH,
            (u16)(SCREEN_HEIGHT - first_row)
        );
        frontend_dirty_present();
        frontend_title_brand_pending = 0;
        return;
    }

    if (game_state == STATE_TITLE) {
        if (pad_now != 0 || pad_pressed != 0) {
            frontend_title_idle_frames = 0;
        } else if (
            ++frontend_title_idle_frames >=
                FRONTEND_DEMO_IDLE_VBLANKS
        ) {
            frontend_title_idle_frames = 0;
            (void)frontend_demo_start(1);
            return;
        }
    }

    frontend_data_animate();
    frontend_data_reader_animate();
    frontend_upgrade_sim_update();
    if (frontend_save_frontend_update()) return;

    if (game_state == STATE_NEXT_LEVEL_MENU) {
        s16 delta_x = (s16)(
            frontend_nav_target_x - frontend_nav_x
        );
        s16 delta_y = (s16)(
            frontend_nav_target_y - frontend_nav_y
        );
        u8 background_redraw = 0;
        u8 planet_redraw = 0;
        u8 dot_redraw = 0;

        if (delta_x >= -1 && delta_x <= 1) {
            frontend_nav_x = frontend_nav_target_x;
        } else {
            frontend_nav_x = (s16)(
                frontend_nav_x + delta_x / 2
            );
        }
        if (delta_y >= -1 && delta_y <= 1) {
            frontend_nav_y = frontend_nav_target_y;
        } else {
            frontend_nav_y = (s16)(
                frontend_nav_y + delta_y / 2
            );
        }
        if (++frontend_nav_animation_wait >= 4) {
            frontend_nav_animation_wait = 0;
            frontend_nav_planet_animation =
                frontend_nav_planet_animation >= 14 ?
                    0 :
                    (u8)(frontend_nav_planet_animation + 1u);
            planet_redraw = 1;
        }
        if (++frontend_nav_dot_wait >= 6) {
            u8 choice_count = frontend_next_choice_count();

            frontend_nav_dot_wait = 0;
            if (frontend_selection < choice_count) {
                u8 destination = frontend_map.map_planet[
                    frontend_next_map_choice(frontend_selection)
                ];
                u8 dot_count =
                    frontend_nav_dot_count(destination);

                if (frontend_nav_dot_animation < dot_count) {
                    frontend_nav_dot_animation++;
                }
            }
            dot_redraw = 1;
        }
        if (
            (
                frontend_nav_presented_x != frontend_nav_x ||
                frontend_nav_presented_y != frontend_nav_y
            ) &&
            frontend_nav_animation_wait == 0
        ) {
            background_redraw = 1;
        }
        if (
            background_redraw &&
            frontend_pending_kind != FRONTEND_PENDING_FULL_FRAME
        ) {
            frontend_render_next_level_nav();
        } else if (planet_redraw) {
            frontend_nav_obj_animate();
        } else if (dot_redraw) {
            frontend_nav_obj_render_oam();
        }
    }

    if (game_state == STATE_LEVEL_STATS) {
        if (
            level_exit_music_started &&
            telemetry_end_level_music_natural_stops == 0 &&
            !mmActive()
        ) {
            telemetry_end_level_music_natural_stops = 1;
        }
        if (pad_pressed != 0) {
            if (frontend_stats_stage < FRONTEND_STATS_STAGE_FINAL) {
                telemetry_stats_stage_advances +=
                    FRONTEND_STATS_STAGE_FINAL -
                        frontend_stats_stage;
                frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
                if (
                    frontend_play_mode != FRONTEND_PLAY_ARCADE
                ) {
                    u8 cube_total = frontend_stats_cube_total();

                    while (
                        frontend_stats_cube_visible_count <
                        cube_total
                    ) {
                        frontend_stats_cube_visible_count++;
                        telemetry_stats_cube_reveals++;
                        audio_effect(SFX_ITEM);
                    }
                }
                frontend_stats_timer = 0;
                frontend_redraw();
            } else {
                if (
                    frontend_level_completed &&
                    source_parity_level.bonus_level &&
                    source_parity_level.next_level != 0
                ) {
                    frontend_main_section =
                        source_parity_level.next_level;
                } else if (
                    frontend_level_completed &&
                    frontend_level_ready &&
                    frontend_level.next_section != 0
                ) {
                    frontend_main_section = frontend_level.next_section;
                }
                frontend_start_game_menu_transition(
                    FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                    FRONTEND_GAME_MENU_JOB_LOAD_SONG
                );
#ifdef AUTOTEST
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
                if (
                    autotest_campaign_levels_completed >=
                        AUTOTEST_CAMPAIGN_LEVEL_COUNT
                ) {
                    autotest_frontend_finish_pending = 1;
                }
#else
                autotest_frontend_finish_pending = 1;
#endif
#endif
            }
            return;
        }
        if (frontend_stats_timer > 0) {
            frontend_stats_timer--;
            frontend_stats_advance_glow();
            if (
                frontend_stats_timer == 0 &&
                frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL
            ) {
                frontend_render_stats();
            }
            return;
        }
        if (frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL) {
            return;
        }
        if (frontend_stats_stage == FRONTEND_STATS_STAGE_CUBES) {
            u8 cube_total = frontend_stats_cube_total();

            if (
                frontend_play_mode != FRONTEND_PLAY_ARCADE &&
                frontend_stats_cube_visible_count < cube_total
            ) {
                frontend_stats_cube_visible_count++;
                frontend_stats_timer = FRONTEND_STATS_CUBE_TICKS;
                telemetry_stats_cube_reveals++;
                audio_effect(SFX_ITEM);
                frontend_redraw();
                return;
            }
            frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
        } else if (
            frontend_stats_stage == FRONTEND_STATS_STAGE_ENEMIES &&
            frontend_play_mode == FRONTEND_PLAY_ARCADE
        ) {
            frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
        } else {
            frontend_stats_stage++;
        }
        frontend_stats_timer = FRONTEND_STATS_STAGE_TICKS;
        frontend_stats_restart_glow();
        telemetry_stats_stage_advances++;
        frontend_redraw();
        return;
    }
    if (pad_pressed & KEY_UP) {
        switch (game_state) {
        case STATE_TITLE:
            frontend_move_selection(4, -1);
            break;
        case STATE_PLAY_MODE_MENU:
            frontend_move_selection(2, -1);
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(
                (u8)(frontend_next_choice_count() + 1u),
                -1
            );
            break;
        case STATE_EPISODE_MENU:
            frontend_move_selection(4, -1);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_move_selection(3, -1);
            break;
        case STATE_GAME_MENU:
            frontend_move_selection(6, -1);
            break;
        case STATE_UPGRADE_MENU:
            frontend_move_selection(
                FRONTEND_UPGRADE_MENU_ITEM_COUNT,
                -1
            );
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_move_submenu(-1);
            break;
        case STATE_DATA_CUBES:
            frontend_start_data_selection_transition(-1);
            break;
        case STATE_DATA_CUBE_READER:
            frontend_data_reader_scroll(-1);
            break;
        default:
            break;
        }
        return;
    }
    if (pad_pressed & KEY_DOWN) {
        switch (game_state) {
        case STATE_TITLE:
            frontend_move_selection(4, 1);
            break;
        case STATE_PLAY_MODE_MENU:
            frontend_move_selection(2, 1);
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(
                (u8)(frontend_next_choice_count() + 1u),
                1
            );
            break;
        case STATE_EPISODE_MENU:
            frontend_move_selection(4, 1);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_move_selection(3, 1);
            break;
        case STATE_GAME_MENU:
            frontend_move_selection(6, 1);
            break;
        case STATE_UPGRADE_MENU:
            frontend_move_selection(
                FRONTEND_UPGRADE_MENU_ITEM_COUNT,
                1
            );
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_move_submenu(1);
            break;
        case STATE_DATA_CUBES:
            frontend_start_data_selection_transition(1);
            break;
        case STATE_DATA_CUBE_READER:
            frontend_data_reader_scroll(1);
            break;
        default:
            break;
        }
        return;
    }

    if (pad_pressed & (KEY_LEFT | KEY_RIGHT)) {
        if (game_state == STATE_UPGRADE_SUBMENU) {
            frontend_upgrade_power_adjust(
                (pad_pressed & KEY_LEFT) != 0 ? -1 : 1
            );
            return;
        }
        if (game_state == STATE_QUIT_CONFIRM) {
            frontend_quit_yes ^= 1u;
            frontend_selection = frontend_quit_yes ? 0 : 1;
            audio_effect(SFX_CURSOR);
            frontend_render_quit_selection();
            return;
        }
    }
    if (
        (pad_pressed & KEY_R) != 0 &&
        game_state == STATE_UPGRADE_SUBMENU &&
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON &&
        frontend_selection < frontend_upgrade_sub_count
    ) {
        OtWeaponPortDefinition port;
        u8 rear = frontend_player_items.weapon[
            FRONTEND_WEAPON_REAR
        ].id;

        if (
            rear != 0 &&
            ot_data_hdt_weapon_port_read(rear, &port) &&
            port.opnum != 0
        ) {
            frontend_player_items.weapon_mode++;
            if (frontend_player_items.weapon_mode > port.opnum) {
                frontend_player_items.weapon_mode = 1;
            }
            frontend_upgrade_sim_mark_loadout_dirty();
            audio_effect(SFX_CURSOR);
            frontend_render_upgrade_submenu_update(
                frontend_selection,
                frontend_upgrade_sub_scroll
            );
        } else {
            audio_effect(SFX_CLINK);
        }
        return;
    }

    if (pad_pressed & KEY_B) {
        switch (game_state) {
        case STATE_PLAY_MODE_MENU:
            audio_effect(SFX_SPRING);
            enter_title();
            break;
        case STATE_EPISODE_MENU:
            audio_effect(SFX_SPRING);
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
            break;
        case STATE_DIFFICULTY_MENU:
            audio_effect(SFX_SPRING);
            frontend_enter_state(STATE_EPISODE_MENU, frontend_episode);
            break;
        case STATE_GAME_MENU:
            frontend_quit_yes = 1;
            audio_effect(SFX_SPRING);
            frontend_start_quit_transition(1);
            break;
        case STATE_NEXT_LEVEL_MENU:
            audio_effect(SFX_SPRING);
            frontend_start_game_menu_transition(0);
            break;
        case STATE_UPGRADE_MENU:
            audio_effect(SFX_SPRING);
            frontend_enter_state(STATE_GAME_MENU, 2);
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_cancel_submenu();
            break;
        case STATE_QUIT_CONFIRM:
            frontend_quit_yes = 0;
            frontend_start_quit_transition(0);
            audio_effect(SFX_CLICK);
            break;
        case STATE_DATA_CUBES:
            frontend_data_cache_valid = 0;
            frontend_start_game_menu_transition_selected(0, 0);
            audio_effect(SFX_SPRING);
            break;
        case STATE_DATA_CUBE_READER:
            frontend_start_data_cubes_transition(
                frontend_data_reader_index,
                0
            );
            audio_effect(SFX_SPRING);
            break;
        case STATE_SHIP_SPECS:
            frontend_start_game_menu_transition_selected(0, 1);
            audio_effect(SFX_SPRING);
            break;
        default:
            break;
        }
        return;
    }
    if (!confirm) return;

    switch (game_state) {
    case STATE_TITLE:
        if (frontend_selection == 0) {
            audio_effect(SFX_SELECT);
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        } else if (frontend_selection == 1) {
            audio_effect(SFX_SELECT);
            frontend_save_open_slots(
                FRONTEND_SAVE_MODE_LOAD,
                STATE_TITLE,
                1
            );
        } else if (frontend_selection == 2) {
            audio_effect(SFX_SELECT);
            (void)frontend_demo_start(0);
        } else {
            audio_effect(SFX_SELECT);
            jukebox_enter();
        }
        break;
    case STATE_PLAY_MODE_MENU:
        audio_effect(SFX_SELECT);
        frontend_play_mode = frontend_selection;
        frontend_enter_state(STATE_EPISODE_MENU, 0);
        break;
    case STATE_EPISODE_MENU:
        audio_effect(SFX_SELECT);
        frontend_episode = frontend_selection;
        frontend_main_section = 1;
        frontend_map = (OtEpisodeMap){0};
        frontend_level = (OtEpisodeLevel){0};
        frontend_map_ready = 0;
        frontend_level_ready = 0;
        frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        break;
    case STATE_DIFFICULTY_MENU:
        audio_effect(SFX_SELECT);
        frontend_difficulty =
            (u8)(FRONTEND_DIFFICULTY_EASY + frontend_selection);
#ifdef AUTOTEST_FRONTEND_CAPTURE_SECTION
        /*
         * Deterministic visual regression for a post-level merchant/map
         * section.  Release builds always keep the selected Episode's
         * normal section-1 entry point.
         */
        frontend_main_section = AUTOTEST_FRONTEND_CAPTURE_SECTION;
#endif
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_INITIALIZE_CAMPAIGN |
            FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
            FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
        break;
    case STATE_GAME_MENU:
        if (frontend_selection == 0) {
            audio_effect(SFX_CLICK);
            frontend_start_data_cubes_transition(
                0,
                FRONTEND_DATA_JOB_RELOAD |
                    FRONTEND_DATA_JOB_FULL_FRAME
            );
        } else if (frontend_selection == 1) {
            audio_effect(SFX_SPRING);
            frontend_start_ship_specs_transition();
        } else if (frontend_selection == 2) {
            frontend_start_upgrade_menu_transition(0);
            audio_effect(SFX_CLICK);
        } else if (frontend_selection == 3) {
            frontend_enter_state(STATE_OPTIONS_MENU, 0);
            audio_effect(SFX_CLICK);
        } else if (frontend_selection == 4) {
            audio_effect(SFX_CLICK);
            if (
                !frontend_map_ready &&
                !frontend_prepare_map()
            ) {
                frontend_load_game_menu_song();
            } else if (frontend_map.episode_complete) {
                frontend_advance_episode();
            } else {
                frontend_nav_reset();
                frontend_start_next_level_transition();
            }
        } else if (frontend_selection == 5) {
            frontend_quit_yes = 1;
            audio_effect(SFX_CLICK);
            frontend_start_quit_transition(1);
        }
        break;
    case STATE_DATA_CUBES:
        if (frontend_selection < frontend_data_page_count) {
            audio_effect(SFX_CLICK);
            frontend_start_data_reader_transition(
                frontend_selection
            );
        } else {
            frontend_data_cache_valid = 0;
            audio_effect(SFX_CLICK);
            frontend_start_game_menu_transition_selected(0, 0);
        }
        break;
    case STATE_DATA_CUBE_READER:
        audio_effect(SFX_CLICK);
        frontend_start_data_cubes_transition(
            frontend_data_reader_index,
            0
        );
        break;
    case STATE_SHIP_SPECS:
        audio_effect(SFX_CLICK);
        frontend_start_game_menu_transition_selected(0, 1);
        break;
    case STATE_UPGRADE_MENU:
        if (frontend_selection == FRONTEND_UPGRADE_DONE) {
            frontend_start_game_menu_transition_selected(0, 2);
            audio_effect(SFX_CLICK);
        } else if (
            frontend_upgrade_begin_submenu(frontend_selection)
        ) {
            frontend_start_upgrade_submenu_transition(
                frontend_selection
            );
            frontend_upgrade_apply_preview();
            audio_effect(SFX_CLICK);
        } else {
            audio_effect(SFX_CLINK);
        }
        break;
    case STATE_UPGRADE_SUBMENU:
        if (frontend_selection < frontend_upgrade_sub_count) {
            u8 old_selection = frontend_selection;
            u8 old_scroll = frontend_upgrade_sub_scroll;

            /* JE_menuFunction() snapshots the preview into old_items before
             * advancing to Done; the following frame restores that accepted
             * item and JE_cashLeft() charges it on the second confirmation. */
            frontend_upgrade_accept_preview();
            frontend_selection = frontend_upgrade_sub_count;
            if (
                frontend_selection >=
                    frontend_upgrade_sub_scroll + 6u
            ) {
                frontend_upgrade_sub_scroll =
                    (u8)(frontend_selection - 5u);
            }
            audio_effect(SFX_CLICK);
            frontend_upgrade_apply_preview();
            frontend_render_upgrade_submenu_update(
                old_selection,
                old_scroll
            );
        } else {
            frontend_upgrade_commit_submenu();
        }
        break;
    case STATE_QUIT_CONFIRM:
        if (frontend_quit_yes) {
            audio_effect(SFX_SPRING);
            enter_title();
        } else {
            audio_effect(SFX_CLICK);
            frontend_start_quit_transition(0);
        }
        break;
    case STATE_NEXT_LEVEL_MENU:
        audio_effect(SFX_CLICK);
        if (frontend_selection < frontend_next_choice_count()) {
            u8 map_choice =
                frontend_next_map_choice(frontend_selection);

            if (frontend_map_level[map_choice].episode_complete) {
                frontend_advance_episode();
            } else {
                frontend_start_level_entry_transition(
                    frontend_map.map_section[map_choice]
                );
            }
        } else {
            frontend_start_game_menu_transition(0);
        }
        break;
    default:
        break;
    }
}

#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
static void frontend_autotest_record_campaign_level(u8 completed)
{
    u32 record_index = autotest_campaign_levels_completed;
    u32 prior;
    AutotestCampaignRecord *record;

    if (record_index >= AUTOTEST_CAMPAIGN_LEVEL_COUNT) {
        autotest_campaign_failures++;
        return;
    }
    record = &autotest_campaign_record[record_index];
    record->episode = frontend_level.episode;
    record->resolved_section = frontend_level.resolved_section;
    record->lvl_file_number = frontend_level.lvl_file_number;
    record->source_song = frontend_level.source_song;
    record->event_index = source_parity_level.event_index;
    record->level_position = level_position;
    record->enemy_kills = telemetry_source_enemy_kills;
    record->background_approximations =
        background_cache_approximations;

    for (prior = 0; prior < record_index; prior++) {
        if (
            autotest_campaign_record[prior].episode ==
                record->episode &&
            autotest_campaign_record[prior].resolved_section ==
                record->resolved_section
        ) {
            record->failure_flags |=
                AUTOTEST_CAMPAIGN_FAIL_DUPLICATE_ROUTE;
        }
    }
    autotest_campaign_checksum ^= record->episode;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->resolved_section;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->lvl_file_number;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->source_song;
    autotest_campaign_checksum *= 16777619u;

    if (!completed || !frontend_level_ready) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_COMPLETION;
    }
    if (
        !source_parity_level.assets_valid ||
        source_parity_level.event_index == 0 ||
        level_position == 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_ASSETS;
    }
    if (
        source_parity_level.applied_event_count +
            source_parity_level.deferred_event_count +
            source_parity_level.skipped_event_count !=
            source_parity_level.event_index
    ) {
        record->failure_flags |=
            AUTOTEST_CAMPAIGN_FAIL_EVENT_ACCOUNTING;
    }
    if (
        source_parity_level.spawn_success_count +
            source_parity_level.spawn_pool_full_count +
            source_parity_level.spawn_missing_definition_count !=
            source_parity_level.spawn_attempt_count ||
        source_parity_level.spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_EVENT_SPAWN;
    }
    if (
        source_parity_level.enemy_launch_success_count +
            source_parity_level.enemy_launch_pool_full_count +
            source_parity_level.enemy_launch_missing_definition_count !=
            source_parity_level.enemy_launch_attempt_count ||
        source_parity_level.enemy_launch_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_LAUNCH_SPAWN;
    }
    if (
        source_parity_level.random_spawn_success_count +
            source_parity_level.random_spawn_pool_full_count +
            source_parity_level.random_spawn_missing_definition_count !=
            source_parity_level.random_spawn_attempt_count ||
        source_parity_level.random_spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_RANDOM_SPAWN;
    }
    if (
        source_parity_level.death_spawn_success_count +
            source_parity_level.death_spawn_pool_full_count +
            source_parity_level.death_spawn_missing_definition_count !=
            source_parity_level.death_spawn_attempt_count ||
        source_parity_level.death_spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_DEATH_SPAWN;
    }
    if (telemetry_romfs_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_ROMFS;
    }
    if (telemetry_sprite2_decode_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_SPRITE_DECODE;
    }
    if (
        telemetry_sprite2_cache_drops != 0 ||
        telemetry_sprite2_uploads +
                telemetry_sprite2_upload_coalesces +
                source_sprite2_pending_upload_count() !=
            telemetry_sprite2_cache_misses ||
        telemetry_sprite2_compact_uploads >
            telemetry_sprite2_uploads ||
        telemetry_sprite2_upload_bytes !=
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_SPRITE_CACHE;
    }
    if (telemetry_effect_cache_drops != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_EFFECT_CACHE;
    }
    if (telemetry_projectile_cache_drops != 0) {
        record->failure_flags |=
            AUTOTEST_CAMPAIGN_FAIL_PROJECTILE_CACHE;
    }
    if (telemetry_layer_rule_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_LAYER_RULE;
    }
    if (telemetry_end_level_music_starts != 1) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_END_MUSIC;
    }
    if (record->failure_flags != 0) autotest_campaign_failures++;
    autotest_campaign_levels_completed++;
}
#endif

static void frontend_enter_level_stats(u8 completed)
{
    u16 index;

    if (frontend_demo_active) {
        frontend_demo_finish(0);
        return;
    }
    frontend_campaign_sync_from_level();
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    frontend_autotest_record_campaign_level(completed);
#endif
    frontend_level_completed = completed;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = FRONTEND_STATS_STAGE_TICKS;
    /*
     * JE_endLevelAni() writes onto VGAScreenSeg without clearing it.  Keep
     * the final Mode-0 gameplay frame and add only a transparent BG3 text
     * layer; switching to the generated Mode-4 stats atlas caused the
     * incorrect black background.
     */
    game_state = STATE_LEVEL_STATS;
    frontend_selection = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_stats_overlay_active = 1;
    frontend_stats_palette_dirty = 0;
    frontend_stats_scene_oam_count = previous_oam_count;
    for (index = 0; index < frontend_stats_scene_oam_count; index++) {
        oam_shadow[index].attr2 =
            (u16)((oam_shadow[index].attr2 & ~0x0c00u) | 0x0c00u);
    }
    oam_dirty = 1;
    if (frontend_stats_prepare_tiles()) {
        frontend_render_stats();
    } else {
        frontend_stats_overlay_active = 0;
    }
    telemetry_state_transitions++;
    source_sound_effect(34);
    telemetry_level_complete_voice_starts++;
}

static void frontend_enter_game_over(void)
{
    if (frontend_demo_active) {
        frontend_demo_finish(0);
        return;
    }
    /*
     * tyrian2.c leaves VGAScreen on the live gameplay frame and overlays
     * miscText[21].  Stay in Mode 0, request the OBJ glyph upload, and select
     * zero-based SONG_GAMEOVER (10) without cancelling the last explosion.
     */
    game_state = STATE_GAME_OVER;
    frontend_selection = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_paused = 0;
    player_death_music_fade_active = 0;
    secret_level_music_active = 0;
    game_over_tile_upload_pending = 1;
    audio_load_source_song_once_preserve_effects(10);
    telemetry_game_over_music_starts++;
    telemetry_state_transitions++;
}

static void game_over_update(void)
{
    if (
        telemetry_game_over_music_starts != 0 &&
        telemetry_game_over_music_natural_stops == 0 &&
        !mmActive()
    ) {
        telemetry_game_over_music_natural_stops = 1;
    }
    if (pad_pressed == 0) return;

    if (
        frontend_script_game_active &&
        frontend_script_checkpoint_restore()
    ) {
        /*
         * tyrian2.c start_level death branch: ]e sets doNotSaveBackup, so
         * the final arcade life reloads the ]s/]b LAST LEVEL checkpoint.
         */
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
    } else {
        frontend_script_game_active = 0;
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
    }
    telemetry_game_over_exits++;
}
