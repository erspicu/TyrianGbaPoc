/*
 * OpenTyrian-compatible campaign save/load front end for GBA SRAM.
 *
 * The PC game exposes eleven one-player slots and stores progression,
 * equipment, money, cubes and a fourteen-character name.  This port keeps
 * that user-visible model while serializing fields explicitly so compiler
 * padding can never become part of the on-cart format.
 *
 * Two 4 KiB banks live in the upper half of standard 32 KiB GBA SRAM.  A new
 * bank is written while the old bank remains valid; CRC32 is stored before a
 * one-byte commit marker is written last.  Power loss therefore leaves either
 * the old snapshot or the complete new snapshot, never a half-valid save.
 */

enum {
    FRONTEND_SAVE_SLOT_COUNT = 11,
    FRONTEND_SAVE_NAME_LENGTH = 14,
    FRONTEND_SAVE_NAME_BYTES = FRONTEND_SAVE_NAME_LENGTH + 1,
    FRONTEND_SAVE_LEVEL_NAME_BYTES = 11,
    FRONTEND_SAVE_SERIAL_SLOT_BYTES = 64,
    FRONTEND_SAVE_HEADER_BYTES = 20,
    FRONTEND_SAVE_PAYLOAD_BYTES =
        FRONTEND_SAVE_SLOT_COUNT * FRONTEND_SAVE_SERIAL_SLOT_BYTES,
    FRONTEND_SAVE_PAGE_BYTES =
        FRONTEND_SAVE_HEADER_BYTES + FRONTEND_SAVE_PAYLOAD_BYTES,
    FRONTEND_SAVE_BANK_BYTES = 0x1000,
    /* PC LAST LEVEL is an internal slot, not one of the eleven named saves. */
    FRONTEND_SCRIPT_CHECKPOINT_OFFSET = 0x5fc0,
    FRONTEND_SCRIPT_CHECKPOINT_BYTES = 64,
    FRONTEND_SCRIPT_CHECKPOINT_COMMIT = 0xc7,
    FRONTEND_SCRIPT_CHECKPOINT_SCHEMA = 1,
    FRONTEND_SAVE_BANK_0_OFFSET = 0x6000,
    FRONTEND_SAVE_BANK_1_OFFSET = 0x7000,
    FRONTEND_SAVE_COMMIT = 0xa5,
    FRONTEND_SAVE_SCHEMA = 1,
    FRONTEND_SAVE_WRITE_BYTES_PER_VBLANK = 64,
    FRONTEND_SAVE_MODE_LOAD = 0,
    FRONTEND_SAVE_MODE_SAVE = 1,
    FRONTEND_SAVE_OPTIONS_COUNT = 3,
    FRONTEND_SAVE_SLOT_MENU_COUNT = FRONTEND_SAVE_SLOT_COUNT + 1,
    FRONTEND_SAVE_REPEAT_DELAY = 15,
    FRONTEND_SAVE_REPEAT_RATE = 4,
    FRONTEND_SAVE_CLEAR_HOLD_FRAMES = 60,
    FRONTEND_SAVE_SLOT_ROW_HEIGHT = 8,
    FRONTEND_SAVE_PATCH_ROWS_PER_VBLANK = 3,
};

typedef struct {
    u32 cash;
    u16 main_section;
    FrontendPlayerItems items;
    char name[FRONTEND_SAVE_NAME_BYTES];
    char level_name[FRONTEND_SAVE_LEVEL_NAME_BYTES];
    u8 occupied;
    u8 play_mode;
    u8 episode;
    u8 difficulty;
    u8 armor;
    u8 shield;
    u8 shield_max;
    u8 cube_count;
    u8 cube_list[OT_EPISODE_CUBE_CAPACITY];
    u8 secret_hint;
} FrontendSaveSlot;

static FrontendSaveSlot
    frontend_save_slot[FRONTEND_SAVE_SLOT_COUNT] EWRAM_BSS;
static u8 frontend_save_page[FRONTEND_SAVE_PAGE_BYTES]
    EWRAM_BSS __attribute__((aligned(4)));
static char frontend_save_name[FRONTEND_SAVE_NAME_BYTES] EWRAM_BSS;
static u32 frontend_save_sequence EWRAM_BSS;
static u16 frontend_save_write_position EWRAM_BSS;
static u16 frontend_save_target_offset EWRAM_BSS;
static u8 frontend_save_active_bank EWRAM_BSS;
static u8 frontend_save_has_active_bank EWRAM_BSS;
static u8 frontend_save_slot_mode EWRAM_BSS;
static u8 frontend_save_return_state EWRAM_BSS;
static u8 frontend_save_return_selection EWRAM_BSS;
static u8 frontend_save_name_cursor EWRAM_BSS;
static u8 frontend_save_name_slot EWRAM_BSS;
static u8 frontend_save_name_base_ready EWRAM_BSS;
static u8 frontend_save_slots_patch_pending EWRAM_BSS;
static u8 frontend_save_slots_patch_cursor EWRAM_BSS;
static u8 frontend_save_write_active EWRAM_BSS;
static u8 frontend_save_repeat_direction EWRAM_BSS;
static u8 frontend_save_repeat_frames EWRAM_BSS;
static u8 frontend_save_clear_frames EWRAM_BSS;

static volatile u8 *frontend_save_sram(void)
{
    return (volatile u8 *)0x0e000000;
}

static void frontend_save_u16_write(u8 *destination, u16 value)
{
    destination[0] = (u8)value;
    destination[1] = (u8)(value >> 8);
}

static void frontend_save_u32_write(u8 *destination, u32 value)
{
    destination[0] = (u8)value;
    destination[1] = (u8)(value >> 8);
    destination[2] = (u8)(value >> 16);
    destination[3] = (u8)(value >> 24);
}

static u16 frontend_save_u16_read(const u8 *source)
{
    return (u16)(source[0] | ((u16)source[1] << 8));
}

static u32 frontend_save_u32_read(const u8 *source)
{
    return
        (u32)source[0] |
        ((u32)source[1] << 8) |
        ((u32)source[2] << 16) |
        ((u32)source[3] << 24);
}

