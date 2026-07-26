/*
 * OpenTyrian front-end state adapter with a GBA-native presentation path.
 *
 * PIC/SHP/HDT decoding and all static composition happen in build_assets.py.
 * Runtime selection changes are one raw 240x160 Mode-4 DMA to the hidden
 * bitmap page, followed by a VBlank page flip.  Invalid keys do no work.
 */

enum {
    FRONTEND_MODE4_PAGE_BYTES = 0xA000,
    FRONTEND_DYNAMIC_PERCENT_GLYPH = 10,
    FRONTEND_PENDING_NONE = 0,
    FRONTEND_PENDING_FULL_FRAME = 1,
    FRONTEND_PENDING_SELECTION_PATCH = 2,
};

#define FRONTEND_MODE4_PAGE0 ((u8 *)0x06000000)
#define FRONTEND_MODE4_PAGE1 \
    ((u8 *)(0x06000000 + FRONTEND_MODE4_PAGE_BYTES))

_Static_assert(
    FRONTEND_FRAME_BYTES == SCREEN_WIDTH * SCREEN_HEIGHT,
    "Mode 4 frame must be exactly 240x160 bytes"
);
_Static_assert(
    FRONTEND_FRAME_COUNT == 25,
    "generated front-end state atlas changed"
);
_Static_assert(
    FRONTEND_GLYPH_COUNT == 11 &&
        FRONTEND_GLYPH_BYTES == 64,
    "dynamic stats glyph atlas changed"
);

static const u8 frontend_glyph_advances[FRONTEND_GLYPH_COUNT] = {
    FRONTEND_GLYPH_0_ADVANCE,
    FRONTEND_GLYPH_1_ADVANCE,
    FRONTEND_GLYPH_2_ADVANCE,
    FRONTEND_GLYPH_3_ADVANCE,
    FRONTEND_GLYPH_4_ADVANCE,
    FRONTEND_GLYPH_5_ADVANCE,
    FRONTEND_GLYPH_6_ADVANCE,
    FRONTEND_GLYPH_7_ADVANCE,
    FRONTEND_GLYPH_8_ADVANCE,
    FRONTEND_GLYPH_9_ADVANCE,
    FRONTEND_GLYPH_10_ADVANCE,
};

static const u8 *frontend_frame_address(u8 frame)
{
    return frontend_frames + (u32)frame * FRONTEND_FRAME_BYTES;
}

static const u8 *frontend_palette_address(u8 frame)
{
    return frontend_palettes + (u32)frame * 512u;
}

static void frontend_set_affine_identity(void)
{
    REG_BG2PA = 1 << 8;
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = 1 << 8;
    REG_BG2X = 0;
    REG_BG2Y = 0;
    REG_BG2CNT = BG_PRIORITY(0);
}

static void frontend_show_immediate(
    const u8 *frame,
    const u8 *palette
)
{
    REG_DISPCNT = LCDC_OFF;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    oam_dirty = 0;
    frontend_set_affine_identity();
    dmaCopy(frame, FRONTEND_MODE4_PAGE0, FRONTEND_FRAME_BYTES);
    dmaCopy(palette, BG_PALETTE, 512);
    frontend_display_page = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_mode4_active = 1;
    REG_DISPCNT = MODE_4 | BG2_ON;
    game_paused = 0;
}

static void frontend_present(
    const u8 *frame,
    const u8 *palette
)
{
    if (!frontend_mode4_active) {
        frontend_show_immediate(frame, palette);
        return;
    }
    frontend_pending_frame = frame;
    frontend_pending_palette = palette;
    frontend_pending_kind = FRONTEND_PENDING_FULL_FRAME;
    frontend_frame_pending = 1;
}

static void frontend_present_index(u8 frame)
{
    if (frame >= FRONTEND_FRAME_COUNT) return;
    frontend_present(
        frontend_frame_address(frame),
        frontend_palette_address(frame)
    );
}

static u8 *frontend_visible_page(void)
{
    return frontend_display_page ?
        FRONTEND_MODE4_PAGE1 :
        FRONTEND_MODE4_PAGE0;
}

