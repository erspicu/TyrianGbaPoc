/* GBA level entry/exit and resource setup; legacy v11 baseline. */

static void clear_entities(void)
{
    u16 index;
    for (index = 0; index < MAX_ENEMIES; index++) enemies[index].active = 0;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shots[index].active = 0;
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shots[index].active = 0;
    }
    for (index = 0; index < MAX_EFFECTS; index++) effects[index].active = 0;
    for (index = 0; index < MAX_REWARDS; index++) rewards[index].active = 0;
    active_effect_count = 0;
    active_reward_count = 0;
}

static const u8 *map_row(const u8 *map, u16 row)
{
    return map + (u32)row * MAP_ROW_BYTES;
}

static void load_ring(const u8 *map, u16 rows, u16 top_row, u16 screen_block)
{
    u16 row;
    u16 first = top_row > 32 ? top_row - 32 : 0;
    u16 last = first + MAP_RING_ROWS;
    u16 *ring = (u16 *)SCREEN_BASE_BLOCK(screen_block);

    if (last > rows) {
        last = rows;
        first = last - MAP_RING_ROWS;
    }
    for (row = first; row < last; row++) {
        dmaCopy(map_row(map, row), ring + ((row & 63) * 32), MAP_ROW_BYTES);
    }
}

static u16 title_palette_colour(const OtDataView *palette, u8 index)
{
    const u8 *rgb = palette->data + (u32)index * 3;

    /* Stock palette.dat stores VGA 6-bit RGB; GBA uses 5 bits/channel. */
    return (u16)(
        (rgb[0] >> 1) |
        ((rgb[1] >> 1) << 5) |
        ((rgb[2] >> 1) << 10)
    );
}

static u8 decode_title_shp_sprite(const OtShpSprite *sprite)
{
    u32 source = 0;
    u32 x = 0;
    u32 y = 0;
    u32 pixel_count = (u32)sprite->width * sprite->height;

    if (
        !sprite->populated ||
        pixel_count == 0 ||
        pixel_count > TITLE_SHP_SCRATCH_PIXELS
    ) {
        return 0;
    }
    /*
     * 0xff is an SHP control opcode and can never be an opaque pixel, so it
     * is also an unambiguous one-byte transparency sentinel.
     */
    memset(
        title_shp_pixels,
        0xff,
        pixel_count * sizeof(title_shp_pixels[0])
    );
    while (source < sprite->encoded.size && y < sprite->height) {
        u8 code = sprite->encoded.data[source++];

        switch (code) {
        case 255:
            if (source >= sprite->encoded.size) return 0;
            x += sprite->encoded.data[source++];
            break;
        case 254:
            x = sprite->width;
            break;
        case 253:
            x++;
            break;
        default:
            if (x < sprite->width) {
                title_shp_pixels[y * sprite->width + x] = code;
            }
            x++;
            break;
        }
        while (x >= sprite->width) {
            x -= sprite->width;
            y++;
        }
    }
    return source == sprite->encoded.size;
}

static void draw_title_logo(
    u16 *framebuffer,
    const OtDataView *palette
)
{
    enum {
        LOGO_SOURCE_WIDTH = 304,
        LOGO_SOURCE_HEIGHT = 121,
        LOGO_WIDTH = 228,
        LOGO_HEIGHT = 91,
        LOGO_X = 6,
        LOGO_Y = 6,
    };
    OtShpSprite logo;
    u16 y;

    if (
        !ot_data_shp_sprite_read(3, 146, &logo) ||
        logo.width != LOGO_SOURCE_WIDTH ||
        logo.height != LOGO_SOURCE_HEIGHT ||
        !decode_title_shp_sprite(&logo)
    ) {
        return;
    }
    for (y = 0; y < LOGO_HEIGHT; y++) {
        u16 x;
        u16 source_y = (u16)((u32)y * logo.height / LOGO_HEIGHT);

        for (x = 0; x < LOGO_WIDTH; x++) {
            u16 source_x =
                (u16)((u32)x * logo.width / LOGO_WIDTH);
            u8 pixel =
                title_shp_pixels[(u32)source_y * logo.width + source_x];

            if (pixel != 0xff) {
                framebuffer[
                    (LOGO_Y + y) * SCREEN_WIDTH + LOGO_X + x
                ] = title_palette_colour(palette, (u8)pixel);
            }
        }
    }
}

