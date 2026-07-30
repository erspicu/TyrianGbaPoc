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
    FRONTEND_SAVE_SLOT_ROW_WIDTH =
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT -
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
    FRONTEND_SAVE_SLOT_ROW_HEIGHT = 8,
    FRONTEND_SAVE_SLOT_ROW_BYTES =
        FRONTEND_SAVE_SLOT_ROW_WIDTH * FRONTEND_SAVE_SLOT_ROW_HEIGHT,
    FRONTEND_SAVE_SPECIAL_ROW_WIDTH = 120,
    FRONTEND_SAVE_SPECIAL_ROW_BYTES =
        FRONTEND_SAVE_SPECIAL_ROW_WIDTH *
            FRONTEND_SAVE_SLOT_ROW_HEIGHT,
    FRONTEND_SAVE_SPECIAL_EMPTY_LOAD_ACTIVE = 0,
    FRONTEND_SAVE_SPECIAL_EMPTY_SAVE_ACTIVE = 1,
    FRONTEND_SAVE_SPECIAL_DONE_INACTIVE = 2,
    FRONTEND_SAVE_SPECIAL_ROW_COUNT = 3,
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
static u8 frontend_save_name_cursor EWRAM_BSS;
static u8 frontend_save_name_slot EWRAM_BSS;
static u8 frontend_save_write_active EWRAM_BSS;
static u8 frontend_save_repeat_direction EWRAM_BSS;
static u8 frontend_save_repeat_frames EWRAM_BSS;
static u8 frontend_save_clear_frames EWRAM_BSS;
/*
 * Occupied slot labels are dynamic, but redrawing all eleven mixed-case
 * strings on every page entry is too expensive for ARM7TDMI.  Cache only the
 * 115x8 text rows (9.9 KiB total); the static ROM panel supplies empty
 * rows and the active row remains one cheap runtime overlay.
 */
static u8 frontend_save_slot_row_cache[
    FRONTEND_SAVE_SLOT_COUNT
][FRONTEND_SAVE_SLOT_ROW_BYTES] EWRAM_BSS __attribute__((aligned(4)));
static u16 frontend_save_slot_row_cache_valid EWRAM_BSS;
static u8 frontend_save_special_row_cache[
    FRONTEND_SAVE_SPECIAL_ROW_COUNT
][FRONTEND_SAVE_SPECIAL_ROW_BYTES] EWRAM_BSS __attribute__((aligned(4)));

static void frontend_save_build_slot_row_cache(u8 slot);
static void frontend_save_build_special_row_cache(void);

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
    frontend_save_slot_row_cache_valid = 0;
    frontend_save_slot_mode = FRONTEND_SAVE_MODE_LOAD;
    frontend_save_build_special_row_cache();
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
    for (selected_bank = 0; selected_bank < FRONTEND_SAVE_SLOT_COUNT;
         selected_bank++) {
        if (frontend_save_slot[selected_bank].occupied) {
            frontend_save_build_slot_row_cache(selected_bank);
        }
    }
}

static s16 frontend_save_option_y(u8 selection)
{
    return (s16)(
        TYRIAN_GBA_LAYOUT_OPTIONS_FIRST_Y +
        selection * TYRIAN_GBA_LAYOUT_OPTIONS_ROW_STEP
    );
}

