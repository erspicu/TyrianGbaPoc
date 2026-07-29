# Tyrian GBA v48：完整靜態選單轉場最佳化

日期：2026-07-29
狀態：完成、High／Normal 全套回歸 PASS

## 結果

本階段把 v43／v44 已驗證的「底圖預先準備、動態內容獨立、分幀組裝、
最後一次換頁」擴充到目前所有已開放的靜態選單路徑。轉場期間仍持續
呼叫 Maxmod，舊畫面保持可見，只有完整新頁面準備好後才在 VBlank
切換，因此不會看到半張畫面，也不會因整頁即時解碼而使音樂頓住。

涵蓋的雙向路徑：

1. Game Menu ↔ Upgrade Ship
2. Title ↔ Play Mode
3. Play Mode ↔ Select an Episode
4. Select an Episode ↔ Difficulty
5. Difficulty／破關統計／死亡流程 → Game Menu
6. Game Menu ↔ Next Level
7. Upgrade Ship ↔ 各裝備子選單
8. Game Menu ↔ Quit Game 對話框

## 圖形資料策略

### 完整原始圖章目錄

build 階段從專案內的原版資料建立通用圖章目錄：

- `tyrian.shp` table 5：sprite 26–32；
- `tyrian.shp` table 6：sprite 0–21；
- Upgrade／Options 所需 Sprite2 bank 38、39：graphic 1–284；
- 4/5 縮放的 25 個 sub-pixel phase。

總計 14,925 個圖章。資料仍依原版 HDT／SHP 編號選用，不含每關專用
對照表。圖章以對齊 sparse runs 保存：

| 項目 | 值 |
|---|---:|
| Offset table | 59,700 bytes |
| Pixel stream | 6,751,580 bytes |
| Opaque pixels | 4,066,187 |
| Runtime SHP/Sprite2 decode | 0 |

相較 dense 版本約節省 1.8 MiB ROM，同時保留無損像素結果。

### 破關統計 OBJ

原版 `TINY_FONT` 的 45 個必要 glyph、outline 與 data cube 在 build
階段無損解碼成 GBA 4bpp OBJ：

- tiles：6,656 bytes；
- widths：45 bytes；
- runtime SHP decode：0；
- tiles CRC32：`0f04dee4`。

這移除了 Episode 2 完整流程中原先固定發生在統計畫面的 3 次
missed VBlank。

## Runtime 管線

### 19.2 KiB 船體面板二級快取

Game Menu 與 Upgrade Ship 的左側 `120×160` 船體區依下列 component
分段組裝：

- ship body；
- generator；
- front／rear weapon；
- left／right sidekick；
- shield；
- status。

組合完成後保存在 EWRAM。裝備與金額 key 未變時直接分兩次、每次
80 rows 複製；key 改變時才逐 component 重建並更新快取。

### 有界分幀工作

靜態頁面工作拆成明確 phase：

- Mode 4 page copy：每次最多 80 rows；
- 靜態右側 panel：每次最多 60 rows；
- Next Level：一個選項／frame；
- Upgrade 子選單：art、details、每列選項分開；
- Quit：shade、overlay、choices 分開；
- Game Menu：map、song、chrome、ship、panel、dialog cache 分開。

正式壓力測試的單一 phase 最大值為 118,465 cycles，低於專案設定的
180,000-cycle 回歸門檻，也遠低於 GBA 60 Hz 一幀約 280,896 cycles。

### `levelsN.dat` 直接 ROMFS section 索引

破關後原流程會為每個下一關選項，從加密 `levelsN.dat` 開頭重新逐行
解密並尋找 `*` section。這是主流程最後一個選單漏幀來源。

新版沒有建立轉換資料檔，而是在 runtime 直接掃描 ROMFS 原始 Pascal
records：

- 只讀 record length 與可判斷 `*` 的加密首字元；
- 保存最多 64 個 section 的 ROM offset；
- 四個原版檔案實際只有 24–51 sections；
- 快取約 272 bytes；
- 後續 section seek 由 O(lines) 變成 O(1)。

原始腳本解析、跳轉、難度與 play mode 規則完全不變。

## 壓力測試

High／Normal、mGBA headless；每條路徑往返共 120 次：

| 路徑 | VBlank | Missed | Runtime SHP | Runtime Sprite2 | 最大 cycles |
|---|---:|---:|---:|---:|---:|
| Game／Upgrade | 121 | 0 | 0 | 0 | 83,110 |
| Title／Play Mode | 121 | 0 | 0 | 0 | 10,906 |
| Play Mode／Episode | 121 | 0 | 0 | 0 | 1,504 |
| Episode／Difficulty | 121 | 0 | 0 | 0 | 1,498 |
| Difficulty／Game（cold ship cache） | 1,441 | 0 | 0 | 0 | 83,396 |
| Game／Next Level | 1,321 | 0 | 0 | 0 | 86,965 |
| Upgrade／子選單 | 1,381 | 0 | 0 | 0 | 118,465 |
| Game／Quit | 541 | 0 | 0 | 0 | 81,029 |

合計 960 次轉場全部：

- `0 missed VBlank`；
- `0 runtime SHP decode`；
- `0 runtime Sprite2 decode`；
- 音樂維持 active；
- 最終 page、state、selection 與 pending job 全部乾淨收斂。

## 完整流程回歸

新增 missed-VBlank 狀態分類後，完整路線結果如下：

| 路線 | 總 missed | Gameplay | Frontend／Stats／Transition |
|---|---:|---:|---:|
| Episode 1 第一關 | 8 | 8 | **0** |
| Episode 2 第一關 | 29 | 29 | **0** |
| Episode 3 第一關 | 13 | 13 | **0** |
| Episode 4 第一關 | 22 | 22 | **0** |

另外通過：

- Episode 1 四關 campaign；
- ROMFS 62／62 sections matrix；
- Arcade equipment／reward route；
- death、Demo、JukeBox；
- 一次性破關／死亡音樂；
- Sprite2 L1/L2、背景工作集與 ROMFS audit；
- runtime error 0。

## 記憶體與正式 ROM

| 項目 | 值 |
|---|---:|
| Release EWRAM free | 30,720 bytes |
| Release IWRAM free | 6,368 bytes |
| ROM bytes | 29,276,900 |
| ROM MiB | 27.9206 |
| 32 MiB 使用率 | 87.2520% |
| SHA-256 | `649466fcc6843d8d0f27fd54ce779d5d73f6694cca8ed6ae8ef08466a9056363` |

完整驗證由 Windows PowerShell 5.1 執行，並修正 redirected process
handle 與 SHA-256 計算相容性；PowerShell 7 仍可使用同一支腳本。
