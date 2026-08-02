# UNDERDELI 粗顆粒畫面：blur／Mosaic 根因與修正

日期：2026-08-02

## 問題重現

以 `問題log/TyrianGBA.sav` 的第 2 個可見存檔槽進入下一關：

- Episode 4、script section 46 `UNDERDELI`
- `tyrian4.lvl` physical section 19
- music 35
- Normal detail、Normal game speed

關卡一開始，背景、玩家、敵人與投射物都呈現明顯 2×2 粗顆粒。

## 根因

關卡原始事件 index 6 在 `eventtime=0` 執行 type 64：

```text
eventdat=4, eventdat2=1, eventdat3=3
```

這會啟用 OpenTyrian 的 `smoothies[4-1]`。PC 原始流程在
`tyrian2.c` 呼叫 `blur_filter(game_screen, VGAScreen)`；
`backgrnd.c::blur_filter()` 對目前中間畫面與前一影格逐像素平均低 nibble
亮度，並保留目前來源像素的色相。它是跨影格的平滑合成，不是放大像素。

GBA 舊適配把同一請求映射成：

- BG0／BG1／BG2 設 `BG_MOSAIC`
- world OBJ 設 `ATTR0_MOSAIC`
- `REG_MOSAIC=0x1111`

因此硬體直接把所有 world 畫素放大成 2×2 方塊。這不只與 PC blur
語意不同，也錯誤影響 PC 流程中在 blur pass **之後**才繪製的玩家與前景
物件。畫面粗化不是 ROMFS 索引、調色盤訓練或背景 streaming 失敗。

## 修正

Mode 0 沒有保留上一個完整 world framebuffer；要逐像素重建 PC temporal
average，必須改用 bitmap framebuffer 或額外保存並軟體合成整個場景，會破壞
目前三層 tilemap、128 OAM 與 drop-frame 架構的效能平衡。

本次採取無失真 fallback：

1. 保留 type 64 事件、detail gate、`source_detail_blur_requested` 與 telemetry。
2. 移除三個 BG 的 `BG_MOSAIC`。
3. 移除 world OBJ 的 `ATTR0_MOSAIC`。
4. gameplay 中固定 `REG_MOSAIC=0`。
5. Normal detail adapter self-test 新增「blur request 存在，但三層 BG 與
   Mosaic register 必須保持關閉」的回歸條件。

這個選擇不宣稱逐像素重現 PC blur；它優先保留原素材解析度與正確物件
繪製階段，比錯誤的粗粒 Mosaic 更接近 PC 實際觀感。

## 驗證結果

### 同位置截圖

- 修正前：`temp/issue_slot2_underdeli_grain_v70/gba_p1000_normal.png`
- 修正後：`temp/issue_slot2_underdeli_grain_v70/gba_p1000_normal_no_mosaic.png`

修正後背景回復單像素紋理，玩家、敵人與投射物不再被 2×2 方塊化。

### 完整 route smoke

使用同一存檔完成 section 46：

- schema `TGRS` v3、`pass=1`
- `assets_valid=1`
- final level position 7130
- background stream drops 0
- background cache approximations 0
- Sprite2 decode failures 0
- Sprite2 cache drops 0
- projectile cache drops 0

因此本次問題與舊的 UNDERDELI 高速 row streaming 根因可以明確分離。

### Detail adapter regression

Episode 4 section 46、Normal、300 VBlank、壓力武器：

- `detail_adapter_self_test=1`
- `detail_blur_frames=231`，證明來源 blur 事件仍有執行與記錄
- `background_approximations=0`
- `source_assets_valid=1`
- audio frame loss 1／300（0.3333%）

