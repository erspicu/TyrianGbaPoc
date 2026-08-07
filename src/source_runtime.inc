/*
 * GBA presentation/input adapter for the directly translated OpenTyrian
 * selected-level runtime. Source gameplay stays in its original coordinate
 * system.  These helpers reproduce JE_starShowVGA's x=24 extraction, then
 * centre-crop its 264x184 gameplay viewport to the GBA's native 240x160.
 * No source coordinate or sprite is rescaled.
 */

#include "gba_hotpath_layout.inc"

typedef struct {
    u32 last_epoch;
    u32 generation;
    u16 graphic;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 valid;
    u8 resident;
} SourceEnemyCacheSlot;

_Static_assert(
    sizeof(SourceEnemyCacheSlot) == SOURCE_ENEMY_ASM_CACHE_SLOT_SIZE &&
        __builtin_offsetof(SourceEnemyCacheSlot, graphic) ==
            SOURCE_ENEMY_ASM_GRAPHIC_OFFSET &&
        __builtin_offsetof(SourceEnemyCacheSlot, shape_table) ==
            SOURCE_ENEMY_ASM_SHAPE_TABLE_OFFSET &&
        __builtin_offsetof(SourceEnemyCacheSlot, size) ==
            SOURCE_ENEMY_ASM_SIZE_OFFSET &&
        __builtin_offsetof(SourceEnemyCacheSlot, filter) ==
            SOURCE_ENEMY_ASM_FILTER_OFFSET &&
        __builtin_offsetof(SourceEnemyCacheSlot, valid) ==
            SOURCE_ENEMY_ASM_VALID_OFFSET,
    "SourceEnemyCacheSlot ARM layout drift"
);

typedef struct {
    u32 slot_generation;
    u32 l2_generation;
    u8 slot;
    u8 l2_slot;
} SourceEnemyUpload;

