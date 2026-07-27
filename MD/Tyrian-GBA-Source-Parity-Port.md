# Tyrian GBA 第一關原始碼直譯移植

## 目標與狀態

工作分支：

```text
opentyrian-source-parity-port
```

行為基準固定為：

```text
OpenTyrian commit
1c34d1bddac8c8f2de834229d04b5a729525c944
```

本分支的目標不是繼續修飾既有 POC，而是把進入第一關之後的程式依
OpenTyrian C 原始碼逐段直譯。PC 版本不需要另外編譯；原始碼及資料檔
作為唯讀依據，C# 版只協助理解，不作為第二套混合規則。

Stage 4 已讓 source runtime 成為第一關本體的唯一 gameplay authority。
`JE_eventSystem()`、`JE_makeEnemy()`、四組 25-entry pool、
`JE_drawEnemy()` movement／animation／release／fire、60-entry enemy-shot
pool、玩家彈碰撞、armor／damaged transition、linked death、`eenemydie`
子物件、score item 與 cash 均直接使用 ROMFS 的原始 LVL/HDT 資料。
GBA 端只保留輸入、PC 264×184 gameplay viewport 的 240×160 中央裁切、
BG/OAM、音訊及效果呈現。敵人、玩家、彈幕、碰撞與三層背景都留在 PC
座標；不再用 GBA 尺寸反推或縮放 gameplay state。
`curLoc=5400` 後仍切到既有簡化 Boss，尚不能宣稱 Boss source parity。

## 已鎖定的顯示與操作規格

OpenTyrian 的邏輯 framebuffer 是 320×200；以 4:3 像素比例顯示時，外觀
接近 320×240。原始遊戲區為 264×184：

| 區域 | 尺寸 | GBA 規格 |
|---|---:|---|
| Gameplay viewport | 264×184 | 1:1 中央裁成 240×160；四邊各捨去 12 px |
| 右側 HUD | 56×184 | 不移植 |
| 下方訊息 banner | 320×16 | 不移植 |

右側 HUD 不顯示，但裝甲、護盾、能源、武器狀態等若會影響 game loop，
內部計算仍保留。第一關驗證必要的 `PAUSED`、Boss 血條及金額回饋保留。

操作只包含：

- `Start`：開場進入第一關；遊戲中暫停／繼續
- D-pad：移動
- `A`／`B`：射擊

不移植 ESC 設定介面、遊戲內設定選單、商店、裝備選擇、關卡選擇、
存讀檔及第一關以外的 episode 流程。

## 原始資料讀取

v16 不再產生或嵌入 `opentyrian_level1_events.bin`、
`opentyrian_level1_enemies.bin` 或舊 `level_events.bin`。
`src/opentyrian_data.c` 直接對 ROMFS 內的 stock 檔案建立唯讀 view：

- `tyrian1.lvl`：第一關 1,009 筆 `JE_EventRecType`、level enemy 清單、
  三層 map lookup 與 map bytes。
- `tyrian.hdt`：完整 781 筆 weapon 與 851 筆 enemy definition。

第一關事件仍是 11-byte little-endian record；欄位順序與
`src/varz.h` 一致：

```text
eventtime, eventtype, eventdat, eventdat2,
eventdat3, eventdat5, eventdat6, eventdat4
```

已驗證的 ROMFS 定位：

| 資料 | Runtime 定位 |
|---|---:|
| `tyrian1.lvl` level count | 37 |
| 第一關 `lvlFileNum`／offset index | 9／16 |
| 第一關 section | 221,628..255,121 |
| Level enemy IDs／events | 7／1,009 |
| `tyrian.hdt` item data start | 16,465 |
| HDT weapon table start | 16,479 |
| HDT enemy table start | 88,130 |
| HDT enemy table | 851 × 77 bytes，結尾恰為檔案 EOF |

`tools/build_assets.py` 仍會在 host 端計算第一關 110 筆 transitive enemy
dependency closure，僅用於稽核 `elaunchtype` 與 `eenemydie`；不再輸出
runtime blob。完整欄位稽核在：

```text
res/opentyrian_level1_source_audit.txt
```

## Stage 1 直譯程式（歷史里程碑）

`src/opentyrian_level_port.c` 已建立：

- `OtEventRecord`：固定寬度的 `JE_EventRecType`
- `OtEnemyDefinition`：固定寬度的 `JE_EnemyDat`
- 原始 event 與 enemy ROM table reader
- 第一關 `JE_main()` 狀態初始化
- `JE_eventSystem()` 中不依賴 SDL 或 entity pool 的第一批 case
- 原始欄位 signedness 與 8/16-bit 寬度 static assertions

Stage 1 當時直譯的事件 case 包含背景速度、shape bank、星空開關、敵人總開關、
前後景順序、音樂 fade、新曲狀態、關卡結束準備、持續傷害旗標及 Boss
bar link 等純狀態操作。

當時尚未直譯的 spawn、敵人控制、碰撞及玩家相關 case 不產生近似結果；
它們只增加 `deferred_event_count`，並由當時的 legacy runtime 執行。

