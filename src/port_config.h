/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Small, explicit GBA adaptation switches.  Detail values retain the
 * OpenTyrian processor-profile ordering used by JE_initProcessorType():
 * Low/386, Normal/486, High Detail and Pentium.
 *
 * Override examples:
 *   make DETAIL_LEVEL=normal GAME_SPEED=normal
 *   make DETAIL_LEVEL=high   GAME_SPEED=normal
 *   make DETAIL_LEVEL=pentium GAME_SPEED=normal
 *   make DETAIL_LEVEL=low    GAME_SPEED=low
 */
#ifndef TYRIAN_GBA_PORT_CONFIG_H
#define TYRIAN_GBA_PORT_CONFIG_H

#define TYRIAN_GBA_DETAIL_LOW 0
#define TYRIAN_GBA_DETAIL_NORMAL 1
#define TYRIAN_GBA_DETAIL_HIGH 2
#define TYRIAN_GBA_DETAIL_PENTIUM 3

#ifndef TYRIAN_GBA_DETAIL_LEVEL
#define TYRIAN_GBA_DETAIL_LEVEL TYRIAN_GBA_DETAIL_LOW
#endif

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_NORMAL && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_HIGH && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_PENTIUM
#error TYRIAN_GBA_DETAIL_LEVEL must be LOW, NORMAL, HIGH or PENTIUM
#endif

#define TYRIAN_GBA_GAME_SPEED_LOW 0
#define TYRIAN_GBA_GAME_SPEED_NORMAL 1

/*
 * 34.78259095 Hz / the GBA's 59.72750057 Hz display rate, expressed
 * with the original 1,193,182 Hz PC PIT numerator.  This timing is a
 * platform rule, not generated metadata from any particular level.
 */
#define ORIGINAL_LOGIC_NUMERATOR 1193182ul
#define ORIGINAL_LOGIC_DENOMINATOR 2048892ul

#ifndef TYRIAN_GBA_GAME_SPEED
#define TYRIAN_GBA_GAME_SPEED TYRIAN_GBA_GAME_SPEED_NORMAL
#endif

#if TYRIAN_GBA_GAME_SPEED != TYRIAN_GBA_GAME_SPEED_LOW && \
    TYRIAN_GBA_GAME_SPEED != TYRIAN_GBA_GAME_SPEED_NORMAL
#error TYRIAN_GBA_GAME_SPEED must be TYRIAN_GBA_GAME_SPEED_LOW or NORMAL
#endif

#ifndef TYRIAN_GBA_STRESS_PSG_AUDIO
#define TYRIAN_GBA_STRESS_PSG_AUDIO 1
#endif

/*
 * OpenTyrian Normal is PIT speed 0x4300 with frameCountMax=2.  Its Low/Slow
 * choice uses the same PIT speed and alternates frameCountMax between 2 and
 * 3, so its average logic rate is exactly four fifths of Normal.
 */
#if TYRIAN_GBA_GAME_SPEED == TYRIAN_GBA_GAME_SPEED_LOW
#define TYRIAN_GBA_LOGIC_NUMERATOR (ORIGINAL_LOGIC_NUMERATOR * 4ul)
#define TYRIAN_GBA_LOGIC_DENOMINATOR (ORIGINAL_LOGIC_DENOMINATOR * 5ul)
#else
#define TYRIAN_GBA_LOGIC_NUMERATOR ORIGINAL_LOGIC_NUMERATOR
#define TYRIAN_GBA_LOGIC_DENOMINATOR ORIGINAL_LOGIC_DENOMINATOR
#endif

#endif
