/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct decoder for OpenTyrian's Sprite2/newsh*.shp command stream.
 * The decoder preserves PC palette indices.  A zero uint16_t output value
 * means transparent; an opaque PC index N is stored as N + 1 so index zero
 * remains representable without a separate mask.
 */
#ifndef TYRIAN_GBA_OPENTYRIAN_SPRITE2_H
#define TYRIAN_GBA_OPENTYRIAN_SPRITE2_H

#include <stdbool.h>
#include <stdint.h>

enum {
    OT_SPRITE2_COMPONENT_WIDTH = 12,
    OT_SPRITE2_COMPONENT_HEIGHT = 14,
    OT_SPRITE2_FRAME_WIDTH = 32,
    OT_SPRITE2_FRAME_HEIGHT = 32,
    OT_SPRITE2_FRAME_PIXELS =
        OT_SPRITE2_FRAME_WIDTH * OT_SPRITE2_FRAME_HEIGHT,
};

/*
 * graphic retains OpenTyrian's one-based Sprite2 index.  A size value of
 * one composes graphic, +1, +19 and +20 exactly like JE_drawEnemy();
 * every other value decodes the single component.
 */
bool ot_sprite2_frame_decode(
    uint8_t shape_table,
    uint16_t graphic,
    uint8_t size,
    uint8_t filter,
    uint16_t *destination,
    uint32_t destination_pixels
);

#endif
