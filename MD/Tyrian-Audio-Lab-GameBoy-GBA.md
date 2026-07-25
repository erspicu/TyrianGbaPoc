# Tyrian Audio Lab：Game Boy / GBC 與 GBA 音源研究、實作與校準

日期：2026-07-24  
VBA-M 參考版本：`c7f57102c8c133673f4aed2c3fb3122114964907`

## 目標與邊界

本階段在 TyrianAudioLab 增加兩個不依賴模擬器混音器的播放 profile：

- `GameBoy`：四個實體聲道 Pulse 1、Pulse 2、Wave、Noise；為了量測與
  校準，time-shared Wave 再拆成 melody 與 kick，共五個 analysis stem。
- `GameBoyAdvance`：AGB 模式的四個 PSG 聲道，加上 Direct Sound FIFO
  A、FIFO B；Wave melody/kick 分開量測，共七個 analysis stem。

每個 stem 在合成、輸出濾波、立體聲定位、完整 loop RMS 量測與 gain
校準階段都保持分離，最後才進入 UI 的群組 bus。這不是 Game Boy 或 GBA
整機模擬器：沒有 CPU、記憶體映射、LCD、DMA controller 或遊戲 ROM。
Direct Sound 的 FIFO 補充端由 Audio Lab 的 Tyrian 編曲器扮演。

## 本地參考原始碼

VBA-M 已 shallow clone 到：

`C:\ai_project\AprTyrianNes\org\visualboyadvance-m`

主要研究檔案：

- `src/core/apu/Gb_Apu.h/.cpp`
- `src/core/apu/Gb_Oscs.h/.cpp`
- `src/core/gb/gbSound.cpp`
- `src/core/gba/gbaSound.cpp`
- `doc/License.txt`

VBA-M 只作行為與數值參考；TyrianAudioLab 未編譯、連結或複製其 C++
實作。Audio Lab 的 C# 音源位於：

- `org/TyrianAudioLab/Audio/GameBoySound.cs`
- `org/TyrianAudioLab/Audio/GameBoyTimelineRenderer.cs`

## VBA-M 顯示的 GB / GBC 音訊模型

`Gb_Apu` 的時鐘是 4,194,304 Hz，並明確列出四個 oscillator：

1. Square 1：有 sweep。
2. Square 2。
3. Wave。
4. Noise。

frame sequencer 是 512 Hz；length、sweep、envelope 分別在其子節拍運作，
其中 volume envelope 是 64 Hz。Audio Lab 的 OPL 配器不強行套用持續硬體
sweep，避免改變原曲旋律；但音量更新被限制在 64 Hz、0–15 的硬體階梯。

### Pulse

11-bit period 與實際基頻：

```text
period = round(2048 - 131072 / requestedHz)
actualHz = 131072 / (2048 - period)
```

period 被限制在 0–2047。四個 duty 是 12.5%、25%、50%、75%。輸出在每個
channel 內使用 PolyBLEP 處理轉折，再通過自己的 DC blocker；這相當於用
band-limited 重建取代直接讓 44.1 kHz PCM 產生額外鋸齒。

### Wave

DMG/CGB Wave RAM 使用 32 個 4-bit sample。基頻公式：

```text
period = round(2048 - 65536 / requestedHz)
actualHz = 65536 / (2048 - period)
```

原 OPL instrument 的 modulator、carrier waveform、feedback 先生成單週期，
再量化為 0–15。DMG/CGB 輸出音量只選擇 0%、25%、50%、100% 中最接近的
硬體 level。

### Wave kick 與 channel 3 time-sharing

第 18 首的密集 kick 約每 100.72 ms retrigger 一次。舊版把 kick、snare、
hi-hat 全交給 LFSR Noise，kick 的寬頻 polarity jump 會比原 OPL2 的低頻
transient 尖銳許多。新版將 kick 改成 32-sample（AGB 為 64-sample）、
4-bit Wave RAM transient：

