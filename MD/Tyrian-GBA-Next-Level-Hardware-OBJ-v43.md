# Tyrian GBA Next Level Hardware OBJ v43

更新日期：2026-07-29

狀態：已實作、畫面核對、專用壓力測試與完整回歸通過

工作分支：`opentyrian-source-parity-port`

發行標籤：`v43`

## 問題定案

Next Level 是偏靜態的設定選單，不是遊戲關卡 renderer。舊路徑卻在每個
星球動畫 tick，以及切換目的地後的 camera easing 期間，反覆執行：

1. 還原 126x138 Mode 4 導航區；
2. CPU 逐像素畫格線；
3. runtime 解碼 route dots；
4. runtime 解碼、縮放並陰影處理 planet；
5. runtime 解碼大型 `tyrian.shp` table 5 sprite 28 裝飾框；
6. 逐列 DMA 導航區。

因此靜止後可能正常，但星球旋轉或切換選項時會讓畫面和音訊停頓。

## v43 架構

### Planet 與 route dots 使用硬體 OBJ

- source authority 維持 ROMFS 的 `tyrian.shp`：
  - planet：table 3；
  - route dots：table 5 sprites 29/30；
  - palette：原版 index 17。
- build 時完整預解碼，不建立 per-level 手工對照表。
- 300x200 到 240x160 是 4/5；為避免 sprite 隨座標相位出現 1-pixel
  抖動，預先產生 5x5 共 25 種 subpixel phase。
- 原版 planet 的 `-4` brightness shadow 在 build 時烘焙。
- runtime 只在動畫 frame 改變時串流目前 OBJ tiles；一般 tick 只更新
  OAM attr。
- WIN0 將 OBJ 裁在導航框的透明 aperture 內，使星球維持在原版裝飾框
  後方。

Mode 4 可用 OBJ VRAM 為 `0x06014000..0x06017fff`：

| 項目 | Bytes |
|---|---:|
| Route dots | 128 |
| Worst-case visible planets | 15,616 |
| 合計 | 15,744 / 16,384 |

生成資源：

| 資源 | Bytes |
|---|---:|
| `frontend_nav_obj_tiles.bin` | 3,027,200 |
| `frontend_nav_obj_meta.bin` | 45,900 |
| `frontend_nav_obj_palette.bin` | 512 |

### Camera 背景使用全域 phase pages

格線只依賴：

```text
(nav_x >> 1) mod 15
(nav_y >> 1) mod 15
```

所以 build 時從 stock PIC1、SHP table 5 sprite 28 通用產生 15x15，共
225 張「menu chrome + grid + ornate frame」導航頁。這是全 Episode、
全關卡共用的無損重新編碼，不是 per-level GBA 資料。

- 有效畫面：126x138；
- ROM row stride：128 bytes；
- 每頁：17,664 bytes；
- 總量：3,974,400 bytes；
- runtime SHP decode：0；
- runtime grid plot：0。

126-byte row 刻意補到 128 bytes。IWRAM/ARM hotpath 每列執行 31 次
32-bit copy 加一次 16-bit copy，所有 ROM source row 都維持 word
alignment。這取代 138 次通用 `memcpy` 呼叫，也是 camera easing 從
missed VBlank 降到零的關鍵。

### 選項更新不再誤重畫地圖

`Tyrian` 與 `Exit to Game Menu` 之間切換時，camera target 並未改變。
舊版仍會重畫整張地圖。v43 只更新：

- 舊、新兩列 native text；
- route-dot OAM。

每次 cursor update 搬移 2,160 bytes，不再搬 17,388-byte 導航區。
若 stock route 真有兩個目的地，原本 camera easing 仍完整保留，並由
phase page hotpath 處理。

## Gemini 3.1 Pro 評估

依使用者要求，以 `gemini-3.1-pro-preview` 提供 GBA Mode 4、OBJ VRAM、
Maxmod、現有 telemetry 和候選方案後諮詢。

