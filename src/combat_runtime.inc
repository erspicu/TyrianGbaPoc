/* Legacy player/enemy combat runtime retained until source-parity replacement. */

static u8 source_weapon_is_valid(const OtWeaponDefinition *weapon)
{
    return
        weapon->multi > 0 &&
        weapon->multi <= 8 &&
        weapon->max > 0 &&
        weapon->max <= 8;
}

/*
 * Direct single-player slice of tyrian2.c's Shield Gen and Power Bar blocks.
 * This runs once before event/enemy/player work, matching the PC game loop.
 */
static void source_player_energy_update(void)
{
    if (source_parity_level.galaga_mode) {
        /*
         * Literal tyrian2.c Galaga block.  Front-weapon power aliases the
         * remaining lives, the shield stays empty, and score thresholds
         * award additional lives up to the source cap.
         */
        player_shield = 0;
        if (player_cash >= source_parity_level.galaga_life_threshold) {
            source_parity_level.frame_sound_queue[6] = 11;
            source_parity_level.frame_sound_queue[7] = 21;
            if (source_parity_level.player_front_weapon_power < 11) {
                source_parity_level.player_front_weapon_power++;
                player_lives =
                    source_parity_level.player_front_weapon_power;
            } else {
                player_cash += 1000;
            }
            if (source_parity_level.galaga_life_threshold == 10000) {
                source_parity_level.galaga_life_threshold = 20000;
            } else {
                source_parity_level.galaga_life_threshold += 25000;
            }
        }
        player_weapon_energy = 900;
        return;
    }
#ifdef AUTOTEST
    if (autotest_unlimited_weapon_energy) {
        player_weapon_energy = 900;
        return;
    }
#endif
#if TYRIAN_GBA_STRESS_LOADOUT
    /* onePlayerAction in the original forces the shared weapon pool to 900. */
    player_weapon_energy = 900;
#else
    if (
        player_alive &&
        player_shield < player_shield_max &&
        player_weapon_energy > player_shield_recharge_cost
    ) {
        if (player_shield_wait == 0) player_shield_wait = 1;
        player_shield_wait--;
        if (player_shield_wait == 0) {
            player_shield_wait = 15;
            player_weapon_energy = (u16)(
                player_weapon_energy - player_shield_recharge_cost
            );
            player_shield++;
        }
    }
    if (source_parity_level.arcade_mode) {
        player_weapon_energy = 900;
    } else {
        player_weapon_energy = (u16)(
            player_weapon_energy + player_generator_power
        );
        if (player_weapon_energy > 900) player_weapon_energy = 900;
    }
#endif
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static const u8 source_weapon_sound_slot[SOURCE_WEAPON_BAY_COUNT] = {
    0, 2, 4, 4, 2, 2, 5, 5, 1, 4, 1
};

static u8 source_weapon_energy_consume(u16 poweruse)
{
#ifdef AUTOTEST
    if (autotest_unlimited_weapon_energy) return 1;
#endif
    if (player_weapon_energy < poweruse) return 0;
    player_weapon_energy = (u16)(player_weapon_energy - poweruse);
    return 1;
}

static u8 source_front_weapon_sync(void)
{
    OtWeaponPortDefinition front_port;
    u8 port_id = source_parity_level.player_front_weapon_id;
    /* mainint.c uses power row zero while galagaMode owns power as lives. */
    u8 power = source_parity_level.galaga_mode ?
        1 :
        source_parity_level.player_front_weapon_power;
    u16 weapon_id;

    if (
        source_front_weapon_bound &&
        source_front_weapon_port_id == port_id &&
        source_front_weapon_power == power
    ) {
        return source_front_weapon_valid;
    }

    source_front_weapon_bound = 1;
    source_front_weapon_port_id = port_id;
    source_front_weapon_power = power;
    source_front_weapon_hdt_id = 0;
    source_front_weapon_poweruse = 0;
    source_front_weapon_valid = 0;
    source_shot_multi_pos[SOURCE_WEAPON_BAY_FRONT] = 0;
    memset(&source_front_weapon, 0, sizeof(source_front_weapon));
    if (
        port_id == 0 ||
        power == 0 ||
        power > 11 ||
        !ot_data_hdt_weapon_port_read(port_id, &front_port)
    ) {
        return 0;
    }

    /*
     * mainint.c JE_mainGamePlayerFunctions():
     * weaponPort[item].op[0][items.weapon[FRONT].power - 1].
     * Front weapons always use mode zero; weapon_mode belongs to the rear
     * port.  Reading this table on state changes preserves every stock power
     * level instead of pinning the adapter to HDT record 165.
     */
    weapon_id = front_port.op[0][power - 1u];
    source_front_weapon_hdt_id = weapon_id;
    source_front_weapon_poweruse = front_port.poweruse;
    source_front_weapon_valid =
        weapon_id != 0 &&
        ot_data_hdt_weapon_read(weapon_id, &source_front_weapon) &&
        source_weapon_is_valid(&source_front_weapon);
    return source_front_weapon_valid;
}

static u8 source_rear_weapon_sync(void)
{
    OtWeaponPortDefinition rear_port;
    u8 port_id = source_parity_level.player_rear_weapon_id;
    u8 power = source_parity_level.player_rear_weapon_power;
    u8 mode = source_parity_level.player_weapon_mode;
    u16 weapon_id;

    if (
        source_rear_weapon_bound &&
        source_rear_weapon_port_id == port_id &&
        source_rear_weapon_power == power &&
        source_rear_weapon_mode == mode
    ) {
        return source_rear_weapon_valid;
    }

    source_rear_weapon_bound = 1;
    source_rear_weapon_port_id = port_id;
    source_rear_weapon_power = power;
    source_rear_weapon_mode = mode;
    source_rear_weapon_hdt_id = 0;
    source_rear_weapon_poweruse = 0;
    source_rear_weapon_valid = 0;
    source_shot_multi_pos[SOURCE_WEAPON_BAY_REAR] = 0;
    memset(&source_rear_weapon, 0, sizeof(source_rear_weapon));
    if (
        port_id == 0 ||
        power == 0 ||
        power > 11 ||
        !ot_data_hdt_weapon_port_read(port_id, &rear_port) ||
        rear_port.opnum == 0
    ) {
        return 0;
    }
    if (mode == 0 || mode > rear_port.opnum || mode > 2) {
        mode = 1;
        source_parity_level.player_weapon_mode = mode;
        source_rear_weapon_mode = mode;
    }
    weapon_id = rear_port.op[mode - 1u][power - 1u];
    source_rear_weapon_hdt_id = weapon_id;
    source_rear_weapon_poweruse = rear_port.poweruse;
    source_rear_weapon_valid =
        weapon_id != 0 &&
        ot_data_hdt_weapon_read(weapon_id, &source_rear_weapon) &&
        source_weapon_is_valid(&source_rear_weapon);
    return source_rear_weapon_valid;
}

static u8 source_sidekick_sync(u8 side)
{
    SourceSidekickRuntime *runtime;
    OtWeaponPortDefinition port;
    u8 item_id;
    u16 weapon_id;

    if (side > 1) return 0;
    runtime = &source_sidekick[side];
    item_id = source_parity_level.player_sidekick[side];
    if (runtime->item_id != item_id) {
        source_shot_multi_pos[
            SOURCE_WEAPON_BAY_LEFT_SIDEKICK + side
        ] = 0;
        memset(runtime, 0, sizeof(*runtime));
        runtime->item_id = item_id;
        runtime->weapon_charge = 0xff;
        runtime->x = player_source_x;
        runtime->y = (s16)(player_source_y - 20);
        if (
            item_id == 0 ||
            !ot_data_hdt_option_read(item_id, &runtime->option)
        ) {
            return 0;
        }
        runtime->valid = runtime->option.option != 0;
        runtime->ammo = runtime->option.ammo;
        runtime->ammo_max = runtime->option.ammo;
        runtime->ammo_refill_ticks_max =
            (u16)(105u - runtime->ammo) * 4u;
        runtime->ammo_refill_ticks =
            runtime->ammo_refill_ticks_max;
        runtime->animation_enabled =
            runtime->option.option == 1;
        runtime->charge_ticks = 20;
    }
    if (!runtime->valid) return 0;
    if (
        runtime->weapon_charge == runtime->charge &&
        runtime->weapon_valid
    ) {
        return 1;
    }
    runtime->weapon_charge = runtime->charge;
    runtime->weapon_valid = 0;
    memset(&runtime->weapon, 0, sizeof(runtime->weapon));
    runtime->poweruse = 0;
    if (runtime->option.weapon_port == 0) return 1;
    if (
        !ot_data_hdt_weapon_port_read(
            runtime->option.weapon_port,
            &port
        )
    ) {
        return 0;
    }
    runtime->poweruse = port.poweruse;
    weapon_id = (u16)(runtime->option.weapon + runtime->charge);
    runtime->weapon_valid =
        weapon_id != 0 &&
        ot_data_hdt_weapon_read(weapon_id, &runtime->weapon) &&
        source_weapon_is_valid(&runtime->weapon);
    return runtime->weapon_valid;
}

static void source_front_weapon_init(void)
{
    u8 index;

    source_front_weapon_bound = 0;
    source_front_weapon_port_id = 0;
    source_front_weapon_power = 0;
    source_front_weapon_hdt_id = 0;
    source_front_weapon_poweruse = 0;
    source_front_weapon_valid = 0;
    source_rear_weapon_bound = 0;
    source_rear_weapon_port_id = 0;
    source_rear_weapon_power = 0;
    source_rear_weapon_mode = 0;
    source_rear_weapon_hdt_id = 0;
    source_rear_weapon_poweruse = 0;
    source_rear_weapon_valid = 0;
    memset(source_shot_repeat, 0, sizeof(source_shot_repeat));
    memset(source_shot_multi_pos, 0, sizeof(source_shot_multi_pos));
    memset(source_sidekick, 0, sizeof(source_sidekick));
    for (index = 0; index < 20; index++) {
        source_player_old_x[index] =
            (s16)(player_source_x - (19 - index));
        source_player_old_y[index] = (s16)(player_source_y - 18);
    }
    source_sidekick_orbit_phase = 0;
    source_sidekick_attachment_move = 0;
    source_sidekick_attachment_linked = 1;
    source_sidekick_attachment_return = 0;
    source_player_delta_x = 0;
    source_player_delta_y = 0;
    source_special_bound_id = 0xff;
    source_special_valid = 0;
    source_special_power = 0;
    source_special_type = 0;
    source_special_definition_weapon = 0;
    source_player_invulnerable_visual_active = 0;
    source_special_ready_indicator = 0;
    source_special_wait = 0;
    source_special_next_wait = 0;
    source_special_fire_held = 0;
    source_special_flare_start = 0;
    source_special_spray = 0;
    source_special_link_to_player = 0;
    source_special_weapon_frequency = 0;
    source_special_zinglon_duration = 0;
    source_special_zinglon_visual_half_width = 0;
    source_special_zinglon_visual_active = 0;
    source_special_zinglon_damage_pending = 0;
    source_special_astral_duration = 0;
    source_special_astral_visual_active = 0;
    source_special_flare_duration = 0;
    source_special_weapon_id = 0;
    source_special_weapon_filter = -99;
    source_last_spawned_shot_index = MAX_PLAYER_SHOTS;
#if TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER
    /*
     * Regression-only loadout.  Release builds cannot enable this macro,
     * and therefore start from JE_initPlayerData() power 1.
     */
    source_parity_level.player_front_weapon_power =
        TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER;
    source_parity_level.player_purple_balls_needed =
        TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER == 11 ? 50 : 1;
#endif
    (void)source_front_weapon_sync();
    (void)source_rear_weapon_sync();
    (void)source_sidekick_sync(0);
    (void)source_sidekick_sync(1);
}
#endif

#if TYRIAN_GBA_STRESS_LOADOUT
static void stress_loadout_init(void)
{
    OtWeaponPortDefinition front_port;
    OtWeaponPortDefinition rear_port;
    u16 weapon_id[STRESS_BAY_EQUIPMENT_COUNT];
    u8 bay;

    memset(stress_weapon, 0, sizeof(stress_weapon));
    memset(stress_shot_repeat, 0, sizeof(stress_shot_repeat));
    memset(stress_shot_multi_pos, 0, sizeof(stress_shot_multi_pos));
    memset(stress_sidekick_animation, 0, sizeof(stress_sidekick_animation));
    stress_loadout_valid =
        ot_data_hdt_weapon_port_read(30, &front_port) &&
        ot_data_hdt_weapon_port_read(22, &rear_port) &&
        ot_data_hdt_option_read(22, &stress_left_option) &&
        ot_data_hdt_option_read(14, &stress_right_option) &&
        ot_data_hdt_special_read(41, &stress_special);
    if (!stress_loadout_valid) return;

    weapon_id[STRESS_BAY_FRONT] = front_port.op[0][10];
    weapon_id[STRESS_BAY_REAR] = rear_port.op[0][10];
    weapon_id[STRESS_BAY_LEFT_SIDEKICK] =
        stress_left_option.weapon;
    weapon_id[STRESS_BAY_RIGHT_SIDEKICK] =
        stress_right_option.weapon;
    weapon_id[STRESS_BAY_SPECIAL] = stress_special.weapon;
    /* JE_mainGamePlayerFunctions() SUPER BOMB source weapon. */
    weapon_id[STRESS_BAY_SUPERBOMB] = 535;

    /*
     * These IDs are stock tyrian.hdt facts, not GBA-authored weapon data.
     * Fail closed if a different data set is mounted so the diagnostic ROM
     * can never silently measure another loadout.
     */
    if (
        weapon_id[STRESS_BAY_FRONT] != 608 ||
        weapon_id[STRESS_BAY_REAR] != 443 ||
        weapon_id[STRESS_BAY_LEFT_SIDEKICK] != 729 ||
        weapon_id[STRESS_BAY_RIGHT_SIDEKICK] != 298 ||
        stress_special.type != 9 ||
        weapon_id[STRESS_BAY_SPECIAL] != 778
    ) {
        stress_loadout_valid = 0;
        return;
    }
    for (bay = 0; bay < STRESS_BAY_EQUIPMENT_COUNT; bay++) {
        if (
            !ot_data_hdt_weapon_read(
                weapon_id[bay],
                &stress_weapon[bay]
            ) ||
            !source_weapon_is_valid(&stress_weapon[bay])
        ) {
            stress_loadout_valid = 0;
            return;
        }
    }

    source_parity_level.player_front_weapon_id = 30;
    source_parity_level.player_front_weapon_power = 11;
    source_parity_level.player_rear_weapon_id = 22;
    source_parity_level.player_rear_weapon_power = 11;
    source_parity_level.player_weapon_mode = 1;
    source_parity_level.player_special = 41;
    source_parity_level.player_superbombs = 10;
    source_player_invulnerable_visual_active = 0;
    source_special_ready_indicator = 1;
    stress_sidekick_x[0] = player_source_x;
    stress_sidekick_y[0] = (s16)(player_source_y - 20);
    stress_sidekick_x[1] = (s16)(player_source_x + 16);
    stress_sidekick_y[1] = player_source_y;
    stress_player_delta_x = 0;
    stress_player_delta_y = 0;
}

static void stress_update_shot_peak(void)
{
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
    u8 active = active_player_shot_count;
#else
    u8 index;
    u8 active = 0;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active++;
    }
#endif
    if (active > telemetry_player_shot_max_active) {
        telemetry_player_shot_max_active = active;
    }
}