typedef struct {
    u32 last_epoch;
    u32 generation;
    u16 graphic;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 valid;
    u8 pin_count;
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
 * scratch a debug/fallback-only path.  Retail keeps it in IWRAM so the 12 KiB
 * EWRAM heap floor is unchanged.  A full-loadout build also places the large
 * collision dispatcher in IWRAM; move only this cold scratch to EWRAM there
 * so the benchmark/playable stress ROM retains a usable call stack.  The
 * stress EWRAM map still keeps the project's 6 KiB minimum heap margin.
 */
static u16 source_enemy_decode_pixels[OT_SPRITE2_FRAME_PIXELS]
    STRESS_EWRAM_BSS
    __attribute__((aligned(4)));
static u8 source_enemy_palette_map[OT_PALETTE_COLOUR_COUNT]
    EWRAM_BSS;
static u32 source_enemy_cache_epoch;
static u8 source_enemy_upload_count;
static u16 source_player_cache_graphic;
static u8 source_player_cache_valid;
static u8 source_player_cache_resident;
static u8 source_player_cache_visible;
static u8 source_player_upload_pending;
static u32 source_player_cache_generation;
static u32 source_player_upload_cache_generation;
static u32 source_player_upload_l2_generation;

#ifdef AUTOTEST
static u32 source_sprite2_pending_upload_count(void)
{
    return source_enemy_upload_count +
        (source_player_upload_pending ? 1u : 0u);
}
#endif
static u8 source_player_upload_l2_slot;
static u32 source_cache_generation_counter;
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

#include "source_enemy_cache_lookup.inc"


#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_STATIC_ATLAS_MAX_SLOTS 4u
#else
#define SOURCE_STATIC_ATLAS_MAX_SLOTS SOURCE_SPECIAL_OPTION_BANANA_SLOT_COUNT
#endif

enum {
    SOURCE_STATIC_ATLAS_OWNER_NONE = 0,
    SOURCE_STATIC_ATLAS_OWNER_OPTION = 1,
    SOURCE_STATIC_ATLAS_OWNER_SPECIAL = 2,
    SOURCE_STATIC_BUILD_FAILED = 0,
    SOURCE_STATIC_BUILD_READY = 1,
    SOURCE_STATIC_BUILD_DEFERRED = 2,
};

typedef struct {
    u32 l2_generation[SOURCE_STATIC_ATLAS_MAX_SLOTS];
    u32 bytes;
    u16 tile_base;
    u16 owner_key;
    u8 slot_count;
    u8 owner;
    u8 active;
} SourceStaticAtlasUpload;

static SourceStaticAtlasUpload source_static_atlas_upload EWRAM_BSS;
static u8 source_option_projectile_resident;
#if !TYRIAN_GBA_STRESS_LOADOUT
static u8 source_special_option_projectile_resident;
static const u8 source_option_projectile_palette_slot[14] = {
    1, 1, 4, 5, 6, 7, 8,
    9, 10, 11, 12, 13, 14, 15,
};
#endif

static u32 source_cache_next_generation(void)
{
    source_cache_generation_counter++;
    if (source_cache_generation_counter == 0) {
        source_cache_generation_counter = 1;
    }
    return source_cache_generation_counter;
}

static u8 source_sprite2_l2_pin(u8 slot, u32 generation)
{
    SourceSprite2L2Slot *entry;

    if (slot >= SOURCE_SPRITE2_L2_SLOT_COUNT) return 0;
    entry = &source_sprite2_l2[slot];
    if (
        entry->generation != generation ||
        entry->pin_count == 0xffu
    ) {
        telemetry_sprite2_generation_mismatches++;
        return 0;
    }
    entry->pin_count++;
    return 1;
}

static void source_sprite2_l2_unpin(u8 slot, u32 generation)
{
    SourceSprite2L2Slot *entry;

    if (slot >= SOURCE_SPRITE2_L2_SLOT_COUNT) return;
    entry = &source_sprite2_l2[slot];
    if (entry->generation != generation || entry->pin_count == 0) {
        telemetry_sprite2_generation_mismatches++;
        return;
    }
    entry->pin_count--;
}

static void source_enemy_cache_cancel_upload(u8 slot)
{
    u8 index = 0;

    while (index < source_enemy_upload_count) {
        SourceEnemyUpload *upload = &source_enemy_uploads[index];

        if (upload->slot != slot) {
            index++;
            continue;
        }
        source_sprite2_l2_unpin(
            upload->l2_slot,
            upload->l2_generation
        );
        source_enemy_upload_count--;
        if (index < source_enemy_upload_count) {
            source_enemy_uploads[index] =
                source_enemy_uploads[source_enemy_upload_count];
        }
        telemetry_sprite2_upload_coalesces++;
        telemetry_sprite2_pending_cancellations++;
    }
}

static u8 source_enemy_cache_upload_pending(u8 slot, u32 generation)
{
    u8 index;

    for (index = 0; index < source_enemy_upload_count; index++) {
        if (
            source_enemy_uploads[index].slot == slot &&
            source_enemy_uploads[index].slot_generation == generation
        ) {
            return 1;
        }
    }
    return 0;
}

static void source_enemy_cache_invalidate_slot(u8 slot)
{
    if (slot >= SOURCE_ENEMY_CACHE_SLOT_COUNT) return;
    source_enemy_cache_cancel_upload(slot);
    source_enemy_cache[slot].valid = 0;
    source_enemy_cache[slot].resident = 0;
    source_enemy_cache[slot].generation = source_cache_next_generation();
}

static void source_static_atlas_cancel(u8 owner)
{
    u8 index;

    if (
        !source_static_atlas_upload.active ||
        (
            owner != SOURCE_STATIC_ATLAS_OWNER_NONE &&
            source_static_atlas_upload.owner != owner
        )
    ) {
        return;
    }
    for (
        index = 0;
        index < source_static_atlas_upload.slot_count;
        index++
    ) {
        source_sprite2_l2_unpin(
            index,
            source_static_atlas_upload.l2_generation[index]
        );
    }
    source_static_atlas_upload.active = 0;
}

static u8 source_static_atlas_begin(
    u8 owner,
    u16 owner_key,
    u8 slot_count,
    u16 tile_base,
    u32 bytes
)
{
    u8 index;

    if (source_static_atlas_upload.active) {
        telemetry_sprite2_scratch_deferrals++;
        return 0;
    }
    if (
        slot_count == 0 ||
        slot_count > SOURCE_STATIC_ATLAS_MAX_SLOTS ||
        slot_count > SOURCE_SPRITE2_L2_SLOT_COUNT
    ) {
        return 0;
    }
    for (index = 0; index < slot_count; index++) {
        if (source_sprite2_l2[index].pin_count != 0) {
            telemetry_sprite2_scratch_deferrals++;
            return 0;
        }
    }
    memset(&source_static_atlas_upload, 0, sizeof(source_static_atlas_upload));
    source_static_atlas_upload.bytes = bytes;
    source_static_atlas_upload.tile_base = tile_base;
    source_static_atlas_upload.owner_key = owner_key;
    source_static_atlas_upload.slot_count = slot_count;
    source_static_atlas_upload.owner = owner;
    source_static_atlas_upload.active = 1;
    for (index = 0; index < slot_count; index++) {
        SourceSprite2L2Slot *entry = &source_sprite2_l2[index];

        entry->valid = 0;
        entry->last_epoch = 0;
        entry->generation = source_cache_next_generation();
        entry->pin_count = 1;
        source_static_atlas_upload.l2_generation[index] =
            entry->generation;
    }
    return 1;
}

static u8 source_static_atlas_is_pending(u8 owner, u16 owner_key)
{
    return
        source_static_atlas_upload.active &&
        source_static_atlas_upload.owner == owner &&
        source_static_atlas_upload.owner_key == owner_key;
}

static void source_static_atlas_upload_commit(void)
{
    u8 index;
    u8 valid = 1;

    if (!source_static_atlas_upload.active) return;
    for (
        index = 0;
        index < source_static_atlas_upload.slot_count;
        index++
    ) {
        const SourceSprite2L2Slot *entry = &source_sprite2_l2[index];

        if (
            entry->generation !=
                source_static_atlas_upload.l2_generation[index] ||
            entry->pin_count == 0
        ) {
            valid = 0;
            break;
        }
    }
    if (!valid) {
        telemetry_sprite2_generation_mismatches++;
        source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_NONE);
        return;
    }
    dmaCopy(
        source_sprite2_l2_tiles[0],
        (u8 *)OBJ_BASE_ADR +
            (u32)source_static_atlas_upload.tile_base * 32u,
        source_static_atlas_upload.bytes
    );
    if (
        source_static_atlas_upload.owner ==
            SOURCE_STATIC_ATLAS_OWNER_OPTION &&
        source_option_projectile_valid != 0
    ) {
        source_option_projectile_resident = 1;
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (
        source_static_atlas_upload.owner ==
            SOURCE_STATIC_ATLAS_OWNER_SPECIAL &&
        source_special_option_projectile_graphic ==
            source_static_atlas_upload.owner_key
    ) {
        source_special_option_projectile_resident = 1;
    }
#endif
    source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_NONE);
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static void source_obj_palette_base_write(u16 index, u16 colour)
{
    if (!source_detail_obj_palette_base_valid) {
        SPRITE_PALETTE[index] = colour;
        return;
    }
    source_detail_obj_palette_base[index] = colour;
    source_detail_effect_obj_palette[index] = colour;
    source_detail_palette_dirty = 1;
}

static u8 source_option_projectile_palette_apply(void)
{
    OtDataView palette;
    u8 colour;
    u16 base = SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16u;

    if (!ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        return 0;
    }
    source_obj_palette_base_write(base, 0);
    for (colour = 0; colour < 14; colour++) {
        source_obj_palette_base_write(
            (u16)(base + source_option_projectile_palette_slot[colour]),
            title_palette_colour(&palette, (u8)(114u + colour))
        );
    }
    source_obj_palette_base_write(
        (u16)(base + 2u),
        ((const u16 *)(const void *)obj_palette)[
            OBJ_PAL_HUD_GENERATOR * 16u + 2u
        ]
    );
    source_obj_palette_base_write(
        (u16)(base + 3u),
        ((const u16 *)(const void *)obj_palette)[
            OBJ_PAL_HUD_GENERATOR * 16u + 3u
        ]
    );
    return 1;
}

static void source_option_projectile_palette_restore(void)
{
    u8 colour;
    u16 base = SOURCE_OPTION_PROJECTILE_PALETTE_BANK * 16u;

    for (colour = 0; colour < 16u; colour++) {
        source_obj_palette_base_write(
            (u16)(base + colour),
            ((const u16 *)(const void *)obj_palette)[base + colour]
        );
    }
}
#endif
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
    source_option_projectile_resident = 0;
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
        return SOURCE_STATIC_BUILD_FAILED;
    }
    if (
        !source_static_atlas_begin(
            SOURCE_STATIC_ATLAS_OWNER_OPTION,
            SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC,
            (u8)(
                SOURCE_OPTION_PROJECTILE_BYTES /
                    SOURCE_SPRITE2_L2_FRAME_BYTES
            ),
            SOURCE_OPTION_PROJECTILE_TILE_BASE,
            SOURCE_OPTION_PROJECTILE_BYTES
        )
    ) {
        return SOURCE_STATIC_BUILD_DEFERRED;
    }
    memset(target, 0, SOURCE_OPTION_PROJECTILE_BYTES);
    encoded = sprite.encoded.data;
    while (source < sprite.encoded.size && y < sprite.height) {
        u8 code = encoded[source++];

        if (code == 255) {
            if (source >= sprite.encoded.size) goto option_build_failed;
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
        goto option_build_failed;
    }
    source_option_projectile_valid = 1;
    return SOURCE_STATIC_BUILD_READY;

option_build_failed:
    source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_OPTION);
    source_option_projectile_valid = 0;
    source_option_projectile_resident = 0;
    return SOURCE_STATIC_BUILD_FAILED;
