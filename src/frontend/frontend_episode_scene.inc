/*
 * Direct levelsN.dat inter-level presentation adapter.
 *
 * Script selection, conditional jumps, text, pictures and music remain the
 * stock PC data.  This file only adapts the final 320x200 indexed image to
 * GBA Mode 4 and maps keyboard dismissal to A/Start.
 */

enum {
    FRONTEND_SCENE_ANM_FRAME_VBLANKS = 7,
    FRONTEND_SCENE_WARNING_SOUND_VBLANKS = 90,
};

static void frontend_scene_palette_rgb(
    const OtDataView *palette,
    u8 eight_bit
)
{
    u16 index;

    memset(frontend_scene_palette, 0, sizeof(frontend_scene_palette));
    if (
        palette == 0 ||
        palette->data == 0 ||
        palette->size < OT_PALETTE_BYTES
    ) {
        return;
    }
    for (index = 0; index < OT_PALETTE_COLOUR_COUNT; index++) {
        const u8 *rgb = palette->data + (u32)index * 3u;
        u8 shift = eight_bit ? 3 : 1;

        frontend_scene_palette[index] = (u16)(
            (rgb[0] >> shift) |
            ((rgb[1] >> shift) << 5) |
            ((rgb[2] >> shift) << 10)
        );
    }
}

static void frontend_scene_palette_anm(const OtDataView *palette)
{
    u16 index;

    memset(frontend_scene_palette, 0, sizeof(frontend_scene_palette));
    if (
        palette == 0 ||
        palette->data == 0 ||
        palette->size < 4u * OT_PALETTE_COLOUR_COUNT
    ) {
        return;
    }
    for (index = 1; index < OT_PALETTE_COLOUR_COUNT; index++) {
        const u8 *bgra = palette->data + (u32)index * 4u;

        frontend_scene_palette[index] = (u16)(
            (bgra[2] >> 3) |
            ((bgra[1] >> 3) << 5) |
            ((bgra[0] >> 3) << 10)
        );
    }
}

static void frontend_scene_scale_pc_frame(void)
{
    u8 *frame = (u8 *)(void *)&frontend_gameplay_arena;
    u16 y;

    /*
     * Destination addresses always precede unread source addresses, so the
     * stock 64,000-byte decode can be reduced in place without a second
     * frame buffer.  This is the exact 3/4 x 4/5 full-screen transform.
     */
    for (y = 0; y < SCREEN_HEIGHT; y++) {
        u16 source_y = (u16)(((u32)y * OT_PIC_DECODED_HEIGHT) /
            SCREEN_HEIGHT);
        u16 x;

        for (x = 0; x < SCREEN_WIDTH; x++) {
            u16 source_x = (u16)(((u32)x * OT_PIC_DECODED_WIDTH) /
                SCREEN_WIDTH);

            frame[(u32)y * SCREEN_WIDTH + x] =
                frame[(u32)source_y * OT_PIC_DECODED_WIDTH + source_x];
        }
    }
}

static u8 frontend_scene_load_picture(u16 picture)
{
    u8 *decode = (u8 *)(void *)&frontend_gameplay_arena;
    OtDataView palette;
    u8 decoded = 0;

    if (picture > 900u) {
        u16 source_picture = (u16)(picture - 900u);

        if (
            source_picture == 0 ||
            source_picture > OT_PIC_COUNT ||
            !ot_data_pic_palette_view((u8)source_picture, &palette)
        ) {
            return 0;
        }
        frontend_scene_palette_rgb(&palette, 0);
        memset(frontend_frame_scratch, 0, FRONTEND_FRAME_BYTES);
    } else if (picture == 0) {
        if (!ot_data_pcx_decode(
                "tshp2.pcx",
                decode,
                sizeof(frontend_gameplay_arena),
                &palette
            )) {
            return 0;
        }
        frontend_scene_palette_rgb(&palette, 1);
        decoded = 1;
    } else {
        if (
            picture > OT_PIC_COUNT ||
            !ot_data_pic_decode(
                (u8)picture,
                decode,
                sizeof(frontend_gameplay_arena)
            ) ||
            !ot_data_pic_palette_view((u8)picture, &palette)
        ) {
            return 0;
        }
        frontend_scene_palette_rgb(&palette, 0);
        decoded = 1;
    }
    if (decoded) frontend_scene_scale_pc_frame();
    frontend_present(
        frontend_frame_scratch,
        (const u8 *)(const void *)frontend_scene_palette
    );
    return 1;
}

