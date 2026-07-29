# Tyrian GBA v47：統一設定、能源 HUD 與設定頁字型

日期：2026-07-29

## 結果

本階段把容易反覆校調的開發開關與畫面位置集中到根目錄
`Configure.h`。所有欄位都有中英文註解，build-time 靜態選單和
runtime 共用同一份數值；修改後執行 `Build-GBA-ROM.bat` 即會重新
產生受影響的資源與 ROM。

設定涵蓋：

- 主角無敵測試模式；
- 原版資料驅動的全武器極限壓力配置；
- 金額、武器能源、機體耐久、護盾；
- PAUSED、Secret Level、Insert Coin、Game Over、Boss 血條；
- 破關摘要各列與 data cube；
- 首頁、Play Mode、Episode、Difficulty；
- Game Menu、Upgrade Ship、Next Level 與 Quit Game 對話框。

目前預設保留開發中的無敵模式，極限武器模式關閉；正式遊戲裝備仍由
campaign 與 Upgrade Ship 決定。

## 原版能源語意

右下角三列不是新造的生命值：

1. `player_weapon_energy`：原版共用武器能源，範圍 0～900；
2. `player_armor`：機體耐久；
3. `player_shield`：護盾／備用能源。

同時補回 OpenTyrian 單人流程中的 generator 增加、護盾充能消耗，
以及前武器、後武器與兩個 sidekick 的 HDT `poweruse` 消耗。HUD
使用原本金額的 TINY_FONT 數字，靠右排列且最多新增七個 OBJ。

ARM7TDMI 沒有硬體除法器，因此 HUD 不使用逐幀 `/`、`%`。十進位拆分
以固定減法組合完成，並為三列各自保存格式化快取；護甲與護盾不變時
不重算，重複 presentation frame 也能沿用武器能源結果。

## 設定頁專用字型

新增 `tools/frontend_pregame_font.txt`：

- 5×8 原始筆畫、7-pixel renderer cell；
- build/runtime 共同套用一像素水平半粗體；
- 完整大寫、小寫、數字與設定頁所需標點；
- 原版文字的大小寫關係保留，例如 `Start New Game`、`Play Mode`、
  `Select an Episode`、`Difficulty Level`。

字型只替換進入 Game Menu 前的首頁與三個設定頁，不影響已校調好的
Game Menu／Upgrade Ship 小字體。mGBA 實機擷取已逐頁檢查，長度最長
的四個 Episode 名稱也都位於 240×160 安全範圍。

## 自動測試契約

真實武器能源會讓原有「Power 11 永久射擊」的 deterministic route
失去原本的擊殺與 Boss 時序。為避免兩種測試目的互相污染：

- release ROM 永遠使用真實能源規則；
- route/golden AUTOTEST 保留測試專用無限能源負荷；
- focused equipment test 明確關閉該測試旗標，逐項驗證 generator、
  shield recharge、成功消耗與能源不足拒絕發射。

## 驗證

`build.ps1 -KeepIntermediates` 完整通過：

- Episode 1 完整流程與四關 campaign；
- Episode 2／3／4 第一關 route；
- Arcade、death、Demo、JukeBox；
- ROMFS 62 sections matrix；
- 原版 end-level、聲音、Sprite2 L1/L2 與背景工作集 golden。

Episode 2 high/normal 完整路線為 32／10,475 missed VBlank
（0.306%），OAM 峰值 78／128，無 stream、decode、cache 或
projectile drop。回歸上限固定為 0.31%，再增加一個 deterministic
missed frame 即會失敗。

本階段正式 ROM：

- size：22,455,600 bytes；
- SHA-256：
  `b53d54f4950ecf44a66dcc1ff7651cb3b4ae622f583829e420cef166fa742882`。
