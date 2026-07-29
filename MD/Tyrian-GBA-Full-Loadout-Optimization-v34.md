# Tyrian GBA v34：全武器碰撞瓶頸與安全最佳化

日期：2026-07-27

分支：`opentyrian-source-parity-port`

狀態：實作、A/B、完整回歸與可玩 ROM 均完成

## 結論

Pentium Detail、六套最重武器無限同時發射仍超出 GBA 可穩定維持的
負載，本版沒有宣稱已解決這個刻意建立的極限案例。

不過，實測已把主要瓶頸從「可能是 OAM／音訊／圖形」收斂為玩家彈幕
碰撞，並在完全相同的遊戲工作量下得到：

- collision 平均：117,750.88 → 58,875.21 cycles，下降 50.0%；
- logic 平均：189,792.19 → 131,671.27 cycles，下降 30.6%；
- missed VBlank：2,086 → 1,370／3,600，下降 34.3%；
- shot spawn、drop、chain volley、敵人、爆炸與 OAM 峰值完全相同。

因此這些修改值得保留。它們不能讓「六套最重武器無限全開」滿幀，
但能讓正式遊戲在 stock ammo、cooldown、裝備互斥與正常存活時間下
容納更多高負載武器組合。

## Gemini／Knowledgebase 諮詢

本階段使用：

`C:\ai_project\AprTyrianNes\knowledgebase\gemini_query.py`

向 `gemini-3.1-pro-preview` 提供 v33 telemetry、GBA memory budget、
source-parity 限制與死亡生成物的 mutation 語意。保存的原始紀錄：

- `TyrianGbaPoc-v34-full-loadout-performance-query-2026-07-27.md`
- `TyrianGbaPoc-v34-full-loadout-performance-response-2026-07-27.md`
- `TyrianGbaPoc-v34-full-loadout-performance-followup-2026-07-27.md`
- `TyrianGbaPoc-v34-full-loadout-performance-followup-response-2026-07-27.md`
- `TyrianGbaPoc-v34-gemini-evaluation-email-2026-07-27.md`

第一輪回答不能直接照做：

- 把 `enemy_avail` 的 0／1 極性理解反了；
- 沒有證據就把所有 cache drops 視為不可見；
- 沒處理 collision 途中死亡物生成／pool 重用；
- render command 排序可能破壞 OpenTyrian draw／OAM order。

追問並指出限制後，第二輪建議改為：

1. 先用 no-collision／no-render／pre-cache-cull 診斷；
2. 記錄離屏、OAM-full、post-visibility 與真正 capacity drops；
3. 只有在 A/B 證明 collision 是主因後，才建立 mutation-safe active
   index；
4. unified cache 只在可見 capacity 壓力被實測證實後再評估。

本版採用前三項。沒有因諮詢而降低 BGM 品質，也沒有直接實作可能改變
source order 的 command sort。

## 瓶頸定案

初始 Pentium full-loadout 固定 3,600 display frames：

| 診斷 | missed VBlank | logic avg | render avg | collision avg |
|---|---:|---:|---:|---:|
| v33 原始滿載 | 2,111 | 205,801.49 | 140,853.46 | 133,883.61 |
| 關閉 player collision | 470 | 80,438.62 | 127,402.85 | 36.12 |
| 關閉 player projectile render | 1,591 | 205,800.63 | 59,532.05 | 133,883.17 |

即使完全不畫玩家彈幕，仍有 44.2% display frames missed；關閉碰撞則
降到 13.1%。所以：

- OAM=128 是畫面完整度限制，但不是最大 CPU 原因；
- projectile render／cache 是第二大成本；
- 最大成本是最多 81 發 active shots 對 100-slot enemy pool 的重複
  collision scan。

音訊前一階段的 Maxmod／PSG A/B 只差 31／3,600 frames，因此保留完整
BGM，不以 Game Boy PSG 模式掩蓋碰撞問題。

## 實作

### 1. Pre-cache presentation cull

