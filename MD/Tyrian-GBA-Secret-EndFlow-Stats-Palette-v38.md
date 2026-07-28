# Tyrian GBA Secret／結束流程／摘要字色／地圖調色 v38

更新日期：2026-07-28
工作分支：`opentyrian-source-parity-port`

## 本階段結果

本階段修正四組彼此相關、但來源不同的問題：

1. Secret Level 提示套用 GBA 上方裁切補償，提示音結束後恢復關卡音樂。
2. 死亡與擊敗 Boss 的結束音樂只播放一次；結束期間不再凍結遊戲畫面。
3. 過關摘要保留最後一張遊戲畫面，改用 PC 的小字型、座標與發光色彩。
4. 修復 `erro4.png` 中可破壞地物被整塊量化成綠色的 4bpp palette 問題。

所有資料仍由 ROMFS 的原始 LVL／HDT／SHP／palette 資料決定，沒有新增
per-level 圖表、Episode 特例或 GBA 專用關卡素材。

## Secret Level 提示與音樂

OpenTyrian `mainint.c` 的 `JE_playerCollide()` 在拾取
`evalue > 10000` 的 portal 時：

- 播放 source song 30；
- 設定 `bonusLevel` 與 `nextLevel`；
- 設定 `displayTime = 150`。

`tyrian2.c` 則在 `(90, 10)` 顯示 `miscText[59]`。GBA 戰鬥畫面從
PC 264×184 viewport 的上方裁掉 12 pixels，因此系統提示不能直接套用
world crop；v38 將這類頂端提示加回 `SOURCE_PRESENTATION_Y_ORIGIN=12`，
Secret Level 字樣不再被上邊界裁掉。

Maxmod 的一般關卡模組刻意使用 loop mode。v38 新增獨立的
`MM_PLAY_ONCE` 路徑：

- Secret Level song 30 以 one-shot 啟動；
- `displayTime` 的 150 個 source ticks 結束時，若仍在關卡且尚未進入
  end-level，明確恢復 `frontend_level.source_song`；
- 進入死亡、破關或新關卡時清除 secret-song 狀態，避免延遲恢復覆蓋
  正確的轉場歌曲。

因此提示音不會無限反覆，也不會把後續關卡 BGM 永久蓋掉。

## 死亡與擊敗 Boss 的畫面推進

OpenTyrian 的 GAME OVER 判斷位於同一個 `level_loop` 內；玩家爆炸結束
後雖會顯示 GAME OVER，但在收到離開輸入以前仍會繼續跑關卡 loop。

舊 GBA state machine 在 `STATE_GAME_OVER` 只畫最後一幀，造成背景、
敵人與特效完全凍結。v38 在 GAME OVER overlay 下仍依相同 fixed
logic cadence 呼叫翻寫後的 `update_logic()`、`render_game()` 與背景
prefetch；玩家已死亡，所以不會重新參與碰撞。

擊敗 Boss 後則保留既有的 40-tick `playerEndLevel` 飛離流程，期間背景
與關卡照常前進。進入 `JE_endLevelAni()` 摘要後才保留最後一張 gameplay
frame；PC 版同樣是在現有 `VGAScreenSeg` 上逐段畫摘要，而不是換成黑色
畫面或在摘要期間繼續跑戰鬥。

音樂路徑同步改為：

- end-level source song 9：`MM_PLAY_ONCE`；
- GAME OVER source song 10：`MM_PLAY_ONCE`；
- 離開 GAME OVER 回到 Game Menu：重新選取 title song 29 loop。

## 過關摘要背景、字型、大小與顏色

### PC 來源規格

OpenTyrian `mainint.c::JE_endLevelAni()` 指定：

- `textGlowFont = SMALL_FONT_SHAPES`；
- `set_colors(white, 254, 254)`；
- Completed `(20,20)`；
- Cash `(30,50)`；
- Enemies Destroyed `(40,90)`；
- Cubes `(30,120)`；
- cube `(50 + 30*i,135)`；
- Press A Key `(90,160)`。

`fonthand.c::JE_outTextGlow()` 先以 hue 0、brightness -12 畫四向暗邊，
再以 hue 15 畫發光本體，最後停在 brightness -4。

