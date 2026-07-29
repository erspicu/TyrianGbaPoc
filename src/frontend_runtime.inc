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
    FRONTEND_STATS_SOURCE_FONT_TABLE = 2,
    /*
     * The GBA summary deliberately uses TINY_FONT so the 264-pixel PC
     * battle text fits the 240-pixel crop.  Its dominant source intensity
     * is 7, while JE_endLevelAni's original SMALL_FONT_SHAPES intensity is
     * 13.  Apply only that six-step intensity normalization; positioning,
     * hue 15 and JE_outTextGlow's -4 settled brightness stay source exact.
     */
    FRONTEND_STATS_FONT_BRIGHTNESS_BIAS = 6,
    FRONTEND_DEMO_COUNT = 5,
    FRONTEND_DEMO_IDLE_VBLANKS = 1800,
    FRONTEND_NEXT_PANEL_X = 126,
    FRONTEND_NEXT_PANEL_Y = 29,
    FRONTEND_NEXT_PANEL_WIDTH = 108,
    FRONTEND_NEXT_PANEL_HEIGHT = 116,
    FRONTEND_NEXT_CHOICE_Y = 32,
    FRONTEND_NEXT_CHOICE_STEP = 14,
    FRONTEND_NEXT_EXIT_Y = 119,
    FRONTEND_NEXT_ITEM_HEIGHT = 10,
    FRONTEND_NEXT_BACKGROUND_COLOUR = 228,
    FRONTEND_NEXT_TEXT_COLOUR = 244,
    FRONTEND_NEXT_SELECTED_COLOUR = 246,
    FRONTEND_NEXT_SHADOW_COLOUR = 240,
    FRONTEND_NEXT_FONT_WIDTH = 6,
    FRONTEND_NEXT_FONT_SPACE = 3,
    FRONTEND_COMPACT_FONT_WIDTH = 5,
    FRONTEND_COMPACT_FONT_SPACE = 3,
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

static u8 frontend_prepare_map(void)
{
    u8 choice;

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
        return 1;
    }
    for (choice = 0; choice < frontend_map.choice_count; choice++) {
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
            return 0;
        }
    }
    frontend_map_ready = 1;
    return 1;
}

