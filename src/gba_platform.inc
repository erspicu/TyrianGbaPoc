/*
 * Transitional single-translation-unit GBA platform layer.
 * It stays isolated from source-parity gameplay and can become a standalone
 * object after the shared runtime state is encapsulated.
 */

static void audio_mix_physical_frame(void)
{
    u16 interrupt_master_enable = REG_IME;

    /*
     * Maxmod's GBA mixer advances an EWRAM output cursor while mmFrame()
     * runs.  mmVBlank() resets that cursor every second physical VBlank.
     * A late recovery pass can otherwise be interrupted between the cursor
     * load and store, resume with the stale address, and write PCM over the
     * adjacent module-channel array.  Keep only this mixer transaction
     * atomic; a pending VBlank remains latched in REG_IF and is dispatched
     * immediately when IME is restored.
     */
    REG_IME = 0;
    __asm__ volatile("" ::: "memory");
    mmFrame();
    __asm__ volatile("" ::: "memory");
    REG_IME = interrupt_master_enable;
}

static void vblank_handler(void)
{
#if TYRIAN_GBA_PERF_TIMER
    u32 cycle_start = 0;

    if (perf_timer_ready) {
        cycle_start = stress_cycle_counter_read();
    }
#endif
    telemetry_vblank_irqs++;
    mmVBlank();
#if TYRIAN_GBA_PERF_TIMER
    if (perf_timer_ready) {
        u32 cycle_end = stress_cycle_counter_read();
        u32 elapsed = cycle_end - cycle_start;

        telemetry_perf_vblank_irq_cycles_last = elapsed;
        telemetry_perf_vblank_irq_cycles_total += elapsed;
        if (elapsed > telemetry_perf_vblank_irq_cycles_max) {
            telemetry_perf_vblank_irq_cycles_max = elapsed;
        }
        /*
         * The main loop may consume this IRQ after the raster has already
         * left VBlank.  Keep the IRQ-end timestamp so presentation decisions
         * use the real LCD deadline rather than granting a recovery loop an
         * artificial full frame.
         */
        perf_vblank_cycle_stamp = cycle_end;
    }
#endif
}

static void hide_all_sprites(void)
{
    u16 index;
    for (index = 0; index < HARDWARE_OAM_ENTRIES; index++) {
        oam_shadow[index].attr0 = ATTR0_DISABLED;
        oam_shadow[index].attr1 = 0;
        oam_shadow[index].attr2 = 0;
        oam_shadow[index].dummy = 0;
    }
    previous_oam_count = 0;
    oam_dirty = 1;
}

static void commit_background_row(const u8 *source, u16 *left_target)
{
    dmaCopy(source, left_target, MAP_HALF_ROW_BYTES);
    dmaCopy(
        source + MAP_HALF_ROW_BYTES,
        left_target + MAP_SCREEN_BLOCK_WORDS,
        MAP_HALF_ROW_BYTES
    );
}

static void commit_background_rows(u8 layer)
{
    BackgroundPendingRows *pending =
        &background_pending_rows[layer];
    u8 index;

    if (pending->count == 0) return;
    background_commit_row_uploads(layer);
    for (index = 0; index < pending->count; index++) {
        commit_background_row(
            pending->source[index],
            pending->target[index]
        );
        background_layer_cache[layer].ring_vram_source_row[
            pending->row[index] & (MAP_RING_ROWS - 1u)
        ] = pending->row[index];
    }
    pending->count = 0;
}

static u8 gameplay_filter_brightness_level(void)
{
#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW
    int16_t brightness;

    if (
        !source_parity_level.filter_active ||
        source_parity_level.level_brightness == -99
    ) {
        return 0;
    }
    brightness = source_parity_level.level_brightness;
    if (brightness < 0) brightness = (int16_t)-brightness;
    if (brightness > 16) brightness = 16;
    return (u8)brightness;
#else
    /* PC processor type 1 disables explosionTransparent/brightness filters. */
    return 0;
#endif
}

