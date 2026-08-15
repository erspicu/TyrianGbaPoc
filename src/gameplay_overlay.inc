/*
 * Hardware-native translation of the two PC framebuffer-only level layers:
 * backgrnd.c's 100-star field and mainint.c's in-level text/warning overlays.
 *
 * BG3 is otherwise unused by gameplay.  Sparse stars are rebuilt into at
 * most one transparent 4bpp tile per occupied 8x8 cell; event and warning
 * rows then replace stars in their cells.  This preserves source state and
 * avoids spending scarce OAM entries or redrawing a 240x160 bitmap.
 */

static ARM_CODE __attribute__((noinline, noclone)) u32
gameplay_overlay_colour_distance_c_reference(u16 left, u16 right)
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

#if TYRIAN_GBA_HOTPATH_ASM
extern u32 gameplay_overlay_colour_distance(u16 left, u16 right);
#else
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
#endif

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

static u8 gameplay_overlay_pause_coherent_nibble(u8 bank, u8 nibble)
{
    u16 colour = BG_PALETTE[(u16)bank * 16u + nibble];
    u8 red = (u8)(colour & 31u);
    u8 green = (u8)((colour >> 5) & 31u);
    u8 blue = (u8)((colour >> 10) & 31u);
    u16 target_luma;
    u16 best_delta = UINT16_MAX;
    u8 best = nibble;
    u8 candidate;

    /*
     * JE_dString() deliberately calls blit_sprite_hv_unsafe() with value -3.
     * Very dark FONT_SHAPES pixels therefore wrap through 0xfe/0xff in the
     * original 8-bit palette.  On the PC this is a tiny hue-15 accent, but a
     * trained mixed-material GBA BG bank can turn those few pixels into vivid
     * red dots inside otherwise brown PAUSED lettering.
     *
     * Keep the selected bank and its accepted brown appearance.  Only when a
     * mapped slot is a strongly red outlier, replace it with a warm-neutral
     * slot of similar luminance from that same bank.  No palette RAM is
     * overwritten, so the world background remains completely untouched.
     */
    if (red <= (u8)(green * 2u)) return nibble;

    target_luma = (u16)(red * 3u + green * 6u + blue);
    for (candidate = 1; candidate < 16u; candidate++) {
        u16 candidate_colour =
            BG_PALETTE[(u16)bank * 16u + candidate];
        u8 candidate_red = (u8)(candidate_colour & 31u);
        u8 candidate_green =
            (u8)((candidate_colour >> 5) & 31u);
        u8 candidate_blue =
            (u8)((candidate_colour >> 10) & 31u);
        u16 candidate_luma;
        u16 delta;

        if (
            candidate_red < candidate_green ||
            candidate_green < candidate_blue ||
            candidate_red > (u8)(candidate_green * 2u)
        ) {
            continue;
        }
        candidate_luma = (u16)(
            candidate_red * 3u +
            candidate_green * 6u +
            candidate_blue
        );
        delta = candidate_luma > target_luma ?
            (u16)(candidate_luma - target_luma) :
            (u16)(target_luma - candidate_luma);
        if (delta < best_delta) {
            best_delta = delta;
            best = candidate;
        }
    }
    return best;
}