static void frontend_load_game_menu_song(void)
{
    audio_load_source_song(
        frontend_map_ready &&
        frontend_map.menu_song_valid &&
        frontend_map.menu_song < JUKEBOX_MUSIC_COUNT ?
            frontend_map.menu_song :
            29
    );
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

static void frontend_draw_runtime_text_wrapped_compact(
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
    char line[43];
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
            text[word_length] != ' '
        ) {
            word_length++;
        }
        if (word_length == 0) break;

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
        candidate_width = frontend_runtime_text_width_scaled(
            line,
            FRONTEND_COMPACT_FONT_WIDTH,
            FRONTEND_COMPACT_FONT_SPACE
        );
        if (
            candidate_width > right - x &&
            line_length != 0
        ) {
            line[line_length] = '\0';
            frontend_draw_runtime_text_clipped_scaled(
                frame,
                line,
                x,
                (s16)(y + lines * line_height),
                right,
                colour,
                FRONTEND_COMPACT_FONT_WIDTH,
                FRONTEND_COMPACT_FONT_SPACE
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
        frontend_draw_runtime_text_clipped_scaled(
            frame,
            line,
            x,
            (s16)(y + lines * line_height),
            right,
            colour,
            FRONTEND_COMPACT_FONT_WIDTH,
            FRONTEND_COMPACT_FONT_SPACE
        );
    }
}

static void frontend_draw_runtime_text(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    u8 colour
)
{
    frontend_draw_runtime_text_clipped(
        frame,
        text,
        x,
        y,
        FRONTEND_NEXT_PANEL_X + FRONTEND_NEXT_PANEL_WIDTH,
        colour
    );
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

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_copy_static_menu_panel(u8 panel_index)
{
    const u8 *source;
    u8 *destination;
    u8 row;

    if (panel_index >= FRONTEND_STATIC_MENU_PANEL_COUNT) return 0;
    source =
        frontend_static_menu_panels +
        (u32)panel_index * FRONTEND_STATIC_MENU_PANEL_BYTES;
    destination =
        frontend_frame_scratch +
        (u32)FRONTEND_STATIC_MENU_PANEL_Y * SCREEN_WIDTH +
        FRONTEND_STATIC_MENU_PANEL_X;
    for (row = 0; row < FRONTEND_STATIC_MENU_PANEL_HEIGHT; row++) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words =
            (u32 *)(void *)destination;
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

/*
 * Sparse choice patches run only on Quit selection changes.  Keep this
 * comparatively large parser in ROM so the gameplay hot-path retains the
 * project's 6 KiB IWRAM safety margin.
 */
static __attribute__((noinline, noclone)) u8
frontend_apply_static_sparse_overlay(
    const u8 *source,
    u32 total_bytes,
    u16 expected_run_count,
    u16 expected_version,
    u8 magic_kind
)
{
    u32 consumed = 8;
    u16 run_count;
    u16 run;

    if (
        source[0] != 'O' ||
        source[1] != 'T' ||
        source[2] != 'Q' ||
        source[3] != magic_kind ||
        (u16)(source[4] | (source[5] << 8)) !=
            expected_version
    ) {
        return 0;
    }
    run_count = (u16)(source[6] | (source[7] << 8));
    if (run_count != expected_run_count) {
        return 0;
    }
    source += 8;
    for (run = 0; run < run_count; run++) {
        u16 offset;
        u16 length;
        u16 padded_length;
        u8 *destination;

        if (
            consumed + 4u >
            total_bytes
        ) {
            return 0;
        }
        offset = (u16)(source[0] | (source[1] << 8));
        length = (u16)(source[2] | (source[3] << 8));
        source += 4;
        consumed += 4;
        padded_length = (u16)((length + 3u) & ~3u);
        if (
            length == 0 ||
            (u32)offset + length > FRONTEND_FRAME_BYTES ||
            consumed + padded_length >
                total_bytes
        ) {
            return 0;
        }
        destination = frontend_frame_scratch + offset;
        if ((offset & 3u) == 0) {
            const u32 *source_words =
                (const u32 *)(const void *)source;
            u32 *destination_words =
                (u32 *)(void *)destination;
            u16 word_count = (u16)(length >> 2);
            u16 tail_count = (u16)(length & 3u);
            u16 word;
            const u8 *tail_source;

            for (word = 0; word < word_count; word++) {
                destination_words[word] = source_words[word];
            }
            destination += (u16)(word_count << 2);
            tail_source = source + (u16)(word_count << 2);
            while (tail_count-- != 0) {
                *destination++ = *tail_source++;
            }
        } else {
            u16 byte;

            for (byte = 0; byte < length; byte++) {
                destination[byte] = source[byte];
            }
        }
        source += padded_length;
        consumed += padded_length;
    }
    return consumed == total_bytes;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_apply_static_quit_overlay(void)
{
    const u8 *source =
        frontend_static_quit_overlay +
        FRONTEND_STATIC_QUIT_OVERLAY_HEADER_BYTES;
    u8 row;

    if (
        frontend_static_quit_overlay[0] != 'O' ||
        frontend_static_quit_overlay[1] != 'T' ||
        frontend_static_quit_overlay[2] != 'Q' ||
        frontend_static_quit_overlay[3] != 'F' ||
        (u16)(
            frontend_static_quit_overlay[4] |
            (frontend_static_quit_overlay[5] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_VERSION ||
        (u16)(
            frontend_static_quit_overlay[6] |
            (frontend_static_quit_overlay[7] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_X ||
        (u16)(
            frontend_static_quit_overlay[8] |
            (frontend_static_quit_overlay[9] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_Y ||
        (u16)(
            frontend_static_quit_overlay[10] |
            (frontend_static_quit_overlay[11] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_WIDTH ||
        (u16)(
            frontend_static_quit_overlay[12] |
            (frontend_static_quit_overlay[13] << 8)
        ) != FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT
    ) {
        return 0;
    }
    for (
        row = 0;
        row < FRONTEND_STATIC_QUIT_OVERLAY_HEIGHT;
        row++
    ) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)(
            frontend_frame_scratch +
            (u32)(FRONTEND_STATIC_QUIT_OVERLAY_Y + row) *
                SCREEN_WIDTH +
            FRONTEND_STATIC_QUIT_OVERLAY_X
        );
        u8 word;

        for (
            word = 0;
            word < FRONTEND_STATIC_QUIT_OVERLAY_WIDTH / 4u;
            word++
        ) {
            u32 pixel = source_words[word];
            u32 inverse = ~pixel;
            u32 transparent = (
                (inverse - 0x01010101u) &
                ~inverse &
                0x80808080u
            );

            if (transparent == 0) {
                destination_words[word] = pixel;
            } else {
                const u8 *source_pixels =
                    (const u8 *)(const void *)&pixel;
                u8 *destination_pixels =
                    (u8 *)(void *)&destination_words[word];
                u8 index;

                for (index = 0; index < 4; index++) {
                    if (source_pixels[index] != 0xFF) {
                        destination_pixels[index] =
                            source_pixels[index];
                    }
                }
            }
        }
        source += FRONTEND_STATIC_QUIT_OVERLAY_WIDTH;
    }
    return 1;
}

static u8 frontend_apply_static_quit_choice(void)
{
    u8 variant = frontend_quit_yes ? 0 : 1;

    return frontend_apply_static_sparse_overlay(
        frontend_static_quit_choices +
            (u32)variant *
                FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES,
        FRONTEND_STATIC_QUIT_CHOICE_VARIANT_BYTES,
        FRONTEND_STATIC_QUIT_CHOICE_RUN_COUNT,
        FRONTEND_STATIC_QUIT_CHOICE_VERSION,
        'C'
    );
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) u8
frontend_apply_static_quit_shade(void)
{
    const u8 *source = frontend_static_quit_shade;
    u16 run_count;
    u16 shaded_pixels = 0;
    u16 run;

    if (
        source[0] != 'O' ||
        source[1] != 'T' ||
        source[2] != 'Q' ||
        source[3] != 'S' ||
        (u16)(source[4] | (source[5] << 8)) !=
            FRONTEND_STATIC_QUIT_SHADE_VERSION
    ) {
        return 0;
    }
    run_count = (u16)(source[6] | (source[7] << 8));
    if (run_count != FRONTEND_STATIC_QUIT_SHADE_RUN_COUNT) {
        return 0;
    }
    source += 8;
    for (run = 0; run < run_count; run++) {
        u16 offset = (u16)(source[0] | (source[1] << 8));
        u16 length = (u16)(source[2] | (source[3] << 8));
        u8 *destination;

        source += 4;
        if (
            length == 0 ||
            (u32)offset + length > FRONTEND_FRAME_BYTES
        ) {
            return 0;
        }
        destination = frontend_frame_scratch + offset;
        shaded_pixels = (u16)(shaded_pixels + length);
        while (length != 0 && ((u32)destination & 3u) != 0) {
            u8 pixel = *destination;
            u8 value = pixel & 0x0Fu;

            value = value > 3 ? (u8)(value - 3u) : 0;
            *destination++ = (u8)((pixel & 0xF0u) | value);
            length--;
        }
        while (length >= 4) {
            u32 *pixels = (u32 *)(void *)destination;
            u32 pixel = *pixels;
            u32 value = pixel & 0x0f0f0f0fu;
            u32 difference =
                (value | 0x10101010u) - 0x03030303u;
            u32 valid = difference & 0x10101010u;
            u32 mask = valid - (valid >> 4);

            *pixels =
                (pixel & 0xf0f0f0f0u) |
                (difference & mask);
            destination += 4;
            length = (u16)(length - 4u);
        }
        while (length-- != 0) {
            u8 pixel = *destination;
            u8 value = pixel & 0x0Fu;

            value = value > 3 ? (u8)(value - 3u) : 0;
            *destination++ = (u8)((pixel & 0xF0u) | value);
        }
    }
    return shaded_pixels == FRONTEND_STATIC_QUIT_SHADE_PIXEL_COUNT;
}

enum {
    FRONTEND_SOURCE_BLIT_NORMAL = 0,
    FRONTEND_SOURCE_BLIT_DARK = 1,
    FRONTEND_SOURCE_BLIT_HUE = 2,
};

static void frontend_u32_string(
    u32 value,
    char destination[11]
)
{
    char reversed[10];
    u8 count = 0;
    u8 index;

    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reversed));
    for (index = 0; index < count; index++) {
        destination[index] = reversed[count - index - 1u];
    }
    destination[count] = '\0';
}

static void frontend_text_with_u32(
    char *destination,
    u8 destination_size,
    const char *prefix,
    u32 value
)
{
    char digits[11];
    u8 position = 0;

    if (destination == 0 || destination_size == 0) return;
    while (
        prefix != 0 &&
        *prefix != '\0' &&
        position + 1u < destination_size
    ) {
        destination[position++] = *prefix++;
    }
    frontend_u32_string(value, digits);
    {
        const char *digit = digits;

        while (*digit != '\0' && position + 1u < destination_size) {
            destination[position++] = *digit++;
        }
    }
    destination[position] = '\0';
}

static u8 frontend_source_colour(
    u8 pixel,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    s16 value;

    if (mode == FRONTEND_SOURCE_BLIT_NORMAL) return pixel;
    value = (s16)(pixel & 0x0fu) + brightness;
    if (mode == FRONTEND_SOURCE_BLIT_DARK) {
        value -= 4;
        hue = pixel >> 4;
    }
    if (value < 0) value = 0;
    if (value > 15) value = 15;
    return (u8)((hue << 4) | (u8)value);
}

static s16 frontend_source_to_screen_x(s16 source_x)
{
    return (s16)(
        (
            (s32)source_x - FRONTEND_MENU_SOURCE_CROP_X
        ) *
        SCREEN_WIDTH /
        FRONTEND_MENU_SOURCE_WIDTH
    );
}

static s16 frontend_source_to_screen_y(s16 source_y)
{
    return (s16)(
        (s32)source_y * SCREEN_HEIGHT /
        OT_PIC_DECODED_HEIGHT
    );
}

static void frontend_source_plot(
    u8 *frame,
    s16 source_x,
    s16 source_y,
    u8 pixel,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    u16 x;
    u16 y;

    if (
        frame == 0 ||
        source_x < (s16)FRONTEND_MENU_SOURCE_CROP_X ||
        source_x >=
            (s16)(
                FRONTEND_MENU_SOURCE_CROP_X +
                FRONTEND_MENU_SOURCE_WIDTH
            ) ||
        source_y < 0 ||
        source_y >= (s16)OT_PIC_DECODED_HEIGHT
    ) {
        return;
    }
    x = (u16)frontend_source_to_screen_x(source_x);
    y = (u16)frontend_source_to_screen_y(source_y);
    frame[(u32)y * SCREEN_WIDTH + x] =
        frontend_source_colour(pixel, mode, hue, brightness);
}

static u8 frontend_blit_shp_source(
    u8 *frame,
    u8 table,
    u16 sprite_index,
    s16 source_x,
    s16 source_y,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    OtShpSprite sprite;
    u32 position = 0;
    u16 x = 0;
    u16 y = 0;

    if (
        !ot_data_shp_sprite_read(table, sprite_index, &sprite) ||
        !sprite.populated ||
        sprite.width == 0 ||
        sprite.height == 0
    ) {
        return 0;
    }
    telemetry_frontend_runtime_shp_decodes++;
    while (position < sprite.encoded.size && y < sprite.height) {
        u8 code = sprite.encoded.data[position++];

        if (code == 255) {
            if (position >= sprite.encoded.size) return 0;
            x = (u16)(x + sprite.encoded.data[position++]);
        } else if (code == 254) {
            x = sprite.width;
        } else if (code == 253) {
            x++;
        } else {
            if (x < sprite.width) {
                frontend_source_plot(
                    frame,
                    (s16)(source_x + (s16)x),
                    (s16)(source_y + (s16)y),
                    code,
                    mode,
                    hue,
                    brightness
                );
            }
            x++;
        }
        while (x >= sprite.width && y < sprite.height) {
            x = (u16)(x - sprite.width);
            y++;
        }
    }
    return y >= sprite.height;
}

static u8 frontend_blit_comp_source(
    u8 *frame,
    u8 shape_table,
    u16 graphic,
    s16 source_x,
    s16 source_y,
    u8 mode,
    u8 hue,
    s8 brightness
)
{
    u8 y;

    if (
        graphic == 0 ||
        !ot_sprite2_frame_decode(
            shape_table,
            graphic,
            1,
            0,
            frontend_sprite2_decode_scratch,
            OT_SPRITE2_FRAME_PIXELS
        )
    ) {
        return 0;
    }
    telemetry_frontend_runtime_sprite2_decodes++;
    /*
     * ot_sprite2_frame_decode() uses the gameplay 32x32 container with the
     * first component at (4,2).  blit_sprite2x2() anchors that component at
     * the requested point, so compensate exactly for the container margin.
     */
    source_x = (s16)(source_x - 4);
    source_y = (s16)(source_y - 2);
    for (y = 0; y < OT_SPRITE2_FRAME_HEIGHT; y++) {
        u8 x;

        for (x = 0; x < OT_SPRITE2_FRAME_WIDTH; x++) {
            u16 stored = frontend_sprite2_decode_scratch[
                (u16)y * OT_SPRITE2_FRAME_WIDTH + x
            ];

            if (stored != 0) {
                frontend_source_plot(
                    frame,
                    (s16)(source_x + x),
                    (s16)(source_y + y),
                    (u8)(stored - 1u),
                    mode,
                    hue,
                    brightness
                );
            }
        }
    }
    return 1;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
frontend_shade_source_rect(
    u8 *frame,
    s16 source_x0,
    s16 source_y0,
    s16 source_x1,
    s16 source_y1,
    u8 amount
)
{
    s16 mapped_x0 = frontend_source_to_screen_x(source_x0);
    s16 mapped_y0 = frontend_source_to_screen_y(source_y0);
    s16 mapped_x1 = frontend_source_to_screen_x(
        (s16)(source_x1 + 1)
    );
    s16 mapped_y1 = frontend_source_to_screen_y(
        (s16)(source_y1 + 1)
    );
    u16 x0;
    u16 y0;
    u16 x1;
    u16 y1;
    u16 y;

    if (mapped_x0 < 0) mapped_x0 = 0;
    if (mapped_y0 < 0) mapped_y0 = 0;
    if (mapped_x1 < 0) mapped_x1 = 0;
    if (mapped_y1 < 0) mapped_y1 = 0;
    x0 = (u16)mapped_x0;
    y0 = (u16)mapped_y0;
    x1 = (u16)mapped_x1;
    y1 = (u16)mapped_y1;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;
    if (
        amount == 3 &&
        (x0 & 3u) == 0 &&
        ((x1 - x0) & 3u) == 0
    ) {
        /*
         * Process four independent palette-value nibbles per ARM word.
         * Setting bit 4 before subtracting prevents cross-byte borrows; the
         * surviving bit 4 then expands into a 0x0f mask.  This reproduces
         * max(value - 3, 0) exactly, without per-pixel branches.
         */
        for (y = y0; y < y1; y++) {
            u32 *pixels = (u32 *)(void *)(
                frame + (u32)y * SCREEN_WIDTH + x0
            );
            u16 word_count = (u16)((x1 - x0) >> 2);
            u16 word;

            for (word = 0; word < word_count; word++) {
                u32 pixel = pixels[word];
                u32 value = pixel & 0x0f0f0f0fu;
                u32 difference =
                    (value | 0x10101010u) - 0x03030303u;
                u32 valid = difference & 0x10101010u;
                u32 mask = valid - (valid >> 4);

                pixels[word] =
                    (pixel & 0xf0f0f0f0u) |
                    (difference & mask);
            }
        }
        return;
    }
    for (y = y0; y < y1; y++) {
        u16 x;

        for (x = x0; x < x1; x++) {
            u8 pixel = frame[(u32)y * SCREEN_WIDTH + x];
            u8 value = pixel & 0x0fu;

            value = value > amount ? (u8)(value - amount) : 0;
            frame[(u32)y * SCREEN_WIDTH + x] =
                (u8)((pixel & 0xf0u) | value);
        }
    }
}

static void frontend_copy_string(
    char *destination,
    u8 destination_size,
    const char *source
)
{
    u8 index = 0;

    if (destination == 0 || destination_size == 0) return;
    while (
        source != 0 &&
        source[index] != '\0' &&
        index + 1u < destination_size
    ) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static u32 frontend_weapon_total_cost(
    u16 base_cost,
    u8 power
)
{
    u32 total = base_cost;
    u32 increment = 0;
    u8 level;

    if (power == 0) power = 1;
    if (power > 11) power = 11;
    for (level = 1; level < power; level++) {
        increment += (u32)base_cost * level;
        total += increment;
    }
    return total;
}

static u32 frontend_upgrade_base_cost(
    u8 category,
    u8 item
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        if (item > 90) return 100;
        return ot_data_hdt_ship_read(item, &ship) ? ship.cost : 0;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        return ot_data_hdt_weapon_port_read(item, &port) ?
            port.cost :
            0;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        return ot_data_hdt_shield_read(item, &shield) ?
            shield.cost :
            0;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        return ot_data_hdt_power_read(item, &power) ?
            power.cost :
            0;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        return ot_data_hdt_option_read(item, &option) ?
            option.cost :
            0;
    }
    default:
        return 0;
    }
}

static u32 frontend_upgrade_total_cost(
    u8 category,
    u8 item,
    u8 power
)
{
    u32 base = frontend_upgrade_base_cost(category, item);

    if (
        category == FRONTEND_UPGRADE_FRONT_WEAPON ||
        category == FRONTEND_UPGRADE_REAR_WEAPON
    ) {
        return frontend_weapon_total_cost((u16)base, power);
    }
    return base;
}

static void frontend_upgrade_item_name(
    u8 category,
    u8 item,
    char *destination,
    u8 destination_size
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        if (item > 90) {
            frontend_text_with_u32(
                destination,
                destination_size,
                "CUSTOM SHIP ",
                item - 90u
            );
        } else if (ot_data_hdt_ship_read(item, &ship)) {
            frontend_copy_string(
                destination,
                destination_size,
                ship.name
            );
        } else {
            frontend_copy_string(
                destination,
                destination_size,
                "UNKNOWN SHIP"
            );
        }
        break;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_weapon_port_read(item, &port) ?
                port.name :
                "UNKNOWN WEAPON"
        );
        break;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_shield_read(item, &shield) ?
                shield.name :
                "UNKNOWN SHIELD"
        );
        break;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_power_read(item, &power) ?
                power.name :
                "UNKNOWN GENERATOR"
        );
        break;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        frontend_copy_string(
            destination,
            destination_size,
            ot_data_hdt_option_read(item, &option) ?
                option.name :
                "UNKNOWN SIDEKICK"
        );
        break;
    }
    default:
        frontend_copy_string(
            destination,
            destination_size,
            "UNKNOWN"
        );
        break;
    }
}

static u16 frontend_upgrade_item_graphic(
    u8 category,
    u8 item
)
{
    switch (category) {
    case FRONTEND_UPGRADE_SHIP: {
        OtShipDefinition ship;

        return ot_data_hdt_ship_read(item, &ship) ?
            ship.shipgraphic :
            0;
    }
    case FRONTEND_UPGRADE_FRONT_WEAPON:
    case FRONTEND_UPGRADE_REAR_WEAPON: {
        OtWeaponPortDefinition port;

        return ot_data_hdt_weapon_port_read(item, &port) ?
            port.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_SHIELD: {
        OtShieldDefinition shield;

        return ot_data_hdt_shield_read(item, &shield) ?
            shield.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_GENERATOR: {
        OtPowerDefinition power;

        return ot_data_hdt_power_read(item, &power) ?
            power.itemgraphic :
            0;
    }
    case FRONTEND_UPGRADE_LEFT_SIDEKICK:
    case FRONTEND_UPGRADE_RIGHT_SIDEKICK: {
        OtOptionDefinition option;

        return ot_data_hdt_option_read(item, &option) ?
            option.itemgraphic :
            0;
    }
    default:
        return 0;
    }
}

static void frontend_draw_item_source(
    u8 *frame,
    u8 category,
    u8 item,
    s16 source_x,
    s16 source_y
)
{
    u16 graphic;

    if (item == 0) return;
    graphic = frontend_upgrade_item_graphic(category, item);
    if (graphic == 0) return;
    (void)frontend_blit_comp_source(
        frame,
        category == FRONTEND_UPGRADE_SHIP ?
            OT_COMP_SHAPE_TABLE_OPTIONS_SMALL :
            OT_COMP_SHAPE_TABLE_SHOP,
        graphic,
        source_x,
        source_y,
        FRONTEND_SOURCE_BLIT_NORMAL,
        0,
        0
    );
}

static void frontend_draw_segment_bar_source(
    u8 *frame,
    s16 source_x,
    s16 source_y,
    u8 resolution,
    u8 colour,
    u8 amount,
    u8 source_width,
    u8 source_height
)
{
    u8 segment_count;
    u8 segment;

    if (resolution == 0) return;
    segment_count = (u8)(amount / resolution);
    for (segment = 0; segment < segment_count; segment++) {
        u8 y;

        for (y = 0; y < source_height; y++) {
            u8 x;

            for (x = 0; x < source_width; x++) {
                u8 pixel = (u8)(colour + 12u);

                if (y == 0) pixel = (u8)(colour + 13u);
                if (y + 1u == source_height) {
                    pixel = (u8)(colour + 11u);
                }
                if (x == 0 && y == 0) {
                    pixel = (u8)(colour + 15u);
                }
                frontend_source_plot(
                    frame,
                    (s16)(
                        source_x +
                        segment * (source_width + 2u) +
                        x
                    ),
                    (s16)(source_y + y),
                    pixel,
                    FRONTEND_SOURCE_BLIT_NORMAL,
                    0,
                    0
                );
            }
        }
    }
}

static void frontend_draw_ship_illustration(u8 *frame)
{
    static const s16 weapon_sprite[43] = {
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,
        9, 10, 11, 21, 5, 13, -1, 14, 15, 0,
        14, 9, 8, 2, 15, 0, 13, 0, 8, 8,
        11, 1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 2, 1
    };
    static const s8 front_xy[43] = {
        -1, 4, 9, 3, 8, 2, 5, 10, 1, -1,
        -1, -1, -1, 7, 8, -1, -1, 0, -1, 4,
        0, -1, -1, 3, -1, 4, -1, 4, -1, -1,
        -1, 9, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 3, 9
    };
    static const s16 front_x[12] = {
        59, 66, 66, 54, 61, 51, 58, 51, 61, 52, 53, 58
    };
    static const s16 front_y[12] = {
        38, 53, 41, 36, 48, 35, 41, 35, 53, 41, 39, 31
    };
    static const s8 rear_xy[43] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,
        1, 2, 3, -1, 4, 5, -1, -1, 6, -1,
        -1, 1, 0, -1, 6, -1, 5, -1, 0, 0,
        3, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, -1, -1
    };
    static const s16 rear_x[7] = {41, 27, 49, 43, 51, 39, 41};
    static const s16 rear_y[7] = {92, 92, 113, 102, 97, 96, 76};
    OtShipDefinition ship;
    OtPowerDefinition generator;
    OtShieldDefinition shield;
    u8 front = frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id;
    u8 rear = frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id;
    char cash[11];

    if (
        ot_data_hdt_ship_read(frontend_player_items.ship, &ship) &&
        ship.bigshipgraphic > 0
    ) {
        static const s16 ship_x[6] = {31, 0, 0, 0, 35, 31};
        static const s16 ship_y[6] = {36, 0, 0, 0, 33, 35};
        s16 sprite = (s16)ship.bigshipgraphic - 1;

        if (sprite >= 27 && sprite <= 32) {
            (void)frontend_blit_shp_source(
                frame,
                5,
                (u16)sprite,
                ship_x[sprite - 27],
                ship_y[sprite - 27],
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
    }
    if (
        frontend_player_items.generator > 0 &&
        frontend_player_items.generator < 7 &&
        ot_data_hdt_power_read(
            frontend_player_items.generator,
            &generator
        )
    ) {
        static const s16 generator_x[5] = {62, 64, 67, 66, 63};
        static const s16 generator_y[5] = {84, 85, 86, 84, 97};
        s16 sprite =
            frontend_player_items.generator == 1 ?
                frontend_player_items.generator + 15 :
                frontend_player_items.generator + 14;

        if (sprite >= 16 && sprite <= 20) {
            (void)frontend_blit_shp_source(
                frame,
                6,
                (u16)sprite,
                generator_x[sprite - 16],
                generator_y[sprite - 16],
                FRONTEND_SOURCE_BLIT_NORMAL,
                0,
                0
            );
        }
    }
    if (
        front < 43 &&
        weapon_sprite[front] >= 0 &&
        front_xy[front] >= 0
    ) {
        (void)frontend_blit_shp_source(
            frame,
            6,
            (u16)weapon_sprite[front],
            front_x[(u8)front_xy[front]],
            front_y[(u8)front_xy[front]],
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
    }
    if (
        rear < 43 &&
        weapon_sprite[rear] >= 0 &&
        rear_xy[rear] >= 0
    ) {
        (void)frontend_blit_shp_source(
            frame,
            6,
            (u16)weapon_sprite[rear],
            rear_x[(u8)rear_xy[rear]],
            rear_y[(u8)rear_xy[rear]],
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
    }
    frontend_draw_item_source(
        frame,
        FRONTEND_UPGRADE_LEFT_SIDEKICK,
        frontend_player_items.sidekick[0],
        3,
        84
    );
    frontend_draw_item_source(
        frame,
        FRONTEND_UPGRADE_RIGHT_SIDEKICK,
        frontend_player_items.sidekick[1],
        129,
        84
    );
    if (
        ot_data_hdt_shield_read(
            frontend_player_items.shield,
            &shield
        )
    ) {
        (void)frontend_blit_shp_source(
            frame,
            5,
            26,
            28,
            23,
            FRONTEND_SOURCE_BLIT_HUE,
            15,
            (s8)shield.max_power - 10
        );
    }

    frontend_u32_string(player_cash, cash);
    frontend_draw_runtime_text_clipped(
        frame,
        cash,
        49,
        138,
        116,
        0xfbu
    );
    if (ot_data_hdt_ship_read(frontend_player_items.ship, &ship)) {
        frontend_draw_segment_bar_source(
            frame, 42, 152, 3, 14, ship.damage, 2, 13
        );
    }
    if (
        ot_data_hdt_shield_read(
            frontend_player_items.shield,
            &shield
        )
    ) {
        frontend_draw_segment_bar_source(
            frame,
            104,
            152,
            2,
            14,
            (u8)(shield.max_power * 2u),
            2,
            13
        );
    }
}

static const char *frontend_game_menu_label(u8 index)
{
    static const char *const fallback[7] = {
        "GAME MENU",
        "DATA CUBES",
        "SHIP SPECS",
        "UPGRADE SHIP",
        "OPTIONS",
        "PLAY NEXT LEVEL",
        "QUIT GAME",
    };

    if (
        frontend_text_ready &&
        index < 7 &&
        frontend_text.full_game_menu[index][0] != '\0'
    ) {
        return frontend_text.full_game_menu[index];
    }
    return fallback[index < 7 ? index : 0];
}

static const char *frontend_pre_game_title_label(u8 selection)
{
    static const char *const fallback[3] = {
        "START NEW GAME",
        "DEMO",
        "JUKEBOX",
    };
    static const u8 source_index[3] = {0, 5, 0xff};

    if (
        selection < 3 &&
        source_index[selection] != 0xff &&
        frontend_text_ready &&
        frontend_text.title_menu[source_index[selection]][0] != '\0'
    ) {
        return frontend_text.title_menu[source_index[selection]];
    }
    return fallback[selection < 3 ? selection : 0];
}

static const char *frontend_pre_game_header(u8 state)
{
    if (state == STATE_PLAY_MODE_MENU) return "PLAY MODE";
    if (
        state == STATE_EPISODE_MENU &&
        frontend_text_ready &&
        frontend_text.episode_name[0][0] != '\0'
    ) {
        return frontend_text.episode_name[0];
    }
    if (
        state == STATE_DIFFICULTY_MENU &&
        frontend_text_ready &&
        frontend_text.difficulty_name[0][0] != '\0'
    ) {
        return frontend_text.difficulty_name[0];
    }
    return state == STATE_EPISODE_MENU ?
        "SELECT AN EPISODE" :
        "DIFFICULTY LEVEL";
}

static u8 frontend_pre_game_item_count(u8 state)
{
    switch (state) {
    case STATE_TITLE:
    case STATE_DIFFICULTY_MENU:
        return 3;
    case STATE_PLAY_MODE_MENU:
        return 2;
    case STATE_EPISODE_MENU:
        return 4;
    default:
        return 0;
    }
}

static const char *frontend_pre_game_item_label(
    u8 state,
    u8 selection
)
{
    static const char *const play_mode[2] = {
        "FULL GAME",
        "ARCADE",
    };
    static const char *const episode[4] = {
        "EPISODE 1: ESCAPE",
        "EPISODE 2: TREACHERY",
        "EPISODE 3: MISSION: SUICIDE",
        "EPISODE 4: AN END TO FATE",
    };
    static const char *const difficulty[3] = {
        "EASY",
        "NORMAL",
        "HARD",
    };

    if (state == STATE_TITLE) {
        return frontend_pre_game_title_label(selection);
    }
    if (state == STATE_PLAY_MODE_MENU) {
        return play_mode[selection < 2 ? selection : 0];
    }
    if (state == STATE_EPISODE_MENU) {
        if (
            selection < 4 &&
            frontend_text_ready &&
            frontend_text.episode_name[selection + 1u][0] != '\0'
        ) {
            return frontend_text.episode_name[selection + 1u];
        }
        return episode[selection < 4 ? selection : 0];
    }
    if (
        state == STATE_DIFFICULTY_MENU &&
        selection < 3 &&
        frontend_text_ready &&
        frontend_text.difficulty_name[selection + 1u][0] != '\0'
    ) {
        return frontend_text.difficulty_name[selection + 1u];
    }
    return difficulty[selection < 3 ? selection : 0];
}

static s16 frontend_pre_game_item_y(u8 state, u8 selection)
{
    switch (state) {
    case STATE_TITLE:
        return (s16)(86 + selection * 10u);
    case STATE_EPISODE_MENU:
        return (s16)(40 + selection * 24u);
    case STATE_PLAY_MODE_MENU:
    case STATE_DIFFICULTY_MENU:
    default:
        return (s16)(43 + selection * 19u);
    }
}

static void frontend_pre_game_item_rect(
    u8 state,
    u8 selection,
    u16 *x,
    u16 *y,
    u16 *width,
    u16 *height
)
{
    *x = state == STATE_EPISODE_MENU ? 8 : 64;
    *y = (u16)frontend_pre_game_item_y(state, selection);
    *width = state == STATE_EPISODE_MENU ? 184 : 112;
    *height = 9;
}

static void frontend_draw_pre_game_item(
    u8 state,
    u8 selection,
    u8 active_selection
)
{
    const char *label =
        frontend_pre_game_item_label(state, selection);
    s16 y = frontend_pre_game_item_y(state, selection);
    u8 colour = selection == active_selection ? 0xfeu : 0xfau;

    if (state == STATE_EPISODE_MENU) {
        frontend_draw_runtime_text_clipped(
            frontend_frame_scratch,
            label,
            15,
            y,
            190,
            colour
        );
    } else {
        frontend_draw_runtime_text_centered(
            frontend_frame_scratch,
            label,
            120,
            y,
            colour
        );
    }
}

static void frontend_render_pre_game_menu(void)
{
    u8 base_frame =
        game_state == STATE_TITLE ?
            FRONTEND_FRAME_TITLE_CHROME :
            FRONTEND_FRAME_SELECT_CHROME;
    u8 static_base;
    u8 item_count = frontend_pre_game_item_count(game_state);
    u8 selection;

    switch (game_state) {
    case STATE_TITLE:
        static_base = FRONTEND_STATIC_PRE_GAME_TITLE_BASE;
        break;
    case STATE_PLAY_MODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE;
        break;
    case STATE_EPISODE_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_EPISODE_BASE;
        break;
    case STATE_DIFFICULTY_MENU:
        static_base = FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE;
        break;
    default:
        static_base = FRONTEND_STATIC_PRE_GAME_FRAME_COUNT;
        break;
    }
    if (
        frontend_selection < item_count &&
        static_base + frontend_selection <
            FRONTEND_STATIC_PRE_GAME_FRAME_COUNT
    ) {
        frontend_present(
            frontend_static_pre_game_frames +
                (u32)(static_base + frontend_selection) *
                    FRONTEND_FRAME_BYTES,
            frontend_palette_address(base_frame)
        );
        return;
    }

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(base_frame),
        FRONTEND_FRAME_BYTES
    );
    if (game_state != STATE_TITLE) {
        frontend_draw_runtime_text_centered(
            frontend_frame_scratch,
            frontend_pre_game_header(game_state),
            120,
            15,
            0xfbu
        );
    }
    for (selection = 0; selection < item_count; selection++) {
        frontend_draw_pre_game_item(
            game_state,
            selection,
            frontend_selection
        );
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(base_frame)
    );
}

static void frontend_render_pre_game_selection(u8 old_selection)
{
    u8 base_frame =
        game_state == STATE_TITLE ?
            FRONTEND_FRAME_TITLE_CHROME :
            FRONTEND_FRAME_SELECT_CHROME;
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        u16 x;
        u16 y;
        u16 width;
        u16 height;

        frontend_pre_game_item_rect(
            game_state,
            selection[index],
            &x,
            &y,
            &width,
            &height
        );
        frontend_restore_frame_rect(
            base_frame,
            x,
            y,
            width,
            height
        );
        frontend_draw_pre_game_item(
            game_state,
            selection[index],
            frontend_selection
        );
        frontend_dirty_add(x, y, width, height);
    }
    frontend_dirty_present();
}

static s16 frontend_game_menu_item_y(u8 selection)
{
    return frontend_source_to_screen_y(
        (s16)(
            38 +
            selection * 16 +
            (selection == 5 ? 16 : 0)
        )
    );
}

static void frontend_draw_game_menu_item(
    u8 selection,
    u8 active_selection
)
{
    u8 disabled = (
        selection == 0 ||
        selection == 1 ||
        selection == 3
    );
    u8 colour;

    if (disabled) {
        colour =
            selection == active_selection ? 0xf8u : 0xf4u;
    } else {
        colour =
            selection == active_selection ? 0xfeu : 0xfau;
    }
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        frontend_game_menu_label((u8)(selection + 1u)),
        125,
        frontend_game_menu_item_y(selection),
        238,
        colour
    );
}

static void frontend_draw_game_menu_text(u8 active_selection)
{
    u8 selection;

    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        frontend_game_menu_label(0),
        178,
        6,
        0xfbu
    );
    for (selection = 0; selection < 6; selection++) {
        frontend_draw_game_menu_item(selection, active_selection);
    }
}

static void frontend_quit_capture_dialog_background(void);

static void frontend_compose_game_menu(u8 selection)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    if (frontend_player_items_valid) {
        frontend_draw_ship_illustration(frontend_frame_scratch);
    }
    if (
        selection >= FRONTEND_STATIC_GAME_MENU_COUNT ||
        !frontend_copy_static_menu_panel(
            (u8)(FRONTEND_STATIC_GAME_MENU_BASE + selection)
        )
    ) {
        frontend_draw_game_menu_text(selection);
    }
}

static void frontend_render_game_menu(void)
{
    frontend_compose_game_menu(frontend_selection);
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = frontend_selection;
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_game_menu_selection(u8 old_selection)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        s16 y = frontend_game_menu_item_y(selection[index]);

        frontend_restore_menu_chrome_rect(120, (u16)y, 120, 9);
        frontend_draw_game_menu_item(
            selection[index],
            frontend_selection
        );
        frontend_dirty_add(120, y, 120, 9);
    }
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = frontend_selection;
    frontend_dirty_present();
}

static const char *frontend_upgrade_menu_text(
    u8 index,
    const char *fallback
)
{
    if (
        frontend_text_ready &&
        index < 9 &&
        frontend_text.upgrade_menu[index][0] != '\0'
    ) {
        return frontend_text.upgrade_menu[index];
    }
    return fallback;
}

static const char *frontend_upgrade_menu_item_label(u8 index)
{
    static const char *const fallback[
        FRONTEND_UPGRADE_MENU_ITEM_COUNT
    ] = {
        "SHIP TYPE",
        "FRONT GUN",
        "REAR GUN",
        "SHIELD",
        "GENERATOR",
        "LEFT SIDEKICK",
        "RIGHT SIDEKICK",
        "DONE"
    };

    return frontend_upgrade_menu_text(
        (u8)(index + 1u),
        fallback[
            index < FRONTEND_UPGRADE_MENU_ITEM_COUNT ? index : 0
        ]
    );
}

static s16 frontend_upgrade_menu_item_y(u8 index)
{
    return (s16)(30 + index * 10u);
}

static void frontend_draw_upgrade_menu_item(u8 index)
{
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        frontend_upgrade_menu_item_label(index),
        125,
        frontend_upgrade_menu_item_y(index),
        238,
        index == frontend_selection ? 0xfeu : 0xfau
    );
}

static void frontend_draw_upgrade_menu_text(void)
{
    u8 index;

    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        frontend_upgrade_menu_text(0, "UPGRADE SHIP"),
        176,
        7,
        0xfbu
    );
    for (
        index = 0;
        index < FRONTEND_UPGRADE_MENU_ITEM_COUNT;
        index++
    ) {
        frontend_draw_upgrade_menu_item(index);
    }
}

