/* Legacy player/enemy combat runtime retained until source-parity replacement. */

static void spawn_player_shot(void)
{
    u8 index;
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        if (!player_shots[index].active) {
            player_shots[index].active = 1;
            player_shots[index].x = player_x + PLAYER_SHOT_X_OFFSET;
            player_shots[index].y = player_y + PLAYER_SHOT_Y_OFFSET;
            audio_effect(SFX_WEAPON_1);
            return;
        }
    }
}

static const WeaponDef *find_weapon_def(u8 id)
{
    u8 index;
    for (index = 0; index < sizeof(weapon_defs) / sizeof(weapon_defs[0]); index++) {
        if (weapon_defs[index].id == id) return &weapon_defs[index];
    }
    return 0;
}

static s8 aim_component(s16 delta, u16 maximum, u8 magnitude)
{
    u16 absolute;
    u16 scaled;
    if (!maximum) return 0;
    absolute = delta < 0 ? (u16)-delta : (u16)delta;
    scaled = (u16)(absolute * magnitude);
    scaled = (u16)((scaled + maximum / 2) / maximum);
    return delta < 0 ? -(s8)scaled : (s8)scaled;
}

static u8 spawn_enemy_shot(
    s16 x,
    s16 y,
    s8 dx,
    s8 dy,
    u8 graphic,
    u8 animax
)
{
    u8 index;
    u8 scan;
    u8 active_count;
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        if (!enemy_shots[index].active) {
            enemy_shots[index].active = 1;
            enemy_shots[index].x = x;
            enemy_shots[index].y = y;
            enemy_shots[index].dx = dx;
            enemy_shots[index].dy = dy;
            enemy_shots[index].duration = 255;
            enemy_shots[index].graphic = graphic;
            enemy_shots[index].animate = 0;
            enemy_shots[index].animax = animax;
            telemetry_enemy_shots_spawned++;
            active_count = 0;
            for (scan = 0; scan < MAX_ENEMY_SHOTS; scan++) {
                if (enemy_shots[scan].active) active_count++;
            }
            if (active_count > telemetry_max_enemy_shots) {
                telemetry_max_enemy_shots = active_count;
            }
            return 1;
        }
    }
    telemetry_enemy_shot_drops++;
    return 0;
}

static void fire_enemy_weapon(
    s16 origin_x,
    s16 origin_y,
    u8 weapon_id,
    u8 slot,
    u8 *multi_pos
)
{
    const WeaponDef *weapon = find_weapon_def(weapon_id);
    const WeaponShot *pattern;
    s16 delta_x;
    s16 delta_y;
    u16 magnitude_x;
    u16 magnitude_y;
    u16 maximum;
    s8 dx;
    s8 dy;
    u8 shot_index;
    u8 position;
    u8 spawned = 0;

    if (!weapon) return;
    for (shot_index = 0; shot_index < weapon->multi; shot_index++) {
        position = (u8)(*multi_pos + 1);
        if (position > weapon->maximum) position = 1;
        *multi_pos = position;
        pattern = &weapon_shots[weapon->first + position - 1];

        if (slot == 2) {
            dx = -pattern->sy;
            dy = -pattern->sx;
        } else if (slot == 1) {
            dx = pattern->sy;
            dy = -pattern->sx;
        } else {
            dx = pattern->sx;
            dy = pattern->sy;
        }

        if (weapon->aim) {
            delta_x = (player_x + 16) - origin_x;
            delta_y = (player_y + 14) - origin_y;
            magnitude_x = delta_x < 0 ? (u16)-delta_x : (u16)delta_x;
            magnitude_y = delta_y < 0 ? (u16)-delta_y : (u16)delta_y;
            maximum = magnitude_x > magnitude_y ? magnitude_x : magnitude_y;
            dx = aim_component(delta_x, maximum, weapon->aim);
            dy = aim_component(delta_y, maximum, weapon->aim);
        }

        if (!spawn_enemy_shot(
                origin_x + pattern->bx,
                origin_y + pattern->by,
                dx,
                dy,
                pattern->graphic,
                weapon->animax)) {
            break;
        }
        spawned = 1;
    }
    if (spawned) audio_effect(weapon->sound);
}

static void player_hit(void)
{
    if (player_invulnerable) return;
    player_invulnerable = 45;
    telemetry_collision_count++;
    audio_effect(SFX_ENEMY_HIT);
}

static void update_player(void)
{
    if ((pad_now & KEY_LEFT) && player_x > 0) player_x -= 3;
    if ((pad_now & KEY_RIGHT) && player_x < 208) player_x += 3;
    if ((pad_now & KEY_UP) && player_y > 8) player_y -= 3;
    if ((pad_now & KEY_DOWN) && player_y < 128) player_y += 3;

    if (fire_cooldown) fire_cooldown--;
    if ((pad_now & (KEY_A | KEY_B)) && !fire_cooldown) {
        spawn_player_shot();
        fire_cooldown = PLAYER_SHOT_COOLDOWN;
    }
    if (player_invulnerable) player_invulnerable--;

    /*
     * Tyrian ship graphics 233 and 235 are neutral and right-bank poses,
     * not alternating animation frames.  Toggling them every logic update
     * made the player's silhouette flash at about 34.78 Hz.
     */
    if ((pad_now & KEY_LEFT) && !(pad_now & KEY_RIGHT)) {
        player_bank = -1;
    } else if ((pad_now & KEY_RIGHT) && !(pad_now & KEY_LEFT)) {
        player_bank = 1;
    } else {
        player_bank = 0;
    }
}

