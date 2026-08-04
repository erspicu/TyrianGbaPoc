/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * TyrianGbaPoc user-facing build and layout configuration.
 * TyrianGbaPoc 使用者可調整的建置與版面設定。
 *
 * All coordinates in this file are final 240x160 GBA screen pixels unless a
 * comment explicitly says "PC source coordinate".  Change only the numeric
 * value after each #define, then run Build-GBA-ROM.bat; generated static menu
 * assets and runtime code will be rebuilt from the same settings.
 *
 * 除非註解明確標示「PC 原始座標」，本檔所有座標皆為最終 GBA
 * 240x160 畫面像素。只需修改 #define 後方數值，再執行
 * Build-GBA-ROM.bat；靜態選單資源與 runtime 程式會共用同一份設定重建。
 */
#ifndef TYRIAN_GBA_CONFIGURE_H
#define TYRIAN_GBA_CONFIGURE_H

/* ------------------------------------------------------------------------- */
/* Development switches / 開發測試開關                                      */
/* ------------------------------------------------------------------------- */

/*
 * 1: collisions and hit sounds are retained, but shield/armor/death state is
 *    not changed.  Useful while validating complete levels.
 * 0: normal OpenTyrian damage and death flow.
 *
 * 1：仍計算碰撞並播放受擊聲，但不扣護盾、機體耐久，也不會死亡。
 *    適合驗證完整關卡。
 * 0：使用正常的 OpenTyrian 傷害與死亡流程。
 */
#ifndef TYRIAN_GBA_DEV_PLAYER_INVINCIBLE
#define TYRIAN_GBA_DEV_PLAYER_INVINCIBLE 1
#endif

/*
 * 1: diagnostic maximum-load configuration.  It equips the most projectile-
 *    intensive stock front/rear guns, both sidekicks, a special weapon and a
 *    supplemental super-bomb path.  This is deliberately more violent than a
 *    normal campaign loadout and is intended for CPU/OAM stress measurement.
 * 0: use equipment selected by the normal campaign/Upgrade Ship flow.
 *
 * 1：診斷用極限負荷配置；同時掛上原版資料中高子彈量的前後主武器、
 *    左右側翼、特殊武器與額外超級炸彈路徑，用來壓測 CPU／OAM。
 * 0：使用一般劇情與 Upgrade Ship 選擇的裝備。
 */
#ifndef TYRIAN_GBA_STRESS_LOADOUT
#define TYRIAN_GBA_STRESS_LOADOUT 0
#endif

#if TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 0 && \
    TYRIAN_GBA_DEV_PLAYER_INVINCIBLE != 1
#error TYRIAN_GBA_DEV_PLAYER_INVINCIBLE must be 0 or 1
#endif
#if TYRIAN_GBA_STRESS_LOADOUT != 0 && TYRIAN_GBA_STRESS_LOADOUT != 1
#error TYRIAN_GBA_STRESS_LOADOUT must be 0 or 1
#endif

/* ------------------------------------------------------------------------- */
/* Gameplay presentation / 關卡畫面呈現                                     */
/* ------------------------------------------------------------------------- */

/*
 * OpenTyrian detail profile selected for a normal Build-GBA-ROM.bat build.
 * This is a compile-time choice: unused detail branches are removed by the
 * compiler and therefore add no per-frame runtime switch cost.
 *
 *   LOW     : PC 386 profile.  Disables the normal second background layer
 *             and brightness/translucency-class presentation work.  Authored
 *             background2over==3 events may still restore layer 2, exactly as
 *             in the PC source.
 *   NORMAL  : PC 486 profile.  Keeps BG2, translucent explosions, shadows,
 *             brightness, iced and special-light presentation through GBA
 *             hardware adapters; this is the recommended balance.  Source
 *             blur events remain timed and measured, but the PC temporal
 *             framebuffer average has no distortion-free Mode-0 equivalent.
 *   HIGH    : PC High Detail gates.  Adds the authored lava/water palette and
 *             scanline-wave passes through GBA hardware adapters.
 *   PENTIUM : PC Pentium gates.  Adds wild BG2 colour blending and final
 *             filtration.  Their GBA tile/palette adapters preserve source
 *             timing and intent, but are not a pixel-identical framebuffer.
 *
 * A command-line build such as
 *   Build-GBA-ROM.bat -DetailLevel low
 * overrides this value for that build only.
 *
 * 一般執行 Build-GBA-ROM.bat 時所採用的 OpenTyrian 細節等級。這是
 * 「編譯期」選項，未使用的細節分支會被編譯器移除，不會在每幀增加
 * 動態切換判斷成本。
 *
 *   LOW     ：PC 386 等級；通常關閉第二背景層與亮度／半透明類效果。
 *             原始關卡的 background2over==3 事件仍可照 PC 規則恢復 BG2。
 *   NORMAL  ：PC 486 等級；以 GBA 硬體方式保留 BG2、透明爆炸、陰影、
 *             亮暗、iced 與特殊光照，建議作為正式版平衡值。原始 blur
 *             事件仍照時序執行與記錄，但 PC 的跨影格 framebuffer 平均
 *             在 Mode 0 沒有不破壞畫質的直接硬體等價物。
 *   HIGH    ：PC High Detail 門檻；用 GBA 調色盤與掃描線硬體適配加入
 *             關卡定義的 lava／water 效果。
 *   PENTIUM ：PC Pentium 門檻；再加入 wild BG2 混色與最終 filtration。
 *             GBA 版保留原始觸發時序與視覺意圖，但不是逐像素 framebuffer。
 *
 * 命令列例如 Build-GBA-ROM.bat -DetailLevel low，只會暫時覆寫該次建置。
 */