static void frontend_upgrade_sort_items(void)
{
    u8 outer;

    for (outer = 0; outer < frontend_upgrade_sub_count; outer++) {
        u8 inner;

        for (
            inner = (u8)(outer + 1u);
            inner < frontend_upgrade_sub_count;
            inner++
        ) {
            u8 left = frontend_upgrade_sub_item[outer];
            u8 right = frontend_upgrade_sub_item[inner];

            if (
                left == 0 ||
                (right != 0 && left > right)
            ) {
                frontend_upgrade_sub_item[outer] = right;
                frontend_upgrade_sub_item[inner] = left;
            }
        }
    }
}

static u8 frontend_upgrade_begin_submenu(u8 category)
{
    const u8 *current_field;
    u8 current;
    u8 group;
    u8 index;
    u8 found = 0;

    if (
        category >= FRONTEND_UPGRADE_CATEGORY_COUNT ||
        !frontend_player_items_valid
    ) {
        return 0;
    }
    current_field = frontend_player_item_field_const(
        &frontend_player_items,
        category
    );
    if (current_field == 0) return 0;
    current = *current_field;
    group = frontend_upgrade_item_group[category];
    frontend_upgrade_sub_count = 0;
    memset(
        frontend_upgrade_sub_item,
        0,
        sizeof(frontend_upgrade_sub_item)
    );
    memset(
        frontend_upgrade_sub_power,
        1,
        sizeof(frontend_upgrade_sub_power)
    );
    if (
        frontend_map.item_inventory_valid &&
        group < OT_EPISODE_ITEM_GROUP_COUNT
    ) {
        u8 count = frontend_map.item_count[group];

        if (count > OT_EPISODE_ITEM_GROUP_CAPACITY) {
            count = OT_EPISODE_ITEM_GROUP_CAPACITY;
        }
        for (index = 0; index < count; index++) {
            frontend_upgrade_sub_item[
                frontend_upgrade_sub_count++
            ] = frontend_map.item_avail[group][index];
        }
    }
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == current) {
            found = 1;
            break;
        }
    }
    if (
        !found &&
        frontend_upgrade_sub_count <
            OT_EPISODE_ITEM_GROUP_CAPACITY + 1u
    ) {
        frontend_upgrade_sub_item[
            frontend_upgrade_sub_count++
        ] = current;
    }
    if (frontend_upgrade_sub_count == 0) {
        frontend_upgrade_sub_item[
            frontend_upgrade_sub_count++
        ] = current;
    }
    frontend_upgrade_sort_items();
    frontend_upgrade_category = category;
    frontend_upgrade_original_items = frontend_player_items;
    frontend_upgrade_original_cash = player_cash;
    frontend_upgrade_trade_cash =
        player_cash +
        frontend_upgrade_total_cost(
            category,
            current,
            (
                category == FRONTEND_UPGRADE_FRONT_WEAPON ?
                    frontend_player_items.weapon[
                        FRONTEND_WEAPON_FRONT
                    ].power :
                    (
                        category == FRONTEND_UPGRADE_REAR_WEAPON ?
                            frontend_player_items.weapon[
                                FRONTEND_WEAPON_REAR
                            ].power :
                            1
                    )
            )
        );
    frontend_selection = 0;
    for (index = 0; index < frontend_upgrade_sub_count; index++) {
        if (frontend_upgrade_sub_item[index] == current) {
            frontend_selection = index;
        }
        if (
            category == FRONTEND_UPGRADE_FRONT_WEAPON ||
            category == FRONTEND_UPGRADE_REAR_WEAPON
        ) {
            frontend_upgrade_sub_power[index] =
                frontend_upgrade_sub_item[index] == current ?
                    frontend_player_items.weapon[
                        category == FRONTEND_UPGRADE_FRONT_WEAPON ?
                            FRONTEND_WEAPON_FRONT :
                            FRONTEND_WEAPON_REAR
                    ].power :
                    1;
        }
    }
    frontend_upgrade_sub_scroll = 0;
    return 1;
}

static void frontend_upgrade_apply_preview(void)
{
    u8 *field;

    if (
        frontend_selection >= frontend_upgrade_sub_count ||
        frontend_upgrade_category >=
            FRONTEND_UPGRADE_CATEGORY_COUNT
    ) {
        return;
    }
    field = frontend_player_item_field(
        &frontend_player_items,
        frontend_upgrade_category
    );
    if (field == 0) return;
    *field = frontend_upgrade_sub_item[frontend_selection];
    frontend_quit_dialog_cache_valid = 0;
    if (
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_FRONT_WEAPON ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON
    ) {
        u8 weapon =
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_FRONT_WEAPON ?
                    FRONTEND_WEAPON_FRONT :
                    FRONTEND_WEAPON_REAR;

        frontend_player_items.weapon[weapon].power =
            frontend_upgrade_sub_power[frontend_selection];
        if (weapon == FRONTEND_WEAPON_REAR) {
            frontend_player_items.weapon_mode = 1;
        }
    }
}

static u32 frontend_upgrade_current_cost(void)
{
    if (frontend_selection >= frontend_upgrade_sub_count) return 0;
    return frontend_upgrade_total_cost(
        frontend_upgrade_category,
        frontend_upgrade_sub_item[frontend_selection],
        frontend_upgrade_sub_power[frontend_selection]
    );
}

static u32 frontend_upgrade_cash_left(void)
{
    u32 cost = frontend_upgrade_current_cost();

    return cost > frontend_upgrade_trade_cash ?
        0 :
        frontend_upgrade_trade_cash - cost;
}

static void frontend_upgrade_cancel_submenu(void)
{
    frontend_player_items = frontend_upgrade_original_items;
    player_cash = frontend_upgrade_original_cash;
    frontend_quit_dialog_cache_valid = 0;
    frontend_enter_state(
        STATE_UPGRADE_MENU,
        frontend_upgrade_category
    );
    audio_effect(SFX_SPRING);
}

static void frontend_upgrade_commit_submenu(void)
{
    player_cash = frontend_upgrade_cash_left();
    frontend_enter_state(
        STATE_UPGRADE_MENU,
        frontend_upgrade_category
    );
    audio_effect(SFX_ITEM);
}

static void frontend_upgrade_move_submenu(s8 direction)
{
    u8 old_selection = frontend_selection;
    u8 old_scroll = frontend_upgrade_sub_scroll;
    u8 item_count = (u8)(frontend_upgrade_sub_count + 1u);
    u8 attempts = 0;

    do {
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
        attempts++;
    } while (
        frontend_selection < frontend_upgrade_sub_count &&
        frontend_upgrade_total_cost(
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection],
            frontend_upgrade_sub_power[frontend_selection]
        ) > frontend_upgrade_trade_cash &&
        attempts < item_count
    );
    frontend_upgrade_apply_preview();
    if (frontend_selection < frontend_upgrade_sub_scroll) {
        frontend_upgrade_sub_scroll = frontend_selection;
    } else if (
        frontend_selection >=
            frontend_upgrade_sub_scroll + 6u
    ) {
        frontend_upgrade_sub_scroll =
            (u8)(frontend_selection - 5u);
    }
    if (frontend_selection != old_selection) {
        audio_effect(SFX_CURSOR);
        frontend_render_upgrade_submenu_update(
            old_selection,
            old_scroll
        );
    }
}

static void frontend_upgrade_power_adjust(s8 direction)
{
    u8 old_selection = frontend_selection;
    u8 old_scroll = frontend_upgrade_sub_scroll;
    u8 *power;
    u8 candidate;

    if (
        frontend_selection >= frontend_upgrade_sub_count ||
        (
            frontend_upgrade_category !=
                FRONTEND_UPGRADE_FRONT_WEAPON &&
            frontend_upgrade_category !=
                FRONTEND_UPGRADE_REAR_WEAPON
        ) ||
        frontend_upgrade_sub_item[frontend_selection] == 0
    ) {
        audio_effect(SFX_CLINK);
        return;
    }
    power = &frontend_upgrade_sub_power[frontend_selection];
    candidate = *power;
    if (direction < 0) {
        if (candidate <= 1) {
            audio_effect(SFX_CLINK);
            return;
        }
        candidate--;
    } else {
        if (candidate >= 11) {
            audio_effect(SFX_CLINK);
            return;
        }
        candidate++;
        if (
            frontend_upgrade_total_cost(
                frontend_upgrade_category,
                frontend_upgrade_sub_item[frontend_selection],
                candidate
            ) > frontend_upgrade_trade_cash
        ) {
            audio_effect(SFX_CLINK);
            return;
        }
    }
    *power = candidate;
    frontend_upgrade_apply_preview();
    audio_effect(SFX_CURSOR);
    frontend_render_upgrade_submenu_update(
        old_selection,
        old_scroll
    );
}

static void frontend_render_upgrade_menu(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_draw_ship_illustration(frontend_frame_scratch);
    if (
        frontend_selection >= FRONTEND_STATIC_UPGRADE_MENU_COUNT ||
        !frontend_copy_static_menu_panel(
            (u8)(
                FRONTEND_STATIC_UPGRADE_MENU_BASE +
                frontend_selection
            )
        )
    ) {
        frontend_draw_upgrade_menu_text();
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_upgrade_menu_selection(u8 old_selection)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    for (index = 0; index < 2; index++) {
        s16 y = frontend_upgrade_menu_item_y(selection[index]);

        frontend_restore_menu_chrome_rect(120, (u16)y, 120, 9);
        frontend_draw_upgrade_menu_item(selection[index]);
        frontend_dirty_add(120, y, 120, 9);
    }
    frontend_dirty_present();
}

static u8 frontend_render_quit_transition(
    u8 old_state,
    u8 new_state
);

static u8 frontend_render_shared_menu_transition(
    u8 old_state,
    u8 new_state
)
{
    if (
        !frontend_mode4_active ||
        frontend_frame_pending
    ) {
        return 0;
    }
    if (
        (
            old_state == STATE_GAME_MENU &&
            new_state == STATE_QUIT_CONFIRM
        ) ||
        (
            old_state == STATE_QUIT_CONFIRM &&
            new_state == STATE_GAME_MENU
        )
    ) {
        return frontend_render_quit_transition(
            old_state,
            new_state
        );
    }
    if (
        !(
            (
                old_state == STATE_GAME_MENU &&
                new_state == STATE_UPGRADE_MENU
            ) ||
            (
                old_state == STATE_UPGRADE_MENU &&
                new_state == STATE_GAME_MENU
            )
        )
    ) {
        return 0;
    }

    /*
     * Game Menu and Upgrade Ship share the exact same menu chrome and ship
     * illustration.  Keep the already composed left half resident and only
     * replace the native-text panel; rebuilding the old path decoded four
     * SHP sprites and copied the full 38.4 KiB frame on every transition.
    */
    if (new_state == STATE_GAME_MENU) {
        if (
            frontend_selection >= FRONTEND_STATIC_GAME_MENU_COUNT ||
            !frontend_copy_static_menu_panel(
                (u8)(
                    FRONTEND_STATIC_GAME_MENU_BASE +
                    frontend_selection
                )
            )
        ) {
            return 0;
        }
        if (
            !frontend_quit_dialog_cache_valid ||
            frontend_quit_dialog_cache_selection !=
                frontend_selection
        ) {
            frontend_quit_capture_dialog_background();
            frontend_quit_dialog_cache_valid = 1;
            frontend_quit_dialog_cache_selection =
                frontend_selection;
        }
    } else {
        if (
            frontend_selection >= FRONTEND_STATIC_UPGRADE_MENU_COUNT ||
            !frontend_copy_static_menu_panel(
                (u8)(
                    FRONTEND_STATIC_UPGRADE_MENU_BASE +
                    frontend_selection
                )
            )
        ) {
            return 0;
        }
    }
    /*
     * A half-screen rectangle would require 120 tiny DMA launches because
     * Mode 4 rows are 240 bytes wide.  One contiguous page transfer is both
     * faster and friendlier to Maxmod than that row-wise commit.
     */
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
    return 1;
}

static void frontend_draw_upgrade_submenu_preview(void)
{
    char line[40];
    char digits[11];

    if (
        frontend_upgrade_category == FRONTEND_UPGRADE_SHIP ||
        frontend_upgrade_category == FRONTEND_UPGRADE_GENERATOR
    ) {
        frontend_draw_ship_illustration(frontend_frame_scratch);
    } else if (frontend_selection < frontend_upgrade_sub_count) {
        frontend_draw_item_source(
            frontend_frame_scratch,
            frontend_upgrade_category,
            frontend_upgrade_sub_item[frontend_selection],
            55,
            70
        );
    }
    if (
        (
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_FRONT_WEAPON ||
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_REAR_WEAPON
        ) &&
        frontend_selection < frontend_upgrade_sub_count
    ) {
        frontend_text_with_u32(
            line,
            sizeof(line),
            "POWER ",
            frontend_upgrade_sub_power[frontend_selection]
        );
        frontend_draw_runtime_text_clipped(
            frontend_frame_scratch,
            line,
            20,
            118,
            111,
            0xfeu
        );
        frontend_draw_runtime_text_clipped(
            frontend_frame_scratch,
            "< LEFT  RIGHT >",
            8,
            130,
            114,
            0xfau
        );
        if (
            frontend_upgrade_category ==
                FRONTEND_UPGRADE_REAR_WEAPON
        ) {
            frontend_text_with_u32(
                line,
                sizeof(line),
                "MODE ",
                frontend_player_items.weapon_mode
            );
            frontend_draw_runtime_text_clipped(
                frontend_frame_scratch,
                line,
                26,
                142,
                111,
                0xfau
            );
        }
    }
    frontend_u32_string(frontend_upgrade_cash_left(), digits);
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        "CASH",
        8,
        148,
        45,
        0xfau
    );
    frontend_draw_runtime_text_clipped(
        frontend_frame_scratch,
        digits,
        45,
        148,
        115,
        0xfeu
    );
}

