/* Scene-to-OAM renderer; contains no gameplay updates. */

static void render_player_at_source_y(
    s16 source_y,
    u8 priority,
    u8 exit_flight
)
{
    if (player_ship_graphic > 1) {
        u16 tile = source_player_cache_acquire(
            (u16)(
                player_ship_graphic +
                (exit_flight ? 0 : (s16)player_bank * 2)
            )
        );

        if (tile != 0xffff) {
            /*
             * JE_playerMovement() ordinary ship path is
             * blit_sprite2x2(x - 5, y - 7, spriteSheet9, ship_sprite).
             * The raw Sprite2 frame retains its native (4,2) component
             * placement, matching the established 32x32 player container.
             */
            s16 screen_y = (s16)(
                source_world_to_screen_y(
                    (s16)(
                        source_y +
                        SOURCE_PLAYER_PRESENTATION_CENTRE_Y
                    )
                ) - 16
            );

            if (
                !exit_flight &&
                source_player_invulnerable_visual_active
            ) {
                put_sprite_8bpp_32_blended(
                    source_player_screen_x(),
                    screen_y,
                    tile,
                    priority
                );
            } else {
                put_sprite_8bpp_32(
                    source_player_screen_x(),
                    screen_y,
                    tile,
                    priority
                );
            }
            return;
        }
    }

    /*
     * Source ship graphics 0 and 1 are special two-body cases.  The current
     * stock title demos use ordinary graphics; retain the validated USP
     * Talon presentation as a fail-closed fallback for those rare cases or
     * a corrupt/missing Sprite2 frame.
     */
    {
        s16 screen_y = (s16)(
            source_world_to_screen_y(
                (s16)(
                    source_y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y
                )
            ) - 16
        );
        u16 tile = exit_flight || player_bank == 0 ?
            OBJ_TILE_PLAYER_0 : OBJ_TILE_PLAYER_1;

        if (
            !exit_flight &&
            source_player_invulnerable_visual_active
        ) {
            put_sprite_blended_with_attr1(
                source_player_screen_x(),
                screen_y,
                tile,
                OBJ_PAL_PLAYER_0,
                ATTR1_SIZE_32,
                priority,
                player_bank < 0 ? OBJ_HFLIP : 0
            );
        } else {
            put_sprite_with_attr1(
                source_player_screen_x(),
                screen_y,
                tile,
                OBJ_PAL_PLAYER_0,
                ATTR1_SIZE_32,
                priority,
                !exit_flight && player_bank < 0 ? OBJ_HFLIP : 0
            );
        }
    }
}

static bool source_detail_shadows_allowed(void)
{
    return
        source_background2_enabled &&
        !source_detail_spotlight_requested &&
#if TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA
        /* Smooth wild BG1 Alpha has priority over local OBJ-window shadows. */
        !source_detail_wild_active &&
#endif
        gameplay_filter_brightness_level() == 0;
}

static void render_player_shadow_at_source_y(
    s16 source_y,
    u8 priority
)
{
    s16 screen_x = (s16)(
        source_player_screen_x() + 30 -
        (s16)source_parity_level.presentation_map_x2_offset
    );
    s16 screen_y = (s16)(
        source_world_to_screen_y(
            (s16)(source_y + SOURCE_PLAYER_PRESENTATION_CENTRE_Y)
        ) - 16 + 10
    );

    if (!source_detail_shadows_allowed()) return;
    if (player_ship_graphic > 1) {
        u16 tile = source_player_cache_acquire(
            (u16)(
                player_ship_graphic +
                (s16)player_bank * 2
            )
        );

        if (tile != 0xffff) {
            put_sprite_8bpp_32_windowed(
                screen_x,
                screen_y,
                tile,
                priority
            );
            return;
        }
    }
    put_sprite_windowed_with_attr1(
        screen_x,
        screen_y,
        player_bank == 0 ?
            OBJ_TILE_PLAYER_0 :
            OBJ_TILE_PLAYER_1,
        OBJ_PAL_PLAYER_0,
        ATTR1_SIZE_32,
        priority,
        player_bank < 0 ? OBJ_HFLIP : 0
    );
}

static void render_player_sprite(u8 priority)
{
    s16 trail_positions[19];
    s16 trail_y;
    s16 delta;
    s8 render_index;
    u8 trail_count = 0;
    u8 trail_spacing;
    u8 trail_index;
    u8 num_trails;

    if (!player_alive) return;
    if (
        !source_parity_level.end_level ||
        source_parity_level.level_end == 0
    ) {
        render_player_shadow_at_source_y(player_source_y, priority);
        render_player_at_source_y(player_source_y, priority, 0);
        return;
    }

    /*
     * Direct JE_playerMovement() end-flight trail. The PC does not draw a
     * separate ordinary player sprite in endLevel; it draws up to nineteen
     * copies along this triangular-spacing history instead.
     */
    delta = (s16)(41 - source_parity_level.level_end);
    if (delta < 0) delta = (s16)-delta;
    num_trails = delta > 20 ? 20 : (u8)delta;
    trail_spacing = 1;
    trail_y = player_source_y;
    for (trail_index = 0; trail_index < num_trails; trail_index++) {
        trail_y = (s16)(trail_y + trail_spacing);
        trail_spacing++;
    }
    for (trail_index = 1; trail_index < num_trails; trail_index++) {
        trail_y = (s16)(trail_y - trail_spacing);
        trail_spacing--;
        if (trail_y > 0 && trail_y < 170) {
            trail_positions[trail_count++] = trail_y;
        }
    }
    /*
     * Later PC blits win. Lower GBA OAM indices win equal-priority overlap,
     * so emit the stored source sequence in reverse.
     */
    for (
        render_index = (s8)trail_count - 1;
        render_index >= 0;
        render_index--
    ) {
        render_player_at_source_y(
            trail_positions[(u8)render_index],
            priority,
            1
        );
    }
    if (trail_count > telemetry_end_level_trail_max) {
        telemetry_end_level_trail_max = trail_count;
    }
}

static void render_player_sidekicks(u8 priority)
{
    u8 side;

    if (!player_alive || source_parity_level.end_level) return;
#if TYRIAN_GBA_STRESS_LOADOUT
    if (!stress_loadout_valid) return;
#endif
    for (side = 0; side < 2; side++) {
#if TYRIAN_GBA_STRESS_LOADOUT
        const OtOptionDefinition *option =
            side == 0 ? &stress_left_option : &stress_right_option;
        u8 animation = stress_sidekick_animation[side];
        u8 charge = 0;
        s16 source_x = stress_sidekick_x[side];
        s16 source_y = stress_sidekick_y[side];
#else
        const SourceSidekickRuntime *runtime =
            &source_sidekick[side];
        const OtOptionDefinition *option = &runtime->option;
        u8 animation = runtime->animation_frame;
        u8 charge = runtime->charge;
        s16 source_x = runtime->x;
        s16 source_y = runtime->y;
#endif
        OtEnemyDrawCommand draw;
        u16 graphic;
        u16 tile;
        s16 screen_x;
        s16 screen_y;

        if (
#if !TYRIAN_GBA_STRESS_LOADOUT
            !runtime->valid ||
#endif
            option->option == 0 ||
            option->animation_count == 0 ||
            animation >= option->animation_count
        ) {
            continue;
        }
        graphic = (u16)(option->graphic[animation] + charge);
        draw = (OtEnemyDrawCommand){
            .active = true,
            .x = source_x,
            .y = source_y,
            .graphic = graphic,
            .shape_table =
                option->style == 1 || option->style == 2 ?
                    26 :
                    OT_COMP_SHAPE_TABLE_OPTIONS_SMALL,
            .size =
                option->style == 1 || option->style == 2 ?
                    1 :
                    0,
            .filter = 0,
            .pool = 0,
        };
        tile = source_enemy_cache_acquire(&draw);
        if (tile == 0xffff) continue;
        if (draw.size != 0) {
            /*
             * blit_sprite2x2(x - 6, y) places its first 12x14 component at
             * canvas (4,2); compensate that unchanged source layout.
             */
            screen_x = source_world_to_screen_x(
                (s16)(source_x - 10)
            );
            screen_y = source_world_to_screen_y(
                (s16)(source_y - 2)
            );
            put_sprite_8bpp_32(
                screen_x,
                screen_y,
                tile,
                priority
            );
        } else if (source_enemy_cache_tile_is_compact(tile)) {
            put_sprite_8bpp_16(
                source_world_to_screen_x(source_x),
                source_world_to_screen_y(source_y),
                tile,
                priority
            );
        } else {
            /*
             * A size-0 Sprite2 occupies (8,8)..(23,23) in a full L2 frame.
             */
            put_sprite_8bpp_32(
                source_world_to_screen_x(
                    (s16)(source_x - 8)
                ),
                source_world_to_screen_y(
                    (s16)(source_y - 8)
                ),
                tile,
                priority
            );
        }
    }
}

/*
 * One presentation snapshot feeds OAM budgeting, cache admission and the
 * eventual draw pass.  Gameplay state remains authoritative; this is only a
 * compact per-frame directory which prevents each resource tier from
 * rescanning the same pools and recomputing the same screen coordinates.
 */
