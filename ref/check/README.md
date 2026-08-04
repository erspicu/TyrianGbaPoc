# Load / Save background check

- `title_load_pic2_clean.png`：直接由原始 `tyrian.pic` 的 PIC 2 解碼並縮放成 240x160，完全沒有選單文字。
- `title_load_static_base_no_slots.png`：GBA 首頁 Load 實際使用的靜態底圖；只有來源大型標題與 GBA 操作提示，沒有 11 列存檔資料。

逐像素檢查 `y=24..148`（11 個存檔槽所在區域）後，兩張圖的差異像素數為 **0**。因此先前看見的列文字「疊影」不是背景烘入舊文字，而是 runtime 將一份固定色的字形偏移到 `(x+1,y+1)` 當陰影；這不符合 `mainint.c::JE_loadScreen()` 使用的 `JE_textShade(..., FULL_SHADE)`。

修正版改為依 PC 原始碼順序，在字形四周對既有背景做亮度減半，再繪製一次主字形，不再畫第二份彩色字。

## 修正後擷取

- `title_load_fullshade_fixed.png`：首頁 Load 的 11 列改為來源 `FULL_SHADE` 規則後的畫面。
- `options_load_left_fixed.png`：Options > Load，左側飛船／金額／儀表不再被存檔列清背景破壞。
- `options_save_left_fixed.png`：Options > Save，同一項左側保護修正。

Options 的問題是 runtime 原先為每一個槽位回復一條寬 240 pixels 的 PIC 1 背景；12 次更新會把左側即時船體畫面逐列抹除。現在 Game Menu context 僅回復與提交右側 `x=120..239`，首頁 Load context 才維持全寬列更新。
