# TyrianGbaPoc v56：關卡色盤、柔性鏡頭與 Data／Ship Specs 技術分析

日期：2026-07-30  
分支：`palette-camera-data-v56`  
分析範圍：`ref/1.png`～`ref/6.png`、GBA runtime、build-time
palette trainer，以及 OpenTyrian 原始流程。

## 結論摘要

1. **Episode 4 第一關的紅框色差不是目前技術極限。**
   v55 雖然已用 62 關真實資料訓練，但同一個 shape profile 仍共用
   調色盤。Episode 4 第一關使用的 `z` profile 同時服務 32 關，
   不同材質會彼此爭用少量 4bpp palette banks。以第一關自身資料
   重訓後，OKLab 加權誤差可再下降約 **43.49%**，CIEDE2000
   加權誤差可再下降約 **17.04%**；最差的綠色 tile 個案下降約
   **98%**。因此紅框內植物周圍偏亮／偏褐的方塊主要是
   **跨關卡共用 palette profile 的量化衝突**，不是 GBA 已無法改善。
2. **不建議每五秒在 gameplay 中重新訓練與整批換色盤。**
   目前每個背景 8×8 tile 的 4bpp nibble 已綁定當時選定的 palette
   bank；換 palette 會使已快取的 tile 一起變色，並需要失效、重建
   tile cache 和 map entries，容易產生閃色、爆幀和明顯跳色。
3. 最適方案是 **build 時為每一關自動訓練，入關前只選取該關結果**。
   訓練資料仍直接來自 ROMFS 內的 stock LVL／MAP／SHP，不需要維護
   手工的每關轉換表，也不改關卡內容。runtime 的切換只是複製
   palette、選 LUT 與小型 bank table，遠低於一秒，不需要刻意延遲。
4. Episode 1 第一關上下邊界的直條破圖是 **柔性鏡頭與背景 ring
   cache 不同步的 bug**，不是硬體極限。畫面 VOFS 已上下移動，但
   cache 還只保留未偏移的 21 列；靠近上下邊界時 PPU 讀到尚未安裝
   或已釋放的 tile row。
5. Data 少了四顆 cube 是 **流程翻寫缺漏**。PC 的 `JE_loadMap()`
   在進 Game Menu 前便執行 `]?`、`]!`、`]+`，建立 `cubeList` /
   `cubeMax`；GBA 的 map parser 目前忽略這三種 directive，卻在真正
   進關之後的 level parser 才套用，時機已太晚。
6. Data／Ship Specs 的卡頓不是功能本身超出 GBA，而是目前在單一
   input frame 內同步解密、排版、畫滿 38,400 bytes Mode 4 frame，
   離開時又同步重建 Game Menu。應改成可讓 Maxmod 持續更新的分段
   transition job。

## 1. Episode 4 第一關色盤實測

### 1.1 現況限制

PC 資料為 256 色索引圖；GBA gameplay 使用 Mode 0 的 4bpp text
background，一塊 8×8 tile 只能選一個 16 色 bank，其中 index 0
還保留透明／基底用途，實際只有 15 個非零顏色。

v55 的 trainer 已經重建真實 runtime tile key 與出現頻率，但最後
仍按五個 shape profiles 共用結果。Episode 4 第一關所屬的 `z`
profile 服務 32 關：

- 10,872 個 unique runtime keys；
- 94 個 active hue masks；
- 既有保護規則下，profile-wide 訓練只有很少的自由 banks 可同時
  照顧 32 關的石材、植被、金屬與地面。

這就是 `ref/1.png`、`ref/2.png` 紅框內「植物本體還算接近，但
8×8 tile 方塊底色突兀」的主要成因：同一 hue mask 在跨關卡資料中
被較高曝光量的其他材質主導。

### 1.2 以真實 Episode 4／Level 1 資料重訓

本次只做離線診斷，沒有手工挑圖或修改權重：

