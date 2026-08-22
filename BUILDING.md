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

首頁左下角會由建置工具自動寫入
`AprTyrianGba-<7 位 Git short hash>`。顯示內容固定採用目前 commit，
不再因工作樹含未提交修改而附加容易誤解的 `+`。此文字由
`tools/write_build_version.py` 產生，不應手動修改 `res/build_version.h`。

可選參數範例：

```powershell
.\Build-GBA-ROM.bat
.\Build-GBA-ROM.bat -DetailLevel low
.\Build-GBA-ROM.bat -DetailLevel normal -GameSpeed normal
.\Build-GBA-ROM.bat -DetailLevel high
.\Build-GBA-ROM.bat -DetailLevel custom
.\Build-GBA-ROM.bat -RebuildAssets
```

未指定 `-DetailLevel` 時，建置會讀取 `Configure.h` 的
`TYRIAN_GBA_CONFIG_DETAIL_LEVEL`；正式 release 預設使用專案的 `LOW`。
`-DetailLevel low|normal|high|pentium|custom` 可只覆寫該次建置而不修改設定檔。這是
編譯期選擇，每個 ROM 固定一種等級，未選取的分支不會增加 runtime
判斷成本。`pentium` 仍保留給研究／極限壓力測試，不建議作為一般版本。

`-RebuildAssets` 會先移除可重建的資源輸出，再從 `vendor/` 完整重建。

## Configure.h：測試模式與版面校調

專案根目錄的 `Configure.h` 是一般開發者應優先修改的設定入口。每個
選項都有中英文註解，修改數值後重新執行 `Build-GBA-ROM.bat` 即可。

- `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE`：開關主角無敵測試模式。
- `TYRIAN_GBA_STRESS_LOADOUT`：開關全武器、最大火力的極限負荷配置。
- `TYRIAN_GBA_GAMEPAD_FULL_AUTO_SIDEKICKS`：設為 `1` 時，關卡內按住
  A 也會讓有限彈藥的左右 Sidekick 一起發射；預設 `0` 使用 A 主砲、
  B 雙 Sidekick 的分工配置。
- `TYRIAN_GBA_CONFIG_DETAIL_LEVEL`：選擇預設 `LOW`、`NORMAL`、`HIGH`
  、`PENTIUM` 或 `CUSTOM`。`CUSTOM` 是 Normal 基礎加上 Pentium 的
  wild 50/50 Alpha 與最終 hue／brightness filtration，但不含
  lava／water 色相與掃描線波動。
- `TYRIAN_GBA_DYNAMIC_FRAME_DROP`：正式 gameplay 的固定時間步進與完整
  場景 deadline 保護；預設開啟。
- `TYRIAN_GBA_ADAPTIVE_PRESENTATION_DISPATCH`：把持續高負載場景自動調整為
  每 2 個 source tick 建構一張完整畫面；只省略 presentation，不省略邏輯、
  碰撞、RNG 或音訊，預設全域開啟。
- `TYRIAN_GBA_ADAPTIVE_MAX_LOGIC_TICKS_PER_FRAME`：正式版固定為 `2`，把
  Adaptive 的最低目標封頂在約 17.4 FPS，不再進入三 tick／11.6 FPS。
- `TYRIAN_GBA_WAVE_ADAPTIVE_DISPATCH`：lava／water 波紋確認超載時直接使用
  Severe tier；其他重關卡與複雜武器仍由上一項全域量測處理。
- `TYRIAN_GBA_LAYOUT_*`：調整關卡 HUD、PAUSED／Secret Level 提示、
  Boss 血條、破關摘要，以及首頁、Play Mode、Episode、Difficulty、
  Game Menu、Upgrade Ship、Next Level、Quit Game 對話框的位置。

公開建置的預設值為正常傷害流程（無敵模式關閉）與一般劇情裝備
（極限負荷配置關閉）。需要長時間驗證關卡或量測 CPU／OAM 上限時，
再個別把對應開關設為 `1`；請勿把壓力測試配置誤當成遊戲平衡設定。