static u32 frontend_save_crc_byte(u32 crc, u8 value)
{
    u8 bit;

    crc ^= value;
    for (bit = 0; bit < 8; bit++) {
        u32 mask = 0u - (crc & 1u);

        crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
    return crc;
}

static u32 frontend_save_page_crc(const u8 *page)
{
    u32 crc = 0xffffffffu;
    u16 index;

    /* Commit byte and stored CRC are deliberately excluded. */
    for (index = 1; index < 16; index++) {
        crc = frontend_save_crc_byte(crc, page[index]);
    }
    for (
        index = FRONTEND_SAVE_HEADER_BYTES;
        index < FRONTEND_SAVE_PAGE_BYTES;
        index++
    ) {
        crc = frontend_save_crc_byte(crc, page[index]);
    }
    return crc ^ 0xffffffffu;
}

static void frontend_save_string_copy(
    char *destination,
    u8 capacity,
    const char *source
)
{
    u8 index = 0;

    if (capacity == 0) return;
    while (
        source != 0 &&
        source[index] != '\0' &&
        index + 1u < capacity
    ) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
    while (++index < capacity) destination[index] = '\0';
}

static u32 frontend_script_checkpoint_crc(const u8 *page)
{
    u32 crc = 0xffffffffu;
    u8 index;

    /* Commit byte and the stored CRC at bytes 40..43 are excluded. */
    for (index = 1; index < 40; index++) {
        crc = frontend_save_crc_byte(crc, page[index]);
    }
    for (index = 44; index < FRONTEND_SCRIPT_CHECKPOINT_BYTES; index++) {
        crc = frontend_save_crc_byte(crc, page[index]);
    }
    return crc ^ 0xffffffffu;
}

static void frontend_script_checkpoint_clear(void)
{
    frontend_save_sram()[FRONTEND_SCRIPT_CHECKPOINT_OFFSET] = 0;
}

static void frontend_script_checkpoint_write(u16 main_section)
{
    volatile u8 *sram = frontend_save_sram();
    u8 page[FRONTEND_SCRIPT_CHECKPOINT_BYTES];
    u8 index;

    memset(page, 0, sizeof(page));
    page[1] = 'A';
    page[2] = 'T';
    page[3] = 'G';
    page[4] = 'C';
    page[5] = FRONTEND_SCRIPT_CHECKPOINT_SCHEMA;
    page[6] = frontend_play_mode;
    page[7] = frontend_episode;
    page[8] = frontend_difficulty;
    page[9] = player_armor;
    page[10] = player_shield;
    page[11] = player_shield_max;
    page[12] = frontend_data_cube_count;
    memcpy(page + 13, frontend_data_cube_list, OT_EPISODE_CUBE_CAPACITY);
    page[17] = frontend_player_items.ship;
    page[18] = frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id;
    page[19] = frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power;
    page[20] = frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id;
    page[21] = frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power;
    page[22] = frontend_player_items.shield;
    page[23] = frontend_player_items.generator;
    page[24] = frontend_player_items.sidekick[0];
    page[25] = frontend_player_items.sidekick[1];
    page[26] = frontend_player_items.special;
    page[27] = frontend_player_items.sidekick_level;
    page[28] = frontend_player_items.sidekick_series;
    page[29] = frontend_player_items.super_arcade_mode;
    page[30] = frontend_player_items.weapon_mode;
    page[31] = frontend_secret_hint;
    frontend_save_u32_write(page + 32, player_cash);
    frontend_save_u16_write(page + 36, main_section);
    frontend_save_u32_write(page + 40, frontend_script_checkpoint_crc(page));

    /* Power-loss safety: invalidate first and commit only after the payload. */
    sram[FRONTEND_SCRIPT_CHECKPOINT_OFFSET] = 0;
    for (index = 1; index < FRONTEND_SCRIPT_CHECKPOINT_BYTES; index++) {
        sram[FRONTEND_SCRIPT_CHECKPOINT_OFFSET + index] = page[index];
    }
    sram[FRONTEND_SCRIPT_CHECKPOINT_OFFSET] =
        FRONTEND_SCRIPT_CHECKPOINT_COMMIT;
}

static u8 frontend_script_checkpoint_read(
    FrontendScriptCheckpoint *checkpoint
)
{
    volatile u8 *sram = frontend_save_sram();
    u8 page[FRONTEND_SCRIPT_CHECKPOINT_BYTES];
    u8 index;

    if (
        checkpoint == 0 ||
        sram[FRONTEND_SCRIPT_CHECKPOINT_OFFSET] !=
            FRONTEND_SCRIPT_CHECKPOINT_COMMIT
    ) {
        return 0;
    }
    for (index = 0; index < FRONTEND_SCRIPT_CHECKPOINT_BYTES; index++) {
        page[index] = sram[FRONTEND_SCRIPT_CHECKPOINT_OFFSET + index];
    }
    if (
        page[0] != FRONTEND_SCRIPT_CHECKPOINT_COMMIT ||
        page[1] != 'A' ||
        page[2] != 'T' ||
        page[3] != 'G' ||
        page[4] != 'C' ||
        page[5] != FRONTEND_SCRIPT_CHECKPOINT_SCHEMA ||
        page[6] > FRONTEND_PLAY_ARCADE ||
        page[7] >= OT_EPISODE_COUNT ||
        page[8] < FRONTEND_DIFFICULTY_EASY ||
        page[8] > FRONTEND_DIFFICULTY_HARD ||
        page[12] > OT_EPISODE_CUBE_CAPACITY ||
        frontend_save_u16_read(page + 36) == 0 ||
        frontend_save_u32_read(page + 40) !=
            frontend_script_checkpoint_crc(page)
    ) {
        return 0;
    }

    memset(checkpoint, 0, sizeof(*checkpoint));
    checkpoint->play_mode = page[6];
    checkpoint->episode = page[7];
    checkpoint->difficulty = page[8];
    checkpoint->armor = page[9];
    checkpoint->shield = page[10];
    checkpoint->shield_max = page[11];
    checkpoint->cube_count = page[12];
    memcpy(
        checkpoint->cube_list,
        page + 13,
        OT_EPISODE_CUBE_CAPACITY
    );
    checkpoint->items.ship = page[17];
    checkpoint->items.weapon[FRONTEND_WEAPON_FRONT].id = page[18];
    checkpoint->items.weapon[FRONTEND_WEAPON_FRONT].power = page[19];
    checkpoint->items.weapon[FRONTEND_WEAPON_REAR].id = page[20];
    checkpoint->items.weapon[FRONTEND_WEAPON_REAR].power = page[21];
    checkpoint->items.shield = page[22];
    checkpoint->items.generator = page[23];
    checkpoint->items.sidekick[0] = page[24];
    checkpoint->items.sidekick[1] = page[25];
    checkpoint->items.special = page[26];
    checkpoint->items.sidekick_level = page[27];
    checkpoint->items.sidekick_series = page[28];
    checkpoint->items.super_arcade_mode = page[29];
    checkpoint->items.weapon_mode = page[30];
    checkpoint->secret_hint =
        page[31] >= 1 && page[31] <= 3 ? page[31] : 1;
    checkpoint->cash = frontend_save_u32_read(page + 32);
    checkpoint->main_section = frontend_save_u16_read(page + 36);
    return 1;
}

/*
 * Direct campaign-state counterpart of levelsN.dat ]s / ]b.  This is kept
 * separate from the user slot encoder because OpenTyrian's LAST LEVEL slot
 * is an internal rollback point, not a player-selected save destination.
 * A compact atomic SRAM record also avoids permanently charging scarce EWRAM.
 */
static void frontend_script_checkpoint_apply_map(
    const OtEpisodeMap *map
)
{
    u16 checkpoint_section;

    if (map == 0) return;
    checkpoint_section = map->save_section_valid ?
        map->save_section :
        frontend_main_section;
    if (map->backup_requested) {
        frontend_script_checkpoint_write(checkpoint_section);
    }
    if (map->save_section_valid) {
        frontend_main_section = map->save_section;
    }
}

static u8 frontend_script_checkpoint_restore(void)
{
    FrontendScriptCheckpoint checkpoint;

    if (!frontend_script_checkpoint_read(&checkpoint)) return 0;

    frontend_player_items = checkpoint.items;
    frontend_player_items_valid = 1;
    player_cash = checkpoint.cash;
    frontend_main_section = checkpoint.main_section;
    frontend_play_mode = checkpoint.play_mode;
    frontend_episode = checkpoint.episode;
    frontend_difficulty = checkpoint.difficulty;
    player_armor = checkpoint.armor;
    player_shield = checkpoint.shield;
    player_shield_max = checkpoint.shield_max;
    frontend_data_cube_count = checkpoint.cube_count;
    memcpy(
        frontend_data_cube_list,
        checkpoint.cube_list,
        sizeof(frontend_data_cube_list)
    );
    frontend_data_page_count = 0;
    frontend_data_cache_valid = 0;
    frontend_secret_hint = checkpoint.secret_hint;
    frontend_route_state.player_alive = true;
    frontend_route_state.level_timer_expired = false;
    frontend_route_state.player_ship = checkpoint.items.ship;
    frontend_level_cube_start_count = frontend_data_cube_count;
    frontend_ship_panel_cache_valid = 0;
    frontend_map = (OtEpisodeMap){0};
    memset(frontend_map_level, 0, sizeof(frontend_map_level));
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_level_completed = 0;
    frontend_script_game_active = 0;
    return 1;
}

static void frontend_save_slot_encode(u8 *output, const FrontendSaveSlot *slot)
{
    memset(output, 0, FRONTEND_SAVE_SERIAL_SLOT_BYTES);
    output[0] = slot->occupied ? 1 : 0;
    output[1] = slot->play_mode;
    output[2] = slot->episode;
    output[3] = slot->difficulty;
    frontend_save_u16_write(output + 4, slot->main_section);
    output[6] = slot->armor;
    output[7] = slot->shield;
    output[8] = slot->shield_max;
    output[9] = slot->cube_count;
    memcpy(output + 10, slot->cube_list, OT_EPISODE_CUBE_CAPACITY);
    output[14] = slot->items.ship;
    output[15] = slot->items.weapon[FRONTEND_WEAPON_FRONT].id;
    output[16] = slot->items.weapon[FRONTEND_WEAPON_FRONT].power;
    output[17] = slot->items.weapon[FRONTEND_WEAPON_REAR].id;
    output[18] = slot->items.weapon[FRONTEND_WEAPON_REAR].power;
    output[19] = slot->items.shield;
    output[20] = slot->items.generator;
    output[21] = slot->items.sidekick[0];
    output[22] = slot->items.sidekick[1];
    output[23] = slot->items.special;
    output[24] = slot->items.sidekick_level;
    output[25] = slot->items.sidekick_series;
    output[26] = slot->items.super_arcade_mode;
    output[27] = slot->items.weapon_mode;
    frontend_save_u32_write(output + 28, slot->cash);
    memcpy(output + 32, slot->name, FRONTEND_SAVE_NAME_BYTES);
    memcpy(
        output + 47,
        slot->level_name,
        FRONTEND_SAVE_LEVEL_NAME_BYTES
    );
    /* Reserved byte 58 stores PC secretHint; zero is an old-save default. */
    output[58] = slot->secret_hint;
}

static u8 frontend_save_slot_decode(
    FrontendSaveSlot *slot,
    const u8 *input
)
{
    memset(slot, 0, sizeof(*slot));
    if (input[0] == 0) return 1;
    if (
        input[0] != 1 ||
        input[1] > FRONTEND_PLAY_ARCADE ||
        input[2] >= OT_EPISODE_COUNT ||
        input[3] < FRONTEND_DIFFICULTY_EASY ||
        input[3] > FRONTEND_DIFFICULTY_HARD ||
        frontend_save_u16_read(input + 4) == 0 ||
        input[9] > OT_EPISODE_CUBE_CAPACITY
    ) {
        return 0;
    }
    slot->occupied = 1;
    slot->play_mode = input[1];
    slot->episode = input[2];
    slot->difficulty = input[3];
    slot->main_section = frontend_save_u16_read(input + 4);
    slot->armor = input[6];
    slot->shield = input[7];
    slot->shield_max = input[8];
    slot->cube_count = input[9];
    memcpy(slot->cube_list, input + 10, OT_EPISODE_CUBE_CAPACITY);
    slot->items.ship = input[14];
    slot->items.weapon[FRONTEND_WEAPON_FRONT].id = input[15];
    slot->items.weapon[FRONTEND_WEAPON_FRONT].power = input[16];
    slot->items.weapon[FRONTEND_WEAPON_REAR].id = input[17];
    slot->items.weapon[FRONTEND_WEAPON_REAR].power = input[18];
    slot->items.shield = input[19];
    slot->items.generator = input[20];
    slot->items.sidekick[0] = input[21];
    slot->items.sidekick[1] = input[22];
    slot->items.special = input[23];
    slot->items.sidekick_level = input[24];
    slot->items.sidekick_series = input[25];
    slot->items.super_arcade_mode = input[26];
    slot->items.weapon_mode = input[27];
    slot->cash = frontend_save_u32_read(input + 28);
    memcpy(slot->name, input + 32, FRONTEND_SAVE_NAME_BYTES);
    memcpy(
        slot->level_name,
        input + 47,
        FRONTEND_SAVE_LEVEL_NAME_BYTES
    );
    slot->secret_hint =
        input[58] >= 1 && input[58] <= 3 ? input[58] : 1;
    slot->name[FRONTEND_SAVE_NAME_LENGTH] = '\0';
    slot->level_name[FRONTEND_SAVE_LEVEL_NAME_BYTES - 1u] = '\0';
    return slot->name[0] != '\0';
}

static void frontend_save_page_build(void)
{
    u8 slot;

    memset(frontend_save_page, 0, sizeof(frontend_save_page));
    frontend_save_page[0] = FRONTEND_SAVE_COMMIT;
    frontend_save_page[1] = 'A';
    frontend_save_page[2] = 'T';
    frontend_save_page[3] = 'G';
    frontend_save_page[4] = 'S';
    frontend_save_page[5] = FRONTEND_SAVE_SCHEMA;
    frontend_save_page[6] = FRONTEND_SAVE_SLOT_COUNT;
    frontend_save_u32_write(
        frontend_save_page + 8,
        frontend_save_sequence
    );
    frontend_save_u16_write(
        frontend_save_page + 12,
        FRONTEND_SAVE_PAYLOAD_BYTES
    );
    for (slot = 0; slot < FRONTEND_SAVE_SLOT_COUNT; slot++) {
        frontend_save_slot_encode(
            frontend_save_page +
                FRONTEND_SAVE_HEADER_BYTES +
                (u16)slot * FRONTEND_SAVE_SERIAL_SLOT_BYTES,
            &frontend_save_slot[slot]
        );
    }
    frontend_save_u32_write(
        frontend_save_page + 16,
        frontend_save_page_crc(frontend_save_page)
    );
}

static u8 frontend_save_bank_read(u8 bank, u32 *sequence)
{
    volatile u8 *sram = frontend_save_sram();
    u16 offset =
        bank == 0 ?
            FRONTEND_SAVE_BANK_0_OFFSET :
            FRONTEND_SAVE_BANK_1_OFFSET;
    u16 index;

    if (sram[offset] != FRONTEND_SAVE_COMMIT) return 0;
    for (index = 0; index < FRONTEND_SAVE_PAGE_BYTES; index++) {
        frontend_save_page[index] = sram[offset + index];
    }
    if (
        frontend_save_page[0] != FRONTEND_SAVE_COMMIT ||
        frontend_save_page[1] != 'A' ||
        frontend_save_page[2] != 'T' ||
        frontend_save_page[3] != 'G' ||
        frontend_save_page[4] != 'S' ||
        frontend_save_page[5] != FRONTEND_SAVE_SCHEMA ||
        frontend_save_page[6] != FRONTEND_SAVE_SLOT_COUNT ||
        frontend_save_u16_read(frontend_save_page + 12) !=
            FRONTEND_SAVE_PAYLOAD_BYTES ||
        frontend_save_u32_read(frontend_save_page + 16) !=
            frontend_save_page_crc(frontend_save_page)
    ) {
        return 0;
    }
    *sequence = frontend_save_u32_read(frontend_save_page + 8);
    return 1;
}

static u8 frontend_save_page_decode(void)
{
    u8 slot;

    for (slot = 0; slot < FRONTEND_SAVE_SLOT_COUNT; slot++) {
        if (
            !frontend_save_slot_decode(
                &frontend_save_slot[slot],
                frontend_save_page +
                    FRONTEND_SAVE_HEADER_BYTES +
                    (u16)slot * FRONTEND_SAVE_SERIAL_SLOT_BYTES
            )
        ) {
            memset(
                frontend_save_slot,
                0,
                sizeof(frontend_save_slot)
            );
            return 0;
        }
    }
    return 1;
}

static void frontend_save_initialize(void)
{
    u32 sequence_0 = 0;
    u32 sequence_1 = 0;
    u8 valid_0;
    u8 valid_1;
    u8 selected_bank;

    /*
     * Keep a relocation to the save marker so --gc-sections cannot remove
     * the scanner signature from release ROMs.
     */
    __asm__ volatile("" : : "r"(save_type_marker));
    memset(frontend_save_slot, 0, sizeof(frontend_save_slot));
    frontend_save_slot_mode = FRONTEND_SAVE_MODE_LOAD;
    frontend_save_return_state = STATE_OPTIONS_MENU;
    frontend_save_return_selection = FRONTEND_SAVE_MODE_LOAD;
    frontend_save_name_base_ready = 0;
    frontend_save_slots_patch_pending = 0;
    frontend_save_sequence = 0;
    frontend_save_has_active_bank = 0;
    frontend_save_write_active = 0;
    valid_0 = frontend_save_bank_read(0, &sequence_0);
    valid_1 = frontend_save_bank_read(1, &sequence_1);
    if (!valid_0 && !valid_1) return;
    if (
        valid_1 &&
        (
            !valid_0 ||
            (s32)(sequence_1 - sequence_0) > 0
        )
    ) {
        selected_bank = 1;
        frontend_save_sequence = sequence_1;
    } else {
        selected_bank = 0;
        frontend_save_sequence = sequence_0;
    }
    if (
        !frontend_save_bank_read(
            selected_bank,
            &frontend_save_sequence
        ) ||
        !frontend_save_page_decode()
    ) {
        memset(frontend_save_slot, 0, sizeof(frontend_save_slot));
        frontend_save_sequence = 0;
        return;
    }
    frontend_save_active_bank = selected_bank;
    frontend_save_has_active_bank = 1;
}

static s16 frontend_save_option_y(u8 selection)
{
    return (s16)(
        TYRIAN_GBA_LAYOUT_OPTIONS_FIRST_Y +
        selection * TYRIAN_GBA_LAYOUT_OPTIONS_ROW_STEP
    );
}

static u8 frontend_save_title_context(void)
{
    return frontend_save_return_state == STATE_TITLE;
}

static s16 frontend_save_slot_y(u8 selection)
{
    if (frontend_save_title_context()) {
        if (selection == FRONTEND_SAVE_SLOT_COUNT) {
            return TYRIAN_GBA_LAYOUT_SAVE_EXIT_Y;
        }
        return (s16)(
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_FIRST_Y +
            selection * TYRIAN_GBA_LAYOUT_SAVE_SLOT_ROW_STEP
        );
    }
    if (selection == FRONTEND_SAVE_SLOT_COUNT) {
        return TYRIAN_GBA_LAYOUT_GAME_SAVE_EXIT_Y;
    }
    return (s16)(
        TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_FIRST_Y +
        selection * TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_ROW_STEP
    );
}

static void frontend_save_restore_help(void)
{
    u16 y = TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y > 0 ?
        TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y - 1u :
        0;

    frontend_restore_menu_chrome_rect(
        0,
        y,
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - y)
    );
}