- 從事件音高降兩個八度並限制在 42–180 Hz。
- 起音由最高 420 Hz 向基頻作 20 ms pitch sweep。
- 1.5 ms attack/retrigger crossfade、34 ms decay、總長 95 ms。
- transient 存在時，melodic Wave 以 2 ms duck、6 ms restore 平滑
  time-share 同一個 channel 3。

renderer 雖分開累計 melody/kick RMS，最後不會同時輸出兩條完整 Wave
聲道，因此沒有虛構第五個 GB PSG channel。

### Noise

Noise 使用 15-bit LFSR，width mode 時另外回寫 bit 6，形成 7-bit 短週期。
`NR43` 的 divisor 第一級是 `{1,2,4,6,8,10,12,14}`，clock rate 為：

```text
4194304 / (divisor * 8 * 2^shift)
```

snare、hi-hat 分別選擇最接近 4,200、9,000 Hz 的 NR43 組合；hi-hat
使用 7-bit mode。每次打擊仍保有自己的 4-bit、64 Hz envelope。LFSR DAC
在 44.1 kHz 輸出前通過 9 kHz 一階 reconstruction pole；重新觸發時保留
舊輸出並用 1.5 ms smoothstep crossfade 接到新序列，避免 reset LFSR
造成單一樣本爆音。

### DMG 與 CGB 的差異

VBA-M 的數位四聲道核心相同，但輸出 EQ 常數指出 DMG headphone bass
cutoff 約 30 Hz、CGB 約 300 Hz。現有單一 `GameBoy` profile 採 30 Hz，
代表較飽滿、經典 DMG 的版本；UI 名稱同時標示 Game Boy Color，表示編曲
與四聲道數位能力同樣適用。若後續需要嚴格 A/B，可再增加只改 analog
cutoff 的 `GameBoyColor` profile，不需重寫音源。

## VBA-M 顯示的 GBA 音訊模型

GBA 並不是只剩 PCM。VBA-M 在 `mode_agb` 下繼續使用同一個四聲道
`Gb_Apu`，再並列兩個 `Gba_Pcm_Fifo`：

```text
AGB PSG：Pulse 1 + Pulse 2 + Wave + Noise
Direct Sound：FIFO A + FIFO B
```

### AGB PSG

Pulse duty 與 Noise 的最終 polarity 依 AGB 模式反轉。Wave 可使用兩 bank
的 64-sample 路徑，Audio Lab 因而使用 64 個 4-bit sample；要保持同一基頻
時，分子改為 32,768：

```text
actualHz = 32768 / (2048 - period)
```

AGB Wave 另可選 75% level，故可用 0%、25%、50%、75%、100% 五級。

### Direct Sound FIFO A / B

VBA-M 的模型是 32 bytes 可見容量：

- 7-word 主 ring，共 28 bytes。
- 1-word playing buffer，共 4 bytes。
- timer overflow 每次取出一個 signed 8-bit DAC sample。
- 主 ring 剩三 word 以下時要求 DMA 補充。
- FIFO 空時維持上一個 DAC 值，不自動歸零。
- A、B 可各自選 timer、50%/100% 及左右聲道。

Audio Lab 的 A/B 各有自己的 32-byte ring、generator、DAC latch、
12 kHz reconstruction pole、DC blocker、pan 與 gain。timer rate 選
32,768 Hz，PCM generator 使用兩 operator FM 產生 signed 8-bit 資料，
因此比 PSG 多保留 Tyrian OPL instrument 的 modulation 與 waveform
特徵。當 FIFO 剩 16 bytes 時，軟體配器補滿 32 bytes；這代表典型 DMA
串流行為，但不宣稱模擬 GBA CPU/DMA 的 cycle 細節。

VBA-M 的預設混音還顯示：

- PSG ratio 可由 `SOUNDCNT_H` 選 25%、50%、100%。
- PCM delta synth 的基準量約為 `0.66 / 256`。