static u16 title_text_width(const char *text)
{
    u16 width = 0;

    while (*text != 0) {
        if (*text == ' ') {
            width += 5;
        } else if (*text >= 'A' && *text <= 'Z') {
            OtShpSprite glyph;

            if (
                ot_data_shp_sprite_read(
                    0,
                    (u16)(*text - 'A'),
                    &glyph
                ) &&
                glyph.populated
            ) {
                width = (u16)(width + (glyph.width * 3u + 3u) / 4u + 1u);
            }
        }
        text++;
    }
    return width > 0 ? (u16)(width - 1) : 0;
}

static void draw_title_text(
    u16 *framebuffer,
    const char *text,
    u16 y,
    u16 colour
)
{
    u16 x = (u16)((SCREEN_WIDTH - title_text_width(text)) / 2);

    while (*text != 0) {
        if (*text == ' ') {
            x += 5;
        } else if (*text >= 'A' && *text <= 'Z') {
            OtShpSprite glyph;

            if (
                ot_data_shp_sprite_read(
                    0,
                    (u16)(*text - 'A'),
                    &glyph
                ) &&
                glyph.populated &&
                decode_title_shp_sprite(&glyph)
            ) {
                u16 output_width =
                    (u16)((glyph.width * 3u + 3u) / 4u);
                u16 output_height =
                    (u16)((glyph.height * 4u + 4u) / 5u);
                u16 output_y;

                for (
                    output_y = 0;
                    output_y < output_height &&
                        y + output_y < SCREEN_HEIGHT;
                    output_y++
                ) {
                    u16 output_x;
                    u16 source_y = (u16)(
                        (u32)output_y * glyph.height / output_height
                    );

                    for (
                        output_x = 0;
                        output_x < output_width &&
                            x + output_x < SCREEN_WIDTH;
                        output_x++
                    ) {
                        u16 source_x = (u16)(
                            (u32)output_x * glyph.width / output_width
                        );
                        if (
                            title_shp_pixels[
                                (u32)source_y * glyph.width + source_x
                            ] != 0xff
                        ) {
                            framebuffer[
                                (y + output_y) * SCREEN_WIDTH +
                                x + output_x
                            ] = colour;
                        }
                    }
                }
                x = (u16)(x + output_width + 1);
            }
        }
        text++;
    }
}

static u8 render_title_from_romfs(void)
{
    OtDataView palette;
    u16 *framebuffer = (u16 *)MODE3_FB;
    u16 y;

    if (
        !ot_data_pic_decode(
            4,
            title_pic_pixels,
            sizeof(title_pic_pixels)
        ) ||
        !ot_data_pic_palette_view(4, &palette)
    ) {
        return 0;
    }
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        u16 x;
        u16 source_y =
            (u16)((u32)y * OT_PIC_DECODED_HEIGHT / SCREEN_HEIGHT);

        for (x = 0; x < SCREEN_WIDTH; x++) {
            u16 source_x =
                (u16)((u32)x * OT_PIC_DECODED_WIDTH / SCREEN_WIDTH);
            framebuffer[y * SCREEN_WIDTH + x] = title_palette_colour(
                &palette,
                title_pic_pixels[
                    (u32)source_y * OT_PIC_DECODED_WIDTH + source_x
                ]
            );
        }
    }
    draw_title_logo(framebuffer, &palette);
    for (y = 108; y < SCREEN_HEIGHT; y++) {
        memset(
            framebuffer + y * SCREEN_WIDTH,
            0,
            SCREEN_WIDTH * sizeof(framebuffer[0])
        );
    }
    draw_title_text(framebuffer, "PRESS START", 117, RGB5(31, 31, 31));
    draw_title_text(
        framebuffer,
        "ROMFS SOURCE PORT",
        144,
        RGB5(13, 26, 31)
    );
    return 1;
}

