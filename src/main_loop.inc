static void telemetry_record_missed_vblank_state(u32 count)
{
    if (game_state == STATE_PLAY) {
        telemetry_missed_vblanks_play += count;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
        {
            u16 bucket = (u16)(
                level_position >>
                    ROUTE_MISSED_VBLANK_POSITION_BUCKET_SHIFT
            );

            if (
                bucket >=
                ROUTE_MISSED_VBLANK_POSITION_BUCKET_COUNT
            ) {
                bucket =
                    ROUTE_MISSED_VBLANK_POSITION_BUCKET_COUNT - 1;
            }
            if (
                telemetry_route_first_missed_vblank_position == 0
            ) {
                telemetry_route_first_missed_vblank_position =
                    level_position;
            }
            telemetry_route_last_missed_vblank_position =
                level_position;
            telemetry_route_missed_vblank_position_bucket[bucket] +=
                count;
#ifdef AUTOTEST_ROUTE_PERF
            if (
                route_perf_missed_record_count <
                ROUTE_PERF_MISSED_RECORD_COUNT
            ) {
                u8 record_index =
                    route_perf_missed_record_count++;
                RoutePerfMissedRecord *record =
                    &route_perf_missed_record[
                        record_index
                    ];
                u8 stage;

                record->level_position = level_position;
                record->missed_count = count;
                record->loop_cycles = route_perf_previous_loop_cycles;
                record->prelogic_cycles =
                    route_perf_previous_prelogic_cycles;
                record->logic_cycles =
                    route_perf_previous_logic_cycles;
                record->render_cycles =
                    route_perf_previous_render_cycles;
                record->prefetch_cycles =
                    route_perf_previous_prefetch_cycles;
                record->background_tile_renders =
                    route_perf_previous_background_tile_renders;
                record->sprite2_misses =
                    route_perf_previous_sprite2_misses;
                record->projectile_misses =
                    route_perf_previous_projectile_misses;
                record->effect_misses =
                    route_perf_previous_effect_misses;
                record->oam_count = route_perf_previous_oam_count;
                record->active_enemies =
                    route_perf_previous_active_enemies;
                record->active_enemy_shots =
                    route_perf_previous_active_enemy_shots;
                record->logic_updated =
                    route_perf_previous_logic_updated;
                record->rendered = route_perf_previous_rendered;
                for (
                    stage = 0;
                    stage < ROUTE_PERF_LOGIC_STAGE_COUNT;
                    stage++
                ) {
                    route_perf_missed_logic_stage[record_index][stage] =
                        route_perf_previous_logic_stage[stage];
                }
            }
#endif
        }
#endif
    } else if (game_state == STATE_GAME_OVER) {
        telemetry_missed_vblanks_game_over += count;
    } else {
        telemetry_missed_vblanks_frontend += count;
        if (game_state == STATE_LEVEL_STATS) {
            telemetry_missed_vblanks_stats += count;
        } else if (frontend_transition_busy()) {
            telemetry_missed_vblanks_transition += count;
            telemetry_missed_vblank_transition_job_last =
                frontend_transition_job;
            /*
             * The overrun belongs to the work completed in the previous
             * loop.  The job has already advanced this field, so recording
             * the next phase makes the causal phase unambiguous as N - 1.
             */
            telemetry_missed_vblank_transition_phase_next =
                frontend_transition_phase;
        } else {
            telemetry_missed_vblanks_frontend_other += count;
        }
    }
}

