# ICESECRET 紅框物件、圖層與柔性鏡頭 Root Cause

日期：2026-08-12

## 結論

重新依使用者提供的三張紅框截圖追查後，這些外觀看似基地、圓頂與固定
砲台的圖形，確實都有 `enemyground=1`；但這個欄位在 Tyrian 不是「固定
於地圖」的旗標，而是由 HDT `explosiontype` 奇偶數拆出的地面型爆炸／
濾鏡屬性。

真正決定座標圖層的是 LVL spawn event：紅框中的 definition 130／132
由 PC 原始資料的 **event 15 `Sky Enemy`** 放入 enemy pool 0，PC 隨後又以
event 19／20／27指定速度、加速度與反轉值。因此它們是用敵人物件組成的
關卡演出；外觀看似建築，不代表它們是背景 tile 或靜止 map object。

本次也直接量測 GBA 的 OBJ／BG camera transform。結果顯示柔性鏡頭沒有
造成額外圖層漂移；把這些物件強制釘到 BG1 反而會偏離 OpenTyrian 與
AprCSTyrian 的原始流程。

## 原始程式與資料證據

### PC 繪製層

OpenTyrian 與 AprCSTyrian 都執行相同順序：

1. event 15 呼叫 sky-enemy pool 0；
2. sky pool 繪製時使用 `mapX2Ofs`；
3. `tempBackMove=0`；
4. event 19 改 `exc/eyc`；
5. event 20 改 `excc/eycc`；
6. event 27設定速度反轉邊界。

ICESECRET 的實際事件包括：

- time 10900～10986：反覆生成 definition 132，並對每組 link 設定
  `excc=+3/-3`、反轉值 `+8/-8`；
- time 11000～11025：從左右生成六個 definition 130，設定
  `exc=+4/-4/-5/+3/-4/+4`；
- time 11050～11075：再把其 Y 速度改為 `+8`。

所以畫面上的大幅滑入、移動與離場不是柔性鏡頭造成；GBA camera 的最大
位移只有 X/Y 各 12 pixel，也不可能產生截圖中跨越大範圍的位移。

### `enemyground` 的真正含義

PC `JE_makeEnemy()` 的來源是：

```text
enemyground = (explosiontype & 1) == 0
explonum    = explosiontype >> 1
```

它用於地面型爆炸、iced filter 等效果，不決定 enemy pool，也沒有 map
anchor 座標。因此不能用 `enemyground=1` 將所有物件改派到 BG1。

## 精確 camera／layer 量測

在同一段事件取兩個不同柔性鏡頭位置：

| 項目 | time 10910 | time 10930 |
|---|---:|---:|
| camera X | 0 | -3 |
| camera Y | -12 | -1 |
| MAP1 X offset | 17 | 21 |
| MAP2 X offset | 34 | 42 |
| BG1 HOFS | 67 | 60 |
| BG2 HOFS | 50 | 39 |
| BG1 raw／camera scroll | 8104／8092 | 8104／8103 |
| BG2 raw／camera scroll | 1329／1317 | 969／968 |
| definition 130 pool mask | pool 0 | pool 0 |
| definition 132 pool mask | pool 0 | pool 0 |
| deferred frame BG freeze | enabled | enabled |

兩次量測都滿足：

```text
BG2_HOFS + MAP2_offset - camera_X = 84
BG_camera_scroll - BG_raw_scroll = camera_Y
```

OBJ 的 screen transform 同樣減去相同 camera X/Y，因此 camera 項會完全
相消。Drop-frame 時 `FREEZE_BACKGROUND_ON_DEFER=1`，BG register 與 OAM
也維持同一個已提交的 presentation，不會一邊更新一邊停留舊幀。

## 修正決策

目前沒有套用「definition 130／132 固定到 BG1」的特判，原因是它會：

- 忽略 PC 明確的 event 15 sky layer；
- 破壞 MAP2 parallax；
- 抵銷 event 19／20／27 的演出；
- 讓 GBA 與 C／C# 原始版本產生新的規格差異。

如果產品方向確定要把這段改成 GBA 專屬的靜止地面建築，可以另做明確的
非 source-parity 規格；但那應視為改編，而不是修復目前的 camera bug。