static u32 frontend_scene_total_score(void)
{
    u32 score = player_cash;

    if (!frontend_player_items_valid) return score;
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_SHIP,
        frontend_player_items.ship,
        1
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_FRONT_WEAPON,
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id,
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_REAR_WEAPON,
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id,
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_SHIELD,
        frontend_player_items.shield,
        1
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_GENERATOR,
        frontend_player_items.generator,
        1
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_LEFT_SIDEKICK,
        frontend_player_items.sidekick[0],
        1
    );
    score += frontend_upgrade_total_cost(
        FRONTEND_UPGRADE_RIGHT_SIDEKICK,
        frontend_player_items.sidekick[1],
        1
    );
    return score;
}

/*
 * OpenTyrian renders levelsN.dat prose with TINY_FONT at x=10 on a
 * 320-pixel-wide surface.  A literal one-line copy does not fit after the
 * front-end is adapted to 240 pixels.  Keep every source string intact and
 * wrap only at this final presentation boundary.  The font is the same
 * source-derived 5-row face used by the Data reader, not a second baked
 * alphabet.
 */
static const char *frontend_scene_wrap_segment(
    const char *text,
    char segment[OT_EPISODE_SCENE_TEXT_LINE_BYTES],
    s16 maximum_width
)
{
    u8 length = 0;
    u8 take;
    u8 last_space = 0xffu;
    s16 width = 0;
    const char *next;

    if (text == 0 || *text == '\0') {
        segment[0] = '\0';
        return 0;
    }
    while (
        text[length] != '\0' &&
        length + 1u < OT_EPISODE_SCENE_TEXT_LINE_BYTES
    ) {
        char character = text[length];
        u8 advance = character == '~' ? 0u :
            frontend_data_tiny_font_advance(
                frontend_pregame_font_tile(character)
            );

        if (width + advance > maximum_width && length != 0) break;
        width = (s16)(width + advance);
        if (character == ' ') last_space = length;
        length++;
        if (width > maximum_width) break;
    }
    if (text[length] == '\0') {
        take = length;
        next = 0;
    } else {
        take =
            last_space != 0xffu && last_space != 0 ?
                last_space :
                length;
        if (take == 0) take = 1;
        next = text + take;
        while (*next == ' ') next++;
        if (*next == '\0') next = 0;
    }
    while (take != 0 && text[take - 1u] == ' ') take--;
    memcpy(segment, text, take);
    segment[take] = '\0';
    return next;
}

static u8 frontend_scene_wrapped_rows(
    const char *text,
    s16 maximum_width
)
{
    char segment[OT_EPISODE_SCENE_TEXT_LINE_BYTES];
    const char *remaining = text;
    u8 rows = 0;

    if (text == 0 || *text == '\0') return 1;
    do {
        remaining = frontend_scene_wrap_segment(
            remaining,
            segment,
            maximum_width
        );
        rows++;
    } while (remaining != 0 && rows != 0xffu);
    return rows;
}

