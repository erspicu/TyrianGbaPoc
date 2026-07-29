# Tyrian GBA v40：來源對齊收斂與正式 QA

日期：2026-07-28
分支：`opentyrian-source-parity-port`

## 專案定位

本專案自 v40 起不再以「第一關概念驗證」作為驗收標準，而是採用可持續
擴充的來源對齊移植流程：

1. OpenTyrian 原始碼是 gameplay 與流程規格。
2. LVL、HDT、SHP、PIC、MUS、demo 等資料由 ROMFS 或其完整無損 build
   表示讀取，不建立 per-level 特例表。
3. GBA 差異只放在顯示、輸入、音訊、儲存及效能 adapter。
4. 每個已宣告支援的流程都要有可重跑的 ROM/SRAM 驗收；不能用目視
   「看起來差不多」取代資料與狀態驗證。
5. 尚未整合的功能必須明列，不能以簡化實作冒充已完成的來源流程。

## 本次五項問題的來源對照

### 1. Boss 統計 Cube 顏色與字體

OpenTyrian `src/mainint.c::JE_endLevelAni()` 使用：

- `SMALL_FONT_SHAPES` 顯示統計文字；
- `JE_drawCube(..., 9, 0)` 顯示 Data Cube；
- `JE_drawCube()` 先畫兩層暗色陰影，再以 hue 9、brightness 0 畫前景；
- 不清除最後一張遊戲畫面，統計內容直接疊在持續前進的關卡畫面上。

GBA v40 保留上述流程、座標與 hue 規則。依 240×160 顯示需求及使用者
指定，文字改用比 PC `SMALL_FONT_SHAPES` 再小一級的 `TINY_FONT`，但
仍使用來源 hue 15 的 active/final brightness。Cube 使用獨立 palette
bank 13，文字使用 14/15，避免先前 palette bank 共用造成的黑綠色或
橘色錯誤。

模擬器擷取結果：

- 背景是最後一張持續捲動的關卡畫面，不是一片黑；
- Cube 是來源 hue 9 的藍紫色；
- 最終文字比舊版再小一級，完整落在 240×160 裁切範圍。

### 2. 勝利、死亡與 Secret 曲目只播放一次

OpenTyrian `src/musmast.h` 定義：

- `SONG_LEVELEND = 9`
- `SONG_GAMEOVER = 10`
- `SONG_TITLE = 29`

Secret 提示使用來源曲 30。GBA Maxmod 的 `MM_PLAY_ONCE` 只會在 IT order
list 結束時停止；原轉換檔內的 Bxx position jump 會在抵達結尾前自行
跳回，因此單改播放旗標仍會無限循環。

v40 在 build 階段另外產生 9、10、30 的 finite module：

- 普通 41 首 catalog 保持不變，供關卡與 JukeBox 循環播放；
- finite 版本只停用每首末端的一個 Bxx jump；
- 音符、樂器、pattern、tempo 與聲道資料不變；
- `asset_report.txt` 強制驗證三首各停用且只停用一個 jump；
- runtime 監看 `mmActive()`，自然停止後只增加一次 telemetry，不重新
  啟動曲目。

實測：

- End Level：自然停止 1 次；
- Game Over：先確認真的進入播放狀態，627 個 settled frames 後停止，
  自然停止計數 1；
- Secret：提示結束後恢復目前關卡主題，不反覆啟動提示曲。

### 3. Arcade 隨機裝備掉落可拾取

OpenTyrian `src/mainint.c` 的 player/enemy collision 以 `evalue` 分流：

- `30000`：purple ball；
- `30001..31000`：front weapon，並套用
  `src/varz.c::specialArcadeWeapon[]`；
- `31001..32000`：rear weapon；
- `32001..32100`：sidekick；
- `>32100`：special weapon。

v40 將這五條 one-player Arcade 分支翻入
`ot_level_port_collide_player()`，保留來源 ID、250/100 credit、音效與
物件消耗語意。驗收分成兩層：

1. 固定 fixture 分別碰撞五種代表值，逐欄檢查裝備、金額、音效及物件
   回收。
2. 真正跑完 Arcade Episode 1 section 1，要求來源隨機掉落中至少一個
   高價裝備被實際拾取。

實際路線取得 4 個高價裝備，fixture 五分支全數通過，reward、Sprite2
與 projectile drop 都是 0。

### 4. 標題 Demo 與 30 秒 attract mode

OpenTyrian `src/tyrian2.c` 在 title idle 超過 30000 ms 後設定
`play_demo`；`src/mainint.c::replay_demo_keys()` 讀取錄製的按鍵串流。

v40：

