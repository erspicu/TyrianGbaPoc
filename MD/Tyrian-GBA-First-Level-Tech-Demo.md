# Tyrian GBA 第一關技術展示版

更新日期：2026-07-25

## 階段成果

目前已完成一個可在 GBA/mGBA 執行的第一關概念驗證。流程為：

```text
Tyrian 開場畫面
    ↓ Start
直接進入第一關
    ↓ 完整事件路線
簡化版第一關 Boss
    ↓ 擊破
回到開場畫面
```

沒有關卡選單、武器選單或右側武器資訊欄。主角在技術展示階段沒有死亡流程，
但保留移動、射擊、敵彈、命中、爆炸、敵機碰撞與 Boss 碰撞等 game loop 要素。

正式 ROM：

```text
repo/TyrianGbaPoc/build/tyrian_gba_level1_tech_demo_v9.gba
```

SHA-256：

```text
e0f895b050d61bf944a975d34c40ef29402ea2346d8955e463e917d544a7627b
```

ROM 大小為 650,196 bytes（634.96 KiB，約 0.620 MiB），只使用標準
32 MiB GBA ROM 視窗的 1.9377%。本次容量完全不是限制。

## 原始資料來源與轉換

轉換器直接讀取工作區內的原始 Tyrian 資源，不是從 NES 或 SNES ROM
反向複製：

- `org/AprCSTyrian/Build/data/tyrian1.lvl`
- `org/AprCSTyrian/Build/data/tyrian.hdt`
- `org/AprCSTyrian/Build/data/tyrian.snd`
- `org/AprCSTyrian/image`
- `org/TyrianAudioLab/Music/30_tyrian_the_song.tym`
- `org/TyrianAudioLab/Music/18_tyrian_the_level.tym`

可重現的轉換入口是：

```text
repo/TyrianGbaPoc/tools/build_assets.py
```

它沿用已驗證的 Tyrian 解析邏輯，重新輸出 GBA 原生的 packed-nibble
4bpp tile、GBA tilemap attribute、OBJ 1D atlas、BGR555 palette、事件 bytecode
及 Maxmod soundbank。

目前資源統計：

| 資源 | 大小 |
|---|---:|
| Mode 3 開場圖 | 76,800 bytes |
| 三組背景 tile | 49,152 bytes |
| 三層完整關卡 map | 324,800 bytes |
| 背景與 OBJ palette | 1,024 bytes |
| OBJ atlas | 32,640 bytes |
| 關卡事件 bytecode | 6,415 bytes |
| 兩首音樂及七組音效 soundbank | 122,660 bytes |
| 主要資源合計 | 613,491 bytes |

## GBA 畫面架構

遊戲中使用 Mode 0：

| GBA 元件 | Tyrian 用途 | 設定 |
|---|---|---|
| BG0 | MAP1 地形底層 | 4bpp、char block 0、screen block 24、priority 3 |
| BG1 | MAP2 中景透明層 | 4bpp、char block 1、screen block 26、priority 2 |
| BG2 | MAP3 前景透明層 | 4bpp、char block 2、screen block 28、priority 1/0 |
| OBJ | 主角、敵機、子彈、爆炸、獎賞、數字、Boss、血條 | 4bpp、1D mapping、priority 0/1 |
| Mode 3 | 開場畫面 | 240×160、15-bit bitmap |

GBA 硬體實際有 128 筆 OAM；每一筆 OAM 本身就是一個 Sprite 描述，不是
OAM 與 Sprite 各有一套獨立容量。v6 已取消先前的 64 筆軟體限制，可使用
完整 128 筆硬體 OAM。

三層 map 都完整保留在 ROM；VRAM 只維持每層 64 列的環狀視窗，跨過
tile row 時才在 VBlank 期間 DMA 新的一列。這避免在遊戲中大量搬動 map。

v6 已超越先前 Low Detail 的雙層內容邊界，補齊 MAP2，使用 MAP1、MAP2、
MAP3 三個真正獨立的硬體背景。三層各保留 512 個 tile，共使用 15 組
16 色 palette。MAP2 的 257 個來源 unique tile 全數容納；MAP1 與 MAP3
超過各自 tile bank 的部分會比對到最接近的現有 tile。

MAP3 在原關卡 event 21 前位於主要物件後方，事件發生後切換為真正前景；
程式會同步調整 BG2 與 OBJ priority，保留原作遮擋關係。

OBJ atlas 包含：