static void frontend_draw_upgrade_submenu_row(u8 visible_index)
{
    u8 item_index = (u8)(
        frontend_upgrade_sub_scroll + visible_index
    );
    s16 y = (s16)(21 + visible_index * 21u);
    u8 selected = item_index == frontend_selection;
    char line[40];

    if (item_index < frontend_upgrade_sub_count) {
        u8 item = frontend_upgrade_sub_item[item_index];
        u32 cost = frontend_upgrade_total_cost(
            frontend_upgrade_category,
            item,
            frontend_upgrade_sub_power[item_index]
        );
        u8 affordable = cost <= frontend_upgrade_trade_cash;
        const u8 *owned = frontend_player_item_field_const(
            &frontend_upgrade_original_items,
            frontend_upgrade_category
        );

        frontend_upgrade_item_name(
            frontend_upgrade_category,
            item,
            line,
            sizeof(line)
        );
        frontend_draw_item_source(
            frontend_frame_scratch,
            frontend_upgrade_category,
            item,
            160,
            (s16)(y * 5 / 4 - 4)
        );
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            line,
            142,
            y,
            231,
            !affordable ? 0xf4u :
                (selected ? 0xfeu : 0xfau)
        );
        frontend_text_with_u32(
            line,
            sizeof(line),
            "COST ",
            cost
        );
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            line,
            142,
            (s16)(y + 9),
            201,
            !affordable ? 0xf3u :
                (selected ? 0xfcu : 0xf8u)
        );
        if (owned != 0 && *owned == item && item != 0) {
            frontend_draw_upgrade_list_text(
                frontend_frame_scratch,
                "OWN",
                205,
                (s16)(y + 9),
                231,
                0xedu
            );
        }
    } else if (item_index == frontend_upgrade_sub_count) {
        frontend_draw_upgrade_list_text(
            frontend_frame_scratch,
            (
                frontend_text_ready &&
                frontend_text.misc_text[13][0] != '\0'
            ) ?
                frontend_text.misc_text[13] :
                "DONE",
            133,
            y,
            232,
            selected ? 0xfeu : 0xfau
        );
    }
}

static void frontend_draw_upgrade_submenu_rows(void)
{
    u8 visible_index;

    for (visible_index = 0; visible_index < 6; visible_index++) {
        frontend_draw_upgrade_submenu_row(visible_index);
    }
}

static void frontend_render_upgrade_submenu(void)
{
    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        frontend_upgrade_menu_text(
            (u8)(frontend_upgrade_category + 1u),
            "UPGRADE"
        ),
        176,
        6,
        0xfbu
    );
    frontend_draw_upgrade_submenu_preview();
    frontend_draw_upgrade_submenu_rows();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static void frontend_render_upgrade_submenu_update(
    u8 old_selection,
    u8 old_scroll
)
{
    u8 selection[2] = {old_selection, frontend_selection};
    u8 index;

    frontend_dirty_begin();
    /*
     * The left preview is the only configuration-dependent illustration.
     * Rebuild it without touching the 120-pixel-wide list or page chrome.
     */
    frontend_restore_menu_chrome_rect(0, 16, 120, 144);
    frontend_draw_upgrade_submenu_preview();
    frontend_dirty_add(0, 16, 120, 144);

    if (old_scroll != frontend_upgrade_sub_scroll) {
        frontend_restore_menu_chrome_rect(120, 16, 120, 132);
        frontend_draw_upgrade_submenu_rows();
        frontend_dirty_add(120, 16, 120, 132);
    } else {
        for (index = 0; index < 2; index++) {
            u8 item_index = selection[index];

            if (
                item_index >= frontend_upgrade_sub_scroll &&
                item_index <
                    frontend_upgrade_sub_scroll + 6u
            ) {
                u8 visible_index = (u8)(
                    item_index - frontend_upgrade_sub_scroll
                );
                s16 y = (s16)(17 + visible_index * 21u);

                frontend_restore_menu_chrome_rect(
                    120,
                    (u16)y,
                    120,
                    27
                );
                frontend_draw_upgrade_submenu_row(visible_index);
                frontend_dirty_add(120, y, 120, 27);
            }
        }
    }
    frontend_dirty_present();
}

static void frontend_quit_capture_choice_background(void)
{
    u8 row;

    for (row = 0; row < 14; row++) {
        memcpy(
            frontend_quit_choice_background + (u16)row * 160u,
            frontend_frame_scratch +
                (u32)(104u + row) * SCREEN_WIDTH + 32u,
            160
        );
    }
}

static void frontend_quit_restore_choice_background(void)
{
    u8 row;

    for (row = 0; row < 14; row++) {
        memcpy(
            frontend_frame_scratch +
                (u32)(104u + row) * SCREEN_WIDTH + 32u,
            frontend_quit_choice_background + (u16)row * 160u,
            160
        );
    }
}

static void frontend_quit_capture_dialog_background(void)
{
    /*
     * Cache complete scanlines.  The extra 5.9 KiB costs less than 100
     * separate memcpy calls and still leaves 768 bytes in the shared arena.
     */
    memcpy(
        frontend_quit_dialog_background,
        frontend_frame_scratch +
            (u32)FRONTEND_QUIT_DIALOG_CACHE_Y * SCREEN_WIDTH,
        FRONTEND_QUIT_DIALOG_CACHE_BYTES
    );
}

static void frontend_quit_restore_dialog_background(void)
{
    memcpy(
        frontend_frame_scratch +
            (u32)FRONTEND_QUIT_DIALOG_CACHE_Y * SCREEN_WIDTH,
        frontend_quit_dialog_background,
        FRONTEND_QUIT_DIALOG_CACHE_BYTES
    );
}

static void frontend_draw_quit_choices(
    const char *yes,
    const char *no
)
{
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        yes,
        66,
        108,
        frontend_quit_yes ? 0xfeu : 0xf6u
    );
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        no,
        137,
        108,
        frontend_quit_yes ? 0xf6u : 0xfeu
    );
}

static void frontend_render_quit_selection(void)
{
    const char *yes =
        frontend_text_ready &&
        frontend_text.misc_text[9][0] != '\0' ?
            frontend_text.misc_text[9] :
            "OK";
    const char *no =
        frontend_text_ready &&
        frontend_text.misc_text[10][0] != '\0' ?
            frontend_text.misc_text[10] :
            "CANCEL";

    if (!frontend_apply_static_quit_choice()) {
        frontend_quit_restore_choice_background();
        frontend_draw_quit_choices(yes, no);
    }
    frontend_dirty_begin();
    frontend_dirty_add(32, 104, 160, 14);
    frontend_dirty_present();
}

static void frontend_compose_quit_dialog(void)
{
    const char *question =
        frontend_text_ready &&
        frontend_text.misc_text[28][0] != '\0' ?
            frontend_text.misc_text[28] :
            "ARE YOU SURE YOU WANT TO EXIT?";
    const char *help =
        frontend_text_ready &&
        frontend_text.misc_text[30][0] != '\0' ?
            frontend_text.misc_text[30] :
            "YOU WILL RETURN TO THE MAIN MENU.";
    const char *yes =
        frontend_text_ready &&
        frontend_text.misc_text[9][0] != '\0' ?
            frontend_text.misc_text[9] :
            "OK";
    const char *no =
        frontend_text_ready &&
        frontend_text.misc_text[10][0] != '\0' ?
            frontend_text.misc_text[10] :
            "CANCEL";
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    u32 cycle_start = stress_cycle_counter_read();
    u32 elapsed;
#endif

    if (!frontend_apply_static_quit_shade()) {
        frontend_shade_source_rect(
            frontend_frame_scratch,
            65,
            55,
            255,
            155,
            3
        );
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_shade_cycles_max) {
        telemetry_frontend_quit_shade_cycles_max = elapsed;
    }
    cycle_start = stress_cycle_counter_read();
#endif
    if (!frontend_apply_static_quit_overlay()) {
        (void)frontend_blit_shp_source(
            frontend_frame_scratch,
            5,
            35,
            50,
            50,
            FRONTEND_SOURCE_BLIT_NORMAL,
            0,
            0
        );
        frontend_draw_runtime_text_clipped_scaled(
            frontend_frame_scratch,
            question,
            37,
            53,
            174,
            0xfeu,
            FRONTEND_COMPACT_FONT_WIDTH,
            FRONTEND_COMPACT_FONT_SPACE
        );
        frontend_draw_runtime_text_wrapped_compact(
            frontend_frame_scratch,
            help,
            37,
            73,
            174,
            9,
            3,
            0xfau
        );
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_overlay_cycles_max) {
        telemetry_frontend_quit_overlay_cycles_max = elapsed;
    }
    cycle_start = stress_cycle_counter_read();
#endif
    if (!frontend_apply_static_quit_choice()) {
        frontend_quit_capture_choice_background();
        frontend_draw_quit_choices(yes, no);
    }
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
    elapsed = stress_cycle_counter_read() - cycle_start;
    if (elapsed > telemetry_frontend_quit_choices_cycles_max) {
        telemetry_frontend_quit_choices_cycles_max = elapsed;
    }
#endif
}

static u8 frontend_render_quit_transition(
    u8 old_state,
    u8 new_state
)
{
    if (
        old_state == STATE_GAME_MENU &&
        new_state == STATE_QUIT_CONFIRM
    ) {
        /*
         * The confirmation dialog is an overlay on the current Game Menu.
         * Preserve its affected rectangle instead of decoding the four SHP
         * ship sprites and rebuilding all menu chrome a second time.
         */
        if (
            !frontend_quit_dialog_cache_valid ||
            frontend_quit_dialog_cache_selection != 5
        ) {
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
            u32 cycle_start = stress_cycle_counter_read();
#endif
            if (
                !frontend_copy_static_menu_panel(
                    FRONTEND_STATIC_GAME_MENU_BASE + 5u
                )
            ) {
                return 0;
            }
            frontend_quit_capture_dialog_background();
            frontend_quit_dialog_cache_valid = 1;
            frontend_quit_dialog_cache_selection = 5;
#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
            {
                u32 elapsed =
                    stress_cycle_counter_read() - cycle_start;

                if (
                    elapsed >
                    telemetry_frontend_quit_capture_cycles_max
                ) {
                    telemetry_frontend_quit_capture_cycles_max =
                        elapsed;
                }
            }
#endif
        }
        frontend_compose_quit_dialog();
    } else if (
        old_state == STATE_QUIT_CONFIRM &&
        new_state == STATE_GAME_MENU
    ) {
        frontend_quit_restore_dialog_background();
    } else {
        return 0;
    }
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
    return 1;
}

static void frontend_render_quit_confirm(void)
{
    frontend_compose_game_menu(5);
    frontend_quit_capture_dialog_background();
    frontend_quit_dialog_cache_valid = 1;
    frontend_quit_dialog_cache_selection = 5;
    frontend_compose_quit_dialog();
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_MENU_CHROME)
    );
}

static const u16 frontend_planet_x[21] = {
    200, 150, 240, 300, 270, 280, 320, 260, 220, 150, 160,
    210, 80, 240, 220, 180, 310, 330, 150, 240, 200
};

static const u16 frontend_planet_y[21] = {
    40, 90, 90, 80, 170, 30, 50, 130, 120, 150, 220,
    200, 80, 50, 160, 10, 55, 55, 90, 90, 40
};

static const u16 frontend_planet_graphic[21] = {
    4, 1, 2, 3, 20, 36, 52, 68, 84, 100, 116,
    132, 151, 151, 151, 151, 52, 52, 1, 2, 4
};

static const u8 frontend_planet_animated[21] = {
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

static u8 frontend_nav_obj_phase_component(s16 coordinate)
{
    s16 phase = (s16)(coordinate % FRONTEND_NAV_OBJ_SCALE_PHASES);

    if (phase < 0) phase += FRONTEND_NAV_OBJ_SCALE_PHASES;
    return (u8)phase;
}

static const FrontendNavObjMeta *frontend_nav_obj_metadata(
    u16 catalog,
    u8 phase
)
{
    const FrontendNavObjMeta *records =
        (const FrontendNavObjMeta *)(const void *)frontend_nav_obj_meta;

    if (
        catalog >= FRONTEND_NAV_OBJ_CATALOG_COUNT ||
        phase >= FRONTEND_NAV_OBJ_PHASE_COUNT
    ) {
        return 0;
    }
    return &records[
        (u32)catalog * FRONTEND_NAV_OBJ_PHASE_COUNT + phase
    ];
}

static u16 frontend_nav_obj_graphic(u8 planet_index)
{
    u16 graphic;

    if (
        planet_index >= 21 ||
        frontend_planet_graphic[planet_index] == 0
    ) {
        return UINT16_MAX;
    }
    graphic = (u16)(frontend_planet_graphic[planet_index] - 1u);
    if (
        frontend_planet_animated[planet_index] &&
        frontend_nav_planet_animation < 15
    ) {
        graphic = (u16)(
            graphic + frontend_nav_planet_animation
        );
    }
    return graphic;
}

static u8 frontend_nav_obj_source_position(
    u8 planet_index,
    u16 graphic,
    const FrontendNavObjMeta **metadata,
    u8 *phase,
    s16 *source_x,
    s16 *source_y
)
{
    const FrontendNavObjMeta *base;
    const FrontendNavObjMeta *resolved;
    u8 resolved_phase;
    s16 x;
    s16 y;

    if (
        planet_index >= 21 ||
        graphic >= FRONTEND_NAV_OBJ_PLANET_CATALOG_COUNT
    ) {
        return 0;
    }
    base = frontend_nav_obj_metadata(graphic, 0);
    if (
        base == 0 ||
        base->source_width == 0 ||
        base->source_height == 0
    ) {
        return 0;
    }
    x = (s16)(
        frontend_planet_x[planet_index] +
        66 -
        frontend_nav_presented_x -
        (s16)(base->source_width / 2u)
    );
    y = (s16)(
        frontend_planet_y[planet_index] +
        85 -
        frontend_nav_presented_y -
        (s16)(base->source_height / 2u)
    );
    resolved_phase = (u8)(
        frontend_nav_obj_phase_component(y) *
            FRONTEND_NAV_OBJ_SCALE_PHASES +
        frontend_nav_obj_phase_component(x)
    );
    resolved = frontend_nav_obj_metadata(graphic, resolved_phase);
    if (
        resolved == 0 ||
        resolved->tile_bytes == 0 ||
        resolved->source_width == 0 ||
        resolved->source_height == 0
    ) {
        return 0;
    }
    if (metadata != 0) *metadata = resolved;
    if (phase != 0) *phase = resolved_phase;
    if (source_x != 0) *source_x = x;
    if (source_y != 0) *source_y = y;
    return 1;
}

static u8 frontend_nav_obj_planet_visible(
    s16 source_x,
    s16 source_y,
    const FrontendNavObjMeta *metadata
)
{
    if (metadata == 0) return 0;
    /*
     * Preserve JE_drawNavLines' source-space rejection before the hardware
     * window performs the final ornate-frame clip.
     */
    return !(
        source_x <= -7 ||
        source_x + (s16)metadata->source_width >= 170 ||
        source_y <= 0 ||
        source_y >= 160
    );
}

static u8 frontend_nav_obj_collect_planets(
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY]
)
{
    u32 present = 0;
    u8 count = 0;
    u8 planet;
    u8 choice_count = frontend_next_choice_count();

    for (planet = 0; planet < 11; planet++) {
        planets[count++] = planet;
        present |= 1u << planet;
    }
    for (planet = 0; planet < choice_count; planet++) {
        u8 map_planet = frontend_map.map_planet[
            frontend_next_map_choice(planet)
        ];
        u8 index;

        if (map_planet <= 11 || map_planet > 21) continue;
        index = (u8)(map_planet - 1u);
        if (present & (1u << index)) continue;
        planets[count++] = index;
        present |= 1u << index;
    }
    if (
        frontend_map.map_origin > 11 &&
        frontend_map.map_origin <= 21
    ) {
        u8 index = (u8)(frontend_map.map_origin - 1u);

        if (!(present & (1u << index))) {
            planets[count++] = index;
        }
    }
    return count;
}

