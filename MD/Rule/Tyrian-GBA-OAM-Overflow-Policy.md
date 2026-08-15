# TyrianGbaPoc OAM 超量處理規則

日期：2026-08-02
狀態：目前正式實作規則

## 核心結論

當一個畫面需要的 OBJ 超過 GBA 硬體 OAM 上限 128 筆時，專案採用：

> **保留完整遊戲邏輯，在顯示端依重要度與動態額度裁切。**

不會讓第 129 筆資料覆寫 OAM，也不會為了符合顯示上限而刪除敵人、
子彈、爆炸或碰撞邏輯。Drop Frame 處理的是 CPU／DMA 趕不上 VBlank
的問題，不能增加 OAM 容量，兩者必須分開看待。

## 1. 邏輯池與顯示池分離

關卡仍保留 OpenTyrian 對應規模的邏輯物件：

- 敵人物件：100。
- 玩家子彈：81。
- 敵方子彈：60。
- 爆炸／效果：200。

OAM 裁切只影響該顯示幀是否看得到物件，不影響：

- 物件移動與動畫生命週期。
- 碰撞判定與傷害。
- 擊殺、獎賞、關卡事件與 Boss 流程。
- 原始 RNG 與事件時序。

因此在極端 OAM 壓力下，可能發生「物件邏輯仍存在並可碰撞，但該幀
沒有顯示」的退化情況。這比直接刪除邏輯物件更能維持 PC 版規格，
但必須透過顯示優先權盡量避免重要子彈變成不可見。

程式依據：`main.c` 的 gameplay pool 與 presentation-only budget 定義。

## 2. 每幀 OAM 配置流程

### 2.1 先排入畫面最前方資訊

每幀會先配置需要保持可見的前景資訊，例如：

- GAME OVER、SECRET LEVEL、PAUSED 等系統提示。
- 金額及 SHIELD／ARMOR／GENERATOR 數值。
- Boss 血條。

這些項目先占用 OAM，後續世界物件只能使用剩餘容量。

### 2.2 動態計算結構性 OAM 保留量

接著掃描目前畫面內的結構性物件，預先保留 OAM：

- 可見敵人與 Boss 組件。
- 主角飛機。
- 主角陰影（細節等級允許時）。
- 左右側翼。
- 過關離場時的飛機軌跡。

一般建置中，一個 runtime Sprite2 可能因 VRAM 快取配置而拆成上下兩個
32x16 OBJ，因此敵人和側翼會保守地按最多兩筆 OAM 計算。未實際使用的
保留量會成為安全餘裕；它不會造成額外硬體物件。

程式依據：`src/gba_scene.inc` 的 `source_structural_oam_required()`。

### 2.3 非結構物件只能使用剩餘容量

目前原始碼預設 `TYRIAN_GBA_STRESS_LOADOUT=0`，單類顯示上限為：

| 類別 | 單幀顯示上限 |
|---|---:|
| 一般爆炸／效果 | 21 |
| 敵方子彈 | 32 |
| 玩家子彈 | 36 |

上述數字只是單類上限，不是保證數量，也不能直接相加成 89。HUD、Boss
血條及結構性物件越多，非結構物件能使用的總額度就越少。

目前剩餘額度大致依下列送出順序消耗：

1. 拾取／獎賞相關爆炸。
2. 一般爆炸與效果。
3. 敵方子彈。
4. 玩家子彈。

所以總 OAM 極度緊張時，較晚送出的玩家子彈目前較可能先被裁切。這是
現行實作的真實行為，不應誤寫成所有非結構物件具有相同存活優先權。

程式依據：`main.c` 的 `MAX_VISIBLE_*`，以及
`src/gba_scene.inc` 的 `render_source_play_scene()`。

## 3. 類別內的選擇規則

- 先剔除畫面外物件，並盡可能在 Sprite 快取查找前完成，避免浪費
  解碼、VRAM 快取與 VBlank DMA 成本。
- 爆炸效果由較新的 slot 開始送出，使較新的、PC 版較晚繪製的效果
  優先保留。
- 玩家與敵方子彈反向巡覽來源 slot，以配合「PC 後畫者覆蓋前畫者」
  與「GBA 同 priority 時較低 OAM index 優先」之間的差異。