採用的建議：

- ROM 空間換 runtime 成本；
- build 時預產 15x15 phase；
- 126-byte row padding 到 128；
- 以 32-bit aligned copy 取代通用逐列 compose。

未直接採用的建議：

- Mode 4 雙頁分幀重建；
- 將單一頁面全面改寫成 Mode 0；
- 取消 camera easing。

原因是量測顯示 aligned IWRAM copy 已在現有 Mode 4 架構達到零漏幀，
無須承擔頁面同步或重寫整套選單文字／chrome 的額外風險。技術決策以
mGBA 實測結果為準。

## 專用壓力測試

組態：High Detail、Normal Game Speed、mGBA headless。

### TGN7：閒置與一般切換

| 指標 | 閒置旋轉 600 updates | Tyrian/Exit 120 次 |
|---|---:|---:|
| VBlanks | 601 | 121 |
| Missed VBlank | 0 | 0 |
| Full redraw | 0 | 0 |
| Bitmap nav redraw | 0 | 0 |
| Dirty commits | 0 | 120 |
| Dirty bytes | 0 | 259,200 |
| Runtime SHP decode | 0 | 0 |
| Runtime Sprite2 decode | 0 | 0 |
| Music active | 1 | 1 |

閒置期間 OBJ update 為 201，動畫 tile uploads 為 150／115,200 bytes，
overflow 為 0。

### TGNC：真正 camera movement

測試在兩個相距 `(45,30)` 的 camera target 間往返 40 次：

| 指標 | 結果 |
|---|---:|
| Camera transitions | 40 |
| VBlanks | 320 |
| Bitmap phase redraws | 80 |
| Dirty commits | 80 |
| Dirty bytes | 1,391,040 |
| Missed VBlank | 0 |
| Runtime SHP/Sprite2 decode | 0 / 0 |
| OBJ overflow | 0 |
| Music active | 1 |
| Final presented camera equals target | 1 |

舊 compose 路徑在同一測試為 80 missed VBlanks；aligned phase copy 後為
0。

## 完整發行回歸

`build.ps1 -DetailLevel high -GameSpeed normal` 與兩個 v43 frontend
stress ROM 全部通過：

- `TGBA schema 25` 主流程、Boss、離場、統計、Game Menu：PASS；
- forced death、Jukebox、五份 Demo：PASS；
- `TGLM schema 2`：62/62 sections、24/24 routes、8,063 sprites；
- `TGCM schema 3`：Episode 1 連續四關；
- Episode 2、3、4 section 1：PASS；
- Arcade route 與 equipment fixture：PASS；
- 600-frame software-renderer boot：`AGB-TYGA,600,79895,software`；
- 所有 mGBA runtime error count：0。

記憶體：

| 項目 | 餘量 |
|---|---:|
| Release EWRAM | 49,960 bytes |
| Release IWRAM | 7,968 bytes |

正式 ROM：

| 項目 | 值 |
|---|---|
| 檔名 | `tyrian_gba_level1_pc_flow_mode4_romfs_v40_detail_high_speed_normal.gba` |
| Bytes | 21,771,560 |
| 32 MiB 使用率 | 64.8843% |
| SHA-256 | `9029a95293a5ae024c40ee32b02169be64afa369d4ded3ddca79bc04553c977f` |
| Release asset | `TyrianGbaPoc-v43-high-normal.gba` |
| GitHub Release | <https://github.com/erspicu/TyrianGbaPoc/releases/tag/v43> |

build artifact policy 維持 release-only：`build/` 最終只保留最新版正式
ROM；測試與歷史 ROM 送入 `Backup/`。

## 可重複指令

```powershell
make frontend-nav-stress DETAIL_LEVEL=high GAME_SPEED=normal
make frontend-nav-camera-stress DETAIL_LEVEL=high GAME_SPEED=normal
pwsh -File .\build.ps1 -DetailLevel high -GameSpeed normal
```
