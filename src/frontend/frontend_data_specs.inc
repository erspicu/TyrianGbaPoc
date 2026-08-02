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
    FRONTEND_DATA_LEFT_HEIGHT = 148,
    FRONTEND_DATA_RIGHT_X = 128,
    FRONTEND_DATA_RIGHT_Y = 28,
    FRONTEND_DATA_RIGHT_WIDTH = 112,
    FRONTEND_DATA_RIGHT_HEIGHT = 110,
    FRONTEND_DATA_TEXT_X = 154,
    FRONTEND_DATA_TEXT_RIGHT = 239,
    FRONTEND_DATA_TEXT_LINE_HEIGHT = 9,
    FRONTEND_DATA_READER_X = 129,
    FRONTEND_DATA_READER_Y = 40,
    FRONTEND_DATA_READER_LINE_HEIGHT = 9,
    /*
     * The PC loop attempts ten 12px rows, but its bottom status strip at
     * y=157 overwrites the tenth; the scroll percentage likewise divides by
     * last_line - 9.  Nine rows are therefore the true visible viewport.
     */
    FRONTEND_DATA_READER_VISIBLE_LINES = 9,
    /*
     * Source-font stamps are deliberately issued in small batches.  This
     * keeps each static-page construction step below one GBA display frame
     * while Maxmod continues servicing the menu song every VBlank.
     */
    FRONTEND_COOPERATIVE_TEXT_GLYPHS_PER_STEP = 16,
    FRONTEND_DATA_RECORDS_PER_STEP = 4,
    FRONTEND_DATA_STAMP_RUNS_PER_STEP = 16,
    FRONTEND_DATA_SELECTION_STAMP_RUNS_PER_STEP = 32,
    FRONTEND_DATA_READER_GLYPHS_PER_STEP = 48,
    FRONTEND_SHIP_ART_DECODE_ROWS_PER_STEP = 6,
    FRONTEND_SHIP_ART_FILTER_ROWS_PER_STEP = 6,
    FRONTEND_DATA_FACE_BASE_X = 34,
    FRONTEND_DATA_FACE_BASE_Y = 50,
    FRONTEND_DATA_FACE_BASE_WIDTH = 58,
    FRONTEND_DATA_FACE_BASE_HEIGHT = 48,
    FRONTEND_DATA_HEADER_BASE_X = 0,
    FRONTEND_DATA_HEADER_BASE_Y = 136,
    FRONTEND_DATA_HEADER_BASE_WIDTH = FRONTEND_DATA_LEFT_WIDTH,
    FRONTEND_DATA_HEADER_BASE_HEIGHT = 12,
    FRONTEND_DATA_FACE_BASE_BYTES =
        FRONTEND_DATA_FACE_BASE_WIDTH *
        FRONTEND_DATA_FACE_BASE_HEIGHT,
    FRONTEND_DATA_HEADER_BASE_BYTES =
        FRONTEND_DATA_HEADER_BASE_WIDTH *
        FRONTEND_DATA_HEADER_BASE_HEIGHT,
    FRONTEND_DATA_READER_FONT_GLYPHS =
        FRONTEND_PREGAME_FONT_GLYPH_COUNT + 1u,
    FRONTEND_DATA_READER_FONT_WIDTH_BYTES =
        FRONTEND_DATA_READER_FONT_GLYPHS,
    FRONTEND_DATA_READER_FONT_ROW_BYTES =
        FRONTEND_DATA_READER_FONT_GLYPHS * 5u,
};

typedef struct {
    const char *text;
    s16 x;
    s16 y;
    s16 right;
    s16 draw_x;
    u8 line_height;
    u8 max_lines;
    u8 colour;
    u8 shadow_colour;
    u8 lines;
    u8 line_length;
    u8 draw_index;
    u8 line_ready;
    /* 0 = shared 4x8 mixed face, 1 = source-like Data tiny face. */
    u8 font_mode;
    u8 highlight;
    u8 active;
    char line[67];
} FrontendWrappedTextJob;

typedef struct {
    OtShpSprite sprite;
    u32 position;
    u16 x;
    u16 y;
    u16 filter_row;
    s16 source_x;
    s16 source_y;
    u8 phase;
    u8 active;
} FrontendShipArtJob;

typedef struct {
    s16 draw_x;
    u8 first_line;
    u8 line_offset;
    u8 visual_line;
    u8 char_index;
    u8 highlight;
    u8 active;
} FrontendDataReaderPanelJob;

static OtDataCubeReader frontend_data_cube_reader EWRAM_BSS;
static FrontendSourceStampJob frontend_data_stamp_job EWRAM_BSS;
static FrontendWrappedTextJob frontend_data_text_job EWRAM_BSS;
static FrontendDataReaderPanelJob
    frontend_data_reader_panel_job EWRAM_BSS;
static FrontendWrappedTextJob
    frontend_ship_specs_text_job EWRAM_BSS;
static FrontendShipArtJob frontend_ship_specs_art_job EWRAM_BSS;
static u8 frontend_data_reader_font_ready EWRAM_BSS;
static u8 frontend_data_reader_font_prepare_index EWRAM_BSS;
static u8 frontend_data_portrait_base_valid EWRAM_BSS;

#define frontend_data_portrait_base_cache \
    ((u8 *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET + \
        sizeof(OtDataCube) * OT_EPISODE_CUBE_CAPACITY + \
        OT_PALETTE_COLOUR_COUNT * sizeof(u16) \
    ))
#define frontend_data_header_base_cache \
    ( \
        frontend_data_portrait_base_cache + \
        FRONTEND_DATA_FACE_BASE_BYTES \
    )
#define frontend_data_reader_font_width \
    ( \
        frontend_data_header_base_cache + \
        FRONTEND_DATA_HEADER_BASE_BYTES \
    )
#define frontend_data_reader_font_row \
    ((u8 (*)[5])(void *)( \
        frontend_data_reader_font_width + \
        FRONTEND_DATA_READER_FONT_WIDTH_BYTES \
    ))
_Static_assert(
    FRONTEND_COLD_PAGE_CACHE_OFFSET +
        sizeof(OtDataCube) * OT_EPISODE_CUBE_CAPACITY +
        OT_PALETTE_COLOUR_COUNT * sizeof(u16) +
        FRONTEND_DATA_FACE_BASE_BYTES +
        FRONTEND_DATA_HEADER_BASE_BYTES +
        FRONTEND_DATA_READER_FONT_WIDTH_BYTES +
        FRONTEND_DATA_READER_FONT_ROW_BYTES <=
        sizeof(FrontendGameplayArena),
    "Data portrait and reader-font caches must fit the shared arena tail"
);

static void frontend_wrapped_text_job_begin(
    FrontendWrappedTextJob *job,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 line_height,
    u8 max_lines,
    u8 colour
);
static u8 frontend_wrapped_text_job_step(
    FrontendWrappedTextJob *job
);
static void frontend_data_wrapped_text_job_begin(
    FrontendWrappedTextJob *job,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 line_height,
    u8 max_lines,
    u8 colour
);
static s16 frontend_data_tiny_text_width(const char *text);
static s16 frontend_data_fine_text_width(const char *text);
static void frontend_data_fine_text_clipped_colours(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour,
    u8 shadow_colour
);

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

