/*
 * OpenTyrian front-end state adapter with a GBA-native presentation path.
 *
 * Static chrome is composed once from the original PIC/SHP/HDT files, while
 * all level destinations and names are read from ROMFS levelsN.dat at
 * runtime.  Selection changes patch only the affected Mode-4 rows.
 */

enum {
    FRONTEND_MODE4_PAGE_BYTES = 0xA000,
    FRONTEND_DYNAMIC_PERCENT_GLYPH = 10,
    FRONTEND_PENDING_NONE = 0,
    FRONTEND_PENDING_FULL_FRAME = 1,
    FRONTEND_PENDING_SELECTION_PATCH = 2,
    FRONTEND_PENDING_DIRTY_RECTS = 3,
    FRONTEND_STATS_STAGE_COMPLETED = 0,
    FRONTEND_STATS_STAGE_CASH = 1,
    FRONTEND_STATS_STAGE_ENEMIES = 2,
    FRONTEND_STATS_STAGE_CUBES = 3,
    FRONTEND_STATS_STAGE_FINAL = 4,
    FRONTEND_STATS_STAGE_TICKS = 30,
    FRONTEND_STATS_CUBE_TICKS = 18,
    FRONTEND_STATS_MAX_CUBES = 4,
    FRONTEND_DEMO_COUNT = 5,
    FRONTEND_DEMO_IDLE_VBLANKS = 1800,
    FRONTEND_NEXT_PANEL_X = TYRIAN_GBA_LAYOUT_NEXT_PANEL_X,
    FRONTEND_NEXT_PANEL_Y = TYRIAN_GBA_LAYOUT_NEXT_PANEL_Y,
    FRONTEND_NEXT_PANEL_WIDTH = TYRIAN_GBA_LAYOUT_NEXT_PANEL_WIDTH,
    FRONTEND_NEXT_PANEL_HEIGHT = TYRIAN_GBA_LAYOUT_NEXT_PANEL_HEIGHT,
    FRONTEND_NEXT_CHOICE_Y = TYRIAN_GBA_LAYOUT_NEXT_CHOICE_FIRST_Y,
    FRONTEND_NEXT_CHOICE_STEP = TYRIAN_GBA_LAYOUT_NEXT_CHOICE_ROW_STEP,
    FRONTEND_NEXT_EXIT_Y = TYRIAN_GBA_LAYOUT_NEXT_EXIT_Y,
    FRONTEND_NEXT_ITEM_HEIGHT = 10,
    FRONTEND_NEXT_BACKGROUND_COLOUR = 228,
    /*
     * Palette 17 deliberately replaces the Game Menu background for the
     * navigation screen, but its source 0xfa/0xfb/0xfe ramp is colour-
     * compatible with palette 0.  Retain the same normal/title/selected
     * relationship instead of the old dark-red 0xf4/0xf6 approximation.
     */
    FRONTEND_NEXT_TEXT_COLOUR = 250,
    FRONTEND_NEXT_TITLE_COLOUR = 251,
    FRONTEND_NEXT_SELECTED_COLOUR = 254,
    FRONTEND_NEXT_SHADOW_COLOUR = 240,
    FRONTEND_NEXT_FONT_WIDTH = 6,
    FRONTEND_NEXT_FONT_SPACE = 3,
    FRONTEND_MENU_FONT_MAX_ADVANCE = 7,
    FRONTEND_MENU_FONT_SPACE = 3,
    FRONTEND_SMALL_MIXED_FONT_WIDTH = 4,
    FRONTEND_SMALL_MIXED_FONT_SPACE = 2,
    FRONTEND_COMPACT_FONT_WIDTH = 5,
    FRONTEND_COMPACT_FONT_SPACE = 3,
    FRONTEND_PREGAME_FONT_MAX_ADVANCE = 8,
    FRONTEND_PREGAME_FONT_SPACE = 4,
    FRONTEND_UPGRADE_CATEGORY_COUNT = 7,
    FRONTEND_UPGRADE_MENU_ITEM_COUNT = 8,
    FRONTEND_UPGRADE_SHIP = 0,
    FRONTEND_UPGRADE_FRONT_WEAPON = 1,
    FRONTEND_UPGRADE_REAR_WEAPON = 2,
    FRONTEND_UPGRADE_SHIELD = 3,
    FRONTEND_UPGRADE_GENERATOR = 4,
    FRONTEND_UPGRADE_LEFT_SIDEKICK = 5,
    FRONTEND_UPGRADE_RIGHT_SIDEKICK = 6,
    FRONTEND_UPGRADE_DONE = 7,
    FRONTEND_WEAPON_FRONT = 0,
    FRONTEND_WEAPON_REAR = 1,
    FRONTEND_DIRTY_RECT_CAPACITY = 8,
    FRONTEND_NAV_DIRTY_X = 0,
    FRONTEND_NAV_DIRTY_Y = 0,
    FRONTEND_NAV_DIRTY_WIDTH = 126,
    FRONTEND_NAV_DIRTY_HEIGHT = 138,
    FRONTEND_NAV_OBJ_SLOT_CAPACITY = 21,
    FRONTEND_NAV_OBJ_UPLOAD_CAPACITY = 24,
    FRONTEND_NAV_OBJ_VRAM_TILE_BASE = 512,
    FRONTEND_NAV_OBJ_DOT_OFF_OFFSET = 0,
    FRONTEND_NAV_OBJ_DOT_ON_OFFSET = 64,
    FRONTEND_NAV_OBJ_DOT_DIM = 8,
    FRONTEND_NAV_WINDOW_X0 = 21,
    FRONTEND_NAV_WINDOW_X1 = 101,
    FRONTEND_NAV_WINDOW_Y0 = 26,
    FRONTEND_NAV_WINDOW_Y1 = 132,
};

#define FRONTEND_MODE4_PAGE0 ((u8 *)0x06000000)
#define FRONTEND_MODE4_PAGE1 \
    ((u8 *)(0x06000000 + FRONTEND_MODE4_PAGE_BYTES))
#define FRONTEND_NAV_OBJ_VRAM ((u8 *)0x06014000)

