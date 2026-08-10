/*
 * Source-parity Upgrade Ship weapon simulator.
 *
 * OpenTyrian game_menu.c does not show a static item icon for Front Gun,
 * Rear Gun, Generator or either Sidekick.  Those five pages execute
 * JE_initWeaponView(), then JE_weaponSimUpdate()/JE_weaponViewFrame() every
 * three display ticks.  This fragment preserves that control flow and the
 * stock HDT/Sprite2 data.  The only presentation adapter is to put the
 * moving layers in GBA OBJ hardware, transformed by the same exact 4/5
 * coordinate map as the 300x200 Mode-4 menu underneath.
 */

enum {
    FRONTEND_UPGRADE_SIM_SHOT_COUNT = 81,
    FRONTEND_UPGRADE_SIM_STAR_COUNT = 100,
    FRONTEND_UPGRADE_SIM_BAY_COUNT = 4,
    FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT = 24,
    FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES = 256,
    FRONTEND_UPGRADE_SIM_STAR_TILE_COUNT = 16,
    FRONTEND_UPGRADE_SIM_STAR_TILE_BYTES = 64,
    FRONTEND_UPGRADE_SIM_STAR_TILES_BYTES =
        FRONTEND_UPGRADE_SIM_STAR_TILE_COUNT *
            FRONTEND_UPGRADE_SIM_STAR_TILE_BYTES,
    FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES = 1024,
    FRONTEND_UPGRADE_SIM_BAR_TILES_BYTES = 640,
    FRONTEND_UPGRADE_SIM_STAR_VRAM_OFFSET = 0,
    FRONTEND_UPGRADE_SIM_SHIP_VRAM_OFFSET =
        FRONTEND_UPGRADE_SIM_STAR_VRAM_OFFSET +
            FRONTEND_UPGRADE_SIM_STAR_TILES_BYTES,
    FRONTEND_UPGRADE_SIM_BAR_VRAM_OFFSET =
        FRONTEND_UPGRADE_SIM_SHIP_VRAM_OFFSET +
            FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES,
    FRONTEND_UPGRADE_SIM_PROJECTILE_VRAM_OFFSET =
        FRONTEND_UPGRADE_SIM_BAR_VRAM_OFFSET +
            FRONTEND_UPGRADE_SIM_BAR_TILES_BYTES,
    FRONTEND_UPGRADE_SIM_VRAM_BYTES =
        FRONTEND_UPGRADE_SIM_PROJECTILE_VRAM_OFFSET +
            FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT *
                FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES,
    FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE =
        FRONTEND_NAV_OBJ_VRAM_TILE_BASE,
    FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X = 72,
    FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y = 110,
    FRONTEND_UPGRADE_SIM_SOURCE_LEFT_X = 57,
    FRONTEND_UPGRADE_SIM_SOURCE_RIGHT_X = 87,
    FRONTEND_UPGRADE_SIM_SOURCE_SIDEKICK_Y = 120,
    FRONTEND_UPGRADE_SIM_BAR_SOURCE_X = 141,
    FRONTEND_UPGRADE_SIM_BAR_SOURCE_Y = 56,
};

typedef struct {
    u8 active;
    u8 visible;
    u8 ttl;
    u8 animation;
    u8 animation_max;
    u8 complicated;
    u8 circle_size_x;
    u8 circle_size_y;
    u16 graphic;
    u16 render_graphic;
    s16 x;
    s16 y;
    s16 xm;
    s16 ym;
    s8 xc;
    s8 yc;
    s8 dev_x;
    s8 dir_x;
    s8 dev_y;
    s8 dir_y;
    u8 render_shape_table;
    u8 reserved;
} FrontendUpgradeSimShot;

typedef struct {
    u16 position;
    u8 speed;
    u8 colour;
} FrontendUpgradeSimStar;

typedef struct {
    u32 last_epoch;
    u16 graphic;
    u8 shape_table;
    u8 phase;
    u8 valid;
    u8 reserved[3];
} FrontendUpgradeSimProjectileSlot;

typedef struct {
    OtWeaponPortDefinition port;
    OtWeaponDefinition weapon;
    u8 valid;
    u8 reserved[3];
} FrontendUpgradeSimWeaponBay;

typedef struct {
    /* DMA source buffers stay first and naturally word aligned. */
    u8 star_tiles[FRONTEND_UPGRADE_SIM_STAR_TILES_BYTES];
    u8 ship_tiles[FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES];
    u8 bar_tiles[FRONTEND_UPGRADE_SIM_BAR_TILES_BYTES];
    u8 projectile_tiles[
        FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT
    ][FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES];
    FrontendUpgradeSimShot shot[FRONTEND_UPGRADE_SIM_SHOT_COUNT];
    FrontendUpgradeSimStar star[FRONTEND_UPGRADE_SIM_STAR_COUNT];
    FrontendUpgradeSimProjectileSlot projectile[
        FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT
    ];
    FrontendUpgradeSimWeaponBay bay[
        FRONTEND_UPGRADE_SIM_BAY_COUNT
    ];
    OtOptionDefinition option[2];
    OtShipDefinition ship;
    FrontendPlayerItems loadout;
    u32 projectile_epoch;
    u32 projectile_pending_mask;
    u32 random_state;
    u32 telemetry_sound_starts;
    u32 telemetry_projectile_dma_calls;
    u32 telemetry_projectile_upload_bytes;
    u32 telemetry_loadout_refreshes;
    u32 telemetry_ship_tile_builds;
    u32 telemetry_sound_replacements;
    u32 telemetry_sound_cancels;
    mm_sfxhand sound_handle[6];
    mm_sfxhand menu_effect_handle;
    u16 power;
    u8 repeat[FRONTEND_UPGRADE_SIM_BAY_COUNT];
    u8 multi_position[FRONTEND_UPGRADE_SIM_BAY_COUNT];
    u8 sound_queue[6];
    u8 generator_power;
    u8 frame_wait;
    u8 active;
    u8 loadout_dirty;
    u8 loadout_settle_wait;
    u8 loadout_initialized;
    u8 palette_pending;
    u8 star_tiles_pending;
    u8 ship_tiles_pending;
    u8 bar_tiles_pending;
    u8 ship_tile_valid;
} FrontendUpgradeSimArena;