static void stress_spawn_weapon(
    const OtWeaponDefinition *weapon,
    u8 bay,
    s16 origin_x,
    s16 origin_y
)
{
    u8 allocated = 0;
    u8 multi;
    u8 spawned = 0;

    if (
        !stress_loadout_valid ||
        weapon == 0 ||
        bay >= STRESS_BAY_COUNT ||
        !source_weapon_is_valid(weapon)
    ) {
        telemetry_stress_loadout_failures++;
        return;
    }

    for (multi = 0; multi < weapon->multi; multi++) {
        PlayerShot *shot = 0;
        u8 shot_index;
        u8 position;
        u8 attack;
        u8 delay;

        shot_index = player_shot_find_free_slot();
        if (shot_index < MAX_PLAYER_SHOTS) {
            shot = &player_shots[shot_index];
        }
        if (shot == 0) {
            telemetry_player_shot_drops++;
            break;
        }
        allocated = 1;

        if (
            stress_shot_multi_pos[bay] == weapon->max ||
            stress_shot_multi_pos[bay] > 8
        ) {
            stress_shot_multi_pos[bay] = 1;
        } else {
            stress_shot_multi_pos[bay]++;
        }
        position = (u8)(stress_shot_multi_pos[bay] - 1u);
        if (weapon->sg[position] == 0) continue;
        delay = weapon->delay[position];
        if (delay == 0) continue;
        if (delay == 121) {
            delay = 255;
        }

        memset(shot, 0, sizeof(*shot));
        player_shot_set_active(shot_index, 1);
        shot->ttl = delay;
        shot->x = (s16)(origin_x + weapon->bx[position]);
        shot->y = (s16)(origin_y + weapon->by[position]);
        shot->xc = weapon->accelerationx;
        shot->yc = (s16)-weapon->acceleration;
        shot->xm = weapon->sx[position];
        shot->ym = (s16)-weapon->sy[position];
        if (weapon->sy[position] == 98) {
            shot->ym = 0;
            shot->yc = -1;
        } else if (weapon->sy[position] > 100) {
            shot->ym = weapon->sy[position];
            shot->y = (s16)(
                shot->y - stress_player_delta_y
            );
        }
        if (weapon->sx[position] > 100) {
            shot->xm = weapon->sx[position];
            shot->x = (s16)(
                shot->x - stress_player_delta_x
            );
            if (shot->xm == 101) {
                shot->y = (s16)(
                    shot->y - stress_player_delta_y
                );
            }
        }
        shot->graphic = weapon->sg[position];
        shot->render_graphic = shot->graphic;
        shot->animation_max =
            delay > 100 && delay < 120 ?
                (u8)(delay - 99u) :
                (u8)(weapon->weapani + 1u);
        if (shot->animation_max == 0) shot->animation_max = 1;
        shot->trail =
            weapon->delay[position] == 121 ?
                0 :
                weapon->trail;
        shot->blast_filter = weapon->shipblastfilter;
        attack = weapon->attack[position];
        if (attack == 99) {
            shot->damage = 0;
            shot->iced = 40;
        } else if (attack > 99 && attack < 250) {
            shot->chain_weapon = (u16)(attack - 100u);
            shot->damage = 1;
        } else if (attack >= 250) {
            shot->damage = (u8)(attack - 250u);
            shot->infinite = 1;
        } else {
            shot->damage = attack;
        }
        telemetry_player_shot_spawns++;
        spawned++;
    }
    /* shots.c changes shotRepeat only after finding a free logical slot. */
    if (allocated) stress_shot_repeat[bay] = weapon->shotrepeat;
    if (spawned) audio_effect(SFX_WEAPON_1);
    stress_update_shot_peak();
}
#endif

#if !TYRIAN_GBA_STRESS_LOADOUT
static s8 source_clamp_s16_to_s8(
    s16 value,
    s8 minimum,
    s8 maximum
)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return (s8)value;
}

