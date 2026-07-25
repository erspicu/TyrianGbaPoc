# Tyrian GBA cartridge ROMFS

## 目的與目前成果

這套 ROMFS 是給 OpenTyrian C 原始碼直譯使用的唯讀資料層。PC 版原本以
`fopen()`、`fread()`、`fseek()`、`ftell()` 讀取音樂、圖形、文字及關卡
檔案；GBA 版保留相同的「開檔、讀取、定位、關檔」模型，只把底層改成
cartridge ROM 的 memory-mapped 位址。

目前 v1 已實際嵌入 ROM 並在 mGBA 上驗證：

| 項目 | 結果 |
|---|---:|
| Stock runtime files | 68 |
| 原始 payload | 9,849,648 bytes |
| ROMFS image | 9,853,080 bytes |
| 索引、路徑與 alignment overhead | 3,432 bytes |
| 完整 release ROM | 10,453,364 bytes |
| 32 MiB ROM 使用率 | 31.1535% |
| 尚餘標準 ROM 空間 | 23,101,068 bytes（約 22.03 MiB） |
| 開機 ROMFS 自我檢查 | 93／93 PASS |
| Missed VBlank／runtime errors | 0／0 |

封裝內容包含 stock 遊戲所需的 MUS、SND、SHP、PIC、HDT、CDT、LVL、
palette、episode level table、文字及 demo 資料。DOS executable、安裝程式、
說明文件、網路工具及選用的 user ship pack 不會放入 ROM。

原始 PC 資料、產生的 ROMFS image 及 `.gba` 都在 Git ignore 範圍內；
repository 只保存可重現封裝流程與 manifest。

## 資料流程

```text
org/AprCSTyrian/Build/data
              │
              ├── vfs/manifest.json
              ▼
      tools/build_romfs.py
              │
              ├── res/tyrian_romfs.bin
              ├── res/tyrian_romfs_meta.h
              └── res/tyrian_romfs_audit.json
                          │
                          ▼
                  assets.s .incbin
                          │
                          ▼
                  GBA cartridge ROM
                          │
            ┌─────────────┴─────────────┐
            ▼                           ▼
       src/romfs.c          src/opentyrian_rom_io.c
    mount/stat/direct view    fopen/fread/fseek adapter
            │                           │
            └─────────────┬─────────────┘
                          ▼
               src/opentyrian_data.c
               MUS/SHP/PIC/HDT/LVL
```

ROMFS 不會在開機時複製 9.85 MB 資料。GBA 的 cartridge address space
本來就能直接讀取 ROM；file handle 只保存資料起點、長度及目前位置。
需要送入 VRAM、EWRAM 或音訊 buffer 的片段才由各 decoder／loader 搬移。

## v1 image 格式

所有整數都是 little-endian。Image 由 64-byte header、固定寬度索引、
NUL-terminated path table、alignment padding 及未壓縮檔案資料組成。

### Header

| Offset | 型別 | 欄位 |
|---:|---|---|
| 0 | `char[8]` | `TYRVFS1\0` magic |
| 8 | `u16` | format version |
| 10 | `u16` | header size，v1 為 64 |
| 12 | `u16` | entry size，v1 為 32 |
| 14 | `u16` | feature flags |
| 16 | `u32` | entry count |
| 20 | `u32` | index offset |
| 24 | `u32` | string table offset |
| 28 | `u32` | data region offset |
| 32 | `u32` | image bytes |
| 36 | `u32` | logical payload bytes |
| 40 | `u32` | path table bytes |
| 44 | `u32` | deterministic manifest identity CRC32 |
| 48 | `u32` | index + path metadata CRC32 |
| 52 | `u32` | data region CRC32 |
| 56 | `u32` | header CRC32；計算時此欄視為 0 |
| 60 | `u32` | reserved |

### 32-byte file entry

| Offset | 型別 | 欄位 |
|---:|---|---|
| 0 | `u32` | normalized path FNV-1a hash |
| 4 | `u32` | path table relative offset |
| 8 | `u32` | image-relative data offset |
| 12 | `u32` | stored bytes |
| 16 | `u32` | logical bytes |
| 20 | `u32` | individual file CRC32 |
| 24 | `u16` | flags；v1 必須為 0 |
| 26 | `u16` | data alignment |
| 28 | `u32` | reserved |

索引依 `(hash, normalized path)` 排序。查檔先對 32-bit hash 做二分搜尋，
再比較完整 path，因此 hash collision 不會讀到錯誤檔案。

### 路徑規則

