/*
 * GBA-only presentation adapter for very large, multi-link Sprite2 bosses.
 *
 * A stock size-one Sprite2 frame is a sparse 24x28 picture carried in a
 * 32x32 8bpp OBJ canvas (1 KiB).  Some late-game bosses expose roughly forty
 * different canvases at once, while the ordinary L1 layout can hold only 24.
 * OAM still has room, so rotating OAM entries cannot solve that mismatch.
 *
 * During every confirmed Boss scene, physical full-size enemy slots 2..22
 * are split
 * into two 512-byte 4bpp canvases.  Two palettes are trained from the
 * actual visible frames and active level palette, then refreshed only when
 * the Boss assembly grows or a later frame introduces a materially new
 * colour range.  This yields 42 stable
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
    SOURCE_BOSS_COMPACT_PALETTE_RETRAIN_LIMIT = 8u,
    SOURCE_BOSS_COMPACT_PALETTE_RETRAIN_COOLDOWN = 8u,
    /*
     * Bosses often assemble over several event ticks.  Re-training for
     * every newly visible pair both wastes a frame and locks an
     * intermediate palette into VRAM.  Wait until the visible component
     * high-water mark has been stable for twelve presentation frames, then
     * replace the palette and all dependent compact tiles atomically.
     */
    SOURCE_BOSS_COMPACT_PALETTE_STABLE_FRAMES = 12u,
    SOURCE_BOSS_COMPACT_PRETRAINED_REFRESH_COOLDOWN = 120u,
    /* Stock corpus invariant emitted/audited by build_boss_manifest.py. */
    SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT = 3u,
    /* Squared RGB555 error allowed per non-transparent source pixel. */
    SOURCE_BOSS_COMPACT_PALETTE_ERROR_PER_PIXEL = 32u,
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
static u8 source_boss_compact_prepared_colour[
    SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT
][SOURCE_BOSS_COMPACT_PALETTE_COUNT][15] EWRAM_BSS;
static u8 source_boss_compact_prepared_primary_table[
    SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT
] EWRAM_BSS;
static u8 source_boss_compact_prepared_valid[
    SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT
] EWRAM_BSS;
static u32 source_boss_compact_epoch EWRAM_BSS;
static u8 source_boss_compact_upload_count EWRAM_BSS;
static u8 source_boss_compact_active EWRAM_BSS;
static u8 source_boss_compact_palette_valid EWRAM_BSS;
static u8 source_boss_compact_primary_table EWRAM_BSS;
static u8 source_boss_compact_palette_installed EWRAM_BSS;
static u8 source_boss_compact_trained_boss_count EWRAM_BSS;
static u8 source_boss_compact_observed_boss_count EWRAM_BSS;
static u8 source_boss_compact_palette_refresh_pending EWRAM_BSS;
static u8 source_boss_compact_palette_retrain_count EWRAM_BSS;
static u8 source_boss_compact_active_cohort EWRAM_BSS;
static u8 source_boss_compact_palette_pretrained EWRAM_BSS;
static u32 source_boss_compact_palette_train_epoch EWRAM_BSS;
static u32 source_boss_compact_palette_refresh_epoch EWRAM_BSS;
static u32 source_boss_compact_palette_error_total_max EWRAM_BSS;
static u16 source_boss_compact_palette_error_pixels_at_max EWRAM_BSS;
#ifdef AUTOTEST
static u16 source_boss_compact_training_position EWRAM_BSS;
static u8 source_boss_compact_training_enemy_count EWRAM_BSS;
static u8 source_boss_compact_training_boss_count EWRAM_BSS;
static u8 source_boss_compact_training_full_size_count EWRAM_BSS;
#endif

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

static void source_boss_compact_invalidate_all_slots(void)
{
    u8 slot;

    for (slot = 0; slot < SOURCE_BOSS_COMPACT_SLOT_COUNT; slot++) {
        source_boss_compact_invalidate_slot(slot);
    }
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
source_boss_compact_choose_palette(u8 group)
{
    u8 selected_count = 0;

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
    }
}

