/* Legacy player/enemy combat runtime retained until source-parity replacement. */

#if TYRIAN_GBA_STRESS_LOADOUT
static u8 stress_weapon_is_valid(const OtWeaponDefinition *weapon)
{
    return
        weapon->multi > 0 &&
        weapon->multi <= 8 &&
        weapon->max > 0 &&
        weapon->max <= 8;
}

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
            !stress_weapon_is_valid(&stress_weapon[bay])
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
        !stress_weapon_is_valid(weapon)
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
static void spawn_player_shot(void)
{
    u8 index;
    if (!player_alive || source_parity_level.end_level) return;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (!player_shots[index].active) {
            player_shots[index].active = 1;
            player_shots[index].x = player_source_x;
            player_shots[index].y = player_source_y;
            /* Pulse-Cannon power 1, HDT weapon 155 attack[0]. */
            player_shots[index].damage = 2;
            audio_effect(SFX_WEAPON_1);
            return;
        }
    }
}
#endif

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
                audio_effect(SFX_EXPLOSION_22);
                telemetry_player_death_sfx_22++;
            } else {
                player_armor = (u8)(player_armor - remaining);
                audio_effect(SFX_ENEMY_HIT);
            }
        }
    } else {
        player_shield = (u8)(player_shield - remaining);
        audio_effect(SFX_ENEMY_HIT);
    }
    source_parity_level.player_armor = player_armor;
}

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
             * has an extra life in the first-level proof.
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
    player_bank = ship_banking < 0 ? -1 : ship_banking > 0 ? 1 : 0;

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
    if (fire_cooldown) fire_cooldown--;
    if ((pad_now & (KEY_A | KEY_B)) && !fire_cooldown) {
        spawn_player_shot();
        fire_cooldown = PLAYER_SHOT_COOLDOWN;
    }
#endif
    if (player_invulnerable) player_invulnerable--;
    ot_level_port_update_parallax(&source_parity_level, player_source_x);
}

static void update_shots(void)
{
    u8 index;
    PlayerShot *player_shot;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shot = &player_shots[index];
        if (!player_shot->active) continue;
#if TYRIAN_GBA_STRESS_LOADOUT
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
                    stress_player_delta_x
                );
                player_shot->y = (s16)(
                    player_shot->y + stress_player_delta_y
                );
            } else {
                player_shot->x = (s16)(
                    player_shot->x - 120 +
                    stress_player_delta_x
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
                stress_player_delta_y
            );
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
#else
        player_shot->y -= PLAYER_SHOT_SPEED;
        if (player_shot->y < 0) player_shot->active = 0;
#endif
    }
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_update_shot_peak();
#endif
}

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