座標預設為最終 240×160 GBA 畫面像素；只有名稱含 `SOURCE_Y` 的欄位
是原始 PC 200-line 座標，runtime 會套用既有的 200→160 轉換。
建置時會檢查重要矩形與 HUD 是否仍位於畫面內；靜態選單的 build-time
資源與 runtime 會共同讀取同一份設定，避免兩邊位置不一致。

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

正式效能驗收允許少量、可重現、範圍有界且僅發生於 gameplay 的
`missed VBlank`；完整第一關 golden 的上限為 display frames 的 **2%**，
較密集的 Episode 2 stock route 上限為 **2.5%**。
既有 fixed-timestep drop-frame 只略過 presentation，不改變關卡時間、
碰撞、RNG、音訊更新或遊戲邏輯節奏，因此門檻內的退化通常無法由遊玩
感受到。前端、摘要、死亡與轉場仍要求 0 missed VBlank；上述 gameplay 容許也不
適用於卡音、輸入停頓、功能／畫面錯誤，或會持續惡化的負載。調整門檻
前仍須先重跑、定位並記錄數據。

正式版同時啟用全域 Adaptive presentation dispatch：持續超載最多降至每
2 個 source tick 一張完整場景（約 17.4 FPS）；Severe／wave 只改變壓力
分類與進入門檻，不再降到每 3 tick 一張。低負載仍維持 Light，不會因單一
cold-cache miss 長期降頻。設計、門檻及 A/B 數據詳見
`MD/Rule/Tyrian-GBA-Adaptive-Drop-Frame-Rule.md`。

目前獨立化音訊與 presentation 改善、記憶體配置及最新實測數據詳見
`MD/Tyrian-GBA-Standalone-Audio-Presentation-v64.md`。

## SRAM 存檔

release ROM 內含標準 `SRAM_V121` 標記，模擬器應建立 32 KiB SRAM。
Game Menu 的 Options 提供 Load、Save、Done，並沿用 PC 單人模式的
11 個存檔槽與 14 字元名稱。存檔採兩個 4 KiB bank、CRC32 與最後寫入
commit byte；每個 VBlank 最多寫 64 bytes，寫入中斷時仍可回退到上一個
完整 bank。

`0x5FC0..0x5FFF` 另保留 64 bytes 給 PC `LAST LEVEL` 的隱藏 rollback
checkpoint；它不會出現在玩家可選的 11 個槽中。這筆資料同樣有 schema、
CRC32 與最後 commit byte，只在進入 `]s`／`]b` 特殊關及最終死亡回復時
讀寫，因此不占用 gameplay EWRAM，也不影響一般存檔 bank 格式。

首頁的 `Load Game` 與 `Game Menu > Options > Load` 都會開啟真實的
11 槽瀏覽器；有效槽會還原 campaign、裝備、金錢、Data Cubes 與關卡進度。
格式、欄位、手把命名操作與自動測試詳見
`MD/Tyrian-GBA-Save-Load-Build-ID-v61.md`。

## 目錄配置

| 路徑 | 用途 | Git |
|---|---|---|
| `src/`、`main.c` | GBA runtime 與 OpenTyrian 翻寫層 | 提交 |
| `TextRes/` | 關卡／章節故事的 GBA 人工斷行文字；流程仍讀 `levelsN.dat` | 提交 |
| `vendor/tyrian/data/` | Tyrian 2.1 原始遊戲資料 | 提交 |
| `vendor/tyrian/image/` | 建置使用的原始圖像工作資料 | 提交 |
| `vendor/opentyrian/` | 固定 revision 的 OpenTyrian 參考 source | 提交 |
| `vendor/audio/Music/` | Tyrian tracker 音樂與校準資料 | 提交 |
| `vendor/gba-sdk/` | libgba、Maxmod、GBA CRT 與資源工具 | 提交 |
| `vendor/mgba/` | headless/perf 回歸測試 runtime | 提交 |
| `tools/gba_*` | GBA 原生圖形、音樂與資源轉換程式 | 提交 |
| `tools/opl_renderer/` | 固定版 OpenTyrian/DOSBox OPL 離線渲染橋接器與重建腳本 | 提交 |
| `tools/templates/` | GBA Maxmod 建置所需的本專案 template | 提交 |
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