static void frontend_save_draw_help(u8 strip_index, const char *text)
{
    if (frontend_copy_static_menu_help(strip_index)) return;
    frontend_save_restore_help();
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

static const char *frontend_save_options_text(
    u8 source_index,
    const char *fallback
)
{
    if (
        frontend_text_ready &&
        source_index < 8 &&
        frontend_text.options_menu[source_index][0] != '\0'
    ) {
        return frontend_text.options_menu[source_index];
    }
    return fallback;
}

static u8 frontend_save_options_source_index(u8 selection)
{
    static const u8 source_index[FRONTEND_SAVE_OPTIONS_COUNT] = {
        1,
        2,
        7,
    };

    return source_index[
        selection < FRONTEND_SAVE_OPTIONS_COUNT ? selection : 0
    ];
}

static u8 frontend_save_options_help_index(u8 selection)
{
    /* helptext.c menuHelp[MENU_OPTIONS] = {13,14,...,12}. */
    static const u8 source_index[FRONTEND_SAVE_OPTIONS_COUNT] = {
        12,
        13,
        11,
    };

    return source_index[
        selection < FRONTEND_SAVE_OPTIONS_COUNT ? selection : 0
    ];
}

static const char *frontend_save_options_help(u8 selection)
{
    static const char *const fallback[FRONTEND_SAVE_OPTIONS_COUNT] = {
        "Load a saved campaign.",
        "Save the current campaign.",
        "Return to Game Menu.",
    };
    u8 safe = selection < FRONTEND_SAVE_OPTIONS_COUNT ? selection : 0;
    u8 source_index = frontend_save_options_help_index(safe);

    if (
        frontend_text_ready &&
        frontend_text.main_menu_help[source_index][0] != '\0'
    ) {
        return frontend_text.main_menu_help[source_index];
    }
    return fallback[safe];
}

static void frontend_save_draw_option(u8 selection, u8 active)
{
    static const char *const fallback[FRONTEND_SAVE_OPTIONS_COUNT] = {
        "Load",
        "Save",
        "Done",
    };
    s16 y = frontend_save_option_y(selection);

    frontend_restore_menu_chrome_rect(120, (u16)y, 120, 10);
    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        frontend_save_options_text(
            frontend_save_options_source_index(selection),
            fallback[selection]
        ),
        TYRIAN_GBA_LAYOUT_OPTIONS_CENTER_X,
        y,
        selection == active ? 0xfeu : 0xfau
    );
}