enum {
    SOURCE_CANDIDATE_RENDER_VISIBLE = 1u << 0,
    SOURCE_CANDIDATE_DEMAND_VISIBLE = 1u << 1,
    SOURCE_ENEMY_CANDIDATE_BOSS_CORE = 1u << 0,
    SOURCE_ENEMY_CANDIDATE_SELECTED = 1u << 1,
    SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE = 1u << 2,
    SOURCE_ENEMY_CANDIDATE_BOSS_COMPACT = 1u << 3,
    SOURCE_OAM_DEMAND_SATURATION = SPRITE_LIMIT + 1u,
    SOURCE_BOSS_LINK_WORD_COUNT = 256u / 32u,
    SOURCE_BOSS_COMPACT_HOLD_FRAMES = 60u,
};

typedef struct {
    s16 screen_x;
    s16 screen_y;
    u16 tile;
    u8 index;
    u8 flags;
    u8 palette;
} SourceEnemyOamCandidate;

typedef struct {
    s16 screen_x;
    s16 screen_y;
    u8 index;
    u8 oam_required;
    u8 flags;
    u8 reserved;
} SourceProjectileOamCandidate;

typedef struct {
    s16 screen_x;
    s16 screen_y;
    u8 index;
    u8 reserved;
} SourcePickupOamCandidate;

typedef struct {
    u8 first;
    u8 count;
} SourceEnemyOamRange;

typedef struct {
    SourceEnemyOamCandidate enemy[OT_ENEMY_COUNT];
    SourceProjectileOamCandidate player_shot[MAX_PLAYER_SHOTS];
    SourceProjectileOamCandidate enemy_shot[OT_ENEMY_SHOT_COUNT];
    SourcePickupOamCandidate pickup[MAX_PICKUP_EXPLOSIONS];
    u8 effect_index[MAX_EFFECTS];
    SourceEnemyOamRange sky_enemy;
    SourceEnemyOamRange top_enemy;
    SourceEnemyOamRange ground_enemy_75;
    SourceEnemyOamRange ground_enemy_25;
    u8 enemy_count;
    u8 player_shot_count;
    u8 enemy_shot_count;
    u8 pickup_count;
    u8 effect_count;
    u8 boss_structure_count;
    u8 boss_compact_count;
    u8 enemy_full_size_count;
    u16 player_structural_oam;
    u16 structural_oam;
    u16 player_projectile_oam;
    u16 enemy_projectile_oam;
    u16 decorative_oam;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    u16 structural_oam_exact;
    u16 player_projectile_oam_exact;
    u16 enemy_projectile_oam_exact;
    u16 decorative_oam_exact;
#endif
} SourceFrameOamCandidates;

_Static_assert(
    OT_ENEMY_COUNT <= UINT8_MAX &&
        MAX_PLAYER_SHOTS <= UINT8_MAX &&
        OT_ENEMY_SHOT_COUNT <= UINT8_MAX &&
        MAX_PICKUP_EXPLOSIONS <= UINT8_MAX &&
        MAX_EFFECTS <= UINT8_MAX,
    "OAM candidate indices and counts must fit in u8"
);

/*
 * Object type is determined by its owning pool.  Only ordinary enemies and
 * Boss components share enemy[100], so only that pool needs a persistent
 * presentation identity.  Membership is keyed by the gameplay-maintained
 * slot generation; moving a component cannot revoke it, while freeing and
 * reusing the same slot cannot inherit the old instance accidentally.
 */
static u16 source_boss_member_generation[OT_ENEMY_COUNT]
    EWRAM_BSS __attribute__((aligned(4)));
static u32 source_boss_confirmed_link_mask[SOURCE_BOSS_LINK_WORD_COUNT]
    EWRAM_BSS __attribute__((aligned(4)));
static u8 source_boss_compact_hold_frames EWRAM_BSS;

#include "source_boss_compact_cache.inc"

static SourceFrameOamCandidates source_frame_oam_candidates
    EWRAM_BSS __attribute__((aligned(4)));

static void source_boss_membership_reset(void)
{
    memset(
        source_boss_member_generation,
        0,
        sizeof(source_boss_member_generation)
    );
    memset(
        source_boss_confirmed_link_mask,
        0,
        sizeof(source_boss_confirmed_link_mask)
    );
    source_boss_compact_hold_frames = 0;
}

static bool source_boss_link_is_confirmed(u8 link)
{
    if (link == 0) return false;
    return (
        source_boss_confirmed_link_mask[link >> 5] &
        (1u << (link & 31u))
    ) != 0;
}

static void source_boss_confirm_link(u8 link)
{
    if (link == 0) return;
    source_boss_confirmed_link_mask[link >> 5] |=
        1u << (link & 31u);
}

static bool source_boss_member_is_current(u8 index)
{
    u16 generation =
        source_parity_level.enemy_instance_generation[index];

    return
        generation != 0 &&
        source_parity_level.enemy_avail[index] != 1 &&
        source_boss_member_generation[index] == generation;
}

static bool source_boss_manifest_member_is_current(u8 index)
{
    return
        source_parity_level.enemy_avail[index] != 1 &&
        ot_level_port_boss_manifest_member(&source_parity_level, index);
}

static void source_boss_mark_member(u8 index)
{
    u16 generation =
        source_parity_level.enemy_instance_generation[index];

    if (generation == 0 || source_parity_level.enemy_avail[index] == 1) {
        return;
    }
    source_boss_member_generation[index] = generation;
    /* Event 39 may rename a live link; the instance identity survives. */
    source_boss_confirm_link(source_parity_level.enemy[index].linknum);
}

static inline u16 source_oam_demand_add_saturated(
    u16 total,
    u16 amount
)
{
    if (
        total >= SOURCE_OAM_DEMAND_SATURATION ||
        amount >= SOURCE_OAM_DEMAND_SATURATION - total
    ) {
        return SOURCE_OAM_DEMAND_SATURATION;
    }
    return (u16)(total + amount);
}

static bool source_player_shot_demand_visible(
    u16 frame,
    s16 screen_x,
    s16 screen_y
)
{
    if (frame > 60000u) {
#if !TYRIAN_GBA_STRESS_LOADOUT
        u16 option_graphic = (u16)(frame - 60001u);

        if (option_graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
            return
                screen_x > -(s16)SOURCE_SPECIAL_OPTION_BANANA_WIDTH &&
                screen_x < SCREEN_WIDTH &&
                screen_y > -(s16)SOURCE_SPECIAL_OPTION_BANANA_HEIGHT &&
                screen_y < SCREEN_HEIGHT;
        }
#endif
        return
            screen_x > -64 && screen_x < SCREEN_WIDTH &&
            screen_y > -64 && screen_y < SCREEN_HEIGHT;
    }
    return
        screen_x > -16 && screen_x < SCREEN_WIDTH &&
        screen_y > -16 && screen_y < SCREEN_HEIGHT;
}

static bool source_player_shot_render_visible(
    u16 frame,
    s16 screen_x,
    s16 screen_y
)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (frame > 60000u) {
        u16 option_graphic = (u16)(frame - 60001u);

        if (option_graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC) {
            return
                screen_x > -(s16)SOURCE_SPECIAL_OPTION_BANANA_WIDTH &&
                screen_x < SCREEN_WIDTH &&
                screen_y > -(s16)SOURCE_SPECIAL_OPTION_BANANA_HEIGHT &&
                screen_y < SCREEN_HEIGHT;
        }
        if (
            option_graphic >= SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC &&
            option_graphic <
                SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC +
                    SOURCE_OPTION_PROJECTILE_FRAME_COUNT
        ) {
            return
                screen_x > -64 && screen_x < SCREEN_WIDTH &&
                screen_y > -32 && screen_y < SCREEN_HEIGHT;
        }
    }
#endif
    return source_player_shot_demand_visible(frame, screen_x, screen_y);
}

static u8 source_player_shot_candidate_oam(u16 frame)
{
#if !TYRIAN_GBA_STRESS_LOADOUT
    if (
        frame > 60000u &&
        (u16)(frame - 60001u) ==
            SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC
    ) {
        return 6u;
    }
#else
    (void)frame;
#endif
    return 1u;
}

static void source_collect_player_shot_candidates(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    s16 index;
    u8 demand_submitted = 0;

    frame->player_shot_count = 0;
    frame->player_projectile_oam = 0;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    frame->player_projectile_oam_exact = 0;
#endif
    for (index = MAX_PLAYER_SHOTS - 1; index >= 0; index--) {
        SourceProjectileOamCandidate *candidate;
        u16 graphic;
        s16 screen_x;
        s16 screen_y;
        u8 flags = 0;
        u8 required;

        if (!player_shots[index].active) continue;
        graphic = player_shots[index].render_graphic;
        screen_x = source_player_shot_screen_x(&player_shots[index]);
        screen_y = source_player_shot_screen_y(&player_shots[index]);
        required = source_player_shot_candidate_oam(graphic);
        if (
            source_player_shot_render_visible(
                graphic,
                screen_x,
                screen_y
            )
        ) {
            flags |= SOURCE_CANDIDATE_RENDER_VISIBLE;
        }
        if (
            source_player_shot_demand_visible(
                graphic,
                screen_x,
                screen_y
            )
        ) {
            flags |= SOURCE_CANDIDATE_DEMAND_VISIBLE;
        }
        candidate = &frame->player_shot[frame->player_shot_count++];
        candidate->screen_x = screen_x;
        candidate->screen_y = screen_y;
        candidate->index = (u8)index;
        candidate->oam_required = required;
        candidate->flags = flags;
        candidate->reserved = 0;
        if (
            (flags & SOURCE_CANDIDATE_DEMAND_VISIBLE) != 0 &&
            demand_submitted < MAX_VISIBLE_PLAYER_SHOTS
        ) {
            frame->player_projectile_oam =
                source_oam_demand_add_saturated(
                    frame->player_projectile_oam,
                    required
                );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            frame->player_projectile_oam_exact = (u16)(
                frame->player_projectile_oam_exact + required
            );
#endif
            demand_submitted++;
        }
    }
}

