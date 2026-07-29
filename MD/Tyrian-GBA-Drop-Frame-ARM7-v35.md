# Tyrian GBA v35：Fixed-timestep Drop-frame 與 ARM7 Hot Path

日期：2026-07-27

分支：`opentyrian-source-parity-port`

狀態：實作、單變量 A/B、完整回歸與可玩 ROM 均完成

## 結論

Pentium Detail、Normal Speed、81 發玩家彈幕、128 OAM、六套最重武器
無限同時發射仍超過 GBA 的穩定滿幀能力；這是刻意建立的硬體上限，
不是正式裝備配置。

v35 保留 OpenTyrian gameplay 時間軸與完整 Maxmod BGM，只動態省略來不及
完成的 presentation。相同 3,600 個實體 LCD periods、2,096 次 source
logic 與完全相同武器／碰撞工作量下：

- missed presentation periods：719 → 590，下降 17.94%；
- Maxmod `mmFrame()`：2,881 → 3,600，恢復為 3,600／3,600；
- 成功產生的新 scene：1,381 → 1,470，增加 6.44%；
- 極端負載的有效新 scene rate：約 22.91 → 24.39 Hz；
- logic backlog：0，單一主迴圈最多只需 1 次 logic update；
- unknown visuals、background approximation、Sprite2 L2 drop：全部 0。

因此 missed-VBlank recovery 與 whole-scene presentation defer 值得保留。
它們沒有讓極限配置變成滿幀，而是讓無法滿幀時仍保持正確遊戲節奏、
音樂更新與完整場景一致性。

## 使用者提出的 branchless 程式評估

### Enemy shot update

提出的 unsigned bounds 與 player AABB 寫法，在目前 `int16_t` 座標範圍
內可保持原始整數邊界：

- X 存活範圍仍為 `1..275`；
- Y 存活範圍仍為 `-13..190`；
- 玩家命中範圍仍為 X `-10..10`、Y `-13..13`。

`dir * velocity < target` 也可表達原始正向／反向追蹤條件，但 ARM7TDMI
的實際結果不較快：

| 版本 | missed／3,600 | logic avg cycles |
|---|---:|---:|
| 原控制流，搬入 ARM/IWRAM 前 | 1,388 | 123,225.83 |
| `dir * velocity` branchless | 1,394 | 123,804.46 |
| 原控制流，ARM/IWRAM | **1,382** | **122,672.64** |

反組譯中的 `dir * velocity` 產生真正的 ARM multiply；ARM state 原寫法
可使用便宜的 conditional execution。最終只保留函式搬入 ARM/IWRAM，
不保留乘法版。

`(animate + 1) % animax` 不採用。ARM7TDMI 沒有除法指令，runtime divisor
會落入 libgcc division/modulo 路徑；原本 `++animate` 後 compare/reset
明顯較便宜。

### Player-shot collision

提出的 unsigned AABB：

```text
(uint16_t)(delta + radius) <= radius * 2
```

會接受 `delta == -radius` 與 `delta == +radius`。OpenTyrian 原條件是
`abs(delta) < radius`，兩端都不接受，因此會多出一圈 1-pixel 命中
邊界。若要保留 strict 語意，unsigned interval 必須改成長度
`2 * radius - 1` 的區間。

更重要的是，使用 bitwise `&`／`|` 會強制計算 Y 軸。實際 trace 中大部分
candidate 在 X 軸就 miss，保留 `&&` short-circuit 反而大幅節省工作：

| 版本 | missed／3,600 | logic avg | collision avg |
|---|---:|---:|---:|
| fast mask + lazy result | 1,388 | 123,225.83 | 49,989.60 |
| 全 branchless geometry + bitwise axes | 1,440 | 130,025.18 | 56,786.04 |

collision 平均增加 13.60%，因此否決。只用 mask 選 radius、恢復 `&&`
時 collision 約 49,595.88 cycles，僅改善 0.79%，但 end-to-end scheduler
結果不穩定且較差，也不作預設。

其他語意注意：

- `if (!result || !state || !damage) return` 若先返回，會留下舊的
  `result->collided`；現有 API 對有效 result 保證 miss 時寫入 false。
- 每次先清除完整 result 會取消 v35 的 hit-only lazy initialization；
  81-shot trace 中 miss 佔絕大多數。
- `target_link` branchless mask 只在少量真實 hit 後執行，收益可忽略。
- `(!edamaged) ^ (edani < 0)` 保留 source XOR 語意即可；不能假設 C
  寫法必然只編成一個 N-flag 判斷。

結論是：branchless 不是 ARM7 的通用加速器。資料分布、short-circuit、
ARM conditional execution 與實際 objdump／cycle A/B 比語法外觀重要。

## 保留的 ARM7 最佳化

### 1. IWRAM／ARM placement

保留：

- `source_enemy_cache_acquire()`：ARM/IWRAM；
- Sprite2 raw writer 與 palette pack：ARM/IWRAM；
- player-shot collision／overlap：ARM/IWRAM；
- `ot_level_port_update_enemy_shots()`：ARM/IWRAM。