static s16 frontend_save_slot_y(u8 selection)
{
    return (s16)(
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_FIRST_Y +
        selection * TYRIAN_GBA_LAYOUT_SAVE_SLOT_ROW_STEP
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

static void frontend_save_draw_option(u8 selection, u8 active)
{
    static const char *const label[FRONTEND_SAVE_OPTIONS_COUNT] = {
        "Load",
        "Save",
        "Done",
    };
    s16 y = frontend_save_option_y(selection);

    frontend_restore_menu_chrome_rect(120, (u16)y, 120, 10);
    frontend_draw_menu_text_centered(
        frontend_frame_scratch,
        label[selection],
        TYRIAN_GBA_LAYOUT_OPTIONS_CENTER_X,
        y,
        selection == active ? 0xfeu : 0xfau
    );
}

static void frontend_render_options_menu(void)
{
    if (
        !frontend_copy_static_menu_panel(
            FRONTEND_STATIC_OPTIONS_MENU_BASE
        )
    ) {
        frontend_restore_menu_chrome_rect(120, 0, 120, SCREEN_HEIGHT);
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            "Options",
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X,
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y,
            0xfbu
        );
    }
    frontend_save_draw_option(frontend_selection, frontend_selection);
    frontend_save_draw_help(
        (u8)(
            FRONTEND_STATIC_OPTIONS_HELP_STRIP_BASE +
            frontend_selection
        ),
        frontend_selection == 0 ?
            "Load a saved campaign." :
            (
                frontend_selection == 1 ?
                    "Save the current campaign." :
                    "Return to Game Menu."
            )
    );
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_STATIC_MENU_PANEL_X,
        FRONTEND_STATIC_MENU_PANEL_Y,
        FRONTEND_STATIC_MENU_PANEL_WIDTH,
        FRONTEND_STATIC_MENU_PANEL_HEIGHT
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static void frontend_render_options_selection(u8 old_selection)
{
    frontend_save_draw_option(old_selection, frontend_selection);
    frontend_save_draw_option(frontend_selection, frontend_selection);
    frontend_save_draw_help(
        (u8)(
            FRONTEND_STATIC_OPTIONS_HELP_STRIP_BASE +
            frontend_selection
        ),
        frontend_selection == 0 ?
            "Load a saved campaign." :
            (
                frontend_selection == 1 ?
                    "Save the current campaign." :
                    "Return to Game Menu."
            )
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

static void frontend_save_append_u8(char *text, u8 *length, u8 value)
{
    if (value >= 10) text[(*length)++] = (char)('0' + value / 10u);
    text[(*length)++] = (char)('0' + value % 10u);
    text[*length] = '\0';
}

static void frontend_save_draw_slot(u8 selection, u8 active)
{
    char text[32];
    u8 length = 0;
    s16 y = frontend_save_slot_y(selection);
    u8 colour = selection == active ? 0xfeu : 0xfau;

    frontend_restore_menu_chrome_rect(120, (u16)y, 120, 8);
    if (selection == FRONTEND_SAVE_SLOT_COUNT) {
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "Done",
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
            y,
            TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
            colour,
            0xe2u
        );
        return;
    }
    frontend_save_append_u8(text, &length, (u8)(selection + 1u));
    text[length++] = ' ';
    text[length] = '\0';
    if (!frontend_save_slot[selection].occupied) {
        frontend_save_string_copy(
            text + length,
            (u8)(sizeof(text) - length),
            "Empty"
        );
        if (frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD) {
            colour = selection == active ? 0xf8u : 0xf4u;
        }
    } else {
        u8 name_length;

        frontend_save_string_copy(
            text + length,
            (u8)(sizeof(text) - length),
            frontend_save_slot[selection].name
        );
        name_length = (u8)strlen(text);
        if (name_length + 5u < sizeof(text)) {
            text[name_length++] = ' ';
            text[name_length++] = 'E';
            text[name_length++] =
                (char)('1' + frontend_save_slot[selection].episode);
            text[name_length++] = ' ';
            text[name_length++] = 'L';
            text[name_length] = '\0';
            frontend_save_append_u8(
                text,
                &name_length,
                (u8)(
                    frontend_save_slot[selection].main_section > 99 ?
                        99 :
                        frontend_save_slot[selection].main_section
                )
            );
        }
    }
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        text,
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
        y,
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
        colour,
        0xe2u
    );
}

static void frontend_save_build_slot_row_cache(u8 slot)
{
    u8 row;
    s16 y;

    if (
        slot >= FRONTEND_SAVE_SLOT_COUNT ||
        !frontend_save_slot[slot].occupied
    ) {
        return;
    }
    y = frontend_save_slot_y(slot);
    frontend_save_draw_slot(slot, 0xffu);
    for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
        memcpy(
            frontend_save_slot_row_cache[slot] +
                (u16)row * FRONTEND_SAVE_SLOT_ROW_WIDTH,
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH +
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
            FRONTEND_SAVE_SLOT_ROW_WIDTH
        );
    }
    frontend_save_slot_row_cache_valid |= (u16)(1u << slot);
}

static void frontend_save_copy_slot_row_cache(u8 slot)
{
    u8 row;
    s16 y = frontend_save_slot_y(slot);

    if (
        (
            frontend_save_slot_row_cache_valid &
                (u16)(1u << slot)
        ) == 0
    ) {
        frontend_save_build_slot_row_cache(slot);
    }
    for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH +
                TYRIAN_GBA_LAYOUT_SAVE_SLOT_X,
            frontend_save_slot_row_cache[slot] +
                (u16)row * FRONTEND_SAVE_SLOT_ROW_WIDTH,
            FRONTEND_SAVE_SLOT_ROW_WIDTH
        );
    }
}

