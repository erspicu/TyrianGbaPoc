# Tyrian GBA ROM／RAM／調色盤校正與精簡（v53）

日期：2026-07-30
狀態：完整回歸通過

## 目標與資源保留原則

本階段處理 Game Menu 音樂、現金數字、ROM 成長、RAM 安全邊界與
背景調色盤忠實度。ROM 精簡的定義不是「現在沒用就刪」，而是只移除
可以證明已由另一份 runtime payload 完整承接的重複資料：

- `tyrend.anm`、四章 cubetxt、CDT、EST、Christmas 資源及全部
  MUS/SHP/PIC/HDT/LVL 等唯一 stock 資料仍留在 ROMFS，供後續完整
  移植使用。
- 原始檔永遠保留在 `vendor/tyrian/data/`，確保專案可重建。
- ROMFS image 只排除 `tyrian.snd` 與 `voices.snd`。兩者的 29 個
  SFX 與 9 個 voice 已完整轉入 runtime 實際使用的
  `res/soundbank.bin`，程式也沒有 raw SND reader。
- 排除項目由 `vfs/manifest.json` 的 `omitted_duplicates` 明列；
  `build_romfs.py` 會記錄來源大小、SHA-256、替代資源並禁止同一檔案
  同時出現在 active ROMFS。

新 ROMFS 有 66 個唯一檔案，payload 9,452,369 bytes，image
9,455,704 bytes。相較 v52 的 9,853,080 bytes，image 減少
397,376 bytes，其中 397,279 bytes 是兩份 SND source payload。

## Game Menu 來源規格修正

### 音樂

OpenTyrian `musmast.h` 的 `DEFAULT_SONG_BUY` 是 zero-based `2`，
`game_menu.c` 直接呼叫 `play_song(songBuy)`；`musmast.c` 的曲目 2
是 `Buy/Sell Music`。只有 episode script 的 `]i` 是 one-based，
loader 已在讀取時減一。

因此 fallback 從錯誤的 song 1 改為 song 2，並同步更新死亡返回
Game Menu 的自動測試。重新進入同一頁時仍沿用既有「相同 module 不
重啟」規則，避免轉場斷音。

### 現金

PC `tyrian2.c` 的四章初始金額是
`{10000, 15000, 20000, 30000}`，Episode 1 顯示 `10000` 才是正確
規格。問題不是資料值，而是原生選單字型的數字 `1` 與大寫 `I`
使用相同筆畫。數字 `1` 已改為具有斜肩與底座的獨立 glyph。

實際 Mode 4 擷取位於：

`temp/v53_visual/game_menu.png`

畫面已確認為清楚的 `10000`，不是 `I000`。

## Next Level 無損字典

舊版把 15×15 相位全部存為 dense pages：

| 項目 | bytes |
|---|---:|
| 原始 225 pages | 3,974,400 |
| 2-row block data | 746,496 |
| u16 indices | 31,050 |
| 新格式合計 | 777,546 |
| 節省 | 3,196,854 |

每個 block 是兩列連續、每列 stride 128 的 256-byte payload，共
2,916 個唯一 block。runtime 每頁只有 69 次來源定位，每次仍連續
複製兩列；ARM/IWRAM copier 保持 32-bit 傳輸。

Build 端會從序列化後的 block/index 逐一重建全部 225 頁並 byte-for-byte
比較，任何一個像素不一致就中止建置。本版 audit：

- raw CRC32：`5425b8c0`
- block data CRC32：`72fb6c0f`
- index CRC32：`e50cb351`
- round-trip：PASS

舊的 `res/frontend_nav_bitmap_pages.bin` 會在增量建置時主動移除，
避免工作樹看似同時保有新舊兩份。120 次、8 條靜態選單轉場路徑的
實測均為 0 missed VBlank、0 runtime SHP/Sprite2 decode；Game Menu
到 Next Level 的最慢工作幀為 96,822 cycles。

Gemini 曾擔心 ROM dictionary 的 non-sequential waitstate 必然抵消
效益；實機模擬器 telemetry 並不支持這個推論，因此保留已量測通過
且完全無損的 block dictionary，不改用需要 EWRAM 解壓緩衝的 BIOS
LZ77。

