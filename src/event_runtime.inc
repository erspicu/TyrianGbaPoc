/* Legacy compact event bytecode runner retained for regression comparison. */

static void process_events(void)
{
    u8 opcode;
    u8 length;
    while (
        event_offset + 1u < LEVEL_EVENT_BYTES &&
        event_time <= level_position
    ) {
        opcode = level_events[event_offset + 1];
        if (opcode == EVENT_END) return;
        length = 2;
        if (opcode < ENEMY_ARCHETYPES) {
            spawn_enemy(
                (s16)(
                    (u16)level_events[event_offset + 2] |
                    ((u16)level_events[event_offset + 3] << 8)
                ),
                (s16)(
                    (u16)level_events[event_offset + 4] |
                    ((u16)level_events[event_offset + 5] << 8)
                ),
                opcode,
                level_events[event_offset + 6],
                (s8)level_events[event_offset + 7],
                (s8)level_events[event_offset + 8],
                (s8)level_events[event_offset + 9],
                level_events[event_offset + 10],
                level_events[event_offset + 11],
                level_events[event_offset + 12],
                (u16)(
                    (u16)level_events[event_offset + 13] |
                    ((u16)level_events[event_offset + 14] << 8)
                ),
                level_events[event_offset + 15],
                level_events[event_offset + 16],
                level_events[event_offset + 17],
                level_events[event_offset + 18],
                level_events[event_offset + 19],
                level_events[event_offset + 20]
            );
            length = 21;
        } else if (
            opcode == EVENT_MOVE ||
            opcode == EVENT_ACCEL ||
            opcode == EVENT_REVERSE
        ) {
            apply_control(
                opcode,
                level_events[event_offset + 2],
                level_events[event_offset + 3],
                0,
                0
            );
            length = 4;
        } else if (opcode == EVENT_FIRE) {
            apply_control(
                opcode,
                level_events[event_offset + 2],
                level_events[event_offset + 3],
                level_events[event_offset + 4],
                level_events[event_offset + 5]
            );
            length = 6;
        } else if (opcode == EVENT_FOREGROUND) {
            foreground_phase = 1;
            telemetry_control_count++;
        } else if (opcode == EVENT_SCROLL) {
            set_background_motion(
                level_events[event_offset + 2],
                level_events[event_offset + 3],
                level_events[event_offset + 4],
                level_events[event_offset + 5],
                level_events[event_offset + 6]
            );
            length = 7;
        } else if (opcode == EVENT_REWARD) {
            apply_reward_control(
                level_events[event_offset + 2],
                level_events[event_offset + 3]
            );
            length = 4;
        }
        event_offset += length;
        if (event_offset + 1u >= LEVEL_EVENT_BYTES) return;
        event_time += level_events[event_offset];
    }
}
