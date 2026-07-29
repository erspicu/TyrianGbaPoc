/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Stock Tyrian data readers over the memory-mapped cartridge ROMFS.
 */
#include "opentyrian_data.h"

#include <string.h>

#include "opentyrian_rom_io.h"

enum {
    OT_HDT_ITEM_COUNT_BYTES = 14,
    OT_HDT_SPECIAL_RECORD_BYTES = 37,
    OT_HDT_POWER_RECORD_BYTES = 37,
    OT_HDT_SHIP_RECORD_BYTES = 41,
    OT_HDT_SHIELD_RECORD_BYTES = 37,
    OT_LEVEL_MAP_SHAPE_LAYER_BYTES = OT_LEVEL_MAP_SHAPE_COUNT * 2,
    OT_LEVEL_MAP_SHAPE_BYTES = 3 * OT_LEVEL_MAP_SHAPE_LAYER_BYTES,
    OT_LEVEL_MAP1_BYTES = OT_LEVEL_MAP1_COLUMNS * OT_LEVEL_MAP1_ROWS,
    OT_LEVEL_MAP2_BYTES = OT_LEVEL_MAP2_COLUMNS * OT_LEVEL_MAP2_ROWS,
    OT_LEVEL_MAP3_BYTES = OT_LEVEL_MAP3_COLUMNS * OT_LEVEL_MAP3_ROWS,
    /*
     * The four stock levelsN.dat files contain at most 51 '*' sections.
     * Keep a little format headroom while retaining a tiny direct-ROM
     * lookup table.
     */
    OT_EPISODE_SCRIPT_SECTION_CAPACITY = 64,
};

typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t source_offset;
    uint32_t weapon_table_offset;
    uint32_t port_table_offset;
    uint32_t special_table_offset;
    uint32_t power_table_offset;
    uint32_t ship_table_offset;
    uint32_t option_table_offset;
    uint32_t shield_table_offset;
    uint32_t enemy_table_offset;
} OtItemDatabase;

typedef struct {
    OtRomFsStat lvl;
    OtRomFsStat hdt;
    OtRomFsStat pic;
    OtRomFsStat palette;
    OtRomFsStat shp;
    OtRomFsStat mus;
    const uint8_t *level_enemy_ids;
    const uint8_t *level_events;
    const uint8_t *level_map_shapes;
    const uint8_t *level_maps[3];
    uint32_t level_map_bytes[3];
    OtLevelInfo level_info;
    OtItemDatabase items;
} OtDataState;

typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t section_offset[OT_EPISODE_SCRIPT_SECTION_CAPACITY];
    uint8_t section_count;
    bool valid;
} OtEpisodeScriptIndex;

static OtDataCatalog catalog;
static OtDataState data_state;
static OtEpisodeScriptIndex episode_script_index;
static bool initialization_attempted;

static const uint8_t encrypted_pascal_key[10] = {
    204, 129, 63, 255, 71, 19, 25, 62, 1, 99
};

static const uint8_t pcx_palette[OT_PIC_COUNT] = {
    0, 7, 5, 8, 10, 5, 18, 19, 19, 20, 21, 22, 5
};

/* OpenTyrian src/lvlmast.c, shapeFile[34]. */
static const char shape_file[34] = {
    '2', '4', '7', '8', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', '5', '#', 'V', '0', '@',
    '3', '^', '5', '9'
};

_Static_assert(sizeof(uint8_t) == 1, "OpenTyrian byte width changed");
_Static_assert(sizeof(int8_t) == 1, "OpenTyrian shortint width changed");
_Static_assert(sizeof(uint16_t) == 2, "OpenTyrian word width changed");
_Static_assert(sizeof(int16_t) == 2, "OpenTyrian integer width changed");
_Static_assert(
    sizeof(pcx_palette) / sizeof(pcx_palette[0]) == OT_PIC_COUNT,
    "PCX palette lookup count changed"
);

