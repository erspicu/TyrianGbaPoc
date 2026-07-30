/*
 * Generic Tyrian background adapter.
 *
 * No GBA-native level background is stored in the cartridge.  The selected
 * tyrianN.lvl map, its big-endian shape lookup, shapes?.dat pixels and the
 * stock palette.dat are read directly from ROMFS.  The hardware map remains
 * a 32-row GBA ring.  The 21 rows which can intersect the 160-pixel viewport
 * plus the soft camera's two-row hysteresis margin own cache references.
 * This retains rows already exposed by camera motion without repeatedly
 * decoding them when the player reverses direction.  A separately referenced
 * prefetch row covers the next upward-scroll boundary.
 */

enum {
    BACKGROUND_LAYER_COUNT = 3,
    /*
     * Measured campaign maxima are 533 / 434 / 454 resident patterns.
     * Keep a small margin on the first two layers and leave the final 58
     * characters of charblock 2 for the source-parity end-level overlay.
     */
    BACKGROUND_LAYER0_CACHE_SLOTS = 576,
    BACKGROUND_LAYER1_CACHE_SLOTS = 448,
    BACKGROUND_LAYER2_CACHE_SLOTS = 454,
    BACKGROUND_TOTAL_CACHE_SLOTS =
        BACKGROUND_LAYER0_CACHE_SLOTS +
        BACKGROUND_LAYER1_CACHE_SLOTS +
        BACKGROUND_LAYER2_CACHE_SLOTS,
    BACKGROUND_PATTERN_BUCKETS = 512,
    BACKGROUND_TILE_BYTES = 32,
    BACKGROUND_ROW_TILES = 64,
    BACKGROUND_SHAPE_COUNT = 600,
    BACKGROUND_SHAPE_WIDTH = OT_LEVEL_MAP_CELL_WIDTH,
    BACKGROUND_SHAPE_HEIGHT = OT_LEVEL_MAP_CELL_HEIGHT,
    BACKGROUND_SHAPE_BYTES =
        BACKGROUND_SHAPE_WIDTH * BACKGROUND_SHAPE_HEIGHT,
    BACKGROUND_PALETTE_BANKS = 16,
    BACKGROUND_PALETTE_BANK_COLOURS = 16,
    BACKGROUND_MAX_ROW_UPLOADS = BACKGROUND_ROW_TILES,
    /*
     * A non-tile-aligned 160-pixel viewport intersects 21 source tile rows.
     * Keeping all 32 hardware map rows referenced inflated Episode 2's
     * layer-1 working set beyond the 512-pattern cache and repeatedly forced
     * its O(512 * 32-byte) visual-approximation fallback.
     */
    BACKGROUND_VISIBLE_TILE_ROWS = SCREEN_HEIGHT / 8 + 1,
    BACKGROUND_CAMERA_RETAIN_TILE_ROWS =
        TYRIAN_GBA_SOFT_CROP_CAMERA ?
            (SOURCE_VIEW_CROP_Y + 7) / 8 :
            0,
    BACKGROUND_RETAINED_TILE_ROWS =
        BACKGROUND_VISIBLE_TILE_ROWS +
        BACKGROUND_CAMERA_RETAIN_TILE_ROWS * 2,
    BACKGROUND_SEMANTIC_CACHE_SIZE = 1024,
    BACKGROUND_SEMANTIC_WAYS = 4,
    BACKGROUND_SEMANTIC_SETS =
        BACKGROUND_SEMANTIC_CACHE_SIZE / BACKGROUND_SEMANTIC_WAYS,
    /*
     * Source-parity logic does not run on every 59.73 Hz display frame.
     * Decode ROMFS tiles only on those idle presentation frames so the
     * background work cannot push an already busy gameplay tick past
     * VBlank.  Four cache misses fit comfortably when no gameplay/render
     * pass ran and provide more throughput than two misses on every frame.
     * Six misses leaves headroom on an idle ARM7 frame while keeping the
     * three scrolling layers ahead of their next eight-pixel boundary.
     */
    BACKGROUND_PREFETCH_IDLE_MISSES = 6,
    BACKGROUND_PC_PALETTE_INDEX = 0,
};

_Static_assert(
    BACKGROUND_RETAINED_TILE_ROWS + 1 <= MAP_RING_ROWS,
    "retained camera window plus prefetch row must fit the hardware ring"
);

typedef struct {
    u32 key;
    u16 word;
    u16 slot_generation;
} BackgroundSemanticEntry;

typedef struct {
    u8 (*pattern)[BACKGROUND_TILE_BYTES];
    u16 *reference_count;
    u16 *generation;
    u8 *valid;
    u32 *pattern_hash;
    u16 *pattern_next;
    u16 *free_queue;
    u8 *free_queued;
    u16 capacity;
    u16 pool_offset;
    u16 physical_start;
    u16 tile_index_offset;
    u8 char_base;
    BackgroundSemanticEntry
        semantic[BACKGROUND_SEMANTIC_CACHE_SIZE];
    u8 semantic_replace[BACKGROUND_SEMANTIC_SETS];
    /*
     * Collision-safe pattern index.  The former one-entry-per-bucket table
     * could forget a still-resident pattern when another hash landed in the
     * same bucket, then allocate a duplicate slot.  Near a 512-pattern
     * visible set those duplicates could trigger a false capacity fallback.
     * Across all three layers, hash + next + bucket heads uses the same
     * 12 KiB as the old three direct tables while preserving every resident
     * pattern.
     */
    u16 pattern_bucket_head[BACKGROUND_PATTERN_BUCKETS];
    u16 ring_word[MAP_RING_ROWS][BACKGROUND_ROW_TILES];
    u16 ring_source_row[MAP_RING_ROWS];
    u8 ring_reference_rows;
    u16 prefetch_word[BACKGROUND_ROW_TILES];
    u16 prefetch_row;
    u16 upload_slot[BACKGROUND_MAX_ROW_UPLOADS];
    u8 upload_count;
    u8 prefetch_column;
    u8 prefetch_active;
    u8 prefetch_ready;
    u8 prefetch_existing;
    u16 allocation_cursor;
    u16 valid_count;
    u16 free_head;
    u16 free_tail;
    u16 free_count;
    u8 prepared;
} BackgroundLayerCache;

static BackgroundLayerCache
    background_layer_cache[BACKGROUND_LAYER_COUNT]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 background_pattern_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS][BACKGROUND_TILE_BYTES]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 background_reference_count_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 background_generation_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 background_valid_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u32 background_pattern_hash_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 background_pattern_next_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u16 background_free_queue_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 background_free_queued_pool
    [BACKGROUND_TOTAL_CACHE_SLOTS]
    EWRAM_BSS __attribute__((aligned(4)));