static void frontend_save_copy_special_row(u8 cache_index, u8 selection)
{
    u8 row;
    s16 y = frontend_save_slot_y(selection);

    for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH +
                FRONTEND_STATIC_MENU_PANEL_X,
            frontend_save_special_row_cache[cache_index] +
                (u16)row * FRONTEND_SAVE_SPECIAL_ROW_WIDTH,
            FRONTEND_SAVE_SPECIAL_ROW_WIDTH
        );
    }
}

static void frontend_save_build_special_row_cache(void)
{
    u8 saved_mode = frontend_save_slot_mode;
    u8 cache_index;
    u8 row;
    const u8 selection[FRONTEND_SAVE_SPECIAL_ROW_COUNT] = {
        0,
        0,
        FRONTEND_SAVE_SLOT_COUNT,
    };
    const u8 active[FRONTEND_SAVE_SPECIAL_ROW_COUNT] = {
        0,
        0,
        0xffu,
    };

    for (
        cache_index = 0;
        cache_index < FRONTEND_SAVE_SPECIAL_ROW_COUNT;
        cache_index++
    ) {
        frontend_save_slot_mode =
            cache_index == FRONTEND_SAVE_SPECIAL_EMPTY_SAVE_ACTIVE ?
                FRONTEND_SAVE_MODE_SAVE :
                FRONTEND_SAVE_MODE_LOAD;
        frontend_save_draw_slot(
            selection[cache_index],
            active[cache_index]
        );
        for (row = 0; row < FRONTEND_SAVE_SLOT_ROW_HEIGHT; row++) {
            memcpy(
                frontend_save_special_row_cache[cache_index] +
                    (u16)row * FRONTEND_SAVE_SPECIAL_ROW_WIDTH,
                frontend_frame_scratch +
                    (u32)(
                        frontend_save_slot_y(selection[cache_index]) +
                        row
                    ) * SCREEN_WIDTH +
                    FRONTEND_STATIC_MENU_PANEL_X,
                FRONTEND_SAVE_SPECIAL_ROW_WIDTH
            );
        }
    }
    frontend_save_slot_mode = saved_mode;
}

static void frontend_save_restore_slot_row(u8 selection)
{
    s16 y = frontend_save_slot_y(selection);

    if (selection < FRONTEND_SAVE_SLOT_COUNT) {
        if (frontend_save_slot[selection].occupied) {
            frontend_save_copy_slot_row_cache(selection);
        } else {
            (void)frontend_copy_static_menu_panel_rows(
                (u8)(
                    FRONTEND_STATIC_SAVE_MENU_BASE +
                    frontend_save_slot_mode
                ),
                (u8)y,
                FRONTEND_SAVE_SLOT_ROW_HEIGHT
            );
        }
        return;
    }
    frontend_save_copy_special_row(
        FRONTEND_SAVE_SPECIAL_DONE_INACTIVE,
        selection
    );
}

