# Boss 碰撞快照與 Active Directory 優化（v88）

日期：2026-08-08

## 結論

大型 Boss 受攻擊時的主要 CPU 瓶頸不是 OAM 上限，也不是 Sprite2
首次解碼。固定在 Episode 1／Section 1 的真實 Boss 區間量測後，原版
壓力路徑有約 52% 的 logic cycles 花在玩家子彈碰撞流程，其中每次
命中又會為 linked status、死亡群組、damaged transition 與玩家接觸
重掃 100 格 enemy pool。

本次保留兩項 source-parity 安全的改善：

1. build/runtime 共用的 8-byte collision snapshot：每個可碰撞 enemy
   預先保存 `x/y/radius_x/radius_y`，ARM miss path 只需兩次連續 EWRAM
   word load，不再跨 134-byte `OtEnemy` 結構做四次零散讀取。
2. collision active directory：仍以 PC 原始碼的 0..99 升冪順序處理，
   但只走 `enemy_active_mask` 內的 live slots。每次 callback/mutation 後
   都重新讀 mask，因此同一輪中新生或被釋放的 enemy 行為不變。

## 固定量測條件

- Detail：CUSTOM
- Game Speed：Normal
- 路線：Episode 1、Section 1
- 玩家：無敵、全武器壓力模式、持續射擊
- 測試前段：所有 source ticks／events 依序執行，只省略顯示
- Boss 區間：`level_position 5401 -> 5466`，65 個 logic ticks、113 個
  display frames
- 比較組的 Boss candidates 均為 23,019、hits 均為 251、kills 均為 5

Boss 起訖現在由 authoritative logic tick 偵測，不再由可能被
drop-frame 略過的 render path 偵測，因此 A/B 不會偏移一個 tick。

## 合併前後 A/B

| 指標 | 舊路徑（snapshot 0／directory 0） | 新路徑（snapshot 1／directory 1） | 差異 |
|---|---:|---:|---:|
| Boss logic cycles | 9,318,471 | 8,284,637 | **-11.09%** |
| Boss collision cycles | 4,853,511 | 3,938,054 | **-18.86%** |
| 全路線 logic cycles | 640,692,542 | 616,279,153 | **-3.81%** |
| 全路線 collision cycles | 240,151,927 | 230,157,657 | **-4.16%** |
| 全路線 missed VBlank | 1,120 | 1,097 | **-2.05%** |
| 音訊 frame loss | 21 | 19 | -2 frames |

Boss 區間的 missed VBlank 仍為 34；原因是 render、滿載武器與其他邏輯
仍超出單一 VBlank 預算，但碰撞本身的成本已顯著下降，且 audio 在該
Boss 區間仍保持 112／112 physical frames。

## Active Directory 的直接證據

開啟 snapshot、只切換 active directory 時：

| Boss 指標 | 關閉 | 開啟 | 差異 |
|---|---:|---:|---:|
| collision cycles | 4,826,498 | 3,938,054 | **-18.41%** |
| logic cycles | 9,371,046 | 8,284,637 | **-11.59%** |
| linked-status visits | 25,100 | 1,255 | **-95.00%** |
| kill-group visits | 100 | 5 | **-95.00%** |
| damaged-transition visits | 100 | 5 | **-95.00%** |
| player-contact visits | 6,500 | 315 | **-95.15%** |

這些 visit 數從每次固定 100 格，變成當時約 5 個 live enemies；命中、
擊殺、邏輯位置與 Boss 區間完全相同。

## Snapshot ARM 核心的獨立效益

開啟 active directory、只切換 snapshot 時：

| 指標 | snapshot 關閉 | snapshot 開啟 | 差異 |
|---|---:|---:|---:|
| Boss collision cycles | 4,067,802 | 3,938,054 | **-3.19%** |
| Boss logic cycles | 8,414,421 | 8,284,637 | **-1.54%** |
| 全路線 collision cycles | 234,449,478 | 230,157,657 | **-1.83%** |
| 全路線 logic cycles | 620,543,844 | 616,279,153 | **-0.69%** |
| missed VBlank | 1,109 | 1,097 | **-1.08%** |

代價是 800 bytes EWRAM snapshot 與約 2 KiB ROM code。最終壓力 ROM 的
IWRAM stack canary 仍剩 3,448 bytes，高於專案 3,072-byte 門檻；因此
這項小幅但可重現的增益值得保留。

## 正確性驗證

- `level_port_asm_differential = 3`：通過全部 16-bit delta／12 組 radius
  的 axis 測試，以及 10 組 directed、128 組 deterministic random 的
  packed-collision C／ARM 全 state/result hash 比對。
- 測試涵蓋 linked group death、damaged graphic transition、iced/filter
  propagation、strict boundary、link-254、death spawn，以及 collision
  phase 內 geometry 改變後的 snapshot refresh。
- `enemy_mask_consistency = 7`：active、collidable 與計數目錄一致。
- 最終 Boss 壓力截圖未見 OAM、palette 或 Sprite2 cache 汙染。

## 為何不把整段 Boss 邏輯改寫成組語

外部建議中的 spatial grid／Boss 特例可以再降低 candidates，但會增加
同步結構與 source-order 風險。實測顯示最划算的做法是：只把規則穩定、
可做完整 differential test 的 miss kernel 留在 ARM；linked rewards、
death spawn、event jump 等分支繁多的 source semantics 留在 C，再用 exact
active directory 消除 100-slot 空掃。這保留逐行移植行為，也避免大型 ARM
函式侵占有限 IWRAM。