static void frontend_render_options_menu(void)
{
    u8 selection;

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
        !frontend_copy_static_menu_panel(
            FRONTEND_STATIC_OPTIONS_MENU_BASE
        )
    ) {
        frontend_restore_menu_chrome_rect(120, 0, 120, SCREEN_HEIGHT);
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            frontend_save_options_text(0, "Options"),
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X,
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y,
            0xfbu
        );
        for (selection = 0; selection < FRONTEND_SAVE_OPTIONS_COUNT;
             selection++) {
            frontend_save_draw_option(selection, 0xffu);
        }
    }
    frontend_save_draw_option(frontend_selection, frontend_selection);
    frontend_save_draw_help(
        frontend_save_options_help_index(frontend_selection),
        frontend_save_options_help(frontend_selection)
    );
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_options_selection(u8 old_selection)
{
    frontend_save_draw_option(old_selection, frontend_selection);
    frontend_save_draw_option(frontend_selection, frontend_selection);
    frontend_save_draw_help(
        frontend_save_options_help_index(frontend_selection),
        frontend_save_options_help(frontend_selection)
    );
    frontend_dirty_begin();
    frontend_dirty_add(
        120,
        (u16)frontend_save_option_y(old_selection),
        120,
        10
    );
    frontend_dirty_add(
        120,
        (u16)frontend_save_option_y(frontend_selection),
        120,
        10
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static const u8 *frontend_save_title_slots_frame(void)
{
    return frontend_static_pre_game_frames +
        (u32)FRONTEND_STATIC_PRE_GAME_SAVE_SLOTS_BASE *
            FRONTEND_FRAME_BYTES;
}

static u8 frontend_save_game_slots_panel(void)
{
    return (u8)(
        FRONTEND_STATIC_SAVE_MENU_BASE +
        (
            frontend_save_slot_mode == FRONTEND_SAVE_MODE_SAVE ?
                1u :
                0u
        )
    );
}

static void frontend_save_copy_rows(
    const u8 *source,
    u16 y,
    u16 row_count
)
{
    if (y >= SCREEN_HEIGHT) return;
    if (y + row_count > SCREEN_HEIGHT) {
        row_count = (u16)(SCREEN_HEIGHT - y);
    }
    memcpy(
        frontend_frame_scratch + (u32)y * SCREEN_WIDTH,
        source + (u32)y * SCREEN_WIDTH,
        (u32)row_count * SCREEN_WIDTH
    );
}

static void frontend_save_clear_slot_row(u8 selection)
{
    u16 y = (u16)frontend_save_slot_y(selection);

    if (frontend_save_title_context()) {
        /* mainint.c:JE_loadScreen() owns the complete PIC 2 page. */
        frontend_save_copy_rows(
            frontend_save_title_slots_frame(),
            y,
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        );
        return;
    }

    /*
     * game_menu.c:MENU_LOAD_SAVE only redraws the right-hand slot panel.
     * Restoring a complete 240-pixel row here used to erase the live ship,
     * cash and status illustration on the left once for every save slot,
     * leaving the characteristic horizontal bands seen in the broken UI.
     */
    if (!frontend_copy_static_menu_panel_rows(
            frontend_save_game_slots_panel(),
            (u8)y,
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        )) {
        frontend_restore_menu_chrome_rect(
            FRONTEND_STATIC_MENU_PANEL_X,
            y,
            FRONTEND_STATIC_MENU_PANEL_WIDTH,
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        );
    }
}

static u16 frontend_save_slot_dirty_x(void)
{
    return frontend_save_title_context() ?
        0u :
        FRONTEND_STATIC_MENU_PANEL_X;
}

static u16 frontend_save_slot_dirty_width(void)
{
    return frontend_save_title_context() ?
        SCREEN_WIDTH :
        FRONTEND_STATIC_MENU_PANEL_WIDTH;
}

static const char *frontend_save_misc_text(
    u8 source_index,
    const char *fallback
)
{
    if (
        frontend_text_ready &&
        source_index < 68 &&
        frontend_text.misc_text[source_index][0] != '\0'
    ) {
        return frontend_text.misc_text[source_index];
    }
    return fallback;
}

static const char *frontend_save_misc_text_b(
    u8 source_index,
    const char *fallback
)
{
    if (
        frontend_text_ready &&
        source_index < 5 &&
        frontend_text.misc_text_b[source_index][0] != '\0'
    ) {
        return frontend_text.misc_text_b[source_index];
    }
    return fallback;
}

static void frontend_save_append_text(
    char *text,
    u8 capacity,
    const char *suffix
)
{
    u8 length = (u8)strlen(text);

    while (
        suffix != 0 &&
        *suffix != '\0' &&
        length + 1u < capacity
    ) {
        text[length++] = *suffix++;
    }
    text[length] = '\0';
}

static void frontend_save_draw_title_slot_pass(
    u8 selection,
    u8 active,
    u8 full_shade
)
{
    char last_level_text[40];
    char episode_text[16];
    const char *name;
    u8 name_colour;
    u8 detail_colour;
    s16 y = frontend_save_slot_y(selection);

    if (selection == FRONTEND_SAVE_SLOT_COUNT) {
        /* Keep the full-width source face here: compressing the six-column
         * M glyph to four columns made "Main Menu" look like "Nain Nenu". */
        if (full_shade) {
            frontend_draw_pregame_text_full_shade(
                frontend_frame_scratch,
                frontend_save_misc_text(33, "Exit to Main Menu"),
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
                y,
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
                active ? 0xddu : 0xd9u
            );
        } else {
            frontend_draw_pregame_text_clipped_no_shadow(
                frontend_frame_scratch,
                frontend_save_misc_text(33, "Exit to Main Menu"),
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
                y,
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
                active ? 0xddu : 0xd9u
            );
        }
        return;
    }

    if (frontend_save_slot[selection].occupied) {
        name = frontend_save_slot[selection].name;
        frontend_save_string_copy(
            last_level_text,
            sizeof(last_level_text),
            frontend_save_misc_text_b(2, "Last Level")
        );
        frontend_save_append_text(
            last_level_text,
            sizeof(last_level_text),
            " "
        );
        frontend_save_append_text(
            last_level_text,
            sizeof(last_level_text),
            frontend_save_slot[selection].level_name
        );
        frontend_save_string_copy(
            episode_text,
            sizeof(episode_text),
            frontend_save_misc_text_b(1, "Episode")
        );
        frontend_save_append_text(
            episode_text,
            sizeof(episode_text),
            " "
        );
        {
            u8 length = (u8)strlen(episode_text);

            if (length + 1u < sizeof(episode_text)) {
                episode_text[length++] = (char)(
                    '1' + frontend_save_slot[selection].episode
                );
                episode_text[length] = '\0';
            }
        }
        /* JE_loadScreen(): hue 13/5, brightness 6 when selected, 2 idle. */
        name_colour = active ? 0xddu : 0xd9u;
        detail_colour = active ? 0x5du : 0x59u;
    } else {
        name = frontend_save_misc_text(2, "EMPTY SLOT");
        frontend_save_string_copy(
            last_level_text,
            sizeof(last_level_text),
            frontend_save_misc_text_b(2, "Last Level")
        );
        frontend_save_append_text(
            last_level_text,
            sizeof(last_level_text),
            " -----"
        );
        episode_text[0] = '\0';
        /* Disabled rows use the source brightness 0 instead of 2. */
        name_colour = active ? 0xddu : 0xd7u;
        detail_colour = active ? 0x5du : 0x57u;
    }

    if (full_shade) {
        frontend_draw_small_mixed_text_full_shade(
            frontend_frame_scratch,
            name,
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
            y,
            TYRIAN_GBA_LAYOUT_SAVE_NAME_RIGHT,
            name_colour
        );
        frontend_draw_small_mixed_text_full_shade(
            frontend_frame_scratch,
            last_level_text,
            TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_X,
            y,
            TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_RIGHT,
            detail_colour
        );
        if (episode_text[0] != '\0') {
            frontend_draw_small_mixed_text_full_shade(
                frontend_frame_scratch,
                episode_text,
                TYRIAN_GBA_LAYOUT_SAVE_EPISODE_X,
                y,
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
                detail_colour
            );
        }
    } else {
        frontend_draw_small_mixed_text_clipped_no_shadow(
            frontend_frame_scratch,
            name,
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
            y,
            TYRIAN_GBA_LAYOUT_SAVE_NAME_RIGHT,
            name_colour
        );
        frontend_draw_small_mixed_text_clipped_no_shadow(
            frontend_frame_scratch,
            last_level_text,
            TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_X,
            y,
            TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_RIGHT,
            detail_colour
        );
        if (episode_text[0] != '\0') {
            frontend_draw_small_mixed_text_clipped_no_shadow(
                frontend_frame_scratch,
                episode_text,
                TYRIAN_GBA_LAYOUT_SAVE_EPISODE_X,
                y,
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
                detail_colour
            );
        }
    }
}

static void frontend_save_draw_title_slot(u8 selection, u8 active)
{
    frontend_save_draw_title_slot_pass(selection, active, 1);
}

static void frontend_save_recolour_title_slot(u8 selection, u8 active)
{
    frontend_save_draw_title_slot_pass(selection, active, 0);
}

static void frontend_save_draw_game_slot(u8 selection, u8 active)
{
    const FrontendSaveSlot *slot;
    const char *name;
    const char *level;
    char episode_text[8];
    u8 colour = active ? 0xfeu : 0xfau;
    s16 y = frontend_save_slot_y(selection);

    if (selection == FRONTEND_SAVE_SLOT_COUNT) {
        frontend_draw_menu_text_clipped(
            frontend_frame_scratch,
            frontend_save_misc_text(5, "Exit to Game Menu"),
            TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_X,
            y,
            TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_RIGHT,
            colour
        );
        return;
    }

    slot = &frontend_save_slot[selection];
    name = slot->occupied ?
        slot->name :
        frontend_save_misc_text(2, "EMPTY SLOT");
    level = slot->occupied ? slot->level_name : "-----";
    episode_text[0] = '\0';
    if (slot->occupied) {
        frontend_save_string_copy(
            episode_text,
            sizeof(episode_text),
            frontend_save_misc_text_b(0, "Ep")
        );
        {
            u8 length = (u8)strlen(episode_text);

            if (length + 1u < sizeof(episode_text)) {
                episode_text[length++] = (char)('1' + slot->episode);
                episode_text[length] = '\0';
            }
        }
    }

    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        name,
        TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_X,
        y,
        TYRIAN_GBA_LAYOUT_GAME_SAVE_NAME_RIGHT,
        colour,
        0xe2u
    );
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        level,
        TYRIAN_GBA_LAYOUT_GAME_SAVE_LEVEL_X,
        y,
        TYRIAN_GBA_LAYOUT_GAME_SAVE_LEVEL_RIGHT,
        colour,
        0xe2u
    );
    if (episode_text[0] != '\0') {
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            episode_text,
            TYRIAN_GBA_LAYOUT_GAME_SAVE_EPISODE_X,
            y,
            TYRIAN_GBA_LAYOUT_GAME_SAVE_SLOT_RIGHT,
            colour,
            0xe2u
        );
    }
}

