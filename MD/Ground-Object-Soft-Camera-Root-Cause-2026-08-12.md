# 地面附著物件與柔性鏡頭 Root Cause

日期：2026-08-12

## 結論

這次現象包含兩種外觀相近、但 PC 資料語意完全不同的物件，不能用
`enemyground` 或「看起來像建築」一概固定：

1. `ICESECRET` 第一 Boss 後的灰色圓頂／紫色支架是 enemy definition
   130／132，從 **sky pool** 生成；LVL 的 event 19、20、27 明確設定其
   X/Y 速度、加速度與反轉幅度。它們在 PC 版原本就會移動。
2. 砲台、基地等真正附著 MAP1 的結構由 event 12 等事件放入
   **ground pool 25／75**。它們才應與 BG1 保持完全固定的相對位置。

因此，若把 definition 130／132 因外觀像建築而鎖在背景，反而會破壞
PC 關卡流程。可靠的 attachment authority 是 enemy pool 加上原始運動
欄位，不是圖像外觀，也不是 `enemyground`。

## ICESECRET 實測

以目前 LOW／NORMAL speed、Episode 4 Section 50 LVL 20 跑過雙 Boss，
在回捲後的 position 216～278 量測 event 12 結構：

- 連續 ground attachment 樣本：222；
- 其中柔性鏡頭座標正在變動的樣本：84；
- 水平 attachment invariant 失敗：0；
- 垂直 attachment invariant 失敗：0；
- 最大相對位移：X=0、Y=0 pixel。

量測的不變量為：

```text
horizontal anchor = draw.x - presentation_map_x_offset
vertical anchor   = draw.y + BG1 presentation scroll
```

柔性鏡頭在 OBJ 端相減、在 BG 端相加，兩者會精確抵消；目前 ICESECRET
真正的 ground-pool 砲台／基地沒有被 camera 拖動。

## 2026-08-15 補充校正

後續針對使用者紅框內 event 12 definition 80～83、87～90 再檢查時，發現
上述概念式仍少表達一個邊界條件：BG1 在地圖頂／底會夾限捲動，但舊測試
使用的是尚未夾限的 camera 要求值。runtime 與測試現已統一使用 BG1
真正呈現的 camera delta；3,600 VBlank 路線取得 1,262 個樣本（530 個在
鏡頭移動中），X/Y 誤差與失敗數均為 0。

紅框中可見的矩形接縫主要不是座標漂移，而是 Sprite2 圖塊自帶不透明冰地
像素，經 GBA BG／OBJ 分域量化後產生的色差。完整結論與全資源色盤修復見
`ICESECRET-Ground-Structure-Palette-and-Camera-Fix-2026-08-15.md`。

## 另外找到的真實通用缺口

逐行比對 `vendor/opentyrian/src/tyrian2.c` 後，發現 GBA adapter 對
event 3 的延遲捲動仍有 source-parity 缺口：

- PC 在 `map1YDelayMax > 1 && backMove < 2` 時，會先依目前 delay phase
  把本 tick 的有效 `backMove` 改成 0 或 1；
- 同一個有效值同時供 BG1 與 ground enemy pool 使用；
- GBA 目前讓 BG1 使用自己的 delay counter，ground pool 卻固定使用原始
  `state->back_move`。

結果是採用 event 3 的關卡中，BG1 不動的兩個 tick，靜態地面 OBJ 仍各走
1 pixel，確實會累積成「建築物從背景上滑動」的現象。全四章共有 6 個
LVL 使用 event 3，其中 5 個同時大量生成 ground 結構。

這個缺口不是本次 ICESECRET definition 130／132 移動的原因，但它會在
其他關卡造成使用者所描述的同類症狀，下一步會以 PC 的單一 delay phase
修正，並保留 ground attachment invariant 回歸檢查。