`ot_sprite2_frame_decode()` 不搬入 IWRAM。完整 Sprite2 bank 已在 build
階段無損展開，62-section matrix 與正式 route 的 runtime RLE fallback
均為 0；把不可達 debug fallback 搬入 32 KiB IWRAM 沒有收益。

### 2. 32-bit palette packing

Sprite2 L2 miss 直接讀 ROM raw index，每四個 pixel 在 ARM registers
組成一個 `uint32_t`，再以 32-bit store 寫入 EWRAM。12×14 component
依 tile 對齊分三種：

- row offset 0：兩個本 tile word 加一個相鄰 tile word；
- row offset 4：一個本 tile word加兩個相鄰 tile words；
- row offset 2：兩個 aligned words 搭配兩個 aligned halfwords。

這保留 tile order、filter 與 palette mapping，並避免未對齊 32-bit
存取。舊 RLE byte path只保留為驗證 fallback。

### 3. Collision fast mask 與 lazy result

- 每個 collision phase 從 authoritative `enemy_avail[]` 建立 live mask；
- masked build 不再逐 candidate 重查 fallback mode 或 `enemy_avail`；
- no-hit 只寫 `collided=false`；
- 第一次真實 hit 才初始化 consumed、damage、counts 與 awards。

最終舊 scheduler 固定工作量的 collision 平均約 49,989 cycles；v34
active-mask 版本為 58,875 cycles。

### 4. 沒有假 SIMD

ARM7TDMI 沒有 NEON、DSP halfword SIMD、CLZ 或 data cache。跨 halfword
的 SWAR compare 會遇到 lane borrow；目前 iterator 也沒有
`__ctzsi2`／`__clzsi2` 呼叫，所以未加入 De Bruijn table。只有能在
objdump 與整體 A/B 證明收益的轉換才保留。

## Fixed-timestep presentation scheduler

### 時間軸

- LCD／Maxmod：59.7275 Hz；
- OpenTyrian Normal logic：34.7826 Hz；
- logic accumulator 由實際 VBlank periods 推進；
- 關卡事件、敵人、武器 cadence、碰撞與 RNG 不因畫面省略而減速；
- 中間 scene 不補畫，下一次有預算時直接顯示最新 authoritative state。

### Deadline 判斷

Timer 2/3 建立 32-bit cycle counter。render 預估使用：

- render cycle EWMA；
- EWMA absolute deviation；
- 8,192-cycle guard；
- 60,000..200,000 cycle envelope。

若當前 VBlank-to-deadline 餘額不足，完整 `render_game()` 延後。這不是
刪除個別 projectile，也不改 gameplay pool。

### Whole-scene coherence

只重複 OAM、讓 BG 使用最新 scroll，會造成 sprite 相對背景滑動；實測
後已否決。現在同時保留：

- 最後成功 scene 的 OAM；
- BG HOFS／VOFS、CNT、blend registers；
- last-presented 與 current logic 的背景 row ownership union。

最多保留 22 rows；pending 達兩個 logic ticks 或超出安全 ownership
便強制 render。BG、enemy、projectile、effect 與 OAM 以一個完整 scene
一起前進。

## VBlank／Maxmod recovery

Maxmod 原始 header 明確要求 `mmFrame()` 每 frame 呼叫；`mmVBlank()` 只
處理 DMA buffer 交換，不能代替 tracker／mixer update。

舊流程在重負載跨過 VBlank 後仍呼叫 BIOS `VBlankIntrWait()`。SWI 5 會
丟棄已 latch 的 IRQ，再等待下一個 VBlank，因此：

- 每次 overrun 會額外睡掉一個 LCD period；
- 3,600 wall periods 只執行 2,881 次 `mmFrame()`；
- module tempo／buffer producer 會逐步落後。

v35 由 IRQ counter 逐一消費 period：

1. 若尚無 overdue IRQ，使用 `IntrWait(0, IRQ_VBLANK)`；它不丟棄 race
   window 內剛到的 IRQ。
2. 若 IRQ counter 已領先，每次立即做一個 recovery loop。
3. recovery loop 執行 `mmFrame()`、input 與 fixed-time logic。
4. active display 期間絕不做 VRAM／OAM DMA；最新完整 scene 留到真正
   waited VBlank 原子 commit。
5. 若 logic 執行途中又有 VBlank 到達，本輪不啟動 render。

IRQ-end cycle timestamp 用作真正的 LCD deadline；不能把 recovery loop
開始時間誤當成新 frame 起點。

這不能修復已經播放過去的單次 audio underrun，但可防止 tracker 時間
持續落後，也避免 BIOS 再無條件浪費下一幀。

## 最終固定工作量 A/B

兩個 ROM 都是 Episode 2 第一關、Pentium、Normal、3,600 wall VBlanks：

