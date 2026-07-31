#ifdef AUTOTEST_SAVE_FLOW
static void autotest_save_flow(void)
{
    volatile u8 *sram = (volatile u8 *)0x0e000000;
    u32 failures = 0;
    u32 sequence = 0;
    u16 corrupt_offset;
    u16 index;
    u8 slot = 3;

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
    sram[124] = *(volatile const u8 *)save_type_marker;
    __asm__ volatile("swi 3");
}
#endif
