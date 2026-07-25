# Tyrian GBA 開發工具鏈安裝與 Runtime 驗證

日期：2026-07-25

## 結論

原本的 runtime 只有 mGBA 模擬器與 MSYS2，缺少 ARM cross compiler、GBA
startup/linker files、ROM header 工具、圖像轉換器及音樂轉換器，因此還不能建立
GBA ROM。

目前環境已補齊，而且已完成「C 原始碼 → ARM7TDMI ELF → GBA ROM → 修正
header → mGBA 實跑 600 frames」的端到端驗證，也另外通過
「MOD/WAV → mmutil soundbank → Maxmod ROM → mGBA 實跑」的音訊流程。這套
runtime 已足夠開始製作 Tyrian GBA 技術概念驗證版。

## 安裝位置

- SDK 根目錄：`C:\ai_project\AprTyrianNes\tools\gba-sdk`
- ARM 工具鏈：`C:\ai_project\AprTyrianNes\tools\msys64\ucrt64\bin`
- GBA host tools：`C:\ai_project\AprTyrianNes\tools\gba-sdk\tools\bin`
- libgba：`C:\ai_project\AprTyrianNes\tools\gba-sdk\libgba`
- Maxmod：`C:\ai_project\AprTyrianNes\tools\gba-sdk\maxmod`
- GBA 官方範例：`C:\ai_project\AprTyrianNes\tools\gba-sdk\examples`
- mGBA 原始碼：`C:\ai_project\AprTyrianNes\org\mgba`
- 最小驗證專案：`C:\ai_project\AprTyrianNes\org\GbaToolchainSmoke`

## 已安裝的主要工具

| 元件 | 版本 / 狀態 |
|---|---|
| ARM GCC | 16.1.0 |
| ARM binutils | 2.46.1 |
| newlib | 4.6.0.20260123 |
| GNU Make | 4.4.1 |
| CMake | 4.4.0 |
| Ninja | 1.13.2 |
| GDB multiarch | 17.2 |
| devkitARM rules | v1.6.1 |
| devkitARM crtls/specs | v1.2.7 |
| libgba | v0.5.4，見下方相容性說明 |
| Maxmod | v2.1.0 |
| mmutil | v1.10.1 |
| grit | v0.10.0 |
| gba-tools | v1.2.0 |
| general-tools | v1.4.4 |
| mGBA GUI | 0.10.5 |
| mGBA source/perf runner | `c034660f007c` |

可用的 GBA host tools 包含：

- `gbafix`：修正及驗證 GBA ROM header。
- `grit`：把圖片、tile、map、palette 轉成 GBA 可用資源。
- `mmutil`：把 tracker module 與 sample 轉成 Maxmod soundbank。
- `bin2s`、`raw2c`、`bmp2bin`、`padbin`。
- `gbalzss` 與 GBFS 工具。

## 為何採用混合安裝

devkitPro 官方建議透過 pacman 安裝 `gba-dev` 套件群。本次環境連到
`pkg.devkitpro.org` 時遭遠端服務拒絕（HTTP 403），所以未採用不明來源的
預編譯檔，改成：

1. 由 MSYS2 官方套件安裝 ARM GCC、binutils、newlib、Make、CMake 及 Ninja。
2. 由 devkitPro 官方 GitHub 固定 release tag 取得 rules、crtls、libgba、
   Maxmod、mmutil、grit 與 ROM tools。
3. 在本機編譯 GBA 支援庫與 host tools。

因此這是可工作的相容環境，但不是 devkitPro pacman 所發行套件的
byte-for-byte 複本。

## libgba 相容性說明

MSYS2 upstream newlib 沒有 devkitPro 私有的 `sys/iosupport.h`。為避免把
devkitPro 私有 newlib ABI 假裝成 upstream ABI，本機 `libgba-lite` 只排除了
`console.c`，其餘 libgba 原始碼照 v0.5.4 建置。