static OtDataView background_map_view[BACKGROUND_LAYER_COUNT];
static OtDataView background_lookup_view[BACKGROUND_LAYER_COUNT];
/*
 * Only two pointer reads feed each newly materialized 8x8 pattern, whereas
 * the 4 KiB nearest-colour table is read for every pixel pair.  The enlarged
 * stress build therefore keeps the colour LUT in IWRAM but places this
 * 2.4 KiB bank view in EWRAM to preserve stack headroom.
 */
static const u8 *background_shape_pixels[BACKGROUND_SHAPE_COUNT]
    STRESS_EWRAM_BSS;
/*
 * Nearest colour within each selected 4bpp palette bank.
 */
static u8 background_palette_nearest
    [BACKGROUND_PALETTE_BANKS][OT_PALETTE_COLOUR_COUNT]
    EWRAM_BSS __attribute__((aligned(4)));
static const u8 *background_palette_mask_active;
static const u8 *background_palette_level_mask_active;
static const u8 *background_palette_mask_id_active;
/*
 * palette.dat uses the low nibble as brightness, but an RGB-energy lookup is
 * a better generic tie-breaker for special banks.  Keep this tiny hot table
 * in IWRAM; it replaces the expensive per-tile 16-bank RGB error search.
 */
static u32 background_cache_approximations;
static u32 background_cache_evictions;
static u32 background_cache_uploads;
static u32 background_rows_prefetched;
static u32 background_rows_synchronous;
static u32 background_rows_existing;
static u32 background_prefetch_late_columns;
static u32 background_tile_render_count;
static u8 background_runtime_valid;
static u8 background_prefetch_round_robin;

/*
 * A full-source scan of all 62 logical levels found per-layer 22-row maxima
 * of 533 / 434 / 454 patterns.  This single cartridge-wide partition keeps
 * a safety margin without level tables or load-time whole-map analysis.
 *
 * Physical slots:
 *   layer 0:   0..575  through char base 0, indices   0..575
 *   layer 1: 576..1055 through char base 1, indices  64..543
 *   layer 2: 1056..1535 through char base 2, indices 32..511
 */
static const u16 background_cache_capacity[BACKGROUND_LAYER_COUNT] = {
    BACKGROUND_LAYER0_CACHE_SLOTS,
    BACKGROUND_LAYER1_CACHE_SLOTS,
    BACKGROUND_LAYER2_CACHE_SLOTS,
};

static const u16 background_cache_pool_offset[BACKGROUND_LAYER_COUNT] = {
    0,
    BACKGROUND_LAYER0_CACHE_SLOTS,
    BACKGROUND_LAYER0_CACHE_SLOTS + BACKGROUND_LAYER1_CACHE_SLOTS,
};

static const u16 background_cache_physical_start
    [BACKGROUND_LAYER_COUNT] = {
        0,
        BACKGROUND_LAYER0_CACHE_SLOTS,
        BACKGROUND_LAYER0_CACHE_SLOTS +
            BACKGROUND_LAYER1_CACHE_SLOTS,
    };

static const u16 background_cache_tile_index_offset
    [BACKGROUND_LAYER_COUNT] = {
        0,
        BACKGROUND_LAYER0_CACHE_SLOTS - 512,
        BACKGROUND_LAYER0_CACHE_SLOTS +
            BACKGROUND_LAYER1_CACHE_SLOTS - 1024,
    };

static const u8 background_cache_char_base[BACKGROUND_LAYER_COUNT] = {
    0, 1, 2,
};

_Static_assert(
    BACKGROUND_TOTAL_CACHE_SLOTS == 1478,
    "BG character pool must reserve 58 charblock-2 tiles for stats"
);
_Static_assert(
    BACKGROUND_LAYER0_CACHE_SLOTS <= 1024 &&
        BACKGROUND_LAYER0_CACHE_SLOTS - 512 +
            BACKGROUND_LAYER1_CACHE_SLOTS <= 1024 &&
        BACKGROUND_LAYER0_CACHE_SLOTS +
            BACKGROUND_LAYER1_CACHE_SLOTS - 1024 +
            BACKGROUND_LAYER2_CACHE_SLOTS <= 1024,
    "each BG partition must fit its 10-bit character-index window"
);
_Static_assert(
    BACKGROUND_TOTAL_CACHE_SLOTS * BACKGROUND_TILE_BYTES <
        BG0_SCREEN_BLOCK * 2048,
    "BG character pool must stop before the first gameplay screen block"
);

static const u16 background_map_first_row[BACKGROUND_LAYER_COUNT] = {
    OT_LEVEL_MAP1_FIRST_SOURCE_ROW,
    OT_LEVEL_MAP23_FIRST_SOURCE_ROW,
    OT_LEVEL_MAP23_FIRST_SOURCE_ROW,
};

static const u16 background_map_rows[BACKGROUND_LAYER_COUNT] = {
    OT_LEVEL_MAP1_ROWS,
    OT_LEVEL_MAP2_ROWS,
    OT_LEVEL_MAP3_ROWS,
};

static const u8 background_map_columns[BACKGROUND_LAYER_COUNT] = {
    OT_LEVEL_MAP1_COLUMNS,
    OT_LEVEL_MAP2_COLUMNS,
    OT_LEVEL_MAP3_COLUMNS,
};

/*
 * OpenTyrian fills ref[layer][] from shapes?.dat with these exact bounds:
 *
 *   layer 1: x <= 71
 *   layer 2: x <= 71, but x == 71 is forced to NULL
 *   layer 3: x <= 71, but x >= 70 is forced to NULL
 *
 * The stock maps deliberately use 71 as an empty-cell sentinel (and layer 3
 * may also use 70).  Looking those values up through the otherwise populated
 * 128-entry mapSh table turns empty space into an unrelated repeated shape.
 */
static const u8 background_map_shape_slots[BACKGROUND_LAYER_COUNT] = {
    72,
    71,
    70,
};

static const u16 background_canvas_rows[BACKGROUND_LAYER_COUNT] = {
    (
        (
            OT_LEVEL_MAP1_ROWS -
            OT_LEVEL_MAP1_FIRST_SOURCE_ROW
        ) * BACKGROUND_SHAPE_HEIGHT + 7
    ) / 8,
    (
        (
            OT_LEVEL_MAP2_ROWS -
            OT_LEVEL_MAP23_FIRST_SOURCE_ROW
        ) * BACKGROUND_SHAPE_HEIGHT + 7
    ) / 8,
    (
        (
            OT_LEVEL_MAP3_ROWS -
            OT_LEVEL_MAP23_FIRST_SOURCE_ROW
        ) * BACKGROUND_SHAPE_HEIGHT + 7
    ) / 8,
};

