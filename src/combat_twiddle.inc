/*
 * OpenTyrian's single-player Street-Fighter/Twiddle command decoder.
 *
 * The GBA D-pad replaces the PC movement direction and A replaces the
 * primary fire button.  The authored token stream, per-ship command table,
 * cancellation rules and terminal Special IDs remain byte-for-byte the
 * values in mainint.c/varz.c.  Equipped Special firing stays on L and is a
 * separate path with deliberately different payment semantics.
 */

enum {
    SOURCE_TWIDDLE_COMBO_COUNT = 26,
    SOURCE_TWIDDLE_COMBO_BYTES = 8,
    SOURCE_TWIDDLE_SHIP_COUNT = 14,
    SOURCE_TWIDDLE_SHIP_SLOTS = 3,
};

static const u8 source_twiddle_keyboard_combos[
    SOURCE_TWIDDLE_COMBO_COUNT
][SOURCE_TWIDDLE_COMBO_BYTES] = {
    {2, 1, 2, 5, 137, 0, 0, 0},
    {4, 3, 2, 5, 138, 0, 0, 0},
    {3, 4, 6, 139, 0, 0, 0, 0},
    {2, 5, 142, 0, 0, 0, 0, 0},
    {6, 2, 6, 143, 0, 0, 0, 0},
    {6, 7, 5, 8, 6, 7, 5, 112},
    {7, 8, 101, 0, 0, 0, 0, 0},
    {1, 7, 6, 146, 0, 0, 0, 0},
    {8, 6, 7, 1, 120, 0, 0, 0},
    {3, 6, 8, 5, 121, 0, 0, 0},
    {1, 2, 7, 8, 119, 0, 0, 0},
    {3, 4, 3, 6, 123, 0, 0, 0},
    {6, 7, 5, 8, 124, 0, 0, 0},
    {1, 6, 125, 0, 0, 0, 0, 0},
    {9, 5, 126, 0, 0, 0, 0, 0},
    {1, 7, 127, 0, 0, 0, 0, 0},
    {1, 8, 128, 0, 0, 0, 0, 0},
    {9, 7, 129, 0, 0, 0, 0, 0},
    {9, 8, 130, 0, 0, 0, 0, 0},
    {4, 2, 3, 5, 131, 0, 0, 0},
    {3, 1, 2, 8, 132, 0, 0, 0},
    {2, 4, 5, 133, 0, 0, 0, 0},
    {3, 4, 2, 8, 134, 0, 0, 0},
    {1, 4, 6, 135, 0, 0, 0, 0},
    {1, 3, 6, 137, 0, 0, 0, 0},
    {1, 4, 3, 4, 7, 136, 0, 0},
};

static const u8 source_twiddle_ship_combos[
    SOURCE_TWIDDLE_SHIP_COUNT
][SOURCE_TWIDDLE_SHIP_SLOTS] = {
    {5, 4, 7},
    {1, 2, 0},
    {14, 4, 0},
    {4, 5, 0},
    {6, 5, 0},
    {7, 8, 0},
    {7, 9, 0},
    {10, 3, 5},
    {5, 8, 9},
    {1, 3, 0},
    {7, 16, 17},
    {2, 11, 12},
    {3, 8, 10},
    {0, 0, 0},
};

static u8 source_twiddle_current_code[
    SOURCE_TWIDDLE_SHIP_SLOTS
] EWRAM_BSS;
/* varz.c SFExecuted[0]: retained until JE_doSpecialShot can consume it. */
static u8 source_twiddle_executed EWRAM_BSS;

_Static_assert(
    sizeof(source_twiddle_keyboard_combos) ==
        SOURCE_TWIDDLE_COMBO_COUNT * SOURCE_TWIDDLE_COMBO_BYTES,
    "OpenTyrian twiddle command table changed"
);
_Static_assert(
    sizeof(source_twiddle_ship_combos) ==
        SOURCE_TWIDDLE_SHIP_COUNT * SOURCE_TWIDDLE_SHIP_SLOTS,
    "OpenTyrian ship twiddle table changed"
);

static void source_twiddle_reset(void)
{
    memset(
        source_twiddle_current_code,
        0,
        sizeof(source_twiddle_current_code)
    );
    source_twiddle_executed = 0;
}

/*
 * Return the pending Special ID, or zero.  If two authored slots complete
 * together, the later slot wins exactly as SFExecuted[0] was overwritten
 * by JE_SFCodes().  The value deliberately survives later ticks until the
 * Special path is ready to consume it.
 */