#define TYRIAN_GBA_CONFIG_DETAIL_LOW 0
#define TYRIAN_GBA_CONFIG_DETAIL_NORMAL 1
#define TYRIAN_GBA_CONFIG_DETAIL_HIGH 2
#define TYRIAN_GBA_CONFIG_DETAIL_PENTIUM 3

#ifndef TYRIAN_GBA_CONFIG_DETAIL_LEVEL
#define TYRIAN_GBA_CONFIG_DETAIL_LEVEL TYRIAN_GBA_CONFIG_DETAIL_NORMAL
#endif

#if TYRIAN_GBA_CONFIG_DETAIL_LEVEL != TYRIAN_GBA_CONFIG_DETAIL_LOW && \
    TYRIAN_GBA_CONFIG_DETAIL_LEVEL != TYRIAN_GBA_CONFIG_DETAIL_NORMAL && \
    TYRIAN_GBA_CONFIG_DETAIL_LEVEL != TYRIAN_GBA_CONFIG_DETAIL_HIGH && \
    TYRIAN_GBA_CONFIG_DETAIL_LEVEL != TYRIAN_GBA_CONFIG_DETAIL_PENTIUM
#error TYRIAN_GBA_CONFIG_DETAIL_LEVEL must be LOW, NORMAL, HIGH or PENTIUM
#endif

/*
 * 1: release gameplay uses the measured whole-scene presentation scheduler.
 *    Source logic follows wall-clock time and Maxmod is serviced once per LCD
 *    VBlank, while a scene that cannot safely meet the next deadline keeps the
 *    previous complete frame.  No partial OAM/VRAM scene is ever presented.
 * 0: render every source logic tick directly; intended only for controlled
 *    performance A/B tests because an over-budget tick causes real slowdown.
 *
 * 1：正式版啟用已量測的「完整場景」動態掉幀。遊戲邏輯維持真實時間節奏，
 *    Maxmod 每個 LCD VBlank 仍更新一次；若新場景無法安全趕上期限，就保留
 *    上一張完整畫面，不會送出半套 OAM／VRAM 資料。
 * 0：每個來源邏輯 tick 都直接渲染；只建議用於受控 A/B，超時會造成真實慢動作。
 */
#ifndef TYRIAN_GBA_DYNAMIC_FRAME_DROP
#define TYRIAN_GBA_DYNAMIC_FRAME_DROP 1
#endif

/*
 * Keep source logic tied to elapsed LCD periods when presentation drops a
 * frame.  This must remain enabled with the release drop-frame scheduler.
 * 掉幀時仍依 LCD 經過時間追上來源邏輯；正式版動態掉幀必須搭配開啟。
 */
#ifndef TYRIAN_GBA_WALL_CLOCK_LOGIC
#define TYRIAN_GBA_WALL_CLOCK_LOGIC TYRIAN_GBA_DYNAMIC_FRAME_DROP
#endif

