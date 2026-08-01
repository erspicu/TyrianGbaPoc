/*
 * Direct levelsN.dat inter-level scene reader.
 *
 * This implementation unit is included by opentyrian_data.c so it can reuse
 * the ROMFS and encrypted-string helpers without exporting internal symbols.
 */
enum { EPISODE_SCENE_LINE_BYTES = 256 };
static char episode_scene_line[
    EPISODE_SCENE_LINE_BYTES
] __attribute__((section(".sbss")));

static bool episode_scene_jump(
    OtEpisodeSceneReader *reader,
    uint16_t section
)
{
    OtRomFsStat script = {0};
    uint32_t position;

    if (
        reader == 0 ||
        section == 0 ||
        reader->jump_count >= 64
    ) {
        return false;
    }
    script.data = reader->script.data;
    script.size = reader->script.size;
    if (!episode_seek_section(&script, section, &position)) return false;
    reader->section = section;
    reader->position = position;
    reader->jump_count++;
    return true;
}

static bool episode_scene_text_copy(
    OtEpisodeSceneAction *action,
    const char *line
)
{
    size_t length;

    if (
        action == 0 ||
        line == 0 ||
        action->text_line_count >= OT_EPISODE_SCENE_TEXT_LINE_COUNT
    ) {
        return false;
    }
    length = strlen(line);
    if (length >= OT_EPISODE_SCENE_TEXT_LINE_BYTES) {
        length = OT_EPISODE_SCENE_TEXT_LINE_BYTES - 1u;
    }
    memcpy(
        action->text[action->text_line_count],
        line,
        length
    );
    action->text[action->text_line_count][length] = '\0';
    action->text_line_count++;
    return true;
}

static bool episode_scene_read_text_block(
    const OtRomFsStat *script,
    uint32_t *position,
    OtEpisodeSceneAction *action,
    char line[EPISODE_SCENE_LINE_BYTES]
)
{
    for (;;) {
        if (!encrypted_pascal_read(
                script,
                position,
                line,
                EPISODE_SCENE_LINE_BYTES
            )) {
            return false;
        }
        if (line[0] == '#') return true;
        if (!episode_scene_text_copy(action, line)) return false;
    }
}

bool ot_data_episode_scene_begin(
    uint8_t episode,
    uint16_t main_section,
    uint8_t play_mode,
    uint8_t difficulty,
    const OtEpisodeRouteState *route,
    OtEpisodeSceneReader *reader
)
{
    OtRomFsStat script;
    OtEpisodeRouteState default_route = {true, false, 0};
    char filename[] = "levels1.dat";
    uint32_t position;

    if (!initialization_attempted) ot_data_init();
    if (
        !catalog.initialized ||
        reader == 0 ||
        episode == 0 ||
        episode > OT_EPISODE_COUNT ||
        main_section == 0
    ) {
        return false;
    }
    filename[6] = (char)('0' + episode);
    if (
        !stat_data_file(filename, &script) ||
        !episode_seek_section(&script, main_section, &position)
    ) {
        return false;
    }
    *reader = (OtEpisodeSceneReader){0};
    reader->script.data = script.data;
    reader->script.size = script.size;
    reader->route = route != 0 ? *route : default_route;
    reader->position = position;
    reader->section = main_section;
    reader->episode = episode;
    reader->play_mode = play_mode;
    reader->difficulty = difficulty;
    reader->text_bank = 14;
    reader->valid = true;
    return true;
}

