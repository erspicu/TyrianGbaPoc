#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS

/* Keep the release gate exhaustive while permitting short local smoke runs. */
#ifndef AUTOTEST_FRONTEND_TRANSITION_ITERATIONS
#define AUTOTEST_FRONTEND_TRANSITION_ITERATIONS 120
#endif

enum {
    FRONTEND_TRANSITION_PATH_GAME_UPGRADE = 0,
    FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE,
    FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE,
    FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY,
    FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME,
    FRONTEND_TRANSITION_PATH_GAME_NEXT,
    FRONTEND_TRANSITION_PATH_GAME_DATA,
    FRONTEND_TRANSITION_PATH_DATA_SELECTION,
    FRONTEND_TRANSITION_PATH_DATA_READER,
    FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS,
    FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU,
    FRONTEND_TRANSITION_PATH_GAME_QUIT,
    FRONTEND_TRANSITION_PATH_COUNT,
    FRONTEND_TRANSITION_ITERATIONS =
        AUTOTEST_FRONTEND_TRANSITION_ITERATIONS,
};

typedef struct {
    u32 transitions;
    u32 missed_vblanks;
    u32 vblank_irqs;
    u32 full_redraws;
    u32 dirty_commits;
    u32 dirty_bytes;
    u32 runtime_shp_decodes;
    u32 runtime_sprite2_decodes;
    u32 music_active;
    u32 max_cpu_cycles;
    u32 failures;
    u32 phase_cycles[FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT];
} AutotestFrontendTransitionResult;

static AutotestFrontendTransitionResult
    autotest_frontend_transition_result[
        FRONTEND_TRANSITION_PATH_COUNT
    ];
static u8 autotest_frontend_transition_phase;
static u8 autotest_frontend_transition_path;
static u16 autotest_frontend_transition_count;
static u32 autotest_frontend_transition_vblank_start;
static u32 autotest_frontend_transition_max_cycles;
static u32 autotest_frontend_transition_failures;

static void autotest_frontend_transition_counters_reset(
    u32 current_vblank
)
{
    telemetry_missed_vblanks = 0;
    telemetry_frontend_full_redraws = 0;
    telemetry_frontend_dirty_commits = 0;
    telemetry_frontend_dirty_bytes = 0;
    telemetry_frontend_runtime_shp_decodes = 0;
    telemetry_frontend_runtime_sprite2_decodes = 0;
    telemetry_frontend_transition_job_cycles_max = 0;
    memset(
        (void *)telemetry_frontend_transition_phase_cycles_max,
        0,
        sizeof(telemetry_frontend_transition_phase_cycles_max)
    );
    telemetry_state_transitions = 0;
    telemetry_frontend_quit_capture_cycles_max = 0;
    telemetry_frontend_quit_shade_cycles_max = 0;
    telemetry_frontend_quit_overlay_cycles_max = 0;
    telemetry_frontend_quit_choices_cycles_max = 0;
    autotest_frontend_transition_count = 0;
    autotest_frontend_transition_max_cycles = 0;
    autotest_frontend_transition_failures = 0;
    autotest_frontend_transition_vblank_start =
        telemetry_vblank_irqs;
    last_vblank_seen = current_vblank;
    if (
        autotest_frontend_transition_path ==
            FRONTEND_TRANSITION_PATH_DATA_READER
    ) {
        /*
         * Exercise the first reader open as well as its warm path.  A real
         * player can confirm immediately after entering Data, before the
         * list's idle-time font preparation has completed.
         */
        frontend_data_reader_font_ready = 0;
        frontend_data_reader_font_prepare_index = 0;
    }
}

static u8 autotest_frontend_transition_start_state(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
    case FRONTEND_TRANSITION_PATH_GAME_DATA:
    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        return STATE_GAME_MENU;
    case FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE:
        return STATE_TITLE;
    case FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE:
        return STATE_PLAY_MODE_MENU;
    case FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY:
        return STATE_EPISODE_MENU;
    case FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU:
        return STATE_UPGRADE_MENU;
    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
    case FRONTEND_TRANSITION_PATH_DATA_READER:
        return STATE_DATA_CUBES;
    default:
        return STATE_GAME_MENU;
    }
}