Shadow runtime 每個 legacy logic tick 都以同一 `curLoc` 讀取原始事件。
Stage 1 完整 auto-test 在進 Boss 前實際讀取 878 筆原始事件，其中 17 筆
已由直譯 case 處理，861 筆明確標記 deferred。此數字會在每次把一個事件
家族切換成直譯實作後重新驗證；applied + deferred 必須永遠等於已讀事件
數，避免事件無聲遺失。

## Stage 1 驗證

2026-07-26 以 ARM GCC 16.1.0 及 mGBA 0.11.0 完成全流程測試：

| 項目 | 結果 |
|---|---:|
| Auto-test | PASS |
| Release ROM | 677,136 bytes（661.27 KiB） |
| Release SHA-256 | `d09020c9d969e96d177b69cdc15b30e4ccc5ceb96341a89c2c9f2741653d38f0` |
| 原始事件已讀／applied／deferred | 878／17／861 |
| Legacy logic updates | 7,092 |
| Missed VBlank | 0 |
| Runtime errors | 0 |

該次完整數值記錄於本節。現在預設建置會清除可重建的 verification 檔；
需要保留時使用 `.\build.ps1 -KeepIntermediates`。既有 v11 的 414 spawn、
380 control、434 collision、金額、掉落、暫停及回到標題等回歸數字維持
不變。

## Stage 2 敵人池與事件直譯（歷史里程碑）

Stage 2 依 `src/varz.h` 建立固定寬度的 `OtEnemy`，逐欄位對應
`JE_SingleEnemyType`，並保留原始四組各 25 格的 `enemyAvail[100]`。
`Sprite2_array *sprite2s` 與 `void *enemydatofs` 是 PC 指標，GBA 端最小必要
修改為 shape-bank ID 與 enemy-definition ID；其餘速度、加速度、動畫、
armor、score item、turret、launch、death spawn、bounce 及特殊旗標欄位
保留原始 8/16-bit signedness。

`JE_makeEnemy()` 目前鎖定已確認的遊戲模式：

```text
single player
Normal difficulty
SA_NONE
non-superTyrian
non-Galaga
```

Normal 難度的 armor 與 value 分支是原值 identity transform，因此沒有把
其他難度公式近似套用進來。初始化順序包含 turret wait、animate mode、
random start、acceleration wait/reverse、score-item 判定及 `totalEnemy`。
原始碼在 armor=0 且 value=0 時沒有清除 recycled slot 的 `scoreitem`，
Stage 2 也刻意保存此行為，沒有自行「修正」。

已直譯的第一關 entity event 家族：

| 家族 | Event types | 狀態 |
|---|---|---|
| 四個 25-slot pool 生成 | 6, 7, 10, 15 | 已執行 |
| 四片組合與底部生成 | 12, 17, 18, 23, 32, 56 | 已執行 |
| 移動／加速度／反轉 | 19, 20, 27 | 已執行 |
| 武器 override／death spawn | 31, 33, 45 | 已執行 |
| 特殊敵人與條件跳過 | 60, 61 | 已執行 |

事件 19、27、31、33、60 在 OpenTyrian 中會掃描所有 100 格，即使該格
目前是 free；直譯版保留這個容易被誤改成「只掃 active」的細節。事件
61 會改變 `eventLoc`，因此 telemetry 另外保存 skipped count，驗證式改為：

```text
applied + deferred + skipped == source event index
```

### RNG

Stage 2 使用與 OpenTyrian `src/mtrand.c` 相同的 624-word MT19937 state、
twist 與 temper 運算。PC 程式在 `main()` 以 `time(NULL)` seed，並會在進入
關卡前由其他系統消耗亂數；Stage 2 shadow runtime 為可重現 auto-test 使用
固定 seed 5489。演算法內及關卡事件的 RNG 呼叫順序已保留，但在整個
session 初始化也直譯完成前，不能宣稱與某一次 PC 執行具有相同亂數序列。

### Stage 2 歷史限制

Stage 2 尚未執行 movement/off-screen release，因此 source pool 只增不減，
會填滿 100 格並產生 373 次 pool-full。這是舊版里程碑的鎖定數字；Stage 3
已由實際 slot release 取代，不應再把飽和結果當成預期行為。

Stage 2 的 source context 為 15,928 bytes。v15 另加入 64,000-byte PIC
decode buffer 與 40,000-byte SHP scratch；linker 的 EWRAM heap start 為
`0x0201e4c0`，共使用 124,096 bytes，仍保留 138,048 bytes
（約 134.81 KiB）。這些都是 `.sbss`，不增加 cartridge 內的零資料。

## Stage 2 驗證

2026-07-26 以 ARM GCC 16.1.0 及 mGBA 0.11.0 完成全流程測試：

| 項目 | 結果 |
|---|---:|
| Auto-test | PASS |
| Release ROM | 682,192 bytes（666.20 KiB） |
| Release SHA-256 | `b70e9f65352ae6f1f9cd8a61b570518bf0d4e8eabd95c8669df8c422e9525e8c` |
| Source events index | 878 |
| Applied／deferred／skipped | 869／5／4 |
| Spawn attempts／success／pool full／missing | 473／100／373／0 |
| Source enemy-control field writes | 3,586 |
| Source MT19937 calls | 30 |
| Missed VBlank／runtime errors | 0／0 |

