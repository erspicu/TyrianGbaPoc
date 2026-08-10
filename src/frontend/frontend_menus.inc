enum {
    /*
     * OPTION_SHAPES frame 12 contains 278 sparse runs.  Replaying 64 per
     * display tick keeps each source-identical slice below one GBA frame;
     * the transition simply takes five quiet ticks instead of blocking
     * Maxmod for one oversized draw call.
     */
    FRONTEND_UPGRADE_STAMP_RUNS_PER_STEP = 48,
    FRONTEND_UPGRADE_PREVIEW_CACHE_X = 8,
    FRONTEND_UPGRADE_PREVIEW_CACHE_Y = 104,
    FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH = 112,
    FRONTEND_UPGRADE_PREVIEW_CACHE_HEIGHT = 48,
    FRONTEND_UPGRADE_CASH_DIRTY_X = 44,
    FRONTEND_UPGRADE_CASH_DIRTY_Y = 134,
    FRONTEND_UPGRADE_CASH_DIRTY_WIDTH = 72,
    FRONTEND_UPGRADE_CASH_DIRTY_HEIGHT = 14,
    FRONTEND_UPGRADE_SUBMENU_ROW_Y = 21,
    FRONTEND_UPGRADE_SUBMENU_ROW_STEP = 21,
    /* 6-pixel glyph + one shadow row, twice, at y and y + 9. */
    FRONTEND_UPGRADE_SUBMENU_ROW_TEXT_HEIGHT = 16,
};

static FrontendSourceStampJob frontend_upgrade_stamp_job EWRAM_BSS;

/*
 * The immutable OPTION_SHAPES simulator chrome already exists in the Mode-4
 * frame.  Preserve only the small area underneath its power/cash text in the
 * gameplay-only background cache arena.  Upgrade menus and gameplay never
 * overlap, so this avoids another permanent EWRAM allocation.
 */
static void frontend_upgrade_preview_cache_capture(void)
{
    u8 row;

    if (!frontend_upgrade_sim_category_active()) {
        frontend_upgrade_preview_cache_valid = 0;
        return;
    }
    for (
        row = 0;
        row < FRONTEND_UPGRADE_PREVIEW_CACHE_HEIGHT;
        row++
    ) {
        memcpy(
            frontend_ship_panel_cache +
                (u16)row * FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH,
            frontend_frame_scratch +
                (u32)(FRONTEND_UPGRADE_PREVIEW_CACHE_Y + row) *
                    SCREEN_WIDTH +
                FRONTEND_UPGRADE_PREVIEW_CACHE_X,
            FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH
        );
    }
    /* The same arena no longer contains a keyed ship-panel image. */
    frontend_ship_panel_cache_valid = 0;
    frontend_upgrade_preview_cache_category =
        frontend_upgrade_category;
    frontend_upgrade_preview_cache_valid = 1;
}

static u8 frontend_upgrade_preview_cache_restore(
    u8 x,
    u8 y,
    u8 width,
    u8 height
)
{
    u8 row;

    if (
        !frontend_upgrade_preview_cache_valid ||
        frontend_upgrade_preview_cache_category !=
            frontend_upgrade_category ||
        x < FRONTEND_UPGRADE_PREVIEW_CACHE_X ||
        y < FRONTEND_UPGRADE_PREVIEW_CACHE_Y ||
        x + width >
            FRONTEND_UPGRADE_PREVIEW_CACHE_X +
                FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH ||
        y + height >
            FRONTEND_UPGRADE_PREVIEW_CACHE_Y +
                FRONTEND_UPGRADE_PREVIEW_CACHE_HEIGHT
    ) {
        return 0;
    }
    for (row = 0; row < height; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH + x,
            frontend_ship_panel_cache +
                (u16)(y - FRONTEND_UPGRADE_PREVIEW_CACHE_Y + row) *
                    FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH +
                (u16)(x - FRONTEND_UPGRADE_PREVIEW_CACHE_X),
            width
        );
    }
    return 1;
}

static u8 frontend_upgrade_ship_preview_cache_matches_items(
    const FrontendPlayerItems *items,
    u32 cash,
    u8 armor
)
{
    return
        frontend_upgrade_ship_preview_cache_valid &&
        items != 0 &&
        frontend_upgrade_ship_preview_cache_cash == cash &&
        frontend_upgrade_ship_preview_cache_armor == armor &&
        memcmp(
            &frontend_upgrade_ship_preview_cache_items,
            items,
            sizeof(*items)
        ) == 0;
}

static u8 frontend_upgrade_static_panel_category_active(void)
{
    return
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIP ||
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIELD;
}

static void frontend_upgrade_ship_preview_copy_to_cache(void)
{
    u16 row;

    if (!frontend_player_items_valid) {
        frontend_upgrade_ship_preview_cache_valid = 0;
        return;
    }
    for (
        row = 0;
        row < FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_HEIGHT;
        row++
    ) {
        const u32 *source = (const u32 *)(const void *)(
            frontend_frame_scratch +
                (u32)(FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_Y + row) *
                    SCREEN_WIDTH
        );
        u32 *destination = (u32 *)(void *)(
            frontend_upgrade_ship_preview_cache +
                (u32)row * FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_WIDTH
        );
        u8 word;

        for (
            word = 0;
            word < FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_WIDTH / 4u;
            word++
        ) {
            destination[word] = source[word];
        }
    }
    frontend_upgrade_ship_preview_cache_items = frontend_player_items;
    frontend_upgrade_ship_preview_cache_cash = player_cash;
    frontend_upgrade_ship_preview_cache_armor = player_armor;
    frontend_upgrade_ship_preview_cache_valid = 1;
}

static void frontend_upgrade_ship_preview_prepare_candidate(u8 item)
{
    FrontendPlayerItems candidate;

    if (
        !frontend_upgrade_static_panel_category_active() ||
        !frontend_player_items_valid
    ) {
        frontend_upgrade_ship_preview_swap_pending = 0;
        return;
    }
    candidate = frontend_player_items;
    if (frontend_upgrade_category == FRONTEND_UPGRADE_SHIP) {
        candidate.ship = item;
    } else {
        candidate.shield = item;
    }
    if (
        frontend_upgrade_ship_preview_cache_matches_items(
            &candidate,
            player_cash,
            player_armor
        )
    ) {
        /* Preserve the panel currently in scratch as the next cache entry. */
        frontend_upgrade_ship_preview_swap_items = frontend_player_items;
        frontend_upgrade_ship_preview_swap_cash = player_cash;
        frontend_upgrade_ship_preview_swap_armor = player_armor;
        frontend_upgrade_ship_preview_swap_pending = 1;
        return;
    }
    frontend_upgrade_ship_preview_swap_pending = 0;
    frontend_upgrade_ship_preview_copy_to_cache();
}

static u8 frontend_upgrade_ship_preview_apply_cached(void)
{
    u16 row;
    u8 swap;

    if (
        !frontend_upgrade_static_panel_category_active() ||
        !frontend_upgrade_ship_preview_cache_matches_items(
            &frontend_player_items,
            player_cash,
            player_armor
        )
    ) {
        frontend_upgrade_ship_preview_swap_pending = 0;
        return 0;
    }
    swap = frontend_upgrade_ship_preview_swap_pending;
    for (
        row = 0;
        row < FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_HEIGHT;
        row++
    ) {
        u32 *frame = (u32 *)(void *)(
            frontend_frame_scratch +
                (u32)(FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_Y + row) *
                    SCREEN_WIDTH
        );
        u32 *cache = (u32 *)(void *)(
            frontend_upgrade_ship_preview_cache +
                (u32)row * FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_WIDTH
        );
        u8 word;

        for (
            word = 0;
            word < FRONTEND_UPGRADE_SHIP_PREVIEW_CACHE_WIDTH / 4u;
            word++
        ) {
            u32 cached = cache[word];

            if (swap) {
                cache[word] = frame[word];
            }
            frame[word] = cached;
        }
    }
    if (swap) {
        frontend_upgrade_ship_preview_cache_items =
            frontend_upgrade_ship_preview_swap_items;
        frontend_upgrade_ship_preview_cache_cash =
            frontend_upgrade_ship_preview_swap_cash;
        frontend_upgrade_ship_preview_cache_armor =
            frontend_upgrade_ship_preview_swap_armor;
        telemetry_frontend_upgrade_ship_cache_swaps++;
    }
    telemetry_frontend_upgrade_ship_cache_hits++;
    frontend_upgrade_ship_preview_swap_pending = 0;
    return 1;
}