- 路徑是相對路徑，v1 mount point 為 `data`
- `/` 與 `\` 都接受並正規化成 `/`
- ASCII 大小寫不敏感
- 重複 separator 與 `.` segment 會被消除
- 拒絕 absolute path、drive/device `:`、`..` traversal 及非 printable
  ASCII
- C reader 的單一路徑上限是 127 characters 加 NUL

例如以下三個名稱會開啟同一筆資料：

```text
data/tyrian1.lvl
DATA\TYRIAN1.LVL
.\data//tyrian1.lvl
```

## C API

底層 `src/romfs.h` 提供 mount、stat、open、direct view 與 CRC API。
OpenTyrian 移植通常使用 `src/opentyrian_rom_io.h` 的 stdio-like adapter：

| PC／OpenTyrian 動作 | GBA adapter |
|---|---|
| `FILE *` | `OtFile *` |
| `fopen(path, "rb")` | `ot_fopen(path, "rb")` |
| `dir_fopen(dir, file, "rb")` | `ot_dir_fopen(dir, file, "rb")` |
| `fread()` | `ot_fread()` |
| exact read | `ot_fread_exact()` |
| little-endian typed read | `ot_fread_u16le_exact()` 等 |
| `fseek()` | `ot_fseek()` |
| `ftell()` | `ot_ftell()` |
| seek-to-end size helper | `ot_ftell_eof()` |
| `fgetc()` | `ot_fgetc()` |
| `feof()`／`ferror()` | `ot_feof()`／`ot_ferror()` |
| `fclose()` | `ot_fclose()` |
| `file_exists()` | `ot_file_exists()` |

簡單範例：

```c
OtFile *file = ot_dir_fopen(ot_data_dir(), "tyrian1.lvl", "rb");
uint16_t header_word;

if (file == NULL) {
    /* missing/corrupt resource */
}
if (!ot_fread_u16le_exact(&header_word, 1, file)) {
    /* truncated resource */
}
if (ot_fseek(file, 0, OT_ROMFS_SEEK_SET) != 0) {
    /* invalid seek */
}
ot_fclose(file);
```

同時最多有 8 個 handle，不使用 heap。需要零拷貝 parser 時，可透過
`ot_romfs_direct_data()` 取得目前位置的唯讀 ROM pointer；不能把該位址
當 RAM 寫入，也不應直接提供給只能從 RAM 來源工作的 DMA／decoder。

### 移植成本控制

不建議在每個 gameplay module 自行加入 GBA 分支。比較穩定的方式是只改
OpenTyrian 原有的 file helper boundary：

1. `FILE *` handle 改成 `OtFile *`。
2. `dir_fopen()`、`file_exists()` 與 typed `fread_*()` helper 改呼叫本
   adapter。
3. 上層 episode、shape、music、sound 與 level parser 保留原本的 read
   order、seek offset、signedness 及 error policy。
4. SDL renderer／audio backend 只接收 parser 解出的資料，不知道 ROMFS
   image 格式。

這樣後續逐行翻寫時，資料 parser 不需要先全部改成生成式 C array，也不會
把 GBA storage 細節滲入 game loop。

## v15 原始格式 reader

`src/opentyrian_data.c/.h` 是 ROMFS 與 gameplay 間的格式層。所有 view 都
直接指向 cartridge ROM；只有 PIC 的 320×200 解碼輸出與 SHP sprite
scratch 需要 EWRAM。

### LVL

`tyrian1.lvl` 開頭為 level count 與 signed 32-bit offset table。原作第一關
使用 `lvlFileNum=9`，所以 map section 是 offset index 16；下一個 map
section 是 index 18，不是相鄰的 index 17：

```text
section       221628 .. 255121
map/shape     Z / Z
levelEnemy    7 x u16
events        1009 x 11 bytes
mapSh         3 x 128 x u16
maps          14x300, 14x600, 15x600 bytes
```

Event reader 直接解碼原始欄位，不再讀取 8-byte `OTL1` header 或生成式
event blob。

### HDT

Reader 依 `JE_loadItemDat()` 的固定 record layout 從 offset 16,465 開始，
跳過七個 item count，weapon table 從 16,479 開始；enemy table 位於
88,130，共 851×77 bytes，結尾必須恰好等於 `tyrian.hdt` EOF。

目前提供完整第一關會用到的 80-byte `JE_WeaponType` 與 77-byte
`JE_EnemyDat` decoder；不再對 110 筆 dependency closure 做 runtime
binary search。

### PIC 與 palette

`tyrian.pic` 有 13 個 signed 32-bit offsets。每張圖是 Tyrian PCX-style
RLE，解碼後必須恰為 320×200 indexed pixels；stock member 另有一個
OpenTyrian 原 loader 會忽略的尾端 `0x0c`。`palette.dat` 驗證為
23×256×3 VGA 6-bit RGB。

開場已由 picture 4／palette 8 即時轉成 GBA Mode 3，原本的 76,800-byte
`title_bitmap.bin` 已移除。

### SHP

`tyrian.shp` 驗證 12-entry section table。前七個 section 依
`load_sprites()` 解析 populated、width、height、encoded size；後五個保持
compact Sprite2 zero-copy view。Shape-table ID 依 OpenTyrian
`shapeFile[34]` 對映成 `newsh*.shp`。

開場 logo 實際使用 PLANET_SHAPES sprite 146，兩行文字使用
FONT_SHAPES；兩者都在 GBA 開機時由 raw SHP command stream 解碼。

### MUS

`music.mus` 驗證 41-entry song offset table及每首 LDS：

```text
mode/speed/tempo/pattern length
9 channel delays + rhythm register
numpatch x 46-byte patch
numposi x 9 x (u16 pattern offset + u8 transpose)
u16 unused digital-sound count
u16 pattern words
```

Title 選 song index 29，第一關選 index 17。Song loader 已完全從 ROMFS
取得結構；Maxmod IT 仍只是 GBA waveform synthesis cache，並非 song
metadata 的權威來源。下一步是移植 LDS/OPL mixer 後移除這兩個 cache。

## 封裝與擴充

選檔規則在 `vfs/manifest.json`：

```json
{
  "format_version": 1,
  "mount": "data",
  "alignment": 4,
  "include": ["music.mus", "tyrian?.lvl", "newsh*.shp"],
  "exclude": [],
  "probes": [
    "tyrian1.lvl",
    "tyrian.hdt",
    "music.mus",
    "tyrian.shp",
    "tyrian.pic"
  ]
}
```

實際 manifest 已列出完整 stock runtime pattern。日後增加資料時：

1. 把來源檔放在 PC data root，或調整 `VFS_SOURCE_ROOT`。
2. 在 `include` 加入精確名稱或 glob。
3. 重要格式加入 `probes`，讓 GBA 開機檢查檔案長度、CRC、頭四 bytes 及
   尾四 bytes。
4. 執行 `.\build.ps1`。
5. 檢查 console 驗證結果與 `res/tyrian_romfs_audit.json`。需要保存
   `build/verification.txt` 及除錯產物時改用
   `.\build.ps1 -KeepIntermediates`。

直接執行封裝器：

```powershell
python tools\build_romfs.py `
  --manifest vfs\manifest.json `
  --source-root ..\..\org\AprCSTyrian\Build\data `
  --output res\tyrian_romfs.bin `
  --meta-header res\tyrian_romfs_meta.h `
  --audit res\tyrian_romfs_audit.json
```

