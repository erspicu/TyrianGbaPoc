# Gameplay Overlay VRAM Regression（2026-08-01）

## 現象

Episode 1 第一關即使使用全新進度、初始單一武器且不射擊，入場前幾秒的畫面底部仍出現灰黑色重複圖塊。約三秒後圖塊自行消失，因此它不是武器、爆炸、OAM 數量或 missed VBlank 造成的負荷問題。

## 回歸邊界與根因

問題由 `ccec608`（`complete source-parity loop phases`）加入的 gameplay BG3 overlay 暴露。該功能補回 PC 版的 `JE_drawTextWindow(miscText[20])`、事件提示、警告與星野，但最初的 VRAM 配置互相重疊：

| 用途 | 舊位址 | 問題 |
|---|---:|---|
| BG3 字形 tile（Character Block 3） | `0x0600C000` 起 | 覆寫世界 BG0 map |
| 世界三層 map（Screen Block 24–29） | `0x0600C000–0x0600EFFF` | 與上列完全重疊 |
| BG3 map（Screen Block 31） | `0x0600F800` | 本身沒有重疊，但無法保護字形區 |

`Good luck` 的生命週期是 100 個 source tick。第 70 tick 仍進行 BG3 字形 DMA，所以底部破圖；第 105 tick 訊息已消失，不再進行該 DMA，畫面因而恢復。這也解釋了為何問題看似只發生在入場幾秒。

## 修正配置

保留既有三層背景的 1478 格 character cache 與 overlay 的 136 格完整容量，不降低地圖或提示品質：

| 用途 | 新位址 | 大小／備註 |
|---|---:|---|
| 世界 BG character cache | `0x06000000–0x0600B8BF` | 原容量不變 |
| BG3 可見 map | `0x0600B8C0–0x0600BDBF` | Screen Block 23 的 row 3–22 |
| BG3 tile segment 0 | `0x0600BDC0–0x0600BFFF` | 18 格 |
| 世界三層 map | `0x0600C000–0x0600EFFF` | Screen Block 24–29，不變 |
| BG3 tile segment 1 | `0x0600F000–0x0600FFFF` | 128 格，實際最多再用 118 格 |

BG3 使用 Character Base 2，同時以 `VOFS=24` 讓 Screen Block 23 的 row 3 對應 LCD 最上列。tile 上傳依當幀用量拆成最多兩段 DMA；map、tile 與三層世界背景現在完全不相交。

程式另加入編譯期 `_Static_assert`，驗證背景 cache、overlay map、兩段 tile bank 與 10-bit tile index 的界線。map DMA 目的地明確轉為 `u16 *` 後再做列偏移，避免 SDK 的 `void *` 算術把 96 words 誤算成 96 bytes。

## 針對性驗證

- PC 版直接進 Episode 1 第一關，擷取約 2–3 秒畫面作為來源參考。
- GBA 第 70 tick：修正前可穩定重現底部破圖；停用初始提示後破圖消失。
- GBA 第 70 tick：修正後保留 `Good luck`，且與停用提示版本相比只差 101 個文字像素（差異框 `x=4..61, y=152..158`）；其餘 38,299 個像素完全一致。
- GBA 第 105 tick：修正前後 PNG SHA-256 相同，逐像素差異為 0。
- 強制 100-star overlay：mGBA trace 顯示兩段 tile DMA 分別落在 `0x0600BDC0` 與 `0x0600F000`，map 落在 `0x0600B8C0`，未再寫入世界 map 範圍。
- 上述 ROM 均維持正式版 `TYRIAN_GBA_DYNAMIC_FRAME_DROP=1` 與 `TYRIAN_GBA_WALL_CLOCK_LOGIC=1`；drop-frame 狀態不是此回歸的變因。

診斷截圖保存在 `temp/issue_level1_opening_20260801/`，不納入版本控制。
