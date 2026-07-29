/* Minimal in-play overlays retained by the GBA specification. */

static void render_cash_counter(void)
{
    u8 digits[OBJ_SCORE_DIGIT_COUNT];
    u8 count = 0;
    u8 digit;
    u32 value = player_cash;
    s16 x = TYRIAN_GBA_LAYOUT_CASH_X;

    /*
     * PC JE_inGameDisplays() renders player.cash at (30,175) on 320x200
     * using TINY_FONT, hue 2, brightness 4 and FULL_SHADE.  That point falls
     * below the 1:1 gameplay crop.  With the old bottom banner removed, the
     * retained minimal GBA HUD places it at (22,148), eight pixels below
     * the prior adapter position. Gameplay/world coordinates never use this
     * exception. There is no pickup icon or five-digit cap in the source.
     */
    do {
        digits[count++] = value % 10;
        value /= 10;
    } while (value && count < sizeof(digits));

    while (count) {
        digit = digits[--count];
        put_sprite(
            x, TYRIAN_GBA_LAYOUT_CASH_Y,
            OBJ_TILE_SCORE_DIGITS + digit,
            OBJ_PAL_SCORE_DIGITS, ATTR1_SIZE_8, 0
        );
        x += cash_digit_advances[digit];
    }
}

static inline void put_energy_hud_digit(s16 x, s16 y, u8 digit)
{
    OBJATTR *object;

    /*
     * Configure.h statically keeps these rows on-screen.  Avoid the generic
     * sprite helper's size and clipping branches in this per-frame hot path;
     * only the hardware OAM guard remains necessary.
     */
    if (oam_count >= SPRITE_LIMIT) return;
    object = &oam_shadow[oam_count++];
    object->attr0 =
        (y & 0x00ff) | ATTR0_NORMAL | ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | ATTR1_SIZE_8;
    object->attr2 =
        OBJ_CHAR(OBJ_TILE_SCORE_DIGITS + digit) |
        ATTR2_PRIORITY(0) |
        ATTR2_PALETTE(OBJ_PAL_SCORE_DIGITS);
    object->dummy = 0;
}

typedef struct {
    u16 value;
    s16 x;
    u8 digits[3];
    u8 count;
    u8 valid;
} EnergyHudDecimalCache;

static EnergyHudDecimalCache energy_hud_decimal_cache[3];

static void render_hud_value_right(
    EnergyHudDecimalCache *cache,
    u16 value,
    s16 right_x,
    s16 y
)
{
    u8 hundreds = 0;
    u8 tens = 0;
    u8 index;
    s16 x;

    /*
     * These three source values are bounded by 900/255/255.  Avoid / and %
     * here: ARM7TDMI has no divider and GCC otherwise emits several
     * __aeabi_uidivmod calls.  Armor/shield change rarely and presentation
     * may render more often than the 30 Hz logic clock, so retain the
     * formatted digits until the underlying source value changes.
     */
    if (cache->valid && cache->value == value) {
        x = cache->x;
        for (index = 0; index < cache->count; index++) {
            u8 digit = cache->digits[index];

            put_energy_hud_digit(x, y, digit);
            x = (s16)(x + cash_digit_advances[digit]);
        }
        return;
    }
    if (value > 999) value = 999;
    cache->value = value;
    if (value >= 500) {
        value = (u16)(value - 500);
        hundreds = 5;
    }
    if (value >= 300) {
        value = (u16)(value - 300);
        hundreds = (u8)(hundreds + 3);
    }
    if (value >= 200) {
        value = (u16)(value - 200);
        hundreds = (u8)(hundreds + 2);
    }
    if (value >= 100) {
        value = (u16)(value - 100);
        hundreds++;
    }
    if (value >= 50) {
        value = (u16)(value - 50);
        tens = 5;
    }
    if (value >= 30) {
        value = (u16)(value - 30);
        tens = (u8)(tens + 3);
    }
    if (value >= 20) {
        value = (u16)(value - 20);
        tens = (u8)(tens + 2);
    }
    if (value >= 10) {
        value = (u16)(value - 10);
        tens++;
    }
    if (hundreds != 0) {
        cache->digits[0] = hundreds;
        cache->digits[1] = tens;
        cache->digits[2] = (u8)value;
        cache->count = 3;
    } else if (tens != 0) {
        cache->digits[0] = tens;
        cache->digits[1] = (u8)value;
        cache->count = 2;
    } else {
        cache->digits[0] = (u8)value;
        cache->count = 1;
    }

    x = right_x;
    for (index = 0; index < cache->count; index++) {
        x = (s16)(
            x - cash_digit_advances[cache->digits[index]]
        );
    }
    cache->x = x;
    cache->valid = 1;
    for (index = 0; index < cache->count; index++) {
        u8 digit = cache->digits[index];

        put_energy_hud_digit(x, y, digit);
        x = (s16)(x + cash_digit_advances[digit]);
    }
}

