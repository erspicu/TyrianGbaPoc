# Tyrian GBA Save/Load、Build ID 與前端捲動 v61

日期：2026-07-31

## 完成範圍

- 首頁左下角顯示 `AprTyrianGba-<7 位 Git short hash>`。
- `Start New Game` 下新增停用狀態的 `Load Game`，並保留 Demo、
  JukeBox。
- Game Menu 的 Options 已啟用，內容為 Load、Save、Done。
- 實作 11 個單人存檔槽、14 字元名稱輸入、真正的 SRAM 寫入與讀回。
- Data Cube 長文改為增量捲動，避免超出一頁後重畫整個閱讀區造成斷音。
- Options、存檔槽、命名頁與固定提示改用 build-time panel／strip，
  動態名稱列使用 EWRAM 快取。

## 對照來源

本次不是只仿造畫面。規格核對來源如下：

- AprCSTyrian `Tyrian2.cs`：首頁左下角在 PC 320×200 座標 `(2, 192)`
  顯示 `opentyrian_version`。
- AprCSTyrian `Mainint.cs::JE_operation()`：Load／Save 行為、14 字元
  名稱與原始鍵盤命名流程。
- AprCSTyrian `GameMenuShop.cs`：Options 進入 Load／Save，以及存檔槽
  名稱、關卡與 Episode 顯示規則。
- OpenTyrian `config.h`、`config.c`：單人 11 槽、關卡進度、裝備、
  武器 power、金錢、Episode、難度、Data Cubes 與名稱欄位。
- OpenTyrian `game_menu.c`：Options 與 Load/Save menu 的選擇流程。

GBA 沒有鍵盤，也沒有使用 PC 的雙人模式，因此只保留前 11 個單人槽，
輸入介面改為手把字元輪盤；遊戲資料語意仍對應原始流程。

## 手把命名

- Up／Down：循環 `空白、a-z、0-9、-、.`。
- R + Up／Down：同一字母輪盤，但英文字母輸出大寫。
- A／Right：確認目前字元並前往下一格。
- Left：游標往前一格。
- B：刪除；游標位於第一格時取消並回到槽位畫面。
- Start：確認名稱並開始寫入。
- 長按 Select 60 frames：清空名稱。

方向鍵 repeat 採首次 15 frames、之後每 4 frames 一次，避免一次跳過
多個字母。空名稱不允許存檔。

## SRAM 格式與斷電安全

ROM 具有 `SRAM_V121` 掃描標記，使用標準 32 KiB SRAM。上半部配置：

| 區域 | 位址 offset | 大小 |
|---|---:|---:|
| Bank A | `0x6000` | 4 KiB |
| Bank B | `0x7000` | 4 KiB |

每個 bank 包含 20-byte header 與 11 × 64-byte slot payload。欄位使用
明確 little-endian serialization，不把 C struct padding 寫入永久格式。
每槽保存：

- Full Game／Arcade、Episode、Difficulty、main section。
- 船體、前後武器與 power、Shield、Generator、左右 Sidekick、
  Special、weapon mode。
- 金錢、目前 Armor／Shield、已取得 Data Cube 清單。
- 14 字元存檔名稱與關卡名稱。

寫入流程：

1. 選擇非目前有效的 bank。
2. 先清除目標 bank 的 commit byte。
3. 每個 VBlank 最多寫入 64 bytes。
4. header、payload、CRC32 全部完成後，最後才寫入 `0xA5` commit。
5. 開機同時驗證兩個 bank，選擇 sequence 較新且 CRC 正確者；新 bank
   損毀時自動回退舊 bank。

因此關機或 reset 發生於寫入途中，不會破壞上一份完整快照。

## Build ID

`tools/write_build_version.py` 在每次 Make 執行時產生被 Git 忽略的
`res/build_version.h`：

- clean commit：`AprTyrianGba-155a7b0`
- 有 tracked 修改的開發版：`AprTyrianGba-155a7b0+`

首頁先直接顯示 build-time 靜態畫面，下一個 VBlank 僅更新底部版本列；
不再為一行字從 Game Pak 複製完整 38.4 KiB Mode-4 frame。

## 前端效能策略

- Options、Load、Save、Save Name：各自只有一份 120×120 右側底板，
  不為每個游標狀態重複保存整頁。
- 七條固定 help text：只保存 240×11 bottom strip。
- 動態已占用槽位：EWRAM 保存 115×8 inactive row；畫面進入時線性複製。
- 開機、音樂開始前預製最常見的 Empty slot 0 與 Done row。
- Save Name 的固定標題與操作說明預先建立；按鍵時只動態畫名稱與游標。
- Data Cube 長文：EWRAM viewport 做 row shift，只重畫進入畫面的少量
  source lines，不再每個 scroll tick 重排全部十行。

## 自動驗證結果

`save-autotest` 驗證：

- 11 槽 round-trip。
- 所有進度、裝備、金錢、護甲、護盾與 Cube 欄位讀回一致。
- 較新的 bank 故意破壞 CRC 後，能回退上一個完整 bank。
- telemetry：`TGSV` schema 1、pass 1、failures 0。

前端轉場壓測擴充為 17 條路徑，每條 120 次，共 2,040 次：

- 全部 `missed VBlank = 0`、`failures = 0`。
- Options ↔ slots：最大約 118,595 cycles。
- slots ↔ Save Name：最大約 143,574 cycles。
- 連續字母切換：最大約 144,341 cycles。
- Data Cube 長文增量捲動：最大約 864 cycles。
- 最深 IWRAM stack 尚餘 1,880 bytes，guard 完整。
- EWRAM runtime heap 測試後仍餘 8,192 bytes。

以上數字來自 mGBA headless/performance telemetry。2026-07-31 已再由
根目錄 `build.ps1 -KeepIntermediates` 完成整套回歸：gameplay、死亡、
Demo、JukeBox、11 槽 Save/Load、ROMFS 全關卡矩陣、Episode 2／3／4、
Arcade、四關 campaign、17 條前端轉場，以及 release 開機 600 frames
全部 PASS。正式 ROM 另由 clean Git commit 重建，確保首頁不帶 `+`
且顯示的 short hash 能直接對應公開原始碼。
