/*
 * GBA-only presentation adapter for very large, multi-link Sprite2 bosses.
 *
 * A stock size-one Sprite2 frame is a sparse 24x28 picture carried in a
 * 32x32 8bpp OBJ canvas (1 KiB).  Some late-game bosses expose roughly forty
 * different canvases at once, while the ordinary L1 layout can hold only 24.
 * OAM still has room, so rotating OAM entries cannot solve that mismatch.
 *
 * During those scenes only, physical full-size enemy slots 2..22 are split
 * into two 512-byte 4bpp canvases.  Two palettes are trained once from the
 * actual visible frames and active level palette.  This yields 42 stable
 * 32x32 canvases without generated per-level art or changes to gameplay
 * state.  Boss pieces are admitted first, then ordinary enemies can use any
 * remaining compact canvases; explosions remain the first presentation
 * class allowed to yield under pressure.
 */

#if !TYRIAN_GBA_STRESS_LOADOUT

enum {
    SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT = 2u,
    SOURCE_BOSS_COMPACT_FULL_SLOT_COUNT =
        SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT -
            SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT,
    SOURCE_BOSS_COMPACT_SLOT_COUNT =
        SOURCE_BOSS_COMPACT_FULL_SLOT_COUNT * 2u,
    SOURCE_BOSS_COMPACT_FRAME_BYTES = SOURCE_ENEMY_FRAME_BYTES / 2u,
    SOURCE_BOSS_COMPACT_TILES_PER_FRAME =
        SOURCE_ENEMY_TILES_PER_SLOT / 2u,
    SOURCE_BOSS_COMPACT_PALETTE_COUNT = 2u,
    SOURCE_BOSS_COMPACT_PALETTE_A = OBJ_PAL_EXPLOSION,
    SOURCE_BOSS_COMPACT_PALETTE_B = OBJ_PAL_GAME_OVER,
    SOURCE_BOSS_COMPACT_TRIGGER_COUNT =
        SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT,
};

_Static_assert(
    SOURCE_BOSS_COMPACT_SLOT_COUNT >= 40u &&
        SOURCE_BOSS_COMPACT_SLOT_COUNT <= UINT8_MAX,
    "release boss compact cache must hold the SAVARA assembly"
);

typedef struct {
    u32 last_epoch;
    u32 generation;
    u16 graphic;
    u8 shape_table;
    u8 size;
    u8 filter;
    u8 palette_group;
    u8 valid;
    u8 resident;
} SourceBossCompactSlot;

typedef struct {
    u32 slot_generation;
    u32 l2_generation;
    u8 slot;
    u8 l2_slot;
} SourceBossCompactUpload;

static SourceBossCompactSlot source_boss_compact_cache[
    SOURCE_BOSS_COMPACT_SLOT_COUNT
] EWRAM_BSS;
static SourceBossCompactUpload source_boss_compact_uploads[
    SOURCE_BOSS_COMPACT_SLOT_COUNT
] EWRAM_BSS;
static u16 source_boss_compact_histogram[
    SOURCE_BOSS_COMPACT_PALETTE_COUNT
][256] EWRAM_BSS;
/*
 * Palette training runs below render_game(), whose frame is already large.
 * Keep this scratch table out of the user stack so entering a large boss
 * cannot collide with the IRQ stack/canary boundary.
 */
static u8 source_boss_compact_table_count[
    SPRITE2_RAW_TABLE_COUNT + 1u
] EWRAM_BSS;
static u8 source_boss_compact_palette_map[
    SOURCE_BOSS_COMPACT_PALETTE_COUNT
][256] EWRAM_BSS;
static u16 source_boss_compact_palette_error[
    SOURCE_BOSS_COMPACT_PALETTE_COUNT
][256] EWRAM_BSS;
static u8 source_boss_compact_palette_colour[
    SOURCE_BOSS_COMPACT_PALETTE_COUNT
][15] EWRAM_BSS;
static u32 source_boss_compact_epoch EWRAM_BSS;
static u8 source_boss_compact_upload_count EWRAM_BSS;
static u8 source_boss_compact_active EWRAM_BSS;
static u8 source_boss_compact_palette_valid EWRAM_BSS;
static u8 source_boss_compact_primary_table EWRAM_BSS;
static u8 source_boss_compact_palette_installed EWRAM_BSS;

