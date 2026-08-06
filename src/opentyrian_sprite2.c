/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal platform-neutral translation of OpenTyrian sprite.c:
 * blit_sprite2(), blit_sprite2_filter() and JE_drawEnemy's 2x2 layout.
 */
#include "opentyrian_sprite2.h"

#include <string.h>

#include "res/sprite2_raw_meta.h"
#include "opentyrian_data.h"
#include "opentyrian_season.h"

extern const uint8_t sprite2_raw_components[];
extern const uint8_t sprite2_raw_components_end[];
extern const uint8_t sprite2_xmas_raw_components[];
extern const uint8_t sprite2_xmas_raw_components_end[];

static int christmas_table_index(uint8_t shape_table)
{
    switch (shape_table) {
    case SPRITE2_XMAS_RAW_TABLE_0:
        return 0;
    case SPRITE2_XMAS_RAW_TABLE_1:
        return 1;
    case SPRITE2_XMAS_RAW_TABLE_2:
        return 2;
    default:
        return -1;
    }
}

bool ot_sprite2_raw_catalog_valid(void)
{
    return
        SPRITE2_RAW_VERSION == 1u &&
        SPRITE2_RAW_TABLE_COUNT ==
            OT_COMP_SHAPE_TABLE_OPTIONS_SMALL &&
        SPRITE2_RAW_COMPONENT_WIDTH ==
            OT_SPRITE2_COMPONENT_WIDTH &&
        SPRITE2_RAW_COMPONENT_HEIGHT ==
            OT_SPRITE2_COMPONENT_HEIGHT &&
        (uint32_t)(
            sprite2_raw_components_end -
            sprite2_raw_components
        ) == SPRITE2_RAW_DATA_BYTES &&
        (uint32_t)(
            sprite2_xmas_raw_components_end -
            sprite2_xmas_raw_components
        ) == SPRITE2_XMAS_RAW_DATA_BYTES &&
        SPRITE2_RAW_COMPONENT_COUNT ==
            SPRITE2_RAW_TABLE_COUNT *
                SPRITE2_RAW_COMPONENTS_PER_TABLE &&
        SPRITE2_RAW_ROUNDTRIP_COMPONENTS ==
            SPRITE2_RAW_COMPONENT_COUNT &&
        SPRITE2_XMAS_RAW_COMPONENT_COUNT ==
            SPRITE2_XMAS_RAW_TABLE_COUNT *
                SPRITE2_RAW_COMPONENTS_PER_TABLE &&
        SPRITE2_XMAS_RAW_ROUNDTRIP_COMPONENTS ==
            SPRITE2_XMAS_RAW_COMPONENT_COUNT;
}

const uint8_t *ot_sprite2_raw_component(
    uint8_t shape_table,
    uint16_t sprite_number
)
{
    uint32_t component;
    int xmas_table;

    if (
        !ot_sprite2_raw_catalog_valid() ||
        shape_table == 0 ||
        shape_table > SPRITE2_RAW_TABLE_COUNT ||
        sprite_number == 0 ||
        sprite_number > SPRITE2_RAW_COMPONENTS_PER_TABLE
    ) {
        return 0;
    }
    component =
        (uint32_t)(shape_table - 1u) *
            SPRITE2_RAW_COMPONENTS_PER_TABLE +
        (uint32_t)(sprite_number - 1u);
    xmas_table = ot_season_mode() == OT_SEASON_XMAS ?
        christmas_table_index(shape_table) :
        -1;
    if (xmas_table >= 0) {
        component =
            (uint32_t)xmas_table *
                SPRITE2_RAW_COMPONENTS_PER_TABLE +
            (uint32_t)(sprite_number - 1u);
        return
            sprite2_xmas_raw_components +
            component * SPRITE2_RAW_COMPONENT_BYTES;
    }
    return
        sprite2_raw_components +
        component * SPRITE2_RAW_COMPONENT_BYTES;
}

static bool decode_component(
    uint8_t shape_table,
    uint16_t sprite_number,
    uint8_t filter,
    uint16_t *destination,
    uint8_t origin_x,
    uint8_t origin_y
)
{
    const uint8_t *raw = ot_sprite2_raw_component(
        shape_table,
        sprite_number
    );
    uint8_t y;

    if (
        raw == 0 ||
        origin_x + OT_SPRITE2_COMPONENT_WIDTH >
            OT_SPRITE2_FRAME_WIDTH ||
        origin_y + OT_SPRITE2_COMPONENT_HEIGHT >
            OT_SPRITE2_FRAME_HEIGHT
    ) {
        return false;
    }
    for (y = 0; y < OT_SPRITE2_COMPONENT_HEIGHT; y++) {
        uint8_t x;

        for (x = 0; x < OT_SPRITE2_COMPONENT_WIDTH; x++) {
            uint8_t pixel = raw[
                (uint32_t)y * OT_SPRITE2_COMPONENT_WIDTH + x
            ];

            if (pixel != 0) {
                uint8_t presented = filter != 0 ?
                    (uint8_t)(filter | (pixel & 0x0f)) :
                    pixel;
                uint32_t target =
                    (uint32_t)(origin_y + y) *
                        OT_SPRITE2_FRAME_WIDTH +
                    origin_x + x;

                destination[target] = (uint16_t)presented + 1u;
            }
        }
    }
    return true;
}

bool ot_sprite2_frame_decode(
    uint8_t shape_table,
    uint16_t graphic,
    uint8_t size,
    uint8_t filter,
    uint16_t *destination,
    uint32_t destination_pixels
)
{
    if (
        destination == 0 ||
        destination_pixels < OT_SPRITE2_FRAME_PIXELS ||
        graphic == 0
    ) {
        return false;
    }
    memset(
        destination,
        0,
        OT_SPRITE2_FRAME_PIXELS * sizeof(destination[0])
    );

    if (size == 1) {
        /*
         * JE_drawEnemy blits at (-6,-7), (+6,-7), (-6,+7), (+6,+7).
         * The enclosing 32x32 OBJ starts at enemy (-10,-9), hence these
         * exact component origins.
         */
        return
            decode_component(
                shape_table, graphic, filter, destination, 4, 2
            ) &&
            decode_component(
                shape_table, (uint16_t)(graphic + 1),
                filter, destination, 16, 2
            ) &&
            decode_component(
                shape_table, (uint16_t)(graphic + 19),
                filter, destination, 4, 16
            ) &&
            decode_component(
                shape_table, (uint16_t)(graphic + 20),
                filter, destination, 16, 16
            );
    }

    /* A 12x14 PC component is anchored in the same 32x32 enemy container. */
    return decode_component(
        shape_table,
        graphic,
        filter,
        destination,
        10,
        9
    );
}
