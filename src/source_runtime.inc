/*
 * GBA presentation/input adapter for the directly translated OpenTyrian
 * selected-level runtime. Source gameplay stays in its original coordinate
 * system.  These helpers reproduce JE_starShowVGA's x=24 extraction, then
 * centre-crop its 264x184 gameplay viewport to the GBA's native 240x160.
 * No source coordinate or sprite is rescaled.
 */

typedef struct {
    u16 graphic;
    u32 last_epoch;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 valid;
} SourceEnemyCacheSlot;

typedef struct {
    u8 slot;
    u8 l2_slot;
} SourceEnemyUpload;

typedef struct {
    u32 last_epoch;
    u16 graphic;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 valid;
} SourceSprite2L2Slot;

static SourceEnemyCacheSlot
    source_enemy_cache[SOURCE_ENEMY_CACHE_SLOT_COUNT] EWRAM_BSS;
static SourceEnemyUpload
    source_enemy_uploads[SOURCE_ENEMY_CACHE_SLOT_COUNT] EWRAM_BSS;
static SourceSprite2L2Slot source_sprite2_l2[
    SOURCE_SPRITE2_L2_SLOT_COUNT
] EWRAM_BSS;
/*
 * The complete build-time raw Sprite2 catalog makes this 2 KiB decoder
 * scratch a debug/fallback-only path.  Keep it in IWRAM BSS: the packed
 * background colour-error table recovered enough IWRAM headroom, while this
 * placement preserves the release 12 KiB EWRAM heap floor without shrinking
 * L2.  Runtime stack-canary tests independently guard the IWRAM boundary.
 */
static u16 source_enemy_decode_pixels[OT_SPRITE2_FRAME_PIXELS]
    __attribute__((aligned(4)));
static u8 source_enemy_palette_map[OT_PALETTE_COLOUR_COUNT]
    EWRAM_BSS;
static u32 source_enemy_cache_epoch;
static u8 source_enemy_upload_count;
static u16 source_player_cache_graphic;
static u8 source_player_cache_valid;
static u8 source_player_cache_visible;
static u8 source_player_upload_pending;

#ifdef AUTOTEST
static u32 source_sprite2_pending_upload_count(void)
{
    return source_enemy_upload_count +
        (source_player_upload_pending ? 1u : 0u);
}
#endif
static u8 source_player_upload_l2_slot;
static u8 source_enemy_palette_valid;
static u8 source_enemy_first_decode_failure_valid;
static u8 source_enemy_first_decode_failure_shape_table;
static u8 source_enemy_first_decode_failure_size;
static u8 source_enemy_first_decode_failure_filter;
static u8 source_enemy_first_decode_failure_pool;
static u8 source_enemy_first_decode_failure_source_index;
static u16 source_enemy_first_decode_failure_graphic;
static u16 source_enemy_first_decode_failure_definition;
static u16 source_enemy_first_decode_failure_level_position;
static s16 source_enemy_first_decode_failure_x;
static s16 source_enemy_first_decode_failure_y;
static u8 source_enemy_first_decode_failure_cycle;
static u32 source_enemy_first_decode_failure_shape_banks;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
enum {
    SOURCE_ENEMY_CACHE_DROP_RECORD_COUNT = 4,
    SOURCE_ENEMY_CACHE_DROP_NO_SLOT = 1,
    SOURCE_ENEMY_CACHE_DROP_UPLOAD_LIMIT = 2,
};

typedef struct {
    u32 reason;
    u32 level_position;
    u32 shape_table;
    u32 graphic;
    u32 size;
    u32 filter;
    u32 pool;
    u32 source_index;
    u32 definition_id;
    s32 x;
    s32 y;
    u32 enemy_cycle;
    u32 upload_count;
    u32 active_slots;
    u32 active_full_slots;
    u32 active_compact_slots;
} SourceEnemyCacheDropRecord;

static SourceEnemyCacheDropRecord source_enemy_cache_drop_record[
    SOURCE_ENEMY_CACHE_DROP_RECORD_COUNT
] EWRAM_BSS;
static u8 source_enemy_cache_drop_record_count;
#endif
static u32 source_sprite2_l2_epoch;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
static u8 source_autotest_unreachable_stop_ticks;
#endif

/*
 * The source level needs more than 15 simultaneous colours per enemy frame,
 * so it uses 8bpp OBJ.  Static 4bpp actors retain their authored banks; only
 * banks unused by the source-level renderer are time-shared here.
 */
static const u8 source_enemy_dynamic_palette_banks[
    SOURCE_ENEMY_DYNAMIC_PALETTE_BANK_COUNT
] = {
    1, 2, 3, 4, 5, 8, 13, 15,
};

static const u8 source_enemy_brightness_samples[
    SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT
] = {
    0, 2, 4, 6, 8, 10, 12, 15,
};

static u32 source_palette_distance(
    const u8 *left,
    const u8 *right
)
{
    s16 red = (s16)left[0] - right[0];
    s16 green = (s16)left[1] - right[1];
    s16 blue = (s16)left[2] - right[2];

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}

static u8 source_enemy_palette_destination(u8 logical_index)
{
    u8 bank = source_enemy_dynamic_palette_banks[
        logical_index / 16
    ];

    return (u8)(bank * 16 + logical_index % 16);
}

static void source_sprite2_l2_flush(void)
{
    memset(source_sprite2_l2, 0, sizeof(source_sprite2_l2));
    source_sprite2_l2_epoch = 0;
    telemetry_sprite2_l2_flushes++;
}

static u8 source_enemy_palette_setup(u8 palette_index)
{
    OtDataView palette;
    u8 hue;

    source_enemy_palette_valid = 0;
    source_sprite2_l2_flush();
    if (!ot_data_palette_view(palette_index, &palette)) return 0;

    for (hue = 0; hue < 16; hue++) {
        u8 brightness;

        for (
            brightness = 0;
            brightness < SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT;
            brightness++
        ) {
            u8 logical = (u8)(
                hue * SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT +
                brightness
            );
            u8 source_index = (u8)(
                hue * 16 +
                source_enemy_brightness_samples[brightness]
            );
            u8 destination =
                source_enemy_palette_destination(logical);

            SPRITE_PALETTE[destination] =
                title_palette_colour(&palette, source_index);
        }

        /*
         * Preserve the PC hue nibble and choose the closest of eight
         * brightnesses in the actual active palette.  This is deterministic
         * and applies to every current or future Sprite2 frame.
         */
        for (brightness = 0; brightness < 16; brightness++) {
            const u8 *source_rgb =
                palette.data + (u32)(hue * 16 + brightness) * 3;
            u32 best_distance = 0xffffffffu;
            u8 best = 0;
            u8 candidate;

            for (
                candidate = 0;
                candidate < SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT;
                candidate++
            ) {
                u8 candidate_index = (u8)(
                    hue * 16 +
                    source_enemy_brightness_samples[candidate]
                );
                u32 distance = source_palette_distance(
                    source_rgb,
                    palette.data + (u32)candidate_index * 3
                );

                if (distance < best_distance) {
                    best_distance = distance;
                    best = candidate;
                }
            }
            source_enemy_palette_map[hue * 16 + brightness] =
                source_enemy_palette_destination(
                    (u8)(
                        hue * SOURCE_ENEMY_BRIGHTNESS_SAMPLE_COUNT +
                        best
                    )
                );
        }
    }
    source_enemy_palette_valid = 1;
    return 1;
}

static void source_sprite2_l2_begin_frame(void)
{
    source_sprite2_l2_epoch++;
    if (source_sprite2_l2_epoch == 0) {
        u8 index;

        source_sprite2_l2_epoch = 1;
        for (
            index = 0;
            index < SOURCE_SPRITE2_L2_SLOT_COUNT;
            index++
        ) {
            source_sprite2_l2[index].last_epoch = 0;
        }
    }
}

static void source_sprite2_l2_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_SPRITE2_L2_SLOT_COUNT;
        index++
    ) {
        if (
            source_sprite2_l2[index].valid &&
            source_sprite2_l2[index].last_epoch ==
                source_sprite2_l2_epoch
        ) {
            visible_unique++;
        }
    }
    if (
        visible_unique >
        telemetry_sprite2_l2_max_visible_unique
    ) {
        telemetry_sprite2_l2_max_visible_unique = visible_unique;
    }
}

static IWRAM_CODE ARM_CODE void source_sprite2_l2_clear_frame(
    u8 *target
)
{
    u32 *words = (u32 *)target;
    u16 index;

    for (
        index = 0;
        index < SOURCE_SPRITE2_L2_FRAME_BYTES / sizeof(u32);
        index++
    ) {
        words[index] = 0;
    }
}

static IWRAM_CODE ARM_CODE u32 source_sprite2_tile_offset(
    u8 x,
    u8 y,
    u8 tile_columns
)
{
    return
        (u32)((y >> 3) * tile_columns + (x >> 3)) * 64u +
        (u32)(y & 7u) * 8u +
        (x & 7u);
}

/*
 * OpenTyrian draws weapon graphic 60034 with
 * blit_sprite_blend(OPTION_SHAPES, 33).  That authored sprite is 80x79,
 * larger than the GBA's maximum 64x64 OBJ.  Decode the stock ROMFS stream
 * directly and centre-crop it at 1:1; presentation then uses the GBA's
 * semi-transparent OBJ mode.  This is a hardware-bound adapter, not a
 * generated GBA-only replacement asset.
 */
static u8 source_option_projectile_build(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    OtShpSprite sprite;
    const u8 *encoded;
    u8 *target = (u8 *)source_sprite2_l2_tiles;
    u32 source = 0;
    u16 x = 0;
    u16 y = 0;

    source_option_projectile_valid = 0;
    memset(
        target,
        0,
        SOURCE_OPTION_PROJECTILE_BYTES
    );
    if (
        !source_enemy_palette_valid ||
        !ot_data_shp_sprite_read(
            SOURCE_OPTION_PROJECTILE_SOURCE_TABLE,
            SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC,
            &sprite
        ) ||
        !sprite.populated ||
        sprite.width != SOURCE_OPTION_PROJECTILE_SOURCE_WIDTH ||
        sprite.height != SOURCE_OPTION_PROJECTILE_SOURCE_HEIGHT
    ) {
        return 0;
    }
    encoded = sprite.encoded.data;
    while (source < sprite.encoded.size && y < sprite.height) {
        u8 code = encoded[source++];

        if (code == 255) {
            if (source >= sprite.encoded.size) return 0;
            x = (u16)(x + encoded[source++]);
        } else if (code == 254) {
            x = sprite.width;
        } else if (code == 253) {
            x++;
        } else {
            if (
                x >= SOURCE_OPTION_PROJECTILE_CROP_X &&
                x <
                    SOURCE_OPTION_PROJECTILE_CROP_X +
                        SOURCE_OPTION_PROJECTILE_SIZE &&
                y >= SOURCE_OPTION_PROJECTILE_CROP_Y &&
                y <
                    SOURCE_OPTION_PROJECTILE_CROP_Y +
                        SOURCE_OPTION_PROJECTILE_SIZE
            ) {
                u8 target_x = (u8)(
                    x - SOURCE_OPTION_PROJECTILE_CROP_X
                );
                u8 target_y = (u8)(
                    y - SOURCE_OPTION_PROJECTILE_CROP_Y
                );

                target[
                    source_sprite2_tile_offset(
                        target_x,
                        target_y,
                        SOURCE_OPTION_PROJECTILE_SIZE / 8
                    )
                ] = source_enemy_palette_map[code];
            }
            x++;
        }
        if (x >= sprite.width) {
            x = 0;
            y++;
        }
    }
    if (
        y < SOURCE_OPTION_PROJECTILE_CROP_Y +
            SOURCE_OPTION_PROJECTILE_SIZE
    ) {
        return 0;
    }
    dmaCopy(
        target,
        (u8 *)OBJ_BASE_ADR +
            SOURCE_OPTION_PROJECTILE_TILE_BASE * 32,
        SOURCE_OPTION_PROJECTILE_BYTES
    );
    source_option_projectile_valid = 1;
    return 1;
#else
    static const u8 expected_width[
        SOURCE_OPTION_PROJECTILE_FRAME_COUNT
    ] = {62, 65, 62};
    static const u8 expected_height[
        SOURCE_OPTION_PROJECTILE_FRAME_COUNT
    ] = {24, 28, 23};
    static const u8 palette_slot[14] = {
        1, 1, 4, 5, 6, 7, 8,
        9, 10, 11, 12, 13, 14, 15,
    };
    OtDataView palette;
    u8 frame;
    u8 colour;

    source_option_projectile_valid = 0;
    if (
        !source_enemy_palette_valid ||
        !ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)
    ) {
        return 0;
    }
    SPRITE_PALETTE[
        SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16
    ] = 0;
    for (colour = 0; colour < 14; colour++) {
        SPRITE_PALETTE[
            SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16 +
            palette_slot[colour]
        ] = title_palette_colour(
            &palette,
            (u8)(114u + colour)
        );
    }
    /*
     * Palette bank 12 is otherwise the persistent gold GENERATOR digit
     * bank. Plasma Storm needs fourteen red shades, so merge only its two
     * darkest source entries and reserve slots 2/3 for the HUD. This avoids
     * recolouring the OSD while that source-faithful sidekick is equipped.
     */
    SPRITE_PALETTE[
        SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16 + 2u
    ] = ((const u16 *)(const void *)obj_palette)[
        OBJ_PAL_HUD_GENERATOR * 16u + 2u
    ];
    SPRITE_PALETTE[
        SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16 + 3u
    ] = ((const u16 *)(const void *)obj_palette)[
        OBJ_PAL_HUD_GENERATOR * 16u + 3u
    ];
    for (
        frame = 0;
        frame < SOURCE_OPTION_PROJECTILE_FRAME_COUNT;
        frame++
    ) {
        OtShpSprite sprite;
        const u8 *encoded;
        u8 *target = (u8 *)source_sprite2_l2_tiles;
        u32 source = 0;
        u16 x = 0;
        u16 y = 0;

        memset(target, 0, SOURCE_OPTION_PROJECTILE_BYTES);
        if (
            !source_enemy_palette_valid ||
            !ot_data_shp_sprite_read(
                SOURCE_OPTION_PROJECTILE_SOURCE_TABLE,
                (u16)(
                    SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC + frame
                ),
                &sprite
            ) ||
            !sprite.populated ||
            sprite.width != expected_width[frame] ||
            sprite.height != expected_height[frame]
        ) {
            return 0;
        }
        encoded = sprite.encoded.data;
        while (
            source < sprite.encoded.size &&
            y < sprite.height
        ) {
            u8 code = encoded[source++];

            if (code == 255) {
                if (source >= sprite.encoded.size) return 0;
                x = (u16)(x + encoded[source++]);
            } else if (code == 254) {
                x = sprite.width;
            } else if (code == 253) {
                x++;
            } else {
                if (
                    x < SOURCE_OPTION_PROJECTILE_SIZE_X &&
                    y < SOURCE_OPTION_PROJECTILE_SIZE_Y
                ) {
                    u32 target_offset;
                    u8 palette_index;

                    if (code < 114 || code > 127) return 0;
                    palette_index = palette_slot[code - 114u];
                    target_offset =
                        (u32)(
                            (y >> 3) *
                                (
                                    SOURCE_OPTION_PROJECTILE_SIZE_X /
                                    8
                                ) +
                            (x >> 3)
                        ) *
                            32u +
                        (u32)(y & 7u) * 4u +
                        ((x & 7u) >> 1);
                    if (x & 1u) {
                        target[target_offset] |=
                            (u8)(palette_index << 4);
                    } else {
                        target[target_offset] |= palette_index;
                    }
                }
                x++;
            }
            if (x >= sprite.width) {
                x = 0;
                y++;
            }
        }
        if (y < sprite.height) return 0;
        dmaCopy(
            target,
            (u8 *)OBJ_BASE_ADR +
                (
                    SOURCE_OPTION_PROJECTILE_TILE_BASE +
                    frame *
                        SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME
                ) *
                32,
            SOURCE_OPTION_PROJECTILE_BYTES
        );
        source_option_projectile_valid |= (u8)(1u << frame);
    }
    return source_option_projectile_valid ==
        (1u << SOURCE_OPTION_PROJECTILE_FRAME_COUNT) - 1u;
