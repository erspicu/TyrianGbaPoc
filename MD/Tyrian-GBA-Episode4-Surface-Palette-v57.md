# TyrianGbaPoc v57：Episode 4 SURFACE 樹木色盤修正

日期：2026-07-30
分支：`palette-camera-data-v56`

## 結論

Episode 4 第一個可玩關卡 SURFACE 的樹木綠色方塊不是 LVL／SHP
讀錯，也不是 GBA 已經無法改善的硬體上限。直接原因是 build-time
adapter 把同時含綠葉與沙地的 `hue mask 0x0404` 指派到單純綠色
的 palette bank 2，導致 tile 內的褐色沙地像素被量化成綠色。

GBA 的 4bpp text BG 確實限制每塊 8×8 tile 只能使用一個
15 個非零色的 bank；這會犧牲少量相鄰亮度層次。但本次已找到一組
mixed bank，可在保留綠、褐兩個 hue family 的同時，讓 SURFACE
全部 867 個實際 runtime keys 在 OKLab 與 CIEDE2000 都零退步。
因此這個案例主要是訓練與驗收規則問題，而不是硬體表現上限。

## 關卡對應更正

v56 報告錯把 `tyrian4.lvl` 實體 section #1 當成 Episode 4 的
第一個可玩關卡：

| 對象 | 實體 section | shape bank | v56 OKLab／CIE 改善 |
|---|---:|---|---:|
| v56 誤分析的資料 | 1 | `z` | 43.485578%／17.043786% |
| 真正 SURFACE | 4 | `)` | 0.055378%／0.040607% |

SURFACE 在 v56 只改到一個 mask，幾乎沒有改善。這也是使用者仍能
清楚看到綠色方塊的原因；不能把實體 #1 的結果當成修復證據。

## 精確根因

以相同 camera position 重建 PC stock 背景與 GBA adapter 後，樹木
周圍的沙地幾何和 source index 都相同，差異完全來自 palette：

- 樹木 tile：`mask 0x0404`，即 hue 2（綠）＋ hue A（沙／褐）；
- v56 assignment：bank 2，只有綠色 ramp；
- 相鄰純沙 tile：`mask 0x0400 → bank 10`，幾乎精確；
- 例如 source `0xA8 = RGB(125,105,85)` 被錯映成
  `RGB(90,132,66)`；
- source `0xA9 = RGB(146,121,97)` 被錯映成
  `RGB(123,156,90)`。

同一塊 PC background shape 內本來就含樹葉與周圍沙地，不是透明
sprite 疊在純沙 tile 上。當沙色被單綠 bank 取代時，整個 8×8 tile
邊界自然會形成規則綠色方塊。

## Gemini 3.1 Pro 複核

本次把硬體限制、真實 key/histogram、兩種 perceptual metrics 和
ramp 統計完整交給 `gemini-3.1-pro-preview` 審查。模型的主要裁決
與本地實驗一致：

1. 問題約 90% 來自離線 pipeline／gate，4bpp 只造成少量色階取捨；
2. 「raw ramp collision 數不得增加」不應是 hard gate；
3. 錯誤 baseline 雖把多個沙地亮度映成不同顏色，但那些顏色全部是
   綠色；它在 collision 計數上漂亮，視覺 hue 卻完全錯誤；
4. 先保持 mask-only、零 runtime cost；只有 constraint generation
   找不到共同解時，才值得增加 key override schema；
5. hard gate 應保留 per-key OKLab／CIEDE2000 非退步與 lightness
   inversion，不應用 raw collision 否決正確 hue 的 mixed bank。

完整問題與回答保存在工作區：

- `knowledgebase/message/TyrianGbaPoc-v57-episode4-surface-palette-query-2026-07-30.md`
- `knowledgebase/message/TyrianGbaPoc-v57-episode4-surface-palette-response-2026-07-30.md`

## 通用修法

新的第二階段只在 build time 執行，不增加 ROM runtime 查表與每幀
成本：