_Static_assert(
    FRONTEND_FRAME_BYTES == SCREEN_WIDTH * SCREEN_HEIGHT,
    "Mode 4 frame must be exactly 240x160 bytes"
);
_Static_assert(
    FRONTEND_FRAME_COUNT == 34,
    "generated front-end state atlas changed"
);
_Static_assert(
    FRONTEND_MENU_SOURCE_CROP_X == 0 &&
        FRONTEND_MENU_SOURCE_WIDTH == 300 &&
        FRONTEND_NEXT_LEVEL_PALETTE_INDEX == 17,
    "front-end 300x200 crop or OpenTyrian navigation palette changed"
);
_Static_assert(
    FRONTEND_GLYPH_COUNT == 11 &&
        FRONTEND_GLYPH_BYTES == 64,
    "dynamic stats glyph atlas changed"
);
_Static_assert(
    FRONTEND_NAV_OBJ_META_BYTES == 12 &&
        FRONTEND_NAV_OBJ_SCALE_PHASES == 5 &&
        FRONTEND_NAV_OBJ_PHASE_COUNT == 25 &&
        FRONTEND_NAV_OBJ_CATALOG_COUNT == 153,
    "generated navigation OBJ catalog changed"
);
_Static_assert(
    FRONTEND_NAV_OBJ_DOT_BYTES +
        FRONTEND_NAV_OBJ_WORST_PLANET_BYTES <=
        FRONTEND_NAV_OBJ_VRAM_BYTES &&
        FRONTEND_NAV_OBJ_VRAM_BYTES == 0x4000,
    "navigation OBJ atlas exceeds Mode-4 OBJ VRAM"
);
_Static_assert(
    FRONTEND_NAV_BITMAP_WIDTH == FRONTEND_NAV_DIRTY_WIDTH &&
        FRONTEND_NAV_BITMAP_HEIGHT == FRONTEND_NAV_DIRTY_HEIGHT &&
        FRONTEND_NAV_BITMAP_STRIDE == 128 &&
        FRONTEND_NAV_GRID_PHASES == 15 &&
        FRONTEND_NAV_BITMAP_PAGE_COUNT == 225 &&
        FRONTEND_NAV_BITMAP_PAGE_BYTES ==
            FRONTEND_NAV_BITMAP_STRIDE * FRONTEND_NAV_DIRTY_HEIGHT,
    "generated navigation bitmap phase table changed"
);
_Static_assert(
    FRONTEND_STATIC_MENU_PANEL_X == 120 &&
        FRONTEND_STATIC_MENU_PANEL_Y == 0 &&
        FRONTEND_STATIC_MENU_PANEL_WIDTH == 120 &&
        FRONTEND_STATIC_MENU_PANEL_HEIGHT == 120 &&
        FRONTEND_STATIC_MENU_PANEL_BYTES ==
            FRONTEND_STATIC_MENU_PANEL_WIDTH *
                FRONTEND_STATIC_MENU_PANEL_HEIGHT &&
        FRONTEND_STATIC_GAME_MENU_COUNT == 6 &&
        FRONTEND_STATIC_UPGRADE_MENU_COUNT == 8 &&
        FRONTEND_STATIC_UPGRADE_MENU_BASE ==
            FRONTEND_STATIC_GAME_MENU_COUNT &&
        FRONTEND_STATIC_MENU_PANEL_COUNT ==
            FRONTEND_STATIC_GAME_MENU_COUNT +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT &&
        FRONTEND_STATIC_MENU_PANELS_BYTES ==
            FRONTEND_STATIC_MENU_PANEL_COUNT *
                FRONTEND_STATIC_MENU_PANEL_BYTES,
    "generated static menu panel atlas changed"
);
_Static_assert(
    FRONTEND_STATIC_PRE_GAME_TITLE_BASE == 0 &&
        FRONTEND_STATIC_PRE_GAME_TITLE_COUNT == 3 &&
        FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE == 3 &&
        FRONTEND_STATIC_PRE_GAME_PLAY_MODE_COUNT == 2 &&
        FRONTEND_STATIC_PRE_GAME_EPISODE_BASE == 5 &&
        FRONTEND_STATIC_PRE_GAME_EPISODE_COUNT == 4 &&
        FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE == 9 &&
        FRONTEND_STATIC_PRE_GAME_DIFFICULTY_COUNT == 3 &&
        FRONTEND_STATIC_PRE_GAME_FRAME_COUNT == 12 &&
        FRONTEND_STATIC_PRE_GAME_FRAMES_BYTES ==
            FRONTEND_STATIC_PRE_GAME_FRAME_COUNT *
                FRONTEND_FRAME_BYTES,
    "generated static pre-game frame atlas changed"
);
_Static_assert(
    FRONTEND_STATIC_QUIT_OVERLAY_VERSION == 1 &&
        FRONTEND_STATIC_QUIT_OVERLAY_X % 4 == 0 &&
        FRONTEND_STATIC_QUIT_OVERLAY_WIDTH % 4 == 0 &&
        FRONTEND_STATIC_QUIT_OVERLAY_HEADER_BYTES == 16 &&
        FRONTEND_STATIC_QUIT_OVERLAY_PIXEL_BYTES ==
            FRONTEND_STATIC_QUIT_OVERLAY_WIDTH *
                FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT &&
        FRONTEND_STATIC_QUIT_OVERLAY_BYTES ==
            FRONTEND_STATIC_QUIT_OVERLAY_HEADER_BYTES +
                FRONTEND_STATIC_QUIT_OVERLAY_PIXEL_BYTES,
    "generated static quit-dialog overlay changed"
);
_Static_assert(
    FRONTEND_STATIC_QUIT_CHOICE_VERSION == 1 &&
        FRONTEND_STATIC_QUIT_CHOICE_COUNT == 2 &&
        FRONTEND_STATIC_QUIT_CHOICE_RUN_COUNT > 0 &&
        FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES > 8 &&
        FRONTEND_STATIC_QUIT_CHOICES_BYTES ==
            FRONTEND_STATIC_QUIT_CHOICE_COUNT *
                FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES,
    "generated static quit-choice overlays changed"
);
_Static_assert(
    FRONTEND_STATIC_QUIT_SHADE_VERSION == 1 &&
        FRONTEND_STATIC_QUIT_SHADE_RUN_COUNT > 0 &&
        FRONTEND_STATIC_QUIT_SHADE_PIXEL_COUNT > 0 &&
        FRONTEND_STATIC_QUIT_SHADE_BYTES ==
            8 + FRONTEND_STATIC_QUIT_SHADE_RUN_COUNT * 4,
    "generated static quit-shade runs changed"
);

typedef struct {
    u32 tile_offset;
    u16 tile_bytes;
    u8 source_width;
    u8 source_height;
    u8 screen_width;
    u8 screen_height;
    u16 reserved;
} FrontendNavObjMeta;

typedef struct {
    const u8 *source;
    u16 vram_offset;
    u16 bytes;
} FrontendNavObjUpload;

typedef struct {
    u8 planet_index;
    u8 graphic;
    u8 phase;
    u8 animated;
    u16 vram_offset;
    u16 reserve_bytes;
} FrontendNavObjSlot;

_Static_assert(
    sizeof(FrontendNavObjMeta) == FRONTEND_NAV_OBJ_META_BYTES,
    "navigation OBJ metadata ABI changed"
);

static FrontendNavObjUpload frontend_nav_obj_upload[
    FRONTEND_NAV_OBJ_UPLOAD_CAPACITY
] EWRAM_BSS;
static FrontendNavObjSlot frontend_nav_obj_slot[
    FRONTEND_NAV_OBJ_SLOT_CAPACITY
] EWRAM_BSS;
static u16 frontend_nav_obj_reserve_cache[21] EWRAM_BSS;
static u8 frontend_nav_obj_upload_count EWRAM_BSS;
static u8 frontend_nav_obj_slot_count EWRAM_BSS;
static u8 frontend_nav_obj_active EWRAM_BSS;
static u8 frontend_nav_obj_palette_pending EWRAM_BSS;
static u8 frontend_nav_obj_dot_tiles_pending EWRAM_BSS;
static s16 frontend_nav_presented_x EWRAM_BSS;
static s16 frontend_nav_presented_y EWRAM_BSS;

static void frontend_redraw(void);
static void frontend_enter_state(u8 state, u8 selection);
static void frontend_start_game_menu_transition(u8 flags);
static void frontend_start_upgrade_menu_transition(u8 selection);
static void frontend_start_quit_transition(u8 enter_dialog);
static void frontend_render_upgrade_submenu_update(
    u8 old_selection,
    u8 old_scroll
);
static void frontend_nav_obj_commit_vblank(void);
static void frontend_nav_obj_disable(void);
static u8 frontend_nav_dot_count(u8 destination);
static s16 frontend_nav_part_way(
    s16 start,
    s16 finish,
    u8 dots,
    u8 distance
);

static const u8 frontend_upgrade_item_group[
    FRONTEND_UPGRADE_CATEGORY_COUNT
] = {
    0, 1, 2, 8, 3, 5, 6
};

static u8 *frontend_player_item_field(
    FrontendPlayerItems *items,
    u8 category
)
{
    if (items == 0) return 0;
    switch (category) {
    case FRONTEND_UPGRADE_SHIP:
        return &items->ship;
    case FRONTEND_UPGRADE_FRONT_WEAPON:
        return &items->weapon[FRONTEND_WEAPON_FRONT].id;
    case FRONTEND_UPGRADE_REAR_WEAPON:
        return &items->weapon[FRONTEND_WEAPON_REAR].id;
    case FRONTEND_UPGRADE_SHIELD:
        return &items->shield;
    case FRONTEND_UPGRADE_GENERATOR:
        return &items->generator;
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
        return &items->sidekick[0];
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK:
        return &items->sidekick[1];
    default:
        return 0;
    }
}