static void frontend_selection_rect(
    u8 state,
    u8 selection,
    u16 *x,
    u16 *y,
    u16 *width,
    u16 *height
)
{
    s16 source_y = 0;

    *x = 0;
    *width = SCREEN_WIDTH;
    *height = 12;
    switch (state) {
    case STATE_TITLE:
        source_y = (s16)(108 + selection * 24);
        break;
    case STATE_PLAY_MODE_MENU:
    case STATE_DIFFICULTY_MENU:
        source_y = (s16)(54 + selection * 24);
        break;
    case STATE_EPISODE_MENU:
        source_y = (s16)(50 + selection * 30);
        break;
    case STATE_GAME_MENU:
        source_y = (s16)(
            38 + selection * 16 + (selection == 5 ? 16 : 0)
        );
        *x = 120;
        *width = 120;
        *height = 9;
        break;
    case STATE_NEXT_LEVEL_MENU:
        source_y = selection == 0 ? 38 : 150;
        *x = 120;
        *width = 120;
        *height = 9;
        break;
    default:
        *height = 0;
        break;
    }
    *y = source_y > 0 ?
        (u16)((u32)source_y * SCREEN_HEIGHT / 200u) :
        0;
    if (*y + *height > SCREEN_HEIGHT) {
        *height = (u16)(SCREEN_HEIGHT - *y);
    }
}

static void frontend_commit_rect(
    const u8 *source_frame,
    u8 *target_frame,
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        u32 offset = (u32)(y + row) * SCREEN_WIDTH + x;

        dmaCopy(
            source_frame + offset,
            target_frame + offset,
            width
        );
    }
}

static void frontend_commit_selection_patch(void)
{
    u16 old_x;
    u16 old_y;
    u16 old_width;
    u16 old_height;
    u16 new_x;
    u16 new_y;
    u16 new_width;
    u16 new_height;
    u8 *target = frontend_visible_page();

    frontend_selection_rect(
        frontend_patch_state,
        frontend_patch_old_selection,
        &old_x,
        &old_y,
        &old_width,
        &old_height
    );
    frontend_selection_rect(
        frontend_patch_state,
        frontend_patch_new_selection,
        &new_x,
        &new_y,
        &new_width,
        &new_height
    );
    if (old_height != 0) {
        frontend_commit_rect(
            frontend_pending_frame,
            target,
            old_x,
            old_y,
            old_width,
            old_height
        );
    }
    if (
        new_height != 0 &&
        (
            new_x != old_x ||
            new_y != old_y ||
            new_width != old_width ||
            new_height != old_height
        )
    ) {
        frontend_commit_rect(
            frontend_pending_frame,
            target,
            new_x,
            new_y,
            new_width,
            new_height
        );
    }
}

static void frontend_commit_vblank(void)
{
    u8 next_page;
    u8 *target;

    if (!frontend_frame_pending) return;
    if (frontend_pending_kind == FRONTEND_PENDING_SELECTION_PATCH) {
        frontend_commit_selection_patch();
        frontend_frame_pending = 0;
        frontend_pending_kind = FRONTEND_PENDING_NONE;
        return;
    }
    next_page = frontend_display_page ^ 1u;
    target = next_page ?
        FRONTEND_MODE4_PAGE1 :
        FRONTEND_MODE4_PAGE0;
    dmaCopy(
        frontend_pending_frame,
        target,
        FRONTEND_FRAME_BYTES
    );
    dmaCopy(frontend_pending_palette, BG_PALETTE, 512);
    frontend_display_page = next_page;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    REG_DISPCNT =
        MODE_4 | BG2_ON |
        (frontend_display_page ? BACKBUFFER : 0);
}

static void frontend_blit_glyph(
    u8 *frame,
    u8 glyph,
    s16 x,
    s16 y
)
{
    const u8 *source;
    u8 row;

    if (glyph >= FRONTEND_GLYPH_COUNT) return;
    source = frontend_glyphs + (u32)glyph * FRONTEND_GLYPH_BYTES;
    for (row = 0; row < 8; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < 8; column++) {
            u8 pixel = source[(u16)row * 8u + column];
            s16 target_x = (s16)(x + column);

            if (
                pixel != 0xff &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x
                ] = pixel;
            }
        }
    }
}

static s16 frontend_draw_u32(
    u8 *frame,
    u32 value,
    s16 x,
    s16 y
)
{
    u8 digit[10];
    u8 count = 0;

    do {
        digit[count++] = (u8)(value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digit));
    while (count != 0) {
        u8 current = digit[--count];

        frontend_blit_glyph(frame, current, x, y);
        x = (s16)(x + frontend_glyph_advances[current]);
    }
    return x;
}

static u8 frontend_stats_frame(void)
{
    if (frontend_play_mode == FRONTEND_PLAY_ARCADE) {
        return FRONTEND_FRAME_STATS_ARCADE;
    }
    return source_parity_level.data_cube_pickup_count != 0 ?
        FRONTEND_FRAME_STATS_FULL_COLLECTED :
        FRONTEND_FRAME_STATS_FULL_NONE;
}

