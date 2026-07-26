# Tyrian GBA 第一關技術展示版

更新日期：2026-07-26

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

目前 source-parity／ROMFS v19 player-bounds 正式 ROM：

```text
repo/TyrianGbaPoc/build/tyrian_gba_level1_source_parity_crop1to1_playerbounds_romfs_v19.gba
```

SHA-256：

```text
04b664d9fb48b0a73363d9b8b0b1f0fc9028fb07ae39ef7632d1de9d284839b5
```

ROM 大小為 10,883,584 bytes（約 10.38 MiB），其中 9,853,080 bytes 是
68 個 stock Tyrian runtime 檔案的唯讀 ROMFS；使用標準 32 MiB GBA ROM
視窗的約 32.44%。

## 原始資料來源與轉換

轉換器直接讀取工作區內的原始 Tyrian 資源，不是從 NES 或 SNES ROM
反向複製：

- `org/AprCSTyrian/Build/data/tyrian1.lvl`
- `org/AprCSTyrian/Build/data/tyrian.hdt`
- `org/AprCSTyrian/Build/data/tyrian.snd`
- `org/AprCSTyrian/Build/data/music.mus`
- `org/AprCSTyrian/Build/data/tyrian.pic`
- `org/AprCSTyrian/Build/data/tyrian.shp`
- `org/AprCSTyrian/Build/data/palette.dat`
- `org/AprCSTyrian/image`
- `org/TyrianAudioLab/Music/30_tyrian_the_song.tym`
- `org/TyrianAudioLab/Music/18_tyrian_the_level.tym`

可重現的轉換入口是：

```text
repo/TyrianGbaPoc/tools/build_assets.py
```

v19 的 LVL/HDT/PIC/SHP/MUS loader 由 `src/opentyrian_data.c` 直接讀
ROMFS raw bytes。`tools/build_assets.py` 仍負責尚未替換的 GBA
packed-nibble 4bpp tile cache、OBJ atlas 與 Maxmod waveform cache。舊
10,273-byte event bytecode 只在 host 計算資源稽核，不再輸出或連入 ROM。

目前資源統計：

| 資源 | 大小 |
|---|---:|
| Mode 3 開場圖 | 0-byte blob；由 PIC/SHP runtime 解碼 |
| 三組背景 tile | 49,152 bytes |
| 三層完整關卡 map | 658,176 bytes；64-column 1:1 PC raster |
| 背景與 OBJ palette | 1,024 bytes |
| 靜態 OBJ atlas／VRAM image | 32,768 bytes |
| 198-frame enemy/reward catalog＋tiles | 102,976 bytes |
| Runtime 關卡事件 blob | 0 bytes；直接讀 ROMFS `tyrian1.lvl` |
| 兩首音樂及七組音效 soundbank | 122,660 bytes |
| Stock Tyrian ROMFS | 9,853,080 bytes |

## GBA 畫面架構

遊戲中使用 Mode 0：

| GBA 元件 | Tyrian 用途 | 設定 |
|---|---|---|
| BG0 | MAP1 地形底層 | 4bpp、char block 0、screen block 24+25、priority 3 |
| BG1 | MAP2 中景透明層 | 4bpp、char block 1、screen block 26+27、priority 2 |
| BG2 | MAP3 前景透明層 | 4bpp、char block 2、screen block 28+29、priority 1/0 |
| OBJ | 主角、敵機、子彈、爆炸、獎賞、數字、Boss、血條 | 4bpp、1D mapping、priority 0/1/2 |
| Mode 3 | 開場畫面 | 240×160、15-bit bitmap |

GBA 硬體實際有 128 筆 OAM；每一筆 OAM 本身就是一個 Sprite 描述，不是
OAM 與 Sprite 各有一套獨立容量。v6 已取消先前的 64 筆軟體限制，可使用
完整 128 筆硬體 OAM。