static u8 background_parse_shapes(const OtDataView *file)
{
    u32 position = 0;
    u16 shape;

    memset(background_shape_pixels, 0, sizeof(background_shape_pixels));
    for (shape = 0; shape < BACKGROUND_SHAPE_COUNT; shape++) {
        u8 blank;

        if (position >= file->size) return 0;
        blank = file->data[position++];
        if (blank == 0) {
            if (
                BACKGROUND_SHAPE_BYTES >
                file->size - position
            ) {
                return 0;
            }
            background_shape_pixels[shape] = file->data + position;
            position += BACKGROUND_SHAPE_BYTES;
        }
    }
    return 1;
}

static u8 background_palette_variant(char shape_file_id)
{
    if (shape_file_id >= 'A' && shape_file_id <= 'Z') {
        shape_file_id = (char)(shape_file_id + ('a' - 'A'));
    }
    switch (shape_file_id) {
    case ')': return 0;
    case 'w': return 1;
    case 'x': return 2;
    case 'y': return 3;
    case 'z': return 4;
    default: return UINT8_MAX;
    }
}

static u8 background_palette_level_variant(
    u8 episode,
    u16 lvl_file_number
)
{
    u16 offset;
    u16 count;

    switch (episode) {
    case 1:
        offset = BACKGROUND_PALETTE_EPISODE_1_OFFSET;
        count = BACKGROUND_PALETTE_EPISODE_1_COUNT;
        break;
    case 2:
        offset = BACKGROUND_PALETTE_EPISODE_2_OFFSET;
        count = BACKGROUND_PALETTE_EPISODE_2_COUNT;
        break;
    case 3:
        offset = BACKGROUND_PALETTE_EPISODE_3_OFFSET;
        count = BACKGROUND_PALETTE_EPISODE_3_COUNT;
        break;
    case 4:
        offset = BACKGROUND_PALETTE_EPISODE_4_OFFSET;
        count = BACKGROUND_PALETTE_EPISODE_4_COUNT;
        break;
    default:
        return UINT8_MAX;
    }
    if (
        lvl_file_number == 0 ||
        lvl_file_number > count
    ) {
        return UINT8_MAX;
    }
    return (u8)(offset + lvl_file_number - 1u);
}

static u8 background_setup_palette(
    char shape_file_id,
    u8 episode,
    u16 lvl_file_number
)
{
    u8 profile_variant =
        background_palette_variant(shape_file_id);
    u8 level_variant = background_palette_level_variant(
        episode,
        lvl_file_number
    );
    u8 palette_variant;

    _Static_assert(
        BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT == 5 &&
            BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT == 62 &&
            BACKGROUND_PALETTE_VARIANT_COUNT ==
                BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT +
                    BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT &&
            BACKGROUND_GBA_PALETTE_VARIANT_BYTES == 512 &&
            BACKGROUND_GBA_PALETTE_BYTES ==
                BACKGROUND_PALETTE_VARIANT_COUNT *
                    BACKGROUND_GBA_PALETTE_VARIANT_BYTES &&
            BACKGROUND_PALETTE_NEAREST_VARIANT_BYTES ==
                BACKGROUND_PALETTE_BANKS *
                    OT_PALETTE_COLOUR_COUNT &&
            BACKGROUND_PALETTE_NEAREST_BYTES ==
                BACKGROUND_PALETTE_VARIANT_COUNT *
                    BACKGROUND_PALETTE_NEAREST_VARIANT_BYTES &&
            BACKGROUND_PALETTE_MASK_BANK_VARIANT_BYTES == 65536 &&
            BACKGROUND_PALETTE_MASK_ID_OFFSET ==
                BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT *
                    BACKGROUND_PALETTE_MASK_BANK_VARIANT_BYTES &&
            BACKGROUND_PALETTE_LEVEL_MASK_BANK_OFFSET ==
                BACKGROUND_PALETTE_MASK_ID_OFFSET + 65536 &&
            BACKGROUND_PALETTE_LEVEL_MASK_BANK_VARIANT_BYTES ==
                BACKGROUND_PALETTE_ACTIVE_MASK_COUNT &&
            BACKGROUND_PALETTE_MASK_BANK_BYTES ==
                BACKGROUND_PALETTE_LEVEL_MASK_BANK_OFFSET +
                    BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT *
                        BACKGROUND_PALETTE_ACTIVE_MASK_COUNT &&
            BACKGROUND_PALETTE_EPISODE_1_OFFSET == 0 &&
            BACKGROUND_PALETTE_EPISODE_2_OFFSET ==
                BACKGROUND_PALETTE_EPISODE_1_COUNT &&
            BACKGROUND_PALETTE_EPISODE_3_OFFSET ==
                BACKGROUND_PALETTE_EPISODE_2_OFFSET +
                    BACKGROUND_PALETTE_EPISODE_2_COUNT &&
            BACKGROUND_PALETTE_EPISODE_4_OFFSET ==
                BACKGROUND_PALETTE_EPISODE_3_OFFSET +
                    BACKGROUND_PALETTE_EPISODE_3_COUNT &&
            BACKGROUND_PALETTE_EPISODE_4_OFFSET +
                BACKGROUND_PALETTE_EPISODE_4_COUNT ==
                    BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT,
        "generated background palette adapter changed"
    );
    if (
        profile_variant >=
            BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT
    ) {
        return 0;
    }
    palette_variant = profile_variant;
    background_palette_level_mask_active = 0;
    background_palette_mask_id_active = 0;
    if (
        level_variant <
            BACKGROUND_PALETTE_LEVEL_VARIANT_COUNT
    ) {
        palette_variant = (u8)(
            BACKGROUND_PALETTE_PROFILE_VARIANT_COUNT +
            level_variant
        );
        background_palette_mask_id_active =
            background_palette_mask_bank +
            BACKGROUND_PALETTE_MASK_ID_OFFSET;
        background_palette_level_mask_active =
            background_palette_mask_bank +
            BACKGROUND_PALETTE_LEVEL_MASK_BANK_OFFSET +
            (u32)level_variant *
                BACKGROUND_PALETTE_ACTIVE_MASK_COUNT;
    }
    dmaCopy(
        background_gba_palette +
            (u32)palette_variant *
                BACKGROUND_GBA_PALETTE_VARIANT_BYTES,
        BG_PALETTE,
        BACKGROUND_GBA_PALETTE_VARIANT_BYTES
    );
    memcpy(
        background_palette_nearest,
        background_palette_nearest_asset +
            (u32)palette_variant *
                BACKGROUND_PALETTE_NEAREST_VARIANT_BYTES,
        BACKGROUND_PALETTE_NEAREST_VARIANT_BYTES
    );
    background_palette_mask_active =
        background_palette_mask_bank +
        (u32)profile_variant *
            BACKGROUND_PALETTE_MASK_BANK_VARIANT_BYTES;
    return 1;
}

