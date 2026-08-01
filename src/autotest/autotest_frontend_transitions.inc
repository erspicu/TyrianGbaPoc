#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS

/* Keep the release gate exhaustive while permitting short local smoke runs. */
#ifndef AUTOTEST_FRONTEND_TRANSITION_ITERATIONS
#define AUTOTEST_FRONTEND_TRANSITION_ITERATIONS 120
#endif

enum {
    FRONTEND_TRANSITION_PATH_GAME_UPGRADE = 0,
    FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE,
    FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE,
    FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY,
    FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME,
    FRONTEND_TRANSITION_PATH_GAME_NEXT,
    FRONTEND_TRANSITION_PATH_GAME_DATA,
    FRONTEND_TRANSITION_PATH_DATA_SELECTION,
    FRONTEND_TRANSITION_PATH_DATA_READER,
    FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS,
    FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU,
    FRONTEND_TRANSITION_PATH_GAME_OPTIONS,
    FRONTEND_TRANSITION_PATH_OPTIONS_SLOTS,
    FRONTEND_TRANSITION_PATH_SAVE_NAME,
    FRONTEND_TRANSITION_PATH_SAVE_NAME_EDIT,
    FRONTEND_TRANSITION_PATH_DATA_READER_SCROLL,
    FRONTEND_TRANSITION_PATH_GAME_QUIT,
    FRONTEND_TRANSITION_PATH_COUNT,
    FRONTEND_TRANSITION_ITERATIONS =
        AUTOTEST_FRONTEND_TRANSITION_ITERATIONS,
};

typedef struct {
    u32 transitions;
    u32 missed_vblanks;
    u32 vblank_irqs;
    u32 full_redraws;
    u32 dirty_commits;
    u32 dirty_bytes;
    u32 runtime_shp_decodes;
    u32 runtime_sprite2_decodes;
    u32 music_active;
    u32 max_cpu_cycles;
    u32 failures;
    u32 phase_cycles[FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT];
} AutotestFrontendTransitionResult;

static AutotestFrontendTransitionResult
    autotest_frontend_transition_result[
        FRONTEND_TRANSITION_PATH_COUNT
    ] EWRAM_BSS;
static u8 autotest_frontend_transition_phase;
static u8 autotest_frontend_transition_path;
static u16 autotest_frontend_transition_count;
static u32 autotest_frontend_transition_vblank_start;
static u32 autotest_frontend_transition_max_cycles;
static u32 autotest_frontend_transition_failures;
static u8 autotest_frontend_data_progression_checked;
static u8 autotest_frontend_upgrade_inventory_checked;
static u8 autotest_frontend_upgrade_transaction_checked;
static u8 autotest_frontend_upgrade_transaction_failed;

typedef struct {
    FrontendPlayerItems items;
    FrontendPlayerItems accepted_items;
    FrontendPlayerItems level_items;
    FrontendSaveSlot save_slot;
    OtEpisodeMap map;
    OtShipDefinition ship_specs;
    OtShipInfo ship_info;
    u8 sub_item[OT_EPISODE_ITEM_GROUP_CAPACITY + 1];
    u8 sub_power[OT_EPISODE_ITEM_GROUP_CAPACITY + 1];
} AutotestFrontendUpgradeSnapshot;

/* Keep the regression snapshot out of the 32 KiB IWRAM IRQ/main stack. */
static AutotestFrontendUpgradeSnapshot
    autotest_frontend_upgrade_snapshot EWRAM_BSS;

/*
 * Source-chain regression for game_menu.c's two-confirm purchase protocol:
 * preview -> Done without accepting restores old_items; preview -> first
 * confirm snapshots old_items; the second confirm charges JE_cashLeft().
 * The committed values must then feed both level setup and save capture.
 */
