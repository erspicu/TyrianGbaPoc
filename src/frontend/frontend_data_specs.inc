/*
 * Data Cube and Ship Specs pages translated from OpenTyrian game_menu.c.
 *
 * All prose, cube/face IDs and ship records come from ROMFS stock files.
 * Only the final 300x200 -> 240x160 presentation and the input adapter are
 * GBA-specific.
 */

enum {
    FRONTEND_DATA_LEFT_X = 0,
    FRONTEND_DATA_LEFT_Y = 0,
    FRONTEND_DATA_LEFT_WIDTH = 118,
    FRONTEND_DATA_LEFT_HEIGHT = 144,
    FRONTEND_DATA_RIGHT_X = 128,
    FRONTEND_DATA_RIGHT_Y = 28,
    FRONTEND_DATA_RIGHT_WIDTH = 112,
    FRONTEND_DATA_RIGHT_HEIGHT = 110,
    FRONTEND_DATA_TEXT_X = 154,
    FRONTEND_DATA_TEXT_RIGHT = 239,
    FRONTEND_DATA_TEXT_LINE_HEIGHT = 7,
    FRONTEND_DATA_READER_X = 129,
    FRONTEND_DATA_READER_Y = 40,
    FRONTEND_DATA_READER_LINE_HEIGHT = 9,
    /*
     * The PC loop attempts ten 12px rows, but its bottom status strip at
     * y=157 overwrites the tenth; the scroll percentage likewise divides by
     * last_line - 9.  Nine rows are therefore the true visible viewport.
     */
    FRONTEND_DATA_READER_VISIBLE_LINES = 9,
};

static const u8 frontend_data_face_palette_index[12] = {
    1, 2, 3, 4, 6, 9, 11, 12, 16, 13, 14, 15,
};

static const char *frontend_data_text(u8 index, const char *fallback)
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

static u8 frontend_data_prepare(void)
{
    u8 source_count =
        frontend_data_cube_count > OT_EPISODE_CUBE_CAPACITY ?
            OT_EPISODE_CUBE_CAPACITY :
            frontend_data_cube_count;
    u8 source_index;

    frontend_data_page_count = 0;
    frontend_data_cache_valid = 0;
    for (source_index = 0; source_index < source_count; source_index++) {
        u8 cube_index = frontend_data_cube_list[source_index];

        if (
            cube_index != 0 &&
            ot_data_cube_read(
                (u8)(frontend_episode + 1u),
                cube_index,
                &frontend_data_cube_cache[
                    frontend_data_page_count
                ]
            )
        ) {
            frontend_data_page_count++;
        }
    }
    frontend_data_cache_valid = 1;
    frontend_data_reader_index = 0;
    frontend_data_scroll_line = 0;
    frontend_data_glow = 1;
    frontend_data_glow_delta = 1;
    frontend_data_glow_wait = 0;
    frontend_quit_dialog_cache_valid = 0;
    return 1;
}

static void frontend_data_prepare_palette(void)
{
    const u8 *base =
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME);

    memcpy(
        frontend_data_face_palette,
        base,
        OT_PALETTE_COLOUR_COUNT * sizeof(u16)
    );
    if (
        frontend_selection < frontend_data_page_count &&
        frontend_data_cache_valid
    ) {
        s16 face = frontend_data_cube_cache[
            frontend_selection
        ].face_sprite;

        if (
            face >= 0 &&
            face < (s16)(
                sizeof(frontend_data_face_palette_index) /
                    sizeof(frontend_data_face_palette_index[0])
            )
        ) {
            OtDataView palette;

            if (ot_data_palette_view(
                    frontend_data_face_palette_index[face],
                    &palette
                )) {
                u16 index;

                /*
                 * OpenTyrian changes colors[1..207] for the selected face
                 * and deliberately leaves the upper three UI ramps intact.
                 */
                for (index = 1; index <= 207; index++) {
                    frontend_data_face_palette[index] =
                        title_palette_colour(&palette, (u8)index);
                }
            }
        }
    }
}