static const u8 *frontend_player_item_field_const(
    const FrontendPlayerItems *items,
    u8 category
)
{
    return frontend_player_item_field(
        (FrontendPlayerItems *)items,
        category
    );
}

static void frontend_campaign_initialize(void)
{
    static const u32 initial_cash[OT_EPISODE_COUNT] = {
        10000u, 15000u, 20000u, 30000u
    };

    memset(
        &frontend_player_items,
        0,
        sizeof(frontend_player_items)
    );
    frontend_player_items.ship =
        frontend_play_mode == FRONTEND_PLAY_ARCADE ? 8 : 1;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id = 1;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power = 1;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id = 0;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power = 1;
    frontend_player_items.shield = 4;
    frontend_player_items.generator = 2;
    frontend_player_items.sidekick_level = 0;
    frontend_player_items.sidekick_series = 0;
    frontend_player_items.weapon_mode = 1;
    player_cash =
        frontend_play_mode == FRONTEND_PLAY_FULL_GAME &&
        frontend_episode < OT_EPISODE_COUNT ?
            initial_cash[frontend_episode] :
            0;
    frontend_player_items_valid = 1;
}

static void frontend_campaign_apply_to_level(void)
{
    if (!frontend_player_items_valid) return;

    source_parity_level.player_front_weapon_id =
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id;
    source_parity_level.player_front_weapon_power =
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power;
    source_parity_level.player_rear_weapon_id =
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id;
    source_parity_level.player_rear_weapon_power =
        frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power;
    source_parity_level.player_special =
        frontend_player_items.special;
    source_parity_level.player_sidekick[0] =
        frontend_player_items.sidekick[0];
    source_parity_level.player_sidekick[1] =
        frontend_player_items.sidekick[1];
    source_parity_level.player_generator =
        frontend_player_items.generator;
    source_parity_level.player_shield_item =
        frontend_player_items.shield;
    source_parity_level.player_ship =
        frontend_player_items.ship;
    source_parity_level.player_super_arcade_mode =
        frontend_player_items.super_arcade_mode;
    source_parity_level.player_sidekick_level =
        frontend_player_items.sidekick_level;
    source_parity_level.player_sidekick_series =
        frontend_player_items.sidekick_series;
    source_parity_level.player_weapon_mode =
        frontend_player_items.weapon_mode;
}

static void frontend_campaign_sync_from_level(void)
{
    if (!frontend_player_items_valid || frontend_demo_active) return;

    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id =
        source_parity_level.player_front_weapon_id;
    frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power =
        source_parity_level.player_front_weapon_power;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id =
        source_parity_level.player_rear_weapon_id;
    frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power =
        source_parity_level.player_rear_weapon_power;
    frontend_player_items.special =
        source_parity_level.player_special;
    frontend_player_items.sidekick[0] =
        source_parity_level.player_sidekick[0];
    frontend_player_items.sidekick[1] =
        source_parity_level.player_sidekick[1];
    frontend_player_items.generator =
        source_parity_level.player_generator;
    frontend_player_items.shield =
        source_parity_level.player_shield_item;
    frontend_player_items.ship =
        source_parity_level.player_ship;
    frontend_player_items.super_arcade_mode =
        source_parity_level.player_super_arcade_mode;
    frontend_player_items.sidekick_level =
        source_parity_level.player_sidekick_level;
    frontend_player_items.sidekick_series =
        source_parity_level.player_sidekick_series;
    frontend_player_items.weapon_mode =
        source_parity_level.player_weapon_mode;
}
_Static_assert(
    FRONTEND_CUBE_BYTES ==
        FRONTEND_CUBE_WIDTH * FRONTEND_CUBE_HEIGHT,
    "generated OPTION_SHAPES cube stamp changed"
);
static const char frontend_runtime_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-%";
static const char frontend_pregame_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789.,!?'/:-%";

#ifndef TYRIAN_GBA_UPGRADE_LIST_FONT_MODE
#define TYRIAN_GBA_UPGRADE_LIST_FONT_MODE 1
#endif

_Static_assert(
    TYRIAN_GBA_UPGRADE_LIST_FONT_MODE >= 0 &&
        TYRIAN_GBA_UPGRADE_LIST_FONT_MODE <= 2,
    "upgrade-list font mode must be 0=5x7, 1=adaptive or 2=8px-wide"
);

/*
 * The GBA-native 5x7 face is stored as one shared build asset.  The same
 * rows drive both runtime text and build-time static panel baking, so the
 * fast pages cannot drift visually from dynamic labels.
 */

/*
 * Dense 3x5 face for the Upgrade Ship inventory rows only.  Long stock
 * weapon names must share the right-hand panel with their source icon,
 * price and OWN marker; shrinking the general menu face would make every
 * other page harder to read.
 */
#if TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 1
static const u8 frontend_compact_font_rows[][6] = {
    {0x6,0x9,0x9,0xf,0x9,0x9}, /* A */
    {0xe,0x9,0xe,0x9,0x9,0xe}, /* B */
    {0x7,0x8,0x8,0x8,0x8,0x7}, /* C */
    {0xe,0x9,0x9,0x9,0x9,0xe}, /* D */
    {0xf,0x8,0xe,0x8,0x8,0xf}, /* E */
    {0xf,0x8,0xe,0x8,0x8,0x8}, /* F */
    {0x7,0x8,0xb,0x9,0x9,0x7}, /* G */
    {0x9,0x9,0xf,0x9,0x9,0x9}, /* H */
    {0x7,0x2,0x2,0x2,0x2,0x7}, /* I */
    {0x3,0x1,0x1,0x1,0x9,0x6}, /* J */
    {0x9,0xa,0xc,0xa,0x9,0x9}, /* K */
    {0x8,0x8,0x8,0x8,0x8,0xf}, /* L */
    {0x9,0xf,0xf,0x9,0x9,0x9}, /* M */
    {0x9,0xd,0xd,0xb,0xb,0x9}, /* N */
    {0x6,0x9,0x9,0x9,0x9,0x6}, /* O */
    {0xe,0x9,0xe,0x8,0x8,0x8}, /* P */
    {0x6,0x9,0x9,0x9,0xb,0x7}, /* Q */
    {0xe,0x9,0xe,0xa,0x9,0x9}, /* R */
    {0x7,0x8,0x6,0x1,0x1,0xe}, /* S */
    {0xf,0x2,0x2,0x2,0x2,0x2}, /* T */
    {0x9,0x9,0x9,0x9,0x9,0x6}, /* U */
    {0x9,0x9,0x9,0x9,0x6,0x6}, /* V */
    {0x9,0x9,0x9,0xf,0xf,0x9}, /* W */
    {0x9,0x9,0x6,0x6,0x9,0x9}, /* X */
    {0x9,0x9,0x6,0x2,0x2,0x2}, /* Y */
    {0xf,0x1,0x2,0x4,0x8,0xf}, /* Z */
    {0x6,0x9,0xb,0xd,0x9,0x6}, /* 0 */
    {0x2,0x6,0x2,0x2,0x2,0x7}, /* 1 */
    {0xe,0x1,0x2,0x4,0x8,0xf}, /* 2 */
    {0xe,0x1,0x6,0x1,0x1,0xe}, /* 3 */
    {0x2,0x6,0xa,0xf,0x2,0x2}, /* 4 */
    {0xf,0x8,0xe,0x1,0x1,0xe}, /* 5 */
    {0x7,0x8,0xe,0x9,0x9,0x6}, /* 6 */
    {0xf,0x1,0x2,0x4,0x4,0x4}, /* 7 */
    {0x6,0x9,0x6,0x9,0x9,0x6}, /* 8 */
    {0x6,0x9,0x7,0x1,0x1,0xe}, /* 9 */
    {0x0,0x0,0x0,0x0,0x2},     /* . */
    {0x0,0x0,0x0,0x2,0x4},     /* , */
    {0x2,0x2,0x2,0x2,0x0,0x2}, /* ! */
    {0xe,0x1,0x2,0x4,0x0,0x4}, /* ? */
    {0x2,0x2,0x0,0x0,0x0,0x0}, /* ' */
    {0x1,0x1,0x2,0x4,0x8,0x8}, /* / */
    {0x0,0x2,0x0,0x0,0x2,0x0}, /* : */
    {0x0,0x0,0xf,0x0,0x0,0x0}, /* - */
    {0x9,0x1,0x2,0x4,0x8,0x9}, /* % */
};

