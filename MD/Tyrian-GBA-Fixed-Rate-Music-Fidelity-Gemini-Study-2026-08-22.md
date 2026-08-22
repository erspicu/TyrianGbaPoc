# Tyrian GBA 固定 15,768 Hz 音樂真實度改善研究

日期：2026-08-22  
狀態：Gemini 3.1 Pro 兩輪諮詢、本地校驗、轉換器實作與完整 ROM 建置均已完成。

> 最終實作採容量受控的一至三個 adaptive roots，而不是研究初稿的三至五個
> 固定 zones。完整結果與限制見
> `Tyrian-GBA-Adaptive-OPL2-Music-Implementation-2026-08-22.md`。

## 目標與固定條件

本研究回答的問題是：GBA ROM 尚有數 MiB 空間時，能否在不改變
Maxmod 最終 15,768 Hz mixer rate 的前提下，讓 Tyrian 的 OPL2 音樂更接近
PC 原曲。

固定限制如下：

- 最終 PCM／Maxmod 輸出率保持 15,768 Hz；不以假升頻增加檔案大小。
- 保留 9 個原始 OPL2 音樂聲道，不增加同時 mixer voice 數。
- 不在 ARM7TDMI runtime 執行完整 OPL emulator 或高成本 DSP。
- 目前 ROM 為 27,090,348 bytes；距 32 MiB 上限尚有 6,464,084 bytes，
  約 6.16 MiB。
- 現有 `soundbank.bin` 為 1,848,584 bytes。

## 現況量測

目前 44 個 IT（41 首正式曲目與 3 個 once cue）包含：

| 項目 | 數值 |
|---|---:|
| IT sample headers | 673 |
| mmutil 可辨識的 unique PCM | 500 |
| 所有 IT header 引用的 PCM 合計 | 456,612 bytes |
| unique PCM 合計 | 404,539 bytes |
| tonal loop headers | 556 |
| unique tonal PCM | 383 |
| 每份 tonal PCM | 301 bytes |
| percussion headers／unique PCM | 117／117 |

現有 tonal sample 只含 60-sample lead 與 241-sample、四週期 sustain loop，
同一 `(voice, instrument)` 的 120 個音符都由 Maxmod 轉調同一份 sample。
簡化合成器只近似少數 operator level、feedback 與 waveform，沒有完整重現
OPL2 ADSR、KSL/KSR、multiplier、feedback、vibrato、tremolo 及 rhythm mode。

## Gemini 建議與最終判斷

Gemini 3.1 Pro 的兩輪建議，經本地程式與 soundbank 實測修正後，最有價值的
方向如下：

1. **用完整 OPL core 在 build time 離線渲染。** 專案已包含
   OpenTyrian 使用的 OPL2/OPL3 core，可以先直接重用，餵入真實 register、
   note 與 rhythm event。建議先以 OPL 原生約 49.7 kHz 產生高精度中間訊號，
   再用 band-limited resampler 降至 15,768 Hz。最終取樣率不變，但可保留
   15,768 Hz Nyquist 範圍內更多正確泛音並減少 aliasing。
2. **把單 sample note map 改成 adaptive key-zone map。** 每次 note-on 仍只
   選一份 sample，因此 9 個音樂 voice 與 runtime mixer 成本不變。第一輪可
   先做 3 zones；之後依曲目實際使用音域與 log-spectral error，只有跨音域
   誤差大的 patch 擴成 5 zones，不應把所有音色機械式膨脹。
3. **保留真實 attack／timbre evolution，再接自動搜尋的 sustain loop。** 先
   離線 render 較長片段，依 envelope derivative、自相關、波形值與一階導數
   找 loop；不能假設所有音色在 60 或 100 ms 已達穩態。
4. **對 AM／VIB 音色保留 LFO。** 短 241-sample loop 會凍結 LFO。應為少數
   LFO-critical patch 使用完整 LFO 週期的長 loop，或在最小測試確認 Maxmod
   支援程度後轉成 tracker vibrato／tremolo；不要讓所有音色承擔長 loop。
5. **真實渲染 OPL rhythm percussion。** 以 register tuple、pitch 與必要的
   velocity zone 作 key，輸出含完整 decay 的 one-shot，取代目前通用程序鼓。
