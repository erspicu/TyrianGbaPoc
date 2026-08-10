#ifdef AUTOTEST_UPGRADE_SIDEKICK_STRESS

#if defined(AUTOTEST_UPGRADE_SHIP_ONLY) || \
    defined(AUTOTEST_UPGRADE_SHIELD_ONLY)
#define AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
#endif

#ifdef AUTOTEST_UPGRADE_SHIP_ONLY
#define AUTOTEST_UPGRADE_FIRST_CATEGORY FRONTEND_UPGRADE_SHIP
#define AUTOTEST_UPGRADE_SECOND_CATEGORY FRONTEND_UPGRADE_SHIP
#elif defined(AUTOTEST_UPGRADE_SHIELD_ONLY)
#define AUTOTEST_UPGRADE_FIRST_CATEGORY FRONTEND_UPGRADE_SHIELD
#define AUTOTEST_UPGRADE_SECOND_CATEGORY FRONTEND_UPGRADE_SHIELD
#else
#define AUTOTEST_UPGRADE_FIRST_CATEGORY FRONTEND_UPGRADE_LEFT_SIDEKICK
#define AUTOTEST_UPGRADE_SECOND_CATEGORY FRONTEND_UPGRADE_RIGHT_SIDEKICK
#endif

enum {
    AUTOTEST_UPGRADE_SIDEKICK_MOVES_PER_SIDE = 300,
    AUTOTEST_UPGRADE_SIDEKICK_SETTLE_VBLANKS = 30,
    AUTOTEST_UPGRADE_SIDEKICK_EXIT_VBLANKS = 90,
};

static u8 autotest_upgrade_sidekick_phase;
static u8 autotest_upgrade_sidekick_settle;
static u16 autotest_upgrade_sidekick_moves;
static u32 autotest_upgrade_sidekick_last_vblank;
static u32 autotest_upgrade_sidekick_vblank_start;
static u32 autotest_upgrade_sidekick_move_cycles_total;
static u32 autotest_upgrade_sidekick_move_cycles_max;
static u32 autotest_upgrade_sidekick_sound_starts;
static u32 autotest_upgrade_sidekick_projectile_dma_calls;
static u32 autotest_upgrade_sidekick_projectile_upload_bytes;
static u32 autotest_upgrade_sidekick_loadout_refreshes;
static u32 autotest_upgrade_sidekick_ship_tile_builds;
static u32 autotest_upgrade_sidekick_pending_slots_max;
static u32 autotest_upgrade_sidekick_active_shots_max;
static u32 autotest_upgrade_sidekick_oam_max;
static u32 autotest_upgrade_sidekick_exit_vblank_start;
static u32 autotest_upgrade_recolour_words[64];
static u8 autotest_upgrade_recolour_asm_ok;

static u8 autotest_upgrade_recolour_expected(u8 value, u8 selected)
{
    if (selected) {
        if (value == 0xfau) return 0xfeu;
        if (value == 0xf8u) return 0xfcu;
    } else {
        if (value == 0xfeu) return 0xfau;
        if (value == 0xfcu) return 0xf8u;
    }
    return value;
}

static u8 autotest_upgrade_recolour_differential(void)
{
    u8 selected;

    for (selected = 0; selected < 2; selected++) {
        u16 value;

        for (value = 0; value < 256; value++) {
            ((u8 *)(void *)autotest_upgrade_recolour_words)[value] =
                (u8)value;
        }
        frontend_upgrade_recolour_words_asm(
            autotest_upgrade_recolour_words,
            64,
            selected
        );
        for (value = 0; value < 256; value++) {
            if (
                ((u8 *)(void *)autotest_upgrade_recolour_words)[value] !=
                autotest_upgrade_recolour_expected((u8)value, selected)
            ) {
                return 0;
            }
        }
    }
    return 1;
}

#ifndef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
static u32 autotest_upgrade_sidekick_popcount(u32 value)
{
    u32 count = 0;

    while (value != 0) {
        value &= value - 1u;
        count++;
    }
    return count;
}
#endif