static const u8 frontend_tiny_font_rows[][5] = {
    {0x2,0x5,0x7,0x5,0x5}, /* A */
    {0x6,0x5,0x6,0x5,0x6}, /* B */
    {0x3,0x4,0x4,0x4,0x3}, /* C */
    {0x6,0x5,0x5,0x5,0x6}, /* D */
    {0x7,0x4,0x6,0x4,0x7}, /* E */
    {0x7,0x4,0x6,0x4,0x4}, /* F */
    {0x3,0x4,0x5,0x5,0x3}, /* G */
    {0x5,0x5,0x7,0x5,0x5}, /* H */
    {0x7,0x2,0x2,0x2,0x7}, /* I */
    {0x1,0x1,0x1,0x5,0x2}, /* J */
    {0x5,0x5,0x6,0x5,0x5}, /* K */
    {0x4,0x4,0x4,0x4,0x7}, /* L */
    {0x5,0x7,0x7,0x5,0x5}, /* M */
    {0x5,0x7,0x7,0x7,0x5}, /* N */
    {0x2,0x5,0x5,0x5,0x2}, /* O */
    {0x6,0x5,0x6,0x4,0x4}, /* P */
    {0x2,0x5,0x5,0x7,0x3}, /* Q */
    {0x6,0x5,0x6,0x5,0x5}, /* R */
    {0x3,0x4,0x2,0x1,0x6}, /* S */
    {0x7,0x2,0x2,0x2,0x2}, /* T */
    {0x5,0x5,0x5,0x5,0x7}, /* U */
    {0x5,0x5,0x5,0x5,0x2}, /* V */
    {0x5,0x5,0x7,0x7,0x5}, /* W */
    {0x5,0x5,0x2,0x5,0x5}, /* X */
    {0x5,0x5,0x2,0x2,0x2}, /* Y */
    {0x7,0x1,0x2,0x4,0x7}, /* Z */
    {0x2,0x5,0x5,0x5,0x2}, /* 0 */
    {0x2,0x6,0x2,0x2,0x7}, /* 1 */
    {0x6,0x1,0x2,0x4,0x7}, /* 2 */
    {0x6,0x1,0x2,0x1,0x6}, /* 3 */
    {0x1,0x3,0x5,0x7,0x1}, /* 4 */
    {0x7,0x4,0x6,0x1,0x6}, /* 5 */
    {0x3,0x4,0x6,0x5,0x2}, /* 6 */
    {0x7,0x1,0x2,0x2,0x2}, /* 7 */
    {0x2,0x5,0x2,0x5,0x2}, /* 8 */
    {0x2,0x5,0x3,0x1,0x6}, /* 9 */
    {0x0,0x0,0x0,0x2},     /* . */
    {0x0,0x0,0x2,0x4},     /* , */
    {0x2,0x2,0x2,0x0,0x2}, /* ! */
    {0x6,0x1,0x2,0x0,0x2}, /* ? */
    {0x2,0x2,0x0,0x0,0x0}, /* ' */
    {0x1,0x1,0x2,0x4,0x4}, /* / */
    {0x0,0x2,0x0,0x2,0x0}, /* : */
    {0x0,0x0,0x7,0x0,0x0}, /* - */
    {0x5,0x1,0x2,0x4,0x5}, /* % */
};
#endif

_Static_assert(
    sizeof(frontend_runtime_font_characters) - 1u ==
        JUKEBOX_FONT_TILE_COUNT - 1u,
    "shared runtime font order changed"
);
_Static_assert(
    sizeof(frontend_runtime_font_characters) - 1u ==
        FRONTEND_STATS_FONT_GLYPH_COUNT,
    "source stats glyph count changed"
);
_Static_assert(
    FRONTEND_NATIVE_FONT_GLYPH_COUNT ==
        sizeof(frontend_runtime_font_characters) - 1u &&
        FRONTEND_NATIVE_FONT_HEIGHT == 7 &&
        FRONTEND_NATIVE_FONT_BYTES ==
            FRONTEND_NATIVE_FONT_GLYPH_COUNT *
                FRONTEND_NATIVE_FONT_HEIGHT,
    "native front-end glyph catalog changed"
);
_Static_assert(
    FRONTEND_PREGAME_FONT_GLYPH_COUNT ==
        sizeof(frontend_pregame_font_characters) - 1u &&
        FRONTEND_PREGAME_FONT_HEIGHT == 8 &&
        FRONTEND_PREGAME_FONT_BYTES ==
            FRONTEND_PREGAME_FONT_GLYPH_COUNT *
                FRONTEND_PREGAME_FONT_HEIGHT,
    "mixed-case pre-game glyph catalog changed"
);
_Static_assert(
    FRONTEND_STATS_PREBAKED_GLYPH_COUNT ==
        FRONTEND_STATS_FONT_GLYPH_COUNT &&
        FRONTEND_STATS_PREBAKED_WIDTH_BYTES ==
            FRONTEND_STATS_FONT_GLYPH_COUNT &&
        FRONTEND_STATS_PREBAKED_TILE_COUNT ==
            FRONTEND_STATS_TILE_COUNT &&
        FRONTEND_STATS_PREBAKED_TILE_BYTES ==
            FRONTEND_STATS_TILE_BYTES &&
        FRONTEND_STATS_PREBAKED_CUBE_TILE_OFFSET ==
            FRONTEND_STATS_CUBE_TILE_BASE -
                FRONTEND_STATS_TILE_BASE,
    "prebaked source summary OBJ catalog changed"
);
#if TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 1
_Static_assert(
    sizeof(frontend_compact_font_rows) /
        sizeof(frontend_compact_font_rows[0]) ==
        sizeof(frontend_runtime_font_characters) - 1u,
    "compact inventory glyph catalog changed"
);
_Static_assert(
    sizeof(frontend_tiny_font_rows) /
        sizeof(frontend_tiny_font_rows[0]) ==
        sizeof(frontend_runtime_font_characters) - 1u,
    "tiny inventory glyph catalog changed"
);
#endif
_Static_assert(
    FRONTEND_STATS_TILE_BASE + FRONTEND_STATS_TILE_COUNT <=
        SOURCE_ENEMY_CACHE_LOWER_TILE_BASE +
            SOURCE_ENEMY_CACHE_LOWER_SLOT_COUNT *
                SOURCE_ENEMY_TILES_PER_SLOT,
    "stats font and cube must fit the end-level enemy-cache window"
);

static u8 frontend_prepare_map_begin(void)
{
    frontend_map = (OtEpisodeMap){0};
    memset(
        frontend_map_level,
        0,
        sizeof(frontend_map_level)
    );
    frontend_map_ready = 0;
    if (
        !ot_data_episode_map_resolve(
            (u8)(frontend_episode + 1u),
            frontend_main_section,
            frontend_play_mode,
            frontend_difficulty,
            &frontend_map
        )
    ) {
        return 0;
    }
    if (frontend_map.episode_complete) {
        frontend_map_ready = 1;
    }
    return 1;
}