static u8 autotest_frontend_upgrade_transaction(void)
{
    AutotestFrontendUpgradeSnapshot *snapshot =
        &autotest_frontend_upgrade_snapshot;

    snapshot->items = frontend_player_items;
    snapshot->accepted_items = frontend_upgrade_accepted_items;
    snapshot->save_slot = frontend_save_slot[0];
    snapshot->map = frontend_map;
    snapshot->ship_specs = frontend_ship_specs_ship;
    snapshot->ship_info = *frontend_ship_info_cache;
    snapshot->level_items = (FrontendPlayerItems){
        .ship = source_parity_level.player_ship,
        .weapon = {
            {
                source_parity_level.player_front_weapon_id,
                source_parity_level.player_front_weapon_power
            },
            {
                source_parity_level.player_rear_weapon_id,
                source_parity_level.player_rear_weapon_power
            }
        },
        .shield = source_parity_level.player_shield_item,
        .generator = source_parity_level.player_generator,
        .sidekick = {
            source_parity_level.player_sidekick[0],
            source_parity_level.player_sidekick[1]
        },
        .special = source_parity_level.player_special,
        .sidekick_level = source_parity_level.player_sidekick_level,
        .sidekick_series = source_parity_level.player_sidekick_series,
        .super_arcade_mode =
            source_parity_level.player_super_arcade_mode,
        .weapon_mode = source_parity_level.player_weapon_mode,
    };
    u32 saved_cash = player_cash;
    u32 saved_trade_cash = frontend_upgrade_trade_cash;
    u8 saved_category = frontend_upgrade_category;
    u8 saved_count = frontend_upgrade_sub_count;
    u8 saved_scroll = frontend_upgrade_sub_scroll;
    u8 saved_selection = frontend_selection;
    u8 saved_items_valid = frontend_player_items_valid;
    u8 saved_quit_cache_valid = frontend_quit_dialog_cache_valid;
    u8 saved_quit_cache_selection =
        frontend_quit_dialog_cache_selection;
    u8 item_index = 0xffu;
    u8 power = 1;
    u8 index;
    u8 passed = 1;
    u16 ship_sprite_index = 0;
    s16 ship_x = 0;
    s16 ship_y = 0;
    u32 expected_cash;

    memcpy(
        snapshot->sub_item,
        frontend_upgrade_sub_item,
        sizeof(snapshot->sub_item)
    );
    memcpy(
        snapshot->sub_power,
        frontend_upgrade_sub_power,
        sizeof(snapshot->sub_power)
    );
    frontend_player_items_valid = 1;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id = 1;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power = 1;
    player_cash = 10000u;

    if (!frontend_upgrade_begin_submenu(
            FRONTEND_UPGRADE_FRONT_WEAPON
        )) {
        passed = 0;
        goto restore;
    }
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == 2) {
            item_index = index;
            break;
        }
    }
    if (item_index == 0xffu) {
        passed = 0;
        goto restore;
    }

    /* Merely navigating to Done must restore the entry equipment. */
    frontend_selection = item_index;
    frontend_upgrade_apply_preview();
    frontend_selection = frontend_upgrade_sub_count;
    frontend_upgrade_apply_preview();
    frontend_upgrade_commit_transaction();
    if (
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id != 1 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power != 1 ||
        player_cash != 10000u
    ) {
        passed = 0;
        goto restore;
    }

    /* First confirm accepts item 2; the Done confirm must charge it. */
    if (!frontend_upgrade_begin_submenu(
            FRONTEND_UPGRADE_FRONT_WEAPON
        )) {
        passed = 0;
        goto restore;
    }
    item_index = 0xffu;
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == 2) {
            item_index = index;
            break;
        }
    }
    if (item_index == 0xffu) {
        passed = 0;
        goto restore;
    }
    if (
        frontend_upgrade_total_cost(
            FRONTEND_UPGRADE_FRONT_WEAPON,
            2,
            2
        ) <= frontend_upgrade_trade_cash
    ) {
        power = 2;
    }
    frontend_selection = item_index;
    frontend_upgrade_sub_power[item_index] = power;
    frontend_upgrade_apply_preview();
    expected_cash = frontend_upgrade_trade_cash -
        frontend_upgrade_total_cost(
            FRONTEND_UPGRADE_FRONT_WEAPON,
            2,
            power
        );
    frontend_upgrade_accept_preview();
    frontend_selection = frontend_upgrade_sub_count;
    frontend_upgrade_apply_preview();
    frontend_upgrade_commit_transaction();
    if (
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id != 2 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power != power ||
        player_cash != expected_cash
    ) {
        passed = 0;
        goto restore;
    }

    frontend_campaign_apply_to_level();
    if (
        source_parity_level.player_front_weapon_id != 2 ||
        source_parity_level.player_front_weapon_power != power
    ) {
        passed = 0;
        goto restore;
    }
    frontend_save_capture_slot(0);
    if (
        frontend_save_slot[0].items.weapon[
            FRONTEND_WEAPON_FRONT
        ].id != 2 ||
        frontend_save_slot[0].items.weapon[
            FRONTEND_WEAPON_FRONT
        ].power != power ||
        frontend_save_slot[0].cash != expected_cash
    ) {
        passed = 0;
        goto restore;
    }

    /*
     * A later stock Episode 1 merchant (section 12) offers ship IDs 1, 3
     * and 9.  Buy the Gencore Phoenix through the same two-confirm flow,
     * then prove JE_drawShipSpecs()'s complete source chain changes:
     * PlayerItems.ship -> ships[id] -> shipInfo[id-1] -> bigshipgraphic.
     */
    if (
        !ot_data_episode_map_resolve(
            1,
            12,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            &frontend_map
        ) ||
        !frontend_map.item_inventory_valid
    ) {
        passed = 0;
        goto restore;
    }
    frontend_player_items.ship = 1;
    player_cash = 50000u;
    if (
        !frontend_ship_specs_prepare() ||
        strncmp(frontend_ship_specs_ship.name, "USP Talon", 9) != 0 ||
        strncmp(
            frontend_ship_info_cache->paragraph[0],
            "The ~USP Talon~",
            15
        ) != 0 ||
        frontend_ship_specs_ship.bigshipgraphic != 32 ||
        !frontend_ship_specs_art_parameters(
            &ship_sprite_index,
            &ship_x,
            &ship_y
        ) ||
        ship_sprite_index != 31 ||
        ship_x != 5 ||
        ship_y != 33
    ) {
        passed = 0;
        goto restore;
    }
    if (!frontend_upgrade_begin_submenu(FRONTEND_UPGRADE_SHIP)) {
        passed = 0;
        goto restore;
    }
    item_index = 0xffu;
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == 3) {
            item_index = index;
            break;
        }
    }
    if (item_index == 0xffu) {
        passed = 0;
        goto restore;
    }
    frontend_selection = item_index;
    frontend_upgrade_apply_preview();
    expected_cash = frontend_upgrade_trade_cash -
        frontend_upgrade_total_cost(FRONTEND_UPGRADE_SHIP, 3, 1);
    frontend_upgrade_accept_preview();
    frontend_selection = frontend_upgrade_sub_count;
    frontend_upgrade_apply_preview();
    frontend_upgrade_commit_transaction();
    ship_sprite_index = 0;
    ship_x = 0;
    ship_y = 0;
    if (
        frontend_player_items.ship != 3 ||
        player_cash != expected_cash ||
        !frontend_ship_specs_prepare() ||
        strncmp(
            frontend_ship_specs_ship.name,
            "Gencore Phoenix",
            15
        ) != 0 ||
        strncmp(
            frontend_ship_info_cache->paragraph[0],
            "The ~Gencore Phoenix~",
            21
        ) != 0 ||
        frontend_ship_specs_ship.bigshipgraphic != 28 ||
        !frontend_ship_specs_art_parameters(
            &ship_sprite_index,
            &ship_x,
            &ship_y
        ) ||
        ship_sprite_index != 27 ||
        ship_x != 1 ||
        ship_y != 36 ||
        frontend_ship_specs_text_width("~A~;") !=
            (s16)(
                frontend_small_mixed_font_advance(
                    frontend_pregame_font_tile('A')
                ) +
                frontend_small_mixed_font_advance(
                    frontend_pregame_font_tile(';')
                )
            ) ||
        frontend_pregame_font_tile(';') ==
            frontend_pregame_font_tile('?')
    ) {
        passed = 0;
        goto restore;
    }
    frontend_campaign_apply_to_level();
    frontend_save_capture_slot(0);
    if (
        source_parity_level.player_ship != 3 ||
        frontend_save_slot[0].items.ship != 3
    ) {
        passed = 0;
    }