static void frontend_data_draw_cube(
    u8 cube_slot,
    s8 glow
)
{
    s16 source_x = 166;
    s16 source_y = (s16)(38 + cube_slot * 28);

    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        25,
        (s16)(source_x + 4),
        (s16)(source_y + 4),
        FRONTEND_SOURCE_BLIT_DARK,
        0,
        0
    );
    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        25,
        (s16)(source_x + 3),
        (s16)(source_y + 3),
        FRONTEND_SOURCE_BLIT_DARK,
        0,
        0
    );
    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        25,
        source_x,
        source_y,
        FRONTEND_SOURCE_BLIT_HUE,
        13,
        glow
    );
}

static void frontend_data_draw_portrait(void)
{
    frontend_fill_source_rect(
        frontend_frame_scratch,
        1,
        1,
        145,
        170,
        0
    );
    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        20,
        1,
        1,
        FRONTEND_SOURCE_BLIT_NORMAL,
        0,
        0
    );
    if (
        frontend_selection < frontend_data_page_count &&
        frontend_data_cache_valid
    ) {
        const OtDataCube *cube =
            &frontend_data_cube_cache[frontend_selection];

        if (cube->face_sprite >= 0) {
            OtShpSprite face;

            if (ot_data_shp_sprite_read(
                    4,
                    (u16)cube->face_sprite,
                    &face
                ) &&
                face.populated
            ) {
                (void)frontend_blit_shp_source(
                    frontend_frame_scratch,
                    4,
                    (u16)cube->face_sprite,
                    (s16)(77 - (s16)face.width / 2),
                    (s16)(92 - (s16)face.height / 2),
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        }
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            cube->header,
            (s16)(
                frontend_source_to_screen_x(75) -
                frontend_small_mixed_text_width(cube->header) / 2
            ),
            frontend_source_to_screen_y(173),
            FRONTEND_DATA_LEFT_WIDTH,
            0xedu,
            0xe2u
        );
    }
}

static void frontend_data_draw_list(void)
{
    u8 cube_slot;

    if (frontend_data_page_count == 0) {
        frontend_draw_small_mixed_text_wrapped(
            frontend_frame_scratch,
            frontend_data_text(
                15,
                "No Data is available at this time."
            ),
            frontend_source_to_screen_x(166),
            frontend_source_to_screen_y(80),
            FRONTEND_DATA_TEXT_RIGHT,
            8,
            5,
            0xfau
        );
    } else {
        for (
            cube_slot = 0;
            cube_slot < frontend_data_page_count;
            cube_slot++
        ) {
            u8 colour =
                cube_slot == frontend_selection ? 0xfcu : 0xfau;

            frontend_data_draw_cube(
                cube_slot,
                cube_slot == frontend_selection ?
                    frontend_data_glow :
                    0
            );
            frontend_draw_small_mixed_text_wrapped(
                frontend_frame_scratch,
                frontend_data_cube_cache[cube_slot].title,
                FRONTEND_DATA_TEXT_X,
                frontend_source_to_screen_y(
                    (s16)(44 + cube_slot * 28)
                ),
                FRONTEND_DATA_TEXT_RIGHT,
                FRONTEND_DATA_TEXT_LINE_HEIGHT,
                2,
                colour
            );
        }
    }
    {
        u8 done_slot = frontend_data_page_count;
        s16 done_source_y = frontend_data_page_count == 0 ?
            160 :
            (s16)(44 + done_slot * 28);

        frontend_draw_small_mixed_text_clipped(
            frontend_frame_scratch,
            frontend_data_text(5, "Exit to Game Menu"),
            frontend_source_to_screen_x(172),
            frontend_source_to_screen_y(done_source_y),
            SCREEN_WIDTH,
            done_slot == frontend_selection ? 0xfcu : 0xfau
        );
    }
}

static void frontend_data_compose_list(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_data_draw_portrait();
    frontend_data_draw_list();
    frontend_data_prepare_palette();
}