int main(void)
{
    u32 current_vblank;
    u32 vblank_steps;
    u8 logic_updated;
    u8 commit_vblank_now;
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
    u32 observed_vblank;
    u32 vblank_irq_latch_consumed = 0;
    u32 audio_vblank_seen;
#endif
#if TYRIAN_GBA_PERF_TIMER
    u32 stress_cycle_start;
    u32 perf_frame_cycle_start;
#endif
#ifdef AUTOTEST_ROUTE_PERF
    u32 route_perf_loop_start;
    u32 route_perf_phase_start;
    u32 route_perf_background_render_start;
    u32 route_perf_sprite2_miss_start;
    u32 route_perf_projectile_miss_start;
    u32 route_perf_effect_miss_start;
    u32 route_perf_current_prelogic_cycles;
    u32 route_perf_current_logic_cycles;
    u32 route_perf_current_render_cycles;
    u32 route_perf_current_prefetch_cycles;
    u8 route_perf_stage_index;
#endif
    uint32_t romfs_passed_checks = 0;
    uint32_t romfs_failed_checks = 0;
    const OtRomFs *mounted_romfs;

#ifdef AUTOTEST_STACK_CANARY
    autotest_stack_canary_begin();
#endif
    /*
     * WS0 3/1-cycle reads plus Game Pak prefetch.  Sprite2 raw data, ROMFS
     * and executable code all live in the Game Pak address space.
     */
    GBA_WAITCNT = GBA_WAITCNT_ROM_PREFETCH_3_1;
    telemetry_waitcnt = GBA_WAITCNT;
    irqInit();
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
    ot_rom_io_self_test(&romfs_passed_checks, &romfs_failed_checks);
    mounted_romfs = ot_rom_io_filesystem();
    telemetry_romfs_entries =
        mounted_romfs != 0 ? mounted_romfs->entry_count : 0;
    telemetry_romfs_image_bytes =
        mounted_romfs != 0 ? mounted_romfs->image_size : 0;
    telemetry_romfs_payload_bytes =
        mounted_romfs != 0 ? mounted_romfs->payload_bytes : 0;
    telemetry_romfs_checks =
        romfs_passed_checks + romfs_failed_checks;
    telemetry_romfs_failures = romfs_failed_checks;
    telemetry_romfs_manifest_crc32 =
        mounted_romfs != 0 ? mounted_romfs->manifest_crc32 : 0;
#ifdef AUTOTEST_ROMFS_LEVEL_MATRIX
    /*
     * Exercise every stock tyrianN.lvl section and every levelsN.dat route
     * on the actual GBA runtime.  This path deliberately runs before audio
     * or front-end setup: its only inputs are the original ROMFS files.
     */
    autotest_romfs_level_matrix();
#endif
    /*
     * Maxmod's GBA initializer waits across two display frames while its
     * VBlank routine primes the double-buffer write position.  The IRQ must
     * therefore be live before mmInitDefault(), or the first mmFrame() writes
     * through an uninitialized output pointer.
     */
    mmInitDefault((mm_addr)soundbank, 16);

#if TYRIAN_GBA_PERF_TIMER
    REG_TM2CNT_H = 0;
    REG_TM3CNT_H = 0;
    REG_TM2CNT_L = 0;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TIMER_COUNT | TIMER_START;
    REG_TM2CNT_H = TIMER_START;
    perf_timer_ready = 1;
#endif
    telemetry_vblank_irqs = 0;
    telemetry_state_transitions = 0;
    hide_all_sprites();
    frontend_begin();
#if defined(AUTOTEST_FRONTEND_CAPTURE_SECTION) && \
    defined(AUTOTEST_FRONTEND_CAPTURE_EPISODE)
    if (AUTOTEST_FRONTEND_CAPTURE_STATE == STATE_EPISODE_SCENE) {
        frontend_episode = AUTOTEST_FRONTEND_CAPTURE_EPISODE - 1;
        frontend_main_section = AUTOTEST_FRONTEND_CAPTURE_SECTION;
        frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
        frontend_campaign_initialize();
        if (!frontend_episode_scene_begin(
                frontend_main_section,
                FRONTEND_SCENE_RETURN_GAME_MENU,
                0
            )) {
            volatile u8 *failure_sram = (volatile u8 *)0x0e000000;

            failure_sram[0] = 'T';
            failure_sram[1] = 'G';
            failure_sram[2] = 'S';
            failure_sram[3] = 'F';
            __asm__ volatile("swi 3");
        }
    }
#endif
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
    /*
     * mmVBlank() owns the physical double-buffer cadence.  Recovery may run
     * several logical main-loop iterations after one already-observed IRQ,
     * but mmFrame() must never be replayed for those synthetic iterations:
     * doing so advances the mixer write pointer beyond its two buffers and
     * eventually overwrites Maxmod channel/instrument state.
     */
    audio_vblank_seen = telemetry_vblank_irqs;
#endif
#ifdef AUTOTEST_SAVE_FLOW
    autotest_save_flow();
#endif
#ifdef AUTOTEST_LOAD_SAVE_SLOT
    /*
     * Diagnostic route entry may reuse a real adjacent emulator SRAM file.
     * Loading through the production decoder/apply path keeps equipment,
     * campaign state and player resources identical to the reported save.
     */
    if (!frontend_save_apply_slot((u8)AUTOTEST_LOAD_SAVE_SLOT)) {
        volatile u8 *failure_sram = (volatile u8 *)0x0e000000;

        failure_sram[0] = 'T';
        failure_sram[1] = 'G';
        failure_sram[2] = 'S';
        failure_sram[3] = 'F';
        __asm__ volatile("swi 3");
    }
#endif
#if defined(AUTOTEST_SCRIPTED_SURVIVAL_FLOW) && \
    !defined(AUTOTEST_LOAD_SAVE_SLOT)
    /*
     * Keep the permanent Time War regression self-contained.  A separate
     * diagnostic build may still define AUTOTEST_LOAD_SAVE_SLOT to replay
     * an exact user SRAM image through the same production path.
     */
    frontend_episode = 3;
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_campaign_initialize();
#endif
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * Visual/test-only direct route entry.  Release builds always traverse
     * the original logo and menu sequence.
     */
#ifdef AUTOTEST_FRONTEND_ROUTE_EPISODE
    frontend_episode = AUTOTEST_FRONTEND_ROUTE_EPISODE - 1;
#else
    frontend_episode = 0;
#endif
    frontend_main_section = AUTOTEST_FRONTEND_ROUTE_SECTION;
#ifdef AUTOTEST_FRONTEND_ROUTE_ARCADE
    frontend_play_mode = FRONTEND_PLAY_ARCADE;
#endif
    if (frontend_prepare_map()) {
        frontend_enter_state(STATE_NEXT_LEVEL_MENU, 0);
    }
#endif

    for (;;) {
        logic_updated = 0;
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
        /*
         * A heavy logic/render pass can finish after one or more VBlank IRQs
         * have already run.  Do not call VBlankIntrWait() in that case: SWI 5
         * discards the latched IRQ and would sleep through one extra frame.
         * Consume one counted LCD period per loop so input and the wall-clock
         * logic accumulator recover at the correct 59.73 Hz.  Maxmod remains
         * tied to the newest physical IRQ and is deliberately not replayed
         * for these synthetic catch-up iterations.
         *
         * Active-display recovery loops must not DMA pending rows/OAM.  The
         * next genuinely waited VBlank commits the newest complete scene.
         */
        observed_vblank = telemetry_vblank_irqs;
        if (
            last_vblank_seen &&
            observed_vblank > last_vblank_seen
        ) {
            /*
             * BIOS keeps only a bit for an already-serviced VBlank.  Consume
             * that latch exactly once per newest IRQ count; ReturnFlag=0 is
             * guaranteed to return immediately here because the dispatcher
             * set it before invoking vblank_handler().  Without this step a
             * later wait would mistake the stale latch for a new LCD period.
             */
            if (observed_vblank > vblank_irq_latch_consumed) {
                IntrWait(0, IRQ_VBLANK);
                observed_vblank = telemetry_vblank_irqs;
                vblank_irq_latch_consumed = observed_vblank;
            }
            current_vblank = last_vblank_seen + 1;
            vblank_steps = 1;
            commit_vblank_now = 0;
            telemetry_missed_vblanks++;
            telemetry_record_missed_vblank_state(1);
            telemetry_vblank_recovery_loops++;
        } else {
            commit_vblank_now = 1;
            /*
             * ReturnFlag=0 also closes the small race between the counter
             * check above and entering BIOS: an IRQ arriving there is
             * consumed immediately instead of being discarded.
             */
            IntrWait(0, IRQ_VBLANK);
            observed_vblank = telemetry_vblank_irqs;
            vblank_irq_latch_consumed = observed_vblank;
            if (
                last_vblank_seen &&
                observed_vblank > last_vblank_seen
            ) {
                /*
                 * Normally this is exactly +1.  Consuming only one keeps the
                 * audio/logic accounting correct even if an emulator or an
                 * unusually long IRQ reports more than one at once.
                 */
                current_vblank = last_vblank_seen + 1;
            } else {
                current_vblank = observed_vblank;
            }
            vblank_steps = 1;
        }
#else
        commit_vblank_now = 1;
        VBlankIntrWait();
        current_vblank = telemetry_vblank_irqs;
        vblank_steps = 1;
        if (
            last_vblank_seen &&
            current_vblank > last_vblank_seen
        ) {
            vblank_steps = current_vblank - last_vblank_seen;
        }
        if (last_vblank_seen && current_vblank > last_vblank_seen + 1) {
            u32 missed =
                current_vblank - last_vblank_seen - 1;

            telemetry_missed_vblanks += missed;
            telemetry_record_missed_vblank_state(missed);
        }
#endif
        last_vblank_seen = current_vblank;
#if TYRIAN_GBA_PERF_TIMER
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
        perf_frame_cycle_start = perf_vblank_cycle_stamp;
        if (!perf_frame_cycle_start) {
            perf_frame_cycle_start = stress_cycle_counter_read();
        }
#else
        perf_frame_cycle_start = stress_cycle_counter_read();
#endif
        stress_cycle_start = stress_cycle_counter_read();
#endif
#ifdef AUTOTEST_ROUTE_PERF
        route_perf_loop_start = stress_cycle_counter_read();
        route_perf_background_render_start =
            background_tile_render_count;
        route_perf_sprite2_miss_start =
            telemetry_sprite2_cache_misses;
        route_perf_projectile_miss_start =
            telemetry_projectile_cache_misses;
        route_perf_effect_miss_start =
            telemetry_effect_cache_misses;
        route_perf_current_prelogic_cycles = 0;
        route_perf_current_logic_cycles = 0;
        route_perf_current_render_cycles = 0;
        route_perf_current_prefetch_cycles = 0;
        for (
            route_perf_stage_index = 0;
            route_perf_stage_index < ROUTE_PERF_LOGIC_STAGE_COUNT;
            route_perf_stage_index++
        ) {
            route_perf_current_logic_stage[
                route_perf_stage_index
            ] = 0;
        }
#endif
        if (commit_vblank_now) {
            commit_vblank_work();
        }
#if TYRIAN_GBA_PERF_TIMER
        if (commit_vblank_now) {
            stress_cycle_accumulate(
                stress_cycle_start,
                &telemetry_perf_commit_cycles_total,
                &telemetry_perf_commit_cycles_max
            );
        }
#endif
#ifdef AUTOTEST_SCREENSHOT_ENABLED
        if (autotest_screenshot_delay &&
            --autotest_screenshot_delay == 0) {
            __asm__ volatile("swi 3");
        }
#endif
#if TYRIAN_GBA_PERF_TIMER
        stress_cycle_start = stress_cycle_counter_read();
#endif
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
        if (observed_vblank != audio_vblank_seen) {
            /*
             * At most one mix pass is useful after a newly observed physical
             * VBlank.  If several IRQs elapsed while a heavy frame ran, the
             * missed audio buffers are already in the past; trying to replay
             * them now overruns Maxmod's live double buffer.
             */
            audio_vblank_seen = observed_vblank;
            audio_mix_physical_frame();
            audio_level_music_fade_in_update();
            if (
                game_state == STATE_PLAY ||
                game_state == STATE_GAME_OVER
            ) {
                telemetry_audio_frames++;
            }
        }
#else
        audio_mix_physical_frame();
        audio_level_music_fade_in_update();
        if (
            game_state == STATE_PLAY ||
            game_state == STATE_GAME_OVER
        ) {
            telemetry_audio_frames++;
        }
#endif

        scanKeys();
        pad_now = keysHeld();
        pad_pressed = keysDown();
        if (frontend_demo_active) {
            if (frontend_demo_input_guard > 0) {
                frontend_demo_input_guard--;
            } else if (pad_now != 0 || pad_pressed != 0) {
                frontend_demo_finish(1);
                pad_now = 0;
                pad_pressed = 0;
            }
        }
#if TYRIAN_GBA_PERF_TIMER
        stress_cycle_accumulate(
            stress_cycle_start,
            &telemetry_perf_audio_input_cycles_total,
            &telemetry_perf_audio_input_cycles_max
        );
        {
            u32 prelogic_elapsed =
                stress_cycle_counter_read() - perf_frame_cycle_start;

            telemetry_perf_prelogic_cycles_total += prelogic_elapsed;
            if (
                prelogic_elapsed >
                telemetry_perf_prelogic_cycles_max
            ) {
                telemetry_perf_prelogic_cycles_max = prelogic_elapsed;
            }
#ifdef AUTOTEST_ROUTE_PERF
            route_perf_current_prelogic_cycles = prelogic_elapsed;
#endif
        }
#endif
#ifdef AUTOTEST
        if (game_state == STATE_GAME_OVER) {
            pad_now = 0;
            pad_pressed = 0;
        } else if (game_state != STATE_PLAY) {
            if (!autotest_running) autotest_running = 1;
            pad_now = 0;
#ifdef AUTOTEST_JUKEBOX_FLOW
            pad_pressed = autotest_jukebox_input();
#elif defined(AUTOTEST_DEMO_FLOW)
            pad_pressed = autotest_demo_input();
#elif defined(AUTOTEST_EPISODE_WRAP_FLOW)
            pad_pressed = autotest_episode_wrap_input();
#elif defined(AUTOTEST_FRONTEND_STRESS)
            pad_pressed =
                game_state == STATE_GAME_MENU ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_NAV_STRESS)
            pad_pressed =
                game_state == STATE_NEXT_LEVEL_MENU ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_NAV_CAMERA_STRESS)
            pad_pressed =
                game_state == STATE_NEXT_LEVEL_MENU ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_TRANSITION_STRESS)
            pad_pressed =
                autotest_frontend_transition_ready ||
                game_state == STATE_GAME_MENU ?
                    0 :
                    KEY_A;
