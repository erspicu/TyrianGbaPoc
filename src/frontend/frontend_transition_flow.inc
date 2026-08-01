/*
 * Incremental static-menu transition engine.
 * Included by frontend_flow.inc before frontend_update().
 */

static u8 frontend_transition_busy(void)
{
    return frontend_transition_job != FRONTEND_TRANSITION_JOB_NONE;
}

static void frontend_transition_begin(
    u8 state,
    u8 selection,
    u8 job,
    u8 flags
)
{
    u8 old_state = game_state;

    if (
        old_state == STATE_NEXT_LEVEL_MENU &&
        state != STATE_NEXT_LEVEL_MENU
    ) {
        frontend_nav_obj_disable();
    }
    if (
        old_state == STATE_UPGRADE_SUBMENU &&
        state != STATE_UPGRADE_SUBMENU
    ) {
        frontend_upgrade_sim_disable();
    }
    if (state != STATE_LEVEL_STATS) {
        frontend_stats_overlay_active = 0;
        frontend_stats_tiles_pending = 0;
        frontend_stats_palette_dirty = 0;
        frontend_stats_font_ready = 0;
    }
    game_state = state;
    frontend_selection = selection;
    frontend_transition_job = job;
    frontend_transition_phase = 0;
    frontend_transition_flags = flags;
    frontend_transition_failed = 0;
    frontend_transition_work_index = 0;
    frontend_transition_work_mode = 0;
    if (state != STATE_SHIP_SPECS) {
        frontend_ship_specs_scale_active = 0;
        frontend_ship_specs_scale_step =
            FRONTEND_SHIP_SPECS_SCALE_STEPS;
    }
    telemetry_state_transitions++;
}

static void frontend_start_next_level_transition(void)
{
    frontend_transition_begin(
        STATE_NEXT_LEVEL_MENU,
        0,
        FRONTEND_TRANSITION_JOB_NEXT_LEVEL,
        0
    );
}

static void frontend_start_upgrade_submenu_transition(
    u8 selection
)
{
    frontend_transition_begin(
        STATE_UPGRADE_SUBMENU,
        selection,
        FRONTEND_TRANSITION_JOB_UPGRADE_SUBMENU,
        0
    );
}

static void frontend_start_game_menu_transition_selected(
    u8 flags,
    u8 selection
)
{
    frontend_transition_begin(
        STATE_GAME_MENU,
        selection,
        FRONTEND_TRANSITION_JOB_GAME_MENU,
        flags
    );
}

static void frontend_start_game_menu_transition(u8 flags)
{
    frontend_start_game_menu_transition_selected(flags, 4);
}

static void frontend_start_upgrade_menu_transition(u8 selection)
{
    frontend_transition_begin(
        STATE_UPGRADE_MENU,
        selection,
        FRONTEND_TRANSITION_JOB_UPGRADE_MENU,
        0
    );
}

static void frontend_start_quit_transition(u8 enter_dialog)
{
    frontend_transition_begin(
        enter_dialog ? STATE_QUIT_CONFIRM : STATE_GAME_MENU,
        enter_dialog ? 0 : 5,
        FRONTEND_TRANSITION_JOB_QUIT,
        enter_dialog ? FRONTEND_QUIT_JOB_ENTER : 0
    );
}

static void frontend_start_data_cubes_transition(
    u8 selection,
    u8 flags
)
{
    frontend_transition_begin(
        STATE_DATA_CUBES,
        selection,
        FRONTEND_TRANSITION_JOB_DATA_CUBES,
        flags
    );
}

static void frontend_start_data_selection_transition(s8 direction)
{
    u8 item_count = (u8)(frontend_data_page_count + 1u);
    u8 old_selection = frontend_selection;

    if (direction < 0) {
        frontend_selection =
            frontend_selection == 0 ?
                (u8)(item_count - 1u) :
                (u8)(frontend_selection - 1u);
    } else {
        frontend_selection =
            frontend_selection + 1u >= item_count ?
                0 :
                (u8)(frontend_selection + 1u);
    }
    frontend_patch_old_selection = old_selection;
    frontend_transition_job =
        FRONTEND_TRANSITION_JOB_DATA_CUBES;
    frontend_transition_phase = 0;
    frontend_transition_flags = FRONTEND_DATA_JOB_SELECTION;
    frontend_transition_failed = 0;
    frontend_transition_work_index = 0;
    frontend_transition_work_mode = 0;
    frontend_data_stamp_job.active = 0;
    frontend_data_text_job.active = 0;
    source_sound_effect(28); /* OpenTyrian S_CURSOR. */
}

static void frontend_start_data_reader_transition(u8 selection)
{
    frontend_data_reader_index = selection;
    frontend_data_scroll_pixel = 0;
    frontend_data_scroll_target = 0;
    frontend_transition_begin(
        STATE_DATA_CUBE_READER,
        selection,
        FRONTEND_TRANSITION_JOB_DATA_READER,
        0
    );
}

