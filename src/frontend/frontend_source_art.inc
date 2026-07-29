/*
 * Sparse choice patches run only on Quit selection changes.  Keep this
 * comparatively large parser in ROM so the gameplay hot-path retains the
 * project's 6 KiB IWRAM safety margin.
 */
static __attribute__((noinline, noclone)) u8
frontend_apply_static_sparse_overlay(
    const u8 *source,
    u32 total_bytes,
    u16 expected_run_count,
    u16 expected_version,
    u8 magic_kind
)
{
    u32 consumed = 8;
    u16 run_count;
    u16 run;

    if (
        source[0] != 'O' ||
        source[1] != 'T' ||
        source[2] != 'Q' ||
        source[3] != magic_kind ||
        (u16)(source[4] | (source[5] << 8)) !=
            expected_version
    ) {
        return 0;
    }
    run_count = (u16)(source[6] | (source[7] << 8));
    if (run_count != expected_run_count) {
        return 0;
    }
    source += 8;
    for (run = 0; run < run_count; run++) {
        u16 offset;
        u16 length;
        u16 padded_length;
        u8 *destination;

        if (
            consumed + 4u >
            total_bytes
        ) {
            return 0;
        }
        offset = (u16)(source[0] | (source[1] << 8));
        length = (u16)(source[2] | (source[3] << 8));
        source += 4;
        consumed += 4;
        padded_length = (u16)((length + 3u) & ~3u);
        if (
            length == 0 ||
            (u32)offset + length > FRONTEND_FRAME_BYTES ||
            consumed + padded_length >
                total_bytes
        ) {
            return 0;
        }
        destination = frontend_frame_scratch + offset;
        if ((offset & 3u) == 0) {
            const u32 *source_words =
                (const u32 *)(const void *)source;
            u32 *destination_words =
                (u32 *)(void *)destination;
            u16 word_count = (u16)(length >> 2);
            u16 tail_count = (u16)(length & 3u);
            u16 word;
            const u8 *tail_source;

            for (word = 0; word < word_count; word++) {
                destination_words[word] = source_words[word];
            }
            destination += (u16)(word_count << 2);
            tail_source = source + (u16)(word_count << 2);
            while (tail_count-- != 0) {
                *destination++ = *tail_source++;
            }
        } else {
            u16 byte;

            for (byte = 0; byte < length; byte++) {
                destination[byte] = source[byte];
            }
        }
        source += padded_length;
        consumed += padded_length;
    }
    return consumed == total_bytes;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_apply_static_quit_overlay(void)
{
    const u8 *source =
        frontend_static_quit_overlay +
        FRONTEND_STATIC_QUIT_OVERLAY_HEADER_BYTES;
    u8 row;

    if (
        frontend_static_quit_overlay[0] != 'O' ||
        frontend_static_quit_overlay[1] != 'T' ||
        frontend_static_quit_overlay[2] != 'Q' ||
        frontend_static_quit_overlay[3] != 'F' ||
        (u16)(
            frontend_static_quit_overlay[4] |
            (frontend_static_quit_overlay[5] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_VERSION ||
        (u16)(
            frontend_static_quit_overlay[6] |
            (frontend_static_quit_overlay[7] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_X ||
        (u16)(
            frontend_static_quit_overlay[8] |
            (frontend_static_quit_overlay[9] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_Y ||
        (u16)(
            frontend_static_quit_overlay[10] |
            (frontend_static_quit_overlay[11] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_WIDTH ||
        (u16)(
            frontend_static_quit_overlay[12] |
            (frontend_static_quit_overlay[13] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT
    ) {
        return 0;
    }
    for (
        row = 0;
        row < FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT;
        row++
    ) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)(
            frontend_frame_scratch +
            (u32)(FRONTEND_STATIC_QUIT_OVERLAY_Y + row) *
                SCREEN_WIDTH +
            FRONTEND_STATIC_QUIT_OVERLAY_X
        );
        u8 word;

        for (
            word = 0;
            word < FRONTEND_STATIC_QUIT_OVERLAY_WIDTH / 4u;
            word++
        ) {
            u32 pixel = source_words[word];
            u32 inverse = ~pixel;
            u32 transparent = (
                (inverse - 0x01010101u) &
                ~inverse &
                0x80808080u
            );

            if (transparent == 0) {
                destination_words[word] = pixel;
            } else {
                const u8 *source_pixels =
                    (const u8 *)(const void *)&pixel;
                u8 *destination_pixels =
                    (u8 *)(void *)&destination_words[word];
                u8 index;

                for (index = 0; index < 4; index++) {
                    if (source_pixels[index] != 0xFF) {
                        destination_pixels[index] =
                            source_pixels[index];
                    }
                }
            }
        }
        source += FRONTEND_STATIC_QUIT_OVERLAY_WIDTH;
    }
    return 1;
}

static u8 frontend_apply_static_quit_choice(void)
{
    u8 variant = frontend_quit_yes ? 0 : 1;

    return frontend_apply_static_sparse_overlay(
        frontend_static_quit_choices +
            (u32)variant *
                FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES,
        FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES,
        FRONTEND_STATIC_QUIT_CHOICE_RUN_COUNT,
        FRONTEND_STATIC_QUIT_CHOICE_VERSION,
        'C'
    );
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_apply_static_quit_shade(void)
{
    const u8 *source = frontend_static_quit_shade;
    u16 run_count;
    u16 shaded_pixels = 0;
    u16 run;

    if (
        source[0] != 'O' ||
        source[1] != 'T' ||
        source[2] != 'Q' ||
        source[3] != 'S' ||
        (u16)(source[4] | (source[5] << 8)) !=
            FRONTEND_STATIC_QUIT_SHADE_VERSION
    ) {
        return 0;
    }
    run_count = (u16)(source[6] | (source[7] << 8));
    if (run_count != FRONTEND_STATIC_QUIT_SHADE_RUN_COUNT) {
        return 0;
    }
    source += 8;
    for (run = 0; run < run_count; run++) {
        u16 offset = (u16)(source[0] | (source[1] << 8));
        u16 length = (u16)(source[2] | (source[3] << 8));
        u8 *destination;

        source += 4;
        if (
            length == 0 ||
            (u32)offset + length > FRONTEND_FRAME_BYTES
        ) {
            return 0;
        }
        destination = frontend_frame_scratch + offset;
        shaded_pixels = (u16)(shaded_pixels + length);
        while (length != 0 && ((u32)destination & 3u) != 0) {
            u8 pixel = *destination;
            u8 value = pixel & 0x0Fu;

            value = value > 3 ? (u8)(value - 3u) : 0;
            *destination++ = (u8)((pixel & 0xF0u) | value);
            length--;
        }
        while (length >= 4) {
            u32 *pixels = (u32 *)(void *)destination;
            u32 pixel = *pixels;
            u32 value = pixel & 0x0f0f0f0fu;
            u32 difference =
                (value | 0x10101010u) - 0x03030303u;
            u32 valid = difference & 0x10101010u;
            u32 mask = valid - (valid >> 4);

            *pixels =
                (pixel & 0xf0f0f0f0u) |
                (difference & mask);
            destination += 4;
            length = (u16)(length - 4u);
        }
        while (length-- != 0) {
            u8 pixel = *destination;
            u8 value = pixel & 0x0Fu;

            value = value > 3 ? (u8)(value - 3u) : 0;
            *destination++ = (u8)((pixel & 0xF0u) | value);
        }
    }
    return shaded_pixels == FRONTEND_STATIC_QUIT_SHADE_PIXEL_COUNT;
}

enum {
    FRONTEND_SOURCE_BLIT_NORMAL = 0,
    FRONTEND_SOURCE_BLIT_DARK = 1,
    FRONTEND_SOURCE_BLIT_HUE = 2,
};

static void frontend_u32_string(
    u32 value,
    char destination[11]
)
{
    char reversed[10];
    u8 count = 0;
    u8 index;

    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reversed));
    for (index = 0; index < count; index++) {
        destination[index] = reversed[count - index - 1u];
    }
    destination[count] = '\0';
}

static void frontend_text_with_u32(
    char *destination,
    u8 destination_size,
    const char *prefix,
    u32 value
)
{
    char digits[11];
    u8 position = 0;

    if (destination == 0 || destination_size == 0) return;
    while (
        prefix != 0 &&
        *prefix != '\0' &&
        position + 1u < destination_size
    ) {
        destination[position++] = *prefix++;
    }
    frontend_u32_string(value, digits);
    {
        const char *digit = digits;

        while (*digit != '\0' && position + 1u < destination_size) {
            destination[position++] = *digit++;
        }
    }
    destination[position] = '\0';
}

static u8 frontend_source_colour(
    u8 pixel,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    s16 value;

    if (mode == FRONTEND_SOURCE_BLIT_NORMAL) return pixel;
    value = (s16)(pixel & 0x0fu) + brightness;
    if (mode == FRONTEND_SOURCE_BLIT_DARK) {
        value -= 4;
        hue = pixel >> 4;
    }
    if (value < 0) value = 0;
    if (value > 15) value = 15;
    return (u8)((hue << 4) | (u8)value);
}

static s16 frontend_source_to_screen_x(s16 source_x)
{
    return (s16)(
        (
            (s32)source_x - FRONTEND_MENU_SOURCE_CROP_X
        ) *
        SCREEN_WIDTH /
        FRONTEND_MENU_SOURCE_WIDTH
    );
}

static s16 frontend_source_to_screen_y(s16 source_y)
{
    return (s16)(
        (s32)source_y * SCREEN_HEIGHT /
        OT_PIC_DECODED_HEIGHT
    );
}

static void frontend_source_plot(
    u8 *frame,
    s16 source_x,
    s16 source_y,
    u8 pixel,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    u16 x;
    u16 y;

    if (
        frame == 0 ||
        source_x < (s16)FRONTEND_MENU_SOURCE_CROP_X ||
        source_x >=
            (s16)(
                FRONTEND_MENU_SOURCE_CROP_X +
                FRONTEND_MENU_SOURCE_WIDTH
            ) ||
        source_y < 0 ||
        source_y >= (s16)OT_PIC_DECODED_HEIGHT
    ) {
        return;
    }
    x = (u16)frontend_source_to_screen_x(source_x);
    y = (u16)frontend_source_to_screen_y(source_y);
    frame[(u32)y * SCREEN_WIDTH + x] =
        frontend_source_colour(pixel, mode, hue, brightness);
}

static u32 frontend_source_stamp_shp_key(
    u8 table,
    u16 sprite_index
)
{
    if (
        table == 5 &&
        sprite_index >= FRONTEND_SOURCE_STAMP_SHP_TABLE5_FIRST &&
        sprite_index <
            FRONTEND_SOURCE_STAMP_SHP_TABLE5_FIRST +
                FRONTEND_SOURCE_STAMP_SHP_TABLE5_COUNT
    ) {
        return
            sprite_index -
            FRONTEND_SOURCE_STAMP_SHP_TABLE5_FIRST;
    }
    if (
        table == 6 &&
        sprite_index <
            FRONTEND_SOURCE_STAMP_SHP_TABLE6_FIRST +
                FRONTEND_SOURCE_STAMP_SHP_TABLE6_COUNT
    ) {
        return
            FRONTEND_SOURCE_STAMP_SHP_TABLE5_COUNT +
            sprite_index -
            FRONTEND_SOURCE_STAMP_SHP_TABLE6_FIRST;
    }
    return FRONTEND_SOURCE_STAMP_KEY_COUNT;
}

static u32 frontend_source_stamp_comp_key(
    u8 shape_table,
    u16 graphic
)
{
    if (
        shape_table <
            FRONTEND_SOURCE_STAMP_COMP_TABLE_FIRST ||
        shape_table >=
            FRONTEND_SOURCE_STAMP_COMP_TABLE_FIRST +
                FRONTEND_SOURCE_STAMP_COMP_TABLE_COUNT ||
        graphic == 0 ||
        graphic > FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT
    ) {
        return FRONTEND_SOURCE_STAMP_KEY_COUNT;
    }
    return
        FRONTEND_SOURCE_STAMP_SHP_KEY_COUNT +
        (
            (u32)(
                shape_table -
                FRONTEND_SOURCE_STAMP_COMP_TABLE_FIRST
            ) *
                FRONTEND_SOURCE_STAMP_COMP_GRAPHIC_COUNT
        ) +
        graphic - 1u;
}

/*
 * Build-time source stamps are the static-menu equivalent of the gameplay
 * Sprite2 raw catalog: stock indices and layering remain authoritative,
 * while immutable RLE decode and 4/5 coordinate scaling leave the ARM7
 * hot path.  Keep this parser in ROM; sequential Game Pak reads benefit
 * from prefetch and the gameplay IWRAM budget remains untouched.
 */
static __attribute__((noinline, noclone)) u8
frontend_blit_source_stamp(
    u8 *frame,
    u32 key,
    s16 source_x,
    s16 source_y,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    const u32 *offsets =
        (const u32 *)(const void *)frontend_source_stamp_offsets;
    u32 phase_x;
    u32 phase_y;
    u32 record;
    u32 stream_offset;
    u32 stream_end;
    const u8 *stream;
    const u8 *cursor;
    const u8 *end;
    u16 run_count;
    u16 run;
    s16 base_x;
    s16 base_y;

    if (
        frame == 0 ||
        key >= FRONTEND_SOURCE_STAMP_KEY_COUNT ||
        source_x < (s16)FRONTEND_MENU_SOURCE_CROP_X ||
        source_y < 0
    ) {
        return 0;
    }
    phase_x = (u32)(
        source_x - FRONTEND_MENU_SOURCE_CROP_X
    ) % FRONTEND_SOURCE_STAMP_SCALE_PHASES;
    phase_y =
        (u32)source_y % FRONTEND_SOURCE_STAMP_SCALE_PHASES;
    record =
        key * FRONTEND_SOURCE_STAMP_PHASE_COUNT +
        phase_y * FRONTEND_SOURCE_STAMP_SCALE_PHASES +
        phase_x;
    if (record >= FRONTEND_SOURCE_STAMP_COUNT) return 0;
    stream_offset = offsets[record];
    stream_end =
        record + 1u < FRONTEND_SOURCE_STAMP_COUNT ?
            offsets[record + 1u] :
            FRONTEND_SOURCE_STAMP_DATA_BYTES;
    if (
        stream_offset >= stream_end ||
        stream_end > FRONTEND_SOURCE_STAMP_DATA_BYTES ||
        stream_end - stream_offset < 4u
    ) {
        return 0;
    }
    stream = frontend_source_stamp_data + stream_offset;
    run_count = (u16)(stream[0] | ((u16)stream[1] << 8));
    cursor = stream + 4;
    end = frontend_source_stamp_data + stream_end;
    base_x = frontend_source_to_screen_x(source_x);
    base_y = frontend_source_to_screen_y(source_y);

    for (run = 0; run < run_count; run++) {
        u8 relative_y;
        u8 relative_x;
        u16 length;
        u32 padded_length;
        s16 target_x;
        s16 target_y;
        u16 source_skip = 0;
        u16 visible_length;
        u16 pixel_index;

        if ((u32)(end - cursor) < 4u) return 0;
        relative_y = cursor[0];
        relative_x = cursor[1];
        length = (u16)(cursor[2] | ((u16)cursor[3] << 8));
        cursor += 4;
        padded_length = (length + 3u) & ~3u;
        if (
            length == 0 ||
            (u32)(end - cursor) < padded_length
        ) {
            return 0;
        }
        target_x = (s16)(base_x + relative_x);
        target_y = (s16)(base_y + relative_y);
        visible_length = length;
        if (target_x < 0) {
            source_skip = (u16)(-target_x);
            if (source_skip >= visible_length) {
                cursor += padded_length;
                continue;
            }
            visible_length = (u16)(visible_length - source_skip);
            target_x = 0;
        }
        if (target_x >= SCREEN_WIDTH) {
            cursor += padded_length;
            continue;
        }
        if (target_x + visible_length > SCREEN_WIDTH) {
            visible_length = (u16)(SCREEN_WIDTH - target_x);
        }
        if (
            target_y >= 0 &&
            target_y < SCREEN_HEIGHT &&
            visible_length != 0
        ) {
            u8 *destination =
                frame + (u32)target_y * SCREEN_WIDTH + target_x;
            const u8 *source_pixels = cursor + source_skip;

            if (mode == FRONTEND_SOURCE_BLIT_NORMAL) {
                memcpy(destination, source_pixels, visible_length);
            } else {
                for (
                    pixel_index = 0;
                    pixel_index < visible_length;
                    pixel_index++
                ) {
                    destination[pixel_index] =
                        frontend_source_colour(
                            source_pixels[pixel_index],
                            mode,
                            hue,
                            brightness
                        );
                }
            }
        }
        cursor += padded_length;
    }
    return cursor == end;
}

static u8 frontend_blit_shp_source(
    u8 *frame,
    u8 table,
    u16 sprite_index,
    s16 source_x,
    s16 source_y,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    OtShpSprite sprite;
    u32 position = 0;
    u16 x = 0;
    u16 y = 0;
    u32 stamp_key = frontend_source_stamp_shp_key(
        table,
        sprite_index
    );

    if (
        frontend_blit_source_stamp(
            frame,
            stamp_key,
            source_x,
            source_y,
            mode,
            hue,
            brightness
        )
    ) {
        return 1;
    }

    if (
        !ot_data_shp_sprite_read(table, sprite_index, &sprite) ||
        !sprite.populated ||
        sprite.width == 0 ||
        sprite.height == 0
    ) {
        return 0;
    }
    telemetry_frontend_runtime_shp_decodes++;
    while (position < sprite.encoded.size && y < sprite.height) {
        u8 code = sprite.encoded.data[position++];

        if (code == 255) {
            if (position >= sprite.encoded.size) return 0;
            x = (u16)(x + sprite.encoded.data[position++]);
        } else if (code == 254) {
            x = sprite.width;
        } else if (code == 253) {
            x++;
        } else {
            if (x < sprite.width) {
                frontend_source_plot(
                    frame,
                    (s16)(source_x + (s16)x),
                    (s16)(source_y + (s16)y),
                    code,
                    mode,
                    hue,
                    brightness
                );
            }
            x++;
        }
        while (x >= sprite.width && y < sprite.height) {
            x = (u16)(x - sprite.width);
            y++;
        }
    }
    return y >= sprite.height;
}

static u8 frontend_blit_comp_source(
    u8 *frame,
    u8 shape_table,
    u16 graphic,
    s16 source_x,
    s16 source_y,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    u8 y;
    u32 stamp_key = frontend_source_stamp_comp_key(
        shape_table,
        graphic
    );

    if (
        frontend_blit_source_stamp(
            frame,
            stamp_key,
            source_x,
            source_y,
            mode,
            hue,
            brightness
        )
    ) {
        return 1;
    }

    if (
        graphic == 0 ||
        !ot_sprite2_frame_decode(
            shape_table,
            graphic,
            1,
            0,
            frontend_sprite2_decode_scratch,
            OT_SPRITE2_FRAME_PIXELS
        )
    ) {
        return 0;
    }
    telemetry_frontend_runtime_sprite2_decodes++;
    /*
     * ot_sprite2_frame_decode() uses the gameplay 32x32 container with the
     * first component at (4,2).  blit_sprite2x2() anchors that component at
     * the requested point, so compensate exactly for the container margin.
     */
    source_x = (s16)(source_x - 4);
    source_y = (s16)(source_y - 2);
    for (y = 0; y < OT_SPRITE2_FRAME_HEIGHT; y++) {
        u8 x;

        for (x = 0; x < OT_SPRITE2_FRAME_WIDTH; x++) {
            u16 stored = frontend_sprite2_decode_scratch[
                (u16)y * OT_SPRITE2_FRAME_WIDTH + x
            ];

            if (stored != 0) {
                frontend_source_plot(
                    frame,
                    (s16)(source_x + x),
                    (s16)(source_y + y),
                    (u8)(stored - 1u),
                    mode,
                    hue,
                    brightness
                );
            }
        }
    }
    return 1;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
frontend_shade_source_rect(
    u8 *frame,
    s16 source_x0,
    s16 source_y0,
    s16 source_x1,
    s16 source_y1,
    u8 amount
)
{
    s16 mapped_x0 = frontend_source_to_screen_x(source_x0);
    s16 mapped_y0 = frontend_source_to_screen_y(source_y0);
    s16 mapped_x1 = frontend_source_to_screen_x(
        (s16)(source_x1 + 1)
    );
    s16 mapped_y1 = frontend_source_to_screen_y(
        (s16)(source_y1 + 1)
    );
    u16 x0;
    u16 y0;
    u16 x1;
    u16 y1;
    u16 y;

    if (mapped_x0 < 0) mapped_x0 = 0;
    if (mapped_y0 < 0) mapped_y0 = 0;
    if (mapped_x1 < 0) mapped_x1 = 0;
    if (mapped_y1 < 0) mapped_y1 = 0;
    x0 = (u16)mapped_x0;
    y0 = (u16)mapped_y0;
    x1 = (u16)mapped_x1;
    y1 = (u16)mapped_y1;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (
        amount == 3 &&
        (x0 & 3u) == 0 &&
        ((x1 - x0) & 3u) == 0
    ) {
        /*
         * Process four independent palette-value nibbles per ARM word.
         * Setting bit 4 before subtracting prevents cross-byte borrows; the
         * surviving bit 4 then expands into a 0x0f mask.  This reproduces
         * max(value - 3, 0) exactly, without per-pixel branches.
         */
        for (y = y0; y < y1; y++) {
            u32 *pixels = (u32 *)(void *)(
                frame + (u32)y * SCREEN_WIDTH + x0
            );
            u16 word_count = (u16)((x1 - x0) >> 2);
            u16 word;

            for (word = 0; word < word_count; word++) {
                u32 pixel = pixels[word];
                u32 value = pixel & 0x0f0f0f0fu;
                u32 difference =
                    (value | 0x10101010u) - 0x03030303u;
                u32 valid = difference & 0x10101010u;
                u32 mask = valid - (valid >> 4);

                pixels[word] =
                    (pixel & 0xf0f0f0f0u) |
                    (difference & mask);
            }
        }
        return;
    }
    for (y = y0; y < y1; y++) {
        u16 x;

        for (x = x0; x < x1; x++) {
            u8 pixel = frame[(u32)y * SCREEN_WIDTH + x];
            u8 value = pixel & 0x0fu;

            value = value > amount ? (u8)(value - amount) : 0;
            frame[(u32)y * SCREEN_WIDTH + x] =
                (u8)((pixel & 0xf0u) | value);
        }
    }
}

static void frontend_copy_string(
    char *destination,
    u8 destination_size,
    const char *source
)
{
    u8 index = 0;

    if (destination == 0 || destination_size == 0) return;
    while (
        source != 0 &&
        source[index] != '\0' &&
        index + 1u < destination_size
    ) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static u32 frontend_weapon_total_cost(
    u16 base_cost,
    u8 power
)
{
    u32 total = base_cost;
    u32 increment = 0;
    u8 level;

    if (power == 0) power = 1;
    if (power > 11) power = 11;
    for (level = 1; level < power; level++) {
        increment += (u32)base_cost * level;
        total += increment;
    }
    return total;
}

static u32 frontend_upgrade_base_cost(
    u8 category,
    u8 item
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        if (item > 90) return 100;
        return ot_data_hdt_ship_read(item, &ship) ? ship.cost : 0;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        return ot_data_hdt_weapon_port_read(item, &port) ?
            port.cost :
            0;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        return ot_data_hdt_shield_read(item, &shield) ?
            shield.cost :
            0;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        return ot_data_hdt_power_read(item, &power) ?
            power.cost :
            0;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        return ot_data_hdt_option_read(item, &option) ?
            option.cost :
            0;
    }
    default:
        return 0;
    }
}

static u32 frontend_upgrade_total_cost(
    u8 category,
    u8 item,
    u8 power
)
{
    u32 base = frontend_upgrade_base_cost(category, item);

    if (
        category == FRONTEND_UPGRADE_FRONT_WEAPON ||
        category == FRONTEND_UPGRADE_REAR_WEAPON
    ) {
        return frontend_weapon_total_cost((u16)base, power);
    }
    return base;
}

static void frontend_upgrade_item_name(
    u8 category,
    u8 item,
    char *destination,
    u8 destination_size
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        if (item > 90) {
            frontend_text_with_u32(
                destination,
                destination_size,
                "CUSTOM SHIP ",
                item - 90u
            );
        } else if (ot_data_hdt_ship_read(item, &ship)) {
            frontend_copy_string(
                destination,
                destination_size,
                ship.name
            );
        } else {
            frontend_copy_string(
                destination,
                destination_size,
                "UNKNOWN SHIP"
            );
        }
        break;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_weapon_port_read(item, &port) ?
                port.name :
                "UNKNOWN WEAPON"
        );
        break;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_shield_read(item, &shield) ?
                shield.name :
                "UNKNOWN SHIELD"
        );
        break;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_power_read(item, &power) ?
                power.name :
                "UNKNOWN GENERATOR"
        );
        break;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_option_read(item, &option) ?
                option.name :
                "UNKNOWN SIDEKICK"
        );
        break;
    }
    default:
        frontend_copy_string(
            destination,
            destination_size,
            "UNKNOWN"
        );
        break;
    }
}

static u16 frontend_upgrade_item_graphic(
    u8 category,
    u8 item
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        return ot_data_hdt_ship_read(item, &ship) ?
            ship.shipgraphic :
            0;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        return ot_data_hdt_weapon_port_read(item, &port) ?
            port.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        return ot_data_hdt_shield_read(item, &shield) ?
            shield.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        return ot_data_hdt_power_read(item, &power) ?
            power.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        return ot_data_hdt_option_read(item, &option) ?
            option.itemgraphic :
            0;
    }
    default:
        return 0;
    }
}

