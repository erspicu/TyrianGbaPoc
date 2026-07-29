# Tyrian GBA Updated Plan v39

更新日期：2026-07-28

工作分支：`opentyrian-source-parity-port`

## v39 已完成

- release 拆除固定 Pulse-Cannon Power 11／HDT weapon 165。
- 由玩家目前 front weapon ID／power 動態查
  `weaponPort.op[0][power-1]`。
- 新遊戲恢復原版 Pulse-Cannon power 1／HDT weapon 155。
- weapon ID／power 改變時重綁 definition 並清空 front
  `shotMultiPos`；拾取升級會改變實際 volley。
- `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE` release 預設改為 0。
- Power 11 固定配置隔離成 AUTOTEST-only，不能進入一般 ROM。
- death regression 新增 power 1／HDT 155／無固定覆寫遙測。
- High／Normal gameplay、death、Jukebox、62-section matrix、
  four-level campaign、Episode 2 route 全部 PASS。
- `build` 只保留最新 v39 release ROM。

## 固定原則

- Gameplay authority 保持 OpenTyrian C 與 ROMFS 的 LVL／HDT／SHP。
- 玩家裝備 state 決定武器；renderer／測試工具不能覆寫 release state。
- build-time 只做完整、可重現且不改變來源索引語意的轉碼。
- 測試輔助必須由 compile-time guard 與 release 明確隔離。
- Logic、碰撞、RNG 與事件順序不因 presentation 最佳化而改變。
- 一般 build 至少保留 48 KiB EWRAM 與 6 KiB IWRAM。
- `build` 最終只留最新可玩 ROM，其他 ROM 移至 `Backup`。

## 下一階段

1. 建立跨關 campaign player state，保存 weapon ID／power、cash、
   cube、armor、shield 與 global flags。
2. 接回原版 Game Menu 裝備／升級選擇，不以預設配置取代。
3. 恢復 stock energy／ammo、charge、rear weapon mode 與 cooldown。
4. 完成 rear／sidekick／special／superbomb 的正常 source firing path。
5. 建立合法裝備組合效能矩陣，再擴大 Episode 1／2 完整 campaign。

詳細紀錄：

- `Tyrian-GBA-Normal-Weapon-Mortal-Player-v39.md`
