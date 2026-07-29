# Tyrian GBA Updated Plan v29

更新日期：2026-07-27
工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 仍是 OpenTyrian C 的 LVL／HDT／SHP 流程。
- GBA 使用 264×184 source 座標，最後 1:1 中央裁成 240×160。
- 不新增 per-level 或 event-limited 的 GBA-only 敵人／關卡資料。
- Build-time 只可預處理與關卡無關、可逐 byte／逐 pixel 證明無損的資料。
- Runtime palette、filter、draw order、graphic selection 與碰撞不可 bake。

## P8：Boss Sprite2 效能（完成）

- 37 banks、11,248 components 已在 build 時無損展開成 12×14 palette
  index raw。
- Runtime 使用 64×1 KiB palette-aware EWRAM L2；enemy 與 projectile
  共用。
- Front-end scratch 與 gameplay L2 以 union overlay，EWRAM 尚餘
  53,764 bytes。
- Game Pak 使用 `WAITCNT=0x4317`；上色熱路徑移入 ARM/IWRAM。
- Boss missed VBlank 437 → 4；全關 625 → 13。
- 62-section matrix 完成 6,146,816 個 runtime output pixel parity。
- Low／Normal Detail 與 Low／Normal Game Speed 組合均通過。

詳細數據見 `Tyrian-GBA-Boss-Sprite2-L2-v29.md`。

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
3. 移除 EP4 route-test combat assist。
4. 補齊 turret 251..255、magnet、special effect 與 misc shot 104。

驗收：EP1–EP4 代表 route 不注入測試碰撞，仍可自然清除 stop group。

## 每階段固定回歸

- `TGBA schema 25` 第一關 gameplay golden 與 Boss 效能視窗。
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