#else
    static const u8 expected_width[
        SOURCE_OPTION_PROJECTILE_FRAME_COUNT
    ] = {62, 65, 62};
    static const u8 expected_height[
        SOURCE_OPTION_PROJECTILE_FRAME_COUNT
    ] = {24, 28, 23};
    u8 frame;

    source_option_projectile_valid = 0;
    source_option_projectile_resident = 0;
    if (!source_enemy_palette_valid) return SOURCE_STATIC_BUILD_FAILED;
    if (
        !source_static_atlas_begin(
            SOURCE_STATIC_ATLAS_OWNER_OPTION,
            SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC,
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT,
            SOURCE_OPTION_PROJECTILE_TILE_BASE,
            SOURCE_OPTION_PROJECTILE_BYTES *
                SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        )
    ) {
        return SOURCE_STATIC_BUILD_DEFERRED;
    }
    for (
        frame = 0;
        frame < SOURCE_OPTION_PROJECTILE_FRAME_COUNT;
        frame++
    ) {
        OtShpSprite sprite;
        const u8 *encoded;
        u8 *target = source_sprite2_l2_tiles[frame];
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
            goto option_build_failed;
        }
        encoded = sprite.encoded.data;
        while (
            source < sprite.encoded.size &&
            y < sprite.height
        ) {
            u8 code = encoded[source++];

            if (code == 255) {
                if (source >= sprite.encoded.size) {
                    goto option_build_failed;
                }
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

                    if (code < 114 || code > 127) {
                        goto option_build_failed;
                    }
                    palette_index =
                        source_option_projectile_palette_slot[
                            code - 114u
                        ];
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
        if (y < sprite.height) goto option_build_failed;
        source_option_projectile_valid |= (u8)(1u << frame);
    }
    if (
        source_option_projectile_valid !=
            (1u << SOURCE_OPTION_PROJECTILE_FRAME_COUNT) - 1u ||
        !source_option_projectile_palette_apply()
    ) {
        goto option_build_failed;
    }
    return SOURCE_STATIC_BUILD_READY;

option_build_failed:
    source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_OPTION);
    source_option_projectile_valid = 0;
    source_option_projectile_resident = 0;
    return SOURCE_STATIC_BUILD_FAILED;