static void enter_title(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    if (!render_title_from_romfs()) {
        memset(MODE3_FB, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    }
    REG_DISPCNT = MODE_3 | BG2_ON;
    audio_load_music(MOD_TYRIAN_TITLE_FULL);
    game_paused = 0;
    game_state = STATE_TITLE;
    telemetry_state_transitions++;
}

static void enter_level(void)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    clear_entities();
    hide_all_sprites();

    dmaCopy(bg1_tiles, CHAR_BASE_BLOCK(0), 512 * 32);
    dmaCopy(bg2_tiles, CHAR_BASE_BLOCK(1), 512 * 32);
    dmaCopy(bg3_tiles, CHAR_BASE_BLOCK(2), 512 * 32);
    dmaCopy(bg_palette, BG_PALETTE, 512);
    dmaCopy(obj_tiles, OBJ_BASE_ADR, OBJ_TILE_COUNT * 32);
    dmaCopy(obj_palette, SPRITE_PALETTE, 512);

    REG_BG0CNT = BG_PRIORITY(3) | CHAR_BASE(0) |
                 SCREEN_BASE(BG0_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG1CNT = BG_PRIORITY(2) | CHAR_BASE(1) |
                 SCREEN_BASE(BG1_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;
    REG_BG2CNT = BG_PRIORITY(1) | CHAR_BASE(2) |
                 SCREEN_BASE(BG2_SCREEN_BLOCK) | BG_16_COLOR | BG_SIZE_2;

    bg1_scroll_pixel = (BG1_ROWS - 20) * 8;
    bg2_scroll_pixel = (BG2_ROWS - 20) * 8;
    bg3_scroll_pixel = (BG3_ROWS - 20) * 8;
    load_ring(bg1_map, BG1_ROWS, bg1_scroll_pixel >> 3, BG0_SCREEN_BLOCK);
    load_ring(bg2_map, BG2_ROWS, bg2_scroll_pixel >> 3, BG1_SCREEN_BLOCK);
    load_ring(bg3_map, BG3_ROWS, bg3_scroll_pixel >> 3, BG2_SCREEN_BLOCK);
    /* First-level event 0 selects the original 1:2:0 opening motion. */
    bg1_scroll_speed = 1;
    bg2_scroll_speed = 2;
    bg3_scroll_speed = 0;
    bg1_scroll_delay = 1;
    bg2_scroll_delay = 1;
    bg1_scroll_delay_max = 1;
    bg2_scroll_delay_max = 1;

    player_x = 104;
    player_y = 124;
    player_invulnerable = 0;
    fire_cooldown = 0;
    player_bank = 0;
    player_cash = 0;
    game_paused = 0;
#ifdef AUTOTEST_CASH_VISUAL_TEST
    player_cash = 12345;
#endif
    level_tick = 0;
    level_position = 0;
    event_offset = 0;
    event_time = level_events[0];
    foreground_phase = 0;
    logic_accumulator = 0;
    ot_level_port_init(&source_parity_level);

    telemetry_display_frames = 0;
    telemetry_logic_updates = 0;
    telemetry_spawn_count = 0;
    telemetry_control_count = 0;
    telemetry_collision_count = 0;
    telemetry_map_rows = 0;
    telemetry_missed_vblanks = 0;
    telemetry_stream_drops = 0;
    telemetry_max_enemies = 0;
    telemetry_max_oam = 0;
    telemetry_max_effects = 0;
    telemetry_effect_drops = 0;
    telemetry_reward_spawns = 0;
    telemetry_reward_pickups = 0;
    telemetry_max_rewards = 0;
    telemetry_reward_drops = 0;
    telemetry_enemy_shots_spawned = 0;
    telemetry_enemy_shot_drops = 0;
    telemetry_max_enemy_shots = 0;
    telemetry_enemy_replacements = 0;
    telemetry_kill_cash = 0;
    telemetry_reward_controls = 0;
    telemetry_reward_assignments = 0;
    telemetry_pause_toggles = 0;
    telemetry_paused_frames = 0;
    telemetry_source_events = 0;
    telemetry_source_events_applied = 0;
    telemetry_source_events_deferred = 0;
    telemetry_source_events_skipped = 0;
    telemetry_source_spawn_attempts = 0;
    telemetry_source_spawn_successes = 0;
    telemetry_source_spawn_pool_full = 0;
    telemetry_source_spawn_missing = 0;
    telemetry_source_max_enemies = 0;
    telemetry_source_control_writes = 0;
    telemetry_source_rng_calls = 0;
    telemetry_source_motion_updates = 0;
    telemetry_source_releases = 0;
    telemetry_source_shot_triggers = 0;
    telemetry_source_launch_attempts = 0;
    telemetry_source_launch_successes = 0;
    telemetry_source_random_attempts = 0;
    telemetry_source_random_successes = 0;
    last_vblank_seen = telemetry_vblank_irqs;

    REG_BG0HOFS = 8;
    REG_BG1HOFS = 8;
    REG_BG2HOFS = 8;
    REG_BG0VOFS = bg1_scroll_pixel & 511;
    REG_BG1VOFS = bg2_scroll_pixel & 511;
    REG_BG2VOFS = bg3_scroll_pixel & 511;
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    REG_DISPCNT = MODE_0 | BG0_ON | BG1_ON | BG2_ON |
                  OBJ_ON | OBJ_1D_MAP;
    audio_load_music(MOD_TYRIAN_LEVEL_FULL);
    game_state = STATE_PLAY;
    telemetry_state_transitions++;
}