static u8 source_boss_compact_palette_bank(u8 group)
{
    return group == 0 ?
        SOURCE_BOSS_COMPACT_PALETTE_A :
        SOURCE_BOSS_COMPACT_PALETTE_B;
}

static u8 source_boss_compact_physical_slot(u8 compact_slot)
{
    return (u8)(
        SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT +
        (compact_slot >> 1)
    );
}

static u16 source_boss_compact_tile(u8 compact_slot)
{
    return (u16)(
        source_enemy_cache_tile(
            source_boss_compact_physical_slot(compact_slot)
        ) +
        (compact_slot & 1u) * SOURCE_BOSS_COMPACT_TILES_PER_FRAME
    );
}

static u8 source_boss_compact_owns_enemy_slot(u8 slot)
{
    return
        source_boss_compact_active &&
        slot >= SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT &&
        slot < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
}

static u8 source_boss_compact_upload_pending(
    u8 slot,
    u32 generation
)
{
    u8 index;

    for (index = 0; index < source_boss_compact_upload_count; index++) {
        if (
            source_boss_compact_uploads[index].slot == slot &&
            source_boss_compact_uploads[index].slot_generation == generation
        ) {
            return 1;
        }
    }
    return 0;
}

static void source_boss_compact_cancel_upload(u8 slot)
{
    u8 index = 0;

    while (index < source_boss_compact_upload_count) {
        SourceBossCompactUpload *upload =
            &source_boss_compact_uploads[index];

        if (upload->slot != slot) {
            index++;
            continue;
        }
        source_sprite2_l2_unpin(
            upload->l2_slot,
            upload->l2_generation
        );
        source_boss_compact_upload_count--;
        if (index < source_boss_compact_upload_count) {
            source_boss_compact_uploads[index] =
                source_boss_compact_uploads[
                    source_boss_compact_upload_count
                ];
        }
        telemetry_sprite2_upload_coalesces++;
        telemetry_sprite2_pending_cancellations++;
    }
}

static void source_boss_compact_invalidate_slot(u8 slot)
{
    if (slot >= SOURCE_BOSS_COMPACT_SLOT_COUNT) return;
    source_boss_compact_cancel_upload(slot);
    source_boss_compact_cache[slot].valid = 0;
    source_boss_compact_cache[slot].resident = 0;
    source_boss_compact_cache[slot].generation =
        source_cache_next_generation();
}

static u16 source_boss_compact_colour(u8 absolute_index)
{
    return source_detail_obj_palette_base_valid ?
        source_detail_obj_palette_base[absolute_index] :
        SPRITE_PALETTE[absolute_index];
}

static u32 source_boss_compact_colour_distance(u16 left, u16 right)
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

