# Tyrian Special Weapon 資料、屬性與 HUD 顯示規則

日期：2026-08-04
適用範圍：PC OpenTyrian 原始規格與 TyrianGbaPoc 目前移植狀態

## 結論

Tyrian 的 `Special Weapon` 是一個獨立裝備槽，同一時間只會裝備一項。PC 版在遊戲畫面左上角，使用該項目的 `itemgraphic` 從 `spriteSheet10` 拼成 24×28 圖示。四個 Episode 的名稱與圖示編號一致；47 筆定義中：

- ID 0 是 `None`。
- 24 筆具有非零 `itemgraphic`，能顯示正常左上角圖示。
- 22 筆的 `itemgraphic=0`，多半是 Super Arcade、事件、雙人或內部輔助定義，沒有可供 HUD 使用的專屬圖示。
- ID 32 `MicroSol Option2` 是唯一 Episode 屬性差異：Episode 1–3 為 `stype=18`，Episode 4 為 `stype=17`；名稱、武器與圖示狀態不變。

下圖完全由原始 `tyrian.shp` 的 `10_powerups` Sprite2 bank 導出；沒有重畫、量化或替換圖形。

![全部具有 HUD 圖示的 Special Weapon](assets/special-weapons/special-weapons-atlas.png)

部分項目共用綠色問號圖形（graphic 125）。這是原始 Tyrian 素材本身的設計，不是解包錯誤或 GBA 調色盤問題。

## HDT 欄位意義

每筆 Special Weapon 記錄為 37 bytes：

| 欄位 | 大小 | 意義 |
|---|---:|---|
| `name` | 31 bytes | Pascal 長度加最多 30 bytes CP437 名稱 |
| `itemgraphic` | `u16` | `spriteSheet10` 的一基底 Sprite2 編號；0 表示沒有圖示 |
| `pwr` | `u8` | 啟動成本編碼；某些 `stype` 也會把付款後數值用於持續時間或冷卻 |
| `stype` | `u8` | Special 行為分派編號 |
| `wpn` | `u16` | HDT Weapon 或 Option 定義編號；實際意義由 `stype` 決定 |

### `pwr` 不是單純威力值

PC `JE_doSpecialShot()` 的真實支付規則如下：

| `pwr` | 啟動成本／效果輸入 |
|---:|---|
| 0 | 免費 |
| 1–97 | 扣除相同數量的 Shield；不足則不能啟動 |
| 98 | 至少要有 4 Shield，啟動後耗盡全部 Shield；原 Shield 量成為後續效果值 |
| 99 | Shield 留下一半；取整後的一半成為後續效果值 |
| 100–255 | 扣除 `pwr-100` Armor，而且必須至少保留 1 Armor |

因此 `pwr=104` 代表消耗 4 Armor，不是「104 點威力」。`pwr=100` 的實際扣除量是 0，但仍要求玩家 Armor 大於 0。

## `stype` 行為分派

| `stype` | PC 原始行為 |
|---:|---|
| 1 | 直接發射 `wpn` 指定的 Weapon |
| 2 | Repulsor：把所有有效敵彈的速度往遠離玩家的方向修正一次 |
| 3 | Soul of Zinglon：啟動 50 tick 的縱向 Zinglon 光束與專屬音效，Special 冷卻 100 tick |
| 4 | Attractor：讓所有可收集且有價值的 `scoreitem` 加速朝玩家移動 |
| 5 | Flare：filter 7、頻率 2、持續 50 tick，隨機位置發射 `wpn` |
| 6 | SandStorm：filter 1、頻率 7，持續 `200 + 25×前武器等級` tick |
| 7 | MineField/Zinglon field：filter 3、頻率 3，持續 `50 + 10×前武器等級` tick，並啟動 Zinglon 效果 |
| 8 | 無濾鏡的短時間隨機武器場；頻率 7，持續 `10 + 前武器等級` tick |
| 9 | 與玩家位置連結的武器場；持續 `8 + 2×前武器等級` tick，`pwr` 同時成為追加等待時間 |
| 10 | 與玩家位置連結的武器場；持續 `14 + 4×前武器等級` tick |
| 11 | Astral Zone 隨機武器場；頻率為 `pwr`，並維護獨立 Astral 計時 |
| 12 | 無敵；一般模式持續 `付款後效果值×10` tick，Super Arcade 另有固定 100 tick 與 Weapon 707 行為 |
| 13 | 修復 Player 1 Armor，修復量為 `付款後效果值/4 + 1` |
| 14 | 修復 Player 2 Armor；GBA 單人版刻意不實作 Player 2 |
| 16 | 連結玩家的散射場；持續 `付款後效果值×16 + 8` tick，生成彈丸另加隨機 X/Y 速度 |
| 17 | 若左 Option 已是 `wpn`，再裝到右側；否則裝到左側 |
| 18 | 直接把 `wpn` 裝到右側 Option |

