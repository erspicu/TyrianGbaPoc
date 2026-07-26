/*
 * Generic Tyrian background adapter.
 *
 * No GBA-native level background is stored in the cartridge.  The selected
 * tyrianN.lvl map, its big-endian shape lookup, shapes?.dat pixels and the
 * stock palette.dat are read directly from ROMFS.  Only the active 32-row
 * GBA ring and a reclaimable 512-pattern cache per layer live in EWRAM.
 */

enum {
    BACKGROUND_LAYER_COUNT = 3,
    BACKGROUND_CACHE_SLOTS = 512,
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

typedef struct {
    u32 key;
    u16 word;
    u16 slot_generation;
} BackgroundSemanticEntry;

typedef struct {
    u32 hash;
    u16 slot;
    u16 slot_generation;
} BackgroundPatternEntry;

typedef struct {
    u8 pattern[BACKGROUND_CACHE_SLOTS][BACKGROUND_TILE_BYTES];
    u16 reference_count[BACKGROUND_CACHE_SLOTS];
    u16 generation[BACKGROUND_CACHE_SLOTS];
    u8 valid[BACKGROUND_CACHE_SLOTS];
    BackgroundSemanticEntry
        semantic[BACKGROUND_SEMANTIC_CACHE_SIZE];
    u8 semantic_replace[BACKGROUND_SEMANTIC_SETS];
    BackgroundPatternEntry
        pattern_index[BACKGROUND_CACHE_SLOTS];
    u16 free_queue[BACKGROUND_CACHE_SLOTS];
    u8 free_queued[BACKGROUND_CACHE_SLOTS];
    u16 ring_word[MAP_RING_ROWS][BACKGROUND_ROW_TILES];
    u16 ring_source_row[MAP_RING_ROWS];
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
static OtDataView background_map_view[BACKGROUND_LAYER_COUNT];
static OtDataView background_lookup_view[BACKGROUND_LAYER_COUNT];
static const u8 *background_shape_pixels[BACKGROUND_SHAPE_COUNT];
static u8 background_palette_nearest
    [BACKGROUND_PALETTE_BANKS][OT_PALETTE_COLOUR_COUNT];
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

static u16 background_rgb_to_gba(const u8 *rgb)
{
    return (u16)(
        (rgb[0] >> 1) |
        ((rgb[1] >> 1) << 5) |
        ((rgb[2] >> 1) << 10)
    );
}

static u32 background_rgb_distance(const u8 *left, const u8 *right)
{
    s16 red = (s16)left[0] - right[0];
    s16 green = (s16)left[1] - right[1];
    s16 blue = (s16)left[2] - right[2];

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}

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

static u8 background_setup_palette(void)
{
    OtDataView palette;
    u8 bank;

    if (!ot_data_palette_view(BACKGROUND_PC_PALETTE_INDEX, &palette)) {
        return 0;
    }
    for (bank = 0; bank < BACKGROUND_PALETTE_BANKS; bank++) {
        u16 source_index;

        for (
            source_index = 0;
            source_index < OT_PALETTE_COLOUR_COUNT;
            source_index++
        ) {
            u8 best = 0;
            u32 best_error = 0;
            u8 candidate;

            if (source_index != 0) {
                best_error = 0xffffffffu;
                /*
                 * GBA BG colour zero is transparent in every 4bpp bank.
                 * Keep raw Tyrian zero transparent, but never map an opaque
                 * source pixel onto that hardware index.
                 */
                for (
                    candidate = 1;
                    candidate < BACKGROUND_PALETTE_BANK_COLOURS;
                    candidate++
                ) {
                    u16 candidate_index =
                        (u16)(bank * 16u + candidate);
                    u32 error = background_rgb_distance(
                        palette.data + (u32)source_index * 3u,
                        palette.data + (u32)candidate_index * 3u
                    );

                    if (error < best_error) {
                        best_error = error;
                        best = candidate;
                    }
                }
            }
            background_palette_nearest[bank][source_index] = best;
        }
        for (
            source_index = 0;
            source_index < BACKGROUND_PALETTE_BANK_COLOURS;
            source_index++
        ) {
            u16 palette_index = (u16)(bank * 16u + source_index);

            BG_PALETTE[palette_index] = background_rgb_to_gba(
                palette.data + (u32)palette_index * 3u
            );
        }
    }
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
    if (!background_setup_palette()) return 0;
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

static IWRAM_CODE ARM_CODE u16 background_map_shape_number(
    u8 layer,
    u16 map_row,
    u8 map_column
)
{
    const OtDataView *map = &background_map_view[layer];
    const OtDataView *lookup = &background_lookup_view[layer];
    u8 map_index;
    u16 shape_number;

    if (
        map_row >= background_map_rows[layer] ||
        map_column >= background_map_columns[layer]
    ) {
        return 0;
    }
    map_index = map->data[
        (u32)map_row * background_map_columns[layer] + map_column
    ];
    if (map_index >= OT_LEVEL_MAP_SHAPE_COUNT) return 0;
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

static IWRAM_CODE ARM_CODE u32 background_tile_key(
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
    u8 hue_count[BACKGROUND_PALETTE_BANKS] = {0};
    u8 best_bank = 0;
    u8 best_count = 0;
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
                hue_count[
                    source[(u8)(y * 8u + x)] >> 4
                ]++;
            }
        }
    }
    for (y = 0; y < BACKGROUND_PALETTE_BANKS; y++) {
        if (hue_count[y] > best_count) {
            best_count = hue_count[y];
            best_bank = y;
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
        slot = entry->word & 0x01ffu;
        if (
            cache->valid[slot] &&
            entry->slot_generation == cache->generation[slot]
        ) {
            *word = entry->word;
            return 1;
        }
    }
    return 0;
}

static IWRAM_CODE ARM_CODE void background_semantic_store(
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
        u16 slot = entry->word & 0x01ffu;

        if (entry->key == key || entry->key == 0) {
            replacement = (u16)(base + way);
            break;
        }
        if (
            replacement == UINT16_MAX &&
            (
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
        cache->generation[word & 0x01ffu];
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
    word = (u16)(slot | ((u16)palette_bank << 12));
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

static IWRAM_CODE ARM_CODE u16 background_pattern_difference(
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

static IWRAM_CODE ARM_CODE void background_queue_unreferenced(
    BackgroundLayerCache *cache,
    u16 slot
)
{
    if (
        cache->reference_count[slot] != 0 ||
        cache->free_queued[slot] ||
        cache->free_count >= BACKGROUND_CACHE_SLOTS
    ) {
        return;
    }
    cache->free_queue[cache->free_tail] = slot;
    cache->free_tail = (u16)(
        (cache->free_tail + 1u) &
        (BACKGROUND_CACHE_SLOTS - 1u)
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
    u16 pattern_index = (u16)(
        (hash ^ (hash >> 16)) &
        (BACKGROUND_CACHE_SLOTS - 1u)
    );
    BackgroundPatternEntry *indexed =
        &cache->pattern_index[pattern_index];
    u16 slot;
    u16 replacement;

    slot = indexed->slot;
    if (
        indexed->hash == hash &&
        slot < BACKGROUND_CACHE_SLOTS &&
        cache->valid[slot] &&
        indexed->slot_generation == cache->generation[slot] &&
        memcmp(
            cache->pattern[slot],
            pattern,
            BACKGROUND_TILE_BYTES
        ) == 0
    ) {
        return slot;
    }
    replacement = BACKGROUND_CACHE_SLOTS;
    if (cache->valid_count < BACKGROUND_CACHE_SLOTS) {
        for (slot = 0; slot < BACKGROUND_CACHE_SLOTS; slot++) {
            u16 candidate = (u16)(
                (cache->allocation_cursor + slot) &
                (BACKGROUND_CACHE_SLOTS - 1u)
            );

            if (!cache->valid[candidate]) {
                replacement = candidate;
                cache->valid_count++;
                break;
            }
        }
    } else {
        while (cache->free_count != 0) {
            u16 candidate = cache->free_queue[cache->free_head];

            cache->free_head = (u16)(
                (cache->free_head + 1u) &
                (BACKGROUND_CACHE_SLOTS - 1u)
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
        cache->valid_count == BACKGROUND_CACHE_SLOTS &&
        replacement == BACKGROUND_CACHE_SLOTS
    ) {
        for (slot = 0; slot < BACKGROUND_CACHE_SLOTS; slot++) {
            u16 candidate = (u16)(
                (cache->allocation_cursor + slot) &
                (BACKGROUND_CACHE_SLOTS - 1u)
            );

            if (cache->reference_count[candidate] == 0) {
                replacement = candidate;
                break;
            }
        }
    }
    if (replacement == BACKGROUND_CACHE_SLOTS) {
        u16 best_difference = UINT16_MAX;

        /*
         * More than 512 distinct patterns are simultaneously visible.
         * Preserve the authored palette and select only the nearest cached
         * 4bpp pattern; this is the sole lossy hardware-limit fallback.
         */
        for (slot = 0; slot < BACKGROUND_CACHE_SLOTS; slot++) {
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
    cache->allocation_cursor = (u16)(
        (replacement + 1u) &
        (BACKGROUND_CACHE_SLOTS - 1u)
    );
    if (cache->valid[replacement]) background_cache_evictions++;
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
    indexed->hash = hash;
    indexed->slot = replacement;
    indexed->slot_generation = cache->generation[replacement];
    if (
        queue_upload &&
        cache->upload_count < BACKGROUND_MAX_ROW_UPLOADS
    ) {
        cache->upload_slot[cache->upload_count++] = replacement;
    }
    return replacement;
}

static IWRAM_CODE ARM_CODE void background_release_ring_row(
    u8 layer,
    u8 ring_row
)
{
    BackgroundLayerCache *cache = &background_layer_cache[layer];
    u8 column;

    for (column = 0; column < BACKGROUND_ROW_TILES; column++) {
        u16 slot = cache->ring_word[ring_row][column] & 0x01ffu;

        if (cache->reference_count[slot] != 0) {
            cache->reference_count[slot]--;
            if (cache->reference_count[slot] == 0) {
                background_queue_unreferenced(cache, slot);
            }
        }
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
        u16 slot = word & 0x01ffu;

        cache->ring_word[ring_row][column] = word;
        cache->reference_count[slot]++;
    }
    cache->ring_source_row[ring_row] = source_row;
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
    u16 last = (u16)(first + MAP_RING_ROWS);
    u16 row;

    if (
        !background_runtime_valid ||
        layer >= BACKGROUND_LAYER_COUNT
    ) {
        return 0;
    }
    if (last > background_canvas_rows[layer]) {
        last = background_canvas_rows[layer];
        first = (u16)(last - MAP_RING_ROWS);
    }
    cache = &background_layer_cache[layer];
    memset(cache, 0, sizeof(*cache));
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
    u16 last = (u16)(first + MAP_RING_ROWS);
    u16 row;

    dmaCopy(
        cache->pattern,
        CHAR_BASE_BLOCK(layer),
        BACKGROUND_CACHE_SLOTS * BACKGROUND_TILE_BYTES
    );
    if (last > background_canvas_rows[layer]) {
        last = background_canvas_rows[layer];
        first = (u16)(last - MAP_RING_ROWS);
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
    return 1;
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
        u16 slot = cache->prefetch_word[column] & 0x01ffu;

        if (cache->reference_count[slot] != 0) {
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
        u16 slot = word & 0x01ffu;

        cache->prefetch_word[cache->prefetch_column++] = word;
        cache->reference_count[slot]++;
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
            row = bg1_scroll_pixel >> 3;
            active = bg1_scroll_speed != 0;
        } else if (layer == 1) {
            row = bg2_scroll_pixel >> 3;
            active =
                source_background2_enabled &&
                !source_background2_upload_pending &&
                bg2_scroll_speed != 0;
        } else {
            row = bg3_scroll_pixel >> 3;
            active = bg3_scroll_speed != 0;
        }
        if (!active || row == 0) continue;
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
            (u8 *)CHAR_BASE_BLOCK(layer) +
                (u32)slot * BACKGROUND_TILE_BYTES,
            BACKGROUND_TILE_BYTES
        );
        background_cache_uploads++;
    }
    cache->upload_count = 0;
}
