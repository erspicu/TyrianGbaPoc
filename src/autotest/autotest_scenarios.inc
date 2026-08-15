#ifdef AUTOTEST_FULL_LOADOUT_STRESS
extern u32 gameplay_overlay_colour_distance_asm(u16 left, u16 right);
extern u32 source_detail_palette_distance_asm(
    u16 colour,
    const u8 *rgb
);
extern IWRAM_CODE ARM_CODE bool ot_player_shot_axis_overlaps_asm(
    int16_t delta,
    uint16_t radius
);
extern IWRAM_CODE ARM_CODE bool
ot_player_shot_axis_overlaps_unsigned_asm(
    int16_t delta,
    uint16_t radius
);
#if TYRIAN_GBA_COLLISION_UNSIGNED_RANGE
#define AUTOTEST_AXIS_OVERLAPS_ASM \
    ot_player_shot_axis_overlaps_unsigned_asm
#else
#define AUTOTEST_AXIS_OVERLAPS_ASM ot_player_shot_axis_overlaps_asm
#endif

static u32 autotest_colour_distance_differential_result EWRAM_BSS;
static u32 autotest_level_port_hotpath_differential_result EWRAM_BSS;
static u32 autotest_overlay_distance_c_cycles EWRAM_BSS;
static u32 autotest_overlay_distance_asm_cycles EWRAM_BSS;
static u32 autotest_palette_distance_c_cycles EWRAM_BSS;
static u32 autotest_palette_distance_asm_cycles EWRAM_BSS;
static u32 autotest_axis_overlap_c_cycles EWRAM_BSS;
static u32 autotest_axis_overlap_asm_cycles EWRAM_BSS;
static u32 autotest_sprite2_exact_differential_result EWRAM_BSS;
static u32 autotest_sprite2_exact_c_cycles EWRAM_BSS;
static u32 autotest_sprite2_exact_asm_cycles EWRAM_BSS;
static u32 autotest_starfield_differential_result EWRAM_BSS;
static u32 autotest_starfield_divmod_c_cycles EWRAM_BSS;
static u32 autotest_starfield_divmod_asm_cycles EWRAM_BSS;
static u32 autotest_starfield_plot_c_cycles EWRAM_BSS;
static u32 autotest_starfield_plot_asm_cycles EWRAM_BSS;
static u32 autotest_projectile_hint_differential_result EWRAM_BSS;
static u32 autotest_projectile_hint_c_cycles EWRAM_BSS;
static u32 autotest_projectile_hint_asm_cycles EWRAM_BSS;
static u32 autotest_pool_bit_scan_differential_result EWRAM_BSS;
static u32 autotest_pool_lowest_c_cycles EWRAM_BSS;
static u32 autotest_pool_lowest_asm_cycles EWRAM_BSS;
static u32 autotest_pool_highest_c_cycles EWRAM_BSS;
static u32 autotest_pool_highest_asm_cycles EWRAM_BSS;
static u32 autotest_hotpath_benchmark_sink EWRAM_BSS;
static u32 autotest_detail_effect_differential_result EWRAM_BSS;
static u32 autotest_detail_palette_c_cycles EWRAM_BSS;
static u32 autotest_detail_palette_asm_cycles EWRAM_BSS;
static u32 autotest_detail_spotlight_c_cycles EWRAM_BSS;
static u32 autotest_detail_spotlight_asm_cycles EWRAM_BSS;
static u32 autotest_detail_wave_c_cycles EWRAM_BSS;
static u32 autotest_detail_wave_asm_cycles EWRAM_BSS;
static u32 autotest_detail_effect_benchmark_sink EWRAM_BSS;
static u32 autotest_enemy_pool_consistency_snapshot EWRAM_BSS;
static u32 autotest_enemy_pool_active_visits_snapshot EWRAM_BSS;
static u32 autotest_enemy_pool_linear_visits_snapshot EWRAM_BSS;
static u32 autotest_enemy_allocator_mask_probes_snapshot EWRAM_BSS;
static u32 autotest_enemy_allocator_slot_probes_snapshot EWRAM_BSS;
static u32 autotest_collision_hit_apply_calls_snapshot EWRAM_BSS;
static u32 autotest_collision_status_link_visits_snapshot EWRAM_BSS;
static u32 autotest_collision_kill_group_visits_snapshot EWRAM_BSS;
static u32 autotest_collision_damaged_transition_visits_snapshot EWRAM_BSS;
static u32 autotest_collision_player_contact_visits_snapshot EWRAM_BSS;
static u32 autotest_collision_zinglon_visits_snapshot EWRAM_BSS;
static u32 autotest_boss_manifest_spawn_count_snapshot EWRAM_BSS;
static u32 autotest_boss_manifest_active_count_snapshot EWRAM_BSS;
static u32 autotest_boss_manifest_episode_snapshot EWRAM_BSS;
static u32 autotest_boss_manifest_lvl_snapshot EWRAM_BSS;

static u32 autotest_enemy_pool_mask_consistency(void)
{
#if TYRIAN_GBA_ENEMY_ACTIVE_MASK
    u8 index;
    u32 result = 0;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        u32 bit = 1u << (index & 31u);
        u8 active = (
            source_parity_level.enemy_active_mask[index >> 5] & bit
        ) != 0;
        u8 collidable = (
            source_parity_level.player_shot_collision_active_mask[
                index >> 5
            ] & bit
        ) != 0;

        if (active != (source_parity_level.enemy_avail[index] != 1)) {
            return result;
        }
        if (collidable != (source_parity_level.enemy_avail[index] == 0)) {
            return result | 1u;
        }
    }
    result |= 3u;
    return result | 4u;
#else
    return 0;
#endif
}

static u32 autotest_mt_rand_reference(OtLevelPortState *state)
{
    OtMt19937 *rng = &state->rng;
    u32 combined =
        (rng->values[rng->p0] & 0x80000000u) |
        (rng->values[rng->p1] & 0x7fffffffu);
    u32 odd_mask = 0u - (rng->values[rng->p1] & 1u);
    u32 value = rng->values[rng->pm] ^ (combined >> 1) ^
        (odd_mask & 0x9908b0dfu);

    rng->values[rng->p0] = value;
    rng->p0 = rng->p1;
    rng->p1++;
    rng->pm++;
    if (rng->pm == OT_MT_STATE_COUNT) rng->pm = 0;
    if (rng->p1 == OT_MT_STATE_COUNT) rng->p1 = 0;

    value ^= value >> 11;
    value ^= (value << 7) & 0x9d2c5680u;
    value ^= (value << 15) & 0xefc60000u;
    value ^= value >> 18;
    state->rng_call_count++;
    return value;
}

static u32 autotest_sprite2_pack_reference(
    const u8 *raw,
    u8 filter,
    const u8 *palette_map
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
            pixel = palette_map[pixel];
        }
        packed |= (u32)pixel << (lane * 8u);
    }
    return packed;
}

static u8 autotest_hotpath_asm_self_test(void)
{
    u8 *scale_source = (u8 *)(void *)&frontend_gameplay_arena;
    u8 *scale_expected = (u8 *)(void *)frontend_sprite2_decode_scratch;
    u8 raw[4];
    u8 rgb[3];
    u8 star_expected[32];
    u8 star_actual[32];
    u32 colour_index;
    u32 position;
    u16 channel;
    u16 left_component;
    u16 right_component;
    u16 rgb_component;
    u16 index;
    u16 filter;

    autotest_starfield_differential_result = 0;
    /* Complete u16 source-position domain for the exact divmod projection. */
    for (position = 0; position < 65536u; position++) {
        if (
            gameplay_overlay_divmod320_asm((u16)position) !=
            gameplay_overlay_divmod320_c_reference((u16)position)
        ) {
            return 0;
        }
    }
    autotest_starfield_differential_result |= 1u;
    /*
     * Complete 8x8 local coordinate x all 4-bit centre/dim combinations.
     * A non-zero pattern also proves that untouched neighbour nibbles remain
     * bit-identical, not merely that the requested pixels appear.
     */
    {
        u8 local_y;

        for (local_y = 0; local_y < 8u; local_y++) {
            u8 local_x;

            for (local_x = 0; local_x < 8u; local_x++) {
                u8 centre;

                for (centre = 0; centre < 16u; centre++) {
                    u8 dim;

                    for (dim = 0; dim < 16u; dim++) {
                        u8 byte_index;
                        u32 packed =
                            (u32)centre | ((u32)dim << 8);

                        for (byte_index = 0; byte_index < 32u; byte_index++) {
                            u8 value = (u8)(
                                byte_index * 73u +
                                local_x * 19u +
                                local_y * 31u +
                                centre * 7u +
                                dim * 11u
                            );

                            star_expected[byte_index] = value;
                            star_actual[byte_index] = value;
                        }
                        gameplay_overlay_plot_star_tile_c_reference(
                            star_expected,
                            local_x,
                            local_y,
                            packed
                        );
                        gameplay_overlay_plot_star_tile_asm(
                            star_actual,
                            local_x,
                            local_y,
                            packed
                        );
                        if (
                            memcmp(
                                star_expected,
                                star_actual,
                                sizeof(star_expected)
                            ) != 0
                        ) {
                            return 0;
                        }
                    }
                }
            }
        }
    }
    autotest_starfield_differential_result |= 2u;

    autotest_projectile_hint_differential_result =
        source_projectile_cache_hint_differential_test();
    if (autotest_projectile_hint_differential_result != 1u) return 0;

    autotest_pool_bit_scan_differential_result =
        source_pool_bit_scan_differential_test();
    if (autotest_pool_bit_scan_differential_result != 3u) return 0;

    autotest_sprite2_exact_differential_result =
        source_enemy_cache_find_exact_differential_test();
    if (autotest_sprite2_exact_differential_result != 1u) return 0;

    /* Exact in-place 320-byte row -> 240-byte row comparison. */
    for (index = 0; index < OT_PIC_DECODED_WIDTH; index++) {
        scale_source[index] = (u8)(index * 73u + (index >> 2) + 19u);
    }
    for (index = 0; index < SCREEN_WIDTH / 12u; index++) {
        const u32 *source = (const u32 *)(const void *)(
            scale_source + (u32)index * 16u
        );
        u32 *destination = (u32 *)(void *)(
            scale_expected + (u32)index * 12u
        );
        u32 source0 = source[0];
        u32 source1 = source[1];
        u32 source2 = source[2];
        u32 source3 = source[3];

        destination[0] =
            (source0 & 0x00ffffffu) |
            ((source1 & 0x000000ffu) << 24);
        destination[1] =
            ((source1 >> 8) & 0x0000ffffu) |
            ((source2 & 0x0000ffffu) << 16);
        destination[2] =
            ((source2 >> 16) & 0x000000ffu) |
            ((source3 & 0x00ffffffu) << 8);
    }
    frontend_scale_swar_row_asm(
        (u32 *)(void *)scale_source,
        (const u32 *)(const void *)scale_source,
        SCREEN_WIDTH / 12u
    );
    if (memcmp(scale_source, scale_expected, SCREEN_WIDTH) != 0) return 0;

    /*
     * Exercise every source byte and filter byte against a deliberately
     * non-monotonic map.  Back up and restore the live level palette map.
     */
    memcpy(scale_expected, source_enemy_palette_map, OT_PALETTE_COLOUR_COUNT);
    for (index = 0; index < OT_PALETTE_COLOUR_COUNT; index++) {
        source_enemy_palette_map[index] = (u8)(index * 73u + 19u);
    }
    for (filter = 0; filter < 256u; filter++) {
        for (index = 0; index < 256u; index++) {
            raw[0] = (u8)index;
            raw[1] = (u8)(index ^ 0x55u);
            raw[2] = (u8)(index + 0x71u);
            raw[3] = (u8)(255u - index);
            if (
                source_sprite2_l2_pack_raw_word_asm(
                    raw,
                    (u8)filter,
                    source_enemy_palette_map
                ) !=
                autotest_sprite2_pack_reference(
                    raw,
                    (u8)filter,
                    source_enemy_palette_map
                )
            ) {
                memcpy(
                    source_enemy_palette_map,
                    scale_expected,
                    OT_PALETTE_COLOUR_COUNT
                );
                return 0;
            }
        }
    }
    memcpy(
        source_enemy_palette_map,
        scale_expected,
        OT_PALETTE_COLOUR_COUNT
    );

    autotest_colour_distance_differential_result = 0;
    /*
     * Exhaust every 5-bit pair independently in R/G/B, then sweep every
     * 15-bit colour with a full-colour permutation to validate accumulation.
     * The distance is a separable sum, so this covers the complete component
     * input domain without attempting an impractical 2^30 pair loop on GBA.
     */
    for (channel = 0; channel < 3u; channel++) {
        u16 shift = (u16)(channel * 5u);

        for (left_component = 0; left_component < 32u; left_component++) {
            for (right_component = 0;
                 right_component < 32u;
                 right_component++) {
                u16 left = (u16)(left_component << shift);
                u16 right = (u16)(right_component << shift);

                if (
                    gameplay_overlay_colour_distance_asm(left, right) !=
                    gameplay_overlay_colour_distance_c_reference(left, right)
                ) {
                    return 0;
                }
            }
        }
    }
    for (colour_index = 0; colour_index < 32768u; colour_index++) {
        u16 left = (u16)colour_index;
        u16 right = (u16)(
            (colour_index * 24593u + 0x5a5au) & 0x7fffu
        );

        if (
            gameplay_overlay_colour_distance_asm(left, right) !=
            gameplay_overlay_colour_distance_c_reference(left, right)
        ) {
            return 0;
        }
    }
    autotest_colour_distance_differential_result |= 1u;
    /* Complete 5-bit colour-channel x 8-bit source-channel domain. */
    for (channel = 0; channel < 3u; channel++) {
        u16 shift = (u16)(channel * 5u);

        for (left_component = 0; left_component < 32u; left_component++) {
            for (rgb_component = 0; rgb_component < 256u; rgb_component++) {
                u16 colour = (u16)(left_component << shift);

                rgb[0] = 0;
                rgb[1] = 0;
                rgb[2] = 0;
                rgb[channel] = (u8)rgb_component;
                if (
                    source_detail_palette_distance_asm(colour, rgb) !=
                    source_detail_palette_distance_c_reference(colour, rgb)
                ) {
                    return 0;
                }
            }
        }
    }
    for (colour_index = 0; colour_index < 32768u; colour_index++) {
        u16 colour = (u16)colour_index;

        rgb[0] = (u8)(colour_index * 73u + 19u);
        rgb[1] = (u8)(colour_index * 151u + 0xa5u);
        rgb[2] = (u8)(colour_index * 199u + 0x31u);
        if (
            source_detail_palette_distance_asm(colour, rgb) !=
            source_detail_palette_distance_c_reference(colour, rgb)
        ) {
            return 0;
        }
    }
    autotest_colour_distance_differential_result |= 2u;

    /*
     * Advance one shared MT state through both implementations.  Restoring
     * the single word and cursors touched by the ARM call lets the C oracle
     * replay exactly the same step without allocating a second 2.5 KiB MT.
     */
    for (index = 0; index < 4096u; index++) {
        OtMt19937 *rng = &source_parity_level.rng;
        u16 old_p0 = rng->p0;
        u16 old_p1 = rng->p1;
        u16 old_pm = rng->pm;
        u32 old_word = rng->values[old_p0];
        u32 old_calls = source_parity_level.rng_call_count;
        u32 asm_value = ot_level_port_random(&source_parity_level);
        u16 asm_p0 = rng->p0;
        u16 asm_p1 = rng->p1;
        u16 asm_pm = rng->pm;
        u32 asm_word = rng->values[old_p0];
        u32 asm_calls = source_parity_level.rng_call_count;
        u32 reference_value;

        rng->p0 = old_p0;
        rng->p1 = old_p1;
        rng->pm = old_pm;
        rng->values[old_p0] = old_word;
        source_parity_level.rng_call_count = old_calls;
        reference_value = autotest_mt_rand_reference(&source_parity_level);
        if (
            asm_value != reference_value ||
            asm_p0 != rng->p0 ||
            asm_p1 != rng->p1 ||
            asm_pm != rng->pm ||
            asm_word != rng->values[old_p0] ||
            asm_calls != source_parity_level.rng_call_count
        ) {
            return 0;
        }
    }
    autotest_level_port_hotpath_differential_result =
        ot_level_port_hotpath_asm_differential_test(
            &source_parity_level
        );
    if (autotest_level_port_hotpath_differential_result != 3u) {
        return 0;
    }
    return 1;
}

