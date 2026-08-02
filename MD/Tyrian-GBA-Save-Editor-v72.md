# TyrianSaveEditor：WinForms／CLI 共用存檔工具（v72）

日期：2026-08-02

## 定位

`TyrianSaveEditor/` 是獨立的 .NET 8 Windows 專案。GUI 與 CLI 共用同一份
`SaveCodec` 與原始資料 catalog，不靠 pattern search 猜測 SRAM 位址，也不
建立另一套與遊戲分離的存檔定義。

支援項目：

- 新建標準 32 KiB emulator `.sav`。
- 開啟、驗證、修改並另存既有存檔。
- PC/GBA 單人模式使用的 11 個玩家槽。
- Episode、難度、main section、關卡標籤、金錢。
- Armor、Shield、飛船、前／後武器及 1～11 power、Generator、Sidekick、
  Special weapon、weapon mode。
- 四個 Data Cube ID 與 secret-hint column。
- 顯示並可獨立清除 `LAST LEVEL` rollback checkpoint。

## 格式相容性

Codec 逐欄對應 `src/frontend/frontend_save.inc`：

- Bank 0=`0x6000`、Bank 1=`0x7000`，各保留 4 KiB。
- `ATGS` schema 1；20-byte header、11×64-byte slot payload。
- commit=`0xA5`、sequence wraparound 比較、相同的 `0xEDB88320` CRC32。
- checkpoint=`0x5FC0`、`ATGC` schema 1、獨立 CRC 與 commit=`0xC7`。
- Editor 改檔時保留上述區域之外的每個 SRAM byte。
- Host 寫檔先產生 sibling temporary file；原地修改預設留下 `.bak`。
- 每次儲存重建兩個有效快照，Bank 1 為更新 sequence。

若較新的 Bank header/CRC 正確、但 slot field 不符合遊戲 range check，遊戲會
選中後拒絕整頁。Editor 會顯示相容性警告、先讀另一個完整有效 Bank，使用者
存檔一次即可修復兩邊。

## 原始資料驅動的選單

`tools/build_catalog.py` 直接讀取專案內：

- Episode 1～3：`tyrian.hdt` 的 item block。
- Episode 4：`tyrian4.lvl` offset table 的最後一個 item block。
- 關卡進度：解密 `levels1.dat`～`levels4.dat`，追蹤 `]J`／`]G`／`]L`。

產生的 `Resources/catalog.json` 包含飛船、weapon port、shield、generator、
sidekick、special weapon 的真實名稱與 ID，也帶入 C port
`frontend_campaign_initialize()` 的 Episode 初始金額、armor/shield。JSON 已
嵌入執行檔；一般使用者建置不需要 Python。

## CLI 與建置

`build.bat` 產生 framework-dependent win-x64 publish：

- `TyrianSaveEditor.exe`：WinForms。
- `TyrianSaveEditor.Cli.cmd`：會等待完成並保留 stdout/exit code 的 CLI。

CLI 提供 `create`、`info/list`、`show`、`set`、`clear-slot`、`validate`、
`clear-checkpoint` 與 `self-test`。完整範例在
`TyrianSaveEditor/README.md`。

## 驗證結果

- `dotnet build -c Release`：0 warning、0 error。
- `dotnet format --verify-no-changes`：PASS。
- CRC32 標準向量 `123456789`：`CBF43926`。
- C# encode→decode：雙 Bank、sequence、全部欄位與 Data Cube round-trip PASS。
- 刻意破壞較新 Bank payload：正確回復較舊 Bank。
- Bank 外 sentinel byte：encode 後未改變。
- WinForms 控制樹建立 smoke：PASS。
- GBA `AUTOTEST_SAVE_FLOW`：`TGSV pass=1`、failure bits=0。
- 上述 GBA C encoder 產生的 Slot 4，C# 完整解出 `AprPilot`、Episode 3、
  section 42、金額 123456 與全部裝備／Cube。
- C# 以相同欄位反向 encode 的 64-byte slot payload，與 GBA C encoder
  byte-for-byte 比對：mismatch=0。