/*
 * The PC battle viewport is 264x184, while the GBA LCD is 240x160.  Keep
 * source gameplay at 1:1 pixels and use the otherwise hidden 24-pixel slack
 * as a soft camera.  The camera stays centred while the player is inside the
 * dead zone, then eases toward the appropriate source edge.
 *
 * PC 戰鬥視野為 264x184，GBA LCD 為 240x160。遊戲仍維持 1:1 像素，
 * 並把原本裁掉的橫向與縱向各 24 像素作為柔性鏡頭範圍。玩家位於
 * 死區內時保持置中，接近來源視野邊緣時才平順移動裁切位置。
 */
#ifndef TYRIAN_GBA_SOFT_CROP_CAMERA
#define TYRIAN_GBA_SOFT_CROP_CAMERA 1
#endif

/* PC gameplay coordinates / PC 戰鬥座標。 */
#ifndef TYRIAN_GBA_CAMERA_SOURCE_CENTER_X
#define TYRIAN_GBA_CAMERA_SOURCE_CENTER_X 156
#endif
#ifndef TYRIAN_GBA_CAMERA_SOURCE_CENTER_Y
#define TYRIAN_GBA_CAMERA_SOURCE_CENTER_Y 92
#endif

/*
 * Half-size of the stationary dead zone.  60x40 means a 120x80 centre zone.
 * 靜止死區的半寬／半高；60x40 代表中央 120x80 區域。
 */
#ifndef TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_X
#define TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_X 60
#endif
#ifndef TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_Y
#define TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_Y 40
#endif

/*
 * First-order smoothing divisor: 2 means 1/4 of the remaining distance per
 * 30 Hz logic tick.  Larger values feel heavier but take longer to settle.
 *
 * 一階平滑位移量：2 代表每個 30 Hz 邏輯 tick 移動剩餘距離的 1/4。
 * 數值越大越有阻尼感，但抵達目標所需時間也越長。
 */
#ifndef TYRIAN_GBA_CAMERA_RESPONSE_SHIFT
#define TYRIAN_GBA_CAMERA_RESPONSE_SHIFT 2
#endif

#if TYRIAN_GBA_SOFT_CROP_CAMERA != 0 && \
    TYRIAN_GBA_SOFT_CROP_CAMERA != 1
#error TYRIAN_GBA_SOFT_CROP_CAMERA must be 0 or 1
#endif
#if TYRIAN_GBA_CAMERA_RESPONSE_SHIFT < 1 || \
    TYRIAN_GBA_CAMERA_RESPONSE_SHIFT > 6
#error TYRIAN_GBA_CAMERA_RESPONSE_SHIFT must be in the range 1..6
#endif

/* ------------------------------------------------------------------------- */
/* Menu-to-level music transition / 選單進關卡音樂轉場                      */
/* ------------------------------------------------------------------------- */

/*
 * Fade only the Maxmod module, not UI sound effects.  After the fade reaches
 * zero, retain one silent VBlank before mmStop(), then start the level module
 * muted and fade it in.  This avoids a discontinuity in the Direct Sound FIFO.
 *
 * 只淡出 Maxmod 背景模組，不切斷 UI 音效。音量歸零後保留一個靜音
 * VBlank 才呼叫 mmStop()；關卡歌曲以零音量啟動後再淡入，避免
 * Direct Sound FIFO 的波形不連續爆音。
 */
#ifndef TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS
#define TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS 18
#endif
#ifndef TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS
#define TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS 1
#endif
#ifndef TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS
#define TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS 30
#endif

#if TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS < 1
#error TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS must be at least 1
#endif
#if TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS < 1
#error TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS must be at least 1
#endif
#if TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS < 1
#error TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS must be at least 1
#endif
#if TYRIAN_GBA_LEVEL_MUSIC_FADE_OUT_VBLANKS > 255 || \
    TYRIAN_GBA_LEVEL_MUSIC_SILENT_VBLANKS > 255 || \
    TYRIAN_GBA_LEVEL_MUSIC_FADE_IN_VBLANKS > 255
#error Level music transition counts must fit in one byte
#endif

/* ------------------------------------------------------------------------- */
/* In-level HUD and notices / 關卡內 HUD 與系統提示                          */
/* ------------------------------------------------------------------------- */

/* Accumulated cash, left-aligned / 累積金額，以左側座標向右排列。 */
#ifndef TYRIAN_GBA_LAYOUT_CASH_X
#define TYRIAN_GBA_LAYOUT_CASH_X 22
#endif
#ifndef TYRIAN_GBA_LAYOUT_CASH_Y
#define TYRIAN_GBA_LAYOUT_CASH_Y 148
#endif

