# TextRes 與不中斷音訊的章節故事畫面（v71）

日期：2026-08-02

## 字體與文字來源

章節／關卡之間的 `levelsN.dat` prose 現在統一使用 Data Cube reader 的
source-derived 5-row 細字體；Episode announcement 也改用同一字面，不再
混用較粗的 pre-game 小字。240x160 capture 已確認筆畫與大小符合
`episode_scene_v67/ep4_section32_red.png` 的基準風格。

新增 `TextRes/Episode1`～`Episode4` 共 67 個 UTF-8 文字檔。檔案中的
換行會成為 GBA presentation 的人工斷行；建置器限制每區塊 10 行、每行
60 個 CP437 bytes，錯誤時會回報檔名與行號。

這不是第二套劇情流程：

- `levelsN.dat` 仍負責圖片、音樂、條件、跳轉與 section 銜接。
- parser 先完整消耗原始 text block，再以 Episode + 原始 ROM offset
  查找 TextRes，只替換顯示文字。
- pack 缺失或格式錯誤時直接回退 stock text，不影響路線。
- `tools/build_textres.py export` 可重新匯出 PC 原文；預設不覆蓋人工編輯。

編譯後的完整 TextRes pack 只有 11,977 bytes。

## 轉場效能

舊路徑會在同一 host frame 做完 64,000-byte PIC RLE、320x200 縮放、
字型準備、所有文字 raster 及 Mode-4 present，會讓 Maxmod 輸入停頓。

新路徑改成可恢復工作：

- PIC RLE 每幀最多輸出 2,048 bytes。
- 320x200 → 240x160 每幀處理 24 rows。
- 縮放使用精確 4:3 x／5:4 y 的 ARM SWAR word pack；每 16 個來源 pixel
  直接組成 12 個目標 pixel，不再逐 pixel 做乘除與位址計算。
- 細字型每幀準備 12 glyph，正文每幀 raster 2 列。
- 完整 inactive Mode-4 page 只在內容完成後由 VBlank present。

Episode 4 section 44 的完整故事、秘密提示、episode announcement 與返回
Game Menu 測試，scene deadline miss 從第一版分段策略的 121 降至 2；
其中 text=0、scale=0、animation=0，僅剩一次 PIC decode 邊界及一次其他
章節命令邊界。玩家可見的長時間連續卡音已消除。

## 驗證

- ROMFS／route matrix：62/62，failure=0，route failure=0。
- matrix 實際 TextRes hit：12。
- Episode 4 wrap：pass=1，story=1，announcement=1，TextRes hit=2。
- resumable PIC 以 4,096-byte budget 精確完成 16 steps。
- Episode 1 section 39 取得實際 240x160 capture，文字、背景與 prompt 完整。
