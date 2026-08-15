# Camanis Boss／LOW compact cache 與碰撞修復報告

日期：2026-08-11
對象：Episode 3、Section 9 → Camanis（EP3 LVL 1）
測試：LOW detail、正式版 cache 配置、目前 SAV 的真實裝備，同一 Boss 視窗 A/B。

## 已完成修復

1. **Boss compact cache 改為正式版固定機制**
   - 移除原本「大型 32×32 Sprite 必須塞滿 23 格」才啟動的行為門檻。
   - manifest／Boss bar 一確認 Boss，且畫面已有可 compact 的大型 Boss 組件，就立即啟用。
   - 不受 LOW／NORMAL／CUSTOM／HIGH／PENTIUM detail level 影響，也沒有提供玩家或 release build 關閉選項。
   - 原門檻只留下 telemetry 指標，完全不再控制執行行為。

2. **相同 Boss link 的命中狀態只傳播一次**
   - 同一 collision phase 內，相同 `(link, blast filter, iced)` 不再每顆子彈重掃整組 Boss。
   - 直接命中的組件仍會逐次更新；敵人生成、釋放或 damaged transition 會立刻讓快取失效，因此保留 PC 原始碰撞／狀態語意。

3. **加入保守式 collision broad phase**
   - 每個 collision phase 由原本的精確 enemy snapshot 建立全體可碰撞範圍。
   - 子彈矩形完全不可能碰到任何敵人時，直接回報 miss，不進入 ARM packed candidate scan。
   - 邊界採保守包含規則；敵人池或幾何一變動就停用該 phase 的快速拒絕，不會漏判新生成／變形物件。

4. **建置時保護測試 SAV**
   - `tools/build_release.ps1` 現在會在清理 `build` 前暫存 `TyrianGBA.sav`，ROM 完成後逐位元還原，避免往後建置再清掉人工測試進度。

## 持續開火 A/B

| LOW release-runtime Boss 視窗 | 修復前 | 修復後 | 差異 |
|---|---:|---:|---:|
| Boss logic ticks | 82 | 82 | 相同 |
| missed VBlank／wall VBlank | 105／140 | 84／140 | 75% → 60% |
| logic cycles／tick | 180,355.7 | 119,695.7 | **-33.6%** |
| collision cycles／tick | 95,631.8 | 35,068.2 | **-63.3%** |
| collision candidates | 50,398 | 2,696 | **-94.6%** |
| linked-status visits | 12,075 | 1,863 | **-84.6%** |
| completed render 平均 cycles | 389,980.0 | 374,771.0 | -3.9% |
| 最高 OAM | 93 | 91 | 非 OAM 128 上限問題 |

這是刻意使用 `FastForwardTicks=8`、完整 telemetry 與真實高負荷裝備的診斷 ROM；其絕對 missed-VBlank 比率比一般 release 遊玩嚴苛。可比較的重點是同條件前後：碰撞成本已下降約 63%，而 Boss 仍在相同 82 個 logic ticks 完成。

## 不開火基準

| LOW release-runtime、800 display frames | 修復前 | 修復後 |
|---|---:|---:|
| missed VBlank／wall VBlank | 10／799（1.25%） | 8／799（1.00%） |
| audio frames／wall VBlank | 799／799 | 799／799 |
| compact context／active frames | 483／464 | 472／472 |

固定啟用後，Boss context 不再先經過 16～19 個一般 cache 畫面；不開火的穩定負荷沒有退化，音訊也保持完整。

## 驗證結果

- ARM packed collision differential：`3`（完整通過）。
- hotpath ASM self-test：`1`（通過）。
- Source asset validation：通過。
- LOW 正式 ROM 建置成功，26.75 MiB。
- ROM：`build/TyrianGBA.gba`
- SHA-256：`2041802ee83dca4a701f013ceef5df7c70d16eb079f8002e3a21a405d8e1b982`
- 測試 SAV 已恢復；SHA-256：`7575eac2c309444733c39562df8720e4023c1d6ff13b2aee8166f9fc3e7e4cb9`

## 結論

LOW 原本就有 Boss dispatch；這次把 compact cache 從「容量滿才啟動」提升為「確認 Boss 就固定啟動」，並處理真正的攻擊熱點。高負荷火力仍可能讓 GBA 的 adaptive/drop-frame 介入，但已不是因為 Boss 漏走 compact，也不再為畫面外子彈與相同 link 狀態支付大量重複掃描成本。