#elif defined(AUTOTEST_FRONTEND_CAPTURE_STATE)
            /*
             * Deterministically route to the requested front-end page.
             * The normal Game Menu cursor starts on "Play Next Level", so
             * captures of Upgrade Ship and Quit Game need the same explicit
             * menu choice a player would make before pressing A.
             */
            if (
                game_state == STATE_TITLE &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE == STATE_SAVE_SLOTS ||
                    AUTOTEST_FRONTEND_CAPTURE_STATE == STATE_SAVE_NAME
                )
            ) {
                /* Homepage Load Game is the production entry path. */
                frontend_selection = 1;
            } else if (
                game_state == STATE_SAVE_SLOTS &&
                AUTOTEST_FRONTEND_CAPTURE_STATE == STATE_SAVE_NAME
            ) {
                /* Exercise the GBA name editor without requiring SRAM. */
                frontend_save_slot_mode = FRONTEND_SAVE_MODE_SAVE;
            } else if (
                game_state == STATE_GAME_MENU &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_UPGRADE_MENU ||
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_UPGRADE_SUBMENU
                )
            ) {
                frontend_selection = 2;
            } else if (
                game_state == STATE_GAME_MENU &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_DATA_CUBES ||
                    AUTOTEST_FRONTEND_CAPTURE_STATE ==
                        STATE_DATA_CUBE_READER
                )
            ) {
                /*
                 * The stock Episode 1 pre-menu script exposes four cubes.
                 * Keep that naturally parsed campaign state intact so the
                 * visual regression catches ]?, ]! and ]+ timing failures.
                 */
                frontend_selection = 0;
            } else if (
                game_state == STATE_GAME_MENU &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_SHIP_SPECS
            ) {
                frontend_selection = 1;
            } else if (
                game_state == STATE_DATA_CUBES &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_DATA_CUBE_READER
            ) {
                frontend_selection = 0;
            } else if (
                game_state == STATE_UPGRADE_MENU &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_UPGRADE_SUBMENU
            ) {
                /*
                 * The front-weapon inventory has the longest labels and is
                 * the deterministic visual regression page for state 13.
                 */
                frontend_selection = 1;
            } else if (
                game_state == STATE_GAME_MENU &&
                AUTOTEST_FRONTEND_CAPTURE_STATE ==
                    STATE_QUIT_CONFIRM
            ) {
                frontend_selection = 5;
            }
            pad_pressed =
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE ?
                    0 :
                    KEY_A;
#else
            pad_pressed =
                game_state == STATE_LEVEL_STATS ?
                    (
                        frontend_stats_stage >=
                            FRONTEND_STATS_STAGE_FINAL &&
                        telemetry_end_level_music_natural_stops != 0 ?
                                KEY_A :
                                0
                    ) :
                    (
                        autotest_frontend_finish_pending ?
                            0 :
                            KEY_A
                    );
