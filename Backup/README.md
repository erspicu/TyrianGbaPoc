# GBA ROM backup

這個目錄保存 `Build.ps1` 歸檔的歷史、auto-test 及其他開發用 `.gba`。
ROM 檔受 repository 根目錄的 `*.gba` 規則排除，不會提交到 Git。

一般執行：

```powershell
.\Build.ps1
```

完整驗證成功後，`build/` 只保留最新 release ROM；其他 ROM 會移到本
目錄，object、ELF、map、save、log、preview 及 verification 等可重建
產物會清除。若同名備份內容相同會去除重複檔；同名但內容不同時會附加
時間與 SHA-256 短碼，避免覆蓋。

需要保留除錯中間產物時可明確指定：

```powershell
.\Build.ps1 -KeepIntermediates
```
