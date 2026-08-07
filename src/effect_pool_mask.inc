/*
 * Dense active-slot directories for the two explosion/effect pools.
 *
 * The original active byte remains authoritative state.  These masks are a
 * derived directory which preserves the source allocator and traversal order:
 * allocation chooses the lowest free slot, update walks low to high, and
 * presentation walks high to low.  A build-time switch keeps an exact legacy
 * traversal available for same-ROM/A-B regression work.
 */

#if !TYRIAN_GBA_POOL_BIT_SCAN_ASM || \
    defined(AUTOTEST_FULL_LOADOUT_STRESS)
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone, noipa))
u32 source_pool_lowest_set_bit_c_reference(u32 bits)
{
    u32 index = 0;

    if (bits == 0) return 32u;
    while ((bits & 1u) == 0) {
        bits >>= 1;
        index++;
    }
    return index;
}

static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone, noipa))
u32 source_pool_highest_set_bit_c_reference(u32 bits)
{
    u32 index = 0;

    if (bits == 0) return 32u;
    if (bits >= 0x10000u) {
        bits >>= 16;
        index += 16u;
    }
    if (bits >= 0x100u) {
        bits >>= 8;
        index += 8u;
    }
    if (bits >= 0x10u) {
        bits >>= 4;
        index += 4u;
    }
    if (bits >= 0x4u) {
        bits >>= 2;
        index += 2u;
    }
    if (bits >= 0x2u) index++;
    return index;
}
#endif

#if TYRIAN_GBA_POOL_BIT_SCAN_ASM || \
    defined(AUTOTEST_FULL_LOADOUT_STRESS)
extern IWRAM_CODE ARM_CODE u32 source_pool_lowest_set_bit_asm(u32 bits);
extern IWRAM_CODE ARM_CODE u32 source_pool_highest_set_bit_asm(u32 bits);
#endif

static inline u32 source_pool_lowest_set_bit(u32 bits)
{
#if TYRIAN_GBA_POOL_BIT_SCAN_ASM
    return source_pool_lowest_set_bit_asm(bits);
#else
    return source_pool_lowest_set_bit_c_reference(bits);
#endif
}

static inline void source_pool_mask_set(u32 *mask, u16 index)
{
    mask[index >> 5] |= 1u << (index & 31u);
}

static inline void source_pool_mask_clear(u32 *mask, u16 index)
{
    mask[index >> 5] &= ~(1u << (index & 31u));
}

static inline u16 source_pool_find_first_clear(
    const u32 *mask,
    u16 count
)
{
    u16 base = 0;

    while (base < count) {
        u32 free_bits = ~mask[base >> 5];
        u32 bit;

        if ((u16)(count - base) < 32u) {
            free_bits &= (1u << (count - base)) - 1u;
        }
        if (free_bits != 0) {
            bit = source_pool_lowest_set_bit(free_bits);
            return (u16)(base + bit);
        }
        base = (u16)(base + 32u);
    }
    return count;
}

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static u32 source_pool_bit_scan_differential_test(void)
{
    u32 value;
    u32 state = 0x6d2b79f5u;
    u32 index;

    if (
        source_pool_lowest_set_bit_c_reference(0) != 32u ||
        source_pool_lowest_set_bit_asm(0) != 32u ||
        source_pool_highest_set_bit_c_reference(0) != 32u ||
        source_pool_highest_set_bit_asm(0) != 32u
    ) {
        return 0;
    }
    for (index = 0; index < 32u; index++) {
        value = 1u << index;
        if (
            source_pool_lowest_set_bit_c_reference(value) != index ||
            source_pool_lowest_set_bit_asm(value) != index ||
            source_pool_highest_set_bit_c_reference(value) != index ||
            source_pool_highest_set_bit_asm(value) != index
        ) {
            return 0;
        }
    }
    for (index = 0; index < 65536u; index++) {
        state = state * 1664525u + 1013904223u;
        value = state ^ (state >> 11) ^ (index * 0x9e3779b9u);
        if (
            source_pool_lowest_set_bit_c_reference(value) !=
                source_pool_lowest_set_bit_asm(value) ||
            source_pool_highest_set_bit_c_reference(value) !=
                source_pool_highest_set_bit_asm(value)
        ) {
            return 0;
        }
    }
    return 3u;
}

static u32 source_pool_mask_consistency_test(void)
{
#if TYRIAN_GBA_EFFECT_ACTIVE_MASK
    u16 index;
    u32 result = 0;

    for (index = 0; index < MAX_EFFECTS; index++) {
        u8 active = (u8)(
            (effect_active_mask[index >> 5] >> (index & 31u)) & 1u
        );

        if (active != (effects[index].active != 0)) return 0;
    }
    result |= 1u;
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        u8 active = (u8)(
            (pickup_explosion_active_mask[index >> 5] >>
                (index & 31u)) & 1u
        );

        if (active != (pickup_explosions[index].active != 0)) return result;
    }
    return result | 2u;
#else
    return 0;
#endif
}
#endif
