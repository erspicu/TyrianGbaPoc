/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Direct translation staging area for OpenTyrian's first-level game loop.
 * The field assignments below follow JE_main() and JE_eventSystem() in
 * src/tyrian2.c at revision
 * 1c34d1bddac8c8f2de834229d04b5a729525c944.
 */
#include "opentyrian_level_port.h"

extern const uint8_t opentyrian_level1_events[];
extern const uint8_t opentyrian_level1_enemies[];

enum {
    OT_SOURCE_HEADER_BYTES = 8,
    OT_EVENT_RECORD_BYTES = 11,
    OT_ENEMY_RAW_RECORD_BYTES = 77,
    OT_ENEMY_RECORD_BYTES = 79,
};

_Static_assert(sizeof(uint8_t) == 1, "OpenTyrian byte width changed");
_Static_assert(sizeof(int8_t) == 1, "OpenTyrian shortint width changed");
_Static_assert(sizeof(uint16_t) == 2, "OpenTyrian word width changed");
_Static_assert(sizeof(int16_t) == 2, "OpenTyrian integer width changed");
_Static_assert(
    OPENTYRIAN_LEVEL1_EVENT_RECORD_BYTES == OT_EVENT_RECORD_BYTES,
    "generated OpenTyrian event record width changed"
);
_Static_assert(
    OPENTYRIAN_LEVEL1_ENEMY_RECORD_BYTES == OT_ENEMY_RECORD_BYTES,
    "generated OpenTyrian enemy record width changed"
);

static uint16_t read_u16(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static int16_t read_s16(const uint8_t *source)
{
    return (int16_t)read_u16(source);
}

static bool header_matches(
    const uint8_t *source,
    char a,
    char b,
    char c,
    char d,
    uint16_t count,
    uint8_t record_bytes
)
{
    return source[0] == (uint8_t)a &&
           source[1] == (uint8_t)b &&
           source[2] == (uint8_t)c &&
           source[3] == (uint8_t)d &&
           read_u16(source + 4) == count &&
           source[6] == record_bytes;
}

bool ot_level1_event_read(uint16_t index, OtEventRecord *event)
{
    const uint8_t *source;

    if (event == 0 || index >= OPENTYRIAN_LEVEL1_EVENT_COUNT) {
        return false;
    }
    source = opentyrian_level1_events + OT_SOURCE_HEADER_BYTES +
             (uint32_t)index * OT_EVENT_RECORD_BYTES;
    event->eventtime = read_u16(source);
    event->eventtype = source[2];
    event->eventdat = read_s16(source + 3);
    event->eventdat2 = read_s16(source + 5);
    event->eventdat3 = (int8_t)source[7];
    event->eventdat5 = (int8_t)source[8];
    event->eventdat6 = (int8_t)source[9];
    event->eventdat4 = source[10];
    return true;
}

bool ot_level1_enemy_read(uint16_t enemy_id, OtEnemyDefinition *enemy)
{
    uint16_t low = 0;
    uint16_t high = OPENTYRIAN_LEVEL1_ENEMY_COUNT;

    if (enemy == 0) return false;
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2);
        const uint8_t *record =
            opentyrian_level1_enemies + OT_SOURCE_HEADER_BYTES +
            (uint32_t)middle * OT_ENEMY_RECORD_BYTES;
        uint16_t record_id = read_u16(record);
        const uint8_t *source;
        uint8_t index;

        if (record_id < enemy_id) {
            low = (uint16_t)(middle + 1);
            continue;
        }
        if (record_id > enemy_id) {
            high = middle;
            continue;
        }

        source = record + 2;
        enemy->ani = source[0];
        for (index = 0; index < 3; index++) {
            enemy->tur[index] = source[1 + index];
            enemy->freq[index] = source[4 + index];
        }
        enemy->xmove = (int8_t)source[7];
        enemy->ymove = (int8_t)source[8];
        enemy->xaccel = (int8_t)source[9];
        enemy->yaccel = (int8_t)source[10];
        enemy->xcaccel = (int8_t)source[11];
        enemy->ycaccel = (int8_t)source[12];
        enemy->startx = read_s16(source + 13);
        enemy->starty = read_s16(source + 15);
        enemy->startxc = (int8_t)source[17];
        enemy->startyc = (int8_t)source[18];
        enemy->armor = source[19];
        enemy->esize = source[20];
        for (index = 0; index < 20; index++) {
            enemy->egraphic[index] = read_u16(source + 21 + index * 2);
        }
        enemy->explosiontype = source[61];
        enemy->animate = source[62];
        enemy->shapebank = source[63];
        enemy->xrev = (int8_t)source[64];
        enemy->yrev = (int8_t)source[65];
        enemy->dgr = read_u16(source + 66);
        enemy->dlevel = (int8_t)source[68];
        enemy->dani = (int8_t)source[69];
        enemy->elaunchfreq = source[70];
        enemy->elaunchtype = read_u16(source + 71);
        enemy->value = read_s16(source + 73);
        enemy->eenemydie = read_u16(source + 75);
        return true;
    }
    return false;
}

