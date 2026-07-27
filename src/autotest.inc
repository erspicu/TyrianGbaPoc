/* Deterministic mGBA regression harness, excluded from release builds. */

#ifdef AUTOTEST
enum {
    /*
     * Deterministic first-level run through the authored boss group and
     * OpenTyrian end-level flight with the stock Port 1 Pulse-Cannon at
     * power 11.  The stronger source weapon legitimately clears the boss
     * earlier and destroys more authored enemies than the old power-1
     * regression loadout.
    */
    SOURCE_STAGE4_EXPECTED_EVENTS = 892,
    SOURCE_STAGE4_EXPECTED_APPLIED = 888,
    SOURCE_STAGE4_EXPECTED_DEFERRED = 0,
    SOURCE_STAGE4_EXPECTED_SKIPPED = 4,
    SOURCE_STAGE4_EXPECTED_SPAWN_ATTEMPTS = 473,
    SOURCE_STAGE4_EXPECTED_SPAWN_SUCCESSES = 473,
    SOURCE_STAGE4_EXPECTED_POOL_FULL = 0,
    SOURCE_STAGE4_EXPECTED_CONTROL_WRITES = 3413,
    SOURCE_STAGE4_EXPECTED_RNG_CALLS = 1168,
    SOURCE_STAGE4_EXPECTED_MOTION_UPDATES = 39885,
    SOURCE_STAGE4_EXPECTED_RELEASES = 497,
    SOURCE_STAGE4_EXPECTED_SHOT_TRIGGERS = 86,
    SOURCE_STAGE4_EXPECTED_SHOT_SPAWNS = 86,
    SOURCE_STAGE4_EXPECTED_SHOT_MAX_ACTIVE = 8,
    SOURCE_STAGE4_EXPECTED_SHOT_UPDATES = 5198,
    SOURCE_STAGE4_EXPECTED_SHOT_RELEASES = 84,
    SOURCE_STAGE4_EXPECTED_SHOT_PLAYER_HITS = 12,
    SOURCE_STAGE4_EXPECTED_PLAYER_SHOT_HITS = 1073,
    SOURCE_STAGE4_EXPECTED_PLAYER_CONTACTS = 62,
    SOURCE_STAGE4_EXPECTED_ENEMY_KILLS = 298,
    SOURCE_STAGE4_EXPECTED_DIRECT_CASH = 4123,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_SPAWNS = 22,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS = 9,
    SOURCE_STAGE4_EXPECTED_SCORE_ITEM_MAX_ACTIVE = 4,
    SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS = 21,
    SOURCE_STAGE4_EXPECTED_DEATH_CONTROLS = 32,
    SOURCE_STAGE4_EXPECTED_DEATH_ASSIGNMENTS = 57,
    SOURCE_STAGE4_EXPECTED_VISIBLE_ENEMIES = 30,
    SOURCE_STAGE4_EXPECTED_UNKNOWN_VISUALS = 0,
};

static void sram_write_u32(u32 offset, u32 value)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    sram[offset] = (u8)value;
    sram[offset + 1] = (u8)(value >> 8);
    sram[offset + 2] = (u8)(value >> 16);
    sram[offset + 3] = (u8)(value >> 24);
}

#ifdef AUTOTEST_ROMFS_LEVEL_MATRIX
enum {
    ROMFS_MATRIX_SCHEMA = 2,
    ROMFS_MATRIX_RECORD_OFFSET = 128,
    ROMFS_MATRIX_RECORD_BYTES = 32,
    ROMFS_MATRIX_ROUTE_LIMIT = 128,
    ROMFS_MATRIX_FAIL_EPISODE_COUNT = 1,
    ROMFS_MATRIX_FAIL_LEVEL_SELECT = 2,
    ROMFS_MATRIX_FAIL_LEVEL_INFO = 3,
    ROMFS_MATRIX_FAIL_BACKGROUND_BIND = 4,
    ROMFS_MATRIX_FAIL_BACKGROUND_PREPARE = 5,
    ROMFS_MATRIX_FAIL_ENEMY_POOL = 6,
    ROMFS_MATRIX_FAIL_EVENT_READ = 7,
    ROMFS_MATRIX_FAIL_EVENT_ORDER = 8,
    ROMFS_MATRIX_FAIL_EVENT_ENEMY = 9,
    ROMFS_MATRIX_FAIL_EVENT_SHAPE_BANK = 10,
    ROMFS_MATRIX_FAIL_EVENT_GRAPHIC = 11,
    ROMFS_MATRIX_FAIL_EVENT_MUSIC = 12,
    ROMFS_MATRIX_FAIL_LEVEL_PORT_INIT = 13,
    ROMFS_MATRIX_FAIL_ENEMY_DEFINITION = 14,
    ROMFS_MATRIX_FAIL_ENEMY_GRAPHIC = 15,
    ROMFS_MATRIX_FAIL_WEAPON_DEFINITION = 16,
    ROMFS_MATRIX_FAIL_WEAPON_GRAPHIC = 17,
    ROMFS_MATRIX_FAIL_ROUTE_RESOLVE = 18,
    ROMFS_MATRIX_FAIL_ROUTE_CYCLE = 19,
    ROMFS_MATRIX_FAIL_ROUTE_LEVEL = 20,
    ROMFS_MATRIX_FAIL_ROUTE_MUSIC = 21,
    ROMFS_MATRIX_FAIL_ROUTE_LIMIT = 22,
    ROMFS_MATRIX_FAIL_SPRITE2_PALETTE = 23,
    ROMFS_MATRIX_FAIL_SPRITE2_L2_PARITY = 24,
    ROMFS_MATRIX_FAIL_BACKGROUND_SENTINEL = 25,
};

typedef struct {
    u32 total_levels;
    u32 passed_levels;
    u32 failed_levels;
    u32 total_events;
    u32 total_enemy_pool_entries;
    u32 unknown_events;
    u32 route_checks;
    u32 route_failures;
    u32 first_failure_code;
    u32 first_failure_episode;
    u32 first_failure_level;
    u32 first_failure_detail;
    u32 background_approximations;
    u32 shape_banks_validated;
    u32 sprites_validated;
    u32 enemies_validated;
    u32 weapons_validated;
    u32 level_section_bytes;
    u32 route_checksum;
    u32 matrix_checksum;
    u32 sprite2_l2_frames;
    u32 sprite2_l2_pixels;
    u32 sprite2_l2_filter_frames;
} RomfsMatrixTelemetry;

static u8 romfs_matrix_enemy_queued[OT_HDT_ENEMY_COUNT] EWRAM_BSS;
static u8 romfs_matrix_weapon_validated[OT_HDT_WEAPON_COUNT] EWRAM_BSS;
static u8 romfs_matrix_shape_validated[
    OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY + 1
]
    EWRAM_BSS;
static u16 romfs_matrix_enemy_queue[OT_HDT_ENEMY_COUNT] EWRAM_BSS;
static u16 romfs_matrix_sprite2_pixels[
    OT_SPRITE2_FRAME_PIXELS
] EWRAM_BSS __attribute__((aligned(4)));
static u16 romfs_matrix_enemy_queue_count;

static u32 romfs_matrix_hash(u32 hash, u32 value)
{
    hash ^= value;
    return hash * 16777619u;
}

static void romfs_matrix_fail(
    RomfsMatrixTelemetry *telemetry,
    u32 code,
    u8 episode,
    u16 level,
    u32 detail,
    u32 *level_failure
)
{
    if (telemetry->first_failure_code == 0) {
        telemetry->first_failure_code = code;
        telemetry->first_failure_episode = episode;
        telemetry->first_failure_level = level;
        telemetry->first_failure_detail = detail;
    }
    if (level_failure != 0 && *level_failure == 0) {
        *level_failure = code;
    }
}

static u8 romfs_matrix_enqueue_enemy(
    RomfsMatrixTelemetry *telemetry,
    u8 episode,
    u16 level,
    int32_t enemy_id,
    u32 *level_failure
)
{
    if (enemy_id < 0 || enemy_id >= OT_HDT_ENEMY_COUNT) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_EVENT_ENEMY,
            episode,
            level,
            (u32)enemy_id,
            level_failure
        );
        return 0;
    }
    if (!romfs_matrix_enemy_queued[enemy_id]) {
        romfs_matrix_enemy_queued[enemy_id] = 1;
        romfs_matrix_enemy_queue[
            romfs_matrix_enemy_queue_count++
        ] = (u16)enemy_id;
    }
    return 1;
}

static u8 romfs_matrix_validate_shape_bank(
    RomfsMatrixTelemetry *telemetry,
    u8 episode,
    u16 level,
    u8 shape_table,
    u32 *level_failure
)
{
    OtDataView bank;

    if (shape_table == 0) return 1;
    if (shape_table > OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_EVENT_SHAPE_BANK,
            episode,
            level,
            shape_table,
            level_failure
        );
        return 0;
    }
    if (romfs_matrix_shape_validated[shape_table]) return 1;
    if (!ot_data_comp_shape_bank_view(shape_table, &bank) || bank.size < 2) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_EVENT_SHAPE_BANK,
            episode,
            level,
            shape_table,
            level_failure
        );
        return 0;
    }
    romfs_matrix_shape_validated[shape_table] = 1;
    telemetry->shape_banks_validated++;
    telemetry->matrix_checksum =
        romfs_matrix_hash(telemetry->matrix_checksum, shape_table);
    telemetry->matrix_checksum =
        romfs_matrix_hash(telemetry->matrix_checksum, bank.size);
    return 1;
}

static u8 romfs_matrix_validate_sprite2_l2(
    RomfsMatrixTelemetry *telemetry,
    u8 shape_table,
    u16 graphic,
    u8 size,
    u8 filter
)
{
    const u8 projectile =
        shape_table == OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY ||
        shape_table == OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY;
    const u8 width = projectile ? 16 : OT_SPRITE2_FRAME_WIDTH;
    const u8 height = projectile ? 16 : OT_SPRITE2_FRAME_HEIGHT;
    const u8 tile_columns = projectile ? 2 : 4;
    const u8 *actual = source_sprite2_l2_tiles[0];
    u8 y;

    if (
        !source_enemy_palette_valid ||
        !ot_sprite2_frame_decode(
            shape_table,
            graphic,
            size,
            filter,
            romfs_matrix_sprite2_pixels,
            OT_SPRITE2_FRAME_PIXELS
        ) ||
        !source_sprite2_l2_build(
            0,
            shape_table,
            graphic,
            size,
            filter
        )
    ) {
        return 0;
    }

    for (y = 0; y < height; y++) {
        u8 x;

        for (x = 0; x < width; x++) {
            u16 decoded = projectile ?
                romfs_matrix_sprite2_pixels[
                    (u32)(y + 9u) * OT_SPRITE2_FRAME_WIDTH +
                    x + 10u
                ] :
                romfs_matrix_sprite2_pixels[
                    (u32)y * OT_SPRITE2_FRAME_WIDTH + x
                ];
            u8 expected = decoded == 0 ?
                0 :
                source_enemy_palette_map[(u8)(decoded - 1u)];

            if (
                actual[source_sprite2_tile_offset(
                    x,
                    y,
                    tile_columns
                )] != expected
            ) {
                return 0;
            }
        }
    }
    telemetry->sprite2_l2_frames++;
    telemetry->sprite2_l2_pixels += (u32)width * height;
    if (filter != 0) telemetry->sprite2_l2_filter_frames++;
    return 1;
}