- 主角機的中立與側傾姿態
- 24 種經稽核的敵機視覺 archetype
- 64×64 Boss
- 主角彈、八張 PC 原始敵彈圖、爆炸、三種獎賞、金額數字與 Boss 血條

69 個原始敵人 ID 都能對應，未知 ID 與錯誤 sprite bank 都是 0；其中
290 次生成可使用精確來源圖像，其餘 ID 使用相近 archetype。這仍是目前
相對 PC 原版最明顯的簡化之一。

### Pulse-Cannon 主角彈修正

自 v4 起不再使用程序生成的白色菱形佔位圖。轉換器會直接解析
`tyrian.hdt`：

```text
新遊戲前方武器 Port 1 / power 1
    → Pulse-Cannon
    → weapon record 155
    → sprite sheet 08_player_shots / graphic 59
```

原始圖是 12×11 的雙橘紅脈衝彈，單張動畫；`shotrepeat=3`、垂直速度
`sy=10`。GBA 端的出生點、每次 logic update 的移動量及命中框也依該圖
的非透明範圍一併校正，不只是單純替換 atlas 圖片。轉換結果會把上述資料
寫入 `res/asset_report.txt`，若來源格式或預期記錄不一致就停止建置。

### 敵機與 Boss 子彈同步 PC 版

v8 以前的敵彈不是 PC 資料：所有敵人共用一張程式繪製的 16×16 橘色圓球，
速度固定為 `(0,3)`；是否射擊則由 24 筆手寫週期決定。Boss 也是自行設計的
三向大球，低血量時加快，與第一關原始 Boss 無關。

v9 改為直接解析 `tyrian.hdt`，並對照 OpenTyrian `JE_drawEnemy()`：

- 每一筆 spawn 都保留原敵人的 `tur[3]`、`freq[3]`，不再依 visual
  archetype 猜武器。
- 12 筆 event type 31 保留三個獨立頻率；舊版取三欄最大值後寫進單一週期，
  會讓錯誤砲位開火。
- 保留 `WeaponType` 的 `multi/max`、`bx/by`、`sx/sy`、`aim`、`weapani`
  與砲位 1/2/3 的旋轉規則。
- 這一關敵機會引用 weapon records
  `2,3,4,59,62,78,115,116,125,126`；所用的 acceleration、`tx/ty`
  都是 0，而且每個位置的 `del` 都是 255，因此目前的精簡 runtime
  沒有遺漏這批武器的曲線加速或個別壽命。
- PC 的填入砲位初始等待 20 tick、各自重裝頻率、3→1 砲位處理順序及
  Normal 難度 Chebyshev 瞄準量都已對應。

敵彈圖不再重畫。轉換器從 `08_player_shots` 直接打包 graphics
58、112–113、145–147、201–202；紅色子彈保留兩幀動畫。紅彈、橘色飛鏢、
紫色雷射分用三個 OBJ palette bank，避免把 27 種來源色階硬壓進同一個
15 色 bank。透明邊界經裁切後用 GBA 8×8、8×16 或 16×16 OBJ 儲存，
繪製時再套回來源左上錨點，所以 18 個 tile 就能保留原輪廓，不需縮圖。
完整 OBJ atlas 現為 1,020 / 1,024 tiles。

Boss 身體與移動仍是 64×64 POC 簡化版，但射擊已換成原始 enemy records
52／54 的雙側 weapon 59 追蹤彈（每 10 tick），以及 record 53 的
weapon 127 五向扇形彈（每 60 tick）；三個砲口由原 120×112 component
grid 等比例映射到目前 Boss 圖。敵彈 pool 也由 24 提升為 OpenTyrian
相同的 60 發。weapon sound 4、6、13 已從 `tyrian.snd` 加入 soundbank。

![PC 敵彈來源圖](../build/preview/enemy_projectiles_pc_source.png)

![一般敵機 PC 追蹤彈](../build/enemy_projectiles_pc_sync.png)

![Boss 雙追蹤與五向扇形彈](../build/boss_projectiles_pc_sync.png)

### 主角飛機持續閃爍修正

舊程式將 `09_player_ships` 的 graphic 233 與 235 誤當成兩張循環動畫，
每個 logic update 執行一次切換。它們其實分別是 USP Talon 的中立與
右傾姿態，因此主角即使沒有移動，也會以約 34.78 Hz 改變輪廓，形成持續
閃爍。

v5 改依水平操控選擇姿態：