static void source_collect_enemy_shot_candidates(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    s16 index;
    u8 demand_submitted = 0;

    frame->enemy_shot_count = 0;
    frame->enemy_projectile_oam = 0;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    frame->enemy_projectile_oam_exact = 0;
#endif
    for (index = OT_ENEMY_SHOT_COUNT - 1; index >= 0; index--) {
        const OtEnemyShot *shot = &source_parity_level.enemy_shot[index];
        SourceProjectileOamCandidate *candidate;
        s16 screen_x;
        s16 screen_y;
        u8 flags = 0;

        if (!shot->active) continue;
        screen_x = source_enemy_shot_screen_x(shot);
        screen_y = source_enemy_shot_screen_y(shot);
        if (
            screen_x > -16 && screen_x < SCREEN_WIDTH &&
            screen_y > -16 && screen_y < SCREEN_HEIGHT
        ) {
            flags =
                SOURCE_CANDIDATE_RENDER_VISIBLE |
                SOURCE_CANDIDATE_DEMAND_VISIBLE;
        }
        candidate = &frame->enemy_shot[frame->enemy_shot_count++];
        candidate->screen_x = screen_x;
        candidate->screen_y = screen_y;
        candidate->index = (u8)index;
        candidate->oam_required = 1u;
        candidate->flags = flags;
        candidate->reserved = 0;
        if (
            (flags & SOURCE_CANDIDATE_DEMAND_VISIBLE) != 0 &&
            demand_submitted < MAX_VISIBLE_ENEMY_SHOTS
        ) {
            frame->enemy_projectile_oam =
                source_oam_demand_add_saturated(
                    frame->enemy_projectile_oam,
                    1u
                );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            frame->enemy_projectile_oam_exact++;
#endif
            demand_submitted++;
        }
    }
}

static void source_collect_effect_candidates(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    s16 index = (s16)effect_slot_high_water - 1;
    u8 demand_submitted = 0;

    frame->effect_count = 0;
    while (index >= 0) {
        s16 word_first = (s16)(index & ~31);

#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
        u16 word = (u16)index >> 5;
        u8 high_bit = (u8)(index & 31);
        u32 active_bits = effect_active_mask[word];

        if (high_bit < 31u) {
            active_bits &= (1u << (high_bit + 1u)) - 1u;
        }
        if (active_bits == 0) {
            index = (s16)(word_first - 1);
            continue;
        }
#endif
        for (; index >= word_first; index--) {
            if (!effects[index].active) continue;
            if (
                effects[index].x <= -16 ||
                effects[index].x >= SCREEN_WIDTH ||
                effects[index].y <= -16 ||
                effects[index].y >= SCREEN_HEIGHT
            ) {
                continue;
            }
            frame->effect_index[frame->effect_count++] = (u8)index;
            if (demand_submitted < MAX_VISIBLE_EFFECTS) {
                frame->decorative_oam =
                    source_oam_demand_add_saturated(
                        frame->decorative_oam,
                        1u
                    );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                frame->decorative_oam_exact++;
#endif
                demand_submitted++;
            }
        }
    }
}

static void source_collect_pickup_candidates(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    s16 index = MAX_PICKUP_EXPLOSIONS - 1;
#if TYRIAN_GBA_STRESS_LOADOUT
    const u16 candidate_oam = 1u;
#else
    const u16 candidate_oam = 2u;
#endif

    frame->pickup_count = 0;
    while (index >= 0) {
        s16 word_first = (s16)(index & ~31);

#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
        u16 word = (u16)index >> 5;
        u8 high_bit = (u8)(index & 31);
        u32 active_bits = pickup_explosion_active_mask[word];

        if (high_bit < 31u) {
            active_bits &= (1u << (high_bit + 1u)) - 1u;
        }
        if (active_bits == 0) {
            index = (s16)(word_first - 1);
            continue;
        }
#endif
        for (; index >= word_first; index--) {
            const PickupExplosion *explosion;
            SourcePickupOamCandidate *candidate;
            OtEnemyDrawCommand draw;
            s16 screen_x;
            s16 screen_y;

            if (!pickup_explosions[index].active) continue;
            explosion = &pickup_explosions[index];
            draw = (OtEnemyDrawCommand){
                .active = true,
                .x = explosion->x,
                .y = explosion->y,
            };
            screen_x = source_enemy_draw_screen_x(&draw);
            screen_y = source_enemy_draw_screen_y(&draw);
            if (
                screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
                screen_y <= -32 || screen_y >= SCREEN_HEIGHT
            ) {
                continue;
            }
            candidate = &frame->pickup[frame->pickup_count++];
            candidate->screen_x = screen_x;
            candidate->screen_y = screen_y;
            candidate->index = (u8)index;
            candidate->reserved = 0;
            frame->decorative_oam = source_oam_demand_add_saturated(
                frame->decorative_oam,
                candidate_oam
            );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            frame->decorative_oam_exact = (u16)(
                frame->decorative_oam_exact + candidate_oam
            );
#endif
        }
    }
}

static void source_collect_non_enemy_oam_candidates(void)
{
    source_frame_oam_candidates.decorative_oam = 0;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    source_frame_oam_candidates.decorative_oam_exact = 0;
#endif
    source_collect_player_shot_candidates();
    source_collect_enemy_shot_candidates();
    source_collect_pickup_candidates();
    source_collect_effect_candidates();
}

#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
static u8 source_player_shot_oam_cursor EWRAM_BSS;

static void render_player_shots(
    u8 priority,
    u8 oam_limit,
    u8 shadows_allowed,
    u8 fair_rotate
)
{
    SourceFrameOamCandidates *candidates = &source_frame_oam_candidates;
    u16 position;
    u16 range_end;
    u16 resume;
    u8 start;
    u8 wrapped = 0;
    u8 submitted = 0;

    if (candidates->player_shot_count == 0) {
        source_player_shot_oam_cursor = 0;
        if (fair_rotate) telemetry_projectile_oam_rotation_frames++;
        return;
    }
    if (
        source_player_shot_oam_cursor >= candidates->player_shot_count
    ) {
        source_player_shot_oam_cursor = 0;
    }
    start = fair_rotate ? source_player_shot_oam_cursor : 0u;
    position = start;
    range_end = candidates->player_shot_count;
    resume = start;

    /* PC blits ascending slots; later slots must win the GBA OBJ tie. */
    for (;;) {
        for (; position < range_end; position++) {
            const SourceProjectileOamCandidate *candidate =
                &candidates->player_shot[position];
            u8 index = candidate->index;

            if (
                submitted >= MAX_VISIBLE_PLAYER_SHOTS ||
                oam_count >= oam_limit
            ) {
                telemetry_player_shot_oam_culls++;
                goto player_shots_done;
            }
            resume = (u16)(position + 1u);
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
            if (
                (candidate->flags & SOURCE_CANDIDATE_RENDER_VISIBLE) == 0
            ) {
#if TYRIAN_GBA_STRESS_LOADOUT
                telemetry_projectile_culled_offscreen_before_cache++;
#endif
                continue;
            }
#endif
        {
            u16 frame = player_shots[index].render_graphic;
            u8 shape_table;
            u16 graphic;
            u16 tile;
            s16 screen_x = candidate->screen_x;
            s16 screen_y = candidate->screen_y;

            if (frame > 60000) {
#if TYRIAN_GBA_STRESS_LOADOUT
                if (
                    frame !=
                        60001 +
                            SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
                    !source_option_projectile_valid ||
                    !source_option_projectile_resident
                ) {
                    telemetry_stress_loadout_failures++;
                    continue;
                }
                if (
                    screen_x <= -64 || screen_x >= SCREEN_WIDTH ||
                    screen_y <= -64 || screen_y >= SCREEN_HEIGHT
                ) {
                    continue;
                }
                put_sprite_8bpp_64_blended(
                    screen_x,
                    screen_y,
                    SOURCE_OPTION_PROJECTILE_TILE_BASE,
                    priority
                );
                telemetry_stress_option_blend_draws++;
                submitted++;
#else
                u16 option_graphic = (u16)(frame - 60001u);
                u8 option_frame;

                if (
                    option_graphic == SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC ||
                    option_graphic == SOURCE_SPECIAL_OPTION_BANANA_GRAPHIC
                ) {
                    u8 required_oam = candidate->oam_required;
                    u16 special_tile;

                    if (
                        source_special_option_projectile_graphic !=
                            option_graphic ||
                        !source_special_option_projectile_resident ||
                        oam_count + required_oam > oam_limit
                    ) {
                        telemetry_player_shot_oam_culls++;
                        continue;
                    }
                    if (
                        option_graphic ==
                            SOURCE_SPECIAL_OPTION_XEGA_GRAPHIC
                    ) {
                        if (
                            screen_x <= -64 || screen_x >= SCREEN_WIDTH ||
                            screen_y <= -64 || screen_y >= SCREEN_HEIGHT
                        ) {
                            continue;
                        }
                        put_sprite_8bpp_64_blended(
                            screen_x,
                            screen_y,
                            SOURCE_SPECIAL_OPTION_XEGA_TILE_BASE,
                            priority
                        );
                    } else {
                        if (
                            screen_x <=
                                -(s16)SOURCE_SPECIAL_OPTION_BANANA_WIDTH ||
                            screen_x >= SCREEN_WIDTH ||
                            screen_y <=
                                -(s16)SOURCE_SPECIAL_OPTION_BANANA_HEIGHT ||
                            screen_y >= SCREEN_HEIGHT
                        ) {
                            continue;
                        }
                        special_tile =
                            SOURCE_SPECIAL_OPTION_BANANA_TILE_BASE;
                        put_sprite_8bpp_64_blended(
                            screen_x,
                            screen_y,
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_MAIN_TILE_OFFSET
                            ),
                            priority
                        );
                        put_sprite_8bpp_shaped_blended(
                            (s16)(screen_x + 64), screen_y,
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_RIGHT_TOP_TILE_OFFSET
                            ),
                            ATTR0_TALL, ATTR1_SIZE_32,
                            16, 32, priority
                        );
                        put_sprite_8bpp_shaped_blended(
                            (s16)(screen_x + 64), (s16)(screen_y + 32),
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_RIGHT_BOTTOM_TILE_OFFSET
                            ),
                            ATTR0_TALL, ATTR1_SIZE_32,
                            16, 32, priority
                        );
                        put_sprite_8bpp_shaped_blended(
                            screen_x, (s16)(screen_y + 64),
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_LEFT_TILE_OFFSET
                            ),
                            ATTR0_WIDE, ATTR1_SIZE_32,
                            32, 16, priority
                        );
                        put_sprite_8bpp_shaped_blended(
                            (s16)(screen_x + 32), (s16)(screen_y + 64),
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_BOTTOM_MIDDLE_TILE_OFFSET
                            ),
                            ATTR0_WIDE, ATTR1_SIZE_32,
                            32, 16, priority
                        );
                        put_sprite_8bpp_shaped_blended(
                            (s16)(screen_x + 64), (s16)(screen_y + 64),
                            (u16)(
                                special_tile +
                                SOURCE_SPECIAL_OPTION_BANANA_CORNER_TILE_OFFSET
                            ),
                            ATTR0_SQUARE, ATTR1_SIZE_16,
                            16, 16, priority
                        );
                    }
                    submitted++;
                    continue;
                }

                if (
                    option_graphic <
                        SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC ||
                    option_graphic >=
                        SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC +
                            SOURCE_OPTION_PROJECTILE_FRAME_COUNT
                ) {
                    continue;
                }
                option_frame = (u8)(
                    option_graphic -
                    SOURCE_OPTION_PROJECTILE_SOURCE_GRAPHIC
                );
                if (
                    (
                        source_option_projectile_valid &
                        (1u << option_frame)
                    ) == 0 ||
                    !source_option_projectile_resident
                ) {
                    continue;
                }
                if (
                    screen_x <= -64 || screen_x >= SCREEN_WIDTH ||
                    screen_y <= -32 || screen_y >= SCREEN_HEIGHT
                ) {
                    continue;
                }
                put_sprite_4bpp_64x32_blended(
                    screen_x,
                    screen_y,
                    (u16)(
                        SOURCE_OPTION_PROJECTILE_TILE_BASE +
                        option_frame *
                            SOURCE_OPTION_PROJECTILE_TILES_PER_FRAME
                    ),
                    priority
                );
                submitted++;
