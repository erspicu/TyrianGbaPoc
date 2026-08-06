# Tyrian GBA Adaptive／Drop-frame 全域規則

日期：2026-08-05  
狀態：正式版預設啟用

## 結論

`Drop-frame` 與 `Adaptive presentation dispatch` 解決的是不同層次的問題，
兩者應同時開啟：

- Drop-frame 是逐 LCD frame 的截止時間保護。預估來不及安全完成下一張場景時，
  保留上一張完整畫面，避免半套 OAM／VRAM 狀態被送到螢幕。
- Adaptive 是建立在 Drop-frame 上的壓力狀態機。當連續量測證明某段關卡或武器
  組合長期超載時，主動把完整場景建構固定在較低但穩定的頻率，避免一再累積後
  被 freshness 上限強迫做昂貴 render。

兩者都只省略已被較新狀態取代的 presentation。遊戲邏輯、碰撞、敵人事件、RNG、
掉落、Boss／關卡流程與每個實體 VBlank 的音訊服務都不得省略或改序。

## 正式設定

`Configure.h` 的正式預設為：

```c
#define TYRIAN_GBA_DYNAMIC_FRAME_DROP 1
#define TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH 1
#define TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH 1
```

三者的關係如下：

| 設定 | 作用 |
|---|---|
| `TYRIAN_GBA_DYNAMIC_FRAME_DROP` | 固定時間步進及完整場景 deadline scheduler；正式版基礎機制 |
| `TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH` | 對所有 gameplay 關卡依實測負載啟用 Medium／Severe tier |
| `TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH` | lava／water 波紋確認超載後直接採 Severe tier；不是另一套 drop-frame |

關閉 Adaptive 只供效能 A/B 診斷；不得作為一般 release 設定。前端靜態選單不套用
gameplay Adaptive，仍由既有轉場快取與分段建構機制處理。

## 三段呈現狀態

來源 Tyrian Normal game speed 約為 34.8 logic tick/s。

| 狀態 | 完整場景節奏 | 進入／離開原則 |
|---|---:|---|
| Light | 有安全預算就呈現 | 預設狀態，不因單一冷快取尖峰降級 |
| Medium | 每 2 個 source tick 一張，約 17.4 FPS | 一般場景累積 4 點 deadline 壓力，且 baseline 後至少 2 次 missed VBlank |
| Severe | 同樣封頂為每 2 tick 一張，約 17.4 FPS | 壓力達 8、missed VBlank 達 4，或 wave 場景確認超載；只保留壓力分類，不再降低 cadence |

狀態另受下列安全規則約束：

- Medium／Severe 最多保留 2 個 pending logic ticks；達到上限就強制建立完整場景。
- 背景 ring row 快耗盡、關卡狀態轉換或其他 freshness 條件永遠有權強制 render。
- 連續 16 張完整 render 都不超過 150,000 cycles，才退出 Adaptive，避免 tier 抖動。
- 未啟動時，一張低於 150,000 cycles 的完整 render 會清除累積壓力；單次 cache cold miss
  不會讓後續低負載畫面被長期降頻。
- lava／water 只縮短確認門檻（2 點壓力、1 次 missed VBlank）並直接進 Severe；
  其他高負載關卡、Boss、爆炸或複雜武器仍由同一套全域量測自動處理。

`TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME=2` 是正式的流暢度下限。
Normal speed 無法用整數 source tick 精確得到 15 FPS，因此採用較平順的約
17.4 FPS，而不是較低的 11.6 FPS。若單次不可切割 render 本身跨越多個實體
VBlank，實際畫面仍可能偶發停留較久；這是硬體超時，不是 Adaptive 主動選擇
低於下限的 cadence。

## NORMAL、Episode 2 非 wave A/B（舊三 tick Severe 基準）

條件：Episode 2 Section 1、NORMAL、600 個 LCD VBlank、相同最大壓力武器與輸入。
此段沒有 lava／water wave scope。

| 指標 | Drop-frame only | Drop-frame + Adaptive | 差異 |
|---|---:|---:|---:|
| display frames | 600 | 600 | 0 |
| source logic updates | 349 | 349 | 0 |
| missed VBlank | 165 | 93 | **-72（-43.64%）** |
| complete renders | 238 | 100 | 改為穩定較低 cadence |
| forced renders | 104 | 52 | -52 |
| loop work 平均 | 224,781.42 cycles | 186,695.33 cycles | **-16.94%** |
| audio frame loss | 0 | 0 | 0 |
| RNG calls | 1,320 | 1,320 | 0 |
| 玩家子彈生成 | 1,981 | 1,981 | 0 |

這組數據來自 Severe 尚可降到三 tick 的歷史版本。它證明補強可作用於一般
高負載關卡，而非只對 lava／water 寫死特例；現行 release 已以遊玩流暢度
優先，把 Severe 封頂為兩 tick，因此不應直接拿這組 complete-render 數量
當作新版本預期值。

同一關不開火的 600-VBlank 回歸只有 1 次冷快取 missed VBlank，Adaptive entry 為 0、
結束時 pressure 為 0。全域開啟不等於全域固定低 FPS。

## 能力邊界

GBA 一個 LCD frame 約 280,896 cycles。若單一不可切割的 logic 或完整 render phase
本身已超過此值，Drop-frame／Adaptive 只能避免它和其他昂貴 phase 疊在同一個 LCD
期間，無法讓該 phase 本身不跨 VBlank。這類瓶頸仍應以 cache、DMA、工作分段或真正
hot-path 優化處理，不能藉由少算碰撞、少推進 RNG 或放慢關卡來製造假性改善。

## 診斷與驗證

標準全域策略：

```powershell
.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel normal `
  -Variant active_mask_fast_wall_lazy_packed `
  -Episode 2 -Section 1 -DurationVBlanks 600
```

只留 Drop-frame 的 A/B 診斷版：

```powershell
.\tools\run_full_loadout_stress.ps1 `
  -DetailLevel normal `
  -Variant active_mask_fast_wall_lazy_packed_no_adaptive `
  -Episode 2 -Section 1 -DurationVBlanks 600
```

SRAM telemetry 會記錄 entry、severe entry、exit、logic-busy defer、idle render、
safety-forced、結束時 tier 與 pressure。驗收時除了 missed VBlank，必須一起確認音訊
frame loss、logic updates、RNG、子彈／敵人更新等權威計數沒有因呈現策略而改變。