enum {
    /* Zero-based mainMenuHelp[] entries used by PC game_menu.c. */
    FRONTEND_DATA_HELP_EXIT = 11,
    FRONTEND_DATA_HELP_LIST = 21,
    FRONTEND_DATA_HELP_READER = 22,
};

static u8 frontend_data_help_index(u8 reader)
{
    if (reader) return FRONTEND_DATA_HELP_READER;
    if (
        frontend_data_page_count == 0 ||
        frontend_selection >= frontend_data_page_count
    ) {
        return FRONTEND_DATA_HELP_EXIT;
    }
    return FRONTEND_DATA_HELP_LIST;
}

static const char *frontend_data_help_text(u8 reader)
{
    static const char *const fallback[3] = {
        "Return to the previous menu.",
        "Read the selected Data Cube.",
        "Scroll through this Data Cube.",
    };
    u8 source_index = frontend_data_help_index(reader);
    u8 fallback_index = reader ? 2u :
        (source_index == FRONTEND_DATA_HELP_EXIT ? 0u : 1u);

    if (
        frontend_text_ready &&
        source_index < 34u &&
        frontend_text.main_menu_help[source_index][0] != '\0'
    ) {
        return frontend_text.main_menu_help[source_index];
    }
    return fallback[fallback_index];
}

static void frontend_data_draw_help(u8 reader)
{
    u8 source_index = frontend_data_help_index(reader);

    frontend_draw_menu_help(
        source_index,
        frontend_data_help_text(reader)
    );
}

static const char *frontend_data_menu_header(u8 reader)
{
    if (
        reader &&
        frontend_data_reader_index < frontend_data_page_count
    ) {
        return frontend_data_cube_cache[
            frontend_data_reader_index
        ].header;
    }
    return frontend_game_menu_label(1);
}

/* Direct translation of JE_drawMenuHeader() for Data / Data Cube Sub. */
static void frontend_data_draw_menu_header(u8 reader)
{
    const u16 x = frontend_source_to_screen_x(150);
    const u16 height = frontend_source_to_screen_y(30);

    frontend_restore_menu_chrome_rect(
        x,
        0,
        (u16)(SCREEN_WIDTH - x),
        height
    );
    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        frontend_data_menu_header(reader),
        frontend_source_to_screen_x(232),
        frontend_source_to_screen_y(10),
        0xfbu
    );
}

static u8 frontend_data_source_count(void)
{
    return
        frontend_data_cube_count > OT_EPISODE_CUBE_CAPACITY ?
            OT_EPISODE_CUBE_CAPACITY :
            frontend_data_cube_count;
}

static void frontend_data_prepare_begin(void)
{
    frontend_data_page_count = 0;
    frontend_data_cache_valid = 0;
    frontend_data_reader_index = 0;
    frontend_data_scroll_pixel = 0;
    frontend_data_scroll_target = 0;
    frontend_data_glow = 1;
    frontend_data_glow_delta = 1;
    frontend_data_glow_wait = 0;
    frontend_quit_dialog_cache_valid = 0;
    frontend_data_cube_reader = (OtDataCubeReader){0};
    frontend_data_stamp_job = (FrontendSourceStampJob){0};
    frontend_data_text_job = (FrontendWrappedTextJob){0};
    frontend_data_reader_panel_job =
        (FrontendDataReaderPanelJob){0};
    frontend_data_reader_font_ready = 0;
    frontend_data_reader_font_prepare_index = 0;
    frontend_data_portrait_base_valid = 0;
}

static void frontend_data_prepare_one(u8 source_index)
{
    u8 cube_index;

    if (source_index >= frontend_data_source_count()) return;
    cube_index = frontend_data_cube_list[source_index];
    if (
        cube_index != 0 &&
        ot_data_cube_read(
            (u8)(frontend_episode + 1u),
            cube_index,
            &frontend_data_cube_cache[frontend_data_page_count]
        )
    ) {
        frontend_data_page_count++;
    }
}

static void frontend_data_prepare_finish(void)
{
    frontend_data_cache_valid = 1;
}

static u8 frontend_data_prepare(void)
{
    u8 source_index;

    frontend_data_prepare_begin();
    for (
        source_index = 0;
        source_index < frontend_data_source_count();
        source_index++
    ) {
        frontend_data_prepare_one(source_index);
    }
    frontend_data_prepare_finish();
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
    u8 layer;

    for (layer = 0; layer < 3; layer++) {
        s16 source_x = 166;
        s16 source_y = (s16)(38 + cube_slot * 28);

        if (layer == 0) {
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
        } else if (layer == 1) {
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
        } else {
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
    }
}

static void frontend_data_draw_cube_layer(
    u8 cube_slot,
    s8 glow,
    u8 layer
)
{
    s16 source_x = 166;
    s16 source_y = (s16)(38 + cube_slot * 28);

    if (layer == 0) {
        source_x = (s16)(source_x + 4);
        source_y = (s16)(source_y + 4);
    } else if (layer == 1) {
        source_x = (s16)(source_x + 3);
        source_y = (s16)(source_y + 3);
    }
    (void)frontend_blit_shp_source(
        frontend_frame_scratch,
        5,
        25,
        source_x,
        source_y,
        layer < 2 ?
            FRONTEND_SOURCE_BLIT_DARK :
            FRONTEND_SOURCE_BLIT_HUE,
        layer < 2 ? 0 : 13,
        layer < 2 ? 0 : glow
    );
}

static u8 frontend_data_cube_layer_begin(
    u8 cube_slot,
    s8 glow,
    u8 layer
)
{
    s16 source_x = 166;
    s16 source_y = (s16)(38 + cube_slot * 28);

    if (layer == 0) {
        source_x = (s16)(source_x + 4);
        source_y = (s16)(source_y + 4);
    } else if (layer == 1) {
        source_x = (s16)(source_x + 3);
        source_y = (s16)(source_y + 3);
    }
    return frontend_source_stamp_job_begin(
        &frontend_data_stamp_job,
        frontend_frame_scratch,
        frontend_source_stamp_shp_key(5, 25),
        source_x,
        source_y,
        layer < 2 ?
            FRONTEND_SOURCE_BLIT_DARK :
            FRONTEND_SOURCE_BLIT_HUE,
        layer < 2 ? 0 : 13,
        layer < 2 ? 0 : glow
    );
}

static void frontend_data_draw_portrait_frame(void)
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
}

static void frontend_data_portrait_base_capture_rect(
    u8 *cache,
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        memcpy(
            cache + (u32)row * width,
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH + x,
            width
        );
    }
}

static void frontend_data_portrait_base_restore_rect(
    const u8 *cache,
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH + x,
            cache + (u32)row * width,
            width
        );
    }
}

static void frontend_data_portrait_base_capture(void)
{
    frontend_data_portrait_base_capture_rect(
        frontend_data_portrait_base_cache,
        FRONTEND_DATA_FACE_BASE_X,
        FRONTEND_DATA_FACE_BASE_Y,
        FRONTEND_DATA_FACE_BASE_WIDTH,
        FRONTEND_DATA_FACE_BASE_HEIGHT
    );
    frontend_data_portrait_base_capture_rect(
        frontend_data_header_base_cache,
        FRONTEND_DATA_HEADER_BASE_X,
        FRONTEND_DATA_HEADER_BASE_Y,
        FRONTEND_DATA_HEADER_BASE_WIDTH,
        FRONTEND_DATA_HEADER_BASE_HEIGHT
    );
    frontend_data_portrait_base_valid = 1;
}