#endif
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static u16 source_special_option_tile_base(u16 graphic)
{
    if (graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC) {
        return SOURCE_SPECIAL_OPTION_XEGA_TILE_BASE;
    }
    if (graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
        return SOURCE_SPECIAL_OPTION_BANANA_TILE_BASE;
    }
    return 0;
}

static u8 source_special_option_slot_count(u16 graphic)
{
    if (graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC) {
        return SOURCE_SPECIAL_OPTION_XEGA_SLOT_COUNT;
    }
    if (graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
        return SOURCE_SPECIAL_OPTION_BANANA_SLOT_COUNT;
    }
    return 0;
}

static u16 source_special_option_graphic_from_weapon(
    u16 weapon_id,
    u8 depth
)
{
    OtWeaponDefinition weapon;
    u8 position;

    if (
        weapon_id == 0 ||
        depth >= 4 ||
        !ot_data_hdt_weapon_read(weapon_id, &weapon) ||
        !source_weapon_is_valid(&weapon)
    ) {
        return 0;
    }
    /* Only 0..max-1 are live; Episode 4 leaves executable-era filler in
     * unused record positions, which must never be interpreted as chains. */
    for (position = 0; position < weapon.max; position++) {
        if (weapon.sg[position] > 60000u) {
            u16 graphic = (u16)(weapon.sg[position] - 60001u);

            if (
                graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC ||
                graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC
            ) {
                return graphic;
            }
        }
    }
    for (position = 0; position < weapon.max; position++) {
        u8 attack = weapon.attack[position];

        if (attack > 99u && attack < 250u) {
            u16 graphic = source_special_option_graphic_from_weapon(
                (u16)(attack - 100u),
                (u8)(depth + 1u)
            );

            if (graphic != 0) return graphic;
        }
    }
    return 0;
}

static u16 source_special_option_required_graphic(void)
{
    OtSpecialDefinition special;

    if (
        source_parity_level.player_special == 0 ||
        !ot_data_hdt_special_read(
            source_parity_level.player_special,
            &special
        ) ||
        !(
            special.type == 1u ||
            (special.type >= 5u && special.type <= 11u) ||
            special.type == 16u
        ) ||
        special.weapon == 0
    ) {
        return 0;
    }
    return source_special_option_graphic_from_weapon(special.weapon, 0);
}

static u8 source_special_option_pixel_offset(
    u16 graphic,
    u16 x,
    u16 y,
    u32 *offset
)
{
    if (offset == 0) return 0;
    if (graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC) {
        if (
            x >= SOURCE_SPECIAL_OPTION_XEGA_WIDTH ||
            y >= SOURCE_SPECIAL_OPTION_XEGA_HEIGHT
        ) {
            return 0;
        }
        *offset = source_sprite2_tile_offset(x, y, 8);
        return 1;
    }
    if (graphic != SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) return 0;
    if (
        x >= SOURCE_SPECIAL_OPTION_BANANA_WIDTH ||
        y >= SOURCE_SPECIAL_OPTION_BANANA_HEIGHT
    ) {
        return 0;
    }

    if (y < 64u) {
        if (x < 64u) {
            *offset =
                SOURCE_SPECIAL_OPTION_BANANA_MAIN_TILE_OFFSET * 32u +
                source_sprite2_tile_offset(x, y, 8);
        } else if (y < 32u) {
            *offset =
                SOURCE_SPECIAL_OPTION_BANANA_RIGHT_TOP_TILE_OFFSET * 32u +
                source_sprite2_tile_offset((u16)(x - 64u), y, 2);
        } else {
            *offset =
                SOURCE_SPECIAL_OPTION_BANANA_RIGHT_BOTTOM_TILE_OFFSET * 32u +
                source_sprite2_tile_offset(
                    (u16)(x - 64u),
                    (u16)(y - 32u),
                    2
                );
        }
    } else if (x < 32u) {
        *offset =
            SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_LEFT_TILE_OFFSET * 32u +
            source_sprite2_tile_offset(x, (u16)(y - 64u), 4);
    } else if (x < 64u) {
        *offset =
            SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_MIDDLE_TILE_OFFSET * 32u +
            source_sprite2_tile_offset(
                (u16)(x - 32u),
                (u16)(y - 64u),
                4
            );
    } else {
        *offset =
            SOURCE_SPECIAL_OPTION_BANANA_CORNER_TILE_OFFSET * 32u +
            source_sprite2_tile_offset(
                (u16)(x - 64u),
                (u16)(y - 64u),
                2
            );
    }
    return 1;
}