static void frontend_draw_item_source(
    u8 *frame,
    u8 category,
    u8 item,
    s16 source_x,
    s16 source_y
)
{
    u16 graphic;

    if (item == 0) return;
    graphic = frontend_upgrade_item_graphic(category, item);
    if (graphic == 0) return;
    (void)frontend_blit_comp_source(
        frame,
        category == FRONTEND_UPGRADE_SHIP ?
            OT_COMP_SHAPE_TABLE_OPTIONS_SMALL :
            OT_COMP_SHAPE_TABLE_SHOP,
        graphic,
        source_x,
        source_y,
        FRONTEND_SOURCE_BLIT_NORMAL,
        0,
        0
    );
}

static void frontend_draw_segment_bar_source(
    u8 *frame,
    s16 source_x,
    s16 source_y,
    u8 resolution,
    u8 colour,
    u8 amount,
    u8 source_width,
    u8 source_height
)
{
    u8 segment_count;
    u8 segment;

    if (resolution == 0) return;
    segment_count = (u8)(amount / resolution);
    for (segment = 0; segment < segment_count; segment++) {
        u8 y;

        for (y = 0; y < source_height; y++) {
            u8 x;

            for (x = 0; x < source_width; x++) {
                u8 pixel = (u8)(colour + 12u);

                if (y == 0) pixel = (u8)(colour + 13u);
                if (y + 1u == source_height) {
                    pixel = (u8)(colour + 11u);
                }
                if (x == 0 && y == 0) {
                    pixel = (u8)(colour + 15u);
                }
                frontend_source_plot(
                    frame,
                    (s16)(
                        source_x +
                        segment * (source_width + 2u) +
                        x
                    ),
                    (s16)(source_y + y),
                    pixel,
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        }
    }
}

enum {
    FRONTEND_SHIP_DRAW_BODY = 0,
    FRONTEND_SHIP_DRAW_GENERATOR,
    FRONTEND_SHIP_DRAW_FRONT_WEAPON,
    FRONTEND_SHIP_DRAW_REAR_WEAPON,
    FRONTEND_SHIP_DRAW_LEFT_SIDEKICK,
    FRONTEND_SHIP_DRAW_RIGHT_SIDEKICK,
    FRONTEND_SHIP_DRAW_SHIELD,
    FRONTEND_SHIP_DRAW_STATUS,
    FRONTEND_SHIP_DRAW_PART_COUNT,
};

/*
 * The stock ship panel is deliberately separable: transition jobs execute
 * one source-equivalent layer per VBlank, while synchronous callers simply
 * iterate every part in the original draw order.
 */
static void frontend_draw_ship_illustration_part(
    u8 *frame,
    u8 part
)
{
    static const s16 weapon_sprite[43] = {
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 21, 5, 13, -1, 14, 15, 0,
        14, 9, 8, 2, 15, 0, 13, 0, 8, 8,
        11, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 2, 1
    };
    static const s8 front_xy[43] = {
        -1, 4, 9, 3, 8, 2, 5, 10, 1, -1,
        -1, -1, -1, 7, 8, -1, -1, 0, -1, 4,
        0, -1, -1, 3, -1, 4, -1, 4, -1, -1,
        -1, 9, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 3, 9
    };
    static const s16 front_x[12] = {
        59, 66, 66, 54, 61, 51, 58, 51, 61, 52, 53, 58
    };
    static const s16 front_y[12] = {
        38, 53, 41, 36, 48, 35, 41, 35, 53, 41, 39, 31
    };
    static const s8 rear_xy[43] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
        1, 2, 3, -1, 4, 5, -1, -1, 6, -1,
        -1, 1, 0, -1, 6, -1, 5, -1, 0, 0,
        3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, -1, -1
    };
    static const s16 rear_x[7] = {41, 27, 49, 43, 51, 39, 41};
    static const s16 rear_y[7] = {92, 92, 113, 102, 97, 96, 76};
    OtShipDefinition ship;
    OtPowerDefinition generator;
    OtShieldDefinition shield;
    u8 front = frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id;
    u8 rear = frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id;

    switch (part) {
    case FRONTEND_SHIP_DRAW_BODY:
        if (
            ot_data_hdt_ship_read(frontend_player_items.ship, &ship) &&
            ship.bigshipgraphic > 0
        ) {
            static const s16 ship_x[6] = {31, 0, 0, 0, 35, 31};
            static const s16 ship_y[6] = {36, 0, 0, 0, 33, 35};
            s16 sprite = (s16)ship.bigshipgraphic - 1;

            if (sprite >= 27 && sprite <= 32) {
                (void)frontend_blit_shp_source(
                    frame,
                    5,
                    (u16)sprite,
                    ship_x[sprite - 27],
                    ship_y[sprite - 27],
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        }
        break;

    case FRONTEND_SHIP_DRAW_GENERATOR:
        if (
            frontend_player_items.generator > 0 &&
            frontend_player_items.generator < 7 &&
            ot_data_hdt_power_read(
                frontend_player_items.generator,
                &generator
            )
        ) {
            static const s16 generator_x[5] = {
                62, 64, 67, 66, 63
            };
            static const s16 generator_y[5] = {
                84, 85, 86, 84, 97
            };
            s16 sprite =
                frontend_player_items.generator == 1 ?
                    frontend_player_items.generator + 15 :
                    frontend_player_items.generator + 14;

            if (sprite >= 16 && sprite <= 20) {
                (void)frontend_blit_shp_source(
                    frame,
                    6,
                    (u16)sprite,
                    generator_x[sprite - 16],
                    generator_y[sprite - 16],
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        }
        break;

    case FRONTEND_SHIP_DRAW_FRONT_WEAPON:
        if (
            front < 43 &&
            weapon_sprite[front] >= 0 &&
            front_xy[front] >= 0
        ) {
            (void)frontend_blit_shp_source(
                frame,
                6,
                (u16)weapon_sprite[front],
                front_x[(u8)front_xy[front]],
                front_y[(u8)front_xy[front]],
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
        break;

    case FRONTEND_SHIP_DRAW_REAR_WEAPON:
        if (
            rear < 43 &&
            weapon_sprite[rear] >= 0 &&
            rear_xy[rear] >= 0
        ) {
            (void)frontend_blit_shp_source(
                frame,
                6,
                (u16)weapon_sprite[rear],
                rear_x[(u8)rear_xy[rear]],
                rear_y[(u8)rear_xy[rear]],
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
        break;

    case FRONTEND_SHIP_DRAW_LEFT_SIDEKICK:
        frontend_draw_item_source(
            frame,
            FRONTEND_UPGRADE_LEFT_SIDEKICK,
            frontend_player_items.sidekick[0],
            3,
            84
        );
        break;

    case FRONTEND_SHIP_DRAW_RIGHT_SIDEKICK:
        frontend_draw_item_source(
            frame,
            FRONTEND_UPGRADE_RIGHT_SIDEKICK,
            frontend_player_items.sidekick[1],
            129,
            84
        );
        break;

    case FRONTEND_SHIP_DRAW_SHIELD:
        if (
            ot_data_hdt_shield_read(
                frontend_player_items.shield,
                &shield
            )
        ) {
            (void)frontend_blit_shp_source(
                frame,
                5,
                26,
                28,
                23,
                FRONTEND_SOURCE_BLIT_HUE,
                15,
                (s8)shield.max_power - 10
            );
        }
        break;

    case FRONTEND_SHIP_DRAW_STATUS:
        {
            char cash[11];

            frontend_u32_string(player_cash, cash);
            frontend_draw_runtime_text_clipped(
                frame,
                cash,
                49,
                138,
                116,
                0xfbu
            );
        }
        if (
            ot_data_hdt_ship_read(
                frontend_player_items.ship,
                &ship
            )
        ) {
            frontend_draw_segment_bar_source(
                frame, 42, 152, 3, 14, ship.damage, 2, 13
            );
        }
        if (
            ot_data_hdt_shield_read(
                frontend_player_items.shield,
                &shield
            )
        ) {
            frontend_draw_segment_bar_source(
                frame,
                104,
                152,
                2,
                14,
                (u8)(shield.max_power * 2u),
                2,
                13
            );
        }
        break;

    default:
        break;
    }
}

static void frontend_draw_ship_illustration(u8 *frame)
{
    u8 part;

    for (part = 0; part < FRONTEND_SHIP_DRAW_PART_COUNT; part++) {
        frontend_draw_ship_illustration_part(frame, part);
    }
}

static u8 frontend_ship_panel_cache_matches(void)
{
    return
        frontend_ship_panel_cache_valid &&
        frontend_player_items_valid &&
        frontend_ship_panel_cache_cash == player_cash &&
        memcmp(
            &frontend_ship_panel_cache_items,
            &frontend_player_items,
            sizeof(frontend_player_items)
        ) == 0;
}

static void frontend_ship_panel_cache_capture_rows(
    const u8 *frame,
    u8 first_row,
    u8 row_count
)
{
    u8 row;

    if (frame == 0 || !frontend_player_items_valid) {
        frontend_ship_panel_cache_valid = 0;
        return;
    }
    if (first_row >= FRONTEND_SHIP_PANEL_CACHE_HEIGHT) return;
    if (
        first_row + row_count >
        FRONTEND_SHIP_PANEL_CACHE_HEIGHT
    ) {
        row_count = (u8)(
            FRONTEND_SHIP_PANEL_CACHE_HEIGHT - first_row
        );
    }
    for (row = 0; row < row_count; row++) {
        u8 source_row = (u8)(first_row + row);

        memcpy(
            frontend_ship_panel_cache +
                (u16)source_row * FRONTEND_SHIP_PANEL_CACHE_WIDTH,
            frame + (u32)source_row * SCREEN_WIDTH,
            FRONTEND_SHIP_PANEL_CACHE_WIDTH
        );
    }
}

static void frontend_ship_panel_cache_finalize(void)
{
    if (!frontend_player_items_valid) {
        frontend_ship_panel_cache_valid = 0;
        return;
    }
    frontend_ship_panel_cache_items = frontend_player_items;
    frontend_ship_panel_cache_cash = player_cash;
    frontend_ship_panel_cache_valid = 1;
}

static void frontend_ship_panel_cache_capture(const u8 *frame)
{
    frontend_ship_panel_cache_capture_rows(
        frame,
        0,
        FRONTEND_SHIP_PANEL_CACHE_HEIGHT
    );
    if (frame == 0 || !frontend_player_items_valid) return;
    frontend_ship_panel_cache_finalize();
}

static u8 frontend_ship_panel_cache_apply_rows(
    u8 *frame,
    u8 first_row,
    u8 row_count
)
{
    u8 row;

    if (frame == 0 || !frontend_ship_panel_cache_matches()) {
        return 0;
    }
    if (first_row >= FRONTEND_SHIP_PANEL_CACHE_HEIGHT) return 0;
    if (
        first_row + row_count >
        FRONTEND_SHIP_PANEL_CACHE_HEIGHT
    ) {
        row_count = (u8)(
            FRONTEND_SHIP_PANEL_CACHE_HEIGHT - first_row
        );
    }
    for (row = 0; row < row_count; row++) {
        u8 destination_row = (u8)(first_row + row);

        memcpy(
            frame + (u32)destination_row * SCREEN_WIDTH,
            frontend_ship_panel_cache +
                (u16)destination_row *
                    FRONTEND_SHIP_PANEL_CACHE_WIDTH,
            FRONTEND_SHIP_PANEL_CACHE_WIDTH
        );
    }
    return 1;
}

static u8 frontend_ship_panel_cache_apply(u8 *frame)
{
    return frontend_ship_panel_cache_apply_rows(
        frame,
        0,
        FRONTEND_SHIP_PANEL_CACHE_HEIGHT
    );
}