static void render_player_energy_status(void)
{
    /*
     * The removed PC sidebar exposed three live resources: shared weapon
     * power, hull armor and shield.  Keep their source values, but present
     * them as compact numbers in the user-configurable bottom-right stack.
     */
    render_hud_value_right(
        &energy_hud_decimal_cache[0],
        player_weapon_energy,
        TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_RIGHT_X,
        TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_Y
    );
    render_hud_value_right(
        &energy_hud_decimal_cache[1],
        player_armor,
        TYRIAN_GBA_LAYOUT_SHIP_ENERGY_RIGHT_X,
        TYRIAN_GBA_LAYOUT_SHIP_ENERGY_Y
    );
    render_hud_value_right(
        &energy_hud_decimal_cache[2],
        player_shield,
        TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_RIGHT_X,
        TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_Y
    );
}

static void render_pause_text(void)
{
    u8 index;
    s16 x = TYRIAN_GBA_LAYOUT_PAUSED_X;
    s16 y = TYRIAN_GBA_LAYOUT_PAUSED_Y;

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

static void render_game_over_text(void)
{
    u8 index;
    s16 x = TYRIAN_GBA_LAYOUT_GAME_OVER_X;
    s16 y = TYRIAN_GBA_LAYOUT_GAME_OVER_Y;

    /*
     * Direct tyrian2.c GAME OVER call:
     *   JE_dString(VGAScreen, 120, 60, miscText[21], FONT_SHAPES)
     * The generated bank omits the blank glyph; apply JE_dString's six-PC-
     * pixel space (scaled by the existing GBA FONT_SHAPES adapter) before O.
     */
    for (index = 0; index < OBJ_GAME_OVER_GLYPH_COUNT; index++) {
        if (index == 4) x += OBJ_GAME_OVER_WORD_GAP;
        put_sprite_shaped(
            x, y,
            OBJ_TILE_GAME_OVER_RUNTIME + index * 2,
            OBJ_PAL_GAME_OVER,
            ATTR0_TALL, ATTR1_SIZE_8,
            8, 16, 0
        );
        x += game_over_glyph_advances[index];
    }
    telemetry_game_over_overlay_frames++;
}

static void render_secret_level_text(void)
{
    u8 index;
    s16 x = TYRIAN_GBA_LAYOUT_SECRET_LEVEL_X;
    s16 y = TYRIAN_GBA_LAYOUT_SECRET_LEVEL_Y;

    /*
     * Direct tyrian2.c placement of miscText[59].  Unlike world geometry,
     * a system notice must survive the 12-pixel top crop, so the GBA
     * presentation adapter offsets top-edge notices downward by that crop.
     * Repeated letters reference one eight-glyph transient bank instead of
     * consuming extra OBJ VRAM.
     */
    for (index = 0; index < SECRET_LEVEL_TEXT_GLYPH_COUNT; index++) {
        u8 glyph = secret_level_glyph_map[index];

        if (index == 6) x += OBJ_SECRET_LEVEL_WORD_GAP;
        put_sprite_shaped(
            x, y,
            OBJ_TILE_SECRET_LEVEL_RUNTIME + glyph * 2,
            OBJ_PAL_SECRET_LEVEL,
            ATTR0_TALL, ATTR1_SIZE_8,
            8, 16, 0
        );
        x += secret_level_glyph_advances[glyph];
    }
}

static void render_insert_coin_text(void)
{
    u8 index;
    s16 x = TYRIAN_GBA_LAYOUT_INSERT_COIN_X;
    s16 y = TYRIAN_GBA_LAYOUT_INSERT_COIN_Y;

    /*
     * Direct mainint.c demo overlay:
     *   JE_dString(VGAScreen, 115, 10, miscText[7], SMALL_FONT_SHAPES)
     * As with the Secret Level notice, compensate for the 12-pixel top crop
     * so this fixed HUD text remains at GBA y=10.  The blank uses
     * JE_dString's exact six-pixel advance.
     */
    for (index = 0; index < INSERT_COIN_TEXT_GLYPH_COUNT; index++) {
        u8 glyph = insert_coin_glyph_map[index];

        if (glyph == INSERT_COIN_SPACE_GLYPH) {
            x = (s16)(x + 6);
            continue;
        }
        put_sprite_shaped(
            x, y,
            OBJ_TILE_INSERT_COIN_RUNTIME + glyph * 2,
            OBJ_PAL_INSERT_COIN,
            ATTR0_TALL, ATTR1_SIZE_8,
            8, 16, 0
        );
        x = (s16)(x + insert_coin_glyph_advances[glyph]);
    }
}

static u8 source_boss_bar_armor(u8 bar)
{
    u8 link = source_parity_level.boss_bar_link[bar];
    u16 armor = 256;
    u8 index;

    if (link == 0) return 0;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[index];

        if (
            source_parity_level.enemy_avail[index] != 1 &&
            enemy->linknum == link &&
            enemy->armorleft < armor
        ) {
            armor = enemy->armorleft;
        }
    }
    if (armor == 0 || armor == 256) {
        source_parity_level.boss_bar_link[bar] = 0;
        return 0;
    }
    return armor == 255 ? 254 : (u8)armor;
}