- 敵人 pool 也採反向送出，以維持 PC 版同層物件的前後關係。

這些反向巡覽同時影響 OAM 滿載時哪些 slot 會留下，但它的首要目的仍是
保持 PC 版圖層順序。

## 4. 不允許半張 Sprite

如果一個 Sprite2 需要拆成兩筆 OAM，而當下只剩一筆容量，必須整個略過，
不能只顯示上半或下半。拾取爆炸與 enemy Sprite2 都有這項原子提交防護，
可避免產生半張圖或上下分離的破圖。

程式依據：`src/gba_scene.inc` 的
`render_source_enemy_cached_sprite()`、
`render_source_enemy_cached_sprite_blended()` 與
`render_source_pickup_explosions()`。

2026-08-11 補充：所有直接使用通用 Sprite2 L1 cache 的繪製路徑，都必須
透過上述 common renderer 解讀 tile ownership，不能只用
`source_enemy_cache_tile_is_compact()` 後把其餘 tile 一律視為連續 32×32。
`render_player_sidekicks()` 曾漏掉這條規則，使合法取得 split tile 160 的
Sidekick 把 projectile tiles 176..191 誤當下半圖；現已改為與 enemy 共用
ordinary／compact／split 三路呈現，AUTOTEST 亦會驗證 split 的 shadow 與
hardware OAM 上下半必須分別指向 tile 160／598。

## 5. 最後一道硬體安全限制

所有底層 `put_sprite*()` 函式都會先檢查：

```c
if (oam_count >= SPRITE_LIMIT) return;
```

其中 `SPRITE_LIMIT == HARDWARE_OAM_ENTRIES == 128`。即使上層預算計算
發生遺漏，第 129 筆也不會寫出 `oam_shadow` 範圍。

新畫面比上一幀使用更少 OAM 時，尾端舊項目會設成 `ATTR0_DISABLED`，
避免殘留 Sprite 或鬼影；完整 shadow OAM 只在 VBlank 期間 DMA 到硬體。

程式依據：`src/gba_oam.inc`、`src/gba_scene.inc::render_game()` 與
`src/gba_platform.inc` 的 VBlank commit。

## 6. Drop Frame 的責任邊界

`TYRIAN_GBA_DYNAMIC_FRAME_DROP` 目前預設開啟。若 CPU、快取上傳或背景
串流無法安全趕上下一次 VBlank，呈現排程器會保留上一張完整畫面，遊戲
邏輯仍依 LCD 經過時間前進；不會送出只有部分 OAM／VRAM 更新的新畫面。

但 Drop Frame：

- 不會把 128 筆 OAM 變多。
- 不會替代上述 OAM 分級與裁切。
- 不代表 OAM 超量時必然掉幀；若運算時間足夠，仍只做顯示端裁切。

程式依據：`Configure.h` 的 `TYRIAN_GBA_DYNAMIC_FRAME_DROP` 說明與
`src/main_loop.inc` 的 presentation scheduler。

## 7. 已知硬體邊界

上述規則主要處理「整個畫面共 128 筆 OAM」的總量限制。GBA 另外還有
同一條掃描線上的 OBJ 評估／繪製時間限制。目前沒有實作逐掃描線的
sprite binning 或動態 scanline budget；因此即使總 OAM 未達 128，若
大量大型 OBJ 集中在同一水平區域，仍可能出現硬體層級的局部缺圖。

若未來要繼續改善，應把「總 OAM 裁切」與「逐掃描線 OBJ 負載」分成
兩組 telemetry 和排程策略，不能只觀察 `telemetry_max_oam`。

## 8. 現有量測指標

目前已有下列 telemetry 可用來判斷 OAM 壓力來源：

- `telemetry_max_oam`
- `telemetry_structural_oam_required_max`
- `telemetry_effect_oam_culls`
- `telemetry_enemy_shot_oam_culls`
- `telemetry_player_shot_oam_culls`
- `telemetry_projectile_culled_oam_full_before_cache`

分析破圖或物件消失時，應先判斷是：

1. 總 OAM 額度裁切。
2. 同掃描線 OBJ 硬體限制。
3. Sprite2／projectile VRAM cache miss 或 upload drop。
4. VBlank deadline 導致保留上一張完整畫面。

這四種現象不能只用「OAM 爆滿」概括，修正策略也不同。
