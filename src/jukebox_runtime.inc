/*
 * OpenTyrian jukebox.c / starlib.c adapter.
 *
 * The source draws 1,000 software-rendered 3D stars into a 320x200 surface.
 * GBA keeps the same z-depth projection, respawn, setup-change, song-loop
 * fade and circular selection rules, while presentation uses one tiled
 * parallax BG plus 112 hardware OBJ stars.  Text is a 2 KiB tile map patch;
 * no key path rebuilds or DMA-copies a 38.4 KiB bitmap frame.
 */

enum {
    JUKEBOX_STAR_COUNT = 112,
    JUKEBOX_TEXT_SCREEN_BLOCK = 31,
    JUKEBOX_BACKDROP_SCREEN_BLOCK = 30,
    JUKEBOX_TEXT_PALETTE_TITLE = 8,
    JUKEBOX_TEXT_PALETTE_BODY = 9,
    JUKEBOX_TEXT_PALETTE_TRACK = 10,
    JUKEBOX_TEXT_PALETTE_DIM = 11,
    JUKEBOX_VISIBLE_COLUMNS = 30,
    JUKEBOX_VISIBLE_ROWS = 20,
    JUKEBOX_SOURCE_VOLUME = 255,
    JUKEBOX_FADE_STEPS = 16,
    JUKEBOX_STAR_SPEED = 2,
};

typedef struct {
    s16 sp_x;
    s16 sp_y;
    u16 sp_z;
    u8 hue;
} JukeboxStar;

static const char jukebox_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-";
static const u8 jukebox_setup_choices[] = {0, 2, 3, 6, 8};

static JukeboxStar jukebox_stars[JUKEBOX_STAR_COUNT] EWRAM_DATA;
static u16 jukebox_text_map[32 * 32] EWRAM_DATA;
static u16 jukebox_bg_palette_shadow[256] EWRAM_DATA;
static u16 jukebox_obj_palette_shadow[256] EWRAM_DATA;
static u32 jukebox_rng_state;
static u16 jukebox_setup_timer;
static u16 jukebox_scroll_phase;
static u16 jukebox_phase;
static u16 jukebox_last_order;
static u8 jukebox_setup;
static u8 jukebox_song;
static u8 jukebox_source_volume;
static u8 jukebox_fading_song;
static u8 jukebox_hide_text;
static u8 jukebox_quitting;
static u8 jukebox_fade_level;
static u8 jukebox_palette_phase;
static u8 jukebox_text_map_dirty;
static u8 jukebox_palette_dirty;

volatile u32 telemetry_jukebox_frames;
volatile u32 telemetry_jukebox_track_changes;
volatile u32 telemetry_jukebox_previous_wraps;
volatile u32 telemetry_jukebox_next_wraps;
volatile u32 telemetry_jukebox_text_toggles;
volatile u32 telemetry_jukebox_text_map_commits;
volatile u32 telemetry_jukebox_palette_commits;
volatile u32 telemetry_jukebox_natural_advances;
volatile u32 telemetry_jukebox_max_oam;
volatile u32 telemetry_jukebox_exits;

_Static_assert(
    JUKEBOX_STAR_COUNT < HARDWARE_OAM_ENTRIES,
    "Jukebox must leave spare hardware OAM entries"
);
_Static_assert(
    sizeof(jukebox_font_characters) - 1 ==
        JUKEBOX_FONT_TILE_COUNT - 1,
    "Jukebox runtime font order changed"
);
_Static_assert(
    JUKEBOX_RECIPROCAL_MAX_Z == 500 &&
        JUKEBOX_SINE_COUNT == 256,
    "Jukebox projection tables changed"
);

static u32 jukebox_random(void)
{
    u32 value = jukebox_rng_state;

    if (value == 0) value = 0x4a554b45u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    jukebox_rng_state = value;
    return value;
}

static s16 jukebox_sine_value(u8 phase)
{
    return ((const s16 *)jukebox_sine)[phase];
}

