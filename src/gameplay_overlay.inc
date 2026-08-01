/*
 * Hardware-native translation of the two PC framebuffer-only level layers:
 * backgrnd.c's 100-star field and mainint.c's in-level text/warning overlays.
 *
 * BG3 is otherwise unused by gameplay.  Sparse stars are rebuilt into at
 * most one transparent 4bpp tile per occupied 8x8 cell; event and warning
 * rows then replace stars in their cells.  This preserves source state and
 * avoids spending scarce OAM entries or redrawing a 240x160 bitmap.
 */

static u32 gameplay_overlay_colour_distance(u16 left, u16 right)
{
    s16 red = (s16)(left & 31u) - (s16)(right & 31u);
    s16 green =
        (s16)((left >> 5) & 31u) - (s16)((right >> 5) & 31u);
    s16 blue =
        (s16)((left >> 10) & 31u) - (s16)((right >> 10) & 31u);

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}

static u8 gameplay_overlay_best_nibble(
    u16 source,
    u8 bank,
    u32 *distance_out
)
{
    u8 nibble;
    u8 best = 1;
    u32 best_distance = 0xffffffffu;

    for (nibble = 1; nibble < 16; nibble++) {
        u32 distance = gameplay_overlay_colour_distance(
            source,
            BG_PALETTE[(u16)bank * 16u + nibble]
        );

        if (distance < best_distance) {
            best_distance = distance;
            best = nibble;
        }
    }
    if (distance_out != 0) *distance_out = best_distance;
    return best;
}

static u8 gameplay_overlay_best_colour(u16 source)
{
    u8 bank;
    u8 best_bank = 0;
    u8 best_nibble = 1;
    u32 best_distance = 0xffffffffu;

    for (bank = 0; bank < 16; bank++) {
        u32 distance;
        u8 nibble = gameplay_overlay_best_nibble(
            source,
            bank,
            &distance
        );

        if (distance < best_distance) {
            best_distance = distance;
            best_bank = bank;
            best_nibble = nibble;
        }
    }
    return (u8)((best_bank << 4) | best_nibble);
}

static u8 gameplay_overlay_best_shared_bank(
    const u16 *source,
    u8 count
)
{
    u8 bank;
    u8 best_bank = 0;
    u32 best_score = 0xffffffffu;

    for (bank = 0; bank < 16; bank++) {
        u8 index;
        u32 score = 0;

        for (index = 0; index < count; index++) {
            u32 distance;

            (void)gameplay_overlay_best_nibble(
                source[index],
                bank,
                &distance
            );
            score += distance;
        }
        if (score < best_score) {
            best_score = score;
            best_bank = bank;
        }
    }
    return best_bank;
}

static void gameplay_overlay_prepare_palette(void)
{
    OtDataView palette;
    u16 star_source[16];
    u16 event_source[2];
    u8 index;

    gameplay_overlay_star_bank = 0;
    gameplay_overlay_event_bank = 0;
    memset(gameplay_overlay_star_nibble, 1, 16);
    memset(gameplay_overlay_warning_colour, 1, 16);
    gameplay_overlay_event_nibble[0] = 1;
    gameplay_overlay_event_nibble[1] = 1;
    if (!ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        return;
    }

    for (index = 0; index < 16; index++) {
        star_source[index] = title_palette_colour(
            &palette,
            (u8)(0x90u + index)
        );
        gameplay_overlay_warning_colour[index] =
            gameplay_overlay_best_colour(
                title_palette_colour(
                    &palette,
                    (u8)(0x70u + index)
                )
            );
    }
    gameplay_overlay_star_bank = gameplay_overlay_best_shared_bank(
        star_source,
        16
    );
    for (index = 0; index < 16; index++) {
        gameplay_overlay_star_nibble[index] =
            gameplay_overlay_best_nibble(
                star_source[index],
                gameplay_overlay_star_bank,
                0
            );
    }

    /* JE_drawTextWindow(..., hue 0, brightness 4), plus '~' +4. */
    event_source[0] = title_palette_colour(&palette, 0x04);
    event_source[1] = title_palette_colour(&palette, 0x08);
    gameplay_overlay_event_bank = gameplay_overlay_best_shared_bank(
        event_source,
        2
    );
    gameplay_overlay_event_nibble[0] =
        gameplay_overlay_best_nibble(
            event_source[0],
            gameplay_overlay_event_bank,
            0
        );
    gameplay_overlay_event_nibble[1] =
        gameplay_overlay_best_nibble(
            event_source[1],
            gameplay_overlay_event_bank,
            0
        );
}

