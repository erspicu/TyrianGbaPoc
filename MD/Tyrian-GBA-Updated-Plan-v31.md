# Tyrian GBA Updated Plan v31

更新日期：2026-07-27
工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 是 OpenTyrian C 的 LVL／HDT／SHP 流程。
- GBA 使用 264×184 source 座標，最後 1:1 中央裁成 240×160。
- 不新增 per-level 或 event-limited 的 GBA-only 敵人／關卡資料。
- Build-time 只預處理與關卡無關、可證明無損的完整資源 bank。
- Runtime palette、filter、draw order、graphic selection 與碰撞不 bake。
- 效能修正以實測 telemetry 為準，不為了推測性收益降低音質。

## P8：Sprite2 與背景效能（完成）

- v29：完整 37-bank Sprite2 無損 raw catalog、64-slot EWRAM L2、
  `WAITCNT=0x4317`。
- v31：32-row BG hardware ring 只保護 21-row visible working set，
  加一列 prefetch／transition guard。
- Episode 2 第一關 missed VBlank 553 → 30，background approximation
  472 → 28。
- Sprite2 raw 上色使用 32／16-bit grouped stores。
- cache acquire、player-shot collision 與 raw writer 固定在 ARM/IWRAM。
- Episode 2 route 已加入每次 `build.ps1` 的永久效能回歸。
- 關閉音訊只再改善 30 → 29，保留完整 Maxmod，不改用 PSG。

## P9：完整 Episode 路徑

1. 將四關 campaign 擴大成 Episode 1 完整 Full Game 路徑。
2. 驗證多目的地 `]G`、`next_section`、`]Q` 與 Episode 2 轉場。
3. 保存 cash、cube、weapon、armor 與 global flag 的跨關狀態。
4. 對實際可達的特殊／bonus records 逐行補齊。

驗收：同一 release ROM 從 Episode 1 開始完成所有可達關卡並進入
Episode 2，不使用 section override 或 per-level asset。

## P10：玩家武器與剩餘戰鬥語意

1. 逐行翻寫 PC front／rear／special weapon 與 shot shape。
2. 取代 temporary Pulse Cannon adapter。
3. 移除 route-test combat assist。
4. 補齊 turret 251..255、magnet、special effect 與 misc shot 104。

驗收：EP1–EP4 代表 route 不注入測試碰撞，仍可自然清除 stop group。

## P11：極端背景工作集

Episode 2 logical level 6／11 的 authored 可見工作集本身略超目前每層
512 patterns。先以既有有界 approximation 保持可玩；進入這些關卡
的完整 route 後，再依實測選擇：

1. 調整三層 BG charblock 配額，讓 layer 1 使用超過 512 patterns；
2. 或建立完整 shape-bank、palette-independent 的通用 fragment
   backing。

不得建立 level 6／11 專用圖表或事件例外。

## 每階段固定回歸

- `TGBA schema 25` Episode 1 gameplay golden 與 Boss 效能視窗。
- `TGRS schema 3` Episode 2 第一關完整 route 與效能 budget。
- `TGLM schema 2` 62-section ROMFS／Sprite2 pixel matrix。
- `TGCM schema 3` 多關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal Detail、Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、32 KiB IWRAM、96 KiB VRAM、32 MiB ROM。

## 成果管理

- `build` 最後只保留 Low Detail／Normal Speed 最新 release ROM。
- 其他 release ROM 移到 `Backup`；test ROM 與中間檔不提交。
- 里程碑更新 MD、commit、push，ROM 只放 GitHub Release。
- 階段完成或需要決策時，以 `tools/send_mail.py` 通知。