static void frontend_render_save_slots(void)
{
    u8 selection;

    if (
        !frontend_copy_static_menu_panel(
            (u8)(
                FRONTEND_STATIC_SAVE_MENU_BASE +
                frontend_save_slot_mode
            )
        )
    ) {
        frontend_restore_menu_chrome_rect(120, 0, 120, SCREEN_HEIGHT);
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD ?
                "Load Game" :
                "Save Game",
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X,
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y,
            0xfbu
        );
        for (
            selection = 0;
            selection < FRONTEND_SAVE_SLOT_COUNT;
            selection++
        ) {
            frontend_save_draw_slot(selection, 0xffu);
        }
    } else {
        for (
            selection = 0;
            selection < FRONTEND_SAVE_SLOT_COUNT;
            selection++
        ) {
            if (
                selection != frontend_selection &&
                frontend_save_slot[selection].occupied
            ) {
                frontend_save_copy_slot_row_cache(selection);
            }
        }
    }
    frontend_save_copy_special_row(
        FRONTEND_SAVE_SPECIAL_DONE_INACTIVE,
        FRONTEND_SAVE_SLOT_COUNT
    );
    if (
        frontend_selection == 0 &&
        !frontend_save_slot[0].occupied
    ) {
        frontend_save_copy_special_row(
            frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD ?
                FRONTEND_SAVE_SPECIAL_EMPTY_LOAD_ACTIVE :
                FRONTEND_SAVE_SPECIAL_EMPTY_SAVE_ACTIVE,
            0
        );
    } else if (
        frontend_selection == FRONTEND_SAVE_SLOT_COUNT
    ) {
        frontend_save_draw_slot(
            frontend_selection,
            frontend_selection
        );
    } else {
        frontend_save_draw_slot(
            frontend_selection,
            frontend_selection
        );
    }
    frontend_save_draw_help(
        (u8)(
            FRONTEND_STATIC_SAVE_HELP_STRIP_BASE +
            frontend_save_slot_mode
        ),
        frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD ?
            "A: Load   B: Back" :
            "A: Name and save   B: Back"
    );
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_STATIC_MENU_PANEL_X,
        FRONTEND_STATIC_MENU_PANEL_Y,
        FRONTEND_STATIC_MENU_PANEL_WIDTH,
        (u16)(
            frontend_save_slot_y(FRONTEND_SAVE_SLOT_COUNT) +
            FRONTEND_SAVE_SLOT_ROW_HEIGHT
        )
    );
    frontend_dirty_add(
        0,
        frontend_menu_help_dirty_y(),
        SCREEN_WIDTH,
        (u16)(SCREEN_HEIGHT - frontend_menu_help_dirty_y())
    );
    frontend_dirty_present();
}

static void frontend_render_save_slot_selection(u8 old_selection)
{
    frontend_save_restore_slot_row(old_selection);
    if (
        frontend_selection == 0 &&
        !frontend_save_slot[0].occupied
    ) {
        frontend_save_copy_special_row(
            frontend_save_slot_mode == FRONTEND_SAVE_MODE_LOAD ?
                FRONTEND_SAVE_SPECIAL_EMPTY_LOAD_ACTIVE :
                FRONTEND_SAVE_SPECIAL_EMPTY_SAVE_ACTIVE,
            0
        );
    } else if (
        frontend_selection == FRONTEND_SAVE_SLOT_COUNT
    ) {
        frontend_save_draw_slot(
            frontend_selection,
            frontend_selection
        );
    } else {
        frontend_save_draw_slot(
            frontend_selection,
            frontend_selection
        );
    }
    frontend_dirty_begin();
    frontend_dirty_add(
        120,
        (u16)frontend_save_slot_y(old_selection),
        120,
        8
    );
    frontend_dirty_add(
        120,
        (u16)frontend_save_slot_y(frontend_selection),
        120,
        8
    );
    frontend_dirty_present();
}

