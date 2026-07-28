# Tyrian GBA v42：Game Menu 前端呈現與效能重構定案

日期：2026-07-29  
狀態：已實作並通過 mGBA 壓力與畫面驗證

## 範圍邊界

本方案**只處理關卡外、偏靜態的前端／設定介面**：

- Game Menu；
- 首頁、Play Mode、Episode、Difficulty；
- Upgrade Ship 與其裝備子選單；
- Next Level；
- Quit confirmation；
- 日後性質相同的設定或資料選單。

本方案**不修改遊戲關卡內 renderer**。關卡內既有的 Mode 0 多圖層、
OAM、Sprite2 cache、30/60 Hz 節奏與 dynamic drop-frame 路徑視為目前
GBA 版本的最佳實作，繼續獨立維護。下文所有 Mode 4、bitmap、字型、
dirty rectangle 與 palette 討論都只指關卡外前端。

## 結論

偏靜態前端採用「**美術先縮放、文字最後以 GBA 原生像素疊加**」的
強化 Mode 4 架構：

1. PC 版 320×200 美術層保留 `x=0..299`，只裁右側 20 pixel，
   得到 300×200；左側不可裁，否則會吃掉側翼裝備的顯示範圍；
2. build time 將美術層縮成 240×160；
3. PIC 背景在 build time 生成 art-only base；船、裝備與導航等動態
   素材仍由 ROMFS 的原始 HDT／SHP／Sprite2 資料決定；
4. runtime 只組合必要區塊，並在最後疊上 240×160 原生細字型；
5. 選取、數值及動畫只更新 dirty rectangles，不再因一次按鍵重畫
   38,400-byte 全頁。

這保留 OpenTyrian 的資料、索引與流程權威；build 端轉換以完整通用
素材表為單位，不建立 per-level 特製資源。

## Gemini 諮詢後的核對

Gemini 支持：

- 美術層和文字層分離；
- build-time 預縮放；
- Mode 4 dirty rectangle；
- 5×7／6×8 proportional 原生字型；
- 用 ROM 容量換 runtime CPU。

但 Gemini 回覆中「Mode 4 bitmap 不支援 affine」並不正確。本地
mGBA renderer 的 Mode 3／4／5 都透過 BG2 affine 座標取樣。
真正限制是：

- Mode 4 的來源 framebuffer 固定為 240×160；
- 即使 BG2 affine 可縮放／旋轉，也不能在該 framebuffer 內放入
  300×200 來源；
- 300×200 硬體來源必須改成 affine tiled BG，通常要放在 512×512
  map；
- 單一 8bpp affine BG 只能引用 256 個 unique tiles，而既有前端
  畫面實測為 27～600、平均 342，複雜頁面無法穩定裝入；
- Mode 2 雙 affine BG 拼接會失去方便的文字 BG，還增加 tile
  allocator、palette、layer 與頁面切換複雜度。

因此不採 Mode 1／2 全畫面硬體縮放。build-time 的 300×200→240×160
會得到相同的固定取樣結果，且 runtime 成本為零。

## 已確認的星球錯色原因

星球的 `PGR`、`PAni`、`planetX/Y`、`PLANET_SHAPES` table 3 與
0-based graphic 索引均已和 OpenTyrian 對上，並不是素材索引錯誤。

真正原因是 palette 狀態漏移植：

- OpenTyrian 在 `MENU_FULL_GAME` 選擇 Next Level 時明確執行
  `newPal = 18`；
- 下一輪把 `palettes[newPal - 1]`，也就是 palette index 17，
  設成顯示 palette；
- GBA runtime 卻一直使用 `FRONTEND_FRAME_MENU_CHROME` 的
  picture 1 palette（palette index 0）。

直接證據：

- 導航格線使用 index 2；
- palette 0 的 index 2 是灰色 `(24,24,24)`，正是目前 GBA 截圖；
- palette 17 的 index 2 是綠色 `(0,28,0)`，正是 PC 參考畫面。

中心星球的白色彩色雜訊感也是同一批 8bpp index 被 palette 0
錯誤解讀，而不是 SHP RLE 指標損壞。v42 將加入獨立的 Next Level
palette 17 資產和 golden-frame index／RGB 驗證。

## 各頁面更新策略

### Game Menu

- 進入頁面時只組合一次：
  - 240×160 art-only base；
  - 玩家船、武器、側翼、護盾等 native stamps；
  - 所有 native text。
- 一般上下移動：
  - 還原舊列與新列；
  - 重畫兩列文字；
  - VBlank 只 DMA 兩個列區塊。
- 船型與金錢只在裝備／金錢真的改變後重建左側 preview。

### 首頁與進入遊戲前設定頁

