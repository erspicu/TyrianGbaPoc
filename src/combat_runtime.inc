/* Legacy player/enemy combat runtime retained until source-parity replacement. */

static u8 source_weapon_is_valid(const OtWeaponDefinition *weapon)
{
    return
        weapon->multi > 0 &&
        weapon->multi <= 8 &&
        weapon->max > 0 &&
        weapon->max <= 8;
}

#if !TYRIAN_GBA_STRESS_LOADOUT
static const u8 source_weapon_sound_slot[SOURCE_WEAPON_BAY_COUNT] = {
    0, 2, 4, 4, 2
};

static u8 source_front_weapon_sync(void)
{
    OtWeaponPortDefinition front_port;
    u8 port_id = source_parity_level.player_front_weapon_id;
    u8 power = source_parity_level.player_front_weapon_power;
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
    source_rear_weapon_valid =
        weapon_id != 0 &&
        ot_data_hdt_weapon_read(weapon_id, &source_rear_weapon) &&
        source_weapon_is_valid(&source_rear_weapon);
    return source_rear_weapon_valid;
}

static u8 source_sidekick_sync(u8 side)
{
    SourceSidekickRuntime *runtime;
    u8 item_id;
    u16 weapon_id;

    if (side > 1) return 0;
    runtime = &source_sidekick[side];
    item_id = source_parity_level.player_sidekick[side];
    if (runtime->item_id != item_id) {
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
    if (runtime->option.weapon_port == 0) return 1;
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
    source_front_weapon_valid = 0;
    source_rear_weapon_bound = 0;
    source_rear_weapon_port_id = 0;
    source_rear_weapon_power = 0;
    source_rear_weapon_mode = 0;
    source_rear_weapon_hdt_id = 0;
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
    stress_sidekick_x[0] = player_source_x;
    stress_sidekick_y[0] = (s16)(player_source_y - 20);
    stress_sidekick_x[1] = (s16)(player_source_x + 16);
    stress_sidekick_y[1] = player_source_y;
    stress_player_delta_x = 0;
    stress_player_delta_y = 0;
}

static void stress_update_shot_peak(void)
{
    u8 index;
    u8 active = 0;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (player_shots[index].active) active++;
    }
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

        for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
            if (!player_shots[shot_index].active) {
                shot = &player_shots[shot_index];
                break;
            }
        }
        if (shot == 0) {
            telemetry_player_shot_drops++;
            break;
        }

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
        shot->active = 1;
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
        attack = weapon->attack[position];
        if (attack > 99 && attack < 250) {
            shot->chain_weapon = (u16)(attack - 100u);
            shot->damage = 1;
        } else if (attack >= 250) {
            shot->damage = (u8)(attack - 250u);
            shot->infinite = 1;
        } else {
            shot->damage = attack;
        }
        if (shot->damage == 0) shot->damage = 1;
        telemetry_player_shot_spawns++;
        spawned++;
    }
    stress_shot_repeat[bay] = weapon->shotrepeat;
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
    u8 multi;
    u8 spawned = 0;

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

        for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
            if (!player_shots[index].active) {
                shot = &player_shots[index];
                break;
            }
        }
        if (shot == 0) {
            telemetry_player_shot_drops++;
            break;
        }
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
                    (s16)(origin_x - player_source_x),
                    -5,
                    5
                )
            );
        }
        if (delay == 99 || delay == 100) {
            shot->ym = source_clamp_s16_to_s8(
                (s16)(
                    origin_y -
                    player_source_y -
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
        if (attack > 99 && attack < 250) {
            shot->chain_weapon = (u16)(attack - 100u);
            shot->damage = 1;
        } else if (attack >= 250) {
            shot->damage = (u8)(attack - 250u);
            shot->infinite = 1;
        } else {
            shot->damage = attack;
        }
        if (shot->damage == 0) shot->damage = 1;
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
                    enemy->ex + enemy->mapoffset - shot->x
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
        shot->active = shot->graphic != 0 && delay != 0;
        if (shot->active) {
            telemetry_player_shot_spawns++;
            spawned++;
        }
    }
    source_shot_repeat[bay] = weapon->shotrepeat;
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
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (!player_shots[index].active) {
            shot = &player_shots[index];
            break;
        }
    }
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
    shot->damage = weapon.attack[0];
    if (shot->damage == 0) shot->damage = 1;
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
    shot->active = shot->graphic != 0 && delay != 0;
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
static void player_damage(u16 damage)
{
    u16 remaining = damage;

    if (!player_alive || player_invulnerable || remaining == 0) return;
    telemetry_collision_count++;
    source_parity_level.frame_sound_queue[7] = 27;
#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
    /*
     * Development validation still records the collision, but deliberately
     * leaves the translated shield, armor and death state untouched.
     */
    return;
#endif
#if defined(AUTOTEST) && !defined(AUTOTEST_FORCE_PLAYER_DEATH)
    /*
     * The deterministic source-event audit must reach the authored boss and
     * level-exit records; release builds retain the translated damage path.
     */
    return;
#endif
    if (player_shield < remaining) {
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
    }
    source_parity_level.player_armor = player_armor;
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
            left->x = (s16)(player_source_x - 14);
            left->y = player_source_y;
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
            right->x = (s16)(player_source_x + 16);
            right->y = player_source_y;
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
                    if (pad_now & KEY_L) {
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
                if (pad_pressed & KEY_L) {
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
        !(pad_pressed & KEY_L) ||
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

static void source_sidekick_fire_update(u8 side, u16 button)
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
        fire = (pad_now & button) != 0 && runtime->ammo > 0;
    } else {
        fire = (pad_now & (KEY_A | button)) != 0;
    }
    if (!fire) return;
    if (
        source_spawn_weapon(
            &runtime->weapon,
            bay,
            runtime->x,
            runtime->y
        ) == 0
    ) {
        return;
    }
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
    u8 bay;

    (void)source_front_weapon_sync();
    (void)source_rear_weapon_sync();
    (void)source_sidekick_sync(0);
    (void)source_sidekick_sync(1);
    if (!source_option_projectile_sync()) {
        source_parity_level.assets_valid = false;
    }
    source_rear_mode_update();
    for (bay = 0; bay < SOURCE_WEAPON_BAY_COUNT; bay++) {
        if (source_shot_repeat[bay] > 0) {
            source_shot_repeat[bay]--;
        }
    }
    if (pad_now & KEY_A) {
        if (
            source_front_weapon_valid &&
            source_shot_repeat[SOURCE_WEAPON_BAY_FRONT] == 0
        ) {
            source_spawn_weapon(
                &source_front_weapon,
                SOURCE_WEAPON_BAY_FRONT,
                player_source_x,
                player_source_y
            );
        }
        if (
            source_rear_weapon_valid &&
            source_shot_repeat[SOURCE_WEAPON_BAY_REAR] == 0
        ) {
            source_spawn_weapon(
                &source_rear_weapon,
                SOURCE_WEAPON_BAY_REAR,
                player_source_x,
                player_source_y
            );
        }
    }
    source_sidekick_fire_update(0, KEY_B);
    source_sidekick_fire_update(1, KEY_R);
    source_sidekick_animation_update(0);
    source_sidekick_animation_update(1);
}
#endif

static void update_player(void)
{
    s16 old_x = player_source_x;
    s16 old_y = player_source_y;
    s16 accel_x;
    s16 accel_y;
    s16 ship_banking;

#ifdef AUTOTEST_FORCE_PLAYER_DEATH
    if (level_tick == 2 && player_alive) {
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
                (s16)(explosion_x - SOURCE_PRESENTATION_X_ORIGIN),
                (s16)(
                    explosion_y + 7 -
                    SOURCE_PRESENTATION_Y_ORIGIN
                ),
                0
            );
            telemetry_player_death_large_explosions++;
            spawn_large_effect(
                (s16)(player_source_x - SOURCE_PRESENTATION_X_ORIGIN),
                (s16)(
                    player_source_y + 7 -
                    SOURCE_PRESENTATION_Y_ORIGIN
                ),
                0
            );
            telemetry_player_death_large_explosions++;

            if (source_parity_level.level_end > 0) {
                source_parity_level.level_end--;
            }
            player_death_music_fade_active = 1;
            telemetry_source_rng_calls =
                source_parity_level.rng_call_count;
            if (
                player_exploding_ticks == 0 &&
                game_state == STATE_PLAY
            ) {
                /*
                 * Full Game starts with exactly one life.  Unlike Arcade,
                 * there is no JE_playerMovement respawn branch here.
                 */
                frontend_enter_game_over();
            }
        } else if (game_state == STATE_PLAY) {
            /*
             * onePlayerAction aliases lives to front-weapon power.  A fresh
             * Pulse-Cannon starts at power/lives 1, so neither requested mode
             * has an extra life in the selected-level source port.
             */
            frontend_enter_game_over();
        }
        return;
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
        return;
    }

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
    player_source_velocity_x = (s8)(
        player_source_velocity_x + accel_x
    );
    player_source_velocity_y = (s8)(
        player_source_velocity_y + accel_y
    );
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
    if (player_source_y < SOURCE_PLAYER_MIN_Y) {
        player_source_y = SOURCE_PLAYER_MIN_Y;
    } else if (player_source_y > SOURCE_PLAYER_MAX_Y) {
        player_source_y = SOURCE_PLAYER_MAX_Y;
    }

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
    source_sidekick_positions_update();
    source_player_weapons_update();
#endif
    if (player_invulnerable) player_invulnerable--;
    ot_level_port_update_parallax(&source_parity_level, player_source_x);
}