static const char *frontend_game_menu_label(u8 index)
{
    static const char *const fallback[7] = {
        "Game Menu",
        "Data",
        "Ship Specs",
        "Upgrade Ship",
        "Options",
        "Play Next Level",
        "Quit Game",
    };

    if (
        frontend_text_ready &&
        index < 7 &&
        frontend_text.full_game_menu[index][0] != '\0'
    ) {
        return frontend_text.full_game_menu[index];
    }
    return fallback[index < 7 ? index : 0];
}

/*
 * Direct zero-based form of OpenTyrian helptext.c menuHelp[MENU_FULL_GAME].
 * The second entry deliberately selects mainMenuHelp[34] ("Ship Specs").
 */
static u8 frontend_game_menu_help_index(u8 selection)
{
    static const u8 help_index[6] = {0, 33, 1, 2, 3, 4};
    return help_index[selection < 6 ? selection : 0];
}

static const char *frontend_game_menu_help(u8 selection)
{
    static const char *const fallback[6] = {
        "Communications : Read incoming messages",
        "Access schematics and detailed info on your ship.",
        "Purchase new weapons and ship components.",
        "Configuration and Load/Save game",
        "Journey onward to another level.",
        "Quit the game.",
    };
    u8 index = selection < 6 ? selection : 0;
    u8 source_index = frontend_game_menu_help_index(selection);

    if (
        frontend_text_ready &&
        frontend_text.main_menu_help[source_index][0] != '\0'
    ) {
        return frontend_text.main_menu_help[source_index];
    }
    return fallback[index];
}

/*
 * Direct zero-based form of helptext.c menuHelp[MENU_UPGRADES].
 * Both sidekick slots intentionally share mainMenuHelp[11], matching PC.
 */
static u8 frontend_upgrade_menu_help_index(u8 selection)
{
    static const u8 help_index[8] = {5, 6, 7, 8, 9, 10, 10, 11};
    return help_index[selection < 8 ? selection : 0];
}

static const char *frontend_upgrade_menu_help(u8 selection)
{
    static const char *const fallback[8] = {
        "Each ship has a different armor rating.",
        "Change and upgrade your forward weapon.",
        "Change and upgrade your rear/side weapon.",
        "Shields are your first line of defense.",
        "Generators power your weapons and shields.",
        "Take a chance : Purchase a special weapon.",
        "Take a chance : Purchase a special weapon.",
        "Return to the previous menu.",
    };
    u8 index = selection < 8 ? selection : 0;
    u8 source_index = frontend_upgrade_menu_help_index(selection);

    if (
        frontend_text_ready &&
        frontend_text.main_menu_help[source_index][0] != '\0'
    ) {
        return frontend_text.main_menu_help[source_index];
    }
    return fallback[index];
}

static u16 frontend_menu_help_dirty_y(void)
{
    return TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y > 0 ?
        (u16)(TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y - 1) :
        0;
}

static u8 frontend_copy_static_menu_help(u8 source_index)
{
    const u32 *source;
    u32 *destination;
    u16 word;

    if (source_index >= FRONTEND_STATIC_HELP_STRIP_COUNT) return 0;
    source = (const u32 *)(const void *)(
        frontend_static_help_strips +
        (u32)source_index * FRONTEND_STATIC_HELP_STRIP_BYTES
    );
    destination = (u32 *)(void *)(
        frontend_frame_scratch +
        (u32)FRONTEND_STATIC_HELP_STRIP_Y * SCREEN_WIDTH
    );
    for (
        word = 0;
        word < FRONTEND_STATIC_HELP_STRIP_BYTES / 4u;
        word++
    ) {
        destination[word] = source[word];
    }
    return 1;
}

static void frontend_draw_menu_help(
    u8 source_index,
    const char *text
)
{
    u16 dirty_y = frontend_menu_help_dirty_y();

    if (frontend_copy_static_menu_help(source_index)) return;
    /*
     * JE_drawMainMenuHelpText() redraws this line on every selection.  The
     * chrome copy removes every old glyph first, then a tiny mixed-case face
     * reproduces HDT text without scaling a one-pixel PC font.
     */
    frontend_restore_menu_chrome_rect(
        0,
        dirty_y,
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - dirty_y)
    );
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        text,
        TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X,
        TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y,
        TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT,
        0xeau,
        0xe2u
    );
}

static void frontend_draw_game_menu_help(u8 selection)
{
    frontend_draw_menu_help(
        frontend_game_menu_help_index(selection),
        frontend_game_menu_help(selection)
    );
}

static void frontend_draw_upgrade_menu_help(u8 selection)
{
    frontend_draw_menu_help(
        frontend_upgrade_menu_help_index(selection),
        frontend_upgrade_menu_help(selection)
    );
}

/*
 * OpenTyrian game_menu.c draws OPTION_SHAPES sprite 34 once per cube.
 * The count is gameplay state (not a decorative baked panel), so retain
 * the exact source coordinates and redraw it after every right-panel copy.
 */
static void frontend_draw_game_menu_cubes(void)
{
    u8 cube;

    for (cube = 0; cube < frontend_data_cube_count; cube++) {
        s16 source_x = (s16)(208 + cube * 18);

        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            34,
            (s16)(source_x + 2),
            38,
            FRONTEND_SOURCE_BLIT_DARK,
            0,
            0
        );
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            34,
            source_x,
            37,
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
    }
}

static void frontend_draw_game_menu_dynamic(u8 selection)
{
    frontend_draw_game_menu_cubes();
    frontend_draw_game_menu_help(selection);
}

static const char *frontend_pre_game_title_label(u8 selection)
{
    static const char *const fallback[4] = {
        "Start New Game",
        "Load Game",
        "Demo",
        "Jukebox",
    };
    static const u8 source_index[4] = {0, 1, 5, 0xff};

    if (
        selection < 4 &&
        source_index[selection] != 0xff &&
        frontend_text_ready &&
        frontend_text.title_menu[source_index[selection]][0] != '\0'
    ) {
        return frontend_text.title_menu[source_index[selection]];
    }
    return fallback[selection < 4 ? selection : 0];
}

static const char *frontend_pre_game_header(u8 state)
{
    if (state == STATE_PLAY_MODE_MENU) return "Play Mode";
    if (
        state == STATE_EPISODE_MENU &&
        frontend_text_ready &&
        frontend_text.episode_name[0][0] != '\0'
    ) {
        return frontend_text.episode_name[0];
    }
    if (
        state == STATE_DIFFICULTY_MENU &&
        frontend_text_ready &&
        frontend_text.difficulty_name[0][0] != '\0'
    ) {
        return frontend_text.difficulty_name[0];
    }
    return state == STATE_EPISODE_MENU ?
        "Select an Episode" :
        "Difficulty Level";
}

static u8 frontend_pre_game_item_count(u8 state)
{
    switch (state) {
    case STATE_TITLE:
        return 4;
    case STATE_DIFFICULTY_MENU:
        return 3;
    case STATE_PLAY_MODE_MENU:
        return 2;
    case STATE_EPISODE_MENU:
        return 4;
    default:
        return 0;
    }
}

static const char *frontend_pre_game_item_label(
    u8 state,
    u8 selection
)
{
    static const char *const play_mode[2] = {
        "Full Game",
        "Arcade",
    };
    static const char *const episode[4] = {
        "Episode 1: Escape",
        "Episode 2: Treachery",
        "Episode 3: Mission: Suicide",
        "Episode 4: An End to Fate",
    };
    static const char *const difficulty[3] = {
        "Easy",
        "Normal",
        "Hard",
    };

    if (state == STATE_TITLE) {
        return frontend_pre_game_title_label(selection);
    }
    if (state == STATE_PLAY_MODE_MENU) {
        return play_mode[selection < 2 ? selection : 0];
    }
    if (state == STATE_EPISODE_MENU) {
        if (
            selection < 4 &&
            frontend_text_ready &&
            frontend_text.episode_name[selection + 1u][0] != '\0'
        ) {
            return frontend_text.episode_name[selection + 1u];
        }
        return episode[selection < 4 ? selection : 0];
    }
    if (
        state == STATE_DIFFICULTY_MENU &&
        selection < 3 &&
        frontend_text_ready &&
        frontend_text.difficulty_name[selection + 1u][0] != '\0'
    ) {
        return frontend_text.difficulty_name[selection + 1u];
    }
    return difficulty[selection < 3 ? selection : 0];
}

