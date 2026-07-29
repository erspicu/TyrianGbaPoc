# Tyrian GBA v49：本機專案網站

日期：2026-07-29
狀態：完成、本機靜態網站驗證 PASS、尚未對外部署

## 目的

在專案根目錄新增 `Website/`，以一般玩家也能理解的方式介紹
TyrianGbaPoc，並保留獨立的技術研究專區。網站說明本專案如何從
「GBA 能否忠實執行 Tyrian」的單關技術驗證，逐步轉為盡可能完整、
可維護且依原始資料與程式行為移植的專案。

網站沒有把效能成果包裝成硬體奇蹟，而是交代 GBA 的 16.78 MHz
ARM7TDMI、VRAM、OAM、ROM 與 VBlank 限制，以及專案透過資料預處理、
快取、固定時序、硬體圖層和回歸量測逐一處理瓶頸的過程。開發工具
部分也誠實說明 Codex 參與實作與驗證、Gemini 3.1 Pro 曾提供設計
諮詢，而最終決策仍以原始碼、實機規格與 telemetry 為準。

## 網站內容

- `index.html`：專案定位、移植難點、工程成果、AI 輔助方式、
  實際 GBA 畫面、技術文章入口與下載入口。
- `download.html`：GitHub Releases、遊玩方式、一鍵建置、
  `Configure.h` 與權利聲明。
- `research/index.html`：技術研究索引。
- `research/rendering.html`：三層背景、中央 1:1 裁切與繪圖順序。
- `research/sprite-cache.html`：Sprite2 預解壓、EWRAM L2 與
  palette/filter 邊界。
- `research/timing.html`：固定遊戲時鐘、VBlank 與 frame drop。
- `research/frontend.html`：靜態選單的 build-time page、局部文字、
  硬體 OBJ 與原子換頁。
- `research/romfs.html`：原始 MUS/SHP/PIC/HDT/LVL 的 ROMFS 資料層。
- `research/verification.html`：mGBA、SRAM telemetry 與回歸契約。

首頁畫廊使用目前 ROM 的真實 mGBA 截圖，未使用錯誤標註圖或虛構
遊戲畫面。ROM 不放進網站或 Git；下載按鈕固定連到 GitHub 最新
Release，後續發布新版本不必再修改頁面。

## 視覺與操作

- Tyrian 風格的深色太空背景、青色訊號與黃銅色重點；
- 純 HTML、CSS、JavaScript，沒有套件管理器或外部 runtime；
- 桌面、平板與手機自適應；
- 行動版導覽使用可操作的展開按鈕；
- skip link、語意化導覽、替代文字與 reduced-motion 支援；
- 網站可直接開啟，也可執行 `Website\Serve-Website.bat`，
  在 `http://127.0.0.1:8080/` 預覽。

## 驗證

| 項目 | 結果 |
|---|---:|
| HTML 頁面 | 9 |
| 相對連結／fragment | 全部存在 |
| 重複 HTML id | 0 |
| JavaScript 語法 | PASS |
| SVG XML | PASS |
| 本機 HTTP smoke | 9/9 HTTP 200 |
| 桌面首頁視覺檢查 | PASS |
| 技術研究頁視覺檢查 | PASS |
| 真正 390 px mobile viewport | PASS |
| mobile document scroll width | 390 px |
| mobile 水平溢位 | 0 px |
| mobile 導覽開啟／關閉 | PASS |

## 發布狀態

依目前需求，這一階段只建立可獨立預覽的本機網站；未建立
GitHub Pages、Sites 專案或任何 production deployment。正式發布
時可直接把 `Website/` 當作靜態站台來源，不需要額外 build。