#define frontend_upgrade_sim \
    ((FrontendUpgradeSimArena *)(void *)( \
        (u8 *)(void *)&frontend_gameplay_arena + \
        FRONTEND_COLD_PAGE_CACHE_OFFSET \
    ))

_Static_assert(
    FRONTEND_UPGRADE_SIM_VRAM_BYTES <= FRONTEND_NAV_OBJ_VRAM_BYTES,
    "Upgrade simulator exceeds shared Mode-4 OBJ VRAM"
);
_Static_assert(
    FRONTEND_COLD_PAGE_CACHE_OFFSET +
        sizeof(FrontendUpgradeSimArena) <=
        sizeof(FrontendGameplayArena),
    "Upgrade simulator must fit the shared cold front-end arena"
);

static u8 frontend_upgrade_sim_category_active(void)
{
    return
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_FRONT_WEAPON ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_REAR_WEAPON ||
        frontend_upgrade_category == FRONTEND_UPGRADE_GENERATOR ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_LEFT_SIDEKICK ||
        frontend_upgrade_category ==
            FRONTEND_UPGRADE_RIGHT_SIDEKICK;
}

static u8 frontend_upgrade_sim_is_active(void)
{
    /*
     * Short-circuit on the external flag and category before reading the
     * aliased arena.  In Ship/Data/Ship Specs that memory contains cache
     * pixels or records, not a FrontendUpgradeSimArena.
     */
    return
        frontend_upgrade_sim_live &&
        frontend_upgrade_sim_category_active() &&
        frontend_upgrade_sim->active;
}

static u32 frontend_upgrade_sim_random(void)
{
    u32 value = frontend_upgrade_sim->random_state;

    /* The source consumes the process-wide MT stream here.  Its exact state
     * depends on every earlier menu/game call, so the GBA adapter retains the
     * same four draws per star with a tiny deterministic local stream. */
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    if (value == 0) value = 0x6d2b79f5u;
    frontend_upgrade_sim->random_state = value;
    return value;
}

static void frontend_upgrade_sim_tile_plot(
    u8 *tiles,
    u8 tile_width,
    u8 x,
    u8 y,
    u8 colour
)
{
    u16 offset = (u16)(
        (
            (u16)(y >> 3) * (tile_width >> 3) +
            (x >> 3)
        ) * 64u +
        (u16)(y & 7u) * 8u +
        (x & 7u)
    );

    tiles[offset] = colour;
}

static void frontend_upgrade_sim_build_star_tiles(void)
{
    u8 index;

    memset(
        frontend_upgrade_sim->star_tiles,
        0,
        sizeof(frontend_upgrade_sim->star_tiles)
    );
    for (index = 0; index < FRONTEND_UPGRADE_SIM_STAR_TILE_COUNT; index++) {
        u8 *tile =
            frontend_upgrade_sim->star_tiles +
            (u16)index * FRONTEND_UPGRADE_SIM_STAR_TILE_BYTES;
        u8 colour = (u8)(0x90u + index);

        frontend_upgrade_sim_tile_plot(tile, 8, 3, 3, colour);
        if (colour >= 0x94u) {
            u8 dim = (u8)(colour - 4u);

            frontend_upgrade_sim_tile_plot(tile, 8, 2, 3, dim);
            frontend_upgrade_sim_tile_plot(tile, 8, 4, 3, dim);
            frontend_upgrade_sim_tile_plot(tile, 8, 3, 2, dim);
            frontend_upgrade_sim_tile_plot(tile, 8, 3, 4, dim);
        }
    }
    frontend_upgrade_sim->star_tiles_pending = 1;
}

static void frontend_upgrade_sim_initialize_starfield(
    u8 processed,
    u8 count
)
{
    s16 index = (s16)(
        FRONTEND_UPGRADE_SIM_STAR_COUNT - 1u - processed
    );
    s16 last = (s16)(index - count);

    /* Literal initialize_starfield() loop order and source dimensions. */
    for (
        ;
        index >= 0 && index > last;
        index--
    ) {
        FrontendUpgradeSimStar *star =
            &frontend_upgrade_sim->star[index];
        u16 x = (u16)(frontend_upgrade_sim_random() % 320u);
        u16 y = (u16)(frontend_upgrade_sim_random() % 200u);

        star->position = (u16)(x + y * 320u);
        star->speed = (u8)(frontend_upgrade_sim_random() % 3u + 2u);
        star->colour = (u8)(
            frontend_upgrade_sim_random() % 16u + 0x90u
        );
    }
}

static u8 frontend_upgrade_sim_abs_s8(s8 value)
{
    return (u8)(value < 0 ? -value : value);
}

static s8 frontend_upgrade_sim_clamp_s16(
    s16 value,
    s8 minimum,
    s8 maximum
)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return (s8)value;
}

static void frontend_upgrade_sim_load_weapon_bay(
    u8 bay,
    u8 port_id,
    u8 power_level,
    u8 mode
)
{
    FrontendUpgradeSimWeaponBay *runtime;
    u16 weapon_id;

    if (bay >= FRONTEND_UPGRADE_SIM_BAY_COUNT) return;
    runtime = &frontend_upgrade_sim->bay[bay];
    memset(runtime, 0, sizeof(*runtime));
    if (
        port_id == 0 ||
        !ot_data_hdt_weapon_port_read(port_id, &runtime->port)
    ) {
        return;
    }
    if (power_level == 0) power_level = 1;
    if (power_level > 11) power_level = 11;
    if (mode > 1) mode = 1;
    weapon_id = runtime->port.op[mode][power_level - 1u];
    if (
        weapon_id == 0 ||
        !ot_data_hdt_weapon_read(weapon_id, &runtime->weapon)
    ) {
        return;
    }
    runtime->valid = 1;
}