#endif
                continue;
            }
            if (frame > 1000) frame %= 1000;
            shape_table =
                frame > 500 ?
                    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY :
                    OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
            graphic = frame > 500 ? (u16)(frame - 500) : frame;
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
            tile = source_projectile_cache_acquire(
                shape_table,
                graphic,
                1
            );
#else
            tile = source_projectile_cache_acquire(
                shape_table,
                graphic,
                0
            );
#endif
            if (tile == 0xffff) continue;
            put_sprite_8bpp_16(
                screen_x,
                screen_y,
                tile,
                priority
            );
            if (
                shadows_allowed &&
                source_detail_shadows_allowed() &&
                player_shots[index].y + 10 < 190 &&
                player_shots[index].xm < 100 &&
                oam_count < oam_limit
            ) {
                put_sprite_8bpp_16_windowed(
                    screen_x,
                    (s16)(screen_y + 10),
                    tile,
                    priority
                );
            }
            submitted++;
        }
        }
        if (!fair_rotate || wrapped || start == 0) break;
        position = 0;
        range_end = start;
        wrapped = 1;
    }
player_shots_done:
    if (fair_rotate) {
        if (resume >= candidates->player_shot_count) {
            resume = (u16)(resume - candidates->player_shot_count);
        }
        source_player_shot_oam_cursor = (u8)resume;
        telemetry_projectile_oam_rotation_frames++;
    }
}
#endif

static u16 source_effect_oam_cursor EWRAM_BSS;

static void render_effect_sprites(
    u8 priority,
    u8 oam_limit,
    u8 fair_rotate
)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u16 position;
    u16 range_end;
    u16 resume;
    u16 start;
    u8 wrapped = 0;
    u8 submitted = 0;

    if (frame->effect_count == 0) {
        source_effect_oam_cursor = 0;
        if (fair_rotate) telemetry_effect_oam_rotation_frames++;
        return;
    }
    if (source_effect_oam_cursor >= frame->effect_count) {
        source_effect_oam_cursor = 0;
    }
    start = fair_rotate ? source_effect_oam_cursor : 0u;
    position = start;
    range_end = frame->effect_count;
    resume = start;
    for (;;) {
        for (; position < range_end; position++) {
            u8 index = frame->effect_index[position];
            u16 tile;

            if (
                submitted >= MAX_VISIBLE_EFFECTS ||
                oam_count >= oam_limit
            ) {
                telemetry_effect_oam_culls++;
                goto effects_done;
            }
            resume = (u16)(position + 1u);
            /*
             * Keep OpenTyrian's full 200-entry allocator and lifecycle,
             * then apply a presentation-only GBA budget.  The candidate
             * list already contains only active, on-screen entries in the
             * original reverse-PC order.
             */
            tile = source_effect_cache_acquire(
                effects[index].sequence,
                effects[index].frame
            );
            if (tile == 0xffff) continue;
            submitted++;
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
            put_sprite(
                effects[index].x, effects[index].y,
                tile,
                OBJ_PAL_EXPLOSION,
                ATTR1_SIZE_16, priority
            );
#else
            put_sprite_blended(
                effects[index].x, effects[index].y,
                tile,
                OBJ_PAL_EXPLOSION,
                ATTR1_SIZE_16, priority
            );
#endif
        }
        if (!fair_rotate || wrapped || start == 0) break;
        position = 0;
        range_end = start;
        wrapped = 1;
    }
effects_done:
    if (fair_rotate) {
        if (resume >= frame->effect_count) {
            resume = (u16)(resume - frame->effect_count);
        }
        source_effect_oam_cursor = resume;
        telemetry_effect_oam_rotation_frames++;
    }
}

static void render_legacy_rewards(u8 priority)
{
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    s16 index;

    for (index = MAX_REWARDS - 1; index >= 0; index--) {
        if (!rewards[index].active) continue;
        put_sprite(
            rewards[index].x, rewards[index].y,
            OBJ_TILE_REWARD +
                (
                    rewards[index].sequence * REWARD_FRAME_COUNT +
                    rewards[index].frame
                ) * REWARD_TILES_PER_FRAME,
            OBJ_PAL_REWARD, ATTR1_SIZE_16, priority
        );
    }
#else
    (void)priority;
#endif
}

