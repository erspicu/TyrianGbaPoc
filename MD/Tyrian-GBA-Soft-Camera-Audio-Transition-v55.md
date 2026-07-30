# Tyrian GBA v55：柔性裁切鏡頭與進關音訊轉場

日期：2026-07-30  
狀態：完成並通過完整自動回歸

## 目標

- 保留 PC 版 264×184 戰鬥座標與 1:1 像素，不增加 gameplay 縮放成本。
- 利用 GBA 240×160 視窗相對於來源戰鬥區橫、縱各少 24 像素的空間，
  在玩家接近邊緣時平順移動裁切範圍。
- 消除由 Game Menu 進入關卡時，Maxmod 模組突然停止／啟動造成的
  不自然爆音，同時保留選單確認音效。

## 來源規格與決策

OpenTyrian 的戰鬥 viewport 是 264×184；背景則是持續串流的地圖，
並不等於只有一張 264×184 靜態圖。GBA 版仍以 PC 戰鬥座標執行事件、
碰撞與移動，柔性鏡頭只改最後呈現的 viewport，不改 source gameplay
結果。

Gemini 3.1 Pro 的諮詢建議包含 dead zone、一階低通平滑、音樂淡出後
保留靜音 buffer 再換歌。實作採納這些原則，但沒有採用它建議的額外
介面縮放或改變遊戲座標；本專案的正確邊界是既有 1:1 source parity。
完整諮詢問題與回覆保存在：

- `temp/v55_gemini_consultation_prompt_zh.md`
- `temp/v55_gemini_consultation_response_zh.md`

## 柔性鏡頭

- 固定中心原點原為 `(36, 12)`。
- 動態原點範圍為 X `24..48`、Y `0..24`，完整使用兩軸各 24 像素 slack。
- 中央 dead zone 為 120×80 source pixels。
- 每個 30 Hz logic tick 移動剩餘距離的 1/4（Q8 fixed point）。
- 三層背景、敵人、玩家、子彈、爆炸與拾取物共用同一裁切原點；
  各背景原有 parallax 仍完整保留。
- 玩家可移動邊界、碰撞、事件與關卡時間軸完全不因鏡頭改變。

相關參數集中在 `Configure.h`：

- `TYRIAN_GBA_SOFT_CROP_CAMERA`
- `TYRIAN_GBA_CAMERA_SOURCE_CENTER_X/Y`
- `TYRIAN_GBA_CAMERA_DEAD_ZONE_HALF_X/Y`
- `TYRIAN_GBA_CAMERA_RESPONSE_SHIFT`

## 無爆音音樂轉場

Next Level 確認後改用非同步 VBlank 狀態機：

1. 僅淡出 Maxmod module 18 VBlanks；不停止 UI SFX。
2. 音量為零後維持 1 個完整靜音 VBlank。
3. 呼叫 `mmStop()`，載入關卡。
4. 關卡歌曲以 module volume 0 啟動，再於 30 VBlanks 淡入。

這避免在 Direct Sound FIFO 中途直接切斷非零波形。參數同樣放在
`Configure.h`，可調整淡出、靜音與淡入長度。

## 驗證結果

High detail、Normal game speed 的完整建置結果：

- 主回歸：PASS，telemetry schema 28。
- 鏡頭實測範圍：X `24..48`、Y `0..24`。
- 音訊包絡：transition `1`、fade-out `18`、silent `1`、fade-in `30`。
- 轉場 missed VBlank：`0`。
- Episode 2 完整關卡 missed VBlank：`31 / 10475` display frames，
  且全數位於 gameplay。
- Sprite2 decode failure／cache drop／ROMFS failure：皆為 `0`。
- Episode 1 主流程、Episode 2/3/4 第一關、Arcade、四關 campaign、
  死亡、勝利、Demo、Jukebox、62 關 ROMFS matrix 與靜態選單壓力測試：
  全部通過。

正式 ROM：

- 大小：26,237,068 bytes（32 MiB 的 78.19%）
- SHA-256：
  `b48eb6c5bbc921ce304b3e763ac0016b5378e86f28add828d8a05dd2128c1c08`