五筆 deferred record 全是尚未移植的 type-16 text-window/audio UI 事件。
legacy 的 414 spawn、380 control、434 collision、金額、掉落、暫停與
Boss 回標題等數字仍完全相同，證明新增 shadow 工作沒有改壞現有展示。

## Stage 3 `JE_drawEnemy()` 生命週期（歷史里程碑）

v15 依 OpenTyrian `JE_main()` 的呼叫順序逐段直譯：

```text
ground pool 25..49
ground2 pool 75..99
continual enemy check
sky pool 0..24
top pool 50..74
```

該階段每個 active slot 已執行：

- animate cycle、`egr == 999` 消失條件
- player-seeking random acceleration 與 MT19937 呼叫順序
- `excc`／`eycc` fixed acceleration、reverse 與 wait
- `fixedmovey`、X/Y movement、bounce、score-item 邊界修正
- `-80..340`／`-112..190` off-screen release
- 三個 `tur[]`／`freq[]` fire wait 與 HDT weapon multiposition cadence
- `elaunchfreq`／`elaunchtype` 子敵人配置、位置、aim 及 link 繼承
- `levelEnemy[]` continual spawn 路徑

第一關在 event index 3、`curLoc=0` 立即執行 type 13，關閉 continual
enemy，因此這條路徑在本關量測為 0；本身仍完整保留。進 Boss 前也沒有
任何 active definition 觸發 enemy launch，所以 launch count 為 0。

Stage 3 尚未建立 shadow runtime 自己的 60-entry projectile objects；
目前只保留 weapon read、fire wait、multiposition、animation activation、
sound RNG 與 trigger count。251..255 magnet／特殊 render opcode、實際
projectile movement/collision、敵人受傷死亡、`enemydie`、reward 與
`globalFlags` 仍是下一階段。這些缺口都有明確 adapter boundary，沒有用
POC 規則填入 source-parity context。

### Stage 3 固定 seed 驗證

| 項目 | v15 結果 |
|---|---:|
| Source event index | 878 |
| Applied／deferred／skipped | 869／5／4 |
| Event spawn attempts／success／pool full／missing | 473／473／0／0 |
| Peak source enemies | 39／100 |
| Motion updates／off-screen releases | 63,381／453 |
| HDT weapon shot triggers | 200 |
| Enemy-control field writes | 2,535 |
| MT19937 calls | 2,266 |
| Missed VBlank／runtime errors | 0／0 |

`473 success - 453 release = 20`，等於進 Boss 前 shadow pool 的剩餘 active
數；Stage 2 的假性 100-slot 飽和已消失。

## Stage 4 authoritative gameplay

v16 刪除 `src/event_runtime.inc`、48-entry legacy enemy pool 及其壓縮
`level_events.bin`。第一關本體的敵人不再同時跑兩套規則；畫面直接走訪
`OtLevelPortState.enemy[100]`，戰鬥直接修改同一份 source state。

### 執行順序

每個約 34.78 Hz logic tick 保留 OpenTyrian 的主要 phase boundary：

```text
原始 event records
→ ground / ground2 / continual / sky / top enemy pools
→ 既有玩家彈移動與 source enemy collision
→ 玩家與 score-item／enemy collision
→ GBA 玩家輸入、移動及新玩家彈建立
→ source enemy-shot movement／player collision
→ GBA presentation
```

新建立的玩家彈不會在建立當 tick 立刻移動或命中。敵彈瞄準在玩家移動後
更新，玩家撞機則遵守受擊無敵間隔；這兩點都是直接對照
`JE_main()`／`JE_playerCollide()` 後修正的時序。

Source gameplay 保持 320×200 座標。v18 的 `src/source_runtime.inc`
只做整數平移：

```text
GBA x = PC game_screen x - 36
GBA y = PC game_screen y - 12
```

其中原版 `JE_starShowVGA()` 先從 `game_screen + 24` 取 264×184 gameplay
viewport，再從該 viewport 四邊各裁 12 px，因此真正的 PC framebuffer
來源範圍是 `x=36..275, y=12..171`。敵人 pool 仍決定 OBJ layer priority，
主角、子彈、獎賞及爆炸固定在背景之前。這個 adapter 不回寫敵人位置、
速度、碰撞尺寸或視差。

### 60-entry enemy-shot pool

`OtEnemyShot[60]` 逐欄位保存 OpenTyrian `EnemyShotType`：

- HDT `multi/max/bx/by/sx/sy/attack/delay/sg`
- 三個 turret slot 的旋轉、acceleration 及 multiposition
- `aim` 的原始 max-magnitude normalization
- `tx/ty` tracking、duration、animation 及 source graphic ID
- source pool-full 時跳過該敵人剩餘 launch routine 的控制流
- 每次 weapon sound 所需的 MT19937 channel-selection 消耗順序

在 Boss handoff 時，仍在飛行的 source shot 會經同一 release path 清除，
v18 固定 route 的 181 spawn 對應 181 release，沒有隱藏的 active
projectile。

### 玩家彈、死亡與獎賞

玩家 Pulse-Cannon 以 HDT weapon 155 的 power-1 damage 進入 source
collision。已直譯：

