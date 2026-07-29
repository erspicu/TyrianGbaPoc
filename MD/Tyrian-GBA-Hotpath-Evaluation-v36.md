# Tyrian GBA Hotpath Evaluation v36

日期：2026-07-28

分支：`opentyrian-source-parity-port`

## 結論

這批建議只有一項值得成為預設實作：把 `ot_mt_rand()` 搬到
ARM/IWRAM。其餘候選不是目前 hotpath、實測變慢，就是會破壞既定的
IWRAM 安全餘裕。

最終保留：

- `ot_mt_rand()` 使用 `IWRAM_CODE ARM_CODE`；
- MT19937 的輸出與呼叫順序不變；
- 保留清楚的 `if (index == 624) index = 0`；
- 壓力測試 schema 升為 `TGW8`，加入真實呼叫量與 RNG 微基準。

未保留：

- 手寫 `p1 & -(p1 != 624)`；
- `ot_round_ratio()` reciprocal table 或近似除法；
- 整個 `ot_draw_enemy_pool()` 搬入 IWRAM；
- enemy-shot 的 `dir * velocity` branchless 版本；
- `% animax`。

## 先確認目前已經有的機制

- Player-shot active mask 已使用 32-bit mask 與 De Bruijn lowest-set-bit
  查表；這不是尚未實作的建議。
- Collision unsigned-range 仍是可測開關，但預設關閉。v35 已證明把
  雙軸改成 bitwise `&`／`|` 會取消 X-axis short-circuit，使 collision
  平均成本增加 13.60%。
- `ot_level_port_update_enemy_shots()` 已經在 ARM/IWRAM。
- Sprite2 raw palette pack 已經每四個 pixel 組成 `uint32_t` 後寫入
  EWRAM。

## 真實呼叫量

測試條件：

- Episode 2 section 1 deterministic full-loadout；
- Pentium Detail／Normal Speed；
- 六套最重武器、81 player-shot slots、128 OAM；
- 3,600 wall VBlanks、2,096 logic ticks。

| 計數 | 數值 |
|---|---:|
| RNG calls | 135 |
| `ot_round_ratio()` calls | 14 |
| enemy motion updates | 16,084 |
| enemy-shot motion updates | 75 |
| enemy-shot triggers | 9 |
| successful enemy launches | 7 |

`ot_round_ratio()` 的確被編成 `BL __aeabi_idiv`，但整段壓測只呼叫
14 次。即使完全消除除法，對 2.65 億級 logic cycles 也沒有可見影響。
為了 14 次呼叫加入 reciprocal table、近似誤差或額外維護成本不合理，
因此維持 OpenTyrian 的精確整數 rounding。

enemy-shot update 也只有 75 個 active-shot updates。v35 同負載 A/B
已測得：

| 版本 | missed／3,600 | logic avg cycles |
|---|---:|---:|
| 原控制流，搬入 ARM/IWRAM 前 | 1,388 | 123,225.83 |
| `dir * velocity` branchless | 1,394 | 123,804.46 |
| 原控制流，ARM/IWRAM | **1,382** | **122,672.64** |

ARM7TDMI 的 ARM state 可用 conditional execution；手寫乘法版本反而
增加真正的 multiply。現有版本已是這個候選中較佳的實作。

## MT19937 索引改寫

`p1 & -(p1 != 624)` 在本函式內語意成立，是因為合法 index 原本必定
位於 `0..623`，加一後只可能是 `1..624`。它不是一般的 modulo-624
bitmask；若輸入超出這個 invariant 就不等價。

更重要的是，原始 C 的兩個 `if` 已被編譯器消除：

- Thumb/ROM baseline 產生 `subs`／`sbcs`／`and`，沒有條件 branch；
- ARM/IWRAM final 產生 `cmp`／`moveq`，利用 ARM conditional
  execution，也沒有 pipeline branch。

同一 ARM/IWRAM 條件下的 10,000-call 微基準：

| 版本 | cycles/call | 相對 final |
|---|---:|---:|
| 原始 `if` | **206.67** | baseline |
| 手寫 mask | 207.63 | +0.46% |

