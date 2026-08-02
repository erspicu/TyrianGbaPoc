# Episode 4 LAVA EXIT Lava／Water 極限負荷驗證 v70

日期：2026-08-02

## 驗證目標

- Episode 4
- `levels4.dat` Section 31：`LAVA EXIT`
- `tyrian4.lvl` LVL 9
- High Detail、Normal Game Speed
- 無敵模式
- 原版資料定義的極限前武器、後武器、左右 Sidekick、Special 與
  Super Bomb 同時發射
- release-positive presentation scheduler：dynamic frame drop、wall-clock
  logic、missed-VBlank recovery、lazy result 與 packed collision 均啟用

關卡原始 event 64 時序為：

| PC event position | 狀態變化 |
|---:|---|
| 0 | Water ON，data 7 |
| 3340 | Lava ON，data 7 |
| 3390 | Water OFF，data 7 |

事件是在下一個 source tick 開頭處理，因此停在 position 3340 時仍是
Water-only；停在 3390 時仍可觀察到最後一個 Lava＋Water 狀態。

## 壓測入口改善

`full-loadout-stress` 不再固定 Episode 2／Section 1。Makefile 與
`tools/run_full_loadout_stress.ps1` 現在可指定：

- Episode
- Section
- 精確的 source event position，或固定 VBlank 數
- Detail Level
- 診斷 scheduler variant

壓測 ROM 會自行強制啟用無敵與極限武器，不修改正式
`Configure.h` 的裝備設定。TGW8 SRAM 尾端也追加實際 Episode、Section、
LVL、Lava／Water 最終狀態與 data，避免只靠檔名猜測測到了哪一關。

## 實測結果

以下是同一套 deterministic input 的累積快照：

| 停止位置 | Lava | Water | Logic | Display | Recovered missed VBlank | Audio loss | Palette rebuild |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 3300 | 0 | 1 | 3300 | 5667 | 1062 | 2 | 1 |
| 3340 | 0 | 1 | 3340 | 5736 | 1071 | 2 | 1 |
| 3390 | 1 | 1 | 3390 | 5822 | 1093 | 2 | 2 |
| 3440 | 1 | 0 | 3440 | 5908 | 1127 | 3 | 2 |

切換區間增量：

| 區間 | 狀態 | Logic | Display | Recovered missed VBlank | 完成新畫面 | 延後新畫面 | Audio loss |
|---|---|---:|---:|---:|---:|---:|---:|
| 3300–3340 | Water-only | 40 | 69 | 9 | 29 | 29 | 0 |
| 3340–3390 | Lava＋Water | 50 | 86 | 22 | 36 | 36 | 0 |
| 3390–3440 | Lava-only | 50 | 86 | 34 | 34 | 32 | 1 |

position 3440 的完整壓力累積值：

- OAM 最高 128，確實打滿硬體上限。
- 產生 21,848 發 player shots；81-slot pool 因刻意極限配置拒絕 975 發。
- projectile cache drops：0。
- unknown source visuals：0。
- background approximations：0。
- source assets valid：1。
- stress loadout failures：0。
- 音訊只少 3／5908 個 display periods，`0.0508%`，低於專案允許的
  1% 壓力退化範圍。
- logic backlog：0；關卡位置與 logic updates 均精確到 3440，沒有因
  presentation 掉幀拖慢遊戲時序。
- enemy cache drops 從 position 3300 的 665 增至 3440 的 680；effect
  cache drops 維持 288。切換本身沒有造成未知素材或背景快取破圖。

## 判定

LAVA EXIT 的 Water-only → Lava＋Water → Lava-only 三段事件均正確觸發，
High Detail 的 palette／wave adapter 也在 Lava 開啟時恰好多重建一次。
三張實際 240×160 畫面未見未初始化 tile、錯讀 LVL、背景 approximation
或調色盤崩壞；畫面中的大型光束及滿屏子彈來自刻意啟用的 stock
壓力武器。

這個極端配置確實超過每次都產生新畫面的預算，尤其 Lava-only 區段；
但正式 drop-frame scheduler 會保留上一張完整場景，邏輯、事件時序及
Maxmod 音訊仍按 wall clock 前進。這輪結果不顯示 Lava／Water 切換有
功能性 bug，也不需要為這個切換另寫關卡特例。

## 重現命令

```powershell
./tools/run_full_loadout_stress.ps1 `
  -DetailLevel high `
  -Variant active_mask_fast_wall_lazy_packed `
  -Episode 4 -Section 31 -EndPosition 3390
```

## 保存產物

實際 240×160 PNG 與完整 TGW8 JSON 保存在：

`temp/episode4_lava_exit_stress_v70/`

測試 ROM 已依專案產物政策移至 `Backup/`，不會取代正式的
`build/TyrianGBA.gba`。
