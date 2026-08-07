# Player Shot Update Hybrid Directory v87

日期：2026-08-08  
狀態：完成、預設啟用

## 實作

`MAX_PWEAPON=81` 的每 tick 更新原本固定巡訪全部 81 格。v87 沿用已驗證
的 player-shot active mask，但採動態雙路徑：

- active shot `<= 24`：只依 bit directory 走訪有效 slot；
- active shot `> 24`：維持連續 81-slot byte scan，避免密集狀態下每發
  子彈都支付 bit-scan 成本；
- 兩條路徑都保持 slot 升冪順序，且共用同一份強制 inline 的來源語意
  update body，TTL、移動、complicated 軌跡、trail、追蹤、superpixel 與
  動畫順序皆不改變。

功能位於新檔 `src/combat_player_shot_update.inc`；原本 91,962-byte 的
`combat_runtime.inc` 已降為 86,823 bytes，符合來源檔不得超過約 90 KiB
的維護規則。

## A/B：全武器壓力

條件：CUSTOM、Episode 1 Section 5、600 VBlank、全武器，其他 v82-v86
最佳化全開。

| 指標 | 固定 81-slot scan | hybrid directory | 差異 |
|---|---:|---:|---:|
| update cycles total | 9,132,529 | 9,003,423 | -1.41% |
| update max cycles | 33,758 | 33,382 | -1.11% |
| loop-work cycles total | 133,952,666 | 133,850,383 | -0.08% |
| missed VBlank | 253 | 253 | 相同 |
| audio frame loss | 0 | 0 | 相同 |

這段壓測幾乎全程維持 25 發以上，因此 hybrid 正確選回線性密集路徑。
最終截圖 SHA-256 也完全相同：
`C5AE62C7DA10FE32061345CFDC4A1201D47462C42C615E497B7F02989871A449`。

## A/B：零發射稀疏邊界

同一路線但不發射，用來隔離空 pool 巡訪成本：

| 指標 | 固定 81-slot scan | hybrid directory | 差異 |
|---|---:|---:|---:|
| update slot visits | 28,269 | 0 | -100% |
| update cycles total | 1,081,890 | 97,539 | -90.98% |
| loop-work cycles total | 98,856,978 | 98,508,691 | -0.35% |
| missed VBlank | 12 | 12 | 相同 |
| audio frame loss | 0 | 0 | 相同 |

兩張稀疏截圖 SHA-256 均為
`F4417EA3DD4C8B5E44B13DDF74EB833534F285B29A64225097DBCC665E14313C`。

## 為何本階段不把整段改為 ARM/IWRAM

特徵遙測顯示全武器的 349 次 update 中，有 1,220 次 trail/explosion
建立，但 superpixel 與 aiming 都未觸發；資料結構巡訪只占局部成本。
把分支多、會呼叫 explosion/RNG 的完整 update orchestration 搬成 ARM，
會膨脹數百 bytes，且目前壓力 ROM 的 IWRAM stack canary 只剩 3,152 bytes，
距既有 3,072-byte 門檻僅 80 bytes。這不適合用安全空間交換很小的收益。

因此正式採用資料索引與 C/Thumb 共用 body；後續 ARM 工作只考慮可獨立
差分測試、無外部副作用的小型 leaf kernel。
