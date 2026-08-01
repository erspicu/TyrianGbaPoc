# Episode 4 `Skip It` 流程修復

日期：2026-08-01
狀態：Root cause、修復與動態回歸已完成

## 問題

開發版啟用 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=1` 時，Episode 4
Time War 無法藉由玩家生命耗盡離場，因此需在 Next Level 畫面
選擇原始資料已提供的 `Skip It`。修復前，選擇後卻直接回到
Tyrian 首頁，與 PC 原始流程不同。

## Root cause

`levels4.dat` section 44 的第二條路線是 planet 21 `Skip It`，
目標 section 42；section 42 最後以 `]Q[` 結束 Episode。GBA 的
episode parser 已正確解出這條路線與 `episode_complete`，錯誤在
`frontend_advance_episode()`：

- 修復前：Episode 4 沒有下一個陣列索引，便硬編碼呼叫
  `enter_title()`。
- PC 原始碼：`JE_nextEpisode()` 呼叫 `JE_findNextEpisode()`；超過
  Episode 4 時會設定 `jumpBackToEpisode1`，將 episode 循環回 1，
  重設 `mainLevel` / `saveLevel` 為 `FIRST_LEVEL`，再繼續 campaign。

因此這不是按鍵、地圖索引或無敵判斷問題，而是 GBA 章節進階函式
將「最後一章」誤當成「離開遊戲」。

## 修復

1. `frontend_advance_episode()` 改為與 `JE_findNextEpisode()` 一致的週期進階：
   Episode 4 完成後轉到 Episode 1 section 1，準備對應地圖與
   Game Menu 音樂，不再返回首頁。
2. 開發無敵設定恢復為絕對無敵；Time War 在此模式下就使用
   `Skip It`，不在 gameplay 裡加入隱藏的死亡例外。
3. 新增 `episode-wrap-autotest`，直接從 Episode 4 section 44 選擇
   原始第二條路線，驗證循環後的 campaign 狀態。
4. 原有 `scripted-survival-autotest` 改用獨立
   `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0` 編譯，繼續驗證 PC 原始的
   三命、GAME OVER 與 section 44 checkpoint 回復路線。

## 驗證結果

`episode-wrap-autotest` 在 mGBA headless 實際執行後寫入：

- signature：`TGSI`
- schema：`1`
- pass：`1`
- 來源路線：Episode 4 section 44, choice 2, planet 21, section 42
- 結果：Episode 1 section 1
- 狀態：`STATE_GAME_MENU`
- Game Menu selection：`Play Next Level`
- 音樂引擎：active
- runtime errors：`0`

另外，以無敵關閉的獨立 Time War 回歸再測，仍得到
`TGSX schema=1, pass=1`；三命耗盡後回復 section 44 的正常遊戲
路徑沒有被本修復破壞。