#endif
        } else {
            pad_now = autotest_input();
            pad_pressed = 0;
#ifndef AUTOTEST_FULL_LOADOUT_STRESS
            if (
                telemetry_display_frames == 120 ||
                telemetry_display_frames == 180
            ) {
                pad_pressed = KEY_START;
            }
#endif
        }
#endif

        if (game_state == STATE_GAME_OVER) {
            u8 game_over_logic_updates = 0;

            game_over_update();
            if (game_state == STATE_GAME_OVER) {
                /*
                 * OpenTyrian keeps executing level_loop while GAME OVER is
                 * overlaid; only the player is gone.  Advance the translated
                 * event/enemy/background loop at the same configured source
                 * cadence instead of freezing the final death frame.
                 */
                telemetry_wall_vblanks += vblank_steps;
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                logic_accumulator +=
                    vblank_steps * TYRIAN_GBA_LOGIC_NUMERATOR;
#else
                logic_accumulator += TYRIAN_GBA_LOGIC_NUMERATOR;
#endif
                while (
                    logic_accumulator >=
                        TYRIAN_GBA_LOGIC_DENOMINATOR
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                    &&
                    game_over_logic_updates <
                        PRESENTATION_MAX_CATCHUP_TICKS
#else
                    &&
                    game_over_logic_updates == 0
#endif
                ) {
                    logic_accumulator -=
                        TYRIAN_GBA_LOGIC_DENOMINATOR;
#ifdef AUTOTEST_ROUTE_PERF
                    route_perf_phase_start =
                        stress_cycle_counter_read();
#endif
                    update_logic();
#ifdef AUTOTEST_ROUTE_PERF
                    route_perf_current_logic_cycles +=
                        stress_cycle_counter_read() -
                            route_perf_phase_start;
#endif
                    game_over_logic_updates++;
                }
                render_game();
                background_prefetch_step(
                    game_over_logic_updates != 0 ?
                        0 :
                        BACKGROUND_PREFETCH_FRAME_BUDGET(
                            BACKGROUND_PREFETCH_IDLE_MISSES
                        )
                );
                telemetry_display_frames++;
#ifdef AUTOTEST_DEATH_FLOW
                autotest_death_update();
#endif
#ifdef AUTOTEST_SCRIPTED_SURVIVAL_FLOW
                autotest_scripted_survival_update();
#endif
            }
        } else if (game_state == STATE_JUKEBOX) {
            jukebox_update();
            if (game_state == STATE_JUKEBOX) {
                jukebox_render();
                telemetry_display_frames++;
            }
        } else if (game_state != STATE_PLAY) {
#ifdef AUTOTEST_BOOT_ONLY
            u8 old_state = game_state;
#endif
            if (frontend_transition_busy()) {
#if TYRIAN_GBA_PERF_TIMER
                u32 frontend_transition_cycle_start =
                    stress_cycle_counter_read();
                u8 frontend_transition_phase_before =
                    frontend_transition_phase;
#endif
                frontend_transition_update();
#if TYRIAN_GBA_PERF_TIMER
                {
                    u32 elapsed =
                        stress_cycle_counter_read() -
                        frontend_transition_cycle_start;

                    if (
                        elapsed >
                            telemetry_frontend_transition_job_cycles_max
                    ) {
                        telemetry_frontend_transition_job_cycles_max =
                            elapsed;
                    }
                    if (
                        frontend_transition_phase_before <
                            FRONTEND_TRANSITION_PHASE_TELEMETRY_COUNT &&
                        elapsed >
                            telemetry_frontend_transition_phase_cycles_max[
                                frontend_transition_phase_before
                            ]
                    ) {
                        telemetry_frontend_transition_phase_cycles_max[
                            frontend_transition_phase_before
                        ] = elapsed;
                    }
                }
#endif
            } else {
                frontend_update();
            }
#ifdef AUTOTEST_FRONTEND_CAPTURE_STATE
            static u8 frontend_capture_armed;
            static u8 frontend_capture_settle_frames;

            if (
                game_state == AUTOTEST_FRONTEND_CAPTURE_STATE &&
                !frontend_transition_busy() &&
                (
                    AUTOTEST_FRONTEND_CAPTURE_STATE !=
                        STATE_LEVEL_STATS ||
                    (
                        frontend_stats_stage >=
                            FRONTEND_STATS_STAGE_FINAL &&
                        frontend_stats_timer == 0
                    )
                )
            ) {
                if (!frontend_capture_armed) {
#ifdef AUTOTEST_FRONTEND_CAPTURE_SELECTION
                    frontend_selection =
                        AUTOTEST_FRONTEND_CAPTURE_SELECTION;
#endif
#ifdef AUTOTEST_FRONTEND_CAPTURE_CUBES
                    frontend_data_cube_count =
                        AUTOTEST_FRONTEND_CAPTURE_CUBES > 4 ?
                            4 :
                            AUTOTEST_FRONTEND_CAPTURE_CUBES;
#endif
#if defined(AUTOTEST_FRONTEND_CAPTURE_SELECTION) || \
    defined(AUTOTEST_FRONTEND_CAPTURE_CUBES)
                    frontend_redraw();
#endif
                    frontend_capture_armed = 1;
                } else if (
                    frontend_capture_armed == 1 &&
                    !frontend_frame_pending
                ) {
                    /*
                     * The page-select write happens during this VBlank.
                     * Let the PPU rasterize one complete frame before the
                     * headless screenshot hook stops execution.
                     */
                    frontend_capture_armed = 2;
                } else if (
                    frontend_capture_armed == 2 &&
                    (
                        AUTOTEST_FRONTEND_CAPTURE_STATE !=
                            STATE_UPGRADE_SUBMENU ||
                        frontend_capture_settle_frames++ >= 30
                    )
                ) {
#if defined(AUTOTEST_FRONTEND_CAPTURE_STATE)
                    if (
                        AUTOTEST_FRONTEND_CAPTURE_STATE ==
                            STATE_UPGRADE_SUBMENU
                    ) {
                        volatile u8 *sram =
                            (volatile u8 *)0x0E000000;
                        u16 tile_nonzero = 0;
                        u16 tile_index;

                        /*
                         * Deterministic source-parity diagnostics for the
                         * hardware Upgrade simulator.  Keep this outside
                         * the normal save header so capture ROMs can prove
                         * that HDT selection, Sprite2 composition and OAM
                         * presentation all reached the final rasterized
                         * frame independently of a visual inspection.
                         */
                        for (
                            tile_index = 0;
                            tile_index <
                                FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES;
                            tile_index++
                        ) {
                            if (
                                frontend_upgrade_sim->ship_tiles[
                                    tile_index
                                ] != 0
                            ) {
                                tile_nonzero++;
                            }
                        }
                        sram[0x100] = 'T';
                        sram[0x101] = 'G';
                        sram[0x102] = 'U';
                        sram[0x103] = 'S';
                        sram[0x104] = frontend_player_items.ship;
                        sram[0x105] = (u8)(
                            frontend_upgrade_sim->ship.shipgraphic &
                            0xffu
                        );
                        sram[0x106] = (u8)(
                            frontend_upgrade_sim->ship.shipgraphic >> 8
                        );
                        sram[0x107] =
                            frontend_upgrade_sim->ship_tile_valid;
                        sram[0x108] = (u8)(tile_nonzero & 0xffu);
                        sram[0x109] = (u8)(tile_nonzero >> 8);
                        sram[0x10a] = previous_oam_count;
                        sram[0x10b] = oam_count;
                        sram[0x10c] = (u8)(oam_shadow[0].attr0 & 0xffu);
                        sram[0x10d] = (u8)(oam_shadow[0].attr0 >> 8);
                        sram[0x10e] = (u8)(oam_shadow[0].attr1 & 0xffu);
                        sram[0x10f] = (u8)(oam_shadow[0].attr1 >> 8);
                        sram[0x110] = (u8)(oam_shadow[0].attr2 & 0xffu);
                        sram[0x111] = (u8)(oam_shadow[0].attr2 >> 8);
                    }
#endif
#ifdef AUTOTEST_DEATH_MUSIC_CHECK
                    const OtDataCatalog *catalog = ot_data_catalog();
                    volatile u8 *sram = (volatile u8 *)0x0E000000;

                    sram[0] = 'T';
                    sram[1] = 'G';
                    sram[2] = 'D';
                    sram[3] = 'M';
                    sram_write_u32(
                        4,
                        catalog ? catalog->selected_mus_song : 0xffffu
                    );
                    sram_write_u32(8, mmActive());
                    sram_write_u32(12, game_state);
                    sram_write_u32(
                        16,
                        TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
                    );
#endif
                    __asm__ volatile("swi 3");
                }
            }
#endif
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
            autotest_frontend_transition_update(current_vblank);
#endif
#ifdef AUTOTEST_FRONTEND_NAV_CAMERA_STRESS
            {
                static u8 frontend_nav_camera_started;
                static u8 frontend_nav_camera_direction;
                static u8 frontend_nav_camera_transitions;
                static s16 frontend_nav_camera_anchor_x;
                static s16 frontend_nav_camera_anchor_y;
                static u32 frontend_nav_camera_vblank_start;

                if (game_state == STATE_NEXT_LEVEL_MENU) {
                    if (!frontend_nav_camera_started) {
                        if (
                            !frontend_frame_pending &&
                            frontend_nav_obj_active &&
                            frontend_nav_x == frontend_nav_target_x &&
                            frontend_nav_y == frontend_nav_target_y &&
                            frontend_nav_presented_x == frontend_nav_x &&
                            frontend_nav_presented_y == frontend_nav_y
                        ) {
                            frontend_nav_camera_started = 1;
                            frontend_nav_camera_anchor_x =
                                frontend_nav_target_x;
                            frontend_nav_camera_anchor_y =
                                frontend_nav_target_y;
                            frontend_nav_camera_direction = 1;
                            frontend_nav_target_x = (s16)(
                                frontend_nav_camera_anchor_x + 45
                            );
                            frontend_nav_target_y = (s16)(
                                frontend_nav_camera_anchor_y + 30
                            );
                            telemetry_missed_vblanks = 0;
                            telemetry_frontend_full_redraws = 0;
                            telemetry_frontend_dirty_commits = 0;
                            telemetry_frontend_dirty_bytes = 0;
                            telemetry_frontend_runtime_shp_decodes = 0;
                            telemetry_frontend_runtime_sprite2_decodes = 0;
                            telemetry_frontend_nav_bitmap_redraws = 0;
                            telemetry_frontend_nav_obj_updates = 0;
                            telemetry_frontend_nav_obj_uploads = 0;
                            telemetry_frontend_nav_obj_upload_bytes = 0;
                            telemetry_frontend_nav_obj_overflows = 0;
                            frontend_nav_camera_vblank_start =
                                telemetry_vblank_irqs;
                            last_vblank_seen = current_vblank;
                        }
                    } else if (
                        !frontend_frame_pending &&
                        frontend_nav_x == frontend_nav_target_x &&
                        frontend_nav_y == frontend_nav_target_y &&
                        frontend_nav_presented_x == frontend_nav_x &&
                        frontend_nav_presented_y == frontend_nav_y
                    ) {
                        if (frontend_nav_camera_transitions < 40) {
                            frontend_nav_camera_transitions++;
                        }
                        if (frontend_nav_camera_transitions < 40) {
                            frontend_nav_camera_direction ^= 1u;
                            frontend_nav_target_x =
                                frontend_nav_camera_direction ?
                                    (s16)(
                                        frontend_nav_camera_anchor_x + 45
                                    ) :
                                    frontend_nav_camera_anchor_x;
                            frontend_nav_target_y =
                                frontend_nav_camera_direction ?
                                    (s16)(
                                        frontend_nav_camera_anchor_y + 30
                                    ) :
                                    frontend_nav_camera_anchor_y;
                        } else {
                            volatile u8 *sram =
                                (volatile u8 *)0x0E000000;

                            sram[0] = 'T';
                            sram[1] = 'G';
                            sram[2] = 'N';
                            sram[3] = 'C';
                            sram_write_u32(
                                4,
                                frontend_nav_camera_transitions
                            );
                            sram_write_u32(8, telemetry_missed_vblanks);
                            sram_write_u32(
                                12,
                                telemetry_vblank_irqs -
                                    frontend_nav_camera_vblank_start
                            );
                            sram_write_u32(16, frontend_frame_pending);
                            sram_write_u32(
                                20,
                                telemetry_frontend_full_redraws
                            );
                            sram_write_u32(
                                24,
                                telemetry_frontend_dirty_commits
                            );
                            sram_write_u32(
                                28,
                                telemetry_frontend_dirty_bytes
                            );
                            sram_write_u32(
                                32,
                                telemetry_frontend_runtime_shp_decodes
                            );
                            sram_write_u32(
                                36,
                                telemetry_frontend_runtime_sprite2_decodes
                            );
                            sram_write_u32(40, game_state);
                            sram_write_u32(
                                44,
                                telemetry_frontend_nav_bitmap_redraws
                            );
                            sram_write_u32(
                                48,
                                telemetry_frontend_nav_obj_updates
                            );
                            sram_write_u32(
                                52,
                                telemetry_frontend_nav_obj_uploads
                            );
                            sram_write_u32(
                                56,
                                telemetry_frontend_nav_obj_upload_bytes
                            );
                            sram_write_u32(
                                60,
                                telemetry_frontend_nav_obj_overflows
                            );
                            sram_write_u32(64, mmActive());
                            sram_write_u32(
                                68,
                                frontend_nav_obj_active
                            );
                            sram_write_u32(
                                72,
                                frontend_nav_obj_slot_count
                            );
                            sram_write_u32(
                                76,
                                frontend_nav_presented_x ==
                                        frontend_nav_x &&
                                    frontend_nav_presented_y ==
                                        frontend_nav_y
                            );
                            __asm__ volatile("swi 3");
                        }
                    }
                }
            }
#endif
#ifdef AUTOTEST_FRONTEND_NAV_STRESS
            {
                static u8 frontend_nav_stress_phase;
                static u16 frontend_nav_stress_updates;
                static u32 frontend_nav_stress_vblank_start;
                static u32 frontend_nav_stress_idle[16];

                if (game_state == STATE_NEXT_LEVEL_MENU) {
                    if (frontend_nav_stress_phase == 0) {
                        if (
                            !frontend_frame_pending &&
                            frontend_nav_obj_active &&
                            frontend_nav_x == frontend_nav_target_x &&
                            frontend_nav_y == frontend_nav_target_y &&
                            frontend_nav_presented_x == frontend_nav_x &&
                            frontend_nav_presented_y == frontend_nav_y
                        ) {
                            frontend_nav_stress_phase = 1;
                            telemetry_missed_vblanks = 0;
                            telemetry_frontend_full_redraws = 0;
                            telemetry_frontend_dirty_commits = 0;
                            telemetry_frontend_dirty_bytes = 0;
                            telemetry_frontend_runtime_shp_decodes = 0;
                            telemetry_frontend_runtime_sprite2_decodes = 0;
                            telemetry_frontend_nav_bitmap_redraws = 0;
                            telemetry_frontend_nav_obj_updates = 0;
                            telemetry_frontend_nav_obj_uploads = 0;
                            telemetry_frontend_nav_obj_upload_bytes = 0;
                            telemetry_frontend_nav_obj_overflows = 0;
                            frontend_nav_stress_vblank_start =
                                telemetry_vblank_irqs;
                            last_vblank_seen = current_vblank;
                        }
                    } else if (frontend_nav_stress_phase == 1) {
                        if (frontend_nav_stress_updates < 600) {
                            frontend_nav_stress_updates++;
                        } else {
                            frontend_nav_stress_idle[0] =
                                telemetry_missed_vblanks;
                            frontend_nav_stress_idle[1] =
                                telemetry_vblank_irqs -
                                    frontend_nav_stress_vblank_start;
                            frontend_nav_stress_idle[2] =
                                frontend_frame_pending;
                            frontend_nav_stress_idle[3] =
                                telemetry_frontend_full_redraws;
                            frontend_nav_stress_idle[4] =
                                telemetry_frontend_dirty_commits;
                            frontend_nav_stress_idle[5] =
                                telemetry_frontend_dirty_bytes;
                            frontend_nav_stress_idle[6] =
                                telemetry_frontend_runtime_shp_decodes;
                            frontend_nav_stress_idle[7] =
                                telemetry_frontend_runtime_sprite2_decodes;
                            frontend_nav_stress_idle[8] =
                                telemetry_frontend_nav_bitmap_redraws;
                            frontend_nav_stress_idle[9] =
                                telemetry_frontend_nav_obj_updates;
                            frontend_nav_stress_idle[10] =
                                telemetry_frontend_nav_obj_uploads;
                            frontend_nav_stress_idle[11] =
                                telemetry_frontend_nav_obj_upload_bytes;
                            frontend_nav_stress_idle[12] =
                                telemetry_frontend_nav_obj_overflows;
                            frontend_nav_stress_idle[13] = mmActive();
                            frontend_nav_stress_idle[14] =
                                frontend_nav_obj_active;
                            frontend_nav_stress_idle[15] =
                                frontend_nav_obj_slot_count;

                            frontend_nav_stress_phase = 2;
                            frontend_nav_stress_updates = 0;
                            telemetry_missed_vblanks = 0;
                            telemetry_frontend_full_redraws = 0;
                            telemetry_frontend_dirty_commits = 0;
                            telemetry_frontend_dirty_bytes = 0;
                            telemetry_frontend_runtime_shp_decodes = 0;
                            telemetry_frontend_runtime_sprite2_decodes = 0;
                            telemetry_frontend_nav_bitmap_redraws = 0;
                            telemetry_frontend_nav_obj_updates = 0;
                            telemetry_frontend_nav_obj_uploads = 0;
                            telemetry_frontend_nav_obj_upload_bytes = 0;
                            telemetry_frontend_nav_obj_overflows = 0;
                            frontend_nav_stress_vblank_start =
                                telemetry_vblank_irqs;
                            last_vblank_seen = current_vblank;
                        }
                    } else if (
                        frontend_nav_stress_updates < 120 &&
                        !frontend_frame_pending
                    ) {
                        u8 old_selection = frontend_selection;

                        frontend_selection =
                            frontend_selection == 0 ?
                                frontend_next_choice_count() :
                                0;
                        frontend_render_next_level_selection(
                            old_selection
                        );
                        frontend_nav_stress_updates++;
                    } else if (
                        frontend_nav_stress_updates == 120 &&
                        !frontend_frame_pending
                    ) {
                        volatile u8 *sram =
                            (volatile u8 *)0x0E000000;

                        sram[0] = 'T';
                        sram[1] = 'G';
                        sram[2] = 'N';
                        sram[3] = '7';
                        sram_write_u32(4, 600);
                        sram_write_u32(8, frontend_nav_stress_idle[0]);
                        sram_write_u32(12, frontend_nav_stress_idle[1]);
                        sram_write_u32(16, frontend_nav_stress_idle[2]);
                        sram_write_u32(20, frontend_nav_stress_idle[3]);
                        sram_write_u32(24, frontend_nav_stress_idle[4]);
                        sram_write_u32(28, frontend_nav_stress_idle[5]);
                        sram_write_u32(32, frontend_nav_stress_idle[6]);
                        sram_write_u32(36, frontend_nav_stress_idle[7]);
                        sram_write_u32(40, game_state);
                        sram_write_u32(44, frontend_selection);
                        sram_write_u32(48, frontend_nav_stress_idle[8]);
                        sram_write_u32(52, frontend_nav_stress_idle[9]);
                        sram_write_u32(56, frontend_nav_stress_idle[10]);
                        sram_write_u32(60, frontend_nav_stress_idle[11]);
                        sram_write_u32(64, frontend_nav_stress_idle[12]);
                        sram_write_u32(68, frontend_nav_stress_idle[13]);
                        sram_write_u32(72, frontend_nav_stress_idle[14]);
                        sram_write_u32(76, frontend_nav_stress_idle[15]);
                        sram_write_u32(80, frontend_nav_stress_updates);
                        sram_write_u32(84, telemetry_missed_vblanks);
                        sram_write_u32(
                            88,
                            telemetry_vblank_irqs -
                                frontend_nav_stress_vblank_start
                        );
                        sram_write_u32(92, frontend_frame_pending);
                        sram_write_u32(
                            96,
                            telemetry_frontend_full_redraws
                        );
                        sram_write_u32(
                            100,
                            telemetry_frontend_dirty_commits
                        );
                        sram_write_u32(
                            104,
                            telemetry_frontend_dirty_bytes
                        );
                        sram_write_u32(
                            108,
                            telemetry_frontend_runtime_shp_decodes
                        );
                        sram_write_u32(
                            112,
                            telemetry_frontend_runtime_sprite2_decodes
                        );
                        sram_write_u32(
                            116,
                            telemetry_frontend_nav_bitmap_redraws
                        );
                        sram_write_u32(
                            120,
                            telemetry_frontend_nav_obj_updates
                        );
                        sram_write_u32(
                            124,
                            telemetry_frontend_nav_obj_uploads
                        );
                        sram_write_u32(
                            128,
                            telemetry_frontend_nav_obj_upload_bytes
                        );
                        sram_write_u32(
                            132,
                            telemetry_frontend_nav_obj_overflows
                        );
                        sram_write_u32(136, mmActive());
                        __asm__ volatile("swi 3");
                    }
                }
            }
#endif
#ifdef AUTOTEST_FRONTEND_STRESS
            {
                static u8 frontend_stress_started;
                static u16 frontend_stress_updates;
                static u32 frontend_stress_vblank_start;

                if (game_state == STATE_GAME_MENU) {
                    if (!frontend_stress_started) {
                        if (!frontend_frame_pending) {
                            frontend_stress_started = 1;
                            telemetry_missed_vblanks = 0;
                            telemetry_frontend_full_redraws = 0;
                            telemetry_frontend_dirty_commits = 0;
                            telemetry_frontend_dirty_bytes = 0;
                            telemetry_frontend_runtime_shp_decodes = 0;
                            telemetry_frontend_runtime_sprite2_decodes = 0;
                            frontend_stress_vblank_start =
                                telemetry_vblank_irqs;
                            last_vblank_seen = current_vblank;
                        }
                    } else if (
                        frontend_stress_updates < 600 &&
                        !frontend_frame_pending
                    ) {
                        u8 old_selection = frontend_selection;

                        frontend_selection =
                            frontend_selection == 4 ? 5 : 4;
                        frontend_redraw_selection(old_selection);
                        frontend_stress_updates++;
                    } else if (
                        frontend_stress_updates == 600 &&
                        !frontend_frame_pending
                    ) {
                        volatile u8 *sram =
                            (volatile u8 *)0x0E000000;

                        sram[0] = 'T';
                        sram[1] = 'G';
                        sram[2] = 'F';
                        sram[3] = '5';
                        sram_write_u32(4, frontend_stress_updates);
                        sram_write_u32(8, telemetry_missed_vblanks);
                        sram_write_u32(
                            12,
                            telemetry_vblank_irqs -
                                frontend_stress_vblank_start
                        );
                        sram_write_u32(
                            16,
                            frontend_frame_pending
                        );
                        sram_write_u32(
                            20,
                            telemetry_frontend_full_redraws
                        );
                        sram_write_u32(
                            24,
                            telemetry_frontend_dirty_commits
                        );
                        sram_write_u32(
                            28,
                            telemetry_frontend_dirty_bytes
                        );
                        sram_write_u32(
                            32,
                            telemetry_frontend_runtime_shp_decodes
                        );
                        sram_write_u32(
                            36,
                            telemetry_frontend_runtime_sprite2_decodes
                        );
                        sram_write_u32(40, game_state);
                        sram_write_u32(44, frontend_selection);
                        __asm__ volatile("swi 3");
                    }
                }
            }
#endif
#ifdef AUTOTEST_DEATH_FLOW
            autotest_death_finish_update();
#endif
#ifdef AUTOTEST_SCRIPTED_SURVIVAL_FLOW
            autotest_scripted_survival_finish_update();
#endif
#ifdef AUTOTEST_EPISODE_WRAP_FLOW
            autotest_episode_wrap_finish_update();
#endif
#ifdef AUTOTEST
            if (
                autotest_frontend_finish_pending &&
                !frontend_frame_pending &&
                !frontend_transition_busy()
            ) {
                autotest_finish();
            }
#endif
#ifdef AUTOTEST_BOOT_ONLY
            if (old_state == STATE_NEXT_LEVEL_MENU &&
                game_state == STATE_PLAY) {
                __asm__ volatile("swi 3");
            }
#endif
        } else {
            u8 logic_updates_this_loop = 0;
            u8 rendered_this_loop = 0;
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
            u8 game_state_before_logic = game_state;
#endif

            telemetry_wall_vblanks += vblank_steps;
            if (
                game_state == STATE_PLAY &&
                (pad_pressed & KEY_START)
            ) {
                toggle_pause();
            }
            if (game_paused) {
                telemetry_paused_frames++;
                render_game();
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                presentation_render_pending = 0;
                presentation_pending_logic_ticks = 0;
#endif
                rendered_this_loop = 1;
#ifdef AUTOTEST_SCREENSHOT_PAUSE
                if (!autotest_screenshot_delay) {
                    autotest_screenshot_delay = 2;
                }
#endif
            } else {
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                logic_accumulator +=
                    vblank_steps * TYRIAN_GBA_LOGIC_NUMERATOR;
#else
                logic_accumulator += TYRIAN_GBA_LOGIC_NUMERATOR;
#endif
                while (
                    logic_accumulator >=
                        TYRIAN_GBA_LOGIC_DENOMINATOR
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                    &&
                    logic_updates_this_loop <
                        PRESENTATION_MAX_CATCHUP_TICKS
#else
                    &&
                    logic_updates_this_loop == 0
#endif
                ) {
                    logic_accumulator -= TYRIAN_GBA_LOGIC_DENOMINATOR;
                    logic_updated = 1;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    /*
                     * Wall-clock catch-up may execute more than one source
                     * tick after a delayed VBlank.  Regenerate the scripted
                     * input from the current authoritative state for each
                     * tick so the stress workload remains identical to the
                     * one-tick-per-loop baseline.
                     */
                    pad_now = autotest_input();
                    stress_cycle_start = stress_cycle_counter_read();
#endif
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
                    if (presentation_render_pending) {
                        /*
                         * This update supersedes a scene already waiting for
                         * presentation, so the scheduler will force a render
                         * afterward.  Let background streaming release the
                         * old display-only row before it allocates the new
                         * one; both VRAM and register changes commit together
                         * at the next VBlank.
                         */
                        presentation_release_held_window = 1;
                    }
#endif
#ifdef AUTOTEST_ROUTE_PERF
                    route_perf_phase_start =
                        stress_cycle_counter_read();
#endif
                    update_logic();
#ifdef AUTOTEST_ROUTE_PERF
                    route_perf_current_logic_cycles +=
                        stress_cycle_counter_read() -
                            route_perf_phase_start;
#endif
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    stress_cycle_accumulate(
                        stress_cycle_start,
                        &telemetry_stress_logic_cycles_total,
                        &telemetry_stress_logic_cycles_max
                    );
#endif
                    logic_updates_this_loop++;
                    /*
                     * keysDown() describes one physical sample, not every
                     * source tick recovered after an overrun.  Held input
                     * remains in pad_now; consume edge-triggered input only
                     * on the first catch-up update.
                     */
                    pad_pressed = 0;
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                    gameplay_presentation_mark_logic_update();
#endif
                    if (
                        game_state != STATE_PLAY &&
                        game_state != STATE_GAME_OVER
                    ) {
                        break;
                    }
#if !TYRIAN_GBA_DYNAMIC_FRAME_DROP
                    if (game_state == STATE_PLAY ||
                        game_state == STATE_GAME_OVER) {
                        u32 render_elapsed;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                        stress_cycle_start = stress_cycle_counter_read();
#endif
#ifdef AUTOTEST_ROUTE_PERF
                        route_perf_phase_start =
                            stress_cycle_counter_read();
#endif
                        render_game();
#ifdef AUTOTEST_ROUTE_PERF
                        route_perf_current_render_cycles +=
                            stress_cycle_counter_read() -
                                route_perf_phase_start;
#endif
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                        render_elapsed =
                            stress_cycle_counter_read() -
                            stress_cycle_start;
                        telemetry_stress_render_cycles_total +=
                            render_elapsed;
                        if (
                            render_elapsed >
                            telemetry_stress_render_cycles_max
                        ) {
                            telemetry_stress_render_cycles_max =
                                render_elapsed;
                        }
#else
                        render_elapsed = 0;
#endif
                        (void)render_elapsed;
                        telemetry_presentation_render_attempts++;
                        telemetry_presentation_render_completed++;
                        rendered_this_loop = 1;
                    }
#endif
                    if (game_state == STATE_PLAY) {
#ifdef AUTOTEST_SCREENSHOT_TICK
                        if (
                            !autotest_screenshot_delay &&
                            game_state == STATE_PLAY &&
                            level_tick >= AUTOTEST_SCREENSHOT_TICK
                        ) {
                            autotest_screenshot_delay = 3;
                        }
#endif
#ifdef AUTOTEST_SCREENSHOT_POSITION
                        if (
                            !autotest_screenshot_delay &&
                            game_state == STATE_PLAY &&
                            level_position >= AUTOTEST_SCREENSHOT_POSITION
                        ) {
                            /*
                             * Position-based capture makes the PC event
                             * sequence reproducible even when presentation
                             * frame counts change.
                             */
                            autotest_screenshot_delay = 3;
                        }
#endif
#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
                        if (
                            !autotest_screenshot_delay &&
                            autotest_explosion_visible()
                        ) {
                            autotest_screenshot_delay = 3;
                        }
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
                        if (
                            !autotest_screenshot_delay &&
                            autotest_reward_visible()
                        ) {
                            /*
                             * The first VBlank commits OAM; the second lets
                             * mGBA finish a rasterized frame before capture.
                             */
                            game_paused = 1;
                            autotest_screenshot_delay = 2;
                        }
#endif
                    }
                }
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                if (logic_updates_this_loop > 1) {
                    telemetry_logic_catchup_updates +=
                        logic_updates_this_loop - 1;
                }
                if (
                    logic_accumulator >=
                    TYRIAN_GBA_LOGIC_DENOMINATOR
                ) {
                    u32 backlog =
                        logic_accumulator /
                        TYRIAN_GBA_LOGIC_DENOMINATOR;

                    if (
                        backlog >
                        telemetry_logic_backlog_frames_max
                    ) {
                        telemetry_logic_backlog_frames_max =
                            backlog;
                    }
                }
#endif
                if (
                    logic_updates_this_loop >
                    telemetry_logic_updates_per_loop_max
                ) {
                    telemetry_logic_updates_per_loop_max =
                        logic_updates_this_loop;
                }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
                if (
                    presentation_render_pending &&
                    (
                        game_state == STATE_PLAY ||
                        game_state == STATE_GAME_OVER
                    )
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK
                    &&
                    current_vblank >= telemetry_vblank_irqs
#endif
                    &&
                    gameplay_presentation_should_render(
                        perf_frame_cycle_start,
                        game_state != game_state_before_logic
                    )
                ) {
                    u32 render_start =
                        stress_cycle_counter_read();
                    u32 render_elapsed;

                    render_game();
                    render_elapsed =
                        stress_cycle_counter_read() - render_start;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                    telemetry_stress_render_cycles_total +=
                        render_elapsed;
                    if (
                        render_elapsed >
                        telemetry_stress_render_cycles_max
                    ) {
                        telemetry_stress_render_cycles_max =
                            render_elapsed;
                    }
#endif
                    gameplay_presentation_complete(render_elapsed);
                    rendered_this_loop = 1;
                }
#endif
            }
            if (game_state == STATE_PLAY) {
#ifdef AUTOTEST_ROUTE_PERF
                route_perf_phase_start = stress_cycle_counter_read();
#endif
                background_prefetch_step(
                    logic_updated || rendered_this_loop ?
                        0 :
                        BACKGROUND_PREFETCH_FRAME_BUDGET(
                            BACKGROUND_PREFETCH_IDLE_MISSES
                        )
                );
#ifdef AUTOTEST_ROUTE_PERF
                route_perf_current_prefetch_cycles =
                    stress_cycle_counter_read() -
                        route_perf_phase_start;
#endif
            }
            telemetry_display_frames++;
#ifdef AUTOTEST
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            if (
                game_state == STATE_PLAY &&
#if TYRIAN_GBA_WALL_CLOCK_LOGIC
                telemetry_wall_vblanks >= 3600
#else
                telemetry_display_frames >= 3600
#endif
            ) {
                autotest_full_loadout_stress_finish();
            }
#endif
            if (
                autotest_running &&
                telemetry_display_frames >= 20000 &&
                game_state == STATE_PLAY
            ) {
                /*
                 * Deterministic deadlock watchdog: commit partial telemetry
                 * instead of leaving the headless verifier waiting forever.
                 */
                autotest_finish();
            }
#endif
        }
