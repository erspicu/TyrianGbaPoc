# ICESECRET 雙 Boss 中場流程驗證完成

日期：2026-08-11

## 處理結果

本次沒有修改 enemy definition 130 的移動、事件或圖層。逐項核對後確認，
紅框物件是 PC 原始 `tyrian4.lvl` 安排的移動敵人波次；若將它固定在地圖，
會讓 GBA 版偏離 OpenTyrian／Tyrian 原始規格。

已完成的回歸核對：

- 目前 SAV 正確解析為 Episode 4、Section 50、LVL 20 `ICESECRET`；
- 第一 Boss link 189 可被擊破，event 70 能跳至 10900；
- event 53 的 `forceEvents` 在主背景停止時仍推進 `curLoc`；
- 10900～11025 的中場事件可繼續生成 definition 132／130；
- definition 130 由 event 15 放入 sky-enemy pool，event 19 設定左右移動；
- 下一組大型敵人／Boss 的 11100 以後事件仍可接續；
- LOW 與 NORMAL 的同位置 A/B 沒有背景層遺失差異；
- LOW 正式 ROM 已重新建置，原測試 SAV 已保留。

## 正式產物

- ROM：`build/TyrianGBA.gba`
- Detail Level：LOW
- Game Speed：NORMAL
- 大小：26.75 MiB
- SHA-256：`fbd442f94cce445fb0fd0760a4d6e56004ee064e225b4ebdfde985351a1aa627`

## 結論

這不是 GBA 的物件座標污染、背景停止失效、Boss 跳轉錯誤或 OAM 問題；
它是原始關卡用「停止主背景、讓事件時間軸與敵人波次繼續」製作的雙 Boss
中場演出。為保持成熟移植版的 source parity，本次採取的正確修復決策是
**保留原行為，不加入錯誤的固定物件特判**。
