# Tyrian GBA v31：Episode 2 背景快取與熱路徑效能

日期：2026-07-27
分支：`opentyrian-source-parity-port`
狀態：完成

## 結果

Episode 2 第一關的完整 deterministic route 保持相同的 LVL／HDT
事件、敵人、碰撞與 Sprite2 工作量，`missed VBlank` 從 553 降為
30，背景的 512-pattern 近似 fallback 從 472 次降為 28 次。

| Low Detail／Normal Speed | v30 | v31 | 變化 |
|---|---:|---:|---:|
| logic updates | 6,065 | 6,065 | 相同 |
| display frames | 10,475 | 10,475 | 相同 |
| missed VBlank | 553 | 30 | -94.6% |
| background approximations | 472 | 28 | -94.1% |
| collision calls | 838 | 838 | 相同 |
| Sprite2 L1 hits／misses | 69,060／3,276 | 69,060／3,276 | 相同 |
| Sprite2 L2 hits／misses | 2,868／413 | 2,868／413 | 相同 |
| Sprite2 RLE fallback | 0 | 0 | 相同 |
| stream／Sprite2 drops | 0／0 | 0／0 | 相同 |

v31 的 route 最後仍位於 source position 6,632，使用 Episode 2
logical level 1、source song 27 與 1,752 個 event records，並正常回到
Game Menu。

## 根因

GBA 的 Mode-0 64×32 tilemap 使用 32 列硬體 ring。v30 也讓這 32 列
全部持有 background pattern references，即使 160-pixel 畫面在任一
時間只會碰到 21 列。

Episode 2 第一關 layer 1 的 32 列工作集會膨脹到約 642 個 pattern，
超過每層 512 格的快取。當 512 格全部仍被 reference 保護時，
`background_cache_acquire()` 只能對 512 個 32-byte pattern 做逐一
差異比較，再選一張近似圖。這個 fallback 同時造成明顯停頓與局部
畫面近似。

## v31 的通用修正

- 32 列硬體 tilemap ring 保持不變。
- 只有當前 21 列可見工作集持有 ring references。
- 下一個向上捲動 row 使用既有的獨立 prefetch references。
- 跨越 tile 邊界時，新 row 與前一個 presentation frame 的舊底列會
  暫時重疊，因此最大 ownership 是 22 列。
- 下一個 gameplay update 在任何新 allocation／VBlank upload 前釋放
  已離開畫面的底列。
- 不可見的 tilemap word 可以留在硬體 ring；其 source-row identity
  會失效，重新進入畫面之前一定重新 resolve 與 upload。
- 初始化與完整 layer commit 只建立 21 列，其餘 screen-block rows
  清為空白。

這個方法沒有修改 LVL、沒有新增 Episode 判斷，也沒有建立 per-level
轉換資料。

## 全關卡工作集探勘

以 runtime 相同的 `tyrianN.lvl`、`shapes?.dat`、palette bank 選擇與
4bpp packing，離線掃描全部 62 個 logical levels：

| Episode 2 第一關 layer 1 | 最大 pattern 數 |
|---|---:|
| 20 列 | 474 |
| 21 列 | 486 |
| 22 列（含預取／轉場 guard） | 501 |
| 32 列 | 約 642 |

Episode 2 logical level 6 與 11 的 authored layer 1 即使只看可見區也
可能略超過 512（22 列分別為 520／533）。這是現有每層 512-pattern
VRAM 配置的真實上限，因此 v31 保留原本有界的 visual-approximation
fallback；一般關卡不為了兩個極端區段增加 per-level 資料。

## Sprite2 grouped stores

v29 已將完整 37-bank、11,248-component Sprite2 catalog 無損展開為
12×14 raw PC palette indices。v31 不重做資源，而是最佳化 runtime
上色：