- `enemycycle==0` 與一般 enemy 的兩套命中框公式
- armor 255、普通 armor、`edlevel`／`edani` damaged transition
- link 254、`link-100` 與 40+ group destruction
- `dlevel=-1` 的 availability-2 固定殘骸與 `edgr` 切換
- `special/flagnum/setto`、直接 `evalue` cash
- `eenemydie` 在原 25-slot group 產生子物件
- cash item、data cube、front/rear weapon power-up pickup 分流

本回歸路線收取五個實體物件，其中兩個是 data cube、三個合計 175 的
cash item；沒有落入 unsupported pickup。因本 POC 固定武器且不做右側
HUD，data cube／weapon power-up 只保存 gameplay counter，不開啟 PC
訊息視窗。

### v16 固定 seed 驗證

2026-07-26 使用 ARM GCC 16.1.0、mGBA 0.11.0 與 MT19937 seed 5489：

| 項目 | v16 結果 |
|---|---:|
| ROM／host verifier | PASS／PASS |
| Source event index | 878 |
| Applied／deferred／skipped | 869／5／4 |
| Event spawn attempts／success／pool full／missing | 473／473／0／0 |
| Death spawn attempts／success／pool full／missing | 5／5／0／0 |
| Peak／handoff active source enemies | 39／20 |
| Enemy motion updates／releases | 52,103／458 |
| Enemy-control writes／MT19937 calls | 2,444／1,659 |
| Source shots spawn／release／drop／peak | 168／168／0／8 |
| Source shot movement／player hits | 8,347／17 |
| Player-shot hits／enemy contacts／kills | 467／28／137 |
| Direct cash／pickup cash／final cash | 1,785／175／1,960 |
| Score-item spawn／pickup／peak | 5／5／2 |
| Data cubes／unsupported pickup | 2／0 |
| Peak visible source enemies／peak OAM | 36／48 |
| Stream／effect／reward drops | 0／0／0 |
| Missed VBlank／display frames | 13／12,239（約 0.11%） |
| Runtime errors／ROMFS self-test failures | 0／0 |

13 次 missed VBlank 低於 v16 regression 上限 16，但不是「零成本」：
完整 tracker mixing、三層串流、100-slot source runtime、碰撞與最高 27
個同時 effect 在少數 frame 會跨過單一 VBlank。此數字作為 GBA 技術展示
的效能量測保留，不用降低 logic rate 或刪除音樂掩蓋。

## v17 敵人圖像與獎賞逐行直譯

v17 把使用者指定的兩項問題納入 source-parity runtime，而不是繼續修補
舊 POC：

- `JE_playerCollide()`、`power_up_weapon()` 與
  `handle_got_purple_ball()` 依固定 single-player／Normal 模式保留原分支
  順序與 gameplay state。
- 玩家彈死亡路徑補齊直接 data-cube credit；`eenemydie`、linked death、
  cash 與 `dlevel=-1` 殘骸仍使用同一份 source enemy slot。
- 移除 24 個 GBA 自訂 enemy archetype 及 fallback。
- 在 `JE_drawEnemy()` 的原始 `blit_enemy()` phase 保存
  `shape_table/egr[enemycycle-1]/size/filter` draw command。
- 對第一關全部 1,009 event 做 spawn／launch／death closure，建立 113
  個 definition、198 個原始 Sprite2 畫格的完整 catalog。
- `size==1` 依原碼用 `graphic + 0/+1/+19/+20` 組成 24×28 圖，不重畫
  silhouette；金幣、寶石與 cube 也走 shape table 21 source object。
- 使用 24-slot true-LRU OBJ cache，在 VBlank 上傳 32×32 4bpp container。

固定 route 的 catalog miss／cache drop／fallback visual 都是 0。
153 次 frame upload 共 78,336 bytes；cache hit／miss／eviction 為
44,509／153／129，單一 frame 最高七次 upload。完整 mGBA route 仍維持
5/5 death-spawn、5/5 score-item、2 data cube、1,785 direct cash 與
1,960 final cash。

詳細逐行對照、GBA palette 限制與 telemetry 見
[Tyrian-GBA-Enemy-Reward-Source-Parity-v17.md](Tyrian-GBA-Enemy-Reward-Source-Parity-v17.md)。

### v17 已知邊界

- `curLoc=5400` 轉入既有簡化 Boss；Boss body、Boss damage lifecycle 及
  結束事件尚未逐行移植。
- 4912..5384 的大型結構與 Boss component 畫格已進入 198-frame
  catalog；但 5400 handoff 尚未讓原始 Boss event/lifecycle 實際接管。
- 五筆 deferred event 是不影響本展示流程的 type-16 text/audio UI。
- turret 251..255 magnet／特殊 render opcode 尚只保存 wait/animation
  狀態；本次測試路線沒有需要其完整玩家物理效果的 discharge。
- 玩家仍沒有死亡、生命與重生流程；碰撞及 hit cadence 有執行，但技術
  展示不會中止。
- MUS metadata 已由 raw loader 讀取，實際 waveform 仍由 Maxmod IT cache
  播放。

## v18 PC 座標與 1:1 中央裁切

v18 移除舊的 4:5 presentation scaling，讓 GBA 僅作最終裁切。改動涵蓋
玩家、敵人、敵彈、玩家彈、效果及三層背景，而不是只修改 OBJ 顯示公式：

