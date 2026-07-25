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

Stage 3 已完成 ROMFS 原始格式資料層、100-entry enemy pool、
`JE_makeEnemy()`、`JE_createNewEventEnemy()`、第一關 enemy-control 事件，
以及 `JE_drawEnemy()` 的 movement／animation／off-screen release／fire
cadence／launch 生命週期。畫面、碰撞與死亡仍由 v11 legacy game loop
執行，所以此版本不能宣稱第一關已完成 source parity；shadow runtime
的用途是讓每個後續函式都有可量測、可逐步取代的基礎。

## 已鎖定的顯示與操作規格

OpenTyrian 的邏輯 framebuffer 是 320×200；以 4:3 像素比例顯示時，外觀
接近 320×240。原始遊戲區為 264×184：

| 區域 | 尺寸 | GBA 規格 |
|---|---:|---|
| Gameplay viewport | 264×184 | 映射至 240×160 |
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

v15 不再產生或嵌入 `opentyrian_level1_events.bin` 與
`opentyrian_level1_enemies.bin`。`src/opentyrian_data.c` 直接對 ROMFS
內的 stock 檔案建立唯讀 view：

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

## Stage 1 直譯程式

`src/opentyrian_level_port.c` 已建立：

- `OtEventRecord`：固定寬度的 `JE_EventRecType`
- `OtEnemyDefinition`：固定寬度的 `JE_EnemyDat`
- 原始 event 與 enemy ROM table reader
- 第一關 `JE_main()` 狀態初始化
- `JE_eventSystem()` 中不依賴 SDL 或 entity pool 的第一批 case
- 原始欄位 signedness 與 8/16-bit 寬度 static assertions

目前已直譯的事件 case 包含背景速度、shape bank、星空開關、敵人總開關、
前後景順序、音樂 fade、新曲狀態、關卡結束準備、持續傷害旗標及 Boss
bar link 等純狀態操作。

尚未直譯完成的 spawn、敵人控制、碰撞及玩家相關 case 不會產生近似
結果；它們只增加 `deferred_event_count`，仍由 legacy runtime 執行。

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

## Stage 2 敵人池與事件直譯

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
關卡前由其他系統消耗亂數；目前 shadow runtime 為可重現 auto-test 使用
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

## Stage 3 `JE_drawEnemy()` 生命週期

v15 依 OpenTyrian `JE_main()` 的呼叫順序逐段直譯：

```text
ground pool 25..49
ground2 pool 75..99
continual enemy check
sky pool 0..24
top pool 50..74
```

目前每個 active slot 已執行：

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

## ROMFS v1 與原始格式 loader

v14 建立通用資料 I/O；v15 的 `src/opentyrian_data.c/.h` 開始實際使用它。
68 個 stock runtime 檔案以原始 bytes 封裝到 9,853,080-byte ROMFS image，
資料直接留在 cartridge ROM。Loader 只保存 const pointer、size 與 offset，
不把完整檔案複製到 256 KiB EWRAM。

| Loader | v15 runtime 行為 |
|---|---|
| LVL | 直接索引第一關 section、events、enemy IDs、map lookup／bytes |
| HDT | 直接解碼 80-byte weapon 與 77-byte enemy records |
| PIC | 驗證 13-entry offset table、RLE 320×200 與尾端 `0x0c` |
| SHP | 驗證 12 sections、前七組 Sprite records、後五組 compact views，並依 shape-table ID 開 `newsh*.shp` |
| MUS | 驗證 41 首 LDS、46-byte patch、9-channel positions 與 pattern words |

開場畫面已實際由 ROMFS `tyrian.pic` picture 4、`palette.dat` palette 8、
`tyrian.shp` PLANET_SHAPES sprite 146 及 FONT_SHAPES 字元在 GBA 開機時
解碼。`title_bitmap.bin`、`opentyrian_level1_events.bin` 與
`opentyrian_level1_enemies.bin` 均不再產生或連結。

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
| `src/gba_platform.inc` | VBlank、音訊及暫停平台層 |
| `src/level_setup.inc` | 關卡進出與 VRAM 資源設定 |
| `src/event_runtime.inc` | legacy 簡化事件 runner |
| `src/entity_runtime.inc` | legacy entity 配置與控制 |
| `src/combat_runtime.inc` | legacy 戰鬥 |
| `src/level_update.inc` | legacy 背景、Boss 與 frame orchestration |
| `src/gba_oam.inc` | OAM primitive 及 projectile presentation |
| `src/gba_hud.inc` | GBA 保留的最小 HUD |
| `src/gba_scene.inc` | scene-to-OAM renderer |
| `src/autotest.inc` | mGBA deterministic regression harness |
| `src/opentyrian_data.c` | ROMFS MUS/SHP/PIC/HDT/LVL 原始格式 reader |
| `src/opentyrian_level_port.c` | 新的原始碼直譯 runtime |

`.inc` 檔是維持 v11 完全相同行為的過渡分檔，仍由同一 translation unit
編譯；新的直譯程式使用真正獨立的 `.c/.h` 模組。等共享狀態逐步收進
source-parity context 後，legacy `.inc` 會被刪除，而不是成為新架構。

## 最小必要修改原則

允許修改：

- SDL input → GBA keypad
- SDL surface／blit → GBA BG、OAM、palette、DMA
- SDL audio → Maxmod
- PC timer → VBlank fixed-step scheduler
- 檔案讀取 → ROM 中的 const table
- PC 動態配置 → GBA 固定 pool
- 264×184 gameplay viewport → 240×160 presentation transform

不允許在 platform adapter 內修改：

- event 順序
- RNG 呼叫順序
- enemy update 順序
- 座標、速度與碰撞公式
- damage、armor、score、cash、reward 規則
- Boss 狀態轉換

所有無法直接保留的差異要以 `GBA_PORT` 註解及本文件記錄。

## 下一個直譯階段

1. 建立 source-parity 60-entry projectile pool，把 Stage 3 的 200 個
   trigger 轉成完整 weapon movement／animation／collision。
2. 將 source pool 的 presentation 接到 GBA renderer，但不改內部
   320×200 gameplay 座標與 update order。
3. 移植玩家射擊、碰撞、死亡、獎賞及 Boss 結束流程，讓
   `globalFlags`／`enemydie` 進入真實生命週期。
4. 重新驗證 type-61 分支、death spawn 與 reward lifecycle。
5. source-parity loop 成為 authoritative 後移除 legacy event/gameplay。

## 建置

```powershell
.\build.ps1
```

目前 ROMFS v15 ROM：

```text
build/tyrian_gba_level1_source_parity_romfs_v15.gba
```

ROM 與中間產物不納入 Git。

## 授權

新的直譯程式衍生自 OpenTyrian GPL 原始碼，使用
`GPL-2.0-or-later`，並在 repository 根目錄保存原始 `COPYING`。
MT19937 段落沿用 OpenTyrian `mtrand.c` 的 BSD 3-clause 授權，完整
copyright、條件與 disclaimer 保留在 `src/opentyrian_level_port.c`。