- Title、Play Mode、Episode、Difficulty 各自使用 art-only base；
- 全部改用和 Game Menu 相同的 GBA 原生 5×7 proportional 字；
- 上下移動只還原、重畫舊列與新列，不切換整張預先烘焙選取畫面；
- Intro logo 圖與關卡內 renderer 完全不變。

### Upgrade Ship

- category 頁的船圖和底圖保留快取；
- category 移動只更新舊、新文字列；
- submenu 移動只更新：
  - 舊、新 item rows；
  - item preview rectangle；
  - cost、cash、power rectangle；
- power 調整不重畫 item list 或整頁；
- 清單採 4×6、5-pixel 字格；超長原始名稱才自動降為
  3×5、4-pixel 字格，避免截字；
- 8-pixel 寬測試版會截斷 `PULSE-CANNON` 且讓 COST／OWN 擁擠，
  已明確排除；
- 圖示、名稱、價格、OWN、DONE 整組向左偏移 6 GBA pixels。

### Next Level

- 進入時載入 palette 17，建立一次 chrome、標題與右側文字；
- 左側導航區維持獨立 native 240×160 座標：
  - grid 用整數 native 座標直接畫；
  - planet／dot 使用 build-time native stamps；
  - camera、route、動畫仍逐行沿用 OpenTyrian 規則；
- camera 移動時只重建並 DMA 左側導航 rectangle；
- 選項變更只重畫右側舊、新文字列和必要的導航 rectangle；
- 星球每四 tick、route dot 每六 tick 才更新，沒有動畫變化就不搬資料。

## 原生細字型

- 一般介面為 6×8 cell、主要筆畫 5×7；
- proportional advance，窄字如 `I`, `L`, `1`, punctuation 使用
  2～4 pixels；
- Upgrade inventory 使用自適應 4×6／3×5 專用字；
- 單 pixel 筆畫，不縮放 PC glyph；
- 一般文字使用 1-pixel 右下陰影；小型數值可不加陰影；
- 文字顏色不是硬覆寫 palette slot，而是 build time 對每種頁面
  palette 選取最接近的前景／選取／disabled／shadow index，避免
  汙染原始 8bpp 美術；
- 至少包含大小寫英文字母、數字與目前 HDT 文案使用的標點。

## 資源格式

新增通用 frontend stamp atlas：

- key：`(source kind, table/bank, graphic, scale profile)`；
- 像素保留原始 8bpp palette index；
- 透明 pixel 不 bake 成頁面顏色；
- row span metadata 記錄每列連續 opaque 範圍，runtime 可用
  16／32-bit copy，不做 per-pixel division；
- 300×200 crop profile 的座標：
  - `x_gba = x_pc * 4 / 5`
  - `y_gba = y_pc * 4 / 5`
- build 時驗證原始 SHP decode 與 stamp 的透明遮罩、邊界和 checksum。

## 驗收門檻

- 一次按鍵不得觸發 full-frame compose 或 full-frame DMA；
- Game Menu 一般移動：dirty DMA 目標不超過 3 KiB；
- Upgrade category 移動：目標不超過 4 KiB；
- Upgrade item preview 更新：目標不超過 20 KiB；
- Next Level 一次動畫更新：只允許導航 rectangle，目標不超過
  16 KiB；
- Game Menu 一般游標移動的 runtime frontend SHP／Sprite2 decode
  次數為 0；
- 600-frame 選單壓力測試新增 missed VBlank 為 0；
- full redraw counter 只在進入新頁面時增加；
- Next Level palette 必須為 source palette index 17；
- PC reference 經同一 crop/scale 後，layout mask／planet silhouette
  必須一致；RGB 差異只允許 GBA RGB555 量化。

## 實測結果

`frontend-menu-stress` 在 mGBA 連續切換 Game Menu 600 次：

- 601 VBlanks 完成 600 次輸入；
- missed VBlank：0；
- full redraw：0；
- dirty commits：600；
- runtime SHP decode：0；
- runtime Sprite2 decode：0；
- dirty DMA：1,296,000 bytes。

若每次仍搬整張 38,400-byte Mode 4 frame，總量會是
23,040,000 bytes；本版減少 94.375%。畫面擷取保存在
`temp/frontend_v42_capture/`，其中 `state13_adaptive_left6.png`
是正式採用的 Upgrade inventory 字型，`state13_wide8_crop_right.png`
是被排除的 8-pixel 寬比較版。

完整 `high/normal` 回歸亦通過：

- release ROM：14,721,408 bytes；
- SHA-256：
  `847cd70915c74779854846d25de513042eb528c3944812de4f0fbb973587cae8`；
- release EWRAM 餘量：50,376 bytes；
- release IWRAM 餘量：8,400 bytes；
- ROMFS matrix、Episode 1～4 route、四關 campaign、Arcade、
  death、Jukebox、Demo 均通過；
- build artifact policy 為 release-only，`build/` 僅保留最新正式
  GBA ROM。
