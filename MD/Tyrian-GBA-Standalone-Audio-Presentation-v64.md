# Tyrian GBA 獨立化、音訊與 Presentation 完整驗證（v64）

日期：2026-08-01
設定：High Detail／Normal Game Speed／正式 drop-frame scheduler

## 目標

本階段把 `TyrianGbaPoc` 的有效建置鏈完全收斂為 GBA 專案，移除早期
實驗留下的 NES／SNES builder、混音規則與校準資料；同時修正 drop-frame
恢復路徑中的 Maxmod 競態、延後 VRAM 上傳遺失，以及 Episode 3 高密度
爆炸場景的 OBJ cache 容量問題。

正式驗收原則為：遊戲邏輯與音訊必須維持 wall-clock 節奏；只允許略過
無法在安全期限內完成的整張 presentation。完整關卡的 missed VBlank
上限為 1%，前端、摘要、死亡與轉場仍要求 0。

## 專案獨立化

- GBA 圖形／資源解析集中於 `tools/gba_asset_support.py`。
- GBA Maxmod IT 產生集中於 `tools/gba_music_builder.py`。
- 必要 IT 結構模板移至 `tools/templates/gba_maxmod_base.it`。
- 移除 `vendor/builders/nes`、`vendor/builders/snes` 及另一主機的
  channel calibration。
- 41 首音樂的參考量測改為
  `vendor/audio/Music/gba-opl-reference.json`，只描述專案內 TYM1／OPL
  source 與 GBA 目標，不再引用其他主機 voice map 或 mixer gain。
- `tools/audit_project_independence.py` 成為資源建置的強制 gate，會掃描
  有效 runtime／build scripts、阻擋固定工作區路徑與跨主機 builder。

最新 audit 結果：60 個有效檔案、41 首 GBA OPL reference、0 forbidden
dependency。

## GBA 音樂重建與校準

八聲道選擇由原始 OPL stem 直接決定：先保留打擊聲道，再依完整循環的
source RMS 選擇其餘聲道。輸出採 Maxmod 15,768 Hz、signed 8-bit PCM、
固定 catalog-wide +3 dB presentation reference；不做逐曲 maximum
normalization，避免把原本安靜的聲部不自然放大。

| 項目 | 結果 |
|---|---:|
| 音樂 | 41 首 |
| 校準 source stems | 308 |
| 舊 adapter 平均絕對誤差 | 13.897332 dB |
| 新 adapter 平均絕對誤差 | 0.154308 dB |
| PCM clipping | 0 |
| peak-limited stems | 22 |

End of Level、Game Over、Secret Level 的 `_once` 版本只改 IT order flow，
mmutil 會共用相同 PCM；一次性播放不再以複製整套樣本換取功能。

## Drop-frame 與 Maxmod 正確性

`mmFrame()` 會修改 EWRAM mixer cursor，而 VBlank IRQ 內的 `mmVBlank()`
可能在恢復迴圈中途重設它。現在只在 `mmFrame()` 交易期間短暫關閉 IME；
已 pending 的 VBlank 仍留在 `REG_IF`，IME 恢復後立即派送。這避免舊 cursor
覆寫相鄰 module channel state，也保留每個實體 VBlank 一次的音訊服務。

延後 presentation 時，enemy Sprite2、projectile 與 effect 的待上傳項目
不再於每次 render attempt 清除。相同 VRAM slot 的後續請求會 coalesce
成最新 source，直到安全 VBlank commit；因此 cache 不會把尚未進 VRAM
的圖誤認為 resident，畫面也不會出現透明或舊幀物件。

## 爆炸特效快取

Episode 3 實測同一 scene 需要 27 個不同的 16x16 explosion frame。正式版
配置如下：

- 24 個連續 explosion slots；
- 1 個使用 static OBJ bank 與 upper Sprite2 cache 間的四-tile alignment gap；
- 1 個只在 boss bar 未啟用時共享 boss-bar tiles；
- 1 個共享 source-parity gameplay 不使用的 legacy POC reward 最末幀。

共享區域在 boss bar／legacy reward 真正需要時會取消 pending upload、使
cache entry 失效並於同一 VBlank 還原原圖。Episode 3 完整 route 結果為
max visible unique 27、effect drops 0、pending uploads 0。

Effect metadata 以 `0xff` frame key 表示 invalid，將 27-slot metadata
從 108 bytes 壓至 82 bytes。六個只在 Boss 區段開始／結束讀寫的統計
快照搬至 EWRAM，換回 24 bytes IWRAM user-stack 空間；沒有降低 runtime
canary 或 heap 門檻。

## 完整驗證

執行：

```powershell
.\build.ps1 -KeepIntermediates -DetailLevel high -GameSpeed normal
```

| 路線 | Display frames | Missed VBlank | 比率 | 非 gameplay misses |
|---|---:|---:|---:|---:|
| Episode 2 第一關 | 10,475 | 32 | 0.3055% | 0 |
| Episode 3 第一關 | 5,291 | 16 | 0.3024% | 0 |
| Episode 4 第一關 | 10,027 | 74 | 0.7380% | 0 |

三條路線均低於 1%；Sprite2 decode failure、Sprite2 cache drop、projectile
cache drop 與 pending upload 均為 0。Episode 1 四關 campaign 為 4/4，
ROMFS 62-section matrix 為 62/62。

前端 17 條已開放路徑各執行 120 次：全部 0 missed VBlank、music active、
0 failure。最重的轉場 runtime stack canary 仍完整，最低剩餘 1,568 bytes；
EWRAM runtime heap peak 使用 4,216 bytes，保留 8,192 bytes。Release 靜態
配置另保有 13,440 bytes EWRAM 與 4,488 bytes IWRAM user-stack 空間。

最終 release ROM 為 28,285,436 bytes，SHA-256：

```text
762552B8DD3FDED234AA61F75F75E622BD2BD031951695B68FCA73D480E74C51
```

以上 hash 是 commit 前驗證產物；首頁 build ID 會在正式 commit 後重建，
因此發行 ROM 會有不同但可由 release 記錄追溯的 hash。