static void autotest_upgrade_sidekick_capture_sim(void)
{
#ifdef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
    return;
#else
    u32 pending = autotest_upgrade_sidekick_popcount(
        frontend_upgrade_sim->projectile_pending_mask
    );
    u32 active_shots = 0;
    u8 index;

    for (index = 0; index < FRONTEND_UPGRADE_SIM_SHOT_COUNT; index++) {
        if (frontend_upgrade_sim->shot[index].active) active_shots++;
    }
    if (pending > autotest_upgrade_sidekick_pending_slots_max) {
        autotest_upgrade_sidekick_pending_slots_max = pending;
    }
    if (active_shots > autotest_upgrade_sidekick_active_shots_max) {
        autotest_upgrade_sidekick_active_shots_max = active_shots;
    }
    if (previous_oam_count > autotest_upgrade_sidekick_oam_max) {
        autotest_upgrade_sidekick_oam_max = previous_oam_count;
    }
#endif
}

static void autotest_upgrade_sidekick_accumulate_sim(void)
{
#ifndef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
    autotest_upgrade_sidekick_sound_starts +=
        frontend_upgrade_sim->telemetry_sound_starts;
    autotest_upgrade_sidekick_projectile_dma_calls +=
        frontend_upgrade_sim->telemetry_projectile_dma_calls;
    autotest_upgrade_sidekick_projectile_upload_bytes +=
        frontend_upgrade_sim->telemetry_projectile_upload_bytes;
    autotest_upgrade_sidekick_loadout_refreshes +=
        frontend_upgrade_sim->telemetry_loadout_refreshes;
    autotest_upgrade_sidekick_ship_tile_builds +=
        frontend_upgrade_sim->telemetry_ship_tile_builds;
#endif
}

static u8 autotest_upgrade_submenu_ready(void)
{
    if (game_state != STATE_UPGRADE_SUBMENU) return 0;
#ifdef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
    return !frontend_upgrade_sim_is_active();
#else
    return frontend_upgrade_sim_is_active();
#endif
}

static s8 autotest_upgrade_move_direction(u16 move)
{
#ifdef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
    /* Exercise the exact rapid Up/Down toggle reported by the user. */
    return (move & 1u) != 0 ? -1 : 1;
#else
    u16 local = (u16)(
        move % AUTOTEST_UPGRADE_SIDEKICK_MOVES_PER_SIDE
    );

    return local < AUTOTEST_UPGRADE_SIDEKICK_MOVES_PER_SIDE / 2u ?
        1 : -1;
#endif
}

static void autotest_upgrade_sidekick_begin_category(u8 category)
{
    if (!frontend_upgrade_begin_submenu(category)) {
        volatile u8 *sram = (volatile u8 *)0x0e000000;

        sram[0] = 'T';
        sram[1] = 'G';
        sram[2] = 'U';
        sram[3] = 'F';
        sram_write_u32(4, category);
        __asm__ volatile("swi 3");
    }
    frontend_start_upgrade_submenu_transition(frontend_selection);
    frontend_upgrade_apply_preview();
}

static void autotest_upgrade_sidekick_reset_counters(u32 current_vblank)
{
    telemetry_missed_vblanks = 0;
    telemetry_frontend_full_redraws = 0;
    telemetry_frontend_dirty_commits = 0;
    telemetry_frontend_dirty_bytes = 0;
    telemetry_frontend_dma_calls = 0;
    telemetry_frontend_dma_bytes = 0;
    telemetry_frontend_runtime_shp_decodes = 0;
    telemetry_frontend_runtime_sprite2_decodes = 0;
    telemetry_frontend_upgrade_ship_cache_hits = 0;
    telemetry_frontend_upgrade_ship_cache_misses = 0;
    telemetry_frontend_upgrade_ship_cache_swaps = 0;
    telemetry_frontend_prepared_flips = 0;
    telemetry_frontend_prepared_flip_outside_vblank = 0;
    telemetry_frontend_prepared_flip_vcount_max = 0;
    telemetry_frontend_prepared_flip_vcount_last = 0;
    telemetry_perf_commit_cycles_max = 0;
    telemetry_perf_audio_input_cycles_max = 0;
    autotest_upgrade_sidekick_vblank_start = telemetry_vblank_irqs;
    autotest_upgrade_sidekick_last_vblank = current_vblank;
    last_vblank_seen = current_vblank;
}

static void autotest_upgrade_sidekick_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0e000000;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'U';
    sram[3] =
#ifdef AUTOTEST_UPGRADE_SHIP_ONLY
        'H';
#elif defined(AUTOTEST_UPGRADE_SHIELD_ONLY)
        'D';
#else
        'S';