static void frontend_upgrade_sim_build_ship_tile(void)
{
    static const u8 component_origin_x[4] = {4, 16, 4, 16};
    static const u8 component_origin_y[4] = {2, 2, 16, 16};
    static const u8 component_graphic_offset[4] = {0, 1, 19, 20};
    const s16 source_x = FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X - 5 - 4;
    const s16 source_y = FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y - 7 - 2;
    const s16 screen_x = frontend_source_to_screen_x(source_x);
    const s16 screen_y = frontend_source_to_screen_y(source_y);
    u8 component;

    memset(
        frontend_upgrade_sim->ship_tiles,
        0,
        sizeof(frontend_upgrade_sim->ship_tiles)
    );
    frontend_upgrade_sim->ship_tile_valid = 0;
    if (frontend_upgrade_sim->ship.shipgraphic == 0) {
        frontend_upgrade_sim->ship_tiles_pending = 1;
        return;
    }
    /*
     * game_menu.c JE_drawItem(1, ...) calls blit_sprite2x2() on
     * spriteSheet9.  Preserve that literal four-component layout here:
     * graphic, graphic+1, graphic+19 and graphic+20 at the exact component
     * origins used by the source renderer.  The build-time raw bank is a
     * lossless decode of those same stock RLE records; using it avoids a
     * redundant runtime decode without introducing GBA-authored artwork.
     */
    for (component = 0; component < 4; component++) {
        const u8 *raw = ot_sprite2_raw_component(
            OT_COMP_SHAPE_TABLE_OPTIONS_SMALL,
            (u16)(
                frontend_upgrade_sim->ship.shipgraphic +
                component_graphic_offset[component]
            )
        );
        u8 y;

        if (raw == 0) {
            frontend_upgrade_sim->ship_tiles_pending = 1;
            return;
        }
        for (y = 0; y < OT_SPRITE2_COMPONENT_HEIGHT; y++) {
            u8 x;

            for (x = 0; x < OT_SPRITE2_COMPONENT_WIDTH; x++) {
                u8 colour = raw[
                    (u16)y * OT_SPRITE2_COMPONENT_WIDTH + x
                ];

                if (colour != 0) {
                    s16 frame_x = (s16)(
                        component_origin_x[component] + x
                    );
                    s16 frame_y = (s16)(
                        component_origin_y[component] + y
                    );
                    s16 target_x = (s16)(
                        frontend_source_to_screen_x(
                            (s16)(source_x + frame_x)
                        ) -
                        screen_x
                    );
                    s16 target_y = (s16)(
                        frontend_source_to_screen_y(
                            (s16)(source_y + frame_y)
                        ) -
                        screen_y
                    );

                    if (
                        target_x >= 0 && target_x < 32 &&
                        target_y >= 0 && target_y < 32
                    ) {
                        frontend_upgrade_sim_tile_plot(
                            frontend_upgrade_sim->ship_tiles,
                            32,
                            (u8)target_x,
                            (u8)target_y,
                            colour
                        );
                    }
                }
            }
        }
    }
    frontend_upgrade_sim->ship_tile_valid = 1;
    frontend_upgrade_sim->ship_tiles_pending = 1;
    frontend_upgrade_sim->telemetry_ship_tile_builds++;
}

static void frontend_upgrade_sim_refresh_loadout(void)
{
    u8 initial = !frontend_upgrade_sim->loadout_initialized;
    u8 rear_mode;
    u8 side;
    OtPowerDefinition generator;

    frontend_upgrade_sim->telemetry_loadout_refreshes++;
    if (
        initial ||
        frontend_upgrade_sim->loadout.generator !=
            frontend_player_items.generator
    ) {
        frontend_upgrade_sim->generator_power = 0;
        if (
            frontend_player_items.generator != 0 &&
            ot_data_hdt_power_read(
                frontend_player_items.generator,
                &generator
            )
        ) {
            frontend_upgrade_sim->generator_power = generator.power;
        }
    }
    if (
        initial ||
        frontend_upgrade_sim->loadout.weapon[
            FRONTEND_WEAPON_FRONT
        ].id != frontend_player_items.weapon[
            FRONTEND_WEAPON_FRONT
        ].id ||
        frontend_upgrade_sim->loadout.weapon[
            FRONTEND_WEAPON_FRONT
        ].power != frontend_player_items.weapon[
            FRONTEND_WEAPON_FRONT
        ].power
    ) {
        frontend_upgrade_sim_load_weapon_bay(
            0,
            frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id,
            frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].power,
            0
        );
    }
    rear_mode = frontend_player_items.weapon_mode;
    if (rear_mode == 0) rear_mode = 1;
    if (
        initial ||
        frontend_upgrade_sim->loadout.weapon[
            FRONTEND_WEAPON_REAR
        ].id != frontend_player_items.weapon[
            FRONTEND_WEAPON_REAR
        ].id ||
        frontend_upgrade_sim->loadout.weapon[
            FRONTEND_WEAPON_REAR
        ].power != frontend_player_items.weapon[
            FRONTEND_WEAPON_REAR
        ].power ||
        frontend_upgrade_sim->loadout.weapon_mode !=
            frontend_player_items.weapon_mode
    ) {
        frontend_upgrade_sim_load_weapon_bay(
            1,
            frontend_player_items.weapon[FRONTEND_WEAPON_REAR].id,
            frontend_player_items.weapon[FRONTEND_WEAPON_REAR].power,
            (u8)(rear_mode - 1u)
        );
    }
    for (side = 0; side < 2; side++) {
        FrontendUpgradeSimWeaponBay *runtime =
            &frontend_upgrade_sim->bay[2u + side];
        u8 option_id = frontend_player_items.sidekick[side];

        if (
            !initial &&
            frontend_upgrade_sim->loadout.sidekick[side] == option_id
        ) {
            continue;
        }
        memset(&frontend_upgrade_sim->option[side], 0,
            sizeof(frontend_upgrade_sim->option[side]));
        memset(runtime, 0, sizeof(*runtime));
        if (
            option_id != 0 &&
            ot_data_hdt_option_read(
                option_id,
                &frontend_upgrade_sim->option[side]
            )
        ) {
            OtOptionDefinition *option =
                &frontend_upgrade_sim->option[side];

            if (
                option->weapon_port != 0 &&
                option->weapon != 0 &&
                ot_data_hdt_weapon_port_read(
                    option->weapon_port,
                    &runtime->port
                ) &&
                ot_data_hdt_weapon_read(
                    option->weapon,
                    &runtime->weapon
                )
            ) {
                runtime->valid = 1;
            }
        }
    }
    if (
        initial ||
        frontend_upgrade_sim->loadout.ship !=
            frontend_player_items.ship
    ) {
        memset(&frontend_upgrade_sim->ship, 0,
            sizeof(frontend_upgrade_sim->ship));
        if (frontend_player_items.ship != 0) {
            (void)ot_data_hdt_ship_read(
                frontend_player_items.ship,
                &frontend_upgrade_sim->ship
            );
        }
        frontend_upgrade_sim_build_ship_tile();
    }
    frontend_upgrade_sim->loadout = frontend_player_items;
    frontend_upgrade_sim->loadout_initialized = 1;
    frontend_upgrade_sim->loadout_dirty = 0;
    frontend_upgrade_sim->loadout_settle_wait = 0;
}