#endif
}

static u8 source_enemy_cache_slot_reserved(u8 slot)
{
    if (
        slot >= SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT &&
        slot <
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT +
                SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT &&
        slot > SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT &&
        (
            game_state != STATE_PLAY ||
            source_parity_level.display_time != 0 ||
            frontend_demo_active
        )
    ) {
        /*
         * Compact slots after the primary one time-share the 16-tile
         * GAME OVER / SECRET LEVEL / INSERT COIN bank.  These overlays are
         * mutually exclusive with ordinary play, but a demo or source
         * secret notice can occupy the bank while the world still renders.
         */
        return 1;
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (
        slot == SOURCE_ENEMY_CACHE_SPLIT_SLOT &&
        game_paused
    ) {
        /* The lower 32x16 half time-shares the PAUSED glyph bank. */
        return 1;
    }
#endif
#if TYRIAN_GBA_STRESS_LOADOUT
    (void)slot;
    return 0;
#else
    return
        source_option_projectile_valid != 0 &&
        slot >=
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                SOURCE_OPTION_PROJECTILE_FRAME_COUNT &&
        slot < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
#endif
}

static u8 source_option_projectile_sync(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    if (source_option_projectile_valid) return 1;
    return source_option_projectile_build();
#else
    u8 required =
        source_parity_level.player_sidekick[0] == 9 ||
        source_parity_level.player_sidekick[1] == 9;
    u8 index;

    if (!required) {
        source_option_projectile_valid = 0;
        return 1;
    }
    if (
        source_option_projectile_valid ==
            (1u << SOURCE_OPTION_PROJECTILE_FRAME_COUNT) - 1u
    ) {
        return 1;
    }
    /*
     * The three 4bpp 64x32 Plasma Storm frames time-share the final three
     * full-size enemy cache slots only while that option is equipped.
     * Standard loadouts retain the complete ordinary full-size bank plus
     * the fragmented overflow slot used by dense SAVARA scenes.
     */
    for (
        index =
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                SOURCE_OPTION_PROJECTILE_FRAME_COUNT;
        index < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
        index++
    ) {
        source_enemy_cache[index].valid = 0;
    }
    if (!source_option_projectile_build()) {
        source_option_projectile_valid = 0;
        return 0;
    }
    return 1;
#endif
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u32
source_sprite2_l2_pack_raw_word(
    const u8 *raw,
    u8 filter
)
{
    u32 packed = 0;
    u8 lane;

    for (lane = 0; lane < 4; lane++) {
        u8 pixel = raw[lane];

        if (pixel != 0) {
            if (filter != 0) {
                pixel = (u8)(filter | (pixel & 0x0f));
            }
            pixel = source_enemy_palette_map[pixel];
        }
        packed |= (u32)pixel << (lane * 8u);
    }
    return packed;
}

/*
 * Keep this as a real ARM/IWRAM leaf.  With -O3 the former annotation was
 * defeated by inlining the complete byte-at-a-time loop into its ROM caller.
 */
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
source_sprite2_l2_write_raw_component(
    u8 *target,
    u8 tile_columns,
    u8 shape_table,
    u16 graphic,
    u8 filter,
    u8 origin_x,
    u8 origin_y
)
{
    const u8 *raw = ot_sprite2_raw_component(
        shape_table,
        graphic
    );
    u8 tile_column = origin_x >> 3;
    u8 row_offset = origin_x & 7u;
    u8 y;

    if (
        raw == 0 ||
        (
            row_offset != 0 &&
            row_offset != 2 &&
            row_offset != 4
        )
    ) {
        return 0;
    }
    for (y = 0; y < SPRITE2_RAW_COMPONENT_HEIGHT; y++) {
        u8 target_y = (u8)(origin_y + y);
        u8 *row = target +
            (
                (u32)(
                    (target_y >> 3) * tile_columns +
                    tile_column
                ) * 64u
            ) +
            (u32)(target_y & 7u) * 8u;
        u32 word0 = source_sprite2_l2_pack_raw_word(raw, filter);
        u32 word1 = source_sprite2_l2_pack_raw_word(raw + 4, filter);
        u32 word2 = source_sprite2_l2_pack_raw_word(raw + 8, filter);

        if (row_offset == 0) {
            ((u32 *)row)[0] = word0;
            ((u32 *)row)[1] = word1;
            *(u32 *)(row + 64) = word2;
        } else if (row_offset == 4) {
            *(u32 *)(row + 4) = word0;
            ((u32 *)(row + 64))[0] = word1;
            ((u32 *)(row + 64))[1] = word2;
        } else {
            /*
             * origin_x=10 spans x=2..7 of one tile and x=0..5 of
             * the next.  Two aligned words plus two aligned halfwords
             * preserve the exact little-endian byte layout.
             */
            *(u16 *)(row + 2) = (u16)word0;
            *(u32 *)(row + 4) =
                (word0 >> 16) | (word1 << 16);
            *(u32 *)(row + 64) =
                (word1 >> 16) | (word2 << 16);
            *(u16 *)(row + 68) = (u16)(word2 >> 16);
        }
        raw += SPRITE2_RAW_COMPONENT_WIDTH;
    }
    return 1;
}

static IWRAM_CODE ARM_CODE void source_sprite2_l2_pack_decoded(
    u8 *target,
    u8 tile_columns,
    u8 source_x,
    u8 source_y,
    u8 width,
    u8 height
)
{
    u8 y;

    for (y = 0; y < height; y++) {
        u8 x;

        for (x = 0; x < width; x++) {
            u16 source_pixel = source_enemy_decode_pixels[
                (u32)(source_y + y) * OT_SPRITE2_FRAME_WIDTH +
                source_x + x
            ];

            if (source_pixel != 0) {
                target[source_sprite2_tile_offset(
                    x,
                    y,
                    tile_columns
                )] = source_enemy_palette_map[
                    (u8)(source_pixel - 1u)
                ];
            }
        }
    }
}

static u8 source_sprite2_l2_build(
    u8 slot,
    u8 shape_table,
    u16 graphic,
    u8 size,
    u8 filter
)
{
    u8 *target = source_sprite2_l2_tiles[slot];
    u8 projectile =
        shape_table == OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY ||
        shape_table == OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY;
    u8 raw_valid;

    source_sprite2_l2_clear_frame(target);
    if (size == 1) {
        raw_valid =
            source_sprite2_l2_write_raw_component(
                target, 4, shape_table, graphic, filter, 4, 2
            ) &&
            source_sprite2_l2_write_raw_component(
                target, 4, shape_table,
                (u16)(graphic + 1u), filter, 16, 2
            ) &&
            source_sprite2_l2_write_raw_component(
                target, 4, shape_table,
                (u16)(graphic + 19u), filter, 4, 16
            ) &&
            source_sprite2_l2_write_raw_component(
                target, 4, shape_table,
                (u16)(graphic + 20u), filter, 16, 16
            );
    } else if (projectile) {
        raw_valid = source_sprite2_l2_write_raw_component(
            target, 2, shape_table, graphic, filter, 0, 0
        );
    } else {
        raw_valid = source_sprite2_l2_write_raw_component(
            target, 4, shape_table, graphic, filter, 10, 9
        );
    }
    if (raw_valid) {
        telemetry_sprite2_l2_raw_builds++;
        return 1;
    }

    /*
     * The exhaustive build-time catalog should make this unreachable.
     * Preserve the original ROMFS RLE decoder as a parity/debug fallback.
     */
    telemetry_sprite2_l2_rle_fallbacks++;
    if (
        !ot_sprite2_frame_decode(
            shape_table,
            graphic,
            size,
            filter,
            source_enemy_decode_pixels,
            OT_SPRITE2_FRAME_PIXELS
        )
    ) {
        return 0;
    }
    source_sprite2_l2_clear_frame(target);
    if (projectile && size == 0) {
        source_sprite2_l2_pack_decoded(
            target, 2, 10, 9, 16, 16
        );
    } else {
        source_sprite2_l2_pack_decoded(
            target, 4, 0, 0, 32, 32
        );
    }
    return 1;
}

static u8 source_sprite2_l2_acquire(
    u8 shape_table,
    u16 graphic,
    u8 size,
    u8 filter
)
{
    u8 index;
    u8 free_slot = SOURCE_SPRITE2_L2_SLOT_COUNT;
    u8 eviction_slot = SOURCE_SPRITE2_L2_SLOT_COUNT;
    u8 selected;
    u32 oldest_epoch = 0xffffffffu;

    for (
        index = 0;
        index < SOURCE_SPRITE2_L2_SLOT_COUNT;
        index++
    ) {
        SourceSprite2L2Slot *slot = &source_sprite2_l2[index];

        if (
            slot->valid &&
            slot->shape_table == shape_table &&
            slot->graphic == graphic &&
            slot->size == size &&
            slot->filter == filter
        ) {
            slot->last_epoch = source_sprite2_l2_epoch;
            telemetry_sprite2_l2_hits++;
            return index;
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_SPRITE2_L2_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_sprite2_l2_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != SOURCE_SPRITE2_L2_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    telemetry_sprite2_l2_misses++;
    if (selected == SOURCE_SPRITE2_L2_SLOT_COUNT) {
        telemetry_sprite2_l2_drops++;
        return 0xff;
    }
    if (
        !source_sprite2_l2_build(
            selected,
            shape_table,
            graphic,
            size,
            filter
        )
    ) {
        telemetry_sprite2_l2_drops++;
        source_sprite2_l2[selected].valid = 0;
        return 0xff;
    }
    if (source_sprite2_l2[selected].valid) {
        telemetry_sprite2_l2_evictions++;
    }
    source_sprite2_l2[selected].last_epoch =
        source_sprite2_l2_epoch;
    source_sprite2_l2[selected].graphic = graphic;
    source_sprite2_l2[selected].shape_table = shape_table;
    source_sprite2_l2[selected].size = size;
    source_sprite2_l2[selected].filter = filter;
    source_sprite2_l2[selected].valid = 1;
    return selected;
}

/*
 * Ordinary player ships are spriteSheet9 Sprite2 frames in the PC renderer,
 * but they must not consume an enemy-cache slot.  Episode 1's third routed
 * level reaches the complete 24-frame enemy working set; sharing that cache
 * made one authored object disappear.  OBJ tiles 0..31 already back the
 * legacy 32x32 player atlas, so time-share that exact 1 KiB region as a
 * dedicated source-player frame.
 */
static u16 source_player_cache_acquire(u16 graphic)
{
    u8 l2_slot;

    source_player_cache_visible = 1;
    if (
        source_player_cache_valid &&
        source_player_cache_graphic == graphic
    ) {
        telemetry_sprite2_cache_hits++;
        return SOURCE_PLAYER_CACHE_TILE_BASE;
    }
    if (!source_enemy_palette_valid || graphic == 0) {
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        return 0xffff;
    }

    telemetry_sprite2_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        OT_COMP_SHAPE_TABLE_OPTIONS_SMALL,
        graphic,
        1,
        0
    );
    if (l2_slot == 0xff) {
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        source_player_cache_valid = 0;
        return 0xffff;
    }
    if (source_player_cache_valid) {
        telemetry_sprite2_cache_evictions++;
    }
    if (source_player_upload_pending) {
        /* Supersede an uncommitted player frame without losing its upload. */
        telemetry_sprite2_upload_coalesces++;
    }
    source_player_cache_graphic = graphic;
    source_player_cache_valid = 1;
    source_player_upload_l2_slot = l2_slot;
    source_player_upload_pending = 1;
    if (
        source_enemy_upload_count + 1u >
        telemetry_sprite2_max_uploads
    ) {
        telemetry_sprite2_max_uploads =
            source_enemy_upload_count + 1u;
    }
    return SOURCE_PLAYER_CACHE_TILE_BASE;
}

static void source_player_cache_commit(void)
{
    if (!source_player_upload_pending) return;

    dmaCopy(
        source_sprite2_l2_tiles[source_player_upload_l2_slot],
        (u8 *)OBJ_BASE_ADR + SOURCE_PLAYER_CACHE_TILE_BASE * 32u,
        SOURCE_ENEMY_FRAME_BYTES
    );
    telemetry_sprite2_uploads++;
    telemetry_sprite2_upload_bytes += SOURCE_ENEMY_FRAME_BYTES;
    source_player_upload_pending = 0;
}

static u16 source_enemy_cache_tile(u8 slot)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (slot == SOURCE_ENEMY_CACHE_SPLIT_SLOT) {
        return SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE;
    }
#endif
    if (slot >= SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT) {
        u8 compact =
            (u8)(slot - SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT);

        if (compact != 0) {
            return (u16)(
                OBJ_TILE_GAME_OVER_RUNTIME +
                (compact - 1u) *
                    SOURCE_ENEMY_COMPACT_TILES_PER_SLOT
            );
        }
        return (u16)(
            SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE
        );
    }
    if (slot < SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT) {
        return (u16)(
            SOURCE_ENEMY_CACHE_RECLAIMED_TILE_BASE +
            slot * SOURCE_ENEMY_TILES_PER_SLOT
        );
    }
    slot = (u8)(slot - SOURCE_ENEMY_CACHE_RECLAIMED_SLOT_COUNT);
    if (slot < SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT) {
        return (u16)(
            SOURCE_ENEMY_CACHE_LOWER_TILE_BASE +
            slot * SOURCE_ENEMY_TILES_PER_SLOT
        );
    }
    return (u16)(
        SOURCE_ENEMY_CACHE_UPPER_TILE_BASE +
        (slot - SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT) *
            SOURCE_ENEMY_TILES_PER_SLOT
    );
}

static u8 source_enemy_cache_slot_is_compact(u8 slot)
{
    return
        slot >= SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT &&
        slot <
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT +
                SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT;
}

static u8 source_enemy_cache_tile_is_split(u16 tile)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    (void)tile;
    return 0;
#else
    return tile == SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE;
#endif
}

static u8 source_enemy_cache_tile_is_compact(u16 tile)
{
    return
        (
            tile >= SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE &&
            tile <
                SOURCE_ENEMY_CACHE_COMPACT_TILE_BASE +
                    SOURCE_ENEMY_COMPACT_TILES_PER_SLOT
        ) ||
        (
            tile >= OBJ_TILE_GAME_OVER_RUNTIME &&
            tile <
                OBJ_TILE_GAME_OVER_RUNTIME +
                    (
                        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT -
                        1u
                    ) *
                        SOURCE_ENEMY_COMPACT_TILES_PER_SLOT
        );
}

static void source_enemy_cache_begin_frame(void)
{
    source_enemy_cache_epoch++;
    source_player_cache_visible = 0;
    if (
        SOURCE_ENEMY_CACHE_COMPACT_SLOT_COUNT > 1 &&
        (
            game_state != STATE_PLAY ||
            source_parity_level.display_time != 0 ||
            frontend_demo_active
        )
    ) {
        /*
         * A status-label DMA overwrites the time-shared compact slot.
         * Leave it invalid until the overlay disappears so a later hit can
         * never reuse corrupted OBJ data.
         */
        source_enemy_cache[
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT + 1u
        ].valid = 0;
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (game_paused) {
        source_enemy_cache[SOURCE_ENEMY_CACHE_SPLIT_SLOT].valid = 0;
    }
#endif
    if (source_enemy_cache_epoch == 0) {
        u8 index;

        source_enemy_cache_epoch = 1;
        for (
            index = 0;
            index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
            index++
        ) {
            source_enemy_cache[index].last_epoch = 0;
        }
    }
}

static void source_enemy_cache_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
        index++
    ) {
        if (source_enemy_cache_slot_reserved(index)) continue;
        if (
            source_enemy_cache[index].valid &&
            source_enemy_cache[index].last_epoch ==
                source_enemy_cache_epoch
        ) {
            visible_unique++;
        }
    }
    if (source_player_cache_visible) visible_unique++;
    if (
        visible_unique >
        telemetry_sprite2_max_visible_unique
    ) {
        telemetry_sprite2_max_visible_unique = visible_unique;
    }
}

