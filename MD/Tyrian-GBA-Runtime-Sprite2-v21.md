# Tyrian GBA v21：ROMFS 原始 Sprite2 runtime 直讀版

更新日期：2026-07-26

## 結果

v21 移除遊戲對 Python 預先產生的 198-frame enemy catalog、frame tiles
與專用 structure palette 的依賴。第一關敵人與實體獎賞現在由 GBA C
runtime 直接讀 ROMFS：

```text
HDT/LVL 的 shape_table、graphic、size、filter
    ↓
ROMFS newsh*.shp，或 tyrian.shp 的 compact section
    ↓
OpenTyrian Sprite2 skip/fill 命令流
    ↓
32×32 PC palette-index canvas
    ↓
8bpp GBA OBJ cache
```

`newsh*.shp` 沒有加密。它的 Sprite2 command stream 本身是輕量壓縮；
ROMFS v1 則是 stored、可直接定位的唯讀容器，沒有再壓縮或解密。

## 直接翻寫範圍

新增的 `src/opentyrian_sprite2.c/.h` 是平台無關的 C decoder，對照
OpenTyrian `sprite.c` 的 `blit_sprite2()`、
`blit_sprite2_filter()` 與 `JE_drawEnemy()`：

- 第一個 little-endian `u16` offset 同時決定 one-based sprite count。
- command 低 nibble 是透明 skip，高 nibble 是 opaque fill。
- `0x0f` 結束 component；每個 component 是 12×14。
- `filter != 0` 時逐 pixel 執行
  `filter | (source_index & 0x0f)`。
- large enemy 依原作組合 `graphic + 0, +1, +19, +20`，位置為
  24×28 source frame 內的 `(0,0), (12,0), (0,14), (12,14)`；
  放入 32×32 OBJ canvas 後的起點則是 `(4,2), (16,2), (4,16), (16,16)`。
- 透明與 PC palette index 0 用 16-bit scratch 的 `0`／`index+1`
  分開表示，不假設 opaque index 0 永遠不會出現。

`src/opentyrian_data.c` 的 shape-table dispatch 也改為原作規則：

- table 21 → `tyrian.shp` section 11（coins/datacubes）
- table 26 → `tyrian.shp` section 10（power-ups）
- 其他 table → `shapeFile[]` 對應的 `newsh*.shp`
- 原作 shape table 30 的 DOS 字元 `@` 會對應發行資料實際檔名中的 `~`，
  因此可正確開啟 `newsh~.shp`

這個介面不綁第一關事件清單；後續關卡只要其 SHP bank 已存在 ROMFS，
不需要再新增逐敵人 Python mapping。

## GBA 顯示轉接

單張 PC enemy frame 可能使用超過 15 個 palette index，無法由一組
4bpp OBJ palette 精確表示。v21 因此只把 source enemy 改為 8bpp OBJ：

- 使用第一關實際的 PC palette 5。
- 保留 16 個 PC hue。
- 每個 hue 保存八個 brightness sample：
  `0, 2, 4, 6, 8, 10, 12, 15`。
- 其餘亮度在同 hue 內依 palette RGB distance 選最近 sample。
- 玩家、玩家彈、爆炸、HUD、敵彈與 PAUSED 仍保留各自的 4bpp bank。

敵人 cache key 是：

```text
shape_table + graphic + size + filter
```

21 個 32×32 8bpp slot 分成兩段 OBJ VRAM：

```text
tiles 224..511： 9 slots
tiles 640..1023：12 slots
```

同時可見的 unique frame 峰值實測為 16，所以完整路線保有五個 slot
餘裕。Cache miss 在 EWRAM 解碼與排列，實際 VRAM DMA 只在 VBlank
執行。

敵人 cache 會覆寫原本全數常駐的 explosion tile 範圍，因此 v21 另有
32-slot、16×16、4bpp explosion cache；原始 explosion frame 仍保留在
cartridge-side `obj_tiles`，需要時才在 VBlank 複製。切到簡化 Boss 前
則恢復完整 static OBJ palette。

## Python 邊界

`Makefile` 與 `assets.s` 已不再產生或連結：

```text
enemy_frame_catalog.bin
enemy_frame_tiles.bin
enemy_structure_palette.bin
```

`res/asset_report.txt` 會明確記錄：

```text
obj_enemy_preconverted_frames=0
obj_enemy_runtime_source=ROMFS newsh*.shp/tyrian.shp
```

目前 Python 仍負責尚未改成 runtime provider 的三層 GBA background
tile cache、玩家／敵彈／爆炸等 static presentation assets，以及 Maxmod
waveform cache。這些不是 v21 enemy Sprite2 的 runtime 來源。

## 完整路線量測

Telemetry schema 19 在 mGBA 跑完開場、第一關、簡化 Boss、回開場：

| 項目 | 結果 |
|---|---:|
| Logic updates／display frames | 7,093／12,239 |
| Sprite2 decode failure／cache drop | 0／0 |
| Sprite2 hit／miss／eviction | 44,926／152／131 |
| Sprite2 uploads／bytes／單 frame 峰值 | 152／155,648／7 |
| Sprite2 同 frame unique 峰值／slots | 16／21 |
| Explosion cache hit／miss／drop | 4,266／2,382／0 |
| Explosion uploads／bytes／單 frame 峰值 | 2,382／304,896／11 |
| Missed VBlank | 155（約 1.27%） |
| Peak OAM | 43／128 |
| Layer relation checks | 252／252 PASS |
| ROMFS checks | 93／93 PASS |

這表示 raw command-stream 解碼沒有成為每 frame 固定成本：152 次 miss
之後，44,926 次 enemy draw 直接命中 cache。不過 v21 的整體延遲 frame
由 v20 的 54 增至 155，因此此 ROM 刻意作為「音畫正確性與 runtime
成本」試用基準，不把成本隱藏掉。

## 若 runtime raw 版本手感不理想

下一個 provider 應是通用、與關卡無關的 SHP row database，而不是再回到
第一關事件 catalog：

1. 一次掃描 ROMFS manifest 內所有 `newsh*.shp` 與 `tyrian.shp` compact
   bank。
2. 每個 12×14 component 預先展開成 row-major PC palette indices。
3. 保存 bank/sprite offset table、版本、CRC 與透明 mask。
4. C 層維持相同
   `shape_table + graphic + size + filter` API，只替換 component provider。
5. 大小敵人組合、filter、active palette、8bpp cache 與 OAM 邏輯仍由
   共用 runtime 處理。

如此每新增一關只增加原始 LVL/HDT 所引用的 bank，不需寫關卡專用 Python
規則，也可移除 runtime skip/fill 解壓成本。

## 試用 ROM

```text
build/tyrian_gba_level1_source_parity_runtime_sprite2_romfs_v21.gba
10,785,544 bytes
SHA-256 8c976a8c6abfec7f931eea17e76d2f3dc5b7e1a3b998c5dee75321c167e99a33
```
