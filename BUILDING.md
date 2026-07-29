# TyrianGbaPoc 建置與目錄指南

這份文件是給接手修改、重建與驗證 ROM 的開發者。專案採用相對路徑，
除了首次下載的大型 ARM 編譯器與本機 Python 虛擬環境外，建置所需的
Tyrian 資料、OpenTyrian 參考原始碼、GBA SDK、資源工具與測試模擬器都
位於本專案內。

## 最快建置方式

Windows 10/11 上安裝 Python 3.10 以上版本，第一次執行時需可連線到
Internet，然後直接執行：

```text
Build-GBA-ROM.bat
```

腳本會：

1. 在 `.toolchain/` 安裝固定版本且經 SHA-256 驗證的 Arm GNU
   Toolchain 15.2.Rel1。
2. 在 `.venv/` 建立 Python 環境並安裝 `requirements.txt`。
3. 從 `vendor/` 的原始 Tyrian 資料產生 ROMFS、圖像與聲音資源。
4. 編譯並檢查 GBA header、`TYGA` game code 與 32 MiB 容量上限。
5. 把舊 ROM 移至 `Backup/`，並讓 `build/` 最後只保留
   `TyrianGBA.gba`。

可選參數範例：

```powershell
.\Build-GBA-ROM.bat -DetailLevel normal -GameSpeed normal
.\Build-GBA-ROM.bat -RebuildAssets
```

`-RebuildAssets` 會先移除可重建的資源輸出，再從 `vendor/` 完整重建。

## 完整回歸驗證

日常最終 ROM 使用上面的 BAT。要執行全部 mGBA 自動測試、路線矩陣、
選單轉場、死亡、JukeBox、Demo 與效能 telemetry，使用：

```powershell
.\build.ps1
```

保留 ELF、map、log、SRAM 與畫面等除錯中間產物：

```powershell
.\build.ps1 -KeepIntermediates
```

## 目錄配置

| 路徑 | 用途 | Git |
|---|---|---|
| `src/`、`main.c` | GBA runtime 與 OpenTyrian 翻寫層 | 提交 |
| `vendor/tyrian/data/` | Tyrian 2.1 原始遊戲資料 | 提交 |
| `vendor/tyrian/image/` | 建置使用的原始圖像工作資料 | 提交 |
| `vendor/opentyrian/` | 固定 revision 的 OpenTyrian 參考 source | 提交 |
| `vendor/audio/Music/` | Tyrian tracker 音樂與校準資料 | 提交 |
| `vendor/builders/` | 共用、可重現的資源轉換程式 | 提交 |
| `vendor/gba-sdk/` | libgba、Maxmod、GBA CRT 與資源工具 | 提交 |
| `vendor/mgba/` | headless/perf 回歸測試 runtime | 提交 |
| `tools/portable-msys2/` | 建置所需的最小 Bash/Make runtime | 提交 |
| `.toolchain/` | 官方 Arm 編譯器與下載快取 | 不提交 |
| `.venv/` | Python 套件隔離環境 | 不提交 |
| `res/` | 從 stock data 產生的可重建 GBA 資源 | 不提交 |
| `build/` | 最新的 `TyrianGBA.gba` | 不提交 |
| `Backup/` | 本機歷史 ROM | 不提交 |
| `MD/` | 移植設計、parity 與效能研究紀錄 | 提交 |

所有腳本均以專案根目錄為基準，不依賴
`C:\ai_project\AprTyrianNes` 或其他固定磁碟位置。整個目錄搬到其他
Windows 路徑後仍可建置。

## 手動環境設定

如只想準備環境，不立即編譯：

```powershell
.\tools\bootstrap.ps1
```

重新下載並安裝 ARM 編譯器：

```powershell
.\tools\bootstrap.ps1 -ForceToolchain
```

建置系統會優先使用 `.venv\Scripts\python.exe`，不會修改系統 Python
套件。ARM 編譯器也只會安裝在 `.toolchain/`。

## 資源與 source parity

`tools/build_assets.py` 只做平台所需的無損解包、排列與 GBA 格式轉換；
關卡、敵人、武器、事件、獎賞與流程仍由 ROMFS 內的 stock
MUS/SHP/PIC/HDT/LVL 等資料和 OpenTyrian 語意決定。不要為單一關卡建立
專用表格或手工修補資源；新增關卡支援時應修正共用 loader／adapter。

`vendor/opentyrian/REVISION` 記錄用來核對翻寫規格的 upstream commit。
更新 snapshot 時，必須同步檢查 parity 測試與更新該檔案。

## 發佈與版本控制

- `.gba` 不提交 Git；成熟成果以 GitHub Release asset 發布。
- `.toolchain/`、`.venv/`、`res/` 與 `build/` 都能重建，因此不提交。
- `vendor/` 內的必要原始資料、source snapshot、SDK 與小型工具需提交，
  否則 clone 後無法獨立重建。
- 新 release 前請先執行 `build.ps1` 完整回歸，再建立 tag 與 Release。
- 第三方元件與授權位置見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
