# Tyrian GBA 關卡間劇情與章節銜接（v67）

日期：2026-08-01
狀態：已實作並納入正式建置回歸

## 目標

這一階段補回 PC 版 `JE_loadMap()` 在「離開一關、進入下一關」之間執行的劇情與章節流程。重點不是仿造幾張固定畫面，而是讓 GBA 直接解讀 ROMFS 內原始 `levels1.dat`～`levels4.dat`，依遊戲狀態走原作者編排的分支。

來源基準為：

- `vendor/opentyrian/src/tyrian2.c` 的 `JE_loadMap()`。
- ROMFS 內 stock `levelsN.dat`、PIC、PCX、ANM 與調色盤資料。
- 既有 Episode map/level loader 與實際存檔狀態。

沒有新增每關專用劇情表，也沒有把故事文字轉成 GBA-only 資源。

## 已直接承接的腳本規則

可恢復執行的 `OtEpisodeSceneReader` 會從目前 section 繼續讀取原始加密 Pascal 字串，並支援：

- `]J` 無條件跳轉及 `]2` 模式分支。
- `]w` 船型、`]t` 計時器、`]l` 玩家死亡分支。
- `]H`／`]h` 難度分支、`]e` Engage 狀態、`]@` 文字色組切換。
- `]P` 圖片、`]U`／`]V`／`]R` 圖片轉場終點、`]M` 音樂。
- `]W` 一般文字、`Wy` 警告閃爍條與警報、十位數旗標指定的紅色文字模式。
- `]C` 清畫面、`]B` 淡黑、`]F` 閃白清除、`]A` `tyrend.anm` 動畫。
- `]Q` Episode 結尾提示；亂數只在真正遇到 `]Q` 時消耗，並沿用存檔中的 `secretHint`。
- `]I`／`]L` 返回選單或開始關卡，以及作者允許的 `*` 下一 section 順序落入。

條件判斷使用「剛結束關卡」的玩家生存、計時器與船型狀態，避免在讀到後續腳本前過早重設而走錯故事路線。

## GBA 顯示轉接

PC 劇情底圖是 320×200，GBA 以 Mode 4 在載入時一次縮成 240×160。PIC／PCX／ANM 解碼器直接讀 ROMFS；縮放在既有工作緩衝區就地完成，不保留第二份 64 KiB 畫面。

文字仍取 stock `levelsN.dat` 內容。因 PC 原行寬在 240 像素畫面會被截斷，顯示層使用既有 Data reader 的 source-derived 5-row tiny font 做單字換行：

- 不改寫原文與腳本資料。
- 依實際視覺行數調整 6～8 pixel 行距。
- 長段落會自動上移，保留底部按鍵提示空間。
- `Wy` 警告列與紅字提示使用不同旗標及座標，避免把兩種 PC 規則混為一談。

`P/U/V/R` 的最終圖片與調色盤保持來源一致；目前 GBA 版省略 PC 逐像素滑動的中間幀，直接呈現相同終點畫面。這是為靜態介面音訊不中斷所做的顯示層調整，不改變腳本順序或目標圖片。

## 流程銜接

- 一般關卡結束後先執行該 section 尚未執行的劇情，再回 Game Menu／Next Level。
- Episode 完成時先顯示原始結尾內容與 Episode announcement，再前進到下一 Episode。
- Episode 4 的 `Skip It` 仍執行作者編排的故事與 `]Q`，完成後回 Episode 1 首頁流程，而不是直接硬跳標題。
- `secretHint` 已加入 checkpoint/save round-trip，載入後不會遺失結尾提示路線。

## 驗證

- `romfs-matrix-autotest`：62/62 LVL 路徑及四 Episode 劇情指令矩陣通過，0 route failure、0 ROMFS failure。
- `episode-wrap-autotest`：Episode 4 section 44 的 `Skip It`、故事、Episode announcement、回 Episode 1 Game Menu 全部通過。
- `campaign-smoke-autotest`：連續四關後會實際送出按鍵走完新加入的來源劇情，確認最後回到 Game Menu；不再於劇情第一幀提早結束測試。
- `save-autotest`：新版 `secretHint` 欄位 round-trip 通過，舊版存檔仍由相容讀取路徑處理。
- `frontend-transition-stress`：17 條靜態選單路徑各 120 次，0 failure、0 missed VBlank。
- 實際截圖檢查過一般長文、`Wy` 警告及紅色文字三種版面，未再出現 240 像素右側截字。

正式 `build.ps1` 會另外執行 `episode-wrap-autotest`，並驗證 SRAM `TGSI` schema、來源路線、故事／公告是否出現、最終 Episode/section/state、音樂狀態與 ROM/IWRAM/EWRAM 預算。

## 完整 gameloop 後的效能基準

逐行補齊 PC 版 event、碰撞、死亡生成物與回饋特效後，舊版只涵蓋部分
gameloop 的工作量門檻已不再代表正式程式。第一關 high/normal 自動流程的新
基準為 7,096 次邏輯更新、12,246 個顯示迴圈、893 個來源事件，最終金額
15,539。這些數值已改為完整流程的 deterministic golden。

正式版保留 dynamic frame drop、wall-clock logic 與 physical-VBlank audio：

- 全關實測 12,685 次 VBlank 中遺漏 141 次，約 1.11%；全部發生於 gameplay。
- 前端、死亡、統計與轉場的 missed VBlank 均為 0。
- map stream、reward、projectile、ROMFS 與未完成 DMA 均為 0 drop／failure。
- PC 規格補齊後 Sprite2 工作量為 1,100 次 cache miss；硬體同畫面需求最高為
  cache 26 格加獨立玩家格，超量呈現與回饋特效池採受控 drop，不回退遊戲
  邏輯、碰撞、獎賞或來源事件。

正式門檻因此改為最多 2% presentation deadline miss，並繼續分別驗證
wall-clock 邏輯、音訊、前端零掉幀、快取帳務與來源 gameplay golden。這是
完整 PC 規格在 GBA 上的明確降級契約，不是把舊測試失敗靜默忽略。

## Episode 2～4 路線重校驗

完整 gameloop 納入後，正式建置原先的 Episode 2 精確計數仍停留在舊的
局部流程。重新以相同 high/normal 路線量測後，Episode 2 已完整走到事件
`1751/1752`、位置 `8829`，回到 Game Menu；素材解碼、ROMFS、L2 與所有
前端 missed VBlank 均為零失敗。`216/10831` gameplay presentation miss
落在正式 2% 契約內，因此更新 deterministic completion golden 與快取
帳務範圍，不刪除任何 gameplay 功能。

Episode 3 第一關直接通過來源流程。Episode 4 第一關則確認原本不是關卡
事件缺漏，而是 route-smoke 的自動輸入卡在 event-4 sky stop group：控制
hitbox 會短暫跨過 y 邊界，使既有「連續離屏 60 tick」測試 fallback 每次
歸零，永遠無法清掉 link 39。fallback 現在保留已累積的離屏 grace，只在
hitbox 真正位於可射擊範圍時交給按鍵射擊；下一次離屏即可完成原本已存在、
且只編入 autotest ROM 的一次碰撞。修正後 Episode 4 走到事件 `903/904`、
位置 `6624`，勝利音樂自然停止、四階段統計完成並回 Game Menu。正式遊戲
ROM 沒有這個測試輔助分支。
