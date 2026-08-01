# Episode 4 UNDERDELI 高速背景破圖：根因與修復

日期：2026-08-01
狀態：已修復並用使用者問題存檔完成整關驗證

## 問題存檔與路由

- 輸入：`C:\ai_project\AprTyrianNes\問題log\TyrianGBA-破圖很嚴重.sav`
- SHA-256：`d04cfc5e187e7538acaca3eb2ca69070bd2a0d2e00ecfe3538d5a935d5aa2969`
- 兩個 SRAM bank 的 CRC 都有效，active bank sequence 為 28。
- Slot 1 是 Full Game、Episode 4、section 46 `UNDERDELI`。
- 路由正確解析到 `tyrian4.lvl` 的 physical section 19、song 35、1288 個事件，以及 `Y/y` shape profile。

因此，問題不是壞存檔、章節索引錯誤、ROMFS 指錯檔案或 shape bank 選錯。

## 高速是否為 BUG

不是遊戲時間軸的加速 BUG。原始 LVL 在 time 0 明確設定：

- layer 1：`backMove = 1`
- layer 2：`backMove2 = 20`
- layer 3：`backMove3 = 0`

後續事件還會讓 layer 2 在 16～22 pixel／logic tick 間變化。OpenTyrian 的 `vendor/opentyrian/src/tyrian2.c` 也直接把 `eventdat2`／`eventdat3` 寫入 `backMove2`／`backMove3`，再由 `backgrnd.c` 更新該層位置。

所以玩家感受到的高速主要是原版刻意設計的第二層視差；第一層、事件時鐘、敵人、碰撞與音樂並沒有被 GBA 版錯誤加速。

## 真正根因

GBA 版以三個 4bpp text BG 和 32-row circular tilemap 呈現 PC 背景。舊串流器每層只有一個 pending row：

1. 一個 8-pixel tile row 是最小更新單位。
2. UNDERDELI 的 layer 2 每 tick 移動 20～22 pixel，一次會露出 2～3 個新 row。
3. 舊程式發現可見視窗缺超過一列時拒絕排程，但 VOFS 和邏輯 scroll 仍繼續前進。
4. PPU 因而讀到 32-row ring 中尚未替換的舊資料，形成整條橫向接縫、舊地形殘片與上下區塊錯置。

這是 GBA 轉譯層的 producer/consumer 合約錯誤，不是 GBA 硬體本身不能顯示高速捲動。Drop-frame 只能省略畫面，無法讓「一次只能生一列」的 producer 突然供應三列，因此原機制無法掩蓋這個 BUG。

## 修復設計

### 1. 完整可見視窗批次

- 單一 pending row 改成每層最多 21 列的 bounded batch。
- 每次呈現計算最新 240×160 視窗真正缺少的所有 row。
- character tile、tilemap row 與 VOFS 在同一個 VBlank 邊界發布。
- 新視窗沒有完整準備好時不發布新 VOFS，因此不再讓 PPU 看到 stale ring row。

### 2. 邏輯與呈現合併

- 邏輯 tick 只推進 PC 權威 scroll／camera 狀態。
- Drop-frame 發生時不建立永遠不會顯示的中間 row。
- 真正 render 時直接 materialize 最新可見視窗，保留 PC 關卡時序但省去無效工作。

### 3. 預取與 cache 所有權

- `ring_source_row` 表示 EWRAM 已準備內容；新增 `ring_vram_source_row` 表示 PPU 實際可見的 VRAM 內容，避免把「預取完成」誤認成「已發布」。
- 依 layer speed 預取最多七列，包含兩個 pending logic tick 的高速需求。
- dirty character tile 由固定長度 upload list 改為 cache-capacity bitset，能跨多列去重且不會溢位。
- palette/cache reference 只有在新呈現視窗接管時才釋放，避免覆寫仍在顯示的圖塊。

### 4. ARM7 hot path

- row key 將每列重複的 Y 除法、餘數與 shape lookup 移到 cursor 初始化，只保留每三個 tile 的 X 推進。
- row build／release 與 cursor 放入 IWRAM、ARM state。
- 預取以當幀實際剩餘 cycle 動態縮減；正常 idle frame 仍用完整預算，音訊或其他工作已偏重時則提前停止，避免預取自己製造 missed VBlank。

## Gemini 3.1 Pro 複核

Gemini 在取得原始事件速度、32-row ring、舊 pending-row 規則和實測截圖後，也判定根因是高速 layer 需要多列但 producer 只有一列，並建議：

1. 邏輯座標與呈現座標分離。
2. 合併到最新可見視窗，不建立被 drop 的中間列。
3. 所有 row 完成後才發布 VOFS。
4. 視覺層限速只作最後備援。

採用了前三項方向；沒有直接照用它估計的「11 個安全隱藏 row」，因為本專案還有柔性鏡頭保留區，實際 ownership 必須用 32-row ring、refcount 與可見／預取視窗共同約束。

## 實測結果

測試條件：問題存檔、Episode 4 section 46、Detail High、Game Speed Normal、完整路線至離關。

| 指標 | 舊版 | 最終修復版 |
|---|---:|---:|
| logic updates | 4227 | 4227 |
| display frames | 7319 | 7319 |
| missed VBlank | 419（5.72%） | 637（8.70%） |
| background rows committed | 0 | 2586 |
| background stream drops | 0 | **0** |
| rows prepared by idle prefetch | 0 | 2132 |
| rows built synchronously | 0 | 456 |
| max retained rows | 舊單列模型 | 27／32 |
| final level position | 7127 | 7127 |

舊版的 missed VBlank 較少，是因為它直接拒絕了高速層需要的 row 工作，代價就是嚴重破圖；不能把該數字視為正確畫面的效能基準。最終版完成所有 2586 個必要 row 且沒有 stream drop。問題存檔本身含高階武器與 sidekick，另有大量 projectile-cache 壓力；那會影響子彈呈現負荷，但不是背景破圖原因。

效能 instrumentation 最壞配置仍保有：

- IWRAM：`__iheap_start=0x030072B0` 到 user stack `0x03007F00`，3152 bytes。
- EWRAM：`__eheap_start=0x0203D554`，剩餘約 10.7 KiB。

## 視覺證據

- 修正前 position 1000：`temp/issue_section46_capture_before_series/frame_p1000.png`
- 修正後 position 1000：`temp/issue_section46_final_p1000/frame_p1000.png`

修正前可見明顯水平舊列接縫；修正後整個 240×160 水面連續一致。

## 最終速度決策

目前不啟用 GBA 專用 map-speed cap：

- 高速是原版 authored parallax，不是錯誤的遊戲時鐘。
- 完整高速已能維持 `stream_drops=0`，視覺破圖消失。
- 現有正式 drop-frame／wall-clock logic 會在重負荷時犧牲呈現 frame，而不改變敵人、碰撞、音樂與關卡節奏。

若未來在實機或 flash cart 仍出現不可接受負荷，備援應只讓 layer 2 的「呈現座標」漸進追上權威座標，不能降低整體 level position 或事件時鐘；否則會改變 PC 關卡行為。