#endif
    sram_write_u32(4, 1);
    sram_write_u32(8, autotest_upgrade_sidekick_moves);
    sram_write_u32(12, telemetry_missed_vblanks);
    sram_write_u32(
        16,
        telemetry_vblank_irqs - autotest_upgrade_sidekick_vblank_start
    );
    sram_write_u32(20, telemetry_frontend_full_redraws);
    sram_write_u32(24, telemetry_frontend_dirty_commits);
    sram_write_u32(28, telemetry_frontend_dirty_bytes);
    sram_write_u32(32, telemetry_frontend_dma_calls);
    sram_write_u32(36, telemetry_frontend_dma_bytes);
    sram_write_u32(40, telemetry_frontend_runtime_shp_decodes);
    sram_write_u32(44, telemetry_frontend_runtime_sprite2_decodes);
    sram_write_u32(48, autotest_upgrade_sidekick_move_cycles_total);
    sram_write_u32(52, autotest_upgrade_sidekick_move_cycles_max);
    sram_write_u32(56, autotest_upgrade_sidekick_sound_starts);
    sram_write_u32(60, autotest_upgrade_sidekick_projectile_dma_calls);
    sram_write_u32(64, autotest_upgrade_sidekick_projectile_upload_bytes);
    sram_write_u32(68, autotest_upgrade_sidekick_loadout_refreshes);
    sram_write_u32(72, autotest_upgrade_sidekick_ship_tile_builds);
    sram_write_u32(76, autotest_upgrade_sidekick_pending_slots_max);
    sram_write_u32(80, autotest_upgrade_sidekick_active_shots_max);
    sram_write_u32(84, autotest_upgrade_sidekick_oam_max);
    sram_write_u32(88, telemetry_perf_commit_cycles_max);
    sram_write_u32(92, telemetry_perf_audio_input_cycles_max);
    sram_write_u32(96, mmActive());
    sram_write_u32(100, game_state);
    sram_write_u32(104, frontend_upgrade_sim_is_active());
#ifdef AUTOTEST_STACK_CANARY
    sram_write_u32(108, autotest_stack_canary_remaining());
    sram_write_u32(
        112,
        autotest_stack_canary_remaining() >= sizeof(u32) * 8u
    );
#endif
    sram_write_u32(116, frontend_player_items.sidekick[0]);
    sram_write_u32(120, frontend_player_items.sidekick[1]);
    sram_write_u32(124, frontend_player_items.weapon[0].id);
    sram_write_u32(128, frontend_player_items.weapon[1].id);
    sram_write_u32(132, frontend_player_items.weapon[0].power);
    sram_write_u32(136, frontend_player_items.weapon[1].power);
    sram_write_u32(140, autotest_upgrade_recolour_asm_ok);
    sram_write_u32(144, telemetry_frontend_upgrade_ship_cache_hits);
    sram_write_u32(148, telemetry_frontend_upgrade_ship_cache_misses);
    sram_write_u32(152, telemetry_frontend_upgrade_ship_cache_swaps);
    sram_write_u32(156, telemetry_frontend_prepared_flips);
    sram_write_u32(
        160,
        telemetry_frontend_prepared_flip_outside_vblank
    );
    sram_write_u32(164, telemetry_frontend_prepared_flip_vcount_max);
    sram_write_u32(168, telemetry_frontend_prepared_flip_vcount_last);
    __asm__ volatile("swi 3");
}