static u16 gameplay_filter_bldcnt(void)
{
    if (gameplay_filter_brightness_level() == 0) return 0;
    /* BG0..3, OBJ and backdrop; negative=darken, positive=brighten. */
    return (u16)(
        0x003fu |
        (
            source_parity_level.level_brightness < 0 ?
                (3u << 6) :
                (2u << 6)
        )
    );
}

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
static void gameplay_presentation_capture_registers(void)
{
    u8 background2_priority = source_background2_priority();
    u8 background3_priority = source_background3_priority();
    u8 background2_present = source_background2_present();
    u16 blend_first = 0;
    u16 blend_second = 0;

    gameplay_presentation.hofs[0] = source_background_hofs(0);
    gameplay_presentation.hofs[1] = source_background_hofs(1);
    gameplay_presentation.hofs[2] = source_background_hofs(2);
    gameplay_presentation.vofs[0] =
        source_background_vofs(0);
    gameplay_presentation.vofs[1] =
        source_background_vofs(1);
    gameplay_presentation.vofs[2] =
        source_background_vofs(2);
    gameplay_presentation.full_scroll_pixel[0] =
        background_presentation_scroll_pixel(
            0,
            source_camera_offset_y
        );
    gameplay_presentation.full_scroll_pixel[1] =
        background_presentation_scroll_pixel(
            1,
            source_camera_offset_y
        );
    gameplay_presentation.full_scroll_pixel[2] =
        background_presentation_scroll_pixel(
            2,
            source_camera_offset_y
        );
    gameplay_presentation.bg1cnt =
        BG_PRIORITY(background2_priority) |
        CHAR_BASE(background_cache_char_base[1]) |
        SCREEN_BASE(BG1_SCREEN_BLOCK) |
        BG_16_COLOR | BG_SIZE_1;
    gameplay_presentation.bg2cnt =
        BG_PRIORITY(background3_priority) |
        CHAR_BASE(background_cache_char_base[2]) |
        SCREEN_BASE(BG2_SCREEN_BLOCK) |
        BG_16_COLOR | BG_SIZE_1;
    gameplay_presentation.dispcnt =
        MODE_0 | BG2_ON | OBJ_ON | OBJ_1D_MAP |
#if !TYRIAN_GBA_STRESS_LOADOUT
        (source_special_astral_duration == 0 ? BG0_ON : 0) |
#else
        BG0_ON |
#endif
        (background2_present ? BG1_ON : 0) |
        (
            game_state == STATE_PLAY && gameplay_overlay_active ?
                BG3_ON : 0
        );
#if TYRIAN_GBA_STRESS_LOADOUT
    blend_first |= 1u << 4;
    blend_second |= 1u | 2u | 4u | 32u;
#endif
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
    if (
        background2_present &&
        !source_parity_level.background2_not_transparent
    ) {
        blend_first |= 1u << 1;
        blend_second |= 1u | 4u | 32u;
    }
#endif
    if (gameplay_filter_brightness_level() != 0) {
        gameplay_presentation.bldcnt = gameplay_filter_bldcnt();
        gameplay_presentation.bldalpha = 0;
        gameplay_presentation.bldy = gameplay_filter_brightness_level();
    } else if (blend_first != 0) {
        gameplay_presentation.bldcnt =
            blend_first | (1u << 6) | (blend_second << 8);
        gameplay_presentation.bldalpha = 8u | (8u << 8);
        gameplay_presentation.bldy = 0;
    } else {
        gameplay_presentation.bldcnt = 0;
        gameplay_presentation.bldalpha = 0;
        gameplay_presentation.bldy = 0;
    }
    presentation_registers_valid = 1;
}

static void gameplay_presentation_mark_logic_update(void)
{
    if (presentation_render_pending) {
        telemetry_presentation_superseded++;
    } else {
        presentation_render_pending = 1;
    }
    if (presentation_pending_logic_ticks != 0xff) {
        presentation_pending_logic_ticks++;
    }
    if (
        presentation_pending_logic_ticks >
        telemetry_presentation_pending_logic_max
    ) {
        telemetry_presentation_pending_logic_max =
            presentation_pending_logic_ticks;
    }
}