- 玩家位置改為 PC `player.x/y` authority，保留每次鍵盤 slice 的 ±1
  input、X/Y friction cadence、速度 ±4 clamp 與 PC 邊界
  `x=40..256, y=10..160`。
- 玩家射擊、敵人瞄準、碰撞與 score item pickup 都直接讀同一份 PC
  玩家座標，不再從已縮放的 GBA 畫面座標逆轉換。
- 直譯 `JE_mainGamePlayerFunctions()` 的整數視差：

  ```text
  temp = floor((296 - playerX) * 72 / 224) - 1
  mapX3ofs = temp
  mapX2ofs = floor(temp * 2 / 3)
  mapXofs  = floor(mapX2ofs / 2)
  ```

- `JE_drawEnemy()` 的 `mapoffset` 依 ground／sky／top pool 採用正確
  map X offset；出界回收、射擊、碰撞與 OAM draw command 使用相同值。
- 主角 24×28 與敵人 12×14／24×28 Sprite2 source cell 固定錨定在
  32×32 OAM container，不再逐幀裁透明 bbox 後置中而造成位置偏移或
  animation jitter。
- 背景不再把 264 px 橫向壓成 240 px。ROM 中每層是 64 tile＝512 px
  寬；VRAM 以兩個相鄰 screen block 組成 GBA `BG_SIZE_1`，保留完整 PC
  來源 raster 後用 HOFS 選出中央 240 px。
- MAP1 從原始 row 3、MAP2/MAP3 從 row 14 開始；初始垂直來源分別是
  8,104 與 16,196 px。MAP1/MAP2 保留「先畫後推進」，MAP3 保留
  「先推進後畫」的原版 phase。
- 64-column map 只在 VRAM 維持 32 tile-row 環形視窗；每次跨列於
  VBlank 同步更新左右兩個 screen block。

自動測試 schema 16 額外保存 PC 玩家位置、三組 presentation map
offset、三層 HOFS、三層垂直來源與裁切 origin，host verifier 逐一檢查
上述關係。固定 route 結果：

| 項目 | v18 結果 |
|---|---:|
| ROM／host verifier | PASS／PASS |
| PC crop origin | 36, 12 |
| 最終 PC player x/y | 77／10 |
| MAP1／MAP2／MAP3 x offset | 24／49／74 |
| MAP1／MAP2／MAP3 HOFS | 60／35／34 |
| MAP1／MAP2／MAP3 vertical source | 2,363／5,225／4,198 |
| Source event applied／deferred／skipped | 869／5／4 |
| Event spawn success／pool full／missing | 473／0／0 |
| Peak source enemies／peak OAM | 39／43 |
| Stream／effect／reward／shot drops | 0／0／0／0 |
| Missed VBlank／display frames | 58／12,239（約 0.47%） |
| Runtime errors／ROMFS failures | 0／0 |

完整公式、原碼定位與 framebuffer 檢查記錄見
[Tyrian-GBA-1to1-Crop-Source-Parity-v18.md](Tyrian-GBA-1to1-Crop-Source-Parity-v18.md)。

## v19 玩家可見邊界

v18 的最終裁切正確，但玩家仍可到達 PC 完整 viewport 使用的
`Y=10..160`，因此 24×28 飛機在 GBA 上下邊緣會被裁切。v19 依兩張
玩家 source graphic 的實際 alpha bbox `[2,27)` 推導安全範圍：

```text
player draw origin = playerY - 7
GBA visible source rows = 12..171
playerY - 7 + 2  >= 12  -> playerY >= 17
playerY - 7 + 26 <= 171 -> playerY <= 152
```

玩家 source Y clamp 因此改為 `17..152`；X clamp `40..256` 不變。
`source_player_screen_y()` 的 presentation centre 同時由 `+8` 修正為
`+7`，使 32×32 OAM container 內的 24×28 source cell 精確對回 PC
`playerY-7` draw origin。

這個差異只存在於 GBA viewport adapter。敵人、背景、子彈、碰撞及
parallax 仍使用 PC source space；玩家位置也沒有改成 GBA 座標。上下
極限專用 framebuffer 回歸確認第一個／最後一個不透明 pixel 分別落在
GBA `y=0`／`y=159`。

v19 telemetry schema 17 的完整路線取得 ROM／host PASS，最終玩家座標
為 `(77,17)`，54/12,239 missed VBlank，ROMFS 93/93 checks，且所有
stream、effect、reward、projectile、catalog 及 frame-cache drop 都是 0。
推導、靜態斷言與驗證記錄見
[Tyrian-GBA-Player-Crop-Bounds-v19.md](Tyrian-GBA-Player-Crop-Bounds-v19.md)。

## v20 PC draw order 與結構 palette

v19 以前的 GBA scene adapter 仍把四組 enemy pool 指派到固定 OBJ
priority，沒有把 `tyrian2.c` 的
`background2over`、`background3over`、`topEnemyOver` 與
`skyEnemyOverAll` software-blit 順序完整翻譯。因為 GBA OBJ 在與 BG
同 priority 時會位於 BG 前方，ground pool priority 2 會錯誤突出到
priority 2 的雲層上。

v20 保留原始 pool identity，新增 `src/layer_runtime.inc`：