static s16 frontend_scene_draw_wrapped_line(
    const char *text,
    s16 y,
    u8 row_step,
    u8 colour,
    u8 shadow
)
{
    char segment[OT_EPISODE_SCENE_TEXT_LINE_BYTES];
    const char *remaining = text;
    const s16 maximum_width =
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT -
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_X;

    if (text == 0 || *text == '\0') return (s16)(y + row_step);
    do {
        remaining = frontend_scene_wrap_segment(
            remaining,
            segment,
            maximum_width
        );
        frontend_data_tiny_text_clipped_colours(
            frontend_frame_scratch,
            segment,
            TYRIAN_GBA_LAYOUT_SCENE_TEXT_X,
            y,
            TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT,
            colour,
            shadow
        );
        y = (s16)(y + row_step);
    } while (remaining != 0);
    return y;
}

static void frontend_scene_draw_text(
    const OtEpisodeSceneAction *action,
    u8 episode_end
)
{
    s16 y;
    s16 minimum_y;
    s16 prompt_y;
    s16 maximum_width;
    u16 required_height;
    u8 row_count = 0;
    u8 row_step = TYRIAN_GBA_LAYOUT_SCENE_TEXT_LINE_STEP;
    u8 line;
    u8 colour;
    u8 shadow;
    const char *prompt = frontend_stats_text(4, "PRESS A KEY");
    char score[61];

    if (action == 0) return;
    frontend_data_reader_font_prepare();
    colour = (u8)((action->text_bank << 4) | 15u);
    shadow = (u8)((action->text_bank << 4) | 2u);
    if (action->warning) {
        frontend_scene_warning_colour = (14u << 4) | 5u;
        frontend_scene_warning_direction = 1;
        frontend_scene_warning_bar_delay = 0;
        memset(
            frontend_frame_scratch,
            frontend_scene_warning_colour,
            5u * SCREEN_WIDTH
        );
        memset(
            frontend_frame_scratch +
                (SCREEN_HEIGHT - 5u) * SCREEN_WIDTH,
            frontend_scene_warning_colour,
            5u * SCREEN_WIDTH
        );
    }
    maximum_width =
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT -
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_X;
    prompt_y = action->warning_red ?
        TYRIAN_GBA_LAYOUT_SCENE_WARNING_PROMPT_Y :
        TYRIAN_GBA_LAYOUT_SCENE_PROMPT_Y;
    minimum_y = action->warning ? 8 : 4;
    score[0] = '\0';
    if (episode_end) {
        const char *prefix =
            frontend_text_ready &&
                frontend_text.misc_text[37][0] != '\0' ?
                frontend_text.misc_text[37] :
                "TOTAL SCORE ";

        frontend_text_with_u32(
            score,
            sizeof(score),
            prefix,
            frontend_scene_total_score()
        );
        row_count = (u8)(
            frontend_scene_wrapped_rows(score, maximum_width) + 1u
        );
    }
    for (line = 0; line < action->text_line_count; line++) {
        row_count = (u8)(
            row_count +
            frontend_scene_wrapped_rows(
                action->text[line],
                maximum_width
            )
        );
    }
    if (row_step < 6u) row_step = 6u;
    while (
        row_step > 6u &&
        (u16)row_count * row_step >
            (u16)(prompt_y - minimum_y - 2)
    ) {
        row_step--;
    }
    y = action->warning_red ?
        TYRIAN_GBA_LAYOUT_SCENE_WARNING_TEXT_Y :
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_Y;
    required_height = (u16)row_count * row_step;
    if (y + required_height > prompt_y - 2) {
        y = (s16)(prompt_y - 2 - required_height);
    }
    if (y < minimum_y) y = minimum_y;
    if (episode_end) {
        y = frontend_scene_draw_wrapped_line(
            score,
            y,
            row_step,
            colour,
            shadow
        );
        y = (s16)(y + row_step);
    }
    for (line = 0; line < action->text_line_count; line++) {
        y = frontend_scene_draw_wrapped_line(
            action->text[line],
            y,
            row_step,
            colour,
            shadow
        );
    }
    frontend_data_tiny_text_clipped_colours(
        frontend_frame_scratch,
        prompt,
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_data_tiny_text_width(prompt) / 2
        ),
        prompt_y,
        SCREEN_WIDTH,
        colour,
        shadow
    );
    frontend_present(
        frontend_frame_scratch,
        (const u8 *)(const void *)frontend_scene_palette
    );
}

