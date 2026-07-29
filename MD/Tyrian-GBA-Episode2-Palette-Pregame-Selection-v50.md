# Tyrian GBA v50：Episode 2 貼圖、前置字形與章節切換修正

日期：2026-07-30

## 1. Episode 2 第一關貼圖問題

### 根因

問題並不是 `tyrian2.lvl`、Shape 編號或 ROMFS 索引讀錯。
逐筆解析 Episode 2 第一關的 LVL 與 `shapesx.dat` 後，GBA 取得的
原始 8-bit 像素資料和 PC 版相同。

實際錯誤發生在 GBA 4bpp 背景轉換：舊程式每張 8x8 tile 只能挑選
一個主 hue 的 16 色 bank。TORM 場景的岩壁、水面與陰影會在同一張
tile 內同時使用灰、藍、黑等色系，舊演算法把其他色系壓進主 hue，
因此雖然圖形索引正確，視覺上卻像是讀到了完全不同的貼圖。

### 修正

- 仍由 runtime 直接讀取原始 `LVL` 與 `shapes*.dat`。
- build 階段掃描五個原始 Shape bank，建立全遊戲共用的
  source-hue-aware 4bpp 調色盤。
- 保留 11 組單 hue bank；另以所有原始 Shape tile 訓練 5 組
  混合材質 bank。
- 建立完整的 `hue mask -> palette bank` ROM 查表，runtime
  不再逐 bank 計算權重。
- 沒有加入 Episode 2 專用圖、關卡專用 palette 或手工 tile
  對照表，因此相同修正能套用到後續所有 Episode。

build 一致性檢查：

- Shape bank：5
- 訓練 tile：25,323
- 實際 hue mask：191
- TORM 關鍵 mask `0x1004`、`0x1005`：均映射至 mixed bank 14
- 關卡專用表：0

### 驗證

以 Episode 2 / Section 1 自動路由，於關卡位置 50 產生 mGBA
硬體畫面截圖。原本紅框內被壓成綠色或單一藍色的岩壁、水道與黑色
陰影已恢復成可辨識且接近 PC 原始畫面的混合材質。

同一位置也完成 Episode 1、3、4 第一關的畫面回歸截圖；其雪地、
岩石、金屬與沙地材質沒有因共用 palette adapter 發生明顯色系
崩壞。

診斷截圖保存在忽略版本控制的：

`temp/phase50_ep2/episode2_fixed_pos50.png`

`temp/phase50_ep2/episode{1,3,4}_palette_regression_pos50.png`

## 2. 前置選單字形

- 前置選單字形由舊 5x8 加程式性橫向加粗，改成手工定義的
  mixed-case 6x8 筆劃。
- 最大字寬改為 8 pixels，讓 `M/W/m/w` 能保留足夠內部空隙。
- 小寫 `m` 不再被橫向加粗合併成實心方塊。
- runtime 和 build-time renderer 使用完全相同的字形資料與
  advance 規則。

驗證截圖：

`temp/phase50_ep2/title_font_fixed.png`

## 3. Select Episode 切換爆音

對照 OpenTyrian，選項上下移動使用 `S_CURSOR`（sound 28）。

舊 GBA 流程在每次選項移動時重新還原並呈現整張 prebuilt frame，
造成不必要的大量畫面複製，容易讓音訊呈現短暫中斷。現在改為：

1. 預先選取對應 selection 的完整靜態 frame。
2. 只 dirty-patch 舊選項列與新選項列。
3. 選項移動後播放原版 `S_CURSOR`。

因此按上下鍵不再要求整張 240x160 frame 重繪，並補回 PC 版游標音效。
