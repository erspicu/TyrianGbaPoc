/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct translation staging area for OpenTyrian's selected-level game loop.
 * Field assignments follow JE_main() and JE_eventSystem() in tyrian2.c at
 * revision 1c34d1bddac8c8f2de834229d04b5a729525c944.
 *
 * The ordered fragments below remain one translation unit, preserving all
 * static linkage and source-parity execution order while making each concern
 * small enough to review independently.
 */
#include "opentyrian_level_port.h"
#include "res/boss_manifest.h"

#include "level_port/level_port_spawn.inc"
#include "level_port/level_port_events.inc"
#include "level_port/level_port_enemy_motion.inc"
#include "level_port/level_port_collisions.inc"
#include "level_port/level_port_advance.inc"