static u16 frontend_nav_obj_planet_reserve(u8 planet_index)
{
    u16 graphic = (u16)(
        frontend_planet_graphic[planet_index] - 1u
    );
    u8 frame_count =
        frontend_planet_animated[planet_index] ? 15 : 1;
    u16 reserve;
    u8 frame;

    if (frontend_nav_obj_reserve_cache[planet_index] != 0) {
        return frontend_nav_obj_reserve_cache[planet_index];
    }
    reserve = 0;
    for (frame = 0; frame < frame_count; frame++) {
        u8 phase;

        for (
            phase = 0;
            phase < FRONTEND_NAV_OBJ_PHASE_COUNT;
            phase++
        ) {
            const FrontendNavObjMeta *metadata =
                frontend_nav_obj_metadata(
                    (u16)(graphic + frame),
                    phase
                );

            if (
                metadata != 0 &&
                metadata->tile_bytes > reserve
            ) {
                reserve = metadata->tile_bytes;
            }
        }
    }
    frontend_nav_obj_reserve_cache[planet_index] = reserve;
    return reserve;
}

static void frontend_nav_obj_schedule_upload(
    const FrontendNavObjMeta *metadata,
    u16 vram_offset
)
{
    FrontendNavObjUpload *upload;

    if (
        metadata == 0 ||
        metadata->tile_bytes == 0
    ) {
        return;
    }
    if (
        frontend_nav_obj_upload_count >=
            FRONTEND_NAV_OBJ_UPLOAD_CAPACITY
    ) {
        telemetry_frontend_nav_obj_overflows++;
        return;
    }
    upload = &frontend_nav_obj_upload[
        frontend_nav_obj_upload_count++
    ];
    upload->source =
        frontend_nav_obj_tiles + metadata->tile_offset;
    upload->vram_offset = vram_offset;
    upload->bytes = metadata->tile_bytes;
}

static u8 frontend_nav_obj_axis_chunk(u8 remaining)
{
    if (remaining > 24) return 32;
    if (remaining > 8) return 16;
    return 8;
}

static void frontend_nav_obj_put(
    s16 x,
    s16 y,
    u16 tile,
    u8 width,
    u8 height
)
{
    OBJATTR *object;
    u16 shape;
    u16 size;

    if (
        oam_count >= HARDWARE_OAM_ENTRIES ||
        x <= -(s16)width || x >= SCREEN_WIDTH ||
        y <= -(s16)height || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    if (width == height) {
        shape = ATTR0_SQUARE;
        size =
            width == 32 ? ATTR1_SIZE_32 :
            (width == 16 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    } else if (width > height) {
        shape = ATTR0_WIDE;
        size =
            width == 32 && height == 16 ? ATTR1_SIZE_32 :
            (width == 32 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    } else {
        shape = ATTR0_TALL;
        size =
            width == 16 && height == 32 ? ATTR1_SIZE_32 :
            (height == 32 ? ATTR1_SIZE_16 : ATTR1_SIZE_8);
    }
    object = &oam_shadow[oam_count++];
    object->attr0 =
        (y & 0x00ff) |
        ATTR0_NORMAL |
        ATTR0_COLOR_256 |
        shape;
    object->attr1 = (x & 0x01ff) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(0);
    object->dummy = 0;
}

static void frontend_nav_obj_draw_slot(
    const FrontendNavObjSlot *slot
)
{
    const FrontendNavObjMeta *metadata;
    s16 source_x;
    s16 source_y;
    s16 screen_x;
    s16 screen_y;
    u16 asset_offset = 0;
    u8 y = 0;

    if (
        slot == 0 ||
        !frontend_nav_obj_source_position(
            slot->planet_index,
            slot->graphic,
            &metadata,
            0,
            &source_x,
            &source_y
        ) ||
        !frontend_nav_obj_planet_visible(
            source_x,
            source_y,
            metadata
        )
    ) {
        return;
    }
    screen_x = frontend_source_to_screen_x(source_x);
    screen_y = frontend_source_to_screen_y(source_y);
    while (y < metadata->screen_height) {
        u8 chunk_height = frontend_nav_obj_axis_chunk(
            (u8)(metadata->screen_height - y)
        );
        u8 x = 0;

        while (x < metadata->screen_width) {
            u8 chunk_width = frontend_nav_obj_axis_chunk(
                (u8)(metadata->screen_width - x)
            );

            frontend_nav_obj_put(
                (s16)(screen_x + x),
                (s16)(screen_y + y),
                (u16)(
                    FRONTEND_NAV_OBJ_VRAM_TILE_BASE +
                    (slot->vram_offset + asset_offset) / 32u
                ),
                chunk_width,
                chunk_height
            );
            asset_offset = (u16)(
                asset_offset +
                (u16)chunk_width * chunk_height
            );
            x = (u8)(x + chunk_width);
        }
        y = (u8)(y + chunk_height);
    }
}

static void frontend_nav_obj_draw_dots(void)
{
    u8 route;
    u8 route_count = frontend_next_choice_count();
    u8 origin = frontend_map.map_origin;

    if (origin == 0 || origin > 21) return;
    for (route = 0; route < route_count; route++) {
        u8 map_choice = frontend_next_map_choice(route);
        u8 destination = frontend_map.map_planet[map_choice];
        u8 dots = frontend_nav_dot_count(destination);
        u8 dot;

        for (dot = 0; dot < dots; dot++) {
            s16 x = frontend_nav_part_way(
                (s16)frontend_planet_x[origin - 1u],
                (s16)frontend_planet_x[destination - 1u],
                dots,
                dot
            );
            s16 y = frontend_nav_part_way(
                (s16)frontend_planet_y[origin - 1u],
                (s16)frontend_planet_y[destination - 1u],
                dots,
                dot
            );
            u16 tile;

            x = (s16)(x - frontend_nav_presented_x + 64);
            y = (s16)(y - frontend_nav_presented_y + 83);
            if (x <= 0 || x >= 140 || y <= 0 || y >= 168) {
                continue;
            }
            tile = (u16)(
                FRONTEND_NAV_OBJ_VRAM_TILE_BASE +
                (
                    (
                        route == frontend_selection &&
                        dot < frontend_nav_dot_animation
                    ) ?
                        FRONTEND_NAV_OBJ_DOT_ON_OFFSET :
                        FRONTEND_NAV_OBJ_DOT_OFF_OFFSET
                ) /
                32u
            );
            frontend_nav_obj_put(
                frontend_source_to_screen_x(x),
                frontend_source_to_screen_y(y),
                tile,
                FRONTEND_NAV_OBJ_DOT_DIM,
                FRONTEND_NAV_OBJ_DOT_DIM
            );
        }
    }
}

static void frontend_nav_obj_render_oam(void)
{
    u8 old_oam_count = previous_oam_count;
    u8 slot = frontend_nav_obj_slot_count;

    if (!frontend_nav_obj_active) return;
    oam_count = 0;
    /*
     * Software drew later planets over earlier ones.  Equal-priority GBA
     * OBJ uses the lower OAM index in front, so emit the source list in
     * reverse and append route dots behind every planet.
     */
    while (slot != 0) {
        frontend_nav_obj_draw_slot(
            &frontend_nav_obj_slot[--slot]
        );
    }
    frontend_nav_obj_draw_dots();
    {
        u8 new_oam_count = oam_count;

        while (oam_count < old_oam_count) {
            oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
            oam_shadow[oam_count].attr1 = 0;
            oam_shadow[oam_count].attr2 = 0;
            oam_shadow[oam_count].dummy = 0;
            oam_count++;
        }
        previous_oam_count = new_oam_count;
        oam_count = new_oam_count;
    }
    oam_dirty = 1;
    telemetry_frontend_nav_obj_updates++;
}

static void frontend_nav_obj_rebuild(void)
{
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY];
    u8 candidate_count = frontend_nav_obj_collect_planets(planets);
    u16 vram_offset = FRONTEND_NAV_OBJ_DOT_BYTES;
    u8 candidate;

    frontend_nav_obj_active = 1;
    frontend_nav_obj_palette_pending = 1;
    frontend_nav_obj_dot_tiles_pending = 1;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_slot_count = 0;
    for (candidate = 0; candidate < candidate_count; candidate++) {
        u8 planet_index = planets[candidate];
        u16 graphic = frontend_nav_obj_graphic(planet_index);
        const FrontendNavObjMeta *metadata;
        s16 source_x;
        s16 source_y;
        u8 phase;
        u16 reserve;
        FrontendNavObjSlot *slot;

        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                planet_index,
                graphic,
                &metadata,
                &phase,
                &source_x,
                &source_y
            ) ||
            !frontend_nav_obj_planet_visible(
                source_x,
                source_y,
                metadata
            )
        ) {
            continue;
        }
        reserve = frontend_nav_obj_planet_reserve(planet_index);
        if (
            reserve == 0 ||
            vram_offset + reserve > FRONTEND_NAV_OBJ_VRAM_BYTES ||
            frontend_nav_obj_slot_count >=
                FRONTEND_NAV_OBJ_SLOT_CAPACITY
        ) {
            telemetry_frontend_nav_obj_overflows++;
            continue;
        }
        slot = &frontend_nav_obj_slot[
            frontend_nav_obj_slot_count++
        ];
        slot->planet_index = planet_index;
        slot->graphic = (u8)graphic;
        slot->phase = phase;
        slot->animated = frontend_planet_animated[planet_index];
        slot->vram_offset = vram_offset;
        slot->reserve_bytes = reserve;
        frontend_nav_obj_schedule_upload(metadata, vram_offset);
        vram_offset = (u16)(vram_offset + reserve);
    }
    frontend_nav_obj_render_oam();
}

static u8 frontend_nav_obj_has_visible_layout_change(void)
{
    u8 planets[FRONTEND_NAV_OBJ_SLOT_CAPACITY];
    u8 candidate_count = frontend_nav_obj_collect_planets(planets);
    u8 visible_count = 0;
    u8 candidate;

    for (candidate = 0; candidate < candidate_count; candidate++) {
        u8 planet_index = planets[candidate];
        u16 graphic = frontend_nav_obj_graphic(planet_index);
        const FrontendNavObjMeta *metadata;
        s16 source_x;
        s16 source_y;

        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                planet_index,
                graphic,
                &metadata,
                0,
                &source_x,
                &source_y
            ) ||
            !frontend_nav_obj_planet_visible(
                source_x,
                source_y,
                metadata
            )
        ) {
            continue;
        }
        if (
            visible_count >= frontend_nav_obj_slot_count ||
            frontend_nav_obj_slot[visible_count].planet_index !=
                planet_index
        ) {
            return 1;
        }
        visible_count++;
    }
    return visible_count != frontend_nav_obj_slot_count;
}

static void frontend_nav_obj_animate(void)
{
    u8 slot_index;

    if (
        !frontend_nav_obj_active ||
        frontend_nav_obj_has_visible_layout_change()
    ) {
        frontend_nav_obj_rebuild();
        return;
    }
    frontend_nav_obj_upload_count = 0;
    for (
        slot_index = 0;
        slot_index < frontend_nav_obj_slot_count;
        slot_index++
    ) {
        FrontendNavObjSlot *slot =
            &frontend_nav_obj_slot[slot_index];
        u16 graphic;
        const FrontendNavObjMeta *metadata;
        u8 phase;

        if (!slot->animated) continue;
        graphic = frontend_nav_obj_graphic(slot->planet_index);
        if (
            graphic == UINT16_MAX ||
            !frontend_nav_obj_source_position(
                slot->planet_index,
                graphic,
                &metadata,
                &phase,
                0,
                0
            ) ||
            metadata->tile_bytes > slot->reserve_bytes
        ) {
            frontend_nav_obj_rebuild();
            return;
        }
        slot->graphic = (u8)graphic;
        slot->phase = phase;
        frontend_nav_obj_schedule_upload(
            metadata,
            slot->vram_offset
        );
    }
    frontend_nav_obj_render_oam();
}

static void frontend_nav_obj_commit_vblank(void)
{
    u8 upload;

    if (!frontend_nav_obj_active) {
        frontend_nav_obj_upload_count = 0;
        frontend_nav_obj_palette_pending = 0;
        frontend_nav_obj_dot_tiles_pending = 0;
        return;
    }
    if (frontend_nav_obj_palette_pending) {
        dmaCopy(
            frontend_nav_obj_palette,
            SPRITE_PALETTE,
            FRONTEND_NAV_OBJ_PALETTE_BYTES
        );
        frontend_nav_obj_palette_pending = 0;
    }
    if (frontend_nav_obj_dot_tiles_pending) {
        const FrontendNavObjMeta *off =
            frontend_nav_obj_metadata(
                FRONTEND_NAV_OBJ_DOT_OFF_CATALOG,
                0
            );
        const FrontendNavObjMeta *on =
            frontend_nav_obj_metadata(
                FRONTEND_NAV_OBJ_DOT_ON_CATALOG,
                0
            );

        if (off != 0 && on != 0) {
            dmaCopy(
                frontend_nav_obj_tiles + off->tile_offset,
                FRONTEND_NAV_OBJ_VRAM +
                    FRONTEND_NAV_OBJ_DOT_OFF_OFFSET,
                off->tile_bytes
            );
            dmaCopy(
                frontend_nav_obj_tiles + on->tile_offset,
                FRONTEND_NAV_OBJ_VRAM +
                    FRONTEND_NAV_OBJ_DOT_ON_OFFSET,
                on->tile_bytes
            );
            telemetry_frontend_nav_obj_uploads += 2;
            telemetry_frontend_nav_obj_upload_bytes +=
                off->tile_bytes + on->tile_bytes;
        }
        frontend_nav_obj_dot_tiles_pending = 0;
    }
    for (
        upload = 0;
        upload < frontend_nav_obj_upload_count;
        upload++
    ) {
        const FrontendNavObjUpload *pending =
            &frontend_nav_obj_upload[upload];

        dmaCopy(
            pending->source,
            FRONTEND_NAV_OBJ_VRAM + pending->vram_offset,
            pending->bytes
        );
        telemetry_frontend_nav_obj_uploads++;
        telemetry_frontend_nav_obj_upload_bytes += pending->bytes;
    }
    frontend_nav_obj_upload_count = 0;
}

static void frontend_nav_obj_disable(void)
{
    if (
        frontend_nav_obj_active ||
        previous_oam_count != 0
    ) {
        hide_all_sprites();
    }
    frontend_nav_obj_active = 0;
    frontend_nav_obj_slot_count = 0;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_palette_pending = 0;
    frontend_nav_obj_dot_tiles_pending = 0;
}

static u8 frontend_nav_planet_dimensions(
    u8 planet,
    u16 *width,
    u16 *height
)
{
    const FrontendNavObjMeta *metadata;
    u16 graphic;

    if (
        planet == 0 ||
        planet > 21
    ) {
        return 0;
    }
    graphic = (u16)(frontend_planet_graphic[planet - 1u] - 1u);
    metadata = frontend_nav_obj_metadata(graphic, 0);
    if (
        metadata == 0 ||
        metadata->source_width == 0 ||
        metadata->source_height == 0
    ) {
        return 0;
    }
    if (width != 0) *width = metadata->source_width;
    if (height != 0) *height = metadata->source_height;
    return 1;
}

static void frontend_nav_set_target(void)
{
    u8 choice_count = frontend_next_choice_count();
    u8 origin = frontend_map.map_origin;
    u8 destination;
    u16 origin_width;
    u16 origin_height;
    u16 destination_width;
    u16 destination_height;

    if (
        frontend_selection >= choice_count ||
        origin == 0 ||
        origin > 21
    ) {
        return;
    }
    destination = frontend_map.map_planet[
        frontend_next_map_choice(frontend_selection)
    ];
    if (
        destination == 0 ||
        destination > 21 ||
        !frontend_nav_planet_dimensions(
            origin,
            &origin_width,
            &origin_height
        ) ||
        !frontend_nav_planet_dimensions(
            destination,
            &destination_width,
            &destination_height
        )
    ) {
        return;
    }
    frontend_nav_target_x = (s16)(
        (
            (s16)frontend_planet_x[origin - 1u] -
                (s16)(origin_width / 2u) +
            (s16)frontend_planet_x[destination - 1u] -
                (s16)(destination_width / 2u)
        ) /
        2
    );
    frontend_nav_target_y = (s16)(
        (
            (s16)frontend_planet_y[origin - 1u] -
                (s16)(origin_height / 2u) +
            (s16)frontend_planet_y[destination - 1u] -
                (s16)(destination_height / 2u)
        ) /
        2
    );
}

static void frontend_nav_reset(void)
{
    u8 origin = frontend_map.map_origin;

    if (origin == 0 || origin > 21) origin = 1;
    frontend_nav_x = (s16)frontend_planet_x[origin - 1u];
    frontend_nav_y = (s16)frontend_planet_y[origin - 1u];
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
    frontend_nav_target_x = frontend_nav_x;
    frontend_nav_target_y = frontend_nav_y;
    frontend_nav_planet_animation = 0;
    frontend_nav_animation_wait = 0;
    frontend_nav_dot_animation = 0;
    frontend_nav_dot_wait = 0;
    frontend_nav_set_target();
}

static u8 frontend_nav_dot_count(u8 destination)
{
    s32 dx;
    s32 dy;
    u32 squared;
    u8 root;
    u8 best = 0;
    u32 best_distance = UINT32_MAX;
    u8 origin = frontend_map.map_origin;

    if (
        origin == 0 ||
        origin > 21 ||
        destination == 0 ||
        destination > 21
    ) {
        return 0;
    }
    dx =
        (s32)frontend_planet_x[destination - 1u] -
        frontend_planet_x[origin - 1u];
    dy =
        (s32)frontend_planet_y[destination - 1u] -
        frontend_planet_y[origin - 1u];
    squared = (u32)(dx * dx + dy * dy);
    if (squared == 0) return 0;
    for (root = 1; root <= 20; root++) {
        u32 square = (u32)root * root;
        u32 fourth = square * square;
        u32 distance =
            fourth > squared ?
                fourth - squared :
                squared - fourth;

        if (distance < best_distance) {
            best_distance = distance;
            best = root;
        }
    }
    if (best == 0) return 0;
    best--;
    return best > 10 ? 10 : best;
}