void ot_level_port_init(OtLevelPortState *state)
{
    OtEnemyDefinition first_spawn_definition;

    *state = (OtLevelPortState){0};

    /*
     * JE_main(), start_level_first initialization.  UI, SDL, demo, network
     * and save-game calls are outside this GBA proof's agreed scope.
     */
    state->map1_y_delay = 1;
    state->map1_y_delay_max = 1;
    state->map2_y_delay = 1;
    state->map2_y_delay_max = 1;
    state->back_move = 1;
    state->back_move2 = 2;
    state->back_move3 = 3;
    state->explode_move = 2;
    state->starfield_speed = 1;
    state->star_active = true;
    state->enemies_active = true;
    state->background2_over = 1;
    state->level_enemy_frequency = 96;
    state->current_song = -1;
    state->level_end = 255;

    state->assets_valid = header_matches(
        opentyrian_level1_events,
        'O', 'T', 'L', '1',
        OPENTYRIAN_LEVEL1_EVENT_COUNT,
        OT_EVENT_RECORD_BYTES
    ) && header_matches(
        opentyrian_level1_enemies,
        'O', 'T', 'E', '1',
        OPENTYRIAN_LEVEL1_ENEMY_COUNT,
        OT_ENEMY_RECORD_BYTES
    ) && opentyrian_level1_enemies[7] == OT_ENEMY_RAW_RECORD_BYTES &&
        ot_level1_enemy_read(10, &first_spawn_definition) &&
        first_spawn_definition.ani == 8 &&
        first_spawn_definition.armor == 3 &&
        first_spawn_definition.esize == 1 &&
        first_spawn_definition.shapebank == 1 &&
        first_spawn_definition.value == 15;
}

static bool apply_event(OtLevelPortState *state, const OtEventRecord *event)
{
    switch (event->eventtype) {
    case 1:
        state->starfield_speed = event->eventdat;
        return true;

    case 2:
        state->map1_y_delay = 1;
        state->map1_y_delay_max = 1;
        state->map2_y_delay = 1;
        state->map2_y_delay_max = 1;
        state->back_move = (uint16_t)event->eventdat;
        state->back_move2 = (uint16_t)event->eventdat2;
        state->explode_move = state->back_move2 > 0 ?
            state->back_move2 : state->back_move;
        state->back_move3 = (uint16_t)(int16_t)event->eventdat3;
        if (state->back_move > 0) state->stop_background_num = 0;
        return true;

    case 3:
        state->back_move = 1;
        state->map1_y_delay = 3;
        state->map1_y_delay_max = 3;
        state->back_move2 = 1;
        state->map2_y_delay = 2;
        state->map2_y_delay_max = 2;
        state->back_move3 = 1;
        return true;

    case 5:
        state->shape_bank[0] =
            event->eventdat > 0 ? (uint8_t)event->eventdat : 0;
        state->shape_bank[1] =
            event->eventdat2 > 0 ? (uint8_t)event->eventdat2 : 0;
        state->shape_bank[2] =
            event->eventdat3 > 0 ? (uint8_t)event->eventdat3 : 0;
        state->shape_bank[3] = event->eventdat4;
        return true;

    case 8:
        state->star_active = false;
        return true;

    case 9:
        state->star_active = true;
        return true;

    case 13:
        state->enemies_active = false;
        return true;

    case 14:
        state->enemies_active = true;
        return true;

    case 21:
        state->background3_over = 1;
        return true;

    case 22:
        state->background3_over = 0;
        return true;

    case 28:
        state->top_enemy_over = false;
        return true;

    case 29:
        state->top_enemy_over = true;
        return true;

    case 30:
        state->map1_y_delay = 1;
        state->map1_y_delay_max = 1;
        state->map2_y_delay = 1;
        state->map2_y_delay_max = 1;
        state->back_move = (uint16_t)event->eventdat;
        state->back_move2 = (uint16_t)event->eventdat2;
        state->explode_move = state->back_move2;
        state->back_move3 = (uint16_t)(int16_t)event->eventdat3;
        return true;

    case 34:
        state->music_fade = true;
        return true;

    case 35:
        state->current_song = (int16_t)(event->eventdat - 1);
        state->music_fade = false;
        return true;

    case 36:
        state->ready_to_end_level = true;
        return true;

    case 37:
        state->level_enemy_frequency = (uint16_t)event->eventdat;
        return true;

    case 40:
        state->enemy_continual_damage = true;
        return true;

    case 42:
        state->background3_over = 2;
        return true;

    case 43:
        state->background2_over = (uint8_t)event->eventdat;
        return true;

    case 48:
        state->background2_not_transparent = true;
        return true;

    case 53:
        state->force_events = event->eventdat != 99;
        return true;

    case 57:
        state->super_enemy_254_jump = (uint16_t)event->eventdat;
        return true;

    case 65:
        state->background3_x1 = event->eventdat == 0;
        return true;

    case 68:
        state->random_explosions = event->eventdat == 1;
        return true;

    case 72:
        state->background3_x1b = event->eventdat == 1;
        return true;

    case 73:
        state->sky_enemy_over_all = event->eventdat == 1;
        return true;

    case 79:
        state->boss_bar_link[0] = (uint8_t)event->eventdat;
        state->boss_bar_link[1] = (uint8_t)event->eventdat2;
        return true;

    default:
        /*
         * Entity/player/collision/audio cases remain owned by the legacy
         * runtime in stage 1.  Their exact records are still consumed and
         * counted, but no approximate behavior is added here.
         */
        return false;
    }
}

void ot_level_port_advance(OtLevelPortState *state, uint16_t cur_loc)
{
    OtEventRecord event;

    state->cur_loc = cur_loc;
    while (
        state->event_index < OPENTYRIAN_LEVEL1_EVENT_COUNT &&
        ot_level1_event_read(state->event_index, &event) &&
        event.eventtime <= state->cur_loc
    ) {
        if (apply_event(state, &event)) {
            state->applied_event_count++;
        } else {
            state->deferred_event_count++;
        }
        state->event_index++;
    }
}
