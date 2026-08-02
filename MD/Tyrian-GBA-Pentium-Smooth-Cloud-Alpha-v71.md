# PENTIUM 平滑雲層 Alpha（v71）

日期：2026-08-02

## 問題根因

先前 PENTIUM 的 `wild` 雲層不是 GBA 預設透明效果。移植層為了同時
保留玩家／子彈的 OBJ-window 陰影，會在 BG1 tile cache 中清除交錯的
4-bit pixel，形成 50% 棋盤網格。這保留了半透明的亮度比例，卻無法
呈現 PC 版逐像素平均的平滑觀感。

## Gemini 3.1 Pro 諮詢與採用方案

諮詢結論建議使用 GBA 單一 colour-effects unit：PC 的 BG2 對應 GBA
BG1，將 BG1 設為 first target、其下所有可見層設為 second target，並
使用 `BLDALPHA = 8:8` 做硬體 50/50 Alpha。

本專案採用混合策略：

- 一般 PENTIUM `wild` 雲層固定使用硬體 8:8 Alpha。
- 雲層期間不送出會搶占同一 colour-effects unit 的玩家／子彈
  OBJ-window 陰影；透明雲層的主要 PC parity 優先。
- 只有 PC 聚光效果需要另一種同 scanline 全畫面效果時，才原子切回
  原棋盤 fallback。
- `wild` 與 `wild_dither` 分開計數，避免「啟用透明」被誤記成
  「實際退化」。

未採用 HBlank 重寫 `BLDCNT`：DMA0 已供水波位移使用，而且同一像素仍
無法同時執行兩種 colour-effect mode，複雜度與效果不成比例。

## 驗證

測試路線：Episode 1、Section 1、position 600，PENTIUM、無敵與完整
壓力武器。

- `detail_wild_frames`: 867
- `detail_wild_dither_frames`: 0
- `detail_adapter_self_test`: 1
- `source_assets_valid`: 1
- `audio_frame_loss_percent`: 0.097%

position 120 的 240x160 截圖也確認雲層為連續平滑混色，不再有前景
破洞網格。聚光衝突的 fallback 仍保留為正確性保險，並能由 telemetry
明確觀察。