因此不採用手寫 mask。它會降低可讀性，而且在目標 codegen 上略慢。

## RNG 搬入 ARM/IWRAM

10,000-call、IRQ masked、Timer 2/3 system-clock 微基準：

| 版本 | cycles/call |
|---|---:|
| Thumb／Game Pak ROM | 329.08 |
| ARM／IWRAM | **206.67** |

單函式快 37.2%，MT19937 output sink 完全相同：
`2314020218`。

實際 gameplay 只有 135 次呼叫，直接節省約 16.5k cycles；因此不能把
整個 end-to-end 差異都歸因於 RNG。本次 section placement 也改變了
後續 ROM/IWRAM function layout，會影響 Game Pak prefetch 與
interworking veneer。受控整體 A/B 仍是正向：

| 指標 | ROM/Thumb | ARM/IWRAM |
|---|---:|---:|
| missed VBlank | 588 | **583** |
| logic avg cycles | 127,282.61 | **126,220.97** |
| collision avg cycles | 49,997.46 | **49,566.74** |
| gameplay RNG calls | 135 | 135 |
| logic ticks | 2,096 | 2,096 |
| audio frames | 3,600 | 3,600 |

直接微基準證明函式本身變快；整體 A/B 則證明目前完整 link layout 沒有
負收益。未來若連結排列大改，仍應使用 `TGW8` 重測，不能假設額外的
layout 收益永久固定。

## 為何不搬整個 enemy pool

`ot_draw_enemy_pool()` 是混合 hot/cold path 的大型函式，包含 movement、
animation、HDT weapon read、fire、launch 與多個稀有分支。

- ARM/IWRAM 函式本體：4,904 bytes；
- 實際 link 後壓力版 IWRAM free：3,120 bytes；
- 專案安全門檻：6,144 bytes。

這個版本雖可 link，卻只剩約 3 KiB 給 stack/heap，不能作為安全 ROM。
因此未執行它，也未納入預設。若未來要處理這條路徑，應先加入 phase
cycle telemetry，再把高命中、少呼叫依賴的 movement core 與 cold
fire/launch path 分離；不能把整個 4.9 KiB 函式直接塞入 IWRAM。

## IWRAM 對帳

| Build | IWRAM free |
|---|---:|
| Low release | 8,184 bytes |
| Episode 1 campaign test | 7,936 bytes |
| Episode 2 route test | 7,944 bytes |
| Pentium recovery stress | 7,912 bytes |
| Pentium full-loadout playable | 7,872 bytes |
| rejected whole enemy-pool candidate | 3,120 bytes |

所有保留版本仍高於 6 KiB gate。RNG placement 的固定成本為 208 bytes。

## 回歸結果

`build.ps1 -KeepIntermediates -DetailLevel low -GameSpeed normal`：

- gameplay／Boss golden：PASS；
- death／Game Over／回前端：PASS；
- 41-song Jukebox：PASS；
- ROMFS／Sprite2 matrix：62／62；
- Episode 1 campaign：4／4；
- Episode 2 route：3／10,475 missed VBlank；
- unknown visuals、background approximation、stream/cache drops：0；
- mGBA runtime errors：0。

最終極限壓力：

- `TGW8` validation：PASS；
- 3,600 wall/audio frames；
- 2,096 logic ticks；
- 583 missed VBlanks；
- 128 max OAM；
- logic backlog 0；
- Sprite2 L2 drops 0。

## 手動測試 ROM

`build/tyrian_gba_full_loadout_playable_v36_detail_pentium_speed_normal.gba`

- bytes：14,211,096；
- SHA-256：
  `ce88436e7739dcaf08dcbaeedbf655596cd2569b6aa58aadf6529d4c06ac2f88`；
- title／game code：`TYR FULL ARM`／`TYGP`；
- mGBA software renderer 600-frame boot：PASS（`AGB-TYGP`）；
- Detail：Pentium；
- Game Speed：Normal；
- 開發無敵：開啟。