static void jukebox_new_star(JukeboxStar *star)
{
    s32 radius;
    u8 phase = (u8)jukebox_phase;

    star->sp_z = JUKEBOX_RECIPROCAL_MAX_Z;
    star->hue = (u8)(jukebox_random() & 7u);
    switch (jukebox_setup) {
    case 2:
        star->sp_x = (s16)(
            (s32)jukebox_sine_value((u8)(phase + 64u)) *
            20000 /
            32767
        );
        star->sp_y = (s16)(
            (s32)jukebox_sine_value(phase) *
            20000 /
            32767
        );
        jukebox_phase = (u16)(jukebox_phase + 11u);
        break;
    case 3:
        radius = 4000 + (phase & 63u) * 400;
        star->sp_x = (s16)(
            (s32)jukebox_sine_value((u8)(phase + 64u)) *
            radius /
            32767
        );
        star->sp_y = (s16)(
            (s32)jukebox_sine_value((u8)(phase * 3u)) *
            radius /
            32767
        );
        jukebox_phase = (u16)(jukebox_phase + 7u);
        break;
    case 6:
        star->sp_x = (s16)(
            (s32)jukebox_sine_value((u8)(phase + 64u)) *
            30000 /
            32767
        );
        star->sp_y = (s16)(
            (s32)jukebox_sine_value(phase) *
            16000 /
            32767
        );
        jukebox_phase = (u16)(jukebox_phase + 17u);
        break;
    case 8:
        radius = 12000 + (
            jukebox_sine_value((u8)(phase * 3u)) < 0 ?
                -jukebox_sine_value((u8)(phase * 3u)) :
                jukebox_sine_value((u8)(phase * 3u))
        ) / 2;
        star->sp_x = (s16)(
            (s32)jukebox_sine_value((u8)(phase + 64u)) *
            radius /
            32767
        );
        star->sp_y = (s16)(
            (s32)jukebox_sine_value(phase) *
            18000 /
            32767
        );
        jukebox_phase = (u16)(jukebox_phase + 13u);
        break;
    default:
        star->sp_x = (s16)(
            (s32)(jukebox_random() % 64000u) - 32000
        );
        star->sp_y = (s16)(
            (s32)(jukebox_random() % 40000u) - 20000
        );
        break;
    }
}

static void jukebox_change_setup(void)
{
    jukebox_setup = jukebox_setup_choices[
        jukebox_random() %
            (sizeof(jukebox_setup_choices) /
                sizeof(jukebox_setup_choices[0]))
    ];
    jukebox_setup_timer = (u16)(300u + jukebox_random() % 1000u);
    jukebox_phase = 0;
}

static u8 jukebox_font_tile(char character)
{
    u8 index;

    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character == ' ') return 0;
    for (
        index = 0;
        index < sizeof(jukebox_font_characters) - 1;
        index++
    ) {
        if (jukebox_font_characters[index] == character) {
            return (u8)(index + 1);
        }
    }
    for (
        index = 0;
        index < sizeof(jukebox_font_characters) - 1;
        index++
    ) {
        if (jukebox_font_characters[index] == '?') {
            return (u8)(index + 1);
        }
    }
    return 0;
}

static void jukebox_draw_text_span(
    u8 row,
    u8 palette,
    const char *text,
    u8 length
)
{
    u8 column;

    if (row >= JUKEBOX_VISIBLE_ROWS) return;
    if (length > JUKEBOX_VISIBLE_COLUMNS) {
        length = JUKEBOX_VISIBLE_COLUMNS;
    }
    column = (u8)((JUKEBOX_VISIBLE_COLUMNS - length) / 2u);
    while (length-- != 0) {
        jukebox_text_map[(u16)row * 32u + column] =
            jukebox_font_tile(*text++) |
            (u16)palette << 12;
        column++;
    }
}

static void jukebox_draw_text(
    u8 row,
    u8 palette,
    const char *text
)
{
    u8 length = 0;

    while (
        text[length] != '\0' &&
        length < JUKEBOX_VISIBLE_COLUMNS
    ) {
        length++;
    }
    jukebox_draw_text_span(row, palette, text, length);
}

