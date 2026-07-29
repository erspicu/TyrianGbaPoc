# Tyrian GBA v17 敵人圖像與死亡獎賞逐行直譯

## 本階段範圍

本階段處理兩個明確的 source-parity issue：

1. 敵人死亡後的子物件、金額與實體獎賞，必須依 OpenTyrian 原始分支
   翻寫。
2. 敵人與獎賞的畫面不得再用 GBA 自訂 archetype 代替，必須由原始
   `shape table`、`egr[enemycycle - 1]` 與 `size` 決定。

行為基準固定為 OpenTyrian commit：

```text
1c34d1bddac8c8f2de834229d04b5a729525c944
```

目前仍是固定 single-player、Normal、`SA_NONE` 的第一關技術展示。
`curLoc=5400` 後會交給既有簡化 Boss；因此本文件所稱的完整，是指這兩套
規則與第一關全資源的轉接已完成，不代表原作 Boss lifecycle 已完成。

## 死亡生成物、金額與獎賞

對照來源如下：

| OpenTyrian | GBA 逐行翻寫 |
|---|---|
| `tyrian2.c` 玩家彈命中／死亡分支 | `ot_level_port_collide_player_shot()` |
| linked death 掃描與 `eenemydie` | `ot_kill_enemy_group()`／`ot_spawn_death_enemy()` |
| 直接 `evalue` credit | `ot_credit_destroyed_enemy()` |
| `mainint.c:JE_playerCollide()` | `ot_level_port_collide_player()` |
| `player.c:power_up_weapon()` | `ot_power_up_weapon()` |
| `player.c:handle_got_purple_ball()` | `ot_handle_purple_ball()` |

死亡分支保留原始順序：

```text
link 目標與 group match
→ special flagnum/setto
→ enemydie 在原 25-slot pool 生成
→ value > 30000 的 child 改用 pool 0
→ evalue 1 記 data cube；2..9999 直接加 cash
→ dlevel=-1 留 availability 2 殘骸並切 edgr
→ 其餘 slot release
```

`JE_playerCollide()` 也維持原始判斷優先序：

```text
30000+ weapon/special/sidekick 類
→ 20000+ armor
→ 10000+ availability-2 bonus portal
→ scoreitem：
   data cube / front power / rear power /
   orbiting asteroid / superbomb / HOT DOG / cash
→ 非獎賞敵人的碰撞
```

PC 的文字視窗、右側 HUD 重畫與 weapon-shot repeat array 屬於未移植 UI／
玩家武器 presentation；獎賞造成的 cash、weapon power、armor、superbomb、
special、bonus level 等 gameplay state 已由 `OtLevelPortState` 保存。
目前固定回歸路線實際遇到三個 cash item 與兩個 data cube，其他分支由
同一翻寫程式保留，但第一關 Boss handoff 前沒有觸發。

## 原始敵人與獎賞畫格

舊版的 24 個 presentation archetype 已完全移除。`JE_drawEnemy()` 在
呼叫 `blit_enemy()` 的相同 phase 留下不可變的 `OtEnemyDrawCommand`：

```text
x / y
shape_table
egr[enemycycle - 1]
size
filter
pool
```

這很重要，因為 PC 原作是在移動、發射與玩家彈碰撞之前畫圖；敵人可能在
同一 tick 稍後移動或死亡。GBA renderer 若重新讀取 tick 結尾的 enemy
slot，便會少畫死亡當幀或使用下一個位置。現在畫面只讀上述 pre-update
command。

Host 資源階段對第一關 1,009 筆事件做 spawn、`elaunchtype` 與
`eenemydie` transitive closure，共找到：

| 項目 | 數量 |
|---|---:|
| Enemy definitions | 113 |
| 唯一 `(shape_table, graphic, size)` 畫格 | 198 |
| 原始畫格資料 | 101,376 bytes |
| Catalog | 1,600 bytes |