## 會顯示左上角圖示的 24 項

「成本」欄依 PC 原始 `pwr` 規則解碼。「前武器等級」指目前 Front Weapon Power。

| 圖示 | ID／名稱 | 原始屬性 | 成本 | 行為摘要 |
|---|---|---|---|---|
| <img src="assets/special-weapons/special-01-repulsor.png" width="48" alt="Repulsor"> | 1 Repulsor | graphic 271; pwr 1; type 2; wpn 0 | Shield 1 | 對所有有效敵彈施加一次遠離玩家的速度修正。 |
| <img src="assets/special-weapons/special-02-pearl-wind.png" width="48" alt="Pearl Wind"> | 2 Pearl Wind | graphic 273; pwr 10; type 1; wpn 620 | Shield 10 | 直接發射 Weapon 620。 |
| <img src="assets/special-weapons/special-03-soul-of-zinglon.png" width="48" alt="Soul of Zinglon"> | 3 Soul of Zinglon | graphic 275; pwr 50; type 3; wpn 0 | Shield 50 | 50 tick 的 Zinglon 縱向光束與專屬音效，冷卻 100 tick。 |
| <img src="assets/special-weapons/special-04-attractor.png" width="48" alt="Attractor"> | 4 Attractor | graphic 277; pwr 2; type 4; wpn 0 | Shield 2 | 使有效金錢、獎賞等 `scoreitem` 的速度朝玩家修正一次。 |
| <img src="assets/special-weapons/special-05-ice-beam.png" width="48" alt="Ice Beam"> | 5 Ice Beam | graphic 121; pwr 5; type 1; wpn 621 | Shield 5 | 直接發射 Weapon 621。 |
| <img src="assets/special-weapons/special-06-flare.png" width="48" alt="Flare"> | 6 Flare | graphic 123; pwr 10; type 5; wpn 622 | Shield 10 | filter 7、頻率 2、50 tick 的全畫面 Flare 武器場。 |
| <img src="assets/special-weapons/special-07-blade-field.png" width="48" alt="Blade Field"> | 7 Blade Field | graphic 125; pwr 20; type 8; wpn 623 | Shield 20 | 無濾鏡隨機 Blade 場，持續 `10 + 前武器等級` tick。 |
| <img src="assets/special-weapons/special-08-sandstorm.png" width="48" alt="SandStorm"> | 8 SandStorm | graphic 125; pwr 20; type 6; wpn 624 | Shield 20 | filter 1、頻率 7，持續 `200 + 25×前武器等級` tick。 |
| <img src="assets/special-weapons/special-09-minefield.png" width="48" alt="MineField"> | 9 MineField | graphic 283; pwr 104; type 7; wpn 29 | Armor 4 | filter 3 的 MineField，並同時啟動 Zinglon 效果。 |
| <img src="assets/special-weapons/special-10-dual-vulcan.png" width="48" alt="Dual Vulcan"> | 10 Dual Vulcan | graphic 131; pwr 20; type 9; wpn 658 | Shield 20 | 玩家連結型 Weapon 658 場；持續 `8 + 2×前武器等級`，追加等待 20 tick。 |
| <img src="assets/special-weapons/special-11-banana-bomb.png" width="48" alt="Banana Bomb"> | 11 Banana Bomb | graphic 125; pwr 20; type 1; wpn 525 | Shield 20 | 直接發射 Weapon 525。 |
| <img src="assets/special-weapons/special-12-protron-dispersal.png" width="48" alt="Protron Dispersal"> | 12 Protron Dispersal | graphic 125; pwr 30; type 10; wpn 670 | Shield 30 | 玩家連結型 Weapon 670 場；持續 `14 + 4×前武器等級` tick。 |
| <img src="assets/special-weapons/special-13-astral-zone.png" width="48" alt="Astral Zone"> | 13 Astral Zone | graphic 125; pwr 2; type 11; wpn 672 | Shield 2 | 隨機 Weapon 672 場；頻率 2，並啟動 Astral 計時。 |
| <img src="assets/special-weapons/special-14-xega-ball.png" width="48" alt="Xega Ball"> | 14 Xega Ball | graphic 127; pwr 20; type 1; wpn 720 | Shield 20 | 直接發射 Weapon 720。 |
| <img src="assets/special-weapons/special-15-megalaser-dual.png" width="48" alt="MegaLaser Dual"> | 15 MegaLaser Dual | graphic 279; pwr 20; type 9; wpn 751 | Shield 20 | 玩家連結型 Weapon 751 場，追加等待 20 tick。 |
| <img src="assets/special-weapons/special-16-orange-shield.png" width="48" alt="Orange Shield"> | 16 Orange Shield | graphic 125; pwr 80; type 9; wpn 749 | Shield 80 | 玩家連結型 Weapon 749 場，追加等待 80 tick。 |
| <img src="assets/special-weapons/special-17-pulse-blast.png" width="48" alt="Pulse Blast"> | 17 Pulse Blast | graphic 55; pwr 2; type 1; wpn 753 | Shield 2 | 直接發射 Weapon 753。 |
| <img src="assets/special-weapons/special-18-megalaser.png" width="48" alt="MegaLaser"> | 18 MegaLaser | graphic 91; pwr 100; type 9; wpn 754 | Armor 0，但需 Armor > 0 | 玩家連結型 Weapon 754 場，追加等待 100 tick。 |
| <img src="assets/special-weapons/special-19-missile-pod.png" width="48" alt="Missile Pod"> | 19 Missile Pod | graphic 125; pwr 80; type 9; wpn 755 | Shield 80 | 玩家連結型 Weapon 755 場，追加等待 80 tick。 |
| <img src="assets/special-weapons/special-37-invulnerability.png" width="48" alt="Invulnerability"> | 37 Invulnerability | graphic 129; pwr 98; type 12; wpn 0 | 全部 Shield；至少需 4 | 無敵時間為啟動前 Shield 數量×10 tick；Super Arcade 使用另一套固定規則。 |
| <img src="assets/special-weapons/special-40-lightning-zone.png" width="48" alt="Lightning Zone"> | 40 Lightning Zone | graphic 93; pwr 4; type 9; wpn 750 | Shield 4 | 玩家連結型 Weapon 750 場，追加等待 4 tick。 |
| <img src="assets/special-weapons/special-41-sdf-main-gun.png" width="48" alt="SDF Main Gun"> | 41 SDF Main Gun | graphic 125; pwr 4; type 9; wpn 778 | Shield 4 | 玩家連結型 Weapon 778 場，追加等待 4 tick。 |
| <img src="assets/special-weapons/special-44-pearl-wind.png" width="48" alt="Pearl Wind alternate"> | 44 Pearl Wind | graphic 273; pwr 10; type 8; wpn 620 | Shield 10 | 與 ID 2 共用名稱和圖示，但改為短時間隨機 Weapon 620 場。 |
| <img src="assets/special-weapons/special-45-8-way-microbomb.png" width="48" alt="8-Way Microbomb"> | 45 8-Way Microbomb | graphic 129; pwr 10; type 1; wpn 29 | Shield 10 | 直接發射 Weapon 29。 |

