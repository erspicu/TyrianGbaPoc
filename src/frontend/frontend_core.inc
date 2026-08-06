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
    /*
     * OpenTyrian JE_weaponViewFrame clears source (8,8)..(143,182).
     * The menu presentation is the exact 4/5 transform used by every other
     * 300x200 front-end element.  WIN0 keeps the hardware OBJ simulator
     * inside that source aperture while the Mode-4 chrome remains intact.
     */
    FRONTEND_UPGRADE_SIM_WINDOW_X0 = 6,
    FRONTEND_UPGRADE_SIM_WINDOW_X1 = 120,
    FRONTEND_UPGRADE_SIM_WINDOW_Y0 = 6,
    FRONTEND_UPGRADE_SIM_WINDOW_Y1 = 147,
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
    FRONTEND_FRAME_COUNT == 35,
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
        FRONTEND_NAV_BITMAP_BLOCK_ROWS == 2 &&
        FRONTEND_NAV_BITMAP_BLOCK_BYTES ==
            FRONTEND_NAV_BITMAP_STRIDE *
                FRONTEND_NAV_BITMAP_BLOCK_ROWS &&
        FRONTEND_NAV_BITMAP_BLOCKS_PER_PAGE *
            FRONTEND_NAV_BITMAP_BLOCK_ROWS ==
                FRONTEND_NAV_BITMAP_HEIGHT &&
        FRONTEND_NAV_BITMAP_BLOCK_DATA_BYTES ==
            FRONTEND_NAV_BITMAP_BLOCK_COUNT *
                FRONTEND_NAV_BITMAP_BLOCK_BYTES &&
        FRONTEND_NAV_BITMAP_INDEX_BYTES ==
            FRONTEND_NAV_BITMAP_PAGE_COUNT *
                FRONTEND_NAV_BITMAP_BLOCKS_PER_PAGE *
                sizeof(u16) &&
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
        FRONTEND_STATIC_OPTIONS_MENU_COUNT == 1 &&
        FRONTEND_STATIC_OPTIONS_MENU_BASE ==
            FRONTEND_STATIC_UPGRADE_MENU_BASE +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT &&
        FRONTEND_STATIC_SAVE_MENU_COUNT == 2 &&
        FRONTEND_STATIC_SAVE_MENU_BASE ==
            FRONTEND_STATIC_OPTIONS_MENU_BASE +
                FRONTEND_STATIC_OPTIONS_MENU_COUNT &&
        FRONTEND_STATIC_SAVE_NAME_MENU_COUNT == 0 &&
        FRONTEND_STATIC_SAVE_NAME_MENU_BASE ==
            FRONTEND_STATIC_SAVE_MENU_BASE +
                FRONTEND_STATIC_SAVE_MENU_COUNT &&
        FRONTEND_STATIC_MENU_PANEL_COUNT ==
            FRONTEND_STATIC_GAME_MENU_COUNT +
                FRONTEND_STATIC_UPGRADE_MENU_COUNT +
                FRONTEND_STATIC_OPTIONS_MENU_COUNT +
                FRONTEND_STATIC_SAVE_MENU_COUNT +
                FRONTEND_STATIC_SAVE_NAME_MENU_COUNT &&
        FRONTEND_STATIC_MENU_PANELS_BYTES ==
            FRONTEND_STATIC_MENU_PANEL_COUNT *
                FRONTEND_STATIC_MENU_PANEL_BYTES,
    "generated static menu panel atlas changed"
);
_Static_assert(
    FRONTEND_STATIC_PRE_GAME_TITLE_BASE == 0 &&
        FRONTEND_STATIC_PRE_GAME_TITLE_COUNT == 4 &&
        FRONTEND_STATIC_PRE_GAME_PLAY_MODE_BASE == 4 &&
        FRONTEND_STATIC_PRE_GAME_PLAY_MODE_COUNT == 2 &&
        FRONTEND_STATIC_PRE_GAME_EPISODE_BASE == 6 &&
        FRONTEND_STATIC_PRE_GAME_EPISODE_COUNT == 4 &&
        FRONTEND_STATIC_PRE_GAME_DIFFICULTY_BASE == 10 &&
        FRONTEND_STATIC_PRE_GAME_DIFFICULTY_COUNT == 3 &&
        FRONTEND_STATIC_PRE_GAME_SAVE_SLOTS_BASE == 13 &&
        FRONTEND_STATIC_PRE_GAME_SAVE_SLOTS_COUNT == 1 &&
        FRONTEND_STATIC_PRE_GAME_SAVE_NAME_BASE == 14 &&
        FRONTEND_STATIC_PRE_GAME_SAVE_NAME_COUNT == 0 &&
        FRONTEND_STATIC_PRE_GAME_FRAME_COUNT == 14 &&
        FRONTEND_STATIC_PRE_GAME_FRAMES_BYTES ==
            FRONTEND_STATIC_PRE_GAME_FRAME_COUNT *
                FRONTEND_FRAME_BYTES,
    "generated static pre-game frame atlas changed"
);
_Static_assert(
    FRONTEND_STATIC_SAVE_NAME_OVERLAY_VERSION == 1 &&
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_X % 4 == 0 &&
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH % 4 == 0 &&
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEADER_BYTES == 16 &&
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_PIXEL_BYTES ==
            FRONTEND_STATIC_SAVE_NAME_OVERLAY_WIDTH *
                FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEIGHT &&
        FRONTEND_STATIC_SAVE_NAME_OVERLAY_BYTES ==
            FRONTEND_STATIC_SAVE_NAME_OVERLAY_HEADER_BYTES +
                FRONTEND_STATIC_SAVE_NAME_OVERLAY_PIXEL_BYTES,
    "generated static save-name overlay changed"
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
static void frontend_start_game_menu_transition_selected(
    u8 flags,
    u8 selection
);
static void frontend_start_upgrade_menu_transition(u8 selection);
static void frontend_start_quit_transition(u8 enter_dialog);
static void frontend_render_upgrade_submenu_update(
    u8 old_selection,
    u8 old_scroll
);
static void frontend_nav_obj_commit_vblank(void);
static void frontend_nav_obj_disable(void);
static u8 frontend_upgrade_sim_is_active(void);
static u8 frontend_upgrade_sim_category_active(void);
static void frontend_upgrade_sim_enter_begin(void);
static void frontend_upgrade_sim_enter_stars(u8 processed, u8 count);
static void frontend_upgrade_sim_enter_loadout(void);
static void frontend_upgrade_sim_enter_prepare(void);
static void frontend_upgrade_sim_enter_finish(void);
static void frontend_upgrade_sim_update(void);
static void frontend_upgrade_sim_mark_loadout_dirty(void);
static void frontend_upgrade_sim_commit_vblank(void);
static void frontend_upgrade_sim_disable(void);
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
    OtShipDefinition ship;
    OtShieldDefinition shield;

    /*
     * Episode 4 replaces the shared HDT item block inside tyrian4.lvl.
     * Select that source table before reading ship/shield values or opening
     * Upgrade Ship; loading a gameplay level merely to populate a menu would
     * be an incorrect and much more expensive dependency.
     */
    (void)ot_data_episode_items_select((u8)(frontend_episode + 1u));
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
    frontend_data_cube_count = 0;
    frontend_data_page_count = 0;
    frontend_data_cache_valid = 0;
    memset(
        frontend_data_cube_list,
        0,
        sizeof(frontend_data_cube_list)
    );
    frontend_level_cube_start_count = 0;
    player_armor =
        ot_data_hdt_ship_read(frontend_player_items.ship, &ship) ?
            ship.damage :
            10;
    if (
        ot_data_hdt_shield_read(
            frontend_player_items.shield,
            &shield
        )
    ) {
        player_shield = shield.max_power;
        player_shield_max = (u8)(shield.max_power * 2u);
    } else {
        player_shield = 0;
        player_shield_max = 0;
    }
    frontend_player_items_valid = 1;
    frontend_route_state.player_alive = true;
    frontend_route_state.level_timer_expired = false;
    frontend_route_state.player_ship = frontend_player_items.ship;
    /* OpenTyrian chooses one of three persistent hint columns per game. */
    frontend_secret_hint = (u8)(1u + telemetry_vblank_irqs % 3u);
    frontend_script_checkpoint_clear();
    frontend_script_game_active = 0;
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

static void frontend_campaign_apply_cube_script(
    const u8 cube_list[OT_EPISODE_CUBE_CAPACITY],
    u8 cube_list_valid,
    const OtEpisodeCubeOperation *cube_operation,
    u8 cube_operation_count
)
{
    u8 operation;

    if (cube_list == 0 || cube_operation == 0) return;
    frontend_data_cache_valid = 0;
    if (cube_list_valid) {
        memcpy(
            frontend_data_cube_list,
            cube_list,
            sizeof(frontend_data_cube_list)
        );
    }
    for (
        operation = 0;
        operation < cube_operation_count;
        operation++
    ) {
        const OtEpisodeCubeOperation *item =
            &cube_operation[operation];

        switch (item->type) {
        case OT_EPISODE_CUBE_OP_CLAMP:
            if (frontend_data_cube_count > item->value) {
                frontend_data_cube_count = item->value;
            }
            break;
        case OT_EPISODE_CUBE_OP_SET:
            frontend_data_cube_count = item->value;
            break;
        case OT_EPISODE_CUBE_OP_ADD:
            if (
                frontend_data_cube_count <
                    OT_EPISODE_CUBE_CAPACITY
            ) {
                u8 room = (u8)(
                    OT_EPISODE_CUBE_CAPACITY -
                    frontend_data_cube_count
                );
                frontend_data_cube_count = (u8)(
                    frontend_data_cube_count +
                    (
                        item->value < room ?
                            item->value :
                            room
                    )
                );
            }
            break;
        default:
            break;
        }
    }
}

static void frontend_campaign_apply_map_script(
    const OtEpisodeMap *map
)
{
    if (map == 0) return;
    frontend_campaign_apply_cube_script(
        map->cube_list,
        map->cube_list_valid,
        map->cube_operation,
        map->cube_operation_count
    );
}

static void frontend_campaign_apply_level_script(
    const OtEpisodeLevel *level
)
{
    if (level == 0) return;
    frontend_campaign_apply_cube_script(
        level->cube_list,
        level->cube_list_valid,
        level->cube_operation,
        level->cube_operation_count
    );
    /*
     * OpenTyrian JE_main() resets cubeMax to zero after JE_loadMap() has
     * selected the playable level.  Cube pickups therefore describe only
     * the level just played; the following map section replaces cubeList
     * and clamps that new list to the number actually collected.  Carrying
     * the pre-level menu's four records forward made every post-Tyrian
     * message visible even when the player collected none.
     */
    frontend_data_cube_count = 0;
    frontend_level_cube_start_count = 0;
    frontend_data_cache_valid = 0;
}

static void frontend_campaign_capture_level_cubes(void)
{
    u32 total =
        frontend_level_cube_start_count +
        source_parity_level.data_cube_pickup_count;

    frontend_data_cube_count =
        total > OT_EPISODE_CUBE_CAPACITY ?
            OT_EPISODE_CUBE_CAPACITY :
            (u8)total;
    frontend_data_cache_valid = 0;
}

static void frontend_campaign_sync_from_level(void)
{
    if (!frontend_player_items_valid || frontend_demo_active) return;

    /*
     * Capture JE_loadMap()'s post-level conditional inputs before the next
     * LVL initialization resets them.  Episode 4 relies on all three.
     */
    frontend_route_state.player_alive = player_alive != 0;
    frontend_route_state.level_timer_expired =
        source_parity_level.level_timer &&
        source_parity_level.level_timer_countdown == 0;
    frontend_route_state.player_ship =
        source_parity_level.player_ship;

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
    frontend_campaign_capture_level_cubes();
}

#ifdef AUTOTEST_FRONTEND_TRANSITION_STRESS
typedef union {
    OtEpisodeMap map;
    OtEpisodeLevel level;
} FrontendCampaignAutotestRecord;

/* The source parser records are hundreds of bytes each; one reusable cold
 * union keeps deterministic regressions off the shared IWRAM call stack. */
static FrontendCampaignAutotestRecord
    frontend_campaign_autotest_record EWRAM_BSS;

/*
 * Exercise the exact Episode 1 source route without entering rendering:
 * intro Data (1,2,23,5) -> Tyrian pickups -> Asteroid decision Data
 * (6,7,12,18) -> Asteroid 1 pickups -> Savara Data (15,13,28).
 * Every global touched here is restored before returning.
 */
static u8 frontend_campaign_data_cube_progression_autotest(void)
{
    static const u8 after_tyrian[OT_EPISODE_CUBE_CAPACITY] = {
        6, 7, 12, 18
    };
    static const u8 after_asteroid[OT_EPISODE_CUBE_CAPACITY] = {
        15, 13, 28, 0
    };
    FrontendCampaignAutotestRecord *record =
        &frontend_campaign_autotest_record;
    u8 saved_count = frontend_data_cube_count;
    u8 saved_list[OT_EPISODE_CUBE_CAPACITY];
    u8 saved_cache_valid = frontend_data_cache_valid;
    u8 saved_start_count = frontend_level_cube_start_count;
    u32 saved_pickups = source_parity_level.data_cube_pickup_count;
    u8 passed = 1;

    memcpy(saved_list, frontend_data_cube_list, sizeof(saved_list));
    if (
        !ot_data_episode_level_resolve(
            1,
            3,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            &record->level
        )
    ) {
        passed = 0;
        goto restore;
    }

    frontend_data_cube_count = 4;
    frontend_campaign_apply_level_script(&record->level);
    if (
        frontend_data_cube_count != 0 ||
        frontend_level_cube_start_count != 0
    ) {
        passed = 0;
    }
    source_parity_level.data_cube_pickup_count = 2;
    frontend_campaign_capture_level_cubes();
    if (
        !ot_data_episode_map_resolve(
            1,
            4,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            &record->map
        )
    ) {
        passed = 0;
        goto restore;
    }
    frontend_campaign_apply_map_script(&record->map);
    if (
        frontend_data_cube_count != 2 ||
        memcmp(
            frontend_data_cube_list,
            after_tyrian,
            sizeof(after_tyrian)
        ) != 0
    ) {
        passed = 0;
    }

    if (
        !ot_data_episode_level_resolve(
            1,
            5,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            &record->level
        ) ||
        record->level.next_section != 8
    ) {
        passed = 0;
        goto restore;
    }
    frontend_campaign_apply_level_script(&record->level);
    source_parity_level.data_cube_pickup_count = 1;
    frontend_campaign_capture_level_cubes();
    if (
        !ot_data_episode_map_resolve(
            1,
            8,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            &record->map
        )
    ) {
        passed = 0;
        goto restore;
    }
    frontend_campaign_apply_map_script(&record->map);
    if (
        frontend_data_cube_count != 1 ||
        memcmp(
            frontend_data_cube_list,
            after_asteroid,
            sizeof(after_asteroid)
        ) != 0
    ) {
        passed = 0;
    }

restore:
    frontend_data_cube_count = saved_count;
    memcpy(frontend_data_cube_list, saved_list, sizeof(saved_list));
    frontend_data_cache_valid = saved_cache_valid;
    frontend_level_cube_start_count = saved_start_count;
    source_parity_level.data_cube_pickup_count = saved_pickups;
    return passed;
}

static u8 frontend_campaign_item_group_matches(
    const OtEpisodeMap *map,
    u8 group,
    const u8 *expected,
    u8 expected_count
)
{
    return
        map != 0 &&
        map->item_inventory_valid &&
        group < OT_EPISODE_ITEM_GROUP_COUNT &&
        map->item_count[group] == expected_count &&
        memcmp(
            map->item_avail[group],
            expected,
            expected_count
        ) == 0;
}

/*
 * Exact JE_loadMap inventory regression for the first Episode 1 merchant,
 * the post-Tyrian merchant, and the stock ]G -> ]J -> ]I Savara chain.
 * These are source IDs from levels1.dat, not a GBA-authored shop catalog.
 */
static u8 frontend_campaign_upgrade_inventory_autotest(void)
{
    static const u8 initial_ship[] = {1};
    static const u8 initial_front[] = {13, 2, 7};
    static const u8 initial_rear[] = {0, 11, 10, 29};
    static const u8 initial_generator[] = {1, 2, 3};
    static const u8 initial_left[] = {0, 9, 16, 30};
    static const u8 initial_right[] = {0, 9, 20, 30};
    static const u8 initial_shield[] = {1, 2, 3};
    static const u8 after_ship[] = {1};
    static const u8 after_front[] = {2, 13, 17};
    static const u8 after_rear[] = {0, 11, 12};
    static const u8 after_generator[] = {1, 2, 3};
    static const u8 after_left[] = {0, 1, 19, 15};
    static const u8 after_right[] = {0, 1, 19, 15};
    static const u8 after_shield[] = {3, 4, 5};
    static const u8 indirect_ship[] = {1};
    static const u8 indirect_front[] = {1, 2, 17};
    static const u8 indirect_rear[] = {0, 12, 15, 29};
    static const u8 indirect_generator[] = {1, 2, 3, 4};
    static const u8 indirect_left[] = {0, 2, 6, 15};
    static const u8 indirect_right[] = {0, 2, 6, 15};
    static const u8 indirect_shield[] = {3, 4};
    OtEpisodeMap *map = &frontend_campaign_autotest_record.map;

    if (
        !ot_data_episode_map_resolve(
            1,
            1,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            map
        ) ||
        map->resolved_section != 2 ||
        !frontend_campaign_item_group_matches(
            map, 0, initial_ship, sizeof(initial_ship)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 1, initial_front, sizeof(initial_front)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 2, initial_rear, sizeof(initial_rear)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 3, initial_generator, sizeof(initial_generator)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 5, initial_left, sizeof(initial_left)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 6, initial_right, sizeof(initial_right)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 8, initial_shield, sizeof(initial_shield)
        )
    ) {
        return 0;
    }
    if (
        !ot_data_episode_map_resolve(
            1,
            4,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            map
        ) ||
        map->resolved_section != 4 ||
        !frontend_campaign_item_group_matches(
            map, 0, after_ship, sizeof(after_ship)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 1, after_front, sizeof(after_front)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 2, after_rear, sizeof(after_rear)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 3, after_generator, sizeof(after_generator)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 5, after_left, sizeof(after_left)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 6, after_right, sizeof(after_right)
        ) ||
        !frontend_campaign_item_group_matches(
            map, 8, after_shield, sizeof(after_shield)
        )
    ) {
        return 0;
    }
    return
        ot_data_episode_map_resolve(
            1,
            7,
            FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL,
            map
        ) &&
        map->resolved_section == 7 &&
        frontend_campaign_item_group_matches(
            map,
            0,
            indirect_ship,
            sizeof(indirect_ship)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            1,
            indirect_front,
            sizeof(indirect_front)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            2,
            indirect_rear,
            sizeof(indirect_rear)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            3,
            indirect_generator,
            sizeof(indirect_generator)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            5,
            indirect_left,
            sizeof(indirect_left)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            6,
            indirect_right,
            sizeof(indirect_right)
        ) &&
        frontend_campaign_item_group_matches(
            map,
            8,
            indirect_shield,
            sizeof(indirect_shield)
        );
}
#endif
_Static_assert(
    FRONTEND_CUBE_BYTES ==
        FRONTEND_CUBE_WIDTH * FRONTEND_CUBE_HEIGHT,
    "generated OPTION_SHAPES cube stamp changed"
);
_Static_assert(
    FRONTEND_STATIC_SOURCE_HELP_STRIP_BASE == 0 &&
        FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT == 34 &&
        FRONTEND_STATIC_OPTIONS_HELP_STRIP_BASE ==
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT &&
        FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT == 0 &&
        FRONTEND_STATIC_SAVE_HELP_STRIP_BASE ==
            FRONTEND_STATIC_OPTIONS_HELP_STRIP_BASE +
                FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT &&
        FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT == 0 &&
        FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_BASE ==
            FRONTEND_STATIC_SAVE_HELP_STRIP_BASE +
                FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT &&
        FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT == 0 &&
        FRONTEND_STATIC_HELP_STRIP_COUNT ==
            FRONTEND_STATIC_SOURCE_HELP_STRIP_COUNT +
                FRONTEND_STATIC_OPTIONS_HELP_STRIP_COUNT +
                FRONTEND_STATIC_SAVE_HELP_STRIP_COUNT +
                FRONTEND_STATIC_SAVE_NAME_HELP_STRIP_COUNT &&
        FRONTEND_STATIC_HELP_STRIP_Y ==
            TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y - 1 &&
        FRONTEND_STATIC_HELP_STRIP_WIDTH == SCREEN_WIDTH &&
        FRONTEND_STATIC_HELP_STRIP_HEIGHT ==
            SCREEN_HEIGHT - FRONTEND_STATIC_HELP_STRIP_Y &&
        FRONTEND_STATIC_HELP_STRIP_BYTES ==
            FRONTEND_STATIC_HELP_STRIP_WIDTH *
                FRONTEND_STATIC_HELP_STRIP_HEIGHT &&
        FRONTEND_STATIC_HELP_STRIPS_BYTES ==
            FRONTEND_STATIC_HELP_STRIP_COUNT *
                FRONTEND_STATIC_HELP_STRIP_BYTES &&
        (FRONTEND_STATIC_HELP_STRIP_BYTES & 3u) == 0,
    "build-time main-menu help strips changed"
);
static const char frontend_runtime_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'/:-%";
static const char frontend_pregame_font_characters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789.,!?'/:-%;[]+^";
static u8 frontend_pregame_font_width_cache[
    FRONTEND_PREGAME_FONT_GLYPH_COUNT + 1u
] EWRAM_BSS;

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
    "stats font and cube must fit the retired end-level 8bpp cache window"
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
        !ot_data_episode_items_select(
            (u8)(frontend_episode + 1u)
        )
    ) {
        return 0;
    }
    /*
     * The IDs may be unchanged while Episode 4's underlying item records
     * and art selectors differ.  A cache key made only from IDs would then
     * be stale, so changing/refreshing the source database is an explicit
     * invalidation boundary.
     */
    frontend_ship_panel_cache_valid = 0;
    if (
        !ot_data_episode_map_resolve_with_route(
            (u8)(frontend_episode + 1u),
            frontend_main_section,
            frontend_play_mode,
            frontend_difficulty,
            &frontend_route_state,
            &frontend_map
        )
    ) {
        return 0;
    }
    /*
     * JE_loadMap() executes ]s / ]b only after following the authored
     * section chain.  Apply the resolved directives while campaign state
     * still describes the level preceding a scripted bonus game.
     */
    frontend_script_checkpoint_apply_map(&frontend_map);
    /*
     * JE_loadMap() executes ]?, ]! and ]+ before ]I opens Game Menu.
     * Applying them here makes the four stock Data cubes visible on the
     * menu that immediately follows, rather than one level too late.
     */
    frontend_campaign_apply_map_script(&frontend_map);
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
        !ot_data_episode_level_resolve_with_route(
            (u8)(frontend_episode + 1u),
            frontend_map.map_section[choice],
            frontend_play_mode,
            frontend_difficulty,
            &frontend_route_state,
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
    if (frontend_map.direct_level) {
        OtEpisodeLevel *level = &frontend_map_level[choice];

        /*
         * A direct ]L route is parsed once for the pre-menu map pass and
         * again for level metadata.  Its cube prefix is therefore identical
         * and has already mutated campaign state above.  Validate that
         * invariant, then remove the duplicate from the level-entry pass.
         */
        if (
            level->cube_list_valid != frontend_map.cube_list_valid ||
            level->cube_list_count != frontend_map.cube_list_count ||
            level->cube_operation_count !=
                frontend_map.cube_operation_count ||
            (
                level->cube_list_valid &&
                memcmp(
                    level->cube_list,
                    frontend_map.cube_list,
                    sizeof(level->cube_list)
                ) != 0
            ) ||
            memcmp(
                level->cube_operation,
                frontend_map.cube_operation,
                (size_t)level->cube_operation_count *
                    sizeof(level->cube_operation[0])
            ) != 0
        ) {
            return 0;
        }
        level->cube_list_valid = false;
        level->cube_list_count = 0;
        level->cube_operation_count = 0;
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
            2;

    /*
     * OpenTyrian initializes songBuy to DEFAULT_SONG_BUY (zero-based 2)
     * and passes it directly to play_song().  Script ]i values are the
     * separate one-based representation and are normalized by the episode
     * parser.  Therefore the no-override fallback is song 2, "Buy/Sell
     * Music", exactly as in the PC source.
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

/*
 * JE_scaleInPicture() expands the completed 320x200 Ship Specs page from a
 * narrow, half-height rectangle.  Mode 4 is affine, so the GBA can reproduce
 * that presentation in hardware without resampling 38.4 KiB in software on
 * every animation frame.
 */
static void frontend_set_affine_ship_specs(void)
{
    u16 step = frontend_ship_specs_scale_step;
    u16 width;
    u16 height;
    u16 scale_x;
    u16 scale_y;
    s16 left;

    if (step == 0) step = 1;
    if (step > FRONTEND_SHIP_SPECS_SCALE_STEPS) {
        step = FRONTEND_SHIP_SPECS_SCALE_STEPS;
    }
    width = (u16)(
        (
            (u32)SCREEN_WIDTH * step +
            FRONTEND_SHIP_SPECS_SCALE_STEPS - 1u
        ) /
        FRONTEND_SHIP_SPECS_SCALE_STEPS
    );
    if (width < 4) width = 4;
    height = (u16)(
        SCREEN_HEIGHT / 2u +
        (
            (u32)(SCREEN_HEIGHT / 2u) * step +
            FRONTEND_SHIP_SPECS_SCALE_STEPS - 1u
        ) /
        FRONTEND_SHIP_SPECS_SCALE_STEPS
    );
    if (height > SCREEN_HEIGHT) height = SCREEN_HEIGHT;
    scale_x = (u16)(((u32)SCREEN_WIDTH << 8) / width);
    scale_y = (u16)(((u32)SCREEN_HEIGHT << 8) / height);
    left = (s16)((SCREEN_WIDTH - width) / 2u);

    REG_BG2PA = scale_x;
    REG_BG2PB = 0;
    REG_BG2PC = 0;
    REG_BG2PD = scale_y;
    REG_BG2X = (u32)(-(s32)left * scale_x);
    REG_BG2Y = 0;
    REG_BG2CNT = BG_PRIORITY(0);
}

static void frontend_commit_mode4_registers(void)
{
    u16 display =
        MODE_4 | BG2_ON |
        (frontend_display_page ? BACKBUFFER : 0);

    if (
        game_state == STATE_SHIP_SPECS &&
        frontend_ship_specs_scale_active
    ) {
        frontend_set_affine_ship_specs();
    } else {
        frontend_set_affine_identity();
    }
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
    } else if (
        game_state == STATE_UPGRADE_SUBMENU &&
        frontend_upgrade_sim_is_active()
    ) {
        REG_WIN0H =
            (FRONTEND_UPGRADE_SIM_WINDOW_X0 << 8) |
            FRONTEND_UPGRADE_SIM_WINDOW_X1;
        REG_WIN0V =
            (FRONTEND_UPGRADE_SIM_WINDOW_Y0 << 8) |
            FRONTEND_UPGRADE_SIM_WINDOW_Y1;
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
    background_pending_rows[0].count = 0;
    background_pending_rows[1].count = 0;
    background_pending_rows[2].count = 0;
    hide_all_sprites();
    dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
    oam_dirty = 0;
    frontend_set_affine_identity();
    dmaCopy(frame, FRONTEND_MODE4_PAGE0, FRONTEND_FRAME_BYTES);
    dmaCopy(palette, BG_PALETTE, 512);
    frontend_display_page = 0;
    frontend_frame_pending = 0;
    frontend_pending_kind = FRONTEND_PENDING_NONE;
    frontend_palette_pending = 0;
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
    frontend_palette_pending = 0;
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

static void frontend_dirty_present_with_palette(const u16 *palette)
{
    frontend_dirty_present();
    if (palette == 0) return;
    frontend_pending_palette = (const u8 *)(const void *)palette;
    frontend_palette_pending = 1;
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
    frontend_upgrade_sim_commit_vblank();
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
            frontend_palette_pending = 0;
        }
        frontend_frame_pending = 0;
        frontend_pending_kind = FRONTEND_PENDING_NONE;
    }
    if (frontend_palette_pending) {
        dmaCopy(frontend_pending_palette, BG_PALETTE, 512);
        frontend_palette_pending = 0;
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

/*
 * OpenTyrian font.c/fonthand.c uses '~' as a zero-width highlight toggle.
 * Colour-bearing text paths add four brightness steps between a marker pair;
 * the GBA palette adapter keeps the hue and saturates the four-bit value just
 * like blit_sprite_hv().  Shadow/darken passes consume the marker but do not
 * change colour.
 */
static u8 frontend_text_highlight_colour(u8 colour)
{
    u8 brightness = (u8)(colour & 0x0fu);

    brightness = brightness > 11u ? 15u : (u8)(brightness + 4u);
    return (u8)((colour & 0xf0u) | brightness);
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
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_runtime_font_tile(character);
        advance = frontend_runtime_font_advance_scaled(
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
            highlight ? frontend_text_highlight_colour(colour) : colour,
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
    u8 highlight = 0;

    while (*measure != '\0') {
        char character = *measure++;

        if (character == '~') continue;
        compact_width = (s16)(
            compact_width + (character == ' ' ? 3 : 5)
        );
    }
    use_tiny = compact_width > right - x;
    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_runtime_font_tile(character);

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
                highlight ? frontend_text_highlight_colour(colour) : colour
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
                highlight ? frontend_text_highlight_colour(colour) : colour
            );
            x = (s16)(x + 5);
        }
    }
#elif TYRIAN_GBA_UPGRADE_LIST_FONT_MODE == 2
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_runtime_font_tile(character);

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
            highlight ? frontend_text_highlight_colour(colour) : colour
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
        if (*text == '~') {
            text++;
            continue;
        }
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
    if (character == ' ') return 0;
    if (character >= 'A' && character <= 'Z') {
        return (u8)(character - 'A' + 1);
    }
    if (character >= 'a' && character <= 'z') {
        return (u8)(character - 'a' + 27);
    }
    if (character >= '0' && character <= '9') {
        return (u8)(character - '0' + 53);
    }
    switch (character) {
    case '.': return 63;
    case ',': return 64;
    case '!': return 65;
    case '?': return 66;
    case '\'': return 67;
    case '/': return 68;
    case ':': return 69;
    case '-': return 70;
    case '%': return 71;
    case ';': return 72;
    case '[': return 73;
    case ']': return 74;
    case '+': return 75;
    case '^': return 76;
    default:
        /* The stock helper face uses '?' for every unsupported byte. */
        return 66;
    }
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
    u8 cached;

    if (
        tile_index == 0 ||
        tile_index > FRONTEND_PREGAME_FONT_GLYPH_COUNT
    ) {
        return 0;
    }
    cached = frontend_pregame_font_width_cache[tile_index];
    if (cached != 0) return cached;
    for (row = 0; row < FRONTEND_PREGAME_FONT_HEIGHT; row++) {
        occupied |= frontend_pregame_font_row(tile_index, row);
    }
    while (occupied >>= 1) width++;
    frontend_pregame_font_width_cache[tile_index] = width;
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

static void frontend_blit_pregame_font_darken(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y
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
                u8 *pixel = frame +
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x;

                *pixel = (u8)(
                    (*pixel & 0xf0u) |
                    ((*pixel & 0x0fu) >> 1)
                );
            }
        }
    }
}