static u8 frontend_upgrade_sim_spawn_weapon(
    u8 bay,
    s16 origin_x,
    s16 origin_y
)
{
    static const u8 sound_channel[FRONTEND_UPGRADE_SIM_BAY_COUNT] = {
        0, 2, 4, 4
    };
    FrontendUpgradeSimWeaponBay *runtime;
    const OtWeaponDefinition *weapon;
    u8 multi;
    u8 spawned = 0;

    if (bay >= FRONTEND_UPGRADE_SIM_BAY_COUNT) return 0;
    runtime = &frontend_upgrade_sim->bay[bay];
    if (!runtime->valid) return 0;
    weapon = &runtime->weapon;
    if (frontend_upgrade_sim->power < runtime->port.poweruse) return 0;
    frontend_upgrade_sim->power = (u16)(
        frontend_upgrade_sim->power - runtime->port.poweruse
    );
    if (weapon->sound != 0) {
        /* shots.c soundChannel[]: later shots on one source mixer channel
         * replace the earlier request before the per-frame queue drain. */
        frontend_upgrade_sim->sound_queue[
            sound_channel[bay]
        ] = weapon->sound;
    }

    for (multi = 0; multi < weapon->multi; multi++) {
        FrontendUpgradeSimShot *shot = 0;
        u8 index;
        u8 position;
        u8 delay;
        u8 circle;

        for (
            index = 0;
            index < FRONTEND_UPGRADE_SIM_SHOT_COUNT;
            index++
        ) {
            if (!frontend_upgrade_sim->shot[index].active) {
                shot = &frontend_upgrade_sim->shot[index];
                break;
            }
        }
        if (shot == 0) return spawned;
        if (
            frontend_upgrade_sim->multi_position[bay] == weapon->max ||
            frontend_upgrade_sim->multi_position[bay] > 8
        ) {
            frontend_upgrade_sim->multi_position[bay] = 1;
        } else {
            frontend_upgrade_sim->multi_position[bay]++;
        }
        position = (u8)(
            frontend_upgrade_sim->multi_position[bay] - 1u
        );
        if (position >= 8) position = 0;
        delay = weapon->delay[position];

        memset(shot, 0, sizeof(*shot));
        shot->x = (s16)(origin_x + weapon->bx[position]);
        shot->y = (s16)(origin_y + weapon->by[position]);
        shot->yc = (s8)-weapon->acceleration;
        shot->xc = weapon->accelerationx;
        shot->xm = weapon->sx[position];
        if (delay == 121) delay = 255;
        shot->graphic = weapon->sg[position];
        shot->ttl = delay;
        shot->animation_max =
            delay > 100 && delay < 120 ?
                (u8)(delay - 99u) :
                (u8)(weapon->weapani + 1u);
        if (shot->animation_max == 0) shot->animation_max = 1;
        if (delay == 99 || delay == 98) {
            shot->xm = (s16)(
                shot->xm +
                frontend_upgrade_sim_clamp_s16(
                    (s16)(
                        origin_x -
                        FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X
                    ),
                    -5,
                    5
                )
            );
        }
        if (delay == 99 || delay == 100) {
            shot->ym = frontend_upgrade_sim_clamp_s16(
                (s16)(
                    origin_y -
                    FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y -
                    weapon->sy[position]
                ),
                -4,
                4
            );
        } else if (weapon->sy[position] == 98) {
            shot->ym = 0;
            shot->yc = -1;
        } else if (weapon->sy[position] > 100) {
            shot->ym = weapon->sy[position];
        } else {
            shot->ym = (s16)-weapon->sy[position];
        }
        if (weapon->sx[position] > 100) {
            shot->xm = weapon->sx[position];
        }
        circle = weapon->circlesize;
        shot->complicated = circle != 0;
        if (circle != 0) {
            if (circle > 19) {
                shot->circle_size_x = (u8)(circle % 20u);
                shot->circle_size_y = (u8)(circle / 20u);
            } else {
                shot->circle_size_x = circle;
                shot->circle_size_y = circle;
            }
            shot->dev_x = (s8)(shot->circle_size_x >> 1);
            shot->dev_y = (s8)(shot->circle_size_y >> 1);
            shot->dir_x = 1;
            shot->dir_y = -1;
        }
        shot->active = shot->graphic != 0 && delay != 0;
        if (shot->active) spawned++;
    }
    frontend_upgrade_sim->repeat[bay] = weapon->shotrepeat;
    return spawned;
}

static void frontend_upgrade_sim_create_shots(void)
{
    u8 bay;
    s16 right_x = FRONTEND_UPGRADE_SIM_SOURCE_RIGHT_X;
    s16 right_y = FRONTEND_UPGRADE_SIM_SOURCE_SIDEKICK_Y;

    /* Literal JE_weaponViewFrame front/rear loop. */
    for (bay = 0; bay < 2; bay++) {
        if (frontend_upgrade_sim->repeat[bay] > 0) {
            frontend_upgrade_sim->repeat[bay]--;
        } else {
            (void)frontend_upgrade_sim_spawn_weapon(
                bay,
                FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X,
                FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y
            );
        }
    }

    if (frontend_upgrade_sim->bay[2].valid) {
        if (frontend_upgrade_sim->repeat[2] > 0) {
            frontend_upgrade_sim->repeat[2]--;
        } else {
            (void)frontend_upgrade_sim_spawn_weapon(
                2,
                FRONTEND_UPGRADE_SIM_SOURCE_LEFT_X,
                FRONTEND_UPGRADE_SIM_SOURCE_SIDEKICK_Y
            );
        }
    }
    if (frontend_upgrade_sim->option[1].style == 2) {
        right_x = FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X;
        right_y = FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y - 20;
        if (right_y < 10) right_y = 10;
    }
    if (frontend_upgrade_sim->bay[3].valid) {
        if (frontend_upgrade_sim->repeat[3] > 0) {
            frontend_upgrade_sim->repeat[3]--;
        } else {
            (void)frontend_upgrade_sim_spawn_weapon(
                3,
                right_x,
                right_y
            );
        }
    }
}