v18 不再把 enemy ID 對到 24 個自訂 archetype。第一關 113 個 transitive
definition 共 198 個原始 Sprite2 frame，以
`shape_table/egr[enemycycle-1]/size` 查表；24-slot true-LRU cache 在
VBlank 將需要的 32×32 4bpp frame 搬入 OBJ VRAM。金幣、寶石與 data cube
也使用同一套 source enemy draw command，不另套 GBA reward 外框。
32×32 只是 OAM container；12×14／24×28 PC source cell 使用固定
top-left anchor，不逐幀裁透明 bbox 後置中，因此動畫不會在相同
`ex/ey` 上產生 presentation jitter。

三層 map 都完整保留在 ROM；每層完整寬度為 64 tile／512 px，VRAM
用兩個相鄰 screen block 維持 32 tile-row 的 512×256 環狀視窗。跨過
tile row 時才在 VBlank 期間把該列的左右各 32 tile DMA 到兩個 block。
這保留 PC 背景的 1:1 像素寬度，也避免遊戲中大量搬動 map。

v6 已超越先前 Low Detail 的雙層內容邊界，補齊 MAP2，使用 MAP1、MAP2、
MAP3 三個真正獨立的硬體背景。三層各保留 512 個 tile，共使用 15 組
16 色 palette。MAP2 的 257 個來源 unique tile 全數容納；MAP1 與 MAP3
超過各自 tile bank 的部分會比對到最接近的現有 tile。

MAP3 在原關卡 event 21 前位於主要物件後方，事件發生後切換為真正前景；
程式會同步調整 BG2 與 OBJ priority，保留原作遮擋關係。

OBJ atlas 包含：

- 主角機的中立與側傾姿態
- 64×64 Boss
- 主角彈、八張 PC 原始敵彈圖、爆炸、legacy 視覺測試獎賞、金額數字、
  PAUSED 字樣與 Boss 血條
- tile 640..1023 的 24 個動態 32×32 enemy/reward frame slots

Host 稽核現在掃描全部 1,009 event 的 spawn、launch 與 death closure，
建立 198 個精確 `(shape_table, graphic, size)` 畫格。Runtime 直接使用
`JE_drawEnemy()` 當幀的 `egr[enemycycle-1]` 查表，4,912..5,384 的大型
地景／Boss component 也已納入資源，不再有 fallback ID。原始 Boss
lifecycle 尚未接管，是另一個獨立邊界。

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

### v10 固定地景、小戰車與 Boss 血條同步

本次重新逐段對照 OpenTyrian `JE_createNewEventEnemy()`、
`JE_drawEnemy()` 與第一關 event route，找到舊版共用 NES 事件格式會遺失：

- PC 初始 Y 與 `eventdat5`
- `tyrian.hdt` 的 `xmove`／`ymove`
- sky、ground、top、ground2 所屬 enemy pool
- ground／top 每次實際背景捲動量
- 原始 armor

舊 runtime 因此把所有敵物件放在 `y=-8` 或 `-30`，再套用手寫的基礎
`dy=2`；在 MAP1 慢速段也仍以 wall-clock tick 觸發事件。結果就是地景元件
比對應地圖提早出現、離開背景，而多片物件彼此錯位。

v10 的 414 筆 spawn 現在都保存上述 PC 欄位。事件時間也不再直接比較
`level_tick`，改用原作 `curLoc` 語意：MAP1 本次真正前進幾 pixel，
event position 才增加幾。這使慢速與關底加速段的敵人、地景和背景保持
同一條世界座標時間軸。

第一關九筆 event type 12 各展開為四個可破壞地景元件：

```text
左下 (x, y=-28)       右下 (x+24, y=-28)
左上 (x, y=-56)       右上 (x+24, y=-56)
```

每片使用來源 armor 10、`dx=dy=0`，只加所屬 MAP1 的「實際」捲動 step；
因此它們固定在地形上，射擊破壞 OBJ 後自然露出原本 BG tile，不會留下
自行繪製的替代底圖。

第一批小戰車則不是一張圖，而是上排 HDT 6/7 或 13/14，加上下排 8/9
組成的四片結構。PC route 在 `curLoc` 870、920、970 與 1,000 生成它們。
v10 保留 24px 左右間距、上排 `y=-44`、下排 `y=-28`，以及 event 的
`yspeed=3` 或 `1`；兩個 ground pool 都再加 MAP1 step，所以控制事件將
`eyc=-1` 時可像 PC 一樣與 1px 地圖捲動互相抵銷，而不是向上飄走。