static s16 frontend_nav_part_way(
    s16 start,
    s16 finish,
    u8 dots,
    u8 distance
)
{
    return (s16)(
        (finish - start) / (dots + 2) * (distance + 1) + start
    );
}

static u8 frontend_nav_grid_phase(s16 coordinate)
{
    s16 phase = (s16)(coordinate % FRONTEND_NAV_GRID_PHASES);

    if (phase < 0) phase += FRONTEND_NAV_GRID_PHASES;
    return (u8)phase;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone)) void
frontend_nav_copy_bitmap_page(u8 *destination, const u8 *source)
{
    u8 row;

    /*
     * The generated ROM page pads each 126-pixel row to 128 bytes.  Both
     * pointers are therefore word aligned for every iteration, avoiding 138
     * generic memcpy calls and keeping sequential Game Pak prefetch active.
     */
    for (row = 0; row < FRONTEND_NAV_BITMAP_HEIGHT; row++) {
        const u32 *source_words =
            (const u32 *)(const void *)source;
        u32 *destination_words = (u32 *)(void *)destination;
        u8 word;

        for (word = 0; word < 31; word++) {
            destination_words[word] = source_words[word];
        }
        *(u16 *)(void *)(destination + 124) =
            *(const u16 *)(const void *)(source + 124);
        source += FRONTEND_NAV_BITMAP_STRIDE;
        destination += SCREEN_WIDTH;
    }
}

static void frontend_nav_draw(void)
{
    u8 phase_x = frontend_nav_grid_phase(
        (s16)(frontend_nav_presented_x >> 1)
    );
    u8 phase_y = frontend_nav_grid_phase(
        (s16)(frontend_nav_presented_y >> 1)
    );
    const u8 *source =
        frontend_nav_bitmap_pages +
        (
            (u32)phase_y * FRONTEND_NAV_GRID_PHASES +
            phase_x
        ) *
        FRONTEND_NAV_BITMAP_PAGE_BYTES;
    frontend_nav_copy_bitmap_page(frontend_frame_scratch, source);
    telemetry_frontend_nav_bitmap_redraws++;
}

static void frontend_render_next_level_item(u8 selection)
{
    u8 choice_count = frontend_next_choice_count();
    const char *label;
    u16 y;

    if (selection < choice_count) {
        y = (u16)(
            FRONTEND_NEXT_CHOICE_Y +
            selection * FRONTEND_NEXT_CHOICE_STEP
        );
        label = frontend_next_choice_label(selection);
    } else {
        y = FRONTEND_NEXT_EXIT_Y;
        label = (
            frontend_text_ready &&
            frontend_text.misc_text[5][0] != '\0'
        ) ?
            frontend_text.misc_text[5] :
            "EXIT TO GAME MENU";
    }
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_ITEM_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
    frontend_draw_runtime_text(
        frontend_frame_scratch,
        label,
        FRONTEND_NEXT_PANEL_X,
        (s16)y,
        selection == frontend_selection ?
            FRONTEND_NEXT_SELECTED_COLOUR :
            FRONTEND_NEXT_TEXT_COLOUR
    );
}

static void frontend_render_next_level(void)
{
    u8 choice_count = frontend_next_choice_count();
    u8 choice;

    memcpy(
        frontend_frame_scratch,
        frontend_frame_address(FRONTEND_FRAME_MENU_CHROME),
        FRONTEND_FRAME_BYTES
    );
    frontend_nav_set_target();
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
    frontend_nav_draw();
    frontend_draw_runtime_text_centered(
        frontend_frame_scratch,
        "NEXT LEVEL",
        176,
        7,
        0xfbu
    );
    frontend_fill_rect(
        frontend_frame_scratch,
        FRONTEND_NEXT_PANEL_X,
        FRONTEND_NEXT_PANEL_Y,
        FRONTEND_NEXT_PANEL_WIDTH,
        FRONTEND_NEXT_PANEL_HEIGHT,
        FRONTEND_NEXT_BACKGROUND_COLOUR
    );
    for (choice = 0; choice < choice_count; choice++) {
        frontend_render_next_level_item(choice);
    }
    frontend_render_next_level_item(choice_count);
    frontend_present(
        frontend_frame_scratch,
        frontend_palette_address(FRONTEND_FRAME_NEXT_LEVEL_0)
    );
    frontend_nav_obj_rebuild();
}

static void frontend_render_next_level_selection(u8 old_selection)
{
    u8 choice_count = frontend_next_choice_count();

    frontend_nav_dot_animation = 0;
    frontend_nav_dot_wait = 0;
    frontend_nav_set_target();
    frontend_render_next_level_item(old_selection);
    frontend_render_next_level_item(frontend_selection);
    frontend_dirty_begin();
    {
        u16 old_y =
            old_selection < choice_count ?
                (u16)(
                    FRONTEND_NEXT_CHOICE_Y +
                    old_selection * FRONTEND_NEXT_CHOICE_STEP
                ) :
                FRONTEND_NEXT_EXIT_Y;
        u16 new_y =
            frontend_selection < choice_count ?
                (u16)(
                    FRONTEND_NEXT_CHOICE_Y +
                    frontend_selection *
                        FRONTEND_NEXT_CHOICE_STEP
                ) :
                FRONTEND_NEXT_EXIT_Y;

        frontend_dirty_add(
            FRONTEND_NEXT_PANEL_X,
            (s16)old_y,
            FRONTEND_NEXT_PANEL_WIDTH,
            FRONTEND_NEXT_ITEM_HEIGHT
        );
        frontend_dirty_add(
            FRONTEND_NEXT_PANEL_X,
            (s16)new_y,
            FRONTEND_NEXT_PANEL_WIDTH,
            FRONTEND_NEXT_ITEM_HEIGHT
        );
    }
    frontend_dirty_present();
    /*
     * Selecting "Exit" does not move the navigation camera.  The old path
     * nevertheless decoded and redrew the complete 126x138 map on every
     * cursor press.  Route dots are OBJ, so selection changes only need OAM
     * plus the two affected native-text rows; genuine destination changes
     * are picked up by the camera update below.
     */
    frontend_nav_obj_render_oam();
}

static void frontend_render_next_level_nav(void)
{
    frontend_nav_presented_x = frontend_nav_x;
    frontend_nav_presented_y = frontend_nav_y;
    frontend_nav_draw();
    frontend_dirty_begin();
    frontend_dirty_add(
        FRONTEND_NAV_DIRTY_X,
        FRONTEND_NAV_DIRTY_Y,
        FRONTEND_NAV_DIRTY_WIDTH,
        FRONTEND_NAV_DIRTY_HEIGHT
    );
    frontend_dirty_present();
    frontend_nav_obj_rebuild();
}

static u8 frontend_stats_cube_total(void)
{
    u32 total = source_parity_level.data_cube_pickup_count;

    return total > FRONTEND_STATS_MAX_CUBES ?
        FRONTEND_STATS_MAX_CUBES :
        (u8)total;
}

static u8 *frontend_stats_tile_scratch(void)
{
    return frontend_frame_scratch;
}

static const char *frontend_stats_text(u8 index, const char *fallback)
{
    if (
        frontend_text_ready &&
        index < 68 &&
        frontend_text.misc_text[index][0] != '\0'
    ) {
        return frontend_text.misc_text[index];
    }
    return fallback;
}

static u16 frontend_stats_source_glyph(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return (u16)(character - 'A');
    }
    if (character >= '1' && character <= '9') {
        return (u16)(69 + character - '0');
    }
    switch (character) {
    case '0': return 79;
    case '!': return 26;
    case '?': return 27;
    case '.': return 28;
    case ',': return 29;
    case ':': return 31;
    case '\'': return 32;
    case '%': return 62;
    case '-': return 83;
    case '/': return 80;
    default: return 27;
    }
}

static void frontend_stats_write_4bpp(
    u8 *tiles,
    u8 tile_columns,
    u8 x,
    u8 y,
    u8 colour
)
{
    u32 offset =
        (
            (u32)(y >> 3) * tile_columns +
            (u32)(x >> 3)
        ) * 32u +
        (u32)(y & 7u) * 4u +
        (u32)(x & 7u) / 2u;
    u8 shift = (x & 1u) != 0 ? 4 : 0;

    tiles[offset] = (u8)(
        (tiles[offset] & (u8)(0xf0u >> shift)) |
        (colour << shift)
    );
}

static u8 frontend_stats_decode_glyph(
    u8 slot,
    const OtShpSprite *sprite
)
{
    u8 pixels[16 * 16];
    u8 *target = frontend_stats_tile_scratch() +
        (u32)slot * FRONTEND_STATS_FONT_TILES_PER_GLYPH * 32u;
    u32 source = 0;
    u16 x = 0;
    u16 y = 0;

    if (
        sprite == 0 ||
        !sprite->populated ||
        sprite->width == 0 ||
        sprite->height == 0 ||
        sprite->width > 14 ||
        sprite->height > 14
    ) {
        return 0;
    }
    memset(pixels, 0, sizeof(pixels));
    while (source < sprite->encoded.size && y < sprite->height) {
        u8 code = sprite->encoded.data[source++];

        if (code == 255) {
            if (source >= sprite->encoded.size) return 0;
            x = (u16)(x + sprite->encoded.data[source++]);
        } else if (code == 254) {
            x = sprite->width;
        } else if (code == 253) {
            x++;
        } else {
            u8 raw = code & 0x0fu;

            if (x < sprite->width && raw >= 2) {
                u8 normalized = (u8)(
                    raw + FRONTEND_STATS_FONT_BRIGHTNESS_BIAS
                );

                if (normalized > 15) normalized = 15;
                /*
                 * Store the SMALL_FONT-equivalent brightness as 1..13; the
                 * two OBJ palettes apply JE_outTextGlow's current/final
                 * brightness at presentation.
                 */
                pixels[(u16)(y + 1u) * 16u + x + 1u] =
                    (u8)(normalized - 2u);
            }
            x++;
        }
        if (x >= sprite->width) {
            x = 0;
            y++;
        }
    }
    if (y < sprite->height) return 0;

    /*
     * JE_outTextGlow first draws four hue-0 copies at x/y +/-1.  One dark
     * palette entry is sufficient for that outline; the centre pixels keep
     * all thirteen source brightness values exactly.
     */
    for (y = 1; y <= sprite->height; y++) {
        for (x = 1; x <= sprite->width; x++) {
            if (
                pixels[y * 16u + x] == 0 ||
                pixels[y * 16u + x] == 14
            ) {
                continue;
            }
            if (pixels[y * 16u + x - 1u] == 0) {
                pixels[y * 16u + x - 1u] = 14;
            }
            if (pixels[y * 16u + x + 1u] == 0) {
                pixels[y * 16u + x + 1u] = 14;
            }
            if (pixels[(y - 1u) * 16u + x] == 0) {
                pixels[(y - 1u) * 16u + x] = 14;
            }
            if (pixels[(y + 1u) * 16u + x] == 0) {
                pixels[(y + 1u) * 16u + x] = 14;
            }
        }
    }
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            u8 colour = pixels[y * 16u + x];

            if (colour != 0) {
                frontend_stats_write_4bpp(
                    target,
                    2,
                    (u8)x,
                    (u8)y,
                    colour
                );
            }
        }
    }
    frontend_stats_glyph_width[slot] = (u8)sprite->width;
    return 1;
}

static u8 frontend_stats_adjust_brightness(u8 raw, s8 brightness)
{
    s16 value = (s16)raw + brightness;

    if (value < 0) return 0;
    if (value > 15) return value >= 31 ? 0 : 15;
    return (u8)value;
}

static void frontend_stats_fill_palette(u16 *target, s8 brightness)
{
    OtDataView palette;
    u8 raw;

    memset(target, 0, 16u * sizeof(u16));
    if (!ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        return;
    }
    for (raw = 3; raw <= 15; raw++) {
        u8 value = frontend_stats_adjust_brightness(
            raw,
            brightness
        );
        u8 source_index = (u8)(0xf0u | value);

        /*
         * JE_endLevelAni calls set_colors(white, 254, 254) before any glow.
         */
        target[raw - 2u] =
            source_index == 254 ?
                0x7fffu :
                title_palette_colour(&palette, source_index);
    }
    target[14] = title_palette_colour(&palette, 3);
    target[15] = 0x7fffu;
}

static void frontend_stats_refresh_palettes(void)
{
    OtDataView palette;
    u8 value;

    memset(frontend_stats_obj_palette, 0, 16u * sizeof(u16));
    if (ot_data_palette_view(SOURCE_LEVEL_PALETTE_INDEX, &palette)) {
        /*
         * JE_drawCube uses hue 9.  OBJ colour zero remains transparent;
         * entry 1 doubles as the two dark shadow passes and the visually
         * indistinguishable lowest foreground value in the 4bpp adapter.
         */
        frontend_stats_obj_palette[1] =
            title_palette_colour(&palette, 0);
        for (value = 2; value <= 15; value++) {
            frontend_stats_obj_palette[value] =
                title_palette_colour(
                    &palette,
                    (u8)(0x90u | value)
                );
        }
    }
    frontend_stats_fill_palette(
        frontend_stats_obj_palette + 16,
        frontend_stats_glow_brightness
    );
    frontend_stats_fill_palette(
        frontend_stats_obj_palette + 32,
        -4
    );
    frontend_stats_palette_dirty = 1;
}

static void frontend_stats_restart_glow(void)
{
    frontend_stats_glow_brightness = 2;
    frontend_stats_refresh_palettes();
}

static void frontend_stats_advance_glow(void)
{
    if (
        frontend_stats_timer % 5u == 0 &&
        frontend_stats_glow_brightness > -4
    ) {
        frontend_stats_glow_brightness--;
        frontend_stats_refresh_palettes();
    }
}

static u8 frontend_stats_prepare_tiles(void)
{
    u8 *tiles = frontend_stats_tile_scratch();
    u8 slot;
    u8 *cube = tiles +
        (
            FRONTEND_STATS_CUBE_TILE_BASE -
            FRONTEND_STATS_TILE_BASE
        ) * 32u;
    u8 y;

    memset(tiles, 0, FRONTEND_STATS_TILE_BYTES);
    memset(
        frontend_stats_glyph_width,
        0,
        sizeof(frontend_stats_glyph_width)
    );
    for (
        slot = 0;
        slot < FRONTEND_STATS_FONT_GLYPH_COUNT;
        slot++
    ) {
        OtShpSprite sprite;

        if (
            !ot_data_shp_sprite_read(
                FRONTEND_STATS_SOURCE_FONT_TABLE,
                frontend_stats_source_glyph(
                    frontend_runtime_font_characters[slot]
                ),
                &sprite
            ) ||
            !frontend_stats_decode_glyph(slot, &sprite)
        ) {
            frontend_stats_font_ready = 0;
            return 0;
        }
    }
    for (y = 0; y < FRONTEND_CUBE_HEIGHT; y++) {
        u8 x;

        for (x = 0; x < FRONTEND_CUBE_WIDTH; x++) {
            u8 pixel = frontend_cube[
                (u16)y * FRONTEND_CUBE_WIDTH + x
            ];

            if (pixel != 0xff) {
                u8 colour = pixel & 0x0fu;

                if (colour == 0) colour = 1;
                frontend_stats_write_4bpp(
                    cube,
                    4,
                    x,
                    y,
                    colour
                );
            }
        }
    }
    frontend_stats_restart_glow();
    frontend_stats_tiles_pending = 1;
    frontend_stats_font_ready = 1;
    return 1;
}

static void frontend_stats_put_sprite(
    s16 x,
    s16 y,
    u16 tile,
    u8 palette,
    u16 size,
    s16 dimension
)
{
    OBJATTR *object;

    if (oam_count >= HARDWARE_OAM_ENTRIES) return;
    if (
        x <= -dimension || x >= SCREEN_WIDTH ||
        y <= -dimension || y >= SCREEN_HEIGHT
    ) {
        return;
    }
    object = &oam_shadow[oam_count++];
    object->attr0 = (y & 0x00ff) | ATTR0_NORMAL |
                    ATTR0_COLOR_16 | ATTR0_SQUARE;
    object->attr1 = (x & 0x01ff) | size;
    object->attr2 = OBJ_CHAR(tile) | ATTR2_PRIORITY(0) |
                    ATTR2_PALETTE(palette);
    object->dummy = 0;
}

static s16 frontend_stats_draw_text(
    s16 x,
    s16 y,
    const char *text,
    u8 palette
)
{
    while (*text != '\0') {
        u8 tile = frontend_runtime_font_tile(*text++);

        if (tile == 0) {
            x = (s16)(x + 6);
            continue;
        }
        tile--;
        frontend_stats_put_sprite(
            (s16)(x - 1),
            (s16)(y - 1),
            (u16)(
                FRONTEND_STATS_TILE_BASE +
                (u16)tile * FRONTEND_STATS_FONT_TILES_PER_GLYPH
            ),
            palette,
            ATTR1_SIZE_16,
            16
        );
        x = (s16)(x + frontend_stats_glyph_width[tile] + 1);
    }
    return x;
}