/*
 * Persistent special weapon and per-level Super Bomb stock.
 *
 * PC JE_inGameDisplays() draws the equipped special with
 *   blit_sprite2x2(25, 1, spriteSheet10, special[id].itemgraphic)
 * and draws one spriteSheet9 graphic 304 at (30 + 12*n, 160) for every
 * carried Super Bomb. Those absolute 320x200 coordinates fall into the GBA
 * crop or collide with the relocated cash counter, so only presentation
 * coordinates are adapted here; source graphics, composition, count limit
 * and persistence rules remain unchanged.
 *
 * Special Weapon 與單關 Super Bomb 庫存的位置。
 *
 * PC 的 JE_inGameDisplays() 會用 special[id].itemgraphic，在 (25,1)
 * 依原始四塊 Sprite2 組合畫出特殊武器；每顆 Super Bomb 則以
 * spriteSheet9 第 304 號圖，在 (30 + 12*n,160) 逐顆排列。這些
 * 320x200 絕對座標在 GBA 1:1 裁切後會落到畫面外或撞到已搬移的
 * 金額，因此此處只調整最終顯示位置；原圖、四塊拼法、十顆上限與
 * 跨關保存規則完全不變。
 */
#ifndef TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_X
#define TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_X 4
#endif
#ifndef TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_Y
#define TYRIAN_GBA_LAYOUT_SPECIAL_WEAPON_Y 2
#endif
#ifndef TYRIAN_GBA_LAYOUT_SUPERBOMB_X
#define TYRIAN_GBA_LAYOUT_SUPERBOMB_X 4
#endif
#ifndef TYRIAN_GBA_LAYOUT_SUPERBOMB_Y
#define TYRIAN_GBA_LAYOUT_SUPERBOMB_Y 132
#endif
#ifndef TYRIAN_GBA_LAYOUT_SUPERBOMB_X_STEP
#define TYRIAN_GBA_LAYOUT_SUPERBOMB_X_STEP 12
#endif

/*
 * The three compact PC-sidebar values are right-aligned at RIGHT_X.
 * User-selected top-to-bottom order and source colour families:
 *   SHIELD    - PC JE_drawShield hue 0x90 (blue)
 *   ARMOR     - PC JE_drawArmor  hue 0xe0 (brown)
 *   GENERATOR - PC power bar     hue 0x70 (gold)
 *
 * 三項數值以 RIGHT_X 為右邊界靠右排列。由上到下依照使用者指定：
 *   SHIELD    - PC JE_drawShield 的 0x90 藍色色階
 *   ARMOR     - PC JE_drawArmor  的 0xe0 褐色色階
 *   GENERATOR - PC Power Bar     的 0x70 金色色階
 */
#ifndef TYRIAN_GBA_LAYOUT_SHIELD_RIGHT_X
#define TYRIAN_GBA_LAYOUT_SHIELD_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_SHIELD_Y
#define TYRIAN_GBA_LAYOUT_SHIELD_Y 128
#endif
#ifndef TYRIAN_GBA_LAYOUT_ARMOR_RIGHT_X
#define TYRIAN_GBA_LAYOUT_ARMOR_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_ARMOR_Y
#define TYRIAN_GBA_LAYOUT_ARMOR_Y 138
#endif
#ifndef TYRIAN_GBA_LAYOUT_GENERATOR_RIGHT_X
#define TYRIAN_GBA_LAYOUT_GENERATOR_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_GENERATOR_Y
#define TYRIAN_GBA_LAYOUT_GENERATOR_Y 148
#endif

/*
 * Backward-compatible aliases for build scripts which still override the
 * v54 names. New code and documentation should use the semantic names above.
 *
 * 舊版 v54 建置參數的相容別名；新程式與文件請使用上方語意化名稱。
 */
#ifndef TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_RIGHT_X \
    TYRIAN_GBA_LAYOUT_GENERATOR_RIGHT_X
#endif
#ifndef TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_Y \
    TYRIAN_GBA_LAYOUT_GENERATOR_Y
#endif
#ifndef TYRIAN_GBA_LAYOUT_SHIP_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_SHIP_ENERGY_RIGHT_X \
    TYRIAN_GBA_LAYOUT_ARMOR_RIGHT_X
