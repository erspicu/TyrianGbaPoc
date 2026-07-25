/* Legacy entity allocation and control, replaced incrementally by the direct port. */

static void spawn_effect(s16 x, s16 y, u8 sequence)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) {
            effects[index].active = 1;
            effects[index].x = x;
            effects[index].y = y;
            effects[index].frame = 0;
            effects[index].sequence = sequence;
            active_effect_count++;
            if (active_effect_count > telemetry_max_effects) {
                telemetry_max_effects = active_effect_count;
            }
            return;
        }
    }
    telemetry_effect_drops++;
}

static void spawn_large_effect(s16 x, s16 y, u8 ground)
{
    u8 sequence = ground ?
        EXPLOSION_SEQUENCE_GROUND_TOP_LEFT :
        EXPLOSION_SEQUENCE_AIR_TOP_LEFT;
    spawn_effect(x - 6, y - 14, sequence);
    spawn_effect(x + 6, y - 14, sequence + 1);
    spawn_effect(x - 6, y, sequence + 2);
    spawn_effect(x + 6, y, sequence + 3);
}

static void spawn_enemy_effect(const Enemy *enemy)
{
    u8 type = enemy->type < ENEMY_ARCHETYPES ? enemy->type : 0;
    if (enemy_large_explosion[type]) {
        spawn_large_effect(
            enemy->x + 16,
            enemy->y + 16,
            enemy_ground_explosion[type]
        );
    } else {
        spawn_effect(
            enemy->x + 8,
            enemy->y + 8,
            EXPLOSION_SEQUENCE_SMALL
        );
    }
}

static void spawn_reward(s16 x, s16 y, u8 code)
{
    u8 index;
    Reward *reward;
    if (code == 0 || code > REWARD_SEQUENCE_COUNT) return;
    for (index = 0; index < MAX_REWARDS; index++) {
        if (!rewards[index].active) {
            reward = &rewards[index];
            reward->active = 1;
            reward->x = x;
            reward->y = y;
            reward->frame = 0;
            reward->sequence = code - 1;
            reward->phase = 0;
            reward->value = reward_value_table[code];
            active_reward_count++;
            telemetry_reward_spawns++;
            if (active_reward_count > telemetry_max_rewards) {
                telemetry_max_rewards = active_reward_count;
            }
            return;
        }
    }
    telemetry_reward_drops++;
}

static void spawn_enemy_reward(const Enemy *enemy)
{
    spawn_reward(enemy->x + 8, enemy->y + 8, enemy->reward);
}

static s8 decode_component(u8 value)
{
    if (value == 15) return -99;
    return (s8)value - 7;
}

static u8 event_matches(const Enemy *enemy, u8 link)
{
    return link == 0 || link == 99 || enemy->link == link;
}

static u8 active_enemy_count(void)
{
    u8 index;
    u8 count = 0;
    for (index = 0; index < MAX_ENEMIES; index++) {
        if (enemies[index].active) count++;
    }
    return count;
}

static void spawn_enemy(
    s16 x,
    s16 y,
    u8 type,
    u8 pool,
    s8 dx,
    s8 dy,
    s8 fixed_dy,
    u8 hp,
    u8 link,
    u8 reward,
    u16 kill_value,
    u8 turret1,
    u8 turret2,
    u8 turret3,
    u8 frequency1,
    u8 frequency2,
    u8 frequency3
)
{
    u8 index;
    u8 slot;
    u8 replace = 0;
    u8 found_free = 0;
    s16 largest_y = -32767;
    Enemy *enemy;

    if (type >= ENEMY_ARCHETYPES) type = 0;
    for (index = 0; index < MAX_ENEMIES; index++) {
        if (!enemies[index].active) {
            replace = index;
            found_free = 1;
            break;
        }
        if (enemies[index].y > largest_y) {
            largest_y = enemies[index].y;
            replace = index;
        }
    }
    if (!found_free) telemetry_enemy_replacements++;

    enemy = &enemies[replace];
    enemy->active = 1;
    enemy->x = x;
    enemy->y = y;
    enemy->type = type;
    enemy->pool = pool;
    enemy->hp = hp;
    enemy->phase = (u8)(x + level_tick);
    enemy->dx = dx;
    enemy->dy = dy;
    enemy->fixed_dy = fixed_dy;
    enemy->accel_x = 0;
    enemy->accel_y = 0;
    enemy->link = link;
    enemy->reward = reward;
    enemy->kill_value = kill_value;
    enemy->turret[0] = turret1;
    enemy->turret[1] = turret2;
    enemy->turret[2] = turret3;
    enemy->frequency[0] = frequency1;
    enemy->frequency[1] = frequency2;
    enemy->frequency[2] = frequency3;
    for (slot = 0; slot < 3; slot++) {
        /*
         * JE_makeEnemy starts every populated turret at 20 ticks even when
         * its initial frequency is zero; event 31 can enable it later.
         */
        enemy->fire_wait[slot] = enemy->turret[slot] ? 20 : 255;
        enemy->multi_pos[slot] = 0;
    }
    telemetry_spawn_count++;

    index = active_enemy_count();
    if (index > telemetry_max_enemies) telemetry_max_enemies = index;
}

static void apply_control(
    u8 opcode,
    u8 link,
    u8 value,
    u8 value2,
    u8 value3
)
{
    u8 index;
    u8 slot;
    s8 x = decode_component(value >> 4);
    s8 y = decode_component(value & 15);
    Enemy *enemy;

    for (index = 0; index < MAX_ENEMIES; index++) {
        enemy = &enemies[index];
        if (!enemy->active || !event_matches(enemy, link)) continue;
        if (opcode == EVENT_FIRE) {
            enemy->frequency[0] = value;
            enemy->frequency[1] = value2;
            enemy->frequency[2] = value3;
            for (slot = 0; slot < 3; slot++) enemy->fire_wait[slot] = 1;
        } else if (opcode == EVENT_MOVE) {
            if (x != -99) enemy->dx = x;
            if (y != -99) enemy->dy = y;
        } else if (opcode == EVENT_ACCEL) {
            if (x != -99) enemy->accel_x = x;
            if (y != -99) enemy->accel_y = y;
        } else {
            if (x != -99 && x != 0) {
                enemy->dx = -enemy->dx;
                enemy->accel_x = -enemy->accel_x;
            }
            if (y != -99 && y != 0) {
                enemy->dy = -enemy->dy;
                enemy->accel_y = -enemy->accel_y;
            }
        }
    }
    telemetry_control_count++;
}

static void apply_reward_control(u8 link, u8 reward)
{
    u8 index;
    Enemy *enemy;

    for (index = 0; index < MAX_ENEMIES; index++) {
        enemy = &enemies[index];
        if (!enemy->active || enemy->link != link) continue;
        enemy->reward = reward;
        if (reward) telemetry_reward_assignments++;
    }
    telemetry_reward_controls++;
    telemetry_control_count++;
}

static void set_background_motion(
    u8 speed1,
    u8 speed2,
    u8 speed3,
    u8 delay1,
    u8 delay2
)
{
    bg1_scroll_speed = speed1;
    bg2_scroll_speed = speed2;
    bg3_scroll_speed = speed3;
    bg1_scroll_delay_max = delay1 ? delay1 : 1;
    bg2_scroll_delay_max = delay2 ? delay2 : 1;
    bg1_scroll_delay = bg1_scroll_delay_max;
    bg2_scroll_delay = bg2_scroll_delay_max;
}
