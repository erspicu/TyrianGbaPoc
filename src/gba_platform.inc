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
#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW && \
    !TYRIAN_GBA_DETAIL_HAS_FINAL_FILTER
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
    /*
     * Low/386 disables brightness filtering.  Pentium and Custom apply the
     * source palette-nibble transform below, leaving the GBA colour-effects
     * unit available for their smooth wild BG2 Alpha pass.
     */
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

static u16 source_detail_spotlight_window_value(
    u16 line,
    s16 center_x,
    s16 apex_y
)
{
    s16 radius = (s16)(apex_y - (s16)line);
    s16 left;
    s16 right;

    if (radius <= 0) return 0;
    left = (s16)(center_x - radius);
    right = (s16)(center_x + radius + 1);
    if (left < 0) left = 0;
    if (right > SCREEN_WIDTH) right = SCREEN_WIDTH;
    if (left >= right) return 0;
    return (u16)((left << 8) | right);
}

static __attribute__((noinline, noclone)) void
source_detail_spotlight_prepare_table_c_reference(
    u16 *destination,
    s16 center_x,
    s16 apex_y
)
{
    u16 line;

    for (line = 0; line < SCREEN_HEIGHT; line++) {
        destination[line] = source_detail_spotlight_window_value(
            line,
            center_x,
            apex_y
        );
    }
    destination[SCREEN_HEIGHT] = 0;
}

extern IWRAM_CODE ARM_CODE void
source_detail_spotlight_prepare_table_asm(
    u16 *destination,
    s16 center_x,
    s16 apex_y
);

static void source_detail_spotlight_prepare(
    s16 center_x,
    s16 apex_y
)
{
    u8 table_index = (u8)(source_detail_spotlight_table_active ^ 1u);

#if TYRIAN_GBA_DETAIL_EFFECT_ASM
    source_detail_spotlight_prepare_table_asm(
        source_detail_spotlight_table[table_index],
        center_x,
        apex_y
    );
#else
    source_detail_spotlight_prepare_table_c_reference(
        source_detail_spotlight_table[table_index],
        center_x,
        apex_y
    );
#endif
    source_detail_spotlight_table_pending = table_index;
}

enum {
    SOURCE_DETAIL_EFFECT_NONE = 0,
    SOURCE_DETAIL_EFFECT_GLOBAL_HUE = 1,
    SOURCE_DETAIL_EFFECT_WATER_HUE = 2,
    SOURCE_DETAIL_HUE_NONE = 0xff,
};

static u16 source_detail_palette_colour(
    const u8 *palette,
    u8 index
)
{
    const u8 *rgb = palette + (u32)index * 3u;

    return (u16)(
        (rgb[0] >> 1) |
        ((rgb[1] >> 1) << 5) |
        ((rgb[2] >> 1) << 10)
    );
}

static ARM_CODE __attribute__((noinline, noclone)) u32
source_detail_palette_distance_c_reference(
    u16 colour,
    const u8 *rgb
)
{
    s16 red = (s16)(colour & 31u) - (s16)(rgb[0] >> 1);
    s16 green =
        (s16)((colour >> 5) & 31u) - (s16)(rgb[1] >> 1);
    s16 blue =
        (s16)((colour >> 10) & 31u) - (s16)(rgb[2] >> 1);

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}

#if TYRIAN_GBA_HOTPATH_ASM
extern u32 source_detail_palette_distance(u16 colour, const u8 *rgb);
#else
static u32 source_detail_palette_distance(u16 colour, const u8 *rgb)
{
    s16 red = (s16)(colour & 31u) - (s16)(rgb[0] >> 1);
    s16 green =
        (s16)((colour >> 5) & 31u) - (s16)(rgb[1] >> 1);
    s16 blue =
        (s16)((colour >> 10) & 31u) - (s16)(rgb[2] >> 1);

    return
        (u32)(red * red) +
        (u32)(green * green) +
        (u32)(blue * blue);
}
#endif

static u8 source_detail_nearest_source_index(
    u16 colour,
    const OtDataView *palette
)
{
    u32 best_distance = 0xffffffffu;
    u16 red = colour & 31u;
    u16 green = (colour >> 5) & 31u;
    u16 blue = (colour >> 10) & 31u;
    u16 luminance = (u16)(
        (red * 3u + green * 6u + blue + 5u) / 10u
    );
    u8 brightness = (u8)((luminance + 1u) >> 1);
    u8 hue;
    u8 best = 0;

    if (brightness > 15u) brightness = 15u;
    /*
     * Tyrian's low nibble is an authored brightness coordinate.  Estimate
     * it once from GBA luminance, then search only the 16 source hues.  This
     * is both more semantically stable than an unrestricted RGB match and
     * avoids delaying Maxmod by several LCD frames at level entry.
     */
    for (hue = 0; hue < 16u; hue++) {
        u8 candidate = (u8)((hue << 4) | brightness);
        u32 distance = source_detail_palette_distance(
            colour,
            palette->data + (u32)candidate * 3u
        );

        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    return best;
}

static void source_detail_prepare_palette_indices(void)
{
    OtDataView palette;
    u16 index;

    source_detail_palette_indices_valid = 0;
    if (
        background_gba_palette_active == 0 ||
        !source_detail_obj_palette_base_valid ||
        !ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)
    ) {
        return;
    }
    /*
     * This O(256*16) source-hue pass runs once while entering a level.
     * Every later fade/filter tick is O(512), preserving gameplay budget.
     */
    for (index = 0; index < 256u; index++) {
        source_detail_bg_source_index[index] =
            source_detail_nearest_source_index(
                background_gba_palette_active[index],
                &palette
            );
        source_detail_obj_source_index[index] =
            source_detail_nearest_source_index(
                source_detail_obj_palette_base[index],
                &palette
            );
    }
    source_detail_palette_indices_valid = 1;
}

static void source_detail_capture_obj_palette(void)
{
    u16 index;

    for (index = 0; index < 256u; index++) {
        source_detail_obj_palette_base[index] = SPRITE_PALETTE[index];
    }
    source_detail_obj_palette_base_valid = 1;
    source_detail_prepare_palette_indices();
}

static u8 source_detail_world_obj_palette_entry(u16 index)
{
    u8 bank = (u8)(index >> 4);

    /* 0..8 are gameplay actors; 13/15 are Sprite2 time-share banks. */
    return bank <= 8u || bank == 13u || bank == 15u;
}

