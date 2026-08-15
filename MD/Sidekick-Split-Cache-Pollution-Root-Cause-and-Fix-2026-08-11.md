# Sidekick fragmented Sprite2 cache 污染：Root Cause 與修復

日期：2026-08-11
測試來源：`build/TyrianGBA.sav`，Episode 4 `SURFACE` 第一組小魔王

## 結論

小魔王雷電附近看似「玩家飛機 tile 被污染」的物件，實際是左右
Mini-Missile Sidekick。雷電與 Boss 圖形沒有直接覆寫 player cache；它們
提高了 Sprite2 L1 壓力，使 Sidekick 合法取得 fragmented split slot，進而
觸發 Sidekick renderer 漏掉的 split 呈現分支。

定點取證值：

- Sidekick source graphic：左右皆為 182，來源資料正確。
- cache tile：左右皆為 160。
- shadow／hardware OAM attr2：左右皆為 2208，低 10-bit 均為 tile 160。
- 問題幀 OAM 高峰：116／128，enemy OAM cull 為 0。

因此不是 128 OAM 超量、SHP 解碼、exact lookup 或 shadow／hardware OAM
不同步。

## 失效鏈

split slot 的 32×32 8bpp canvas 刻意分散在：

- 上半 32×16：OBJ tiles 160..175。
- 下半 32×16：OBJ tiles 598..613。

通用 enemy renderer 會用兩個 32×16 OBJ 原子提交並拼回完整畫面；舊的
`render_player_sidekicks()` 卻把任何非 compact tile 都當成連續 32×32。
因此從 tile 160 連續讀到 191：正確上半之後，誤把另一個 projectile cache
的 tiles 176..191 當成下半，形成兩側相同的藍黑大型錯圖。

## 修復

Sidekick 不再自行分流 ordinary／compact，而是統一呼叫
`render_source_enemy_cached_sprite()`：

- ordinary：一個 32×32 OBJ；
- compact：一個 16×16 OBJ，維持既有 +8 canvas 補償；
- split：tile 160 與 tile 598 各一個 32×16 OBJ，容量不足時整張略過。

這同時涵蓋 style 0 的單 component Sidekick 與可能取得 split slot 的
size-one Sidekick；武器邏輯、Boss compact cache、原始圖形與優先權均未改。

## 針對性回歸

同一路線、同一火力與相同停止點，修復後仍刻意取得 tile 160：

- 左／右 source graphic：182／182。
- 左／右 cache tile：160／160。
- 左右各送出兩筆 OAM；第二筆相對第一筆 +1。
- shadow／hardware 上半 tile：160；下半 tile：598。
- `sidekick_split_layout_pass=1`。
- Boss compact activation：1；compact objects 正常送出，沒有退回舊策略。
- 截圖中左右 Mini-Missile 恢復為正確灰紅色小型飛彈。

回歸 telemetry 僅存在 AUTOTEST build，正式 ROM 沒有額外執行成本。
