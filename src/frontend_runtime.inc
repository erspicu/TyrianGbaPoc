/*
 * OpenTyrian front-end state adapter with a GBA-native presentation path.
 *
 * Static chrome is composed once from the original PIC/SHP/HDT files, while
 * all level destinations and names are read from ROMFS levelsN.dat at
 * runtime.  Selection changes patch only the affected Mode-4 rows.
 */

enum {
    FRONTEND_MODE4_PAGE_BYTES = 0xA000,
    FRONTEND_DYNAMIC_PERCENT_GLYPH = 10,
    FRONTEND_PENDING_NONE = 0,
    FRONTEND_PENDING_FULL_FRAME = 1,
    FRONTEND_PENDING_SELECTION_PATCH = 2,
    FRONTEND_STATS_STAGE_COMPLETED = 0,
    FRONTEND_STATS_STAGE_CASH = 1,
    FRONTEND_STATS_STAGE_ENEMIES = 2,
    FRONTEND_STATS_STAGE_CUBES = 3,
    FRONTEND_STATS_STAGE_FINAL = 4,
    FRONTEND_STATS_STAGE_TICKS = 30,
    FRONTEND_STATS_CUBE_TICKS = 18,
    FRONTEND_STATS_MAX_CUBES = 4,
    FRONTEND_NEXT_PANEL_X = 126,
    FRONTEND_NEXT_PANEL_Y = 29,
    FRONTEND_NEXT_PANEL_WIDTH = 108,
    FRONTEND_NEXT_PANEL_HEIGHT = 116,
    FRONTEND_NEXT_CHOICE_Y = 32,
    FRONTEND_NEXT_CHOICE_STEP = 14,
    FRONTEND_NEXT_EXIT_Y = 119,
    FRONTEND_NEXT_ITEM_HEIGHT = 10,
    FRONTEND_NEXT_BACKGROUND_COLOUR = 228,
    FRONTEND_NEXT_TEXT_COLOUR = 244,
    FRONTEND_NEXT_SELECTED_COLOUR = 246,
    FRONTEND_NEXT_SHADOW_COLOUR = 240,
    FRONTEND_NEXT_FONT_WIDTH = 6,
    FRONTEND_NEXT_FONT_SPACE = 3,
};

#define FRONTEND_MODE4_PAGE0 ((u8 *)0x06000000)
#define FRONTEND_MODE4_PAGE1 \
    ((u8 *)(0x06000000 + FRONTEND_MODE4_PAGE_BYTES))

_Static_assert(
    FRONTEND_FRAME_BYTES == SCREEN_WIDTH * SCREEN_HEIGHT,
    "Mode 4 frame must be exactly 240x160 bytes"
);
_Static_assert(
    FRONTEND_FRAME_COUNT == 30,
    "generated front-end state atlas changed"
);
_Static_assert(
    FRONTEND_GLYPH_COUNT == 11 &&
        FRONTEND_GLYPH_BYTES == 64,
    "dynamic stats glyph atlas changed"
);
_Static_assert(
    FRONTEND_CUBE_BYTES ==
        FRONTEND_CUBE_WIDTH * FRONTEND_CUBE_HEIGHT,
    "generated OPTION_SHAPES cube stamp changed"
);
static const u8 frontend_glyph_advances[FRONTEND_GLYPH_COUNT] = {
    FRONTEND_GLYPH_0_ADVANCE,
    FRONTEND_GLYPH_1_ADVANCE,
    FRONTEND_GLYPH_2_ADVANCE,
    FRONTEND_GLYPH_3_ADVANCE,
    FRONTEND_GLYPH_4_ADVANCE,
    FRONTEND_GLYPH_5_ADVANCE,
    FRONTEND_GLYPH_6_ADVANCE,
    FRONTEND_GLYPH_7_ADVANCE,
    FRONTEND_GLYPH_8_ADVANCE,
    FRONTEND_GLYPH_9_ADVANCE,
    FRONTEND_GLYPH_10_ADVANCE,
};

static const u8 frontend_stats_cube_x[FRONTEND_STATS_MAX_CUBES] = {
    FRONTEND_STATS_CUBE_X_0,
    FRONTEND_STATS_CUBE_X_1,
    FRONTEND_STATS_CUBE_X_2,
    FRONTEND_STATS_CUBE_X_3,
};

static const char frontend_runtime_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-";

_Static_assert(
    sizeof(frontend_runtime_font_characters) - 1u ==
        JUKEBOX_FONT_TILE_COUNT - 1u,
    "shared runtime font order changed"
);