static u8 autotest_frontend_transition_start_selection(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
        return 2;
    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
        return 4;
    case FRONTEND_TRANSITION_PATH_GAME_DATA:
        return 0;
    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
        return 1;
    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        return 5;
    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
    case FRONTEND_TRANSITION_PATH_DATA_READER:
        return 0;
    default:
        return 0;
    }
}

static void autotest_frontend_transition_prepare_path(u8 path)
{
    u8 state = autotest_frontend_transition_start_state(path);
    u8 selection =
        autotest_frontend_transition_start_selection(path);

    if (
        path == FRONTEND_TRANSITION_PATH_DATA_SELECTION ||
        path == FRONTEND_TRANSITION_PATH_DATA_READER
    ) {
        if (
            game_state == STATE_DATA_CUBES &&
            frontend_selection == selection &&
            frontend_data_cache_valid &&
            frontend_data_page_count == 4
        ) {
            return;
        }
        if (game_state != STATE_GAME_MENU) {
            frontend_enter_state(STATE_GAME_MENU, 0);
        }
        frontend_start_data_cubes_transition(
            selection,
            FRONTEND_DATA_JOB_RELOAD |
                FRONTEND_DATA_JOB_FULL_FRAME
        );
        return;
    }
    if (game_state != state || frontend_selection != selection) {
        frontend_enter_state(state, selection);
    }
}