static void jukebox_rebuild_text_map(void)
{
    const char *source_title = (
        (const char *)jukebox_titles +
        (u32)jukebox_song * JUKEBOX_TITLE_BYTES
    );
    char title[JUKEBOX_TITLE_BYTES + 1];
    char counter[] = "TRACK 00 / 41";
    u8 length = 0;
    u8 split;
    u16 index;

    for (index = 0; index < 32u * 32u; index++) {
        jukebox_text_map[index] = 0;
    }
    if (jukebox_hide_text) {
        jukebox_text_map_dirty = 1;
        return;
    }

    jukebox_draw_text(
        2,
        JUKEBOX_TEXT_PALETTE_TITLE,
        "TYRIAN JUKEBOX"
    );
    counter[6] = (char)('0' + (jukebox_song + 1) / 10u);
    counter[7] = (char)('0' + (jukebox_song + 1) % 10u);
    jukebox_draw_text(
        7,
        JUKEBOX_TEXT_PALETTE_DIM,
        counter
    );

    while (
        length < JUKEBOX_TITLE_BYTES &&
        source_title[length] != '\0'
    ) {
        char character = source_title[length];

        title[length] = (
            character >= 'a' && character <= 'z' ?
                (char)(character - 'a' + 'A') :
                character
        );
        length++;
    }
    title[length] = '\0';
    if (length <= 28) {
        jukebox_draw_text_span(
            10,
            JUKEBOX_TEXT_PALETTE_TRACK,
            title,
            length
        );
    } else {
        split = 28;
        while (split > 0 && title[split] != ' ') split--;
        if (split == 0) split = 28;
        jukebox_draw_text_span(
            9,
            JUKEBOX_TEXT_PALETTE_TRACK,
            title,
            split
        );
        while (split < length && title[split] == ' ') split++;
        jukebox_draw_text_span(
            11,
            JUKEBOX_TEXT_PALETTE_TRACK,
            title + split,
            (u8)(length - split)
        );
    }

    jukebox_draw_text(
        16,
        JUKEBOX_TEXT_PALETTE_BODY,
        "D-PAD / A CHANGE TRACK"
    );
    jukebox_draw_text(
        17,
        JUKEBOX_TEXT_PALETTE_BODY,
        "B / START EXIT"
    );
    jukebox_draw_text(
        18,
        JUKEBOX_TEXT_PALETTE_DIM,
        "SELECT HIDE TEXT"
    );
    jukebox_text_map_dirty = 1;
}

static u16 jukebox_scale_colour(u16 colour, u8 scale)
{
    u16 red = colour & 31u;
    u16 green = (colour >> 5) & 31u;
    u16 blue = (colour >> 10) & 31u;

    red = (u16)(red * scale / JUKEBOX_FADE_STEPS);
    green = (u16)(green * scale / JUKEBOX_FADE_STEPS);
    blue = (u16)(blue * scale / JUKEBOX_FADE_STEPS);
    return red | green << 5 | blue << 10;
}

static void jukebox_rebuild_palettes(void)
{
    const u16 *source_bg = (const u16 *)jukebox_bg_palette;
    const u16 *source_obj = (const u16 *)jukebox_obj_palette;
    u8 rotation = (u8)((jukebox_palette_phase >> 4) & 7u);
    u16 bank;

    for (bank = 0; bank < 16; bank++) {
        u16 colour;
        u16 source_bank = bank < 8 ?
            (u16)((bank + rotation) & 7u) :
            bank;

        for (colour = 0; colour < 16; colour++) {
            jukebox_bg_palette_shadow[bank * 16u + colour] =
                jukebox_scale_colour(
                    source_bg[source_bank * 16u + colour],
                    jukebox_fade_level
                );
            jukebox_obj_palette_shadow[bank * 16u + colour] =
                bank < 8 ?
                    jukebox_scale_colour(
                        source_obj[source_bank * 16u + colour],
                        jukebox_fade_level
                    ) :
                    0;
        }
    }
    jukebox_palette_dirty = 1;
}

static void jukebox_play_song(u8 song, u8 natural)
{
    jukebox_song = song;
    jukebox_source_volume = JUKEBOX_SOURCE_VOLUME;
    jukebox_fading_song = 0;
    jukebox_last_order = 0;
    audio_load_source_song(song);
    jukebox_rebuild_text_map();
    telemetry_jukebox_track_changes++;
    if (natural) telemetry_jukebox_natural_advances++;
}

static void jukebox_previous_song(void)
{
    if (jukebox_song == 0) {
        jukebox_song = JUKEBOX_MUSIC_COUNT - 1;
        telemetry_jukebox_previous_wraps++;
    } else {
        jukebox_song--;
    }
    jukebox_play_song(jukebox_song, 0);
}

static void jukebox_next_song(void)
{
    jukebox_song++;
    if (jukebox_song >= JUKEBOX_MUSIC_COUNT) {
        jukebox_song = 0;
        telemetry_jukebox_next_wraps++;
    }
    jukebox_play_song(jukebox_song, 0);
}