static uint16_t read_u16(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static int16_t read_s16(const uint8_t *source)
{
    return (int16_t)read_u16(source);
}

static uint32_t read_u32(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

static int32_t read_s32(const uint8_t *source)
{
    return (int32_t)read_u32(source);
}

static bool span_is_valid(
    uint32_t file_size,
    uint32_t offset,
    uint32_t byte_count
)
{
    return offset <= file_size && byte_count <= file_size - offset;
}

static bool multiply_is_valid(
    uint32_t count,
    uint32_t width,
    uint32_t *bytes
)
{
    if (count != 0 && width > UINT32_MAX / count) return false;
    *bytes = count * width;
    return true;
}

/*
 * JE_loadItemDat() uses the same fixed record layout from two different
 * stock sources: tyrian.hdt for Episodes 1-3, and the final offset in
 * tyrian4.lvl for Episode 4.  Keep offsets relative to the selected item
 * block so every existing HDT reader can follow the current episode without
 * copying or generating an adapter-specific database.
 */
static bool item_database_view(
    const OtRomFsStat *source,
    uint32_t item_offset,
    OtItemDatabase *items
)
{
    static const uint16_t expected_item_maximums[7] = {
        OT_HDT_WEAPON_COUNT - 1,
        OT_HDT_PORT_COUNT - 1,
        6,
        OT_HDT_SHIP_COUNT - 1,
        OT_HDT_OPTION_COUNT - 1,
        OT_HDT_SHIELD_COUNT - 1,
        OT_HDT_ENEMY_COUNT - 1,
    };
    uint32_t expected_bytes =
        OT_HDT_ITEM_COUNT_BYTES +
        OT_HDT_WEAPON_COUNT * OT_HDT_WEAPON_RECORD_BYTES +
        OT_HDT_PORT_COUNT * OT_HDT_PORT_RECORD_BYTES +
        OT_HDT_SPECIAL_COUNT * OT_HDT_SPECIAL_RECORD_BYTES +
        OT_HDT_POWER_COUNT * OT_HDT_POWER_RECORD_BYTES +
        OT_HDT_SHIP_COUNT * OT_HDT_SHIP_RECORD_BYTES +
        OT_HDT_OPTION_COUNT * OT_HDT_OPTION_RECORD_BYTES +
        OT_HDT_SHIELD_COUNT * OT_HDT_SHIELD_RECORD_BYTES +
        OT_HDT_ENEMY_COUNT * OT_HDT_ENEMY_RECORD_BYTES;
    uint8_t index;

    if (
        source == 0 ||
        source->data == 0 ||
        items == 0 ||
        !span_is_valid(source->size, item_offset, expected_bytes) ||
        item_offset + expected_bytes != source->size
    ) {
        return false;
    }
    for (index = 0; index < 7; index++) {
        if (
            read_u16(source->data + item_offset + (uint32_t)index * 2u) !=
            expected_item_maximums[index]
        ) {
            return false;
        }
    }

    *items = (OtItemDatabase){0};
    items->data = source->data + item_offset;
    items->size = expected_bytes;
    items->source_offset = item_offset;
    items->weapon_table_offset = OT_HDT_ITEM_COUNT_BYTES;
    items->port_table_offset =
        items->weapon_table_offset +
        OT_HDT_WEAPON_COUNT * OT_HDT_WEAPON_RECORD_BYTES;
    items->special_table_offset =
        items->port_table_offset +
        OT_HDT_PORT_COUNT * OT_HDT_PORT_RECORD_BYTES;
    items->power_table_offset =
        items->special_table_offset +
        OT_HDT_SPECIAL_COUNT * OT_HDT_SPECIAL_RECORD_BYTES;
    items->ship_table_offset =
        items->power_table_offset +
        OT_HDT_POWER_COUNT * OT_HDT_POWER_RECORD_BYTES;
    items->option_table_offset =
        items->ship_table_offset +
        OT_HDT_SHIP_COUNT * OT_HDT_SHIP_RECORD_BYTES;
    items->shield_table_offset =
        items->option_table_offset +
        OT_HDT_OPTION_COUNT * OT_HDT_OPTION_RECORD_BYTES;
    items->enemy_table_offset =
        items->shield_table_offset +
        OT_HDT_SHIELD_COUNT * OT_HDT_SHIELD_RECORD_BYTES;
    return true;
}

static bool stat_data_file(const char *name, OtRomFsStat *stat)
{
    const OtRomFs *filesystem = ot_rom_io_filesystem();
    char path[OT_ROMFS_MAX_PATH];
    size_t length;

    if (filesystem == 0) {
        if (!ot_rom_io_init()) return false;
        filesystem = ot_rom_io_filesystem();
    }
    if (filesystem == 0 || name == 0 || stat == 0) return false;
    length = strlen(name);
    if (length + 6 > sizeof(path)) return false;
    memcpy(path, "data/", 5);
    memcpy(path + 5, name, length + 1);
    return ot_romfs_stat(filesystem, path, stat) == OT_ROMFS_OK;
}

static bool encrypted_pascal_read(
    const OtRomFsStat *file,
    uint32_t *position,
    char *destination,
    uint32_t destination_size
)
{
    uint8_t length;
    uint32_t index;

    if (
        file == 0 ||
        position == 0 ||
        destination == 0 ||
        destination_size == 0 ||
        !span_is_valid(file->size, *position, 1)
    ) {
        return false;
    }
    length = file->data[(*position)++];
    if (
        length >= destination_size ||
        !span_is_valid(file->size, *position, length)
    ) {
        return false;
    }
    memcpy(destination, file->data + *position, length);
    *position += length;
    for (index = length; index > 0; index--) {
        uint32_t i = index - 1;

        destination[i] = (char)(
            (uint8_t)destination[i] ^
            encrypted_pascal_key[i % 10]
        );
        if (i != 0) {
            destination[i] = (char)(
                (uint8_t)destination[i] ^
                (uint8_t)destination[i - 1]
            );
        }
    }
    destination[length] = '\0';
    return true;
}

/*
 * levelsN.dat is a Pascal-string stream.  A section starts after a record
 * whose decrypted first character is '*'.  The first character needs only
 * one XOR with key[0], so indexing the complete stock script does not need
 * to decrypt or copy any line.  This preserves direct ROMFS source-data
 * semantics while replacing repeated O(lines) seeks with O(1) lookups.
 */
static bool episode_script_index_build(const OtRomFsStat *file)
{
    OtEpisodeScriptIndex built = {0};
    uint32_t cursor = 0;

    if (file == 0 || file->data == 0) return false;
    if (
        episode_script_index.valid &&
        episode_script_index.data == file->data &&
        episode_script_index.size == file->size
    ) {
        return true;
    }
    built.data = file->data;
    built.size = file->size;
    while (cursor < file->size) {
        uint8_t length = file->data[cursor++];
        uint32_t record_end;

        if (!span_is_valid(file->size, cursor, length)) return false;
        record_end = cursor + length;
        if (
            length != 0 &&
            (
                file->data[cursor] ^
                encrypted_pascal_key[0]
            ) == '*'
        ) {
            if (
                built.section_count >=
                    OT_EPISODE_SCRIPT_SECTION_CAPACITY
            ) {
                return false;
            }
            built.section_offset[built.section_count++] =
                record_end;
        }
        cursor = record_end;
    }
    if (built.section_count == 0) return false;
    built.valid = true;
    episode_script_index = built;
    return true;
}

static uint16_t script_number(const char *text)
{
    uint32_t value = 0;

    if (text == 0) return 0;
    while (*text == ' ' || *text == '\t') text++;
    while (*text >= '0' && *text <= '9') {
        value = value * 10u + (uint8_t)(*text - '0');
        if (value > UINT16_MAX) return UINT16_MAX;
        text++;
    }
    return (uint16_t)value;
}

static uint8_t script_item_values(
    const char *text,
    uint8_t destination[OT_EPISODE_ITEM_GROUP_CAPACITY]
)
{
    uint8_t count = 0;

    if (text == 0 || destination == 0) return 0;
    memset(destination, 0, OT_EPISODE_ITEM_GROUP_CAPACITY);
    while (
        *text != '\0' &&
        count < OT_EPISODE_ITEM_GROUP_CAPACITY
    ) {
        uint16_t value;

        while (*text == ' ' || *text == '\t' || *text == ',') {
            text++;
        }
        if (*text < '0' || *text > '9') break;
        value = script_number(text);
        if (value > UINT8_MAX) value = UINT8_MAX;
        destination[count++] = (uint8_t)value;
        while (*text >= '0' && *text <= '9') text++;
    }
    return count;
}

static bool episode_seek_section(
    const OtRomFsStat *file,
    uint16_t section,
    uint32_t *position
)
{
    if (
        section == 0 ||
        position == 0 ||
        !episode_script_index_build(file) ||
        section > episode_script_index.section_count
    ) {
        return false;
    }
    *position = episode_script_index.section_offset[section - 1u];
    return true;
}

static bool offset_table_is_valid(
    const OtRomFsStat *file,
    uint16_t count
)
{
    uint32_t header_bytes = 2u + (uint32_t)count * 4u;
    uint32_t previous = header_bytes;
    uint16_t index;

    if (!span_is_valid(file->size, 0, header_bytes)) return false;
    for (index = 0; index < count; index++) {
        int32_t signed_offset =
            read_s32(file->data + 2u + (uint32_t)index * 4u);
        uint32_t offset;

        if (signed_offset < 0) return false;
        offset = (uint32_t)signed_offset;
        if (offset < previous || offset > file->size) return false;
        previous = offset;
    }
    return true;
}

static uint32_t table_offset(
    const OtRomFsStat *file,
    uint16_t index
)
{
    return read_u32(file->data + 2u + (uint32_t)index * 4u);
}

static bool resolve_item_database(
    uint8_t episode,
    const OtRomFsStat *level_file,
    uint16_t level_offset_count,
    OtItemDatabase *items
)
{
    const OtRomFsStat *source;
    uint32_t item_offset;

    if (episode == 4) {
        if (
            level_file == 0 ||
            level_offset_count == 0 ||
            (level_offset_count & 1u) == 0
        ) {
            return false;
        }
        source = level_file;
        item_offset = table_offset(
            level_file,
            (uint16_t)(level_offset_count - 1u)
        );
    } else if (episode >= 1 && episode <= 3) {
        source = &data_state.hdt;
        if (
            source->data == 0 ||
            !span_is_valid(source->size, 0, 4)
        ) {
            return false;
        }
        item_offset = read_u32(source->data);
    } else {
        return false;
    }
    return item_database_view(source, item_offset, items);
}

static bool select_lvl(uint8_t episode, uint16_t lvl_file_number)
{
    OtRomFsStat lvl;
    OtItemDatabase items;
    OtLevelInfo level_info;
    const uint8_t *level_enemy_ids;
    const uint8_t *level_events;
    const uint8_t *level_map_shapes;
    const uint8_t *level_maps[3];
    uint32_t level_map_bytes[3];
    char filename[] = "tyrian1.lvl";
    const uint8_t *source;
    uint16_t level_count;
    uint32_t offset_index;
    uint32_t offset;
    uint32_t section_end;
    uint32_t position;
    uint32_t bytes;
    uint16_t enemy_count;
    uint16_t event_count;

    if (
        episode == 0 ||
        episode > OT_EPISODE_COUNT ||
        lvl_file_number == 0
    ) {
        return false;
    }
    filename[6] = (char)('0' + episode);
    if (
        !stat_data_file(filename, &lvl) ||
        !span_is_valid(lvl.size, 0, 2)
    ) {
        return false;
    }
    source = lvl.data;
    level_count = read_u16(source);
    offset_index = ((uint32_t)lvl_file_number - 1u) * 2u;
    if (
        offset_index + 1u >= level_count ||
        !offset_table_is_valid(&lvl, level_count)
    ) {
        return false;
    }

    offset = table_offset(&lvl, (uint16_t)offset_index);
    section_end = offset_index + 2u < level_count ?
        table_offset(&lvl, (uint16_t)(offset_index + 2u)) :
        lvl.size;
    if (
        section_end <= offset ||
        !span_is_valid(lvl.size, offset, section_end - offset) ||
        !span_is_valid(section_end, offset, 10)
    ) {
        return false;
    }

    level_info = (OtLevelInfo){0};
    level_info.map_file = (char)source[offset];
    level_info.shape_file = (char)source[offset + 1];
    level_info.map_x = read_u16(source + offset + 2);
    level_info.map_x2 = read_u16(source + offset + 4);
    level_info.map_x3 = read_u16(source + offset + 6);
    enemy_count = read_u16(source + offset + 8);
    position = offset + 10;
    if (
        !multiply_is_valid(enemy_count, 2, &bytes) ||
        !span_is_valid(section_end, position, bytes + 2)
    ) {
        return false;
    }
    level_enemy_ids = source + position;
    position += bytes;

    event_count = read_u16(source + position);
    position += 2;
    if (
        !multiply_is_valid(event_count, OT_LEVEL_EVENT_RECORD_BYTES, &bytes) ||
        !span_is_valid(section_end, position, bytes)
    ) {
        return false;
    }
    level_events = source + position;
    position += bytes;

    if (!span_is_valid(section_end, position, OT_LEVEL_MAP_SHAPE_BYTES)) {
        return false;
    }
    level_map_shapes = source + position;
    position += OT_LEVEL_MAP_SHAPE_BYTES;

    level_map_bytes[0] = OT_LEVEL_MAP1_BYTES;
    level_map_bytes[1] = OT_LEVEL_MAP2_BYTES;
    level_map_bytes[2] = OT_LEVEL_MAP3_BYTES;
    for (uint8_t layer = 0; layer < 3; layer++) {
        if (
            !span_is_valid(
                section_end,
                position,
                level_map_bytes[layer]
            )
        ) {
            return false;
        }
        level_maps[layer] = source + position;
        position += level_map_bytes[layer];
    }
    if (
        position != section_end ||
        !resolve_item_database(episode, &lvl, level_count, &items)
    ) {
        return false;
    }

    level_info.enemy_count = enemy_count;
    level_info.event_count = event_count;
    level_info.section_offset = offset;
    level_info.section_bytes = section_end - offset;
    data_state.lvl = lvl;
    data_state.level_info = level_info;
    data_state.items = items;
    data_state.level_enemy_ids = level_enemy_ids;
    data_state.level_events = level_events;
    data_state.level_map_shapes = level_map_shapes;
    for (uint8_t layer = 0; layer < 3; layer++) {
        data_state.level_maps[layer] = level_maps[layer];
        data_state.level_map_bytes[layer] = level_map_bytes[layer];
    }
    catalog.lvl_count = level_count;
    catalog.selected_episode = episode;
    catalog.selected_lvl_file_number = lvl_file_number;
    catalog.level_enemy_count = enemy_count;
    catalog.level_event_count = event_count;
    catalog.hdt_enemy_table_offset =
        items.source_offset + items.enemy_table_offset;
    return true;
}

static bool parse_lvl(void)
{
    /* OpenTyrian Episode 1 / TYRIAN normal route boot-time sanity level. */
    return select_lvl(1, 9);
}

static bool parse_hdt(void)
{
    int32_t item_offset;
    OtItemDatabase items;

    if (!stat_data_file("tyrian.hdt", &data_state.hdt)) return false;
    if (!span_is_valid(data_state.hdt.size, 0, 4)) return false;
    item_offset = read_s32(data_state.hdt.data);
    if (
        item_offset < 0 ||
        !span_is_valid(
            data_state.hdt.size,
            (uint32_t)item_offset,
            OT_HDT_ITEM_COUNT_BYTES
        )
    ) {
        return false;
    }

    if (!item_database_view(
            &data_state.hdt,
            (uint32_t)item_offset,
            &items
        )) {
        return false;
    }
    data_state.items = items;
    catalog.hdt_enemy_table_offset =
        items.source_offset + items.enemy_table_offset;
    return true;
}

static bool hdt_pascal_read(
    uint32_t *position,
    char *destination,
    uint32_t destination_size
)
{
    static const uint8_t crypt_key[10] = {
        204, 129, 63, 255, 71, 19, 25, 62, 1, 99
    };
    uint8_t length;
    uint32_t index;

    if (
        position == 0 ||
        destination == 0 ||
        destination_size == 0 ||
        !span_is_valid(data_state.hdt.size, *position, 1)
    ) {
        return false;
    }
    length = data_state.hdt.data[(*position)++];
    if (
        length >= destination_size ||
        !span_is_valid(data_state.hdt.size, *position, length)
    ) {
        return false;
    }
    memcpy(destination, data_state.hdt.data + *position, length);
    *position += length;

    /*
     * OpenTyrian helptext.c decrypt_string(), kept in the same reverse
     * dependency order.  The preceding encrypted byte must be consumed
     * before the crypt-key XOR for every character except the first.
     */
    for (index = length; index > 0; index--) {
        uint32_t i = index - 1;

        destination[i] = (char)(
            (uint8_t)destination[i] ^ crypt_key[i % 10]
        );
        if (i != 0) {
            destination[i] = (char)(
                (uint8_t)destination[i] ^
                (uint8_t)destination[i - 1]
            );
        }
    }
    destination[length] = '\0';
    return true;
}

static bool hdt_pascal_skip(uint32_t *position)
{
    uint8_t length;

    if (
        position == 0 ||
        !span_is_valid(data_state.hdt.size, *position, 1)
    ) {
        return false;
    }
    length = data_state.hdt.data[(*position)++];
    if (!span_is_valid(data_state.hdt.size, *position, length)) {
        return false;
    }
    *position += length;
    return true;
}

static bool hdt_pascal_skip_many(uint32_t *position, uint16_t count)
{
    uint16_t index;

    for (index = 0; index < count; index++) {
        if (!hdt_pascal_skip(position)) return false;
    }
    return true;
}

static bool hdt_group_skip(uint32_t *position, uint16_t entry_count)
{
    return hdt_pascal_skip(position) &&
           hdt_pascal_skip_many(position, entry_count) &&
           hdt_pascal_skip(position);
}

bool ot_data_frontend_text_load(OtFrontendText *text)
{
    uint32_t position = 4;
    uint16_t index;

    if (!initialization_attempted) ot_data_init();
    if (!catalog.hdt_valid || text == 0) return false;
    memset(text, 0, sizeof(*text));

    /*
     * This is JE_loadHelpText() in file order.  Groups not needed by this
     * platform adapter are advanced as Pascal records, never re-encoded by
     * the host asset pipeline.
     */
    if (!hdt_group_skip(&position, 39)) return false; /* Online Help */

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 21; index++) {
        if (!hdt_pascal_read(
                &position,
                text->planet_name[index],
                sizeof(text->planet_name[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 68; index++) {
        if (!hdt_pascal_read(
                &position,
                text->misc_text[index],
                sizeof(text->misc_text[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_group_skip(&position, 5)) return false;  /* Misc B */
    if (!hdt_group_skip(&position, 11)) return false; /* Key names */

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 7; index++) {
        if (!hdt_pascal_read(
                &position,
                text->title_menu[index],
                sizeof(text->title_menu[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_group_skip(&position, 9)) return false;  /* Event text */
    if (!hdt_group_skip(&position, 6)) return false;  /* Help topics */
    if (!hdt_group_skip(&position, 34)) return false; /* Main menu help */

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 7; index++) {
        if (!hdt_pascal_read(
                &position,
                text->full_game_menu[index],
                sizeof(text->full_game_menu[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 9; index++) {
        if (!hdt_pascal_read(
                &position,
                text->upgrade_menu[index],
                sizeof(text->upgrade_menu[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;
    if (!hdt_group_skip(&position, 8)) return false; /* Menu 3 */
    if (!hdt_group_skip(&position, 6)) return false; /* In-game menu */
    if (!hdt_group_skip(&position, 6)) return false; /* Detail level */
    if (!hdt_group_skip(&position, 5)) return false; /* Game speed */

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 6; index++) {
        if (!hdt_pascal_read(
                &position,
                text->episode_name[index],
                sizeof(text->episode_name[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 7; index++) {
        if (!hdt_pascal_read(
                &position,
                text->difficulty_name[index],
                sizeof(text->difficulty_name[index])
            )) {
            return false;
        }
    }
    if (!hdt_pascal_skip(&position)) return false;

    if (!hdt_pascal_skip(&position)) return false;
    for (index = 0; index < 5; index++) {
        if (!hdt_pascal_read(
                &position,
                text->gameplay_name[index],
                sizeof(text->gameplay_name[index])
            )) {
            return false;
        }
    }
    return hdt_pascal_skip(&position);
}

static bool pic_stream_is_valid(const OtDataView *view)
{
    uint32_t source_offset = 0;
    uint32_t output_offset = 0;

    while (output_offset < OT_PIC_DECODED_BYTES) {
        uint8_t code;

        if (source_offset >= view->size) return false;
        code = view->data[source_offset++];
        if ((code & 0xc0u) == 0xc0u) {
            uint8_t count = code & 0x3fu;

            if (
                count == 0 ||
                source_offset >= view->size ||
                count > OT_PIC_DECODED_BYTES - output_offset
            ) {
                return false;
            }
            source_offset++;
            output_offset += count;
        } else {
            output_offset++;
        }
    }
    /*
     * Every stock PIC member carries one trailing 0x0c DOS form-feed byte.
     * JE_loadPic() stops after 320x200 output pixels and deliberately leaves
     * that container terminator unread.
     */
    return source_offset + 1 == view->size &&
           view->data[source_offset] == 0x0c;
}

static bool parse_pic(void)
{
    uint16_t count;
    uint16_t index;

    if (
        !stat_data_file("tyrian.pic", &data_state.pic) ||
        !stat_data_file("palette.dat", &data_state.palette) ||
        !span_is_valid(data_state.pic.size, 0, 2)
    ) {
        return false;
    }
    count = read_u16(data_state.pic.data);
    if (
        count != OT_PIC_COUNT ||
        !offset_table_is_valid(&data_state.pic, count) ||
        data_state.palette.size !=
            OT_PALETTE_COUNT * OT_PALETTE_BYTES
    ) {
        return false;
    }
    catalog.pic_count = count;
    for (index = 0; index < count; index++) {
        OtDataView view;
        uint32_t start = table_offset(&data_state.pic, index);
        uint32_t end = index + 1 < count ?
            table_offset(&data_state.pic, index + 1) :
            data_state.pic.size;

        if (end <= start) return false;
        view.data = data_state.pic.data + start;
        view.size = end - start;
        if (!pic_stream_is_valid(&view)) return false;
    }
    return true;
}

static bool shp_table_is_valid(
    uint8_t section,
    uint16_t *sprite_count
)
{
    uint32_t start = table_offset(&data_state.shp, section);
    uint32_t end = section + 1 < OT_SHP_SECTION_COUNT ?
        table_offset(&data_state.shp, section + 1) :
        data_state.shp.size;
    uint32_t position = start;
    uint16_t count;
    uint16_t index;

    if (!span_is_valid(end, position, 2)) return false;
    count = read_u16(data_state.shp.data + position);
    position += 2;
    if (count > OT_SHP_MAX_SPRITES_PER_TABLE) return false;
    for (index = 0; index < count; index++) {
        uint8_t populated;

        if (!span_is_valid(end, position, 1)) return false;
        populated = data_state.shp.data[position++];
        if (populated != 0) {
            uint16_t encoded_bytes;

            if (!span_is_valid(end, position, 6)) return false;
            encoded_bytes = read_u16(data_state.shp.data + position + 4);
            position += 6;
            if (!span_is_valid(end, position, encoded_bytes)) return false;
            position += encoded_bytes;
        }
    }
    if (position != end) return false;
    if (sprite_count != 0) *sprite_count = count;
    return true;
}

static bool parse_shp(void)
{
    uint16_t count;
    uint8_t section;

    if (
        !stat_data_file("tyrian.shp", &data_state.shp) ||
        !span_is_valid(data_state.shp.size, 0, 2)
    ) {
        return false;
    }
    count = read_u16(data_state.shp.data);
    if (
        count != OT_SHP_SECTION_COUNT ||
        !offset_table_is_valid(&data_state.shp, count)
    ) {
        return false;
    }
    for (section = 0; section < OT_SHP_TABLE_SECTION_COUNT; section++) {
        if (!shp_table_is_valid(section, 0)) return false;
    }
    for (section = OT_SHP_TABLE_SECTION_COUNT; section < count; section++) {
        uint32_t start = table_offset(&data_state.shp, section);
        uint32_t end = section + 1 < count ?
            table_offset(&data_state.shp, section + 1) :
            data_state.shp.size;

        if (end <= start) return false;
    }
    catalog.shp_section_count = count;
    return true;
}

static bool parse_mus_song(
    uint16_t song_index,
    OtDataView *view,
    OtMusSongInfo *info
)
{
    uint32_t start;
    uint32_t end;
    uint32_t position;
    uint32_t patch_bytes;
    uint32_t position_bytes;
    uint16_t patch_count;
    uint16_t position_count;
    OtMusSongInfo parsed = {0};

    if (song_index >= catalog.mus_song_count) return false;
    start = table_offset(&data_state.mus, song_index);
    end = song_index + 1 < catalog.mus_song_count ?
        table_offset(&data_state.mus, song_index + 1) :
        data_state.mus.size;
    if (end <= start || !span_is_valid(end, start, 17)) return false;

    parsed.mode = data_state.mus.data[start];
    parsed.speed = read_u16(data_state.mus.data + start + 1);
    parsed.tempo = data_state.mus.data[start + 3];
    parsed.pattern_length = data_state.mus.data[start + 4];
    memcpy(
        parsed.channel_delay,
        data_state.mus.data + start + 5,
        OT_MUS_LDS_CHANNEL_COUNT
    );
    parsed.rhythm_register = data_state.mus.data[start + 14];
    patch_count = read_u16(data_state.mus.data + start + 15);
    position = start + 17;
    if (
        parsed.mode > 2 ||
        !multiply_is_valid(
            patch_count,
            OT_MUS_LDS_PATCH_BYTES,
            &patch_bytes
        ) ||
        !span_is_valid(end, position, patch_bytes + 2)
    ) {
        return false;
    }
    position += patch_bytes;
    position_count = read_u16(data_state.mus.data + position);
    position += 2;
    if (
        !multiply_is_valid(
            position_count,
            OT_MUS_LDS_CHANNEL_COUNT * 3,
            &position_bytes
        ) ||
        !span_is_valid(end, position, position_bytes + 2)
    ) {
        return false;
    }
    position += position_bytes;
    /* Two-byte count of unused digital sounds. */
    position += 2;
    if (((end - position) & 1u) != 0) return false;

    parsed.patch_count = patch_count;
    parsed.position_count = position_count;
    parsed.pattern_word_count = (end - position) / 2;
    if (view != 0) {
        view->data = data_state.mus.data + start;
        view->size = end - start;
    }
    if (info != 0) *info = parsed;
    return true;
}

static bool parse_mus(void)
{
    uint16_t count;
    uint16_t index;

    if (
        !stat_data_file("music.mus", &data_state.mus) ||
        !span_is_valid(data_state.mus.size, 0, 2)
    ) {
        return false;
    }
    count = read_u16(data_state.mus.data);
    if (count == 0 || !offset_table_is_valid(&data_state.mus, count)) {
        return false;
    }
    catalog.mus_song_count = count;
    for (index = 0; index < count; index++) {
        if (!parse_mus_song(index, 0, 0)) return false;
    }
    return true;
}

bool ot_data_init(void)
{
    if (initialization_attempted) return catalog.initialized;
    initialization_attempted = true;
    catalog = (OtDataCatalog){0};
    data_state = (OtDataState){0};
    episode_script_index = (OtEpisodeScriptIndex){0};
    catalog.selected_mus_song = UINT16_MAX;

    catalog.hdt_valid = parse_hdt();
    catalog.lvl_valid = parse_lvl();
    catalog.pic_valid = parse_pic();
    catalog.shp_valid = parse_shp();
    catalog.mus_valid = parse_mus();
    catalog.raw_bytes_referenced =
        data_state.lvl.size +
        data_state.hdt.size +
        data_state.pic.size +
        data_state.palette.size +
        data_state.shp.size +
        data_state.mus.size;
    catalog.initialized =
        catalog.lvl_valid &&
        catalog.hdt_valid &&
        catalog.pic_valid &&
        catalog.shp_valid &&
        catalog.mus_valid;
    return catalog.initialized;
}

const OtDataCatalog *ot_data_catalog(void)
{
    if (!initialization_attempted) ot_data_init();
    return &catalog;
}

bool ot_data_episode_level_resolve(
    uint8_t episode,
    uint16_t main_section,
    uint8_t play_mode,
    uint8_t difficulty,
    OtEpisodeLevel *level
)
{
    OtRomFsStat script;
    OtEpisodeLevel resolved;
    char filename[] = "levels1.dat";
    char line[256];
    uint16_t section = main_section;
    uint8_t jump_count;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.initialized ||
        level == 0 ||
        episode == 0 ||
        episode > OT_EPISODE_COUNT ||
        main_section == 0
    ) {
        return false;
    }
    filename[6] = (char)('0' + episode);
    if (!stat_data_file(filename, &script)) return false;

    resolved = (OtEpisodeLevel){0};
    resolved.episode = episode;
    resolved.requested_section = main_section;
    for (jump_count = 0; jump_count < 64; jump_count++) {
        uint32_t position;
        uint16_t line_count;
        bool jumped = false;

        if (!episode_seek_section(&script, section, &position)) {
            return false;
        }
        for (line_count = 0; line_count < 4096; line_count++) {
            uint32_t length;

            if (!encrypted_pascal_read(
                    &script,
                    &position,
                    line,
                    sizeof(line)
                )) {
                return false;
            }
            if (line[0] != ']') continue;
            length = (uint32_t)strlen(line);
            switch (line[1]) {
            case 'J':
                section = script_number(line + 3);
                jumped = true;
                break;

            case '2':
                if (play_mode != 0) {
                    section = script_number(line + 3);
                    jumped = true;
                }
                break;

            case 'H':
                if (difficulty < 3) {
                    section = script_number(line + 4);
                    jumped = true;
                }
                break;

            case 'h':
                if (
                    difficulty > 2 &&
                    !encrypted_pascal_read(
                        &script,
                        &position,
                        line,
                        sizeof(line)
                    )
                ) {
                    return false;
                }
                break;

            case 'I': {
                uint8_t item_group;

                /*
                 * JE_loadMap() consumes nine encrypted item-availability
                 * records before opening the PC item screen.  The GBA menu
                 * omits that screen but must advance the same script stream.
                 */
                for (item_group = 0; item_group < 9; item_group++) {
                    if (!encrypted_pascal_read(
                            &script,
                            &position,
                            line,
                            sizeof(line)
                        )) {
                        return false;
                    }
                }
                break;
            }

            case 'W':
                /* Skip the source warning-text block through its '#'. */
                do {
                    if (!encrypted_pascal_read(
                            &script,
                            &position,
                            line,
                            sizeof(line)
                        )) {
                        return false;
                    }
                } while (line[0] != '#');
                break;

            case 'G': {
                uint16_t choice_count = script_number(line + 7);
                uint16_t choice_index;

                /*
                 * JE_itemScreen() exposes every ]G destination in Full
                 * Game, while one-player Arcade immediately chooses the
                 * final mapSection.  This compatibility entry point picks
                 * the first Full Game destination; the GBA front-end uses
                 * ot_data_episode_map_resolve() to present all choices.
                 */
                if (
                    choice_count == 0 ||
                    choice_count > OT_EPISODE_MAP_CHOICE_COUNT
                ) {
                    return false;
                }
                choice_index = play_mode != 0 ?
                    (uint16_t)(choice_count - 1u) :
                    0;
                section = script_number(
                    line + 4u + (choice_index + 1u) * 8u
                );
                if (section == 0) return false;
                jumped = true;
                break;
            }

            case 'L': {
                uint16_t one_based_song;
                uint8_t index;

                resolved.resolved_section = section;
                resolved.next_section =
                    length > 9 ? script_number(line + 9) : 0;
                if (resolved.next_section == 0) {
                    resolved.next_section = (uint16_t)(section + 1u);
                }
                memset(resolved.level_name, 0, sizeof(resolved.level_name));
                for (
                    index = 0;
                    index < sizeof(resolved.level_name) - 1u &&
                    13u + index < length;
                    index++
                ) {
                    resolved.level_name[index] = line[13u + index];
                }
                one_based_song =
                    length > 22 ? script_number(line + 22) : 0;
                resolved.source_song =
                    one_based_song > 0 ? one_based_song - 1u : 0;
                resolved.lvl_file_number =
                    length > 25 ? script_number(line + 25) : 0;
                resolved.normal_bonus_level =
                    length > 27 && line[27] == '$';
                resolved.bonus_level =
                    length > 28 && line[28] == '$';
                resolved.episode_complete = false;
                if (resolved.lvl_file_number == 0) return false;
                *level = resolved;
                return true;
            }

            case 'Q':
                resolved.resolved_section = section;
                resolved.next_section = 0;
                resolved.episode_complete = true;
                *level = resolved;
                return true;

            default:
                /*
                 * Presentation, save, item-state and music directives do
                 * not alter which raw LVL section is selected by this GBA
                 * adapter. Their records are still consumed directly.
                 */
                break;
            }
            if (jumped) break;
        }
        if (!jumped || section == 0) return false;
    }
    return false;
}

bool ot_data_episode_map_resolve(
    uint8_t episode,
    uint16_t main_section,
    uint8_t play_mode,
    uint8_t difficulty,
    OtEpisodeMap *map
)
{
    OtRomFsStat script;
    OtEpisodeMap resolved;
    char filename[] = "levels1.dat";
    char line[256];
    uint16_t section = main_section;
    uint8_t jump_count;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.initialized ||
        map == 0 ||
        episode == 0 ||
        episode > OT_EPISODE_COUNT ||
        main_section == 0
    ) {
        return false;
    }
    filename[6] = (char)('0' + episode);
    if (!stat_data_file(filename, &script)) return false;

    resolved = (OtEpisodeMap){0};
    resolved.episode = episode;
    resolved.requested_section = main_section;
    for (jump_count = 0; jump_count < 64; jump_count++) {
        uint32_t position;
        uint16_t line_count;
        bool jumped = false;

        if (!episode_seek_section(&script, section, &position)) {
            return false;
        }
        for (line_count = 0; line_count < 4096; line_count++) {
            if (!encrypted_pascal_read(
                    &script,
                    &position,
                    line,
                    sizeof(line)
                )) {
                return false;
            }
            if (line[0] != ']') continue;
            switch (line[1]) {
            case 'J':
                section = script_number(line + 3);
                jumped = true;
                break;

            case '2':
                if (play_mode != 0) {
                    section = script_number(line + 3);
                    jumped = true;
                }
                break;

            case 'H':
                if (difficulty < 3) {
                    section = script_number(line + 4);
                    jumped = true;
                }
                break;

            case 'h':
                if (
                    difficulty > 2 &&
                    !encrypted_pascal_read(
                        &script,
                        &position,
                        line,
                        sizeof(line)
                    )
                ) {
                    return false;
                }
                break;

            case 'I': {
                uint8_t item_group;

                for (item_group = 0; item_group < 9; item_group++) {
                    if (!encrypted_pascal_read(
                            &script,
                            &position,
                            line,
                            sizeof(line)
                        )) {
                        return false;
                    }
                    resolved.item_count[item_group] =
                        script_item_values(
                            strlen(line) > 8 ? line + 8 : "",
                            resolved.item_avail[item_group]
                        );
                }
                resolved.item_inventory_valid = true;
                break;
            }

            case 'i': {
                uint16_t one_based_song = script_number(line + 3);

                if (one_based_song != 0 && one_based_song <= UINT8_MAX) {
                    resolved.menu_song = (uint8_t)(one_based_song - 1u);
                    resolved.menu_song_valid = true;
                }
                break;
            }

            case 'W':
                do {
                    if (!encrypted_pascal_read(
                            &script,
                            &position,
                            line,
                            sizeof(line)
                        )) {
                        return false;
                    }
                } while (line[0] != '#');
                break;

            case 'G': {
                uint16_t choice_count = script_number(line + 7);
                uint8_t choice;

                if (
                    choice_count == 0 ||
                    choice_count > OT_EPISODE_MAP_CHOICE_COUNT
                ) {
                    return false;
                }
                resolved.resolved_section = section;
                resolved.map_origin = (uint8_t)script_number(line + 4);
                resolved.choice_count = (uint8_t)choice_count;
                for (choice = 0; choice < resolved.choice_count; choice++) {
                    resolved.map_planet[choice] = (uint8_t)script_number(
                        line + 1u + (uint16_t)(choice + 1u) * 8u
                    );
                    resolved.map_section[choice] = script_number(
                        line + 4u + (uint16_t)(choice + 1u) * 8u
                    );
                    if (
                        resolved.map_planet[choice] == 0 ||
                        resolved.map_section[choice] == 0
                    ) {
                        return false;
                    }
                }
                *map = resolved;
                return true;
            }

            case 'L':
                /*
                 * Some script paths jump directly to a playable level
                 * without returning to JE_itemScreen().  Keep the same
                 * section as the sole selectable destination.
                 */
                resolved.resolved_section = section;
                resolved.choice_count = 1;
                resolved.map_section[0] = section;
                resolved.direct_level = true;
                *map = resolved;
                return true;

            case 'Q':
                resolved.resolved_section = section;
                resolved.episode_complete = true;
                *map = resolved;
                return true;

            default:
                break;
            }
            if (jumped) break;
        }
        if (!jumped || section == 0) return false;
    }
    return false;
}

bool ot_data_episode_lvl_count(
    uint8_t episode,
    uint16_t *level_count
)
{
    OtRomFsStat lvl;
    char filename[] = "tyrian1.lvl";
    uint16_t offset_count;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.initialized ||
        episode == 0 ||
        episode > OT_EPISODE_COUNT ||
        level_count == 0
    ) {
        return false;
    }
    filename[6] = (char)('0' + episode);
    if (
        !stat_data_file(filename, &lvl) ||
        !span_is_valid(lvl.size, 0, 2)
    ) {
        return false;
    }
    offset_count = read_u16(lvl.data);
    /*
     * OpenTyrian's LVL directory has two offsets per logical level and one
     * trailing sentinel.  Enforce that contract here so a corrupt table
     * cannot silently turn into a generated/fallback catalog.
     */
    if (
        offset_count < 3 ||
        (offset_count & 1u) == 0 ||
        !offset_table_is_valid(&lvl, offset_count)
    ) {
        return false;
    }
    *level_count = (uint16_t)(offset_count / 2u);
    return *level_count != 0;
}

bool ot_data_level_select(
    uint8_t episode,
    uint16_t lvl_file_number
)
{
    if (!initialization_attempted) ot_data_init();
    if (!catalog.initialized) return false;
    return select_lvl(episode, lvl_file_number);
}

bool ot_data_level_info(OtLevelInfo *info)
{
    if (!initialization_attempted) ot_data_init();
    if (!catalog.lvl_valid || info == 0) return false;
    *info = data_state.level_info;
    return true;
}

bool ot_data_level_event_read(uint16_t index, OtEventRecord *event)
{
    const uint8_t *source;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.lvl_valid ||
        event == 0 ||
        index >= catalog.level_event_count
    ) {
        return false;
    }
    source =
        data_state.level_events +
        (uint32_t)index * OT_LEVEL_EVENT_RECORD_BYTES;
    event->eventtime = read_u16(source);
    event->eventtype = source[2];
    event->eventdat = read_s16(source + 3);
    event->eventdat2 = read_s16(source + 5);
    event->eventdat3 = (int8_t)source[7];
    event->eventdat5 = (int8_t)source[8];
    event->eventdat6 = (int8_t)source[9];
    event->eventdat4 = source[10];
    return true;
}

bool ot_data_level_enemy_pool_read(uint16_t index, uint16_t *enemy_id)
{
    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.lvl_valid ||
        enemy_id == 0 ||
        index >= catalog.level_enemy_count
    ) {
        return false;
    }
    *enemy_id = read_u16(data_state.level_enemy_ids + (uint32_t)index * 2);
    return true;
}

bool ot_data_level_map_shape_view(uint8_t layer, OtDataView *view)
{
    if (!initialization_attempted) ot_data_init();
    if (!catalog.lvl_valid || view == 0 || layer >= 3) return false;
    view->data =
        data_state.level_map_shapes +
        (uint32_t)layer * OT_LEVEL_MAP_SHAPE_LAYER_BYTES;
    view->size = OT_LEVEL_MAP_SHAPE_LAYER_BYTES;
    return true;
}

bool ot_data_level_map_view(uint8_t layer, OtDataView *view)
{
    if (!initialization_attempted) ot_data_init();
    if (!catalog.lvl_valid || view == 0 || layer >= 3) return false;
    view->data = data_state.level_maps[layer];
    view->size = data_state.level_map_bytes[layer];
    return true;
}

bool ot_data_background_shape_file_view(
    char shape_file_id,
    OtDataView *view
)
{
    OtRomFsStat file;
    char filename[] = "shapesx.dat";
    uint32_t position = 0;
    uint16_t shape_index;

    if (!initialization_attempted) ot_data_init();
    if (!catalog.initialized || view == 0) return false;
    if (shape_file_id >= 'A' && shape_file_id <= 'Z') {
        shape_file_id = (char)(shape_file_id + ('a' - 'A'));
    }
    filename[6] = shape_file_id;
    if (!stat_data_file(filename, &file)) return false;

    /*
     * Stock shapes?.dat contains 600 Pascal boolean records followed by
     * 672 raw PC palette indices for each nonblank 24x28 shape. Some files
     * carry unused trailing bytes, so validate the records but retain the
     * complete zero-copy ROM view.
     */
    for (shape_index = 0; shape_index < 600; shape_index++) {
        uint8_t blank;

        if (!span_is_valid(file.size, position, 1)) return false;
        blank = file.data[position++];
        if (blank == 0) {
            if (!span_is_valid(file.size, position, 24u * 28u)) {
                return false;
            }
            position += 24u * 28u;
        }
    }
    view->data = file.data;
    view->size = file.size;
    return true;
}

bool ot_data_hdt_enemy_read(
    uint16_t enemy_id,
    OtEnemyDefinition *enemy
)
{
    const uint8_t *source;
    uint8_t index;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        enemy == 0 ||
        enemy_id >= OT_HDT_ENEMY_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.enemy_table_offset +
        (uint32_t)enemy_id * OT_HDT_ENEMY_RECORD_BYTES;
    enemy->ani = source[0];
    for (index = 0; index < 3; index++) {
        enemy->tur[index] = source[1 + index];
        enemy->freq[index] = source[4 + index];
    }
    enemy->xmove = (int8_t)source[7];
    enemy->ymove = (int8_t)source[8];
    enemy->xaccel = (int8_t)source[9];
    enemy->yaccel = (int8_t)source[10];
    enemy->xcaccel = (int8_t)source[11];
    enemy->ycaccel = (int8_t)source[12];
    enemy->startx = read_s16(source + 13);
    enemy->starty = read_s16(source + 15);
    enemy->startxc = (int8_t)source[17];
    enemy->startyc = (int8_t)source[18];
    enemy->armor = source[19];
    enemy->esize = source[20];
    for (index = 0; index < 20; index++) {
        enemy->egraphic[index] = read_u16(source + 21 + index * 2);
    }
    enemy->explosiontype = source[61];
    enemy->animate = source[62];
    enemy->shapebank = source[63];
    enemy->xrev = (int8_t)source[64];
    enemy->yrev = (int8_t)source[65];
    enemy->dgr = read_u16(source + 66);
    enemy->dlevel = (int8_t)source[68];
    enemy->dani = (int8_t)source[69];
    enemy->elaunchfreq = source[70];
    enemy->elaunchtype = read_u16(source + 71);
    enemy->value = read_s16(source + 73);
    enemy->eenemydie = read_u16(source + 75);
    return true;
}

bool ot_data_hdt_weapon_read(
    uint16_t weapon_id,
    OtWeaponDefinition *weapon
)
{
    const uint8_t *source;
    uint8_t index;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        weapon == 0 ||
        weapon_id >= OT_HDT_WEAPON_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.weapon_table_offset +
        (uint32_t)weapon_id * OT_HDT_WEAPON_RECORD_BYTES;
    weapon->drain = read_u16(source);
    weapon->shotrepeat = source[2];
    weapon->multi = source[3];
    weapon->weapani = read_u16(source + 4);
    weapon->max = source[6];
    weapon->tx = source[7];
    weapon->ty = source[8];
    weapon->aim = source[9];
    for (index = 0; index < 8; index++) {
        weapon->attack[index] = source[10 + index];
        weapon->delay[index] = source[18 + index];
        weapon->sx[index] = (int8_t)source[26 + index];
        weapon->sy[index] = (int8_t)source[34 + index];
        weapon->bx[index] = (int8_t)source[42 + index];
        weapon->by[index] = (int8_t)source[50 + index];
        weapon->sg[index] = read_u16(source + 58 + index * 2);
    }
    weapon->acceleration = (int8_t)source[74];
    weapon->accelerationx = (int8_t)source[75];
    weapon->circlesize = source[76];
    weapon->sound = source[77];
    weapon->trail = source[78];
    weapon->shipblastfilter = source[79];
    return true;
}

static void hdt_item_name_copy(
    const uint8_t *source,
    char destination[31]
)
{
    uint8_t length = source[0] <= 30 ? source[0] : 30;

    memcpy(destination, source + 1, 30);
    destination[length] = '\0';
}

bool ot_data_hdt_weapon_port_read(
    uint8_t port_id,
    OtWeaponPortDefinition *port
)
{
    const uint8_t *source;
    uint8_t mode;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        port == 0 ||
        port_id >= OT_HDT_PORT_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.port_table_offset +
        (uint32_t)port_id * OT_HDT_PORT_RECORD_BYTES;
    hdt_item_name_copy(source, port->name);
    port->opnum = source[31];
    for (mode = 0; mode < 2; mode++) {
        uint8_t power;

        for (power = 0; power < 11; power++) {
            port->op[mode][power] = read_u16(
                source + 32u +
                    (uint32_t)mode * 22u +
                    (uint32_t)power * 2u
            );
        }
    }
    port->cost = read_u16(source + 76);
    port->itemgraphic = read_u16(source + 78);
    port->poweruse = read_u16(source + 80);
    return true;
}

bool ot_data_hdt_special_read(
    uint8_t special_id,
    OtSpecialDefinition *special
)
{
    const uint8_t *source;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        special == 0 ||
        special_id >= OT_HDT_SPECIAL_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.special_table_offset +
        (uint32_t)special_id * OT_HDT_SPECIAL_RECORD_BYTES;
    hdt_item_name_copy(source, special->name);
    special->itemgraphic = read_u16(source + 31);
    special->power = source[33];
    special->type = source[34];
    special->weapon = read_u16(source + 35);
    return true;
}

bool ot_data_hdt_power_read(
    uint8_t power_id,
    OtPowerDefinition *power
)
{
    const uint8_t *source;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        power == 0 ||
        power_id >= OT_HDT_POWER_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.power_table_offset +
        (uint32_t)power_id * OT_HDT_POWER_RECORD_BYTES;
    hdt_item_name_copy(source, power->name);
    power->itemgraphic = read_u16(source + 31);
    power->power = source[33];
    power->speed = (int8_t)source[34];
    power->cost = read_u16(source + 35);
    return true;
}

bool ot_data_hdt_ship_read(
    uint8_t ship_id,
    OtShipDefinition *ship
)
{
    const uint8_t *source;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        ship == 0 ||
        ship_id >= OT_HDT_SHIP_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.ship_table_offset +
        (uint32_t)ship_id * OT_HDT_SHIP_RECORD_BYTES;
    hdt_item_name_copy(source, ship->name);
    ship->shipgraphic = read_u16(source + 31);
    ship->itemgraphic = read_u16(source + 33);
    ship->animation = source[35];
    ship->speed = (int8_t)source[36];
    ship->damage = source[37];
    ship->cost = read_u16(source + 38);
    ship->bigshipgraphic = source[40];
    return true;
}

bool ot_data_hdt_option_read(
    uint8_t option_id,
    OtOptionDefinition *option
)
{
    const uint8_t *source;
    uint8_t index;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        option == 0 ||
        option_id >= OT_HDT_OPTION_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.option_table_offset +
        (uint32_t)option_id * OT_HDT_OPTION_RECORD_BYTES;
    hdt_item_name_copy(source, option->name);
    option->power = source[31];
    option->itemgraphic = read_u16(source + 32);
    option->cost = read_u16(source + 34);
    option->style = source[36];
    option->option = source[37];
    option->speed = (int8_t)source[38];
    option->animation_count = source[39];
    for (index = 0; index < 20; index++) {
        option->graphic[index] = read_u16(
            source + 40u + (uint32_t)index * 2u
        );
    }
    option->weapon_port = source[80];
    option->weapon = read_u16(source + 81);
    option->ammo = source[83];
    option->stop = source[84] != 0;
    option->icon_graphic = source[85];
    return true;
}

bool ot_data_hdt_shield_read(
    uint8_t shield_id,
    OtShieldDefinition *shield
)
{
    const uint8_t *source;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.hdt_valid ||
        data_state.items.data == 0 ||
        shield == 0 ||
        shield_id >= OT_HDT_SHIELD_COUNT
    ) {
        return false;
    }
    source =
        data_state.items.data +
        data_state.items.shield_table_offset +
        (uint32_t)shield_id * OT_HDT_SHIELD_RECORD_BYTES;
    hdt_item_name_copy(source, shield->name);
    shield->recharge_power = source[31];
    shield->max_power = source[32];
    shield->itemgraphic = read_u16(source + 33);
    shield->cost = read_u16(source + 35);
    return true;
}

bool ot_data_pic_view(uint8_t picture_number, OtDataView *view)
{
    uint16_t index;
    uint32_t start;
    uint32_t end;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.pic_valid ||
        view == 0 ||
        picture_number == 0 ||
        picture_number > catalog.pic_count
    ) {
        return false;
    }
    index = (uint16_t)(picture_number - 1);
    start = table_offset(&data_state.pic, index);
    end = index + 1 < catalog.pic_count ?
        table_offset(&data_state.pic, index + 1) :
        data_state.pic.size;
    view->data = data_state.pic.data + start;
    view->size = end - start;
    return true;
}

bool ot_data_pic_palette_view(
    uint8_t picture_number,
    OtDataView *view
)
{
    uint8_t palette_index;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.pic_valid ||
        view == 0 ||
        picture_number == 0 ||
        picture_number > catalog.pic_count
    ) {
        return false;
    }
    palette_index = pcx_palette[picture_number - 1];
    return ot_data_palette_view(palette_index, view);
}

bool ot_data_palette_view(
    uint8_t palette_index,
    OtDataView *view
)
{
    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.pic_valid ||
        view == 0 ||
        palette_index >= OT_PALETTE_COUNT
    ) {
        return false;
    }
    view->data =
        data_state.palette.data + (uint32_t)palette_index * OT_PALETTE_BYTES;
    view->size = OT_PALETTE_BYTES;
    return true;
}

bool ot_data_pic_decode(
    uint8_t picture_number,
    uint8_t *destination,
    uint32_t destination_bytes
)
{
    OtDataView view;
    uint32_t source_offset = 0;
    uint32_t output_offset = 0;

    if (
        destination == 0 ||
        destination_bytes < OT_PIC_DECODED_BYTES ||
        !ot_data_pic_view(picture_number, &view)
    ) {
        return false;
    }
    while (output_offset < OT_PIC_DECODED_BYTES) {
        uint8_t code = view.data[source_offset++];

        if ((code & 0xc0u) == 0xc0u) {
            uint8_t count = code & 0x3fu;
            uint8_t colour = view.data[source_offset++];

            memset(destination + output_offset, colour, count);
            output_offset += count;
        } else {
            destination[output_offset++] = code;
        }
    }
    return source_offset + 1 == view.size &&
           view.data[source_offset] == 0x0c;
}

bool ot_data_shp_section_view(uint8_t section_number, OtDataView *view)
{
    uint16_t index;
    uint32_t start;
    uint32_t end;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.shp_valid ||
        view == 0 ||
        section_number == 0 ||
        section_number > catalog.shp_section_count
    ) {
        return false;
    }
    index = (uint16_t)(section_number - 1);
    start = table_offset(&data_state.shp, index);
    end = index + 1 < catalog.shp_section_count ?
        table_offset(&data_state.shp, index + 1) :
        data_state.shp.size;
    view->data = data_state.shp.data + start;
    view->size = end - start;
    return true;
}

bool ot_data_shp_sprite_read(
    uint8_t table,
    uint16_t sprite_index,
    OtShpSprite *sprite
)
{
    OtDataView section;
    uint32_t position = 2;
    uint16_t count;
    uint16_t index;

    if (
        sprite == 0 ||
        table >= OT_SHP_TABLE_SECTION_COUNT ||
        !ot_data_shp_section_view((uint8_t)(table + 1), &section)
    ) {
        return false;
    }
    count = read_u16(section.data);
    if (sprite_index >= count) return false;
    for (index = 0; index <= sprite_index; index++) {
        bool populated = section.data[position++] != 0;

        if (!populated) {
            if (index == sprite_index) {
                *sprite = (OtShpSprite){0};
                return true;
            }
        } else {
            uint16_t width = read_u16(section.data + position);
            uint16_t height = read_u16(section.data + position + 2);
            uint16_t encoded_bytes =
                read_u16(section.data + position + 4);

            position += 6;
            if (index == sprite_index) {
                sprite->populated = true;
                sprite->width = width;
                sprite->height = height;
                sprite->encoded.data = section.data + position;
                sprite->encoded.size = encoded_bytes;
                return true;
            }
            position += encoded_bytes;
        }
    }
    return false;
}

bool ot_data_comp_shape_bank_view(
    uint8_t shape_table,
    OtDataView *view
)
{
    char name[12] = "newsh0.shp";
    OtRomFsStat stat;
    char character;

    if (!initialization_attempted) ot_data_init();
    if (!catalog.shp_valid || view == 0 || shape_table == 0) {
        return false;
    }

    /*
     * JE_makeEnemy bypasses shapeFile[] for these two logical banks and
     * points at SpriteSheet11 (coins) or SpriteSheet10 (power-ups) from
     * tyrian.shp.  Preserve that source dispatch before consulting newsh.
     */
    if (shape_table == 21) {
        return ot_data_shp_section_view(11, view);
    }
    if (shape_table == 26) {
        return ot_data_shp_section_view(10, view);
    }
    if (shape_table == OT_COMP_SHAPE_TABLE_EXPLOSION) {
        /*
         * OpenTyrian loads explosionSpriteSheet outside shapeFile[].  Expose
         * its unchanged ROMFS stream through the same Sprite2 decoder used
         * by source enemy graphics.
         */
        if (!stat_data_file("newsh6.shp", &stat) || stat.size == 0) {
            return false;
        }
        view->data = stat.data;
        view->size = stat.size;
        return true;
    }
    if (shape_table == OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY) {
        return ot_data_shp_section_view(8, view);
    }
    if (shape_table == OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY) {
        return ot_data_shp_section_view(12, view);
    }
    if (shape_table == OT_COMP_SHAPE_TABLE_OPTIONS_SMALL) {
        return ot_data_shp_section_view(9, view);
    }
    if (shape_table == OT_COMP_SHAPE_TABLE_SHOP) {
        if (!stat_data_file("newsh1.shp", &stat) || stat.size == 0) {
            return false;
        }
        view->data = stat.data;
        view->size = stat.size;
        return true;
    }
    if (shape_table > sizeof(shape_file) / sizeof(shape_file[0])) {
        return false;
    }

    character = shape_file[shape_table - 1];
    if (character >= 'A' && character <= 'Z') {
        character = (char)(character + ('a' - 'A'));
    }
    /*
     * The distributed data set carries shapeFile '@' under its DOS-export
     * alias '~'.  Keep the logical table byte identical to OpenTyrian while
     * resolving the actual cartridge filename.
     */
    if (character == '@') character = '~';
    name[5] = character;
    if (!stat_data_file(name, &stat) || stat.size == 0) return false;
    view->data = stat.data;
    view->size = stat.size;
    return true;
}

bool ot_data_comp_shape_sprite_view(
    uint8_t shape_table,
    uint16_t sprite_number,
    OtDataView *view
)
{
    OtDataView bank;
    uint16_t first_offset;
    uint16_t sprite_count;
    uint16_t start;
    uint32_t end;

    if (
        view == 0 ||
        sprite_number == 0 ||
        !ot_data_comp_shape_bank_view(shape_table, &bank) ||
        bank.size < 2
    ) {
        return false;
    }

    /*
     * Sprite2 has no explicit count: the first u16 offset is also the byte
     * size of its u16 offset table.  OpenTyrian indexes it one-based.
     */
    first_offset = read_u16(bank.data);
    if (
        (first_offset & 1u) != 0 ||
        first_offset < 2 ||
        first_offset > bank.size
    ) {
        return false;
    }
    sprite_count = (uint16_t)(first_offset / 2);
    if (sprite_number > sprite_count) return false;

    start = read_u16(
        bank.data + (uint32_t)(sprite_number - 1) * 2
    );
    end = sprite_number < sprite_count ?
        read_u16(bank.data + (uint32_t)sprite_number * 2) :
        bank.size;
    if (
        start < first_offset ||
        end <= start ||
        end > bank.size
    ) {
        return false;
    }
    view->data = bank.data + start;
    view->size = end - start;
    return true;
}

bool ot_data_mus_song_read(
    uint16_t song_index,
    OtDataView *view,
    OtMusSongInfo *info
)
{
    if (!initialization_attempted) ot_data_init();
    if (!catalog.mus_valid || (view == 0 && info == 0)) return false;
    return parse_mus_song(song_index, view, info);
}

bool ot_data_mus_select(uint16_t song_index)
{
    OtMusSongInfo info;

    if (!ot_data_mus_song_read(song_index, 0, &info)) return false;
    catalog.selected_mus_song = song_index;
    return true;
}