static void frontend_save_draw_slot_pc(u8 selection, u8 active)
{
    if (frontend_save_title_context()) {
        frontend_save_draw_title_slot(selection, active);
    } else {
        frontend_save_draw_game_slot(selection, active);
    }
}

static u8 frontend_save_slots_help_index(u8 selection)
{
    /* game_menu.c:JE_drawMainMenuHelpText(), MENU_LOAD_SAVE (6). */
    return selection == FRONTEND_SAVE_SLOT_COUNT ? 11u : 20u;
}

static const char *frontend_save_slots_help(u8 selection)
{
    u8 source_index = frontend_save_slots_help_index(selection);

    if (
        frontend_text_ready &&
        frontend_text.main_menu_help[source_index][0] != '\0'
    ) {
        return frontend_text.main_menu_help[source_index];
    }
    return selection == FRONTEND_SAVE_SLOT_COUNT ?
        "Return to the Game Menu." :
        "Press up or down to select the save slot you want.";
}

static void frontend_save_restore_slot_row_pc(u8 selection)
{
    frontend_save_clear_slot_row(selection);
    frontend_save_draw_slot_pc(selection, 0);
}

static void frontend_render_save_slots(void)
{
    if (frontend_save_title_context()) {
        /* mainint.c:JE_loadScreen(): full-width PIC 2 page. */
        frontend_present(
            frontend_save_title_slots_frame(),
            frontend_palette_address(FRONTEND_FRAME_SELECT_CHROME)
        );
    } else {
        /* game_menu.c:MENU_LOAD_SAVE: preserve PIC 1, ship, cash and bars;
         * only the right panel changes between Load and Save. */
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
        if (!frontend_copy_static_menu_panel(
                frontend_save_game_slots_panel()
            )) {
            frontend_draw_menu_text_centered(
                frontend_frame_scratch,
                frontend_save_options_text(
                    frontend_save_slot_mode == FRONTEND_SAVE_MODE_SAVE ?
                        2u :
                        1u,
                    frontend_save_slot_mode == FRONTEND_SAVE_MODE_SAVE ?
                        "Save" :
                        "Load"
                ),
                TYRIAN_GBA_LAYOUT_GAME_SAVE_TITLE_CENTER_X,
                TYRIAN_GBA_LAYOUT_GAME_SAVE_TITLE_Y,
                0xfbu
            );
        }
        frontend_save_draw_help(
            frontend_save_slots_help_index(frontend_selection),
            frontend_save_slots_help(frontend_selection)
        );
        frontend_present(
            frontend_frame_scratch,
            frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
        );
    }
    frontend_save_slots_patch_pending = 1;
    frontend_save_slots_patch_cursor = 0;
}