static u8 frontend_prepare_map(void)
{
    u8 choice;

    frontend_map = (OtEpisodeMap){0};
    memset(
        frontend_map_level,
        0,
        sizeof(frontend_map_level)
    );
    frontend_map_ready = 0;
    if (
        !ot_data_episode_map_resolve(
            (u8)(frontend_episode + 1u),
            frontend_main_section,
            frontend_play_mode,
            frontend_difficulty,
            &frontend_map
        )
    ) {
        return 0;
    }
    if (frontend_map.episode_complete) {
        frontend_map_ready = 1;
        return 1;
    }
    for (choice = 0; choice < frontend_map.choice_count; choice++) {
        if (
            !ot_data_episode_level_resolve(
                (u8)(frontend_episode + 1u),
                frontend_map.map_section[choice],
                frontend_play_mode,
                frontend_difficulty,
                &frontend_map_level[choice]
            )
        ) {
            frontend_map = (OtEpisodeMap){0};
            memset(
                frontend_map_level,
                0,
                sizeof(frontend_map_level)
            );
            return 0;
        }
    }
    frontend_map_ready = 1;
    return 1;
}

static u8 frontend_next_choice_count(void)
{
    if (
        !frontend_map_ready ||
        frontend_map.episode_complete ||
        frontend_map.choice_count == 0
    ) {
        return 0;
    }
    return frontend_play_mode == FRONTEND_PLAY_ARCADE ?
        1 :
        frontend_map.choice_count;
}

static u8 frontend_next_map_choice(u8 display_choice)
{
    if (frontend_play_mode == FRONTEND_PLAY_ARCADE) {
        return (u8)(frontend_map.choice_count - 1u);
    }
    return display_choice;
}

static const char *frontend_next_choice_label(u8 display_choice)
{
    u8 map_choice = frontend_next_map_choice(display_choice);
    u8 planet = frontend_map.map_planet[map_choice];

    if (
        frontend_text_ready &&
        planet > 0 &&
        planet <= 21 &&
        frontend_text.planet_name[planet - 1u][0] != '\0'
    ) {
        return frontend_text.planet_name[planet - 1u];
    }
    return frontend_map_level[map_choice].level_name;
}

static const u8 *frontend_frame_address(u8 frame)
{
    return frontend_frames + (u32)frame * FRONTEND_FRAME_BYTES;
}

static const u8 *frontend_palette_address(u8 frame)
{
    return frontend_palettes + (u32)frame * 512u;
}

static void frontend_set_affine_identity(void)
{
    REG_BG2PA = 1 << 8;
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = 1 << 8;
    REG_BG2X = 0;
    REG_BG2Y = 0;
    REG_BG2CNT = BG_PRIORITY(0);
}

static void frontend_show_immediate(
    const u8 *frame,
    const u8 *palette
)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    oam_dirty = 0;
    frontend_set_affine_identity();
    dmaCopy(frame, FRONTEND_MODE4_PAGE0, FRONTEND_FRAME_BYTES);
    dmaCopy(palette, BG_PALETTE, 512);
    frontend_display_page = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_mode4_active = 1;
    REG_DISPCNT = MODE_4 | BG2_ON;
    game_paused = 0;
}

static void frontend_present(
    const u8 *frame,
    const u8 *palette
)
{
    if (!frontend_mode4_active) {
        frontend_show_immediate(frame, palette);
        return;
    }
    frontend_pending_frame = frame;
    frontend_pending_palette = palette;
    frontend_pending_kind = FRONTEND_PENDING_FULL_FRAME;
    frontend_frame_pending = 1;
}

static void frontend_present_index(u8 frame)
{
    if (frame >= FRONTEND_FRAME_COUNT) return;
    frontend_present(
        frontend_frame_address(frame),
        frontend_palette_address(frame)
    );
}

static u8 *frontend_visible_page(void)
{
    return frontend_display_page ?
        FRONTEND_MODE4_PAGE1 :
        FRONTEND_MODE4_PAGE0;
}

