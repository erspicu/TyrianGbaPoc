# Tyrian GBA Updated Plan v33

更新日期：2026-07-27

工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 是 OpenTyrian C 與 ROMFS 內的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 GBA-only 的武器／敵人資料表。
- Build-time 只容許完整 bank、palette-independent、可 round-trip
  驗證的無損解碼資料。
- GBA 使用 264×184 source 座標，最後 1:1 中央裁成 240×160。
- Low、Normal、High、Pentium 均保留；不以刪除高細節來掩蓋其他
  CPU／OAM／VRAM 瓶頸。

## P8：Sprite2／背景效能（完成）

- 完整 38-bank Sprite2 raw catalog。
- EWRAM L2、32/16-bit grouped pack、IWRAM/ARM hot paths。
- Collision-safe 背景 cache 與全域 576/480/480 VRAM partition。
- Episode 1 與 Episode 2 一般 route 保持近零 missed VBlank。

## P9：四 Episode 與 Campaign 流程（進行中）

1. 將目前四關 campaign 擴大成 Episode 1 完整 Full Game 路徑。
2. 驗證多目的地 `]G`、`next_section`、`]Q` 與跨 Episode 轉場。
3. 保存 cash、cube、weapon、armor、shield 與 global flag。
4. 補齊實際可達 special／bonus records。

## P10：玩家武器 source parity（第一個診斷階段完成）

已完成：

- HDT weapon／port／option／special runtime reader；
- stock multi-position、repeat、速度、加速度、動畫、trail、attack；
- chain weapon 與 large Super Bomb OPTION_SHAPES；
- 81-shot source pool 與 200-explosion stress pool；
- 六系統滿載 Pentium 手動 ROM 與 deterministic telemetry。

下一步：

1. 把同一套通用 HDT weapon runtime 接回正式 campaign inventory；
2. 取代 ordinary build 的 temporary Pulse Cannon adapter；
3. 恢復 stock ammo、charge、cooldown、equipment mutual exclusion；
4. 補齊 turret 251..255、magnet、special effect 與 misc shot 104；
5. 以代表性合法裝備組合建立效能 budget，不以無限六系統全開作
   正式遊戲門檻。

## P11：High／Pentium Detail（profile 完成，filter parity 待辦）

已完成：

- Makefile 與 runtime 的四級 profile；
- Normal 以上第二背景層；
- High lava／water request gate；
- Normal iced／blur request gate；
- Pentium wild 第二背景硬體 alpha；
- 各 filter request telemetry。

待辦：

1. 從有 event 64 的 18 個 stock section 選代表 route；
2. 分別驗證 lava、water、iced、blur 的畫面需求；
3. 評估 HBlank DMA／scanline offset、palette cycling 或短暫 Mode 4
   是否能在不破壞 tiled gameplay 的前提下接近 PC；
4. 無法達成 pixel parity 時，以明確的 GBA 硬體 adapter 規則記錄，
   不假稱已逐像素移植。

## P12：硬體上限與正式效能目標

目前最壞壓測為 81 active shots、128 OAM、六系統無限全開，
Normal／High／Pentium 均約 58.64% missed VBlank。這是硬體上限探勘，
不是正式遊戲可接受值。

正式路線目標：

- 一般合法裝備：missed VBlank < 0.1%；
- Sprite2 L2 drop = 0；
- background approximation = 0；
- projectile cache drop = 0，若超出則以顯示優先權裁切，不改 source
  碰撞與傷害；
- OAM 先依 source layer／重要度裁切到 128，不讓邏輯 pool 跟著消失。

## 每階段固定回歸

- `TGBA` Episode 1 gameplay golden 與 Boss 效能視窗。
- `TGRS` Episode 2 第一關完整 route 與效能 budget。
- `TGLM` 62-section ROMFS／Sprite2 pixel matrix。
- `TGCM` 多關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal／High／Pentium Detail；Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、32 KiB IWRAM、96 KiB VRAM、32 MiB ROM。

## 成果管理

- 本階段 `build` 只保留 Pentium 滿載可手動測試 ROM。
- 其他 ROM 移到 `Backup`，編譯中間檔清除。
- MD、程式與 build rule commit／push；GBA ROM 不提交。
- 階段完成或需要決策時，以 UTF-8 郵件通知。