static void autotest_hotpath_asm_benchmark(void)
{
    enum { AUTOTEST_HOTPATH_BENCHMARK_CALLS = 16384 };
    u8 rgb[3] = { 0, 0, 0 };
    u8 star_c[32] = { 0 };
    u8 star_asm[32] = { 0 };
    u16 old_ime = REG_IME;
    u32 start;
    u32 index;
    u32 sink = 0;

    REG_IME = 0;
    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= gameplay_overlay_colour_distance_c_reference(
            (u16)index,
            (u16)((index * 24593u + 0x5a5au) & 0x7fffu)
        );
    }
    autotest_overlay_distance_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= gameplay_overlay_colour_distance_asm(
            (u16)index,
            (u16)((index * 24593u + 0x5a5au) & 0x7fffu)
        );
    }
    autotest_overlay_distance_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        rgb[0] = (u8)(index * 73u + 19u);
        rgb[1] = (u8)(index * 151u + 0xa5u);
        rgb[2] = (u8)(index * 199u + 0x31u);
        sink ^= source_detail_palette_distance_c_reference(
            (u16)index,
            rgb
        );
    }
    autotest_palette_distance_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        rgb[0] = (u8)(index * 73u + 19u);
        rgb[1] = (u8)(index * 151u + 0xa5u);
        rgb[2] = (u8)(index * 199u + 0x31u);
        sink ^= source_detail_palette_distance_asm((u16)index, rgb);
    }
    autotest_palette_distance_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= ot_player_shot_axis_overlaps_c_reference(
            (int16_t)(index * 4051u),
            (uint16_t)((index & 255u) + 1u)
        );
    }
    autotest_axis_overlap_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= AUTOTEST_AXIS_OVERLAPS_ASM(
            (int16_t)(index * 4051u),
            (uint16_t)((index & 255u) + 1u)
        );
    }
    autotest_axis_overlap_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= source_pool_lowest_set_bit_c_reference(
            (u32)(index * 0x9e3779b9u) ^ 0xa5a55a5au
        );
    }
    autotest_pool_lowest_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= source_pool_lowest_set_bit_asm(
            (u32)(index * 0x9e3779b9u) ^ 0xa5a55a5au
        );
    }
    autotest_pool_lowest_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= source_pool_highest_set_bit_c_reference(
            (u32)(index * 0x9e3779b9u) ^ 0xa5a55a5au
        );
    }
    autotest_pool_highest_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= source_pool_highest_set_bit_asm(
            (u32)(index * 0x9e3779b9u) ^ 0xa5a55a5au
        );
    }
    autotest_pool_highest_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= gameplay_overlay_divmod320_c_reference(
            (u16)(index * 4051u)
        );
    }
    autotest_starfield_divmod_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        sink ^= gameplay_overlay_divmod320_asm(
            (u16)(index * 4051u)
        );
    }
    autotest_starfield_divmod_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        u8 local_x = (u8)(index & 7u);
        u8 local_y = (u8)((index >> 3) & 7u);
        u32 packed =
            (u32)(index & 15u) |
            ((u32)((index * 7u) & 15u) << 8);

        gameplay_overlay_plot_star_tile_c_reference(
            star_c,
            local_x,
            local_y,
            packed
        );
    }
    autotest_starfield_plot_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_HOTPATH_BENCHMARK_CALLS; index++) {
        u8 local_x = (u8)(index & 7u);
        u8 local_y = (u8)((index >> 3) & 7u);
        u32 packed =
            (u32)(index & 15u) |
            ((u32)((index * 7u) & 15u) << 8);

        gameplay_overlay_plot_star_tile_asm(
            star_asm,
            local_x,
            local_y,
            packed
        );
    }
    autotest_starfield_plot_asm_cycles =
        stress_cycle_counter_read() - start;
    for (index = 0; index < sizeof(star_c); index++) {
        sink += (u32)star_c[index] + (u32)star_asm[index];
    }

    {
        const SourceEnemyCacheSlot *result;
        u8 slot_index;

        memset(
            source_enemy_cache_lookup_test_slots,
            0,
            sizeof(source_enemy_cache_lookup_test_slots)
        );
        for (slot_index = 0; slot_index < 4u; slot_index++) {
            SourceEnemyCacheSlot *slot =
                &source_enemy_cache_lookup_test_slots[slot_index];
            u16 graphic = (u16)(0x5a31u + slot_index * 0x111u);
            u8 shape_table = (u8)(7u + slot_index * 3u);
            u8 size = (u8)(slot_index & 1u);
            u8 filter = (u8)(0x2bu + slot_index * 0x11u);

            slot->graphic = graphic;
            slot->shape_table = shape_table;
            slot->size = size;
            slot->filter = filter;
            slot->valid = 1;
            source_enemy_cache_lookup_benchmark_keys[slot_index] =
                source_enemy_cache_packed_key(
                    shape_table,
                    graphic,
                    size
                );
            source_enemy_cache_lookup_benchmark_filters[slot_index] =
                filter;
        }

        start = stress_cycle_counter_read();
        for (
            index = 0;
            index < AUTOTEST_HOTPATH_BENCHMARK_CALLS;
            index++
        ) {
            slot_index = (u8)(index & 3u);
            result = source_enemy_cache_find_exact_c_reference(
                source_enemy_cache_lookup_test_slots,
                4,
                source_enemy_cache_lookup_benchmark_keys[slot_index],
                source_enemy_cache_lookup_benchmark_filters[slot_index]
            );
            sink += (u32)(result - source_enemy_cache_lookup_test_slots);
        }
        autotest_sprite2_exact_c_cycles =
            stress_cycle_counter_read() - start;

        start = stress_cycle_counter_read();
        for (
            index = 0;
            index < AUTOTEST_HOTPATH_BENCHMARK_CALLS;
            index++
        ) {
            slot_index = (u8)(index & 3u);
            result = source_enemy_cache_find_exact_asm(
                source_enemy_cache_lookup_test_slots,
                4,
                source_enemy_cache_lookup_benchmark_keys[slot_index],
                source_enemy_cache_lookup_benchmark_filters[slot_index]
            );
            sink += (u32)(result - source_enemy_cache_lookup_test_slots);
        }
        autotest_sprite2_exact_asm_cycles =
            stress_cycle_counter_read() - start;
    }

    {
        static const u16 graphics[4] = { 1u, 2u, 3u, 4u };
        static const u8 shape_tables[4] = { 4u, 4u, 5u, 5u };
        const SourceProjectileCacheSlot *result;
        u8 slot_index;

        memset(
            source_projectile_hint_test_slots,
            0,
            sizeof(source_projectile_hint_test_slots)
        );
        memset(
            source_projectile_hint_test_directory,
            0,
            sizeof(source_projectile_hint_test_directory)
        );
        for (slot_index = 0; slot_index < 4u; slot_index++) {
            SourceProjectileCacheSlot *slot =
                &source_projectile_hint_test_slots[slot_index];
            u32 packed_key = source_projectile_cache_packed_key(
                shape_tables[slot_index],
                graphics[slot_index]
            );

            slot->graphic = graphics[slot_index];
            slot->shape_table = shape_tables[slot_index];
            slot->valid = 1;
            source_projectile_hint_benchmark_keys[slot_index] =
                packed_key;
            source_projectile_hint_test_directory[
                source_projectile_cache_hint_hash(packed_key)
            ] = (u8)(slot_index + 1u);
        }

        start = stress_cycle_counter_read();
        for (
            index = 0;
            index < AUTOTEST_HOTPATH_BENCHMARK_CALLS;
            index++
        ) {
            slot_index = (u8)(index & 3u);
            result = source_projectile_cache_find_hint_c_reference(
                source_projectile_hint_test_directory,
                source_projectile_hint_test_slots,
                source_projectile_hint_benchmark_keys[slot_index],
                4u
            );
            sink += (u32)(result - source_projectile_hint_test_slots);
        }
        autotest_projectile_hint_c_cycles =
            stress_cycle_counter_read() - start;

        start = stress_cycle_counter_read();
        for (
            index = 0;
            index < AUTOTEST_HOTPATH_BENCHMARK_CALLS;
            index++
        ) {
            slot_index = (u8)(index & 3u);
            result = source_projectile_cache_find_hint_asm(
                source_projectile_hint_test_directory,
                source_projectile_hint_test_slots,
                source_projectile_hint_benchmark_keys[slot_index],
                4u
            );
            sink += (u32)(result - source_projectile_hint_test_slots);
        }
        autotest_projectile_hint_asm_cycles =
            stress_cycle_counter_read() - start;
    }
    REG_IME = old_ime;
    autotest_hotpath_benchmark_sink = sink;
}

static void autotest_detail_palette_prepare_inputs(
    u8 *palette,
    u8 *bg_source,
    u8 *obj_source,
    u16 *obj_base
)
{
    u16 index;

    for (index = 0; index < 768u; index++) {
        palette[index] = (u8)(index * 73u + (index >> 2) + 19u);
    }
    for (index = 0; index < 256u; index++) {
        bg_source[index] = (u8)(index * 151u + 0xa5u);
        obj_source[index] = (u8)(index * 199u + 0x31u);
        obj_base[index] = (u16)(
            (index * 24593u + 0x5a5au) & 0x7fffu
        );
    }
}