static void frontend_start_ship_specs_transition(void)
{
    frontend_ship_specs_scale_active = 0;
    frontend_ship_specs_scale_step = 1;
    frontend_transition_begin(
        STATE_SHIP_SPECS,
        0,
        FRONTEND_TRANSITION_JOB_SHIP_SPECS,
        0
    );
}

static void frontend_start_level_entry_transition(u16 selected_section)
{
    frontend_level_entry_section = selected_section;
    telemetry_level_music_transition_starts++;
    /*
     * This is an asynchronous operation inside the existing Next Level
     * state, not a visible state change.  Do not increment the source-style
     * front-end transition counter merely because audio now has a fade.
     */
    frontend_transition_job = FRONTEND_TRANSITION_JOB_LEVEL_ENTRY;
    frontend_transition_phase = 0;
    frontend_transition_flags = 0;
    frontend_transition_failed = 0;
    frontend_transition_work_index = 0;
    frontend_transition_work_mode = 0;
}

enum {
    FRONTEND_TRANSITION_SHIP_NONE = 0,
    FRONTEND_TRANSITION_SHIP_CACHE_APPLY,
    FRONTEND_TRANSITION_SHIP_DRAW,
    FRONTEND_TRANSITION_SHIP_CACHE_CAPTURE,
    FRONTEND_TRANSITION_PANEL_STATIC,
    FRONTEND_TRANSITION_PANEL_FALLBACK,
};

static void frontend_transition_ship_begin(void)
{
    frontend_transition_work_index = 0;
    if (!frontend_player_items_valid) {
        frontend_transition_work_mode =
            FRONTEND_TRANSITION_SHIP_NONE;
    } else if (frontend_ship_panel_cache_matches()) {
        frontend_transition_work_mode =
            FRONTEND_TRANSITION_SHIP_CACHE_APPLY;
    } else {
        frontend_transition_work_mode =
            FRONTEND_TRANSITION_SHIP_DRAW;
    }
}

/*
 * Returns true once the 120x160 configuration-dependent panel is complete.
 * Cache transfers are split into two 80-row groups; a cold panel builds one
 * stock source layer per update and then captures two equally bounded rows.
 */
static u8 frontend_transition_ship_update(void)
{
    switch (frontend_transition_work_mode) {
    case FRONTEND_TRANSITION_SHIP_NONE:
        return 1;

    case FRONTEND_TRANSITION_SHIP_CACHE_APPLY:
        (void)frontend_ship_panel_cache_apply_rows(
            frontend_frame_scratch,
            (u8)(frontend_transition_work_index * 80u),
            80
        );
        if (++frontend_transition_work_index >= 2) return 1;
        break;

    case FRONTEND_TRANSITION_SHIP_DRAW:
        frontend_draw_ship_illustration_part(
            frontend_frame_scratch,
            frontend_transition_work_index
        );
        if (
            ++frontend_transition_work_index >=
            FRONTEND_SHIP_DRAW_PART_COUNT
        ) {
            frontend_transition_work_mode =
                FRONTEND_TRANSITION_SHIP_CACHE_CAPTURE;
            frontend_transition_work_index = 0;
        }
        break;

    case FRONTEND_TRANSITION_SHIP_CACHE_CAPTURE:
        frontend_ship_panel_cache_capture_rows(
            frontend_frame_scratch,
            (u8)(frontend_transition_work_index * 80u),
            80
        );
        if (++frontend_transition_work_index >= 2) {
            frontend_ship_panel_cache_finalize();
            return 1;
        }
        break;

    default:
        return 1;
    }
    return 0;
}

static void frontend_transition_fail_to_title(void)
{
    frontend_transition_failed = 1;
    frontend_transition_job = FRONTEND_TRANSITION_JOB_NONE;
    enter_title();
}

/*
 * Static pages are assembled in bounded pieces while the previously
 * presented page remains stable.  mmFrame() still runs once per VBlank;
 * only the final phase schedules the atomic Mode-4 page flip.
 */
