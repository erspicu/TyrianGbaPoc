# Tyrian GBA v22：Mode 4 選單呈現與局部更新

## 問題

先前的選單雖然直接讀取 ROMFS 內的 `tyrian.pic`、`tyrian.shp`、
`palette.dat` 與 `tyrian.hdt`，但每次游標移動都會在 GBA 上執行：

1. PIC RLE 解碼；
2. 320×200 至 240×160 的逐像素縮放；
3. 每個字元重新搜尋及解碼 SHP；
4. 重畫整張 16-bit Mode 3 framebuffer。

因此成本與按鍵次數直接相關。這不是 GBA 選單本身的硬體極限，而是把
PC 的資源載入工作放在互動路徑所造成的架構問題。

## Mode 4 與 TILE 的實測選擇

`tools/build_assets.py` 現在於建置期直接解析原始 Tyrian PIC、SHP、HDT
及 VGA palette，產生 25 張 240×160、8-bit indexed Mode 4 畫面。

對全部畫面做 8×8 tile 稽核後：

- 每張畫面共有 600 個 tile 位置；
- 單張獨立 tile 數為 27 至 600；
- 平均為 342.24；
- 全部狀態合計有 2,950 個獨立 tile；
- 若每個狀態使用自己的 8bpp tile bank 與 tilemap，成本為
  2,928 至 39,600 bytes；
- Mode 4 固定為 38,400 bytes。

最複雜的 logo 與標題畫面幾乎沒有 tile 重複，TILE 模式最壞情況反而
比 Mode 4 略大。若改用 4bpp tiles，還必須把原始 256 色畫面重新分割
成多組 16 色 palette，會降低畫面一致性並增加資產維護成本。

因此 v22 採用：

- 靜態 PC 選單：Mode 4；
- 遊戲關卡：原本的 Mode 0 三層 tile 背景；
- 進入新選單：Mode 4 hidden page 整頁 DMA，VBlank page flip；
- 同一選單移動游標：只更新舊選項與新選項的兩條文字區域；
- 統計數字：建置期轉好的小字形，疊到 38.4 KB EWRAM scratch frame；
- 無效按鍵：不重畫、不搬資料。

## 資產配置

`res/frontend_frames.bin` 包含：

- 2 張 intro logo；
- 2 張 Title 選取狀態；
- 2 張 Play Mode；
- 4 張 Episode；
- 3 張 Difficulty；
- 6 張 Game Menu；
- 2 張 Next Level；
- 3 張 level stats；
- 1 張 Game Over。

總計：

- frame：960,000 bytes；
- palette：12,800 bytes；
- 動態數字／百分比字形：704 bytes；
- 合計：973,504 bytes。

若把所有 frame 用 zlib 壓縮，可縮至 183,606 bytes，但每次切換將重新
引入解壓成本。現有 ROM 只有 11,759,328 bytes，約為 32 MiB ROM
視窗的 35.05%，所以本版選擇 raw frame，以容量換取固定且低的互動成本。

## 更新成本

新畫面切換一次搬移：

- 38,400-byte frame；
- 512-byte palette。

游標移動不切 page，只在 VBlank 從目標預算圖取回兩個選項列：

- Title、Play Mode、Episode、Difficulty：最壞約 5,760 bytes；
- Game Menu、Next Level：約 2,160 bytes。

這些區域包含原本與新選項的完整背景，因此不需在 GBA 上重跑字型混色，
也不會留下前一個高亮狀態。

## 驗證

### 600-frame 游標壓力測試

測試 ROM 在 Title 畫面連續 600 幀、每幀切換一次選項：

- 測試完成 600 次；
- 最終畫面與目標預算 frame 逐像素一致；
- 局部更新測得 89 個 delayed VBlank；
- 完全不重畫的同長度基準也為 89；
- 因選單局部更新新增的 delayed VBlank 為 0。

這個 60 次／秒的切換速度遠高於實際方向鍵輸入頻率。

### 完整流程回歸

`build.ps1 -KeepIntermediates` 驗證結果：

- telemetry schema 20，`pass=1`；
- intro → Title → Play Mode → Episode → Difficulty → Game Menu →
  Next Level → 第一關 → stats → Game Menu；
- 最終 state 7（Game Menu）；
- source event cursor 935；
- level position 6477；
- 100 個 source enemy kills；
- 真正 boss group 清空；
- Sprite2 decode/cache drop 為 0；
- ROMFS 68 個檔案及 CRC/self-test 全數通過；
- mGBA 未報告 bad memory、illegal instruction 或 runtime error。

## 維護規則

- UI 文案或座標修改在 `build_frontend_mode4_assets()` 完成，不在 GBA
  runtime 增加 PIC/SHP 解碼。
- 新的靜態選單狀態要加入 frame atlas 與 metadata。
- 選取效果若只改變局部文字，沿用 selection rectangle 更新。
- 只有動畫範圍很小且需要每幀變動時，才考慮 OBJ/tile overlay。
- 關卡進入時必須停用 Mode 4 pending work，並重新載入 Mode 0 BG／OBJ
  VRAM；離開關卡後則由 front-end 重新初始化 Mode 4。