static void frontend_selection_rect(
    u8 state,
    u8 selection,
    u16 *x,
    u16 *y,
    u16 *width,
    u16 *height
)
{
    s16 source_y = 0;

    *x = 0;
    *width = SCREEN_WIDTH;
    *height = 12;
    switch (state) {
    case STATE_TITLE:
        source_y = (s16)(108 + selection * 24);
        break;
    case STATE_PLAY_MODE_MENU:
    case STATE_DIFFICULTY_MENU:
        source_y = (s16)(54 + selection * 24);
        break;
    case STATE_EPISODE_MENU:
        source_y = (s16)(50 + selection * 30);
        break;
    case STATE_GAME_MENU:
        source_y = (s16)(
            38 + selection * 16 + (selection == 5 ? 16 : 0)
        );
        *x = 120;
        *width = 120;
        *height = 9;
        break;
    case STATE_NEXT_LEVEL_MENU:
        source_y = selection < frontend_next_choice_count() ?
            (s16)(
                FRONTEND_NEXT_CHOICE_Y +
                selection * FRONTEND_NEXT_CHOICE_STEP
            ) :
            FRONTEND_NEXT_EXIT_Y;
        *x = FRONTEND_NEXT_PANEL_X;
        *width = FRONTEND_NEXT_PANEL_WIDTH;
        *height = FRONTEND_NEXT_ITEM_HEIGHT;
        break;
    default:
        *height = 0;
        break;
    }
    *y = state == STATE_NEXT_LEVEL_MENU ?
        (u16)source_y :
        (
            source_y > 0 ?
                (u16)((u32)source_y * SCREEN_HEIGHT / 200u) :
                0
        );
    if (*y + *height > SCREEN_HEIGHT) {
        *height = (u16)(SCREEN_HEIGHT - *y);
    }
}

static void frontend_commit_rect(
    const u8 *source_frame,
    u8 *target_frame,
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        u32 offset = (u32)(y + row) * SCREEN_WIDTH + x;

        dmaCopy(
            source_frame + offset,
            target_frame + offset,
            width
        );
    }
}

static void frontend_commit_selection_patch(void)
{
    u16 old_x;
    u16 old_y;
    u16 old_width;
    u16 old_height;
    u16 new_x;
    u16 new_y;
    u16 new_width;
    u16 new_height;
    u8 *target = frontend_visible_page();

    frontend_selection_rect(
        frontend_patch_state,
        frontend_patch_old_selection,
        &old_x,
        &old_y,
        &old_width,
        &old_height
    );
    frontend_selection_rect(
        frontend_patch_state,
        frontend_patch_new_selection,
        &new_x,
        &new_y,
        &new_width,
        &new_height
    );
    if (old_height != 0) {
        frontend_commit_rect(
            frontend_pending_frame,
            target,
            old_x,
            old_y,
            old_width,
            old_height
        );
    }
    if (
        new_height != 0 &&
        (
            new_x != old_x ||
            new_y != old_y ||
            new_width != old_width ||
            new_height != old_height
        )
    ) {
        frontend_commit_rect(
            frontend_pending_frame,
            target,
            new_x,
            new_y,
            new_width,
            new_height
        );
    }
}

static void frontend_commit_vblank(void)
{
    u8 next_page;
    u8 *target;

    if (!frontend_frame_pending) return;
    if (frontend_pending_kind == FRONTEND_PENDING_SELECTION_PATCH) {
        frontend_commit_selection_patch();
        frontend_frame_pending = 0;
        frontend_pending_kind = FRONTEND_PENDING_NONE;
        return;
    }
    next_page = frontend_display_page ^ 1u;
    target = next_page ?
        FRONTEND_MODE4_PAGE1 :
        FRONTEND_MODE4_PAGE0;
    dmaCopy(
        frontend_pending_frame,
        target,
        FRONTEND_FRAME_BYTES
    );
    dmaCopy(frontend_pending_palette, BG_PALETTE, 512);
    frontend_display_page = next_page;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    REG_DISPCNT =
        MODE_4 | BG2_ON |
        (frontend_display_page ? BACKBUFFER : 0);
}

static void frontend_blit_glyph(
    u8 *frame,
    u8 glyph,
    s16 x,
    s16 y
)
{
    const u8 *source;
    u8 row;

    if (glyph >= FRONTEND_GLYPH_COUNT) return;
    source = frontend_glyphs + (u32)glyph * FRONTEND_GLYPH_BYTES;
    for (row = 0; row < 8; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < 8; column++) {
            u8 pixel = source[(u16)row * 8u + column];
            s16 target_x = (s16)(x + column);

            if (
                pixel != 0xff &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x
                ] = pixel;
            }
        }
    }
}

