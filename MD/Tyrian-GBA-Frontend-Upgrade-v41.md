# Tyrian GBA v41：指定前端、Upgrade Ship 與裝備執行路徑

日期：2026-07-28

分支：`opentyrian-source-parity-port`

來源基準：OpenTyrian `1c34d1`

## 本階段範圍

本階段只處理下列已指定項目，不擴張到 GBA 不需要的全部 PC 設定介面：

1. Demo 關卡曲目索引。
2. Boss 擊敗後摘要文字與 Cube 色彩。
3. Game Menu 的船艦、裝備、護盾／裝甲與金額資訊。
4. Upgrade Ship 選單、交易與實際裝備效果。
5. Quit Game 確認視窗。
6. Next Level 星圖、行星、航線點與關卡選擇。

## 來源對照與 GBA adapter

### Demo 音樂

Stock `demo.1` 到 `demo.5` header 儲存的是 one-based `levelSong`。
OpenTyrian 進入關卡時使用 `play_song(levelSong - 1)`；GBA 現在於
header 解析邊界轉成 zero-based，之後直接沿用同一個
`OtEpisodeLevel.source_song` 契約。五個 Demo 的期望曲目為
`17, 32, 15, 15, 13`，不再誤用下一首曲目。

### Boss 摘要色彩

摘要仍疊在持續捲動的最後遊戲畫面上。文字使用適合 240x160 的
`TINY_FONT`，但保留 `JE_endLevelAni()` 的 hue 15 與 glow 階段；只補上
來源 `SMALL_FONT_SHAPES` 與 TINY 字形本身相差的六級亮度。Cube 使用
來源 `JE_drawCube(..., 9, 0)` 的 hue 9，並與文字拆成獨立 OBJ palette
bank，避免互相覆寫色盤。

### Game Menu 船艦資訊

左側不是 GBA 自畫的固定圖片。畫面依目前 `FrontendPlayerItems`：

- 從 HDT 讀 ship、front/rear weapon、shield、generator 定義；
- 從來源 SHP／Sprite2 畫大船、武器、sidekick 與裝備圖；
- 顯示目前 cash、armor 與 shield bar；
- 過關、拾取及 Upgrade 後的狀態會跨關保留。

右側只保留目前規格要求的可用流程。Data Cubes、Ship Specs、Options
仍顯示但保持 disabled；Play Next Level、Upgrade Ship、Quit Game 可用。

### Upgrade Ship

實作依 `src/game_menu.c::JE_itemScreen()` 的 one-player Full Game 核心：

- 七類：Ship、Front Gun、Rear Gun、Shield、Generator、Left Sidekick、
  Right Sidekick，另有 Done。
- 商店 inventory 從 `levelsN.dat` 的 `I` command／Episode 4 內嵌 item
  database 讀取；不建立每關 GBA 專用清單。
- 名稱、cost、item graphic、weapon port、power、ammo 與 option style
  均從目前 episode HDT／ROMFS 讀取。
- 目前裝備若不在本次商店清單，會像來源一樣補入再排序。
- 交易現金先加入舊裝備售回價，再扣新裝備及武器 power 1..11 的累進
  成本。
- 子選單支援即時預覽、owned 標記、不可負擔暗色、取消回復、確認交易。
- Rear Gun 支援兩種 source port mode；Sidekick 支援來源 style、ammo、
  recharge、animation、charge、跟隨／軌道位置及射擊點。
- Plasma Storm option 9／weapon 88 直接使用 `OPTION_SHAPES` 22..24；
  三張 64x32 frame 只在裝備時借用最後三格 enemy VRAM cache，
  一般裝備仍保有完整 24 格 enemy cache。

控制：

- 上／下：分類或物品。
- 左／右：Front／Rear Gun power。
- R：Rear Gun mode。
- A：選取／移至 Done／提交。
- B：取消子選單或回 Game Menu。

### Quit Game

Quit 不再立即返回首頁。確認視窗從 ROMFS `miscText` 取得問題、說明、
OK／Cancel，並使用來源 menu chrome／SHP window 裝飾；左右切換，
A 確認，B 取消。確認後才播放來源 spring cue 並回到 title。

### Next Level

星圖資料來自目前 `OtEpisodeMap`：

- `map_origin`、`map_x`、`map_y`、`map_planet`；
- 多個可選 route 與對應 level name；
- 來源 grid、planet Sprite2、移動 dot animation。

GBA 只將 320x200 左／右面板重新排入 240x160；路線、星球編號及選擇
結果仍由 ROMFS map data 決定。

## 記憶體處理

新增前端功能一度使 release EWRAM safety margin 降至 44,424 bytes，
低於專案固定的 48 KiB 門檻。原因是三組互斥畫面仍各自保留暫存區：

- Mode-4 前端 frame；
- gameplay 64-slot Sprite2 L2；
- Jukebox stars／tile map／palette shadow；
- 前端 Sprite2 decode canvas。

目前將它們疊在同一個 64 KiB `FrontendGameplayArena`。Jukebox、前端與
gameplay 不會同時執行，因此不減少任何 Sprite2 slot，也不改畫面內容。
結果回收 6,016 bytes：

- release EWRAM free：50,440 bytes；
- release IWRAM free：8,416 bytes；
- Sprite2 L2：仍為 64 slots。

## 驗收結果

組態：High Detail、Normal Game Speed、mGBA 0.11.0。

- 完整 `build.ps1 -KeepIntermediates`：PASS。
- 主回歸 `upgrade_loadout_runtime=1`。
- Episode 1 campaign 四關、Episode 2／3／4 route、Arcade、Demo、
  Jukebox、death、62-section ROMFS matrix：全部 PASS。
- unknown visual、Sprite2 decode/cache drop、projectile drop：0。
- Episode 2：10,475 display frames、29 missed VBlanks，約 0.28%。
  現有 wall-clock/drop-frame 機制保持遊戲節奏；警戒門檻保留為
  0.30%，沒有取消效能回歸檢查。
- Release ROM：14,629,568 bytes，佔 32 MiB 的 43.60%。

指定 PC／GBA 對照圖放在
`temp/pc_frontend_reference/selected/`，沒有把其他 PC 設定頁納入本次
移植範圍。
