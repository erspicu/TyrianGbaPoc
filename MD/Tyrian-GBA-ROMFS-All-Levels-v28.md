# Tyrian GBA ROMFS 多關卡 runtime v28

更新日期：2026-07-27
工作分支：`opentyrian-source-parity-port`

## 階段目標

本階段把原本只保證第一關可執行的資料路徑，改成所有 Episode／LVL
section 共用的 runtime。關卡不可再新增專用 Python 轉換表、預先展開
enemy catalog 或每關一份背景資源；GBA ROM 只封裝 PC 原始資料，C
runtime 依目前 Episode、script section 與 LVL file number 選取內容。

ROMFS 目前包含 68 個原始檔，主要資料流為：

```text
levelsN.dat
  -> 解密 script 與選單路徑
  -> episode / map section / next section / song / LVL file number

tyrianN.lvl
  -> level header / random enemy pool / event records
  -> MAP1 / MAP2 / MAP3 lookup 與 map rows

tyrian.hdt
  -> enemy / weapon definition

newsh*.shp + tyrian.shp
  -> runtime Sprite2 decode

music.mus / palette.dat / tyrian.pic
  -> 音樂、palette、前端原始資料
```

## 通用 loader

`src/opentyrian_data.c` 現在以目前選取的 Episode 與 LVL section 提供：

- `OtLevelInfo` 與三層 map view。
- 40-entry random enemy pool。
- 11-byte 原始 event record。
- HDT enemy／weapon definition。
- `levelsN.dat` 的 `]J`、`]2`、`]H`、`]h`、`]G`、`]L`、`]Q` 路徑。
- 原始 MUS、PIC、SHP 與 background shape file view。

背景不再引用 builder 產生的第一關 map／tile atlas。
`src/background_runtime.inc` 直接讀取目前 LVL 的 lookup、map row 與
`shapes?.dat`，以固定 GBA cache 在 VBlank 上傳需要的 tile。

## OpenTyrian 跨關卡語意

`src/opentyrian_level_port.c` 已由固定第一關改為 selected-level runtime，
並補齊 stock event 1..82 的共用處理。未知的特殊／bonus record 仍依
OpenTyrian 警告後略過，不偽造等價效果。

敵人 `sprite2s` 不再只保存 shape bank 數字，而是保存
`enemySpriteSheets[4]` 的 slot identity：

- event 5 替換某個 sheet 後，仍指向該 slot 的既有敵人會看到新 bank。
- APPROACH 使用未載入 bank 時沿用同一 enemy slot 的前次 pointer。
- 21／26 對應固定 sprite sheet。
- 冷啟動且沒有前次 pointer 時，依原版 `sprite2s == NULL` 不繪圖。

這個狀態會在 Next Level 之間保留，符合 PC 全域 enemy pool／sheet
生命週期；矩陣的獨立 section 測試則可要求 cold start。

## 全資料矩陣

`make romfs-matrix-autotest` 會在 GBA runtime 上讀取所有原始檔，不使用
host 端預轉換結果。`TGLM schema 1` 結果：

| 項目 | 結果 |
|---|---:|
| LVL sections | 62 / 62 PASS |
| 原始 events | 53,338 |
| random enemy pool entries | 459 |
| 特殊／未知 records | 39 |
| Episode × mode × difficulty routes | 24 / 24 PASS |
| ROMFS failure | 0 |
| 靜態 background approximation | 0 |
| 驗證 shape banks | 35 |
| 驗證 Sprite2 graphics | 6,097 |
| 驗證 enemy definitions | 818 |
| 驗證 weapon definitions | 52 |

代表性實機 route smoke 亦已完成：

| Route | 最終位置 | Event cursor / total | Pool full | Combat assist | 結果 |
|---|---:|---:|---:|---:|---:|
| EP1 section 5 | 7,640 | 445 / 456 | 0 | 0 | PASS |
| EP2 section 1 | 6,632 | 1,484 / 1,752 | 0 | 0 | PASS |
| EP3 section 1 | 7,677 | 1,015 / 1,021 | 17 | 0 | PASS |
| EP4 section 1 | 6,624 | 903 / 904 | 1 | 4 | PASS |

