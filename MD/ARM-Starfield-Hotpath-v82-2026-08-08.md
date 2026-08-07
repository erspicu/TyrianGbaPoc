# GBA 關卡星空 ARM 熱點優化（v82，2026-08-08）

## 結論

關卡內 BG3 星空原本是目前最大的未處理 render 熱點。每顆星都從
OpenTyrian 的 320-wide 線性位置做 `/ 320`、`% 320`，亮星再經由
`gameplay_overlay_set_pixel()` 逐點重做座標、cell、tile 查找與配置。

本階段保留 PC 星星資料、反向繪製順序、文字遮擋、亮度、裁切及 tile
配額，只把 GBA adapter 的重複工作換成兩個小型 IWRAM ARM leaf：

1. `gameplay_overlay_divmod320_asm()`：一次回傳精確的 source x/y。
2. `gameplay_overlay_plot_star_tile_asm()`：配置一次 sparse tile 後，在
   同一 8x8 4bpp tile 內批次寫入中心與最多四個十字像素。

## 正確性

- divmod：窮舉全部 65,536 個 `u16` position，與 C `/`、`%` 完全一致。
- tile plot：窮舉 64 個 local coordinate 與 256 組 centre/dim nibble，
  共 16,384 組；除了目標像素，也驗證其餘既有 nibble 未被破壞。
- 差分結果：`3`（divmod 與 plot 兩部分均通過）。
- A/B 的邏輯更新均為 349、位置均為 698、shot spawn 均為 2,070；
  表示優化沒有改變 gameplay 時間軸或 RNG 結果。
- 壓力畫面人工抽查沒有星空、武器、敵人或 tile 污染。

## 同 ROM 微測試

每項 16,384 calls：

| 核心 | C cycles | ARM cycles | 改善 |
|---|---:|---:|---:|
| divmod-320 | 1,939,683 | 1,031,428 | **-46.83%** |
| in-tile star plot | 4,942,270 | 2,280,741 | **-53.86%** |

## Episode 1／Section 5 全武器壓力 A/B

條件：CUSTOM detail、Normal speed、600 VBlank、無敵、全武器壓力模式；
既有 ARM hotpath、Detail ASM、Sprite2 exact lookup 均保持開啟。唯一變因
是 `TYRIAN_GBA_STARFIELD_BATCH_ASM`。

| 指標 | 舊逐點路徑 | ARM batch | 差異 |
|---|---:|---:|---:|
| logic updates | 349 | 349 | 0 |
| missed VBlank | 434 | 313 | **-27.88%** |
| render cycles total | 57,310,343 | 41,463,562 | **-27.65%** |
| render cycles／logic | 164,213.02 | 118,806.77 | **-27.65%** |
| loop-work cycles／display | 257,864.16 | 238,428.60 | **-7.54%** |
| audio frame loss | 71 | 1 | **-98.59%** |

`presentation_render_completed` 從 166 增至 169；主要收益不是改變遊戲
速度，而是讓 render 不再長時間佔住 CPU，使音訊與 VBlank recovery 有
充分的排程空間。

## IWRAM

壓力測試 stack canary 尚餘 3,688 bytes，高於專案 3,072-byte 壓力建置
門檻。新增的只有兩個小型 leaf；沒有把整個 overlay orchestration 搬入
ARM/IWRAM。

## 開關

正式版預設啟用；壓力 A/B 可使用：

```powershell
.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel custom -Episode 1 -Section 5 `
  -DurationVBlanks 600 -StarfieldBatchAsm 0
```

將最後一項改成 `1` 即為新路徑。
