# Tyrian GBA Updated Plan v37

更新日期：2026-07-28
工作分支：`opentyrian-source-parity-port`

## v37 已完成

- 一般 release 切回 stock Port 1 Pulse-Cannon，Power 11。
- 預設 Detail High／Game Speed Normal。
- 玩家五發武器由 HDT record 165 與 ROMFS Sprite2 直接驅動。
- 打包 29 SFX + 9 voices，完整接通八格 source `soundQueue[]`。
- 補回 hit、explosion、enemy weapon、Good Luck、Boss、Data Cube、
  Level Complete 等來源聲音。
- Secret Level portal、song 30、150-tick 閃字及 route override。
- 每關清空四個 enemy shape slots，修復 Episode 4 跨關卡 bank 汙染。
- 金額提示下移 12 pixels。
- 新增 source sound bitset 與 Secret Level collision 永久回歸。
- High／Normal 的 gameplay、death、Jukebox、62-level matrix、
  four-level campaign、Episode 2 與 Episode 4 均通過。

## 固定原則

- Gameplay authority 保持 OpenTyrian C 與 ROMFS 的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 Episode 專用 GBA 資源表。
- build-time 只做完整、可重現且不改變來源索引語意的轉碼。
- Logic、碰撞、RNG 與事件順序不因 presentation 最佳化而改變。
- `build` 最終只留最新可玩 ROM，其他 ROM 移至 `Backup`。

## 下一階段

1. 把目前固定 Power-11 驗證裝備接回 campaign/save equipment state。
2. 完成合法 front／rear／sidekick／special 的選單與互斥規則。
3. 擴大 Episode 1 campaign，保存 cash、cube、armor、shield、weapon
   與 global flags。
4. 補齊 turret 251..255、magnet、special effect 與 misc-shot 104。
5. 建立能實際走到 stock Secret Level portal 的完整 route test；
   目前已有 branch-level collision 與 presentation 靜態回歸。
