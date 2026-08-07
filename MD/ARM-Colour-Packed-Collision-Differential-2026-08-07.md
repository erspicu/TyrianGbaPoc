# ARM 色距與 Packed 碰撞差分／效能報告（2026-08-07）

## 結論

四個指定入口均已有可切回 C oracle 的 ARM 版本：

- `gameplay_overlay_colour_distance`
- `source_detail_palette_distance`
- `ot_player_shot_axis_overlaps`
- `ot_level_port_collide_player_shot_packed`

所有差分測試皆通過。ASTEROID 1、CUSTOM、壓力武器、持續射擊的
固定 600 VBlank A/B 中，packed 碰撞區段平均週期降低 **6.75%**，
整體 logic 平均週期降低 **3.16%**；邏輯進度與主要狀態計數完全一致。

## 實作邊界

色距與 axis 是零 stack 的 leaf ARM 函式。Packed 版本由 ARM 負責
active-mask 掃描、候選順序、敵人位址、兩軸 strict AABB 與 miss
快路徑；真正 overlap 後才呼叫既有 source-parity mutation helper，
處理 linked group、damage transition、death spawn、reward、event jump
等低頻但龐大的遊戲規則。因此它不是只轉呼叫 C 的 wrapper，而是把
高頻 collision kernel 完整移到 ARM，同時避免複製兩份易失同步的規則。

關鍵改善是：同一個 32-slot mask word 內若只是 miss，ARM 保留尚未掃描
的 bits，不再為每個候選重讀及重建 mask；真正命中可能改動 enemy pool
時，立即丟棄暫存並從 authoritative mask 重讀，故不改變 PC 掃描順序。

結構欄位 offset 由 `gba_hotpath_layout.inc` 共用，C 端 `_Static_assert`
會在結構布局漂移時讓建置失敗，避免組語默默寫壞資料。

## Differential test 覆蓋

### 色距

`gameplay_overlay_colour_distance`：

- R/G/B 各自完整跑過 `32 × 32` 的所有 5-bit channel 配對，共 3,072 組。
- 再掃過全部 32,768 個 15-bit 顏色，搭配一個覆蓋完整色域的對色排列，
  驗證三 channel 累加與 bit extraction。

`source_detail_palette_distance`：

- R/G/B 各自完整跑過 `32 × 256` 的 GBA channel／來源 RGB byte 配對，
  共 24,576 組。
- 再掃過全部 32,768 個 15-bit 顏色與 deterministic RGB triple，驗證累加。

### Axis 與 packed collision

- Axis：12 個代表性 radius（含 0、正常遊戲值、255、32767、32768、65535）
  × 全部 65,536 個 `int16_t delta`，共 **786,432** 組逐值比對。
- Packed：9 組人工語意案例，涵蓋 miss、armor hit、255 armor、穿透多目標、
  linked kill、damaged graphic、ice/filter、strict radius、reward/event flag。
- Packed：另跑 128 組 deterministic randomized enemy pool；每組比較完整
  `OtLevelPortState` 與 `OtShotCollisionResult` 的逐 byte hash。
- 額外用 no-mask linear、active-mask fallback、unsigned-range、fast eager、
  fast lazy/source-parity 等組態驗證 generic 與正式 optimized kernel。

所有組態的 `level_port_asm_differential=3`、
`colour_distance_asm_differential=3`。

## ASTEROID 1 固定路徑 A/B

條件：Episode 1 / Section 5、CUSTOM、Normal speed、壓力武器、無敵、
持續射擊、adaptive/drop-frame 開啟、600 VBlank。

| 指標 | 純 C (`HOTPATH_ASM=0`) | ARM (`HOTPATH_ASM=1`) | 差異 |
|---|---:|---:|---:|
| 邏輯更新數 | 349 | 349 | 相同 |
| 地圖位置 | 698 | 698 | 相同 |
| Collision 平均 cycles | 86,444.89 | 80,610.24 | **-6.75%** |
| Logic 平均 cycles | 179,632.21 | 173,953.60 | **-3.16%** |
| 完成 render 平均 cycles | 364,333.16 | 363,148.60 | **-0.33%** |
| 完成 render 數 | 159 | 164 | **+5 frames** |
| Missed VBlank | 461 | 456 | **-1.08%** |
| Audio frame loss | 93 | 93 | 不變 |
| `.iwram` | 17,848 bytes | 17,632 bytes | **-216 bytes** |
| 實測 stack canary 餘量 | 5,440 bytes | 5,704 bytes | **+264 bytes** |

一致性計數包括：RNG calls 2,246、collision candidate visits 196,483、
mask rebuilds 348、enemy motion updates 2,860、player shot spawns 2,070、
active shots 78；兩版全部相同。ARM 版 render total cycles 較高，是因為
同一時間窗多完成 5 個 frame，不是單幀退化。

## Leaf microbenchmark

每項 16,384 calls，同一 ROM、IRQ 關閉、相同輸入：

| 函式 | C cycles | ARM cycles | 差異 |
|---|---:|---:|---:|
| Overlay colour distance | 2,383,346 | 2,383,377 | +0.0013%（等同雜訊） |
| Detail palette distance | 2,794,487 | 2,728,867 | **-2.35%** |
| Axis overlap | 1,183,579 | 1,183,633 | +0.0046%（等同雜訊） |
| MT19937 10,000 calls | 2,066,728 | 1,956,330 | **-5.34%** |

GCC 對 overlay 與 axis 已產生接近最佳的 ARM 指令，因此手寫版的價值是
明確 ABI、可測試的 bit-exact 實作，而不是虛報微小加速。正式收益主要來自
palette、RNG，以及能跨候選保留 mask word 的 packed collision kernel。

## ASTEROID 1 長時間卡點回歸

先前壓力版約在 map position 2,050 停住的 root cause 是 IWRAM 配置，
不是關卡事件或 mGBA：`source_process_player_collisions()` 是約 4 KiB 的
orchestration shell，壓力組態曾把整段放入 IWRAM，擠壓共用 user/IRQ stack，
事件尖峰時可能覆寫返回路徑。現在只保留真正 packed kernel 在 IWRAM，shell
回 ROM；cold 2 KiB decoder scratch 在壓力組態移到 EWRAM，組語各 kernel
亦採可獨立 GC 的 input section，避免未使用版本佔住 IWRAM。

修正後 C 與 ARM 版皆完成 3,600 VBlank、2,096 logic updates、map position
4,192；ARM stack canary 尚餘 5,704 bytes，C 版 5,440 bytes。這已跨過舊卡點
超過兩倍，故 BUG 判定修復，而不是單純把 timeout 往後延。
