# Tyrian GBA Jukebox v27

更新日期：2026-07-26
工作分支：`opentyrian-source-parity-port`

## 本階段成果

主選單原本停用的 `Jukebox` 已可實際進入。v27 對照固定基準
OpenTyrian commit `1c34d1bddac8c8f2de834229d04b5a729525c944` 的
`jukebox.c`、`starlib.c` 與 `musmast.c`，保留原始曲目順序、曲名、
循環切歌、自然播畢換曲、淡出及星空投影語意；只有顯示與音訊輸出改成
適合 GBA 的硬體轉接。

ROM 內已放入全部 41 首 Tyrian 曲目，並保持：

```text
OpenTyrian source song index == Maxmod module index
```

因此 title 29、第一關 17、End of Level 9、Game Over 10 及 Jukebox
0..40 都共用同一套 song-selection API，不再維護四首曲目的 switch。

## 操作

- D-pad 左／上：上一首；第 1 首再往前會到第 41 首。
- D-pad 右／下或 `A`：下一首；第 41 首再往後會回到第 1 首。
- `Select`：隱藏／恢復文字，只保留星空。
- `B` 或 `Start`：淡出並回到主選單。

退出完成後會重新載入 title song 29；不會把 Jukebox 最後播放的曲目留在
選單。

## GBA 顯示轉接

PC 原版每幀以 software renderer 畫 1,000 顆 3D 星。若直接照搬成
240×160 bitmap，切歌或刷新文字時會產生不必要的 38.4 KiB frame copy。
v27 改用 Mode 0：

| GBA 資源 | 用途 |
|---|---|
| BG0 | 8×8 tile 文字及 2 KiB text map |
| BG1 | 16-tile 星空底圖及 parallax scroll |
| OBJ | 112 顆即時投影星，保留 16 格 OAM 餘裕 |
| BG／OBJ palette | 15/16 階淡出、亮度與色相循環 |

星體仍保存 OpenTyrian `spX`、`spY`、`spZ` 概念及多種 setup pattern。
進入與 respawn 時可做整數除法；每幀投影改查 1..500 的 reciprocal table，
避免對 112 顆星逐一做昂貴除法。星形以三個距離層級的 OBJ tile 呈現。

曲目變更只重建 BG0 text map；VBlank 最多更新 2 KiB map、palette 及 OAM，
不重建整張畫面。

字型不是臨時 ASCII 字庫。Host asset build 直接讀原始資料目錄的
`tyrian.shp` table 1 字形，再離線縮入 8×8 tile；曲名直接解析
`musmast.c` 的 41 筆原始標題。

## 41 首音樂轉換

原始 41 個 TYM／LDS tracker 輸入合計 1,235,984 bytes。現有 GBA
waveform adapter 會將每首轉成 sparse IT module；41 個產生的 IT 合計
1,369,119 bytes，再由 Maxmod 建立單一 soundbank：

```text
MSL_NSONGS = 41
MSL_NSAMPS = 456
soundbank  = 1,077,332 bytes
```

部分曲目（例如 Camanis）的 loop 前 intro pattern 超過 IT／Maxmod
可接受的 200 rows。v27 沒有刪除前奏或壓短時間，而是在 asset builder：

1. 解析 packed IT row boundaries。
2. 將 oversized pattern 分成多個不超過 200-row 的 pattern。
3. 展開 order table，使播放順序及 timing 不變。
4. 重定位 `Bxx` position jump；原本跳向某個 source order 的 loop 仍會
   跳到該 order 分段後的第一個 pattern。

41 首均可完成轉換及 Maxmod soundbank 建置。這一層仍是 GBA
presentation adapter；ROMFS 的原始 `music.mus` 及 loader 沒有被替換。
未來若直譯 LDS／OPL mixer，可保留相同 source song index API。

## 自動測試

新增獨立 game code `TYGJ` 的 Jukebox ROM 與 `TGJ1` SRAM telemetry。
測試實際從主選單進入並執行：

1. song 0 按左，驗證環回 song 40。
2. 按右，驗證 song 40 環回 song 0。
3. 按 `A`，切到 song 1。
4. `Select` 隱藏／恢復文字。
5. `B` 執行 fade-out，回到 title song 29。

預設 Low Detail／Normal Speed 實測：

| 項目 | 結果 |
|---|---:|
| Jukebox display frames | 177 |
| Track changes | 3 |
| Previous／next wraps | 1／1 |
| Text toggles | 2 |
| Text-map commits | 5 |
| Palette commits | 41 |
| 最大 OAM | 112／128 |
| Exit | 1 |
| 最終 Jukebox song | 1 |
| 返回後 source song | 29 |
| Maxmod module count | 41 |

四組 `Detail Level` × `Game Speed` 組態皆同時通過完整第一關、強制死亡、
Jukebox 與 600-frame release boot 測試。

## 容量

預設 v27 release ROM：

```text
12,931,768 bytes
12,628.68 KiB
38.5397% of 32 MiB
```

全部 41 首仍只使用標準 GBA 32 MiB ROM window 的約 38.54%，不需要刪除
曲目。ROMFS 原始遊戲資料約 9.85 MB，Maxmod soundbank 約 1.08 MB。

## 主要程式位置

- `src/jukebox_runtime.inc`：星空、文字、切歌、fade 與控制流程。
- `src/frontend_runtime.inc`：主選單 Jukebox 入口。
- `src/gba_platform.inc`：Jukebox 專用 VBlank commit 與通用 song loader。
- `src/autotest.inc`：Jukebox deterministic input／SRAM telemetry。
- `tools/build_assets.py`：來源字型、曲名、投影表與 41 首 IT 轉換。
- `build.ps1`：完整關卡、死亡、Jukebox、release boot 的統一驗證。

## 已知界線

- 星體數由 PC 的 1,000 降為 112，因為 GBA 全畫面只有 128 OAM；保留的是
  source movement/setup 語意，而不是逐像素 software framebuffer。
- 音訊目前是 TYM/LDS 到 IT/Maxmod 的離線轉接，不是 FM/OPL register
  emulator。曲目事件與順序完整保留，但合成器 timbre 仍會受 Maxmod
  sample bank 影響。
- Jukebox 完成後，下一個主要移植階段是 Episode、`nextLevel` 與第二關
  的通用資料驅動流程。