玩家與敵方 projectile 在 cache acquire 前先判斷：

- 是否已在 240×160 最終裁切範圍外；
- OAM 是否已滿；
- 通過可見性後是否真的遇到 cache capacity。

碰撞仍在 264×184 source 座標進行，沒有刪除離屏 projectile gameplay。

同一工作量下：

- render：141,577.22 → 133,697.90 cycles，下降 5.6%；
- projectile cache drops：7,015 → 3,069，下降 56.3%；
- 提前裁掉 27,546 次離屏與 1,465 次 OAM-full cache request；
- missed VBlank 只少 18，證明它有正面效益但不是主解。

### 2. Mutation-safe active mask

每個 player-shot collision phase 只掃一次 100-slot `enemy_avail[]`，
建立四個 `uint32_t` bit words。每一發子彈依 slot index 由小到大只
走 active bits。

這不是新的 gameplay authority：

- `enemy_avail[]` 仍是唯一真實狀態；
- mask 每個 collision phase 重建；
- `ot_new_enemy_with_definition()` 立刻加入新 bit；
- `ot_release_enemy()` 立刻清除 bit；
- availability 轉成 2 的 damaged remnant 立刻清除 active bit。

iterator 每處理一個候選後重新讀 live mask，且 cursor 保存下一個數字
slot。因而保留原始 `for (index=0; index<100; index++)` 的 mutation
語意：

- 死亡生成物若配置到較大的 slot，同一發可穿透子彈仍可在後面命中；
- 若 pool 重用較小的 slot，該發不會回頭重複命中；
- 下一發會看到所有當下 active slots。

壓測共建立 2,095 次 mask、拜訪 639,111 個真實候選。

### 3. 不清除未使用的 effect arrays

`OtShotCollisionResult` 有 16 格 effect array。舊路徑每一發都用 aggregate
zero 清掉整個 struct，但 consumer 只讀 `0..effect_count-1`。

現在只初始化 scalar 欄位與 count；`OtPlayerCollisionResult` 的兩個
length-delimited arrays 同樣處理。這在 81-shot 壓力下消除每 tick
數 KiB 的無效 stack 寫入，不改任何可見資料。

### 4. Hot／cold code placement

- 高頻 player collision dispatcher 在 full-loadout build 使用
  ARM/IWRAM；
- chain-weapon overlap query 保留 ARM/IWRAM；
- 大型、低頻的 group-death 與 damaged-transition 分支禁止 inline，
  留在預取 ROM；
- 一般 build 將 background tile key 留在 IWRAM；
- full-loadout build 為保留至少 6 KiB stack／IWRAM 餘裕，把該 key
  留在預取 ROM。

曾測試把 dispatcher、result handler 與更多函式全部搬入 IWRAM。
速度更快，但壓力 ROM 只剩約 1.8 KiB，低於安全線，因此已撤回。
Thumb/IWRAM 折衷也較慢且曾跌到 5,976 bytes，亦未保留。

### 5. 可重複 telemetry

新增：

- Timer 2/3 32-bit cycle counter；
- logic／render／collision total 與 max；
- offscreen／OAM-full pre-cache cull；
- post-visibility acquire／visible capacity drop；
- projectile cache hit／miss／eviction／upload；
- collision mask rebuild／candidate visits；
- baseline、no-collision、no-render、precache-cull、active-mask build variants；
- `tools/run_full_loadout_stress.ps1`。

## 最終 A/B

三個 build 使用完全相同的 Pentium Detail、Normal Speed、Episode 2
第一關、3,600 display frames 與全武器輸入：