restore:
    frontend_player_items = snapshot->items;
    frontend_upgrade_accepted_items = snapshot->accepted_items;
    frontend_map = snapshot->map;
    frontend_ship_specs_ship = snapshot->ship_specs;
    *frontend_ship_info_cache = snapshot->ship_info;
    frontend_player_items_valid = saved_items_valid;
    player_cash = saved_cash;
    frontend_upgrade_trade_cash = saved_trade_cash;
    frontend_upgrade_category = saved_category;
    frontend_upgrade_sub_count = saved_count;
    frontend_upgrade_sub_scroll = saved_scroll;
    frontend_selection = saved_selection;
    memcpy(
        frontend_upgrade_sub_item,
        snapshot->sub_item,
        sizeof(snapshot->sub_item)
    );
    memcpy(
        frontend_upgrade_sub_power,
        snapshot->sub_power,
        sizeof(snapshot->sub_power)
    );
    frontend_save_slot[0] = snapshot->save_slot;
    frontend_quit_dialog_cache_valid = saved_quit_cache_valid;
    frontend_quit_dialog_cache_selection =
        saved_quit_cache_selection;
    source_parity_level.player_front_weapon_id =
        snapshot->level_items.weapon[FRONTEND_WEAPON_FRONT].id;
    source_parity_level.player_front_weapon_power =
        snapshot->level_items.weapon[FRONTEND_WEAPON_FRONT].power;
    source_parity_level.player_rear_weapon_id =
        snapshot->level_items.weapon[FRONTEND_WEAPON_REAR].id;
    source_parity_level.player_rear_weapon_power =
        snapshot->level_items.weapon[FRONTEND_WEAPON_REAR].power;
    source_parity_level.player_special = snapshot->level_items.special;
    source_parity_level.player_sidekick[0] =
        snapshot->level_items.sidekick[0];
    source_parity_level.player_sidekick[1] =
        snapshot->level_items.sidekick[1];
    source_parity_level.player_generator =
        snapshot->level_items.generator;
    source_parity_level.player_shield_item =
        snapshot->level_items.shield;
    source_parity_level.player_ship = snapshot->level_items.ship;
    source_parity_level.player_super_arcade_mode =
        snapshot->level_items.super_arcade_mode;
    source_parity_level.player_sidekick_level =
        snapshot->level_items.sidekick_level;
    source_parity_level.player_sidekick_series =
        snapshot->level_items.sidekick_series;
    source_parity_level.player_weapon_mode =
        snapshot->level_items.weapon_mode;
    (void)ot_data_episode_items_select((u8)(frontend_episode + 1u));
    return passed;
}

