# Tyrian GBA cartridge 資源去重稽核

日期：2026-08-16  
基準：LOW detail、Normal game speed、正式非壓力測試組態

## 結論

本次同時檢查 `assets.s` 靜態 payload、`res/*.bin`、ROMFS manifest、
正式 ELF 未解析符號與檔案 SHA-256。精簡後：

- 正式 ROM 由 28,063,516 降至 26,934,588 bytes，淨減
  **1,128,928 bytes**。
- 32 MiB 上限內尚餘 6,619,844 bytes（約 6.313 MiB）。
- `res/` 不再殘留未嵌入的 `.bin`；所有現存 `.bin` 都是正式 runtime
  payload。
- ROMFS 30 個 active payload 之間沒有整檔 SHA-256 重複。
- 所有 `assets.s` payload 起始符號都能在正式 object code 找到真實引用。

## 已移除的 cartridge 重複資料

新版前端已使用「共用 chrome + 預烘培局部面板 + runtime 動態數據」，舊
Mode-4 atlas 仍把每個 selection 與舊統計畫面保存成完整 240×160 frame。
那些 frame 只用於 build-time 預覽與 dirty-rectangle coverage 驗證，正式
runtime 已沒有完整畫面消費者。

| 項目 | 修改前 | 修改後 | 省下 |
|---|---:|---:|---:|
| Mode-4 完整 frame | 35 × 38,400 | 6 × 38,400 | 1,113,600 bytes |
| Mode-4 palette | 35 × 512 | 6 × 512 | 14,848 bytes |
| 舊 dynamic glyph atlas | 704 | 0 | 704 bytes |
| 靜態 payload 總省略 | | | 1,129,152 bytes |

新增的 semantic frame/palette slot dispatch 及對齊約使用 224 bytes，所以
最終 ROM 淨減 1,128,928 bytes。這個 dispatch 只在前端畫面切換時執行，
不進入關卡 game loop。

仍輸出的六張 frame 是：兩張啟動畫面、Game Menu chrome、Title chrome、
Select chrome 與 Game Over。35 張設計畫面仍會在 builder 記憶體中完整
生成、輸出預覽並做 selection patch coverage 驗證，不因 ROM 去重而降低
視覺回歸能力。

## 已清掉但原本不佔 ROM 的歷史輸出

下列 708,258 bytes 是舊 builder／中間輸出殘留；它們不在 `assets.s`，
所以沒有造成 `.gba` 膨脹，但會讓 `res/` 看起來像同時維護兩套背景資料：

- `bg_palette.bin`
- `bg1_map.bin`、`bg1_tiles.bin`
- `bg2_map.bin`、`bg2_tiles.bin`
- `bg3_map.bin`、`bg3_tiles.bin`
- `frontend_cube.bin`

`build_assets.py` 現在每次建置都會主動清掉上述檔案及其他已退休輸出，
避免增量建置再次留下假重複。

## ROMFS 結果

目前 active ROMFS：30 files、payload 4,491,366 bytes、image 4,492,912
bytes。manifest 已排除 39 份有完整 cartridge 替代品的來源，共
5,420,450 bytes：

- `tyrian.snd`、`voices.snd`、`voicesc.snd` → Maxmod soundbank。
- `tyrend.anm` → GBA Mode-4 ending frames/palette。
- 34 份 `newsh*.shp` → 完整 raw Sprite2 catalog + frontend source stamps。
- `tyrianc.shp` → Christmas raw Sprite2 catalog；未變的 table 仍由
  `tyrian.shp` 提供。

本次沒有再刪 active ROMFS 檔案。`music.mus`、`estsc.shp` 與
`tyrian.cdt` 被加入明確保留契約：前者是 41 首曲目權威資料，後兩者是
PC credits 的唯一圖像／文字來源。它們沒有完整靜態替代品，符合「日後
完整移植仍需的唯一來源必須保留」原則。

`.it` 與 `.wav` 也是 soundbank 的 build inputs，不會以個別檔案再塞入
ROM，因此不能把 `res/` 目錄大小直接當成 cartridge 重複量。

## 暫不移除的高容量衍生資料

以下資料與原始來源有語意重疊，但不是可直接刪除的重複：

- `frontend_source_stamp_data.bin`（8,218,212 bytes）：包含 Upgrade Ship、
  Data、Ship Specs 等頁面所需的 25-phase 預縮放 sparse stamps；改回
  runtime Sprite2 解碼會重新造成選單卡頓與音訊中斷。
- `frontend_nav_obj_tiles.bin`（3,027,200 bytes）：其中約 906,368 bytes
  的 32-byte tile block 可在理論上重用，但目前依賴單次連續 DMA 與固定
  OAM layout。未建立 staged EWRAM dictionary/cache 前不能直接去重。
- `tyrend_gba_frames.bin`（4,262,400 bytes）：111 張 frame 的整張 CRC
  全部不同；delta 壓縮可能省 ROM，但會增加播放期間解碼成本。
- `sprite2_raw_components.bin`（1,940,736 bytes）：是 gameplay cache miss
  快速路徑，不等同於 frontend 的預縮放 stamp layout。

下一輪若仍需縮 ROM，優先研究 navigation OBJ 的透明 tile strip packing，
其次才是 source-stamp dictionary；兩者都必須先保證靜態選單音訊不退化。

## 驗證

- LOW 正式編譯、link、gbafix：PASS。
- build-time 35 張畫面／64 selection transitions coverage：PASS，未覆蓋
  pixel 為 0。
- 實機輸出路徑截圖：Title、Logo、Play Mode、Game Menu、Next Level、
  Game Over frame/palette slot 均已覆蓋；Game Menu 與 Next Level 實際
  240×160 畫面正常。
- `TGFA` 17 條前端路徑 × 120 次：功能 failure 0、runtime Sprite2 decode
  0。此工作樹既有 Options／Save 等路徑仍有 missed-VBlank；本次只確認
  資料映射與功能完整性，不把既有效能問題宣稱為已解決。

