# Tyrian GBA v73：動態工作量調節與零漏幀可行性研究

日期：2026-08-03
狀態：證據盤點與策略定案；本文件尚未宣稱已完成程式實作

## 結論

極限武器造成的 `20.836%` missed VBlank 已證明存在嚴重的工作聚集：logic、
collision、render 與 forced presentation 會集中在同一個 LCD period，產生超過
280,896 cycles 的尖峰。現有互斥量測至少涵蓋 wall-time cycle budget 的
`69.053%`；其餘 `30.947%` 混合了尚未獨立量測的 prefetch、loop overhead 與
真正 idle，不能直接全部視為可用 headroom。

因此 dynamic dispatch 很值得實測，也有機會把目前極限案例大幅改善；但現有
資料尚不能證明長期 throughput 一定足夠。在所有不可中斷 phase 的 measured
worst case 加 guard 都低於期限、且 backlog 長時間不成長前，不能宣稱這條路線
或全遊戲已獲得「零漏幀硬保證」。

推薦順序：

1. 先做小範圍的 accumulator-aware、row-ownership-aware dynamic scheduler A/B；
2. 再做保持 source slot order 的 conservative spatial broad phase；
3. 加上只影響 presentation 的 hysteretic quality ladder；
4. 若 render/collision 尖峰仍跨幀，才評估 bounded render jobs 或 cooperative
   logic phases；
5. 明確拒絕任意 N-tick authoritative logic skipping。

## 現行機制已經省略的工作

目前正式 scheduler 在 deadline 不足時，會在呼叫 `render_game()` 前 defer。
因此被省略的中間 scene 本來就不會執行：

- OAM scene construction；
- player projectile presentation cache acquire；
- enemy Sprite2 presentation assembly；
- scene 專屬 VRAM upload；
- 新 OAM commit。

上一張完整 OAM、三層背景 register／row ownership 與 blend state 會一起保留，
下一次只提交最新完整 scene。使用者提出的「N 與 N+2 之間不建立那張 OAM」
已經是現行基本方向，不能再把它當成尚未取得的主要收益。

可繼續改善的是：現行 pending scene 到兩個 logic ticks 便強制 render，未充分
利用實際 row ownership 和下一個 logic deadline。

## v72 證據

條件：Episode 4 Section 31 `LAVA EXIT`、PENTIUM、Normal Speed、完整最重
武器、無敵壓測、position 3440。

| 指標 | 數值 |
|---|---:|
| LCD/display frames | 5,908 |
| source logic updates | 3,440 |
| missed VBlank | 1,231（20.836%） |
| active player shots max | 81 |
| player shots generated | 21,794 |
| OAM max | 128 |
| render attempts/completed/deferred | 4,806 / 2,426 / 2,380 |
| forced render | 1,002 |
| superseded pending scenes | 1,013 |
| completed presentation rate | 24.526 Hz |
| actual held background rows max | 25 / 32 |
| logic average/max | 126,341 / 387,082 cycles |
| collision average/max | 52,216 / 309,952 cycles |
| completed render average/max | 184,916 / 360,954 cycles |
| audio+input average/max | 38,417 / 61,374 cycles |
| commit average/max | 7,681 / 83,763 cycles |

總 budget 與可互斥相加的已量測工作：

| 區段 | wall-time budget 佔比 |
|---|---:|
| audio/input | 13.667% |
| commit | 2.165% |
| logic | 26.189% |
| completed render | 27.032% |
| exclusive measured lower bound | 69.053% |
| 尚未獨立歸類的上限 | 30.947% |

`prelogic_cycles_total` 從 physical VBlank timestamp 算到 input 結束，已包含
commit 與 audio/input；recovery loop 也可能重複涵蓋 elapsed wall time。因此它
不能再與上述區段直接相加。後續必須新增 exclusive busy/idle/prefetch telemetry，
才能量出真正長期 headroom。

`5,908 - 3,440 = 2,468` 個 LCD slots 沒有 source logic update；實際完成
`2,426` 個 render。數量幾乎相同，支持「重負載時把 render 主動排入
logic-idle slots」的策略。

相同路線不射擊只有 1 次 missed VBlank，logic/render 平均為
42,534/65,703 cycles，forced render 只有 5 次，證明一般場景不需要固定退化。