static u8 background_bind_selected_level(void)
{
    OtLevelInfo level_info;
    OtDataView shape_file;
    u8 layer;

    background_runtime_valid = 0;
    if (
        !ot_data_level_info(&level_info) ||
        !ot_data_background_shape_file_view(
            level_info.shape_file,
            &shape_file
        ) ||
        !background_parse_shapes(&shape_file)
    ) {
        return 0;
    }
    for (layer = 0; layer < BACKGROUND_LAYER_COUNT; layer++) {
        if (
            !ot_data_level_map_view(
                layer,
                &background_map_view[layer]
            ) ||
            !ot_data_level_map_shape_view(
                layer,
                &background_lookup_view[layer]
            )
        ) {
            return 0;
        }
    }
    if (
        !background_setup_palette(
            level_info.shape_file,
            level_info.episode,
            level_info.lvl_file_number
        )
    ) {
        return 0;
    }
    background_cache_approximations = 0;
    background_cache_evictions = 0;
    background_cache_uploads = 0;
    background_rows_prefetched = 0;
    background_rows_synchronous = 0;
    background_rows_existing = 0;
    background_prefetch_late_columns = 0;
    background_tile_render_count = 0;
    background_prefetch_round_robin = 0;
    background_runtime_valid = 1;
    return 1;
}

static IWRAM_CODE ARM_CODE u16 background_shape_number_for_map_index(
    u8 layer,
    u8 map_index
)
{
    const OtDataView *lookup = &background_lookup_view[layer];
    u16 shape_number;

    if (map_index >= background_map_shape_slots[layer]) return 0;
    shape_number = (u16)(
        ((u16)lookup->data[(u16)map_index * 2u] << 8) |
        lookup->data[(u16)map_index * 2u + 1u]
    );
    if (
        shape_number == 0 ||
        shape_number > BACKGROUND_SHAPE_COUNT
    ) {
        return 0;
    }
    return background_shape_pixels[shape_number - 1u] != 0 ?
        shape_number :
        0;
}

static IWRAM_CODE ARM_CODE u16 background_map_shape_number(
    u8 layer,
    u16 map_row,
    u8 map_column
)
{
    const OtDataView *map = &background_map_view[layer];
    u8 map_index;

    if (
        map_row >= background_map_rows[layer] ||
        map_column >= background_map_columns[layer]
    ) {
        return 0;
    }
    map_index = map->data[
        (u32)map_row * background_map_columns[layer] + map_column
    ];
    return background_shape_number_for_map_index(layer, map_index);
}

#if TYRIAN_GBA_STRESS_LOADOUT
static ARM_CODE __attribute__((noinline, noclone)) u32
#else
static IWRAM_CODE ARM_CODE u32
#endif
background_tile_key(
    u8 layer,
    u16 tile_x,
    u16 tile_y
)
{
    u16 canvas_x = (u16)(tile_x * 8u);
    u16 canvas_y = (u16)(tile_y * 8u);
    u16 map_row;
    u16 top_shape;
    u16 bottom_shape = 0;
    u8 map_column;
    u8 sub_x;
    u8 phase;
    u32 packed;

    if (
        canvas_x >=
            (u16)background_map_columns[layer] *
                BACKGROUND_SHAPE_WIDTH ||
        tile_y >= background_canvas_rows[layer]
    ) {
        return 1;
    }
    map_row = (u16)(
        background_map_first_row[layer] +
        canvas_y / BACKGROUND_SHAPE_HEIGHT
    );
    map_column = (u8)(canvas_x / BACKGROUND_SHAPE_WIDTH);
    sub_x = (u8)(
        (canvas_x % BACKGROUND_SHAPE_WIDTH) / 8u
    );
    phase = (u8)(canvas_y % BACKGROUND_SHAPE_HEIGHT);
    top_shape = background_map_shape_number(
        layer,
        map_row,
        map_column
    );
    if (phase + 7u >= BACKGROUND_SHAPE_HEIGHT) {
        bottom_shape = background_map_shape_number(
            layer,
            (u16)(map_row + 1u),
            map_column
        );
    }
    if (top_shape == 0 && bottom_shape == 0) return 1;
    packed =
        top_shape |
        ((u32)bottom_shape << 10) |
        ((u32)(phase / 4u) << 20) |
        ((u32)sub_x << 23);
    return packed + 1u;
}

static IWRAM_CODE ARM_CODE void background_render_tile(
    u32 key,
    u8 *pattern,
    u8 *palette_bank
)
{
    u8 source[64];
    u32 packed = key - 1u;
    u16 top_shape = (u16)(packed & 0x03ffu);
    u16 bottom_shape = (u16)((packed >> 10) & 0x03ffu);
    u8 phase = (u8)(((packed >> 20) & 7u) * 4u);
    u8 sub_x = (u8)((packed >> 23) & 3u);
    const u8 *top = top_shape != 0 ?
        background_shape_pixels[top_shape - 1u] :
        0;
    const u8 *bottom = bottom_shape != 0 ?
        background_shape_pixels[bottom_shape - 1u] :
        0;
    u16 hue_mask = 0;
    u8 best_bank = 0;
    u8 y;

    for (y = 0; y < 8; y++) {
        u8 x;

        for (x = 0; x < 8; x++) {
            u8 source_y = (u8)(phase + y);
            const u8 *shape = top;

            if (source_y >= BACKGROUND_SHAPE_HEIGHT) {
                source_y = (u8)(
                    source_y - BACKGROUND_SHAPE_HEIGHT
                );
                shape = bottom;
            }
            source[(u8)(y * 8u + x)] = shape != 0 ?
                shape[
                    (u16)source_y * BACKGROUND_SHAPE_WIDTH +
                    sub_x * 8u + x
                ] :
                0;
            if (source[(u8)(y * 8u + x)] != 0) {
                hue_mask |= (u16)(
                    1u <<
                    (source[(u8)(y * 8u + x)] >> 4)
                );
            }
        }
    }
    if (hue_mask != 0) {
        /*
         * Build-time training assigns each stock hue combination to one of
         * eleven exact or five mixed-material banks.  The 64 KiB table lives
         * in ROM, so a miss still performs one O(1) bank read plus the same
         * 64 source-pixel pass as before.
         */
        if (
            background_palette_level_mask_active != 0 &&
            background_palette_mask_id_active != 0
        ) {
            u8 mask_id =
                background_palette_mask_id_active[hue_mask];

            best_bank =
                mask_id < BACKGROUND_PALETTE_ACTIVE_MASK_COUNT ?
                    background_palette_level_mask_active[mask_id] :
                    background_palette_mask_active[hue_mask];
        } else {
            best_bank = background_palette_mask_active[hue_mask];
        }
    }
    for (y = 0; y < 8; y++) {
        u8 pair;

        for (pair = 0; pair < 4; pair++) {
            u8 left = source[(u8)(y * 8u + pair * 2u)];
            u8 right = source[
                (u8)(y * 8u + pair * 2u + 1u)
            ];

            pattern[(u8)(y * 4u + pair)] = (u8)(
                background_palette_nearest[best_bank][left] |
                (
                    background_palette_nearest[best_bank][right]
                    << 4
                )
            );
        }
    }
    *palette_bank = best_bank;
}

