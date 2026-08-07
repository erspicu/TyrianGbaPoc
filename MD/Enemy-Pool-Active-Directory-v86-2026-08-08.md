# Enemy Pool Active/Free Directory v86

日期：2026-08-08  
狀態：完成、預設啟用

## 目的與相容條件

OpenTyrian 的關卡敵人狀態固定分成 4 個 pool、每池 25 格。舊路徑每個
logic tick 固定逐格掃描全部 100 格；建立敵人時也逐格尋找最低編號的
空格，而玩家子彈碰撞階段又會再掃 100 格重建 collidable mask。

新路徑在 `OtLevelPortState` 尾端維護 4 個 32-bit active words，並讓
`enemy_avail[]` 的所有狀態變更集中經過同一個 setter：

- `enemy_avail[]` 仍是正式、具來源語意的權威狀態；
- active directory 收錄 `avail != 1`，因此仍包含 PC 版會繼續更新／繪製
  的死亡過場狀態 `avail == 2`；
- collidable directory 精確收錄 `avail == 0`；
- update 依原始 slot 升冪順序迭代，不改敵人更新、射擊與 RNG 順序；
- spawn 仍選每個 25-slot pool 中最低編號的空格；
- persistent collidable directory 省去每個碰撞 phase 的 100-slot rebuild。

## 正確性檢查

- 完成 600 VBlank 後逐格核對 100 個 `enemy_avail[]`、active mask 與
  collidable mask，結果 `7/7` 通過。
- legacy 與新路徑皆為 349 次 logic update、level position 698、
  2,860 次敵人 motion update。
- 兩版玩家武器結果一致：2,070 次生成、234 次滿池丟棄。
- packed collision 與色距 ARM 差分自測仍完整通過；音訊皆無 frame loss。
- 效能改善改變了最後一次成功呈現的 render tick，因此最終截圖的子彈／
  爆炸動畫相位不同；關卡邏輯 tick、位置與事件計數保持一致，這不是素材
  或遊戲狀態差異。

## A/B 壓力測試

條件：CUSTOM、Episode 1 Section 5、全武器、600 VBlank；其餘已採用的
星空、projectile cache、effect pool、player-shot directory 與 ARM
hotpath 均保持相同。

| 指標 | legacy scan | active/free directory | 差異 |
|---|---:|---:|---:|
| enemy update slot visits | 34,900 | 2,860 | -91.81% |
| enemy allocator slot / word probes | 528 | 120 | -77.27% |
| logic cycles total | 55,785,262 | 52,982,597 | -5.02% |
| prelogic cycles total | 59,603,899 | 58,288,540 | -2.21% |
| loop-work cycles total | 136,092,095 | 133,476,976 | -1.92% |
| missed VBlank | 262 | 253 | -3.44% |
| audio frame loss | 0 | 0 | 相同 |

正式預設：

```c
#define TYRIAN_GBA_ENEMY_ACTIVE_MASK 1
```

此實作只增加 16 bytes 關卡狀態，不新增大型 ARM/IWRAM 函式。壓力 ROM
的 IWRAM stack canary 尚餘 3,184 bytes，高於既有 3,072-byte 門檻。