static u8 romfs_matrix_validate_graphic(
    RomfsMatrixTelemetry *telemetry,
    u8 episode,
    u16 level,
    u8 shape_table,
    u16 graphic,
    u8 size,
    u32 failure_code,
    u32 *level_failure
)
{
    OtDataView sprite;

    if (graphic == 0 || shape_table == 0) return 1;
    if (
        !romfs_matrix_validate_shape_bank(
            telemetry,
            episode,
            level,
            shape_table,
            level_failure
        ) ||
        !ot_data_comp_shape_sprite_view(
            shape_table,
            graphic,
            &sprite
        ) ||
        ot_sprite2_raw_component(shape_table, graphic) == 0 ||
        !ot_sprite2_raw_component_matches_rle(
            shape_table,
            graphic,
            romfs_matrix_sprite2_pixels,
            OT_SPRITE2_FRAME_PIXELS
        ) ||
        (
            size == 1 &&
            (
                !ot_data_comp_shape_sprite_view(
                    shape_table,
                    (u16)(graphic + 1u),
                    &sprite
                ) ||
                !ot_data_comp_shape_sprite_view(
                    shape_table,
                    (u16)(graphic + 19u),
                    &sprite
                ) ||
                !ot_data_comp_shape_sprite_view(
                    shape_table,
                    (u16)(graphic + 20u),
                    &sprite
                ) ||
                ot_sprite2_raw_component(
                    shape_table,
                    (u16)(graphic + 1u)
                ) == 0 ||
                ot_sprite2_raw_component(
                    shape_table,
                    (u16)(graphic + 19u)
                ) == 0 ||
                ot_sprite2_raw_component(
                    shape_table,
                    (u16)(graphic + 20u)
                ) == 0 ||
                !ot_sprite2_raw_component_matches_rle(
                    shape_table,
                    (u16)(graphic + 1u),
                    romfs_matrix_sprite2_pixels,
                    OT_SPRITE2_FRAME_PIXELS
                ) ||
                !ot_sprite2_raw_component_matches_rle(
                    shape_table,
                    (u16)(graphic + 19u),
                    romfs_matrix_sprite2_pixels,
                    OT_SPRITE2_FRAME_PIXELS
                ) ||
                !ot_sprite2_raw_component_matches_rle(
                    shape_table,
                    (u16)(graphic + 20u),
                    romfs_matrix_sprite2_pixels,
                    OT_SPRITE2_FRAME_PIXELS
                )
            )
        )
    ) {
        romfs_matrix_fail(
            telemetry,
            failure_code,
            episode,
            level,
            ((u32)shape_table << 16) | graphic,
            level_failure
        );
        return 0;
    }
    if (
        !romfs_matrix_validate_sprite2_l2(
            telemetry,
            shape_table,
            graphic,
            size,
            0
        )
    ) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_SPRITE2_L2_PARITY,
            episode,
            level,
            ((u32)shape_table << 16) | graphic,
            level_failure
        );
        return 0;
    }
    telemetry->sprites_validated++;
    telemetry->matrix_checksum =
        romfs_matrix_hash(
            telemetry->matrix_checksum,
            ((u32)shape_table << 16) | graphic
        );
    return 1;
}

static void romfs_matrix_validate_weapon(
    RomfsMatrixTelemetry *telemetry,
    u16 weapon_id
)
{
    OtWeaponDefinition weapon;
    u8 projectile;

    if (
        weapon_id == 0 ||
        weapon_id >= OT_HDT_WEAPON_COUNT ||
        romfs_matrix_weapon_validated[weapon_id]
    ) {
        return;
    }
    romfs_matrix_weapon_validated[weapon_id] = 1;
    if (
        !ot_data_hdt_weapon_read(weapon_id, &weapon) ||
        weapon.multi > 8 ||
        weapon.max > 8
    ) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_WEAPON_DEFINITION,
            0,
            0,
            weapon_id,
            0
        );
        return;
    }
    telemetry->weapons_validated++;
    for (
        projectile = 0;
        projectile < weapon.multi;
        projectile++
    ) {
        u16 source_graphic = weapon.sg[projectile];
        u8 shape_table;
        u16 graphic;

        if (source_graphic == 0) continue;
        shape_table =
            source_graphic >= 500 ?
                OT_COMP_SHAPE_TABLE_SHOTS_SECONDARY :
                OT_COMP_SHAPE_TABLE_SHOTS_PRIMARY;
        graphic =
            source_graphic >= 500 ?
                (u16)(source_graphic - 500u) :
                source_graphic;
        if (
            !romfs_matrix_validate_graphic(
                telemetry,
                0,
                0,
                shape_table,
                graphic,
                0,
                ROMFS_MATRIX_FAIL_WEAPON_GRAPHIC,
                0
            )
        ) {
            continue;
        }
        if (weapon.weapani > 1) {
            romfs_matrix_validate_graphic(
                telemetry,
                0,
                0,
                shape_table,
                (u16)(graphic + weapon.weapani - 1u),
                0,
                ROMFS_MATRIX_FAIL_WEAPON_GRAPHIC,
                0
            );
        }
    }
}

static void romfs_matrix_validate_enqueued_enemies(
    RomfsMatrixTelemetry *telemetry
)
{
    u16 queue_index = 0;

    while (queue_index < romfs_matrix_enemy_queue_count) {
        OtEnemyDefinition enemy;
        u16 enemy_id = romfs_matrix_enemy_queue[queue_index++];
        u8 graphic_index;
        u8 turret_index;

        if (!ot_data_hdt_enemy_read(enemy_id, &enemy)) {
            romfs_matrix_fail(
                telemetry,
                ROMFS_MATRIX_FAIL_ENEMY_DEFINITION,
                0,
                0,
                enemy_id,
                0
            );
            continue;
        }
        telemetry->enemies_validated++;
        if (enemy.shapebank != 0) {
            romfs_matrix_validate_shape_bank(
                telemetry,
                0,
                0,
                enemy.shapebank,
                0
            );
            for (graphic_index = 0; graphic_index < 20; graphic_index++) {
                romfs_matrix_validate_graphic(
                    telemetry,
                    0,
                    0,
                    enemy.shapebank,
                    enemy.egraphic[graphic_index],
                    enemy.esize,
                    ROMFS_MATRIX_FAIL_ENEMY_GRAPHIC,
                    0
                );
            }
            if (enemy.dgr != 0) {
                romfs_matrix_validate_graphic(
                    telemetry,
                    0,
                    0,
                    enemy.shapebank,
                    enemy.dgr,
                    enemy.esize,
                    ROMFS_MATRIX_FAIL_ENEMY_GRAPHIC,
                    0
                );
            }
        }
        for (turret_index = 0; turret_index < 3; turret_index++) {
            if (enemy.tur[turret_index] > 0 &&
                enemy.tur[turret_index] < 251) {
                romfs_matrix_validate_weapon(
                    telemetry,
                    enemy.tur[turret_index]
                );
            }
        }
        if (enemy.elaunchtype % 1000u != 0) {
            romfs_matrix_enqueue_enemy(
                telemetry,
                0,
                0,
                enemy.elaunchtype % 1000u,
                0
            );
        }
        if (enemy.eenemydie % 1000u != 0) {
            romfs_matrix_enqueue_enemy(
                telemetry,
                0,
                0,
                enemy.eenemydie % 1000u,
                0
            );
        }
    }
}

static void romfs_matrix_scan_event(
    RomfsMatrixTelemetry *telemetry,
    u8 episode,
    u16 level,
    const OtEventRecord *event,
    u32 *level_failure
)
{
    OtDataView song;
    OtMusSongInfo song_info;
    u8 shape_table;
    int16_t offset;

    switch (event->eventtype) {
    case 5:
        if (event->eventdat > 0) {
            romfs_matrix_validate_shape_bank(
                telemetry,
                episode,
                level,
                (u8)event->eventdat,
                level_failure
            );
        }
        if (event->eventdat2 > 0) {
            romfs_matrix_validate_shape_bank(
                telemetry,
                episode,
                level,
                (u8)event->eventdat2,
                level_failure
            );
        }
        if (event->eventdat3 > 0) {
            romfs_matrix_validate_shape_bank(
                telemetry,
                episode,
                level,
                (u8)event->eventdat3,
                level_failure
            );
        }
        if (event->eventdat4 > 0) {
            romfs_matrix_validate_shape_bank(
                telemetry,
                episode,
                level,
                event->eventdat4,
                level_failure
            );
        }
        break;

    case 6:
    case 7:
    case 10:
    case 15:
    case 17:
    case 18:
    case 23:
    case 32:
    case 56:
        romfs_matrix_enqueue_enemy(
            telemetry,
            episode,
            level,
            event->eventdat,
            level_failure
        );
        break;

    case 12:
        for (offset = 0; offset < 4; offset++) {
            romfs_matrix_enqueue_enemy(
                telemetry,
                episode,
                level,
                (int32_t)event->eventdat + offset,
                level_failure
            );
        }
        break;

    case 33:
    case 45:
        if (event->eventdat > 0) {
            romfs_matrix_enqueue_enemy(
                telemetry,
                episode,
                level,
                (u16)event->eventdat % 1000u,
                level_failure
            );
        }
        break;

    case 35:
        if (
            event->eventdat <= 0 ||
            !ot_data_mus_song_read(
                (u16)(event->eventdat - 1),
                &song,
                &song_info
            )
        ) {
            romfs_matrix_fail(
                telemetry,
                ROMFS_MATRIX_FAIL_EVENT_MUSIC,
                episode,
                level,
                (u16)event->eventdat,
                level_failure
            );
        }
        break;

    case 49:
    case 50:
    case 51:
    case 52:
        romfs_matrix_enqueue_enemy(
            telemetry,
            episode,
            level,
            0,
            level_failure
        );
        shape_table = (u8)event->eventdat3;
        romfs_matrix_validate_graphic(
            telemetry,
            episode,
            level,
            shape_table,
            (u16)event->eventdat,
            0,
            ROMFS_MATRIX_FAIL_EVENT_GRAPHIC,
            level_failure
        );
        break;

    default:
        if (event->eventtype == 0 || event->eventtype > 82) {
            telemetry->unknown_events++;
        }
        break;
    }
}

