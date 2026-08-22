/*
 * Deterministic host-side bridge for Tyrian's vendored OPL2 emulator.
 *
 * This file deliberately contains only the small ABI and the LDS patch
 * register/effect adapter.  The synthesizer itself remains the unmodified
 * OpenTyrian/DOSBox implementation in vendor/opentyrian/src/opl.c.
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "opl.h"

#if defined(_WIN32)
#define TYOPL_EXPORT __declspec(dllexport)
#else
#define TYOPL_EXPORT __attribute__((visibility("default")))
#endif

#define TYOPL_ABI_VERSION 2u
#define TYOPL_INSTRUMENT_BYTES 46u
#define TYOPL_LDS_TICK_NUMERATOR 139u
#define TYOPL_LDS_TICK_DENOMINATOR 2u

static const uint8_t k_vibrato[64] = {
    0, 13, 25, 37, 50, 62, 74, 86, 98, 109, 120, 131, 142, 152,
    162, 171, 180, 189, 197, 205, 212, 219, 225, 231, 236, 240,
    244, 247, 250, 252, 254, 255, 255, 255, 254, 252, 250, 247,
    244, 240, 236, 231, 225, 219, 212, 205, 197, 189, 180, 171,
    162, 152, 142, 131, 120, 109, 98, 86, 74, 62, 50, 37, 25, 13
};

static const uint8_t k_tremolo[128] = {
    0, 0, 1, 1, 2, 4, 5, 7, 10, 12, 15, 18, 21, 25, 29, 33,
    37, 42, 47, 52, 57, 62, 67, 73, 79, 85, 90, 97, 103, 109,
    115, 121, 128, 134, 140, 146, 152, 158, 165, 170, 176, 182,
    188, 193, 198, 203, 208, 213, 218, 222, 226, 230, 234, 237,
    240, 243, 245, 248, 250, 251, 253, 254, 254, 255, 255, 255,
    254, 254, 253, 251, 250, 248, 245, 243, 240, 237, 234, 230,
    226, 222, 218, 213, 208, 203, 198, 193, 188, 182, 176, 170,
    165, 158, 152, 146, 140, 134, 127, 121, 115, 109, 103, 97,
    90, 85, 79, 73, 67, 62, 57, 52, 47, 42, 37, 33, 29, 25,
    21, 18, 15, 12, 10, 7, 5, 4, 2, 1, 1, 0
};

typedef struct TyOplEffects {
    int32_t note_q8;
    uint32_t vibrato_count;
    uint32_t vibrato_wait;
    uint32_t arpeggio_position;
    uint32_t arpeggio_count;
    uint32_t mod_tremolo_count;
    uint32_t car_tremolo_count;
    uint32_t mod_tremolo_wait;
    uint32_t car_tremolo_wait;
} TyOplEffects;

static void write_note(int32_t note_q8, int key_on)
{
    /* OPL2 f = fnum * 2^block * 49716 / 2^20.  Search the eight legal
     * blocks instead of depending on the LDS player's finite lookup table;
     * this also retains TYM Q8.8 tuning at adaptive sample roots. */
    const double note = (double)note_q8 / 256.0;
    const double frequency = 440.0 * pow(2.0, (note - 69.0) / 12.0);
    uint32_t best_fnum = 0;
    uint32_t best_block = 0;
    double best_error = 1.0e100;
    uint32_t block;

    for (block = 0; block < 8; ++block) {
        const double scale = (double)(1u << block) * 49716.0 / 1048576.0;
        int32_t fnum = (int32_t)(frequency / scale + 0.5);
        double realized;
        double error;
        if (fnum < 0) fnum = 0;
        if (fnum > 1023) fnum = 1023;
        realized = (double)fnum * scale;
        error = realized > frequency ? realized - frequency : frequency - realized;
        if (error < best_error) {
            best_error = error;
            best_fnum = (uint32_t)fnum;
            best_block = block;
        }
    }
    adlib_write(0xa0, (uint8_t)(best_fnum & 0xff));
    adlib_write(
        0xb0,
        (uint8_t)(((best_fnum >> 8) & 3u) | (best_block << 2) |
                  (key_on ? 0x20u : 0u)));
}

static void write_patch(const uint8_t *instrument, int32_t note_q8)
{
    adlib_init(49716u);
    srand(1u);
    adlib_write(0x01, 0x20);
    adlib_write(0x08, 0x00);
    adlib_write(0xbd, 0x00);

    adlib_write(0x20, instrument[0]);
    adlib_write(0x40, (uint8_t)(instrument[1] ^ 0x3f));
    adlib_write(0x60, instrument[2]);
    adlib_write(0x80, instrument[3]);
    adlib_write(0xe0, instrument[4]);

    adlib_write(0x23, instrument[5]);
    adlib_write(0x43, (uint8_t)(instrument[6] ^ 0x3f));
    adlib_write(0x63, instrument[7]);
    adlib_write(0x83, instrument[8]);
    adlib_write(0xe3, instrument[9]);
    adlib_write(0xc0, instrument[10]);
    write_note(note_q8, 1);
}

