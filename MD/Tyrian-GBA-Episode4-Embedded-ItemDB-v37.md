# Tyrian GBA Episode 4 Embedded Item Database v37

日期：2026-07-28
分支：`opentyrian-source-parity-port`
狀態：已修正、完整回歸通過

## 結論

`ref/error3.png` 的三座砲台不是 OAM 遺失、調色盤錯誤或
Sprite2 解碼失敗。真正原因是 GBA runtime 在 Episode 4 仍讀取
`tyrian.hdt` 的 enemy definitions；OpenTyrian 原始流程其實改讀
`tyrian4.lvl` 尾端嵌入的另一套 item database。

修正後，Episode 4 第一關 `curLoc=100` 可見三座與 PC 版一致的灰色
砲台；它們不再是「碰撞存在、畫面透明」的實體。這是通用資料讀取
修正，沒有新增 Episode 專用圖形、event table 或 Python 關卡轉換檔。

## 原因證據

OpenTyrian `src/episodes.c` 的 `JE_loadItemDat()` 有兩條來源路徑：

- Episode 1–3：從 `tyrian.hdt` 第一個 32-bit offset 載入。
- Episode 4：跳到 `tyrian4.lvl` 的 `lvlPos[lvlNum - 1]`，亦即 offset
  table 最後一筆。

目前 stock 檔案的實際資料如下：

| 項目 | 數值 |
|---|---:|
| `tyrian4.lvl` 大小 | 800,006 bytes |
| offset table 筆數 | 41 |
| Episode 4 item database 起點 | 662,814 |
| item database 大小 | 137,192 bytes |
| Episode 4 第一關 LVL file number | 4 |
| 開場 shape banks | 31、32、20、33 |
| event 50／52 生成 | enemy 201 × 3 |

Enemy 201 是能直接證明來源錯誤的指紋：

| 欄位 | 錯誤的 `tyrian.hdt` record | 正確的 Episode 4 record |
|---|---:|---:|
| `shapebank` | 6 | 31 |
| `egraphic[0]` | 2 | 1 |
| `armor` | 254 | 6 |
| `value` | 500 | 25 |

舊流程因此建立了完整 enemy/collision state，玩家子彈仍能命中；
但 opening bank slots 沒有 bank 6，`shape_table_slot()` 無法解析，
renderer 只能略過。舊 Episode 4 route 曾累積
`sprite2_null_pointer_skips=89,503`，和症狀完全吻合。

## 通用修正

`src/opentyrian_data.c` 新增 `OtItemDatabase` view，保存來源指標與
weapon／port／special／option／enemy 相對 offsets。選關時：

1. Episode 1–3 解析 `tyrian.hdt` item offset。
2. Episode 4 解析目前 `tyrian4.lvl` 最後一個 offset。
3. 驗證七個 source maxima、固定 record layout 與 137,192-byte
   完整邊界。
4. 所有既有 `ot_data_hdt_*_read()` 直接讀目前選定 view。

資料仍由 ROMFS 原始 stock bytes 直接讀取，沒有複製整套 database，
也沒有建構 GBA-only enemy mapping。

`select_lvl()` 同時禁止把 Episode 4 最後的 item block 誤當成關卡
section；初始化順序改成先建立 HDT view，再選預設關卡。

## 回歸測試修正

舊 62-level matrix 把四個 Episode 的 enemy ID 合併後，最後只用一套
HDT 驗證；這也會掩蓋 Episode 4 的錯誤。現在測試分成兩個 definition
domains：

- Episode 1–3 共用 HDT queue。
- Episode 4 使用 embedded item database queue。

此外，每個 Episode 第一個 section 都驗證 enemy 201 指紋。這可以
攔住「record layout 合法，但來源選錯」的回歸。OpenTyrian 的
`egraphic == 999` 是移除 enemy 的控制 sentinel，不是 Sprite2
graphic；matrix 也依原始 draw path 跳過它。

修正後 matrix：

| 指標 | 結果 |
|---|---:|
| LVL sections | 62／62 |
| events | 53,338 |
| enemy pool entries | 459 |
| shape banks | 35 |
| Sprite2 frames | 8,063 |
| enemy definitions | 1,285 |
| weapon definitions | 100 |
| first failure | 0 |
| test pass | 1 |

## Episode 4 Projectile VRAM

改讀正確 definitions 後，完整 Episode 4 route 顯示另一個真實需求：
同一畫面最多有 10 種不同的 16×16 projectile graphics。舊 cache 只有
8 格，所以雖然 enemy 已恢復，仍有 203 次 projectile presentation
drop。

一般版 OBJ VRAM 現在做以下重分配：

- explosion cache：32 → 28 格；
- projectile cache：8 → 10 格；
- enemy cache：維持 24 格；
- full-loadout stress build：維持原本 32 explosion／18 projectile。

這不改變 projectile lifecycle、碰撞或 OpenTyrian draw order，只調整
GBA presentation resident slots。High Detail 的 Episode 4 實測
explosion unique peak 是 17，低於保留的 28 格。

## 驗證結果

Episode 4 第一關完整 route，High Detail／Normal Speed：

| 指標 | 結果 |
|---|---:|
| route pass | 1 |
| final state | Game Menu |
| event index／count | 903／904 |
| final position | 6,624 |
| unknown visuals | 0 |
| Sprite2 decode failures | 0 |
| Sprite2 null-pointer skips | 0 |
| enemy cache drops | 0 |
| projectile cache drops | 0 |
| effect cache drops | 0 |
| projectile max visible unique | 10 |
| maximum OAM | 98／128 |
| missed VBlank | 16／10,027（0.160%） |

完整 `build.ps1 -DetailLevel high -GameSpeed normal` 亦為 `PASS`：
gameplay、death、Jukebox、62-level matrix、Episode 1 四關 campaign
與 Episode 2 route 全部通過。

最新 release：

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v37_detail_high_speed_normal.gba
```

```text
bytes   = 14,514,160
sha256  = 224909d77669e97ec1f1f49f69d1ea2d8ca4dd2908a7adfb670dffba3073410d
```