#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
static void source_enemy_cache_record_drop(
    const OtEnemyDrawCommand *draw,
    u8 reason
)
{
    SourceEnemyCacheDropRecord *record;
    u8 index;

    if (
        source_enemy_cache_drop_record_count >=
        SOURCE_ENEMY_CACHE_DROP_RECORD_COUNT
    ) {
        return;
    }
    record = &source_enemy_cache_drop_record[
        source_enemy_cache_drop_record_count++
    ];
    record->reason = reason;
    record->level_position = level_position;
    record->shape_table = draw->shape_table;
    record->graphic = draw->graphic;
    record->size = draw->size;
    record->filter = draw->filter;
    record->pool = draw->pool;
    record->source_index = draw->source_index;
    record->definition_id = draw->enemy_definition_id;
    record->x = draw->x;
    record->y = draw->y;
    record->enemy_cycle = draw->enemy_cycle;
    record->upload_count = source_enemy_upload_count;
    for (
        index = 0;
        index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
        index++
    ) {
        if (
            source_enemy_cache_slot_reserved(index) ||
            !source_enemy_cache[index].valid ||
            source_enemy_cache[index].last_epoch !=
                source_enemy_cache_epoch
        ) {
            continue;
        }
        record->active_slots++;
        if (source_enemy_cache_slot_is_compact(index)) {
            record->active_compact_slots++;
        } else {
            record->active_full_slots++;
        }
    }
}
#endif

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u16
source_enemy_cache_acquire(
    const OtEnemyDrawCommand *draw
)
{
    u8 index;
    u8 selected = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 free_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 l2_slot;
    u32 oldest_epoch = 0xffffffffu;

    if (draw->shape_table == 0) {
        /*
         * Exact counterpart of blit_enemy(sprite2s == NULL): a cold-start
         * APPROACH pointer has no previous enemySpriteSheets[] slot to reuse.
         * This is not malformed ROM data and must not poison decoder/cache
         * telemetry.
         */
        telemetry_sprite2_null_pointer_skips++;
        return 0xffff;
    }
    if (draw->graphic == 0) {
        /*
         * Some stock control animations select an authored zero egraphic.
         * The DOS-era renderer's index-1 access is undefined for that value;
         * on GBA it is an explicit transparent/no-draw command, never a ROM
         * decode failure.
         */
        telemetry_sprite2_zero_graphic_skips++;
        return 0xffff;
    }
    if (!source_enemy_palette_valid) {
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        return 0xffff;
    }

    for (
        index = 0;
        index < SOURCE_ENEMY_CACHE_SLOT_COUNT;
        index++
    ) {
        SourceEnemyCacheSlot *slot = &source_enemy_cache[index];

        if (source_enemy_cache_slot_reserved(index)) continue;
        if (
            source_enemy_cache_slot_is_compact(index) &&
            draw->size != 0
        ) {
            continue;
        }
        if (
            slot->valid &&
            slot->shape_table == draw->shape_table &&
            slot->graphic == draw->graphic &&
            slot->size == draw->size &&
            slot->filter == draw->filter
        ) {
            slot->last_epoch = source_enemy_cache_epoch;
            telemetry_sprite2_cache_hits++;
            return source_enemy_cache_tile(index);
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_enemy_cache_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
        source_enemy_cache_record_drop(
            draw,
            SOURCE_ENEMY_CACHE_DROP_NO_SLOT
        );
#endif
        telemetry_sprite2_cache_drops++;
        return 0xffff;
    }
    if (source_enemy_cache[selected].valid) {
        telemetry_sprite2_cache_evictions++;
    }
    source_enemy_cache[selected].valid = 1;
    source_enemy_cache[selected].shape_table = draw->shape_table;
    source_enemy_cache[selected].graphic = draw->graphic;
    source_enemy_cache[selected].size = draw->size;
    source_enemy_cache[selected].filter = draw->filter;
    source_enemy_cache[selected].last_epoch = source_enemy_cache_epoch;
    telemetry_sprite2_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        draw->shape_table,
        draw->graphic,
        draw->size,
        draw->filter
    );
    if (l2_slot == 0xff) {
        if (!source_enemy_first_decode_failure_valid) {
            source_enemy_first_decode_failure_valid = 1;
            source_enemy_first_decode_failure_shape_table =
                draw->shape_table;
            source_enemy_first_decode_failure_graphic = draw->graphic;
            source_enemy_first_decode_failure_size = draw->size;
            source_enemy_first_decode_failure_filter = draw->filter;
            source_enemy_first_decode_failure_pool = draw->pool;
            source_enemy_first_decode_failure_source_index =
                draw->source_index;
            source_enemy_first_decode_failure_definition =
                draw->enemy_definition_id;
            source_enemy_first_decode_failure_level_position =
                level_position;
            source_enemy_first_decode_failure_x = draw->x;
            source_enemy_first_decode_failure_y = draw->y;
            source_enemy_first_decode_failure_cycle =
                draw->enemy_cycle;
            source_enemy_first_decode_failure_shape_banks =
                (u32)source_parity_level.shape_bank[0] |
                ((u32)source_parity_level.shape_bank[1] << 8) |
                ((u32)source_parity_level.shape_bank[2] << 16) |
                ((u32)source_parity_level.shape_bank[3] << 24);
        }
        telemetry_source_unknown_visuals++;
        telemetry_sprite2_decode_failures++;
        source_enemy_cache[selected].valid = 0;
        return 0xffff;
    }

    /*
     * A drop-frame recovery loop may render again before the next safe
     * VBlank commit. Keep one pending upload per physical VRAM slot and let
     * the newest scene replace its source. Resetting this list per render
     * made a cache entry look resident even though its tiles never reached
     * VRAM.
     */
    for (index = 0; index < source_enemy_upload_count; index++) {
        if (source_enemy_uploads[index].slot == selected) {
            source_enemy_uploads[index].l2_slot = l2_slot;
            telemetry_sprite2_upload_coalesces++;
            return source_enemy_cache_tile(selected);
        }
    }
    if (source_enemy_upload_count >= SOURCE_ENEMY_CACHE_SLOT_COUNT) {
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
        source_enemy_cache_record_drop(
            draw,
            SOURCE_ENEMY_CACHE_DROP_UPLOAD_LIMIT
        );
#endif
        telemetry_sprite2_cache_drops++;
        source_enemy_cache[selected].valid = 0;
        return 0xffff;
    }
    source_enemy_uploads[source_enemy_upload_count].slot = selected;
    source_enemy_uploads[source_enemy_upload_count].l2_slot = l2_slot;
    source_enemy_upload_count++;
    if (
        source_enemy_upload_count +
            (source_player_upload_pending ? 1u : 0u) >
        telemetry_sprite2_max_uploads
    ) {
        telemetry_sprite2_max_uploads =
            source_enemy_upload_count +
                (source_player_upload_pending ? 1u : 0u);
    }
    return source_enemy_cache_tile(selected);
}