static void frontend_render_data_cubes(void)
{
    if (!frontend_data_cache_valid) (void)frontend_data_prepare();
    if (frontend_selection > frontend_data_page_count) {
        frontend_selection = frontend_data_page_count;
    }
    frontend_data_compose_list();
    frontend_present(
        frontend_frame_scratch,
        (const u8 *)(const void *)frontend_data_face_palette
    );
}

static void frontend_render_data_selection(u8 old_selection)
{
    (void)old_selection;
    frontend_restore_menu_chrome_rect(
        FRONTEND_DATA_LEFT_X,
        FRONTEND_DATA_LEFT_Y,
        FRONTEND_DATA_LEFT_WIDTH,
        FRONTEND_DATA_LEFT_HEIGHT
    );
    frontend_restore_menu_chrome_rect(
        FRONTEND_DATA_RIGHT_X,
        FRONTEND_DATA_RIGHT_Y,
        FRONTEND_DATA_RIGHT_WIDTH,
        FRONTEND_DATA_RIGHT_HEIGHT
    );
    frontend_data_draw_portrait();
    frontend_data_draw_list();
    frontend_data_prepare_palette();

    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_DATA_LEFT_X,
        FRONTEND_DATA_LEFT_Y,
        FRONTEND_DATA_LEFT_WIDTH,
        FRONTEND_DATA_LEFT_HEIGHT
    );
    frontend_dirty_add(
        FRONTEND_DATA_RIGHT_X,
        FRONTEND_DATA_RIGHT_Y,
        FRONTEND_DATA_RIGHT_WIDTH,
        FRONTEND_DATA_RIGHT_HEIGHT
    );
    frontend_dirty_present_with_palette(frontend_data_face_palette);
}