Boss event 79 在 `curLoc=5400` 指向 link 142。該組 PC 元件最低初始
armor 為 254，因此 v10 Boss 不再由手寫 96 HP 開始。血條也不再是八個
16×16 方塊，而是依 `draw_boss_bar()`：

- PC 單血條：中心 `x=155`，背景 `x=130..180`、`y=7..12`
- GBA 240×160 對應：`x=96..135`、`y=6..11`
- 填色寬度依目前 armor 從中心向兩側縮短
- 命中時沿用 PC palette 117–125 的六階退色閃光

血條只重用原本保留的四個 OBJ tile，整體 atlas 仍是
1,020 / 1,024 tiles。

![PC 敵彈來源圖](../build/preview/enemy_projectiles_pc_source.png)

![一般敵機 PC 追蹤彈](../build/enemy_projectiles_pc_sync.png)

![Boss 雙追蹤與五向扇形彈](../build/boss_projectiles_pc_sync.png)

### v11 擊破金額、動態獎賞與暫停同步

舊版只有轉換器自行挑出的 19 筆 `HDT value >= 50` 敵人會生成實體
50／100／1,000 獎賞；這不是 PC 的規則，而且漏掉兩條原始流程：

1. OpenTyrian 擊破敵人時，若該敵人的 `0 < evalue < 10000`，會立即把
   `evalue` 加到左下金額，不要求生成或拾取物件。
2. 第一關有 33 筆 event type 33，會依 `linknum` 動態改寫仍在場敵人的
   `enemydie`。死亡時再建立該 HDT 物件；armor=0 的 391–395 正是
   25／50／75／100／250 分的可拾取金幣或寶石。

v11 在每筆 spawn bytecode 加入原始 Normal 難度 `evalue`，並新增
`EVENT_REWARD` opcode。33 筆 type 33 全部保留執行順序；26 筆對應金額
獎賞，另外七筆資料方塊、武器或特殊模式物件目前只保留「清除金額掉落」
語意，尚未把非金額道具系統加入這個 POC。轉換結果為：

| 項目 | 數量 |
|---|---:|
| 第一關 spawn | 414 |
| 帶直接擊破金額的 spawn | 367 |
| 這 367 筆原始配置值合計 | 11,356 |
| type 33 動態 `enemydie` 控制 | 33 |
| 其中金額獎賞控制 | 26 |
| 完整 auto-test 實際取得掉落設定的敵人 | 44 |

實體獎賞不再使用價值區間猜測，而是精確比對 HDT 391–395。為容納五種
原圖，六幀與十六幀 PC 動畫各抽取三個關鍵幀；25／50／75 每兩個 logic
tick 換幀，100／250 每五個 tick 換幀，維持接近各自原始循環長度。
獎賞 pool 由 16 提高為 32。完整測試實際擊破直接結算 396，拾取一枚
25 分獎賞後最後為 421，沒有 reward pool drop。

![25／50／75／100／250 原始獎賞](../build/preview/reward_coins_25_50_75_100_250.png)

![v11 實機事件掉落](../build/reward_capture_v11.png)

遊戲中也加入 `Start` 暫停／再按一次繼續。行為依
OpenTyrian `JE_pauseGame()`：

- 關卡時鐘、事件、背景、敵我物件、碰撞與效果完全停止。
- tracker 不停止，module volume 從 896 降到 448；繼續時恢復 896。
- 使用 PC `JE_dString(..., FONT_SHAPES)` 的 P/A/U/S/E/D sprites
  15、0、20、18、4、3。
- 前景依 hue 15、brightness -3 產生淡橙至黃白漸層，帶右下暗影；
  PC `game_screen (120,90)` 經相同 1:1 crop 顯示在 GBA `(84,78)`。

自動測試在遊戲中暫停 60 個 display frame；前後 logic tick 沒有趕幀，
暫停／繼續 toggle 均為一次。以下是 mGBA 真正 framebuffer，不是資源
預覽：