static u32 autotest_detail_effect_asm_differential_test(void)
{
    u8 *scratch = gameplay_overlay_tiles;
    u8 *palette = scratch;
    u8 *bg_source = scratch + 768u;
    u8 *obj_source = scratch + 1024u;
    u16 *obj_base = (u16 *)(void *)(scratch + 1280u);
    u16 *expected_bg = (u16 *)(void *)(scratch + 1792u);
    u16 *expected_obj = (u16 *)(void *)(scratch + 2304u);
    static const s16 spotlight_centres[] = {
        -64, -1, 0, 1, 119, 120, 239, 240, 304,
    };
    static const s16 spotlight_apices[] = {
        -32, 0, 1, 80, 159, 160, 240,
    };
    u32 result = 0;
    s16 brightness;
    u16 index;
    u8 effect_kind;
    u8 filter_objects;
    u8 hue;

    autotest_detail_palette_prepare_inputs(
        palette,
        bg_source,
        obj_source,
        obj_base
    );
    /* Every brightness value, effect kind and OBJ policy; hue is permuted. */
    for (effect_kind = SOURCE_DETAIL_EFFECT_NONE;
         effect_kind <= SOURCE_DETAIL_EFFECT_WATER_HUE;
         effect_kind++) {
        for (brightness = -15; brightness <= 15; brightness++) {
            hue = (u8)(brightness * 11 + effect_kind * 5) & 0x0fu;
            for (filter_objects = 0; filter_objects <= 1; filter_objects++) {
                source_detail_build_effect_palette_c_reference(
                    palette,
                    bg_source,
                    obj_source,
                    obj_base,
                    expected_bg,
                    expected_obj,
                    effect_kind,
                    hue,
                    (s8)brightness,
                    filter_objects
                );
                source_detail_build_effect_palette_asm(
                    palette,
                    bg_source,
                    obj_source,
                    obj_base,
                    source_detail_effect_bg_palette,
                    source_detail_effect_obj_palette,
                    effect_kind,
                    hue,
                    (s8)brightness,
                    filter_objects
                );
                if (
                    memcmp(
                        expected_bg,
                        source_detail_effect_bg_palette,
                        256u * sizeof(u16)
                    ) != 0 ||
                    memcmp(
                        expected_obj,
                        source_detail_effect_obj_palette,
                        256u * sizeof(u16)
                    ) != 0
                ) {
                    return result;
                }
            }
        }
    }
    /* Sweep all replacement hues independently for both hue-changing modes. */
    for (effect_kind = SOURCE_DETAIL_EFFECT_GLOBAL_HUE;
         effect_kind <= SOURCE_DETAIL_EFFECT_WATER_HUE;
         effect_kind++) {
        for (hue = 0; hue < 16u; hue++) {
            source_detail_build_effect_palette_c_reference(
                palette, bg_source, obj_source, obj_base,
                expected_bg, expected_obj,
                effect_kind, hue, -7, 1
            );
            source_detail_build_effect_palette_asm(
                palette, bg_source, obj_source, obj_base,
                source_detail_effect_bg_palette,
                source_detail_effect_obj_palette,
                effect_kind, hue, -7, 1
            );
            if (
                memcmp(
                    expected_bg,
                    source_detail_effect_bg_palette,
                    256u * sizeof(u16)
                ) != 0 ||
                memcmp(
                    expected_obj,
                    source_detail_effect_obj_palette,
                    256u * sizeof(u16)
                ) != 0
            ) {
                return result;
            }
        }
    }
    result |= 1u;

    for (index = 0;
         index < sizeof(spotlight_centres) / sizeof(spotlight_centres[0]);
         index++) {
        u16 apex_index;
        u16 *expected = (u16 *)(void *)scratch;
        u16 *actual = (u16 *)(void *)(scratch + 512u);

        for (apex_index = 0;
             apex_index <
                sizeof(spotlight_apices) / sizeof(spotlight_apices[0]);
             apex_index++) {
            source_detail_spotlight_prepare_table_c_reference(
                expected,
                spotlight_centres[index],
                spotlight_apices[apex_index]
            );
            source_detail_spotlight_prepare_table_asm(
                actual,
                spotlight_centres[index],
                spotlight_apices[apex_index]
            );
            if (
                memcmp(
                    expected,
                    actual,
                    SOURCE_DETAIL_SPOTLIGHT_SCANLINES * sizeof(u16)
                ) != 0
            ) {
                return result;
            }
        }
    }
    result |= 2u;

#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    {
        static const u16 strengths[] = { 0, 1, 8, 159, 160, 255, 256 };
        static const u32 bg0_pairs[] = {
            0x00000000u, 0x01ff0001u, 0xffff01ffu,
        };
        static const u32 bg1_pairs[] = {
            0x00000000u, 0x00a500ffu, 0xffff0000u,
        };
        s8 *profile = (s8 *)(void *)scratch;
        u16 *expected = (u16 *)(void *)(scratch + 256u);
        u16 *actual = (u16 *)(void *)(scratch + 1600u);
        u16 strength_index;
        u16 pair_index;

        for (index = 0; index < SOURCE_DETAIL_WAVE_SCANLINES; index++) {
            profile[index] = (s8)((index % 7u) - 3);
        }
        for (strength_index = 0;
             strength_index < sizeof(strengths) / sizeof(strengths[0]);
             strength_index++) {
            for (pair_index = 0;
                 pair_index < sizeof(bg0_pairs) / sizeof(bg0_pairs[0]);
                 pair_index++) {
                source_detail_wave_prepare_table_c_reference(
                    expected,
                    profile,
                    strengths[strength_index],
                    bg0_pairs[pair_index],
                    bg1_pairs[pair_index]
                );
                source_detail_wave_prepare_table_asm(
                    actual,
                    profile,
                    strengths[strength_index],
                    bg0_pairs[pair_index],
                    bg1_pairs[pair_index]
                );
                if (
                    memcmp(
                        expected,
                        actual,
                        SOURCE_DETAIL_WAVE_SCANLINES * 4u * sizeof(u16)
                    ) != 0
                ) {
                    return result;
                }
            }
        }
        result |= 4u;
    }
#endif
    return result;
}

static void autotest_detail_effect_asm_benchmark(void)
{
    enum {
        AUTOTEST_DETAIL_PALETTE_CALLS = 64,
        AUTOTEST_DETAIL_SCANLINE_CALLS = 512,
    };
    u8 *scratch = gameplay_overlay_tiles;
    u8 *palette = scratch;
    u8 *bg_source = scratch + 768u;
    u8 *obj_source = scratch + 1024u;
    u16 *obj_base = (u16 *)(void *)(scratch + 1280u);
    u16 *palette_bg = (u16 *)(void *)(scratch + 1792u);
    u16 *palette_obj = (u16 *)(void *)(scratch + 2304u);
    u16 *scanline_output = (u16 *)(void *)(scratch + 2816u);
    u16 old_ime = REG_IME;
    u32 start;
    u32 sink = 0;
    u32 index;

    autotest_detail_palette_prepare_inputs(
        palette,
        bg_source,
        obj_source,
        obj_base
    );
    REG_IME = 0;
    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_PALETTE_CALLS; index++) {
        source_detail_build_effect_palette_c_reference(
            palette, bg_source, obj_source, obj_base,
            palette_bg, palette_obj,
            (u8)(index % 3u),
            (u8)(index & 15u),
            (s8)((s32)(index % 31u) - 15),
            (u8)(index & 1u)
        );
        sink ^= palette_bg[index & 255u];
        sink ^= palette_obj[(index * 13u) & 255u];
    }
    autotest_detail_palette_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_PALETTE_CALLS; index++) {
        source_detail_build_effect_palette_asm(
            palette, bg_source, obj_source, obj_base,
            source_detail_effect_bg_palette,
            source_detail_effect_obj_palette,
            (u8)(index % 3u),
            (u8)(index & 15u),
            (s8)((s32)(index % 31u) - 15),
            (u8)(index & 1u)
        );
        sink ^= source_detail_effect_bg_palette[index & 255u];
        sink ^= source_detail_effect_obj_palette[(index * 13u) & 255u];
    }
    autotest_detail_palette_asm_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_SCANLINE_CALLS; index++) {
        source_detail_spotlight_prepare_table_c_reference(
            scanline_output,
            (s16)((index * 37u) % 369u - 64),
            (s16)((index * 53u) % 273u - 32)
        );
        sink ^= scanline_output[index % SOURCE_DETAIL_SPOTLIGHT_SCANLINES];
    }
    autotest_detail_spotlight_c_cycles =
        stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_SCANLINE_CALLS; index++) {
        source_detail_spotlight_prepare_table_asm(
            scanline_output,
            (s16)((index * 37u) % 369u - 64),
            (s16)((index * 53u) % 273u - 32)
        );
        sink ^= scanline_output[index % SOURCE_DETAIL_SPOTLIGHT_SCANLINES];
    }
    autotest_detail_spotlight_asm_cycles =
        stress_cycle_counter_read() - start;

#if TYRIAN_GBA_DETAIL_HAS_LAVA_WATER
    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_SCANLINE_CALLS; index++) {
        source_detail_wave_prepare_table_c_reference(
            scanline_output,
            source_detail_wave_profile[index & 1u],
            index % 257u,
            0x01230045u,
            0x02340067u
        );
        sink ^= scanline_output[
            (index % SOURCE_DETAIL_WAVE_SCANLINES) * 4u
        ];
    }
    autotest_detail_wave_c_cycles = stress_cycle_counter_read() - start;

    start = stress_cycle_counter_read();
    for (index = 0; index < AUTOTEST_DETAIL_SCANLINE_CALLS; index++) {
        source_detail_wave_prepare_table_asm(
            scanline_output,
            source_detail_wave_profile[index & 1u],
            index % 257u,
            0x01230045u,
            0x02340067u
        );
        sink ^= scanline_output[
            (index % SOURCE_DETAIL_WAVE_SCANLINES) * 4u
        ];
    }
    autotest_detail_wave_asm_cycles = stress_cycle_counter_read() - start;
#else
    autotest_detail_wave_c_cycles = 0;
    autotest_detail_wave_asm_cycles = 0;
#endif
    REG_IME = old_ime;
    autotest_detail_effect_benchmark_sink = sink;
}

static u8 autotest_detail_adapter_self_test(void)
{
    u8 pass = 1;

    memset(
        source_parity_level.smoothies,
        0,
        sizeof(source_parity_level.smoothies)
    );
    memset(
        source_parity_level.smoothie_data,
        0,
        sizeof(source_parity_level.smoothie_data)
    );
    source_parity_level.filter_active = false;
    source_parity_level.level_filter = -99;
    source_parity_level.level_brightness = -99;
    source_parity_level.background2_over = 0;
    source_parity_level.background2_not_transparent = false;

#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
    source_parity_level.smoothies[0] = 1;
    source_parity_level.smoothies[2] = 1;
    source_parity_level.smoothies[3] = 1;
    source_detail_prepare_scene();
    pass = (u8)(
        !source_detail_iced_requested &&
        !source_detail_blur_requested &&
        !source_detail_wave_requested &&
        !source_detail_wild_active &&
        !source_detail_palette_mode_pending
    );
#elif TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_NORMAL
    source_parity_level.smoothies[2] = 1;
    source_parity_level.smoothies[3] = 1;
    source_parity_level.smoothies[5] = 1;
    source_detail_prepare_scene();
    gameplay_presentation_capture_registers();
    pass = (u8)(
        source_detail_spotlight_requested &&
        source_detail_iced_requested &&
        source_detail_blur_requested &&
        !source_detail_wave_requested &&
        !source_detail_wild_active &&
        source_detail_palette_mode_pending &&
        gameplay_presentation.mosaic == 0 &&
        (gameplay_presentation.bg0cnt & BG_MOSAIC) == 0 &&
        (gameplay_presentation.bg1cnt & BG_MOSAIC) == 0 &&
        (gameplay_presentation.bg2cnt & BG_MOSAIC) == 0 &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_GLOBAL_HUE &&
        source_detail_effect_hue_pending == 8u
    );
#elif TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_HIGH
    source_parity_level.smoothies[0] = 1;
    source_parity_level.smoothie_data[0] = 0;
    source_parity_level.smoothies[1] = 1;
    source_parity_level.smoothie_data[1] = 13;
    source_detail_prepare_scene();
    pass = (u8)(
        source_detail_wave_requested &&
        !source_detail_wild_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_WATER_HUE &&
        source_detail_effect_hue_pending == 13u
    );
#elif TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
    source_parity_level.smoothies[1] = 1;
    source_parity_level.smoothie_data[1] = 13;
    source_parity_level.filter_active = true;
    source_parity_level.level_filter = 6;
    source_parity_level.level_brightness = -3;
    source_detail_prepare_scene();
    pass = (u8)(
        source_detail_wave_requested &&
        source_detail_wild_active &&
        !source_detail_wild_dither_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_GLOBAL_HUE &&
        source_detail_effect_hue_pending == 6u &&
        source_detail_effect_brightness_pending == -3 &&
        source_detail_effect_objects_pending &&
        source_detail_filtered_source_index(
            0x35u,
            SOURCE_DETAIL_EFFECT_WATER_HUE,
            13u,
            0
        ) == 0xd5u &&
        source_detail_filtered_source_index(
            0x85u,
            SOURCE_DETAIL_EFFECT_WATER_HUE,
            13u,
            0
        ) == 0x85u
    );
    /* Event 48 must switch already-visible BG2 tiles back to opaque. */
    source_parity_level.background2_not_transparent = true;
    source_detail_prepare_scene();
    pass = (u8)(
        pass &&
        !source_detail_wild_active &&
        !source_detail_wild_dither_active
    );
#else
    /*
     * Custom is Normal + Pentium wild/final filtration only.  A level may
     * still author water/lava smoothies, but neither their wave nor their
     * hue pass is allowed into this profile.  Source special-code 2 is also
     * intentionally ignored, so its triangular spotlight cannot pre-empt
     * the selected CUSTOM colour-effects path.
     */
    source_parity_level.smoothies[0] = 1;
    source_parity_level.smoothie_data[0] = 0;
    source_parity_level.smoothies[1] = 1;
    source_parity_level.smoothie_data[1] = 13;
    source_parity_level.smoothies[5] = 1;
    source_parity_level.filter_active = true;
    source_parity_level.level_filter = 6;
    source_parity_level.level_brightness = -3;
    source_detail_prepare_scene();
    pass = (u8)(
        !source_detail_spotlight_requested &&
        !source_detail_wave_requested &&
        source_detail_wild_active &&
        !source_detail_wild_dither_active &&
        source_detail_palette_mode_pending &&
        source_detail_effect_kind_pending ==
            SOURCE_DETAIL_EFFECT_GLOBAL_HUE &&
        source_detail_effect_hue_pending == 6u &&
        source_detail_effect_brightness_pending == -3 &&
        source_detail_effect_objects_pending
    );
    source_parity_level.background2_not_transparent = true;
    source_detail_prepare_scene();
    pass = (u8)(
        pass &&
        !source_detail_wave_requested &&
        !source_detail_wild_active &&
        !source_detail_wild_dither_active
    );
#endif
    return pass;
}