static u8 source_spawn_weapon(
    const OtWeaponDefinition *weapon,
    u8 bay,
    s16 origin_x,
    s16 origin_y
)
{
    u8 allocated = 0;
    u8 multi;
    u8 spawned = 0;

    source_last_spawned_shot_index = MAX_PLAYER_SHOTS;

    if (
        !player_alive ||
        source_parity_level.end_level ||
        weapon == 0 ||
        bay >= SOURCE_WEAPON_BAY_COUNT ||
        !source_weapon_is_valid(weapon)
    ) {
        return 0;
    }
    if (weapon->sound > 0) {
        source_parity_level.frame_sound_queue[
            source_weapon_sound_slot[bay]
        ] = weapon->sound;
    }
    for (multi = 0; multi < weapon->multi; multi++) {
        PlayerShot *shot = 0;
        u8 index;
        u8 position;
        u8 delay;
        u8 attack;
        u8 circle;

        index = player_shot_find_free_slot();
        if (index < MAX_PLAYER_SHOTS) shot = &player_shots[index];
        if (shot == 0) {
            telemetry_player_shot_drops++;
            /* player_shot_create() returns MAX_PWEAPON immediately. */
            source_last_spawned_shot_index = MAX_PLAYER_SHOTS;
            break;
        }
        allocated = 1;
        if (
            source_shot_multi_pos[bay] == weapon->max ||
            source_shot_multi_pos[bay] > 8
        ) {
            source_shot_multi_pos[bay] = 1;
        } else {
            source_shot_multi_pos[bay]++;
        }
        position = (u8)(source_shot_multi_pos[bay] - 1u);
        delay = weapon->delay[position];
        if (delay == 121) delay = 255;

        memset(shot, 0, sizeof(*shot));
        shot->x = (s16)(origin_x + weapon->bx[position]);
        shot->y = (s16)(origin_y + weapon->by[position]);
        shot->xc = weapon->accelerationx;
        shot->yc = (s8)-weapon->acceleration;
        shot->xm = weapon->sx[position];
        if (delay == 99 || delay == 98) {
            shot->xm = (s16)(
                shot->xm +
                source_clamp_s16_to_s8(
                    (s16)(
                        origin_x - player_source_x +
                        source_player_delta_x
                    ),
                    -5,
                    5
                )
            );
        }
        if (delay == 99 || delay == 100) {
            shot->ym = source_clamp_s16_to_s8(
                (s16)(
                    origin_y -
                    player_source_y +
                    source_player_delta_y -
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
            shot->y = (s16)(shot->y - source_player_delta_y);
        } else {
            shot->ym = (s16)-weapon->sy[position];
        }
        if (weapon->sx[position] > 100) {
            shot->xm = weapon->sx[position];
            shot->x = (s16)(shot->x - source_player_delta_x);
            if (shot->xm == 101) {
                shot->y = (s16)(shot->y - source_player_delta_y);
            }
        }
        shot->graphic = weapon->sg[position];
        shot->render_graphic = shot->graphic;
        shot->animation_max =
            delay > 100 && delay < 120 ?
                (u8)(delay - 99u) :
                (u8)(weapon->weapani + 1u);
        if (shot->animation_max == 0) shot->animation_max = 1;
        shot->trail =
            weapon->delay[position] == 121 ?
                0 :
                weapon->trail;
        shot->blast_filter = weapon->shipblastfilter;
        attack = weapon->attack[position];
        if (attack == 99) {
            shot->damage = 0;
            shot->iced = 40;
        } else if (attack > 99 && attack < 250) {
            shot->chain_weapon = (u16)(attack - 100u);
            shot->damage = 1;
        } else if (attack >= 250) {
            shot->damage = (u8)(attack - 250u);
            shot->infinite = 1;
        } else {
            shot->damage = attack;
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
        if (weapon->aim > 5) {
            u8 enemy_index;
            u16 best_distance = 65000;

            for (
                enemy_index = 0;
                enemy_index < OT_ENEMY_COUNT;
                enemy_index++
            ) {
                const OtEnemy *enemy =
                    &source_parity_level.enemy[enemy_index];
                u16 distance;
                s16 dx;
                s16 dy;

                if (
                    source_parity_level.enemy_avail[enemy_index] == 1 ||
                    enemy->scoreitem
                ) {
                    continue;
                }
                dx = (s16)(
                    enemy->ex - shot->x
                );
                dy = (s16)(enemy->ey - shot->y);
                distance = (u16)(
                    (dx < 0 ? -dx : dx) +
                    (dy < 0 ? -dy : dy)
                );
                if (distance < best_distance) {
                    best_distance = distance;
                    shot->aim_enemy = (u8)(enemy_index + 1u);
                }
            }
            if (shot->aim_enemy != 0) {
                shot->aim_delay = 5;
                shot->aim_delay_max = (u8)(weapon->aim - 5u);
                if (shot->aim_delay_max == 0) {
                    shot->aim_delay_max = 1;
                }
            }
        }
        shot->ttl = delay;
        if (shot->graphic != 0 && delay != 0) {
            player_shot_set_active(index, 1);
            source_last_spawned_shot_index = index;
            telemetry_player_shot_spawns++;
            spawned++;
        }
    }
    /*
     * player_shot_create() returns immediately when all 81 slots are busy;
     * in that case it does not consume the bay cooldown.  A slot whose HDT
     * graphic/delay resolves to zero still counts as an authored allocation.
     */
    if (allocated) source_shot_repeat[bay] = weapon->shotrepeat;
    return spawned;
}
#endif

/*
 * Literal player_shot_create(..., SHOT_MISC, ..., 104, ...) path used by
 * JE_playerCollide() for evalue == -3.  It deliberately reads the stock HDT
 * definition at runtime; no GBA-authored pickup weapon table is involved.
 */
static u8 spawn_orbiting_asteroid_shot(void)
{
    OtWeaponDefinition weapon;
    PlayerShot *shot = 0;
    u8 index;
    u8 delay;
    u8 circle;
#if TYRIAN_GBA_STRESS_LOADOUT
    s16 delta_x = stress_player_delta_x;
    s16 delta_y = stress_player_delta_y;
#else
    s16 delta_x = source_player_delta_x;
    s16 delta_y = source_player_delta_y;
#endif

    if (!ot_data_hdt_weapon_read(104, &weapon) ||
        !source_weapon_is_valid(&weapon)) {
        return 0;
    }
    index = player_shot_find_free_slot();
    if (index < MAX_PLAYER_SHOTS) shot = &player_shots[index];
    if (shot == 0) {
        telemetry_player_shot_drops++;
        return 0;
    }

    /* shotMultiPos[SHOT_MISC] is reset to zero immediately before create. */
    delay = weapon.delay[0];
    if (delay == 121) delay = 255;
    memset(shot, 0, sizeof(*shot));
    shot->x = (s16)(player_source_x + weapon.bx[0]);
    shot->y = (s16)(player_source_y + weapon.by[0]);
    shot->xc = weapon.accelerationx;
    shot->yc = (s8)-weapon.acceleration;
    shot->xm = weapon.sx[0];
    shot->ym = (s16)-weapon.sy[0];
    if (weapon.sy[0] == 98) {
        shot->ym = 0;
        shot->yc = -1;
    } else if (weapon.sy[0] > 100) {
        shot->ym = weapon.sy[0];
        shot->y = (s16)(shot->y - delta_y);
    }
    if (weapon.sx[0] > 100) {
        shot->xm = weapon.sx[0];
        shot->x = (s16)(shot->x - delta_x);
        if (shot->xm == 101) {
            shot->y = (s16)(shot->y - delta_y);
        }
    }
    shot->graphic = weapon.sg[0];
    shot->render_graphic = shot->graphic;
    shot->animation_max =
        delay > 100 && delay < 120 ?
            (u8)(delay - 99u) :
            (u8)(weapon.weapani + 1u);
    if (shot->animation_max == 0) shot->animation_max = 1;
    shot->trail = weapon.delay[0] == 121 ? 0 : weapon.trail;
    if (weapon.attack[0] == 99) {
        shot->damage = 0;
        shot->iced = 40;
    } else if (weapon.attack[0] >= 250) {
        shot->damage = (u8)(weapon.attack[0] - 250u);
        shot->infinite = 1;
    } else {
        shot->damage = weapon.attack[0];
    }
    shot->ttl = delay;

    circle = weapon.circlesize;
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
    player_shot_set_active(
        index,
        shot->graphic != 0 && delay != 0
    );
    if (weapon.sound > 0) {
        /* shots.c soundChannel[SHOT_MISC] is source queue slot two. */
        source_parity_level.frame_sound_queue[2] = weapon.sound;
    }
    if (shot->active) telemetry_player_shot_spawns++;
    return shot->active;
}

/*
 * Direct single-player JE_playerDamage() translation.  HUD redraw calls are
 * presentation-only in the 320x200 PC build and intentionally absent from
 * the cropped GBA combat viewport.
 */
static u16 player_damage(u16 damage)
{
    u16 remaining = damage;
    u16 damage_through_shield = 0;

    if (!player_alive || player_invulnerable || remaining == 0) return 0;
    telemetry_collision_count++;
    source_parity_level.frame_sound_queue[7] = 27;
#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
    /*
     * Development validation still records the collision, but deliberately
     * leaves the translated shield, armor and death state untouched.  This
     * flag is intentionally absolute: an authored survival game cannot end
     * through death while it is enabled, so developers use its source
     * Next-Level "Skip It" route instead.
     */
    return 0;
#endif
#if defined(AUTOTEST) && !defined(AUTOTEST_FORCE_PLAYER_DEATH)
    /*
     * The deterministic source-event audit must reach the authored boss and
     * level-exit records; release builds retain the translated damage path.
     */
    return 0;
#endif
    if (player_shield < remaining) {
        damage_through_shield = remaining;
        remaining -= player_shield;
        player_shield = 0;
        if (remaining > 0) {
            if (player_armor < remaining) {
                player_armor = 0;
                player_alive = 0;
                player_exploding_ticks = 60;
                player_death_fx_wait = 0;
                player_death_music_volume = 255;
                player_death_music_fade_active = 0;
                source_parity_level.level_timer = false;
                source_parity_level.level_end = 40;
                /*
                 * varz.c JE_playerDamage() assigns S_EXPLOSION_22 at the
                 * lethal transition before JE_playerMovement begins the
                 * repeating 9/11 cadence later in this same source tick.
                 */
                source_parity_level.frame_sound_queue[1] = 22;
                telemetry_player_death_sfx_22++;
            } else {
                player_armor = (u8)(player_armor - remaining);
                source_parity_level.frame_sound_queue[7] = 19;
            }
        }
    } else {
        player_shield = (u8)(player_shield - remaining);
        /*
         * Literal single-player JE_playerDamage() shield-hit corona.  These
         * nine explosions are part of the 264x184 battle presentation, not
         * the omitted PC sidebar HUD, and enter the normal explosion pool at
         * the same pre-JE_playerMovement phase as the source.
         */
        source_spawn_explosion(
            (s16)(player_source_x - 17),
            (s16)(player_source_y - 12), 0, 14, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x - 5),
            (s16)(player_source_y - 12), 0, 15, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x + 7),
            (s16)(player_source_y - 12), 0, 16, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x + 19),
            (s16)(player_source_y - 12), 0, 17, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x - 17),
            (s16)(player_source_y + 2), 0, 18, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x + 19),
            (s16)(player_source_y + 2), 0, 19, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x - 17),
            (s16)(player_source_y + 16), 0, 20, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x - 5),
            (s16)(player_source_y + 16), 0, 21, 1
        );
        source_spawn_explosion(
            (s16)(player_source_x + 7),
            (s16)(player_source_y + 16), 0, 22, 1
        );
    }
    source_parity_level.player_armor = player_armor;
    return damage_through_shield;
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static s16 source_sidekick_sine(u8 phase)
{
    return ((const s16 *)jukebox_sine)[phase];
}

