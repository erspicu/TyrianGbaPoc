# GBA Detail Level 畫面特效 ARM 組語優化評估

日期：2026-08-07
階段：實作前評估

## 結論

依 `MD/Rule/Tyrian-GBA-Detail-Level-Visual-Stacking-Rule.md` 與目前 GBA
runtime 的實際呼叫路徑核對後，真正值得改成 ARM7TDMI 組語的主要是
三段「由 CPU 逐項建立硬體輸入資料」的迴圈：

1. **最終 hue／brightness 與 iced／lava-water 色盤建表**
   (`source_detail_build_effect_palette`)。
2. **High／Pentium lava-water 161 條掃描線位移表建表**
   (`source_detail_wave_prepare`)。
3. **Normal／High／Pentium spotlight 161 條 WIN0H 表建表**
   (`source_detail_spotlight_prepare`)。

這三者都有固定大小、連續記憶體、整數運算密集的特性，適合 ARM 的
條件執行、批次 load/store 與暫存器常駐。其餘大部分 Detail 效果已由
GBA PPU／DMA 完成，硬改組語不會明顯加速。

## 各效果逐項判斷

| 畫面效果 | 使用層級 | 現行成本位置 | 組語價值 | 決定 |
|---|---|---|---|---|
| 基本 BG1／BG3、多背景層 | 全部；BG2 自 Normal 起 | tile cache、row build、DMA | 相關 tile/cache 熱路徑多數已是 ARM/IWRAM；不是本次 Detail 專屬核心 | 不重複改寫 |
| 垂直翻轉特殊場景 | 全部 | 最多掃過 128 筆 OAM | 事件稀少，單次工作量小 | 保留 C，除非量測證明為熱點 |
| 半透明爆炸 | Normal 以上 | 設定 OBJ mode 位元 | PPU 硬體混色，CPU 幾乎只有一筆 OAM 屬性 | 不改 |
| 玩家／子彈陰影 | Normal 以上 | 額外 OAM 與 sprite cache | 限制通常是 OAM 數量／VRAM cache，不是算術 | 不以組語處理 |
| BLDY 亮暗 | Normal／High | 寫 BLDCNT／BLDY | 純硬體效果，數次暫存器寫入 | 不改 |
| iced hue adapter | Normal 以上 | 256 BG + 選擇性 OBJ 色盤重建 | 固定迴圈、會隨事件/fade 重建 | **實作 ARM kernel** |
| blur request | Normal 以上 | 目前只保留規則與 telemetry，沒有逐像素假 blur | 沒有 CPU pixel loop 可優化 | 不改 |
| spotlight | Normal／High／Pentium；Custom 關閉 | 每次 render 建 161 個 WIN0H 值，再由 HBlank DMA 播放 | 條件式 clamp 與連續 halfword store 很適合 ARM | **實作 ARM kernel** |
| lava／water hue | High／Pentium | 共用色盤重建 | 同 iced/final filter | **共用 ARM kernel** |
| lava／water scanline wave | High／Pentium | 每次 render 建 161×4 halfword，含 Q8 有號縮放 | 是最明確的 Detail 每幀 CPU 熱迴圈 | **優先實作 ARM kernel** |
| wild BG2 50/50 Alpha | Pentium／Custom | BLDCNT/BLDALPHA 設定 | PPU 硬體 Alpha，CPU 成本極低 | 不改 |
| wild 衝突用 checkerboard fallback | Pentium | cache miss 時 8 個 32-bit AND；現已在 Thumb IWRAM | 極短且罕見，再手寫 ARM 可能反而增大 IWRAM | 不改 |
| 最終 hue／brightness filtration | Pentium／Custom | 色盤重建；fade 時可能連續觸發 | 固定 256-entry 映射與 16-bit 輸出 | **實作 ARM kernel** |

## 預定實作方式

- 新增獨立的 `DETAIL_EFFECT_ASM=0/1` 建置開關，不與既有碰撞/RNG
  `HOTPATH_ASM` 混在一起，確保 A/B 只比較本次效果核心。
- 保留 C reference；release 預設使用 ARM 實作。
- wave kernel 分成 full／zero／scaled 三條路徑，在迴圈外決定模式；每條
  scanline 用兩個 32-bit store 寫完四個暫存器值。
- spotlight kernel 用 ARM 條件執行完成 radius、左右界 clamp，避免每行
  呼叫小函式。
- palette kernel 將 filter state 與 palette base 常駐暫存器，批次完成
  source-index 轉換及 15-bit 色彩打包；保留 OBJ bank 0..8/13/15 的原規則。

## 驗證與比較方法

1. **differential test**：同一批輸入分別跑 C 與 ARM，逐 byte／halfword
   比較輸出；涵蓋正負亮度、GLOBAL/WATER/NONE hue、OBJ filter 開關、
   water/lava profile、0/160/256 Q8 強度，以及 spotlight 邊界座標。
2. **microbenchmark**：GBA Timer2/3 cycle counter 量測單次與大量重複呼叫，
   扣除 loop/control overhead。
3. **整體 A/B**：固定 ROM、路線、Detail、武器壓力與模擬器，僅切換
   `DETAIL_EFFECT_ASM`，比較 logic/render/loop cycles、missed VBlank、
   render defer/drop 與音訊 underrun。
4. 至少覆蓋：Normal spotlight 路徑、High/Pentium 的 LAVA EXIT wave，及
   Custom 最終色盤 filter。LOW 用來證明未編入不需要的效果、沒有退化。

## 風險界線

- 不改視覺規格、不降低效果更新率、不調整 adaptive/drop-frame 門檻。
- 不把 PPU/DMA 已做好的工作搬回 CPU。
- ARM 程式只有在 bit-exact differential test 為零差異後才啟用。
- 會檢查 IWRAM map；若某 kernel 的整體收益不足以抵銷 IWRAM 壓力，則留在
  ROM ARM 或撤回，不犧牲既有 stack/熱路徑空間。

## 預期

wave 與 spotlight 是「每個 render frame」成本，預期最容易反映在
High/Pentium 特效關卡的 render cycles；palette 是「狀態變更/fade tick」
成本，主要改善切換與濾鏡動畫尖峰。整體遊戲改善幅度仍會受 Sprite2 cache、
OAM 與背景 tile 工作量主導，因此最終只採信實機指令週期與固定路線 A/B，
不以原始碼看起來較短作為成功標準。