static void gameplay_overlay_reset(void)
{
    gameplay_overlay_tile_count = 0;
    gameplay_overlay_active = 0;
    gameplay_overlay_upload_pending = 0;
    gameplay_overlay_message_index = 0;
    gameplay_overlay_message_ticks = 0;
    gameplay_overlay_pickup_message_type = OT_PICKUP_MESSAGE_NONE;
    gameplay_overlay_pickup_message_item_id = 0;
    memset(gameplay_overlay_map, 0, sizeof(gameplay_overlay_map));
}

static void gameplay_overlay_logic_tick(void)
{
    if (gameplay_overlay_message_ticks == 0) return;
    gameplay_overlay_message_ticks--;
    if (gameplay_overlay_message_ticks == 0) {
        gameplay_overlay_message_index = 0;
        gameplay_overlay_pickup_message_type = OT_PICKUP_MESSAGE_NONE;
        gameplay_overlay_pickup_message_item_id = 0;
    }
}

static void gameplay_overlay_show_event(u8 event_index)
{
    if (event_index == 0 || event_index > 9) return;
    gameplay_overlay_message_index = event_index;
    gameplay_overlay_pickup_message_type = OT_PICKUP_MESSAGE_NONE;
    gameplay_overlay_pickup_message_item_id = 0;
    /* mainint.c JE_drawTextWindow assigns textErase=100. */
    gameplay_overlay_message_ticks = 100;
}

static void gameplay_overlay_show_initial_message(void)
{
    /* Index ten is a GBA adapter sentinel for source miscText[20]. */
    gameplay_overlay_message_index = 10;
    gameplay_overlay_pickup_message_type = OT_PICKUP_MESSAGE_NONE;
    gameplay_overlay_pickup_message_item_id = 0;
    gameplay_overlay_message_ticks = 100;
}

static void gameplay_overlay_show_pickup(u8 message_type, u8 item_id)
{
    if (message_type == OT_PICKUP_MESSAGE_NONE) return;
    gameplay_overlay_message_index = 0;
    gameplay_overlay_pickup_message_type = message_type;
    gameplay_overlay_pickup_message_item_id = item_id;
    gameplay_overlay_message_ticks = 100;
}

static u8 gameplay_overlay_allocate_tile(u16 cell, u8 bank)
{
    u8 tile;

    if (gameplay_overlay_map[cell] != 0) {
        return (u8)(gameplay_overlay_map[cell] & 0x03ffu);
    }
    if (
        gameplay_overlay_tile_count + 1u >=
            GAMEPLAY_OVERLAY_MAX_TILES
    ) {
        return 0;
    }
    tile = ++gameplay_overlay_tile_count;
    memset(gameplay_overlay_tiles + (u16)tile * 32u, 0, 32);
    gameplay_overlay_map[cell] = (u16)(
        tile | ((u16)bank << 12)
    );
    return tile;
}

static void gameplay_overlay_set_pixel(
    s16 x,
    s16 y,
    u8 bank,
    u8 nibble
)
{
    u16 cell;
    u8 tile;
    u8 *pixels;
    u8 pixel_offset;

    if (
        x < 0 || x >= SCREEN_WIDTH ||
        y < 0 || y >= SCREEN_HEIGHT ||
        nibble == 0
    ) {
        return;
    }
    cell = (u16)((y >> 3) * 32 + (x >> 3));
    tile = gameplay_overlay_allocate_tile(cell, bank);
    if (tile == 0) return;
    pixels = gameplay_overlay_tiles + (u16)tile * 32u;
    pixel_offset = (u8)(((y & 7) << 2) + ((x & 7) >> 1));
    if (x & 1) {
        pixels[pixel_offset] = (u8)(
            (pixels[pixel_offset] & 0x0fu) | (nibble << 4)
        );
    } else {
        pixels[pixel_offset] = (u8)(
            (pixels[pixel_offset] & 0xf0u) | nibble
        );
    }
}

static void gameplay_overlay_clear_row(u8 row)
{
    memset(
        gameplay_overlay_map + (u16)row * 32u,
        0,
        32u * sizeof(u16)
    );
}

