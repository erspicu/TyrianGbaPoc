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
#define TYRIAN_GBA_DEV_PLAYER_INVINCIBLE 0
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
 * The following three numbers are right-aligned at RIGHT_X and are ordered
 * weapon power, hull armor, reserve shield from top to bottom.
 *
 * 下列三項以 RIGHT_X 為右邊界靠右排列，由上到下依序為：
 * 武器能源、機體耐久、備用護盾能源。
 */
#ifndef TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_WEAPON_ENERGY_Y 128
#endif
#ifndef TYRIAN_GBA_LAYOUT_SHIP_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_SHIP_ENERGY_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_SHIP_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_SHIP_ENERGY_Y 138
#endif
#ifndef TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_RIGHT_X
#define TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_RIGHT_X 238
#endif
#ifndef TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_Y
#define TYRIAN_GBA_LAYOUT_RESERVE_ENERGY_Y 148
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