| 項目 | 結果 |
|---|---:|
| 非空白 map-tile occurrences | 125,768 |
| unique runtime keys | 1,957 |
| active hue masks | 43 |
| 可安全重訓 banks | 6 |
| 變更 assignment 的 active masks | 9 |
| v55 OKLab weighted MSE | 0.000505506056 |
| 逐關 OKLab weighted MSE | 0.000285683826 |
| OKLab 改善 | **43.485578%** |
| v55 CIEDE2000 weighted MSE | 2.489587012 |
| 逐關 CIEDE2000 weighted MSE | 2.065267119 |
| CIEDE2000 改善 | **17.043786%** |
| regression keys | **0** |

最差個案 `key=0x00c000e8, mask=0x0406` 在該關出現 235 次：

- v55 OKLab error：`0.011719493`
- 逐關結果：`0.000202549`
- 約改善 98%；v55 將深綠材質映成褐色，逐關結果恢復為深綠。

診斷預覽已產生在
`temp/v56_ep4_level1_worst_palette.png`，每列依序為 PC source、
v55 與逐關訓練結果。

### 1.3 建議實作

採用以下兩層資料：

1. 一份全域 `65536 × 1 byte` 的 `hue mask -> active-mask ID`
   table；stock 62 關實際只有約 202 種 active masks。
2. 每關保存：
   - 約 202 bytes 的 `active-mask ID -> bank`；
   - 512 bytes GBA palette；
   - 4,096 bytes `bank × source index -> nearest nibble`。

62 關總增量約低於 0.4 MiB，遠小於 GBA 32 MiB ROM 上限，也避免
直接保存 `62 × 64 KiB` dense mask tables。遇到未收錄／修改過的
關卡，仍 fallback 到 v55 shape-profile adapter。

入關時只需：

- 用 episode + LVL section 選 variant；
- DMA 512-byte palette；
- 切換兩個 LUT 指標；
- flush 該背景 pattern cache。

這個動作不需要一秒。若希望視覺上掩蓋 cache warm-up，可把它放在
既有 fade-to-black／loading transition 中。

### 1.4 為何不採 gameplay 每五秒動態訓練

- ARM7TDMI 16.78 MHz 不適合在 gameplay 做 OKLab／CIEDE2000
  clustering；
- 訓練前還要遍歷後續地圖與 shape；
- palette bank 一改，舊 4bpp patterns 的 nibble 意義也改變；
- 必須整批失效並重建 cache，可能造成同屏 tile 瞬間換色；
- 同一時刻仍只有硬體 16 banks，動態訓練不會解除單 tile 15 色的
  上限。

未來若極少數超長關卡仍有區段衝突，可以在 build 時產生
segment-specific variants，只在淡出、場景遮蔽或明確 section
boundary 切換；不應在可見 gameplay 中週期性重訓。

## 2. 柔性鏡頭上下邊界破圖

### 2.1 根因

目前柔性鏡頭把 264×184 source viewport 裁成 240×160，Y 可在
source viewport 內由 0 移到 24。`source_background_vofs()` 已把
這個偏移加入 BG VOFS。

然而背景 cache／row scheduler 仍依未偏移的 background scroll
保留固定 21 個 tile rows，且只在背景自身跨越 8-pixel 邊界時載入
新 row。玩家移動造成 camera offset 跨 tile 邊界時：

1. 硬體 VOFS 開始取更上方或更下方 row；
2. scheduler 沒有收到背景 scroll crossing；
3. ring map 該 row 尚未安裝，或 tile patterns 已被 cache 釋放；
4. 畫面出現 `ref/3.png`、`ref/4.png` 的垂直垃圾條紋。

### 2.2 修正方向

- 以實際 presentation scroll
  `(layer presentation scroll + signed camera offset)` 計算首列；
- row retention 和 visible-window residency 都使用相同數值；
- camera crossing 也能排程上／下方新 row，不再只看 map scroll；
- 負座標使用 signed clamp，避免轉成 unsigned 後 wrap；
- 新增極上／極下掃描測試，逐 frame 驗證 PPU 會取樣的 21 rows
  全部 resident。