static void frontend_data_draw_reader_panel(void)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    u8 line;
    u8 max_scroll =
        cube->last_line > FRONTEND_DATA_READER_VISIBLE_LINES ?
            (u8)(
                cube->last_line -
                FRONTEND_DATA_READER_VISIBLE_LINES
            ) :
            0;
    u8 percent =
        max_scroll == 0 ?
            100 :
            (u8)(
                (u16)frontend_data_scroll_line * 100u /
                max_scroll
            );
    char percent_text[48];
    char percent_prefix[44];
    u8 length;

    frontend_fill_source_rect(
        frontend_frame_scratch,
        160,
        39,
        310,
        166,
        228
    );
    for (
        line = 0;
        line < FRONTEND_DATA_READER_VISIBLE_LINES;
        line++
    ) {
        u8 source_line =
            (u8)(frontend_data_scroll_line + line);

        if (source_line >= cube->last_line) break;
        const char *text = cube->text[source_line];
        s16 x = FRONTEND_DATA_READER_X;
        s16 y = (s16)(
            FRONTEND_DATA_READER_Y +
            line * FRONTEND_DATA_READER_LINE_HEIGHT
        );

        /*
         * PC TINY_FONT is 4x6-ish and its 150px wrapped line maps into a
         * 112px GBA pane.  Reusing the 4-column help face would clip roughly
         * a quarter of every line.  Downsample the shared mixed-case 6x8
         * source to a dense 3x5 face here: no new alphabet/resource, and the
         * original lower-case text remains distinguishable.
         */
        while (*text != '\0' && x < SCREEN_WIDTH) {
            u8 tile = frontend_pregame_font_tile(*text++);
            u8 source_width =
                frontend_pregame_font_width(tile);
            u8 target_width =
                tile == 0 ?
                    2 :
                    (
                        source_width > 3 ?
                            3 :
                            source_width
                    );
            u8 target_row;

            if (x + target_width > SCREEN_WIDTH) break;
            if (tile == 0) {
                x = (s16)(x + target_width);
                continue;
            }
            for (target_row = 0; target_row < 5; target_row++) {
                u8 source_y0 = (u8)(
                    (u16)target_row *
                        FRONTEND_PREGAME_FONT_HEIGHT / 5u
                );
                u8 source_y1 = (u8)(
                    (
                        (u16)(target_row + 1u) *
                            FRONTEND_PREGAME_FONT_HEIGHT +
                        4u
                    ) / 5u
                );
                u8 target_column;

                if (source_y1 > FRONTEND_PREGAME_FONT_HEIGHT) {
                    source_y1 = FRONTEND_PREGAME_FONT_HEIGHT;
                }
                for (
                    target_column = 0;
                    target_column < target_width;
                    target_column++
                ) {
                    u8 source_x0 = (u8)(
                        (u16)target_column *
                            source_width /
                        target_width
                    );
                    u8 source_x1 = (u8)(
                        (
                            (u16)(target_column + 1u) *
                                source_width +
                            target_width - 1u
                        ) / target_width
                    );
                    u8 source_y;
                    u8 occupied = 0;

                    if (source_x1 > source_width) {
                        source_x1 = source_width;
                    }
                    for (
                        source_y = source_y0;
                        source_y < source_y1 && !occupied;
                        source_y++
                    ) {
                        u8 source_row =
                            frontend_pregame_font_row(
                                tile,
                                source_y
                            );
                        u8 source_x;

                        for (
                            source_x = source_x0;
                            source_x < source_x1;
                            source_x++
                        ) {
                            if (
                                (
                                    source_row &
                                    (
                                        1u <<
                                        (
                                            source_width -
                                            source_x -
                                            1u
                                        )
                                    )
                                ) != 0
                            ) {
                                occupied = 1;
                                break;
                            }
                        }
                    }
                    if (occupied) {
                        s16 pixel_x = (s16)(x + target_column);
                        s16 pixel_y = (s16)(y + target_row);

                        if (
                            pixel_x + 1 < SCREEN_WIDTH &&
                            pixel_y + 1 < SCREEN_HEIGHT
                        ) {
                            frontend_frame_scratch[
                                (u32)(pixel_y + 1) *
                                    SCREEN_WIDTH +
                                (u16)(pixel_x + 1)
                            ] = 0xe2u;
                        }
                        frontend_frame_scratch[
                            (u32)pixel_y * SCREEN_WIDTH +
                            (u16)pixel_x
                        ] = 0xedu;
                    }
                }
            }
            x = (s16)(x + target_width);
        }
    }
    {
        const char *source_prefix =
            frontend_data_text(11, "Read:");
        size_t prefix_length = strlen(source_prefix);

        if (prefix_length > sizeof(percent_prefix) - 2u) {
            prefix_length = sizeof(percent_prefix) - 2u;
        }
        memcpy(percent_prefix, source_prefix, prefix_length);
        percent_prefix[prefix_length++] = ' ';
        percent_prefix[prefix_length] = '\0';
    }
    frontend_text_with_u32(
        percent_text,
        sizeof(percent_text),
        percent_prefix,
        percent
    );
    length = (u8)strlen(percent_text);
    if (length + 1u < sizeof(percent_text)) {
        percent_text[length++] = '%';
        percent_text[length] = '\0';
    }
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        percent_text,
        frontend_source_to_screen_x(176),
        frontend_source_to_screen_y(160),
        frontend_source_to_screen_x(254),
        0xebu,
        0xe2u
    );
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        frontend_data_text(12, "EXIT"),
        frontend_source_to_screen_x(260),
        frontend_source_to_screen_y(160),
        SCREEN_WIDTH,
        0xfeu,
        0xf2u
    );
}

static void frontend_render_data_reader(void)
{
    /*
     * Entering the reader from the list keeps the already composed portrait
     * and selected-face palette; only the right text pane changes.
     */
    frontend_data_draw_reader_panel();
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_DATA_RIGHT_X,
        FRONTEND_DATA_RIGHT_Y,
        FRONTEND_DATA_RIGHT_WIDTH,
        FRONTEND_DATA_RIGHT_HEIGHT
    );
    frontend_dirty_present();
}