static __attribute__((noinline, noclone)) void
source_boss_compact_select_palette(u8 group)
{
    u8 selected_count = 0;
    u8 palette_index;

    memset(
        source_boss_compact_palette_colour[group],
        0,
        sizeof(source_boss_compact_palette_colour[group])
    );
    while (selected_count < 15u) {
        u32 best_score = 0;
        u8 best_colour = 0;
        u16 candidate;

        for (candidate = 1; candidate < 256u; candidate++) {
            u16 frequency =
                source_boss_compact_histogram[group][candidate];
            u32 nearest = 0xffffffffu;
            u8 chosen;

            if (frequency == 0) continue;
            for (chosen = 0; chosen < selected_count; chosen++) {
                if (
                    source_boss_compact_palette_colour[group][chosen] ==
                    candidate
                ) {
                    nearest = 0;
                    break;
                }
                {
                    u32 distance = source_boss_compact_colour_distance(
                        source_boss_compact_colour((u8)candidate),
                        source_boss_compact_colour(
                            source_boss_compact_palette_colour[group][chosen]
                        )
                    );

                    if (distance < nearest) nearest = distance;
                }
            }
            if (selected_count == 0) nearest = 1u;
            /* Frequency keeps broad ramps; distance protects rare accents. */
            {
                u32 score = nearest * (frequency + 8u);

                if (score > best_score) {
                    best_score = score;
                    best_colour = (u8)candidate;
                }
            }
        }
        if (best_colour == 0) break;
        source_boss_compact_palette_colour[group][selected_count++] =
            best_colour;
    }
    if (selected_count == 0 && group != 0) {
        memcpy(
            source_boss_compact_palette_colour[group],
            source_boss_compact_palette_colour[0],
            sizeof(source_boss_compact_palette_colour[group])
        );
        selected_count = 15u;
    }

    source_obj_palette_base_write(
        (u16)(source_boss_compact_palette_bank(group) * 16u),
        0
    );
    for (palette_index = 0; palette_index < 15u; palette_index++) {
        u8 absolute =
            source_boss_compact_palette_colour[group][palette_index];
        u16 colour = absolute != 0 ?
            source_boss_compact_colour(absolute) : 0;

        source_obj_palette_base_write(
            (u16)(
                source_boss_compact_palette_bank(group) * 16u +
                palette_index + 1u
            ),
            colour
        );
    }
    source_boss_compact_palette_map[group][0] = 0;
    source_boss_compact_palette_error[group][0] = 0;
    for (palette_index = 1; palette_index != 0; palette_index++) {
        u32 nearest = 0xffffffffu;
        u8 nearest_nibble = 1u;
        u8 chosen;
        u16 source_colour = source_boss_compact_colour(palette_index);

        for (chosen = 0; chosen < 15u; chosen++) {
            u8 absolute =
                source_boss_compact_palette_colour[group][chosen];
            u32 distance;

            if (absolute == 0) continue;
            distance = source_boss_compact_colour_distance(
                source_colour,
                source_boss_compact_colour(absolute)
            );
            if (distance < nearest) {
                nearest = distance;
                nearest_nibble = (u8)(chosen + 1u);
            }
        }
        source_boss_compact_palette_map[group][palette_index] =
            nearest_nibble;
        source_boss_compact_palette_error[group][palette_index] =
            (u16)nearest;
    }
}

/*
 * The two compact palettes are trained from the dominant Boss table and
 * the remaining visible tables, but later attack phases can introduce a
 * frame whose colours fit the other palette much better.  Shape-table
 * ownership is therefore only a tie-breaker: choose the lower-error palette
 * for every newly decoded frame.  This keeps late red/cyan accents without
 * changing either palette (and hence without invalidating resident tiles).
 */
static u8 source_boss_compact_best_palette_group(
    const u8 *pixels,
    u8 preferred_group
)
{
    u32 error[SOURCE_BOSS_COMPACT_PALETTE_COUNT] = {0};
    u16 pixel;

    for (pixel = 0; pixel < SOURCE_ENEMY_FRAME_BYTES; pixel++) {
        u8 colour = pixels[pixel];

        if (colour == 0) continue;
        error[0] += source_boss_compact_palette_error[0][colour];
        error[1] += source_boss_compact_palette_error[1][colour];
    }
    if (error[preferred_group ^ 1u] < error[preferred_group]) {
        return (u8)(preferred_group ^ 1u);
    }
    return preferred_group;
}