static u8 frontend_data_portrait_base_restore(void)
{
    if (!frontend_data_portrait_base_valid) return 0;
    frontend_data_portrait_base_restore_rect(
        frontend_data_portrait_base_cache,
        FRONTEND_DATA_FACE_BASE_X,
        FRONTEND_DATA_FACE_BASE_Y,
        FRONTEND_DATA_FACE_BASE_WIDTH,
        FRONTEND_DATA_FACE_BASE_HEIGHT
    );
    frontend_data_portrait_base_restore_rect(
        frontend_data_header_base_cache,
        FRONTEND_DATA_HEADER_BASE_X,
        FRONTEND_DATA_HEADER_BASE_Y,
        FRONTEND_DATA_HEADER_BASE_WIDTH,
        FRONTEND_DATA_HEADER_BASE_HEIGHT
    );
    return 1;
}

static void frontend_data_draw_selected_face(void)
{
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
    }
}

static void frontend_data_draw_selected_header(void)
{
    if (
        frontend_selection < frontend_data_page_count &&
        frontend_data_cache_valid
    ) {
        const OtDataCube *cube =
            &frontend_data_cube_cache[frontend_selection];

        frontend_data_fine_text_clipped_colours(
            frontend_frame_scratch,
            cube->header,
            (s16)(
                frontend_source_to_screen_x(75) -
                frontend_data_fine_text_width(cube->header) / 2
            ),
            frontend_source_to_screen_y(173),
            FRONTEND_DATA_LEFT_WIDTH,
            0xedu,
            0xe2u
        );
    }
}

static void frontend_data_draw_selected_portrait(void)
{
    frontend_data_draw_selected_face();
    frontend_data_draw_selected_header();
}

/*
 * One means a build-time face stamp was started, zero means the selected
 * item has no portrait, and -1 asks the caller to use the bounded fallback.
 */
static s8 frontend_data_selected_face_job_begin(void)
{
    const OtDataCube *cube;
    OtShpSprite face;

    if (
        frontend_selection >= frontend_data_page_count ||
        !frontend_data_cache_valid
    ) {
        return 0;
    }
    cube = &frontend_data_cube_cache[frontend_selection];
    if (
        cube->face_sprite < 0 ||
        !ot_data_shp_sprite_read(
            4,
            (u16)cube->face_sprite,
            &face
        ) ||
        !face.populated
    ) {
        return 0;
    }
    if (!frontend_source_stamp_job_begin(
            &frontend_data_stamp_job,
            frontend_frame_scratch,
            frontend_source_stamp_shp_key(
                4,
                (u16)cube->face_sprite
            ),
            (s16)(77 - (s16)face.width / 2),
            (s16)(92 - (s16)face.height / 2),
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        )) {
        return -1;
    }
    return 1;
}

static void frontend_data_selected_header_job_begin(void)
{
    const char *header = "";
    s16 x = 0;

    if (
        frontend_selection < frontend_data_page_count &&
        frontend_data_cache_valid
    ) {
        header =
            frontend_data_cube_cache[frontend_selection].header;
        x = (s16)(
            frontend_source_to_screen_x(75) -
            frontend_data_fine_text_width(header) / 2
        );
    }
    frontend_data_wrapped_text_job_begin(
        &frontend_data_text_job,
        header,
        x,
        frontend_source_to_screen_y(173),
        FRONTEND_DATA_LEFT_WIDTH,
        8,
        1,
        0xedu
    );
    frontend_data_text_job.shadow_colour = 0xe2u;
}