static void frontend_data_reader_scroll(s8 direction)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    u8 max_scroll =
        cube->last_line > FRONTEND_DATA_READER_VISIBLE_LINES ?
            (u8)(
                cube->last_line -
                FRONTEND_DATA_READER_VISIBLE_LINES
            ) :
            0;
    u8 old_scroll = frontend_data_scroll_line;

    if (direction < 0) {
        if (frontend_data_scroll_line != 0) {
            frontend_data_scroll_line--;
        }
    } else if (frontend_data_scroll_line < max_scroll) {
        frontend_data_scroll_line++;
    }
    if (old_scroll != frontend_data_scroll_line) {
        frontend_data_draw_reader_panel();
        frontend_dirty_begin();
        frontend_dirty_add(
            FRONTEND_DATA_RIGHT_X,
            FRONTEND_DATA_RIGHT_Y,
            FRONTEND_DATA_RIGHT_WIDTH,
            FRONTEND_DATA_RIGHT_HEIGHT
        );
        frontend_dirty_present();
        source_sound_effect(28); /* OpenTyrian S_CURSOR. */
    }
}

static void frontend_data_animate(void)
{
    u16 x0;
    u16 y0;
    u16 x1;
    u16 y1;
    s16 source_y;

    if (
        game_state != STATE_DATA_CUBES ||
        frontend_selection >= frontend_data_page_count ||
        frontend_frame_pending ||
        ++frontend_data_glow_wait < 3
    ) {
        return;
    }
    frontend_data_glow_wait = 0;
    frontend_data_glow =
        (s8)(frontend_data_glow + frontend_data_glow_delta);
    if (
        frontend_data_glow < -2 ||
        frontend_data_glow > 6
    ) {
        frontend_data_glow_delta =
            (s8)-frontend_data_glow_delta;
        frontend_data_glow =
            (s8)(frontend_data_glow + frontend_data_glow_delta);
    }

    source_y = (s16)(38 + frontend_selection * 28);
    frontend_source_rect_bounds(
        166,
        source_y,
        194,
        (s16)(source_y + 27),
        &x0,
        &y0,
        &x1,
        &y1
    );
    frontend_restore_menu_chrome_rect(
        x0,
        y0,
        (u16)(x1 - x0),
        (u16)(y1 - y0)
    );
    frontend_data_draw_cube(
        frontend_selection,
        frontend_data_glow
    );
    frontend_dirty_begin();
    frontend_dirty_add(
        x0,
        y0,
        (u16)(x1 - x0),
        (u16)(y1 - y0)
    );
    frontend_dirty_present();
}

static u8 frontend_ship_specs_prepare(void)
{
    u8 ship_id;

    if (!frontend_player_items_valid) return 0;
    ship_id = frontend_player_items.ship;
    (void)ot_data_episode_items_select((u8)(frontend_episode + 1u));
    if (
        !ot_data_hdt_ship_read(
            ship_id,
            &frontend_ship_specs_ship
        ) ||
        !ot_data_ship_info_read(
            ship_id,
            frontend_ship_info_cache
        )
    ) {
        return 0;
    }
    /* Ship Specs aliases the datacube arena tail. */
    frontend_data_cache_valid = 0;
    frontend_quit_dialog_cache_valid = 0;
    return 1;
}

