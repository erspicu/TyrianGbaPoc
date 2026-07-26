# Tyrian GBA v25：Boss 離場與過關統計逐行移植

更新日期：2026-07-26

## 目標

v25 補齊第一關 Boss group 清空後到 Game Menu 之間的 PC 流程：

1. 玩家飛機的 end-level 離場加速及殘影。
2. 真正的 End of Level 曲目。
3. `Level completed` 原始語音。
4. 逐段出現的 Cash、Enemies Destroyed、Cubes 與 Press a key。
5. 以原始 OPTION_SHAPES data cube 圖像逐一顯示收集結果。

本階段不處理下一關資料載入；完成統計後仍回到可操作的 Game Menu，
後續由多關卡階段接通 `nextLevel`。

## 離場流程

對照 OpenTyrian：

- `tyrian2.c` 初始化 `levelEnd = 255`、`levelEndWarp = -4`。
- 進入 `endLevel` 後，每 tick 先遞減 `levelEnd`，再遞增
  `levelEndWarp`。
- `JE_playerMovement()` 執行 `player.y -= levelEndWarp`。
- 殘影數量使用 `abs(41 - levelEnd)`，上限 20；間距為逐次增加的
  三角序列。
- `player.y < -200` 或 `levelEnd == 0` 才離開關卡。

舊 GBA 程式把 `player_end_warp` 宣告成 `u8` 並由 0 起跑，造成飛機立即
向上加速，也沒有 PC 開始時短暫下沉再上升的動作。v25 改回 `s8` 及
`-4` 初值。

PC 軟體 renderer 是後畫者覆蓋前畫者；GBA 則是較小 OAM index 在同
priority 重疊時勝出。因此 runtime 先保存 PC 殘影位置，再反向送入 OAM，
維持相同覆蓋順序。

第一關 deterministic 結果：

```text
initial levelEndWarp = -4
final levelEndWarp   = 27
visible trail peak  = 16
final player y      = -220
```

## End of Level 音訊

OpenTyrian 在存活玩家進入 `endLevel` 時執行 `play_song(9)`。v25 使用
TyrianAudioLab 的 `10_end_of_level.tym`，轉換後 IT 長度約 5.575 秒，
並以 Maxmod 循環播放直到離開統計畫面。

這首短曲只有 7 個有效 SuperNintendo 校準聲道。共用 tracker writer
固定要求 8 個 source，因此 GBA builder 保留全部 7 組 source/gain，
第八組使用不可能匹配 TYM event 的 sentinel；沒有複製、遺漏或重映射
實際聲部。

切換 End 曲時只停止上一個 module，不取消仍在播放的爆炸 effects，
對應 PC `play_song()` 與 sample mixer 分離的行為。

`V_LEVEL_END` 是 `voices.snd` 的第 5 筆 voice（zero-based entry 4）。
依 `nortsong.c` 移除尾端 100 bytes 壞資料後，以 11,025 Hz PCM 建入
soundbank。進入統計畫面時播放一次，End 曲保持在背景。

## 統計畫面

舊版一次顯示所有文字，並以自訂 `Collected` 代替 cube。v25 改為下列
狀態：

| Stage | 顯示內容 |
|---:|---|
| 0 | Completed Tyrian |
| 1 | Cash 與真實累計金額 |
| 2 | Enemies Destroyed 與原版四捨五入百分比 |
| 3 | Cubes；無資料時顯示原文 None |
| 4 | Press a key |

每個文字 stage 間隔 30 個 VBlank。Full Game 的 cube stage 會每 18 個
VBlank 顯示一顆並播放一次 `S_ITEM`，最多依 PC `cubeList[4]` 顯示四顆。
Arcade 模式依原版略過 Cubes。

Cube 不是新畫素材：

- 來源：`tyrian.shp` 的 `OPTION_SHAPES` sprite 25。
- 繪圖：兩個 dark shadow，加上 hue 9 foreground。
- 離線產生 19 × 22、透明值為 `0xff` 的 Mode 4 stamp。
- runtime 依 PC `x = 20 + 30 * cubeIndex`、`y = 135` 的縮放座標逐顆
  疊到 stats scratch frame。

若玩家在動畫中按鍵，會快速完成剩餘 stage；cube 仍會顯示並播放 item
效果，再等待下一次按鍵離開，對應 PC 把 `frameCountMax` 切到 0 而不是
直接捨棄剩餘內容。

## 回歸結果

| Detail | Game Speed | 結果 | 邏輯更新 | 遊戲 display frame |
|---|---|---:|---:|---:|
| Low | Normal | PASS | 7,832 | 13,509 |
| Normal | Normal | PASS | 7,832 | 13,509 |
| Low | Low | PASS | 7,832 | 16,872 |
| Normal | Low | PASS | 7,832 | 16,872 |

四組皆確認：

- 935 個來源事件及 100 個敵人擊破。
- Boss group 清空，最後位置 6481。
- End 曲啟動一次。
- Level completed voice 啟動一次。
- 四次 stats stage 推進。
- 一顆 data cube 顯示及一次 item effect。
- 最大 OAM 89/128。
- map stream、reward、Sprite2 decode 與 ROMFS self-test 零失敗。

預設 Low Detail／Normal Speed release 為 11,978,840 bytes，使用標準
32 MiB GBA ROM 空間的 35.70%。

## 下一階段

依 Updated Plan 進入 P2：

- 玩家死亡每 tick 的雙大型爆炸。
- `S_EXPLOSION_9`／`S_EXPLOSION_11` 隨機 cadence。
- 關卡音樂 fade。
- 在最後 gameplay 畫面上顯示 PC 風格 `GAME OVER`。
- 真正 Game Over 曲目與按鍵返回 Game Menu。
- 保留可切換的開發無敵旗標。
