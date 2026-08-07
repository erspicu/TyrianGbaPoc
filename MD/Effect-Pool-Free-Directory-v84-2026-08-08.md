# Effect Pool Free-Slot Directory v84

日期：2026-08-08  
狀態：完成、預設啟用

## 結論

這一階段保留 OpenTyrian 原本的 200 格 effect／explosion 配置語意，
以 32-bit bitmask 建立 free-slot directory，將新增效果時的線性
`active` 掃描改為最多 7 個 mask word 的查找。配置仍選擇最低空槽，
update 仍由低到高，render 仍由高到低，因此不改變生命週期、繪製
優先順序或 OAM 取捨。

曾實作並量測「update/render 也只走 active bit」的完整 active
iterator，但壓力場景的 pickup explosion 平均密度很高，200 格中峰值
達 110 格；每一 active slot 的 bit-scan 呼叫成本反而高於單純略過
inactive byte。該 traversal 版本因此沒有納入正式路徑，正式版本只在
最有確定收益的配置階段使用 directory。

## 正確性防護

- `active` byte 仍是遊戲狀態；mask 是同步維護的索引。
- 關卡重設、配置、自然失效都同步 clear/set mask。
- 壓力測試結束會逐格比較兩個 pool，共 200 + 200 格。
- C 與 ARM lowest/highest-set-bit 對 zero、32 個 one-hot 以及 65,536
  組 deterministic pseudo-random 輸入做 differential test。
- legacy 與正式 C directory 的最終畫面 PNG SHA-256 完全一致。

## A/B 壓力測試

條件：CUSTOM、Episode 1 Section 5、全武器壓力模式、600 VBlank，
其餘既有 ARM hotpath、星空 batch 與 projectile hint 均開啟。

| 指標 | legacy 線性配置 | C free directory | 差異 |
|---|---:|---:|---:|
| missed VBlank | 292 | 288 | -1.37% |
| prelogic cycles total | 62,507,419 | 62,076,971 | -0.69% |
| loop-work cycles total | 139,764,995 | 139,033,244 | -0.52% |
| render cycles total | 38,426,359 | 38,555,518 | +0.34% |
| audio frame loss | 1 | 0 | -1 frame |
| logic updates / position / shot spawns | 349 / 698 / 2,070 | 349 / 698 / 2,070 | 相同 |
| pool mask consistency | 停用 | 3/3 | 通過 |

render 的 0.34% 變化不在 directory 所在路徑，屬連結布局／量測擾動；
真正相關的 prelogic 與整體 loop-work 均下降。

## ARM bit-scan 決策

16,384 次同 ROM microbenchmark：

| helper | C ARM/IWRAM | 手寫 ARM/IWRAM | 結果 |
|---|---:|---:|---:|
| lowest set bit | 994,527 | 1,052,171 | ARM 慢 5.80% |
| highest set bit | 1,150,763 | 1,167,066 | ARM 慢 1.42% |

兩個 ARM helper 的 65,536 組差分都正確，但工作量過小，函式呼叫與
Thumb/ARM 交換成本抵銷指令節省。因此保留組語作為研究／回歸路徑，
正式預設使用較快的 C ARM/IWRAM lowest-set-bit helper：

```c
#define TYRIAN_GBA_EFFECT_ACTIVE_MASK 1
#define TYRIAN_GBA_POOL_BIT_SCAN_ASM 0
```

壓力 ROM 的 IWRAM stack canary 尚餘 3,168 bytes，高於既有 3,072-byte
壓力測試門檻。
