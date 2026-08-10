# Upgrade Ship／Ship Type 快速切換撕裂根因（更正版）

日期：2026-08-10

## 最終結論

Definitive root cause 是 **Ship 預覽快取與武器預覽模擬器共用 cold
arena，但模擬器的 active 狀態也存放在同一段會被覆寫的 arena**。

Ship Type 不需要武器模擬器；然而 Ship 預覽影像寫入共用 arena 後，
影像中的某個 palette index 會覆蓋 `FrontendUpgradeSimArena.active`。
舊程式只讀這個被覆寫的 byte 判斷 simulator 是否啟用，因此把普通
畫素誤認成 active，開始執行不存在的武器模擬器。

## 直接證據

- 測試：CUSTOM／Normal、Ship Type 連續上下往返 600 次。
- Ship Type 頁不屬於 simulator category，`sim_active` 正確值必須是 0；
  實測卻是 **26**，此值正是共用 arena 中的影像資料。
- 幽靈 simulator 會把 Ship cache bytes 解讀為 pending flags、shot、
  sound handle 與 tile buffer，造成額外 OBJ／VRAM DMA、錯誤視窗狀態，
  並回寫污染 Ship 預覽快取。
- 前一輪量到完整提交最壞 90,615 cycles，超過約 83,776-cycle VBlank。
  將畫面 DMA 從 16-bit 改成 32-bit 後仍為 90,656 cycles，證明主要
  成本並不是完整頁 DMA，而是提交前被誤啟動的 simulator 工作。
- 這也完整解釋：最終 PNG 可以正常，但實際快速切換會間歇撕裂、
  震動；問題取決於快取畫素剛好寫成什麼值，所以帶有偶發性。

## 前一封信的更正

前一封信判定「完整頁 DMA 跨出 VBlank」是直接原因。VBlank 超時確實
存在，也確實會造成翻頁撕裂，但現在已證實它是幽靈 simulator 額外
DMA／OBJ 工作造成的**下游症狀**，不是完整頁 DMA 本身的根因。

## 修復設計

1. 把 simulator 的 authoritative live flag 移出共用 cold arena，放在
   不會被 Ship cache／Data／Ship Specs 覆蓋的獨立 EWRAM 狀態中。
2. simulator 的 update、OAM render、SFX、loadout dirty、VBlank commit
   全部同時檢查「live flag + simulator category」。
3. 非 simulator category 時不得讀寫／清空 aliased arena；否則清零
   動作本身也會破壞 Ship cache。
4. 重跑 600 次 Ship Type 往返，要求 `sim_active=0`、提交最壞值低於
   VBlank、無額外 OBJ simulator 工作，並確認音樂持續播放。