- MAP2/MAP3 依 PC 當幀 stage 分別使用 BG priority 2／1；
- 位於兩層後、兩層間、兩層前的 OBJ 分別使用 priority 3／2／0；
- scene category 與每個 pool slot 以 PC blit 的反向順序寫入 OAM；
- 事件 21/22/28/29/42/43/73 改變的 layer flags 直接驅動 VBlank 設定，
  沒有第一關座標特例。

4×3 種背景狀態和 10 個物件 stage 共 252 項前後關係全部通過。固定
`curLoc=240`、5000、5050 framebuffer 回歸確認 type-12 可破壞建物會被
白雲遮住，而 Boss 前機關位於 MAP2 與 MAP3 之間。

同一版把 type-12 結構使用的八張 table-1 frame 改用專屬 15 色 palette；
第一關期間與 position-5400 後的簡化 Boss 分時使用 OBJ palette bank 5。
PC source RGB RMSE 由 12.4863 降為 6.7365（降低 46.05%）。

Telemetry schema 18 的完整路線仍為 7,093 logic updates、12,239 display
frames、54 missed VBlank、peak OAM 43，所有 stream/effect/reward/
projectile/catalog/cache drops 都是 0。完整設計與驗證見
[Tyrian-GBA-PC-Layer-Order-Palette-v20.md](Tyrian-GBA-PC-Layer-Order-Palette-v20.md)。

## v21 通用 ROMFS Sprite2 直讀

v20 的 gameplay 已直接讀 LVL/HDT，但 presentation 仍只能從 Python
產生的 198-frame 第一關 catalog 取圖；增加關卡就必須先擴充 event
closure 與 palette mapping。v21 移除這個維護邊界：

- `opentyrian_data.c` 依 PC `shapeFile[]` 直接開 ROMFS `newsh*.shp`；
- shape table 21／26 依 `JE_makeEnemy()` 改讀 `tyrian.shp` 的
  coins／power-up compact sections；
- `opentyrian_sprite2.c` 直接翻寫 one-based offset、skip/fill、
  `0x0f` terminator、filter 與 2×2 component composition；
- GBA adapter 把 PC palette index 投影到 16 hue × 8 brightness 的
  8bpp OBJ palette；
- 21-slot cache 的 key 包含 shape table、graphic、size 與 filter，不再
  存在 first-level catalog lookup 或 fallback visual。

因此增加後續關卡時，gameplay loader 只需照原始 LVL 載入它的 shape-bank
需求；只要 bank 已包含在 ROMFS manifest，enemy presentation 不需要新增
關卡專用 Python 分支。

完整 route 是 0 decode failure、0 cache drop，152 次 decode miss 與
44,926 次 hit；代價是 delayed VBlank 由 v20 的 54 增至 155。這一版先
保留 raw runtime 路徑供實際試用。若成本不合適，下一個版本會以全 bank、
非 event-driven 的預展開 row provider 替換 component reader，並保持
相同 gameplay／composition API。詳見
[Tyrian-GBA-Runtime-Sprite2-v21.md](Tyrian-GBA-Runtime-Sprite2-v21.md)。

## ROMFS v1 與原始格式 loader

v14 建立通用資料 I/O；v15 的 `src/opentyrian_data.c/.h` 開始實際使用它，
v16 則讓 LVL/HDT reader 成為 gameplay 唯一資料入口。
68 個 stock runtime 檔案以原始 bytes 封裝到 9,853,080-byte ROMFS image，
資料直接留在 cartridge ROM。Loader 只保存 const pointer、size 與 offset，
不把完整檔案複製到 256 KiB EWRAM。

| Loader | v16 runtime 行為 |
|---|---|
| LVL | 直接索引第一關 section、events、enemy IDs、map lookup／bytes |
| HDT | 直接解碼 80-byte weapon 與 77-byte enemy records |
| PIC | 驗證 13-entry offset table、RLE 320×200 與尾端 `0x0c` |
| SHP | 驗證 12 sections、前七組 Sprite records、後五組 compact views，並依 shape-table ID 開 `newsh*.shp` |
| MUS | 驗證 41 首 LDS、46-byte patch、9-channel positions 與 pattern words |

開場畫面已實際由 ROMFS `tyrian.pic` picture 4、`palette.dat` palette 8、
`tyrian.shp` PLANET_SHAPES sprite 146 及 FONT_SHAPES 字元在 GBA 開機時
解碼。`title_bitmap.bin`、`opentyrian_level1_events.bin` 與
`opentyrian_level1_enemies.bin` 均不再產生或連結；v16 也移除舊
`level_events.bin`。

MUS 的 song selection 與 LDS 結構來源已改為 raw `music.mus`（title
index 29、level index 17）；實際 GBA waveform synthesis 仍暫由 Maxmod
IT cache 擔任，待 LDS/OPL mixer 直譯後再移除該 presentation adapter。
格式、路徑、API、容量與擴充方式詳見
[Tyrian-GBA-ROMFS.md](Tyrian-GBA-ROMFS.md)。

2026-07-26 mGBA auto-test 驗證 93 項 ROMFS mount、lookup、read、seek、
typed little-endian read、EOF、path normalization、read-only mode 及
8-handle pool 檢查全部通過；probe 已包含 LVL、HDT、MUS、SHP、PIC。

## 分檔

