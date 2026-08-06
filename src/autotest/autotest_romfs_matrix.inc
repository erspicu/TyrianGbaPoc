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
    ROMFS_MATRIX_FAIL_ITEM_DATABASE = 26,
    ROMFS_MATRIX_FAIL_DATA_CUBE = 27,
    ROMFS_MATRIX_FAIL_SHIP_INFO = 28,
    ROMFS_MATRIX_FAIL_ROUTE_ITEMS = 29,
    ROMFS_MATRIX_FAIL_EPISODE_SCENE = 30,
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
    SPRITE2_RAW_TABLE_COUNT + 1
]
    EWRAM_BSS;
static u16 romfs_matrix_enemy_queue[OT_HDT_ENEMY_COUNT] EWRAM_BSS;
static u16 romfs_matrix_sprite2_pixels[
    OT_SPRITE2_FRAME_PIXELS
] EWRAM_BSS __attribute__((aligned(4)));
static u16 romfs_matrix_enemy_queue_count;
static u32 romfs_matrix_enemy_queue_total;

static void romfs_matrix_reset_item_validation(void)
{
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
    romfs_matrix_enemy_queue_count = 0;
}

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

/*
 * Enemy 201 is deliberately used as a source-identity sentinel.  The stock
 * HDT and Episode 4 embedded item database both contain a valid record at
 * this index, but they describe entirely different objects.  Structural
 * validation alone therefore cannot catch the old "valid but wrong source"
 * regression that produced collidable, invisible Episode 4 enemies.
 */