static void source_player_history_update(s16 old_x, s16 old_y)
{
    u8 index;

    if (player_source_x == old_x && player_source_y == old_y) return;
    for (index = 1; index < 20; index++) {
        source_player_old_x[index - 1u] =
            source_player_old_x[index];
        source_player_old_y[index - 1u] =
            source_player_old_y[index];
    }
    source_player_old_x[19] = player_source_x;
    source_player_old_y[19] = player_source_y;
}

/*
 * Translate the compact physical GBA layout into OpenTyrian's logical
 * gameplay buttons.  Source demo streams remain logical recordings: their
 * left/right Sidekick buttons must never be merged merely because the GBA
 * player uses one physical B button for both.
 */
static u8 source_gameplay_main_fire_held(void)
{
    if (frontend_demo_active) {
        return (
            frontend_demo_keys & FRONTEND_DEMO_KEY_MAIN_FIRE
        ) != 0;
    }
    return (pad_now & KEY_A) != 0;
}

static u8 source_gameplay_sidekick_command_held(u8 side)
{
    if (frontend_demo_active) {
        u8 mask = side == 0 ?
            FRONTEND_DEMO_KEY_LEFT_SIDEKICK :
            FRONTEND_DEMO_KEY_RIGHT_SIDEKICK;

        return (frontend_demo_keys & mask) != 0;
    }
    return (pad_now & KEY_B) != 0;
}

static u8 source_gameplay_sidekick_command_pressed(u8 side)
{
    if (frontend_demo_active) {
        u8 mask = side == 0 ?
            FRONTEND_DEMO_KEY_LEFT_SIDEKICK :
            FRONTEND_DEMO_KEY_RIGHT_SIDEKICK;

        return (frontend_demo_keys_pressed & mask) != 0;
    }
    return (pad_pressed & KEY_B) != 0;
}

static u8 source_gameplay_sidekick_fire_held(u8 side)
{
    if (source_gameplay_sidekick_command_held(side)) return 1;
#if TYRIAN_GBA_GAMEPAD_FULL_AUTO_SIDEKICKS
    if (!frontend_demo_active && source_gameplay_main_fire_held()) {
        return 1;
    }
#endif
    return 0;
}

static u8 source_gameplay_rear_mode_pressed(void)
{
    if (frontend_demo_active) {
        return (
            frontend_demo_keys_pressed &
            FRONTEND_DEMO_KEY_REAR_MODE
        ) != 0;
    }
    return (pad_pressed & KEY_SELECT) != 0;
}

static u8 source_gameplay_special_fire_held(void)
{
    if (frontend_demo_active) {
        if (
            frontend_demo_keys & FRONTEND_DEMO_KEY_MAIN_FIRE
        ) {
            return 1;
        }
        return
            source_parity_level.player_super_arcade_mode != 0 &&
            (
                frontend_demo_keys &
                (
                    FRONTEND_DEMO_KEY_LEFT_SIDEKICK |
                    FRONTEND_DEMO_KEY_RIGHT_SIDEKICK
                )
            ) != 0;
    }
    return (pad_now & KEY_L) != 0;
}

static u8 source_gameplay_superbomb_fire_held(void)
{
    if (frontend_demo_active) {
        return (
            frontend_demo_keys &
            (
                FRONTEND_DEMO_KEY_LEFT_SIDEKICK |
                FRONTEND_DEMO_KEY_RIGHT_SIDEKICK
            )
        ) != 0;
    }
    return (pad_now & KEY_R) != 0;
}

#ifdef AUTOTEST
static u8 source_gameplay_mapping_selftest(void)
{
    u16 saved_pad_now = pad_now;
    u16 saved_pad_pressed = pad_pressed;
    u8 saved_demo_active = frontend_demo_active;
    u8 saved_demo_keys = frontend_demo_keys;
    u8 saved_demo_keys_pressed = frontend_demo_keys_pressed;
    u8 saved_super_arcade =
        source_parity_level.player_super_arcade_mode;
    u8 pass = 1;

    frontend_demo_active = 0;
    source_parity_level.player_super_arcade_mode = 0;

    pad_now = KEY_A;
    pad_pressed = 0;
    pass = pass && source_gameplay_main_fire_held();
    pass = pass && !source_gameplay_sidekick_command_held(0);
    pass = pass && !source_gameplay_sidekick_command_held(1);
#if TYRIAN_GBA_GAMEPAD_FULL_AUTO_SIDEKICKS
    pass = pass && source_gameplay_sidekick_fire_held(0);
    pass = pass && source_gameplay_sidekick_fire_held(1);
#else
    pass = pass && !source_gameplay_sidekick_fire_held(0);
    pass = pass && !source_gameplay_sidekick_fire_held(1);
#endif
    pass = pass && !source_gameplay_special_fire_held();
    pass = pass && !source_gameplay_superbomb_fire_held();

    pad_now = KEY_B;
    pad_pressed = KEY_B;
    pass = pass && !source_gameplay_main_fire_held();
    pass = pass && source_gameplay_sidekick_command_held(0);
    pass = pass && source_gameplay_sidekick_command_held(1);
    pass = pass && source_gameplay_sidekick_command_pressed(0);
    pass = pass && source_gameplay_sidekick_command_pressed(1);
    pass = pass && !source_gameplay_special_fire_held();
    pass = pass && !source_gameplay_superbomb_fire_held();

    pad_now = KEY_L;
    pad_pressed = KEY_L;
    pass = pass && source_gameplay_special_fire_held();
    pass = pass && !source_gameplay_superbomb_fire_held();

    pad_now = KEY_R;
    pad_pressed = KEY_R;
    pass = pass && !source_gameplay_special_fire_held();
    pass = pass && source_gameplay_superbomb_fire_held();

    pad_now = 0;
    pad_pressed = KEY_SELECT;
    pass = pass && source_gameplay_rear_mode_pressed();

    /* PC demo bits retain their four independent logical buttons. */
    frontend_demo_active = 1;
    frontend_demo_keys = FRONTEND_DEMO_KEY_MAIN_FIRE;
    frontend_demo_keys_pressed = FRONTEND_DEMO_KEY_MAIN_FIRE;
    pass = pass && source_gameplay_main_fire_held();
    pass = pass && source_gameplay_special_fire_held();
    pass = pass && !source_gameplay_sidekick_fire_held(0);
    pass = pass && !source_gameplay_sidekick_fire_held(1);
    pass = pass && !source_gameplay_superbomb_fire_held();

    frontend_demo_keys = FRONTEND_DEMO_KEY_LEFT_SIDEKICK;
    frontend_demo_keys_pressed = FRONTEND_DEMO_KEY_LEFT_SIDEKICK;
    pass = pass && source_gameplay_sidekick_fire_held(0);
    pass = pass && !source_gameplay_sidekick_fire_held(1);
    pass = pass && source_gameplay_superbomb_fire_held();

    frontend_demo_keys = FRONTEND_DEMO_KEY_RIGHT_SIDEKICK;
    frontend_demo_keys_pressed = FRONTEND_DEMO_KEY_RIGHT_SIDEKICK;
    pass = pass && !source_gameplay_sidekick_fire_held(0);
    pass = pass && source_gameplay_sidekick_fire_held(1);
    pass = pass && source_gameplay_superbomb_fire_held();

    frontend_demo_keys = FRONTEND_DEMO_KEY_REAR_MODE;
    frontend_demo_keys_pressed = FRONTEND_DEMO_KEY_REAR_MODE;
    pass = pass && source_gameplay_rear_mode_pressed();

    pad_now = saved_pad_now;
    pad_pressed = saved_pad_pressed;
    frontend_demo_active = saved_demo_active;
    frontend_demo_keys = saved_demo_keys;
    frontend_demo_keys_pressed = saved_demo_keys_pressed;
    source_parity_level.player_super_arcade_mode = saved_super_arcade;
    return pass;
}
#endif

static void source_sidekick_positions_update(void)
{
    SourceSidekickRuntime *left = &source_sidekick[0];
    SourceSidekickRuntime *right = &source_sidekick[1];
    u8 orbiting =
        (left->valid && left->option.style == 4) +
        (right->valid && right->option.style == 4);
    s16 orbit_x;
    s16 orbit_y;

    if (orbiting != 0) {
        source_sidekick_orbit_phase = (u8)(
            source_sidekick_orbit_phase +
            (orbiting == 2 ? 8u : 6u)
        );
    }
    orbit_x = (s16)(
        (s32)source_sidekick_sine(source_sidekick_orbit_phase) *
        20 /
        32767
    );
    orbit_y = (s16)(
        (s32)source_sidekick_sine(
            (u8)(source_sidekick_orbit_phase + 64u)
        ) *
        20 /
        32767
    );

    if (left->valid) {
        switch (left->option.style) {
        case 0:
            left->x = (s16)(
                player_source_x - source_player_delta_x - 14
            );
            left->y = (s16)(
                player_source_y - source_player_delta_y
            );
            break;
        case 1:
        case 3:
            left->x = source_player_old_x[9];
            left->y = source_player_old_y[9];
            break;
        case 2:
            left->x = player_source_x;
            left->y = (s16)(player_source_y - 20);
            if (left->y < 10) left->y = 10;
            break;
        case 4:
            left->x = (s16)(player_source_x + orbit_x);
            left->y = (s16)(player_source_y + orbit_y);
            break;
        default:
            break;
        }
    }
    if (right->valid) {
        switch (right->option.style) {
        case 0:
            /*
             * The GBA raw frames expose the real opaque bounds: the common
             * 12x14 option body is centred at x+5.5, while the neutral
             * converted player silhouette is centred at player_x+6.  A
             * +15 right anchor therefore matches the left -14 anchor at an
             * equal 14.5-pixel visible-centre distance.  OpenTyrian's +16
             * uses the nominal 24-pixel player frame rather than its opaque
             * silhouette and remains one pixel wider on this presentation.
             */
            right->x = (s16)(
                player_source_x - source_player_delta_x + 15
            );
            right->y = (s16)(
                player_source_y - source_player_delta_y
            );
            break;
        case 1:
        case 3:
            right->x = source_player_old_x[0];
            right->y = source_player_old_y[0];
            break;
        case 2:
            if (!source_sidekick_attachment_linked) {
                s16 target_y = (s16)(player_source_y - 20);
                s16 step;
                s16 distance;

                right->y = (s16)(
                    right->y + source_sidekick_attachment_move / 2
                );
                if (source_sidekick_attachment_move >= -2) {
                    step =
                        source_sidekick_attachment_return ? 2 : 0;
                    if (right->y > target_y + 5) {
                        step = 2;
                        source_sidekick_attachment_move = (s8)(
                            source_sidekick_attachment_move -
                            1 -
                            source_sidekick_attachment_return
                        );
                    } else if (right->y > target_y) {
                        step = 3;
                        if (source_sidekick_attachment_move > 0) {
                            source_sidekick_attachment_move--;
                        } else {
                            source_sidekick_attachment_move++;
                        }
                    } else if (right->y > target_y - 5) {
                        step = 2;
                        source_sidekick_attachment_move++;
                    } else if (
                        source_sidekick_attachment_move <
                            2 +
                            source_sidekick_attachment_return * 4
                    ) {
                        source_sidekick_attachment_move = (s8)(
                            source_sidekick_attachment_move +
                            1 +
                            source_sidekick_attachment_return
                        );
                    }
                    if (source_sidekick_attachment_return) {
                        step = (s16)(step * 2);
                    }
                    distance = (s16)(right->x - player_source_x);
                    if (distance < 0) distance = (s16)-distance;
                    if (distance < step) step = 1;
                    if (right->x > player_source_x) {
                        right->x = (s16)(right->x - step);
                    } else if (right->x < player_source_x) {
                        right->x = (s16)(right->x + step);
                    }
                    distance = (s16)(right->y - target_y);
                    if (distance < 0) distance = (s16)-distance;
                    step = (s16)(right->x - player_source_x);
                    if (step < 0) step = (s16)-step;
                    if (distance + step < 8) {
                        source_sidekick_attachment_linked = 1;
                        source_parity_level.frame_sound_queue[2] =
                            23;
                    }
                    if (source_gameplay_sidekick_command_held(1)) {
                        source_sidekick_attachment_return = 1;
                    }
                } else {
                    source_sidekick_attachment_move = (s8)(
                        source_sidekick_attachment_move +
                        1 +
                        source_sidekick_attachment_return
                    );
                    source_spawn_explosion(
                        (s16)(right->x + 1),
                        (s16)(right->y + 10),
                        0,
                        0,
                        0
                    );
                }
            } else {
                right->x = player_source_x;
                right->y = (s16)(player_source_y - 20);
                if (source_gameplay_sidekick_command_pressed(1)) {
                    source_sidekick_attachment_linked = 0;
                    source_sidekick_attachment_return = 0;
                    source_sidekick_attachment_move = -20;
                    source_parity_level.frame_sound_queue[3] =
                        26;
                }
            }
            if (right->y < 10) right->y = 10;
            break;
        case 4:
            right->x = (s16)(player_source_x - orbit_x);
            right->y = (s16)(player_source_y - orbit_y);
            break;
        default:
            break;
        }
    }
}