static void source_enemy_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_enemy_upload_count; index++) {
        const SourceEnemyUpload *upload = &source_enemy_uploads[index];
        const u8 *source =
            source_sprite2_l2_tiles[upload->l2_slot];
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            source_enemy_cache_tile(upload->slot) * 32;
        u32 bytes = source_enemy_cache_slot_is_compact(upload->slot) ?
            SOURCE_ENEMY_COMPACT_FRAME_BYTES :
            SOURCE_ENEMY_FRAME_BYTES;

        if (source_enemy_cache_tile_is_split(
                source_enemy_cache_tile(upload->slot)
            )) {
#if !TYRIAN_GBA_STRESS_LOADOUT
            /* L2 is 4x4 8bpp tile order: each 32x16 half is 512 B. */
            dmaCopy(
                source,
                (u8 *)OBJ_BASE_ADR +
                    SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE * 32u,
                SOURCE_ENEMY_CACHE_SPLIT_HALF_BYTES
            );
            dmaCopy(
                source + SOURCE_ENEMY_CACHE_SPLIT_HALF_BYTES,
                (u8 *)OBJ_BASE_ADR +
                    SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE * 32u,
                SOURCE_ENEMY_CACHE_SPLIT_HALF_BYTES
            );
            telemetry_sprite2_split_uploads++;
#endif
        } else if (source_enemy_cache_slot_is_compact(upload->slot)) {
            /*
             * The compact 16x16 slot is canvas (8,8)..(23,23), i.e. tiles
             * 5,6,9,10 of the L2's ordinary 4x4 tile-order frame.
             */
            dmaCopy(source + 5 * 64, target, 64);
            dmaCopy(source + 6 * 64, target + 64, 64);
            dmaCopy(source + 9 * 64, target + 128, 64);
            dmaCopy(source + 10 * 64, target + 192, 64);
        } else {
            dmaCopy(source, target, bytes);
        }
        telemetry_sprite2_uploads++;
        if (source_enemy_cache_slot_is_compact(upload->slot)) {
            telemetry_sprite2_compact_uploads++;
        }
        telemetry_sprite2_upload_bytes += bytes;
    }
    source_enemy_upload_count = 0;
}

typedef struct {
    u32 last_epoch;
    u16 graphic;
    u8 shape_table;
    u8 valid;
} SourceProjectileCacheSlot;

typedef struct {
    u8 slot;
    u8 l2_slot;
} SourceProjectileUpload;

static SourceProjectileCacheSlot source_projectile_cache[
    SOURCE_PROJECTILE_CACHE_SLOT_COUNT
] EWRAM_BSS;
static SourceProjectileUpload source_projectile_uploads[
    SOURCE_PROJECTILE_CACHE_SLOT_COUNT
] EWRAM_BSS;
static u32 source_projectile_cache_epoch;
static u8 source_projectile_upload_count;

static u16 source_projectile_cache_tile(u8 slot)
{
    if (slot < SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT) {
        return (u16)(
            SOURCE_PROJECTILE_CACHE_LOWER_TILE_BASE +
            slot * SOURCE_PROJECTILE_TILES_PER_SLOT
        );
    }
    if (
        slot >=
        SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT +
            SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT
    ) {
        return (u16)(
            SOURCE_PROJECTILE_CACHE_EXTRA_TILE_BASE +
            (
                slot -
                SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT -
                SOURCE_PROJECTILE_CACHE_UPPER_SLOT_COUNT
            ) * SOURCE_PROJECTILE_TILES_PER_SLOT
        );
    }
    return (u16)(
        SOURCE_PROJECTILE_CACHE_UPPER_TILE_BASE +
        (
            slot - SOURCE_PROJECTILE_CACHE_LOWER_SLOT_COUNT
        ) * SOURCE_PROJECTILE_TILES_PER_SLOT
    );
}

static void source_projectile_cache_begin_frame(void)
{
    source_projectile_cache_epoch++;
    if (source_projectile_cache_epoch == 0) {
        u8 index;

        source_projectile_cache_epoch = 1;
        for (
            index = 0;
            index < SOURCE_PROJECTILE_CACHE_SLOT_COUNT;
            index++
        ) {
            source_projectile_cache[index].last_epoch = 0;
        }
    }
}

static u16 source_projectile_cache_acquire(
    u8 shape_table,
    u16 graphic,
    u8 presentation_eligible
)
{
    u8 index;
    u8 free_slot = SOURCE_PROJECTILE_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_PROJECTILE_CACHE_SLOT_COUNT;
    u8 selected;
    u8 l2_slot;
    u32 oldest_epoch = 0xffffffffu;

#if TYRIAN_GBA_STRESS_LOADOUT
    if (presentation_eligible) {
        telemetry_projectile_post_visibility_acquires++;
    }
#else
    (void)presentation_eligible;
#endif
    if (!source_enemy_palette_valid || graphic == 0) {
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    for (
        index = 0;
        index < SOURCE_PROJECTILE_CACHE_SLOT_COUNT;
        index++
    ) {
        SourceProjectileCacheSlot *slot =
            &source_projectile_cache[index];

        if (
            slot->valid &&
            slot->shape_table == shape_table &&
            slot->graphic == graphic
        ) {
            slot->last_epoch = source_projectile_cache_epoch;
            telemetry_projectile_cache_hits++;
            return source_projectile_cache_tile(index);
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_PROJECTILE_CACHE_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_projectile_cache_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != SOURCE_PROJECTILE_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == SOURCE_PROJECTILE_CACHE_SLOT_COUNT) {
        telemetry_projectile_cache_drops++;
#if TYRIAN_GBA_STRESS_LOADOUT
        if (presentation_eligible) {
            telemetry_projectile_visible_capacity_drops++;
        }
#endif
        return 0xffff;
    }
    if (source_projectile_cache[selected].valid) {
        telemetry_projectile_cache_evictions++;
    }
    source_projectile_cache[selected].valid = 1;
    source_projectile_cache[selected].shape_table = shape_table;
    source_projectile_cache[selected].graphic = graphic;
    source_projectile_cache[selected].last_epoch =
        source_projectile_cache_epoch;
    telemetry_projectile_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        shape_table,
        graphic,
        0,
        0
    );
    if (l2_slot == 0xff) {
        source_projectile_cache[selected].valid = 0;
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    for (index = 0; index < source_projectile_upload_count; index++) {
        if (source_projectile_uploads[index].slot == selected) {
            source_projectile_uploads[index].l2_slot = l2_slot;
            telemetry_projectile_cache_upload_coalesces++;
            return source_projectile_cache_tile(selected);
        }
    }
    if (
        source_projectile_upload_count >=
        SOURCE_PROJECTILE_CACHE_SLOT_COUNT
    ) {
        source_projectile_cache[selected].valid = 0;
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    source_projectile_uploads[
        source_projectile_upload_count
    ].slot = selected;
    source_projectile_uploads[
        source_projectile_upload_count
    ].l2_slot = l2_slot;
    source_projectile_upload_count++;
    if (
        source_projectile_upload_count >
        telemetry_projectile_cache_max_uploads
    ) {
        telemetry_projectile_cache_max_uploads =
            source_projectile_upload_count;
    }
    return source_projectile_cache_tile(selected);
}

static void source_projectile_cache_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_PROJECTILE_CACHE_SLOT_COUNT;
        index++
    ) {
        if (
            source_projectile_cache[index].valid &&
            source_projectile_cache[index].last_epoch ==
                source_projectile_cache_epoch
        ) {
            visible_unique++;
        }
    }
    if (
        visible_unique >
        telemetry_projectile_cache_max_visible_unique
    ) {
        telemetry_projectile_cache_max_visible_unique =
            visible_unique;
    }
}

static void source_projectile_cache_commit(void)
{
    u8 index;

    for (
        index = 0;
        index < source_projectile_upload_count;
        index++
    ) {
        u8 slot = source_projectile_uploads[index].slot;
        u8 l2_slot = source_projectile_uploads[index].l2_slot;

        dmaCopy(
            source_sprite2_l2_tiles[l2_slot],
            (u8 *)OBJ_BASE_ADR +
                source_projectile_cache_tile(slot) * 32,
            SOURCE_PROJECTILE_FRAME_BYTES
        );
        telemetry_projectile_cache_uploads++;
    }
    source_projectile_upload_count = 0;
}

typedef struct {
    u8 slot;
    u8 frame_key;
} SourceEffectUpload;

#define SOURCE_EFFECT_FRAME_KEY_INVALID 0xffu

/*
 * A frame key is only 0..107, so 0xff is an unambiguous invalid marker.
 * Keeping the keys and epochs in separate arrays removes the padded valid
 * byte from every slot (108 -> 82 bytes including natural alignment).  The
 * saved EWRAM is deliberately traded for additional IWRAM user-stack room.
 */
static u16 source_effect_cache_last_epoch[
    SOURCE_EFFECT_CACHE_SLOT_COUNT
] EWRAM_BSS;
static u8 source_effect_cache_frame_key[
    SOURCE_EFFECT_CACHE_SLOT_COUNT
] EWRAM_BSS;
static SourceEffectUpload source_effect_uploads[
    SOURCE_EFFECT_CACHE_SLOT_COUNT
] EWRAM_BSS;
/*
 * Effect hits dominate explosion-heavy routes. Keep a direct mutable
 * frame-key index so a hit is O(1); only misses scan the 27 physical/logical
 * slots for an eviction candidate.  The 108-byte table lives in EWRAM:
 * retaining the larger hot acquire/prefetch routines in IWRAM saves far more
 * cycles while preserving the measured user-stack floor.
 * Zero means absent, otherwise the stored value is slot + 1.
 */
static u8 source_effect_frame_slot[
    OBJ_EXPLOSION_SEQUENCE_COUNT * OBJ_EXPLOSION_FRAME_COUNT
] EWRAM_BSS;
static u16 source_effect_cache_epoch;
static u8 source_effect_upload_count;
static u8 source_effect_boss_bar_tiles_dirty EWRAM_BSS;
static u8 source_effect_reward_tiles_dirty EWRAM_BSS;
static u8 source_effect_reward_tiles_required EWRAM_BSS;
static u8 source_effect_status_tiles_dirty EWRAM_BSS;
static u8 source_effect_status_tiles_required EWRAM_BSS;

static u8 source_effect_boss_bar_active(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    return 0;
#else
    return
        source_parity_level.boss_bar_link[0] != 0 ||
        source_parity_level.boss_bar_link[1] != 0;
#endif
}

static u8 source_effect_legacy_reward_frame_active(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT || \
    !defined(AUTOTEST_REWARD_VISUAL_TEST)
    return 0;
#else
    u8 index;

    for (index = 0; index < MAX_REWARDS; index++) {
        if (
            rewards[index].active &&
            rewards[index].sequence == REWARD_SEQUENCE_COUNT - 1u &&
            rewards[index].frame == REWARD_FRAME_COUNT - 1u
        ) {
            return 1;
        }
    }
    return 0;
#endif
}

static u8 source_effect_status_label_active(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    return 0;
#else
    return
        game_state == STATE_GAME_OVER ||
        (
            game_state == STATE_PLAY &&
            (
                source_parity_level.display_time != 0 ||
                frontend_demo_active
            )
        );
#endif
}

static void source_effect_restore_shared_tiles_if_needed(void)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (
        source_effect_boss_bar_tiles_dirty &&
        source_effect_boss_bar_active()
    ) {
        dmaCopy(
            obj_tiles + OBJ_TILE_BOSS_BAR * 32u,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_BOSS_BAR * 32u,
            SOURCE_EFFECT_FRAME_BYTES
        );
        source_effect_boss_bar_tiles_dirty = 0;
    }
    if (
        source_effect_reward_tiles_dirty &&
        source_effect_reward_tiles_required
    ) {
        dmaCopy(
            obj_tiles + SOURCE_EFFECT_CACHE_REWARD_TILE_BASE * 32u,
            (u8 *)OBJ_BASE_ADR +
                SOURCE_EFFECT_CACHE_REWARD_TILE_BASE * 32u,
            SOURCE_EFFECT_FRAME_BYTES
        );
        source_effect_reward_tiles_dirty = 0;
    }
    if (
        source_effect_status_tiles_dirty &&
        source_effect_status_tiles_required
    ) {
        const u8 *source = 0;

        if (game_state == STATE_GAME_OVER) {
            source = obj_tiles + OBJ_TILE_GAME_OVER_SOURCE * 32u;
        } else if (source_parity_level.display_time != 0) {
            source = obj_tiles + OBJ_TILE_SECRET_LEVEL_SOURCE * 32u;
        } else if (frontend_demo_active) {
            source = obj_tiles + OBJ_TILE_INSERT_COIN_SOURCE * 32u;
        }
        if (source != 0) {
            dmaCopy(
                source + 8u * 32u,
                (u8 *)OBJ_BASE_ADR +
                    SOURCE_EFFECT_CACHE_STATUS_TILE_BASE * 32u,
                SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT *
                    SOURCE_EFFECT_FRAME_BYTES
            );
        }
        source_effect_status_tiles_dirty = 0;
    }
#endif
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static void source_effect_cancel_pending_slot(u8 target_slot)
{
    u8 index;

    for (index = 0; index < source_effect_upload_count; index++) {
        if (
            source_effect_uploads[index].slot == target_slot
        ) {
            u8 move;

            for (
                move = index;
                move + 1u < source_effect_upload_count;
                move++
            ) {
                source_effect_uploads[move] =
                    source_effect_uploads[move + 1u];
            }
            source_effect_upload_count--;
            telemetry_effect_cache_upload_coalesces++;
            return;
        }
    }
}
#endif

static u8 source_effect_cache_slot_reserved(u8 slot)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    (void)slot;
    return 0;
#else
    return
        (
            slot == SOURCE_EFFECT_CACHE_BOSS_SLOT &&
            source_effect_boss_bar_active()
        ) ||
        (
            slot == SOURCE_EFFECT_CACHE_REWARD_SLOT &&
            source_effect_reward_tiles_required
        ) ||
        (
            slot >= SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT &&
            slot <
                SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT +
                    SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT &&
            source_effect_status_tiles_required
        );
#endif
}

