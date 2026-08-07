#define SOURCE_PROJECTILE_HINT_COUNT 64u
#define SOURCE_PROJECTILE_HINT_MASK (SOURCE_PROJECTILE_HINT_COUNT - 1u)

_Static_assert(
    (SOURCE_PROJECTILE_HINT_COUNT & SOURCE_PROJECTILE_HINT_MASK) == 0,
    "projectile hint directory must have a power-of-two size"
);
_Static_assert(
    sizeof(SourceProjectileCacheSlot) ==
        SOURCE_PROJECTILE_ASM_CACHE_SLOT_SIZE,
    "projectile cache slot layout changed"
);
_Static_assert(
    offsetof(SourceProjectileCacheSlot, graphic) ==
        SOURCE_PROJECTILE_ASM_GRAPHIC_OFFSET,
    "projectile graphic offset changed"
);
_Static_assert(
    offsetof(SourceProjectileCacheSlot, shape_table) ==
        SOURCE_PROJECTILE_ASM_SHAPE_TABLE_OFFSET,
    "projectile shape-table offset changed"
);
_Static_assert(
    offsetof(SourceProjectileCacheSlot, valid) ==
        SOURCE_PROJECTILE_ASM_VALID_OFFSET,
    "projectile valid offset changed"
);

static u8 source_projectile_cache_hint[
    SOURCE_PROJECTILE_HINT_COUNT
] EWRAM_BSS;

static inline u32 source_projectile_cache_packed_key(
    u8 shape_table,
    u16 graphic
)
{
    return (u32)graphic | ((u32)shape_table << 16);
}

static inline u8 source_projectile_cache_hint_hash(u32 packed_key)
{
    return (u8)(
        (
            packed_key ^
            (packed_key >> 6) ^
            (packed_key >> 12)
        ) & SOURCE_PROJECTILE_HINT_MASK
    );
}

#if !TYRIAN_GBA_PROJECTILE_HINT_ASM || \
    defined(AUTOTEST_FULL_LOADOUT_STRESS)
static IWRAM_CODE ARM_CODE __attribute__((noinline, noclone, noipa))
const SourceProjectileCacheSlot *
source_projectile_cache_find_hint_c_reference(
    const u8 *hint,
    const SourceProjectileCacheSlot *cache,
    u32 packed_key,
    u32 slot_count
)
{
    u8 encoded = hint[source_projectile_cache_hint_hash(packed_key)];
    u8 index;
    const SourceProjectileCacheSlot *slot;

    if (encoded == 0) return 0;
    index = (u8)(encoded - 1u);
    if (index >= slot_count) return 0;
    slot = &cache[index];
    if (
        !slot->valid ||
        slot->graphic != (u16)packed_key ||
        slot->shape_table != (u8)(packed_key >> 16)
    ) {
        return 0;
    }
    return slot;
}
#endif

#if TYRIAN_GBA_PROJECTILE_HINT_ASM || \
    defined(AUTOTEST_FULL_LOADOUT_STRESS)
extern IWRAM_CODE ARM_CODE const SourceProjectileCacheSlot *
source_projectile_cache_find_hint_asm(
    const u8 *hint,
    const SourceProjectileCacheSlot *cache,
    u32 packed_key,
    u32 slot_count
);
#endif

static inline const SourceProjectileCacheSlot *
source_projectile_cache_find_hint(u32 packed_key)
{
#if TYRIAN_GBA_PROJECTILE_HINT_ASM
    return source_projectile_cache_find_hint_asm(
        source_projectile_cache_hint,
        source_projectile_cache,
        packed_key,
        SOURCE_PROJECTILE_CACHE_SLOT_COUNT
    );
#else
    return source_projectile_cache_find_hint_c_reference(
        source_projectile_cache_hint,
        source_projectile_cache,
        packed_key,
        SOURCE_PROJECTILE_CACHE_SLOT_COUNT
    );
#endif
}

static inline void source_projectile_cache_hint_store(
    u8 shape_table,
    u16 graphic,
    u8 slot
)
{
    u32 packed_key = source_projectile_cache_packed_key(
        shape_table,
        graphic
    );

    source_projectile_cache_hint[
        source_projectile_cache_hint_hash(packed_key)
    ] = (u8)(slot + 1u);
}

static inline void source_projectile_cache_hint_clear_slot(u8 slot)
{
    const SourceProjectileCacheSlot *entry =
        &source_projectile_cache[slot];
    u32 packed_key;
    u8 bucket;

    if (!entry->valid) return;
    packed_key = source_projectile_cache_packed_key(
        entry->shape_table,
        entry->graphic
    );
    bucket = source_projectile_cache_hint_hash(packed_key);
    if (source_projectile_cache_hint[bucket] == (u8)(slot + 1u)) {
        source_projectile_cache_hint[bucket] = 0;
    }
}

#ifdef AUTOTEST_FULL_LOADOUT_STRESS
static SourceProjectileCacheSlot
    source_projectile_hint_test_slots[4] EWRAM_BSS;
static u8 source_projectile_hint_test_directory[
    SOURCE_PROJECTILE_HINT_COUNT
] EWRAM_BSS;
static u32 source_projectile_hint_benchmark_keys[4] EWRAM_BSS;

static u32 source_projectile_cache_hint_differential_test(void)
{
    u32 iteration;

    for (iteration = 0; iteration < 4096u; iteration++) {
        u8 slot_index = (u8)(iteration & 3u);
        SourceProjectileCacheSlot *slot =
            &source_projectile_hint_test_slots[slot_index];
        u16 graphic = (u16)((iteration * 73u) % 500u + 1u);
        u8 shape_table = (u8)(4u + ((iteration >> 2) & 1u));
        u32 packed_key = source_projectile_cache_packed_key(
            shape_table,
            graphic
        );
        u8 bucket = source_projectile_cache_hint_hash(packed_key);
        const SourceProjectileCacheSlot *c_result;
        const SourceProjectileCacheSlot *asm_result;

        memset(
            source_projectile_hint_test_slots,
            0,
            sizeof(source_projectile_hint_test_slots)
        );
        memset(
            source_projectile_hint_test_directory,
            0,
            sizeof(source_projectile_hint_test_directory)
        );
        slot->graphic = graphic;
        slot->shape_table = shape_table;
        slot->valid = 1;
        source_projectile_hint_test_directory[bucket] =
            (u8)(slot_index + 1u);

#define PROJECTILE_HINT_COMPARE(expected) \
        do { \
            c_result = source_projectile_cache_find_hint_c_reference( \
                source_projectile_hint_test_directory, \
                source_projectile_hint_test_slots, \
                packed_key, \
                4u \
            ); \
            asm_result = source_projectile_cache_find_hint_asm( \
                source_projectile_hint_test_directory, \
                source_projectile_hint_test_slots, \
                packed_key, \
                4u \
            ); \
            if (c_result != asm_result || c_result != (expected)) { \
                return 0; \
            } \
        } while (0)

        PROJECTILE_HINT_COMPARE(slot);
        slot->valid = 0;
        PROJECTILE_HINT_COMPARE(0);
        slot->valid = 1;
        slot->graphic ^= 1u;
        PROJECTILE_HINT_COMPARE(0);
        slot->graphic = graphic;
        slot->shape_table ^= 1u;
        PROJECTILE_HINT_COMPARE(0);
        slot->shape_table = shape_table;
        source_projectile_hint_test_directory[bucket] = 0xffu;
        PROJECTILE_HINT_COMPARE(0);
#undef PROJECTILE_HINT_COMPARE
    }
    return 1u;
}
#endif