## Root cause 分層

### 長期吞吐量

目前資料顯示有排程重整空間，但尚未證明平均吞吐量一定放得下。不能只看
`logic average + render average` 並假設兩者必須在同一 LCD period 完成；
34.7826 Hz logic 本來就會留下 nominal no-logic LCD slots，但其中還包含多少真正
CPU idle，必須由新 telemetry 回答。

### 排程聚集

1,002 次 forced render 佔 completed render 的 41.3%。固定 pending=2 門檻會讓
重 render 和 logic 疊在同一 slot，即使實際背景 held rows 峰值只有 25/32。

### 單一 phase 尖峰

logic max 387,082、collision max 309,952、render max 360,954 均可單獨跨過
280,896-cycle LCD budget。只改 scheduler 可以大幅改善平均表現，但不能消除這些
不可中斷尖峰。

### OAM

OAM=128 是畫面容量與壓力指標，不是最大 CPU root cause。先前診斷證明即使不畫
玩家 projectile，仍有大量 deadline miss；完全關閉 collision 的改善反而更大。

## 策略 A：動態分離 logic 與 presentation

### 不變量

- authoritative logic 仍按現有 34.7826 Hz numerator/denominator 推進；
- Maxmod 仍由 main loop 對每個 newly observed physical VBlank service；
- partial logic state 永不呈現；
- partial OAM/BG/cache scene 永不 commit；
- 新 scene 只能在 VBlank 原子提交；
- 壓力只影響 presentation cadence／quality，不影響 RNG、碰撞或關卡時間。

### 預看 accumulator

不可把 source tick 錯當成每 280,896 cycles 一次。scheduler 應直接預看現有
accumulator：

```c
logic_due_now =
    logic_accumulator >= TYRIAN_GBA_LOGIC_DENOMINATOR;
logic_due_next_lcd =
    logic_accumulator + TYRIAN_GBA_LOGIC_NUMERATOR >=
        TYRIAN_GBA_LOGIC_DENOMINATOR;
```

高壓時優先：

1. 先完成應到的 source logic；
2. 若本 LCD slot 執行過 logic，且 render estimate 無法安全容納，保留 scene；
3. 在下一個 logic-idle LCD slot 建立最新 presentation；
4. 若下一個 slot 又要到 logic，使用 estimate、scene age 與背景 ownership 決定
   是否等待下一個 idle slot；
5. 不再只因 pending count 到 2 就無條件忽略 deadline 強制 render。

### 三層背景安全條件

不能只用 camera Y 或固定「88 pixels」判斷。每一層必須同時驗證：

- last-presented viewport 所需 source-row interval；
- latest logic viewport 加 soft-camera retain/lookahead 的 interval；
- 兩者 union 是否能放入 32-row physical ring；
- `ring_source_row[]`／`ring_vram_source_row[]` 與 slot generation 不會覆寫
  PPU 仍可能取樣的 row；
- pending row upload capacity 足夠；
- semantic tile ownership／eviction 不會使 held scene 的 tile 失效；
- layer enable、事件跳躍、palette/filter 與 BG priority 仍以整張 scene 原子切換。

短期 A/B 可以保留最大 scene-age cap（例如 3 或 4 source ticks），但真正 force
條件應以每層實際 union/capacity 為主。若安全條件不成立且本 slot 又沒有 render
預算，長期解法是將 background materialization 與 logic position 解耦，而不是
覆寫 held rows 或修改遊戲捲動速度。

## Dynamic presentation tiers

使用 cycle estimate、deadline defer、scene age、row ownership、cache queue 和
collision pressure，配合不同進入／離開門檻：

| Tier | 行為 |
|---|---|
| Light | 每個 source scene 完整 PENTIUM；cache 允許時預取。 |
| Medium | logic/render phase-separated；鎖定穩定約 29.86 Hz presentation。 |
| High | 省略更多 intermediate presentation；降低 wave/filter 等純視覺更新幅度。 |
| Severe | 完全不建立無法安全提交的 OAM/cache scene，只保留最新 authoritative snapshot。 |

第一階段依使用者決定，只有來源 lava／water wave request active 時才啟用這套
adaptive dispatch；其他場景完全沿用 v72 scheduler。這會把 scheduler 回歸面限制
在已知問題場景。Spatial broad phase 若日後證明 source-exact，則可獨立成全域
最佳化，不永久綁在 wave gate。

