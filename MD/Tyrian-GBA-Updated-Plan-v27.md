# Tyrian GBA Updated Plan v27

更新日期：2026-07-26
工作分支：`opentyrian-source-parity-port`

## 專案方向

專案已由第一關技術展示改為逐步完整移植。戰鬥流程以 OpenTyrian
原始 C 程式的資料、條件、更新順序與座標為準；GBA 只在顯示、輸入、
音訊與 ROM I/O 邊界做必要轉接。

戰鬥畫面維持 PC 264 × 184 區域的 1:1 座標，不做即時縮放；輸出時裁切
成 GBA 240 × 160。選單與 Logo 則可依 GBA VRAM／DMA 特性使用預先產生
的 Mode 4 畫面、tile 或局部更新，不要求逐像素照搬 PC renderer。

## 已完成的共用設定

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

## v27 驗證矩陣

| Detail | Game Speed | 完整關卡 | 死亡流程 | Jukebox | 邏輯更新 | BG2 最終狀態 |
|---|---|---:|---:|---:|---:|---:|
| Low | Normal | PASS | PASS | PASS | 7,832 | 關閉 |
| Normal | Normal | PASS | PASS | PASS | 7,832 | 開啟 |
| Low | Low | PASS | PASS | PASS | 7,832 | 關閉 |
| Normal | Low | PASS | PASS | PASS | 7,832 | 開啟 |

四種組合皆走完 935 個第一關事件、100 個敵人擊破、Boss group 清空、
1 個 data cube、2 個 `scoreitem` 拾取，最後回到 Game Menu。ROMFS
self-test、Sprite2 decode、map stream 與 reward pool 均為零失敗／
零丟失；最大硬體 OAM 使用量為 89/128。

四種組合也各自執行獨立的強制死亡測試，皆得到 120 次大型爆炸呼叫、
59 次音樂 fade、138 次來源 RNG 呼叫、零 effect drop；Game Over 使用
曲目索引 10，按鍵返回後才切至選單曲索引 29。

四種組合另執行獨立 Jukebox 測試，驗證 41 首 source song 全數嵌入、
首尾雙向環回、文字隱藏／恢復、112/128 OAM 星空、fade-out 及回到 title
song 29。詳細結果見 `Tyrian-GBA-Jukebox-v27.md`。

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

### P2：玩家死亡（v26 已完成）

1. 保留 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE` 開發旗標，death auto-test
   會明確設為 0。
2. 已翻寫 60 tick、每 tick 雙大型爆炸、來源 MT19937、隨機 SFX 與
   59 次音樂 fade。
3. effect 邏輯 pool 對齊 PC 的 200 格；GBA presentation 每幀最多顯示
   48 個 effect，來源 allocation drop 為 0。
4. 在凍結的最後 gameplay composition 上疊出 `GAME OVER`，播放真正
   Game Over 曲目索引 10。
5. 按鍵後回到 Game Menu，才切換到 title song 索引 29。

驗收：四組 Detail／Game Speed 的獨立強制死亡 auto-test 全數 PASS。
完整結果見 `Tyrian-GBA-Player-Death-Source-Parity-v26.md`。

### P3：Jukebox（v27 已完成）

1. 已啟用主選單 Jukebox，保留 OpenTyrian 41 首 source index 與
   `musmast.c` 曲名。
2. 以 Mode 0 BG0 tile text、BG1 parallax、palette cycling 與 112 OBJ
   投影星轉接 PC `starlib.c`，切歌不重建 38.4 KiB bitmap。
3. 已支援雙向環回切歌、`Select` 隱藏文字及自然播畢淡出換曲。
4. 已將 41 首 TYM/LDS 轉成 IT/Maxmod；oversized intro pattern 會分段並
   重定位 `Bxx` jump，不刪除音樂事件。
5. 退出 fade 完成後恢復 title song 29 與 Mode 4 選單。

驗收：四組建置的 `TGJ1` auto-test 全數 PASS；最大 OAM 112、text map
commit 5、palette commit 41、最終 module count 41。

### P4：Episode、Next Level 與多關卡（目前階段）

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