int8_t ot_data_episode_scene_step(
    OtEpisodeSceneReader *reader,
    uint8_t secret_hint,
    OtEpisodeSceneAction *action
)
{
    OtRomFsStat script = {0};
    char *line = episode_scene_line;
    uint16_t line_count;

    if (
        reader == 0 ||
        action == 0 ||
        !reader->valid ||
        reader->done
    ) {
        return reader != 0 && reader->done ? 0 : -1;
    }
    script.data = reader->script.data;
    script.size = reader->script.size;
    *action = (OtEpisodeSceneAction){0};
    if (reader->end_hint_pending) {
        uint8_t block;

        if (secret_hint == 0) secret_hint = 1;
        if (secret_hint > 9) secret_hint = 9;
        for (block = 1; block < secret_hint; block++) {
            do {
                if (!encrypted_pascal_read(
                        &script,
                        &reader->position,
                        line,
                        sizeof(episode_scene_line)
                    )) {
                    reader->valid = false;
                    return -1;
                }
            } while (line[0] != '#');
        }
        action->type = OT_EPISODE_SCENE_ACTION_END_EPISODE;
        action->text_bank = reader->text_bank;
        if (!episode_scene_read_text_block(
                &script,
                &reader->position,
                action,
                line
            )) {
            reader->valid = false;
            return -1;
        }
        reader->end_hint_pending = false;
        reader->done = true;
        return 1;
    }
    for (line_count = 0; line_count < 4096; line_count++) {
        uint16_t destination = 0;
        bool jump = false;

        if (!encrypted_pascal_read(
                &script,
                &reader->position,
                line,
                sizeof(episode_scene_line)
            )) {
            reader->valid = false;
            return -1;
        }
        if (line[0] == '*') {
            /* JE_loadMap() permits authored sequential-section fallthrough. */
            reader->section++;
            continue;
        }
        if (line[0] != ']') continue;
        switch (line[1]) {
        case 'J':
            destination = script_number(line + 3);
            jump = true;
            break;

        case '2':
            if (reader->play_mode != 0 || reader->engage_mode) {
                destination = script_number(line + 3);
                jump = true;
            }
            break;

        case 'w':
            if (reader->route.player_ship == 13) {
                destination = script_number(line + 3);
                jump = true;
            }
            break;

        case 't':
            if (reader->route.level_timer_expired) {
                destination = script_number(line + 3);
                jump = true;
            }
            break;

        case 'l':
            if (!reader->route.player_alive) {
                destination = script_number(line + 3);
                jump = true;
            }
            break;

        case 'H':
            if (reader->difficulty < 3) {
                destination = script_number(line + 4);
                jump = true;
            }
            break;

        case 'h':
            if (
                reader->difficulty > 2 &&
                !encrypted_pascal_read(
                    &script,
                    &reader->position,
                    line,
                    sizeof(episode_scene_line)
                )
            ) {
                reader->valid = false;
                return -1;
            }
            break;

        case 'e':
            reader->engage_mode = true;
            break;

        case '@':
            reader->text_bank = reader->text_bank == 14 ? 15 : 14;
            break;

        case 'I': {
            uint8_t item_group;

            for (item_group = 0; item_group < 9; item_group++) {
                if (!encrypted_pascal_read(
                        &script,
                        &reader->position,
                        line,
                        sizeof(episode_scene_line)
                    )) {
                    reader->valid = false;
                    return -1;
                }
            }
            action->type = OT_EPISODE_SCENE_ACTION_END_MAP;
            reader->done = true;
            return 1;
        }

        case 'L':
            action->type = OT_EPISODE_SCENE_ACTION_END_MAP;
            reader->done = true;
            return 1;

        case 'P':
            action->type = OT_EPISODE_SCENE_ACTION_PICTURE;
            action->value = script_number(line + 3);
            return 1;

        case 'U':
            action->type = OT_EPISODE_SCENE_ACTION_PAN_UP;
            action->value = script_number(line + 3);
            return 1;

        case 'V':
            action->type = OT_EPISODE_SCENE_ACTION_SLIDE_UP;
            action->value = script_number(line + 3);
            return 1;

        case 'R':
            action->type = OT_EPISODE_SCENE_ACTION_PAN_RIGHT;
            action->value = script_number(line + 3);
            return 1;

        case 'M': {
            uint16_t one_based_song = script_number(line + 3);

            action->type = OT_EPISODE_SCENE_ACTION_MUSIC;
            action->value = one_based_song == 0 ? 0 : one_based_song - 1u;
            return 1;
        }

        case 'W':
            action->type = OT_EPISODE_SCENE_ACTION_TEXT;
            action->warning = line[2] == 'y';
            action->value = script_number(line + 4);
            action->warning_red = action->value / 10u != 0;
            action->text_bank =
                action->warning_red ? 7 : reader->text_bank;
            if (!episode_scene_read_text_block(
                    &script,
                    &reader->position,
                    action,
                    line
                )) {
                reader->valid = false;
                return -1;
            }
            return 1;

        case 'C':
            action->type = OT_EPISODE_SCENE_ACTION_CLEAR;
            return 1;

        case 'B':
            action->type = OT_EPISODE_SCENE_ACTION_FADE_BLACK;
            return 1;

        case 'F':
            action->type = OT_EPISODE_SCENE_ACTION_FLASH_CLEAR;
            return 1;

        case 'A':
            action->type = OT_EPISODE_SCENE_ACTION_ANIMATION;
            return 1;

        case 'Q':
            /* PC consumes mt_rand() only when ]Q itself is reached. */
            reader->end_hint_pending = true;
            action->type = OT_EPISODE_SCENE_ACTION_SELECT_END_HINT;
            return 1;

        default:
            break;
        }
        if (jump) {
            if (!episode_scene_jump(reader, destination)) {
                reader->valid = false;
                return -1;
            }
        }
    }
    reader->valid = false;
    return -1;
}
