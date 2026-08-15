# 地面附著物件與柔性鏡頭修復完成報告

日期：2026-08-12

## 修復摘要

本次沒有把「看起來像建築」的 enemy 一律固定，而是依 PC 原始資料的
真正語意處理：

- ground pool 25／75 的靜態結構，與 MAP1 共用同一個捲動相位；
- sky pool 或具有原始速度／加速度的物件，繼續依 PC LVL 事件移動；
- 柔性鏡頭只作最後呈現座標轉換，不改變敵人世界座標。

這可避免把 ICESECRET 中原本就會移動的 definition 130／132 誤鎖死，
同時修復其他關卡在 event 3 延遲捲動期間，地面建築與背景逐步分離的真實
問題。

## 程式修正

### 1. 共用 PC 的有效 `backMove`

`src/level_port/level_port_enemy_motion.inc` 新增
`ot_effective_ground_back_move()`。當 PC event 3 啟用三 tick 一步的
MAP1 捲動時，ground pool 使用與背景相同的 `0, 0, 1` 移動序列，不再
每個 tick 都錯誤移動 1 pixel。

一般 `map1_y_delay_max == 1` 的關卡仍直接使用原始 `back_move`，沒有額外
降速或行為改變。

### 2. 背景與 source runtime 共用 delay phase

`source_apply_background_state()` 每 tick 接收 source event interpreter 的
當前 delay phase；`advance_backgrounds()` 消耗後再寫回。event 2／3 在 PC
原始碼中對 phase 的立即重設，因此也能在 GBA 同一 tick 生效。

### 3. 增加可重複的 attachment invariant

壓力測試版會追蹤無原始運動欄位的 ground-pool 物件：

```text
horizontal anchor = draw.x - presentation_map_x_offset
vertical anchor   = draw.y + BG1 presentation scroll
```

測試工具會把失敗數與最大 X/Y 誤差寫入 telemetry，之後若 camera、背景或
enemy transform 再度分歧，測試會直接失敗，不需只靠肉眼等待偶發重現。

## 實機路線驗證

條件：LOW detail、NORMAL game speed、Episode 4 Section 50（ICESECRET）、
release loadout、2,900 VBlank。

| 項目 | 結果 |
|---|---:|
| 關卡位置 | 278 |
| BG2 pointer wrap 次數 | 1 |
| ground attachment 樣本 | 222 |
| 柔性鏡頭變動中的樣本 | 84 |
| attachment 失敗 | 0 |
| 最大 X 誤差 | 0 pixel |
| 最大 Y 誤差 | 0 pixel |
| 關卡素材有效 | 通過 |
| 音樂持續播放 | 通過 |

這證明真正的地面砲台／基地在柔性鏡頭移動時仍牢固貼附於背景。畫面中
若仍看到 definition 130／132 等物件移動，屬於 PC LVL event 明確配置的
敵人動作，而不是 camera 漂移或資源污染。

## 2026-08-15 後續修正

柔性鏡頭呈現值已改為由 BG1 套用地圖邊界夾限後的實際 scroll delta
產生，OBJ、BG1、BG2 與 attachment 測試共用同一個值。另以完整 normal
與 Christmas Sprite2 raw catalog 訓練 16 組 hue-specific brightness
medoid，在不增加 RAM／VRAM／OAM 與執行期 pack 成本下，將全資源加權
RGB 色差降低 34.13%。詳見
`ICESECRET-Ground-Structure-Palette-and-Camera-Fix-2026-08-15.md`。

## 正式 ROM

- Detail level：LOW
- Game speed：NORMAL
- 檔案：`build/TyrianGBA.gba`
- 大小：26.75 MiB
- SHA-256：`df8d5505f1eed61c3d6621947f0fc16cc44f2c061192298b0aa874ce42ba3258`
