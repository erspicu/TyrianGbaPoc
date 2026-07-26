# Tyrian GBA Updated Plan v24

更新日期：2026-07-26
工作分支：`opentyrian-source-parity-port`

## 專案方向

專案已由第一關技術展示改為逐步完整移植。戰鬥流程以 OpenTyrian
原始 C 程式的資料、條件、更新順序與座標為準；GBA 只在顯示、輸入、
音訊與 ROM I/O 邊界做必要轉接。

戰鬥畫面維持 PC 264 × 184 區域的 1:1 座標，不做即時縮放；輸出時裁切
成 GBA 240 × 160。選單與 Logo 則可依 GBA VRAM／DMA 特性使用預先產生
的 Mode 4 畫面、tile 或局部更新，不要求逐像素照搬 PC renderer。

## 本階段已完成

### Detail Level

`src/port_config.h` 提供兩種建置設定：

- `TYRIAN_GBA_DETAIL_LOW`，預設值。
- `TYRIAN_GBA_DETAIL_NORMAL`。

Low 對照 PC 低細節模式，在第一關不配置與串流第二背景層，可省下約
20 KiB VRAM 上傳及 1,555 次 map row 更新。若後續關卡事件指定
`background2over == 3`，runtime 仍會在 VBlank 延遲恢復該層，保留原始
事件的強制顯示語意。

Normal 會顯示並串流第二背景層。玩家現金 HUD 仍保留，因為它是本移植已
確認需要保留的遊戲回饋，不因低細節模式關閉。

建置方式：

```powershell
.\build.ps1 -DetailLevel low
.\build.ps1 -DetailLevel normal
```

### Game Speed

提供：

- `TYRIAN_GBA_GAME_SPEED_LOW`。
- `TYRIAN_GBA_GAME_SPEED_NORMAL`，預設值。

Normal 使用既有的原版 Normal 邏輯更新比例。Low 對照 PC
`fastPlay == 5` 的 2／3 frame 交替節奏，平均邏輯速度是 Normal 的
4/5；音樂及 Maxmod 的 VBlank 更新不跟著降速。

建置方式：

```powershell
.\build.ps1 -GameSpeed low
.\build.ps1 -GameSpeed normal
```

### 拾取金額／獎賞提示

PC 版不是以一般文字繪製拾取金額，而是在 `scoreitem` 分支建立
`fixedPosition` explosion。v24 已依此處理：

- `explosiontype` 仍由 HDT enemy 定義決定。
- sprite 與 TTL 使用 `varz.c` 的 53 筆 explosion 定義。
- 畫面由 ROMFS 中的原始 `newsh6.shp` Sprite2 即時解碼。
- 保留 PC source 座標、裁切與 effect layer 優先權。
- 不再替所有 pickup 額外產生 GBA 自訂爆炸提示。

第一關自動測試實際拾取兩個 `scoreitem`，產生兩個固定金額效果，
pool drop 為 0。

## v24 驗證矩陣

| Detail | Game Speed | 結果 | 邏輯更新 | 顯示 frame | BG2 最終狀態 |
|---|---|---:|---:|---:|---:|
| Low | Normal | PASS | 7,828 | 13,502 | 關閉 |
| Normal | Normal | PASS | 7,828 | 13,502 | 開啟 |
| Low | Low | PASS | 7,828 | 16,863 | 關閉 |
| Normal | Low | PASS | 7,828 | 16,863 | 開啟 |

四種組合皆走完 935 個第一關事件、100 個敵人擊破、Boss group 清空、
1 個 data cube、2 個 `scoreitem` 拾取，最後回到 Game Menu。ROMFS
self-test、Sprite2 decode、map stream 與 reward pool 均為零失敗／
零丟失；最大硬體 OAM 使用量為 89/128。

預設交付組合維持：

```text
Detail Level = Low
Game Speed   = Normal
```

## 後續 Updated Plan

### P1：Boss 擊破與過關（v25 已完成）

1. 將 `levelEndWarp` 改回原始 signed 狀態並以 `-4` 起始。
2. 逐行翻寫玩家離場的加速、殘影與終止條件。
3. 接入原版 End of Level 曲目及 `Level completed` voice。
4. 還原分段顯示的 Cash、Enemies Destroyed、Data Cubes 與按鍵等待。
5. Full Game 完成後推進下一關；Arcade 依原版模式分支處理。

驗收：Boss 死亡後不立即切黑畫面；離場、音樂、統計與 Game Menu
順序和 PC 原始流程相同。

### P2：玩家死亡

1. 保留 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE` 開發旗標，release 驗證時可關。
2. 翻寫每 tick 的雙爆炸、隨機 SFX、音樂 fade 與死亡計時。
3. 在最後一幀關卡畫面上疊出 PC 風格 `GAME OVER`，播放真正 Game Over
   曲目，不提早切回 title music。
4. 按鍵後回到 Game Menu，才切換選單音樂。

驗收：另建強制死亡 auto-test，確認死亡曲目索引、GAME OVER 狀態、
按鍵返回與無敵旗標四項。

### P3：Jukebox

1. 啟用目前 disabled 的 Jukebox 選項。
2. 對照 PC 原始星空／粒子背景，依 GBA 特性使用 tile、palette cycling
   與少量 OBJ 組合，不每次按鍵重建全畫面。
3. 從 ROMFS 曲目 catalog 選歌，支援上一首／下一首及曲名顯示。
4. 退出時恢復選單曲與前端狀態。

驗收：曲目可連續切換、無 VRAM 重建卡頓、退出後音樂正確。

### P4：Episode、Next Level 與多關卡

1. Episode 選擇改為真正決定 episode data，不只改 UI selection。
2. `Next Level -> Tyrian` 改為讀取目前 episode／level，不固定第一關。
3. 讓過關結果更新 `nextLevel`，打通第二關及後續關卡。
4. MUS、SHP、PIC、HDT、LVL loader 逐步切到通用 ROMFS 原始檔介面。
5. 關卡背景、enemy bank、event、palette 與音樂改成資料驅動，禁止為
   每關新增一套 Python 特例。

驗收：至少 Episode 1 第一、二關可依同一套 loader 連續進入，第一關
程式碼不含專為第二關複製的分支。

### P5：持續圖層與效能回歸

每一階段都要同時回歸：

- PC 1:1 crop 與玩家完整可見邊界。
- `background2over`、`background3over`、`topEnemyOver`、
  `skyEnemyOverAll` 的前中後景順序。
- 敵人／可破壞背景／Boss 圖像及 palette。
- 128 OAM 上限、Sprite2 cache、VBlank row DMA 與 32 MiB ROM 上限。
- Low／Normal Detail 及 Low／Normal Game Speed 四種建置。

## 成果管理

- `build` 最後只保留預設 Low Detail／Normal Speed 的最新 release ROM。
- 其他可玩的 `.gba` 移到 `Backup`；測試 ROM 與中間檔不提交。
- 每個可玩里程碑更新 `MD`、commit 並 push 到目前 source-parity branch。
- `.gba` 只在成果里程碑建立 GitHub Release，不放進一般 git tree。
- 階段完成或遇到需要決策的阻塞時，使用
  `C:\ai_project\AprTyrianNes\tools\send_mail.py` 通知。
