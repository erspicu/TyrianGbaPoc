# Tyrian GBA：Adaptive OPL2 背景音樂實作結果

日期：2026-08-22（2026-08-23 更新長音生命週期修正）
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
  percussion 使用有界 one-shot，不循環寂靜尾端；finite tonal one-shot 會
  依曲目真正的 note generation 長度渲染，不再固定於 420 ms 截斷；
- 每個實際 `(source channel, patch)` 依 authored note span 配置一至三個
  root samples，IT instrument note-map 選擇最近 root；
- percussion 使用原始 OPL patch 與原始音高，不再以 kick/snare/hat 通用公式
  猜測音色；
- 量化仍採 signed 8-bit PCM，維持 GBA 容量與 Maxmod 成本的合理平衡。

## 自適應配置結果

41 首正式曲目共有 656 組實際使用的 `(source, patch)`：

| Root zones | Patch 組數 |
|---:|---:|
| 1 | 412 |
| 2 | 204 |
| 3 | 40 |

正式曲目合計產生 813 個 tonal zones 與 127 個 percussion zones；其中 512
個 tonal zones 是 sustain loop，301 個是自然衰減 one-shot；265 zones 含
OPL hardware LFO，699 zones 含 LDS software LFO。47 組長尾 finite patch
在不超過 15 semitones 的條件下減少重複 root，將有限的 32 MiB ROM 優先
留給真正的長音尾端。對曲目實際音符，最近 root 的最大轉調距離為 15
semitones。

TYM 的 `generation` 用來區分真正 note-on 與同一發聲週期內的 pitch state
更新。IT 只在 generation 改變時重新觸發 sample，因此不會因每個 LFO／
arpeggio tick 反覆重啟 attack；patch 內建的 LFO 與 arpeggio 已烘入 loop。

## 音量與峰值校準

校準仍以原始 OPL stem、mono L+R、Maxmod module volume 896/1024 與全 catalog
固定 +3 dB presentation reference 為準，不做 per-song maximum normalization。
PCM gain 上限維持 1.075；少數安靜瞬態聲道改由 IT event volume 補足，最大
倍率 1.239861，避免為追 RMS 把 8-bit sample 烘到 clipping。

| 指標 | 結果 |
|---|---:|
| 曲目／source | 41／334 |
| 校準後平均絕對 RMS 誤差 | 0.022327 dB |
| clipped PCM samples | 0 |
| peak-limited sources | 7 |
| 最大 event-volume gain | 1.239861 |
| 最大 peak ratio | 2.701857 |

Runtime 使用者音量規則沒有改變：背景音樂仍為校準後的 90%（806/1024），
爆炸、子彈等 SFX 仍為 70%（627/1024）。

## 容量結果

| 項目 | 舊版 | 新版 | 增量 |
|---|---:|---:|---:|
| `soundbank.bin` | 1,848,584 | 8,230,316 | 6,381,732 bytes |
| LOW ROM | 27,090,348 | 33,472,076 | 6,381,728 bytes |

新 ROM 使用 32 MiB 視窗的約 99.7546%，尚餘 82,356 bytes（約 0.0785 MiB）。
41 首正式 IT 合計 8,030,392 bytes，其中 PCM 為 6,905,558 bytes。舊版 ROM、
soundbank、校準檔及 44 個 IT 已備份至忽略的
`Backup/MusicA-B/pre-adaptive-opl2-20260822/`，可供人工 A/B 或回復。

## 2026-08-23：第 11／15／16 首斷續修正

斷續不是 49,716 → 15,768 Hz sinc resample 造成，而是舊生命週期判斷把所有
carrier EGT=0 的 tonal patch 一律當作短 one-shot，且 `_trim_one_shot()`
無條件在 420 ms 截斷。第 11 首的約 2.07 秒長音、第 15 首最長約 9.2 秒的
finite tail，以及第 16 首 6～62 秒的稀疏 note generation，都會在 envelope
仍可聽見時突然歸零，直到下一個 note-on 才恢復，因而聽成斷斷續續。

修正後：

- 直接依 OPL operator 的 EGT、decay rate、sustain level、release rate 與
  additive/FM connection 判定「可無限持續」或「自然衰減」；
- sustain patch 渲染 1.5～3 秒分析窗，再產生具 attack 的緊湊循環，loop
  seam 以交叉淡化降低 click；
- finite tonal patch 依每個 TYM generation 的真實持續時間及 root transpose
  速率配置 PCM，只在低於 -58 dB 後裁掉尾端；
- production build 會拒絕「曲目要求的 hold 仍可聽見、sample 卻提早結束」；
- 最長 authored hold 為 62.068966 秒；最長實際 finite sample 為
  10.207128 秒，證明流程不再受 420 ms 上限控制。

## 驗證

- 完整 LOW ROM 建置成功，GBA header 與 32 MiB 容量門檻通過；
- mmutil 接受全部 41 首循環曲目、3 個 finite cue 與既有 SFX／voice；
- JukeBox mGBA 自動流程 PASS：44 songs、播放 active、首尾環狀切歌與退出
  都正常；
- 新增 4 個 lifecycle regression tests：精確 envelope 分類、第 11 首長音
  loop、第 15／16 首 finite tail，以及 41 首全 catalog／PCM 容量門檻全數
  通過；
- 44 個 IT 共 673 instruments、967 sample headers（534 loop、433
  one-shot），全部 C5 rate 為
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

另外，0.022327 dB 只量測聲道 RMS parity，不等同完整的 timbre 距離。音色
改善的工程依據是原始 OPL register/patch、attack、LFO、key zones 與原始鼓
均已取代舊近似模型；最終主觀判斷仍應以舊版／新版／PC OPL reference 做
人工 A/B。
