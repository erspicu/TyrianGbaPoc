# Tyrian GBA：完整九聲道音樂與取樣品質升級

日期：2026-08-16  
正式設定：LOW Detail／Normal Game Speed／Maxmod 15,768 Hz

## 結論

Tyrian 的 TYM／OPL2 音樂最多同時描述九個來源聲道。舊版為控制早期 ROM
容量與 mixer 壓力，每首只保留能量最高的八個聲道；41 首曲目中有 26 首
實際使用第九聲道。全 catalog 被捨棄的 RMS 能量平均約 8.39%，最嚴重的
曲目約 26.48%。ROM 精簡後已有足夠餘裕，因此正式音訊管線改為：

- 41 首曲目完整保留九個 OPL source channel；
- Maxmod mixer slots 由 16 增至 18，容納九個音樂聲道、八個邏輯 SFX
  聲道及一個安全餘額；
- 程序產生的 kick、snare、hi-hat、crash／ride 由 11,025 Hz 提升到
  15,768 Hz，與 GBA runtime mixer rate 一致；
- 原始 `tyrian.snd`／voice PCM 維持來源原生 11,025 Hz。來源沒有更高頻
  資訊，單純插值只會增加 ROM，不會增加真實細節；
- 保留固定 OPL reference、catalog-wide +3 dB、1.60x percussion peak
  ceiling 與禁止逐曲 maximum normalization 的既有校準原則。

這個切法提升可聽見的編曲完整度與程序打擊的時間解析度，同時避免用無效
升頻假裝提升原始 SFX 品質。

## 離線校準結果

| 指標 | 舊八聲道版 | 新九聲道版 |
|---|---:|---:|
| 曲目 | 41 | 41 |
| 校準 OPL sources | 308 | 334 |
| IT 總量（含有限提示曲） | 1,504,890 bytes | 1,683,850 bytes |
| Maxmod soundbank | 1,694,108 bytes | 1,848,544 bytes |
| 校準平均絕對 RMS 誤差 | 0.154308 dB | 0.159788 dB |
| peak-limited percussion sources | 22 | 22 |
| 最大 peak/reference 比 | 1.626460 | 1.626063 |
| clipped PCM samples | 0 | 0 |

平均誤差的 0.00548 dB 變化來自新增 26 個原先未納入統計的低能量聲道，
不是既有聲道退化；仍低於 build gate 的 0.2 dB。九聲道版不再用
「挑八個」的近似策略，因此曲目的配器資訊更完整。

## ROM 與 runtime 驗證

正式 LOW ROM：

- bytes：27,090,252（25.835 MiB）；
- SHA-256：
  `af4ac2f7557ba303ca682669e453f503eb4f60d2a5395ffd6082724bbd8a5ddb`；
- 相較八聲道參考 ROM 增加 154,432 bytes；
- 距 32 MiB cartridge 上限仍約有 6.16 MiB。

聚焦驗證：

- JukeBox 自動流程：PASS；44 modules、環狀前後切歌、淡出退出與回標題
  音樂均正確；
- Episode 1／Section 1、全武器壓力、600 VBlank：`music_active=1`、
  `audio_frame_loss=0`、EWRAM heap remaining 12,288 bytes、IWRAM stack
  canary remaining 4,812 bytes；
- calibration build gate：334 sources、0 clipping、22 peak-limited、
  所有 41 首與三個 `_once` cue 均成功打包。

壓力測試的 134 次 missed VBlank 是故意把所有高負荷武器同時開啟時的
presentation 壓力；音訊 wall-clock frame loss 仍為 0，正式普通裝備版
不會使用這個壓測負載。

## 可復原備份

升級前的完整八聲道參考資源保存在 Git 忽略目錄：

`Backup/MusicA-B/2026-08-16-eight-channel-15k-reference/`

內容包含舊 ROM／SAV、全部 loop／`_once` IT、soundbank、音樂校準 JSON、
原始 SFX／voice WAV 與 asset report。備份內 README 記錄逐項還原方式。
這份資料不提交 Git，避免把可重建二進位與正式 source history 混在一起。

## 建置防退化規則

`build.ps1` 現在會拒絕以下狀況：

- source count 不是 334；
- catalog profile 不是完整九聲道 profile；
- 程序打擊不是 15,768 Hz；
- 原始 SFX 被誤標成非來源原生 11,025 Hz；
- clipping、gain、peak ceiling、有限提示曲或曲目數量不符合規格。

因此未來重建資產時，不會在不知情下退回八聲道或用無效升頻取代來源
音效。
