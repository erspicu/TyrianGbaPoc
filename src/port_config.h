/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Small, explicit GBA adaptation switches.  Detail values retain the
 * OpenTyrian processor-profile ordering used by JE_initProcessorType():
 * Low/386, Normal/486, High Detail and Pentium.
 *
 * Override examples:
 *   make DETAIL_LEVEL=config GAME_SPEED=normal
 *   make DETAIL_LEVEL=normal GAME_SPEED=normal
 *   make DETAIL_LEVEL=high   GAME_SPEED=normal
 *   make DETAIL_LEVEL=pentium GAME_SPEED=normal
 *   make DETAIL_LEVEL=custom GAME_SPEED=normal
 *   make DETAIL_LEVEL=low    GAME_SPEED=low
 */
#ifndef TYRIAN_GBA_PORT_CONFIG_H
#define TYRIAN_GBA_PORT_CONFIG_H

#include "Configure.h"

/*
 * Keep a compile-time C reference path for reproducible before/after
 * measurements and emergency diagnosis.  Production builds default to the
 * bit-exact ARM/IWRAM hot paths.
 */
#ifndef TYRIAN_GBA_HOTPATH_ASM
#define TYRIAN_GBA_HOTPATH_ASM 1
#endif
#if TYRIAN_GBA_HOTPATH_ASM != 0 && TYRIAN_GBA_HOTPATH_ASM != 1
#error TYRIAN_GBA_HOTPATH_ASM must be 0 or 1
#endif

/*
 * Detail-effect kernels have their own switch so their C/ARM measurements do
 * not include the unrelated collision, RNG, scaling and Sprite2 hot paths.
 */
#ifndef TYRIAN_GBA_DETAIL_EFFECT_ASM
#define TYRIAN_GBA_DETAIL_EFFECT_ASM 1
#endif
#if TYRIAN_GBA_DETAIL_EFFECT_ASM != 0 && \
    TYRIAN_GBA_DETAIL_EFFECT_ASM != 1
#error TYRIAN_GBA_DETAIL_EFFECT_ASM must be 0 or 1
#endif

#define TYRIAN_GBA_DETAIL_LOW 0
#define TYRIAN_GBA_DETAIL_NORMAL 1
#define TYRIAN_GBA_DETAIL_HIGH 2
#define TYRIAN_GBA_DETAIL_PENTIUM 3
#define TYRIAN_GBA_DETAIL_CUSTOM 4

#ifndef TYRIAN_GBA_DETAIL_LEVEL
#define TYRIAN_GBA_DETAIL_LEVEL TYRIAN_GBA_CONFIG_DETAIL_LEVEL
#endif

#if TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_LOW && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_NORMAL && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_HIGH && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_PENTIUM && \
    TYRIAN_GBA_DETAIL_LEVEL != TYRIAN_GBA_DETAIL_CUSTOM
#error TYRIAN_GBA_DETAIL_LEVEL must be LOW, NORMAL, HIGH, PENTIUM or CUSTOM
#endif

/*
 * Do not infer individual effects from numeric ordering.  CUSTOM is based on
 * Normal and deliberately takes only Pentium's wild 50/50 BG2 Alpha and final
 * hue/brightness filtration.  In particular it must never enter the High /
 * Pentium lava-water hue or per-scanline wave paths.
 */
#define TYRIAN_GBA_DETAIL_HAS_LAVA_WATER ( \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_HIGH || \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM \
)
#define TYRIAN_GBA_DETAIL_HAS_SPOTLIGHT ( \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_NORMAL || \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_HIGH || \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM \
)
#define TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA ( \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM || \
    TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_CUSTOM \
)
#define TYRIAN_GBA_DETAIL_HAS_FINAL_FILTER \
    TYRIAN_GBA_DETAIL_HAS_WILD_ALPHA

#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_CUSTOM && \
    TYRIAN_GBA_DETAIL_HAS_SPOTLIGHT
#error CUSTOM detail must not compile the triangular spotlight path
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
 * Presentation-only deadline recovery.  Configure.h enables the measured
 * whole-scene scheduler for release builds; command-line diagnostics may
 * still override it for controlled A/B without changing OpenTyrian's logic
 * rate.
 */