static void frontend_upgrade_sim_update_shots(void)
{
    u8 index;

    /* Literal simulate_player_shots() motion and 0..140/0..170 bounds. */
    for (
        index = 0;
        index < FRONTEND_UPGRADE_SIM_SHOT_COUNT;
        index++
    ) {
        FrontendUpgradeSimShot *shot =
            &frontend_upgrade_sim->shot[index];
        u16 animation_frame;

        shot->visible = 0;
        if (!shot->active || shot->ttl == 0) continue;
        shot->ttl--;
        shot->xm = (s16)(shot->xm + shot->xc);
        if (shot->xm <= 100) shot->x = (s16)(shot->x + shot->xm);
        shot->ym = (s16)(shot->ym + shot->yc);
        shot->y = (s16)(shot->y + shot->ym);
        if (shot->ym > 100) shot->y = (s16)(shot->y - 120);

        if (shot->complicated) {
            shot->dev_x = (s8)(shot->dev_x + shot->dir_x);
            shot->x = (s16)(shot->x + shot->dev_x);
            if (
                frontend_upgrade_sim_abs_s8(shot->dev_x) ==
                    shot->circle_size_x
            ) {
                shot->dir_x = (s8)-shot->dir_x;
            }
            shot->dev_y = (s8)(shot->dev_y + shot->dir_y);
            shot->y = (s16)(shot->y + shot->dev_y);
            if (
                frontend_upgrade_sim_abs_s8(shot->dev_y) ==
                    shot->circle_size_y
            ) {
                shot->dir_y = (s8)-shot->dir_y;
            }
        }
        if (
            shot->x < 0 || shot->x > 140 ||
            shot->y < 0 || shot->y > 170
        ) {
            shot->active = 0;
            continue;
        }
        animation_frame = (u16)(shot->graphic + shot->animation);
        shot->animation++;
        if (shot->animation == shot->animation_max) {
            shot->animation = 0;
        }
        if (animation_frame < 60000u) {
            if (animation_frame > 1000u) {
                animation_frame %= 1000u;
            }
            if (animation_frame > 500u) {
                shot->render_shape_table =
                    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY;
                shot->render_graphic = (u16)(animation_frame - 500u);
            } else {
                shot->render_shape_table =
                    OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
                shot->render_graphic = animation_frame;
            }
            shot->visible = shot->render_graphic != 0;
        }
        if (shot->ttl == 0) shot->active = 0;
    }
}

static void frontend_upgrade_sim_update_stars(void)
{
    s16 index;

    /* update_and_draw_starfield(..., 1), including uint16 wrap. */
    for (
        index = FRONTEND_UPGRADE_SIM_STAR_COUNT - 1;
        index >= 0;
        index--
    ) {
        FrontendUpgradeSimStar *star =
            &frontend_upgrade_sim->star[index];

        star->position = (u16)(
            star->position + (u16)(star->speed + 1u) * 320u
        );
    }
}

static void frontend_upgrade_sim_bar_plot(
    s16 source_x,
    s16 source_y,
    u8 colour
)
{
    s16 target_x = (s16)(
        frontend_source_to_screen_x(source_x) -
        frontend_source_to_screen_x(
            FRONTEND_UPGRADE_SIM_BAR_SOURCE_X
        )
    );
    s16 target_y = (s16)(
        frontend_source_to_screen_y(source_y) -
        frontend_source_to_screen_y(
            FRONTEND_UPGRADE_SIM_BAR_SOURCE_Y
        )
    );

    if (target_x >= 0 && target_x < 8 && target_y >= 0 && target_y < 80) {
        frontend_upgrade_sim_tile_plot(
            frontend_upgrade_sim->bar_tiles,
            8,
            (u8)target_x,
            (u8)target_y,
            colour
        );
    }
}

static void frontend_upgrade_sim_build_power_bar(void)
{
    s16 row;
    s16 top;
    s16 colour;

    memset(
        frontend_upgrade_sim->bar_tiles,
        0,
        sizeof(frontend_upgrade_sim->bar_tiles)
    );
    top = (s16)(147 - frontend_upgrade_sim->power / 10u);
    for (row = top; row <= 146; row++) {
        s16 phase;
        s16 x;

        colour = (s16)(113 + (146 - row) / 9 + 2);
        phase = (s16)((row + 1) % 6);
        if (phase == 1) colour += 3;
        else if (phase != 0) colour += 2;
        frontend_upgrade_sim_bar_plot(141, row, (u8)(colour - 3));
        frontend_upgrade_sim_bar_plot(142, row, (u8)(colour - 3));
        frontend_upgrade_sim_bar_plot(143, row, (u8)(colour - 2));
        frontend_upgrade_sim_bar_plot(144, row, (u8)(colour - 1));
        for (x = 145; x <= 149; x++) {
            frontend_upgrade_sim_bar_plot(x, row, (u8)colour);
        }
    }
    colour = (s16)(113 + (146 - top) / 9 + 4);
    frontend_upgrade_sim_bar_plot(141, (s16)(top - 1),
        (u8)(colour - 1));
    frontend_upgrade_sim_bar_plot(142, (s16)(top - 1),
        (u8)(colour - 1));
    frontend_upgrade_sim_bar_plot(143, (s16)(top - 1),
        (u8)(colour - 1));
    frontend_upgrade_sim_bar_plot(144, (s16)(top - 1),
        (u8)(colour - 1));
    {
        s16 x;

        for (x = 145; x <= 149; x++) {
            frontend_upgrade_sim_bar_plot(x, (s16)(top - 1),
                (u8)colour);
        }
    }
    frontend_upgrade_sim->bar_tiles_pending = 1;
}