static void jukebox_enter(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    u16 index;

    game_state = STATE_JUKEBOX;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    bg1_row_pending = 0;
    bg2_row_pending = 0;
    bg3_row_pending = 0;
    source_background2_upload_pending = 0;
    game_paused = 0;
    jukebox_song = (
        catalog != 0 &&
        catalog->selected_mus_song < JUKEBOX_MUSIC_COUNT
    ) ?
        (u8)catalog->selected_mus_song :
        29;
#ifdef AUTOTEST_JUKEBOX_FLOW
    jukebox_song = 0;
    audio_load_source_song(jukebox_song);
#endif
    jukebox_source_volume = JUKEBOX_SOURCE_VOLUME;
    jukebox_fading_song = 0;
    jukebox_hide_text = 0;
    jukebox_quitting = 0;
    jukebox_fade_level = 0;
    jukebox_palette_phase = 0;
    jukebox_scroll_phase = 0;
    jukebox_last_order = mmActive() ? mmGetPosition() : 0;
    jukebox_rng_state = 0x4a554b45u;
    jukebox_setup = 2;
    jukebox_setup_timer = 600;
    jukebox_phase = 0;

    telemetry_jukebox_frames = 0;
    telemetry_jukebox_track_changes = 0;
    telemetry_jukebox_previous_wraps = 0;
    telemetry_jukebox_next_wraps = 0;
    telemetry_jukebox_text_toggles = 0;
    telemetry_jukebox_text_map_commits = 0;
    telemetry_jukebox_palette_commits = 0;
    telemetry_jukebox_natural_advances = 0;
    telemetry_jukebox_max_oam = 0;
    telemetry_jukebox_exits = 0;

    for (index = 0; index < JUKEBOX_STAR_COUNT; index++) {
        jukebox_stars[index].sp_z = (u16)(
            1u +
            (u32)index *
                (JUKEBOX_RECIPROCAL_MAX_Z - 1u) /
                JUKEBOX_STAR_COUNT
        );
        jukebox_new_star(&jukebox_stars[index]);
        jukebox_stars[index].sp_z = (u16)(
            1u +
            (u32)index *
                (JUKEBOX_RECIPROCAL_MAX_Z - 1u) /
                JUKEBOX_STAR_COUNT
        );
    }
    jukebox_rebuild_text_map();
    jukebox_rebuild_palettes();

    REG_DISPCNT = LCDC_OFF;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    oam_dirty = 0;
    dmaCopy(
        jukebox_font_tiles,
        CHAR_BASE_BLOCK(0),
        JUKEBOX_FONT_TILE_COUNT * 32u
    );
    dmaCopy(
        jukebox_backdrop_tiles,
        CHAR_BASE_BLOCK(1),
        JUKEBOX_BACKDROP_TILE_COUNT * 32u
    );
    dmaCopy(
        jukebox_backdrop_map,
        SCREEN_BASE_BLOCK(JUKEBOX_BACKDROP_SCREEN_BLOCK),
        32u * 32u * 2u
    );
    dmaCopy(
        jukebox_text_map,
        SCREEN_BASE_BLOCK(JUKEBOX_TEXT_SCREEN_BLOCK),
        sizeof(jukebox_text_map)
    );
    dmaCopy(
        jukebox_bg_palette_shadow,
        BG_PALETTE,
        sizeof(jukebox_bg_palette_shadow)
    );
    dmaCopy(
        jukebox_obj_tiles,
        OBJ_BASE_ADR,
        JUKEBOX_STAR_TILE_COUNT * 32u
    );
    dmaCopy(
        jukebox_obj_palette_shadow,
        SPRITE_PALETTE,
        sizeof(jukebox_obj_palette_shadow)
    );
    jukebox_text_map_dirty = 0;
    jukebox_palette_dirty = 0;
    REG_BG0CNT =
        BG_PRIORITY(0) |
        CHAR_BASE(0) |
        SCREEN_BASE(JUKEBOX_TEXT_SCREEN_BLOCK) |
        BG_16_COLOR |
        BG_SIZE_0;
    REG_BG1CNT =
        BG_PRIORITY(3) |
        CHAR_BASE(1) |
        SCREEN_BASE(JUKEBOX_BACKDROP_SCREEN_BLOCK) |
        BG_16_COLOR |
        BG_SIZE_0;
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    REG_DISPCNT =
        MODE_0 | BG0_ON | BG1_ON |
        OBJ_ON | OBJ_1D_MAP;
    telemetry_state_transitions++;
}