static inline u16 background_word_local_slot(
    const BackgroundLayerCache *cache,
    u16 word
)
{
    u16 hardware_index = word & 0x03ffu;

    if (
        hardware_index < cache->tile_index_offset ||
        hardware_index >=
            cache->tile_index_offset + cache->capacity
    ) {
        return cache->capacity;
    }
    return (u16)(hardware_index - cache->tile_index_offset);
}

static inline u16 background_cache_next(
    u16 value,
    u16 capacity
)
{
    value++;
    return value < capacity ? value : 0;
}

static IWRAM_CODE ARM_CODE u8 background_semantic_lookup(
    u8 layer,
    u32 key,
    u16 *word
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u32 hash = key;
    u16 base;
    u8 way;

    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    base = (u16)(
        (hash & (BACKGROUND_SEMANTIC_SETS - 1u)) *
        BACKGROUND_SEMANTIC_WAYS
    );
    for (way = 0; way < BACKGROUND_SEMANTIC_WAYS; way++) {
        BackgroundSemanticEntry *entry =
            &cache->semantic[base + way];
        u16 slot;

        if (entry->key != key) continue;
        slot = background_word_local_slot(cache, entry->word);
        if (
            slot < cache->capacity &&
            cache->valid[slot] &&
            entry->slot_generation == cache->generation[slot]
        ) {
            *word = entry->word;
            return 1;
        }
    }
    return 0;
}

static __attribute__((noinline, noclone)) void
background_semantic_store(
    u8 layer,
    u32 key,
    u16 word
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u32 hash = key;
    u16 base;
    u16 replacement = UINT16_MAX;
    u8 set;
    u8 way;

    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    set = (u8)(hash & (BACKGROUND_SEMANTIC_SETS - 1u));
    base = (u16)(set * BACKGROUND_SEMANTIC_WAYS);
    for (way = 0; way < BACKGROUND_SEMANTIC_WAYS; way++) {
        BackgroundSemanticEntry *entry =
            &cache->semantic[base + way];
        u16 slot = background_word_local_slot(cache, entry->word);

        if (entry->key == key || entry->key == 0) {
            replacement = (u16)(base + way);
            break;
        }
        if (
            replacement == UINT16_MAX &&
            (
                slot >= cache->capacity ||
                !cache->valid[slot] ||
                entry->slot_generation != cache->generation[slot]
            )
        ) {
            replacement = (u16)(base + way);
        }
    }
    if (replacement == UINT16_MAX) {
        way = cache->semantic_replace[set];
        cache->semantic_replace[set] = (u8)(
            (way + 1u) & (BACKGROUND_SEMANTIC_WAYS - 1u)
        );
        replacement = (u16)(base + way);
    }
    cache->semantic[replacement].key = key;
    cache->semantic[replacement].word = word;
    cache->semantic[replacement].slot_generation =
        cache->generation[background_word_local_slot(cache, word)];
}

static IWRAM_CODE ARM_CODE u16 background_cache_acquire(
    u8 layer,
    const u8 *pattern,
    u8 queue_upload
);
static IWRAM_CODE ARM_CODE u8 background_prefetch_layer(
    u8 layer,
    u16 target_row,
    u8 column_budget
);

static IWRAM_CODE ARM_CODE u16 background_resolve_word(
    u8 layer,
    u16 source_row,
    u8 column,
    u8 queue_upload,
    u8 *rendered
)
{
    u8 pattern[BACKGROUND_TILE_BYTES];
    u32 key = background_tile_key(layer, column, source_row);
    u8 palette_bank;
    u16 slot;
    u16 word;

    if (background_semantic_lookup(layer, key, &word)) {
        if (rendered != 0) *rendered = 0;
        return word;
    }
    if (rendered != 0) *rendered = 1;
    background_tile_render_count++;
    background_render_tile(key, pattern, &palette_bank);
    slot = background_cache_acquire(
        layer,
        pattern,
        queue_upload
    );
    word = (u16)(
        (
            slot +
            background_layer_cache[layer].tile_index_offset
        ) |
        ((u16)palette_bank << 12)
    );
    background_semantic_store(layer, key, word);
    return word;
}

static IWRAM_CODE ARM_CODE u32 background_tile_hash(const u8 *pattern)
{
    u32 hash = 2166136261u;
    u8 index;

    for (index = 0; index < BACKGROUND_TILE_BYTES; index++) {
        hash ^= pattern[index];
        hash *= 16777619u;
    }
    return hash;
}

