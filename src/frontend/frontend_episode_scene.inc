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
    FRONTEND_SCENE_PIC_OUTPUT_BUDGET = 2048,
    FRONTEND_SCENE_SCALE_ROW_BUDGET = 24,
    FRONTEND_SCENE_TEXT_ROW_BUDGET = 2,
    FRONTEND_SCENE_PICTURE_JOB_NONE = 0,
    FRONTEND_SCENE_PICTURE_JOB_DECODE,
    FRONTEND_SCENE_PICTURE_JOB_SCALE,
    FRONTEND_SCENE_PICTURE_JOB_PRESENT,
    FRONTEND_SCENE_TEXT_PHASE_PREPARE = 1,
    FRONTEND_SCENE_TEXT_PHASE_SCORE,
    FRONTEND_SCENE_TEXT_PHASE_LINES,
    FRONTEND_SCENE_TEXT_PHASE_PROMPT,
};

typedef struct {
    const char *remaining;
    s16 y;
    s16 prompt_y;
    u8 active;
    u8 episode_end;
    u8 phase;
    u8 line;
    u8 line_started;
    u8 row_step;
    u8 colour;
    u8 shadow;
} FrontendSceneTextJob;

static OtPicReader frontend_scene_pic_reader EWRAM_BSS;
static FrontendSceneTextJob frontend_scene_text_job EWRAM_BSS;
static u8 frontend_scene_picture_job EWRAM_BSS;
static u8 frontend_scene_picture_scale_row EWRAM_BSS;

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

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
frontend_scene_scale_pc_frame_rows(
    u8 first_row,
    u8 row_count
)
{
    u8 *frame = (u8 *)(void *)&frontend_gameplay_arena;
    u8 y;

    /*
     * Destination addresses always precede unread source addresses, so the
     * stock 64,000-byte decode can be reduced in place without a second
     * frame buffer.  This is the exact 3/4 x 4/5 full-screen transform.
     */
    if (first_row >= SCREEN_HEIGHT) return;
    if (first_row + row_count > SCREEN_HEIGHT) {
        row_count = (u8)(SCREEN_HEIGHT - first_row);
    }
    for (y = first_row; y < first_row + row_count; y++) {
        /* floor(y * 5 / 4), matching the former 200/160 expression. */
        u16 source_y = (u16)(y + (y >> 2));
        const u32 *source = (const u32 *)(const void *)(
            frame + (u32)source_y * OT_PIC_DECODED_WIDTH
        );
        u32 *destination = (u32 *)(void *)(
            frame + (u32)y * SCREEN_WIDTH
        );
        u8 block;

        /*
         * Exact floor(x * 4 / 3) nearest-neighbour projection.  Every four
         * aligned source words become three destination words, discarding
         * source columns 3, 7, 11 and 15.  This SWAR path replaces 5,760
         * per-pixel address calculations in a 24-row slice with 480 compact
         * register packs and is safe in-place because destination trails the
         * unread 320-wide source.
         */
        for (block = 0; block < SCREEN_WIDTH / 12u; block++) {
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
            source += 4;
            destination += 3;
        }
    }
}

/*
 * Begin one source picture without monopolising a host frame.  Stock PIC RLE
 * decoding and the 320x200 -> 240x160 projection continue cooperatively from
 * frontend_episode_scene_update(), keeping maxmod input and VBlank service
 * alive during story transitions.
 */
static int8_t frontend_scene_picture_begin(u16 picture)
{
    u8 *decode = (u8 *)(void *)&frontend_gameplay_arena;
    OtDataView palette;

    frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
    frontend_scene_picture_scale_row = 0;
    if (picture > 900u) {
        u16 source_picture = (u16)(picture - 900u);

        if (
            source_picture == 0 ||
            source_picture > OT_PIC_COUNT ||
            !ot_data_pic_palette_view((u8)source_picture, &palette)
        ) {
            return -1;
        }
        frontend_scene_palette_rgb(&palette, 0);
        memset(frontend_frame_scratch, 0, FRONTEND_FRAME_BYTES);
        frontend_present(
            frontend_frame_scratch,
            (const u8 *)(const void *)frontend_scene_palette
        );
        return 0;
    } else if (picture == 0) {
        if (!ot_data_pcx_decode(
                "tshp2.pcx",
                decode,
                sizeof(frontend_gameplay_arena),
                &palette
            )) {
            return -1;
        }
        frontend_scene_palette_rgb(&palette, 1);
        frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_SCALE;
        return 1;
    } else {
        if (
            picture > OT_PIC_COUNT ||
            !ot_data_pic_decode_begin(
                (u8)picture,
                &frontend_scene_pic_reader
            ) ||
            !ot_data_pic_palette_view((u8)picture, &palette)
        ) {
            return -1;
        }
        frontend_scene_palette_rgb(&palette, 0);
        frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_DECODE;
        return 1;
    }
}