static u8 source_detail_filtered_source_index(
    u8 source_index,
    u8 effect_kind,
    u8 effect_hue,
    s8 brightness
)
{
    s16 value = (s16)(source_index & 0x0fu) + brightness;
    u8 hue = (u8)(source_index >> 4);

    if (
        effect_kind == SOURCE_DETAIL_EFFECT_GLOBAL_HUE ||
        (
            effect_kind == SOURCE_DETAIL_EFFECT_WATER_HUE &&
            (source_index & 0x30u) != 0
        )
    ) {
        hue = effect_hue & 0x0fu;
    }
    if (value < 0) value = 0;
    if (value > 15) value = 15;
    return (u8)((hue << 4) | (u8)value);
}

static __attribute__((noinline, noclone)) void
source_detail_build_effect_palette_c_reference(
    const u8 *palette,
    const u8 *bg_source_index,
    const u8 *obj_source_index,
    const u16 *obj_palette_base,
    u16 *effect_bg_palette,
    u16 *effect_obj_palette,
    u8 effect_kind,
    u8 effect_hue,
    s8 brightness,
    u8 filter_objects
)
{
    u16 index;

    for (index = 0; index < 256u; index++) {
        u8 bg_source = source_detail_filtered_source_index(
            bg_source_index[index],
            effect_kind,
            effect_hue,
            brightness
        );

        effect_bg_palette[index] =
            source_detail_palette_colour(palette, bg_source);
        if (
            filter_objects &&
            source_detail_world_obj_palette_entry(index)
        ) {
            u8 obj_source = source_detail_filtered_source_index(
                obj_source_index[index],
                effect_kind,
                effect_hue,
                brightness
            );

            effect_obj_palette[index] =
                source_detail_palette_colour(palette, obj_source);
        } else {
            effect_obj_palette[index] = obj_palette_base[index];
        }
    }
}

extern ARM_CODE void source_detail_build_colour_lut_asm(
    u16 *destination,
    const u8 *source_rgb,
    u32 effect_kind,
    u32 hue_brightness
);
extern ARM_CODE void source_detail_apply_colour_lut_asm(
    u16 *destination,
    const u8 *source_indices,
    const u16 *colour_lut,
    u32 count
);

static __attribute__((noinline, noclone)) void
source_detail_build_effect_palette_asm(
    const u8 *palette,
    const u8 *bg_source_index,
    const u8 *obj_source_index,
    const u16 *obj_palette_base,
    u16 *effect_bg_palette,
    u16 *effect_obj_palette,
    u8 effect_kind,
    u8 effect_hue,
    s8 brightness,
    u8 filter_objects
)
{
    u32 hue_brightness =
        ((u32)(effect_hue & 0x0fu) << 8) |
        (u8)brightness;

    source_detail_build_colour_lut_asm(
        source_detail_effect_colour_lut,
        palette,
        effect_kind,
        hue_brightness
    );
    source_detail_apply_colour_lut_asm(
        effect_bg_palette,
        bg_source_index,
        source_detail_effect_colour_lut,
        256u
    );
    if (filter_objects) {
        source_detail_apply_colour_lut_asm(
            effect_obj_palette,
            obj_source_index,
            source_detail_effect_colour_lut,
            256u
        );
        /* PC leaves UI/reserved OBJ palette banks 9..12 and 14 untouched. */
        memcpy(
            effect_obj_palette + 9u * 16u,
            obj_palette_base + 9u * 16u,
            4u * 16u * sizeof(u16)
        );
        memcpy(
            effect_obj_palette + 14u * 16u,
            obj_palette_base + 14u * 16u,
            16u * sizeof(u16)
        );
    } else {
        memcpy(
            effect_obj_palette,
            obj_palette_base,
            256u * sizeof(u16)
        );
    }
}

static u8 source_detail_build_effect_palette(
    u8 effect_kind,
    u8 effect_hue,
    s8 brightness,
    u8 filter_objects
)
{
    OtDataView palette;

    if (
        !source_detail_palette_indices_valid ||
        !ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)
    ) {
        return 0;
    }
#if TYRIAN_GBA_DETAIL_EFFECT_ASM
    source_detail_build_effect_palette_asm(
#else
    source_detail_build_effect_palette_c_reference(
#endif
        palette.data,
        source_detail_bg_source_index,
        source_detail_obj_source_index,
        source_detail_obj_palette_base,
        source_detail_effect_bg_palette,
        source_detail_effect_obj_palette,
        effect_kind,
        effect_hue,
        brightness,
        filter_objects
    );
    telemetry_detail_palette_rebuilds++;
    return 1;
}

#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
#define SOURCE_DETAIL_WAVE_STRENGTH_FULL_Q8 256u
#define SOURCE_DETAIL_WAVE_STRENGTH_BUSY_Q8 160u
#define SOURCE_DETAIL_WAVE_STRENGTH_SEVERE_Q8 0u
#define SOURCE_DETAIL_WAVE_PRESSURE_MAX 24u
#define SOURCE_DETAIL_WAVE_PRESSURE_ENTER 6u
#define SOURCE_DETAIL_WAVE_PRESSURE_SEVERE 24u
#define SOURCE_DETAIL_WAVE_PRESSURE_EXIT 2u
#define SOURCE_DETAIL_WAVE_RENDER_HEAVY_CYCLES 220000u

static void source_detail_wave_runtime_reset(void)
{
    source_detail_wave_strength_q8 =
        SOURCE_DETAIL_WAVE_STRENGTH_FULL_Q8;
    source_detail_wave_pressure_score = 0;
    source_detail_wave_pressure_active = 0;
}

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
static void source_detail_wave_record_pressure_score(void)
{
    if (
        source_detail_wave_pressure_score >
        telemetry_detail_wave_pressure_score_max
    ) {
        telemetry_detail_wave_pressure_score_max =
            source_detail_wave_pressure_score;
    }
}

static void source_detail_wave_note_render_deferred(void)
{
    u16 score;

    if (!source_detail_wave_requested) return;
    score = (u16)source_detail_wave_pressure_score + 3u;
    if (score > SOURCE_DETAIL_WAVE_PRESSURE_MAX) {
        score = SOURCE_DETAIL_WAVE_PRESSURE_MAX;
    }
    source_detail_wave_pressure_score = (u8)score;
    if (
        source_detail_wave_pressure_score >=
        SOURCE_DETAIL_WAVE_PRESSURE_ENTER
    ) {
        source_detail_wave_pressure_active = 1;
    }
    source_detail_wave_record_pressure_score();
}

