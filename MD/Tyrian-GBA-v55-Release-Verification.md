# TyrianGbaPoc v55 完成與發布驗證

日期：2026-07-30  
建置設定：Detail Level `high`、Game Speed `normal`  
玩家無敵：關閉  
極限武器壓測配置：關閉

## 本版範圍

v55 完成下列四個彼此相關的改進：

1. 遊戲 HUD 依序顯示 `SHIELD`、`ARMOR`、`GENERATOR`，並使用接近
   PC 儀表板語意的藍、褐、金配色。
2. 補齊 Game Menu 的 Data Cube 動態物件、ROMFS Data 閱讀器與
   Ship Specs；資料直接來自 stock `cubetxt*.dat`、HDT 與原始
   Sprite2，不建立每章專用文字或船型 bitmap。
3. 保留 PC 版 264×184 戰鬥座標與 1:1 像素，以 24 像素 slack
   實作有 dead zone 和阻尼的 240×160 柔性裁切鏡頭。
4. 選單進關卡改為 VBlank 音量包絡轉場，並重新針對 GBA Maxmod
   校準 41 首 Tyrian 音樂。

各子系統的設計細節：

- `MD/Tyrian-GBA-Data-Ship-Specs-v55.md`
- `MD/Tyrian-GBA-Soft-Camera-Audio-Transition-v55.md`
- `MD/Tyrian-GBA-Maxmod-Music-Calibration-v55.md`

## 完整建置結果

執行：

```powershell
.\build.ps1 -KeepIntermediates -DetailLevel high -GameSpeed normal
```

結果為 PASS，包含：

- 正式 ROM header、大小與 600-frame release boot；
- Episode 1 主流程、死亡、勝利、Secret Level 與一般武器流程；
- Jukebox、Demo、Full Game 四關 campaign；
- Episode 2、3、4 第一關及 Arcade；
- 62 個關卡 section 的 ROMFS／HDT／LVL／Sprite2 矩陣；
- Data Cube、Ship Specs 與靜態選單轉場壓力測試；
- EWRAM、IWRAM、stack canary、Sprite2 L2 與背景快取回歸。

關鍵結果：

| 項目 | 結果 |
|---|---:|
| 主流程 `telemetry_pass` | 1 |
| campaign／Episode 2／3／4／Arcade | 全部 PASS |
| ROMFS self-test failures | 0 |
| Sprite2 decode failures／cache drops | 0／0 |
| Projectile cache drops | 0 |
| 前端所有轉場 missed VBlank | 0 |
| 主流程鏡頭 X 範圍 | 24..48 |
| 主流程鏡頭 Y 範圍 | 0..24 |
| 音訊轉場 fade-out／silent／fade-in | 18／1／30 VBlanks |
| 勝利音樂自然停止 | PASS |
| 死亡音樂自然停止 | PASS |
| Episode 2 gameplay missed VBlank | 32 / 10,475 frames |
| Episode 3 gameplay missed VBlank | 12 |
| Episode 4 gameplay missed VBlank | 28 |
| IWRAM stack guard | intact |
| 主流程最低剩餘 IWRAM stack | 4,756 bytes |
| 主流程剩餘 EWRAM heap | 24,576 bytes |

Episode 2／3／4 的 missed VBlank 全部位於遊戲關卡，沒有發生在前端、
死亡、統計或轉場；既有 drop-frame 機制仍保持遊戲邏輯時間軸。

## 音樂與重複資源稽核

- 41 首循環版模組供 Jukebox／一般播放。
- `09`、`10`、`30` 另有一次播放版，分別供 End Level、Game Over、
  Secret Level。
- loop 與 `_once` 的 PCM payload 完全相同，Maxmod soundbank 已自動
  全域去重；44 個 module 合計只保留 483 個 unique samples。
- 移除三個 `_once` 只節省 4,028 bytes，卻會失去自然停止語意，因此
  保留兩種 order-flow。
- 沒有同時打包舊版與新版 IT。舊版只存在 Git 忽略的本機
  `Backup/MusicA-B/`，正式 soundbank 僅包含新版校準資源。

新版 soundbank：

- 大小：1,505,368 bytes
- SHA-256：
  `3854aefd200aaa6531aac87be21f848da74ad22959bf0312e806aa5c879c92bc`

## 發布 ROM

- 檔名：`TyrianGBA.gba`
- 大小：26,348,524 bytes（25.128 MiB，32 MiB 的 78.52%）
- SHA-256：
  `196b40a48cc61a887079e6811b20ca3110a5be6066f8ecc356ed4c9f9894a4c3`

本機 `Backup/MusicA-B/TyrianGBA-v55-music-maxmod-calibrated.gba`
與完整回歸產物逐 byte 相同，SHA-256 亦相同。

舊音樂 A/B ROM：

- 大小：26,237,068 bytes
- SHA-256：
  `b48eb6c5bbc921ce304b3e763ac0016b5378e86f28add828d8a05dd2128c1c08`

兩個 A/B ROM 只保留於本機 Backup，不提交 source Git；正式可玩成果
依專案政策以 GitHub Release asset 發布。