static void autotest_frontend_transition_counters_reset(
    u32 current_vblank
)
{
    telemetry_missed_vblanks = 0;
    telemetry_frontend_full_redraws = 0;
    telemetry_frontend_dirty_commits = 0;
    telemetry_frontend_dirty_bytes = 0;
    telemetry_frontend_runtime_shp_decodes = 0;
    telemetry_frontend_runtime_sprite2_decodes = 0;
    telemetry_frontend_transition_job_cycles_max = 0;
    memset(
        (void *)telemetry_frontend_transition_phase_cycles_max,
        0,
        sizeof(telemetry_frontend_transition_phase_cycles_max)
    );
    telemetry_state_transitions = 0;
    telemetry_frontend_quit_capture_cycles_max = 0;
    telemetry_frontend_quit_shade_cycles_max = 0;
    telemetry_frontend_quit_overlay_cycles_max = 0;
    telemetry_frontend_quit_choices_cycles_max = 0;
    autotest_frontend_transition_count = 0;
    autotest_frontend_transition_max_cycles = 0;
    autotest_frontend_transition_failures = 0;
    autotest_frontend_data_progression_checked = 0;
    autotest_frontend_upgrade_inventory_checked = 0;
    autotest_frontend_transition_vblank_start =
        telemetry_vblank_irqs;
    last_vblank_seen = current_vblank;
    if (
        autotest_frontend_transition_path ==
            FRONTEND_TRANSITION_PATH_DATA_READER
    ) {
        /*
         * Exercise the first reader open as well as its warm path.  A real
         * player can confirm immediately after entering Data, before the
         * list's idle-time font preparation has completed.
         */
        frontend_data_reader_font_ready = 0;
        frontend_data_reader_font_prepare_index = 0;
    }
}

static u8 autotest_frontend_transition_start_state(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
    case FRONTEND_TRANSITION_PATH_GAME_DATA:
    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
    case FRONTEND_TRANSITION_PATH_GAME_OPTIONS:
    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        return STATE_GAME_MENU;
    case FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE:
        return STATE_TITLE;
    case FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE:
        return STATE_PLAY_MODE_MENU;
    case FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY:
        return STATE_EPISODE_MENU;
    case FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU:
        return STATE_UPGRADE_MENU;
    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
    case FRONTEND_TRANSITION_PATH_DATA_READER:
        return STATE_DATA_CUBES;
    case FRONTEND_TRANSITION_PATH_OPTIONS_SLOTS:
        return STATE_OPTIONS_MENU;
    case FRONTEND_TRANSITION_PATH_SAVE_NAME:
        return STATE_SAVE_SLOTS;
    case FRONTEND_TRANSITION_PATH_SAVE_NAME_EDIT:
        return STATE_SAVE_NAME;
    case FRONTEND_TRANSITION_PATH_DATA_READER_SCROLL:
        return STATE_DATA_CUBE_READER;
    default:
        return STATE_GAME_MENU;
    }
}

static u8 autotest_frontend_transition_start_selection(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
        return 2;
    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
        return 4;
    case FRONTEND_TRANSITION_PATH_GAME_DATA:
        return 0;
    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
        return 1;
    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        return 5;
    case FRONTEND_TRANSITION_PATH_GAME_OPTIONS:
        return 3;
    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
    case FRONTEND_TRANSITION_PATH_DATA_READER:
        return 0;
    default:
        return 0;
    }
}

