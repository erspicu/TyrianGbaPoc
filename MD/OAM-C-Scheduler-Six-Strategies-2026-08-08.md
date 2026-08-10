# OAM C 層級排程六項改善報告

日期：2026-08-08  
測試路徑：Episode 1／Section 37（LVL 17）  
壓力條件：PENTIUM、無敵、全武器壓力配置、持續射擊、600 VBlank

## 結論

六項 C 層級改善已全部實作。最終版與修改前使用相同 ROM 資料、遊戲邏輯、
物件數與 OAM 優先規則；畫面截圖 SHA-256 完全一致。渲染總週期下降
3.86%，完成一次 render 的平均週期下降 6.01%，missed VBlank 由 156
降至 147。敵人／Boss OAM 淘汰與敵人快取預鎖失敗都維持 0，音訊更新沒有
丟失。

## 六項策略與實作

1. **每個 presentation frame 只建立一次候選目錄**

   `SourceFrameOamCandidates` 集中保存敵人、玩家子彈、敵方子彈、拾取爆炸與
  一般效果的候選資料。收集時一次算好 screen coordinate、可見性、OAM 成本與
   draw stage，後續 demand、cache admission 與 render 共用同一份 snapshot，
   不再各自重掃 pool。

2. **敵人選擇與快取預鎖合併**

   `source_enemy_select_and_prime()` 依 Boss 優先、一般敵人次之的順序，在同一
   traversal 內完成 OAM admission、Sprite2 cache acquire 與 tile 記錄。舊的
   visible-order、selected-array、primed-tile 與獨立 prime traversal 已移除。

3. **OAM demand 使用飽和計數**

   正式排程只需要知道需求是否超過 128，所以累加值在 `SPRITE_LIMIT + 1`
   飽和，避免無意義的大數累加。壓力測試組態另保留 exact counter，因此
   telemetry 仍能報出本場景真實需求峰值 223。

4. **爆炸 pool 採稀疏／密集混合掃描**

   每個 32-slot mask word 若為 0，直接跳過整段；非 0 word 則線性走訪其中的
   active byte。這避開「稀疏時掃 200 格」與「密集時每格做 bit-scan」兩個極端
   成本。關閉 active-mask 的 fallback 組態也已通過實機路徑測試。

5. **輪替改成兩段連續 range**

   玩家子彈、敵方子彈、拾取爆炸、一般效果與敵人 admission 都將
   `[cursor, count)`、`[0, cursor)` 分成兩段走訪；內層不再對每個候選做 modulo
   或 wrap branch。

6. **cursor 從最後處理候選的下一位續跑**

   不再每幀只做固定 `cursor + 1`。當 OAM window 提前用完時，cursor 會記住
   最後嘗試／成功 admission 的候選下一位，下一個 presentation frame 從該處
   繼續，讓超額低優先效果與結構物 fallback 輪替更公平。

## A/B 效能結果

| 指標 | 修改前 | 六項改善後 | 差異 |
|---|---:|---:|---:|
| 遊戲邏輯更新 | 349 | 349 | 相同 |
| 關卡位置 | 374 | 374 | 相同 |
| 場景 OAM 需求峰值 | 223 | 223 | 相同 |
| 實際 OAM 峰值 | 128 | 128 | 相同 |
| 玩家子彈生成／drop／峰值 | 1984／202／81 | 1984／202／81 | 相同 |
| render cycles total | 34,799,429 | 33,455,571 | **-3.86%** |
| completed render 平均 cycles | 198,853.88 | 186,902.63 | **-6.01%** |
| completed render | 175 | 179 | +4 |
| deferred render | 295 | 292 | -3 |
| missed VBlank | 156 | 147 | **-9（-5.77%）** |
| 敵人 OAM 淘汰 | 0 | 0 | 相同 |
| 敵人 cache prime 失敗 | 0 / 228 | 0 / 228 | 相同 |
| 玩家子彈 OAM 淘汰 | 1 | 1 | 相同 |
| 低優先效果 OAM 淘汰 | 149 | 149 | 相同 |
| 音訊 frame loss | 0 | 0 | 相同 |

最終畫面與 baseline 的 SHA-256 均為：
`549B490F84201455DFB38B643E183615A0DA1DBE4771145302313AF8B3E9D632`。

`enemy_cache_drops` 由 66 增至 90，但敵人預鎖失敗仍為 0；這個欄位也包含
敵人之後進入共享 Sprite2 L1 的低優先圖幀競爭。改善後多完成 4 個 render，
輪替窗口也多服務 5 個 pool-frame，因此會看見更多低優先 cache admission
嘗試。玩法物件、最終畫面與進度均未退化。

## 記憶體成本

- 壓力版 `.bss`：239,272 → 242,228 bytes，增加 2,956 bytes。
- 動態 EWRAM heap used：3,764 → 4,904 bytes，增加 1,140 bytes。
- allocator 回報 remaining：16,384 → 12,288 bytes（以配置區塊為粒度）。
- IWRAM stack canary remaining：3,208 bytes，與 baseline 相同。
- 正式 CUSTOM build 成功；一般版的 pickup pool 比壓力版小，因此候選目錄
  的 EWRAM 成本也較低。

這次沒有把整個排程器改寫成 ARM 組語。既有 differential benchmark 顯示
pool bit-scan ARM 版本比 `-O3` C 版本慢 1.4%～5.8%，而排程器分支多、資料
導向明顯，整體 ARM 化還會擠壓 IWRAM。先改善資料流與 traversal 才是此處
較高收益且較低風險的做法。

## 驗證

- PENTIUM／Episode 1 Section 37／全武器壓力／600 VBlank：通過。
- active-mask 關閉的 180 VBlank fallback：通過，mask consistency 符合預期。
- CUSTOM 一般版完整第一關 golden autotest：`Pass=1`、`State=7`、音樂啟用、
  截圖成功。
- `git diff --check`：通過；只有既有 PowerShell LF/CRLF 提示。
- 編譯器沒有新增 warning；現有 unused reference helper 與 GNU-stack linker
  warning 仍維持原狀。

## 最終 ROM

- 組態：CUSTOM detail、Normal game speed。
- 路徑：`build/TyrianGBA.gba`
- 大小：28,038,404 bytes（26.74 MiB）。
- SHA-256：`CC22390F42DD6F2D3669F98B6AD036BF7C08A8467B52344D4C90A3B5EF5184E8`
- 正式 ROM 在 mGBA 連續執行 30 秒，stdout／stderr 均無錯誤；正式版不會像
  autotest 呼叫退出 SWI，所以到時由測試器主動停止。

## 證據檔

- [修改前完整 telemetry](Evidence/OAM-C-Scheduler-Baseline-Section37-2026-08-08.json)
- [改善後完整 telemetry](Evidence/OAM-C-Scheduler-Final-Section37-2026-08-08.json)
- [改善後壓力場景截圖](Evidence/OAM-C-Scheduler-Final-Section37-2026-08-08.png)
