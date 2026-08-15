# Boss 持續受擊卡頓：修復與實測（2026-08-11）

## 結論

這不是 OAM 用盡或圖形快取汙染，而是玩家高負荷武器持續命中大型多組件
Boss 時，碰撞、子彈更新、命中特效與 projectile render 同時突破
ARM7TDMI 的單幀 CPU 預算。問題包含 GBA 的客觀算力上限，但仍有可避免的
軟體成本；本次在不降低遊戲邏輯 FPS、不改命中／傷害／生成順序的前提下，
降低了約 6% 的碰撞及每完成畫面繪製成本。

## Gemini 3.1 Pro 諮詢與實測篩選

將同一關卡的有／無開火 telemetry、資料結構、PC parity 限制及 IWRAM
預算交給 Gemini 3.1 Pro。它建議空間 broad phase、Boss group AABB、
projectile MRU、一般子彈快速路徑及 pool allocator 等方向。

建議僅作為候選，實際以相同 Episode 4／LVL 11 Boss 視窗驗證：

- 16 個水平 bucket：候選走訪 -57%，碰撞 cycles 卻 +10.5%，撤回。
- C 語言一般子彈快速路徑：player-shot update cycles +14%，撤回。
- 8-entry direct-map render memo：沒有優於 1-entry MRU，撤回。
- free-list：現有配置器已使用 bitset，不是本次瓶頸，未重複實作。
- group AABB：大量子彈正對巨大 Boss 時無法有效拒絕，未採用。
- 1-entry projectile MRU：安全且有小幅收益，保留。

## 保留的通用修正

### 1. Projectile 同幀 MRU

同一 volley 的相鄰子彈經常使用相同 `(shape_table, graphic)`。renderer
保留一筆當幀 MRU，連續命中時直接重用已取得的 tile；每幀重新初始化，
不跨幀保存 ownership，因此不會引入 cache stale 或污染。

### 2. 將命中結果套用移出未命中熱路徑

`source_apply_player_shot_result()` 改為 `noinline,noclone`，主碰撞迴圈
只在 `result.collided` 時呼叫。這使常見 miss 路徑縮短，避免每顆子彈都
攜帶命中、爆炸、音效與 SuperPixel 處理的巨大 cold body。

### 3. 三筆連續 MT RNG 批次 wrapper

PC 的 `JE_doSP()` 每個 SuperPixel 固定依序取三次共用 MT19937。
`ot_level_port_random3()` 仍連續呼叫同一個 ARM RNG core 三次，只把三次
C wrapper／counter 更新合成一次：輸出順序、state 演進及
`rng_call_count` 均不變。

### 4. 壓力時只限制命中火花的呈現量

Boss 的 `armorleft == 255` 命中會依 PC 規則大量生成 SuperPixel，101 格
ring 很快接近滿載；每個粒子在 BG3 最多寫五個像素。當玩家同時存在超過
48 顆子彈、且 SuperPixel 超過 48 顆時，仍完整更新全部粒子狀態、生命週期
與 RNG，只限制該畫面最多呈現 48 顆。這符合既有「高負荷時先犧牲低優先
命中火花」策略，不會改變 Boss、玩家子彈、命中、傷害、掉落或關卡流程。

## 相同 Boss 視窗量測

條件：LOW detail、Episode 4／Section 7（LVL 11）、持續開火、Boss
100 VBlank／57 logic ticks。

| 指標 | 修復前 | 修復後 | 差異 |
|---|---:|---:|---:|
| missed VBlank | 90 | 83 | -7.8% |
| logic cycles | 7,970,046 | 7,812,033 | -2.0% |
| collision cycles | 2,661,919 | 2,505,650 | -5.9% |
| render cycles／完成畫面 | 503,548 | 471,331 | -6.4% |
| 完成 render 數 | 22 | 24 | +9.1% |
| 碰撞候選／命中／子彈生成 | 3,900／274／538 | 3,900／274／538 | 完全一致 |
| RNG 呼叫總數 | 5,014 | 5,014 | 完全一致 |
| 最高 OAM／場景需求 | 90／112 | 90／112 | 完全一致 |
| projectile／Sprite2 cache drop | 0／0 | 0／0 | 無配置失敗 |

單次 emulator telemetry 仍受 Windows 排程與快取暖機影響，因此 missed
VBlank 的改善應視為量級而非固定保證；但 collision、正規化 render 成本及
遊戲結果計數共同證明保留修改有正收益，且沒有拿正確性換數字。

## 驗證

- packed collision／axis overlap／色距等既有 differential tests 全部通過。
- 命中數、候選數、生成數、RNG 呼叫數與 OAM 峰值一致。
- 沒有新增 OAM、projectile cache 或 Sprite2 L2 配置失敗。
- 被實測證明退化的實驗程式均已撤回，正式路徑不留 runtime dispatch。