static u16 background_pattern_difference(
    const u8 *left,
    const u8 *right
)
{
    u16 difference = 0;
    u8 index;

    for (index = 0; index < BACKGROUND_TILE_BYTES; index++) {
        u8 value = (u8)(left[index] ^ right[index]);

        if ((value & 0x0f) != 0) difference++;
        if ((value & 0xf0) != 0) difference++;
    }
    return difference;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u16
background_pattern_lookup(
    const BackgroundLayerCache *cache,
    const u8 *pattern,
    u32 hash
)
{
    u16 bucket = (u16)(
        (hash ^ (hash >> 16)) &
        (BACKGROUND_PATTERN_BUCKETS - 1u)
    );
    u16 slot = cache->pattern_bucket_head[bucket];

    while (slot != UINT16_MAX) {
        if (
            slot >= cache->capacity ||
            !cache->valid[slot]
        ) {
            break;
        }
        if (
            cache->pattern_hash[slot] == hash &&
            memcmp(
                cache->pattern[slot],
                pattern,
                BACKGROUND_TILE_BYTES
            ) == 0
        ) {
            return slot;
        }
        slot = cache->pattern_next[slot];
    }
    return cache->capacity;
}

static __attribute__((noinline, noclone)) void
background_pattern_unlink(
    BackgroundLayerCache *cache,
    u16 slot
)
{
    u32 hash;
    u16 bucket;
    u16 current;
    u16 previous = UINT16_MAX;

    if (
        slot >= cache->capacity ||
        !cache->valid[slot]
    ) {
        return;
    }
    hash = cache->pattern_hash[slot];
    bucket = (u16)(
        (hash ^ (hash >> 16)) &
        (BACKGROUND_PATTERN_BUCKETS - 1u)
    );
    current = cache->pattern_bucket_head[bucket];
    while (current != UINT16_MAX) {
        if (current == slot) {
            if (previous == UINT16_MAX) {
                cache->pattern_bucket_head[bucket] =
                    cache->pattern_next[current];
            } else {
                cache->pattern_next[previous] =
                    cache->pattern_next[current];
            }
            break;
        }
        if (current >= cache->capacity) break;
        previous = current;
        current = cache->pattern_next[current];
    }
    cache->pattern_next[slot] = UINT16_MAX;
}

static void background_pattern_insert(
    BackgroundLayerCache *cache,
    u16 slot,
    u32 hash
)
{
    u16 bucket = (u16)(
        (hash ^ (hash >> 16)) &
        (BACKGROUND_PATTERN_BUCKETS - 1u)
    );

    cache->pattern_hash[slot] = hash;
    cache->pattern_next[slot] =
        cache->pattern_bucket_head[bucket];
    cache->pattern_bucket_head[bucket] = slot;
}

static IWRAM_CODE ARM_CODE void background_queue_unreferenced(
    BackgroundLayerCache *cache,
    u16 slot
)
{
    if (
        cache->reference_count[slot] != 0 ||
        cache->free_queued[slot] ||
        cache->free_count >= cache->capacity
    ) {
        return;
    }
    cache->free_queue[cache->free_tail] = slot;
    cache->free_tail = background_cache_next(
        cache->free_tail,
        cache->capacity
    );
    cache->free_count++;
    cache->free_queued[slot] = 1;
}

static IWRAM_CODE ARM_CODE u16 background_cache_acquire(
    u8 layer,
    const u8 *pattern,
    u8 queue_upload
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u32 hash = background_tile_hash(pattern);
    u16 slot;
    u16 replacement;

    slot = background_pattern_lookup(cache, pattern, hash);
    if (slot < cache->capacity) return slot;
    replacement = cache->capacity;
    if (cache->valid_count < cache->capacity) {
        for (slot = 0; slot < cache->capacity; slot++) {
            u16 candidate = (u16)(
                cache->allocation_cursor + slot
            );

            if (candidate >= cache->capacity) {
                candidate = (u16)(candidate - cache->capacity);
            }

            if (!cache->valid[candidate]) {
                replacement = candidate;
                cache->valid_count++;
                break;
            }
        }
    } else {
        while (cache->free_count != 0) {
            u16 candidate = cache->free_queue[cache->free_head];

            cache->free_head = background_cache_next(
                cache->free_head,
                cache->capacity
            );
            cache->free_count--;
            cache->free_queued[candidate] = 0;
            if (cache->reference_count[candidate] == 0) {
                replacement = candidate;
                break;
            }
        }
    }
    if (
        cache->valid_count == cache->capacity &&
        replacement == cache->capacity
    ) {
        for (slot = 0; slot < cache->capacity; slot++) {
            u16 candidate = (u16)(
                cache->allocation_cursor + slot
            );

            if (candidate >= cache->capacity) {
                candidate = (u16)(candidate - cache->capacity);
            }

            if (cache->reference_count[candidate] == 0) {
                replacement = candidate;
                break;
            }
        }
    }
    if (replacement == cache->capacity) {
        u16 best_difference = UINT16_MAX;

        /*
         * More distinct patterns than this layer's cartridge-wide partition
         * are simultaneously visible. Preserve the authored palette and
         * select only the nearest cached 4bpp pattern; this is the sole
         * lossy hardware-limit fallback.
         */
        for (slot = 0; slot < cache->capacity; slot++) {
            u16 difference = background_pattern_difference(
                pattern,
                cache->pattern[slot]
            );

            if (difference < best_difference) {
                best_difference = difference;
                replacement = slot;
                if (difference == 0) break;
            }
        }
        background_cache_approximations++;
        return replacement;
    }
    cache->allocation_cursor = background_cache_next(
        replacement,
        cache->capacity
    );
    if (cache->valid[replacement]) {
        background_pattern_unlink(cache, replacement);
        background_cache_evictions++;
    }
    memcpy(
        cache->pattern[replacement],
        pattern,
        BACKGROUND_TILE_BYTES
    );
    cache->valid[replacement] = 1;
    cache->generation[replacement]++;
    if (cache->generation[replacement] == 0) {
        cache->generation[replacement] = 1;
    }
    background_pattern_insert(cache, replacement, hash);
    if (
        queue_upload &&
        cache->upload_count < BACKGROUND_MAX_ROW_UPLOADS
    ) {
        cache->upload_slot[cache->upload_count++] = replacement;
    }
    return replacement;
}

static __attribute__((noinline, noclone)) void
background_release_ring_row(
    u8 layer,
    u8 ring_row
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 column;

    if (cache->ring_source_row[ring_row] == UINT16_MAX) return;
    for (column = 0; column < BACKGROUND_ROW_TILES; column++) {
        u16 slot = background_word_local_slot(
            cache,
            cache->ring_word[ring_row][column]
        );

        if (
            slot < cache->capacity &&
            cache->reference_count[slot] != 0
        ) {
            cache->reference_count[slot]--;
            if (cache->reference_count[slot] == 0) {
                background_queue_unreferenced(cache, slot);
            }
        }
    }
    cache->ring_source_row[ring_row] = UINT16_MAX;
    if (cache->ring_reference_rows != 0) {
        cache->ring_reference_rows--;
    }
}

static void background_build_ring_row(
    u8 layer,
    u16 source_row,
    u8 replacing
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 ring_row = (u8)(source_row & (MAP_RING_ROWS - 1u));
    u8 column;

    if (replacing) background_release_ring_row(layer, ring_row);
    cache->upload_count = 0;
    for (column = 0; column < BACKGROUND_ROW_TILES; column++) {
        u16 word = background_resolve_word(
            layer,
            source_row,
            column,
            replacing,
            0
        );
        u16 slot = background_word_local_slot(cache, word);

        cache->ring_word[ring_row][column] = word;
        if (slot < cache->capacity) {
            cache->reference_count[slot]++;
        }
    }
    cache->ring_source_row[ring_row] = source_row;
    cache->ring_reference_rows++;
}

static void background_copy_ring_row(
    u8 layer,
    u16 source_row,
    u16 screen_block
)
{
    const u16 *source = background_layer_cache[layer].ring_word[
        source_row & (MAP_RING_ROWS - 1u)
    ];
    u16 row_slot = (u16)((source_row & 31u) * 32u);
    u16 *left = (u16 *)SCREEN_BASE_BLOCK(screen_block);
    u16 *right = (u16 *)SCREEN_BASE_BLOCK(screen_block + 1u);

    dmaCopy(source, left + row_slot, MAP_HALF_ROW_BYTES);
    dmaCopy(
        source + 32,
        right + row_slot,
        MAP_HALF_ROW_BYTES
    );
}

static u8 background_prepare_layer(u8 layer, u16 top_row)
{
    BackgroundLayerCache *cache;
    u16 first = top_row;
    u16 last = (u16)(first + BACKGROUND_VISIBLE_TILE_ROWS);
    u16 row;

    if (
        !background_runtime_valid ||
        layer >= BACKGROUND_LAYER_COUNT
    ) {
        return 0;
    }
    if (last > background_canvas_rows[layer]) {
        last = background_canvas_rows[layer];
    }
    cache = &background_layer_cache[layer];
    memset(cache, 0, sizeof(*cache));
    cache->capacity = background_cache_capacity[layer];
    cache->pool_offset = background_cache_pool_offset[layer];
    cache->physical_start =
        background_cache_physical_start[layer];
    cache->tile_index_offset =
        background_cache_tile_index_offset[layer];
    cache->char_base = background_cache_char_base[layer];
    cache->pattern =
        background_pattern_pool + cache->pool_offset;
    cache->reference_count =
        background_reference_count_pool + cache->pool_offset;
    cache->generation =
        background_generation_pool + cache->pool_offset;
    cache->valid =
        background_valid_pool + cache->pool_offset;
    cache->pattern_hash =
        background_pattern_hash_pool + cache->pool_offset;
    cache->pattern_next =
        background_pattern_next_pool + cache->pool_offset;
    cache->free_queue =
        background_free_queue_pool + cache->pool_offset;
    cache->free_queued =
        background_free_queued_pool + cache->pool_offset;
    memset(
        cache->pattern,
        0,
        (u32)cache->capacity * BACKGROUND_TILE_BYTES
    );
    memset(
        cache->reference_count,
        0,
        (u32)cache->capacity * sizeof(*cache->reference_count)
    );
    memset(
        cache->generation,
        0,
        (u32)cache->capacity * sizeof(*cache->generation)
    );
    memset(cache->valid, 0, cache->capacity);
    memset(
        cache->pattern_hash,
        0,
        (u32)cache->capacity * sizeof(*cache->pattern_hash)
    );
    memset(
        cache->pattern_next,
        0xff,
        (u32)cache->capacity * sizeof(*cache->pattern_next)
    );
    memset(
        cache->free_queue,
        0,
        (u32)cache->capacity * sizeof(*cache->free_queue)
    );
    memset(cache->free_queued, 0, cache->capacity);
    memset(
        cache->pattern_bucket_head,
        0xff,
        sizeof(cache->pattern_bucket_head)
    );
    for (row = 0; row < MAP_RING_ROWS; row++) {
        cache->ring_source_row[row] = UINT16_MAX;
    }
    for (row = first; row < last; row++) {
        background_build_ring_row(layer, row, 0);
    }
    cache->upload_count = 0;
    cache->prepared = 1;
    return 1;
}

static void background_commit_full_layer(
    u8 layer,
    u16 top_row,
    u16 screen_block
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u16 first = top_row;
    u16 last = (u16)(first + BACKGROUND_VISIBLE_TILE_ROWS);
    u16 row;

    dmaCopy(
        cache->pattern,
        (u8 *)CHAR_BASE_BLOCK(0) +
            (u32)cache->physical_start * BACKGROUND_TILE_BYTES,
        (u32)cache->capacity * BACKGROUND_TILE_BYTES
    );
    {
        static const u32 blank_word = 0;

        DMA3COPY(
            &blank_word,
            SCREEN_BASE_BLOCK(screen_block),
            DMA_SRC_FIXED | DMA32 |
                (
                    MAP_SCREEN_BLOCK_WORDS * 2u * sizeof(u16) /
                    sizeof(u32)
                )
        );
    }
    if (last > background_canvas_rows[layer]) {
        last = background_canvas_rows[layer];
    }
    for (row = first; row < last; row++) {
        background_copy_ring_row(layer, row, screen_block);
    }
}

static IWRAM_CODE ARM_CODE u8 background_schedule_row(
    u8 layer,
    u16 row,
    u16 screen_block,
    const u8 **source,
    u16 **target
)
{
    BackgroundLayerCache *cache;

    if (
        !background_runtime_valid ||
        layer >= BACKGROUND_LAYER_COUNT ||
        row >= background_canvas_rows[layer]
    ) {
        return 0;
    }
    cache = &background_layer_cache[layer];
    if (!cache->prepared) return 0;
    while (
        cache->prefetch_active &&
        cache->prefetch_row == row &&
        !cache->prefetch_ready
    ) {
        background_prefetch_late_columns +=
            BACKGROUND_ROW_TILES - cache->prefetch_column;
        background_prefetch_layer(
            layer,
            row,
            BACKGROUND_ROW_TILES
        );
    }
    if (cache->ring_source_row[row & 31u] == row) {
        /* A farther-ahead prefetch may already own queued tile uploads. */
        background_rows_existing++;
    } else if (
        cache->prefetch_ready &&
        cache->prefetch_row == row &&
        !cache->prefetch_existing
    ) {
        u8 ring_row = (u8)(row & 31u);

        background_release_ring_row(layer, ring_row);
        memcpy(
            cache->ring_word[ring_row],
            cache->prefetch_word,
            sizeof(cache->prefetch_word)
        );
        cache->ring_source_row[ring_row] = row;
        cache->ring_reference_rows++;
        /*
         * Prefetch incremented references while building. They now become
         * the installed ring's references without another adjustment.
         */
        cache->prefetch_active = 0;
        cache->prefetch_ready = 0;
        background_rows_prefetched++;
    } else {
        background_build_ring_row(layer, row, 1);
        background_rows_synchronous++;
    }
    *source = (const u8 *)cache->ring_word[row & 31u];
    *target =
        (u16 *)SCREEN_BASE_BLOCK(screen_block) +
        ((row & 31u) * 32u);
    if (
        cache->ring_reference_rows >
        telemetry_background_held_rows_max
    ) {
        telemetry_background_held_rows_max =
            cache->ring_reference_rows;
    }
    return 1;
}

/*
 * Drop rows which can no longer be sampled by the presentation viewport.
 * This runs before the next scroll update: a row installed during the prior
 * update therefore overlaps the old presentation window for one frame, then
 * its outgoing bottom row becomes reclaimable before any later VBlank upload.
 */
static IWRAM_CODE ARM_CODE void background_retain_visible_rows(
    u8 layer,
    u16 top_row
)
{
    BackgroundLayerCache *cache;
    u16 retain_top =
        top_row > BACKGROUND_CAMERA_RETAIN_TILE_ROWS ?
            (u16)(
                top_row - BACKGROUND_CAMERA_RETAIN_TILE_ROWS
            ) :
            0;
    u16 retain_last =
        (u16)(
            top_row +
            BACKGROUND_VISIBLE_TILE_ROWS +
            BACKGROUND_CAMERA_RETAIN_TILE_ROWS
        );
    u8 ring_row;

    if (
        !background_runtime_valid ||
        layer >= BACKGROUND_LAYER_COUNT
    ) {
        return;
    }
    cache = &background_layer_cache[layer];
    if (!cache->prepared) return;
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
    if (
        presentation_render_pending &&
        presentation_registers_valid &&
        !presentation_release_held_window
    ) {
        u16 held_top =
            gameplay_presentation.full_scroll_pixel[layer] >> 3;
        u16 held_last =
            (u16)(
                held_top +
                BACKGROUND_VISIBLE_TILE_ROWS +
                BACKGROUND_CAMERA_RETAIN_TILE_ROWS
            );

        held_top =
            held_top > BACKGROUND_CAMERA_RETAIN_TILE_ROWS ?
                (u16)(
                    held_top - BACKGROUND_CAMERA_RETAIN_TILE_ROWS
                ) :
                0;

        /*
         * The two viewports differ by at most the forced-presentation
         * window.  Retaining their bounding interval is equivalent while
         * they overlap, and remains conservative if a future scroll mode
         * ever separates them: the gap is retained briefly, never sampled
         * incorrectly or released too early.
         */
        if (held_top < retain_top) retain_top = held_top;
        if (held_last > retain_last) retain_last = held_last;
    }
#endif
    for (ring_row = 0; ring_row < MAP_RING_ROWS; ring_row++) {
        u16 source_row = cache->ring_source_row[ring_row];

        if (
            source_row != UINT16_MAX &&
            (
                source_row < retain_top ||
                source_row >= retain_last
            )
        ) {
            background_release_ring_row(layer, ring_row);
        }
    }
    if (
        cache->ring_reference_rows >
        telemetry_background_held_rows_max
    ) {
        telemetry_background_held_rows_max =
            cache->ring_reference_rows;
    }
}

static IWRAM_CODE ARM_CODE void background_cancel_prefetch(u8 layer)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 column;

    if (
        !cache->prefetch_active ||
        cache->prefetch_existing
    ) {
        cache->prefetch_active = 0;
        cache->prefetch_ready = 0;
        return;
    }
    for (column = 0; column < cache->prefetch_column; column++) {
        u16 slot = background_word_local_slot(
            cache,
            cache->prefetch_word[column]
        );

        if (
            slot < cache->capacity &&
            cache->reference_count[slot] != 0
        ) {
            cache->reference_count[slot]--;
            if (cache->reference_count[slot] == 0) {
                background_queue_unreferenced(cache, slot);
            }
        }
    }
    cache->prefetch_active = 0;
    cache->prefetch_ready = 0;
}