![v11 PC 字型與色盤 PAUSED](../build/pause_capture_v11.png)

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
- 每個 source enemy 直接依自己的 HDT `esize` 選大型四象限或普通小型
  爆炸，不再從 visual archetype 推測。
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
OBJ tile；靜態內容使用 636 tiles，後方對齊後保留 24×16 tiles 給
exact-frame cache，完整 VRAM image 為 1,024 / 1,024 tiles，仍符合
Mode 0 的 32 KiB OBJ VRAM 上限。效果 pool 為 48；v19 固定路線峰值為
31 個同時作用的效果，爆炸象限丟棄數為 0。

轉換器輸出的 12 幀空中／地面四象限組合預覽：

![四象限原生錨點組合](../build/preview/explosion_large_composite_air_ground.png)

專用測試 ROM 在畫面中央生成四象限空中爆炸後，由 mGBA 擷取的實際
framebuffer；中央沒有額外水平或垂直間隔：

![中央無縫爆炸](../build/explosion_seam_center_v7.png)

### 敵機獎賞掉落

OpenTyrian 的正常擊破流程會把敵人的正 `evalue` 直接加入金額；只有
HDT `eenemydie` 指向「armor=0 且 value != 0」的另一筆敵人物件時，
才會在死亡位置建立可拾取的實體獎賞。第一關的 spawn HDT 本身沒有
靜態 `eenemydie`，但 33 筆 event type 33 會在遊戲進行中依 link 動態
指定；v17 由 source pool 直接執行這條原始路徑：

| HDT | 掉落金額 | 第一關 type 33 記錄 | 原始圖像 |
|---:|---:|---:|---|
| 391 | 25 | 15 | graphics 7–12 |
| 392 | 50 | 3 | graphics 26–31 |
| 393 | 75 | 4 | graphics 20–25 |
| 394 | 100 | 3 | graphics 32–36 往返 |
| 395 | 250 | 1 | graphics 14–18 往返 |

其餘七筆 type 33 目標是資料方塊、武器或特殊模式物件，不會被錯當成
金額。實體獎賞現在與 PC 一樣占用原四組 enemy pool 的 availability-2
slot；與主角碰撞後依 `evalue` 分流 cash、data cube 或 weapon power-up，
並播放原始 `S_ITEM`。普通敵人則在被玩家子彈擊破時直接加入自己的精確
`evalue`，不要求掉出物件。

v8 移除先前為提高辨識度而自行添加的 1 px 淡色輪廓；v11 的五種獎賞
都保留 `11_coins_cubes` 原圖的透明邊緣與色彩，不再出現
原版沒有的特殊色環。

累計金額也改回 PC 原版 `JE_inGameDisplays()` 的表現方式：

- 不顯示先前自行加入的右上角寶石圖示。
- 不使用自製 5×7 黃色數字，也不再限制為五位數。
- 直接使用 TINY_FONT sprite 79（0）及 70–78（1–9）。
- 沿用原版 hue 2、brightness 4、四方向黑色 `FULL_SHADE` 與可變字寬。
- PC 的 `(30,175)` 位於本次 1:1 gameplay crop 下方；因右側 HUD 與
  底部 banner 都已省略，這個唯一的最小 HUD overlay 明確移到 GBA
  `(22,140)`。敵人、背景、玩家、彈幕及碰撞不使用這項 UI 例外。

![25／50／75／100／250 原始獎賞動畫](../build/preview/reward_coins_25_50_75_100_250.png)

![原版 TINY_FONT 金額數字](../build/preview/cash_tiny_font_digits.png)

完整 auto-test 不注入測試金幣；v19 的 PC 慣性移動固定路線從真正
event-33 death spawn 建立三個物件、拾取兩個，取得 50 cash，並與
1,121 direct cash 合計為 1,171。這條路線沒有碰到 data cube，但
data-cube 分流及 counter 仍留在 source runtime；此數字是測試駕駛路徑
變更，不是移除功能。

## 第一關事件與 game loop

第一關來源 route 有 1,009 筆 11-byte 記錄，v19 runtime 直接從
`tyrian1.lvl` 讀取，不先轉為 GBA opcode。簡化 Boss handoff 前實際消耗
878 筆：869 applied、五筆 type-16 UI/audio deferred、四筆由 type-61
條件流程跳過。

