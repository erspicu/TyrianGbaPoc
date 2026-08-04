# Tyrian GBA Load / Save 重畫修正（v77）

日期：2026-08-04

## Options > Load / Save 左側破圖

`game_menu.c::MENU_LOAD_SAVE` 的存檔列只屬於 PIC 1 的右側面板；左側飛船、金額、護甲與護盾是同一頁上的即時資料。

舊 GBA 程式在每列文字改色前，以寬 240 pixels 的背景列清除整個畫面。11 個存檔槽加上 Exit 共 12 列，會依序把左側即時船體畫面抹成水平背景條帶。

修正版：

- 首頁 `JE_loadScreen()` context 仍使用全寬 PIC 2 列更新。
- Game Menu `MENU_LOAD_SAVE` context 只回復與提交 `x=120..239` 的右側面板。
- 左側飛船、金額與狀態不再被存檔列更新碰觸。
- Save autotest 新增左側 120 pixels 不變性檢查。

## 首頁 Load 的 11 列疊影

導出的 `ref/check/title_load_pic2_clean.png` 與實際靜態底圖 `title_load_static_base_no_slots.png`，在存檔列範圍 `y=24..148` 的差異像素數為 0；背景本身沒有存檔文字。

問題來自舊 runtime 把第二份固定色字形畫在 `(x+1,y+1)` 當陰影。PC 來源 `mainint.c::JE_loadScreen()` 實際呼叫 `JE_textShade(..., FULL_SHADE)`，會在字形上、下、左、右將既有背景亮度減半，再繪製一次主字形。

修正版改為同一個四向暗化順序，並恢復來源欄位色系：

- 存檔名稱：hue 13（青色系）。
- Last Level／Episode：hue 5（紫色系）。
- 已選取、一般存檔與 EMPTY SLOT 分別套用來源亮度差。

## 驗證

- `ref/check/options_load_left_fixed.png`
- `ref/check/options_save_left_fixed.png`
- `ref/check/title_load_fullshade_fixed.png`
- Save autotest：`Pass=1`、`Failures=0x0000`。
- 正式建置：Detail Level Normal、Game Speed Normal。
