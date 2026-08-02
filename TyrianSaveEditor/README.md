# TyrianSaveEditor

`TyrianSaveEditor` 是 AprTyrianGba 的 Windows 存檔工具，同一個執行檔同時
提供 WinForms GUI 與 CLI。`TyrianSaveEditor.exe` 開啟圖形介面；同目錄的
`TyrianSaveEditor.Cli.cmd` 會等待命令完成並把結果正確輸出到終端機。兩者
呼叫同一份 C# codec 與 catalog，不是兩套格式實作。

它直接實作遊戲目前使用的存檔格式，而不是搜尋／猜測位址：

- 標準 32 KiB GBA SRAM image。
- 11 個玩家存檔槽。
- 位於 `0x6000`、`0x7000` 的兩個 4 KiB Bank。
- `ATGS` schema 1、sequence rollover 比較、CRC32 與 commit byte。
- 保留 Bank 外所有 emulator SRAM bytes。
- 保留或明確清除位於 `0x5FC0` 的內部 `LAST LEVEL` checkpoint。
- 寫檔時重建兩個有效 Bank；原地修改預設另存 `<name>.sav.bak`。

裝備名稱、Episode 進度與初始數值不是在 C# 內手寫。內嵌的
`Resources/catalog.json` 由專案所附的原始 `tyrian.hdt`、`tyrian4.lvl` 與
`levels1.dat`～`levels4.dat` 產生，因此 Episode 4 的獨立裝備資料也不會誤用
Episode 1～3 的表格。

## 建置

需求：Windows 與 .NET 8 SDK。

```powershell
cd C:\ai_project\AprTyrianNes\repo\TyrianGbaPoc\TyrianSaveEditor
dotnet build TyrianSaveEditor.csproj -c Release
```

或執行 `build.bat`，framework-dependent 的 win-x64 發佈檔會放在
`TyrianSaveEditor\publish`。此工具不會被包進 GBA ROM，也不影響 ROM 建置。

若日後替換專案內的原始 Tyrian data，可重新產生 catalog：

```powershell
..\.venv\Scripts\python.exe .\tools\build_catalog.py
```

一般使用者不需要 Python；產生好的 JSON 已納入 Git。

## GUI

直接執行：

```powershell
.\bin\Release\net8.0-windows\TyrianSaveEditor.exe
```

左側選擇 1～11 槽，右側分成：

- `Progress`：名稱、模式、Episode、難度、關卡 checkpoint、金錢。
- `Ship & Weapons`：Armor/Shield、飛船、前後武器與 power、Generator、
  Sidekick、Special、rear weapon mode。
- `Data Cubes`：實際存入存檔的最多四個 Cube ID。

`Initialize Slot` 會套用目前 C port 的 `frontend_campaign_initialize()` 初始值；
切換 Episode 時裝備選單也會改讀該 Episode catalog。拖放單一 `.sav` 到視窗
也能開啟。`Clear LAST LEVEL Checkpoint` 不會清除 11 個玩家槽。

## CLI

顯示 Bank、checkpoint 及全部存檔槽：

```powershell
TyrianSaveEditor.Cli.cmd info game.sav
TyrianSaveEditor.Cli.cmd info game.sav --json
```

建立 32 KiB 新檔，並初始化第一槽：

```powershell
TyrianSaveEditor.Cli.cmd create game.sav --slot 1 --name PILOT `
  --episode 1 --mode full --difficulty normal
```

檢視或修改一槽：

```powershell
TyrianSaveEditor.Cli.cmd show game.sav --slot 1 --json
TyrianSaveEditor.Cli.cmd set game.sav --slot 1 `
  --section 13 --level-name HARVEST --cash 125000 `
  --ship 7 --front 30 --front-power 11 `
  --rear 22 --rear-power 8 --generator 5 `
  --left-sidekick 14 --right-sidekick 15 --special 41 `
  --cubes 9,4,7,2
```

輸出到新檔、不覆蓋來源：

```powershell
TyrianSaveEditor.Cli.cmd set source.sav --slot 2 --cash 999999 `
  --output edited.sav
```

清除玩家槽或只清除內部 rollback checkpoint：

```powershell
TyrianSaveEditor.Cli.cmd clear-slot game.sav --slot 3
TyrianSaveEditor.Cli.cmd clear-checkpoint game.sav
```

驗證與工具自身 round-trip 測試：

```powershell
TyrianSaveEditor.Cli.cmd validate game.sav
TyrianSaveEditor.Cli.cmd self-test
```

完整參數可用 `TyrianSaveEditor.Cli.cmd help` 查看。原地修改預設建立 `.bak`；明確
不需要備份時才加 `--no-backup`。

## 存檔相容性注意事項

玩家名稱最多 14 個、關卡標籤最多 10 個 printable ASCII 字元，與 GBA 遊戲
目前可輸入／顯示的集合一致。GUI 的「Campaign checkpoint」是方便選擇的
原始 `levelsN.dat` 路線；`Main section` 仍可手動輸入，以便測試 secret route
或日後新增的流程。

如果較新的 Bank 具有正確 header/CRC，卻包含遊戲拒絕的 slot field，遊戲會
選到它後清空載入結果。Editor 會明確警告並暫時採用另一個完整有效 Bank；
重新儲存一次便會把兩個 Bank 都修復。
