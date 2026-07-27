# Tyrian GBA v33：Pentium Detail 與全武器滿載上限

日期：2026-07-27

分支：`opentyrian-source-parity-port`

狀態：可手動測試版完成

## 可手動測試 ROM

檔案：

`build/tyrian_gba_full_loadout_playable_v33_detail_pentium_speed_normal.gba`

建置設定：

- Detail Level：Pentium
- Game Speed：Normal
- 開發無敵：開啟
- 前端、選單、章節選擇與暫停流程：保留一般可玩版流程
- 關卡 BGM：保留 Maxmod
- 武器密集 SFX：改用 GBA 原生 PSG square channel，避免每發武器都
  啟動 Maxmod effect mixer

最終檔案驗證：

- ROM bytes：14,206,992（13.549 MiB）
- SHA-256：
  `1481f9ba898c1b1d12956f83c5e2444dd63de10f4616a4a25e7b042d8ccc0a94`
- GBA title／game code：`TYR FULL ARM`／`TYGP`
- GBA header complement：通過
- 600-frame mGBA boot：`AGB-TYGP,600,77810,software`
- EWRAM／IWRAM 剩餘：45,024／6,720 bytes

操作：

- 方向鍵：移動
- `A` 或 `B`：同時啟動主武器、後置武器、左右側翼、特殊武器與
  Super Bomb
- `START`：依一般版流程暫停／繼續

這是刻意把所有高負載系統無限供應、同時發射的硬體上限測試，不是
平衡後的正式裝備。連續按住 `A`／`B` 時預期會非常卡；放開射擊鍵後
可直接比較一般關卡與滿載時的差距。

## 裝備來源

所有 weapon、port、option、special 與 chain 定義都在 runtime
直接由 ROMFS 內的 stock `tyrian.hdt` 讀取。程式不建立 GBA 專用
武器數值表。

| 系統 | stock HDT 選擇 | power 11／weapon |
|---|---|---:|
| Front | Port 30 Scatter Wave | 608 |
| Rear | Port 22 Rear Mega Pulse | 443 |
| Left sidekick | Option 22 BattleShip-Class Firebomb | 729 |
| Right sidekick | Option 14 8-Way MicroBomb | 298 |
| Special | Special 41 SDF Main Gun，type 9 | 778 |
| Super Bomb | `JE_mainGamePlayerFunctions()` stock weapon | 535 |

實作保留 stock weapon 的：

- `multi`、`max`、`shotrepeat` 與 multi-position；
- `sx/sy`、`acceleration`、跟隨玩家的特殊速度值；
- graphic、animation、trail、attack、infinite penetration；
- attack 100..249 的 chain weapon 生成；
- attack 250 以上的 source damage 語意。

MicroBomb 與 Super Bomb 在這個 diagnostic build 中刻意改為無限供應，
以維持全系統同時工作的最壞情況。

## Super Bomb 顯示

stock chain weapon 87 使用 `OPTION_SHAPES` table 5、graphic 33，
原始 framebuffer-blend 圖形是 80×79 pixels。

GBA OBJ 單體最大 64×64，因此 adapter：

1. 從 ROMFS／完整無損 Sprite2 raw bank 讀取；
2. 保持 1:1 像素，不縮放；
3. 中央裁切為 64×64；
4. 使用 GBA semi-transparent OBJ 與硬體 alpha；
5. 碰撞仍使用 source 80×79 的半徑 40×39，不因顯示裁切縮小。

## Detail Level 實作

四種設定均保留，可由 Makefile 選擇：

```text
DETAIL_LEVEL=low
DETAIL_LEVEL=normal
DETAIL_LEVEL=high
DETAIL_LEVEL=pentium
```

- Low／Normal／High／Pentium 保留 OpenTyrian processor profile 的次序。
- Normal 以上開啟第二背景層。
- High 會接受並記錄 source event 64 的 lava／water filter request。
- Normal 以上會接受並記錄 iced／blur filter request。
- Pentium 在 source wild／透明第二背景條件下，以 GBA BG1 對
  BG0／BG2／backdrop 的硬體 alpha 近似 PC wild blend。