static void render_source_enemy_cached_sprite(
    s16 screen_x,
    s16 screen_y,
    u16 tile,
    u8 priority
)
{
    if (source_enemy_cache_tile_is_split(tile)) {
#if !TYRIAN_GBA_STRESS_LOADOUT
        /* A fragmented canvas is one scene object: submit both halves or 0. */
        if (oam_count + 2u > SPRITE_LIMIT) return;
        put_sprite_8bpp_32x16(
            screen_x,
            screen_y,
            SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE,
            priority
        );
        put_sprite_8bpp_32x16(
            screen_x,
            (s16)(screen_y + 16),
            SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE,
            priority
        );
#endif
    } else if (source_enemy_cache_tile_is_compact(tile)) {
        put_sprite_8bpp_16(
            screen_x + 8,
            screen_y + 8,
            tile,
            priority
        );
    } else {
        put_sprite_8bpp_32(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}

/*
 * Direct translation of the inventory portions of JE_inGameDisplays().
 *
 * Both authoritative source states already existed in the GBA port, but
 * their persistent presentation was omitted. Acquire the unchanged Sprite2
 * records before world objects so the HUD owns the front-most OAM indices
 * and its cache frames cannot be evicted by the same scene. Decorative
 * effects and projectiles continue to yield through the existing structural
 * OAM budget.
 */
static void render_source_inventory_hud(void)
{
    OtEnemyDrawCommand draw = {0};
    u16 tile;

    if (source_parity_level.player_special > 0) {
        OtSpecialDefinition special;

        /*
         * JE_doSpecialShot() draws spriteSheet9 graphic 94 when all four
         * readiness conditions are clear, otherwise graphic 93. Emit this
         * one-component lamp before the 2x2 item so its lower OAM index
         * preserves the PC framebuffer draw order in their slight overlap.
         */
        draw.shape_table = OT_COMP_SHAPE_TABLE_OPTIONS_SMALL;
        draw.graphic = source_special_ready_indicator ?
            SOURCE_HUD_SPECIAL_READY_GRAPHIC :
            SOURCE_HUD_SPECIAL_COOLDOWN_GRAPHIC;
        tile = source_enemy_cache_acquire(&draw);
        if (tile != 0xffff) {
            render_source_enemy_cached_sprite(
                (s16)(TYRIAN_GBA_LAYOUT_SPECIAL_STATUS_X - 10),
                (s16)(TYRIAN_GBA_LAYOUT_SPECIAL_STATUS_Y - 9),
                tile,
                0
            );
        }

        if (
            ot_data_hdt_special_read(
                source_parity_level.player_special,
                &special
            ) &&
            special.itemgraphic > 0
        ) {
            /*
             * PC spriteSheet10 is the authored shape-table 26 path. A
             * size-one frame composes graphic,+1,+19,+20 at (4,2),
             * (16,2),(4,16),(16,16); offset the 32x32 GBA canvas so the
             * first component remains anchored at the configured HUD point.
             */
            memset(&draw, 0, sizeof(draw));
            draw.shape_table = SOURCE_HUD_SPECIAL_SHAPE_TABLE;
            draw.graphic = special.itemgraphic;
            draw.size = 1;
            tile = source_enemy_cache_acquire(&draw);
            if (tile != 0xffff) {
                render_source_enemy_cached_sprite(
                    (s16)(TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_X - 4),
                    (s16)(TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_Y - 2),
                    tile,
                    0
                );
            }
        }
    }

    if (source_parity_level.player_superbombs > 0) {
        u8 count = source_parity_level.player_superbombs;
        u8 index;

        if (count > SOURCE_HUD_SUPERBOMB_LIMIT) {
            count = SOURCE_HUD_SUPERBOMB_LIMIT;
        }
        memset(&draw, 0, sizeof(draw));
        draw.shape_table = OT_COMP_SHAPE_TABLE_OPTIONS_SMALL;
        draw.graphic = SOURCE_HUD_SUPERBOMB_GRAPHIC;
        tile = source_enemy_cache_acquire(&draw);
        if (tile == 0xffff) return;

        /*
         * A single Sprite2 component begins at (10,9) in the canonical
         * 32x32 frame. render_source_enemy_cached_sprite() also compensates
         * the compact 16x16 cache path, so each repeated icon keeps the
         * exact PC anchor and twelve-pixel spacing.
         */
        for (index = 0; index < count; index++) {
            render_source_enemy_cached_sprite(
                (s16)(
                    TYRIAN_GBA_LAYOUT_SUPERBOMB_X - 10 +
                    index * TYRIAN_GBA_LAYOUT_SUPERBOMB_X_STEP
                ),
                (s16)(TYRIAN_GBA_LAYOUT_SUPERBOMB_Y - 9),
                tile,
                0
            );
        }
    }
}

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW
static void render_source_enemy_cached_sprite_blended(
    s16 screen_x,
    s16 screen_y,
    u16 tile,
    u8 priority
)
{
    if (source_enemy_cache_tile_is_split(tile)) {
#if !TYRIAN_GBA_STRESS_LOADOUT
        if (oam_count + 2u > SPRITE_LIMIT) return;
        put_sprite_8bpp_32x16_blended(
            screen_x,
            screen_y,
            SOURCE_ENEMY_CACHE_SPLIT_TOP_TILE_BASE,
            priority
        );
        put_sprite_8bpp_32x16_blended(
            screen_x,
            (s16)(screen_y + 16),
            SOURCE_ENEMY_CACHE_SPLIT_BOTTOM_TILE_BASE,
            priority
        );
#endif
    } else if (source_enemy_cache_tile_is_compact(tile)) {
        put_sprite_8bpp_16_blended(
            screen_x + 8,
            screen_y + 8,
            tile,
            priority
        );
    } else {
        put_sprite_8bpp_32_blended(
            screen_x,
            screen_y,
            tile,
            priority
        );
    }
}
#endif

static u16 source_pickup_explosion_oam_cursor EWRAM_BSS;

static void render_source_pickup_explosions(
    u8 priority,
    u8 oam_limit,
    u8 fair_rotate
)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u16 position;
    u16 range_end;
    u16 resume;
    u16 start;
    u8 wrapped = 0;

    if (frame->pickup_count == 0) {
        source_pickup_explosion_oam_cursor = 0;
        if (fair_rotate) telemetry_effect_oam_rotation_frames++;
        return;
    }
    if (source_pickup_explosion_oam_cursor >= frame->pickup_count) {
        source_pickup_explosion_oam_cursor = 0;
    }
    start = fair_rotate ? source_pickup_explosion_oam_cursor : 0u;
    position = start;
    range_end = frame->pickup_count;
    resume = start;
    for (;;) {
        for (; position < range_end; position++) {
            const SourcePickupOamCandidate *candidate =
                &frame->pickup[position];
            u8 index = candidate->index;
            const PickupExplosion *explosion = &pickup_explosions[index];
            OtEnemyDrawCommand draw;
            u16 tile;
            u8 oam_required;

            if (oam_count >= oam_limit) {
                telemetry_effect_oam_culls++;
                goto pickup_explosions_done;
            }
            resume = (u16)(position + 1u);
            draw = (OtEnemyDrawCommand){
                .active = true,
                .x = explosion->x,
                .y = explosion->y,
                .graphic = (u16)(explosion->graphic + 1),
                .shape_table = OT_COMP_SHAPE_TABLE_EXPLOSION,
                .size = 0,
                .filter = 0,
                .pool = 0,
            };
            tile = source_enemy_cache_acquire(&draw);
            if (tile == 0xffff) continue;
            oam_required =
                source_enemy_cache_tile_is_split(tile) ? 2u : 1u;
            if (oam_count + oam_required > oam_limit) {
                telemetry_effect_oam_culls++;
                continue;
            }
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_LOW
            render_source_enemy_cached_sprite(
                candidate->screen_x,
                candidate->screen_y,
                tile,
                priority
            );
#else
            render_source_enemy_cached_sprite_blended(
                candidate->screen_x,
                candidate->screen_y,
                tile,
                priority
            );
#endif
        }
        if (!fair_rotate || wrapped || start == 0) break;
        position = 0;
        range_end = start;
        wrapped = 1;
    }
pickup_explosions_done:
    if (fair_rotate) {
        if (resume >= frame->pickup_count) {
            resume = (u16)(resume - frame->pickup_count);
        }
        source_pickup_explosion_oam_cursor = resume;
        telemetry_effect_oam_rotation_frames++;
    }
}

/*
 * Explosions and authored enemies share the Sprite2 L1 canvas cache.  The PC
 * draw order emits explosions first, but that must not let decorative frames
 * pin every physical GBA slot before an enemy or Boss requests its canvas.
 * Prime only visible structural draw commands, then let the ordinary reverse
 * draw pass reuse these stable tiles.  Effects may consume whatever remains.
 */
static u8 source_enemy_rotation_start EWRAM_BSS;

static u16 source_player_structural_oam_required(void)
{
    u16 required = 0;
#if !TYRIAN_GBA_STRESS_LOADOUT
    u8 index;
#endif

    if (!player_alive) return 0;
    if (
        source_parity_level.end_level &&
        source_parity_level.level_end != 0
    ) {
        /* JE_playerMovement() can emit at most nineteen exit copies. */
        return 19u;
    }

    required++;
    if (source_detail_shadows_allowed()) required++;
#if TYRIAN_GBA_STRESS_LOADOUT
    if (stress_loadout_valid) required += 2u;
#else
    for (index = 0; index < 2; index++) {
        const SourceSidekickRuntime *runtime = &source_sidekick[index];

        if (
            runtime->valid &&
            runtime->option.option != 0 &&
            runtime->option.animation_count != 0 &&
            runtime->animation_frame < runtime->option.animation_count
        ) {
            required += 2u;
        }
    }
#endif
    return required;
}

static bool source_enemy_is_boss_core_component(u8 index)
{
    u8 link = source_parity_level.enemy[index].linknum;

    return
        link != 0 &&
        (
            link == source_parity_level.boss_bar_link[0] ||
            link == source_parity_level.boss_bar_link[1]
        );
}

static bool source_enemy_boss_structure_overlaps(
    const SourceEnemyOamCandidate *left,
    const SourceEnemyOamCandidate *right
)
{
    /*
     * JE_drawEnemy's largest authored canvas is 32x32.  Touching canvases
     * are part of the same visual assembly; a four-pixel tolerance bridges
     * the deliberate seams between adjacent SAVARA/DREAD-NOT components.
     */
    return
        left->screen_x < right->screen_x + 36 &&
        right->screen_x < left->screen_x + 36 &&
        left->screen_y < right->screen_y + 36 &&
        right->screen_y < left->screen_y + 36;
}

static __attribute__((noinline, noclone)) void
source_enemy_mark_boss_structure(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u8 changed;
    u16 position;

    frame->boss_structure_count = 0;
    frame->boss_compact_count = 0;
    frame->enemy_full_size_count = 0;
    for (position = 0; position < frame->enemy_count; position++) {
        SourceEnemyOamCandidate *candidate = &frame->enemy[position];
        u8 index = candidate->index;
        u16 generation =
            source_parity_level.enemy_instance_generation[index];

        if (
            source_boss_member_generation[index] != 0 &&
            source_boss_member_generation[index] != generation
        ) {
            /* The pool slot now belongs to a different enemy instance. */
            source_boss_member_generation[index] = 0;
        }
        if (
            (candidate->flags & SOURCE_ENEMY_CANDIDATE_BOSS_CORE) != 0 ||
            source_boss_manifest_member_is_current(index) ||
            source_boss_member_is_current(index)
        ) {
            candidate->flags |= SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE;
            source_boss_mark_member(index);
        }
    }

    /*
     * The build-time LVL manifest seeds pre-intro components at their spawn
     * event, before Event 79 exists.  Event 79 remains the authoritative
     * damage/bar link and runtime fallback.  Stock bosses can surround
     * that seed with many invulnerable link groups (SAVARA uses 14..19 plus
     * transformed 121/141/161/181 groups).  A visible assembled frame is a
     * useful bootstrap, but current coordinates are not identity: the LVL
     * script deliberately moves those link groups apart.  Every discovered
     * member is therefore pinned to its slot generation and confirms its
     * link for the remainder of this level.  Later frames first inherit by
     * that persistent identity/link and only then extend the bootstrap walk.
     */
    do {
        changed = 0;
        for (position = 0; position < frame->enemy_count; position++) {
            SourceEnemyOamCandidate *candidate = &frame->enemy[position];
            const OtEnemy *enemy =
                &source_parity_level.enemy[candidate->index];
            u16 seed_position;

            if (
                (candidate->flags &
                    SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) != 0 ||
                enemy->linknum == 0
            ) {
                continue;
            }
            if (source_boss_link_is_confirmed(enemy->linknum)) {
                candidate->flags |=
                    SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE;
                source_boss_mark_member(candidate->index);
                changed = 1;
                continue;
            }
            if (!enemy->enemyground || enemy->armorleft != 255) continue;
            for (
                seed_position = 0;
                seed_position < frame->enemy_count;
                seed_position++
            ) {
                const SourceEnemyOamCandidate *seed =
                    &frame->enemy[seed_position];

                if (
                    (seed->flags &
                        SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) == 0 ||
                    !source_enemy_boss_structure_overlaps(candidate, seed)
                ) {
                    continue;
                }
                candidate->flags |=
                    SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE;
                source_boss_mark_member(candidate->index);
                changed = 1;
                break;
            }
        }
    } while (changed);

    for (position = 0; position < frame->enemy_count; position++) {
        const SourceEnemyOamCandidate *candidate = &frame->enemy[position];
        bool full_size =
            source_parity_level.enemy_draw[candidate->index].size == 1;

        if (full_size) frame->enemy_full_size_count++;

        if (
            (candidate->flags &
                SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) == 0
        ) {
            continue;
        }
        frame->boss_structure_count++;
        if (full_size) frame->boss_compact_count++;
    }
}

static void source_enemy_collect_visible_pool(
    u8 pool,
    SourceEnemyOamRange *range
)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    s16 slot;

    range->first = frame->enemy_count;
    range->count = 0;
    for (slot = OT_ENEMY_POOL_SIZE - 1; slot >= 0; slot--) {
        u8 index = (u8)(pool + slot);
        const OtEnemyDrawCommand *draw =
            &source_parity_level.enemy_draw[index];
        SourceEnemyOamCandidate *candidate;
        s16 screen_x;
        s16 screen_y;

        if (!draw->active) continue;
        screen_x = source_enemy_draw_screen_x(draw);
        screen_y = source_enemy_draw_screen_y(draw);
        if (
            screen_x <= -32 || screen_x >= SCREEN_WIDTH ||
            screen_y <= -32 || screen_y >= SCREEN_HEIGHT
        ) {
            continue;
        }
        if (frame->enemy_count >= OT_ENEMY_COUNT) break;
        candidate = &frame->enemy[frame->enemy_count++];
        candidate->screen_x = screen_x;
        candidate->screen_y = screen_y;
        candidate->tile = 0xffff;
        candidate->index = index;
        candidate->flags = source_enemy_is_boss_core_component(index) ?
            SOURCE_ENEMY_CANDIDATE_BOSS_CORE : 0u;
        candidate->palette = 0;
        range->count++;
    }
}