static s16 frontend_stats_draw_u32(
    s16 x,
    s16 y,
    u32 value,
    u8 palette
)
{
    char digit[10];
    u8 count = 0;

    do {
        digit[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(digit));
    while (count != 0) {
        char text[2] = {digit[--count], '\0'};

        x = frontend_stats_draw_text(x, y, text, palette);
    }
    return x;
}

static void frontend_render_stats(void)
{
    u32 killed_percent =
        source_parity_level.total_enemy == 0 ?
            0 :
            (
                source_parity_level.enemy_kill_count * 100u +
                source_parity_level.total_enemy / 2u
            ) /
            source_parity_level.total_enemy;
    s16 x;
    u8 old_oam_count = previous_oam_count;
    u8 settled_palette = 15;
    u8 active_palette = 14;
    u8 palette;

    if (!frontend_stats_font_ready) return;
    oam_count = frontend_stats_scene_oam_count;
    if (source_parity_level.bonus_level) {
        frontend_stats_draw_text(
            20,
            source_world_to_screen_y(20),
            frontend_stats_text(16, "BONUS LEVEL"),
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
    } else {
        x = frontend_stats_draw_text(
            20,
            source_world_to_screen_y(20),
            frontend_stats_text(
                frontend_level_completed ? 26 : 61,
                frontend_level_completed ? "COMPLETED" : "EXITING"
            ),
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
        x = (s16)(x + 6);
        frontend_stats_draw_text(
            x,
            source_world_to_screen_y(20),
            frontend_level.level_name,
            frontend_stats_stage == FRONTEND_STATS_STAGE_COMPLETED ?
                active_palette :
                settled_palette
        );
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_CASH) {
        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_CASH ?
                active_palette :
                settled_palette;
        x = frontend_stats_draw_text(
            30,
            source_world_to_screen_y(50),
            frontend_stats_text(27, "CASH"),
            palette
        );
        frontend_stats_draw_u32(
            (s16)(x + 6),
            source_world_to_screen_y(50),
            player_cash,
            palette
        );
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_ENEMIES) {
        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_ENEMIES ?
                active_palette :
                settled_palette;
        x = frontend_stats_draw_text(
            40,
            source_world_to_screen_y(90),
            frontend_stats_text(62, "ENEMIES DESTROYED"),
            palette
        );
        x = frontend_stats_draw_u32(
            (s16)(x + 6),
            source_world_to_screen_y(90),
            killed_percent,
            palette
        );
        frontend_stats_draw_text(
            x,
            source_world_to_screen_y(90),
            "%",
            palette
        );
    }
    if (
        frontend_play_mode != FRONTEND_PLAY_ARCADE &&
        frontend_stats_stage >= FRONTEND_STATS_STAGE_CUBES
    ) {
        u8 cube_total = frontend_stats_cube_total();
        u8 cube_count = frontend_stats_cube_visible_count;
        u8 cube_index;

        palette =
            frontend_stats_stage == FRONTEND_STATS_STAGE_CUBES ?
                active_palette :
                settled_palette;
        frontend_stats_draw_text(
            30,
            source_world_to_screen_y(120),
            frontend_stats_text(3, "CUBES"),
            palette
        );
        if (cube_count > cube_total) cube_count = cube_total;
        for (cube_index = 0; cube_index < cube_count; cube_index++) {
            frontend_stats_put_sprite(
                (s16)(50 + (u16)cube_index * 30u),
                source_world_to_screen_y(135),
                FRONTEND_STATS_CUBE_TILE_BASE,
                13,
                ATTR1_SIZE_32,
                32
            );
        }
        if (
            cube_total == 0 &&
            frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL
        ) {
            frontend_stats_draw_text(
                50,
                source_world_to_screen_y(135),
                frontend_stats_text(14, "NONE"),
                settled_palette
            );
        }
    }
    if (frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL) {
        frontend_stats_draw_text(
            90,
            source_world_to_screen_y(160),
            frontend_stats_text(4, "PRESS A KEY"),
            frontend_stats_timer == 0 ?
                settled_palette :
                active_palette
        );
    }
    {
        u8 new_oam_count = oam_count;

        while (oam_count < old_oam_count) {
            oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
            oam_count++;
        }
        previous_oam_count = new_oam_count;
        oam_count = new_oam_count;
    }
    oam_dirty = 1;
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
        return FRONTEND_FRAME_NEXT_LEVEL_0;
    case STATE_GAME_OVER:
        return FRONTEND_FRAME_GAME_OVER;
    default:
        return FRONTEND_FRAME_COUNT;
    }
}

static void frontend_redraw(void)
{
    u8 frame;

    if (
        game_state == STATE_TITLE ||
        game_state == STATE_PLAY_MODE_MENU ||
        game_state == STATE_EPISODE_MENU ||
        game_state == STATE_DIFFICULTY_MENU
    ) {
        frontend_render_pre_game_menu();
        return;
    }
    if (game_state == STATE_LEVEL_STATS) {
        frontend_render_stats();
        return;
    }
    if (game_state == STATE_GAME_MENU) {
        frontend_render_game_menu();
        return;
    }
    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level();
        return;
    }
    if (game_state == STATE_UPGRADE_MENU) {
        frontend_render_upgrade_menu();
        return;
    }
    if (game_state == STATE_UPGRADE_SUBMENU) {
        frontend_render_upgrade_submenu();
        return;
    }
    if (game_state == STATE_QUIT_CONFIRM) {
        frontend_render_quit_confirm();
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
        game_state == STATE_TITLE ||
        game_state == STATE_PLAY_MODE_MENU ||
        game_state == STATE_EPISODE_MENU ||
        game_state == STATE_DIFFICULTY_MENU
    ) {
        frontend_render_pre_game_selection(old_selection);
        return;
    }
    if (game_state == STATE_NEXT_LEVEL_MENU) {
        frontend_render_next_level_selection(old_selection);
        return;
    }
    if (game_state == STATE_GAME_MENU) {
        frontend_render_game_menu_selection(old_selection);
        return;
    }
    if (game_state == STATE_UPGRADE_MENU) {
        frontend_render_upgrade_menu_selection(old_selection);
        return;
    }
    if (
        game_state == STATE_UPGRADE_SUBMENU ||
        game_state == STATE_QUIT_CONFIRM
    ) {
        frontend_redraw();
        return;
    }
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
    u8 old_state = game_state;

    if (
        old_state == STATE_NEXT_LEVEL_MENU &&
        state != STATE_NEXT_LEVEL_MENU
    ) {
        frontend_nav_obj_disable();
    }
    if (state != STATE_LEVEL_STATS) {
        frontend_stats_overlay_active = 0;
        frontend_stats_tiles_pending = 0;
        frontend_stats_palette_dirty = 0;
        frontend_stats_font_ready = 0;
    }
    game_state = state;
    frontend_selection = selection;
    if (!frontend_render_shared_menu_transition(old_state, state)) {
        frontend_redraw();
    }
    telemetry_state_transitions++;
}

static void frontend_demo_close(void)
{
    if (frontend_demo_file != 0) {
        ot_fclose(frontend_demo_file);
        frontend_demo_file = 0;
    }
    frontend_demo_active = 0;
    frontend_demo_eof = 0;
    frontend_demo_keys = 0;
    frontend_demo_keys_wait = 0;
    frontend_demo_input_guard = 0;
    insert_coin_tile_upload_pending = 0;
}

static void enter_title(void)
{
    frontend_demo_close();
    frontend_player_items_valid = 0;
    player_cash = 0;
    frontend_title_idle_frames = 0;
    frontend_enter_state(STATE_TITLE, 0);
    audio_load_source_song(29);
}

static void frontend_demo_finish(u8 aborted)
{
    if (!frontend_demo_active) return;
    if (aborted) {
        telemetry_demo_aborts++;
    } else {
        telemetry_demo_finishes++;
    }
    frontend_demo_close();
    enter_title();
}

static u8 frontend_demo_read_header(void)
{
    u8 wait_bytes[2];
    u8 unused[3];
    u8 one_based_song;
    char filename[7] = "demo.1";

    if (++frontend_demo_number > FRONTEND_DEMO_COUNT) {
        frontend_demo_number = 1;
    }
    filename[5] = (char)('0' + frontend_demo_number);
    frontend_demo_file = ot_dir_fopen(
        ot_data_dir(),
        filename,
        "rb"
    );
    if (frontend_demo_file == 0) return 0;
    memset(&frontend_demo_header, 0, sizeof(frontend_demo_header));
    if (
        !ot_fread_u8_exact(
            &frontend_demo_header.episode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            (u8 *)frontend_demo_header.level_name,
            10,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.lvl_file_number,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.front_weapon,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.rear_weapon,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.super_arcade_mode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            frontend_demo_header.sidekick,
            2,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.generator,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.sidekick_level,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.sidekick_series,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.initial_episode,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.shield,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.special,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.ship,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.front_power,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(
            &frontend_demo_header.rear_power,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(unused, 3, frontend_demo_file) ||
        !ot_fread_u8_exact(
            &one_based_song,
            1,
            frontend_demo_file
        ) ||
        !ot_fread_u8_exact(wait_bytes, 2, frontend_demo_file)
    ) {
        return 0;
    }
    frontend_demo_header.level_name[10] = '\0';
    if (
        frontend_demo_header.episode == 0 ||
        frontend_demo_header.episode > OT_EPISODE_COUNT ||
        frontend_demo_header.lvl_file_number == 0 ||
        one_based_song == 0 ||
        one_based_song > JUKEBOX_MUSIC_COUNT ||
        frontend_demo_header.front_weapon >= OT_HDT_PORT_COUNT ||
        frontend_demo_header.rear_weapon >= OT_HDT_PORT_COUNT ||
        frontend_demo_header.generator >= OT_HDT_POWER_COUNT ||
        frontend_demo_header.sidekick[0] >= OT_HDT_OPTION_COUNT ||
        frontend_demo_header.sidekick[1] >= OT_HDT_OPTION_COUNT ||
        frontend_demo_header.shield >= OT_HDT_SHIELD_COUNT ||
        frontend_demo_header.special >= OT_HDT_SPECIAL_COUNT ||
        frontend_demo_header.ship >= OT_HDT_SHIP_COUNT ||
        frontend_demo_header.front_power == 0 ||
        frontend_demo_header.front_power > 11 ||
        frontend_demo_header.rear_power == 0 ||
        frontend_demo_header.rear_power > 11
    ) {
        return 0;
    }
    /*
     * demo.? stores OpenTyrian's levelSong value.  It is one-based and the
     * PC path deliberately calls play_song(levelSong - 1).  Keep every
     * FrontendDemoHeader consumer zero-based, like OtEpisodeLevel.
     */
    frontend_demo_header.source_song = (u8)(one_based_song - 1u);
    frontend_demo_keys = 0;
    frontend_demo_keys_wait =
        (u16)(((u16)wait_bytes[0] << 8) | wait_bytes[1]);
    return 1;
}

static u8 frontend_demo_start(u8 idle_start)
{
    OtEpisodeLevel demo_level;

    frontend_demo_close();
    if (!frontend_demo_read_header()) {
        telemetry_demo_parse_failures++;
        frontend_demo_close();
        enter_title();
        return 0;
    }
    memset(&demo_level, 0, sizeof(demo_level));
    demo_level.episode = frontend_demo_header.episode;
    demo_level.lvl_file_number =
        frontend_demo_header.lvl_file_number;
    demo_level.source_song = frontend_demo_header.source_song;
    memcpy(
        demo_level.level_name,
        frontend_demo_header.level_name,
        sizeof(demo_level.level_name)
    );
    demo_level.level_name[
        sizeof(demo_level.level_name) - 1u
    ] = '\0';
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = (u8)(demo_level.episode - 1u);
    frontend_difficulty = FRONTEND_DIFFICULTY_NORMAL;
    frontend_main_section = 0;
    frontend_map = (OtEpisodeMap){0};
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_demo_active = 1;
    frontend_demo_eof = 0;
    frontend_demo_input_guard = 12;
    if (!enter_selected_level(&demo_level)) {
        telemetry_demo_parse_failures++;
        frontend_demo_close();
        enter_title();
        return 0;
    }
    telemetry_demo_starts++;
    if (idle_start) telemetry_demo_idle_starts++;
    return 1;
}

static u8 frontend_demo_apply_input(void)
{
    u8 wait_bytes[2];

    if (!frontend_demo_active || frontend_demo_file == 0) return 1;
    while (frontend_demo_keys_wait == 0) {
        if (
            !ot_fread_u8_exact(
                &frontend_demo_keys,
                1,
                frontend_demo_file
            ) ||
            !ot_fread_u8_exact(
                wait_bytes,
                2,
                frontend_demo_file
            )
        ) {
            frontend_demo_eof = 1;
            frontend_demo_keys = 0;
            source_parity_level.end_level = true;
            source_parity_level.level_end = 40;
            source_begin_end_level_audio();
            break;
        }
        frontend_demo_keys_wait =
            (u16)(((u16)wait_bytes[0] << 8) | wait_bytes[1]);
    }
    if (frontend_demo_keys_wait > 0) {
        frontend_demo_keys_wait--;
    }
    pad_now = 0;
    if (frontend_demo_keys & (1u << 0)) pad_now |= KEY_UP;
    if (frontend_demo_keys & (1u << 1)) pad_now |= KEY_DOWN;
    if (frontend_demo_keys & (1u << 2)) pad_now |= KEY_LEFT;
    if (frontend_demo_keys & (1u << 3)) pad_now |= KEY_RIGHT;
    if (frontend_demo_keys & (1u << 4)) pad_now |= KEY_A;
    if (frontend_demo_keys & (1u << 5)) pad_now |= KEY_L;
    if (frontend_demo_keys & (1u << 6)) pad_now |= KEY_B;
    if (frontend_demo_keys & (1u << 7)) pad_now |= KEY_R;
    return 1;
}

static void frontend_advance_episode(void)
{
    if (frontend_episode + 1u >= OT_EPISODE_COUNT) {
        enter_title();
        return;
    }
    frontend_episode++;
    frontend_main_section = 1;
    frontend_map = (OtEpisodeMap){0};
    frontend_level = (OtEpisodeLevel){0};
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    if (!frontend_prepare_map()) {
        enter_title();
        return;
    }
    frontend_enter_state(STATE_GAME_MENU, 4);
    frontend_load_game_menu_song();
}

static void frontend_begin(void)
{
    frontend_play_mode = FRONTEND_PLAY_FULL_GAME;
    frontend_episode = 0;
    frontend_difficulty = FRONTEND_DIFFICULTY_NORMAL;
    frontend_main_section = 1;
    frontend_map = (OtEpisodeMap){0};
    memset(
        frontend_map_level,
        0,
        sizeof(frontend_map_level)
    );
    frontend_level = (OtEpisodeLevel){0};
    frontend_text_ready =
        ot_data_frontend_text_load(&frontend_text);
    frontend_map_ready = 0;
    frontend_level_ready = 0;
    frontend_level_completed = 0;
    frontend_player_items_valid = 0;
    memset(
        &frontend_player_items,
        0,
        sizeof(frontend_player_items)
    );
    player_cash = 0;
    frontend_demo_file = 0;
    frontend_demo_keys_wait = 0;
    frontend_title_idle_frames = 0;
    frontend_demo_keys = 0;
    frontend_demo_number = 0;
    frontend_demo_active = 0;
    frontend_demo_eof = 0;
    frontend_demo_input_guard = 0;
    frontend_timer = 200;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = 0;
    frontend_stats_overlay_active = 0;
    frontend_stats_tiles_pending = 0;
    frontend_stats_palette_dirty = 0;
    frontend_stats_font_ready = 0;
    frontend_stats_scene_oam_count = 0;
    frontend_stats_glow_brightness = -4;
    frontend_nav_obj_active = 0;
    frontend_nav_obj_slot_count = 0;
    frontend_nav_obj_upload_count = 0;
    frontend_nav_obj_palette_pending = 0;
    frontend_nav_obj_dot_tiles_pending = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_state = STATE_INTRO_LOGO_1;
    frontend_selection = 0;
    mmStop();
    frontend_redraw();
    telemetry_state_transitions++;
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    autotest_campaign_levels_completed = 0;
    autotest_campaign_failures = 0;
    autotest_campaign_checksum = 2166136261u;
    memset(
        autotest_campaign_record,
        0,
        sizeof(autotest_campaign_record)
    );
#endif
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

    if (game_state == STATE_TITLE) {
        if (pad_now != 0 || pad_pressed != 0) {
            frontend_title_idle_frames = 0;
        } else if (
            ++frontend_title_idle_frames >=
                FRONTEND_DEMO_IDLE_VBLANKS
        ) {
            frontend_title_idle_frames = 0;
            (void)frontend_demo_start(1);
            return;
        }
    }

    if (game_state == STATE_NEXT_LEVEL_MENU) {
        s16 delta_x = (s16)(
            frontend_nav_target_x - frontend_nav_x
        );
        s16 delta_y = (s16)(
            frontend_nav_target_y - frontend_nav_y
        );
        u8 background_redraw = 0;
        u8 planet_redraw = 0;
        u8 dot_redraw = 0;

        if (delta_x >= -1 && delta_x <= 1) {
            frontend_nav_x = frontend_nav_target_x;
        } else {
            frontend_nav_x = (s16)(
                frontend_nav_x + delta_x / 2
            );
        }
        if (delta_y >= -1 && delta_y <= 1) {
            frontend_nav_y = frontend_nav_target_y;
        } else {
            frontend_nav_y = (s16)(
                frontend_nav_y + delta_y / 2
            );
        }
        if (++frontend_nav_animation_wait >= 4) {
            frontend_nav_animation_wait = 0;
            frontend_nav_planet_animation =
                frontend_nav_planet_animation >= 14 ?
                    0 :
                    (u8)(frontend_nav_planet_animation + 1u);
            planet_redraw = 1;
        }
        if (++frontend_nav_dot_wait >= 6) {
            u8 choice_count = frontend_next_choice_count();

            frontend_nav_dot_wait = 0;
            if (frontend_selection < choice_count) {
                u8 destination = frontend_map.map_planet[
                    frontend_next_map_choice(frontend_selection)
                ];
                u8 dot_count =
                    frontend_nav_dot_count(destination);

                if (frontend_nav_dot_animation < dot_count) {
                    frontend_nav_dot_animation++;
                }
            }
            dot_redraw = 1;
        }
        if (
            (
                frontend_nav_presented_x != frontend_nav_x ||
                frontend_nav_presented_y != frontend_nav_y
            ) &&
            frontend_nav_animation_wait == 0
        ) {
            background_redraw = 1;
        }
        if (
            background_redraw &&
            frontend_pending_kind != FRONTEND_PENDING_FULL_FRAME
        ) {
            frontend_render_next_level_nav();
        } else if (planet_redraw) {
            frontend_nav_obj_animate();
        } else if (dot_redraw) {
            frontend_nav_obj_render_oam();
        }
    }

    if (game_state == STATE_LEVEL_STATS) {
        if (
            level_exit_music_started &&
            telemetry_end_level_music_natural_stops == 0 &&
            !mmActive()
        ) {
            telemetry_end_level_music_natural_stops = 1;
        }
        if (pad_pressed != 0) {
            if (frontend_stats_stage < FRONTEND_STATS_STAGE_FINAL) {
                telemetry_stats_stage_advances +=
                    FRONTEND_STATS_STAGE_FINAL -
                        frontend_stats_stage;
                frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
                if (
                    frontend_play_mode != FRONTEND_PLAY_ARCADE
                ) {
                    u8 cube_total = frontend_stats_cube_total();

                    while (
                        frontend_stats_cube_visible_count <
                        cube_total
                    ) {
                        frontend_stats_cube_visible_count++;
                        telemetry_stats_cube_reveals++;
                        audio_effect(SFX_ITEM);
                    }
                }
                frontend_stats_timer = 0;
                frontend_redraw();
            } else {
                if (
                    frontend_level_completed &&
                    source_parity_level.bonus_level &&
                    source_parity_level.next_level != 0
                ) {
                    frontend_main_section =
                        source_parity_level.next_level;
                } else if (
                    frontend_level_completed &&
                    frontend_level_ready &&
                    frontend_level.next_section != 0
                ) {
                    frontend_main_section = frontend_level.next_section;
                }
                frontend_prepare_map();
                frontend_enter_state(STATE_GAME_MENU, 4);
                frontend_load_game_menu_song();
#ifdef AUTOTEST
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
                if (
                    autotest_campaign_levels_completed >=
                        AUTOTEST_CAMPAIGN_LEVEL_COUNT
                ) {
                    autotest_frontend_finish_pending = 1;
                }
#else
                autotest_frontend_finish_pending = 1;
#endif
#endif
            }
            return;
        }
        if (frontend_stats_timer > 0) {
            frontend_stats_timer--;
            frontend_stats_advance_glow();
            if (
                frontend_stats_timer == 0 &&
                frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL
            ) {
                frontend_render_stats();
            }
            return;
        }
        if (frontend_stats_stage >= FRONTEND_STATS_STAGE_FINAL) {
            return;
        }
        if (frontend_stats_stage == FRONTEND_STATS_STAGE_CUBES) {
            u8 cube_total = frontend_stats_cube_total();

            if (
                frontend_play_mode != FRONTEND_PLAY_ARCADE &&
                frontend_stats_cube_visible_count < cube_total
            ) {
                frontend_stats_cube_visible_count++;
                frontend_stats_timer = FRONTEND_STATS_CUBE_TICKS;
                telemetry_stats_cube_reveals++;
                audio_effect(SFX_ITEM);
                frontend_redraw();
                return;
            }
            frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
        } else if (
            frontend_stats_stage == FRONTEND_STATS_STAGE_ENEMIES &&
            frontend_play_mode == FRONTEND_PLAY_ARCADE
        ) {
            frontend_stats_stage = FRONTEND_STATS_STAGE_FINAL;
        } else {
            frontend_stats_stage++;
        }
        frontend_stats_timer = FRONTEND_STATS_STAGE_TICKS;
        frontend_stats_restart_glow();
        telemetry_stats_stage_advances++;
        frontend_redraw();
        return;
    }
    if (pad_pressed & KEY_UP) {
        switch (game_state) {
        case STATE_TITLE:
            frontend_move_selection(3, -1);
            break;
        case STATE_PLAY_MODE_MENU:
            frontend_move_selection(2, -1);
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(
                (u8)(frontend_next_choice_count() + 1u),
                -1
            );
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
        case STATE_UPGRADE_MENU:
            frontend_move_selection(
                FRONTEND_UPGRADE_MENU_ITEM_COUNT,
                -1
            );
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_move_submenu(-1);
            break;
        default:
            break;
        }
        return;
    }
    if (pad_pressed & KEY_DOWN) {
        switch (game_state) {
        case STATE_TITLE:
            frontend_move_selection(3, 1);
            break;
        case STATE_PLAY_MODE_MENU:
            frontend_move_selection(2, 1);
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_move_selection(
                (u8)(frontend_next_choice_count() + 1u),
                1
            );
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
        case STATE_UPGRADE_MENU:
            frontend_move_selection(
                FRONTEND_UPGRADE_MENU_ITEM_COUNT,
                1
            );
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_move_submenu(1);
            break;
        default:
            break;
        }
        return;
    }

    if (pad_pressed & (KEY_LEFT | KEY_RIGHT)) {
        if (game_state == STATE_UPGRADE_SUBMENU) {
            frontend_upgrade_power_adjust(
                (pad_pressed & KEY_LEFT) != 0 ? -1 : 1
            );
            return;
        }
        if (game_state == STATE_QUIT_CONFIRM) {
            frontend_quit_yes ^= 1u;
            frontend_selection = frontend_quit_yes ? 0 : 1;
            audio_effect(SFX_CURSOR);
            frontend_render_quit_selection();
            return;
        }
    }
    if (
        (pad_pressed & KEY_R) != 0 &&
        game_state == STATE_UPGRADE_SUBMENU &&
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON &&
        frontend_selection < frontend_upgrade_sub_count
    ) {
        OtWeaponPortDefinition port;
        u8 rear = frontend_player_items.weapon[
            FRONTEND_WEAPON_REAR
        ].id;

        if (
            rear != 0 &&
            ot_data_hdt_weapon_port_read(rear, &port) &&
            port.opnum != 0
        ) {
            frontend_player_items.weapon_mode++;
            if (frontend_player_items.weapon_mode > port.opnum) {
                frontend_player_items.weapon_mode = 1;
            }
            audio_effect(SFX_CURSOR);
            frontend_render_upgrade_submenu_update(
                frontend_selection,
                frontend_upgrade_sub_scroll
            );
        } else {
            audio_effect(SFX_CLINK);
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
            frontend_quit_yes = 1;
            frontend_enter_state(STATE_QUIT_CONFIRM, 0);
            break;
        case STATE_NEXT_LEVEL_MENU:
            frontend_enter_state(STATE_GAME_MENU, 4);
            break;
        case STATE_UPGRADE_MENU:
            frontend_enter_state(STATE_GAME_MENU, 2);
            break;
        case STATE_UPGRADE_SUBMENU:
            frontend_upgrade_cancel_submenu();
            break;
        case STATE_QUIT_CONFIRM:
            frontend_quit_yes = 0;
            frontend_enter_state(STATE_GAME_MENU, 5);
            audio_effect(SFX_CLICK);
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
        } else if (frontend_selection == 1) {
            (void)frontend_demo_start(0);
        } else {
            jukebox_enter();
        }
        break;
    case STATE_PLAY_MODE_MENU:
        frontend_play_mode = frontend_selection;
        frontend_enter_state(STATE_EPISODE_MENU, 0);
        break;
    case STATE_EPISODE_MENU:
        frontend_episode = frontend_selection;
        frontend_main_section = 1;
        frontend_map = (OtEpisodeMap){0};
        frontend_level = (OtEpisodeLevel){0};
        frontend_map_ready = 0;
        frontend_level_ready = 0;
        frontend_enter_state(STATE_DIFFICULTY_MENU, 1);
        break;
    case STATE_DIFFICULTY_MENU:
        frontend_difficulty =
            (u8)(FRONTEND_DIFFICULTY_EASY + frontend_selection);
        frontend_campaign_initialize();
        frontend_prepare_map();
        frontend_enter_state(STATE_GAME_MENU, 4);
        frontend_load_game_menu_song();
        break;
    case STATE_GAME_MENU:
        if (frontend_selection == 2) {
            frontend_enter_state(STATE_UPGRADE_MENU, 0);
            audio_effect(SFX_CLICK);
        } else if (frontend_selection == 4) {
            if (
                !frontend_map_ready &&
                !frontend_prepare_map()
            ) {
                frontend_load_game_menu_song();
            } else if (frontend_map.episode_complete) {
                frontend_advance_episode();
            } else {
                frontend_nav_reset();
                frontend_enter_state(STATE_NEXT_LEVEL_MENU, 0);
            }
        } else if (frontend_selection == 5) {
            frontend_quit_yes = 1;
            frontend_enter_state(STATE_QUIT_CONFIRM, 0);
        }
        /* Data Cubes, Ship Specs and Options remain visible but disabled. */
        break;
    case STATE_UPGRADE_MENU:
        if (frontend_selection == FRONTEND_UPGRADE_DONE) {
            frontend_enter_state(STATE_GAME_MENU, 2);
            audio_effect(SFX_CLICK);
        } else if (
            frontend_upgrade_begin_submenu(frontend_selection)
        ) {
            frontend_enter_state(
                STATE_UPGRADE_SUBMENU,
                frontend_selection
            );
            frontend_upgrade_apply_preview();
            audio_effect(SFX_CLICK);
        } else {
            audio_effect(SFX_CLINK);
        }
        break;
    case STATE_UPGRADE_SUBMENU:
        if (frontend_selection < frontend_upgrade_sub_count) {
            u8 old_selection = frontend_selection;
            u8 old_scroll = frontend_upgrade_sub_scroll;

            frontend_selection = frontend_upgrade_sub_count;
            if (
                frontend_selection >=
                    frontend_upgrade_sub_scroll + 6u
            ) {
                frontend_upgrade_sub_scroll =
                    (u8)(frontend_selection - 5u);
            }
            audio_effect(SFX_CLICK);
            frontend_render_upgrade_submenu_update(
                old_selection,
                old_scroll
            );
        } else {
            frontend_upgrade_commit_submenu();
        }
        break;
    case STATE_QUIT_CONFIRM:
        if (frontend_quit_yes) {
            audio_effect(SFX_SPRING);
            enter_title();
        } else {
            audio_effect(SFX_CLICK);
            frontend_enter_state(STATE_GAME_MENU, 5);
        }
        break;
    case STATE_NEXT_LEVEL_MENU:
        if (frontend_selection < frontend_next_choice_count()) {
            u8 map_choice =
                frontend_next_map_choice(frontend_selection);

            if (frontend_map_level[map_choice].episode_complete) {
                frontend_advance_episode();
            } else {
                frontend_nav_obj_disable();
                if (
                    !enter_level(
                        frontend_map.map_section[map_choice]
                    )
                ) {
                    frontend_enter_state(STATE_GAME_MENU, 4);
                    frontend_load_game_menu_song();
                }
            }
        } else {
            frontend_enter_state(STATE_GAME_MENU, 4);
        }
        break;
    default:
        break;
    }
}

#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
static void frontend_autotest_record_campaign_level(u8 completed)
{
    u32 record_index = autotest_campaign_levels_completed;
    u32 prior;
    AutotestCampaignRecord *record;

    if (record_index >= AUTOTEST_CAMPAIGN_LEVEL_COUNT) {
        autotest_campaign_failures++;
        return;
    }
    record = &autotest_campaign_record[record_index];
    record->episode = frontend_level.episode;
    record->resolved_section = frontend_level.resolved_section;
    record->lvl_file_number = frontend_level.lvl_file_number;
    record->source_song = frontend_level.source_song;
    record->event_index = source_parity_level.event_index;
    record->level_position = level_position;
    record->enemy_kills = telemetry_source_enemy_kills;
    record->background_approximations =
        background_cache_approximations;

    for (prior = 0; prior < record_index; prior++) {
        if (
            autotest_campaign_record[prior].episode ==
                record->episode &&
            autotest_campaign_record[prior].resolved_section ==
                record->resolved_section
        ) {
            record->failure_flags |=
                AUTOTEST_CAMPAIGN_FAIL_DUPLICATE_ROUTE;
        }
    }
    autotest_campaign_checksum ^= record->episode;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->resolved_section;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->lvl_file_number;
    autotest_campaign_checksum *= 16777619u;
    autotest_campaign_checksum ^= record->source_song;
    autotest_campaign_checksum *= 16777619u;

    if (!completed || !frontend_level_ready) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_COMPLETION;
    }
    if (
        !source_parity_level.assets_valid ||
        source_parity_level.event_index == 0 ||
        level_position == 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_ASSETS;
    }
    if (
        source_parity_level.applied_event_count +
            source_parity_level.deferred_event_count +
            source_parity_level.skipped_event_count !=
            source_parity_level.event_index
    ) {
        record->failure_flags |=
            AUTOTEST_CAMPAIGN_FAIL_EVENT_ACCOUNTING;
    }
    if (
        source_parity_level.spawn_success_count +
            source_parity_level.spawn_pool_full_count +
            source_parity_level.spawn_missing_definition_count !=
            source_parity_level.spawn_attempt_count ||
        source_parity_level.spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_EVENT_SPAWN;
    }
    if (
        source_parity_level.enemy_launch_success_count +
            source_parity_level.enemy_launch_pool_full_count +
            source_parity_level.enemy_launch_missing_definition_count !=
            source_parity_level.enemy_launch_attempt_count ||
        source_parity_level.enemy_launch_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_LAUNCH_SPAWN;
    }
    if (
        source_parity_level.random_spawn_success_count +
            source_parity_level.random_spawn_pool_full_count +
            source_parity_level.random_spawn_missing_definition_count !=
            source_parity_level.random_spawn_attempt_count ||
        source_parity_level.random_spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_RANDOM_SPAWN;
    }
    if (
        source_parity_level.death_spawn_success_count +
            source_parity_level.death_spawn_pool_full_count +
            source_parity_level.death_spawn_missing_definition_count !=
            source_parity_level.death_spawn_attempt_count ||
        source_parity_level.death_spawn_missing_definition_count != 0
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_DEATH_SPAWN;
    }
    if (telemetry_romfs_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_ROMFS;
    }
    if (telemetry_sprite2_decode_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_SPRITE_DECODE;
    }
    if (
        telemetry_sprite2_cache_drops != 0 ||
        telemetry_sprite2_uploads !=
            telemetry_sprite2_cache_misses ||
        telemetry_sprite2_compact_uploads >
            telemetry_sprite2_uploads ||
        telemetry_sprite2_upload_bytes !=
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES
    ) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_SPRITE_CACHE;
    }
    if (telemetry_effect_cache_drops != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_EFFECT_CACHE;
    }
    if (telemetry_projectile_cache_drops != 0) {
        record->failure_flags |=
            AUTOTEST_CAMPAIGN_FAIL_PROJECTILE_CACHE;
    }
    if (telemetry_layer_rule_failures != 0) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_LAYER_RULE;
    }
    if (telemetry_end_level_music_starts != 1) {
        record->failure_flags |= AUTOTEST_CAMPAIGN_FAIL_END_MUSIC;
    }
    if (record->failure_flags != 0) autotest_campaign_failures++;
    autotest_campaign_levels_completed++;
}
#endif

