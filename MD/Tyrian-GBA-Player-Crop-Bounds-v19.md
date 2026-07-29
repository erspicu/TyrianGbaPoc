# Tyrian GBA v19 玩家飛機裁切邊界修正

更新日期：2026-07-26

## 問題

v18 已把 PC 版 264×184 戰鬥區改成 GBA 240×160 的 1:1 中央裁切，
但仍沿用 OpenTyrian 玩家中心座標的垂直邊界 `Y=10..160`。這個範圍是
為完整 PC viewport 設計；套到上下各裁掉 12 pixels 的 GBA viewport
之後會出現：

- 飛到最上方時，機頭超出畫面。
- 飛到最下方時，尾翼超出畫面。

本次不縮小飛機、不改背景或敵人座標，也不把 GBA presentation 座標
寫回 gameplay。只依最終可見 viewport 收窄玩家能到達的 PC source
座標範圍。

## 來源圖與座標推導

玩家 neutral graphic 233 與 right-bank graphic 235 都是 24×28
Sprite2 composite。建置器會固定驗證兩張圖的 alpha bounding box：

| Graphic | 24×28 source alpha bbox |
|---|---|
| 233 | `(3, 2)..(20, 26)` |
| 235 | `(5, 2)..(20, 26)` |

兩張圖的實際垂直像素範圍相同，都是半開區間 `[2, 27)`。PC 版的玩家
繪圖起點為：

```text
drawY = playerY - 7
```

GBA 顯示 PC gameplay `Y=12..171`，因此完整飛機必須滿足：

```text
上界：playerY - 7 + 2  >= 12
下界：playerY - 7 + 26 <= 171
```

解得新的安全範圍：

```text
playerY = 17..152
```

相較於舊範圍 `10..160`，上方內縮 7 pixels、下方內縮 8 pixels。
水平範圍仍是 `X=40..256`；24-pixel 玩家圖在左右各裁 12 pixels 後仍
能完整顯示，不需要修改。

## GBA OAM 錨點修正

玩家 24×28 source cell 固定放在 32×32 OAM container 的 `(4,2)`。
舊的 `source_player_screen_y()` 使用 `playerY + 8`，比 PC
`playerY - 7` 的 source draw origin 低 1 pixel。v19 改成：

```text
OAM top =
    playerY + 7
    - cropY(12)
    - container centre(16)
```

加上 container Y offset 2 後，24×28 source cell 的頂端正好等於
PC `playerY - 7` 經裁切後的位置。

程式中的 compile-time assertions 另外鎖定：

```text
playerY=17  -> 第一個不透明 pixel 位於 GBA y=0
playerY=152 -> 最後一個不透明 pixel 位於 GBA y=159
```

所以兩個極限都會貼齊畫面，但不會裁掉機頭或尾翼。

## 回歸驗證

除一般完整路線外，另建了兩個僅供截圖的 auto-test 變體：

- 持續按 `UP` 到 `playerY=17`，確認機頭輪廓完整且最上列仍可見。
- 初始位置向下 clamp 到 `playerY=152`，確認尾翼輪廓完整且最下列仍可見。

兩張 240×160 framebuffer 均通過人工檢查。正式 deterministic route
因原本測試駕駛長時間貼著舊上界，收窄邊界後會改變敵彈瞄準、碰撞及
RNG 消耗；這是 gameplay 路線合理變動，不是遙測誤差。v19 已重新鎖定
整條路線的 golden telemetry。

| 項目 | v19 結果 |
|---|---:|
| ROM internal／host verifier | PASS／PASS |
| Telemetry schema | 17 |
| 玩家垂直 source 邊界 | 17..152 |
| 最終 PC player x/y | 77／17 |
| Logic／display frames | 7,093／12,239 |
| Missed VBlank | 54（約 0.44%） |
| Source events applied／deferred／skipped | 869／5／4 |
| Event spawn success／pool full／missing | 473／0／0 |
| Source enemy shot spawn／release／drop | 185／185／0 |
| Stream／effect／reward／frame-cache drop | 0／0／0／0 |
| ROMFS checks／failures | 93／0 |
| Runtime errors | 0 |

正式 ROM：

```text
build/tyrian_gba_level1_source_parity_crop1to1_playerbounds_romfs_v19.gba
10,883,584 bytes
SHA-256 04b664d9fb48b0a73363d9b8b0b1f0fc9028fb07ae39ef7632d1de9d284839b5
```

## 保留的移植邊界

這是 GBA viewport adapter 的必要差異，而不是改寫第一關座標演算法。
敵人、背景、子彈、碰撞及 parallax 仍完整使用 PC source space；只有
玩家 movement clamp 因 GBA 看不到 PC viewport 上下各 12 pixels 而採用
可見安全範圍。`curLoc=5400` 之後的簡化 Boss 仍是目前最大的 source-
parity 邊界。
