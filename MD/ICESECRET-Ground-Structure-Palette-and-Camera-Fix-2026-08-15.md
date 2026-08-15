# ICESECRET 固定建築色盤與柔性鏡頭修復

日期：2026-08-15

## 結論

使用者紅框內的主體是 Episode 4 Section 50（實體 `LVL 20`）由 event 12
生成的 Sprite2 地面結構，主要來自 enemy definition 80～83 與 87～90。
它們的原始 X/Y 速度、加速度與反轉欄位均為零，必須牢固附著 MAP1；不是
先前報告中會自行移動的 definition 130／132。

本次把兩個不同問題分開處理：

1. 呈現座標現在統一使用 **BG1 套用地圖邊界夾限後的實際柔性鏡頭位移**；
   OBJ、BG1、BG2 與測試不再各自使用未夾限的要求值。
2. 這批 Sprite2 圖塊本身帶有不透明冰地像素。PC 的 MAP 與 Sprite2 共用
   同一套 256 色盤；GBA 則必須將 4bpp BG 與 8bpp OBJ 分開配置。舊版 OBJ
   使用每個 hue 固定的等距亮度樣本，會使建築周圍冰地與 MAP 的量化結果
   分歧，看起來像灰色矩形、接縫或「浮在背景上」。現在改為全資源訓練。

沒有新增 per-level 對照表，也沒有針對某棟建築硬改顏色或座標。

## 柔性鏡頭修正

新增 `source_camera_presentation_offset_y`，其值由 BG1 真正的呈現捲動量
反推出來：

```text
actual camera Y = BG1 scroll(requested camera Y) - BG1 scroll(camera Y = 0)
```

以下消費者共用這個值：

- ground Sprite2 的最後畫面座標；
- BG1／BG2 的垂直呈現座標；
- 背景列準備與 autotest；
- ground attachment invariant。

因此在地圖頂／底邊界，背景已停止而要求中的 camera offset 尚未歸零時，
OBJ 也不會多走或少走一段。

更新後直接量測實際螢幕不變量：

```text
screen attachment Y = actual Sprite2 screen Y + actual BG1 presentation scroll
```

## Sprite2 全資源色盤訓練

建置工具讀取完整 normal 與 Christmas Sprite2 raw catalog，針對 Tyrian 的
16 個 hue 各自從 16 個 brightness 中選出 8 個 RGB medoid。搜尋空間為
每個 hue 的 `C(16, 8)`，以實際使用像素頻率加權；輸出仍是原本的 128 個
OBJ 色盤 entry。

| 指標 | 舊等距樣本 | 新全資源訓練 |
|---|---:|---:|
| 加權 RGB 平方色差 | 19,602,644 | 12,910,795 |
| 改善 | — | 34.13% |
| OBJ 色盤 entry | 128 | 128 |
| 執行期 RAM／VRAM／OAM 增量 | 0 | 0 |
| 每像素 pack 成本增量 | 0 | 0 |

訓練是整個 Sprite2 bank 的通用無損來源統計，不是 ICESECRET 專用資料；
未來其他章節與季節素材會自動套用相同流程。建置時若訓練結果沒有優於舊
基準，工具會直接失敗，避免靜默退化。

## 色彩判讀注意事項

definition 89／90 的原始 Sprite2 圓頂像素本來就是灰、紫與高光色，不應
直接硬改為紅色。PC 最終觀感還會受到該畫面共用色盤與 Detail filtration
影響；CUSTOM 模式刻意不啟用 Pentium 的 spotlight。這次修復的目標是：

- 保留原始 source index 的色相與明暗關係；
- 降低 Sprite2 與 MAP 同一片冰地的量化接縫；
- 不以關卡例外或假造紅色掩蓋真正問題。

## 針對性驗證

條件：CUSTOM detail、NORMAL game speed、Episode 4 Section 50、正式配裝
路徑、3,600 VBlank。

| 項目 | 結果 |
|---|---:|
| ground attachment 樣本 | 1,262 |
| 鏡頭移動中的樣本 | 530 |
| attachment 失敗 | 0 |
| 最大 X／Y 誤差 | 0／0 pixel |
| source assets valid | 通過 |
| enemy／projectile／effect cache drops | 0／0／0 |
| 音樂保持 active | 通過 |
| 最大 OAM | 116／128 |

本輪只跑對應問題的定點測試，沒有進行不必要的廣泛回歸。

## 建置產物

- 模式：CUSTOM／NORMAL speed
- ROM：`build/TyrianGBA.gba`
- 大小：28,057,868 bytes（26.76 MiB）
- SHA-256：`73bf3842afc9939e85bf1a2ff96717b504f9ef70b0371645502859884d6f4b36`

## 2026-08-15 圖層 sentinel 後續修正

上述 attachment invariant 證實 Sprite2 本身與 BG1 沒有座標誤差，但後續
逐行比對又找到另一個畫面來源：Episode 4 以 `background2over=254/255`
停止繪製 BG2，GBA 舊版卻仍讓該視差層留在畫面上。完整 root cause、修復
與 LOW／CUSTOM 對照量測見
`ICESECRET-BG2-Sentinel-Layer-Attachment-Fix-2026-08-15.md`。
