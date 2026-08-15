# Tyrian GBA ROMFS 資源去重與保留規則

日期：2026-08-05  
適用範圍：`vfs/manifest.json`、`vendor/tyrian/data/`、build-time `res/`

## 定案

ROMFS 只排除「另一份 cartridge runtime payload 已完整承接」的原始檔，
不能以「目前尚未呼叫」作為刪除理由。原始來源一律留在 `vendor/`，所以
這裡的去重只縮小 `.gba`，不會破壞專案獨立重建或日後繼續移植。

本次從 active ROMFS 排除 34 份 `newsh*.shp`。它們共 1,074,186 bytes，
已由下列完整資料承接：

- `res/sprite2_raw_components.bin`：38 個 logical bank × 304 components，
  共 11,552 個 12×14 component；透明度與 PC 256 色 index 無損保留。
- build 時逐 component 重播原始 RLE，確認 skip、fill、像素 index 與
  terminator 均可 round-trip；目前 raw CRC32 為 `38f795b9`。
- `newsh1.shp` 是 Upgrade Ship／shop 專用 bank。它的 284 個可組合
  2×2 graphics 與 25 個縮放相位已完整放入 front-end source-stamp
  catalog；17 條選單路徑各 120 次測試均未觸發 runtime Sprite2 decode。

runtime 的 `ot_sprite2_frame_decode()` 現在直接組合完整 raw catalog；
LVL／HDT 仍以原始 `shape_table`、`graphic`、`size` 與 `filter` 決定要畫
哪一張，並沒有建立 per-level 對照表或改寫遊戲資料語意。

## 已排除的完整重複 payload

| 原始來源 | 檔案數 | bytes | cartridge 替代資料 |
|---|---:|---:|---|
| `tyrian.snd`、`voices.snd`、`voicesc.snd` | 3 | 585,554 | `res/soundbank.bin` |
| `tyrend.anm` | 1 | 3,315,848 | `res/tyrend_gba_frames.bin` + palette |
| `newsh*.shp` | 34 | 1,074,186 | Sprite2 raw + front-end source stamps |
| `tyrianc.shp` | 1 | 444,862 | Christmas Sprite2 raw；共用 table 由 `tyrian.shp` 提供 |
| **合計** | **39** | **5,420,450** | build audit 逐檔記錄 SHA-256 |

`vfs/manifest.json` 的 `omitted_duplicates` 必須列出每個排除 pattern 與
替代資源。`tools/build_romfs.py` 仍會讀取原始檔、記錄大小與 SHA-256；
原始檔遺失、同時出現在 active ROMFS，或 pattern 重複時都會中止建置。

## 完整曲目與 Christmas 資源保留

下列來源由 `retained_sources` 或完整 build 資產契約保護；任何後續精簡
若誤刪，build 會直接失敗：

| 用途 | 保留資料 | bytes | 原因 |
|---|---|---:|---|
| PC 完整曲目 | `music.mus` | 153,482 | runtime 曲目權威資料，包含正常由腳本選用的 Halloween Ramble |
| PC 完整曲目 | `res/tyrian_music_15.it` | build 產生 | Halloween Ramble 的 GBA Maxmod module；41 首完整集合的一部分 |
| 聖誕節 | `tyrianc.shp` | 444,862 | Christmas main shape tables |
| 聖誕節 | `voicesc.snd` | 188,275 | Christmas voice bank |

`newsh*.shp` 的原始檔雖不再重複塞進 ROMFS，仍完整留在
`vendor/tyrian/data/`，而其所有現有 Sprite2 像素已在 cartridge raw
catalog 中；若日後需要新增非像素 metadata，可從 vendor 原始流擴充
builder，不需找回已刪除資料。

## 仍保留在 ROMFS 的「相同來源衍生資源」

以下檔案雖也被 build-time 資源讀取，但靜態 `res` 只承接一部分顯示工作，
不能視為完整替代品：

- `tyrian.shp`：runtime 字型、portrait、player／projectile 與一般 SHP
  reader 仍直接取用。
- `tyrian.pic`／`palette.dat`：章節劇情、PIC 解碼與動態 palette 仍取用。
- `tyrian?.lvl`／`shapes*.dat`：事件、enemy pool、三層地圖、tile pattern
  與背景 streaming 的唯一權威資料。
- `tyrian.hdt`／`levels?.dat`／`cubetxt?.dat`：武器、敵人、商店、路線、
  劇情與 Data Cube 的真實規則資料。
- `music.mus`：Maxmod module 是播放 adapter；曲目結構、合法 song ID 與
  選曲狀態仍由原始 MUS reader 驗證。
- `estsc.shp`、`tyrian.cdt`、`tshp2.pcx`：仍是唯一 stock payload，不能
  因為某條目前流程使用較少就刪除。

## 本次容量結果

| 項目 | 修改前 | 修改後 | 差異 |
|---|---:|---:|---:|
| ROMFS entries | 66 | 32 | -34 |
| ROMFS image | 6,202,024 | 5,126,152 | -1,075,872 bytes |
| Normal release ROM | 29,395,520 | 28,318,588 | -1,076,932 bytes |
| 32 MiB ROM 使用率 | 87.60% | 84.40% | -3.20 percentage points |

ROMFS overhead 也因少了 34 筆 path/index 而縮小，所以 image 節省量略大於
`newsh*.shp` payload 本身。Normal ROM 尚餘 5,235,844 bytes（4.99 MiB）。

## 驗證門檻

- ROMFS manifest／payload／metadata CRC 與 SHA-256：PASS。
- Sprite2 build-time 11,552-component RLE round-trip：PASS。
- 62/62 playable sections：PASS；53,338 events、459 enemy-pool entries。
- 8,063 個實際引用 graphic 的 frame compositor／tiled L2 parity：PASS。
- route failure、ROMFS failure、background approximation：全部 0。
- 一般 Episode 1 autotest：PASS；Sprite2 decode failure 0。
- front-end 17 paths × 120 transitions：runtime Sprite2 decode 0、功能
  failure 0。選單既有的 Options／Save timing telemetry 不屬本次資源
  去重，另由前端效能工作追蹤。

以後若要新增排除項目，必須同時回答三件事：替代 payload 是否完整、
runtime 是否已沒有原始 reader、以及跨四 Episode／季節模式的驗證如何
證明沒有 coverage hole。三者任一不成立，就保留原始 ROMFS 檔案。

## 2026-08-16 再稽核

目前 active ROMFS 為 30 files、payload 4,491,366 bytes、image
4,492,912 bytes，active payload 之間沒有整檔 SHA-256 重複。
`music.mus`、credits 圖像 `estsc.shp` 與 credits 文字 `tyrian.cdt` 已列入
`retained_sources`；三者都是唯一來源，不能因目前 UI 尚未完整接通而刪除。

靜態 atlas 與 `res/` 的詳細稽核、1,128,928-byte 正式 ROM 精簡結果見
[`Tyrian-GBA-Cartridge-Resource-Dedup-Audit-2026-08-16.md`](Tyrian-GBA-Cartridge-Resource-Dedup-Audit-2026-08-16.md)。