static void autotest_frontend_transition_step(u8 path)
{
    switch (path) {
    case FRONTEND_TRANSITION_PATH_GAME_UPGRADE:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_upgrade_menu_transition(0);
        } else {
            frontend_start_game_menu_transition_selected(0, 2);
        }
        break;

    case FRONTEND_TRANSITION_PATH_TITLE_PLAY_MODE:
        if (game_state == STATE_TITLE) {
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        } else {
            enter_title();
        }
        break;

    case FRONTEND_TRANSITION_PATH_PLAY_MODE_EPISODE:
        if (game_state == STATE_PLAY_MODE_MENU) {
            frontend_enter_state(STATE_EPISODE_MENU, 0);
        } else {
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_EPISODE_DIFFICULTY:
        if (game_state == STATE_EPISODE_MENU) {
            frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        } else {
            frontend_enter_state(STATE_EPISODE_MENU, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_DIFFICULTY_GAME:
        if (game_state == STATE_DIFFICULTY_MENU) {
            /*
             * Level completion changes cash/equipment, so returning to the
             * Game Menu must also prove the cold ship-panel path.  Force a
             * miss here instead of letting 120 identical default loadouts
             * exercise only the warm 19.2 KiB cache.
             */
            frontend_ship_panel_cache_valid = 0;
            frontend_start_game_menu_transition(
                FRONTEND_GAME_MENU_JOB_INITIALIZE_CAMPAIGN |
                FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
            );
        } else {
            frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_NEXT:
        if (game_state == STATE_GAME_MENU) {
            frontend_nav_reset();
            frontend_start_next_level_transition();
        } else {
            frontend_start_game_menu_transition(0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_DATA:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_data_cubes_transition(
                0,
                FRONTEND_DATA_JOB_RELOAD |
                    FRONTEND_DATA_JOB_FULL_FRAME
            );
        } else {
            /*
             * Episode 1 section 2 exposes exactly four records before ]I.
             * This catches regressions which apply ]?, ]! and ]+ only after
             * level entry instead of before the Game Menu, as JE_loadMap()
             * does on PC.
             */
            if (
                frontend_data_cube_count != 4 ||
                frontend_data_page_count != 4
            ) {
                autotest_frontend_transition_failures++;
            }
            frontend_data_cache_valid = 0;
            frontend_start_game_menu_transition_selected(0, 0);
        }
        break;

    case FRONTEND_TRANSITION_PATH_DATA_SELECTION:
        if (
            game_state != STATE_DATA_CUBES ||
            frontend_data_page_count != 4
        ) {
            autotest_frontend_transition_failures++;
            break;
        }
        frontend_start_data_selection_transition(1);
        break;

    case FRONTEND_TRANSITION_PATH_DATA_READER:
        if (game_state == STATE_DATA_CUBES) {
            if (frontend_selection >= frontend_data_page_count) {
                frontend_selection = 0;
            }
            frontend_start_data_reader_transition(
                frontend_selection
            );
        } else if (game_state == STATE_DATA_CUBE_READER) {
            frontend_start_data_cubes_transition(
                frontend_data_reader_index,
                0
            );
        } else {
            autotest_frontend_transition_failures++;
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_SHIP_SPECS:
        if (game_state == STATE_GAME_MENU) {
            frontend_start_ship_specs_transition();
        } else {
            frontend_start_game_menu_transition_selected(0, 1);
        }
        break;

    case FRONTEND_TRANSITION_PATH_UPGRADE_SUBMENU:
        if (game_state == STATE_UPGRADE_MENU) {
            if (
                !frontend_upgrade_begin_submenu(
                    FRONTEND_UPGRADE_FRONT_WEAPON
                )
            ) {
                autotest_frontend_transition_failures++;
                return;
            }
            frontend_start_upgrade_submenu_transition(
                frontend_selection
            );
            frontend_upgrade_apply_preview();
        } else {
            frontend_upgrade_cancel_submenu();
        }
        break;

    case FRONTEND_TRANSITION_PATH_GAME_QUIT:
        if (game_state == STATE_GAME_MENU) {
            frontend_quit_yes = 1;
            frontend_start_quit_transition(1);
        } else {
            frontend_quit_yes = 0;
            frontend_start_quit_transition(0);
        }
        break;

    default:
        autotest_frontend_transition_failures++;
        break;
    }
    audio_effect(SFX_CLICK);
}

static void autotest_frontend_transition_record(void)
{
    AutotestFrontendTransitionResult *result =
        &autotest_frontend_transition_result[
            autotest_frontend_transition_path
        ];

    result->transitions = autotest_frontend_transition_count;
    result->missed_vblanks = telemetry_missed_vblanks;
    result->vblank_irqs =
        telemetry_vblank_irqs -
        autotest_frontend_transition_vblank_start;
    result->full_redraws = telemetry_frontend_full_redraws;
    result->dirty_commits = telemetry_frontend_dirty_commits;
    result->dirty_bytes = telemetry_frontend_dirty_bytes;
    result->runtime_shp_decodes =
        telemetry_frontend_runtime_shp_decodes;
    result->runtime_sprite2_decodes =
        telemetry_frontend_runtime_sprite2_decodes;
    result->music_active = mmActive();
    result->max_cpu_cycles =
        autotest_frontend_transition_max_cycles >
            telemetry_frontend_transition_job_cycles_max ?
                autotest_frontend_transition_max_cycles :
                telemetry_frontend_transition_job_cycles_max;
    result->failures = autotest_frontend_transition_failures;
    memcpy(
        result->phase_cycles,
        (const void *)
            telemetry_frontend_transition_phase_cycles_max,
        sizeof(result->phase_cycles)
    );
}

static void autotest_frontend_transition_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 path;

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'F';
    sram[3] = 'A';
    sram_write_u32(4, 10);
    sram_write_u32(8, FRONTEND_TRANSITION_PATH_COUNT);
    sram_write_u32(12, FRONTEND_TRANSITION_ITERATIONS);
    for (path = 0; path < FRONTEND_TRANSITION_PATH_COUNT; path++) {
        const AutotestFrontendTransitionResult *result =
            &autotest_frontend_transition_result[path];
        u32 offset = 16u + (u32)path * 108u;
        u8 phase;

        sram_write_u32(offset + 0, result->transitions);
        sram_write_u32(offset + 4, result->missed_vblanks);
        sram_write_u32(offset + 8, result->vblank_irqs);
        sram_write_u32(offset + 12, result->full_redraws);
        sram_write_u32(offset + 16, result->dirty_commits);
        sram_write_u32(offset + 20, result->dirty_bytes);
        sram_write_u32(offset + 24, result->runtime_shp_decodes);
        sram_write_u32(
            offset + 28,
            result->runtime_sprite2_decodes
        );
        sram_write_u32(offset + 32, result->music_active);
        sram_write_u32(offset + 36, result->max_cpu_cycles);
        sram_write_u32(offset + 40, result->failures);
        for (
            phase = 0;
            phase < FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT;
            phase++
        ) {
            sram_write_u32(
                offset + 44u + (u32)phase * 4u,
                result->phase_cycles[phase]
            );
        }
    }
    {
        u32 offset =
            16u + FRONTEND_TRANSITION_PATH_COUNT * 108u;

        sram_write_u32(offset + 0, game_state);
        sram_write_u32(offset + 4, frontend_selection);
        sram_write_u32(offset + 8, frontend_frame_pending);
        sram_write_u32(offset + 12, frontend_pending_kind);
        sram_write_u32(
            offset + 16,
            telemetry_frontend_quit_capture_cycles_max
        );
        sram_write_u32(
            offset + 20,
            telemetry_frontend_quit_shade_cycles_max
        );
        sram_write_u32(
            offset + 24,
            telemetry_frontend_quit_overlay_cycles_max
        );
        sram_write_u32(
            offset + 28,
            telemetry_frontend_quit_choices_cycles_max
        );
#ifdef AUTOTEST_STACK_CANARY
        {
            u32 stack_remaining = autotest_stack_canary_remaining();

            sram_write_u32(offset + 32, stack_remaining);
            sram_write_u32(
                offset + 36,
                stack_remaining >= sizeof(u32) * 8u ? 1u : 0u
            );
            sram_write_u32(
                offset + 40,
                autotest_stack_canary_filled_bytes
            );
            sram_write_u32(offset + 44, autotest_ewram_heap_used());
            sram_write_u32(offset + 48, autotest_ewram_heap_remaining());
        }
#endif
    }
    __asm__ volatile("swi 3");
}

static void autotest_frontend_transition_update(u32 current_vblank)
{
    if (frontend_transition_busy()) return;
    if (
        autotest_frontend_transition_phase == 0 &&
        game_state == STATE_GAME_MENU &&
        !frontend_frame_pending
    ) {
        autotest_frontend_transition_ready = 1;
        autotest_frontend_transition_path = 0;
        autotest_frontend_transition_prepare_path(0);
        autotest_frontend_transition_phase = 1;
        return;
    }
    if (autotest_frontend_transition_phase == 1) {
        u8 expected_state =
            autotest_frontend_transition_start_state(
                autotest_frontend_transition_path
            );
        u8 expected_selection =
            autotest_frontend_transition_start_selection(
                autotest_frontend_transition_path
            );

        if (frontend_frame_pending) return;
        if (
            game_state != expected_state ||
            frontend_selection != expected_selection
        ) {
            autotest_frontend_transition_prepare_path(
                autotest_frontend_transition_path
            );
            return;
        }
        autotest_frontend_transition_counters_reset(
            current_vblank
        );
        autotest_frontend_transition_phase = 2;
        return;
    }
    if (autotest_frontend_transition_phase != 2) return;
    if (frontend_frame_pending) return;
    if (
        autotest_frontend_transition_count <
            FRONTEND_TRANSITION_ITERATIONS
    ) {
        u32 cycle_start = stress_cycle_counter_read();
        u32 elapsed;

        autotest_frontend_transition_step(
            autotest_frontend_transition_path
        );
        elapsed = stress_cycle_counter_read() - cycle_start;
        if (elapsed > autotest_frontend_transition_max_cycles) {
            autotest_frontend_transition_max_cycles = elapsed;
        }
        autotest_frontend_transition_count++;
        return;
    }

    autotest_frontend_transition_record();
    autotest_frontend_transition_path++;
    if (
        autotest_frontend_transition_path >=
            FRONTEND_TRANSITION_PATH_COUNT
    ) {
        autotest_frontend_transition_finish();
        return;
    }
    autotest_frontend_transition_prepare_path(
        autotest_frontend_transition_path
    );
    autotest_frontend_transition_phase = 1;
}

#endif