Audio Lab 先以 100% PSG 與對應 signed 8-bit 尺度合成，再用每首歌、每個
stem 的完整 loop 校準取代單一全域比例。

## Tyrian 聲道分配

`TYM1` 仍保留原始 LDS 與九個 neutral timeline channel。分配原則：

### Game Boy

| GB stem | Tyrian 來源 |
|---|---|
| Pulse 1 | 原本 2A03/VRC7 APU 第一順位 |
| Pulse 2 | 原本 2A03/VRC7 APU 第二順位 |
| Wave melody | 原本 2A03/VRC7 APU第三順位 |
| Wave kick（time-shared） | percussion channel 中的 MIDI 35/36 或低音 kick |
| Noise | percussion channel 中的 snare / hi-hat |

### Game Boy Advance

| GBA stem | Tyrian 來源 |
|---|---|
| PSG Pulse 1 / 2 / Wave melody | 與 Game Boy 相同 |
| PSG Wave kick（time-shared） | 與 Game Boy 相同 |
| PSG Noise | snare / hi-hat |
| FIFO A / B | 優先挑尚未被 PSG 使用的 VRC7 FM 來源，再依必要補入 |

FIFO A/B 不複製混音後的 PSG，而是從兩個指定 OPL source 各自重新產生
PCM。Pulse 1 偏左、Pulse 2 偏右、Wave/Noise 居中；FIFO A/B 輕微分置
左右。pan 採 power-preserving 計算，避免定位本身改變校準能量。

## 音量校準

GB percussion split 在 v5 導入；加入 SNES 後目前 schema 為
`tyrian-channel-calibration-v6`。校準程序對 41 首歌逐一：

1. 原始 LDS/OPL2 跑完一個完整 loop。
2. 在 OPL2 合計與 limiter 之前量九個 channel 的 RMS。
3. GB/GBA 以沒有 stored gain 的狀態跑同一完整 loop。
4. 量每個獨立 stem 的 RMS。
5. 以 `targetRms / measuredRms` 解出 gain，限制在 0.02–16。
6. tonal source 若被兩個不同音源重複承載，以平方能量拆分目標。
7. GB/GBA 沒有 DMC；依每個 percussion source 的新 generation、MIDI
   drum kind 與 velocity-squared 權重，把原 OPL source power 分配到
   Wave kick 與 PSG Noise，兩者 root-sum-square 仍等於原 percussion
   目標。

產物：

- `org/TyrianAudioLab/Music/channel-calibration.json`
- `org/TyrianAudioLab/Music/channel-calibration.md`

播放器載入歌曲時以 LDS SHA-256 找對應紀錄，避免檔名相同但內容不同時套錯
gain。UI 的 100% 是「保持已校準比例」，不是所有 raw oscillator 使用相同
振幅。

## 驗證方式

```powershell
cd C:\ai_project\AprTyrianNes\org\TyrianAudioLab

dotnet .\bin\Release\net8.0-windows\TyrianAudioLab.dll `
  --analyze-channels .\Music .\Music\channel-calibration.json

dotnet .\bin\Release\net8.0-windows\TyrianAudioLab.dll `
  --self-test .\Music .\build\self-test.json

dotnet .\bin\Release\net8.0-windows\TyrianAudioLab.dll `
  --render-wav .\Music\18_tyrian_the_level.tym GameBoy `
  .\build\track18-gameboy.wav 30

dotnet .\bin\Release\net8.0-windows\TyrianAudioLab.dll `
  --render-wav .\Music\18_tyrian_the_level.tym GameBoyAdvance `
  .\build\track18-gba.wav 30
```

self-test 另外檢查：

- 440 Hz pulse、32-sample Wave、64-sample AGB Wave 的 period 誤差。
- 15/7-bit Noise register 與實際 clock。
- FIFO 容量 32 bytes、native rate 32,768 Hz、signed 8-bit DAC。
- Pulse 1、Pulse 2、Wave melody、Wave kick、Noise、FIFO A、FIFO B
  七條診斷輸出皆非靜音。
