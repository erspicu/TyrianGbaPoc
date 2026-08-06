/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "opentyrian_season.h"

static OtSeasonMode active_season;

static uint8_t ascii_upper(uint8_t value)
{
    if (value >= 'a' && value <= 'z') {
        return (uint8_t)(value - ('a' - 'A'));
    }
    return value;
}

static bool save_name_equals(const char *name, const char *expected)
{
    uint8_t index = 0;

    if (name == 0 || expected == 0) return false;
    while (expected[index] != '\0') {
        if (
            name[index] == '\0' ||
            ascii_upper((uint8_t)name[index]) !=
                ascii_upper((uint8_t)expected[index])
        ) {
            return false;
        }
        index++;
    }
    /* SRAM editors sometimes space-pad fixed fields; accept only padding. */
    while (name[index] == ' ') index++;
    return name[index] == '\0';
}

OtSeasonMode ot_season_mode(void)
{
    return active_season;
}

bool ot_season_set(OtSeasonMode mode)
{
    if (mode != OT_SEASON_XMAS) {
        mode = OT_SEASON_NONE;
    }
    if (active_season == mode) return false;
    active_season = mode;
    return true;
}

OtSeasonMode ot_season_from_save_name(const char *name)
{
    if (save_name_equals(name, "XMAS")) return OT_SEASON_XMAS;
    return OT_SEASON_NONE;
}