#endif
#ifndef TYRIAN_GBA_LAYOUT_SHIP_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_SHIP_ENERGY_Y TYRIAN_GBA_LAYOUT_ARMOR_Y
#endif
#ifndef TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_RIGHT_X \
    TYRIAN_GBA_LAYOUT_SHIELD_RIGHT_X
#endif
#ifndef TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_Y TYRIAN_GBA_LAYOUT_SHIELD_Y
#endif

/* Fixed gameplay notices after the 1:1 crop / 1:1 裁切後的固定提示位置。 */
#ifndef TYRIAN_GBA_LAYOUT_PAUSED_X
#define TYRIAN_GBA_LAYOUT_PAUSED_X 84
#endif
#ifndef TYRIAN_GBA_LAYOUT_PAUSED_Y
#define TYRIAN_GBA_LAYOUT_PAUSED_Y 78
#endif
#ifndef TYRIAN_GBA_LAYOUT_SECRET_LEVEL_X
#define TYRIAN_GBA_LAYOUT_SECRET_LEVEL_X 54
#endif
#ifndef TYRIAN_GBA_LAYOUT_SECRET_LEVEL_Y
#define TYRIAN_GBA_LAYOUT_SECRET_LEVEL_Y 10
#endif
#ifndef TYRIAN_GBA_LAYOUT_INSERT_COIN_X
#define TYRIAN_GBA_LAYOUT_INSERT_COIN_X 79
#endif
#ifndef TYRIAN_GBA_LAYOUT_INSERT_COIN_Y
#define TYRIAN_GBA_LAYOUT_INSERT_COIN_Y 10
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_OVER_X
#define TYRIAN_GBA_LAYOUT_GAME_OVER_X 84
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_OVER_Y
#define TYRIAN_GBA_LAYOUT_GAME_OVER_Y 48
#endif

/* Boss bars / 魔王血條。 */
#ifndef TYRIAN_GBA_LAYOUT_BOSS_BAR_Y
#define TYRIAN_GBA_LAYOUT_BOSS_BAR_Y 6
#endif
#ifndef TYRIAN_GBA_LAYOUT_BOSS_BAR_SINGLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_BOSS_BAR_SINGLE_CENTER_X 116
#endif
#ifndef TYRIAN_GBA_LAYOUT_BOSS_BAR_LEFT_CENTER_X
#define TYRIAN_GBA_LAYOUT_BOSS_BAR_LEFT_CENTER_X 94
#endif
#ifndef TYRIAN_GBA_LAYOUT_BOSS_BAR_RIGHT_CENTER_X
#define TYRIAN_GBA_LAYOUT_BOSS_BAR_RIGHT_CENTER_X 139
#endif

/* ------------------------------------------------------------------------- */
/* End-of-level summary / 擊敗 Boss 後的關卡摘要                            */
/* ------------------------------------------------------------------------- */

#ifndef TYRIAN_GBA_LAYOUT_STATS_COMPLETED_X
#define TYRIAN_GBA_LAYOUT_STATS_COMPLETED_X 20
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_COMPLETED_Y
#define TYRIAN_GBA_LAYOUT_STATS_COMPLETED_Y 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CASH_X
#define TYRIAN_GBA_LAYOUT_STATS_CASH_X 30
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CASH_Y
#define TYRIAN_GBA_LAYOUT_STATS_CASH_Y 38
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_ENEMIES_X
#define TYRIAN_GBA_LAYOUT_STATS_ENEMIES_X 40
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_ENEMIES_Y
#define TYRIAN_GBA_LAYOUT_STATS_ENEMIES_Y 78
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_X
#define TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_X 30
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_Y
#define TYRIAN_GBA_LAYOUT_STATS_CUBES_LABEL_Y 108
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CUBE_FIRST_X
#define TYRIAN_GBA_LAYOUT_STATS_CUBE_FIRST_X 50
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CUBE_Y
#define TYRIAN_GBA_LAYOUT_STATS_CUBE_Y 123
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_CUBE_X_STEP
#define TYRIAN_GBA_LAYOUT_STATS_CUBE_X_STEP 30
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_PROMPT_X
#define TYRIAN_GBA_LAYOUT_STATS_PROMPT_X 90
#endif
#ifndef TYRIAN_GBA_LAYOUT_STATS_PROMPT_Y
#define TYRIAN_GBA_LAYOUT_STATS_PROMPT_Y 148
#endif

