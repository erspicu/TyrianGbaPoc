#ifdef AUTOTEST_SAVE_FLOW
static void autotest_save_flow(void)
{
    volatile u8 *sram = (volatile u8 *)0x0e000000;
    u32 failures = 0;
    u32 season_failures = 0;
    u32 sequence = 0;
    u16 corrupt_offset;
    u16 index;
    u8 slot = 3;

    {
        const u8 *ordinary;
        const u8 *christmas;

        if (
            ot_season_from_save_name("XMAS") != OT_SEASON_XMAS ||
            ot_season_from_save_name("xmas") != OT_SEASON_XMAS ||
            ot_season_from_save_name("xMaS") != OT_SEASON_XMAS ||
            ot_season_from_save_name("PUMP") != OT_SEASON_NONE ||
            ot_season_from_save_name("pump") != OT_SEASON_NONE ||
            ot_season_from_save_name("PuMp") != OT_SEASON_NONE ||
            ot_season_from_save_name("XMAS1") != OT_SEASON_NONE ||
            ot_season_from_save_name("PUMPKIN") != OT_SEASON_NONE ||
            ot_season_from_save_name("ordinary") != OT_SEASON_NONE
        ) {
            season_failures |= 1u << 0;
        }
        (void)ot_season_set(OT_SEASON_NONE);
        ordinary = ot_sprite2_raw_component(36, 1);
        (void)ot_season_set(OT_SEASON_XMAS);
        christmas = ot_sprite2_raw_component(36, 1);
        if (
            ordinary == 0 ||
            christmas == 0 ||
            ordinary == christmas ||
            memcmp(
                ordinary,
                christmas,
                SPRITE2_RAW_COMPONENT_BYTES
            ) == 0
        ) {
            season_failures |= 1u << 1;
        }
        (void)ot_season_set(OT_SEASON_NONE);
    }

    /* Each regression starts from deterministic blank campaign banks. */
    sram[FRONTEND_SAVE_BANK_0_OFFSET] = 0;
    sram[FRONTEND_SAVE_BANK_1_OFFSET] = 0;
    frontend_save_initialize();
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = 2;
    frontend_difficulty = FRONTEND_DIFFICULTY_HARD;
    frontend_main_section = 42;
    frontend_campaign_initialize();
    frontend_player_items.ship = 7;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id = 19;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power = 11;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id = 12;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power = 8;
    frontend_player_items.sidekick[0] = 14;
    frontend_player_items.sidekick[1] = 15;
    frontend_player_items.special = 4;
    frontend_player_items.generator = 5;
    frontend_player_items.shield = 6;
    frontend_player_items.weapon_mode = 2;
    player_cash = 123456u;
    player_armor = 17;
    player_shield = 9;
    player_shield_max = 20;
    frontend_data_cube_count = 4;
    frontend_data_cube_list[0] = 9;
    frontend_data_cube_list[1] = 4;
    frontend_data_cube_list[2] = 7;
    frontend_data_cube_list[3] = 2;
    frontend_secret_hint = 3;
    frontend_save_string_copy(
        frontend_save_name,
        sizeof(frontend_save_name),
        "AprPilot"
    );
    frontend_save_name_slot = slot;
    frontend_save_write_begin(slot);
    while (frontend_save_write_active) {
        frontend_save_write_update();
    }
    if (
        !frontend_save_has_active_bank ||
        sram[
            frontend_save_active_bank == 0 ?
                FRONTEND_SAVE_BANK_0_OFFSET :
                FRONTEND_SAVE_BANK_1_OFFSET
        ] != FRONTEND_SAVE_COMMIT
    ) {
        failures |= 1u << 0;
    }
    if (
        !frontend_save_bank_read(
            frontend_save_active_bank,
            &sequence
        ) ||
        sequence != 1 ||
        !frontend_save_page_decode()
    ) {
        failures |= 1u << 1;
    }

    /*
     * Create a newer-looking but CRC-corrupt inactive bank.  Reinitializing
     * must retain the last committed valid snapshot.
     */
    corrupt_offset =
        frontend_save_active_bank == 0 ?
            FRONTEND_SAVE_BANK_1_OFFSET :
            FRONTEND_SAVE_BANK_0_OFFSET;
    frontend_save_u32_write(frontend_save_page + 8, sequence + 1u);
    frontend_save_u32_write(
        frontend_save_page + 16,
        frontend_save_page_crc(frontend_save_page)
    );
    for (index = 1; index < FRONTEND_SAVE_PAGE_BYTES; index++) {
        sram[corrupt_offset + index] = frontend_save_page[index];
    }
    sram[
        corrupt_offset + FRONTEND_SAVE_HEADER_BYTES + 5u
    ] ^= 0x5au;
    sram[corrupt_offset] = FRONTEND_SAVE_COMMIT;
    memset(frontend_save_slot, 0, sizeof(frontend_save_slot));
    frontend_save_initialize();
    if (
        frontend_save_sequence != 1 ||
        !frontend_save_slot[slot].occupied
    ) {
        failures |= 1u << 2;
    }

    frontend_play_mode = FRONTEND_PLAY_ARCADE;
    frontend_episode = 0;
    frontend_difficulty = FRONTEND_DIFFICULTY_EASY;
    frontend_main_section = 1;
    memset(&frontend_player_items, 0, sizeof(frontend_player_items));
    player_cash = 0;
    player_armor = 0;
    player_shield = 0;
    player_shield_max = 0;
    frontend_data_cube_count = 0;
    frontend_secret_hint = 0;
    memset(frontend_data_cube_list, 0, sizeof(frontend_data_cube_list));
    if (!frontend_save_apply_slot(slot)) {
        failures |= 1u << 3;
    }
    if (
        frontend_play_mode != FRONTEND_PLAY_FULL_GAME ||
        frontend_episode != 2 ||
        frontend_difficulty != FRONTEND_DIFFICULTY_HARD ||
        frontend_main_section != 42 ||
        player_cash != 123456u ||
        player_armor != 17 ||
        player_shield != 9 ||
        player_shield_max != 20
    ) {
        failures |= 1u << 4;
    }
    if (
        frontend_player_items.ship != 7 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id != 19 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power != 11 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id != 12 ||
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power != 8 ||
        frontend_player_items.sidekick[0] != 14 ||
        frontend_player_items.sidekick[1] != 15 ||
        frontend_player_items.special != 4 ||
        frontend_player_items.generator != 5 ||
        frontend_player_items.shield != 6 ||
        frontend_player_items.weapon_mode != 2
    ) {
        failures |= 1u << 5;
    }
    if (
        frontend_data_cube_count != 4 ||
        frontend_data_cube_list[0] != 9 ||
        frontend_data_cube_list[1] != 4 ||
        frontend_data_cube_list[2] != 7 ||
        frontend_data_cube_list[3] != 2 ||
        frontend_secret_hint != 3 ||
        strcmp(frontend_save_slot[slot].name, "AprPilot") != 0
    ) {
        failures |= 1u << 6;
    }
    {
        u16 ship_sprite_index = 0;
        s16 ship_x = 0;
        s16 ship_y = 0;

        /*
         * Save/load must feed JE_drawShipSpecs() through the restored
         * PlayerItems.ship, not through a fixed Talon presentation.
         */
        if (
            !frontend_ship_specs_prepare() ||
            strncmp(
                frontend_ship_specs_ship.name,
                "Prototype Stalker-C",
                19
            ) != 0 ||
            strncmp(
                frontend_ship_info_cache->paragraph[0],
                "~TOP SECRET:~",
                13
            ) != 0 ||
            frontend_ship_specs_ship.bigshipgraphic != 33 ||
            !frontend_ship_specs_art_parameters(
                &ship_sprite_index,
                &ship_x,
                &ship_y
            ) ||
            ship_sprite_index != 32 ||
            ship_x != 1 ||
            ship_y != 35
        ) {
            failures |= 1u << 7;
        }
    }

    /*
     * Title Load and Options Load/Save deliberately share the SRAM model and
     * controller, but preserve the two distinct PC presentation contexts.
     * Title cancel returns to its highlighted item; Options cancel returns to
     * the matching Load/Save row on the PIC 1 Game Menu chrome.
     */
    frontend_save_open_slots(
        FRONTEND_SAVE_MODE_LOAD,
        STATE_TITLE,
        1
    );
    if (
        game_state != STATE_SAVE_SLOTS ||
        frontend_save_slot_mode != FRONTEND_SAVE_MODE_LOAD ||
        frontend_save_return_state != STATE_TITLE ||
        !frontend_save_slots_patch_pending
    ) {
        failures |= 1u << 8;
    }
    /* This synchronous unit path has no VBlank loop to commit the static
     * page and its bounded row patch; production and transition stress do. */
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_save_slots_patch_pending = 0;
    pad_pressed = KEY_B;
    if (
        !frontend_save_frontend_update() ||
        game_state != STATE_TITLE ||
        frontend_selection != 1
    ) {
        failures |= 1u << 9;
    }
    frontend_save_open_slots(
        FRONTEND_SAVE_MODE_LOAD,
        STATE_OPTIONS_MENU,
        0
    );
    if (
        game_state != STATE_SAVE_SLOTS ||
        frontend_save_title_context() ||
        frontend_save_game_slots_panel() !=
            FRONTEND_STATIC_SAVE_MENU_BASE
    ) {
        failures |= 1u << 11;
    }
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_save_slots_patch_pending = 0;
    pad_pressed = KEY_B;
    if (
        !frontend_save_frontend_update() ||
        game_state != STATE_OPTIONS_MENU ||
        frontend_selection != 0
    ) {
        failures |= 1u << 12;
    }
    frontend_save_open_slots(
        FRONTEND_SAVE_MODE_SAVE,
        STATE_OPTIONS_MENU,
        1
    );
    if (
        game_state != STATE_SAVE_SLOTS ||
        frontend_save_title_context() ||
        frontend_save_slot_mode != FRONTEND_SAVE_MODE_SAVE ||
        frontend_save_game_slots_panel() !=
            FRONTEND_STATIC_SAVE_MENU_BASE + 1u
    ) {
        failures |= 1u << 13;
    }
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_save_slots_patch_pending = 0;
    pad_pressed = KEY_B;
    if (
        !frontend_save_frontend_update() ||
        game_state != STATE_OPTIONS_MENU ||
        frontend_selection != 1
    ) {
        failures |= 1u << 14;
    }
    {
        u16 x;
        u16 row = TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_FIRST_Y;

        /* MENU_LOAD_SAVE row patches must never touch the live ship/status
         * half of PIC 1.  This catches the former 240-pixel row restore that
         * sliced the left illustration into horizontal background bands. */
        frontend_save_return_state = STATE_OPTIONS_MENU;
        frontend_save_slot_mode = FRONTEND_SAVE_MODE_LOAD;
        for (x = 0; x < FRONTEND_STATIC_MENU_PANEL_X; x++) {
            frontend_frame_scratch[(u32)row * SCREEN_WIDTH + x] =
                (u8)(0x40u ^ x);
        }
        frontend_save_clear_slot_row(0);
        for (x = 0; x < FRONTEND_STATIC_MENU_PANEL_X; x++) {
            if (
                frontend_frame_scratch[(u32)row * SCREEN_WIDTH + x] !=
                    (u8)(0x40u ^ x)
            ) {
                failures |= 1u << 15;
                break;
            }
        }
    }
    {
        u32 recolour_checksum = 2166136261u;
        u32 redraw_checksum = 2166136261u;
        u8 old_selection = slot;
        u8 new_selection = (u8)(slot + 1u);
        u8 row_index;
        u16 x;

        /*
         * Title slot navigation keeps the source FULL_SHADE background and
         * only repaints main glyph pixels.  Its result must remain bit-exact
         * to an expensive clear + complete redraw; otherwise repeated cursor
         * movement could trade the audio fix for text/shade artefacts.
         */
        frontend_save_return_state = STATE_TITLE;
        memcpy(
            frontend_frame_scratch,
            frontend_save_title_slots_frame(),
            FRONTEND_FRAME_BYTES
        );
        frontend_save_draw_title_slot(old_selection, 1);
        frontend_save_draw_title_slot(new_selection, 0);
        frontend_selection = new_selection;
        frontend_render_save_slot_selection(old_selection);
        for (row_index = 0; row_index < 2; row_index++) {
            u8 selection = row_index == 0 ? old_selection : new_selection;
            u16 y = (u16)frontend_save_slot_y(selection);
            u16 row;

            for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
                for (x = 0; x < SCREEN_WIDTH; x++) {
                    recolour_checksum ^=
                        frontend_frame_scratch[
                            (u32)(y + row) * SCREEN_WIDTH + x
                        ];
                    recolour_checksum *= 16777619u;
                }
            }
        }
        frontend_frame_pending = 0;
        frontend_pending_kind = FRONTEND_PENDING_NONE;
        frontend_dirty_count = 0;
        frontend_save_clear_slot_row(old_selection);
        frontend_save_draw_title_slot(old_selection, 0);
        frontend_save_clear_slot_row(new_selection);
        frontend_save_draw_title_slot(new_selection, 1);
        for (row_index = 0; row_index < 2; row_index++) {
            u8 selection = row_index == 0 ? old_selection : new_selection;
            u16 y = (u16)frontend_save_slot_y(selection);
            u16 row;

            for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
                for (x = 0; x < SCREEN_WIDTH; x++) {
                    redraw_checksum ^=
                        frontend_frame_scratch[
                            (u32)(y + row) * SCREEN_WIDTH + x
                        ];
                    redraw_checksum *= 16777619u;
                }
            }
        }
        if (recolour_checksum != redraw_checksum) {
            failures |= 1u << 15;
        }
    }
    frontend_save_open_slots(
        FRONTEND_SAVE_MODE_LOAD,
        STATE_TITLE,
        1
    );
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_save_slots_patch_pending = 0;
    frontend_selection = slot;
    frontend_main_section = 1;
    player_cash = 0;
    pad_pressed = KEY_A;
    if (
        !frontend_save_frontend_update() ||
        frontend_main_section != 42 ||
        player_cash != 123456u ||
        frontend_transition_job != FRONTEND_TRANSITION_JOB_GAME_MENU
    ) {
        failures |= 1u << 10;
    }
    pad_pressed = 0;

    frontend_save_string_copy(
        frontend_save_slot[slot].name,
        sizeof(frontend_save_slot[slot].name),
        "xMaS"
    );
    if (
        !frontend_save_apply_slot(slot) ||
        ot_season_mode() != OT_SEASON_XMAS
    ) {
        season_failures |= 1u << 2;
    }
    frontend_save_string_copy(
        frontend_save_slot[slot].name,
        sizeof(frontend_save_slot[slot].name),
        "PuMp"
    );
    if (
        !frontend_save_apply_slot(slot) ||
        ot_season_mode() != OT_SEASON_NONE
    ) {
        season_failures |= 1u << 3;
    }
    frontend_save_string_copy(
        frontend_save_slot[slot].name,
        sizeof(frontend_save_slot[slot].name),
        "AprPilot"
    );
    if (
        !frontend_save_apply_slot(slot) ||
        ot_season_mode() != OT_SEASON_NONE
    ) {
        season_failures |= 1u << 4;
    }
    if (season_failures != 0) failures |= 1u << 15;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'S';
    sram[3] = 'V';
    sram[4] = 1;
    sram[5] = failures == 0 ? 1 : 0;
    sram[6] = (u8)failures;
    sram[7] = (u8)(failures >> 8);
    sram_write_u32(8, frontend_save_sequence);
    sram[12] = frontend_save_active_bank;
    sram[13] = FRONTEND_SAVE_SLOT_COUNT;
    sram[14] = FRONTEND_SAVE_SCHEMA;
    sram_write_u32(128, season_failures);
    sram[124] = *(volatile const u8 *)save_type_marker;
    __asm__ volatile("swi 3");
}
#endif
