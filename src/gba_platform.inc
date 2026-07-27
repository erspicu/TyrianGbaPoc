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
    if (game_state == STATE_JUKEBOX) {
        jukebox_commit_vblank();
        if (oam_dirty) {
            dmaCopy(oam_shadow, OAM, sizeof(oam_shadow));
            oam_dirty = 0;
        }
        return;
    }
    if (bg1_row_pending) {
        background_commit_row_uploads(0);
        commit_background_row(bg1_row_source, bg1_row_target);
        bg1_row_pending = 0;
    }
    if (source_background2_upload_pending) {
        /*
         * Low detail leaves this 20 KiB BG layer unallocated.  Match the PC
         * background2over==3 escape hatch by restoring its tiles and current
         * ring atomically in VBlank before enabling BG1.
         */
        background_commit_full_layer(
            1,
            bg2_scroll_pixel >> 3,
            BG1_SCREEN_BLOCK
        );
        bg2_row_pending = 0;
        source_background2_upload_pending = 0;
    } else if (bg2_row_pending) {
        background_commit_row_uploads(1);
        commit_background_row(bg2_row_source, bg2_row_target);
        bg2_row_pending = 0;
    }
    if (bg3_row_pending) {
        background_commit_row_uploads(2);
        commit_background_row(bg3_row_source, bg3_row_target);
        bg3_row_pending = 0;
    }
    if (
        game_state == STATE_PLAY ||
        game_state == STATE_GAME_OVER
    ) {
        u8 background2_priority = 2;
        u8 background3_priority = 1;
        u8 background2_present;

        background2_priority = source_background2_priority();
        background3_priority = source_background3_priority();
        background2_present = source_background2_present();
        REG_BG0HOFS = source_background_hofs(0);
        REG_BG1HOFS = source_background_hofs(1);
        REG_BG2HOFS = source_background_hofs(2);
        REG_BG0VOFS = bg1_presentation_scroll_pixel & 255;
        REG_BG1VOFS = bg2_presentation_scroll_pixel & 255;
        REG_BG2VOFS = bg3_presentation_scroll_pixel & 255;
        REG_BG1CNT = BG_PRIORITY(background2_priority) |
                     CHAR_BASE(background_cache_char_base[1]) |
                     SCREEN_BASE(BG1_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_BG2CNT = BG_PRIORITY(background3_priority) |
                     CHAR_BASE(background_cache_char_base[2]) |
                     SCREEN_BASE(BG2_SCREEN_BLOCK) |
                     BG_16_COLOR | BG_SIZE_1;
        REG_DISPCNT =
            MODE_0 | BG0_ON | BG2_ON | OBJ_ON | OBJ_1D_MAP |
            (background2_present ? BG1_ON : 0);
        /*
         * Count the source software-filter requests independently from the
         * GBA presentation.  This distinguishes a cheap hardware detail
         * profile from a level that actually asks for the PC framebuffer
         * lava/water/iced/blur passes.
         */
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_HIGH
        if (source_parity_level.smoothies[0]) {
            telemetry_detail_lava_frames++;
        }
        if (source_parity_level.smoothies[1]) {
            telemetry_detail_water_frames++;
        }
#endif
#if TYRIAN_GBA_DETAIL_LEVEL >= TYRIAN_GBA_DETAIL_NORMAL
        if (
            source_parity_level.smoothies[2] ||
            source_parity_level.smoothies[4]
        ) {
            telemetry_detail_iced_frames++;
        }
        if (source_parity_level.smoothies[3]) {
            telemetry_detail_blur_frames++;
        }
#endif
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
        if (
            background2_present &&
            !source_parity_level.background2_not_transparent
        ) {
            telemetry_detail_wild_frames++;
        }
#endif
        {
            u16 blend_first = 0;
            u16 blend_second = 0;

#if TYRIAN_GBA_STRESS_LOADOUT
            /*
             * OpenTyrian's large Super Bomb OPTION_SHAPES image is a
             * framebuffer blend.  Only that OBJ uses semi-transparent mode;
             * ordinary objects remain opaque.
             */
            blend_first |= 1u << 4;
            blend_second |= 1u | 2u | 4u | 32u;
#endif
#if TYRIAN_GBA_DETAIL_LEVEL == TYRIAN_GBA_DETAIL_PENTIUM
            if (
                background2_present &&
                !source_parity_level.background2_not_transparent
            ) {
                /*
                 * OpenTyrian Pentium/wild averages the overlay brightness
                 * with the existing framebuffer.  GBA alpha blending is the
                 * closest hardware-native equivalent: BG1 is source
                 * background 2 and BG0/BG2/backdrop are the destination.
                 */
                blend_first |= 1u << 1;
                blend_second |= 1u | 4u | 32u;
            }
#endif
            if (blend_first != 0) {
                REG_BLDCNT =
                    blend_first | (1u << 6) |
                    (blend_second << 8);
                REG_BLDALPHA = 8u | (8u << 8);
            } else {
                REG_BLDCNT = 0;
                REG_BLDALPHA = 0;
            }
        }
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
    source_projectile_cache_commit();
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

static void audio_load_source_song_internal(
    u8 source_song,
    u8 cancel_effects
)
{
    mmStop();
    if (cancel_effects) mmEffectCancelAll();
    /*
     * The authoritative song selection and LDS structure now come directly
     * from ROMFS music.mus.  Maxmod remains the temporary GBA synthesis
     * adapter until the OPL/LDS mixer translation replaces its cached IT
     * modules.
     */
    if (
        source_song >= JUKEBOX_MUSIC_COUNT ||
        !ot_data_mus_select(source_song)
    ) {
        return;
    }
    /*
     * v27 builds all 41 modules in music.mus order.  The generated soundbank
     * asserts source song N == Maxmod module N, so Jukebox and game flow use
     * one authoritative zero-based index without a four-song switch table.
     */
    mmStart(source_song, MM_PLAY_LOOP);
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

static void audio_load_source_song(u8 source_song)
{
    audio_load_source_song_internal(source_song, 1);
}

static void audio_load_source_song_preserve_effects(u8 source_song)
{
    audio_load_source_song_internal(source_song, 0);
}

static void audio_effect(u16 effect)
{
#if TYRIAN_GBA_STRESS_LOADOUT && TYRIAN_GBA_STRESS_PSG_AUDIO
    /*
     * The all-weapons diagnostic can request several effects in one logic
     * tick.  Route those deliberately extreme SFX through one native DMG
     * square channel, leaving the Maxmod background module intact.  This
     * isolates sprite/gameplay cost from sample-mixer voice pressure and is
     * also the requested "basic Game Boy channel" quality mode.
     */
    u16 frequency = (u16)(1420u + (effect & 15u) * 24u);

    if (frequency > 2047u) frequency = 2047u;
    REG_SOUNDCNT_X = 0x0080;
    REG_SOUNDCNT_L |= 0x1177;
    REG_SOUNDCNT_H = (u16)((REG_SOUNDCNT_H & 0xfffcu) | 0x0002u);
    REG_SOUND1CNT_L = 0;
    REG_SOUND1CNT_H =
        (u16)((2u << 6) | (2u << 8) | (8u << 12));
    REG_SOUND1CNT_X =
        (u16)(frequency | (1u << 14) | (1u << 15));
    telemetry_stress_psg_triggers++;
#else
    mmEffect(effect);
#endif
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