/*
 * Inter-level story text is rendered over the stock PC picture after a
 * 320x200 -> 240x160 scale.  Normal prose keeps the PC y=55 relationship;
 * warning prose begins below the flashing top bar.
 *
 * 關卡間劇情文字會疊在由 320x200 縮至 240x160 的 PC 原圖上。
 * 一般文字保留 PC y=55 的相對位置；警告文字則避開頂端閃爍列。
 */
#ifndef TYRIAN_GBA_LAYOUT_SCENE_TEXT_X
#define TYRIAN_GBA_LAYOUT_SCENE_TEXT_X 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_SCENE_TEXT_Y
#define TYRIAN_GBA_LAYOUT_SCENE_TEXT_Y 44
#endif
#ifndef TYRIAN_GBA_LAYOUT_SCENE_WARNING_TEXT_Y
#define TYRIAN_GBA_LAYOUT_SCENE_WARNING_TEXT_Y 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT
#define TYRIAN_GBA_LAYOUT_SCENE_TEXT_RIGHT 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_SCENE_TEXT_LINE_STEP
#define TYRIAN_GBA_LAYOUT_SCENE_TEXT_LINE_STEP 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_SCENE_PROMPT_Y
#define TYRIAN_GBA_LAYOUT_SCENE_PROMPT_Y 148
#endif

/*
 * ZH: levelsN.dat 的 W??1x 紅色文字模式，其按鍵提示沿用 PC y=118
 *     經 200->160 縮放的位置；這與 Wy 警告閃爍條是不同旗標。
 * EN: Prompt Y for the W??1x red-text mode (PC y=118 scaled to 160).
 *     This is deliberately independent from the Wy warning-bar flag.
 */
#ifndef TYRIAN_GBA_LAYOUT_SCENE_WARNING_PROMPT_Y
#define TYRIAN_GBA_LAYOUT_SCENE_WARNING_PROMPT_Y 94
#endif

/* ------------------------------------------------------------------------- */
/* Pre-GameMenu setup pages / 進入 GameMenu 前的設定頁                      */
/* ------------------------------------------------------------------------- */

/*
 * These pages use the dedicated mixed-case semibold font.  TITLE controls the
 * Start New Game/Demo/Jukebox page.  SETUP controls Play Mode and Difficulty.
 * EPISODE uses a left-aligned list because its stock names are longer.
 *
 * 這些頁面使用專屬、可區分大小寫的半粗體字型。TITLE 控制首頁；
 * SETUP 控制 Play Mode 與 Difficulty；EPISODE 因名稱較長而採靠左排列。
 */
#ifndef TYRIAN_GBA_LAYOUT_TITLE_MENU_CENTER_X
#define TYRIAN_GBA_LAYOUT_TITLE_MENU_CENTER_X 120
#endif
#ifndef TYRIAN_GBA_LAYOUT_TITLE_MENU_FIRST_Y
#define TYRIAN_GBA_LAYOUT_TITLE_MENU_FIRST_Y 86
#endif
#ifndef TYRIAN_GBA_LAYOUT_TITLE_MENU_ROW_STEP
#define TYRIAN_GBA_LAYOUT_TITLE_MENU_ROW_STEP 10
#endif

#ifndef TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X
#define TYRIAN_GBA_LAYOUT_SETUP_HEADER_CENTER_X 120
#endif
#ifndef TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y
#define TYRIAN_GBA_LAYOUT_SETUP_HEADER_Y 15
#endif
#ifndef TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X
#define TYRIAN_GBA_LAYOUT_SETUP_CHOICE_CENTER_X 120
#endif
#ifndef TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y
#define TYRIAN_GBA_LAYOUT_SETUP_CHOICE_FIRST_Y 43
#endif
#ifndef TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP
#define TYRIAN_GBA_LAYOUT_SETUP_CHOICE_ROW_STEP 19
#endif

#ifndef TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_X
#define TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_X 15
#endif
#ifndef TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_RIGHT
#define TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_RIGHT 230
#endif
#ifndef TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_FIRST_Y
#define TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_FIRST_Y 40
#endif
#ifndef TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_ROW_STEP
#define TYRIAN_GBA_LAYOUT_EPISODE_CHOICE_ROW_STEP 24
#endif

/* ------------------------------------------------------------------------- */
/* Open static menus / 已開放的靜態選單                                     */
/* ------------------------------------------------------------------------- */

