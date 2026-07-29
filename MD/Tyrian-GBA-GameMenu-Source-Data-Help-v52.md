# Tyrian GBA Game Menu 原始資料與靜態介面同步（v52）

日期：2026-07-30

## 本階段完成項目

### 1. Game Menu 底部提示

- 依 OpenTyrian `helptext.c` 的 `mainMenuHelp[34]` 格式，直接從
  ROMFS 內的 `tyrian.hdt` 解密載入全部 34 筆提示。
- Game Menu 使用來源 `menuHelp[MENU_FULL_GAME]` 對照：
  `1, 34, 2, 3, 4, 5`。
- Upgrade Ship 使用來源 `menuHelp[MENU_UPGRADES]` 對照：
  `6, 7, 8, 9, 10, 11, 11, 12`。
- 位置與明暗關係依
  `JE_drawMainMenuHelpText()`／`JE_textShade(10,187,...,14,1,DARKEN)`
  換算到 240×160。
- 字形使用可辨識大小寫的極小 mixed-case face，不再以全大寫或
  自訂文案代替原始 HDT 字串。

初版 runtime 逐字縮放會在轉場中執行大量軟體除法。最終改為
build 時預先建立 34 組 240×11 最終解析度 strips；runtime 只做
對齊的 32-bit ROM 複製，保留來源內容並避免音訊中斷。

### 2. 左側玩家狀態

- 飛船圖仍由原始 `OPTION_SHAPES`／compound Sprite2 資料組合。
- 金額依來源 `JE_textShade(65,173,...)` 換算，並保留動態數值。
- Armor 直接使用玩家目前 armor，而不是飛船資料表的初始 damage。
- Shield 依目前 shield 型號的 `mpwr * 2` 顯示。
- Armor／Shield bar 逐項對照 `JE_barDrawShadow()`：
  - `xsize--`／`ysize--`
  - 分段間距
  - 陰影
  - 上下邊與左上高光
  - 不足一整段的 remainder
- PC 的 1-pixel primitive 不再各自取整縮放，而是先轉成一個連續
  GBA-native rectangle，修正原先斷裂、缺列與錯格。
- Game Menu 的 Data Cube 依來源 `OPTION_SHAPES` sprite 34、來源座標
  `190 + i*18, 37` 與 dark shadow 繪製；數量來自目前遊戲狀態。

Episode 4 的 item definition 位於 `tyrian4.lvl`，不是共用
`tyrian.hdt`。新增只選擇該章 item database 的 ROMFS 介面，讓選單
可讀取正確船體、盾牌與裝備資料，而不必為了畫選單先載入整關。

### 3. 動態更新與轉場

- Game Menu 選項改變後，會同步恢復資料方塊與底部提示，避免右側
  selection patch 抹掉動態內容。
- Game Menu、Upgrade Ship、Quit dialog 的冷啟動與共享轉場都會
  恢復正確的提示與左側狀態。
- `Configure.h` 新增中英文註解的 Game Menu 提示與金額位置參數。
- build metadata 加入 help strip 尺寸、總量與 compile-time
  一致性檢查。

## 來源規格依據

- `vendor/opentyrian/src/helptext.c`
  - `mainMenuHelp[34]`
  - `menuHelp[MENU_FULL_GAME]`
  - `menuHelp[MENU_UPGRADES]`
- `vendor/opentyrian/src/game_menu.c`
  - Data Cube sprite 34
  - cash、armor、shield 的座標與數值
  - `JE_drawMainMenuHelpText()`
- `vendor/opentyrian/src/nortvars.c`
  - `JE_barDrawShadow()`
- ROMFS：
  - Episode 1–3 item data：`tyrian.hdt`
  - Episode 4 item data：`tyrian4.lvl`

## 驗證結果

### 靜態轉場壓力測試

8 條路徑各反覆 120 次，全部：

- `missed_vblanks=0`
- `failures=0`
- `music_active=1`
- runtime SHP／Sprite2 decode 均為 0

| 路徑 | 最大 CPU cycles |
|---|---:|
| Game Menu ↔ Upgrade Ship | 94,913 |
| Title ↔ Play Mode | 10,885 |
| Play Mode ↔ Episode | 1,534 |
| Episode ↔ Difficulty | 1,531 |
| Difficulty ↔ Game Menu | 83,417 |
| Game Menu ↔ Next Level | 96,821 |
| Upgrade category ↔ inventory | 118,429 |
| Game Menu ↔ Quit dialog | 81,044 |

全部低於專案的 180,000-cycle 靜態轉場門檻。作為對照，runtime
逐字縮放版本的 Game Menu ↔ Upgrade Ship 曾達 492,593 cycles 並
漏 239 個 VBlank；預烘 strip 已消除此回歸。

### 關卡路徑

Episode 2 與 Episode 4 Section 1 路線 smoke test 均通過：

- SRAM schema/pass：`TGRS / 3 / 1`
- 最終回到 Game Menu，選單音樂有效
- unknown visual、Sprite2 decode failure、Sprite2 cache drop、
  projectile cache drop：全部 0
- front-end／transition missed VBlank：全部 0

### 代表畫面

本機驗證截圖位於（不納入版本控制）：

- `temp/phase52_final_capture/game_menu_play_next.png`
- `temp/phase52_game_menu/game_menu_four_cubes_v2.png`
- `temp/phase52_game_menu/upgrade_ship.png`
- `temp/phase52_game_menu/quit_dialog.png`
- `temp/phase52_game_menu/next_level.png`

Game Menu 的 bar、格線、金額、飛船、Data Cube 與底部提示已在
240×160 最終畫面檢查，未再出現舊版縮放造成的斷格。