static void frontend_scene_clear(u8 palette_index)
{
    OtDataView palette;

    if (ot_data_palette_view(palette_index, &palette)) {
        frontend_scene_palette_rgb(&palette, 0);
    }
    memset(frontend_frame_scratch, 0, FRONTEND_FRAME_BYTES);
    frontend_present(
        frontend_frame_scratch,
        (const u8 *)(const void *)frontend_scene_palette
    );
}

static u8 frontend_scene_start_animation(void)
{
    OtDataView palette;

    if (!ot_data_anm_begin(&frontend_scene_anm, &palette)) return 0;
    frontend_scene_palette_anm(&palette);
    memset(frontend_frame_scratch, 0, FRONTEND_FRAME_BYTES);
    frontend_scene_animation_active = 1;
    frontend_scene_animation_wait = 0;
    return 1;
}

static void frontend_scene_finish(void)
{
    if (frontend_frame_pending) {
        frontend_scene_finish_pending = 1;
        return;
    }
    frontend_scene_finish_pending = 0;
    frontend_scene_waiting_input = 0;
    frontend_scene_animation_active = 0;
    if (frontend_scene_end_episode) {
        frontend_scene_end_episode = 0;
        frontend_advance_episode();
    } else if (
        frontend_scene_return_mode == FRONTEND_SCENE_RETURN_LEVEL
    ) {
        frontend_start_level_entry_transition(
            frontend_scene_target_section
        );
    } else {
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
    }
}

static void frontend_episode_announcement_begin(void)
{
    const char *episode_name =
        frontend_text_ready &&
            frontend_episode + 1u < 6u &&
            frontend_text.episode_name[frontend_episode + 1u][0] != '\0' ?
            frontend_text.episode_name[frontend_episode + 1u] :
            "Episode";
    const char *prompt = frontend_stats_text(4, "PRESS A KEY");
    u8 colour = 0xff;
    u8 shadow = 0xf2;

    frontend_scene_waiting_input = 0;
    frontend_scene_finish_pending = 0;
    frontend_scene_end_episode = 0;
    frontend_scene_animation_active = 0;
    frontend_scene_episode_announcement = 1;
    frontend_scene_animation_wait = 0;
    frontend_scene_warning_delay = 0;
    frontend_scene_warning_bar_delay = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_state = STATE_EPISODE_SCENE;
    frontend_selection = 0;
    frontend_scene_clear(5); /* OpenTyrian palettes[6 - 1]. */
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        episode_name,
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_small_mixed_text_width(episode_name) / 2
        ),
        104, /* source y=130 scaled from 200 to 160 */
        SCREEN_WIDTH,
        colour,
        shadow
    );
    frontend_draw_small_mixed_text_clipped_colours(
        frontend_frame_scratch,
        prompt,
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_small_mixed_text_width(prompt) / 2
        ),
        148, /* source y=185 scaled from 200 to 160 */
        SCREEN_WIDTH,
        colour,
        shadow
    );
    frontend_present(
        frontend_frame_scratch,
        (const u8 *)(const void *)frontend_scene_palette
    );
    if (26u < JUKEBOX_MUSIC_COUNT) {
        audio_load_source_song_preserve_effects(26);
    }
    telemetry_state_transitions++;
}