static void gameplay_overlay_prepare_palette(void)
{
    OtDataView palette;
    u16 star_source[16];
    u16 event_source[2];
    u16 pause_source[15];
    u16 superpixel_source[16];
    u8 index;

    gameplay_overlay_star_bank = 0;
    gameplay_overlay_event_bank = 0;
    gameplay_overlay_pause_bank = 0;
    memset(gameplay_overlay_star_nibble, 1, 16);
    memset(gameplay_overlay_warning_colour, 1, 16);
    memset(gameplay_overlay_pause_nibble, 1, 16);
    gameplay_overlay_pause_nibble[0] = 0;
    memset(gameplay_overlay_superpixel_bank, 0, 16);
    memset(
        gameplay_overlay_superpixel_nibble,
        1,
        sizeof(gameplay_overlay_superpixel_nibble)
    );
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

    /*
     * GBA crop adaptation: the bottom notice shares the left edge with the
     * compact HUD, so use Tyrian's own clear-red hue ramp rather than the PC
     * window's low-contrast grey.  '~' markup keeps its brighter emphasis.
     */
    event_source[0] = title_palette_colour(&palette, 0x77);
    event_source[1] = title_palette_colour(&palette, 0x7b);
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

    /*
     * PAUSED used to be a 4bpp OBJ in bank 14.  The large-boss compact cache
     * legitimately time-shares that bank for its second trained palette, so
     * restoring the glyph tiles could not restore both palettes at once.
     * Keep the authored FONT_SHAPES colours, but map them into one existing
     * BG bank and draw the glyphs through the palette-independent BG3 overlay.
     */
    for (index = 1; index < 16; index++) {
        pause_source[index - 1u] =
            ((const u16 *)(const void *)obj_palette)[
                OBJ_PAL_PAUSE_TEXT * 16u + index
            ];
    }
    gameplay_overlay_pause_bank = gameplay_overlay_best_shared_bank(
        pause_source,
        15
    );
    for (index = 1; index < 16; index++) {
        gameplay_overlay_pause_nibble[index] =
            gameplay_overlay_pause_coherent_nibble(
                gameplay_overlay_pause_bank,
                gameplay_overlay_best_nibble(
                pause_source[index - 1u],
                gameplay_overlay_pause_bank,
                0
                )
            );
    }

    /*
     * JE_drawSP replaces the destination hue with color's high nibble and
     * derives brightness from the particle plus the covered pixel.  BG3 is
     * 4bpp, so choose one existing hardware bank per source hue and retain
     * all sixteen source brightness steps inside that bank.
     */
    for (index = 0; index < 16; index++) {
        u8 brightness;

        for (brightness = 0; brightness < 16; brightness++) {
            superpixel_source[brightness] = title_palette_colour(
                &palette,
                (u8)((index << 4) | brightness)
            );
        }
        gameplay_overlay_superpixel_bank[index] =
            gameplay_overlay_best_shared_bank(
                superpixel_source,
                16
            );
        for (brightness = 0; brightness < 16; brightness++) {
            gameplay_overlay_superpixel_nibble[index][brightness] =
                gameplay_overlay_best_nibble(
                    superpixel_source[brightness],
                    gameplay_overlay_superpixel_bank[index],
                    0
                );
        }
    }
}

