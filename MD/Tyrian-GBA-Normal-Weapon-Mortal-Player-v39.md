# Tyrian GBA v39：正常武器狀態與可死亡主角

日期：2026-07-28

分支：`opentyrian-source-parity-port`

## 目標

- 拆除一般 ROM 固定 Pulse-Cannon Power 11／HDT weapon 165 的驗證
  loadout。
- 回到 OpenTyrian 新遊戲的 front weapon ID 1、power 1。
- 讓拾取升級後的 weapon ID／power 真正改變下一次發射內容。
- 關閉正式 ROM 無敵，啟用既有 shield、armor、死亡與 Game Over 流程。

## 原因

`OtLevelPortState` 原本已由 `JE_initPlayerData()` 的逐行翻寫初始化為：

```text
front weapon = 1 (Pulse-Cannon)
front power  = 1
rear weapon  = 0
```

但舊的 GBA 射擊 adapter 在關卡載入後又強制把 front power 改成 11，
永遠只讀 `weaponPort[1].op[0][10]`，也就是 HDT record 165。結果是
`power_up_weapon()` 雖然能更新 state，畫面上的實際射擊仍被鎖在最強
五發版本。

## v39 實作

`source_front_weapon_sync()` 現在以原版索引規則動態綁定：

```text
port  = player_front_weapon_id
power = player_front_weapon_power
HDT   = weaponPort[port].op[0][power - 1]
```

- front weapon 固定使用 source mode 0；`player_weapon_mode` 仍只屬於
  rear port。
- ID／power 沒變時直接使用快取，不重讀 ROMFS。
- ID／power 改變時重新讀 weapon port 與 weapon definition，並重設
  front `shotMultiPos`，對齊原版武器升級／換裝行為。
- 無武器、power 0、power > 11、無效 HDT definition 都 fail closed，
  不產生自行猜測的 GBA projectile。
- `source_front_weapon_init()` 不再改寫玩家的 weapon ID、power 或 rear
  weapon。

正常 release 因此以 Pulse-Cannon power 1／HDT record 155 開始。已移植
的 front power-up 與 HOT DOG 狀態變更會在下一次發射時套用，不再只是
帳面 counter。

## 測試與 release 隔離

完整第一關與 Episode 2 自動路線原本以 Power 11 建立精確 Boss、cache
與效能金標。v39 保留一個只允許 `AUTOTEST` 使用的
`TYRIAN_GBA_AUTOTEST_FRONT_WEAPON_POWER=11`；一般 build 若非零會觸發
編譯錯誤。這讓既有回歸維持可比較，同時保證玩家拿到的 ROM 沒有鎖定
武器。

death auto-test 不啟用該參數，新增 release-like 遙測：

| 欄位 | 結果 |
|---|---:|
| fixed weapon override | 0 |
| front weapon ID | 1 |
| front weapon power | 1 |
| resolved HDT weapon | 155 |
| definition valid | 1 |
| dev invincible | 0 |
| normal weapon binding | PASS |
| death / Game Over / menu return | PASS |

## 完整回歸

High Detail／Normal Speed：

- 第一關完整 Boss／end-flight／統計／返回選單：PASS
- 強制死亡／Game Over／返回選單：PASS
- Jukebox：PASS
- 全 62 section ROMFS matrix：62／62 PASS
- Episode 1 四關 campaign smoke：4／4 PASS
- Episode 2 第一關 route：PASS
- Episode 2 missed VBlank：24／10,475
- release EWRAM free：49,708 bytes
- release IWRAM free：8,888 bytes

## 產物

```text
build/tyrian_gba_level1_pc_flow_mode4_romfs_v39_detail_high_speed_normal.gba
```

- bytes：14,520,120
- SHA-256：
  `addc679846f3e34210b64412f2cd177db7074e50576be36810e895442d764092`

`build` 只保留這個最新 release；測試 ROM 依既有政策移至 `Backup`。

## 尚未擴張的範圍

本次完成「關卡內動態武器／升級」與正常死亡，不把尚未完成的 Game Menu
裝備商店假裝成已移植。跨關保存 weapon、power、cash、cube、armor 與
shield，以及 rear／sidekick／special 的完整 source firing path，仍列入
後續 campaign state 工作。