static void source_detail_wave_complete_render(u32 actual_render_cycles)
{
    u16 target;
    u16 strength;
    u16 score;
    u8 heavy;

    if (!source_detail_wave_requested) {
        source_detail_wave_runtime_reset();
        return;
    }
    heavy = (u8)(
        presentation_pending_logic_ticks > 1u ||
        actual_render_cycles >= SOURCE_DETAIL_WAVE_RENDER_HEAVY_CYCLES
    );
    if (heavy) {
        score = (u16)source_detail_wave_pressure_score + 2u;
        if (score > SOURCE_DETAIL_WAVE_PRESSURE_MAX) {
            score = SOURCE_DETAIL_WAVE_PRESSURE_MAX;
        }
        source_detail_wave_pressure_score = (u8)score;
    } else if (source_detail_wave_pressure_score != 0) {
        source_detail_wave_pressure_score--;
    }
    if (
        source_detail_wave_pressure_score >=
        SOURCE_DETAIL_WAVE_PRESSURE_ENTER
    ) {
        source_detail_wave_pressure_active = 1;
    } else if (
        source_detail_wave_pressure_score <=
        SOURCE_DETAIL_WAVE_PRESSURE_EXIT
    ) {
        source_detail_wave_pressure_active = 0;
    }
    source_detail_wave_record_pressure_score();

    if (!source_detail_wave_pressure_active) {
        target = SOURCE_DETAIL_WAVE_STRENGTH_FULL_Q8;
    } else if (
        source_detail_wave_pressure_score >=
        SOURCE_DETAIL_WAVE_PRESSURE_SEVERE
    ) {
        target = SOURCE_DETAIL_WAVE_STRENGTH_SEVERE_Q8;
    } else {
        target = SOURCE_DETAIL_WAVE_STRENGTH_BUSY_Q8;
    }
    strength = source_detail_wave_strength_q8;
    if (strength > target) {
        strength = strength > target + 32u ?
            (u16)(strength - 32u) : target;
    } else if (strength < target) {
        strength = strength + 8u < target ?
            (u16)(strength + 8u) : target;
    }
    source_detail_wave_strength_q8 = strength;
    if (strength < SOURCE_DETAIL_WAVE_STRENGTH_FULL_Q8) {
        telemetry_detail_wave_attenuated_frames++;
        if (strength < telemetry_detail_wave_strength_min_q8) {
            telemetry_detail_wave_strength_min_q8 = strength;
        }
    }
}
#endif

static u8 source_detail_wave_hardware_active(void)
{
    if (
        !source_detail_wave_requested ||
        source_detail_wave_strength_q8 == 0
    ) {
        return 0;
    }
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
    /*
     * OpenTyrian's filters vary their sample offset every eight horizontal
     * pixels. A regular GBA text BG can only apply one HOFS value to the
     * whole scanline, so the HBlank approximation can expose full-width
     * bands under sustained load even though its LUT is atomically double
     * buffered. Once the measured wave dispatcher enters pressure mode,
     * retain the authored hue/palette treatment but suspend this geometric
     * approximation and its DMA0 traffic. Gameplay cadence and the current
     * pressure-mode presentation floor are unchanged.
     */
    if (presentation_adaptive_dispatch_active) return 0;
#endif
    return 1;
}

static __attribute__((noinline, noclone)) void
source_detail_wave_prepare_table_c_reference(
    u16 *destination,
    const s8 *profile,
    u32 strength_q8,
    u32 bg0_hofs_vofs,
    u32 bg1_hofs_vofs
)
{
    u16 bg0_hofs = (u16)bg0_hofs_vofs;
    u16 bg1_hofs = (u16)bg1_hofs_vofs;
    u16 bg0_vofs = (u16)(bg0_hofs_vofs >> 16);
    u16 bg1_vofs = (u16)(bg1_hofs_vofs >> 16);
    u16 line;

    for (line = 0; line <= SCREEN_HEIGHT; line++) {
        s8 profile_wave = profile[line];
        s8 wave;

        if (strength_q8 == SOURCE_DETAIL_WAVE_STRENGTH_FULL_Q8) {
            wave = profile_wave;
        } else if (strength_q8 == 0) {
            wave = 0;
        } else {
            s32 scaled = (s32)profile_wave * strength_q8;

            if (scaled >= 0) {
                wave = (s8)((scaled + 128) >> 8);
            } else {
                wave = (s8)-(((-scaled) + 128) >> 8);
            }
        }

        destination[line * 4u + 0u] = (u16)(bg0_hofs + wave);
        destination[line * 4u + 1u] = bg0_vofs;
        destination[line * 4u + 2u] = (u16)(bg1_hofs + wave);
        destination[line * 4u + 3u] = bg1_vofs;
    }
}

extern IWRAM_CODE ARM_CODE void source_detail_wave_prepare_table_asm(
    u16 *destination,
    const s8 *profile,
    u32 strength_q8,
    u32 bg0_hofs_vofs,
    u32 bg1_hofs_vofs
);

