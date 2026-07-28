# 本階段前端對照圖

日期：2026-07-28

此目錄只保留本階段實際需要的 PC／GBA 對照畫面，不擴張到 GBA
不需要的全部 PC 設定頁。

## PC OpenTyrian 參考

- `pc_game_menu.bmp`：Game Menu 船艦、金額、裝備與選項配置。
- `pc_upgrade_menu.bmp`：Upgrade Ship 七種裝備分類與 Done。
- `pc_upgrade_ship_submenu.bmp`：Ship Type 子選單、物品圖、價格與
  owned 狀態。
- `pc_next_level.bmp`：Next Level 星圖、行星、航線點與目的地。
- `pc_quit_selected.bmp`：Quit Game 的來源選單位置與左側船艦資訊。

來源程式為 `org/opentyrian` 固定版本；原始擷取保持 320x200，不修改
像素，供後續核對。

## GBA 240x160 適配結果

- `gba_game_menu.png`
- `gba_upgrade_menu.png`
- `gba_upgrade_ship_submenu.png`
- `gba_next_level.png`
- `gba_quit_confirm.png`
- `gba_boss_stats.png`

GBA 版保留來源選單結構、ROMFS/HDT/SHP 資料與操作語意；排版依
240x160 重新裁切及壓縮，不要求逐像素縮放 PC 畫面。