- 首頁選項為 `Start New Game`、`Demo`、`JukeBox`；
- 30 秒以 60 Hz VBlank 計為 1800 frames；
- `Demo` 手動項目或 idle timeout 都循環讀取 stock `demo.1` 到
  `demo.5`；
- 直接解析原始 demo header，包括 episode、LVL、武器、power、
  sidekick、generator、shield、special、ship 與 song；
- generator、ship、shield 定義從目前 episode 的 HDT 讀取，Episode 4
  會走其 LVL 內嵌 item database；
- ordinary ship 從來源 `spriteSheet9`/Sprite2 bank 38 顯示；
- gameplay 顯示來源 `INSERT COIN` 小字，任何實體輸入可離開 Demo 並
  恢復 title song 29。

自動測試逐一實際載入 demo 1 到 demo 5，逐欄驗證五份 header、目前
episode HDT 套用後的機體／裝甲／護盾、五段非零輸入串流、abort 返回、
title 音樂及 44-module Maxmod catalog。模擬器擷取亦確認
`INSERT COIN` 與來源機體可見。

### 5. 主角 Sprite2 不得造成敵人消失

第一次完整跑 Episode 1 四關 campaign 時，第三關偵測到一次
`AUTOTEST_CAMPAIGN_FAIL_SPRITE_CACHE`。原因是主角機體改為來源 Sprite2
後，曾與 24-slot enemy L1 共用；該關某一幀正好需要完整 24 個敵人
frame，主角多佔一格便使一個來源物件沒有 VRAM slot。

修正不是放寬驗收，也不是增加丟幀：

- OBJ tiles 0..31 原本就是舊 32×32 主角 atlas；
- v40 將這 1 KiB 改為主角專屬的 source Sprite2 VRAM cache；
- enemy L1 仍保有完整 24 slots；
- 主角 banking frame 仍由共享的 64-slot、palette-aware EWRAM L2
  取得；
- VBlank 先上傳主角，再上傳 enemy/projectile/effect，最後提交 OAM。

重跑四關後 campaign failure 由 1 降為 0，所有關卡 Sprite2 cache drop
皆為 0。

## 自動驗收結果

組態：High Detail、Normal Game Speed、mGBA 0.11.0。

| 驗收 ROM | 結果 | 主要條件 |
|---|---:|---|
| TGBA schema 25 | PASS | 第一章第一關、Boss、離場、統計、Game Menu |
| TGD2 | PASS | 正常死亡、死亡曲曾播放且自然停止、返回 Game Menu |
| TGJ1 | PASS | 41 首來源曲循環切換、44 個 Maxmod modules |
| TGDM | PASS | 30 秒 idle、5/5 demo header/input、返回 title |
| TGLM schema 2 | PASS | 62/62 LVL sections、24/24 routes、8,063 sprites |
| TGCM schema 3 | PASS | Episode 1 連續四關，0 failure |
| TGRS Episode 2 | PASS | 0 unknown/decode/cache drop，勝利曲停止 |
| TGRS Episode 3 | PASS | 0 unknown/decode/cache drop，勝利曲停止 |
| TGRS Episode 4 | PASS | 內嵌 HDT，0 unknown/decode/cache drop，勝利曲停止 |
| TGRS Arcade | PASS | 實際高價拾取 4，五分支 fixture PASS |

效能與容量：

- release ROM：14,568,412 bytes，佔 32 MiB 的 43.42%；
- Episode 1 完整路線：12,168 display frames、10 missed VBlanks；
- Episode 1 Boss 視窗：439 frames、3 missed VBlanks；
- Episode 2 完整路線：10,475 frames、24 missed VBlanks（0.23%）；
- Sprite2、projectile、reward、ROMFS stream drop：全部 0；
- release EWRAM safety margin：49,628 bytes；
- release IWRAM safety margin：8,760 bytes。

## 發行與後續功能邊界

v40 對上述已整合流程採正式回歸門檻，但不以此宣稱整個 PC 遊戲已全部
完成。目前仍需逐步接通的主要區塊包括完整 shop/upgrade/save 流程、
所有章節的連續 campaign 規則、雙人／network 與 Destruct。這些項目
應沿用同一套來源翻寫及 ROMFS 方法，完成前不得以簡化等價實作標示成
來源對齊。

正式建置命令：

```powershell
.\build.ps1 -DetailLevel high -GameSpeed normal
```

成功時腳本會驗證 ROM header、32 MiB 上限、EWRAM/IWRAM safety margin、
所有 mGBA/SRAM invariant，再把測試與歷史 ROM 移入 `Backup`，使
`build` 只保留最新 release ROM。