static s16 frontend_pregame_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        if (*text == '~') {
            text++;
            continue;
        }
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
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_pregame_font_tile(character);
        advance = frontend_pregame_font_advance(tile_index);

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
            highlight ? frontend_text_highlight_colour(colour) : colour
        );
        x = (s16)(x + advance);
    }
}

/*
 * Recolour an already composed FULL_SHADE label without rebuilding its four
 * darkening passes.  This is used by the title Load selector: selection only
 * changes the glyph brightness, while the text and shaded background remain
 * identical.  Keeping the cheap pass separate also prevents a cursor move
 * from delaying the following Maxmod mix period.
 */
static void frontend_draw_pregame_text_clipped_no_shadow(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_pregame_font_tile(character);
        advance = frontend_pregame_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_pregame_font(
            frame,
            tile_index,
            x,
            y,
            highlight ? frontend_text_highlight_colour(colour) : colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_draw_pregame_text_full_shade(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_pregame_font_tile(character);
        advance = frontend_pregame_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_pregame_font_darken(
            frame,
            tile_index,
            (s16)(x - 1),
            y
        );
        frontend_blit_pregame_font_darken(
            frame,
            tile_index,
            (s16)(x + 1),
            y
        );
        frontend_blit_pregame_font_darken(
            frame,
            tile_index,
            x,
            (s16)(y - 1)
        );
        frontend_blit_pregame_font_darken(
            frame,
            tile_index,
            x,
            (s16)(y + 1)
        );
        frontend_blit_pregame_font(
            frame,
            tile_index,
            x,
            y,
            highlight ? frontend_text_highlight_colour(colour) : colour
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
        if (*text == '~') {
            text++;
            continue;
        }
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
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        char character = *text++;
        u8 tile_index;
        u8 advance;

        if (character == '~') {
            highlight ^= 1u;
            continue;
        }
        tile_index = frontend_pregame_font_tile(character);
        advance = frontend_menu_font_advance(tile_index);

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
            highlight ? frontend_text_highlight_colour(colour) : colour
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

static u8 frontend_small_mixed_source_column(
    u8 source_width,
    u8 target_width,
    u8 column
)
{
    static const u8 one_third[8] = {
        0, 0, 0, 1, 1, 1, 2, 2,
    };
    static const u8 two_thirds[8] = {
        0, 0, 1, 2, 2, 3, 4, 4,
    };
    u8 last = (u8)(source_width - 1u);

    if (target_width <= 1 || column == 0) return 0;
    if (column + 1u >= target_width) return last;
    if (target_width == 3) return (u8)(last >> 1);
    return column == 1 ?
        one_third[last] :
        two_thirds[last];
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
                frontend_small_mixed_source_column(
                    source_width,
                    target_width,
                    column
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

static void frontend_blit_small_mixed_font_darken(
    u8 *frame,
    u8 tile_index,
    s16 x,
    s16 y
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
            u8 source_column = frontend_small_mixed_source_column(
                source_width,
                target_width,
                column
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
                u8 *pixel = frame +
                    (u16)target_y * SCREEN_WIDTH + (u16)target_x;

                *pixel = (u8)(
                    (*pixel & 0xf0u) |
                    ((*pixel & 0x0fu) >> 1)
                );
            }
        }
    }
}

static s16 frontend_small_mixed_text_width(const char *text)
{
    s16 width = 0;

    while (text != 0 && *text != '\0') {
        /* OpenTyrian treats '~' as a zero-width highlight toggle. */
        if (*text == '~') {
            text++;
            continue;
        }
        width = (s16)(
            width +
            frontend_small_mixed_font_advance(
                frontend_pregame_font_tile(*text++)
            )
        );
    }
    return width;
}

static void frontend_draw_small_mixed_text_clipped_colours(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour,
    u8 shadow_colour
)
{
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        if (*text == '~') {
            highlight ^= 1u;
            text++;
            continue;
        }
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_small_mixed_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            (s16)(x + 1),
            (s16)(y + 1),
            shadow_colour
        );
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            x,
            y,
            highlight ? frontend_text_highlight_colour(colour) : colour
        );
        x = (s16)(x + advance);
    }
}

static void frontend_draw_small_mixed_text_clipped_no_shadow(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        if (*text == '~') {
            highlight ^= 1u;
            text++;
            continue;
        }
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_small_mixed_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            x,
            y,
            highlight ? frontend_text_highlight_colour(colour) : colour
        );
        x = (s16)(x + advance);
    }
}