## 沒有左上角素材的 22 項

這些定義仍可能由事件或 Super Arcade 流程使用，但 `itemgraphic=0`，所以不能當作一般 2×2 HUD 圖示解碼。

| ID／名稱 | pwr | stype | wpn | 來源行為摘要 |
|---|---:|---:|---:|---|
| 20 Minefield | 104 | 16 | 710 | 消耗 4 Armor；玩家連結的散射 Weapon 710 場。 |
| 21 Post-It Blast | 105 | 1 | 708 | 消耗 5 Armor；直接發射 Weapon 708。 |
| 22 Drone ** | 103 | 12 | 0 | 消耗 3 Armor；一般 type 12 路徑給 30 tick 無敵，Super Arcade 有額外 Weapon 707 行為。 |
| 23 Repair Player 1 | 98 | 14 | 0 | 名稱如此，但 `stype=14` 的原始碼實際修復 Player 2；GBA 單人版不執行。 |
| 24 Super Bomb | 104 | 5 | 622 | Special 定義中的 Flare 型 Weapon 622；不是關卡內可累積的 Super Bomb 庫存。 |
| 25 Hot Dog | 101 | 1 | 711 | 消耗 1 Armor；直接發射 Weapon 711。 |
| 26 Lightning UP | 0 | 1 | 721 | 免費直接發射 Weapon 721。 |
| 27 Lightning UP+LEFT | 0 | 1 | 722 | 免費直接發射 Weapon 722。 |
| 28 Lightning UP+RIGHT | 0 | 1 | 723 | 免費直接發射 Weapon 723。 |
| 29 Lightning LEFT | 0 | 1 | 724 | 免費直接發射 Weapon 724。 |
| 30 Lightning RIGHT | 0 | 1 | 725 | 免費直接發射 Weapon 725。 |
| 31 MicroSol Option | 0 | 17 | 19 | 把 Option 19 裝到左側，若左側已有則裝右側。 |
| 32 MicroSol Option2 | 0 | 18／17 | 26 | Episode 1–3 直接裝右側；Episode 4 改為左側優先、已有才裝右側。 |
| 33 MicroSol Option3 | 0 | 17 | 21 | 把 Option 21 裝到左側，若左側已有則裝右側。 |
| 34 MicroSol Option4 | 0 | 18 | 22 | 直接把 Option 22 裝到右側。 |
| 35 MicroSol Option5 | 0 | 18 | 23 | 直接把 Option 23 裝到右側。 |
| 36 MicroSol Option6 | 0 | 18 | 25 | 直接把 Option 25 裝到右側。 |
| 38 Atom Bomb | 102 | 1 | 19 | 消耗 2 Armor；直接發射 Weapon 19。 |
| 39 Seeker Bombs | 103 | 1 | 709 | 消耗 3 Armor；直接發射 Weapon 709。 |
| 42 Ice Blast | 4 | 1 | 706 | 消耗 4 Shield；直接發射 Weapon 706。 |
| 43 Repair Player 1 | 98 | 13 | 0 | 耗盡 Shield，修復 Player 1 Armor：`原 Shield/4 + 1`。 |
| 46 Protron Field | 99 | 1 | 707 | Shield 留下一半；直接發射 Weapon 707。 |

