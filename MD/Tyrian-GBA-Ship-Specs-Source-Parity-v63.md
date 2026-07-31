# Tyrian GBA Ship Specs 原始碼與資料鏈稽核（v63）

日期：2026-07-31

## 結論

`Ship Specs` 不是固定畫面。GBA 版每次進入頁面時都從目前已確認的
`frontend_player_items.ship` 重新讀取船體定義、兩段介紹與大型船圖。
購買新船及載入存檔後，名稱、介紹和圖片會一起切換。

本次確認到的實際缺陷是文字 renderer 漏掉 PC 版的 `~` 高亮控制碼，
因此原始 `shipInfo` 裡每一對 `~` 都被畫成問號；另外自訂 GBA mixed-case
字型缺少分號。HDT 的解密演算法與 13 艘船、共 26 段原始文字本身正確。

## PC 原始流程

OpenTyrian `game_menu.c::JE_drawShipSpecs()` 的資料關係為：

1. 標題：`ships[player[0].items.ship].name`；
2. 第一段：`shipInfo[player[0].items.ship - 1][0]`；
3. 第二段：`shipInfo[player[0].items.ship - 1][1]`；
4. 圖片：`ships[player[0].items.ship].bigshipgraphic - 1`；
5. `JE_helpBox()` 最後呼叫 `JE_textShade()`／`JE_outText()`；
6. `JE_outText()` 遇到 `~` 時切換 `+4` brightness，不把它當字畫出。

`helptext.c::JE_loadHelpText()` 則依檔案順序從 `tyrian.hdt` 讀取 13 組、
每組兩段的 encrypted Pascal strings。GBA 的 `hdt_pascal_read()` 採用相同
反向 XOR 順序；逐筆探勘確認全部 26 段都是正確 ASCII 內容，唯一超出原
GBA mixed-case glyph catalog 的一般標點是分號。

## GBA 對應資料鏈

GBA `frontend_ship_specs_prepare()` 現在依序執行：

1. 讀取 `frontend_player_items.ship`；
2. 依目前 Episode 選擇 PC 原始 item database；Episode 1–3 使用
   `tyrian.hdt`，Episode 4 使用 `tyrian4.lvl` 的原始 item block；
3. `ot_data_hdt_ship_read(ship_id)` 讀名稱與 `bigshipgraphic`；
4. `ot_data_ship_info_read(ship_id)` 讀 `shipInfo[ship_id - 1]` 的兩段文字；
5. 船圖仍從 ROMFS `tyrian.shp` 的 `OPTION_SHAPES` 讀取並套用 PC 的
   local-brightness green wireframe 運算。

Upgrade Ship 的第二次確認會把購買結果留在同一份
`frontend_player_items`。關卡初始化、Save capture、Save load 和 Ship Specs
都讀這一份狀態，沒有另外維護固定船 ID 或 GBA-only 介紹表。

## 本次修正

- Ship Specs 與 Data 的 cooperative wrapped-text job 現在都無條件把 `~`
  當控制碼，不再交給缺字 fallback 變成 `?`。
- Ship Specs 計算換行寬度時忽略 `~`，避免不可見控制碼改變排版。
- Ship Specs 高亮文字重現 PC 的同色盤 `+4` brightness 規則。
- mixed-case GBA 字型新增真正的分號 glyph；未改寫原始介紹文字。

## 端到端回歸

前端壓力 ROM 新增真實購買鏈測試：

1. 從 Episode 1 `levels1.dat` 第 12 節商店取得 stock 船體清單
   `1, 3, 9`；
2. 初始 ID 1 驗證為 `USP Talon`、介紹以 `The ~USP Talon~` 開始，
   `bigshipgraphic=32`；
3. 以兩次確認購買 ID 3，依 PC 賣回／購買公式由 50,000 cr 結算為
   44,000 cr；
4. Ship Specs 隨即驗證為 `Gencore Phoenix`、對應 Phoenix 介紹及
   `bigshipgraphic=28`；
5. 同一船 ID 同步套用到下一關資料與 Save capture。

Save ROM 另以 ID 7 驗證寫入、SRAM 讀回後，Ship Specs 顯示
`Prototype Stalker-C`、`~TOP SECRET:~` 介紹和 `bigshipgraphic=33`。

High detail／Normal speed 完整建置結果：

- `save_telemetry_failures=0`；
- `frontend_transition_upgrade_submenu_failures=0`；
- `frontend_transition_game_ship_specs_failures=0`；
- Ship Specs 120 次轉場 `missed_vblanks=0`；
- ROMFS 關卡矩陣 62/62 通過；
- Episode 1 四關 campaign 4/4 通過。