static void autotest_upgrade_sidekick_stress_update(u32 current_vblank)
{
    if (autotest_upgrade_sidekick_phase == 0) {
        if (frontend_transition_busy() || frontend_frame_pending) return;
        autotest_upgrade_recolour_asm_ok =
            autotest_upgrade_recolour_differential();
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
        autotest_upgrade_sidekick_phase = 1;
        return;
    }
    if (frontend_transition_busy() || frontend_frame_pending) return;

    switch (autotest_upgrade_sidekick_phase) {
    case 1:
        if (game_state != STATE_GAME_MENU) return;
        frontend_start_upgrade_menu_transition(
            AUTOTEST_UPGRADE_FIRST_CATEGORY
        );
        autotest_upgrade_sidekick_phase = 2;
        break;

    case 2:
        if (game_state != STATE_UPGRADE_MENU) return;
        autotest_upgrade_sidekick_begin_category(
            AUTOTEST_UPGRADE_FIRST_CATEGORY
        );
        autotest_upgrade_sidekick_phase = 3;
        autotest_upgrade_sidekick_settle = 0;
        break;

    case 3:
        if (!autotest_upgrade_submenu_ready()) return;
        if (
            autotest_upgrade_sidekick_settle++ <
                AUTOTEST_UPGRADE_SIDEKICK_SETTLE_VBLANKS
        ) {
            return;
        }
        autotest_upgrade_sidekick_reset_counters(current_vblank);
        autotest_upgrade_sidekick_phase = 4;
        break;

    case 4:
        if (game_state != STATE_UPGRADE_SUBMENU) return;
        autotest_upgrade_sidekick_capture_sim();
        if (
            current_vblank == autotest_upgrade_sidekick_last_vblank
        ) {
            return;
        }
        autotest_upgrade_sidekick_last_vblank = current_vblank;
        if (
            autotest_upgrade_sidekick_moves <
                AUTOTEST_UPGRADE_SIDEKICK_MOVES_PER_SIDE
        ) {
            u32 cycle_start = stress_cycle_counter_read();
            u32 elapsed;

            frontend_upgrade_move_submenu(
                autotest_upgrade_move_direction(
                    autotest_upgrade_sidekick_moves
                )
            );
            elapsed = stress_cycle_counter_read() - cycle_start;
            autotest_upgrade_sidekick_move_cycles_total += elapsed;
            if (elapsed > autotest_upgrade_sidekick_move_cycles_max) {
                autotest_upgrade_sidekick_move_cycles_max = elapsed;
            }
            autotest_upgrade_sidekick_moves++;
            return;
        }
        autotest_upgrade_sidekick_accumulate_sim();
        frontend_upgrade_cancel_submenu();
        autotest_upgrade_sidekick_phase = 5;
        break;

    case 5:
        if (game_state != STATE_UPGRADE_MENU) return;
        autotest_upgrade_sidekick_begin_category(
            AUTOTEST_UPGRADE_SECOND_CATEGORY
        );
        autotest_upgrade_sidekick_phase = 6;
        autotest_upgrade_sidekick_settle = 0;
        break;

    case 6:
        if (!autotest_upgrade_submenu_ready()) return;
        if (
            autotest_upgrade_sidekick_settle++ <
                AUTOTEST_UPGRADE_SIDEKICK_SETTLE_VBLANKS
        ) {
            return;
        }
        autotest_upgrade_sidekick_phase = 7;
        break;

    case 7:
        if (game_state != STATE_UPGRADE_SUBMENU) return;
        autotest_upgrade_sidekick_capture_sim();
        if (
            current_vblank == autotest_upgrade_sidekick_last_vblank
        ) {
            return;
        }
        autotest_upgrade_sidekick_last_vblank = current_vblank;
        if (
            autotest_upgrade_sidekick_moves <
                AUTOTEST_UPGRADE_SIDEKICK_MOVES_PER_SIDE * 2u
        ) {
            u32 cycle_start = stress_cycle_counter_read();
            u32 elapsed;

            frontend_upgrade_move_submenu(
                autotest_upgrade_move_direction(
                    autotest_upgrade_sidekick_moves
                )
            );
            elapsed = stress_cycle_counter_read() - cycle_start;
            autotest_upgrade_sidekick_move_cycles_total += elapsed;
            if (elapsed > autotest_upgrade_sidekick_move_cycles_max) {
                autotest_upgrade_sidekick_move_cycles_max = elapsed;
            }
            autotest_upgrade_sidekick_moves++;
            return;
        }
#ifdef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
        /* Leave the static panel visible for the final PNG inspection. */
        autotest_upgrade_sidekick_finish();
        return;
#endif
        autotest_upgrade_sidekick_accumulate_sim();
        frontend_upgrade_cancel_submenu();
        autotest_upgrade_sidekick_exit_vblank_start = current_vblank;
        autotest_upgrade_sidekick_phase = 8;
        break;

    case 8:
        if (
            game_state != STATE_UPGRADE_MENU ||
            current_vblank - autotest_upgrade_sidekick_exit_vblank_start <
                AUTOTEST_UPGRADE_SIDEKICK_EXIT_VBLANKS
        ) {
            return;
        }
        autotest_upgrade_sidekick_finish();
        break;
    }
}

#endif

#undef AUTOTEST_UPGRADE_FIRST_CATEGORY
#undef AUTOTEST_UPGRADE_SECOND_CATEGORY
#undef AUTOTEST_UPGRADE_STATIC_PANEL_ONLY