#ifndef TYRIAN_GBA_DYNAMIC_FRAME_DROP
#define TYRIAN_GBA_DYNAMIC_FRAME_DROP 0
#endif
#if TYRIAN_GBA_DYNAMIC_FRAME_DROP != 0 && \
    TYRIAN_GBA_DYNAMIC_FRAME_DROP != 1
#error TYRIAN_GBA_DYNAMIC_FRAME_DROP must be 0 or 1
#endif
#ifndef TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
#define TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH 0
#endif
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH != 0 && \
    TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH != 1
#error TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH must be 0 or 1
#endif
#if TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH && \
    !TYRIAN_GBA_DYNAMIC_FRAME_DROP
#error Adaptive presentation dispatch requires dynamic presentation scheduling
#endif
#ifndef TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH
#define TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH 0
#endif
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH != 0 && \
    TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH != 1
#error TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH must be 0 or 1
#endif
#if TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH && \
    !TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH
#error Wave policy requires global adaptive presentation dispatch
#endif
#ifndef TYRIAN_GBA_WALL_CLOCK_LOGIC
#define TYRIAN_GBA_WALL_CLOCK_LOGIC TYRIAN_GBA_DYNAMIC_FRAME_DROP
#endif
#if TYRIAN_GBA_WALL_CLOCK_LOGIC != 0 && \
    TYRIAN_GBA_WALL_CLOCK_LOGIC != 1
#error TYRIAN_GBA_WALL_CLOCK_LOGIC must be 0 or 1
#endif
#if TYRIAN_GBA_WALL_CLOCK_LOGIC && \
    !TYRIAN_GBA_DYNAMIC_FRAME_DROP
#error TYRIAN_GBA_WALL_CLOCK_LOGIC requires dynamic presentation scheduling
#endif
/*
 * VBlankIntrWait() deliberately discards an already-latched VBlank.  After
 * an overrun that would make the main loop wait through one additional LCD
 * period, and Maxmod's required once-per-frame mmFrame() call would fall
 * progressively behind.  Dynamic builds instead consume every VBlank IRQ
 * counted by the handler: overdue periods run an input/logic recovery
 * iteration without attempting an unsafe active-display VRAM commit. Audio
 * mixing remains one transaction per newly observed physical IRQ and is
 * protected against a mid-mix mmVBlank cursor reset.
 */
#ifndef TYRIAN_GBA_RECOVER_MISSED_VBLANK
#define TYRIAN_GBA_RECOVER_MISSED_VBLANK TYRIAN_GBA_WALL_CLOCK_LOGIC
#endif
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK != 0 && \
    TYRIAN_GBA_RECOVER_MISSED_VBLANK != 1
#error TYRIAN_GBA_RECOVER_MISSED_VBLANK must be 0 or 1
#endif
#if TYRIAN_GBA_RECOVER_MISSED_VBLANK && \
    !TYRIAN_GBA_WALL_CLOCK_LOGIC
#error Missed-VBlank recovery requires wall-clock logic
#endif
#ifndef TYRIAN_GBA_PRESENTATION_DEFER
#define TYRIAN_GBA_PRESENTATION_DEFER TYRIAN_GBA_DYNAMIC_FRAME_DROP
#endif
#ifndef TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER
#define TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER 1
#endif
#if TYRIAN_GBA_PRESENTATION_DEFER != 0 && \
    TYRIAN_GBA_PRESENTATION_DEFER != 1
#error TYRIAN_GBA_PRESENTATION_DEFER must be 0 or 1
#endif
#if TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER != 0 && \
    TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER != 1
#error TYRIAN_GBA_FREEZE_BACKGROUND_ON_DEFER must be 0 or 1
#endif
#if TYRIAN_GBA_PRESENTATION_DEFER && \
    !TYRIAN_GBA_DYNAMIC_FRAME_DROP
#error TYRIAN_GBA_PRESENTATION_DEFER requires dynamic presentation scheduling
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
