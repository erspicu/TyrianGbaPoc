# PAUSED 色盤改善評估（2026-08-13）

## 最終結論

同幀放大比對後，使用者實際在意的不是褐色基調，而是字內零星的高飽和
紅點。逐行追到 OpenTyrian 後確認：`JE_dString()` 刻意呼叫
`blit_sprite_hv_unsafe(..., hue=15, value=-3)`；非常暗的 FONT_SHAPES
像素會由負值回捲成 palette index `0xfe/0xff`。PC 的 8-bit hue-15 色盤
只呈現很小的紅色明暗變化，但 GBA 將整套字色 nearest-map 到背景的
mixed-material bank 後，這些少數像素會命中飽和紅色槽，視覺上變成突兀
紅點。

正式修正不再保留 BG bank 15，也不覆寫任何 palette RAM。它沿用目前可
接受的褐色 bank，只把「紅色分量大於綠色兩倍」的離群映射，改配到同一
bank 中亮度最接近的暖中性色。背景 bank、tile、調色盤訓練結果皆不變；
因此不會為修一行文字犧牲關卡背景品質。

## 初始問題分析

目前 `PAUSED` 偏褐、偏髒且會隨關卡改變，根因不是字形大小或座標，
而是色盤所有權設計：PC 的 `JE_dString()` 使用 hue 15、brightness -3
的原始漸層與 `(x+2, y+2)` 暗影；GBA BG palette 的 bank 0–10 保留
hue 0–10，bank 11–15 則全部供背景 mixed-material 訓練使用，沒有任何
bank 能精確表示 hue 15。

`gameplay_overlay_prepare_palette()` 現在把 PAUSED 的 15 個來源色，整組
nearest-map 到某個既有 mixed bank。這只能找「誤差較小」的錯色，無法
得到固定的 PC 金橙色漸層，因此在冰地、岩地等不同關卡會出現不同的
褐色、髒色或陰影對比不足。

## Gemini 3.1 Pro 諮詢後的定案排序

1. **優先原型：建置期保留固定 UI bank**
   - 將 BG bank 15 固定為 PC hue-15／PAUSED 原始色盤。
   - 背景 mixed bank 從五組減為四組，並禁止背景 mixed-mask 分派到
     bank 15。
   - 重新執行既有 OKLab、CIEDE2000、palette collision 與關卡截圖回歸。
   - 若背景退化不可見或在門檻內，這是最可靠的正式方案：PAUSED 每關
     都是相同正確顏色，沒有 runtime 租借、OAM、OBJ cache 或恢復時序。

2. **若固定 bank 造成可見背景退化：可見 bank 交易式租借**
   - 使用 renderer 維護的 16-bit visible-bank usage mask，或暫停時只掃描
     Mode-0 可見 tilemap。
   - 只選擇目前 BG0–BG3 可見範圍完全沒有引用的 bank。
   - VBlank 內備份 32 bytes、上傳精確 PAUSED 色盤，解除暫停時先恢復
     色盤，再讓 camera／tilemap／gameplay 繼續。
   - 若 16 個 bank 全部在畫面上使用，禁止覆寫，改走 fallback。

3. **安全 fallback：非破壞式 2／3 色映射**
   - 不再平均匹配全部 15 階顏色。
   - 只選明亮前景、中間色與暗影三個角色，使用實際像素頻率加權，且
     把前景與暗影的亮度差列為硬性條件。
   - 顏色不一定完全等於 PC，但會比目前的泥褐漸層穩定且清楚。

## 明確否決

- 不採用 Gemini 初稿提出的「直接覆寫 bank 15，再用全畫面暗化遮掩」。
  這會故意讓引用 bank 15 的背景 tile 變色，重新引入色盤汙染。
- 不以全畫面 dim 或半透明面板代替字色修復。暗化只能是字色正確後的
  選配視覺效果。
- 不退回 PAUSED OBJ；它會重新占用 OAM／OBJ VRAM，並與 boss compact
  cache 的 palette bank 發生所有權衝突。
- 暫不把字形尺寸納入第一階段修改；使用者本輪指定色盤才是主問題。

## 原型評估後的取捨

原先規劃的「bank 15 固定 hue-15」不再實作：真正需要處理的只是
`0xfe/0xff` 回捲色在 mixed bank 中被放大的紅色離群點，永久拿走一組
背景 bank 代價過高。visible-bank lease 亦暫不需要，因為同 bank 的
暖中性色重映射已能在不碰 palette 所有權的前提下排除紅點。

Gemini 原始回覆保留於：

- `knowledgebase/message/TyrianGbaPoc-pause-presentation-gemini-2026-08-13.md`
- `knowledgebase/message/TyrianGbaPoc-pause-palette-gemini-followup-2026-08-13.md`
- `knowledgebase/message/TyrianGbaPoc-pause-palette-gemini-final-2026-08-13.md`