#if TYRIAN_GBA_STRESS_LOADOUT
#define SOURCE_PLAYER_SHOT_DELTA_X stress_player_delta_x
#define SOURCE_PLAYER_SHOT_DELTA_Y stress_player_delta_y
#else
#define SOURCE_PLAYER_SHOT_DELTA_X source_player_delta_x
#define SOURCE_PLAYER_SHOT_DELTA_Y source_player_delta_y
#endif

static void update_shots(void)
{
    u8 index;
    PlayerShot *player_shot;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shot = &player_shots[index];
        if (!player_shot->active) continue;
        if (player_shot->ttl == 0) {
            player_shot->active = 0;
            continue;
        }
        player_shot->ttl--;
        player_shot->xm = (s16)(
            player_shot->xm + player_shot->xc
        );
        player_shot->x = (s16)(
            player_shot->x + player_shot->xm
        );
        if (player_shot->xm > 100) {
            if (player_shot->xm == 101) {
                player_shot->x = (s16)(
                    player_shot->x - 101 +
                    SOURCE_PLAYER_SHOT_DELTA_X
                );
                player_shot->y = (s16)(
                    player_shot->y + SOURCE_PLAYER_SHOT_DELTA_Y
                );
            } else {
                player_shot->x = (s16)(
                    player_shot->x - 120 +
                    SOURCE_PLAYER_SHOT_DELTA_X
                );
            }
        }
        player_shot->ym = (s16)(
            player_shot->ym + player_shot->yc
        );
        player_shot->y = (s16)(
            player_shot->y + player_shot->ym
        );
        if (player_shot->ym > 100) {
            player_shot->y = (s16)(
                player_shot->y - 120 +
                SOURCE_PLAYER_SHOT_DELTA_Y
            );
        }
        if (player_shot->complicated) {
            player_shot->dev_x =
                (s8)(player_shot->dev_x + player_shot->dir_x);
            player_shot->x =
                (s16)(player_shot->x + player_shot->dev_x);
            if (
                (player_shot->dev_x < 0 ?
                    -player_shot->dev_x :
                    player_shot->dev_x) ==
                player_shot->circle_size_x
            ) {
                player_shot->dir_x = (s8)-player_shot->dir_x;
            }
            player_shot->dev_y =
                (s8)(player_shot->dev_y + player_shot->dir_y);
            player_shot->y =
                (s16)(player_shot->y + player_shot->dev_y);
            if (
                (player_shot->dev_y < 0 ?
                    -player_shot->dev_y :
                    player_shot->dev_y) ==
                player_shot->circle_size_y
            ) {
                player_shot->dir_y = (s8)-player_shot->dir_y;
            }
        }
        if (
            player_shot->x < -34 ||
            player_shot->x > 290 ||
            player_shot->y < -15 ||
            player_shot->y > 190
        ) {
            player_shot->active = 0;
            continue;
        }
        if (player_shot->trail != 255) {
            source_spawn_explosion(
                player_shot->trail == 98 ?
                    (s16)(player_shot->x - player_shot->xm) :
                    player_shot->x,
                player_shot->trail == 98 ?
                    (s16)(player_shot->y - player_shot->ym) :
                    player_shot->y,
                0,
                player_shot->trail,
                0
            );
        }
        if (player_shot->aim_enemy != 0) {
            if (player_shot->aim_delay > 0) {
                player_shot->aim_delay--;
            }
            if (player_shot->aim_delay == 0) {
                u8 enemy_index =
                    (u8)(player_shot->aim_enemy - 1u);

                player_shot->aim_delay =
                    player_shot->aim_delay_max;
                if (
                    enemy_index < OT_ENEMY_COUNT &&
                    source_parity_level.enemy_avail[enemy_index] != 1
                ) {
                    const OtEnemy *enemy =
                        &source_parity_level.enemy[enemy_index];
                    s16 target_x = (s16)(
                        enemy->ex + enemy->mapoffset
                    );

                    if (player_shot->x < target_x) {
                        player_shot->xm++;
                    } else {
                        player_shot->xm--;
                    }
                    if (player_shot->y < enemy->ey) {
                        player_shot->ym++;
                    } else {
                        player_shot->ym--;
                    }
                } else if (player_shot->xm > 0) {
                    player_shot->xm++;
                } else {
                    player_shot->xm--;
                }
            }
        }
        player_shot->render_graphic = (u16)(
            player_shot->graphic + player_shot->animation
        );
        player_shot->animation++;
        if (
            player_shot->animation >=
            player_shot->animation_max
        ) {
            player_shot->animation = 0;
        }
    }
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_update_shot_peak();
#endif
}

#undef SOURCE_PLAYER_SHOT_DELTA_X
#undef SOURCE_PLAYER_SHOT_DELTA_Y

static void update_rewards(void)
{
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