static void update_shots(void)
{
    u8 index;
    PlayerShot *player_shot;
    EnemyShot *enemy_shot;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        player_shot = &player_shots[index];
        if (!player_shot->active) continue;
        player_shot->y -= PLAYER_SHOT_SPEED;
        if (player_shot->y < -16) player_shot->active = 0;
    }
    for (index = 0; index < MAX_ENEMY_SHOTS; index++) {
        enemy_shot = &enemy_shots[index];
        if (!enemy_shot->active) continue;
        enemy_shot->x += enemy_shot->dx;
        enemy_shot->y += enemy_shot->dy;
        if (
            enemy_shot->duration-- == 0 ||
            enemy_shot->x < -16 || enemy_shot->x > 240 ||
            enemy_shot->y > 168
        ) {
            enemy_shot->active = 0;
            continue;
        }
        if (BOX_OVERLAPS(
                enemy_shot->x, enemy_shot->y, 8, 8,
                player_x + 6, player_y + 5, 20, 23)) {
            enemy_shot->active = 0;
            player_hit();
            continue;
        }
        if (enemy_shot->animax &&
            ++enemy_shot->animate >= enemy_shot->animax) {
            enemy_shot->animate = 0;
        }
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
                player_x + 6, player_y + 5, 20, 23)) {
            player_cash += reward->value;
            reward->active = 0;
            if (active_reward_count) active_reward_count--;
            telemetry_reward_pickups++;
            audio_effect(SFX_ITEM);
        }
    }
}

static void update_enemies(void)
{
    u8 index;
    u8 slot;
    s8 scroll_step;
    Enemy *enemy;

    for (index = 0; index < MAX_ENEMIES; index++) {
        enemy = &enemies[index];
        if (!enemy->active) continue;
        enemy->phase++;
        if ((enemy->phase & 7) == 0) {
            if (enemy->accel_x > 0 && enemy->dx < 3) enemy->dx++;
            if (enemy->accel_x < 0 && enemy->dx > -3) enemy->dx--;
            if (enemy->accel_y > 0 && enemy->dy < 5) enemy->dy++;
            if (enemy->accel_y < 0 && enemy->dy > -3) enemy->dy--;
        }
        enemy->x += enemy->dx;
        scroll_step = 0;
        if (enemy->pool == 1 || enemy->pool == 3) {
            scroll_step = bg1_step;
        } else if (enemy->pool == 2) {
            scroll_step = bg3_step;
        }
        enemy->y += enemy->dy + enemy->fixed_dy + scroll_step;
        /*
         * PC defaults its bounce window to +/-10000.  The old GBA clamp at
         * x=0/208 made formations reverse at the viewport edge and could
         * pin components in place.  Keep the PC lifetime bounds instead.
         */
        if (
            enemy->x < -80 || enemy->x > 320 ||
            enemy->y < -112 || enemy->y > 190
        ) {
            enemy->active = 0;
            continue;
        }
        /*
         * PC order is slot 3 -> 1. A populated turret starts at wait 20,
         * then reloads from its own frequency after each discharge.
         */
        for (slot = 3; slot > 0; slot--) {
            u8 firing_slot = (u8)(slot - 1);
            if (!enemy->frequency[firing_slot]) continue;
            enemy->fire_wait[firing_slot]--;
            if (
                enemy->fire_wait[firing_slot] == 0 &&
                enemy->turret[firing_slot]
            ) {
                enemy->fire_wait[firing_slot] =
                    enemy->frequency[firing_slot];
                fire_enemy_weapon(
                    enemy->x,
                    enemy->y,
                    enemy->turret[firing_slot],
                    firing_slot,
                    &enemy->multi_pos[firing_slot]
                );
            }
        }
        if (BOX_OVERLAPS(
                enemy->x + 4, enemy->y + 4, 24, 24,
                player_x + 6, player_y + 5, 20, 23)) {
            enemy->active = 0;
            spawn_enemy_effect(enemy);
            player_hit();
        }
    }
}

static void collide_player_shots(void)
{
    u8 enemy_index;
    u8 shot_index;
    Enemy *enemy;
    PlayerShot *shot;

    for (shot_index = 0; shot_index < MAX_PLAYER_SHOTS; shot_index++) {
        shot = &player_shots[shot_index];
        if (!shot->active) continue;
        for (enemy_index = 0; enemy_index < MAX_ENEMIES; enemy_index++) {
            enemy = &enemies[enemy_index];
            if (!enemy->active) continue;
            if (BOX_OVERLAPS(
                    shot->x + PLAYER_SHOT_HIT_X,
                    shot->y + PLAYER_SHOT_HIT_Y,
                    PLAYER_SHOT_HIT_WIDTH,
                    PLAYER_SHOT_HIT_HEIGHT,
                    enemy->x + 3, enemy->y + 3, 26, 26)) {
                shot->active = 0;
                telemetry_collision_count++;
                audio_effect(SFX_ENEMY_HIT);
                if (enemy->hp) enemy->hp--;
                if (!enemy->hp) {
                    spawn_enemy_reward(enemy);
                    player_cash += enemy->kill_value;
                    telemetry_kill_cash += enemy->kill_value;
                    enemy->active = 0;
                    spawn_enemy_effect(enemy);
                    audio_effect(SFX_EXPLOSION_9);
                }
                break;
            }
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