static void frontend_data_draw_list_title(u8 cube_slot)
{
    u8 colour =
        cube_slot == frontend_selection ? 0xfcu : 0xfau;

    frontend_data_wrapped_text_job_begin(
        &frontend_data_text_job,
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
    while (!frontend_wrapped_text_job_step(
        &frontend_data_text_job
    )) {
        /* Synchronous fallback; transitions use the bounded job. */
    }
}

static void frontend_data_list_title_begin(u8 cube_slot)
{
    u8 colour =
        cube_slot == frontend_selection ? 0xfcu : 0xfau;

    frontend_data_wrapped_text_job_begin(
        &frontend_data_text_job,
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

static void frontend_data_draw_list_item(u8 cube_slot)
{
    frontend_data_draw_cube(
        cube_slot,
        cube_slot == frontend_selection ?
            frontend_data_glow :
            0
    );
    frontend_data_draw_list_title(cube_slot);
}

static void frontend_data_draw_exit_item(void)
{
    u8 done_slot = frontend_data_page_count;
    s16 done_source_y = frontend_data_page_count == 0 ?
        160 :
        (s16)(44 + done_slot * 28);

    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        frontend_data_text(5, "Exit to Game Menu"),
        frontend_source_to_screen_x(172),
        frontend_source_to_screen_y(done_source_y),
        SCREEN_WIDTH,
        done_slot == frontend_selection ? 0xfcu : 0xfau,
        FRONTEND_NEXT_SHADOW_COLOUR
    );
}

static void frontend_data_exit_text_begin(void)
{
    u8 done_slot = frontend_data_page_count;
    s16 done_source_y = frontend_data_page_count == 0 ?
        160 :
        (s16)(44 + done_slot * 28);

    frontend_data_wrapped_text_job_begin(
        &frontend_data_text_job,
        frontend_data_text(5, "Exit to Game Menu"),
        frontend_source_to_screen_x(172),
        frontend_source_to_screen_y(done_source_y),
        SCREEN_WIDTH,
        8,
        1,
        done_slot == frontend_selection ? 0xfcu : 0xfau
    );
}

/*
 * Repaint one list row without rebuilding the other unchanged rows.  Cube
 * stamps and title glyphs remain cooperative, so a D-pad press cannot turn
 * into one long Game Pak read/scale burst.
 */
static u8 frontend_data_selection_item_job_step(u8 cube_slot)
{
    if (cube_slot >= frontend_data_page_count) {
        if (frontend_transition_work_index == 0) {
            frontend_data_exit_text_begin();
            frontend_transition_work_index = 1;
        }
        return frontend_wrapped_text_job_step(
            &frontend_data_text_job
        );
    }
    if (frontend_transition_work_index < 3) {
        s8 glow =
            cube_slot == frontend_selection ?
                frontend_data_glow :
                0;

        if (!frontend_data_stamp_job.active) {
            if (!frontend_data_cube_layer_begin(
                    cube_slot,
                    glow,
                    frontend_transition_work_index
                )) {
                frontend_data_draw_cube_layer(
                    cube_slot,
                    glow,
                    frontend_transition_work_index++
                );
                return 0;
            }
        }
        {
            s8 result = frontend_source_stamp_job_step(
                &frontend_data_stamp_job,
                FRONTEND_DATA_SELECTION_STAMP_RUNS_PER_STEP
            );

            if (result != 0) {
                if (result < 0) {
                    frontend_data_draw_cube_layer(
                        cube_slot,
                        glow,
                        frontend_transition_work_index
                    );
                }
                frontend_transition_work_index++;
            }
        }
        return 0;
    }
    if (frontend_transition_work_index == 3) {
        frontend_data_list_title_begin(cube_slot);
        frontend_transition_work_index = 4;
    }
    return frontend_wrapped_text_job_step(
        &frontend_data_text_job
    );
}

static void frontend_data_draw_empty_message(void)
{
    frontend_data_wrapped_text_job_begin(
        &frontend_data_text_job,
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
    while (!frontend_wrapped_text_job_step(
        &frontend_data_text_job
    )) {
        /* Synchronous fallback; transitions use the bounded job. */
    }
}

static void frontend_data_draw_list(void)
{
    u8 cube_slot;

    if (frontend_data_page_count == 0) {
        frontend_data_draw_empty_message();
    } else {
        for (
            cube_slot = 0;
            cube_slot < frontend_data_page_count;
            cube_slot++
        ) {
            frontend_data_draw_list_item(cube_slot);
        }
    }
    frontend_data_draw_exit_item();
}

static void frontend_data_compose_list(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_data_draw_portrait_frame();
    frontend_data_portrait_base_capture();
    frontend_data_draw_selected_portrait();
    frontend_data_draw_list();
    frontend_data_draw_menu_header(0);
    frontend_data_draw_help(0);
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
        FRONTEND_DATA_RIGHT_X,
        FRONTEND_DATA_RIGHT_Y,
        FRONTEND_DATA_RIGHT_WIDTH,
        FRONTEND_DATA_RIGHT_HEIGHT
    );
    if (!frontend_data_portrait_base_restore()) {
        frontend_data_draw_portrait_frame();
        frontend_data_portrait_base_capture();
    }
    frontend_data_draw_selected_portrait();
    frontend_data_draw_list();
    frontend_data_draw_help(0);
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
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present_with_palette(frontend_data_face_palette);
}

/*
 * Compact episode-scene layouts can use a 3/4x5 projection of the shared
 * mixed-case face.  Build those masks once in EWRAM; Data pages deliberately
 * retain the clearer eight authored rows through frontend_data_fine_*().
 * Wide M/W/m/w glyphs keep four columns so "Mom" cannot collapse into "Non".
 */
static u8 __attribute__((unused))
frontend_data_reader_font_prepare_step(u8 glyph_budget)
{
    static const u8 source_y0[5] = {0, 1, 3, 4, 6};
    static const u8 source_y1[5] = {2, 4, 5, 7, 8};
    u8 completed = 0;

    if (frontend_data_reader_font_ready) return 1;
    if (frontend_data_reader_font_prepare_index == 0) {
        frontend_data_reader_font_prepare_index = 1;
    }
    while (
        frontend_data_reader_font_prepare_index <=
            FRONTEND_PREGAME_FONT_GLYPH_COUNT &&
        completed < glyph_budget
    ) {
        u8 tile = frontend_data_reader_font_prepare_index++;
        u8 source_width = frontend_pregame_font_width(tile);
        u8 target_width =
            source_width > 4 ? 4 : source_width;
        u8 target_row;

        frontend_data_reader_font_width[tile] = target_width;
        for (target_row = 0; target_row < 5; target_row++) {
            u8 target_column;
            u8 packed = 0;

            for (
                target_column = 0;
                target_column < target_width;
                target_column++
            ) {
                u8 source_x0 = (u8)(
                    (u16)target_column *
                        source_width / target_width
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
                    source_y = source_y0[target_row];
                    source_y < source_y1[target_row] &&
                        !occupied;
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
                    packed |= (u8)(
                        1u <<
                        (
                            target_width -
                            target_column -
                            1u
                        )
                    );
                }
            }
            frontend_data_reader_font_row[tile][target_row] =
                packed;
        }
        completed++;
    }
    if (
        frontend_data_reader_font_prepare_index >
            FRONTEND_PREGAME_FONT_GLYPH_COUNT
    ) {
        static const u8 uppercase_m[5] = {
            0x9, 0xf, 0xf, 0x9, 0x9
        };
        static const u8 uppercase_n[5] = {
            0x9, 0xd, 0xd, 0xb, 0x9
        };
        static const u8 lowercase_m[5] = {
            0x0, 0x0, 0xe, 0xb, 0xb
        };
        static const u8 lowercase_n[5] = {
            0x0, 0x0, 0xe, 0x9, 0x9
        };

        frontend_data_reader_font_width[13] = 4; /* M */
        frontend_data_reader_font_width[14] = 4; /* N */
        frontend_data_reader_font_width[39] = 4; /* m */
        frontend_data_reader_font_width[40] = 4; /* n */
        memcpy(frontend_data_reader_font_row[13], uppercase_m, 5);
        memcpy(frontend_data_reader_font_row[14], uppercase_n, 5);
        memcpy(frontend_data_reader_font_row[39], lowercase_m, 5);
        memcpy(frontend_data_reader_font_row[40], lowercase_n, 5);
        frontend_data_reader_font_ready = 1;
    }
    return frontend_data_reader_font_ready;
}

static u8 frontend_data_tiny_font_advance(u8 tile)
{
    if (tile == 0) return 2;
    return (u8)(frontend_data_reader_font_width[tile] + 1u);
}

static s16 frontend_data_tiny_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        char character = *text++;

        /* PC JE_outText() treats '~' as brightness control, not a glyph. */
        if (character == '~') continue;
        width = (s16)(
            width +
            frontend_data_tiny_font_advance(
                frontend_pregame_font_tile(character)
            )
        );
    }
    return width;
}

static u8 frontend_data_tiny_highlight_colour(u8 colour)
{
    u8 brightness = (u8)(colour & 0x0fu);

    brightness = brightness > 13u ? 15u : (u8)(brightness + 2u);
    return (u8)((colour & 0xf0u) | brightness);
}

/*
 * Data pages use the same thin 4x8 mixed-case face as the clear episode
 * scene reference.  Keep Data's source '~' brightness control, but do not
 * collapse eight authored rows into the dense five-row projection.
 */
static s16 frontend_data_fine_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        char character = *text++;

        if (character == '~') continue;
        width = (s16)(
            width +
            frontend_small_mixed_font_advance(
                frontend_pregame_font_tile(character)
            )
        );
    }
    return width;
}

static void frontend_data_fine_text_clipped_colours(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour,
    u8 shadow_colour
)
{
    u8 highlight = 0;

    while (text != 0 && *text != '\0' && x < right) {
        char character = *text++;
        u8 tile;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile = frontend_pregame_font_tile(character);
        advance = frontend_small_mixed_font_advance(tile);
        if (x + advance > right) break;
        if (tile != 0) {
            frontend_blit_small_mixed_font(
                frame,
                tile,
                (s16)(x + 1),
                (s16)(y + 1),
                shadow_colour
            );
            frontend_blit_small_mixed_font(
                frame,
                tile,
                x,
                y,
                highlight ?
                    frontend_data_tiny_highlight_colour(colour) :
                    colour
            );
        }
        x = (s16)(x + advance);
    }
}

/*
 * Ship Specs is drawn through JE_helpBox() -> JE_textShade() on PC.
 * JE_outText() treats '~' as a +4 brightness toggle rather than a glyph.
 * Keep the marker in the source-backed string, but exclude it from the GBA
 * adapter's measured width and reproduce the same colour-bank operation.
 */
static s16 frontend_ship_specs_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        char character = *text++;

        if (character == '~') continue;
        width = (s16)(
            width +
            frontend_small_mixed_font_advance(
                frontend_pregame_font_tile(character)
            )
        );
    }
    return width;
}