static u8 frontend_save_slots_patch_update(void)
{
    u8 patched = 0;

    if (!frontend_save_slots_patch_pending) return 0;
    if (frontend_frame_pending) return 1;
    frontend_dirty_begin();
    if (frontend_save_slots_patch_cursor == 0) {
        frontend_save_clear_slot_row(frontend_selection);
        frontend_save_draw_slot_pc(frontend_selection, 1);
        frontend_dirty_add(
            frontend_save_slot_dirty_x(),
            (u16)frontend_save_slot_y(frontend_selection),
            frontend_save_slot_dirty_width(),
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        );
        frontend_save_slots_patch_cursor = 1;
        patched++;
    }
    while (
        frontend_save_slots_patch_cursor <=
            FRONTEND_SAVE_SLOT_MENU_COUNT &&
        patched < FRONTEND_SAVE_PATCH_ROWS_PER_VBLANK
    ) {
        u8 selection = (u8)(frontend_save_slots_patch_cursor - 1u);

        frontend_save_slots_patch_cursor++;
        if (selection == frontend_selection) {
            continue;
        }
        frontend_save_clear_slot_row(selection);
        frontend_save_draw_slot_pc(selection, 0);
        frontend_dirty_add(
            frontend_save_slot_dirty_x(),
            (u16)frontend_save_slot_y(selection),
            frontend_save_slot_dirty_width(),
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        );
        patched++;
    }
    if (
        frontend_save_slots_patch_cursor >
            FRONTEND_SAVE_SLOT_MENU_COUNT
    ) {
        frontend_save_slots_patch_pending = 0;
    }
    if (patched != 0) frontend_dirty_present();
    return 1;
}

static void frontend_render_save_slot_selection(u8 old_selection)
{
    u8 title_context = frontend_save_title_context();

    if (title_context) {
        /*
         * Every title row was already composed by the incremental entry job.
         * Selection changes only alter source brightness, so retain the PIC 2
         * background and FULL_SHADE outline and overwrite the glyph pixels.
         */
        frontend_save_recolour_title_slot(old_selection, 0);
        frontend_save_recolour_title_slot(frontend_selection, 1);
    } else {
        frontend_save_restore_slot_row_pc(old_selection);
        frontend_save_clear_slot_row(frontend_selection);
        frontend_save_draw_slot_pc(frontend_selection, 1);
        frontend_save_draw_help(
            frontend_save_slots_help_index(frontend_selection),
            frontend_save_slots_help(frontend_selection)
        );
    }
    frontend_dirty_begin();
    frontend_dirty_add(
        frontend_save_slot_dirty_x(),
        (u16)frontend_save_slot_y(old_selection),
        frontend_save_slot_dirty_width(),
        FRONTEND_SAVE_SLOT_ROW_HEIGHT
    );
    frontend_dirty_add(
        frontend_save_slot_dirty_x(),
        (u16)frontend_save_slot_y(frontend_selection),
        frontend_save_slot_dirty_width(),
        FRONTEND_SAVE_SLOT_ROW_HEIGHT
    );
    if (!title_context) {
        frontend_dirty_add(
            0,
            frontend_menu_help_dirty_y(),
            SCREEN_WIDTH,
            (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
        );
    }
    frontend_dirty_present();
}

static void frontend_save_draw_name_editor_pc(void)
{
    char display[FRONTEND_SAVE_NAME_BYTES + 24];
    char cursor_prefix[FRONTEND_SAVE_NAME_BYTES];
    const char *current_level_name =
        frontend_level.level_name[0] != '\0' ?
            frontend_level.level_name :
            (
                frontend_map_level[0].level_name[0] != '\0' ?
                    frontend_map_level[0].level_name :
                    "Tyrian"
            );
    u8 length = (u8)strlen(frontend_save_name);

    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        current_level_name,
        TYRIAN_GBA_LAYOUT_SAVE_DIALOG_LEVEL_X,
        TYRIAN_GBA_LAYOUT_SAVE_DIALOG_LEVEL_Y,
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_X +
            FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH - 4,
        0xfau,
        0xe2u
    );
    frontend_save_string_copy(
        display,
        sizeof(display),
        frontend_save_misc_text(1, "Save name:")
    );
    frontend_save_append_text(display, sizeof(display), frontend_save_name);
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        display,
        TYRIAN_GBA_LAYOUT_SAVE_DIALOG_NAME_X,
        TYRIAN_GBA_LAYOUT_SAVE_DIALOG_NAME_Y,
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_X +
            FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH - 4,
        0xfeu,
        0xe2u
    );
    if (frontend_save_name_cursor > length) {
        frontend_save_name_cursor = length;
    }
    memcpy(
        cursor_prefix,
        frontend_save_name,
        frontend_save_name_cursor
    );
    cursor_prefix[frontend_save_name_cursor] = '\0';
    frontend_draw_small_mixed_text_clipped_no_shadow(
        frontend_frame_scratch,
        "^",
        (s16)(
            TYRIAN_GBA_LAYOUT_SAVE_DIALOG_NAME_X +
            frontend_small_mixed_text_width(
                frontend_save_misc_text(1, "Save name:")
            ) +
            frontend_small_mixed_text_width(cursor_prefix)
        ),
        (s16)(TYRIAN_GBA_LAYOUT_SAVE_DIALOG_NAME_Y + 8),
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_X +
            FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH - 4,
        0xfbu
    );
}

static void frontend_save_compose_name_overlay(void)
{
    if (!frontend_save_name_base_ready) {
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
    }
    if (!frontend_apply_static_save_name_overlay()) {
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
            frontend_save_misc_text(0, "Last Level Completed"),
            48,
            44,
            192,
            0xfau
        );
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            frontend_save_misc_text(9, "OK"),
            TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X,
            TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y,
            0xfeu
        );
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            frontend_save_misc_text(10, "CANCEL"),
            TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X,
            TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y,
            0xf6u
        );
    }
    frontend_save_draw_name_editor_pc();
    frontend_save_restore_help();
    if (frontend_save_write_active) {
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "Saving... Do not turn off power.",
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X,
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y,
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT,
            0xfeu,
            0xe2u
        );
    } else {
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "D-Pad edit  R: CAPS  START: Save  B: Back",
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X,
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y,
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT,
            0xeau,
            0xe2u
        );
    }
}

