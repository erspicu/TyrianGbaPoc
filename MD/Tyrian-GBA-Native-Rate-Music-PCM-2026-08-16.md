# Tyrian GBA：背景音樂 PCM 對齊 15,768 Hz

日期：2026-08-16  
正式設定：LOW Detail／Normal Game Speed／Maxmod 15,768 Hz

## 結論

背景音樂的音調音色與程序打擊 PCM 已全部改用 15,768 Hz，與 GBA 版
Maxmod 最終混音輸出率一致。41 首音樂、334 個 OPL2 source channel 及
九聲道配置均完整保留。

stock `tyrian.snd`／voice 的原始 PCM 仍維持來源原生 11,025 Hz；這些來源
本來沒有 15,768 Hz 的高頻資訊，離線插值只會增加 ROM，不能增加真實細節。
它們在播放時仍由 Maxmod 混入 15,768 Hz 最終輸出。

## 為何不能只把 16,744 改成 15,768

舊音調 wavetable 的基礎週期是 64 samples，IT 的 C5 speed `16,744` 同時
也是調音基準：`16,744 / 64` 約等於中央 C。若只把欄位改為 15,768，整套
音樂會下降約一個半音，並不是正確的取樣率轉換。

新版改用：

- C5 playback／PCM rate：15,768 Hz；
- loop：241 samples、四個完整週期；
- 前置區：60 samples；
- 每個音調 sample：301 bytes；
- 實際 C5：261.709544 Hz；
- 相對標準中央 C 的誤差：+0.555614 cent。

因此 C5 路徑可接近 Maxmod 1:1 取樣，音高誤差遠低於 1 cent，聽感上不可辨，
也不會因這次改動而讓所有曲目降調。

## 離線校準比較

| 指標 | 九聲道 v89 | 15,768 Hz native-rate |
|---|---:|---:|
| 曲目 | 41 | 41 |
| OPL2 sources | 334 | 334 |
| 平均絕對 RMS 誤差 | 0.159788 dB | 0.147180 dB |
| clipped PCM samples | 0 | 0 |
| peak-limited sources | 22 | 22 |
| Maxmod soundbank | 1,848,544 bytes | 1,848,584 bytes |
| LOW ROM（驗證建置） | 27,090,252 bytes | 27,090,300 bytes |

soundbank 只增加 40 bytes，LOW ROM 增加 48 bytes；ROM 容量成本可以忽略，
而 catalog-wide 校準誤差略降約 0.012608 dB。

## Runtime 驗證

- JukeBox 自動流程：PASS；首尾環狀前後切歌、淡出退出及回到標題音樂正常；
- Episode 1／Section 1、全武器壓力、600 VBlank：
  `audio_frames=600`、`audio_frame_loss=0`、`music_active=true`；
- 同一壓測觸發 133 次 missed VBlank，但音訊 wall-clock 更新沒有漏幀；
- asset／calibration gate：41 tracks、334 sources、0 clipping、
  22 peak-limited sources，音調與程序打擊率均為 15,768 Hz。
- binary IT audit：44 個 modules、673 個 sample headers 全部為 15,768 Hz；
  所有 looped tonal samples 均為 301-byte／loop 60..301 的新格式。

完整歷史 gameplay golden 目前另有既存 LOW 邏輯數值差異，因此沒有把舊
golden 任意改寫成新值來替音訊改動過關。本次改以 JukeBox 與高負荷關卡的
聚焦音訊 telemetry 驗證，兩者均通過。

## 建置防退化規則

`build.ps1` 現在會明確檢查：

- calibration catalog 的 `tonalPcmRate` 與
  `proceduralPercussionRate` 都必須是 15,768；
- asset report 的 loop 必須是 241 samples／四週期；
- C5 音高誤差必須小於或等於 1 cent；
- 原始 SFX／voice 仍必須標記為來源原生 11,025 Hz。

這些規則可防止日後重建時誤退回 16,744 metadata，或以直接改 C5 值的方式
意外破壞全曲音高。

本次執行完整 gate 時，也發現 `build.ps1` 尚未同步同一個既有提交內已改變
的三類固定契約：ROMFS retained sources、四組 front-end 產物 CRC，以及目前
retail IWRAM 靜態餘量。門檻已依 committed manifest、可重現資產輸出與既有
2,028-byte stack peak 量測同步；IWRAM 仍保留 2,688 bytes 靜態空間，且 runtime
canary 的 512-byte 最低餘量規則沒有降低。這些同步不改遊戲或音訊行為。
