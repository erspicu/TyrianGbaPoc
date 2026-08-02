# TextRes：關卡／章節故事文字

這個目錄提供 GBA 畫面專用的人工斷行文字。執行建置時，
`tools/build_textres.py` 會將 `Episode1`～`Episode4` 的 UTF-8 `.txt`
編譯成約 12 KiB 的 ROM 查找表。

## 設計邊界

- 原始 `vendor/tyrian/data/levelsN.dat` 仍是劇情流程的唯一權威來源。
- 圖片、音樂、條件跳轉、難度／船型分支及 section 銜接都直接讀原檔。
- TextRes 只在原始 reader 已讀完一個文字區塊後覆蓋「顯示文字」。
- 檔名中的 `offset_XXXXXXXX` 是該區塊在原始腳本中的穩定索引，請勿改名。
- 若 TextRes pack 缺少或損壞，runtime 會安全沿用 `levelsN.dat` 原文。

因此人工改斷行不會改變故事路線，也不需要為每一關另寫流程資料。

## 編輯規則

直接編輯對應 `.txt` 即可；一個文字檔就是一次等待玩家按鍵的文字區塊。

- 一行最多 60 個 CP437 bytes。
- 一個區塊最多 10 行。
- 可保留原版 `~` 明亮字切換控制碼。
- 建置器會拒絕字型無法顯示的 Unicode 字元，並指出檔名與行號。
- 畫面層仍保留安全自動換行；建議以檔案中的人工換行為準，方便細調。

檔名中的 `section_NNN` 與 `text_NN`／`end_hint_NN` 是方便人類查找的
註記，真正 runtime key 是 Episode 加原始 offset。

## 指令

重新編譯文字 pack（一般 ROM 建置會自動執行）：

```powershell
python tools/build_textres.py build `
  --input TextRes `
  --output res/textres_scene.bin
```

從原始 PC 資料重新匯出預設文字：

```powershell
python tools/build_textres.py export `
  --source-root vendor/tyrian/data `
  --output TextRes
```

若既有檔案與原文不同，匯出器預設會停止，避免覆蓋人工修改。只有確定要
全部還原成 PC 原文時才加 `--force`。