static s16 frontend_pre_game_item_y(u8 state, u8 selection)
{
    switch (state) {
    case STATE_TITLE:
        return (s16)(
            TYRIAN_GBA_LAYOUT_TITLE_MENU_FIRST_Y +
            selection * TYRIAN_GBA_LAYOUT_TITLE_MENU_ROW_STEP
        );
    case STATE_EPISODE_MENU:
        return (s16)(
            TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_FIRST_Y +
            selection * TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_ROW_STEP
        );
    case STATE_PLAY_MODE_MENU:
    case STATE_DIFFICULTY_MENU:
    default:
        return (s16)(
            TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y +
            selection * TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP
        );
    }
}

static void frontend_pre_game_item_rect(
    u8 state,
    u8 selection,
    u16 *x,
    u16 *y,
    u16 *width,
    u16 *height
)
{
    *x = 0;
    *y = (u16)frontend_pre_game_item_y(state, selection);
    *width = SCREEN_WIDTH;
    *height = FRONTEND_PREGAME_FONT_HEIGHT + 2u;
}

static void frontend_draw_pre_game_item(
    u8 state,
    u8 selection,
    u8 active_selection
)
{
    const char *label =
        frontend_pre_game_item_label(state, selection);
    s16 y = frontend_pre_game_item_y(state, selection);
    u8 colour;

    colour = selection == active_selection ? 0xfeu : 0xfau;

    if (state == STATE_EPISODE_MENU) {
        frontend_draw_pregame_text_clipped(
            frontend_frame_scratch,
            label,
            TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_X,
            y,
            TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_RIGHT,
            colour
        );
    } else {
        frontend_draw_pregame_text_centered(
            frontend_frame_scratch,
            label,
            state == STATE_TITLE ?
                TYRIAN_GBA_LAYOUT_TITLE_MENU_CENTER_X :
                TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X,
            y,
            colour
        );
    }
}

static void frontend_render_pre_game_menu(void)
{
    u8 base_frame =
        game_state == STATE_TITLE ?
            FRONTEND_FRAME_TITLE_CHROME :
            FRONTEND_FRAME_SELECT_CHROME;
    u8 static_base;
    u8 item_count = frontend_pre_game_item_count(game_state);
    u8 selection;

    switch (game_state) {
    case STATE_TITLE:
        static_base = FRONTEND_STATIC_PRE_GAME_TITLE_BASE;
        break;
    case STATE_PLAY_MODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE;
        break;
    case STATE_EPISODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_EPISODE_BASE;
        break;
    case STATE_DIFFICULTY_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE;
        break;
    default:
        static_base = FRONTEND_STATIC_PRE_GAME_FRAME_COUNT;
        break;
    }
    if (
        frontend_selection < item_count &&
        static_base + frontend_selection <
            FRONTEND_STATIC_PRE_GAME_FRAME_COUNT
    ) {
        const u8 *static_frame =
            frontend_static_pre_game_frames +
            (u32)(static_base + frontend_selection) *
                FRONTEND_FRAME_BYTES;

        if (game_state == STATE_TITLE) {
            u8 first_row = (u8)(
                TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y > 0 ?
                    TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y - 1 :
                    0
            );
            u8 row_count = (u8)(SCREEN_HEIGHT - first_row);
            u8 row;

            /*
             * Present the immutable build-time title immediately.  Prepare
             * only the tiny version strip in EWRAM and patch it on the next
             * VBlank; copying all 38.4 KiB merely to add one line caused the
             * old title transition to miss a frame.
             */
            for (row = 0; row < row_count; row++) {
                memcpy(
                    frontend_frame_scratch +
                        (u32)(first_row + row) * SCREEN_WIDTH,
                    static_frame +
                        (u32)(first_row + row) * SCREEN_WIDTH,
                    SCREEN_WIDTH
                );
            }
            frontend_draw_small_mixed_text_clipped_no_shadow(
                frontend_frame_scratch,
                TYRIAN_GBA_BUILD_LABEL,
                TYRIAN_GBA_LAYOUT_TITLE_BUILD_X,
                TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y,
                SCREEN_WIDTH,
                0xfeu
            );
            frontend_present(
                static_frame,
                frontend_palette_address(base_frame)
            );
            frontend_title_brand_pending = 1;
            return;
        }
        frontend_present(
            static_frame,
            frontend_palette_address(base_frame)
        );
        return;
    }

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(base_frame),
        FRONTEND_FRAME_BYTES
    );
    if (game_state != STATE_TITLE) {
        frontend_draw_pregame_text_centered(
            frontend_frame_scratch,
            frontend_pre_game_header(game_state),
            TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X,
            TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y,
            0xfbu
        );
    }
    for (selection = 0; selection < item_count; selection++) {
        frontend_draw_pre_game_item(
            game_state,
            selection,
            frontend_selection
        );
    }
    if (game_state == STATE_TITLE) {
        frontend_draw_small_mixed_text_clipped_no_shadow(
            frontend_frame_scratch,
            TYRIAN_GBA_BUILD_LABEL,
            TYRIAN_GBA_LAYOUT_TITLE_BUILD_X,
            TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y,
            SCREEN_WIDTH,
            0xfeu
        );
        frontend_title_brand_pending = 0;
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(base_frame)
    );
}

static void frontend_render_pre_game_selection(u8 old_selection)
{
    u8 static_base;
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    switch (game_state) {
    case STATE_TITLE:
        static_base = FRONTEND_STATIC_PRE_GAME_TITLE_BASE;
        break;
    case STATE_PLAY_MODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE;
        break;
    case STATE_EPISODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_EPISODE_BASE;
        break;
    case STATE_DIFFICULTY_MENU:
    default:
        static_base = FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE;
        break;
    }
    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        u16 x;
        u16 y;
        u16 width;
        u16 height;

        frontend_pre_game_item_rect(
            game_state,
            selection[index],
            &x,
            &y,
            &width,
            &height
        );
        frontend_dirty_add(x, y, width, height);
    }
    frontend_dirty_present();
    /*
     * frontend_dirty_present() defaults to the mutable scratch frame.  This
     * path instead copies the two dirty rows from the already-baked frame
     * for the new selection; assign it after arming the dirty commit.
     */
    frontend_pending_frame =
        frontend_static_pre_game_frames +
        (u32)(static_base + frontend_selection) *
            FRONTEND_FRAME_BYTES;
}

static s16 frontend_game_menu_item_y(u8 selection)
{
    return frontend_source_to_screen_y(
        (s16)(
            TYRIAN_GBA_LAYOUT_GAME_MENU_FIRST_SOURCE_Y +
            selection *
                TYRIAN_GBA_LAYOUT_GAME_MENU_SOURCE_ROW_STEP +
            (
                selection == 5 ?
                    TYRIAN_GBA_LAYOUT_GAME_MENU_QUIT_SOURCE_GAP :
                    0
            )
        )
    );
}

static void frontend_draw_game_menu_item(
    u8 selection,
    u8 active_selection
)
{
    u8 disabled = 0;
    u8 colour;

    if (disabled) {
        colour =
            selection == active_selection ? 0xf8u : 0xf4u;
    } else {
        colour =
            selection == active_selection ? 0xfeu : 0xfau;
    }
    frontend_draw_menu_text_clipped(
        frontend_frame_scratch,
        frontend_game_menu_label((u8)(selection + 1u)),
        TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_X,
        frontend_game_menu_item_y(selection),
        TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_RIGHT,
        colour
    );
}

static void frontend_draw_game_menu_text(u8 active_selection)
{
    u8 selection;

    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        frontend_game_menu_label(0),
        TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_CENTER_X,
        TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_Y,
        0xfbu
    );
    for (selection = 0; selection < 6; selection++) {
        frontend_draw_game_menu_item(selection, active_selection);
    }
}

static void frontend_quit_capture_dialog_background(void);

static void frontend_compose_game_menu(u8 selection)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    if (frontend_player_items_valid) {
        if (!frontend_ship_panel_cache_apply(frontend_frame_scratch)) {
            frontend_draw_ship_illustration(frontend_frame_scratch);
            frontend_ship_panel_cache_capture(frontend_frame_scratch);
        }
    }
    if (
        selection >= FRONTEND_STATIC_GAME_MENU_COUNT ||
        !frontend_copy_static_menu_panel(
            (u8)(FRONTEND_STATIC_GAME_MENU_BASE + selection)
        )
    ) {
        frontend_draw_game_menu_text(selection);
    }
    frontend_draw_game_menu_dynamic(selection);
}