Enemy gameplay 只保留 OpenTyrian 的四組 25-entry pool，不再另跑
48-entry POC enemy pool。固定回歸路線有 473 次 event spawn 與三次
`eenemydie` spawn，最高同時 39 個，沒有 pool-full 或 missing definition。
其餘固定 pool 為 12 發主角彈、60 發 source 敵彈與 48 個 GBA effect。

目前第一關本體會執行：

- 原始 link control、速度、加速度、bounce、animation 與出界回收
- 三個 HDT turret slot、event-31 override、aim／track／multiposition
- 玩家彈 armor damage、damaged transition、linked death 與固定殘骸
- `special/globalFlags`、直接 cash、`eenemydie` 及 score-item pickup
- 玩家撞機與敵彈命中 cadence

內部 gameplay 保持 320×200 source 座標；敵人、玩家、子彈、碰撞與
背景視差全部執行 PC 演算法。`src/source_runtime.inc` 最後才把原版
264×184 gameplay viewport 四邊各裁 12 px，不做縮放：

```text
GBA x = PC game_screen x - 36
GBA y = PC game_screen y - 12
```

原版先在 `JE_starShowVGA()` 捨去 framebuffer 左側 24 px，GBA 再捨去
gameplay viewport 左側 12 px，所以 X origin 是 36；Y origin 是 12。
背景使用相同來源矩形與玩家相關的 `mapX/mapX2/mapX3` 水平視差。
為避免 24×28 玩家圖在這個裁切視窗的上下邊被切掉，v19 依 graphic
233/235 的 alpha bbox 把玩家 source Y clamp 由 PC 完整 viewport 的
`10..160` 收窄為 `17..152`；X clamp `40..256` 不變。這是 viewport
adapter 的可見範圍差異，敵人、碰撞、背景與 projectile 座標公式不變。
`curLoc=5400` 後的 Boss 身體、damage lifecycle 與結束流程仍是 POC
簡化行為。

## 速度設定

GBA 顯示更新率為約 59.72750057 Hz。遊戲邏輯使用 fixed-step accumulator，
目標是原版 Normal game speed：

```text
1,193,182 / (0x4300 × 2) = 34.78259095 logic updates/s
```

排程分母已依 GBA 實際顯示率校正為 2,048,892，而不是把顯示率近似成
60 Hz。畫面仍每個 VBlank 更新；敵機、碰撞與地圖事件以約 34.78 Hz
推進，沒有沿用 NES 版的 30 Hz 或 15 Hz 限制。

三個背景也不再使用固定近似速度；source event state 直接執行第一關的
背景控制：

| PC `curLoc` | MAP1 | MAP2 | MAP3 | 說明 |
|---:|---:|---:|---:|---|
| 0 | 1 | 2 | 0 | 原作開場段 |
| 1,000 | 1 | 2 | 3 | 標準 1:2:3 parallax |
| 2,400 | 1/3 | 1/2 | 1 | 原作慢速段 |
| 3,420 | 1 | 2 | 3 | 恢復標準速度 |
| 4,020 | 2 | 4 | 6 | 關底加速段 |
| 5,000 | 3 | 6 | 0 | Boss 入場前加速 |
| 5,280 | 2 | 4 | 0 | 入場減速 |
| 5,300 | 1 | 2 | 0 | Boss handoff 速度 |

數值單位是每個 logic update 的像素；慢速段由原作的 delay counter 重現。
事件以 MAP1 的有效 step 推進 `curLoc`，不是以經過的 logic update 數量
直接推進。

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