畫格使用工作區已由 stock SHP 解出的 Sprite2 PNG，不重新設計輪廓。小型
enemy 保持原始 12×14 pixels；`size == 1` 完全依 `blit_enemy()` 組合：

```text
graphic + 0    graphic + 1
graphic + 19   graphic + 20
```

四塊位置為 `(0,0)`、`(12,0)`、`(0,14)`、`(12,14)`，得到 24×28
pixels，再置中於 32×32 GBA OBJ container。容器只解決 OAM 尺寸，不放大
或重畫來源圖。

金幣、寶石、data cube 與 power-up 不再走另一套 GBA reward artwork；
它們和 PC 一樣是 shape table 21／26 的 enemy object，animation cycle、
位置、速度、碰撞與畫格全部走相同 source pool。

GBA OBJ 是 4bpp，因此顏色必須量化成 16 色 palette bank；這是硬體
presentation adapter。圖形選擇、透明輪廓與 12×14／24×28 組合保持原始
資料，顏色則不是 PC 256 色 framebuffer 的 byte-for-byte 複製。

## OBJ 動態快取

全部 198 幀無法同時放進 32 KiB OBJ VRAM。v17 保留 24 個 32×32
動態 slot，每格 16 tiles／512 bytes：

```text
ROM exact-frame catalog
        ↓ binary search
24-slot true-LRU cache
        ↓ queued upload
VBlank DMA to OBJ VRAM
        ↓
shadow OAM commit
```

同一 presentation frame 正在使用的 slot 不會被替換。完全離開
240×160 viewport 的 command 先裁掉，不耗用 cache 或 OAM。OAM 發出順序
反轉 PC software-blit 順序，讓 GBA 的低 OAM index 規則仍呈現「PC 後畫
者在上」。

## v17 驗證結果

2026-07-26 使用 ARM GCC 16.1.0 與 mGBA 0.11.0 跑完整固定路線：

| 項目 | 結果 |
|---|---:|
| ROM internal／host verifier | PASS／PASS |
| Telemetry schema | 15 |
| Catalog entries／lookup misses | 198／0 |
| Cache hits／misses | 44,509／153 |
| Cache evictions／drops | 129／0 |
| Frame uploads／bytes | 153／78,336 |
| 單幀最高 uploads | 7 |
| 唯一 fallback visuals | 0 |
| Peak visible source objects／OAM | 35／47 |
| Death spawn attempt／success／full／missing | 5／5／0／0 |
| Score-item spawn／pickup／peak | 5／5／2 |
| Data cubes／unsupported pickup | 2／0 |
| Direct／pickup／final cash | 1,785／175／1,960 |
| Missed VBlank／display frames | 101／12,239（約 0.83%） |
| Runtime／ROMFS errors | 0／0 |

101 次 missed VBlank 是完整音樂、三層背景、source gameplay、爆炸及
動態 OBJ frame upload 同時運作的量測結果；沒有以降低 logic rate、刪除
音樂或省略圖像掩蓋。回歸上限設為 160，並獨立要求 catalog miss、
cache drop、stream drop、effect drop 與 projectile drop 全部為零。

可重建稽核輸出：

```text
res/enemy_frame_audit.csv
build/preview/enemy_frames_exact_catalog.png
build/verification.txt
```

`build` 的 preview 與 verification 是中間產物；使用
`.\build.ps1 -KeepIntermediates` 才會保留。

## 尚未跨過的邊界

- position 5400 後仍是簡化 Boss。Catalog 已包含第一關後段／Boss
  component 需要的畫格，但 source Boss event、armor link、movement、
  damage bar 與 level-end 尚未接管 runtime。
- 玩家死亡／生命／重生仍未移植。
- orbiting asteroid 的特殊 player-shot 104 尚缺玩家 misc-shot pool；
  對應 pickup state 已保留，本固定路線不會取得它。
- PC text window 與右側 HUD 刻意不顯示。
