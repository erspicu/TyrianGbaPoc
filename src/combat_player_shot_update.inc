/*
 * Existing-player-shot movement and animation, translated from tyrian2.c.
 * Kept separate from weapon creation so combat_runtime.inc remains readable.
 */

#if TYRIAN_GBA_PLAYER_SHOT_UPDATE_MASK
static const u8 player_shot_update_debruijn_index[32] = {
    0, 1, 28, 2, 29, 14, 24, 3,
    30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7,
    26, 12, 18, 6, 11, 5, 10, 9,
};

static inline u8 player_shot_update_lowest_set_bit(u32 bits)
{
    u32 isolated = bits & (0u - bits);

    return player_shot_update_debruijn_index[
        (isolated * 0x077cb531u) >> 27
    ];
}
#endif

static inline __attribute__((always_inline)) void
update_player_shot_slot(u8 index)
{
    PlayerShot *player_shot = &player_shots[index];

    if (player_shot->ttl == 0) {
        player_shot_set_active(index, 0);
        return;
    }
    player_shot->ttl--;
    player_shot->xm = (s16)(player_shot->xm + player_shot->xc);
    player_shot->x = (s16)(player_shot->x + player_shot->xm);
    if (player_shot->xm > 100) {
        if (player_shot->xm == 101) {
            player_shot->x = (s16)(
                player_shot->x - 101 + SOURCE_PLAYER_SHOT_DELTA_X
            );
            player_shot->y = (s16)(
                player_shot->y + SOURCE_PLAYER_SHOT_DELTA_Y
            );
        } else {
            player_shot->x = (s16)(
                player_shot->x - 120 + SOURCE_PLAYER_SHOT_DELTA_X
            );
        }
    }
    player_shot->ym = (s16)(player_shot->ym + player_shot->yc);
    player_shot->y = (s16)(player_shot->y + player_shot->ym);
    if (player_shot->ym > 100) {
        player_shot->y = (s16)(
            player_shot->y - 120 + SOURCE_PLAYER_SHOT_DELTA_Y
        );
    }
    if (player_shot->complicated) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_update_complicated_visits++;
#endif
        player_shot->dev_x = (s8)(
            player_shot->dev_x + player_shot->dir_x
        );
        player_shot->x = (s16)(
            player_shot->x + player_shot->dev_x
        );
        if (
            (player_shot->dev_x < 0 ?
                -player_shot->dev_x :
                player_shot->dev_x) == player_shot->circle_size_x
        ) {
            player_shot->dir_x = (s8)-player_shot->dir_x;
        }
        player_shot->dev_y = (s8)(
            player_shot->dev_y + player_shot->dir_y
        );
        player_shot->y = (s16)(
            player_shot->y + player_shot->dev_y
        );
        if (
            (player_shot->dev_y < 0 ?
                -player_shot->dev_y :
                player_shot->dev_y) == player_shot->circle_size_y
        ) {
            player_shot->dir_y = (s8)-player_shot->dir_y;
        }
    }
    if (
        player_shot->x < -34 || player_shot->x > 290 ||
        player_shot->y < -15 || player_shot->y > 190
    ) {
        player_shot_set_active(index, 0);
        return;
    }
    if (player_shot->trail != 255) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_update_trail_visits++;
#endif
        source_spawn_explosion(
            player_shot->trail == 98 ?
                (s16)(player_shot->x - player_shot->xm) :
                player_shot->x,
            player_shot->trail == 98 ?
                (s16)(player_shot->y - player_shot->ym) :
                player_shot->y,
            0,
            player_shot->trail,
            0
        );
    }
    if (player_shot->aim_enemy != 0) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_update_aimed_visits++;
#endif
        if (player_shot->aim_delay > 0) player_shot->aim_delay--;
        if (player_shot->aim_delay == 0) {
            u8 enemy_index = (u8)(player_shot->aim_enemy - 1u);

            player_shot->aim_delay = player_shot->aim_delay_max;
            if (
                enemy_index < OT_ENEMY_COUNT &&
                source_parity_level.enemy_avail[enemy_index] != 1
            ) {
                const OtEnemy *enemy =
                    &source_parity_level.enemy[enemy_index];
                s16 target_x = enemy->ex;

                if (player_shot->x < target_x) {
                    player_shot->xm++;
                } else {
                    player_shot->xm--;
                }
                if (player_shot->y < enemy->ey) {
                    player_shot->ym++;
                } else {
                    player_shot->ym--;
                }
            } else if (player_shot->xm > 0) {
                player_shot->xm++;
            } else {
                player_shot->xm--;
            }
        }
    }
    player_shot->render_graphic = (u16)(
        player_shot->graphic + player_shot->animation
    );
    if (
        player_shot->render_graphic > 1000u &&
        player_shot->render_graphic <= 60000u
    ) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
        telemetry_player_shot_update_superpixel_visits++;
#endif
        source_superpixel_spawn(
            (s16)(player_shot->x + 7),
            (s16)(player_shot->y + 6),
            5,
            3,
            (u8)((player_shot->render_graphic / 1000u) << 4)
        );
    }
    player_shot->animation++;
    if (player_shot->animation >= player_shot->animation_max) {
        player_shot->animation = 0;
    }
}

static void update_shots(void)
{
    u8 index;

#if TYRIAN_GBA_PLAYER_SHOT_UPDATE_MASK
    /*
     * Bit iteration wins for sparse normal loadouts, but a nearly full
     * stress pool is cheaper as a sequential 81-byte scan.  Both paths keep
     * the source's ascending slot order and execute the identical body.
     */
    if (active_player_shot_count <= 24u) {
        u8 word;

        for (word = 0; word < PLAYER_SHOT_FREE_MASK_WORDS; word++) {
            u32 active_bits = player_shot_active_mask[word];

            while (active_bits != 0) {
                index = (u8)(
                    ((u16)word << 5) +
                    player_shot_update_lowest_set_bit(active_bits)
                );
                active_bits &= active_bits - 1u;
                if (index >= MAX_PLAYER_SHOTS) break;
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
                telemetry_player_shot_update_active_visits++;
#endif
                update_player_shot_slot(index);
            }
        }
    } else
#endif
    {
        for (index = 0; index < MAX_PLAYER_SHOTS; index++) {
#ifdef AUTOTEST_FULL_LOADOUT_STRESS
            telemetry_player_shot_update_linear_visits++;
#endif
            if (!player_shots[index].active) continue;
            update_player_shot_slot(index);
        }
    }
#if TYRIAN_GBA_STRESS_LOADOUT
    stress_update_shot_peak();
#endif
}
