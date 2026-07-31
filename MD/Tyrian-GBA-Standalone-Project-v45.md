# TyrianGbaPoc 專案獨立化與可重現建置（v45）

日期：2026-07-29
分支：`main`

## 目標

把原本散落在 `AprTyrianNes` 工作區其他位置的 Tyrian 資料、
OpenTyrian 參考 source、GBA SDK、音樂、資源轉換器與 mGBA 測試工具
收進 `TyrianGbaPoc`，讓專案搬到其他 Windows 路徑後仍可從 source
獨立產生 ROM。

## 最終目錄

- `vendor/tyrian/`：Tyrian 2.1 stock data 與資源工作圖。
- `vendor/opentyrian/`：行為規格使用的固定 source snapshot；
  `REVISION` 記錄 upstream commit。
- `vendor/audio/`：原始 tracker 音樂與 GBA OPL 參考量測。
- `tools/gba_asset_support.py`：GBA 原生圖形與 OBJ 資源轉換。
- `tools/gba_music_builder.py`：GBA Maxmod IT 產生器。
- `tools/templates/gba_maxmod_base.it`：專案自有的 Maxmod 結構模板。
- `vendor/gba-sdk/`：實際建置需要的 GBA CRT/spec、libgba、Maxmod 與
  host resource tools。
- `vendor/mgba/`：自動回歸用 headless/perf runtime。
- `tools/portable-msys2/`：最小 Bash/Make runtime。
- `.toolchain/`：首次建置時下載的官方 ARM compiler；不進 Git。
- `.venv/`：專案 Python 環境；不進 Git。

搬入 SDK 時一併移除了舊 CMake/Ninja build tree、examples 與 stage
目錄；它們不是 ROM 建置輸入，且包含原工作機器絕對路徑。原工作區的
外部 SDK、data 與 source 均保留，沒有刪除。

## 建置入口

- `Build-GBA-ROM.bat`：一般使用者的一鍵最終 ROM 建置。
- `tools/bootstrap.ps1`：準備固定版本的專案內 ARM/Python 環境。
- `tools/build_release.ps1`：產生並驗證
  `build/TyrianGBA.gba`，歸檔舊 ROM，清除中間產物。
- `build.ps1`：開發者完整 mGBA regression suite。
- `BUILDING.md`：接手、路徑、建置、Git 與 release 政策。

Arm GNU Toolchain 固定為 15.2.Rel1，下載 archive 的 SHA-256 為：

```text
7936cac895611023ffb22a64b8e426098c7104cb689778c1894572ca840b9ece
```

工具鏈安裝後約 1.1 GiB，不適合 GitHub source repository，因此由
bootstrap 下載、驗證並安裝在 ignored `.toolchain/`。這仍然是
project-local，不依賴系統 devkitARM。

## 隱藏依賴修正

從零資源重建找出兩個舊環境曾遮蔽的依賴：

1. 早期 Tracker IT builder 曾透過另一主機的 builder 取得結構模板。
   此相依已完全移除；模板現為專案內的
   `tools/templates/gba_maxmod_base.it`，讀寫、聲道選擇、PCM 合成與
   GBA tile 轉換都由 `tools/gba_*.py` 直接實作。
2. 官方 ARM toolchain 的 start-file 搜尋路徑與舊 devkitARM layout
   不同。Makefile 以 project-local GBA CRT 路徑加入 `-B`，使
   `gba.specs`、`gba_cart.ld` 與 `gba_crt0.o` 均能穩定解析。

## 驗證結果

先執行 `tools/build_release.ps1 -RebuildAssets`，從搬入的 stock data
完整重建 ROMFS、Sprite2 raw banks、front-end frames、音樂與 SFX；
再執行 `build.ps1` 完整驗證。

- ARM compiler：15.2.1（Arm GNU Toolchain 15.2.Rel1）
- 最終 ROM：21.41 MiB
- game code：`TYGA`
- 62-section ROMFS matrix：62/62 PASS
- Episode 1 四關 campaign：4/4 PASS
- Episode 2、3、4 route：PASS
- Arcade、Demo、JukeBox、death/end-flow：PASS
- Sprite2 decode/cache drop：0
- 600-frame release boot benchmark：PASS
- `build/` 最終只保留 `TyrianGBA.gba`

後續建置新增 `tools/audit_project_independence.py` 作為強制 gate；它會
掃描有效建置與 runtime 檔案、禁止固定工作區路徑與跨主機 builder，並
核對 41 首 GBA-only OPL reference。這使獨立性成為每次資源重建都會
驗證的規格，而不是只靠一次人工搬移。

2026-08-01 的完整移除、GBA-only 音樂重建、drop-frame／Maxmod 修正與
最新回歸結果續記於 `Tyrian-GBA-Standalone-Audio-Presentation-v64.md`。
