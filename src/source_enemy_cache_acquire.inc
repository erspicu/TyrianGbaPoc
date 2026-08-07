/*
 * Sprite2 L1 admission/replacement hot path.  Kept separate from the broad
 * presentation adapter so this performance-sensitive policy remains
 * reviewable without growing source_runtime.inc beyond the project limit.
 */

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u16
source_enemy_cache_acquire(
    const OtEnemyDrawCommand *draw
)
{
    u8 index;
#if TYRIAN_GBA_SPRITE2_EXACT_LOOKUP_ASM
    const SourceEnemyCacheSlot *exact_slot;
#endif
    u8 selected = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 free_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 eviction_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
    u8 base_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 filtered_eviction_slot = SOURCE_ENEMY_CACHE_SLOT_COUNT;
    u8 effective_filter = draw->filter;
#endif
    u8 l2_slot;
    u32 oldest_epoch = 0xffffffffu;
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
    u32 oldest_filtered_epoch = 0xffffffffu;
#endif

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

#if TYRIAN_GBA_SPRITE2_EXACT_LOOKUP_ASM
    telemetry_sprite2_exact_lookup_probes++;
    exact_slot = source_enemy_cache_find_exact(
        draw->shape_table,
        draw->graphic,
        draw->size,
        draw->filter
    );
    if (exact_slot != NULL) {
        index = (u8)(exact_slot - source_enemy_cache);
    }
    if (
        exact_slot != NULL &&
        !source_enemy_cache_slot_reserved(index) &&
        (
            !source_enemy_cache_slot_is_compact(index) ||
            draw->size == 0
        )
    ) {
        SourceEnemyCacheSlot *slot = &source_enemy_cache[index];

        if (
            slot->resident ||
            source_enemy_cache_upload_pending(index, slot->generation)
        ) {
            slot->last_epoch = source_enemy_cache_epoch;
            telemetry_sprite2_cache_hits++;
            telemetry_sprite2_exact_lookup_hits++;
            return source_enemy_cache_tile(index);
        }
        source_enemy_cache_invalidate_slot(index);
    }
#endif

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
            slot->size == draw->size
        ) {
            if (slot->filter == draw->filter) {
                if (
                    slot->resident ||
                    source_enemy_cache_upload_pending(
                        index,
                        slot->generation
                    )
                ) {
                    slot->last_epoch = source_enemy_cache_epoch;
                    telemetry_sprite2_cache_hits++;
                    return source_enemy_cache_tile(index);
                }
                source_enemy_cache_invalidate_slot(index);
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
            } else if (draw->filter != 0 && slot->filter == 0) {
                if (
                    slot->resident ||
                    source_enemy_cache_upload_pending(
                        index,
                        slot->generation
                    )
                ) {
                    /*
                     * Keep the base candidate cold until admission is known.
                     * Pending tile DMA commits before OAM DMA, so it is safe
                     * to reference this slot in the same presentation.
                     */
                    if (base_slot == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
                        base_slot = index;
                    }
                } else {
                    source_enemy_cache_invalidate_slot(index);
                }
#endif
            }
        }
        if (!slot->valid) {
            if (free_slot == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
                free_slot = index;
            }
        } else if (slot->last_epoch != source_enemy_cache_epoch) {
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
            if (slot->filter != 0) {
                if (slot->last_epoch < oldest_filtered_epoch) {
                    filtered_eviction_slot = index;
                    oldest_filtered_epoch = slot->last_epoch;
                }
            } else if (slot->last_epoch < oldest_epoch) {
                eviction_slot = index;
                oldest_epoch = slot->last_epoch;
            }
#else
            if (slot->last_epoch < oldest_epoch) {
                eviction_slot = index;
                oldest_epoch = slot->last_epoch;
            }
#endif
        }
    }
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
    if (draw->filter != 0 && free_slot == SOURCE_ENEMY_CACHE_SLOT_COUNT) {
        telemetry_sprite2_filter_admission_denials++;
        if (base_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT) {
            source_enemy_cache[base_slot].last_epoch =
                source_enemy_cache_epoch;
            telemetry_sprite2_cache_hits++;
            telemetry_sprite2_filter_fallback_hits++;
            return source_enemy_cache_tile(base_slot);
        }
        effective_filter = 0;
    }
    selected = free_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT ?
        free_slot :
        (
            filtered_eviction_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT ?
                filtered_eviction_slot :
                eviction_slot
        );
#else
    selected = free_slot != SOURCE_ENEMY_CACHE_SLOT_COUNT ?
        free_slot :
        eviction_slot;
#endif
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
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
        if (source_enemy_cache[selected].filter != 0) {
            telemetry_sprite2_filter_evictions++;
        }
#endif
    }
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
    if (draw->filter != 0 && effective_filter == 0) {
        telemetry_sprite2_filter_fallback_builds++;
    }
#endif
    source_enemy_cache_invalidate_slot(selected);
    source_enemy_cache[selected].generation =
        source_cache_next_generation();
    source_enemy_cache[selected].valid = 1;
    source_enemy_cache[selected].resident = 0;
    source_enemy_cache[selected].shape_table = draw->shape_table;
    source_enemy_cache[selected].graphic = draw->graphic;
    source_enemy_cache[selected].size = draw->size;
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
    source_enemy_cache[selected].filter = effective_filter;
#else
    source_enemy_cache[selected].filter = draw->filter;
#endif
    source_enemy_cache[selected].last_epoch = source_enemy_cache_epoch;
    telemetry_sprite2_cache_misses++;
    l2_slot = source_sprite2_l2_acquire(
        draw->shape_table,
        draw->graphic,
        draw->size,
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
        effective_filter
#else
        draw->filter
#endif
    );
    if (l2_slot == 0xff) {
        if (!source_enemy_first_decode_failure_valid) {
            source_enemy_first_decode_failure_valid = 1;
            source_enemy_first_decode_failure_shape_table =
                draw->shape_table;
            source_enemy_first_decode_failure_graphic = draw->graphic;
            source_enemy_first_decode_failure_size = draw->size;
#if TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE
            source_enemy_first_decode_failure_filter = effective_filter;
#else
            source_enemy_first_decode_failure_filter = draw->filter;
#endif
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
        source_enemy_cache_invalidate_slot(selected);
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
            source_sprite2_l2_unpin(
                source_enemy_uploads[index].l2_slot,
                source_enemy_uploads[index].l2_generation
            );
            source_enemy_uploads[index].l2_slot = l2_slot;
            source_enemy_uploads[index].slot_generation =
                source_enemy_cache[selected].generation;
            source_enemy_uploads[index].l2_generation =
                source_sprite2_l2[l2_slot].generation;
            if (
                !source_sprite2_l2_pin(
                    l2_slot,
                    source_enemy_uploads[index].l2_generation
                )
            ) {
                source_enemy_cache_invalidate_slot(selected);
                return 0xffff;
            }
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
        source_enemy_cache_invalidate_slot(selected);
        return 0xffff;
    }
    source_enemy_uploads[source_enemy_upload_count].slot = selected;
    source_enemy_uploads[source_enemy_upload_count].l2_slot = l2_slot;
    source_enemy_uploads[source_enemy_upload_count].slot_generation =
        source_enemy_cache[selected].generation;
    source_enemy_uploads[source_enemy_upload_count].l2_generation =
        source_sprite2_l2[l2_slot].generation;
    if (
        !source_sprite2_l2_pin(
            l2_slot,
            source_enemy_uploads[source_enemy_upload_count].l2_generation
        )
    ) {
        source_enemy_cache_invalidate_slot(selected);
        return 0xffff;
    }
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