static void autotest_frontend_transition_prepare_path(u8 path)
{
    u8 state = autotest_frontend_transition_start_state(path);
    u8 selection =
        autotest_frontend_transition_start_selection(path);

    if (
        path == FRONTEND_TRANSITION_PATH_SAVE_NAME ||
        path == FRONTEND_TRANSITION_PATH_SAVE_NAME_EDIT
    ) {
        frontend_save_slot_mode = FRONTEND_SAVE_MODE_SAVE;
        if (path == FRONTEND_TRANSITION_PATH_SAVE_NAME) {
            if (
                game_state != STATE_SAVE_SLOTS ||
                frontend_selection != 0
            ) {
                frontend_enter_state(STATE_SAVE_SLOTS, 0);
            }
        } else if (game_state != STATE_SAVE_NAME) {
            frontend_save_name_begin(0);
        }
        return;
    }
    if (path == FRONTEND_TRANSITION_PATH_DATA_READER_SCROLL) {
        if (
            game_state == STATE_DATA_CUBE_READER &&
            frontend_data_cube_cache[frontend_data_reader_index].last_line >
                FRONTEND_DATA_READER_VISIBLE_LINES
        ) {
            return;
        }
        if (
            game_state == STATE_DATA_CUBES &&
            frontend_data_cache_valid &&
            frontend_data_page_count != 0
        ) {
            u8 index;
            u8 longest = 0;

            for (index = 1; index < frontend_data_page_count; index++) {
                if (
                    frontend_data_cube_cache[index].last_line >
                    frontend_data_cube_cache[longest].last_line
                ) {
                    longest = index;
                }
            }
            if (
                frontend_data_cube_cache[longest].last_line <=
                    FRONTEND_DATA_READER_VISIBLE_LINES
            ) {
                autotest_frontend_transition_failures++;
                return;
            }
            frontend_start_data_reader_transition(longest);
            return;
        }
        if (game_state != STATE_GAME_MENU) {
            frontend_enter_state(STATE_GAME_MENU, 0);
        }
        frontend_start_data_cubes_transition(
            0,
            FRONTEND_DATA_JOB_RELOAD |
                FRONTEND_DATA_JOB_FULL_FRAME
        );
        return;
    }
    if (
        path == FRONTEND_TRANSITION_PATH_DATA_SELECTION ||
        path == FRONTEND_TRANSITION_PATH_DATA_READER
    ) {
        if (
            game_state == STATE_DATA_CUBES &&
            frontend_selection == selection &&
            frontend_data_cache_valid &&
            frontend_data_page_count == 4
        ) {
            return;
        }
        if (game_state != STATE_GAME_MENU) {
            frontend_enter_state(STATE_GAME_MENU, 0);
        }
        frontend_start_data_cubes_transition(
            selection,
            FRONTEND_DATA_JOB_RELOAD |
                FRONTEND_DATA_JOB_FULL_FRAME
        );
        return;
    }
    if (game_state != state || frontend_selection != selection) {
        frontend_enter_state(state, selection);
    }
}