static u8 frontend_prepare_map_choice(u8 choice)
{
    if (
        choice >= frontend_map.choice_count ||
        frontend_map.episode_complete
    ) {
        return 1;
    }
    if (
        !ot_data_episode_level_resolve(
            (u8)(frontend_episode + 1u),
            frontend_map.map_section[choice],
            frontend_play_mode,
            frontend_difficulty,
            &frontend_map_level[choice]
        )
    ) {
        frontend_map = (OtEpisodeMap){0};
        memset(
            frontend_map_level,
            0,
            sizeof(frontend_map_level)
        );
        frontend_map_ready = 0;
        return 0;
    }
    return 1;
}

static u8 frontend_prepare_map(void)
{
    u8 choice;

    if (!frontend_prepare_map_begin()) return 0;
    for (choice = 0; choice < frontend_map.choice_count; choice++) {
        if (!frontend_prepare_map_choice(choice)) return 0;
    }
    frontend_map_ready = 1;
    return 1;
}

static void frontend_load_game_menu_song(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    u8 song =
        frontend_map_ready &&
        frontend_map.menu_song_valid &&
        frontend_map.menu_song < JUKEBOX_MUSIC_COUNT ?
            frontend_map.menu_song :
            1;

    /*
     * OpenTyrian initializes songBuy to DEFAULT_SONG_BUY (one-based 2);
     * play_song() therefore receives zero-based song 1 whenever levelsN.dat
     * does not override it with ]i.
     *
     * Re-entering a static page must not stop and restart an already active
     * identical module.  Besides preserving the source song position, this
     * removes Maxmod's module-start work from repeat menu transitions.
     */
    if (
        catalog != 0 &&
        catalog->selected_mus_song == song &&
        mmActive()
    ) {
        return;
    }
    audio_load_source_song(song);
}

static u8 frontend_next_choice_count(void)
{
    if (
        !frontend_map_ready ||
        frontend_map.episode_complete ||
        frontend_map.choice_count == 0
    ) {
        return 0;
    }
    return frontend_play_mode == FRONTEND_PLAY_ARCADE ?
        1 :
        frontend_map.choice_count;
}

static u8 frontend_next_map_choice(u8 display_choice)
{
    if (frontend_play_mode == FRONTEND_PLAY_ARCADE) {
        return (u8)(frontend_map.choice_count - 1u);
    }
    return display_choice;
}

static const char *frontend_next_choice_label(u8 display_choice)
{
    u8 map_choice = frontend_next_map_choice(display_choice);
    u8 planet = frontend_map.map_planet[map_choice];

    if (
        frontend_text_ready &&
        planet > 0 &&
        planet <= 21 &&
        frontend_text.planet_name[planet - 1u][0] != '\0'
    ) {
        return frontend_text.planet_name[planet - 1u];
    }
    return frontend_map_level[map_choice].level_name;
}

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

static void frontend_commit_mode4_registers(void)
{
    u16 display =
        MODE_4 | BG2_ON |
        (frontend_display_page ? BACKBUFFER : 0);

    if (
        game_state == STATE_NEXT_LEVEL_MENU &&
        frontend_nav_obj_active
    ) {
        /*
         * The source OPTION_SHAPES frame has an irregular transparent map
         * aperture.  WIN0 uses its largest fully transparent GBA rectangle,
         * keeping hardware planets behind the ornate bitmap border without
         * spending OBJ VRAM on a duplicate mask.
         */
        REG_WIN0H =
            (FRONTEND_NAV_WINDOW_X0 << 8) |
            FRONTEND_NAV_WINDOW_X1;
        REG_WIN0V =
            (FRONTEND_NAV_WINDOW_Y0 << 8) |
            FRONTEND_NAV_WINDOW_Y1;
        REG_WININ = (1u << 2) | (1u << 4);
        REG_WINOUT = 1u << 2;
        REG_BG2CNT = BG_PRIORITY(1);
        display |=
            OBJ_ON |
            OBJ_1D_MAP |
            (1u << 13);
    } else {
        REG_WININ = 0;
        REG_WINOUT = 0;
        REG_BG2CNT = BG_PRIORITY(0);
    }
    REG_DISPCNT = display;
}

static void frontend_show_immediate(
    const u8 *frame,
    const u8 *palette
)
{
    REG_DISPCNT = LCDC_OFF;
    REG_BLDCNT = 0;
    REG_BLDALPHA = 0;
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
    frontend_dirty_count = 0;
    frontend_mode4_active = 1;
    frontend_commit_mode4_registers();
    game_paused = 0;
}

static void frontend_present(
    const u8 *frame,
    const u8 *palette
)
{
    telemetry_frontend_full_redraws++;
    if (!frontend_mode4_active) {
        frontend_show_immediate(frame, palette);
        return;
    }
    frontend_pending_frame = frame;
    frontend_pending_palette = palette;
    frontend_pending_kind = FRONTEND_PENDING_FULL_FRAME;
    frontend_dirty_count = 0;
    frontend_frame_pending = 1;
}

static void frontend_dirty_begin(void)
{
    frontend_dirty_count = 0;
}

static void frontend_dirty_add(
    s16 x,
    s16 y,
    s16 width,
    s16 height
)
{
    s16 right;
    s16 bottom;
    u8 index;

    if (width <= 0 || height <= 0) return;
    right = (s16)(x + width);
    bottom = (s16)(y + height);
    if (
        right <= 0 ||
        bottom <= 0 ||
        x >= SCREEN_WIDTH ||
        y >= SCREEN_HEIGHT
    ) {
        return;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > SCREEN_WIDTH) right = SCREEN_WIDTH;
    if (bottom > SCREEN_HEIGHT) bottom = SCREEN_HEIGHT;
    width = (s16)(right - x);
    height = (s16)(bottom - y);
    if (width <= 0 || height <= 0) return;

    /*
     * Merge overlapping rectangles.  This keeps the VBlank DMA call count
     * bounded while preserving exact row transfers for disjoint menu areas.
     */
    for (index = 0; index < frontend_dirty_count; index++) {
        s16 old_right = (s16)(
            frontend_dirty_x[index] + frontend_dirty_width[index]
        );
        s16 old_bottom = (s16)(
            frontend_dirty_y[index] + frontend_dirty_height[index]
        );

        if (
            x <= old_right &&
            right >= (s16)frontend_dirty_x[index] &&
            y <= old_bottom &&
            bottom >= (s16)frontend_dirty_y[index]
        ) {
            s16 merged_x =
                x < (s16)frontend_dirty_x[index] ?
                    x :
                    (s16)frontend_dirty_x[index];
            s16 merged_y =
                y < (s16)frontend_dirty_y[index] ?
                    y :
                    (s16)frontend_dirty_y[index];
            s16 merged_right =
                right > old_right ? right : old_right;
            s16 merged_bottom =
                bottom > old_bottom ? bottom : old_bottom;

            frontend_dirty_x[index] = (u16)merged_x;
            frontend_dirty_y[index] = (u16)merged_y;
            frontend_dirty_width[index] =
                (u16)(merged_right - merged_x);
            frontend_dirty_height[index] =
                (u16)(merged_bottom - merged_y);
            return;
        }
    }
    if (frontend_dirty_count >= FRONTEND_DIRTY_RECT_CAPACITY) {
        /*
         * The planned callers need at most four rectangles.  If a future
         * page exceeds that, merge the overflow into the final slot rather
         * than falling back to a hidden full-frame redraw.
         */
        index = (u8)(FRONTEND_DIRTY_RECT_CAPACITY - 1u);
        {
            s16 old_right = (s16)(
                frontend_dirty_x[index] + frontend_dirty_width[index]
            );
            s16 old_bottom = (s16)(
                frontend_dirty_y[index] + frontend_dirty_height[index]
            );
            s16 merged_x =
                x < (s16)frontend_dirty_x[index] ?
                    x :
                    (s16)frontend_dirty_x[index];
            s16 merged_y =
                y < (s16)frontend_dirty_y[index] ?
                    y :
                    (s16)frontend_dirty_y[index];
            s16 merged_right =
                right > old_right ? right : old_right;
            s16 merged_bottom =
                bottom > old_bottom ? bottom : old_bottom;

            frontend_dirty_x[index] = (u16)merged_x;
            frontend_dirty_y[index] = (u16)merged_y;
            frontend_dirty_width[index] =
                (u16)(merged_right - merged_x);
            frontend_dirty_height[index] =
                (u16)(merged_bottom - merged_y);
        }
        return;
    }
    index = frontend_dirty_count++;
    frontend_dirty_x[index] = (u16)x;
    frontend_dirty_y[index] = (u16)y;
    frontend_dirty_width[index] = (u16)width;
    frontend_dirty_height[index] = (u16)height;
}

