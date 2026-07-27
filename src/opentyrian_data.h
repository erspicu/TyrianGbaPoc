/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Zero-copy readers for the stock Tyrian MUS/SHP/PIC/HDT/LVL formats stored
 * in the cartridge ROMFS.  Item/enemy reads follow JE_loadItemDat(): HDT for
 * Episodes 1-3 and the item database embedded in tyrian4.lvl for Episode 4.
 *
 * The structures below retain the integer widths and field order used by
 * OpenTyrian revision 1c34d1bddac8c8f2de834229d04b5a729525c944.  They are
 * decoded from little-endian ROM bytes; no generated gameplay tables are
 * required.
 */
#ifndef TYRIAN_GBA_OPENTYRIAN_DATA_H
#define TYRIAN_GBA_OPENTYRIAN_DATA_H

#include <stdbool.h>
#include <stdint.h>

enum {
    OT_EPISODE_COUNT = 4,
    OT_EPISODE_MAP_CHOICE_COUNT = 5,
    OT_LEVEL_EVENT_RECORD_BYTES = 11,
    OT_LEVEL_MAP_SHAPE_COUNT = 128,
    OT_LEVEL_MAP1_COLUMNS = 14,
    OT_LEVEL_MAP1_ROWS = 300,
    OT_LEVEL_MAP2_COLUMNS = 14,
    OT_LEVEL_MAP2_ROWS = 600,
    OT_LEVEL_MAP3_COLUMNS = 15,
    OT_LEVEL_MAP3_ROWS = 600,
    OT_LEVEL_MAP_CELL_WIDTH = 24,
    OT_LEVEL_MAP_CELL_HEIGHT = 28,
    OT_LEVEL_MAP1_FIRST_SOURCE_ROW = 3,
    OT_LEVEL_MAP23_FIRST_SOURCE_ROW = 14,
    OT_LEVEL_INITIAL_BOTTOM_MARGIN_ROWS = 8,
    OT_HDT_WEAPON_COUNT = 781,
    OT_HDT_WEAPON_RECORD_BYTES = 80,
    OT_HDT_PORT_COUNT = 43,
    OT_HDT_PORT_RECORD_BYTES = 82,
    OT_HDT_OPTION_COUNT = 31,
    OT_HDT_OPTION_RECORD_BYTES = 86,
    OT_HDT_ENEMY_COUNT = 851,
    OT_HDT_ENEMY_RECORD_BYTES = 77,
    OT_PIC_COUNT = 13,
    OT_PIC_DECODED_WIDTH = 320,
    OT_PIC_DECODED_HEIGHT = 200,
    OT_PIC_DECODED_BYTES =
        OT_PIC_DECODED_WIDTH * OT_PIC_DECODED_HEIGHT,
    OT_PALETTE_COUNT = 23,
    OT_PALETTE_COLOUR_COUNT = 256,
    OT_PALETTE_BYTES =
        OT_PALETTE_COLOUR_COUNT * 3,
    OT_SHP_SECTION_COUNT = 12,
    OT_SHP_TABLE_SECTION_COUNT = 7,
    OT_SHP_MAX_SPRITES_PER_TABLE = 151,
    /*
     * Adapter-only ID for OpenTyrian's separately loaded
     * explosionSpriteSheet (newsh6.shp).  Authored shape-table values occupy
     * 1..34, so this cannot alias level data.
     */
    OT_COMP_SHAPE_TABLE_EXPLOSION = 35,
    /*
     * Adapter-only IDs for the two compressed player/enemy projectile banks
     * embedded unchanged in tyrian.shp sections 8 and 12.
     */
    OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY = 36,
    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY = 37,
    /*
     * Adapter-only ID for OpenTyrian's spriteSheet9, which contains the
     * ordinary (non-2x2) sidekick bodies.  The stock level shape-table range
     * ends at 34 and the three adapter banks above occupy 35..37.
     */
    OT_COMP_SHAPE_TABLE_OPTIONS_SMALL = 38,
    OT_MUS_LDS_PATCH_BYTES = 46,
    OT_MUS_LDS_CHANNEL_COUNT = 9,
};

typedef struct {
    const uint8_t *data;
    uint32_t size;
} OtDataView;

typedef struct {
    uint16_t eventtime;
    uint8_t eventtype;
    int16_t eventdat;
    int16_t eventdat2;
    int8_t eventdat3;
    int8_t eventdat5;
    int8_t eventdat6;
    uint8_t eventdat4;
} OtEventRecord;