static void frontend_scene_dispatch(u8 action_ready)
{
    u8 actions = 0;

    while (actions++ < 64) {
        int8_t status;

        if (!action_ready) {
            status = ot_data_episode_scene_step(
                &frontend_scene_reader,
                frontend_scene_secret_hint,
                &frontend_scene_action
            );
            if (status <= 0) {
                frontend_scene_finish();
                return;
            }
        }
        action_ready = 0;
        switch (frontend_scene_action.type) {
        case OT_EPISODE_SCENE_ACTION_END_MAP:
            frontend_scene_finish();
            return;

        case OT_EPISODE_SCENE_ACTION_PICTURE:
        case OT_EPISODE_SCENE_ACTION_PAN_UP:
        case OT_EPISODE_SCENE_ACTION_SLIDE_UP:
        case OT_EPISODE_SCENE_ACTION_PAN_RIGHT:
            /*
             * GBA adaptation: retain the exact destination art and palette;
             * omit only the 200/320 intermediate software-scroll frames.
             */
            (void)frontend_scene_load_picture(
                frontend_scene_action.value
            );
            break;

        case OT_EPISODE_SCENE_ACTION_MUSIC:
            if (frontend_scene_action.value < JUKEBOX_MUSIC_COUNT) {
                audio_load_source_song_preserve_effects(
                    (u8)frontend_scene_action.value
                );
            }
            break;

        case OT_EPISODE_SCENE_ACTION_TEXT:
            frontend_scene_draw_text(&frontend_scene_action, 0);
            frontend_scene_waiting_input = 1;
            frontend_scene_warning_delay = 0;
            frontend_scene_warning_bar_delay = 0;
            return;

        case OT_EPISODE_SCENE_ACTION_CLEAR:
            frontend_scene_clear(7);
            break;

        case OT_EPISODE_SCENE_ACTION_FADE_BLACK:
        case OT_EPISODE_SCENE_ACTION_FLASH_CLEAR:
            frontend_scene_clear(7);
            break;

        case OT_EPISODE_SCENE_ACTION_ANIMATION:
            if (frontend_scene_start_animation()) return;
            break;

        case OT_EPISODE_SCENE_ACTION_SELECT_END_HINT:
            /* JE_loadMap(): secretHint + (mt_rand() % 3) * 3 at ]Q. */
            frontend_scene_secret_hint = (u8)(
                frontend_secret_hint +
                (ot_level_port_random(&source_parity_level) % 3u) * 3u
            );
            break;

        case OT_EPISODE_SCENE_ACTION_END_EPISODE:
            frontend_scene_end_episode = 1;
            frontend_scene_draw_text(&frontend_scene_action, 1);
            frontend_scene_waiting_input = 1;
            return;

        default:
            break;
        }
    }
    frontend_scene_finish();
}

static u8 frontend_episode_scene_begin(
    u16 section,
    u8 return_mode,
    u16 target_section
)
{
    int8_t status;
    u8 old_state = game_state;

    if (!ot_data_episode_scene_begin(
            (u8)(frontend_episode + 1u),
            section,
            frontend_play_mode,
            frontend_difficulty,
            &frontend_route_state,
            &frontend_scene_reader
        )) {
        return 0;
    }
    status = ot_data_episode_scene_step(
        &frontend_scene_reader,
        1,
        &frontend_scene_action
    );
    if (
        status <= 0 ||
        frontend_scene_action.type == OT_EPISODE_SCENE_ACTION_END_MAP
    ) {
        return 0;
    }
    frontend_scene_return_mode = return_mode;
    frontend_scene_target_section = target_section;
    frontend_scene_waiting_input = 0;
    frontend_scene_finish_pending = 0;
    frontend_scene_end_episode = 0;
    frontend_scene_animation_active = 0;
    frontend_scene_episode_announcement = 0;
    frontend_scene_animation_wait = 0;
    frontend_scene_warning_delay = 0;
    frontend_scene_warning_bar_delay = 0;
    frontend_scene_secret_hint = frontend_secret_hint;
    if (
        old_state == STATE_NEXT_LEVEL_MENU &&
        frontend_nav_obj_active
    ) {
        frontend_nav_obj_disable();
    }
    frontend_stats_overlay_active = 0;
    frontend_stats_tiles_pending = 0;
    frontend_stats_palette_dirty = 0;
    frontend_stats_font_ready = 0;
    game_state = STATE_EPISODE_SCENE;
    frontend_selection = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_scene_clear(7);
    telemetry_state_transitions++;
    frontend_scene_dispatch(1);
    return 1;
}