static void frontend_enter_level_stats(u8 completed)
{
    u16 index;

    if (frontend_demo_active) {
        frontend_demo_finish(0);
        return;
    }
    frontend_campaign_sync_from_level();
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    frontend_autotest_record_campaign_level(completed);
#endif
    frontend_level_completed = completed;
    frontend_stats_stage = FRONTEND_STATS_STAGE_COMPLETED;
    frontend_stats_cube_visible_count = 0;
    frontend_stats_timer = FRONTEND_STATS_STAGE_TICKS;
    /*
     * JE_endLevelAni() writes onto VGAScreenSeg without clearing it.  Keep
     * the final Mode-0 gameplay frame and add only a transparent BG3 text
     * layer; switching to the generated Mode-4 stats atlas caused the
     * incorrect black background.
     */
    game_state = STATE_LEVEL_STATS;
    frontend_selection = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_stats_overlay_active = 1;
    frontend_stats_palette_dirty = 0;
    frontend_stats_scene_oam_count = previous_oam_count;
    for (index = 0; index < frontend_stats_scene_oam_count; index++) {
        oam_shadow[index].attr2 =
            (u16)((oam_shadow[index].attr2 & ~0x0c00u) | 0x0c00u);
    }
    oam_dirty = 1;
    if (frontend_stats_prepare_tiles()) {
        frontend_render_stats();
    } else {
        frontend_stats_overlay_active = 0;
    }
    telemetry_state_transitions++;
    source_sound_effect(34);
    telemetry_level_complete_voice_starts++;
}

static void frontend_enter_game_over(void)
{
    if (frontend_demo_active) {
        frontend_demo_finish(0);
        return;
    }
    /*
     * tyrian2.c leaves VGAScreen on the live gameplay frame and overlays
     * miscText[21].  Stay in Mode 0, request the OBJ glyph upload, and select
     * zero-based SONG_GAMEOVER (10) without cancelling the last explosion.
     */
    game_state = STATE_GAME_OVER;
    frontend_selection = 0;
    frontend_mode4_active = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    game_paused = 0;
    player_death_music_fade_active = 0;
    secret_level_music_active = 0;
    game_over_tile_upload_pending = 1;
    audio_load_source_song_once_preserve_effects(10);
    telemetry_game_over_music_starts++;
    telemetry_state_transitions++;
}

static void game_over_update(void)
{
    if (
        telemetry_game_over_music_starts != 0 &&
        telemetry_game_over_music_natural_stops == 0 &&
        !mmActive()
    ) {
        telemetry_game_over_music_natural_stops = 1;
    }
    if (pad_pressed == 0) return;

    frontend_enter_state(STATE_GAME_MENU, 4);
    frontend_load_game_menu_song();
    telemetry_game_over_exits++;
}