static u8 frontend_runtime_font_tile(char character)
{
    u8 index;

    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character == ' ') return 0;
    for (
        index = 0;
        index < sizeof(frontend_runtime_font_characters) - 1u;
        index++
    ) {
        if (
            frontend_runtime_font_characters[index] == character
        ) {
            return (u8)(index + 1u);
        }
    }
    for (
        index = 0;
        index < sizeof(frontend_runtime_font_characters) - 1u;
        index++
    ) {
        if (frontend_runtime_font_characters[index] == '?') {
            return (u8)(index + 1u);
        }
    }
    return 0;
}

static u8 frontend_runtime_font_advance(u8 tile_index)
{
    return tile_index == 0 ?
        FRONTEND_NEXT_FONT_SPACE :
        FRONTEND_NEXT_FONT_WIDTH;
}

static void frontend_blit_runtime_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    const u8 *tile;
    u8 row;

    if (tile_index == 0 || tile_index >= JUKEBOX_FONT_TILE_COUNT) {
        return;
    }
    tile = jukebox_font_tiles + (u32)tile_index * 32u;
    for (row = 0; row < 8; row++) {
        u8 output_column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (
            output_column = 0;
            output_column < FRONTEND_NEXT_FONT_WIDTH;
            output_column++
        ) {
            u8 first_source = (u8)(
                output_column * 8u / FRONTEND_NEXT_FONT_WIDTH
            );
            u8 last_source = (u8)(
                (
                    (u16)(output_column + 1u) * 8u +
                    FRONTEND_NEXT_FONT_WIDTH - 1u
                ) /
                FRONTEND_NEXT_FONT_WIDTH
            );
            u8 source_column;
            u8 pixel = 0;
            s16 target_x = (s16)(x + output_column);

            for (
                source_column = first_source;
                source_column < last_source;
                source_column++
            ) {
                u8 packed = tile[
                    (u16)row * 4u + (source_column >> 1)
                ];

                pixel |= (source_column & 1u) != 0 ?
                    packed >> 4 :
                    packed & 0x0fu;
            }

            if (
                pixel != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH +
                    (u16)target_x
                ] = colour;
            }
        }
    }
}

static void frontend_draw_runtime_text(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    u8 colour
)
{
    s16 right = FRONTEND_NEXT_PANEL_X + FRONTEND_NEXT_PANEL_WIDTH;

    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_runtime_font_tile(*text++);
        u8 advance = frontend_runtime_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_runtime_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR
        );
        frontend_blit_runtime_font(
            frame,
            tile_index,
            x,
            y,
            colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_fill_rect(
    u8 *frame,
    u16 x,
    u16 y,
    u16 width,
    u16 height,
    u8 colour
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        memset(
            frame + (u32)(y + row) * SCREEN_WIDTH + x,
            colour,
            width
        );
    }
}

static void frontend_render_next_level_item(u8 selection)
{
    u8 choice_count = frontend_next_choice_count();
    const char *label;
    u16 y;

    if (selection < choice_count) {
        y = (u16)(
            FRONTEND_NEXT_CHOICE_Y +
            selection * FRONTEND_NEXT_CHOICE_STEP
        );
        label = frontend_next_choice_label(selection);
    } else {
        y = FRONTEND_NEXT_EXIT_Y;
        label = (
            frontend_text_ready &&
            frontend_text.misc_text[5][0] != '\0'
        ) ?
            frontend_text.misc_text[5] :
            "EXIT TO GAME MENU";
    }
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_ITEM_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
    frontend_draw_runtime_text(
        frontend_frame_scratch,
        label,
        FRONTEND_NEXT_PANEL_X,
        (s16)y,
        selection == frontend_selection ?
            FRONTEND_NEXT_SELECTED_COLOUR :
            FRONTEND_NEXT_TEXT_COLOUR
    );
}

static void frontend_render_next_level(void)
{
    u8 choice_count = frontend_next_choice_count();
    u8 choice;

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_NEXT_LEVEL_0),
        FRONTEND_FRAME_BYTES
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        FRONTEND_NEXT_PANEL_Y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_PANEL_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
    for (choice = 0; choice < choice_count; choice++) {
        frontend_render_next_level_item(choice);
    }
    frontend_render_next_level_item(choice_count);
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_NEXT_LEVEL_0)
    );
}

static void frontend_render_next_level_selection(u8 old_selection)
{
    if (
        !frontend_mode4_active ||
        frontend_pending_kind == FRONTEND_PENDING_FULL_FRAME
    ) {
        frontend_render_next_level();
        return;
    }
    frontend_render_next_level_item(old_selection);
    frontend_render_next_level_item(frontend_selection);
    frontend_pending_frame = frontend_frame_scratch;
    frontend_patch_state = STATE_NEXT_LEVEL_MENU;
    frontend_patch_old_selection = old_selection;
    frontend_patch_new_selection = frontend_selection;
    frontend_pending_kind = FRONTEND_PENDING_SELECTION_PATCH;
    frontend_frame_pending = 1;
}

