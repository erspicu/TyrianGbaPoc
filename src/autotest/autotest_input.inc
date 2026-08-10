#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
#ifdef AUTOTEST_NO_FIRE
#define AUTOTEST_HELD_FIRE 0
#else
#define AUTOTEST_HELD_FIRE KEY_A
#endif

static u16 autotest_aim_keys(s16 target_x, s16 target_y)
{
    u16 keys = AUTOTEST_HELD_FIRE;

    if (target_x < SOURCE_PLAYER_MIN_X) target_x = SOURCE_PLAYER_MIN_X;
    if (target_x > SOURCE_PLAYER_MAX_X) target_x = SOURCE_PLAYER_MAX_X;
    if (target_y < SOURCE_PLAYER_CLAMP_MIN_Y) {
        target_y = SOURCE_PLAYER_CLAMP_MIN_Y;
    }
    if (target_y > SOURCE_PLAYER_CLAMP_MAX_Y) {
        target_y = SOURCE_PLAYER_CLAMP_MAX_Y;
    }
    if (player_source_x < target_x - 2) {
        keys |= KEY_RIGHT;
    } else if (player_source_x > target_x + 2) {
        keys |= KEY_LEFT;
    } else if (player_source_velocity_x > 0) {
        keys |= KEY_LEFT;
    } else if (player_source_velocity_x < 0) {
        keys |= KEY_RIGHT;
    }
    if (player_source_y < target_y - 2) {
        keys |= KEY_DOWN;
    } else if (player_source_y > target_y + 2) {
        keys |= KEY_UP;
    } else if (player_source_velocity_y > 0) {
        keys |= KEY_UP;
    } else if (player_source_velocity_y < 0) {
        keys |= KEY_DOWN;
    }
    return keys;
}
#endif

static u16 autotest_input(void)
{
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    u8 reward_index;
#endif
    u8 source_index;
    u16 phase = (level_tick >> 5) & 3;
    u16 keys =
#ifdef AUTOTEST_NO_FIRE
        0;
#else
        KEY_A;
#endif
    s16 player_screen_x = source_player_screen_x();
    s16 player_screen_y = source_player_screen_y();
#ifdef AUTOTEST_FORCE_PLAYER_TOP
    /*
     * Visual boundary regression mode: hold UP until the PC-space clamp
     * reaches the GBA crop-safe upper limit.
     */
    return KEY_UP;
#endif
#ifdef AUTOTEST_STATIONARY_PLAYER
    /*
     * Screenshot regression mode: hold a neutral pose so two frames from
     * different level ticks can be compared without position compensation.
     */
    return 0;
#endif
#ifndef AUTOTEST_ROUTE_MANUAL_INPUT
    if (source_parity_level.boss_bar_link[0] != 0) {
        for (
            source_index = 0;
            source_index < OT_ENEMY_COUNT;
            source_index++
        ) {
            const OtEnemy *enemy =
                &source_parity_level.enemy[source_index];

            if (
                source_parity_level.enemy_avail[source_index] == 1 ||
                enemy->linknum !=
                    source_parity_level.boss_bar_link[0]
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
                ||
                enemy->armorleft == 255 ||
                !source_parity_level.enemy_draw[source_index].active
#endif
            ) {
                continue;
            }
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
            return autotest_aim_keys(
                (s16)(enemy->ex + enemy->mapoffset),
                (s16)(enemy->ey + 80)
            );
#else
            /*
             * Preserve the original first-level golden's deterministic
             * keypad trace.  Generic route-smoke needs the source-space
             * target adapter above, but changing this historical trace
             * changes boss kill timing without changing release gameplay.
             */
            if (player_source_x < enemy->ex - 2) keys |= KEY_RIGHT;
            if (player_source_x > enemy->ex + 2) keys |= KEY_LEFT;
            if (player_source_y < 140) keys |= KEY_DOWN;
            return keys;
#endif
        }
    }
#endif
#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
    /*
     * Generic non-first-level smoke runs must clear authored event-4 stop
     * groups instead of relying on the first-level movement pattern.  This
     * is test input only; release gameplay has no attraction or aim assist.
     */
#ifndef AUTOTEST_ROUTE_MANUAL_INPUT
    if (source_parity_level.stop_backgrounds) {
        /*
         * Prefer a currently drawn member of the exact source stop pool.
         * Hidden PC special-weapon hitboxes are handled by the separately
         * metered route-smoke combat assist after a grace period.
         */
        for (
            source_index = 0;
            source_index < OT_ENEMY_COUNT;
            source_index++
        ) {
            const OtEnemy *enemy =
                &source_parity_level.enemy[source_index];
            s16 target_x;

            if (
                source_parity_level.enemy_avail[source_index] == 1 ||
                !source_autotest_enemy_matches_stop_pool(source_index) ||
                enemy->scoreitem ||
                enemy->armorleft == 255 ||
                !source_parity_level.enemy_draw[source_index].active
            ) {
                continue;
            }
            target_x = (s16)(enemy->ex + enemy->mapoffset);
            return autotest_aim_keys(
                target_x,
                (s16)(enemy->ey + 80)
            );
        }
    }
#endif
#endif
    /*
     * Event-33 data cubes are physical score-item enemies (evalue==1), not
     * destruction awards.  Prioritize them over cash objects so the exact
     * JE_playerCollide() cube branch remains covered with PC inertia.
     */
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 target_x;
        s16 target_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem ||
            enemy->evalue != 1
        ) {
            continue;
        }
        target_x = (s16)(enemy->ex + enemy->mapoffset);
        target_y = enemy->ey;
        if (player_source_x < target_x - 2) keys |= KEY_RIGHT;
        if (player_source_x > target_x + 2) keys |= KEY_LEFT;
        if (player_source_y < target_y - 2) keys |= KEY_DOWN;
        if (player_source_y > target_y + 2) keys |= KEY_UP;
        return keys;
    }
    /*
     * Follow the first authored score item long enough to exercise the
     * physical pickup path. The release build has no attraction or assist.
     */
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    for (reward_index = 0; reward_index < MAX_REWARDS; reward_index++) {
        if (!rewards[reward_index].active) continue;
        if (player_screen_x + 16 < rewards[reward_index].x + 8) {
            keys |= KEY_RIGHT;
        }
        if (player_screen_x + 16 > rewards[reward_index].x + 8) {
            keys |= KEY_LEFT;
        }
        if (player_screen_y + 14 < rewards[reward_index].y + 8) {
            keys |= KEY_DOWN;
        }
        if (player_screen_y + 14 > rewards[reward_index].y + 8) {
            keys |= KEY_UP;
        }
        return keys;
    }
