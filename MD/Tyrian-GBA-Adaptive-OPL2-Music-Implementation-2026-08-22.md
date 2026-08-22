# Tyrian GBA：Adaptive OPL2 背景音樂實作結果

日期：2026-08-22  
正式建置：LOW Detail／Normal Speed／Maxmod 15,768 Hz

## 結論

背景音樂已從「短近似 wavetable＋通用程序鼓」改為「原始 LDS／OPL2 patch
離線渲染＋自適應音域 sample」。41 首曲目與九個原始 source channel 全部
保留，GBA runtime 仍使用相同的 Maxmod voice 數與 15,768 Hz mixer；新增
成本集中在 build time 與 ROM，不增加 gameplay 的即時 OPL／DSP 負擔。

這次改善的重點不是假升頻，而是讓 15,768 Hz 內的音色資訊更接近原始 OPL：

- 使用專案 vendored OpenTyrian/DOSBox OPL core，先以原生 49,716 Hz 渲染；
- 寫入原始 patch 的 ADSR、KSL/KSR、operator multiplier、feedback、waveform、
  硬體 vibrato/tremolo 與 LDS 軟體 LFO／arpeggio；
- 以 127-tap Blackman-windowed sinc 低通後降至 15,768 Hz；
- 保留真正 attack，為 sustain 音色自動尋找 loop；自然衰減的 carrier 與
  percussion 使用有界 one-shot，不循環寂靜尾端；
- 每個實際 `(source channel, patch)` 依 authored note span 配置一至三個
  root samples，IT instrument note-map 選擇最近 root；
- percussion 使用原始 OPL patch 與原始音高，不再以 kick/snare/hat 通用公式
  猜測音色；
- 量化仍採 signed 8-bit PCM，維持 GBA 容量與 Maxmod 成本的合理平衡。

## 自適應配置結果

41 首正式曲目共有 656 組實際使用的 `(source, patch)`：

| Root zones | Patch 組數 |
|---:|---:|
| 1 | 371 |
| 2 | 239 |
| 3 | 46 |

正式曲目合計產生 860 個 tonal zones 與 127 個 percussion zones；277 zones
含 OPL hardware LFO，741 zones 含 LDS software LFO。對曲目真正用到的音符，
最近 root 的最大轉調距離為 12 semitones。這個分配將 ROM 空間優先留給跨
音域較大的 patch，而不是讓所有音色機械式複製三至五份。

TYM 的 `generation` 用來區分真正 note-on 與同一發聲週期內的 pitch state
更新。IT 只在 generation 改變時重新觸發 sample，因此不會因每個 LFO／
arpeggio tick 反覆重啟 attack；patch 內建的 LFO 與 arpeggio 已烘入 loop。

## 音量與峰值校準

校準仍以原始 OPL stem、mono L+R、Maxmod module volume 896/1024 與全 catalog
固定 +3 dB presentation reference 為準，不做 per-song maximum normalization。
PCM gain 上限維持 1.075；少數安靜瞬態聲道改由 IT event volume 補足，最大
倍率 1.288720，避免為追 RMS 把 8-bit sample 烘到 clipping。

| 指標 | 結果 |
|---|---:|
| 曲目／source | 41／334 |
| 校準後平均絕對 RMS 誤差 | 0.168523 dB |
| clipped PCM samples | 0 |
| peak-limited sources | 19 |
| 最大 event-volume gain | 1.288720 |
| 最大 peak ratio | 3.014826 |

Runtime 使用者音量規則沒有改變：背景音樂仍為校準後的 90%（806/1024），
爆炸、子彈等 SFX 仍為 70%（627/1024）。

## 容量結果

| 項目 | 舊版 | 新版 | 增量 |
|---|---:|---:|---:|
| `soundbank.bin` | 1,848,584 | 6,683,320 | 4,834,736 bytes |
| LOW ROM | 27,090,348 | 31,925,084 | 4,834,736 bytes |

新 ROM 使用 32 MiB 視窗的約 95.14%，尚餘 1,629,348 bytes（約 1.55 MiB）。
41 首正式 IT 合計 6,487,530 bytes，其中 PCM 為 5,358,748 bytes。舊版 ROM、
soundbank、校準檔及 44 個 IT 已備份至忽略的
`Backup/MusicA-B/pre-adaptive-opl2-20260822/`，可供人工 A/B 或回復。

## 驗證

- 完整 LOW ROM 建置成功，GBA header 與 32 MiB 容量門檻通過；
- mmutil 接受全部 41 首循環曲目、3 個 finite cue 與既有 SFX／voice；
- JukeBox mGBA 自動流程 PASS：44 songs、播放 active、首尾環狀切歌與退出
  都正常；
- 44 個 IT 共 673 instruments、1,015 sample headers，全部 C5 rate 為
  15,768 Hz；instrument map、loop、PCM pointer 與 pattern bounds 全數有效；
- OPL bridge ABI 2 的相同輸入重複渲染結果逐 sample 相同；
- Python 模組語法檢查、asset schema、41 tracks／334 sources 與 zero-clipping
  gate 通過。

## 已知近似邊界

這不是在 GBA runtime 執行完整 LDS player。build-time sample 能完整保留 patch
本身的主要音色與 LFO，但曲目事件中少見、會在發聲途中臨時改寫的 glide、
portamento 或 F7/F2 類 command，仍受 IT／預渲染 sample 模型限制。現行策略
優先保留 attack、原始 timbre、九聲道與穩定 runtime；若日後針對少數曲目
聽出明確差異，應新增通用的 tracker pitch/effect adapter，而不是對單曲烘焙
手工修補。

另外，0.168523 dB 只量測聲道 RMS parity，不等同完整的 timbre 距離。音色
改善的工程依據是原始 OPL register/patch、attack、LFO、key zones 與原始鼓
均已取代舊近似模型；最終主觀判斷仍應以舊版／新版／PC OPL reference 做
人工 A/B。
