# Tyrian GBA Data 與 Ship Specs 原始資料介面（v55）

日期：2026-07-30

## 本階段目標

將 Game Menu 的 `Data` 與 `Ship Specs` 從只有選單文字的占位項目，
改成可實際操作的頁面；內容必須直接讀取 ROMFS 內的 PC 原始資料，
不建立每章、每關專用的 GBA 文字資源。

## Data Cube

- `Data` 項目會依目前存檔狀態顯示已取得的藍色 Data Cube。
- 清單中的方塊、陰影、人物臉孔與配色，依
  `vendor/opentyrian/src/game_menu.c` 的 `MENU_DATA_CUBES` 分支移植。
- 藍色方塊使用原始 `OPTION_SHAPES` 圖形；選中的方塊保留來源動畫。
- Data 內容由 ROMFS 的 `cubetxt1.dat`～`cubetxt4.dat` 即時解密讀取。
- 標題、人物編號與本文換行規則直接依 PC 資料格式處理。
- 閱讀器保留 240×160 下可辨識的大小寫字形、捲動百分比與 `EXIT`。
- 取得順序保存於 SRAM；關卡事件只記錄來源 cube 編號，不複製文字。

Episode 4 的前三個 marker 是原始檔案中不可直接閱讀的占位槽，
因此 ROMFS 矩陣驗證使用該章第一個有效項目（cube 4），而不是把
占位槽誤判成損毀資料。

## Ship Specs

- `Ship Specs` 項目已可進入。
- 船名、船型圖編號與大圖編號直接讀目前 Episode 的 HDT/LVL item
  database。
- 兩段船艦說明直接依 `JE_loadHelpText()` 的檔案群組順序，從
  ROMFS `tyrian.hdt` 解密讀取。
- 畫面使用 PC 版的綠色網格、船名、兩段說明、大型船體圖與
  `Press a key` 結構。
- 大型船體圖由原始 `OPTION_SHAPES`／compound Sprite2 解碼；
  沒有新增船型專用 bitmap。
- 省略 PC 版進場縮放動畫，避免為純裝飾效果增加轉場成本與靜態資源。

## 記憶體與轉場

- Data 文件與 Ship Specs 文件共用前端冷頁面的 EWRAM arena。
- 這塊 arena 與 Quit dialog 的冷快取互斥，不會同時配置兩份。
- 人物 palette 只在 VBlank 中提交；頁面選擇及方塊動畫使用 dirty
  rectangle，不需每次上傳整張 38,400-byte Mode 4 畫面。
- 既有 Game Menu、Upgrade、Next Level 等轉場資產不受影響。

## 驗證

`build.ps1 -KeepIntermediates -DetailLevel high -GameSpeed normal`
完整通過：

- 正式版與主流程、死亡、Jukebox、Demo 測試
- ROMFS 62 個關卡 section 矩陣
- 四章代表 Data Cube 與 13 艘船說明讀取
- Episode 1～4 路線、Arcade、四關 campaign
- 靜態選單轉場壓力測試
- ROM、EWRAM、IWRAM、stack canary 與各項資產稽核

ROMFS 文件矩陣結果：

- signature：`TGLM`
- `FirstFailure=0`
- `romfs_failures=0`
- 62/62 sections 通過

前端轉場壓力測試仍為：

- 所有路徑 `missed_vblanks=0`
- 所有路徑 `failures=0`
- runtime SHP／Sprite2 decode 均為 0

本階段 release ROM：

- 大小：26,234,972 bytes
- SHA-256：
  `2ee9db195eda294e7a804f25aef62c6728b95d1f93de3fc05a7eca261f2e0475`

本機畫面驗證（不納入版本控制）：

- `temp/v55_stage2_state15.png`：Data 清單
- `temp/v55_stage2_state16b.png`：Data 閱讀器
- `temp/v55_stage2_state17.png`：Ship Specs