static u8 frontend_ship_specs_highlight_colour(u8 colour)
{
    u8 brightness = (u8)(colour & 0x0fu);

    brightness = brightness > 11u ? 15u : (u8)(brightness + 4u);
    return (u8)((colour & 0xf0u) | brightness);
}

static void frontend_data_tiny_draw_glyph(
    u8 *frame,
    u8 tile,
    s16 x,
    s16 y,
    s16 right,
    u8 colour,
    u8 shadow_colour
)
{
    u8 width = frontend_data_reader_font_width[tile];
    u8 row;

    if (width == 0) return;
    for (row = 0; row < 5; row++) {
        u8 packed = frontend_data_reader_font_row[tile][row];
        u8 column;
        s16 pixel_y = (s16)(y + row);

        if (pixel_y < 0 || pixel_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < width; column++) {
            s16 pixel_x = (s16)(x + column);

            if (
                (
                    packed &
                    (1u << (width - column - 1u))
                ) == 0 ||
                pixel_x < 0 ||
                pixel_x >= right ||
                pixel_x >= SCREEN_WIDTH
            ) {
                continue;
            }
            if (
                pixel_x + 1 < right &&
                pixel_x + 1 < SCREEN_WIDTH &&
                pixel_y + 1 < SCREEN_HEIGHT
            ) {
                frame[
                    (u32)(pixel_y + 1) * SCREEN_WIDTH +
                    (u16)(pixel_x + 1)
                ] = shadow_colour;
            }
            frame[
                (u32)pixel_y * SCREEN_WIDTH +
                (u16)pixel_x
            ] = colour;
        }
    }
}

static void frontend_data_reader_draw_glyph(
    u8 tile,
    s16 x,
    s16 y,
    u8 colour
)
{
    frontend_blit_small_mixed_font(
        frontend_frame_scratch,
        tile,
        (s16)(x + 1),
        (s16)(y + 1),
        0xe2u
    );
    frontend_blit_small_mixed_font(
        frontend_frame_scratch,
        tile,
        x,
        y,
        colour
    );
}

static void frontend_data_draw_reader_chrome(void);

static void frontend_data_reader_draw_line(u8 source_line, s16 y)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    if (source_line >= cube->last_line) return;
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        cube->text[source_line],
        FRONTEND_DATA_READER_X,
        y,
        SCREEN_WIDTH,
        0xedu,
        0xe2u
    );
}

static void frontend_data_reader_clear_text_rows(s16 y, s16 height)
{
    const s16 top = frontend_source_to_screen_y(49);
    const s16 bottom = frontend_source_to_screen_y(157);
    s16 end = (s16)(y + height);

    if (y < top) y = top;
    if (end > bottom) end = bottom;
    while (y < end) {
        memset(
            frontend_frame_scratch +
                (u32)y * SCREEN_WIDTH +
                FRONTEND_DATA_RIGHT_X,
            228,
            SCREEN_WIDTH - FRONTEND_DATA_RIGHT_X
        );
        y++;
    }
}

static void frontend_data_reader_shift_text_rows(s8 direction)
{
    const s16 top = frontend_source_to_screen_y(49);
    const s16 bottom = frontend_source_to_screen_y(157);
    const u16 width = SCREEN_WIDTH - FRONTEND_DATA_RIGHT_X;

    if (direction < 0) {
        s16 y;

        for (y = top; y + 1 < bottom; y++) {
            memcpy(
                frontend_frame_scratch +
                    (u32)y * SCREEN_WIDTH +
                    FRONTEND_DATA_RIGHT_X,
                frontend_frame_scratch +
                    (u32)(y + 1) * SCREEN_WIDTH +
                    FRONTEND_DATA_RIGHT_X,
                width
            );
        }
        frontend_data_reader_clear_text_rows((s16)(bottom - 1), 1);
    } else if (direction > 0) {
        s16 y;

        for (y = (s16)(bottom - 1); y > top; y--) {
            memcpy(
                frontend_frame_scratch +
                    (u32)y * SCREEN_WIDTH +
                    FRONTEND_DATA_RIGHT_X,
                frontend_frame_scratch +
                    (u32)(y - 1) * SCREEN_WIDTH +
                    FRONTEND_DATA_RIGHT_X,
                width
            );
        }
        frontend_data_reader_clear_text_rows(top, 1);
    }
}

/*
 * The PC reader scrolls one source pixel at a time.  Rebuilding all ten text
 * rows for each pixel caused the long-cube audio stutter.  Most projected
 * glyph rows move by the same one-pixel delta, so shift the solid text pane
 * in EWRAM and redraw only the minority whose 4/5 scaling phase differs.
 * Header/footer masking remains the exact source operation.
 */
static void frontend_data_reader_scroll_incremental(
    u16 old_scroll,
    u16 new_scroll
)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    const s16 top = frontend_source_to_screen_y(49);
    const s16 bottom = frontend_source_to_screen_y(157);
    u8 first =
        (u8)(
            (
                old_scroll < new_scroll ?
                    old_scroll :
                    new_scroll
            ) / 12u
        );
    u8 last =
        (u8)(
            (
                old_scroll > new_scroll ?
                    old_scroll :
                    new_scroll
            ) / 12u +
            FRONTEND_DATA_READER_VISIBLE_LINES +
            2u
        );
    u8 stationary = 0;
    u8 moved = 0;
    s8 common_delta;
    u8 line;

    if (first > 0) first--;
    if (last > cube->last_line) last = cube->last_line;
    for (line = first; line < last; line++) {
        s16 old_y = frontend_source_to_screen_y(
            (s16)(50 + (u16)line * 12u - old_scroll)
        );
        s16 new_y = frontend_source_to_screen_y(
            (s16)(50 + (u16)line * 12u - new_scroll)
        );

        if (
            old_y + 6 <= top ||
            old_y >= bottom
        ) {
            if (new_y + 6 <= top || new_y >= bottom) continue;
        }
        if (new_y == old_y) {
            stationary++;
        } else {
            moved++;
        }
    }
    if (moved > stationary) {
        common_delta = new_scroll > old_scroll ? -1 : 1;
    } else {
        common_delta = 0;
    }
    frontend_data_reader_shift_text_rows(common_delta);
    for (line = first; line < last; line++) {
        s16 old_y = frontend_source_to_screen_y(
            (s16)(50 + (u16)line * 12u - old_scroll)
        );
        s16 new_y = frontend_source_to_screen_y(
            (s16)(50 + (u16)line * 12u - new_scroll)
        );
        s16 shifted_y = (s16)(old_y + common_delta);
        u8 old_visible = old_y + 6 > top && old_y < bottom;
        u8 new_visible = new_y + 6 > top && new_y < bottom;

        if (
            new_y - old_y == common_delta &&
            old_visible == new_visible
        ) {
            continue;
        }
        {
            s16 clear_y =
                shifted_y < new_y ? shifted_y : new_y;
            s16 clear_end =
                shifted_y > new_y ? shifted_y : new_y;

            frontend_data_reader_clear_text_rows(
                clear_y,
                (s16)(clear_end - clear_y + 6)
            );
        }
        if (new_visible) {
            frontend_data_reader_draw_line(line, new_y);
        }
    }
    frontend_data_draw_reader_chrome();
}