| 指標 | Baseline | Pre-cache cull | Active mask + cull |
|---|---:|---:|---:|
| missed VBlank | 2,104 | 2,086 | **1,370** |
| missed 比率 | 58.44% | 57.94% | **38.06%** |
| logic avg cycles | 189,793.37 | 189,792.19 | **131,671.27** |
| render avg cycles | 141,577.22 | 133,697.90 | **133,629.94** |
| collision avg cycles | 117,750.97 | 117,750.88 | **58,875.21** |
| logic max cycles | 404,964 | 404,964 | 421,687 |
| collision max cycles | 323,040 | 323,040 | 337,269 |
| projectile cache drops | 7,015 | 3,069 | 3,069 |

三者的 workload 均為：

- player shot spawn／drop：12,374／639；
- max active player shots：81；
- chain volleys：253；
- max OAM：128；
- max active explosions：65；
- max visible enemies：16；
- enemy Sprite2 cache drops：881；
- Sprite2 L2 drops：0；
- unknown visuals／background approximations：0。

平均 logic + render 已由 pre-cache 版的 323,490 降到 265,301 cycles，
低於約 280,896 cycles 的單一 59.7 Hz display-frame 預算；但 workload
分布有尖峰，最壞 tick 仍超過預算，所以 38.06% missed 並未消失。
active-mask 的最壞 tick 也仍比 baseline 高約 4%，這是後續若要追求
極限時應優先觀察的區段。

## 一般遊戲回歸

`build.ps1 -KeepIntermediates -DetailLevel low -GameSpeed normal`：

- TGBA gameplay／Boss golden：PASS；
- death／Game Over／回前端：PASS；
- 41-song Jukebox：PASS；
- ROMFS／Sprite2 matrix：62／62 sections；
- Episode 1 campaign：4／4 levels；
- Episode 2 第一關：3／10,475 missed VBlank；
- unknown visuals、background approximations、cache drops：0；
- mGBA runtime errors：0。

Pre-cache cull 讓一般第一關 projectile cache miss 由 150 降到 113；
Sprite2 upload、Boss window 與其他 gameplay golden 不變。永久測試仍用
精確的新數值，不是放寬條件。

記憶體：

| Build | EWRAM free | IWRAM free |
|---|---:|---:|
| Low release | 49,384 | 8,864 |
| Episode 1 campaign test | 49,240 | 8,616 |
| Pentium active-mask stress | 44,992 | 6,488 |
| Pentium full-loadout playable | 44,992 | 6,432 |

一般 release／campaign 通過既有 48 KiB EWRAM、6 KiB IWRAM gate。
full-loadout 是額外擴大 shot／effect pools 的上限研究 ROM；其 EWRAM
與 v33 相同量級，不是一般 release 配置。

## 可手動測試 ROM

`build/tyrian_gba_full_loadout_playable_v34_detail_pentium_speed_normal.gba`

- bytes：14,209,192；
- SHA-256：
  `103ac366db731cd84d400bf58c52e23b6c0ecd5369ab48b21f803236ee4d259e`；
- title／game code：`TYR FULL ARM`／`TYGP`；
- Detail：Pentium；
- Game Speed：Normal；
- 開發無敵：開啟。

按住 `A` 或 `B` 仍會同時啟動六套最重系統。這個 ROM 的目的就是保留
不可滿幀的硬體上限，方便與 v33 直接比較。

## 尚未做與下一步

本階段沒有：

- 用刪武器或降 BGM 品質換取漂亮數字；
- 改變 enemy／projectile source collision；
- 建立 GBA-only weapon 或 per-level 資料；
- 實作 render-command sort；
- 在沒有完整所有權規則前合併 enemy／projectile／effect VRAM cache。

若要定義正式遊戲可用範圍，下一個測試應改成「合法裝備矩陣」：

1. 恢復 stock ammo、cooldown、charge 與 equipment mutual exclusion；
2. 逐組測 front／rear／sidekick／special 的實際組合；
3. 以 missed VBlank、OAM cull、visible cache capacity drop 與最壞
   collision tick 建立 allow／limit 表；
4. 只有合法組合仍出現大量 visible capacity drop，才評估動態共用
   OBJ tile cache；
5. 最後才移除或限制少數無法在 GBA 硬體上成立的組合。
