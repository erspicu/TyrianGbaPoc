# Tyrian GBA v26：玩家死亡與 Game Over 逐行移植

更新日期：2026-07-26

## 目標

v26 補齊 OpenTyrian 第一關玩家受到致命傷害後的流程：

1. 原始死亡計時、雙大型爆炸與隨機爆炸音效。
2. 關卡音樂逐 tick 淡出。
3. 保留最後一幀戰鬥場景並疊出 `GAME OVER`。
4. 播放真正的 Game Over 曲目。
5. 玩家按鍵後回到 Game Menu，這時才恢復選單曲。

一般 release 仍依開發需求保留無敵旗標；另建的強制死亡 ROM 會明確關閉
無敵，完整驗證實際死亡路徑。

## OpenTyrian 對照流程

致命傷害沿用 `varz.c` 的 `JE_playerDamage()` 語意：

- 玩家生命狀態切為死亡。
- `exploding_ticks = 60`。
- `levelEnd = 40`。
- 音量暫存值設為 255。
- 致命幀立即播放 `S_EXPLOSION_22`。

接下來每個死亡邏輯 tick 保留原始呼叫順序：

1. 前一幀已啟用 fade 時，把音量減 1。
2. 遞減 `exploding_ticks`。
3. 以同一套 MT19937 stream 計算 `levelEndFxWait = rand % 6 + 3`。
4. cadence 到期時，以 `rand % 3 == 1` 選擇 `S_EXPLOSION_9`，否則使用
   `S_EXPLOSION_11`。
5. 以原始 ±16 像素範圍產生隨機位置的大型爆炸。
6. 在玩家中心再產生一組大型爆炸。
7. `levelEnd` 尚未歸零時遞減，並啟用下一幀 fade。

致命幀本身不執行 fade，因此 60 tick 實際得到 59 次一單位淡出。第 60
tick 結束、`exploding_ticks == 0` 且 `levelEnd == 0` 時，在同一個
logic update 進入 Game Over。

為避免 GBA adapter 自己產生另一條亂數序列，v26 將
`opentyrian_level_port.c` 內已翻寫的 MT19937 透過
`ot_level_port_random()` 提供給死亡流程；測試會直接檢查亂數呼叫數。

## 爆炸 pool 與 GBA 顯示預算

OpenTyrian 的 `MAX_EXPLOSIONS` 是 200。舊 GBA adapter 只有 48 個 effect
slot，無法容納死亡動畫每 tick 兩組大型爆炸。v26 改成：

- 邏輯 effect pool：200，對照 PC allocator。
- 畫面每幀 effect 預算：48，作為 GBA OAM presentation 限制。
- 超過可見預算時保留較新的 PC blit。
- 以 `effect_slot_high_water` 只掃描曾用到的 slot，避免正常第一關每幀
  固定掃描 200 格。

這個限制只裁切當幀 presentation，不改動 effect 的建立、TTL、動畫與
釋放狀態。強制死亡測試峰值為 96 個 active effects，allocation drop
為 0；進入 Game Over 時實際提交 53 個 OAM。

## Game Over 畫面

PC 流程是在最後的 gameplay 畫面執行：

```c
JE_dString(VGAScreen, 120, 60, miscText[21], FONT_SHAPES);
```

GBA 版因此不切換到黑色的 Mode 4 靜態頁面。`STATE_GAME_OVER` 保持
Mode 0、凍結最後一次 gameplay composition，並以既有
`FONT_SHAPES` GBA presentation adapter 疊出 `GAME OVER`。

8 個 glyph 共 16 個 OBJ tile。其 cartridge backing 放在上層 enemy
cache 未使用區；進入 Game Over 時，VBlank DMA 到 runtime tile
512..527。文字先送入 OAM，因此在同 priority 重疊時維持最上層。

這是對原始顯示呼叫的硬體轉接；字型仍使用既有的 GBA 8×12 adapter，
不是宣稱逐像素重建 PC software renderer。

## 音訊

v26 soundbank 新增：

- `11_game_over_solo.tym` 轉換的 `tyrian_game_over_full.it`。
- `explosion_11.wav`。
- `explosion_22.wav`。

Game Over 使用 PC MUS zero-based song index 10。進入 Game Over 時只切換
module，不停止最後仍在播放的 explosion effect。來源 0..255 音量依
既有 Maxmod 校準曲線映射到 0..896，保留逐 tick fade。

任意按鍵後才執行：

- 離開 Game Over。
- 回到 Game Menu，selection 保留在 `Play Next Level`。
- 切換到 PC title song index 29。
- 從 Mode 0 回到前端 Mode 4。

## 專用死亡測試

建置目標：

```powershell
make death-autotest
```

`build.ps1` 會自動建置及執行：

```text
tyrian_gba_level1_pc_flow_mode4_death_autotest_romfs_v26_<config>.gba
```

測試 ROM 使用 game code `TYGD`，並編譯：

```text
AUTOTEST
AUTOTEST_FORCE_PLAYER_DEATH
AUTOTEST_DEATH_FLOW
TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0
```

Low Detail／Normal Speed 的 deterministic 結果：

| 項目 | 結果 |
|---|---:|
| 死亡 tick | 60 |
| 大型爆炸呼叫 | 120 |
| `S_EXPLOSION_9` | 3 |
| `S_EXPLOSION_11` | 6 |
| `S_EXPLOSION_22` | 1 |
| 音樂 fade step | 59 |
| MT19937 呼叫 | 138 |
| Game Over music start | 1 |
| active effects | 96 |
| effect allocation drop | 0 |
| Game Over OAM | 53 |
| Game Over 曲目索引 | 10 |
| 返回後曲目索引 | 29 |

測試同時確認：

- Game Over 保持 Mode 0，不切到靜態 Mode 4。
- 玩家為死亡狀態，`exploding_ticks` 與 `levelEnd` 都歸零。
- `GAME OVER` overlay 至少呈現 4 個 frame。
- 按鍵後到 Game Menu，前端恢復 Mode 4。
- runtime error、effect drop 與音樂狀態錯誤皆為 0。

## 四組回歸

| Detail | Game Speed | 完整關卡 | 死亡流程 | 邏輯更新 | 顯示 frame | 最大 OAM |
|---|---|---:|---:|---:|---:|---:|
| Low | Normal | PASS | PASS | 7,832 | 13,509 | 89 |
| Normal | Normal | PASS | PASS | 7,832 | 13,509 | 89 |
| Low | Low | PASS | PASS | 7,832 | 16,872 | 89 |
| Normal | Low | PASS | PASS | 7,832 | 16,872 | 89 |

四組完整關卡仍處理 935 個來源事件、100 個敵人擊破、Boss、離場與統計；
死亡測試的 120 次爆炸、59 次 fade、138 次 RNG 及兩個音樂索引也完全
一致。

預設 Low Detail／Normal Speed release：

```text
ROM bytes = 12,019,376
32 MiB 使用率 = 35.8205%
SHA-256 = 2738a0e27ca451f131cb8cfb11999020c4100ea630ba41ff50b25d45ddcde341
```

## 下一階段

依 Updated Plan 進入 P3 Jukebox：

- 啟用主選單的 Jukebox。
- 對照 PC 星塵／粒子背景與曲目切換流程。
- 使用 GBA tile、palette cycling 及少量 OBJ，避免按鍵時重建整頁。
- 從 ROMFS catalog 循環選曲並正確恢復選單音樂。