static void source_rear_mode_update(void)
{
    OtWeaponPortDefinition port;
    u8 port_id = source_parity_level.player_rear_weapon_id;

    if (
        !source_gameplay_rear_mode_pressed() ||
        port_id == 0 ||
        !ot_data_hdt_weapon_port_read(port_id, &port) ||
        port.opnum == 0
    ) {
        return;
    }
    source_shot_multi_pos[SOURCE_WEAPON_BAY_REAR] = 0;
    source_parity_level.player_weapon_mode++;
    if (
        source_parity_level.player_weapon_mode > port.opnum ||
        source_parity_level.player_weapon_mode > 2
    ) {
        source_parity_level.player_weapon_mode = 1;
    }
    source_rear_weapon_bound = 0;
    (void)source_rear_weapon_sync();
}

static u8 source_special_sync(void)
{
    OtSpecialDefinition definition;
    u8 special_id = source_parity_level.player_special;

    if (source_special_bound_id == special_id) {
        return source_special_valid;
    }
    source_special_bound_id = special_id;
    source_special_valid = 0;
    source_special_power = 0;
    source_special_type = 0;
    source_special_definition_weapon = 0;
    source_shot_multi_pos[SOURCE_WEAPON_BAY_SPECIAL] = 0;
    source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 0;
    source_shot_multi_pos[SOURCE_WEAPON_BAY_SPECIAL2] = 0;
    source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL2] = 0;
    if (
        special_id == 0 ||
        !ot_data_hdt_special_read(special_id, &definition)
    ) {
        (void)source_option_projectile_sync();
        return 0;
    }
    source_special_power = definition.power;
    source_special_type = definition.type;
    source_special_definition_weapon = definition.weapon;
    source_special_valid = definition.type != 0;
    (void)source_option_projectile_sync();
    return source_special_valid;
}

/*
 * Direct translation of JE_doSpecialShot()'s pwr payment block.
 *
 * This helper belongs to OpenTyrian's SFExecuted twiddle/command path, not
 * to the equipped Special item drawn in the upper-left HUD.  Keep it for the
 * translated command semantics and their tests, but never call it from the
 * ordinary gamepad fire path below.  The affordability test, mutation order
 * and adjusted temp2/effect value remain source-authored.  In particular,
 * pwr==98 consumes the remaining shield even when fewer than four points
 * make the activation fail; that surprising ordering is intentional parity.
 */
static __attribute__((unused)) u8 source_special_try_pay_cost(
    u8 encoded_power,
    u8 *effect_power
)
{
    if (effect_power == NULL) return 0;
    *effect_power = encoded_power;
    if (encoded_power == 0) return 1;

    if (encoded_power < 98) {
        if (player_shield < encoded_power) return 0;
        player_shield = (u8)(player_shield - encoded_power);
        return 1;
    }
    if (encoded_power == 98) {
        u8 can_afford = player_shield >= 4;

        *effect_power = player_shield;
        player_shield = 0;
        return can_afford;
    }
    if (encoded_power == 99) {
        *effect_power = (u8)(player_shield / 2u);
        player_shield = *effect_power;
        return 1;
    }

    *effect_power = (u8)(encoded_power - 100u);
    if (player_armor <= *effect_power) return 0;
    player_armor = (u8)(player_armor - *effect_power);
    source_parity_level.player_armor = player_armor;
    return 1;
}

/*
 * JE_doSpecialShot() has two independent activation paths.  SFExecuted
 * decodes pwr while paying for a keyboard combo; the ordinary equipped-item
 * button does neither, but JE_specialComplete() still reads the shared
 * scratch byte temp2 for types 12, 13 and 16.  Its value at that point is an
 * unrelated caller residue, so reproducing it would make Invulnerability
 * duration depend on whichever PC loop happened to touch temp2 last.
 *
 * Keep the source-visible equipped behaviour (no Shield/Armor mutation and
 * no shot phase reset), while deterministically supplying the same effect
 * value that the authored pwr encoding denotes.  This is especially
 * important for Special 37: pwr 98 means the current Shield quantity, not
 * the literal value 98 (which would incorrectly grant 980 ticks).
 */
static u8 source_special_effect_power_without_payment(u8 encoded_power)
{
    if (encoded_power < 98u) return encoded_power;
    if (encoded_power == 98u) return player_shield;
    if (encoded_power == 99u) return (u8)(player_shield / 2u);
    return (u8)(encoded_power - 100u);
}

static void source_special_complete(u8 effect_power)
{
    OtWeaponDefinition weapon;
    u8 index;

    if (!source_special_valid) return;
    source_special_next_wait = 0;
    switch (source_special_type) {
    case 1: /* Weapon */
        if (
            ot_data_hdt_weapon_read(
                source_special_definition_weapon,
                &weapon
            ) &&
            source_weapon_is_valid(&weapon)
        ) {
            source_spawn_weapon(
                &weapon,
                SOURCE_WEAPON_BAY_SPECIAL2,
                player_source_x,
                player_source_y
            );
            source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] =
                source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL2];
        }
        break;

    case 2: /* Repulsor */
        for (index = 0; index < OT_ENEMY_SHOT_COUNT; index++) {
            OtEnemyShot *shot =
                &source_parity_level.enemy_shot[index];

            if (!shot->active) continue;
            if (player_source_x > shot->sx) {
                shot->sxm--;
            } else if (player_source_x < shot->sx) {
                shot->sxm++;
            }
            if (player_source_y > shot->sy) {
                shot->sym--;
            } else if (player_source_y < shot->sy) {
                shot->sym++;
            }
        }
        break;

    case 3: /* Zinglon Blast */
        source_special_zinglon_duration = 50;
        source_special_zinglon_damage_pending = 0;
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 100;
        source_parity_level.frame_sound_queue[7] = 21;
        break;

    case 4: /* Attractor */
        for (index = 0; index < OT_ENEMY_COUNT; index++) {
            OtEnemy *enemy = &source_parity_level.enemy[index];

            if (
                source_parity_level.enemy_avail[index] == 1 ||
                !enemy->scoreitem ||
                enemy->evalue == 0
            ) {
                continue;
            }
            if (player_source_x > enemy->ex) {
                enemy->exc++;
            } else if (player_source_x < enemy->ex) {
                enemy->exc--;
            }
            if (player_source_y > enemy->ey) {
                enemy->eyc++;
            } else if (player_source_y < enemy->ey) {
                enemy->eyc--;
            }
        }
        break;

    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 16:
        if (source_special_flare_duration == 0) {
            source_special_flare_start = 1;
        }
        source_special_weapon_id =
            source_special_definition_weapon;
        source_special_link_to_player = 0;
        source_special_spray = 0;
        switch (source_special_type) {
        case 5:
            source_special_weapon_filter = 7;
            source_special_weapon_frequency = 2;
            source_special_flare_duration = 50;
            break;
        case 6:
            source_special_weapon_filter = 1;
            source_special_weapon_frequency = 7;
            source_special_flare_duration = (u16)(
                200u +
                25u * source_parity_level.player_front_weapon_power
            );
            break;
        case 7:
            source_special_weapon_filter = 3;
            source_special_weapon_frequency = 3;
            source_special_flare_duration = (u16)(
                50u +
                10u * source_parity_level.player_front_weapon_power
            );
            source_special_zinglon_duration = 50;
            source_special_zinglon_damage_pending = 0;
            source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 100;
            source_parity_level.frame_sound_queue[7] = 21;
            break;
        case 8:
            source_special_weapon_filter = -99;
            source_special_weapon_frequency = 7;
            source_special_flare_duration = (u16)(
                10u + source_parity_level.player_front_weapon_power
            );
            break;
        case 9:
            source_special_weapon_filter = -99;
            source_special_weapon_frequency = 8;
            source_special_flare_duration = (u16)(
                8u +
                2u * source_parity_level.player_front_weapon_power
            );
            source_special_link_to_player = 1;
            source_special_next_wait = source_special_power;
            break;
        case 10:
            source_special_weapon_filter = -99;
            source_special_weapon_frequency = 8;
            source_special_flare_duration = (u16)(
                14u +
                4u * source_parity_level.player_front_weapon_power
            );
            source_special_link_to_player = 1;
            break;
        case 11:
            source_special_weapon_filter = -99;
            source_special_weapon_frequency = source_special_power;
            source_special_flare_duration = (u16)(
                10u +
                10u * source_parity_level.player_front_weapon_power
            );
            source_special_astral_duration = (u8)(
                20u +
                10u * source_parity_level.player_front_weapon_power
            );
            break;
        default: /* type 16 */
            source_special_weapon_filter = -99;
            source_special_weapon_frequency = 8;
            source_special_flare_duration = (u16)(
                effect_power * 16u + 8u
            );
            source_special_link_to_player = 1;
            source_special_spray = 1;
            break;
        }
        break;

    case 12: /* Invulnerability */
        player_invulnerable = (u16)effect_power * 10u;
        if (
            source_parity_level.player_super_arcade_mode >= 1 &&
            source_parity_level.player_super_arcade_mode <= 7
        ) {
            source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] = 250;
            if (
                ot_data_hdt_weapon_read(707, &weapon) &&
                source_weapon_is_valid(&weapon)
            ) {
                source_spawn_weapon(
                    &weapon,
                    SOURCE_WEAPON_BAY_SPECIAL2,
                    player_source_x,
                    player_source_y
                );
            }
            player_invulnerable = 100;
        }
        break;

    case 13: /* Repair player one */
        {
            u16 repaired = (u16)(
                player_armor + effect_power / 4u + 1u
            );

            player_armor = repaired > 255 ? 255 : (u8)repaired;
            source_parity_level.player_armor = player_armor;
            source_parity_level.frame_sound_queue[3] = 29;
        }
        break;

    case 14: /* Player two is intentionally absent on GBA. */
        break;

    case 17: /* Spawn left or right sidekick */
        source_parity_level.frame_sound_queue[3] = 29;
        if (
            source_parity_level.player_sidekick[0] ==
                source_special_definition_weapon
        ) {
            source_parity_level.player_sidekick[1] =
                (u8)source_special_definition_weapon;
            source_shot_multi_pos[
                SOURCE_WEAPON_BAY_RIGHT_SIDEKICK
            ] = 0;
        } else {
            source_parity_level.player_sidekick[0] =
                (u8)source_special_definition_weapon;
            source_shot_multi_pos[
                SOURCE_WEAPON_BAY_LEFT_SIDEKICK
            ] = 0;
        }
        break;

    case 18: /* Spawn right sidekick */
        source_parity_level.player_sidekick[1] =
            (u8)source_special_definition_weapon;
        source_shot_multi_pos[
            SOURCE_WEAPON_BAY_RIGHT_SIDEKICK
        ] = 0;
        source_parity_level.frame_sound_queue[4] = 29;
        break;

    default:
        break;
    }
}