static u8 source_special_option_projectile_build(u16 graphic)
{
    OtShpSprite sprite;
    const u8 *encoded;
    u8 *target = (u8 *)source_sprite2_l2_tiles;
    u32 bytes;
    u32 source = 0;
    u16 expected_width;
    u16 expected_height;
    u16 tile_base = source_special_option_tile_base(graphic);
    u8 scratch_slots = source_special_option_slot_count(graphic);
    u16 x = 0;
    u16 y = 0;

    if (graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC) {
        expected_width = SOURCE_SPECIAL_OPTION_XEGA_WIDTH;
        expected_height = SOURCE_SPECIAL_OPTION_XEGA_HEIGHT;
        bytes = SOURCE_SPECIAL_OPTION_XEGA_TILES * 32u;
    } else if (graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
        expected_width = SOURCE_SPECIAL_OPTION_BANANA_WIDTH;
        expected_height = SOURCE_SPECIAL_OPTION_BANANA_HEIGHT;
        bytes = SOURCE_SPECIAL_OPTION_BANANA_TILES * 32u;
    } else {
        return 0;
    }
    source_special_option_projectile_graphic = 0;
    source_special_option_projectile_resident = 0;
    if (
        tile_base == 0 ||
        !source_enemy_palette_valid ||
        !ot_data_shp_sprite_read(
            SOURCE_OPTION_PROJECTILE_SOURCE_TABLE,
            graphic,
            &sprite
        ) ||
        !sprite.populated ||
        sprite.width != expected_width ||
        sprite.height != expected_height
    ) {
        return SOURCE_STATIC_BUILD_FAILED;
    }
    if (
        !source_static_atlas_begin(
            SOURCE_STATIC_ATLAS_OWNER_SPECIAL,
            graphic,
            scratch_slots,
            tile_base,
            bytes
        )
    ) {
        return SOURCE_STATIC_BUILD_DEFERRED;
    }
    memset(target, 0, bytes);
    encoded = sprite.encoded.data;
    while (source < sprite.encoded.size && y < sprite.height) {
        u8 code = encoded[source++];

        if (code == 255u) {
            if (source >= sprite.encoded.size) {
                goto special_build_failed;
            }
            x = (u16)(x + encoded[source++]);
        } else if (code == 254u) {
            x = sprite.width;
        } else if (code == 253u) {
            x++;
        } else {
            u32 target_offset;

            if (
                source_special_option_pixel_offset(
                    graphic,
                    x,
                    y,
                    &target_offset
                )
            ) {
                target[target_offset] = source_enemy_palette_map[code];
            }
            x++;
        }
        if (x >= sprite.width) {
            x = 0;
            y++;
        }
    }
    if (y < sprite.height) goto special_build_failed;
    source_special_option_projectile_graphic = graphic;
    return SOURCE_STATIC_BUILD_READY;

special_build_failed:
    source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_SPECIAL);
    source_special_option_projectile_graphic = 0;
    source_special_option_projectile_resident = 0;
    return SOURCE_STATIC_BUILD_FAILED;
}

static u8 source_special_option_projectile_sync(void)
{
    u16 required = source_special_option_required_graphic();
    u8 slot_count;
    u8 index;
    u8 build_result;

    if (
        required == source_special_option_projectile_graphic &&
        (
            source_special_option_projectile_resident ||
            source_static_atlas_is_pending(
                SOURCE_STATIC_ATLAS_OWNER_SPECIAL,
                required
            )
        )
    ) {
        return 1;
    }
    source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_SPECIAL);
    source_special_option_projectile_graphic = 0;
    source_special_option_projectile_resident = 0;
    if (required == 0) return 1;
    slot_count = source_special_option_slot_count(required);
    if (slot_count == 0) return 0;

    build_result = source_special_option_projectile_build(required);
    if (build_result == SOURCE_STATIC_BUILD_DEFERRED) return 1;
    if (build_result != SOURCE_STATIC_BUILD_READY) return 0;

    for (
        index = (u8)(
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT -
            slot_count
        );
        index <
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                SOURCE_OPTION_PROJECTILE_FRAME_COUNT;
        index++
    ) {
        source_enemy_cache_invalidate_slot(index);
    }
    return 1;
}
#endif

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
    if (source_special_option_projectile_graphic != 0) {
        u8 slot_count = source_special_option_slot_count(
            source_special_option_projectile_graphic
        );
        u8 first = (u8)(
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
            SOURCE_OPTION_PROJECTILE_FRAME_COUNT -
            slot_count
        );

        if (
            slot >= first &&
            slot <
                SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                    SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ) {
            return 1;
        }
    }
    return source_option_projectile_valid != 0 &&
        slot >=
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                SOURCE_OPTION_PROJECTILE_FRAME_COUNT &&
        slot < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
#endif
}

