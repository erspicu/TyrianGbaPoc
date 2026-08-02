# Tyrian GBA v72：Lava／Water 連射橫帶根因與自適應修正

日期：2026-08-03
狀態：根因已確認，修正已完成定向 A/B 驗證

## 問題

PENTIUM Detail 在 Episode 4、Section 31、LVL 9 `LAVA EXIT` 已能呈現
lava／water 的色相與波動效果；主角持續發射極限壓力武器時，背景卻會出現
明顯閃爍及水平橫帶。

## 根因

這不是 DMA 越界、半幀提交或 mGBA/APRNES 顯示錯誤，而是兩個條件疊加：

1. 極限武器讓玩家彈幕達到 81 個 active slot、128 OAM，碰撞、Sprite2、
   projectile cache 與 render 工作量超過單一 GBA LCD frame 的 280,896 cycles。
   正式 drop-frame scheduler 會正確保留上一個完整 scene，再提交較新的完整
   scene；它不會提交半張背景，但相鄰呈現畫面的世界位移會變大。
2. OpenTyrian 的 `lava_filter()`／`water_filter()` 每 8 個水平像素只改變局部
   framebuffer 樣本及色彩平均。GBA Mode 0 沒有 affine text BG，也不能在
   HDraw 中安全地每 8 像素改寫 scroll，因此舊 adapter 把單一樣本位移套到
   整條 BG0/BG1 scanline。高對比 lava 紋理遇到跳幀時，這個近似會被放大成
   水平橫帶。

DMA0 的 HBlank table 只有 4 個 halfword；DMA1/2 由 Maxmod 使用，DMA3 負責
一般 VBlank copy。mGBA 原始碼與實測都證明 DMA0 只在可見 160 條掃描線執行，
不存在第 161 筆資料寫入 VBlank register 的越界路徑。

## 修正策略

### 1. Source-derived 空間低通 profile

- 依 OpenTyrian 原公式計算每個 8-pixel block 的 waver。
- 對 GBA 實際 240-pixel crop 做可見像素加權平均；左右兩個部分 block 依
  實際可見寬度加權。
- 再做五條 scanline 的空間低通，降低「局部 PC 樣本」被放大成「整列 GBA
  位移」所產生的硬階梯。
- 移除 waver 的正向 DC bias：PC 的 bias 用於色彩樣本位置，不是整張攝影機
  平移；lava／water 的色相仍由既有 palette adapter 完整保留。

兩種固定 profile 在 IRQ 與 Maxmod 啟動前各建立一次。它們不再於首次進入
lava／water 時搶走 gameplay 或 mixer frame。

### 2. 只作用於 presentation 的負載遲滯

新增 0..24 的 presentation pressure score：

- render 被 deadline scheduler 延後時累加；
- scene 積欠超過一個 source tick，或完整 render 超過 220,000 cycles 時累加；
- 正常完整 render 時緩慢下降；
- 進入與離開使用不同門檻，避免效果每幀開關。

波紋強度以 Q8 緩升／緩降：

| 狀態 | 強度 | 行為 |
|---|---:|---|
| 正常 | 256/256 | 保留完整、平滑後的波紋 |
| 持續忙碌 | 160/256 | 降低幾何位移，色相不變 |
| 壓力分數 24/24 | 0/256 | 暫停幾何位移，消除橫帶；lava/water 色相仍保留 |

壓力解除後會逐步恢復，不會一幀突然切換。所有敵人、子彈、碰撞、RNG、音效、
關卡進度及 source game loop 都沒有被減少或改速；這不是刪除 projectile 的
假優化。

## 定向 A/B 數據

共同條件：PENTIUM／Normal Speed／Episode 4 Section 31／position 3440／
完整極限武器／無敵測試／5,908 display frames、3,440 source logic updates。

### 連續射擊

| 指標 | v71 修正前 | v72 修正後 |
|---|---:|---:|
| missed VBlank | 1,254 | 1,231 |
| missed 比率 | 21.225% | 20.836% |
| audio frame loss | 5（0.0846%） | 4（0.0677%） |
| 最大 OAM | 128 | 128 |
| 玩家射擊生成 | 21,794 | 21,794 |
| 玩家彈幕最大 active | 81 | 81 |
| pressure score 最大值 | — | 24/24 |
| wave 最低強度 | — | 0/256 |

完整 source 負載相同，missed VBlank 反而減少 23 次，完整 render 平均亦由
187,921 cycles 降至 184,916 cycles；原因是完整強度與零強度都有 fast path，
不再逐 scanline 執行不必要的乘法。畫面 A/B 已確認原本覆蓋整個場景的水平
橫帶消失。

### 不射擊基準

同一路線不射擊時：

- missed VBlank：1；
- audio frame loss：0；
- 最大 OAM：36；
- projectile spawn：0；
- 正式 severe 門檻為 24，定向基準最大壓力為 20，因此不會關閉波紋；只有
  冷快取集中出現時短暫進入 160/256 的中度保護。

## Gemini 3.1 Pro 諮詢評估

以模型預設參數諮詢後，值得採用的方向是：空間低通、遲滯式自適應幅度，以及
維持 wave 與 held scene 同步。沒有採用「讓 wave phase 獨立更新」或任意
projectile pin/cap：前者會讓保留畫面上的波紋自行游動，後者會破壞 source
畫面完整性，而且現有 cache 已按 `(shape_table, graphic)` 共用，不是每顆
子彈重複配置。

後續 Gemini 諮詢依使用者要求使用模型預設值，不再刻意傳入
`--temperature` 或 `--max-tokens`。

## 結論

極限武器的 CPU/OAM 壓力屬於 GBA 真實硬體上限，drop-frame 仍是必要的正式
機制；水平橫帶則不是必須接受的硬體結果，而是可改善的 Mode 0 presentation
近似。v72 保留完整 gameplay，在正常負載呈現波紋，只有持續達到最嚴重負載
時平滑暫停幾何扭曲，避免玩家看到閃爍與橫帶。