static void frontend_episode_scene_update(void)
{
    if (frontend_scene_episode_announcement) {
        if (pad_pressed == 0 || frontend_frame_pending) return;
        frontend_scene_episode_announcement = 0;
        frontend_start_game_menu_transition(
            FRONTEND_GAME_MENU_JOB_PREPARE_MAP |
                FRONTEND_GAME_MENU_JOB_LOAD_SONG
        );
        return;
    }
    if (frontend_scene_finish_pending) {
        frontend_scene_finish();
        return;
    }
    if (frontend_scene_animation_active) {
        int8_t status;

        if (pad_pressed != 0) {
            frontend_scene_animation_active = 0;
            frontend_scene_animation_wait = 0;
            if (!frontend_frame_pending) frontend_scene_dispatch(0);
            return;
        }
        if (frontend_frame_pending) return;
        if (frontend_scene_animation_wait != 0) {
            frontend_scene_animation_wait--;
            return;
        }
        status = ot_data_anm_decode_next(
            &frontend_scene_anm,
            frontend_frame_scratch,
            FRONTEND_FRAME_BYTES,
            SCREEN_WIDTH,
            SCREEN_HEIGHT
        );
        if (status > 0) {
            frontend_present(
                frontend_frame_scratch,
                (const u8 *)(const void *)frontend_scene_palette
            );
            frontend_scene_animation_wait =
                FRONTEND_SCENE_ANM_FRAME_VBLANKS;
            return;
        }
        frontend_scene_animation_active = 0;
        frontend_scene_dispatch(0);
        return;
    }
    if (!frontend_scene_waiting_input) {
        frontend_scene_dispatch(0);
        return;
    }
    if (frontend_scene_action.warning) {
        if (frontend_scene_warning_bar_delay == 0) {
            s16 next = (s16)(
                frontend_scene_warning_colour +
                frontend_scene_warning_direction
            );

            if (next > 14 * 16 + 10 || next < 14 * 16 + 4) {
                frontend_scene_warning_direction =
                    (s8)-frontend_scene_warning_direction;
                next = (s16)(
                    frontend_scene_warning_colour +
                    frontend_scene_warning_direction
                );
            }
            frontend_scene_warning_colour = (u8)next;
            memset(
                frontend_frame_scratch,
                frontend_scene_warning_colour,
                5u * SCREEN_WIDTH
            );
            memset(
                frontend_frame_scratch +
                    (SCREEN_HEIGHT - 5u) * SCREEN_WIDTH,
                frontend_scene_warning_colour,
                5u * SCREEN_WIDTH
            );
            frontend_dirty_begin();
            frontend_dirty_add(0, 0, SCREEN_WIDTH, 5);
            frontend_dirty_add(
                0,
                SCREEN_HEIGHT - 5,
                SCREEN_WIDTH,
                5
            );
            frontend_dirty_present();
            frontend_scene_warning_bar_delay = 5;
        } else {
            frontend_scene_warning_bar_delay--;
        }
        if (frontend_scene_warning_delay == 0) {
            source_sound_effect(17); /* OpenTyrian S_WARNING. */
            frontend_scene_warning_delay =
                FRONTEND_SCENE_WARNING_SOUND_VBLANKS;
        } else {
            frontend_scene_warning_delay--;
        }
    }
    if (pad_pressed == 0 || frontend_frame_pending) return;
    frontend_scene_waiting_input = 0;
    if (frontend_scene_end_episode) {
        frontend_scene_finish();
    } else {
        frontend_scene_dispatch(0);
    }
}