static void frontend_transition_update(void)
{
    if (
        frontend_transition_job == FRONTEND_TRANSITION_JOB_NONE ||
        frontend_frame_pending
    ) {
        return;
    }
    switch (frontend_transition_job) {
    case FRONTEND_TRANSITION_JOB_NEXT_LEVEL:
        switch (frontend_transition_phase) {
        case 0:
            frontend_nav_set_target();
            frontend_nav_presented_x = frontend_nav_x;
            frontend_nav_presented_y = frontend_nav_y;
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                0,
                80
            );
            frontend_transition_phase = 1;
            break;
        case 1:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                80,
                80
            );
            frontend_transition_phase = 2;
            break;
        case 2:
            frontend_nav_draw();
            frontend_transition_phase = 3;
            break;
        case 3:
            frontend_prepare_next_level_text_base();
            frontend_transition_work_index = 0;
            frontend_transition_phase = 4;
            break;
        case 4:
            {
                u8 final_item = frontend_next_choice_count();

                frontend_render_next_level_item(
                    frontend_transition_work_index
                );
                if (
                    frontend_transition_work_index++ >= final_item
                ) {
                    frontend_transition_phase = 5;
                }
            }
            break;
        default:
            frontend_nav_obj_rebuild();
            frontend_present(
                frontend_frame_scratch,
                frontend_palette_address(
                    FRONTEND_FRAME_NEXT_LEVEL_0
                )
            );
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_UPGRADE_SUBMENU:
        switch (frontend_transition_phase) {
        case 0:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                0,
                80
            );
            frontend_transition_phase = 1;
            break;
        case 1:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                80,
                80
            );
            frontend_transition_phase = 2;
            break;
        case 2:
            frontend_draw_upgrade_submenu_title();
            frontend_upgrade_stamp_job =
                (FrontendSourceStampJob){0};
            frontend_transition_phase = 3;
            break;
        case 3:
            if (frontend_upgrade_sim_category_active()) {
                s8 result;

                if (frontend_transition_work_mode == 0) {
                    frontend_fill_source_rect(
                        frontend_frame_scratch,
                        8,
                        8,
                        143,
                        182,
                        0
                    );
                    if (
                        !frontend_source_stamp_job_begin(
                            &frontend_upgrade_stamp_job,
                            frontend_frame_scratch,
                            frontend_source_stamp_shp_key(5, 12),
                            0,
                            0,
                            FRONTEND_SOURCE_BLIT_NORMAL,
                            0,
                            0
                        )
                    ) {
                        frontend_draw_upgrade_submenu_preview_art();
                        frontend_transition_phase = 4;
                        break;
                    }
                    frontend_transition_work_mode = 1;
                }
                result = frontend_source_stamp_job_step(
                    &frontend_upgrade_stamp_job,
                    FRONTEND_UPGRADE_STAMP_RUNS_PER_STEP
                );
                if (result == 0) break;
                if (result < 0) {
                    frontend_draw_upgrade_submenu_preview_art();
                }
            } else {
                frontend_draw_upgrade_submenu_preview_art();
            }
            frontend_transition_phase = 4;
            break;
        case 4:
            frontend_draw_upgrade_submenu_preview_details();
            frontend_transition_work_index = 0;
            frontend_transition_phase = 5;
            break;
        case 5:
            frontend_draw_upgrade_submenu_row(
                frontend_transition_work_index
            );
            if (++frontend_transition_work_index >= 6) {
                frontend_transition_phase = 6;
            }
            break;
        case 6:
            frontend_present(
                frontend_frame_scratch,
                frontend_palette_address(
                    FRONTEND_FRAME_MENU_CHROME
                )
            );
            if (frontend_upgrade_sim_category_active()) {
                frontend_transition_phase = 7;
            } else {
                frontend_upgrade_sim_disable();
                frontend_transition_job =
                    FRONTEND_TRANSITION_JOB_NONE;
            }
            break;
        case 7:
            frontend_upgrade_sim_enter_begin();
            frontend_transition_work_index = 0;
            frontend_transition_phase = 8;
            break;
        case 8:
            frontend_upgrade_sim_enter_stars(
                frontend_transition_work_index,
                40
            );
            frontend_transition_work_index = (u8)(
                frontend_transition_work_index + 40u
            );
            if (
                frontend_transition_work_index >=
                    FRONTEND_UPGRADE_SIM_STAR_COUNT
            ) {
                frontend_transition_phase = 9;
            }
            break;
        case 9:
            frontend_upgrade_sim_enter_loadout();
            frontend_transition_phase = 10;
            break;
        case 10:
            frontend_upgrade_sim_enter_prepare();
            frontend_transition_phase = 11;
            break;
        default:
            frontend_upgrade_sim_enter_finish();
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_GAME_MENU:
        switch (frontend_transition_phase) {
        case 0:
            if (
                frontend_transition_flags &
                    FRONTEND_GAME_MENU_JOB_INITIALIZE_CAMPAIGN
            ) {
                frontend_campaign_initialize();
            }
            frontend_transition_phase = 1;
            break;
        case 1:
            if (
                frontend_transition_flags &
                    FRONTEND_GAME_MENU_JOB_PREPARE_MAP
            ) {
                if (!frontend_prepare_map_begin()) {
                    frontend_transition_fail_to_title();
                    break;
                }
                frontend_transition_work_index = 0;
                frontend_transition_phase =
                    (
                        frontend_map.episode_complete ||
                        frontend_map.choice_count == 0
                    ) ?
                        3 :
                        2;
                if (
                    !frontend_map.episode_complete &&
                    frontend_map.choice_count == 0
                ) {
                    frontend_map_ready = 1;
                }
            } else {
                frontend_transition_phase = 3;
            }
            break;
        case 2:
            if (
                !frontend_prepare_map_choice(
                    frontend_transition_work_index
                )
            ) {
                frontend_transition_fail_to_title();
                break;
            }
            if (
                ++frontend_transition_work_index >=
                frontend_map.choice_count
            ) {
                frontend_map_ready = 1;
                frontend_transition_phase = 3;
            }
            break;
        case 3:
            if (
                frontend_transition_flags &
                    FRONTEND_GAME_MENU_JOB_LOAD_SONG
            ) {
                frontend_load_game_menu_song();
            }
            frontend_transition_phase = 4;
            break;
        case 4:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                0,
                80
            );
            frontend_transition_phase = 5;
            break;
        case 5:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                80,
                80
            );
            frontend_transition_phase = 6;
            break;
        case 6:
            frontend_transition_ship_begin();
            frontend_transition_phase = 7;
            break;
        case 7:
            if (frontend_transition_ship_update()) {
                frontend_transition_phase = 8;
            }
            break;
        case 8:
            frontend_transition_work_mode =
                frontend_copy_static_menu_panel_rows(
                    (u8)(
                        FRONTEND_STATIC_GAME_MENU_BASE +
                        frontend_selection
                    ),
                    0,
                    60
                ) ?
                    FRONTEND_TRANSITION_PANEL_STATIC :
                    FRONTEND_TRANSITION_PANEL_FALLBACK;
            frontend_transition_phase =
                frontend_transition_work_mode ==
                    FRONTEND_TRANSITION_PANEL_STATIC ?
                        9 :
                        10;
            break;
        case 9:
            (void)frontend_copy_static_menu_panel_rows(
                (u8)(
                    FRONTEND_STATIC_GAME_MENU_BASE +
                    frontend_selection
                ),
                60,
                60
            );
            frontend_transition_phase = 10;
            break;
        case 10:
            if (
                frontend_transition_work_mode ==
                    FRONTEND_TRANSITION_PANEL_FALLBACK
            ) {
                frontend_draw_game_menu_text(frontend_selection);
            }
            frontend_draw_game_menu_dynamic(frontend_selection);
            frontend_transition_phase = 11;
            break;
        case 11:
            frontend_quit_capture_dialog_background();
            frontend_quit_dialog_cache_valid = 1;
            frontend_quit_dialog_cache_selection =
                frontend_selection;
            frontend_transition_phase = 12;
            break;
        default:
            frontend_present(
                frontend_frame_scratch,
                frontend_palette_address(
                    FRONTEND_FRAME_MENU_CHROME
                )
            );
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_UPGRADE_MENU:
        switch (frontend_transition_phase) {
        case 0:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                0,
                80
            );
            frontend_transition_phase = 1;
            break;
        case 1:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                80,
                80
            );
            frontend_transition_phase = 2;
            break;
        case 2:
            frontend_transition_ship_begin();
            frontend_transition_phase = 3;
            break;
        case 3:
            if (frontend_transition_ship_update()) {
                frontend_transition_phase = 4;
            }
            break;
        case 4:
            frontend_transition_work_mode =
                frontend_copy_static_menu_panel_rows(
                    (u8)(
                        FRONTEND_STATIC_UPGRADE_MENU_BASE +
                        frontend_selection
                    ),
                    0,
                    60
                ) ?
                    FRONTEND_TRANSITION_PANEL_STATIC :
                    FRONTEND_TRANSITION_PANEL_FALLBACK;
            frontend_transition_phase =
                frontend_transition_work_mode ==
                    FRONTEND_TRANSITION_PANEL_STATIC ?
                        5 :
                        6;
            break;
        case 5:
            (void)frontend_copy_static_menu_panel_rows(
                (u8)(
                    FRONTEND_STATIC_UPGRADE_MENU_BASE +
                    frontend_selection
                ),
                60,
                60
            );
            frontend_transition_phase = 6;
            break;
        case 6:
            if (
                frontend_transition_work_mode ==
                    FRONTEND_TRANSITION_PANEL_FALLBACK
            ) {
                frontend_draw_upgrade_menu_text();
            }
            frontend_draw_upgrade_menu_help(frontend_selection);
            frontend_transition_phase = 7;
            break;
        default:
            frontend_present(
                frontend_frame_scratch,
                frontend_palette_address(
                    FRONTEND_FRAME_MENU_CHROME
                )
            );
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_QUIT:
        if (
            frontend_transition_flags &
                FRONTEND_QUIT_JOB_ENTER
        ) {
            switch (frontend_transition_phase) {
            case 0:
                frontend_draw_game_menu_dynamic(5);
                if (
                    frontend_quit_dialog_cache_valid &&
                    frontend_quit_dialog_cache_selection == 5
                ) {
                    frontend_transition_phase = 3;
                } else {
                    (void)frontend_copy_static_menu_panel_rows(
                        FRONTEND_STATIC_GAME_MENU_BASE + 5u,
                        0,
                        60
                    );
                    frontend_transition_phase = 1;
                }
                break;
            case 1:
                (void)frontend_copy_static_menu_panel_rows(
                    FRONTEND_STATIC_GAME_MENU_BASE + 5u,
                    60,
                    60
                );
                frontend_transition_phase = 2;
                break;
            case 2:
                frontend_draw_game_menu_dynamic(5);
                frontend_quit_capture_dialog_background();
                frontend_quit_dialog_cache_valid = 1;
                frontend_quit_dialog_cache_selection = 5;
                frontend_transition_phase = 3;
                break;
            case 3:
                frontend_compose_quit_shade();
                frontend_transition_phase = 4;
                break;
            case 4:
                frontend_compose_quit_overlay();
                frontend_transition_phase = 5;
                break;
            case 5:
                frontend_compose_quit_choices();
                frontend_transition_phase = 6;
                break;
            default:
                frontend_present(
                    frontend_frame_scratch,
                    frontend_palette_address(
                        FRONTEND_FRAME_MENU_CHROME
                    )
                );
                frontend_transition_job =
                    FRONTEND_TRANSITION_JOB_NONE;
                break;
            }
        } else {
            if (frontend_transition_phase == 0) {
                frontend_quit_restore_dialog_background();
                frontend_transition_phase = 1;
            } else {
                frontend_present(
                    frontend_frame_scratch,
                    frontend_palette_address(
                        FRONTEND_FRAME_MENU_CHROME
                    )
                );
                frontend_transition_job =
                    FRONTEND_TRANSITION_JOB_NONE;
            }
        }
        break;

    case FRONTEND_TRANSITION_JOB_DATA_CUBES:
        switch (frontend_transition_phase) {
        case 0:
            if (
                frontend_transition_flags &
                    FRONTEND_DATA_JOB_SELECTION
            ) {
                frontend_transition_work_index = 0;
                frontend_transition_work_mode = 0;
                frontend_data_stamp_job.active = 0;
                frontend_data_text_job.active = 0;
                frontend_transition_phase = 14;
                break;
            }
            if (!frontend_data_cache_valid) {
                frontend_transition_flags |=
                    FRONTEND_DATA_JOB_RELOAD |
                    FRONTEND_DATA_JOB_FULL_FRAME;
            }
            if (
                frontend_transition_flags &
                    FRONTEND_DATA_JOB_RELOAD
            ) {
                frontend_data_prepare_begin();
                frontend_transition_work_index = 0;
                frontend_transition_phase = 1;
            } else if (
                frontend_transition_flags &
                    FRONTEND_DATA_JOB_FULL_FRAME
            ) {
                frontend_transition_phase = 3;
            } else {
                frontend_transition_phase = 10;
            }
            break;
        case 1:
            if (
                frontend_transition_work_index <
                    frontend_data_source_count()
            ) {
                if (frontend_transition_work_mode == 0) {
                    u8 cube_index = frontend_data_cube_list[
                        frontend_transition_work_index
                    ];

                    if (
                        cube_index == 0 ||
                        !ot_data_cube_read_begin(
                            (u8)(frontend_episode + 1u),
                            cube_index,
                            &frontend_data_cube_cache[
                                frontend_data_page_count
                            ],
                            &frontend_data_cube_reader
                        )
                    ) {
                        frontend_transition_work_index++;
                    } else {
                        frontend_transition_work_mode = 1;
                    }
                } else {
                    u8 record;

                    for (
                        record = 0;
                        record < FRONTEND_DATA_RECORDS_PER_STEP;
                        record++
                    ) {
                        s8 result = ot_data_cube_read_step(
                            &frontend_data_cube_reader,
                            &frontend_data_cube_cache[
                                frontend_data_page_count
                            ]
                        );

                        if (result != 0) {
                            if (result > 0) {
                                frontend_data_page_count++;
                            }
                            frontend_transition_work_index++;
                            frontend_transition_work_mode = 0;
                            break;
                        }
                    }
                }
                break;
            }
            frontend_data_prepare_finish();
            if (frontend_selection > frontend_data_page_count) {
                frontend_selection = frontend_data_page_count;
            }
            frontend_transition_phase = 3;
            break;
        case 3:
            if (!frontend_data_reader_font_prepare_step(16)) {
                break;
            }
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                0,
                80
            );
            frontend_transition_phase = 4;
            break;
        case 4:
            frontend_copy_frame_rows(
                FRONTEND_FRAME_MENU_CHROME,
                80,
                80
            );
            frontend_transition_phase = 5;
            break;
        case 5:
            frontend_data_draw_portrait_frame();
            frontend_transition_phase = 15;
            break;
        case 6:
            frontend_data_draw_selected_portrait();
            frontend_transition_phase = 7;
            break;
        case 7:
            if (frontend_data_page_count == 0) {
                frontend_data_draw_empty_message();
            }
            frontend_transition_work_index = 0;
            frontend_transition_work_mode = 0;
            frontend_data_text_job.active = 0;
            frontend_transition_phase = 8;
            break;
        case 8:
            if (
                frontend_transition_work_index <
                    frontend_data_page_count
            ) {
                if (frontend_transition_work_mode < 3) {
                    s8 glow =
                        frontend_transition_work_index ==
                            frontend_selection ?
                                frontend_data_glow :
                                0;

                    if (!frontend_data_stamp_job.active) {
                        if (!frontend_data_cube_layer_begin(
                                frontend_transition_work_index,
                                glow,
                                frontend_transition_work_mode
                            )) {
                            frontend_data_draw_cube_layer(
                                frontend_transition_work_index,
                                glow,
                                frontend_transition_work_mode++
                            );
                            break;
                        }
                    }
                    {
                        s8 result =
                            frontend_source_stamp_job_step(
                                &frontend_data_stamp_job,
                                FRONTEND_DATA_STAMP_RUNS_PER_STEP
                            );

                        if (result != 0) {
                            if (result < 0) {
                                frontend_data_draw_cube_layer(
                                    frontend_transition_work_index,
                                    glow,
                                    frontend_transition_work_mode
                                );
                            }
                            frontend_transition_work_mode++;
                        }
                    }
                } else {
                    if (frontend_transition_work_mode == 3) {
                        frontend_data_list_title_begin(
                            frontend_transition_work_index
                        );
                        frontend_transition_work_mode = 4;
                    }
                    if (frontend_wrapped_text_job_step(
                            &frontend_data_text_job
                        )) {
                        frontend_transition_work_index++;
                        frontend_transition_work_mode = 0;
                    }
                }
            } else {
                if (frontend_transition_work_mode == 0) {
                    frontend_data_exit_text_begin();
                    frontend_transition_work_mode = 4;
                }
                if (frontend_wrapped_text_job_step(
                        &frontend_data_text_job
                    )) {
                    frontend_transition_work_mode = 0;
                    frontend_transition_phase = 9;
                }
            }
            break;
        case 9:
            frontend_data_draw_menu_header(0);
            frontend_data_draw_help(0);
            frontend_data_prepare_palette();
            frontend_present(
                frontend_frame_scratch,
                (const u8 *)(const void *)
                    frontend_data_face_palette
            );
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        case 10:
            frontend_restore_menu_chrome_rect(
                FRONTEND_DATA_RIGHT_X,
                FRONTEND_DATA_RIGHT_Y,
                FRONTEND_DATA_RIGHT_WIDTH,
                FRONTEND_DATA_RIGHT_HEIGHT
            );
            frontend_transition_work_index = 0;
            frontend_transition_work_mode = 0;
            frontend_data_stamp_job.active = 0;
            frontend_data_text_job.active = 0;
            frontend_transition_phase = 11;
            break;
        case 11:
            if (frontend_data_page_count == 0) {
                frontend_data_draw_empty_message();
                frontend_transition_phase = 12;
            } else if (
                frontend_transition_work_index <
                    frontend_data_page_count
            ) {
                if (frontend_transition_work_mode < 3) {
                    s8 glow =
                        frontend_transition_work_index ==
                            frontend_selection ?
                                frontend_data_glow :
                                0;

                    if (!frontend_data_stamp_job.active) {
                        if (!frontend_data_cube_layer_begin(
                                frontend_transition_work_index,
                                glow,
                                frontend_transition_work_mode
                            )) {
                            frontend_data_draw_cube_layer(
                                frontend_transition_work_index,
                                glow,
                                frontend_transition_work_mode++
                            );
                            break;
                        }
                    }
                    {
                        s8 result =
                            frontend_source_stamp_job_step(
                                &frontend_data_stamp_job,
                                FRONTEND_DATA_STAMP_RUNS_PER_STEP
                            );

                        if (result != 0) {
                            if (result < 0) {
                                frontend_data_draw_cube_layer(
                                    frontend_transition_work_index,
                                    glow,
                                    frontend_transition_work_mode
                                );
                            }
                            frontend_transition_work_mode++;
                        }
                    }
                } else {
                    if (frontend_transition_work_mode == 3) {
                        frontend_data_list_title_begin(
                            frontend_transition_work_index
                        );
                        frontend_transition_work_mode = 4;
                    }
                    if (frontend_wrapped_text_job_step(
                            &frontend_data_text_job
                        )) {
                        frontend_transition_work_index++;
                        frontend_transition_work_mode = 0;
                    }
                }
            } else {
                if (frontend_transition_work_mode == 0) {
                    frontend_data_exit_text_begin();
                    frontend_transition_work_mode = 4;
                }
                if (frontend_wrapped_text_job_step(
                        &frontend_data_text_job
                    )) {
                    frontend_transition_work_mode = 0;
                    frontend_transition_phase = 12;
                }
            }
            break;
        case 12:
            frontend_data_draw_menu_header(0);
            frontend_data_draw_help(0);
            frontend_data_prepare_palette();
            frontend_transition_phase = 13;
            break;
        case 14:
            switch (frontend_transition_work_mode) {
            case 0:
                if (!frontend_data_portrait_base_restore()) {
                    frontend_data_draw_portrait_frame();
                    frontend_data_portrait_base_capture();
                }
                frontend_transition_work_index = 0;
                frontend_transition_work_mode = 1;
                break;
            case 1:
                if (!frontend_data_stamp_job.active) {
                    s8 started =
                        frontend_data_selected_face_job_begin();

                    if (started <= 0) {
                        if (started < 0) {
                            frontend_data_draw_selected_face();
                        }
                        frontend_transition_work_index = 0;
                        frontend_transition_work_mode = 2;
                        break;
                    }
                }
                {
                    s8 result = frontend_source_stamp_job_step(
                        &frontend_data_stamp_job,
                        FRONTEND_DATA_SELECTION_STAMP_RUNS_PER_STEP
                    );

                    if (result != 0) {
                        if (result < 0) {
                            frontend_data_draw_selected_face();
                        }
                        frontend_transition_work_index = 0;
                        frontend_transition_work_mode = 2;
                    }
                }
                break;
            case 2:
                if (frontend_transition_work_index == 0) {
                    frontend_data_selected_header_job_begin();
                    frontend_transition_work_index = 1;
                }
                if (frontend_wrapped_text_job_step(
                        &frontend_data_text_job
                    )) {
                    frontend_transition_work_index = 0;
                    frontend_transition_work_mode = 3;
                }
                break;
            case 3:
                if (frontend_data_selection_item_job_step(
                        frontend_patch_old_selection
                    )) {
                    frontend_transition_work_index = 0;
                    frontend_data_stamp_job.active = 0;
                    frontend_data_text_job.active = 0;
                    frontend_transition_work_mode = 4;
                }
                break;
            case 4:
                if (frontend_data_selection_item_job_step(
                        frontend_selection
                    )) {
                    frontend_transition_work_index = 0;
                    frontend_data_stamp_job.active = 0;
                    frontend_data_text_job.active = 0;
                    frontend_transition_work_mode = 5;
                }
                break;
            case 5:
                frontend_data_draw_help(0);
                frontend_data_prepare_palette();
                frontend_transition_work_mode = 6;
                break;
            default:
                frontend_dirty_begin();
                frontend_dirty_add(
                    FRONTEND_DATA_LEFT_X,
                    FRONTEND_DATA_LEFT_Y,
                    FRONTEND_DATA_LEFT_WIDTH,
                    FRONTEND_DATA_LEFT_HEIGHT
                );
                frontend_dirty_add(
                    FRONTEND_DATA_RIGHT_X,
                    FRONTEND_DATA_RIGHT_Y,
                    FRONTEND_DATA_RIGHT_WIDTH,
                    FRONTEND_DATA_RIGHT_HEIGHT
                );
                frontend_dirty_add(
                    0,
                    frontend_menu_help_dirty_y(),
                    SCREEN_WIDTH,
                    (u16)(
                        SCREEN_HEIGHT -
                        frontend_menu_help_dirty_y()
                    )
                );
                frontend_dirty_present_with_palette(
                    frontend_data_face_palette
                );
                frontend_transition_job =
                    FRONTEND_TRANSITION_JOB_NONE;
                break;
            }
            break;
        case 15:
            frontend_data_portrait_base_capture();
            frontend_transition_phase = 6;
            break;
        default:
            frontend_dirty_begin();
            frontend_dirty_add(
                frontend_source_to_screen_x(150),
                0,
                (u16)(
                    SCREEN_WIDTH -
                    frontend_source_to_screen_x(150)
                ),
                frontend_source_to_screen_y(30)
            );
            frontend_dirty_add(
                FRONTEND_DATA_RIGHT_X,
                FRONTEND_DATA_RIGHT_Y,
                FRONTEND_DATA_RIGHT_WIDTH,
                FRONTEND_DATA_RIGHT_HEIGHT
            );
            frontend_dirty_add(
                0,
                frontend_menu_help_dirty_y(),
                SCREEN_WIDTH,
                (u16)(
                    SCREEN_HEIGHT -
                    frontend_menu_help_dirty_y()
                )
            );
            frontend_dirty_present_with_palette(
                frontend_data_face_palette
            );
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_DATA_READER:
        switch (frontend_transition_phase) {
        case 0:
            if (!frontend_data_reader_font_prepare_step(8)) {
                break;
            }
            frontend_transition_phase = 1;
            break;
        case 1:
            frontend_data_reader_panel_job_begin();
            frontend_transition_phase = 2;
            break;
        case 2:
            if (frontend_data_reader_panel_job_step(
                    FRONTEND_DATA_READER_GLYPHS_PER_STEP
                )) {
                frontend_transition_phase = 3;
            }
            break;
        case 3:
            frontend_data_draw_reader_chrome();
            frontend_data_draw_menu_header(1);
            frontend_data_draw_help(1);
            frontend_transition_phase = 4;
            break;
        default:
            frontend_dirty_begin();
            frontend_dirty_add(
                frontend_source_to_screen_x(150),
                0,
                (u16)(
                    SCREEN_WIDTH -
                    frontend_source_to_screen_x(150)
                ),
                frontend_source_to_screen_y(30)
            );
            frontend_dirty_add(
                FRONTEND_DATA_RIGHT_X,
                FRONTEND_DATA_RIGHT_Y,
                FRONTEND_DATA_RIGHT_WIDTH,
                FRONTEND_DATA_RIGHT_HEIGHT
            );
            frontend_dirty_add(
                0,
                frontend_menu_help_dirty_y(),
                SCREEN_WIDTH,
                (u16)(
                    SCREEN_HEIGHT -
                    frontend_menu_help_dirty_y()
                )
            );
            frontend_dirty_present();
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_SHIP_SPECS:
        switch (frontend_transition_phase) {
        case 0:
            if (!frontend_ship_specs_prepare()) {
                audio_effect(SFX_CLINK);
                frontend_start_game_menu_transition_selected(0, 1);
                break;
            }
            frontend_transition_phase = 1;
            break;
        case 1:
            frontend_ship_specs_clear();
            frontend_transition_phase = 2;
            break;
        case 2:
            frontend_ship_specs_draw_grid_vertical();
            frontend_transition_phase = 3;
            break;
        case 3:
            frontend_ship_specs_draw_grid_horizontal();
            frontend_transition_phase = 4;
            break;
        case 4:
            frontend_ship_specs_draw_title();
            frontend_transition_phase = 5;
            break;
        case 5:
            if (frontend_transition_work_mode == 0) {
                frontend_ship_specs_text_begin(0);
                frontend_transition_work_mode = 1;
            }
            if (frontend_wrapped_text_job_step(
                    &frontend_ship_specs_text_job
                )) {
                frontend_transition_work_mode = 0;
                frontend_transition_phase = 6;
            }
            break;
        case 6:
            if (frontend_transition_work_mode == 0) {
                frontend_ship_specs_text_begin(1);
                frontend_transition_work_mode = 1;
            }
            if (frontend_wrapped_text_job_step(
                    &frontend_ship_specs_text_job
                )) {
                frontend_transition_work_mode = 0;
                frontend_transition_phase = 7;
            }
            break;
        case 7:
            frontend_ship_specs_draw_footer();
            frontend_transition_phase = 8;
            break;
        case 8:
            if (frontend_transition_work_mode == 0) {
                if (frontend_ship_specs_art_begin()) {
                    frontend_transition_work_mode = 1;
                } else {
                    frontend_ship_specs_draw_art_fallback();
                    frontend_transition_phase = 9;
                }
            } else {
                s8 result =
                    frontend_ship_specs_green_ship_step();

                if (result != 0) {
                    if (result < 0) {
                        frontend_ship_specs_draw_art_fallback();
                    }
                    frontend_transition_work_mode = 0;
                    frontend_transition_phase = 9;
                }
            }
            break;
        case 9:
            frontend_ship_specs_scale_active = 1;
            frontend_ship_specs_scale_step = 1;
            frontend_present(
                frontend_frame_scratch,
                frontend_palette_address(
                    FRONTEND_FRAME_MENU_CHROME
                )
            );
            frontend_transition_phase = 10;
            break;
        default:
            if (pad_pressed != 0) {
                frontend_ship_specs_scale_step =
                    FRONTEND_SHIP_SPECS_SCALE_STEPS;
            } else if (
                frontend_ship_specs_scale_step <
                    FRONTEND_SHIP_SPECS_SCALE_STEPS
            ) {
                frontend_ship_specs_scale_step++;
            }
            if (
                frontend_ship_specs_scale_step >=
                    FRONTEND_SHIP_SPECS_SCALE_STEPS
            ) {
                frontend_transition_job =
                    FRONTEND_TRANSITION_JOB_NONE;
            }
            break;
        }
        break;

    case FRONTEND_TRANSITION_JOB_LEVEL_ENTRY:
        switch (frontend_transition_phase) {
        case 0:
            /*
             * Fade the module only.  UI confirmation effects remain audible
             * while the menu song approaches an exact zero sample.
             */
            if (!mmActive()) {
                mmSetModuleVolume(0);
                frontend_transition_work_index = 0;
                frontend_transition_phase = 1;
                break;
            }
            if (
                frontend_transition_work_index <
                    TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS
            ) {
                u16 remaining;
                u16 volume;

                frontend_transition_work_index++;
                telemetry_level_music_fade_out_steps++;
                remaining = (u16)(
                    TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS -
                    frontend_transition_work_index
                );
                volume = (u16)(
                    (
                        (u32)AUDIO_SOURCE_MUSIC_VOLUME_MAX *
                            remaining +
                        TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS /
                            2u
                    ) /
                    TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS
                );
                mmSetModuleVolume(volume);
            }
            if (
                frontend_transition_work_index >=
                    TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS
            ) {
                frontend_transition_work_index = 0;
                frontend_transition_phase = 1;
            }
            break;

        case 1:
            /*
             * mmFrame() ran before this state update.  Holding zero for the
             * configured count lets Maxmod submit a silent buffer before
             * mmStop(), avoiding a Direct Sound FIFO/DC discontinuity.
             */
            frontend_transition_work_index++;
            telemetry_level_music_silent_vblanks++;
            if (
                frontend_transition_work_index >=
                    TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS
            ) {
                mmStop();
                frontend_transition_work_index = 0;
                frontend_transition_phase = 2;
            }
            break;

        default:
            frontend_nav_obj_disable();
            frontend_transition_job =
                FRONTEND_TRANSITION_JOB_NONE;
            if (!enter_level(frontend_level_entry_section)) {
                frontend_start_game_menu_transition(
                    FRONTEND_GAME_MENU_JOB_LOAD_SONG
                );
            }
            frontend_level_entry_section = 0;
            break;
        }
        break;

    default:
        frontend_transition_failed = 1;
        frontend_transition_job = FRONTEND_TRANSITION_JOB_NONE;
        break;
    }
}
