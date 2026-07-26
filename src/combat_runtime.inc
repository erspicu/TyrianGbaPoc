/* Legacy player/enemy combat runtime retained until source-parity replacement. */

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
#ifdef AUTOTEST
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
                source_parity_level.level_end = 40;
                audio_effect(SFX_EXPLOSION_9);
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

    if (!player_alive) {
        if (player_exploding_ticks > 0) {
            s16 centre_x = (s16)(source_player_screen_x() + 16);
            s16 centre_y = (s16)(source_player_screen_y() + 16);

            player_exploding_ticks--;
            if ((player_exploding_ticks & 3u) == 0) {
                s16 offset_x =
                    (s16)(((player_exploding_ticks * 13u) & 31u) - 16);
                s16 offset_y =
                    (s16)(((player_exploding_ticks * 7u) & 31u) - 16);

                spawn_large_effect(
                    (s16)(centre_x + offset_x),
                    (s16)(centre_y + offset_y),
                    0
                );
            }
            if ((player_exploding_ticks % 9u) == 0) {
                audio_effect(SFX_EXPLOSION_9);
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

    if (fire_cooldown) fire_cooldown--;
    if ((pad_now & (KEY_A | KEY_B)) && !fire_cooldown) {
        spawn_player_shot();
        fire_cooldown = PLAYER_SHOT_COOLDOWN;
    }
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
        player_shot->y -= PLAYER_SHOT_SPEED;
        if (player_shot->y < 0) player_shot->active = 0;
    }
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
    for (index = 0; index < MAX_EFFECTS; index++) {
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
}
