# Tyrian GBA 調色盤訓練實作與驗證 v54

日期：2026-07-30

分支：`palette-training-v54`

狀態：完成；符合合併門檻

## 結論

v54 已把背景調色盤訓練從「只看獨立 `shapes?.dat` 切片」改成
「重建 62 個原始關卡真正會送進 GBA tile cache 的資料分布」。

正式採用的不是追求平均分數最高、但可能讓少數灰色物件變成棕色
的激進模型，而是 `safe-unused` 策略：

1. 保留每套 shape profile 中，v53 實際有使用的所有 palette bank。
2. 只訓練該 profile 在 62 關 runtime 資料中未使用的 bank。
3. 既有 bank 的色槽映射只有在 OKLab 與 CIEDE2000 同時不退步，
   且不增加亮度階梯倒置或相鄰色碰撞時，才接受 Pareto 改善。
4. 訓練 bank 只有在同一 hue mask 的每一種 runtime tile key 都不
   退步時才會被採用；否則仍使用原本 v53 bank。

因此，正式候選在 33,360 種實際 runtime tile key 上同時達成：

- OKLab squared error：0 個退步 key。
- CIEDE2000：0 個退步 key。
- 五套 profile 的平均與 CVaR95 感知色差都下降。
- 亮度階梯倒置與 palette collision 均未增加。

## 真實 runtime 訓練資料

工具：`tools/background_palette_training.py`

資料直接讀取專案內的原始 Tyrian 檔案：

- `tyrian1.lvl`～`tyrian4.lvl`
- `shapes).dat`、`shapesw.dat`、`shapesx.dat`、`shapesy.dat`、
  `shapesz.dat`
- `palette.dat`

重建的 key 與 runtime `background_runtime.inc` 相同：

```text
(top_shape, bottom_shape, vertical_phase, horizontal_sub_x)
```

這也涵蓋 28-pixel shape 跨界時，由上、下兩個 shape 合成的 8x8
tile；v53 的獨立 shape 切片訓練沒有完整表示這一部分。

固定資料集：

| 項目 | 數量 |
|---|---:|
| 邏輯關卡 | 62 |
| 含空白的地圖 tile occurrence | 13,771,254 |
| 非空白 unique runtime key | 33,360 |
| 跨 profile unique hue mask | 202 |

每個 profile 的 key 集與 SHA-256 皆寫入 `asset_report.txt`，正式
建置會檢查固定 hash，避免 loader 或訓練資料範圍意外改變。

## 心理視覺模型

訓練與驗收以兩種感知模型共同約束：

- OKLab：用於離散 BGR555 中心與高效率訓練。
- CIEDE2000（D65 CIELAB）：作為獨立感知色差驗證。

流程會先做標準 sRGB inverse transfer，再進入線性 RGB 與對應
色彩空間。CIEDE2000 實作以 Sharma 標準測試向量驗證，三組誤差
分別小於 `0.00005`。

原始 sRGB code-space 或 linear-RGB 距離不是正式驗收目標；感知
模型改善時，少數 profile 的原始 RGB 距離可能上升。這是刻意的
psychovisual 取捨，不是未被發現的回歸。

## safe-unused 演算法

每套 profile 各自擁有 16 個 4bpp palette bank：

1. 從所有 active runtime mask 找出 v53 實際引用的 protected bank。
2. 其餘 bank 才可被重新訓練；依 profile 可用 bank 數為 1～6。
3. 以 occurrence-weighted runtime histogram 做離散 BGR555
   Lloyd 更新。
4. 對 protected bank 嘗試 Pareto nearest mapping；每個來源色的
   OKLab 與 CIEDE2000 都不得差於 v53。
5. 對每個 active mask 逐 bank 比較：
   - 該 mask 的每一個 runtime key，OKLab 不得退步。
   - 該 mask 的每一個 runtime key，CIEDE2000 不得退步。
   - hue ramp 的 lightness inversion 不得增加。
   - 相鄰 palette collision 不得增加。
6. 任一條件失敗即保留 v53 bank。
7. 未出現在 stock 62 關的未知 mask，只能回退到 protected bank，
   不會誤用已重新訓練的舊 bank 編號。

這個設計讓 v53 本身永遠是可行解，也就是不靠「平均分數較好」
掩蓋少數肉眼明顯的錯色。