static void frontend_data_reader_panel_job_begin(void)
{
    frontend_data_reader_panel_job =
        (FrontendDataReaderPanelJob){0};
    frontend_data_reader_panel_job.first_line =
        (u8)(frontend_data_scroll_pixel / 12u);
    frontend_data_reader_panel_job.line_offset =
        (u8)(frontend_data_scroll_pixel % 12u);
    frontend_data_reader_panel_job.draw_x =
        FRONTEND_DATA_READER_X;
    frontend_data_reader_panel_job.active = 1;

    frontend_fill_source_rect(
        frontend_frame_scratch,
        160,
        39,
        310,
        166,
        228
    );
}

static u8 frontend_data_reader_panel_job_step(u8 glyph_budget)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    u8 glyphs = 0;

    while (
        frontend_data_reader_panel_job.active &&
        glyphs < glyph_budget
    ) {
        u8 source_line;
        const char *text;
        u8 tile;
        u8 target_width;
        s16 y;

        if (
            frontend_data_reader_panel_job.visual_line >=
                FRONTEND_DATA_READER_VISIBLE_LINES + 1u
        ) {
            frontend_data_reader_panel_job.active = 0;
            break;
        }
        source_line = (u8)(
            frontend_data_reader_panel_job.first_line +
            frontend_data_reader_panel_job.visual_line
        );
        if (source_line >= cube->last_line) {
            frontend_data_reader_panel_job.active = 0;
            break;
        }
        text = cube->text[source_line];
        if (
            text[frontend_data_reader_panel_job.char_index] ==
                '\0' ||
            frontend_data_reader_panel_job.draw_x >= SCREEN_WIDTH
        ) {
            frontend_data_reader_panel_job.visual_line++;
            frontend_data_reader_panel_job.char_index = 0;
            frontend_data_reader_panel_job.draw_x =
                FRONTEND_DATA_READER_X;
            frontend_data_reader_panel_job.highlight = 0;
            continue;
        }
        {
            char character = text[
                frontend_data_reader_panel_job.char_index++
            ];

            if (character == '~') {
                frontend_data_reader_panel_job.highlight ^= 1u;
                continue;
            }
            tile = frontend_pregame_font_tile(character);
        }
        target_width = frontend_small_mixed_font_advance(tile);
        if (
            frontend_data_reader_panel_job.draw_x +
                target_width >
            SCREEN_WIDTH
        ) {
            frontend_data_reader_panel_job.visual_line++;
            frontend_data_reader_panel_job.char_index = 0;
            frontend_data_reader_panel_job.draw_x =
                FRONTEND_DATA_READER_X;
            frontend_data_reader_panel_job.highlight = 0;
            continue;
        }
        y = frontend_source_to_screen_y(
            (s16)(
                50 +
                frontend_data_reader_panel_job.visual_line *
                    12u -
                frontend_data_reader_panel_job.line_offset
            )
        );
        if (tile != 0) {
            frontend_data_reader_draw_glyph(
                tile,
                frontend_data_reader_panel_job.draw_x,
                y,
                frontend_data_reader_panel_job.highlight ?
                    frontend_data_tiny_highlight_colour(0xedu) :
                    0xedu
            );
        }
        frontend_data_reader_panel_job.draw_x = (s16)(
            frontend_data_reader_panel_job.draw_x +
            target_width
        );
        glyphs++;
    }
    return !frontend_data_reader_panel_job.active;
}

static void frontend_data_draw_reader_chrome(void)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    u16 max_scroll_pixels =
        cube->last_line > FRONTEND_DATA_READER_VISIBLE_LINES ?
            (u16)(
                (
                    cube->last_line -
                    FRONTEND_DATA_READER_VISIBLE_LINES
                ) * 12u
            ) :
            0;
    u8 percent =
        max_scroll_pixels == 0 ?
            100 :
            (u8)(
                (u32)frontend_data_scroll_pixel * 100u /
                max_scroll_pixels
            );
    char percent_text[48];
    char percent_prefix[44];
    u8 length;

    /*
     * PC redraws the fixed header/footer bands after the smoothly moving
     * text.  This masks the partial first/tenth line at exactly the source
     * viewport edges instead of snapping one full wrapped line per input.
     */
    frontend_fill_source_rect(
        frontend_frame_scratch,
        160,
        39,
        310,
        48,
        228
    );
    frontend_fill_source_rect(
        frontend_frame_scratch,
        160,
        157,
        310,
        166,
        228
    );
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
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        percent_text,
        frontend_source_to_screen_x(176),
        frontend_source_to_screen_y(160),
        frontend_source_to_screen_x(254),
        0xebu,
        0xe2u
    );
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        frontend_data_text(12, "EXIT"),
        frontend_source_to_screen_x(260),
        frontend_source_to_screen_y(160),
        SCREEN_WIDTH,
        0xfeu,
        0xf2u
    );
}

static void frontend_data_draw_reader_panel(void)
{
    frontend_data_reader_panel_job_begin();
    while (!frontend_data_reader_panel_job_step(255)) {
        /* Synchronous fallback; transitions use the bounded step directly. */
    }
    frontend_data_draw_reader_chrome();
}