static u8 gameplay_presentation_should_render(
    u32 frame_cycle_start,
    u8 state_transition_forces_render
)
{
#if TYRIAN_GBA_PRESENTATION_DEFER
    u32 now;
    u32 elapsed;
    u32 deadline;
    u8 layer;
    u8 current_held_rows = 0;
    u8 forced = state_transition_forces_render;
#endif

    if (!presentation_render_pending) return 0;
    telemetry_presentation_render_attempts++;
#if !TYRIAN_GBA_PRESENTATION_DEFER
    (void)frame_cycle_start;
    (void)state_transition_forces_render;
    telemetry_presentation_render_forced++;
    return 1;
#else
    for (layer = 0; layer < BACKGROUND_LAYER_COUNT; layer++) {
        if (
            background_layer_cache[layer].ring_reference_rows >
            current_held_rows
        ) {
            current_held_rows =
                background_layer_cache[layer].ring_reference_rows;
        }
    }
    if (
        presentation_pending_logic_ticks >=
            PRESENTATION_MAX_PENDING_LOGIC_TICKS ||
        current_held_rows >
            BACKGROUND_RETAINED_TILE_ROWS
    ) {
        forced = 1;
    }
    if (forced) {
        telemetry_presentation_render_forced++;
        return 1;
    }

    now = stress_cycle_counter_read();
    elapsed = now - frame_cycle_start;
    if (
        elapsed >
        telemetry_presentation_deadline_elapsed_max
    ) {
        telemetry_presentation_deadline_elapsed_max = elapsed;
    }
    deadline = GBA_DISPLAY_FRAME_CYCLES;
    if (
        telemetry_perf_vblank_irq_cycles_last +
            PRESENTATION_DEADLINE_GUARD_CYCLES <
        deadline
    ) {
        deadline -=
            telemetry_perf_vblank_irq_cycles_last +
            PRESENTATION_DEADLINE_GUARD_CYCLES;
    } else {
        deadline = 0;
    }
    if (
        elapsed >= deadline ||
        presentation_render_estimate > deadline - elapsed
    ) {
        telemetry_presentation_render_deferred++;
        return 0;
    }
    return 1;
#endif
}

static void gameplay_presentation_complete(u32 actual_render_cycles)
{
    u32 delta;

    if (actual_render_cycles >= presentation_render_ewma) {
        delta = actual_render_cycles - presentation_render_ewma;
        presentation_render_ewma += delta >> 3;
    } else {
        delta = presentation_render_ewma - actual_render_cycles;
        presentation_render_ewma -= delta >> 3;
    }
    if (delta >= presentation_render_deviation) {
        presentation_render_deviation +=
            (delta - presentation_render_deviation) >> 3;
    } else {
        presentation_render_deviation -=
            (presentation_render_deviation - delta) >> 3;
    }
    presentation_render_estimate =
        presentation_render_ewma +
        (presentation_render_deviation << 1) +
        PRESENTATION_DEADLINE_GUARD_CYCLES;
    /*
     * A single cold Sprite2/cache frame can exceed one entire LCD period.
     * Keeping that raw peak as the next prediction deadlocks the scheduler:
     * no idle frame is ever tried, so the estimate can only decay on forced
     * renders.  Cap the envelope below the measured idle-frame budget; a
     * rare cold miss may still overrun once, while warm scenes resume on the
     * immediately following logic-idle frame.
     */
    if (presentation_render_estimate > 200000u) {
        presentation_render_estimate = 200000u;
    } else if (presentation_render_estimate < 60000u) {
        presentation_render_estimate = 60000u;
    }
    if (
        presentation_render_estimate >
        telemetry_presentation_estimate_max
    ) {
        telemetry_presentation_estimate_max =
            presentation_render_estimate;
    }
    presentation_render_pending = 0;
    presentation_pending_logic_ticks = 0;
    presentation_release_held_window = 0;
    telemetry_presentation_render_completed++;
}
#endif

#if TYRIAN_GBA_PERF_TIMER
static IWRAM_CODE ARM_CODE u8 background_prefetch_frame_budget(
    u32 frame_cycle_start,
    u8 requested
)
{
    u32 elapsed =
        stress_cycle_counter_read() - frame_cycle_start;
    u32 deadline = GBA_DISPLAY_FRAME_CYCLES;
    u32 remaining;
    u32 budget;

    if (
        telemetry_perf_vblank_irq_cycles_last +
            PRESENTATION_DEADLINE_GUARD_CYCLES <
        deadline
    ) {
        deadline -=
            telemetry_perf_vblank_irq_cycles_last +
            PRESENTATION_DEADLINE_GUARD_CYCLES;
    } else {
        background_prefetch_cycle_limit = 0;
        return 0;
    }
    if (
        elapsed + BACKGROUND_PREFETCH_DEADLINE_RESERVE_CYCLES >=
        deadline
    ) {
        background_prefetch_cycle_limit = 0;
        return 0;
    }
    background_prefetch_cycle_start = frame_cycle_start;
    background_prefetch_cycle_limit =
        deadline - BACKGROUND_PREFETCH_DEADLINE_RESERVE_CYCLES;
    remaining =
        deadline - elapsed -
        BACKGROUND_PREFETCH_DEADLINE_RESERVE_CYCLES;
    budget = remaining >> BACKGROUND_PREFETCH_MISS_CYCLE_SHIFT;
    if (budget > requested) budget = requested;
    return (u8)budget;
}
#define BACKGROUND_PREFETCH_FRAME_BUDGET(requested) \
    background_prefetch_frame_budget( \
        perf_frame_cycle_start, \
        (requested) \
    )