static IWRAM_CODE ARM_CODE u8 background_prefetch_layer(
    u8 layer,
    u16 target_row,
    u8 column_budget
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 work = 0;

    if (
        !cache->prepared ||
        target_row >= background_canvas_rows[layer]
    ) {
        return 0;
    }
    if (
        cache->prefetch_active &&
        cache->prefetch_row != target_row
    ) {
        background_cancel_prefetch(layer);
    }
    if (!cache->prefetch_active) {
        cache->prefetch_row = target_row;
        cache->prefetch_column = 0;
        cache->prefetch_ready = 0;
        cache->prefetch_existing =
            cache->ring_source_row[target_row & 31u] ==
                target_row;
        cache->prefetch_active = 1;
        if (cache->prefetch_existing) {
            cache->prefetch_ready = 1;
            return 0;
        }
    }
    if (cache->prefetch_ready) return 0;
    while (
        cache->prefetch_column < BACKGROUND_ROW_TILES &&
        work < column_budget
    ) {
        u8 rendered;
        u16 word = background_resolve_word(
            layer,
            target_row,
            cache->prefetch_column,
            1,
            &rendered
        );
        u16 slot = background_word_local_slot(cache, word);

        cache->prefetch_word[cache->prefetch_column++] = word;
        if (slot < cache->capacity) {
            cache->reference_count[slot]++;
        }
        work = (u8)(work + rendered);
    }
    if (cache->prefetch_column == BACKGROUND_ROW_TILES) {
        cache->prefetch_ready = 1;
    }
    return work;
}

