# Tyrian GBA Updated Plan v36

更新日期：2026-07-28

工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay authority 是 OpenTyrian C 與 ROMFS 內的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 GBA-only 武器／敵人資料表。
- 最佳化必須先確認真實呼叫量、objdump 與固定負載 A/B。
- Drop-frame 只省略 presentation；logic、RNG、碰撞與關卡時間不減速。
- Maxmod BGM 保留。
- 一般 build 至少保留 48 KiB EWRAM 與 6 KiB IWRAM。

## P12：GBA 上限與 Drop-frame（完成）

- wall-clock fixed logic；
- whole-scene presentation defer；
- background ownership freeze；
- missed-VBlank recovery；
- 每個實體 period 一次 `mmFrame()`；
- recovery period 禁止 active-display VRAM DMA。

v36 極端負載：

- 3,600 wall/audio frames；
- 2,096 logic ticks；
- 583 missed VBlanks；
- 1,470 completed scenes；
- 128 OAM；
- logic backlog 0。

## P13：Hotpath 實測與 IWRAM 配置（完成）

- `TGW8` 新增 RNG、ratio、enemy／enemy-shot 呼叫量。
- `TGW8` 新增 IRQ-masked 10,000-call RNG 微基準。
- `ot_mt_rand()` 搬入 ARM/IWRAM，329.08 → 206.67 cycles/call。
- 保留原始 index `if`；手寫 624 mask 實測略慢。
- `ot_round_ratio()` 只有 14 次，不為它建立 reciprocal table。
- enemy-shot branchless 版本沿用 v35 的實測否決。
- whole `ot_draw_enemy_pool()` IWRAM candidate 只剩 3,120 bytes，
  低於 6 KiB gate，否決。

詳細資料：

- `Tyrian-GBA-Hotpath-Evaluation-v36.md`

## P14：合法武器效能包絡（下一階段）

1. 列出 stock 可同時裝備的 front／rear／sidekick／special 組合。
2. 恢復 stock ammo、cooldown、charge 與設備互斥。
3. 用 deterministic Episode 2 route 測：
   - logic／render／collision average 與 max；
   - missed VBlank；
   - OAM cull；
   - visible cache capacity；
   - audio recovery parity。
4. 分類可直接使用、需限速、需互斥與 GBA 不採用的組合。

## P15：Campaign source parity（進行中）

1. 把四關 smoke campaign 擴大成 Episode 1 完整 Full Game。
2. 驗證 Episode 間 `]G`、`next_section`、`]Q` 與前端轉場。
3. 保存 cash、cube、weapon、armor、shield 與 global flags。
4. 補齊 turret 251..255、magnet、special effects 與 misc-shot 104。

## 固定回歸

- Episode 1 gameplay／Boss golden。
- Episode 2 完整 route。
- 62-section ROMFS／Sprite2 matrix。
- 四關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal／High／Pentium；Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、32 KiB IWRAM、96 KiB VRAM、32 MiB ROM。
- `TGW8` fixed-workload、audio-wall parity 與 RNG output parity。

## 成果管理

- `build` 最後只留最新可玩 ROM。
- 其他 GBA ROM 移到 `Backup`，ROM 不進 Git。
- MD、程式與測試工具 commit／push。
- 階段完成或需要決策時使用 UTF-8 郵件通知。