/* Game Menu right panel / Game Menu 右側文字面板。 */
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_CENTER_X 178
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_Y
#define TYRIAN_GBA_LAYOUT_GAME_MENU_TITLE_Y 6
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_X
#define TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_X 125
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_RIGHT
#define TYRIAN_GBA_LAYOUT_GAME_MENU_ITEM_RIGHT 238
#endif
/* PC source-coordinate rows; runtime applies the shared 200->160 adapter. */
/* PC 原始座標列；runtime 會套用共用的 200→160 轉換。 */
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_FIRST_SOURCE_Y
#define TYRIAN_GBA_LAYOUT_GAME_MENU_FIRST_SOURCE_Y 38
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_SOURCE_ROW_STEP
#define TYRIAN_GBA_LAYOUT_GAME_MENU_SOURCE_ROW_STEP 16
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_QUIT_SOURCE_GAP
#define TYRIAN_GBA_LAYOUT_GAME_MENU_QUIT_SOURCE_GAP 16
#endif

/*
 * Source JE_drawMainMenuHelpText() uses PC (10,187).  DARKEN offsets the
 * actual glyph origin by one source pixel; the defaults below are its
 * 300x200 -> 240x160 GBA-native counterpart.
 *
 * PC 的 JE_drawMainMenuHelpText() 使用 (10,187)，DARKEN 會把實際字形
 * 再偏移一個來源像素；下列預設值是其 300x200 → 240x160 原生 GBA
 * 對應位置。
 */
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X
#define TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_X 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y
#define TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_Y 150
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT
#define TYRIAN_GBA_LAYOUT_GAME_MENU_HELP_RIGHT 238
#endif

/*
 * Dynamic left-panel score origin, derived from source JE_textShade(65,173).
 * 左側動態金額起點，依來源 JE_textShade(65,173) 的實際字形偏移換算。
 */
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_X
#define TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_X 52
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_Y
#define TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_Y 139
#endif
#ifndef TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_RIGHT
#define TYRIAN_GBA_LAYOUT_GAME_MENU_CASH_RIGHT 116
#endif

/*
 * Options remains on the Game Menu chrome.  Load/Save follows the PC
 * JE_loadScreen() full-width PIC 2 layout, scaled to 240x160 with its
 * one-player name, Last level and Episode columns.  These are final screen
 * coordinates; keep the slot step at least 8 pixels.
 *
 * Options 仍沿用 Game Menu 底圖；讀檔／存檔則依 PC JE_loadScreen() 改為
 * PIC 2 全寬單人介面，保留名稱、Last level 與 Episode 三欄並適配
 * 240x160。下列皆為最終螢幕座標；槽列距至少保留 8 像素。
 */
#ifndef TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_CENTER_X 180
#endif
#ifndef TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y
#define TYRIAN_GBA_LAYOUT_OPTIONS_TITLE_Y 6
#endif
#ifndef TYRIAN_GBA_LAYOUT_OPTIONS_CENTER_X
#define TYRIAN_GBA_LAYOUT_OPTIONS_CENTER_X 180
#endif
#ifndef TYRIAN_GBA_LAYOUT_OPTIONS_FIRST_Y
#define TYRIAN_GBA_LAYOUT_OPTIONS_FIRST_Y 38
#endif
#ifndef TYRIAN_GBA_LAYOUT_OPTIONS_ROW_STEP
#define TYRIAN_GBA_LAYOUT_OPTIONS_ROW_STEP 24
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_SLOT_X
#define TYRIAN_GBA_LAYOUT_SAVE_SLOT_X 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_NAME_RIGHT
#define TYRIAN_GBA_LAYOUT_SAVE_NAME_RIGHT 86
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_X
#define TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_X 86
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_RIGHT
#define TYRIAN_GBA_LAYOUT_SAVE_LAST_LEVEL_RIGHT 195
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_EPISODE_X
#define TYRIAN_GBA_LAYOUT_SAVE_EPISODE_X 196
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT
#define TYRIAN_GBA_LAYOUT_SAVE_SLOT_RIGHT 239
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_TITLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_SAVE_TITLE_CENTER_X 120
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_TITLE_Y
#define TYRIAN_GBA_LAYOUT_SAVE_TITLE_Y 4
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_SLOT_FIRST_Y
#define TYRIAN_GBA_LAYOUT_SAVE_SLOT_FIRST_Y 24
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_SLOT_ROW_STEP
#define TYRIAN_GBA_LAYOUT_SAVE_SLOT_ROW_STEP 10
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_EXIT_Y
#define TYRIAN_GBA_LAYOUT_SAVE_EXIT_Y 139
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_FOOTER_X
#define TYRIAN_GBA_LAYOUT_SAVE_FOOTER_X 8
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_FOOTER_Y
#define TYRIAN_GBA_LAYOUT_SAVE_FOOTER_Y 151
#endif