static s16 frontend_draw_u32(
    u8 *frame,
    u32 value,
    s16 x,
    s16 y
)
{
    u8 digit[10];
    u8 count = 0;

    do {
        digit[count++] = (u8)(value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digit));
    while (count != 0) {
        u8 current = digit[--count];

        frontend_blit_glyph(frame, current, x, y);
        x = (s16)(x + frontend_glyph_advances[current]);
    }
    return x;
}

static u8 frontend_stats_frame(void)
{
    if (frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED) {
        return FRONTEND_FRAME_STATS_COMPLETED;
    }
    if (frontend_stats_stage == FRONTEND_STATS_STAGE_CASH) {
        return FRONTEND_FRAME_STATS_CASH;
    }
    if (frontend_stats_stage == FRONTEND_STATS_STAGE_ENEMIES) {
        return FRONTEND_FRAME_STATS_ENEMIES;
    }
    if (frontend_stats_stage == FRONTEND_STATS_STAGE_CUBES) {
        return source_parity_level.data_cube_pickup_count != 0 ?
            FRONTEND_FRAME_STATS_FULL_CUBES :
            FRONTEND_FRAME_STATS_FULL_NONE;
    }
    if (frontend_play_mode == FRONTEND_PLAY_ARCADE) {
        return FRONTEND_FRAME_STATS_ARCADE_FINAL;
    }
    return source_parity_level.data_cube_pickup_count != 0 ?
        FRONTEND_FRAME_STATS_FULL_FINAL_CUBES :
        FRONTEND_FRAME_STATS_FULL_FINAL_NONE;
}

static u8 frontend_stats_cube_total(void)
{
    u32 total = source_parity_level.data_cube_pickup_count;

    return total > FRONTEND_STATS_MAX_CUBES ?
        FRONTEND_STATS_MAX_CUBES :
        (u8)total;
}

static void frontend_blit_cube(
    u8 *frame,
    s16 x,
    s16 y
)
{
    u8 row;

    for (row = 0; row < FRONTEND_CUBE_HEIGHT; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < FRONTEND_CUBE_WIDTH; column++) {
            u8 pixel = frontend_cube[
                (u16)row * FRONTEND_CUBE_WIDTH + column
            ];
            s16 target_x = (s16)(x + column);

            if (
                pixel != 0xff &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x
                ] = pixel;
            }
        }
    }
}

static void frontend_render_stats(void)
{
    u8 frame = frontend_stats_frame();
    u32 killed_percent =
        source_parity_level.total_enemy == 0 ?
            0 :
            (
                source_parity_level.enemy_kill_count * 100u +
                source_parity_level.total_enemy / 2u
            ) /
            source_parity_level.total_enemy;
    s16 x;

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(frame),
        FRONTEND_FRAME_BYTES
    );
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_CASH) {
        frontend_draw_u32(
            frontend_frame_scratch,
            player_cash,
            FRONTEND_STATS_CASH_X,
            FRONTEND_STATS_CASH_Y
        );
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_ENEMIES) {
        x = frontend_draw_u32(
            frontend_frame_scratch,
            killed_percent,
            FRONTEND_STATS_KILLED_X,
            FRONTEND_STATS_KILLED_Y
        );
        frontend_blit_glyph(
            frontend_frame_scratch,
            FRONTEND_DYNAMIC_PERCENT_GLYPH,
            x,
            FRONTEND_STATS_KILLED_Y
        );
    }
    if (
        frontend_play_mode != FRONTEND_PLAY_ARCADE &&
        frontend_stats_stage >= FRONTEND_STATS_STAGE_CUBES
    ) {
        u8 cube_total = frontend_stats_cube_total();
        u8 cube_count = frontend_stats_cube_visible_count;
        u8 cube_index;

        if (cube_count > cube_total) cube_count = cube_total;
        for (cube_index = 0; cube_index < cube_count; cube_index++) {
            frontend_blit_cube(
                frontend_frame_scratch,
                frontend_stats_cube_x[cube_index],
                FRONTEND_STATS_CUBE_Y
            );
        }
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(frame)
    );
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

    if (game_state == STATE_LEVEL_STATS) {
        frontend_render_stats();
        return;
    }
    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level();
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

    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level_selection(old_selection);
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
    game_state = state;
    frontend_selection = selection;
    frontend_redraw();
    telemetry_state_transitions++;
}