static void source_collect_enemy_oam_candidates(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
#if TYRIAN_GBA_STRESS_LOADOUT
    const u16 object_cost = 1u;
#else
    /* Fragmented Sprite2 canvases can require two 32x16 OBJ entries. */
    const u16 object_cost = 2u;
#endif
    u16 enemy_oam;

    frame->enemy_count = 0;
    if (source_parity_level.sky_enemy_over_all) {
        source_enemy_collect_visible_pool(0, &frame->sky_enemy);
    }
    if (source_parity_level.top_enemy_over) {
        source_enemy_collect_visible_pool(50, &frame->top_enemy);
    }
    if (!source_parity_level.top_enemy_over) {
        source_enemy_collect_visible_pool(50, &frame->top_enemy);
    }
    if (!source_parity_level.sky_enemy_over_all) {
        source_enemy_collect_visible_pool(0, &frame->sky_enemy);
    }
    source_enemy_collect_visible_pool(75, &frame->ground_enemy_75);
    source_enemy_collect_visible_pool(25, &frame->ground_enemy_25);
    source_enemy_mark_boss_structure();

    frame->player_structural_oam =
        source_player_structural_oam_required();
    enemy_oam = (u16)(frame->enemy_count * object_cost);
    frame->structural_oam = source_oam_demand_add_saturated(
        frame->player_structural_oam,
        enemy_oam
    );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    frame->structural_oam_exact = (u16)(
        frame->player_structural_oam + enemy_oam
    );
    if (
        frame->structural_oam_exact >
        telemetry_structural_oam_required_max
    ) {
        telemetry_structural_oam_required_max =
            frame->structural_oam_exact;
    }
#else
    if (frame->structural_oam > telemetry_structural_oam_required_max) {
        telemetry_structural_oam_required_max = frame->structural_oam;
    }
#endif
}

/*
 * OAM is a presentation resource, not gameplay state.  Reserve it by class:
 * player/authored enemies first, live projectiles second, and explosions or
 * shadows last.  If authored enemies alone exceed the remaining hardware
 * entries, rotate the admitted window once per presented frame.  A crowded
 * object therefore flickers fairly instead of the same pool disappearing
 * forever.  The source enemy pool, collision and update order are unchanged.
 */
static void source_enemy_select_and_prime(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u16 non_enemy_required;
    u16 available;
    u16 object_cost;
    u16 selected_capacity;
    u16 selected_count = 0;
    u16 resume;
    u16 frame_prime_failures = 0;
    u8 priority_pass;

    if (frame->enemy_count == 0) {
        source_enemy_rotation_start = 0;
        return;
    }
    if (source_enemy_rotation_start >= frame->enemy_count) {
        source_enemy_rotation_start = 0;
    }
    non_enemy_required = frame->player_structural_oam;
    available = oam_count < SPRITE_LIMIT ?
        (u16)(SPRITE_LIMIT - oam_count) : 0u;
    available = available > non_enemy_required ?
        (u16)(available - non_enemy_required) : 0u;
#if TYRIAN_GBA_STRESS_LOADOUT
    object_cost = 1u;
#else
    /* Fragmented Sprite2 canvases can require two 32x16 OBJ entries. */
    object_cost = 2u;
#endif
    selected_capacity = available / object_cost;
    if (selected_capacity > frame->enemy_count) {
        selected_capacity = frame->enemy_count;
    }
    if (selected_capacity < frame->enemy_count) {
        telemetry_enemy_oam_rotation_frames++;
    }
    resume = source_enemy_rotation_start;
    /*
     * The direct boss-bar core wins first, its connected multi-link assembly
     * second, and unrelated enemies third. Each pass is split at the cursor,
     * so no candidate pays a wrap/modulo test. Cache admission happens at
     * selection time and cannot be polluted by low-priority explosions.
     */
    for (
        priority_pass = 0;
        priority_pass < 3 && selected_count < selected_capacity;
        priority_pass++
    ) {
        u8 segment;

        for (
            segment = 0;
            segment < 2 && selected_count < selected_capacity;
            segment++
        ) {
            u16 position = segment == 0 ?
                source_enemy_rotation_start : 0u;
            u16 range_end = segment == 0 ?
                frame->enemy_count : source_enemy_rotation_start;

            for (
                ;
                position < range_end &&
                    selected_count < selected_capacity;
                position++
            ) {
                SourceEnemyOamCandidate *candidate =
                    &frame->enemy[position];
                const OtEnemyDrawCommand *draw =
                    &source_parity_level.enemy_draw[candidate->index];
                bool boss_core =
                    (candidate->flags &
                        SOURCE_ENEMY_CANDIDATE_BOSS_CORE) != 0;
                bool boss_structure =
                    (candidate->flags &
                        SOURCE_ENEMY_CANDIDATE_BOSS_STRUCTURE) != 0;

                if (
                    (priority_pass == 0 && !boss_core) ||
                    (
                        priority_pass == 1 &&
                        (boss_core || !boss_structure)
                    ) ||
                    (priority_pass == 2 && boss_structure)
                ) {
                    continue;
                }
                candidate->flags |= SOURCE_ENEMY_CANDIDATE_SELECTED;
                telemetry_enemy_cache_prime_attempts++;
                candidate->palette = 0;
                /*
                 * Once a large boss owns the compact presentation cache,
                 * admit every visible 32x32 authored enemy to it.  Priority
                 * passes still guarantee boss parts first; spare canvases
                 * prevent unrelated enemies from disappearing merely
                 * because the boss reserved the ordinary 8bpp slots.
                 */
                if (draw->size == 1) {
                    candidate->tile = source_boss_compact_acquire(
                        draw,
                        &candidate->palette
                    );
                    if (candidate->tile != 0xffff) {
                        candidate->flags |=
                            SOURCE_ENEMY_CANDIDATE_BOSS_COMPACT;
                    }
                } else {
                    candidate->tile = 0xffff;
                }
                if (candidate->tile == 0xffff) {
                    candidate->tile = source_enemy_cache_acquire(draw);
                }
                if (candidate->tile == 0xffff) {
                    telemetry_enemy_cache_prime_failures++;
                    frame_prime_failures++;
                }
                selected_count++;
                resume = (u16)(position + 1u);
            }
        }
    }
    if (resume >= frame->enemy_count) {
        resume = (u16)(resume - frame->enemy_count);
    }
    if (selected_capacity < frame->enemy_count) {
        source_enemy_rotation_start = (u8)resume;
    } else if (frame_prime_failures != 0) {
        /* Preserve cache-admission fairness if every OAM candidate fit. */
        source_enemy_rotation_start++;
        if (source_enemy_rotation_start >= frame->enemy_count) {
            source_enemy_rotation_start = 0;
        }
    }
}

