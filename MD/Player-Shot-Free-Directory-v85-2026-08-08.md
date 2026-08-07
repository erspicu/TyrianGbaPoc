# Player Shot Free-Slot Directory v85

日期：2026-08-08  
狀態：完成、預設啟用

## 目的與不變條件

OpenTyrian 的 `MAX_PWEAPON` 是 81 格，且配置必須選擇最低編號空格。
全武器壓力下，pool 經常全滿；舊實作每次配置或確認滿載都從 slot 0
逐格掃描，造成大量無效 EWRAM byte read。

新路徑以 3 個 32-bit word 維護 active directory：

- lowest-free 規則不變；
- `PlayerShot.active` 仍是正式遊戲狀態；
- directory 與 active count 只作索引／telemetry；
- 關卡清除、一般／壓力武器、orbiting asteroid、碰撞消耗、TTL 與
  越界失效全部經同一組 set-active helper 同步；
- update、碰撞與 render 的 slot 順序完全不變。

## 正確性

- 600 VBlank 後逐格比較 81 個 active byte 與 mask：通過。
- active count 78，與既有 `active_shots_at_finish=78` 相同。
- legacy 與 directory 的最終 PNG SHA-256 均為
  `C22319211A872123F68CCB3A5B1173F069F95DC73BD500202E55ED07A9799B22`。
- 兩版皆為 349 logic updates、level position 698、2,070 次成功生成、
  234 次滿池丟棄、峰值 81 格。

## A/B 壓力測試

條件：CUSTOM、Episode 1 Section 5、全武器、600 VBlank；effect free
directory、星空 batch、projectile cache hint 與既有 ARM hotpath 開啟。

| 指標 | legacy 81-byte scan | 3-word directory | 差異 |
|---|---:|---:|---:|
| allocator calls | 2,304 | 2,304 | 相同 |
| slot / mask-word probes | 92,632 | 4,182 | -95.49% |
| missed VBlank | 292 | 262 | -10.27% |
| prelogic cycles total | 62,893,428 | 59,481,607 | -5.42% |
| loop-work cycles total | 139,774,748 | 135,969,324 | -2.72% |
| render cycles total | 38,636,190 | 38,732,167 | +0.25% |
| audio frame loss | 0 | 0 | 相同 |

render 不使用此 directory，0.25% 屬連結布局／量測擾動；直接受影響的
prelogic 與整體 loop-work 都有明顯改善。正式預設：

```c
#define TYRIAN_GBA_PLAYER_SHOT_FREE_MASK 1
```

壓力 ROM 的 IWRAM stack canary 尚餘 3,152 bytes，高於既有 3,072-byte
壓力測試門檻。