static void source_detail_wave_prepare(u8 water)
{
    u8 table_index = (u8)(source_detail_wave_table_active ^ 1u);
    u8 profile_index = water != 0;
    u16 bg0_hofs = source_background_hofs(0);
    u16 bg1_hofs = source_background_hofs(1);
    u16 bg0_vofs = source_background_vofs(0);
    u16 bg1_vofs = source_background_vofs(1);
    u32 bg0_hofs_vofs = bg0_hofs | ((u32)bg0_vofs << 16);
    u32 bg1_hofs_vofs = bg1_hofs | ((u32)bg1_vofs << 16);

#if TYRIAN_GBA_DETAIL_EFFECT_ASM
    source_detail_wave_prepare_table_asm(
#else
    source_detail_wave_prepare_table_c_reference(
#endif
        &source_detail_wave_table[table_index][0][0],
        source_detail_wave_profile[profile_index],
        source_detail_wave_strength_q8,
        bg0_hofs_vofs,
        bg1_hofs_vofs
    );
    source_detail_wave_table_pending = table_index;
}
#endif

static void source_detail_scanline_commit(
    u8 spotlight_active,
    u8 spotlight_table_index,
    u8 wave_active,
    u8 wave_table_index
)
{
    REG_DMA0CNT = 0;
    if (spotlight_active) {
        spotlight_table_index &= 1u;
        REG_WIN0H =
            source_detail_spotlight_table[spotlight_table_index][0];
        REG_DMA0SAD = (u32)&source_detail_spotlight_table[
            spotlight_table_index
        ][1];
        REG_DMA0DAD = (u32)&REG_WIN0H;
        REG_DMA0CNT = (u32)(
            DMA_ENABLE | DMA_HBLANK | DMA_REPEAT |
            DMA_SRC_INC | DMA_DST_FIXED | DMA16 | 1u
        );
        source_detail_spotlight_table_active = spotlight_table_index;
        return;
    }
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    if (wave_active) {
        const u16 *first;

        wave_table_index &= 1u;
        first = source_detail_wave_table[wave_table_index][0];
        REG_BG0HOFS = first[0];
        REG_BG0VOFS = first[1];
        REG_BG1HOFS = first[2];
        REG_BG1VOFS = first[3];
        REG_DMA0SAD = (u32)&source_detail_wave_table[
            wave_table_index
        ][1][0];
        REG_DMA0DAD = (u32)&REG_BG0HOFS;
        REG_DMA0CNT = (u32)(
            DMA_ENABLE | DMA_HBLANK | DMA_REPEAT |
            DMA_SRC_INC | DMA_DST_RELOAD | DMA16 | 4u
        );
        source_detail_wave_table_active = wave_table_index;
    }
#else
    (void)wave_active;
    (void)wave_table_index;
#endif
}

#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
static void source_detail_set_wild_active(u8 active)
{
    source_detail_wild_active = active != 0;
}

static void source_detail_set_wild_dither_active(u8 active)
{
    u16 top_row;

    active = active != 0;
    if (source_detail_wild_dither_active == active) return;
    source_detail_wild_dither_active = active;
    if (
        !source_background2_enabled ||
        !background_layer_cache[1].prepared
    ) {
        return;
    }
    top_row = (u16)(
        background_presentation_scroll_pixel(
            1,
            source_camera_offset_y
        ) >> 3
    );
    if (background_prepare_layer(1, top_row)) {
        source_background2_upload_pending = 1;
    } else {
        source_parity_level.assets_valid = false;
    }
}
#endif

static void source_detail_prepare_scene(void)
{
    u8 special_code = (u8)(
        (source_parity_level.smoothies[8] ? 1u : 0u) +
        (source_parity_level.smoothies[5] ? 2u : 0u)
    );
    u8 vertical_flip = special_code == 1u;
    u8 effect_kind = SOURCE_DETAIL_EFFECT_NONE;
    u8 effect_hue = SOURCE_DETAIL_HUE_NONE;
    s8 effect_brightness = 0;
    u8 effect_objects = 0;
    u8 effect_active;

    background_set_vertical_flip(vertical_flip);
#if TYRIAN_GBA_DETAIL_HAS_SPOTLIGHT
    source_detail_spotlight_requested = special_code == 2u;
#else
    /* CUSTOM deliberately omits source special-code 2; LOW does so by PC gate. */
    source_detail_spotlight_requested = 0;
#endif
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_NORMAL
    source_detail_iced_requested =
        source_parity_level.smoothies[2] ||
        source_parity_level.smoothies[4];
    source_detail_blur_requested = source_parity_level.smoothies[3];
#else
    /* PC processorType 1 bypasses every smoothie 3/4/5 pass. */
    source_detail_iced_requested = 0;
    source_detail_blur_requested = 0;
#endif
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    source_detail_wave_requested =
        source_parity_level.smoothies[0] ||
        source_parity_level.smoothies[1];
#else
    source_detail_wave_requested = 0;
#endif
    source_detail_shadow_windows_active = 0;
    if (source_detail_spotlight_requested) {
        source_detail_spotlight_prepare(
            (s16)(source_player_screen_x() + 16),
            source_world_to_screen_y((s16)(player_source_y + 12))
        );
        /* starShowVGASpecialCode is later than the smoothie raster pass. */
        source_detail_wave_requested = 0;
    }
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    else if (
        source_detail_wave_hardware_active()
    ) {
        source_detail_wave_prepare(
            source_parity_level.smoothies[1] != 0
        );
    }
#endif

#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
    source_detail_set_wild_active(
        source_background2_present() &&
        source_parity_level.background2_over != 3u &&
        !source_parity_level.background2_not_transparent
    );
    /*
     * PC starShowVGASpecialCode 2 needs a region-selective brighten/darken
     * mode, which cannot share the single GBA colour-effects unit with BG1
     * Alpha in the same scanline.  Keep dither solely for that rare conflict;
     * ordinary wild clouds stay smoothly hardware blended.
     */
    source_detail_set_wild_dither_active(
        source_detail_wild_active && source_detail_spotlight_requested
    );
#endif

    /* Exact tyrian2.c filter order, represented by GBA palette adapters. */
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_NORMAL
    if (source_parity_level.smoothies[4]) {
        effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
        effect_hue = 8u;
    }
#endif
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    if (
        source_parity_level.smoothies[0] &&
        source_parity_level.smoothie_data[0] == 0
    ) {
        effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
        effect_hue = 7u;
    }
    if (source_parity_level.smoothies[1]) {
        effect_kind = SOURCE_DETAIL_EFFECT_WATER_HUE;
        effect_hue = source_parity_level.smoothie_data[1] & 0x0fu;
    }
    if (
        source_parity_level.smoothies[0] &&
        source_parity_level.smoothie_data[0] > 0
    ) {
        effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
        effect_hue = 7u;
        effect_objects = 1;
    }
#endif
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_NORMAL
    if (source_parity_level.smoothies[2]) {
        effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
        effect_hue = 8u;
        effect_objects = 1;
    }
#endif
#if TYRIAN_GBA_DETAIL_HAS_FINAL_FILTER
    if (source_parity_level.filter_active) {
        if (source_parity_level.level_filter != -99) {
            effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
            effect_hue =
                (u8)source_parity_level.level_filter & 0x0fu;
            effect_objects = 1;
        }
        if (source_parity_level.level_brightness != -99) {
            effect_brightness =
                source_parity_level.level_brightness;
            if (effect_brightness < -15) effect_brightness = -15;
            if (effect_brightness > 15) effect_brightness = 15;
            effect_objects = 1;
        }
    }
#endif
    effect_active = (u8)(
        effect_kind != SOURCE_DETAIL_EFFECT_NONE ||
        effect_brightness != 0
    );
    if (
        source_detail_palette_mode_pending != effect_active ||
        source_detail_effect_kind_pending != effect_kind ||
        source_detail_effect_hue_pending != effect_hue ||
        source_detail_effect_brightness_pending != effect_brightness ||
        source_detail_effect_objects_pending != effect_objects
    ) {
        source_detail_palette_mode_pending = effect_active;
        source_detail_effect_kind_pending = effect_kind;
        source_detail_effect_hue_pending = effect_hue;
        source_detail_effect_brightness_pending = effect_brightness;
        source_detail_effect_objects_pending = effect_objects;
        if (
            effect_active &&
            !source_detail_build_effect_palette(
                effect_kind,
                effect_hue,
                effect_brightness,
                effect_objects
            )
        ) {
            source_parity_level.assets_valid = false;
        }
        source_detail_palette_dirty = 1;
    }
}

static void source_detail_palette_commit(u8 effect_active)
{
    if (
        source_detail_palette_mode_active == effect_active &&
        !source_detail_palette_dirty
    ) {
        return;
    }
    if (effect_active) {
        dmaCopy(
            source_detail_effect_bg_palette,
            BG_PALETTE,
            sizeof(source_detail_effect_bg_palette)
        );
    } else if (background_gba_palette_active != 0) {
        dmaCopy(
            background_gba_palette_active,
            BG_PALETTE,
            BACKGROUND_GBA_PALETTE_VARIANT_BYTES
        );
    }
    if (source_detail_obj_palette_base_valid) {
        dmaCopy(
            effect_active ?
                source_detail_effect_obj_palette :
                source_detail_obj_palette_base,
            SPRITE_PALETTE,
            sizeof(source_detail_obj_palette_base)
        );
    }
    source_detail_palette_mode_active = effect_active;
    source_detail_palette_dirty = 0;
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static u8 source_special_zinglon_window(
    u16 *horizontal,
    u16 *vertical
)
{
    s16 centre;
    s16 left;
    s16 right;
    s16 outer_half_width;

    if (
        !source_special_zinglon_visual_active ||
        horizontal == 0 ||
        vertical == 0
    ) {
        return 0;
    }
    centre = source_world_to_screen_x((s16)(player_source_x + 7));
    /* The second PC JE_barBright() extends two pixels past the inner pass. */
    outer_half_width = (s16)(
        source_special_zinglon_visual_half_width + 2u
    );
    left = (s16)(centre - outer_half_width);
    right = (s16)(centre + outer_half_width + 1);
    if (left < 0) left = 0;
    if (right > SCREEN_WIDTH) right = SCREEN_WIDTH;
    if (left >= right) return 0;

    *horizontal = (u16)(((u16)left << 8) | (u16)right);
    *vertical = SCREEN_HEIGHT;
    return 1;
}
#endif

static void source_detail_compose_effect_registers(
    u8 background2_present,
    u16 *window_enable,
    u16 *bldcnt,
    u16 *bldalpha,
    u16 *bldy,
    u16 *win0h,
    u16 *win0v,
    u16 *winin,
    u16 *winout
)
{
    u16 blend_first = 0;
    u16 blend_second = 0;

    *window_enable = 0;
    *win0h = 0;
    *win0v = 0;
    *winin = 0;
    *winout = 0;
    (void)background2_present;
    /*
     * Semi-transparent source objects (explosions, OPTION_SHAPES and the
     * invulnerable ship) require valid second targets at every detail level.
     */
    blend_second = 0x003fu;
#if TYRIAN_GBA_STRESS_LOADOUT
    blend_first |= 1u << 4;
    blend_second |= 1u | 2u | 4u | 8u | 16u | 32u;
#endif
    /*
     * PC applies JE_barBright twice across the complete vertical Zinglon
     * rectangle.  The former GBA adapter drew only six outline pixels.  Use
     * WIN0 to confine the hardware brightness-increase unit to the authored
     * filled beam instead; all BG and OBJ pixels retain their hue underneath.
     * A single BLDY=5 pass is the closest smooth GBA approximation to the
     * PC inner +4 / two-pixel outer +2 palette-brightness profile.
     */
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (source_special_zinglon_window(win0h, win0v)) {
        *window_enable = WIN0_ON;
        /* All layers visible; colour effects enabled only inside WIN0. */
        *winin = 0x003fu;
        *winout = (u16)(0x001fu | (0x001fu << 8));
        *bldcnt = (u16)(
            0x003fu | (2u << 6) | (blend_second << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = 5u;
    } else
#endif
    /* Pentium wild BG2 normally owns the smooth hardware Alpha pass. */
    if (source_detail_spotlight_requested) {
        *window_enable = WIN0_ON;
        *win0h = source_detail_spotlight_window_value(
            0,
            (s16)(source_player_screen_x() + 16),
            source_world_to_screen_y((s16)(player_source_y + 12))
        );
        *win0v = SCREEN_HEIGHT;
        /* WIN0 is the lit cone; the outside region enables darkening. */
        *winin = 0x001fu;
        *winout = (u16)(0x003fu | (0x001fu << 8));
        *bldcnt = (u16)(
            0x003fu | (3u << 6) | (blend_second << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = 12u;
    } else if (gameplay_filter_brightness_level() != 0) {
        *bldcnt = (u16)(
            gameplay_filter_bldcnt() | (blend_second << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = gameplay_filter_brightness_level();
    }
#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
    else if (
        background2_present &&
        source_detail_wild_active &&
        !source_detail_wild_dither_active
    ) {
        /*
         * PC draw_background_2_blend() averages BG2 with the framebuffer.
         * PC BG2 is GBA BG1; all layers underneath it are valid second
         * targets.  8:8 is an exact 50/50 hardware mix with no grid pattern.
         */
        *bldcnt = (u16)(
            (1u << 1) | (1u << 6) | (0x003fu << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = 0;
    }
#endif
    else if (source_detail_shadow_windows_active) {
        *window_enable = OBJ_WIN_ON;
        /* Normal screen has no colour effect; OBJ-window masks darken. */
        *winout = (u16)(0x001fu | (0x003fu << 8));
        *bldcnt = (u16)(
            0x003fu | (3u << 6) | (blend_second << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = 8u;
    } else if (blend_first != 0) {
        *bldcnt = (u16)(
            blend_first | (1u << 6) | (blend_second << 8)
        );
        *bldalpha = 8u | (8u << 8);
        *bldy = 0;
    } else {
        /* Target-2 bits are still required by semi-transparent OBJ mode. */
        *bldcnt = (u16)(blend_second << 8);
        *bldalpha = 8u | (8u << 8);
        *bldy = 0;
    }
}

#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
static u8 gameplay_presentation_wave_scope_requested(void)
{
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH && \
    TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    u8 special_code = (u8)(
        (source_parity_level.smoothies[8] ? 1u : 0u) +
        (source_parity_level.smoothies[5] ? 2u : 0u)
    );

    /*
     * This mirrors source_detail_prepare_scene() without building a scene.
     * Source special code 2 owns DMA0 for its spotlight window, so authored
     * lava/water flags do not produce a scanline wave in that case.
     */
    return (u8)(
        (
            source_parity_level.smoothies[0] ||
            source_parity_level.smoothies[1]
        ) &&
        special_code != 2u
    );
#else
    return 0;
#endif
}
#endif

static void gameplay_presentation_capture_registers(void)
{
    u8 background2_priority = source_background2_priority();
    u8 background3_priority = source_background3_priority();
    u8 background2_present = source_background2_present();
    u16 window_enable;

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
    gameplay_presentation.bg0cnt =
        BG_PRIORITY(3) |
        CHAR_BASE(background_cache_char_base[0]) |
        SCREEN_BASE(BG0_SCREEN_BLOCK) |
        BG_16_COLOR | BG_SIZE_1;
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
    source_detail_compose_effect_registers(
        background2_present,
        &window_enable,
        &gameplay_presentation.bldcnt,
        &gameplay_presentation.bldalpha,
        &gameplay_presentation.bldy,
        &gameplay_presentation.win0h,
        &gameplay_presentation.win0v,
        &gameplay_presentation.winin,
        &gameplay_presentation.winout
    );
    gameplay_presentation.dispcnt =
        MODE_0 | BG2_ON | OBJ_ON | OBJ_1D_MAP |
#if !TYRIAN_GBA_STRESS_LOADOUT
        (!source_special_astral_visual_active ? BG0_ON : 0) |
#else
        BG0_ON |
#endif
        (background2_present ? BG1_ON : 0) |
        (
            game_state == STATE_PLAY && gameplay_overlay_active ?
                BG3_ON : 0
        ) |
        window_enable;
    /*
     * OpenTyrian's smoothie 4 averages the current intermediate framebuffer
     * with the previous frame one pixel at a time.  GBA Mode 0 has no retained
     * world framebuffer; hardware Mosaic enlarges pixels instead of averaging
     * them and also corrupts actors drawn after the source blur pass.  Keep the
     * authored request/telemetry, but prefer the undistorted Mode-0 image over
     * that semantically incorrect approximation.
     */
    gameplay_presentation.mosaic = 0;
    gameplay_presentation.spotlight_active =
#if !TYRIAN_GBA_STRESS_LOADOUT
        source_detail_spotlight_requested &&
        !source_special_zinglon_visual_active;
#else
        source_detail_spotlight_requested;
#endif
    gameplay_presentation.spotlight_table_index =
        source_detail_spotlight_table_pending;
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    gameplay_presentation.wave_active =
        source_detail_wave_hardware_active();
#else
    gameplay_presentation.wave_active = 0;
#endif
    gameplay_presentation.wave_table_index =
        source_detail_wave_table_pending;
    gameplay_presentation.effect_palette_active =
        source_detail_palette_mode_pending;
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
    u8 state_transition_forces_render,
    u8 logic_updates_this_loop
)
{
#if TYRIAN_GBA_PRESENTATION_DEFER
    u32 now;
    u32 elapsed;
    u32 deadline;
    u8 layer;
    u8 current_held_rows = 0;
    u8 forced = state_transition_forces_render;
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    u8 wave_scope =
        gameplay_presentation_wave_scope_requested();
    u8 adaptive = presentation_adaptive_dispatch_active;
    u8 adaptive_severe = (u8)(
        adaptive && presentation_adaptive_dispatch_severe
    );
    u8 pending_limit =
        adaptive ?
            (
                adaptive_severe ?
                    PRESENTATION_ADAPTIVE_SEVERE_MAX_PENDING_LOGIC_TICKS :
                    PRESENTATION_ADAPTIVE_MEDIUM_MAX_PENDING_LOGIC_TICKS
            ) :
            PRESENTATION_MAX_PENDING_LOGIC_TICKS;
    u8 minimum_render_logic_ticks =
        adaptive_severe ?
            PRESENTATION_ADAPTIVE_SEVERE_MIN_RENDER_LOGIC_TICKS :
            PRESENTATION_ADAPTIVE_MEDIUM_MIN_RENDER_LOGIC_TICKS;
#else
    const u8 pending_limit =
        PRESENTATION_MAX_PENDING_LOGIC_TICKS;
#endif
#endif

#if TYRIAN_GBA_PRESENTATION_DEFER && \
    !TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    (void)logic_updates_this_loop;
#endif

    if (!presentation_render_pending) return 0;
    telemetry_presentation_render_attempts++;
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    presentation_adaptive_dispatch_idle_render = 0;
#endif
#if !TYRIAN_GBA_PRESENTATION_DEFER
    (void)frame_cycle_start;
    (void)state_transition_forces_render;
    (void)logic_updates_this_loop;
    telemetry_presentation_render_forced++;
    return 1;
#else
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    telemetry_adaptive_dispatch_attempts++;
    if (wave_scope) {
        telemetry_wave_dispatch_scope_attempts++;
    }
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
    if (
        adaptive &&
        wave_scope &&
        !presentation_adaptive_dispatch_severe
    ) {
        /* Wave owns DMA0 timing and immediately upgrades an active tier. */
        presentation_adaptive_dispatch_severe = 1;
        adaptive_severe = 1;
        pending_limit =
            PRESENTATION_ADAPTIVE_SEVERE_MAX_PENDING_LOGIC_TICKS;
        minimum_render_logic_ticks =
            PRESENTATION_ADAPTIVE_SEVERE_MIN_RENDER_LOGIC_TICKS;
        telemetry_adaptive_dispatch_severe_entries++;
        telemetry_wave_dispatch_entries++;
    }
#endif
#endif
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
            pending_limit ||
        current_held_rows > BACKGROUND_RETAINED_TILE_ROWS
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
        ||
        (
            adaptive &&
            current_held_rows >= BACKGROUND_RETAINED_TILE_ROWS
        )
#endif
    ) {
        forced = 1;
    }
    if (forced) {
        telemetry_presentation_render_forced++;
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
        if (adaptive) {
            telemetry_adaptive_dispatch_safety_forced++;
            presentation_adaptive_dispatch_idle_render =
                logic_updates_this_loop == 0;
        }
        if (adaptive && wave_scope) {
            telemetry_wave_dispatch_safety_forced++;
        }
#endif
        return 1;
    }

#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    if (
        adaptive &&
        logic_updates_this_loop == 0 &&
        presentation_pending_logic_ticks <
            minimum_render_logic_ticks
    ) {
        /*
         * Once measured overload is active, lock presentation to a stable
         * source-tick cadence instead of opportunistically producing an
         * irregular sequence of frames.  Both Medium and Severe are capped
         * at the configured two logic ticks (~17.4 FPS); Severe remains a
         * pressure/telemetry state and never selects the old three-tick
         * cadence.  Authoritative gameplay and audio keep full cadence, while
         * skipped LCD periods remain available for cache prefetch.  The
         * safety and freshness forcing above always wins.
         */
        telemetry_presentation_render_deferred++;
        return 0;
    }
#endif

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
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
        source_detail_wave_note_render_deferred();
#endif
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
        if (
            presentation_adaptive_dispatch_pressure <
                PRESENTATION_ADAPTIVE_PRESSURE_MAX
        ) {
            presentation_adaptive_dispatch_pressure++;
        }
        {
            u32 missed_since_baseline =
                telemetry_missed_vblanks -
                presentation_adaptive_dispatch_missed_baseline;
            u8 entry_ready = (u8)(
                presentation_adaptive_dispatch_pressure >=
                    PRESENTATION_ADAPTIVE_GENERAL_PRESSURE_ENTER &&
                missed_since_baseline >=
                    PRESENTATION_ADAPTIVE_GENERAL_MISSED_ENTER
            );
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
            if (
                wave_scope &&
                presentation_adaptive_dispatch_pressure >=
                    PRESENTATION_ADAPTIVE_WAVE_PRESSURE_ENTER &&
                missed_since_baseline >=
                    PRESENTATION_ADAPTIVE_WAVE_MISSED_ENTER
            ) {
                entry_ready = 1;
            }
#endif

            if (
                !presentation_adaptive_dispatch_active &&
                entry_ready
            ) {
                presentation_adaptive_dispatch_active = 1;
                presentation_adaptive_dispatch_severe = (u8)(
                    missed_since_baseline >=
                        PRESENTATION_ADAPTIVE_SEVERE_MISSED ||
                    presentation_adaptive_dispatch_pressure >=
                        PRESENTATION_ADAPTIVE_SEVERE_PRESSURE
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
                    || wave_scope
#endif
                );
                presentation_adaptive_dispatch_recovery_ticks = 0;
                telemetry_adaptive_dispatch_entries++;
                if (presentation_adaptive_dispatch_severe) {
                    telemetry_adaptive_dispatch_severe_entries++;
                }
                if (wave_scope) telemetry_wave_dispatch_entries++;
            } else if (
                presentation_adaptive_dispatch_active &&
                !presentation_adaptive_dispatch_severe &&
                (
                    presentation_adaptive_dispatch_pressure >=
                        PRESENTATION_ADAPTIVE_SEVERE_PRESSURE ||
                    missed_since_baseline >=
                        PRESENTATION_ADAPTIVE_SEVERE_MISSED
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
                    || wave_scope
#endif
                )
            ) {
                presentation_adaptive_dispatch_severe = 1;
                presentation_adaptive_dispatch_recovery_ticks = 0;
                telemetry_adaptive_dispatch_severe_entries++;
                if (wave_scope) telemetry_wave_dispatch_entries++;
            }
        }
#endif
        telemetry_presentation_render_deferred++;
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
        if (adaptive && logic_updates_this_loop != 0) {
            telemetry_adaptive_dispatch_logic_busy_deferred++;
        }
        if (
            adaptive &&
            wave_scope &&
            logic_updates_this_loop != 0
        ) {
            telemetry_wave_dispatch_logic_busy_deferred++;
        }
#endif
        return 0;
    }
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    if (adaptive && logic_updates_this_loop != 0) {
        /*
         * Keep authoritative logic and complete scene construction in
         * separate LCD periods until measured light renders prove recovery.
         * Intentional phase separation is not new pressure input.
         */
        telemetry_presentation_render_deferred++;
        telemetry_adaptive_dispatch_logic_busy_deferred++;
        if (wave_scope) {
            telemetry_wave_dispatch_logic_busy_deferred++;
        }
        return 0;
    } else if (adaptive) {
        presentation_adaptive_dispatch_idle_render = 1;
    }
#endif
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
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    source_detail_wave_complete_render(actual_render_cycles);
#endif
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    {
        u8 wave_scope =
            gameplay_presentation_wave_scope_requested();

        if (presentation_adaptive_dispatch_idle_render) {
            telemetry_adaptive_dispatch_idle_renders++;
            if (wave_scope) telemetry_wave_dispatch_idle_renders++;
        }
    }
    if (presentation_adaptive_dispatch_active) {
        if (
            actual_render_cycles <=
                PRESENTATION_ADAPTIVE_RECOVERY_RENDER_CYCLES
        ) {
            if (
                presentation_adaptive_dispatch_recovery_ticks <
                    PRESENTATION_ADAPTIVE_RECOVERY_LIGHT_RENDERS
            ) {
                presentation_adaptive_dispatch_recovery_ticks++;
            }
            if (
                presentation_adaptive_dispatch_recovery_ticks >=
                    PRESENTATION_ADAPTIVE_RECOVERY_LIGHT_RENDERS
            ) {
                presentation_adaptive_dispatch_active = 0;
                presentation_adaptive_dispatch_severe = 0;
                presentation_adaptive_dispatch_pressure = 0;
                presentation_adaptive_dispatch_recovery_ticks = 0;
                presentation_adaptive_dispatch_missed_baseline =
                    telemetry_missed_vblanks;
                telemetry_adaptive_dispatch_exits++;
                if (
                    telemetry_wave_dispatch_exits <
                        telemetry_wave_dispatch_entries
                ) {
                    telemetry_wave_dispatch_exits++;
                }
            }
        } else {
            presentation_adaptive_dispatch_recovery_ticks = 0;
            if (
                presentation_adaptive_dispatch_pressure <
                    PRESENTATION_ADAPTIVE_PRESSURE_MAX
            ) {
                presentation_adaptive_dispatch_pressure++;
            }
            if (
                !presentation_adaptive_dispatch_severe &&
                presentation_adaptive_dispatch_pressure >=
                    PRESENTATION_ADAPTIVE_SEVERE_PRESSURE
            ) {
                presentation_adaptive_dispatch_severe = 1;
                telemetry_adaptive_dispatch_severe_entries++;
                if (gameplay_presentation_wave_scope_requested()) {
                    telemetry_wave_dispatch_entries++;
                }
            }
        }
    } else if (
        actual_render_cycles <=
            PRESENTATION_ADAPTIVE_RECOVERY_RENDER_CYCLES
    ) {
        /*
         * A genuinely light complete render disproves accumulated deadline
         * pressure.  Heavy renders retain the evidence across intervening
         * LCD periods, so sustained overload can enter adaptive dispatch
         * while isolated cold-cache spikes cannot.
         */
        presentation_adaptive_dispatch_pressure = 0;
        presentation_adaptive_dispatch_missed_baseline =
            telemetry_missed_vblanks;
    } else {
        if (
            presentation_adaptive_dispatch_pressure <
                PRESENTATION_ADAPTIVE_PRESSURE_MAX
        ) {
            presentation_adaptive_dispatch_pressure++;
        }
    }
    presentation_adaptive_dispatch_idle_render = 0;
#endif
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
        source_detail_scanline_commit(0, 0, 0, 0);
        frontend_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    if (game_state == STATE_JUKEBOX) {
        source_detail_scanline_commit(0, 0, 0, 0);
        jukebox_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    if (
        game_state != STATE_PLAY &&
        game_state != STATE_GAME_OVER
    ) {
        /*
         * A level can exit while the Normal spotlight or iced palette is
         * active.  Those are presentation-only states and must not leak into
         * the Mode-0 result/statistics screen that follows gameplay.
         */
        source_detail_scanline_commit(0, 0, 0, 0);
        source_detail_palette_commit(0);
        REG_MOSAIC = 0;
        REG_WIN0H = 0;
        REG_WIN0V = 0;
        REG_WININ = 0;
        REG_WINOUT = 0;
        REG_BLDCNT = 0;
        REG_BLDALPHA = 0;
        REG_BLDY = 0;
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
        REG_BG0CNT = gameplay_presentation.bg0cnt;
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
        REG_MOSAIC = gameplay_presentation.mosaic;
        REG_WIN0H = gameplay_presentation.win0h;
        REG_WIN0V = gameplay_presentation.win0v;
        REG_WININ = gameplay_presentation.winin;
        REG_WINOUT = gameplay_presentation.winout;
        source_detail_palette_commit(
            gameplay_presentation.effect_palette_active
        );
        source_detail_scanline_commit(
            gameplay_presentation.spotlight_active,
            gameplay_presentation.spotlight_table_index,
            gameplay_presentation.wave_active,
            gameplay_presentation.wave_table_index
        );
#else
        u8 background2_priority = 2;
        u8 background3_priority = 1;
        u8 background2_present;
        u16 window_enable;
        u16 bldcnt;
        u16 bldalpha;
        u16 bldy;
        u16 win0h;
        u16 win0v;
        u16 winin;
        u16 winout;

        background2_priority = source_background2_priority();
        background3_priority = source_background3_priority();
        background2_present = source_background2_present();
        REG_BG0HOFS = source_background_hofs(0);
        REG_BG1HOFS = source_background_hofs(1);
        REG_BG2HOFS = source_background_hofs(2);
        REG_BG0VOFS = source_background_vofs(0);
        REG_BG1VOFS = source_background_vofs(1);
        REG_BG2VOFS = source_background_vofs(2);
        REG_BG0CNT = BG_PRIORITY(3) |
                     CHAR_BASE(background_cache_char_base[0]) |
                     SCREEN_BASE(BG0_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
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
        source_detail_compose_effect_registers(
            background2_present,
            &window_enable,
            &bldcnt,
            &bldalpha,
            &bldy,
            &win0h,
            &win0v,
            &winin,
            &winout
        );
        REG_DISPCNT =
            MODE_0 | BG2_ON | OBJ_ON | OBJ_1D_MAP |
#if !TYRIAN_GBA_STRESS_LOADOUT
            (!source_special_astral_visual_active ? BG0_ON : 0) |
#else
            BG0_ON |
#endif
            (background2_present ? BG1_ON : 0) |
            (
                game_state == STATE_PLAY && gameplay_overlay_active ?
                    BG3_ON : 0
            ) |
            window_enable;
        REG_BLDCNT = bldcnt;
        REG_BLDALPHA = bldalpha;
        REG_BLDY = bldy;
        REG_MOSAIC = 0;
        REG_WIN0H = win0h;
        REG_WIN0V = win0v;
        REG_WININ = winin;
        REG_WINOUT = winout;
        source_detail_palette_commit(
            source_detail_palette_mode_pending
        );
        source_detail_scanline_commit(
#if !TYRIAN_GBA_STRESS_LOADOUT
            source_detail_spotlight_requested &&
                !source_special_zinglon_visual_active,
#else
            source_detail_spotlight_requested,
#endif
            source_detail_spotlight_table_pending,
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
            source_detail_wave_hardware_active(),
#else
            0,
#endif
            source_detail_wave_table_pending
        );
#endif
        /*
         * Count the source software-filter requests independently from the
         * GBA presentation.  This distinguishes a cheap hardware detail
         * profile from a level that actually asks for the PC framebuffer
         * lava/water/iced/blur passes.
         */
#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
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
#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
        if (source_detail_wild_active) {
            telemetry_detail_wild_frames++;
            if (source_detail_wild_dither_active) {
                telemetry_detail_wild_dither_frames++;
            }
        }
        if (
            source_parity_level.filter_active &&
            source_parity_level.level_filter != -99
        ) {
            telemetry_detail_filter_hue_frames++;
        }
#endif
        if (source_detail_wave_requested) {
            telemetry_detail_wave_frames++;
        }
        if (boss_obj_palette_restore_pending) {
            dmaCopy(
                obj_palette,
                SPRITE_PALETTE,
                512
            );
            boss_obj_palette_restore_pending = 0;
            /* Reapply an active source filter after this one-frame restore. */
            if (source_detail_palette_mode_pending) {
                source_detail_palette_dirty = 1;
            }
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
        source_static_atlas_upload_commit();
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
    static const u16 source_xmas_voice_to_maxmod[9] = {
        SFX_SOURCE_XMAS_VOICE_01, SFX_SOURCE_XMAS_VOICE_02,
        SFX_SOURCE_XMAS_VOICE_03, SFX_SOURCE_XMAS_VOICE_04,
        SFX_SOURCE_XMAS_VOICE_05, SFX_SOURCE_XMAS_VOICE_06,
        SFX_SOURCE_XMAS_VOICE_07, SFX_SOURCE_XMAS_VOICE_08,
        SFX_SOURCE_XMAS_VOICE_09,
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
    if (
        ot_season_mode() == OT_SEASON_XMAS &&
        source_sound >= 30u
    ) {
        audio_effect(source_xmas_voice_to_maxmod[source_sound - 30u]);
        return;
    }
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
