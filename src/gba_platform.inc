/*
 * Transitional single-translation-unit GBA platform layer.
 * It stays isolated from source-parity gameplay and can become a standalone
 * object after the shared runtime state is encapsulated.
 */

static void vblank_handler(void)
{
    telemetry_vblank_irqs++;
    mmVBlank();
}

static void hide_all_sprites(void)
{
    u16 index;
    for (index = 0; index < HARDWARE_OAM_ENTRIES; index++) {
        oam_shadow[index].attr0 = ATTR0_DISABLED;
        oam_shadow[index].attr1 = 0;
        oam_shadow[index].attr2 = 0;
        oam_shadow[index].dummy = 0;
    }
    previous_oam_count = 0;
    oam_dirty = 1;
}

static void commit_background_row(const u8 *source, u16 *left_target)
{
    dmaCopy(source, left_target, MAP_HALF_ROW_BYTES);
    dmaCopy(
        source + MAP_HALF_ROW_BYTES,
        left_target + MAP_SCREEN_BLOCK_WORDS,
        MAP_HALF_ROW_BYTES
    );
}

static void commit_vblank_work(void)
{
    volatile u16 *boss_palette =
        (volatile u16 *)SPRITE_PALETTE + OBJ_PAL_BOSS_BAR * 16;

    if (frontend_mode4_active) {
        frontend_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    if (bg1_row_pending) {
        commit_background_row(bg1_row_source, bg1_row_target);
        bg1_row_pending = 0;
    }
    if (source_background2_upload_pending) {
        /*
         * Low detail leaves this 20 KiB BG layer unallocated.  Match the PC
         * background2over==3 escape hatch by restoring its tiles and current
         * ring atomically in VBlank before enabling BG1.
         */
        dmaCopy(bg2_tiles, CHAR_BASE_BLOCK(1), 512 * 32);
        load_ring(
            bg2_map,
            BG2_ROWS,
            bg2_scroll_pixel >> 3,
            BG1_SCREEN_BLOCK
        );
        bg2_row_pending = 0;
        source_background2_upload_pending = 0;
    } else if (bg2_row_pending) {
        commit_background_row(bg2_row_source, bg2_row_target);
        bg2_row_pending = 0;
    }
    if (bg3_row_pending) {
        commit_background_row(bg3_row_source, bg3_row_target);
        bg3_row_pending = 0;
    }
    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        u8 background2_priority = 2;
        u8 background3_priority = 1;

        background2_priority = source_background2_priority();
        background3_priority = source_background3_priority();
        REG_BG0HOFS = source_background_hofs(0);
        REG_BG1HOFS = source_background_hofs(1);
        REG_BG2HOFS = source_background_hofs(2);
        REG_BG0VOFS = bg1_presentation_scroll_pixel & 255;
        REG_BG1VOFS = bg2_presentation_scroll_pixel & 255;
        REG_BG2VOFS = bg3_presentation_scroll_pixel & 255;
        REG_BG1CNT = BG_PRIORITY(background2_priority) |
                     CHAR_BASE(1) | SCREEN_BASE(BG1_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_BG2CNT = BG_PRIORITY(background3_priority) |
                     CHAR_BASE(2) | SCREEN_BASE(BG2_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_DISPCNT =
            MODE_0 | BG0_ON | BG2_ON | OBJ_ON | OBJ_1D_MAP |
            (source_background2_enabled ? BG1_ON : 0);
        if (boss_obj_palette_restore_pending) {
            dmaCopy(
                obj_palette,
                SPRITE_PALETTE,
                512
            );
            boss_obj_palette_restore_pending = 0;
        }
    }
    if (game_over_tile_upload_pending) {
        /*
         * The generated GAME OVER glyphs live in cartridge backing at the
         * first upper Sprite2 cache slot.  At death completion, copy them
         * into the otherwise unused 512..527 runtime gap.  The explosion
         * cache reads its source frames from cartridge ROM, so replacing
         * those VRAM characters cannot corrupt the final death effects.
         */
        dmaCopy(
            obj_tiles + OBJ_TILE_GAME_OVER_SOURCE * 32,
            (u8 *)OBJ_BASE_ADR + OBJ_TILE_GAME_OVER_RUNTIME * 32,
            OBJ_GAME_OVER_TILE_COUNT * 32
        );
        game_over_tile_upload_pending = 0;
    }
    /*
     * Source Sprite2 frames are copied into the reserved OBJ tile cache only
     * during VBlank.  The shadow OAM written below may then safely point at
     * the freshly uploaded frame, exactly one presentation frame after the
     * translated JE_drawEnemy selected it.
     */
    source_enemy_cache_commit();
    source_effect_cache_commit();
    if (oam_dirty) {
        dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
        oam_dirty = 0;
    }
    if (boss_bar_palette_dirty) {
        boss_palette[4] = boss_bar_fill_colours[boss_bar_flash][0];
        boss_palette[5] = boss_bar_fill_colours[boss_bar_flash][1];
        boss_palette[6] = boss_bar_fill_colours[boss_bar_flash][2];
        boss_bar_palette_dirty = 0;
    }
}

static void audio_load_music_internal(
    u16 module,
    u8 cancel_effects
)
{
    uint16_t source_song;

    switch (module) {
    case MOD_TYRIAN_TITLE_FULL:
        source_song = 29u;
        break;
    case MOD_TYRIAN_END_LEVEL_FULL:
        source_song = 9u;
        break;
    case MOD_TYRIAN_GAME_OVER_FULL:
        source_song = 10u;
        break;
    default:
        source_song = 17u;
        break;
    }

    mmStop();
    if (cancel_effects) mmEffectCancelAll();
    /*
     * The authoritative song selection and LDS structure now come directly
     * from ROMFS music.mus.  Maxmod remains the temporary GBA synthesis
     * adapter until the OPL/LDS mixer translation replaces its cached IT
     * modules.
     */
    if (!ot_data_mus_select(source_song)) return;
    mmStart(module, MM_PLAY_LOOP);
    mmSetModuleVolume(896);
    mmSetEffectsVolume(896);
}

static void audio_set_source_music_volume(u8 source_volume)
{
    /*
     * OpenTyrian's tyrMusicVolume/tempVolume range is 0..255.  Preserve the
     * established calibrated Maxmod ceiling (896) while translating the
     * source's one-unit-per-tick death fade.
     */
    mmSetModuleVolume(
        (u16)(((u32)source_volume * 896u + 127u) / 255u)
    );
}

static void audio_load_music(u16 module)
{
    audio_load_music_internal(module, 1);
}

static void audio_load_music_preserve_effects(u16 module)
{
    audio_load_music_internal(module, 0);
}

static void audio_effect(u16 effect)
{
    mmEffect(effect);
}

static void toggle_pause(void)
{
    game_paused = !game_paused;
    /*
     * OpenTyrian JE_pauseGame() freezes the game world while leaving the
     * module running at half music volume. Effects retain their normal mix.
     */
    mmSetModuleVolume(game_paused ? 448 : 896);
    telemetry_pause_toggles++;
}