## 左上角圖示與狀態燈

PC 每幀有兩個獨立的顯示：

1. `JE_inGameDisplays()` 在 `(25,1)` 以 `blit_sprite2x2()` 畫目前 Special 的 `itemgraphic`。
2. `JE_doSpecialShot()` 在 `(47,4)` 畫獨立狀態燈：Special 可用時為 graphic 94，冷卻／作用中為 graphic 93。

| 狀態 | 原始圖示 |
|---|---|
| Ready，graphic 94 | <img src="assets/special-weapons/special-ready-graphic-094.png" width="24" alt="Special ready"> |
| Cooling down／active，graphic 93 | <img src="assets/special-weapons/special-cooldown-graphic-093.png" width="24" alt="Special cooldown"> |

目前 GBA 已經使用 HDT `itemgraphic` 與原始 2×2 Sprite2 拼法顯示裝備圖示；PC 的 93／94 獨立 Ready 燈尚未接到 GBA HUD，這是後續 source-parity 項目，不應誤認為武器圖示的一部分。

## `Super Bomb` 的兩套不同機制

名稱相近但不可混為一談：

| 機制 | 資料與保存 | HUD |
|---|---|---|
| Special ID 24 `Super Bomb` | 一般 `player_special` 裝備定義；`itemgraphic=0` | 沒有左上角專屬圖示 |
| 關卡掉落 Super Bomb | 敵人 `evalue=-4`；`player_superbombs` 最多 10，使用 Weapon 535，每關重置且不寫存檔 | 每顆用 `spriteSheet9` graphic 304 排列 |