static u16 source_effect_cache_tile(u8 slot)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (slot == SOURCE_EFFECT_CACHE_OVERFLOW_SLOT) {
        return SOURCE_EFFECT_CACHE_OVERFLOW_TILE_BASE;
    }
    if (slot == SOURCE_EFFECT_CACHE_BOSS_SLOT) {
        return SOURCE_EFFECT_CACHE_BOSS_TILE_BASE;
    }
    if (slot == SOURCE_EFFECT_CACHE_REWARD_SLOT) {
        return SOURCE_EFFECT_CACHE_REWARD_TILE_BASE;
    }
    if (
        slot >= SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT &&
        slot <
            SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT +
                SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT
    ) {
        return (u16)(
            SOURCE_EFFECT_CACHE_STATUS_TILE_BASE +
            (slot - SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT) *
                SOURCE_EFFECT_TILES_PER_SLOT
        );
    }
#endif
    return (u16)(
        SOURCE_EFFECT_CACHE_TILE_BASE +
        slot * SOURCE_EFFECT_TILES_PER_SLOT
    );
}

static void source_effect_cache_begin_frame(void)
{
    source_effect_cache_epoch++;
#if !TYRIAN_GBA_STRESS_LOADOUT
    source_effect_reward_tiles_required =
        source_effect_legacy_reward_frame_active();
    source_effect_status_tiles_required =
        source_effect_status_label_active();
    if (source_effect_boss_bar_active()) {
        u8 frame_key = source_effect_cache_frame_key[
            SOURCE_EFFECT_CACHE_BOSS_SLOT];

        if (
            frame_key != SOURCE_EFFECT_FRAME_KEY_INVALID &&
            source_effect_frame_slot[frame_key] ==
                SOURCE_EFFECT_CACHE_BOSS_SLOT + 1u
        ) {
            source_effect_frame_slot[frame_key] = 0;
        }
        source_effect_cache_frame_key[
            SOURCE_EFFECT_CACHE_BOSS_SLOT
        ] = SOURCE_EFFECT_FRAME_KEY_INVALID;
        source_effect_cancel_pending_slot(
            SOURCE_EFFECT_CACHE_BOSS_SLOT
        );
    }
    if (source_effect_reward_tiles_required) {
        u8 frame_key = source_effect_cache_frame_key[
            SOURCE_EFFECT_CACHE_REWARD_SLOT];

        if (
            frame_key != SOURCE_EFFECT_FRAME_KEY_INVALID &&
            source_effect_frame_slot[frame_key] ==
                SOURCE_EFFECT_CACHE_REWARD_SLOT + 1u
        ) {
            source_effect_frame_slot[frame_key] = 0;
        }
        source_effect_cache_frame_key[
            SOURCE_EFFECT_CACHE_REWARD_SLOT
        ] = SOURCE_EFFECT_FRAME_KEY_INVALID;
        source_effect_cancel_pending_slot(
            SOURCE_EFFECT_CACHE_REWARD_SLOT
        );
    }
    if (source_effect_status_tiles_required) {
        u8 slot;

        for (
            slot = SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT;
            slot <
                SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT +
                    SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT;
            slot++
        ) {
            u8 frame_key = source_effect_cache_frame_key[slot];

            if (frame_key != SOURCE_EFFECT_FRAME_KEY_INVALID) {
                if (source_effect_frame_slot[frame_key] == slot + 1u) {
                    source_effect_frame_slot[frame_key] = 0;
                }
                source_effect_status_tiles_dirty = 1;
            }
            source_effect_cache_frame_key[slot] =
                SOURCE_EFFECT_FRAME_KEY_INVALID;
            source_effect_cancel_pending_slot(slot);
        }
    }
#endif
    if (source_effect_cache_epoch == 0) {
        u8 index;

        source_effect_cache_epoch = 1;
        for (
            index = 0;
            index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
            index++
        ) {
            source_effect_cache_last_epoch[index] = 0;
        }
    }
}

static void source_effect_cache_end_frame(void)
{
    u8 index;
    u8 visible_unique = 0;

    for (
        index = 0;
        index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
        index++
    ) {
        if (
            source_effect_cache_frame_key[index] !=
                SOURCE_EFFECT_FRAME_KEY_INVALID &&
            source_effect_cache_last_epoch[index] ==
                source_effect_cache_epoch
        ) {
            visible_unique++;
        }
    }
    if (
        visible_unique >
        telemetry_effect_cache_max_visible_unique
    ) {
        telemetry_effect_cache_max_visible_unique = visible_unique;
    }
}

static __attribute__((noinline, noclone)) u16
source_effect_cache_acquire(u8 sequence, u8 frame)
{
    u8 frame_key;
    u8 index;
    u8 free_slot = SOURCE_EFFECT_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_EFFECT_CACHE_SLOT_COUNT;
    u16 oldest_epoch = 0xffffu;
    u8 selected;
    u8 mapped_slot;

    if (
        sequence >= OBJ_EXPLOSION_SEQUENCE_COUNT ||
        frame >= OBJ_EXPLOSION_FRAME_COUNT
    ) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    frame_key = (u8)(
        sequence * OBJ_EXPLOSION_FRAME_COUNT + frame
    );
    mapped_slot = source_effect_frame_slot[frame_key];
    if (mapped_slot != 0) {
        index = (u8)(mapped_slot - 1u);
        if (
            index < SOURCE_EFFECT_CACHE_SLOT_COUNT &&
            !source_effect_cache_slot_reserved(index)
        ) {
            if (source_effect_cache_frame_key[index] == frame_key) {
                source_effect_cache_last_epoch[index] =
                    source_effect_cache_epoch;
                telemetry_effect_cache_hits++;
                return source_effect_cache_tile(index);
            }
        }
        /* A time-shared bank was restored or the entry was superseded. */
        source_effect_frame_slot[frame_key] = 0;
    }
    for (
        index = 0;
        index < SOURCE_EFFECT_CACHE_SLOT_COUNT;
        index++
    ) {
        if (source_effect_cache_slot_reserved(index)) continue;
        if (
            source_effect_cache_frame_key[index] ==
            SOURCE_EFFECT_FRAME_KEY_INVALID
        ) {
            if (free_slot == SOURCE_EFFECT_CACHE_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            source_effect_cache_last_epoch[index] !=
                source_effect_cache_epoch &&
            source_effect_cache_last_epoch[index] < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = source_effect_cache_last_epoch[index];
        }
    }
    selected = free_slot != SOURCE_EFFECT_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
    if (selected == SOURCE_EFFECT_CACHE_SLOT_COUNT) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    if (
        source_effect_cache_frame_key[selected] !=
        SOURCE_EFFECT_FRAME_KEY_INVALID
    ) {
        u8 old_frame_key = source_effect_cache_frame_key[selected];

        if (
            source_effect_frame_slot[old_frame_key] == selected + 1u
        ) {
            source_effect_frame_slot[old_frame_key] = 0;
        }
        telemetry_effect_cache_evictions++;
    }
    for (index = 0; index < source_effect_upload_count; index++) {
        if (source_effect_uploads[index].slot == selected) {
            source_effect_cache_frame_key[selected] = frame_key;
            source_effect_cache_last_epoch[selected] =
                source_effect_cache_epoch;
            source_effect_frame_slot[frame_key] = selected + 1u;
            source_effect_uploads[index].frame_key = frame_key;
            telemetry_effect_cache_misses++;
            telemetry_effect_cache_upload_coalesces++;
            return source_effect_cache_tile(selected);
        }
    }
    if (source_effect_upload_count >= SOURCE_EFFECT_CACHE_SLOT_COUNT) {
        telemetry_effect_cache_drops++;
        return 0xffff;
    }
    source_effect_cache_frame_key[selected] = frame_key;
    source_effect_cache_last_epoch[selected] =
        source_effect_cache_epoch;
    source_effect_frame_slot[frame_key] = selected + 1u;
    source_effect_uploads[source_effect_upload_count].slot = selected;
    source_effect_uploads[source_effect_upload_count].frame_key =
        frame_key;
    source_effect_upload_count++;
    telemetry_effect_cache_misses++;
    if (
        source_effect_upload_count >
        telemetry_effect_cache_max_uploads
    ) {
        telemetry_effect_cache_max_uploads =
            source_effect_upload_count;
    }
    return source_effect_cache_tile(selected);
}

static void source_effect_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_effect_upload_count; index++) {
        const SourceEffectUpload *upload =
            &source_effect_uploads[index];
        const u8 *source =
            obj_tiles +
            (
                OBJ_TILE_EXPLOSION +
                upload->frame_key *
                    OBJ_EXPLOSION_TILES_PER_FRAME
            ) * 32;
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            source_effect_cache_tile(upload->slot) * 32;

        dmaCopy(source, target, SOURCE_EFFECT_FRAME_BYTES);
#if !TYRIAN_GBA_STRESS_LOADOUT
        if (upload->slot == SOURCE_EFFECT_CACHE_BOSS_SLOT) {
            source_effect_boss_bar_tiles_dirty = 1;
        }
        if (upload->slot == SOURCE_EFFECT_CACHE_REWARD_SLOT) {
            source_effect_reward_tiles_dirty = 1;
        }
        if (
            upload->slot >= SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT &&
            upload->slot <
                SOURCE_EFFECT_CACHE_STATUS_FIRST_SLOT +
                    SOURCE_EFFECT_CACHE_STATUS_SLOT_COUNT
        ) {
            source_effect_status_tiles_dirty = 1;
        }
#endif
        telemetry_effect_cache_uploads++;
        telemetry_effect_cache_upload_bytes +=
            SOURCE_EFFECT_FRAME_BYTES;
    }
    source_effect_upload_count = 0;
}

static s16 source_camera_target_offset(
    s16 player,
    s16 centre,
    s16 dead_zone_half,
    s16 slack
)
{
    s16 relative = (s16)(player - centre);
    s16 target = 0;

    if (relative > dead_zone_half) {
        target = (s16)(relative - dead_zone_half);
    } else if (relative < -dead_zone_half) {
        target = (s16)(relative + dead_zone_half);
    }
    if (target < -slack) target = (s16)-slack;
    if (target > slack) target = slack;
    return target;
}

static s16 source_camera_smooth_axis(
    s16 current_q8,
    s16 target
)
{
    s16 target_q8 = (s16)(target * 256);
    s16 delta = (s16)(target_q8 - current_q8);
    s16 step;

    if (delta > 0) {
        step = (s16)(delta >> TYRIAN_GBA_CAMERA_RESPONSE_SHIFT);
        if (step == 0) step = 1;
    } else if (delta < 0) {
        step = (s16)(
            -(
                (s16)(-delta) >>
                TYRIAN_GBA_CAMERA_RESPONSE_SHIFT
            )
        );
        if (step == 0) step = -1;
    } else {
        return current_q8;
    }
    current_q8 = (s16)(current_q8 + step);
    if (
        (delta > 0 && current_q8 > target_q8) ||
        (delta < 0 && current_q8 < target_q8)
    ) {
        current_q8 = target_q8;
    }
    return current_q8;
}

static s8 source_camera_q8_to_pixel(s16 value)
{
    if (value >= 0) {
        return (s8)((value + 128) >> 8);
    }
    return (s8)(-(((-value) + 128) >> 8));
}

static void source_camera_reset(void)
{
    source_camera_offset_x_q8 = 0;
    source_camera_offset_y_q8 = 0;
    source_camera_offset_x = 0;
    source_camera_offset_y = 0;
}

static void source_camera_update(void)
{
#if TYRIAN_GBA_SOFT_CROP_CAMERA
    s16 target_x = source_camera_target_offset(
        player_source_x,
        TYRIAN_GBA_CAMERA_SOURCE_CENTER_X,
        TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_X,
        SOURCE_VIEW_CROP_X
    );
    s16 target_y = source_camera_target_offset(
        player_source_y,
        TYRIAN_GBA_CAMERA_SOURCE_CENTER_Y,
        TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_Y,
        SOURCE_VIEW_CROP_Y
    );

    s16 next_x_q8 = source_camera_smooth_axis(
        source_camera_offset_x_q8,
        target_x
    );
    s16 next_y_q8 = source_camera_smooth_axis(
        source_camera_offset_y_q8,
        target_y
    );
    s8 next_y = source_camera_q8_to_pixel(next_y_q8);

    source_camera_offset_x_q8 = next_x_q8;
    source_camera_offset_x = source_camera_q8_to_pixel(next_x_q8);
    /*
     * Camera state is authoritative logic.  The presentation pass later
     * materializes the final complete background window and publishes VOFS
     * atomically, so skipped frames no longer build intermediate camera rows.
     */
    source_camera_offset_y_q8 = next_y_q8;
    source_camera_offset_y = next_y;
    if (
        (u32)(u16)source_camera_origin_x() <
            telemetry_camera_min_origin_x
    ) {
        telemetry_camera_min_origin_x =
            (u32)(u16)source_camera_origin_x();
    }
    if (
        (u32)(u16)source_camera_origin_x() >
            telemetry_camera_max_origin_x
    ) {
        telemetry_camera_max_origin_x =
            (u32)(u16)source_camera_origin_x();
    }
    if (
        (u32)(u16)source_camera_origin_y() <
            telemetry_camera_min_origin_y
    ) {
        telemetry_camera_min_origin_y =
            (u32)(u16)source_camera_origin_y();
    }
    if (
        (u32)(u16)source_camera_origin_y() >
            telemetry_camera_max_origin_y
    ) {
        telemetry_camera_max_origin_y =
            (u32)(u16)source_camera_origin_y();
    }
#else
    source_camera_reset();
#endif
}

