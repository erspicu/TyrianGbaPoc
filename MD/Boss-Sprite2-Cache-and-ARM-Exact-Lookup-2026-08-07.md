# 大型 Boss Sprite2 快取與 ARM 精確命中優化（2026-08-07）

## 結論

大型 Boss 被密集射擊時的主要瓶頸不是 Boss 碰撞體積，也不是單純
OAM 到達 128 個，而是受擊閃色 `filter != 0` 產生大量短命 Sprite2
變體，反覆逐出下一幀仍會使用的 `filter == 0` 本體影格。這造成 L1、
L2 與 VRAM DMA 一起顛簸。

本階段採用兩層改善：

1. **filter-aware admission**：濾色變體只准使用真正空槽；滿載時退回
   同圖形的 base frame，若 base 尚未建立則建立 `filter == 0` 版本。
2. **唯讀 ARM exact scan**：先用零 stack ARM leaf 掃描精確 key；命中
   後略過完整 C replacement-policy 迴圈，miss 則完整回到原管理器。

兩項都保留遊戲邏輯、碰撞、Boss 血量與事件流程；第一項只允許在
快取壓力高時省略極短暫的受擊閃色，不會省略 Boss 本體。

## Gemini 3.1 Pro 諮詢的採用與否

採用的部分：

- 將問題判定為 transient filtered frame 引起的 cache thrashing。
- `filter != 0` 採「真正 free slot 才准入」，否則 fallback 至 base。
- base slot 可以是 resident 或 pending；本專案的提交順序是 tile DMA
  先於 OAM DMA，因此同一 VBlank 內引用 pending slot 安全。
- 一次 C 掃描同時蒐集 exact/base/free/eviction 候選，不額外再掃一次。

未採用或修正的部分：

- 諮詢回覆把 `761,088 bytes` 的測試視窗累計值誤讀成單幀上傳量；
  本報告只使用相同視窗的 A/B 累計數值。
- 「把星空完全改成靜態硬體 BG」不符合現有 Tyrian 多層與事件資料
  路徑，也不是本次 Boss root cause，因此沒有改動。
- 沒有導入 Boss linked-list 或整段 `source_enemy_cache_acquire()` ASM；
  本次證據顯示先修 admission policy 的收益遠大於改寫複雜控制流程。

完整諮詢輸入與回覆保留於工作區 knowledgebase 的 message 目錄，沒有
直接把回覆當成規格，所有採用項目均以本專案 telemetry 再驗證。

## Filter-aware L1 規則

- 精確 filtered hit 仍照常顯示，保留暖快取時的完整受擊閃色。
- filtered miss 只有遇到未保留、未使用的真正 free slot 才建立。
- 沒有 free slot 時：
  - 已有 resident/pending base：直接使用 base。
  - 沒有 base：以 `filter == 0` 建立可長期重用的本體影格。
- 需要逐出時，先逐出過期 filtered slot，再考慮 base slot。
- palette setup 仍沿用既有 L1/L2 flush 規則，不保留跨 palette 資料。

新增 telemetry：admission denial、fallback hit、fallback build、filtered
eviction、exact lookup probe/hit，方便日後找出其他高壓武器或 Boss。

## Episode 3 / Section 11（MACES）Boss A/B

條件相同：CUSTOM、Normal speed、前武器 power 11、無敵測試、自動持續
射擊；兩版均完成 5,853 次邏輯更新、擊殺 150、到達 position 8,058，
unknown visual 與 Sprite2 decode failure 都是 0。

| 指標 | 舊 admission | filter-aware | 差異 |
|---|---:|---:|---:|
| 全路線 missed VBlank | 1,990 | 1,874 | **-5.8%** |
| 全路線 Sprite2 L1 miss | 5,797 | 1,909 | **-67.1%** |
| 全路線 Sprite2 upload bytes | 5,837,056 | 1,848,832 | **-68.3%** |
| Boss 視窗 missed VBlank | 404 | 324 | **-19.8%** |
| Boss 視窗 Sprite2 L1 miss | 4,195 | 1,110 | **-73.5%** |
| Boss 視窗 upload bytes | 4,226,560 | 1,057,536 | **-75.0%** |
| Boss 視窗 L2 miss | 322 | 43 | **-86.6%** |

