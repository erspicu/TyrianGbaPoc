# Tyrian GBA Updated Plan v28

更新日期：2026-07-27
工作分支：`opentyrian-source-parity-port`

## 固定原則

- Gameplay 使用 PC 264×184 source 座標，最終 1:1 裁成 GBA 240×160。
- 關卡事件、敵人、碰撞、RNG、獎賞與更新順序以 OpenTyrian C 原始碼為準。
- GBA 只改輸入、顯示、音訊、固定 pool 與 ROM I/O 邊界。
- 所有關卡直接讀 ROMFS 的原始 `levelsN.dat`、LVL、HDT、SHP、PIC、MUS
  與 background shape data。
- 禁止新增每關專用 Python 轉換表或產生一份 GBA-only 關卡資料。

## 已完成

### P1：第一關完整來源流程

- PC 事件、enemy、projectile、collision、death spawn、scoreitem 與 Boss。
- End-level flight、音樂、voice、分段 stats 與返回 Game Menu。
- 玩家死亡、Game Over 音樂／畫面與返回流程。
- Low／Normal Detail、Low／Normal Game Speed flags。

### P2：前端與 Jukebox

- Logo、Title、Play Mode、Episode、Difficulty、Game Menu、Next Level。
- 41 首 source song Jukebox、雙向環回、Mode 0 星空與 fade。
- 選單使用適合 GBA VRAM／DMA 的 presentation，不在每次按鍵重建整畫面。

### P3：通用 ROMFS runtime

- 62/62 LVL sections 與 53,338 events 已由 GBA runtime 實際讀取。
- 24 條 Episode／mode／difficulty route 全部可解析。
- 背景三層、enemy pool、event、HDT、Sprite2 與音樂都由 selected level
  決定，不再固定第一關。
- event 1..82 共用處理及跨關卡 `enemySpriteSheets[4]` pointer 語意。
- EP1–EP4 各有實機 gameplay route smoke。

### P4：Next Level 連續關卡

- Episode 1 已連續跑完 script sections 3、5、29、25。
- 四次都走 Game Menu → Next Level → gameplay → flight → stats。
- 第 24 個同幀 Sprite2 以 compact cache slot 呈現，零 cache drop。
- Low/Normal Detail 與 Low/Normal speed 的核心回歸均維持第一關 golden。

詳細數據見 `Tyrian-GBA-ROMFS-All-Levels-v28.md`。

## 目前限制

- Route smoke 的 EP4 仍需 4 次 test-only combat assist；原因是 PC special
  weapon 尚未完整移植，普通 Pulse Cannon 無法命中隱藏控制 hitbox。
- Matrix 中有 39 個特殊／bonus unknown records；OpenTyrian 本身會警告
  並略過，需在對應模式變成可達時逐一處理。
- 四關 campaign 證明共用資料與 Next Level pipeline，但尚未宣稱完整
  Episode 1 所有分支、商店／裝備與 episode transition 已完成。
- 右側 320×200 PC HUD、item／weapon shop 與 ESC options 仍依既定規格
  不進入目前 GBA gameplay viewport。

## 下一階段

### P5：完整 Episode 路徑

1. 將 campaign 擴大到 Episode 1 的完整 Full Game 路徑。
2. 驗證 `]G` 多目的地、`next_section`、`]Q` 與 Episode 2 轉場。
3. 保存每關後應延續的 player、cube、cash、weapon 與 global flag 狀態。
4. 對可達的特殊／bonus event 補齊直接翻寫。

驗收：同一 release ROM 可從 Episode 1 開始連續完成所有可達關卡並進入
Episode 2，不使用測試用 section override。

### P6：玩家武器與剩餘戰鬥語意

1. 逐行翻寫 PC front／rear／special weapon 與 shot shape。
2. 取代 GBA temporary Pulse Cannon adapter。
3. 移除 EP4 route-test combat assist。
4. 補齊 turret 251..255、magnet、special effect 與 misc shot 104。

驗收：代表性 EP1–EP4 route 不需要注入碰撞，仍可自然清除 stop group。

### P7：持續硬體回歸

每階段固定執行：

- `TGBA` 第一關 exact golden。
- `TGLM` 62-section ROMFS matrix。
- `TGCM` 多關 campaign。
- death／Game Over 與 41-song Jukebox。
- Low／Normal Detail、Low／Normal Game Speed。
- 128 OAM、256 KiB EWRAM、96 KiB VRAM 與 32 MiB ROM 上限。

## 成果管理

- `build` 最後只保留預設 Low Detail／Normal Speed 最新 release ROM。
- 其他 `.gba` 移到 `Backup`，測試 ROM 與中間檔不提交。
- 里程碑更新 MD、commit 並 push 到 source-parity branch。
- `.gba` 只在成果里程碑建立 GitHub Release，不進一般 git tree。
- 階段完成或需要使用者決策時，以 `tools/send_mail.py` 通知。
