# Tyrian GBA v18：PC 座標 authority 與 1:1 中央裁切

更新日期：2026-07-26

## 結論

v18 已把第一關 gameplay 與 presentation 的責任分開：

```text
OpenTyrian LVL/HDT + PC 座標、移動、碰撞、視差與背景 phase
                              ↓
                   264×184 gameplay viewport
                              ↓
             四邊各捨去 12 px；不縮放、不回寫
                              ↓
                     GBA 240×160 BG/OAM
```

敵人、玩家、子彈、爆炸與三層背景全部留在 PC 座標系。GBA adapter
只在送入 BG/OAM 時選取中央 240×160 像素；不再用 4:5 scale，也不把
GBA 座標逆轉換回碰撞或瞄準資料。

## 原版依據

本次以工作區 OpenTyrian commit
`1c34d1bddac8c8f2de834229d04b5a729525c944` 為固定依據：

- `src/video.h`：邏輯 framebuffer 是 320×200。
- `src/tyrian2.c:75` 的 `JE_starShowVGA()`：來源從
  `game_screen->pixels + 24` 開始，每列複製 264 bytes，共 184 列。
- `src/mainint.c:3584` 起：鍵盤每次用 `CURRENT_KEY_SPEED=1` 修改
  player source position。
- `src/mainint.c:3882` 起：X/Y friction、速度累積與 ±4 clamp。
- `src/mainint.c:3965` 起：single-player 邊界是
  `x=40..256, y=10..160`。
- `src/mainint.c:4542` 起：依 player X 計算
  `mapXOfs/mapX2Ofs/mapX3Ofs`。
- `src/tyrian2.c:1274` 起：MAP1/MAP2 先畫再推進；
  MAP3 保留先推進再畫的 phase。

## 精確裁切矩形

PC 已先從 320×200 `game_screen` 取出：

```text
x = 24..287
y =  0..183
size = 264×184
```

GBA 再從這個 gameplay viewport 四邊各裁 12 px：

```text
PC game_screen source: x = 36..275, y = 12..171
GBA destination:       x =  0..239, y =  0..159

GBA x = PC game_screen x - 36
GBA y = PC game_screen y - 12
```

這是整數平移，不是 scale。PC 物件接近 gameplay viewport 邊緣時會被
部分或完全裁掉，屬於規格預期；它在 source pool 的位置、出界判斷、
瞄準、碰撞及死亡生成物都不受裁切影響。

## 玩家與物件

`src/combat_runtime.inc` 現在直接保存 `player_source_x/y`：

- D-pad 只取代 PC keyboard input。
- 每個 input slice 先做 ±1，再執行原版 friction、velocity accumulation
  與 ±4 clamp。
- 玩家邊界使用 PC 的 40／256／10／160。
- 玩家彈 spawn、敵彈瞄準、玩家與敵人碰撞、score item pickup 都讀
  source player position。
- 主角 banking 仍由 source velocity 與當次位移計算。

`src/source_runtime.inc` 對玩家、敵人 draw command、兩類 projectile
與效果只套用 `-36/-12`。OBJ container 的透明 padding／anchor 只處理
GBA OAM shape，沒有改動 source hit box。

主角的 24×28 source cell 也固定放在 32×32 container 的 `(4,2)`；
neutral／right-bank atlas 在量化後只補回舊 converter 裁 bbox 時造成的
`(0,1)`／`(1,1)` 位移，不縮放圖像。

敵人資源也不再逐幀裁掉透明 bbox 再置中。12×14 component 固定放在
32×32 container 的 `(10,9)`；24×28 composite 固定放在 `(4,2)`，
恰好對應 `JE_drawEnemy()` 的 `0,0` 與 `-6,-7` source anchor。這讓
同一 `ex/ey` 的動畫只改變圖像內容，不會因透明邊界不同而抖動。

## 水平視差與 enemy mapoffset

`src/opentyrian_level_port.c` 使用等價的正整數公式：

```text
temp = floor((296 - playerX) * 72 / 224) - 1
mapX3ofs = temp
mapX2ofs = floor(temp * 2 / 3)
mapXofs  = floor(mapX2ofs / 2)
```

四組敵人 pool 在 `JE_drawEnemy()` phase 使用對應的 `mapoffset`：

- ground／ground2：`mapXofs`
- sky：`mapX2ofs`
- top：一般為 `mapX3ofs`；`background3x1` 時為 `mapXofs`

同一個 mapoffset 同時進入 draw command、可見範圍、出界回收、射擊與
碰撞，因此不會發生「畫面在新位置、碰撞仍在舊位置」。

背景 HOFS 使用同一份 snapshot：