static void romfs_matrix_validate_item_database_identity(
    RomfsMatrixTelemetry *telemetry,
    u8 episode,
    u16 level,
    u32 *level_failure
)
{
    OtEnemyDefinition enemy;
    u8 expected_shape_bank = episode == 4 ? 31 : 6;
    u16 expected_graphic = episode == 4 ? 1 : 2;
    u8 expected_armor = episode == 4 ? 6 : 254;

    if (
        !ot_data_hdt_enemy_read(201, &enemy) ||
        enemy.shapebank != expected_shape_bank ||
        enemy.egraphic[0] != expected_graphic ||
        enemy.armor != expected_armor
    ) {
        u32 detail = 0xffffffffu;

        if (ot_data_hdt_enemy_read(201, &enemy)) {
            detail =
                ((u32)enemy.shapebank << 24) |
                ((u32)enemy.armor << 16) |
                enemy.egraphic[0];
        }
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_ITEM_DATABASE,
            episode,
            level,
            detail,
            level_failure
        );
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
    if (shape_table == 0) return 1;
    if (shape_table > SPRITE2_RAW_TABLE_COUNT) {
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
    if (
        ot_sprite2_raw_component(shape_table, 1) == 0 ||
        ot_sprite2_raw_component(
            shape_table,
            SPRITE2_RAW_COMPONENTS_PER_TABLE
        ) == 0
    ) {
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
    telemetry->matrix_checksum = romfs_matrix_hash(
        telemetry->matrix_checksum,
        SPRITE2_RAW_COMPONENTS_PER_TABLE * SPRITE2_RAW_COMPONENT_BYTES
    );
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
    if (graphic == 0 || shape_table == 0) return 1;
    if (
        !romfs_matrix_validate_shape_bank(
            telemetry,
            episode,
            level,
            shape_table,
            level_failure
        ) ||
        ot_sprite2_raw_component(shape_table, graphic) == 0 ||
        (
            size == 1 &&
            (
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
                ) == 0
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
                /*
                 * OpenTyrian treats 999 as an authored "remove this
                 * enemy" animation sentinel before attempting a blit.
                 */
                if (enemy.egraphic[graphic_index] == 999) continue;
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
            if (enemy.dgr != 0 && enemy.dgr != 999) {
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
                    if (
                        !map.direct_level &&
                        !map.item_inventory_valid
                    ) {
                        romfs_matrix_fail(
                            telemetry,
                            ROMFS_MATRIX_FAIL_ROUTE_ITEMS,
                            episode,
                            section,
                            map.resolved_section,
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

static void romfs_matrix_validate_frontend_documents(
    RomfsMatrixTelemetry *telemetry
)
{
    /*
     * Episode 4 begins with three deliberately empty marker slots whose
     * following record cannot fit cube_struct.header.  They are never
     * referenced as documents by the stock script.  Validate a real
     * representative document from each file instead of redefining those
     * source placeholders as errors.
     */
    static const u8 representative_cube[OT_EPISODE_COUNT] = {
        1, 1, 1, 4,
    };
    OtDataCube cube;
    OtShipInfo ship_info;
    u8 episode;
    u8 ship;

    for (episode = 1; episode <= OT_EPISODE_COUNT; episode++) {
        u8 cube_index = representative_cube[episode - 1u];

        if (
            !ot_data_cube_read(
                episode,
                cube_index,
                &cube
            ) ||
            cube.title[0] == '\0' ||
            cube.header[0] == '\0' ||
            cube.last_line == 0
        ) {
            romfs_matrix_fail(
                telemetry,
                ROMFS_MATRIX_FAIL_DATA_CUBE,
                episode,
                0,
                cube_index,
                0
            );
            continue;
        }
        telemetry->matrix_checksum = romfs_matrix_hash(
            telemetry->matrix_checksum,
            ((u32)episode << 24) |
                ((u32)cube_index << 16) |
                ((u32)(u8)cube.title[0] << 8) |
                cube.last_line
        );
    }
    for (ship = 1; ship <= OT_SHIP_INFO_COUNT; ship++) {
        if (
            !ot_data_ship_info_read(ship, &ship_info) ||
            ship_info.paragraph[0][0] == '\0' ||
            ship_info.paragraph[1][0] == '\0'
        ) {
            romfs_matrix_fail(
                telemetry,
                ROMFS_MATRIX_FAIL_SHIP_INFO,
                0,
                0,
                ship,
                0
            );
            break;
        }
        telemetry->matrix_checksum = romfs_matrix_hash(
            telemetry->matrix_checksum,
            ((u32)ship << 24) |
                ((u32)(u8)ship_info.paragraph[0][0] << 8) |
                (u8)ship_info.paragraph[1][0]
        );
    }
}

static void romfs_matrix_validate_episode_scenes(
    RomfsMatrixTelemetry *telemetry
)
{
    static const OtEpisodeRouteState normal = {true, false, 1};
    static const OtEpisodeRouteState timed_out = {true, true, 1};
    OtEpisodeSceneReader reader;
    OtEpisodeSceneAction action;
    u8 animation_seen = 0;
    u8 hint_select_seen = 0;
    u8 fallthrough_hint_seen = 0;
    u8 pcx_seen = 0;
    u8 step;
    u32 failure = 0;

    if (
        !ot_data_episode_scene_begin(
            1, 39, FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
        ) ||
        ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
        action.type != OT_EPISODE_SCENE_ACTION_MUSIC ||
        action.value != 8 ||
        ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
        action.type != OT_EPISODE_SCENE_ACTION_PICTURE ||
        action.value != 5 ||
        ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
        action.type != OT_EPISODE_SCENE_ACTION_TEXT ||
        strncmp(action.text[0], "Now that you've", 15) != 0
    ) {
        failure = 1;
    }

    if (
        failure == 0 &&
        (
            !ot_data_episode_scene_begin(
                4, 32, FRONTEND_PLAY_FULL_GAME,
                FRONTEND_DIFFICULTY_NORMAL, &timed_out, &reader
            ) ||
            ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
            action.type != OT_EPISODE_SCENE_ACTION_END_MAP
        )
    ) {
        failure = 2;
    }
    if (
        failure == 0 &&
        (
            !ot_data_episode_scene_begin(
                4, 32, FRONTEND_PLAY_FULL_GAME,
                FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
            ) ||
            ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
            action.type != OT_EPISODE_SCENE_ACTION_PICTURE ||
            action.value != 13
        )
    ) {
        failure = 3;
    }
    if (
        failure == 0 &&
        (
            ot_data_episode_scene_step(&reader, 0, &action) != 1 ||
            action.type != OT_EPISODE_SCENE_ACTION_TEXT ||
            action.warning ||
            !action.warning_red ||
            action.text_bank != 7
        )
    ) {
        failure = 18;
    }
    if (
        failure == 0 &&
        !ot_data_episode_scene_begin(
            1, 11, FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
        )
    ) {
        failure = 19;
    }
    for (step = 0; failure == 0 && step < 4; step++) {
        if (ot_data_episode_scene_step(&reader, 0, &action) != 1) {
            failure = 20;
            break;
        }
        if (action.type == OT_EPISODE_SCENE_ACTION_TEXT) break;
    }
    if (
        failure == 0 &&
        (
            action.type != OT_EPISODE_SCENE_ACTION_TEXT ||
            !action.warning ||
            action.warning_red ||
            action.text_bank != 14
        )
    ) {
        failure = 21;
    }

    if (
        failure == 0 &&
        !ot_data_episode_scene_begin(
            3, 13, FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
        )
    ) {
        failure = 4;
    }
    for (step = 0; failure == 0 && step < 24; step++) {
        if (ot_data_episode_scene_step(&reader, 0, &action) != 1) {
            failure = 5;
            break;
        }
        if (action.type == OT_EPISODE_SCENE_ACTION_ANIMATION) {
            animation_seen = 1;
        }
        if (action.type == OT_EPISODE_SCENE_ACTION_SELECT_END_HINT) {
            hint_select_seen = 1;
            break;
        }
    }
    if (failure == 0 && (!animation_seen || !hint_select_seen)) {
        failure = 6;
    }
    if (
        failure == 0 &&
        (
            ot_data_episode_scene_step(&reader, 9, &action) != 1 ||
            action.type != OT_EPISODE_SCENE_ACTION_END_EPISODE ||
            strncmp(action.text[0], "Secret Hint 3-3-3:", 18) != 0
        )
    ) {
        failure = 7;
    }

    if (
        failure == 0 &&
        !ot_data_episode_scene_begin(
            4, 41, FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
        )
    ) {
        failure = 8;
    }
    for (step = 0; failure == 0 && step < 24; step++) {
        if (ot_data_episode_scene_step(&reader, 0, &action) != 1) {
            failure = 9;
            break;
        }
        if (
            action.type == OT_EPISODE_SCENE_ACTION_PICTURE &&
            action.value == 0
        ) {
            pcx_seen = 1;
        }
        if (
            action.type == OT_EPISODE_SCENE_ACTION_END_MAP ||
            action.type == OT_EPISODE_SCENE_ACTION_SELECT_END_HINT
        ) {
            break;
        }
    }
    if (failure == 0 && !pcx_seen) failure = 10;

    if (
        failure == 0 &&
        !ot_data_episode_scene_begin(
            4, 42, FRONTEND_PLAY_FULL_GAME,
            FRONTEND_DIFFICULTY_NORMAL, &normal, &reader
        )
    ) {
        failure = 14;
    }
    for (step = 0; failure == 0 && step < 16; step++) {
        if (ot_data_episode_scene_step(&reader, 0, &action) != 1) {
            failure = 15;
            break;
        }
        if (action.type == OT_EPISODE_SCENE_ACTION_SELECT_END_HINT) {
            fallthrough_hint_seen = 1;
            break;
        }
    }
    if (failure == 0 && !fallthrough_hint_seen) failure = 16;
    if (
        failure == 0 &&
        (
            ot_data_episode_scene_step(&reader, 1, &action) != 1 ||
            action.type != OT_EPISODE_SCENE_ACTION_END_EPISODE ||
            strncmp(action.text[0], "Secret Hint 4-1-1:", 18) != 0
        )
    ) {
        failure = 17;
    }

    if (failure == 0) {
        OtPicReader picture;
        int8_t status;
        u8 steps = 0;

        if (!ot_data_pic_decode_begin(5, &picture)) {
            failure = 22;
        } else {
            do {
                status = ot_data_pic_decode_step(
                    &picture,
                    (u8 *)(void *)&frontend_gameplay_arena,
                    sizeof(frontend_gameplay_arena),
                    4096
                );
                steps++;
            } while (status > 0 && steps < 32u);
            if (status != 0 || steps != 16u) failure = 23;
        }
    }
    if (
        failure == 0 &&
        ot_data_episode_scene_textres_hits() == 0
    ) {
        failure = 24;
    }
    if (failure == 0) {
        OtDataView palette;

        if (!ot_data_pcx_decode(
                "tshp2.pcx",
                (u8 *)(void *)&frontend_gameplay_arena,
                sizeof(frontend_gameplay_arena),
                &palette
            ) || palette.size != OT_PALETTE_BYTES) {
            failure = 11;
        }
    }
    if (failure == 0) {
        /*
         * tyrend.anm is deliberately absent from ROMFS.  Its immutable
         * delta decode and exact 320x200 -> 240x160 projection are verified
         * by gba_anm_builder.py, then linked as one direct-copy stream.
         * Validate the linked spans here so an incremental or hand-edited
         * asset build cannot silently ship truncated ending frames.
         */
        if (
            (u32)(tyrend_gba_frames_end - tyrend_gba_frames) !=
                TYREND_GBA_DATA_BYTES ||
            (u32)(tyrend_gba_palette_end - tyrend_gba_palette) !=
                TYREND_GBA_PALETTE_BYTES
        ) {
            failure = 12;
        } else if (
            TYREND_GBA_FRAME_COUNT == 0 ||
            TYREND_GBA_FRAME_BYTES != FRONTEND_FRAME_BYTES ||
            TYREND_GBA_DATA_CRC32 == 0 ||
            TYREND_GBA_PALETTE_CRC32 == 0
        ) {
            failure = 13;
        }
    }

    if (failure != 0) {
        romfs_matrix_fail(
            telemetry,
            ROMFS_MATRIX_FAIL_EPISODE_SCENE,
            0,
            0,
            failure,
            0
        );
        telemetry->route_failures++;
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
    romfs_matrix_reset_item_validation();
    memset(
        romfs_matrix_shape_validated,
        0,
        sizeof(romfs_matrix_shape_validated)
    );
    romfs_matrix_enemy_queue_total = 0;
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
    romfs_matrix_validate_frontend_documents(&telemetry);
    romfs_matrix_validate_episode_scenes(&telemetry);

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
                if (level == 1) {
                    romfs_matrix_validate_item_database_identity(
                        &telemetry,
                        episode,
                        level,
                        &level_failure
                    );
                }
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
        /*
         * Episodes 1-3 share tyrian.hdt.  Episode 4 replaces every item,
         * weapon, and enemy definition with the database appended to
         * tyrian4.lvl, so validate the two source domains independently.
         */
        if (episode == 3 || episode == 4) {
            romfs_matrix_validate_enqueued_enemies(&telemetry);
            romfs_matrix_enemy_queue_total +=
                romfs_matrix_enemy_queue_count;
            if (episode != 4) {
                romfs_matrix_reset_item_validation();
            }
        }
    }

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
    sram_write_u32(120, romfs_matrix_enemy_queue_total);
    sram[124] = *(volatile const u8 *)save_type_marker;
    sram_write_u32(6000, telemetry.sprite2_l2_frames);
    sram_write_u32(6004, telemetry.sprite2_l2_pixels);
    sram_write_u32(6008, telemetry.sprite2_l2_filter_frames);
    sram_write_u32(6012, ot_sprite2_raw_catalog_valid() ? 1 : 0);
    sram_write_u32(6016, SPRITE2_RAW_DATA_BYTES);
    sram_write_u32(6020, SPRITE2_RAW_DATA_CRC32);
    sram_write_u32(6024, ot_data_episode_scene_textres_hits());
    for (delay = 0; delay < 10000; delay++) {
        __asm__ volatile("" ::: "memory");
    }
    __asm__ volatile("swi 3");
    for (;;) {
        VBlankIntrWait();
    }
}
#endif
