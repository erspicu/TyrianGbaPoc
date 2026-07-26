/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Zero-copy readers for the stock Tyrian MUS/SHP/PIC/HDT/LVL formats stored
 * in the cartridge ROMFS.
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
    OT_LEVEL1_LVL_FILE_NUMBER = 9,
    OT_LEVEL1_LVL_OFFSET_INDEX = (OT_LEVEL1_LVL_FILE_NUMBER - 1) * 2,
    OT_LEVEL1_EXPECTED_EVENT_COUNT = 1009,
    OT_LEVEL1_EVENT_RECORD_BYTES = 11,
    OT_HDT_WEAPON_COUNT = 781,
    OT_HDT_WEAPON_RECORD_BYTES = 80,
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
    char map_file;
    char shape_file;
    uint16_t map_x;
    uint16_t map_x2;
    uint16_t map_x3;
    uint16_t enemy_count;
    uint16_t event_count;
    uint32_t section_offset;
    uint32_t section_bytes;
} OtLevel1Info;

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
    uint16_t level1_enemy_count;
    uint16_t level1_event_count;
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
bool ot_data_level1_info(OtLevel1Info *info);
bool ot_data_level1_event_read(uint16_t index, OtEventRecord *event);
bool ot_data_level1_enemy_pool_read(uint16_t index, uint16_t *enemy_id);
bool ot_data_level1_map_shape_view(uint8_t layer, OtDataView *view);
bool ot_data_level1_map_view(uint8_t layer, OtDataView *view);
bool ot_data_hdt_enemy_read(
    uint16_t enemy_id,
    OtEnemyDefinition *enemy
);
bool ot_data_hdt_weapon_read(
    uint16_t weapon_id,
    OtWeaponDefinition *weapon
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