static u8 source_option_projectile_sync(void)
{
#if TYRIAN_GBA_STRESS_LOADOUT
    u8 build_result;

    if (
        source_option_projectile_valid &&
        (
            source_option_projectile_resident ||
            source_static_atlas_is_pending(
                SOURCE_STATIC_ATLAS_OWNER_OPTION,
                SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
            )
        )
    ) {
        return 1;
    }
    build_result = source_option_projectile_build();
    return build_result != SOURCE_STATIC_BUILD_FAILED;
#else
    u8 required =
        source_parity_level.player_sidekick[0] == 9 ||
        source_parity_level.player_sidekick[1] == 9;
    u8 success = 1;
    u8 index;
    u8 build_result;

    if (!required) {
        u8 was_active = source_option_projectile_valid != 0;

        source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_OPTION);
        source_option_projectile_valid = 0;
        source_option_projectile_resident = 0;
        if (was_active) source_option_projectile_palette_restore();
    } else if (
        source_option_projectile_valid ==
            (1u << SOURCE_OPTION_PROJECTILE_FRAME_COUNT) - 1u &&
        (
            source_option_projectile_resident ||
            source_static_atlas_is_pending(
                SOURCE_STATIC_ATLAS_OWNER_OPTION,
                SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
            )
        )
    ) {
        /* Already resident or safely pinned for the next VBlank. */
    } else {
        source_static_atlas_cancel(SOURCE_STATIC_ATLAS_OWNER_OPTION);
        source_option_projectile_valid = 0;
        source_option_projectile_resident = 0;
        build_result = source_option_projectile_build();
        if (build_result == SOURCE_STATIC_BUILD_DEFERRED) {
            /* A different static atlas owns the scratch until VBlank. */
        } else if (build_result != SOURCE_STATIC_BUILD_READY) {
            source_option_projectile_valid = 0;
            success = 0;
        } else {
        /*
         * The three 4bpp 64x32 Plasma Storm frames time-share the final
         * three full-size enemy cache slots only while option 9 is equipped.
         */
        for (
            index =
                SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
                    SOURCE_OPTION_PROJECTILE_FRAME_COUNT;
            index < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
            index++
        ) {
                source_enemy_cache_invalidate_slot(index);
            }
        }
    }
    if (!source_special_option_projectile_sync()) success = 0;
    return success;
#endif
}

extern u32 source_sprite2_l2_pack_raw_word_asm(
    const u8 *raw,
    u8 filter,
    const u8 *palette_map
);

#if !TYRIAN_GBA_HOTPATH_ASM
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u32
source_sprite2_l2_pack_raw_word_c(
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
                pixel = (u8)(filter | (pixel & 0x0fu));
            }
            pixel = source_enemy_palette_map[pixel];
        }
        packed |= (u32)pixel << (lane * 8u);
    }
    return packed;
}
#endif

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
#if TYRIAN_GBA_HOTPATH_ASM
        u32 word0 = source_sprite2_l2_pack_raw_word_asm(
            raw,
            filter,
            source_enemy_palette_map
        );
        u32 word1 = source_sprite2_l2_pack_raw_word_asm(
            raw + 4,
            filter,
            source_enemy_palette_map
        );
        u32 word2 = source_sprite2_l2_pack_raw_word_asm(
            raw + 8,
            filter,
            source_enemy_palette_map
        );
#else
        u32 word0 = source_sprite2_l2_pack_raw_word_c(raw, filter);
        u32 word1 = source_sprite2_l2_pack_raw_word_c(raw + 4, filter);
        u32 word2 = source_sprite2_l2_pack_raw_word_c(raw + 8, filter);
#endif

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
     * Keep the independent frame compositor as a packing/debug fallback;
     * it consumes the same lossless raw catalog, never a duplicate ROMFS
     * command stream.
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
        if (slot->pin_count != 0) continue;
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
        for (
            index = 0;
            index < SOURCE_SPRITE2_L2_SLOT_COUNT;
            index++
        ) {
            if (source_sprite2_l2[index].pin_count != 0) {
                telemetry_sprite2_l2_pin_exhaustions++;
                break;
            }
        }
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
    source_sprite2_l2[selected].generation =
        source_cache_next_generation();
    source_sprite2_l2[selected].pin_count = 0;
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
        if (
            !source_player_cache_resident &&
            !source_player_upload_pending
        ) {
            source_player_cache_valid = 0;
        } else {
            telemetry_sprite2_cache_hits++;
            return SOURCE_PLAYER_CACHE_TILE_BASE;
        }
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
        /* Supersede the old transaction, then pin the newer decoded frame. */
        source_sprite2_l2_unpin(
            source_player_upload_l2_slot,
            source_player_upload_l2_generation
        );
        source_player_upload_pending = 0;
        telemetry_sprite2_upload_coalesces++;
    }
    source_player_cache_generation = source_cache_next_generation();
    source_player_cache_graphic = graphic;
    source_player_cache_valid = 1;
    source_player_cache_resident = 0;
    source_player_upload_l2_slot = l2_slot;
    source_player_upload_cache_generation =
        source_player_cache_generation;
    source_player_upload_l2_generation =
        source_sprite2_l2[l2_slot].generation;
    if (
        !source_sprite2_l2_pin(
            l2_slot,
            source_player_upload_l2_generation
        )
    ) {
        source_player_cache_valid = 0;
        return 0xffff;
    }
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
    u8 valid;

    if (!source_player_upload_pending) return;
    valid =
        source_player_cache_valid &&
        source_player_cache_generation ==
            source_player_upload_cache_generation &&
        source_sprite2_l2[source_player_upload_l2_slot].valid &&
        source_sprite2_l2[source_player_upload_l2_slot].generation ==
            source_player_upload_l2_generation &&
        source_sprite2_l2[source_player_upload_l2_slot].pin_count != 0;
    if (valid) {
        dmaCopy(
            source_sprite2_l2_tiles[source_player_upload_l2_slot],
            (u8 *)OBJ_BASE_ADR + SOURCE_PLAYER_CACHE_TILE_BASE * 32u,
            SOURCE_ENEMY_FRAME_BYTES
        );
        source_player_cache_resident = 1;
        telemetry_sprite2_uploads++;
        telemetry_sprite2_upload_bytes += SOURCE_ENEMY_FRAME_BYTES;
    } else {
        source_player_cache_valid = 0;
        source_player_cache_resident = 0;
        telemetry_sprite2_generation_mismatches++;
    }
    source_sprite2_l2_unpin(
        source_player_upload_l2_slot,
        source_player_upload_l2_generation
    );
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
        source_enemy_cache_invalidate_slot(
            SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT + 1u
        );
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (game_paused) {
        source_enemy_cache_invalidate_slot(
            SOURCE_ENEMY_CACHE_SPLIT_SLOT
        );
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

#include "source_enemy_cache_acquire.inc"


static void source_enemy_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_enemy_upload_count; index++) {
        const SourceEnemyUpload *upload = &source_enemy_uploads[index];
        const u8 *source;
        u8 *target =
            (u8 *)OBJ_BASE_ADR +
            source_enemy_cache_tile(upload->slot) * 32;
        u32 bytes = source_enemy_cache_slot_is_compact(upload->slot) ?
            SOURCE_ENEMY_COMPACT_FRAME_BYTES :
            SOURCE_ENEMY_FRAME_BYTES;
        u8 valid =
            source_enemy_cache[upload->slot].valid &&
            source_enemy_cache[upload->slot].generation ==
                upload->slot_generation &&
            source_sprite2_l2[upload->l2_slot].valid &&
            source_sprite2_l2[upload->l2_slot].generation ==
                upload->l2_generation &&
            source_sprite2_l2[upload->l2_slot].pin_count != 0;

        if (!valid) {
            if (
                source_enemy_cache[upload->slot].generation ==
                    upload->slot_generation
            ) {
                source_enemy_cache[upload->slot].valid = 0;
                source_enemy_cache[upload->slot].resident = 0;
            }
            telemetry_sprite2_generation_mismatches++;
            source_sprite2_l2_unpin(
                upload->l2_slot,
                upload->l2_generation
            );
            continue;
        }
        source = source_sprite2_l2_tiles[upload->l2_slot];

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
        source_enemy_cache[upload->slot].resident = 1;
        telemetry_sprite2_uploads++;
        if (source_enemy_cache_slot_is_compact(upload->slot)) {
            telemetry_sprite2_compact_uploads++;
        }
        telemetry_sprite2_upload_bytes += bytes;
        source_sprite2_l2_unpin(
            upload->l2_slot,
            upload->l2_generation
        );
    }
    source_enemy_upload_count = 0;
}