static void frontend_render_game_menu(void)
{
    frontend_compose_game_menu(frontend_selection);
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = frontend_selection;
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_game_menu_selection(u8 old_selection)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        s16 y = frontend_game_menu_item_y(selection[index]);

        frontend_restore_menu_chrome_rect(120, (u16)y, 120, 9);
        frontend_draw_game_menu_item(
            selection[index],
            frontend_selection
        );
        frontend_dirty_add(120, y, 120, 9);
    }
    frontend_draw_game_menu_cubes();
    frontend_draw_game_menu_help(frontend_selection);
    frontend_dirty_add(
        frontend_source_to_screen_x(206),
        frontend_source_to_screen_y(35),
        (u16)(
            SCREEN_WIDTH -
            frontend_source_to_screen_x(206)
        ),
        (u16)(
            frontend_source_to_screen_y(59) -
            frontend_source_to_screen_y(35)
        )
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(
            SCREEN_HEIGHT -
            frontend_menu_help_dirty_y()
        )
    );
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = frontend_selection;
    frontend_dirty_present();
}

static const char *frontend_upgrade_menu_text(
    u8 index,
    const char *fallback
)
{
    if (
        frontend_text_ready &&
        index < 9 &&
        frontend_text.upgrade_menu[index][0] != '\0'
    ) {
        return frontend_text.upgrade_menu[index];
    }
    return fallback;
}

static const char *frontend_upgrade_menu_item_label(u8 index)
{
    static const char *const fallback[
        FRONTEND_UPGRADE_MENU_ITEM_COUNT
    ] = {
        "Ship Type",
        "Front Gun",
        "Rear Gun",
        "Shield",
        "Generator",
        "Left Sidekick",
        "Right Sidekick",
        "Done"
    };

    return frontend_upgrade_menu_text(
        (u8)(index + 1u),
        fallback[
            index < FRONTEND_UPGRADE_MENU_ITEM_COUNT ? index : 0
        ]
    );
}

static s16 frontend_upgrade_menu_item_y(u8 index)
{
    return (s16)(
        TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_FIRST_Y +
        index * TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_ROW_STEP
    );
}

static void frontend_draw_upgrade_menu_item(u8 index)
{
    frontend_draw_menu_text_clipped(
        frontend_frame_scratch,
        frontend_upgrade_menu_item_label(index),
        TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_X,
        frontend_upgrade_menu_item_y(index),
        TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_RIGHT,
        index == frontend_selection ? 0xfeu : 0xfau
    );
}

static void frontend_draw_upgrade_menu_text(void)
{
    u8 index;

    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        frontend_upgrade_menu_text(0, "Upgrade Ship"),
        TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X,
        TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y,
        0xfbu
    );
    for (
        index = 0;
        index < FRONTEND_UPGRADE_MENU_ITEM_COUNT;
        index++
    ) {
        frontend_draw_upgrade_menu_item(index);
    }
}

static void frontend_upgrade_sort_items(void)
{
    u8 outer;

    for (outer = 0; outer < frontend_upgrade_sub_count; outer++) {
        u8 inner;

        for (
            inner = (u8)(outer + 1u);
            inner < frontend_upgrade_sub_count;
            inner++
        ) {
            u8 left = frontend_upgrade_sub_item[outer];
            u8 right = frontend_upgrade_sub_item[inner];

            if (
                left == 0 ||
                (right != 0 && left > right)
            ) {
                frontend_upgrade_sub_item[outer] = right;
                frontend_upgrade_sub_item[inner] = left;
            }
        }
    }
}

static u8 frontend_upgrade_begin_submenu(u8 category)
{
    const u8 *current_field;
    u8 current;
    u8 group;
    u8 index;
    u8 found = 0;

    if (
        category >= FRONTEND_UPGRADE_CATEGORY_COUNT ||
        !frontend_player_items_valid
    ) {
        return 0;
    }
    current_field = frontend_player_item_field_const(
        &frontend_player_items,
        category
    );
    if (current_field == 0) return 0;
    current = *current_field;
    group = frontend_upgrade_item_group[category];
    frontend_upgrade_sub_count = 0;
    memset(
        frontend_upgrade_sub_item,
        0,
        sizeof(frontend_upgrade_sub_item)
    );
    memset(
        frontend_upgrade_sub_power,
        1,
        sizeof(frontend_upgrade_sub_power)
    );
    if (
        frontend_map.item_inventory_valid &&
        group < OT_EPISODE_ITEM_GROUP_COUNT
    ) {
        u8 count = frontend_map.item_count[group];

        if (count > OT_EPISODE_ITEM_GROUP_CAPACITY) {
            count = OT_EPISODE_ITEM_GROUP_CAPACITY;
        }
        for (index = 0; index < count; index++) {
            frontend_upgrade_sub_item[
                frontend_upgrade_sub_count++
            ] = frontend_map.item_avail[group][index];
        }
    }
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == current) {
            found = 1;
            break;
        }
    }
    if (
        !found &&
        frontend_upgrade_sub_count <
            OT_EPISODE_ITEM_GROUP_CAPACITY + 1u
    ) {
        frontend_upgrade_sub_item[
            frontend_upgrade_sub_count++
        ] = current;
    }
    if (frontend_upgrade_sub_count == 0) {
        frontend_upgrade_sub_item[
            frontend_upgrade_sub_count++
        ] = current;
    }
    frontend_upgrade_sort_items();
    frontend_upgrade_category = category;
    frontend_upgrade_accepted_items = frontend_player_items;
    frontend_upgrade_trade_cash =
        player_cash +
        frontend_upgrade_total_cost(
            category,
            current,
            (
                category == FRONTEND_UPGRADE_FRONT_WEAPON ?
                    frontend_player_items.weapon[
                        FRONTEND_WEAPON_FRONT
                    ].power :
                    (
                        category == FRONTEND_UPGRADE_REAR_WEAPON ?
                            frontend_player_items.weapon[
                                FRONTEND_WEAPON_REAR
                            ].power :
                            1
                    )
            )
        );
    frontend_selection = 0;
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == current) {
            frontend_selection = index;
        }
        if (
            category == FRONTEND_UPGRADE_FRONT_WEAPON ||
            category == FRONTEND_UPGRADE_REAR_WEAPON
        ) {
            frontend_upgrade_sub_power[index] =
                frontend_upgrade_sub_item[index] == current ?
                    frontend_player_items.weapon[
                        category == FRONTEND_UPGRADE_FRONT_WEAPON ?
                            FRONTEND_WEAPON_FRONT :
                            FRONTEND_WEAPON_REAR
                    ].power :
                    1;
        }
    }
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        frontend_upgrade_item_name(
            category,
            frontend_upgrade_sub_item[index],
            frontend_upgrade_sub_name[index],
            sizeof(frontend_upgrade_sub_name[index])
        );
        frontend_upgrade_sub_cost[index] =
            frontend_upgrade_total_cost(
                category,
                frontend_upgrade_sub_item[index],
                frontend_upgrade_sub_power[index]
            );
    }
    frontend_upgrade_sub_scroll = 0;
    /* The cold arena may have been used by Data, Ship Specs or a simulator. */
    frontend_upgrade_ship_preview_cache_valid = 0;
    frontend_upgrade_ship_preview_swap_pending = 0;
    return 1;
}

static void frontend_upgrade_apply_preview(void)
{
    u8 *field;

    if (
        frontend_upgrade_category >=
            FRONTEND_UPGRADE_CATEGORY_COUNT
    ) {
        return;
    }
    if (frontend_selection >= frontend_upgrade_sub_count) {
        /*
         * game_menu.c restores old_items for the Done row.  weapon_mode is
         * a Player field in the source, not part of PlayerItems, so retain
         * its current value across this adapter-structure assignment.
         */
        u8 weapon_mode = frontend_player_items.weapon_mode;

        frontend_player_items = frontend_upgrade_accepted_items;
        frontend_player_items.weapon_mode = weapon_mode;
        frontend_quit_dialog_cache_valid = 0;
        frontend_upgrade_sim_mark_loadout_dirty();
        return;
    }
    field = frontend_player_item_field(
        &frontend_player_items,
        frontend_upgrade_category
    );
    if (field == 0) return;
    *field = frontend_upgrade_sub_item[frontend_selection];
    frontend_quit_dialog_cache_valid = 0;
    if (
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_FRONT_WEAPON ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON
    ) {
        u8 weapon =
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_FRONT_WEAPON ?
                    FRONTEND_WEAPON_FRONT :
                    FRONTEND_WEAPON_REAR;

        frontend_player_items.weapon[weapon].power =
            frontend_upgrade_sub_power[frontend_selection];
        if (weapon == FRONTEND_WEAPON_REAR) {
            frontend_player_items.weapon_mode = 1;
        }
    }
    frontend_upgrade_sim_mark_loadout_dirty();
}

