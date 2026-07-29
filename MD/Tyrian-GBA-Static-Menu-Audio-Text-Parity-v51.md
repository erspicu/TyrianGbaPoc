# Tyrian GBA 靜態選單音訊與文字規格同步（v51）

日期：2026-07-30

## 本階段完成項目

1. Game Menu 音樂改回 PC 原版 `songBuy` 規格。
   - OpenTyrian 的 `DEFAULT_SONG_BUY` 是一基底編號 2。
   - GBA／Maxmod 使用零基底編號，因此預設曲目是 **1**。
   - `levelsN.dat` 若有 `]i` 覆寫，仍優先使用該章資料。
   - 原先錯用的 29 是標題畫面歌曲，現已只留給 Title／Demo／Jukebox
     回首頁流程。

2. 依 OpenTyrian 功能語意補齊靜態選單音效。
   - 上下／左右移動：`S_CURSOR`（28）
   - Title、Play Mode、Episode、Difficulty 確認：`S_SELECT`（8）
   - Game Menu、Next Level 與 Quit 入口動作：`S_CLICK`（24）
   - 返回上一層／取消：`S_SPRING`（16）
   - 尚未開放的選項：`S_CLINK`（23）
   - Quit 對話框左右切換與確認／取消也沿用來源規則。

3. 修正 Select Episode 切換時的短暫音訊爆音來源。
   - 選擇列更新沿用預先建立的完整畫面，只 DMA 兩個 dirty rows。
   - 修正 `frontend_dirty_present()` 覆寫來源指標的呼叫順序；現在
     VBlank 會從新選項的 ROM 畫面複製，而不會誤讀 scratch frame。
   - 選單轉場壓力測試中，此路徑連續 120 次切換為
     `missed_vblanks=0`、`failures=0`。

4. Game Menu、Upgrade Ship、Next Level 與 Quit 對話框改用混合大小寫。
   - 顯示文字仍從 ROMFS 內原始 `tyrian.hdt` 解密讀取。
   - fallback 字串改成 PC 原版大小寫，例如 `Game Menu`、
     `Play Next Level`、`Exit to Game Menu`。
   - 使用專案的 6×8 混合大小寫筆劃；Game Menu 版本採較緊字距，
     可完整放入 108-pixel 的 Next Level 文字欄。
   - Quit 說明文字使用同一字形派生的 4-column 小字，不另外維護
     一套會與主字形漂移的 alphabet。

5. Next Level 的明暗與色盤關係改回 Game Menu 規格。
   - palette 17 的來源索引 `0xfa / 0xfb / 0xfe / 0xf0` 與 palette 0
     對應色相相容。
   - 現在分別用於一般文字、標題、選取文字與陰影。
   - 移除舊版錯用 `0xf4 / 0xf6` 所造成的暗紅與亮度關係錯亂。

## 驗證結果

- 完整 `build.ps1 -KeepIntermediates`：通過。
- 主流程、死亡、Jukebox、Demo、ROMFS 62 關資料矩陣、Episode
  1–4 路線、Arcade、四關 campaign：全部通過。
- 死亡流程回 Game Menu：
  - `return_song=1`
  - `return_state=7`
  - `return_music_active=1`
  - `full_pass=1`
- 八條靜態選單轉場路徑各 120 次：
  - 全部 `failures=0`
  - 全部 `missed_vblanks=0`
  - 全部保持 `music_active=1`
- 正式 ROM SHA-256：
  `54b22e694571ccfbda32f0fb216dbcea1afb1febbbf956283b4f327a2fe927b3`
- 正式 ROM 記憶體餘裕：
  - EWRAM：30,720 bytes
  - IWRAM：7,120 bytes

## 代表畫面

本機驗證截圖位於：

- `temp/phase51_static_ui/state7_v2.png`：Game Menu
- `temp/phase51_static_ui/state8_v2.png`：Next Level
- `temp/phase51_static_ui/state12_v2.png`：Upgrade Ship
- `temp/phase51_static_ui/state14_v2.png`：Quit 對話框

下一階段會把 `mainMenuHelp[34]` 直接從 ROMFS HDT 載入，補回
Game Menu 最下方的來源提示字串，並依 PC `JE_itemScreen()`、
`JE_drawScore()` 與 `draw_ship_illustration()` 規格重建左側
Ship Specs／金額／Armor／Shield 資訊及原生解析度格線。