static void frontend_dirty_present(void)
{
    if (frontend_dirty_count == 0) return;
    frontend_pending_frame = frontend_frame_scratch;
    frontend_pending_kind = FRONTEND_PENDING_DIRTY_RECTS;
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
        source_y = (s16)(108 + selection * 12);
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
        source_y = selection < frontend_next_choice_count() ?
            (s16)(
                FRONTEND_NEXT_CHOICE_Y +
                selection * FRONTEND_NEXT_CHOICE_STEP
            ) :
            FRONTEND_NEXT_EXIT_Y;
        *x = FRONTEND_NEXT_PANEL_X;
        *width = FRONTEND_NEXT_PANEL_WIDTH;
        *height = FRONTEND_NEXT_ITEM_HEIGHT;
        break;
    default:
        *height = 0;
        break;
    }
    *y = state == STATE_NEXT_LEVEL_MENU ?
        (u16)source_y :
        (
            source_y > 0 ?
                (u16)((u32)source_y * SCREEN_HEIGHT / 200u) :
                0
        );
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

static void frontend_commit_dirty_rects(void)
{
    u8 index;
    u8 *target = frontend_visible_page();

    for (index = 0; index < frontend_dirty_count; index++) {
        u32 bytes =
            (u32)frontend_dirty_width[index] *
            frontend_dirty_height[index];

        frontend_commit_rect(
            frontend_pending_frame,
            target,
            frontend_dirty_x[index],
            frontend_dirty_y[index],
            frontend_dirty_width[index],
            frontend_dirty_height[index]
        );
        telemetry_frontend_dirty_bytes += bytes;
    }
    telemetry_frontend_dirty_commits++;
    frontend_dirty_count = 0;
}

static void frontend_commit_vblank(void)
{
    u8 next_page;
    u8 *target;

    frontend_nav_obj_commit_vblank();
    if (frontend_frame_pending) {
        if (
            frontend_pending_kind ==
                FRONTEND_PENDING_SELECTION_PATCH
        ) {
            frontend_commit_selection_patch();
        } else if (
            frontend_pending_kind ==
                FRONTEND_PENDING_DIRTY_RECTS
        ) {
            frontend_commit_dirty_rects();
        } else {
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
        }
        frontend_frame_pending = 0;
        frontend_pending_kind = FRONTEND_PENDING_NONE;
    }
    frontend_commit_mode4_registers();
}

static u8 frontend_runtime_font_tile(char character)
{
    u8 index;

    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character == ' ') return 0;
    for (
        index = 0;
        index < sizeof(frontend_runtime_font_characters) - 1u;
        index++
    ) {
        if (
            frontend_runtime_font_characters[index] == character
        ) {
            return (u8)(index + 1u);
        }
    }
    for (
        index = 0;
        index < sizeof(frontend_runtime_font_characters) - 1u;
        index++
    ) {
        if (frontend_runtime_font_characters[index] == '?') {
            return (u8)(index + 1u);
        }
    }
    return 0;
}

static u8 frontend_runtime_font_advance_scaled(
    u8 tile_index,
    u8 glyph_width,
    u8 space_width
)
{
    u8 occupied;
    u8 row;
    u8 advance;

    if (tile_index == 0) return space_width;
    if (tile_index > FRONTEND_NATIVE_FONT_GLYPH_COUNT) {
        return glyph_width;
    }
    occupied = 0;
    for (row = 0; row < 7; row++) {
        occupied |= frontend_native_font[
            (u16)(tile_index - 1u) * FRONTEND_NATIVE_FONT_HEIGHT +
            row
        ];
    }
    advance = 1;
    while (occupied >>= 1) advance++;
    if (advance < glyph_width) advance++;
    return advance > glyph_width ? glyph_width : advance;
}

static void frontend_blit_runtime_font_scaled(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour,
    u8 glyph_width
)
{
    const u8 *glyph;
    u8 occupied = 0;
    u8 natural_width = 1;
    u8 row;

    (void)glyph_width;
    if (
        tile_index == 0 ||
        tile_index > FRONTEND_NATIVE_FONT_GLYPH_COUNT
    ) {
        return;
    }
    glyph =
        frontend_native_font +
        (u16)(tile_index - 1u) * FRONTEND_NATIVE_FONT_HEIGHT;
    for (row = 0; row < 7; row++) occupied |= glyph[row];
    while (occupied >>= 1) natural_width++;
    for (row = 0; row < 7; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < natural_width; column++) {
            s16 target_x = (s16)(x + column);

            if (
                (glyph[row] &
                    (1u << (natural_width - column - 1u))) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH +
                    (u16)target_x
                ] = colour;
            }
        }
    }
}

static void frontend_draw_runtime_text_clipped_scaled(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour,
    u8 glyph_width,
    u8 space_width
)
{
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_runtime_font_tile(*text++);
        u8 advance = frontend_runtime_font_advance_scaled(
            tile_index,
            glyph_width,
            space_width
        );

        if (x + advance > right) break;
        frontend_blit_runtime_font_scaled(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR,
            glyph_width
        );
        frontend_blit_runtime_font_scaled(
            frame,
            tile_index,
            x,
            y,
            colour,
            glyph_width
        );
        x = (s16)(x + advance);
    }
}

#if TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 1
static void frontend_blit_compact_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    const u8 *glyph;
    u8 row;

    if (
        tile_index == 0 ||
        tile_index >
            sizeof(frontend_compact_font_rows) /
                sizeof(frontend_compact_font_rows[0])
    ) {
        return;
    }
    glyph = frontend_compact_font_rows[tile_index - 1u];
    for (row = 0; row < 6; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < 4; column++) {
            s16 target_x = (s16)(x + column);

            if (
                (glyph[row] & (1u << (3u - column))) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH +
                    (u16)target_x
                ] = colour;
            }
        }
    }
}

static void frontend_blit_tiny_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    const u8 *glyph;
    u8 row;

    if (
        tile_index == 0 ||
        tile_index >
            sizeof(frontend_tiny_font_rows) /
                sizeof(frontend_tiny_font_rows[0])
    ) {
        return;
    }
    glyph = frontend_tiny_font_rows[tile_index - 1u];
    for (row = 0; row < 5; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < 3; column++) {
            s16 target_x = (s16)(x + column);

            if (
                (glyph[row] & (1u << (2u - column))) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH +
                    (u16)target_x
                ] = colour;
            }
        }
    }
}
#elif TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 2
static void frontend_blit_wide_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    const u8 *glyph;
    u8 occupied = 0;
    u8 natural_width = 1;
    u8 row;

    if (
        tile_index == 0 ||
        tile_index > FRONTEND_NATIVE_FONT_GLYPH_COUNT
    ) {
        return;
    }
    glyph =
        frontend_native_font +
        (u16)(tile_index - 1u) * FRONTEND_NATIVE_FONT_HEIGHT;
    for (row = 0; row < 7; row++) occupied |= glyph[row];
    while (occupied >>= 1) natural_width++;
    for (row = 0; row < 7; row++) {
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < 7; column++) {
            u8 source_column = (u8)(
                (u16)column * natural_width / 7u
            );
            s16 target_x = (s16)(x + column);

            if (
                (glyph[row] &
                    (1u << (natural_width - source_column - 1u))) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH +
                    (u16)target_x
                ] = colour;
            }
        }
    }
}
#endif

