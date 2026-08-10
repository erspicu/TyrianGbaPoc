# OAM 結構物優先與公平輪替修正

日期：2026-08-08  
測試路徑：Episode 1／Section 37（LVL 17）  
條件：PENTIUM、無敵、全武器壓力配置、持續射擊、600 VBlank

## Root cause

GBA 硬體最多只有 128 個 OBJ／OAM entry。修正前的壓力版本略過了
結構物 OAM 保留，而且反向提交 PC 畫面順序時，爆炸與子彈會先取得 OAM；
稍後才提交的敵人因此可能完全沒有 OAM 可用。

另一個相互加重的問題是：關卡敵人與 `pickup_explosions` 共用 Sprite2
L1 VRAM 快取。爆炸圖幀若先取得並 pin 住 18 格壓力版快取，真正敵人即使
還活在 game state 中，也可能無法取得圖幀，形成「可碰撞但透明」的現象。

這是呈現資源排程優先級錯誤，不是敵人 game state 被刪除，也不是單純無法
迴避的 GBA 規格限制。

## 修正策略

資源優先級固定為：

1. 主角、敵人與 Boss 等結構物。
2. 具有玩法意義的敵我子彈。
3. 爆炸、短命效果與子彈陰影。

每次 render 先收集可見敵人，Boss bar link 對應的 Boss components 最先取得
admission，接著才是一般敵人；選中的 Sprite2 圖幀會先鎖入共享快取，之後
才允許爆炸使用餘下快取。

Gemini 3.1 Pro 技術審查指出：GBA 即使全域 OAM 尚未超過 128，同一掃描線的
OBJ rendering budget 仍可能超載，硬體會優先捨棄較後 OAM index。因此超載
場景不再先建立爆炸後搬動整張 OAM，而是直接按下列順序產生：

1. HUD、玩家、Boss／敵人與主要敵我子彈先取得較前 OAM index。
2. 預留最多 8 entries 作為低 duty-cycle cosmetic window。
3. 一般 effect 與 pickup explosion 在尾端 OAM 各自 round-robin；前者最多先用
   半個 window，後者可借用未用額度。

這個排程是 work-conserving：若高優先物件未用滿，爆炸可取得全部餘額；若
高優先物件很多，爆炸至少保留很小且刻意較閃的展示窗口。若結構物本身已經
占滿硬體容量，玩法正確性仍優先，此時 cosmetic window 可以縮到零。

若結構性敵人本身已超過剩餘 OAM 或 Sprite2 L1 容量，系統會按實際呈現幀
輪替 admission window。這不改變敵人更新、碰撞或 PC 原始 pool 順序；只讓
超額物件公平閃爍，避免固定索引長時間完全消失。

## 量測結果

| 指標 | 修正前 | 結構優先 v1 | 最終帶權輪替 |
|---|---:|---:|---:|
| 實際 OAM 峰值 | 128 / 128 | 125 / 128 | 128 / 128 |
| 場景來源端瞬間 OAM 需求峰值 | 未記錄 | 223 | 223 |
| 敵人 OAM 裁切 | 未細分 | 0 | 0 |
| 敵人預鎖快取失敗 | 未細分 | 0 / 228 | 0 / 228 |
| 爆炸／效果 OAM 裁切 | 未細分 | 175 | 149 |
| 玩家子彈 OAM 裁切 | 未細分 | 50 | 1 |
| 敵人／爆炸共享 L1 drop | 283 | 125 | 66（均為低優先爆炸競爭） |
| 爆炸 pool 輪替 | 無 | 無 | 193 pool-frames／99 壓力 render |
| Gameplay OBJ 前置 | 無 | 無 | 99 壓力 render |
| Missed VBlank | 137 | 148 | 156 |
| 完成 render | 182 | 178 | 175 |
| 音訊更新 | 600 / 600 | 600 / 600 | 600 / 600 |

無射擊基準的 OAM 峰值只有 80；即使關閉玩家子彈碰撞、讓敵人持續存在，
全武器持續射擊仍會到 128，證明主要瓶頸是呈現資源飽和，而非擊殺流程。

本次目標場景的結構需求峰值只有 7，因此不需要啟動敵人輪替；輪替是針對
未來真正連結構物都超過硬體容量的最後保險。最終版保留全部敵人與幾乎全部
主要子彈，爆炸仍以較低 duty cycle 出現。相對原始壓力版，600 VBlank 內
missed VBlank 增加 19、完成 render 減少 7；但邏輯更新同為 349，音訊保持
600/600，沒有掉音。這是以少量 presentation frame 換取玩法物件可靠可見與
公平輪替的取捨，符合專案既有 adaptive/drop-frame 原則。

## 證據檔

- [壓力測試截圖](Evidence/OAM-Weighted-Rotation-Section37-2026-08-08.png)
- [完整 telemetry](Evidence/OAM-Weighted-Rotation-Section37-2026-08-08.json)
