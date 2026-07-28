# Tyrian GBA Updated Plan v38

更新日期：2026-07-28
工作分支：`opentyrian-source-parity-port`

## v38 已完成

- Secret Level 提示依 12-pixel 上方裁切向下補償。
- Secret Level song 30 改為 one-shot，150 source ticks 後恢復關卡 BGM。
- end-level song 9 與 GAME OVER song 10 改為 one-shot。
- GAME OVER overlay 下繼續推進 PC level loop；Boss 飛離期間維持背景
  與關卡前進。
- 過關摘要保留最後 gameplay frame，不再切成黑底。
- 摘要字型改為直接解碼 ROMFS `tyrian.shp` 的 `SMALL_FONT_SHAPES`。
- 摘要座標、可變字寬、hue 15 glow、brightness -4 最終字色及白色
  index 254 對齊 `JE_endLevelAni()`／`JE_outTextGlow()`。
- 修復混合灰／棕／暗綠 BG tile 被多數暗綠 pixel 整塊染綠。
- 最終調色演算法維持 O(64)，Episode 2 missed VBlank 為 24／10,475。
- High／Normal 的 gameplay、death、Jukebox、62-section matrix、
  four-level campaign 與 Episode 2 route 全部通過。
- release 保留 49,708 bytes EWRAM、8,888 bytes IWRAM。

## 固定原則

- Gameplay authority 保持 OpenTyrian C 與 ROMFS 的 LVL／HDT／SHP。
- 不建立 per-level、event-limited 或 Episode 專用 GBA 資源表。
- build-time 只做完整、可重現且不改變來源索引語意的轉碼。
- Logic、碰撞、RNG 與事件順序不因 presentation 最佳化而改變。
- 一般 build 至少保留 48 KiB EWRAM 與 6 KiB IWRAM。
- `build` 最終只留最新可玩 ROM，其他 ROM 移至 `Backup`。

## 下一階段

1. 把固定 Power-11 驗證裝備接回 campaign equipment state。
2. 恢復 stock ammo、charge、cooldown 與裝備互斥。
3. 建立合法 front／rear／sidekick／special 組合效能矩陣。
4. 將四關 campaign 擴大成 Episode 1 完整 Full Game 與 Episode 2
   轉場，保存 player、cash、cube、weapon、armor、shield 與 flags。
5. 完成 turret 251..255、magnet、special effects 與剩餘 misc-shot。

詳細紀錄：

- `Tyrian-GBA-Secret-EndFlow-Stats-Palette-v38.md`