static void gameplay_overlay_clear_map(void)
{
    u16 index;

    for (index = 0; index < GAMEPLAY_OVERLAY_MAP_ENTRIES; index++) {
        gameplay_overlay_map[index] =
            GAMEPLAY_OVERLAY_BLANK_TILE_INDEX;
    }
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
    gameplay_overlay_clear_map();
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

static u16 gameplay_overlay_hardware_tile(u8 tile)
{
    if (tile < GAMEPLAY_OVERLAY_TILE0_CAPACITY) {
        return (u16)(GAMEPLAY_OVERLAY_TILE0_INDEX_BASE + tile);
    }
    return (u16)(
        GAMEPLAY_OVERLAY_TILE1_INDEX_BASE +
        tile - GAMEPLAY_OVERLAY_TILE0_CAPACITY
    );
}

static u8 gameplay_overlay_local_tile(u16 hardware_tile)
{
    if (
        hardware_tile >= GAMEPLAY_OVERLAY_TILE0_INDEX_BASE &&
        hardware_tile <
            GAMEPLAY_OVERLAY_TILE0_INDEX_BASE +
                GAMEPLAY_OVERLAY_TILE0_CAPACITY
    ) {
        return (u8)(
            hardware_tile - GAMEPLAY_OVERLAY_TILE0_INDEX_BASE
        );
    }
    if (
        hardware_tile >= GAMEPLAY_OVERLAY_TILE1_INDEX_BASE &&
        hardware_tile <
            GAMEPLAY_OVERLAY_TILE1_INDEX_BASE +
                GAMEPLAY_OVERLAY_MAX_TILES -
                GAMEPLAY_OVERLAY_TILE0_CAPACITY
    ) {
        return (u8)(
            GAMEPLAY_OVERLAY_TILE0_CAPACITY +
            hardware_tile - GAMEPLAY_OVERLAY_TILE1_INDEX_BASE
        );
    }
    return 0;
}

static u8 gameplay_overlay_allocate_tile(u16 cell, u8 bank)
{
    u16 hardware_tile = (u16)(gameplay_overlay_map[cell] & 0x03ffu);
    u8 tile;

    if (hardware_tile != GAMEPLAY_OVERLAY_BLANK_TILE_INDEX) {
        return gameplay_overlay_local_tile(hardware_tile);
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
        gameplay_overlay_hardware_tile(tile) |
        ((u16)bank << 12)
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

static void gameplay_overlay_draw_pause(void)
{
    s16 glyph_x = TYRIAN_GBA_LAYOUT_PAUSED_X;
    u8 glyph;

    if (!game_paused) return;
#ifdef AUTOTEST_SCREENSHOT_REWARD
    if (autotest_screenshot_delay) return;
#endif
    for (glyph = 0; glyph < OBJ_PAUSE_GLYPH_COUNT; glyph++) {
        const u8 *tiles = obj_tiles +
            (u32)(OBJ_TILE_PAUSE_TEXT + glyph * 2u) * 32u;
        u8 row;

        for (row = 0; row < 16u; row++) {
            const u8 *tile_row = tiles +
                (u16)(row >> 3) * 32u +
                (u16)(row & 7u) * 4u;
            u8 column;

            for (column = 0; column < 8u; column++) {
                u8 packed = tile_row[column >> 1];
                u8 source_nibble = (column & 1u) ?
                    (u8)(packed >> 4) :
                    (u8)(packed & 0x0fu);

                if (source_nibble != 0) {
                    gameplay_overlay_set_pixel(
                        (s16)(glyph_x + column),
                        (s16)(TYRIAN_GBA_LAYOUT_PAUSED_Y + row),
                        gameplay_overlay_pause_bank,
                        gameplay_overlay_pause_nibble[source_nibble]
                    );
                }
            }
        }
        glyph_x = (s16)(glyph_x + pause_glyph_advances[glyph]);
    }
}

static void gameplay_overlay_clear_row(u8 row)
{
    u8 column;

    for (column = 0; column < 32u; column++) {
        gameplay_overlay_map[(u16)row * 32u + column] =
            GAMEPLAY_OVERLAY_BLANK_TILE_INDEX;
    }
}

static u8 gameplay_overlay_cell_occupied(u16 cell)
{
    return (u8)(
        (gameplay_overlay_map[cell] & 0x03ffu) !=
            GAMEPLAY_OVERLAY_BLANK_TILE_INDEX
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

/*
 * In-level notices share the thin 4x8 mixed-case face used by the clear
 * episode-scene reference.  The old notice path drew the full-width 6x8
 * pre-game face, which made adjacent strokes merge in the bottom row.
 */
static u8 gameplay_overlay_fine_glyph_width(
    char character,
    u8 *tile_out
)
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
    return frontend_small_mixed_font_width(tile);
}

static void gameplay_overlay_draw_fine_glyph(
    char character,
    s16 x,
    s16 y,
    u8 bank,
    u8 nibble
)
{
    u8 tile;
    u8 target_width = gameplay_overlay_fine_glyph_width(
        character,
        &tile
    );
    u8 source_width = tile == 0xff ?
        target_width :
        frontend_pregame_font_width(tile);
    u8 row;

    if (target_width == 0 || source_width == 0) return;
    for (row = 0; row < FRONTEND_PREGAME_FONT_HEIGHT; row++) {
        u8 bits = tile == 0xff ?
            gameplay_overlay_custom_glyph_row(character, row) :
            frontend_pregame_font_row(tile, row);
        u8 column;

        for (column = 0; column < target_width; column++) {
            u8 source_column = tile == 0xff ?
                column :
                frontend_small_mixed_source_column(
                    source_width,
                    target_width,
                    column
                );

            if (
                bits &
                    (1u << (source_width - source_column - 1u))
            ) {
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

static s16 gameplay_overlay_fine_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        u8 tile;
        u8 glyph_width;

        if (*text == '~') {
            text++;
            continue;
        }
        glyph_width = gameplay_overlay_fine_glyph_width(*text, &tile);
        width = (s16)(
            width +
            (
                *text == ' ' ?
                    frontend_small_mixed_font_advance(0) :
                    glyph_width + 1u
            )
        );
        text++;
    }
    return width;
}

static void gameplay_overlay_draw_fine_text(
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
        width = gameplay_overlay_fine_glyph_width(*text, &tile);
        if (*text == ' ') {
            x = (s16)(
                x + frontend_small_mixed_font_advance(0)
            );
        } else {
            gameplay_overlay_draw_fine_glyph(
                *text,
                x,
                y,
                bank,
                bright ? bright_nibble : normal_nibble
            );
            x = (s16)(x + width + 1u);
        }
        text++;
    }
}

#if TYRIAN_GBA_STARFIELD_BATCH_ASM || \
    defined(AUTOTEST_FULL_LOADOUT_STRESS)
extern IWRAM_CODE ARM_CODE u32 gameplay_overlay_divmod320_asm(
    u16 position
);
extern IWRAM_CODE ARM_CODE void gameplay_overlay_plot_star_tile_asm(
    u8 *tile,
    u8 local_x,
    u8 local_y,
    u32 packed_nibbles
);
#endif

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static ARM_CODE __attribute__((noinline, noclone, noipa)) u32
gameplay_overlay_divmod320_c_reference(u16 position)
{
    u16 source_y = (u16)(position / 320u);
    u16 source_x = (u16)(position % 320u);

    return (u32)source_x | ((u32)source_y << 16);
}

static inline void gameplay_overlay_plot_star_pixel_c_reference(
    u8 *tile,
    u8 local_x,
    u8 local_y,
    u8 nibble
)
{
    u8 *pixel = tile + (u16)local_y * 4u + (local_x >> 1);

    if (local_x & 1u) {
        *pixel = (u8)((*pixel & 0x0fu) | (nibble << 4));
    } else {
        *pixel = (u8)((*pixel & 0xf0u) | nibble);
    }
}

static ARM_CODE __attribute__((noinline, noclone, noipa)) void
gameplay_overlay_plot_star_tile_c_reference(
    u8 *tile,
    u8 local_x,
    u8 local_y,
    u32 packed_nibbles
)
{
    u8 centre = (u8)packed_nibbles;
    u8 dim = (u8)(packed_nibbles >> 8);

    gameplay_overlay_plot_star_pixel_c_reference(
        tile,
        local_x,
        local_y,
        centre
    );
    if (dim == 0) return;
    if (local_x != 0) {
        gameplay_overlay_plot_star_pixel_c_reference(
            tile,
            (u8)(local_x - 1u),
            local_y,
            dim
        );
    }
    if (local_x != 7u) {
        gameplay_overlay_plot_star_pixel_c_reference(
            tile,
            (u8)(local_x + 1u),
            local_y,
            dim
        );
    }
    if (local_y != 0) {
        gameplay_overlay_plot_star_pixel_c_reference(
            tile,
            local_x,
            (u8)(local_y - 1u),
            dim
        );
    }
    if (local_y != 7u) {
        gameplay_overlay_plot_star_pixel_c_reference(
            tile,
            local_x,
            (u8)(local_y + 1u),
            dim
        );
    }
}
#endif

static void gameplay_overlay_draw_starfield(void)
{
    int16_t index;

#if !defined(AUTOTEST_FORCE_OVERLAY_STARFIELD)
    if (
        !source_parity_level.star_active &&
        !source_special_astral_visual_active
    ) {
        return;
    }
#endif
    for (index = OT_STARFIELD_STAR_COUNT - 1; index >= 0; index--) {
        u16 position = source_parity_level.star_position[index];
        s16 source_x;
        s16 source_y;
        s16 x;
        s16 y;
        u8 colour;
        u8 nibble;

        if (position >= 177u * 320u) continue;
#if TYRIAN_GBA_STARFIELD_BATCH_ASM
        {
            u32 source_position =
                gameplay_overlay_divmod320_asm(position);

            source_x = (s16)(u16)source_position;
            source_y = (s16)(u16)(source_position >> 16);
        }
#else
        source_x = (s16)(position % 320u);
        source_y = (s16)(position / 320u);
#endif
        x = (s16)(source_x - SOURCE_PRESENTATION_X_ORIGIN);
        y = (s16)(source_y - SOURCE_PRESENTATION_Y_ORIGIN);
        if (
            x < 0 || x >= SCREEN_WIDTH ||
            y < 0 || y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        /* Text is authored above the starfield; never merge palette banks. */
        if (gameplay_overlay_cell_occupied(
                (u16)(y >> 3) * 32u + (u16)(x >> 3)
            )) {
            continue;
        }
        colour = (u8)(source_parity_level.star_meta[index] >> 4);
        nibble = gameplay_overlay_star_nibble[colour];
#if TYRIAN_GBA_STARFIELD_BATCH_ASM
        {
            u8 tile = gameplay_overlay_allocate_tile(
                (u16)(y >> 3) * 32u + (u16)(x >> 3),
                gameplay_overlay_star_bank
            );
            u8 dim = colour >= 4u ?
                gameplay_overlay_star_nibble[colour - 4u] :
                0;

            if (tile == 0) continue;
            gameplay_overlay_plot_star_tile_asm(
                gameplay_overlay_tiles + (u16)tile * 32u,
                (u8)x & 7u,
                (u8)y & 7u,
                (u32)nibble | ((u32)dim << 8)
            );
        }
#else
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
#endif
    }
}

static void gameplay_overlay_set_superpixel(
    s16 x,
    s16 y,
    u8 hue,
    u8 brightness
)
{
    u16 cell;
    u8 bank = gameplay_overlay_superpixel_bank[hue & 0x0fu];

    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }
    cell = (u16)((y >> 3) * 32 + (x >> 3));
    if (
        gameplay_overlay_cell_occupied(cell) &&
        (
            game_paused ||
            (u8)(gameplay_overlay_map[cell] >> 12) != bank
        )
    ) {
        /* Text keeps priority; a 4bpp cell cannot select two banks. */
        return;
    }
    gameplay_overlay_set_pixel(
        x,
        y,
        bank,
        gameplay_overlay_superpixel_nibble[hue & 0x0fu][
            brightness & 0x0fu
        ]
    );
}

static void gameplay_overlay_draw_superpixels(void)
{
    s16 index;
    u8 presentation_budget = MAX_SUPERPIXELS;

    if (source_active_superpixels == 0) return;
    /*
     * JE_doSP state and its shared RNG stream remain exact.  At a saturated
     * projectile pool, limiting only the sparse BG3 presentation prevents
     * 101 particles x five pixel writes from competing with Boss/projectile
     * rendering.  Hit feedback remains dense; normal loads stay uncapped.
     */
    if (
        active_player_shot_count > 48u &&
        source_active_superpixels > 48u
    ) {
        presentation_budget = 48u;
    }
    for (index = MAX_SUPERPIXELS - 1; index >= 0; index--) {
        const SourceSuperpixel *pixel = &source_superpixels[index];
        s16 x;
        s16 y;
        u8 hue;
        u8 centre;
        u8 edge;

        if (pixel->z == 0) continue;
        x = source_world_to_screen_x(pixel->x);
        y = source_world_to_screen_y(pixel->y);
        hue = (u8)(pixel->color >> 4);
        /* Destination brightness is unknown to sparse BG3; use its midpoint. */
        centre = (u8)((8u + pixel->z) >> 1);
        edge = (u8)((8u + (pixel->z >> 1)) >> 1);
        gameplay_overlay_set_superpixel(x, y, hue, centre);
        gameplay_overlay_set_superpixel((s16)(x - 1), y, hue, edge);
        gameplay_overlay_set_superpixel((s16)(x + 1), y, hue, edge);
        gameplay_overlay_set_superpixel(x, (s16)(y - 1), hue, edge);
        gameplay_overlay_set_superpixel(x, (s16)(y + 1), hue, edge);
        if (--presentation_budget == 0) break;
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
    gameplay_overlay_draw_fine_text(
        text,
        x,
        SCREEN_HEIGHT - 8,
        gameplay_overlay_event_bank,
        gameplay_overlay_event_nibble[0],
        gameplay_overlay_event_nibble[1]
    );
    if (item_name != 0 && item_name[0] != '\0') {
        x = (s16)(x + gameplay_overlay_fine_text_width(text) + 6);
        gameplay_overlay_draw_fine_text(
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

static void gameplay_overlay_prepare_frame(void)
{
    gameplay_overlay_active = (u8)(
#if defined(AUTOTEST_FORCE_OVERLAY_STARFIELD)
        1 ||
#endif
        source_parity_level.star_active ||
        source_special_astral_visual_active ||
        source_active_superpixels != 0 ||
        game_paused ||
        source_parity_level.warning_active ||
        gameplay_overlay_message_ticks != 0 ||
        (
            source_parity_level.level_timer &&
            source_parity_level.level_timer_countdown != 0
        )
    );
    if (!gameplay_overlay_active) {
        gameplay_overlay_upload_pending = 0;
        return;
    }
    gameplay_overlay_tile_count = 0;
    memset(gameplay_overlay_tiles, 0, 32);
    gameplay_overlay_clear_map();
    /* Allocate gameplay-critical text first; stars use only remaining cells. */
    gameplay_overlay_draw_event_message();
    gameplay_overlay_draw_warning();
    gameplay_overlay_draw_level_timer();
    gameplay_overlay_draw_pause();
    gameplay_overlay_draw_superpixels();
    gameplay_overlay_draw_starfield();
    gameplay_overlay_upload_pending = 1;
}
