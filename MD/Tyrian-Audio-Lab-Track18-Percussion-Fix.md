# Tyrian Audio Lab：第 18 首 GB/GBA 尖銳爆音分析與修正

日期：2026-07-24  
曲目：18 `Tyrian, The Level`  
影響 profile：`GameBoy`、`GameBoyAdvance`

## 結論

問題不是 PCM clipping，也不是 GBA Direct Sound FIFO。尖銳突變來自把
原 OPL2 kick、snare、hi-hat 全映射到同一條 GB 15/7-bit LFSR Noise；
第 18 首的 kick 在 33–40 秒間約每 7 ticks（100.72 ms）重新觸發，LFSR
reset、寬頻波形與 AGB polarity 差異讓 kick 比原 OPL2 低頻 transient
尖銳許多。

修正後：

- kick 改用短促、可變音高的 4-bit Wave RAM transient；
- kick 和 melody 以 duck/restore time-share channel 3；
- Noise 只保留 snare、hi-hat；
- Noise 加入 9 kHz reconstruction pole；
- Noise 與 Wave kick retrigger 都使用 1.5 ms smoothstep crossfade；
- calibration schema 升到 `tyrian-channel-calibration-v5`，分開量 Wave
  kick 與 Noise 的目標功率。

## 修正前證據

- 完整 77.31 秒沒有 digital clipping：
  - GB peak：-11.396 dBFS。
  - GBA peak：-9.446 dBFS。
- 最大 GB sample step `0.304169` 發生在 37.913651 秒，與密集 kick
  retrigger 對齊。
- 33–40 秒：
  - OPL2 max step：`0.167236`。
  - GB max step：`0.304169`。
  - GBA max step：`0.267822`。
- 同一問題同時存在於沒有 FIFO 的 `GameBoy` profile，故可排除 Direct
  Sound A/B。

## Wave kick

- Wave RAM：DMG 32×4-bit；AGB 64×4-bit。
- pitch：事件音高下降兩個八度，限制 42–180 Hz。
- 起音：最高 420 Hz，20 ms 指數 pitch sweep。
- envelope：1.5 ms attack、34 ms decay、95 ms duration、12 ms release。
- melody duck：2 ms attack、6 ms restore。
- stem 4 沿用既有 `DmcGain` 儲存欄位，但 GB/GBA 的 UI DMC bus 仍完全
  靜音；Wave kick 與 Noise 一起送入 percussion bus。

這個做法刻意維持四聲道硬體邊界。melody 與 kick 分開合成是為了量測，
輸出時 kick 存在便 duck melody，不把它當作可同時全音量播放的第五聲道。

## v5 校準

每個 percussion source 的新 generation 事件依 MIDI drum kind 分類：

- MIDI 35/36：Wave kick。
- MIDI 38/40：Noise snare。
- MIDI 42/44/46/49/51：Noise hi-hat/cymbal。

每個事件以 `velocityAmplitude²` 作功率權重，再把該 OPL source 的完整
loop RMS power 分到 kick/noise。兩個目標的 root-sum-square 保持原
percussion power。

第 18 首：

| Profile | Noise target / raw / gain | Wave kick target / raw / gain |
|---|---|---|
| GB | 0.024585 / 0.021662 / 1.1349 | 0.027518 / 0.022599 / 1.2177 |
| GBA | 0.024585 / 0.021589 / 1.1388 | 0.027518 / 0.022997 / 1.1966 |

41 首全量結果：

- GB：165 個有目標 stems；gain 0.289–4.688。
- GBA：247 個有目標 stems；gain 0.139–4.639。
- 無 target 非零但 raw stem 靜音，無 gain clamp。
- 最大回算誤差：GB `7.20e-7 dB`；GBA `8.50e-7 dB`。

## 前後量測

33–40 秒密集 kick 區：

| Profile | Before max step | Fixed max step | 降幅 | Before P99.9 | Fixed P99.9 |
|---|---:|---:|---:|---:|---:|
| GB | 0.304169 | 0.182526 | 40.0% | 0.158997 | 0.119232 |
| GBA | 0.267822 | 0.185211 | 30.8% | 0.157257 | 0.117999 |

密集區 RMS 分別降低 1.329 dB 與 0.800 dB，但完整歌曲 RMS 與舊版只差
+0.012 dB／+0.019 dB，代表修正集中在錯誤 percussion timbre 與瞬態，
不是整首降音量。新版仍有的最大 step 接近原 OPL2 合法 waveform transition，
不再是單一 LFSR kick reset 的突出離群值。

## Regression

`tyrian-audio-lab-self-test-v5` 已通過：

- 41 首 × 8 profiles。
- 41 首皆有 6 組 stored calibration。
- 七條 GB/GBA 診斷 stem 都非靜音。
- Noise retrigger 第一 sample step：`0.001463`，門檻 `< 0.02`。
- GB DMC/expansion bus 維持靜音；GBA DMC 靜音，FIFO expansion 有訊號。

以上為該里程碑當時的 v5 結果；加入 SNES profile 後目前 catalog／
self-test schema 已升為 v6，GB/GBA percussion 修正與數值維持不變。

試聽與診斷檔：

- `org/TyrianAudioLab/build/diagnostics-track18-opl2-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gb-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gb-fixed-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gba-full.wav`
- `org/TyrianAudioLab/build/diagnostics-track18-gba-fixed-full.wav`
