# Tyrian GBA v55：TYM → IT / Maxmod 音樂校準

日期：2026-07-30
分支：`frontend-data-camera-audio-v55`

## 問題結論

舊 GBA ROM 的 41 首 IT 雖然讀取正確的 TYM 事件與 velocity，聲道增益卻
直接沿用 `SuperNintendo` profile。該增益原本是針對 SNES S-DSP／BRR
聲學模型量測，不是針對 GBA Maxmod 的 signed 8-bit PCM mixer。舊流程還
會用每首歌最大的 voice gain 再正規化一次，因此：

- 不同歌曲沒有共同的絕對響度基準；
- 某首歌最強的聲道會改變其餘聲道比例；
- 稀疏 percussion 若用過短 sample 表示，為了追上整曲 RMS 會把瞬態放得
  過大，容易出現刺耳聲；
- SNES profile 的 ADSR、BRR、echo 與 S-DSP panning 假設並不存在於 GBA
  Maxmod PCM adapter。

## 新流程

`tools/music_maxmod_calibration.py` 對每首 TYM 做完整事件時間軸量測：

1. 保留 TYM 已包含 carrier total-level／channel volume 的 velocity。
2. 仍使用已驗證的最多八個 OPL source channel 選擇，但不再使用 SNES
   gain。
3. 以 Maxmod 實際 IT volume-column 量化、signed 8-bit sample 及 ROM
   runtime module volume `896/1024` 量測每個 source。
4. 以 `channel-calibration.json` 的完整 OPL2 stem RMS 為固定 reference；
   因原始 OPL2 與 GBA 內建喇叭都是 mono reference，採用 Maxmod
   `L+R` fold-down 校準。IT 內的 stereo pan 仍完整保留。
5. 全 catalog 只加一次固定 `+3 dB` presentation gain，讓音樂與 SFX
   共存時不致太小聲；不做 per-track／per-sample master normalization。
6. 8-bit PCM 量化後再迭代量測，build 會拒絕 sample clipping、無來源
   target、錯誤 track mapping 或超出增益上限。
7. percussion 另設原版 peak 的 `1.60x` 軟上限；22 個稀疏 percussion
   source 因此寧可略低於 RMS target，也不放大成刺耳瞬態。

程序鼓聲亦改為依 General MIDI drum 類型使用不同長度：

- kick、snare、closed/open hi-hat、crash/ride 不再共用同一個 46 ms
  短 noise sample；
- 每個 one-shot 具有 1.5 ms smoothstep attack、5 ms release；
- 只做極低頻 DC blocker，不套用會改變全 catalog 音色的固定 120 Hz
  high-pass 或 8 kHz shelf。

## 量測結果

本次 41 首歌曲共校準 308 個實際 source：

| 指標 | 結果 |
|---|---:|
| gain 範圍 | 0.028249449 ～ 0.626500145 |
| 舊 SNES gain model 平均絕對 RMS 偏差 | 8.572177 dB |
| 新模型平均絕對 RMS 偏差（含 peak-limited source） | 0.154308 dB |
| peak-limited percussion source | 22 |
| 最大量化後 peak/reference 比 | 1.626460 |
| clipped sample | 0 |
| per-song maximum normalization | 0 |

「舊 gain model 偏差」是在同一個新版 PCM/event adapter 上替換回舊增益的
控制組，不宣稱是舊 ROM 的實機錄音 LUFS。真正主觀 A/B 仍應直接播放兩個
保留的 ROM。

完整逐曲、逐 source 資料由 build 產生：

`res/music_maxmod_calibration.json`

`res/` 是可重建輸出，不提交 Git；`build.ps1` 會驗證 schema、41 tracks、
308 sources、gain 上限、peak ceiling、誤差與 clipping。

## `_once.it` 是否重複

以下三組看似重複，實際具有不同 order-flow：

- `09`：End of Level；
- `10`：Game Over；
- `30`：Secret Level。

一般檔保留來源 Bxx loop，供 Jukebox／循環播放；`_once` 版本只移除 Bxx，
供遊戲流程用 `MM_PLAY_ONCE` 自然到達 order-list 結尾。直接刪除任一版本
會使 Jukebox 不能循環，或使勝利／死亡／Secret Level 音樂無限播放。

以同一組新版資源實測 mmutil：

| 打包方式 | Modules | Unique samples | soundbank bytes |
|---|---:|---:|---:|
| 保留三個 `_once` | 44 | 483 | 1,505,368 |
| 移除三個 `_once` | 41 | 483 | 1,501,340 |

Maxmod 已全域去重相同 PCM；三個獨立 order/pattern 只增加 4,028 bytes。
為保留精確播放語意，這 0.004 MB 不再冒險精簡。

完整 IT SHA-256 不同（因 Bxx 控制資料不同），但逐 sample payload 相同：

| Cue | loop IT SHA-256（前 12 碼） | once IT SHA-256（前 12 碼） | PCM payload |
|---|---|---|---|
| 09 | `6f974999b623` | `03cf8a398533` | 7/7 相同 |
| 10 | `23d9f25ae316` | `459ed29a724d` | 6/6 相同 |
| 30 | `cc72b5d6214f` | `58a90405da36` | 4/4 相同 |

## 本機 A/B

以下資料保存在被 Git 忽略的 `Backup/MusicA-B/`：

- `TyrianGBA-v55-music-legacy.gba`
- `TyrianGBA-v55-music-maxmod-calibrated.gba`
- `legacy-it/`
- `maxmod-calibrated-it/`
- `music_maxmod_calibration.json`

舊 ROM：

- bytes：26,237,068
- SHA-256：
  `b48eb6c5bbc921ce304b3e763ac0016b5378e86f28add828d8a05dd2128c1c08`

新版 ROM：

- bytes：26,348,524（較舊版增加 111,456 bytes，仍低於 32 MiB）
- SHA-256：
  `196b40a48cc61a887079e6811b20ca3110a5be6066f8ecc356ed4c9f9894a4c3`

建議先比較第 18 首、percussion 密集曲，以及舊版曾感覺低頻轟隆或尖銳的
曲目；再檢查背景音樂與爆炸／語音 SFX 的相對大小。

## Gemini 建議的採用與修正

採用：

- 固定 OPL reference，不做每首歌 peak normalization；
- 保留 TYM event volume；
- 以離線 stem RMS、peak、crest／transient 與 A/B 異常值檢查；
- runtime 不加入 limiter 或 noise shaper，避免浪費 ARM7TDMI。

修正：

- 本專案的 TYM velocity 已包含 carrier loudness，因此不能把
  「sample waveform 使用固定 peak reference」誤認為 carrier TL 已遺失；
- 120 Hz high-pass 與 8 kHz shelf 不適合不分曲目全面套用。新版只處理
  DC 與 one-shot 邊界，避免把 Tyrian 原始音色一律 EQ 成同一種聲音；
- 目前沒有可重現的實機／mGBA Direct Sound WAV dump，因此不把離線模型
  稱為最終 LUFS 真值。ROM A/B 仍是合併前的人耳驗收依據。