Boss 的 GBA presentation 身分由獨立工具
`tools/build_boss_manifest.py` 在每次資源建置時掃描四章全部 `tyrianN.lvl`
自動產生。它從 Event 79、link alias、spawn cohort 與 group-control event
導出 `(Episode, LVL, event index)`，不含任何關卡／Boss 專用 runtime
清單；`res/boss_manifest_audit.json` 會對全部資料做 coverage 稽核。規則與
生命週期詳見 `MD/Rule/Tyrian-GBA-Boss-Identity-Manifest-Rule.md`。

唯一可人工編輯的劇情 presentation adapter 位於 `TextRes/`。建置器以
原始 `levelsN.dat` 文字區塊 offset 建立查找表，只覆蓋顯示文字與斷行；
圖片、音樂、條件分支及 section parser 仍直接讀 stock data。詳細限制與
重新匯出指令請見 `TextRes/README.md`。

背景 8bpp → GBA 4bpp 調色盤是平台限制下唯一必要的有損 adapter。
`tools/background_palette_training.py` 會從全部 62 關重建真正的
runtime tile keys，只訓練該 shape profile 未使用的 palette banks，
並要求每個既有 key 在 OKLab 與 CIEDE2000 都不得退步。輸出 hash、
資料集 coverage 與感知色差門檻由 `build.ps1` 稽核；不要以單關手工
palette 表取代此共用流程。

TYM 音樂經 `tools/music_maxmod_calibration.py` 以 GBA Maxmod 的 IT
volume、signed 8-bit PCM 與 runtime module volume 重新量測。正式管線保留
每首曲目全部九個 OPL2 source channel；source-RMS 排序只決定 tracker voice
順序，不再刪除第九聲道。不得引用其他主機的 voice map、mixer gain，亦
不得加入 per-song maximum normalization。逐曲結果輸出至
`res/music_maxmod_calibration.json`，完整 build 會檢查 41 首／334 個
source、RMS 誤差、transient peak ceiling 與 sample clipping。

音樂音色不再由短小的近似 wavetable 或通用程序鼓生成。
`tools/opl_renderer/tyrian_opl_bridge.dll` 會在 build time 使用 vendored
OpenTyrian/DOSBox OPL core，依原始 46-byte LDS patch 的 ADSR、KSL/KSR、
operator multiplier、feedback、waveform 與硬體／LDS LFO，在 49,716 Hz
離線渲染；`tools/opl_sample_renderer.py` 再以 127-tap Blackman-windowed
sinc 低通降至 15,768 Hz。每個實際 `(source, patch)` 依使用音域配置一至
三個 root sample，保留 attack，並為可持續音色搜尋 loop；原始 percussion
patch 則以原音高輸出 one-shot。一般建置直接使用已提交的 DLL，不需要 LLVM；
只有修改橋接器時才執行 `tools/opl_renderer/rebuild.ps1` 重新編譯。

OPL sample 的生命週期必須依 operator envelope 判定，不得只用 carrier EGT
bit 猜測，也不得重新加入固定 420 ms 的 tonal one-shot 上限。Sustain patch
使用緊湊 loop；finite tonal patch 依 TYM note generation 的實際 hold、root
轉調播放速率與 -58 dB silence floor 渲染／裁尾。長尾 finite patch 可為節省
ROM 將一至三個 root zones 減為一至兩個，但曲目實際音符到最近 root 的距離
不得超過 15 semitones。相關全 catalog regression 在
`tools/test_opl_sample_renderer.py`。