static s16 source_camera_origin_x(void)
{
    return (s16)(
        SOURCE_PRESENTATION_X_ORIGIN + source_camera_offset_x
    );
}

static s16 source_camera_origin_y(void)
{
    return (s16)(
        SOURCE_PRESENTATION_Y_ORIGIN + source_camera_offset_y
    );
}

static s16 source_world_to_screen_x(s16 source_x)
{
    return (s16)(source_x - source_camera_origin_x());
}

static s16 source_world_to_screen_y(s16 source_y)
{
    return (s16)(source_y - source_camera_origin_y());
}

static u16 source_background_hofs(u8 layer)
{
    const OtLevelPortState *state = &source_parity_level;

    /*
     * The initial PC frame deliberately uses mapXbpPos=0.  Later frames use
     * the pointer/position pair calculated by JE_mainGamePlayerFunctions().
     * Since the GBA map contains the complete unscaled PC raster, HOFS is the
     * exact source-map pixel at game_screen x=36.
     */
    if (!state->presentation_parallax_initialized) {
        return (u16)(
            (layer == 2 ? BG3_INITIAL_HOFS : BG12_INITIAL_HOFS) +
            source_camera_offset_x
        );
    }
    if (layer == 0) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x_offset +
            source_camera_offset_x
        );
    }
    if (layer == 1) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x2_offset +
            source_camera_offset_x
        );
    }
    if (state->presentation_background3_x1) {
        return (u16)(
            SOURCE_BG12_PARALLAX_BASE_X -
            state->presentation_map_x3_offset +
            source_camera_offset_x
        );
    }
    return (u16)(
        SOURCE_BG3_PARALLAX_BASE_X -
        state->presentation_map_x3_offset +
        source_camera_offset_x
    );
}

static u16 source_background_vofs(u8 layer)
{
    return (u16)(
        background_presentation_scroll_pixel(
            layer,
            source_camera_offset_y
        ) &
        255u
    );
}

static s16 source_player_screen_x(void)
{
    return (s16)(
        source_world_to_screen_x((s16)(player_source_x + 7)) - 16
    );
}

static s16 source_player_screen_y(void)
{
    return (s16)(
        source_world_to_screen_y(
            (s16)(
                player_source_y +
                SOURCE_PLAYER_PRESENTATION_CENTRE_Y
            )
        ) - 16
    );
}

static s16 source_enemy_draw_screen_x(
    const OtEnemyDrawCommand *draw
)
{
    return (s16)(
        source_world_to_screen_x((s16)(draw->x + 6)) - 16
    );
}

static s16 source_enemy_draw_screen_y(
    const OtEnemyDrawCommand *draw
)
{
    return (s16)(
        source_world_to_screen_y((s16)(draw->y + 7)) - 16
    );
}

#ifdef AUTOTEST
/*
 * The deterministic test pilot steers toward the live translated enemy slot,
 * after JE_drawEnemy has advanced it.  Presentation itself never uses these
 * helpers: it consumes the pre-update OtEnemyDrawCommand above.
 */
static s16 source_score_item_screen_x(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_x((s16)(enemy->ex + 6)) - 8
    );
}

static s16 source_score_item_screen_y(const OtEnemy *enemy)
{
    return (s16)(
        source_world_to_screen_y((s16)(enemy->ey + 7)) - 8
    );
}
#endif

#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
static s16 source_player_shot_screen_x(const PlayerShot *shot)
{
    return source_world_to_screen_x((s16)(shot->x + 1));
}

static s16 source_player_shot_screen_y(const PlayerShot *shot)
{
    return source_world_to_screen_y(shot->y);
}
#endif

static s16 source_enemy_shot_screen_x(const OtEnemyShot *shot)
{
    return source_world_to_screen_x(shot->sx);
}

static s16 source_enemy_shot_screen_y(const OtEnemyShot *shot)
{
    return source_world_to_screen_y(shot->sy);
}

typedef struct {
    u16 sprite;
    u8 ttl;
} SourceExplosionDefinition;

enum {
    SOURCE_REPEATING_EXPLOSION_COUNT = 20,
};

/* Direct fixed-width counterpart of varz.h rep_explosion_type. */
typedef struct {
    u16 delay;
    u16 ttl;
    s16 x;
    s16 y;
    u8 big;
} SourceRepeatingExplosion;

static SourceRepeatingExplosion source_repeating_explosion[
    SOURCE_REPEATING_EXPLOSION_COUNT
] EWRAM_DATA;

/*
 * Direct JE_setupExplosion() table from OpenTyrian varz.c.  For a fixed
 * pickup label the PC renderer does not increment sprite; blit_sprite2()
 * receives sprite + 1 on every tick.
 */
static const SourceExplosionDefinition source_explosion_definition[53] = {
    { 144,  7 }, { 120, 12 }, { 190, 12 }, { 209, 12 },
    { 152, 12 }, { 171, 12 }, { 133,  7 }, {   1, 12 },
    {  20, 12 }, {  39, 12 }, {  58, 12 }, { 110,  3 },
    {  76,  7 }, {  91,  3 }, { 227,  3 }, { 230,  3 },
    { 233,  3 }, { 252,  3 }, { 246,  3 }, { 249,  3 },
    { 265,  3 }, { 268,  3 }, { 271,  3 }, { 236,  3 },
    { 239,  3 }, { 242,  3 }, { 261,  3 }, { 274,  3 },
    { 277,  3 }, { 280,  3 }, { 299,  3 }, { 284,  3 },
    { 287,  3 }, { 290,  3 }, { 293,  3 }, { 165,  8 },
    { 184,  8 }, { 203,  8 }, { 222,  8 }, { 168,  8 },
    { 187,  8 }, { 206,  8 }, { 225, 10 }, { 169, 10 },
    { 188, 10 }, { 207, 20 }, { 226, 14 }, { 170, 14 },
    { 189, 14 }, { 208, 14 }, { 246, 14 }, { 227, 14 },
    { 265, 14 },
};

static void source_spawn_explosion(
    s16 x,
    s16 y,
    s8 delta_y,
    u8 type,
    u8 fixed_position
)
{
    u8 index;

    if (y <= -16 || y >= 190) return;
    if (type == 6) {
        y = (s16)(y + 12);
        x = (s16)(x + 2);
    } else if (type == 98) {
        type = 6;
    }
    if (
        type >=
        sizeof(source_explosion_definition) /
            sizeof(source_explosion_definition[0])
    ) {
        telemetry_pickup_explosion_drops++;
        return;
    }
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        PickupExplosion *explosion = &pickup_explosions[index];

        if (explosion->active) continue;
        explosion->active = 1;
        explosion->x = x;
        explosion->y = y;
        explosion->graphic =
            source_explosion_definition[type].sprite;
        explosion->ttl = source_explosion_definition[type].ttl;
        explosion->fixed_position = fixed_position;
        explosion->delta_y = delta_y;
        active_pickup_explosion_count++;
        telemetry_pickup_explosion_spawns++;
        if (
            active_pickup_explosion_count >
            telemetry_pickup_explosion_max_active
        ) {
            telemetry_pickup_explosion_max_active =
                active_pickup_explosion_count;
        }
        return;
    }
    telemetry_pickup_explosion_drops++;
}

static void source_spawn_fixed_pickup_explosion(
    const OtPickupEffect *effect
)
{
    source_spawn_explosion(
        effect->x,
        effect->y,
        0,
        effect->explosion_type,
        1
    );
}

static void source_update_explosions(void)
{
    u8 index;

    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        PickupExplosion *explosion = &pickup_explosions[index];

        if (!explosion->active) continue;
        if (!explosion->fixed_position) {
            s16 explode_move =
                source_parity_level.back_move2 > 0 ?
                    source_parity_level.back_move2 :
                    source_parity_level.back_move;

            explosion->graphic++;
            explosion->y = (s16)(explosion->y + explode_move);
        }
        explosion->y = (s16)(explosion->y + explosion->delta_y);
        if (explosion->y > 186) {
            explosion->ttl = 0;
        }
        if (explosion->ttl > 0) explosion->ttl--;
        if (explosion->ttl == 0) {
            explosion->active = 0;
            if (active_pickup_explosion_count) {
                active_pickup_explosion_count--;
            }
        }
    }
}

static void source_spawn_large_explosion_visual(
    bool ground,
    s16 x,
    s16 y
)
{
    s16 centre_x;
    s16 centre_y;

    if (y < 0) return;
    centre_x = source_world_to_screen_x((s16)(x + 6));
    centre_y = source_world_to_screen_y((s16)(y + 7));
    spawn_large_effect(centre_x, centre_y, ground);
}

static void source_setup_large_explosion(
    bool ground,
    u8 repeat_count,
    s16 x,
    s16 y
)
{
    u8 index;
    u8 big = 0;

    if (y < 0) return;
    source_spawn_large_explosion_visual(ground, x, y);
    if (repeat_count > 10) {
        repeat_count = (u8)(repeat_count - 10u);
        big = 1;
    }
    if (repeat_count == 0) return;
    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        SourceRepeatingExplosion *repeating =
            &source_repeating_explosion[index];

        if (repeating->ttl != 0) continue;
        repeating->ttl = repeat_count;
        repeating->delay = 2;
        repeating->x = x;
        repeating->y = y;
        repeating->big = big;
        return;
    }
}

static u8 source_repeating_explosions_active(void)
{
    u8 index;

    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        if (source_repeating_explosion[index].ttl != 0) return 1;
    }
    return 0;
}

static void source_update_repeating_explosions(void)
{
    u8 index;

    for (index = 0; index < SOURCE_REPEATING_EXPLOSION_COUNT; index++) {
        SourceRepeatingExplosion *repeating =
            &source_repeating_explosion[index];
        s16 x;
        s16 y;

        if (repeating->ttl == 0) continue;
        if (repeating->delay > 0) {
            repeating->delay--;
            continue;
        }
        repeating->y = (s16)(
            repeating->y + source_parity_level.back_move2 + 1
        );
        x = (s16)(
            repeating->x +
            (s16)(ot_level_port_random(&source_parity_level) % 24u) -
            12
        );
        y = (s16)(
            repeating->y +
            (s16)(ot_level_port_random(&source_parity_level) % 27u) -
            24
        );
        if (repeating->big) {
            source_setup_large_explosion(false, 2, x, y);
            if (
                repeating->ttl == 1 ||
                ot_level_port_random(&source_parity_level) % 5u == 1u
            ) {
                source_parity_level.frame_sound_queue[7] = 11;
            } else {
                source_parity_level.frame_sound_queue[6] = 9;
            }
            repeating->delay = (u16)(
                4u + ot_level_port_random(&source_parity_level) % 3u
            );
        } else {
            source_spawn_explosion(x, y, 0, 1, 0);
            source_parity_level.frame_sound_queue[5] = 4;
            repeating->delay = 3;
        }
        repeating->ttl--;
    }
}

static void source_update_random_explosions(void)
{
    if (
        source_parity_level.random_explosions &&
        ot_level_port_random(&source_parity_level) % 10u == 1u
    ) {
        s16 x = (s16)(
            ot_level_port_random(&source_parity_level) % 280u
        );
        s16 y = (s16)(
            ot_level_port_random(&source_parity_level) % 180u
        );

        source_setup_large_explosion(false, 20, x, y);
    }
}

static void source_apply_level_frame_outputs(void)
{
    u8 index;

    if (source_parity_level.pending_text_window != 0) {
        gameplay_overlay_show_event(
            source_parity_level.pending_text_window
        );
        source_parity_level.pending_text_window = 0;
    }
    /* JE_drawEnemy() runs before JE_playerMovement() in the source loop. */
    player_source_velocity_x +=
        source_parity_level.frame_player_velocity_x_delta;
    player_source_velocity_y +=
        source_parity_level.frame_player_velocity_y_delta;
    source_parity_level.frame_player_velocity_x_delta = 0;
    source_parity_level.frame_player_velocity_y_delta = 0;
    for (
        index = 0;
        index < source_parity_level.frame_explosion_count;
        index++
    ) {
        const OtFrameExplosion *request =
            &source_parity_level.frame_explosion[index];

        source_spawn_explosion(
            request->x,
            request->y,
            request->delta_y,
            request->explosion_type,
            request->fixed_position
        );
    }
    source_parity_level.frame_explosion_count = 0;
}

static void source_emit_effect(const OtHitEffect *effect)
{
    s16 centre_x = source_world_to_screen_x((s16)(effect->x + 6));
    s16 centre_y = source_world_to_screen_y((s16)(effect->y + 7));

    if (effect->large) {
        source_setup_large_explosion(
            effect->ground,
            effect->repeat_count,
            effect->x,
            effect->y
        );
    } else {
        spawn_effect(
            (s16)(centre_x - 8),
            (s16)(centre_y - 8),
            EXPLOSION_SEQUENCE_SMALL
        );
    }
}