影響範圍是 libgba 的 ANSI/debug console glue。GBA 遊戲所需的顯示、按鍵、
DMA、timer、interrupt 等核心 API 仍可連結；實際的 Tyrian 畫面本來就不會
依賴 ANSI console。Maxmod 與 mmutil 已完整建置。

devkitARM 的 crt0 另會尋找 devkitPro newlib 的 `fake_heap_end`，Maxmod
連結時也會需要 upstream newlib 的 `_sbrk` syscall。驗證專案已加入
`gba_heap.c`，使用 linker 提供的 `__eheap_start`/`__eheap_end` 做 EWRAM
邊界檢查，Maxmod ROM 已經實際通過連結與 runtime 測試。正式遊戲仍建議讓
關卡物件、子彈及音效 voice 使用固定大小 pool，避免 heap fragmentation。

另外有兩個純 host compiler 相容處理：

- GCC 16 編譯 `gba-tools` 時，在 `gbalzss.cpp` 補入標準 `<cstdint>`。
- GCC 16 預設 C23 會把 `bool` 視為關鍵字，因此 mmutil 使用
  `-std=gnu17` 建置；沒有改動 mmutil 原始碼。

## 使用方法

在 PowerShell 載入本機 SDK 環境：

```powershell
. C:\ai_project\AprTyrianNes\tools\gba-sdk\gba-env.ps1
arm-none-eabi-gcc --version
```

建立最小 ROM：

```powershell
cd C:\ai_project\AprTyrianNes\org\GbaToolchainSmoke
.\build.ps1
```

在新版 mGBA 測試前端實跑 600 frames：

```powershell
.\verify-runtime.ps1
```

GUI 檢查可直接把 `gba_toolchain_smoke.gba` 拖入既有的 mGBA 0.10.5。
測試畫面是 Mode 3 漸層與色條，方向鍵可移動金色方塊。

## 驗證結果

| 檢查 | 結果 |
|---|---|
| C 編譯成 ARM7TDMI/Thumb object | PASS |
| `gba.specs`、crt0 與 cartridge linker script | PASS |
| 連結 `libgba-lite` | PASS |
| `objcopy` 產生 ROM | PASS |
| `gbafix` 修正 title/code/header checksum | PASS |
| MOD/WAV 經 `mmutil` 建立 soundbank 並連結 Maxmod | PASS |
| 畫面測試 ROM 以 mGBA software renderer 執行 600 frames | PASS |
| Maxmod 音訊測試 ROM 以 mGBA 執行 600 frames | PASS |

mGBA runtime 測試回傳：

```text
result=PASS
video_game=AGB-TGPT
video_frames=600
video_renderer=software
maxmod_audio_game=AGB-TMPT
maxmod_audio_frames=600
maxmod_audio_renderer=software
```

第二個 ROM 為 `gba_maxmod_smoke.gba`，辨識碼 `AGB-TMPT`；它循環播放
Maxmod module，按 A 可觸發獨立 WAV sound effect。

## 對 Tyrian GBA POC 的建議起點

1. 先以 tile/map 背景取代 Mode 3 framebuffer，降低 VRAM bandwidth。
2. 主角、敵機、子彈與爆炸採 OBJ sprite，更新資料用 DMA。
3. 先做開場畫面、Start、第一關 game loop 與 Boss 完成後返回標題。
4. 音樂先轉為 Maxmod module/soundbank；各 SFX 保持獨立 sample/channel。
5. 第一版避免一般用途 heap，關卡物件、子彈與音效 voice 使用固定大小 pool。

## 上游來源

- devkitPro Getting Started：
  <https://devkitpro.org/wiki/Getting_Started/devkitPPC>
- devkitPro 官方 GitHub：
  <https://github.com/devkitPro>
- libgba：<https://github.com/devkitPro/libgba>
- Maxmod：<https://github.com/devkitPro/maxmod>
- grit：<https://github.com/devkitPro/grit>
- mGBA：<https://github.com/mgba-emu/mgba>