static void romfs_matrix_validate_routes(
    RomfsMatrixTelemetry *telemetry,
    const u16 episode_level_count[OT_EPISODE_COUNT]
)
{
    u8 episode;

    for (episode = 1; episode <= OT_EPISODE_COUNT; episode++) {
        u8 play_mode;

        for (
            play_mode = FRONTEND_PLAY_FULL_GAME;
            play_mode <= FRONTEND_PLAY_ARCADE;
            play_mode++
        ) {
            u8 difficulty;

            for (
                difficulty = FRONTEND_DIFFICULTY_EASY;
                difficulty <= FRONTEND_DIFFICULTY_HARD;
                difficulty++
            ) {
                u16 seen[ROMFS_MATRIX_ROUTE_LIMIT];
                u16 section = 1;
                u16 seen_count = 0;
                u8 completed = 0;

                telemetry->route_checks++;
                while (seen_count < ROMFS_MATRIX_ROUTE_LIMIT) {
                    OtEpisodeMap map;
                    OtEpisodeLevel choice_route[
                        OT_EPISODE_MAP_CHOICE_COUNT
                    ];
                    u16 prior;
                    u8 duplicate = 0;
                    u8 choice;
                    u8 selected_choice;
                    u8 route_failed = 0;

                    for (prior = 0; prior < seen_count; prior++) {
                        if (seen[prior] == section) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (duplicate || section == 0) {
                        romfs_matrix_fail(
                            telemetry,
                            ROMFS_MATRIX_FAIL_ROUTE_CYCLE,
                            episode,
                            section,
                            ((u32)play_mode << 16) | difficulty,
                            0
                        );
                        telemetry->route_failures++;
                        break;
                    }
                    seen[seen_count++] = section;
                    if (
                        !ot_data_episode_map_resolve(
                            episode,
                            section,
                            play_mode,
                            difficulty,
                            &map
                        )
                    ) {
                        romfs_matrix_fail(
                            telemetry,
                            ROMFS_MATRIX_FAIL_ROUTE_RESOLVE,
                            episode,
                            section,
                            ((u32)play_mode << 16) | difficulty,
                            0
                        );
                        telemetry->route_failures++;
                        break;
                    }
                    telemetry->route_checksum =
                        romfs_matrix_hash(
                            telemetry->route_checksum,
                            ((u32)episode << 24) |
                                ((u32)play_mode << 20) |
                                ((u32)difficulty << 16) |
                                map.resolved_section
                        );
                    if (map.episode_complete) {
                        completed = 1;
                        break;
                    }
                    if (
                        map.choice_count == 0 ||
                        map.choice_count >
                            OT_EPISODE_MAP_CHOICE_COUNT
                    ) {
                        romfs_matrix_fail(
                            telemetry,
                            ROMFS_MATRIX_FAIL_ROUTE_LEVEL,
                            episode,
                            section,
                            map.choice_count,
                            0
                        );
                        telemetry->route_failures++;
                        break;
                    }
                    for (
                        choice = 0;
                        choice < map.choice_count;
                        choice++
                    ) {
                        OtDataView song;
                        OtMusSongInfo song_info;
                        OtEpisodeLevel *route =
                            &choice_route[choice];

                        if (
                            !ot_data_episode_level_resolve(
                                episode,
                                map.map_section[choice],
                                play_mode,
                                difficulty,
                                route
                            )
                        ) {
                            romfs_matrix_fail(
                                telemetry,
                                ROMFS_MATRIX_FAIL_ROUTE_RESOLVE,
                                episode,
                                map.map_section[choice],
                                ((u32)play_mode << 16) |
                                    difficulty,
                                0
                            );
                            route_failed = 1;
                            break;
                        }
                        /*
                         * Stock ]G tables can deliberately expose a planet
                         * whose destination is ]Q (for example Episode 1's
                         * END GAME choice).  It is a valid menu branch, not
                         * a malformed playable level.
                         */
                        if (route->episode_complete) {
                            telemetry->route_checksum =
                                romfs_matrix_hash(
                                    telemetry->route_checksum,
                                    ((u32)map.map_planet[choice] << 24) |
                                        ((u32)map.map_section[choice] << 16) |
                                        0x0000ffffu
                                );
                            continue;
                        }
                        if (
                            route->lvl_file_number == 0 ||
                            route->lvl_file_number >
                                episode_level_count[episode - 1]
                        ) {
                            romfs_matrix_fail(
                                telemetry,
                                ROMFS_MATRIX_FAIL_ROUTE_LEVEL,
                                episode,
                                map.map_section[choice],
                                route->lvl_file_number,
                                0
                            );
                            route_failed = 1;
                            break;
                        }
                        if (
                            !ot_data_mus_song_read(
                                route->source_song,
                                &song,
                                &song_info
                            )
                        ) {
                            romfs_matrix_fail(
                                telemetry,
                                ROMFS_MATRIX_FAIL_ROUTE_MUSIC,
                                episode,
                                map.map_section[choice],
                                route->source_song,
                                0
                            );
                            route_failed = 1;
                            break;
                        }
                        telemetry->route_checksum =
                            romfs_matrix_hash(
                                telemetry->route_checksum,
                                ((u32)map.map_planet[choice] << 24) |
                                    ((u32)map.map_section[choice] << 16) |
                                    ((u32)route->lvl_file_number << 8) |
                                    route->source_song
                            );
                    }
                    if (route_failed) {
                        telemetry->route_failures++;
                        break;
                    }
                    selected_choice =
                        play_mode == FRONTEND_PLAY_ARCADE ?
                            (u8)(map.choice_count - 1u) :
                            0;
                    if (
                        choice_route[selected_choice].episode_complete
                    ) {
                        completed = 1;
                        break;
                    }
                    section =
                        choice_route[selected_choice].next_section;
                }
                if (
                    !completed &&
                    seen_count == ROMFS_MATRIX_ROUTE_LIMIT
                ) {
                    romfs_matrix_fail(
                        telemetry,
                        ROMFS_MATRIX_FAIL_ROUTE_LIMIT,
                        episode,
                        section,
                        ((u32)play_mode << 16) | difficulty,
                        0
                    );
                    telemetry->route_failures++;
                }
            }
        }
    }
}

static void autotest_romfs_level_matrix(void)
{
    RomfsMatrixTelemetry telemetry = {0};
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    const OtDataCatalog *catalog = ot_data_catalog();
    u16 episode_level_count[OT_EPISODE_COUNT] = {0};
    u32 record_index = 0;
    u8 episode;
    volatile u32 delay;

    telemetry.matrix_checksum = 2166136261u;
    telemetry.route_checksum = 2166136261u;
    memset(
        romfs_matrix_enemy_queued,
        0,
        sizeof(romfs_matrix_enemy_queued)
    );
    memset(
        romfs_matrix_weapon_validated,
        0,
        sizeof(romfs_matrix_weapon_validated)
    );
    memset(
        romfs_matrix_shape_validated,
        0,
        sizeof(romfs_matrix_shape_validated)
    );
    romfs_matrix_enemy_queue_count = 0;
    if (!source_enemy_palette_setup(SOURCE_LEVEL_PALETTE_INDEX)) {
        romfs_matrix_fail(
            &telemetry,
            ROMFS_MATRIX_FAIL_SPRITE2_PALETTE,
            0,
            0,
            SOURCE_LEVEL_PALETTE_INDEX,
            0
        );
    }

    for (episode = 1; episode <= OT_EPISODE_COUNT; episode++) {
        u16 level_count;
        u16 level;

        if (!ot_data_episode_lvl_count(episode, &level_count)) {
            romfs_matrix_fail(
                &telemetry,
                ROMFS_MATRIX_FAIL_EPISODE_COUNT,
                episode,
                0,
                0,
                0
            );
            continue;
        }
        episode_level_count[episode - 1] = level_count;
        for (level = 1; level <= level_count; level++) {
            OtLevelInfo info;
            OtEventRecord event;
            u32 level_failure = 0;
            u32 approximations = 0;
            u16 enemy_index;
            u16 event_index;
            u16 previous_event_time = 0;
            u32 record_offset =
                ROMFS_MATRIX_RECORD_OFFSET +
                record_index * ROMFS_MATRIX_RECORD_BYTES;

            telemetry.total_levels++;
            if (!ot_data_level_select(episode, level)) {
                romfs_matrix_fail(
                    &telemetry,
                    ROMFS_MATRIX_FAIL_LEVEL_SELECT,
                    episode,
                    level,
                    0,
                    &level_failure
                );
            } else if (!ot_data_level_info(&info)) {
                romfs_matrix_fail(
                    &telemetry,
                    ROMFS_MATRIX_FAIL_LEVEL_INFO,
                    episode,
                    level,
                    0,
                    &level_failure
                );
            } else {
                telemetry.total_events += info.event_count;
                telemetry.total_enemy_pool_entries += info.enemy_count;
                telemetry.level_section_bytes += info.section_bytes;
                telemetry.matrix_checksum =
                    romfs_matrix_hash(
                        telemetry.matrix_checksum,
                        ((u32)episode << 24) |
                            ((u32)level << 16) |
                            info.event_count
                    );
                if (!background_bind_selected_level()) {
                    romfs_matrix_fail(
                        &telemetry,
                        ROMFS_MATRIX_FAIL_BACKGROUND_BIND,
                        episode,
                        level,
                        (u8)info.shape_file,
                        &level_failure
                    );
                } else {
                    if (
                        background_shape_number_for_map_index(0, 72) != 0 ||
                        background_shape_number_for_map_index(1, 71) != 0 ||
                        background_shape_number_for_map_index(2, 70) != 0 ||
                        background_shape_number_for_map_index(2, 71) != 0
                    ) {
                        romfs_matrix_fail(
                            &telemetry,
                            ROMFS_MATRIX_FAIL_BACKGROUND_SENTINEL,
                            episode,
                            level,
                            0x484746u,
                            &level_failure
                        );
                    }
                    if (
                        !background_prepare_layer(
                            0,
                            BG1_INITIAL_SCROLL >> 3
                        ) ||
                        !background_prepare_layer(
                            1,
                            BG2_INITIAL_SCROLL >> 3
                        ) ||
                        !background_prepare_layer(
                            2,
                            BG3_INITIAL_SCROLL >> 3
                        )
                    ) {
                        romfs_matrix_fail(
                            &telemetry,
                            ROMFS_MATRIX_FAIL_BACKGROUND_PREPARE,
                            episode,
                            level,
                            0,
                            &level_failure
                        );
                    }
                }
                approximations = background_cache_approximations;
                telemetry.background_approximations += approximations;

                for (
                    enemy_index = 0;
                    enemy_index < info.enemy_count;
                    enemy_index++
                ) {
                    u16 enemy_id;

                    if (
                        !ot_data_level_enemy_pool_read(
                            enemy_index,
                            &enemy_id
                        )
                    ) {
                        romfs_matrix_fail(
                            &telemetry,
                            ROMFS_MATRIX_FAIL_ENEMY_POOL,
                            episode,
                            level,
                            enemy_index,
                            &level_failure
                        );
                        break;
                    }
                    romfs_matrix_enqueue_enemy(
                        &telemetry,
                        episode,
                        level,
                        enemy_id,
                        &level_failure
                    );
                }
                for (
                    event_index = 0;
                    event_index < info.event_count;
                    event_index++
                ) {
                    if (
                        !ot_data_level_event_read(
                            event_index,
                            &event
                        )
                    ) {
                        romfs_matrix_fail(
                            &telemetry,
                            ROMFS_MATRIX_FAIL_EVENT_READ,
                            episode,
                            level,
                            event_index,
                            &level_failure
                        );
                        break;
                    }
                    if (
                        event_index != 0 &&
                        event.eventtime < previous_event_time
                    ) {
                        romfs_matrix_fail(
                            &telemetry,
                            ROMFS_MATRIX_FAIL_EVENT_ORDER,
                            episode,
                            level,
                            event_index,
                            &level_failure
                        );
                    }
                    previous_event_time = event.eventtime;
                    romfs_matrix_scan_event(
                        &telemetry,
                        episode,
                        level,
                        &event,
                        &level_failure
                    );
                }
                ot_level_port_init(
                    &source_parity_level,
                    FRONTEND_DIFFICULTY_NORMAL,
                    false,
                    false
                );
                if (!source_parity_level.assets_valid) {
                    romfs_matrix_fail(
                        &telemetry,
                        ROMFS_MATRIX_FAIL_LEVEL_PORT_INIT,
                        episode,
                        level,
                        0,
                        &level_failure
                    );
                }
            }
            if (level_failure == 0) {
                telemetry.passed_levels++;
            } else {
                telemetry.failed_levels++;
            }
            sram_write_u32(record_offset, episode);
            sram_write_u32(record_offset + 4, level);
            sram_write_u32(record_offset + 8, level_failure);
            sram_write_u32(
                record_offset + 12,
                level_failure == ROMFS_MATRIX_FAIL_LEVEL_SELECT ||
                    level_failure == ROMFS_MATRIX_FAIL_LEVEL_INFO ?
                    0 :
                    info.enemy_count
            );
            sram_write_u32(
                record_offset + 16,
                level_failure == ROMFS_MATRIX_FAIL_LEVEL_SELECT ||
                    level_failure == ROMFS_MATRIX_FAIL_LEVEL_INFO ?
                    0 :
                    info.event_count
            );
            sram_write_u32(
                record_offset + 20,
                level_failure == ROMFS_MATRIX_FAIL_LEVEL_SELECT ||
                    level_failure == ROMFS_MATRIX_FAIL_LEVEL_INFO ?
                    0 :
                    ((u8)info.map_file |
                        ((u32)(u8)info.shape_file << 8))
            );
            sram_write_u32(record_offset + 24, approximations);
            sram_write_u32(
                record_offset + 28,
                telemetry.matrix_checksum
            );
            record_index++;
        }
    }

    romfs_matrix_validate_enqueued_enemies(&telemetry);
    if (
        !romfs_matrix_validate_sprite2_l2(
            &telemetry,
            1,
            1,
            0,
            0x09
        )
    ) {
        romfs_matrix_fail(
            &telemetry,
            ROMFS_MATRIX_FAIL_SPRITE2_L2_PARITY,
            0,
            0,
            0x00010001u,
            0
        );
    }
    romfs_matrix_validate_routes(
        &telemetry,
        episode_level_count
    );

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'L';
    sram[3] = 'M';
    sram[4] = ROMFS_MATRIX_SCHEMA;
    sram[5] = (
        telemetry.failed_levels == 0 &&
        telemetry.route_failures == 0 &&
        telemetry.first_failure_code == 0 &&
        telemetry_romfs_failures == 0 &&
        catalog != 0 &&
        catalog->initialized
    );
    sram[6] = OT_EPISODE_COUNT;
    sram[7] = (u8)telemetry.total_levels;
    sram_write_u32(8, telemetry.total_levels);
    sram_write_u32(12, telemetry.passed_levels);
    sram_write_u32(16, telemetry.failed_levels);
    sram_write_u32(20, telemetry.total_events);
    sram_write_u32(24, telemetry.total_enemy_pool_entries);
    sram_write_u32(28, telemetry.unknown_events);
    sram_write_u32(32, telemetry.route_checks);
    sram_write_u32(36, telemetry.route_failures);
    sram_write_u32(40, telemetry_romfs_failures);
    sram_write_u32(44, telemetry.first_failure_code);
    sram_write_u32(48, telemetry.first_failure_episode);
    sram_write_u32(52, telemetry.first_failure_level);
    sram_write_u32(56, telemetry.first_failure_detail);
    sram_write_u32(60, telemetry_romfs_manifest_crc32);
    sram_write_u32(64, telemetry.background_approximations);
    sram_write_u32(68, telemetry.shape_banks_validated);
    sram_write_u32(72, telemetry.sprites_validated);
    sram_write_u32(76, telemetry.enemies_validated);
    sram_write_u32(80, telemetry.weapons_validated);
    sram_write_u32(84, telemetry.level_section_bytes);
    sram_write_u32(88, telemetry.route_checksum);
    sram_write_u32(92, telemetry.matrix_checksum);
    sram_write_u32(96, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(100, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(104, telemetry_romfs_entries);
    sram_write_u32(108, telemetry_romfs_image_bytes);
    sram_write_u32(112, telemetry_romfs_payload_bytes);
    sram_write_u32(
        116,
        catalog != 0 ? catalog->raw_bytes_referenced : 0
    );
    sram_write_u32(120, romfs_matrix_enemy_queue_count);
    sram[124] = *(volatile const u8 *)save_type_marker;
    sram_write_u32(6000, telemetry.sprite2_l2_frames);
    sram_write_u32(6004, telemetry.sprite2_l2_pixels);
    sram_write_u32(6008, telemetry.sprite2_l2_filter_frames);
    sram_write_u32(6012, ot_sprite2_raw_catalog_valid() ? 1 : 0);
    sram_write_u32(6016, SPRITE2_RAW_DATA_BYTES);
    sram_write_u32(6020, SPRITE2_RAW_DATA_CRC32);
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
#endif

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static void autotest_full_loadout_stress_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u32 rng_benchmark_start;
    u32 rng_benchmark_elapsed;
    u32 rng_benchmark_sink = 0;
    u16 rng_benchmark_ime;
    u16 rng_benchmark_index;
    u8 active_shots = 0;
    u8 index;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active_shots++;
    }
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'W';
    sram[3] = '8';
    sram[4] = 1;
    sram[5] =
        stress_loadout_valid &&
        telemetry_stress_loadout_failures == 0;
    sram_write_u32(8, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(12, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(16, telemetry_display_frames);
    sram_write_u32(20, telemetry_logic_updates);
    sram_write_u32(24, telemetry_missed_vblanks);
    sram_write_u32(28, telemetry_max_oam);
    sram_write_u32(32, telemetry_player_shot_spawns);
    sram_write_u32(36, telemetry_player_shot_drops);
    sram_write_u32(40, telemetry_player_shot_max_active);
    sram_write_u32(44, telemetry_player_chain_volleys);
    sram_write_u32(48, telemetry_projectile_cache_drops);
    sram_write_u32(
        52,
        telemetry_projectile_cache_max_visible_unique
    );
    sram_write_u32(56, telemetry_sprite2_cache_drops);
    sram_write_u32(60, telemetry_sprite2_max_visible_unique);
    sram_write_u32(64, telemetry_sprite2_l2_drops);
    sram_write_u32(68, telemetry_effect_cache_drops);
    sram_write_u32(72, telemetry_pickup_explosion_drops);
    sram_write_u32(76, telemetry_pickup_explosion_max_active);
    sram_write_u32(80, telemetry_source_max_visible_enemies);
    sram_write_u32(84, telemetry_source_unknown_visuals);
    sram_write_u32(88, background_cache_approximations);
    sram_write_u32(92, telemetry_stress_loadout_failures);
    sram_write_u32(96, telemetry_vblank_irqs);
    sram_write_u32(100, level_position);
    sram_write_u32(104, active_shots);
    sram_write_u32(108, SOURCE_PROJECTILE_CACHE_SLOT_COUNT);
    sram_write_u32(112, SOURCE_ENEMY_CACHE_SLOT_COUNT);
    sram_write_u32(116, MAX_PLAYER_SHOTS);
    sram_write_u32(120, source_parity_level.assets_valid);
    sram_write_u32(128, telemetry_stress_option_blend_draws);
    sram_write_u32(132, source_option_projectile_valid);
    sram_write_u32(136, telemetry_detail_lava_frames);
    sram_write_u32(140, telemetry_detail_water_frames);
    sram_write_u32(144, telemetry_detail_iced_frames);
    sram_write_u32(148, telemetry_detail_blur_frames);
    sram_write_u32(152, telemetry_detail_wild_frames);
    sram_write_u32(156, telemetry_stress_psg_triggers);
    sram_write_u32(
        160,
        telemetry_projectile_culled_offscreen_before_cache
    );
    sram_write_u32(
        164,
        telemetry_projectile_culled_oam_full_before_cache
    );
    sram_write_u32(
        168,
        telemetry_projectile_post_visibility_acquires
    );
    sram_write_u32(
        172,
        telemetry_projectile_visible_capacity_drops
    );
    sram_write_u32(176, telemetry_stress_logic_cycles_total);
    sram_write_u32(180, telemetry_stress_logic_cycles_max);
    sram_write_u32(184, telemetry_stress_render_cycles_total);
    sram_write_u32(188, telemetry_stress_render_cycles_max);
    sram_write_u32(192, telemetry_stress_collision_cycles_total);
    sram_write_u32(196, telemetry_stress_collision_cycles_max);
    sram_write_u32(
        200,
        TYRIAN_GBA_PROJECTILE_PRECACHE_CULL |
            (TYRIAN_GBA_STRESS_SKIP_PLAYER_COLLISION << 1) |
            (
                TYRIAN_GBA_STRESS_SKIP_PLAYER_PROJECTILE_RENDER <<
                2
            ) |
            (TYRIAN_GBA_PLAYER_SHOT_ACTIVE_MASK << 3) |
            (TYRIAN_GBA_DYNAMIC_FRAME_DROP << 4) |
            (TYRIAN_GBA_WALL_CLOCK_LOGIC << 5) |
            (TYRIAN_GBA_COLLISION_UNSIGNED_RANGE << 6) |
            (TYRIAN_GBA_COLLISION_MASK_FAST_PATH << 7) |
            (TYRIAN_GBA_PRESENTATION_DEFER << 8) |
            (TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER << 9) |
            (TYRIAN_GBA_COLLISION_LAZY_RESULT << 10) |
            (TYRIAN_GBA_COLLISION_PACKED_CALL << 11) |
            (TYRIAN_GBA_RECOVER_MISSED_VBLANK << 12)
    );
    sram_write_u32(204, telemetry_projectile_cache_hits);
    sram_write_u32(208, telemetry_projectile_cache_misses);
    sram_write_u32(212, telemetry_projectile_cache_evictions);
    sram_write_u32(216, telemetry_projectile_cache_uploads);
    sram_write_u32(
        220,
        source_parity_level.player_shot_collision_mask_rebuild_count
    );
    sram_write_u32(
        224,
        source_parity_level.player_shot_collision_candidate_visit_count
    );
    sram_write_u32(
        228,
        source_parity_level.player_shot_collision_linear_slot_visit_count
    );
    sram_write_u32(
        232,
        source_parity_level.player_shot_collision_mask_active
    );
    sram_write_u32(236, telemetry_perf_vblank_irq_cycles_total);
    sram_write_u32(240, telemetry_perf_vblank_irq_cycles_max);
    sram_write_u32(244, telemetry_perf_commit_cycles_total);
    sram_write_u32(248, telemetry_perf_commit_cycles_max);
    sram_write_u32(252, telemetry_perf_audio_input_cycles_total);
    sram_write_u32(256, telemetry_perf_audio_input_cycles_max);
    sram_write_u32(260, telemetry_perf_prelogic_cycles_total);
    sram_write_u32(264, telemetry_perf_prelogic_cycles_max);
    sram_write_u32(268, telemetry_wall_vblanks);
    sram_write_u32(
        272,
        telemetry_presentation_render_attempts
    );
    sram_write_u32(
        276,
        telemetry_presentation_render_completed
    );
    sram_write_u32(
        280,
        telemetry_presentation_render_deferred
    );
    sram_write_u32(
        284,
        telemetry_presentation_render_forced
    );
    sram_write_u32(288, telemetry_presentation_superseded);
    sram_write_u32(
        292,
        telemetry_presentation_pending_logic_max
    );
    sram_write_u32(
        296,
        telemetry_presentation_estimate_max
    );
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP
    sram_write_u32(300, presentation_render_estimate);
    sram_write_u32(324, presentation_render_pending);
#else
    sram_write_u32(300, 0);
    sram_write_u32(324, 0);
#endif
    sram_write_u32(
        304,
        telemetry_presentation_deadline_elapsed_max
    );
    sram_write_u32(308, telemetry_logic_catchup_updates);
    sram_write_u32(
        312,
        telemetry_logic_updates_per_loop_max
    );
    sram_write_u32(316, telemetry_logic_backlog_frames_max);
    sram_write_u32(320, telemetry_background_held_rows_max);
    sram_write_u32(328, telemetry_vblank_recovery_loops);
    sram_write_u32(332, telemetry_audio_frames);
    sram_write_u32(336, source_parity_level.rng_call_count);
    sram_write_u32(
        340,
        source_parity_level.enemy_motion_update_count
    );
    sram_write_u32(
        344,
        source_parity_level.enemy_shot_motion_update_count
    );
    sram_write_u32(
        348,
        ot_level_port_stress_round_ratio_call_count()
    );
    sram_write_u32(
        352,
        source_parity_level.enemy_shot_trigger_count
    );
    sram_write_u32(
        356,
        source_parity_level.enemy_launch_success_count
    );
    /*
     * Measure the generator itself after all gameplay counters have been
     * captured.  IRQs are masked so an audio/VBlank interrupt cannot turn
     * this into an interrupt-latency benchmark; Timer 2/3 keeps counting.
     */
    rng_benchmark_ime = REG_IME;
    REG_IME = 0;
    rng_benchmark_start = stress_cycle_counter_read();
    for (rng_benchmark_index = 0;
         rng_benchmark_index < 10000;
         rng_benchmark_index++) {
        rng_benchmark_sink ^=
            ot_level_port_random(&source_parity_level);
    }
    rng_benchmark_elapsed =
        stress_cycle_counter_read() - rng_benchmark_start;
    REG_IME = rng_benchmark_ime;
    sram_write_u32(360, rng_benchmark_elapsed);
    sram_write_u32(364, rng_benchmark_sink);
    sram_write_u32(368, 10000);
    sram[124] = *(volatile const u8 *)save_type_marker;
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
#endif

#ifdef AUTOTEST_DEATH_FLOW
static void autotest_death_update(void)
{
    static u8 settled_frames;
    const OtDataCatalog *catalog;
    volatile u8 *sram;
    u8 game_over_pass;

    if (++settled_frames < 4) return;

    catalog = ot_data_catalog();
    sram = (volatile u8 *)0x0E000000;
    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'D';
    sram[3] = '2';
    sram_write_u32(
        4,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(8, mmActive());
    sram_write_u32(12, game_state);
    sram_write_u32(16, TYRIAN_GBA_DEV_PLAYER_INVINCIBLE);
    sram_write_u32(20, player_alive);
    sram_write_u32(24, player_exploding_ticks);
    sram_write_u32(28, telemetry_player_death_large_explosions);
    sram_write_u32(32, telemetry_player_death_sfx_9);
    sram_write_u32(36, telemetry_player_death_sfx_11);
    sram_write_u32(40, telemetry_player_death_sfx_22);
    sram_write_u32(44, telemetry_player_death_music_fade_steps);
    sram_write_u32(48, telemetry_game_over_music_starts);
    sram_write_u32(52, telemetry_game_over_overlay_frames);
    sram_write_u32(56, source_parity_level.rng_call_count);
    sram_write_u32(60, game_over_tile_upload_pending);
    sram_write_u32(64, source_parity_level.level_end);
    sram_write_u32(68, frontend_mode4_active);
    sram_write_u32(72, active_effect_count);
    sram_write_u32(76, telemetry_effect_drops);
    sram_write_u32(80, telemetry_display_frames);
    sram_write_u32(84, oam_count);
    game_over_pass = (
        game_state == STATE_GAME_OVER &&
        catalog != 0 &&
        catalog->selected_mus_song == 10 &&
        mmActive() &&
        !TYRIAN_GBA_DEV_PLAYER_INVINCIBLE &&
        !player_alive &&
        player_exploding_ticks == 0 &&
        telemetry_player_death_large_explosions == 120 &&
        telemetry_player_death_sfx_22 == 1 &&
        telemetry_player_death_sfx_9 +
            telemetry_player_death_sfx_11 > 0 &&
        telemetry_player_death_music_fade_steps == 59 &&
        telemetry_game_over_music_starts == 1 &&
        telemetry_game_over_overlay_frames >= 4 &&
        !game_over_tile_upload_pending &&
        source_parity_level.level_end == 0 &&
        !frontend_mode4_active
    );
    sram_write_u32(88, game_over_pass);

#ifdef AUTOTEST_DEATH_CAPTURE
    /*
     * Optional visual harness: stop before the synthetic return key so
     * mGBA -O captures the complete live gameplay/GAME OVER composition.
     */
    __asm__ volatile("swi 3");
#endif

    /*
     * Exercise the source "any input" exit after recording the live Mode-0
     * overlay, then verify that Game Menu restores the title module.
     */
    pad_pressed = KEY_A;
    game_over_update();
    catalog = ot_data_catalog();
    sram_write_u32(
        92,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(96, game_state);
    sram_write_u32(100, frontend_selection);
    sram_write_u32(104, frontend_mode4_active);
    sram_write_u32(108, telemetry_game_over_exits);
    sram_write_u32(112, mmActive());
    sram_write_u32(
        116,
        game_over_pass &&
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_mode4_active &&
        telemetry_game_over_exits == 1 &&
        mmActive()
    );
    __asm__ volatile("swi 3");
}
#endif

#ifdef AUTOTEST_JUKEBOX_FLOW
static u8 autotest_jukebox_entered;

static u16 autotest_jukebox_input(void)
{
    if (
        game_state == STATE_INTRO_LOGO_1 ||
        game_state == STATE_INTRO_LOGO_2
    ) {
        return KEY_A;
    }
    if (game_state == STATE_TITLE) {
        if (autotest_jukebox_entered) return 0;
        return frontend_selection == 0 ? KEY_DOWN : KEY_A;
    }
    if (game_state != STATE_JUKEBOX) return 0;

    autotest_jukebox_entered = 1;
    switch (telemetry_jukebox_frames + 1u) {
    case 24:
        return KEY_LEFT;
    case 48:
        return KEY_RIGHT;
    case 72:
        return KEY_A;
    case 96:
    case 120:
        return KEY_SELECT;
    case 160:
        return KEY_B;
    default:
        return 0;
    }
}

static void autotest_jukebox_maybe_finish(void)
{
    const OtDataCatalog *catalog = ot_data_catalog();
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 passed;

#ifdef AUTOTEST_JUKEBOX_CAPTURE
    if (
        game_state == STATE_JUKEBOX &&
        telemetry_jukebox_frames >= 140
    ) {
        __asm__ volatile("swi 3");
    }
#endif
    if (
        !autotest_jukebox_entered ||
        game_state != STATE_TITLE
    ) {
        return;
    }

    passed = (
        catalog != 0 &&
        catalog->selected_mus_song == 29 &&
        mmActive() &&
        frontend_mode4_active &&
        telemetry_jukebox_frames >= 176 &&
        telemetry_jukebox_track_changes == 3 &&
        telemetry_jukebox_previous_wraps == 1 &&
        telemetry_jukebox_next_wraps == 1 &&
        telemetry_jukebox_text_toggles == 2 &&
        telemetry_jukebox_text_map_commits >= 5 &&
        telemetry_jukebox_palette_commits >= 30 &&
        telemetry_jukebox_max_oam <= HARDWARE_OAM_ENTRIES &&
        telemetry_jukebox_max_oam >= 80 &&
        telemetry_jukebox_exits == 1 &&
        jukebox_song == 1 &&
        !jukebox_hide_text &&
        jukebox_fade_level == 0
    );

    sram[0] = 'T';
    sram[1] = 'G';
    sram[2] = 'J';
    sram[3] = '1';
    sram_write_u32(4, passed);
    sram_write_u32(
        8,
        catalog ? catalog->selected_mus_song : 0xffffu
    );
    sram_write_u32(12, game_state);
    sram_write_u32(16, frontend_mode4_active);
    sram_write_u32(20, telemetry_jukebox_frames);
    sram_write_u32(24, telemetry_jukebox_track_changes);
    sram_write_u32(28, telemetry_jukebox_previous_wraps);
    sram_write_u32(32, telemetry_jukebox_next_wraps);
    sram_write_u32(36, telemetry_jukebox_text_toggles);
    sram_write_u32(40, telemetry_jukebox_text_map_commits);
    sram_write_u32(44, telemetry_jukebox_palette_commits);
    sram_write_u32(48, telemetry_jukebox_max_oam);
    sram_write_u32(52, telemetry_jukebox_exits);
    sram_write_u32(56, mmActive());
    sram_write_u32(60, jukebox_song);
    sram_write_u32(64, jukebox_hide_text);
    sram_write_u32(68, jukebox_quitting);
    sram_write_u32(72, jukebox_fade_level);
    sram_write_u32(76, MSL_NSONGS);
    __asm__ volatile("swi 3");
}
#endif

static void autotest_finish(void)
{
    volatile u8 *sram = (volatile u8 *)0x0E000000;
    u8 pass;
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * A route smoke test deliberately has no level-specific golden counts.
     * It proves that an arbitrary levelsN.dat destination can traverse the
     * common ROMFS-backed gameplay, end-flight and statistics path without
     * missing source data or exhausting a runtime graphics cache.
     */
    pass = (
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_level_completed &&
        frontend_level_ready &&
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
        autotest_campaign_levels_completed ==
            AUTOTEST_CAMPAIGN_LEVEL_COUNT &&
        autotest_campaign_failures == 0 &&
        autotest_campaign_checksum != 2166136261u &&
#endif
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !game_paused &&
        level_position != 0 &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_reward_drops == 0 &&
        telemetry_enemy_replacements == 0 &&
        telemetry_pause_toggles == 2 &&
        telemetry_paused_frames >= 60 &&
        source_parity_level.assets_valid &&
        telemetry_romfs_entries == TYRIAN_ROMFS_ENTRY_COUNT &&
        telemetry_romfs_image_bytes == TYRIAN_ROMFS_IMAGE_BYTES &&
        telemetry_romfs_payload_bytes == TYRIAN_ROMFS_PAYLOAD_BYTES &&
        telemetry_romfs_checks ==
            OT_ROM_IO_SELF_TEST_FIXED_CHECKS +
                11 * TYRIAN_ROMFS_PROBE_COUNT &&
        telemetry_romfs_failures == 0 &&
        telemetry_romfs_manifest_crc32 ==
            TYRIAN_ROMFS_MANIFEST_CRC32 &&
        telemetry_source_events_applied +
            telemetry_source_events_deferred +
            telemetry_source_events_skipped == telemetry_source_events &&
        telemetry_source_spawn_successes +
            telemetry_source_spawn_pool_full +
            telemetry_source_spawn_missing ==
            telemetry_source_spawn_attempts &&
        telemetry_source_spawn_missing == 0 &&
        source_parity_level.enemy_launch_success_count +
            source_parity_level.enemy_launch_pool_full_count +
            source_parity_level.enemy_launch_missing_definition_count ==
            source_parity_level.enemy_launch_attempt_count &&
        source_parity_level.enemy_launch_missing_definition_count == 0 &&
        source_parity_level.random_spawn_success_count +
            source_parity_level.random_spawn_pool_full_count +
            source_parity_level.random_spawn_missing_definition_count ==
            source_parity_level.random_spawn_attempt_count &&
        source_parity_level.random_spawn_missing_definition_count == 0 &&
        source_parity_level.death_spawn_success_count +
            source_parity_level.death_spawn_pool_full_count +
            source_parity_level.death_spawn_missing_definition_count ==
            source_parity_level.death_spawn_attempt_count &&
        source_parity_level.death_spawn_missing_definition_count == 0 &&
        source_parity_level.score_item_unsupported_pickup_count == 0 &&
        telemetry_effect_drops == 0 &&
        telemetry_source_enemy_shot_drops == 0 &&
        telemetry_sprite2_decode_failures == 0 &&
        telemetry_sprite2_cache_drops == 0 &&
        telemetry_sprite2_uploads ==
            telemetry_sprite2_cache_misses &&
        telemetry_sprite2_compact_uploads <=
            telemetry_sprite2_uploads &&
        telemetry_sprite2_upload_bytes ==
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES &&
        telemetry_effect_cache_drops == 0 &&
        telemetry_projectile_cache_drops == 0 &&
        telemetry_layer_rule_failures == 0 &&
        telemetry_end_level_music_starts == 1 &&
        telemetry_level_complete_voice_starts == 1 &&
        telemetry_stats_stage_advances == 4 &&
        frontend_stats_stage == FRONTEND_STATS_STAGE_FINAL
    );
#else
    pass = (
        game_state == STATE_GAME_MENU &&
        frontend_selection == 4 &&
        frontend_level_completed &&
        frontend_mode4_active &&
        !frontend_frame_pending &&
        !game_paused &&
        level_position >= 5400u &&
        telemetry_max_oam <= SPRITE_LIMIT &&
        telemetry_effect_drops <= 33 &&
        telemetry_reward_drops == 0 &&
        telemetry_enemy_replacements == 0 &&
        telemetry_pause_toggles == 2 &&
        telemetry_paused_frames >= 60 &&
        source_parity_level.assets_valid &&
        telemetry_romfs_entries == TYRIAN_ROMFS_ENTRY_COUNT &&
        telemetry_romfs_image_bytes == TYRIAN_ROMFS_IMAGE_BYTES &&
        telemetry_romfs_payload_bytes == TYRIAN_ROMFS_PAYLOAD_BYTES &&
        telemetry_romfs_checks ==
            OT_ROM_IO_SELF_TEST_FIXED_CHECKS +
                11 * TYRIAN_ROMFS_PROBE_COUNT &&
        telemetry_romfs_failures == 0 &&
        telemetry_romfs_manifest_crc32 ==
            TYRIAN_ROMFS_MANIFEST_CRC32 &&
        telemetry_source_events == SOURCE_STAGE4_EXPECTED_EVENTS &&
        telemetry_source_events_applied ==
            SOURCE_STAGE4_EXPECTED_APPLIED &&
        telemetry_source_events_deferred ==
            SOURCE_STAGE4_EXPECTED_DEFERRED &&
        telemetry_source_events_skipped ==
            SOURCE_STAGE4_EXPECTED_SKIPPED &&
        telemetry_source_events_applied +
            telemetry_source_events_deferred +
            telemetry_source_events_skipped == telemetry_source_events &&
        telemetry_source_spawn_attempts ==
            SOURCE_STAGE4_EXPECTED_SPAWN_ATTEMPTS &&
        telemetry_source_spawn_successes +
            telemetry_source_spawn_pool_full +
            telemetry_source_spawn_missing ==
            telemetry_source_spawn_attempts &&
        telemetry_source_spawn_successes ==
            SOURCE_STAGE4_EXPECTED_SPAWN_SUCCESSES &&
        telemetry_source_spawn_pool_full ==
            SOURCE_STAGE4_EXPECTED_POOL_FULL &&
        telemetry_source_spawn_missing == 0 &&
        telemetry_source_control_writes ==
            SOURCE_STAGE4_EXPECTED_CONTROL_WRITES &&
        telemetry_source_rng_calls ==
            SOURCE_STAGE4_EXPECTED_RNG_CALLS &&
        telemetry_source_motion_updates ==
            SOURCE_STAGE4_EXPECTED_MOTION_UPDATES &&
        telemetry_source_releases ==
            SOURCE_STAGE4_EXPECTED_RELEASES &&
        telemetry_source_shot_triggers ==
            SOURCE_STAGE4_EXPECTED_SHOT_TRIGGERS &&
        telemetry_source_max_enemies <= OT_ENEMY_COUNT &&
        telemetry_source_enemy_shots_spawned ==
            SOURCE_STAGE4_EXPECTED_SHOT_SPAWNS &&
        telemetry_source_enemy_shot_drops == 0 &&
        telemetry_source_max_enemy_shots ==
            SOURCE_STAGE4_EXPECTED_SHOT_MAX_ACTIVE &&
        telemetry_source_enemy_shot_updates ==
            SOURCE_STAGE4_EXPECTED_SHOT_UPDATES &&
        telemetry_source_enemy_shot_releases ==
            SOURCE_STAGE4_EXPECTED_SHOT_RELEASES &&
        telemetry_source_enemy_shot_player_hits ==
            SOURCE_STAGE4_EXPECTED_SHOT_PLAYER_HITS &&
        telemetry_source_player_shot_hits ==
            SOURCE_STAGE4_EXPECTED_PLAYER_SHOT_HITS &&
        source_parity_level.player_enemy_contact_count ==
            SOURCE_STAGE4_EXPECTED_PLAYER_CONTACTS &&
        telemetry_source_enemy_kills ==
            SOURCE_STAGE4_EXPECTED_ENEMY_KILLS &&
        telemetry_source_direct_cash ==
            SOURCE_STAGE4_EXPECTED_DIRECT_CASH &&
        telemetry_source_score_item_spawns ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_SPAWNS &&
        telemetry_source_score_item_pickups ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS &&
        telemetry_source_score_item_max_active ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_MAX_ACTIVE &&
        telemetry_source_death_spawn_attempts ==
            SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS &&
        telemetry_source_death_spawn_successes ==
            SOURCE_STAGE4_EXPECTED_DEATH_SPAWNS &&
        source_parity_level.death_spawn_pool_full_count == 0 &&
        source_parity_level.death_spawn_missing_definition_count == 0 &&
        source_parity_level.score_item_unsupported_pickup_count == 0 &&
        source_parity_level.data_cube_pickup_count == 2 &&
        source_parity_level.front_weapon_powerup_count == 0 &&
        source_parity_level.rear_weapon_powerup_count == 0 &&
        telemetry_source_death_control_events ==
            SOURCE_STAGE4_EXPECTED_DEATH_CONTROLS &&
        telemetry_source_death_assignments ==
            SOURCE_STAGE4_EXPECTED_DEATH_ASSIGNMENTS &&
        telemetry_source_max_visible_enemies ==
            SOURCE_STAGE4_EXPECTED_VISIBLE_ENEMIES &&
        telemetry_source_unknown_visuals ==
            SOURCE_STAGE4_EXPECTED_UNKNOWN_VISUALS &&
        telemetry_sprite2_decode_failures == 0 &&
        telemetry_sprite2_cache_drops == 0 &&
        telemetry_sprite2_uploads ==
            telemetry_sprite2_cache_misses &&
        telemetry_sprite2_compact_uploads <=
            telemetry_sprite2_uploads &&
        telemetry_sprite2_upload_bytes ==
            (
                telemetry_sprite2_uploads -
                    telemetry_sprite2_compact_uploads
            ) * SOURCE_ENEMY_FRAME_BYTES +
                telemetry_sprite2_compact_uploads *
                    SOURCE_ENEMY_COMPACT_FRAME_BYTES &&
        telemetry_sprite2_max_uploads <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT &&
        telemetry_sprite2_max_visible_unique <=
            SOURCE_ENEMY_CACHE_SLOT_COUNT &&
        telemetry_effect_cache_drops == 0 &&
        telemetry_effect_cache_uploads ==
            telemetry_effect_cache_misses &&
        telemetry_effect_cache_upload_bytes ==
            telemetry_effect_cache_uploads *
                SOURCE_EFFECT_FRAME_BYTES &&
        telemetry_effect_cache_max_uploads <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_effect_cache_max_visible_unique <=
            SOURCE_EFFECT_CACHE_SLOT_COUNT &&
        telemetry_pickup_explosion_spawns ==
            SOURCE_STAGE4_EXPECTED_SCORE_ITEM_PICKUPS &&
        telemetry_pickup_explosion_drops == 0 &&
        telemetry_pickup_explosion_max_active <=
            MAX_PICKUP_EXPLOSIONS &&
        telemetry_end_level_music_starts == 1 &&
        telemetry_end_level_initial_warp == (u8)-4 &&
        telemetry_end_level_trail_max == 16 &&
        player_end_warp == 27 &&
        telemetry_level_complete_voice_starts == 1 &&
        telemetry_stats_stage_advances == 4 &&
        telemetry_stats_cube_reveals == 2 &&
        frontend_stats_stage == FRONTEND_STATS_STAGE_FINAL &&
        frontend_stats_cube_visible_count == 2 &&
        source_parity_level.enemy_shot_active_count == 2 &&
        source_parity_level.active_enemy_count == 0 &&
        player_cash == 4573 &&
        /*
         * Runtime ROMFS background decoding adds a small, bounded amount of
         * work compared with the former first-level-only generated maps.
         * Keep the performance gate proportional across Low/Normal speed:
         * at least 95 percent of gameplay display frames must meet VBlank.
         */
        telemetry_missed_vblanks * 20u <=
            telemetry_display_frames &&
        telemetry_source_launch_attempts == 3 &&
        telemetry_source_launch_successes == 3 &&
        telemetry_source_random_attempts == 0 &&
        telemetry_source_random_successes == 0 &&
        source_world_to_screen_x(SOURCE_PRESENTATION_X_ORIGIN) == 0 &&
        source_world_to_screen_x(
            SOURCE_PRESENTATION_X_ORIGIN + SCREEN_WIDTH - 1
        ) == SCREEN_WIDTH - 1 &&
        source_world_to_screen_y(SOURCE_PRESENTATION_Y_ORIGIN) == 0 &&
        source_world_to_screen_y(
            SOURCE_PRESENTATION_Y_ORIGIN + SCREEN_HEIGHT - 1
        ) == SCREEN_HEIGHT - 1 &&
        source_background_hofs(0) < 512 &&
        source_background_hofs(1) < 512 &&
        source_background_hofs(2) < 512 &&
        bg1_scroll_speed == 1 &&
        bg2_scroll_speed == 2 &&
        bg3_scroll_speed == 0 &&
        telemetry_layer_rule_checks == 252 &&
        telemetry_layer_rule_failures == 0
    );
#endif
    volatile u32 delay;

    sram[0] = 'T';
    sram[1] = 'G';
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    sram[2] = 'C';
    sram[3] = 'M';
    sram[4] = 3;
#elif defined(AUTOTEST_FRONTEND_ROUTE_SECTION)
    sram[2] = 'R';
    sram[3] = 'S';
    sram[4] = 3;
#else
    sram[2] = 'B';
    sram[3] = 'A';
    sram[4] = 25;
#endif
    sram[5] = pass;
    sram[6] = game_state;
    sram[7] = mmActive() ? 1 : 0;
    /*
     * Keep the standard save-type signature in the ROM so emulators select
     * SRAM before the first telemetry write.  The volatile read prevents
     * section garbage collection from discarding the otherwise metadata-only
     * string.
     */
    sram[64] = *(volatile const u8 *)save_type_marker;
    sram_write_u32(8, telemetry_logic_updates);
    sram_write_u32(12, telemetry_display_frames);
    sram_write_u32(16, telemetry_vblank_irqs);
    sram_write_u32(20, telemetry_missed_vblanks);
    sram_write_u32(24, telemetry_spawn_count);
    sram_write_u32(28, telemetry_control_count);
    sram_write_u32(32, telemetry_collision_count);
    sram_write_u32(36, telemetry_map_rows);
    sram_write_u32(40, telemetry_max_enemies);
    sram_write_u32(44, telemetry_max_oam);
    sram_write_u32(48, telemetry_stream_drops);
    sram_write_u32(52, source_parity_level.event_index);
    sram_write_u32(56, level_tick);
    sram_write_u32(60, telemetry_state_transitions);
    sram_write_u32(64, telemetry_max_effects);
    sram_write_u32(68, telemetry_effect_drops);
    sram_write_u32(72, telemetry_reward_spawns);
    sram_write_u32(76, telemetry_reward_pickups);
    sram_write_u32(80, telemetry_max_rewards);
    sram_write_u32(84, telemetry_reward_drops);
    sram_write_u32(88, player_cash);
    sram_write_u32(92, telemetry_enemy_shots_spawned);
    sram_write_u32(96, telemetry_enemy_shot_drops);
    sram_write_u32(100, telemetry_max_enemy_shots);
    sram_write_u32(104, level_position);
    sram_write_u32(108, telemetry_enemy_replacements);
    sram_write_u32(112, telemetry_kill_cash);
    sram_write_u32(116, telemetry_reward_controls);
    sram_write_u32(120, telemetry_reward_assignments);
    sram_write_u32(124, telemetry_pause_toggles);
    sram_write_u32(128, telemetry_paused_frames);
    sram_write_u32(132, telemetry_source_events);
    sram_write_u32(136, telemetry_source_events_applied);
    sram_write_u32(140, telemetry_source_events_deferred);
    sram_write_u32(144, telemetry_source_events_skipped);
    sram_write_u32(148, telemetry_source_spawn_attempts);
    sram_write_u32(152, telemetry_source_spawn_successes);
    sram_write_u32(156, telemetry_source_spawn_pool_full);
    sram_write_u32(160, telemetry_source_spawn_missing);
    sram_write_u32(164, telemetry_source_max_enemies);
    sram_write_u32(168, telemetry_source_control_writes);
    sram_write_u32(172, telemetry_source_rng_calls);
    sram_write_u32(176, telemetry_romfs_entries);
    sram_write_u32(180, telemetry_romfs_image_bytes);
    sram_write_u32(184, telemetry_romfs_payload_bytes);
    sram_write_u32(188, telemetry_romfs_checks);
    sram_write_u32(192, telemetry_romfs_failures);
    sram_write_u32(196, telemetry_romfs_manifest_crc32);
    sram_write_u32(200, telemetry_source_motion_updates);
    sram_write_u32(204, telemetry_source_releases);
    sram_write_u32(208, telemetry_source_shot_triggers);
    sram_write_u32(212, telemetry_source_launch_attempts);
    sram_write_u32(216, telemetry_source_launch_successes);
    sram_write_u32(220, telemetry_source_random_attempts);
    sram_write_u32(224, telemetry_source_random_successes);
    sram_write_u32(228, telemetry_source_enemy_shots_spawned);
    sram_write_u32(232, telemetry_source_enemy_shot_drops);
    sram_write_u32(236, telemetry_source_max_enemy_shots);
    sram_write_u32(240, telemetry_source_enemy_shot_updates);
    sram_write_u32(244, telemetry_source_enemy_shot_releases);
    sram_write_u32(248, telemetry_source_enemy_shot_player_hits);
    sram_write_u32(252, telemetry_source_player_shot_hits);
    sram_write_u32(256, telemetry_source_enemy_kills);
    sram_write_u32(260, telemetry_source_direct_cash);
    sram_write_u32(264, telemetry_source_score_item_spawns);
    sram_write_u32(268, telemetry_source_score_item_pickups);
    sram_write_u32(272, telemetry_source_score_item_max_active);
    sram_write_u32(276, telemetry_source_death_spawn_attempts);
    sram_write_u32(280, telemetry_source_death_spawn_successes);
    sram_write_u32(284, telemetry_source_death_control_events);
    sram_write_u32(288, telemetry_source_death_assignments);
    sram_write_u32(292, telemetry_source_max_visible_enemies);
    sram_write_u32(296, telemetry_source_unknown_visuals);
    sram_write_u32(
        300,
        source_parity_level.player_enemy_contact_count
    );
    sram_write_u32(
        304,
        source_parity_level.score_item_unsupported_pickup_count
    );
    sram_write_u32(
        308,
        source_parity_level.death_spawn_pool_full_count
    );
    sram_write_u32(
        312,
        source_parity_level.death_spawn_missing_definition_count
    );
    sram_write_u32(316, source_parity_level.active_enemy_count);
    sram_write_u32(
        320,
        source_parity_level.enemy_shot_active_count
    );
    sram_write_u32(324, source_parity_level.data_cube_pickup_count);
    sram_write_u32(
        328,
        source_parity_level.front_weapon_powerup_count
    );
    sram_write_u32(
        332,
        source_parity_level.rear_weapon_powerup_count
    );
    sram_write_u32(336, bg1_scroll_speed);
    sram_write_u32(340, bg2_scroll_speed);
    sram_write_u32(344, bg3_scroll_speed);
    sram_write_u32(348, source_parity_level.assets_valid ? 1 : 0);
    sram_write_u32(352, game_paused);
    sram_write_u32(356, telemetry_sprite2_decode_failures);
    sram_write_u32(360, telemetry_sprite2_cache_hits);
    sram_write_u32(364, telemetry_sprite2_cache_misses);
    sram_write_u32(368, telemetry_sprite2_cache_evictions);
    sram_write_u32(372, telemetry_sprite2_cache_drops);
    sram_write_u32(376, telemetry_sprite2_uploads);
    sram_write_u32(380, telemetry_sprite2_upload_bytes);
    sram_write_u32(384, telemetry_sprite2_max_uploads);
    sram_write_u32(388, SOURCE_ENEMY_CACHE_SLOT_COUNT);
    sram_write_u32(
        392,
        source_parity_level.powerup_consolation_cash
    );
    sram_write_u32(
        396,
        source_parity_level.orbiting_asteroid_pickup_count
    );
    sram_write_u32(400, source_parity_level.superbomb_pickup_count);
    sram_write_u32(404, source_parity_level.hotdog_pickup_count);
    sram_write_u32(408, source_parity_level.armor_pickup_count);
    sram_write_u32(412, source_parity_level.bonus_portal_pickup_count);
    sram_write_u32(416, source_parity_level.high_value_pickup_count);
    sram_write_u32(420, source_parity_level.player_front_weapon_id);
    sram_write_u32(424, source_parity_level.player_front_weapon_power);
    sram_write_u32(428, source_parity_level.player_rear_weapon_id);
    sram_write_u32(432, source_parity_level.player_rear_weapon_power);
    sram_write_u32(436, source_parity_level.player_superbombs);
    sram_write_u32(440, source_parity_level.player_armor);
    sram_write_u32(444, source_parity_level.player_weapon_mode);
    sram_write_u32(448, source_parity_level.player_special);
    sram_write_u32(452, source_parity_level.player_purple_balls_needed);
    sram_write_u32(456, source_parity_level.bonus_level ? 1 : 0);
    sram_write_u32(460, source_parity_level.next_level);
    sram_write_u32(464, source_parity_level.display_time);
    sram_write_u32(468, (u16)player_source_x);
    sram_write_u32(472, (u16)player_source_y);
    sram_write_u32(
        476,
        source_parity_level.presentation_map_x_offset
    );
    sram_write_u32(
        480,
        source_parity_level.presentation_map_x2_offset
    );
    sram_write_u32(
        484,
        source_parity_level.presentation_map_x3_offset
    );
    sram_write_u32(488, source_background_hofs(0));
    sram_write_u32(492, source_background_hofs(1));
    sram_write_u32(496, source_background_hofs(2));
    sram_write_u32(500, bg1_presentation_scroll_pixel);
    sram_write_u32(504, bg2_presentation_scroll_pixel);
    sram_write_u32(508, bg3_presentation_scroll_pixel);
    sram_write_u32(512, SOURCE_PRESENTATION_X_ORIGIN);
    sram_write_u32(516, SOURCE_PRESENTATION_Y_ORIGIN);
    sram_write_u32(520, telemetry_layer_rule_checks);
    sram_write_u32(524, telemetry_layer_rule_failures);
    sram_write_u32(528, source_parity_level.background2_over);
    sram_write_u32(532, source_parity_level.background3_over);
    sram_write_u32(536, source_parity_level.top_enemy_over ? 1 : 0);
    sram_write_u32(
        540,
        source_parity_level.sky_enemy_over_all ? 1 : 0
    );
    sram_write_u32(544, source_background2_priority());
    sram_write_u32(548, source_background3_priority());
    sram_write_u32(552, telemetry_sprite2_max_visible_unique);
    sram_write_u32(556, telemetry_effect_cache_hits);
    sram_write_u32(560, telemetry_effect_cache_misses);
    sram_write_u32(564, telemetry_effect_cache_evictions);
    sram_write_u32(568, telemetry_effect_cache_drops);
    sram_write_u32(572, telemetry_effect_cache_uploads);
    sram_write_u32(576, telemetry_effect_cache_upload_bytes);
    sram_write_u32(580, telemetry_effect_cache_max_uploads);
    sram_write_u32(584, telemetry_effect_cache_max_visible_unique);
    sram_write_u32(588, telemetry_pickup_explosion_spawns);
    sram_write_u32(592, telemetry_pickup_explosion_drops);
    sram_write_u32(596, telemetry_pickup_explosion_max_active);
    sram_write_u32(600, TYRIAN_GBA_DETAIL_LEVEL);
    sram_write_u32(604, TYRIAN_GBA_GAME_SPEED);
    sram_write_u32(608, source_background2_enabled);
    sram_write_u32(612, telemetry_end_level_music_starts);
    sram_write_u32(616, telemetry_end_level_trail_max);
    sram_write_u32(620, telemetry_level_complete_voice_starts);
    sram_write_u32(624, telemetry_stats_stage_advances);
    sram_write_u32(628, telemetry_stats_cube_reveals);
    sram_write_u32(632, frontend_stats_stage);
    sram_write_u32(636, frontend_stats_cube_visible_count);
    sram_write_u32(640, (u8)player_end_warp);
    sram_write_u32(644, telemetry_end_level_initial_warp);
    sram_write_u32(648, background_cache_approximations);
    sram_write_u32(652, background_cache_evictions);
    sram_write_u32(656, background_cache_uploads);
    sram_write_u32(660, background_layer_cache[0].valid_count);
    sram_write_u32(664, background_layer_cache[1].valid_count);
    sram_write_u32(668, background_layer_cache[2].valid_count);
    sram_write_u32(672, background_rows_prefetched);
    sram_write_u32(676, background_rows_synchronous);
    sram_write_u32(680, background_rows_existing);
    sram_write_u32(684, background_prefetch_late_columns);
    sram_write_u32(688, background_tile_render_count);
    sram_write_u32(692, telemetry_projectile_cache_hits);
    sram_write_u32(696, telemetry_projectile_cache_misses);
    sram_write_u32(700, telemetry_projectile_cache_evictions);
    sram_write_u32(704, telemetry_projectile_cache_drops);
    sram_write_u32(708, telemetry_projectile_cache_uploads);
    sram_write_u32(712, telemetry_projectile_cache_max_uploads);
    sram_write_u32(
        716,
        telemetry_projectile_cache_max_visible_unique
    );
    sram_write_u32(6084, telemetry_sprite2_compact_uploads);
    sram_write_u32(6088, telemetry_source_sound_mask_low);
    sram_write_u32(6092, telemetry_source_sound_mask_high);
    sram_write_u32(6112, telemetry_secret_level_collision_pass);
    sram_write_u32(6200, telemetry_boss_perf_started);
    sram_write_u32(6204, telemetry_boss_perf_completed);
    sram_write_u32(6208, telemetry_boss_perf_start_position);
    sram_write_u32(6212, telemetry_boss_perf_end_position);
    sram_write_u32(6216, telemetry_boss_perf_display_frames);
    sram_write_u32(6220, telemetry_boss_perf_missed_vblanks);
    sram_write_u32(6224, telemetry_boss_perf_sprite2_misses);
    sram_write_u32(6228, telemetry_boss_perf_sprite2_evictions);
    sram_write_u32(6232, telemetry_boss_perf_sprite2_upload_bytes);
    sram_write_u32(6236, telemetry_boss_perf_projectile_misses);
    sram_write_u32(6240, telemetry_waitcnt);
    sram_write_u32(6244, telemetry_sprite2_l2_hits);
    sram_write_u32(6248, telemetry_sprite2_l2_misses);
    sram_write_u32(6252, telemetry_sprite2_l2_evictions);
    sram_write_u32(6256, telemetry_sprite2_l2_drops);
    sram_write_u32(6260, telemetry_sprite2_l2_flushes);
    sram_write_u32(6264, telemetry_sprite2_l2_raw_builds);
    sram_write_u32(6268, telemetry_sprite2_l2_rle_fallbacks);
    sram_write_u32(6272, telemetry_sprite2_l2_max_visible_unique);
    sram_write_u32(6276, telemetry_boss_perf_l2_hits);
    sram_write_u32(6280, telemetry_boss_perf_l2_misses);
    sram_write_u32(6284, telemetry_boss_perf_l2_evictions);
    sram_write_u32(6288, telemetry_boss_perf_l2_raw_builds);
    sram_write_u32(6292, telemetry_boss_perf_l2_fallbacks);
    sram_write_u32(6296, ot_sprite2_raw_catalog_valid() ? 1 : 0);
    sram_write_u32(6300, SPRITE2_RAW_DATA_BYTES);
    sram_write_u32(6304, SPRITE2_RAW_DATA_CRC32);
    sram_write_u32(6308, SOURCE_SPRITE2_L2_SLOT_COUNT);
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    sram_write_u32(720, AUTOTEST_FRONTEND_ROUTE_EPISODE);
    sram_write_u32(724, AUTOTEST_FRONTEND_ROUTE_SECTION);
    sram_write_u32(728, frontend_level.lvl_file_number);
    sram_write_u32(732, frontend_level.source_song);
    sram_write_u32(736, source_parity_level.event_count);
    sram_write_u32(
        6000,
        source_enemy_first_decode_failure_valid
    );
    sram_write_u32(
        6004,
        source_enemy_first_decode_failure_shape_table
    );
    sram_write_u32(
        6008,
        source_enemy_first_decode_failure_graphic
    );
    sram_write_u32(6012, source_enemy_first_decode_failure_size);
    sram_write_u32(6016, source_enemy_first_decode_failure_filter);
    sram_write_u32(6020, source_enemy_first_decode_failure_pool);
    sram_write_u32(
        6024,
        source_enemy_first_decode_failure_source_index
    );
    sram_write_u32(6028, telemetry_autotest_combat_assists);
    sram_write_u32(6032, source_parity_level.stop_background_num);
    sram_write_u32(
        6036,
        source_parity_level.stop_backgrounds ? 1 : 0
    );
    sram_write_u32(6040, source_parity_level.frame_enemy_on_screen);
    sram_write_u32(
        6044,
        source_parity_level.frame_ground_enemy_on_screen
    );
    sram_write_u32(
        6048,
        source_parity_level.frame_sky_enemy_on_screen
    );
    sram_write_u32(
        6052,
        source_enemy_first_decode_failure_definition
    );
    sram_write_u32(
        6056,
        source_enemy_first_decode_failure_level_position
    );
    sram_write_u32(
        6060,
        (u16)source_enemy_first_decode_failure_x
    );
    sram_write_u32(
        6064,
        (u16)source_enemy_first_decode_failure_y
    );
    sram_write_u32(
        6068,
        source_enemy_first_decode_failure_shape_banks
    );
    sram_write_u32(6072, telemetry_sprite2_null_pointer_skips);
    sram_write_u32(6076, source_enemy_first_decode_failure_cycle);
    sram_write_u32(6080, telemetry_sprite2_zero_graphic_skips);
    {
        OtEventRecord pending_event = {0};
        u8 active_count = 0;
        u8 enemy_index;

        if (
            ot_data_level_event_read(
                source_parity_level.event_index,
                &pending_event
            )
        ) {
            sram_write_u32(744, pending_event.eventtime);
            sram_write_u32(748, pending_event.eventtype);
            sram_write_u32(752, (u16)pending_event.eventdat);
            sram_write_u32(756, (u16)pending_event.eventdat2);
            sram_write_u32(760, (u16)pending_event.eventdat3);
            sram_write_u32(764, pending_event.eventdat4);
        }
        for (
            enemy_index = 0;
            enemy_index < OT_ENEMY_COUNT;
            enemy_index++
        ) {
            const OtEnemy *enemy = &source_parity_level.enemy[enemy_index];
            u32 record;

            if (source_parity_level.enemy_avail[enemy_index] == 1) {
                continue;
            }
            record = 768u + (u32)active_count * 48u;
            sram_write_u32(record, enemy_index);
            sram_write_u32(
                record + 4u,
                source_parity_level.enemy_avail[enemy_index]
            );
            sram_write_u32(record + 8u, enemy->enemy_definition_id);
            sram_write_u32(record + 12u, enemy->linknum);
            sram_write_u32(record + 16u, enemy->armorleft);
            sram_write_u32(record + 20u, (u16)enemy->ex);
            sram_write_u32(record + 24u, (u16)enemy->ey);
            sram_write_u32(record + 28u, enemy->mapoffset);
            sram_write_u32(record + 32u, (u16)enemy->evalue);
            sram_write_u32(
                record + 36u,
                enemy->enemycycle |
                    ((u32)enemy->scoreitem << 8) |
                    ((u32)enemy->enemyground << 9) |
                    (
                        (u32)source_parity_level
                            .enemy_draw[enemy_index].active <<
                        10
                    )
            );
            sram_write_u32(record + 40u, (u16)enemy->fixedmovey);
            sram_write_u32(
                record + 44u,
                ((u32)(u8)enemy->exc << 24) |
                    ((u32)(u8)enemy->eyc << 16) |
                    ((u32)(u8)enemy->excc << 8) |
                    (u8)enemy->eycc
            );
            active_count++;
        }
        sram_write_u32(740, active_count);
    }
#endif
#ifdef AUTOTEST_CAMPAIGN_LEVEL_COUNT
    {
        u32 record_index;

        sram_write_u32(6096, AUTOTEST_CAMPAIGN_LEVEL_COUNT);
        sram_write_u32(6100, autotest_campaign_levels_completed);
        sram_write_u32(6104, autotest_campaign_failures);
        sram_write_u32(6108, autotest_campaign_checksum);
        for (
            record_index = 0;
            record_index < AUTOTEST_CAMPAIGN_LEVEL_COUNT;
            record_index++
        ) {
            const AutotestCampaignRecord *record =
                &autotest_campaign_record[record_index];
            u32 offset = 6144u + record_index * 36u;

            sram_write_u32(offset, record->episode);
            sram_write_u32(offset + 4u, record->resolved_section);
            sram_write_u32(offset + 8u, record->lvl_file_number);
            sram_write_u32(offset + 12u, record->source_song);
            sram_write_u32(offset + 16u, record->event_index);
            sram_write_u32(offset + 20u, record->level_position);
            sram_write_u32(offset + 24u, record->enemy_kills);
            sram_write_u32(
                offset + 28u,
                record->background_approximations
            );
            sram_write_u32(offset + 32u, record->failure_flags);
        }
    }
#endif
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    /*
     * Always terminate the headless run after telemetry is committed.
     * Host-side verification still rejects pass=0, while a failed invariant
     * now reports immediately instead of presenting as a 30-second timeout.
     */
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}

#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
static u16 autotest_aim_keys(s16 target_x, s16 target_y)
{
    u16 keys = KEY_A;

    if (target_x < SOURCE_PLAYER_MIN_X) target_x = SOURCE_PLAYER_MIN_X;
    if (target_x > SOURCE_PLAYER_MAX_X) target_x = SOURCE_PLAYER_MAX_X;
    if (target_y < SOURCE_PLAYER_MIN_Y) target_y = SOURCE_PLAYER_MIN_Y;
    if (target_y > SOURCE_PLAYER_MAX_Y) target_y = SOURCE_PLAYER_MAX_Y;
    if (player_source_x < target_x - 2) {
        keys |= KEY_RIGHT;
    } else if (player_source_x > target_x + 2) {
        keys |= KEY_LEFT;
    } else if (player_source_velocity_x > 0) {
        keys |= KEY_LEFT;
    } else if (player_source_velocity_x < 0) {
        keys |= KEY_RIGHT;
    }
    if (player_source_y < target_y - 2) {
        keys |= KEY_DOWN;
    } else if (player_source_y > target_y + 2) {
        keys |= KEY_UP;
    } else if (player_source_velocity_y > 0) {
        keys |= KEY_UP;
    } else if (player_source_velocity_y < 0) {
        keys |= KEY_DOWN;
    }
    return keys;
}
#endif

static u16 autotest_input(void)
{
    u8 reward_index;
    u8 source_index;
    u16 phase = (level_tick >> 5) & 3;
    u16 keys = KEY_A;
    s16 player_screen_x = source_player_screen_x();
    s16 player_screen_y = source_player_screen_y();
#ifdef AUTOTEST_FORCE_PLAYER_TOP
    /*
     * Visual boundary regression mode: hold UP until the PC-space clamp
     * reaches the GBA crop-safe upper limit.
     */
    return KEY_UP;
#endif
#ifdef AUTOTEST_STATIONARY_PLAYER
    /*
     * Screenshot regression mode: hold a neutral pose so two frames from
     * different level ticks can be compared without position compensation.
     */
    return 0;
#endif
    if (source_parity_level.boss_bar_link[0] != 0) {
        for (
            source_index = 0;
            source_index < OT_ENEMY_COUNT;
            source_index++
        ) {
            const OtEnemy *enemy =
                &source_parity_level.enemy[source_index];

            if (
                source_parity_level.enemy_avail[source_index] == 1 ||
                enemy->linknum !=
                    source_parity_level.boss_bar_link[0]
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
                ||
                enemy->armorleft == 255 ||
                !source_parity_level.enemy_draw[source_index].active
#endif
            ) {
                continue;
            }
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
            return autotest_aim_keys(
                (s16)(enemy->ex + enemy->mapoffset),
                (s16)(enemy->ey + 80)
            );
#else
            /*
             * Preserve the original first-level golden's deterministic
             * keypad trace.  Generic route-smoke needs the source-space
             * target adapter above, but changing this historical trace
             * changes boss kill timing without changing release gameplay.
             */
            if (player_source_x < enemy->ex - 2) keys |= KEY_RIGHT;
            if (player_source_x > enemy->ex + 2) keys |= KEY_LEFT;
            if (player_source_y < 140) keys |= KEY_DOWN;
            return keys;
#endif
        }
    }
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * Generic non-first-level smoke runs must clear authored event-4 stop
     * groups instead of relying on the first-level movement pattern.  This
     * is test input only; release gameplay has no attraction or aim assist.
     */
    if (source_parity_level.stop_backgrounds) {
        /*
         * Prefer a currently drawn member of the exact source stop pool.
         * Hidden PC special-weapon hitboxes are handled by the separately
         * metered route-smoke combat assist after a grace period.
         */
        for (
            source_index = 0;
            source_index < OT_ENEMY_COUNT;
            source_index++
        ) {
            const OtEnemy *enemy =
                &source_parity_level.enemy[source_index];
            s16 target_x;

            if (
                source_parity_level.enemy_avail[source_index] == 1 ||
                !source_autotest_enemy_matches_stop_pool(source_index) ||
                enemy->scoreitem ||
                enemy->armorleft == 255 ||
                !source_parity_level.enemy_draw[source_index].active
            ) {
                continue;
            }
            target_x = (s16)(enemy->ex + enemy->mapoffset);
            return autotest_aim_keys(
                target_x,
                (s16)(enemy->ey + 80)
            );
        }
    }
#endif
    /*
     * Event-33 data cubes are physical score-item enemies (evalue==1), not
     * destruction awards.  Prioritize them over cash objects so the exact
     * JE_playerCollide() cube branch remains covered with PC inertia.
     */
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 target_x;
        s16 target_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem ||
            enemy->evalue != 1
        ) {
            continue;
        }
        target_x = (s16)(enemy->ex + enemy->mapoffset);
        target_y = enemy->ey;
        if (player_source_x < target_x - 2) keys |= KEY_RIGHT;
        if (player_source_x > target_x + 2) keys |= KEY_LEFT;
        if (player_source_y < target_y - 2) keys |= KEY_DOWN;
        if (player_source_y > target_y + 2) keys |= KEY_UP;
        return keys;
    }
    /*
     * Follow the first authored score item long enough to exercise the
     * physical pickup path. The release build has no attraction or assist.
     */
    for (reward_index = 0; reward_index < MAX_REWARDS; reward_index++) {
        if (!rewards[reward_index].active) continue;
        if (player_screen_x + 16 < rewards[reward_index].x + 8) {
            keys |= KEY_RIGHT;
        }
        if (player_screen_x + 16 > rewards[reward_index].x + 8) {
            keys |= KEY_LEFT;
        }
        if (player_screen_y + 14 < rewards[reward_index].y + 8) {
            keys |= KEY_DOWN;
        }
        if (player_screen_y + 14 > rewards[reward_index].y + 8) {
            keys |= KEY_UP;
        }
        return keys;
    }
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 reward_x;
        s16 reward_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem
        ) {
            continue;
        }
        reward_x = source_score_item_screen_x(enemy);
        reward_y = source_score_item_screen_y(enemy);
        if (player_screen_x + 16 < reward_x + 8) keys |= KEY_RIGHT;
        if (player_screen_x + 16 > reward_x + 8) keys |= KEY_LEFT;
        if (player_screen_y + 14 < reward_y + 8) keys |= KEY_DOWN;
        if (player_screen_y + 14 > reward_y + 8) keys |= KEY_UP;
        return keys;
    }
    if (phase == 0) keys |= KEY_RIGHT;
    if (phase == 1) keys |= KEY_UP;
    if (phase == 2) keys |= KEY_LEFT;
    if (phase == 3) keys |= KEY_DOWN;
    return keys;
}

#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
static u8 autotest_explosion_visible(void)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) continue;
#ifdef AUTOTEST_SCREENSHOT_EXPLOSION_FRAME
        if (effects[index].frame == AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) {
            return 1;
        }
#else
        return 1;
#endif
    }
    return 0;
}
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
static u8 autotest_reward_visible(void)
{
    u8 index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (
            source_parity_level.enemy_draw[index].active &&
            source_parity_level.enemy[index].scoreitem
        ) {
            return 1;
        }
    }
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) return 1;
    }
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        if (pickup_explosions[index].active) return 1;
    }
    return 0;
}
#endif
#endif
