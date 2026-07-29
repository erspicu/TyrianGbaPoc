# Tyrian GBA Updated Plan v35

更新日期：2026-07-27

工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 是 OpenTyrian C 與 ROMFS 內的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 GBA-only 武器／敵人資料表。
- Drop-frame 只省略 presentation；logic、RNG、碰撞與關卡時間不減速。
- BG、OBJ、OAM 與 register state 必須以完整 scene 原子前進。
- Maxmod BGM 保留；密集 full-loadout SFX 可使用 GBA PSG。
- 一般 build 至少保留 48 KiB EWRAM 與 6 KiB IWRAM。

## P8：Sprite2／背景效能（完成）

- 全 bank 無損 Sprite2 raw catalog、64-slot EWRAM L2。
- 32-bit palette pack 與 alignment-safe word／halfword stores。
- Collision-safe background cache、576／480／480 VRAM partition。
- Episode 2 一般 route：3／10,475 missed、0 approximation。

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
- hit-only collision result initialization；
- projectile presentation pre-cache cull；
- logic／render／collision／VBlank／audio telemetry。

下一步：

1. 把通用 HDT weapon runtime 接回正式 campaign inventory；
2. 取代 ordinary build 的 temporary Pulse Cannon adapter；
3. 恢復 stock ammo、charge、cooldown 與裝備互斥；
4. 補齊 turret 251..255、magnet、special effect、misc shot 104。

## P11：High／Pentium Detail

Profile 與 Pentium wild hardware blend 已完成。Lava、water、iced、blur
仍需在包含對應 event 64 的代表關卡驗證。不能 pixel parity 的效果要
明確記錄 GBA adapter，不以「High／Pentium」名稱掩蓋未移植效果。

## P12：GBA 上限與 Drop-frame（v35 完成）

上限配置：

- 81 active player shots；
- 128 OAM；
- 六套最重系統無限同時發射；
- Pentium Detail／Normal Speed；
- 完整 Maxmod BGM、密集 SFX 使用 PSG。

完成：

- wall-clock fixed logic；
- EWMA／deviation deadline scheduler；
- whole-scene BG／OAM freeze；
- 最多 22-row background ownership union；
- missed-VBlank recovery；
- 每個實體 period 一次 `mmFrame()`；
- recovery period 禁止 active-display VRAM DMA；
- TGW7 timing telemetry 與自動 parity gate。

最終 3,600-period 數值：

- logic：2,096；
- missed：590（16.39%）；
- audio：3,600／3,600；
- completed scenes：1,470（約 24.39 Hz）；
- max OAM：128；
- logic backlog：0；
- source workload parity：PASS。

這是不可滿幀的研究上限，不是正式內容的效能門檻。

## P13：合法武器效能包絡（下一階段）

1. 列出 stock 可同時裝備的 front／rear／sidekick／special 組合。
2. 恢復 stock ammo、cooldown、charge 與設備互斥。
3. 用相同 deterministic Episode 2 route 測：
   - logic／render／collision average 與 max；
   - missed VBlank；
   - OAM cull；
   - visible cache capacity；
   - audio recovery parity。
4. 分類為：
   - 可直接使用；
   - 需要降低 fire cadence；
   - 需要限制同時裝備；
   - GBA 規格下不採用。
5. 合法組合目標：
   - missed VBlank < 0.1%；
   - audio frames = wall VBlanks；
   - Sprite2 L2 drop = 0；
   - background approximation = 0；
   - visible projectile cache drop = 0。

## 不採用的 v35 候選

- bitwise `&`／`|` 強制雙軸 AABB；
- inclusive unsigned collision ring；
- enemy tracking 的 `dir * velocity`；
- runtime `% animax`；
- ARM7 假 SIMD／未使用的 De Bruijn ctz；
- 第二份 shadow OAM；
- OBJ tombstone；
- authoritative enemy SoA；
- 未證明 ownership 完整的 BG delta ring；
- 為了壓測數字靜音 Maxmod BGM。

## 固定回歸

- TGBA Episode 1 gameplay／Boss golden。
- TGRS Episode 2 完整 route。
- TGLM 62-section ROMFS／Sprite2 matrix。
- TGCM 多關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal／High／Pentium；Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、32 KiB IWRAM、96 KiB VRAM、32 MiB ROM。
- TGW7 fixed-workload／audio-wall parity。

## 成果管理

- `build` 最後只留最新可玩 ROM。
- 其他 GBA ROM 移到 `Backup`，ROM 不進 Git。
- MD、程式與測試工具 commit／push。
- 階段完成或需要決策時使用 UTF-8 郵件通知。