## 量化結果

下表的兩組數字均為「平均改善 / CVaR95 改善」；正值代表誤差
下降。Regressed keys 在兩種感知模型下全部為 0。

| Profile | 關卡 | Runtime keys | 可訓練 banks | 安全換 bank masks | OKLab | CIEDE2000 |
|---|---:|---:|---:|---:|---:|---:|
| `)` | 10 | 4,672 | 1 | 11 | 6.892706% / 15.721483% | 1.321783% / 3.348865% |
| `w` | 7 | 5,766 | 3 | 30 | 5.852295% / 13.734587% | 0.442165% / 2.033240% |
| `x` | 7 | 7,637 | 6 | 11 | 37.322817% / 2.044497% | 16.541938% / 0.869534% |
| `y` | 6 | 4,413 | 5 | 4 | 9.577746% / 21.223234% | 2.175298% / 13.416648% |
| `z` | 32 | 10,872 | 1 | 15 | 8.120954% / 13.722057% | 0.787549% / 2.047764% |

Ramp 品質：

| Profile | Lightness inversions | Adjacent collisions |
|---|---:|---:|
| `)` | 4 → 4 | 761 → 648 |
| `w` | 18 → 14 | 1,792 → 1,342 |
| `x` | 16 → 13 | 957 → 877 |
| `y` | 0 → 0 | 260 → 238 |
| `z` | 0 → 0 | 1,367 → 1,238 |

固定輸出 hash：

```text
palette = 130ef4c2292f2d12ba8c8bf544858f22a7795dfbf5160b074320ef7a2ddebe90
nearest = 5026d215f4ea94a75790b8226227da39787557f98b11f6902b1779c8035a64a6
mask    = 24017252ff50a08fa78acbce904c12b38e9a8ae0715305c55babeae338946f45
```

兩次獨立訓練得到相同 hash。

## ROM 成本與效能

資產格式與大小完全不變：

| 資產 | 大小 |
|---|---:|
| `background_gba_palette.bin` | 2,560 bytes |
| `background_palette_nearest.bin` | 20,480 bytes |
| `background_palette_mask_bank.bin` | 327,680 bytes |

所有額外工作都發生在 build host。GBA runtime 仍是同一個
`mask -> bank -> local colour` 查表流程，因此：

- ROM 大小不因本功能增加。
- EWRAM / IWRAM 不增加。
- 每 tile 的 runtime 指令與 DMA 路徑不增加。

## 實機畫面 A/B

以完全相同的 screenshot autotest ROM，在關卡位置 800 停止；
baseline ROM 只把三個背景調色盤資產換回 v53。其餘程式、輸入、
關卡狀態與 OAM 完全相同。

涵蓋：

- `z`: Episode 1 / section 1
- `x`: Episode 2 / section 1
- `w`: Episode 3 / section 1
- `)`: Episode 4 / section 4
- `y`: Episode 1 / section 8

五組畫面皆未發現幾何、tile 索引、圖層或 sprite 回歸。`x` profile
呈現大範圍但幅度小的水面／地形色調修正；`y` profile 的島嶼高光
與材質階梯更清楚。其他 profile 的改動集中在少量原本量化較差的
混色 tile，符合安全採用策略。

本機暫存證據位於：

```text
temp/palette_training_v54/visual_safe_unused/
temp/palette_training_v54/safe_unused_final/report.json
temp/palette_training_v54/safe_unused_final/worst_tile_comparison.png
```

`temp/` 為建置驗證暫存，不納入 Git。

## 完整回歸

`build.ps1 -KeepIntermediates -DetailLevel high -GameSpeed normal`
已通過：

- ROM header、32 MiB 上限與 IWRAM/EWRAM 預算。
- 62/62 關 ROMFS matrix。
- Episode 2、3、4 第一關完整 route smoke。
- Episode 1 四關 campaign。
- Boss、死亡、Jukebox、Demo、Arcade。
- 靜態選單完整轉場壓力測試。
- 背景 approximation 為 0。
- Sprite2 decode failure / cache drop 為 0。

由於正式建置會重新計算資料集、感知誤差、ramp 指標與固定 hash，
未來資源或 loader 變動若破壞 non-regression，會直接使 build
失敗，而不是悄悄產生錯色 ROM。
