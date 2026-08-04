# Tyrian GBA Load / Save 雙流程來源碼對齊（v76）

日期：2026-08-03

## 問題根因

舊版把兩個不同的 PC 流程合併為同一套 GBA 存檔瀏覽器，並把已含文字的畫面擷取當成背景，再於 runtime 疊畫選單文字。因此造成背景、排版、標題與功能脈絡都不正確，並出現重複文字。

原版實際上有兩個不同入口，而且只共用存檔資料與 `JE_operation()`：

1. 程式首頁的 **Load Game**：`vendor/opentyrian/src/mainint.c::JE_loadScreen()`。
2. 關卡間 **Game Menu > Options > Load / Save**：`vendor/opentyrian/src/game_menu.c::MENU_LOAD_SAVE`。

## 首頁 Load Game

- 原始背景：PIC 2。
- 原始標題：HDT `miscText[38]`，即 `One Player Saved Games`。
- 一次列出 11 個單人存檔槽，欄位為名稱、`Last Level` 與 `Episode`。
- 最後一列使用 HDT `miscText[33]`：`Exit to Main Menu`。
- GBA 不支援雙人模式，因此只保留原版單人頁；不顯示 PC 版的 1P/2P 翻頁控制。
- 背景只保存 PIC 2 的圖像內容；所有會變動的存檔文字均由 runtime 繪製，不再使用帶字截圖。

## Game Menu > Options > Load / Save

- 原始背景：PIC 1 的商店／飛船 Game Menu 畫面，不是 PIC 2。
- 原始標題：HDT `menuInt[3][performSave + 1]`，分別為 `Load` 或 `Save`。
- 原版欄位位置來自 `game_menu.c` 的 `MENU_LOAD_SAVE` 分支：名稱、關卡名稱與 `Ep`。
- 最後一列使用 HDT `miscText[5]`：`Exit to Game Menu`。
- 左側保留目前飛船、金額、護甲與護盾等 Game Menu 狀態；右側才是 Load/Save 存檔槽。
- 240x160 版本把原始 320x200 介面依既有靜態選單策略縮放／裁切，但資料來源與流程保持原版語意。

## Save 名稱輸入視窗

- 依 `mainint.c::JE_operation()`：在 Options Save 畫面上加陰影，再疊上 `OPTION_SHAPES` 第 35 號訊息框。
- 關卡名稱、存檔名稱與游標由 runtime 繪製。
- GBA 沒有鍵盤，改用方向鍵選字元與控制鍵切換大小寫；確認後仍寫入同一套 SRAM 存檔格式。
- 視窗底圖不再切換成首頁 PIC 2，也不再使用含既有文字的整張截圖。

## 文字與素材原則

- 固定字串直接從 ROMFS 的 Tyrian HDT 載入：`miscText`、`miscTextB`、`menuInt`。
- 固定圖像由原始 PIC／SHP 解碼建立無文字底圖或密集 overlay。
- 存檔名稱、章節、關卡、選取色與狀態全部由 runtime 根據 SRAM 真實資料產生。
- `Exit to Main Menu` 使用可清楚區分大小寫與 M/N 的字型路徑，修正舊版視覺上呈現為 `Exit to Nain Menu` 的問題。

## 驗證

- 自動測試分別進入首頁 Load、Options Load 與 Options Save，驗證 context、面板選擇、取消返回及 SRAM 寫入／讀回。
- 目前 Save autotest 結果：`Pass=1`、`Failures=0x0000`、11 個存檔槽、32 KiB SRAM。
- 畫面比對輸出位於 `temp/load_save_source_parity_20260803/`，包含首頁 Load、Options Load、Options Save 與 Save 名稱輸入四個狀態。