static __attribute__((noinline, noclone)) u8
source_boss_compact_train_palette(
    const SourceFrameOamCandidates *frame
)
{
    u16 position;
    u8 best_table = 0;
    u8 best_count = 0;

    memset(
        source_boss_compact_table_count,
        0,
        sizeof(source_boss_compact_table_count)
    );
    for (position = 0; position < frame->enemy_count; position++) {
        const SourceEnemyOamCandidate *candidate = &frame->enemy[position];
        const OtEnemyDrawCommand *draw =
            &source_parity_level.enemy_draw[candidate->index];

        if (
            (candidate->flags & SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) == 0 ||
            draw->size != 1 ||
            draw->shape_table == 0 ||
            draw->shape_table > SPRITE2_RAW_TABLE_COUNT
        ) {
            continue;
        }
        if (
            ++source_boss_compact_table_count[draw->shape_table] >
            best_count
        ) {
            best_count =
                source_boss_compact_table_count[draw->shape_table];
            best_table = draw->shape_table;
        }
    }
    if (best_table == 0) return 0;
    source_boss_compact_primary_table = best_table;
    memset(
        source_boss_compact_histogram,
        0,
        sizeof(source_boss_compact_histogram)
    );
    for (position = 0; position < frame->enemy_count; position++) {
        const SourceEnemyOamCandidate *candidate = &frame->enemy[position];
        const OtEnemyDrawCommand *draw =
            &source_parity_level.enemy_draw[candidate->index];
        u8 group;
        u8 l2_slot;
        const u8 *pixels;
        u16 pixel;

        if (draw->size != 1) {
            continue;
        }
        group = draw->shape_table == best_table ? 0u : 1u;
        l2_slot = source_sprite2_l2_acquire(
            draw->shape_table,
            draw->graphic,
            draw->size,
            draw->filter
        );
        if (l2_slot == 0xff) return 0;
        pixels = source_sprite2_l2_tiles[l2_slot];
        for (pixel = 0; pixel < SOURCE_ENEMY_FRAME_BYTES; pixel++) {
            u8 colour = pixels[pixel];

            if (
                colour != 0 &&
                source_boss_compact_histogram[group][colour] != 0xffffu
            ) {
                source_boss_compact_histogram[group][colour]++;
            }
        }
    }
    source_boss_compact_select_palette(0);
    source_boss_compact_select_palette(1);
    source_boss_compact_palette_valid = 1;
    source_boss_compact_palette_installed = 1;
    return 1;
}

static void source_boss_compact_restore_palettes(void)
{
    static const u8 banks[SOURCE_BOSS_COMPACT_PALETTE_COUNT] = {
        SOURCE_BOSS_COMPACT_PALETTE_A,
        SOURCE_BOSS_COMPACT_PALETTE_B,
    };
    u8 group;

    if (!source_boss_compact_palette_installed) return;
    for (group = 0; group < SOURCE_BOSS_COMPACT_PALETTE_COUNT; group++) {
        u8 colour;

        for (colour = 0; colour < 16u; colour++) {
            u16 index = (u16)(banks[group] * 16u + colour);

            source_obj_palette_base_write(
                index,
                ((const u16 *)(const void *)obj_palette)[index]
            );
        }
    }
    source_boss_compact_palette_installed = 0;
}

static void source_boss_compact_deactivate(void)
{
    u8 slot;

    for (slot = 0; slot < SOURCE_BOSS_COMPACT_SLOT_COUNT; slot++) {
        source_boss_compact_invalidate_slot(slot);
    }
    source_boss_compact_active = 0;
    source_boss_compact_palette_valid = 0;
    source_boss_compact_primary_table = 0;
    source_boss_compact_epoch = 0;
    source_boss_compact_restore_palettes();
    for (
        slot = SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT;
        slot < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
        slot++
    ) {
        /* Boss 4bpp halves replaced both bytes of each former 8bpp slot. */
        source_enemy_cache_invalidate_slot(slot);
    }
}

static void source_boss_compact_cache_reset(void)
{
    source_boss_compact_deactivate();
    memset(
        source_boss_compact_cache,
        0,
        sizeof(source_boss_compact_cache)
    );
    memset(
        source_boss_compact_uploads,
        0,
        sizeof(source_boss_compact_uploads)
    );
    source_boss_compact_upload_count = 0;
}