stock SFX／voice 保留來源原生 11,025 Hz，避免無資訊的升頻。Runtime 配置
18 個 Maxmod mixer slots，容納九個音樂聲道、八個邏輯 SFX 聲道與一個安全
餘額。End of Level、Game Over、Secret Level 各自保留 loop 與 `_once`
order-flow；mmutil 會共用相同 PCM。離線音樂校準仍以 896/1024 為 reference；
最終 runtime presentation 再依 `Configure.h` 將音樂設為其 90%
（806/1024），全部 SFX 設為其 70%（627/1024），避免重新校準抵銷使用者
要求的音量衰減。實作與容量稽核見
`MD/Tyrian-GBA-Adaptive-OPL2-Music-Implementation-2026-08-22.md`；runtime
混音規則見 `MD/Tyrian-GBA-Runtime-Mix-Balance-2026-08-22.md`。

背景音樂的 tonal／percussion source rate 以 15,768 Hz 為最低建置規格，
不得為節省 ROM 降回 11,025 Hz。實際 Maxmod GBA mixer 已由組語確認採
nearest-sample phase stepping；11,025 → 15,768 不會降低 runtime 混音成本，
反而會增加非整數 phase step 的重複取樣失真。未來音色豐富度升級應使用 ROM
增加 adaptive key zones、attack、LFO loop 與原始 OPL percussion；
容量不足時先縮減低收益 zone／loop。研究與已完成的實作結果見
`MD/Tyrian-GBA-Fixed-Rate-Music-Fidelity-Gemini-Study-2026-08-22.md`。

ROM 容量精簡採「可證明的功能重複」原則。未來完整移植可能需要的唯一
stock 資料，即使目前尚未接上 runtime，也保留在 ROMFS；只有當完整
runtime 功能已由另一份嵌入資源承接時，才可從 ROMFS image 排除原始
payload，而且原始檔仍須保留在 `vendor/` 作為可重建輸入。每一筆排除
都必須列在 `vfs/manifest.json` 的 `omitted_duplicates`，由建置 audit
驗證檔案、大小、SHA-256 與替代資源，禁止靠未記錄的 glob 或人工刪除。
目前可排除的完整重複 payload 為：

- `tyrian.snd`／`voices.snd`：完整轉入 `res/soundbank.bin`。
- `tyrend.anm`：完整、逐 frame 無損投影至
  `res/tyrend_gba_frames.bin`／`res/tyrend_gba_palette.bin`。
- 34 份 `newsh*.shp`（含 `newsh#.shp`）：38 個 logical bank、11,552 個
  component 已由
  `res/sprite2_raw_components.bin` 完整承接；Upgrade Ship 使用的
  `newsh1.shp` 另由完整 front-end source-stamp catalog 承接。build 會
  對原始 RLE 做逐 component round-trip，runtime 不再攜帶第二份壓縮流。

`retained_sources` 是相反方向的建置契約：列入的來源必須仍存在且保持在
active ROMFS，否則建置直接失敗。`music.mus` 保留全部 41 首 PC 曲目（含
由原始腳本正常選用的 Halloween Ramble）。`tyrianc.shp` 與
`voicesc.snd` 則由 source-to-generated 替代契約承接可驗證的 Christmas
模式，不會和預轉資源重複塞進 active ROMFS。
所有被排除的原始檔也仍保留在 `vendor/`，後續若格式需求擴充仍可重建。
完整分類與數據見
`MD/Rule/Tyrian-GBA-ROMFS-Resource-Retention-Rule.md`。

`vendor/opentyrian/REVISION` 記錄用來核對翻寫規格的 upstream commit。
更新 snapshot 時，必須同步檢查 parity 測試與更新該檔案。

## 發佈與版本控制

- `.gba` 不提交 Git；成熟成果以 GitHub Release asset 發布。
- `.toolchain/`、`.venv/`、`res/` 與 `build/` 都能重建，因此不提交。
- `vendor/` 內的必要原始資料、source snapshot、SDK 與小型工具需提交，
  否則 clone 後無法獨立重建。
- 新 release 前請先執行 `build.ps1` 完整回歸，再建立 tag 與 Release。
- 第三方元件與授權位置見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