static void frontend_ship_specs_draw_grid(void)
{
    u8 line;

    frontend_fill_rect(
        frontend_frame_scratch,
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );
    for (line = 0; line < 8; line++) {
        s16 source_x = (s16)(25 + line * 15);
        s16 bright_x = frontend_source_to_screen_x(source_x);
        s16 dark_x = frontend_source_to_screen_x(
            (s16)(source_x + 1)
        );

        if (dark_x >= 0 && dark_x < SCREEN_WIDTH) {
            u16 y;

            for (y = 0; y < SCREEN_HEIGHT; y++) {
                frontend_frame_scratch[
                    (u32)y * SCREEN_WIDTH + (u16)dark_x
                ] = 35;
            }
        }
        if (bright_x >= 0 && bright_x < SCREEN_WIDTH) {
            u16 y;

            for (y = 0; y < SCREEN_HEIGHT; y++) {
                frontend_frame_scratch[
                    (u32)y * SCREEN_WIDTH + (u16)bright_x
                ] = 37;
            }
        }
    }
    for (line = 0; line < 10; line++) {
        s16 source_y = (s16)(30 + line * 15);
        s16 bright_y = frontend_source_to_screen_y(source_y);
        s16 dark_y = frontend_source_to_screen_y(
            (s16)(source_y + 1)
        );

        if (dark_y >= 0 && dark_y < SCREEN_HEIGHT) {
            memset(
                frontend_frame_scratch +
                    (u32)dark_y * SCREEN_WIDTH,
                35,
                SCREEN_WIDTH
            );
        }
        if (bright_y >= 0 && bright_y < SCREEN_HEIGHT) {
            u8 vertical;

            memset(
                frontend_frame_scratch +
                    (u32)bright_y * SCREEN_WIDTH,
                37,
                SCREEN_WIDTH
            );
            for (vertical = 0; vertical < 8; vertical++) {
                s16 x = frontend_source_to_screen_x(
                    (s16)(25 + vertical * 15)
                );

                if (x >= 0 && x < SCREEN_WIDTH) {
                    frontend_frame_scratch[
                        (u32)bright_y * SCREEN_WIDTH + (u16)x
                    ] = 38;
                }
            }
        }
    }
    frontend_fill_rect(
        frontend_frame_scratch,
        0,
        0,
        SCREEN_WIDTH,
        1,
        37
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        0,
        SCREEN_HEIGHT - 1,
        SCREEN_WIDTH,
        1,
        37
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        0,
        0,
        1,
        SCREEN_HEIGHT,
        37
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        SCREEN_WIDTH - 1,
        0,
        1,
        SCREEN_HEIGHT,
        37
    );
}

static void frontend_render_ship_specs(void)
{
    u8 big_graphic = frontend_ship_specs_ship.bigshipgraphic;
    s16 ship_x;
    s16 ship_y;

    frontend_ship_specs_draw_grid();
    frontend_draw_menu_text_clipped(
        frontend_frame_scratch,
        frontend_ship_specs_ship.name,
        frontend_source_to_screen_x(10),
        frontend_source_to_screen_y(2),
        SCREEN_WIDTH,
        0xcbu
    );
    frontend_draw_small_mixed_text_wrapped(
        frontend_frame_scratch,
        frontend_ship_info_cache->paragraph[0],
        frontend_source_to_screen_x(100),
        frontend_source_to_screen_y(20),
        SCREEN_WIDTH - 3,
        8,
        9,
        0xc9u
    );
    frontend_draw_small_mixed_text_wrapped(
        frontend_frame_scratch,
        frontend_ship_info_cache->paragraph[1],
        frontend_source_to_screen_x(100),
        frontend_source_to_screen_y(100),
        SCREEN_WIDTH - 3,
        8,
        9,
        0xc9u
    );
    frontend_draw_small_mixed_text_clipped(
        frontend_frame_scratch,
        frontend_data_text(4, "Press a key"),
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_small_mixed_text_width(
                frontend_data_text(4, "Press a key")
            ) / 2
        ),
        frontend_source_to_screen_y(190),
        SCREEN_WIDTH,
        0xcau
    );

    if (big_graphic == 0) big_graphic = 32;
    switch (big_graphic) {
    case 28:
        ship_x = 1;
        ship_y = 36;
        break;
    case 33:
        ship_x = 1;
        ship_y = 35;
        break;
    case 32:
    default:
        ship_x = 5;
        ship_y = 33;
        big_graphic = 32;
        break;
    }
    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        (u16)(big_graphic - 1u),
        ship_x,
        ship_y,
        FRONTEND_SOURCE_BLIT_HUE,
        12,
        0
    );
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}