封裝器依 normalized path 排序、不寫入 timestamp，並為每個檔案與完整
image 輸出 CRC32／SHA-256，因此相同輸入會產生 byte-for-byte 相同 image。

## 完整性與失敗處理

Mount 階段會檢查 magic、版本、header CRC、metadata CRC、offset／size
bounds、alignment、path normalization、hash 及 index ordering。每個 entry
也保存自己的 CRC32。

為避免開機掃過將近 10 MB 而延長時間，GBA 不會每次計算整包 payload
CRC；目前會執行 93 項自我測試，並抽查五個主要檔案：

- `tyrian1.lvl`
- `tyrian.hdt`
- `music.mus`
- `tyrian.shp`
- `tyrian.pic`

必要時 loader 可對即將使用的單檔呼叫 `ot_romfs_verify_file()`。Host 端
`Build.ps1` 則會比對完整 ROMFS image SHA-256、audit metadata 與 mGBA
SRAM telemetry。

## 刻意限制

- v1 是唯讀；save game、設定及 high score 應使用 SRAM／Flash 的另一套
  storage adapter，不能假裝寫入 ROMFS。
- v1 不壓縮。現在仍有約 21.95 MiB 空間，直接 mapping 能讓 parser seek
  且避免解壓 RAM／CPU 成本。接近 32 MiB 時再新增 per-file compressed
  flag，適合圖形或文字；不要破壞 v1 stored entry。
- v1 不提供 directory enumeration，因為 OpenTyrian 以已知檔名讀取。
- ROMFS 與 raw parser 不等於 GBA presentation。開場 PIC/SHP 已做 runtime
  轉接；第一關三層背景與 OBJ 目前仍使用可重建的 GBA tile cache，音樂
  waveform 仍送入 Maxmod cache。這些 cache 可以逐項替換，不應把 GBA
  VRAM／mixer 格式滲回原始 loader。
- 若未來 ROM 超過標準 32 MiB，不能只改 linker；需先減少資料、加入壓縮
  或設計非標準 bank switching，並重新確認實機及 flash cartridge 支援。

## v15 驗證識別

```text
Release ROM:
  build/tyrian_gba_level1_source_parity_romfs_v15.gba
  10,453,364 bytes
  SHA-256 7c76472255d49cee9da5b5926d3c8d1997806a43cbe37493b4851a8e1fd4b195

ROMFS:
  9,853,080 bytes
  SHA-256 a9209634a0685fed1e476e60ab1a668b81d2f4ea2671f28f23c8758ae20849af
  manifest CRC32 764b1e68
```

Build 保持 legacy 第一關完整 auto-test PASS，並鎖定 Stage 3：
878 個 source events、869 applied、5 deferred、4 skipped、473/473 event
spawns、453 slot releases、63,381 enemy motion updates。
