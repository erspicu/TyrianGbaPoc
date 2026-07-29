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

extern const uint8_t sprite2_raw_components[];
extern const uint8_t sprite2_raw_components_end[];

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
        SPRITE2_RAW_COMPONENT_COUNT ==
            SPRITE2_RAW_TABLE_COUNT *
                SPRITE2_RAW_COMPONENTS_PER_TABLE &&
        SPRITE2_RAW_ROUNDTRIP_COMPONENTS ==
            SPRITE2_RAW_COMPONENT_COUNT;
}

const uint8_t *ot_sprite2_raw_component(
    uint8_t shape_table,
    uint16_t sprite_number
)
{
    uint32_t component;

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
    return
        sprite2_raw_components +
        component * SPRITE2_RAW_COMPONENT_BYTES;
}

bool ot_sprite2_raw_component_matches_rle(
    uint8_t shape_table,
    uint16_t sprite_number,
    uint16_t *scratch,
    uint32_t scratch_pixels
)
{
    const uint8_t *raw = ot_sprite2_raw_component(
        shape_table,
        sprite_number
    );
    uint8_t y;

    if (
        raw == 0 ||
        scratch == 0 ||
        scratch_pixels < OT_SPRITE2_FRAME_PIXELS ||
        !ot_sprite2_frame_decode(
            shape_table,
            sprite_number,
            0,
            0,
            scratch,
            scratch_pixels
        )
    ) {
        return false;
    }
    for (y = 0; y < OT_SPRITE2_FRAME_HEIGHT; y++) {
        uint8_t x;

        for (x = 0; x < OT_SPRITE2_FRAME_WIDTH; x++) {
            uint16_t expected = 0;

            if (
                x >= 10 &&
                x < 10 + SPRITE2_RAW_COMPONENT_WIDTH &&
                y >= 9 &&
                y < 9 + SPRITE2_RAW_COMPONENT_HEIGHT
            ) {
                uint8_t pixel = raw[
                    (uint32_t)(y - 9) *
                        SPRITE2_RAW_COMPONENT_WIDTH +
                    (x - 10)
                ];

                if (pixel != 0) expected = (uint16_t)pixel + 1u;
            }
            if (
                scratch[
                    (uint32_t)y * OT_SPRITE2_FRAME_WIDTH + x
                ] != expected
            ) {
                return false;
            }
        }
    }
    return true;
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
    OtDataView encoded;
    uint32_t source = 0;
    uint8_t x = 0;
    uint8_t y = 0;

    if (
        !ot_data_comp_shape_sprite_view(
            shape_table,
            sprite_number,
            &encoded
        )
    ) {
        return false;
    }

    while (source < encoded.size) {
        uint8_t code = encoded.data[source++];
        uint8_t skip_count;
        uint8_t fill_count;

        /* OpenTyrian treats 0x0f as the component terminator. */
        if (code == 0x0f) return true;

        skip_count = (uint8_t)(code & 0x0f);
        fill_count = (uint8_t)(code >> 4);
        x = (uint8_t)(x + skip_count);

        if (fill_count == 0) {
            /*
             * This is pixels += pitch - 12 in blit_sprite2().  Stock
             * streams finish every encoded row at exactly x=12.
             */
            if (
                x != OT_SPRITE2_COMPONENT_WIDTH ||
                y >= OT_SPRITE2_COMPONENT_HEIGHT
            ) {
                return false;
            }
            x = 0;
            y++;
            continue;
        }

        if (
            y >= OT_SPRITE2_COMPONENT_HEIGHT ||
            x + fill_count > OT_SPRITE2_COMPONENT_WIDTH ||
            source + fill_count > encoded.size
        ) {
            return false;
        }
        while (fill_count-- != 0) {
            uint8_t pixel = encoded.data[source++];
            uint8_t presented = filter != 0 ?
                (uint8_t)(filter | (pixel & 0x0f)) :
                pixel;
            uint32_t target =
                (uint32_t)(origin_y + y) * OT_SPRITE2_FRAME_WIDTH +
                origin_x + x;

            destination[target] = (uint16_t)presented + 1u;
            x++;
        }
    }
    return false;
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