- 沒有水平輸入時固定使用中立 graphic 233。
- 向右時固定使用右傾 graphic 235。
- 向左時使用 graphic 235 的 GBA OBJ 水平翻轉，形成穩定左傾。
- 技術展示的無敵碰撞狀態不會隱藏或閃爍主角 OAM。

另以專用無移動測試 ROM 在相鄰的 level tick 20 與 21 各擷取一次
framebuffer。
背景在兩次擷取間繼續捲動，但主角 Sprite 的 266 個非透明像素比對結果為
0 個差異：

![主角穩定性 tick 20](../build/player_stability_tick20_v5.png)

![主角穩定性 tick 21](../build/player_stability_tick21_v5.png)

### 敵機爆炸動畫修正

舊版只把 `newsh_6/010.png` 的最大火球幀放進 OBJ atlas，所有敵機死亡後
都固定顯示同一張圖 15–32 個 logic tick；既沒有動畫，也沒有跟隨原版
背景速度向下漂移。

v6 依 OpenTyrian 的 `JE_setupExplosion()` 與 `JE_setupExplosionLarge()`
還原 Low Detail 使用的原始不透明動畫：

- 一般小型敵機：type 1，graphics 122–133，共 12 幀。
- 大型空中敵機：四個象限各 12 幀，共 48 幀。
- 大型地面敵機：另一套四象限各 12 幀，共 48 幀。
- 24 種代表 archetype 中，23 種依 HDT `esize=1` 使用四象限爆炸；
  archetype 1 使用單一小型爆炸。
- HDT `explosiontype` 的奇偶值用來選擇空中或地面版本。
- 每個 logic tick 推進一幀並依 MAP2/MAP1 速度向下漂移；一輪約
  `12 / 34.7826 = 0.345` 秒。

v7 另外修正四象限中間的十字狀裂縫。Tyrian 的每一塊原圖固定寬
12 px，上半部座標高度為 14 px；`JE_setupExplosionLarge()` 正好使用
`x - 6 / x + 6` 與 `y - 14 / y`，因此原生畫布本來就會在 12×14 的
邊界無縫相接。舊轉換器先裁掉各張圖的透明邊界，再各自置中到 16×16，
使四塊圖的錨點不一致。現在不再 crop/resize，而是保留原圖左上座標，
放進透明 16×16 OBJ 畫布；OAM 仍沿用原版 12×14 間距。

總計 9 組 × 12 幀，108 個編碼幀全都非空且互不相同。爆炸使用 432 個
OBJ tile；加入獎賞、數字與 PC 敵彈後的完整 atlas 為 1,020 / 1,024
tiles（99.61%），仍符合 Mode 0
的 32 KiB OBJ VRAM 上限。效果 pool 由 12 提升到 48；完整壓力測試的
峰值為 40 個同時作用的效果，爆炸象限丟棄數為 0。

轉換器輸出的 12 幀空中／地面四象限組合預覽：

![四象限原生錨點組合](../build/preview/explosion_large_composite_air_ground.png)

專用測試 ROM 在畫面中央生成四象限空中爆炸後，由 mGBA 擷取的實際
framebuffer；中央沒有額外水平或垂直間隔：

![中央無縫爆炸](../build/explosion_seam_center_v7.png)

### 敵機獎賞掉落

OpenTyrian 的正常擊破流程會把敵人的正 `evalue` 直接加入金額；只有
HDT `eenemydie` 指向「armor=0 且 value != 0」的另一筆敵人物件時，
才會在死亡位置建立可拾取的實體獎賞。第一關 414 筆生成記錄中沒有任何
這類 `eenemydie`，因此 v7 依技術展示需求，把原始 HDT `value >= 50`
的高價值敵人改成實體掉落，沒有對所有敵人套用隨機機率：

| 掉落金額 | 第一關符合記錄 | 原始圖像 |
|---:|---:|---|
| 50 | 11 | HDT 392，`11_coins_cubes` graphics 26–31 |
| 100 | 2 | HDT 394，graphics 32–36 的往返動畫 |
| 1,000 | 6 | HDT 397，六組 2×2 圖塊組合後縮入 16×16 OBJ |

共 19 筆事件帶有 reward byte。獎賞使用 16-entry pool、以原版
`ymove=1` 下落、每兩個 logic tick 推進動畫；與主角碰撞後累加金額、
播放原始 `S_ITEM`。

v8 移除先前為提高辨識度而自行添加的 1 px 淡色輪廓；50、100 與
1,000 獎賞現在保留 `11_coins_cubes` 原圖的透明邊緣與色彩，不再出現
原版沒有的特殊色環。