static void frontend_render_stats(void)
{
    u8 frame = frontend_stats_frame();
    u32 killed_percent =
        source_parity_level.total_enemy == 0 ?
            0 :
            (
                source_parity_level.enemy_kill_count * 100u +
                source_parity_level.total_enemy / 2u
            ) /
            source_parity_level.total_enemy;
    s16 x;

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(frame),
        FRONTEND_FRAME_BYTES
    );
    frontend_draw_u32(
        frontend_frame_scratch,
        player_cash,
        FRONTEND_STATS_CASH_X,
        FRONTEND_STATS_CASH_Y
    );
    x = frontend_draw_u32(
        frontend_frame_scratch,
        killed_percent,
        FRONTEND_STATS_KILLED_X,
        FRONTEND_STATS_KILLED_Y
    );
    frontend_blit_glyph(
        frontend_frame_scratch,
        FRONTEND_DYNAMIC_PERCENT_GLYPH,
        x,
        FRONTEND_STATS_KILLED_Y
    );
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(frame)
    );
}

static u8 frontend_static_frame_for_state(
    u8 state,
    u8 selection
)
{
    switch (state) {
    case STATE_INTRO_LOGO_1:
        return FRONTEND_FRAME_INTRO_LOGO_1;
    case STATE_INTRO_LOGO_2:
        return FRONTEND_FRAME_INTRO_LOGO_2;
    case STATE_TITLE:
        return (u8)(FRONTEND_FRAME_TITLE_BASE + selection);
    case STATE_PLAY_MODE_MENU:
        return (u8)(FRONTEND_FRAME_PLAY_MODE_BASE + selection);
    case STATE_EPISODE_MENU:
        return (u8)(FRONTEND_FRAME_EPISODE_BASE + selection);
    case STATE_DIFFICULTY_MENU:
        return (u8)(FRONTEND_FRAME_DIFFICULTY_BASE + selection);
    case STATE_GAME_MENU:
        return (u8)(FRONTEND_FRAME_GAME_MENU_BASE + selection);
    case STATE_NEXT_LEVEL_MENU:
        return (u8)(FRONTEND_FRAME_NEXT_LEVEL_BASE + selection);
    case STATE_GAME_OVER:
        return FRONTEND_FRAME_GAME_OVER;
    default:
        return FRONTEND_FRAME_COUNT;
    }
}

static void frontend_redraw(void)
{
    u8 frame;

    if (game_state == STATE_LEVEL_STATS) {
        frontend_render_stats();
        return;
    }
    frame = frontend_static_frame_for_state(
        game_state,
        frontend_selection
    );
    frontend_present_index(frame);
}

static void frontend_redraw_selection(u8 old_selection)
{
    u8 frame = frontend_static_frame_for_state(
        game_state,
        frontend_selection
    );

    if (
        frame >= FRONTEND_FRAME_COUNT ||
        !frontend_mode4_active ||
        frontend_pending_kind == FRONTEND_PENDING_FULL_FRAME
    ) {
        frontend_present_index(frame);
        return;
    }
    frontend_pending_frame = frontend_frame_address(frame);
    frontend_patch_state = game_state;
    frontend_patch_old_selection = old_selection;
    frontend_patch_new_selection = frontend_selection;
    frontend_pending_kind = FRONTEND_PENDING_SELECTION_PATCH;
    frontend_frame_pending = 1;
}

static void frontend_enter_state(u8 state, u8 selection)
{
    game_state = state;
    frontend_selection = selection;
    frontend_redraw();
    telemetry_state_transitions++;
}

static void enter_title(void)
{
    frontend_enter_state(STATE_TITLE, 0);
    audio_load_music(MOD_TYRIAN_TITLE_FULL);
}

static void frontend_begin(void)
{
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = 0;
    frontend_difficulty = FRONTEND_DIFFICULTY_NORMAL;
    frontend_level_completed = 0;
    frontend_timer = 200;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_state = STATE_INTRO_LOGO_1;
    frontend_selection = 0;
    mmStop();
    frontend_redraw();
    telemetry_state_transitions++;
}

static void frontend_next_intro(void)
{
    if (game_state == STATE_INTRO_LOGO_1) {
        frontend_timer = 200;
        frontend_enter_state(STATE_INTRO_LOGO_2, 0);
    } else {
        enter_title();
    }
}