typedef struct {
    u32 last_epoch;
    u32 generation;
    u16 graphic;
    u8 shape_table;
    u8 valid;
    u8 resident;
} SourceProjectileCacheSlot;

typedef struct {
    u32 slot_generation;
    u32 l2_generation;
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

#include "source_projectile_cache_lookup.inc"

static void source_projectile_cache_cancel_upload(u8 slot)
{
    u8 index = 0;

    while (index < source_projectile_upload_count) {
        SourceProjectileUpload *upload =
            &source_projectile_uploads[index];

        if (upload->slot != slot) {
            index++;
            continue;
        }
        source_sprite2_l2_unpin(
            upload->l2_slot,
            upload->l2_generation
        );
        source_projectile_upload_count--;
        if (index < source_projectile_upload_count) {
            source_projectile_uploads[index] =
                source_projectile_uploads[
                    source_projectile_upload_count
                ];
        }
        telemetry_projectile_cache_upload_coalesces++;
        telemetry_sprite2_pending_cancellations++;
    }
}

static void source_projectile_cache_invalidate_slot(u8 slot)
{
    if (slot >= SOURCE_PROJECTILE_CACHE_SLOT_COUNT) return;
#if TYRIAN_GBA_PROJECTILE_CACHE_HINT
    source_projectile_cache_hint_clear_slot(slot);
#endif
    source_projectile_cache_cancel_upload(slot);
    source_projectile_cache[slot].valid = 0;
    source_projectile_cache[slot].resident = 0;
    source_projectile_cache[slot].generation =
        source_cache_next_generation();
}

static u8 source_projectile_cache_upload_pending(
    u8 slot,
    u32 generation
)
{
    u8 index;

    for (index = 0; index < source_projectile_upload_count; index++) {
        if (
            source_projectile_uploads[index].slot == slot &&
            source_projectile_uploads[index].slot_generation == generation
        ) {
            return 1;
        }
    }
    return 0;
}

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
#if TYRIAN_GBA_PROJECTILE_CACHE_HINT
    {
        u32 packed_key = source_projectile_cache_packed_key(
            shape_table,
            graphic
        );
        const SourceProjectileCacheSlot *hint_slot;

        telemetry_projectile_hint_probes++;
        hint_slot = source_projectile_cache_find_hint(packed_key);
        if (hint_slot != 0) {
            index = (u8)(hint_slot - source_projectile_cache);
            if (
                hint_slot->resident ||
                source_projectile_cache_upload_pending(
                    index,
                    hint_slot->generation
                )
            ) {
                source_projectile_cache[index].last_epoch =
                    source_projectile_cache_epoch;
                telemetry_projectile_hint_hits++;
                telemetry_projectile_cache_hits++;
                return source_projectile_cache_tile(index);
            }
            source_projectile_cache_invalidate_slot(index);
        }
        telemetry_projectile_hint_fallback_scans++;
    }
#endif
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
            if (
                slot->resident ||
                source_projectile_cache_upload_pending(
                    index,
                    slot->generation
                )
            ) {
                slot->last_epoch = source_projectile_cache_epoch;
#if TYRIAN_GBA_PROJECTILE_CACHE_HINT
                source_projectile_cache_hint_store(
                    shape_table,
                    graphic,
                    index
                );
#endif
                telemetry_projectile_cache_hits++;
                return source_projectile_cache_tile(index);
            }
            source_projectile_cache_invalidate_slot(index);
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
        if (presentation_eligible) {
            /*
             * All hardware slots are pinned by distinct frames in this same
             * presentation.  This is an intentional GBA display cull, not a
             * corrupt asset/cache failure; gameplay and collision state stay
             * live and may become visible on a later frame.
             */
            telemetry_projectile_visible_capacity_drops++;
            return 0xffff;
        }
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    if (source_projectile_cache[selected].valid) {
        telemetry_projectile_cache_evictions++;
    }
    source_projectile_cache_invalidate_slot(selected);
    source_projectile_cache[selected].generation =
        source_cache_next_generation();
    source_projectile_cache[selected].valid = 1;
    source_projectile_cache[selected].resident = 0;
    source_projectile_cache[selected].shape_table = shape_table;
    source_projectile_cache[selected].graphic = graphic;
    source_projectile_cache[selected].last_epoch =
        source_projectile_cache_epoch;
#if TYRIAN_GBA_PROJECTILE_CACHE_HINT
    source_projectile_cache_hint_store(
        shape_table,
        graphic,
        selected
    );
#endif
    telemetry_projectile_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        shape_table,
        graphic,
        0,
        0
    );
    if (l2_slot == 0xff) {
        source_projectile_cache_invalidate_slot(selected);
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    for (index = 0; index < source_projectile_upload_count; index++) {
        if (source_projectile_uploads[index].slot == selected) {
            source_sprite2_l2_unpin(
                source_projectile_uploads[index].l2_slot,
                source_projectile_uploads[index].l2_generation
            );
            source_projectile_uploads[index].l2_slot = l2_slot;
            source_projectile_uploads[index].slot_generation =
                source_projectile_cache[selected].generation;
            source_projectile_uploads[index].l2_generation =
                source_sprite2_l2[l2_slot].generation;
            if (
                !source_sprite2_l2_pin(
                    l2_slot,
                    source_projectile_uploads[index].l2_generation
                )
            ) {
                source_projectile_cache_invalidate_slot(selected);
                return 0xffff;
            }
            telemetry_projectile_cache_upload_coalesces++;
            return source_projectile_cache_tile(selected);
        }
    }
    if (
        source_projectile_upload_count >=
        SOURCE_PROJECTILE_CACHE_SLOT_COUNT
    ) {
        source_projectile_cache_invalidate_slot(selected);
        telemetry_projectile_cache_drops++;
        return 0xffff;
    }
    source_projectile_uploads[
        source_projectile_upload_count
    ].slot = selected;
    source_projectile_uploads[
        source_projectile_upload_count
    ].l2_slot = l2_slot;
    source_projectile_uploads[
        source_projectile_upload_count
    ].slot_generation = source_projectile_cache[selected].generation;
    source_projectile_uploads[
        source_projectile_upload_count
    ].l2_generation = source_sprite2_l2[l2_slot].generation;
    if (
        !source_sprite2_l2_pin(
            l2_slot,
            source_projectile_uploads[
                source_projectile_upload_count
            ].l2_generation
        )
    ) {
        source_projectile_cache_invalidate_slot(selected);
        return 0xffff;
    }
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
        const SourceProjectileUpload *upload =
            &source_projectile_uploads[index];
        u8 slot = upload->slot;
        u8 l2_slot = upload->l2_slot;
        u8 valid =
            source_projectile_cache[slot].valid &&
            source_projectile_cache[slot].generation ==
                upload->slot_generation &&
            source_sprite2_l2[l2_slot].valid &&
            source_sprite2_l2[l2_slot].generation ==
                upload->l2_generation &&
            source_sprite2_l2[l2_slot].pin_count != 0;

        if (valid) {
            dmaCopy(
                source_sprite2_l2_tiles[l2_slot],
                (u8 *)OBJ_BASE_ADR +
                    source_projectile_cache_tile(slot) * 32,
                SOURCE_PROJECTILE_FRAME_BYTES
            );
            source_projectile_cache[slot].resident = 1;
            telemetry_projectile_cache_uploads++;
        } else {
            if (
                source_projectile_cache[slot].generation ==
                    upload->slot_generation
            ) {
                source_projectile_cache[slot].valid = 0;
                source_projectile_cache[slot].resident = 0;
            }
            telemetry_sprite2_generation_mismatches++;
        }
        source_sprite2_l2_unpin(l2_slot, upload->l2_generation);
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

static u8 source_effect_cache_available_slots(void)
{
    u8 slot;
    u8 available = 0;

    for (slot = 0; slot < SOURCE_EFFECT_CACHE_SLOT_COUNT; slot++) {
        if (!source_effect_cache_slot_reserved(slot)) available++;
    }
    return available;
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

/* Camera, effects, collision integration and gameplay runtime orchestration. */
#include "source_gameplay_runtime.inc"
