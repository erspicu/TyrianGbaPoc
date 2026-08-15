# Boss 持續受擊卡頓：Root Cause（2026-08-11）

## 結論

EP4／LVL 11 的 Boss 在我方持續開火時卡頓，主因不是 128 OAM 用盡，也不是 Sprite2／projectile cache 汙染，而是高負荷武器讓大量玩家子彈同時存在並反覆命中多組件 Boss，令下列 CPU 工作在同一邏輯 tick 疊加：

1. 每顆有效子彈對 Boss 組件做碰撞候選掃描與精確 AABB 判定。
2. 每次命中套用護甲、連結狀態、命中特效與音效語意。
3. 81 格玩家子彈池接近滿載時的移動／生命週期更新。
4. 每個完成畫面對可見子彈做候選收集、圖形快取取得與 OAM 輸出。

這些工作使 ARM7TDMI 的 CPU 預算被突破；現有 adaptive/drop-frame 正確地保住遊戲時間軸，但只能捨棄部分呈現幀，不能消除上述工作本身。

## 公平 A/B 條件

- 同一關：Episode 4、Section 7，實際載入 LVL 11。
- LOW detail、正式 runtime 武器／音效路徑。
- 同一 Boss 視窗：100 VBlank、57 次邏輯更新。
- 唯一差異：持續開火或完全不開火。

| Boss 視窗 | 不開火 | 持續開火 | 增量 |
|---|---:|---:|---:|
| missed VBlank | 11 | 90 | +79 |
| 邏輯 cycles | 3,416,875 | 7,970,046 | +4,553,171 |
| 碰撞 cycles | 194,821 | 2,661,919 | +2,467,098 |
| 子彈更新 cycles | 14,649 | 1,161,081 | +1,146,432 |
| render cycles | 4,882,729 | 11,078,049 | +6,195,320 |
| 碰撞候選走訪 | 0 | 3,900 | +3,900 |
| Boss 命中 | 0 | 274 | +274 |
| 玩家子彈生成 | 0 | 538 | +538 |
| 音訊完成幀 | 98/99 | 80/99 | -18 |
| 最高實際 OAM | 54 | 90 | +36 |
| 最高場景 OAM 需求 | 73 | 112 | +39 |

換算每邏輯 tick，持續開火額外增加約：

- 碰撞：43.3k cycles；
- 玩家子彈更新：20.1k cycles；
- 其他邏輯／命中特效等：16.5k cycles；
- render：108.7k cycles（以邏輯 tick 正規化）。

## 排除項目

- GBA 硬體 OAM 上限是 128；本次最高實際使用 90、估算需求 112，沒有撞上硬上限。
- 玩家子彈 OAM 僅輪替裁切 22 次，Boss／敵人並非因 OAM 排程而消失。
- projectile cache drop = 0、Sprite2 L2 drop = 0，沒有資源配置失敗。
- 不開火時同一 Boss 只有 11 missed VBlank，證明關卡與 Boss 本體不是單獨就超出硬體能力。

## 程式級根因

目前碰撞核心雖已是 ARM/IWRAM、active mask、8-byte snapshot 與全域 bounds 快速拒絕，但 bounds 只描述「全部敵人的聯集」。巨大多組件 Boss 橫跨大片畫面後，大量子彈都會通過這層聯集檢查，接著仍依 slot 順序逐一測試多個 Boss 組件。這就是持續命中時 3,900 次候選走訪與 274 次 hit apply 的來源。

render 端則必須處理最高約 76 顆同時存在的子彈；即使快取沒有失敗，候選收集、快取查找、OAM 寫入與 Boss 圖形處理仍是實際 CPU 成本。因此這是「硬體預算有限 + 尚可避免的 O(N) 重複工作」共同造成，不應把它簡化成無法修的 GBA 規格上限。

## 下一步

優先採用不改變 PC 遊戲結果的通用改善：

1. 在每個碰撞 phase 建立水平空間 bucket，以子彈覆蓋區間先產生候選 bitmask，再交給既有 source-order ARM 精確判定。
2. 穿透彈命中並造成 pool mutation 後，自動退回 live active mask，維持死亡生成物與 slot 順序語意。
3. 對新 spatial ARM 路徑做既有固定案例及 randomized differential test。
4. render 端加入每幀小型 projectile tile memo，避免同一圖形在同幀反覆進完整 cache acquire 路徑。
5. 用相同 Boss 100-VBlank 視窗重測，不以降低遊戲 FPS 或省略命中規格換取數字。

## 後續實測更正

上述水平 bucket 是 root-cause 階段的候選方案，不是最後採用的實作。
完成版本以相同 Boss 視窗 A/B 後發現，16 個水平 bucket 雖把候選走訪
由 3,900 降到 1,675（-57%），但建表、合併 bitmask 與 EWRAM 存取令
碰撞成本由 2.662M 增至 2.941M cycles，missed VBlank 亦由 90 增至
96，因此已完整撤回。最後採用的方案與量測結果見
`Boss-Continuous-Fire-Slowdown-Fix-2026-08-11.md`。
