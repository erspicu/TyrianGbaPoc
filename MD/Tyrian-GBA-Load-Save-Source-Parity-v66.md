# Tyrian GBA Load／Save 單人版來源對照（v66）

## 結論

首頁 `Load Game` 與 Game Menu → Options 的 `Load`／`Save` 已共用同一套
真實 SRAM 資料及操作流程。畫面不再借用 Game Menu 右半欄，而是依
OpenTyrian `JE_loadScreen()` 的 `PIC 2` 全畫面設計適配成 240×160。

GBA 版刻意移除 PC 的 1P／2P 分頁箭頭與說明，只保留單人遊戲需要的：

- 11 個存檔槽；
- 玩家名稱；
- `Last level`；
- `Episode`；
- `Exit to Main Menu`；
- 首頁 Load、Options Load、Options Save 的正確返回路徑。

## 來源對照

主要來源是 `vendor/opentyrian/src/mainint.c` 的 `JE_loadScreen()` 與
`JE_operation()`：

- PC 背景：`JE_loadPic(..., 2, false)`；
- PC 欄位起點：名稱 x=10、Last level x=120、Episode x=250；
- PC 槽位：y=30、間距 13，共 11 槽，再加離開列；
- 空槽：`EMPTY SLOT` 與 `Last level -----`；
- 已使用槽：真實名稱、關卡名稱與 Episode；
- Load 只接受已使用槽；Save 允許空槽並進入 14 字元名稱編輯。

GBA 使用等比例座標作起點後再微調欄寬，避免 240 像素下的 Episode
裁字；背景與調色盤仍直接來自原始 `PIC 2`，沒有建立仿製背景。

## 缺字修正

舊版命名提示中的 `[`、`]`、`+`、`^` 未收錄於 mixed-case 字庫，runtime
依設計退回 `?`。v66 將四個字形加入同一份 6×8 authored font，C runtime
與 build-time renderer 使用完全相同順序；`[player]`、`R+Up/Down` 與游標
現在都會顯示正確字形。

## 效能策略

- 兩張槽位頁與一張名稱頁在 build 時以最終 240×160 解析度烘焙。
- 進頁時直接由 ROM DMA 靜態畫面，不先複製整張到 EWRAM。
- 選取列先在下一個 VBlank 修補；已使用槽每 VBlank 最多補三列。
- 名稱編輯只更新 23 列高的名稱區，不重畫整頁。
- 舊的右欄 Load／Save panel、四條無用 help strip 及 9.9 KiB 動態列快取
  已移除，避免 ROM 資源重複並降低 EWRAM 壓力。

transition stress（120 次／路徑）的結果：

| 路徑 | missed VBlank | 最大 CPU cycles |
|---|---:|---:|
| Options ↔ Slots | 0 | 118,568 |
| Slots ↔ Save Name | 0 | 1,731 |
| Save Name 編輯 | 0 | 74,600 |

release BSS 為 246,900 bytes；改版前約 260 KiB，主要差異是移除舊列快取。

## 驗證

- `TGSV` SRAM 自動測試：schema 1、pass 1、failures 0、11 槽。
- `TGFA` transition stress：三條 Load／Save 相關路徑均為 0 missed VBlank。
- mGBA runtime capture：首頁 Load、名稱輸入、首頁 build label 均以實際
  Mode 4 raster 驗證，而非只檢查 Python 預覽。
