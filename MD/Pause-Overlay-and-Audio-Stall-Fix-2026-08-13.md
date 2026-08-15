# PAUSED 文字消失與暫停音訊卡頓修復

日期：2026-08-13

## Root Cause

PAUSED 從會與 Boss compact palette 衝突的 OBJ 搬到 BG3 後，舊暫停流程仍在
每個 LCD frame 呼叫完整 `render_game()`。高負荷關卡的一次完整世界重畫可能
跨過 VBlank，主迴圈便優先回放 missed-VBlank recovery；BG3 的 PAUSED DMA
只能在真正安全的 VBlank 提交，因此可能長時間看不到文字。相同的無效重畫也
讓 Maxmod 的補充時點變得不規則，產生暫停後音訊卡頓感。

## 修正

- START 切換暫停狀態時，只重建一次小型 BG3 gameplay overlay。
- 暫停期間凍結既有背景與 OAM，不再重畫完整關卡。
- 進入暫停時丟棄已無意義的 presentation recovery backlog；下一圈等待真正
  VBlank，立即提交 PAUSED overlay。
- 音訊仍按實體 VBlank 更新，並依 OpenTyrian `JE_pauseGame()` 保持音樂半音量。
- 暫停期間停止背景預取。
- 移除舊 PAUSED OBJ 路徑留下的 enemy split-cache 保留與逐幀失效規則。

## 驗證

測試路線：LOW、Episode 4 Section 49（ICE EXIT），同一 deterministic input。

| 截點 | Display frames | VBlank IRQ | Missed VBlank | Audio frames | Paused frames |
|---|---:|---:|---:|---:|---:|
| 暫停前 | 119 | 289 | 14 | 116 | 0 |
| 暫停期間末端 | 179 | 350 | 14 | 176 | 59 |
| 暫停區間增量 | +60 | +61 | **+0** | **+60** | +59 |

修正後 PAUSED 可在進入暫停後的下一個安全 VBlank 顯示；暫停區間沒有新增
missed VBlank，且每個 LCD 期間都有對應音訊混音更新。

對照截圖：

- 修正前：`temp/pause_heavy_repro_before/pause_before.png`
- 修正後：`temp/pause_heavy_repro_after/pause_after_fast_commit.png`
