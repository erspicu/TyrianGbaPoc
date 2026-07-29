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

#if !TYRIAN_GBA_STRESS_LOADOUT
/*
 * Cold-start regression for the Upgrade Ship equipment path.  It binds
 * stock HDT rear/sidekick definitions, exercises both rear modes, fires
 * ordinary and OPTION_SHAPES sidekick volleys, then restores the selected
 * campaign/demo loadout before the first authored gameplay tick.
 */
static u8 autotest_source_upgrade_loadout(void)
{
    OtWeaponPortDefinition rear_port;
    OtOptionDefinition left_option;
    OtOptionDefinition right_option;
    OtOptionDefinition plasma_option;
    u8 saved_front_id =
        source_parity_level.player_front_weapon_id;
    u8 saved_front_power =
        source_parity_level.player_front_weapon_power;
    u8 saved_rear_id =
        source_parity_level.player_rear_weapon_id;
    u8 saved_rear_power =
        source_parity_level.player_rear_weapon_power;
    u8 saved_weapon_mode =
        source_parity_level.player_weapon_mode;
    u8 saved_sidekick[2] = {
        source_parity_level.player_sidekick[0],
        source_parity_level.player_sidekick[1]
    };
    u16 saved_pad_now = pad_now;
    u16 saved_pad_pressed = pad_pressed;
    u8 pass = 1;
    u8 index;
    u8 active;
    u8 special_frames;
    u8 expected_shots;

    pass =
        pass &&
        ot_data_hdt_weapon_port_read(10, &rear_port) &&
        rear_port.opnum == 2 &&
        ot_data_hdt_option_read(19, &left_option) &&
        ot_data_hdt_option_read(20, &right_option) &&
        ot_data_hdt_option_read(9, &plasma_option);
    if (!pass) goto restore;

    clear_entities();
    source_parity_level.player_front_weapon_id = 1;
    source_parity_level.player_front_weapon_power = 1;
    source_parity_level.player_rear_weapon_id = 10;
    source_parity_level.player_rear_weapon_power = 1;
    source_parity_level.player_weapon_mode = 1;
    source_parity_level.player_sidekick[0] = 19;
    source_parity_level.player_sidekick[1] = 20;
    source_front_weapon_init();
    source_sidekick_positions_update();
    expected_shots = (u8)(
        source_front_weapon.multi +
        source_rear_weapon.multi +
        source_sidekick[0].weapon.multi +
        source_sidekick[1].weapon.multi
    );
    pass =
        pass &&
        source_front_weapon_valid &&
        source_rear_weapon_valid &&
        source_rear_weapon_hdt_id == rear_port.op[0][0] &&
        source_sidekick[0].valid &&
        source_sidekick[0].weapon_valid &&
        source_sidekick[0].option.weapon == left_option.weapon &&
        source_sidekick[1].valid &&
        source_sidekick[1].weapon_valid &&
        source_sidekick[1].option.weapon == right_option.weapon;

    pad_now = KEY_A | KEY_B | KEY_R;
    pad_pressed = 0;
    source_player_weapons_update();
    active = 0;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active++;
    }
    pass = pass && active == expected_shots;

    pad_now = 0;
    pad_pressed = KEY_L;
    source_player_weapons_update();
    pass =
        pass &&
        source_parity_level.player_weapon_mode == 2 &&
        source_rear_weapon_hdt_id == rear_port.op[1][0];

    clear_entities();
    source_parity_level.player_sidekick[0] = 9;
    source_parity_level.player_sidekick[1] = 0;
    source_front_weapon_init();
    source_sidekick_positions_update();
    pad_now = KEY_B;
    pad_pressed = 0;
    source_player_weapons_update();
    active = 0;
    special_frames = 0;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (!player_shots[index].active) continue;
        active++;
        if (
            player_shots[index].render_graphic >= 60023 &&
            player_shots[index].render_graphic <= 60025
        ) {
            special_frames |= (u8)(
                1u <<
                    (player_shots[index].render_graphic - 60023u)
            );
        }
    }
    pass =
        pass &&
        plasma_option.weapon == 88 &&
        source_sidekick[0].weapon_valid &&
        source_sidekick[0].ammo ==
            (u8)(plasma_option.ammo - 1u) &&
        active == 3 &&
        special_frames == 7 &&
        source_option_projectile_valid == 7;

restore:
    clear_entities();
    source_parity_level.player_front_weapon_id = saved_front_id;
    source_parity_level.player_front_weapon_power = saved_front_power;
    source_parity_level.player_rear_weapon_id = saved_rear_id;
    source_parity_level.player_rear_weapon_power = saved_rear_power;
    source_parity_level.player_weapon_mode = saved_weapon_mode;
    source_parity_level.player_sidekick[0] = saved_sidekick[0];
    source_parity_level.player_sidekick[1] = saved_sidekick[1];
    memset(
        source_parity_level.frame_sound_queue,
        0,
        sizeof(source_parity_level.frame_sound_queue)
    );
    pad_now = saved_pad_now;
    pad_pressed = saved_pad_pressed;
    source_front_weapon_init();
    (void)source_option_projectile_sync();
    return pass;
}
#endif