static u8 source_special_zinglon_half_width_for_duration(u8 duration)
{
    u8 distance;

    if (duration > 50u) duration = 50u;
    distance = duration > 25u ?
        (u8)(duration - 25u) :
        (u8)(25u - duration);
    return (u8)(25u - distance);
}

static u8 source_special_zinglon_damage_due_after_tick(u8 duration)
{
    return (u8)(
        duration > 1u &&
        ((u8)(duration - 1u) % 5u) == 0u
    );
}

static void source_special_flare_update(void)
{
    OtWeaponDefinition weapon;

    /* Backgrounds are drawn before JE_doSpecialShot() decrements Astral. */
    source_special_astral_visual_active =
        source_special_astral_duration != 0;
    if (source_special_astral_duration > 0) {
        source_special_astral_duration--;
    }
    if (source_special_flare_duration > 1) {
        if (source_special_weapon_filter != -99) {
            s8 colour_change =
                ot_level_port_random(&source_parity_level) % 2u == 0 ?
                    -1 : 1;

            if (
                source_parity_level.level_filter == -99 &&
                source_parity_level.level_brightness == -99
            ) {
                source_parity_level.filter_active = false;
            }
            if (!source_parity_level.filter_active) {
                source_parity_level.level_filter =
                    source_special_weapon_filter;
                if (source_parity_level.level_filter == 7) {
                    source_parity_level.level_brightness = 0;
                }
                source_parity_level.filter_active = true;
            }
            if (source_parity_level.level_filter == 7) {
                if (source_parity_level.level_brightness < -6) {
                    colour_change = 1;
                }
                if (source_parity_level.level_brightness > 6) {
                    colour_change = -1;
                }
                source_parity_level.level_brightness = (int16_t)(
                    source_parity_level.level_brightness +
                    colour_change
                );
            }
        }
        if (
            (s32)(ot_level_port_random(&source_parity_level) % 6u) <
                source_special_weapon_frequency &&
            ot_data_hdt_weapon_read(
                source_special_weapon_id,
                &weapon
            ) &&
            source_weapon_is_valid(&weapon)
        ) {
            u8 spawned = 0;

            if (source_special_link_to_player) {
                if (
                    source_shot_repeat[
                        SOURCE_WEAPON_BAY_SPECIAL
                    ] == 0
                ) {
                    spawned = source_spawn_weapon(
                        &weapon,
                        SOURCE_WEAPON_BAY_SPECIAL,
                        player_source_x,
                        player_source_y
                    );
                }
            } else {
                spawned = source_spawn_weapon(
                    &weapon,
                    SOURCE_WEAPON_BAY_SPECIAL,
                    (s16)(
                        ot_level_port_random(
                            &source_parity_level
                        ) % 280u
                    ),
                    (s16)(
                        ot_level_port_random(
                            &source_parity_level
                        ) % 180u
                    )
                );
            }
            if (
                source_special_spray &&
                spawned != 0 &&
                source_last_spawned_shot_index < MAX_PLAYER_SHOTS
            ) {
                PlayerShot *shot = &player_shots[
                    source_last_spawned_shot_index
                ];

                shot->xm = (s16)(
                    ot_level_port_random(&source_parity_level) % 5u
                ) - 2;
                shot->ym = (s16)(
                    ot_level_port_random(&source_parity_level) % 5u
                ) - 2;
                if (shot->ym == 0) shot->ym++;
            }
        }
        source_special_flare_duration--;
        if (source_special_flare_duration == 1) {
            source_special_wait = source_special_next_wait;
        }
    } else if (source_special_flare_start) {
        source_special_flare_start = 0;
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] =
            source_special_link_to_player ? 15 : 200;
        source_special_flare_duration = 0;
        if (
            source_parity_level.level_filter ==
                source_special_weapon_filter
        ) {
            source_parity_level.level_filter = -99;
            source_parity_level.level_brightness = -99;
            source_parity_level.filter_active = false;
        }
    }
    source_special_zinglon_visual_active = 0;
    source_special_zinglon_visual_half_width = 0;
    if (source_special_zinglon_duration > 1) {
        /*
         * varz.c draws the beam with the pre-decrement duration.  Latch
         * that exact source width for the later GBA presentation pass, then
         * advance the lifetime and arm only the authored five-tick damage
         * pulse (shotAvail[MAX_PWEAPON-1]).
         */
        source_special_zinglon_visual_half_width =
            source_special_zinglon_half_width_for_duration(
                source_special_zinglon_duration
            );
        source_special_zinglon_visual_active = 1;
        if (
            source_special_zinglon_damage_due_after_tick(
                source_special_zinglon_duration
            )
        ) {
            source_special_zinglon_damage_pending = 1;
        }
        source_special_zinglon_duration--;
    }
}

static void source_special_update(void)
{
    u8 special_fire;

    (void)source_special_sync();
    /* PC draws graphic 93/94 before decrementing these counters. */
    source_special_ready_indicator = (
        source_parity_level.player_special > 0 &&
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] == 0 &&
        source_special_wait == 0 &&
        source_special_flare_duration < 2 &&
        source_special_zinglon_duration < 2
    );
    if (source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] > 0) {
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL]--;
    }
    if (source_special_wait > 0) source_special_wait--;
    special_fire = source_gameplay_special_fire_held();
    if (
        frontend_demo_active &&
        source_parity_level.player_super_arcade_mode != 0 &&
        (
            frontend_demo_keys &
            (
                FRONTEND_DEMO_KEY_LEFT_SIDEKICK |
                FRONTEND_DEMO_KEY_RIGHT_SIDEKICK
            )
        ) != 0
    ) {
        source_special_fire_held = 0;
    }
    if (!special_fire) {
        source_special_fire_held = 0;
    } else if (
        source_special_valid &&
        source_shot_repeat[SOURCE_WEAPON_BAY_SPECIAL] == 0 &&
        !source_special_fire_held &&
        source_special_flare_duration == 0 &&
        source_special_wait == 0
    ) {
        source_special_fire_held = 1;
        /*
         * varz.c:747..762 equipped-item path: no Shield/Armor payment and
         * no shotMultiPos reset.  Reset/payment are exclusive to SFExecuted
         * at varz.c:694..738; preserving the phase lets Pearl Wind, Xega and
         * the other authored multi-position patterns advance across presses.
         */
        source_special_complete(
            source_special_effect_power_without_payment(
                source_special_power
            )
        );
    }
    source_special_flare_update();
}

static void source_superbomb_update(void)
{
    OtWeaponDefinition weapon;

    if (
        source_shot_repeat[
            SOURCE_WEAPON_BAY_P1_SUPERBOMB
        ] > 0
    ) {
        source_shot_repeat[SOURCE_WEAPON_BAY_P1_SUPERBOMB]--;
    } else if (
        source_parity_level.player_superbombs > 0 &&
        source_gameplay_superbomb_fire_held()
    ) {
        source_parity_level.player_superbombs--;
        source_shot_multi_pos[
            SOURCE_WEAPON_BAY_P1_SUPERBOMB
        ] = 0;
        if (
            ot_data_hdt_weapon_read(535, &weapon) &&
            source_weapon_is_valid(&weapon)
        ) {
            source_spawn_weapon(
                &weapon,
                SOURCE_WEAPON_BAY_P1_SUPERBOMB,
                player_source_x,
                player_source_y
            );
        }
    }
}