## Shape-bank 調色盤

GBA Mode 0 的單一 4bpp tile 物理上最多只能選一組 16 色，因此任何
方法都不能宣稱無條件還原 PC 256 色。v53 不建立每關專用修補表，
而是從五份 stock `shapes?.dat` 的完整 tile 分布，各自訓練五組
mixed-material banks；十一組常用 single-hue banks 保持原樣。

runtime 只依 LVL 原本的 `shape_file` 選擇 `) / w / x / y / z`
adapter。LVL map、shape index 與 pixel index 仍直接來自 ROMFS，
所以後續關卡不需要新增 Python per-level 資源。

相對 v52 全域 mixed-bank 的加權 RGB 誤差改善：

| shape bank | 改善 |
|---|---:|
| `)` | 5.2874% |
| `w` | 10.4044% |
| `x` | 2.2627% |
| `y` | 4.9231% |
| `z` | 5.2059% |

若某 bank 的局部訓練結果比全域基線差，builder 會自動退回全域版本，
因此每個 adapter 都有 non-regression 保證。Episode 1–4 在同一
`curLoc=50` 的實際擷取位於：

`temp/v53_visual/episode{1,2,3,4}_bank_palette_pos50.png`

四章的地圖、敵人、前景與索引均正常，Episode 2／4 沒有再出現破圖。

## RAM：以量測取代猜測

Gemini 建議把 EWRAM／IWRAM gate 直接降到 2 KiB／2.5 KiB。實測發現
這不能直接採用：

- libgba linker 的舊 `IWRAM free` 包含 `__sp_usr` 上方保留給
  user/IRQ 的 256 bytes，不全是 C stack 空間。
- Maxmod 的 `mmInitDefault()` 會經 `calloc` 實際使用 3,892 bytes
  EWRAM；因此 EWRAM 只留 2 KiB 甚至無法涵蓋現有音訊引擎。

本版建置改為直接解析 `__sp_usr`，並只在 AUTOTEST ROM 啟用 IWRAM
stack canary。Release 沒有 canary 掃描成本。

| 路徑 | canary 填入 | 最深後仍未碰觸 | guard |
|---|---:|---:|---:|
| 完整 Episode 1／Boss／結算／返回 | 6,260 | 4,692 | PASS |
| 8 路徑 × 120 次靜態選單轉場 | 5,284 | 4,496 | PASS |

完整流程在 canary 區內使用 1,568 bytes；連同初始化時主動保留的
stack frame/guard，保守峰值約 2,028 bytes。兩條路徑的 EWRAM heap
結果一致：

- Maxmod heap high-water：3,892 bytes
- runtime 剩餘 EWRAM heap：24,576 bytes

據此把 link-time gate 調整為：

- EWRAM static free 至少 12 KiB；AUTOTEST 在 Maxmod 配置後仍須至少
  8 KiB。
- IWRAM `__iheap_start` 到 `__sp_usr` 至少 3 KiB；AUTOTEST canary
  實際仍須至少 2 KiB，且底部 guard 不得被改寫。

這比舊的 24 KiB／名義 5 KiB gate 積極，約釋出 12 KiB EWRAM 與
1.75 KiB IWRAM 給後續 cache／hotpath，同時不是盲目假設。

## 完整建置結果

- 完整驗證：PASS
- ROM：26,221,980 bytes（25.01 MiB，32 MiB 的 78.1476%）
- 相較 v52：減少 3,313,496 bytes（3.16 MiB，11.2187%）
- 尚餘標準 ROM window：7,332,452 bytes（6.99 MiB）
- SHA-256：
  `aa10d2250fe227ece9a997dc0a28ce1038545557da52c1879af47c1cb1024b38`
- ROMFS 62/62 section matrix：PASS
- Episode 1 四關 campaign：PASS
- Episode 2／3／4 第一關 route：PASS
- Arcade、死亡、Demo、JukeBox：PASS
- 靜態選單 8 路徑各 120 transitions：0 missed VBlank
- Episode 1 missed VBlank：8，全部在 gameplay
- Episode 2／3／4：28／12／22，全部在 gameplay；前端、結算與
  transition 均為 0

完整 machine-readable 結果位於 `build/verification.txt`。