#ifdef AUTOTEST_ROUTE_PERF
        route_perf_previous_loop_cycles =
            stress_cycle_counter_read() - route_perf_loop_start;
        route_perf_previous_prelogic_cycles =
            route_perf_current_prelogic_cycles;
        route_perf_previous_logic_cycles =
            route_perf_current_logic_cycles;
        route_perf_previous_render_cycles =
            route_perf_current_render_cycles;
        route_perf_previous_prefetch_cycles =
            route_perf_current_prefetch_cycles;
        route_perf_previous_background_tile_renders =
            background_tile_render_count -
                route_perf_background_render_start;
        route_perf_previous_sprite2_misses =
            telemetry_sprite2_cache_misses -
                route_perf_sprite2_miss_start;
        route_perf_previous_projectile_misses =
            telemetry_projectile_cache_misses -
                route_perf_projectile_miss_start;
        route_perf_previous_effect_misses =
            telemetry_effect_cache_misses -
                route_perf_effect_miss_start;
        route_perf_previous_oam_count = previous_oam_count;
        route_perf_previous_active_enemies =
            source_parity_level.active_enemy_count;
        route_perf_previous_active_enemy_shots =
            source_parity_level.enemy_shot_active_count;
        route_perf_previous_logic_updated = logic_updated;
        route_perf_previous_rendered =
            route_perf_current_render_cycles != 0;
        for (
            route_perf_stage_index = 0;
            route_perf_stage_index < ROUTE_PERF_LOGIC_STAGE_COUNT;
            route_perf_stage_index++
        ) {
            route_perf_previous_logic_stage[
                route_perf_stage_index
            ] = route_perf_current_logic_stage[
                route_perf_stage_index
            ];
        }
#endif
#ifdef AUTOTEST_JUKEBOX_FLOW
        autotest_jukebox_maybe_finish();
#endif
#ifdef AUTOTEST_DEMO_FLOW
        autotest_demo_maybe_finish();
#endif
    }
}