static u32 frontend_upgrade_equipped_cost(void)
{
    const u8 *field = frontend_player_item_field_const(
        &frontend_player_items,
        frontend_upgrade_category
    );
    u8 power = 1;

    if (field == 0) return 0;
    if (
        frontend_selection < frontend_upgrade_sub_count &&
        frontend_upgrade_sub_item[frontend_selection] == *field
    ) {
        return frontend_upgrade_sub_cost[frontend_selection];
    }
    if (
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_FRONT_WEAPON
    ) {
        power = frontend_player_items.weapon[
            FRONTEND_WEAPON_FRONT
        ].power;
    } else if (
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON
    ) {
        power = frontend_player_items.weapon[
            FRONTEND_WEAPON_REAR
        ].power;
    }
    return frontend_upgrade_total_cost(
        frontend_upgrade_category,
        *field,
        power
    );
}

static u32 frontend_upgrade_cash_left(void)
{
    u32 cost = frontend_upgrade_equipped_cost();

    return cost > frontend_upgrade_trade_cash ?
        0 :
        frontend_upgrade_trade_cash - cost;
}

static void frontend_upgrade_accept_preview(void)
{
    frontend_upgrade_accepted_items = frontend_player_items;
}

static void frontend_upgrade_restore_accepted(void)
{
    u8 weapon_mode = frontend_player_items.weapon_mode;

    frontend_player_items = frontend_upgrade_accepted_items;
    frontend_player_items.weapon_mode = weapon_mode;
    frontend_quit_dialog_cache_valid = 0;
    frontend_upgrade_sim_mark_loadout_dirty();
}

static void frontend_upgrade_commit_transaction(void)
{
    frontend_upgrade_restore_accepted();
    player_cash = frontend_upgrade_cash_left();
}

static void frontend_upgrade_cancel_submenu(void)
{
    /* Escape restores game_menu.c old_items, which may already contain the
     * preview accepted by the first confirm, then settles JE_cashLeft(). */
    frontend_upgrade_commit_transaction();
    frontend_start_upgrade_menu_transition(
        frontend_upgrade_category
    );
    audio_effect(SFX_SPRING);
}

static void frontend_upgrade_commit_submenu(void)
{
    frontend_upgrade_commit_transaction();
    frontend_start_upgrade_menu_transition(
        frontend_upgrade_category
    );
    audio_effect(SFX_ITEM);
}

static void frontend_upgrade_move_submenu(s8 direction)
{
    u8 old_selection = frontend_selection;
    u8 old_scroll = frontend_upgrade_sub_scroll;
    u8 item_count = (u8)(frontend_upgrade_sub_count + 1u);
    u8 attempts = 0;

    do {
        if (direction < 0) {
            frontend_selection =
                frontend_selection == 0 ?
                    (u8)(item_count - 1u) :
                    (u8)(frontend_selection - 1u);
        } else {
            frontend_selection =
                frontend_selection + 1u >= item_count ?
                    0 :
                    (u8)(frontend_selection + 1u);
        }
        attempts++;
    } while (
        frontend_selection < frontend_upgrade_sub_count &&
        frontend_upgrade_sub_cost[frontend_selection] >
            frontend_upgrade_trade_cash &&
        attempts < item_count
    );
    if (frontend_upgrade_static_panel_category_active()) {
        u8 candidate_item =
            frontend_selection < frontend_upgrade_sub_count ?
                frontend_upgrade_sub_item[frontend_selection] :
                (
                    frontend_upgrade_category == FRONTEND_UPGRADE_SHIP ?
                        frontend_upgrade_accepted_items.ship :
                        frontend_upgrade_accepted_items.shield
                );

        frontend_upgrade_ship_preview_prepare_candidate(candidate_item);
    }
    frontend_upgrade_apply_preview();
    if (frontend_selection < frontend_upgrade_sub_scroll) {
        frontend_upgrade_sub_scroll = frontend_selection;
    } else if (
        frontend_selection >=
            frontend_upgrade_sub_scroll + 6u
    ) {
        frontend_upgrade_sub_scroll =
            (u8)(frontend_selection - 5u);
    }
    if (frontend_selection != old_selection) {
        frontend_upgrade_sim_play_menu_effect(SFX_CURSOR);
        frontend_render_upgrade_submenu_update(
            old_selection,
            old_scroll
        );
    }
}

static void frontend_upgrade_power_adjust(s8 direction)
{
    u8 old_selection = frontend_selection;
    u8 old_scroll = frontend_upgrade_sub_scroll;
    u8 *power;
    u8 candidate;

    if (
        frontend_selection >= frontend_upgrade_sub_count ||
        (
            frontend_upgrade_category !=
                FRONTEND_UPGRADE_FRONT_WEAPON &&
            frontend_upgrade_category !=
                FRONTEND_UPGRADE_REAR_WEAPON
        ) ||
        frontend_upgrade_sub_item[frontend_selection] == 0
    ) {
        audio_effect(SFX_CLINK);
        return;
    }
    power = &frontend_upgrade_sub_power[frontend_selection];
    candidate = *power;
    if (direction < 0) {
        if (candidate <= 1) {
            audio_effect(SFX_CLINK);
            return;
        }
        candidate--;
    } else {
        if (candidate >= 11) {
            audio_effect(SFX_CLINK);
            return;
        }
        candidate++;
        if (
            frontend_upgrade_total_cost(
                frontend_upgrade_category,
                frontend_upgrade_sub_item[frontend_selection],
                candidate
            ) > frontend_upgrade_trade_cash
        ) {
            audio_effect(SFX_CLINK);
            return;
        }
    }
    *power = candidate;
    frontend_upgrade_sub_cost[frontend_selection] =
        frontend_upgrade_total_cost(
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection],
            candidate
        );
    frontend_upgrade_apply_preview();
    frontend_upgrade_sim_play_menu_effect(SFX_CURSOR);
    frontend_render_upgrade_submenu_update(
        old_selection,
        old_scroll
    );
}

static void frontend_render_upgrade_menu(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    if (!frontend_ship_panel_cache_apply(frontend_frame_scratch)) {
        frontend_draw_ship_illustration(frontend_frame_scratch);
        frontend_ship_panel_cache_capture(frontend_frame_scratch);
    }
    if (
        frontend_selection >= FRONTEND_STATIC_UPGRADE_MENU_COUNT ||
        !frontend_copy_static_menu_panel(
            (u8)(
                FRONTEND_STATIC_UPGRADE_MENU_BASE +
                frontend_selection
            )
        )
    ) {
        frontend_draw_upgrade_menu_text();
    }
    frontend_draw_upgrade_menu_help(frontend_selection);
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_upgrade_menu_selection(u8 old_selection)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        s16 y = frontend_upgrade_menu_item_y(selection[index]);

        frontend_restore_menu_chrome_rect(120, (u16)y, 120, 9);
        frontend_draw_upgrade_menu_item(selection[index]);
        frontend_dirty_add(120, y, 120, 9);
    }
    frontend_draw_upgrade_menu_help(frontend_selection);
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static u8 frontend_render_quit_transition(
    u8 old_state,
    u8 new_state
);

