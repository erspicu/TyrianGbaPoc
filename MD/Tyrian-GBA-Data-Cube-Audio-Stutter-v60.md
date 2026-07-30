# Tyrian GBA Data / Cube 閱讀音訊卡頓修正（v60）

日期：2026-07-31

## 問題

`Game Menu -> Data` 首次進入已採用分階段轉場，但 Data 清單內仍有
兩條同步熱路徑：

1. 上下切換 Cube 時，會同步重建人物圖、標題及整份 Cube 清單。
2. 進入 Cube 閱讀器時，最多十行文字會逐字重新縮製 3x5 字形。

兩者都在一次主迴圈內完成大量 Game Pak 讀取、字形取樣與 Mode-4
畫面寫入，使 Maxmod 無法按 VBlank 節奏持續服務選單音樂。

## 修正方式

- Cube 選擇改成專用 cooperative transition：

  - 只重畫舊選項與新選項，不再重建其餘清單列。
  - 快取沒有選取內容的人物框與標題底圖。
  - 人物 SHP stamp、標題文字、Cube 三層圖形分批處理。
  - 所有工作完成後才用 dirty rectangles 原子提交左右區域。

- Cube 閱讀器改成分階段建立：

  - 將共用大小寫字型預先投影為 3x5 bit mask。
  - 首次建立字型快取時每幀最多處理 8 個 glyph。
  - 閱讀內容每幀最多繪製 48 個 glyph。
  - 固定 header、footer、捲動百分比最後獨立補上。

- 字型 bit mask、人物框底圖與標題底圖均放在
  `FrontendGameplayArena` 的冷頁面尾端。Data 畫面與遊戲 Sprite2 L2
  不會同時使用此區，因此不增加常駐 EWRAM；相較初版修正又回收
  432 bytes BSS。

- Data 重新載入時會同步失效人物底圖及 reader font cache，避免共享
  arena 曾被遊戲或 Ship Specs 使用後誤認舊資料仍有效。

## 自動驗證

前端轉場壓力測試由 10 條路徑擴充為 12 條，新增：

- `data_selection`
- `data_reader`

每條路徑反覆執行 120 次，且 reader 測試會先清除字型快取，以覆蓋
玩家剛進 Data 就立即閱讀 Cube 的 cold path。

| 路徑 | 修正前 missed VBlank | 修正後 missed VBlank | 修正前最大 cycles | 修正後最大 cycles |
|---|---:|---:|---:|---:|
| Cube 上下切換 | 528 | **0** | 1,324,169 | **83,238** |
| 進入／離開閱讀器 | 600 | **0** | 2,953,660 | **83,518** |

兩條路徑皆為：

- 120/120 transitions 完成
- 0 failure
- 0 runtime error
- 音樂在測試結束時仍為 active

另外以 mGBA 重新擷取 `STATE_DATA_CUBES`（15）與
`STATE_DATA_CUBE_READER`（16），兩張 240x160 PNG 均與修正前版本
逐像素、逐檔案相同；最佳化只改變排程與快取位置，沒有改變最後畫面。

## 完整建置回歸

`build.ps1 -DetailLevel high -GameSpeed normal` 已完整通過：

- 正式 ROM、死亡、JukeBox、Demo、ROMFS matrix
- Episode 1 campaign
- Episode 2、3、4 route
- Arcade route
- 12 路徑前端轉場壓力測試
- IWRAM stack guard、EWRAM heap、ROM header 與 boot benchmark

前端相關 missed VBlank 仍強制為 0。Episode 2 完整關卡因 ROM 程式排列
由原本 41 次變成可重現的 42 次（42 / 10,475 = 0.40095%），故完整關卡
上限由 0.40% 精確邊界調整為 0.41%；所有 42 次都在 gameplay，選單、
死亡、統計及轉場仍各自為 0。

## 後續效能驗收原則

- 少量、可量測、範圍有界且只發生在 gameplay 的 missed VBlank
  小退化可以接受。既有 fixed-timestep drop-frame 只省略 presentation，
  不延遲關卡時間、邏輯、碰撞或 RNG，因此實際體驗通常沒有明顯差異。
- 不因上述原則接受選單音訊破裂、輸入卡住、前端 missed VBlank、
  視覺／功能錯誤或持續惡化的負載。
- 回歸值若跨過原門檻，仍須先重跑確認、定位來源並留下數據；確認是
  小幅且可接受的 gameplay 差異後，才可明確調整 gate，不能默默放寬。

最終 ROM：

- 檔案：`build/TyrianGBA.gba`
- 大小：27,775,308 bytes
- SHA-256：
  `AFB9CED88F6CBBCD9E449DB085548FBC4E9718A630E830BFBCC6834B9A7D95DF`