/*
 * Build identity in the lower-left corner of the title screen.
 * 首頁左下角的專案名稱、Git short hash 顯示位置。
 */
#ifndef TYRIAN_GBA_LAYOUT_TITLE_BUILD_X
#define TYRIAN_GBA_LAYOUT_TITLE_BUILD_X 2
#endif
#ifndef TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y
#define TYRIAN_GBA_LAYOUT_TITLE_BUILD_Y 151
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_NAME_X
#define TYRIAN_GBA_LAYOUT_SAVE_NAME_X 54
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_NAME_Y
#define TYRIAN_GBA_LAYOUT_SAVE_NAME_Y 62
#endif
#ifndef TYRIAN_GBA_LAYOUT_SAVE_NAME_HELP_X
#define TYRIAN_GBA_LAYOUT_SAVE_NAME_HELP_X 46
#endif

/* Upgrade Ship category panel / Upgrade Ship 分類面板。 */
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_CENTER_X 176
#endif
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y
#define TYRIAN_GBA_LAYOUT_UPGRADE_TITLE_Y 7
#endif
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_X
#define TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_X 125
#endif
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_RIGHT
#define TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_RIGHT 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_FIRST_Y
#define TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_FIRST_Y 30
#endif
#ifndef TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_ROW_STEP
#define TYRIAN_GBA_LAYOUT_UPGRADE_ITEM_ROW_STEP 10
#endif

/* Next Level text panel; the planet/map remains an independent OBJ/BG path. */
/* Next Level 文字面板；星球與地圖仍由獨立 OBJ／BG 路徑處理。 */
#ifndef TYRIAN_GBA_LAYOUT_NEXT_TITLE_CENTER_X
#define TYRIAN_GBA_LAYOUT_NEXT_TITLE_CENTER_X 176
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_TITLE_Y
#define TYRIAN_GBA_LAYOUT_NEXT_TITLE_Y 7
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_PANEL_X
#define TYRIAN_GBA_LAYOUT_NEXT_PANEL_X 126
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_PANEL_Y
#define TYRIAN_GBA_LAYOUT_NEXT_PANEL_Y 29
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_PANEL_WIDTH
#define TYRIAN_GBA_LAYOUT_NEXT_PANEL_WIDTH 108
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_PANEL_HEIGHT
#define TYRIAN_GBA_LAYOUT_NEXT_PANEL_HEIGHT 116
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_CHOICE_FIRST_Y
#define TYRIAN_GBA_LAYOUT_NEXT_CHOICE_FIRST_Y 32
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_CHOICE_ROW_STEP
#define TYRIAN_GBA_LAYOUT_NEXT_CHOICE_ROW_STEP 14
#endif
#ifndef TYRIAN_GBA_LAYOUT_NEXT_EXIT_Y
#define TYRIAN_GBA_LAYOUT_NEXT_EXIT_Y 119
#endif

/* Quit Game confirmation text; dialog artwork itself remains stock Tyrian. */
/* Quit Game 確認文字；對話框圖形本身仍使用原版 Tyrian 素材。 */
#ifndef TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X
#define TYRIAN_GBA_LAYOUT_QUIT_QUESTION_X 37
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y
#define TYRIAN_GBA_LAYOUT_QUIT_QUESTION_Y 53
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT
#define TYRIAN_GBA_LAYOUT_QUIT_QUESTION_RIGHT 174
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_HELP_X
#define TYRIAN_GBA_LAYOUT_QUIT_HELP_X 37
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_HELP_Y
#define TYRIAN_GBA_LAYOUT_QUIT_HELP_Y 73
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_HELP_RIGHT
#define TYRIAN_GBA_LAYOUT_QUIT_HELP_RIGHT 174
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X
#define TYRIAN_GBA_LAYOUT_QUIT_OK_CENTER_X 66
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X
#define TYRIAN_GBA_LAYOUT_QUIT_CANCEL_CENTER_X 137
#endif
#ifndef TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y
#define TYRIAN_GBA_LAYOUT_QUIT_CHOICES_Y 108
#endif

#endif /* TYRIAN_GBA_CONFIGURE_H */