static void frontend_render_save_name(void)
{
    u8 first_frame = !frontend_save_name_base_ready;

    frontend_save_compose_name_overlay();
    frontend_save_name_base_ready = 1;
    if (first_frame) {
        frontend_present(
            frontend_frame_scratch,
            frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
        );
        return;
    }
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_X,
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_Y,
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH,
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEIGHT
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static void frontend_save_name_begin(u8 slot)
{
    frontend_save_name_slot = slot;
    if (frontend_save_slot[slot].occupied) {
        frontend_save_string_copy(
            frontend_save_name,
            sizeof(frontend_save_name),
            frontend_save_slot[slot].name
        );
    } else {
        frontend_save_string_copy(
            frontend_save_name,
            sizeof(frontend_save_name),
            "player"
        );
    }
    frontend_save_name_cursor = 0;
    frontend_save_repeat_direction = 0;
    frontend_save_repeat_frames = 0;
    frontend_save_clear_frames = 0;
    frontend_save_name_base_ready = 0;
    frontend_enter_state(STATE_SAVE_NAME, slot);
}

static void frontend_save_capture_slot(u8 slot)
{
    FrontendSaveSlot *destination = &frontend_save_slot[slot];
    const char *level_name =
        frontend_level.level_name[0] != '\0' ?
            frontend_level.level_name :
            (
                frontend_map_level[0].level_name[0] != '\0' ?
                    frontend_map_level[0].level_name :
                    "Tyrian"
            );

    memset(destination, 0, sizeof(*destination));
    destination->occupied = 1;
    destination->play_mode = frontend_play_mode;
    destination->episode = frontend_episode;
    destination->difficulty = frontend_difficulty;
    destination->main_section =
        frontend_main_section == 0 ? 1 : frontend_main_section;
    destination->items = frontend_player_items;
    destination->cash = player_cash;
    destination->armor = player_armor;
    destination->shield = player_shield;
    destination->shield_max = player_shield_max;
    destination->cube_count =
        frontend_data_cube_count > OT_EPISODE_CUBE_CAPACITY ?
            OT_EPISODE_CUBE_CAPACITY :
            frontend_data_cube_count;
    destination->secret_hint = frontend_secret_hint;
    memcpy(
        destination->cube_list,
        frontend_data_cube_list,
        sizeof(destination->cube_list)
    );
    frontend_save_string_copy(
        destination->name,
        sizeof(destination->name),
        frontend_save_name
    );
    frontend_save_string_copy(
        destination->level_name,
        sizeof(destination->level_name),
        level_name
    );
}

static void frontend_save_write_begin(u8 slot)
{
    volatile u8 *sram = frontend_save_sram();
    u8 target_bank =
        frontend_save_has_active_bank ?
            (u8)(frontend_save_active_bank ^ 1u) :
            0;

    frontend_save_capture_slot(slot);
    frontend_save_sequence++;
    frontend_save_page_build();
    frontend_save_target_offset =
        target_bank == 0 ?
            FRONTEND_SAVE_BANK_0_OFFSET :
            FRONTEND_SAVE_BANK_1_OFFSET;
    frontend_save_active_bank = target_bank;
    frontend_save_write_position = 1;
    frontend_save_write_active = 1;
    /* Invalidate the target before touching any header/payload byte. */
    sram[frontend_save_target_offset] = 0;
    frontend_render_save_name();
}

static void frontend_save_write_update(void)
{
    volatile u8 *sram = frontend_save_sram();
    u8 written = 0;

    while (
        frontend_save_write_position < FRONTEND_SAVE_PAGE_BYTES &&
        written < FRONTEND_SAVE_WRITE_BYTES_PER_VBLANK
    ) {
        sram[
            frontend_save_target_offset +
            frontend_save_write_position
        ] = frontend_save_page[frontend_save_write_position];
        frontend_save_write_position++;
        written++;
    }
    if (frontend_save_write_position < FRONTEND_SAVE_PAGE_BYTES) return;
    /* The single commit byte is always the final write. */
    sram[frontend_save_target_offset] = FRONTEND_SAVE_COMMIT;
    frontend_save_write_active = 0;
    frontend_save_has_active_bank = 1;
    frontend_selection = frontend_save_name_slot;
    game_state = STATE_SAVE_SLOTS;
    frontend_render_save_slots();
    audio_effect(SFX_SELECT);
}

static u8 frontend_save_apply_slot(u8 slot)
{
    const FrontendSaveSlot *source;

    if (
        slot >= FRONTEND_SAVE_SLOT_COUNT ||
        !frontend_save_slot[slot].occupied
    ) {
        return 0;
    }
    source = &frontend_save_slot[slot];
    /*
     * The GBA has no real-time clock.  An exact case-insensitive XMAS save
     * name replaces OpenTyrian's host-date selector and chooses tyrianc.shp
     * plus voicesc.snd.  Loading any other slot returns to the ordinary data
     * set; stock Halloween Ramble selection remains script-driven.
     */
    (void)ot_season_set(ot_season_from_save_name(source->name));
    frontend_play_mode = source->play_mode;
    frontend_episode = source->episode;
    frontend_difficulty = source->difficulty;
    frontend_main_section = source->main_section;
    frontend_player_items = source->items;
    frontend_player_items_valid = 1;
    player_cash = source->cash;
    player_armor = source->armor;
    player_shield = source->shield;
    player_shield_max = source->shield_max;
    frontend_data_cube_count = source->cube_count;
    memcpy(
        frontend_data_cube_list,
        source->cube_list,
        sizeof(frontend_data_cube_list)
    );
    frontend_data_page_count = 0;
    frontend_data_cache_valid = 0;
    frontend_secret_hint = source->secret_hint;
    frontend_route_state.player_alive = true;
    frontend_route_state.level_timer_expired = false;
    frontend_route_state.player_ship = source->items.ship;
    frontend_level_cube_start_count = frontend_data_cube_count;
    frontend_ship_panel_cache_valid = 0;
    frontend_map = (OtEpisodeMap){0};
    memset(frontend_map_level, 0, sizeof(frontend_map_level));
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_level_completed = 0;
    frontend_script_checkpoint_clear();
    frontend_script_game_active = 0;
    return 1;
}

static void frontend_save_open_slots(
    u8 mode,
    u8 return_state,
    u8 return_selection
)
{
    frontend_save_slot_mode =
        mode == FRONTEND_SAVE_MODE_SAVE ?
            FRONTEND_SAVE_MODE_SAVE :
            FRONTEND_SAVE_MODE_LOAD;
    frontend_save_return_state =
        return_state == STATE_TITLE ?
            STATE_TITLE :
            STATE_OPTIONS_MENU;
    frontend_save_return_selection = return_selection;
    frontend_enter_state(STATE_SAVE_SLOTS, 0);
}

static void frontend_save_return_to_parent(void)
{
    u8 return_state = frontend_save_return_state;
    u8 return_selection = frontend_save_return_selection;

    if (return_state == STATE_TITLE) {
        frontend_title_idle_frames = 0;
        frontend_enter_state(STATE_TITLE, return_selection);
    } else {
        frontend_enter_state(STATE_OPTIONS_MENU, return_selection);
    }
}

static void frontend_save_move_selection(u8 count, s8 direction)
{
    u8 old_selection = frontend_selection;

    if (direction < 0) {
        frontend_selection =
            frontend_selection == 0 ?
                (u8)(count - 1u) :
                (u8)(frontend_selection - 1u);
    } else {
        frontend_selection =
            frontend_selection + 1u >= count ?
                0 :
                (u8)(frontend_selection + 1u);
    }
    if (game_state == STATE_OPTIONS_MENU) {
        frontend_render_options_selection(old_selection);
    } else {
        frontend_render_save_slot_selection(old_selection);
    }
    source_sound_effect(28);
}

static s8 frontend_save_name_repeat(void)
{
    s8 direction = 0;
    u8 held = 0;

    if (pad_now & KEY_UP) {
        direction = -1;
        held = KEY_UP;
    } else if (pad_now & KEY_DOWN) {
        direction = 1;
        held = KEY_DOWN;
    }
    if (held == 0) {
        frontend_save_repeat_direction = 0;
        frontend_save_repeat_frames = 0;
        return 0;
    }
    if (pad_pressed & held) {
        frontend_save_repeat_direction = held;
        frontend_save_repeat_frames = 0;
        return direction;
    }
    if (frontend_save_repeat_direction != held) {
        frontend_save_repeat_direction = held;
        frontend_save_repeat_frames = 0;
        return 0;
    }
    if (frontend_save_repeat_frames < 255) {
        frontend_save_repeat_frames++;
    }
    if (
        frontend_save_repeat_frames >= FRONTEND_SAVE_REPEAT_DELAY &&
        (
            frontend_save_repeat_frames -
            FRONTEND_SAVE_REPEAT_DELAY
        ) % FRONTEND_SAVE_REPEAT_RATE == 0
    ) {
        return direction;
    }
    return 0;
}

static void frontend_save_name_cycle(s8 direction)
{
    static const char wheel[] =
        " abcdefghijklmnopqrstuvwxyz0123456789-.";
    char current;
    u8 index;
    u8 count = (u8)(sizeof(wheel) - 1u);
    u8 uppercase = (pad_now & KEY_R) != 0;
    u8 length = (u8)strlen(frontend_save_name);

    if (frontend_save_name_cursor >= FRONTEND_SAVE_NAME_LENGTH) return;
    if (frontend_save_name_cursor >= length) {
        frontend_save_name[frontend_save_name_cursor] = 'a';
        frontend_save_name[frontend_save_name_cursor + 1u] = '\0';
        current = 'a';
    } else {
        current = frontend_save_name[frontend_save_name_cursor];
    }
    if (current >= 'A' && current <= 'Z') {
        current = (char)(current - 'A' + 'a');
    }
    for (index = 0; index < count; index++) {
        if (wheel[index] == current) break;
    }
    if (index >= count) index = 1;
    if (direction < 0) {
        index = index == 0 ? (u8)(count - 1u) : (u8)(index - 1u);
    } else {
        index = index + 1u >= count ? 0 : (u8)(index + 1u);
    }
    current = wheel[index];
    if (uppercase && current >= 'a' && current <= 'z') {
        current = (char)(current - 'a' + 'A');
    }
    frontend_save_name[frontend_save_name_cursor] = current;
    frontend_render_save_name();
    source_sound_effect(28);
}

static void frontend_save_name_backspace(void)
{
    u8 length = (u8)strlen(frontend_save_name);

    if (frontend_save_name_cursor > 0) {
        u8 index;

        frontend_save_name_cursor--;
        for (
            index = frontend_save_name_cursor;
            index < length;
            index++
        ) {
            frontend_save_name[index] = frontend_save_name[index + 1u];
        }
        frontend_render_save_name();
        audio_effect(SFX_CLICK);
    } else {
        frontend_enter_state(
            STATE_SAVE_SLOTS,
            frontend_save_name_slot
        );
        audio_effect(SFX_SPRING);
    }
}

static u8 frontend_save_name_update(void)
{
    s8 repeat;
    u8 length;

    if (frontend_save_write_active) {
        frontend_save_write_update();
        return 1;
    }
    if (pad_now & KEY_SELECT) {
        if (frontend_save_clear_frames < 255) {
            frontend_save_clear_frames++;
        }
        if (
            frontend_save_clear_frames ==
            FRONTEND_SAVE_CLEAR_HOLD_FRAMES
        ) {
            frontend_save_name[0] = '\0';
            frontend_save_name_cursor = 0;
            frontend_render_save_name();
            audio_effect(SFX_CLICK);
        }
    } else {
        frontend_save_clear_frames = 0;
    }
    repeat = frontend_save_name_repeat();
    if (repeat != 0) {
        frontend_save_name_cycle(repeat);
        return 1;
    }
    length = (u8)strlen(frontend_save_name);
    if (pad_pressed & (KEY_A | KEY_RIGHT)) {
        if (frontend_save_name_cursor < FRONTEND_SAVE_NAME_LENGTH - 1u) {
            if (frontend_save_name_cursor >= length) {
                frontend_save_name[frontend_save_name_cursor] = 'a';
                frontend_save_name[frontend_save_name_cursor + 1u] = '\0';
            }
            frontend_save_name_cursor++;
            frontend_render_save_name();
            audio_effect(SFX_CLICK);
        }
        return 1;
    }
    if (pad_pressed & KEY_LEFT) {
        if (frontend_save_name_cursor > 0) {
            frontend_save_name_cursor--;
            frontend_render_save_name();
            audio_effect(SFX_CLICK);
        }
        return 1;
    }
    if (pad_pressed & KEY_B) {
        frontend_save_name_backspace();
        return 1;
    }
    if (pad_pressed & KEY_START) {
        if (frontend_save_name[0] == '\0') {
            audio_effect(SFX_CLINK);
        } else {
            frontend_save_write_begin(frontend_save_name_slot);
            audio_effect(SFX_SELECT);
        }
        return 1;
    }
    return 1;
}

static u8 frontend_save_frontend_update(void)
{
    if (
        game_state == STATE_SAVE_SLOTS &&
        frontend_save_slots_patch_update()
    ) {
        return 1;
    }
    if (game_state == STATE_SAVE_NAME) {
        return frontend_save_name_update();
    }
    if (
        game_state != STATE_OPTIONS_MENU &&
        game_state != STATE_SAVE_SLOTS
    ) {
        return 0;
    }
    if (pad_pressed & KEY_UP) {
        frontend_save_move_selection(
            game_state == STATE_OPTIONS_MENU ?
                FRONTEND_SAVE_OPTIONS_COUNT :
                FRONTEND_SAVE_SLOT_MENU_COUNT,
            -1
        );
        return 1;
    }
    if (pad_pressed & KEY_DOWN) {
        frontend_save_move_selection(
            game_state == STATE_OPTIONS_MENU ?
                FRONTEND_SAVE_OPTIONS_COUNT :
                FRONTEND_SAVE_SLOT_MENU_COUNT,
            1
        );
        return 1;
    }
    if (pad_pressed & KEY_B) {
        if (game_state == STATE_OPTIONS_MENU) {
            frontend_start_game_menu_transition_selected(0, 3);
        } else {
            frontend_save_return_to_parent();
        }
        audio_effect(SFX_SPRING);
        return 1;
    }
    if ((pad_pressed & (KEY_A | KEY_START)) == 0) return 1;
    if (game_state == STATE_OPTIONS_MENU) {
        if (frontend_selection < 2) {
            frontend_save_open_slots(
                frontend_selection,
                STATE_OPTIONS_MENU,
                frontend_selection
            );
            audio_effect(SFX_CLICK);
        } else {
            frontend_start_game_menu_transition_selected(0, 3);
            audio_effect(SFX_CLICK);
        }
        return 1;
    }
    if (frontend_selection == FRONTEND_SAVE_SLOT_COUNT) {
        frontend_save_return_to_parent();
        audio_effect(SFX_CLICK);
    } else if (frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD) {
        if (frontend_save_apply_slot(frontend_selection)) {
            audio_effect(SFX_SELECT);
            frontend_start_game_menu_transition(
                FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
            );
        } else {
            audio_effect(SFX_CLINK);
        }
    } else {
        frontend_save_name_begin(frontend_selection);
        audio_effect(SFX_CLICK);
    }
    return 1;
}

_Static_assert(
    FRONTEND_SAVE_PAGE_BYTES < FRONTEND_SAVE_BANK_BYTES,
    "serialized campaign snapshot must fit one SRAM bank"
);
_Static_assert(
    FRONTEND_SCRIPT_CHECKPOINT_OFFSET +
        FRONTEND_SCRIPT_CHECKPOINT_BYTES <=
        FRONTEND_SAVE_BANK_0_OFFSET,
    "internal checkpoint must not overlap named save banks"
);
_Static_assert(
    FRONTEND_SAVE_BANK_1_OFFSET + FRONTEND_SAVE_BANK_BYTES <= 0x8000,
    "campaign banks must fit standard 32 KiB GBA SRAM"
);