static void source_play_frame_weapon_sounds(void)
{
    u8 queue_index;

    /*
     * Direct tyrian2.c queue drain: every authored SFX and voice uses its
     * original one-based sndmast.h number.  Multiple occupied channels in
     * the same logic tick remain distinct Maxmod effects.
     */
    for (queue_index = 0; queue_index < 8; queue_index++) {
        source_sound_effect(
            source_parity_level.frame_sound_queue[queue_index]
        );
    }
}

static void source_sync_runtime_telemetry(void)
{
    telemetry_source_events = source_parity_level.event_index;
    telemetry_source_events_applied =
        source_parity_level.applied_event_count;
    telemetry_source_events_deferred =
        source_parity_level.deferred_event_count;
    telemetry_source_events_skipped =
        source_parity_level.skipped_event_count;
    telemetry_source_spawn_attempts =
        source_parity_level.spawn_attempt_count;
    telemetry_source_spawn_successes =
        source_parity_level.spawn_success_count;
    telemetry_source_spawn_pool_full =
        source_parity_level.spawn_pool_full_count;
    telemetry_source_spawn_missing =
        source_parity_level.spawn_missing_definition_count;
    telemetry_source_max_enemies =
        source_parity_level.max_active_enemy_count;
    telemetry_source_control_writes =
        source_parity_level.enemy_control_write_count;
    telemetry_source_rng_calls =
        source_parity_level.rng_call_count;
    telemetry_source_motion_updates =
        source_parity_level.enemy_motion_update_count;
    telemetry_source_releases =
        source_parity_level.enemy_release_count;
    telemetry_source_shot_triggers =
        source_parity_level.enemy_shot_trigger_count;
    telemetry_source_enemy_shots_spawned =
        source_parity_level.enemy_shot_spawn_count;
    telemetry_source_enemy_shot_drops =
        source_parity_level.enemy_shot_drop_count;
    telemetry_source_max_enemy_shots =
        source_parity_level.enemy_shot_max_active_count;
    telemetry_source_enemy_shot_updates =
        source_parity_level.enemy_shot_motion_update_count;
    telemetry_source_enemy_shot_releases =
        source_parity_level.enemy_shot_release_count;
    telemetry_source_enemy_shot_player_hits =
        source_parity_level.enemy_shot_player_hit_count;
    telemetry_source_player_shot_hits =
        source_parity_level.player_shot_collision_count;
    telemetry_source_enemy_kills =
        source_parity_level.enemy_kill_count;
    telemetry_source_direct_cash =
        source_parity_level.direct_cash_awarded;
    telemetry_source_score_item_spawns =
        source_parity_level.score_item_spawn_count;
    telemetry_source_score_item_pickups =
        source_parity_level.score_item_pickup_count;
    telemetry_source_score_item_max_active =
        source_parity_level.score_item_max_active_count;
    telemetry_source_death_spawn_attempts =
        source_parity_level.death_spawn_attempt_count;
    telemetry_source_death_spawn_successes =
        source_parity_level.death_spawn_success_count;
    telemetry_source_death_control_events =
        source_parity_level.death_control_event_count;
    telemetry_source_death_assignments =
        source_parity_level.death_assignment_count;
    telemetry_source_launch_attempts =
        source_parity_level.enemy_launch_attempt_count;
    telemetry_source_launch_successes =
        source_parity_level.enemy_launch_success_count;
    telemetry_source_random_attempts =
        source_parity_level.random_spawn_attempt_count;
    telemetry_source_random_successes =
        source_parity_level.random_spawn_success_count;

    /*
     * Stage 4 makes these formerly legacy-facing counters describe the
     * authoritative source runtime.  The separate source counters above
     * remain available for exact regression auditing.
     */
    telemetry_spawn_count =
        source_parity_level.spawn_success_count +
        source_parity_level.death_spawn_success_count;
    telemetry_control_count =
        source_parity_level.enemy_control_write_count;
    telemetry_max_enemies =
        source_parity_level.max_active_enemy_count;
    telemetry_enemy_shots_spawned =
        source_parity_level.enemy_shot_spawn_count;
    telemetry_enemy_shot_drops =
        source_parity_level.enemy_shot_drop_count;
    telemetry_max_enemy_shots =
        source_parity_level.enemy_shot_max_active_count;
    telemetry_kill_cash =
        source_parity_level.direct_cash_awarded;
    telemetry_reward_spawns =
        source_parity_level.score_item_spawn_count;
    telemetry_reward_pickups =
        source_parity_level.score_item_pickup_count;
    telemetry_max_rewards =
        source_parity_level.score_item_max_active_count;
    telemetry_reward_controls =
        source_parity_level.death_control_event_count;
    telemetry_reward_assignments =
        source_parity_level.death_assignment_count;
}

static void source_apply_background_state(void)
{
    /*
     * PC Low detail starts with background2=false.  Its special
     * background2over==3 draw path both draws layer 2 and permanently flips
     * background2=true, so a later authored event can still restore it.
     */
    if (
        !source_background2_enabled &&
        source_parity_level.background2_over == 3
    ) {
        if (background_prepare_layer(
                1,
                background_presentation_scroll_pixel(
                    1,
                    source_camera_offset_y
                ) >> 3
            )) {
            source_background2_enabled = 1;
            source_background2_upload_pending = 1;
        }
    }
    bg1_scroll_speed = (u8)source_parity_level.back_move;
    bg2_scroll_speed = (u8)source_parity_level.back_move2;
    bg3_scroll_speed = (u8)source_parity_level.back_move3;
    bg1_scroll_delay_max =
        source_parity_level.map1_y_delay_max ?
        source_parity_level.map1_y_delay_max : 1;
    bg2_scroll_delay_max =
        source_parity_level.map2_y_delay_max ?
        source_parity_level.map2_y_delay_max : 1;
    if (bg1_scroll_delay > bg1_scroll_delay_max) {
        bg1_scroll_delay = bg1_scroll_delay_max;
    }
    if (bg2_scroll_delay > bg2_scroll_delay_max) {
        bg2_scroll_delay = bg2_scroll_delay_max;
    }
}

static __attribute__((unused)) void source_apply_player_shot_result(
    PlayerShot *shot,
    OtShotCollisionResult *result
)
{
    u8 effect_index;
    u8 original_damage = shot != 0 ? shot->damage : 0;

    if (!result->collided) return;
    if (shot != 0) {
        if (shot->infinite) {
            shot->damage = original_damage;
            shot->active = 1;
        } else
        {
            shot->damage = result->remaining_damage;
            if (result->consumed) shot->active = 0;
        }
    }
    player_cash += result->cash_awarded;
    for (
        effect_index = 0;
        effect_index < result->effect_count;
        effect_index++
    ) {
        source_emit_effect(&result->effects[effect_index]);
    }
}

static u8 source_option_projectile_collision_radii(
    u16 render_graphic,
    u8 *radius_w,
    u8 *radius_h
)
{
    if (
        render_graphic <= 60000 ||
        radius_w == 0 ||
        radius_h == 0
    ) {
        return 0;
    }
#if TYRIAN_GBA_STRESS_LOADOUT
    if (
        render_graphic !=
            60001 + SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
    ) {
        return 0;
    }
    *radius_w = SOURCE_OPTION_PROJECTILE_SOURCE_WIDTH / 2;
    *radius_h = SOURCE_OPTION_PROJECTILE_SOURCE_HEIGHT / 2;
#else
    {
        static const u8 width[
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ] = {62, 65, 62};
        static const u8 height[
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ] = {24, 28, 23};
        u16 graphic = (u16)(render_graphic - 60001u);
        u8 frame;

        if (
            graphic < SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
            graphic >=
                SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC +
                    SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ) {
            return 0;
        }
        frame = (u8)(
            graphic - SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
        );
        *radius_w = (u8)(width[frame] / 2u);
        *radius_h = (u8)(height[frame] / 2u);
    }
#endif
    return 1;
}

#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
static bool source_autotest_enemy_matches_stop_pool(u8 index)
{
    switch (source_parity_level.stop_background_num) {
    case 1:
        return
            (index >= 25 && index < 50) ||
            (index >= 75 && index < 100);
    case 2:
        return index < 25;
    default:
        return true;
    }
}

/*
 * Route-smoke validates common ROMFS/event/death paths, not the temporary
 * legacy Pulse-Cannon adapter.  Some stock levels deliberately place
 * destructible control hitboxes above the visible playfield for weapons with
 * a taller PC collision radius.  If such a hitbox is the only remaining
 * member of an authored event-4 stop group, inject one source collision after
 * a grace period.  This exists only in route-smoke ROMs and is separately
 * reported, so release gameplay can never inherit or conceal the assist.
 */
static __attribute__((unused)) void
source_autotest_route_combat_assist(void)
{
    u8 index;
    u8 candidate = OT_ENEMY_COUNT;

    if (!source_parity_level.stop_backgrounds) {
        source_autotest_unreachable_stop_ticks = 0;
        return;
    }
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[index];

        if (
            !source_autotest_enemy_matches_stop_pool(index) ||
            source_parity_level.enemy_avail[index] != 0 ||
            enemy->scoreitem ||
            enemy->armorleft == 255
        ) {
            continue;
        }
        /*
         * The current GBA shot adapter releases at y < 0.  A normal collision
         * can still reach an enemy centre down to y=-8; let keypad aiming
         * handle every such target without assistance.
         */
        if (enemy->ey >= -8 && enemy->ey <= 190) {
            source_autotest_unreachable_stop_ticks = 0;
            return;
        }
        if (candidate == OT_ENEMY_COUNT) candidate = index;
    }
    if (candidate == OT_ENEMY_COUNT) {
        source_autotest_unreachable_stop_ticks = 0;
        return;
    }
    if (++source_autotest_unreachable_stop_ticks < 60) return;
    source_autotest_unreachable_stop_ticks = 0;
    {
        const OtEnemy *enemy = &source_parity_level.enemy[candidate];
        OtShotCollisionResult result;
        uint8_t damage = enemy->armorleft;

        if (damage == 0) damage = 1;
        ot_level_port_begin_player_shot_collision_phase(
            &source_parity_level
        );
        ot_level_port_collide_player_shot(
            &source_parity_level,
            (int16_t)(enemy->ex + enemy->mapoffset),
            (int16_t)(enemy->ey - 6),
            damage,
            &result
        );
        ot_level_port_end_player_shot_collision_phase(
            &source_parity_level
        );
        if (result.collided) {
            telemetry_autotest_combat_assists++;
            source_apply_player_shot_result(0, &result);
        }
    }
}
#endif

#if TYRIAN_GBA_STRESS_LOADOUT
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
#else
static void
#endif
source_process_player_collisions(bool collide_player)
{
#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    u8 shot_index;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    bool collision_phase_started = false;
#endif
#endif
    u8 effect_index;
    OtPlayerCollisionResult player_result;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    u32 collision_cycle_start = stress_cycle_counter_read();
#endif

#if !TYRIAN_GBA_STRESS_LOADOUT
    if (
        !source_parity_level.end_level &&
        source_special_zinglon_duration > 1
    ) {
        s16 width = (s16)(
            25 -
            (
                source_special_zinglon_duration > 25 ?
                    source_special_zinglon_duration - 25 :
                    25 - source_special_zinglon_duration
            )
        );
        OtShotCollisionResult zinglon_result;

        if (width > 0) {
            ot_level_port_collide_zinglon_beam(
                &source_parity_level,
                (s16)(player_source_x + 7),
                (u8)width,
                &zinglon_result
            );
            source_apply_player_shot_result(
                0,
                &zinglon_result
            );
        }
    }
#endif

#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
        PlayerShot *shot = &player_shots[shot_index];
        OtShotCollisionResult result;

        if (!shot->active) continue;
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
        if (!collision_phase_started) {
            ot_level_port_begin_player_shot_collision_phase(
                &source_parity_level
            );
            collision_phase_started = true;
        }
#endif
        if (shot->chain_weapon != 0) {
            if (
                ot_level_port_player_shot_overlaps(
                    &source_parity_level,
                    shot->x,
                    shot->y
                )
            ) {
                OtWeaponDefinition chain;

#if TYRIAN_GBA_STRESS_LOADOUT
                stress_shot_multi_pos[STRESS_BAY_MISC] = 0;
#else
                source_shot_multi_pos[SOURCE_WEAPON_BAY_MISC] = 0;
#endif
                if (
                    ot_data_hdt_weapon_read(
                        shot->chain_weapon,
                        &chain
                    ) &&
                    source_weapon_is_valid(&chain)
                ) {
#if TYRIAN_GBA_STRESS_LOADOUT
                    stress_spawn_weapon(
                        &chain,
                        STRESS_BAY_MISC,
                        shot->x,
                        shot->y
                    );
#else
                    source_spawn_weapon(
                        &chain,
                        SOURCE_WEAPON_BAY_MISC,
                        shot->x,
                        shot->y
                    );
#endif
                    telemetry_player_chain_volleys++;
                } else {
                    telemetry_stress_loadout_failures++;
                }
                shot->active = 0;
            }
            continue;
        }
#if TYRIAN_GBA_COLLISION_PACKED_CALL
        {
            u32 damage_and_radii = shot->damage;
            u8 radius_w;
            u8 radius_h;

            if (
                source_option_projectile_collision_radii(
                    shot->render_graphic,
                    &radius_w,
                    &radius_h
                )
            ) {
                damage_and_radii |=
                    (u32)radius_w << 8;
                damage_and_radii |=
                    (u32)radius_h << 16;
            }
            damage_and_radii |=
                (u32)(shot->blast_filter & 0x0fu) << 24;
            if (shot->iced != 0) {
                damage_and_radii |= (u32)1u << 28;
            }
            ot_level_port_collide_player_shot_packed(
                &source_parity_level,
                shot->x,
                shot->y,
                &result,
                damage_and_radii
            );
        }
#else
        {
            u8 radius_w;
            u8 radius_h;
            u32 damage_and_radii = shot->damage |
                ((u32)(shot->blast_filter & 0x0fu) << 24);

            if (shot->iced != 0) {
                damage_and_radii |= (u32)1u << 28;
            }

            if (
                source_option_projectile_collision_radii(
                    shot->render_graphic,
                    &radius_w,
                    &radius_h
                )
            ) {
                damage_and_radii |= (u32)radius_w << 8;
                damage_and_radii |= (u32)radius_h << 16;
                ot_level_port_collide_player_shot_packed(
                    &source_parity_level,
                    shot->x,
                    shot->y,
                    &result,
                    damage_and_radii
                );
            } else {
                ot_level_port_collide_player_shot_packed(
                    &source_parity_level,
                    shot->x,
                    shot->y,
                    &result,
                    damage_and_radii
                );
            }
        }
#endif
        source_apply_player_shot_result(shot, &result);
    }