static void frontend_move_selection(u8 item_count, s8 direction)
{
    u8 old_selection = frontend_selection;

    if (direction < 0) {
        frontend_selection =
            frontend_selection == 0 ?
                (u8)(item_count - 1) :
                (u8)(frontend_selection - 1);
    } else {
        frontend_selection =
            frontend_selection + 1 >= item_count ?
                0 :
                (u8)(frontend_selection + 1);
    }
    frontend_redraw_selection(old_selection);
}

static void frontend_update(void)
{
    u16 confirm = pad_pressed & (KEY_A | KEY_START);

    if (
        game_state == STATE_INTRO_LOGO_1 ||
        game_state == STATE_INTRO_LOGO_2
    ) {
        if (pad_pressed != 0 || frontend_timer == 0) {
            frontend_next_intro();
        } else {
            frontend_timer--;
        }
        return;
    }

    if (game_state == STATE_LEVEL_STATS) {
        if (pad_pressed != 0) {
            frontend_enter_state(STATE_GAME_MENU, 4);
            audio_load_music(MOD_TYRIAN_TITLE_FULL);
#ifdef AUTOTEST
            autotest_frontend_finish_pending = 1;
#endif
        }
        return;
    }
    if (game_state == STATE_GAME_OVER) {
        if (pad_pressed != 0) {
            frontend_enter_state(STATE_GAME_MENU, 4);
        }
        return;
    }

    if (pad_pressed & KEY_UP) {
        switch (game_state) {
        case STATE_TITLE:
        case STATE_PLAY_MODE_MENU:
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(2, -1);
            break;
        case STATE_EPISODE_MENU:
            frontend_move_selection(4, -1);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_move_selection(3, -1);
            break;
        case STATE_GAME_MENU:
            frontend_move_selection(6, -1);
            break;
        default:
            break;
        }
        return;
    }
    if (pad_pressed & KEY_DOWN) {
        switch (game_state) {
        case STATE_TITLE:
        case STATE_PLAY_MODE_MENU:
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(2, 1);
            break;
        case STATE_EPISODE_MENU:
            frontend_move_selection(4, 1);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_move_selection(3, 1);
            break;
        case STATE_GAME_MENU:
            frontend_move_selection(6, 1);
            break;
        default:
            break;
        }
        return;
    }

    if (pad_pressed & KEY_B) {
        switch (game_state) {
        case STATE_PLAY_MODE_MENU:
            enter_title();
            break;
        case STATE_EPISODE_MENU:
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
            break;
        case STATE_DIFFICULTY_MENU:
            frontend_enter_state(STATE_EPISODE_MENU, frontend_episode);
            break;
        case STATE_GAME_MENU:
            enter_title();
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_enter_state(STATE_GAME_MENU, 4);
            break;
        default:
            break;
        }
        return;
    }
    if (!confirm) return;

    switch (game_state) {
    case STATE_TITLE:
        if (frontend_selection == 0) {
            frontend_enter_state(STATE_PLAY_MODE_MENU, 0);
        }
        /* JukeBox remains visible but intentionally disabled. */
        break;
    case STATE_PLAY_MODE_MENU:
        frontend_play_mode = frontend_selection;
        frontend_enter_state(STATE_EPISODE_MENU, 0);
        break;
    case STATE_EPISODE_MENU:
        frontend_episode = frontend_selection;
        frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        break;
    case STATE_DIFFICULTY_MENU:
        frontend_difficulty =
            (u8)(FRONTEND_DIFFICULTY_EASY + frontend_selection);
        frontend_enter_state(STATE_GAME_MENU, 4);
        break;
    case STATE_GAME_MENU:
        if (frontend_selection == 4) {
            frontend_enter_state(STATE_NEXT_LEVEL_MENU, 0);
        } else if (frontend_selection == 5) {
            enter_title();
        }
        /* Data Cubes through Options remain visible but disabled. */
        break;
    case STATE_NEXT_LEVEL_MENU:
        if (frontend_selection == 0) {
            enter_level();
        } else {
            frontend_enter_state(STATE_GAME_MENU, 4);
        }
        break;
    default:
        break;
    }
}

static void frontend_enter_level_stats(u8 completed)
{
    frontend_level_completed = completed;
    frontend_enter_state(STATE_LEVEL_STATS, 0);
}

static void frontend_enter_game_over(void)
{
    frontend_enter_state(STATE_GAME_OVER, 0);
    /*
     * The level module loops indefinitely, so leaving gameplay must select
     * the front-end source song explicitly before Game Over / Game Menu.
     */
    audio_load_music(MOD_TYRIAN_TITLE_FULL);
}
