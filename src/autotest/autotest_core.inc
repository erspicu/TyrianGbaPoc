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
    /*
     * The soft 1:1 sub-viewport reveals the authored 12-pixel margins near
     * the player.  Two additional source enemies can therefore be presented
     * concurrently without changing the gameplay pool or event trace.
     */
    SOURCE_STAGE4_EXPECTED_VISIBLE_ENEMIES = 32,
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

#ifdef AUTOTEST_STACK_CANARY
extern u8 __iheap_start[];
extern u8 __eheap_start[];
extern u8 __eheap_end[];
extern void *fake_heap_high_water;

#define AUTOTEST_STACK_CANARY_WORD 0xD34DB33Fu
#define AUTOTEST_STACK_CANARY_INIT_GUARD_BYTES 64u

static u32 *autotest_stack_canary_end;
static u32 autotest_stack_canary_filled_bytes;

/*
 * Fill only the linker-free IWRAM interval below the currently active C
 * stack.  The extra 64-byte gap protects this initializer's own frame.
 * Stack growth later overwrites the pattern from high addresses downward,
 * making the untouched prefix an exact conservative high-water measure.
 */
static __attribute__((noinline, noclone)) void
autotest_stack_canary_begin(void)
{
    uintptr_t stack_pointer;
    u32 *cursor = (u32 *)(void *)__iheap_start;
    u32 *limit;

    __asm__ volatile("mov %0, sp" : "=r"(stack_pointer));
    if (stack_pointer <= AUTOTEST_STACK_CANARY_INIT_GUARD_BYTES) {
        autotest_stack_canary_end = cursor;
        autotest_stack_canary_filled_bytes = 0;
        return;
    }
    limit = (u32 *)(void *)(
        (stack_pointer - AUTOTEST_STACK_CANARY_INIT_GUARD_BYTES) &
        ~(uintptr_t)3u
    );
    if (limit < cursor) limit = cursor;
    autotest_stack_canary_end = limit;
    autotest_stack_canary_filled_bytes =
        (u32)((u8 *)(void *)limit - __iheap_start);
    while (cursor < limit) {
        *cursor++ = AUTOTEST_STACK_CANARY_WORD;
    }
}

static __attribute__((noinline, noclone)) u32
autotest_stack_canary_remaining(void)
{
    const u32 *cursor = (const u32 *)(const void *)__iheap_start;

    while (
        cursor < autotest_stack_canary_end &&
        *cursor == AUTOTEST_STACK_CANARY_WORD
    ) {
        cursor++;
    }
    return (u32)(
        (const u8 *)(const void *)cursor -
        (const u8 *)(const void *)__iheap_start
    );
}

static u32 autotest_ewram_heap_used(void)
{
    uintptr_t start = (uintptr_t)__eheap_start;
    uintptr_t end = (uintptr_t)__eheap_end;
    uintptr_t high_water = fake_heap_high_water ?
        (uintptr_t)fake_heap_high_water :
        start;

    if (high_water < start || high_water > end) return UINT32_MAX;
    return (u32)(high_water - start);
}

static u32 autotest_ewram_heap_remaining(void)
{
    u32 used = autotest_ewram_heap_used();
    u32 capacity = (u32)(
        (uintptr_t)__eheap_end -
        (uintptr_t)__eheap_start
    );

    return used <= capacity ? capacity - used : 0;
}
#endif

#if !TYRIAN_GBA_STRESS_LOADOUT
/*
 * Cold-start regression for the Upgrade Ship equipment path.  It binds
 * stock HDT rear/sidekick definitions, exercises both rear modes, fires
 * ordinary and OPTION_SHAPES sidekick volleys, then restores the selected
 * campaign/demo loadout before the first authored gameplay tick.
 */
static u8 autotest_source_upgrade_loadout(void)
{
    OtWeaponPortDefinition front_port;
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
    u16 saved_weapon_energy = player_weapon_energy;
    u16 saved_shield_recharge_cost = player_shield_recharge_cost;
    u8 saved_shield = player_shield;
    u8 saved_shield_max = player_shield_max;
    u8 saved_shield_wait = player_shield_wait;
    u8 saved_generator_power = player_generator_power;
    u8 saved_player_alive = player_alive;
    u8 saved_unlimited_weapon_energy =
        autotest_unlimited_weapon_energy;
    u8 pass = 1;
    u8 index;
    u8 active;
    u8 special_frames;
    u8 expected_shots;

    /*
     * This focused probe validates the real energy economy.  Other authored
     * route/golden tests intentionally retain their pre-energy unlimited
     * load generator so level timing remains a stable independent contract.
     */
    autotest_unlimited_weapon_energy = 0;
    pass =
        pass &&
        ot_data_hdt_weapon_port_read(1, &front_port) &&
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
        source_front_weapon_poweruse == front_port.poweruse &&
        source_rear_weapon_valid &&
        source_rear_weapon_poweruse == rear_port.poweruse &&
        source_rear_weapon_hdt_id == rear_port.op[0][0] &&
        source_sidekick[0].valid &&
        source_sidekick[0].weapon_valid &&
        source_sidekick[0].option.weapon == left_option.weapon &&
        source_sidekick[1].valid &&
        source_sidekick[1].weapon_valid &&
        source_sidekick[1].option.weapon == right_option.weapon;

    player_weapon_energy = 900;
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
    player_weapon_energy = 900;
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

    /*
     * tyrian2.c: shield generation consumes power before powerAdd, and
     * shots.c consumes a port's poweruse before searching the shot pool.
     */
    player_alive = 1;
    player_shield = 5;
    player_shield_max = 10;
    player_shield_recharge_cost = 40;
    player_shield_wait = 1;
    player_generator_power = 30;
    player_weapon_energy = 100;
    source_player_energy_update();
    pass =
        pass &&
        player_shield == 6 &&
        player_shield_wait == 15 &&
        player_weapon_energy == 90 &&
        source_weapon_energy_consume(25) &&
        player_weapon_energy == 65 &&
        !source_weapon_energy_consume(66) &&
        player_weapon_energy == 65;

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
    player_weapon_energy = saved_weapon_energy;
    player_shield_recharge_cost = saved_shield_recharge_cost;
    player_shield = saved_shield;
    player_shield_max = saved_shield_max;
    player_shield_wait = saved_shield_wait;
    player_generator_power = saved_generator_power;
    player_alive = saved_player_alive;
    autotest_unlimited_weapon_energy =
        saved_unlimited_weapon_energy;
    source_front_weapon_init();
    (void)source_option_projectile_sync();
    return pass;
}
#endif
