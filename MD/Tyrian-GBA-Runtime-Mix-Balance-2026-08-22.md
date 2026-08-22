# Tyrian GBA：Runtime 音樂／音效混音比例

日期：2026-08-22  
正式設定：LOW Detail／Normal Game Speed

## 設定

依目前既有 `896/1024` Maxmod presentation ceiling 作線性縮放：

| 類別 | 設定 | Maxmod 整數值 | 相對舊版 | 約略變化 |
|---|---:|---:|---:|---:|
| 背景音樂 | 90% | 806/1024 | 89.96% | -0.92 dB |
| 音效／語音 | 70% | 627/1024 | 69.98% | -3.10 dB |
| 暫停中背景音樂 | 正常音樂的 50% | 403/1024 | 44.98% | -6.94 dB |

`Configure.h` 新增：

- `TYRIAN_GBA_MUSIC_VOLUME_PERCENT=90`
- `TYRIAN_GBA_EFFECTS_VOLUME_PERCENT=70`

兩者範圍均為 0..100，可在日後建置前直接調整。

## 套用位置

- 音樂比例位於 Maxmod module volume 總控層，涵蓋標題、選單、關卡、
  JukeBox、勝利、Game Over 與 Secret Level；
- 關卡進出淡入淡出、事件淡出與死亡淡出都以新的 806 ceiling 計算；
- 暫停不再使用硬編碼 448／896，而是自動使用目前音樂 ceiling 的一半；
- 武器、子彈、爆炸、受擊、掉落物、選單提示與 voice 共用 Maxmod effects
  global volume 627；
- 極限壓測才使用的 GBA PSG fallback，envelope 也由 8 降為 6。

## 為何不修改離線音樂校準

音樂資產仍以既有 896 reference 校準，再於 runtime 乘上 90%。若把離線
校準本身改成 806，產生器會為了追上原始 reference 而提高 sample gain，
反而抵銷這次希望聽見的 10% 衰減。這次改動因此只影響最終 presentation，
不改 41 首曲目的聲道比例、PCM、音高或九聲道結構。

## 驗證

- LOW／正式裝備／Episode 1 Section 1／300 VBlank：
  `audio_frames=300`、`audio_frame_loss=0`、`music_active=true`；
- ARM ROM 的 song-load literal pool 已確認為 `0x326`（806）與 `0x273`
  （627），不是只有文件或 header 數值改變；
- 編譯時範圍檢查會拒絕 0..100 以外的比例，並保證兩種 Maxmod volume
  不超過既有 896 ceiling。