static void source_sidekick_fire_update(u8 side)
{
    SourceSidekickRuntime *runtime = &source_sidekick[side];
    u8 bay = (u8)(
        SOURCE_WEAPON_BAY_LEFT_SIDEKICK + side
    );
    u8 fire = 0;

    if (!source_sidekick_sync(side) || !runtime->weapon_valid) {
        return;
    }
    if (source_shot_repeat[bay] != 0) return;

    if (runtime->ammo_max > 0) {
        if (runtime->ammo_refill_ticks > 0) {
            runtime->ammo_refill_ticks--;
        } else {
            runtime->ammo_refill_ticks =
                runtime->ammo_refill_ticks_max;
            if (runtime->ammo < runtime->ammo_max) {
                runtime->ammo++;
            }
        }
        fire =
            source_gameplay_sidekick_fire_held(side) &&
            runtime->ammo > 0;
    } else {
        fire =
            source_gameplay_main_fire_held() ||
            source_gameplay_sidekick_fire_held(side);
    }
    if (!fire) return;
    if (!source_weapon_energy_consume(runtime->poweruse)) return;
    (void)source_spawn_weapon(
        &runtime->weapon,
        bay,
        runtime->x,
        runtime->y
    );
    if (runtime->ammo_max > 0) runtime->ammo--;
    if (runtime->charge > 0) {
        source_shot_multi_pos[bay] = 0;
        runtime->charge = 0;
        runtime->weapon_charge = 0xff;
    }
    runtime->charge_ticks = 20;
    runtime->animation_enabled = 1;
}

static void source_sidekick_animation_update(u8 side)
{
    SourceSidekickRuntime *runtime = &source_sidekick[side];

    if (!runtime->valid) return;
    if (
        runtime->animation_enabled &&
        runtime->option.animation_count > 0
    ) {
        runtime->animation_frame++;
        if (
            runtime->animation_frame >=
                runtime->option.animation_count
        ) {
            runtime->animation_frame = 0;
            runtime->animation_enabled =
                runtime->option.option == 1;
        }
    }
    if (runtime->charge_ticks > 0) runtime->charge_ticks--;
    if (runtime->charge_ticks == 0) {
        if (runtime->charge < runtime->option.power) {
            runtime->charge++;
            runtime->weapon_charge = 0xff;
        }
        runtime->charge_ticks = 20;
    }
}

static void source_player_weapons_update(void)
{
    (void)source_front_weapon_sync();
    (void)source_rear_weapon_sync();
    (void)source_sidekick_sync(0);
    (void)source_sidekick_sync(1);
    if (!source_option_projectile_sync()) {
        source_parity_level.assets_valid = false;
    }
    source_rear_mode_update();
    source_special_update();
    if (source_shot_repeat[SOURCE_WEAPON_BAY_FRONT] > 0) {
        source_shot_repeat[SOURCE_WEAPON_BAY_FRONT]--;
    }
    if (source_shot_repeat[SOURCE_WEAPON_BAY_REAR] > 0) {
        source_shot_repeat[SOURCE_WEAPON_BAY_REAR]--;
    }
    if (
        source_shot_repeat[SOURCE_WEAPON_BAY_LEFT_SIDEKICK] > 0
    ) {
        source_shot_repeat[SOURCE_WEAPON_BAY_LEFT_SIDEKICK]--;
    }
    if (
        source_shot_repeat[SOURCE_WEAPON_BAY_RIGHT_SIDEKICK] > 0
    ) {
        source_shot_repeat[SOURCE_WEAPON_BAY_RIGHT_SIDEKICK]--;
    }
    if (source_gameplay_main_fire_held()) {
        if (
            source_front_weapon_valid &&
            source_shot_repeat[SOURCE_WEAPON_BAY_FRONT] == 0
        ) {
            if (
                source_weapon_energy_consume(
                    source_front_weapon_poweruse
                )
            ) {
                source_spawn_weapon(
                    &source_front_weapon,
                    SOURCE_WEAPON_BAY_FRONT,
                    player_source_x,
                    player_source_y
                );
            }
        }
        if (
            source_rear_weapon_valid &&
            source_shot_repeat[SOURCE_WEAPON_BAY_REAR] == 0
        ) {
            if (
                source_weapon_energy_consume(
                    source_rear_weapon_poweruse
                )
            ) {
                source_spawn_weapon(
                    &source_rear_weapon,
                    SOURCE_WEAPON_BAY_REAR,
                    player_source_x,
                    player_source_y
                );
            }
        }
    }
    source_superbomb_update();
    source_sidekick_positions_update();
    source_sidekick_fire_update(0);
    source_sidekick_fire_update(1);
    source_sidekick_animation_update(0);
    source_sidekick_animation_update(1);
}
#endif

static u8 source_player_arcade_respawn(void)
{
    if (
        !source_parity_level.arcade_mode ||
        source_parity_level.player_front_weapon_power <= 1
    ) {
        return 0;
    }

    /*
     * Direct onePlayerAction branch from mainint.c JE_playerMovement().
     * Arcade aliases lives to front-weapon power, so losing a life also
     * selects the preceding authored weapon-power row.
     */
    source_parity_level.player_front_weapon_power--;
    player_lives = source_parity_level.player_front_weapon_power;
    source_parity_level.really_end_level = false;
    source_parity_level.end_level = false;
    source_parity_level.all_players_gone = false;
    ot_level_port_recalculate_player_power_progress(
        &source_parity_level
    );
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_shot_multi_pos[STRESS_BAY_FRONT] = 0;
#else
    source_shot_multi_pos[SOURCE_WEAPON_BAY_FRONT] = 0;
#endif
    player_source_y = 160;
    player_invulnerable = 100;
    player_alive = 1;
    player_armor = (
        source_parity_level.galaga_mode ||
        frontend_level.episode == 4
    ) ?
        player_initial_armor :
        (u8)(player_initial_armor / 2u);
    player_shield = source_parity_level.galaga_mode ?
        0 :
        (u8)(player_shield_max / 2u);
    source_parity_level.player_armor = player_armor;
    player_death_music_volume = 255;
    player_death_music_fade_active = 0;
    audio_set_source_music_volume(255);
    return 1;
}