- Noise 在 1.5 ms retrigger 邊界的第一個 sample step 必須小於 0.02。
- Game Boy 的 DMC/expansion bus 必須為靜音。
- GBA 的 DMC bus 必須靜音，Direct Sound expansion bus 必須有訊號。
- 41 首歌 × 9 profiles 均不得產生 NaN/Infinity 或全靜音。

### 2026-07-24 實測結果

- calibration schema：`tyrian-channel-calibration-v6`
- 曲目：41
- 每曲校準 profiles：7（NES2A03、VRC6、VRC7、5B、GB、GBA、SNES）
- Game Boy 有目標的獨立 stems：165；gain 範圍 0.289–4.688。
- GBA 有目標的獨立 stems：247；gain 範圍 0.139–4.639。
- 兩 profile 都沒有 target 非零但 raw stem 靜音的案例。
- 兩 profile 都沒有撞到 0.02 或 16 的 gain clamp。
- 以儲存後的 `float` gain 回算，最差 stem RMS 誤差分別為
  `7.20e-7 dB`（GB）與 `8.50e-7 dB`（GBA）。
- Noise retrigger regression 的第一個 sample step 為 `0.001463`，
  低於 `0.02` 上限。
- self-test：41 首、9 profiles、全部通過。

Track 18 `Tyrian, The Level` 的完整 77.31 秒校準後 WAV：

| Profile | RMS dBFS | Peak dBFS | Spectral centroid | Stereo correlation |
|---|---:|---:|---:|---:|
| Original OPL2 | -26.328 | -8.537 | 1,320 Hz | 1.000 |
| Game Boy fixed | -25.605 | -9.642 | 891 Hz | 0.867 |
| Game Boy Advance fixed | -24.789 | -9.274 | 914 Hz | 0.873 |

問題最明顯的 33–40 秒密集 kick 區段，其相鄰 sample step 比對：

| Profile | Before max step | Fixed max step | Before P99.9 | Fixed P99.9 | Dense RMS change |
|---|---:|---:|---:|---:|---:|
| Game Boy | 0.304169 | 0.182526 | 0.158997 | 0.119232 | -1.329 dB |
| Game Boy Advance | 0.267822 | 0.185211 | 0.157257 | 0.117999 | -0.800 dB |

GB 最大突變降低 40.0%，GBA 降低 30.8%；P99.9 都降低約 25%。完整曲目的
整體 RMS 和舊版只差 +0.012 dB（GB）／+0.019 dB（GBA），因此改善不是把
整首歌粗暴調小，而是改正 kick timbre、Noise retrigger 與 percussion
功率分配。三個新版檔案的 DC 平均值都在 `2.5e-5` 內，沒有明顯 DC offset。

試聽檔：

- `org/TyrianAudioLab/build/diagnostics-track18-opl2-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gb-fixed-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gba-fixed-full.wav`

## 已知邊界與後續方向

- neutral `EVNT` 尚未攜帶 LDS 所有連續 tremolo、vibrato、portamento
  automation；原始 `LDS ` chunk 沒有遺失，後續可升級 converter。
- Pulse 1 sweep 硬體機制已在研究中確認，但目前不自動套用，因為 LDS
  timeline 沒有一對一 sweep 意圖，任意套用反而會改旋律。
- `GameBoy` 目前是 DMG analog cutoff；可新增 CGB 300 Hz A/B profile。
- Direct Sound generator 是「GBA 可播放的 signed 8-bit PCM 方案」，不是
  將 GBA game ROM 或 CPU instruction 送進 emulator。
- 若要再追求 GBA 實機感，可加入 SOUNDBIAS PWM bit-depth/rate、timer 0/1
  可選 rate，以及 A/B hard-left/right register routing 的 UI。