static void autotest_frontend_transition_step(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_upgrade_menu_transition(0);
        } else {
            frontend_start_game_menu_transition_selected(0, 2);
        }
        break;

    case FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE:
        if (game_state == STATE_TITLE) {
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        } else {
            enter_title();
        }
        break;

    case FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE:
        if (game_state == STATE_PLAY_MODE_MENU) {
            frontend_enter_state(STATE_EPISODE_MENU, 0);
        } else {
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY:
        if (game_state == STATE_EPISODE_MENU) {
            frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        } else {
            frontend_enter_state(STATE_EPISODE_MENU, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
        if (game_state == STATE_DIFFICULTY_MENU) {
            /*
             * Level completion changes cash/equipment, so returning to the
             * Game Menu must also prove the cold ship-panel path.  Force a
             * miss here instead of letting 120 identical default loadouts
             * exercise only the warm 19.2 KiB cache.
             */
            frontend_ship_panel_cache_valid = 0;
            frontend_start_game_menu_transition(
                FRONTEND_GAME_MENU_JOB_INITIALIZE_CAMPAIGN |
                FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
            );
        } else {
            frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
        if (game_state == STATE_GAME_MENU) {
            frontend_nav_reset();
            frontend_start_next_level_transition();
        } else {
            frontend_start_game_menu_transition(0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_DATA:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_data_cubes_transition(
                0,
                FRONTEND_DATA_JOB_RELOAD |
                    FRONTEND_DATA_JOB_FULL_FRAME
            );
        } else {
            /*
             * Episode 1 section 2 exposes exactly four records before ]I.
             * This catches regressions which apply ]?, ]! and ]+ only after
             * level entry instead of before the Game Menu, as JE_loadMap()
             * does on PC.
             */
            if (
                frontend_data_cube_count != 4 ||
                frontend_data_page_count != 4 ||
                frontend_data_cube_list[0] != 1 ||
                frontend_data_cube_list[1] != 2 ||
                frontend_data_cube_list[2] != 23 ||
                frontend_data_cube_list[3] != 5 ||
                strcmp(
                    frontend_data_cube_cache[3].header,
                    "Mom"
                ) != 0
            ) {
                autotest_frontend_transition_failures++;
            }
            if (!autotest_frontend_data_progression_checked) {
                if (!frontend_campaign_data_cube_progression_autotest()) {
                    autotest_frontend_transition_failures++;
                }
                autotest_frontend_data_progression_checked = 1;
            }
            frontend_data_cache_valid = 0;
            frontend_start_game_menu_transition_selected(0, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
        if (
            game_state != STATE_DATA_CUBES ||
            frontend_data_page_count != 4
        ) {
            autotest_frontend_transition_failures++;
            break;
        }
        frontend_start_data_selection_transition(1);
        break;

    case FRONTEND_TRANSITION_PATH_DATA_READER:
        if (game_state == STATE_DATA_CUBES) {
            if (frontend_selection >= frontend_data_page_count) {
                frontend_selection = 0;
            }
            frontend_start_data_reader_transition(
                frontend_selection
            );
        } else if (game_state == STATE_DATA_CUBE_READER) {
            frontend_start_data_cubes_transition(
                frontend_data_reader_index,
                0
            );
        } else {
            autotest_frontend_transition_failures++;
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_ship_specs_transition();
        } else {
            frontend_start_game_menu_transition_selected(0, 1);
        }
        break;

    case FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU:
        if (game_state == STATE_UPGRADE_MENU) {
            u8 category;

            if (!autotest_frontend_upgrade_inventory_checked) {
                if (!frontend_campaign_upgrade_inventory_autotest()) {
                    autotest_frontend_transition_failures++;
                }
                for (
                    category = 0;
                    category < FRONTEND_UPGRADE_CATEGORY_COUNT;
                    category++
                ) {
                    u8 group = frontend_upgrade_item_group[category];
                    u8 source_index;

                    if (!frontend_upgrade_begin_submenu(category)) {
                        autotest_frontend_transition_failures++;
                        continue;
                    }
                    for (
                        source_index = 0;
                        source_index < frontend_map.item_count[group];
                        source_index++
                    ) {
                        u8 item = frontend_map.item_avail[
                            group
                        ][source_index];
                        u8 menu_index;
                        u8 found = 0;

                        for (
                            menu_index = 0;
                            menu_index < frontend_upgrade_sub_count;
                            menu_index++
                        ) {
                            if (
                                frontend_upgrade_sub_item[menu_index] ==
                                    item
                            ) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            autotest_frontend_transition_failures++;
                        }
                    }
                }
                autotest_frontend_upgrade_inventory_checked = 1;
            }
            category = (u8)(
                (autotest_frontend_transition_count >> 1) %
                    FRONTEND_UPGRADE_CATEGORY_COUNT
            );
            if (
                !frontend_upgrade_begin_submenu(
                    category
                )
            ) {
                autotest_frontend_transition_failures++;
                return;
            }
            if (
                category == FRONTEND_UPGRADE_FRONT_WEAPON &&
                (
                    frontend_upgrade_sub_count != 4 ||
                    frontend_upgrade_sub_item[0] != 1 ||
                    frontend_upgrade_sub_item[1] != 2 ||
                    frontend_upgrade_sub_item[2] != 7 ||
                    frontend_upgrade_sub_item[3] != 13
                )
            ) {
                autotest_frontend_transition_failures++;
            }
            frontend_start_upgrade_submenu_transition(
                frontend_selection
            );
            frontend_upgrade_apply_preview();
        } else {
            u8 expected_simulator =
                frontend_upgrade_category ==
                    FRONTEND_UPGRADE_FRONT_WEAPON ||
                frontend_upgrade_category ==
                    FRONTEND_UPGRADE_REAR_WEAPON ||
                frontend_upgrade_category ==
                    FRONTEND_UPGRADE_GENERATOR ||
                frontend_upgrade_category ==
                    FRONTEND_UPGRADE_LEFT_SIDEKICK ||
                frontend_upgrade_category ==
                    FRONTEND_UPGRADE_RIGHT_SIDEKICK;

            if (
                frontend_upgrade_sim_is_active() !=
                    expected_simulator
            ) {
                autotest_frontend_transition_failures++;
            }
            if (expected_simulator) {
                u16 ship_pixels = 0;
                u16 tile_index;

                for (
                    tile_index = 0;
                    tile_index <
                        FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES;
                    tile_index++
                ) {
                    if (
                        frontend_upgrade_sim->ship_tiles[tile_index] !=
                            0
                    ) {
                        ship_pixels++;
                    }
                }
                if (
                    !frontend_upgrade_sim->ship_tile_valid ||
                    frontend_upgrade_sim->ship.shipgraphic == 0 ||
                    ship_pixels == 0 ||
                    !frontend_upgrade_sim->bay[0].valid ||
                    previous_oam_count == 0 ||
                    frontend_upgrade_sim->power > 900
                ) {
                    autotest_frontend_transition_failures++;
                }
            }
            frontend_upgrade_cancel_submenu();
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_OPTIONS:
        if (game_state == STATE_GAME_MENU) {
            frontend_enter_state(STATE_OPTIONS_MENU, 0);
        } else {
            frontend_start_game_menu_transition_selected(0, 3);
        }
        break;

    case FRONTEND_TRANSITION_PATH_OPTIONS_SLOTS:
        if (game_state == STATE_OPTIONS_MENU) {
            frontend_save_slot_mode = FRONTEND_SAVE_MODE_LOAD;
            frontend_enter_state(STATE_SAVE_SLOTS, 0);
        } else {
            frontend_enter_state(STATE_OPTIONS_MENU, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_SAVE_NAME:
        if (game_state == STATE_SAVE_SLOTS) {
            frontend_save_name_begin(0);
        } else if (game_state == STATE_SAVE_NAME) {
            frontend_enter_state(STATE_SAVE_SLOTS, 0);
        } else {
            autotest_frontend_transition_failures++;
        }
        break;

    case FRONTEND_TRANSITION_PATH_SAVE_NAME_EDIT:
        if (game_state != STATE_SAVE_NAME) {
            autotest_frontend_transition_failures++;
            break;
        }
        frontend_save_name_cycle(
            (autotest_frontend_transition_count & 1u) == 0 ?
                1 :
                -1
        );
        break;

    case FRONTEND_TRANSITION_PATH_DATA_READER_SCROLL:
        if (game_state != STATE_DATA_CUBE_READER) {
            autotest_frontend_transition_failures++;
            break;
        }
        /*
         * One source pixel per stress operation exercises the incremental
         * renderer 120 times without inflating this suite by 1,440 display
         * frames.  Playable input still advances the stock 12-pixel row.
         */
        frontend_data_scroll_target =
            (autotest_frontend_transition_count & 1u) == 0 ?
                (u16)(frontend_data_scroll_pixel + 1u) :
                (u16)(frontend_data_scroll_pixel - 1u);
        break;

    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        if (game_state == STATE_GAME_MENU) {
            frontend_quit_yes = 1;
            frontend_start_quit_transition(1);
        } else {
            frontend_quit_yes = 0;
            frontend_start_quit_transition(0);
        }
        break;

    default:
        autotest_frontend_transition_failures++;
        break;
    }
    audio_effect(SFX_CLICK);
}

static void autotest_frontend_transition_record(void)
{
    AutotestFrontendTransitionResult *result =
        &autotest_frontend_transition_result[
            autotest_frontend_transition_path
        ];

    result->transitions = autotest_frontend_transition_count;
    result->missed_vblanks = telemetry_missed_vblanks;
    result->vblank_irqs =
        telemetry_vblank_irqs -
        autotest_frontend_transition_vblank_start;
    result->full_redraws = telemetry_frontend_full_redraws;
    result->dirty_commits = telemetry_frontend_dirty_commits;
    result->dirty_bytes = telemetry_frontend_dirty_bytes;
    result->runtime_shp_decodes =
        telemetry_frontend_runtime_shp_decodes;
    result->runtime_sprite2_decodes =
        telemetry_frontend_runtime_sprite2_decodes;
    result->music_active = mmActive();
    result->max_cpu_cycles =
        autotest_frontend_transition_max_cycles >
            telemetry_frontend_transition_job_cycles_max ?
                autotest_frontend_transition_max_cycles :
                telemetry_frontend_transition_job_cycles_max;
    result->failures = autotest_frontend_transition_failures;
    memcpy(
        result->phase_cycles,
        (const void *)
            telemetry_frontend_transition_phase_cycles_max,
        sizeof(result->phase_cycles)
    );
}

static void autotest_frontend_transition_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 path;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'F';
    sram[3] = 'A';
    sram_write_u32(4, 12);
    sram_write_u32(8, FRONTEND_TRANSITION_PATH_COUNT);
    sram_write_u32(12, FRONTEND_TRANSITION_ITERATIONS);
    for (path = 0; path < FRONTEND_TRANSITION_PATH_COUNT; path++) {
        const AutotestFrontendTransitionResult *result =
            &autotest_frontend_transition_result[path];
        u32 offset = 16u + (u32)path * 108u;
        u8 phase;

        sram_write_u32(offset + 0, result->transitions);
        sram_write_u32(offset + 4, result->missed_vblanks);
        sram_write_u32(offset + 8, result->vblank_irqs);
        sram_write_u32(offset + 12, result->full_redraws);
        sram_write_u32(offset + 16, result->dirty_commits);
        sram_write_u32(offset + 20, result->dirty_bytes);
        sram_write_u32(offset + 24, result->runtime_shp_decodes);
        sram_write_u32(
            offset + 28,
            result->runtime_sprite2_decodes
        );
        sram_write_u32(offset + 32, result->music_active);
        sram_write_u32(offset + 36, result->max_cpu_cycles);
        sram_write_u32(offset + 40, result->failures);
        for (
            phase = 0;
            phase < FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT;
            phase++
        ) {
            sram_write_u32(
                offset + 44u + (u32)phase * 4u,
                result->phase_cycles[phase]
            );
        }
    }
    {
        u32 offset =
            16u + FRONTEND_TRANSITION_PATH_COUNT * 108u;

        sram_write_u32(offset + 0, game_state);
        sram_write_u32(offset + 4, frontend_selection);
        sram_write_u32(offset + 8, frontend_frame_pending);
        sram_write_u32(offset + 12, frontend_pending_kind);
        sram_write_u32(
            offset + 16,
            telemetry_frontend_quit_capture_cycles_max
        );
        sram_write_u32(
            offset + 20,
            telemetry_frontend_quit_shade_cycles_max
        );
        sram_write_u32(
            offset + 24,
            telemetry_frontend_quit_overlay_cycles_max
        );
        sram_write_u32(
            offset + 28,
            telemetry_frontend_quit_choices_cycles_max
        );
#ifdef AUTOTEST_STACK_CANARY
        {
            u32 stack_remaining = autotest_stack_canary_remaining();

            sram_write_u32(offset + 32, stack_remaining);
            sram_write_u32(
                offset + 36,
                stack_remaining >= sizeof(u32) * 8u ? 1u : 0u
            );
            sram_write_u32(
                offset + 40,
                autotest_stack_canary_filled_bytes
            );
            sram_write_u32(offset + 44, autotest_ewram_heap_used());
            sram_write_u32(offset + 48, autotest_ewram_heap_remaining());
        }
#endif
    }
    __asm__ volatile("swi 3");
}

static void autotest_frontend_transition_update(u32 current_vblank)
{
    if (frontend_transition_busy()) return;
    if (
        autotest_frontend_transition_phase == 0 &&
        game_state == STATE_GAME_MENU &&
        !frontend_frame_pending
    ) {
        autotest_frontend_transition_ready = 1;
        autotest_frontend_transition_path = 0;
        autotest_frontend_transition_prepare_path(0);
        autotest_frontend_transition_phase = 1;
        return;
    }
    if (autotest_frontend_transition_phase == 1) {
        u8 expected_state =
            autotest_frontend_transition_start_state(
                autotest_frontend_transition_path
            );
        u8 expected_selection =
            autotest_frontend_transition_start_selection(
                autotest_frontend_transition_path
            );

        if (frontend_frame_pending) return;
        if (
            game_state != expected_state ||
            (
                autotest_frontend_transition_path !=
                    FRONTEND_TRANSITION_PATH_DATA_READER_SCROLL &&
                frontend_selection != expected_selection
            )
        ) {
            autotest_frontend_transition_prepare_path(
                autotest_frontend_transition_path
            );
            return;
        }
        if (
            autotest_frontend_transition_path ==
                FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU &&
            !autotest_frontend_upgrade_transaction_checked
        ) {
            autotest_frontend_upgrade_transaction_failed =
                !autotest_frontend_upgrade_transaction();
            autotest_frontend_upgrade_transaction_checked = 1;
            /* Re-enter phase 1 on a fresh LCD period before counters reset. */
            return;
        }
        autotest_frontend_transition_counters_reset(
            current_vblank
        );
        if (
            autotest_frontend_transition_path ==
                FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU &&
            autotest_frontend_upgrade_transaction_failed
        ) {
            autotest_frontend_transition_failures++;
        }
        autotest_frontend_transition_phase = 2;
        return;
    }
    if (autotest_frontend_transition_phase != 2) return;
    if (frontend_frame_pending) return;
    if (
        autotest_frontend_transition_count <
            FRONTEND_TRANSITION_ITERATIONS
    ) {
        u32 cycle_start = stress_cycle_counter_read();
        u32 elapsed;

        autotest_frontend_transition_step(
            autotest_frontend_transition_path
        );
        elapsed = stress_cycle_counter_read() - cycle_start;
        if (elapsed > autotest_frontend_transition_max_cycles) {
            autotest_frontend_transition_max_cycles = elapsed;
        }
        autotest_frontend_transition_count++;
        return;
    }

    autotest_frontend_transition_record();
    autotest_frontend_transition_path++;
    if (
        autotest_frontend_transition_path >=
            FRONTEND_TRANSITION_PATH_COUNT
    ) {
        autotest_frontend_transition_finish();
        return;
    }
    autotest_frontend_transition_prepare_path(
        autotest_frontend_transition_path
    );
    autotest_frontend_transition_phase = 1;
}

#endif