static __attribute__((noinline, noclone)) u8
source_boss_compact_prepare(
    const SourceFrameOamCandidates *frame
)
{
    u8 environment_ready = (u8)(
        game_state == STATE_PLAY &&
        !frontend_demo_active &&
        source_parity_level.display_time == 0 &&
        source_option_projectile_valid == 0 &&
        source_special_option_projectile_graphic == 0
    );
    u8 boss_context = (u8)(
        frame->boss_structure_count != 0 ||
        source_parity_level.boss_bar_link[0] != 0 ||
        source_parity_level.boss_bar_link[1] != 0
    );
    u8 capacity_pressure =
        frame->enemy_full_size_count >= SOURCE_BOSS_COMPACT_TRIGGER_COUNT;

    if (!environment_ready) {
        source_boss_compact_hold_frames = 0;
        if (source_boss_compact_active) source_boss_compact_deactivate();
        return 0;
    }
    if (boss_context) {
        source_boss_compact_hold_frames =
            SOURCE_BOSS_COMPACT_HOLD_FRAMES;
    } else if (source_boss_compact_active) {
        if (source_boss_compact_hold_frames != 0) {
            source_boss_compact_hold_frames--;
        } else {
            source_boss_compact_deactivate();
            return 0;
        }
    }
    if (!source_boss_compact_active) {
        u8 physical;

        if (!boss_context || !capacity_pressure) return 0;
        source_boss_compact_active = 1;
        source_boss_compact_epoch = 0;
        source_boss_compact_upload_count = 0;
        memset(
            source_boss_compact_cache,
            0,
            sizeof(source_boss_compact_cache)
        );
        for (
            physical = SOURCE_BOSS_COMPACT_FIRST_FULL_SLOT;
            physical < SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;
            physical++
        ) {
            source_enemy_cache_invalidate_slot(physical);
        }
    }
    source_boss_compact_epoch++;
    if (source_boss_compact_epoch == 0) {
        u8 slot;

        source_boss_compact_epoch = 1;
        for (slot = 0; slot < SOURCE_BOSS_COMPACT_SLOT_COUNT; slot++) {
            source_boss_compact_cache[slot].last_epoch = 0;
        }
    }
    if (
        !source_boss_compact_palette_valid &&
        !source_boss_compact_train_palette(frame)
    ) {
        source_boss_compact_deactivate();
        return 0;
    }
    return 1;
}

static void source_boss_compact_pack_in_place(
    u8 *pixels,
    u8 palette_group
)
{
    const u8 *map = source_boss_compact_palette_map[palette_group];
    u16 output;

    /* Forward packing is safe: output N never overwrites input 2N+2. */
    for (output = 0; output < SOURCE_BOSS_COMPACT_FRAME_BYTES; output++) {
        u16 input = (u16)(output << 1);

        pixels[output] = (u8)(
            map[pixels[input]] |
            (map[pixels[input + 1u]] << 4)
        );
    }
}

static u16 source_boss_compact_acquire(
    const OtEnemyDrawCommand *draw,
    u8 *palette_bank
)
{
    u8 preferred_palette_group;
    u8 palette_group;
    u8 index;
    u8 free_slot = SOURCE_BOSS_COMPACT_SLOT_COUNT;
    u8 eviction_slot = SOURCE_BOSS_COMPACT_SLOT_COUNT;
    u8 selected;
    u8 l2_slot;
    u32 oldest_epoch = 0xffffffffu;
    SourceBossCompactSlot *slot;

    if (
        !source_boss_compact_active ||
        !source_boss_compact_palette_valid ||
        draw->size != 1 ||
        palette_bank == 0
    ) {
        return 0xffff;
    }
    preferred_palette_group =
        draw->shape_table == source_boss_compact_primary_table ?
        0u : 1u;
    for (index = 0; index < SOURCE_BOSS_COMPACT_SLOT_COUNT; index++) {
        slot = &source_boss_compact_cache[index];
        if (
            slot->valid &&
            slot->shape_table == draw->shape_table &&
            slot->graphic == draw->graphic &&
            slot->size == draw->size &&
            slot->filter == draw->filter
        ) {
            if (
                slot->resident ||
                source_boss_compact_upload_pending(index, slot->generation)
            ) {
                slot->last_epoch = source_boss_compact_epoch;
                *palette_bank =
                    source_boss_compact_palette_bank(
                        slot->palette_group
                    );
                telemetry_sprite2_cache_hits++;
                return source_boss_compact_tile(index);
            }
            source_boss_compact_invalidate_slot(index);
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_BOSS_COMPACT_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != source_boss_compact_epoch &&
            slot->last_epoch < oldest_epoch
        ) {
            eviction_slot = index;
            oldest_epoch = slot->last_epoch;
        }
    }
    selected = free_slot != SOURCE_BOSS_COMPACT_SLOT_COUNT ?
        free_slot : eviction_slot;
    if (selected == SOURCE_BOSS_COMPACT_SLOT_COUNT) {
        telemetry_sprite2_cache_drops++;
        return 0xffff;
    }
    if (source_boss_compact_cache[selected].valid) {
        telemetry_sprite2_cache_evictions++;
    }
    source_boss_compact_invalidate_slot(selected);
    slot = &source_boss_compact_cache[selected];
    slot->generation = source_cache_next_generation();
    slot->last_epoch = source_boss_compact_epoch;
    slot->graphic = draw->graphic;
    slot->shape_table = draw->shape_table;
    slot->size = draw->size;
    slot->filter = draw->filter;
    slot->valid = 1;
    slot->resident = 0;
    telemetry_sprite2_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        draw->shape_table,
        draw->graphic,
        draw->size,
        draw->filter
    );
    if (l2_slot == 0xff) {
        source_boss_compact_invalidate_slot(selected);
        telemetry_sprite2_decode_failures++;
        return 0xffff;
    }
    if (
        !source_sprite2_l2_pin(
            l2_slot,
            source_sprite2_l2[l2_slot].generation
        )
    ) {
        source_boss_compact_invalidate_slot(selected);
        return 0xffff;
    }
    palette_group = source_boss_compact_best_palette_group(
        source_sprite2_l2_tiles[l2_slot],
        preferred_palette_group
    );
    slot->palette_group = palette_group;
    source_boss_compact_pack_in_place(
        source_sprite2_l2_tiles[l2_slot],
        palette_group
    );
    source_boss_compact_uploads[source_boss_compact_upload_count] =
        (SourceBossCompactUpload){
            .slot_generation = slot->generation,
            .l2_generation = source_sprite2_l2[l2_slot].generation,
            .slot = selected,
            .l2_slot = l2_slot,
        };
    source_boss_compact_upload_count++;
    /* The first 512 B are now packed staging data, not a valid 8bpp L2. */
    source_sprite2_l2[l2_slot].valid = 0;
    if (
        source_boss_compact_upload_count +
            source_enemy_upload_count +
            (source_player_upload_pending ? 1u : 0u) >
        telemetry_sprite2_max_uploads
    ) {
        telemetry_sprite2_max_uploads =
            source_boss_compact_upload_count +
            source_enemy_upload_count +
            (source_player_upload_pending ? 1u : 0u);
    }
    *palette_bank = source_boss_compact_palette_bank(palette_group);
    return source_boss_compact_tile(selected);
}