static void enter_title(void)
{
    frontend_enter_state(STATE_TITLE, 0);
    audio_load_source_song(29);
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
    if (!frontend_prepare_map()) {
        enter_title();
        return;
    }
    frontend_enter_state(STATE_GAME_MENU, 4);
    audio_load_source_song(29);
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
    frontend_timer = 200;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
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
}

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

    if (game_state == STATE_LEVEL_STATS) {
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
                    frontend_level_ready &&
                    frontend_level.next_section != 0
                ) {
                    frontend_main_section =
                        frontend_level.next_section;
                }
                frontend_prepare_map();
                frontend_enter_state(STATE_GAME_MENU, 4);
                audio_load_source_song(29);
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
        if (frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL) {
            return;
        }
        if (frontend_stats_timer > 0) {
            frontend_stats_timer--;
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
        telemetry_stats_stage_advances++;
        frontend_redraw();
        return;
    }
    if (pad_pressed & KEY_UP) {
        switch (game_state) {
        case STATE_TITLE:
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
        default:
            break;
        }
        return;
    }
    if (pad_pressed & KEY_DOWN) {
        switch (game_state) {
        case STATE_TITLE:
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
        default:
            break;
        }
        return;
    }

    if (pad_pressed & KEY_B) {
        switch (game_state) {
        case STATE_PLAY_MODE_MENU:
            enter_title();
            break;
        case STATE_EPISODE_MENU:
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_enter_state(STATE_EPISODE_MENU, frontend_episode);
            break;
        case STATE_GAME_MENU:
            enter_title();
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_enter_state(STATE_GAME_MENU, 4);
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
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        } else {
            jukebox_enter();
        }
        break;
    case STATE_PLAY_MODE_MENU:
        frontend_play_mode = frontend_selection;
        frontend_enter_state(STATE_EPISODE_MENU, 0);
        break;
    case STATE_EPISODE_MENU:
        frontend_episode = frontend_selection;
        frontend_main_section = 1;
        frontend_map = (OtEpisodeMap){0};
        frontend_level = (OtEpisodeLevel){0};
        frontend_map_ready = 0;
        frontend_level_ready = 0;
        frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        break;
    case STATE_DIFFICULTY_MENU:
        frontend_difficulty =
            (u8)(FRONTEND_DIFFICULTY_EASY + frontend_selection);
        frontend_prepare_map();
        frontend_enter_state(STATE_GAME_MENU, 4);
        break;
    case STATE_GAME_MENU:
        if (frontend_selection == 4) {
            if (
                !frontend_map_ready &&
                !frontend_prepare_map()
            ) {
                audio_load_source_song(29);
            } else if (frontend_map.episode_complete) {
                frontend_advance_episode();
            } else {
                frontend_enter_state(STATE_NEXT_LEVEL_MENU, 0);
            }
        } else if (frontend_selection == 5) {
            enter_title();
        }
        /* Data Cubes through Options remain visible but disabled. */
        break;
    case STATE_NEXT_LEVEL_MENU:
        if (frontend_selection < frontend_next_choice_count()) {
            u8 map_choice =
                frontend_next_map_choice(frontend_selection);

            if (frontend_map_level[map_choice].episode_complete) {
                frontend_advance_episode();
            } else if (
                !enter_level(frontend_map.map_section[map_choice])
            ) {
                frontend_enter_state(STATE_GAME_MENU, 4);
                audio_load_source_song(29);
            }
        } else {
            frontend_enter_state(STATE_GAME_MENU, 4);
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
        telemetry_sprite2_uploads !=
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
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    frontend_autotest_record_campaign_level(completed);
#endif
    frontend_level_completed = completed;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = FRONTEND_STATS_STAGE_TICKS;
    frontend_enter_state(STATE_LEVEL_STATS, 0);
    audio_effect(SFX_LEVEL_COMPLETE);
    telemetry_level_complete_voice_starts++;
}

static void frontend_enter_game_over(void)
{
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
    game_over_tile_upload_pending = 1;
    audio_load_source_song_preserve_effects(10);
    telemetry_game_over_music_starts++;
    telemetry_state_transitions++;
}

static void game_over_update(void)
{
    if (pad_pressed == 0) return;

    frontend_enter_state(STATE_GAME_MENU, 4);
    audio_load_source_song(29);
    telemetry_game_over_exits++;
}