static void frontend_draw_upgrade_list_text(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
#if TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 1
    const char *measure = text;
    s16 compact_width = 0;
    u8 use_tiny;

    while (*measure != '\0') {
        compact_width = (s16)(
            compact_width + (*measure++ == ' ' ? 3 : 5)
        );
    }
    use_tiny = compact_width > right - x;
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_runtime_font_tile(*text++);

        if (tile_index == 0) {
            x = (s16)(x + (use_tiny ? 2 : 3));
            continue;
        }
        if (use_tiny) {
            if (x + 3 > right) break;
            frontend_blit_tiny_font(
                frame,
                tile_index,
                (s16)(x + 1),
                (s16)(y + 1),
                FRONTEND_NEXT_SHADOW_COLOUR
            );
            frontend_blit_tiny_font(
                frame,
                tile_index,
                x,
                y,
                colour
            );
            x = (s16)(x + 4);
        } else {
            if (x + 4 > right) break;
            frontend_blit_compact_font(
                frame,
                tile_index,
                (s16)(x + 1),
                (s16)(y + 1),
                FRONTEND_NEXT_SHADOW_COLOUR
            );
            frontend_blit_compact_font(
                frame,
                tile_index,
                x,
                y,
                colour
            );
            x = (s16)(x + 5);
        }
    }
#elif TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 2
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_runtime_font_tile(*text++);

        if (tile_index == 0) {
            x = (s16)(x + 4);
            continue;
        }
        if (x + 7 > right) break;
        frontend_blit_wide_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR
        );
        frontend_blit_wide_font(
            frame,
            tile_index,
            x,
            y,
            colour
        );
        x = (s16)(x + 8);
    }
#else
    frontend_draw_runtime_text_clipped_scaled(
        frame,
        text,
        x,
        y,
        right,
        colour,
        FRONTEND_COMPACT_FONT_WIDTH,
        FRONTEND_COMPACT_FONT_SPACE
    );
#endif
}

static void frontend_draw_runtime_text_clipped(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    frontend_draw_runtime_text_clipped_scaled(
        frame,
        text,
        x,
        y,
        right,
        colour,
        FRONTEND_NEXT_FONT_WIDTH,
        FRONTEND_NEXT_FONT_SPACE
    );
}

static s16 frontend_runtime_text_width_scaled(
    const char *text,
    u8 glyph_width,
    u8 space_width
)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        width = (s16)(
            width +
            frontend_runtime_font_advance_scaled(
                frontend_runtime_font_tile(*text++),
                glyph_width,
                space_width
            )
        );
    }
    return width;
}

static s16 frontend_runtime_text_width(const char *text)
{
    return frontend_runtime_text_width_scaled(
        text,
        FRONTEND_NEXT_FONT_WIDTH,
        FRONTEND_NEXT_FONT_SPACE
    );
}

static void frontend_draw_runtime_text_centered(
    u8 *frame,
    const char *text,
    s16 center_x,
    s16 y,
    u8 colour
)
{
    frontend_draw_runtime_text_clipped(
        frame,
        text,
        (s16)(center_x - frontend_runtime_text_width(text) / 2),
        y,
        SCREEN_WIDTH,
        colour
    );
}

static u8 frontend_pregame_font_tile(char character)
{
    u8 index;

    if (character == ' ') return 0;
    for (
        index = 0;
        index < sizeof(frontend_pregame_font_characters) - 1u;
        index++
    ) {
        if (frontend_pregame_font_characters[index] == character) {
            return (u8)(index + 1u);
        }
    }
    for (
        index = 0;
        index < sizeof(frontend_pregame_font_characters) - 1u;
        index++
    ) {
        if (frontend_pregame_font_characters[index] == '?') {
            return (u8)(index + 1u);
        }
    }
    return 0;
}

static u8 frontend_pregame_font_row(u8 tile_index, u8 row)
{
    return frontend_pregame_font[
        (u16)(tile_index - 1u) * FRONTEND_PREGAME_FONT_HEIGHT + row
    ];
}

static u8 frontend_pregame_font_width(u8 tile_index)
{
    u8 occupied = 0;
    u8 row;
    u8 width = 1;

    if (
        tile_index == 0 ||
        tile_index > FRONTEND_PREGAME_FONT_GLYPH_COUNT
    ) {
        return 0;
    }
    for (row = 0; row < FRONTEND_PREGAME_FONT_HEIGHT; row++) {
        occupied |= frontend_pregame_font_row(tile_index, row);
    }
    while (occupied >>= 1) width++;
    return width;
}

static u8 frontend_pregame_font_advance(u8 tile_index)
{
    u8 width;

    if (tile_index == 0) return FRONTEND_PREGAME_FONT_SPACE;
    width = (u8)(frontend_pregame_font_width(tile_index) + 2u);
    return width > FRONTEND_PREGAME_FONT_MAX_ADVANCE ?
        FRONTEND_PREGAME_FONT_MAX_ADVANCE :
        width;
}

static void frontend_blit_pregame_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    u8 width = frontend_pregame_font_width(tile_index);
    u8 row;

    if (width == 0) return;
    for (row = 0; row < FRONTEND_PREGAME_FONT_HEIGHT; row++) {
        u8 value = frontend_pregame_font_row(tile_index, row);
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < width; column++) {
            s16 target_x = (s16)(x + column);

            if (
                (value & (1u << (width - column - 1u))) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x
                ] = colour;
            }
        }
    }
}

static s16 frontend_pregame_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        width = (s16)(
            width +
            frontend_pregame_font_advance(
                frontend_pregame_font_tile(*text++)
            )
        );
    }
    return width;
}

static void frontend_draw_pregame_text_clipped(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_pregame_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_pregame_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR
        );
        frontend_blit_pregame_font(
            frame,
            tile_index,
            x,
            y,
            colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_draw_pregame_text_centered(
    u8 *frame,
    const char *text,
    s16 center_x,
    s16 y,
    u8 colour
)
{
    frontend_draw_pregame_text_clipped(
        frame,
        text,
        (s16)(center_x - frontend_pregame_text_width(text) / 2),
        y,
        SCREEN_WIDTH,
        colour
    );
}

/*
 * Game Menu pages use the same authored mixed-case face as the pre-game
 * pages, with one pixel less tracking.  This keeps PC casing while fitting
 * the 108-pixel Next Level panel without truncating "Exit to Game Menu".
 */
static u8 frontend_menu_font_advance(u8 tile_index)
{
    u8 width;

    if (tile_index == 0) return FRONTEND_MENU_FONT_SPACE;
    width = (u8)(frontend_pregame_font_width(tile_index) + 1u);
    return width > FRONTEND_MENU_FONT_MAX_ADVANCE ?
        FRONTEND_MENU_FONT_MAX_ADVANCE :
        width;
}

static s16 frontend_menu_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        width = (s16)(
            width +
            frontend_menu_font_advance(
                frontend_pregame_font_tile(*text++)
            )
        );
    }
    return width;
}

