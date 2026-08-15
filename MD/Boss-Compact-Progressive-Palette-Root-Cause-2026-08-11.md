# Episode 4 LVL 11 小魔王灰色機身 Root Cause

日期：2026-08-11
狀態：已修復並完成針對性回歸

## 重現定位

- 使用 `build/TyrianGBA.sav` 的第 1 格：Episode 4、`mainSection=7`。
- 正式路線解析結果為 Episode 4、LVL 11。
- 第一段 Boss bar 在 `level_position=241` 啟動；紅框小魔王是同一關稍後大型 Boss 前的多組件 Boss。
- LOW、無開火、正式 release cache 配置可穩定重現。

## 已排除項目

- 不是原始 `newsh*.shp`／Sprite2 資料本來就是灰色。
- 不是 Detail Level 的濾色效果。
- 不是 VBlank 後，爆炸或 GAME OVER 的 palette bank 把 Boss palette 蓋回灰色。
- 不是 OAM 超限造成缺件後的殘影。

同一路線改走原始 8bpp Sprite2 呈現後，來源圖仍保有完整的多色索引；只有啟用 release Boss compact 4bpp 路徑時會整體偏灰。修復後的完整 cohort palette 則恢復本段機身應有的綠色／深色層次。

## Root Cause

Boss compact 把每張 32×32 Sprite2 從 8bpp 壓成 4bpp，並共用兩組各 15 色的 runtime palette。近期為避免大型 Boss 在正式 cache 中先發生 16～19 幀 churn，compact 已改成「Boss 一確認就立即啟用」。但是 palette 仍沿用舊生命週期：**只在第一次啟用時訓練一次，之後永久鎖定到 Boss 結束。**

本關的直接遙測證據：

| 指標 | 數值 |
|---|---:|
| palette 首次訓練位置 | 190 |
| 首次訓練可見敵人／Boss／32×32 | 4／4／4 |
| 後續可見 Boss 32×32 最大值 | 15 |
| 首次 palette 實際選出的有效來源色 | 7 色 |
| 首次來源索引 | `10, 11, 12, 13, 1A, 1B, 81`（hex） |

也就是說，位置 190 時最先出現的 4 個深灰／岩色組件先把兩組 palette 鎖死；位置 200 之後才進場的主機身組件，其棕紅色沒有進過訓練，只能被 nearest-colour 映射到既有灰階。硬體 palette 讀回值和訓練輸出逐項一致，因此可以確定是「訓練樣本過早且不完整」，不是後續 palette 汙染。

## 最終修復

初版曾嘗試在 Boss assembly 擴張後動態重訓，但即使做 debounce，重訓仍必須讓所有既存 4bpp tile 失效並重包，單幀最高約 245 萬 cycles，音訊視窗損失升到 15／473（3.17%）。因此沒有採用這個版本。

正式修法保留「Boss 確認後立即啟用 compact」，並把完整樣本準備移到關卡載入階段：

1. `build_boss_manifest.py` 在既有 `(Episode, LVL, event index)` Boss spawn manifest 追加 cohort ID；這仍是由全部原始 LVL 自動推導的通用資料，不是特定關卡例外表。
2. 關卡畫面尚未開啟、背景音樂尚未 fade-in 前，runtime 逐一讀取該關 Boss cohort 的原始 LVL event、HDT enemy definition 與無損 Sprite2 raw frame。
3. 依完整的 20-frame enemy graphics 定義訓練兩組 15 色 compact palette，只暫存每個 cohort 的 30 個來源色索引。
4. Boss 第一次出現時直接安裝對應 cohort palette；不需要在戰鬥中重新解碼素材、換盤或讓 compact cache 整批失效。
5. 每個 enemy pool instance 保留 manifest cohort 身分，slot 釋放／重用時同步清除，避免跨 Boss 或 recycled slot 汙染。
6. 動態 fallback 只保留給 manifest 外的新 phase；暫時性的受擊 `filter` 不會觸發 palette 重訓。

## 驗證結果

測試條件：LOW、Episode 4 Section 7（實際 LVL 11）、正式 release cache／SAV 裝備、Boss 視窗 60 VBlank、無開火。

| 指標 | 灰色舊版 | 動態重訓嘗試 | 最終 cohort 預訓練 |
|---|---:|---:|---:|
| compact palette 戰鬥中重訓 | 0 | 2～3 | **0** |
| 音訊視窗損失 | 3／473 | 15～19／473 | **3／473（0.63%）** |
| missed VBlank | 42 | 55～58 | **43** |
| enemy cache drop | 0 | 0 | **0** |
| Sprite2 L2 drop | 0 | 0 | **0** |
| 最大可見 Boss 32×32 組件 | 15 | 15 | **15** |

另以相同 SAV 高負荷武器持續開火，截圖確認機身恢復綠色／深色層次，且 `boss_compact_palette_retrains=0`。因此修復沒有用停用 compact cache 換取顏色，也沒有把原本的 Boss cache 效能問題帶回來。
