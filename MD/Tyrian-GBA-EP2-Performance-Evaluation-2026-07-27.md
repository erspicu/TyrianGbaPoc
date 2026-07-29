# TyrianGbaPoc Episode 2 第一關效能評估（修改前）

日期：2026-07-27
基準版本：v30
狀態：評估與實作均已完成

我已完成 v30 的函式 placement、IWRAM／EWRAM 預算、Episode 2
路線實測，以及關閉音訊 A/B。以下數據均是在修改 tracked source
之前取得。

## 主要卡頓原因

- Episode 2 第一關完整 route 的 `missed VBlank` 是 553；Episode 1
  同類測試約為 13。
- `background_cache_approximations=472`、
  `background_late_columns=851`，顯示背景快取耗盡後反覆進入
  512 格逐格比對的昂貴近似路徑。
- 現行程式把硬體 32 列 tilemap ring 全部視為受保護，但 GBA
  畫面實際只需要 21 列（20 列高度，加上捲動時的部分列）。
- 離線重播原始 `tyrianN.lvl`、`shapes?.dat`、`palette.dat` 的全部
  62 個 logical level：Episode 2 第一關的 21 列工作集最高為
  486 patterns，含下一列預取最高為 501，均低於現有 512 格；
  保護 32 列時則可能膨脹到約 642。
- 因此把 reference ownership 縮到可見工作集，預期可移除
  Episode 2 第一關的 472 次近似掃描，而且不需要建立 per-level
  專用資源。
- 少數 Episode 2 後段關卡（例如 logical level 6 與 11）即使只看
  可見區仍可能略超過 512，這是 GBA BG tile index／VRAM 的真實
  上限；會保留原有、有界的近似 fallback。

## IWRAM 建議

v30 release 的 IWRAM 尚餘 7,360 bytes；build 另有至少保留 6 KiB
的安全門檻，不能直接把所有候選函式搬入。

- `ot_sprite2_frame_decode()`：Sprite2 raw L2 路徑的 RLE fallback
  實測為 0 次；搬入 IWRAM 沒有 gameplay 收益，因此不搬。
- 舊 `source_enemy_pack_8bpp()`／`compact()` 已被 v29 raw component
  路徑取代；真正熱點是
  `source_sprite2_l2_write_raw_component()`。
- `source_enemy_cache_acquire()` 值得搬；ARM code 約 1,036 bytes。
- `ot_level_port_collide_player_shot()` 值得搬；ARM code 約
  3,184 bytes，Episode 2 實測呼叫相關 collision 838 次。
- 先把只在前端使用的 `frontend_text`／map 結構約 4.1 KiB 從
  IWRAM BSS 搬到 EWRAM，仍可維持 EWRAM 至少 48 KiB 的門檻，
  再把上述 gameplay 熱函式放進 IWRAM。
- raw component writer 現在雖標示 IWRAM／ARM，卻被 `-O3` inline
  回 ROM；會加上 `noinline,noclone`，並以 ELF symbol 與 linker
  free-space 實測確認真正 placement。

## 32-bit packing

- 方向可行而且有正面效益。L2 arena 為 4-byte aligned；12-pixel
  row 可改成三次 32-bit store，x 偏移不齊時則使用 32-bit 主體
  加 16-bit 邊界。
- 「EWRAM 等待減少 75%」不能照字面成立：GBA EWRAM 是 16-bit
  bus，一次 32-bit store 仍需要兩個 bus transfer；不過 store
  指令數約減少 75%、匯流排 transaction 約減少 50%，也能大幅
  減少逐 pixel tile-offset 計算，仍值得實作。
- 透明 pixel 0、filter 與 little-endian byte 順序會做逐 byte
  parity 驗證。

## Build-time 資源

- v29 已把完整 Sprite2 bank 做成通用、無損 raw component，而
  不是 per-level catalog；Episode 2 的 L2 RLE fallback 是 0，
  所以再次預解壓同一批資料不會解決卡頓。
- `shapes?.dat` 的背景像素本身已是 raw；目前主要成本是動態組合
  24×28 shape 相位與 palette 量化，不是解壓縮。
- 先修正 32→21 列的工作集 ownership 並重測。若仍有尖峰，才考慮
  建立「整個 shape bank 通用」的 palette-independent
  fragment／phase cache；不建立每關 Python 專用表。

## 聲音降級 A/B

- 正常 Maxmod：`missed VBlank=553`。
- 停播音樂與 SFX，但保留安全的音訊 driver／VBlank：
  `missed VBlank=512`。
- 聲音只占約 41 次，約為 7.4%，不是嚴重卡頓主因。現階段改成
  Game Boy 四聲道 PSG 會明顯犧牲 Tyrian 音色，收益卻有限，
  因此先不做。
- 圖形修完後如果仍需額外 headroom，可再做 Maxmod 低 mix
  quality 與 PSG 的可切換 A/B flag，而不是直接取代高品質版本。

## 預定修改順序

1. 背景 ring 保留 32 列硬體映射，只讓 21 列可見工作集持有
   references；下一列獨立預取。
2. cold frontend state 搬入 EWRAM；cache acquire、collision 與
   raw writer 真正放進 IWRAM。
3. raw writer 改為 grouped 32／16-bit stores，並做 parity。
4. 重跑 Episode 1、Episode 2 route、ROMFS 全關卡 matrix、
   campaign、death、jukebox，檢查 `missed VBlank`、
   approximations，以及 IWRAM／EWRAM 門檻。

## 實作結果

v31 已完成上述方案：

- Episode 2 第一關 `missed VBlank`：553 → 30。
- background approximations：472 → 28。
- 最佳化後停播全部 music／SFX：30 → 29，因此保留完整 Maxmod，
  不改成 Game Boy PSG。
- release EWRAM／IWRAM free：49,612／6,408 bytes。
- Low／Normal Detail 與 Low／Normal Game Speed 回歸均維持支援。

完整設計、數據與永久回歸請見
`Tyrian-GBA-EP2-Background-Performance-v31.md`。