進入 adaptive mode 必須鎖存在完整 committed scene 邊界；離開時則等待：

- wave request 已停止；
- 沒有 pending presentation 或未完成的 scene-specific staging job；
- 最新 authoritative scene 已完整 commit；
- 三層 row ownership 回到「只服務目前 committed scene」的一致狀態；
- 連續約 15～30 個乾淨 physical frames，且 backlog 為零。

不要求 row ownership 變成零，因為正常 committed background 本來就持有可見 rows。
mode transition 只要求 ownership 與 pending generation 一致。

建議進入 Medium/High 需連續數次壓力事件，返回 Light 則要求至少約 30 個乾淨
physical frames，避免 20/30/35 Hz 之間快速震盪。

不可在 High/Severe 停止 BG2 gameplay state、effect pool、particle TTL、聲音或
任何來源生成；只能不顯示其中間 presentation，或由當前 authoritative age 直接
選出下一個可見動畫 frame。

## 策略 C：source-order-preserving spatial broad phase

現行 active mask 只去除空 enemy slots，v72 仍有 923,875 candidate visits。
建議先量測 Y-band，再決定是否需要 2D cells。

### 初始資料配置

- 每 16 pixels 一個 conservative Y band；實際 band 數依 source enemy Y 合法範圍
  定義，不先假設固定 512-pixel 世界；
- 每 band 四個 `uint32_t`，涵蓋 100 enemy slots；
- 完整 band table 先放 EWRAM；每顆 shot 的四個 candidate words 放 registers；
- 是否值得用 512 bytes IWRAM，必須以 EWRAM/IWRAM A/B 與 stack gate 決定；
- 不預設 De Bruijn/CLZ 一定較快，沿用已驗證無 libcall 的 iterator 作基準。

### 每顆 shot 必須先 union

```c
candidate[0..3] = OR(all bands overlapped by shot AABB);
candidate[0..3] &= live_active_mask[0..3];
```

敵人跨多個 bands 仍只會在 candidate mask 出現一次。不可逐 band 直接呼叫 narrow
phase，否則同一 enemy 會重複受傷。

### Mutation 與 cursor

1. enemy movement phase 完成後建立/更新 spatial bands；
2. shot 仍依原 source 順序處理；
3. candidate 仍依 enemy slot 0..99 單向遞增；
4. spawn、kill、damaged remnant、move 與 pool reuse 立即更新 live active/spatial mask；
5. mutation generation 改變時，重新 query 本 shot 的 bands，只保留 `slot >= cursor`；
6. 新生成到較大 slot 且 spatial overlap 者，同一 penetrating shot 仍可看見；
7. 新生成或 reuse 到較小 slot 者，不回頭；
8. 最後仍執行原 narrow AABB、damage、linked death 與 reward 路徑。

這是 conservative broad phase：允許 false positive，絕不允許 false negative。

## 為何不採用一般 N-tick logic skipping

任意把 gameplay projectile 從 tick N 直接推到 N+2，會略過或改變：

- 中間 tick 的 moving-enemy collision；
- 穿透 damage 與 slot-order mutation；
- TTL／animation/fire cadence 邊界；
- homing、acceleration 與固定寬度 integer wrap；
- RNG calls、事件跳躍、linked destruction 與聲音 request sequence。

只有在形式化證明「無碰撞、無事件、無 TTL 邊界、無 RNG、無 overflow、純線性」
的極小子集合，closed-form free flight 才可能 exact；在 Tyrian gameplay 中通常不如
spatial broad phase 划算。

可以安全 N-tick 化的是 presentation：未提交的 OAM scene、純視覺動畫 frame 和
wave/filter 中間狀態可以省略，下一次直接由 authoritative tick/age 生成最新畫面。

## Render 尖峰的後續處理

即使 C 消除 collision 尖峰，render max 360,954 仍需處理：

1. 增加 render phase histogram 與 cause tag，區分 Sprite2 miss、projectile cache、
   background row decode、effect upload、OAM build；
2. 對通用 asset/cache 做關卡或事件前預熱；
3. 把可重用的 decode/upload 工作改成有 cycle budget 的 bounded jobs，安排於
   logic-idle slots；
