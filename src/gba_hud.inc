/* Minimal in-play overlays retained by the GBA specification. */

static void render_cash_counter(void)
{
    u8 digits[OBJ_SCORE_DIGIT_COUNT];
    u8 count = 0;
    u8 digit;
    u32 value = player_cash;
    s16 x = CASH_COUNTER_X;

    /*
     * PC JE_inGameDisplays() renders player.cash at (30,175) on 320x200
     * using TINY_FONT, hue 2, brightness 4 and FULL_SHADE.  That point falls
     * below the 1:1 gameplay crop, so the deliberately retained minimal GBA
     * HUD relocates it to (22,140).  Gameplay/world coordinates never use
     * this exception.  There is no pickup icon and no five-digit cap in the
     * original display.
     */
    do {
        digits[count++] = value % 10;
        value /= 10;
    } while (value && count < sizeof(digits));

    while (count) {
        digit = digits[--count];
        put_sprite(
            x, CASH_COUNTER_Y,
            OBJ_TILE_SCORE_DIGITS + digit,
            OBJ_PAL_SCORE_DIGITS, ATTR1_SIZE_8, 0
        );
        x += cash_digit_advances[digit];
    }
}

static void render_pause_text(void)
{
    u8 index;
    s16 x = source_world_to_screen_x(120);
    s16 y = source_world_to_screen_y(90);

    /*
     * PC JE_pauseGame draws FONT_SHAPES at (120,90) on game_screen.  Apply
     * the same final 1:1 crop as world objects: (84,78).  Generated 8x12
     * glyphs retain the original hue-15, brightness -3 JE_dString palette.
     */
    for (index = 0; index < OBJ_PAUSE_GLYPH_COUNT; index++) {
        put_sprite_shaped(
            x, y,
            OBJ_TILE_PAUSE_TEXT + index * 2,
            OBJ_PAL_PAUSE_TEXT,
            ATTR0_TALL, ATTR1_SIZE_8,
            8, 16, 0
        );
        x += pause_glyph_advances[index];
    }
}

static void render_boss_bar(void)
{
    u8 index;
    u8 full_segments;
    u8 pc_width;
    u8 gba_width;
    s16 fill_x;

    /*
     * PC draw_boss_bar(): one bar is centred at x=155, y=7 and uses a
     * 51x6 backing.  Scale its 320x200 HUD position to 240x160, retaining
     * four-pixel fill precision with the generated half-segment tile.
     *
     * Draw fill first because lower OAM indices win same-priority OBJ ties.
     */
    pc_width = (u8)(
        boss_hp / 10u +
        (boss_hp + 5u) / 10u +
        1u
    );
    gba_width = (u8)((pc_width * 3u + 2u) / 4u);
    gba_width = (u8)((gba_width + 3u) & ~3u);
    if (gba_width > 40) gba_width = 40;
    if (gba_width < 4) gba_width = 4;
    fill_x = 116 - gba_width / 2;
    full_segments = gba_width >> 3;
    for (index = 0; index < full_segments; index++) {
        put_sprite(
            fill_x + ((u16)index << 3), 6,
            OBJ_TILE_BOSS_BAR + 1, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
    if (gba_width & 4) {
        put_sprite(
            fill_x + ((u16)full_segments << 3), 6,
            OBJ_TILE_BOSS_BAR + 2, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
    for (index = 0; index < 5; index++) {
        put_sprite(
            96 + ((u16)index << 3), 6,
            OBJ_TILE_BOSS_BAR, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
}