static void render_source_enemy_range(
    const SourceEnemyOamRange *range,
    u8 draw_stage
)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u16 position;
    u16 range_end = (u16)(range->first + range->count);
    u8 priority = source_object_priority(draw_stage);

    for (position = range->first; position < range_end; position++) {
        const SourceEnemyOamCandidate *candidate =
            &frame->enemy[position];
        u8 oam_required;

        if (
            (candidate->flags & SOURCE_ENEMY_CANDIDATE_SELECTED) == 0
        ) {
            telemetry_enemy_oam_culls++;
            continue;
        }
        if (candidate->tile == 0xffff) continue;
        oam_required =
            source_enemy_cache_tile_is_split(candidate->tile) ? 2u : 1u;
        if (oam_count + oam_required > SPRITE_LIMIT) {
            telemetry_enemy_oam_culls++;
            continue;
        }
        if (
            (candidate->flags &
                SOURCE_ENEMY_CANDIDATE_BOSS_COMPACT) != 0
        ) {
            put_sprite(
                candidate->screen_x,
                candidate->screen_y,
                candidate->tile,
                candidate->palette,
                ATTR1_SIZE_32,
                priority
            );
        } else {
            render_source_enemy_cached_sprite(
                candidate->screen_x,
                candidate->screen_y,
                candidate->tile,
                priority
            );
        }
    }
}

static u8 source_enemy_shot_oam_cursor EWRAM_BSS;

static void render_source_enemy_shots(
    u8 priority,
    u8 oam_limit,
    u8 fair_rotate
)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u16 position;
    u16 range_end;
    u16 resume;
    u8 start;
    u8 wrapped = 0;
    u8 submitted = 0;

    if (frame->enemy_shot_count == 0) {
        source_enemy_shot_oam_cursor = 0;
        if (fair_rotate) telemetry_projectile_oam_rotation_frames++;
        return;
    }
    if (source_enemy_shot_oam_cursor >= frame->enemy_shot_count) {
        source_enemy_shot_oam_cursor = 0;
    }
    start = fair_rotate ? source_enemy_shot_oam_cursor : 0u;
    position = start;
    range_end = frame->enemy_shot_count;
    resume = start;
    for (;;) {
        for (; position < range_end; position++) {
            const SourceProjectileOamCandidate *candidate =
                &frame->enemy_shot[position];
            const OtEnemyShot *source_shot =
                &source_parity_level.enemy_shot[candidate->index];
            u8 shape_table;
            u16 graphic;
            u16 tile;

#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
            if (
                submitted >= MAX_VISIBLE_ENEMY_SHOTS ||
                oam_count >= oam_limit
            ) {
                telemetry_enemy_shot_oam_culls++;
#if TYRIAN_GBA_STRESS_LOADOUT
                telemetry_projectile_culled_oam_full_before_cache++;
#endif
                goto enemy_shots_done;
            }
            resume = (u16)(position + 1u);
            if (
                (candidate->flags & SOURCE_CANDIDATE_RENDER_VISIBLE) == 0
            ) {
#if TYRIAN_GBA_STRESS_LOADOUT
                telemetry_projectile_culled_offscreen_before_cache++;
#endif
                continue;
            }
#else
            resume = (u16)(position + 1u);
#endif
            shape_table =
                source_shot->sgr >= 500 ?
                    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY :
                    OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
            graphic = (u16)(
                (
                    source_shot->sgr >= 500 ?
                        source_shot->sgr - 500 :
                        source_shot->sgr
                ) + source_shot->animate
            );
            tile = source_projectile_cache_acquire(
                shape_table,
                graphic,
#if TYRIAN_GBA_PROJECTILE_PRECACHE_CULL
                1
#else
                0
#endif
            );
            if (tile == 0xffff) continue;
            put_sprite_8bpp_16(
                candidate->screen_x,
                candidate->screen_y,
                tile,
                priority
            );
            submitted++;
        }
        if (!fair_rotate || wrapped || start == 0) break;
        position = 0;
        range_end = start;
        wrapped = 1;
    }
enemy_shots_done:
    if (fair_rotate) {
        if (resume >= frame->enemy_shot_count) {
            resume = (u16)(resume - frame->enemy_shot_count);
        }
        source_enemy_shot_oam_cursor = (u8)resume;
        telemetry_projectile_oam_rotation_frames++;
    }
}

/* Convert a saturated per-tier demand into the last usable OAM index. */
static u8 source_oam_limit_after_reserve(u16 required)
{
    u16 available = (u16)(SPRITE_LIMIT - oam_count);

    if (required >= available) return oam_count;
    return (u8)(SPRITE_LIMIT - required);
}

static u8 source_detail_oam_height(u16 attr0, u16 attr1)
{
    static const u8 heights[3][4] = {
        { 8, 16, 32, 64 },
        { 8,  8, 16, 32 },
        {16, 32, 32, 64 },
    };
    u8 shape = (u8)((attr0 >> 14) & 3u);
    u8 size = (u8)((attr1 >> 14) & 3u);

    return shape < 3 ? heights[shape][size] : 8;
}

static void source_detail_transform_world_oam(u8 first, u8 last)
{
    u8 index;

    if (!source_detail_vertical_flip) return;
    for (index = first; index < last; index++) {
        OBJATTR *object = &oam_shadow[index];
        s16 y = (s16)(object->attr0 & 0xffu);
        u8 height = source_detail_oam_height(
            object->attr0,
            object->attr1
        );

        if (y >= SCREEN_HEIGHT) y = (s16)(y - 256);
        y = (s16)(SCREEN_HEIGHT - y - height);
        object->attr0 = (u16)(
            (object->attr0 & ~0xffu) | (y & 0xffu)
        );
        object->attr1 ^= OBJ_VFLIP;
    }
}

/*
 * Enemy and player projectiles share one presentation tier.  Rendering the
 * enemy pool first must not let it consume the player's entire OAM window.
 * Split a contended window fairly, alternate the odd entry, and let either
 * side borrow every slot the other side does not request.
 */
static u16 source_enemy_projectile_fair_budget(
    u16 capacity,
    u16 enemy_demand,
    u16 player_demand
)
{
    u16 enemy_share;
    u16 player_share;

    if (capacity == 0 || enemy_demand == 0) return 0;
    if (player_demand == 0) {
        return enemy_demand < capacity ? enemy_demand : capacity;
    }
    if (enemy_demand + player_demand <= capacity) {
        return enemy_demand;
    }

    enemy_share = (u16)(capacity >> 1);
    if (
        (capacity & 1u) != 0 &&
        (telemetry_display_frames & 1u) == 0
    ) {
        enemy_share++;
    }
    player_share = (u16)(capacity - enemy_share);

    if (player_demand < player_share) {
        u16 available = (u16)(capacity - player_demand);

        return enemy_demand < available ? enemy_demand : available;
    }
    if (enemy_demand < enemy_share) return enemy_demand;
    return enemy_share;
}