static u8 frontend_render_shared_menu_transition(
    u8 old_state,
    u8 new_state
)
{
    if (
        !frontend_mode4_active ||
        frontend_frame_pending
    ) {
        return 0;
    }
    if (
        (
            old_state == STATE_GAME_MENU &&
            new_state == STATE_QUIT_CONFIRM
        ) ||
        (
            old_state == STATE_QUIT_CONFIRM &&
            new_state == STATE_GAME_MENU
        )
    ) {
        return frontend_render_quit_transition(
            old_state,
            new_state
        );
    }
    if (
        !(
            (
                old_state == STATE_GAME_MENU &&
                new_state == STATE_UPGRADE_MENU
            ) ||
            (
                old_state == STATE_UPGRADE_MENU &&
                new_state == STATE_GAME_MENU
            )
        )
    ) {
        return 0;
    }

    /*
     * Game Menu and Upgrade Ship share the exact same menu chrome and ship
     * illustration.  Keep the already composed left half resident and only
     * replace the native-text panel; rebuilding the old path decoded four
     * SHP sprites and copied the full 38.4 KiB frame on every transition.
    */
    if (new_state == STATE_GAME_MENU) {
        if (
            frontend_selection >= FRONTEND_STATIC_GAME_MENU_COUNT ||
            !frontend_copy_static_menu_panel(
                (u8)(
                    FRONTEND_STATIC_GAME_MENU_BASE +
                    frontend_selection
                )
            )
        ) {
            return 0;
        }
        frontend_draw_game_menu_dynamic(frontend_selection);
        if (
            !frontend_quit_dialog_cache_valid ||
            frontend_quit_dialog_cache_selection !=
                frontend_selection
        ) {
            frontend_quit_capture_dialog_background();
            frontend_quit_dialog_cache_valid = 1;
            frontend_quit_dialog_cache_selection =
                frontend_selection;
        }
    } else {
        if (
            frontend_selection >= FRONTEND_STATIC_UPGRADE_MENU_COUNT ||
            !frontend_copy_static_menu_panel(
                (u8)(
                    FRONTEND_STATIC_UPGRADE_MENU_BASE +
                    frontend_selection
                )
            )
        ) {
            return 0;
        }
        frontend_draw_upgrade_menu_help(frontend_selection);
    }
    /*
     * A half-screen rectangle would require 120 tiny DMA launches because
     * Mode 4 rows are 240 bytes wide.  One contiguous page transfer is both
     * faster and friendlier to Maxmod than that row-wise commit.
     */
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
    return 1;
}

static void frontend_draw_upgrade_submenu_preview_art(void)
{
    if (
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIP ||
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIELD
    ) {
        frontend_draw_ship_illustration(frontend_frame_scratch);
    } else if (frontend_upgrade_sim_category_active()) {
        /*
         * game_menu.c:JE_weaponViewFrame clears the simulator and then
         * blits OPTION_SHAPES frame 12.  Stars, shots, the generator bar
         * and the player ship are supplied by frontend_upgrade_sim.inc as
         * hardware OBJ, but this immutable source interface remains Mode 4.
         */
        frontend_fill_source_rect(
            frontend_frame_scratch,
            8,
            8,
            143,
            182,
            0
        );
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            12,
            0,
            0,
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
    }
}

static void frontend_draw_upgrade_submenu_cash(void)
{
    char digits[11];

    frontend_u32_string(frontend_upgrade_cash_left(), digits);
    frontend_draw_upgrade_list_text(
        frontend_frame_scratch,
        digits,
        frontend_source_to_screen_x(65),
        frontend_source_to_screen_y(173),
        frontend_source_to_screen_x(140),
        0x1eu
    );
}

