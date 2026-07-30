# TyrianGbaPoc v56：Data 與 Ship Specs source-parity 移植

日期：2026-07-30
分支：`palette-camera-data-v56`

## 結果

Game Menu 的 Data 與 Ship Specs 已由示意畫面改成 ROMFS-backed
source-parity 流程，並沿用先前解決靜態選單卡頓的分階段轉場架構。
按鍵當幀不會同步解密、縮放或重畫完整 240×160 頁面。

視覺檢查輸出：

- `temp/v56_data_list.png`：Episode 1 真實四筆 cube 清單；
- `temp/v56_data_reader.png`：stock 標題、人物與可閱讀內文；
- `temp/v56_ship_specs.png`：stock 船名／說明與綠色 wireframe。

## Data：直接翻寫的 campaign state

OpenTyrian `tyrian2.c` 的 `JE_loadMap()` 會在 Game Menu 顯示前依序處理：

- `]?`：設定 `cubeList`；
- `]!`：設定已取得的 `cubeMax`；
- `]+`：增加已取得數量。

GBA 的 episode map resolver 現在保存並按原始順序套用這三種 operation。
直接由同 section 進入 `]L` 時不會再次套用，避免 cube 數量重複增加。
Episode 1 的 pre-menu script 因而自然得到四筆資料，不再由測試或 UI
硬塞固定 cube。

`cubetxtN.dat` 仍是 ROMFS 內的原始 encrypted Pascal strings。runtime：

1. 掃描長度 byte 建立 record offset index；
2. 只 seek／解密使用者選到的 cube；
3. 以 OpenTyrian tiny-font 寬度規則進行 word wrap；
4. 每次 transition step 只處理有限筆 record／glyph；
5. reader 使用逐 pixel scroll target，不再整行跳動。

沒有產生 GBA-only cube catalog 或逐關 Python 對照表。

## Data：240×160 呈現

清單頁保留 PC 規格的四個構成：

- FACE_SHAPES 人物；
- 四顆可選藍色 cube；
- 每筆 stock title；
- `Exit to Game Menu`。

閱讀頁顯示 face、header、內文與 read percentage。字體寬度由一次性的
256-entry cache 查表；ASCII→sprite mapping 為 O(1)，small mixed-font
column sampling 也移除了 ARM7 軟體除法。

## Ship Specs

實作直接參照 OpenTyrian `game_menu.c`：

- `JE_drawShipSpecs()` 的兩段 `shipInfo` 文字；
- 當前玩家船名與 stock ship graphic；
- 原版逐 pixel greenify 規則；
- 綠色技術網格；
- `Press a key`；
- `JE_scaleInPicture()` 的中央展開意圖。

最終 240×160 frame 先分階段建立；進場動畫由 Mode 4 BG2 affine
register 執行 24 steps，不用 CPU 每幀 resize 38,400 pixels。96×112
source ship scratch 與 gameplay 冷區 arena 共用，不增加常駐大型
EWRAM buffer。

## 靜態選單卡頓防回歸

Data list、Data reader、Ship Specs、Upgrade、Quit、Game Menu、
Next Level 與所有已開放的前置選單都走 `FrontendTransitionJob`：

- immutable 背景／stock stamp 先準備；
- 每個 phase 有固定 record、run 或 glyph budget；
- 完成後才於 VBlank present；
- 返回上一層也使用 staged restore；
- 不在 navigation input 當幀執行整頁同步 redraw。

High／Normal transition stress 每條路徑執行 120 次：

| 路徑 | 最大 CPU cycles | missed VBlank | runtime Sprite2 decode |
|---|---:|---:|---:|
| Game Menu → Data | 214,961 | 0 | 0 |
| Game Menu → Ship Specs | 169,583 | 0 | 0 |
| Game Menu → Next Level | 87,250 | 0 | 0 |
| Game Menu → Upgrade | 118,968 | 0 | 0 |
| Game Menu → Quit | 81,003 | 0 | 0 |

Ship Specs 的 60 次 runtime SHP decode 是 120 次壓力循環中只在真正
需要 stock 船圖時執行，並分段處理；Data 完全沒有 runtime SHP／
Sprite2 decode。所有十條靜態轉場路徑皆為 0 missed VBlank、0 failure。

## 連帶 Sprite2 修正

柔性鏡頭讓 Episode 1 後續 routed level 可同時看見 25 個不同 Sprite2
working-set entries。原本 23 個 full slots + 1 個 compact slot 會 drop
一個 authored object。

新增第二個 16×16 compact slot，與只在 GAME OVER／SECRET LEVEL／
INSERT COIN 使用的 status-label OBJ bank 分時共用：

- status overlay 或 demo 使用該 bank 時，額外 compact slot 會保留；
- overlay DMA 後立即 invalid，不能誤用被覆寫的 OBJ tile；
- 普通 gameplay 才把它提供給 enemy cache。

四關 campaign smoke 為 4/4，Sprite2／projectile／effect drop 均為 0。

## 完整驗證

- High detail／Normal speed 完整 `build.ps1`：PASS；
- 62 關 ROMFS matrix：PASS；
- Episode 1 四關 campaign：PASS；
- Episode 2／3／4 第一關 route smoke：PASS；
- Arcade、death、demo、jukebox：PASS；
- Data page count：4；
- Data cube count：4；
- static transition paths：10；
- transition missed VBlank：全部 0；
- IWRAM stack guard：完整；
- EWRAM heap gate：PASS；
- release ROM：27,770,076 bytes（約 26.48 MiB）。

這一階段沒有把 Data／Ship Specs 退化成 GBA 專用靜態圖片；原始資料、
campaign 狀態與選擇內容仍由 ROMFS／OpenTyrian source 規格決定，GBA
只負責 240×160 呈現與可中斷的硬體提交。