- 四個 source pixels 在 register 內組成 little-endian 32-bit word。
- 對齊的 component row 使用三次 32-bit store。
- `origin_x=10` 使用兩次 32-bit store 加兩個 16-bit 邊界。
- 透明 index 0、palette filter 與 tile-order 語意不變。
- `TGLM schema 2` 重新驗證 6,098 frames／6,146,816 output pixels，
  全部通過。

GBA EWRAM 是 16-bit bus，所以這不是「匯流排等待減少 75%」：
store 指令數約減少 75%，32-bit transaction 仍由兩個 16-bit transfer
完成；主要收益還包含移除逐 pixel tile-offset 計算。

## IWRAM placement

只在選單使用的文字與 route records 搬到 EWRAM；以下實際 gameplay
熱路徑以 `noinline,noclone` 固定在 ARM/IWRAM，避免 `-O3` 又把它們
展開回 ROM：

| 函式 | ARM/IWRAM bytes |
|---|---:|
| `source_sprite2_l2_pack_raw_word()` | 212 |
| `source_sprite2_l2_write_raw_component()` | 460 |
| `source_enemy_cache_acquire()` | 1,036 |
| `ot_level_port_collide_player_shot()` | 3,196 |

`ot_sprite2_frame_decode()` 與 decoded-frame packer沒有搬入 IWRAM：
全 route 與全關卡 matrix 的 RLE fallback 都是 0，搬移只會浪費空間。

Link-time 安全餘量：

| build | EWRAM free | IWRAM free |
|---|---:|---:|
| release | 49,612 bytes | 6,408 bytes |
| Episode 2 smoke | 49,612 bytes | 6,176 bytes |
| four-level campaign | 49,468 bytes | 6,160 bytes |

所有 build 仍通過 EWRAM 至少 48 KiB、IWRAM 至少 6 KiB 的既有門檻。

## 音訊 A/B 與決定

為確認是否值得改用 Game Boy PSG，診斷 build 只包裝
`mmStart()`／`mmEffect()`，保留 Maxmod 初始化與 VBlank 安全路徑：

| Episode 2 第一關 | 正常音訊 | 不播放 music／SFX |
|---|---:|---:|
| v30 missed VBlank | 553 | 512 |
| v31 missed VBlank | 30 | 29 |

背景修正後，完全停播聲音只省一次 missed VBlank。把完整 Tyrian
tracker 音樂降成 Game Boy 四聲道 PSG 對這個問題沒有合理的
品質／效能交換，因此 v31 保留目前完整 Maxmod 音質，也不增加 PSG
專用資源。日後 PSG 可以作為獨立音色實驗，而不是效能 workaround。

## Build-time 資源決定

- Sprite2 已由 v29 完整、通用且無損地預展開，runtime RLE fallback
  為 0；再建立一份相同資源沒有收益。
- `shapes?.dat` 背景本身已是 raw 256-colour indices；EP2 的瓶頸是
  過度保護的 runtime working set，不是 ROM 解壓。
- v31 因此沒有建立任何 per-level 或 GBA-only background catalog。
  ROM 剩餘空間保留給後續完整 Episode 資料與功能。

## 永久回歸

`build.ps1` 現在固定建立並執行 Episode 2 section 1 route smoke，除
既有測試外另檢查：

- source route、event cursor、完成位置與 collision workload；
- Sprite2 L1／L2 完整 accounting；
- 零 stream drop、零 decode failure、零 RLE fallback；
- `background approximations <= 64`；
- 完整關卡 `missed VBlank <= 50`。

Low／Normal Detail 與 Low／Normal Game Speed 都保留支援；已實測：

- Low Detail／Normal Speed：30 missed VBlanks；
- Normal Detail／Normal Speed：32；
- Low Detail／Low Speed：29。

## 正式成品

- ROM：`tyrian_gba_level1_pc_flow_mode4_romfs_v31_detail_low_speed_normal.gba`
- 容量：14,147,244 bytes
- SHA-256：
  `4b339f00d85dc100309b1d8104cafc177769e52db9ee7fedd5339a856d910fe5`
- 發布版本：GitHub Release `v31`