static void update_software_effects(
    const uint8_t *instrument,
    TyOplEffects *effects)
{
    const uint8_t vibrato = instrument[15];
    const uint8_t mod_tremolo = instrument[17];
    const uint8_t car_tremolo = instrument[18];
    const uint8_t arpeggio_size = instrument[20] & 15u;
    const uint8_t arpeggio_speed = instrument[20] >> 4;
    int32_t vibrato_delta_q8 = 0;
    int32_t arpeggio_delta_q8 = 0;

    if (effects->vibrato_wait != 0) {
        --effects->vibrato_wait;
    } else if (vibrato != 0) {
        const uint32_t amount =
            (uint32_t)k_vibrato[effects->vibrato_count & 63u] *
            ((uint32_t)(vibrato & 15u) + 1u);
        int32_t delta_q4 = (int32_t)(amount >> 8);
        if ((effects->vibrato_count & 64u) != 0) delta_q4 = -delta_q4;
        vibrato_delta_q8 = delta_q4 * 16;
        effects->vibrato_count += (uint32_t)(vibrato >> 4) + 2u;
    }

    if (arpeggio_size != 0) {
        const int32_t first = (int32_t)(int8_t)instrument[21];
        const int32_t current = (int32_t)(int8_t)instrument[
            21u + effects->arpeggio_position
        ];
        arpeggio_delta_q8 = (current - first) * 256;
        if (effects->arpeggio_count == arpeggio_speed) {
            ++effects->arpeggio_position;
            if (effects->arpeggio_position >= arpeggio_size) {
                effects->arpeggio_position = 0;
            }
            effects->arpeggio_count = 0;
        } else {
            ++effects->arpeggio_count;
        }
    }
    if (vibrato != 0 || arpeggio_size != 0) {
        write_note(
            effects->note_q8 + vibrato_delta_q8 + arpeggio_delta_q8,
            1);
    }

    if (effects->mod_tremolo_wait != 0) {
        --effects->mod_tremolo_wait;
    } else if ((mod_tremolo & 15u) != 0) {
        const uint32_t amount =
            (uint32_t)k_tremolo[effects->mod_tremolo_count & 127u] *
            (uint32_t)(mod_tremolo & 15u);
        const uint8_t base = instrument[1] & 0x3f;
        const uint8_t attenuation = (uint8_t)(amount >> 8);
        const uint8_t level = attenuation <= base ? base - attenuation : 0;
        adlib_write(
            0x40,
            (uint8_t)(((instrument[1] & 0xc0) | level) ^ 0x3f));
        effects->mod_tremolo_count += (uint32_t)(mod_tremolo >> 4);
    }

    if (effects->car_tremolo_wait != 0) {
        --effects->car_tremolo_wait;
    } else if ((car_tremolo & 15u) != 0) {
        const uint32_t amount =
            (uint32_t)k_tremolo[effects->car_tremolo_count & 127u] *
            (uint32_t)(car_tremolo & 15u);
        const uint8_t base = instrument[6] & 0x3f;
        const uint8_t attenuation = (uint8_t)(amount >> 8);
        const uint8_t level = attenuation <= base ? base - attenuation : 0;
        adlib_write(
            0x43,
            (uint8_t)(((instrument[6] & 0xc0) | level) ^ 0x3f));
        effects->car_tremolo_count += (uint32_t)(car_tremolo >> 4);
    }
}

TYOPL_EXPORT uint32_t tyrian_opl_abi_version(void)
{
    return TYOPL_ABI_VERSION;
}

TYOPL_EXPORT int32_t tyrian_opl_render_patch(
    const uint8_t *instrument,
    uint32_t instrument_bytes,
    int32_t note_q8,
    uint32_t sustain_samples,
    uint32_t release_samples,
    int16_t *output,
    uint32_t output_samples)
{
    uint32_t rendered = 0;
    uint32_t until_tick = 0;
    uint32_t tick_remainder = 0;
    TyOplEffects effects;

    if (instrument == NULL || output == NULL ||
        instrument_bytes != TYOPL_INSTRUMENT_BYTES ||
        sustain_samples + release_samples > output_samples ||
        sustain_samples == 0) {
        return -1;
    }

    effects.note_q8 = note_q8;
    effects.vibrato_count = 0;
    effects.vibrato_wait = instrument[16];
    effects.arpeggio_position = 0;
    effects.arpeggio_count = 0;
    effects.mod_tremolo_count = 0;
    effects.car_tremolo_count = 0;
    effects.mod_tremolo_wait = (instrument[19] & 0xf0u) >> 3;
    effects.car_tremolo_wait = (instrument[19] & 0x0fu) << 1;
    write_patch(instrument, note_q8);

    while (rendered < sustain_samples) {
        uint32_t count;
        if (until_tick == 0) {
            update_software_effects(instrument, &effects);
            until_tick =
                (49716u * TYOPL_LDS_TICK_DENOMINATOR) /
                TYOPL_LDS_TICK_NUMERATOR;
            tick_remainder +=
                (49716u * TYOPL_LDS_TICK_DENOMINATOR) %
                TYOPL_LDS_TICK_NUMERATOR;
            if (tick_remainder >= TYOPL_LDS_TICK_NUMERATOR) {
                ++until_tick;
                tick_remainder -= TYOPL_LDS_TICK_NUMERATOR;
            }
        }
        count = sustain_samples - rendered;
        if (count > until_tick) count = until_tick;
        adlib_getsample(output + rendered, (intptr_t)count);
        rendered += count;
        until_tick -= count;
    }

    if (release_samples != 0) {
        write_note(effects.note_q8, 0);
        adlib_getsample(output + rendered, (intptr_t)release_samples);
        rendered += release_samples;
    }
    return (int32_t)rendered;
}