typedef struct {
    uint8_t ani;
    uint8_t tur[3];
    uint8_t freq[3];
    int8_t xmove;
    int8_t ymove;
    int8_t xaccel;
    int8_t yaccel;
    int8_t xcaccel;
    int8_t ycaccel;
    int16_t startx;
    int16_t starty;
    int8_t startxc;
    int8_t startyc;
    uint8_t armor;
    uint8_t esize;
    uint16_t egraphic[20];
    uint8_t explosiontype;
    uint8_t animate;
    uint8_t shapebank;
    int8_t xrev;
    int8_t yrev;
    uint16_t dgr;
    int8_t dlevel;
    int8_t dani;
    uint8_t elaunchfreq;
    uint16_t elaunchtype;
    int16_t value;
    uint16_t eenemydie;
} OtEnemyDefinition;

typedef struct {
    uint16_t drain;
    uint8_t shotrepeat;
    uint8_t multi;
    uint16_t weapani;
    uint8_t max;
    uint8_t tx;
    uint8_t ty;
    uint8_t aim;
    uint8_t attack[8];
    uint8_t delay[8];
    int8_t sx[8];
    int8_t sy[8];
    int8_t bx[8];
    int8_t by[8];
    uint16_t sg[8];
    int8_t acceleration;
    int8_t accelerationx;
    uint8_t circlesize;
    uint8_t sound;
    uint8_t trail;
    uint8_t shipblastfilter;
} OtWeaponDefinition;

typedef struct {
    char name[31];
    uint8_t opnum;
    uint16_t op[2][11];
    uint16_t cost;
    uint16_t itemgraphic;
    uint16_t poweruse;
} OtWeaponPortDefinition;

typedef struct {
    char name[31];
    uint16_t itemgraphic;
    uint8_t power;
    uint8_t type;
    uint16_t weapon;
} OtSpecialDefinition;

typedef struct {
    char name[31];
    uint8_t power;
    uint16_t itemgraphic;
    uint16_t cost;
    uint8_t style;
    uint8_t option;
    int8_t speed;
    uint8_t animation_count;
    uint16_t graphic[20];
    uint8_t weapon_port;
    uint16_t weapon;
    uint8_t ammo;
    bool stop;
    uint8_t icon_graphic;
} OtOptionDefinition;

typedef struct {
    char map_file;
    char shape_file;
    uint16_t map_x;
    uint16_t map_x2;
    uint16_t map_x3;
    uint16_t enemy_count;
    uint16_t event_count;
    uint32_t section_offset;
    uint32_t section_bytes;
} OtLevelInfo;

/*
 * Original ]G route information consumed by JE_itemScreen().  Full Game
 * exposes every destination; one-player Arcade selects the final entry.
 * Planet and section values remain the one-based values stored in
 * levelsN.dat.
 */
typedef struct {
    uint8_t episode;
    uint16_t requested_section;
    uint16_t resolved_section;
    uint8_t map_origin;
    uint8_t choice_count;
    uint8_t map_planet[OT_EPISODE_MAP_CHOICE_COUNT];
    uint16_t map_section[OT_EPISODE_MAP_CHOICE_COUNT];
    bool direct_level;
    bool episode_complete;
} OtEpisodeMap;

/*
 * Result of interpreting one original levelsN.dat section. Script sections
 * are one-based and source_song is zero-based for play_song().
 */
typedef struct {
    uint8_t episode;
    uint16_t requested_section;
    uint16_t resolved_section;
    uint16_t next_section;
    uint16_t lvl_file_number;
    uint16_t source_song;
    bool bonus_level;
    bool normal_bonus_level;
    bool episode_complete;
    char level_name[10];
} OtEpisodeLevel;

typedef struct {
    bool populated;
    uint16_t width;
    uint16_t height;
    OtDataView encoded;
} OtShpSprite;

typedef struct {
    uint8_t mode;
    uint16_t speed;
    uint8_t tempo;
    uint8_t pattern_length;
    uint8_t channel_delay[OT_MUS_LDS_CHANNEL_COUNT];
    uint8_t rhythm_register;
    uint16_t patch_count;
    uint16_t position_count;
    uint32_t pattern_word_count;
} OtMusSongInfo;

typedef struct {
    bool initialized;
    bool lvl_valid;
    bool hdt_valid;
    bool pic_valid;
    bool shp_valid;
    bool mus_valid;
    uint16_t lvl_count;
    uint8_t selected_episode;
    uint16_t selected_lvl_file_number;
    uint16_t level_enemy_count;
    uint16_t level_event_count;
    uint16_t pic_count;
    uint16_t shp_section_count;
    uint16_t mus_song_count;
    uint16_t selected_mus_song;
    uint32_t hdt_enemy_table_offset;
    uint32_t raw_bytes_referenced;
} OtDataCatalog;

