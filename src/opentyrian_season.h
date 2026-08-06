/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * GBA replacement for the host-clock Christmas selector.  The stock source
 * has a complete Christmas resource branch but no Pumpkin/Halloween mode.
 * Halloween Ramble remains an ordinary stock song selected by the original
 * level scripts, demos and Jukebox.
 */
#ifndef TYRIAN_GBA_OPENTYRIAN_SEASON_H
#define TYRIAN_GBA_OPENTYRIAN_SEASON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OT_SEASON_NONE = 0,
    OT_SEASON_XMAS = 1,
} OtSeasonMode;

OtSeasonMode ot_season_mode(void);
bool ot_season_set(OtSeasonMode mode);
/* Exact ASCII save-name match; XMAS is intentionally case-insensitive. */
OtSeasonMode ot_season_from_save_name(const char *name);

#endif