static void frontend_render_data_reader(void)
{
    /*
     * Entering the reader from the list keeps the already composed portrait
     * and selected-face palette; only the right text pane changes.
     */
    frontend_data_draw_reader_panel();
    frontend_data_draw_menu_header(1);
    frontend_data_draw_help(1);
    frontend_dirty_begin();
    frontend_dirty_add(
        frontend_source_to_screen_x(150),
        0,
        (u16)(SCREEN_WIDTH - frontend_source_to_screen_x(150)),
        frontend_source_to_screen_y(30)
    );
    frontend_dirty_add(
        FRONTEND_DATA_RIGHT_X,
        FRONTEND_DATA_RIGHT_Y,
        FRONTEND_DATA_RIGHT_WIDTH,
        FRONTEND_DATA_RIGHT_HEIGHT
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static void frontend_data_reader_scroll(s8 direction)
{
    const OtDataCube *cube =
        &frontend_data_cube_cache[frontend_data_reader_index];
    u16 max_scroll =
        cube->last_line > FRONTEND_DATA_READER_VISIBLE_LINES ?
            (u16)(
                (
                    cube->last_line -
                    FRONTEND_DATA_READER_VISIBLE_LINES
                ) * 12u
            ) :
            0;
    u16 target = frontend_data_scroll_target;

    if (direction < 0) {
        target = target >= 12u ? (u16)(target - 12u) : 0;
    } else if (target < max_scroll) {
        target = (u16)(target + 12u);
        if (target > max_scroll) target = max_scroll;
    }
    if (target != frontend_data_scroll_target) {
        frontend_data_scroll_target = target;
        source_sound_effect(28); /* OpenTyrian S_CURSOR. */
    }
}

static void frontend_data_reader_animate(void)
{
    u16 old_scroll;

    if (
        game_state != STATE_DATA_CUBE_READER ||
        frontend_frame_pending ||
        frontend_data_scroll_pixel == frontend_data_scroll_target
    ) {
        return;
    }
    old_scroll = frontend_data_scroll_pixel;
    if (frontend_data_scroll_pixel < frontend_data_scroll_target) {
        frontend_data_scroll_pixel++;
    } else {
        frontend_data_scroll_pixel--;
    }
    {
        frontend_data_reader_scroll_incremental(
            old_scroll,
            frontend_data_scroll_pixel
        );
        frontend_dirty_begin();
        frontend_dirty_add(
            FRONTEND_DATA_RIGHT_X,
            FRONTEND_DATA_RIGHT_Y,
            FRONTEND_DATA_RIGHT_WIDTH,
            FRONTEND_DATA_RIGHT_HEIGHT
        );
        frontend_dirty_present();
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

static void frontend_ship_specs_clear(void)
{
    frontend_fill_rect(
        frontend_frame_scratch,
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );
}

static void frontend_ship_specs_draw_grid_vertical(void)
{
    u8 line;

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

static void frontend_ship_specs_draw_grid_horizontal(void)
{
    u8 line;

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
}

static void frontend_ship_specs_draw_grid(void)
{
    frontend_ship_specs_clear();
    frontend_ship_specs_draw_grid_vertical();
    frontend_ship_specs_draw_grid_horizontal();
}

static void frontend_ship_specs_draw_title(void)
{
    frontend_draw_menu_text_clipped(
        frontend_frame_scratch,
        frontend_ship_specs_ship.name,
        frontend_source_to_screen_x(10),
        frontend_source_to_screen_y(2),
        SCREEN_WIDTH,
        0xcbu
    );
}

static void frontend_wrapped_text_job_begin(
    FrontendWrappedTextJob *job,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 line_height,
    u8 max_lines,
    u8 colour
)
{
    *job = (FrontendWrappedTextJob){0};
    job->text = text;
    job->x = x;
    job->y = y;
    job->right = right;
    job->line_height = line_height;
    job->max_lines = max_lines;
    job->colour = colour;
    job->shadow_colour = FRONTEND_NEXT_SHADOW_COLOUR;
    job->active =
        text != 0 && max_lines != 0 && *text != '\0';
}

static void frontend_data_wrapped_text_job_begin(
    FrontendWrappedTextJob *job,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 line_height,
    u8 max_lines,
    u8 colour
)
{
    frontend_wrapped_text_job_begin(
        job,
        text,
        x,
        y,
        right,
        line_height,
        max_lines,
        colour
    );
    job->font_mode = 2;
}

static void frontend_wrapped_text_job_prepare_line(
    FrontendWrappedTextJob *job
)
{
    u8 line_length = 0;

    while (
        job->active &&
        *job->text != '\0' &&
        job->lines < job->max_lines
    ) {
        const char *word;
        u8 word_length = 0;
        s16 candidate_width;

        while (*job->text == ' ') job->text++;
        if (*job->text == '\0') break;
        word = job->text;
        while (
            job->text[word_length] != '\0' &&
            job->text[word_length] != ' ' &&
            word_length < sizeof(job->line) - 1u
        ) {
            word_length++;
        }
        if (
            word_length == 0 ||
            line_length +
                (line_length != 0 ? 1u : 0u) +
                word_length >= sizeof(job->line)
        ) {
            break;
        }
        if (line_length != 0) {
            job->line[line_length] = ' ';
            memcpy(
                job->line + line_length + 1u,
                word,
                word_length
            );
            job->line[
                line_length + 1u + word_length
            ] = '\0';
        } else {
            memcpy(job->line, word, word_length);
            job->line[word_length] = '\0';
        }
        candidate_width =
            job->font_mode == 1 ?
                frontend_data_tiny_text_width(job->line) :
            job->font_mode == 2 ?
                frontend_data_fine_text_width(job->line) :
                frontend_ship_specs_text_width(job->line);
        if (
            candidate_width > job->right - job->x &&
            line_length != 0
        ) {
            job->line[line_length] = '\0';
            break;
        }
        line_length = (u8)(
            line_length +
            (line_length != 0 ? 1u : 0u) +
            word_length
        );
        job->text += word_length;
    }
    if (line_length != 0 && job->lines < job->max_lines) {
        job->line[line_length] = '\0';
        job->line_length = line_length;
        job->draw_index = 0;
        job->draw_x = job->x;
        job->line_ready = 1;
        job->highlight = 0;
    } else {
        job->active = 0;
    }
}

/*
 * Draw at most a handful of source glyphs per call.  The line breaking is
 * byte-for-byte equivalent to frontend_draw_small_mixed_text_wrapped(); only
 * the scheduling differs.
 */
static u8 frontend_wrapped_text_job_step(
    FrontendWrappedTextJob *job
)
{
    u8 glyphs = 0;

    if (!job->active) return 1;
    if (!job->line_ready) {
        frontend_wrapped_text_job_prepare_line(job);
        if (!job->active) return 1;
    }
    while (
        job->draw_index < job->line_length &&
        glyphs < FRONTEND_COOPERATIVE_TEXT_GLYPHS_PER_STEP
    ) {
        char character = job->line[job->draw_index++];
        u8 tile_index;
        u8 advance;
        s16 draw_y = (s16)(
            job->y + job->lines * job->line_height
        );

        /* PC JE_outText() consumes '~' in both Data and Ship Specs text. */
        if (character == '~') {
            job->highlight ^= 1u;
            continue;
        }
        tile_index = frontend_pregame_font_tile(character);
        advance = job->font_mode == 1 ?
            frontend_data_tiny_font_advance(tile_index) :
            frontend_small_mixed_font_advance(tile_index);

        if (job->draw_x + advance > job->right) {
            job->draw_index = job->line_length;
            break;
        }
        if (job->font_mode == 1) {
            if (tile_index != 0) {
                frontend_data_tiny_draw_glyph(
                    frontend_frame_scratch,
                    tile_index,
                    job->draw_x,
                    draw_y,
                    job->right,
                    job->highlight ?
                        frontend_data_tiny_highlight_colour(
                            job->colour
                        ) :
                        job->colour,
                    job->shadow_colour
                );
            }
        } else {
            u8 foreground_colour = job->highlight ?
                (
                    job->font_mode == 2 ?
                        frontend_data_tiny_highlight_colour(job->colour) :
                        frontend_ship_specs_highlight_colour(job->colour)
                ) :
                job->colour;

            frontend_blit_small_mixed_font(
                frontend_frame_scratch,
                tile_index,
                (s16)(job->draw_x + 1),
                (s16)(draw_y + 1),
                job->shadow_colour
            );
            frontend_blit_small_mixed_font(
                frontend_frame_scratch,
                tile_index,
                job->draw_x,
                draw_y,
                foreground_colour
            );
        }
        job->draw_x = (s16)(job->draw_x + advance);
        glyphs++;
    }
    if (job->draw_index >= job->line_length) {
        job->line_ready = 0;
        job->line_length = 0;
        job->lines++;
        if (
            job->lines >= job->max_lines ||
            *job->text == '\0'
        ) {
            job->active = 0;
            return 1;
        }
    }
    return 0;
}

static void frontend_ship_specs_text_begin(u8 paragraph)
{
    frontend_wrapped_text_job_begin(
        &frontend_ship_specs_text_job,
        paragraph < OT_SHIP_INFO_PARAGRAPH_COUNT ?
            frontend_ship_info_cache->paragraph[paragraph] :
            "",
        frontend_source_to_screen_x(100),
        frontend_source_to_screen_y(
            paragraph == 0 ? 20 : 100
        ),
        SCREEN_WIDTH - 3,
        8,
        9,
        0xc9u
    );
}

static void frontend_ship_specs_draw_paragraph(u8 paragraph)
{
    if (paragraph >= OT_SHIP_INFO_PARAGRAPH_COUNT) return;
    frontend_ship_specs_text_begin(paragraph);
    while (!frontend_wrapped_text_job_step(
        &frontend_ship_specs_text_job
    )) {
    }
}

static void frontend_ship_specs_draw_footer(void)
{
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
}

/*
 * OpenTyrian does not hue-shift the complete ship sprite.  It decodes the
 * stock OPTION_SHAPES frame to a black temporary surface, compares every
 * pixel with its four neighbours, and overlays only local-brightness maxima
 * in green.  Keeping that exact operation here restores the source
 * wireframe/scan look while still reading the original SHP at runtime.
 */
static u8 frontend_ship_specs_green_ship_begin(
    u16 sprite_index,
    s16 source_x,
    s16 source_y
)
{
    OtShpSprite sprite;

    frontend_ship_specs_art_job = (FrontendShipArtJob){0};
    if (
        !ot_data_shp_sprite_read(5, sprite_index, &sprite) ||
        !sprite.populated ||
        sprite.width == 0 ||
        sprite.height == 0 ||
        sprite.width > FRONTEND_SHIP_SPRITE_WIDTH ||
        sprite.height > FRONTEND_SHIP_SPRITE_HEIGHT
    ) {
        return 0;
    }
    memset(
        frontend_ship_sprite_scratch,
        0,
        FRONTEND_SHIP_SPRITE_BYTES
    );
    telemetry_frontend_runtime_shp_decodes++;
    frontend_ship_specs_art_job.sprite = sprite;
    frontend_ship_specs_art_job.source_x = source_x;
    frontend_ship_specs_art_job.source_y = source_y;
    frontend_ship_specs_art_job.active = 1;
    return 1;
}

/*
 * Decode and filter a bounded number of source rows per VBlank.  This is the
 * exact OpenTyrian local-brightness wireframe operation, merely made
 * cooperative so entering Ship Specs cannot starve audio.
 */
static s8 frontend_ship_specs_green_ship_step(void)
{
    FrontendShipArtJob *job = &frontend_ship_specs_art_job;
    const OtShpSprite *sprite = &job->sprite;

    if (!job->active) return -1;
    if (job->phase == 0) {
        u16 row_limit = (u16)(
            job->y + FRONTEND_SHIP_ART_DECODE_ROWS_PER_STEP
        );

        if (row_limit > sprite->height) {
            row_limit = sprite->height;
        }
        while (
            job->position < sprite->encoded.size &&
            job->y < row_limit
        ) {
            u8 code =
                sprite->encoded.data[job->position++];

            if (code == 255) {
                if (job->position >= sprite->encoded.size) {
                    job->active = 0;
                    return -1;
                }
                job->x = (u16)(
                    job->x +
                    sprite->encoded.data[job->position++]
                );
            } else if (code == 254) {
                job->x = sprite->width;
            } else if (code == 253) {
                job->x++;
            } else {
                if (job->x < sprite->width) {
                    frontend_ship_sprite_scratch[
                        (u32)job->y *
                            FRONTEND_SHIP_SPRITE_WIDTH +
                        job->x
                    ] = code;
                }
                job->x++;
            }
            while (
                job->x >= sprite->width &&
                job->y < sprite->height
            ) {
                job->x = (u16)(job->x - sprite->width);
                job->y++;
            }
        }
        if (job->y >= sprite->height) {
            job->phase = 1;
            job->filter_row = 0;
        } else if (job->position >= sprite->encoded.size) {
            job->active = 0;
            return -1;
        }
        return 0;
    } else {
        u16 end_row = (u16)(
            job->filter_row +
            FRONTEND_SHIP_ART_FILTER_ROWS_PER_STEP
        );
        u16 y;

        if (end_row > sprite->height) {
            end_row = sprite->height;
        }
        for (y = job->filter_row; y < end_row; y++) {
            u16 x;

            for (x = 0; x < sprite->width; x++) {
                const u8 *pixel =
                    frontend_ship_sprite_scratch +
                    (u32)y * FRONTEND_SHIP_SPRITE_WIDTH +
                    x;
                u8 value = *pixel & 0x0fu;
                u16 neighbours = 0;

                if (value == 0) continue;
                if (y != 0) {
                    neighbours +=
                        pixel[
                            -(s32)FRONTEND_SHIP_SPRITE_WIDTH
                        ] & 0x0fu;
                }
                if (y + 1u < sprite->height) {
                    neighbours +=
                        pixel[FRONTEND_SHIP_SPRITE_WIDTH] &
                        0x0fu;
                }
                if (x != 0) neighbours += pixel[-1] & 0x0fu;
                if (x + 1u < sprite->width) {
                    neighbours += pixel[1] & 0x0fu;
                }
                if (value > (neighbours >> 2)) {
                    frontend_source_plot(
                        frontend_frame_scratch,
                        (s16)(job->source_x + x),
                        (s16)(job->source_y + y),
                        (u8)(0xc0u | value),
                        FRONTEND_SOURCE_BLIT_NORMAL,
                        0,
                        0
                    );
                }
            }
        }
        job->filter_row = end_row;
        if (job->filter_row >= sprite->height) {
            job->active = 0;
            return 1;
        }
        return 0;
    }
}

static u8 frontend_ship_specs_art_parameters(
    u16 *sprite_index,
    s16 *ship_x,
    s16 *ship_y
)
{
    u8 big_graphic = frontend_ship_specs_ship.bigshipgraphic;

    if (
        sprite_index == 0 ||
        ship_x == 0 ||
        ship_y == 0
    ) {
        return 0;
    }
    if (big_graphic == 0) big_graphic = 32;
    switch (big_graphic) {
    case 28:
        *ship_x = 1;
        *ship_y = 36;
        break;
    case 33:
        *ship_x = 1;
        *ship_y = 35;
        break;
    case 32:
    default:
        *ship_x = 5;
        *ship_y = 33;
        big_graphic = 32;
        break;
    }
    *sprite_index = (u16)(big_graphic - 1u);
    return 1;
}

static u8 frontend_ship_specs_art_begin(void)
{
    u16 sprite_index;
    s16 ship_x;
    s16 ship_y;

    return
        frontend_ship_specs_art_parameters(
            &sprite_index,
            &ship_x,
            &ship_y
        ) &&
        frontend_ship_specs_green_ship_begin(
            sprite_index,
            ship_x,
            ship_y
        );
}

static void frontend_ship_specs_draw_art_fallback(void)
{
    u16 sprite_index;
    s16 ship_x;
    s16 ship_y;

    if (
        frontend_ship_specs_art_parameters(
            &sprite_index,
            &ship_x,
            &ship_y
        )
    ) {
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            sprite_index,
            ship_x,
            ship_y,
            FRONTEND_SOURCE_BLIT_HUE,
            12,
            0
        );
    }
}

static void frontend_ship_specs_draw_art(void)
{
    s8 result;

    if (!frontend_ship_specs_art_begin()) {
        frontend_ship_specs_draw_art_fallback();
        return;
    }
    do {
        result = frontend_ship_specs_green_ship_step();
    } while (result == 0);
    if (result < 0) {
        frontend_ship_specs_draw_art_fallback();
    }
}

static void frontend_render_ship_specs(void)
{
    frontend_ship_specs_draw_grid();
    frontend_ship_specs_draw_title();
    frontend_ship_specs_draw_paragraph(0);
    frontend_ship_specs_draw_paragraph(1);
    frontend_ship_specs_draw_footer();
    frontend_ship_specs_draw_art();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}
