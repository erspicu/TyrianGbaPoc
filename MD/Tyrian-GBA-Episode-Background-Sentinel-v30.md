# Tyrian GBA v30：Episode 背景空白索引修正

日期：2026-07-27
分支：`opentyrian-source-parity-port`

## 結論

Episode 2 與 Episode 4 第一關的嚴重破圖不是 ROMFS 選錯素材，也不是
v29 Sprite2 raw／L2 cache 的問題，而是背景 map lookup 漏掉原版的
保留空白索引規則。

OpenTyrian 在 `tyrian2.c` 建立三層 `ref` 時使用不同的有效範圍：

| 背景層 | 有效 map index | 原版保留空白 |
|---|---:|---:|
| layer 1 | 0..71 | 72..127 |
| layer 2 | 0..70 | 71..127 |
| layer 3 | 0..69 | 70..127 |

舊 GBA runtime 對三層都直接查完整的 128-entry `mapSh`。保留值 71
因此被當成真正 shape number，再經過 tile cache 鋪滿畫面。

## 原始資料證據

以 `levelsN.dat` route 解析首個可玩 section，再直接讀取 ROMFS 內
`tyrianN.lvl`：

| Route | LVL／shape | layer 2 的 71 | layer 3 的 71 | 舊版錯誤 shape |
|---|---|---:|---:|---|
| Episode 2 / TORM | `tyrian2.lvl` #1／`shapesx.dat` | 8,400／8,400 | 9,000／9,000 | 567／567 |
| Episode 4 / SURFACE | `tyrian4.lvl` #4／`shapes).dat` | 8,302／8,400 | 9,000／9,000 | 24／1 |

這與實際畫面完全吻合：

- Episode 2 舊版被同一張綠紅 shape 覆蓋整個畫面。
- Episode 4 舊版被單一沙色材質覆蓋，真正的基地結構消失。
- Episode 1／3 雖然也使用 sentinel 71，但其錯誤 lookup 碰巧呈現為
  透明或不可見，因此沒有暴露同一個程式錯誤。

## 修正

`src/background_runtime.inc` 新增三層共用的有效 slot 數
`{72, 71, 70}`。`background_shape_number_for_map_index()` 在讀取
big-endian `mapSh` 前先套用界線，保留值直接回傳 0／透明。

這是 OpenTyrian `ref` 規則的直接翻寫：

- 不修改原始 LVL／SHP。
- 不新增 Episode 或關卡特例。
- 不新增 GBA-only 背景資源。
- map、shape 與 palette 仍由目前選取的 ROMFS 關卡決定。

`TGLM` matrix 另加入 failure code 25，對每一個 section 驗證
layer 1 index 72、layer 2 index 71、layer 3 index 70／71 都不能解析
成圖形，避免未來又退回 128-entry 共用規則。

## 畫面回歸

四個 Episode 均以 Normal Detail／Normal Speed、第一個 route section、
source position 240 擷取 240×160 framebuffer：

| Episode | 修正前後 PNG |
|---|---|
| 1 | bit-identical，SHA-256 `40bb7b767e38f3784eb1e1ca24f459066c0a9f5783bcf4158923674b9cebbd24` |
| 2 | 正常改變；恢復水面、地面、牆體與敵機分層 |
| 3 | bit-identical，SHA-256 `d48d7fdd9ab8de3c682adfd49265857ce021b4a69588ba801ab3130d638227a7` |
| 4 | 正常改變；恢復沙地、基地建築與中央通道 |

Episode 1／3 bit-identical 證明本次修正沒有改動原本可見的有效圖層；
Episode 2／4 的差異只來自移除原版定義為透明的 sentinel shapes。

## 完整驗證

`build.ps1` release-only 流程結果：

- build：PASS，GCC 16.1.0，無 compiler warning。
- `TGBA schema 25`：PASS；完整第一關、Boss、統計與返回選單。
- death／Game Over：PASS。
- 41-song Jukebox：PASS。
- `TGLM schema 2`：62／62 sections PASS，failure 0，
  background approximation 0。
- `TGCM schema 3`：Episode 1 連續四關 PASS。
- ROMFS：68 files，self-test failure 0。
- release EWRAM 尚餘 53,764 bytes；IWRAM 尚餘 7,360 bytes。

最新 ROM：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v30_detail_low_speed_normal.gba
```

- 大小：14,149,480 bytes（13,817.85 KiB，32 MiB 的 42.1687%）
- SHA-256：
  `7d7a23f88d05e27d0924f41b786169a5f0ccf15c31b256186f63d70243ead440`