原本 2,186 行的 `main.c` 已降至約 580 行。現有程式依責任分為：

| 檔案 | 責任 |
|---|---|
| `main.c` | GBA 入口、共享狀態及主排程 |
| `src/layer_runtime.inc` | PC software draw stage 到 GBA BG/OBJ priority 的純映射與窮舉測試 |
| `src/gba_platform.inc` | VBlank、音訊及暫停平台層 |
| `src/jukebox_runtime.inc` | 41 曲 Jukebox、投影星空、tile text、切歌與 fade |
| `src/level_setup.inc` | 關卡進出與 VRAM 資源設定 |
| `src/entity_runtime.inc` | GBA explosion／測試 reward presentation pool |
| `src/combat_runtime.inc` | GBA 玩家輸入、玩家彈及簡化 Boss projectile adapter |
| `src/source_runtime.inc` | source 座標、OAM、Sprite2 L1/L2、音效、效果與 telemetry adapter |
| `src/level_update.inc` | source phase orchestration、背景及簡化 Boss |
| `src/gba_oam.inc` | OAM primitive 及 projectile presentation |
| `src/gba_hud.inc` | GBA 保留的最小 HUD |
| `src/gba_scene.inc` | scene-to-OAM renderer |
| `src/autotest.inc` | mGBA deterministic regression harness |
| `src/opentyrian_data.c` | ROMFS MUS/SHP/PIC/HDT/LVL 原始格式 reader |
| `src/opentyrian_sprite2.c` | 全 bank raw component reader 與 ROMFS RLE parity/fallback decoder |
| `src/opentyrian_level_port.c` | 第一關本體 authoritative source runtime |

GBA presentation `.inc` 仍由同一 translation unit 編譯；原始事件、敵人、
projectile 及 collision state 已收進獨立的 `.c/.h` 模組。舊
`event_runtime.inc` 與 legacy enemy allocation/control/collision 已刪除，
只在 position 5400 後保留簡化 Boss adapter。

## 最小必要修改原則

允許修改：

- SDL input → GBA keypad
- SDL surface／blit → GBA BG、OAM、palette、DMA
- SDL audio → Maxmod
- PC timer → VBlank fixed-step scheduler
- 檔案讀取 → ROM 中的 const table
- PC 動態配置 → GBA 固定 pool
- 264×184 gameplay viewport → 四邊各裁 12 px 的 240×160 1:1 視窗
- 玩家垂直 clamp → 保持完整 24×28 飛機可見的 source `Y=17..152`

不允許在 platform adapter 內修改：

- event 順序
- RNG 呼叫順序
- enemy update 順序
- 座標、速度與碰撞公式
- damage、armor、score、cash、reward 規則
- Boss 狀態轉換

所有無法直接保留的差異要以 `GBA_PORT` 註解及本文件記錄。

## v25／v26／v27 來源流程進度

v25 已把第一關 Boss group 清空後的 player end-level warp、殘影、End of
Level 曲目、`Level completed` voice、分段統計與按鍵返回翻寫完成。

v26 已完成玩家死亡路徑：

1. `JE_playerDamage()` 致命狀態、60 tick 與 `levelEnd = 40`。
2. 每 tick 雙大型爆炸、來源 MT19937 呼叫順序及隨機
   `S_EXPLOSION_9`／`S_EXPLOSION_11` cadence。
3. 59 次一單位音樂 fade、真正 Game Over 曲目及 effect-preserving
   module switch。
4. 在最後 gameplay composition 上疊出 `GAME OVER`；按鍵後才回到
   Game Menu 並恢復 title music。
5. effect 邏輯 pool 對齊 PC 的 200 格；GBA OAM 僅在 presentation
   階段限制每幀 48 個 effect。

v27 已完成 Jukebox：

1. 主選單入口、41 首 source song index 及 `musmast.c` 曲名已接通。
2. PC `starlib.c` 星空改以 Mode 0 tile／palette 與 112 OBJ 投影呈現；
   切歌只更新 2 KiB text map，不重建整張 bitmap。
3. 上一首／下一首雙向環回、自然播畢淡出換曲、文字隱藏及退出 fade
   均已實作。
4. 全部 41 首 TYM/LDS 轉入 Maxmod；超過 200 rows 的 IT intro pattern
   以保留 timing 及 `Bxx` loop target 的方式分段。
5. 退出後恢復 title song 29，獨立 `TGJ1` SRAM auto-test 已加入四組
   Detail／Game Speed 回歸。

詳細紀錄：

- `Tyrian-GBA-End-Level-Source-Parity-v25.md`
- `Tyrian-GBA-Player-Death-Source-Parity-v26.md`
- `Tyrian-GBA-Jukebox-v27.md`

## v28 通用 ROMFS／多關卡進度

v28 已把第一關專用 reader 改成 selected-level runtime：

1. 62/62 個 LVL sections、53,338 筆 events 與 24 條 script route 已在
   GBA runtime matrix 通過。
2. EP1–EP4 代表 route 均能讀取原始 ROMFS 資料並完成 gameplay／stats。
3. Episode 1 已依 `levels1.dat` 真正連續完成四關，路徑為
   section 3 → 5 → 29 → 25。
4. 跨關卡保留 `enemySpriteSheets[4]` slot pointer 語意；不新增每關
   Python asset。