static u8 frontend_upgrade_sim_projectile_pack(
    u8 slot,
    u8 shape_table,
    u16 graphic,
    s16 source_x,
    s16 source_y
)
{
    const u8 *raw = ot_sprite2_raw_component(shape_table, graphic);
    u8 *tiles;
    s16 base_x;
    s16 base_y;
    u8 y;

    if (
        slot >= FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT ||
        raw == 0
    ) {
        return 0;
    }
    tiles = frontend_upgrade_sim->projectile_tiles[slot];
    memset(tiles, 0, FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES);
    base_x = frontend_source_to_screen_x(source_x);
    base_y = frontend_source_to_screen_y(source_y);
    for (y = 0; y < OT_SPRITE2_COMPONENT_HEIGHT; y++) {
        u8 x;

        for (x = 0; x < OT_SPRITE2_COMPONENT_WIDTH; x++) {
            u8 pixel = raw[
                (u16)y * OT_SPRITE2_COMPONENT_WIDTH + x
            ];

            if (pixel != 0) {
                s16 target_x = (s16)(
                    frontend_source_to_screen_x((s16)(source_x + x)) -
                    base_x
                );
                s16 target_y = (s16)(
                    frontend_source_to_screen_y((s16)(source_y + y)) -
                    base_y
                );

                if (
                    target_x >= 0 && target_x < 16 &&
                    target_y >= 0 && target_y < 16
                ) {
                    frontend_upgrade_sim_tile_plot(
                        tiles,
                        16,
                        (u8)target_x,
                        (u8)target_y,
                        pixel
                    );
                }
            }
        }
    }
    return 1;
}

static u16 frontend_upgrade_sim_projectile_acquire(
    u8 shape_table,
    u16 graphic,
    s16 source_x,
    s16 source_y
)
{
    u8 phase_x = (u8)(source_x % 5);
    u8 phase_y = (u8)(source_y % 5);
    u8 phase = (u8)(phase_y * 5u + phase_x);
    u8 index;
    u8 free_slot = FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT;
    u8 eviction = FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT;
    u32 oldest = 0xffffffffu;

    for (
        index = 0;
        index < FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT;
        index++
    ) {
        FrontendUpgradeSimProjectileSlot *slot =
            &frontend_upgrade_sim->projectile[index];

        if (
            slot->valid &&
            slot->shape_table == shape_table &&
            slot->graphic == graphic &&
            slot->phase == phase
        ) {
            slot->last_epoch = frontend_upgrade_sim->projectile_epoch;
            return (u16)(
                FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE +
                (
                    FRONTEND_UPGRADE_SIM_PROJECTILE_VRAM_OFFSET +
                    (u16)index *
                        FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES
                ) / 32u
            );
        }
        if (!slot->valid) {
            if (free_slot == FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT) {
                free_slot = index;
            }
        } else if (
            slot->last_epoch != frontend_upgrade_sim->projectile_epoch &&
            slot->last_epoch < oldest
        ) {
            oldest = slot->last_epoch;
            eviction = index;
        }
    }
    index = free_slot != FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT ?
        free_slot : eviction;
    if (index == FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT) {
        return 0xffffu;
    }
    if (!frontend_upgrade_sim_projectile_pack(
            index,
            shape_table,
            graphic,
            source_x,
            source_y
        )) {
        frontend_upgrade_sim->projectile[index].valid = 0;
        return 0xffffu;
    }
    frontend_upgrade_sim->projectile[index].valid = 1;
    frontend_upgrade_sim->projectile[index].shape_table = shape_table;
    frontend_upgrade_sim->projectile[index].graphic = graphic;
    frontend_upgrade_sim->projectile[index].phase = phase;
    frontend_upgrade_sim->projectile[index].last_epoch =
        frontend_upgrade_sim->projectile_epoch;
    frontend_upgrade_sim->projectile_pending_mask |= 1u << index;
    return (u16)(
        FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE +
        (
            FRONTEND_UPGRADE_SIM_PROJECTILE_VRAM_OFFSET +
            (u16)index * FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES
        ) / 32u
    );
}

static void frontend_upgrade_sim_render_oam(void)
{
    u8 old_oam_count = previous_oam_count;
    s16 shot_index;
    u8 star_index;

    if (!frontend_upgrade_sim_is_active()) return;
    oam_count = 0;

    /* JE_weaponSimUpdate draws the ship last, in front of shots/stars. */
    if (frontend_upgrade_sim->ship_tile_valid) {
        frontend_nav_obj_put(
            frontend_source_to_screen_x(
                FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_X - 5 - 4
            ),
            frontend_source_to_screen_y(
                FRONTEND_UPGRADE_SIM_SOURCE_PLAYER_Y - 7 - 2
            ),
            (u16)(
                FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE +
                FRONTEND_UPGRADE_SIM_SHIP_VRAM_OFFSET / 32u
            ),
            32,
            32
        );
    }

    /* JE_weaponViewFrame's generator bar is a foreground UI element. */
    {
        s16 bar_x = frontend_source_to_screen_x(
            FRONTEND_UPGRADE_SIM_BAR_SOURCE_X
        );
        s16 bar_y = frontend_source_to_screen_y(
            FRONTEND_UPGRADE_SIM_BAR_SOURCE_Y
        );
        u16 tile = (u16)(
            FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE +
            FRONTEND_UPGRADE_SIM_BAR_VRAM_OFFSET / 32u
        );

        frontend_nav_obj_put(bar_x, bar_y, tile, 8, 32);
        frontend_nav_obj_put((s16)bar_x, (s16)(bar_y + 32),
            (u16)(tile + 8u), 8, 32);
        frontend_nav_obj_put((s16)bar_x, (s16)(bar_y + 64),
            (u16)(tile + 16u), 8, 16);
    }

    /* Later software-blitted shot slots cover earlier slots. */
    for (
        shot_index = FRONTEND_UPGRADE_SIM_SHOT_COUNT - 1;
        shot_index >= 0;
        shot_index--
    ) {
        FrontendUpgradeSimShot *shot =
            &frontend_upgrade_sim->shot[shot_index];
        s16 source_x;
        s16 source_y;
        u16 tile;

        if (!shot->visible) continue;
        source_x = (s16)(shot->x + 1);
        source_y = shot->y;
        tile = frontend_upgrade_sim_projectile_acquire(
            shot->render_shape_table,
            shot->render_graphic,
            source_x,
            source_y
        );
        if (tile == 0xffffu) continue;
        frontend_nav_obj_put(
            frontend_source_to_screen_x(source_x),
            frontend_source_to_screen_y(source_y),
            tile,
            16,
            16
        );
    }

    /* Source draws stars from 99 down to 0; slot zero is therefore front. */
    for (
        star_index = 0;
        star_index < FRONTEND_UPGRADE_SIM_STAR_COUNT;
        star_index++
    ) {
        const FrontendUpgradeSimStar *star =
            &frontend_upgrade_sim->star[star_index];
        u16 position = star->position;
        s16 source_x;
        s16 source_y;
        u16 tile;

        if (position >= 177u * 320u) continue;
        source_x = (s16)(position % 320u);
        source_y = (s16)(position / 320u);
        if (
            source_x < 8 || source_x > 143 ||
            source_y < 8 || source_y > 176
        ) {
            continue;
        }
        tile = (u16)(
            FRONTEND_UPGRADE_SIM_OBJ_TILE_BASE +
            (
                FRONTEND_UPGRADE_SIM_STAR_VRAM_OFFSET +
                (u16)(star->colour - 0x90u) *
                    FRONTEND_UPGRADE_SIM_STAR_TILE_BYTES
            ) / 32u
        );
        frontend_nav_obj_put(
            (s16)(frontend_source_to_screen_x(source_x) - 3),
            (s16)(frontend_source_to_screen_y(source_y) - 3),
            tile,
            8,
            8
        );
    }
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
}

