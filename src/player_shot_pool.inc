/* OpenTyrian MAX_PWEAPON free-slot directory; gameplay state stays in active. */

static inline u8 player_shot_find_free_slot(void)
{
    u8 index;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
    telemetry_player_shot_allocator_calls++;
#endif
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
    for (index = 0; index < PLAYER_SHOT_FREE_MASK_WORDS; index++) {
        u32 free_bits = ~player_shot_active_mask[index];
        u16 base = (u16)index << 5;

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_allocator_mask_word_probes++;
#endif
        if ((u16)(MAX_PLAYER_SHOTS - base) < 32u) {
            free_bits &= (1u << (MAX_PLAYER_SHOTS - base)) - 1u;
        }
        if (free_bits != 0) {
            return (u8)(base + source_pool_lowest_set_bit(free_bits));
        }
    }
    return MAX_PLAYER_SHOTS;
#else
    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_allocator_slot_probes++;
#endif
        if (!player_shots[index].active) return index;
    }
    return MAX_PLAYER_SHOTS;
#endif
}

static inline void player_shot_set_active(u8 index, u8 active)
{
    PlayerShot *shot;
    u8 was_active;

    if (index >= MAX_PLAYER_SHOTS) return;
    shot = &player_shots[index];
    was_active = shot->active != 0;
    active = active != 0;
    shot->active = active;
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
    if (was_active == active) return;
    if (active) {
        source_pool_mask_set(player_shot_active_mask, index);
        active_player_shot_count++;
    } else {
        source_pool_mask_clear(player_shot_active_mask, index);
        if (active_player_shot_count != 0) active_player_shot_count--;
    }
#else
    (void)was_active;
#endif
}

static inline void player_shot_set_active_ptr(PlayerShot *shot, u8 active)
{
    if (shot == 0) return;
    player_shot_set_active((u8)(shot - player_shots), active);
}

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static u32 player_shot_pool_consistency_test(void)
{
#if TYRIAN_GBA_PLAYER_SHOT_FREE_MASK
    u16 index;
    u16 count = 0;

    for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
        u8 mask_active = (u8)(
            (player_shot_active_mask[index >> 5] >> (index & 31u)) & 1u
        );
        u8 slot_active = player_shots[index].active != 0;

        if (mask_active != slot_active) return 0;
        count = (u16)(count + slot_active);
    }
    return count == active_player_shot_count ? 1u : 0u;
#else
    return 0;
#endif
}
#endif