/*
 * mainint.c:JE_loadScreen() uses JE_textShade(..., FULL_SHADE).  Its four
 * blit_sprite_dark() passes halve the existing background brightness around
 * the glyph; drawing a second, fixed-colour glyph at (+1,+1) instead creates
 * a visible duplicate/ghost.  Keep the source operation order here.
 */
static void frontend_draw_small_mixed_text_full_shade(
    u8 *frame,
    const char *text,
    s16 x,
    s16 y,
    s16 right,
    u8 colour
)
{
    u8 highlight = 0;

    while (*text != '\0' && x < right) {
        if (*text == '~') {
            highlight ^= 1u;
            text++;
            continue;
        }
        u8 tile_index = frontend_pregame_font_tile(*text++);
        u8 advance = frontend_small_mixed_font_advance(tile_index);

        if (x + advance > right) break;
        frontend_blit_small_mixed_font_darken(
            frame,
            tile_index,
            (s16)(x - 1),
            y
        );
        frontend_blit_small_mixed_font_darken(
            frame,
            tile_index,
            (s16)(x + 1),
            y
        );
        frontend_blit_small_mixed_font_darken(
            frame,
            tile_index,
            x,
            (s16)(y - 1)
        );
        frontend_blit_small_mixed_font_darken(
            frame,
            tile_index,
            x,
            (s16)(y + 1)
        );
        frontend_blit_small_mixed_font(
            frame,
            tile_index,
            x,
            y,
            highlight ? frontend_text_highlight_colour(colour) : colour
        );
        x = (s16)(x + advance);
    }
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
    frontend_draw_small_mixed_text_clipped_colours(
        frame,
        text,
        x,
        y,
        right,
        colour,
        FRONTEND_NEXT_SHADOW_COLOUR
    );
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