累計金額也改回 PC 原版 `JE_inGameDisplays()` 的表現方式：

- 不顯示先前自行加入的右上角寶石圖示。
- 不使用自製 5×7 黃色數字，也不再限制為五位數。
- 直接使用 TINY_FONT sprite 79（0）及 70–78（1–9）。
- 沿用原版 hue 2、brightness 4、四方向黑色 `FULL_SHADE` 與可變字寬。
- PC 的 `(30,175)`／320×200 座標按比例映射到 GBA 的 `(22,140)`／
  240×160 左下角。

![50／100／1000 原始獎賞動畫](../build/preview/reward_coins_50_100_1000.png)

![原版 TINY_FONT 金額數字](../build/preview/cash_tiny_font_digits.png)

專用測試 ROM 將累計金額設為 12,345 並生成 50 金幣後，由 mGBA 擷取的
實際 framebuffer：

![v8 原版金額與無外圈金幣](../build/reward_cash_pc_style_v8.png)

## 第一關事件與 game loop

解析到的第一關來源 route 有 1,009 筆記錄，轉為：

- 414 個敵機生成命令
- 347 個移動、加速、反轉、射擊或前景控制命令
- 時間差與結束標記

同時敵機池由初版的 16 架提升為 24 架。完整壓力測試最高同時存在
22 架敵機，沒有再碰到 pool 上限。其餘 pool 為 12 發主角彈、
60 發敵彈、48 個效果與 16 個可拾取獎賞。

目前敵人的連結控制、速度、加速度、PC 三砲位射擊、出界回收、主角彈命中、
敵彈命中、敵機碰撞與 Boss 碰撞都會執行。個別敵人的複雜 PC AI 與逐張
動畫仍是代表性簡化；Boss 子彈腳本已同步，Boss 身體移動與受擊流程仍是
POC 簡化行為。

## 速度設定

GBA 顯示更新率為約 59.72750057 Hz。遊戲邏輯使用 fixed-step accumulator，
目標是原版 Normal game speed：

```text
1,193,182 / (0x4300 × 2) = 34.78259095 logic updates/s
```

排程分母已依 GBA 實際顯示率校正為 2,048,892，而不是把顯示率近似成
60 Hz。畫面仍每個 VBlank 更新；敵機、碰撞與地圖事件以約 34.78 Hz
推進，沒有沿用 NES 版的 30 Hz 或 15 Hz 限制。

三個背景也不再使用固定近似速度；轉換器保留第一關的五個原始背景控制
事件：

| Logic tick | MAP1 | MAP2 | MAP3 | 說明 |
|---:|---:|---:|---:|---|
| 0 | 1 | 2 | 0 | 原作開場段 |
| 1,000 | 1 | 2 | 3 | 標準 1:2:3 parallax |
| 2,400 | 1/3 | 1/2 | 1 | 原作慢速段 |
| 3,420 | 1 | 2 | 3 | 恢復標準速度 |
| 4,020 | 2 | 4 | 6 | 關底加速段 |

數值單位是每個 logic update 的像素；慢速段由原作的 delay counter 重現。

## 音樂與音效

音樂流程為：

```text
Tyrian TYM
    → 保留 tracker event 的 IT module
    → mmutil soundbank
    → GBA Maxmod 16-channel mixer
    → Direct Sound stereo
```

目前包含：

- 完整開場曲 `30_tyrian_the_song.tym`
- 完整第一關曲 `18_tyrian_the_level.tym`
- 主角武器、敵人命中、爆炸、拾取 `S_ITEM`，以及敵方 weapon sound
  4／6／13，共七組原始 PCM 音效

開場 IT 為 89,792 bytes、約 113.51 秒；第一關 IT 為 31,822 bytes，
單次音樂 pass 約 77.20 秒，排好的 module sequence 約 308.66 秒。
不是先轉成單一長 PCM，因此 ROM 容量很小，且各 tracker channel 仍可
由 Maxmod 獨立混音。

Maxmod 使用約 15.768 kHz 的 GBA stereo mixer 模式、16 個 mixer channel，
module 與 effects volume 都設為 896/1024。

開發中發現一個重要的初始化順序問題：若先呼叫 `mmInitDefault()` 才掛
VBlank IRQ，Maxmod 第一個 audio frame 的 double-buffer write pointer
尚未初始化，會往位址 0 寫入。現在順序改為先安裝並開啟 VBlank IRQ，
再初始化 Maxmod。mGBA 的非法記憶體寫入由 264 次降為 0。