static void jukebox_commit_vblank(void)
{
    REG_BG1HOFS = (jukebox_scroll_phase >> 1) & 255u;
    REG_BG1VOFS = (jukebox_scroll_phase >> 2) & 255u;
    if (jukebox_text_map_dirty) {
        dmaCopy(
            jukebox_text_map,
            SCREEN_BASE_BLOCK(JUKEBOX_TEXT_SCREEN_BLOCK),
            sizeof(jukebox_text_map)
        );
        jukebox_text_map_dirty = 0;
        telemetry_jukebox_text_map_commits++;
    }
    if (jukebox_palette_dirty) {
        dmaCopy(
            jukebox_bg_palette_shadow,
            BG_PALETTE,
            sizeof(jukebox_bg_palette_shadow)
        );
        dmaCopy(
            jukebox_obj_palette_shadow,
            SPRITE_PALETTE,
            sizeof(jukebox_obj_palette_shadow)
        );
        jukebox_palette_dirty = 0;
        telemetry_jukebox_palette_commits++;
    }
}

static void jukebox_update(void)
{
    u16 position;

    telemetry_jukebox_frames++;
    jukebox_scroll_phase++;
    jukebox_palette_phase++;
    if ((jukebox_palette_phase & 15u) == 0) {
        jukebox_rebuild_palettes();
    }

    if (jukebox_setup_timer > 0) {
        jukebox_setup_timer--;
    } else {
        jukebox_change_setup();
    }

    if (jukebox_quitting) {
        if (jukebox_fade_level > 0) {
            jukebox_fade_level--;
            jukebox_rebuild_palettes();
        } else {
            telemetry_jukebox_exits++;
            enter_title();
        }
        return;
    }
    if (jukebox_fade_level < JUKEBOX_FADE_STEPS) {
        jukebox_fade_level++;
        jukebox_rebuild_palettes();
    }

    if (mmActive()) {
        position = mmGetPosition();
        if (
            !jukebox_fading_song &&
            position < jukebox_last_order
        ) {
            jukebox_fading_song = 1;
        }
        jukebox_last_order = position;
    }
    if (jukebox_fading_song) {
        if (jukebox_source_volume > 5) {
            jukebox_source_volume = (u8)(
                jukebox_source_volume > 2 ?
                    jukebox_source_volume - 2 :
                    0
            );
            audio_set_source_music_volume(jukebox_source_volume);
        } else {
            jukebox_play_song(
                (u8)(jukebox_random() % JUKEBOX_MUSIC_COUNT),
                1
            );
        }
    }

    if (pad_pressed & (KEY_B | KEY_START)) {
        jukebox_quitting = 1;
        return;
    }
    if (pad_pressed & KEY_SELECT) {
        jukebox_hide_text ^= 1u;
        telemetry_jukebox_text_toggles++;
        jukebox_rebuild_text_map();
        return;
    }
    if (pad_pressed & (KEY_LEFT | KEY_UP)) {
        jukebox_previous_song();
        return;
    }
    if (pad_pressed & (KEY_RIGHT | KEY_DOWN | KEY_A)) {
        jukebox_next_song();
    }
}

static void jukebox_render(void)
{
    const u32 *reciprocal = (const u32 *)jukebox_reciprocal;
    u8 old_count = previous_oam_count;
    u8 visible_count;
    u16 index;

    oam_count = 0;
    for (index = 0; index < JUKEBOX_STAR_COUNT; index++) {
        JukeboxStar *star = &jukebox_stars[index];
        s16 x;
        s16 y;
        u16 z = star->sp_z;
        u8 tile;
        u8 palette;

        if (z <= JUKEBOX_STAR_SPEED) {
            jukebox_new_star(star);
            continue;
        }
        x = (s16)(
            120 +
            (
                (s32)star->sp_x *
                (s32)reciprocal[z]
            >> 16)
        );
        y = (s16)(
            80 +
            (
                (s32)star->sp_y *
                (s32)reciprocal[z]
            >> 16)
        );
        star->sp_z = (u16)(z - JUKEBOX_STAR_SPEED);
        if (x < 1 || x > 238 || y < 1 || y > 158) {
            jukebox_new_star(star);
            continue;
        }
        tile = z < 80 ? 2 : (z < 190 ? 1 : 0);
        palette = (u8)(
            (star->hue + (jukebox_palette_phase >> 4)) & 7u
        );
        put_sprite(
            (s16)(x - 3),
            (s16)(y - 3),
            tile,
            palette,
            ATTR1_SIZE_8,
            2
        );
    }
    visible_count = oam_count;
    while (oam_count < old_count) {
        oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
        oam_count++;
    }
    previous_oam_count = visible_count;
    if (visible_count > telemetry_jukebox_max_oam) {
        telemetry_jukebox_max_oam = visible_count;
    }
    oam_dirty = 1;
}