5. OBJ VRAM 增加一個 16×16 compact Sprite2 slot，使第三關 24 個同幀
   unique enemy graphics 維持零 cache drop。

詳細紀錄：

- `Tyrian-GBA-ROMFS-All-Levels-v28.md`
- `Tyrian-GBA-Updated-Plan-v28.md`

## v29 Boss Sprite2 效能進度

v29 將 Sprite2 不變的 RLE 解壓移到 build 階段，runtime 的來源選擇與
palette/filter 語意不變：

1. 37 個 logical banks、11,248 個 12×14 components 完整無損展開；不是
   per-level 或 event-limited catalog。
2. 24-slot OBJ L1 後加入 64×1 KiB EWRAM L2，enemy／projectile 共用。
3. L2 與 Mode-4 front-end scratch 使用 union overlay；release 尚餘
   53,764 bytes EWRAM。
4. `WAITCNT=0x4317`，palette mapping 熱路徑使用 ARM/IWRAM。
5. 第一關 missed VBlank 625 → 13；Boss 區段 437 → 4，而 Boss 的
   432 次 L1 miss、432 次 eviction 與 411,648 upload bytes 完全不變。
6. `TGLM schema 2` 對 6,098 個 runtime frames 執行 6,146,816 個
   palette/filter/tile-order pixel parity，全部通過。

詳細紀錄：

- `Tyrian-GBA-Boss-Sprite2-L2-v29.md`
- `Tyrian-GBA-Updated-Plan-v29.md`

## v30 Episode 2／4 背景空白索引修正

OpenTyrian 載入 `mapSh` 後，三層 `ref` 並不是共用 128 個有效 entry：

- layer 1：`0..71`
- layer 2：`0..70`，`71` 強制為 `NULL`
- layer 3：`0..69`，`70..71` 強制為 `NULL`

GBA 版先前直接對三層查完整的 128-entry table。Episode 2 第一關的
layer 2／3 全部使用空白 sentinel 71，因而被誤查成 shape 567 並鋪滿
畫面；Episode 4 layer 2 的 8,302／8,400 cells 與 layer 3 的
9,000／9,000 cells 也使用 71，分別被誤查成 shape 24／1。

v30 在共用 background lookup 套用原版的 72／71／70 entry 界線，不
修改 LVL、不新增 Episode 特例。Episode 1／3 的固定座標 PNG 在修正前後
SHA-256 完全相同，Episode 2／4 則恢復水面、地面、基地與通道的正常
分層。`TGLM schema 2` 的 62 個 sections 與所有既有回歸均通過。

詳細紀錄：

- `Tyrian-GBA-Episode-Background-Sentinel-v30.md`

## v31 Episode 2 背景效能與 IWRAM 熱路徑

Episode 2 第一關的嚴重停頓不是 Maxmod 或 Sprite2 RLE 解壓造成。
v30 把 64×32 tilemap ring 的全部 32 列都視為正在使用，layer 1
因此同時保護約 642 個 pattern；超過 512-slot cache 後，每次 miss
都會掃描 512×32 bytes 選擇近似圖。

v31 保持硬體 ring 不變，只讓 21 列可見區持有 references，另用一列
支援 prefetch／presentation 轉場。Episode 2 第一關的 22 列最大
工作集是 501，能放進目前配置。`missed VBlank` 由 553 降到 30，
background approximations 由 472 降到 28；collision、event、Sprite2
L1／L2 與關卡完成位置都不變。

Sprite2 raw palette mapping 改為 32／16-bit grouped stores；cache
acquire、player-shot collision 與真正的 raw writer 固定在
ARM/IWRAM。RLE decoder 不搬移，因為 runtime fallback 為 0。
release 仍保留 49,612 bytes EWRAM 與 6,408 bytes IWRAM。

關閉全部 music／SFX 的診斷 build 在修正後只把 missed VBlank
30 降為 29，因此保留完整 Maxmod 音質，不以 Game Boy PSG 作效能
workaround。`build.ps1` 已加入 Episode 2 section 1 的永久 route
效能回歸。

詳細紀錄：

- `Tyrian-GBA-EP2-Background-Performance-v31.md`
- `Tyrian-GBA-EP2-Performance-Evaluation-2026-07-27.md`

## 下一個移植階段

1. 將目前四關 campaign 擴大成 Episode 1 的完整 Full Game 路徑與
   Episode 2 轉場。
2. 保存關卡間的 player、cash、cube、weapon 與 global flag 狀態。
3. 逐行翻寫 front／rear／special weapon，移除 route-test combat assist，
   並完成 turret 251..255 magnet／special effects 與 misc-shot 104。

## 建置

```powershell
.\build.ps1
```

目前 ROMFS v31 預設 ROM：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v31_detail_low_speed_normal.gba
```

ROM 與中間產物不納入 Git。

## 授權

新的直譯程式衍生自 OpenTyrian GPL 原始碼，使用
`GPL-2.0-or-later`，並在 repository 根目錄保存原始 `COPYING`。
MT19937 段落沿用 OpenTyrian `mtrand.c` 的 BSD 3-clause 授權，完整
copyright、條件與 disclaimer 保留在 `src/opentyrian_level_port.c`。
