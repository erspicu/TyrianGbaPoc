# 玩家 Projectile Cache Direct Hint（v83，2026-08-08）

## 結論

玩家子彈 render 約 90% 以上的 frame acquire 是既有 cache hit，但舊路徑
每次仍線性掃描最多 18 格。v83 加入 64-byte direct-mapped hint directory，
先以 O(1) 找候選 slot，再驗證完整 key 與生命週期；碰撞、過期或無效 hint
一律退回既有完整掃描及 replacement policy。

hint 不擁有 slot、不改 eviction、不延長 resident 生命週期，也不省略任何
玩家子彈。slot invalidation 會在安全情況下清除仍指向自己的 hint，換關則
整表清空。

## 正確性與防污染設計

- packed key：`graphic | shape_table << 16`。
- ARM/C 候選函式都重新驗證：index 範圍、`valid`、graphic、shape table。
- hash collision 或 stale entry 只會造成一次 fallback scan，不能形成錯誤 hit。
- 4,096 組變動 key，逐項驗證 valid、invalid、graphic mismatch、shape
  mismatch 與越界 hint；C/ARM differential 結果為 `1`（PASS）。
- A/B 均為 349 logic updates、position 698、2,070 shot spawns、78 active
  shots，沒有改動 gameplay 時間軸。

## Hint 命中率

Episode 1／Section 5、CUSTOM／Normal、全武器壓力、600 VBlank：

- probes：6,990
- direct hits：6,347（**90.80%**）
- fallback scans：643
- cache drops：0

## 同條件完整路線 A/B

兩組都已啟用 v82 星空 ARM fast path；唯一變因是 projectile hint。

| 指標 | 線性掃描 | Direct hint | 差異 |
|---|---:|---:|---:|
| logic updates | 349 | 349 | 0 |
| missed VBlank | 313 | 298 | **-4.79%** |
| render cycles total | 41,464,252 | 38,524,435 | **-7.09%** |
| render cycles／logic | 118,808.74 | 110,385.20 | **-7.09%** |
| prelogic cycles／display | 111,614.44 | 106,408.84 | **-4.66%** |
| loop-work cycles／display | 238,429.95 | 233,850.96 | **-1.92%** |
| audio frame loss | 1 | 1 | 0 |

## 手寫 ARM 是否值得啟用

同 ROM、16,384 次 direct-hit microbenchmark：

| 實作 | cycles |
|---|---:|
| GCC `ARM_CODE`／IWRAM C | 1,561,781 |
| 手寫 ARM／IWRAM | 1,561,783 |

差距只有 2 cycles，完整路線的 render、missed VBlank 與 hint 命中也相同。
GCC 已產生等價的短 leaf，因此正式預設使用較易維護的 C/ARM 版本；手寫
組語與 differential test 保留作為研究及未來工具鏈回歸檢查，不把「有寫
ASM」誤當成實際效能收益。

## 記憶體

- 正式新增 EWRAM：64-byte hint directory。
- 壓力建置 IWRAM stack canary 尚餘 3,480 bytes，高於 3,072-byte 門檻。
- `source_runtime.inc` 仍為 89,042 bytes，低於專案 90 KiB 分檔門檻；
  lookup 與測試支援已獨立放在 `source_projectile_cache_lookup.inc`。