static IWRAM_CODE ARM_CODE u16 background_next_missing_row(
    u8 layer,
    u16 current_row
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 distance;

    if (
        cache->prefetch_active &&
        cache->prefetch_row < current_row
    ) {
        return cache->prefetch_row;
    }
    for (distance = 1; distance <= MAP_RING_ROWS; distance++) {
        u16 candidate;

        if (current_row < distance) break;
        candidate = (u16)(current_row - distance);
        if (
            cache->ring_source_row[candidate & 31u] !=
                candidate
        ) {
            return candidate;
        }
    }
    return current_row != 0 ? (u16)(current_row - 1u) : 0;
}

static IWRAM_CODE ARM_CODE void background_prefetch_step(u8 miss_budget)
{
    u8 used = 0;
    u8 pass;

    if (
        !background_runtime_valid ||
        game_state != STATE_PLAY ||
        miss_budget == 0
    ) {
        return;
    }
    for (pass = 0; pass < BACKGROUND_LAYER_COUNT; pass++) {
        u8 layer = (u8)(background_prefetch_round_robin + pass);
        u16 row;
        u8 active;

        if (layer >= BACKGROUND_LAYER_COUNT) {
            layer = (u8)(layer - BACKGROUND_LAYER_COUNT);
        }
        if (used >= miss_budget) break;
        if (layer == 0) {
            row = background_presentation_scroll_pixel(
                0,
                source_camera_offset_y
            ) >> 3;
            active = bg1_scroll_speed != 0;
        } else if (layer == 1) {
            row = background_presentation_scroll_pixel(
                1,
                source_camera_offset_y
            ) >> 3;
            active =
                source_background2_enabled &&
                !source_background2_upload_pending &&
                bg2_scroll_speed != 0;
        } else {
            row = background_presentation_scroll_pixel(
                2,
                source_camera_offset_y
            ) >> 3;
            active = bg3_scroll_speed != 0;
        }
        if (!active || row == 0) continue;
        /*
         * A just-installed row overlaps the previous presentation window for
         * one frame.  Do not add another prefetched row until that outgoing
         * row has been released, keeping the maximum ownership window at the
         * 25-row camera hysteresis band plus one prefetched row rather than
         * silently growing back toward the old 32-row problem.
         */
        if (
            background_layer_cache[layer].ring_reference_rows >
            BACKGROUND_RETAINED_TILE_ROWS
        ) {
            continue;
        }
        used = (u8)(
            used +
            background_prefetch_layer(
                layer,
                background_next_missing_row(layer, row),
                (u8)(miss_budget - used)
            )
        );
    }
    background_prefetch_round_robin++;
    if (
        background_prefetch_round_robin >=
        BACKGROUND_LAYER_COUNT
    ) {
        background_prefetch_round_robin = 0;
    }
}

static IWRAM_CODE ARM_CODE void background_commit_row_uploads(u8 layer)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 index;

    for (index = 0; index < cache->upload_count; index++) {
        u16 slot = cache->upload_slot[index];

        dmaCopy(
            cache->pattern[slot],
            (u8 *)CHAR_BASE_BLOCK(0) +
                (
                    (u32)cache->physical_start + slot
                ) * BACKGROUND_TILE_BYTES,
            BACKGROUND_TILE_BYTES
        );
        background_cache_uploads++;
    }
    cache->upload_count = 0;
}