1. 第一輪沿用 safe-unused 訓練，保留每個 baseline bank 作 fallback；
2. 找出第一輪仍未改善、含多個 hue、CIEDE2000 平均至少 10、
   曝光至少 32 次的嚴重 mask；
3. 在沒有被 runtime assignment 使用的 bank 建立 mixed candidate；
4. 對所有同 mask runtime keys 分別計算 OKLab 與 CIEDE2000；
5. 若有退步，以最大 violation face 的 histogram、實際曝光量及
   `2**iteration` 權重加回訓練；
6. 保留 best checkpoint；只有所有 key 雙 metric 零退步且
   lightness inversion 不增加時才接受；
7. raw ramp collision 留作 telemetry，不再是 hard rejection。

這是針對真實 runtime dataset 的 constraint generation，不是
Episode 4 專用手工 palette，也沒有增加每關 Python 修圖表。

## SURFACE 結果

`mask 0x0404` 最終切換到重訓的 mixed bank 4：

| 指標 | v56 → v57 |
|---|---:|
| 全關 unique runtime keys | 867 |
| active masks | 9 |
| 全關 OKLab 改善 | 0.055378% → **82.253753%** |
| 全關 CIEDE2000 改善 | 0.040607% → **31.186084%** |
| `0x0404` OKLab 改善 | **96.583188%** |
| `0x0404` CIEDE2000 改善 | **88.847342%** |
| OKLab regressed keys | **0** |
| CIEDE2000 regressed keys | **0** |
| lightness inversions | 0 → 0 |
| raw adjacent-ramp collisions | 5 → 13（telemetry） |

相同 PC stock 背景 composition 的影像級數據：

| 區域 | CIEDE2000 v56 → v57 |
|---|---:|
| 兩棵樹區域平均 | **8.765338 → 1.868281** |
| 兩棵樹區域 P95 | **28.014936 → 3.512212** |
| 全 240×160 背景平均 | **2.197268 → 1.746017** |

raw collision 增加是 15 色 mixed bank 壓縮兩組 brightness ramps 的
可預期代價；畫面上不再把大面積沙地變綠，且所有真實 tile 的兩種
感知誤差都沒有退步。

## 對照圖

郵件附件使用：

- `temp/v57_episode4_palette_mail/01_pc_stock_surface_p50.png`：
  直接從 stock `tyrian4.lvl #4`、`shapes).dat`、`palette.dat`
  重建，不經 GBA palette；
- `temp/v57_episode4_palette_mail/02_gba_v57_mgba_surface_p50.png`：
  新 ROM 由 mGBA 實際執行到同一開場區域的 frame；
- `temp/v57_episode4_palette_mail/03_pc_vs_gba_v57_side_by_side.png`：
  同一 camera composition 的整數倍 nearest-neighbour 對照。

## 驗證

- Python 語法檢查：PASS；
- 62 個實體 LVL section 全部重新產生；
- 每關 OKLab regressed keys：0；
- 每關 CIEDE2000 regressed keys：0；
- SURFACE route ROM：編譯成功；
- mGBA 實際 frame：綠色方塊消失；
- 完整 High Detail／Normal Speed build 與 runtime regression：PASS；
- 主要 route：12,168 display frames，11 missed VBlanks；
- 主要 route unknown visuals／Sprite2 drops／projectile drops：0；
- 62 個 LVL section palette matrix：62／62 PASS；
- 最終 ROM：27,770,076 bytes（32 MiB 的 82.7613%）；
- 最終 ROM SHA-256：
  `5517b18ec048af36ee0dbd494587083332f3cd33893867e71f92120757aff5b7`。

## 官網技術文章

未上線官網新增獨立研究文章：

- `Website/research/palette-training.html`
- `Website/assets/images/research/episode4-palette-pc-vs-gba-v57.png`

文章把本次案例整理成一般技術讀者可理解的流程，並明確區分
build-time 受限色盤訓練與 ROM runtime：訓練不在 GBA 上執行，
遊戲執行時沒有新增每幀最佳化成本。