## mGBA 完整流程驗證

除正式 ROM 外，建置流程會產生獨立 auto-test ROM。它會自動進入第一關、
移動與射擊、走完整事件、進 Boss、擊破 Boss、等待 clear，再回開場畫面。
結果寫進 SRAM 後以 SWI 3 結束。

最終遙測：

| 項目 | 結果 |
|---|---:|
| Auto-test | PASS |
| Logic updates | 5,742 |
| Display frames | 9,859 |
| VBlank IRQ | 9,861 |
| Missed VBlank | 0 |
| Spawn events | 414 |
| Control events | 347 |
| Collision events | 390 |
| Streamed map rows | 4,533 |
| Map stream drops | 0 |
| Peak active enemies | 22 / 24 |
| Peak OAM/Sprite | 63 / 128（完整硬體上限） |
| Peak active effects | 40 / 48 |
| Dropped explosion components | 0 |
| Spawned reward items | 3 |
| Picked-up reward items | 3 |
| Peak active rewards | 1 / 16 |
| Dropped reward items | 0 |
| Final cash | 150 |
| Spawned enemy projectiles | 354 |
| Peak active enemy projectiles | 21 / 60 |
| Dropped enemy projectiles | 0 |
| State transitions | 5 |
| mGBA memory/runtime errors | 0 |

5,742 個 logic tick 約等於 165 秒的 GBA 遊戲時間。Headless mGBA 在 PC
以不限速模式約 3.0 秒跑完；這個 host 時間不是 GBA 效能數字。

實際 framebuffer 截圖：

![v4 原版 Pulse-Cannon 主角彈](../build/player_shot_capture_v4.png)

![第一關中段三層畫面](../build/first_level_capture_v3.png)

![前景切換後三層畫面](../build/first_level_foreground_capture_v3.png)

![Boss 三層畫面](../build/boss_capture_v3.png)

為了取得 SRAM 遙測與 framebuffer，工作區內的 mGBA headless runner
做了兩項小幅測試用途修改：

- 像 SDL/Qt front end 一樣自動掛載相鄰 `.sav`
- 增加 `-O PNG`，在退出時保存最後一個 framebuffer

修改位置：

```text
org/mgba/src/platform/headless-main.c
```

## 操作方式

- `Start`：從開場進第一關
- 方向鍵：移動
- `A` 或 `B`：射擊
- `Select` 或 `L`：開發用的直接進 Boss
- `R`：開發用的立即擊破 Boss

主角在本階段沒有死亡或重生流程，方便連續觀察整關。

## 重建

PowerShell：

```powershell
cd C:\ai_project\AprTyrianNes\repo\TyrianGbaPoc
.\build.ps1
```

腳本會：

1. 必要時重新解析原始 Tyrian 資料並建立 GBA 資源。
2. 建立 Maxmod soundbank。
3. 編譯正式 ROM 與 auto-test ROM。
4. 驗證 GBA header、容量與 SHA-256。
5. 用 mGBA 跑完整第一關並解析 SRAM invariant。
6. 用 `mgba-perf` 跑正式 ROM 的 600-frame 開場 smoke test。
7. 輸出 `build/verification.txt`。

若 mGBA GUI 正載入要覆寫的同名 ROM，Windows 會鎖住該檔案；重建前請先
關閉該 ROM 或關閉模擬器。正式交付使用 `_v9.gba` 檔名，避免與先前測試
ROM 混淆。

## 現階段結論

以這個三背景圖層、單關流程的範圍來看，GBA 硬體還沒有到極限：

- 完整 route 在 24 架敵機 pool 下仍為 0 missed VBlank。
- 三層共串流 4,533 列，仍為 0 stream drop。
- 峰值只使用 128 筆硬體 OAM 中的 63 筆。
- 354 發 PC 規則敵彈的峰值為 21 / 60，沒有 pool drop。
- ROM 只佔 32 MiB 空間約 1.94%。
- 三層背景、完整 tracker 音樂及基本碰撞可同時運作。

目前限制主要是移植工時與內容對應精度，而不是 GBA CPU 或 ROM 容量。
若繼續提高完成度，優先順序應是增加敵機逐張動畫與 AI family、還原更準確
的 Boss 身體移動／受擊 script、加入 HUD/生命流程，再評估把 Maxmod mixer 提高到約
31 kHz；不是先縮減關卡或音樂。
