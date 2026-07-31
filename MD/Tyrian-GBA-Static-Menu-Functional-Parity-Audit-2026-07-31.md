# Tyrian GBA 靜態選單功能對照稽核

- 日期：2026-07-31
- GBA 基準：`main` / v62 candidate
- C# 對照基準：`org/AprCSTyrian` / `36e4470`

## 1. 稽核目的與範圍

本文件只稽核 **TyrianGbaPoc 目前實際顯示、可選或明確標成停用的選項**，
確認它們是否只有 UI，或確實連到遊戲狀態、ROMFS 資料、裝備、關卡路由及
SRAM。沒有放進 GBA 介面的 PC 功能，例如 High Scores、Instructions、
Setup、雙人／Network、鍵盤／搖桿設定及隱藏高難度，不列為本次缺漏。

本次是原始碼功能追蹤，不是只比對截圖：

1. 從 GBA 的選項文字追到輸入 dispatch。
2. 確認 dispatch 是否改變持久狀態，而非只切換畫面。
3. 再追到關卡初始化、地圖 section、裝備套用、Data Cube 或 SRAM。
4. 對照 AprCSTyrian 的同名流程與狀態欄位。

狀態定義：

- **完整**：選項有真實作用，與目前納入範圍的 C# 核心語意一致。
- **部分**：不是假 UI，但仍少了 C# 的一部分流程或持久狀態。
- **僅 UI／停用**：看得到選項，但確認後不會執行對應功能。
- **GBA 改寫**：操作或動畫因硬體不同而改寫，但功能結果相同；視為已實作。

## 2. 最重要結論

目前確認的結果如下：

- **純粹只有 UI、沒有真正功能的選項只有首頁 `Load Game`。** 它目前刻意以
  暗色顯示，按下後只播放 `SFX_CLINK`。這符合先前「先放著但不能使用」的
  階段需求，但和 C# 首頁可直接載入存檔仍有明確落差。
- `Data`、`Ship Specs`、`Upgrade Ship`、`Options`、`Play Next Level`、
  `Quit Game` 都不是空殼；它們已有實際資料及狀態變更。
- 真正需要補齊的主要功能落差是：
  1. Arcade 沒切換成 C# 的專用關卡間選單，且 Arcade 殘機／重生流程未完整。
  2. Easy／Normal／Hard 能生效，但 C# 過關後依金錢提高的動態難度沒有延續。
  3. SRAM Load／Save 可用，但尚未涵蓋 C# 的全部劇情／重玩狀態，也沒有
     C# 的「LAST LEVEL」自動備份。
  4. Upgrade 已補上 C/C# 同一條即時武器發射模擬、兩段確認交易與關卡／
     存檔套用路徑；GBA 只把軟體像素輸出改接 Mode 4 + OBJ。
  5. JukeBox 可播放全部 41 首音樂，但少了 C# 的音效瀏覽及少數進階快捷鍵。

## 3. 首頁選項

| GBA 選項 | GBA 現況 | AprCSTyrian 行為 | 判定 | 實際落差 |
|---|---|---|---|---|
| Start New Game | 進入 Play Mode → Episode → Difficulty；之後建立新 campaign、初始裝備與金錢 | `newGame()` 走同一組選擇流程並初始化單人模式 | **完整** | 無核心落差 |
| Load Game | 暗色顯示；確認鍵只播放無效提示音 | 首頁呼叫 `JE_loadScreen()`，可以直接選存檔並載入 | **僅 UI／停用** | 尚未把已完成的 SRAM Load 畫面接到首頁 |
| Demo | 讀取 ROMFS `demo.1`～`demo.5`，套用關卡、裝備、音樂及逐幀輸入；首頁閒置 30 秒也會啟動 | 同樣循環原始 demo 檔並支援閒置播放 | **完整** | GBA 手把中止操作屬平台改寫 |
| JukeBox | 真正播放 41 首曲目，支援前後切歌、環狀切換、自動換歌、文字顯示切換及星空效果 | C# 位於 Setup → Jukebox，另有音效瀏覽、停止／重播及 fade 行為快捷鍵 | **部分** | 音樂播放核心完整；缺 sound-FX browser 與少數進階控制。入口位置改到首頁是 GBA 設計差異 |