static void frontend_draw_upgrade_submenu_preview_details(void)
{
    char line[40];
    char digits[11];
    u8 weapon_page;
    u8 valid_weapon;
    u8 power = 1;

    if (!frontend_upgrade_sim_category_active()) return;
    weapon_page =
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_FRONT_WEAPON ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON;
    valid_weapon =
        weapon_page &&
        frontend_selection < frontend_upgrade_sub_count &&
        frontend_upgrade_sub_item[frontend_selection] != 0;
    if (valid_weapon) {
        u16 base_cost = (u16)frontend_upgrade_base_cost(
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection]
        );
        u32 downgrade_cost;
        u32 upgrade_cost;
        u8 left_power;
        u8 right_power;
        u8 right_affordable;
        u8 level;

        power = frontend_upgrade_sub_power[frontend_selection];
        if (power == 0) power = 1;
        if (power > 11) power = 11;
        downgrade_cost =
            ((u32)base_cost * (power - 1u) * power) >> 1;
        upgrade_cost =
            ((u32)base_cost * power * (power + 1u)) >> 1;
        left_power = power > 1;
        right_power = power < 11;
        right_affordable =
            frontend_upgrade_total_cost(
                frontend_upgrade_category,
                frontend_upgrade_sub_item[frontend_selection],
                (u8)(power + 1u)
            ) <= frontend_upgrade_trade_cash;

        if (left_power) {
            frontend_u32_string(downgrade_cost, digits);
            frontend_draw_upgrade_list_text(
                frontend_frame_scratch,
                digits,
                frontend_source_to_screen_x(26),
                frontend_source_to_screen_y(137),
                frontend_source_to_screen_x(58),
                0x1du
            );
        } else {
            (void)frontend_blit_shp_source(
                frontend_frame_scratch,
                5,
                13,
                24,
                149,
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
        if (right_power) {
            frontend_u32_string(upgrade_cost, digits);
            frontend_draw_upgrade_list_text(
                frontend_frame_scratch,
                digits,
                frontend_source_to_screen_x(108),
                frontend_source_to_screen_y(137),
                frontend_source_to_screen_x(140),
                right_affordable ? 0x1du : 0x7cu
            );
            if (!right_affordable) {
                (void)frontend_blit_shp_source(
                    frontend_frame_scratch,
                    5,
                    14,
                    119,
                    149,
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        } else {
            (void)frontend_blit_shp_source(
                frontend_frame_scratch,
                5,
                14,
                119,
                149,
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
        for (level = 1; level <= power; level++) {
            s16 x = (s16)(39 + level * 6);

            frontend_fill_source_rect(
                frontend_frame_scratch,
                x,
                151,
                (s16)(x + 4),
                151,
                251
            );
            frontend_source_plot(
                frontend_frame_scratch,
                x,
                151,
                252,
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
            frontend_fill_source_rect(
                frontend_frame_scratch,
                x,
                152,
                (s16)(x + 4),
                164,
                250
            );
            frontend_fill_source_rect(
                frontend_frame_scratch,
                x,
                165,
                (s16)(x + 4),
                165,
                249
            );
        }
        frontend_text_with_u32(
            line,
            sizeof(line),
            "POWER: ",
            power
        );
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            line,
            frontend_source_to_screen_x(58),
            frontend_source_to_screen_y(137),
            frontend_source_to_screen_x(108),
            0xfeu
        );
    } else {
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            17,
            20,
            146,
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
    }

    /* game_menu.c:JE_drawScore(), after every simulator frame. */
    frontend_draw_upgrade_submenu_cash();
}

static void frontend_draw_upgrade_submenu_preview(void)
{
    frontend_draw_upgrade_submenu_preview_art();
    frontend_upgrade_preview_cache_capture();
    frontend_draw_upgrade_submenu_preview_details();
}

static void frontend_draw_upgrade_submenu_row_content(
    u8 visible_index,
    u8 draw_art
)
{
    u8 item_index = (u8)(
        frontend_upgrade_sub_scroll + visible_index
    );
    s16 y = (s16)(
        FRONTEND_UPGRADE_SUBMENU_ROW_Y +
        visible_index * FRONTEND_UPGRADE_SUBMENU_ROW_STEP
    );
    u8 selected = item_index == frontend_selection;
    char line[40];

    if (item_index < frontend_upgrade_sub_count) {
        u8 item = frontend_upgrade_sub_item[item_index];
        u32 cost = frontend_upgrade_sub_cost[item_index];
        u8 affordable = cost <= frontend_upgrade_trade_cash;
        const u8 *owned = frontend_player_item_field_const(
            &frontend_upgrade_accepted_items,
            frontend_upgrade_category
        );

        frontend_copy_string(
            line,
            sizeof(line),
            frontend_upgrade_sub_name[item_index]
        );
        if (draw_art) {
            frontend_draw_item_source(
                frontend_frame_scratch,
                frontend_upgrade_category,
                item,
                160,
                (s16)(y * 5 / 4 - 4)
            );
        }
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            line,
            142,
            y,
            231,
            !affordable ? 0xf4u :
                (selected ? 0xfeu : 0xfau)
        );
        frontend_text_with_u32(
            line,
            sizeof(line),
            "COST ",
            cost
        );
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            line,
            142,
            (s16)(y + 9),
            201,
            !affordable ? 0xf3u :
                (selected ? 0xfcu : 0xf8u)
        );
        if (owned != 0 && *owned == item && item != 0) {
            frontend_draw_upgrade_list_text(
                frontend_frame_scratch,
                "OWN",
                205,
                (s16)(y + 9),
                231,
                0xedu
            );
        }
    } else if (item_index == frontend_upgrade_sub_count) {
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            (
                frontend_text_ready &&
                frontend_text.misc_text[13][0] != '\0'
            ) ?
                frontend_text.misc_text[13] :
                "DONE",
            133,
            y,
            232,
            selected ? 0xfeu : 0xfau
        );
    }
}

static void frontend_draw_upgrade_submenu_row(u8 visible_index)
{
    frontend_draw_upgrade_submenu_row_content(visible_index, 1);
}

static void frontend_draw_upgrade_submenu_row_text(u8 visible_index)
{
    frontend_draw_upgrade_submenu_row_content(visible_index, 0);
}

extern IWRAM_CODE ARM_CODE void frontend_upgrade_recolour_words_asm(
    u32 *pixels,
    u32 word_count,
    u32 selected
);

static void frontend_recolour_upgrade_submenu_row(
    u8 visible_index,
    u8 selected
)
{
    u8 item_index = (u8)(
        frontend_upgrade_sub_scroll + visible_index
    );
    u8 x0 = item_index < frontend_upgrade_sub_count ? 140u : 132u;
    u8 y0 = (u8)(
        FRONTEND_UPGRADE_SUBMENU_ROW_Y +
        visible_index * FRONTEND_UPGRADE_SUBMENU_ROW_STEP
    );
    u8 y;

    _Static_assert((SCREEN_WIDTH & 3u) == 0, "Mode-4 row must be word aligned");
    _Static_assert((132u & 3u) == 0, "Done row must be word aligned");
    _Static_assert((140u & 3u) == 0, "Item row must be word aligned");
    for (
        y = y0;
        y < y0 + FRONTEND_UPGRADE_SUBMENU_ROW_TEXT_HEIGHT &&
            y < SCREEN_HEIGHT;
        y++
    ) {
        u32 *pixel = (u32 *)(void *)(
            frontend_frame_scratch + (u32)y * SCREEN_WIDTH + x0
        );

        frontend_upgrade_recolour_words_asm(
            pixel,
            (SCREEN_WIDTH - x0) >> 2,
            selected
        );
    }
}

static void frontend_draw_upgrade_submenu_rows(void)
{
    u8 visible_index;

    for (visible_index = 0; visible_index < 6; visible_index++) {
        frontend_draw_upgrade_submenu_row(visible_index);
    }
}

static void frontend_draw_upgrade_submenu_title(void)
{
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        frontend_upgrade_menu_text(
            (u8)(frontend_upgrade_category + 1u),
            "UPGRADE"
        ),
        TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X,
        TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y,
        0xfbu
    );
}

static void frontend_prepare_upgrade_submenu_base(void)
{
    frontend_upgrade_preview_cache_valid = 0;
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_draw_upgrade_submenu_title();
}

static void frontend_render_upgrade_submenu(void)
{
    frontend_prepare_upgrade_submenu_base();
    frontend_draw_upgrade_submenu_preview();
    frontend_draw_upgrade_submenu_rows();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_upgrade_submenu_update(
    u8 old_selection,
    u8 old_scroll
)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    if (
        frontend_upgrade_sim_category_active() &&
        frontend_upgrade_preview_cache_valid &&
        frontend_upgrade_preview_cache_category ==
            frontend_upgrade_category
    ) {
        if (
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_FRONT_WEAPON ||
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_REAR_WEAPON
        ) {
            (void)frontend_upgrade_preview_cache_restore(
                FRONTEND_UPGRADE_PREVIEW_CACHE_X,
                FRONTEND_UPGRADE_PREVIEW_CACHE_Y,
                FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH,
                FRONTEND_UPGRADE_PREVIEW_CACHE_HEIGHT
            );
            frontend_draw_upgrade_submenu_preview_details();
            frontend_dirty_add(
                FRONTEND_UPGRADE_PREVIEW_CACHE_X,
                FRONTEND_UPGRADE_PREVIEW_CACHE_Y,
                FRONTEND_UPGRADE_PREVIEW_CACHE_WIDTH,
                FRONTEND_UPGRADE_PREVIEW_CACHE_HEIGHT
            );
        } else {
            /* Generator/sidekick selection changes only JE_drawScore(). */
            (void)frontend_upgrade_preview_cache_restore(
                FRONTEND_UPGRADE_CASH_DIRTY_X,
                FRONTEND_UPGRADE_CASH_DIRTY_Y,
                FRONTEND_UPGRADE_CASH_DIRTY_WIDTH,
                FRONTEND_UPGRADE_CASH_DIRTY_HEIGHT
            );
            frontend_draw_upgrade_submenu_cash();
            frontend_dirty_add(
                FRONTEND_UPGRADE_CASH_DIRTY_X,
                FRONTEND_UPGRADE_CASH_DIRTY_Y,
                FRONTEND_UPGRADE_CASH_DIRTY_WIDTH,
                FRONTEND_UPGRADE_CASH_DIRTY_HEIGHT
            );
        }
    } else {
        if (!frontend_upgrade_ship_preview_apply_cached()) {
            if (frontend_upgrade_static_panel_category_active()) {
                telemetry_frontend_upgrade_ship_cache_misses++;
            }
            /* Ship/shield illustrations really change with the selection. */
            frontend_restore_menu_chrome_rect(0, 16, 120, 144);
            frontend_draw_upgrade_submenu_preview();
        }
        frontend_dirty_add(0, 16, 120, 144);
    }

    if (old_scroll != frontend_upgrade_sub_scroll) {
        frontend_restore_menu_chrome_rect(120, 16, 120, 132);
        frontend_draw_upgrade_submenu_rows();
        frontend_dirty_add(120, 16, 120, 132);
    } else if (old_selection != frontend_selection) {
        /* Only the two source text ramps change on a cursor move. */
        for (index = 0; index < 2; index++) {
            u8 item_index = selection[index];

            if (
                item_index >= frontend_upgrade_sub_scroll &&
                item_index <
                    frontend_upgrade_sub_scroll + 6u
            ) {
                u8 visible_index = (u8)(
                    item_index - frontend_upgrade_sub_scroll
                );
                u8 x =
                    item_index < frontend_upgrade_sub_count ?
                        140u : 132u;
                s16 y = (s16)(
                    FRONTEND_UPGRADE_SUBMENU_ROW_Y +
                    visible_index * FRONTEND_UPGRADE_SUBMENU_ROW_STEP
                );

                frontend_recolour_upgrade_submenu_row(
                    visible_index,
                    index == 1
                );
                frontend_dirty_add(
                    x,
                    y,
                    (u16)(SCREEN_WIDTH - x),
                    FRONTEND_UPGRADE_SUBMENU_ROW_TEXT_HEIGHT
                );
            }
        }
    } else if (
        frontend_selection >= frontend_upgrade_sub_scroll &&
        frontend_selection < frontend_upgrade_sub_scroll + 6u
    ) {
        /* Power adjustment changes the selected row's numeric cost. */
        u8 visible_index = (u8)(
            frontend_selection - frontend_upgrade_sub_scroll
        );
        u8 x =
            frontend_selection < frontend_upgrade_sub_count ?
                140u : 132u;
        s16 y = (s16)(
            FRONTEND_UPGRADE_SUBMENU_ROW_Y +
            visible_index * FRONTEND_UPGRADE_SUBMENU_ROW_STEP
        );

        frontend_restore_menu_chrome_rect(
            x,
            (u16)y,
            (u16)(SCREEN_WIDTH - x),
            FRONTEND_UPGRADE_SUBMENU_ROW_TEXT_HEIGHT
        );
        frontend_draw_upgrade_submenu_row_text(visible_index);
        frontend_dirty_add(
            x,
            y,
            (u16)(SCREEN_WIDTH - x),
            FRONTEND_UPGRADE_SUBMENU_ROW_TEXT_HEIGHT
        );
    }
    if (frontend_upgrade_static_panel_category_active()) {
        /*
         * A Ship Type or Shield change touches most of the left 120x144
         * panel.  The
         * generic dirty path promotes that rectangle and both text rows to
         * full scanlines, which can exceed one VBlank and visibly tear the
         * in-place page during rapid Up/Down input.  Commit the authoritative
         * scratch frame to the hidden Mode-4 page outside VBlank, then let the
         * next VBlank perform only the atomic page flip.  A partial bitmap is
         * therefore never exposed to the LCD scanout.
         */
        frontend_present_prepared_ewram(
            frontend_frame_scratch,
            frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
        );
    } else {
        frontend_dirty_present();
    }
}

static void frontend_quit_capture_choice_background(void)
{
    u8 row;

    for (row = 0; row < FRONTEND_QUIT_CHOICE_CACHE_HEIGHT; row++) {
        memcpy(
            frontend_quit_choice_background +
                (u16)row * FRONTEND_QUIT_CHOICE_CACHE_WIDTH,
            frontend_frame_scratch +
                (u32)(FRONTEND_QUIT_CHOICE_CACHE_Y + row) *
                    SCREEN_WIDTH +
                FRONTEND_QUIT_CHOICE_CACHE_X,
            FRONTEND_QUIT_CHOICE_CACHE_WIDTH
        );
    }
}

static void frontend_quit_restore_choice_background(void)
{
    u8 row;

    for (row = 0; row < FRONTEND_QUIT_CHOICE_CACHE_HEIGHT; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(FRONTEND_QUIT_CHOICE_CACHE_Y + row) *
                    SCREEN_WIDTH +
                FRONTEND_QUIT_CHOICE_CACHE_X,
            frontend_quit_choice_background +
                (u16)row * FRONTEND_QUIT_CHOICE_CACHE_WIDTH,
            FRONTEND_QUIT_CHOICE_CACHE_WIDTH
        );
    }
}