static void autotest_full_loadout_stress_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u32 rng_benchmark_start;
    u32 rng_benchmark_elapsed;
    u32 rng_benchmark_sink = 0;
    u16 rng_benchmark_ime;
    u16 rng_benchmark_index;
    u8 active_shots = 0;
    u8 index;
    u8 background2_enabled_snapshot;
    u8 background2_wrap_active_snapshot;
    u8 background2_present_snapshot;
    u8 background2_upload_pending_snapshot;
    u16 back_move_snapshot;
    u16 back_move2_snapshot;
    u8 bg1_scroll_speed_snapshot;
    u8 bg2_scroll_speed_snapshot;
    u16 bg1_scroll_pixel_snapshot;
    u16 bg2_scroll_pixel_snapshot;
    s8 camera_offset_x_snapshot;
    s8 camera_offset_y_snapshot;
    u16 map_x_offset_snapshot;
    u16 map_x2_offset_snapshot;
    u16 bg1_hofs_snapshot;
    u16 bg2_hofs_snapshot;
    u16 bg1_presentation_scroll_snapshot;
    u16 bg2_presentation_scroll_snapshot;
    u16 bg1_camera_scroll_snapshot;
    u16 bg2_camera_scroll_snapshot;
    u16 bg1_vofs_snapshot;
    u16 bg2_vofs_snapshot;
    u8 background2_over_snapshot;
    u8 background3_over_snapshot;
    u8 definition_130_count_snapshot = 0;
    u8 definition_130_pool_mask_snapshot = 0;
    u8 definition_130_ground_count_snapshot = 0;
    u8 definition_132_count_snapshot = 0;
    u8 definition_132_pool_mask_snapshot = 0;
    u8 definition_132_ground_count_snapshot = 0;

    /* Capture an in-progress Boss window at an exact stress stop point. */
    source_boss_perf_capture_interval();
    background2_enabled_snapshot = source_background2_enabled;
    background2_wrap_active_snapshot = source_background2_wrap_active;
    background2_present_snapshot = source_background2_present();
    background2_upload_pending_snapshot =
        source_background2_upload_pending;
    back_move_snapshot = source_parity_level.back_move;
    back_move2_snapshot = source_parity_level.back_move2;
    bg1_scroll_speed_snapshot = bg1_scroll_speed;
    bg2_scroll_speed_snapshot = bg2_scroll_speed;
    bg1_scroll_pixel_snapshot = bg1_scroll_pixel;
    bg2_scroll_pixel_snapshot = bg2_scroll_pixel;
    camera_offset_x_snapshot = source_camera_offset_x;
    camera_offset_y_snapshot = source_camera_offset_y;
    map_x_offset_snapshot = source_parity_level.presentation_map_x_offset;
    map_x2_offset_snapshot = source_parity_level.presentation_map_x2_offset;
    bg1_hofs_snapshot = source_background_hofs(0);
    bg2_hofs_snapshot = source_background_hofs(1);
    bg1_presentation_scroll_snapshot = bg1_presentation_scroll_pixel;
    bg2_presentation_scroll_snapshot = bg2_presentation_scroll_pixel;
    bg1_camera_scroll_snapshot = background_presentation_scroll_pixel(
        0,
        source_camera_presentation_offset_y
    );
    bg2_camera_scroll_snapshot = background_presentation_scroll_pixel(
        1,
        source_camera_presentation_offset_y
    );
    bg1_vofs_snapshot = source_background_vofs(0);
    bg2_vofs_snapshot = source_background_vofs(1);
    background2_over_snapshot = source_parity_level.background2_over;
    background3_over_snapshot = source_parity_level.background3_over;
    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[index];
        u8 pool_bit;

        if (source_parity_level.enemy_avail[index] == 1) continue;
        pool_bit = (u8)(1u << (index / OT_ENEMY_POOL_SIZE));
        if (enemy->enemy_definition_id == 130u) {
            definition_130_count_snapshot++;
            definition_130_pool_mask_snapshot |= pool_bit;
            if (enemy->enemyground) {
                definition_130_ground_count_snapshot++;
            }
        } else if (enemy->enemy_definition_id == 132u) {
            definition_132_count_snapshot++;
            definition_132_pool_mask_snapshot |= pool_bit;
            if (enemy->enemyground) {
                definition_132_ground_count_snapshot++;
            }
        }
    }

    /*
     * The packed-collision differential test below deliberately reuses and
     * clears source_parity_level.  Preserve gameplay-only enemy-directory
     * telemetry before running any self-test so SRAM reports the measured
     * route rather than the synthetic test fixture's final state.
     */
    autotest_enemy_pool_consistency_snapshot =
        autotest_enemy_pool_mask_consistency();
    autotest_enemy_pool_active_visits_snapshot =
        source_parity_level.enemy_pool_active_visits;
    autotest_enemy_pool_linear_visits_snapshot =
        source_parity_level.enemy_pool_linear_visits;
    autotest_enemy_allocator_mask_probes_snapshot =
        source_parity_level.enemy_allocator_mask_word_probes;
    autotest_enemy_allocator_slot_probes_snapshot =
        source_parity_level.enemy_allocator_slot_probes;
    autotest_collision_hit_apply_calls_snapshot =
        source_parity_level.collision_hit_apply_calls;
    autotest_collision_status_link_visits_snapshot =
        source_parity_level.collision_status_link_visits;
    autotest_collision_kill_group_visits_snapshot =
        source_parity_level.collision_kill_group_visits;
    autotest_collision_damaged_transition_visits_snapshot =
        source_parity_level.collision_damaged_transition_visits;
    autotest_collision_player_contact_visits_snapshot =
        source_parity_level.collision_player_contact_visits;
    autotest_collision_zinglon_visits_snapshot =
        source_parity_level.collision_zinglon_visits;
    autotest_boss_manifest_spawn_count_snapshot =
        ot_level_port_boss_manifest_spawn_count(&source_parity_level);
    autotest_boss_manifest_active_count_snapshot =
        ot_level_port_boss_manifest_active_count(&source_parity_level);
    autotest_boss_manifest_episode_snapshot =
        ot_level_port_boss_manifest_episode(&source_parity_level);
    autotest_boss_manifest_lvl_snapshot =
        ot_level_port_boss_manifest_lvl_file_number(&source_parity_level);

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active_shots++;
    }
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'W';
    sram[3] = '8';
    sram[4] = 1;
#if TYRIAN_GBA_STRESS_LOADOUT
    sram[5] =
        stress_loadout_valid &&
        telemetry_stress_loadout_failures == 0;
#else
    sram[5] =
        source_parity_level.assets_valid &&
        telemetry_stress_loadout_failures == 0;