6. **維持 8-bit PCM。** 16-bit 會增加 ROM、匯流排與混音成本；實際倍率仍須
   benchmark，不能直接宣稱必然精確翻倍。現階段 8-bit 是合理選擇。

### 不值得用 ROM 空間換取的方案

- 把同一份 15,768 Hz 資料升頻後再標回 15,768 Hz，不會增加音訊資訊。
- 直接存 41 首完整 raw PCM 不可行：6,464,084 / 15,768 僅約 410 秒，
  即 6.83 分鐘。
- velocity crossfade layers 會增加同時 mixer voice；若要做 velocity layer，
  應在 note-on 時離散選一份 sample。
- 所有音色一律使用五層、固定長 attack／loop，會浪費 ROM，也可能把錯誤
  loop 複製五份。

## PCM 容量估算

下表依 383 份 unique tonal PCM 計算；60 ms 為 946 bytes，100 ms 為
1,577 bytes，每 zone 再加 241-byte sustain。淨增量已扣除目前
383 × 301 = 115,283 bytes。

| Zones／attack | 新 tonal PCM | PCM 淨增量 | metadata 淨增量估計 | 合計增量估計 |
|---|---:|---:|---:|---:|
| 3／60 ms | 1,363,863 | 1,248,580 | 0.06–0.08 MiB | 約 1.25–1.27 MiB |
| 3／100 ms | 2,088,882 | 1,973,599 | 0.06–0.08 MiB | 約 1.94–1.96 MiB |
| 5／60 ms | 2,273,105 | 2,157,822 | 0.12–0.15 MiB | 約 2.18–2.21 MiB |
| 5／100 ms | 3,481,470 | 3,366,187 | 0.12–0.15 MiB | 約 3.33–3.36 MiB |

這只是固定短 loop 的基準；LFO 長 loop、完整 percussion、alignment 與
soundbank metadata 仍會增加容量。因此不建議直接用滿 6.16 MiB，音樂升級
以新增 3.0–4.2 MiB 為較安全目標，至少保留約 1.5 MiB 給後續內容與對齊。

## 對 Gemini 回答的必要修正

### mmutil 去重不需要重建 soundbank 格式

第一輪回答宣稱必須先建立跨 module instrument bank 才能去重，與本專案
實測不符。41 個 module 加入 3 個 PCM 完全相同的 once module 後，unique
sample 數不變，soundbank 只增加 4,028 bytes。現有 mmutil 1.10.1 已能在
同一次建置中全域去重 byte-identical PCM；應新增 build audit，而不是改寫
soundbank reference 格式。

### Peak-normalized PCM 再由 mixer 衰減不一定降低 SNR

第二輪回答認為 peak-normalized 8-bit PCM 在 runtime 降音量會比預先把音量
烘小更容易產生量化噪聲，這不能直接成立。若 Maxmod mixer 使用足夠寬的中間
精度，先充分利用 8-bit sample range、再於 mixer 衰減，通常反而比先把波形
量化成低振幅 8-bit PCM 保留更多來源解析度。必須以最小 module 驗證 Maxmod
sample/global/instrument volume 的實際支援、解析度與混音結果，再決定是否將
per-source gain 從 PCM 移到 metadata／event。

### Dither 不應一概排除

短 loop 內使用非週期性隨機 dither 確實會產生接縫或週期噪聲，但不代表所有
dither 都有害。第一輪 A/B 應比較：

1. 全段 round-to-nearest、無 dither；
2. attack／non-loop 使用 TPDF，sustain loop 無 dither；
3. attack 使用 high-pass TPDF，sustain loop 無 dither。

periodic loop-aware dither 可工程化，但短 loop 會把噪聲圖樣音調化，優先度低。

### A/B 成功標準應比較原始 OPL reference

Gemini 第二輪所寫「盲測無法分辨新版與舊版」不是正確成功標準。新版應比舊版
更接近同一事件流的完整 OPL reference；若與舊版聽不出差別，只代表改動可能
沒有價值。

## 建議的三階段路線

### Phase 1：代表曲目 3-zone A/B

- 將 instrument 資料模型由 `1 instrument -> 1 sample` 改成
  `1 instrument -> N key-zone samples`，IT note map 指向最近 zone。
- 以 vendored OpenTyrian OPL core 離線渲染，先輸出 3 zones；中間高取樣率
  經低通降至固定 15,768 Hz。