static void frontend_save_draw_name_editor(void)
{
    char display[FRONTEND_SAVE_NAME_BYTES + 3];
    char cursor[FRONTEND_SAVE_NAME_BYTES + 2];
    u8 length = (u8)strlen(frontend_save_name);
    u8 index;

    display[0] = '[';
    memcpy(display + 1, frontend_save_name, length);
    display[length + 1u] = ']';
    display[length + 2u] = '\0';
    for (index = 0; index < sizeof(cursor) - 1u; index++) {
        cursor[index] = ' ';
    }
    cursor[sizeof(cursor) - 1u] = '\0';
    cursor[
        frontend_save_name_cursor + 1u <
                sizeof(cursor) - 1u ?
            frontend_save_name_cursor + 1u :
            sizeof(cursor) - 2u
    ] = '^';
    if (
        !frontend_copy_static_menu_panel(
            FRONTEND_STATIC_SAVE_NAME_MENU_BASE
        )
    ) {
        frontend_restore_menu_chrome_rect(120, 0, 120, 120);
        frontend_draw_menu_text_centered(
            frontend_frame_scratch,
            "Save Name",
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X,
            TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y,
            0xfbu
        );
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "Up/Down: letter",
            126,
            86,
            239,
            0xeau,
            0xe2u
        );
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "R+Up/Down: CAPITAL",
            126,
            94,
            239,
            0xeau,
            0xe2u
        );
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "A/Right: next  B: erase",
            126,
            102,
            239,
            0xeau,
            0xe2u
        );
        frontend_draw_small_mixed_text_clipped_colours(
            frontend_frame_scratch,
            "START: save",
            126,
            110,
            239,
            0xfeu,
            0xe2u
        );
    }
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        display,
        TYRIAN_GBA_LAYOUT_SAVE_NAME_X,
        TYRIAN_GBA_LAYOUT_SAVE_NAME_Y,
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
        0xfeu,
        0xe2u
    );
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        cursor,
        TYRIAN_GBA_LAYOUT_SAVE_NAME_X,
        (s16)(TYRIAN_GBA_LAYOUT_SAVE_NAME_Y + 7),
        TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT,
        0xfbu,
        0xe2u
    );
}

static void frontend_render_save_name(void)
{
    frontend_save_draw_name_editor();
    frontend_save_draw_help(
        (u8)(
            FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_BASE +
            (frontend_save_write_active ? 1u : 0u)
        ),
        frontend_save_write_active ?
            "Saving... Do not turn off power." :
            "Hold SELECT to clear the name."
    );
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_STATIC_MENU_PANEL_X,
        FRONTEND_STATIC_MENU_PANEL_Y,
        FRONTEND_STATIC_MENU_PANEL_WIDTH,
        FRONTEND_STATIC_MENU_PANEL_HEIGHT
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
    frontend_save_slot_row_cache_valid &=
        (u16)~(u16)(1u << slot);
    frontend_save_build_slot_row_cache(slot);
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
    frontend_level_cube_start_count = frontend_data_cube_count;
    frontend_ship_panel_cache_valid = 0;
    frontend_map = (OtEpisodeMap){0};
    memset(frontend_map_level, 0, sizeof(frontend_map_level));
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_level_completed = 0;
    return 1;
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
            frontend_enter_state(
                STATE_OPTIONS_MENU,
                frontend_save_slot_mode
            );
        }
        audio_effect(SFX_SPRING);
        return 1;
    }
    if ((pad_pressed & (KEY_A | KEY_START)) == 0) return 1;
    if (game_state == STATE_OPTIONS_MENU) {
        if (frontend_selection < 2) {
            frontend_save_slot_mode = frontend_selection;
            frontend_enter_state(STATE_SAVE_SLOTS, 0);
            audio_effect(SFX_CLICK);
        } else {
            frontend_start_game_menu_transition_selected(0, 3);
            audio_effect(SFX_CLICK);
        }
        return 1;
    }
    if (frontend_selection == FRONTEND_SAVE_SLOT_COUNT) {
        frontend_enter_state(
            STATE_OPTIONS_MENU,
            frontend_save_slot_mode
        );
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
    FRONTEND_SAVE_BANK_1_OFFSET + FRONTEND_SAVE_BANK_BYTES <= 0x8000,
    "campaign banks must fit standard 32 KiB GBA SRAM"
);
