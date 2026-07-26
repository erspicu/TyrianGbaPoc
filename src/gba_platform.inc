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

static void commit_vblank_work(void)
{
    volatile u16 *boss_palette =
        (volatile u16 *)SPRITE_PALETTE + OBJ_PAL_BOSS_BAR * 16;
    if (bg1_row_pending) {
        dmaCopy(bg1_row_source, bg1_row_target, MAP_ROW_BYTES);
        bg1_row_pending = 0;
    }
    if (bg2_row_pending) {
        dmaCopy(bg2_row_source, bg2_row_target, MAP_ROW_BYTES);
        bg2_row_pending = 0;
    }
    if (bg3_row_pending) {
        dmaCopy(bg3_row_source, bg3_row_target, MAP_ROW_BYTES);
        bg3_row_pending = 0;
    }
    if (game_state != STATE_TITLE) {
        REG_BG0HOFS = 8;
        REG_BG1HOFS = 8;
        REG_BG2HOFS = 8;
        REG_BG0VOFS = bg1_scroll_pixel & 511;
        REG_BG1VOFS = bg2_scroll_pixel & 511;
        REG_BG2VOFS = bg3_scroll_pixel & 511;
        REG_BG2CNT = BG_PRIORITY(foreground_phase ? 0 : 1) |
                     CHAR_BASE(2) | SCREEN_BASE(BG2_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_2;
    }
    /*
     * Source Sprite2 frames are copied into the reserved OBJ tile cache only
     * during VBlank.  The shadow OAM written below may then safely point at
     * the freshly uploaded frame, exactly one presentation frame after the
     * translated JE_drawEnemy selected it.
     */
    source_enemy_cache_commit();
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

static void audio_load_music(u16 module)
{
    uint16_t source_song =
        module == MOD_TYRIAN_TITLE_FULL ? 29u : 17u;

    mmStop();
    mmEffectCancelAll();
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