static void frontend_quit_capture_dialog_background(void)
{
    /*
     * Cache complete scanlines.  The extra 5.9 KiB costs less than 100
     * separate memcpy calls and still leaves 768 bytes in the shared arena.
     */
    memcpy(
        frontend_quit_dialog_background,
        frontend_frame_scratch +
            (u32)FRONTEND_QUIT_DIALOG_CACHE_Y * SCREEN_WIDTH,
        FRONTEND_QUIT_DIALOG_CACHE_BYTES
    );
}

static void frontend_quit_restore_dialog_background(void)
{
    memcpy(
        frontend_frame_scratch +
            (u32)FRONTEND_QUIT_DIALOG_CACHE_Y * SCREEN_WIDTH,
        frontend_quit_dialog_background,
        FRONTEND_QUIT_DIALOG_CACHE_BYTES
    );
}

static void frontend_draw_quit_choices(
    const char *yes,
    const char *no
)
{
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        yes,
        TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X,
        TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y,
        frontend_quit_yes ? 0xfeu : 0xf6u
    );
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        no,
        TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X,
        TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y,
        frontend_quit_yes ? 0xf6u : 0xfeu
    );
}

static void frontend_render_quit_selection(void)
{
    const char *yes =
        frontend_text_ready &&
        frontend_text.misc_text[9][0] != '\0' ?
            frontend_text.misc_text[9] :
            "OK";
    const char *no =
        frontend_text_ready &&
        frontend_text.misc_text[10][0] != '\0' ?
            frontend_text.misc_text[10] :
            "CANCEL";

    if (!frontend_apply_static_quit_choice()) {
        frontend_quit_restore_choice_background();
        frontend_draw_quit_choices(yes, no);
    }
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_QUIT_CHOICE_CACHE_X,
        FRONTEND_QUIT_CHOICE_CACHE_Y,
        FRONTEND_QUIT_CHOICE_CACHE_WIDTH,
        FRONTEND_QUIT_CHOICE_CACHE_HEIGHT
    );
    frontend_dirty_present();
}

static void frontend_compose_quit_shade(void)
{
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    u32 cycle_start = stress_cycle_counter_read();
#endif

    if (!frontend_apply_static_quit_shade()) {
        frontend_shade_source_rect(
            frontend_frame_scratch,
            65,
            55,
            255,
            155,
            3
        );
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    {
        u32 elapsed = stress_cycle_counter_read() - cycle_start;

        if (elapsed > telemetry_frontend_quit_shade_cycles_max) {
            telemetry_frontend_quit_shade_cycles_max = elapsed;
        }
    }
#endif
}

static void frontend_compose_quit_overlay(void)
{
    const char *question =
        frontend_text_ready &&
        frontend_text.misc_text[28][0] != '\0' ?
            frontend_text.misc_text[28] :
            "ARE YOU SURE YOU WANT TO EXIT?";
    const char *help =
        frontend_text_ready &&
        frontend_text.misc_text[30][0] != '\0' ?
            frontend_text.misc_text[30] :
            "YOU WILL RETURN TO THE MAIN MENU.";
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    u32 cycle_start = stress_cycle_counter_read();
#endif

    if (!frontend_apply_static_quit_overlay()) {
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            35,
            50,
            50,
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
        frontend_draw_small_mixed_text_clipped(
            frontend_frame_scratch,
            question,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT,
            0xfeu
        );
        frontend_draw_small_mixed_text_wrapped(
            frontend_frame_scratch,
            help,
            TYRIAN_GBA_LAYOUT_QUIT_HELP_X,
            TYRIAN_GBA_LAYOUT_QUIT_HELP_Y,
            TYRIAN_GBA_LAYOUT_QUIT_HELP_RIGHT,
            9,
            3,
            0xfau
        );
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    {
        u32 elapsed = stress_cycle_counter_read() - cycle_start;

        if (elapsed > telemetry_frontend_quit_overlay_cycles_max) {
            telemetry_frontend_quit_overlay_cycles_max = elapsed;
        }
    }
#endif
}

static void frontend_compose_quit_choices(void)
{
    const char *yes =
        frontend_text_ready &&
        frontend_text.misc_text[9][0] != '\0' ?
            frontend_text.misc_text[9] :
            "OK";
    const char *no =
        frontend_text_ready &&
        frontend_text.misc_text[10][0] != '\0' ?
            frontend_text.misc_text[10] :
            "CANCEL";
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    u32 cycle_start = stress_cycle_counter_read();
#endif

    if (!frontend_apply_static_quit_choice()) {
        frontend_quit_capture_choice_background();
        frontend_draw_quit_choices(yes, no);
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    {
        u32 elapsed = stress_cycle_counter_read() - cycle_start;

        if (elapsed > telemetry_frontend_quit_choices_cycles_max) {
            telemetry_frontend_quit_choices_cycles_max = elapsed;
        }
    }
#endif
}

static void frontend_compose_quit_dialog(void)
{
    frontend_compose_quit_shade();
    frontend_compose_quit_overlay();
    frontend_compose_quit_choices();
}

static u8 frontend_render_quit_transition(
    u8 old_state,
    u8 new_state
)
{
    if (
        old_state == STATE_GAME_MENU &&
        new_state == STATE_QUIT_CONFIRM
    ) {
        /*
         * The confirmation dialog is an overlay on the current Game Menu.
         * Preserve its affected rectangle instead of decoding the four SHP
         * ship sprites and rebuilding all menu chrome a second time.
         */
        if (
            !frontend_quit_dialog_cache_valid ||
            frontend_quit_dialog_cache_selection != 5
        ) {
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
            u32 cycle_start = stress_cycle_counter_read();
#endif
            if (
                !frontend_copy_static_menu_panel(
                    FRONTEND_STATIC_GAME_MENU_BASE + 5u
                )
            ) {
                return 0;
            }
            frontend_draw_game_menu_dynamic(5);
            frontend_quit_capture_dialog_background();
            frontend_quit_dialog_cache_valid = 1;
            frontend_quit_dialog_cache_selection = 5;
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
            {
                u32 elapsed =
                    stress_cycle_counter_read() - cycle_start;

                if (
                    elapsed >
                    telemetry_frontend_quit_capture_cycles_max
                ) {
                    telemetry_frontend_quit_capture_cycles_max =
                        elapsed;
                }
            }
#endif
        }
        frontend_draw_game_menu_dynamic(5);
        frontend_compose_quit_dialog();
    } else if (
        old_state == STATE_QUIT_CONFIRM &&
        new_state == STATE_GAME_MENU
    ) {
        frontend_quit_restore_dialog_background();
    } else {
        return 0;
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
    return 1;
}

static void frontend_render_quit_confirm(void)
{
    frontend_compose_game_menu(5);
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = 5;
    frontend_compose_quit_dialog();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}
