/*
 * Read-only Sprite2 exact-key lookup helpers and their differential oracle.
 * Included after SourceEnemyCacheSlot and the physical cache are declared.
 */

extern const SourceEnemyCacheSlot *source_enemy_cache_find_exact_asm(
    const SourceEnemyCacheSlot *cache,
    u32 slot_count,
    u32 packed_key,
    u8 filter
);

static inline u32 source_enemy_cache_packed_key(
    u8 shape_table,
    u16 graphic,
    u8 size
)
{
    return
        (u32)graphic |
        ((u32)shape_table << 16) |
        ((u32)size << 24);
}

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone, noipa))
const SourceEnemyCacheSlot *source_enemy_cache_find_exact_c_reference(
    const SourceEnemyCacheSlot *cache,
    u32 slot_count,
    u32 packed_key,
    u8 filter
)
{
    u32 index;

    for (index = 0; index < slot_count; index++) {
        const SourceEnemyCacheSlot *slot = &cache[index];

        if (
            slot->valid &&
            slot->graphic == (u16)packed_key &&
            slot->shape_table == (u8)(packed_key >> 16) &&
            slot->size == (u8)(packed_key >> 24) &&
            slot->filter == filter
        ) {
            return slot;
        }
    }
    return NULL;
}
#endif

#if TYRIAN_GBA_SPRITE2_EXACT_LOOKUP_ASM
static inline const SourceEnemyCacheSlot *source_enemy_cache_find_exact(
    u8 shape_table,
    u16 graphic,
    u8 size,
    u8 filter
)
{
    u32 packed_key = source_enemy_cache_packed_key(
        shape_table,
        graphic,
        size
    );

    return source_enemy_cache_find_exact_asm(
        source_enemy_cache,
        SOURCE_ENEMY_CACHE_SLOT_COUNT,
        packed_key,
        filter
    );
}
#endif

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static SourceEnemyCacheSlot source_enemy_cache_lookup_test_slots[4]
    EWRAM_BSS;
static volatile u32 source_enemy_cache_lookup_benchmark_keys[4]
    EWRAM_BSS;
static volatile u8 source_enemy_cache_lookup_benchmark_filters[4]
    EWRAM_BSS;

static u8 source_enemy_cache_find_exact_differential_test(void)
{
    u16 test;

    memset(
        source_enemy_cache_lookup_test_slots,
        0,
        sizeof(source_enemy_cache_lookup_test_slots)
    );
    for (test = 0; test < 2048u; test++) {
        u8 slot_index = (u8)(test & 3u);
        SourceEnemyCacheSlot *slot =
            &source_enemy_cache_lookup_test_slots[slot_index];
        u16 graphic = (u16)(test * 109u + 17u);
        u8 shape_table = (u8)(test * 13u + 1u);
        u8 size = (u8)((test >> 3) & 1u);
        u8 filter = (u8)(test * 29u + 7u);
        u32 packed_key = source_enemy_cache_packed_key(
            shape_table,
            graphic,
            size
        );
        const SourceEnemyCacheSlot *c_result;
        const SourceEnemyCacheSlot *asm_result;

        memset(
            source_enemy_cache_lookup_test_slots,
            0,
            sizeof(source_enemy_cache_lookup_test_slots)
        );
        slot->graphic = graphic;
        slot->shape_table = shape_table;
        slot->size = size;
        slot->filter = filter;
        slot->valid = 1;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != slot) return 0;

        slot->valid = 0;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != NULL) return 0;

        slot->valid = 1;
        slot->graphic ^= 0x8001u;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != NULL) return 0;

        slot->graphic = graphic;
        slot->shape_table ^= 0x81u;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != NULL) return 0;

        slot->shape_table = shape_table;
        slot->size ^= 1u;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != NULL) return 0;

        slot->size = size;
        slot->filter ^= 0x41u;
        c_result = source_enemy_cache_find_exact_c_reference(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        asm_result = source_enemy_cache_find_exact_asm(
            source_enemy_cache_lookup_test_slots,
            4,
            packed_key,
            filter
        );
        if (c_result != asm_result || c_result != NULL) return 0;
    }
    return 1;
}
#endif