### GBA adapter

舊版使用自建等寬 tile font 與 BG3 黑底，字體過大、顏色及背景均與 PC
不同。v38 改為：

- 從 ROMFS `tyrian.shp` 的 table 1 直接讀取 `SMALL_FONT_SHAPES`；
- runtime 解碼成 16×16、4bpp OBJ glyph，保留原始可變字寬；
- 使用兩個 OBJ palette：bank 14 為目前 glow，bank 15 為已完成的
  hue 15／brightness -4；
- source palette index 254 按 PC 的 `set_colors()` 規則覆寫為白色；
- 四向暗邊保留在 glyph 的獨立色格；
- cube 沿用 source option shape 與 palette bank 9；
- 原 gameplay BG／OBJ 留在畫面，舊 OBJ 降到 priority 3，摘要文字使用
  priority 0；不再建立黑色 BG3。

GBA 只在最後輸出套用 y=12 的中央裁切；PC 的摘要座標與資料內容沒有
回寫或重新設計。實機畫面抽查確認最終字色為 PC 的橙紅 hue 15，而不是
舊版的白／綠色。

## `erro4.png` 綠色地物根因與修正

診斷時分別隱藏 OBJ、BG0、BG2，並固定擷取 `curLoc=180/240`。結果確認：

- 綠色區塊屬於 BG0 map tile，不是 enemy pool、Sprite2 cache 或錯誤
  enemy shape bank；
- 該區直接來自 ROMFS `shapesz.dat` 的混合色索引；
- 一張 GBA 4bpp BG tile 只能選一個 16 色 palette bank；
- 舊演算法以「不透明 pixel 數最多的 source hue」決定整張 tile 的
  bank，許多接近黑色的綠色 pixel 因數量較多，會把較亮的灰色機械
  結構整塊映成綠色。

第一版曾逐 pixel 比較 16 個 bank 的 RGB 誤差，畫面正確但 Episode 2
missed VBlank 由 22 增為 40，不適合作為正式方案。

最終版在同一個 O(64) tile scan 中，使用 `palette.dat` 每個 source
colour 的 `R+G+B` 能量累計各 hue，選擇視覺能量最大的 authored hue，
再沿用既有 nearest-colour table。這使暗色填充不會只靠數量壓過亮色
主體，也不增加 per-level 資料或 ROM 資源。

固定 `curLoc=180` 的最終畫面已確認地物恢復灰／棕主體，無整塊綠色；
Episode 2 全關 missed VBlank 為 24／10,475，重新通過 0.25% gate。

## 記憶體配置

未縮小 64-slot Sprite2 L2。完整 raw catalog 已使舊 RLE decoder 成為
不可達的驗證／fallback path，因此把其 2 KiB scratch 移至有餘量的
IWRAM，恢復一般 release 的 EWRAM gate。

| 項目 | v38 release |
|---|---:|
| EWRAM free | 49,708 bytes |
| IWRAM free | 8,888 bytes |
| ROM | 14,519,648 bytes |
| 32 MiB 使用率 | 43.2719% |

仍高於一般 build 的 48 KiB EWRAM／6 KiB IWRAM 固定門檻。

## 回歸結果

`.\build.ps1 -DetailLevel high -GameSpeed normal`：PASS。

| 測試 | 結果 |
|---|---:|
| Episode 1 完整 gameplay／Boss | PASS |
| Episode 1 missed VBlank | 13／12,168 |
| Episode 1 Boss missed VBlank | 3／439 |
| forced death／GAME OVER | PASS |
| 41-song Jukebox | PASS |
| ROMFS／Sprite2 section matrix | 62／62 |
| Episode 1 campaign | 4／4 |
| Episode 2 route | PASS |
| Episode 2 missed VBlank | 24／10,475 |
| runtime errors | 0 |
| unknown visuals／stream drop／cache drop／RLE fallback | 0 |

Release：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v38_detail_high_speed_normal.gba
```

SHA-256：

```text
826ae3dc5fadd0de29ee781e628db918e5f0ee362a02c1b1c87aefcb605e86de
```
