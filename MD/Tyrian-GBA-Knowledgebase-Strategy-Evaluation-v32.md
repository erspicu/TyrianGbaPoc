# Tyrian GBA v32：Knowledgebase 效能策略評估

日期：2026-07-27
狀態：評估完成，值得進入實作

## 諮詢資料

已使用 `C:\ai_project\AprTyrianNes\knowledgebase\gemini_query.py`，
模型為 `gemini-3.1-pro-preview`。

- 問題：
  `knowledgebase/message/TyrianGbaPoc-v31-strategy-query-2026-07-27.md`
- 原始回答：
  `knowledgebase/message/TyrianGbaPoc-v31-strategy-response-2026-07-27.md`
- 62 關掃描：
  `knowledgebase/message/TyrianGbaPoc-v31-bg-working-set-scan-2026-07-27.json`

## Knowledgebase 回答中成立的部分

- v31 之後的主要問題是 BG character VRAM 拓撲，不是 Maxmod、
  Sprite2 RLE 或 ROMFS 解壓。
- GBA 4bpp regular BG 的 10-bit character index，讓單一 BG 最多
  存取 1,024 tiles／32 KiB。
- 高需求圖層可以跨兩個相鄰 charblocks；其他圖層可共用同一個
  character base，只要使用的 tile-index 區間不重疊。
- 動態或非對稱分配 charblocks 0–2，可以消除真正超過 512 tiles
  時的 visual approximation。
- Maxmod 應保留。v31 停播全部音訊只把 missed VBlank 從 30 降到
  29，沒有足以支持 PSG 降級的效能證據。

## 回答中需要修正的部分

回答建議固定使用 `1024/256/256`。這個 layout 在硬體上合法，但
不是本專案的通用解：

- Episode 2 logical level 6 的需求是 `520/311/1`；
- 高需求層使用 1,024 slots 後，另一層只有 256 slots，會低於實際
  311 slots，仍然破圖；
- 現有 cache 的 pattern、reference、generation、free queue 與
  direct hash index 都固定為 512，不能只修改 BGCNT 初始化；
- `0x01ff` slot mask、tilemap word、DMA physical address 與 EWRAM
  backing 都必須一起改。

因此 knowledgebase 的方向值得採用，但原始配置與實作成本估計過度
簡化，不能直接照抄。

## 使用 runtime 相同演算法的 62 關驗證

診斷工具直接讀 stock `tyrianN.lvl`、`shapes?.dat`、palette 0，
重現 runtime 的：

- map sentinel 規則；
- top／bottom shape 與 28-pixel phase；
- dominant palette bank；
- 256 色到 4bpp 的 nearest-colour mapping；
- 22-row visible＋prefetch pattern working set。

結果與 v31 既有探勘一致：

| 關卡 | Layer 0 | Layer 1 | Layer 2 | 合計 |
|---|---:|---:|---:|---:|
| Episode 2 logical 1 | 501 | 1 | 1 | 503 |
| Episode 2 logical 6 | 520 | 311 | 1 | 832 |
| Episode 2 logical 11 | 533 | 1 | 1 | 535 |

全部 62 關：

- 固定 512/512/512 可容納：60 關；
- 需要非對稱配置：2 關；
- 任一單層最大需求：533；
- 任一關三層最大合計：1,053；
- 各 layer 跨全關最大值：`533/434/454`；
- 無任何關卡超過 1,536 physical slots 或單層 1,024-index 上限。

## 比 knowledgebase 範例更通用的配置

不需要 per-level table，也不需要進關時掃完整張 map。所有關卡可固定
使用：

| Layer | Capacity | Physical slots | Character base | Tile index |
|---|---:|---:|---:|---:|
| 0 | 576 | 0..575 | 0 | 0..575 |
| 1 | 480 | 576..1055 | 1 | 64..543 |
| 2 | 480 | 1056..1535 | 2 | 32..511 |

三段恰好使用 charblocks 0–2 的 1,536 tiles；screen blocks 24–29
仍位於 charblock 3，不重疊。相對全關最大值尚有 `43/46/26` slots
餘量。

這是完整 source-bank 探勘後得到的一套全域硬體配置，不含任何 Episode
或 level 特例。

## Knowledgebase 沒有發現的現行缺陷

Episode 2 第一關 22-row 真正峰值只有 501，理論上不應超過 512，
但 v31 route 仍有 28 次 approximation。

現有 `pattern_index[512]` 是單入口 direct-mapped accelerator：

1. hash bucket 被另一張 pattern 覆寫後，原 pattern 仍在 cache；
2. 再次要求原 pattern 時，程式找不到既有 slot；
3. 同一 pattern 可能被重複配置到另一 slot；
4. reference slots 被重複圖樣填滿後，即使真正 unique set 小於 512，
   仍可能誤觸 nearest-pattern fallback。

EP2 第一關 501-pattern 峰值只分布在 309 個 direct-index buckets，
有 192 個 bucket collisions。這不能直接等同 192 次 runtime duplicate，
但足以證明單入口 index 不是 collision-safe，且與 28 次異常
approximation 的現象一致。

## 實作定案

### 第一階段：collision-safe pattern index

- 以每層 bucket heads、每 slot hash 與 next link 取代 direct entry。
- lookup 只走同 bucket chain，找到相同 32-byte pattern 才配置新 slot。
- eviction 時從舊 bucket unlink，再插入新 bucket。
- 使用的 metadata bytes 可維持與現有 direct index 相同量級。

驗收：

- Episode 2 第一關 background approximation 必須由 28 降到 0；
- route、event、collision、Sprite2 workload 完全不變；
- missed VBlank 不得劣於 v31 的 30；
- 若 approximation 未歸零，繼續查 reference ownership，而不是用更大
  VRAM 掩蓋 cache correctness 問題。

### 第二階段：全域 576/480/480 VRAM partition

- 把三層固定 512 arrays 改成總數仍為 1,536 的 pooled backing；
- 每層使用 compile-time capacity、physical start、character base 與
  tile-index offset；
- semantic generation 驗證改用 local slot，tilemap word 使用 10-bit
  hardware index；
- full-layer 與 row upload DMA 依 physical start 寫入；
- 所有 cache queue wrap 改為支援非 2 次方 capacity。

驗收：

- 62 關靜態／runtime matrix 都符合各層 capacity；
- Episode 2 logical level 6、11 不再需要 nearest approximation；
- charblock 3 的 screen maps 不受破壞；
- EWRAM 至少 48 KiB、IWRAM 至少 6 KiB；
- Episode 1、Episode 2 route、campaign、death、Jukebox 全部回歸。

## 不採用項目

- 不把 Maxmod 降成 PSG；
- 不建立 per-level GBA-only background catalog；
- 不重新預展開已是 raw 的 `shapes?.dat`；
- 不先最佳化 nearest-pattern 的 O(512×32) 搜尋來掩蓋容量或索引錯誤。