static void frontend_draw_menu_text_clipped(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_menu_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_pregame_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR
        );
        frontend_blit_pregame_font(
            frame,
            tile_index,
            x,
            y,
            colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_draw_menu_text_centered(
    u8 *frame,
    const char *text,
    s16 center_x,
    s16 y,
    u8 colour
)
{
    frontend_draw_menu_text_clipped(
        frame,
        text,
        (s16)(center_x - frontend_menu_text_width(text) / 2),
        y,
        SCREEN_WIDTH,
        colour
    );
}

/*
 * A four-column mixed-case derivative is reserved for source help/dialog
 * prose.  It samples the authored 6x8 rows at runtime, so no independent
 * alphabet can drift from the main face and lowercase counters survive.
 */
static u8 frontend_small_mixed_font_width(u8 tile_index)
{
    u8 width = frontend_pregame_font_width(tile_index);

    return width > FRONTEND_SMALL_MIXED_FONT_WIDTH ?
        FRONTEND_SMALL_MIXED_FONT_WIDTH :
        width;
}

static u8 frontend_small_mixed_font_advance(u8 tile_index)
{
    if (tile_index == 0) return FRONTEND_SMALL_MIXED_FONT_SPACE;
    return (u8)(frontend_small_mixed_font_width(tile_index) + 1u);
}

static void frontend_blit_small_mixed_font(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y,
    u8 colour
)
{
    u8 source_width = frontend_pregame_font_width(tile_index);
    u8 target_width = frontend_small_mixed_font_width(tile_index);
    u8 row;

    if (source_width == 0 || target_width == 0) return;
    for (row = 0; row < FRONTEND_PREGAME_FONT_HEIGHT; row++) {
        u8 value = frontend_pregame_font_row(tile_index, row);
        u8 column;
        s16 target_y = (s16)(y + row);

        if (target_y < 0 || target_y >= SCREEN_HEIGHT) continue;
        for (column = 0; column < target_width; column++) {
            u8 source_column =
                target_width == 1 ?
                    0 :
                    (u8)(
                        (u16)column * (source_width - 1u) /
                        (target_width - 1u)
                    );
            s16 target_x = (s16)(x + column);

            if (
                (
                    value &
                    (1u << (source_width - source_column - 1u))
                ) != 0 &&
                target_x >= 0 &&
                target_x < SCREEN_WIDTH
            ) {
                frame[
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x
                ] = colour;
            }
        }
    }
}

static s16 frontend_small_mixed_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        width = (s16)(
            width +
            frontend_small_mixed_font_advance(
                frontend_pregame_font_tile(*text++)
            )
        );
    }
    return width;
}

static void frontend_draw_small_mixed_text_clipped(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    while (*text != '\0' && x < right) {
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_small_mixed_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            FRONTEND_NEXT_SHADOW_COLOUR
        );
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            x,
            y,
            colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_draw_small_mixed_text_wrapped(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 line_height,
    u8 max_lines,
    u8 colour
)
{
    char line[67];
    u8 line_length = 0;
    u8 lines = 0;

    if (text == 0 || max_lines == 0) return;
    while (*text != '\0' && lines < max_lines) {
        const char *word;
        u8 word_length = 0;
        s16 candidate_width;

        while (*text == ' ') text++;
        if (*text == '\0') break;
        word = text;
        while (
            text[word_length] != '\0' &&
            text[word_length] != ' ' &&
            word_length < sizeof(line) - 1u
        ) {
            word_length++;
        }
        if (word_length == 0) break;
        if (
            line_length + (line_length != 0 ? 1u : 0u) +
                word_length >= sizeof(line)
        ) {
            break;
        }

        if (line_length != 0) {
            line[line_length] = ' ';
            memcpy(
                line + line_length + 1u,
                word,
                word_length
            );
            line[line_length + 1u + word_length] = '\0';
        } else {
            memcpy(line, word, word_length);
            line[word_length] = '\0';
        }
        candidate_width = frontend_small_mixed_text_width(line);
        if (
            candidate_width > right - x &&
            line_length != 0
        ) {
            line[line_length] = '\0';
            frontend_draw_small_mixed_text_clipped(
                frame,
                line,
                x,
                (s16)(y + lines * line_height),
                right,
                colour
            );
            lines++;
            line_length = 0;
            if (lines >= max_lines) break;
            continue;
        }
        line_length = (u8)(
            line_length +
            (line_length != 0 ? 1u : 0u) +
            word_length
        );
        text += word_length;
    }
    if (line_length != 0 && lines < max_lines) {
        line[line_length] = '\0';
        frontend_draw_small_mixed_text_clipped(
            frame,
            line,
            x,
            (s16)(y + lines * line_height),
            right,
            colour
        );
    }
}

static void frontend_fill_rect(
    u8 *frame,
    u16 x,
    u16 y,
    u16 width,
    u16 height,
    u8 colour
)
{
    u16 row;

    for (row = 0; row < height; row++) {
        memset(
            frame + (u32)(y + row) * SCREEN_WIDTH + x,
            colour,
            width
        );
    }
}

static void frontend_restore_frame_rect(
    u8 frame_index,
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    const u8 *chrome =
        frontend_frame_address(frame_index);
    u16 row;

    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if (x + width > SCREEN_WIDTH) width = (u16)(SCREEN_WIDTH - x);
    if (y + height > SCREEN_HEIGHT) height = (u16)(SCREEN_HEIGHT - y);
    for (row = 0; row < height; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(y + row) * SCREEN_WIDTH + x,
            chrome + (u32)(y + row) * SCREEN_WIDTH + x,
            width
        );
    }
}

static void frontend_restore_menu_chrome_rect(
    u16 x,
    u16 y,
    u16 width,
    u16 height
)
{
    frontend_restore_frame_rect(
        FRONTEND_FRAME_MENU_CHROME,
        x,
        y,
        width,
        height
    );
}

/*
 * Assemble large static pages in bounded scanline groups.  A Mode-4 row is
 * exactly 60 aligned words, so the hot copy loop remains sequential without
 * issuing a single 38.4 KiB Game Pak burst that can delay audio/VBlank work.
 */
static __attribute__((noinline, noclone)) void
frontend_copy_frame_rows(
    u8 frame_index,
    u8 first_row,
    u8 row_count
)
{
    const u8 *source;
    u8 *destination;
    u8 row;

    if (
        frame_index >= FRONTEND_FRAME_COUNT ||
        first_row >= SCREEN_HEIGHT
    ) {
        return;
    }
    if (first_row + row_count > SCREEN_HEIGHT) {
        row_count = (u8)(SCREEN_HEIGHT - first_row);
    }
    source =
        frontend_frame_address(frame_index) +
        (u32)first_row * SCREEN_WIDTH;
    destination =
        frontend_frame_scratch +
        (u32)first_row * SCREEN_WIDTH;
    for (row = 0; row < row_count; row++) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)destination;
        u8 word;

        for (word = 0; word < SCREEN_WIDTH / 4u; word++) {
            destination_words[word] = source_words[word];
        }
        source += SCREEN_WIDTH;
        destination += SCREEN_WIDTH;
    }
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_copy_static_menu_panel_rows(
    u8 panel_index,
    u8 first_row,
    u8 row_count
)
{
    const u8 *source;
    u8 *destination;
    u8 row;

    if (
        panel_index >= FRONTEND_STATIC_MENU_PANEL_COUNT ||
        first_row >= FRONTEND_STATIC_MENU_PANEL_HEIGHT
    ) {
        return 0;
    }
    if (
        first_row + row_count >
        FRONTEND_STATIC_MENU_PANEL_HEIGHT
    ) {
        row_count = (u8)(
            FRONTEND_STATIC_MENU_PANEL_HEIGHT - first_row
        );
    }
    source =
        frontend_static_menu_panels +
        (u32)panel_index * FRONTEND_STATIC_MENU_PANEL_BYTES +
        (u32)first_row * FRONTEND_STATIC_MENU_PANEL_WIDTH;
    destination =
        frontend_frame_scratch +
        (
            (u32)FRONTEND_STATIC_MENU_PANEL_Y + first_row
        ) * SCREEN_WIDTH +
        FRONTEND_STATIC_MENU_PANEL_X;
    for (row = 0; row < row_count; row++) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)destination;
        u8 word;

        for (
            word = 0;
            word < FRONTEND_STATIC_MENU_PANEL_WIDTH / 4u;
            word++
        ) {
            destination_words[word] = source_words[word];
        }
        source += FRONTEND_STATIC_MENU_PANEL_WIDTH;
        destination += SCREEN_WIDTH;
    }
    return 1;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_copy_static_menu_panel(u8 panel_index)
{
    return frontend_copy_static_menu_panel_rows(
        panel_index,
        0,
        FRONTEND_STATIC_MENU_PANEL_HEIGHT
    );
}