本關共拒絕 5,269 次不值得快取的濾色變體，其中 4,353 次直接重用
base、174 次建立 base；只逐出 6 個過期 filtered slot。

## ARM exact scan

`SourceEnemyCacheSlot` 的 `graphic/shape_table/size` 在 offset 8..11 恰好
能組成 little-endian 32-bit packed key。ARM leaf 對每個 slot 使用一次
對齊 word load，再檢查 filter 與 valid；它：

- 不配置或修改 hash table；
- 不擁有 cache lifecycle；
- 不寫入任何 cache metadata；
- 不使用 stack；
- miss 時一定回到原 C manager。

先前曾做過 64-entry hash hint 試作，但在全武器壓力 ROM 的較長視窗
出現無法完成測試的情況。因為無法證明安全，已完整撤除，沒有留在
正式程式；唯讀掃描版不具備該類污染面。

差分測試以 2,048 組 key，逐一驗證 valid hit、invalid、graphic、
shape table、size、filter mismatch，共 12,288 組 C/ARM 結果一致。

16,384 次固定輸入集合 microbenchmark：

| 實作 | cycles | 差異 |
|---|---:|---:|
| C reference | 2,654,916 | baseline |
| ARM/IWRAM | 2,194,299 | **-17.35%** |

## 全武器 600 VBlank A/B（Episode 1 / Section 5）

兩版均為 CUSTOM、Normal、壓力武器、持續射擊、無敵、adaptive 與
drop-frame，且都完成 349 次邏輯更新。

| 指標 | exact scan 關 | exact scan 開 | 差異 |
|---|---:|---:|---:|
| missed VBlank | 452 | 434 | **-4.0%** |
| audio frame loss | 15.00% | 11.83% | **-3.17 個百分點** |
| loop 平均 cycles | 259,173.44 | 257,864.43 | **-0.51%** |
| render 平均 cycles | 169,325.35 | 164,213.13 | **-3.02%** |
| exact probe / hit | 0 / 0 | 7,208 / 5,732 | 79.52% 命中 |

ARM 版 IWRAM runtime canary 尚餘 3,984 bytes，高於壓力測試既有
3,072-byte 判定線；所有既有 colour distance、packed collision、
detail effect 與本次 exact scan differential flag 均通過。

## 最終 MACES 驗證

兩項改善同時啟用後：

- 全路線 missed VBlank：1,672
- Boss 視窗 missed VBlank：308
- Boss 視窗 Sprite2 miss：1,110
- Boss 視窗 upload：1,060,608 bytes
- exact lookup：68,778 / 76,291，命中率 90.15%
- 邏輯更新 5,853、擊殺 150、position 8,058，與比較版一致
- unknown visual / decode failure：0 / 0

相對僅啟用 filter-aware 的版本，ARM exact scan 再讓全路線 missed
VBlank 下降約 10.8%，Boss 視窗下降約 4.9%。

## 維護與預設

- `TYRIAN_GBA_FILTER_AWARE_SPRITE2_CACHE=1`：正式預設。
- `TYRIAN_GBA_SPRITE2_EXACT_LOOKUP_ASM=TYRIAN_GBA_HOTPATH_ASM`：正式
  預設；可獨立關閉供 A/B 與除錯。
- `source_runtime.inc` 的 exact lookup 與 admission hotpath 已拆至
  `source_enemy_cache_lookup.inc`、`source_enemy_cache_acquire.inc`，主檔
  已降至 90 KiB 以下。

本次定案不是「Boss 越大，碰撞數學必然越慢」；真正主因是多部件
受擊閃色把穩定 Sprite2 工作集逐出。先修正資源政策，再針對高命中
唯讀路徑做小型、可差分驗證的 ARM 優化，才是風險最低且收益最大的
處理順序。
