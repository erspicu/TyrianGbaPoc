/*
 * OpenTyrian software-render draw order translated to GBA BG/OBJ priority.
 *
 * PC draws into one framebuffer, so later stages cover earlier stages.  GBA
 * has only four numeric priorities; OBJ wins a tie against BG.  The two
 * transparent PC background layers therefore occupy priorities 2 and 1 in
 * their current back-to-front order.  OBJ below both uses priority 3, OBJ
 * between them uses priority 2, and OBJ above both uses priority 0.
 *
 * Equal-priority OBJ ordering is completed in gba_scene.inc by emitting the
 * later PC draw stage at the lower OAM index.
 */

enum {
    SOURCE_DRAW_STAGE_BACKGROUND1 = 0,
    SOURCE_DRAW_STAGE_BACKGROUND2_BACK = 10,
    SOURCE_DRAW_STAGE_GROUND_ENEMY = 20,
    SOURCE_DRAW_STAGE_BACKGROUND2_MIDDLE = 30,
    SOURCE_DRAW_STAGE_BACKGROUND3_BACK = 40,
    SOURCE_DRAW_STAGE_SKY_ENEMY = 50,
    SOURCE_DRAW_STAGE_BACKGROUND3_MIDDLE = 60,
    SOURCE_DRAW_STAGE_TOP_ENEMY = 70,
    SOURCE_DRAW_STAGE_PLAYER_SHOT = 80,
    SOURCE_DRAW_STAGE_PLAYER = 90,
    SOURCE_DRAW_STAGE_ENEMY_SHOT = 100,
    SOURCE_DRAW_STAGE_BACKGROUND3_FRONT = 110,
    SOURCE_DRAW_STAGE_TOP_ENEMY_OVER = 120,
    SOURCE_DRAW_STAGE_SKY_ENEMY_OVER = 130,
    SOURCE_DRAW_STAGE_EFFECT = 140,
    SOURCE_DRAW_STAGE_BACKGROUND2_FRONT = 150,
    SOURCE_DRAW_STAGE_HUD = 160,
};

static u8 source_background2_draw_stage_for(u8 background2_over)
{
    switch (background2_over) {
    case 1:
        return SOURCE_DRAW_STAGE_BACKGROUND2_MIDDLE;
    case 2:
        return SOURCE_DRAW_STAGE_BACKGROUND2_FRONT;
    case 0:
    case 3:
    default:
        return SOURCE_DRAW_STAGE_BACKGROUND2_BACK;
    }
}

static u8 source_background3_draw_stage_for(u8 background3_over)
{
    switch (background3_over) {
    case 1:
        return SOURCE_DRAW_STAGE_BACKGROUND3_FRONT;
    case 2:
        return SOURCE_DRAW_STAGE_BACKGROUND3_BACK;
    case 0:
    default:
        return SOURCE_DRAW_STAGE_BACKGROUND3_MIDDLE;
    }
}

static u8 source_background_priority_for(
    u8 own_stage,
    u8 other_stage
)
{
    /* The later software-render stage is the front transparent BG. */
    return own_stage > other_stage ? 1 : 2;
}

static u8 source_object_priority_for_stages(
    u8 object_stage,
    u8 background2_stage,
    u8 background3_stage
)
{
    u8 back_stage =
        background2_stage < background3_stage ?
        background2_stage : background3_stage;
    u8 front_stage =
        background2_stage > background3_stage ?
        background2_stage : background3_stage;

    if (object_stage > front_stage) return 0;
    if (object_stage > back_stage) return 2;
    return 3;
}

static u8 source_background2_priority(void)
{
    u8 background2_stage = source_background2_draw_stage_for(
        source_parity_level.background2_over
    );
    u8 background3_stage = source_background3_draw_stage_for(
        source_parity_level.background3_over
    );

    return source_background_priority_for(
        background2_stage,
        background3_stage
    );
}

static u8 source_background3_priority(void)
{
    u8 background2_stage = source_background2_draw_stage_for(
        source_parity_level.background2_over
    );
    u8 background3_stage = source_background3_draw_stage_for(
        source_parity_level.background3_over
    );

    return source_background_priority_for(
        background3_stage,
        background2_stage
    );
}

static u8 source_object_priority(u8 object_stage)
{
    return source_object_priority_for_stages(
        object_stage,
        source_background2_draw_stage_for(
            source_parity_level.background2_over
        ),
        source_background3_draw_stage_for(
            source_parity_level.background3_over
        )
    );
}

static bool source_object_is_above_background(
    u8 object_priority,
    u8 background_priority
)
{
    /* On GBA an OBJ is in front of a BG when their priorities are equal. */
    return object_priority <= background_priority;
}

static bool source_layer_priority_self_test(
    u32 *passed_checks,
    u32 *failed_checks
)
{
    static const u8 object_stages[] = {
        SOURCE_DRAW_STAGE_GROUND_ENEMY,
        SOURCE_DRAW_STAGE_SKY_ENEMY,
        SOURCE_DRAW_STAGE_TOP_ENEMY,
        SOURCE_DRAW_STAGE_PLAYER_SHOT,
        SOURCE_DRAW_STAGE_PLAYER,
        SOURCE_DRAW_STAGE_ENEMY_SHOT,
        SOURCE_DRAW_STAGE_TOP_ENEMY_OVER,
        SOURCE_DRAW_STAGE_SKY_ENEMY_OVER,
        SOURCE_DRAW_STAGE_EFFECT,
        SOURCE_DRAW_STAGE_HUD,
    };
    u8 background2_over;

    *passed_checks = 0;
    *failed_checks = 0;
    for (background2_over = 0; background2_over <= 3; background2_over++) {
        u8 background3_over;

        for (
            background3_over = 0;
            background3_over <= 2;
            background3_over++
        ) {
            u8 background2_stage =
                source_background2_draw_stage_for(background2_over);
            u8 background3_stage =
                source_background3_draw_stage_for(background3_over);
            u8 background2_priority = source_background_priority_for(
                background2_stage,
                background3_stage
            );
            u8 background3_priority = source_background_priority_for(
                background3_stage,
                background2_stage
            );
            u8 index;
            bool background_order_valid =
                background2_priority >= 1 &&
                background2_priority <= 2 &&
                background3_priority >= 1 &&
                background3_priority <= 2 &&
                background2_priority != background3_priority &&
                (
                    (background2_stage > background3_stage) ==
                    (background2_priority < background3_priority)
                );

            if (background_order_valid) {
                (*passed_checks)++;
            } else {
                (*failed_checks)++;
            }
            for (
                index = 0;
                index < sizeof(object_stages) / sizeof(object_stages[0]);
                index++
            ) {
                u8 object_stage = object_stages[index];
                u8 object_priority =
                    source_object_priority_for_stages(
                        object_stage,
                        background2_stage,
                        background3_stage
                    );
                bool background2_relation_valid =
                    source_object_is_above_background(
                        object_priority,
                        background2_priority
                    ) ==
                    (object_stage > background2_stage);
                bool background3_relation_valid =
                    source_object_is_above_background(
                        object_priority,
                        background3_priority
                    ) ==
                    (object_stage > background3_stage);

                if (background2_relation_valid) {
                    (*passed_checks)++;
                } else {
                    (*failed_checks)++;
                }
                if (background3_relation_valid) {
                    (*passed_checks)++;
                } else {
                    (*failed_checks)++;
                }
            }
        }
    }
    return *failed_checks == 0;
}
