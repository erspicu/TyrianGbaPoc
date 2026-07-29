# Tyrian GBA v32：Collision-safe 背景快取與全域 VRAM Partition

日期：2026-07-27
分支：`opentyrian-source-parity-port`
狀態：完成

## 結果

v32 是對 knowledgebase 諮詢建議的驗證與強化實作。它沒有建立
per-level 資源、沒有改動 LVL／shape 資料，也沒有降低音訊品質。

Episode 2 section 1 的 deterministic full route：

| Low Detail／Normal Speed | v30 | v31 | v32 |
|---|---:|---:|---:|
| missed VBlank | 553 | 30 | **3** |
| background approximations | 472 | 28 | **0** |
| logic updates | 6,065 | 6,065 | 6,065 |
| display frames | 10,475 | 10,475 | 10,475 |
| collision calls | 838 | 838 | 838 |
| Sprite2 L1 hits／misses | 69,060／3,276 | 相同 | 相同 |
| Sprite2 L2 hits／misses | 2,868／413 | 相同 | 相同 |
| Sprite2 RLE fallback | 0 | 0 | 0 |

其他正式設定：

- Normal Detail／Normal Speed：5 missed VBlanks、0 approximations；
- Low Detail／Low Speed：2 missed VBlanks、0 approximations。

## Knowledgebase 建議的評估結果

Knowledgebase 正確建議使用非對稱 BG character VRAM，但它提出的固定
`1024/256/256` 不適合全部 Tyrian 關卡：

- Episode 2 logical level 6 的需求是 `520/311/1`；
- 第二層需要 311，放不進 256 slots；
- 現有 cache metadata、DMA address 與 tilemap index 也不能只靠修改
  BGCNT 完成。

完整評估見：

- `Tyrian-GBA-Knowledgebase-Strategy-Evaluation-v32.md`

## 第一個真正瓶頸：direct hash collision

v31 的 Episode 2 第一關 22-row 峰值只有 501 個 unique patterns，
理論上可放入 512 slots，卻仍有 28 次 approximation。

原因是舊 `pattern_index[512]` 每個 hash bucket 只記一個 slot：

1. 不同 pattern 發生 bucket collision；
2. 新 entry 覆寫 index，但舊 pattern 仍實際留在 cache；
3. 舊 pattern 再次出現時無法被找到，因而重複配置；
4. 重複圖樣佔滿 reference slots，誤觸容量 fallback。

v32 以 collision-safe chained buckets 取代 direct entry：

- 每層保留 512 個 bucket heads；
- 每個 resident slot 保存 hash 與 next link；
- lookup 只走相同 bucket，並以 32-byte pattern 完整比對；
- eviction 先從舊 bucket unlink，再插入新 bucket；
- 三層 hash／next／head 合計仍為 12 KiB，與舊三份 4 KiB direct
  index 相同，沒有用更多 index EWRAM 換結果。

只完成這項修正時，Episode 2 第一關已由 `30/28` 降到
`3/0`（missed VBlank／approximation），證明 v31 剩餘尖峰不是
501-pattern 工作集本身，而是 collision-unsafe index。

## 第二個硬體限制：固定 512/512/512

使用 runtime 相同的 map sentinel、shape phase、palette bank 與 4bpp
mapping，掃描全部 62 logical levels 的 22-row working set：

- 固定 512/512/512 可容納 60 關；
- Episode 2 logical level 6：`520/311/1`；
- Episode 2 logical level 11：`533/1/1`；
- 任一關的三層合計最大值：1,053；
- 各 layer 跨全關最大值：`533/434/454`。

全部關卡都低於 GBA 的 1,536 physical BG tiles，且任何單層都低於
10-bit character index 的 1,024 tiles。

## 全專案共用 576/480/480 Partition

v32 不在 runtime 掃完整張關卡，也不儲存 per-level capacity table。
所有 62 關共用一套 layout：

| Layer | Capacity | Physical slots | Character base | Hardware tile index |
|---|---:|---:|---:|---:|
| 0 | 576 | 0..575 | 0 | 0..575 |
| 1 | 480 | 576..1055 | 1 | 64..543 |
| 2 | 480 | 1056..1535 | 2 | 32..511 |

- charblocks 0–2 恰好容納 1,536 個 4bpp tiles；
- screen blocks 24–29 從 charblock 3 開始，與 pattern pool 不重疊；
- 三層餘量分別為 43／46／26 slots；
- 每層 physical range 都位於其 character base 可見的 1,024-index
  window。

## Pooled EWRAM backing

原本三份固定 512 arrays 改為總數仍為 1,536 的 pooled backing：

- pattern pixels；
- reference count、generation、valid；
- pattern hash／next；
- free queue／queued flags。

每個 layer cache 保存 capacity、pool offset、physical start、
character base 與 tile-index offset。semantic cache 仍保存 hardware
tilemap word，但 generation 驗證先把 10-bit hardware index 轉回
local slot。full-layer 與 row DMA 都以 physical start 寫入 VRAM。

480／576 不是 2 次方，所以 allocation cursor 與 free queue wrap 改為
有界比較，不能再使用 `& 511`。

## IWRAM／EWRAM

collision lookup、tile hash 與高頻 semantic lookup 保留在 IWRAM。
只在 semantic miss 寫入 metadata 的 `background_semantic_store()`，
以及每列一次的 release helper 放在 ROM，以保留安全餘量。

| Build | EWRAM free | IWRAM free |
|---|---:|---:|
| release | 49,480 bytes | 7,168 bytes |
| Episode 2 route | 49,480 bytes | 6,936 bytes |
| four-level campaign | 49,336 bytes | 6,920 bytes |

所有 build 仍通過 EWRAM 至少 48 KiB、IWRAM 至少 6 KiB 的門檻。

## 音訊與資源決定

- 保留 Maxmod。v31 停播全部音訊只改善一次 missed VBlank，v32 已在
  保留完整音訊時降到 2–5。
- 不建立 GBA-only per-level background catalog。
- 不預展開已是 raw 的 `shapes?.dat`。
- nearest-pattern code 保留為資料損壞或未來未知資料的 emergency
  fallback，但 Episode 2 永久回歸現在要求 approximation 必須為 0。

## 回歸

- Episode 1 schema 25 gameplay 與 Boss 視窗；
- Episode 2 schema 3 完整 route；
- 62-section ROMFS／6,098-frame Sprite2 matrix；
- four-level campaign；
- death／Game Over；
- 41-song Jukebox；
- Low／Normal Detail；
- Low／Normal Game Speed；
- EWRAM、IWRAM、VRAM 與 32 MiB ROM 安全門檻。

Episode 2 的永久門檻已從 `approximations <= 64`／
`missed VBlank <= 50` 收緊成：

- `approximations == 0`；
- `missed VBlank <= 10`。

## 正式成品

- ROM：
  `tyrian_gba_level1_pc_flow_mode4_romfs_v32_detail_low_speed_normal.gba`
- 容量：14,145,892 bytes
- SHA-256：
  `8af99d6859ddb899e1106473e7b3ddd747e3754c9bf4e53860f5dad1472fba34`
- 發布版本：GitHub Release `v32`
