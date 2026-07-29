# Tyrian GBA 原始碼分檔（v46）

日期：2026-07-29

## 問題

原先四個主要檔案已大到不利人工與 AI 維護：

- `src/frontend_runtime.inc`：196 KiB／7,196 行
- `src/autotest.inc`：113 KiB／3,261 行
- `src/opentyrian_level_port.c`：113 KiB／3,780 行
- `main.c`：121 KiB／3,234 行

## 作法

這次只做結構性分檔，不改函式、資料、編譯旗標、呼叫順序或演算法。
大型 runtime 仍以 ordered `.inc` 方式留在原 translation unit，因此：

- `static` state 與 helper 不需要擴大 visibility。
- IWRAM/ARM attributes 與 linker garbage collection 行為不變。
- OpenTyrian 翻寫的執行順序不變。
- 不增加跨 translation-unit 呼叫成本。

前端依 core/source-art/menus/navigation/flow 分成五份；自動測試依
core/ROMFS matrix/scenarios/telemetry/input 分成五份；關卡翻寫依
spawn/events/enemy-motion/collisions/advance 分成五份；`main()` 搬到
獨立 `main_loop.inc`。

完整導覽見 `src/README.md`。

## 結果

分檔後所有 C/INC 檔都小於 75 KiB；最大檔為
`src/source_runtime.inc`（74.4 KiB），這一輪不需再切。

機械分割時逐段重組並和原檔做 byte-for-byte 驗證。以相同 High
Detail／Normal Speed 設定重新編譯後，release ROM SHA-256 分檔前後
完全相同：

```text
cce1ec6363b5540cc45fa7fa2380284de6c5b1a2161651880966adee68166769
```

這代表 release machine code 與資源沒有因檔案整理而改變。完整 mGBA
regression suite 亦通過：62/62 ROMFS matrix、Episode 1 四關
campaign、Episode 2/3/4 route、Arcade、Demo、JukeBox、death/end-flow、
記憶體預算與 600-frame boot 均為 PASS，證明條件編譯的 AUTOTEST
片段也沒有因分檔改變。