#else
#define BACKGROUND_PREFETCH_FRAME_BUDGET(requested) (requested)
#endif

static void commit_vblank_work(void)
{
    volatile u16 *boss_palette =
        (volatile u16 *)SPRITE_PALETTE + OBJ_PAL_BOSS_BAR * 16;

    if (frontend_mode4_active) {
        frontend_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    if (game_state == STATE_JUKEBOX) {
        jukebox_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    commit_background_rows(0);
    if (source_background2_upload_pending) {
        /*
         * Low detail leaves this 20 KiB BG layer unallocated.  Match the PC
         * background2over==3 escape hatch by restoring its tiles and current
         * ring atomically in VBlank before enabling BG1.
         */
        background_commit_full_layer(
            1,
            background_presentation_scroll_pixel(
                1,
                source_camera_offset_y
            ) >> 3,
            BG1_SCREEN_BLOCK
        );
        background_pending_rows[1].count = 0;
        source_background2_upload_pending = 0;
    } else {
        commit_background_rows(1);
    }
    commit_background_rows(2);
    if (
        game_state == STATE_PLAY &&
        gameplay_overlay_upload_pending
    ) {
        u16 overlay_tile_count =
            (u16)(gameplay_overlay_tile_count + 1u);
        u16 overlay_tile0_count =
            overlay_tile_count < GAMEPLAY_OVERLAY_TILE0_CAPACITY ?
                overlay_tile_count :
                GAMEPLAY_OVERLAY_TILE0_CAPACITY;

        dmaCopy(
            gameplay_overlay_tiles,
            (u8 *)CHAR_BASE_BLOCK(0) +
                GAMEPLAY_OVERLAY_TILE0_VRAM_OFFSET,
            (u32)overlay_tile0_count * 32u
        );
        if (overlay_tile_count > overlay_tile0_count) {
            dmaCopy(
                gameplay_overlay_tiles +
                    GAMEPLAY_OVERLAY_TILE0_CAPACITY * 32u,
                SCREEN_BASE_BLOCK(
                    GAMEPLAY_OVERLAY_TILE1_SCREEN_BLOCK
                ),
                (u32)(overlay_tile_count - overlay_tile0_count) * 32u
            );
        }
        dmaCopy(
            gameplay_overlay_map,
            (u16 *)SCREEN_BASE_BLOCK(GAMEPLAY_OVERLAY_SCREEN_BLOCK) +
                GAMEPLAY_OVERLAY_SCREEN_ROW_OFFSET * 32u,
            sizeof(gameplay_overlay_map)
        );
        gameplay_overlay_upload_pending = 0;
    }
    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
#if !TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
        gameplay_presentation_capture_registers();
#else
        if (!presentation_registers_valid) {
            gameplay_presentation_capture_registers();
        }
#endif
        REG_BG0HOFS = gameplay_presentation.hofs[0];
        REG_BG1HOFS = gameplay_presentation.hofs[1];
        REG_BG2HOFS = gameplay_presentation.hofs[2];
        REG_BG0VOFS = gameplay_presentation.vofs[0];
        REG_BG1VOFS = gameplay_presentation.vofs[1];
        REG_BG2VOFS = gameplay_presentation.vofs[2];
        REG_BG1CNT = gameplay_presentation.bg1cnt;
        REG_BG2CNT = gameplay_presentation.bg2cnt;
        REG_BG3CNT = BG_PRIORITY(0) |
                     CHAR_BASE(GAMEPLAY_OVERLAY_CHAR_BASE) |
                     SCREEN_BASE(GAMEPLAY_OVERLAY_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_0;
        REG_BG3HOFS = 0;
        REG_BG3VOFS = GAMEPLAY_OVERLAY_MAP_VOFS;
        REG_DISPCNT = gameplay_presentation.dispcnt;
        REG_BLDCNT = gameplay_presentation.bldcnt;
        REG_BLDALPHA = gameplay_presentation.bldalpha;
        REG_BLDY = gameplay_presentation.bldy;
#else
        u8 background2_priority = 2;
        u8 background3_priority = 1;
        u8 background2_present;

        background2_priority = source_background2_priority();
        background3_priority = source_background3_priority();
        background2_present = source_background2_present();
        REG_BG0HOFS = source_background_hofs(0);
        REG_BG1HOFS = source_background_hofs(1);
        REG_BG2HOFS = source_background_hofs(2);
        REG_BG0VOFS = source_background_vofs(0);
        REG_BG1VOFS = source_background_vofs(1);
        REG_BG2VOFS = source_background_vofs(2);
        REG_BG1CNT = BG_PRIORITY(background2_priority) |
                     CHAR_BASE(background_cache_char_base[1]) |
                     SCREEN_BASE(BG1_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_BG2CNT = BG_PRIORITY(background3_priority) |
                     CHAR_BASE(background_cache_char_base[2]) |
                     SCREEN_BASE(BG2_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_BG3CNT = BG_PRIORITY(0) |
                     CHAR_BASE(GAMEPLAY_OVERLAY_CHAR_BASE) |
                     SCREEN_BASE(GAMEPLAY_OVERLAY_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_0;
        REG_BG3HOFS = 0;
        REG_BG3VOFS = GAMEPLAY_OVERLAY_MAP_VOFS;
        REG_DISPCNT =
            MODE_0 | BG2_ON | OBJ_ON | OBJ_1D_MAP |
#if !TYRIAN_GBA_STRESS_LOADOUT
            (source_special_astral_duration == 0 ? BG0_ON : 0) |
#else
            BG0_ON |
#endif
            (background2_present ? BG1_ON : 0) |
            (
                game_state == STATE_PLAY && gameplay_overlay_active ?
                    BG3_ON : 0
            );
#endif
        /*
         * Count the source software-filter requests independently from the
         * GBA presentation.  This distinguishes a cheap hardware detail
         * profile from a level that actually asks for the PC framebuffer
         * lava/water/iced/blur passes.
         */
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_HIGH
        if (source_parity_level.smoothies[0]) {
            telemetry_detail_lava_frames++;
        }
        if (source_parity_level.smoothies[1]) {
            telemetry_detail_water_frames++;
        }
#endif
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_NORMAL
        if (
            source_parity_level.smoothies[2] ||
            source_parity_level.smoothies[4]
        ) {
            telemetry_detail_iced_frames++;
        }
        if (source_parity_level.smoothies[3]) {
            telemetry_detail_blur_frames++;
        }
#endif
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
        if (
            source_background2_present() &&
            !source_parity_level.background2_not_transparent
        ) {
            telemetry_detail_wild_frames++;
        }
#endif
#if !TYRIAN_GBA_DYNAMIC_FRAME_DROP
        {
            u16 blend_first = 0;
            u16 blend_second = 0;

#if TYRIAN_GBA_STRESS_LOADOUT
            /*
             * OpenTyrian's large Super Bomb OPTION_SHAPES image is a
             * framebuffer blend.  Only that OBJ uses semi-transparent mode;
             * ordinary objects remain opaque.
             */
            blend_first |= 1u << 4;
            blend_second |= 1u | 2u | 4u | 32u;
#endif
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
            if (
                background2_present &&
                !source_parity_level.background2_not_transparent
            ) {
                /*
                 * OpenTyrian Pentium/wild averages the overlay brightness
                 * with the existing framebuffer.  GBA alpha blending is the
                 * closest hardware-native equivalent: BG1 is source
                 * background 2 and BG0/BG2/backdrop are the destination.
                 */
                blend_first |= 1u << 1;
                blend_second |= 1u | 4u | 32u;
            }
#endif
            if (gameplay_filter_brightness_level() != 0) {
                REG_BLDCNT = gameplay_filter_bldcnt();
                REG_BLDALPHA = 0;
                REG_BLDY = gameplay_filter_brightness_level();
            } else if (blend_first != 0) {
                REG_BLDCNT =
                    blend_first | (1u << 6) |
                    (blend_second << 8);
                REG_BLDALPHA = 8u | (8u << 8);
                REG_BLDY = 0;
            } else {
                REG_BLDCNT = 0;
                REG_BLDALPHA = 0;
                REG_BLDY = 0;
            }
        }
#endif
        if (boss_obj_palette_restore_pending) {
            dmaCopy(
                obj_palette,
                SPRITE_PALETTE,
                512
            );
            boss_obj_palette_restore_pending = 0;
        }
    }
    if (
        game_state == STATE_LEVEL_STATS &&
        frontend_stats_overlay_active
    ) {
        if (frontend_stats_tiles_pending) {
            dmaCopy(
                frontend_frame_scratch,
                (u8 *)OBJ_BASE_ADR +
                    FRONTEND_STATS_TILE_BASE * 32u,
                FRONTEND_STATS_TILE_BYTES
            );
            frontend_stats_tiles_pending = 0;
        }
        if (frontend_stats_palette_dirty) {
            dmaCopy(
                frontend_stats_obj_palette,
                SPRITE_PALETTE + 13u * 16u,
                sizeof(frontend_stats_obj_palette)
            );
            frontend_stats_palette_dirty = 0;
        }
        REG_DISPCNT =
            MODE_0 | BG0_ON | BG2_ON |
            OBJ_ON | OBJ_1D_MAP |
            (source_background2_present() ? BG1_ON : 0);
    }
    if (secret_level_tile_upload_pending) {
        dmaCopy(
            obj_tiles + OBJ_TILE_SECRET_LEVEL_SOURCE * 32,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_SECRET_LEVEL_RUNTIME * 32,
            OBJ_SECRET_LEVEL_TILE_COUNT * 32
        );
        secret_level_tile_upload_pending = 0;
    }
    if (secret_level_palette_dirty) {
        if (game_state == STATE_PLAY) {
            u8 flash = source_parity_level.display_flash;

            if (flash > 5) flash = 5;
            dmaCopy(
                secret_level_palettes + flash * 32,
                SPRITE_PALETTE + OBJ_PAL_SECRET_LEVEL * 16,
                32
            );
        }
        secret_level_palette_dirty = 0;
    }
    if (game_over_tile_upload_pending) {
        /*
         * The generated GAME OVER glyphs live in cartridge backing at the
         * first upper Sprite2 cache slot.  At death completion, copy them
         * into the otherwise unused 512..527 runtime gap.  The explosion
         * cache reads its source frames from cartridge ROM, so replacing
         * those VRAM characters cannot corrupt the final death effects.
         */
        dmaCopy(
            obj_tiles + OBJ_TILE_GAME_OVER_SOURCE * 32,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_GAME_OVER_RUNTIME * 32,
            OBJ_GAME_OVER_TILE_COUNT * 32
        );
        game_over_tile_upload_pending = 0;
    }
    if (insert_coin_tile_upload_pending) {
        dmaCopy(
            obj_tiles + OBJ_TILE_INSERT_COIN_SOURCE * 32,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_INSERT_COIN_RUNTIME * 32,
            OBJ_INSERT_COIN_TILE_COUNT * 32
        );
        dmaCopy(
            insert_coin_palette,
            SPRITE_PALETTE + OBJ_PAL_INSERT_COIN * 16,
            32
        );
        insert_coin_tile_upload_pending = 0;
    }
    /*
     * Source Sprite2 frames are copied into the reserved OBJ tile cache only
     * during VBlank.  The shadow OAM written below may then safely point at
     * the freshly uploaded frame, exactly one presentation frame after the
     * translated JE_drawEnemy selected it.
     */
    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        source_player_cache_commit();
        source_enemy_cache_commit();
        source_projectile_cache_commit();
        source_effect_cache_commit();
        source_effect_restore_shared_tiles_if_needed();
    }
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (game_paused) {
        /*
         * The split Sprite2 overflow slot time-shares characters 598..613.
         * Restore the six authored PAUSED glyphs after all gameplay cache
         * DMAs, before the shadow OAM that references them becomes visible.
         */
        dmaCopy(
            obj_tiles + OBJ_TILE_PAUSE_TEXT * 32u,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_PAUSE_TEXT * 32u,
            OBJ_PAUSE_GLYPH_COUNT * 2u * 32u
        );
    }
#endif
    if (oam_dirty) {
        dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
        oam_dirty = 0;
    }
    if (boss_bar_palette_dirty) {
        boss_palette[4] = boss_bar_fill_colours[boss_bar_flash][0];
        boss_palette[5] = boss_bar_fill_colours[boss_bar_flash][1];
        boss_palette[6] = boss_bar_fill_colours[boss_bar_flash][2];
        boss_bar_palette_dirty = 0;
    }
}

#define AUDIO_SOURCE_MUSIC_VOLUME_MAX 896u

static void audio_load_source_song_internal(
    u8 source_song,
    u8 cancel_effects,
    mm_pmode mode,
    u16 initial_volume
)
{
    u16 module = source_song;

    audio_level_music_fade_in_active = 0;
    audio_level_music_fade_in_step = 0;
    mmStop();
    if (cancel_effects) mmEffectCancelAll();
    /*
     * The authoritative song selection and LDS structure now come directly
     * from ROMFS music.mus.  Maxmod remains the temporary GBA synthesis
     * adapter until the OPL/LDS mixer translation replaces its cached IT
     * modules.
     */
    if (
        source_song >= JUKEBOX_MUSIC_COUNT ||
        !ot_data_mus_select(source_song)
    ) {
        return;
    }
    /*
     * The first 41 modules preserve music.mus order.  OpenTyrian's finite
     * victory, GAME OVER and Secret Level cues have separate modules with
     * their terminal Bxx source loop removed.  MM_PLAY_ONCE can then reach
     * the real order-list boundary and stop naturally.
     */
    if (mode == MM_PLAY_ONCE) {
        switch (source_song) {
        case 9:
            module = MOD_TYRIAN_MUSIC_09_ONCE;
            break;
        case 10:
            module = MOD_TYRIAN_MUSIC_10_ONCE;
            break;
        case 30:
            module = MOD_TYRIAN_MUSIC_30_ONCE;
            break;
        default:
            break;
        }
    }
    mmStart(module, mode);
    if (initial_volume > AUDIO_SOURCE_MUSIC_VOLUME_MAX) {
        initial_volume = AUDIO_SOURCE_MUSIC_VOLUME_MAX;
    }
    mmSetModuleVolume(initial_volume);
    mmSetEffectsVolume(AUDIO_SOURCE_MUSIC_VOLUME_MAX);
}

static void audio_set_source_music_volume(u8 source_volume)
{
    /*
     * OpenTyrian's tyrMusicVolume/tempVolume range is 0..255.  Preserve the
     * established calibrated Maxmod ceiling (896) while translating the
     * source's one-unit-per-tick death fade.
     */
    mmSetModuleVolume(
        (u16)(
            (
                (u32)source_volume *
                    AUDIO_SOURCE_MUSIC_VOLUME_MAX +
                127u
            ) /
            255u
        )
    );
}

static void audio_load_source_song(u8 source_song)
{
    audio_load_source_song_internal(
        source_song,
        1,
        MM_PLAY_LOOP,
        AUDIO_SOURCE_MUSIC_VOLUME_MAX
    );
}

static void audio_load_source_song_preserve_effects(u8 source_song)
{
    audio_load_source_song_internal(
        source_song,
        0,
        MM_PLAY_LOOP,
        AUDIO_SOURCE_MUSIC_VOLUME_MAX
    );
}

static void audio_load_source_song_once_preserve_effects(u8 source_song)
{
    audio_load_source_song_internal(
        source_song,
        0,
        MM_PLAY_ONCE,
        AUDIO_SOURCE_MUSIC_VOLUME_MAX
    );
}

/*
 * Scene entry deliberately starts the new module muted.  The main VBlank
 * loop raises it over TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS, so the first
 * tracker row cannot introduce a discontinuity after the silent load gap.
 */
static void audio_load_level_song_fade_in(u8 source_song)
{
    audio_load_source_song_internal(
        source_song,
        1,
        MM_PLAY_LOOP,
        0
    );
    if (mmActive()) {
        audio_level_music_fade_in_active = 1;
        audio_level_music_fade_in_step = 0;
    }
}

static void audio_level_music_fade_in_update(void)
{
    u16 volume;

    if (!audio_level_music_fade_in_active) return;
    if (!mmActive()) {
        audio_level_music_fade_in_active = 0;
        audio_level_music_fade_in_step = 0;
        return;
    }
    if (
        audio_level_music_fade_in_step <
            TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS
    ) {
        audio_level_music_fade_in_step++;
        telemetry_level_music_fade_in_steps++;
    }
    volume = (u16)(
        (
            (u32)AUDIO_SOURCE_MUSIC_VOLUME_MAX *
                audio_level_music_fade_in_step +
            TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS / 2u
        ) /
        TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS
    );
    mmSetModuleVolume(volume);
    if (
        audio_level_music_fade_in_step >=
            TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS
    ) {
        audio_level_music_fade_in_active = 0;
    }
}

static void audio_effect(u16 effect)
{
#if TYRIAN_GBA_STRESS_LOADOUT && TYRIAN_GBA_STRESS_PSG_AUDIO
    /*
     * The all-weapons diagnostic can request several effects in one logic
     * tick.  Route those deliberately extreme SFX through one native DMG
     * square channel, leaving the Maxmod background module intact.  This
     * isolates sprite/gameplay cost from sample-mixer voice pressure and is
     * also the requested "basic Game Boy channel" quality mode.
     */
    u16 frequency = (u16)(1420u + (effect & 15u) * 24u);

    if (frequency > 2047u) frequency = 2047u;
    REG_SOUNDCNT_X = 0x0080;
    REG_SOUNDCNT_L |= 0x1177;
    REG_SOUNDCNT_H = (u16)((REG_SOUNDCNT_H & 0xfffcu) | 0x0002u);
    REG_SOUND1CNT_L = 0;
    REG_SOUND1CNT_H =
        (u16)((2u << 6) | (2u << 8) | (8u << 12));
    REG_SOUND1CNT_X =
        (u16)(frequency | (1u << 14) | (1u << 15));
    telemetry_stress_psg_triggers++;
#else
    mmEffect(effect);
#endif
}

static void source_sound_effect(u8 source_sound)
{
    static const u16 source_sound_to_maxmod[39] = {
        0,
        SFX_SOURCE_SOUND_01, SFX_SOURCE_SOUND_02,
        SFX_SOURCE_SOUND_03, SFX_SOURCE_SOUND_04,
        SFX_SOURCE_SOUND_05, SFX_SOURCE_SOUND_06,
        SFX_SOURCE_SOUND_07, SFX_SOURCE_SOUND_08,
        SFX_SOURCE_SOUND_09, SFX_SOURCE_SOUND_10,
        SFX_SOURCE_SOUND_11, SFX_SOURCE_SOUND_12,
        SFX_SOURCE_SOUND_13, SFX_SOURCE_SOUND_14,
        SFX_SOURCE_SOUND_15, SFX_SOURCE_SOUND_16,
        SFX_SOURCE_SOUND_17, SFX_SOURCE_SOUND_18,
        SFX_SOURCE_SOUND_19, SFX_SOURCE_SOUND_20,
        SFX_SOURCE_SOUND_21, SFX_SOURCE_SOUND_22,
        SFX_SOURCE_SOUND_23, SFX_SOURCE_SOUND_24,
        SFX_SOURCE_SOUND_25, SFX_SOURCE_SOUND_26,
        SFX_SOURCE_SOUND_27, SFX_SOURCE_SOUND_28,
        SFX_SOURCE_SOUND_29, SFX_SOURCE_SOUND_30,
        SFX_SOURCE_SOUND_31, SFX_SOURCE_SOUND_32,
        SFX_SOURCE_SOUND_33, SFX_SOURCE_SOUND_34,
        SFX_SOURCE_SOUND_35, SFX_SOURCE_SOUND_36,
        SFX_SOURCE_SOUND_37, SFX_SOURCE_SOUND_38,
    };

    if (
        source_sound == 0 ||
        source_sound >=
            sizeof(source_sound_to_maxmod) /
                sizeof(source_sound_to_maxmod[0])
    ) {
        return;
    }
#ifdef AUTOTEST
    if (source_sound <= 32) {
        telemetry_source_sound_mask_low |=
            1u << (source_sound - 1u);
    } else {
        telemetry_source_sound_mask_high |=
            1u << (source_sound - 33u);
    }
#endif
    audio_effect(source_sound_to_maxmod[source_sound]);
}

static void toggle_pause(void)
{
    game_paused = !game_paused;
    /*
     * OpenTyrian JE_pauseGame() freezes the game world while leaving the
     * module running at half music volume. Effects retain their normal mix.
     */
    mmSetModuleVolume(game_paused ? 448 : 896);
    telemetry_pause_toggles++;
}