#if TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK
    if (collision_phase_started) {
        ot_level_port_end_player_shot_collision_phase(
            &source_parity_level
        );
    }
#endif
#endif
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    stress_cycle_accumulate(
        collision_cycle_start,
        &telemetry_stress_collision_cycles_total,
        &telemetry_stress_collision_cycles_max
    );
#endif
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
#if !(TYRIAN_GBA_STRESS_LOADOUT && \
    TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION)
    if (collide_player) source_autotest_route_combat_assist();
#endif
#endif

    /*
     * tyrian2.c keeps advancing/colliding every existing player shot during
     * endLevel, but JE_playerCollide() itself is guarded by
     * player.is_alive && !endLevel.  Keep that boundary explicit: otherwise
     * the GBA port can leave linked enemies or event-jump targets alive while
     * the level-exit animation is already running.
     */
    if (!collide_player) return;

    source_parity_level.player_armor = player_armor;
    ot_level_port_collide_player(
        &source_parity_level,
        player_alive && player_invulnerable == 0,
        &player_result
    );
    player_armor = source_parity_level.player_armor;
    player_cash += player_result.cash_awarded;
    /*
     * JE_playerCollide() runs before JE_playerMovement().  Preserve the wide
     * pre-clamp velocity so the source friction/input/clamp order remains
     * intact even for high-armour authored collision objects.
     */
    player_source_velocity_x +=
        player_result.player_velocity_x_delta;
    player_source_velocity_y +=
        player_result.player_velocity_y_delta;
    for (
        effect_index = 0;
        effect_index < player_result.effect_count;
        effect_index++
    ) {
        source_emit_effect(&player_result.effects[effect_index]);
    }
    for (
        effect_index = 0;
        effect_index < player_result.pickup_effect_count;
        effect_index++
    ) {
        source_spawn_fixed_pickup_explosion(
            &player_result.pickup_effects[effect_index]
        );
    }
    while (player_result.orbiting_asteroids_awarded != 0) {
        (void)spawn_orbiting_asteroid_shot();
        player_result.orbiting_asteroids_awarded--;
    }
    if (player_result.reset_all_shot_multi_pos) {
#if TYRIAN_GBA_STRESS_LOADOUT
        memset(stress_shot_multi_pos, 0, sizeof(stress_shot_multi_pos));
#else
        memset(source_shot_multi_pos, 0, sizeof(source_shot_multi_pos));
#endif
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (player_result.front_weapon_picked_up) {
        /* JE_playerCollide(): shotMultiPos[SHOT_FRONT]=0; repeat=10. */
        source_front_weapon_bound = 0;
        (void)source_front_weapon_sync();
        source_shot_multi_pos[SOURCE_WEAPON_BAY_FRONT] = 0;
        source_shot_repeat[SOURCE_WEAPON_BAY_FRONT] = 10;
    }
    if (player_result.special_weapon_picked_up) {
        source_special_bound_id = 0xff;
        (void)source_special_sync();
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 10;
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL2] = 0;
    }
#endif
    if (
        player_result.pickup_message_type !=
            OT_PICKUP_MESSAGE_NONE
    ) {
        gameplay_overlay_show_pickup(
            player_result.pickup_message_type,
            player_result.pickup_message_item_id
        );
    }
    if (player_result.bonus_level_triggered) {
        /*
         * mainint.c plays source song 30 ("Zanac"), then displays
         * miscText[59] ("Secret Level!") for 150 ticks.
         */
        audio_load_source_song_once_preserve_effects(30);
        secret_level_music_active = 1;
        insert_coin_tile_upload_pending = 0;
        secret_level_tile_upload_pending = 1;
        secret_level_palette_dirty = 1;
    }
    if (player_result.damage) player_damage(player_result.damage);
}

static bool source_apply_enemy_shot_impact(
    void *context,
    const OtEnemyShotImpact *impact
)
{
    u16 damage_through_shield;

    (void)context;
    /* tyrian2.c creates this type-0 impact even during invulnerability. */
    source_spawn_explosion(
        impact->x,
        impact->y,
        0,
        0,
        0
    );
    damage_through_shield = player_damage(impact->damage);
    if (damage_through_shield > 0) {
        player_source_velocity_x +=
            ((s32)impact->velocity_x * damage_through_shield) / 2;
        player_source_velocity_y +=
            ((s32)impact->velocity_y * damage_through_shield) / 2;
    }
    return player_alive != 0;
}

static void source_update_enemy_projectiles(void)
{
    u8 hit_area_x = source_parity_level.player_shot_hit_area_x;
    u8 hit_area_y = source_parity_level.player_shot_hit_area_y;

    /*
     * JE_mainGamePlayerFunctions() runs between player/enemy collision and
     * enemy-shot movement.  Reflect the just-applied GBA keypad input at the
     * same phase boundary, directly in unchanged OpenTyrian coordinates.
     */
    source_parity_level.player_x = player_source_x;
    source_parity_level.player_y = player_source_y;
    if (!player_alive) {
        /* PC keeps moving/drawing shots but skips its alive-player loop. */
        source_parity_level.player_shot_hit_area_x = 0;
        source_parity_level.player_shot_hit_area_y = 0;
    }
    ot_level_port_update_enemy_shots(
        &source_parity_level,
        source_apply_enemy_shot_impact,
        0
    );
    source_parity_level.player_shot_hit_area_x = hit_area_x;
    source_parity_level.player_shot_hit_area_y = hit_area_y;
    telemetry_collision_count =
        source_parity_level.player_shot_collision_count +
        source_parity_level.enemy_shot_player_hit_count +
        source_parity_level.player_enemy_contact_count;
    source_sync_runtime_telemetry();
}

static void source_runtime_reset(void)
{
    u32 layer_passed_checks;
    u32 layer_failed_checks;

    memset(source_enemy_cache, 0, sizeof(source_enemy_cache));
    memset(
        source_repeating_explosion,
        0,
        sizeof(source_repeating_explosion)
    );
    memset(source_enemy_uploads, 0, sizeof(source_enemy_uploads));
    source_player_cache_graphic = 0;
    source_player_cache_valid = 0;
    source_player_cache_visible = 0;
    source_player_upload_pending = 0;
    source_player_upload_l2_slot = 0;
    memset(
        source_projectile_cache,
        0,
        sizeof(source_projectile_cache)
    );
    memset(
        source_projectile_uploads,
        0,
        sizeof(source_projectile_uploads)
    );
    memset(
        source_effect_cache_last_epoch,
        0,
        sizeof(source_effect_cache_last_epoch)
    );
    memset(
        source_effect_cache_frame_key,
        SOURCE_EFFECT_FRAME_KEY_INVALID,
        sizeof(source_effect_cache_frame_key)
    );
    memset(source_effect_uploads, 0, sizeof(source_effect_uploads));
    memset(source_effect_frame_slot, 0, sizeof(source_effect_frame_slot));
    source_enemy_cache_epoch = 0;
    source_enemy_upload_count = 0;
    source_projectile_cache_epoch = 0;
    source_projectile_upload_count = 0;
    source_effect_cache_epoch = 0;
    source_effect_upload_count = 0;
    source_effect_boss_bar_tiles_dirty = 0;
    source_effect_reward_tiles_dirty = 0;
    source_effect_reward_tiles_required = 0;
    source_effect_status_tiles_dirty = 0;
    source_effect_status_tiles_required = 0;
    telemetry_sprite2_l2_hits = 0;
    telemetry_sprite2_l2_misses = 0;
    telemetry_sprite2_l2_evictions = 0;
    telemetry_sprite2_l2_drops = 0;
    telemetry_sprite2_l2_flushes = 0;
    telemetry_sprite2_l2_raw_builds = 0;
    telemetry_sprite2_l2_rle_fallbacks = 0;
    telemetry_sprite2_l2_max_visible_unique = 0;
    if (!source_enemy_palette_setup(SOURCE_LEVEL_PALETTE_INDEX)) {
        source_parity_level.assets_valid = false;
    }
    if (!source_option_projectile_sync()) {
        source_parity_level.assets_valid = false;
#if TYRIAN_GBA_STRESS_LOADOUT
        stress_loadout_valid = 0;
#endif
    }
    if (
        !source_layer_priority_self_test(
            &layer_passed_checks,
            &layer_failed_checks
        )
    ) {
        source_parity_level.assets_valid = false;
    }
    telemetry_layer_rule_checks = layer_passed_checks;
    telemetry_layer_rule_failures = layer_failed_checks;
    telemetry_source_visible_enemies = 0;
    telemetry_source_max_visible_enemies = 0;
    telemetry_source_unknown_visuals = 0;
    telemetry_sprite2_decode_failures = 0;
    telemetry_sprite2_cache_hits = 0;
    telemetry_sprite2_cache_misses = 0;
    telemetry_sprite2_cache_evictions = 0;
    telemetry_sprite2_cache_drops = 0;
    source_enemy_first_decode_failure_valid = 0;
    source_enemy_first_decode_failure_shape_table = 0;
    source_enemy_first_decode_failure_graphic = 0;
    source_enemy_first_decode_failure_size = 0;
    source_enemy_first_decode_failure_filter = 0;
    source_enemy_first_decode_failure_pool = 0;
    source_enemy_first_decode_failure_source_index = 0;
    source_enemy_first_decode_failure_definition = 0;
    source_enemy_first_decode_failure_level_position = 0;
    source_enemy_first_decode_failure_x = 0;
    source_enemy_first_decode_failure_y = 0;
    source_enemy_first_decode_failure_cycle = 0;
    source_enemy_first_decode_failure_shape_banks = 0;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    source_autotest_unreachable_stop_ticks = 0;
#endif
    telemetry_sprite2_uploads = 0;
    telemetry_sprite2_upload_bytes = 0;
    telemetry_sprite2_compact_uploads = 0;
    telemetry_sprite2_split_uploads = 0;
    telemetry_sprite2_upload_coalesces = 0;
    telemetry_sprite2_max_uploads = 0;
    telemetry_sprite2_max_visible_unique = 0;
    telemetry_boss_perf_started = 0;
    telemetry_boss_perf_completed = 0;
    telemetry_boss_perf_start_position = 0;
    telemetry_boss_perf_end_position = 0;
    telemetry_boss_perf_display_frames = 0;
    telemetry_boss_perf_missed_vblanks = 0;
    telemetry_boss_perf_sprite2_misses = 0;
    telemetry_boss_perf_sprite2_evictions = 0;
    telemetry_boss_perf_sprite2_upload_bytes = 0;
    telemetry_boss_perf_projectile_misses = 0;
    telemetry_boss_perf_l2_hits = 0;
    telemetry_boss_perf_l2_misses = 0;
    telemetry_boss_perf_l2_evictions = 0;
    telemetry_boss_perf_l2_raw_builds = 0;
    telemetry_boss_perf_l2_fallbacks = 0;
    boss_perf_start_display_frames = 0;
    boss_perf_start_missed_vblanks = 0;
    boss_perf_start_sprite2_misses = 0;
    boss_perf_start_sprite2_evictions = 0;
    boss_perf_start_sprite2_upload_bytes = 0;
    boss_perf_start_projectile_misses = 0;
    boss_perf_start_l2_hits = 0;
    boss_perf_start_l2_misses = 0;
    boss_perf_start_l2_evictions = 0;
    boss_perf_start_l2_raw_builds = 0;
    boss_perf_start_l2_fallbacks = 0;
    telemetry_effect_cache_hits = 0;
    telemetry_effect_cache_misses = 0;
    telemetry_effect_cache_evictions = 0;
    telemetry_effect_cache_drops = 0;
    telemetry_effect_cache_uploads = 0;
    telemetry_effect_cache_upload_bytes = 0;
    telemetry_effect_cache_upload_coalesces = 0;
    telemetry_effect_cache_max_uploads = 0;
    telemetry_effect_cache_max_visible_unique = 0;
}