#endif
    for (source_index = 0; source_index < OT_ENEMY_COUNT; source_index++) {
        const OtEnemy *enemy = &source_parity_level.enemy[source_index];
        s16 reward_x;
        s16 reward_y;

        if (
            source_parity_level.enemy_avail[source_index] == 1 ||
            !enemy->scoreitem
        ) {
            continue;
        }
        reward_x = source_score_item_screen_x(enemy);
        reward_y = source_score_item_screen_y(enemy);
        if (player_screen_x + 16 < reward_x + 8) keys |= KEY_RIGHT;
        if (player_screen_x + 16 > reward_x + 8) keys |= KEY_LEFT;
        if (player_screen_y + 14 < reward_y + 8) keys |= KEY_DOWN;
        if (player_screen_y + 14 > reward_y + 8) keys |= KEY_UP;
        return keys;
    }
    if (phase == 0) keys |= KEY_RIGHT;
    if (phase == 1) keys |= KEY_UP;
    if (phase == 2) keys |= KEY_LEFT;
    if (phase == 3) keys |= KEY_DOWN;
    return keys;
}

#ifdef AUTOTEST_FRONTEND_ROUTE_SECTION
#undef AUTOTEST_HELD_FIRE
#endif

#if defined(AUTOTEST_SCREENSHOT_EXPLOSION) || \
    defined(AUTOTEST_SCREENSHOT_EXPLOSION_FRAME)
static u8 autotest_explosion_visible(void)
{
    u8 index;
    for (index = 0; index < MAX_EFFECTS; index++) {
        if (!effects[index].active) continue;
#ifdef AUTOTEST_SCREENSHOT_EXPLOSION_FRAME
        if (effects[index].frame == AUTOTEST_SCREENSHOT_EXPLOSION_FRAME) {
            return 1;
        }
#else
        return 1;
#endif
    }
    return 0;
}
#endif
#ifdef AUTOTEST_SCREENSHOT_REWARD
static u8 autotest_reward_visible(void)
{
    u8 index;

    for (index = 0; index < OT_ENEMY_COUNT; index++) {
        if (
            source_parity_level.enemy_draw[index].active &&
            source_parity_level.enemy[index].scoreitem
        ) {
            return 1;
        }
    }
#ifdef AUTOTEST_REWARD_VISUAL_TEST
    for (index = 0; index < MAX_REWARDS; index++) {
        if (rewards[index].active) return 1;
    }
#endif
    for (index = 0; index < MAX_PICKUP_EXPLOSIONS; index++) {
        if (pickup_explosions[index].active) return 1;
    }
    return 0;
}
#endif