static __attribute__((noinline, noclone)) void
source_boss_compact_install_palette(u8 group)
{
    u8 palette_index;

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
 * Resolve exactly the visual definition that the translated event handler
 * will hand to JE_makeEnemy().  Event 49..52 are the sole exception to the
 * ordinary HDT definition path: they clone definition zero, replace graphic
 * zero and supply an explicit Sprite2 bank in eventdat3.
 */
static u8 source_boss_compact_manifest_visual(
    const OtEventRecord *event,
    u8 enemy_type_offset,
    OtEnemyDefinition *definition,
    u8 *shape_table
)
{
    s32 definition_id;

    if (event == 0 || definition == 0 || shape_table == 0) return 0;
    if (event->eventtype >= 49u && event->eventtype <= 52u) {
        if (
            enemy_type_offset != 0 ||
            event->eventdat <= 0 ||
            event->eventdat3 <= 0 ||
            !ot_level_enemy_read(0, definition)
        ) {
            return 0;
        }
        definition->egraphic[0] = (u16)event->eventdat;
        *shape_table = (u8)event->eventdat3;
    } else {
        definition_id =
            (s32)event->eventdat + enemy_type_offset;
        if (
            definition_id < 0 ||
            definition_id > UINT16_MAX ||
            !ot_level_enemy_read((u16)definition_id, definition)
        ) {
            return 0;
        }
        *shape_table = definition->shapebank;
    }
    return
        definition->esize == 1u &&
        *shape_table != 0 &&
        *shape_table <= SPRITE2_RAW_TABLE_COUNT;
}

static u8 source_boss_compact_prepare_cohort_palette(u8 cohort)
{
    OtEventRecord event;
    OtEnemyDefinition definition;
    u16 event_index;
    u8 best_table = 0;
    u8 best_count = 0;

    memset(
        source_boss_compact_table_count,
        0,
        sizeof(source_boss_compact_table_count)
    );
    for (
        event_index = 0;
        event_index < source_parity_level.event_count;
        event_index++
    ) {
        u8 event_cohort;
        u8 spawn_mask = ot_level_port_boss_manifest_event_info(
            source_parity_level.selected_episode,
            source_parity_level.selected_lvl_file_number,
            event_index,
            &event_cohort
        );
        u8 offset;

        if (
            spawn_mask == 0 ||
            event_cohort != cohort ||
            !ot_level_event_read(event_index, &event)
        ) {
            continue;
        }
        for (offset = 0; offset < 4u; offset++) {
            u8 shape_table;

            if ((spawn_mask & (1u << offset)) == 0) continue;
            if (!source_boss_compact_manifest_visual(
                    &event,
                    offset,
                    &definition,
                    &shape_table
                )) {
                continue;
            }
            if (source_boss_compact_table_count[shape_table] != 0xffu) {
                source_boss_compact_table_count[shape_table]++;
            }
            if (
                source_boss_compact_table_count[shape_table] > best_count
            ) {
                best_count = source_boss_compact_table_count[shape_table];
                best_table = shape_table;
            }
        }
    }
    if (best_table == 0) return 0;

    memset(
        source_boss_compact_histogram,
        0,
        sizeof(source_boss_compact_histogram)
    );
    for (
        event_index = 0;
        event_index < source_parity_level.event_count;
        event_index++
    ) {
        u8 event_cohort;
        u8 spawn_mask = ot_level_port_boss_manifest_event_info(
            source_parity_level.selected_episode,
            source_parity_level.selected_lvl_file_number,
            event_index,
            &event_cohort
        );
        u8 offset;

        if (
            spawn_mask == 0 ||
            event_cohort != cohort ||
            !ot_level_event_read(event_index, &event)
        ) {
            continue;
        }
        for (offset = 0; offset < 4u; offset++) {
            u8 shape_table;
            u8 group;
            u8 frame_index;

            if ((spawn_mask & (1u << offset)) == 0) continue;
            if (!source_boss_compact_manifest_visual(
                    &event,
                    offset,
                    &definition,
                    &shape_table
                )) {
                continue;
            }
            group = shape_table == best_table ? 0u : 1u;
            for (frame_index = 0; frame_index < 20u; frame_index++) {
                u16 graphic = definition.egraphic[frame_index];
                u16 pixel;

                if (graphic == 0 || graphic == 999u) continue;
                if (!ot_sprite2_frame_decode(
                        shape_table,
                        graphic,
                        1,
                        0,
                        source_enemy_decode_pixels,
                        OT_SPRITE2_FRAME_PIXELS
                    )) {
                    continue;
                }
                for (
                    pixel = 0;
                    pixel < OT_SPRITE2_FRAME_PIXELS;
                    pixel++
                ) {
                    u16 source_pixel = source_enemy_decode_pixels[pixel];
                    u8 colour;

                    if (
                        source_pixel == 0 ||
                        source_pixel > OT_PALETTE_COLOUR_COUNT
                    ) {
                        continue;
                    }
                    colour = source_enemy_palette_map[
                        (u8)(source_pixel - 1u)
                    ];
                    if (
                        colour != 0 &&
                        source_boss_compact_histogram[group][colour] !=
                            0xffffu
                    ) {
                        source_boss_compact_histogram[group][colour]++;
                    }
                }
            }
        }
    }
    source_boss_compact_choose_palette(0);
    source_boss_compact_choose_palette(1);
    source_boss_compact_prepared_primary_table[cohort - 1u] = best_table;
    memcpy(
        source_boss_compact_prepared_colour[cohort - 1u],
        source_boss_compact_palette_colour,
        sizeof(source_boss_compact_palette_colour)
    );
    return 1;
}

/*
 * Run while the display is blank and before audio_load_level_song_fade_in().
 * This converts no assets and adds no per-level runtime guess: the generated
 * manifest identifies the exact stock spawn cohort, while LVL/HDT and the
 * lossless Sprite2 catalog remain the authoritative visual data.
 */
static void source_boss_compact_prepare_level_palettes(void)
{
    u8 cohort;

    memset(
        source_boss_compact_prepared_valid,
        0,
        sizeof(source_boss_compact_prepared_valid)
    );
    memset(
        source_boss_compact_prepared_primary_table,
        0,
        sizeof(source_boss_compact_prepared_primary_table)
    );
    memset(
        source_boss_compact_prepared_colour,
        0,
        sizeof(source_boss_compact_prepared_colour)
    );
    for (
        cohort = 1;
        cohort <= SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT;
        cohort++
    ) {
        source_boss_compact_prepared_valid[cohort - 1u] =
            source_boss_compact_prepare_cohort_palette(cohort);
    }
}

/*
 * The two compact palettes are trained from the dominant Boss table and
 * the remaining visible tables, but later attack phases can introduce a
 * frame whose colours fit the other palette much better.  Shape-table
 * ownership is therefore only a tie-breaker: choose the lower-error palette
 * for every newly decoded frame.  A materially poor fit also requests one
 * bounded palette refresh on the next presentation frame.  That preserves
 * early compact activation while allowing later Boss phases to extend the
 * colour range without continuously rebuilding palettes.
 */
static u8 source_boss_compact_best_palette_group(
    const u8 *pixels,
    u8 preferred_group,
    u8 refresh_allowed
)
{
    u32 error[SOURCE_BOSS_COMPACT_PALETTE_COUNT] = {0};
    u32 minimum_error;
    u16 nontransparent = 0;
    u16 pixel;
    u8 selected = preferred_group;

    for (pixel = 0; pixel < SOURCE_ENEMY_FRAME_BYTES; pixel++) {
        u8 colour = pixels[pixel];

        if (colour == 0) continue;
        nontransparent++;
        error[0] += source_boss_compact_palette_error[0][colour];
        error[1] += source_boss_compact_palette_error[1][colour];
    }
    if (error[preferred_group ^ 1u] < error[preferred_group]) {
        selected = (u8)(preferred_group ^ 1u);
    }
    minimum_error = error[selected];
    if (minimum_error > source_boss_compact_palette_error_total_max) {
        source_boss_compact_palette_error_total_max = minimum_error;
        source_boss_compact_palette_error_pixels_at_max = nontransparent;
    }
    if (
        refresh_allowed &&
        nontransparent != 0 &&
        source_boss_compact_palette_retrain_count <
            SOURCE_BOSS_COMPACT_PALETTE_RETRAIN_LIMIT &&
        minimum_error >
            (u32)nontransparent *
                SOURCE_BOSS_COMPACT_PALETTE_ERROR_PER_PIXEL
    ) {
        if (!source_boss_compact_palette_refresh_pending) {
            source_boss_compact_palette_refresh_epoch =
                source_boss_compact_epoch;
        }
        source_boss_compact_palette_refresh_pending = 1;
    }
    return selected;
}

static __attribute__((noinline, noclone)) u8
source_boss_compact_train_palette(
    const SourceFrameOamCandidates *frame
)
{
    u16 position;
    u8 best_table = 0;
    u8 best_count = 0;

#ifdef AUTOTEST
    source_boss_compact_training_position = level_position;
    source_boss_compact_training_enemy_count = (u8)frame->enemy_count;
    source_boss_compact_training_boss_count =
        frame->boss_compact_count;
    source_boss_compact_training_full_size_count =
        frame->enemy_full_size_count;
#endif

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
    source_boss_compact_choose_palette(0);
    source_boss_compact_choose_palette(1);
    source_boss_compact_install_palette(0);
    source_boss_compact_install_palette(1);
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

    source_boss_compact_invalidate_all_slots();
    source_boss_compact_active = 0;
    source_boss_compact_palette_valid = 0;
    source_boss_compact_primary_table = 0;
    source_boss_compact_trained_boss_count = 0;
    source_boss_compact_observed_boss_count = 0;
    source_boss_compact_palette_refresh_pending = 0;
    source_boss_compact_palette_retrain_count = 0;
    source_boss_compact_active_cohort = 0;
    source_boss_compact_palette_pretrained = 0;
    source_boss_compact_palette_train_epoch = 0;
    source_boss_compact_palette_refresh_epoch = 0;
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
    memset(
        source_boss_compact_prepared_valid,
        0,
        sizeof(source_boss_compact_prepared_valid)
    );
    source_boss_compact_upload_count = 0;
}

static u8 source_boss_compact_frame_cohort(
    const SourceFrameOamCandidates *frame
)
{
    u16 position;

    for (position = 0; position < frame->enemy_count; position++) {
        const SourceEnemyOamCandidate *candidate = &frame->enemy[position];
        u8 cohort;

        if (
            (candidate->flags & SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) == 0
        ) {
            continue;
        }
        cohort = ot_level_port_boss_manifest_member_cohort(
            &source_parity_level,
            candidate->index
        );
        if (
            cohort != 0 &&
            cohort <= SOURCE_BOSS_COMPACT_PREPARED_COHORT_COUNT
        ) {
            return cohort;
        }
    }
    return 0;
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
    /*
     * Keep the former saturation threshold as telemetry only.  It must not
     * decide whether the production Boss adapter is enabled: waiting for all
     * 23 release slots to fill caused the first 16--19 Camanis Boss frames to
     * churn through the ordinary 8bpp cache before compact mode started.
     */
    u8 capacity_pressure = frame->enemy_full_size_count >=
        SOURCE_ENEMY_CACHE_FULL_SLOT_COUNT;

    if (boss_context) telemetry_boss_compact_context_frames++;
    if (capacity_pressure) telemetry_boss_compact_pressure_frames++;
    if (
        frame->boss_compact_count >
        telemetry_boss_compact_boss_count_max
    ) {
        telemetry_boss_compact_boss_count_max =
            frame->boss_compact_count;
    }
    if (
        frame->enemy_full_size_count >
        telemetry_boss_compact_full_size_count_max
    ) {
        telemetry_boss_compact_full_size_count_max =
            frame->enemy_full_size_count;
    }

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

        /*
         * This is the mandatory production path, not a detail/build option.
         * Activate as soon as the manifest/bar identity confirms a Boss and
         * at least one full-size Boss component can use the compact cache.
         */
        if (!boss_context || frame->boss_compact_count == 0) return 0;
        source_boss_compact_active = 1;
        telemetry_boss_compact_activations++;
        source_boss_compact_epoch = 0;
        source_boss_compact_upload_count = 0;
        source_boss_compact_palette_valid = 0;
        source_boss_compact_primary_table = 0;
        source_boss_compact_trained_boss_count = 0;
        source_boss_compact_observed_boss_count = 0;
        source_boss_compact_palette_refresh_pending = 0;
        source_boss_compact_palette_retrain_count = 0;
        source_boss_compact_active_cohort =
            source_boss_compact_frame_cohort(frame);
        source_boss_compact_palette_pretrained = 0;
        source_boss_compact_palette_train_epoch = 0;
        source_boss_compact_palette_refresh_epoch = 0;
        source_boss_compact_palette_error_total_max = 0;
        source_boss_compact_palette_error_pixels_at_max = 0;
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
        if (
            source_boss_compact_active_cohort != 0 &&
            source_boss_compact_prepared_valid[
                source_boss_compact_active_cohort - 1u
            ]
        ) {
            source_boss_compact_primary_table =
                source_boss_compact_prepared_primary_table[
                    source_boss_compact_active_cohort - 1u
                ];
            memcpy(
                source_boss_compact_palette_colour,
                source_boss_compact_prepared_colour[
                    source_boss_compact_active_cohort - 1u
                ],
                sizeof(source_boss_compact_palette_colour)
            );
            source_boss_compact_install_palette(0);
            source_boss_compact_install_palette(1);
            source_boss_compact_palette_valid = 1;
            source_boss_compact_palette_installed = 1;
            source_boss_compact_palette_pretrained = 1;
            source_boss_compact_trained_boss_count =
                frame->boss_compact_count;
            source_boss_compact_observed_boss_count =
                frame->boss_compact_count;
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
    if (!source_boss_compact_palette_valid) {
        if (!source_boss_compact_train_palette(frame)) {
            source_boss_compact_deactivate();
            return 0;
        }
        source_boss_compact_trained_boss_count =
            frame->boss_compact_count;
        source_boss_compact_observed_boss_count =
            frame->boss_compact_count;
        source_boss_compact_palette_train_epoch =
            source_boss_compact_epoch;
        source_boss_compact_palette_refresh_epoch =
            source_boss_compact_epoch;
        source_boss_compact_palette_refresh_pending = 0;
    } else {
        if (
            !source_boss_compact_palette_pretrained &&
            frame->boss_compact_count >
                source_boss_compact_observed_boss_count
        ) {
            /*
             * Debounce a progressively assembled Boss.  Resetting the
             * stability epoch on every new high-water mark coalesces the
             * whole entrance into one palette refresh instead of one
             * expensive refresh per event burst.
             */
            source_boss_compact_observed_boss_count =
                frame->boss_compact_count;
            source_boss_compact_palette_refresh_pending = 1;
            source_boss_compact_palette_refresh_epoch =
                source_boss_compact_epoch;
        }
        u8 refresh_ready = (u8)(
            source_boss_compact_palette_refresh_pending &&
            source_boss_compact_epoch -
                source_boss_compact_palette_refresh_epoch >=
                    SOURCE_BOSS_COMPACT_PALETTE_STABLE_FRAMES &&
            source_boss_compact_epoch -
                source_boss_compact_palette_train_epoch >=
                    SOURCE_BOSS_COMPACT_PALETTE_RETRAIN_COOLDOWN &&
            (
                !source_boss_compact_palette_pretrained ||
                source_boss_compact_epoch -
                    source_boss_compact_palette_train_epoch >=
                        SOURCE_BOSS_COMPACT_PRETRAINED_REFRESH_COOLDOWN
            )
        );

        if (
            refresh_ready &&
            source_boss_compact_palette_retrain_count <
                SOURCE_BOSS_COMPACT_PALETTE_RETRAIN_LIMIT
        ) {
            u8 old_primary_table =
                source_boss_compact_primary_table;

            if (source_boss_compact_train_palette(frame)) {
                /*
                 * Palette nibbles are part of every resident 4bpp tile's
                 * meaning.  Retire old tiles and pending uploads as one
                 * transaction before this frame acquires the new mapping.
                 */
                source_boss_compact_invalidate_all_slots();
                source_boss_compact_palette_retrain_count++;
                source_boss_compact_trained_boss_count =
                    frame->boss_compact_count;
                source_boss_compact_observed_boss_count =
                    frame->boss_compact_count;
                source_boss_compact_palette_train_epoch =
                    source_boss_compact_epoch;
                source_boss_compact_palette_refresh_epoch =
                    source_boss_compact_epoch;
                source_boss_compact_palette_refresh_pending = 0;
            } else {
                source_boss_compact_primary_table = old_primary_table;
            }
        }
    }
    telemetry_boss_compact_active_frames++;
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
                telemetry_boss_compact_rendered_objects++;
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
        preferred_palette_group,
        draw->filter == 0
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
    telemetry_boss_compact_rendered_objects++;
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

static void source_boss_compact_prepare_level_palettes(void)
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