static void frontend_upgrade_sim_prepare_tick(void)
{
    u8 sound_channel;

    if (
        frontend_upgrade_sim->loadout_dirty ||
        memcmp(
            &frontend_upgrade_sim->loadout,
            &frontend_player_items,
            sizeof(frontend_player_items)
        ) != 0
    ) {
        frontend_upgrade_sim_refresh_loadout();
    }
    frontend_upgrade_sim_update_stars();
    memset(
        frontend_upgrade_sim->sound_queue,
        0,
        sizeof(frontend_upgrade_sim->sound_queue)
    );
    frontend_upgrade_sim_create_shots();
    for (
        sound_channel = 0;
        sound_channel < sizeof(frontend_upgrade_sim->sound_queue);
        sound_channel++
    ) {
        u8 source_sound =
            frontend_upgrade_sim->sound_queue[sound_channel];

        if (source_sound != 0) {
            mm_sfxhand previous =
                frontend_upgrade_sim->sound_handle[sound_channel];

            if (previous != 0 && mmEffectActive(previous)) {
                mmEffectCancel(previous);
                frontend_upgrade_sim->telemetry_sound_replacements++;
            }
            frontend_upgrade_sim->sound_handle[sound_channel] =
                source_sound_effect_start(source_sound);
            frontend_upgrade_sim->telemetry_sound_starts++;
        }
    }
    frontend_upgrade_sim_update_shots();
    frontend_upgrade_sim->power = (u16)(
        frontend_upgrade_sim->power +
        frontend_upgrade_sim->generator_power
    );
    if (frontend_upgrade_sim->power > 900) {
        frontend_upgrade_sim->power = 900;
    }
    frontend_upgrade_sim_build_power_bar();
    frontend_upgrade_sim->projectile_epoch++;
    if (frontend_upgrade_sim->projectile_epoch == 0) {
        u8 index;

        frontend_upgrade_sim->projectile_epoch = 1;
        for (
            index = 0;
            index < FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT;
            index++
        ) {
            frontend_upgrade_sim->projectile[index].last_epoch = 0;
        }
    }
}

static void frontend_upgrade_sim_tick(void)
{
    frontend_upgrade_sim_prepare_tick();
    frontend_upgrade_sim_render_oam();
}

static void frontend_upgrade_sim_enter_begin(void)
{
    u32 seed;

    if (!frontend_upgrade_sim_category_active()) {
        frontend_upgrade_sim_disable();
        return;
    }
    hide_all_sprites();
    memset(frontend_upgrade_sim, 0, sizeof(*frontend_upgrade_sim));
    seed =
        0x6d2b79f5u ^
        ((u32)(frontend_episode + 1u) << 24) ^
        ((u32)frontend_main_section << 8) ^
        frontend_player_items.weapon[FRONTEND_WEAPON_FRONT].id;
    if (seed == 0) seed = 0x6d2b79f5u;
    frontend_upgrade_sim->random_state = seed;
    frontend_upgrade_sim->power = 500;
    memset(
        frontend_upgrade_sim->repeat,
        1,
        sizeof(frontend_upgrade_sim->repeat)
    );
    memset(
        frontend_upgrade_sim->multi_position,
        0,
        sizeof(frontend_upgrade_sim->multi_position)
    );
    frontend_upgrade_sim->projectile_epoch = 1;
    frontend_upgrade_sim->active = 1;
    frontend_upgrade_sim_live = 1;
    frontend_upgrade_sim->loadout_dirty = 1;
    frontend_upgrade_sim->palette_pending = 1;
    frontend_upgrade_sim_build_star_tiles();
}

static void frontend_upgrade_sim_enter_stars(
    u8 processed,
    u8 count
)
{
    if (!frontend_upgrade_sim_is_active()) return;
    frontend_upgrade_sim_initialize_starfield(processed, count);
}

static void frontend_upgrade_sim_enter_loadout(void)
{
    if (!frontend_upgrade_sim_is_active()) return;
    frontend_upgrade_sim_refresh_loadout();
}

static void frontend_upgrade_sim_enter_prepare(void)
{
    if (!frontend_upgrade_sim_is_active()) return;
    frontend_upgrade_sim_prepare_tick();
}

static void frontend_upgrade_sim_enter_finish(void)
{
    if (!frontend_upgrade_sim_is_active()) return;
    frontend_upgrade_sim_render_oam();
    frontend_upgrade_sim->frame_wait = 2;
}