/*
 * Direct ROMFS counterpart of the text tables populated by
 * OpenTyrian JE_loadHelpText().  Only tables used by the GBA front-end and
 * level-completion screen are retained; all strings are decrypted from the
 * stock tyrian.hdt at runtime.
 */
typedef struct {
    char planet_name[21][16];
    char misc_text[68][42];
    char title_menu[7][21];
    char full_game_menu[7][18];
    char episode_name[6][31];
    char difficulty_name[7][21];
    char gameplay_name[5][26];
} OtFrontendText;

bool ot_data_init(void);
const OtDataCatalog *ot_data_catalog(void);
bool ot_data_frontend_text_load(OtFrontendText *text);

/*
 * Interpret the stock encrypted levelsN.dat script without a generated
 * catalog. play_mode is 0 for Full Game and non-zero for Arcade;
 * difficulty follows the source values Easy=1, Normal=2, Hard=3.
 */
bool ot_data_episode_level_resolve(
    uint8_t episode,
    uint16_t main_section,
    uint8_t play_mode,
    uint8_t difficulty,
    OtEpisodeLevel *level
);

/*
 * Resolve the stock script up to its ]G map-choice directive.  Sections
 * which lead directly to ]L are returned as a one-entry direct_level route,
 * and ]Q is reported without inventing a destination.
 */
bool ot_data_episode_map_resolve(
    uint8_t episode,
    uint16_t main_section,
    uint8_t play_mode,
    uint8_t difficulty,
    OtEpisodeMap *map
);

/*
 * Return the number of playable data sections in one stock tyrianN.lvl.
 * The on-disk offset table contains two entries per level plus one final
 * sentinel; callers never need a generated per-Episode level catalog.
 */
bool ot_data_episode_lvl_count(
    uint8_t episode,
    uint16_t *level_count
);

/*
 * Select one original section in tyrianN.lvl. All following views point
 * directly into that ROMFS file.
 */
bool ot_data_level_select(
    uint8_t episode,
    uint16_t lvl_file_number
);
bool ot_data_level_info(OtLevelInfo *info);
bool ot_data_level_event_read(uint16_t index, OtEventRecord *event);
bool ot_data_level_enemy_pool_read(uint16_t index, uint16_t *enemy_id);
bool ot_data_level_map_shape_view(uint8_t layer, OtDataView *view);
bool ot_data_level_map_view(uint8_t layer, OtDataView *view);
bool ot_data_background_shape_file_view(
    char shape_file,
    OtDataView *view
);
bool ot_data_hdt_enemy_read(
    uint16_t enemy_id,
    OtEnemyDefinition *enemy
);
bool ot_data_hdt_weapon_read(
    uint16_t weapon_id,
    OtWeaponDefinition *weapon
);
bool ot_data_hdt_weapon_port_read(
    uint8_t port_id,
    OtWeaponPortDefinition *port
);
bool ot_data_hdt_special_read(
    uint8_t special_id,
    OtSpecialDefinition *special
);
bool ot_data_hdt_option_read(
    uint8_t option_id,
    OtOptionDefinition *option
);

/*
 * PIC numbers retain OpenTyrian's one-based JE_loadPic() convention.
 * The decoder writes the original 320x200 indexed image.
 */
bool ot_data_pic_view(uint8_t picture_number, OtDataView *view);
bool ot_data_pic_palette_view(
    uint8_t picture_number,
    OtDataView *view
);
bool ot_data_palette_view(
    uint8_t palette_index,
    OtDataView *view
);
bool ot_data_pic_decode(
    uint8_t picture_number,
    uint8_t *destination,
    uint32_t destination_bytes
);

/*
 * SHP section numbers are one-based like the on-disk table.  Sprite table
 * and sprite indices use OpenTyrian's zero-based Sprite_array convention.
 */
bool ot_data_shp_section_view(uint8_t section_number, OtDataView *view);
bool ot_data_shp_sprite_read(
    uint8_t table,
    uint16_t sprite_index,
    OtShpSprite *sprite
);
bool ot_data_comp_shape_bank_view(
    uint8_t shape_table,
    OtDataView *view
);
bool ot_data_comp_shape_sprite_view(
    uint8_t shape_table,
    uint16_t sprite_number,
    OtDataView *view
);

/* MUS song indices are zero-based, matching play_song(). */
bool ot_data_mus_song_read(
    uint16_t song_index,
    OtDataView *view,
    OtMusSongInfo *info
);
bool ot_data_mus_select(uint16_t song_index);

#endif