| 指標 | Whole-scene，無 recovery | Whole-scene + recovery |
|---|---:|---:|
| logic updates | 2,096 | 2,096 |
| missed VBlank | 719 | **590** |
| missed 比率 | 19.97% | **16.39%** |
| main/audio frames | 2,881 | **3,600** |
| fresh VBlank commits | 2,881 | 3,010 |
| completed new scenes | 1,381 | **1,470** |
| effective new-scene rate | 22.91 Hz | **24.39 Hz** |
| logic avg cycles | 138,825.28 | **126,734.70** |
| collision avg cycles | 50,001.76 | **49,996.76** |
| render avg／completed | 135,040.93 | 136,046.19 |
| catch-up updates | 62 | **0** |
| max logic updates／loop | 3 | **1** |
| logic backlog max | 0 | 0 |

相同 gameplay workload：

- shot spawn／drop：12,374／639；
- max active shots：81；
- chain volleys：253；
- collision mask rebuilds：2,095；
- collision candidate visits：639,111；
- final level position：2,246；
- max OAM：128；
- max active pickup explosions：65；
- max visible enemies：16。

recovery 版成功 render 更多 scene，因此 presentation-only cache acquire、
eviction 與 capacity-drop 計數也較多；這是多顯示 89 個 scene 的成本，
不是 source projectile 或 collision workload 改變。

## Gemini／Knowledgebase 評估

本階段保存七份 query／response／follow-up 於：

`C:\ai_project\AprTyrianNes\knowledgebase\message`

採納：

- fixed timestep／variable presentation 的大方向；
- whole-scene consistency；
- end-to-end deadline telemetry；
- 先 objdump、再單變量 A/B；
- 不切割 `mmFrame()`。

否決或修正：

- OBJ cache 不需要 tombstone，slot 只在 render phase 換代；
- 專案已有 shadow OAM，不建立第二份；
- GBA Timer 2/3 不會因 DMA 自動停止；
- 134-byte `OtEnemy` AoS 不適合盲目 LDM／SoA；
- active iterator 沒有 ctz libcall，不需要 De Bruijn；
- BG delta ring 不能只保存 row bytes：它還牽涉 tile cache ownership、
  eviction 與三層 scene 的 atomicity，且 telemetry 未證明 22-row
  ownership 是 590 次 miss 的主因；
- group-death 不能重用只含 availability 0 的 active mask，因為 source
  還會處理 availability 2 damaged remnants。

諮詢只作候選產生器；沒有量測與本地 source 驗證的建議不直接落地。

## 音訊取捨

full-loadout build 已把密集武器 SFX 映射到一個 GBA 原生 PSG square
channel；BGM 仍為完整 Maxmod module。先前 Maxmod-SFX／PSG-SFX A/B
只差 31／3,600 missed periods，所以沒有靜音或犧牲 BGM 品質。

把 TYM/LDS 真正改寫成四聲道 DMG PSG sequencer 是獨立音訊移植工作，
不是安全的 hot-path 小修改；若日後需要，可用 TyrianAudioLab 的 GameBoy
映射作輸入，但應另外做音準、節奏與 CPU A/B。

## 完整回歸

`build.ps1 -KeepIntermediates -DetailLevel low -GameSpeed normal`：

- gameplay／Boss golden：PASS；
- death／Game Over／回前端：PASS；
- 41-song Jukebox：PASS；
- ROMFS／Sprite2 matrix：62／62；
- Episode 1 campaign：4／4；
- Episode 2 route：3／10,475 missed VBlank；
- unknown visuals、background approximation、stream/cache drops：0；
- mGBA runtime errors：0。

記憶體：

| Build | EWRAM free | IWRAM free |
|---|---:|---:|
| Low release | 49,384 | 8,392 |
| Episode 1 campaign test | 49,240 | 8,144 |
| Pentium recovery stress | 42,424 | 8,128 |
| Pentium full-loadout playable | 42,424 | 8,080 |

一般 build 保持 48 KiB EWRAM／6 KiB IWRAM gate；極端壓力版擴大 pools，
但 IWRAM 仍高於 6 KiB。

## 可手動測試 ROM

`build/tyrian_gba_full_loadout_playable_v35_detail_pentium_speed_normal.gba`

- bytes：14,211,328；
- SHA-256：
  `6d157ec0d4fdce0e8b392d1e4d616c08c722eaa2842386fcc71801070da7fbe2`；
- title／game code：`TYR FULL ARM`／`TYGP`；
- mGBA software renderer 600-frame boot：PASS（`AGB-TYGP`）；
- Detail：Pentium；
- Game Speed：Normal；
- 開發無敵：開啟。

按住 `A` 或 `B` 會同時啟動六套最重武器。這個 ROM 保留不可滿幀的
硬體極限，供觀察固定遊戲節奏下的動態 presentation。

## 下一步

1. 恢復 stock ammo、charge、cooldown 與裝備互斥。
2. 建立合法 front／rear／sidekick／special 組合的效能矩陣。
3. 對合法組合訂定 missed VBlank、OAM cull 與 visible cache capacity
   上限。
4. 只有合法組合仍失敗時，才評估更複雜的 cache ownership 或 PSG BGM。
5. full-loadout ROM 繼續作上限研究，不把 590／3,600 當正式發行目標。
