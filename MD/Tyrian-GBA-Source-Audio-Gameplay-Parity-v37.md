# Tyrian GBA v37：來源音效、最強單武器與 Episode 4 Sprite 修正

更新日期：2026-07-28
工作分支：`opentyrian-source-parity-port`

## 本階段目標

本階段把先前用於硬體極限測試的六套武器配置切回一般遊戲配置，並完成：

- 原始 Port 1 `Pulse-Cannon` 單一主武器，功率升至 11；
- 預設 `Detail Level = High`、`Game Speed = Normal`；
- 補齊全部 Tyrian 遊戲音效及人聲；
- 修正 Episode 4 第一關跨關卡 Sprite bank 汙染；
- 逐行對照補上 Secret Level 入口反應；
- Data Cube 提示音與金額顯示位置調整。

所有 gameplay 資料仍由 ROMFS 內的 stock HDT／LVL／SHP 決定，沒有建立
Episode 4 或第一關專用的 GBA 資源表。

## 最強單一 Pulse-Cannon

一般 release 不再使用 v36 的 full-loadout stress adapter。武器從
`tyrian.hdt` 依原始 `weaponPort[1].op[0][10]` 讀取：

| 欄位 | 原始值 |
|---|---:|
| Port | 1，`Pulse-Cannon` |
| Power | 11 |
| HDT weapon record | 165 |
| `shotrepeat` | 5 |
| `multi`／`max` | 5／5 |
| Sound | 25 |
| Graphics | 62、59 |
| Attack | 5、2、2、2、2 |

玩家每次發射直接沿用 `player_shot_create()` 的五發建立順序、`bx/by`、
`sx/sy`、加速度、存活時間、動畫及 Sprite2 sheet 選擇。一般版的玩家
彈也改由 ROMFS Sprite2 projectile cache 呈現，不再使用舊的固定 GBA
子彈圖。

固定回歸確認 `front_weapon_id=1`、`front_weapon_power=11`，而 rear、
sidekick、special 均未在一般 release 啟用。

## 完整 Tyrian 音效目錄

舊 soundbank 只含十個局部樣本，事件 16／62 寫入的多數人聲因此完全
沒有可播放的 Maxmod ID。v37 依 OpenTyrian `sndmast.h` 的 one-based
編號完整封裝：

- `tyrian.snd`：29 個一般 SFX；
- `voices.snd`：9 個人聲；
- 合計 38 個 `source_sound_01..38.wav`；
- 取樣率 11,025 Hz；
- 每個 voice 都依 `nortsong.c::JE_loadSndFile()` 移除損壞的末端
  100 bytes。

Runtime 使用固定 1..38 對 Maxmod sample 的完整映射，並依原版逐格清空
及排程八格 `soundQueue[]`。不再用舊的 16-bit sound mask，也不會只播放
同一 tick 中的一個效果。

本次補回的主要路徑包括：

- 非致命敵人打擊：`S_ENEMY_HIT`（3）；
- 小／大型敵人爆炸：`S_EXPLOSION_8/9`；
- 玩家 shield／hull／死亡：27、19、22，加上既有死亡爆炸 9／11；
- enemy weapon 與 launch 的來源聲音；
- 關卡開始 `V_GOOD_LUCK`（33）；
- Boss 接近 `V_BOSS`（31）及事件 16／62 的其他人聲；
- Data Cube 同 tick 播放 `S_ITEM`（18）與 `V_DATA_CUBE`（37）；
- 結算 `V_LEVEL_END`（34）。

自動測試會記錄實際呼叫過的 source sound bitset。第一關固定路線必須
到達：

```text
3,4,6,8,9,13,18,25,26,27,30,31,32,33,34,35,36,37
```

這項檢查會直接抓出 Boss、Good Luck、Data Cube 或其他遊戲中人聲再次
變成靜音的回歸。完整 soundbank 為 1,389,880 bytes。

## Secret Level 逐行對照

OpenTyrian `mainint.c::JE_playerCollide()` 對
`evalue > 10000 && enemyAvail == 2` 的處理已完整接上：

1. 第一次碰到物件時設定 `bonusLevel=true`；
2. `nextLevel=evalue-10000`；
3. 消耗該物件；
4. `displayTime=150`；
5. 保留當前 effects，切換到來源 song 30（Zanac）；
6. 顯示 `miscText[59]` 的 `Secret Level!`；
7. 完成關卡後以 `nextLevel` 優先於一般 `next_section`。

文字位置沿用 PC source `(90,10)`，再經既有的 264×184 到 240×160
中央裁切。顏色依 `flash=0..5` 對應原版 brightness `-8..-3` 六組
palette；150 tick 的閃爍方向及端點規則也與 `tyrian2.c` 相同。

自動測試另建立不影響正式關卡狀態的 `evalue=10042` probe，確認
`nextLevel=42`、物件消耗及 `displayTime=150`，之後立即重新初始化
source state 與 RNG。

## Episode 4 Sprite 修正

舊 adapter 曾把上一關的 `enemySpriteSheets[4]` 對應保存到下一關。
OpenTyrian 實際上在每次 `start_level()` 釋放四個 sheet，並在
`start_level_first()` 清空 `enemySpriteSheetIds`。跨 Episode 保存會使
Episode 4 的 shape slot 指到上一關 bank，造成錯圖或看似消失。

v37 每次進關都 cold-start 四個 shape slots；關卡內 event 5 的 bank
載入與同關 slot reuse 維持原始行為。這是全關共用生命週期修正，不含
Episode 4 特例。

Episode 4 section 1、High／Normal 的 mGBA route 結果：

| 指標 | 結果 |
|---|---:|
| Route pass | 1 |
| Logic／display frames | 7,563／13,047 |
| Missed VBlank | 6 |
| Unknown visuals | 0 |
| Sprite2 decode failures | 0 |
| L1／L2 drops | 0／0 |
| First decode failure | 0 |
| Raw builds／RLE fallback | 37／0 |

## HUD 與設定

- `DETAIL_LEVEL` 與 `build.ps1` 預設改為 `high`；
- `GAME_SPEED` 保持 `normal`；
- 金額文字由 GBA `(22,140)` 下移 8 pixels 至 `(22,148)`；
- 底部 banner 仍不移植；
- 開發驗證版仍保留 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=1`。

## 完整回歸結果

`build.ps1 -KeepIntermediates -DetailLevel high -GameSpeed normal`：

- 第一關 gameplay／Boss／離場／統計／Game Menu：PASS；
- 強制死亡與 Game Over：PASS；
- 41 首 Jukebox：PASS；
- 62/62 ROMFS sections：PASS；
- Episode 1 連續四關：PASS；
- Episode 2 第一關：PASS；
- Episode 4 第一關獨立 route：PASS。

關鍵效能：

| 路線 | Missed VBlank | Display frames |
|---|---:|---:|
| Episode 1 第一關 | 10 | 12,168 |
| Episode 1 Boss window | 2 | 439 |
| Episode 2 第一關 | 22 | 10,475 |
| Episode 4 第一關 | 6 | 13,047 |

Episode 2 的 22 frames 為約 0.21%；`stream drop`、Sprite2 decode、
L1/L2 drop、RLE fallback 仍全為零。

Release ROM：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v37_detail_high_speed_normal.gba
```

大小 14,513,304 bytes，約使用 GBA 32 MiB 上限的 43.253%。一般 release
尚餘 49,348 bytes EWRAM 與 8,160 bytes IWRAM。