#endif
    sram[6] = game_state;
    sram[7] = mmActive() ? 1 : 0;
    sram_write_u32(8, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(12, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(16, telemetry_display_frames);
    sram_write_u32(20, telemetry_logic_updates);
    sram_write_u32(24, telemetry_missed_vblanks);
    sram_write_u32(28, telemetry_max_oam);
    sram_write_u32(32, telemetry_player_shot_spawns);
    sram_write_u32(36, telemetry_player_shot_drops);
    sram_write_u32(40, telemetry_player_shot_max_active);
    sram_write_u32(44, telemetry_player_chain_volleys);
    sram_write_u32(48, telemetry_projectile_cache_drops);
    sram_write_u32(
        52,
        telemetry_projectile_cache_max_visible_unique
    );
    sram_write_u32(56, telemetry_sprite2_cache_drops);
    sram_write_u32(60, telemetry_sprite2_max_visible_unique);
    sram_write_u32(64, telemetry_sprite2_l2_drops);
    sram_write_u32(68, telemetry_effect_cache_drops);
    sram_write_u32(72, telemetry_pickup_explosion_drops);
    sram_write_u32(76, telemetry_pickup_explosion_max_active);
    sram_write_u32(80, telemetry_source_max_visible_enemies);
    sram_write_u32(84, telemetry_source_unknown_visuals);
    sram_write_u32(88, background_cache_approximations);
    sram_write_u32(92, telemetry_stress_loadout_failures);
    sram_write_u32(96, telemetry_vblank_irqs);
    sram_write_u32(100, level_position);
    sram_write_u32(104, active_shots);
    sram_write_u32(108, SOURCE_PROJECTILE_CACHE_SLOT_COUNT);
    sram_write_u32(112, SOURCE_ENEMY_CACHE_SLOT_COUNT);
    sram_write_u32(116, MAX_PLAYER_SHOTS);
    sram_write_u32(120, source_parity_level.assets_valid);
    sram_write_u32(128, telemetry_stress_option_blend_draws);
    sram_write_u32(132, source_option_projectile_valid);
    sram_write_u32(136, telemetry_detail_lava_frames);
    sram_write_u32(140, telemetry_detail_water_frames);
    sram_write_u32(144, telemetry_detail_iced_frames);
    sram_write_u32(148, telemetry_detail_blur_frames);
    sram_write_u32(152, telemetry_detail_wild_frames);
    sram_write_u32(156, telemetry_stress_psg_triggers);
    sram_write_u32(
        160,
        telemetry_projectile_culled_offscreen_before_cache
    );
    sram_write_u32(
        164,
        telemetry_projectile_culled_oam_full_before_cache
    );
    sram_write_u32(
        168,
        telemetry_projectile_post_visibility_acquires
    );
    sram_write_u32(
        172,
        telemetry_projectile_visible_capacity_drops
    );
    sram_write_u32(176, telemetry_stress_logic_cycles_total);
    sram_write_u32(180, telemetry_stress_logic_cycles_max);
    sram_write_u32(184, telemetry_stress_render_cycles_total);
    sram_write_u32(188, telemetry_stress_render_cycles_max);
    sram_write_u32(192, telemetry_stress_collision_cycles_total);
    sram_write_u32(196, telemetry_stress_collision_cycles_max);
    sram_write_u32(
        200,
        TYRIAN_GBA_PROJECTILE_PRECACHE_CULL |
            (TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION << 1) |
            (
                TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER <<
                2
            ) |
            (TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK << 3) |
            (TYRIAN_GBA_DYNAMIC_FRAME_DROP << 4) |
            (TYRIAN_GBA_WALL_CLOCK_LOGIC << 5) |
            (TYRIAN_GBA_COLLISION_UNSIGNED_RANGE << 6) |
            (TYRIAN_GBA_COLLISION_MASK_FAST_PATH << 7) |
            (TYRIAN_GBA_PRESENTATION_DEFER << 8) |
            (TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER << 9) |
            (TYRIAN_GBA_COLLISION_LAZY_RESULT << 10) |
            (TYRIAN_GBA_COLLISION_PACKED_CALL << 11) |
            (TYRIAN_GBA_RECOVER_MISSED_VBLANK << 12) |
            (TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH << 13) |
            (TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH << 14)
    );
    sram_write_u32(204, telemetry_projectile_cache_hits);
    sram_write_u32(208, telemetry_projectile_cache_misses);
    sram_write_u32(212, telemetry_projectile_cache_evictions);
    sram_write_u32(216, telemetry_projectile_cache_uploads);
    sram_write_u32(
        220,
        source_parity_level.player_shot_collision_mask_rebuild_count
    );
    sram_write_u32(
        224,
        source_parity_level.player_shot_collision_candidate_visit_count
    );
    sram_write_u32(
        228,
        source_parity_level.player_shot_collision_linear_slot_visit_count
    );
    sram_write_u32(
        232,
        source_parity_level.player_shot_collision_mask_active
    );
    sram_write_u32(236, telemetry_perf_vblank_irq_cycles_total);
    sram_write_u32(240, telemetry_perf_vblank_irq_cycles_max);
    sram_write_u32(244, telemetry_perf_commit_cycles_total);
    sram_write_u32(248, telemetry_perf_commit_cycles_max);
    sram_write_u32(252, telemetry_perf_audio_input_cycles_total);
    sram_write_u32(256, telemetry_perf_audio_input_cycles_max);
    sram_write_u32(260, telemetry_perf_prelogic_cycles_total);
    sram_write_u32(264, telemetry_perf_prelogic_cycles_max);
    sram_write_u32(268, telemetry_wall_vblanks);
    sram_write_u32(
        272,
        telemetry_presentation_render_attempts
    );
    sram_write_u32(
        276,
        telemetry_presentation_render_completed
    );
    sram_write_u32(
        280,
        telemetry_presentation_render_deferred
    );
    sram_write_u32(
        284,
        telemetry_presentation_render_forced
    );
    sram_write_u32(288, telemetry_presentation_superseded);
    sram_write_u32(
        292,
        telemetry_presentation_pending_logic_max
    );
    sram_write_u32(
        296,
        telemetry_presentation_estimate_max
    );
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    sram_write_u32(300, presentation_render_estimate);
    sram_write_u32(324, presentation_render_pending);
#else
    sram_write_u32(300, 0);
    sram_write_u32(324, 0);
#endif
    sram_write_u32(
        304,
        telemetry_presentation_deadline_elapsed_max
    );
    sram_write_u32(308, telemetry_logic_catchup_updates);
    sram_write_u32(
        312,
        telemetry_logic_updates_per_loop_max
    );
    sram_write_u32(316, telemetry_logic_backlog_frames_max);
    sram_write_u32(320, telemetry_background_held_rows_max);
    sram_write_u32(328, telemetry_vblank_recovery_loops);
    sram_write_u32(332, telemetry_audio_frames);
    sram_write_u32(336, source_parity_level.rng_call_count);
    sram_write_u32(
        340,
        source_parity_level.enemy_motion_update_count
    );
    sram_write_u32(
        344,
        source_parity_level.enemy_shot_motion_update_count
    );
    sram_write_u32(
        348,
        ot_level_port_stress_round_ratio_call_count()
    );
    sram_write_u32(
        352,
        source_parity_level.enemy_shot_trigger_count
    );
    sram_write_u32(
        356,
        source_parity_level.enemy_launch_success_count
    );
    /*
     * Measure the generator itself after all gameplay counters have been
     * captured.  IRQs are masked so an audio/VBlank interrupt cannot turn
     * this into an interrupt-latency benchmark; Timer 2/3 keeps counting.
     */
    rng_benchmark_ime = REG_IME;
    REG_IME = 0;
    rng_benchmark_start = stress_cycle_counter_read();
    for (rng_benchmark_index = 0;
         rng_benchmark_index < 10000;
         rng_benchmark_index++) {
        rng_benchmark_sink ^=
            ot_level_port_random(&source_parity_level);
    }
    rng_benchmark_elapsed =
        stress_cycle_counter_read() - rng_benchmark_start;
    REG_IME = rng_benchmark_ime;
    sram_write_u32(360, rng_benchmark_elapsed);
    sram_write_u32(364, rng_benchmark_sink);
    sram_write_u32(368, 10000);
    sram_write_u32(372, telemetry_detail_filter_hue_frames);
    sram_write_u32(376, telemetry_detail_palette_rebuilds);
    sram_write_u32(380, telemetry_detail_wave_frames);
    sram_write_u32(384, telemetry_detail_wild_dither_frames);
    sram_write_u32(388, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(392, TYRIAN_GBA_STRESS_LOADOUT);
    /* Additive TGW8 route/transition diagnostics. */
    sram_write_u32(400, AUTOTEST_FRONTEND_ROUTE_EPISODE);
    sram_write_u32(404, AUTOTEST_FRONTEND_ROUTE_SECTION);
    sram_write_u32(408, frontend_level.lvl_file_number);
    sram_write_u32(
        412,
        AUTOTEST_FULL_LOADOUT_STRESS_END_POSITION
    );
    sram_write_u32(
        416,
        AUTOTEST_FULL_LOADOUT_STRESS_DURATION_VBLANKS
    );
    sram_write_u32(420, source_parity_level.smoothies[0] != 0);
    sram_write_u32(424, source_parity_level.smoothies[1] != 0);
    sram_write_u32(428, source_parity_level.smoothie_data[0]);
    sram_write_u32(432, source_parity_level.smoothie_data[1]);
    sram_write_u32(436, telemetry_detail_wave_attenuated_frames);
    sram_write_u32(440, telemetry_detail_wave_pressure_score_max);
    sram_write_u32(444, telemetry_detail_wave_strength_min_q8);
    /*
     * Additive v73 legacy wave counters and exclusive cycle accounting.  The
     * loop work envelope is measured once per gameplay loop; commit,
     * audio/input, logic, render and prefetch are mutually exclusive
     * subranges.  Offsets 496..536 extend this with the global dispatcher.
     */
    sram_write_u32(448, telemetry_wave_dispatch_scope_attempts);
    sram_write_u32(452, telemetry_wave_dispatch_entries);
    sram_write_u32(456, telemetry_wave_dispatch_exits);
    sram_write_u32(
        460,
        telemetry_wave_dispatch_logic_busy_deferred
    );
    sram_write_u32(464, telemetry_wave_dispatch_idle_renders);
    sram_write_u32(468, telemetry_wave_dispatch_safety_forced);
    sram_write_u32(472, telemetry_perf_prefetch_cycles_total);
    sram_write_u32(476, telemetry_perf_prefetch_cycles_max);
    sram_write_u32(480, telemetry_perf_loop_work_cycles_total);
    sram_write_u32(484, telemetry_perf_loop_work_cycles_max);
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
    sram_write_u32(
        488,
        presentation_adaptive_dispatch_active &&
            gameplay_presentation_wave_scope_requested()
    );
    sram_write_u32(
        492,
        gameplay_presentation_wave_scope_requested() ?
            presentation_adaptive_dispatch_pressure :
            0
    );
#else
    sram_write_u32(488, 0);
    sram_write_u32(492, 0);
#endif
    /* Additive global adaptive-dispatch diagnostics. */
    sram_write_u32(496, telemetry_adaptive_dispatch_attempts);
    sram_write_u32(500, telemetry_adaptive_dispatch_entries);
    sram_write_u32(504, telemetry_adaptive_dispatch_severe_entries);
    sram_write_u32(508, telemetry_adaptive_dispatch_exits);
    sram_write_u32(
        512,
        telemetry_adaptive_dispatch_logic_busy_deferred
    );
    sram_write_u32(516, telemetry_adaptive_dispatch_idle_renders);
    sram_write_u32(520, telemetry_adaptive_dispatch_safety_forced);
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
    sram_write_u32(524, presentation_adaptive_dispatch_active);
    sram_write_u32(528, presentation_adaptive_dispatch_severe);
    sram_write_u32(532, presentation_adaptive_dispatch_pressure);
#else
    sram_write_u32(524, 0);
    sram_write_u32(528, 0);
    sram_write_u32(532, 0);
#endif
    sram_write_u32(536, TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH);
    /* The self-test mutates smoothie state, so run it after the snapshot. */
    sram_write_u32(396, autotest_detail_adapter_self_test());
    autotest_hotpath_asm_benchmark();
    sram_write_u32(540, autotest_hotpath_asm_self_test());
    autotest_detail_effect_differential_result =
        autotest_detail_effect_asm_differential_test();
    autotest_detail_effect_asm_benchmark();
#ifdef AUTOTEST_STACK_CANARY
    sram_write_u32(544, autotest_stack_canary_remaining());
    sram_write_u32(548, autotest_stack_canary_filled_bytes);
#else
    sram_write_u32(544, UINT32_MAX);
    sram_write_u32(548, 0);
#endif
    /* Additive v79 assembly differential and same-ROM microbench data. */
    sram_write_u32(
        552,
        autotest_level_port_hotpath_differential_result
    );
    sram_write_u32(
        556,
        autotest_colour_distance_differential_result
    );
    sram_write_u32(560, autotest_overlay_distance_c_cycles);
    sram_write_u32(564, autotest_overlay_distance_asm_cycles);
    sram_write_u32(568, autotest_palette_distance_c_cycles);
    sram_write_u32(572, autotest_palette_distance_asm_cycles);
    sram_write_u32(576, autotest_axis_overlap_c_cycles);
    sram_write_u32(580, autotest_axis_overlap_asm_cycles);
    sram_write_u32(584, 16384u);
    sram_write_u32(588, autotest_hotpath_benchmark_sink);
    /* Additive v80 Detail-effect ARM differential and microbench data. */
    sram_write_u32(592, autotest_detail_effect_differential_result);
    sram_write_u32(596, autotest_detail_palette_c_cycles);
    sram_write_u32(600, autotest_detail_palette_asm_cycles);
    sram_write_u32(604, 64u);
    sram_write_u32(608, autotest_detail_spotlight_c_cycles);
    sram_write_u32(612, autotest_detail_spotlight_asm_cycles);
    sram_write_u32(616, 512u);
    sram_write_u32(620, autotest_detail_wave_c_cycles);
    sram_write_u32(624, autotest_detail_wave_asm_cycles);
    sram_write_u32(628, 512u);
    sram_write_u32(632, TYRIAN_GBA_DETAIL_EFFECT_ASM);
    sram_write_u32(636, autotest_detail_effect_benchmark_sink);
    /* Additive v81 Sprite2 exact-scan differential and microbenchmark. */
    sram_write_u32(640, autotest_sprite2_exact_differential_result);
    sram_write_u32(644, autotest_sprite2_exact_c_cycles);
    sram_write_u32(648, autotest_sprite2_exact_asm_cycles);
    sram_write_u32(652, 16384u);
    sram_write_u32(656, telemetry_sprite2_filter_admission_denials);
    sram_write_u32(660, telemetry_sprite2_filter_fallback_hits);
    sram_write_u32(664, telemetry_sprite2_filter_fallback_builds);
    sram_write_u32(668, telemetry_sprite2_filter_evictions);
    sram_write_u32(672, telemetry_sprite2_exact_lookup_probes);
    sram_write_u32(676, telemetry_sprite2_exact_lookup_hits);
    sram_write_u32(680, TYRIAN_GBA_SPRITE2_EXACT_LOOKUP_ASM);
    sram_write_u32(684, TYRIAN_GBA_HOTPATH_ASM);
    /*
     * Audio is heap-backed in Maxmod.  Keep its liveness and actual heap
     * high-water in the stress schema so a linkable-but-silent build caused
     * by EWRAM exhaustion cannot pass the automated runtime check again.
     */
#ifdef AUTOTEST_STACK_CANARY
    sram_write_u32(688, autotest_ewram_heap_used());
    sram_write_u32(692, autotest_ewram_heap_remaining());
#else
    sram_write_u32(688, UINT32_MAX);
    sram_write_u32(692, UINT32_MAX);
#endif
    /* Additive v82 starfield batch differential and same-ROM microbench. */
    sram_write_u32(9000, autotest_starfield_differential_result);
    sram_write_u32(9004, autotest_starfield_divmod_c_cycles);
    sram_write_u32(9008, autotest_starfield_divmod_asm_cycles);
    sram_write_u32(9012, autotest_starfield_plot_c_cycles);
    sram_write_u32(9016, autotest_starfield_plot_asm_cycles);
    sram_write_u32(9020, 16384u);
    sram_write_u32(9024, TYRIAN_GBA_STARFIELD_BATCH_ASM);
    /* Additive v83 projectile direct-hint differential and microbench. */
    sram_write_u32(9028, autotest_projectile_hint_differential_result);
    sram_write_u32(9032, autotest_projectile_hint_c_cycles);
    sram_write_u32(9036, autotest_projectile_hint_asm_cycles);
    sram_write_u32(9040, 16384u);
    sram_write_u32(9044, TYRIAN_GBA_PROJECTILE_CACHE_HINT);
    sram_write_u32(9048, TYRIAN_GBA_PROJECTILE_HINT_ASM);
    sram_write_u32(9052, telemetry_projectile_hint_probes);
    sram_write_u32(9056, telemetry_projectile_hint_hits);
    sram_write_u32(9060, telemetry_projectile_hint_fallback_scans);
    /* Additive v84 effect-pool active directory and bit-scan validation. */
    sram_write_u32(9064, autotest_pool_bit_scan_differential_result);
    sram_write_u32(9068, autotest_pool_lowest_c_cycles);
    sram_write_u32(9072, autotest_pool_lowest_asm_cycles);
    sram_write_u32(9076, autotest_pool_highest_c_cycles);
    sram_write_u32(9080, autotest_pool_highest_asm_cycles);
    sram_write_u32(9084, 16384u);
    sram_write_u32(9088, TYRIAN_GBA_EFFECT_ACTIVE_MASK);
    sram_write_u32(9092, TYRIAN_GBA_POOL_BIT_SCAN_ASM);
    sram_write_u32(9096, source_pool_mask_consistency_test());
    /* Additive v85 player-shot lowest-free directory validation. */
    sram_write_u32(9100, player_shot_pool_consistency_test());
    sram_write_u32(9104, TYRIAN_GBA_PLAYER_SHOT_FREE_MASK);
    sram_write_u32(9108, telemetry_player_shot_allocator_calls);
    sram_write_u32(9112, telemetry_player_shot_allocator_slot_probes);
    sram_write_u32(9116, telemetry_player_shot_allocator_mask_word_probes);
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
    sram_write_u32(9120, active_player_shot_count);
#else
    sram_write_u32(9120, 0);
#endif
    /* Additive v86 persistent enemy active/collidable directories. */
    sram_write_u32(9124, TYRIAN_GBA_ENEMY_ACTIVE_MASK);
    sram_write_u32(9128, autotest_enemy_pool_consistency_snapshot);
    sram_write_u32(
        9132,
        autotest_enemy_pool_active_visits_snapshot
    );
    sram_write_u32(
        9136,
        autotest_enemy_pool_linear_visits_snapshot
    );
    sram_write_u32(
        9140,
        autotest_enemy_allocator_mask_probes_snapshot
    );
    sram_write_u32(
        9144,
        autotest_enemy_allocator_slot_probes_snapshot
    );
    /* Additive v87 player-shot update traversal and feature mix. */
    sram_write_u32(9148, TYRIAN_GBA_PLAYER_SHOT_UPDATE_MASK);
    sram_write_u32(9152, telemetry_player_shot_update_cycles_total);
    sram_write_u32(9156, telemetry_player_shot_update_cycles_max);
    sram_write_u32(9160, telemetry_player_shot_update_active_visits);
    sram_write_u32(9164, telemetry_player_shot_update_linear_visits);
    sram_write_u32(9168, telemetry_player_shot_update_complicated_visits);
    sram_write_u32(9172, telemetry_player_shot_update_trail_visits);
    sram_write_u32(9176, telemetry_player_shot_update_aimed_visits);
    sram_write_u32(9180, telemetry_player_shot_update_superpixel_visits);
    /* Additive v88 focused Boss-window diagnostics. */
    sram_write_u32(
        9184,
        AUTOTEST_FULL_LOADOUT_STRESS_FAST_FORWARD_POSITION
    );
    sram_write_u32(9188, AUTOTEST_FULL_LOADOUT_STRESS_FAST_FORWARD_TICKS);
    sram_write_u32(
        9192,
        AUTOTEST_FULL_LOADOUT_STRESS_BOSS_WINDOW_VBLANKS
    );
    sram_write_u32(9196, telemetry_boss_perf_started);
    sram_write_u32(9200, telemetry_boss_perf_completed);
    sram_write_u32(9204, telemetry_boss_perf_start_position);
    sram_write_u32(9208, telemetry_boss_perf_end_position);
    sram_write_u32(9212, telemetry_boss_perf_display_frames);
    sram_write_u32(9216, telemetry_boss_perf_missed_vblanks);
    sram_write_u32(9220, telemetry_boss_perf_logic_updates);
    sram_write_u32(9224, telemetry_boss_perf_logic_cycles);
    sram_write_u32(9228, telemetry_boss_perf_render_cycles);
    sram_write_u32(9232, telemetry_boss_perf_collision_cycles);
    sram_write_u32(9236, telemetry_boss_perf_player_shot_update_cycles);
    sram_write_u32(9240, telemetry_boss_perf_collision_candidate_visits);
    sram_write_u32(9244, telemetry_boss_perf_collision_hits);
    sram_write_u32(9248, telemetry_boss_perf_shot_update_active_visits);
    sram_write_u32(9252, telemetry_boss_perf_shot_update_linear_visits);
    sram_write_u32(9256, telemetry_boss_perf_shot_update_trail_visits);
    sram_write_u32(9260, telemetry_boss_perf_enemy_active_visits);
    sram_write_u32(9264, telemetry_boss_perf_projectile_hits);
    sram_write_u32(9268, telemetry_boss_perf_projectile_misses);
    sram_write_u32(9272, telemetry_boss_perf_effect_hits);
    sram_write_u32(9276, telemetry_boss_perf_effect_misses);
    sram_write_u32(9280, telemetry_boss_perf_presentation_completed);
    sram_write_u32(9284, telemetry_boss_perf_presentation_deferred);
    sram_write_u32(9288, telemetry_boss_perf_player_shot_spawns);
    sram_write_u32(9292, telemetry_boss_perf_sprite2_misses);
    sram_write_u32(9296, telemetry_boss_perf_sprite2_evictions);
    sram_write_u32(9300, telemetry_boss_perf_sprite2_upload_bytes);
    sram_write_u32(9304, telemetry_boss_perf_l2_hits);
    sram_write_u32(9308, telemetry_boss_perf_l2_misses);
    sram_write_u32(9312, telemetry_boss_perf_filter_admission_denials);
    sram_write_u32(9316, telemetry_boss_perf_filter_fallback_hits);
    sram_write_u32(9320, telemetry_boss_perf_filter_fallback_builds);
    sram_write_u32(9324, telemetry_boss_perf_effect_oam_culls);
    sram_write_u32(9328, telemetry_boss_perf_player_shot_oam_culls);
    sram_write_u32(9332, telemetry_boss_perf_enemy_kills);
    sram_write_u32(9336, telemetry_boss_perf_audio_frames);
    sram_write_u32(9340, telemetry_boss_perf_wall_vblanks);
    sram_write_u32(9344, TYRIAN_GBA_COLLISION_SNAPSHOT_ENABLED);
    sram_write_u32(
        9348,
        TYRIAN_GBA_COLLISION_ACTIVE_DIRECTORY_ENABLED
    );
    sram_write_u32(9352, autotest_collision_hit_apply_calls_snapshot);
    sram_write_u32(9356, autotest_collision_status_link_visits_snapshot);
    sram_write_u32(9360, autotest_collision_kill_group_visits_snapshot);
    sram_write_u32(
        9364,
        autotest_collision_damaged_transition_visits_snapshot
    );
    sram_write_u32(
        9368,
        autotest_collision_player_contact_visits_snapshot
    );
    sram_write_u32(9372, autotest_collision_zinglon_visits_snapshot);
    sram_write_u32(9376, telemetry_boss_perf_hit_apply_calls);
    sram_write_u32(9380, telemetry_boss_perf_status_link_visits);
    sram_write_u32(9384, telemetry_boss_perf_kill_group_visits);
    sram_write_u32(9388, telemetry_boss_perf_damaged_transition_visits);
    sram_write_u32(9392, telemetry_boss_perf_player_contact_visits);
    sram_write_u32(9396, telemetry_boss_perf_zinglon_visits);
    /* Additive structural-first OAM/cache scheduling diagnostics. */
    sram_write_u32(9400, telemetry_structural_oam_required_max);
    sram_write_u32(9404, telemetry_scene_oam_demand_max);
    sram_write_u32(9408, telemetry_effect_oam_culls);
    sram_write_u32(9412, telemetry_enemy_shot_oam_culls);
    sram_write_u32(9416, telemetry_player_shot_oam_culls);
    sram_write_u32(9420, telemetry_enemy_oam_culls);
    sram_write_u32(9424, telemetry_enemy_oam_rotation_frames);
    sram_write_u32(9428, telemetry_enemy_cache_prime_attempts);
    sram_write_u32(9432, telemetry_enemy_cache_prime_failures);
    sram_write_u32(9436, telemetry_projectile_oam_rotation_frames);
    sram_write_u32(9440, telemetry_effect_oam_rotation_frames);
    sram_write_u32(9444, telemetry_critical_oam_promotions);
    /* Additive build-time Boss manifest diagnostics. */
    sram_write_u32(
        9448,
        autotest_boss_manifest_spawn_count_snapshot
    );
    sram_write_u32(
        9452,
        autotest_boss_manifest_active_count_snapshot
    );
    sram_write_u32(
        9456,
        autotest_boss_manifest_episode_snapshot
    );
    sram_write_u32(
        9460,
        autotest_boss_manifest_lvl_snapshot
    );
    /* Additive release-path compact Boss cache diagnostics. */
    sram_write_u32(9464, telemetry_boss_compact_context_frames);
    sram_write_u32(9468, telemetry_boss_compact_pressure_frames);
    sram_write_u32(9472, telemetry_boss_compact_active_frames);
    sram_write_u32(9476, telemetry_boss_compact_activations);
    sram_write_u32(9480, telemetry_boss_compact_boss_count_max);
    sram_write_u32(9484, telemetry_boss_compact_full_size_count_max);
    sram_write_u32(9488, telemetry_boss_compact_rendered_objects);
    sram_write_u32(9492, autotest_sidekick_graphic[0]);
    sram_write_u32(9496, autotest_sidekick_graphic[1]);
    sram_write_u32(9500, autotest_sidekick_cache_tile[0]);
    sram_write_u32(9504, autotest_sidekick_cache_tile[1]);
    sram_write_u32(9508, autotest_sidekick_oam_index[0]);
    sram_write_u32(9512, autotest_sidekick_oam_index[1]);
#if !TYRIAN_GBA_STRESS_LOADOUT
    {
        u8 left = autotest_sidekick_oam_index[0];
        u8 right = autotest_sidekick_oam_index[1];
        u8 split_layout_pass = 1;
        u8 side;
        u16 left_shadow_bottom = left + 1u < HARDWARE_OAM_ENTRIES ?
            oam_shadow[left + 1u].attr2 : 0xffffu;
        u16 right_shadow_bottom = right + 1u < HARDWARE_OAM_ENTRIES ?
            oam_shadow[right + 1u].attr2 : 0xffffu;
        u16 left_hardware_bottom = left + 1u < HARDWARE_OAM_ENTRIES ?
            OAM[left + 1u].attr2 : 0xffffu;
        u16 right_hardware_bottom = right + 1u < HARDWARE_OAM_ENTRIES ?
            OAM[right + 1u].attr2 : 0xffffu;

        sram_write_u32(9516, oam_shadow[left].attr2);
        sram_write_u32(9520, oam_shadow[right].attr2);
        sram_write_u32(9524, OAM[left].attr2);
        sram_write_u32(9528, OAM[right].attr2);
        sram_write_u32(9532, left_shadow_bottom);
        sram_write_u32(9536, right_shadow_bottom);
        sram_write_u32(9540, left_hardware_bottom);
        sram_write_u32(9544, right_hardware_bottom);
        sram_write_u32(9548, SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE);
        for (side = 0; side < 2u; side++) {
            u8 oam_index = autotest_sidekick_oam_index[side];

            if (!source_enemy_cache_tile_is_split(
                    autotest_sidekick_cache_tile[side]
                )) {
                continue;
            }
            if (
                oam_index + 1u >= HARDWARE_OAM_ENTRIES ||
                (oam_shadow[oam_index].attr2 & 0x03ffu) !=
                    SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE ||
                (oam_shadow[oam_index + 1u].attr2 & 0x03ffu) !=
                    SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE ||
                (OAM[oam_index].attr2 & 0x03ffu) !=
                    SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE ||
                (OAM[oam_index + 1u].attr2 & 0x03ffu) !=
                    SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE
            ) {
                split_layout_pass = 0;
            }
        }
        sram_write_u32(9552, split_layout_pass);
    }
#else
    /*
     * The split release slot does not exist in the synthetic stress VRAM
     * layout.  Keep the additive telemetry schema readable without making
     * that unrelated build depend on release-only tile constants.
     */
    sram_write_u32(9516, 0);
    sram_write_u32(9520, 0);
    sram_write_u32(9524, 0);
    sram_write_u32(9528, 0);
    sram_write_u32(9532, 0);
    sram_write_u32(9536, 0);
    sram_write_u32(9540, 0);
    sram_write_u32(9544, 0);
    sram_write_u32(9548, 0);
    sram_write_u32(9552, 1);
#endif
#if !TYRIAN_GBA_STRESS_LOADOUT
    /* Additive compact-palette ownership/training diagnostics. */
    sram_write_u32(9556, source_boss_compact_palette_valid);
    sram_write_u32(9560, source_boss_compact_primary_table);
    {
        u8 group;
        u8 colour;

        for (group = 0; group < SOURCE_BOSS_COMPACT_PALETTE_COUNT; group++) {
            u16 bank = (u16)(source_boss_compact_palette_bank(group) * 16u);

            for (colour = 0; colour < 15u; colour++) {
                sram_write_u32(
                    9564u + ((u32)group * 15u + colour) * 4u,
                    source_boss_compact_palette_colour[group][colour]
                );
            }
            for (colour = 0; colour < 16u; colour++) {
                sram_write_u32(
                    9684u + ((u32)group * 16u + colour) * 4u,
                    SPRITE_PALETTE[bank + colour]
                );
            }
        }
    }
    sram_write_u32(9812, source_boss_compact_training_position);
    sram_write_u32(9816, source_boss_compact_training_enemy_count);
    sram_write_u32(9820, source_boss_compact_training_boss_count);
    sram_write_u32(9824, source_boss_compact_training_full_size_count);
    sram_write_u32(9828, source_boss_compact_palette_retrain_count);
    sram_write_u32(9832, source_boss_compact_trained_boss_count);
    sram_write_u32(9836, source_boss_compact_palette_refresh_pending);
    sram_write_u32(9840, source_boss_compact_palette_error_total_max);
    sram_write_u32(
        9844,
        source_boss_compact_palette_error_pixels_at_max
    );
    sram_write_u32(9848, source_boss_compact_palette_train_epoch);
#else
    sram_write_u32(9556, 0);
    sram_write_u32(9560, 0);
    sram_write_u32(9812, 0);
    sram_write_u32(9816, 0);
    sram_write_u32(9820, 0);
    sram_write_u32(9824, 0);
    sram_write_u32(9828, 0);
    sram_write_u32(9832, 0);
    sram_write_u32(9836, 0);
    sram_write_u32(9840, 0);
    sram_write_u32(9844, 0);
    sram_write_u32(9848, 0);
#endif
    /* Detail-dispatch/background-motion parity diagnostics. */
    sram_write_u32(9852, background2_enabled_snapshot);
    sram_write_u32(9856, background2_wrap_active_snapshot);
    sram_write_u32(9860, background2_present_snapshot);
    sram_write_u32(9864, background2_upload_pending_snapshot);
    sram_write_u32(9868, back_move_snapshot);
    sram_write_u32(9872, back_move2_snapshot);
    sram_write_u32(9876, bg1_scroll_speed_snapshot);
    sram_write_u32(9880, bg2_scroll_speed_snapshot);
    sram_write_u32(9884, bg1_scroll_pixel_snapshot);
    sram_write_u32(9888, bg2_scroll_pixel_snapshot);
    sram_write_u32(9892, source_background2_wrap_threshold_scroll);
    sram_write_u32(9896, source_background2_wrap_to_scroll);
    sram_write_u32(9900, source_background2_wrap_count);
    sram_write_u32(9904, telemetry_ground_attachment_samples);
    sram_write_u32(9908, telemetry_ground_attachment_camera_samples);
    sram_write_u32(9912, telemetry_ground_attachment_failures);
    sram_write_u32(9916, telemetry_ground_attachment_max_delta_x);
    sram_write_u32(9920, telemetry_ground_attachment_max_delta_y);
    /* Additive ICESECRET layer/camera diagnostics. */
    sram_write_u32(9924, (u16)camera_offset_x_snapshot);
    sram_write_u32(9928, (u16)camera_offset_y_snapshot);
    sram_write_u32(9932, map_x_offset_snapshot);
    sram_write_u32(9936, map_x2_offset_snapshot);
    sram_write_u32(9940, bg1_hofs_snapshot);
    sram_write_u32(9944, bg2_hofs_snapshot);
    sram_write_u32(9948, bg1_presentation_scroll_snapshot);
    sram_write_u32(9952, bg2_presentation_scroll_snapshot);
    sram_write_u32(9956, bg1_camera_scroll_snapshot);
    sram_write_u32(9960, bg2_camera_scroll_snapshot);
    sram_write_u32(9964, bg1_vofs_snapshot);
    sram_write_u32(9968, bg2_vofs_snapshot);
    sram_write_u32(9972, definition_130_count_snapshot);
    sram_write_u32(9976, definition_130_pool_mask_snapshot);
    sram_write_u32(9980, definition_130_ground_count_snapshot);
    sram_write_u32(9984, definition_132_count_snapshot);
    sram_write_u32(9988, definition_132_pool_mask_snapshot);
    sram_write_u32(9992, definition_132_ground_count_snapshot);
    sram_write_u32(9996, background2_over_snapshot);
    sram_write_u32(10000, background3_over_snapshot);
    sram_write_u32(10004, TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER);
    sram[124] = *(volatile const u8 *)save_type_marker;
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
#endif

#ifdef AUTOTEST_EPISODE_WRAP_FLOW
static u8 autotest_episode_wrap_selected;
static u8 autotest_episode_wrap_source_pass;
static u8 autotest_episode_wrap_story_seen;
static u8 autotest_episode_wrap_announcement_seen;

static u16 autotest_episode_wrap_input(void)
{
    if (frontend_transition_busy() || frontend_frame_pending) return 0;
    if (
        game_state == STATE_NEXT_LEVEL_MENU &&
        !autotest_episode_wrap_selected
    ) {
        autotest_episode_wrap_source_pass = (u8)(
            frontend_episode == 3 &&
            frontend_main_section == 44 &&
            frontend_map_ready &&
            frontend_map.requested_section == 44 &&
            frontend_map.choice_count == 2 &&
            frontend_map.map_planet[1] == 21 &&
            frontend_map.map_section[1] == 42 &&
            frontend_map_level[1].episode_complete
        );
        frontend_selection = 1;
        autotest_episode_wrap_selected = 1;
        return KEY_A;
    }
    if (game_state == STATE_EPISODE_SCENE) {
        if (frontend_scene_episode_announcement) {
            autotest_episode_wrap_announcement_seen = 1;
            return KEY_A;
        }
        if (frontend_scene_waiting_input) {
            autotest_episode_wrap_story_seen = 1;
            return KEY_A;
        }
        if (frontend_scene_animation_active) return KEY_A;
    }
    return 0;
}

static void autotest_episode_wrap_finish_update(void)
{
    const OtDataCatalog *catalog;
    volatile u8 *sram;
    u8 passed;

    if (
        !autotest_episode_wrap_selected ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }

    catalog = ot_data_catalog();
    passed = (u8)(
        autotest_episode_wrap_source_pass &&
        autotest_episode_wrap_story_seen &&
        autotest_episode_wrap_announcement_seen &&
        frontend_episode == 0 &&
        frontend_main_section == 1 &&
        frontend_map_ready &&
        frontend_map.requested_section == 1 &&
        !frontend_map.episode_complete &&
        frontend_player_items_valid &&
        frontend_player_items.ship == 2 &&
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id == 23 &&
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power == 1 &&
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id == 24 &&
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power == 1 &&
        frontend_route_state.player_ship == 2 &&
        frontend_selection == 4 &&
        game_state == STATE_GAME_MENU &&
        frontend_mode4_active &&
        catalog != 0 &&
        mmActive()
    );

    sram = (volatile u8 *)0x0e000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'S';
    sram[3] = 'I';
    sram[4] = 1;
    sram[5] = passed;
    sram_write_u32(8, autotest_episode_wrap_source_pass);
    sram_write_u32(12, frontend_episode);
    sram_write_u32(16, frontend_main_section);
    sram_write_u32(20, frontend_map.requested_section);
    sram_write_u32(24, game_state);
    sram_write_u32(28, frontend_selection);
    sram_write_u32(32, frontend_map.choice_count);
    sram_write_u32(36, frontend_map.resolved_section);
    sram_write_u32(40, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(44, mmActive());
    sram_write_u32(48, autotest_episode_wrap_story_seen);
    sram_write_u32(52, autotest_episode_wrap_announcement_seen);
    sram_write_u32(56, telemetry_missed_vblanks);
    sram_write_u32(60, telemetry_audio_frames);
    sram_write_u32(64, telemetry_frontend_full_redraws);
    sram_write_u32(68, ot_data_episode_scene_textres_hits());
    sram_write_u32(72, telemetry_missed_vblanks_episode_scene);
    sram_write_u32(76, telemetry_missed_vblanks_scene_picture);
    sram_write_u32(80, telemetry_missed_vblanks_scene_text);
    sram_write_u32(84, telemetry_missed_vblanks_scene_animation);
    sram_write_u32(88, telemetry_missed_vblanks_frontend_other);
    sram_write_u32(92, telemetry_missed_vblanks_scene_picture_decode);
    sram_write_u32(96, telemetry_missed_vblanks_scene_picture_scale);
    sram_write_u32(100, telemetry_missed_vblanks_scene_picture_present);
    sram_write_u32(104, frontend_player_items.ship);
    sram_write_u32(
        108,
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id
    );
    sram_write_u32(
        112,
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power
    );
    sram_write_u32(
        116,
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id
    );
    sram_write_u32(
        120,
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power
    );
    sram_write_u32(124, frontend_route_state.player_ship);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_SCRIPTED_SURVIVAL_FLOW
static u8 autotest_scripted_survival_exit_pending;
static u8 autotest_scripted_survival_game_over_pass;

static void autotest_scripted_survival_update(void)
{
    static u8 settled_frames;
    FrontendScriptCheckpoint checkpoint;
    u8 checkpoint_valid;

    if (autotest_scripted_survival_exit_pending) return;
    if (++settled_frames < 4) return;
    checkpoint_valid = frontend_script_checkpoint_read(&checkpoint);

    autotest_scripted_survival_game_over_pass = (u8)(
        game_state == STATE_GAME_OVER &&
        frontend_script_game_active &&
        checkpoint_valid &&
        checkpoint.main_section == 44 &&
        source_parity_level.arcade_mode &&
        source_parity_level.engage_mode &&
        source_parity_level.galaga_mode &&
        source_parity_level.player_ship == 13 &&
        source_parity_level.player_front_weapon_id == 39 &&
        source_parity_level.player_front_weapon_power == 1 &&
        player_lives == 1 &&
        !player_alive &&
        telemetry_game_over_music_starts == 1
    );

    /* A real player may dismiss GAME OVER immediately. */
    pad_pressed = KEY_A;
    game_over_update();
    autotest_scripted_survival_exit_pending = 1;
}

static void autotest_scripted_survival_finish_update(void)
{
    volatile u8 *sram;
    FrontendScriptCheckpoint checkpoint;
    u8 checkpoint_valid;
    u8 restored;

    if (
        !autotest_scripted_survival_exit_pending ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }
    checkpoint_valid = frontend_script_checkpoint_read(&checkpoint);

    restored = (u8)(
        autotest_scripted_survival_game_over_pass &&
        !frontend_script_game_active &&
        checkpoint_valid &&
        frontend_main_section == 44 &&
        frontend_map.requested_section == 44 &&
        frontend_map_ready &&
        frontend_player_items_valid &&
        player_cash == checkpoint.cash &&
        memcmp(
            &frontend_player_items,
            &checkpoint.items,
            sizeof(frontend_player_items)
        ) == 0 &&
        frontend_selection == 4 &&
        frontend_mode4_active &&
        telemetry_game_over_exits == 1
    );

    sram = (volatile u8 *)0x0e000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'S';
    sram[3] = 'X';
    sram[4] = 1;
    sram[5] = restored;
    sram_write_u32(8, checkpoint.main_section);
    sram_write_u32(12, frontend_main_section);
    sram_write_u32(16, frontend_map.requested_section);
    sram_write_u32(20, game_state);
    sram_write_u32(24, frontend_selection);
    sram_write_u32(28, source_parity_level.player_front_weapon_power);
    sram_write_u32(32, player_lives);
    sram_write_u32(36, telemetry_game_over_exits);
    sram_write_u32(40, checkpoint.cash);
    sram_write_u32(44, player_cash);
    sram_write_u32(48, source_parity_level.engage_mode);
    sram_write_u32(52, source_parity_level.galaga_mode);
    sram_write_u32(56, frontend_map.choice_count);
    sram_write_u32(60, frontend_map.resolved_section);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_DEATH_FLOW
static u8 autotest_death_exit_pending;
static u8 autotest_death_game_over_pass;
static u8 autotest_death_normal_weapon_pass;

static void autotest_death_update(void)
{
    static u16 settled_frames;
    static u8 observed_game_over_music_active;
    const OtDataCatalog *catalog;
    volatile u8 *sram;

    if (autotest_death_exit_pending) return;

    settled_frames++;
    if (mmActive()) observed_game_over_music_active = 1;
    if (settled_frames < 4) return;
    /*
     * The source GAME OVER cue is finite.  Do not synthesize the exit key
     * until Maxmod reaches the stripped order-list boundary; this turns the
     * old four-frame "song started" smoke test into an actual no-loop
     * regression.  The bounded fallback records pass=0 instead of hanging.
     */
    if (mmActive() && settled_frames < 1200) return;

    catalog = ot_data_catalog();
    sram = (volatile u8 *)0x0E000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'D';
    sram[3] = '2';
    sram_write_u32(
        4,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(8, mmActive());
    sram_write_u32(12, game_state);
    sram_write_u32(16, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(20, player_alive);
    sram_write_u32(24, player_exploding_ticks);
    sram_write_u32(28, telemetry_player_death_large_explosions);
    sram_write_u32(32, telemetry_player_death_sfx_9);
    sram_write_u32(36, telemetry_player_death_sfx_11);
    sram_write_u32(40, telemetry_player_death_sfx_22);
    sram_write_u32(44, telemetry_player_death_music_fade_steps);
    sram_write_u32(48, telemetry_game_over_music_starts);
    sram_write_u32(52, telemetry_game_over_overlay_frames);
    sram_write_u32(56, source_parity_level.rng_call_count);
    sram_write_u32(60, game_over_tile_upload_pending);
    sram_write_u32(64, source_parity_level.level_end);
    sram_write_u32(68, frontend_mode4_active);
    sram_write_u32(72, active_effect_count);
    sram_write_u32(76, telemetry_effect_drops);
    sram_write_u32(80, telemetry_display_frames);
    sram_write_u32(84, oam_count);
    autotest_death_normal_weapon_pass = (
        TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER == 0 &&
        source_parity_level.player_front_weapon_id == 1 &&
        source_parity_level.player_front_weapon_power == 1 &&
        source_front_weapon_bound &&
        source_front_weapon_port_id == 1 &&
        source_front_weapon_power == 1 &&
        source_front_weapon_hdt_id == 155 &&
        source_front_weapon_valid
    );
    sram_write_u32(120, TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER);
    sram_write_u32(
        124,
        source_parity_level.player_front_weapon_id
    );
    sram_write_u32(
        128,
        source_parity_level.player_front_weapon_power
    );
    sram_write_u32(132, source_front_weapon_hdt_id);
    sram_write_u32(136, source_front_weapon_valid);
    sram_write_u32(140, autotest_death_normal_weapon_pass);
    sram_write_u32(144, observed_game_over_music_active);
    sram_write_u32(
        148,
        telemetry_game_over_music_natural_stops
    );
    sram_write_u32(152, settled_frames);
    autotest_death_game_over_pass = (
        game_state == STATE_GAME_OVER &&
        catalog != 0 &&
        catalog->selected_mus_song == 10 &&
        observed_game_over_music_active &&
        !mmActive() &&
        telemetry_game_over_music_natural_stops == 1 &&
        !TYRIAN_GBA_DEV_PLAYER_INVINCIBLE &&
        !player_alive &&
        player_exploding_ticks == 0 &&
        telemetry_player_death_large_explosions == 120 &&
        telemetry_player_death_sfx_22 == 1 &&
        telemetry_player_death_sfx_9 +
            telemetry_player_death_sfx_11 > 0 &&
        telemetry_player_death_music_fade_steps == 59 &&
        telemetry_game_over_music_starts == 1 &&
        telemetry_game_over_overlay_frames >= 4 &&
        !game_over_tile_upload_pending &&
        source_parity_level.level_end == 0 &&
        !frontend_mode4_active
    );
    sram_write_u32(88, autotest_death_game_over_pass);

#ifdef AUTOTEST_DEATH_CAPTURE
    /*
     * Optional visual harness: stop before the synthetic return key so
     * mGBA -O captures the complete live gameplay/GAME OVER composition.
     */
    __asm__ volatile("swi 3");
#endif

    /*
     * Exercise the source "any input" exit after recording the live Mode-0
     * overlay, then verify that Game Menu restores the title module.
     */
    pad_pressed = KEY_A;
    game_over_update();
    autotest_death_exit_pending = 1;
}

static void autotest_death_finish_update(void)
{
    const OtDataCatalog *catalog;

    if (
        !autotest_death_exit_pending ||
        game_state != STATE_GAME_MENU ||
        frontend_transition_busy() ||
        frontend_frame_pending
    ) {
        return;
    }
    catalog = ot_data_catalog();
    sram_write_u32(
        92,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(96, game_state);
    sram_write_u32(100, frontend_selection);
    sram_write_u32(104, frontend_mode4_active);
    sram_write_u32(108, telemetry_game_over_exits);
    sram_write_u32(112, mmActive());
    sram_write_u32(
        116,
        autotest_death_game_over_pass &&
        autotest_death_normal_weapon_pass &&
        catalog != 0 &&
        catalog->selected_mus_song == 2 &&
        frontend_selection == 4 &&
        frontend_mode4_active &&
        telemetry_game_over_exits == 1 &&
        mmActive()
    );
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_JUKEBOX_FLOW
static u8 autotest_jukebox_entered;

static u16 autotest_jukebox_input(void)
{
    if (
        game_state == STATE_INTRO_LOGO_1 ||
        game_state == STATE_INTRO_LOGO_2
    ) {
        return KEY_A;
    }
    if (game_state == STATE_TITLE) {
        if (autotest_jukebox_entered) return 0;
        return frontend_selection < 3 ? KEY_DOWN : KEY_A;
    }
    if (game_state != STATE_JUKEBOX) return 0;

    autotest_jukebox_entered = 1;
    switch (telemetry_jukebox_frames + 1u) {
    case 24:
        return KEY_LEFT;
    case 48:
        return KEY_RIGHT;
    case 72:
        return KEY_A;
    case 96:
    case 120:
        return KEY_SELECT;
    case 160:
        return KEY_B;
    default:
        return 0;
    }
}

static void autotest_jukebox_maybe_finish(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 passed;

#ifdef AUTOTEST_JUKEBOX_CAPTURE
    if (
        game_state == STATE_JUKEBOX &&
        telemetry_jukebox_frames >= 140
    ) {
        __asm__ volatile("swi 3");
    }
#endif
    if (
        !autotest_jukebox_entered ||
        game_state != STATE_TITLE
    ) {
        return;
    }

    passed = (
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        mmActive() &&
        frontend_mode4_active &&
        telemetry_jukebox_frames >= 176 &&
        telemetry_jukebox_track_changes == 3 &&
        telemetry_jukebox_previous_wraps == 1 &&
        telemetry_jukebox_next_wraps == 1 &&
        telemetry_jukebox_text_toggles == 2 &&
        telemetry_jukebox_text_map_commits >= 5 &&
        telemetry_jukebox_palette_commits >= 30 &&
        telemetry_jukebox_max_oam <= HARDWARE_OAM_ENTRIES &&
        telemetry_jukebox_max_oam >= 80 &&
        telemetry_jukebox_exits == 1 &&
        jukebox_song == 1 &&
        !jukebox_hide_text &&
        jukebox_fade_level == 0
    );

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'J';
    sram[3] = '1';
    sram_write_u32(4, passed);
    sram_write_u32(
        8,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(12, game_state);
    sram_write_u32(16, frontend_mode4_active);
    sram_write_u32(20, telemetry_jukebox_frames);
    sram_write_u32(24, telemetry_jukebox_track_changes);
    sram_write_u32(28, telemetry_jukebox_previous_wraps);
    sram_write_u32(32, telemetry_jukebox_next_wraps);
    sram_write_u32(36, telemetry_jukebox_text_toggles);
    sram_write_u32(40, telemetry_jukebox_text_map_commits);
    sram_write_u32(44, telemetry_jukebox_palette_commits);
    sram_write_u32(48, telemetry_jukebox_max_oam);
    sram_write_u32(52, telemetry_jukebox_exits);
    sram_write_u32(56, mmActive());
    sram_write_u32(60, jukebox_song);
    sram_write_u32(64, jukebox_hide_text);
    sram_write_u32(68, jukebox_quitting);
    sram_write_u32(72, jukebox_fade_level);
    sram_write_u32(76, MSL_NSONGS);
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_DEMO_FLOW
static u8 autotest_demo_phase;
static u8 autotest_demo_checks = 1;
static u16 autotest_demo_play_frames;
static u8 autotest_demo_nonzero_keys_seen;
static u8 autotest_demo_completed;
static u8 autotest_demo_input_streams;

static u16 autotest_demo_input(void)
{
    if (
        game_state == STATE_TITLE &&
        autotest_demo_phase == 1
    ) {
        return frontend_selection < 2 ? KEY_DOWN : KEY_A;
    }
    return 0;
}

static void autotest_demo_validate_header(u8 demo_number)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    static const char *const expected_name[FRONTEND_DEMO_COUNT] = {
        "TYRIAN   ",
        "SAVARA   ",
        "SOH JIN  ",
        "MINES    ",
        "DELIANI  "
    };
    static const u8 expected_level[FRONTEND_DEMO_COUNT] = {
        9, 12, 13, 7, 5
    };
    static const u8 expected_front[FRONTEND_DEMO_COUNT] = {
        7, 5, 19, 19, 19
    };
    static const u8 expected_rear[FRONTEND_DEMO_COUNT] = {
        29, 29, 0, 0, 0
    };
    static const u8 expected_left[FRONTEND_DEMO_COUNT] = {
        21, 21, 21, 19, 19
    };
    static const u8 expected_right[FRONTEND_DEMO_COUNT] = {
        20, 21, 23, 19, 19
    };
    static const u8 expected_initial_episode[FRONTEND_DEMO_COUNT] = {
        1, 1, 2, 1, 1
    };
    static const u8 expected_shield[FRONTEND_DEMO_COUNT] = {
        6, 8, 6, 9, 9
    };
    static const u8 expected_special[FRONTEND_DEMO_COUNT] = {
        0, 2, 0, 0, 0
    };
    static const u8 expected_ship[FRONTEND_DEMO_COUNT] = {
        1, 6, 4, 4, 3
    };
    static const u8 expected_front_power[FRONTEND_DEMO_COUNT] = {
        1, 10, 3, 10, 10
    };
    static const u8 expected_rear_power[FRONTEND_DEMO_COUNT] = {
        1, 11, 1, 1, 1
    };
    static const u8 expected_song[FRONTEND_DEMO_COUNT] = {
        17, 32, 15, 15, 13
    };
    static const u16 expected_ship_graphic[FRONTEND_DEMO_COUNT] = {
        233, 195, 157, 157, 157
    };
    static const u8 expected_armor[FRONTEND_DEMO_COUNT] = {
        10, 27, 20, 20, 17
    };
    static const u8 expected_shield_power[FRONTEND_DEMO_COUNT] = {
        7, 10, 7, 12, 12
    };
    u8 index;

    if (demo_number == 0 || demo_number > FRONTEND_DEMO_COUNT) {
        autotest_demo_checks = 0;
        return;
    }
    index = (u8)(demo_number - 1u);
    autotest_demo_checks =
        autotest_demo_checks &&
        frontend_demo_active &&
        frontend_demo_file != 0 &&
        game_state == STATE_PLAY &&
        frontend_demo_number == demo_number &&
        telemetry_demo_starts == demo_number &&
        telemetry_demo_idle_starts == 1 &&
        frontend_play_mode == FRONTEND_PLAY_FULL_GAME &&
        frontend_difficulty == FRONTEND_DIFFICULTY_NORMAL &&
        frontend_level_ready &&
        source_parity_level.assets_valid &&
        catalog != 0 &&
        frontend_demo_header.episode == 1 &&
        strcmp(
            frontend_demo_header.level_name,
            expected_name[index]
        ) == 0 &&
        frontend_demo_header.lvl_file_number ==
            expected_level[index] &&
        frontend_demo_header.front_weapon ==
            expected_front[index] &&
        frontend_demo_header.rear_weapon ==
            expected_rear[index] &&
        frontend_demo_header.super_arcade_mode == 0 &&
        frontend_demo_header.sidekick[0] ==
            expected_left[index] &&
        frontend_demo_header.sidekick[1] ==
            expected_right[index] &&
        frontend_demo_header.generator == 5 &&
        frontend_demo_header.sidekick_level == 2 &&
        frontend_demo_header.sidekick_series == 1 &&
        frontend_demo_header.initial_episode ==
            expected_initial_episode[index] &&
        frontend_demo_header.shield ==
            expected_shield[index] &&
        frontend_demo_header.special ==
            expected_special[index] &&
        frontend_demo_header.ship == expected_ship[index] &&
        frontend_demo_header.front_power ==
            expected_front_power[index] &&
        frontend_demo_header.rear_power ==
            expected_rear_power[index] &&
        frontend_demo_header.source_song ==
            expected_song[index] &&
        frontend_level.lvl_file_number ==
            expected_level[index] &&
        frontend_level.source_song == expected_song[index] &&
        source_parity_level.player_front_weapon_id ==
            expected_front[index] &&
        source_parity_level.player_front_weapon_power ==
            expected_front_power[index] &&
        source_parity_level.player_rear_weapon_id ==
            expected_rear[index] &&
        source_parity_level.player_rear_weapon_power ==
            expected_rear_power[index] &&
        source_parity_level.player_special ==
            expected_special[index] &&
        source_parity_level.player_sidekick[0] ==
            expected_left[index] &&
        source_parity_level.player_sidekick[1] ==
            expected_right[index] &&
        source_parity_level.player_generator == 5 &&
        source_parity_level.player_shield_item ==
            expected_shield[index] &&
        source_parity_level.player_ship == expected_ship[index] &&
        player_generator_power == 30 &&
        player_ship_graphic == expected_ship_graphic[index] &&
        player_ship_animation == 2 &&
        player_armor == expected_armor[index] &&
        player_shield == expected_shield_power[index] &&
        player_shield_max ==
            (u8)(expected_shield_power[index] * 2u) &&
        catalog->selected_mus_song == expected_song[index];
}

static void autotest_demo_finish_to_sram(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 passed =
        autotest_demo_checks &&
        autotest_demo_completed == FRONTEND_DEMO_COUNT &&
        autotest_demo_input_streams == FRONTEND_DEMO_COUNT &&
        game_state == STATE_TITLE &&
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !frontend_demo_active &&
        frontend_demo_file == 0 &&
        frontend_demo_number == FRONTEND_DEMO_COUNT &&
        telemetry_demo_starts == FRONTEND_DEMO_COUNT &&
        telemetry_demo_idle_starts == 1 &&
        telemetry_demo_aborts == FRONTEND_DEMO_COUNT &&
        telemetry_demo_finishes == 0 &&
        telemetry_demo_parse_failures == 0 &&
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        mmActive();

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'D';
    sram[3] = 'M';
    sram_write_u32(4, passed);
    sram_write_u32(8, telemetry_demo_starts);
    sram_write_u32(12, telemetry_demo_idle_starts);
    sram_write_u32(16, telemetry_demo_aborts);
    sram_write_u32(20, telemetry_demo_finishes);
    sram_write_u32(24, telemetry_demo_parse_failures);
    sram_write_u32(28, frontend_demo_number);
    sram_write_u32(32, game_state);
    sram_write_u32(36, frontend_mode4_active);
    sram_write_u32(40, frontend_frame_pending);
    sram_write_u32(
        44,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(48, mmActive());
    sram_write_u32(52, autotest_demo_input_streams);
    sram_write_u32(56, FRONTEND_DEMO_IDLE_VBLANKS);
    sram_write_u32(60, MSL_NSONGS);
    __asm__ volatile("swi 3");
}

static void autotest_demo_maybe_finish(void)
{
    if (frontend_demo_active) {
        if (
            (autotest_demo_phase == 0 &&
             telemetry_demo_idle_starts == 1) ||
            autotest_demo_phase == 1
        ) {
            u8 demo_number = (u8)(autotest_demo_completed + 1u);

            autotest_demo_validate_header(demo_number);
            autotest_demo_phase = 2;
            autotest_demo_play_frames = 0;
            autotest_demo_nonzero_keys_seen = 0;
        }
        if (autotest_demo_phase == 2) {
            if (frontend_demo_keys != 0) {
                autotest_demo_nonzero_keys_seen = 1;
            }
            autotest_demo_play_frames++;
            if (autotest_demo_play_frames >= 120) {
                autotest_demo_checks =
                    autotest_demo_checks &&
                    !insert_coin_tile_upload_pending &&
                    autotest_demo_nonzero_keys_seen;
                if (autotest_demo_nonzero_keys_seen) {
                    autotest_demo_input_streams++;
                }
                autotest_demo_completed++;
                frontend_demo_finish(1);
                autotest_demo_phase =
                    autotest_demo_completed >=
                        FRONTEND_DEMO_COUNT ?
                            3 :
                            1;
            }
        }
        return;
    }
    if (
        autotest_demo_phase == 3 &&
        game_state == STATE_TITLE &&
        !frontend_frame_pending
    ) {
        autotest_demo_finish_to_sram();
    }
}
#endif
