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

Stage 1 已完成可編譯的直譯資料層與 shadow runtime。畫面上仍由 v11
legacy game loop 執行，所以此版本不能宣稱第一關已完成 source parity；
shadow runtime 的用途是讓每個後續函式都有可量測、可逐步取代的基礎。

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

完整結果位於 `build/verification.txt`。既有 v11 的 414 spawn、380 control、
434 collision、金額、掉落、暫停及回到標題等回歸數字維持不變。

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

1. 移植 `JE_makeEnemy()` 與 100-entry `enemyAvail`／enemy pool。
2. 將事件 6、7、10、12、15、18、32 的 `JE_createNewEventEnemy()`
   切換至 source-parity runtime。
3. 移植事件 19、20、27、31、33、45、60、61。
4. 移植 `JE_drawEnemy()` 的 update 部分，繪圖呼叫改送 GBA renderer。
5. 移植玩家射擊、碰撞、死亡、獎賞及 Boss 結束流程。
6. legacy event/gameplay runtime 完全移除。

## 建置

```powershell
.\build.ps1
```

Stage 1 ROM：

```text
build/tyrian_gba_level1_source_parity_stage1_v12.gba
```

ROM 與中間產物不納入 Git。

## 授權

新的直譯程式衍生自 OpenTyrian GPL 原始碼，使用
`GPL-2.0-or-later`，並在 repository 根目錄保存原始 `COPYING`。