static void frontend_scene_dispatch(u8 action_ready);

static void frontend_scene_picture_update(void)
{
    u8 *decode = (u8 *)(void *)&frontend_gameplay_arena;

    if (
        frontend_scene_picture_job == FRONTEND_SCENE_PICTURE_JOB_NONE ||
        frontend_frame_pending
    ) {
        return;
    }
    if (frontend_scene_picture_job == FRONTEND_SCENE_PICTURE_JOB_DECODE) {
        int8_t status = ot_data_pic_decode_step(
            &frontend_scene_pic_reader,
            decode,
            sizeof(frontend_gameplay_arena),
            FRONTEND_SCENE_PIC_OUTPUT_BUDGET
        );

        if (status > 0) return;
        if (status < 0) {
            frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
            frontend_scene_dispatch(0);
            return;
        }
        frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_SCALE;
        frontend_scene_picture_scale_row = 0;
    }
    if (frontend_scene_picture_job == FRONTEND_SCENE_PICTURE_JOB_SCALE) {
        u8 rows = FRONTEND_SCENE_SCALE_ROW_BUDGET;

        if (
            frontend_scene_picture_scale_row + rows > SCREEN_HEIGHT
        ) {
            rows = (u8)(
                SCREEN_HEIGHT - frontend_scene_picture_scale_row
            );
        }
        frontend_scene_scale_pc_frame_rows(
            frontend_scene_picture_scale_row,
            rows
        );
        frontend_scene_picture_scale_row = (u8)(
            frontend_scene_picture_scale_row + rows
        );
        if (frontend_scene_picture_scale_row < SCREEN_HEIGHT) return;
        frontend_present(
            frontend_frame_scratch,
            (const u8 *)(const void *)frontend_scene_palette
        );
        frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_PRESENT;
        return;
    }
    if (frontend_scene_picture_job == FRONTEND_SCENE_PICTURE_JOB_PRESENT) {
        frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
        frontend_scene_dispatch(0);
    }
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
 * wrap only at this final presentation boundary.  Use the thin authored
 * 4x8 mixed-case face shown by the clear v67 story reference and used by
 * the current Data reader.  The older source-derived 5-row projection makes
 * neighbouring strokes merge into the blocky appearance explicitly rejected
 * for story prose.
 */
static char frontend_scene_wrap_scratch[
    OT_EPISODE_SCENE_TEXT_LINE_BYTES
] EWRAM_BSS;
static char frontend_scene_score_scratch[61] EWRAM_BSS;

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
            frontend_small_mixed_font_advance(
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
    char *segment = frontend_scene_wrap_scratch;
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

static const char *frontend_scene_draw_text_segment(
    const char *text,
    s16 y,
    u8 colour,
    u8 shadow
)
{
    char *segment = frontend_scene_wrap_scratch;
    const char *remaining;
    const s16 maximum_width =
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT -
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_X;

    remaining = frontend_scene_wrap_segment(
        text,
        segment,
        maximum_width
    );
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        segment,
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_X,
        y,
        TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT,
        colour,
        shadow
    );
    return remaining;
}

static void frontend_scene_text_prepare_layout(
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
    char *score = frontend_scene_score_scratch;

    if (action == 0) return;
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
            sizeof(frontend_scene_score_scratch),
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
    frontend_scene_text_job.y = y;
    frontend_scene_text_job.prompt_y = prompt_y;
    frontend_scene_text_job.row_step = row_step;
    frontend_scene_text_job.colour = colour;
    frontend_scene_text_job.shadow = shadow;
    frontend_scene_text_job.line = 0;
    frontend_scene_text_job.line_started = 0;
    frontend_scene_text_job.remaining = episode_end ? score : 0;
    frontend_scene_text_job.phase = episode_end ?
        FRONTEND_SCENE_TEXT_PHASE_SCORE :
        FRONTEND_SCENE_TEXT_PHASE_LINES;
}

static void frontend_scene_text_begin(u8 episode_end)
{
    frontend_scene_text_job = (FrontendSceneTextJob){0};
    frontend_scene_text_job.active = 1;
    frontend_scene_text_job.episode_end = episode_end;
    frontend_scene_text_job.phase = FRONTEND_SCENE_TEXT_PHASE_PREPARE;
    frontend_scene_waiting_input = 0;
}

static void frontend_scene_text_update(void)
{
    u8 row_budget = FRONTEND_SCENE_TEXT_ROW_BUDGET;
    const char *prompt = frontend_stats_text(4, "PRESS A KEY");

    if (!frontend_scene_text_job.active || frontend_frame_pending) return;
    if (
        frontend_scene_text_job.phase ==
            FRONTEND_SCENE_TEXT_PHASE_PREPARE
    ) {
        frontend_scene_text_prepare_layout(
            &frontend_scene_action,
            frontend_scene_text_job.episode_end
        );
    }
    while (frontend_scene_text_job.active && row_budget != 0) {
        if (
            frontend_scene_text_job.phase ==
                FRONTEND_SCENE_TEXT_PHASE_SCORE
        ) {
            frontend_scene_text_job.remaining =
                frontend_scene_draw_text_segment(
                    frontend_scene_text_job.remaining,
                    frontend_scene_text_job.y,
                    frontend_scene_text_job.colour,
                    frontend_scene_text_job.shadow
                );
            frontend_scene_text_job.y = (s16)(
                frontend_scene_text_job.y +
                frontend_scene_text_job.row_step
            );
            row_budget--;
            if (frontend_scene_text_job.remaining == 0) {
                frontend_scene_text_job.y = (s16)(
                    frontend_scene_text_job.y +
                    frontend_scene_text_job.row_step
                );
                frontend_scene_text_job.phase =
                    FRONTEND_SCENE_TEXT_PHASE_LINES;
            }
            continue;
        }
        if (
            frontend_scene_text_job.phase ==
                FRONTEND_SCENE_TEXT_PHASE_LINES
        ) {
            if (
                frontend_scene_text_job.line >=
                    frontend_scene_action.text_line_count
            ) {
                frontend_scene_text_job.phase =
                    FRONTEND_SCENE_TEXT_PHASE_PROMPT;
                continue;
            }
            if (!frontend_scene_text_job.line_started) {
                frontend_scene_text_job.remaining =
                    frontend_scene_action.text[
                        frontend_scene_text_job.line
                    ];
                frontend_scene_text_job.line_started = 1;
                if (*frontend_scene_text_job.remaining == '\0') {
                    frontend_scene_text_job.y = (s16)(
                        frontend_scene_text_job.y +
                        frontend_scene_text_job.row_step
                    );
                    frontend_scene_text_job.line++;
                    frontend_scene_text_job.line_started = 0;
                    row_budget--;
                    continue;
                }
            }
            frontend_scene_text_job.remaining =
                frontend_scene_draw_text_segment(
                    frontend_scene_text_job.remaining,
                    frontend_scene_text_job.y,
                    frontend_scene_text_job.colour,
                    frontend_scene_text_job.shadow
                );
            frontend_scene_text_job.y = (s16)(
                frontend_scene_text_job.y +
                frontend_scene_text_job.row_step
            );
            row_budget--;
            if (frontend_scene_text_job.remaining == 0) {
                frontend_scene_text_job.line++;
                frontend_scene_text_job.line_started = 0;
            }
            continue;
        }
        frontend_data_fine_text_clipped_colours(
            frontend_frame_scratch,
            prompt,
            (s16)(
                SCREEN_WIDTH / 2 -
                frontend_data_fine_text_width(prompt) / 2
            ),
            frontend_scene_text_job.prompt_y,
            SCREEN_WIDTH,
            frontend_scene_text_job.colour,
            frontend_scene_text_job.shadow
        );
        frontend_present(
            frontend_frame_scratch,
            (const u8 *)(const void *)frontend_scene_palette
        );
        frontend_scene_text_job.active = 0;
        frontend_scene_waiting_input = 1;
    }
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
    frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
    frontend_scene_text_job.active = 0;
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
    frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
    frontend_scene_text_job = (FrontendSceneTextJob){0};
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_state = STATE_EPISODE_SCENE;
    frontend_selection = 0;
    frontend_scene_clear(5); /* OpenTyrian palettes[6 - 1]. */
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        episode_name,
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_data_fine_text_width(episode_name) / 2
        ),
        104, /* source y=130 scaled from 200 to 160 */
        SCREEN_WIDTH,
        colour,
        shadow
    );
    frontend_data_fine_text_clipped_colours(
        frontend_frame_scratch,
        prompt,
        (s16)(
            SCREEN_WIDTH / 2 -
            frontend_data_fine_text_width(prompt) / 2
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
            if (
                frontend_scene_picture_begin(
                    frontend_scene_action.value
                ) > 0
            ) {
                return;
            }
            break;

        case OT_EPISODE_SCENE_ACTION_MUSIC:
            if (frontend_scene_action.value < JUKEBOX_MUSIC_COUNT) {
                audio_load_source_song_preserve_effects(
                    (u8)frontend_scene_action.value
                );
            }
            break;

        case OT_EPISODE_SCENE_ACTION_TEXT:
            frontend_scene_text_begin(0);
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
            frontend_scene_text_begin(1);
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
    frontend_scene_picture_job = FRONTEND_SCENE_PICTURE_JOB_NONE;
    frontend_scene_picture_scale_row = 0;
    frontend_scene_text_job = (FrontendSceneTextJob){0};
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
    if (frontend_scene_picture_job != FRONTEND_SCENE_PICTURE_JOB_NONE) {
        frontend_scene_picture_update();
        return;
    }
    if (frontend_scene_text_job.active) {
        frontend_scene_text_update();
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
