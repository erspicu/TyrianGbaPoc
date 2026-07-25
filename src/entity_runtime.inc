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

#ifdef AUTOTEST_REWARD_VISUAL_TEST
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
#endif