static u8 gameplay_overlay_custom_glyph_row(char character, u8 row)
{
    static const u8 star[8] = {
        0x00, 0x04, 0x15, 0x0e, 0x15, 0x04, 0x00, 0x00,
    };
    static const u8 less_than[8] = {
        0x00, 0x02, 0x04, 0x08, 0x04, 0x02, 0x00, 0x00,
    };
    static const u8 greater_than[8] = {
        0x00, 0x08, 0x04, 0x02, 0x04, 0x08, 0x00, 0x00,
    };

    if (character == '*') return star[row];
    if (character == '<') return less_than[row];
    return greater_than[row];
}

static u8 gameplay_overlay_glyph_width(char character, u8 *tile_out)
{
    u8 tile;

    if (character == ' ') {
        *tile_out = 0;
        return 0;
    }
    if (
        character == '*' ||
        character == '<' ||
        character == '>'
    ) {
        *tile_out = 0xff;
        return 5;
    }
    tile = frontend_pregame_font_tile(character);
    *tile_out = tile;
    return frontend_pregame_font_width(tile);
}

static void gameplay_overlay_draw_glyph(
    char character,
    s16 x,
    s16 y,
    u8 bank,
    u8 nibble
)
{
    u8 tile;
    u8 width = gameplay_overlay_glyph_width(character, &tile);
    u8 row;

    if (width == 0) return;
    for (row = 0; row < 8; row++) {
        u8 bits = tile == 0xff ?
            gameplay_overlay_custom_glyph_row(character, row) :
            frontend_pregame_font_row(tile, row);
        u8 column;

        for (column = 0; column < width; column++) {
            if (bits & (1u << (width - column - 1u))) {
                gameplay_overlay_set_pixel(
                    (s16)(x + column),
                    (s16)(y + row),
                    bank,
                    nibble
                );
            }
        }
    }
}

static s16 gameplay_overlay_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        u8 tile;
        u8 glyph_width;

        if (*text == '~') {
            text++;
            continue;
        }
        glyph_width = gameplay_overlay_glyph_width(*text, &tile);
        width = (s16)(
            width +
            (*text == ' ' ? 6 : glyph_width + 1)
        );
        text++;
    }
    return width > 0 ? (s16)(width - 1) : 0;
}

static void gameplay_overlay_draw_text(
    const char *text,
    s16 x,
    s16 y,
    u8 bank,
    u8 normal_nibble,
    u8 bright_nibble
)
{
    u8 bright = 0;

    while (text != 0 && *text != '\0') {
        u8 tile;
        u8 width;

        if (*text == '~') {
            bright ^= 1u;
            text++;
            continue;
        }
        width = gameplay_overlay_glyph_width(*text, &tile);
        if (*text == ' ') {
            x = (s16)(x + 6);
        } else {
            gameplay_overlay_draw_glyph(
                *text,
                x,
                y,
                bank,
                bright ? bright_nibble : normal_nibble
            );
            x = (s16)(x + width + 1);
        }
        text++;
    }
}