Pool full 是 OpenTyrian 固定 4 × 25 enemy slots 的原始 allocation 結果，
不是 ROMFS 遺失。EP4 的 4 次 assist 只存在 route-test ROM，用來替代尚未
移植的高碰撞半徑 PC special weapon；release ROM 不含該支援。

## 連續四關 campaign

`make campaign-smoke-autotest` 不直接重開四個 ROM，而是實際走：

```text
Game Menu
  -> Next Level
  -> gameplay
  -> end-level flight
  -> stats
  -> Game Menu
  -> 下一個 levels1.dat section
```

`TGCM schema 3` 連續路徑：

| 次序 | Episode | Script section | LVL file | Song | Event cursor | 位置 | Kills |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 3 | 9 | 17 | 906 | 6,054 | 98 |
| 2 | 1 | 5 | 1 | 0 | 445 | 7,640 | 62 |
| 3 | 1 | 29 | 8 | 32 | 514 | 5,450 | 48 |
| 4 | 1 | 25 | 10 | 17 | 240 | 2,824 | 139 |

四關的事件帳、spawn 帳、ROMFS、Sprite2、effect、projectile、layer 與
end-level music failure flags 全為 0；route checksum 為 `EAEB0109`。

## 第 24 個 Sprite2 frame

第三關同一 presentation frame 需要 24 個獨特 enemy graphic。原先只有
23 個 32×32 8bpp cache slots，會漏 5 次 OAM graphic。

v28 使用 OBJ tiles 622..629 的空隙增加一個 16×16 compact slot：

- 只接受原版 12×14、`size == 0` 的 Sprite2。
- 從共用 32×32 decoder canvas 的 `(8,8)..(23,23)` 打包。
- OAM 座標向內補 8 pixel，最終 PC source 座標不變。
- 32×32 大型敵人仍只使用原來的 full slots。
- projectile 8 slots、Boss bar、player、effect 與 HUD VRAM 均未減少。

section 29 的 cache drop 已由 5 降為 0，最大 visible unique 為 24。
第一關使用 compact upload 42 次，仍維持 935 events、位置 6481 與所有
原 golden gameplay counter。

## 建置與驗證

預設交付：

```powershell
.\build.ps1 -DetailLevel low -GameSpeed normal
```

正式 `build.ps1` 現在強制執行：

1. 第一關 `TGBA schema 24` exact golden。
2. 玩家死亡／Game Over。
3. 41 首 Jukebox。
4. 62-section ROMFS matrix。
5. Episode 1 連續四關 campaign。
6. release ROM 600-frame boot benchmark。

已驗證組合：

| Detail | Game Speed | TGBA | TGLM | TGCM |
|---|---|---:|---:|---:|
| Low | Normal | PASS | PASS | PASS |
| Normal | Normal | PASS | PASS | PASS |
| Low | Low | PASS | PASS | PASS |

Low／Normal speed 的 performance gate 統一為至少 95% gameplay display
frame 準時完成，而不是使用只適合單一速度的固定 missed-VBlank 數。

預設 release：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v28_detail_low_speed_normal.gba
```

大小 12,251,772 bytes，使用標準 32 MiB GBA ROM 的 36.51%。最大 BSS
約 209 KiB，仍在 256 KiB EWRAM 內。

## 下一階段

- 把 campaign 從四關延伸到完整 Episode 路徑與 Episode 轉場。
- 逐一取代 route-test 的 temporary combat assist，移植 PC player
  weapon／special weapon collision。
- 對 39 個 stock 特殊／bonus records 依其實際可達模式補齊流程。
- 持續以同一 ROMFS reader 支援新關卡，禁止新增 per-level asset builder。
