# Tyrian GBA v29：Boss Sprite2 raw／EWRAM L2 效能修正

更新日期：2026-07-27
工作分支：`opentyrian-source-parity-port`

## 結論

第一關 Boss 卡頓不是 mGBA、PC 效能或 30 Hz 遊戲邏輯造成，而是 24 格
OBJ L1 cache 在多部件 Boss 動畫期間反覆淘汰。同一批 `.shp` Sprite2
元件因此在 gameplay 熱路徑重複執行 ROMFS lookup、RLE 解碼、palette
映射與 tile 重排。

v29 保留 LVL／HDT 決定圖號、size、filter 與 draw order 的原始流程，只把
不變的 Sprite2 RLE 解壓移到 build 階段，並在 runtime 加入 64 格 EWRAM
上色後 L2。Boss 區段 missed VBlank 由 437 降到 4；整關由 625 降到 13。

| 指標 | v28 基準 | v29 | 改善 |
|---|---:|---:|---:|
| 全關 display frames | 13,509 | 13,509 | 流程相同 |
| 全關 missed VBlank | 625 | 13 | 97.9% |
| Boss display frames | 1,781 | 1,781 | 視窗相同 |
| Boss missed VBlank | 437 | 4 | 99.1% |
| Boss missed 比例 | 24.54% | 0.22% | -24.31 pp |
| Boss Sprite2 L1 miss | 432 | 432 | workload 相同 |
| Boss L1 eviction | 432 | 432 | workload 相同 |
| Boss upload bytes | 411,648 | 411,648 | presentation 相同 |
| Boss projectile miss | 146 | 146 | workload 相同 |

L1 miss、eviction、DMA bytes 與 projectile miss 均未減少，證明結果不是
刪掉 Boss 動畫、少畫物件或改變 gameplay 才換得。

## Build-time raw catalog

`tools/build_assets.py` 現在完整處理 37 個 logical Sprite2 banks，每個
bank 304 個 component：

```text
37 × 304 × 12 × 14 = 1,889,664 bytes
```

資料是 12×14 row-major 的原始 PC 256 色索引：

- `0` 表示透明；stock streams 的 opaque pixel 不使用 palette index 0。
- `1..255` 原樣保存，不在 build 時套 GBA palette。
- 大型敵人的 `graphic +0,+1,+19,+20` 組合仍在 runtime 決定。
- filter 仍在 runtime 執行 `filter | (pixel & 0x0f)`。
- 原始 `newsh*.shp`／`tyrian.shp` 仍保留於 ROMFS，作 debug fallback
  與無損驗證來源。

固定 audit：

| 項目 | 值 |
|---|---:|
| Components | 11,248 |
| Raw bytes | 1,889,664 |
| Raw CRC32 | `aca11e49` |
| Raw SHA-256 | `a6c475d5c02264e8c761eb9ceb208ccbd2f01ef19b29e4e1ca547334b9993819` |
| Source stream bytes | 1,119,622 |
| Source stream CRC32 | `5b6084ce` |

這是全 bank 無損轉碼，不是 per-level、event-limited 或手工圖號表；後續
關卡不需新增 Python 規則。

## Runtime L2

L2 key 與 PC presentation state 相同：

```text
(shape_table, graphic, size, filter)
```

每格保存 palette/filter 已套用、GBA tile order 的 32×32 8bpp frame。
流程為：

```text
L1 miss
  -> 64-slot L2 lookup
  -> L2 hit: 直接排程 VBlank DMA
  -> L2 miss: ROM raw index -> current palette/filter -> EWRAM
  -> VBlank DMA -> OBJ VRAM
```

Projectile 共用同一 L2，但在前 256 bytes 保存 16×16 tile layout。
一般 12×14 enemy 的 compact L1 slot，從 L2 frame 的 tiles
`5,6,9,10` 上傳，座標與 v28 相同。

第一關 Low Detail／Normal Speed：

| L2 指標 | 全關 | Boss |
|---|---:|---:|
| Hit | 568 | 548 |
| Miss | 164 | 30 |
| Eviction | 100 | 30 |
| Raw build | 164 | 30 |
| RLE fallback | 0 | 0 |
| Drop | 0 | 0 |
| 同幀最大 unique | 16 | — |

Palette setup 會完整 flush L2；projectile L1 也在換關時失效，避免沿用舊
palette。

## EWRAM／IWRAM／ROM

獨立增加 64 KiB L2 會超出現有 EWRAM 預算。Mode-4 front-end 的
38,400-byte frame scratch 與 gameplay 不會同時使用，因此 v29 將兩者
放入同一 union：

```text
frontend Mode-4 scratch
        或
64 × 1 KiB Sprite2 L2
```

同時移除 L1 的 24 KiB enemy mirror 與 2 KiB projectile mirror。正式
release linker 結果：

| 項目 | 結果 |
|---|---:|
| EWRAM heap start | `0x02032DFC` |
| EWRAM 剩餘 | 53,764 bytes |
| IWRAM heap start | `0x03006300` |
| IWRAM 剩餘 | 7,424 bytes |
| Release ROM | 14,147,668 bytes |
| 32 MiB 使用率 | 42.16% |

上色、清 frame 與 tile offset 熱路徑使用 ARM/IWRAM；開機設定
`WAITCNT=0x4317`，啟用 WS0 3/1 waitstate 與 Game Pak prefetch。
`build.ps1` 強制保留至少 48 KiB EWRAM 與 6 KiB IWRAM。

## 無損與流程驗證

`TGLM schema 2` 除原有 62-section matrix 外，新增兩層檢查：

1. build raw component 與原始 RLE 解碼逐像素一致。
2. runtime palette/filter、32×32／16×16 組合及 GBA tile order 與舊
   decoder output 逐像素一致。

實測：

| 項目 | 結果 |
|---|---:|
| LVL sections | 62 / 62 PASS |
| 實際引用 Sprite2 frames | 6,097 |
| Runtime L2 parity frames | 6,098（含 filter case） |
| Runtime pixel comparisons | 6,146,816 |
| Enemy definitions | 818 |
| Weapon definitions | 52 |
| ROMFS／decode failure | 0 |

其他回歸亦通過：

- 第一關 `TGBA schema 25` exact gameplay golden。
- 玩家死亡／Game Over。
- 41 首 Jukebox。
- Episode 1 連續四關 campaign。
- Low Detail／Normal Speed。
- Normal Detail／Normal Speed。
- Low Detail／Low Speed；Boss 視窗 2,226 frames、missed VBlank 2。

## 正式防退化門檻

`build.ps1` 現在會拒絕：

- raw catalog size、CRC32、SHA-256 或 round-trip 數改變。
- runtime matrix 任一 palette/filter/tile pixel 不一致。
- L2 request accounting 不平衡、drop 或 RLE fallback。
- 第一關 missed VBlank 超過 20。
- Normal Speed Boss missed VBlank 超過 8。
- Boss L1 workload、位置或顯示區段改變。
- `WAITCNT`、EWRAM、IWRAM 或 32 MiB ROM 上限退化。

## 後續

此階段解決的是通用 Sprite2 presentation 成本，不修改下一階段的來源移植
優先序。後續仍依 v28 計畫延伸完整 Episode route、保存跨關狀態，並逐行
翻寫 front／rear／special weapon，移除 route-test combat assist。