暫停時依 PC `JE_pauseGame()` 只把 module volume 降為 448/1024；音樂
時間軸繼續播放，遊戲世界則完全凍結。再按 `Start` 後恢復 896/1024。

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
| ROM internal／host verifier | PASS／PASS |
| Telemetry schema | 17 |
| Logic updates | 7,093 |
| Final PC `curLoc` | 5,400 |
| Display frames／VBlank IRQ | 12,239／12,658 |
| Missed VBlank | 54（約 0.44%） |
| Source event index | 878 |
| Applied／deferred／skipped | 869／5／4 |
| Event spawn attempt／success／full／missing | 473／473／0／0 |
| Death spawn attempt／success／full／missing | 3／3／0／0 |
| Source control writes／RNG calls | 2,509／1,838 |
| Enemy motion updates／releases | 57,890／456 |
| Peak／handoff active source enemies | 39／20 |
| Streamed map rows | 3,590 |
| Map stream drops | 0 |
| Peak OAM/Sprite | 43 / 128 |
| Peak active effects | 31 / 48 |
| Dropped explosion components | 0 |
| Death control／assignments | 32／60 |
| Spawned／picked source items | 3／2 |
| Peak active source items | 3 |
| Data cubes／unsupported pickups | 0／0 |
| Dropped reward items | 0 |
| Direct／pickup／final cash | 1,121／50／1,171 |
| Pause toggles / frozen display frames | 2 / 60 |
| Source enemy shots spawn／release／drop | 185／185／0 |
| Source shot updates／peak | 9,163／9 of 60 |
| Enemy-shot player hits | 11 |
| Player-shot enemy hits／enemy contacts／kills | 341／39／73 |
| Peak visible source enemies | 30 |
| Exact frame catalog／fallback visuals | 198／0 |
| Frame cache hit／miss／eviction／drop | 44,933／145／121／0 |
| Frame uploads／bytes／single-frame peak | 145／74,240／7 |
| Final PC player x/y | 77／17 |
| Final MAP1／MAP2／MAP3 x offset | 24／49／74 |
| Final MAP1／MAP2／MAP3 HOFS | 60／35／34 |
| Presentation crop origin | 36／12 |
| State transitions | 5 |
| ROMFS checks／failures | 93／0 |
| mGBA memory/runtime errors | 0 |

7,093 個 logic tick 約等於 204 秒的 GBA 遊戲時間。增加的時間來自原作
MAP1 慢速段現在也同步控制事件 `curLoc`。Headless mGBA 在 PC 以不限速
模式約 4.6 秒跑完；這個 host 時間不是 GBA 效能數字。54 次 missed
VBlank 包含新的 ROM→OBJ VRAM frame upload，由測試保留並設上限 160；
沒有透過降 logic rate、刪除音樂或圖層隱藏少數重負載 frame。

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

- `Start`：從開場進第一關；遊戲中暫停／繼續
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
7. 驗證成功後將歷史及測試 ROM 歸檔到 `Backup`，清除可重建產物，只在
   `build` 保留最新 release ROM。需要保存 `verification.txt`、ELF、map
   與 log 時使用 `.\build.ps1 -KeepIntermediates`。

若 mGBA GUI 正載入要覆寫的同名 ROM，Windows 會鎖住該檔案；重建前請先
關閉該 ROM 或關閉模擬器。正式交付使用
`_source_parity_crop1to1_playerbounds_romfs_v19.gba` 檔名，避免與先前測試 ROM
混淆。

## 現階段結論

以這個三背景圖層、單關流程的範圍來看，GBA 仍有內容擴充空間，但 CPU
已出現可量測而非理論上的尖峰：

- 100-entry source enemy pool 峰值 39，無 pool-full；OAM 峰值 43 / 128。
- 三層共串流 3,590 列，0 stream drop。
- 185 發 source 敵彈峰值 9 / 60，0 projectile drop。
- 完整 route 有 54 / 12,239 missed VBlank，約 0.44%。
- 含完整 stock ROMFS 與 198-frame catalog 後佔標準 32 MiB 視窗約
  32.44%。
- 三層背景、完整 tracker 音樂、source event/enemy/projectile/collision
  可以同時運作。

目前 198-frame catalog 已消除第一關 enemy/reward fallback；最大的正確性
限制改為 5,400 後仍使用簡化 Boss，以及尚未加入的玩家死亡流程。容量不是
瓶頸。下一階段應讓 source Boss lifecycle 接管，再針對 54 個
over-budget frame 做定位式優化，而不是先降低 logic rate、縮減關卡或
刪除音樂。
