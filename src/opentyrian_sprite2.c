/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal platform-neutral translation of OpenTyrian sprite.c:
 * blit_sprite2(), blit_sprite2_filter() and JE_drawEnemy's 2x2 layout.
 */
#include "opentyrian_sprite2.h"

#include <string.h>

#include "opentyrian_data.h"

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