static void update_player(void)
{
    s16 old_x = player_source_x;
    s16 old_y = player_source_y;
    s16 accel_x;
    s16 accel_y;
    s16 ship_banking;

#ifdef AUTOTEST_FORCE_PLAYER_DEATH
#ifdef AUTOTEST_SCRIPTED_SURVIVAL_FLOW
    if (
        source_parity_level.engage_mode &&
        player_alive &&
        player_invulnerable == 0
    ) {
#else
    if (level_tick == 2 && player_alive) {
#endif
        player_shield = 0;
        player_armor = 0;
        player_damage(1);
    }
#endif

    if (!player_alive) {
        if (player_exploding_ticks > 0) {
            s16 explosion_x;
            s16 explosion_y;

            /*
             * tyrian2.c evaluates the previous frame's musicFade before
             * JE_playerMovement, then enables it after that movement.  The
             * lethal frame therefore does not fade; the remaining 59 do.
             */
            if (
                player_death_music_fade_active &&
                player_death_music_volume > 10
            ) {
                player_death_music_volume--;
                audio_set_source_music_volume(
                    player_death_music_volume
                );
                telemetry_player_death_music_fade_steps++;
            }

            player_exploding_ticks--;
            if (player_death_fx_wait > 0) {
                player_death_fx_wait--;
            } else {
                player_death_fx_wait = (u8)(
                    ot_level_port_random(&source_parity_level) % 6u + 3u
                );
                if (
                    ot_level_port_random(&source_parity_level) % 3u == 1u
                ) {
                    audio_effect(SFX_EXPLOSION_9);
                    telemetry_player_death_sfx_9++;
                } else {
                    audio_effect(SFX_EXPLOSION_11);
                    telemetry_player_death_sfx_11++;
                }
            }

            explosion_x = (s16)(
                player_source_x +
                (s16)(ot_level_port_random(&source_parity_level) % 32u) -
                16
            );
            explosion_y = (s16)(
                player_source_y +
                (s16)(ot_level_port_random(&source_parity_level) % 32u) -
                16
            );
            spawn_large_effect(
                source_world_to_screen_x(explosion_x),
                source_world_to_screen_y((s16)(explosion_y + 7)),
                0
            );
            telemetry_player_death_large_explosions++;
            spawn_large_effect(
                source_world_to_screen_x(player_source_x),
                source_world_to_screen_y((s16)(player_source_y + 7)),
                0
            );
            telemetry_player_death_large_explosions++;

            if (source_parity_level.level_end > 0) {
                source_parity_level.level_end--;
            }
            /*
             * PC allPlayersGone is true during a Full Game death, but not
             * during an Arcade death which still has a respawn available.
             */
            player_death_music_fade_active =
                !source_parity_level.arcade_mode;
            telemetry_source_rng_calls =
                source_parity_level.rng_call_count;
            if (
                player_exploding_ticks == 0 &&
                !source_parity_level.arcade_mode &&
                game_state == STATE_PLAY
            ) {
                frontend_enter_game_over();
            }
        } else if (game_state == STATE_PLAY) {
            if (!source_player_arcade_respawn()) {
                frontend_enter_game_over();
            }
        }
        if (!player_alive || game_state != STATE_PLAY) return;
    }

    if (source_parity_level.end_level) {
        /*
         * JE_mainGamePlayerFunctions() decrements levelEnd and increments
         * levelEndWarp before JE_playerMovement() performs the exit flight.
         */
        if (source_parity_level.level_end > 0) {
            source_parity_level.level_end--;
            player_end_warp++;
            player_source_y =
                (s16)(player_source_y - player_end_warp);
        }
        if (
            source_parity_level.level_end == 0 ||
            player_source_y < -200 ||
            source_parity_level.really_end_level
        ) {
            frontend_enter_level_stats(1);
        }
        source_camera_update();
        return;
    }

    /*
     * mainint.c decrements an existing invulnerability timer immediately
     * before drawing the blended ship, then may activate a new Special later
     * in the same player update.  Latch the visual state first so the final
     * tick remains visible and a newly activated field begins next frame.
     */
    source_player_invulnerable_visual_active =
        player_invulnerable != 0;
    if (player_invulnerable) player_invulnerable--;

    /*
     * Direct fixed single-player/keyboard slice of JE_playerMovement().
     * GBA D-pad bits replace keysactive[] only; source acceleration,
     * friction, velocity and clamps remain in PC coordinates.
     */
    if (pad_now & KEY_LEFT) player_source_x--;
    if (pad_now & KEY_RIGHT) player_source_x++;
    if (pad_now & KEY_UP) player_source_y--;
    if (pad_now & KEY_DOWN) player_source_y++;
    accel_x = (s16)(player_source_x - old_x);
    accel_y = (s16)(player_source_y - old_y);

    if (player_source_x_friction_ticks > 0) {
        player_source_x_friction_ticks--;
    } else {
        player_source_x_friction_ticks = 1;
        if (player_source_velocity_x < 0) {
            player_source_velocity_x++;
        } else if (player_source_velocity_x > 0) {
            player_source_velocity_x--;
        }
    }
    if (player_source_y_friction_ticks > 0) {
        player_source_y_friction_ticks--;
    } else {
        player_source_y_friction_ticks = 2;
        if (player_source_velocity_y < 0) {
            player_source_velocity_y++;
        } else if (player_source_velocity_y > 0) {
            player_source_velocity_y--;
        }
    }
    player_source_velocity_x += accel_x;
    player_source_velocity_y += accel_y;
    if (player_source_velocity_x < -4) player_source_velocity_x = -4;
    if (player_source_velocity_x > 4) player_source_velocity_x = 4;
    if (player_source_velocity_y < -4) player_source_velocity_y = -4;
    if (player_source_velocity_y > 4) player_source_velocity_y = 4;
    player_source_x = (s16)(
        player_source_x + player_source_velocity_x
    );
    player_source_y = (s16)(
        player_source_y + player_source_velocity_y
    );

    if (player_source_x < SOURCE_PLAYER_MIN_X) {
        player_source_x = SOURCE_PLAYER_MIN_X;
    } else if (player_source_x > SOURCE_PLAYER_MAX_X) {
        player_source_x = SOURCE_PLAYER_MAX_X;
    }
    if (player_source_y < SOURCE_PLAYER_CLAMP_MIN_Y) {
        player_source_y = SOURCE_PLAYER_CLAMP_MIN_Y;
    } else if (player_source_y > SOURCE_PLAYER_CLAMP_MAX_Y) {
        player_source_y = SOURCE_PLAYER_CLAMP_MAX_Y;
    }
    source_camera_update();

    ship_banking = (s16)(
        player_source_velocity_x / 2 +
        (player_source_x - old_x) / 6
    );
    if (ship_banking < -2) ship_banking = -2;
    if (ship_banking > 2) ship_banking = 2;
    player_bank = (s8)ship_banking;

#if TYRIAN_GBA_STRESS_LOADOUT
    stress_player_delta_x = (s16)(player_source_x - old_x);
    stress_player_delta_y = (s16)(player_source_y - old_y);
    stress_sidekick_x[0] = player_source_x;
    stress_sidekick_y[0] = (s16)(player_source_y - 20);
    if (stress_sidekick_y[0] < 10) stress_sidekick_y[0] = 10;
    stress_sidekick_x[1] = (s16)(player_source_x + 16);
    stress_sidekick_y[1] = player_source_y;
    source_special_ready_indicator = (
        source_parity_level.player_special > 0 &&
        stress_shot_repeat[STRESS_BAY_SPECIAL] == 0
    );
    for (u8 bay = 0; bay < STRESS_BAY_COUNT; bay++) {
        if (stress_shot_repeat[bay] > 0) {
            stress_shot_repeat[bay]--;
        }
    }
    if (
        stress_loadout_valid &&
        (pad_now & (KEY_A | KEY_B))
    ) {
        /*
         * Source order: Special, front/rear ports, Super Bomb, sidekicks.
         * The diagnostic deliberately gives the ammo-limited MicroBomb and
         * Super Bomb an infinite supply so all six systems remain active.
         */
        if (!stress_shot_repeat[STRESS_BAY_SPECIAL]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_SPECIAL],
                STRESS_BAY_SPECIAL,
                player_source_x,
                player_source_y
            );
        }
        if (!stress_shot_repeat[STRESS_BAY_FRONT]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_FRONT],
                STRESS_BAY_FRONT,
                player_source_x,
                player_source_y
            );
        }
        if (!stress_shot_repeat[STRESS_BAY_REAR]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_REAR],
                STRESS_BAY_REAR,
                player_source_x,
                player_source_y
            );
        }
        if (!stress_shot_repeat[STRESS_BAY_SUPERBOMB]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_SUPERBOMB],
                STRESS_BAY_SUPERBOMB,
                player_source_x,
                player_source_y
            );
        }
        if (!stress_shot_repeat[STRESS_BAY_LEFT_SIDEKICK]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_LEFT_SIDEKICK],
                STRESS_BAY_LEFT_SIDEKICK,
                stress_sidekick_x[0],
                stress_sidekick_y[0]
            );
        }
        if (!stress_shot_repeat[STRESS_BAY_RIGHT_SIDEKICK]) {
            stress_spawn_weapon(
                &stress_weapon[STRESS_BAY_RIGHT_SIDEKICK],
                STRESS_BAY_RIGHT_SIDEKICK,
                stress_sidekick_x[1],
                stress_sidekick_y[1]
            );
        }
    }
    if (stress_left_option.animation_count > 0) {
        stress_sidekick_animation[0]++;
        if (
            stress_sidekick_animation[0] >=
            stress_left_option.animation_count
        ) {
            stress_sidekick_animation[0] = 0;
        }
    }
    if (stress_right_option.animation_count > 0) {
        stress_sidekick_animation[1]++;
        if (
            stress_sidekick_animation[1] >=
            stress_right_option.animation_count
        ) {
            stress_sidekick_animation[1] = 0;
        }
    }
#else
    source_player_delta_x = (s16)(player_source_x - old_x);
    source_player_delta_y = (s16)(player_source_y - old_y);
    source_player_history_update(old_x, old_y);
    source_player_weapons_update();
#endif
    ot_level_port_update_parallax(&source_parity_level, player_source_x);
}

#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_PLAYER_SHOT_DELTA_X stress_player_delta_x
#define SOURCE_PLAYER_SHOT_DELTA_Y stress_player_delta_y
#else
#define SOURCE_PLAYER_SHOT_DELTA_X source_player_delta_x
#define SOURCE_PLAYER_SHOT_DELTA_Y source_player_delta_y
#endif

static s16 source_superpixel_vector_component(
    u8 phase,
    u32 random_value,
    u8 width
)
{
    s32 product =
        (s32)((const s16 *)jukebox_sine)[phase] *
        (s32)(random_value >> 24) *
        width;

    /* Q15 sine * Q8 uniform radius, with roundf()-equivalent rounding. */
    if (product >= 0) {
        return (s16)((product + (1L << 22)) >> 23);
    }
    return (s16)-(((-product) + (1L << 22)) >> 23);
}

static void source_superpixel_spawn(
    s16 x,
    s16 y,
    u8 count,
    u8 width,
    u8 color
)
{
    u8 index;

    /* Direct fixed-point translation of varz.c::JE_doSP(). */
    for (index = 0; index < count; index++) {
        u32 random_y;
        u32 random_x;
        u8 phase = (u8)(
            ot_level_port_random3(
                &source_parity_level,
                &random_y,
                &random_x
            ) >> 24
        );
        s16 delta_y = source_superpixel_vector_component(
            (u8)(phase + 64u),
            random_y,
            width
        );
        s16 delta_x = source_superpixel_vector_component(
            phase,
            random_x,
            width
        );
        SourceSuperpixel *pixel;

        source_last_superpixel++;
        if (source_last_superpixel >= MAX_SUPERPIXELS) {
            source_last_superpixel = 0;
        }
        pixel = &source_superpixels[source_last_superpixel];
        if (pixel->z == 0 && source_active_superpixels < MAX_SUPERPIXELS) {
            source_active_superpixels++;
        }
        pixel->x = (s16)(x + delta_x);
        pixel->y = (s16)(y + delta_y);
        pixel->delta_x = (s8)delta_x;
        pixel->delta_y = (s8)(delta_y + 1);
        pixel->color = color;
        /*
         * Logic precedes the GBA presentation pass.  One private bias step
         * preserves JE_drawSP's visible 15..1 sequence while still aging on
         * every logic tick (PC draws at z, then decrements in one function).
         */
        pixel->z = 16;
    }
}

static void source_superpixel_update(void)
{
    s16 index;

    if (source_active_superpixels == 0) return;
    /* JE_drawSP() iterates 100..0, moves first, draws, then ages. */
    for (index = MAX_SUPERPIXELS - 1; index >= 0; index--) {
        SourceSuperpixel *pixel = &source_superpixels[index];

        if (pixel->z == 0) continue;
        pixel->x = (s16)(pixel->x + pixel->delta_x);
        pixel->y = (s16)(pixel->y + pixel->delta_y);
        pixel->z--;
        if (pixel->z == 0 && source_active_superpixels != 0) {
            source_active_superpixels--;
        }
    }
}

#include "combat_player_shot_update.inc"

#undef SOURCE_PLAYER_SHOT_DELTA_X
#undef SOURCE_PLAYER_SHOT_DELTA_Y

static void update_rewards(void)
{
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    u8 index;
    Reward *reward;
    for (index = 0; index < MAX_REWARDS; index++) {
        reward = &rewards[index];
        if (!reward->active) continue;
        reward->phase++;
        if (
            reward->phase >=
            reward_frame_delay_table[reward->sequence]
        ) {
            reward->phase = 0;
            reward->frame++;
            if (reward->frame >= REWARD_FRAME_COUNT) reward->frame = 0;
        }
        /* Original spriteSheet11 score items use ymove=1. */
        reward->y++;
        if (reward->y > 160) {
            reward->active = 0;
            if (active_reward_count) active_reward_count--;
            continue;
        }
        if (BOX_OVERLAPS(
                reward->x + 2, reward->y + 1, 12, 14,
                source_player_screen_x() + 6,
                source_player_screen_y() + 5,
                20, 23)) {
            player_cash += reward->value;
            reward->active = 0;
            if (active_reward_count) active_reward_count--;
            telemetry_reward_pickups++;
            audio_effect(SFX_ITEM);
        }
    }
#endif
}

static void update_effects(void)
{
    u8 index;
    for (index = 0; index < effect_slot_high_water; index++) {
        if (!effects[index].active) continue;
        effects[index].y += (
            bg2_scroll_speed ? bg2_scroll_speed : bg1_scroll_speed
        );
        if (effects[index].frame + 1 < EXPLOSION_FRAME_COUNT) {
            effects[index].frame++;
        } else {
            effects[index].active = 0;
#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
            source_pool_mask_clear(effect_active_mask, index);
#endif
            if (active_effect_count) active_effect_count--;
        }
    }
    while (
        effect_slot_high_water > 0 &&
        !effects[effect_slot_high_water - 1].active
    ) {
        effect_slot_high_water--;
    }
}