static u8 source_twiddle_update(void)
{
    u8 ship = source_parity_level.player_ship;
    u8 direction_count;
    u8 token;
    u8 slot;
    u8 fire = (pad_now & KEY_A) != 0;

    if (ship >= SOURCE_TWIDDLE_SHIP_COUNT) return 0;

    direction_count = (u8)(
        ((pad_now & KEY_UP) != 0) +
        ((pad_now & KEY_DOWN) != 0) +
        ((pad_now & KEY_LEFT) != 0) +
        ((pad_now & KEY_RIGHT) != 0)
    );
    token = (u8)(
        ((pad_now & KEY_UP) != 0) * 1u +
        ((pad_now & KEY_DOWN) != 0) * 2u +
        ((pad_now & KEY_LEFT) != 0) * 3u +
        ((pad_now & KEY_RIGHT) != 0) * 4u
    );
    if (token == 0) {
        if (!fire) {
            token = 9;
            direction_count = 1;
        } else {
            token = 99;
            direction_count = 0;
        }
    }
    if (direction_count != 1) return 0;
    token = (u8)(token + fire * 4u);

    for (slot = 0; slot < SOURCE_TWIDDLE_SHIP_SLOTS; slot++) {
        u8 combo = source_twiddle_ship_combos[ship][slot];
        u8 progress = source_twiddle_current_code[slot];
        u8 expected;

        if (combo == 0) {
            source_twiddle_current_code[slot] = 0;
            continue;
        }
        if (progress >= SOURCE_TWIDDLE_COMBO_BYTES) {
            source_twiddle_current_code[slot] = 0;
            progress = 0;
        }
        expected = source_twiddle_keyboard_combos[combo - 1u][progress];
        if (expected == token) {
            u8 next;

            progress++;
            if (progress >= SOURCE_TWIDDLE_COMBO_BYTES) {
                source_twiddle_current_code[slot] = 0;
                continue;
            }
            source_twiddle_current_code[slot] = progress;
            next = source_twiddle_keyboard_combos[combo - 1u][progress];
            if (
                next > 100u &&
                next < 100u + OT_HDT_SPECIAL_COUNT
            ) {
                source_twiddle_current_code[slot] = 0;
                source_twiddle_executed = (u8)(next - 100u);
            }
        } else if (
            token != 9u &&
            expected != 0u &&
            ((u8)(expected - 1u) % 4u) !=
                ((u8)(token - 1u) % 4u) &&
            (
                progress == 0u ||
                source_twiddle_keyboard_combos[
                    combo - 1u
                ][progress - 1u] != token
            )
        ) {
            source_twiddle_current_code[slot] = 0;
        }
    }
    return source_twiddle_executed;
}

#ifdef AUTOTEST
static u8 source_twiddle_selftest(void)
{
    u8 saved_code[SOURCE_TWIDDLE_SHIP_SLOTS];
    u8 saved_executed = source_twiddle_executed;
    u8 saved_ship = source_parity_level.player_ship;
    u16 saved_pad_now = pad_now;
    u8 pass = 1;

    memcpy(saved_code, source_twiddle_current_code, sizeof(saved_code));

    /* SuperCarrot: UP, DOWN+FIRE -> Special 25 Hot Dog. */
    source_twiddle_reset();
    source_parity_level.player_ship = 2;
    pad_now = KEY_UP;
    pass = pass && source_twiddle_update() == 0;
    pass = pass && source_twiddle_current_code[0] == 1;
    pass = pass && source_twiddle_update() == 0;
    pass = pass && source_twiddle_current_code[0] == 1;
    pad_now = KEY_DOWN | KEY_A;
    pass = pass && source_twiddle_update() == 25;
    pass = pass && source_twiddle_current_code[0] == 0;
    pad_now = 0;
    pass = pass && source_twiddle_update() == 25;

    /* USP Talon combo 1: DOWN, UP, DOWN, UP+FIRE -> Invulnerability. */
    source_twiddle_reset();
    source_parity_level.player_ship = 1;
    pad_now = KEY_DOWN;
    pass = pass && source_twiddle_update() == 0;
    pad_now = KEY_UP;
    pass = pass && source_twiddle_update() == 0;
    pad_now = KEY_DOWN;
    pass = pass && source_twiddle_update() == 0;
    pad_now = KEY_UP | KEY_A;
    pass = pass && source_twiddle_update() == 37;

    /* JE_SFCodes ignores diagonals instead of consuming a token. */
    source_twiddle_reset();
    pad_now = KEY_UP | KEY_LEFT;
    pass = pass && source_twiddle_update() == 0;
    pass = pass && source_twiddle_current_code[0] == 0;

    memcpy(source_twiddle_current_code, saved_code, sizeof(saved_code));
    source_twiddle_executed = saved_executed;
    source_parity_level.player_ship = saved_ship;
    pad_now = saved_pad_now;
    return pass;
}
#endif