若同一 tick 有 map scroll 與 camera scroll，只有在 incoming row
已可於該次 VBlank commit 時才套用新 VOFS；否則先暫停一小步 camera
offset，避免顯示未準備的列。

## 3. Data：四顆 cube 與閱讀功能

### 3.1 PC 正確流程

OpenTyrian `JE_loadMap()` 在顯示 Game Menu 之前解析：

- `]?`：載入 cube ID list，並限制 `cubeMax`；
- `]!`：直接指定 `cubeMax`；
- `]+`：增加 `cubeMax`。

因此 `ref/5.png` 的四顆藍色 cube 是實際 campaign state，不是固定
裝飾。進 Data 後，四筆清單應如 `ref/6.png`，可逐筆進入閱讀。

### 3.2 GBA 缺漏

- `OtEpisodeLevel` 已能記錄 cube operations；
- `OtEpisodeMap` 沒有對應欄位；
- map resolver 忽略 `]?`、`]!`、`]+`；
- cube state 直到離開 Game Menu、真正進關時才套用；
- 因而 Game Menu 和 Data 畫面看到 0 顆 cube。

修正應把同一套 directive 解析加入 map flow，並在 Game Menu
初始化前依原始順序套用。直接進關的相同 section 要避免重複套用。

### 3.3 現有 Data 功能與待補項

現有 GBA 程式已有：

- 從 `cubetxtN.dat` 解密讀取 stock cube；
- 四筆清單、cube glow、人物圖、標題與內文；
- 上下選擇、進入閱讀、返回。

仍需補正：

- 真正 map-time `cubeList/cubeMax`；
- 保持 PC 的四筆真實內容與順序；
- 把單 frame 同步解密／排版改成逐 cube、逐 phase；
- 內文滾動目前是整行跳動；PC 是 pixel-based smooth scroll，應按
  240×160 版面保留平滑捲動；
- 進出都走 staged transition，不能在按鍵當幀重畫完整畫面。

## 4. Ship Specs 缺漏與效能

現有 GBA 畫面會從 HDT／stock shape 讀取：

- 真實船名；
- 兩段 ship description；
- big ship graphic；
- 綠色網格與綠化外框效果。

但與 PC 相比仍缺：

- `S_SPRING` 進場音效的完整時序；
- `JE_scaleInPicture()` 的中央展開縮放；
- 分段準備，因此進入時有明顯音訊停頓；
- 離開時未走既有 staged Game Menu transition。

GBA Mode 4 的 BG2 本身是 affine background。最佳做法不是 CPU
每幀縮放 38,400 pixels，而是先分段組成最終畫面，接著用 BG2PA /
BG2PD / BG2X / BG2Y 做硬體中央展開；既保留 PC 的視覺意圖，也幾乎
不增加每幀 CPU 成本。

## 5. 預定實施與驗證

1. 產生 62 關 compact per-level palette variants，保留 profile
   fallback 與 asset report。
2. 修正 camera-aware background visible-window scheduler。
3. 將 map-time cube directives 逐行補入 parser／campaign flow。
4. Data 和 Ship Specs 改為可中斷的 staged transitions；Data 恢復
   四顆 cube 與可閱讀內容，Ship Specs 使用 BG2 affine scale-in。
5. 自動測試：
   - Episode 4 第一關 palette metrics／截圖；
   - camera 上下極限 resident-row invariant；
   - Episode map cube directive parity；
   - Data 四筆清單、reader navigation；
   - Data／Ship Specs 進出 missed-VBlank 與 audio transition；
   - 最終 ROMFS、EWRAM、IWRAM、32 MiB ROM gate。

本分析的判定是：三類問題都有可行改善；其中柔性鏡頭與四顆 Data
cube 是明確 bug／移植缺漏，色盤則適合由 profile-wide 提升成
level-specific build-time training，而不是在可見 gameplay 中動態
重訓。