關卡庫存使用的原始 graphic 304：

<img src="assets/special-weapons/superbomb-stock-graphic-304.png" width="24" alt="Super Bomb stock icon">

## GBA 目前移植稽核

| 項目 | 狀態 |
|---|---|
| HDT Special 讀取 | 已從 ROMFS 的 Episode item database 讀取 |
| 關卡特殊掉落 `evalue > 32100` | 已把 `evalue-32100` 寫入 `player_special` |
| 跨關與 SRAM 保存 | 已保存目前 Special ID |
| 24 筆有效圖示 | 已使用原始 Sprite2 資料顯示於左上角 |
| `stype` 1–18 效果 | GBA 已有相應分派；Player 2 專屬 type 14 刻意省略 |
| PC Shield／Armor 支付區塊 | **尚未完整移植**：目前 GBA 會讀取 `pwr` 作為效果／冷卻參數，但沒有完整執行 PC 的可負擔檢查與 Shield／Armor 扣除 |
| PC graphic 93／94 Ready 狀態燈 | **尚未顯示** |

上述兩個缺口只記錄現況，避免文件把目前 GBA 行為誤寫成已與 PC 完全一致；本文件本身不改變 gameplay 規格。

## 可重現素材導出

執行：

```powershell
.\.venv\Scripts\python.exe tools\export_special_weapon_reference.py
```

腳本會：

1. 分別解析 Episode 1–3 的 `tyrian.hdt` 與 Episode 4 `tyrian4.lvl` item database。
2. 驗證四個 Episode 的 Special 名稱與 `itemgraphic` 一致。
3. 從 `vendor/tyrian/image/sheets/10_powerups` 依 `graphic,+1,+19,+20` 組合 24×28 原始圖示。
4. 導出每個獨立 PNG、總覽圖、Ready／Cooldown 燈和 Super Bomb 庫存圖示。

## 主要來源

- PC 結構與 HDT loader：[`episodes.h`](../../vendor/opentyrian/src/episodes.h)、[`episodes.c`](../../vendor/opentyrian/src/episodes.c)
- PC Special 行為、成本與 Ready 狀態：[`varz.c`](../../vendor/opentyrian/src/varz.c)
- PC 左上角圖示：[`mainint.c`](../../vendor/opentyrian/src/mainint.c)
- PC 2×2 Sprite2 拼法：[`sprite.c`](../../vendor/opentyrian/src/sprite.c)
- GBA HDT reader：[`opentyrian_data.c`](../../src/opentyrian_data.c)
- GBA Special runtime：[`combat_runtime.inc`](../../src/combat_runtime.inc)
- GBA HUD：[`gba_scene.inc`](../../src/gba_scene.inc)
- 素材導出器：[`export_special_weapon_reference.py`](../../tools/export_special_weapon_reference.py)