static void gameplay_overlay_draw_starfield(void)
{
    int16_t index;

    if (!source_parity_level.star_active) return;
    for (index = OT_STARFIELD_STAR_COUNT - 1; index >= 0; index--) {
        u16 position = source_parity_level.star_position[index];
        s16 source_x;
        s16 source_y;
        s16 x;
        s16 y;
        u8 colour;
        u8 nibble;

        if (position >= 177u * 320u) continue;
        source_x = (s16)(position % 320u);
        source_y = (s16)(position / 320u);
        x = (s16)(source_x - SOURCE_PRESENTATION_X_ORIGIN);
        y = (s16)(source_y - SOURCE_PRESENTATION_Y_ORIGIN);
        if (
            x < 0 || x >= SCREEN_WIDTH ||
            y < 0 || y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        /* Text is authored above the starfield; never merge palette banks. */
        if (gameplay_overlay_map[(u16)(y >> 3) * 32u + (u16)(x >> 3)]) {
            continue;
        }
        colour = (u8)(source_parity_level.star_meta[index] >> 4);
        nibble = gameplay_overlay_star_nibble[colour];
        gameplay_overlay_set_pixel(
            x,
            y,
            gameplay_overlay_star_bank,
            nibble
        );
        if (colour >= 4) {
            u8 dim = gameplay_overlay_star_nibble[colour - 4u];

            /* Keep the PC cross inside one sparse cell: no extra OAM/tiles. */
            if ((x & 7) != 0) {
                gameplay_overlay_set_pixel(
                    (s16)(x - 1), y,
                    gameplay_overlay_star_bank, dim
                );
            }
            if ((x & 7) != 7) {
                gameplay_overlay_set_pixel(
                    (s16)(x + 1), y,
                    gameplay_overlay_star_bank, dim
                );
            }
            if ((y & 7) != 0) {
                gameplay_overlay_set_pixel(
                    x, (s16)(y - 1),
                    gameplay_overlay_star_bank, dim
                );
            }
            if ((y & 7) != 7) {
                gameplay_overlay_set_pixel(
                    x, (s16)(y + 1),
                    gameplay_overlay_star_bank, dim
                );
            }
        }
    }
}

static void gameplay_overlay_draw_event_message(void)
{
    const char *text = 0;
    const char *item_name = 0;
    union {
        OtWeaponPortDefinition port;
        OtSpecialDefinition special;
        OtOptionDefinition option;
    } item;
    s16 x = 4;

    if (
        gameplay_overlay_message_ticks == 0 ||
        (
            gameplay_overlay_message_index == 0 &&
            gameplay_overlay_pickup_message_type ==
                OT_PICKUP_MESSAGE_NONE
        ) ||
        !frontend_text_ready
    ) {
        return;
    }
    if (
        gameplay_overlay_pickup_message_type ==
            OT_PICKUP_MESSAGE_FRONT_POWER
    ) {
        text = frontend_text.misc_text[44];
    } else if (
        gameplay_overlay_pickup_message_type ==
            OT_PICKUP_MESSAGE_REAR_POWER
    ) {
        text = frontend_text.misc_text[45];
    } else if (
        gameplay_overlay_pickup_message_type ==
            OT_PICKUP_MESSAGE_WEAPON_PORT &&
        ot_data_hdt_weapon_port_read(
            gameplay_overlay_pickup_message_item_id,
            &item.port
        )
    ) {
        text = frontend_text.misc_text[63];
        item_name = item.port.name;
    } else if (
        gameplay_overlay_pickup_message_type ==
            OT_PICKUP_MESSAGE_SPECIAL &&
        ot_data_hdt_special_read(
            gameplay_overlay_pickup_message_item_id,
            &item.special
        )
    ) {
        text = frontend_text.misc_text[63];
        item_name = item.special.name;
    } else if (
        gameplay_overlay_pickup_message_type ==
            OT_PICKUP_MESSAGE_OPTION &&
        ot_data_hdt_option_read(
            gameplay_overlay_pickup_message_item_id,
            &item.option
        )
    ) {
        text = frontend_text.misc_text[63];
        item_name = item.option.name;
    } else if (gameplay_overlay_message_index != 0) {
        text = gameplay_overlay_message_index == 10 ?
            frontend_text.misc_text[20] :
            frontend_text.event_text[
                gameplay_overlay_message_index - 1u
            ];
    }
    if (text == 0) return;
    if (text[0] == '\0') return;
    gameplay_overlay_clear_row(GAMEPLAY_OVERLAY_MAP_ROWS - 1u);
    gameplay_overlay_draw_text(
        text,
        x,
        SCREEN_HEIGHT - 8,
        gameplay_overlay_event_bank,
        gameplay_overlay_event_nibble[0],
        gameplay_overlay_event_nibble[1]
    );
    if (item_name != 0 && item_name[0] != '\0') {
        x = (s16)(x + gameplay_overlay_text_width(text) + 6);
        gameplay_overlay_draw_text(
            item_name,
            x,
            SCREEN_HEIGHT - 8,
            gameplay_overlay_event_bank,
            gameplay_overlay_event_nibble[0],
            gameplay_overlay_event_nibble[1]
        );
    }
}

static void gameplay_overlay_draw_level_timer(void)
{
    char value[8];
    const char *label;
    u16 countdown;
    u16 seconds;
    u8 tenths;
    u8 colour;
    s16 label_x;
    s16 value_x;

    if (
        !source_parity_level.level_timer ||
        source_parity_level.level_timer_countdown == 0
    ) {
        return;
    }
    countdown = source_parity_level.level_timer_countdown;
    seconds = countdown / 100u;
    tenths = (u8)((countdown % 100u) / 10u);
    if (seconds >= 1000u) seconds = 999u;
    if (seconds >= 100u) {
        value[0] = (char)('0' + seconds / 100u);
        value[1] = (char)('0' + (seconds / 10u) % 10u);
        value[2] = (char)('0' + seconds % 10u);
        value[3] = '.';
        value[4] = (char)('0' + tenths);
        value[5] = '\0';
    } else if (seconds >= 10u) {
        value[0] = (char)('0' + seconds / 10u);
        value[1] = (char)('0' + seconds % 10u);
        value[2] = '.';
        value[3] = (char)('0' + tenths);
        value[4] = '\0';
    } else {
        value[0] = (char)('0' + seconds);
        value[1] = '.';
        value[2] = (char)('0' + tenths);
        value[3] = '\0';
    }
    label =
        frontend_text_ready && frontend_text.misc_text[66][0] != '\0' ?
            frontend_text.misc_text[66] :
            "Timer";
    colour = gameplay_overlay_warning_colour[
        (u8)((countdown % 20u) / 3u)
    ];
    gameplay_overlay_clear_row(0);
    label_x = (s16)(SCREEN_WIDTH / 2 - 4 - gameplay_overlay_text_width(label));
    value_x = (s16)(SCREEN_WIDTH / 2 + 4);
    gameplay_overlay_draw_text(
        label,
        label_x,
        0,
        colour >> 4,
        colour & 0x0f,
        colour & 0x0f
    );
    gameplay_overlay_draw_text(
        value,
        value_x,
        0,
        colour >> 4,
        colour & 0x0f,
        colour & 0x0f
    );
}

static void gameplay_overlay_draw_warning(void)
{
    static const char warning[] = "WARNING";
    u8 colour;
    s16 x;

    if (!source_parity_level.warning_active) return;
    colour = gameplay_overlay_warning_colour[
        (u8)source_parity_level.warning_color & 0x0fu
    ];
    gameplay_overlay_clear_row(1);
    x = (s16)(
        (SCREEN_WIDTH - gameplay_overlay_text_width(warning)) / 2
    );
    gameplay_overlay_draw_text(
        warning,
        x,
        8,
        colour >> 4,
        colour & 0x0f,
        colour & 0x0f
    );
}

static void gameplay_overlay_draw_zinglon(void)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    s16 width;
    s16 centre;
    s16 line[6];
    u8 line_index;

    if (source_special_zinglon_duration <= 1) return;
    width = (s16)(
        25 -
        (
            source_special_zinglon_duration > 25 ?
                source_special_zinglon_duration - 25 :
                25 - source_special_zinglon_duration
        )
    );
    if (width <= 0) return;
    centre = source_world_to_screen_x((s16)(player_source_x + 7));
    line[0] = (s16)(centre - width - 2);
    line[1] = (s16)(centre - width);
    line[2] = (s16)(centre - 1);
    line[3] = (s16)(centre + 1);
    line[4] = (s16)(centre + width);
    line[5] = (s16)(centre + width + 2);
    for (line_index = 0; line_index < 6; line_index++) {
        s16 y;

        for (y = 0; y < SCREEN_HEIGHT; y++) {
            gameplay_overlay_set_pixel(
                line[line_index],
                y,
                gameplay_overlay_event_bank,
                gameplay_overlay_event_nibble[
                    line_index == 0 || line_index == 5 ? 0 : 1
                ]
            );
        }
    }
#endif
}

static void gameplay_overlay_prepare_frame(void)
{
    gameplay_overlay_active = (u8)(
        source_parity_level.star_active ||
        source_parity_level.warning_active ||
        gameplay_overlay_message_ticks != 0 ||
        (
            source_parity_level.level_timer &&
            source_parity_level.level_timer_countdown != 0
        )
#if !TYRIAN_GBA_STRESS_LOADOUT
        || source_special_zinglon_duration > 1
#endif
    );
    if (!gameplay_overlay_active) {
        gameplay_overlay_upload_pending = 0;
        return;
    }
    gameplay_overlay_tile_count = 0;
    memset(gameplay_overlay_tiles, 0, 32);
    memset(gameplay_overlay_map, 0, sizeof(gameplay_overlay_map));
    /* Allocate gameplay-critical text first; stars use only remaining cells. */
    gameplay_overlay_draw_event_message();
    gameplay_overlay_draw_warning();
    gameplay_overlay_draw_level_timer();
    gameplay_overlay_draw_zinglon();
    gameplay_overlay_draw_starfield();
    gameplay_overlay_upload_pending = 1;
}
