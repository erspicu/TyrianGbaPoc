# Tyrian GBA v73：Lava／Water 自適應呈現派送實作報告

日期：2026-08-03
狀態：第一階段實作完成，具正面效益，尚未宣稱零 missed VBlank

## 目標與不變條件

這一階段只處理 OpenTyrian 來源資料真正啟用 lava／water smoothie 的場景。
負載過高時，可以少建構中間 presentation，但不得省略或改序下列權威狀態：

- gameplay fixed tick；
- 玩家／敵人／子彈運動與碰撞；
- RNG 呼叫與事件順序；
- 傷害、掉落、Boss 與關卡流程；
- Maxmod 的 physical-VBlank 音訊服務。

換句話說，這不是把遊戲邏輯改成 N tick 才算一次，而是把尚未來得及顯示的
中間 OAM、tile cache 與 VRAM scene 建構整批省略。畫面永遠保留上一張完整
scene，直到下一張完整 scene 可一起 commit。

## 實作

`Configure.h` 新增 `TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH`，正式設定預設為 1。
實際 gate 同時要求：

1. Detail Level 至少為 High；
2. 來源 `smoothies[0]` 或 `smoothies[1]` 啟用；
3. 當下不是會獨占 DMA0 的 spotlight special code 2；
4. 已觀察到持續 deadline pressure 與至少兩次真正 missed VBlank。

進入 adaptive 後：

- 有執行 source logic 的 LCD loop 優先 defer scene 建構；
- 沒有執行 logic 的 loop 優先完成最新 scene；
- 一般 scheduler 的兩個 pending source ticks 上限，在 wave 壓力期間放寬為三個；
- state transition、背景 ring 安全邊界或 freshness 上限仍強制 render；
- 連續 16 張完整 render 都低於 150,000 cycles，才判定壓力解除；
- 離開 lava／water scope 後回到原本 v72 scheduler。

舊程式會在尚未確定 render 前提早設定
`presentation_release_held_window`。現在改成只由 `render_game()` 在真正建立下一張
完整 scene 時釋放舊 window，defer 期間不會提前放掉仍由 PPU 顯示的背景 row。

## 互斥 cycle accounting

舊 `prelogic_cycles_total` 從 VBlank timestamp 算到 logic 前，因此包含 commit 與
audio/input，不能再跟它們相加。v73 新增：

- `loop_work_cycles_total`：每個 gameplay loop 只量一次的完整工作 envelope；
- `prefetch_cycles_total`：獨立的背景預取區段；
- commit、audio/input、logic、render、prefetch 是可相加的互斥子區段；
- `dispatch_and_other_cycles_total` 是 envelope 扣掉上述子區段後的差額。

最終極限測試中，互斥子區段覆蓋 loop work 的 94.56%；其餘 5.44% 是 scheduler、
輸入分派、telemetry 與其他小型工作，不再把重疊的 `prelogic` 誤稱成額外成本。

## 固定壓力測試結果

條件：Episode 4、Section 31、LVL 9 `LAVA EXIT`、position 3440、PENTIUM、
Normal Speed、無敵、最重完整武器、持續射擊。

| 指標 | v72 baseline | v73 adaptive | 差異 |
|---|---:|---:|---:|
| display frames | 5,908 | 5,908 | 0 |
| source logic updates | 3,440 | 3,440 | 0 |
| missed VBlank | 1,231 | 685 | **-546（-44.35%）** |
| forced render | 1,002 | 60 | -942 |
| complete render | 2,426 | 2,370 | -56 |
| audio frame loss | 4 | 2 | -2 |
| 玩家射擊生成 | 21,794 | 21,794 | 0 |
| 最大 active 玩家彈幕 | 81 | 81 | 0 |
| 最大 OAM | 128 | 128 | 0 |
| RNG calls | 7,581 | 7,581 | 0 |
| enemy motion updates | 14,006 | 14,006 | 0 |
| enemy-shot motion updates | 2,364 | 2,364 | 0 |

Adaptive telemetry：進入 1 次、退出 0 次、logic-busy defer 3,260 次、idle
complete render 2,323 次、安全強制 render 59 次。背景 ring 最高持有 26／32 rows，
`background_approximations` 仍為 0。

### 低負載與非 wave 回歸

- 同一 LAVA EXIT 路線不開火：missed VBlank 1、audio loss 0、adaptive entries 0。
- Episode 1 Section 1、600 VBlank 短測：lava/water/wave frames 全為 0，
  adaptive scope attempts／entries／defer 全為 0。

這證明新 dispatcher 不是全域降低更新率，也不會因單一冷 cache 峰值就在低負載
場景長時間鎖住。

## 為何仍不是 0 missed VBlank

滿火力下的完整 render 平均約 197,892 cycles，峰值約 363,000 cycles；碰撞單一
logic tick 峰值也約 310,000 cycles。只要某一個不可再切割的完整 phase 本身超過
280,896-cycle LCD frame，scheduler 即使把 logic 與 render 分開，也只能減少組合
超時，不能保證該 phase 不跨 VBlank。

因此 44.35% 改善是實測成立的第一階段成果，但「完全消除」仍需後續把 render
尾端工作切成可安全預建的 transaction，或繼續降低 Sprite／effect cache miss
成本。不能靠刪掉 gameplay tick、改 RNG 次序或讓碰撞少算來製造假零漏幀。

## 可重現資料

測試工具：`tools/run_full_loadout_stress.ps1`。v73 追加 `-NoFire`，並用獨立 target
名稱避免 fire／no-fire object 因 Make timestamp 而誤共用。

本機診斷輸出位於 `temp/wave_adaptive_v73/`；該目錄屬可重建、未納入 Git 的測試
產物。正式可維護內容是本報告、SRAM telemetry schema 與測試工具本身。
