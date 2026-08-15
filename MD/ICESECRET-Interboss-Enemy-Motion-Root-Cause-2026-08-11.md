# ICESECRET 雙 Boss 中場物件移動 Root Cause

日期：2026-08-11

## 結論

`build/TyrianGBA.sav` 目前路線會進入 Episode 4、Section 50、
`ICESECRET`（`tyrian4.lvl` 的 LVL 20）。使用者截圖紅框中的灰色圓頂、
紫色腳狀物件不是背景 tile，也不是 Boss 擊破後被錯誤移動的地面建築；
它是關卡資料中的 **enemy definition 130**，屬於可被攻擊的 Sprite2 敵人。

這一段移動是 PC 原始關卡刻意設計的雙 Boss 中場流程，不是 GBA adapter
新增的行為，因此不能把它修成固定物件，否則反而會破壞 PC source parity。

## 原始資料證據

第一組 Boss 使用 link 189：

- event time 10600：生成 Boss 組件 412～415、383、384；
- event time 10700：event 79 顯示 link 189 Boss 血條；
- event time 10650／10750：event 70 在 link 189 消失後跳至 10900；
- event time 10800：Boss 尚存時，event 54 跳回 10601，維持戰鬥迴圈。

第一 Boss 擊破後，原始 LVL 接著安排：

- event time 10900～10986：生成並移動 definition 132 的中場物件；
- event time 11000～11025：以 event 15 從左右生成六個 definition 130；
- 同時間的 event 19 明確賦予 X 速度 `+4/-4/-5/+3/-4/+4`；
- definition 130 自身還有 Y 速度，所以物件會緩慢橫向、向下移動；
- event time 11100 起才開始建立下一組大型組件。

`ICESECRET` 在 time 0 另以 event 53 啟用 `forceEvents`。OpenTyrian
`tyrian2.c` 的原始規則是在 `backMove == 0` 時仍執行 `curLoc++`，因此主
背景停住時，中場事件仍會依序播放。GBA 的 `force_events`、event 15、
event 19、event 70、event 54，以及四個 enemy pool 更新公式均與 PC 對上。

## A/B 與排除項目

- LOW／NORMAL 在同一位置的 A/B 截圖一致，並非 LOW 漏畫第二背景層造成。
- `background2over == 3` 在 LOW 下強制恢復 BG2 的 PC 特例已存在且有生效。
- 測試中 link 189 可正常消失並跳到 10900，並非殘留 Boss 組件卡住流程。
- 紅框輪廓與 definition 130 的四片 Sprite2 組圖完全吻合。

## 處理原則

不修改 definition 130 的速度、pool 或事件資料。後續只加入針對這段
source-authored 流程的回歸檢查，鎖定以下條件：

1. link 189 死亡後跳至 10900；
2. `forceEvents` 在 `backMove == 0` 時維持事件時間軸；
3. 11000 起的 definition 130 仍是移動敵人，而不是背景建築；
4. 下一組 Boss 事件仍能正常接續。
