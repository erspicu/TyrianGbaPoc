# Episode 4 Time War 無限卡關 Root Cause

日期：2026-08-01
狀態：Root cause 已確認；修復與同存檔回歸完成

## 結論

問題存檔顯示的 `NOSE DRIP` 是上一關名稱；依 `main section 37`
繼續解析原始 `levels4.dat` 後，實際進入的是 section 45：

- 關卡名稱：`SQUADRON`
- 原始註解：`Time War`
- `tyrian4.lvl` 實體關卡：18
- 音樂：原始一基底編號 37（程式內零基底 36）

這不是一般可擊破 Boss 後結束的關卡，而是原版 Episode 4 結尾的
生存小遊戲。它的事件表刻意循環產生敵人，沒有 event 11／36
這類正常的結束關卡指令；原版離場條件是玩家生命耗盡。

## PC 原始流程

`levels4.dat` section 45 在 `]L` 前依序帶有：

```text
]e[ ENGAGE items
]g[ GALAGA mode
]L[ 9999 043 SQUADRON 37 18     >> Time War
```

上一層 section 44 另有 `]x[`（bonus game）、`]s[`（設定存檔
section）與 `]b[`（建立進入小遊戲前的備份）。PC `JE_loadMap()`
會因此：

1. 套用 ENGAGE 的固定船艦、武器與生命設定。
2. 開啟 `onePlayerAction`／`galagaMode`。
3. 在死亡時先依前武器 power 所代表的生命數重生。
4. 最後一命耗盡後，以 `doNotSaveBackup` 路徑讀回 section 44 前建立的
   備份，離開 Time War，而不是等待關卡事件表發出結束事件。

## GBA 版修復前的兩個疊加問題

1. `ot_data_episode_level_resolve()` 將 `]e`、`]g`、`]x`、`]s`、`]b`
   歸入不影響關卡的 presentation/default 分支，沒有把模式與備份
   語意傳到 gameplay；因此 SQUADRON 被當成普通 Full Game 關卡。
2. `Configure.h` 的 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE` 目前為 `1`。
   即使只看生存關本身，玩家也永遠無法觸發其唯一的正常離場條件。

## 動態證據

用同一份 SRAM 啟動 route smoke：

- 已正確讀到 LVL 18／song 36。
- 20,000 個顯示 frame 後仍停在 `STATE_PLAY`。
- `cur_loc` 在 Time War 的 60,000 dispatch／attack segment 之間反覆
  跳轉，`event_jump_count` 持續增加。
- `end_level`／勝利音樂／統計流程從未開始。
- 原始 LVL 18 的 595 筆事件沒有正常 end-level event，符合原版
  「生存到死亡」的設計，而非 GBA 偶發漏掉某一隻 Boss。

## 已完成修復

修復沒有硬塞 timeout 或假造 Boss 結束事件，而是補回 PC 原始流程：

1. episode script resolver 現在保留並輸出 ENGAGE、GALAGA、bonus、
   savepoint／backup 指令狀態。
2. 進 SQUADRON 前套用原版 ENGAGE loadout 與以 front-power 表示生命的
   Arcade/Galaga 重生規則。
3. 補回 Galaga 的無護盾、敵方開火頻率、難度護甲倍率與分數換命規則。
4. 最後一命死亡後回到正確的 section 44 備份／選單流程。
5. 開發無敵旗標維持「全程無敵」的單一、可預期語意；此模式下
   Time War 不可能藉由死亡離場，開發測試改用原始 Next Level
   section 44 的 `Skip It` 路線。生存關正常死亡回歸的自動測試會
   以 `TYRIAN_GBA_DEV_PLAYER_INVINCIBLE=0` 單獨編譯，不改動 release
   的開發無敵設定。
6. 新增 `scripted-survival-autotest`，固定驗證三命、GAME OVER、
   checkpoint 與 Game Menu 回復，不依賴單一使用者存檔。

內部 checkpoint 使用 SRAM `0x5FC0..0x5FFF` 的獨立 64-byte 原子紀錄，
具有 schema、CRC32 及最後寫入的 commit byte；玩家的 11 槽存檔格式不變。
這也比常駐一份 campaign snapshot 更符合 PC 隱藏 `LAST LEVEL` slot 語意，
並把本次功能原先會增加的 36 bytes EWRAM 全數收回。High／Normal release
map 維持 `6144 bytes` EWRAM link-time 餘量。

## 修復後動態驗證

以 `問題log/TyrianGBA-打關卡無限卡關.sav` 原檔的複本啟動修正版：

- ENGAGE：`1`；GALAGA：`1`。
- 固定 Atomic RailGun 與三命流程均被套用；最後一命為 `1`。
- GAME OVER 離場次數：`1`。
- 備份 section：`44`；回復 section：`44`；重新解析 section：`44`。
- 備份金額／回復金額：`50,494 / 50,494`。
- 最終狀態：`STATE_GAME_MENU`，游標回到 Play Next Level。
- 測試 signature：`TGSX` schema 1，`pass=1`。

自包含回歸（Episode 4 新遊戲初始金額 30,000）也得到 `TGSX pass=1`，
回復前後金額皆為 30,000。一般非 Arcade 死亡／GAME OVER 回歸仍為
`TGD2 game_over_pass=1, full_pass=1, normal_weapon_pass=1`，且死亡音樂
自然停止一次、回到 Game Menu 的 exit count 為 1，證明特殊處理沒有
改壞普通死亡流程。

2026-08-01 追加驗證：無敵開發版從 section 44 選擇
`Skip It` 後，現在依 PC `JE_nextEpisode()` 週期轉到 Episode 1
section 1 的 Game Menu，不再錯誤回到程式首頁。獨立回歸簽章為
`TGSI schema=1, pass=1`。