4. staging job 必須帶 scene/version，authoritative state 更新後，過期的 scene-specific
   工作要丟棄；asset-only 工作則可繼續；
5. 最終 OAM、BG map/register 與 palette 仍只做 atomic commit。

完整 render pipeline cooperative slicing 的複雜度高於 scheduler A，必須等
histogram 證明是哪個不可中斷 job 造成尖峰後再拆。

## 驗證契約

### Authoritative parity

AUTOTEST build 對每個 source tick 建立 canonical state stream，至少涵蓋：

- level tick/position/event cursor；
- RNG state digest 與 call count；
- player position、energy、armor、weapons；
- 每個 enemy slot 的 availability、position、armor、link、animation/state；
- player/enemy projectile slots；
- effect/reward gameplay pool；
- cash/data cube/secret level/end-level state；
- audio request sequence。

使用 CRC32/FNV-1a rolling digest 搭配各 workload counters；不能使用會互相抵消的
簡單 XOR。baseline 與所有 dynamic tiers 必須逐 tick 相同。

### 新增 performance telemetry

- logic/collision/render/commit phase cycle histograms；
- 200k/240k/260k/deadline-over bins；
- render-on-logic-slot 與 render-on-idle-slot 計數；
- forced-render reason：scene age、row union、generation、pending uploads、deadline；
- 每層 held/union rows max 與 histogram；
- LCD slots between committed scenes；
- dynamic tier residence／transition count；
- broad-phase candidates before/after、mutation requery、false-positive narrow checks；
- cache/decode bounded-job max cycles 與 queue depth；
- audio frame loss、logic backlog 與 missed VBlank。

### v72 固定 workload 不可改變

- logic updates：3,440；
- player shots generated：21,794；
- max active shots：81；
- OAM max：128；
- RNG、enemy、collision hit/kill、reward、level position 與 audio requests 全部相同。

## Gemini 3.1 Pro 諮詢評估

三輪諮詢採模型預設 sampling 參數，沒有傳入 `--temperature` 或 `--max-tokens`。
第三輪已把 `prelogic` 非互斥、先前 77.27% 重複計數的問題交給模型更正；模型
同意把「0 missed VBlank 極有可能」降級為「值得實測、現有資料尚不能證明」。

採納：

- 平均吞吐量足夠時，先 phase-separate logic/render；
- A 小型 scheduler A/B 優先於大規模 C；
- EWRAM spatial bands、register candidate union、保序 mutation；
- stable cadence + hysteresis；
- exact broad phase 優於任意 N-tick gameplay skip；
- worst-case 未通過前不宣稱 0-miss guarantee。
- 第一階段只在 lava／water 期間啟用 adaptive dispatch，並在完整 scene 邊界切換。

修正或拒絕：

- camera Y 不能取代三層 row ownership；
- spatial mask 必須先 union，不能跨 band 重複 narrow phase；
- Maxmod 不是由 ISR 自動完成；
- OAM deferral 已存在；
- WIN0/WIN1 不會降低 OAM construction；
- source logic accumulator 不能用 LCD period 取代；
- 不能停止 BG2/effect gameplay generation；
- LDM/STM、De Bruijn、IWRAM placement 均需 objdump 與 A/B，不先假設較快。

原始 query/response 保存在：

`C:\ai_project\AprTyrianNes\knowledgebase\message`

## 下一階段建議

先只增加 telemetry 與 dynamic scheduler A/B，不同時改 collision：

1. 先新增 exclusive loop busy、true wait/idle、prefetch/overhead、logic/render backlog
   與 per-LCD-slot cycle histogram；
2. baseline v72；
3. 在 lava／water gate 內測 logic-idle render；
4. fixed pending=2 改為 per-layer row-ownership predicate + scene-age cap；
5. 加 stable 2-LCD-slot cadence/hysteresis；
6. 比對 forced render、missed VBlank、audio loss、scene interval、backlog 與逐 tick
   golden；
7. 若 logic 200k+ outlier 仍顯著，再實作 spatial broad phase C。

這個順序能以最小變因回答：目前 1,002 次 forced render 究竟能減少多少，以及
collision 尖峰是否仍是剩餘漏幀的主因。