static void source_boss_compact_cache_commit(void)
{
    u8 index;

    for (index = 0; index < source_boss_compact_upload_count; index++) {
        const SourceBossCompactUpload *upload =
            &source_boss_compact_uploads[index];
        SourceBossCompactSlot *slot =
            &source_boss_compact_cache[upload->slot];
        u8 valid =
            slot->valid &&
            slot->generation == upload->slot_generation &&
            source_sprite2_l2[upload->l2_slot].generation ==
                upload->l2_generation &&
            source_sprite2_l2[upload->l2_slot].pin_count != 0;

        if (valid) {
            dmaCopy(
                source_sprite2_l2_tiles[upload->l2_slot],
                (u8 *)OBJ_BASE_ADR +
                    source_boss_compact_tile(upload->slot) * 32u,
                SOURCE_BOSS_COMPACT_FRAME_BYTES
            );
            slot->resident = 1;
            telemetry_sprite2_uploads++;
            telemetry_sprite2_upload_bytes +=
                SOURCE_BOSS_COMPACT_FRAME_BYTES;
        } else {
            slot->valid = 0;
            slot->resident = 0;
            telemetry_sprite2_generation_mismatches++;
        }
        source_sprite2_l2_unpin(
            upload->l2_slot,
            upload->l2_generation
        );
    }
    source_boss_compact_upload_count = 0;
}

#else

/* Stress builds use one OBJ per authored enemy and intentionally bypass the
 * release-only 4bpp cache, but the common scheduler still queries its state.
 */
static const u8 source_boss_compact_active = 0;

static u8 source_boss_compact_owns_enemy_slot(u8 slot)
{
    (void)slot;
    return 0;
}

static void source_boss_compact_cache_reset(void)
{
}

static u8 source_boss_compact_prepare(
    const SourceFrameOamCandidates *frame
)
{
    (void)frame;
    return 0;
}

static u16 source_boss_compact_acquire(
    const OtEnemyDrawCommand *draw,
    u8 *palette_bank
)
{
    (void)draw;
    (void)palette_bank;
    return 0xffff;
}

static void source_boss_compact_cache_commit(void)
{
}

#endif
