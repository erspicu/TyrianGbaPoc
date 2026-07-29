static const char *frontend_game_menu_label(u8 index)
{
    static const char *const fallback[7] = {
        "GAME MENU",
        "DATA CUBES",
        "SHIP SPECS",
        "UPGRADE SHIP",
        "OPTIONS",
        "PLAY NEXT LEVEL",
        "QUIT GAME",
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

static const char *frontend_pre_game_title_label(u8 selection)
{
    static const char *const fallback[3] = {
        "Start New Game",
        "Demo",
        "Jukebox",
    };
    static const u8 source_index[3] = {0, 5, 0xff};

    if (
        selection < 3 &&
        source_index[selection] != 0xff &&
        frontend_text_ready &&
        frontend_text.title_menu[source_index[selection]][0] != '\0'
    ) {
        return frontend_text.title_menu[source_index[selection]];
    }
    return fallback[selection < 3 ? selection : 0];
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
    u8 colour = selection == active_selection ? 0xfeu : 0xfau;

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
        frontend_present(
            frontend_static_pre_game_frames +
                (u32)(static_base + frontend_selection) *
                    FRONTEND_FRAME_BYTES,
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
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(base_frame)
    );
}

static void frontend_render_pre_game_selection(u8 old_selection)
{
    u8 base_frame =
        game_state == STATE_TITLE ?
            FRONTEND_FRAME_TITLE_CHROME :
            FRONTEND_FRAME_SELECT_CHROME;
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

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
        frontend_restore_frame_rect(
            base_frame,
            x,
            y,
            width,
            height
        );
        frontend_draw_pre_game_item(
            game_state,
            selection[index],
            frontend_selection
        );
        frontend_dirty_add(x, y, width, height);
    }
    frontend_dirty_present();
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
    u8 disabled = (
        selection == 0 ||
        selection == 1 ||
        selection == 3
    );
    u8 colour;

    if (disabled) {
        colour =
            selection == active_selection ? 0xf8u : 0xf4u;
    } else {
        colour =
            selection == active_selection ? 0xfeu : 0xfau;
    }
    frontend_draw_runtime_text_clipped(
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

    frontend_draw_runtime_text_centered(
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
        frontend_draw_ship_illustration(frontend_frame_scratch);
    }
    if (
        selection >= FRONTEND_STATIC_GAME_MENU_COUNT ||
        !frontend_copy_static_menu_panel(
            (u8)(FRONTEND_STATIC_GAME_MENU_BASE + selection)
        )
    ) {
        frontend_draw_game_menu_text(selection);
    }
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
        "SHIP TYPE",
        "FRONT GUN",
        "REAR GUN",
        "SHIELD",
        "GENERATOR",
        "LEFT SIDEKICK",
        "RIGHT SIDEKICK",
        "DONE"
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
    frontend_draw_runtime_text_clipped(
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

    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        frontend_upgrade_menu_text(0, "UPGRADE SHIP"),
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
    frontend_upgrade_original_items = frontend_player_items;
    frontend_upgrade_original_cash = player_cash;
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
    frontend_upgrade_sub_scroll = 0;
    return 1;
}

static void frontend_upgrade_apply_preview(void)
{
    u8 *field;

    if (
        frontend_selection >= frontend_upgrade_sub_count ||
        frontend_upgrade_category >=
            FRONTEND_UPGRADE_CATEGORY_COUNT
    ) {
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
}

static u32 frontend_upgrade_current_cost(void)
{
    if (frontend_selection >= frontend_upgrade_sub_count) return 0;
    return frontend_upgrade_total_cost(
        frontend_upgrade_category,
        frontend_upgrade_sub_item[frontend_selection],
        frontend_upgrade_sub_power[frontend_selection]
    );
}

static u32 frontend_upgrade_cash_left(void)
{
    u32 cost = frontend_upgrade_current_cost();

    return cost > frontend_upgrade_trade_cash ?
        0 :
        frontend_upgrade_trade_cash - cost;
}

static void frontend_upgrade_cancel_submenu(void)
{
    frontend_player_items = frontend_upgrade_original_items;
    player_cash = frontend_upgrade_original_cash;
    frontend_quit_dialog_cache_valid = 0;
    frontend_enter_state(
        STATE_UPGRADE_MENU,
        frontend_upgrade_category
    );
    audio_effect(SFX_SPRING);
}

static void frontend_upgrade_commit_submenu(void)
{
    player_cash = frontend_upgrade_cash_left();
    frontend_enter_state(
        STATE_UPGRADE_MENU,
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
        frontend_upgrade_total_cost(
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection],
            frontend_upgrade_sub_power[frontend_selection]
        ) > frontend_upgrade_trade_cash &&
        attempts < item_count
    );
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
        audio_effect(SFX_CURSOR);
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
    frontend_upgrade_apply_preview();
    audio_effect(SFX_CURSOR);
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
    frontend_draw_ship_illustration(frontend_frame_scratch);
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

static void frontend_draw_upgrade_submenu_preview(void)
{
    char line[40];
    char digits[11];

    if (
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIP ||
        frontend_upgrade_category == FRONTEND_UPGRADE_GENERATOR
    ) {
        frontend_draw_ship_illustration(frontend_frame_scratch);
    } else if (frontend_selection < frontend_upgrade_sub_count) {
        frontend_draw_item_source(
            frontend_frame_scratch,
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection],
            55,
            70
        );
    }
    if (
        (
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_FRONT_WEAPON ||
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_REAR_WEAPON
        ) &&
        frontend_selection < frontend_upgrade_sub_count
    ) {
        frontend_text_with_u32(
            line,
            sizeof(line),
            "POWER ",
            frontend_upgrade_sub_power[frontend_selection]
        );
        frontend_draw_runtime_text_clipped(
            frontend_frame_scratch,
            line,
            20,
            118,
            111,
            0xfeu
        );
        frontend_draw_runtime_text_clipped(
            frontend_frame_scratch,
            "< LEFT  RIGHT >",
            8,
            130,
            114,
            0xfau
        );
        if (
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_REAR_WEAPON
        ) {
            frontend_text_with_u32(
                line,
                sizeof(line),
                "MODE ",
                frontend_player_items.weapon_mode
            );
            frontend_draw_runtime_text_clipped(
                frontend_frame_scratch,
                line,
                26,
                142,
                111,
                0xfau
            );
        }
    }
    frontend_u32_string(frontend_upgrade_cash_left(), digits);
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        "CASH",
        8,
        148,
        45,
        0xfau
    );
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        digits,
        45,
        148,
        115,
        0xfeu
    );
}

static void frontend_draw_upgrade_submenu_row(u8 visible_index)
{
    u8 item_index = (u8)(
        frontend_upgrade_sub_scroll + visible_index
    );
    s16 y = (s16)(21 + visible_index * 21u);
    u8 selected = item_index == frontend_selection;
    char line[40];

    if (item_index < frontend_upgrade_sub_count) {
        u8 item = frontend_upgrade_sub_item[item_index];
        u32 cost = frontend_upgrade_total_cost(
            frontend_upgrade_category,
            item,
            frontend_upgrade_sub_power[item_index]
        );
        u8 affordable = cost <= frontend_upgrade_trade_cash;
        const u8 *owned = frontend_player_item_field_const(
            &frontend_upgrade_original_items,
            frontend_upgrade_category
        );

        frontend_upgrade_item_name(
            frontend_upgrade_category,
            item,
            line,
            sizeof(line)
        );
        frontend_draw_item_source(
            frontend_frame_scratch,
            frontend_upgrade_category,
            item,
            160,
            (s16)(y * 5 / 4 - 4)
        );
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

static void frontend_draw_upgrade_submenu_rows(void)
{
    u8 visible_index;

    for (visible_index = 0; visible_index < 6; visible_index++) {
        frontend_draw_upgrade_submenu_row(visible_index);
    }
}

static void frontend_render_upgrade_submenu(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
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
    /*
     * The left preview is the only configuration-dependent illustration.
     * Rebuild it without touching the 120-pixel-wide list or page chrome.
     */
    frontend_restore_menu_chrome_rect(0, 16, 120, 144);
    frontend_draw_upgrade_submenu_preview();
    frontend_dirty_add(0, 16, 120, 144);

    if (old_scroll != frontend_upgrade_sub_scroll) {
        frontend_restore_menu_chrome_rect(120, 16, 120, 132);
        frontend_draw_upgrade_submenu_rows();
        frontend_dirty_add(120, 16, 120, 132);
    } else {
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
                s16 y = (s16)(17 + visible_index * 21u);

                frontend_restore_menu_chrome_rect(
                    120,
                    (u16)y,
                    120,
                    27
                );
                frontend_draw_upgrade_submenu_row(visible_index);
                frontend_dirty_add(120, y, 120, 27);
            }
        }
    }
    frontend_dirty_present();
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

static void frontend_compose_quit_dialog(void)
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
    u32 elapsed;
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
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_shade_cycles_max) {
        telemetry_frontend_quit_shade_cycles_max = elapsed;
    }
    cycle_start = stress_cycle_counter_read();
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
        frontend_draw_runtime_text_clipped_scaled(
            frontend_frame_scratch,
            question,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y,
            TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT,
            0xfeu,
            FRONTEND_COMPACT_FONT_WIDTH,
            FRONTEND_COMPACT_FONT_SPACE
        );
        frontend_draw_runtime_text_wrapped_compact(
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
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_overlay_cycles_max) {
        telemetry_frontend_quit_overlay_cycles_max = elapsed;
    }
    cycle_start = stress_cycle_counter_read();
#endif
    if (!frontend_apply_static_quit_choice()) {
        frontend_quit_capture_choice_background();
        frontend_draw_quit_choices(yes, no);
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_choices_cycles_max) {
        telemetry_frontend_quit_choices_cycles_max = elapsed;
    }
#endif
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
