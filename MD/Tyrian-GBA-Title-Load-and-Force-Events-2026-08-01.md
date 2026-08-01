# 首頁 Load Game 與續關 Boss 卡死修復

- 日期：2026-08-01
- 範圍：首頁讀檔流程、Episode 4 `HARVEST` 續關、針對性 mGBA 驗證

## 結論

首頁 `Load Game` 現在會開啟真正的 11 槽 SRAM 讀檔頁，而不是暗色停用
項目。有效槽會還原 campaign、裝備、金錢、生命狀態、Data Cubes、Episode
與 map section，再進入該進度的 Game Menu；空槽維持不可載入，`B` 或
`Done` 會回首頁並把游標留在 `Load Game`。

續關後卡在 Boss 的原因不是 `.sav` 損壞，也不是 drop-frame 或 GBA 效能。
GBA 關卡 port 雖然已處理 PC event 53、保存 `forceEvents`，卻漏掉主迴圈中
實際消費這個旗標的單行流程：

```c
if (forceEvents && !backMove)
    curLoc++;
```

Episode 4 `HARVEST` 在關卡位置 9400 以 event 2 停住背景、event 53 開啟
`forceEvents`。PC 版仍會每個 logic tick 推進 `curLoc`，讓 9430～9790 的
機械場景、Boss 生成與 Boss 控制事件依序執行；舊 GBA 版因缺少這一步，
時間軸永久停在 9400，所以畫面上的物件仍可活動，卻永遠不會進入真正的
Boss／離關事件。

## PC 流程對照

首頁路徑直接對照 bundled OpenTyrian source：

1. `vendor/opentyrian/src/tyrian2.c` 的 `MENU_ITEM_LOAD_GAME` 呼叫
   `JE_loadScreen()`。
2. `vendor/opentyrian/src/mainint.c` 的 `JE_loadScreen()` 顯示 11 個單人
   slot 加 `Done`；有效 slot 呼叫 `JE_loadGame(slot)`。
3. `vendor/opentyrian/src/config.c` 的 `JE_loadGame()` 還原難度、Episode、
   裝備、金錢、level 與 cubes，返回正常遊戲流程。

GBA 保留同樣的功能順序，但使用既有的雙 bank、CRC32、最後 commit byte
之 32 KiB SRAM 格式，並把 PC 鍵盤／滑鼠操作換成 GBA 手把。首頁與
`Game Menu > Options > Load` 共用同一份 slot browser 和解序列化程式，
沒有建立第二套假資料或轉換存檔。

關卡時間軸則直接對照 `vendor/opentyrian/src/tyrian2.c`：event system 完成
之後、背景 delay／update 之前執行 `forceEvents && !backMove`。GBA 修正也
放在相同順序，避免用 HARVEST 專用 workaround 破壞其他關卡。

## 使用者存檔探勘

`build/TyrianGBA.sav` 是有效的 32 KiB 雙 bank 存檔。最新有效 bank 的
sequence 為 27，slot 1 指向 Full Game、Episode 4、section 13、關卡名
`HARVEST`；裝備、金錢、Armor、Shield 與 cubes 均可通過現有 CRC／schema
解析。前一個 bank sequence 26 指向 section 11 `ICE EXIT`。因此問題可排除
為「存檔錯誤 section」或「資料毀損」。

## 針對性驗證

### SRAM 首頁讀檔測試

`save-autotest` 新增三個 production-path 檢查：

- 首頁能進入 Load 模式的 11 槽頁；
- `B` 回首頁並保留 `Load Game` 游標；
- 載入已寫入的有效槽，確實還原 section／cash，並排程 Game Menu map 與
  music preparation。

結果：`TGSV` schema 1、`pass=1`、`failures=0`、11 slots。

### Episode 4 section 13 完整路線

組態為 High Detail／Normal Game Speed，mGBA headless 從 section 13 執行到
離關：

| 項目 | 結果 |
|---|---:|
| ROM telemetry | PASS |
| 最終狀態 | Game Menu |
| 最終 level position | 9865 |
| Victory music start／natural stop | 1／1 |
| Level-complete voice | 1 |
| 統計階段推進／最終階段 | 4／4 |
| missed VBlank | 16，全部發生於 gameplay |
| frontend／stats／transition missed VBlank | 0／0／0 |
| mGBA runtime error | 0 |

這證明時間軸已越過原本的 9400 停點，完成真正 Boss、一次性勝利音樂、
離關動畫／統計及返回 Game Menu。測試只針對使用者實際存檔路徑，沒有為
這項修正重跑無關的完整 regression suite。