static void render_boss_bar_at(u8 armor, s16 centre_x)
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
        armor / 10u +
        (armor + 5u) / 10u +
        1u
    );
    gba_width = (u8)((pc_width * 3u + 2u) / 4u);
    gba_width = (u8)((gba_width + 3u) & ~3u);
    if (gba_width > 40) gba_width = 40;
    if (gba_width < 4) gba_width = 4;
    fill_x = (s16)(centre_x - gba_width / 2);
    full_segments = gba_width >> 3;
    for (index = 0; index < full_segments; index++) {
        put_sprite(
            fill_x + ((u16)index << 3),
            TYRIAN_GBA_LAYOUT_BOSS_BAR_Y,
            OBJ_TILE_BOSS_BAR + 1, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
    if (gba_width & 4) {
        put_sprite(
            fill_x + ((u16)full_segments << 3),
            TYRIAN_GBA_LAYOUT_BOSS_BAR_Y,
            OBJ_TILE_BOSS_BAR + 2, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
    for (index = 0; index < 5; index++) {
        put_sprite(
            centre_x - 20 + ((u16)index << 3),
            TYRIAN_GBA_LAYOUT_BOSS_BAR_Y,
            OBJ_TILE_BOSS_BAR, OBJ_PAL_BOSS_BAR,
            ATTR1_SIZE_8, 0
        );
    }
}

static void render_source_boss_bars(void)
{
    u8 armor[2];
    u8 count = 0;
    u8 index;
    u8 had_link =
        source_parity_level.boss_bar_link[0] != 0 ||
        source_parity_level.boss_bar_link[1] != 0;

    if (had_link && !telemetry_boss_perf_started) {
        telemetry_boss_perf_started = 1;
        telemetry_boss_perf_start_position = level_position;
        boss_perf_start_display_frames = telemetry_display_frames;
        boss_perf_start_missed_vblanks = telemetry_missed_vblanks;
        boss_perf_start_sprite2_misses =
            telemetry_sprite2_cache_misses;
        boss_perf_start_sprite2_evictions =
            telemetry_sprite2_cache_evictions;
        boss_perf_start_sprite2_upload_bytes =
            telemetry_sprite2_upload_bytes;
        boss_perf_start_projectile_misses =
            telemetry_projectile_cache_misses;
        boss_perf_start_l2_hits = telemetry_sprite2_l2_hits;
        boss_perf_start_l2_misses = telemetry_sprite2_l2_misses;
        boss_perf_start_l2_evictions =
            telemetry_sprite2_l2_evictions;
        boss_perf_start_l2_raw_builds =
            telemetry_sprite2_l2_raw_builds;
        boss_perf_start_l2_fallbacks =
            telemetry_sprite2_l2_rle_fallbacks;
    }

    for (index = 0; index < 2; index++) {
        u8 value = source_boss_bar_armor(index);

        if (value != 0) armor[count++] = value;
    }
    if (count == 1) {
        render_boss_bar_at(
            armor[0],
            TYRIAN_GBA_LAYOUT_BOSS_BAR_SINGLE_CENTER_X
        );
    } else if (count == 2) {
        render_boss_bar_at(
            armor[0],
            TYRIAN_GBA_LAYOUT_BOSS_BAR_LEFT_CENTER_X
        );
        render_boss_bar_at(
            armor[1],
            TYRIAN_GBA_LAYOUT_BOSS_BAR_RIGHT_CENTER_X
        );
    }
    if (
        telemetry_boss_perf_started &&
        !telemetry_boss_perf_completed &&
        source_parity_level.boss_bar_link[0] == 0 &&
        source_parity_level.boss_bar_link[1] == 0
    ) {
        telemetry_boss_perf_completed = 1;
        telemetry_boss_perf_end_position = level_position;
        telemetry_boss_perf_display_frames =
            telemetry_display_frames -
            boss_perf_start_display_frames;
        telemetry_boss_perf_missed_vblanks =
            telemetry_missed_vblanks -
            boss_perf_start_missed_vblanks;
        telemetry_boss_perf_sprite2_misses =
            telemetry_sprite2_cache_misses -
            boss_perf_start_sprite2_misses;
        telemetry_boss_perf_sprite2_evictions =
            telemetry_sprite2_cache_evictions -
            boss_perf_start_sprite2_evictions;
        telemetry_boss_perf_sprite2_upload_bytes =
            telemetry_sprite2_upload_bytes -
            boss_perf_start_sprite2_upload_bytes;
        telemetry_boss_perf_projectile_misses =
            telemetry_projectile_cache_misses -
            boss_perf_start_projectile_misses;
        telemetry_boss_perf_l2_hits =
            telemetry_sprite2_l2_hits -
            boss_perf_start_l2_hits;
        telemetry_boss_perf_l2_misses =
            telemetry_sprite2_l2_misses -
            boss_perf_start_l2_misses;
        telemetry_boss_perf_l2_evictions =
            telemetry_sprite2_l2_evictions -
            boss_perf_start_l2_evictions;
        telemetry_boss_perf_l2_raw_builds =
            telemetry_sprite2_l2_raw_builds -
            boss_perf_start_l2_raw_builds;
        telemetry_boss_perf_l2_fallbacks =
            telemetry_sprite2_l2_rle_fallbacks -
            boss_perf_start_l2_fallbacks;
    }
}