GBA 首頁 dispatch 可見於
[`frontend_flow.inc`](../src/frontend/frontend_flow.inc#L2711)，其中 `Load Game`
分支只有 `SFX_CLINK`。C# 對照為
`org/AprCSTyrian/cs_ported/Core/Tyrian2.cs:237-267`。

## 4. Start New Game 前置選單

### 4.1 Play Mode

| 選項 | 已真正生效的內容 | 判定 | 尚未對齊的內容 |
|---|---|---|---|
| Full Game | Episode 初始金錢為 10000／15000／20000／30000；初始船型、商店、Data、分支關卡與統計流程皆走 campaign 狀態 | **完整** | 本稽核範圍內未發現假功能 |
| Arcade | 金錢 0、Stalker 船型、weapon energy 規則、Arcade 掉落邏輯、單一路線選擇與 Arcade 統計差異都有實際分支 | **部分** | C# 進入專用 `MENU_1_PLAYER_ARCADE`，只保留 Next Level、Options、Quit；GBA 仍顯示 Full Game 的 Data、Ship Specs、Upgrade。C# Arcade 可用殘機重生；GBA 的 `player_lives` 目前只初始化為 1，沒有完整消耗／重生流程 |

C# 專用 Arcade 選單 dispatch 位於
`org/AprCSTyrian/cs_ported/Core/GameMenuShop.cs:634-652`；GBA 現在所有模式都由同一個
[`STATE_GAME_MENU`](../src/frontend/frontend_flow.inc#L2751) 處理。

### 4.2 Select an Episode

| 選項 | GBA 實際作用 | 判定 | 差異 |
|---|---|---|---|
| Episode 1 | 選擇 Episode 1 的 item database、map script、LVL 與關卡資源 | **完整** | 無目前可見功能落差 |
| Episode 2 | 同上，實際切到 Episode 2 | **完整** | 無目前可見功能落差 |
| Episode 3 | 同上，實際切到 Episode 3 | **完整** | 無目前可見功能落差 |
| Episode 4 | 同上，並選用 Episode 4 內嵌 HDT item block | **完整** | 無目前可見功能落差 |

小差異：C# 會檢查 `episodeAvail[]`，不可用的章節會變暗且不能進入；GBA
固定允許四章。因目前 ROM 已完整打包四章資料，這項差異不會讓現有四個選項
變成假功能，但若未來製作裁切資源版 ROM，需補 availability guard。

### 4.3 Difficulty Level

| 選項 | GBA 實際作用 | 判定 | 差異 |
|---|---|---|---|
| Easy | 以 difficulty 1 送入 map／level resolver、敵人裝甲、射擊頻率與難度事件 | **部分（初始值完整）** | 過關後不會做 C# `adjust_difficulty()` |
| Normal | 同上，difficulty 2 | **部分（初始值完整）** | 同上 |
| Hard | 同上，difficulty 3 | **部分（初始值完整）** | 同上 |

C# 同時保存 `initialDifficulty` 與會動態上升的 `difficultyLevel`，並在過關摘要
依金錢計算最低應提升難度。GBA 目前只有 `frontend_difficulty`，每關都用它重新
建立 current／initial difficulty，因此 **選項本身會生效，但整個 campaign 的
難度演進尚未完整移植**。C# 對照為
`org/AprCSTyrian/cs_ported/Core/Mainint.cs:1775-1799`。

## 5. Game Menu

| 選項 | GBA 真正執行的功能 | 判定 | C# 對照後的落差 |
|---|---|---|---|
| Data | 使用 campaign 實際取得的 cube list，從 ROMFS 讀取標題、人物圖與全文；可選 Cube、閱讀及上下捲動 | **完整** | 沒有只做四張固定假畫面；資料內容及選擇都來自真正狀態 |
| Ship Specs | 依目前裝備的 ship ID 讀 HDT 船名、說明、圖形與數值並呈現 | **完整／GBA 改寫** | C# 的 scale-in 動畫已改成較適合 GBA 的硬體轉場；資訊功能相同 |
| Upgrade Ship | 讀當前 Episode 的真實 item inventory；預覽價格、武器 power、賣回價，採來源的兩段確認後扣款並改變下一關裝備；前後武器、Generator、左右 Sidekick 會執行即時發射模擬與 power bar | **完整／GBA 改寫** | 狀態、商品、成本、射擊與確認控制流直接對照來源；只把 PC 軟體 blit 改為 GBA Mode 4 + OBJ、分幀轉場與 4/5 座標 adapter |
| Options | 進入真正的 Load／Save／Done 頁 | **完整（容器）** | 只保留 GBA 已顯示的三項；PC 其他設定未顯示，故不列缺漏 |
| Play Next Level | 由 map script 產生真正目的地；確認後寫入對應 map section 並載入該關，Arcade 會取最後一條路線 | **完整** | 不是固定回第一關，也不是純 UI |
| Quit Game | 開啟 Yes／Cancel 對話；Yes 清除目前 campaign 並回首頁，Cancel 回 Game Menu | **完整** | 核心結果與 C# 相同 |

GBA 主分派位於
[`frontend_flow.inc:2751-2872`](../src/frontend/frontend_flow.inc#L2751)，C# 對照為
`org/AprCSTyrian/cs_ported/Core/GameMenuShop.cs:347-475`。

## 6. Upgrade Ship 子選項

下列八個畫面項目都有 handler。前七項都會修改 `frontend_player_items`；
第一次確認把預覽寫入等同 `old_items[0]` 的 accepted snapshot 並移到 Done，
第二次確認才執行等同 `JE_cashLeft()` 的結帳。進入下一關與 SRAM capture
都讀取同一份已確認裝備。

| 顯示項目 | 真正影響 | 判定 |
|---|---|---|
| Ship Type | 船體圖形、動畫及 Armor 上限 | **完整（購買／裝備）** |
| Front Gun | 前武器 ID、power、射擊資料及成本；即時執行來源 `player_shot_create()`／`simulate_player_shots()` 路徑 | **完整／GBA 改寫** |
| Rear Gun | 後武器 ID、power；R 可切換該武器的 mode；即時模擬對應 port mode | **完整／GBA 改寫** |
| Shield | Shield 類型、初始值及上限 | **完整（購買／裝備）** |
| Generator | 武器能源回復與護盾充能能力；預覽 power bar 依真實 `powerSys[].power` 回充 | **完整／GBA 改寫** |
| Left Sidekick | 左側 sidekick 實際種類、位置與即時射擊 | **完整／GBA 改寫** |
| Right Sidekick | 右側 sidekick 實際種類、style 2 位置與即時射擊 | **完整／GBA 改寫** |
| Done | 保留已確認交易並回 Game Menu | **完整** |

即時預覽不是重新設計的動畫：初始化常數、81 發 shot pool、100 顆星、
weapon-port op、shot repeat/multi-position、Sprite2 graphic、聲音 queue、
邊界、動畫與 Generator bar 都逐段翻寫自 `JE_initWeaponView()`、
`JE_weaponViewFrame()`、`JE_weaponSimUpdate()`、`player_shot_create()` 與
`simulate_player_shots()`。平台 adapter 只負責把來源 320x200 座標套用
既有 4/5 轉換，並以硬體 OBJ、WIN0 clipping、VBlank DMA 呈現。

## 7. Options、Load 與 Save

| 顯示項目 | GBA 真正執行的功能 | 判定 | 落差 |
|---|---|---|---|
| Load | 顯示 11 個槽；驗證 SRAM bank／CRC，載入 play mode、Episode、difficulty、section、裝備、金錢、Armor／Shield 與 Cubes，重新準備 map | **部分** | 真正可用，但只能從 Game Menu 進入；首頁 Load 未接線。另缺 C# 的部分跨章節狀態，見下表 |
| Save | 11 個槽、14 字元手把命名、雙 bank、sequence、CRC32、最後 commit byte、分幀寫 SRAM | **部分** | 儲存本專案目前主要 campaign 狀態；尚未達到 C# 完整語意，也沒有自動 LAST LEVEL backup |
| Done | 返回 Game Menu 並保留原本游標 | **完整** | 無核心落差 |

### 7.1 已保存且讀回的實際狀態

- Full Game／Arcade、Episode、Difficulty、main section。
- 船體、前後武器與 power、Shield、Generator、左右 Sidekick、Special、
  weapon mode。
- 金錢、Armor、目前／最大 Shield。
- Data Cube 數量與清單、存檔名、關卡名。

### 7.2 相較 C# 尚未保存或尚未建立的狀態

| C# 狀態 | 用途 | GBA 現況／風險 |
|---|---|---|
| `initialDifficulty` 與 `difficultyLevel` 分離 | 保留玩家最初選擇，同時允許動態難度上升 | GBA 只存一個 difficulty；補動態難度時必須升級 SRAM schema |
| `gameHasRepeated` | 記錄跨 Episode／重玩狀態，影響後續流程判斷 | GBA 沒有同等 campaign 欄位 |
| `secretHint` | 部分 map／秘密內容的隨機提示狀態 | GBA 沒有同等 campaign 欄位 |
| `last_items` | C# 存檔保留上一份裝備；商店另以 `old_items` 作本次交易 snapshot | GBA 商店已有 transient `frontend_upgrade_accepted_items`，其更新時點與 `JE_menuFunction()` 結尾一致；SRAM 尚未另存跨 session 的 `last_items` 欄位 |
| 自動 slot 11 `LAST LEVEL` 備份 | C# 每關結束後建立上一關備份 | GBA 11 槽目前全為手動存檔，沒有自動 checkpoint |

PC 的雙人欄位、input device 與 High Score 欄位沒有相對應的 GBA 顯示功能，
因此不列為這次靜態選單缺漏。GBA 使用自己的 SRAM 格式而不是 PC
`tyrian.sav`，屬平台必要差異，不視為 bug。

目前 SRAM 實作位於
[`frontend_save.inc`](../src/frontend/frontend_save.inc)，C# 完整欄位可見
`org/AprCSTyrian/cs_ported/Core/Config.cs:6-30,230-350`。

## 8. 其他已顯示的子頁面／確認項目

| 頁面或項目 | 功能判定 | 說明 |
|---|---|---|
| Data Cube 清單中的各 Cube | **完整** | 選到的是 campaign cube list 對應資料，不是共用佔位頁 |
| Data Cube Reader | **完整／GBA 改寫** | 顯示真正全文並可增量捲動；GBA 沒鍵盤 PageUp／Home，手把 Up／Down 屬等價操作 |
| Ship Specs 返回 | **完整** | A／B 都會回到原 Game Menu 船艦資訊項目 |
| Next Level 的各目的地 | **完整** | 名稱、planet、map section 都由目前 map script 決定 |
| Exit to Game Menu | **完整** | 不改變 section，返回 Game Menu |
| Quit Game：OK／Yes | **完整** | 結束目前 campaign，回首頁並切回 title music |
| Quit Game：Cancel | **完整** | 保留 campaign，回原 Game Menu |
| Save Slot 1～11 | **完整（就目前 schema）** | 每槽都可獨立 round-trip，不是共用同一份資料 |
| Save Name | **完整／GBA 改寫** | Up／Down 字元輪盤、R 大寫、Start 寫入、Select 長按清空 |

## 9. 建議補完優先順序

### P0：會讓已顯示模式產生不同遊戲流程

1. **Arcade 專用 Game Menu**：Arcade 不應開放 Data、Ship Specs、Upgrade；
   應依 C# 顯示 Play Next Level、Options、Quit Game。
2. **Arcade 殘機／重生**：建立與 C# `Player.Lives` 相同語意，死亡時有殘機就
   扣除、半 Armor／Shield 重生，而非一律 Game Over。
3. **動態難度**：拆開 initial/current difficulty，移植 `adjust_difficulty()`，
   並同步擴充存檔 schema。

### P1：Load／Save 完整性

1. 把首頁 `Load Game` 接到現有 11 槽讀檔流程；若仍要刻意停用，應維持現在
   暗色及 clink，避免使用者誤認為 bug。
2. 增加 `gameHasRepeated`、`secretHint` 與必要的初始／目前難度欄位。
3. 決定是否保留一槽作 `LAST LEVEL` 自動備份，或另設不佔手動槽的 checkpoint。

### P2：功能體驗完整度

1. JukeBox 視需求補音效瀏覽、Stop／Restart 與 fade toggle。這些不是目前
   41 首音樂播放的 blocker。
2. Episode 列表增加資源 availability 檢查，支援未來裁切版 ROM。

## 10. 驗證與證據界線

專案現有自動測試已覆蓋 Demo 五檔解析／輸入、JukeBox 切歌與環狀操作、
11 槽 SRAM round-trip／CRC fallback、Full Game／Arcade route，以及靜態選單
轉場壓測。Upgrade 額外驗證 Episode 1 三個 `]I` 商店邊界的七類商品、
預覽不購買、第一次接受／第二次結帳、現金、關卡裝備與存檔 capture；
120 次子選單轉場的 runtime SHP/Sprite2 decode、missed VBlank 與功能失敗均為 0。
這些能證明上述「已實作」項目不是只存在於繪圖碼。

本文件仍刻意把以下兩件事分開：

- **目前 GBA 功能能否正常工作**：例如現有 Save／Load 已能可靠 round-trip。
- **是否完整等同 C# 狀態模型**：例如目前 Save／Load 尚未保存
  `gameHasRepeated`，因此只能判為「部分」。

這樣後續補功能時，可以直接針對真正的狀態缺口處理，而不必重做已經有完整
資料流的 UI。