- 建立 attack／loop 自動偵測與 round-trip audit。
- 先測 Track 1 `Asteroid Dance Part 2`（鼓與 bass）、Track 18
  `Tyrian, The Level`（已知尖銳聲道、pad／lead）、Track 30
  `Tyrian: The Song`（標題曲、長音與整體觀感）。

### Phase 2：自適應 zone、LFO 與 rhythm percussion

- 依實際 note range 與 stem spectral error，只有必要音色從 3 zones 擴為
  5 zones。
- AM／VIB patch 使用完整 LFO loop 或經驗證的 tracker effect。
- 將 117 份程序 percussion 改為真實 OPL rhythm one-shot，按 register、
  pitch、velocity tuple 去重。
- 驗證把 fixed gain 移到 Maxmod metadata/event 是否能維持 RMS parity 並
  增加 byte-level dedup；失敗時保留 gain-baked PCM，不為省空間犧牲音質。

### Phase 3：全曲建置與校準

- 套用至 41 首／334 source channels，維持 9 voice 上限。
- 重新校準 per-source RMS，但不做 per-song maximum normalization。
- 對舊版、新版及完整 OPL reference 執行 multi-resolution STFT／
  log-spectral distance、spectral centroid／band energy、envelope error、
  pitch error、loop value／derivative discontinuity，並做主觀 ABX。
- 以 soundbank／ROM 增量不超過約 4.2 MiB、ROM 保留至少約 1.5 MiB、
  runtime voice 數不增加、CPU／missed VBlank 無可測退化為發布門檻。

## 結論

剩餘 ROM 空間足以帶來實質改善。最推薦的不是提高取樣率或存整首 PCM，而是
把目前「一份很短、簡化公式生成、跨全音域轉調的 wavetable」改成：

> 完整 OPL build-time render + 自適應 3～5 key zones + 真實 attack／
> sustain/LFO loop + OPL rhythm one-shot + 8-bit loop-safe 量化。

這套方案把成本放在 build time 與 ROM，note-on 仍只選一份 sample，因此不
增加 Maxmod 同時 voice 數，最符合目前 GBA 關卡 runtime 已接近負荷上限的條件。

## 追加定案：背景音樂 source rate 不低於 15,768 Hz

第二輪取樣率研究曾評估把部分音色降至 11,025 Hz，以節省約 30% 的個別 PCM
容量；但反組譯本專案實際使用的 Maxmod 1.10.1 `libmm.a` 後，已確認 GBA
mixer 的核心是：

```asm
ldrb sample, [source, phase, lsr #12]
add  phase, phase, step
```

它沒有讀取相鄰 sample，也沒有 fractional-phase interpolation。11,025 Hz
source 混到 15,768 Hz output 時，root step 約為 0.699，仍需不均勻地重複
sample；15,768 Hz zone root 則可接近 step 1.0 的逐 sample 對應。降低 source
rate 不會減少 mixer 每個 output sample 的迴圈成本，只能省 ROM，卻會增加
nearest-sample phase stepping 的 imaging／jitter 並失去 5.5～7.9 kHz 頻帶。

因此正式規則定案如下：

- **所有背景音樂 tonal 與 percussion source PCM 最低均為 15,768 Hz。**
- OPL 可在 build time 以原生約 49.7 kHz 或更高精度渲染，再用 band-limited
  resampler 降至 15,768 Hz；這不改變 GBA 最終 mixer rate。
- ROM 空間用於 adaptive 3～5 key zones、真實 attack、LFO loop 與 OPL
  rhythm percussion，不以降低背景音樂 source rate 換容量。
- 若未來 ROM 空間不足，先把非關鍵音色由 5 zones 降為 3 zones、縮短經驗證
  可縮的 sustain loop，或減少低收益資料；背景音樂不降至 11,025 Hz。
- stock SFX／voice 原始資料仍維持來源原生 11,025 Hz，再由 Maxmod 混入
  15,768 Hz output；它們不屬於這條背景音樂規則。

歷史上也不是整套背景音樂都使用 11,025 Hz：舊 tonal wavetable 的 C5 speed
是 16,744 Hz；只有程序 percussion 曾使用 11,025 Hz，stock SFX／voice 也一直
是 11,025 Hz。現在 tonal 與 percussion 統一為 15,768 Hz 才是正式音樂路徑。
