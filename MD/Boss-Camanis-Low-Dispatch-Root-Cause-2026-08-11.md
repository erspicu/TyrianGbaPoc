# Camanis Boss／LOW 模式效能 Root Cause

日期：2026-08-11
測試對象：目前 `build/TyrianGBA.sav` 對應的 Episode 3、Section 9 → Camanis（EP3 LVL 1）
測試方式：LOW detail、正式版裝備資料、同一 Boss 視窗，分別比較持續開火與完全不開火。

## 結論

LOW 模式確實有進入既有 Boss compact cache；問題不是 LOW 漏掉 dispatch，也不是 OAM 128 格耗盡。真正瓶頸由兩部分疊加：

1. compact cache 啟動過晚：現行邏輯必須等 32×32 大型 Sprite 數量達到 23 才啟動。Camanis 已被 Boss manifest 正確識別，但前 16～19 個 Boss context 畫面仍走一般 8bpp cache，形成切換尖峰。
2. 持續攻擊大型多組件 Boss 時，碰撞與命中狀態傳播成為主要 CPU 成本。Camanis 的 23 個組件共用同一 link；每次命中都重新掃描全部 23 個 linked enemy，使 525 次命中產生 12,075 次重複狀態走訪。

因此「大型 Boss 被打中時卡頓」主要是碰撞／命中熱路徑與 compact cache 延遲啟動，不是 Boss 沒被辨識，也不是單純畫面特效或 OAM 上限。

## 實測證據

| LOW release-runtime | 不開火 | 持續開火 |
|---|---:|---:|
| Boss wall VBlank | 799 | 140 |
| missed VBlank | 10（1.25%） | 105（75.00%） |
| logic cycles／tick | 64,680.8 | 180,355.7 |
| collision cycles／tick | 3,417.5 | 95,631.8 |
| completed render 平均 cycles | 91,840.4 | 389,980.0 |
| collision candidates | 0 | 50,398 |
| Boss 命中次數 | 0 | 525 |
| linked-status visits | 0 | 12,075（正好 525 × 23） |
| compact context frames | 483 | 58 |
| compact active frames | 464 | 42 |
| compact 啟動延遲 | 19 frames | 16 frames |
| 最高 OAM | 68 | 93 |

持續開火時最高 OAM 只有 93，距離硬體 128 格仍有餘量，足以排除「OAM 滿載造成這次主要卡頓」的假設。

## LOW dispatch 檢查

`source_boss_compact_prepare()` 是關卡 scene render 的共通路徑，不依 detail level 編譯或呼叫；LOW／NORMAL／CUSTOM／HIGH／PENTIUM 都共用同一套 Boss compact cache。實測 LOW 也記錄到 1 次 activation 與 919 次 compact object render，證明 dispatch 正常，但既有門檻讓它太晚介入。

## 修復方向

1. 移除 `SOURCE_BOSS_COMPACT_TRIGGER_COUNT == 23` 的啟動門檻。只要 Boss manifest／Boss bar 已確認為 Boss context，且畫面存在可 compact 的大型 Boss 組件，就固定啟用；這會是正式版 hardcode 行為，不做玩家或 build option。
2. 在同一 collision phase 內快取 linked-status 的已套用狀態；相同 link、filter、iced 組合不再每顆子彈重掃 23 個組件。Boss pool 或 link 發生改動時立即失效，維持原始遊戲語意。
3. 用同一份 SAV、LOW、相同開火視窗做修復前後 A/B，只做必要的差分與建置檢查。

## 附註

本次新增的 `STRESS_RELEASE_RUNTIME` 只屬於測量工具，用來讓自動測試採用真實正式版裝備與正式 cache 配置；它不是遊戲功能選項，也不會讓 Boss compact cache 成為可關閉功能。