static void render_source_play_scene(void)
{
    SourceFrameOamCandidates *frame = &source_frame_oam_candidates;
    u8 visible_source_enemies;
    u8 nonstructural_oam_limit = SPRITE_LIMIT;
    u8 effect_oam_limit;
    u8 player_shadows_allowed = 1;
    u8 presentation_pressure;
    u16 structural_oam;
    u16 enemy_projectile_oam;
    u16 player_projectile_oam;
    u16 projectile_oam;
    u16 decorative_oam;
    u16 cosmetic_oam_floor = 0;
    u16 projectile_oam_capacity = 0;
    u16 enemy_projectile_budget = 0;
    u16 scene_oam_demand;

    source_sprite2_l2_begin_frame();
    source_enemy_cache_begin_frame();
    source_projectile_cache_begin_frame();
    render_source_boss_bars();
    source_collect_enemy_oam_candidates();
    (void)source_boss_compact_prepare(frame);
    /* Boss cache ownership is known before HUD Sprite2 admission. */
    render_source_inventory_hud();
    source_collect_non_enemy_oam_candidates();
    source_enemy_select_and_prime();
    visible_source_enemies = frame->enemy_count;
    structural_oam = frame->structural_oam;
    enemy_projectile_oam = frame->enemy_projectile_oam;
    player_projectile_oam = frame->player_projectile_oam;
    projectile_oam = source_oam_demand_add_saturated(
        enemy_projectile_oam,
        player_projectile_oam
    );
    decorative_oam = frame->decorative_oam;
    scene_oam_demand = source_oam_demand_add_saturated(
        oam_count,
        structural_oam
    );
    scene_oam_demand = source_oam_demand_add_saturated(
        scene_oam_demand,
        projectile_oam
    );
    scene_oam_demand = source_oam_demand_add_saturated(
        scene_oam_demand,
        decorative_oam
    );
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    {
        u16 exact_scene_oam_demand = (u16)(
            oam_count +
            frame->structural_oam_exact +
            frame->enemy_projectile_oam_exact +
            frame->player_projectile_oam_exact +
            frame->decorative_oam_exact
        );

        if (exact_scene_oam_demand > telemetry_scene_oam_demand_max) {
            telemetry_scene_oam_demand_max = exact_scene_oam_demand;
        }
    }
#else
    if (scene_oam_demand > telemetry_scene_oam_demand_max) {
        telemetry_scene_oam_demand_max = scene_oam_demand;
    }
#endif
    nonstructural_oam_limit =
        source_oam_limit_after_reserve(structural_oam);
    effect_oam_limit = source_oam_limit_after_reserve(
        source_oam_demand_add_saturated(
            structural_oam,
            projectile_oam
        )
    );
    presentation_pressure = scene_oam_demand > SPRITE_LIMIT;
    if (presentation_pressure) {
        u16 available = oam_count < SPRITE_LIMIT ?
            (u16)(SPRITE_LIMIT - oam_count) : 0u;
        u16 reserved;

        /* Projectile shadows are decorative and yield before primaries. */
        player_shadows_allowed = 0;
        /*
         * Keep a tiny, deliberately flickery cosmetic window. Structural
         * objects still own their full reserve; only overflowing projectiles
         * rotate around these eight entries.
         */
        cosmetic_oam_floor = decorative_oam < 8u ?
            decorative_oam : 8u;
        reserved = source_oam_demand_add_saturated(
            structural_oam,
            cosmetic_oam_floor
        );
        projectile_oam_capacity = available > reserved ?
            (u16)(available - reserved) : 0u;
        enemy_projectile_budget =
            source_enemy_projectile_fair_budget(
                projectile_oam_capacity,
                enemy_projectile_oam,
                player_projectile_oam
            );
        nonstructural_oam_limit = source_oam_limit_after_reserve(
            reserved
        );
    }

    if (!presentation_pressure && !source_boss_compact_active) {
        /* Preserve the exact reverse-PC OBJ order when no culling is needed. */
        render_source_pickup_explosions(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
            effect_oam_limit,
            0
        );
#if !TYRIAN_GBA_STRESS_LOADOUT
        {
            u8 available = source_effect_cache_available_slots();
            u8 budget = available < MAX_VISIBLE_EFFECTS ?
                available : MAX_VISIBLE_EFFECTS;

            if (budget < effect_oam_limit - oam_count) {
                effect_oam_limit = (u8)(oam_count + budget);
            }
        }
#endif
        render_effect_sprites(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
            effect_oam_limit,
            0
        );
        render_legacy_rewards(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
        );
    }
    if (source_parity_level.sky_enemy_over_all) {
        render_source_enemy_range(
            &frame->sky_enemy,
            SOURCE_DRAW_STAGE_SKY_ENEMY_OVER
        );
    }
    if (source_parity_level.top_enemy_over) {
        render_source_enemy_range(
            &frame->top_enemy,
            SOURCE_DRAW_STAGE_TOP_ENEMY_OVER
        );
    }
    {
        u8 enemy_projectile_oam_start = oam_count;
        u8 enemy_projectile_oam_limit = nonstructural_oam_limit;
        u16 enemy_projectile_oam_used;

        if (presentation_pressure) {
            u16 limit = (u16)oam_count + enemy_projectile_budget;

            enemy_projectile_oam_limit = (u8)(
                limit < SPRITE_LIMIT ? limit : SPRITE_LIMIT
            );
        }
        render_source_enemy_shots(
            source_object_priority(SOURCE_DRAW_STAGE_ENEMY_SHOT),
            enemy_projectile_oam_limit,
            presentation_pressure &&
                enemy_projectile_oam > enemy_projectile_budget
        );
        enemy_projectile_oam_used =
            (u16)(oam_count - enemy_projectile_oam_start);
        render_player_sidekicks(
            source_object_priority(SOURCE_DRAW_STAGE_PLAYER)
        );
        render_player_sprite(
            source_object_priority(SOURCE_DRAW_STAGE_PLAYER)
        );
#if !TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER
        {
            u8 player_projectile_oam_limit = nonstructural_oam_limit;
            u16 player_projectile_capacity = player_projectile_oam;

            if (presentation_pressure) {
                u16 limit;

                player_projectile_capacity =
                    projectile_oam_capacity >
                        enemy_projectile_oam_used ?
                    (u16)(
                        projectile_oam_capacity -
                        enemy_projectile_oam_used
                    ) : 0u;
                limit = (u16)oam_count + player_projectile_capacity;
                player_projectile_oam_limit = (u8)(
                    limit < SPRITE_LIMIT ? limit : SPRITE_LIMIT
                );
            }
            render_player_shots(
                source_object_priority(SOURCE_DRAW_STAGE_PLAYER_SHOT),
                player_projectile_oam_limit,
                player_shadows_allowed,
                presentation_pressure &&
                    player_projectile_oam >
                        player_projectile_capacity
            );
        }
#endif
    }
    if (!source_parity_level.top_enemy_over) {
        render_source_enemy_range(
            &frame->top_enemy,
            SOURCE_DRAW_STAGE_TOP_ENEMY
        );
    }
    if (!source_parity_level.sky_enemy_over_all) {
        render_source_enemy_range(
            &frame->sky_enemy,
            SOURCE_DRAW_STAGE_SKY_ENEMY
        );
    }
    /* PC draws ground2 after ground, hence reverse OAM order is 75 then 25. */
    render_source_enemy_range(
        &frame->ground_enemy_75,
        SOURCE_DRAW_STAGE_GROUND_ENEMY
    );
    render_source_enemy_range(
        &frame->ground_enemy_25,
        SOURCE_DRAW_STAGE_GROUND_ENEMY
    );
    if (presentation_pressure && !source_boss_compact_active) {
        u8 first_effect_limit = (u8)(
            oam_count + ((SPRITE_LIMIT - oam_count + 1u) >> 1)
        );

#if !TYRIAN_GBA_STRESS_LOADOUT
        u8 available = source_effect_cache_available_slots();
        u8 budget = available < MAX_VISIBLE_EFFECTS ?
            available : MAX_VISIBLE_EFFECTS;

        if (budget < first_effect_limit - oam_count) {
            first_effect_limit = (u8)(oam_count + budget);
        }
#endif
        /*
         * Gameplay OBJ now own the early OAM indices. Give each explosion
         * implementation a rotating share of the remaining tail; the pickup
         * pool borrows any unused general-effect share.
         */
        render_effect_sprites(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
            first_effect_limit,
            1
        );
        render_source_pickup_explosions(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT),
            SPRITE_LIMIT,
            1
        );
        render_legacy_rewards(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
        );
        telemetry_critical_oam_promotions++;
    } else if (source_boss_compact_active) {
        /*
         * Palette bank 7 and most Sprite2 VRAM belong to the compressed
         * boss during this exceptional scene. Keep collectable legacy
         * rewards, but sacrifice transient explosion presentation first.
         */
        render_legacy_rewards(
            source_object_priority(SOURCE_DRAW_STAGE_EFFECT)
        );
        telemetry_critical_oam_promotions++;
    }
    source_enemy_cache_end_frame();
    source_projectile_cache_end_frame();
    source_sprite2_l2_end_frame();

    telemetry_source_visible_enemies = visible_source_enemies;
    if (
        visible_source_enemies >
        telemetry_source_max_visible_enemies
    ) {
        telemetry_source_max_visible_enemies =
            visible_source_enemies;
    }
}

static void render_game(void)
{
    u8 old_count = previous_oam_count;
    u8 visible_count;
    u8 world_oam_start;

    source_detail_prepare_scene();

    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        gameplay_overlay_prepare_frame();
    } else {
        gameplay_overlay_active = 0;
        gameplay_overlay_upload_pending = 0;
    }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
    /*
     * This render will replace the held scene at the next VBlank.  Its row
     * DMA and VOFS commit together, so old display-only cache references may
     * be released while constructing the newest coalesced window.
     */
    presentation_release_held_window = 1;
#endif
    (void)background_schedule_presentation_window(
        source_camera_offset_y
    );
    source_effect_cache_begin_frame();
    oam_count = 0;
    if (game_state == STATE_GAME_OVER) {
        /*
         * PC draws GAME OVER after the complete gameplay scene. Lower GBA
         * OAM indices win equal-priority ties, so reserve its front-most
         * entries before emitting the translated world in reverse order.
         */
        render_game_over_text();
    }
    if (
        game_state == STATE_PLAY &&
        source_parity_level.display_time > 0
    ) {
        render_secret_level_text();
    }
    if (
        game_state == STATE_PLAY &&
        frontend_demo_active &&
        source_parity_level.display_time == 0
    ) {
        render_insert_coin_text();
    }
    if (
        game_paused
#ifdef AUTOTEST_SCREENSHOT_REWARD
        && !autotest_screenshot_delay
#endif
    ) {
        render_pause_text();
    }
    render_cash_counter();
    render_player_energy_status();
    world_oam_start = oam_count;

    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        render_source_play_scene();
    }
    source_detail_transform_world_oam(world_oam_start, oam_count);
    source_effect_cache_end_frame();

    visible_count = oam_count;
    while (oam_count < old_count) {
        oam_shadow[oam_count].attr0 = ATTR0_DISABLED;
        oam_count++;
    }
    previous_oam_count = visible_count;
    if (visible_count > telemetry_max_oam) {
        telemetry_max_oam = visible_count;
    }
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    gameplay_presentation_capture_registers();
#endif
    oam_dirty = 1;
}