PC 的 lava、water、iced、blur 是 320×200 software framebuffer
post-process；目前 GBA gameplay 使用 Mode 0 tiled backgrounds，不能逐像素
直接套用同一演算法。本版保留 source event／狀態與 profile gate，並對
Pentium wild 使用硬體 blend；其餘四種 filter 尚未宣稱 pixel parity。

## 壓力測試結果

Episode 2 第一關固定跑 3,600 display frames、按住全武器：

| Detail | missed VBlank | 比率 | max OAM | shot spawn／drop |
|---|---:|---:|---:|---:|
| Normal | 2,111 | 58.64% | 128 | 12,374／639 |
| High | 2,111 | 58.64% | 128 | 12,374／639 |
| Pentium | 2,111 | 58.64% | 128 | 12,374／639 |

共同峰值：

- active player shots：81／81
- projectile cache unique：18，drop 7,015
- enemy Sprite2 cache unique：17，drop 881
- active explosions：65
- visible enemies：16
- chain volleys：253
- EWRAM Sprite2 L2 drop：0
- effect cache drop：0

Pentium 在本關 3,600 frames 都啟用 wild blend；Episode 2 第一關沒有
event 64 lava／water／iced／blur request。三種 detail 的 missed VBlank
完全相同，表示這個案例的瓶頸是無限全武器造成的 81-shot pool、
128 OAM、projectile cache 與碰撞工作量，不是 Pentium blend。

## 音訊對照

相同 Maxmod 武器 SFX 路徑為 2,142／3,600 missed VBlanks；密集 SFX
切到 PSG 後為 2,111／3,600，只省 31 frames（0.86 percentage point）。

因此：

- 背景音樂品質可保留；
- 將密集武器 SFX 改為 PSG 有正面但很小的效益；
- 音訊不是這個最壞案例的主要瓶頸。

## 一般裝備基準

Episode 2 第一關完整 deterministic route、一般裝備：

| Detail | display frames | missed VBlank | max OAM | cache drop |
|---|---:|---:|---:|---:|
| Normal | 10,475 | 5（0.0477%） | 42 | 0 |
| High | 10,475 | 5（0.0477%） | 42 | 0 |
| Pentium | 10,475 | 5（0.0477%） | 42 | 0 |

這證明 High／Pentium 本身在 GBA 上可用；不可用的是「六個最重系統
無限全開」這個刻意建立的上限情境。正式遊戲若遵守 stock ammo、
cooldown、裝備互斥與畫面存活時間，仍有很大的調整空間。

## 完整回歸

同一份 source tree 的 Low／Normal Speed 正式回歸已通過：

- Episode 1 gameplay golden；
- 玩家死亡、death music、Game Over 與回到前端；
- 41-song Jukebox；
- ROMFS／Sprite2 matrix：62/62 sections；
- Episode 1 四關 campaign：4/4；
- Episode 2 第一關 route：10,475 frames、3 missed VBlanks、
  0 background approximations、0 Sprite2/cache drops；
- 正式 ROM 600-frame boot。

新增的壓測欄位只存在 `TYRIAN_GBA_STRESS_LOADOUT=1` build。一般
campaign 的 `PlayerShot` 保持原本 8-byte adapter，Campaign 回歸仍有
49,272 bytes EWRAM 與 6,504 bytes IWRAM 安全餘量，沒有放寬既有
48 KiB／6 KiB build gate。

## 建置命令

可手動 Pentium 滿載版：

```powershell
make -j2 DETAIL_LEVEL=pentium GAME_SPEED=normal full-loadout-playable
```

保留的 High 版本：

```powershell
make -j2 DETAIL_LEVEL=high GAME_SPEED=normal full-loadout-playable
```

自動壓測：

```powershell
make -j2 DETAIL_LEVEL=pentium GAME_SPEED=normal full-loadout-stress
```
