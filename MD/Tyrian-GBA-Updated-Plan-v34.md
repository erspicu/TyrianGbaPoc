# Tyrian GBA Updated Plan v34

更新日期：2026-07-27

工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 是 OpenTyrian C 與 ROMFS 內的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 GBA-only 武器／敵人資料表。
- 顯示裁切不能刪除 source collision、damage 或關卡狀態。
- Low／Normal／High／Pentium 均保留，不以降低 BGM 品質掩蓋 CPU
  瓶頸。
- 一般 build 至少保留 48 KiB EWRAM 與 6 KiB IWRAM。

## P8：Sprite2／背景效能（完成）

- 完整 38-bank Sprite2 raw catalog 與 64-slot EWRAM L2。
- Collision-safe 背景 cache 與全域 576／480／480 VRAM partition。
- 一般 Episode 2 第一關：3／10,475 missed VBlank、0 approximation。
- Pre-cache projectile cull 已加入永久 golden。

## P9：四 Episode 與 Campaign（進行中）

1. 將四關 smoke campaign 擴大成 Episode 1 完整 Full Game。
2. 驗證跨 Episode `]G`、`next_section`、`]Q` 與前端轉場。
3. 保存 cash、cube、weapon、armor、shield 與 global flags。
4. 補齊實際可達的 special／bonus records。

## P10：玩家武器 source parity（進行中）

已完成：

- HDT weapon／port／option／special runtime reader；
- multi-position、repeat、速度、加速度、動畫、trail、attack；
- chain weapon、large Super Bomb 與六系統 full-loadout ROM；
- mutation-safe enemy active mask；
- projectile presentation pre-cache cull；
- collision／logic／render cycle telemetry。

下一步：

1. 把通用 HDT weapon runtime 接回正式 campaign inventory；
2. 取代 ordinary build 的 temporary Pulse Cannon adapter；
3. 恢復 stock ammo、charge、cooldown 與裝備互斥；
4. 補齊 turret 251..255、magnet、special effect、misc shot 104；
5. 建立合法裝備組合效能矩陣。

## P11：High／Pentium Detail

Profile 與 Pentium wild hardware blend 已完成；lava、water、iced、blur
仍需在含 event 64 的代表關卡逐項驗證。不能 pixel parity 的效果要記錄
明確 GBA adapter 規則。

## P12：GBA 上限研究（v34 定案）

刻意上限：

- 81 active player shots；
- 128 OAM；
- 六套最重系統無限同時發射；
- Pentium Detail／Normal Speed。

v34 對 v34 pre-cache baseline：

- collision avg：117,750.88 → 58,875.21 cycles；
- missed VBlank：2,086 → 1,370／3,600；
- workload 完全相同；
- full-loadout IWRAM 仍保留 6,432～6,488 bytes。

這個組合仍不可作正式滿幀目標。正式目標維持：

- 合法裝備 missed VBlank < 0.1%；
- Sprite2 L2 drop = 0；
- background approximation = 0；
- projectile visible-capacity drop = 0；
- OAM 依 source layer／重要度裁切，不刪 gameplay pool。

## P13：合法武器效能包絡（下一階段）

1. 列出 stock 可同時裝備的 front／rear／sidekick／special 組合。
2. 用相同 deterministic Episode 2 route 測平均與 max cycles。
3. 分開記錄 CPU、OAM cull 與 visible cache capacity。
4. 建立：
   - 可直接使用；
   - 需要降低 fire cadence；
   - 需要限制同時裝備；
   - GBA 規格下不採用。
5. 若合法組合仍受 18-slot projectile cache 限制，再設計有完整 owner
   與 eviction 規則的 OBJ tile cache；不先做高風險 unified cache。

## 固定回歸

- TGBA Episode 1 gameplay／Boss golden。
- TGRS Episode 2 完整 route。
- TGLM 62-section ROMFS／Sprite2 matrix。
- TGCM 多關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal／High／Pentium；Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、32 KiB IWRAM、96 KiB VRAM、32 MiB ROM。

## 成果管理

- `build` 最後只留最新可玩 ROM。
- 其他 GBA ROM 移到 `Backup`，ROM 不進 Git。
- MD、程式與測試工具 commit／push。
- 階段完成或需要決策時使用 UTF-8 郵件通知。
