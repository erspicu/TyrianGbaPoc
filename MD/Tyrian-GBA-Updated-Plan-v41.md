# Tyrian GBA Updated Plan v41

更新日期：2026-07-28

工作分支：`opentyrian-source-parity-port`

## v41 已完成

- 修正五份 stock Demo 的 one-based `levelSong` 轉換。
- 修正 Boss 摘要文字亮度與來源 hue 9 Cube palette。
- Game Menu 接上目前船艦、裝備、cash、armor 與 shield 顯示。
- 開放 Upgrade Ship 七類裝備與 Done。
- 商店 inventory、名稱、價格、圖形與定義由 ROMFS／HDT 讀取。
- 完成售回、購買、武器 power 1..11、Rear mode、預覽、取消及提交。
- 將選擇結果接到 front/rear weapon、左右 sidekick 與 Plasma Storm
  gameplay 路徑。
- Quit Game 改為來源文字與視窗風格的確認流程。
- Next Level 加入來源星圖 grid、planet、route dots 與動畫。
- 以前端／gameplay／Jukebox 互斥 arena 回收 6,016 bytes EWRAM，
  不縮減 64-slot Sprite2 L2。
- High／Normal 完整 release 與全套 SRAM 回歸 PASS。

## 固定邊界

- 只完成使用者目前指定介面；不主動移植全部 PC Options、鍵盤、
  joystick、network、雙人或其他 GBA 無用設定頁。
- Gameplay 與資料 authority 維持 OpenTyrian C、ROMFS LVL/HDT/SHP/PIC/
  MUS，不建立 per-level GBA 專用資源。
- 320x200 PC 版只作規格與視覺基準；GBA 介面可依 240x160 裁切及重排。
- 一般 build 保留至少 48 KiB EWRAM、6 KiB IWRAM。
- build 最終只保留最新可玩 GBA ROM，歷史 ROM 放入 `Backup`。
- 開發驗證用 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE` 暫時維持啟用，直到
  使用者要求恢復正常受傷／死亡測試。

## 後續

只在收到下一個明確需求後擴充；目前不把 PC 其餘設定介面加入待辦。

詳細紀錄：

- `Tyrian-GBA-Frontend-Upgrade-v41.md`