```text
MAP1 HOFS = 84  - mapXofs
MAP2 HOFS = 84  - mapX2ofs
MAP3 HOFS = 108 - mapX3ofs
```

若 `background3x1`，MAP3 改用 `84 - mapXofs`。第一個 PC frame 原本的
pointer state 尚未經玩家視差函式初始化，因此保留原始起始 HOFS：
MAP1/MAP2 為 60、MAP3 為 84。

## 1:1 背景與 VRAM ring

舊版背景以 256 px 寬資源為前提，無法同時容納 264 px 原始 gameplay
viewport 與玩家水平視差。v18 的 host converter：

- 把每層 PC raster 放在 512 px／64 tile 寬 canvas，沒有 X scale。
- MAP1 解出 source row 3..299；MAP2/MAP3 解出 row 14..599。
- MAP1 初始 source Y 是 8,104；MAP2/MAP3 是 16,196。
- 共用 palette/tile bank quantizer 時只把左右半頁暫時上下排列，完成後
  還原為 64-column map；不會讓左右半頁各自產生不相容 palette。

GBA Mode 0 使用 `BG_SIZE_1`（512×256）：

| Layer | Char block | Screen blocks | ROM rows |
|---|---:|---:|---:|
| MAP1 | 0 | 24+25 | 1,040 |
| MAP2 | 1 | 26+27 | 2,051 |
| MAP3 | 2 | 28+29 | 2,051 |

VRAM ring 為 32 tile rows。每次跨列時，VBlank DMA 把 64-word ROM map
row 分成左右各 32 words，分別寫入兩個 screen block 的同一列。

垂直 presentation snapshot 也保留 PC phase：

- MAP1/MAP2：保存本幀 source scroll，再推進下一幀 state。
- MAP3：先推進 source scroll，再保存本幀值。

## 最小 HUD 例外

這次 1:1 crop 適用於 gameplay world 與 `PAUSED`：

- PC `PAUSED (120,90)` 顯示於 GBA `(84,78)`。

PC cash `(30,175)` 會落在裁切區之外；使用者先前指定仍要在 GBA 顯示
原版 TINY_FONT 累計金額，因此它是明確記錄的最小 HUD 例外，固定放在
GBA `(22,140)`。這個位置不參與任何 gameplay calculation。

`curLoc=5400` 後的 Boss 仍是既有簡化 POC，Boss bar 也屬於該 adapter；
本次不能據此宣稱 Boss source parity。

## 驗證

建置：

```powershell
cd C:\ai_project\AprTyrianNes\repo\TyrianGbaPoc
.\build.ps1 -KeepIntermediates
```

環境：

```text
ARM GCC 16.1.0
mGBA headless 0.11.0
Telemetry schema 16
```

結果：

| 項目 | 結果 |
|---|---:|
| Internal／host verifier | PASS／PASS |
| Crop origin | 36／12 |
| Final PC player x/y | 77／10 |
| MAP1／MAP2／MAP3 x offset | 24／49／74 |
| MAP1／MAP2／MAP3 HOFS | 60／35／34 |
| MAP1／MAP2／MAP3 vertical source | 2,363／5,225／4,198 |
| Source events applied／deferred／skipped | 869／5／4 |
| Spawn success／pool full／missing | 473／0／0 |
| Map rows streamed／drops | 3,590／0 |
| Peak source enemies／OAM／effects | 39／43／30 |
| Source shots spawn／release／drop | 181／181／0 |
| Effect／reward／frame-cache drops | 0／0／0 |
| Missed VBlank／display frames | 58／12,239 |
| Runtime／ROMFS errors | 0／0 |

另以 mGBA 保存 tick 500 與 tick 2,500 的實際 240×160 framebuffer，
確認左右 screen block 沒有接縫、長距離 ring scroll 沒有錯列，
MAP3 foreground 遮擋及水平視差正常。

正式 ROM：

```text
build/tyrian_gba_level1_source_parity_crop1to1_romfs_v18.gba
10,883,584 bytes（10.38 MiB；32 MiB 視窗的 32.4356%）
SHA-256 5b608adee2c4a21725e84769d10d455461150fa1a794b244f75713c80b148153
```

## 尚未完成

- position 5,400 後仍是簡化 Boss。
- 玩家死亡／重生尚未逐行移植；展示版維持不死亡。
- MUS raw loader 已存在，實際 waveform 仍由 Maxmod cache 播放。
- GBA 背景每層只有 512 tiles；MAP1/MAP3 超出的 unique tile 仍以最近
  tile 近似，這是色彩／tile 容量差異，不是座標縮放。
- 固定測試駕駛在改用 PC 慣性後建立六個、拾取四個 score item，沒有
  碰到 data cube；data-cube gameplay branch 並未移除。
