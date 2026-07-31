# Tyrian GBA v62：Data 與 Upgrade Ship 原始碼直譯稽核

- 日期：2026-07-31
- PC 規格來源：`vendor/opentyrian/src/game_menu.c`、`tyrian2.c`、
  `mainint.c`、`shots.c`、`backgrnd.c`、`varz.c`
- 輔助核對：`org/AprCSTyrian/cs_ported/Core/GameMenu*.cs`

## 原則

本階段不再由截圖猜測功能或資料。實作順序固定為：

1. 從 PC C 原始函式追控制流與資料來源。
2. 對照 C# 逐行移植版，確認索引及型別轉換沒有偏移。
3. GBA 只在顯示、輸入、儲存與分幀排程邊界增加 adapter。
4. 自動測試先驗證資料及狀態結果；截圖只作完成後的視覺回歸。

因此，本階段沒有建立手寫商品表、固定 Data 頁面或仿製的武器動畫。

## Data Cube 真實流程

PC 的順序不是「前一頁 Data 永久累加」：

1. `JE_loadMap()` 解析 `]?`／`]!`／`]+`，建立即將顯示的 `cubeList`，
   並依玩家上一關實際取得數量限制 `cubeMax`。
2. 真正進入關卡後，`JE_main()` 把 `cubeMax` 清成 0。
3. 擊破 value 1 的獎賞物時才增加 `cubeMax`。
4. 離關後，下一個 map section 再用新的 `]?` 清單配合這個數量。

GBA 現在照這條資料流處理：關卡開始清空 count、關卡結束擷取真實
`data_cube_pickup_count`、下一個 ROMFS `levelsN.dat` section 再套入新的
cube ID。Episode 1 回歸包含：

- 開場 Data：`1, 2, 23, 5`；
- Tyrian 後清單：`6, 7, 12, 18`，顯示數量由 Tyrian 實際拾取決定；
- Asteroid 1 後清單：`15, 13, 28`，顯示數量由該關實際拾取決定。

標題、人物、人物名、全文、強調符號、閱讀百分比及下方 help banner
都直接來自原始文字與 `cubeList`，不是 GBA 固定頁。

## `JE_loadMap()` 到商店的控制流

原始 `tyrian2.c` 遇到 `]G` 時只保存 planet／section 選項，並不立刻
離開 parser；它會繼續執行到 `]I`，讀完九組 `itemAvail` 後才呼叫
`JE_itemScreen()`。部分 stock route 是 `]G -> ]J -> ]I`。

GBA resolver 已改成相同行為：

- `]G` 保留 route，不提前 return；
- `]J` 跳到指定 section 後繼續；
- 只有讀完 `]I` 九組 inventory 才完成 map resolve；
- 遇到下一個 `*` section boundary 而尚未取得 `]I` 會視為解析失敗，
  不會誤讀相鄰 section。

回歸直接核對 Episode 1 section 2、4，以及 section 7 經 `]J 009`
取得的七類可購買裝備：Ship、Front、Rear、Generator、Left/Right
Sidekick、Shield。期待值取自 stock `levels1.dat`，不是 runtime 自己產生
再與自己比較。

## Upgrade Ship 交易逐行對照

### 物品建立

對應 `JE_itemScreen()` 與 `JE_genItemMenu()`：

- `itemAvailMap {1,2,3,9,4,6,7}` 的 one-based C 索引，GBA 轉成
  `{0,1,2,8,3,5,6}`；
- 目前已裝備物若不在 merchant inventory，補入清單；
- 排序仍使用「0（None）排最後，其餘依 ID」規則；
- 名稱、圖形與 base cost 直接查目前 Episode HDT。

### 現金與兩段確認

對應 `JE_cashLeft()`、`JE_getCost()`、`JE_menuFunction()`：

- 進子選單時，trade cash = 現金 + 舊裝備完整售回價；
- 武器 power 成本使用來源的累進三角和，不是 `base * power`；
- 游標預覽只暫時更換裝備；直接移到 Done 會還原 accepted snapshot；
- 第一次 A 在商品上等同 `JE_menuFunction()` 結尾更新 `old_items[0]`，
  並把游標移到 Done；
- 第二次 A 才以 accepted equipment 執行 `JE_cashLeft()` 並返回上層；
- B 的語意也照來源：還原目前的 `old_items` snapshot 後結算，因此若
  已完成第一次確認，B 不會神奇撤銷已接受的購買。

先前 GBA 在 Done row 以 selection 求 cost，會得到 0，形成保留預覽物卻
沒有扣款的漏洞；現在 cost 一律由實際 equipped item/power 求得。

## 即時武器模擬逐段對照

下列內容直接翻寫來源函式，而非依畫面模仿：

| PC 來源 | GBA 對應 |
|---|---|
| `JE_initWeaponView()` | 玩家 `(72,110)`、Sidekick `(57/87,120)`、power 500、repeat=1、multi-position=0、100 stars |
| `JE_weaponViewFrame()` | Front/Rear、Left/Right Sidekick 建彈順序，Generator 回充、900 上限、power bar |
| `player_shot_create()` | 81-slot pool、port op/mode/power、power use、sound channel、multi、delay、加速度、circle motion、Sprite2 graphic |
| `simulate_player_shots()` | TTL、X/Y motion、circle deviation、`0..140/0..170` 邊界、animation 與 primary/secondary shape bank |
| `JE_weaponSimUpdate()` | 升降級 cost、1..11 power bar、disabled arrow、目前 cash 與最後繪製玩家船 |
| `update_and_draw_starfield()` | 100 stars、source 320x200 position、speed+1、16 階 hue 與亮星十字 |

Source 對 Ship／Shield 類別顯示靜態船圖；只有 Front、Rear、Generator、
Left Sidekick、Right Sidekick 進模擬器。GBA 使用同一判斷。

PC 的星點位置取自 process-wide MT stream；為避免前端預覽改動 GBA 已在
使用的 gameplay RNG state，GBA 只在「初始星點的隨機數來源」使用局部
deterministic stream。draw count、每星四次取值、範圍、更新及顏色規則
維持來源語意；這是目前唯一刻意保留的非遊戲性狀態差異。

## GBA 必要 adapter

- PC software blit 的動態層改由 hardware OBJ 顯示；Menu chrome 仍在
  Mode 4。
- WIN0 以來源 `(8,8)..(143,182)` aperture 經相同 4/5 座標轉換裁切。
- 船與 projectile 圖仍是 stock Sprite2；build 階段只作 lossless RLE
  解碼與 tile 排列，不上色、不重畫。
- OPTION_SHAPES 12／13／14／17 是 stock SHP lossless stamp；轉場把大型
  sparse runs 分到多個 VBlank，避免 Maxmod 斷音。
- UI 文字與商品列因 240x160 重新排版；商品 ID、成本、選取狀態及控制流
  不變。

## 自動驗證結果

High Detail／Normal Game Speed，mGBA headless：

- 17 條已開放靜態選單路徑，各 120 次往返；全部 failure 0。
- 所有路徑 missed VBlank 0，music active 1。
- Upgrade submenu runtime SHP decode 0、runtime Sprite2 decode 0。
- Upgrade submenu 最大單 tick 214,826 cycles。
- Data、Upgrade inventory、兩段交易、現金、gameplay loadout 與 SRAM
  capture regression 全部通過。
- IWRAM stack guard intact，剩餘 1,808 bytes（門檻 1,536）。
- EWRAM heap remaining 8,192 bytes（門檻 8,192）。

以上測試先驗資料與狀態，再以 PC/GBA 畫面作最後視覺檢查；畫面不再是
實作規格的來源。