static void frontend_upgrade_sim_update(void)
{
    if (
        !frontend_upgrade_sim_is_active() ||
        game_state != STATE_UPGRADE_SUBMENU
    ) {
        return;
    }
    /*
     * Cursor repeat can change a preview every VBlank.  The PC simulator is
     * intentionally three-frame paced; wait two physical frames for a stable
     * choice before decoding the transient loadout, spawning its projectile
     * burst and replacing mixer handles.  This coalesces intermediate cursor
     * positions without reducing the menu refresh rate.
     */
    if (
        frontend_upgrade_sim->loadout_dirty &&
        frontend_upgrade_sim->loadout_settle_wait > 0
    ) {
        frontend_upgrade_sim->loadout_settle_wait--;
        return;
    }
    /* Source uses setFrameCount(3) for this exact menu loop. */
    if (frontend_upgrade_sim->frame_wait > 0) {
        frontend_upgrade_sim->frame_wait--;
        return;
    }
    frontend_upgrade_sim->frame_wait = 2;
    frontend_upgrade_sim_tick();
}

static void frontend_upgrade_sim_mark_loadout_dirty(void)
{
    if (!frontend_upgrade_sim_is_active()) return;
    frontend_upgrade_sim->loadout_dirty = 1;
    frontend_upgrade_sim->loadout_settle_wait = 2;
}

static void frontend_upgrade_sim_play_menu_effect(u16 effect)
{
    mm_sfxhand previous;

    if (!frontend_upgrade_sim_is_active()) {
        previous = frontend_upgrade_menu_effect_handle;
        if (previous != 0 && mmEffectActive(previous)) {
            mmEffectCancel(previous);
        }
        frontend_upgrade_menu_effect_handle =
            audio_effect_start(effect);
        return;
    }
    previous = frontend_upgrade_sim->menu_effect_handle;
    if (previous != 0 && mmEffectActive(previous)) {
        mmEffectCancel(previous);
        frontend_upgrade_sim->telemetry_sound_replacements++;
    }
    frontend_upgrade_sim->menu_effect_handle =
        audio_effect_start(effect);
}

static void frontend_upgrade_sim_commit_vblank(void)
{
    u8 index;

    if (!frontend_upgrade_sim_is_active()) return;
    if (frontend_upgrade_sim->palette_pending) {
        dmaCopy(
            frontend_palette_address(FRONTEND_FRAME_MENU_CHROME),
            SPRITE_PALETTE,
            512
        );
        frontend_upgrade_sim->palette_pending = 0;
    }
    if (frontend_upgrade_sim->star_tiles_pending) {
        dmaCopy(
            frontend_upgrade_sim->star_tiles,
            FRONTEND_NAV_OBJ_VRAM +
                FRONTEND_UPGRADE_SIM_STAR_VRAM_OFFSET,
            FRONTEND_UPGRADE_SIM_STAR_TILES_BYTES
        );
        frontend_upgrade_sim->star_tiles_pending = 0;
    }
    if (frontend_upgrade_sim->ship_tiles_pending) {
        dmaCopy(
            frontend_upgrade_sim->ship_tiles,
            FRONTEND_NAV_OBJ_VRAM +
                FRONTEND_UPGRADE_SIM_SHIP_VRAM_OFFSET,
            FRONTEND_UPGRADE_SIM_SHIP_TILES_BYTES
        );
        frontend_upgrade_sim->ship_tiles_pending = 0;
    }
    if (frontend_upgrade_sim->bar_tiles_pending) {
        dmaCopy(
            frontend_upgrade_sim->bar_tiles,
            FRONTEND_NAV_OBJ_VRAM +
                FRONTEND_UPGRADE_SIM_BAR_VRAM_OFFSET,
            FRONTEND_UPGRADE_SIM_BAR_TILES_BYTES
        );
        frontend_upgrade_sim->bar_tiles_pending = 0;
    }
    index = 0;
    while (index < FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT) {
        u8 first;
        u8 count;
        u32 bytes;

        if (
            (frontend_upgrade_sim->projectile_pending_mask &
                (1u << index)) == 0
        ) {
            index++;
            continue;
        }
        first = index;
        do {
            index++;
        } while (
            index < FRONTEND_UPGRADE_SIM_PROJECTILE_CACHE_COUNT &&
            (frontend_upgrade_sim->projectile_pending_mask &
                (1u << index)) != 0
        );
        count = (u8)(index - first);
        bytes =
            (u32)count * FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES;
        dmaCopy(
            frontend_upgrade_sim->projectile_tiles[first],
            FRONTEND_NAV_OBJ_VRAM +
                FRONTEND_UPGRADE_SIM_PROJECTILE_VRAM_OFFSET +
                (u16)first *
                    FRONTEND_UPGRADE_SIM_PROJECTILE_TILE_BYTES,
            bytes
        );
        frontend_upgrade_sim->telemetry_projectile_dma_calls++;
        frontend_upgrade_sim->telemetry_projectile_upload_bytes += bytes;
    }
    frontend_upgrade_sim->projectile_pending_mask = 0;
}

static void frontend_upgrade_sim_disable(void)
{
    u8 channel;
    u8 was_live = frontend_upgrade_sim_live;

    if (was_live) {
        for (channel = 0; channel < 6; channel++) {
            mm_sfxhand handle =
                frontend_upgrade_sim->sound_handle[channel];

            if (handle != 0 && mmEffectActive(handle)) {
                mmEffectCancel(handle);
                frontend_upgrade_sim->telemetry_sound_cancels++;
            }
            frontend_upgrade_sim->sound_handle[channel] = 0;
        }
        if (
            frontend_upgrade_sim->menu_effect_handle != 0 &&
            mmEffectActive(frontend_upgrade_sim->menu_effect_handle)
        ) {
            mmEffectCancel(frontend_upgrade_sim->menu_effect_handle);
            frontend_upgrade_sim->telemetry_sound_cancels++;
        }
        frontend_upgrade_sim->menu_effect_handle = 0;
    }
    if (
        was_live ||
        previous_oam_count != 0
    ) {
        hide_all_sprites();
    }
    if (was_live) {
        frontend_upgrade_sim->active = 0;
        frontend_upgrade_sim->projectile_pending_mask = 0;
        frontend_upgrade_sim->palette_pending = 0;
        frontend_upgrade_sim->star_tiles_pending = 0;
        frontend_upgrade_sim->ship_tiles_pending = 0;
        frontend_upgrade_sim->bar_tiles_pending = 0;
    }
    frontend_upgrade_sim_live = 0;
}
