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

Stage 2 已完成可編譯的直譯資料層、100-entry enemy pool、
`JE_makeEnemy()`、`JE_createNewEventEnemy()` 及第一關 enemy-control
事件。畫面上仍由 v11 legacy game loop 執行，所以此版本不能宣稱第一關
已完成 source parity；shadow runtime 的用途是讓每個後續函式都有可量測、
可逐步取代的基礎。

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

## 原始資料匯出

`tools/build_assets.py` 現在同時產生兩套事件資料：

1. `level_events.bin`：既有 v11 POC 的簡化 bytecode，只供回歸基準使用。
2. `opentyrian_level1_events.bin`：完整原始 `JE_EventRecType`，供直譯
   game loop 使用。

原始事件檔具有 8-byte header，後接 1,009 筆 11-byte little-endian
record；欄位順序與 `src/varz.h` 一致：

```text
eventtime, eventtype, eventdat, eventdat2,
eventdat3, eventdat5, eventdat6, eventdat4
```

Stage 1 匯出結果：

| 資料 | 數量 | Bytes | SHA-256 |
|---|---:|---:|---|
| 原始第一關 events | 1,009 | 11,107 | `a1e458e2aedfe26c2a3f27349575980bb41d2de16abdde6700bcaf676faea833` |
| 第一關 HDT enemy dependency closure | 110 | 8,698 | `5c5959fdc2e46a376dd0dda03b5f4e680badb3cfd66ae60760953cf93811f971` |

Enemy closure 從第一關 spawn 與 type 33 目標開始，繼續追蹤
`elaunchtype` 與 `eenemydie`，每筆保存未修改的 77-byte
`JE_EnemyDat`。完整欄位稽核輸出在：

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

### Shadow 限制

`JE_drawEnemy()` 的 movement/off-screen release、碰撞、死亡與 death-spawn
尚未接管。因此 source pool 只會增加、不會釋放，完整 shadow 路徑會填滿
100 格；後續 373 次 pool-full 是此階段預期且有 assert 的量測結果，不是
最終遊戲行為。同理，特殊敵人死亡尚未寫入 `globalFlags`，所以目前四筆
type-61 分支依初始 flag=0 跳過；這些分支要在死亡流程移植後重新驗證。

15,928-byte source context 放在 GBA EWRAM BSS，不會把整塊零初始化資料
複製進 cartridge；ARM7 執行時仍只使用約 19.58 KiB EWRAM，距離 256 KiB
硬體上限很遠。

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

## ROMFS v1 資料層

v14 不改 Stage 2 gameplay 規則，而是加入後續逐行直譯所需的通用資料 I/O。
68 個 stock runtime 檔案以原始 bytes 封裝到 9,853,080-byte ROMFS image，
並由 `src/opentyrian_rom_io.c` 提供 `fopen`／`fread`／`fseek` 型態的唯讀
介面。資料直接留在 cartridge ROM，不占用 256 KiB EWRAM。

這取代「每移植一種 parser 就另外產生一套 C array」的做法；episode、
SHP、PIC、MUS、SND、HDT 及 LVL loader 可以保留 PC 版的 read order 與
offset 計算，只在既有 file helper boundary 改接 ROM backend。格式、路徑
規則、API、容量與擴充方式詳見
[Tyrian-GBA-ROMFS.md](Tyrian-GBA-ROMFS.md)。

2026-07-26 mGBA auto-test 驗證 82 項 ROMFS mount、lookup、read、seek、
typed little-endian read、EOF、path normalization、read-only mode 及
8-handle pool 檢查全部通過；Stage 2 原有 telemetry 也維持不變。

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

1. 移植 `JE_drawEnemy()` 的 movement、acceleration、animation、turret、
   launch 與 off-screen slot release。
2. 將 source pool 的 presentation 接到 GBA renderer，但不改內部
   320×200 gameplay 座標與 update order。
3. 移植玩家射擊、碰撞、死亡、獎賞及 Boss 結束流程，讓
   `globalFlags`／`enemydie` 進入真實生命週期。
4. 重新驗證 type-61 分支與 pool reuse，取消 Stage 2 的飽和預期。
5. source-parity loop 成為 authoritative 後移除 legacy event/gameplay。

## 建置

```powershell
.\build.ps1
```

目前 ROMFS v14 ROM：

```text
build/tyrian_gba_level1_source_parity_romfs_v14.gba
```

ROM 與中間產物不納入 Git。

## 授權

新的直譯程式衍生自 OpenTyrian GPL 原始碼，使用
`GPL-2.0-or-later`，並在 repository 根目錄保存原始 `COPYING`。
MT19937 段落沿用 OpenTyrian `mtrand.c` 的 BSD 3-clause 授權，完整
copyright、條件與 disclaimer 保留在 `src/opentyrian_level_port.c`。
