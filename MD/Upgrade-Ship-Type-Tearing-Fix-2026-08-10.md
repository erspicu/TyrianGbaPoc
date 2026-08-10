# Upgrade Ship／Ship Type 快速切換撕裂修復

日期：2026-08-10

## 修復內容

1. 將 Upgrade 武器 simulator 的 authoritative live flag 移出共享 cold
   arena，避免 Ship preview、Data、Ship Specs 資料覆蓋生命週期狀態。
2. simulator update、OAM、SFX、loadout dirty 與 VBlank DMA 全部改為同時
   檢查外部 live flag 與 simulator category；非 simulator 頁完全不得
   讀寫 aliased arena。
3. Ship Type 完整畫面先在主迴圈 DMA 到隱藏 Mode-4 page；DMA 完成後才
   armed pending flip，下一個 VBlank 只更新 palette 與 page bit。
   即使完整上傳跨過某次 VBlank，LCD 仍保留舊完整頁，不會顯示半頁。
4. prepared page 尚未翻上螢幕時，保存最後一次按鍵並於安全翻頁後重播，
   避免快速輸入破壞正在上傳的 scratch frame。
5. 非 simulator Upgrade 頁的游標音效改為 cancel/replace 單一 handle，
   防止快速切換時累積 mixer voices。

## Ship Type 壓力驗證

條件：CUSTOM／Normal、現有 SAV、連續上下切換 600 次。

- 測試 schema：`TGUH`，成功。
- Ship preview cache：598 hits、2 cold misses、598 swaps。
- 錯誤 simulator active：修復前 26，修復後 **0**。
- prepared page flips：600。
- 可見掃描期間翻頁：**0**。
- 翻頁 VCOUNT max／last：161／161（VBlank 從 160 開始）。
- missed VBlank：5／660 physical VBlanks。
- 音樂狀態：active。
- audio input max：57,367 cycles。
- 文字 recolour differential test：通過。

## 真正 simulator 回歸

Left Sidekick + Right Sidekick 各 300 次、合計 600 次：

- schema `TGUS`，成功。
- missed VBlank：12／775。
- loadout refreshes：2。
- active shots max：32。
- OAM max：73。
- 音樂持續 active。
- 結束後 simulator active 正常回到 0，未重設、未當機。

結論：Ship Type 不再把快取像素當作 simulator 狀態；所有 600 次頁面
切換均在 VBlank 的第 161 掃描線完成原子翻頁，時間域撕裂路徑已封閉。

## 正式建置

- Detail：CUSTOM
- Game Speed：Normal
- ROM：`build/TyrianGBA.gba`
- 大小：28,053,276 bytes（約 26.75 MiB）
- SHA-256：`423c3ea7e993d3b4e8eb0e1954fdd6f082d92fab71cff86d280deeb595832537`
- 600-frame mGBA production smoke test：通過。
- 原有 `build/TyrianGBA.sav` 已還原，SHA-256 與建置前完全一致。
