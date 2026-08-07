/*
 * Shared C/ARM layout contract for the hand-written collision kernel.
 *
 * This file intentionally contains preprocessor constants only so it can be
 * included by both GNU C and assembler-with-cpp.  C-side _Static_asserts in
 * level_port_collisions.inc make any future structure-layout drift fail the
 * build instead of silently corrupting gameplay state.
 */
#define OT_ASM_ENEMY_SIZE                         134
#define OT_ASM_ENEMY_EX_OFFSET                      2
#define OT_ASM_ENEMY_EY_OFFSET                      4
#define OT_ASM_ENEMY_ARMORLEFT_OFFSET              14
#define OT_ASM_ENEMY_CYCLE_OFFSET                  21
#define OT_ASM_ENEMY_LINKNUM_OFFSET                65
#define OT_ASM_ENEMY_GROUND_OFFSET                112
#define OT_ASM_ENEMY_MAPOFFSET_OFFSET             114

#define OT_ASM_STATE_BOSS_BAR_LINK_OFFSET          28
#define OT_ASM_STATE_BOSS_BAR_COLOR_OFFSET         30
#define OT_ASM_STATE_ENEMY_OFFSET                 150
#define OT_ASM_STATE_ENEMY_AVAIL_OFFSET         13550
#define OT_ASM_STATE_COLLISION_MASK_OFFSET      13652
#define OT_ASM_STATE_COLLISION_MASK_ACTIVE_OFFSET 13668
#define OT_ASM_STATE_COLLISION_COUNT_OFFSET     19756
#define OT_ASM_STATE_CANDIDATE_VISITS_OFFSET    19764

#define OT_ASM_RESULT_SIZE                        140
#define OT_ASM_RESULT_COLLIDED_OFFSET                0
#define OT_ASM_RESULT_CONSUMED_OFFSET                1
#define OT_ASM_RESULT_REMAINING_OFFSET               2
#define OT_ASM_RESULT_HIT_COUNT_OFFSET               3
#define OT_ASM_RESULT_KILL_COUNT_OFFSET              4
#define OT_ASM_RESULT_EFFECT_COUNT_OFFSET            5
#define OT_ASM_RESULT_SUPERPIXEL_COUNT_OFFSET        6
#define OT_ASM_RESULT_CUBES_OFFSET                   7
#define OT_ASM_RESULT_CASH_OFFSET                    8
#define OT_ASM_RESULT_EFFECTS_OFFSET                12

/* ot_player_shot_collision_asm_config bits. */
#define OT_ASM_CONFIG_ACTIVE_MASK                   1
#define OT_ASM_CONFIG_MASK_FAST_PATH                2
#define OT_ASM_CONFIG_LAZY_RESULT                   4
#define OT_ASM_CONFIG_RECORD_VISITS                 8
#define OT_ASM_CONFIG_UNSIGNED_RANGE               16
#define OT_ASM_CONFIG_VISIT_MODE \
    (OT_ASM_CONFIG_ACTIVE_MASK | OT_ASM_CONFIG_RECORD_VISITS)

/* SourceEnemyCacheSlot layout used by the read-only exact-hit scan leaf. */
#define SOURCE_ENEMY_ASM_CACHE_SLOT_SIZE             16
#define SOURCE_ENEMY_ASM_GRAPHIC_OFFSET               8
#define SOURCE_ENEMY_ASM_SHAPE_TABLE_OFFSET           10
#define SOURCE_ENEMY_ASM_SIZE_OFFSET                  11
#define SOURCE_ENEMY_ASM_FILTER_OFFSET                12
#define SOURCE_ENEMY_ASM_VALID_OFFSET                 13

/* SourceProjectileCacheSlot layout for the validated direct-map hint leaf. */
#define SOURCE_PROJECTILE_ASM_CACHE_SLOT_SIZE          16
#define SOURCE_PROJECTILE_ASM_GRAPHIC_OFFSET            8
#define SOURCE_PROJECTILE_ASM_SHAPE_TABLE_OFFSET       10
#define SOURCE_PROJECTILE_ASM_VALID_OFFSET             11
